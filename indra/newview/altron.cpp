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

#include "alpanelcinelightrig.h"
#include "alrotoink.h"
#include "llagent.h"
#include "llgl.h"
#include "llpresentationtime.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewershadermgr.h" // [TronT1] gTronWorldProgram (resolveFrame() draw-eligibility check)
#include "pipeline.h"

#include <cmath>

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
    F32   mPulseGridAmount;
    F32   mPulseSpeed, mPulseWavelength, mPulseDensity;
    S32   mPulseDirection, mPulseColorMode;
    S32   mPulseShape;
    F32   mPulseRate;
    const char* mRotoKey;
    const char* mRigRimLabel;
    S32   mRigRimTronMode;
};

// id/key mirrors kPalettes; see the T1 contract section 8 for the source
// table (common column + this per-row override table).
const LookPreset kLookPresets[] = {
    // key           pal  EV    Desat  TintR  TintG  TintB  TintAmt  Sp    W      MinPx  Maj  PulseAmt  Speed  Wave  Dens  Dir Col  Shape Rate  RotoKey          RigRimLabel                     Mode
    { "legacy",       1, 3.0f, 0.6f,  0.55f, 0.75f, 1.00f, 0.30f,  2.0f, 0.020f, 1.2f,  4,   1.5f,     6.0f, 24.0f, 0.35f, 3,  0,  0,  0.8f, "tron_legacy",      "Tron Suit Kick",              2 },
    { "ares",         2, 3.5f, 0.7f,  1.00f, 0.55f, 0.45f, 0.25f,  3.0f, 0.030f, 1.2f,  3,   1.5f,    10.0f, 24.0f, 0.40f, 1,  0,  2,  1.2f, "tron_ares",        "Ares Red Backlight",          2 },
    { "classic82",    3, 4.0f, 0.9f,  1.00f, 1.00f, 1.00f, 0.00f,  4.0f, 0.040f, 1.2f,  0,   1.5f,     3.0f, 32.0f, 0.30f, 0,  1,  3,  0.5f, "tron_1982",        "Sci-Fi Hologram",             1 },
    { "recognizer",   4, 3.0f, 0.5f,  1.00f, 0.80f, 0.60f, 0.20f,  8.0f, 0.050f, 1.2f,  2,   1.5f,    12.0f, 48.0f, 0.70f, 3,  0,  0,  0.8f, "tron_recognizer",  "Strong Backlight",            1 },
    { "uprising",     5, 2.5f, 0.4f,  0.70f, 0.85f, 1.00f, 0.20f,  1.0f, 0.012f, 1.2f,  8,   1.5f,     4.0f, 16.0f, 0.15f, 3,  0,  0,  0.8f, "tron_uprising",    "Silhouette Glow (any angle)", 1 },
    { "rinzler",      6, 3.5f, 0.7f,  1.00f, 1.00f, 1.00f, 0.00f,  2.0f, 0.020f, 1.2f,  4,   1.5f,     6.0f, 24.0f, 0.35f, 3,  0,  0,  0.8f, "tron_recognizer",  "Noir Kicker",                 2 },
    { "siren",        7, 2.0f, 0.3f,  0.75f, 0.85f, 1.00f, 0.35f,  0.5f, 0.008f, 1.0f,  4,   0.0f,     6.0f, 24.0f, 0.35f, 3,  0,  0,  0.8f, "tron_uprising",    "Dreamy Halo",                 1 },
    { "quorra",       8, 3.0f, 0.5f,  0.55f, 0.75f, 1.00f, 0.30f,  2.0f, 0.020f, 1.2f,  4,   1.5f,     6.0f, 24.0f, 0.35f, 3,  1,  0,  0.8f, "tron_legacy",      "Fashion Edge",                2 },
};

std::string sLastPresetRotoKey;
std::string sLastPresetRigRimLabel;
S32         sLastPresetRigRimTronMode = 0;

ALTron::FrameClock sClock;
ALTron::Anchor     sAnchor;

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

// [TronT1] T0's isEnabled() was the raw master read; T1 extends it with the
// grade/grid OR (design section 4) so isActiveForCurrentPass() -- and every
// caller that gates real render work on it -- returns false whenever there
// is nothing to draw, without needing its own redundant grade/grid check.
bool ALTron::isEnabled()
{
    if (!isEnabledMaster())
    {
        return false;
    }
    static LLCachedControl<F32>  grade_strength(gSavedSettings, "TronGradeStrength", 0.85f);
    static LLCachedControl<bool> grid_enabled(gSavedSettings, "TronGridEnabled", true);
    return finite_or((F32)grade_strength(), 0.85f) > 0.f || grid_enabled();
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
    const bool will_run = isActiveForCurrentPass() && layer() == LAYER_CAMERA &&
        gTronWorldProgram.isComplete() && gPipeline.mWaterDis.isComplete();
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

    // Common column (T1 contract section 8).
    gSavedSettings.setF32("TronGradeStrength", 0.85f);
    gSavedSettings.setF32("TronGradeCrush", 0.05f);
    gSavedSettings.setF32("TronGradeKeepBrightLo", 0.8f);
    gSavedSettings.setF32("TronGradeKeepBrightHi", 2.5f);
    gSavedSettings.setF32("TronGradeKeepSubject", 0.5f);
    gSavedSettings.setF32("TronGradeSkyDarken", 0.f);
    gSavedSettings.setBOOL("TronGridEnabled", true);
    gSavedSettings.setF32("TronGridIntensity", 6.f);
    gSavedSettings.setF32("TronGridMajorWidth", 2.f);
    gSavedSettings.setF32("TronGridMajorIntensity", 1.5f);
    gSavedSettings.setF32("TronGridFarFade", 120.f);
    gSavedSettings.setF32("TronGridFloor", 1.f);
    gSavedSettings.setF32("TronGridWall", 0.6f);
    gSavedSettings.setF32("TronGridCeiling", 0.3f);
    gSavedSettings.setF32("TronGridSharpness", 8.f);
    gSavedSettings.setS32("TronGridNormalSource", 1);
    gSavedSettings.setF32("TronGridGlow", 1.f);
    gSavedSettings.setS32("TronGridWaterMode", 0);
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
    gSavedSettings.setF32("TronPulseGridAmount", row->mPulseGridAmount);
    gSavedSettings.setF32("TronPulseGridSpeed", row->mPulseSpeed);
    gSavedSettings.setF32("TronPulseGridWavelength", row->mPulseWavelength);
    gSavedSettings.setF32("TronPulseGridDensity", row->mPulseDensity);
    gSavedSettings.setS32("TronPulseGridDirection", row->mPulseDirection);
    gSavedSettings.setS32("TronPulseGridColorMode", row->mPulseColorMode);
    gSavedSettings.setS32("TronPulseShape", row->mPulseShape);
    gSavedSettings.setF32("TronPulseRate", row->mPulseRate);

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
