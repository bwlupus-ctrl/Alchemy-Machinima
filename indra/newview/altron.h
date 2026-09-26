/**
 * @file altron.h
 * @brief [TronT0] Tron effects package: shared clock, world anchor, palette
 * and settings-list foundation.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * [TronT0] Phase T0 ("foundation") of the Tron effects design. This module
 * owns the pieces every later Tron phase (T1 Tron World grade/grid, T2
 * circuit traces + Actor FX Tron Suit, T3 light-cycle trails) shares:
 *
 *   - the master enable gate and the shared linear-RGB palette;
 *   - one unwrapped, presentation-time-driven clock plus its own pulse-phase
 *     accumulator, latched exactly once per main-loop iteration so every
 *     consumer within a frame agrees on "now";
 *   - a 1024 m world anchor (so a per-pixel world position never needs to
 *     carry a multi-thousand-metre global coordinate into float32 math) and
 *     a per-pattern-scale global lattice frame for pop-free, session-stable
 *     hashing of grid lines / trace cells across region crossings;
 *   - the Tron* settings list, the single source of truth for
 *     LLFloaterDirector::sceneSettingsList() and the Lightbox tab's
 *     "Reset all" (mirrors ALRotoInk::settings()/resetToDefaults()).
 *
 * Default OFF (TronEnabled = false). T0 adds no render pass and no shader:
 * with the master off the ONLY thing that runs is latchFrame()'s cheap
 * per-frame CPU bookkeeping (no GL, no visual effect, no settings writes).
 * Grade/grid/circuit-trace/neon-rim/trail shader work, the Rig Rim Tron
 * tint hook, the Lightbox "Tron" tab and every look preset are later phases
 * (T1+); see the design doc's phase table.
 */

#pragma once

#ifndef AL_TRON_H
#define AL_TRON_H

#include <string>
#include <vector>

#include "stdtypes.h"
#include "v3color.h"
#include "v3dmath.h"
#include "v3math.h"

namespace ALTron
{
    // Master gate: TronEnabled, a pure settings read (no GL). T0 wires only
    // the master switch; every sub-effect that will later OR into this check
    // (grade/grid/traces/rim/trails) lands in T1-T3.
    bool isEnabled();

    // isEnabled() && the current render context is one Tron may draw into:
    // not a reflection/irradiance probe capture (gCubeSnapshot), not a
    // "no post" snapshot (gSnapshotNoPost), not HUDs/impostors/Prism VCam
    // auxiliary views, and the HDR post chain is running this frame (the
    // same predicate LLPipeline::shouldRunRotoInkAt uses). RENDER-THREAD
    // ONLY. Nothing calls this yet in T0 -- it exists so T1's Tron World
    // pass gate and T3's trail gate share one definition from day one.
    bool isActiveForCurrentPass();

    // One shared colour palette (linear RGB), read from settings.
    struct Palette
    {
        LLColor3 mPrimary;
        LLColor3 mSecondary;
        LLColor3 mAccent;
        LLColor3 mPulse;
    };
    Palette palette();

    // Writes TronColorPrimary/Secondary/Accent/Pulse for one of the named
    // palettes (TronPalette combo ids 1..10, e.g. 1 = Legacy Cyan). Id 0
    // (Custom) or an unknown id is a no-op: the existing colours are left
    // untouched.
    void applyPalettePreset(S32 id);

    // Shared, unwrapped Tron clock. Latched exactly ONCE per main-loop
    // iteration (see latchFrame()) so every consumer within the same frame
    // (Rig Rim tint, Actor FX Tron Suit, the Tron World pass, light-cycle
    // trails -- all later phases) reads the identical value regardless of
    // call order within that frame.
    struct FrameClock
    {
        F64 mClockSeconds = 0.0;   // unwrapped accumulated seconds; never fmod'd on the CPU side
        F64 mClockDelta   = 0.0;   // this latch's contribution (already rate- and pause-scaled)
        F64 mWallDelta    = 0.0;   // unscaled presentation wall delta (future trail-sampling logic)
        F32 mPulsePhase01 = 0.f;   // 0..1 wrapped pulse-waveform phase; its own accumulator so
                                   // TronPulseRate changes never jump it
    };

    // Advances the shared clock and the pulse-phase accumulator, and
    // re-snaps the world anchor (see anchor()). Call exactly once per
    // main-loop iteration, immediately after
    // LLPresentationTime::instance().tick() (llappviewer.cpp) -- before any
    // consumer reads clock()/anchor() this frame. Safe to call even when
    // TronEnabled is off: cheap CPU-only bookkeeping, no GL, no settings
    // writes, and every later phase gates its own visible work on
    // isEnabled()/isActiveForCurrentPass() separately.
    void latchFrame();

    // The clock frozen by the most recent latchFrame() call.
    const FrameClock& clock();

    // World anchor: the 1024 m global lattice cell containing the camera,
    // re-snapped once per latchFrame(). Consumers reconstruct a small,
    // camera-relative world position as cam_rel + R * p_view (R = the
    // pass's own inverse-modelview rotation) and never let a
    // multi-thousand-metre global coordinate reach float32 math directly.
    struct Anchor
    {
        LLVector3d mAnchorGlobal;
        bool       mValid = false;
    };
    const Anchor& anchor();

    // Per-scale global lattice frame: for a world-space pattern of period
    // `scale` metres (grid spacing, major-line spacing, pulse wavelength,
    // trace cell size, ...), returns the remainder `mR` (metres -- add to a
    // camera-relative position before dividing by `scale`) and the integer
    // lattice cell index `mK` (wrapped mod 2^32 per axis -- add to the
    // shader's local cell id) that together make the pattern's phase and
    // per-cell hash identical across region crossings, anchor re-snaps and
    // sessions. Folds in TronGridOriginGlobal. Computed in double on the
    // CPU so no precision is lost before the small, camera-relative shader
    // math takes over. `scale` must be > 0; returns an all-zero frame
    // otherwise.
    struct LatticeFrame
    {
        LLVector3 mR;
        U32       mK[3] = { 0, 0, 0 };
    };
    LatticeFrame computeLattice(F64 scale);

    // Every Tron* setting key introduced so far -- the single source of
    // truth for LLFloaterDirector::sceneSettingsList() and the Lightbox
    // tab's "Reset all" (mirrors ALRotoInk::settings() /
    // ALRotoInk::resetToDefaults()). Grows as later phases add
    // grade/grid/trace/rim/trail/integration keys.
    const std::vector<std::string>& settings();

    // Resets every key in settings() to its settings.xml default.
    void resetToDefaults();
}

#endif // AL_TRON_H
