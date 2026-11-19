/**
 * @file altron.cpp
 * @brief [TronT0] Tron effects package: shared clock, world anchor, palette
 * and settings-list foundation.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "altron.h"

#include "aldirectorswitcher.h" // [TronT3] ALDirectorSwitcher::instance().cutSerial()
#include "alpanelcinelightrig.h"
#include "alrotoink.h"
#include "llagent.h"
#include "llgl.h"
#include "llpresentationtime.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"     // [TronT3] LLViewerObject::asAvatar()
#include "llviewerobjectlist.h" // [TronT3] gObjectList.findObject()
#include "llviewershadermgr.h" // [TronT1] gTronWorldProgram (resolveFrame() draw-eligibility check)
                                // [TronT3] gTronTrailProgram (trailsWanted()/resolveFrame()/renderTrails())
#include "llvoavatar.h"         // [TronT3] getRenderPosition()/getLastAnimExtents()
#include "pipeline.h"           // LLPipeline::RotoInkCandidate, collectRotoInkCandidates, mExposureMap, mRT/mMainRT

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

extern bool gCubeSnapshot;
extern bool gSnapshotNoPost;
// [TronT1] Same re-declaration idiom already used for gFrameCount by
// lldrawpoolavatar.h / llperfstats.h / llvoavatar.h -- avoids pulling in
// llappviewer.h just for one counter.
extern U32 gFrameCount;

namespace
{
// [TronT0] Replace NaN/inf with a fallback before it ever reaches llclamp
// (which passes NaN through unchanged) or a shader uniform -- same pattern
// as ALCineRigRim's finiteOr / LLPipeline::renderCineOutline's finite_or.
F32 finite_or(F32 v, F32 fallback)
{
    return std::isfinite(v) ? v : fallback;
}

F64 finite_or(F64 v, F64 fallback)
{
    return std::isfinite(v) ? v : fallback;
}

LLColor3 sanitize_color(const LLColor3& c, const LLColor3& fallback)
{
    return LLColor3(
        llclamp(finite_or(c.mV[0], fallback.mV[0]), 0.f, 16.f),
        llclamp(finite_or(c.mV[1], fallback.mV[1]), 0.f, 16.f),
        llclamp(finite_or(c.mV[2], fallback.mV[2]), 0.f, 16.f));
}

// [TronT1] Named palette table (TronPalette combo ids 1..10, design v1
// section 8, review fold-in (c) / T1 contract section 10). accent = the
// look's "second neon" (1982 red MCP lines, Uprising blue, Rinzler deep
// orange) -- major grid lines mix(primary, accent, 0.5) and it is Rig Rim
// Tron colour source 2; pulse = a hotter/brighter tint of the primary so
// travelling grid pulses and Rig Rim mode-2 pulses read as energy on the
// line, not just a colour swap.
struct NamedPalette
{
    S32 mId;
    F32 mPr, mPg, mPb;
    F32 mSr, mSg, mSb;
    F32 mAr, mAg, mAb;
    F32 mUr, mUg, mUb;
};

const NamedPalette kPalettes[] = {
    // id  primary                secondary              accent                 pulse
    {  1, 0.15f,0.90f,1.00f,      1.00f,0.45f,0.05f,      1.00f,1.00f,1.00f,     0.60f,1.00f,1.00f }, // Legacy Cyan
    {  2, 1.00f,0.08f,0.06f,      1.00f,1.00f,1.00f,      1.00f,0.90f,0.85f,     1.00f,0.50f,0.40f }, // Ares Red
    {  3, 0.55f,0.85f,1.00f,      1.00f,0.90f,0.50f,      1.00f,0.35f,0.30f,     0.90f,0.97f,1.00f }, // Classic 1982
    {  4, 1.00f,0.55f,0.05f,      1.00f,0.08f,0.06f,      1.00f,0.85f,0.55f,     1.00f,0.75f,0.30f }, // Recognizer Orange
    {  5, 1.00f,1.00f,1.00f,      0.15f,0.90f,1.00f,      0.35f,0.55f,1.00f,     0.70f,0.95f,1.00f }, // Uprising White
    {  6, 1.00f,0.55f,0.05f,      1.00f,0.90f,0.50f,      1.00f,0.25f,0.05f,     1.00f,0.80f,0.45f }, // Rinzler
    {  7, 1.00f,1.00f,1.00f,      1.00f,1.00f,1.00f,      0.85f,0.92f,1.00f,     0.90f,0.95f,1.00f }, // Siren White
    {  8, 1.00f,1.00f,1.00f,      0.15f,0.90f,1.00f,      0.60f,0.75f,1.00f,     0.75f,1.00f,1.00f }, // Quorra
    {  9, 1.00f,0.75f,0.20f,      1.00f,0.45f,0.05f,      1.00f,0.95f,0.70f,     1.00f,0.90f,0.55f }, // Clu Gold
    { 10, 0.15f,0.90f,1.00f,      0.10f,0.30f,1.00f,      0.55f,0.75f,1.00f,     0.40f,0.80f,1.00f }, // Grid Blue
};

// [TronT0] Every Tron* setting introduced by this phase. Later phases
// (T1 grade/grid, T2 traces/rim/Actor FX, T3 trails, plus the Rig Rim /
// Roto Ink integration keys) append their own keys here as they land.
const char* const kAllSettings[] = {
    "TronEnabled",
    "TronPalette",
    "TronColorPrimary",
    "TronColorSecondary",
    "TronColorAccent",
    "TronColorPulse",
    "TronAnimSpeed",
    "TronAnimPause",
    "TronAnimFollowDirector",
    "TronPulseRate",
    "TronPulseShape",
    "TronPulseAmount",
    "TronGridOriginGlobal",
    // [TronT1] Layer (shot key, never written by a preset).
    "TronLayer",
    // [TronT1] Grade.
    "TronGradeStrength",
    "TronGradeDarkenEV",
    "TronGradeDesaturate",
    "TronGradeCrush",
    "TronGradeTint",
    "TronGradeTintAmount",
    "TronGradeKeepBrightLo",
    "TronGradeKeepBrightHi",
    "TronGradeKeepSubject",
    "TronGradeSkyDarken",
    // [TronT1] Grid.
    "TronGridEnabled",
    "TronGridIntensity",
    "TronGridSpacing",
    "TronGridWidth",
    "TronGridMinWidthPx",
    "TronGridMajorEvery",
    "TronGridMajorWidth",
    "TronGridMajorIntensity",
    "TronGridFarFade",
    "TronGridFloor",
    "TronGridWall",
    "TronGridCeiling",
    "TronGridSharpness",
    "TronGridNormalSource",
    "TronGridGlow",
    // [TronT1] Grid / subject (shot keys, never written by a preset).
    "TronGridSubjectExclude",
    "TronGridSubjectRadius",
    "TronGridSubjectRadiusFeather",
    "TronGridWaterMode",
    "TronGridWaterTolerance",
    // [TronT1] Pulses.
    "TronPulseGridAmount",
    "TronPulseGridSpeed",
    "TronPulseGridLength",
    "TronPulseGridWavelength",
    "TronPulseGridDensity",
    "TronPulseGridDirection",
    "TronPulseGridColorMode",
    "TronPulseSeed",
    // [TronT1] Subject / targets (shot keys, never written by a preset).
    "TronSubjectTarget",
    "TronSubjectTargetSet",
    "TronSubjectMaxTargets",
    "TronSubjectShape",
    "TronSubjectFeather",
    "TronSubjectDepthRange",
    "TronSubjectEllipseScale",
    "TronSubjectSourceGrid",
    "TronSubjectSourceRim",
    "TronSubjectInvert",
    // [TronT1] Advanced (shot keys).
    "TronNoPostScale",
    "TronBloomMeterScale",
    // [TronT2] Circuit traces (look keys).
    "TronTraceEnabled",
    "TronTraceIntensity",
    "TronTraceCell",
    "TronTraceWidth",
    "TronTraceDensity",
    "TronTracePadRadius",
    "TronTraceDiagonal",
    "TronTraceWalls",
    "TronTraceFloors",
    "TronTracePulseAmount",
    "TronTracePulseSpeed",
    "TronTracePulseLength",
    "TronTraceSeed",
    // [TronT2] Post neon rim (look keys).
    "TronRimEnabled",
    "TronRimGain",
    "TronRimExponent",
    "TronRimSilhouetteGain",
    "TronRimSilhouetteThreshold",
    "TronRimPulseAmount",
    "TronRimScanAmount",
    "TronRimScanSpeed",
    "TronRimScanWidth",
    "TronRimColorMode",
    "TronRimRejectFloors",
    // [TronT2] Actor FX "Tron Suit" (shot keys -- per cast member styling,
    // never written by a Tron look preset).
    "TronSuitUsePalette",
    "TronSuitSeamCell",
    "TronSuitSeamWidth",
    "TronSuitSeamDensity",
    "TronSuitSeamGain",
    "TronSuitRimGain",
    // [TronT3] Light-cycle trails. "L" (look) keys are written by a Tron look
    // preset (T3 contract section 7); "S" (shot) keys never are -- see the
    // per-key table in the T3 contract section 5.
    "TronTrailEnabled",           // L
    "TronTrailStyle",             // L
    "TronTrailTargetSet",         // S
    "TronTrailTarget",            // S
    "TronTrailMaxActors",         // S
    "TronTrailIntensity",         // L
    "TronTrailColorMode",         // L
    "TronTrailHeight",            // L
    "TronTrailWidth",             // L
    "TronTrailScaleToAvatar",     // L
    "TronTrailLift",              // S
    "TronTrailFadeTime",          // L
    "TronTrailFadeCurve",         // L
    "TronTrailEdge",              // L
    "TronTrailEdgeGain",          // L
    "TronTrailBodyGain",          // L
    "TronTrailGlow",              // L
    "TronTrailFog",               // L
    "TronTrailFogDensity",        // L
    "TronTrailSpacing",           // S
    "TronTrailMinSpeed",          // S
    "TronTrailIdleBreak",         // S
    "TronTrailBreakDistance",     // S
    "TronTrailMaxPoints",         // S
    "TronTrailMaxSegments",       // S
    "TronTrailClearOnCut",        // S
    // [TronT1 P2-10] Rig Rim Tron tint (look keys, written only by Tron
    // presets) and the Roto Ink / Tron clock sync flag. These are NOT
    // ALCineRigRim's / ALRotoInk's own settings -- they live on those other
    // panels' UI but are semantically part of the Tron *integration*
    // surface (the "Tron tint" row on the Rig Rim card; "Sync Roto Ink to
    // Tron clock" on both the Roto Ink and Tron tabs) -- deliberately
    // included here so the Tron tab's "Reset all" puts the WHOLE Tron
    // feature (including how it reaches into Rig Rim / Roto Ink) back to
    // its defaults in one action, same as ALPanelCineLightRig::settings()
    // and ALRotoInk's own kAllSettings independently include their own
    // copies of these same three/one key(s) for their OWN "reset" buttons.
    // This is intentional cross-ownership, not a leak: each feature's reset
    // touches the shared integration keys it introduces.
    "CineRigRimTronMode",
    "CineRigRimTronMix",
    "CineRigRimTronColorSource",
    "CineOutlineAnimUseTronClock",
    // [TronT1] UI behaviour only (shot keys) -- deliberately kept in
    // kAllSettings (and therefore in resetToDefaults()) so "Reset all"
    // restores these two checkboxes too, but excluded from ALTron::
    // settings()'s Director scene list below: they are pure Lightbox UI
    // state (whether applying a look preset also cascades into a linked
    // Roto Ink / Rig Rim preset), not part of a shot or a look, and a scene
    // save/load has no business silently flipping a UI checkbox.
    "TronPresetAlsoRoto",
    "TronPresetAlsoRigRim",
};

// [TronT1 P2-10] Keys that belong in kAllSettings (and therefore in
// resetToDefaults()) but must NOT appear in the Director scene list --
// see the comment on TronPresetAlsoRoto/TronPresetAlsoRigRim above.
const char* const kDirectorExcluded[] = {
    "TronPresetAlsoRoto",
    "TronPresetAlsoRigRim",
};

// [TronT1] One T1 "look" preset (section 8 of the T1 contract). Every row
// writes the full common column plus its own overrides; PulseGridAmount and
// GridMinWidthPx are still per-row fields even though only "siren" departs
// from the shared 1.5 / 1.2 defaults, so a future look preset can silence
// the pulses or tighten the line without adding a special case here.
struct LookPreset
{
    const char* mKey;
    S32   mPaletteId;
    F32   mDarkenEV, mDesaturate;
    F32   mTintR, mTintG, mTintB, mTintAmount;
    F32   mSpacing, mWidth, mMinWidthPx;
    S32   mMajorEvery;
    // [TronT2] Moved out of applyPreset()'s common column into per-row fields
    // -- the new looks need their own grade strength / grid on-off / wall
    // and ceiling weights / water mode (contract section 8.1).
    F32   mGradeStrength;
    bool  mGridEnabled;
    F32   mGridWall, mGridCeiling;
    S32   mWaterMode;
    F32   mPulseGridAmount;
    F32   mPulseSpeed, mPulseWavelength, mPulseDensity;
    S32   mPulseDirection, mPulseColorMode;
    S32   mPulseShape;
    F32   mPulseRate;
    // [TronT2] Circuit traces.
    bool  mTraceEnabled;
    F32   mTraceIntensity, mTraceCell, mTraceWidth, mTraceDensity;
    F32   mTraceWalls, mTraceFloors;
    F32   mTracePulseSpeed;
    // [TronT2] Post neon rim.
    bool  mRimEnabled;
    F32   mRimGain, mRimExponent;
    F32   mRimPulseAmount, mRimScanAmount;
    S32   mRimColorMode;
    const char* mRotoKey;
    const char* mRigRimLabel;
    S32   mRigRimTronMode;
    // [TronT3] Appended at the end (T3 contract section 7.1) so the 14
    // existing rows only gain a trailing line, never a mid-row shift.
    F32   mFarFade;                       // was the common column's TronGridFarFade 120 -- lightcycle needs 400
    bool  mTrailEnabled;
    S32   mTrailStyle;                    // 0 wall / 1 ground / 2 camera
    F32   mTrailIntensity, mTrailHeight, mTrailWidth, mTrailFadeTime, mTrailBodyGain;
    S32   mTrailColorMode;
};

// id/key mirrors kPalettes; see the T1 contract section 8 / T2 contract
// section 8 for the source tables (common column + this per-row override
// table). Field order matches the LookPreset struct declaration above:
// key, pal, EV, Desat, TintR/G/B, TintAmt, Spacing, Width, MinPx, MajorEvery,
// GradeStrength, GridEnabled, GridWall, GridCeiling, WaterMode,
// PulseGridAmount, PulseSpeed, PulseWavelength, PulseDensity, PulseDirection,
// PulseColorMode, PulseShape, PulseRate,
// TraceEnabled, TraceIntensity, TraceCell, TraceWidth, TraceDensity,
// TraceWalls, TraceFloors, TracePulseSpeed,
// RimEnabled, RimGain, RimExponent, RimPulseAmount, RimScanAmount,
// RimColorMode, RotoKey, RigRimLabel, RigRimTronMode,
// [TronT3] FarFade, TrailEnabled, TrailStyle, TrailIntensity, TrailHeight,
// TrailWidth, TrailFadeTime, TrailBodyGain, TrailColorMode.
const LookPreset kLookPresets[] = {
    { "legacy", 1, 3.0f, 0.6f, 0.55f, 0.75f, 1.00f, 0.30f, 2.0f, 0.020f, 1.2f, 4,
      0.85f, true, 0.6f, 0.3f, 0,
      1.5f, 6.0f, 24.0f, 0.35f, 3, 0, 0, 0.8f,
      false, 4.0f, 0.5f, 0.015f, 0.45f, 1.0f, 0.0f, 2.0f,
      true, 6.0f, 3.0f, 0.3f, 0.0f, 0,
      "tron_legacy", "Tron Suit Kick", 2,
      120.f, true, 0, 8.f, 1.2f, 0.6f, 4.f, 0.35f, 1 },
    { "ares", 2, 3.5f, 0.7f, 1.00f, 0.55f, 0.45f, 0.25f, 3.0f, 0.030f, 1.2f, 3,
      0.85f, true, 0.6f, 0.3f, 0,
      1.5f, 10.0f, 24.0f, 0.40f, 1, 0, 2, 1.2f,
      true, 4.0f, 0.5f, 0.015f, 0.45f, 0.35f, 0.0f, 2.0f,
      true, 8.0f, 3.0f, 0.3f, 0.0f, 0,
      "tron_ares", "Ares Red Backlight", 2,
      120.f, true, 0, 9.f, 1.5f, 0.6f, 3.f, 0.35f, 0 },
    { "classic82", 3, 4.0f, 0.9f, 1.00f, 1.00f, 1.00f, 0.00f, 4.0f, 0.040f, 1.2f, 0,
      0.85f, true, 0.6f, 0.3f, 0,
      1.5f, 3.0f, 32.0f, 0.30f, 0, 1, 3, 0.5f,
      false, 4.0f, 0.5f, 0.015f, 0.45f, 1.0f, 0.0f, 2.0f,
      true, 4.0f, 2.0f, 0.3f, 0.0f, 0,
      "tron_1982", "Sci-Fi Hologram", 1,
      120.f, false, 0, 8.f, 2.0f, 0.6f, 6.f, 0.35f, 0 },
    { "recognizer", 4, 3.0f, 0.5f, 1.00f, 0.80f, 0.60f, 0.20f, 8.0f, 0.050f, 1.2f, 2,
      0.85f, true, 0.6f, 0.3f, 0,
      1.5f, 12.0f, 48.0f, 0.70f, 3, 0, 0, 0.8f,
      true, 4.0f, 0.5f, 0.015f, 0.45f, 0.6f, 0.0f, 2.0f,
      true, 5.0f, 3.0f, 0.3f, 0.0f, 0,
      "tron_recognizer", "Strong Backlight", 1,
      120.f, false, 0, 8.f, 1.2f, 0.6f, 4.f, 0.35f, 0 },
    { "uprising", 5, 2.5f, 0.4f, 0.70f, 0.85f, 1.00f, 0.20f, 1.0f, 0.012f, 1.2f, 8,
      0.85f, true, 0.6f, 0.3f, 0,
      1.5f, 4.0f, 16.0f, 0.15f, 3, 0, 0, 0.8f,
      false, 4.0f, 0.5f, 0.015f, 0.45f, 1.0f, 0.0f, 2.0f,
      true, 4.0f, 3.0f, 0.3f, 0.8f, 0,
      "tron_uprising", "Silhouette Glow (any angle)", 1,
      120.f, true, 2, 6.f, 1.0f, 0.35f, 2.5f, 0.35f, 0 },
    { "rinzler", 6, 3.5f, 0.7f, 1.00f, 1.00f, 1.00f, 0.00f, 2.0f, 0.020f, 1.2f, 4,
      0.85f, true, 0.6f, 0.3f, 0,
      1.5f, 6.0f, 24.0f, 0.35f, 3, 0, 0, 0.8f,
      true, 4.0f, 0.5f, 0.015f, 0.45f, 0.3f, 0.0f, 2.0f,
      true, 7.0f, 4.0f, 0.3f, 0.0f, 0,
      "tron_recognizer", "Noir Kicker", 2,
      120.f, true, 0, 8.f, 1.2f, 0.6f, 4.f, 0.35f, 0 },
    { "siren", 7, 2.0f, 0.3f, 0.75f, 0.85f, 1.00f, 0.35f, 0.5f, 0.008f, 1.0f, 4,
      0.85f, true, 0.6f, 0.3f, 0,
      0.0f, 6.0f, 24.0f, 0.35f, 3, 0, 0, 0.8f,
      false, 4.0f, 0.5f, 0.015f, 0.45f, 1.0f, 0.0f, 2.0f,
      true, 3.0f, 1.5f, 0.3f, 0.5f, 0,
      "tron_uprising", "Dreamy Halo", 1,
      120.f, false, 0, 8.f, 1.2f, 0.6f, 4.f, 0.35f, 0 },
    { "quorra", 8, 3.0f, 0.5f, 0.55f, 0.75f, 1.00f, 0.30f, 2.0f, 0.020f, 1.2f, 4,
      0.85f, true, 0.6f, 0.3f, 0,
      1.5f, 6.0f, 24.0f, 0.35f, 3, 1, 0, 0.8f,
      false, 4.0f, 0.5f, 0.015f, 0.45f, 1.0f, 0.0f, 2.0f,
      true, 5.0f, 3.0f, 0.3f, 0.0f, 1,
      "tron_legacy", "Fashion Edge", 2,
      120.f, true, 0, 8.f, 1.2f, 0.6f, 4.f, 0.35f, 1 },
    // [TronT2] Six new looks (design v1 section 10; T2 contract section 8.3).
    { "clugold", 9, 3.5f, 0.6f, 1.00f, 0.85f, 0.60f, 0.30f, 4.0f, 0.030f, 1.2f, 2,
      0.85f, true, 0.6f, 0.3f, 0,
      1.5f, 8.0f, 24.0f, 0.35f, 3, 0, 0, 0.8f,
      true, 4.0f, 0.5f, 0.015f, 0.45f, 0.5f, 0.0f, 2.0f,
      true, 6.0f, 3.0f, 0.3f, 0.0f, 0,
      "tron_recognizer", "Two-Sided Kick", 1,
      120.f, true, 0, 8.f, 1.2f, 0.6f, 4.f, 0.35f, 0 },
    { "seaofsim", 10, 3.0f, 0.6f, 0.55f, 0.75f, 1.00f, 0.40f, 1.0f, 0.015f, 1.2f, 8,
      0.85f, true, 0.6f, 0.3f, 1,
      1.5f, 4.0f, 48.0f, 0.35f, 3, 0, 0, 0.8f,
      false, 4.0f, 0.5f, 0.015f, 0.45f, 1.0f, 0.0f, 2.0f,
      true, 4.0f, 3.0f, 0.3f, 0.0f, 0,
      "tron_legacy", "Moonlit Rim", 1,
      120.f, true, 1, 6.f, 1.2f, 0.8f, 5.f, 0.5f, 0 },
    { "circuit", 1, 3.0f, 0.7f, 1.00f, 1.00f, 1.00f, 0.00f, 4.0f, 0.020f, 1.2f, 4,
      0.85f, true, 0.0f, 0.0f, 0,
      1.5f, 6.0f, 24.0f, 0.15f, 3, 0, 0, 0.8f,
      true, 5.0f, 0.3f, 0.015f, 0.6f, 1.0f, 0.0f, 4.0f,
      true, 5.0f, 3.0f, 0.3f, 0.0f, 0,
      "tron_legacy", "Neon / Stage", 1,
      120.f, false, 0, 8.f, 1.2f, 0.6f, 4.f, 0.35f, 0 },
    { "arena", 5, 2.5f, 0.5f, 0.70f, 0.85f, 1.00f, 0.20f, 1.0f, 0.020f, 1.2f, 2,
      0.85f, true, 0.6f, 0.3f, 0,
      1.5f, 14.0f, 16.0f, 0.80f, 3, 0, 0, 0.8f,
      false, 4.0f, 0.5f, 0.015f, 0.45f, 1.0f, 0.0f, 2.0f,
      true, 8.0f, 3.0f, 0.3f, 1.0f, 0,
      "tron_uprising", "Silhouette Glow (any angle)", 2,
      120.f, true, 1, 8.f, 1.2f, 0.5f, 3.f, 0.5f, 1 },
    { "gridonly", 1, 3.0f, 0.6f, 0.55f, 0.75f, 1.00f, 0.30f, 2.0f, 0.020f, 1.2f, 4,
      0.0f, true, 0.6f, 0.3f, 0,
      1.5f, 6.0f, 24.0f, 0.35f, 3, 0, 0, 0.8f,
      false, 4.0f, 0.5f, 0.015f, 0.45f, 1.0f, 0.0f, 2.0f,
      false, 6.0f, 3.0f, 0.3f, 0.0f, 0,
      "", "", 0,
      120.f, false, 0, 8.f, 1.2f, 0.6f, 4.f, 0.35f, 1 },
    { "neonsuit", 1, 3.0f, 0.6f, 0.55f, 0.75f, 1.00f, 0.30f, 2.0f, 0.020f, 1.2f, 4,
      0.0f, false, 0.6f, 0.3f, 0,
      1.5f, 6.0f, 24.0f, 0.35f, 3, 0, 0, 0.8f,
      false, 4.0f, 0.5f, 0.015f, 0.45f, 1.0f, 0.0f, 2.0f,
      true, 8.0f, 3.0f, 0.4f, 0.0f, 0,
      "tron_legacy", "Tron Suit Kick", 2,
      120.f, false, 0, 8.f, 1.2f, 0.6f, 4.f, 0.35f, 1 },
    // [TronT3] New look (design v1 section 10; T3 contract section 7.3):
    // very sparse pulses, traces off, rim on low, tall long-lived light
    // walls behind the cast.
    { "lightcycle", 1, 4.0f, 0.8f, 0.55f, 0.75f, 1.00f, 0.20f, 8.0f, 0.040f, 1.2f, 4,
      0.85f, true, 0.6f, 0.3f, 0,
      1.5f, 6.0f, 24.0f, 0.10f, 3, 0, 0, 0.8f,
      false, 4.0f, 0.5f, 0.015f, 0.45f, 1.0f, 0.0f, 2.0f,
      true, 3.0f, 3.0f, 0.3f, 0.0f, 0,
      "tron_legacy", "Subtle Edge", 1,
      400.f, true, 0, 8.0f, 2.5f, 0.6f, 8.0f, 0.5f, 1 },
};

std::string sLastPresetRotoKey;
std::string sLastPresetRigRimLabel;
S32         sLastPresetRigRimTronMode = 0;

ALTron::FrameClock sClock;
ALTron::Anchor     sAnchor;

// [TronT3] Light-cycle trail storage (T3 contract section 3.2). Positions are
// GLOBAL so an agent-origin shift on region crossing changes nothing; the
// region handle itself is never tracked. std::vector, NOT std::deque, for
// mPts -- see the storage-budget note in the contract (MSVC deque per-block
// overhead on 40 B elements is ~2.5x; a front-erase on a <=1024-element
// vector is a <=40 KB memmove, negligible).
struct TrailPoint
{
    LLVector3d mGlobal;
    F64        mEmitClock;
    F32        mSize; // cached actor scale (2*hz/1.9 or 1), captured at emission
                       // so a laid trail keeps its shape if the actor rescales later
};
struct TrailSegment
{
    std::vector<TrailPoint> mPts; // oldest first
};
struct Trail
{
    S32  mPalette = 0;                 // RotoInkCandidate::mPaletteIdx (refreshed every sample)
    std::vector<TrailSegment> mSegs;   // oldest first
    LLVector3d mLastGlobal;            // last EMITTED point (spacing + resume rule ONLY)
    LLVector3d mPrevGlobal;            // previous SAMPLED position (speed + teleport, P1-2)
    bool mHaveLast = false;            // mLastGlobal valid (first point emitted)
    bool mPrevValid = false;           // mPrevGlobal valid
    bool mNeedBreak = true;            // next emitted point starts a new segment
    bool mResumePending = false;       // set for every trail on a sampling false->true transition
    F32  mSpeedEma = 0.f;              // m/s, EMA of consecutive-sample speed (tau 0.15 s)
    F32  mFootOffset = 0.f;            // ext[0].z - renderPos.z, cached (P2-4); avatars only
    F32  mSize = 1.f;                  // cached actor scale (2*hz/1.9 or 1)
    F64  mIdleAccum = 0.0;             // sampled-frame wall seconds below TronTrailMinSpeed
    F64  mUnseenAccum = 0.0;           // sampled-frame wall seconds not in the candidate set
    F64  mLastActivityClock = 0.0;     // clock of the last emitted point (eviction key, P1-1)
};
// HARD CAP: size() <= TronTrailMaxActors after every updateTrails() call.
std::map<LLUUID, Trail> sTrails;
bool sHasGeometry = false;
bool sWasSampling = false;
U64  sLastCutSerial = 0;  bool sCutSerialInit = false;
U64  sLastGeneration = 0; bool sGenerationInit = false; // LLPresentationTime generation (P1-3)

// [TronT3] Sum of every point currently stored across every segment of `t`.
S32 trail_total_points(const Trail& t)
{
    S32 total = 0;
    for (const TrailSegment& seg : t.mSegs)
    {
        total += (S32)seg.mPts.size();
    }
    return total;
}

// [TronT3] Enforces the per-trail MaxSegments/MaxPoints caps by trimming the
// OLDEST material first (front segment, then that segment's front point).
// Shared between the per-frame age/prune pass and emit()'s post-push call.
void trail_cap(Trail& t, S32 max_segments, S32 max_points)
{
    while ((S32)t.mSegs.size() > max_segments)
    {
        t.mSegs.erase(t.mSegs.begin());
    }
    while (trail_total_points(t) > max_points)
    {
        for (TrailSegment& seg : t.mSegs)
        {
            if (!seg.mPts.empty())
            {
                seg.mPts.erase(seg.mPts.begin());
                break;
            }
        }
        t.mSegs.erase(std::remove_if(t.mSegs.begin(), t.mSegs.end(),
            [](const TrailSegment& seg) { return seg.mPts.empty(); }), t.mSegs.end());
    }
}

// [TronT3 P2-5] Every `normalize` in the trail vertex builder goes through
// this helper instead of an unchecked normalize() -- a length-squared test
// BEFORE any division, so a degenerate input (e.g. a camera-ribbon point
// sitting exactly at the camera) yields the deterministic fallback, never a
// NaN or a vanishing strip.
LLVector3 trail_safe_unit(const LLVector3& v, const LLVector3& fallback)
{
    const F32 l2 = v.lengthSquared();
    return (l2 > 1.0e-8f) ? (v * (1.f / std::sqrt(l2))) : fallback;
}

// [TronT0] Re-snap the world anchor to the 1024 m global lattice cell
// containing the camera (design v3/v4 section 2.3). Recomputed every latch
// from scratch (not hysteresis-based): floor() is idempotent while the
// camera stays within the same cell, so this is a no-op most frames and
// only changes mAnchorGlobal on the frame the camera actually crosses into
// a new cell.
void update_anchor()
{
    LLViewerCamera* camera = LLViewerCamera::getInstance();
    if (!camera)
    {
        return; // keep the last anchor; nothing to resolve it from yet
    }
    const LLVector3d cam_global = gAgent.getPosGlobalFromAgent(camera->getOrigin());
    if (!cam_global.isFinite())
    {
        return; // corrupt/uninitialised camera state -- keep the last anchor
    }

    const LLVector3d new_anchor(
        std::floor(cam_global.mdV[VX] / 1024.0) * 1024.0,
        std::floor(cam_global.mdV[VY] / 1024.0) * 1024.0,
        std::floor(cam_global.mdV[VZ] / 1024.0) * 1024.0);

    if (!sAnchor.mValid || new_anchor != sAnchor.mAnchorGlobal)
    {
        sAnchor.mAnchorGlobal = new_anchor;
        sAnchor.mValid = true;
    }
}
} // namespace

// [TronT1]
bool ALTron::isEnabledMaster()
{
    static LLCachedControl<bool> enabled(gSavedSettings, "TronEnabled", false);
    return enabled();
}

// [TronT3] EXACTLY renderTronWorld's own local early-out predicate (O-P1-2):
// TronGradeStrength > 0 || TronGridEnabled || (TronTraceEnabled &&
// TronTraceIntensity > 0) || (TronRimEnabled && TronRimGain > 0), finite_or'd.
// Split out of isEnabled() so the trail-only case is distinguishable -- see
// the header comment. This is a PRE-GATE only: renderTronWorld always
// re-evaluates its own local copy from its own sanitised locals, so the two
// can never disagree about whether the world pass actually draws.
bool ALTron::worldHasWork()
{
    static LLCachedControl<F32>  grade_strength(gSavedSettings, "TronGradeStrength", 0.85f);
    static LLCachedControl<bool> grid_enabled(gSavedSettings, "TronGridEnabled", true);
    static LLCachedControl<bool> trace_enabled(gSavedSettings, "TronTraceEnabled", false);
    static LLCachedControl<F32>  trace_intensity(gSavedSettings, "TronTraceIntensity", 4.0f);
    static LLCachedControl<bool> rim_enabled(gSavedSettings, "TronRimEnabled", false);
    static LLCachedControl<F32>  rim_gain(gSavedSettings, "TronRimGain", 6.0f);

    const bool trace_on = trace_enabled() && finite_or((F32)trace_intensity(), 4.0f) > 0.f;
    const bool rim_on   = rim_enabled() && finite_or((F32)rim_gain(), 6.0f) > 0.f;
    return finite_or((F32)grade_strength(), 0.85f) > 0.f || grid_enabled() || trace_on || rim_on;
}

// [TronT1] T0's isEnabled() was the raw master read; T1 extends it with the
// grade/grid OR (design section 4) so isActiveForCurrentPass() -- and every
// caller that gates real render work on it -- returns false whenever there
// is nothing to draw, without needing its own redundant grade/grid check.
// [TronT3] Now isEnabledMaster() && (worldHasWork() || TronTrailEnabled):
// trails alone (world pass fully off) still count as "Tron has something to
// draw" -- see the T3 contract section O-P1-2.
bool ALTron::isEnabled()
{
    if (!isEnabledMaster())
    {
        return false;
    }
    // [TronT3 fix] Keep T2's flag-only world predicate here (NOT
    // worldHasWork(), which also requires intensity/gain > 0): isEnabled()
    // feeds isActiveForCurrentPass() and the bloom-metering ramp, and with
    // trails off both must stay exactly as in T2. worldHasWork() remains the
    // tighter pre-gate for actually issuing the world pass.
    static LLCachedControl<F32>  grade_strength(gSavedSettings, "TronGradeStrength", 0.85f);
    static LLCachedControl<bool> grid_enabled(gSavedSettings, "TronGridEnabled", true);
    static LLCachedControl<bool> trace_enabled(gSavedSettings, "TronTraceEnabled", false);
    static LLCachedControl<bool> rim_enabled(gSavedSettings, "TronRimEnabled", false);
    static LLCachedControl<bool> trail_enabled(gSavedSettings, "TronTrailEnabled", false);
    return finite_or((F32)grade_strength(), 0.85f) > 0.f || grid_enabled() ||
        trace_enabled() || rim_enabled() || trail_enabled();
}

// [TronT0]
bool ALTron::isActiveForCurrentPass()
{
    if (!isEnabled() || gCubeSnapshot || gSnapshotNoPost)
    {
        return false;
    }
    if (LLPipeline::sRenderingHUDs || LLPipeline::sImpostorRender || LLPipeline::sPrismLensRender)
    {
        return false;
    }
    // Mirrors LLPipeline::shouldRunRotoInkAt's HDR predicate exactly (both
    // are pure functions of stable per-session/cached-setting state), so
    // this always agrees with whether the HDR post chain is even running
    // this frame.
    static LLCachedControl<bool> has_hdr(gSavedSettings, "RenderHDREnabled", true);
    return gGLManager.mGLVersion > 4.05f && has_hdr();
}

// [TronT0]
ALTron::Palette ALTron::palette()
{
    static const LLColor3 default_primary(0.15f, 0.90f, 1.00f);
    static const LLColor3 default_secondary(1.00f, 0.45f, 0.05f);
    static const LLColor3 default_accent(1.f, 1.f, 1.f);
    static const LLColor3 default_pulse(0.6f, 1.0f, 1.0f);
    static LLCachedControl<LLColor3> primary_ctrl(gSavedSettings, "TronColorPrimary", default_primary);
    static LLCachedControl<LLColor3> secondary_ctrl(gSavedSettings, "TronColorSecondary", default_secondary);
    static LLCachedControl<LLColor3> accent_ctrl(gSavedSettings, "TronColorAccent", default_accent);
    static LLCachedControl<LLColor3> pulse_ctrl(gSavedSettings, "TronColorPulse", default_pulse);

    Palette p;
    p.mPrimary   = sanitize_color(primary_ctrl,   default_primary);
    p.mSecondary = sanitize_color(secondary_ctrl, default_secondary);
    p.mAccent    = sanitize_color(accent_ctrl,    default_accent);
    p.mPulse     = sanitize_color(pulse_ctrl,     default_pulse);
    return p;
}

// [TronT0]
void ALTron::applyPalettePreset(S32 id)
{
    for (const NamedPalette& row : kPalettes)
    {
        if (row.mId == id)
        {
            gSavedSettings.setUntypedValue("TronColorPrimary",
                LLColor3(row.mPr, row.mPg, row.mPb).getValue());
            gSavedSettings.setUntypedValue("TronColorSecondary",
                LLColor3(row.mSr, row.mSg, row.mSb).getValue());
            gSavedSettings.setUntypedValue("TronColorAccent",
                LLColor3(row.mAr, row.mAg, row.mAb).getValue());
            gSavedSettings.setUntypedValue("TronColorPulse",
                LLColor3(row.mUr, row.mUg, row.mUb).getValue());
            return;
        }
    }
    // id 0 (Custom) or an unknown id: leave the colour settings untouched.
}

// [TronT1]
F32 ALTron::pulse01()
{
    static LLCachedControl<S32> shape_ctrl(gSavedSettings, "TronPulseShape", 0);
    const S32 shape = std::clamp((S32)shape_ctrl(), 0, 3);
    const F32 ph = clock().mPulsePhase01;

    switch (shape)
    {
        case 1: // saw
            return llclamp(ph, 0.f, 1.f);
        case 2: // heartbeat: two bumps, the first wrapped across the 0/1 seam
        {
            const auto bump = [](F32 x, F32 c, F32 w) -> F32
            {
                const F32 t = (x - c) / w;
                return std::exp(-(t * t));
            };
            const F32 b1 = llmax(llmax(bump(ph, 0.f, 0.10f), bump(ph - 1.f, 0.f, 0.10f)),
                                  bump(ph + 1.f, 0.f, 0.10f));
            const F32 b2 = 0.55f * bump(ph, 0.28f, 0.10f);
            return llclamp(llmax(b1, b2), 0.f, 1.f);
        }
        case 3: // square
            return (ph < 0.5f) ? 1.f : 0.f;
        default: // sine
            return 0.5f - 0.5f * std::cos(F_TWO_PI * ph);
    }
}

// [TronT1]
F32 ALTron::pulseMul()
{
    static LLCachedControl<F32> amount_ctrl(gSavedSettings, "TronPulseAmount", 0.35f);
    const F32 amount = llclamp(finite_or((F32)amount_ctrl(), 0.35f), 0.f, 1.f);
    return llmax(1.f + amount * (pulse01() - 0.5f) * 2.f, 0.f);
}

static ALTron::Frame sFrame;

// [TronT1] B1-style bloom-metering ramp, mirroring LLPipeline::
// updateNightMaskAnchor()'s own ramp_bloom_scale idiom exactly (same 0.4 s
// time constant) so a Tron toggle can pump exposure no more than a Night
// Mask toggle can. Called once per frame from renderFinalize's prologue,
// right next to updateNightMaskAnchor() itself.
void ALTron::resolveFrame()
{
    // [TronT1 P2-2 fix] renderFinalize() runs once per VIEW, not once per
    // frame: auxiliary render targets (reflection probes, mirrors, snapshot-
    // with-post) invoke it too, with gPipeline.mRT pointed at their own
    // target rather than &gPipeline.mMainRT. An auxiliary call must be a
    // COMPLETE no-op here -- not merely "skip updating the ramp" -- because
    // if it instead consumed the once-per-frame guard below, an aux call
    // that happens to run BEFORE the main view's own call this frame would
    // "win" the guard using the wrong context (an aux target's mRT can never
    // equal &mMainRT, so will_run would read false there even on a frame
    // where the main view is about to actually draw Tron World), silently
    // starving the real update. Bailing out here, before either the guard or
    // the ramp math, guarantees an auxiliary renderFinalize can never
    // double-step (or pre-empt) the ramp.
    if (gPipeline.mRT != &gPipeline.mMainRT)
    {
        return;
    }

    // Once-per-frame re-entrancy guard for the main view's own call (this
    // call site -- pipeline.cpp renderFinalize()'s HDR prologue -- only runs
    // once per main-view frame today; this makes that an enforced invariant
    // rather than an assumption a future call site could quietly violate).
    // Mirrors the mWeatherRainOcclusionFrame == gFrameCount idiom used
    // elsewhere in pipeline.cpp for the same "already ran this frame" gate.
    static U32 sLastFrame = ~0u;
    if (sLastFrame == gFrameCount)
    {
        return;
    }
    sLastFrame = gFrameCount;

    static LLCachedControl<F32> bloom_meter_scale(gSavedSettings, "TronBloomMeterScale", 0.f);
    // [TronT1 P2-2 fix] will_run must mirror the ACTUAL draw-eligibility
    // predicate renderTronWorld() itself gates on (T1 contract section 3.2
    // line 0), not just isActiveForCurrentPass() && layer()==CAMERA:
    //  - gTronWorldProgram.isComplete(): the shader must have actually
    //    linked -- llviewershadermgr's soft-fail leaves it incomplete
    //    without throwing, and an incomplete program never draws.
    //  - gPipeline.mWaterDis.isComplete(): the scratch target
    //    renderTronWorld() binds must exist.
    // (gPipeline.mRT == &gPipeline.mMainRT is already guaranteed by the
    // early return above, so it is not repeated in this condition.)
    // [TronT3] Mirrors the S3 predicate exactly (T3 contract section 4.5):
    // the S3 pass is what blooms, and a trails-only S3 frame must engage the
    // ramp too, not just a Camera-layer world frame.
    const bool will_run = isActiveForCurrentPass() && gPipeline.mWaterDis.isComplete() &&
        // [TronT3 fix] world term exactly as T2 (no worldHasWork()) so the
        // trails-off metering ramp is T2-identical.
        ((layer() == LAYER_CAMERA && gTronWorldProgram.isComplete()) ||
         (trailsWanted() && gTronTrailProgram.isComplete()));
    const F32  target = will_run ? llclamp(finite_or((F32)bloom_meter_scale(), 0.f), 0.f, 1.f) : 1.f;

    const F64 now = LLPresentationTime::currentFrame().presentation_time;
    static F64 sBloomScaleTime = -1.0;
    // [TronT1 P2] remember whether the clock advanced this frame (see the
    // frozen-clock bypass at the end).
    const bool bloom_clock_frozen = (sBloomScaleTime >= 0.0 && now == sBloomScaleTime);
    if (!std::isfinite(now))
    {
        sFrame.mBloomScale = target;
        return;
    }
    constexpr F32 BLOOM_SCALE_TAU_SEC = 0.4f;
    if (sBloomScaleTime < 0.0 || now < sBloomScaleTime)
    {
        sFrame.mBloomScale = target;
    }
    else
    {
        const F64 dt = now - sBloomScaleTime;
        const F32 alpha = 1.f - expf(-(F32)dt / BLOOM_SCALE_TAU_SEC);
        sFrame.mBloomScale += (target - sFrame.mBloomScale) * alpha;
        // [TronT1 P2-3 fix] The exponential step above only ever
        // asymptotically APPROACHES target, never reaches it exactly -- snap
        // once within float noise so the ramp actually converges (needed for
        // generateLuminance()'s night_mask_bloom_scale multiplier,
        // pipeline.cpp ~11405, to ever settle on a stable value instead of
        // drifting by a diminishing epsilon every frame forever).
        if (std::fabs(target - sFrame.mBloomScale) < 1.0e-5f)
        {
            sFrame.mBloomScale = target;
        }
    }
    sBloomScaleTime = now;

    // [TronT1 P2-3 fix] Explicit off-path bypass: while !will_run, target is
    // always exactly 1.f, but the ramp above only asymptotically approaches
    // it and makes ZERO progress on a frozen clock (dt == 0 -> alpha == 0,
    // e.g. a snapshot/capture path that does not advance presentation time).
    // generateLuminance() multiplies this value straight into the exposure
    // metering (pipeline.cpp ~11405), and the T1 "Tron off: byte-identical
    // captures" checklist item (contract section 12.2) requires that
    // multiplier to be the bit-exact pre-Tron value whenever Tron is not
    // running -- not merely close to it, and not contingent on the
    // presentation clock having advanced enough real time to ramp there.
    // [TronT1 P2] Normally let the ramp ease back to 1.0 (the epsilon snap
    // above makes it land exactly), so switching Tron off / changing layer
    // does not jump the exposure metering in one frame. Only on a frozen
    // presentation clock (no progress possible) force the exact pre-Tron
    // value immediately.
    if (!will_run && bloom_clock_frozen)
    {
        sFrame.mBloomScale = 1.f;
    }
}

// [TronT1]
const ALTron::Frame& ALTron::frame()
{
    return sFrame;
}

// [TronT3] Light-cycle trails: sample emitters, age/prune, enforce every
// storage cap, handle clears -- T3 contract section 3.3. Exactly once per
// main-loop iteration: guarded on LLPresentationTime::currentFrame().
// generation (NOT gFrameCount -- tiles / 360 faces call display() repeatedly
// within one tick, P1-3), and rejects every auxiliary render target (Prism
// aux, reflection/hero probes, impostors, GLTF preview) BEFORE that guard is
// even consulted, so an aux call can never consume the main view's sampling
// slot for the frame.
void ALTron::updateTrails()
{
    if (gPipeline.mRT != &gPipeline.mMainRT)
    {
        return; // aux-target rejection FIRST -- never consumes the generation guard
    }

    const U64 gen = LLPresentationTime::currentFrame().generation;
    if (sGenerationInit && gen == sLastGeneration)
    {
        return; // already sampled this tick
    }
    sLastGeneration = gen;
    sGenerationInit = true;

    static LLCachedControl<bool> trail_enabled_ctrl(gSavedSettings, "TronTrailEnabled", false);
    const bool enabled = isEnabledMaster() && trail_enabled_ctrl();
    if (!enabled)
    {
        // Master-off / trails-off: history cleared (T3 contract section 1 --
        // TronTrailEnabled off has no stale points reappear on re-enable).
        if (!sTrails.empty())
        {
            clearTrails();
        }
        sHasGeometry = false;
        sWasSampling = false;
        sLastCutSerial = ALDirectorSwitcher::instance().cutSerial();
        sCutSerialInit = true;
        return;
    }

    // [TronT3] Cut-serial tracked even when TronTrailClearOnCut is off, so
    // flipping the option on later does not immediately fire on a stale cut.
    static LLCachedControl<bool> clear_on_cut_ctrl(gSavedSettings, "TronTrailClearOnCut", false);
    const U64 cut = ALDirectorSwitcher::instance().cutSerial();
    if (!sCutSerialInit)
    {
        sLastCutSerial = cut;
        sCutSerialInit = true;
    }
    if (clear_on_cut_ctrl() && cut != sLastCutSerial)
    {
        clearTrails();
    }
    sLastCutSerial = cut;

    // --- settings (LLCachedControl + finite_or + clamp) ---------------------
    static LLCachedControl<F32>  fade_time_ctrl(gSavedSettings, "TronTrailFadeTime", 4.f);
    static LLCachedControl<F32>  spacing_ctrl(gSavedSettings, "TronTrailSpacing", 0.15f);
    static LLCachedControl<F32>  min_speed_ctrl(gSavedSettings, "TronTrailMinSpeed", 0.4f);
    static LLCachedControl<F32>  idle_break_ctrl(gSavedSettings, "TronTrailIdleBreak", 0.6f);
    static LLCachedControl<F32>  break_distance_ctrl(gSavedSettings, "TronTrailBreakDistance", 3.f);
    static LLCachedControl<S32>  max_points_ctrl(gSavedSettings, "TronTrailMaxPoints", 256);
    static LLCachedControl<S32>  max_segments_ctrl(gSavedSettings, "TronTrailMaxSegments", 6);
    static LLCachedControl<S32>  max_actors_ctrl(gSavedSettings, "TronTrailMaxActors", 8);
    static LLCachedControl<S32>  target_set_ctrl(gSavedSettings, "TronTrailTargetSet", 1);
    static LLCachedControl<S32>  target_ctrl(gSavedSettings, "TronTrailTarget", 0);
    static LLCachedControl<bool> scale_to_avatar_ctrl(gSavedSettings, "TronTrailScaleToAvatar", true);

    const F32 fade_time      = llclamp(finite_or((F32)fade_time_ctrl(), 4.f), 0.2f, 60.f);
    const F32 spacing        = llclamp(finite_or((F32)spacing_ctrl(), 0.15f), 0.05f, 2.f);
    const F32 min_speed      = llclamp(finite_or((F32)min_speed_ctrl(), 0.4f), 0.f, 10.f);
    const F32 idle_break     = llclamp(finite_or((F32)idle_break_ctrl(), 0.6f), 0.f, 10.f);
    const F32 break_distance = llclamp(finite_or((F32)break_distance_ctrl(), 3.f), 1.f, 64.f);
    const S32 max_points     = std::clamp((S32)max_points_ctrl(), 16, 1024);
    const S32 max_segments   = std::clamp((S32)max_segments_ctrl(), 1, 16);
    const S32 max_actors     = std::clamp((S32)max_actors_ctrl(), 1, 16);
    const S32 target_set     = std::clamp((S32)target_set_ctrl(), 0, 7);
    const S32 target         = std::clamp((S32)target_ctrl(), 0, 4);
    const bool scale_to_avatar = scale_to_avatar_ctrl();

    const ALTron::FrameClock& c = clock();
    const F64 now  = c.mClockSeconds;
    const F64 wall = llclamp(finite_or(c.mWallDelta, 0.0), 0.0, 1.0);

    // --- 4a: age / prune / CAPS -- ALWAYS, emission or not (P1-1) -----------
    for (auto& kv : sTrails)
    {
        Trail& t = kv.second;
        for (TrailSegment& seg : t.mSegs)
        {
            size_t n = 0;
            while (n < seg.mPts.size() && (now - seg.mPts[n].mEmitClock) > ((F64)fade_time + 0.25))
            {
                ++n;
            }
            if (n > 0)
            {
                seg.mPts.erase(seg.mPts.begin(), seg.mPts.begin() + n);
            }
        }
        t.mSegs.erase(std::remove_if(t.mSegs.begin(), t.mSegs.end(),
            [](const TrailSegment& seg) { return seg.mPts.empty(); }), t.mSegs.end());
        trail_cap(t, max_segments, max_points);
    }
    // capMap: covers a LOWERED TronTrailMaxActors/MaxPoints/MaxSegments at any
    // time, not only on emission.
    while ((S32)sTrails.size() > max_actors)
    {
        auto victim = sTrails.end();
        for (auto it = sTrails.begin(); it != sTrails.end(); ++it)
        {
            if (victim == sTrails.end() ||
                it->second.mLastActivityClock < victim->second.mLastActivityClock ||
                (it->second.mLastActivityClock == victim->second.mLastActivityClock && it->first < victim->first))
            {
                victim = it;
            }
        }
        if (victim == sTrails.end())
        {
            break;
        }
        sTrails.erase(victim);
    }

    // --- 4b: sampling gate (v4 section 7.2) ---------------------------------
    const bool sampling = c.mClockDelta > 0.0;
    if (sampling && !sWasSampling)
    {
        for (auto& kv : sTrails)
        {
            kv.second.mResumePending = true; // resume-after-pause rule
        }
    }
    sWasSampling = sampling;
    if (!sampling)
    {
        sHasGeometry = false;
        for (const auto& kv : sTrails)
        {
            for (const TrailSegment& seg : kv.second.mSegs)
            {
                if (seg.mPts.size() >= 2)
                {
                    sHasGeometry = true;
                    break;
                }
            }
            if (sHasGeometry)
            {
                break;
            }
        }
        return; // no emission -- idle/unseen accumulators + mPrevGlobal frozen too
    }

    // --- 5: candidates -------------------------------------------------------
    std::vector<LLPipeline::RotoInkCandidate> cands;
    gPipeline.collectRotoInkCandidates(target_set, target, cands); // UN-culled
    cands.erase(std::remove_if(cands.begin(), cands.end(),
        [](const LLPipeline::RotoInkCandidate& cand)
        { return !cand.mCenter.isFinite() || !cand.mHalfExtents.isFinite(); }), cands.end());
    LLViewerCamera* camera = LLViewerCamera::getInstance();
    const LLVector3 cam_origin = camera ? camera->getOrigin() : LLVector3::zero;
    std::sort(cands.begin(), cands.end(),
        [&cam_origin](const LLPipeline::RotoInkCandidate& a, const LLPipeline::RotoInkCandidate& b)
        {
            const F32 da = (a.mCenter - cam_origin).lengthSquared();
            const F32 db = (b.mCenter - cam_origin).lengthSquared();
            return (da != db) ? (da < db) : (a.mId < b.mId);
        });
    if ((S32)cands.size() > max_actors)
    {
        cands.resize(max_actors); // truncate ONLY, never grow
    }
    std::set<LLUUID> inset;
    for (const auto& cand : cands)
    {
        inset.insert(cand.mId);
    }

    // Motion-domain dt (O-P1-1): == wall in Live; wall x World Time Scale
    // under Temporal; > 0 whenever mClockDelta > 0 (clockDelta = presentation_
    // delta x rate) -- speed/teleport measured here, NEVER on `wall` alone, so
    // slow-mo keeps true metres/second.
    const F64 pdt = llclamp(finite_or(LLPresentationTime::currentFrame().presentation_delta, 0.0), 0.0, 1.0);

    // --- 5b: eviction BEFORE insertion (P1-1) -------------------------------
    {
        S32 need = 0;
        for (const auto& cand : cands)
        {
            if (sTrails.find(cand.mId) == sTrails.end())
            {
                ++need;
            }
        }
        const S32 room = max_actors - (S32)sTrails.size();
        if (need > room)
        {
            std::vector<std::map<LLUUID, Trail>::iterator> not_in_set;
            for (auto it = sTrails.begin(); it != sTrails.end(); ++it)
            {
                if (inset.find(it->first) == inset.end())
                {
                    not_in_set.push_back(it);
                }
            }
            std::sort(not_in_set.begin(), not_in_set.end(),
                [](const std::map<LLUUID, Trail>::iterator& a, const std::map<LLUUID, Trail>::iterator& b)
                {
                    return (a->second.mLastActivityClock != b->second.mLastActivityClock)
                        ? (a->second.mLastActivityClock < b->second.mLastActivityClock)
                        : (a->first < b->first);
                });
            const S32 to_evict = llmin(need - room, (S32)not_in_set.size());
            for (S32 i = 0; i < to_evict; ++i)
            {
                sTrails.erase(not_in_set[i]);
            }
        }
        // Invariant: |inset| <= max_actors, so after evicting every
        // not-in-set trail the set still fits.
    }

    // --- 6: per-candidate sample/emit ----------------------------------------
    for (const auto& cand : cands)
    {
        LLViewerObject* obj = gObjectList.findObject(cand.mId);
        LLVOAvatar*     av  = obj ? obj->asAvatar() : nullptr;
        const bool avatar_ok = av && !av->isDead();
        const F32  hz = cand.mHalfExtents.mV[VZ];

        Trail& t = sTrails[cand.mId]; // new entry: mNeedBreak = true (default member init)
        t.mPalette = cand.mPaletteIdx;
        t.mUnseenAccum = 0.0;
        const bool fresh = !t.mHaveLast;

        // [TronT3 P2-4 / O-P2] Pose-dependent offsets cached at creation and
        // (only) whenever emit() opens a new segment -- never mid-segment, so
        // a pose/attachment change never translates the emitter mid-stroke.
        const auto recache = [&](Trail& tr)
        {
            tr.mSize = scale_to_avatar ? llclamp(2.f * hz / 1.9f, 0.25f, 4.f) : 1.f;
            if (avatar_ok)
            {
                const LLVector3  rp  = av->getRenderPosition();
                const LLVector3* ext = av->getLastAnimExtents();
                // [TronT3 fix] Stale / zero animation extents (not yet
                // animated, off-screen) put ext[0] far from the avatar and the
                // offset would silently pin at the -3 m clamp. Out of range ->
                // fall back to the candidate AABB bottom, then 0.
                F32 off = (rp.isFinite() && ext[0].isFinite())
                    ? ext[0].mV[VZ] - rp.mV[VZ] : -100.f;
                if (!(off >= -3.f && off <= 1.f) && rp.isFinite())
                {
                    off = (cand.mCenter.mV[VZ] - hz) - rp.mV[VZ];
                }
                tr.mFootOffset = (off >= -3.f && off <= 1.f) ? off : 0.f;
            }
        };
        const auto base_pos = [&](const Trail& tr) -> LLVector3
        {
            // Avatar: stable render position + cached feet. Non-avatar (set 6
            // selected objects only): AABB bottom -- documented fallback, the
            // emitter then follows the animated extents.
            return avatar_ok
                ? av->getRenderPosition() + LLVector3(0.f, 0.f, tr.mFootOffset)
                : LLVector3(cand.mCenter.mV[VX], cand.mCenter.mV[VY], cand.mCenter.mV[VZ] - hz);
        };
        const auto emit = [&](Trail& tr)
        {
            if (tr.mNeedBreak || tr.mSegs.empty())
            {
                tr.mSegs.emplace_back();
                tr.mNeedBreak = false;
                recache(tr); // O-P2: recache ONLY when a segment opens
            }
            const LLVector3d pt_global = gAgent.getPosGlobalFromAgent(base_pos(tr));
            if (!pt_global.isFinite())
            {
                return;
            }
            tr.mSegs.back().mPts.push_back({ pt_global, now, tr.mSize });
            tr.mLastGlobal = pt_global;
            tr.mLastActivityClock = now;
            trail_cap(tr, max_segments, max_points);
        };

        if (fresh)
        {
            recache(t);
        }

        // Kinematics (step / dist_emit / speed) are always measured with the
        // PRE-recache offset; only the stored point (inside emit()) takes the
        // possibly-fresh one.
        const LLVector3 base_agent = base_pos(t);
        if (!base_agent.isFinite())
        {
            continue;
        }
        const LLVector3d g = gAgent.getPosGlobalFromAgent(base_agent);

        if (fresh)
        {
            t.mHaveLast  = true;
            t.mLastGlobal = g;
            t.mPrevGlobal = g;
            t.mPrevValid  = true;
            t.mSpeedEma   = 0.f;
            emit(t); // first point immediately (a 1-pt segment; drawn once a 2nd arrives)
            continue;
        }

        // ---- consecutive-sample kinematics (P1-2): speed + teleport from
        // the PREVIOUS SAMPLE, never from the last emission ----
        const F64 step  = t.mPrevValid ? (g - t.mPrevGlobal).length() : 0.0;
        const F32 speed = (F32)(step / llmax(pdt, 1.0e-3));
        const F32 ema_a = 1.f - expf(-(F32)pdt / 0.15f); // EMA, tau 0.15 s of presentation time

        if (step > (F64)break_distance)
        {
            t.mNeedBreak = true; // teleport (consecutive)
            // [TronT3 fix] a teleport jump is not motion: keep the spike
            // (e.g. 6000 m/s) out of the EMA so an actor standing still after
            // it starts idling immediately.
            t.mSpeedEma = 0.f;
        }
        else
        {
            t.mSpeedEma = t.mPrevValid ? (t.mSpeedEma + (speed - t.mSpeedEma) * ema_a) : speed;
        }

        // Spacing + resume use the last EMITTED point only.
        const F64 dist_emit = (g - t.mLastGlobal).length();
        if (t.mResumePending)
        {
            if (dist_emit > 0.5 * (F64)break_distance)
            {
                t.mNeedBreak = true;
            }
            t.mResumePending = false;
        }

        if (t.mSpeedEma < min_speed)
        {
            t.mIdleAccum += wall;
            if (t.mIdleAccum > (F64)idle_break)
            {
                t.mNeedBreak = true;
            }
        }
        else
        {
            t.mIdleAccum = 0.0;
        }

        if (dist_emit >= (F64)spacing && t.mSpeedEma >= min_speed)
        {
            emit(t); // emit() sets mLastGlobal
        }

        t.mPrevGlobal = g;
        t.mPrevValid  = true; // advance the consecutive-sample state every sampled frame
    }

    // --- 7: unseen aging / drop ----------------------------------------------
    for (auto it = sTrails.begin(); it != sTrails.end(); )
    {
        if (inset.find(it->first) == inset.end())
        {
            Trail& t = it->second;
            t.mUnseenAccum += wall;
            if (t.mUnseenAccum > 1.0)
            {
                t.mNeedBreak = true;
            }
            if (t.mUnseenAccum > 5.0 && t.mSegs.empty())
            {
                it = sTrails.erase(it);
                continue;
            }
        }
        ++it;
    }

    // --- 8 --------------------------------------------------------------------
    sHasGeometry = false;
    for (const auto& kv : sTrails)
    {
        for (const TrailSegment& seg : kv.second.mSegs)
        {
            if (seg.mPts.size() >= 2)
            {
                sHasGeometry = true;
                break;
            }
        }
        if (sHasGeometry)
        {
            break;
        }
    }
    llassert((S32)sTrails.size() <= max_actors);
}

// [TronT3] TronTrailEnabled && hasTrailGeometry(). Pure/cheap; callers AND it
// with isActiveForCurrentPass().
bool ALTron::trailsWanted()
{
    static LLCachedControl<bool> trail_enabled_ctrl(gSavedSettings, "TronTrailEnabled", false);
    return trail_enabled_ctrl() && hasTrailGeometry();
}

// [TronT3] At least one segment with >= 2 points after this frame's
// updateTrails().
bool ALTron::hasTrailGeometry()
{
    return sHasGeometry;
}

// [TronT3] Drops every stored point (button, scene load, master-off
// transition, TronTrailEnabled off, cut). Deliberately resets nothing else --
// the cut serial and sampling flags persist (T3 contract section 3.4).
void ALTron::clearTrails()
{
    sTrails.clear();
    sHasGeometry = false;
}

// [TronT3] Draws every trail into the CURRENTLY BOUND target (mWaterDis) --
// T3 contract section 4.4. Vertices are uploaded CAMERA-RELATIVE (double
// subtraction on the CPU) under a rotation-only modelview, exactly like
// renderTronWorld's own cam_rel idiom, so a multi-thousand-metre global
// coordinate never reaches float32 math directly.
void ALTron::renderTrails(const ALTron::TrailDrawParams& p)
{
    if (!gTronTrailProgram.isComplete() || !sHasGeometry)
    {
        return;
    }

    // --- matrices (O-P2: push/pop on BOTH stacks -- exact restore) ----------
    glm::mat4 mv_rot = glm::make_mat4(p.mModelview);
    mv_rot[3] = glm::vec4(0.f, 0.f, 0.f, 1.f); // translation column zeroed -> camera at the origin
    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.pushMatrix();
    gGL.loadMatrix(p.mProjection);
    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.pushMatrix();
    gGL.loadMatrix(glm::value_ptr(mv_rot));

    // --- GL state contract (T3 contract section 1.3) ------------------------
    LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
    LLGLEnable    blend(GL_BLEND);
    LLGLDisable   cull(GL_CULL_FACE);
    gGL.blendFunc(LLRender::BF_ONE, LLRender::BF_ONE, LLRender::BF_ONE, LLRender::BF_ONE_MINUS_SOURCE_ALPHA);

    gTronTrailProgram.bind();
    const S32 ech = gTronTrailProgram.enableTexture(LLShaderMgr::EXPOSURE_MAP);
    if (ech > -1)
    {
        gPipeline.mExposureMap.bindTexture(0, ech);
    }
    gTronTrailProgram.uniform1f(LLShaderMgr::EXPOSURE, p.mExposureSelector);

    // --- per-pass settings ---------------------------------------------------
    static LLCachedControl<S32> style_ctrl(gSavedSettings, "TronTrailStyle", 0);
    static LLCachedControl<F32> intensity_ctrl(gSavedSettings, "TronTrailIntensity", 8.f);
    static LLCachedControl<F32> height_ctrl(gSavedSettings, "TronTrailHeight", 1.2f);
    static LLCachedControl<F32> width_ctrl(gSavedSettings, "TronTrailWidth", 0.6f);
    static LLCachedControl<F32> lift_ctrl(gSavedSettings, "TronTrailLift", 0.03f);
    static LLCachedControl<F32> fade_time_ctrl(gSavedSettings, "TronTrailFadeTime", 4.f);
    static LLCachedControl<F32> fade_curve_ctrl(gSavedSettings, "TronTrailFadeCurve", 1.5f);
    static LLCachedControl<F32> edge_ctrl(gSavedSettings, "TronTrailEdge", 0.12f);
    static LLCachedControl<F32> edge_gain_ctrl(gSavedSettings, "TronTrailEdgeGain", 3.f);
    static LLCachedControl<F32> body_gain_ctrl(gSavedSettings, "TronTrailBodyGain", 0.35f);
    static LLCachedControl<F32> glow_ctrl(gSavedSettings, "TronTrailGlow", 1.f);
    static LLCachedControl<S32> color_mode_ctrl(gSavedSettings, "TronTrailColorMode", 1);
    static LLCachedControl<S32> fog_mode_ctrl(gSavedSettings, "TronTrailFog", 1);
    static LLCachedControl<F32> fog_density_ctrl(gSavedSettings, "TronTrailFogDensity", 0.01f);

    const S32 style       = std::clamp((S32)style_ctrl(), 0, 2);
    const F32 intensity   = llclamp(finite_or((F32)intensity_ctrl(), 8.f), 0.f, 64.f);
    const F32 height      = llclamp(finite_or((F32)height_ctrl(), 1.2f), 0.1f, 8.f);
    const F32 width       = llclamp(finite_or((F32)width_ctrl(), 0.6f), 0.05f, 4.f);
    const F32 lift        = llclamp(finite_or((F32)lift_ctrl(), 0.03f), -0.5f, 1.f);
    const F32 fade_time   = llclamp(finite_or((F32)fade_time_ctrl(), 4.f), 0.2f, 60.f);
    const F32 fade_curve  = llclamp(finite_or((F32)fade_curve_ctrl(), 1.5f), 0.25f, 4.f);
    const F32 edge        = llclamp(finite_or((F32)edge_ctrl(), 0.12f), 0.02f, 0.5f);
    const F32 edge_gain   = llclamp(finite_or((F32)edge_gain_ctrl(), 3.f), 0.f, 8.f);
    const F32 body_gain   = llclamp(finite_or((F32)body_gain_ctrl(), 0.35f), 0.f, 2.f);
    const F32 glow        = llclamp(finite_or((F32)glow_ctrl(), 1.f), 0.f, 1.f);
    const S32 color_mode  = std::clamp((S32)color_mode_ctrl(), 0, 2);
    const S32 fog_mode    = std::clamp((S32)fog_mode_ctrl(), 0, 1);
    const F32 fog_density = llclamp(finite_or((F32)fog_density_ctrl(), 0.01f), 0.f, 0.2f);

    gTronTrailProgram.uniform4f(LLShaderMgr::TRON_TRAIL_PARAMS, edge, edge_gain, body_gain, fade_curve);
    gTronTrailProgram.uniform4f(LLShaderMgr::TRON_TRAIL_PARAMS2, intensity, (F32)style,
        p.mWillExpose ? 1.f : p.mNoPostScale, p.mWillExpose ? 0.f : 1.f);
    gTronTrailProgram.uniform4f(LLShaderMgr::TRON_TRAIL_PARAMS3, (F32)fog_mode, fog_density, 0.f, 0.f);

    const ALTron::Palette pal = palette();
    const LLVector3 up(0.f, 0.f, 1.f);
    const F64 now = clock().mClockSeconds;

    for (const auto& kv : sTrails)
    {
        const Trail& t = kv.second;

        // --- colour (T3 contract section 3.5) -------------------------------
        LLColor3 rgb = pal.mPrimary;
        if (color_mode == 1)
        {
            switch (t.mPalette % 4)
            {
                case 1:  rgb = pal.mSecondary; break;
                case 2:  rgb = pal.mAccent;    break;
                case 3:  rgb = pal.mPulse;     break;
                default: rgb = pal.mPrimary;   break;
            }
        }
        else if (color_mode == 2)
        {
            rgb = pal.mSecondary;
        }
        gTronTrailProgram.uniform4f(LLShaderMgr::TRON_TRAIL_COLOR, rgb.mV[0], rgb.mV[1], rgb.mV[2], glow);

        for (const TrailSegment& seg : t.mSegs)
        {
            const size_t n = seg.mPts.size();
            if (n < 2)
            {
                continue; // never drawn until a 2nd point arrives
            }

            const auto rel_at = [&](size_t i) -> LLVector3
            {
                const LLVector3d& g = seg.mPts[i].mGlobal;
                return LLVector3(
                    (F32)(g.mdV[VX] - p.mCamGlobal.mdV[VX]),
                    (F32)(g.mdV[VY] - p.mCamGlobal.mdV[VY]),
                    (F32)(g.mdV[VZ] - p.mCamGlobal.mdV[VZ]));
            };

            LLVector3 prev_tangent(1.f, 0.f, 0.f);
            LLVector3 prev_side(1.f, 0.f, 0.f);
            bool have_side = false;

            size_t idx = 0;
            while (idx < n - 1)
            {
                const size_t chunk_last = llmin(idx + 999, n - 1); // <=1000 pts/chunk (2000 verts < the 4094 limit)
                bool strip_open = false;

                for (size_t i = idx; i <= chunk_last; ++i)
                {
                    LLVector3 tangent;
                    if (i == 0)
                    {
                        tangent = trail_safe_unit(rel_at(1) - rel_at(0), prev_tangent);
                    }
                    else if (i == n - 1)
                    {
                        tangent = trail_safe_unit(rel_at(i) - rel_at(i - 1), prev_tangent);
                    }
                    else
                    {
                        tangent = trail_safe_unit(rel_at(i + 1) - rel_at(i - 1), prev_tangent);
                    }
                    prev_tangent = tangent;

                    LLVector3 side_fallback = prev_side;
                    if (!have_side)
                    {
                        side_fallback = trail_safe_unit(tangent % LLVector3(0.f, 1.f, 0.f), LLVector3(1.f, 0.f, 0.f));
                        have_side = true;
                    }
                    const LLVector3 ground_side = trail_safe_unit(tangent % up, side_fallback);
                    prev_side = ground_side;

                    const LLVector3 rel = rel_at(i);
                    const F32 pt_size = seg.mPts[i].mSize;
                    const F32 h = height * pt_size;
                    const F32 w = width * pt_size;

                    LLVector3 a, b;
                    if (style == 0) // wall
                    {
                        a = rel + up * lift;
                        b = a + up * h;
                    }
                    else if (style == 1) // ground streak
                    {
                        const LLVector3 base = rel + up * lift;
                        a = base - ground_side * (w * 0.5f);
                        b = base + ground_side * (w * 0.5f);
                    }
                    else // 2: camera-facing ribbon
                    {
                        const LLVector3 cc = rel + up * (lift + 0.5f * h);
                        const LLVector3 view = trail_safe_unit(-cc, LLVector3(0.f, 0.f, -1.f));
                        const LLVector3 side = trail_safe_unit(tangent % view, ground_side);
                        a = cc - side * (w * 0.5f);
                        b = cc + side * (w * 0.5f);
                    }

                    if (!a.isFinite() || !b.isFinite())
                    {
                        if (strip_open)
                        {
                            gGL.end();
                            strip_open = false;
                        }
                        continue; // strip ends; a new begin starts at the next finite pair
                    }
                    if (!strip_open)
                    {
                        gGL.begin(LLRender::TRIANGLE_STRIP);
                        strip_open = true;
                    }

                    const F32 fade01 = (F32)llclamp(
                        1.0 - (now - seg.mPts[i].mEmitClock) / (F64)fade_time,
                        0.0, 1.0);
                    gGL.texCoord2f(fade01, 0.f);
                    gGL.vertex3fv(a.mV);
                    gGL.texCoord2f(fade01, 1.f);
                    gGL.vertex3fv(b.mV);
                }

                if (strip_open)
                {
                    gGL.end();
                }
                if (chunk_last >= n - 1)
                {
                    break;
                }
                idx = chunk_last; // next chunk starts at the last point of this chunk (repeats its vertex pair)
            }
        }
    }

    if (ech > -1)
    {
        gTronTrailProgram.disableTexture(LLShaderMgr::EXPOSURE_MAP);
    }
    LLGLSLShader::unbind();

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.popMatrix();
    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.popMatrix();
    // [TronT3 fix] leave MODELVIEW selected (section 4.4): later callers such
    // as renderFocusPoint() push/translate without selecting a mode.
    gGL.matrixMode(LLRender::MM_MODELVIEW);
    // [TronT3] CANONICAL reassert (path-guides idiom), NOT a restore of the
    // incoming blend func -- renderFinalize's later consumers set their own.
    gGL.setSceneBlendType(LLRender::BT_ALPHA);
}

// [TronT1] Applies one of the 8 T1 look presets -- see kLookPresets above
// and the T1 contract section 8. Common column first, then the row's own
// overrides, then the shared palette, then (only when the matching
// TronPresetAlso* flag is on) the linked Roto Ink / Rig Rim preset -- but
// the linked key/label/mode is ALWAYS remembered (lastPresetRotoKey() etc.)
// so the Lightbox Integration group's buttons can apply it later even if
// the flag was off at the time.
void ALTron::applyPreset(const std::string& key)
{
    const LookPreset* row = nullptr;
    for (const LookPreset& p : kLookPresets)
    {
        if (key == p.mKey)
        {
            row = &p;
            break;
        }
    }
    if (!row)
    {
        return; // unknown preset key: leave settings untouched
    }

    // Common column (T1 contract section 8; T2 contract section 8.1 moves
    // TronGradeStrength/TronGridEnabled/TronGridWall/TronGridCeiling/
    // TronGridWaterMode out of this common column and into per-row fields --
    // see the per-row overrides below).
    gSavedSettings.setF32("TronGradeCrush", 0.05f);
    gSavedSettings.setF32("TronGradeKeepBrightLo", 0.8f);
    gSavedSettings.setF32("TronGradeKeepBrightHi", 2.5f);
    gSavedSettings.setF32("TronGradeKeepSubject", 0.5f);
    gSavedSettings.setF32("TronGradeSkyDarken", 0.f);
    gSavedSettings.setF32("TronGridIntensity", 6.f);
    gSavedSettings.setF32("TronGridMajorWidth", 2.f);
    gSavedSettings.setF32("TronGridMajorIntensity", 1.5f);
    // [TronT3] TronGridFarFade moves out of this common column and into a
    // per-row field (row->mFarFade) -- the lightcycle look needs 400 m
    // instead of the shared 120 m (contract section 7.1).
    gSavedSettings.setF32("TronGridFloor", 1.f);
    gSavedSettings.setF32("TronGridSharpness", 8.f);
    gSavedSettings.setS32("TronGridNormalSource", 1);
    gSavedSettings.setF32("TronGridGlow", 1.f);
    gSavedSettings.setF32("TronGridWaterTolerance", 0.03f);
    gSavedSettings.setF32("TronPulseGridLength", 0.25f);
    // [TronT1 P2-5 fix] Contract section 8 common column -- was missing
    // entirely, so a look preset never reset a previously hand-tuned seed
    // back to the documented default.
    gSavedSettings.setF32("TronPulseSeed", 0.f);
    gSavedSettings.setF32("TronPulseAmount", 0.35f);
    gSavedSettings.setS32("TronPulseShape", 0);
    gSavedSettings.setF32("TronPulseRate", 0.8f);
    gSavedSettings.setF32("CineRigRimTronMix", 1.f);
    gSavedSettings.setS32("CineRigRimTronColorSource", 0);
    // [TronT2] Common column additions (contract section 8.1).
    gSavedSettings.setF32("TronTracePadRadius", 0.03f);
    gSavedSettings.setF32("TronTraceDiagonal", 0.25f);
    gSavedSettings.setF32("TronTracePulseAmount", 1.0f);
    gSavedSettings.setF32("TronTracePulseLength", 0.3f);
    gSavedSettings.setF32("TronTraceSeed", 0.f);
    gSavedSettings.setF32("TronRimSilhouetteGain", 1.0f);
    gSavedSettings.setF32("TronRimSilhouetteThreshold", 0.03f);
    gSavedSettings.setF32("TronRimScanSpeed", 0.5f);
    gSavedSettings.setF32("TronRimScanWidth", 0.08f);
    gSavedSettings.setBOOL("TronRimRejectFloors", true);
    // [TronT3] Common column additions (contract section 7.1).
    gSavedSettings.setBOOL("TronTrailScaleToAvatar", true);
    gSavedSettings.setF32("TronTrailFadeCurve", 1.5f);
    gSavedSettings.setF32("TronTrailEdge", 0.12f);
    gSavedSettings.setF32("TronTrailEdgeGain", 3.f);
    gSavedSettings.setF32("TronTrailGlow", 1.f);
    gSavedSettings.setS32("TronTrailFog", 1);
    gSavedSettings.setF32("TronTrailFogDensity", 0.01f);

    // Per-row overrides.
    gSavedSettings.setF32("TronGradeDarkenEV", row->mDarkenEV);
    gSavedSettings.setF32("TronGradeDesaturate", row->mDesaturate);
    gSavedSettings.setUntypedValue("TronGradeTint",
        LLColor3(row->mTintR, row->mTintG, row->mTintB).getValue());
    gSavedSettings.setF32("TronGradeTintAmount", row->mTintAmount);
    gSavedSettings.setF32("TronGridSpacing", row->mSpacing);
    gSavedSettings.setF32("TronGridWidth", row->mWidth);
    gSavedSettings.setF32("TronGridMinWidthPx", row->mMinWidthPx);
    gSavedSettings.setS32("TronGridMajorEvery", row->mMajorEvery);
    // [TronT2] Now per-row (contract section 8.1).
    gSavedSettings.setF32("TronGradeStrength", row->mGradeStrength);
    gSavedSettings.setBOOL("TronGridEnabled", row->mGridEnabled);
    gSavedSettings.setF32("TronGridWall", row->mGridWall);
    gSavedSettings.setF32("TronGridCeiling", row->mGridCeiling);
    gSavedSettings.setS32("TronGridWaterMode", row->mWaterMode);
    gSavedSettings.setF32("TronPulseGridAmount", row->mPulseGridAmount);
    gSavedSettings.setF32("TronPulseGridSpeed", row->mPulseSpeed);
    gSavedSettings.setF32("TronPulseGridWavelength", row->mPulseWavelength);
    gSavedSettings.setF32("TronPulseGridDensity", row->mPulseDensity);
    gSavedSettings.setS32("TronPulseGridDirection", row->mPulseDirection);
    gSavedSettings.setS32("TronPulseGridColorMode", row->mPulseColorMode);
    gSavedSettings.setS32("TronPulseShape", row->mPulseShape);
    gSavedSettings.setF32("TronPulseRate", row->mPulseRate);
    // [TronT2] Circuit traces.
    gSavedSettings.setBOOL("TronTraceEnabled", row->mTraceEnabled);
    gSavedSettings.setF32("TronTraceIntensity", row->mTraceIntensity);
    gSavedSettings.setF32("TronTraceCell", row->mTraceCell);
    gSavedSettings.setF32("TronTraceWidth", row->mTraceWidth);
    gSavedSettings.setF32("TronTraceDensity", row->mTraceDensity);
    gSavedSettings.setF32("TronTraceWalls", row->mTraceWalls);
    gSavedSettings.setF32("TronTraceFloors", row->mTraceFloors);
    gSavedSettings.setF32("TronTracePulseSpeed", row->mTracePulseSpeed);
    // [TronT2] Post neon rim.
    gSavedSettings.setBOOL("TronRimEnabled", row->mRimEnabled);
    gSavedSettings.setF32("TronRimGain", row->mRimGain);
    gSavedSettings.setF32("TronRimExponent", row->mRimExponent);
    gSavedSettings.setF32("TronRimPulseAmount", row->mRimPulseAmount);
    gSavedSettings.setF32("TronRimScanAmount", row->mRimScanAmount);
    gSavedSettings.setS32("TronRimColorMode", row->mRimColorMode);
    // [TronT3] Now per-row (contract section 7.1: was the common column's
    // static 120.f write above).
    gSavedSettings.setF32("TronGridFarFade", row->mFarFade);
    // [TronT3] Light-cycle trails.
    gSavedSettings.setBOOL("TronTrailEnabled", row->mTrailEnabled);
    gSavedSettings.setS32("TronTrailStyle", row->mTrailStyle);
    gSavedSettings.setF32("TronTrailIntensity", row->mTrailIntensity);
    gSavedSettings.setF32("TronTrailHeight", row->mTrailHeight);
    gSavedSettings.setF32("TronTrailWidth", row->mTrailWidth);
    gSavedSettings.setF32("TronTrailFadeTime", row->mTrailFadeTime);
    gSavedSettings.setF32("TronTrailBodyGain", row->mTrailBodyGain);
    gSavedSettings.setS32("TronTrailColorMode", row->mTrailColorMode);

    // [TronT1 P2-5 fix] applyPalettePreset() only writes the four colour
    // swatches, never TronPalette itself -- without this, the palette combo
    // would keep showing whatever it last displayed (often "Custom") even
    // though the swatches now match a named palette exactly.
    gSavedSettings.setS32("TronPalette", row->mPaletteId);
    applyPalettePreset(row->mPaletteId);

    sLastPresetRotoKey = row->mRotoKey;
    sLastPresetRigRimLabel = row->mRigRimLabel;
    sLastPresetRigRimTronMode = row->mRigRimTronMode;

    // CineRigRimTronMode is a LOOK key (written only by a Tron preset, never
    // by a Rig Rim preset) -- write it now regardless of TronPresetAlsoRigRim
    // (which only gates whether the Rig Rim *preset row* itself -- master/
    // soften/per-light gains -- is also applied).
    gSavedSettings.setS32("CineRigRimTronMode", row->mRigRimTronMode);

    static LLCachedControl<bool> also_roto(gSavedSettings, "TronPresetAlsoRoto", true);
    if (also_roto())
    {
        ALRotoInk::applyPreset(row->mRotoKey);
    }
    static LLCachedControl<bool> also_rigrim(gSavedSettings, "TronPresetAlsoRigRim", true);
    if (also_rigrim())
    {
        ALPanelCineLightRig::applyRigRimPresetByName(row->mRigRimLabel);
    }
}

// [TronT1]
const std::string& ALTron::lastPresetRotoKey()
{
    return sLastPresetRotoKey;
}

// [TronT1]
const std::string& ALTron::lastPresetRigRimLabel()
{
    return sLastPresetRigRimLabel;
}

// [TronT1]
S32 ALTron::lastPresetRigRimTronMode()
{
    return sLastPresetRigRimTronMode;
}

// [TronT0]
void ALTron::latchFrame()
{
    const LLTemporalFrameContext& frame = LLPresentationTime::currentFrame();

    static LLCachedControl<F32>  anim_speed_ctrl(gSavedSettings, "TronAnimSpeed", 1.f);
    static LLCachedControl<bool> anim_pause_ctrl(gSavedSettings, "TronAnimPause", false);
    static LLCachedControl<bool> anim_follow_ctrl(gSavedSettings, "TronAnimFollowDirector", false);
    static LLCachedControl<F32>  pulse_rate_ctrl(gSavedSettings, "TronPulseRate", 0.8f);

    F32 rate = anim_pause_ctrl() ? 0.f
        : llclamp(finite_or((F32)anim_speed_ctrl(), 1.f), 0.f, 8.f);
    if (anim_follow_ctrl())
    {
        // [TronT0] Same key/clamp/fallback ALRotoInk's clock uses
        // (pipeline.cpp's mRotoAnimClock advance) for
        // CineOutlineAnimFollowDirector, so Director slow-motion/scrub
        // scales both clocks identically.
        static LLCachedControl<F32> director_time_speed(gSavedSettings, "FlycamOperatorTimeSpeed", 1.f);
        rate *= llclamp(finite_or((F32)director_time_speed(), 1.f), 0.f, 8.f);
    }

    // presentation_delta is already clamped to >= 0 by LLPresentationTime;
    // re-clamp defensively and cap a stall/hitch at 1 s, same as the Roto
    // Ink clock, so a loading-screen freeze can't leap the pulse/phase
    // forward once it resumes.
    const F64 dt = llclamp(finite_or(frame.presentation_delta, 0.0), 0.0, 1.0);

    sClock.mClockDelta = dt * (F64)rate;
    sClock.mClockSeconds += sClock.mClockDelta;
    sClock.mWallDelta = finite_or(frame.wall_delta, 0.0);

    const F32 pulse_rate = llclamp(finite_or((F32)pulse_rate_ctrl(), 0.8f), 0.05f, 8.f);
    // [TronT0] Accumulate in F64 (persistent) so tiny per-frame increments at
    // very low speeds are never lost to F32 rounding; publish as F32.
    static F64 sPulsePhase64 = 0.0;
    F64 phase = std::fmod(sPulsePhase64 + sClock.mClockDelta * (F64)pulse_rate, 1.0);
    if (!(phase >= 0.0)) // negative or NaN
    {
        phase = (phase < 0.0) ? phase + 1.0 : 0.0;
    }
    sPulsePhase64 = phase;
    sClock.mPulsePhase01 = (F32)phase;

    update_anchor();
}

// [TronT0]
const ALTron::FrameClock& ALTron::clock()
{
    return sClock;
}

// [TronT0]
const ALTron::Anchor& ALTron::anchor()
{
    return sAnchor;
}

// [TronT1]
ALTron::ELayer ALTron::layer()
{
    static LLCachedControl<S32> layer_ctrl(gSavedSettings, "TronLayer", (S32)LAYER_CAMERA);
    return std::clamp((S32)layer_ctrl(), (S32)LAYER_SCENE, (S32)LAYER_CAMERA) == LAYER_SCENE
        ? LAYER_SCENE : LAYER_CAMERA;
}

// [TronT0]
ALTron::LatticeFrame ALTron::computeLattice(F64 scale)
{
    return computeLattice(scale, sAnchor.mAnchorGlobal);
}

// [TronT1] Same body as the T0 single-argument overload, but the anchor is
// now a parameter (see the header comment / T1 contract section 3.3):
// renderTronWorld derives its OWN anchor from the pass's live camera matrix
// and must fold that exact anchor into every lattice frame it asks for,
// never the idle()-latched sAnchor (which lags the camera by one frame).
ALTron::LatticeFrame ALTron::computeLattice(F64 scale, const LLVector3d& anchor_global)
{
    LatticeFrame out;
    out.mR = LLVector3(0.f, 0.f, 0.f);
    out.mK[0] = out.mK[1] = out.mK[2] = 0;

    if (!(scale > 0.0) || !std::isfinite(scale))
    {
        return out;
    }

    LLVector3d origin(LLVector3d::zero);
    {
        static LLCachedControl<LLVector3d> origin_ctrl(gSavedSettings, "TronGridOriginGlobal", LLVector3d::zero);
        const LLVector3d raw = origin_ctrl();
        if (raw.isFinite())
        {
            origin = raw;
        }
    }

    const LLVector3d anchor = anchor_global.isFinite() ? anchor_global : LLVector3d::zero;
    const F64 base_x = anchor.mdV[VX] - origin.mdV[VX];
    const F64 base_y = anchor.mdV[VY] - origin.mdV[VY];
    const F64 base_z = anchor.mdV[VZ] - origin.mdV[VZ];

    // [TronT0] k = floor((A - O) / s), r = (A - O) - k * s -- subtraction,
    // never fmod (fmod goes negative for a negative dividend, which would
    // put r outside [0, s)). Verified against design v4 section 2.3's two
    // worked examples.
    const F64 kx = std::floor(base_x / scale);
    const F64 ky = std::floor(base_y / scale);
    const F64 kz = std::floor(base_z / scale);

    out.mR = LLVector3(
        (F32)(base_x - kx * scale),
        (F32)(base_y - ky * scale),
        (F32)(base_z - kz * scale));

    static constexpr F64 kTwoPow32 = 4294967296.0;
    const auto wrap_u32 = [](F64 k) -> U32
    {
        F64 k_mod = std::fmod(k, kTwoPow32);
        if (k_mod < 0.0)
        {
            k_mod += kTwoPow32;
        }
        return (U32)k_mod;
    };
    out.mK[0] = wrap_u32(kx);
    out.mK[1] = wrap_u32(ky);
    out.mK[2] = wrap_u32(kz);

    return out;
}

// [TronT0]
// [TronT1 P2-10 fix] Was a straight copy of kAllSettings, which meant the
// Director scene list carried TronPresetAlsoRoto/TronPresetAlsoRigRim too --
// pure Lightbox UI checkbox state, not a shot or a look setting -- so
// loading a scene could silently flip either checkbox. Filters those two
// out; resetToDefaults() below still walks the full kAllSettings, so
// "Reset all" is unaffected.
const std::vector<std::string>& ALTron::settings()
{
    static const std::vector<std::string> s_settings = []
    {
        std::vector<std::string> v(std::begin(kAllSettings), std::end(kAllSettings));
        v.erase(std::remove_if(v.begin(), v.end(), [](const std::string& name)
            {
                for (const char* excluded : kDirectorExcluded)
                {
                    if (name == excluded)
                    {
                        return true;
                    }
                }
                return false;
            }), v.end());
        return v;
    }();
    return s_settings;
}

// [TronT0]
void ALTron::resetToDefaults()
{
    for (const char* name : kAllSettings)
    {
        if (LLControlVariable* control = gSavedSettings.getControl(name))
        {
            control->resetToDefault(true);
        }
    }
}
