/**
 * @file alpanelcinelightrig.cpp
 * @brief Shared Director/standalone cinematic light-rig panel.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelcinelightrig.h"

#include "alcinelightrig.h"
#include "alcinelightrigmodel.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llcontrol.h"
#include "lldirectorcast.h"
#include "llfocusmgr.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llscrolllistcell.h"
#include "llscrolllistitem.h"
#include "lltextbox.h"
#include "lluicolortable.h"
#include "llviewercontrol.h"

#include <algorithm>

static LLPanelInjector<ALPanelCineLightRig>
    t_panel_cine_light_rig("panel_cine_light_rig");

namespace
{
const char* const ROLE_NAMES[ALCineLightRigModel::LIGHT_COUNT] = {
    "Key", "Fill", "Rim", "Bg"
};
const char* const ROLE_WIDGET_NAMES[ALCineLightRigModel::LIGHT_COUNT] = {
    "key", "fill", "rim", "bg"
};
const char* const ROLE_ON_SETTINGS[ALCineLightRigModel::LIGHT_COUNT] = {
    "CineLightRigKeyOn", "CineLightRigFillOn",
    "CineLightRigRimOn", "CineLightRigBgOn"
};
std::vector<ALPanelCineLightRig*> LIVE_PANELS;

void addLabeledSeparator(LLComboBox* combo, const std::string& label,
                         bool add_separator)
{
    if (add_separator)
    {
        combo->addSeparator(ADD_BOTTOM);
    }
    combo->add(label, LLSD(), ADD_BOTTOM, false);
}

void registerCineLightRigResetControl()
{
    static bool registered = false;
    if (registered)
    {
        return;
    }
    registered = true;
    LLUICtrl::CommitCallbackRegistry::defaultRegistrar().add(
        "CineLightRig.ResetControl",
        [](LLUICtrl*, const LLSD& param)
        {
            if (LLControlVariable* control =
                    gSavedSettings.getControl(param.asString()))
            {
                control->resetToDefault(true);
            }
        });
}
}

ALPanelCineLightRig::ALPanelCineLightRig()
{
    // XUI commit callbacks are resolved while the panel's children are built.
    registerCineLightRigResetControl();
}

ALPanelCineLightRig::~ALPanelCineLightRig()
{
    LIVE_PANELS.erase(
        std::remove(LIVE_PANELS.begin(), LIVE_PANELS.end(), this),
        LIVE_PANELS.end());
}

const std::vector<std::string>& ALPanelCineLightRig::settings()
{
    static const std::vector<std::string> names = {
        "CineLightRigEnabled",
        "CineLightRigScaleAware",
        "CineLightRigPower",
        "CineLightRigRadius",
        "CineLightRigMasterEV",
        "CineLightRigMasterTempMired",
        "CineLightRigOffsetZ",
        "CineLightRigHeadroomStops",
        "CineLightRigBounceEnabled",
        "CineLightRigBounceRatio",
        "CineLightRigTransitionSec",
        "CineLightRigDamping",
        "CineLightRigTrackMode",
        "CineLightRigCookieUUID",
        "CineLightRigSeed",
        "CineLightRigMirror",
        "CineLightRigOrbitYaw",
        "CineLightRigOrbitPitch",
        "CineLightRigFX",
        "CineLightRigShadowMode",
        "CineLightRigGizmo",
        "CineLightRigKeyYaw",
        "CineLightRigKeyPitch",
        "CineLightRigKeyProfile",
        "CineLightRigKeyEV",
        "CineLightRigKeyBeam",
        "CineLightRigKeyGobo",
        "CineLightRigKeyOn",
        "CineLightRigFillYaw",
        "CineLightRigFillPitch",
        "CineLightRigFillProfile",
        "CineLightRigFillEV",
        "CineLightRigFillBeam",
        "CineLightRigFillGobo",
        "CineLightRigFillOn",
        "CineLightRigRimYaw",
        "CineLightRigRimPitch",
        "CineLightRigRimProfile",
        "CineLightRigRimEV",
        "CineLightRigRimBeam",
        "CineLightRigRimGobo",
        "CineLightRigRimOn",
        "CineLightRigBgYaw",
        "CineLightRigBgPitch",
        "CineLightRigBgProfile",
        "CineLightRigBgEV",
        "CineLightRigBgBeam",
        "CineLightRigBgGobo",
        "CineLightRigBgOn",
    };
    return names;
}

bool ALPanelCineLightRig::postBuild()
{
    mAnchorCombo = getChild<LLComboBox>("cine_anchor");
    mGroupEnable = getChild<LLCheckBoxCtrl>("cine_group_enable");
    mGroupStatus = getChild<LLTextBox>("cine_group_status");
    const char* const group_slot_names[5] = {
        "cine_group_self", "cine_group_a", "cine_group_b",
        "cine_group_c", "cine_group_d",
    };
    for (S32 i = 0; i < 5; ++i)
    {
        mGroupSlotChecks[i] = getChild<LLCheckBoxCtrl>(group_slot_names[i]);
    }
    mSetupCombo = getChild<LLComboBox>("cine_setup_combo");
    mFXCombo = getChild<LLComboBox>("cine_fx_combo");
    mSeedEditor = getChild<LLLineEditor>("cine_seed");
    mShadowHint = getChild<LLTextBox>("cine_shadow_hint");
    mShadowFixIt = getChild<LLButton>("cine_shadow_fixit");
    mRadiusLabel = getChild<LLTextBox>("cine_radius_label");
    mRadiusDefaultColor = LLUIColorTable::instance().getColor(
        "LabelTextColor", LLColor4::white);
    mSetupDelete = getChild<LLButton>("cine_setup_delete");
    if (std::find(LIVE_PANELS.begin(), LIVE_PANELS.end(), this) ==
        LIVE_PANELS.end())
    {
        LIVE_PANELS.push_back(this);
    }

    populateStaticCombos();
    updateAnchorList();
    refreshSetupList();

    mAnchorCombo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onAnchorSelected(); });
    mGroupEnable->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onGroupEnabledCommit(); });
    for (LLCheckBoxCtrl* control : mGroupSlotChecks)
    {
        control->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onGroupSlotsCommit(); });
    }
    mSetupCombo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSetupSelected(); });
    getChild<LLButton>("cine_setup_save")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { saveSetup(); });
    getChild<LLButton>("cine_setup_delete")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { deleteSetup(); });
    getChild<LLButton>("cine_fx_stop")->setCommitCallback(
        [](LLUICtrl*, const LLSD&) { ALCineLightRig::instance().stopFX(); });
    getChild<LLButton>("cine_aim_yaw_minus")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        { adjustAim("CineLightRigOrbitYaw", -15.f); });
    getChild<LLButton>("cine_aim_yaw_plus")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        { adjustAim("CineLightRigOrbitYaw", 15.f); });
    getChild<LLButton>("cine_aim_pitch_minus")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        { adjustAim("CineLightRigOrbitPitch", -5.f); });
    getChild<LLButton>("cine_aim_pitch_plus")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        { adjustAim("CineLightRigOrbitPitch", 5.f); });
    getChild<LLButton>("cine_aim_reset")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { resetAim(); });
    getChild<LLButton>("cine_reset_all")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { resetAll(); });
    mSeedEditor->setMaxTextLength(10);
    mSeedEditor->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { commitSeed(); });
    getChild<LLButton>("cine_seed_randomize")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { randomizeSeed(); });
    mShadowFixIt->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickShadowFixIt(); });
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        const std::string prefix =
            std::string("cine_") + ROLE_WIDGET_NAMES[i];
        mClipStatus[i] = getChild<LLTextBox>(prefix + "_clip");
        mShaftControls[i] = getChild<LLCheckBoxCtrl>(prefix + "_shaft");
        mHeroControls[i] = getChild<LLCheckBoxCtrl>(prefix + "_hero");
        mShaftControls[i]->setCommitCallback(
            [i](LLUICtrl* control, const LLSD&)
            {
                ALCineLightRig::instance().setShaftEnabled(
                    i, control->getValue().asBoolean());
            });
        mHeroControls[i]->setCommitCallback(
            [i](LLUICtrl* control, const LLSD&)
            {
                ALCineLightRig::instance().setHeroEnabled(
                    i, control->getValue().asBoolean());
            });
    }

    syncSeedEditor();
    syncGroupControls();
    updateDerivedStatus();
    return true;
}

void ALPanelCineLightRig::populateStaticCombos()
{
    for (S32 light = 0; light < ALCineLightRigModel::LIGHT_COUNT; ++light)
    {
        const std::string widget_prefix =
            std::string("cine_") + ROLE_WIDGET_NAMES[light];
        LLComboBox* profile = getChild<LLComboBox>(widget_prefix + "_profile");
        LLComboBox* beam = getChild<LLComboBox>(widget_prefix + "_beam");
        LLComboBox* gobo = getChild<LLComboBox>(widget_prefix + "_gobo");
        for (S32 i = 0; i < ALCineLightRigModel::PROFILE_COUNT; ++i)
        {
            profile->add(ALCineLightRigModel::profileName(i), LLSD(i));
        }
        for (S32 i = 0; i < ALCineLightRigModel::BEAM_COUNT; ++i)
        {
            beam->add(ALCineLightRigModel::beamName(i), LLSD(i));
        }
        for (S32 i = 0; i < ALCineLightRigModel::GOBO_COUNT; ++i)
        {
            gobo->add(ALCineLightRigModel::goboName(i), LLSD(i));
        }
        const std::string setting_prefix =
            std::string("CineLightRig") + ROLE_NAMES[light];
        profile->setValue(gSavedSettings.getS32(setting_prefix + "Profile"));
        beam->setValue(gSavedSettings.getS32(setting_prefix + "Beam"));
        gobo->setValue(gSavedSettings.getS32(setting_prefix + "Gobo"));
    }

    mFXCombo->add("None", LLSD(-1));
    for (S32 fx = 0; fx < ALCineLightRigModel::FX_COUNT; ++fx)
    {
        mFXCombo->add(ALCineLightRigModel::fxName(fx), LLSD(fx));
    }
    mFXCombo->setValue(gSavedSettings.getS32("CineLightRigFX"));
}

void ALPanelCineLightRig::updateAnchorList()
{
    const std::vector<LLDirectorCast::CastMember>& cast =
        LLDirectorCast::instance().getCast();
    bool changed = !mCastListInitialized ||
                   cast.size() != mCastIds.size();
    for (size_t i = 0; i < cast.size() && !changed; ++i)
    {
        changed = cast[i].mId != mCastIds[i] ||
                  cast[i].mLastName != mCastNames[i];
    }
    if (!changed)
    {
        syncAnchorSelection();
        return;
    }

    if (mAnchorCombo->hasFocus() ||
        gFocusMgr.childHasKeyboardFocus(mAnchorCombo))
    {
        return;
    }

    mCastIds.clear();
    mCastNames.clear();
    mCastIds.reserve(cast.size());
    mCastNames.reserve(cast.size());
    mAnchorCombo->clearRows();
    mAnchorCombo->add("You", LLSD(LLUUID::null));
    for (const LLDirectorCast::CastMember& member : cast)
    {
        const std::string label = member.mLastName.empty()
            ? member.mId.asString() : member.mLastName;
        mAnchorCombo->add(label, LLSD(member.mId));
        mCastIds.push_back(member.mId);
        mCastNames.push_back(member.mLastName);
    }
    mCastListInitialized = true;
    mAnchorSelectionInitialized = false;
    syncAnchorSelection();
}

void ALPanelCineLightRig::syncAnchorSelection()
{
    const LLUUID anchor = ALCineLightRig::instance().getAnchor();
    if ((mAnchorSelectionInitialized && anchor == mDisplayedAnchor) ||
        mAnchorCombo->hasFocus() ||
        gFocusMgr.childHasKeyboardFocus(mAnchorCombo))
    {
        return;
    }
    mAnchorCombo->setValue(anchor);
    mDisplayedAnchor = anchor;
    mAnchorSelectionInitialized = true;
}

void ALPanelCineLightRig::onAnchorSelected()
{
    if (mAnchorCombo)
    {
        ALCineLightRig::instance().setAnchor(
            mAnchorCombo->getSelectedValue().asUUID());
        mDisplayedAnchor = ALCineLightRig::instance().getAnchor();
        mAnchorSelectionInitialized = true;
    }
}

void ALPanelCineLightRig::onGroupEnabledCommit()
{
    ALCineLightRig::instance().setGroupEnabled(
        mGroupEnable->getValue().asBoolean());
    syncGroupControls();
}

void ALPanelCineLightRig::onGroupSlotsCommit()
{
    U32 slots = 0;
    for (S32 i = 0; i < 5; ++i)
    {
        if (mGroupSlotChecks[i]->getValue().asBoolean())
        {
            slots |= 1u << i;
        }
    }
    ALCineLightRig::instance().setGroupSlots(slots);
    syncGroupControls();
}

void ALPanelCineLightRig::syncGroupControls()
{
    ALCineLightRig& rig = ALCineLightRig::instance();
    const bool enabled = rig.isGroupEnabled();
    const U32 slots = rig.getGroupSlots();
    const U32 resolved = rig.lastResolvedGroupSlots();
    if (enabled == mDisplayedGroupEnabled &&
        slots == mDisplayedGroupSlots &&
        resolved == mDisplayedResolvedSlots)
    {
        return;
    }

    mGroupEnable->setValue(enabled);
    mAnchorCombo->setEnabled(!enabled);
    for (S32 i = 0; i < 5; ++i)
    {
        mGroupSlotChecks[i]->setValue((slots & (1u << i)) != 0);
        mGroupSlotChecks[i]->setEnabled(enabled);
    }

    if (!enabled)
    {
        mGroupStatus->setText(std::string());
        mGroupStatus->setColor(mRadiusDefaultColor);
    }
    else
    {
        S32 requested_count = 0;
        S32 resolved_count = 0;
        for (S32 i = 0; i < 5; ++i)
        {
            requested_count += (slots & (1u << i)) ? 1 : 0;
            resolved_count += (resolved & (1u << i)) ? 1 : 0;
        }
        if (resolved_count == 0)
        {
            mGroupStatus->setText(std::string("none resolved"));
            mGroupStatus->setColor(
                LLUIColor(LLColor4(1.f, 0.75f, 0.25f, 1.f)));
        }
        else
        {
            mGroupStatus->setText(llformat(
                "%d of %d lit", resolved_count, requested_count));
            mGroupStatus->setColor(mRadiusDefaultColor);
        }
    }
    mDisplayedGroupEnabled = enabled;
    mDisplayedGroupSlots = slots;
    mDisplayedResolvedSlots = resolved;
}

void ALPanelCineLightRig::adjustAim(const std::string& setting, F32 delta)
{
    F32 value = gSavedSettings.getF32(setting) + delta;
    if (setting == "CineLightRigOrbitYaw")
    {
        value = ALCineLightRigModel::wrap180(value);
    }
    else
    {
        value = std::clamp(value,
            -ALCineLightRigModel::PITCH_LIMIT_DEG,
             ALCineLightRigModel::PITCH_LIMIT_DEG);
    }
    gSavedSettings.setF32(setting, value);
}

void ALPanelCineLightRig::resetAim()
{
    gSavedSettings.setBOOL("CineLightRigMirror", false);
    gSavedSettings.setF32("CineLightRigOrbitYaw", 0.f);
    gSavedSettings.setF32("CineLightRigOrbitPitch", 0.f);
}

void ALPanelCineLightRig::refreshSetupList(const std::string& select_name,
                                           bool allow_empty, bool force)
{
    if (!mSetupCombo)
    {
        return;
    }
    if (!force && (mSetupCombo->hasFocus() ||
                   gFocusMgr.childHasKeyboardFocus(mSetupCombo)))
    {
        mPendingSetupSelection = select_name;
        mPendingSetupAllowEmpty = allow_empty;
        mSetupRefreshPending = true;
        return;
    }

    mSetupRefreshPending = false;
    mSetupCombo->clearRows();
    addLabeledSeparator(
        mSetupCombo, ALCineLightRig::BUILT_IN_SETUP_CAPTION, false);
    bool added_local_caption = false;
    for (const ALCineLightRig::SetupEntry& entry :
         ALCineLightRig::instance().setupNamesGrouped())
    {
        if (!entry.mMaster && !added_local_caption)
        {
            addLabeledSeparator(
                mSetupCombo, ALCineLightRig::LOCAL_SETUP_CAPTION, true);
            added_local_caption = true;
        }
        LLScrollListItem* item = mSetupCombo->add(
            entry.mName, LLSD(entry.mName));
        if (entry.mMaster && item)
        {
            const ALCineLightRig::MasterSetup* master =
                ALCineLightRig::findMasterSetup(entry.mName);
            if (master && !master->mIntent.empty() && item->getColumn(0))
            {
                item->getColumn(0)->setToolTip(master->mIntent);
            }
        }
    }
    if (!select_name.empty() &&
        mSetupCombo->setSelectedByValue(select_name, true))
    {
        return;
    }
    if (allow_empty)
    {
        mSetupCombo->clear();
    }
    else
    {
        mSetupCombo->setSelectedByValue(
            ALCineLightRig::masterSetups().front().mName, true);
    }
}

//static
void ALPanelCineLightRig::refreshAllSetupLists(
    ALPanelCineLightRig* acting_panel,
    const std::string& acting_selection,
    const std::string& deleted_name)
{
    for (ALPanelCineLightRig* panel : LIVE_PANELS)
    {
        if (panel)
        {
            std::string selection = panel->mSetupCombo
                ? panel->mSetupCombo->getSelectedValue().asString()
                : std::string();
            if (panel == acting_panel)
            {
                selection = acting_selection;
            }
            if (!deleted_name.empty() && selection == deleted_name)
            {
                selection.clear();
            }
            panel->refreshSetupList(selection, true,
                                    panel == acting_panel);
        }
    }
}

void ALPanelCineLightRig::onSetupSelected()
{
    const std::string name = mSetupCombo->getSelectedValue().asString();
    if (!name.empty() && ALCineLightRig::instance().loadSetup(name))
    {
        mSetupCombo->setValue(name);
    }
}

void ALPanelCineLightRig::saveSetup()
{
    std::string name = mSetupCombo ? mSetupCombo->getSimple() : std::string();
    LLStringUtil::trim(name);
    if (name.empty() || ALCineLightRig::isMasterSetup(name) ||
        ALCineLightRig::isSetupDecorationName(name))
    {
        LLNotificationsUtil::add(
            "GenericAlert",
            LLSD().with("MESSAGE",
                "Enter a unique setup name before saving. Built-in setups cannot be overwritten."));
        return;
    }
    if (!ALCineLightRig::instance().saveSetup(name))
    {
        LLNotificationsUtil::add(
            "GenericAlert",
            LLSD().with("MESSAGE",
                "Could not write the cinematic light setup to disk."));
        return;
    }
    refreshAllSetupLists(this, name);
}

void ALPanelCineLightRig::deleteSetup()
{
    const std::string name = mSetupCombo
        ? mSetupCombo->getSelectedValue().asString() : std::string();
    if (name.empty() || ALCineLightRig::isMasterSetup(name) ||
        ALCineLightRig::isSetupDecorationName(name))
    {
        return;
    }
    LLHandle<ALPanelCineLightRig> handle =
        getDerivedHandle<ALPanelCineLightRig>();
    LLNotificationsUtil::add(
        "GenericAlertYesCancel",
        LLSD().with("MESSAGE", "Delete cinematic light setup '" + name + "'?"),
        LLSD(),
        [handle, name](const LLSD& notification, const LLSD& response)
        {
            if (ALPanelCineLightRig* self = handle.get())
            {
                self->deleteSetupCallback(notification, response, name);
            }
        });
}

bool ALPanelCineLightRig::deleteSetupCallback(
    const LLSD& notification, const LLSD& response, const std::string name)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) == 0)
    {
        if (!ALCineLightRig::instance().deleteSetup(name))
        {
            LLNotificationsUtil::add(
                "GenericAlert",
                LLSD().with("MESSAGE",
                    "Could not delete the cinematic light setup from disk."));
            return false;
        }
        const std::string selection = mSetupCombo
            ? mSetupCombo->getSelectedValue().asString() : std::string();
        refreshAllSetupLists(this, selection, name);
    }
    return false;
}

void ALPanelCineLightRig::resetAll()
{
    LLHandle<ALPanelCineLightRig> handle =
        getDerivedHandle<ALPanelCineLightRig>();
    LLNotificationsUtil::add(
        "GenericAlertYesCancel",
        LLSD().with("MESSAGE",
            "Reset every Cinematic Light Rig setting to its default?"),
        LLSD(),
        [handle](const LLSD& notification, const LLSD& response)
        {
            if (ALPanelCineLightRig* self = handle.get())
            {
                self->resetAllCallback(notification, response);
            }
        });
}

bool ALPanelCineLightRig::resetAllCallback(
    const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }
    for (const std::string& setting : settings())
    {
        if (LLControlVariable* control = gSavedSettings.getControl(setting))
        {
            control->resetToDefault(true);
        }
    }
    ALCineLightRig& rig = ALCineLightRig::instance();
    rig.setAnchor(LLUUID::null);
    rig.setGroupEnabled(false);
    rig.setGroupSlots(0);
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        rig.setShaftEnabled(i, false);
        rig.setHeroEnabled(i, false);
    }
    refreshAllSetupLists(
        this, ALCineLightRig::masterSetups().front().mName);
    return false;
}

void ALPanelCineLightRig::commitSeed()
{
    const std::string text = mSeedEditor->getText();
    U32 seed = 0;
    const bool digits_only = !text.empty() && std::all_of(
        text.begin(), text.end(), [](char character)
        {
            return character >= '0' && character <= '9';
        });
    if (!digits_only || !LLStringUtil::convertToU32(text, seed) || seed == 0)
    {
        LLNotificationsUtil::add(
            "GenericAlert",
            LLSD().with("MESSAGE",
                "Seed must be an integer from 1 through 4294967295."));
        mSeedInitialized = false;
        syncSeedEditor(true);
        return;
    }
    gSavedSettings.setU32("CineLightRigSeed", seed);
    mSeedEditor->setText(llformat("%u", seed));
    mDisplayedSeed = seed;
    mSeedInitialized = true;
}

void ALPanelCineLightRig::randomizeSeed()
{
    U32 seed = LLUUID::generateNewID().getCRC32();
    if (seed == 0)
    {
        seed = 1;
    }
    gSavedSettings.setU32("CineLightRigSeed", seed);
    mSeedInitialized = false;
    syncSeedEditor(true);
}

void ALPanelCineLightRig::syncSeedEditor(bool force)
{
    const U32 seed = gSavedSettings.getU32("CineLightRigSeed");
    if ((mSeedInitialized && seed == mDisplayedSeed) ||
        (!force && (mSeedEditor->hasFocus() ||
                    gFocusMgr.childHasKeyboardFocus(mSeedEditor))))
    {
        return;
    }
    mSeedEditor->setText(llformat("%u", seed));
    mDisplayedSeed = seed;
    mSeedInitialized = true;
}

S32 ALPanelCineLightRig::computeRequestedShadowSlots() const
{
    const S32 mode = std::clamp(
        gSavedSettings.getS32("CineLightRigShadowMode"), 0, 2);
    if (mode == 1)
    {
        return gSavedSettings.getBOOL("CineLightRigKeyOn") ? 1 : 0;
    }
    if (mode == 2)
    {
        S32 requested = 0;
        for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
        {
            requested += gSavedSettings.getBOOL(ROLE_ON_SETTINGS[i]) ? 1 : 0;
        }
        return requested;
    }
    return 0;
}

void ALPanelCineLightRig::onClickShadowFixIt()
{
    const S32 requested = computeRequestedShadowSlots();
    gSavedSettings.setU32("BDMergeMaxSpotShadows",
        std::clamp(static_cast<U32>(requested), 2u, 6u));
}

void ALPanelCineLightRig::updateDerivedStatus()
{
    ALCineLightRig& rig = ALCineLightRig::instance();
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        mClipStatus[i]->setVisible(rig.isClipped(i));
        mShaftControls[i]->setValue(rig.isShaftEnabled(i));
        mHeroControls[i]->setValue(rig.isHeroEnabled(i));
    }

    const S32 mode = std::clamp(
        gSavedSettings.getS32("CineLightRigShadowMode"), 0, 2);
    const S32 requested = computeRequestedShadowSlots();
    bool shaft_suppressed = false;
    bool shaft_on_dark_light = false;
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        if (rig.isShaftEnabled(i) &&
            (mode == 0 || (mode == 1 && i != 0)))
        {
            shaft_suppressed = true;
        }
        if (rig.isShaftEnabled(i) &&
            !gSavedSettings.getBOOL(ROLE_ON_SETTINGS[i]))
        {
            shaft_on_dark_light = true;
        }
    }
    const S32 slots = gSavedSettings.getS32("BDMergeMaxSpotShadows");
    const S32 hint_state = requested > slots ? 1
        : shaft_suppressed ? 2
        : shaft_on_dark_light ? 3 : 0;
    if (hint_state != mShadowHintState ||
        (hint_state == 1 &&
         (requested != mShadowHintRequested || slots != mShadowHintSlots)))
    {
        mShadowHint->setVisible(hint_state != 0);
        mShadowFixIt->setVisible(hint_state == 1);
        if (hint_state == 1)
        {
            const S32 displayed_request = std::clamp(requested, 2, 6);
            mShadowHint->setText(llformat(
                "%d rig projectors request %d shadow slots. Raise Max Spot "
                "Shadows or use Key only; shafts also require a slot.",
                displayed_request, slots));
            mShadowFixIt->setLabel(llformat(
                "Allow %d spot shadows", displayed_request));
        }
        else if (hint_state == 2)
        {
            mShadowHint->setText(std::string(
                "A shafted light is excluded by the current shadow policy. "
                "Shafts require a shadow slot; choose All projectors "
                "compete (or Key only for KEY)."));
        }
        else if (hint_state == 3)
        {
            mShadowHint->setText(std::string(
                "A shaft is enabled on an Off light. Turn that light On before "
                "it can request the shadow slot required for its shaft."));
        }
        mShadowHintState = hint_state;
        mShadowHintRequested = requested;
        mShadowHintSlots = slots;
    }
    const bool radius_over_ceiling =
        gSavedSettings.getF32("CineLightRigRadius") >
        ALCineLightRigModel::SCALED_RADIUS_CEIL;
    if (!mRadiusCueInitialized ||
        radius_over_ceiling != mRadiusOverCeiling)
    {
        mRadiusLabel->setColor(radius_over_ceiling
            ? LLUIColor(LLColor4(1.f, 0.75f, 0.25f, 1.f))
            : mRadiusDefaultColor);
        mRadiusOverCeiling = radius_over_ceiling;
        mRadiusCueInitialized = true;
    }
    const std::string selected = mSetupCombo
        ? mSetupCombo->getSelectedValue().asString() : std::string();
    mSetupDelete->setEnabled(!selected.empty() &&
                             !ALCineLightRig::isMasterSetup(selected) &&
                             !ALCineLightRig::isSetupDecorationName(selected));
}

void ALPanelCineLightRig::draw()
{
    updateAnchorList();
    syncGroupControls();
    if (mSetupRefreshPending &&
        !mSetupCombo->hasFocus() &&
        !gFocusMgr.childHasKeyboardFocus(mSetupCombo))
    {
        refreshSetupList(mPendingSetupSelection,
                         mPendingSetupAllowEmpty, true);
    }
    syncSeedEditor();
    updateDerivedStatus();
    LLPanel::draw();
}

void ALPanelCineLightRig::onVisibilityChange(bool new_visibility)
{
    if (new_visibility)
    {
        updateAnchorList();
        syncGroupControls();
        refreshSetupList(mSetupCombo
            ? mSetupCombo->getSelectedValue().asString() : std::string(),
            true);
        updateDerivedStatus();
    }
    LLPanel::onVisibilityChange(new_visibility);
}
