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

#include <string>
#include <vector>

namespace ALCineLightRigModel
{
constexpr S32 LIGHT_COUNT = 4;
constexpr S32 PROFILE_COUNT = 24;
constexpr S32 BEAM_COUNT = 3;
constexpr S32 GOBO_COUNT = 24;
constexpr S32 GOBO_BLUR_BUCKET_COUNT = 3;
constexpr S32 GOBO_CATEGORY_COUNT = 5;
constexpr S32 GOBO_ROTATING_FAN = 22;
constexpr S32 GEL_COUNT = 15;
constexpr S32 FIXTURE_GEL_COUNT = 19;
constexpr S32 FIXTURE_GEL_SLOT_COUNT = 3;
constexpr S32 FIXTURE_PRESET_COUNT = 13;
constexpr S32 FX_COUNT = 63;
enum FlickerProgram : S32
{
    FLICKER_NONE = 0,
    FLICKER_FIRELIGHT,
    FLICKER_TV_MONITOR,
    FLICKER_FLUORESCENT,
    FLICKER_NEON,
    FLICKER_CANDLE,
    FLICKER_PASSING_HEADLIGHTS,
    FLICKER_POLICE_LIGHTBAR,
    FLICKER_COUNT,
};
constexpr F32 FLICKER_INTENSITY_MUL_MAX = 1.5f;
constexpr F32 FLICKER_COLOR_MUL_MAX = 1.25f;
constexpr S32 GROUP_MAX_MEMBERS = 5;
constexpr F32 PITCH_LIMIT_DEG = 85.f;
constexpr F32 MIN_RADIUS = 0.5f;
constexpr U64 DEFAULT_SEED = 0x13579bdfULL;
// Numerically paired with GHOST_SCALE_MIN/MAX in alghoststudio.h. The pure
// model deliberately cannot include that viewer-side header.
constexpr F32 SUBJECT_SCALE_MIN = 0.05f;
constexpr F32 SUBJECT_SCALE_MAX = 150.f;
// Half-height of the nominal two-metre avatar used by object framing.
constexpr F32 AVATAR_REF_RADIUS = 1.f;
constexpr F32 SCALED_RADIUS_FLOOR = 0.1f;
// Largest orbit whose authored 2.2x projector reach fits the 20 m primitive
// light-radius clamp; the primitive clamp itself is intentionally unchanged.
constexpr F32 SCALED_RADIUS_CEIL = 20.f / 2.2f;
constexpr F32 MASTER_TEMP_MIRED_MIN = -110.f;
constexpr F32 MASTER_TEMP_MIRED_MAX = 150.f;
constexpr F32 MASTER_TEMP_PIVOT_KELVIN = 6500.f;
constexpr F32 FIXTURE_KELVIN_MIN = 2000.f;
constexpr F32 FIXTURE_KELVIN_MAX = 20000.f;
constexpr F32 FIXTURE_SOURCE_SIZE_MIN = 0.01f;
constexpr F32 FIXTURE_SOURCE_SIZE_MAX = 4.f;

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
    S32  mFlickerProgram = FLICKER_NONE;
    F32  mFlickerAmount = 0.f;
    // Fixture mode is additive. False preserves the complete 1.x
    // profile/single-gel/shadow-softness path.
    bool mFixtureMode = false;
    F32  mKelvin = 5600.f;
    S32  mGelSlot[FIXTURE_GEL_SLOT_COUNT] = {};
    F32  mSourceSizeM = 0.10f;
    S32  mFixturePreset = 0;
};

struct FixturePreset
{
    const char* mName = "";
    F32 mKelvin = 5600.f;
    F32 mSourceSizeM = 0.10f;
    S32 mBeam = 0;
    S32 mSuggestedGels[FIXTURE_GEL_SLOT_COUNT] = {};
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

enum CueFadeProfile : S32
{
    CUE_FADE_EASE = 0,
    CUE_FADE_LINEAR,
    CUE_FADE_SNAP,
    CUE_FADE_PROFILE_COUNT,
};

struct Cue
{
    std::string mLabel;
    F32 mFadeSec = 3.f;
    F32 mDelaySec = 0.f;
    S32 mProfile = CUE_FADE_EASE;
    // Seconds after delay+fade before automatically advancing; -1 disables.
    S32 mFollow = -1;
    // Absolute presentation time, used only in timecode mode.
    F64 mAtSec = 0.0;
    Setup mSetup;
    Globals mGlobals;
    Transforms mTransforms;
    F32 mShadowSoftOverride[LIGHT_COUNT] = { -1.f, -1.f, -1.f, -1.f };
    S32 mFX = -1;
    std::string mProvenance;
};

struct CueList
{
    std::string mName;
    bool mTimecodeMode = false;
    std::vector<Cue> mCues;
};

struct CueState
{
    Setup mSetup;
    Globals mGlobals;
    Transforms mTransforms;
    F32 mShadowSoftOverride[LIGHT_COUNT] = { -1.f, -1.f, -1.f, -1.f };
    S32 mFX = -1;
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
    // Used only when the corresponding live light is in fixture mode.
    F32 mDerivedShadowSoftness[LIGHT_COUNT] = {};
    // The key fixture drives the single camera-side catchlight. Legacy and
    // non-key fixtures leave the existing authored size unchanged.
    F32 mCatchlightSizeScale = 1.f;
};

Setup sanitizeSetup(const Setup& setup);
Transforms sanitizeTransforms(const Transforms& transforms);
Globals sanitizeGlobals(const Globals& globals);
F32 sanitizeSubjectScale(F32 scale);
F32 objectRadiusToSubjectScale(F32 radius_m);

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
// Transition easing curve. mode: 0 Linear, 1 Smoothstep,
// 2 Ease-In-Out cubic (default), 3 Ease-Out cubic.
F32 ease(F32 t, U32 mode = 2);
void computeLive(const Setup& setup, const Transforms& transforms,
                 LightBase out[LIGHT_COUNT]);
LightBase blendLight(const LightBase& start, const LightBase& target,
                     F32 eased);
Globals blendGlobals(const Globals& start, const Globals& target, F32 weight);
Transforms blendTransforms(const Transforms& start, const Transforms& target,
                           F32 weight);
Cue sanitizeCue(const Cue& cue);
CueList sanitizeCueList(const CueList& list);
CueState cueTargetState(const Cue& cue);
F32 cueFadeWeight(S32 profile, F64 presentation_time, F64 go_time,
                  F32 delay_sec, F32 fade_sec);
CueState evaluateCueTransition(const CueState& start, const Cue& target,
                               F64 presentation_time, F64 go_time);
S32 timecodeCueIndex(const CueList& list, F64 presentation_time);
CueState evaluateTimecodeCueList(const CueList& list,
                                 const CueState& released_state,
                                 F64 presentation_time,
                                 S32* active_index = nullptr);
F32 intensityFromEV(F32 ev_total, F32 headroom_stops, bool* clipped);
void masterTempGain(F32 mired_shift, F32 gain[3]);
// Applies a named gel to linear RGB in place. Gel 0 returns without touching
// the three channels, preserving the exact no-gel path.
void applyGel(S32 index, F32 linear_rgb[3]);
const char* gelName(S32 index);
bool gelIsColourTemperature(S32 index);
F32 gelMiredShift(S32 index);
// Fixture gels are separate from the byte-stable legacy GELS table.
const char* fixtureGelName(S32 index);
bool fixtureGelIsColourTemperature(S32 index);
F32 fixtureGelMiredShift(S32 index);
void fixtureWhite(F32 kelvin,
                  const S32 gel_slots[FIXTURE_GEL_SLOT_COUNT],
                  F32 master_mired, F32 out_linear_rgb[3]);
const FixturePreset& fixturePreset(S32 index);
const char* fixturePresetName(S32 index);
F32 penumbraSoftness(F32 source_size_m, F32 distance_m);
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
// Per-light practical flicker is a final, scrub-safe modulation layer. The
// caller derives one seed per light, then multiplies the rendered intensity
// and colour by these bounded outputs. Amount zero and FLICKER_NONE are exact
// identity paths.
U64 flickerLightSeed(U64 rig_seed, S32 light_index);
void evalFlicker(S32 program, U64 light_seed, F64 t_seconds, F32 amount,
                 F32& out_intensity_mul, F32 out_color_mul[3]);
const char* flickerProgramName(S32 program);

const char* fxName(S32 fx);
F32 fxInterval(S32 fx);
const char* profileName(S32 index);
void profileSRGB(S32 index, F32 rgb[3]);
const char* beamName(S32 index);
F32 beamFov(S32 index);
F32 beamFalloff(S32 index);
// Easy-mode cone width is ordered narrow-to-wide even though the authored
// beam presets are stored Standard, Softbox, Snoot.
S32 easyConeWidthToBeam(S32 width);
S32 easyConeWidthFromBeam(S32 beam);
const char* goboName(S32 index);
// UI grouping and the authored pre-blur lookup are pure so the panel and
// controller cannot disagree about category or softness thresholds.
S32 goboCategory(S32 index);
const char* goboCategoryName(S32 category);
S32 goboBlurBucket(F32 softness);
bool goboIsAnimated(S32 index);

Setup classicSetup();
}

#endif // AL_CINE_LIGHT_RIG_MODEL_H
