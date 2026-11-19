/**
 * @file alfloateravatarlightresponse.cpp
 * @brief [AvatarLightResponse] Floater: per-avatar Diffuse / Tame reflections /
 *        Brightness / Glow trim, target list, presets and the instrument readout.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alfloateravatarlightresponse.h"

#include "alghoststudio.h"
#include "llagentdata.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcharacter.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llcontrolavatar.h"
#include "lldirectorcast.h"
#include "llnotificationsutil.h"
#include "llscrolllistctrl.h"
#include "llsliderctrl.h"
#include "lltextbox.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"
#include "llworld.h"

#include <algorithm>

namespace
{
const char* const ALR_DOT_FILLED = "\xE2\x97\x8F";   // U+25CF
const char* const ALR_DOT_HOLLOW = "\xE2\x97\x8B";   // U+25CB

const LLColor4 ALR_COLOR_GREEN(0.30f, 0.90f, 0.30f, 1.f);
const LLColor4 ALR_COLOR_AMBER(1.00f, 0.70f, 0.10f, 1.f);
const LLColor4 ALR_COLOR_GREY(0.65f, 0.65f, 0.65f, 1.f);

const char* kindLabel(ALAvatarLightResponse::EKind kind)
{
    switch (kind)
    {
    case ALAvatarLightResponse::KIND_SELF:    return "You";
    case ALAvatarLightResponse::KIND_ANIMESH: return "Animesh";
    case ALAvatarLightResponse::KIND_CLONE:   return "Clone";
    default:                                  return "Resident";
    }
}

std::string shortId(const LLUUID& key)
{
    return key.asString().substr(0, 8);
}
}

ALFloaterAvatarLightResponse::ALFloaterAvatarLightResponse(const LLSD& key)
    : LLFloater(key)
{
}

ALFloaterAvatarLightResponse::~ALFloaterAvatarLightResponse()
{
    mChangeConnection.disconnect();
}

bool ALFloaterAvatarLightResponse::postBuild()
{
    mTargetList      = getChild<LLScrollListCtrl>("target_list");
    mOnlyAdjusted    = getChild<LLCheckBoxCtrl>("chk_only_adjusted");
    mDiffuse         = getChild<LLSliderCtrl>("sld_diffuse");
    mTame            = getChild<LLSliderCtrl>("sld_tame");
    mBrightness      = getChild<LLSliderCtrl>("sld_brightness");
    mGlow            = getChild<LLSliderCtrl>("sld_glow");
    mBypass          = getChild<LLCheckBoxCtrl>("chk_bypass");
    mPreset          = getChild<LLComboBox>("combo_preset");
    mApplyPreset     = getChild<LLButton>("btn_apply_preset");
    mResetSelected   = getChild<LLButton>("btn_reset_selected");
    mInheritSelected = getChild<LLButton>("btn_inherit_selected");
    mCopy            = getChild<LLButton>("btn_copy");
    mPaste           = getChild<LLButton>("btn_paste");
    mBrightnessValue = getChild<LLTextBox>("txt_brightness_ev");
    mStatus          = getChild<LLTextBox>("txt_status");
    mBudget          = getChild<LLTextBox>("txt_budget");

    mTargetList->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSelectionChanged(); });
    mTargetList->setCommitOnSelectionChange(true);
    mOnlyAdjusted->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        refreshRows(true);
        refreshEditors();
    });
    getChild<LLButton>("btn_refresh")->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        refreshRows(true);
        refreshEditors();
    });

    mDiffuse->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSliderCommit(FIELD_DIFFUSE); });
    mTame->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSliderCommit(FIELD_TAME); });
    mBrightness->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSliderCommit(FIELD_BRIGHTNESS); });
    mGlow->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSliderCommit(FIELD_GLOW); });
    // Drag tracking: the 1 Hz row refresh must not reset a slider the user is holding.
    mDiffuse->setSliderMouseDownCallback([this](LLUICtrl*, const LLSD&) { mSliderDragging = true; });
    mTame->setSliderMouseDownCallback([this](LLUICtrl*, const LLSD&) { mSliderDragging = true; });
    mBrightness->setSliderMouseDownCallback([this](LLUICtrl*, const LLSD&) { mSliderDragging = true; });
    mGlow->setSliderMouseDownCallback([this](LLUICtrl*, const LLSD&) { mSliderDragging = true; });
    mDiffuse->setSliderMouseUpCallback([this](LLUICtrl*, const LLSD&) { onSliderMouseUp(); });
    mTame->setSliderMouseUpCallback([this](LLUICtrl*, const LLSD&) { onSliderMouseUp(); });
    mBrightness->setSliderMouseUpCallback([this](LLUICtrl*, const LLSD&) { onSliderMouseUp(); });
    mGlow->setSliderMouseUpCallback([this](LLUICtrl*, const LLSD&) { onSliderMouseUp(); });

    mBypass->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBypass(); });
    mApplyPreset->setCommitCallback([this](LLUICtrl*, const LLSD&) { onApplyPreset(); });
    mResetSelected->setCommitCallback([this](LLUICtrl*, const LLSD&) { onResetSelected(); });
    mInheritSelected->setCommitCallback([this](LLUICtrl*, const LLSD&) { onInheritSelected(); });
    getChild<LLButton>("btn_reset_all")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onResetAll(); });
    mCopy->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCopy(); });
    mPaste->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPaste(); });

    mPreset->selectFirstItem();   // never leave the combo empty: an empty combo reads as index 0 (Reset)
    mChangeConnection = ALAvatarLightResponse::instance().setChangeCallback([this]() { mStatesDirty = true; });

    refreshRows(true);
    refreshEditors();
    refreshStatus();
    return LLFloater::postBuild();
}

void ALFloaterAvatarLightResponse::onOpen(const LLSD& key)
{
    LLFloater::onOpen(key);
    LLUUID target;
    if (key.isUUID())
    {
        target = key.asUUID();
    }
    if (target.notNull())
    {
        mPinnedKeys.insert(target);
        refreshRows(true);
        selectKeys(std::vector<LLUUID>(1, target));
    }
    else
    {
        refreshRows(true);
    }
    refreshEditors();
    refreshStatus();
}

void ALFloaterAvatarLightResponse::onClose(bool app_quitting)
{
    ALAvatarLightResponse::instance().saveIfPersist();
    LLFloater::onClose(app_quitting);
}

void ALFloaterAvatarLightResponse::draw()
{
    const bool tick = mRefreshTimer.getElapsedTimeF32() >= 1.f;
    if (tick)
    {
        mRefreshTimer.reset();
        refreshRows(false);
    }
    if (tick || mStatesDirty)
    {
        mStatesDirty = false;
        refreshStateCells();
        refreshStatus();
    }
    LLFloater::draw();
}

std::string ALFloaterAvatarLightResponse::rowName(const LLUUID& key, ALAvatarLightResponse::EKind kind,
                                                  bool& resolved) const
{
    resolved = true;
    switch (kind)
    {
    case ALAvatarLightResponse::KIND_SELF:
        return "You";
    case ALAvatarLightResponse::KIND_CLONE:
    {
        const ALGhostStudio::Instance* inst = ALGhostStudio::instance().getInstance(key);
        if (inst)
        {
            if (!inst->mName.empty())
            {
                return inst->mName;
            }
            if (!inst->mSourceLabel.empty())
            {
                return inst->mSourceLabel;
            }
        }
        return "Clone " + shortId(key);
    }
    case ALAvatarLightResponse::KIND_ANIMESH:
        return "Animesh " + shortId(key);
    default:
        break;
    }
    LLAvatarName av_name;
    if (LLAvatarNameCache::get(key, &av_name))
    {
        return av_name.getDisplayName();
    }
    resolved = false;
    return shortId(key);
}

void ALFloaterAvatarLightResponse::collectRows(std::vector<LLUUID>& order,
                                               std::map<LLUUID, RowInfo>& info) const
{
    ALAvatarLightResponse& store = ALAvatarLightResponse::instance();

    auto add = [&](const LLUUID& key, ALAvatarLightResponse::EKind kind, const std::string& fallback_name)
    {
        if (key.isNull() || info.find(key) != info.end())
        {
            return;
        }
        RowInfo row;
        row.mKind = kind;
        bool resolved = true;
        row.mName = rowName(key, kind, resolved);
        if (!resolved && !fallback_name.empty())
        {
            row.mName = fallback_name;
        }
        info[key] = row;
        order.push_back(key);
    };

    // You
    add(gAgentID, ALAvatarLightResponse::KIND_SELF, "You");

    // Director cast
    for (const LLDirectorCast::CastMember& member : LLDirectorCast::instance().getCast())
    {
        if (member.mId.isNull() || member.mId == gAgentID)
        {
            continue;
        }
        ALAvatarLightResponse::EKind kind = ALAvatarLightResponse::KIND_RESIDENT;
        LLViewerObject* obj = gObjectList.findObject(member.mId);
        LLVOAvatar* avatar = obj ? obj->asAvatar() : nullptr;
        if (obj && (!avatar || avatar->isControlAvatar()))
        {
            kind = ALAvatarLightResponse::KIND_ANIMESH;
        }
        add(member.mId, kind, member.mLastName);
    }

    // Residents in draw distance
    uuid_vec_t resident_ids;
    LLWorld::getInstance()->getAvatars(&resident_ids);
    for (const LLUUID& id : resident_ids)
    {
        if (id != gAgentID)
        {
            add(id, ALAvatarLightResponse::KIND_RESIDENT, std::string());
        }
    }

    // Animesh in view (control avatars keyed by their linkset root)
    for (LLCharacter* character : LLCharacter::sInstances)
    {
        LLVOAvatar* av = static_cast<LLVOAvatar*>(character);
        if (av && av->isControlAvatar() && !av->isDead())
        {
            add(av->getActorFxOwnerId(), ALAvatarLightResponse::KIND_ANIMESH, std::string());
        }
    }

    // Ghost Studio instances (overlay and entity clones)
    for (const ALGhostStudio::Instance& inst : ALGhostStudio::instance().getInstances())
    {
        add(inst.mId, ALAvatarLightResponse::KIND_CLONE, std::string());
    }

    // Requested-by-menu targets and stored entries that are not present
    for (const LLUUID& key : mPinnedKeys)
    {
        add(key, ALAvatarLightResponse::KIND_RESIDENT, std::string());
    }
    for (const auto& kv : store.entries())
    {
        if (info.find(kv.first) == info.end())
        {
            add(kv.first, kv.second.mKind, kv.second.mLabel);
            std::map<LLUUID, RowInfo>::iterator it = info.find(kv.first);
            if (it != info.end())
            {
                it->second.mKind = kv.second.mKind;
                it->second.mAway = true;
                if (!kv.second.mLabel.empty())
                {
                    it->second.mName = kv.second.mLabel;
                }
            }
        }
    }
}

void ALFloaterAvatarLightResponse::refreshRows(bool force)
{
    if (!mTargetList)
    {
        return;
    }
    ALAvatarLightResponse& store = ALAvatarLightResponse::instance();

    std::vector<LLUUID> order;
    std::map<LLUUID, RowInfo> info;
    collectRows(order, info);

    if (mOnlyAdjusted->get())
    {
        std::vector<LLUUID> filtered;
        for (const LLUUID& key : order)
        {
            const ALAvatarLightResponse::Entry* entry = store.findEntry(key);
            if (entry && !ALLightResponse::isIdentity(entry->mParams))
            {
                filtered.push_back(key);
            }
        }
        order.swap(filtered);
    }

    if (!force && order == mRowKeys)
    {
        // Same rows: only names/away flags can have changed (names resolve late).
        mRowInfo = info;
        for (LLScrollListItem* item : mTargetList->getAllData())
        {
            const std::map<LLUUID, RowInfo>::const_iterator it = info.find(item->getUUID());
            if (it == info.end())
            {
                continue;
            }
            LLScrollListCell* name_cell = item->getColumn(1);
            if (name_cell)
            {
                name_cell->setValue(it->second.mName + (it->second.mAway ? " (away)" : ""));
            }
        }
        return;
    }

    const std::vector<LLUUID> keep_selected = selectedKeys();
    const S32 scroll_pos = mTargetList->getScrollPos();

    mUpdating = true;
    mTargetList->deleteAllItems();
    for (const LLUUID& key : order)
    {
        const RowInfo& row = info[key];
        LLSD element;
        element["value"] = key;
        element["columns"][0]["column"] = "state";
        element["columns"][0]["value"] = "";
        element["columns"][1]["column"] = "name";
        element["columns"][1]["value"] = row.mName + (row.mAway ? " (away)" : "");
        element["columns"][2]["column"] = "kind";
        element["columns"][2]["value"] = kindLabel(row.mKind);
        mTargetList->addElement(element, ADD_BOTTOM);
    }
    mRowKeys = order;
    mRowInfo = info;
    selectKeys(keep_selected);
    mTargetList->setScrollPos(scroll_pos);
    mUpdating = false;

    refreshStateCells();
    if (!mSliderDragging)
    {
        refreshEditors();
    }
}

void ALFloaterAvatarLightResponse::refreshStateCells()
{
    if (!mTargetList)
    {
        return;
    }
    ALAvatarLightResponse& store = ALAvatarLightResponse::instance();
    for (LLScrollListItem* item : mTargetList->getAllData())
    {
        const LLUUID key = item->getUUID();
        std::string text;
        LLColor4 color = ALR_COLOR_GREY;
        const ALAvatarLightResponse::Entry* entry = store.findEntry(key);
        if (entry)
        {
            if (ALLightResponse::isIdentity(entry->mParams))
            {
                text = ALR_DOT_HOLLOW;
            }
            else
            {
                const ALAvatarLightResponse::TargetStatus st = store.queryStatus(key);
                const bool ok = st.mUploads > 0 && st.mVisible == 1 && !st.mMuted;   // unknown (-1) stays amber
                text = ALR_DOT_FILLED;
                color = ok ? ALR_COLOR_GREEN : ALR_COLOR_AMBER;
            }
        }
        if (LLScrollListCell* cell = item->getColumn(0))
        {
            cell->setValue(text);
            cell->setColor(color);
        }
    }
}

void ALFloaterAvatarLightResponse::refreshStatus()
{
    if (!mStatus)
    {
        return;
    }
    ALAvatarLightResponse& store = ALAvatarLightResponse::instance();
    S32 adjusted = 0;
    S32 uploaded = 0;
    S32 visible = 0;
    for (const auto& kv : store.entries())
    {
        if (ALLightResponse::isIdentity(kv.second.mParams))
        {
            continue;
        }
        ++adjusted;
        const ALAvatarLightResponse::TargetStatus st = store.queryStatus(kv.first);
        if (st.mUploads > 0)
        {
            ++uploaded;
        }
        if (st.mVisible == 1)
        {
            ++visible;
        }
    }
    mStatus->setText(llformat("%d adjusted  |  %d uploaded  |  %d visible", adjusted, uploaded, visible));

    const S32 over = store.countLiveOverBudget();
    if (over > 0)
    {
        mBudget->setColor(ALR_COLOR_AMBER);
        mBudget->setText(llformat("%d adjusted avatars render at full detail beyond your avatar impostor limit (%d) - expect lower FPS.",
                                  over, static_cast<S32>(LLVOAvatar::sMaxNonImpostors)));
        mBudget->setVisible(true);
    }
    else
    {
        mBudget->setVisible(false);
    }
}

std::vector<LLUUID> ALFloaterAvatarLightResponse::selectedKeys() const
{
    std::vector<LLUUID> keys;
    if (mTargetList)
    {
        for (LLScrollListItem* item : mTargetList->getAllSelected())
        {
            keys.push_back(item->getUUID());
        }
    }
    return keys;
}

void ALFloaterAvatarLightResponse::selectKeys(const std::vector<LLUUID>& keys)
{
    if (!mTargetList)
    {
        return;
    }
    const bool was_updating = mUpdating;
    mUpdating = true;
    mTargetList->deselectAllItems(true);
    for (const LLUUID& key : keys)
    {
        mTargetList->setSelectedByValue(LLSD(key), true);
    }
    mUpdating = was_updating;
}

ALLightResponse::Params ALFloaterAvatarLightResponse::currentParams(const LLUUID& key) const
{
    const ALAvatarLightResponse::Entry* entry = ALAvatarLightResponse::instance().findEntry(key);
    return entry ? entry->mParams : ALLightResponse::Params();
}

void ALFloaterAvatarLightResponse::applyParams(const LLUUID& key, const ALLightResponse::Params& params, bool save_now)
{
    ALAvatarLightResponse::EKind kind = ALAvatarLightResponse::KIND_RESIDENT;
    std::string label = shortId(key);
    const std::map<LLUUID, RowInfo>::const_iterator it = mRowInfo.find(key);
    if (it != mRowInfo.end())
    {
        kind = it->second.mKind;
        if (!it->second.mName.empty())
        {
            label = it->second.mName;
        }
    }
    else if (const ALAvatarLightResponse::Entry* entry = ALAvatarLightResponse::instance().findEntry(key))
    {
        kind = entry->mKind;
        label = entry->mLabel;
    }
    if (!ALAvatarLightResponse::instance().set(key, kind, label, params, save_now))
    {
        // capacity reached: refuse loudly (unique notification, so a slider drag cannot stack them)
        LLNotificationsUtil::add("AvatarLightResponseCapReached",
            LLSD().with("MAX", static_cast<LLSD::Integer>(ALAvatarLightResponse::maxStoredEntries())));
    }
}

void ALFloaterAvatarLightResponse::refreshEditors()
{
    if (!mTargetList)
    {
        return;
    }
    const std::vector<LLUUID> keys = selectedKeys();
    const bool any = !keys.empty();
    const ALLightResponse::Params p = any ? currentParams(keys.front()) : ALLightResponse::Params();

    mUpdating = true;
    mDiffuse->setValue(p.mDiffuse * 100.f);
    mTame->setValue(p.mTame * 100.f);
    mBrightness->setValue(p.mExposureEV);
    mGlow->setValue(p.mGlow * 100.f);
    mBypass->set(p.mBypass);
    mUpdating = false;
    refreshBrightnessValue();

    mDiffuse->setEnabled(any);
    mTame->setEnabled(any);
    mBrightness->setEnabled(any);
    mGlow->setEnabled(any);
    mBypass->setEnabled(any);
    mPreset->setEnabled(any);
    mApplyPreset->setEnabled(any);
    mResetSelected->setEnabled(any);
    mCopy->setEnabled(any);
    mPaste->setEnabled(any && mHaveClip);

    // Inherit is meaningful only where a fallback exists (animesh -> wearer, clone -> source)
    // and only for rows that currently hold an explicit entry.
    bool can_inherit = any;
    for (const LLUUID& key : keys)
    {
        const ALAvatarLightResponse::Entry* entry = ALAvatarLightResponse::instance().findEntry(key);
        const std::map<LLUUID, RowInfo>::const_iterator it = mRowInfo.find(key);
        const bool fallback_kind = it != mRowInfo.end()
            && (it->second.mKind == ALAvatarLightResponse::KIND_ANIMESH
                || it->second.mKind == ALAvatarLightResponse::KIND_CLONE);
        if (!entry || !fallback_kind)
        {
            can_inherit = false;
            break;
        }
    }
    mInheritSelected->setEnabled(can_inherit);
}

void ALFloaterAvatarLightResponse::refreshBrightnessValue()
{
    if (mBrightnessValue && mBrightness)
    {
        mBrightnessValue->setText(llformat("%+.2f EV", mBrightness->getValueF32()));
    }
}

void ALFloaterAvatarLightResponse::onSelectionChanged()
{
    if (mUpdating)
    {
        return;
    }
    refreshEditors();
}

void ALFloaterAvatarLightResponse::onSliderCommit(EField field)
{
    if (mUpdating)
    {
        return;
    }
    for (const LLUUID& key : selectedKeys())
    {
        ALLightResponse::Params p = currentParams(key);
        switch (field)
        {
        case FIELD_DIFFUSE:    p.mDiffuse = mDiffuse->getValueF32() / 100.f; break;
        case FIELD_TAME:       p.mTame = mTame->getValueF32() / 100.f; break;
        case FIELD_BRIGHTNESS: p.mExposureEV = mBrightness->getValueF32(); break;
        case FIELD_GLOW:       p.mGlow = mGlow->getValueF32() / 100.f; break;
        default: break;
        }
        applyParams(key, p, false);   // saved on slider mouse-up
    }
    refreshBrightnessValue();
}

void ALFloaterAvatarLightResponse::onSliderMouseUp()
{
    mSliderDragging = false;
    ALAvatarLightResponse::instance().saveIfPersist();
}

void ALFloaterAvatarLightResponse::onBypass()
{
    if (mUpdating)
    {
        return;
    }
    for (const LLUUID& key : selectedKeys())
    {
        ALLightResponse::Params p = currentParams(key);
        p.mBypass = mBypass->get();
        applyParams(key, p, true);
    }
    refreshEditors();
}

void ALFloaterAvatarLightResponse::onApplyPreset()
{
    if (mPreset->getCurrentIndex() < 0)
    {
        return;   // nothing chosen: never fall through to index 0 (Reset)
    }
    const S32 index = mPreset->getValue().asInteger();
    if (index < 0 || index >= ALLightResponse::PRESET_COUNT)
    {
        return;
    }
    for (const LLUUID& key : selectedKeys())
    {
        applyParams(key,
                    ALLightResponse::applyPreset(currentParams(key), static_cast<ALLightResponse::EPreset>(index)),
                    false);
    }
    ALAvatarLightResponse::instance().saveIfPersist();
    refreshEditors();
}

void ALFloaterAvatarLightResponse::onResetSelected()
{
    // Explicit identity: still terminates the fallback chain (Inherit removes the entry instead).
    for (const LLUUID& key : selectedKeys())
    {
        applyParams(key, ALLightResponse::Params(), false);
    }
    ALAvatarLightResponse::instance().saveIfPersist();
    refreshEditors();
}

void ALFloaterAvatarLightResponse::onInheritSelected()
{
    for (const LLUUID& key : selectedKeys())
    {
        ALAvatarLightResponse::instance().inherit(key, false);
    }
    ALAvatarLightResponse::instance().saveIfPersist();
    refreshRows(true);
    refreshEditors();
}

void ALFloaterAvatarLightResponse::onResetAll()
{
    LLNotificationsUtil::add("AvatarLightResponseResetAll", LLSD(), LLSD(),
        [](const LLSD& notification, const LLSD& response)
        {
            if (LLNotificationsUtil::getSelectedOption(notification, response) == 0)
            {
                ALAvatarLightResponse::instance().clearAll(true);
            }
        });
}

void ALFloaterAvatarLightResponse::onCopy()
{
    const std::vector<LLUUID> keys = selectedKeys();
    if (keys.empty())
    {
        return;
    }
    mClip = currentParams(keys.front());
    mHaveClip = true;
    refreshEditors();
}

void ALFloaterAvatarLightResponse::onPaste()
{
    if (!mHaveClip)
    {
        return;
    }
    for (const LLUUID& key : selectedKeys())
    {
        applyParams(key, mClip, false);
    }
    ALAvatarLightResponse::instance().saveIfPersist();
    refreshEditors();
}
