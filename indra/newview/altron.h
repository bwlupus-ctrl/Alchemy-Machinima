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
    // [TronT1] Raw TronEnabled read, no other gating. Used where a consumer
    // specifically wants "the Tron master switch is on" regardless of
    // whether grade/grid are individually enabled -- e.g. the Rig Rim Tron
    // tint (ALCineRigRim::bindAnimated), which is a stand-alone use of the
    // shared clock/palette and should work even with the world pass itself
    // switched off (grade and grid both at 0).
    bool isEnabledMaster();

    // [TronT1] isEnabledMaster() && (TronGradeStrength > 0 || TronGridEnabled)
    // -- the master gate every render-affecting consumer (isActiveForCurrent-
    // Pass(), the Lightbox tab's group visibility) should use: TronEnabled
    // alone with both the grade and the grid off has nothing to draw, so the
    // Tron World pass should not even attempt its own per-frame settings
    // read/gather. T2/T3 OR their own enable flags into this in later phases.
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

    // [TronT1] Which point in the HDR post chain the Tron World pass runs at
    // (TronLayer, clamped). Mirrors LLPipeline::ERotoInkLayer's Scene/Camera
    // split for Roto Ink, but Tron only ever has these two slots (no
    // Atmosphere/Overlay -- see the T1 contract section 3.6).
    enum ELayer { LAYER_SCENE = 0, LAYER_CAMERA = 1 };
    ELayer layer();

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

    // [TronT1] Same computation, but with the anchor passed in explicitly
    // instead of read from the idle()-latched anchor() -- see the T1
    // contract section 3.3: LLPipeline::renderTronWorld derives its own
    // anchor from the pass's OWN camera matrix (never the one-frame-lagged
    // idle anchor) and must fold that SAME anchor into every lattice frame
    // it asks for. computeLattice(scale) above is now a thin wrapper that
    // forwards to this overload with anchor().mAnchorGlobal.
    LatticeFrame computeLattice(F64 scale, const LLVector3d& anchor_global);

    // [TronT1] Shared pulse waveform, sampled from the shared clock's own
    // phase accumulator (clock().mPulsePhase01) so every consumer (Rig Rim
    // tint, the Tron World pass's tron_master.y, later Actor FX Tron Suit)
    // agrees on "now". Shape from TronPulseShape: 0 sine, 1 saw, 2
    // heartbeat (double-bump), 3 square. Pure function, result in [0,1].
    F32 pulse01();

    // [TronT1] Rig Rim mode-2 ("tint + pulse") multiplier:
    // max(1 + TronPulseAmount * (pulse01() - 0.5) * 2, 0).
    F32 pulseMul();

    // [TronT1] Per-frame resolved state shared between resolveFrame() and
    // its consumers within the same frame -- mirrors LLPipeline's
    // NightMaskFrameState/updateNightMaskAnchor() B1 idiom exactly, so a
    // Tron toggle can't pump exposure any more than a Night Mask toggle can.
    struct Frame
    {
        F32 mBloomScale = 1.f; // ramped toward (willRun ? TronBloomMeterScale : 1)
    };

    // [TronT1] Ramps Frame::mBloomScale toward TronBloomMeterScale while the
    // Tron World pass will actually render at the Camera layer this frame,
    // else toward 1. Call exactly once per frame, in renderFinalize's
    // prologue next to updateNightMaskAnchor() (before generateLuminance()).
    void resolveFrame();

    // The frame state resolved by the most recent resolveFrame() call.
    const Frame& frame();

    // [TronT1] Applies one of the 8 T1 look presets (section 8 of the T1
    // contract): writes every "L" key (grade/grid/pulse/Rig-Rim-Tron look
    // values) and the shared palette (via applyPalettePreset), then --
    // ONLY when the matching TronPresetAlso* flag is on -- the linked Roto
    // Ink / Rig Rim preset. Regardless of those flags, remembers the linked
    // preset's key/label/mode so the Lightbox Integration group's "Apply
    // Tron Roto/Rig Rim preset" buttons can apply it later on demand (see
    // lastPresetRotoKey()/lastPresetRigRimLabel()/lastPresetRigRimTronMode()).
    // Never touches TronEnabled, TronLayer, TronSubject*,
    // TronGridOriginGlobal or TronGridSubject* (shot-specific, not a look).
    // Unknown key: no-op.
    void applyPreset(const std::string& key);

    // Roto Ink preset key / Rig Rim preset label / Rig Rim Tron mode tied to
    // the most recently applied look preset (applyPreset()); empty/0 until
    // the first call.
    const std::string& lastPresetRotoKey();
    const std::string& lastPresetRigRimLabel();
    S32 lastPresetRigRimTronMode();

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
