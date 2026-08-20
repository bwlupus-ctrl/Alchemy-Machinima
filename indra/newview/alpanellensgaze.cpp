/**
 * @file alpanellensgaze.cpp
 * @brief Shared Actor Gaze controls -- see alpanellensgaze.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanellensgaze.h"

#include "llactormover.h"
#include "alpanelcinecamparams.h"
#include "llagent.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldirectorcast.h"
#include "llfloaterreg.h"
#include "llselectmgr.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llvoavatarself.h"

static LLPanelInjector<ALPanelLensGaze> t_panel_lens_gaze("panel_lens_gaze");

namespace
{
std::string actorName(const LLUUID& id)
{
    if (const LLDirectorCast::CastMember* member =
            LLDirectorCast::instance().getMember(id))
    {
        if (!member->mLastName.empty())
        {
            return member->mLastName;
        }
    }
    if (LLAvatarName name; LLAvatarNameCache::get(id, &name))
    {
        return name.getCompleteName();
    }
    return id.asString().substr(0, 8);
}

bool isEditing(LLUICtrl* control)
{
    return control && (control->hasFocus() || control->hasMouseCapture());
}

bool targetNeedsDetail(S32 mode)
{
    return mode == LLActorMover::GAZE_CAST ||
           mode == LLActorMover::GAZE_POINT ||
           mode == LLActorMover::GAZE_OBJECT;
}

struct GazePerformancePreset
{
    const char* mName;
    F32 mDominance;
    F32 mAffection;
    F32 mAnxiety;
    F32 mHeadEye;
    F32 mTorso;
    F32 mIntensity;
    F32 mSmoothing;
    F32 mEyelineYaw;
    F32 mEyelinePitch;
    F32 mMicroLife;
    S32 mBlinks;
    F32 mBlinkRate;
    F32 mVariation;
    F32 mBreakFrequency;
    F32 mAcquire;
    F32 mRelease;
    F32 mDeadZoneDeg;
    F32 mVergence;
};

constexpr F32 GAZE_PRESET_INHERIT = -1000.f;

// Performance library: the first 12 retain their original overrides and order;
// their new columns inherit. Expanded rows follow the document's column order,
// plus the pre-existing blink-rate and vergence fields.
const GazePerformancePreset GAZE_PRESETS[] =
{
    { "Intense Lock",        0.88f,  0.20f, -0.25f, 0.95f, -1.f, -1.f, 0.25f, GAZE_PRESET_INHERIT, GAZE_PRESET_INHERIT, 0.18f, -1, 0.65f, -1.f, 0.05f, 0.16f, 0.55f, -1.f, 1.f },
    { "Nervous Avoider",    -0.65f, -0.15f,  0.92f, 0.42f, -1.f, -1.f, 0.58f, GAZE_PRESET_INHERIT, GAZE_PRESET_INHERIT, 0.85f, -1, 1.45f, -1.f, 0.90f, 0.40f, 0.75f, -1.f, 1.f },
    { "Runway Confidence",   0.82f,  0.05f, -0.35f, 0.92f, -1.f, -1.f, 0.20f, GAZE_PRESET_INHERIT, GAZE_PRESET_INHERIT, 0.16f, -1, 0.70f, -1.f, 0.08f, 0.18f, 0.45f, -1.f, 1.f },
    { "Interrogation Stare", 1.00f, -0.55f, -0.20f, 1.00f, -1.f, -1.f, 0.18f, GAZE_PRESET_INHERIT, GAZE_PRESET_INHERIT, 0.08f, -1, 0.25f, -1.f, 0.00f, 0.12f, 0.40f, -1.f, 1.f },
    { "Lovers' Exchange",    0.22f,  1.00f, -0.35f, 0.78f, -1.f, -1.f, 0.62f, GAZE_PRESET_INHERIT, GAZE_PRESET_INHERIT, 0.30f, -1, 0.80f, -1.f, 0.18f, 0.42f, 0.95f, -1.f, 1.f },
    { "Cold Read",           0.58f, -0.78f,  0.20f, 0.78f, -1.f, -1.f, 0.35f, GAZE_PRESET_INHERIT, GAZE_PRESET_INHERIT, 0.24f, -1, 0.70f, -1.f, 0.22f, 0.24f, 0.62f, -1.f, 1.f },
    { "Grieving Distance",  -0.42f,  0.25f,  0.68f, 0.48f, -1.f, -1.f, 0.78f, GAZE_PRESET_INHERIT, GAZE_PRESET_INHERIT, 0.22f, -1, 0.65f, -1.f, 0.38f, 0.65f, 1.35f, -1.f, 0.65f },
    { "Fan Meeting Idol",    0.18f,  0.88f,  0.62f, 0.72f, -1.f, -1.f, 0.48f, GAZE_PRESET_INHERIT, GAZE_PRESET_INHERIT, 0.62f, -1, 1.25f, -1.f, 0.50f, 0.34f, 0.80f, -1.f, 1.f },
    { "Guilty Party",       -0.72f, -0.38f,  1.00f, 0.36f, -1.f, -1.f, 0.52f, GAZE_PRESET_INHERIT, GAZE_PRESET_INHERIT, 0.92f, -1, 1.60f, -1.f, 1.00f, 0.46f, 0.90f, -1.f, 1.f },
    { "The Bodyguard",       0.68f, -0.62f,  0.58f, 0.70f, -1.f, -1.f, 0.28f, GAZE_PRESET_INHERIT, GAZE_PRESET_INHERIT, 0.48f, -1, 0.85f, -1.f, 0.52f, 0.18f, 0.48f, -1.f, 1.f },
    { "Doting Parent",       0.18f,  1.00f, -0.18f, 0.82f, -1.f, -1.f, 0.66f, GAZE_PRESET_INHERIT, GAZE_PRESET_INHERIT, 0.34f, -1, 0.85f, -1.f, 0.16f, 0.48f, 1.05f, -1.f, 1.f },
    { "Thousand-Yard",      -0.22f, -0.30f, -0.35f, 0.30f, -1.f, -1.f, 0.92f, GAZE_PRESET_INHERIT, GAZE_PRESET_INHERIT, 0.03f, -1, 0.50f, -1.f, 0.04f, 1.20f, 1.60f, -1.f, 0.f },

    { "Predator",             0.85f, -0.20f, -0.30f, 0.70f, 0.40f, 1.00f, 0.70f, 0.f,  0.f, 0.15f, 0, 1.f, 0.10f, 0.00f, 0.60f, 1.20f, 2.f, 1.f },
    { "Menacing Villain",     0.95f, -0.50f, -0.20f, 0.85f, 0.50f, 1.00f, 0.50f, 0.f,  1.f, 0.10f, 0, 1.f, 0.05f, 0.00f, 0.50f, 1.00f, 3.f, 1.f },
    { "Commanding Orator",    0.70f,  0.10f, -0.10f, 0.90f, 0.60f, 0.90f, 0.50f, 0.f,  0.f, 0.25f, 1, 1.f, 0.20f, 0.30f, 0.40f, 0.80f, 4.f, 1.f },
    { "Defiant Challenge",     0.75f, -0.15f,  0.25f, 0.70f, 0.40f, 1.00f, 0.40f, 0.f, -4.f, 0.20f, 1, 1.f, 0.15f, 0.00f, 0.30f, 1.00f, 2.f, 1.f },
    { "Seductive",             0.30f,  0.70f,  0.10f, 0.50f, 0.30f, 0.90f, 0.70f, 3.f,  2.f, 0.30f, 1, 1.f, 0.25f, 0.35f, 0.90f, 1.40f, 3.f, 1.f },
    { "Smitten",               0.00f,  0.80f,  0.35f, 0.60f, 0.30f, 0.90f, 0.60f, 0.f,  2.f, 0.35f, 1, 1.f, 0.30f, 0.20f, 0.70f, 1.20f, 3.f, 1.f },
    { "Flirty-Shy",            0.20f,  0.60f,  0.40f, 0.45f, 0.20f, 0.85f, 0.55f, 4.f,  1.f, 0.40f, 1, 1.f, 0.35f, 0.50f, 0.50f, 0.90f, 3.f, 1.f },
    { "Panic",                -0.30f, -0.10f,  0.95f, 0.40f, 0.20f, 0.85f, 0.20f, 0.f,  0.f, 0.90f, 1, 1.f, 0.50f, 0.85f, 0.15f, 0.40f, 1.f, 1.f },
    { "Vulnerable / Pleading",-0.40f,  0.50f,  0.70f, 0.55f, 0.30f, 0.90f, 0.50f, 0.f,  5.f, 0.45f, 1, 1.f, 0.30f, 0.30f, 0.50f, 1.00f, 3.f, 1.f },
    { "Hostile Witness",      -0.10f, -0.40f,  0.75f, 0.40f, 0.20f, 0.80f, 0.35f, 0.f, -2.f, 0.50f, 1, 1.f, 0.40f, 0.80f, 0.30f, 0.60f, 2.f, 1.f },
    { "Bored / Checked-out",  -0.20f, -0.20f, -0.10f, 0.25f, 0.10f, 0.60f, 0.70f, 0.f, -2.f, 0.50f, 1, 1.f, 0.30f, 0.55f, 0.80f, 1.50f, 6.f, 1.f },
    { "Dissociative-Cold",     0.20f, -0.30f, -0.20f, 0.50f, 0.30f, 0.85f, 0.60f, 0.f,  0.f, 0.08f, 0, 1.f, 0.05f, 0.05f, 0.70f, 1.40f, 4.f, 1.f },
    { "Distracted",            0.00f,  0.00f,  0.40f, 0.35f, 0.15f, 0.75f, 0.40f, 0.f,  0.f, 0.50f, 1, 1.f, 0.40f, 0.70f, 0.35f, 0.60f, 3.f, 1.f },
    { "Zen / Serene",          0.15f,  0.35f, -0.50f, 0.60f, 0.35f, 0.90f, 0.80f, 0.f,  0.f, 0.20f, 1, 1.f, 0.15f, 0.15f, 1.00f, 1.60f, 4.f, 1.f },
    { "Villain Monologue",     0.80f,  0.00f, -0.15f, 0.85f, 0.55f, 1.00f, 0.50f, 0.f,  0.f, 0.20f, 1, 1.f, 0.15f, 0.10f, 0.60f, 1.20f, 3.f, 1.f },
    { "Conspirator",           0.35f,  0.30f,  0.15f, 0.40f, 0.15f, 0.85f, 0.45f, 2.f,  1.f, 0.35f, 1, 1.f, 0.30f, 0.55f, 0.25f, 0.70f, 3.f, 1.f }
};
const S32 GAZE_PRESET_COUNT =
    static_cast<S32>(sizeof(GAZE_PRESETS) / sizeof(GAZE_PRESETS[0]));

void applyPreset(const GazePerformancePreset& preset,
                 LLActorMover::GazeTarget& target)
{
    target.mPersonaDominance = preset.mDominance;
    target.mPersonaAffection = preset.mAffection;
    target.mPersonaAnxiety = preset.mAnxiety;
    target.mHeadEyeBlendOverride = preset.mHeadEye;
    if (preset.mTorso >= 0.f)
    {
        target.mTorsoAmountOverride = preset.mTorso;
    }
    if (preset.mIntensity >= 0.f)
    {
        target.mIntensityOverride = preset.mIntensity;
    }
    target.mSmoothingOverride = preset.mSmoothing;
    if (preset.mEyelineYaw > GAZE_PRESET_INHERIT ||
        preset.mEyelinePitch > GAZE_PRESET_INHERIT)
    {
        target.mEyelineOverride = true;
        target.mEyelineYawDegOverride = preset.mEyelineYaw;
        target.mEyelinePitchDegOverride = preset.mEyelinePitch;
    }
    target.mMicroLifeOverride = preset.mMicroLife;
    if (preset.mBlinks >= 0)
    {
        target.mBlinksOverride = preset.mBlinks;
    }
    if (preset.mVariation >= 0.f)
    {
        target.mVariationOverride = preset.mVariation;
    }
    target.mBreakFrequencyOverride = preset.mBreakFrequency;
    target.mEaseAcquireOverride = preset.mAcquire;
    target.mEaseReleaseOverride = preset.mRelease;
    if (preset.mDeadZoneDeg >= 0.f)
    {
        target.mDeadZoneDegOverride = preset.mDeadZoneDeg;
    }
    target.mBlinkRateScale = preset.mBlinkRate;
    target.mVergenceScale = preset.mVergence;
}

bool presetMatches(const GazePerformancePreset& preset,
                   const LLActorMover::GazeTarget& target)
{
    auto close = [](F32 a, F32 b) { return fabsf(a - b) <= 0.001f; };
    return close(target.mPersonaDominance, preset.mDominance) &&
           close(target.mPersonaAffection, preset.mAffection) &&
           close(target.mPersonaAnxiety, preset.mAnxiety) &&
           close(target.mHeadEyeBlendOverride, preset.mHeadEye) &&
           (preset.mTorso < 0.f ||
            close(target.mTorsoAmountOverride, preset.mTorso)) &&
           (preset.mIntensity < 0.f ||
            close(target.mIntensityOverride, preset.mIntensity)) &&
           close(target.mSmoothingOverride, preset.mSmoothing) &&
           ((preset.mEyelineYaw <= GAZE_PRESET_INHERIT &&
             preset.mEyelinePitch <= GAZE_PRESET_INHERIT) ||
            (target.mEyelineOverride &&
             close(target.mEyelineYawDegOverride, preset.mEyelineYaw) &&
             close(target.mEyelinePitchDegOverride, preset.mEyelinePitch))) &&
           close(target.mMicroLifeOverride, preset.mMicroLife) &&
           (preset.mBlinks < 0 || target.mBlinksOverride == preset.mBlinks) &&
           (preset.mVariation < 0.f ||
            close(target.mVariationOverride, preset.mVariation)) &&
           close(target.mBreakFrequencyOverride, preset.mBreakFrequency) &&
           close(target.mEaseAcquireOverride, preset.mAcquire) &&
           close(target.mEaseReleaseOverride, preset.mRelease) &&
           (preset.mDeadZoneDeg < 0.f ||
            close(target.mDeadZoneDegOverride, preset.mDeadZoneDeg)) &&
           close(target.mBlinkRateScale, preset.mBlinkRate) &&
           close(target.mVergenceScale, preset.mVergence);
}
} // anonymous namespace

ALPanelLensGaze::ALPanelLensGaze()
{
    // The settings-backed rows use the same compact XUI reset callback as the
    // Cinematic Camera parameter panels. Register it before our children build.
    alRegisterMachinimaResetControl();
}

bool ALPanelLensGaze::postBuild()
{
    mStatus = getChild<LLTextBox>("gaze_status");
    mMasterEnable = getChild<LLCheckBoxCtrl>("look_at_camera_enabled");
    mSlotYou = getChild<LLCheckBoxCtrl>("gaze_slot_you");
    mSlotA = getChild<LLCheckBoxCtrl>("gaze_slot_a");
    mSlotB = getChild<LLCheckBoxCtrl>("gaze_slot_b");
    mSlotC = getChild<LLCheckBoxCtrl>("gaze_slot_c");
    mSlotD = getChild<LLCheckBoxCtrl>("gaze_slot_d");

    mEnable = getChild<LLCheckBoxCtrl>("gaze_enable_check");
    mTarget = getChild<LLComboBox>("gaze_target_combo");
    mEyeTarget = getChild<LLComboBox>("gaze_eye_target_combo");
    mPriority = getChild<LLComboBox>("gaze_priority_combo");
    mTargetDetailScope = getChild<LLComboBox>("gaze_target_detail_scope");
    mCast = getChild<LLComboBox>("gaze_cast_combo");
    mSetPoint = getChild<LLButton>("btn_gaze_setpoint");
    mPickObject = getChild<LLButton>("btn_gaze_pick_object");
    mClearObject = getChild<LLButton>("btn_gaze_clear_object");
    mBlend = getChild<LLSliderCtrl>("gaze_blend_slider");
    mTorso = getChild<LLSliderCtrl>("gaze_torso_slider");
    mIntensity = getChild<LLSliderCtrl>("gaze_intensity_slider");
    mSmoothing = getChild<LLSliderCtrl>("gaze_smoothing_slider");
    mDeadZone = getChild<LLSpinCtrl>("gaze_deadzone_spinner");
    mBreakoff = getChild<LLCheckBoxCtrl>("gaze_breakoff_check");
    mBreakoffAngle = getChild<LLSpinCtrl>("gaze_breakoff_spinner");
    mEyelineYaw = getChild<LLSliderCtrl>("gaze_eyeline_yaw_slider");
    mEyelinePitch = getChild<LLSliderCtrl>("gaze_eyeline_pitch_slider");
    mPerformance = getChild<LLComboBox>("gaze_performance_combo");
    mPersonaDominance = getChild<LLSliderCtrl>("gaze_persona_dom_slider");
    mPersonaAffection = getChild<LLSliderCtrl>("gaze_persona_aff_slider");
    mPersonaAnxiety = getChild<LLSliderCtrl>("gaze_persona_anx_slider");
    mCameraRoll = getChild<LLSliderCtrl>("gaze_camera_roll_slider");
    mExaggerate = getChild<LLSliderCtrl>("gaze_exaggerate_slider");
    mGazeCues = getChild<LLButton>("btn_gaze_cues");

    mPerformance->removeall();
    mPerformance->add("Custom", LLSD(-1));
    for (S32 i = 0; i < GAZE_PRESET_COUNT; ++i)
    {
        mPerformance->add(GAZE_PRESETS[i].mName, LLSD(i));
    }
    mPerformance->setValue(LLSD(-1));

    mMasterEnable->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onMasterEnableCommit(); });
    mSlotYou->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSlotCommit(0); });
    mSlotA->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSlotCommit(1); });
    mSlotB->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSlotCommit(2); });
    mSlotC->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSlotCommit(3); });
    mSlotD->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSlotCommit(4); });

    mEnable->setCommitCallback([this](LLUICtrl*, const LLSD&) { onEnableCommit(); });
    mTarget->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTargetCommit(); });
    mEyeTarget->setCommitCallback([this](LLUICtrl*, const LLSD&) { onEyeTargetCommit(); });
    mPriority->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPriorityCommit(); });
    mTargetDetailScope->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onTargetDetailScopeCommit(); });
    mCast->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCastCommit(); });
    mSetPoint->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSetPoint(); });
    mPickObject->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPickObject(); });
    mClearObject->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClearObject(); });
    mBlend->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBlendCommit(); });
    mTorso->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTorsoCommit(); });
    mIntensity->setCommitCallback([this](LLUICtrl*, const LLSD&) { onIntensityCommit(); });
    mSmoothing->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSmoothingCommit(); });
    mBreakoff->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBreakoffCommit(); });
    mEyelineYaw->setCommitCallback([this](LLUICtrl*, const LLSD&) { onEyelineCommit(); });
    mEyelinePitch->setCommitCallback([this](LLUICtrl*, const LLSD&) { onEyelineCommit(); });
    mPerformance->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPerformanceCommit(); });
    mPersonaDominance->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPersonaCommit(); });
    mPersonaAffection->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPersonaCommit(); });
    mPersonaAnxiety->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPersonaCommit(); });
    mGazeCues->setCommitCallback([this](LLUICtrl*, const LLSD&) { onGazeCues(); });

    // Panel-managed values have no gSavedSettings control to reset. Route each
    // micro button through the same commit path as an operator edit so active
    // You/A-D selection and multi-actor forwarding remain unchanged.
    getChild<LLButton>("reset_gaze_target")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            mTarget->setValue(static_cast<S32>(LLActorMover::GazeTarget::CAMERA));
            onTargetCommit();
        });
    getChild<LLButton>("reset_gaze_eye_target")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            mEyeTarget->setValue(-1);
            onEyeTargetCommit();
        });
    getChild<LLButton>("reset_gaze_performance")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onResetPerformance(); });
    getChild<LLButton>("reset_gaze_persona_dom")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            mPersonaDominance->setValue(0.f);
            onPersonaCommit();
        });
    getChild<LLButton>("reset_gaze_persona_aff")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            mPersonaAffection->setValue(0.f);
            onPersonaCommit();
        });
    getChild<LLButton>("reset_gaze_persona_anx")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            mPersonaAnxiety->setValue(0.f);
            onPersonaCommit();
        });
    getChild<LLButton>("reset_gaze_blend")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            mBlend->setValue(0.7f);
            onBlendCommit();
        });
    getChild<LLButton>("reset_gaze_torso")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            mTorso->setValue(0.25f);
            onTorsoCommit();
        });
    getChild<LLButton>("reset_gaze_intensity")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            mIntensity->setValue(1.f);
            onIntensityCommit();
        });
    getChild<LLButton>("reset_gaze_smoothing")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            mSmoothing->setValue(0.5f);
            onSmoothingCommit();
        });
    getChild<LLButton>("reset_gaze_eyeline_yaw")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            mEyelineYaw->setValue(0.f);
            onEyelineCommit();
        });
    getChild<LLButton>("reset_gaze_eyeline_pitch")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            mEyelinePitch->setValue(0.f);
            onEyelineCommit();
        });
    return true;
}

LLUUID ALPanelLensGaze::slotActor(S32 slot_index) const
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    switch (slot_index)
    {
        case 0:
            return isAgentAvatarValid() ? gAgentAvatarp->getID() : LLUUID::null;
        case 1:
            return cast.getSubjectA();
        case 2:
            return cast.getSubjectB();
        case 3:
            return cast.getSubjectC();
        case 4:
            return cast.getSubjectD();
        default:
            return LLUUID::null;
    }
}

LLUUID ALPanelLensGaze::activeSlotActor() const
{
    return slotActor(mActiveSlot);
}

LLUUID ALPanelLensGaze::displayActor() const
{
    return mSelected.empty() ? LLUUID::null : mSelected.front();
}

uuid_vec_t ALPanelLensGaze::commitActors() const
{
    return mSelected.empty() ? uuid_vec_t(1, LLUUID::null) : mSelected;
}

void ALPanelLensGaze::draw()
{
    refreshCastCombo();
    refreshControls();
    LLPanel::draw();
}

void ALPanelLensGaze::refreshCastCombo()
{
    std::string signature = llformat("slot:%d:", mActiveSlot);
    signature += activeSlotActor().asString();
    signature += mEditingEyeTarget ? ":eyes:" : ":head:";
    for (const LLUUID& selected : mSelected)
    {
        signature += selected.asString();
    }
    for (const LLUUID& id : LLDirectorCast::instance().getIds())
    {
        signature += id.asString();
    }
    if (signature != mCastSignature && !isEditing(mCast))
    {
        mCastSignature = signature;
        mCast->removeall();
        const LLUUID displayed = activeSlotActor();
        for (const LLUUID& id : LLDirectorCast::instance().getIds())
        {
            const bool is_target_actor = displayed.notNull()
                ? id == displayed
                : isAgentAvatarValid() && id == gAgentAvatarp->getID();
            if (!is_target_actor)
            {
                mCast->add(actorName(id), LLSD(id.asString()));
            }
        }
        if (mCast->getItemCount() == 0)
        {
            mCast->add("(add another cast member)", LLSD(std::string()));
        }
    }

    if (!isEditing(mCast))
    {
        LLDirectorCast& cast = LLDirectorCast::instance();
        const LLUUID slot_actor = activeSlotActor();
        const LLUUID current = mEditingEyeTarget &&
            cast.hasEyeGazeTarget(slot_actor)
            ? cast.getEyeGazeTarget(slot_actor).mCastRef
            : cast.getGazeTarget(slot_actor).mCastRef;
        if (current.isNull() ||
            !mCast->setSelectedByValue(LLSD(current.asString()), true))
        {
            mCast->selectFirstItem();
        }
    }
}

void ALPanelLensGaze::refreshControls()
{
    LLActorMover& mover = LLActorMover::instance();
    LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID actor = displayActor();
    const bool have_actor = actor.notNull() || isAgentAvatarValid();
    const bool enabled = have_actor && mover.isGazeEnabled(actor);
    const LLUUID slot_actor = activeSlotActor();
    const bool have_slot = slot_actor.notNull();
    const bool slot_enabled = have_slot && cast.isLookAtCamera(slot_actor);
    const LLActorMover::GazeTarget slot_target = have_slot
        ? cast.getGazeTarget(slot_actor) : LLActorMover::GazeTarget();
    const S32 mode = have_slot
        ? static_cast<S32>(slot_target.mMode)
        : static_cast<S32>(LLActorMover::GazeTarget::CAMERA);
    const bool have_eye_target = have_slot && cast.hasEyeGazeTarget(slot_actor);
    const LLActorMover::GazeTarget eye_target = have_eye_target
        ? cast.getEyeGazeTarget(slot_actor) : LLActorMover::GazeTarget();
    const S32 eye_mode = have_eye_target
        ? static_cast<S32>(eye_target.mMode) : -1;

    // Look-at camera slot toggles
    const LLUUID you_id = isAgentAvatarValid() ? gAgentAvatarp->getID() : LLUUID::null;
    const LLUUID sub_a = cast.getSubjectA();
    const LLUUID sub_b = cast.getSubjectB();
    const LLUUID sub_c = cast.getSubjectC();
    const LLUUID sub_d = cast.getSubjectD();

    mSlotYou->setEnabled(isAgentAvatarValid());
    mSlotA->setEnabled(sub_a.notNull());
    mSlotB->setEnabled(sub_b.notNull());
    mSlotC->setEnabled(sub_c.notNull());
    mSlotD->setEnabled(sub_d.notNull());

    if (!isEditing(mSlotYou))
    {
        mSlotYou->set(cast.isLookAtCamera(you_id));
    }
    if (!isEditing(mSlotA))
    {
        mSlotA->set(sub_a.notNull() && cast.isLookAtCamera(sub_a));
    }
    if (!isEditing(mSlotB))
    {
        mSlotB->set(sub_b.notNull() && cast.isLookAtCamera(sub_b));
    }
    if (!isEditing(mSlotC))
    {
        mSlotC->set(sub_c.notNull() && cast.isLookAtCamera(sub_c));
    }
    if (!isEditing(mSlotD))
    {
        mSlotD->set(sub_d.notNull() && cast.isLookAtCamera(sub_d));
    }

    mEnable->setEnabled(have_actor);
    if (!isEditing(mEnable) && mEnable->getValue().asBoolean() != enabled)
    {
        mEnable->set(enabled);
    }
    mTarget->setEnabled(have_slot && slot_enabled);
    mEyeTarget->setEnabled(have_slot && slot_enabled);
    mTargetDetailScope->setEnabled(have_slot && slot_enabled);
    mPriority->setEnabled(true);
    mCameraRoll->setEnabled(
        !have_slot || (slot_enabled && mode == LLActorMover::GAZE_CAMERA));
    mExaggerate->setEnabled(true);
    mGazeCues->setEnabled(have_slot);
    if (!isEditing(mTarget) && mTarget->getValue().asInteger() != mode)
    {
        mTarget->setValue(mode);
    }
    if (!isEditing(mEyeTarget) &&
        mEyeTarget->getValue().asInteger() != eye_mode)
    {
        mEyeTarget->setValue(eye_mode);
    }
    const S32 priority = llclamp(
        gSavedSettings.getS32("DirectorGazePriority"),
        static_cast<S32>(LLActorMover::GAZE_PRIORITY_BLEND),
        static_cast<S32>(LLActorMover::GAZE_PRIORITY_UPPER_BODY));
    if (!isEditing(mPriority) && mPriority->getValue().asInteger() != priority)
    {
        mPriority->setValue(priority);
    }
    if (!isEditing(mTargetDetailScope) &&
        mTargetDetailScope->getValue().asInteger() != (mEditingEyeTarget ? 1 : 0))
    {
        mTargetDetailScope->setValue(mEditingEyeTarget ? 1 : 0);
    }

    const S32 detail_mode = mEditingEyeTarget ? eye_mode : mode;
    const bool cast_mode = (detail_mode == LLActorMover::GAZE_CAST);
    const bool point_mode = (detail_mode == LLActorMover::GAZE_POINT);
    const bool obj_mode = (detail_mode == LLActorMover::GAZE_OBJECT);
    mCast->setVisible(cast_mode);
    mCast->setEnabled(have_slot && slot_enabled && cast_mode && mCast->getItemCount() > 0);
    mSetPoint->setVisible(point_mode);
    mSetPoint->setEnabled(have_slot && slot_enabled && point_mode);
    mPickObject->setVisible(obj_mode);
    mPickObject->setEnabled(have_slot && slot_enabled && obj_mode);
    mClearObject->setVisible(obj_mode);
    mClearObject->setEnabled(have_slot && slot_enabled && obj_mode);

    LLUICtrl* authored[] = {
        mBlend, mTorso, mIntensity, mSmoothing, mEyelineYaw, mEyelinePitch
    };
    for (LLUICtrl* control : authored)
    {
        control->setEnabled(have_actor && enabled);
    }

    auto sync_slider = [](LLSliderCtrl* control, F32 value)
    {
        if (!isEditing(control) &&
            fabsf((F32)control->getValue().asReal() - value) > 0.001f)
        {
            control->setValue(value);
        }
    };
    sync_slider(mBlend, have_actor ? mover.getGazeHeadEyeBlend(actor) : 0.7f);
    sync_slider(mTorso, have_actor ? mover.getGazeTorsoAmount(actor) : 0.25f);
    sync_slider(mIntensity, have_actor ? mover.getGazeIntensity(actor) : 1.f);
    sync_slider(mSmoothing, have_actor ? mover.getGazeSmoothing(actor) : 0.5f);
    sync_slider(mEyelineYaw, have_actor ? mover.getGazeEyelineYaw(actor) : 0.f);
    sync_slider(mEyelinePitch, have_actor ? mover.getGazeEyelinePitch(actor) : 0.f);

    mPerformance->setEnabled(have_slot);
    mPersonaDominance->setEnabled(have_slot);
    mPersonaAffection->setEnabled(have_slot);
    mPersonaAnxiety->setEnabled(have_slot);
    sync_slider(mPersonaDominance,
                have_slot ? slot_target.mPersonaDominance : 0.f);
    sync_slider(mPersonaAffection,
                have_slot ? slot_target.mPersonaAffection : 0.f);
    sync_slider(mPersonaAnxiety,
                have_slot ? slot_target.mPersonaAnxiety : 0.f);
    if (!isEditing(mPerformance))
    {
        S32 preset_index = -1;
        for (S32 i = 0; have_slot && i < GAZE_PRESET_COUNT; ++i)
        {
            if (presetMatches(GAZE_PRESETS[i], slot_target))
            {
                preset_index = i;
                break;
            }
        }
        if (mPerformance->getValue().asInteger() != preset_index)
        {
            mPerformance->setValue(LLSD(preset_index));
        }
    }

    const bool breakoff = gSavedSettings.getS32("BDMergeGazeBehindPolicy") == 1;
    if (!isEditing(mBreakoff) && mBreakoff->getValue().asBoolean() != breakoff)
    {
        mBreakoff->set(breakoff);
    }
    mBreakoffAngle->setEnabled(breakoff);

    std::string status;
    mover.getGazeStatus(actor, status);
    if (mStatus->getValue().asString() != status)
    {
        mStatus->setText(status);
    }
}

void ALPanelLensGaze::onMasterEnableCommit()
{
    // Keep the Actor Gaze master on the exact gate applyDirectorLookAt()
    // consumes. The XML control_name binding mirrors this value as well.
    gSavedSettings.setBOOL(
        "DirectorLookAtCameraEnabled", mMasterEnable->get());
}

void ALPanelLensGaze::onSlotCommit(S32 slot_index)
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    mActiveSlot = llclamp(slot_index, 0, 4);
    const LLUUID target_id = slotActor(mActiveSlot);
    bool is_checked = false;
    switch (slot_index)
    {
        case 0:
            is_checked = mSlotYou->get();
            break;
        case 1:
            is_checked = mSlotA->get();
            break;
        case 2:
            is_checked = mSlotB->get();
            break;
        case 3:
            is_checked = mSlotC->get();
            break;
        case 4:
            is_checked = mSlotD->get();
            break;
        default:
            return;
    }
    if (target_id.notNull())
    {
        cast.setLookAtCamera(target_id, is_checked);
    }
    mCastSignature.clear();
}

void ALPanelLensGaze::onEnableCommit()
{
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazeEnabled(actor, mEnable->get());
    }
}

void ALPanelLensGaze::onTargetCommit()
{
    const LLUUID actor = activeSlotActor();
    if (actor.notNull())
    {
        LLDirectorCast& cast = LLDirectorCast::instance();
        LLActorMover::GazeTarget target = cast.getGazeTarget(actor);
        target.mMode = static_cast<LLActorMover::GazeTarget::EMode>(
            llclamp(mTarget->getValue().asInteger(),
                    static_cast<S32>(LLActorMover::GazeTarget::MOTION),
                    static_cast<S32>(LLActorMover::GazeTarget::OBJECT)));
        cast.setGazeTarget(actor, target);
        if (targetNeedsDetail(static_cast<S32>(target.mMode)))
        {
            mEditingEyeTarget = false;
            mTargetDetailScope->setValue(0);
            mCastSignature.clear();
        }
    }
}

void ALPanelLensGaze::onEyeTargetCommit()
{
    const LLUUID actor = activeSlotActor();
    if (actor.isNull())
    {
        return;
    }

    LLDirectorCast& cast = LLDirectorCast::instance();
    const S32 mode = mEyeTarget->getValue().asInteger();
    if (mode < static_cast<S32>(LLActorMover::GazeTarget::CAMERA))
    {
        cast.clearEyeGazeTarget(actor);
    }
    else
    {
        LLActorMover::GazeTarget target = cast.hasEyeGazeTarget(actor)
            ? cast.getEyeGazeTarget(actor) : LLActorMover::GazeTarget();
        target.mMode = static_cast<LLActorMover::GazeTarget::EMode>(
            llclamp(mode,
                    static_cast<S32>(LLActorMover::GazeTarget::CAMERA),
                    static_cast<S32>(LLActorMover::GazeTarget::OBJECT)));
        cast.setEyeGazeTarget(actor, target);
        if (targetNeedsDetail(static_cast<S32>(target.mMode)))
        {
            mEditingEyeTarget = true;
            mTargetDetailScope->setValue(1);
        }
    }
    mCastSignature.clear();
}

void ALPanelLensGaze::onPriorityCommit()
{
    gSavedSettings.setS32(
        "DirectorGazePriority",
        llclamp(mPriority->getValue().asInteger(),
                static_cast<S32>(LLActorMover::GAZE_PRIORITY_BLEND),
                static_cast<S32>(LLActorMover::GAZE_PRIORITY_UPPER_BODY)));
}

void ALPanelLensGaze::onTargetDetailScopeCommit()
{
    mEditingEyeTarget = mTargetDetailScope->getValue().asInteger() == 1;
    mCastSignature.clear();
}

void ALPanelLensGaze::onCastCommit()
{
    const std::string value = mCast->getSelectedValue().asString();
    const LLUUID cast_target = value.empty() ? LLUUID::null : LLUUID(value);
    const LLUUID actor = activeSlotActor();
    if (actor.notNull())
    {
        LLDirectorCast& cast = LLDirectorCast::instance();
        const bool edit_eye = mEditingEyeTarget && cast.hasEyeGazeTarget(actor);
        LLActorMover::GazeTarget target = edit_eye
            ? cast.getEyeGazeTarget(actor) : cast.getGazeTarget(actor);
        target.mCastRef = cast_target;
        if (edit_eye)
        {
            cast.setEyeGazeTarget(actor, target);
        }
        else
        {
            cast.setGazeTarget(actor, target);
        }
    }
}

void ALPanelLensGaze::onSetPoint()
{
    const LLVector3d point = gAgent.getPosGlobalFromAgent(
        LLViewerCamera::getInstance()->getOrigin());
    const LLUUID actor = activeSlotActor();
    if (actor.notNull())
    {
        LLDirectorCast& cast = LLDirectorCast::instance();
        const bool edit_eye = mEditingEyeTarget && cast.hasEyeGazeTarget(actor);
        LLActorMover::GazeTarget target = edit_eye
            ? cast.getEyeGazeTarget(actor) : cast.getGazeTarget(actor);
        target.mFixedPoint = point;
        if (edit_eye)
        {
            cast.setEyeGazeTarget(actor, target);
        }
        else
        {
            cast.setGazeTarget(actor, target);
        }
    }
}

void ALPanelLensGaze::onPickObject()
{
    LLViewerObject* obj = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    if (!obj)
    {
        obj = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject();
    }
    if (obj)
    {
        const LLUUID object_target = obj->getRootEdit() ? obj->getRootEdit()->getID() : obj->getID();
        const LLUUID actor = activeSlotActor();
        if (actor.notNull())
        {
            LLDirectorCast& cast = LLDirectorCast::instance();
            const bool edit_eye = mEditingEyeTarget && cast.hasEyeGazeTarget(actor);
            LLActorMover::GazeTarget target = edit_eye
                ? cast.getEyeGazeTarget(actor) : cast.getGazeTarget(actor);
            target.mObjectRef = object_target;
            if (edit_eye)
            {
                cast.setEyeGazeTarget(actor, target);
            }
            else
            {
                cast.setGazeTarget(actor, target);
            }
        }
    }
}

void ALPanelLensGaze::onClearObject()
{
    const LLUUID actor = activeSlotActor();
    if (actor.notNull())
    {
        LLDirectorCast& cast = LLDirectorCast::instance();
        const bool edit_eye = mEditingEyeTarget && cast.hasEyeGazeTarget(actor);
        LLActorMover::GazeTarget target = edit_eye
            ? cast.getEyeGazeTarget(actor) : cast.getGazeTarget(actor);
        target.mObjectRef.setNull();
        if (edit_eye)
        {
            cast.setEyeGazeTarget(actor, target);
        }
        else
        {
            cast.setGazeTarget(actor, target);
        }
    }
}

void ALPanelLensGaze::onBlendCommit()
{
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazeHeadEyeBlend(
            actor, (F32)mBlend->getValue().asReal());
    }
}

void ALPanelLensGaze::onTorsoCommit()
{
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazeTorsoAmount(
            actor, (F32)mTorso->getValue().asReal());
    }
}

void ALPanelLensGaze::onIntensityCommit()
{
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazeIntensity(
            actor, (F32)mIntensity->getValue().asReal());
    }
}

void ALPanelLensGaze::onSmoothingCommit()
{
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazeSmoothing(
            actor, (F32)mSmoothing->getValue().asReal());
    }
}

void ALPanelLensGaze::onBreakoffCommit()
{
    gSavedSettings.setS32("BDMergeGazeBehindPolicy", mBreakoff->get() ? 1 : 0);
}

void ALPanelLensGaze::onEyelineCommit()
{
    const F32 yaw = (F32)mEyelineYaw->getValue().asReal();
    const F32 pitch = (F32)mEyelinePitch->getValue().asReal();
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazeEyelineOffset(actor, yaw, pitch);
    }
}

void ALPanelLensGaze::onPersonaCommit()
{
    const LLUUID actor = activeSlotActor();
    if (actor.isNull())
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    LLActorMover::GazeTarget target = cast.getGazeTarget(actor);
    target.mPersonaDominance = llclamp(
        (F32)mPersonaDominance->getValue().asReal(), -1.f, 1.f);
    target.mPersonaAffection = llclamp(
        (F32)mPersonaAffection->getValue().asReal(), -1.f, 1.f);
    target.mPersonaAnxiety = llclamp(
        (F32)mPersonaAnxiety->getValue().asReal(), -1.f, 1.f);
    cast.setGazeTarget(actor, target);
}

void ALPanelLensGaze::onPerformanceCommit()
{
    const S32 preset_index = mPerformance->getValue().asInteger();
    const LLUUID actor = activeSlotActor();
    if (actor.isNull() || preset_index < 0 || preset_index >= GAZE_PRESET_COUNT)
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    LLActorMover::GazeTarget target = cast.getGazeTarget(actor);
    applyPreset(GAZE_PRESETS[preset_index], target);
    cast.setGazeTarget(actor, target);
}

void ALPanelLensGaze::onResetPerformance()
{
    const LLUUID actor = activeSlotActor();
    if (actor.isNull())
    {
        return;
    }

    // Preserve targeting and clear only the fields authored by a performance
    // preset. These are the documented neutral/inherit defaults on GazeTarget.
    LLDirectorCast& cast = LLDirectorCast::instance();
    LLActorMover::GazeTarget target = cast.getGazeTarget(actor);
    const LLActorMover::GazeTarget defaults;
    target.mPersonaDominance = defaults.mPersonaDominance;
    target.mPersonaAffection = defaults.mPersonaAffection;
    target.mPersonaAnxiety = defaults.mPersonaAnxiety;
    target.mHeadEyeBlendOverride = defaults.mHeadEyeBlendOverride;
    target.mTorsoAmountOverride = defaults.mTorsoAmountOverride;
    target.mIntensityOverride = defaults.mIntensityOverride;
    target.mSmoothingOverride = defaults.mSmoothingOverride;
    target.mEyelineOverride = defaults.mEyelineOverride;
    target.mEyelineYawDegOverride = defaults.mEyelineYawDegOverride;
    target.mEyelinePitchDegOverride = defaults.mEyelinePitchDegOverride;
    target.mMicroLifeOverride = defaults.mMicroLifeOverride;
    target.mBlinksOverride = defaults.mBlinksOverride;
    target.mVariationOverride = defaults.mVariationOverride;
    target.mBreakFrequencyOverride = defaults.mBreakFrequencyOverride;
    target.mEaseAcquireOverride = defaults.mEaseAcquireOverride;
    target.mEaseReleaseOverride = defaults.mEaseReleaseOverride;
    target.mDeadZoneDegOverride = defaults.mDeadZoneDegOverride;
    target.mBlinkRateScale = defaults.mBlinkRateScale;
    target.mVergenceScale = defaults.mVergenceScale;
    cast.setGazeTarget(actor, target);
    mPerformance->setValue(LLSD(-1));
}

void ALPanelLensGaze::onGazeCues()
{
    const LLUUID actor = activeSlotActor();
    if (actor.notNull())
    {
        LLFloaterReg::showInstance("gaze_cues", LLSD(actor.asString()));
    }
}
