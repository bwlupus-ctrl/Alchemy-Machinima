/**
 * @file alpanelcinecamparams.cpp
 * @brief Cinematic Camera parameter panel -- see alpanelcinecamparams.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelcinecamparams.h"
#include "alscrollfocus.h"

#include "llcinematiccamera.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "llfile.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "lluri.h"
#include "llviewercontrol.h"        // gSavedSettings

// both the standalone Cinematic Camera floater and the Director Console
// instantiate this class via <panel class="panel_cinecam_params" .../>
static LLPanelInjector<ALPanelCineCamParams> t_panel_cinecam_params("panel_cinecam_params");

namespace
{
constexpr char PRESET_SUBDIR[] = "cinematic_presets";

struct ShakePreset
{
    const char* mName;
    F32 mSurge;
    F32 mSway;
    F32 mHeave;
    F32 mRoll;
    F32 mPitch;
    F32 mYaw;
    F32 mFov;
    F32 mSmoothing;
    S32 mLocomotion;
    S32 mStyle;
    S32 mProfile;
};

// Curated output-authority looks. Style supplies the rig's procedural detail;
// locomotion supplies physical movement (including Drive suspension/road), and
// these final gains decide which parts reach the frame. Index zero is the
// combo's non-action placeholder.
const ShakePreset SHAKE_PRESETS[] = {
    { "",                 0.f,  0.f,  0.f,  0.f,  0.f,  0.f,  0.f, 0.35f,   0, 0, 0 },
    { "Locked Tripod",    0.f,  0.f,  0.f,  0.f,  0.f,  0.f,  0.f, 0.65f,   0, 1, 1 },
    { "Subtle Handheld", .35f, .45f, .35f, .30f, .40f, .35f, .20f, 0.45f,   0, 2, 4 },
    { "Documentary",     .75f, .90f, .80f, .75f, .90f, .90f, .65f, 0.30f, 100, 3, 2 },
    { "Shoulder Rig",    .65f, .90f, .75f, .80f, .70f, .65f, .35f, 0.38f,   2, 5, 4 },
    { "Run-and-Gun",    1.15f,1.30f,1.40f,1.25f,1.35f,1.25f, .75f, 0.16f, 100, 4, 6 },
    { "Vehicle / Drive",1.30f, .65f,1.25f, .80f, .55f, .45f, .20f, 0.28f,   4, 5, 2 },
    { "Drone Float",     .35f, .35f, .45f, .12f, .25f, .30f, .08f, 0.75f,   5, 6, 5 },
    { "Verite",          .90f,1.15f,1.00f,1.00f,1.10f,1.15f, .50f, 0.20f,   6, 7, 3 },
    { "Heartbeat",       .10f, .15f, .45f, .10f, .25f, .15f,1.40f, 0.50f,   0, 8, 4 },
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Global callbacks behind the per-control reset buttons in the shared camera
// panels and the Frame tab. Reset restores just the
// named setting to its built-in default, so XUI can wire a reset button with
//   commit_callback.function="Machinima.ResetControl"
//   commit_callback.parameter="<control_name>"
// A single registration covers both hosts (Director Console + standalone
// floaters) because both embed the same panels. Registered once, before any of
// these panels' reset buttons are built; a param naming no setting is a no-op.
// ---------------------------------------------------------------------------
void alRegisterMachinimaResetControl()
{
    static bool sRegistered = false;
    if (sRegistered)
    {
        return;
    }
    sRegistered = true;
    LLUICtrl::CommitCallbackRegistry::defaultRegistrar().add(
        "Machinima.ResetControl",
        [](LLUICtrl*, const LLSD& param)
        {
            const std::string name = param.asString();
            if (name.empty())
            {
                return;
            }
            if (LLControlVariable* ctrl = gSavedSettings.getControl(name))
            {
                ctrl->resetToDefault(true);
            }
        });
    LLUICtrl::CommitCallbackRegistry::defaultRegistrar().add(
        "Machinima.AutoFrameResolve",
        [](LLUICtrl*, const LLSD&)
        {
            LLCinematicCamera::instance().requestAutoReframe();
        });
}

ALPanelCineCamParams::ALPanelCineCamParams()
{
    // ensure the reset buttons' commit callback resolves before our children build
    alRegisterMachinimaResetControl();
}

// ---------------------------------------------------------------------------
// mode -> panel + settings table
//
// Ground truth is the set of LLCachedControl declarations inside each pattern
// function of llcinematiccamera.cpp. No two modes share a setting, so every
// mode gets its own panel. Keep this table in sync with the camera code.
// ---------------------------------------------------------------------------
//static
const std::vector<ALPanelCineCamParams::ModeEntry>& ALPanelCineCamParams::modeTable()
{
    static const std::vector<ModeEntry> table = {
        {  1, "panel_mode_bone",     { "CinematicCamJoint",
                                       "CinematicCamBoneOffsetForward", "CinematicCamBoneOffsetLeft", "CinematicCamBoneOffsetUp",
                                       "CinematicCamBoneAimYaw", "CinematicCamBoneAimPitch", "CinematicCamBoneAimRoll",
                                       "CinematicCamBoneHorizonLock" } },
        {  2, "panel_mode_orbit",    { "CinematicCamOrbitRadius", "CinematicCamOrbitSpeed",
                                       "CinematicCamOrbitHeight", "CinematicCamOrbitBob" } },
        {  3, "panel_mode_hover",    { "CinematicCamHoverDistance", "CinematicCamHoverWander",
                                       "CinematicCamHoverSpeed", "CinematicCamHoverHeight" } },
        {  4, "panel_mode_sweep",    { "CinematicCamSweepLength", "CinematicCamSweepDistance", "CinematicCamSweepSpeed",
                                       "CinematicCamSweepHeading", "CinematicCamSweepHeight", "CinematicCamSweepPingPong" } },
        {  5, "panel_mode_crane",    { "CinematicCamCraneRadius", "CinematicCamCraneSpeed",
                                       "CinematicCamCraneMinHeight", "CinematicCamCraneMaxHeight",
                                       "CinematicCamCraneRisePeriod" } },
        {  6, "panel_mode_vertigo",  { "CinematicCamVertigoStartDist", "CinematicCamVertigoEndDist",
                                       "CinematicCamVertigoDuration", "CinematicCamVertigoHeading",
                                       "CinematicCamVertigoHeight", "CinematicCamVertigoEndMode" } },
        {  7, "panel_mode_push",     { "CinematicCamPushStartDist", "CinematicCamPushEndDist",
                                       "CinematicCamPushDuration", "CinematicCamPushHeading",
                                       "CinematicCamPushHeight", "CinematicCamPushEndMode" } },
        {  8, "panel_mode_hero",     { "CinematicCamHeroDistance", "CinematicCamHeroHeight", "CinematicCamHeroArc",
                                       "CinematicCamHeroPeriod", "CinematicCamHeroHeading" } },
        {  9, "panel_mode_overhead", { "CinematicCamOverheadStart", "CinematicCamOverheadEnd",
                                       "CinematicCamOverheadDuration", "CinematicCamOverheadSpin",
                                       "CinematicCamOverheadEndMode" } },
        { 10, "panel_mode_ots",      { "CinematicCamOTSSide", "CinematicCamOTSBack",
                                       "CinematicCamOTSOut", "CinematicCamOTSUp" } },
        { 11, "panel_mode_crash",    { "CinematicCamCrashZoom", "CinematicCamCrashDuration" } },
        { 12, "panel_mode_slowzoom", { "CinematicCamSlowZoomTarget", "CinematicCamSlowZoomDuration" } },
        { 13, "panel_mode_whip",     { "CinematicCamWhipFrom", "CinematicCamWhipTo", "CinematicCamWhipDuration",
                                       "CinematicCamWhipDistance", "CinematicCamWhipHeight" } },
        { 14, "panel_mode_arc",      { "CinematicCamArcFrom", "CinematicCamArcTo", "CinematicCamArcDuration",
                                       "CinematicCamArcDistance", "CinematicCamArcHeight", "CinematicCamArcEndMode" } },
        { 15, "panel_mode_reveal",   { "CinematicCamRevealBehind", "CinematicCamRevealLowHeight",
                                       "CinematicCamRevealHighHeight", "CinematicCamRevealAhead",
                                       "CinematicCamRevealDuration" } },
        { 16, "panel_mode_pull",     { "CinematicCamPullStartDist", "CinematicCamPullEndDist",
                                       "CinematicCamPullEndHeight", "CinematicCamPullDuration",
                                       "CinematicCamPullHeading" } },
        { 17, "panel_mode_twoshot",  { "CinematicCamTwoShotSide", "CinematicCamTwoShotPad",
                                       "CinematicCamTwoShotMinDist", "CinematicCamTwoShotHeight" } },
        { 18, "panel_mode_lead",     { "CinematicCamLeadDistance", "CinematicCamLeadHeight",
                                       "CinematicCamLeadSway" } },
        { 19, "panel_mode_ecu",      { "CinematicCamECUDistance", "CinematicCamECUZoom",
                                       "CinematicCamECUDrift" } },
        { 20, "panel_mode_long",     { "CinematicCamLongDistance", "CinematicCamLongZoom", "CinematicCamLongHeading",
                                       "CinematicCamLongHeight", "CinematicCamLongDrift" } },
        { 21, "panel_mode_spiral",   { "CinematicCamSpiralStartRadius", "CinematicCamSpiralEndRadius",
                                       "CinematicCamSpiralStartHeight", "CinematicCamSpiralEndHeight",
                                       "CinematicCamSpiralSpeed", "CinematicCamSpiralDuration" } },
        { 22, "panel_mode_pedestal", { "CinematicCamPedestalDistance", "CinematicCamPedestalStart",
                                       "CinematicCamPedestalEnd", "CinematicCamPedestalDuration",
                                       "CinematicCamPedestalHeading" } },
        { 23, "panel_mode_barrel",   { "CinematicCamBarrelDistance", "CinematicCamBarrelHeight",
                                       "CinematicCamBarrelRollSpeed", "CinematicCamBarrelRollAmplitude",
                                       "CinematicCamBarrelRollPeriod", "CinematicCamBarrelOscillate" } },
        { 24, "panel_mode_cork",     { "CinematicCamCorkStartRadius", "CinematicCamCorkEndRadius",
                                       "CinematicCamCorkStartHeight", "CinematicCamCorkEndHeight",
                                       "CinematicCamCorkTurns", "CinematicCamCorkDuration",
                                       "CinematicCamCorkRollPerTurn", "CinematicCamCorkEndMode" } },
        { 25, "panel_mode_pendulum", { "CinematicCamPendulumRadius", "CinematicCamPendulumHeight",
                                       "CinematicCamPendulumSwing", "CinematicCamPendulumPeriod",
                                       "CinematicCamPendulumHeading" } },
        { 26, "panel_mode_contra",   { "CinematicCamContraRadius", "CinematicCamContraHeight",
                                       "CinematicCamContraSpeed", "CinematicCamContraFovStart",
                                       "CinematicCamContraFovEnd", "CinematicCamContraWarpPeriod" } },
        { 27, "panel_mode_fisheye",  { "CinematicCamFisheyeNear", "CinematicCamFisheyeFar",
                                       "CinematicCamFisheyeFov", "CinematicCamFisheyeDuration",
                                       "CinematicCamFisheyeHeight", "CinematicCamFisheyeEndMode" } },
        { 28, "panel_mode_skimmer",  { "CinematicCamSkimmerHeight", "CinematicCamSkimmerDistance",
                                       "CinematicCamSkimmerLength", "CinematicCamSkimmerSpeed",
                                       "CinematicCamSkimmerHeading" } },
        { 29, "panel_mode_boost",    { "CinematicCamBoostStartHeight", "CinematicCamBoostEndHeight",
                                       "CinematicCamBoostDistance", "CinematicCamBoostDuration",
                                       "CinematicCamBoostHeading", "CinematicCamBoostEndMode" } },
        { 30, "panel_mode_boom",     { "CinematicCamBoomRadius", "CinematicCamBoomApex",
                                       "CinematicCamBoomSpan", "CinematicCamBoomDuration",
                                       "CinematicCamBoomAxis", "CinematicCamBoomEndMode" } },
        { 31, "panel_mode_topspin",  { "CinematicCamTopSpinHeight", "CinematicCamTopSpinSpeed",
                                       "CinematicCamTopSpinOffset" } },
        { 32, "panel_mode_turntable",{ "CinematicCamTurntableRadius", "CinematicCamTurntableSpeed",
                                       "CinematicCamTurntableMinHeight", "CinematicCamTurntableMaxHeight",
                                       "CinematicCamTurntablePeriod" } },
        { 33, "panel_mode_float",    { "CinematicCamFloatDistance", "CinematicCamFloatDrift",
                                       "CinematicCamFloatSpeed", "CinematicCamFloatFov" } },
        { 34, "panel_mode_tiltwhip", { "CinematicCamTiltWhipDistance", "CinematicCamTiltWhipHeight",
                                       "CinematicCamTiltWhipAmplitude", "CinematicCamTiltWhipPeriod",
                                       "CinematicCamTiltWhipSnap" } },
        { 35, "panel_mode_bodyhelix",{ "CinematicCamBodyHelixRadius", "CinematicCamBodyHelixRevolutions",
                                       "CinematicCamBodyHelixDuration", "CinematicCamBodyHelixStartOffset",
                                       "CinematicCamBodyHelixEndOffset" } },
        { 36, "panel_mode_descent",  { "CinematicCamDescentDistance", "CinematicCamDescentAboveHead",
                                       "CinematicCamDescentFootOffset", "CinematicCamDescentDuration",
                                       "CinematicCamDescentHeading" } },
        { 37, "panel_mode_parallax", { "CinematicCamParallaxLength", "CinematicCamParallaxDistance",
                                       "CinematicCamParallaxHeight", "CinematicCamParallaxDuration",
                                       "CinematicCamParallaxHeading" } },
        { 38, "panel_mode_figure8",  { "CinematicCamFigureEightRadius", "CinematicCamFigureEightHeight",
                                       "CinematicCamFigureEightPeriod", "CinematicCamFigureEightHeading" } },
        { 39, "panel_mode_detail",   { "CinematicCamDetailDistance", "CinematicCamDetailLength",
                                       "CinematicCamDetailBand", "CinematicCamDetailDuration",
                                       "CinematicCamDetailFov" } },
        { 41, "panel_mode_cable",    { "CinematicCamCableLength", "CinematicCamCableMissDistance",
                                       "CinematicCamCableHeight", "CinematicCamCableDuration",
                                       "CinematicCamCableHeading" } },
        { 42, "panel_mode_breathing",{ "CinematicCamBreathingDistance", "CinematicCamBreathingHeight",
                                       "CinematicCamBreathingAmplitude", "CinematicCamBreathingPeriod",
                                       "CinematicCamBreathingHeading" } },
        { 43, "panel_mode_static_wide",
              { "CinematicCamStaticWideHeading", "CinematicCamStaticWideDistance",
                "CinematicCamStaticWideCameraUp", "CinematicCamStaticWideAimUp",
                "CinematicCamStaticWideFov" } },
        { 44, "panel_mode_static_medium",
              { "CinematicCamStaticMediumHeading", "CinematicCamStaticMediumDistance",
                "CinematicCamStaticMediumCameraUp", "CinematicCamStaticMediumAimUp",
                "CinematicCamStaticMediumFov" } },
        { 45, "panel_mode_static_close",
              { "CinematicCamStaticCloseHeading", "CinematicCamStaticCloseDistance",
                "CinematicCamStaticCloseCameraUp", "CinematicCamStaticCloseAimUp",
                "CinematicCamStaticCloseFov" } },
        { 46, "panel_mode_static_profile_l",
              { "CinematicCamStaticProfileLHeading", "CinematicCamStaticProfileLDistance",
                "CinematicCamStaticProfileLCameraUp", "CinematicCamStaticProfileLAimUp",
                "CinematicCamStaticProfileLFov" } },
        { 47, "panel_mode_static_profile_r",
              { "CinematicCamStaticProfileRHeading", "CinematicCamStaticProfileRDistance",
                "CinematicCamStaticProfileRCameraUp", "CinematicCamStaticProfileRAimUp",
                "CinematicCamStaticProfileRFov" } },
        { 48, "panel_mode_static_low",
              { "CinematicCamStaticLowHeading", "CinematicCamStaticLowDistance",
                "CinematicCamStaticLowCameraUp", "CinematicCamStaticLowAimUp",
                "CinematicCamStaticLowFov" } },
        { 49, "panel_mode_static_high",
              { "CinematicCamStaticHighHeading", "CinematicCamStaticHighDistance",
                "CinematicCamStaticHighCameraUp", "CinematicCamStaticHighAimUp",
                "CinematicCamStaticHighFov" } },
        { 50, "panel_mode_static_full",
              { "CinematicCamStaticFullHeading", "CinematicCamStaticFullDistance",
                "CinematicCamStaticFullCameraUp", "CinematicCamStaticFullAimUp",
                "CinematicCamStaticFullFov" } },
    };
    return table;
}

//static
const std::vector<std::string>& ALPanelCineCamParams::sharedSettings()
{
    // read for every mode in isActive()/resolveTarget()/updateCamera()
    static const std::vector<std::string> shared = {
        "CinematicCamEnabled",
        "CinematicCamMode",
        "CinematicCamUseSelected",
        "CinematicCamLookAtHead",
        "CinematicCamUseOperator",
        "CinematicCamMotionStartMode",
        "CinematicCamMotionStartOffsetDeg",
        "CinematicCamMotionDirection",
        "CinematicCamMotionSeed",
        "CinematicAutoFrameEnabled",
        "CinematicAutoFrameFill",
        "CinematicAutoFrameComposeLine",
        "CinematicAutoFrameDistanceTrim",
        // Non-destructive delivery frame and differential lens. Additive keys
        // are included in named rigs without changing any legacy preset key.
        "CinematicFrameAspectRatio",
        "CinematicFrameCustomRatio",
        "CinematicFrameFocalLengthMM",
        "CinematicFrameGuideEnabled",
        "CinematicFrameGuideOpacity",
        "CinematicFrameGuideStyle",
        "CinematicFrameLensEnabled",
        // Complete handheld operator surface. Every FlycamOperator* setting is
        // both visible in the shared panel and registered here, so named
        // presets and Reset All round-trip the whole rig without hidden state.
        "FlycamOperatorAutoRunEnter",
        "FlycamOperatorAutoRunEnterDwell",
        "FlycamOperatorAutoRunExit",
        "FlycamOperatorAutoRunExitDwell",
        "FlycamOperatorAutoWalkEnter",
        "FlycamOperatorAutoWalkEnterDwell",
        "FlycamOperatorAutoWalkExit",
        "FlycamOperatorAutoWalkExitDwell",
        "FlycamOperatorBreathAmount",
        "FlycamOperatorBreathFreq",
        "FlycamOperatorBreathLift",
        "FlycamOperatorCadenceDrive",
        "FlycamOperatorDrag",
        "FlycamOperatorEnabled",
        "FlycamOperatorEnergyTrim",
        "FlycamOperatorForceWalk",
        "FlycamOperatorForwardWalkBias",
        "FlycamOperatorGainFOV",
        "FlycamOperatorGainHeave",
        "FlycamOperatorGainPitch",
        "FlycamOperatorGainRoll",
        "FlycamOperatorGainSurge",
        "FlycamOperatorGainSway",
        "FlycamOperatorGainYaw",
        "FlycamOperatorGaitCoupling",
        "FlycamOperatorIdleIntensity",
        "FlycamOperatorLateralStep",
        "FlycamOperatorLateralWalkBias",
        "FlycamOperatorLocomotionMode",
        "FlycamOperatorMaster",
        "FlycamOperatorModeBlendTime",
        "FlycamOperatorMotionBreath",
        "FlycamOperatorMotionCalm",
        "FlycamOperatorMotionPan",
        "FlycamOperatorMotionRoll",
        "FlycamOperatorMotionTilt",
        "FlycamOperatorOnset",
        "FlycamOperatorPanAmount",
        "FlycamOperatorPanMode",
        "FlycamOperatorPanReactSmoothing",
        "FlycamOperatorPanSmoothness",
        "FlycamOperatorPanTiltFreq",
        "FlycamOperatorProfile",
        "FlycamOperatorProfileInfluence",
        "FlycamOperatorReactivity",
        "FlycamOperatorRecomposeAmount",
        "FlycamOperatorRecomposeInterval",
        "FlycamOperatorRefAngularSpeed",
        "FlycamOperatorRefLinearSpeed",
        "FlycamOperatorRollAmount",
        "FlycamOperatorRollDamping",
        "FlycamOperatorRollFreq",
        "FlycamOperatorSeed",
        "FlycamOperatorSettle",
        "FlycamOperatorSettleDecay",
        "FlycamOperatorSettleFreq",
        "FlycamOperatorSimulationHz",
        "FlycamOperatorSmoothing",
        "FlycamOperatorStepBob",
        "FlycamOperatorStepRoll",
        "FlycamOperatorStyle",
        "FlycamOperatorTiltAmount",
        "FlycamOperatorTimeSpeed",
        "FlycamOperatorTremorDamping",
        "FlycamOperatorWalkCadence",
        "CinematicCamSmoothing",
        "CinematicCamDutchAngle",
        "CinematicCamFrameOffsetUp",
    };
    return shared;
}

// ---------------------------------------------------------------------------
ALPanelCineCamParams::~ALPanelCineCamParams()
{
    if (mModeConnection.connected())
    {
        mModeConnection.disconnect();
    }
}

bool ALPanelCineCamParams::postBuild()
{
    // Both the standalone floater and Director place this shared panel in a
    // fixed-height scroll document. Make keyboard Tab reveal focused controls.
    ALScrollFocus::installAncestor(this);

    getChild<LLButton>("btn_reset_mode")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickResetMode(); });
    getChild<LLButton>("btn_reset_all")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickResetAll(); });
    if (LLButton* preset_save = findChild<LLButton>("btn_preset_save"))
    {
        preset_save->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onClickSavePreset(); });
    }
    if (LLButton* preset_delete = findChild<LLButton>("btn_preset_delete"))
    {
        preset_delete->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onClickDeletePreset(); });
    }

    mPresetCombo = findChild<LLComboBox>("preset_combo");
    if (mPresetCombo)
    {
        mPresetCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPresetSelected(); });
    }

    // Only the Camera Shake page owns this optional curated-look combo; the
    // shared controller is also instantiated for the Cinematic page.
    mShakePresetCombo = findChild<LLComboBox>("shake_preset_combo");
    if (mShakePresetCombo)
    {
        mShakePresetCombo->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onShakePresetSelected(); });
    }

    // react to mode changes from the header combo AND from anywhere else
    // (debug settings, scripts, another instance of this panel): the
    // control's commit signal covers all of them
    if (LLControlVariable* mode_ctrl = gSavedSettings.getControl("CinematicCamMode"))
    {
        mModeConnection = mode_ctrl->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&) { updateModePanel(); });
    }

    updateModePanel();
    refreshPresetList();
    return true;
}

void ALPanelCineCamParams::onShakePresetSelected()
{
    if (!mShakePresetCombo)
    {
        return;
    }
    const S32 index = mShakePresetCombo->getValue().asInteger();
    if (index <= 0 || index >= (S32)(sizeof(SHAKE_PRESETS) / sizeof(SHAKE_PRESETS[0])))
    {
        return;
    }

    const ShakePreset& preset = SHAKE_PRESETS[index];
    gSavedSettings.setF32("FlycamOperatorGainSurge", preset.mSurge);
    gSavedSettings.setF32("FlycamOperatorGainSway", preset.mSway);
    gSavedSettings.setF32("FlycamOperatorGainHeave", preset.mHeave);
    gSavedSettings.setF32("FlycamOperatorGainRoll", preset.mRoll);
    gSavedSettings.setF32("FlycamOperatorGainPitch", preset.mPitch);
    gSavedSettings.setF32("FlycamOperatorGainYaw", preset.mYaw);
    gSavedSettings.setF32("FlycamOperatorGainFOV", preset.mFov);
    gSavedSettings.setF32("CinematicCamSmoothing", preset.mSmoothing);
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", preset.mLocomotion);
    gSavedSettings.setS32("FlycamOperatorStyle", preset.mStyle);
    gSavedSettings.setS32("FlycamOperatorProfile", preset.mProfile);
    LL_INFOS("CameraShake") << "Applied built-in shake preset '"
                             << preset.mName << "'" << LL_ENDL;
}

void ALPanelCineCamParams::onVisibilityChange(bool new_visibility)
{
    if (new_visibility)
    {
        updateModePanel();
        // keep the current selection while picking up presets saved from
        // another instance since we were last shown
        refreshPresetList(mPresetCombo ? mPresetCombo->getSelectedItemLabel()
                                       : std::string());
    }
    LLPanel::onVisibilityChange(new_visibility);
}

// ---------------------------------------------------------------------------
// Director Console scene-file hooks
// ---------------------------------------------------------------------------
std::string ALPanelCineCamParams::getSelectedPresetName() const
{
    // the selected LIST item only -- the "no preset" button label never
    // leaks out of here (getSelectedItemLabel is empty without a selection)
    return mPresetCombo ? mPresetCombo->getSelectedItemLabel() : std::string();
}

bool ALPanelCineCamParams::applyPresetByName(const std::string& name)
{
    if (name.empty() || !gDirUtilp->fileExists(presetPath(name)))
    {
        return false;
    }
    applyPreset(name);
    refreshPresetList(name);
    return true;
}

// ---------------------------------------------------------------------------
// per-mode panel visibility
// ---------------------------------------------------------------------------
void ALPanelCineCamParams::updateModePanel()
{
    const S32 mode = gSavedSettings.getS32("CinematicCamMode");
    for (const ModeEntry& entry : modeTable())
    {
        if (entry.mPanel)
        {
            if (LLPanel* panel = findChild<LLPanel>(entry.mPanel))
            {
                panel->setVisible(entry.mMode == mode);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// resets
// ---------------------------------------------------------------------------
void ALPanelCineCamParams::onClickResetMode()
{
    const S32 mode = gSavedSettings.getS32("CinematicCamMode");
    for (const ModeEntry& entry : modeTable())
    {
        if (entry.mMode != mode)
        {
            continue;
        }
        for (const std::string& name : entry.mSettings)
        {
            if (LLControlVariable* ctrl = gSavedSettings.getControl(name))
            {
                ctrl->resetToDefault(true);
            }
        }
        break;
    }
}

void ALPanelCineCamParams::onClickResetAll()
{
    LLHandle<ALPanelCineCamParams> handle = getDerivedHandle<ALPanelCineCamParams>();
    LLNotificationsUtil::add("CinematicCamConfirmResetAll", LLSD(), LLSD(),
        [handle](const LLSD& notification, const LLSD& response)
        {
            if (ALPanelCineCamParams* self = handle.get())
            {
                self->resetAllCallback(notification, response);
            }
        });
}

bool ALPanelCineCamParams::resetAllCallback(const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }
    for (const std::string& name : sharedSettings())
    {
        if (LLControlVariable* ctrl = gSavedSettings.getControl(name))
        {
            ctrl->resetToDefault(true);
        }
    }
    for (const ModeEntry& entry : modeTable())
    {
        for (const std::string& name : entry.mSettings)
        {
            if (LLControlVariable* ctrl = gSavedSettings.getControl(name))
            {
                ctrl->resetToDefault(true);
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// named global presets: one LLSD .xml per preset in the per-user settings
// dir, { "mode": current mode, "settings": { name -> value } } covering the
// shared settings and every mode's settings (a complete rig state)
// ---------------------------------------------------------------------------
//static
std::string ALPanelCineCamParams::presetsDir()
{
    std::string dir = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PRESET_SUBDIR);
    if (!gDirUtilp->fileExists(dir))
    {
        LLFile::mkdir(dir);
    }
    return dir;
}

//static
std::string ALPanelCineCamParams::presetPath(const std::string& name)
{
    // same reversible sanitization the graphics presets use
    return gDirUtilp->add(presetsDir(), LLURI::escape(name) + ".xml");
}

void ALPanelCineCamParams::refreshPresetList(const std::string& select_name)
{
    if (!mPresetCombo)
    {
        return;
    }
    mPresetCombo->clearRows();

    std::vector<std::string> names;
    {
        LLDirIterator dir_iter(presetsDir(), "*.xml");
        std::string file;
        while (dir_iter.next(file))
        {
            names.emplace_back(LLURI::unescape(gDirUtilp->getBaseFileName(file, /*strip_exten=*/true)));
        }
    }
    std::sort(names.begin(), names.end());
    for (const std::string& name : names)
    {
        mPresetCombo->add(name);
    }

    if (!select_name.empty() && mPresetCombo->setSelectedByValue(select_name, true))
    {
        return;
    }
    mPresetCombo->setLabel(getString("no_preset_label"));
}

void ALPanelCineCamParams::onClickSavePreset()
{
    LLSD args;
    // preselect the current name so "tweak and re-save" is one click
    if (mPresetCombo && !mPresetCombo->getSelectedItemLabel().empty())
    {
        args["DESC"] = mPresetCombo->getSelectedItemLabel();
    }
    else
    {
        args["DESC"] = LLStringUtil::null;
    }
    LLHandle<ALPanelCineCamParams> handle = getDerivedHandle<ALPanelCineCamParams>();
    LLNotificationsUtil::add("CinematicCamSavePreset", args, LLSD(),
        [handle](const LLSD& notification, const LLSD& response)
        {
            if (ALPanelCineCamParams* self = handle.get())
            {
                self->savePresetCallback(notification, response);
            }
        });
}

bool ALPanelCineCamParams::savePresetCallback(const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }
    std::string name = response["message"].asString();
    LLStringUtil::trim(name);
    if (name.empty())
    {
        return false;
    }
    writePreset(name);
    refreshPresetList(name);
    return false;
}

void ALPanelCineCamParams::writePreset(const std::string& name)
{
    LLSD settings = LLSD::emptyMap();
    auto capture = [&settings](const std::string& setting)
    {
        if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
        {
            settings[setting] = ctrl->getValue();
        }
    };
    for (const std::string& setting : sharedSettings())
    {
        capture(setting);
    }
    for (const ModeEntry& entry : modeTable())
    {
        for (const std::string& setting : entry.mSettings)
        {
            capture(setting);
        }
    }

    LLSD preset = LLSD::emptyMap();
    preset["mode"] = gSavedSettings.getS32("CinematicCamMode");
    preset["settings"] = settings;

    const std::string path = presetPath(name);
    llofstream out(path.c_str());
    if (!out.is_open())
    {
        LL_WARNS("CinematicCam") << "Cannot write preset file " << path << LL_ENDL;
        return;
    }
    LLSDSerialize::toPrettyXML(preset, out);
    out.close();
    LL_INFOS("CinematicCam") << "Saved cinematic camera preset '" << name << "'" << LL_ENDL;
}

void ALPanelCineCamParams::onPresetSelected()
{
    if (mPresetCombo)
    {
        const std::string name = mPresetCombo->getSelectedItemLabel();
        if (!name.empty())
        {
            applyPreset(name);
        }
    }
}

void ALPanelCineCamParams::applyPreset(const std::string& name)
{
    const std::string path = presetPath(name);
    llifstream in(path.c_str());
    if (!in.is_open())
    {
        LL_WARNS("CinematicCam") << "Cannot open preset file " << path << LL_ENDL;
        return;
    }
    LLSD preset;
    LLSDSerialize::fromXML(preset, in);
    in.close();
    if (!preset.isMap() || !preset["settings"].isMap())
    {
        LL_WARNS("CinematicCam") << "Malformed preset file " << path << LL_ENDL;
        return;
    }

    // only apply keys this panel owns: a preset file is data, not commands
    auto apply = [&preset](const std::string& setting)
    {
        if (preset["settings"].has(setting))
        {
            if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
            {
                ctrl->setValue(preset["settings"][setting]);
            }
        }
    };
    for (const ModeEntry& entry : modeTable())
    {
        for (const std::string& setting : entry.mSettings)
        {
            apply(setting);
        }
    }
    for (const std::string& setting : sharedSettings())
    {
        apply(setting);
    }
    // the explicit mode key wins over any CinematicCamMode inside "settings"
    if (preset["mode"].isInteger())
    {
        gSavedSettings.setS32("CinematicCamMode", preset["mode"].asInteger());
    }
    LL_INFOS("CinematicCam") << "Applied cinematic camera preset '" << name << "'" << LL_ENDL;
}

void ALPanelCineCamParams::onClickDeletePreset()
{
    if (!mPresetCombo)
    {
        return;
    }
    const std::string name = mPresetCombo->getSelectedItemLabel();
    if (name.empty())
    {
        return;
    }
    LLSD args;
    args["NAME"] = name;
    LLHandle<ALPanelCineCamParams> handle = getDerivedHandle<ALPanelCineCamParams>();
    LLNotificationsUtil::add("CinematicCamConfirmDeletePreset", args, LLSD(),
        [handle, name](const LLSD& notification, const LLSD& response)
        {
            if (ALPanelCineCamParams* self = handle.get())
            {
                self->deletePresetCallback(notification, response, name);
            }
        });
}

bool ALPanelCineCamParams::deletePresetCallback(const LLSD& notification, const LLSD& response,
                                                const std::string name)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }
    const std::string path = presetPath(name);
    if (LLFile::remove(path) != 0)
    {
        LL_WARNS("CinematicCam") << "Cannot delete preset file " << path << LL_ENDL;
    }
    refreshPresetList();
    return false;
}
