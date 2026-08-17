/**
 * @file alcinelightrigmodel.h
 * @brief Pure deterministic model for the client-side cinematic light rig.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_CINE_LIGHT_RIG_MODEL_H
#define AL_CINE_LIGHT_RIG_MODEL_H

#include "stdtypes.h"

namespace ALCineLightRigModel
{
constexpr S32 LIGHT_COUNT = 4;
constexpr S32 PROFILE_COUNT = 24;
constexpr S32 BEAM_COUNT = 3;
constexpr S32 GOBO_COUNT = 8;
constexpr S32 GEL_COUNT = 15;
constexpr S32 FX_COUNT = 63;
constexpr S32 GROUP_MAX_MEMBERS = 5;
constexpr F32 PITCH_LIMIT_DEG = 85.f;
constexpr F32 MIN_RADIUS = 0.5f;
constexpr U64 DEFAULT_SEED = 0x13579bdfULL;
// Numerically paired with GHOST_SCALE_MIN/MAX in alghoststudio.h. The pure
// model deliberately cannot include that viewer-side header.
constexpr F32 SUBJECT_SCALE_MIN = 0.05f;
constexpr F32 SUBJECT_SCALE_MAX = 150.f;
constexpr F32 SCALED_RADIUS_FLOOR = 0.1f;
// Largest orbit whose authored 2.2x projector reach fits the 20 m primitive
// light-radius clamp; the primitive clamp itself is intentionally unchanged.
constexpr F32 SCALED_RADIUS_CEIL = 20.f / 2.2f;
constexpr F32 MASTER_TEMP_MIRED_MIN = -110.f;
constexpr F32 MASTER_TEMP_MIRED_MAX = 150.f;
constexpr F32 MASTER_TEMP_PIVOT_KELVIN = 6500.f;

struct LightBase
{
    F32  mYawDeg = 0.f;
    F32  mPitchDeg = 0.f;
    S32  mProfile = 0;
    F32  mEV = 0.f;
    S32  mBeam = 0;
    bool mOn = false;
    S32  mGobo = 0;
    S32  mGel = 0;
};

struct Setup
{
    F32       mRadius = 1.5f;
    LightBase mLights[LIGHT_COUNT];
    bool      mRatioLock = false;
    F32       mRatioStops = 0.f;
};

struct Transforms
{
    bool mMirror = false;
    F32  mFacingAzimuthDeg = 0.f;
    F32  mYawDeg = 0.f;
    F32  mPitchDeg = 0.f;
};

struct Globals
{
    F32  mMasterEV = 0.f;
    F32  mHeadroomStops = 2.f;
    F32  mBounceRatio = 0.45f;
    F32  mTransitionSec = 0.9f;
    bool mBounceEnabled = true;
    bool mPower = true;
    U64  mSeed = DEFAULT_SEED;
    // Rig geometry scale; for a group this is at least the largest member's
    // sanitized subject scale and also carries the group's spatial spread.
    F32  mSubjectScale = 1.f;
    F32  mMasterTempMired = 0.f;
};

struct EmitterState
{
    F32  mOffX = 0.f;
    F32  mOffY = 0.f;
    F32  mOffZ = 0.f;
    F32  mAimX = 0.f;
    F32  mAimY = 0.f;
    F32  mAimZ = -1.f;
    F32  mSR = 1.f;
    F32  mSG = 1.f;
    F32  mSB = 1.f;
    F32  mIntensity = 0.f;
    F32  mLightRadius = 0.f;
    F32  mFalloff = 1.f;
    F32  mFovRad = 1.5f;
    bool mOn = false;
    bool mClipped = false;
    S32  mGobo = 0;
};

struct RigFrame
{
    EmitterState mProj[LIGHT_COUNT];
    EmitterState mOmni[LIGHT_COUNT];
};

Setup sanitizeSetup(const Setup& setup);
Transforms sanitizeTransforms(const Transforms& transforms);
Globals sanitizeGlobals(const Globals& globals);
F32 sanitizeSubjectScale(F32 scale);

// Easy Mode is a UI macro over the existing exposure settings. These pure
// helpers are the single source of truth for its write values and read-back
// buckets; they deliberately do not alter rendering or setup evaluation.
bool easyRimOn(S32 presence);
F32 easyRimEV(S32 presence);
bool easyBgOn(S32 presence);
F32 easyBgEV(S32 presence);
S32 rimPresenceFromEV(bool on, F32 ev);
S32 bgPresenceFromEV(bool on, F32 ev);
F32 easyBrightnessClamp(F32 ev);
F32 easyDramaClamp(F32 stops);

void groupBoundsCentre(const F32 points[][3], S32 count,
                       F32 out_centre[3]);
F32 groupSubjectScale(const F32 points[][3], const F32 member_scales[],
                      S32 count, const F32 centre[3], F32 nominal_radius);

F32 wrap180(F32 degrees);
F32 ease(F32 t);
void computeLive(const Setup& setup, const Transforms& transforms,
                 LightBase out[LIGHT_COUNT]);
LightBase blendLight(const LightBase& start, const LightBase& target,
                     F32 eased);
F32 intensityFromEV(F32 ev_total, F32 headroom_stops, bool* clipped);
void masterTempGain(F32 mired_shift, F32 gain[3]);
// Applies a named gel to linear RGB in place. Gel 0 returns without touching
// the three channels, preserving the exact no-gel path.
void applyGel(S32 index, F32 linear_rgb[3]);
const char* gelName(S32 index);
bool gelIsColourTemperature(S32 index);
F32 gelMiredShift(S32 index);
// Screen-plane offset used by the catchlight placement. Both the view-axis
// distance and this radial offset scale with the finalized subject scale.
void catchlightRadialOffset(F32 subject_scale, F32 angle_degrees,
                            F32 out_right_up[2]);
// Radius is nominal setup geometry. Subject scale changes spatial output only;
// the distance-derived EV term always continues to read the nominal radius.
void render(F32 radius, const LightBase live[LIGHT_COUNT],
            const Globals& globals, RigFrame& out);
// Invisible emitter-box edge for the effective/nominal orbit-radius ratio.
F32 emitterBoxEdgeFromRatio(F32 effective_to_nominal_radius);

void evalFX(S32 fx, U64 seed, F64 t_seconds,
            LightBase out[LIGHT_COUNT]);

const char* fxName(S32 fx);
F32 fxInterval(S32 fx);
const char* profileName(S32 index);
void profileSRGB(S32 index, F32 rgb[3]);
const char* beamName(S32 index);
F32 beamFov(S32 index);
F32 beamFalloff(S32 index);
const char* goboName(S32 index);

Setup classicSetup();
}

#endif // AL_CINE_LIGHT_RIG_MODEL_H
