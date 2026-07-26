/**
 * @file llcameraoperator.cpp
 * @brief Procedural handheld camera operator for the flycam (VCHH port).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix / Firestorm Viewer Source Code
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llcameraoperator.h"

#include <cmath>

#include "llviewercontrol.h"    // gSavedSettings, LLCachedControl
#include "llmath.h"             // DEG_TO_RAD, F_PI, F_TWO_PI, llclamp

// ---------------------------------------------------------------------------
// Constants (ported from VCHH v3.4)
// ---------------------------------------------------------------------------
namespace
{
constexpr F32 DT_REF        = 1.f / 30.f;   // authored per-frame retains map to 30 fps
constexpr F32 ONSET_DECAY   = 0.55f;        // per-frame retain @30fps
constexpr F32 ONSET_KICK    = 5.f;
constexpr F32 SETTLE_KICK   = 5.f;
constexpr F32 REACT_MAX     = 4.f;
constexpr F32 PHASE_WRAP    = 256.f;        // hash lattice period; wrap is seamless
// The shader authored reactive magnitudes in viewport fractions; one full
// viewport is roughly one radian of view angle, so fraction units convert
// to camera radians with a factor of ~1.
constexpr F32 FRAC_TO_RAD   = 1.0f;

inline F32 vc_frac(F32 x)                   { return x - floorf(x); }
inline F32 vc_lerp(F32 a, F32 b, F32 u)     { return a + (b - a) * u; }
inline F32 vc_sat(F32 x)                    { return llclamp(x, 0.f, 1.f); }
inline F32 vc_smoothstep(F32 e0, F32 e1, F32 x)
{
    F32 t = vc_sat((x - e0) / llmax(e1 - e0, 1e-6f));
    return t * t * (3.f - 2.f * t);
}
// quintic ease (C2), no overshoot
inline F32 vc_quintic(F32 u)                { return u * u * u * (u * (u * 6.f - 15.f) + 10.f); }

// frametime-normalized EMA retain: per-frame retain authored @30fps -> dt
inline F32 vc_retain(F32 perFrameRetain30, F32 dtS)
{
    F32 r   = llclamp(perFrameRetain30, 0.02f, 0.9995f);
    F32 tau = -DT_REF / logf(r);
    return expf(-dtS / llmax(tau, 1e-4f));
}

inline F32 vc_wrap(F32 x)                   { return x - floorf(x / PHASE_WRAP) * PHASE_WRAP; }

// ---------------------------------------------------------------------------
// value noise / fBm -- periodic lattice hash, quintic (C2) interpolation
// ---------------------------------------------------------------------------
F32 vc_hash(F32 p)
{
    p = p - floorf(p / PHASE_WRAP) * PHASE_WRAP;    // lattice period 256
    p = vc_frac(p * 0.1031f);
    p *= p + 33.33f;
    p *= p + p;
    return vc_frac(p);
}

F32 vc_vnoise5(F32 x)
{
    F32 i = floorf(x);
    F32 f = vc_frac(x);
    F32 u = vc_quintic(f);
    return vc_lerp(vc_hash(i), vc_hash(i + 1.f), u);
}

// integer octave ratios keep the wrapped phase continuous across the wrap
F32 vc_fbm(F32 x, F32 hiGain)
{
    F32 s =                  (vc_vnoise5(x)               - 0.5f);
    s    += 0.50f * hiGain * (vc_vnoise5(x * 2.f + 11.7f) - 0.5f);
    s    += 0.25f * hiGain * (vc_vnoise5(x * 4.f + 23.3f) - 0.5f);
    return s * 1.4f;
}

F32 vc_fbmSmooth(F32 x, F32 hiGain)
{
    F32 s =                  (vc_vnoise5(x)               - 0.5f);
    s    += 0.45f * hiGain * (vc_vnoise5(x * 2.f + 11.7f) - 0.5f);
    s    += 0.18f * hiGain * (vc_vnoise5(x * 3.f + 23.3f) - 0.5f);
    return s * 1.5f;
}

// sub-Hz sine weave; ratios are exact /256 rationals so seamless on wrap
F32 vc_weave(F32 tf, F32 seed)
{
    F32 s = 0.62f * sinf(tf * (0.625f    * F_TWO_PI) + seed);
    s    += 0.31f * sinf(tf * (1.0f      * F_TWO_PI) + seed * 1.7f + 1.3f);
    s    += 0.17f * sinf(tf * (0.390625f * F_TWO_PI) + seed * 0.6f + 4.1f);
    return s;
}

// sparse operator re-framing (per-window eased retarget)
void vc_recompose(F32 rp, F32 sd, F32 interval, F32& outX, F32& outY)
{
    F32 w  = floorf(rp);
    F32 k0 = w + sd * 0.37f;
    F32 px = vc_hash(k0 - 1.f)         - 0.5f;
    F32 py = vc_hash(k0 - 1.f + 91.7f) - 0.5f;
    F32 cx = vc_hash(k0)               - 0.5f;
    F32 cy = vc_hash(k0 + 91.7f)       - 0.5f;
    F32 fE = 0.15f + 0.55f * vc_hash(k0 + 47.3f);
    F32 blendW = 0.45f / llmax(interval, 0.5f);
    F32 u = vc_quintic(vc_sat((vc_frac(rp) - fE) / llmax(blendW, 0.02f)));
    outX = vc_lerp(px, cx, u);
    outY = vc_lerp(py, cy, u);
}

// ---------------------------------------------------------------------------
// Personality (Operator Style) -- ported verbatim from VCHH
// ---------------------------------------------------------------------------
struct Persona
{
    F32 ampMul, freqMul, hiMul, transMul, rollMul, breathMul;
    F32 energyGain, onsetGain, settleGain, drag, smoothing;
    F32 walkCouple, walkRate, motionCalm;
};

Persona getPersona(S32 s)
{
    Persona p;
    p.ampMul=1.f; p.freqMul=1.f; p.hiMul=1.f; p.transMul=1.f; p.rollMul=1.f; p.breathMul=1.f;
    p.energyGain=2.5f; p.onsetGain=1.6f; p.settleGain=2.f; p.drag=0.010f; p.smoothing=0.85f;
    p.walkCouple=0.f; p.walkRate=1.8f; p.motionCalm=0.4f;
    switch (s)
    {
        case 1: // Tripod / Sticks
            p.ampMul=0.15f; p.freqMul=0.6f; p.hiMul=0.2f; p.transMul=0.1f; p.rollMul=0.1f; p.breathMul=0.6f;
            p.energyGain=0.5f; p.onsetGain=0.5f; p.settleGain=1.f; p.drag=0.f; p.smoothing=0.92f;
            p.walkCouple=0.f; p.walkRate=1.8f; p.motionCalm=0.5f; break;
        case 2: // Subtle Handheld
            p.ampMul=0.5f; p.freqMul=0.8f; p.hiMul=0.4f; p.transMul=0.6f; p.rollMul=0.4f; p.breathMul=1.f;
            p.energyGain=1.5f; p.onsetGain=1.f; p.settleGain=1.6f; p.drag=0.006f; p.smoothing=0.88f;
            p.walkCouple=0.f; p.walkRate=1.7f; p.motionCalm=0.5f; break;
        case 3: // Documentary
            p.ampMul=1.f; p.freqMul=1.f; p.hiMul=1.f; p.transMul=1.f; p.rollMul=0.8f; p.breathMul=1.f;
            p.energyGain=2.2f; p.onsetGain=1.8f; p.settleGain=2.2f; p.drag=0.012f; p.smoothing=0.84f;
            p.walkCouple=2.5f; p.walkRate=1.8f; p.motionCalm=0.5f; break;
        case 4: // Run & Gun
            p.ampMul=1.7f; p.freqMul=1.5f; p.hiMul=1.8f; p.transMul=1.2f; p.rollMul=1.3f; p.breathMul=1.1f;
            p.energyGain=4.f; p.onsetGain=3.2f; p.settleGain=2.6f; p.drag=0.018f; p.smoothing=0.78f;
            p.walkCouple=4.f; p.walkRate=2.3f; p.motionCalm=0.2f; break;
        case 5: // Shoulder Rig
            p.ampMul=0.9f; p.freqMul=0.7f; p.hiMul=0.35f; p.transMul=1.f; p.rollMul=0.6f; p.breathMul=0.9f;
            p.energyGain=2.f; p.onsetGain=1.4f; p.settleGain=2.6f; p.drag=0.022f; p.smoothing=0.90f;
            p.walkCouple=1.8f; p.walkRate=1.5f; p.motionCalm=0.6f; break;
        case 6: // Gimbal Float
            p.ampMul=0.6f; p.freqMul=0.4f; p.hiMul=0.1f; p.transMul=0.3f; p.rollMul=0.2f; p.breathMul=0.8f;
            p.energyGain=0.8f; p.onsetGain=0.5f; p.settleGain=1.f; p.drag=0.004f; p.smoothing=0.94f;
            p.walkCouple=0.f; p.walkRate=1.8f; p.motionCalm=0.85f; break;
        case 7: // Verite (Chaotic)
            p.ampMul=1.3f; p.freqMul=1.3f; p.hiMul=1.5f; p.transMul=1.1f; p.rollMul=1.1f; p.breathMul=1.f;
            p.energyGain=3.f; p.onsetGain=2.8f; p.settleGain=1.8f; p.drag=0.014f; p.smoothing=0.80f;
            p.walkCouple=3.2f; p.walkRate=2.f; p.motionCalm=0.25f; break;
        case 8: // Breathing Only
            p.ampMul=1.f; p.freqMul=0.7f; p.hiMul=0.3f; p.transMul=0.f; p.rollMul=0.15f; p.breathMul=1.3f;
            p.energyGain=0.3f; p.onsetGain=0.3f; p.settleGain=0.6f; p.drag=0.f; p.smoothing=0.90f;
            p.walkCouple=0.f; p.walkRate=1.8f; p.motionCalm=0.5f; break;
        default: break; // 0 = Custom (neutral defaults above)
    }
    return p;
}

// ---------------------------------------------------------------------------
// Motion Profile -- ported verbatim from VCHH
// ---------------------------------------------------------------------------
struct Profile { F32 energyMul, onsetMul, settleMul, dragMul, walkMul, calmMul, smoothMul; };

Profile getProfile(S32 s)
{
    Profile q;
    q.energyMul=1.f; q.onsetMul=1.f; q.settleMul=1.f; q.dragMul=1.f; q.walkMul=1.f; q.calmMul=1.f; q.smoothMul=1.f;
    switch (s)
    {
        case 1: q.energyMul=0.f; q.onsetMul=0.f; q.settleMul=0.f; q.dragMul=0.f; q.walkMul=0.f; break; // Locked
        case 2: q.energyMul=0.8f; q.onsetMul=0.6f; q.settleMul=1.2f; q.dragMul=2.2f; q.walkMul=0.6f; q.calmMul=1.1f; q.smoothMul=1.05f; break; // Follow
        case 3: q.energyMul=1.5f; q.onsetMul=1.6f; q.settleMul=0.8f; q.dragMul=1.f;  q.walkMul=1.3f; q.calmMul=0.6f; q.smoothMul=0.9f;  break; // Restless
        case 4: q.energyMul=0.7f; q.onsetMul=0.5f; q.settleMul=1.9f; q.dragMul=0.9f; q.walkMul=0.8f; q.calmMul=1.4f; q.smoothMul=1.05f; break; // Composed
        case 5: q.energyMul=0.5f; q.onsetMul=0.3f; q.settleMul=0.9f; q.dragMul=0.5f; q.walkMul=0.4f; q.calmMul=1.3f; q.smoothMul=1.1f;  break; // Drift
        case 6: q.energyMul=1.2f; q.onsetMul=2.4f; q.settleMul=0.7f; q.dragMul=1.1f; q.walkMul=1.f;  q.calmMul=0.9f; q.smoothMul=0.85f; break; // Snap
        case 7: q.energyMul=1.f;  q.onsetMul=1.f;  q.settleMul=1.f;  q.dragMul=0.8f; q.walkMul=1.9f; q.calmMul=1.f;  q.smoothMul=1.f;   break; // March
        default: break; // 0 = Natural
    }
    return q;
}


// ---------------------------------------------------------------------------
// LOCOMOTION -- what the operator is physically DOING, as distinct from Persona
// (what RIG they are holding). A shoulder rig can be walking or driving, so the
// two axes are genuinely orthogonal.
//
// Locomotion REPLACES the Profile axis rather than stacking on top of it.
// Three multiplicative axes over the same ~40 settings produces corners nobody
// can debug or predict. Profile therefore stays live ONLY in LOCO_LEGACY, which
// is the default -- so behaviour is unchanged for anyone who never picks a mode.
//
// These are absolute physical targets, resolved BEFORE Persona and master
// intensity. They are deliberately NOT written into the user's settings: a mode
// must never overwrite authored tuning, or Auto would mutate persistent
// settings every time the camera changed speed and the UI could no longer tell
// authored values from generated ones.
// ---------------------------------------------------------------------------
enum ELocomotion
{
    LOCO_LEGACY = 0,    // Profile axis stays in charge; stock behaviour
    LOCO_CREEP,
    LOCO_WALK,
    LOCO_RUN,
    LOCO_DRIVE,
    LOCO_FLOAT,
    LOCO_UNSTEADY,
    LOCO_COUNT
};

struct Locomotion
{
    F32 idleAmp, panTiltFreq, rollFreq, hiContent;
    // NOTE: no tremorDamp here. hiContent replaces the ENTIRE tremor-damping
    // curve rather than scaling it, so a separate damping term would be dead
    // weight that the blend still had to carry.
    F32 smoothing, energyGain, onsetGain, settleGain, drag, motionCalm;
    F32 breathFreq, breathAmount;
    F32 walkCadence, cadenceDrive, fwdBias, latBias;
    F32 stepBob, lateralStep, stepRoll, gaitCouple;
    F32 recomposeAmt, recomposeInterval;
    // SLICE 2 -- carried so the tables are complete and the settings land in one
    // pass, but NOTHING READS THESE YET. They need new simulation layers.
    F32 suspHeave, suspFreq, roadBuzz, roadFreq, turnLean;
};

Locomotion getLocomotion(S32 m)
{
    // Physical intent, not vibes:
    //  Walk ~1.8-2 steps/s, cm-scale vertical displacement.
    //  Run  ~2.7-3 steps/s, several cm -- deliberately BELOW action-game bob.
    //  Drive zeroes gait entirely; a seated operator has no footfall. Its
    //       character comes from suspension heave + road buzz (slice 2).
    //  Creep damps everything EXCEPT breath, so breath reads proportionally
    //       more -- held tension rather than shake.
    //  Float removes gait and high-frequency content: cable cam, drone, vest.
    //  Unsteady is intoxication/panic/injury/dream POV. Its instability is
    //       low-frequency phase-offset sway and roll, NOT random shake.
    switch (m)
    {
        case LOCO_CREEP:    return { 0.32f, 0.34f, 0.18f, 0.18f,
                                     1.08f, 0.45f, 0.42f, 0.85f, 0.004f, 1.45f,
                                     0.20f, 1.80f,
                                     0.72f, 0.12f, 0.85f, 0.12f,
                                     0.004f, 0.003f, 0.10f, 0.65f,
                                     0.12f, 11.0f,
                                     0.000f, 0.00f, 0.0000f, 0.0f, 0.00f };
        case LOCO_WALK:     return { 0.85f, 0.62f, 0.34f, 0.65f,
                                     0.98f, 1.35f, 1.10f, 1.35f, 0.010f, 0.62f,
                                     0.25f, 1.25f,
                                     1.00f, 0.32f, 1.00f, 0.25f,
                                     0.017f, 0.009f, 0.38f, 1.35f,
                                     0.28f, 7.5f,
                                     0.000f, 0.00f, 0.0000f, 0.0f, 0.00f };
        case LOCO_RUN:      return { 1.55f, 1.05f, 0.72f, 1.45f,
                                     0.82f, 2.80f, 2.35f, 2.20f, 0.017f, 0.18f,
                                     0.38f, 1.65f,
                                     1.48f, 0.62f, 1.35f, 0.42f,
                                     0.043f, 0.021f, 0.92f, 2.60f,
                                     0.18f, 4.5f,
                                     0.000f, 0.00f, 0.0000f, 0.0f, 0.00f };
        case LOCO_DRIVE:    return { 0.48f, 0.78f, 1.35f, 1.70f,
                                     0.94f, 1.10f, 0.72f, 1.55f, 0.012f, 0.78f,
                                     0.22f, 0.55f,
                                     0.00f, 0.00f, 0.00f, 0.00f,
                                     0.000f, 0.000f, 0.00f, 0.00f,
                                     0.08f, 10.0f,
                                     0.018f, 1.15f, 0.0035f, 8.5f, 2.20f };
        case LOCO_FLOAT:    return { 0.28f, 0.18f, 0.12f, 0.06f,
                                     1.10f, 0.35f, 0.25f, 0.70f, 0.003f, 1.20f,
                                     0.18f, 0.70f,
                                     0.00f, 0.00f, 0.00f, 0.00f,
                                     0.000f, 0.000f, 0.00f, 0.00f,
                                     0.12f, 14.0f,
                                     0.004f, 0.22f, 0.0000f, 0.0f, 0.15f };
        case LOCO_UNSTEADY: return { 1.15f, 0.42f, 0.24f, 0.28f,
                                     1.04f, 0.90f, 0.65f, 1.20f, 0.009f, 0.45f,
                                     0.21f, 2.10f,
                                     0.45f, 0.08f, 0.25f, 0.12f,
                                     0.006f, 0.012f, 0.75f, 0.35f,
                                     0.42f, 5.5f,
                                     0.006f, 0.38f, 0.0000f, 0.0f, 1.40f };
        default: break;
    }
    // LOCO_LEGACY has no block -- callers must not ask for one. Neutral values
    // so a future miscall degrades to "stock-ish" rather than to zero.
    return { 1.f, 0.6f, 0.3f, 1.f,
             1.f, 1.f, 1.f, 1.f, 0.010f, 1.f,
             0.25f, 1.5f,
             1.f, 0.25f, 1.f, 0.25f,
             0.015f, 0.008f, 0.35f, 1.f,
             0.35f, 7.f,
             0.f, 0.f, 0.f, 0.f, 0.f };
}

// Blend two blocks field by field. Written out EXPLICITLY rather than looping
// over the struct as raw floats: a reinterpret_cast walk assumes every member
// is an F32 with no padding, which no assertion can actually guarantee, and a
// future non-float field would be silently interpolated as though its bit
// pattern were a float. Verbose, but adding a field forces a decision here
// instead of failing quietly.
Locomotion lerpLocomotion(const Locomotion& a, const Locomotion& b, F32 t)
{
    Locomotion r;
#define VC_LERP_FIELD(f) r.f = vc_lerp(a.f, b.f, t)
    VC_LERP_FIELD(idleAmp);      VC_LERP_FIELD(panTiltFreq);
    VC_LERP_FIELD(rollFreq);     VC_LERP_FIELD(hiContent);
    VC_LERP_FIELD(smoothing);
    VC_LERP_FIELD(energyGain);   VC_LERP_FIELD(onsetGain);
    VC_LERP_FIELD(settleGain);   VC_LERP_FIELD(drag);
    VC_LERP_FIELD(motionCalm);   VC_LERP_FIELD(breathFreq);
    VC_LERP_FIELD(breathAmount); VC_LERP_FIELD(walkCadence);
    VC_LERP_FIELD(cadenceDrive); VC_LERP_FIELD(fwdBias);
    VC_LERP_FIELD(latBias);      VC_LERP_FIELD(stepBob);
    VC_LERP_FIELD(lateralStep);  VC_LERP_FIELD(stepRoll);
    VC_LERP_FIELD(gaitCouple);   VC_LERP_FIELD(recomposeAmt);
    VC_LERP_FIELD(recomposeInterval);
    VC_LERP_FIELD(suspHeave);    VC_LERP_FIELD(suspFreq);
    VC_LERP_FIELD(roadBuzz);     VC_LERP_FIELD(roadFreq);
    VC_LERP_FIELD(turnLean);
#undef VC_LERP_FIELD
    return r;
}


// Transition state for the locomotion cross-fade. File scope rather than
// members because Locomotion is a private type in this translation unit and
// LLCameraOperator is a singleton, so the two are equivalent.
//   sModeSource : the block a transition started FROM
//   sModeLive   : the block actually produced last frame
Locomotion sModeSource = getLocomotion(LOCO_LEGACY);
Locomotion sModeLive   = getLocomotion(LOCO_LEGACY);

} // anonymous namespace

// ---------------------------------------------------------------------------
LLCameraOperator& LLCameraOperator::instance()
{
    static LLCameraOperator sInstance;
    return sInstance;
}

void LLCameraOperator::reset()
{
    mSpeed = 0.f;
    mVecX = mVecY = 0.f;
    mOnsetEnv = mSettleEnv = 0.f;
    mLatchX = mLatchY = 0.f;

    // Phases are reset ONLY under a locomotion mode.
    //
    // Zeroing them makes takes repeatable: replaying the same recorded path
    // with the same seed otherwise started at a different point in the noise,
    // so a re-shoot did not match the take it was meant to match. reset() only
    // fires at cuts, flycam toggles, playback starts and CineCam mode/target
    // changes -- all already discontinuities -- so nothing pops.
    //
    // But it IS a behaviour change, and LEGACY promises stock behaviour. Legacy
    // therefore keeps the original "leave the phases running" semantics, and
    // determinism arrives with the locomotion feature the director opted into.
    static LLCachedControl<S32> reset_loco(gSavedSettings, "FlycamOperatorLocomotionMode", 0);
    if (reset_loco != LOCO_LEGACY)
    {
        mPhaseXY = mPhaseRoll = mPhaseBreath = 0.f;
        mPhaseGait = mPhaseSettle = mPhaseRecompose = 0.f;
    }

    mModeBlend = 1.f;
    mModeCurrent = -1;
    sModeSource = sModeLive = getLocomotion(LOCO_LEGACY);
}

LLCameraOperatorOutput LLCameraOperator::update(const LLCameraOperatorInput& input)
{
    LLCameraOperatorOutput out;

    // ---- settings (live) --------------------------------------------------
    static LLCachedControl<S32> style(gSavedSettings, "FlycamOperatorStyle", 3);
    static LLCachedControl<S32> profile(gSavedSettings, "FlycamOperatorProfile", 0);
    static LLCachedControl<F32> profileInfluence(gSavedSettings, "FlycamOperatorProfileInfluence", 0.7f);
    static LLCachedControl<F32> master(gSavedSettings, "FlycamOperatorMaster", 1.f);
    static LLCachedControl<F32> reactivity(gSavedSettings, "FlycamOperatorReactivity", 1.f);
    static LLCachedControl<F32> idleIntensity(gSavedSettings, "FlycamOperatorIdleIntensity", 1.f);
    static LLCachedControl<F32> tremorDamping(gSavedSettings, "FlycamOperatorTremorDamping", 0.6f);
    static LLCachedControl<F32> transFreq(gSavedSettings, "FlycamOperatorPanTiltFreq", 0.6f);
    static LLCachedControl<F32> panAmount(gSavedSettings, "FlycamOperatorPanAmount", 0.6f);        // deg
    static LLCachedControl<S32> lateralMode(gSavedSettings, "FlycamOperatorPanMode", 0);
    static LLCachedControl<F32> lateralSmoothness(gSavedSettings, "FlycamOperatorPanSmoothness", 0.6f);
    static LLCachedControl<F32> lateralPanBoost(gSavedSettings, "FlycamOperatorPanReactSmoothing", 0.5f);
    static LLCachedControl<F32> tiltAmount(gSavedSettings, "FlycamOperatorTiltAmount", 0.6f);      // deg
    static LLCachedControl<F32> rollFreq(gSavedSettings, "FlycamOperatorRollFreq", 0.3f);
    static LLCachedControl<F32> rollAmount(gSavedSettings, "FlycamOperatorRollAmount", 0.5f);      // deg
    static LLCachedControl<F32> rollSmoothness(gSavedSettings, "FlycamOperatorRollDamping", 0.65f);
    static LLCachedControl<F32> breathFreq(gSavedSettings, "FlycamOperatorBreathFreq", 0.25f);
    static LLCachedControl<F32> breathAmount(gSavedSettings, "FlycamOperatorBreathAmount", 1.5f);  // %
    static LLCachedControl<F32> breathLift(gSavedSettings, "FlycamOperatorBreathLift", 0.2f);
    static LLCachedControl<F32> recomposeAmount(gSavedSettings, "FlycamOperatorRecomposeAmount", 0.35f); // deg
    static LLCachedControl<F32> recomposeInterval(gSavedSettings, "FlycamOperatorRecomposeInterval", 7.f);
    static LLCachedControl<F32> timeSpeed(gSavedSettings, "FlycamOperatorTimeSpeed", 1.f);
    static LLCachedControl<F32> seed(gSavedSettings, "FlycamOperatorSeed", 0.f);
    // reactive dynamics
    static LLCachedControl<F32> motionPan(gSavedSettings, "FlycamOperatorMotionPan", 1.f);
    static LLCachedControl<F32> motionTilt(gSavedSettings, "FlycamOperatorMotionTilt", 1.f);
    static LLCachedControl<F32> motionRoll(gSavedSettings, "FlycamOperatorMotionRoll", 1.f);
    static LLCachedControl<F32> motionBreath(gSavedSettings, "FlycamOperatorMotionBreath", 1.f);
    static LLCachedControl<F32> onsetAmount(gSavedSettings, "FlycamOperatorOnset", 1.f);
    static LLCachedControl<F32> dragAmount(gSavedSettings, "FlycamOperatorDrag", 1.f);
    static LLCachedControl<F32> settleAmount(gSavedSettings, "FlycamOperatorSettle", 1.f);
    static LLCachedControl<F32> settleFreq(gSavedSettings, "FlycamOperatorSettleFreq", 8.f);
    static LLCachedControl<F32> settleDecay(gSavedSettings, "FlycamOperatorSettleDecay", 0.85f);
    // walk & gait
    static LLCachedControl<bool> forceWalk(gSavedSettings, "FlycamOperatorForceWalk", false);
    static LLCachedControl<F32> walkCadence(gSavedSettings, "FlycamOperatorWalkCadence", 1.f);
    static LLCachedControl<F32> cadenceDrive(gSavedSettings, "FlycamOperatorCadenceDrive", 0.25f);
    static LLCachedControl<F32> forwardWalkBias(gSavedSettings, "FlycamOperatorForwardWalkBias", 1.f);
    static LLCachedControl<F32> lateralWalkBias(gSavedSettings, "FlycamOperatorLateralWalkBias", 0.25f);
    static LLCachedControl<F32> stepBob(gSavedSettings, "FlycamOperatorStepBob", 0.015f);          // m
    static LLCachedControl<F32> lateralStep(gSavedSettings, "FlycamOperatorLateralStep", 0.008f);  // m
    static LLCachedControl<F32> stepRoll(gSavedSettings, "FlycamOperatorStepRoll", 0.35f);         // deg
    // advanced
    static LLCachedControl<F32> energyTrim(gSavedSettings, "FlycamOperatorEnergyTrim", 1.f);
    static LLCachedControl<F32> motionCalmTrim(gSavedSettings, "FlycamOperatorMotionCalm", 1.f);
    static LLCachedControl<F32> smoothingTrim(gSavedSettings, "FlycamOperatorSmoothing", 1.f);
    static LLCachedControl<F32> gaitCoupling(gSavedSettings, "FlycamOperatorGaitCoupling", 1.f);
    // normalization references for the ground-truth speed metric
    static LLCachedControl<F32> refLinear(gSavedSettings, "FlycamOperatorRefLinearSpeed", 3.f);    // m/s
    static LLCachedControl<F32> refAngular(gSavedSettings, "FlycamOperatorRefAngularSpeed", 60.f); // deg/s

    // ---- locomotion + per-DOF authority (live) ----------------------------
    static LLCachedControl<S32> locoMode(gSavedSettings, "FlycamOperatorLocomotionMode", 0);
    static LLCachedControl<F32> modeBlendTime(gSavedSettings, "FlycamOperatorModeBlendTime", 0.85f);
    static LLCachedControl<F32> gainSurge(gSavedSettings, "FlycamOperatorGainSurge", 1.f);
    static LLCachedControl<F32> gainSway(gSavedSettings,  "FlycamOperatorGainSway",  1.f);
    static LLCachedControl<F32> gainHeave(gSavedSettings, "FlycamOperatorGainHeave", 1.f);
    static LLCachedControl<F32> gainRoll(gSavedSettings,  "FlycamOperatorGainRoll",  1.f);
    static LLCachedControl<F32> gainPitch(gSavedSettings, "FlycamOperatorGainPitch", 1.f);
    static LLCachedControl<F32> gainYaw(gSavedSettings,   "FlycamOperatorGainYaw",   1.f);
    static LLCachedControl<F32> gainFov(gSavedSettings,   "FlycamOperatorGainFOV",   1.f);

    const Persona P = getPersona(llclamp((S32)style, 0, 8));
    const Profile Q = getProfile(llclamp((S32)profile, 0, 7));
    const F32 infl = vc_sat(profileInfluence);
    auto vc_infl = [infl](F32 m) { return vc_lerp(1.f, m, infl); };

    // Locomotion REPLACES Profile. LOCO_LEGACY (0, the default) leaves Profile
    // in charge, so existing users see no change whatsoever.
    const S32 loco = llclamp((S32)locoMode, 0, (S32)LOCO_COUNT - 1);
    const bool loco_active = (loco != LOCO_LEGACY);

    // Cross-fade the parameter block on a mode change so switching mid-shot
    // does not snap. Quintic smoothstep: zero first and second derivative at
    // both ends, so the transition has no visible velocity discontinuity.
    if (loco_active && loco != mModeCurrent)
    {
        // Source is the block we were ACTUALLY producing last frame, not the
        // previous mode's raw table. Retargeting mid-blend (Walk->Run, then
        // Run->Float halfway) must continue from the live mixture, or the
        // parameters snap back to an unblended table on the frame the second
        // change lands.
        sModeSource = (mModeCurrent >= 0) ? sModeLive : getLocomotion(loco);
        mModeCurrent = loco;
        mModeBlend = 0.f;
    }
    else if (!loco_active)
    {
        // LEGACY is a HARD CUT, deliberately: there is no locomotion block to
        // blend from or to -- Profile is a different axis with different
        // semantics, and synthesising a pseudo-block for it would be inventing
        // numbers. Documented in the setting's Comment.
        mModeCurrent = -1;
        mModeBlend = 1.f;
    }

    Locomotion L = getLocomotion(loco);
    if (loco_active && mModeBlend < 1.f)
    {
        const F32 blend_time = llmax((F32)modeBlendTime, 0.01f);
        mModeBlend = vc_sat(mModeBlend + input.mDeltaTime / blend_time);
        const F32 s = mModeBlend;
        const F32 smooth = s * s * s * (s * (s * 6.f - 15.f) + 10.f);
        L = lerpLocomotion(sModeSource, L, smooth);
    }
    if (loco_active)
    {
        sModeLive = L;   // what we actually produced, for a mid-blend retarget
    }

    const F32 dt = llclamp(input.mDeltaTime, 0.0005f, 0.25f);

    // ---- ground-truth speed metric (replaces MV estimation + AGC) ---------
    const F32 linRef = llmax((F32)refLinear, 0.01f);
    const F32 angRef = llmax((F32)refAngular, 1.f) * DEG_TO_RAD;

    // pan-plane flow equivalent: yaw+lateral => X, pitch+vertical => Y
    const F32 vx = input.mAngularVel.mV[VZ] / angRef + input.mLinearVel.mV[VY] / linRef;
    const F32 vy = input.mAngularVel.mV[VY] / angRef + input.mLinearVel.mV[VZ] / linRef;
    const F32 coherentRaw  = sqrtf(vx * vx + vy * vy);
    const F32 divergentRaw = fabsf(input.mLinearVel.mV[VX]) / linRef;
    const F32 rawSpd = llmin(coherentRaw + divergentRaw, REACT_MAX);

    // ---- framerate-independent smoothing -----------------------------------
    // Locomotion supplies an ABSOLUTE smoothing target; Profile supplies a
    // MULTIPLIER. Hence the branch rather than one blended expression -- mixing
    // an absolute and a relative term is exactly the combinatorial mush this
    // design set out to avoid.
    const F32 k = vc_retain(P.smoothing *
                            (loco_active ? L.smoothing : vc_infl(Q.smoothMul)) *
                            smoothingTrim, dt);
    const F32 prevSpeed = mSpeed;
    mSpeed = vc_lerp(rawSpd, mSpeed, k);
    mVecX  = vc_lerp(vx, mVecX, k);
    mVecY  = vc_lerp(vy, mVecY, k);

    // ---- onset / settle envelopes ------------------------------------------
    const F32 accelN = (mSpeed - prevSpeed) * (DT_REF / dt);
    mOnsetEnv  = vc_sat(llmax(mOnsetEnv  * vc_retain(ONSET_DECAY, dt),  llmax(accelN, 0.f)  * ONSET_KICK));
    mSettleEnv = vc_sat(llmax(mSettleEnv * vc_retain(settleDecay, dt),  llmax(-accelN, 0.f) * SETTLE_KICK));

    // latched motion direction (settle bounce axis)
    const F32 coherent = sqrtf(mVecX * mVecX + mVecY * mVecY);
    if (coherent > 0.06f)
    {
        const F32 aDir = 1.f - expf(-dt / 0.15f);
        mLatchX = vc_lerp(mLatchX, mVecX / coherent, aDir);
        mLatchY = vc_lerp(mLatchY, mVecY / coherent, aDir);
    }

    // ---- phase accumulators (wrap-safe, pop-free on slider changes) --------
    const F32 divergent = llmax(mSpeed - coherent, 0.f);
    const F32 adv = dt * llmax((F32)timeSpeed, 0.f);
    // Locomotion supplies absolute rates; LEGACY keeps the authored settings.
    const F32 aCadence   = loco_active ? L.walkCadence  : (F32)walkCadence;
    const F32 aCadDrive  = loco_active ? L.cadenceDrive : (F32)cadenceDrive;
    const F32 aPanTiltHz = loco_active ? L.panTiltFreq  : (F32)transFreq;
    const F32 aRollHz    = loco_active ? L.rollFreq     : (F32)rollFreq;
    const F32 aBreathHz  = loco_active ? L.breathFreq   : (F32)breathFreq;
    const F32 aRecompInt = loco_active ? L.recomposeInterval : (F32)recomposeInterval;

    const F32 gaitRate = P.walkRate * aCadence * (1.f + vc_sat(divergent) * aCadDrive);
    mPhaseXY        = vc_wrap(mPhaseXY        + adv * aPanTiltHz * P.freqMul);
    mPhaseRoll      = vc_wrap(mPhaseRoll      + adv * aRollHz    * P.freqMul);
    mPhaseBreath    = vc_wrap(mPhaseBreath    + adv * aBreathHz  * P.freqMul);
    mPhaseGait      = vc_wrap(mPhaseGait      + adv * gaitRate);
    mPhaseSettle    = vc_wrap(mPhaseSettle    + adv * settleFreq);
    mPhaseRecompose = vc_wrap(mPhaseRecompose + adv / llmax(aRecompInt, 0.5f));

    // ---- reactive gains -----------------------------------------------------
    const F32 R = reactivity;
    // Locomotion replaces the Profile term in each coefficient. Persona (the
    // RIG) still multiplies, because a shoulder rig walking and a gimbal
    // walking really are different -- that is the one axis pairing that stays
    // meaningful. See the ELocomotion comment block.
    const F32 eEnergy = P.energyGain * (loco_active ? L.energyGain : vc_infl(Q.energyMul)) * R * energyTrim;
    const F32 eOnset  = P.onsetGain  * (loco_active ? L.onsetGain  : vc_infl(Q.onsetMul))  * R * onsetAmount  * 0.012f * FRAC_TO_RAD;
    const F32 eSettle = P.settleGain * (loco_active ? L.settleGain : vc_infl(Q.settleMul)) * R * settleAmount * 0.010f * FRAC_TO_RAD;
    // Drag is an absolute physical quantity in the locomotion table (not a
    // multiplier), so it substitutes for P.drag as well.
    const F32 eDrag   = (loco_active ? L.drag : P.drag * vc_infl(Q.dragMul)) * R * dragAmount * FRAC_TO_RAD;
    const F32 eWalk   = P.walkCouple * (loco_active ? L.gaitCouple : vc_infl(Q.walkMul)) * R * gaitCoupling;
    const F32 eCalm   = (loco_active ? L.motionCalm : P.motionCalm * vc_infl(Q.calmMul)) * motionCalmTrim;

    const F32 react = llmin(1.f + mSpeed * eEnergy, REACT_MAX);

    F32 mdirX = 0.f, mdirY = 0.f;
    if (coherent > 1e-4f) { mdirX = mVecX / coherent; mdirY = mVecY / coherent; }

    // direction-confidence gate (smoothed vec ramps from zero at onset)
    const F32 dirConf = vc_smoothstep(0.03f, 0.15f, coherent);
    const F32 whipX = mdirX * mOnsetEnv * eOnset * dirConf;
    const F32 whipY = mdirY * mOnsetEnv * eOnset * dirConf;
    const F32 dragX = -mdirX * coherent * eDrag;
    const F32 dragY = -mdirY * coherent * eDrag;
    const F32 settleW = mSettleEnv * eSettle;

    F32 sDirX = 0.f, sDirY = 1.f;   // settle axis fallback: vertical
    const F32 lLen = sqrtf(mLatchX * mLatchX + mLatchY * mLatchY);
    if (lLen > 0.05f) { sDirX = mLatchX / lLen; sDirY = mLatchY / lLen; }

    const F32 panLat = vc_sat(fabsf(mVecX) * 1.5f);

    F32 gait = 0.f;
    const F32 aFwdBias = loco_active ? L.fwdBias : (F32)forwardWalkBias;
    const F32 aLatBias = loco_active ? L.latBias : (F32)lateralWalkBias;
    const F32 gaitDrive = (divergent * aFwdBias + coherent * aLatBias) * eWalk;
    gait = vc_smoothstep(0.12f, 0.85f, gaitDrive);
    if (forceWalk) gait = llmax(gait, 1.f);

    // ---- idle tremor character ----------------------------------------------
    const F32 calm = vc_sat(mSpeed * eCalm);
    // hiContent is an absolute high-frequency weight in the locomotion table,
    // so it substitutes for the tremor-damping curve rather than scaling it.
    const F32 hi     = (loco_active ? L.hiContent
                                    : vc_lerp(1.4f, 0.2f, vc_sat(tremorDamping)))
                       * P.hiMul * (1.f - calm);
    const F32 hiRoll = hi * (1.f - vc_sat(rollSmoothness) * 0.85f);
    const F32 latSmooth = vc_sat(lateralSmoothness + panLat * lateralPanBoost * (1.f - lateralSmoothness));
    const F32 hiX = hi * (1.f - latSmooth * 0.85f);
    const F32 sd = seed * 13.f;

    F32 sx;
    if ((S32)lateralMode == 1)      sx = vc_weave(mPhaseXY, sd);
    else if ((S32)lateralMode == 2) sx = vc_fbm(mPhaseXY + sd, hi);
    else                            sx = vc_fbmSmooth(mPhaseXY + sd, hiX);

    const F32 sy  = vc_fbm(mPhaseXY   + sd + 31.4f, hi);
    const F32 sr  = vc_fbm(mPhaseRoll + sd + 57.1f, hiRoll);
    const F32 sbz = vc_fbm(mPhaseBreath + sd + 83.9f, hi);

    const F32 aIdle = loco_active ? L.idleAmp : (F32)idleIntensity;
    const F32 mIdle = master * aIdle * P.ampMul;

    // per-axis reactive amplification of idle wander
    const F32 reactX = 1.f + (react - 1.f) * motionPan;
    const F32 reactY = 1.f + (react - 1.f) * motionTilt;
    const F32 reactR = 1.f + (react - 1.f) * 0.5f * motionRoll;   // roll half-weighted
    const F32 reactB = 1.f + (react - 1.f) * motionBreath;

    // idle wander is true camera rotation (deg settings -> radians)
    // NOTE: panAmount/tiltAmount/rollAmount stay AUTHORED even under a
    // locomotion mode. The table's idleAmp already sets how much the mode
    // moves (via mIdle); these three remain the director's framing preference
    // for how that budget is split across the axes.
    F32 yaw   = sx * panAmount  * DEG_TO_RAD * P.transMul * mIdle * reactX;
    F32 pitch = sy * tiltAmount * DEG_TO_RAD * P.transMul * mIdle * reactY;
    F32 roll  = sr * rollAmount * DEG_TO_RAD * P.rollMul  * mIdle * reactR;

    // breathing: true FOV pulse + coupled chest-rise translation
    const F32 aBreathAmt = loco_active ? L.breathAmount : (F32)breathAmount;
    const F32 breath = sbz * (aBreathAmt * 0.01f) * P.breathMul * mIdle * reactB;
    out.mFovMul = 1.f + breath;
    F32 liftZ = breath * breathLift * 0.15f;    // meters of chest rise

    // sparse recompose nudges (eased retarget of the framing center)
    const F32 aRecompAmt = loco_active ? L.recomposeAmt : (F32)recomposeAmount;
    if (aRecompAmt > 1e-4f)
    {
        F32 rcx, rcy;
        vc_recompose(mPhaseRecompose, sd, aRecompInt, rcx, rcy);
        const F32 rcScale = 2.f * aRecompAmt * DEG_TO_RAD * P.transMul * master * aIdle;
        yaw   += rcx * rcScale;
        pitch += rcy * rcScale;
    }

    // additive reactive rotation, gated per axis
    yaw   += (whipX + dragX) * master * motionPan;
    pitch += (whipY + dragY) * master * motionTilt;

    // settle bounce along the axis of the motion that stopped
    const F32 wob = sinf(mPhaseSettle * F_TWO_PI) * settleW;
    yaw   += wob * sDirX * master * motionPan;
    pitch += wob * sDirY * master * motionTilt;
    roll  += wob * 1.5f * (0.4f + 0.6f * fabsf(sDirX)) * master * motionRoll;

    // ---- gait: real head translation (figure-8) -----------------------------
    F32 bobZ = 0.f, swayY = 0.f;
    if (gait > 0.001f)
    {
        const F32 foot   = mPhaseGait * F_TWO_PI;
        const F32 stride = mPhaseGait * F_PI;
        const F32 vbase  = -cosf(foot);
        const F32 vshape = vbase * (0.78f + 0.22f * vc_sat(-vbase));
        const F32 hbase  = sinf(stride);
        // Drive and Float zero these outright -- a seated or flying operator
        // has no footfall -- which is why they must come from the table and not
        // from the user's authored gait settings.
        const F32 aStepBob   = loco_active ? L.stepBob     : (F32)stepBob;
        const F32 aLatStep   = loco_active ? L.lateralStep : (F32)lateralStep;
        const F32 aStepRoll  = loco_active ? L.stepRoll    : (F32)stepRoll;
        bobZ  = -vshape * aStepBob * gait * master;         // up axis (Z)
        swayY =  hbase * aLatStep * gait * master;          // left axis (Y)
        roll += hbase * aStepRoll * DEG_TO_RAD * gait * master;
        out.mFovMul += vbase * aStepBob * 0.15f * gait;
    }

    // ---- per-DOF authority, applied BEFORE the clamps ----------------------
    // Deliberately not fed back into the simulation: scaling an INPUT changes
    // the character of the motion, because envelopes, latching and gait
    // coupling all react to it. A director asking for "no roll" means "remove
    // the roll I can see", not "re-simulate as though the operator never
    // rolled".
    //
    // But it goes BEFORE the clamps, not after: the clamps are safety limits
    // that downstream code relies on, and a gain of 2 applied afterwards would
    // silently produce 20 degrees of rotation and a metre of translation past
    // caps of 10 degrees and 0.5 m. Gains exaggerate within the envelope; they
    // do not raise the ceiling.
    yaw   *= gainYaw;
    pitch *= gainPitch;
    roll  *= gainRoll;
    swayY *= gainSway;
    bobZ  *= gainHeave;
    liftZ *= gainHeave;
    // FOV is a MULTIPLIER, so it scales about 1.0 -- scaling it directly would
    // drive the FOV toward zero rather than toward neutral. Bypassed entirely
    // at unity so the untouched path stays bit-identical.
    if (gainFov != 1.f)
    {
        out.mFovMul = 1.f + (out.mFovMul - 1.f) * gainFov;
    }

    // ---- outputs, with sanity clamps (no overscan needed: real camera) ------
    const F32 rotCap = 10.f * DEG_TO_RAD;
    out.mYaw   = llclamp(yaw,   -rotCap, rotCap);
    out.mPitch = llclamp(pitch, -rotCap, rotCap);
    out.mRoll  = llclamp(roll,  -rotCap, rotCap);
    out.mFovMul = llclamp(out.mFovMul, 0.8f, 1.25f);
    out.mPosOffset = LLVector3(0.f,
                               llclamp(swayY, -0.5f, 0.5f),
                               llclamp(bobZ + liftZ, -0.5f, 0.5f));

    // ---- unused-gain note --------------------------------------------------
    // FINAL output gains, applied after the clamps. Deliberately NOT fed back
    // into the simulation: scaling an input would change the character of the
    // motion (envelopes, latching, gait coupling all react to it), whereas the
    // director asking for "no roll" means exactly "remove the roll I am seeing",
    // not "re-simulate as though the operator never rolled".
    //
    // 0 is a hard disable; values above 1 exaggerate on purpose. FOV is scaled
    // about 1.0 because it is a multiplier, not an offset -- scaling it directly
    // would drive the FOV toward zero rather than toward neutral.
    // Surge has no producer yet: the simulation always emits X = 0, so the
    // control is structurally in place but cannot do anything until a
    // surge-generating layer exists (suspension/road buzz, slice 2). Referenced
    // here so the setting is not silently dead and the compiler does not warn.
    (void)gainSurge;

    return out;
}
