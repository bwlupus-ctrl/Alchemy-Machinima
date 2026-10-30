/**
 * @file alavataranimationtime.h
 * @brief Shared animation-clock delta for procedural avatar joint layers.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef AL_AVATARANIMATIONTIME_H
#define AL_AVATARANIMATIONTIME_H

#include "llmotioncontroller.h"
#include "lluuid.h"
#include <array>
#include <cstring>
#include <cmath>
#include "llframetimer.h"

namespace ALAvatarAnimationTime
{
// Bit tests remain valid under Release /fp:fast.
inline bool finite(F32 value)
{
    U32 bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}

struct Delta
{
    F32 mSeconds = 0.f;
    F32 mWallSeconds = 0.f;
    bool mReseed = false;
    bool mUnscaled = false;
    bool mClockFrozen = false;
};

inline Delta delta(F32 wall_dt, F32 global_factor, F32 avatar_factor)
{
    Delta out;
    out.mUnscaled = global_factor == 1.f && avatar_factor == 1.f;
    out.mClockFrozen = finite(global_factor) && finite(avatar_factor) &&
        global_factor >= 0.f && avatar_factor >= 0.f &&
        (global_factor == 0.f || avatar_factor == 0.f);
    out.mReseed = !finite(wall_dt) || wall_dt < 0.f || wall_dt > 0.25f;
    if (!finite(wall_dt) || wall_dt < 0.f ||
        !finite(global_factor) || !finite(avatar_factor) ||
        global_factor < 0.f || avatar_factor < 0.f)
    {
        return out;
    }
    out.mWallSeconds = wall_dt;
    if (out.mUnscaled)
    {
        out.mSeconds = wall_dt; // exact legacy path, including zero wall delta
    }
    else
    {
        out.mSeconds = wall_dt * global_factor * avatar_factor;
    }
    if (!finite(out.mSeconds)) out.mSeconds = 0.f;
    return out;
}

// Policy is published by the viewer once per frame. This library stays free
// of viewer settings and UI dependencies.
inline bool sKeepLife = false;

inline Delta lifeDelta(F32 wall_dt, F32 global_factor, F32 avatar_factor, bool keep_life)
{
    return keep_life ? delta(wall_dt, 1.f, 1.f)
                     : delta(wall_dt, global_factor, avatar_factor);
}

template<typename Avatar>
inline Delta forLife(F32 wall_dt, Avatar& avatar)
{
    return lifeDelta(wall_dt, LLMotionController::getGlobalTimeFactor(),
                     avatar.getMotionController().getTimeFactor(), sKeepLife);
}

template<typename Avatar>
inline Delta forAvatar(F32 wall_dt, Avatar& avatar)
{
    return delta(wall_dt, LLMotionController::getGlobalTimeFactor(),
                 avatar.getMotionController().getTimeFactor());
}

inline Delta worldDelta(F32 wall_dt, F32 global_factor)
{
    return delta(wall_dt, global_factor, 1.f);
}

inline Delta forWorld(F32 wall_dt)
{
    return worldDelta(wall_dt, LLMotionController::getGlobalTimeFactor());
}

inline bool frozen(const Delta& dt)
{
    return !dt.mUnscaled && dt.mSeconds == 0.f && (!dt.mReseed || dt.mClockFrozen);
}

// A paused pose survives even a stalled wall frame. Live zero intervals retain
// Pose Polish's original dt <= 0 reseed path.
inline bool polishReseed(const Delta& dt)
{
    return !frozen(dt) && (dt.mReseed || (dt.mUnscaled && dt.mWallSeconds <= 0.f));
}

inline bool legacyGazeClock(bool active, bool drives_animation)
{
    return active && !drives_animation;
}

// Identity comparisons never dereference the cached (possibly dead) owner.
struct OwnerIdentity
{
    const void* mPointer = nullptr;
    LLUUID mId;
    U64 mCreation = 0;
    bool matches(const void* pointer, const LLUUID& id, U64 creation) const
    {
        return mPointer == pointer && mId == id && mCreation == creation;
    }
    void set(const void* pointer, const LLUUID& id, U64 creation)
    {
        mPointer = pointer; mId = id; mCreation = creation;
    }
};

template<typename Sample, size_t Count>
struct SlotSamples
{
    std::array<Sample, Count> mValues{};
    std::array<bool, Count> mPresent{};
    void clear() { mPresent.fill(false); }
    template<typename Slot, typename Resolve>
    const Sample& sample(Slot slot, bool hold, Resolve resolve)
    {
        const size_t index = static_cast<size_t>(slot);
        if (!hold || !mPresent[index])
        {
            mValues[index] = resolve();
            mPresent[index] = true;
        }
        return mValues[index];
    }
};

inline bool seedFrozenLayer(bool is_frozen, bool cache_changed)
{
    return is_frozen && cache_changed;
}

inline Delta seedDelta()
{
    return delta(0.f, 0.f, 1.f);
}

// Preserve each placement consumer's original presentation-delta source,
// including its zero on a scrub/reverse frame. Controller speed is absent.
inline F32 placementSeconds(F32 wall_dt, F32 global_factor, F32 presentation_dt,
                            bool drives_animation, bool cap_wall = true)
{
    if (drives_animation) return llclamp(presentation_dt, 0.f, 0.25f);
    return worldDelta(cap_wall ? llclamp(wall_dt, 0.f, 0.25f) : wall_dt, global_factor).mSeconds;
}

inline F32 animationSeconds(F32 wall_dt, F32 global_factor, F32 avatar_factor,
                            F32 presentation_dt, bool drives_animation)
{
    const F32 dt = placementSeconds(wall_dt, global_factor, presentation_dt, drives_animation);
    return avatar_factor == 1.f ? dt : dt * avatar_factor;
}

// Phase clocks previously sampled absolute presentation time. Do not apply
// the envelope's hitch cap to a driven phase; scrub/reverse zero still holds.
inline Delta gazePhaseDelta(Delta wall, F32 presentation_dt, bool drives_animation, F32 avatar_factor)
{
    if (drives_animation)
    {
        const F32 dt = llmax(0.f, presentation_dt);
        wall.mSeconds = avatar_factor == 1.f ? dt : dt * avatar_factor;
    }
    return wall;
}

inline bool holdWorldTurn(F32 dt, F32 global_factor)
{
    return dt == 0.f && global_factor == 0.f;
}

// Absolute-time samplers keep their exact legacy epoch/rounding at unit speed.
// Once scaled, integrate animation deltas instead; never catch up wall time on
// resume. The caller owns the once-per-frame guard and any explicit scrub reset.
struct Clock
{
    F64 mTime = 0.0;
    bool mScaled = false;
    bool mValid = false;
    F64 mLegacyTime = 0.0;

    F64 epochOffset() const { return mTime - mLegacyTime; }

    F64 sample(F64 legacy_time, const Delta& dt, bool unit_speed, bool reanchor = false)
    {
        mLegacyTime = legacy_time;
        if (!mValid)
        {
            mTime = unit_speed ? legacy_time : legacy_time - dt.mWallSeconds;
            mValid = true;
        }
        if (unit_speed && (!mScaled || reanchor))
        {
            mScaled = false;
            mTime = legacy_time;
        }
        else
        {
            mScaled = true;
            mTime += dt.mSeconds;
        }
        return mTime;
    }
};

// Only the program epoch may reanchor. A life clock that has integrated
// scaled time never jumps to a different drift/blink/break lattice cell.
inline F64 sampleGazeClocks(Clock& program, Clock& life, F64 legacy_time,
                           const Delta& animation_dt, const Delta& life_dt,
                           bool unit_speed, bool legacy_only, bool safe)
{
    const bool was_scaled = program.mScaled;
    const F64 next = program.mTime + animation_dt.mSeconds;
    const bool reanchor = was_scaled && safe && (unit_speed || legacy_only);
    program.sample(legacy_time, animation_dt, unit_speed || legacy_only, reanchor);
    life.sample(legacy_time, life_dt, unit_speed || legacy_only);
    return reanchor ? program.mTime - next : 0.0;
}

// Source sampling can be temporarily scaled, then catches up without a jump.
// Recovery starts with zero correction, uses wall time once per caller frame,
// and lands on the live source in at least half a second, extended by the speed cap.
struct DrivenClock
{
    F64 mTime = 0.0;
    F64 mSource = 0.0;
    bool mValid = false;
    bool mScaled = false;
    F64 mRecoveryOffset = 0.0;
    F32 mRecoveryTime = 0.f;
    F64 mRecoveryDuration = 0.5;
    bool mRecovering = false;
    U64 mRevision = 0;
    F64 mHeldProgress = 0.0;

    F64 sample(F64 source, F32 rate, F64 initial, F32 wall_dt = 0.f, U64 revision = 0,
               F64 max_source_speed = -1.0)
    {
        if (mValid && revision != mRevision) *this = DrivenClock();
        mRevision = revision;
        if (!mValid)
        {
            mTime = initial;
            mSource = source;
            mValid = true;
        }
        const F64 previous = mTime;
        const F64 progress = source - mSource;
        mSource = source;
        if (rate == 1.f)
        {
            if (!mScaled) mTime = source;
            else
            {
                mTime += progress;
                if (!mRecovering)
                {
                    mRecoveryOffset = mTime - source;
                    mRecoveryTime = 0.f;
                    // Quintic smootherstep has a peak derivative of 1.875.
                    mRecoveryDuration = max_source_speed > 0.0
                        ? llmax(0.5, 1.875 * std::fabs(mRecoveryOffset) / max_source_speed) : 0.5;
                    mRecovering = true;
                    mHeldProgress = 0.0;
                }
                else if (finite(wall_dt) && wall_dt > 0.f)
                {
                    mRecoveryTime += wall_dt;
                }
                F64 u = llmin(1.0, mRecoveryTime / mRecoveryDuration);
                F64 ease = u * u * u * (u * (u * 6.0 - 15.0) + 10.0);
                if (mHeldProgress != 0.0)
                {
                    if (ease < 1.0)
                        mRecoveryOffset -= mHeldProgress / (1.0 - ease);
                    else
                    {
                        // A scrub at the end of recovery creates a new held
                        // gap. Restart from the displayed phase, never source.
                        mRecoveryOffset = mTime - source;
                        mRecoveryTime = 0.f;
                        mRecoveryDuration = max_source_speed > 0.0
                            ? llmax(0.5, 1.875 * std::fabs(mRecoveryOffset) / max_source_speed) : 0.5;
                        u = ease = 0.0;
                    }
                }
                const F64 desired = source + mRecoveryOffset * (1.0 - ease);
                mTime = desired;
                if (max_source_speed >= 0.0)
                {
                    const F64 max_step = max_source_speed * llmax(0.f, wall_dt);
                    mTime = previous + llclamp(desired - previous, -max_step, max_step);
                }
                if (u >= 1.0 && mTime == source)
                {
                    mTime = source;
                    mScaled = mRecovering = false;
                }
            }
        }
        else
        {
            mRecovering = false;
            mScaled = true;
            mTime += progress * rate;
        }
        mHeldProgress = 0.0;
        return mTime;
    }

    // Authored transport discontinuities replace the absolute source baseline,
    // even when playback is scaled or paused. No old recovery survives a seek.
    void seekSource(F64 source, U64 revision)
    {
        *this = DrivenClock();
        mTime = mSource = source;
        mRevision = revision;
        mValid = true;
    }

    // A zero presentation step observes transport without consuming it on
    // resume. Preserve the displayed phase and every recovery field.
    void holdSource(F64 source, F64 initial = 0.0, U64 revision = 0)
    {
        if (!mValid)
        {
            mTime = initial;
            mSource = source;
            mRevision = revision;
            mValid = mScaled = true;
        }
        if (source != mSource)
        {
            mHeldProgress += source - mSource;
            mScaled = true;
        }
        mSource = source;
    }

    void remapSource(F64 source, F64 displayed_sample)
    {
        mHeldProgress = 0.0; // the remapped offset already accounts for this gap
        mSource = source;
        mTime = displayed_sample;
        if (mRecovering)
        {
            const F64 u = llmin(1.0, mRecoveryTime / mRecoveryDuration);
            const F64 ease = u * u * u * (u * (u * 6.0 - 15.0) + 10.0);
            mRecoveryOffset = ease < 1.0 ? (mTime - source) / (1.0 - ease) : 0.0;
        }
    }
};

// Sync traversal may be faster than the path's authored walking speed. Bound
// total recovery speed against both, so a moving take can still be caught.
inline F64 recoveryArcSpeed(F64 authored_speed, F64 source_step, F32 wall_dt,
                            F64 length, F64 duration)
{
    const F64 source_speed = wall_dt > 0.f ? std::fabs(source_step) * length / duration / wall_dt : 0.0;
    return 1.25 * llmax(llmax(0.0, authored_speed), source_speed);
}

inline F64 boundedRecoveryArc(F64 desired, F64 previous, F64 max_step)
{
    return previous + llclamp(desired - previous, -max_step, max_step);
}

// The leader arc already advances on world time. A follower shares it exactly
// while running; its own controller speed applies only to its skeleton.
inline F64 followerSource(F64 leader_arc, F64 held_arc, F32 global_factor, bool seed_placement = false)
{
    return global_factor == 0.f && !seed_placement ? held_arc : leader_arc;
}

inline bool pathSeek(bool authored_revision, F32 dt, F32 arc_delta, F32 threshold)
{
    return authored_revision || (dt > 0.f && std::fabs(arc_delta) > threshold);
}

inline bool placementNeedsSample(F32 dt, bool evaluated, bool authored_seek = false)
{
    return dt != 0.f || !evaluated || authored_seek;
}

template<typename Position, typename Rotation>
inline void seedPose(bool& seeded, Position& position, Rotation& rotation,
                     const Position& root_position, const Rotation& root_rotation)
{
    if (seeded) return;
    position = root_position;
    rotation = root_rotation;
    seeded = true;
}

template<typename Target>
inline Target* eyeTarget(bool is_frozen, bool& seeded, bool& has_target,
                         Target& held, Target* live)
{
    if (!seeded || !is_frozen)
    {
        has_target = live != nullptr;
        if (live) held = *live;
        seeded = true;
    }
    return is_frozen ? (has_target ? &held : nullptr) : live;
}

inline bool seedFrozenConstraint(bool is_frozen, bool have_pose)
{
    return is_frozen && !have_pose;
}

// Map an unwrapped transport coordinate without confusing repeat wraps with
// backward source seeks. Modes match recorder hold/repeat/ping-pong.
inline F64 playhead(F64 progress, F64 duration, S32 loop_mode)
{
    if (loop_mode == 1)
    {
        progress -= std::floor(progress / duration) * duration;
    }
    else if (loop_mode == 2)
    {
        const F64 period = 2.0 * duration;
        progress -= std::floor(progress / period) * period;
        if (progress > duration) progress = period - progress;
    }
    else
    {
        if (progress < 0.0) progress = 0.0;
        if (progress > duration) progress = duration;
    }
    return progress;
}

// Invert the loop mapping near the new source epoch after a metadata-only
// loop-mode change, retaining the same displayed pose.
inline F64 sourceCoordinate(F64 sample, F64 duration, S32 mode, F64 reference)
{
    if (mode == 1)
        return sample + std::round((reference - sample) / duration) * duration;
    if (mode == 2)
    {
        const F64 period = duration * 2.0;
        const F64 forward = sample + std::round((reference - sample) / period) * period;
        const F64 backward = period - sample + std::round((reference - (period - sample)) / period) * period;
        return std::fabs(forward - reference) <= std::fabs(backward - reference) ? forward : backward;
    }
    return llclamp(sample, 0.0, duration);
}

// Camera resume restarts its direction, but is not an authored seek. Keep the
// unwrapped source and express any changed loop mapping as metadata only.
inline void startPlaybackProgress(bool resume, F32 playhead, F32 duration, S32 mode,
                                  F64& progress, S32& progress_mode, F32& direction,
                                  F64& offset, U64& revision, U64& mapping_revision)
{
    if (!resume)
    {
        progress = playhead;
        progress_mode = mode;
        direction = 1.f;
        offset = 0.0;
        ++revision;
        return;
    }
    F64 effective = offset == 0.0 ? progress : progress + offset;
    F64 next_offset = offset;
    if (mode != progress_mode && duration > 0.f)
    {
        effective = sourceCoordinate(playhead, duration, mode, effective);
        next_offset = effective - progress;
    }
    F32 next_direction = 1.f;
    if (mode == 2 && duration > 0.f)
    {
        const F64 period = 2.0 * duration;
        const F64 phase = effective - std::floor(effective / period) * period;
        next_direction = phase < duration ? 1.f : -1.f;
    }
    if (mode != progress_mode || next_direction != direction || next_offset != offset)
        ++mapping_revision;
    progress_mode = mode;
    direction = next_direction;
    offset = next_offset;
}

// Scale a legacy wall-time damping fraction without changing unit-speed bits.
inline F32 interpolant(F32 legacy, const Delta& animation, F32 wall_dt)
{
    if (animation.mUnscaled) return legacy;
    if (animation.mSeconds == 0.f) return 0.f;
    if (animation.mSeconds == wall_dt) return legacy;
    return 1.f - std::pow(1.f - legacy, animation.mSeconds / wall_dt);
}

// Editing's slerp runs from solved IK toward the old pose, so its parameter
// is the retained fraction, the complement of a toward-target interpolant.
inline F32 retainedInterpolant(F32 legacy, const Delta& animation, F32 wall_dt)
{
    if (animation.mUnscaled) return legacy;
    return 1.f - interpolant(1.f - legacy, animation, wall_dt);
}

template<typename Release>
inline void releaseTurnOverride(bool& active, const LLUUID& source, Release release)
{
    if (!active) return;
    const LLUUID owned_source = source;
    active = false; // consume ownership before callback/reentrant teardown
    if (owned_source.notNull()) release(owned_source);
}

template<typename Avatar>
inline bool unitSpeed(Avatar& avatar)
{
    return LLMotionController::getGlobalTimeFactor() == 1.f &&
           avatar.getMotionController().getTimeFactor() == 1.f;
}
}
#endif
