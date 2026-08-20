/**
 * @file algazemath.h
 * @brief Pure math shared by Actor Mover lens gaze and its unit tests.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALGAZEMATH_H
#define AL_ALGAZEMATH_H

#include "llmath.h"
#include "llquaternion.h"
#include "lluuid.h"
#include "v3math.h"

#include <algorithm>
#include <cmath>

namespace ALGazeMath
{
// ---------------------------------------------------------------------------
// Basic smoothing & chase helpers
// ---------------------------------------------------------------------------
inline F32 chaseAlpha(F32 dt, F32 tau)
{
    return 1.f - expf(-llmax(dt, 0.f) / llmax(tau, 0.01f));
}

inline void deadZoneChase(F32& aim_pitch, F32& aim_yaw,
                          F32 raw_pitch, F32 raw_yaw,
                          F32 dead_zone, F32 alpha)
{
    const F32 pitch_delta = raw_pitch - aim_pitch;
    const F32 yaw_delta = llsimple_angle(raw_yaw - aim_yaw);
    const F32 aim_error = sqrtf(pitch_delta * pitch_delta + yaw_delta * yaw_delta);
    if (aim_error <= dead_zone || aim_error <= 0.f)
    {
        return;
    }

    const F32 chase = alpha * (aim_error - dead_zone) / aim_error;
    aim_pitch += pitch_delta * chase;
    aim_yaw = llsimple_angle(aim_yaw + yaw_delta * chase);
}

inline void torsoChase(F32& torso_pitch, F32& torso_yaw,
                       F32 target_pitch, F32 target_yaw,
                       F32 ratio, F32 dt, F32 tau_user)
{
    if (ratio <= 1.001f)
    {
        torso_pitch = target_pitch;
        torso_yaw = target_yaw;
        return;
    }

    const F32 tau_extra = (ratio - 1.f) * tau_user;
    const F32 alpha = chaseAlpha(dt, tau_extra);
    torso_pitch += (target_pitch - torso_pitch) * alpha;
    torso_yaw = llsimple_angle(
        torso_yaw + llsimple_angle(target_yaw - torso_yaw) * alpha);
}

inline F32 behindEnvStep(F32 env, bool behind, F32 dt, F32 ease_time)
{
    const F32 step = ease_time > 0.f ? llmax(dt, 0.f) / ease_time : 1.f;
    return llclamp(env + (behind ? -step : step), 0.f, 1.f);
}

inline LLVector3 eyelineOffsetDir(const LLVector3& direction,
                                 const LLVector3& up_axis,
                                 F32 yaw, F32 pitch)
{
    if (fabsf(yaw) + fabsf(pitch) <= 1e-4f)
    {
        return direction;
    }

    LLVector3 left_axis = up_axis % direction;
    if (left_axis.normVec() <= 1e-4f)
    {
        return direction;
    }

    const LLQuaternion yaw_rotation(yaw, up_axis);
    const LLQuaternion pitch_rotation(pitch, left_axis);
    return direction * (pitch_rotation * yaw_rotation);
}

// Signed camera dutch angle around its forward axis relative to world up.
// Roll is undefined when forward is parallel to world up; returning neutral is
// deterministic and avoids inventing a horizon at the pole.
inline F32 cameraRollAboutForward(LLVector3 forward, LLVector3 camera_up)
{
    if (!forward.isFinite() || !camera_up.isFinite() ||
        forward.normVec() <= 1e-4f)
    {
        return 0.f;
    }

    const LLVector3 world_up(0.f, 0.f, 1.f);
    LLVector3 reference_left = world_up % forward;
    if (reference_left.normVec() <= 1e-4f)
    {
        return 0.f;
    }
    LLVector3 reference_up = forward % reference_left;
    reference_up.normVec();

    camera_up -= forward * (camera_up * forward);
    if (camera_up.normVec() <= 1e-4f)
    {
        return 0.f;
    }
    return atan2f(
        forward * (reference_up % camera_up),
        llclamp(reference_up * camera_up, -1.f, 1.f));
}

// ---------------------------------------------------------------------------
// Deterministic closed-form hashing & seed helpers (§2, §B)
// ---------------------------------------------------------------------------
constexpr U64 DEFAULT_GAZE_SEED = 0x123456789abcdef0ULL;

inline U64 splitMix64(U64 value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

inline U64 castSeedFromUUID(const LLUUID& id)
{
    if (id.isNull())
    {
        return DEFAULT_GAZE_SEED;
    }
    U64 low = 0;
    U64 high = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        low |= (static_cast<U64>(id.mData[i]) << (i * 8));
        high |= (static_cast<U64>(id.mData[i + 8]) << (i * 8));
    }
    // Fold in byte order so swapping the UUID halves does not collide.
    const U64 seed = splitMix64(
        splitMix64(low) ^ splitMix64(high ^ 0xa0761d6478bd642fULL));
    return seed ? seed : DEFAULT_GAZE_SEED;
}

inline F32 unitHash(U64 seed, S32 channel, U64 counter, S32 subchannel, S32 draw)
{
    U64 key = seed ? seed : DEFAULT_GAZE_SEED;
    key = splitMix64(key ^ (0xd1b54a32d192ed03ULL * static_cast<U64>(channel + 1)));
    key = splitMix64(key ^ (0x9e3779b97f4a7c15ULL * (counter + 1)));
    key = splitMix64(key ^ (0x94d049bb133111ebULL * static_cast<U64>(subchannel + 2)));
    key = splitMix64(key ^ (0xbf58476d1ce4e5b9ULL * static_cast<U64>(draw + 3)));
    return static_cast<F32>((key >> 40) * (1.0 / 16777216.0));
}

inline F32 valueNoise(U64 seed, S32 channel, F64 coordinate, S32 subchannel, S32 draw)
{
    if (!std::isfinite(coordinate) || coordinate < 0.0)
    {
        coordinate = 0.0;
    }
    const U64 cell = static_cast<U64>(std::floor(coordinate));
    F32 fraction = static_cast<F32>(coordinate - std::floor(coordinate));
    fraction = fraction * fraction * (3.f - 2.f * fraction); // smoothstep
    const F32 a = unitHash(seed, channel, cell, subchannel, draw);
    const F32 b = unitHash(seed, channel, cell + 1, subchannel, draw);
    return a + (b - a) * fraction;
}

// ---------------------------------------------------------------------------
// Gaze naturalism parameters & structures
// ---------------------------------------------------------------------------
struct GazeLifeParams
{
    F32  mMicroLife              = 0.4f;   // 0..1 overall micro-life scale
    bool mBlinks                 = true;   // enable periodic eyelid blink / eye-dip
    F32  mBlinkRate              = 1.f;    // multiplier on the deterministic blink lattice
    F32  mReactionMin            = 0.1f;   // min reaction delay, s
    F32  mReactionMax            = 0.6f;   // max reaction delay, s
    F32  mBreakFrequency         = 0.3f;   // 0 = never, 1 = fidgety
    F32  mBodyTurnThresholdDeg   = 90.f;   // yaw threshold to trigger body turn
    F32  mEaseAcquireSec         = 0.25f;  // acquire ease duration
    F32  mEaseReleaseSec         = 0.60f;  // release ease duration
    F32  mVariation              = 0.3f;   // per-subject variation scale 0..1
};

struct MicroLifeOffsets
{
    F32 mEyeSaccadeYaw   = 0.f; // radians
    F32 mEyeSaccadePitch = 0.f; // radians
    F32 mHeadDriftYaw    = 0.f; // radians
    F32 mHeadDriftPitch  = 0.f; // radians
    F32 mBlinkDipPitch   = 0.f; // radians
    F32 mBlinkFraction   = 0.f; // 0 = open, 1 = fully closed
};

struct AnatomicalChainPose
{
    F32  mEyeYaw          = 0.f; // radians
    F32  mEyePitch        = 0.f; // radians
    F32  mHeadYaw         = 0.f; // radians
    F32  mHeadPitch       = 0.f; // radians
    F32  mNeckYaw         = 0.f; // radians
    F32  mNeckPitch       = 0.f; // radians
    F32  mTorsoYaw        = 0.f; // radians
    F32  mTorsoPitch      = 0.f; // radians
    F32  mHipsYaw         = 0.f; // radians
    F32  mHipsPitch       = 0.f; // radians
    bool mTriggerBodyTurn = false;
};

struct GazePersona
{
    F32 mDominance = 0.f; // -1..1
    F32 mAffection = 0.f; // -1..1
    F32 mAnxiety   = 0.f; // -1..1
};

// A single pure persona mapping supplies every expressive layer. These values
// only modulate primitives the gaze solver already owns; no alternate render
// path or temporal state is introduced.
struct PersonaModulation
{
    F32  mContactHoldScale    = 1.f;
    F32  mBreakFrequencyScale = 1.f;
    F32  mBlinkRateScale      = 1.f;
    F32  mChinPitchBias       = 0.f; // radians; positive lowers the chin
    F32  mEyelinePitchBias    = 0.f; // radians; negative is a warmer upward line
    F32  mHeadEyeBlendAdd     = 0.f;
    F32  mMicroLifeScale      = 1.f;
    F32  mAcquireEaseScale    = 1.f;
    F32  mReleaseEaseScale    = 1.f;
    F32  mReactionScale       = 1.f;
    F32  mSmoothingAdd        = 0.f;
    F32  mContactAversionStrength = 0.f;
    F32  mDownwardAversion    = 0.f;
    F32  mSideEyeStrength     = 0.f;
    F32  mLidNarrow           = 0.f;
    bool mAlphaStare          = false;
};

inline PersonaModulation mapPersona(const GazePersona& persona)
{
    PersonaModulation out;
    const F32 dominance = llclamp(persona.mDominance, -1.f, 1.f);
    const F32 affection = llclamp(persona.mAffection, -1.f, 1.f);
    const F32 anxiety = llclamp(persona.mAnxiety, -1.f, 1.f);
    const F32 dom_up = llmax(dominance, 0.f);
    const F32 aff_up = llmax(affection, 0.f);
    const F32 anx_up = llmax(anxiety, 0.f);

    out.mAlphaStare = dominance >= 0.95f;
    out.mContactHoldScale = llclamp(
        1.f + 1.7f * dom_up - 0.65f * anx_up, 0.35f, 3.f);
    out.mBreakFrequencyScale = out.mAlphaStare ? 0.f : llclamp(
        1.f - 0.8f * dom_up + 1.4f * anx_up, 0.f, 3.f);
    out.mBlinkRateScale = out.mAlphaStare ? 0.f : llclamp(
        1.f - 0.55f * dom_up + 0.9f * anx_up - 0.1f * aff_up,
        0.2f, 2.5f);
    out.mChinPitchBias = 2.75f * dom_up * DEG_TO_RAD;
    out.mEyelinePitchBias = -0.8f * aff_up * DEG_TO_RAD;
    out.mHeadEyeBlendAdd = 0.25f * dom_up;
    out.mMicroLifeScale = out.mAlphaStare ? 0.12f : llclamp(
        1.f - 0.55f * dom_up + 0.75f * anx_up - 0.15f * aff_up,
        0.1f, 2.f);
    out.mAcquireEaseScale = 1.f + 0.25f * anx_up + 0.35f * aff_up;
    out.mReleaseEaseScale = 1.f + 0.35f * aff_up;
    out.mReactionScale = 1.f + 0.15f * anx_up;
    out.mSmoothingAdd = 0.12f * aff_up - 0.08f * anx_up;
    out.mContactAversionStrength = anx_up;
    out.mDownwardAversion = llclamp(
        anx_up - 0.30f * dom_up, 0.f, 1.f);
    // Suspicion is strongest when anxiety is high and affection is low.
    out.mSideEyeStrength = anx_up * llclamp((0.4f - affection) * 0.75f, 0.f, 1.f);
    out.mLidNarrow = 0.10f * aff_up;
    return out;
}

inline void applyPersona(const PersonaModulation& persona,
                         const GazeLifeParams& in_params,
                         GazeLifeParams& out_params)
{
    out_params = in_params;
    out_params.mMicroLife = llclamp(
        in_params.mMicroLife * persona.mMicroLifeScale, 0.f, 1.f);
    out_params.mBlinks = in_params.mBlinks && persona.mBlinkRateScale > 0.f;
    out_params.mBlinkRate = llmax(
        in_params.mBlinkRate * persona.mBlinkRateScale, 0.f);
    out_params.mReactionMin = llmax(
        in_params.mReactionMin * persona.mReactionScale, 0.f);
    out_params.mReactionMax = llmax(
        in_params.mReactionMax * persona.mReactionScale,
        out_params.mReactionMin);
    out_params.mBreakFrequency = llclamp(
        in_params.mBreakFrequency * persona.mBreakFrequencyScale, 0.f, 1.f);
    out_params.mEaseAcquireSec = llmax(
        in_params.mEaseAcquireSec * persona.mAcquireEaseScale, 0.f);
    out_params.mEaseReleaseSec = llmax(
        in_params.mEaseReleaseSec * persona.mReleaseEaseScale, 0.f);
}

struct ContactRationOffset
{
    F32  mDeflectYaw = 0.f;
    F32  mDeflectPitch = 0.f;
    F32  mWeight = 0.f;
    bool mAverting = false;
};

// Eyelid aperture follows vertical eye aim. Viewer eye pitch is positive when
// looking down, so upward looks naturally return zero closure. The caller may
// sample eye pitch slightly ahead in presentation time to model lid lead.
inline F32 lidFollowClosure(F32 eye_pitch, F32 gain)
{
    constexpr F32 FULL_FOLLOW_PITCH = 14.f * DEG_TO_RAD;
    return llclamp(llmax(eye_pitch, 0.f) / FULL_FOLLOW_PITCH, 0.f, 1.f) *
           llclamp(gain, 0.f, 1.f);
}

// Symmetric per-eye convergence for a point target. A scale of zero is useful
// for deliberately unfocused performance presets; negative scales are allowed
// for a subtle divergent look but remain bounded by the same anatomical cap.
inline F32 vergenceAngle(F32 target_distance, F32 ipd = 0.064f,
                         F32 scale = 1.f)
{
    if (!std::isfinite(target_distance) || target_distance <= 1e-4f ||
        !std::isfinite(ipd) || ipd <= 0.f || !std::isfinite(scale))
    {
        return 0.f;
    }
    constexpr F32 MAX_VERGENCE = 6.f * DEG_TO_RAD;
    return llclamp(atanf((ipd * 0.5f) / target_distance) * scale,
                   -MAX_VERGENCE, MAX_VERGENCE);
}

// ---------------------------------------------------------------------------
// Naturalism functions (§B1 - §B6)
// ---------------------------------------------------------------------------

// B6. Per-subject variation: auto-jitter intensity, smoothing, and cadences
inline void applySubjectVariation(U64 seed, F32 variation,
                                  const GazeLifeParams& in_params,
                                  GazeLifeParams& out_params,
                                  F32& intensity_scale,
                                  F32& smoothing_scale)
{
    const F32 v = llclamp(variation, 0.f, 1.f);
    out_params = in_params;
    if (v <= 1e-4f)
    {
        intensity_scale = 1.f;
        smoothing_scale = 1.f;
        return;
    }

    const F32 j_int   = (unitHash(seed, 10, 0, 0, 1) * 2.f - 1.f) * 0.15f * v;
    const F32 j_sm    = (unitHash(seed, 10, 0, 0, 2) * 2.f - 1.f) * 0.15f * v;
    const F32 j_react = (unitHash(seed, 10, 0, 0, 3) * 2.f - 1.f) * 0.20f * v;
    const F32 j_break = (unitHash(seed, 10, 0, 0, 4) * 2.f - 1.f) * 0.20f * v;

    intensity_scale = llclamp(1.f + j_int, 0.5f, 1.5f);
    smoothing_scale = llclamp(1.f + j_sm, 0.5f, 1.5f);
    out_params.mReactionMin = llmax(in_params.mReactionMin * (1.f + j_react), 0.01f);
    out_params.mReactionMax = llmax(in_params.mReactionMax * (1.f + j_react), out_params.mReactionMin + 0.05f);
    out_params.mBreakFrequency = llclamp(in_params.mBreakFrequency * (1.f + j_break), 0.f, 1.f);
}

// B2. Reaction latency
inline F32 evalReactionDelay(U64 seed, F32 min_delay, F32 max_delay)
{
    const F32 min_d = llmax(min_delay, 0.f);
    const F32 max_d = llmax(max_delay, min_d);
    const F32 h = unitHash(seed, 20, 0, 0, 1);
    return min_d + (max_d - min_d) * h;
}

// B5. Ease asymmetry
inline F32 asymmetricEnvStep(F32 env, bool active, F32 dt, F32 ease_acquire, F32 ease_release)
{
    if (active)
    {
        const F32 step = ease_acquire > 0.001f ? llmax(dt, 0.f) / ease_acquire : 1.f;
        return llclamp(env + step, 0.f, 1.f);
    }
    else
    {
        const F32 step = ease_release > 0.001f ? llmax(dt, 0.f) / ease_release : 1.f;
        return llclamp(env - step, 0.f, 1.f);
    }
}

// B1. Micro-life while locked: eye saccades, blinks, and slow head drift
inline MicroLifeOffsets evalMicroLife(U64 seed, F64 t_seconds, F32 micro_life,
                                      bool blinks_enabled, F32 blink_rate = 1.f)
{
    MicroLifeOffsets res;
    const F32 life = llclamp(micro_life, 0.f, 1.f);
    if (life <= 1e-4f || !std::isfinite(t_seconds) || t_seconds < 0.0)
    {
        return res;
    }

    // 1. Saccades: each subject has a stable phase and hashed cadence. The
    // previous value is held until a short smooth transition begins exactly at
    // the event boundary, so the function is continuous on both sides.
    const F64 saccade_interval = 0.36 +
        static_cast<F64>(unitHash(seed, 1, 0, 1, 0)) * 0.18;
    const F64 saccade_phase =
        static_cast<F64>(unitHash(seed, 1, 0, 1, 1)) * saccade_interval;
    const F64 saccade_time = t_seconds + saccade_phase;
    const U64 saccade_cell = static_cast<U64>(
        std::floor(saccade_time / saccade_interval));
    const F64 saccade_local = saccade_time -
        static_cast<F64>(saccade_cell) * saccade_interval;
    constexpr F64 FLICK_DUR = 0.06;
    F32 flick_alpha = 1.f;
    if (saccade_local < FLICK_DUR)
    {
        const F32 f = static_cast<F32>(saccade_local / FLICK_DUR);
        flick_alpha = f * f * (3.f - 2.f * f);
    }
    const F32 prev_yaw = (unitHash(seed, 1, saccade_cell > 0 ? saccade_cell - 1 : 0, 0, 1) * 2.f - 1.f);
    const F32 curr_yaw = (unitHash(seed, 1, saccade_cell, 0, 1) * 2.f - 1.f);
    const F32 prev_pitch = (unitHash(seed, 1, saccade_cell > 0 ? saccade_cell - 1 : 0, 0, 2) * 2.f - 1.f);
    const F32 curr_pitch = (unitHash(seed, 1, saccade_cell, 0, 2) * 2.f - 1.f);

    constexpr F32 MAX_SACCADE_RAD = 1.5f * DEG_TO_RAD;
    res.mEyeSaccadeYaw = (prev_yaw + (curr_yaw - prev_yaw) * flick_alpha) * MAX_SACCADE_RAD * life;
    res.mEyeSaccadePitch = (prev_pitch + (curr_pitch - prev_pitch) * flick_alpha) * (MAX_SACCADE_RAD * 0.7f) * life;

    // 2. Head drift: Lissajous phase sines (< 1.0 deg)
    constexpr F32 MAX_DRIFT_YAW_RAD = 0.8f * DEG_TO_RAD;
    constexpr F32 MAX_DRIFT_PITCH_RAD = 0.6f * DEG_TO_RAD;
    const F32 p1 = unitHash(seed, 2, 0, 0, 1) * 6.2831853f;
    const F32 p2 = unitHash(seed, 2, 0, 0, 2) * 6.2831853f;
    res.mHeadDriftYaw = sinf(static_cast<F32>(t_seconds * 0.23 * 6.2831853) + p1) * MAX_DRIFT_YAW_RAD * life;
    res.mHeadDriftPitch = sinf(static_cast<F32>(t_seconds * 0.37 * 6.2831853) + p2) * MAX_DRIFT_PITCH_RAD * life;

    // 3. Blinks / eye dips (~ every 3.5s to 6.0s per subject)
    if (blinks_enabled && blink_rate > 1e-4f)
    {
        const F64 blink_period = llclamp(
            4.5 / static_cast<F64>(blink_rate), 1.8, 18.0);
        const U64 blink_cycle = static_cast<U64>(std::floor(t_seconds / blink_period));
        const F64 cycle_time = t_seconds - static_cast<F64>(blink_cycle) * blink_period;
        const F64 blink_start = 0.2 + static_cast<F64>(
            unitHash(seed, 3, blink_cycle, 0, 1)) * llmax(blink_period - 0.5, 0.1);
        constexpr F64 BLINK_DUR = 0.14; // 140ms
        if (cycle_time >= blink_start && cycle_time <= blink_start + BLINK_DUR)
        {
            const F64 b_t = (cycle_time - blink_start) / BLINK_DUR;
            const F32 blink_shape = static_cast<F32>(4.0 * b_t * (1.0 - b_t));
            res.mBlinkFraction = blink_shape;
            constexpr F32 BLINK_DIP_RAD = 4.0f * DEG_TO_RAD;
            res.mBlinkDipPitch = -blink_shape * BLINK_DIP_RAD * life;
        }
    }

    return res;
}

// B3. Naturalized breaks within range: periodic glance away and return
inline void evalNaturalBreak(U64 seed, F64 t_seconds, F32 break_frequency,
                             F32& out_deflect_yaw, F32& out_deflect_pitch,
                             F32& out_break_weight)
{
    out_deflect_yaw = 0.f;
    out_deflect_pitch = 0.f;
    out_break_weight = 0.f;

    const F32 freq = llclamp(break_frequency, 0.f, 1.f);
    if (freq <= 1e-4f || !std::isfinite(t_seconds) || t_seconds < 0.0)
    {
        return;
    }

    const F64 avg_interval = 9.0 - 4.0 * static_cast<F64>(freq);
    const U64 cycle = static_cast<U64>(std::floor(t_seconds / avg_interval));
    const F64 cycle_t = t_seconds - static_cast<F64>(cycle) * avg_interval;

    const F32 trigger_roll = unitHash(seed, 4, cycle, 0, 0);
    if (trigger_roll > freq * 0.9f)
    {
        return;
    }

    const F64 break_start = 1.0 + static_cast<F64>(unitHash(seed, 4, cycle, 0, 1)) * (avg_interval - 2.5);
    const F64 break_dur = 0.6 + static_cast<F64>(unitHash(seed, 4, cycle, 0, 2)) * 0.6;

    if (cycle_t >= break_start && cycle_t <= break_start + break_dur)
    {
        const F64 norm_t = (cycle_t - break_start) / break_dur;
        const F32 curve = sinf(static_cast<F32>(norm_t * 3.14159265));
        out_break_weight = curve;

        const F32 yaw_dir = unitHash(seed, 4, cycle, 0, 3) > 0.5f ? 1.f : -1.f;
        const F32 raw_yaw_deg = 8.f + unitHash(seed, 4, cycle, 0, 4) * 6.f;
        const F32 raw_pitch_deg = -3.f + unitHash(seed, 4, cycle, 0, 5) * 8.f;

        out_deflect_yaw = curve * (raw_yaw_deg * DEG_TO_RAD * yaw_dir);
        out_deflect_pitch = curve * (raw_pitch_deg * DEG_TO_RAD);
    }
}

inline void evalNaturalBreak(U64 seed, F64 t_seconds, F32 break_frequency,
                             F32& out_deflect_yaw, F32& out_deflect_pitch)
{
    F32 ignored_weight = 0.f;
    evalNaturalBreak(seed, t_seconds, break_frequency,
                     out_deflect_yaw, out_deflect_pitch, ignored_weight);
}

// Persona-driven mandatory contact budget. Each fixed lattice cell contains a
// seeded contact hold followed by one short aversion. Cell selection and every
// offset are functions only of (seed, presentation time, persona), so seeking
// directly to a frame matches replaying into it.
inline ContactRationOffset evalContactRation(
    U64 seed, F64 t_seconds, const PersonaModulation& persona)
{
    ContactRationOffset out;
    const F32 strength = llclamp(persona.mContactAversionStrength, 0.f, 1.f);
    if (persona.mAlphaStare || strength <= 1e-4f ||
        !std::isfinite(t_seconds) || t_seconds < 0.0)
    {
        return out;
    }

    const F64 period = llclamp(
        7.0 * static_cast<F64>(persona.mContactHoldScale), 2.8, 19.0);
    const U64 cell = static_cast<U64>(std::floor(t_seconds / period));
    const F64 local = t_seconds - static_cast<F64>(cell) * period;
    const F64 aversion_duration = llclamp(
        0.60 + 0.65 * static_cast<F64>(persona.mDownwardAversion) +
        0.35 * static_cast<F64>(unitHash(seed, 5, cell, 0, 1)),
        0.55, period * 0.45);
    const F64 aversion_start = period - aversion_duration;
    if (local < aversion_start)
    {
        return out;
    }

    const F64 u = llclamp((local - aversion_start) / aversion_duration, 0.0, 1.0);
    out.mWeight =
        sinf(static_cast<F32>(u * 3.14159265358979323846)) * strength;
    out.mAverting = out.mWeight > 0.f;
    const F32 side = unitHash(seed, 5, cell, 0, 2) >= 0.5f ? 1.f : -1.f;
    const F32 yaw = (10.f + 6.f * unitHash(seed, 5, cell, 0, 3)) * DEG_TO_RAD;
    const F32 pitch = (8.f + 5.f * unitHash(seed, 5, cell, 0, 4)) * DEG_TO_RAD;
    out.mDeflectYaw = side * yaw * (1.f - persona.mDownwardAversion) * out.mWeight;
    out.mDeflectPitch = pitch * persona.mDownwardAversion * out.mWeight;
    return out;
}

// B4. Anatomical chain with limits: distribute required yaw/pitch across
// eyes -> head -> neck -> chest -> hips, recruiting each joint only after the
// preceding joint reaches its independently clamped capacity.
inline void distributeAnatomicalChain(F32 target_yaw, F32 target_pitch,
                                     F32 head_eye_blend, F32 torso_amount,
                                     F32 body_turn_threshold_deg,
                                     AnatomicalChainPose& out_pose,
                                     F32 anatomy_scale = 1.f)
{
    out_pose = AnatomicalChainPose();
    const F32 abs_yaw = fabsf(target_yaw);
    const F32 sign_yaw = target_yaw >= 0.f ? 1.f : -1.f;
    const F32 abs_pitch = fabsf(target_pitch);
    const F32 sign_pitch = target_pitch >= 0.f ? 1.f : -1.f;

    const F32 body_turn_thresh_rad = llclamp(body_turn_threshold_deg, 45.f, 180.f) * DEG_TO_RAD;
    if (abs_yaw > body_turn_thresh_rad)
    {
        out_pose.mTriggerBodyTurn = true;
    }

    constexpr F32 EYE_MAX_YAW = 25.f * DEG_TO_RAD;
    // Eyes reserve only as much vertical budget as they can actually deliver
    // (the eye-apply clamp is ~14 deg); over-reserving here starved the head of
    // pitch and made up/down look-at undershoot, worst off-axis. The head/neck
    // now carry the vertical, matching how people tilt to look up/down.
    constexpr F32 EYE_MAX_PITCH = 14.f * DEG_TO_RAD;
    constexpr F32 HEAD_MAX_YAW = 35.f * DEG_TO_RAD;
    constexpr F32 HEAD_MAX_PITCH = 42.f * DEG_TO_RAD;
    constexpr F32 NECK_MAX_YAW = 35.f * DEG_TO_RAD;
    constexpr F32 NECK_MAX_PITCH = 26.f * DEG_TO_RAD;
    constexpr F32 TORSO_MAX_YAW = 45.f * DEG_TO_RAD;
    constexpr F32 TORSO_MAX_PITCH = 20.f * DEG_TO_RAD;
    constexpr F32 HIPS_MAX_YAW = 35.f * DEG_TO_RAD;
    constexpr F32 HIPS_MAX_PITCH = 15.f * DEG_TO_RAD;

    // Exaggeration deliberately expands only the face/head chain. Chest and
    // hips retain their authored limits so this remains an anatomy control,
    // not a second torso-amount control. Keep the exact constants on the 1x
    // path so the default produces the pre-exaggeration result bit-for-bit.
    const F32 scale = std::isfinite(anatomy_scale)
        ? llclamp(anatomy_scale, 1.f, 3.f) : 1.f;
    const F32 eye_max_yaw = scale == 1.f
        ? EYE_MAX_YAW : EYE_MAX_YAW * scale;
    const F32 eye_max_pitch = scale == 1.f
        ? EYE_MAX_PITCH : EYE_MAX_PITCH * scale;
    const F32 head_max_yaw = scale == 1.f
        ? HEAD_MAX_YAW : HEAD_MAX_YAW * scale;
    const F32 head_max_pitch = scale == 1.f
        ? HEAD_MAX_PITCH : HEAD_MAX_PITCH * scale;
    const F32 neck_max_yaw = scale == 1.f
        ? NECK_MAX_YAW : NECK_MAX_YAW * scale;
    const F32 neck_max_pitch = scale == 1.f
        ? NECK_MAX_PITCH : NECK_MAX_PITCH * scale;

    const F32 blend = llclamp(head_eye_blend, 0.f, 1.f);
    const F32 torso_w = llclamp(torso_amount, 0.f, 1.f);

    if (blend <= 0.001f)
    {
        out_pose.mEyeYaw = sign_yaw * llmin(abs_yaw, eye_max_yaw);
        out_pose.mEyePitch = sign_pitch * llmin(abs_pitch, eye_max_pitch);
        return;
    }

    // Head/eyes is a low-angle distribution control: increasing it reduces
    // the eye-only capacity and opens the head/neck capacities. Torso likewise
    // scales the high-angle chest/hips capacity, but never recruits them early.
    const F32 eye_weight = 1.f - 0.75f * blend;
    const F32 head_weight = blend;
    const F32 torso_weight = blend * torso_w;

    auto allocate = [](F32& residual, F32 capacity) -> F32
    {
        const F32 amount = llmin(residual, llmax(capacity, 0.f));
        residual -= amount;
        return amount;
    };

    F32 rem_yaw = abs_yaw;
    out_pose.mEyeYaw = sign_yaw * allocate(rem_yaw, eye_max_yaw * eye_weight);
    out_pose.mHeadYaw = sign_yaw * allocate(rem_yaw, head_max_yaw * head_weight);
    out_pose.mNeckYaw = sign_yaw * allocate(rem_yaw, neck_max_yaw * head_weight);
    out_pose.mTorsoYaw = sign_yaw * allocate(rem_yaw, TORSO_MAX_YAW * torso_weight);
    out_pose.mHipsYaw = sign_yaw * allocate(rem_yaw, HIPS_MAX_YAW * torso_weight);

    F32 rem_pitch = abs_pitch;
    out_pose.mEyePitch = sign_pitch * allocate(rem_pitch, eye_max_pitch * eye_weight);
    out_pose.mHeadPitch = sign_pitch * allocate(rem_pitch, head_max_pitch * head_weight);
    out_pose.mNeckPitch = sign_pitch * allocate(rem_pitch, neck_max_pitch * head_weight);
    out_pose.mTorsoPitch = sign_pitch * allocate(rem_pitch, TORSO_MAX_PITCH * torso_weight);
    out_pose.mHipsPitch = sign_pitch * allocate(rem_pitch, HIPS_MAX_PITCH * torso_weight);
}

// Suspicious personas keep the combined head/neck yaw within about 15 degrees.
// The existing post-head eye solve automatically carries the residual, which is
// what gives side-eye its strained sockets without creating another eye path.
inline void applySideEye(F32 strength, AnatomicalChainPose& pose)
{
    const F32 weight = llclamp(strength, 0.f, 1.f);
    const F32 combined = pose.mHeadYaw + pose.mNeckYaw;
    if (weight <= 1e-4f || fabsf(combined) <= 1e-5f)
    {
        return;
    }

    constexpr F32 SIDE_EYE_HEAD_MAX = 15.f * DEG_TO_RAD;
    const F32 capped = llclamp(combined, -SIDE_EYE_HEAD_MAX, SIDE_EYE_HEAD_MAX);
    const F32 scale = capped / combined;
    pose.mHeadYaw += (pose.mHeadYaw * scale - pose.mHeadYaw) * weight;
    pose.mNeckYaw += (pose.mNeckYaw * scale - pose.mNeckYaw) * weight;
}

// ---------------------------------------------------------------------------
// Gaze-track macro envelopes. These are deliberately data-only, closed-form
// functions of cue-local presentation time. They carry no frame history, so a
// seek to t produces the same result as playing through t.
// ---------------------------------------------------------------------------
enum EGazeCueMacro : S32
{
    GAZE_MACRO_NONE = 0,
    GAZE_MACRO_DOUBLE_TAKE,
    GAZE_MACRO_BUTTON_LOOK,
    GAZE_MACRO_CREEP_TURN,
    GAZE_MACRO_OBJECT_GLANCE
};

enum EGazeCuePhase : S32
{
    GAZE_CUE_BEFORE = 0,
    GAZE_CUE_ACQUIRE,
    GAZE_CUE_HOLD,
    GAZE_CUE_RELEASE,
    GAZE_CUE_GAP
};

struct GazeMacroParams
{
    F32  mAcquireSec = 0.25f;
    F32  mReleaseSec = 0.60f;
    F64  mDurationSec = 0.0;       // zero means hold (except authored macros)
    F32  mGapSec = 0.35f;          // Double-Take comedy beat, clamped 0.2..0.5
    F32  mDwellSec = 0.60f;        // first glance / object dwell
    S32  mHoldFramesPast = 2;      // Button Look, fixed 30 fps edit-frame basis
    bool mThoughtResidue = false;  // Object Glance slower return
};

struct GazeMacroEnvelope
{
    // Blend from the previous/scene aim to the cue target. Double-Take may
    // briefly exceed one to extrapolate past the target for its overshoot.
    F32 mTargetWeight = 0.f;
    F32 mEyeWeight = 1.f;
    F32 mHeadWeight = 1.f;
    F32 mBodyWeight = 1.f;
    F32 mLidWiden = 0.f;
    F32 mHeadRecoilPitch = 0.f;
    EGazeCuePhase mPhase = GAZE_CUE_BEFORE;
};

inline F32 cueSmoothStep(F64 value)
{
    const F32 u = static_cast<F32>(llclamp(value, 0.0, 1.0));
    return u * u * (3.f - 2.f * u);
}

inline GazeMacroEnvelope evalGazeMacro(
    EGazeCueMacro macro, U64 seed, F64 local_time,
    const GazeMacroParams& raw_params)
{
    GazeMacroEnvelope out;
    if (!std::isfinite(local_time) || local_time < 0.0)
    {
        return out;
    }

    const F64 acquire = llmax(static_cast<F64>(raw_params.mAcquireSec), 0.0);
    const F64 release = llmax(static_cast<F64>(raw_params.mReleaseSec), 0.0);
    const F64 duration = llmax(raw_params.mDurationSec, 0.0);
    const F64 dwell = llmax(static_cast<F64>(raw_params.mDwellSec), 0.0);

    auto acquire_weight = [&](F64 t, F64 seconds) -> F32
    {
        return seconds > 1e-6 ? cueSmoothStep(t / seconds) : 1.f;
    };
    auto release_weight = [&](F64 t, F64 seconds) -> F32
    {
        return seconds > 1e-6 ? 1.f - cueSmoothStep(t / seconds) : 0.f;
    };

    switch (macro)
    {
        case GAZE_MACRO_DOUBLE_TAKE:
        {
            const F64 first_acquire = llmin(llmax(acquire, 0.08), 0.24);
            const F64 casual_release = llmax(release, 0.28);
            const F64 gap = llclamp(
                static_cast<F64>(raw_params.mGapSec), 0.20, 0.50);
            constexpr F64 SNAP_BACK = 0.075;
            constexpr F64 OVERSHOOT = 0.16;

            F64 cursor = first_acquire;
            if (local_time < cursor)
            {
                out.mTargetWeight = acquire_weight(local_time, first_acquire);
                out.mPhase = GAZE_CUE_ACQUIRE;
                return out;
            }
            cursor += dwell;
            if (local_time < cursor)
            {
                out.mTargetWeight = 1.f;
                out.mPhase = GAZE_CUE_HOLD;
                return out;
            }
            const F64 release_start = cursor;
            cursor += casual_release;
            if (local_time < cursor)
            {
                out.mTargetWeight = release_weight(
                    local_time - release_start, casual_release);
                out.mPhase = GAZE_CUE_RELEASE;
                return out;
            }
            cursor += gap;
            if (local_time < cursor)
            {
                out.mTargetWeight = 0.f;
                out.mPhase = GAZE_CUE_GAP;
                return out;
            }
            const F64 snap_start = cursor;
            cursor += SNAP_BACK;
            if (local_time < cursor)
            {
                out.mTargetWeight = acquire_weight(
                    local_time - snap_start, SNAP_BACK);
                out.mPhase = GAZE_CUE_ACQUIRE;
                return out;
            }
            const F64 overshoot_start = cursor;
            cursor += OVERSHOOT;
            if (local_time < cursor)
            {
                const F32 u = static_cast<F32>(
                    (local_time - overshoot_start) / OVERSHOOT);
                const F32 pulse = sinf(llclamp(u, 0.f, 1.f) * F_PI);
                const F32 subject_scale = 0.85f +
                    0.30f * unitHash(seed, 31, 0, 0, 1);
                out.mTargetWeight = 1.f + 0.12f * pulse;
                out.mLidWiden = 0.75f * pulse;
                out.mHeadRecoilPitch = -2.5f * DEG_TO_RAD *
                    subject_scale * pulse;
                out.mPhase = GAZE_CUE_ACQUIRE;
                return out;
            }

            // A positive duration is the authored end of the final hold. Never
            // truncate the fixed double-take sequence when it is shorter.
            const F64 hold_end = duration > 0.0 ? llmax(duration, cursor) : 0.0;
            if (hold_end > 0.0 && local_time >= hold_end)
            {
                out.mTargetWeight = release_weight(local_time - hold_end, release);
                out.mPhase = GAZE_CUE_RELEASE;
            }
            else
            {
                out.mTargetWeight = 1.f;
                out.mPhase = GAZE_CUE_HOLD;
            }
            return out;
        }

        case GAZE_MACRO_BUTTON_LOOK:
        {
            if (local_time < acquire)
            {
                out.mTargetWeight = acquire_weight(local_time, acquire);
                out.mPhase = GAZE_CUE_ACQUIRE;
                return out;
            }
            const F64 hold_past =
                static_cast<F64>(llmax(raw_params.mHoldFramesPast, 0)) / 30.0;
            const F64 hold_end = duration > 0.0 ? duration + hold_past : 0.0;
            if (hold_end > 0.0 && local_time >= hold_end)
            {
                out.mTargetWeight = release_weight(local_time - hold_end, release);
                out.mPhase = GAZE_CUE_RELEASE;
            }
            else
            {
                out.mTargetWeight = 1.f;
                out.mPhase = GAZE_CUE_HOLD;
            }
            return out;
        }

        case GAZE_MACRO_CREEP_TURN:
        {
            const F64 stretch = duration > 1e-6 ? duration : 3.0;
            out.mEyeWeight = cueSmoothStep(local_time / (stretch * 0.28));
            out.mHeadWeight = cueSmoothStep(
                (local_time - stretch * 0.18) / (stretch * 0.62));
            out.mBodyWeight = cueSmoothStep(
                (local_time - stretch * 0.62) / (stretch * 0.38));
            out.mTargetWeight = out.mEyeWeight;
            out.mPhase = local_time < stretch ? GAZE_CUE_ACQUIRE : GAZE_CUE_HOLD;
            return out;
        }

        case GAZE_MACRO_OBJECT_GLANCE:
        {
            if (local_time < acquire)
            {
                out.mTargetWeight = acquire_weight(local_time, acquire);
                out.mPhase = GAZE_CUE_ACQUIRE;
                return out;
            }
            const F64 release_start = acquire + dwell;
            if (local_time < release_start)
            {
                out.mTargetWeight = 1.f;
                out.mPhase = GAZE_CUE_HOLD;
                return out;
            }
            const F64 return_time = release *
                (raw_params.mThoughtResidue ? 2.25 : 1.0);
            out.mTargetWeight = release_weight(
                local_time - release_start, return_time);
            out.mPhase = GAZE_CUE_RELEASE;
            return out;
        }

        case GAZE_MACRO_NONE:
        default:
            break;
    }

    if (local_time < acquire)
    {
        out.mTargetWeight = acquire_weight(local_time, acquire);
        out.mPhase = GAZE_CUE_ACQUIRE;
    }
    else if (duration > 0.0 && local_time >= duration)
    {
        out.mTargetWeight = release_weight(local_time - duration, release);
        out.mPhase = GAZE_CUE_RELEASE;
    }
    else
    {
        out.mTargetWeight = 1.f;
        out.mPhase = GAZE_CUE_HOLD;
    }
    return out;
}

} // namespace ALGazeMath

#endif // AL_ALGAZEMATH_H
