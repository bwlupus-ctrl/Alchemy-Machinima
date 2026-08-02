/**
 * @file alpaneldirectorswitcher.cpp
 * @brief Shared Director camera-switcher panel -- see the header.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpaneldirectorswitcher.h"

#include "alcameracurve.h"
#include "alpanelcinecamparams.h" // alRegisterMachinimaResetControl()
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcinematiccamera.h"
#include "llcombobox.h"
#include "llcontrol.h"
#include "lllineeditor.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "llviewercontrol.h"

#include <utility>

// The injector name must match class="panel_director_switcher" in every host.
static LLPanelInjector<ALPanelDirectorSwitcher>
    t_panel_director_switcher("panel_director_switcher");

namespace
{
constexpr S32 INVALID_SLOT = -1;
enum CustomField : S32
{
    CUSTOM_YAW,
    CUSTOM_PITCH,
    CUSTOM_DISTANCE,
    CUSTOM_HEIGHT,
    CUSTOM_FOV,
};

bool valid_slot(S32 slot)
{
    return slot >= 0 && slot < ALDirectorSwitcher::SLOT_COUNT;
}
} // anonymous namespace

ALPanelDirectorSwitcher::ALPanelDirectorSwitcher()
{
    // The XUI reset buttons resolve their callback while children are built.
    alRegisterMachinimaResetControl();
}

ALPanelDirectorSwitcher::~ALPanelDirectorSwitcher()
{
    ALDirectorSwitcher::instance().cancelEaseWorldTime();
}

bool ALPanelDirectorSwitcher::postBuild()
{
    for (S32 slot = 0; slot < ALDirectorSwitcher::SLOT_COUNT; ++slot)
    {
        const std::string name = llformat("switcher_slot_%02d", slot + 1);
        mSlotButtons[slot] = getChild<LLButton>(name);
        mSlotButtons[slot]->setCommitCallback(
            [this, slot](LLUICtrl*, const LLSD&) { onSlotButton(slot); });
    }

    mSlotEnabled = getChild<LLCheckBoxCtrl>("switcher_slot_enabled");
    mSlotMode = getChild<LLComboBox>("switcher_slot_mode");
    mPrimarySubject =
        getChild<LLComboBox>("switcher_primary_subject");
    mSecondarySubject =
        getChild<LLComboBox>("switcher_secondary_subject");
    mSlotLabel = getChild<LLLineEditor>("switcher_slot_label");
    mCustomEnabled =
        getChild<LLCheckBoxCtrl>("switcher_custom_enabled");
    mCustomYaw = getChild<LLSpinCtrl>("switcher_custom_yaw");
    mCustomPitch = getChild<LLSpinCtrl>("switcher_custom_pitch");
    mCustomDistance =
        getChild<LLSpinCtrl>("switcher_custom_distance");
    mCustomHeight = getChild<LLSpinCtrl>("switcher_custom_height");
    mCustomFov = getChild<LLSpinCtrl>("switcher_custom_fov");
    mCaptureCurrent =
        getChild<LLButton>("switcher_capture_current");
    mCaptureStatus = getChild<LLTextBox>("switcher_capture_status");
    mSelectedSlotText = getChild<LLTextBox>("switcher_selected_slot");
    mProgramText = getChild<LLTextBox>("switcher_program_status");
    getChild<LLButton>("switcher_reset_slots")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onResetSlots(); });

    if (LLControlVariable* control =
            gSavedSettings.getControl("DirectorSwitcherEaseCurve"))
    {
        mEaseCurveConnection = control->getCommitSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&)
            {
                updateCurveControlsEnabled();
            });
    }
    if (LLControlVariable* control =
            gSavedSettings.getControl("DirectorSwitcherFreezeCurve"))
    {
        mFreezeCurveConnection = control->getCommitSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&)
            {
                updateCurveControlsEnabled();
            });
    }

    // One assignment editor serves all twelve slots. This keeps the panel
    // compact while still exposing every existing motion and appended static
    // framing mode.
    for (S32 mode = LLCinematicCamera::MODE_BONE_LOCK;
         mode <= LLCinematicCamera::MODE_STATIC_FULL;
         ++mode)
    {
        if (LLCinematicCamera::migrateLegacyMode(mode) != mode)
        {
            continue;
        }
        mSlotMode->add(LLCinematicCamera::modeName(mode), LLSD(mode));
    }
    mPrimarySubject->add(
        "Default", LLSD(ALDirectorSwitcher::SUBJECT_DEFAULT));
    for (S32 subject = ALDirectorSwitcher::SUBJECT_A;
         subject <= ALDirectorSwitcher::SUBJECT_D; ++subject)
    {
        const std::string label(
            1, (char)('A' + subject - ALDirectorSwitcher::SUBJECT_A));
        mPrimarySubject->add(label, LLSD(subject));
        mSecondarySubject->add(label, LLSD(subject));
    }

    mSlotEnabled->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSlotEnabled(); });
    mSlotMode->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSlotMode(); });
    mSlotLabel->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSlotLabel(); });
    mPrimarySubject->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSlotSubject(); });
    mSecondarySubject->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSlotSubject(); });
    mCustomEnabled->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onCustomEnabled(); });
    for (LLSpinCtrl* spinner :
         { mCustomYaw, mCustomPitch, mCustomDistance,
           mCustomHeight, mCustomFov })
    {
        spinner->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onCustomValue(); });
    }
    mCaptureCurrent->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onCaptureCurrentView(); });
    const char* reset_names[] = {
        "reset_switcher_custom_yaw",
        "reset_switcher_custom_pitch",
        "reset_switcher_custom_distance",
        "reset_switcher_custom_height",
        "reset_switcher_custom_fov",
    };
    for (S32 field = CUSTOM_YAW; field <= CUSTOM_FOV; ++field)
    {
        getChild<LLButton>(reset_names[field])->setCommitCallback(
            [this, field](LLUICtrl*, const LLSD&)
            {
                onCustomReset(field);
            });
    }

    refreshBank(true);
    refreshProgramState();
    updateCurveControlsEnabled();
    return true;
}

void ALPanelDirectorSwitcher::updateCurveControlsEnabled()
{
    const bool ease_custom =
        gSavedSettings.getS32("DirectorSwitcherEaseCurve") ==
            ALCameraCurve::CUSTOM_BEZIER;
    const bool freeze_custom =
        gSavedSettings.getS32("DirectorSwitcherFreezeCurve") ==
            ALCameraCurve::CUSTOM_BEZIER;
    const char* ease_spinners[] = {
        "switcher_ease_bezier_x1", "switcher_ease_bezier_y1",
        "switcher_ease_bezier_x2", "switcher_ease_bezier_y2",
    };
    const char* freeze_spinners[] = {
        "switcher_freeze_bezier_x1", "switcher_freeze_bezier_y1",
        "switcher_freeze_bezier_x2", "switcher_freeze_bezier_y2",
    };
    for (const char* name : ease_spinners)
    {
        getChild<LLView>(name)->setEnabled(ease_custom);
    }
    for (const char* name : freeze_spinners)
    {
        getChild<LLView>(name)->setEnabled(freeze_custom);
    }
}

void ALPanelDirectorSwitcher::draw()
{
    // A mirrored panel or scene load can replace the LLSD bank without touching
    // this instance. Twelve small records are cheap to compare while visible.
    refreshBank(false);
    refreshProgramState();
    LLPanel::draw();
}

//static
ALPanelDirectorSwitcher::Bank ALPanelDirectorSwitcher::loadNormalizedBank()
{
    Bank bank = ALDirectorSwitcher::loadBank();

    if (bank.size() > ALDirectorSwitcher::SLOT_COUNT)
    {
        bank.resize(ALDirectorSwitcher::SLOT_COUNT);
    }
    while (bank.size() < ALDirectorSwitcher::SLOT_COUNT)
    {
        Slot fallback;
        fallback.mEnabled = true;
        fallback.mMode = LLCinematicCamera::MODE_STATIC_FULL;
        fallback.mLabel = llformat("Camera %d", (S32)bank.size() + 1);
        bank.push_back(std::move(fallback));
    }
    return bank;
}

//static
bool ALPanelDirectorSwitcher::banksEqual(const Bank& lhs, const Bank& rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i)
    {
        if (lhs[i].mEnabled != rhs[i].mEnabled ||
            lhs[i].mMode != rhs[i].mMode ||
            lhs[i].mLabel != rhs[i].mLabel ||
            lhs[i].mPrimarySubject != rhs[i].mPrimarySubject ||
            lhs[i].mSecondarySubject != rhs[i].mSecondarySubject ||
            lhs[i].mCustomEnabled != rhs[i].mCustomEnabled ||
            lhs[i].mCustomYawOffsetDeg != rhs[i].mCustomYawOffsetDeg ||
            lhs[i].mCustomPitchDeg != rhs[i].mCustomPitchDeg ||
            lhs[i].mCustomDistanceM != rhs[i].mCustomDistanceM ||
            lhs[i].mCustomHeightM != rhs[i].mCustomHeightM ||
            lhs[i].mCustomFovDeg != rhs[i].mCustomFovDeg)
        {
            return false;
        }
    }
    return true;
}

//static
std::string ALPanelDirectorSwitcher::displayLabel(const Slot& slot)
{
    if (!slot.mLabel.empty())
    {
        return slot.mLabel;
    }
    return LLCinematicCamera::modeName(slot.mMode);
}

void ALPanelDirectorSwitcher::onSlotButton(S32 slot)
{
    if (!valid_slot(slot))
    {
        return;
    }

    mSelectedSlot = slot;
    mCaptureStatus->setText(LLStringExplicit(""));
    refreshEditor();

    // Disarmed buttons remain useful for bank editing, but cannot acquire the
    // camera. Slot enabled is intentionally not checked: it only filters auto.
    if (gSavedSettings.getBOOL("DirectorSwitcherArmed"))
    {
        ALDirectorSwitcher::instance().punch(slot);
    }
    // Toggle buttons mutate their visual state before invoking the callback.
    // Force a full program mirror even when the attempted punch was a no-op
    // (same slot, disarmed, or no valid camera target).
    mHaveProgramSnapshot = false;
    refreshProgramState();
}

void ALPanelDirectorSwitcher::onSlotEnabled()
{
    if (mRefreshing || !valid_slot(mSelectedSlot) ||
        mBank.size() != ALDirectorSwitcher::SLOT_COUNT)
    {
        return;
    }
    mBank[mSelectedSlot].mEnabled = mSlotEnabled->getValue().asBoolean();
    saveBank();
}

void ALPanelDirectorSwitcher::onSlotMode()
{
    if (mRefreshing || !valid_slot(mSelectedSlot) ||
        mBank.size() != ALDirectorSwitcher::SLOT_COUNT)
    {
        return;
    }

    const S32 mode = mSlotMode->getValue().asInteger();
    if (mode < LLCinematicCamera::MODE_BONE_LOCK ||
        mode > LLCinematicCamera::MODE_STATIC_FULL)
    {
        return;
    }
    mBank[mSelectedSlot].mMode = mode;
    saveBank();
}

void ALPanelDirectorSwitcher::onSlotLabel()
{
    if (mRefreshing || !valid_slot(mSelectedSlot) ||
        mBank.size() != ALDirectorSwitcher::SLOT_COUNT)
    {
        return;
    }
    mBank[mSelectedSlot].mLabel = mSlotLabel->getText();
    saveBank();
}

void ALPanelDirectorSwitcher::onSlotSubject()
{
    if (mRefreshing || !valid_slot(mSelectedSlot) ||
        mBank.size() != ALDirectorSwitcher::SLOT_COUNT)
    {
        return;
    }
    Slot& slot = mBank[mSelectedSlot];
    slot.mPrimarySubject = mPrimarySubject->getValue().asInteger();
    slot.mSecondarySubject = mSecondarySubject->getValue().asInteger();
    saveBank(true);
}

void ALPanelDirectorSwitcher::onCustomEnabled()
{
    if (mRefreshing || !valid_slot(mSelectedSlot) ||
        mBank.size() != ALDirectorSwitcher::SLOT_COUNT)
    {
        return;
    }
    mBank[mSelectedSlot].mCustomEnabled =
        mCustomEnabled->getValue().asBoolean();
    saveBank(true);
}

void ALPanelDirectorSwitcher::onCustomValue()
{
    if (mRefreshing || !valid_slot(mSelectedSlot) ||
        mBank.size() != ALDirectorSwitcher::SLOT_COUNT)
    {
        return;
    }
    Slot& slot = mBank[mSelectedSlot];
    slot.mCustomYawOffsetDeg = (F32)mCustomYaw->getValue().asReal();
    slot.mCustomPitchDeg = (F32)mCustomPitch->getValue().asReal();
    slot.mCustomDistanceM = (F32)mCustomDistance->getValue().asReal();
    slot.mCustomHeightM = (F32)mCustomHeight->getValue().asReal();
    slot.mCustomFovDeg = (F32)mCustomFov->getValue().asReal();
    saveBank(true);
}

void ALPanelDirectorSwitcher::onCustomReset(S32 field)
{
    if (!valid_slot(mSelectedSlot) ||
        mBank.size() != ALDirectorSwitcher::SLOT_COUNT)
    {
        return;
    }
    Slot& slot = mBank[mSelectedSlot];
    switch (field)
    {
        case CUSTOM_YAW:      slot.mCustomYawOffsetDeg = 0.f; break;
        case CUSTOM_PITCH:    slot.mCustomPitchDeg = 0.f; break;
        case CUSTOM_DISTANCE: slot.mCustomDistanceM = 3.f; break;
        case CUSTOM_HEIGHT:   slot.mCustomHeightM = 1.3f; break;
        case CUSTOM_FOV:      slot.mCustomFovDeg = 60.f; break;
        default: return;
    }
    saveBank(true);
}

void ALPanelDirectorSwitcher::onCaptureCurrentView()
{
    if (!valid_slot(mSelectedSlot) ||
        mBank.size() != ALDirectorSwitcher::SLOT_COUNT)
    {
        return;
    }
    Slot& slot = mBank[mSelectedSlot];
    if (!LLCinematicCamera::instance().captureCurrentSwitcherView(
            slot.mPrimarySubject,
            slot.mCustomYawOffsetDeg, slot.mCustomPitchDeg,
            slot.mCustomDistanceM, slot.mCustomHeightM,
            slot.mCustomFovDeg))
    {
        mCaptureStatus->setText(
            LLStringExplicit("No subject available - view was not changed"));
        return;
    }

    // A custom rig is a static source. Preserve the selected authored static
    // vocabulary, but make Capture useful even when this slot held motion.
    if (slot.mMode < LLCinematicCamera::MODE_STATIC_WIDE ||
        slot.mMode > LLCinematicCamera::MODE_STATIC_FULL)
    {
        slot.mMode = LLCinematicCamera::MODE_STATIC_FULL;
    }
    slot.mCustomEnabled = true;
    mCaptureStatus->setText(
        LLStringExplicit("Captured - punch the slot to recall this view"));
    saveBank(true);
}

void ALPanelDirectorSwitcher::onResetSlots()
{
    // The setting default is the one canonical stock bank (Wide, Medium,
    // Close, Tight, OTS, Two-shot, profiles, angles, Full, Orbit). Resetting
    // this single control cannot touch arm/auto/scheduler settings.
    if (LLControlVariable* bank_control =
            gSavedSettings.getControl("DirectorSwitcherBank"))
    {
        bank_control->resetToDefault(true);
        refreshBank(true);
        mCaptureStatus->setText(
            LLStringExplicit("All 12 slots restored to stock assignments"));
    }
}

void ALPanelDirectorSwitcher::saveBank(bool retake_custom)
{
    ALDirectorSwitcher::saveBank(mBank);
    // Reload the canonical clamped representation before reflecting it.
    mBank = loadNormalizedBank();
    refreshButtons();
    refreshEditor();
    if (retake_custom &&
        gSavedSettings.getBOOL("DirectorSwitcherArmed") &&
        ALDirectorSwitcher::instance().activeSlot() == mSelectedSlot)
    {
        ALDirectorSwitcher::instance().punch(mSelectedSlot);
    }
    // The active program may be this slot, so its displayed label can change
    // even though the engine's active-slot index did not.
    mHaveProgramSnapshot = false;
}

void ALPanelDirectorSwitcher::refreshBank(bool force)
{
    Bank latest = loadNormalizedBank();
    if (!force && banksEqual(latest, mBank))
    {
        return;
    }
    mBank = std::move(latest);
    refreshButtons();
    refreshEditor();
    mHaveProgramSnapshot = false;
}

void ALPanelDirectorSwitcher::refreshButtons()
{
    if (mBank.size() != ALDirectorSwitcher::SLOT_COUNT)
    {
        return;
    }

    for (S32 slot = 0; slot < ALDirectorSwitcher::SLOT_COUNT; ++slot)
    {
        LLButton* button = mSlotButtons[slot];
        if (!button)
        {
            continue;
        }
        const Slot& entry = mBank[slot];
        const std::string label = displayLabel(entry);
        const std::string mode_label =
            LLCinematicCamera::modeName(entry.mMode);
        button->setLabel(llformat("%d %s", slot + 1, label.c_str()));
        button->setToolTip(LLStringExplicit(llformat(
            "Slot %d: %s (%s). Click to select it; while armed, also cut to it. "
            "Use in auto only controls automatic selection.",
            slot + 1, label.c_str(), mode_label.c_str())));
    }
}

void ALPanelDirectorSwitcher::refreshEditor()
{
    if (!valid_slot(mSelectedSlot) ||
        mBank.size() != ALDirectorSwitcher::SLOT_COUNT)
    {
        return;
    }

    const Slot& slot = mBank[mSelectedSlot];
    mRefreshing = true;
    mSelectedSlotText->setText(
        LLStringExplicit(llformat("Slot %d setup", mSelectedSlot + 1)));
    mSlotEnabled->setValue(slot.mEnabled);
    mSlotMode->setValue(LLSD(slot.mMode));
    mSlotLabel->setText(LLStringExplicit(slot.mLabel));
    mPrimarySubject->setValue(LLSD(slot.mPrimarySubject));
    mSecondarySubject->setValue(LLSD(slot.mSecondarySubject));
    mSecondarySubject->setEnabled(
        slot.mMode == LLCinematicCamera::MODE_OTS ||
        slot.mMode == LLCinematicCamera::MODE_TWO_SHOT);
    mCustomEnabled->setValue(slot.mCustomEnabled);
    mCustomYaw->setValue(slot.mCustomYawOffsetDeg);
    mCustomPitch->setValue(slot.mCustomPitchDeg);
    mCustomDistance->setValue(slot.mCustomDistanceM);
    mCustomHeight->setValue(slot.mCustomHeightM);
    mCustomFov->setValue(slot.mCustomFovDeg);
    for (LLSpinCtrl* spinner :
         { mCustomYaw, mCustomPitch, mCustomDistance,
           mCustomHeight, mCustomFov })
    {
        spinner->setEnabled(slot.mCustomEnabled);
    }
    mRefreshing = false;
}

void ALPanelDirectorSwitcher::refreshProgramState()
{
    ALDirectorSwitcher& switcher = ALDirectorSwitcher::instance();
    const bool armed = gSavedSettings.getBOOL("DirectorSwitcherArmed");
    const bool driving = switcher.isDrivingCamera();
    const S32 active = driving ? switcher.activeSlot() : INVALID_SLOT;

    if (mHaveProgramSnapshot &&
        armed == mLastArmed &&
        driving == mLastDriving &&
        active == mLastActiveSlot)
    {
        return;
    }

    for (S32 slot = 0; slot < ALDirectorSwitcher::SLOT_COUNT; ++slot)
    {
        if (mSlotButtons[slot])
        {
            mSlotButtons[slot]->setToggleState(driving && active == slot);
        }
    }

    if (driving && valid_slot(active) &&
        mBank.size() == ALDirectorSwitcher::SLOT_COUNT)
    {
        mProgramText->setText(LLStringExplicit(llformat(
            "PROGRAM  %d  %s", active + 1,
            displayLabel(mBank[active]).c_str())));
    }
    else if (armed)
    {
        mProgramText->setText(
            LLStringExplicit("Armed - click a slot to put it on program"));
    }
    else
    {
        mProgramText->setText(
            LLStringExplicit("Disarmed - slot buttons select for editing only"));
    }

    mLastArmed = armed;
    mLastDriving = driving;
    mLastActiveSlot = active;
    mHaveProgramSnapshot = true;
}
