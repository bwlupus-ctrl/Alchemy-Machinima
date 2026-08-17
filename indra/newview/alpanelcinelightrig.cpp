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
#include "alcinelightrigmanager.h"
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
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lluicolortable.h"
#include "llviewercontrol.h"
#include "pipeline.h"

#include <algorithm>
#include <cmath>

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
        "CineLightRigAutoShadowSlots",
        "CineLightRigGizmo",
        "CineLightRigRatioLock",
        "CineLightRigRatio",
        "CineLightRigCatchlight",
        "CineLightRigCatchlightEV",
        "CineLightRigCatchlightSize",
        "CineLightRigCatchlightAngle",
        "CineLightRigKeyYaw",
        "CineLightRigKeyPitch",
        "CineLightRigKeyProfile",
        "CineLightRigKeyEV",
        "CineLightRigKeyBeam",
        "CineLightRigKeyGobo",
        "CineLightRigKeyGel",
        "CineLightRigKeyShadowSoft",
        "CineLightRigKeyOn",
        "CineLightRigFillYaw",
        "CineLightRigFillPitch",
        "CineLightRigFillProfile",
        "CineLightRigFillEV",
        "CineLightRigFillBeam",
        "CineLightRigFillGobo",
        "CineLightRigFillGel",
        "CineLightRigFillShadowSoft",
        "CineLightRigFillOn",
        "CineLightRigRimYaw",
        "CineLightRigRimPitch",
        "CineLightRigRimProfile",
        "CineLightRigRimEV",
        "CineLightRigRimBeam",
        "CineLightRigRimGobo",
        "CineLightRigRimGel",
        "CineLightRigRimShadowSoft",
        "CineLightRigRimOn",
        "CineLightRigBgYaw",
        "CineLightRigBgPitch",
        "CineLightRigBgProfile",
        "CineLightRigBgEV",
        "CineLightRigBgBeam",
        "CineLightRigBgGobo",
        "CineLightRigBgGel",
        "CineLightRigBgShadowSoft",
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
    mFillEV = getChild<LLSpinCtrl>("cine_fill_ev");
    mEasyBrightness = getChild<LLUICtrl>("cine_easy_brightness");
    mEasyDrama = getChild<LLUICtrl>("cine_easy_drama");
    mEasyRim = getChild<LLComboBox>("cine_easy_rim");
    mEasyBg = getChild<LLComboBox>("cine_easy_bg");
    mEasyWarmth = getChild<LLUICtrl>("cine_easy_warmth");
    mEasyModeToggle = getChild<LLCheckBoxCtrl>("cine_easy_mode");
    const char* const advanced_driven_names[] = {
        "cine_master_ev", "cine_master_ev_reset",
        "cine_key_ev", "cine_key_ev_reset",
        "cine_fill_ev", "cine_fill_ev_reset",
        "cine_rim_ev", "cine_rim_ev_reset",
        "cine_bg_ev", "cine_bg_ev_reset",
        "cine_ratio_lock", "cine_ratio_lock_reset",
        "cine_ratio", "cine_ratio_reset",
    };
    for (const char* name : advanced_driven_names)
    {
        mAdvancedDrivenControls.push_back(getChild<LLUICtrl>(name));
    }
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
        [](LLUICtrl*, const LLSD&)
        { ALCineLightRigManager::instance().selected().stopFX(); });
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
    mEasyModeToggle->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onEasyModeCommit(); });
    mEasyBrightness->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onEasyBrightnessCommit(); });
    mEasyDrama->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onEasyDramaCommit(); });
    mEasyRim->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onEasyRimCommit(); });
    mEasyBg->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onEasyBgCommit(); });
    mEasyWarmth->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onEasyWarmthCommit(); });
    getChild<LLUICtrl>("cine_manual")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onManualCommit(); });
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
                ALCineLightRigManager::instance().selected().setShaftEnabled(
                    i, control->getValue().asBoolean());
            });
        mHeroControls[i]->setCommitCallback(
            [i](LLUICtrl* control, const LLSD&)
            {
                ALCineLightRigManager::instance().selected().setHeroEnabled(
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
        LLComboBox* gel = getChild<LLComboBox>(widget_prefix + "_gel");
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
        gel->add(ALCineLightRigModel::gelName(0), LLSD(0));
        addLabeledSeparator(gel, "Colour Temp", true);
        for (S32 i = 1; i < ALCineLightRigModel::GEL_COUNT; ++i)
        {
            if (ALCineLightRigModel::gelIsColourTemperature(i))
            {
                gel->add(ALCineLightRigModel::gelName(i), LLSD(i));
            }
        }
        addLabeledSeparator(gel, "Colour", true);
        for (S32 i = 1; i < ALCineLightRigModel::GEL_COUNT; ++i)
        {
            if (!ALCineLightRigModel::gelIsColourTemperature(i))
            {
                gel->add(ALCineLightRigModel::gelName(i), LLSD(i));
            }
        }
        const std::string setting_prefix =
            std::string("CineLightRig") + ROLE_NAMES[light];
        profile->setValue(gSavedSettings.getS32(setting_prefix + "Profile"));
        beam->setValue(gSavedSettings.getS32(setting_prefix + "Beam"));
        gobo->setValue(gSavedSettings.getS32(setting_prefix + "Gobo"));
        gel->setValue(gSavedSettings.getS32(setting_prefix + "Gel"));
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
    LLDirectorCast& cast = LLDirectorCast::instance();
    ALCineLightRigManager& manager = ALCineLightRigManager::instance();
    const LLUUID subject_ids[5] = {
        LLUUID::null, cast.getSubjectA(), cast.getSubjectB(),
        cast.getSubjectC(), cast.getSubjectD()
    };
    const char* const subject_names[5] = {
        "You", "Subject A", "Subject B", "Subject C", "Subject D"
    };
    std::vector<LLUUID> ids;
    std::vector<std::string> labels;
    ids.reserve(5);
    labels.reserve(5);
    for (S32 i = 0; i < 5; ++i)
    {
        const ALCineLightRigManager::Slot slot =
            static_cast<ALCineLightRigManager::Slot>(i);
        std::string label(subject_names[i]);
        if (i > 0)
        {
            std::string resolved_name("unset");
            if (subject_ids[i].notNull())
            {
                const LLDirectorCast::CastMember* member =
                    cast.getMember(subject_ids[i]);
                resolved_name = member && !member->mLastName.empty()
                    ? member->mLastName : subject_ids[i].asString();
            }
            label += " - " + resolved_name;
        }
        if (manager.isSlotLit(slot))
        {
            label = "\xE2\x97\x8F " + label; // filled circle: lit
        }
        else if (manager.isSlotEnabled(slot))
        {
            label = "\xE2\x97\x8B " + label; // hollow circle: enabled/dark
        }
        ids.push_back(subject_ids[i]);
        labels.push_back(label);
    }
    const bool changed = !mCastListInitialized || ids != mCastIds ||
                         labels != mCastNames;
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

    mCastIds = ids;
    mCastNames = labels;
    mAnchorCombo->clearRows();
    for (S32 i = 0; i < 5; ++i)
    {
        mAnchorCombo->add(labels[i], LLSD(i));
    }
    mCastListInitialized = true;
    mAnchorSelectionInitialized = false;
    syncAnchorSelection();
}

void ALPanelCineLightRig::syncAnchorSelection()
{
    const S32 selected = static_cast<S32>(
        ALCineLightRigManager::instance().selectedSlot());
    if ((mAnchorSelectionInitialized && selected == mDisplayedSlot) ||
        mAnchorCombo->hasFocus() ||
        gFocusMgr.childHasKeyboardFocus(mAnchorCombo))
    {
        return;
    }
    mAnchorCombo->setValue(selected);
    mDisplayedSlot = selected;
    mAnchorSelectionInitialized = true;
    mDisplayedGroupEnabled = false;
    mDisplayedGroupSlots = ~0u;
    mDisplayedResolvedSlots = ~0u;
    mSeedInitialized = false;
    mShadowHintState = -1;
    mRadiusCueInitialized = false;
}

void ALPanelCineLightRig::onAnchorSelected()
{
    if (mAnchorCombo)
    {
        const S32 value = mAnchorCombo->getSelectedValue().asInteger();
        if (value < 0 || value >= ALCineLightRigManager::SLOT_COUNT)
        {
            return;
        }
        ALCineLightRigManager::instance().setSelectedSlot(
            static_cast<ALCineLightRigManager::Slot>(value));
        mDisplayedSlot = value;
        mAnchorSelectionInitialized = true;
        mDisplayedGroupEnabled = false;
        mDisplayedGroupSlots = ~0u;
        mDisplayedResolvedSlots = ~0u;
        mSeedInitialized = false;
        mShadowHintState = -1;
        mRadiusCueInitialized = false;
    }
}

void ALPanelCineLightRig::onGroupEnabledCommit()
{
    ALCineLightRigManager::instance().selected().setGroupEnabled(
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
    ALCineLightRigManager::instance().selected().setGroupSlots(slots);
    syncGroupControls();
}

void ALPanelCineLightRig::syncGroupControls()
{
    ALCineLightRig& rig = ALCineLightRigManager::instance().selected();
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
    bool added_genre_caption = false;
    bool added_local_caption = false;
    for (const ALCineLightRig::SetupEntry& entry :
         ALCineLightRigManager::instance().selected().setupNamesGrouped())
    {
        if (entry.mMaster && entry.mGenre && !added_genre_caption)
        {
            addLabeledSeparator(
                mSetupCombo, ALCineLightRig::GENRE_SETUP_CAPTION, true);
            added_genre_caption = true;
        }
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
    if (!name.empty() &&
        ALCineLightRigManager::instance().selected().loadSetup(name))
    {
        mSetupCombo->setValue(name);
        syncEasyModeForSelected(true);
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
    if (!ALCineLightRigManager::instance().selected().saveSetup(name))
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
        if (!ALCineLightRigManager::instance().selected().deleteSetup(name))
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
    ALCineLightRigManager& manager = ALCineLightRigManager::instance();
    ALCineLightRig& rig = manager.selected();
    rig.setAnchor(LLUUID::null);
    rig.setGroupEnabled(false);
    rig.setGroupSlots(0);
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        rig.setShaftEnabled(i, false);
        rig.setHeroEnabled(i, false);
    }
    manager.resetAllToSelf();
    mAnchorSelectionInitialized = false;
    mDisplayedSlot = -1;
    mDisplayedGroupSlots = ~0u;
    mDisplayedResolvedSlots = ~0u;
    mSeedInitialized = false;
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
    return static_cast<S32>(
        ALCineLightRigManager::instance().requestedShadowSlots(
            LLPipeline::MAX_SPOT_SHADOWS));
}

bool ALPanelCineLightRig::selectedIsEasyNative() const
{
    if (std::fabs(gSavedSettings.getF32("CineLightRigKeyEV")) > 0.01f)
    {
        return false;
    }

    const auto is_bucket = [](F32 ev, bool rim)
    {
        for (S32 presence = 1; presence < 4; ++presence)
        {
            const F32 bucket = rim
                ? ALCineLightRigModel::easyRimEV(presence)
                : ALCineLightRigModel::easyBgEV(presence);
            if (std::fabs(ev - bucket) <= 0.01f)
            {
                return true;
            }
        }
        return false;
    };

    return (!gSavedSettings.getBOOL("CineLightRigRimOn") ||
            is_bucket(gSavedSettings.getF32("CineLightRigRimEV"), true)) &&
           (!gSavedSettings.getBOOL("CineLightRigBgOn") ||
            is_bucket(gSavedSettings.getF32("CineLightRigBgEV"), false));
}

bool ALPanelCineLightRig::normalizeSelectedForEasy()
{
    // Fold the key's own EV into Master so subject exposure is preserved while
    // Key is anchored at 0. If the folded value cannot be represented within
    // Master EV's +/-16 range, refuse the fold and leave the rig for Advanced
    // (render would otherwise clamp Master and silently drop exposure).
    const F32 key_ev = gSavedSettings.getF32("CineLightRigKeyEV");
    const F32 folded = gSavedSettings.getF32("CineLightRigMasterEV") + key_ev;
    if (std::fabs(folded) > 16.f + 0.01f)
    {
        return false;
    }
    gSavedSettings.setF32("CineLightRigMasterEV", folded);
    gSavedSettings.setF32("CineLightRigKeyEV", 0.f);
    gSavedSettings.setBOOL("CineLightRigKeyOn", true);
    return true;
}

void ALPanelCineLightRig::syncEasyModeForSelected(bool force)
{
    const S32 selected = static_cast<S32>(
        ALCineLightRigManager::instance().selectedSlot());
    // Recompute the desired mode on every call, not just on a slot change: a
    // Director scene or preset can replace the selected slot's settings in
    // place. If that makes an active-Easy instance no longer Easy-native (e.g.
    // an old scene restoring a non-zero Key EV), we must demote to Advanced so
    // the disabled EV/Ratio controls re-enable and a later Brightness commit
    // does not zero Key EV without folding it into Master.
    const bool want_active =
        gSavedSettings.getBOOL("CineLightRigEasyMode") &&
        selectedIsEasyNative();
    if (!force && selected == mEasyModeSlot && want_active == mEasyModeActive)
    {
        return;
    }

    mEasyModeSlot = selected;
    mEasyModeActive = want_active;
    if (mEasyModeActive)
    {
        // First show and an instance switch both enter Easy only for an
        // already Easy-native instance (Key EV ~ 0), so this fold is a no-op
        // here; explicit entry via the toggle is what folds a real Key EV.
        // If it somehow cannot be represented, stay in Advanced.
        if (!normalizeSelectedForEasy())
        {
            mEasyModeActive = false;
        }
    }
}

void ALPanelCineLightRig::onEasyModeCommit()
{
    if (mSyncingEasyControls)
    {
        return;
    }
    const bool desired = mEasyModeToggle->getValue().asBoolean();
    // Record the user's preference even if this particular look cannot enter
    // Easy, so other (Easy-native) instances still open in Easy.
    gSavedSettings.setBOOL("CineLightRigEasyMode", desired);
    mEasyModeSlot = static_cast<S32>(
        ALCineLightRigManager::instance().selectedSlot());
    mEasyModeActive = desired;
    if (mEasyModeActive && !normalizeSelectedForEasy())
    {
        // Exposure would exceed Master EV's range (an extreme Advanced look);
        // keep this instance in Advanced. syncEasyControls reflects it.
        mEasyModeActive = false;
    }
    syncEasyControls();
}

void ALPanelCineLightRig::onEasyBrightnessCommit()
{
    if (mSyncingEasyControls || !mEasyModeActive)
    {
        return;
    }
    gSavedSettings.setF32(
        "CineLightRigMasterEV",
        ALCineLightRigModel::easyBrightnessClamp(
            static_cast<F32>(mEasyBrightness->getValue().asReal())));
    gSavedSettings.setF32("CineLightRigKeyEV", 0.f);
    gSavedSettings.setBOOL("CineLightRigKeyOn", true);
}

void ALPanelCineLightRig::onEasyDramaCommit()
{
    if (mSyncingEasyControls || !mEasyModeActive)
    {
        return;
    }
    gSavedSettings.setBOOL("CineLightRigRatioLock", true);
    gSavedSettings.setF32(
        "CineLightRigRatio",
        ALCineLightRigModel::easyDramaClamp(
            static_cast<F32>(mEasyDrama->getValue().asReal())));
    gSavedSettings.setBOOL("CineLightRigFillOn", true);
}

void ALPanelCineLightRig::onEasyRimCommit()
{
    if (mSyncingEasyControls || !mEasyModeActive)
    {
        return;
    }
    const S32 presence = std::clamp(
        mEasyRim->getValue().asInteger(), 0, 3);
    const bool on = ALCineLightRigModel::easyRimOn(presence);
    gSavedSettings.setBOOL("CineLightRigRimOn", on);
    if (on)
    {
        gSavedSettings.setF32(
            "CineLightRigRimEV",
            ALCineLightRigModel::easyRimEV(presence));
    }
}

void ALPanelCineLightRig::onEasyBgCommit()
{
    if (mSyncingEasyControls || !mEasyModeActive)
    {
        return;
    }
    const S32 presence = std::clamp(
        mEasyBg->getValue().asInteger(), 0, 3);
    const bool on = ALCineLightRigModel::easyBgOn(presence);
    gSavedSettings.setBOOL("CineLightRigBgOn", on);
    if (on)
    {
        gSavedSettings.setF32(
            "CineLightRigBgEV",
            ALCineLightRigModel::easyBgEV(presence));
    }
}

void ALPanelCineLightRig::onEasyWarmthCommit()
{
    if (mSyncingEasyControls || !mEasyModeActive)
    {
        return;
    }
    gSavedSettings.setF32(
        "CineLightRigMasterTempMired",
        std::clamp(static_cast<F32>(mEasyWarmth->getValue().asReal()),
                   ALCineLightRigModel::MASTER_TEMP_MIRED_MIN,
                   ALCineLightRigModel::MASTER_TEMP_MIRED_MAX));
}

void ALPanelCineLightRig::onManualCommit()
{
    // The check box is control_name-bound, so CineLightRigManual is already
    // updated; just apply the side effects (force FX off + lock the selector).
    applyManualLock();
}

void ALPanelCineLightRig::applyManualLock()
{
    const bool manual = gSavedSettings.getBOOL("CineLightRigManual");
    if (manual && gSavedSettings.getS32("CineLightRigFX") != -1)
    {
        // Manual owns the pose; drop any running effect and hold it off.
        gSavedSettings.setS32("CineLightRigFX", -1);
    }
    // Lock the effect selector while manual so an effect cannot silently take
    // over per-light aim/exposure/colour again.
    mFXCombo->setEnabled(!manual);
    getChild<LLUICtrl>("cine_fx_stop")->setEnabled(!manual);
}

void ALPanelCineLightRig::syncEasyControls()
{
    mSyncingEasyControls = true;
    mEasyModeToggle->setValue(mEasyModeActive);

    const auto set_unfocused = [](LLUICtrl* control, const LLSD& value)
    {
        if (!control->hasFocus() &&
            !gFocusMgr.childHasKeyboardFocus(control))
        {
            control->setValue(value);
        }
    };
    set_unfocused(mEasyBrightness, LLSD(
        ALCineLightRigModel::easyBrightnessClamp(
            gSavedSettings.getF32("CineLightRigMasterEV"))));
    const F32 drama = gSavedSettings.getBOOL("CineLightRigRatioLock")
        ? gSavedSettings.getF32("CineLightRigRatio")
        : gSavedSettings.getF32("CineLightRigKeyEV") -
          gSavedSettings.getF32("CineLightRigFillEV");
    set_unfocused(mEasyDrama, LLSD(
        ALCineLightRigModel::easyDramaClamp(drama)));
    set_unfocused(mEasyRim, LLSD(
        ALCineLightRigModel::rimPresenceFromEV(
            gSavedSettings.getBOOL("CineLightRigRimOn"),
            gSavedSettings.getF32("CineLightRigRimEV"))));
    set_unfocused(mEasyBg, LLSD(
        ALCineLightRigModel::bgPresenceFromEV(
            gSavedSettings.getBOOL("CineLightRigBgOn"),
            gSavedSettings.getF32("CineLightRigBgEV"))));
    set_unfocused(mEasyWarmth, LLSD(
        gSavedSettings.getF32("CineLightRigMasterTempMired")));

    mEasyBrightness->setEnabled(mEasyModeActive);
    mEasyDrama->setEnabled(mEasyModeActive);
    mEasyRim->setEnabled(mEasyModeActive);
    mEasyBg->setEnabled(mEasyModeActive);
    mEasyWarmth->setEnabled(mEasyModeActive);
    for (LLUICtrl* control : mAdvancedDrivenControls)
    {
        control->setEnabled(!mEasyModeActive);
    }
    mSyncingEasyControls = false;
}

void ALPanelCineLightRig::onClickShadowFixIt()
{
    const S32 requested = computeRequestedShadowSlots();
    gSavedSettings.setU32("BDMergeMaxSpotShadows",
        std::clamp(static_cast<U32>(requested), 2u, 10u));
}

void ALPanelCineLightRig::updateDerivedStatus()
{
    syncEasyModeForSelected();
    syncEasyControls();
    applyManualLock();
    ALCineLightRig& rig = ALCineLightRigManager::instance().selected();
    const bool ratio_locked =
        gSavedSettings.getBOOL("CineLightRigRatioLock");
    mFillEV->setEnabled(!mEasyModeActive && !ratio_locked);
    if (!mFillEV->hasFocus() && !gFocusMgr.childHasKeyboardFocus(mFillEV))
    {
        const F32 displayed_fill_ev = ratio_locked
            ? gSavedSettings.getF32("CineLightRigKeyEV") -
                std::clamp(gSavedSettings.getF32("CineLightRigRatio"),
                           0.f, 5.f)
            : gSavedSettings.getF32("CineLightRigFillEV");
        mFillEV->setValue(displayed_fill_ev);
    }
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
            const S32 displayed_request = std::clamp(requested, 2, 10);
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
