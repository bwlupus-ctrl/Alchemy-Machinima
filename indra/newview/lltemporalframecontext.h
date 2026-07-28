/**
 * @file lltemporalframecontext.h
 * @brief Immutable per-iteration snapshot of the presentation clock.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Landing 1 of Temporal Capture (World Time Scale). See
 * doc/TEMPORAL_CAPTURE_WORLD_TIME_SCALE_BRIEF.md.
 *
 * LLPresentationTime freezes ONE of these at the top of LLAppViewer::idle();
 * every locally-evaluated cinematic consumer in the following idle->display pass
 * reads THIS struct and never re-queries "now", so texture animation, particles,
 * avatars, objects and the camera can never observe skewed times within a frame.
 */

#ifndef LL_LLTEMPORALFRAMECONTEXT_H
#define LL_LLTEMPORALFRAMECONTEXT_H

#include "stdtypes.h"

// Presentation-clock mode. Landing 1 wires LIVE / MANUAL_SCALE / PAUSED; the
// remaining values are RESERVED for later Temporal Capture pillars and are NOT
// settings-selectable nor deserializable into partial behavior yet.
enum class LLTemporalMode : U8
{
    LIVE = 0,               // presentation time == wall time (stock behavior)
    MANUAL_SCALE,           // presentation advances at effective_scale
    PAUSED,                 // presentation held (0x)
    ADAPTIVE_SCALE,         // reserved: adaptive-FPS scaling
    FIXED_FRAME_CAPTURE,    // reserved: Pillar 3
    BUFFERED_REPLAY,        // reserved: Pillar 2
    SEEK_REBUILD            // reserved
};

// Which locally-evaluated dynamics consult presentation time. Stored as a bit
// mask in the frozen frame's drive_mask; each bit maps to a TemporalDrive*
// setting. ANIMATION couples skeletal playback AND actor/ghost world motion, so
// a moving actor's gait and traversal stay locked (no foot-slide).
enum class LLTemporalFeature : U32
{
    ANIMATION    = 1u << 0, // all motion controllers + actor-mover/ghost motion
    OBJECTS      = 1u << 1, // target-omega spin + object-path drives
    TEXTURE_ANIM = 1u << 2,
    PARTICLES    = 1u << 3,
    CAMERA       = 1u << 4
};

struct LLTemporalFrameContext
{
    U64            generation = 0;
    LLTemporalMode mode = LLTemporalMode::LIVE;

    // Unscaled monotonic process time (never multiplied by the scale).
    F64 wall_time = 0.0;
    F64 wall_delta = 0.0;           // clamp(now - prev, 0, 1)

    // The scaled cinematic clock local dynamics advance on.
    F64 presentation_time = 0.0;
    F64 presentation_delta = 0.0;   // max(0, presentation_time - previous)

    F32  effective_scale = 1.f;
    bool paused = false;

    U32 drive_mask = 0;             // enabled LLTemporalFeature bits

    bool active() const { return mode != LLTemporalMode::LIVE; }
    bool drives(LLTemporalFeature f) const
    {
        return (drive_mask & static_cast<U32>(f)) != 0u;
    }
};

#endif // LL_LLTEMPORALFRAMECONTEXT_H
