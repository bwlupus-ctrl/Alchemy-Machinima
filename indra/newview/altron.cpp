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

#include "llagent.h"
#include "llgl.h"
#include "llpresentationtime.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "pipeline.h"

#include <cmath>

extern bool gCubeSnapshot;
extern bool gSnapshotNoPost;

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

// [TronT0] Named palette table (TronPalette combo ids 1..10, design v1
// section 8). Primary/secondary pairs come straight from the design's
// palette list; accent/pulse are not separately specified per named palette
// beyond id 1 there, so this table uses a white accent and a
// secondary-tinted pulse as a reasonable placeholder for the other nine --
// revisit when the palette swatches land on the Tron tab (T1).
struct NamedPalette
{
    S32 mId;
    F32 mPr, mPg, mPb;
    F32 mSr, mSg, mSb;
    F32 mAr, mAg, mAb;
    F32 mUr, mUg, mUb;
};

const NamedPalette kPalettes[] = {
    // id  primary                secondary              accent      pulse
    {  1, 0.15f,0.90f,1.00f,      1.00f,0.45f,0.05f,      1,1,1,      0.60f,1.00f,1.00f }, // Legacy Cyan
    {  2, 1.00f,0.08f,0.06f,      1.00f,1.00f,1.00f,      1,1,1,      1.00f,0.60f,0.50f }, // Ares Red
    {  3, 0.55f,0.85f,1.00f,      1.00f,0.90f,0.50f,      1,1,1,      1.00f,0.90f,0.60f }, // Classic 1982
    {  4, 1.00f,0.55f,0.05f,      1.00f,0.08f,0.06f,      1,1,1,      1.00f,0.60f,0.20f }, // Recognizer Orange
    {  5, 1.00f,1.00f,1.00f,      0.15f,0.90f,1.00f,      1,1,1,      0.20f,0.90f,1.00f }, // Uprising White
    {  6, 1.00f,0.55f,0.05f,      1.00f,0.90f,0.50f,      1,1,1,      1.00f,0.70f,0.30f }, // Rinzler
    {  7, 1.00f,1.00f,1.00f,      1.00f,1.00f,1.00f,      1,1,1,      0.80f,0.90f,1.00f }, // Siren White
    {  8, 1.00f,1.00f,1.00f,      0.15f,0.90f,1.00f,      1,1,1,      0.60f,1.00f,1.00f }, // Quorra
    {  9, 1.00f,0.75f,0.20f,      1.00f,0.45f,0.05f,      1,1,1,      1.00f,0.80f,0.40f }, // Clu Gold
    { 10, 0.15f,0.90f,1.00f,      0.10f,0.30f,1.00f,      1,1,1,      0.30f,0.70f,1.00f }, // Grid Blue
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
};

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

// [TronT0]
bool ALTron::isEnabled()
{
    static LLCachedControl<bool> enabled(gSavedSettings, "TronEnabled", false);
    return enabled();
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

// [TronT0]
ALTron::LatticeFrame ALTron::computeLattice(F64 scale)
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

    const F64 base_x = sAnchor.mAnchorGlobal.mdV[VX] - origin.mdV[VX];
    const F64 base_y = sAnchor.mAnchorGlobal.mdV[VY] - origin.mdV[VY];
    const F64 base_z = sAnchor.mAnchorGlobal.mdV[VZ] - origin.mdV[VZ];

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
const std::vector<std::string>& ALTron::settings()
{
    static const std::vector<std::string> s_settings(
        std::begin(kAllSettings), std::end(kAllSettings));
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
