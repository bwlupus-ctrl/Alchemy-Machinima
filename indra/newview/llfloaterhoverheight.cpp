/**
* @file llfloaterhoverheight.cpp
* @brief Controller for self avatar hover height, and (ClientHeight) the local,
*        render-only height offset of any avatar.
* @author vir@lindenlab.com
*
* $LicenseInfo:firstyear=2014&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2014, Linden Research, Inc.
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation;
* version 2.1 of the License only.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*
* Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
* $/LicenseInfo$
*/

#include "llviewerprecompiledheaders.h"

#include "llfloaterhoverheight.h"

#include "alclientheight.h"
#include "alghoststudio.h"
#include "aluirefresh.h"
#include "llagent.h"
#include "llagentdata.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcharacter.h"
#include "llcombobox.h"
#include "llfocusmgr.h"
#include "llsdserialize.h"
#include "llsliderctrl.h"
#include "lltextbox.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "llworld.h"

#include <algorithm>

namespace
{
std::string clientHeightShortId(const LLUUID& key)
{
    return key.asString().substr(0, 8);
}
}

LLFloaterHoverHeight::LLFloaterHoverHeight(const LLSD& key) : LLFloater(key)
{
}

LLFloaterHoverHeight::~LLFloaterHoverHeight()
{
    // [ClientHeight] the lambdas capture this
    mStoreConnection.disconnect();
    for (boost::signals2::connection& conn : mNameConnections)
    {
        conn.disconnect();
    }
}

void LLFloaterHoverHeight::syncFromPreferenceSetting(void *user_data, bool update_offset)
{
    F32 value = gSavedPerAccountSettings.getF32("AvatarHoverOffsetZ");

    LLFloaterHoverHeight *self = static_cast<LLFloaterHoverHeight*>(user_data);
    LLSliderCtrl* sldrCtrl = self->getChild<LLSliderCtrl>("HoverHeightSlider");
    sldrCtrl->setValue(value,false);

    if (isAgentAvatarValid() && update_offset)
    {
        LLVector3 offset(0.0, 0.0, llclamp(value,MIN_HOVER_Z,MAX_HOVER_Z));
        LL_INFOS("Avatar") << "setting hover from preference setting " << offset[2] << LL_ENDL;
        gAgentAvatarp->setHoverOffset(offset);
        //gAgentAvatarp->sendHoverHeight();
    }
}

bool LLFloaterHoverHeight::postBuild()
{
    LLSliderCtrl* sldrCtrl = getChild<LLSliderCtrl>("HoverHeightSlider");
    mHoverSlider = sldrCtrl;
    sldrCtrl->setMinValue(MIN_HOVER_Z);
    sldrCtrl->setMaxValue(MAX_HOVER_Z);
    sldrCtrl->setSliderMouseUpCallback(boost::bind(&LLFloaterHoverHeight::onFinalCommit,this));
    sldrCtrl->setSliderEditorCommitCallback(boost::bind(&LLFloaterHoverHeight::onFinalCommit,this));
    // Stock behaviour, unchanged: the server send happens only on mouse-up / editor commit.
    childSetCommitCallback("HoverHeightSlider", &LLFloaterHoverHeight::onSliderMoved, NULL);

    // [ClientHeight] target selector, local offset slider, resets, status.
    mTargetCombo    = getChild<LLComboBox>("target_combo");
    mClientSlider   = getChild<LLSliderCtrl>("ClientHeightSlider");
    mResetButton    = getChild<LLButton>("btn_reset_local");
    mResetAllButton = getChild<LLButton>("btn_reset_all_local");
    mStatusText     = getChild<LLTextBox>("txt_status");
    mClientSlider->setMinValue(-ALClientHeightModel::MAX_OFFSET);
    mClientSlider->setMaxValue(ALClientHeightModel::MAX_OFFSET);
    mClientSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClientHeightMoved(); });
    mTargetCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTargetChanged(); });
    // The drop-down opening is one of the few places the list is rebuilt (never from draw()).
    mTargetCombo->setPrearrangeCallback([this](LLUICtrl*, const LLSD&) { rebuildTargets(); });
    mResetButton->setCommitCallback([this](LLUICtrl*, const LLSD&) { onResetTarget(); });
    mResetAllButton->setCommitCallback([this](LLUICtrl*, const LLSD&) { onResetAll(); });
    // Quick-reset buttons at the end of each slider row (Director console style).
    getChild<LLButton>("btn_reset_local_mini")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onResetTarget(); });
    getChild<LLButton>("btn_reset_hover")->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        LLSliderCtrl* hover = getChild<LLSliderCtrl>("HoverHeightSlider");
        if (!hover->getEnabled() || !isAgentAvatarValid())
            return;
        hover->setValue(0.f);
        gAgentAvatarp->setHoverOffset(LLVector3(0.f, 0.f, 0.f), false);
        onFinalCommit(); // same send path as slider mouse-up / text entry
    });
    mStoreConnection = ALClientHeight::instance().setChangeCallback([this]() { onStoreChanged(); });

    // Initialize slider from pref setting.
    syncFromPreferenceSetting(this);
    // Update slider on future pref changes.
    if (gSavedPerAccountSettings.getControl("AvatarHoverOffsetZ"))
    {
        gSavedPerAccountSettings.getControl("AvatarHoverOffsetZ")->getCommitSignal()->connect(boost::bind(&syncFromPreferenceSetting, this, false));
    }
    else
    {
        LL_WARNS() << "Control not found for AvatarHoverOffsetZ" << LL_ENDL;
    }

    updateEditEnabled();

    if (!mRegionChangedSlot.connected())
    {
        mRegionChangedSlot = gAgent.addRegionChangedCallback(boost::bind(&LLFloaterHoverHeight::onRegionChanged,this));
    }
    // Set up based on initial region.
    onRegionChanged();

    rebuildTargets();
    refreshTargetWidgets(true);

    return true;
}

// virtual
void LLFloaterHoverHeight::onOpen(const LLSD& key)
{
    LLFloater::onOpen(key);
    // A UUID key (menu "Local Height...") selects that avatar; otherwise keep the selection (default Me).
    if (key.isUUID() && key.asUUID().notNull())
    {
        mTargetKey = key.asUUID();
    }
    if (!mRegionChangedSlot.connected())
    {
        // onClose() drops the region callback; take it back up on re-open.
        mRegionChangedSlot = gAgent.addRegionChangedCallback(boost::bind(&LLFloaterHoverHeight::onRegionChanged,this));
    }
    rebuildTargets();
    refreshTargetWidgets(true);
    updateEditEnabled();
}

void LLFloaterHoverHeight::onClose(bool app_quitting)
{
    if (mRegionChangedSlot.connected())
    {
        mRegionChangedSlot.disconnect();
    }
}

// static
void LLFloaterHoverHeight::onSliderMoved(LLUICtrl* ctrl, void* userData)
{
    if (isAgentAvatarValid())
    {
        LLSliderCtrl* sldrCtrl = static_cast<LLSliderCtrl*>(ctrl);
        F32 value = sldrCtrl->getValueF32();
        LLVector3 offset(0.0, 0.0, llclamp(value, MIN_HOVER_Z, MAX_HOVER_Z));
        LL_INFOS("Avatar") << "setting hover from slider moved" << offset[2] << LL_ENDL;
        gAgentAvatarp->setHoverOffset(offset, false);
    }
}

// Do send-to-the-server work when slider drag completes, or new
// value entered as text.
void LLFloaterHoverHeight::onFinalCommit()
{
    LLSliderCtrl* sldrCtrl = getChild<LLSliderCtrl>("HoverHeightSlider");
    F32 value = sldrCtrl->getValueF32();
    gSavedPerAccountSettings.setF32("AvatarHoverOffsetZ",value);
}

void LLFloaterHoverHeight::onRegionChanged()
{
    LLViewerRegion *region = gAgent.getRegion();
    if (region && region->simulatorFeaturesReceived())
    {
        updateEditEnabled();
    }
    else if (region)
    {
        region->setSimulatorFeaturesReceivedCallback(boost::bind(&LLFloaterHoverHeight::onSimulatorFeaturesReceived,this,_1));
    }
}

void LLFloaterHoverHeight::onSimulatorFeaturesReceived(const LLUUID &region_id)
{
    LLViewerRegion *region = gAgent.getRegion();
    if (region && (region->getRegionID()==region_id))
    {
        updateEditEnabled();
    }
}

void LLFloaterHoverHeight::updateEditEnabled()
{
    // [ClientHeight] The stock (server) hover slider is region-gated AND only meaningful for Me:
    // it never moves when another avatar is selected, so nobody's server hover is changed by accident.
    bool enabled = gAgent.getRegion() && gAgent.getRegion()->avatarHoverHeightEnabled() && isMeSelected();
    LLSliderCtrl* sldrCtrl = getChild<LLSliderCtrl>("HoverHeightSlider");
    sldrCtrl->setEnabled(enabled);
    getChild<LLButton>("btn_reset_hover")->setEnabled(enabled);
    if (enabled)
    {
        syncFromPreferenceSetting(this);
    }
}

// ---------------------------------------------------------------------------
// [ClientHeight] local (client-side) avatar height
// ---------------------------------------------------------------------------
LLUUID LLFloaterHoverHeight::targetKey() const
{
    return mTargetKey.notNull() ? mTargetKey : gAgentID;
}

bool LLFloaterHoverHeight::isMeSelected() const
{
    return targetKey() == gAgentID;
}

void LLFloaterHoverHeight::rebuildTargets()
{
    if (!mTargetCombo)
    {
        return;
    }
    const ALClientHeightModel::Store& store = ALClientHeight::instance().store();

    struct Row
    {
        LLUUID      mKey;
        std::string mName;
    };
    std::vector<Row> rows;
    std::set<LLUUID> seen;
    auto add = [&](const LLUUID& key, const std::string& name)
    {
        if (key.isNull() || !seen.insert(key).second)
        {
            return;
        }
        rows.push_back({key, name});
    };
    // Display name through the name cache; the short id until it resolves (a callback then rebuilds).
    auto resident_name = [this](const LLUUID& id) -> std::string
    {
        LLAvatarName av_name;
        if (LLAvatarNameCache::get(id, &av_name))
        {
            return av_name.getDisplayName();
        }
        if (mNamesRequested.insert(id).second)
        {
            mNameConnections.push_back(LLAvatarNameCache::get(id,
                [this](const LLUUID&, const LLAvatarName&) { onNameResolved(); }));
        }
        return clientHeightShortId(id);
    };

    // Me
    add(gAgentID, "Me");

    // Residents (display name, short id until it resolves)
    uuid_vec_t resident_ids;
    LLWorld::getInstance()->getAvatars(&resident_ids);
    std::vector<Row> residents;
    for (const LLUUID& id : resident_ids)
    {
        if (id.notNull() && id != gAgentID && seen.find(id) == seen.end())
        {
            residents.push_back({id, resident_name(id)});
        }
    }
    std::sort(residents.begin(), residents.end(), [](const Row& a, const Row& b)
    {
        return LLStringUtil::compareInsensitive(a.mName, b.mName) < 0;
    });
    for (const Row& r : residents)
    {
        add(r.mKey, r.mName);
    }

    // Animesh (control avatars, not dead); worn ones are labelled
    for (LLCharacter* character : LLCharacter::sInstances)
    {
        LLVOAvatar* av = static_cast<LLVOAvatar*>(character);
        if (av && av->isControlAvatar() && !av->isDead())
        {
            const LLUUID key = ALClientHeight::keyForAvatar(av);
            add(key, "Animesh " + clientHeightShortId(key) + (av->getAttachedAvatar() ? " (worn)" : ""));
        }
    }

    // Ghost Studio clones
    for (const ALGhostStudio::Instance& inst : ALGhostStudio::instance().getInstances())
    {
        std::string name = !inst.mName.empty() ? inst.mName : inst.mSourceLabel;
        if (name.empty())
        {
            name = clientHeightShortId(inst.mId);
        }
        add(inst.mId, "Clone: " + name);
    }

    // Stored keys that are not present (the entry is kept for the session), and the selection
    // itself when a menu handed us a key that is not in any list above.
    for (const auto& kv : store.entries())
    {
        if (seen.find(kv.first) == seen.end())
        {
            add(kv.first, clientHeightShortId(kv.first) + " (away)");
        }
    }
    if (seen.find(targetKey()) == seen.end())
    {
        add(targetKey(), clientHeightShortId(targetKey()) + " (away)");
    }

    mUpdating = true;
    mTargetCombo->removeall();
    for (const Row& r : rows)
    {
        std::string label = r.mName;
        const F32 z = store.get(r.mKey);
        if (z != 0.f)
        {
            label += llformat("   * %+.3f m", z);
        }
        mTargetCombo->add(label, r.mKey);
    }
    mTargetCombo->setValue(LLSD(targetKey()));
    mUpdating = false;
}

void LLFloaterHoverHeight::onNameResolved()
{
    // A name arrived late. Never rebuild under an open drop-down (the next opening rebuilds anyway).
    if (getVisible() && !alIsBeingEdited(mTargetCombo))
    {
        rebuildTargets();
    }
}

void LLFloaterHoverHeight::onTargetChanged()
{
    if (mUpdating || !mTargetCombo)
    {
        return;
    }
    const LLUUID key = mTargetCombo->getValue().asUUID();
    if (key.isNull())
    {
        return;
    }
    mTargetKey = key;
    refreshTargetWidgets(true);
    updateEditEnabled();
}

void LLFloaterHoverHeight::onClientHeightMoved()
{
    if (mUpdating || !mClientSlider)
    {
        return;
    }
    // Live write while dragging: the store only (cheap); avatars pick it up through their revision cache.
    ALClientHeight::instance().set(targetKey(), mClientSlider->getValueF32());
}

void LLFloaterHoverHeight::onResetTarget()
{
    ALClientHeight::instance().reset(targetKey());
    refreshTargetWidgets(true);
}

void LLFloaterHoverHeight::onResetAll()
{
    ALClientHeight::instance().resetAll();
    rebuildTargets();               // drops the "away" rows and the markers
    refreshTargetWidgets(true);
}

void LLFloaterHoverHeight::onStoreChanged()
{
    // Not forced: a slider being dragged / typed into keeps what the user is doing.
    refreshTargetWidgets(false);
}

void LLFloaterHoverHeight::refreshTargetWidgets(bool force)
{
    if (!mClientSlider)
    {
        return;
    }
    const F32 z = ALClientHeight::instance().get(targetKey());
    mUpdating = true;
    setValueIfChanged(mClientSlider, LLSD(z), force);
    mUpdating = false;
    setEnabledIfChanged(mResetButton, z != 0.f);
    refreshStatus();
}

void LLFloaterHoverHeight::refreshStatus()
{
    const size_t n = ALClientHeight::instance().size();
    setTextIfChanged(mStatusText,
        llformat("%u %s adjusted", (unsigned)n, n == 1 ? "avatar" : "avatars"), true);
}
