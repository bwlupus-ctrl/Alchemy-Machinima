/**
 * @file alcinelightrigmodel.cpp
 * @brief Pure deterministic model for the client-side cinematic light rig.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "alcinelightrigmodel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace ALCineLightRigModel
{
namespace
{
constexpr F32 DEFAULT_RADIUS = 1.5f;
constexpr F32 MAX_RADIUS = 512.f;
constexpr F32 MIN_EV = -20.f;
constexpr F32 MAX_EV = 20.f;
constexpr F32 MAX_MASTER_EV = 16.f;
constexpr F32 MAX_HEADROOM = 16.f;
constexpr F32 MAX_TRANSITION = 10.f;
constexpr F64 MAX_FX_SECONDS = 1.0e12;
constexpr F64 TWO_PI = 6.283185307179586476925286766559;
constexpr F32 DEGREES_TO_RADIANS =
    static_cast<F32>(TWO_PI / 360.0);

struct Profile
{
    const char* mName;
    F32 mR;
    F32 mG;
    F32 mB;
};

struct Beam
{
    const char* mName;
    F32 mFov;
    F32 mFalloff;
};

const Profile PROFILES[PROFILE_COUNT] = {
    { "2700K Incan", 1.00f, 0.55f, 0.15f },
    { "3200K Tung", 1.00f, 0.92f, 0.72f },
    { "4500K Neut", 1.00f, 1.00f, 0.95f },
    { "5600K Day", 0.85f, 0.92f, 1.00f },
    { "6500K Cool", 0.70f, 0.85f, 1.00f },
    { "8000K Moon", 0.45f, 0.55f, 1.00f },
    { "10000K Sky", 0.30f, 0.40f, 1.00f },
    { "Full CTO", 1.00f, 0.65f, 0.05f },
    { "Full CTB", 0.30f, 0.60f, 1.00f },
    { "Plus Green", 0.60f, 1.00f, 0.60f },
    { "Magenta", 1.00f, 0.20f, 0.80f },
    { "Rosco Red", 1.00f, 0.02f, 0.02f },
    { "Congo Blue", 0.05f, 0.05f, 1.00f },
    { "Deep Amber", 1.00f, 0.40f, 0.00f },
    { "Emerald", 0.00f, 1.00f, 0.10f },
    { "Cyber Pink", 1.00f, 0.05f, 0.80f },
    { "Sci-Fi Cyan", 0.05f, 0.85f, 1.00f },
    { "Golden Hour", 1.00f, 0.40f, 0.05f },
    { "Vaporwave", 0.80f, 0.00f, 1.00f },
    { "Blood Red", 0.60f, 0.00f, 0.00f },
    { "Deep Space", 0.00f, 0.00f, 0.20f },
    { "Toxic Waste", 0.20f, 1.00f, 0.00f },
    { "Neon Purple", 0.50f, 0.00f, 1.00f },
    { "Pure White", 1.00f, 1.00f, 1.00f },
};

const Beam BEAMS[BEAM_COUNT] = {
    { "Standard", 1.5f, 1.0f },
    { "Softbox", 2.8f, 1.5f },
    { "Snoot", 0.2f, 0.5f },
};

const char* const GOBO_NAMES[GOBO_COUNT] = {
    "Default", "Venetian Blinds", "Window Panes", "Prison Bars",
    "Slats", "Grid", "Soft Dapple", "Branches",
};

const char* FX_NAMES[FX_COUNT] = {
    "Police Sirens", "Club Strobe", "Fire Flicker", "Streetlight",
    "Neon Pulse", "Paparazzi", "TV Screen", "Underwater",
    "UFO Abduction", "Haunted Flicker", "RGB Gamer", "Disco Ball",
    "Warning Alert", "Matrix Drop", "Thunderstorm", "Searchlight",
    "Heartbeat", "Movie Projector", "Warp Tunnel", "Fairy Woods",
    "Short Circuit", "Red Alert Pulse", "Aurora Borealis",
    "Cyber Scanner", "Shooting Star", "Elevator Fault", "Car Pass",
    "Explosion", "Supernova", "Stage Debut", "Swinging Lamp",
    "Parachute Flare", "Villain Reveal",
};

const F32 FX_INTERVALS[FX_COUNT] = {
    0.15f, 0.10f, 0.20f, 0.20f, 0.10f, 0.10f, 0.20f, 0.20f,
    0.10f, 0.10f, 0.20f, 0.10f, 0.10f, 0.15f, 0.05f, 0.10f,
    0.10f, 0.10f, 0.05f, 0.20f, 0.05f, 0.10f, 0.20f, 0.10f,
    0.05f, 0.10f, 0.07f, 0.08f, 0.08f, 0.15f, 0.07f, 0.12f,
    0.12f,
};

F32 finiteOr(F32 value, F32 fallback)
{
    return std::isfinite(value) ? value : fallback;
}

F64 finiteOr(F64 value, F64 fallback)
{
    return std::isfinite(value) ? value : fallback;
}

F32 clampPitch(F32 value)
{
    return std::clamp(finiteOr(value, 0.f),
                      -PITCH_LIMIT_DEG, PITCH_LIMIT_DEG);
}

LightBase cleanLight(const LightBase& input)
{
    LightBase output;
    std::memset(&output, 0, sizeof(output));
    output.mYawDeg = wrap180(finiteOr(input.mYawDeg, 0.f));
    output.mPitchDeg = clampPitch(input.mPitchDeg);
    output.mProfile = std::clamp(input.mProfile, 0, PROFILE_COUNT - 1);
    output.mEV = std::clamp(finiteOr(input.mEV, 0.f), MIN_EV, MAX_EV);
    output.mBeam = std::clamp(input.mBeam, 0, BEAM_COUNT - 1);
    output.mOn = input.mOn;
    output.mGobo = std::clamp(input.mGobo, 0, GOBO_COUNT - 1);
    return output;
}

// These are model-local copies of the standard sRGB transfer functions used
// by llmath.h. The pure model intentionally does not depend on viewer math.
F32 srgbChannelToLinear(F32 value)
{
    return value < 0.04045f
        ? value / 12.92f
        : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

F32 linearChannelToSRGB(F32 value)
{
    return value <= 0.0031308f
        ? value * 12.92f
        : 1.055f * std::pow(value, 1.f / 2.4f) - 0.055f;
}

void planckianLinearRGB(F64 kelvin, F64 rgb[3])
{
    const F64 t2 = kelvin * kelvin;
    const F64 t3 = t2 * kelvin;
    const F64 x = kelvin <= 4000.0
        ? -0.2661239e9 / t3 - 0.2343589e6 / t2 +
              0.8776956e3 / kelvin + 0.179910
        : -3.0258469e9 / t3 + 2.1070379e6 / t2 +
              0.2226347e3 / kelvin + 0.240390;
    const F64 x2 = x * x;
    const F64 x3 = x2 * x;
    F64 y;
    if (kelvin <= 2222.0)
    {
        y = -1.1063814 * x3 - 1.34811020 * x2 +
            2.18555832 * x - 0.20219683;
    }
    else if (kelvin <= 4000.0)
    {
        y = -0.9549476 * x3 - 1.37418593 * x2 +
            2.09137015 * x - 0.16748867;
    }
    else
    {
        y = 3.0817580 * x3 - 5.87338670 * x2 +
            3.75112997 * x - 0.37001483;
    }

    const F64 xyz_x = x / y;
    const F64 xyz_z = (1.0 - x - y) / y;
    rgb[0] = std::max(0.0,
        3.2406 * xyz_x - 1.5372 - 0.4986 * xyz_z);
    rgb[1] = std::max(0.0,
        -0.9689 * xyz_x + 1.8758 + 0.0415 * xyz_z);
    rgb[2] = std::max(0.0,
        0.0557 * xyz_x - 0.2040 + 1.0570 * xyz_z);
}

F64 positiveFmod(F64 value, F64 modulus)
{
    if (!std::isfinite(value) || !std::isfinite(modulus) || modulus <= 0.0)
    {
        return 0.0;
    }
    F64 result = std::fmod(value, modulus);
    return result < 0.0 ? result + modulus : result;
}

F32 phaseSin(F64 phase)
{
    return static_cast<F32>(std::sin(positiveFmod(phase, TWO_PI)));
}

F32 phaseCos(F64 phase)
{
    return static_cast<F32>(std::cos(positiveFmod(phase, TWO_PI)));
}

U64 splitMix64(U64 value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

F32 unitHash(U64 seed, S32 fx, U64 counter, S32 light, S32 draw)
{
    U64 key = seed ? seed : DEFAULT_SEED;
    key = splitMix64(key ^ (0xd1b54a32d192ed03ULL * (U64)(fx + 1)));
    key = splitMix64(key ^ (0x9e3779b97f4a7c15ULL * (counter + 1)));
    key = splitMix64(key ^ (0x94d049bb133111ebULL * (U64)(light + 2)));
    key = splitMix64(key ^ (0xbf58476d1ce4e5b9ULL * (U64)(draw + 3)));
    return static_cast<F32>((key >> 40) * (1.0 / 16777216.0));
}

S64 virtualStep(F64 seconds, F32 interval)
{
    const F64 safe_time = std::clamp(finiteOr(seconds, 0.0),
                                     0.0, MAX_FX_SECONDS);
    return static_cast<S64>(std::floor(safe_time / (F64)interval));
}

S64 positiveMod(S64 value, S64 modulus)
{
    const S64 result = value % modulus;
    return result < 0 ? result + modulus : result;
}

F32 valueNoise(U64 seed, S32 fx, F64 coordinate, S32 light, S32 draw)
{
    coordinate = std::clamp(finiteOr(coordinate, 0.0), 0.0,
                            MAX_FX_SECONDS * 20.0);
    const U64 cell = static_cast<U64>(std::floor(coordinate));
    F32 fraction = static_cast<F32>(coordinate - std::floor(coordinate));
    fraction = fraction * fraction * (3.f - 2.f * fraction);
    const F32 a = unitHash(seed, fx, cell, light, draw);
    const F32 b = unitHash(seed, fx, cell + 1, light, draw);
    return a + (b - a) * fraction;
}

void setLight(LightBase lights[LIGHT_COUNT], S32 index,
              F32 yaw, F32 pitch, S32 profile, F32 ev,
              S32 beam, bool on)
{
    LightBase& light = lights[index];
    light.mYawDeg = yaw;
    light.mPitchDeg = pitch;
    light.mProfile = profile;
    light.mEV = ev;
    light.mBeam = beam;
    light.mOn = on;
}

void copyCleanLights(const LightBase input[LIGHT_COUNT],
                     LightBase output[LIGHT_COUNT])
{
    std::memset(output, 0, sizeof(LightBase) * LIGHT_COUNT);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        const LightBase safe = cleanLight(input[i]);
        output[i].mYawDeg = safe.mYawDeg;
        output[i].mPitchDeg = safe.mPitchDeg;
        output[i].mProfile = safe.mProfile;
        output[i].mEV = safe.mEV;
        output[i].mBeam = safe.mBeam;
        output[i].mOn = safe.mOn;
        output[i].mGobo = safe.mGobo;
    }
}

void initializeFX(S32 fx, LightBase lights[LIGHT_COUNT])
{
    std::memset(lights, 0, sizeof(LightBase) * LIGHT_COUNT);
    switch (fx)
    {
    case 0:
        setLight(lights, 0, 45.f, 10.f, 11, 0.f, 0, true);
        setLight(lights, 1, -45.f, 10.f, 12, 0.f, 0, true);
        break;
    case 1:
        setLight(lights, 0, 45.f, 30.f, 0, 0.f, 0, true);
        setLight(lights, 1, -45.f, 30.f, 0, 0.f, 0, true);
        setLight(lights, 2, 135.f, 30.f, 0, 0.f, 0, true);
        setLight(lights, 3, -135.f, 30.f, 0, 0.f, 0, true);
        break;
    case 2:
        setLight(lights, 0, 20.f, -20.f, 13, 0.f, 1, true);
        setLight(lights, 1, -20.f, -15.f, 17, 0.f, 1, true);
        break;
    case 3:
        setLight(lights, 0, 0.f, 0.f, 3, 0.f, 1, true);
        break;
    case 4:
        setLight(lights, 0, 45.f, 15.f, 15, 0.f, 1, true);
        setLight(lights, 1, -45.f, 15.f, 16, 0.f, 1, true);
        break;
    case 5:
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            setLight(lights, i, 0.f, 0.f, 5, 0.f, 2, false);
        }
        break;
    case 6:
        setLight(lights, 0, 0.f, 0.f, 0, 0.f, 1, true);
        break;
    case 7:
        setLight(lights, 0, 45.f, 45.f, 16, 0.f, 1, true);
        setLight(lights, 1, -45.f, -20.f, 12, 0.f, 1, true);
        break;
    case 8:
        setLight(lights, 0, 0.f, 85.f, 14, 1.f, 2, true);
        break;
    case 9:
        setLight(lights, 0, 20.f, 15.f, 0, -1.5f, 0, true);
        break;
    case 10:
        setLight(lights, 0, 45.f, 15.f, 0, 0.5f, 1, true);
        setLight(lights, 1, -45.f, 15.f, 0, 0.5f, 1, true);
        setLight(lights, 2, 135.f, 15.f, 0, 0.5f, 1, true);
        setLight(lights, 3, -135.f, 15.f, 0, 0.5f, 1, true);
        break;
    case 11:
        setLight(lights, 0, 0.f, 0.f, 0, 1.f, 2, true);
        setLight(lights, 1, 90.f, 0.f, 0, 1.f, 2, true);
        setLight(lights, 2, 180.f, 0.f, 0, 1.f, 2, true);
        setLight(lights, 3, -90.f, 0.f, 0, 1.f, 2, true);
        break;
    case 12:
        setLight(lights, 0, 0.f, 15.f, 11, 1.f, 1, true);
        break;
    case 13:
        setLight(lights, 0, 0.f, 85.f, 9, 0.5f, 2, true);
        break;
    case 15:
        setLight(lights, 0, 0.f, 15.f, 4, 1.f, 2, true);
        break;
    case 16:
        setLight(lights, 0, 0.f, -10.f, 11, 0.f, 1, true);
        break;
    case 17:
        setLight(lights, 0, 0.f, 5.f, 0, 0.f, 1, true);
        break;
    case 18:
        setLight(lights, 0, 45.f, 0.f, 16, 1.5f, 0, true);
        setLight(lights, 1, -45.f, 0.f, 15, 1.5f, 0, true);
        setLight(lights, 2, 135.f, 0.f, 12, 1.5f, 0, true);
        setLight(lights, 3, -135.f, 0.f, 16, 1.5f, 0, true);
        break;
    case 19:
        setLight(lights, 0, 45.f, 0.f, 14, 0.f, 1, true);
        setLight(lights, 1, -60.f, 0.f, 10, 0.f, 1, true);
        setLight(lights, 2, 180.f, 0.f, 16, 0.f, 1, true);
        break;
    case 20:
        setLight(lights, 0, 0.f, 45.f, 2, 0.f, 0, true);
        break;
    case 21:
        setLight(lights, 0, 45.f, 15.f, 11, 0.f, 0, true);
        setLight(lights, 1, -45.f, 15.f, 11, 0.f, 0, true);
        break;
    case 22:
        setLight(lights, 0, 45.f, 0.f, 9, 0.f, 0, true);
        setLight(lights, 1, -45.f, 0.f, 16, 0.f, 0, true);
        setLight(lights, 2, 180.f, 0.f, 12, 0.f, 0, true);
        break;
    case 23:
        setLight(lights, 0, 0.f, 0.f, 16, 1.f, 2, true);
        break;
    case 24:
        setLight(lights, 0, -115.f, 60.f, 3, -10.f, 0, false);
        break;
    case 25:
        setLight(lights, 0, 0.f, 78.f, 2, 0.2f, 2, true);
        setLight(lights, 1, 90.f, 78.f, 2, 0.2f, 2, true);
        setLight(lights, 2, 180.f, 78.f, 2, 0.2f, 2, true);
        setLight(lights, 3, -90.f, 78.f, 2, 0.2f, 2, true);
        break;
    case 26:
        setLight(lights, 0, 100.f, 12.f, 3, -10.f, 0, false);
        setLight(lights, 1, 108.f, 12.f, 2, -10.f, 0, false);
        break;
    case 27:
        setLight(lights, 0, 45.f, 25.f, 3, 0.f, 0, true);
        setLight(lights, 1, -45.f, 25.f, 3, 0.f, 0, true);
        setLight(lights, 2, 135.f, 40.f, 3, 0.f, 0, true);
        setLight(lights, 3, -135.f, 40.f, 3, 0.f, 0, true);
        break;
    case 28:
        setLight(lights, 0, 45.f, 50.f, 0, -2.f, 0, true);
        setLight(lights, 1, -45.f, 50.f, 0, -2.f, 0, true);
        setLight(lights, 2, 135.f, 50.f, 0, -2.f, 0, true);
        setLight(lights, 3, -135.f, 50.f, 0, -2.f, 0, true);
        break;
    case 30:
        setLight(lights, 0, 0.f, 55.f, 1, 0.5f, 0, true);
        break;
    case 31:
        setLight(lights, 0, 0.f, 85.f, 7, 1.f, 0, true);
        setLight(lights, 1, 0.f, 82.f, 7, 0.f, 0, true);
        break;
    case 32:
        setLight(lights, 0, 0.f, -60.f, 4, -10.f, 0, false);
        setLight(lights, 1, 180.f, 25.f, 12, -10.f, 0, false);
        break;
    default:
        break;
    }
}
} // namespace

Setup sanitizeSetup(const Setup& setup)
{
    Setup output;
    std::memset(&output, 0, sizeof(output));
    output.mRadius = std::clamp(finiteOr(setup.mRadius, DEFAULT_RADIUS),
                                MIN_RADIUS, MAX_RADIUS);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        output.mLights[i] = cleanLight(setup.mLights[i]);
    }
    return output;
}

Transforms sanitizeTransforms(const Transforms& transforms)
{
    Transforms output;
    output.mMirror = transforms.mMirror;
    output.mFacingAzimuthDeg = wrap180(
        finiteOr(transforms.mFacingAzimuthDeg, 0.f));
    output.mYawDeg = wrap180(finiteOr(transforms.mYawDeg, 0.f));
    output.mPitchDeg = clampPitch(transforms.mPitchDeg);
    return output;
}

Globals sanitizeGlobals(const Globals& globals)
{
    Globals output;
    output.mMasterEV = std::clamp(finiteOr(globals.mMasterEV, 0.f),
                                  -MAX_MASTER_EV, MAX_MASTER_EV);
    output.mHeadroomStops = std::clamp(
        finiteOr(globals.mHeadroomStops, 2.f), 0.f, MAX_HEADROOM);
    output.mBounceRatio = std::clamp(
        finiteOr(globals.mBounceRatio, 0.45f), 0.f, 1.f);
    output.mTransitionSec = std::clamp(
        finiteOr(globals.mTransitionSec, 0.9f), 0.f, MAX_TRANSITION);
    output.mBounceEnabled = globals.mBounceEnabled;
    output.mPower = globals.mPower;
    output.mSeed = globals.mSeed ? globals.mSeed : DEFAULT_SEED;
    output.mSubjectScale = sanitizeSubjectScale(globals.mSubjectScale);
    output.mMasterTempMired = std::clamp(
        finiteOr(globals.mMasterTempMired, 0.f),
        MASTER_TEMP_MIRED_MIN, MASTER_TEMP_MIRED_MAX);
    return output;
}

F32 sanitizeSubjectScale(F32 scale)
{
    F32 subject_scale = scale;
    // Subnormal (<= FLT_MIN) collapses to nominal 1.0 intentionally to avoid
    // denormal geometry; GHOST_SCALE clamps make it unreachable in practice.
    if (!std::isfinite(subject_scale) || subject_scale <= 0.f ||
        subject_scale <= std::numeric_limits<F32>::min())
    {
        subject_scale = 1.f;
    }
    return std::clamp(subject_scale,
                      SUBJECT_SCALE_MIN, SUBJECT_SCALE_MAX);
}

void groupBoundsCentre(const F32 points[][3], S32 count,
                       F32 out_centre[3])
{
    if (!out_centre)
    {
        return;
    }
    if (!points || count <= 0)
    {
        out_centre[0] = 0.f;
        out_centre[1] = 0.f;
        out_centre[2] = 0.f;
        return;
    }
    if (count == 1)
    {
        out_centre[0] = points[0][0];
        out_centre[1] = points[0][1];
        out_centre[2] = points[0][2];
        return;
    }

    F32 minimum[3] = { points[0][0], points[0][1], points[0][2] };
    F32 maximum[3] = { points[0][0], points[0][1], points[0][2] };
    for (S32 i = 1; i < count; ++i)
    {
        for (S32 axis = 0; axis < 3; ++axis)
        {
            minimum[axis] = std::min(minimum[axis], points[i][axis]);
            maximum[axis] = std::max(maximum[axis], points[i][axis]);
        }
    }
    for (S32 axis = 0; axis < 3; ++axis)
    {
        out_centre[axis] = (minimum[axis] + maximum[axis]) * 0.5f;
    }
}

F32 groupSubjectScale(const F32 points[][3], const F32 member_scales[],
                      S32 count, const F32 centre[3], F32 nominal_radius)
{
    if (!member_scales || count <= 0)
    {
        return 1.f;
    }
    if (count == 1)
    {
        return sanitizeSubjectScale(member_scales[0]);
    }

    const F32 safe_radius = std::clamp(
        finiteOr(nominal_radius, DEFAULT_RADIUS), MIN_RADIUS, MAX_RADIUS);
    F32 maximum_scale = SUBJECT_SCALE_MIN;
    F32 spread = 0.f;
    for (S32 i = 0; i < count; ++i)
    {
        maximum_scale = std::max(
            maximum_scale, sanitizeSubjectScale(member_scales[i]));
        const F32 dx = points[i][0] - centre[0];
        const F32 dy = points[i][1] - centre[1];
        const F32 dz = points[i][2] - centre[2];
        spread = std::max(spread, std::sqrt(dx * dx + dy * dy + dz * dz));
    }
    return std::clamp(maximum_scale + spread / safe_radius,
                      SUBJECT_SCALE_MIN, SUBJECT_SCALE_MAX);
}

F32 wrap180(F32 degrees)
{
    if (!std::isfinite(degrees))
    {
        return 0.f;
    }
    F32 wrapped = std::fmod(degrees, 360.f);
    if (wrapped > 180.f)
    {
        wrapped -= 360.f;
    }
    else if (wrapped <= -180.f)
    {
        wrapped += 360.f;
    }
    return wrapped == 0.f ? 0.f : wrapped;
}

F32 ease(F32 t)
{
    if (!std::isfinite(t) || t <= 0.f)
    {
        return 0.f;
    }
    if (t >= 1.f)
    {
        return 1.f;
    }
    if (t < 0.5f)
    {
        return 4.f * t * t * t;
    }
    const F32 f = 2.f * t - 2.f;
    return 0.5f * f * f * f + 1.f;
}

void computeLive(const Setup& setup, const Transforms& transforms,
                 LightBase out[LIGHT_COUNT])
{
    const Setup safe_setup = sanitizeSetup(setup);
    const Transforms safe_transforms = sanitizeTransforms(transforms);
    std::memset(out, 0, sizeof(LightBase) * LIGHT_COUNT);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        const LightBase& base = safe_setup.mLights[i];
        const F32 oriented = base.mYawDeg + safe_transforms.mYawDeg;
        out[i].mYawDeg = safe_transforms.mMirror
            ? wrap180(2.f * safe_transforms.mFacingAzimuthDeg - oriented)
            : wrap180(oriented);
        out[i].mPitchDeg = clampPitch(
            base.mPitchDeg + safe_transforms.mPitchDeg);
        out[i].mProfile = base.mProfile;
        out[i].mEV = base.mEV;
        out[i].mBeam = base.mBeam;
        out[i].mOn = base.mOn;
        out[i].mGobo = base.mGobo;
    }
}

LightBase blendLight(const LightBase& start, const LightBase& target,
                     F32 eased)
{
    const LightBase safe_start = cleanLight(start);
    const LightBase safe_target = cleanLight(target);
    if (!std::isfinite(eased) || eased <= 0.f)
    {
        return safe_start;
    }
    if (eased >= 1.f)
    {
        return safe_target;
    }

    LightBase output;
    std::memset(&output, 0, sizeof(output));
    const F32 delta_yaw = wrap180(
        safe_target.mYawDeg - safe_start.mYawDeg);
    output.mYawDeg = wrap180(
        safe_start.mYawDeg + delta_yaw * eased);
    output.mPitchDeg = safe_start.mPitchDeg +
        (safe_target.mPitchDeg - safe_start.mPitchDeg) * eased;
    output.mProfile = eased > 0.5f
        ? safe_target.mProfile : safe_start.mProfile;
    output.mEV = safe_start.mEV +
        (safe_target.mEV - safe_start.mEV) * eased;
    output.mBeam = eased > 0.5f
        ? safe_target.mBeam : safe_start.mBeam;
    output.mOn = safe_start.mOn || safe_target.mOn;
    output.mGobo = eased > 0.5f
        ? safe_target.mGobo : safe_start.mGobo;
    return output;
}

F32 intensityFromEV(F32 ev_total, F32 headroom_stops, bool* clipped)
{
    const F32 safe_ev = std::clamp(finiteOr(ev_total, 0.f), -64.f, 64.f);
    const F32 safe_headroom = std::clamp(
        finiteOr(headroom_stops, 2.f), 0.f, MAX_HEADROOM);
    const F32 raw = std::exp2(safe_ev - safe_headroom);
    if (clipped)
    {
        *clipped = raw > 1.f;
    }
    return std::clamp(raw, 0.f, 1.f);
}

void masterTempGain(F32 mired_shift, F32 gain[3])
{
    if (!gain)
    {
        return;
    }
    const F32 shift = std::clamp(finiteOr(mired_shift, 0.f),
                                 MASTER_TEMP_MIRED_MIN,
                                 MASTER_TEMP_MIRED_MAX);
    if (shift == 0.f)
    {
        gain[0] = 1.f;
        gain[1] = 1.f;
        gain[2] = 1.f;
        return;
    }

    const F64 pivot_mired = 1.0e6 / MASTER_TEMP_PIVOT_KELVIN;
    const F64 mired = std::clamp(
        pivot_mired + static_cast<F64>(shift), 40.0, 500.0);
    F64 white[3];
    F64 pivot[3];
    planckianLinearRGB(1.0e6 / mired, white);
    planckianLinearRGB(MASTER_TEMP_PIVOT_KELVIN, pivot);

    F64 raw_gain[3];
    for (S32 channel = 0; channel < 3; ++channel)
    {
        raw_gain[channel] = white[channel] /
            std::max(pivot[channel], 1.0e-6);
    }
    const F64 green = std::max(raw_gain[1], 1.0e-6);
    gain[0] = static_cast<F32>(std::clamp(raw_gain[0] / green,
                                          0.1, 10.0));
    gain[1] = 1.f;
    gain[2] = static_cast<F32>(std::clamp(raw_gain[2] / green,
                                          0.1, 10.0));
}

void render(F32 radius, const LightBase live[LIGHT_COUNT],
            const Globals& globals, RigFrame& out)
{
    const Globals safe_globals = sanitizeGlobals(globals);
    const F32 safe_radius = std::clamp(
        finiteOr(radius, DEFAULT_RADIUS), MIN_RADIUS, MAX_RADIUS);
    const F32 distance_ev = std::log2(safe_radius / DEFAULT_RADIUS);
    F32 effective_radius = safe_radius;
    if (safe_globals.mSubjectScale != 1.f)
    {
        effective_radius = std::clamp(
            safe_radius * safe_globals.mSubjectScale,
            std::min(safe_radius, SCALED_RADIUS_FLOOR),
            std::max(safe_radius, SCALED_RADIUS_CEIL));
    }
    F32 temp_gain[3] = { 1.f, 1.f, 1.f };
    const bool temp_active = safe_globals.mMasterTempMired != 0.f;
    if (temp_active)
    {
        masterTempGain(safe_globals.mMasterTempMired, temp_gain);
    }
    std::memset(&out, 0, sizeof(out));

    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        const LightBase light = cleanLight(live[i]);
        const F32 yaw = light.mYawDeg * DEGREES_TO_RADIANS;
        const F32 pitch = light.mPitchDeg * DEGREES_TO_RADIANS;
        const F32 cos_pitch = std::cos(pitch);
        const F32 off_x = effective_radius * cos_pitch * std::cos(yaw);
        const F32 off_y = effective_radius * cos_pitch * std::sin(yaw);
        const F32 off_z = effective_radius * std::sin(pitch);
        const F32 total_ev = light.mEV + safe_globals.mMasterEV + distance_ev;
        const F32 pre_headroom = std::exp2(
            std::clamp(total_ev, -64.f, 64.f));
        const F32 raw_intensity = std::exp2(std::clamp(
            total_ev - safe_globals.mHeadroomStops, -64.f, 64.f));
        bool clipped = false;
        const F32 intensity = intensityFromEV(
            total_ev, safe_globals.mHeadroomStops, &clipped);
        const bool on = light.mOn && safe_globals.mPower &&
                        pre_headroom > 0.001f;
        F32 rgb[3];
        profileSRGB(light.mProfile, rgb);
        if (temp_active)
        {
            for (S32 channel = 0; channel < 3; ++channel)
            {
                const F32 linear = std::clamp(
                    srgbChannelToLinear(rgb[channel]) * temp_gain[channel],
                    0.f, 1.f);
                rgb[channel] = linearChannelToSRGB(linear);
            }
        }

        EmitterState& projector = out.mProj[i];
        projector.mOffX = off_x;
        projector.mOffY = off_y;
        projector.mOffZ = off_z;
        projector.mAimX = -off_x / effective_radius;
        projector.mAimY = -off_y / effective_radius;
        projector.mAimZ = -off_z / effective_radius;
        projector.mSR = rgb[0];
        projector.mSG = rgb[1];
        projector.mSB = rgb[2];
        projector.mIntensity = intensity;
        projector.mLightRadius = effective_radius * 2.2f;
        projector.mFalloff = beamFalloff(light.mBeam);
        projector.mFovRad = beamFov(light.mBeam);
        projector.mOn = on;
        projector.mClipped = on && clipped;
        projector.mGobo = light.mGobo;

        const F32 omni_pitch_deg = std::max(
            light.mPitchDeg - 45.f, -PITCH_LIMIT_DEG);
        const F32 omni_pitch = omni_pitch_deg * DEGREES_TO_RADIANS;
        const F32 omni_cos = std::cos(omni_pitch);
        EmitterState& omni = out.mOmni[i];
        omni.mOffX = effective_radius * omni_cos * std::cos(yaw);
        omni.mOffY = effective_radius * omni_cos * std::sin(yaw);
        omni.mOffZ = effective_radius * std::sin(omni_pitch);
        omni.mAimZ = -1.f;
        omni.mSR = rgb[0];
        omni.mSG = rgb[1];
        omni.mSB = rgb[2];
        const F32 raw_omni_intensity =
            raw_intensity * safe_globals.mBounceRatio;
        const bool omni_on = light.mOn && safe_globals.mPower &&
            safe_globals.mBounceEnabled &&
            pre_headroom * safe_globals.mBounceRatio > 0.001f;
        omni.mIntensity = std::clamp(raw_omni_intensity, 0.f, 1.f);
        omni.mLightRadius = effective_radius * 1.5f;
        omni.mFalloff = 0.75f;
        omni.mFovRad = 0.f;
        omni.mOn = omni_on;
        omni.mClipped = omni_on && raw_omni_intensity > 1.f;
    }
}

F32 emitterBoxEdgeFromRatio(F32 effective_to_nominal_radius)
{
    const F32 safe_ratio = finiteOr(effective_to_nominal_radius, 1.f);
    return std::clamp(0.25f * safe_ratio, 0.01f, 1.f);
}

void evalFX(S32 fx, U64 seed, F64 t_seconds,
            LightBase out[LIGHT_COUNT])
{
    LightBase lights[LIGHT_COUNT];
    if (fx < 0 || fx >= FX_COUNT)
    {
        std::memset(out, 0, sizeof(LightBase) * LIGHT_COUNT);
        return;
    }

    initializeFX(fx, lights);
    seed = seed ? seed : DEFAULT_SEED;
    const F32 interval = FX_INTERVALS[fx];
    const F64 seconds = std::clamp(
        finiteOr(t_seconds, 0.0), 0.0, MAX_FX_SECONDS);
    const F64 fs = seconds / (F64)interval;
    const S64 step = virtualStep(seconds, interval);
    const U64 counter = static_cast<U64>(step);

    switch (fx)
    {
    case 0: // Police Sirens
        if ((step & 1) == 0)
        {
            lights[0].mEV = 1.f;
            lights[1].mEV = -10.f;
        }
        else
        {
            lights[0].mEV = -10.f;
            lights[1].mEV = 1.f;
        }
        break;
    case 1: // Club Strobe
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            lights[i].mPitchDeg = 30.f +
                30.f * unitHash(seed, fx, counter, i, 0);
            if (unitHash(seed, fx, counter, i, 1) > 0.6f)
            {
                lights[i].mProfile = 7 + static_cast<S32>(
                    16.f * unitHash(seed, fx, counter, i, 2));
                lights[i].mEV = 0.5f +
                    unitHash(seed, fx, counter, i, 3);
                lights[i].mOn = true;
            }
            else
            {
                lights[i].mEV = -10.f;
            }
        }
        break;
    case 2: // Fire Flicker
        lights[0].mEV = -1.25f + 1.5f *
            unitHash(seed, fx, counter, 0, 0);
        lights[1].mEV = -1.5f +
            unitHash(seed, fx, counter, 1, 0);
        break;
    case 3: // Streetlight
    {
        const F64 cycle = positiveFmod(fs, 40.0);
        if (cycle < 20.0)
        {
            lights[0].mYawDeg = 0.f;
            lights[0].mPitchDeg = 30.f + static_cast<F32>(cycle / 20.0 * 55.0);
        }
        else
        {
            lights[0].mYawDeg = 180.f;
            lights[0].mPitchDeg = 85.f -
                static_cast<F32>((cycle - 20.0) / 20.0 * 55.0);
        }
        lights[0].mEV = ((lights[0].mPitchDeg - 30.f) / 55.f * 2.f) - 1.f;
        break;
    }
    case 4: // Neon Pulse
        lights[0].mEV = phaseSin(fs * 0.4) * 1.5f;
        lights[1].mEV = phaseSin((fs + 5.0) * 0.35) * 1.5f;
        break;
    case 5: // Paparazzi
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            if (unitHash(seed, fx, counter, i, 0) > 0.8f)
            {
                lights[i].mYawDeg =
                    unitHash(seed, fx, counter, i, 1) * 360.f - 180.f;
                lights[i].mPitchDeg =
                    unitHash(seed, fx, counter, i, 2) * 60.f;
                lights[i].mEV = 2.f;
                lights[i].mOn = true;
            }
            else
            {
                lights[i].mOn = false;
            }
        }
        break;
    case 6: // TV Screen
        lights[0].mProfile = unitHash(seed, fx, counter, 0, 0) > 0.5f ? 4 : 6;
        lights[0].mEV = -1.f + 2.f * unitHash(seed, fx, counter, 0, 1);
        break;
    case 7: // Underwater
        lights[0].mEV = phaseSin(fs * 0.2);
        lights[0].mPitchDeg = 45.f + phaseCos(fs * 0.15) * 10.f;
        lights[1].mEV = phaseSin(fs * 0.22 + 2.0) - 0.5f;
        lights[1].mPitchDeg = -20.f + phaseCos(fs * 0.17 + 1.0) * 5.f;
        break;
    case 8: // UFO Abduction
        lights[0].mYawDeg = wrap180(static_cast<F32>(
            positiveFmod(15.0 * fs, 360.0)));
        lights[0].mEV = 1.f + phaseSin(fs * 0.5) * 0.5f;
        break;
    case 9: // Haunted Flicker
        lights[0].mEV = unitHash(seed, fx, counter, 0, 0) > 0.9f
            ? -10.f
            : -1.5f + 0.3f * unitHash(seed, fx, counter, 0, 1);
        break;
    case 10: // RGB Gamer
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            static const F32 yaws[LIGHT_COUNT] = { 45.f, -45.f, 135.f, -135.f };
            lights[i].mYawDeg = wrap180(yaws[i] + static_cast<F32>(
                positiveFmod(5.0 * fs, 360.0)));
            lights[i].mProfile = 7 + static_cast<S32>(
                positiveMod(step + i * 4, 16));
        }
        break;
    case 11: // Disco Ball
    {
        const U64 color_cycle = static_cast<U64>(std::floor(fs / 5.0));
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            static const F32 yaws[LIGHT_COUNT] = { 0.f, 90.f, 180.f, -90.f };
            lights[i].mYawDeg = wrap180(yaws[i] + static_cast<F32>(
                positiveFmod(20.0 * fs, 360.0)));
            lights[i].mPitchDeg = 30.f +
                phaseSin((fs + i) * 0.3) * 30.f;
            lights[i].mProfile = 7 + static_cast<S32>(16.f *
                unitHash(seed, fx, color_cycle, i, 0));
        }
        break;
    }
    case 12: // Warning Alert
        lights[0].mYawDeg = wrap180(static_cast<F32>(
            positiveFmod(25.0 * fs, 360.0)));
        break;
    case 13: // Matrix Drop
    {
        const F64 drop_length = 170.0 / 15.0;
        const F64 phase = positiveFmod(fs, drop_length);
        const U64 cycle = static_cast<U64>(std::floor(fs / drop_length));
        lights[0].mPitchDeg = 85.f - 15.f * static_cast<F32>(phase);
        lights[0].mYawDeg = 180.f * unitHash(seed, fx, cycle, 0, 0) - 90.f;
        break;
    }
    case 14: // Thunderstorm
        lights[0].mOn = true;
        if (unitHash(seed, fx, counter, 0, 0) > 0.9f)
        {
            lights[0].mEV = 2.f + unitHash(seed, fx, counter, 0, 1);
            lights[0].mYawDeg = 360.f *
                unitHash(seed, fx, counter, 0, 2) - 180.f;
            lights[0].mPitchDeg = 10.f + 60.f *
                unitHash(seed, fx, counter, 0, 3);
            lights[0].mProfile = 4;
        }
        else
        {
            lights[0].mEV = -10.f;
        }
        break;
    case 15: // Searchlight
        lights[0].mYawDeg = phaseSin(fs * 0.1) * 90.f;
        break;
    case 16: // Heartbeat
    {
        const S64 beat = positiveMod(step, 15);
        lights[0].mEV = (beat == 0 || beat == 3) ? 1.5f : -3.f;
        break;
    }
    case 17: // Movie Projector
        lights[0].mEV = -1.f + 2.5f * unitHash(seed, fx, counter, 0, 0);
        lights[0].mProfile = unitHash(seed, fx, counter, 0, 1) > 0.5f ? 3 : 2;
        break;
    case 18: // Warp Tunnel
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            const F64 phase = positiveFmod(fs + i * 5.0, 20.0);
            lights[i].mPitchDeg = 85.f - static_cast<F32>(phase / 20.0 * 170.0);
        }
        break;
    case 19: // Fairy Woods
        lights[0].mEV = phaseSin(fs * 0.1);
        lights[1].mEV = phaseCos((fs + 3.0) * 0.13);
        lights[2].mEV = phaseSin((fs + 7.0) * 0.07);
        break;
    case 20: // Short Circuit
        if (unitHash(seed, fx, counter, 0, 0) > 0.8f)
        {
            lights[0].mEV = 1.5f + unitHash(seed, fx, counter, 0, 1);
        }
        else if (unitHash(seed, fx, counter, 0, 2) > 0.6f)
        {
            lights[0].mEV = -1.f;
        }
        else
        {
            lights[0].mEV = -10.f;
        }
        break;
    case 21: // Red Alert Pulse
        lights[0].mEV = phaseSin(fs * 0.2) * 2.f;
        lights[1].mEV = lights[0].mEV;
        break;
    case 22: // Aurora Borealis
        lights[0].mPitchDeg = 45.f + phaseSin(fs * 0.05) * 20.f;
        lights[1].mPitchDeg = 45.f + phaseCos((fs + 1.0) * 0.06) * 20.f;
        lights[2].mPitchDeg = 45.f + phaseSin((fs + 2.0) * 0.04) * 20.f;
        break;
    case 23: // Cyber Scanner
        lights[0].mPitchDeg = phaseSin(fs * 0.15) * 60.f;
        break;
    case 24: // Shooting Star
    {
        const F64 phase = positiveFmod(fs, 40.0);
        if (phase < 10.0)
        {
            lights[0].mYawDeg = -115.f + static_cast<F32>(phase * 23.0);
            lights[0].mPitchDeg = 55.f + phaseSin(phase * 0.3142) * 25.f;
            lights[0].mEV = 1.5f - static_cast<F32>(phase * 0.05);
            lights[0].mOn = true;
        }
        else
        {
            lights[0].mOn = false;
        }
        break;
    }
    case 25: // Elevator Fault
    {
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            lights[i].mEV = 0.2f + 0.05f *
                unitHash(seed, fx, counter, i, 0);
        }
        if (positiveMod(step, 15) == 0)
        {
            for (LightBase& light : lights)
            {
                light.mEV = -10.f;
            }
        }
        const F32 glitch = unitHash(seed, fx, counter, 0, 4);
        if (glitch > 0.96f)
        {
            const S32 bad = std::min(3, static_cast<S32>(
                ((glitch - 0.96f) / 0.04f) * 4.f));
            lights[bad].mEV = -10.f;
        }
        break;
    }
    case 26: // Car Pass
    {
        const F64 cycle = positiveFmod(fs, 55.0);
        if (cycle < 18.0)
        {
            const F32 u = static_cast<F32>(cycle / 18.0);
            lights[0].mYawDeg = 100.f - u * 210.f;
            lights[1].mYawDeg = lights[0].mYawDeg + 8.f;
            lights[0].mOn = lights[1].mOn = true;
            lights[0].mEV = 0.9f;
            lights[1].mEV = 0.7f;
            lights[0].mProfile = 3;
            lights[1].mProfile = 2;
        }
        else if (cycle < 21.0 || cycle >= 35.0)
        {
            lights[0].mOn = lights[1].mOn = false;
        }
        else
        {
            const F32 u = static_cast<F32>((cycle - 21.0) / 14.0);
            lights[0].mYawDeg = -110.f - u * 20.f;
            lights[1].mYawDeg = lights[0].mYawDeg + 8.f;
            lights[0].mOn = lights[1].mOn = true;
            lights[0].mEV = 0.4f - u * 0.6f;
            lights[1].mEV = 0.3f - u * 0.5f;
            lights[0].mProfile = lights[1].mProfile = 11;
        }
        break;
    }
    case 27: // Explosion
    {
        const S64 beat = positiveMod(step, 65);
        const F32 last_decay = 2.f - (8.f / 9.f) * 3.5f;
        lights[2].mEV = last_decay * 0.6f;
        lights[3].mEV = last_decay * 0.4f;
        if (beat < 3)
        {
            for (LightBase& light : lights)
            {
                light.mEV = 2.f;
            }
        }
        else if (beat < 12)
        {
            const F32 u = static_cast<F32>(beat - 3) / 9.f;
            const F32 value = 2.f - u * 3.5f;
            lights[0].mEV = value;
            lights[1].mEV = value * 0.8f;
            lights[2].mEV = value * 0.6f;
            lights[3].mEV = value * 0.4f;
            lights[0].mProfile = lights[1].mProfile = 13;
            lights[2].mProfile = lights[3].mProfile = 0;
        }
        else if (beat < 45)
        {
            const F32 u = static_cast<F32>(beat - 12) / 33.f;
            const F32 base = -0.5f - u * 2.5f;
            lights[0].mEV = base + 0.5f *
                unitHash(seed, fx, counter, 0, 0);
            lights[1].mEV = base - 0.4f + 0.3f *
                unitHash(seed, fx, counter, 1, 0);
            lights[0].mProfile = lights[1].mProfile = 0;
            lights[2].mProfile = lights[3].mProfile = 0;
            lights[2].mOn = lights[3].mOn = false;
        }
        else
        {
            const U64 cycle_start = counter - static_cast<U64>(beat);
            const F32 last_ember_base = -0.5f - (32.f / 33.f) * 2.5f;
            lights[0].mOn = true;
            lights[1].mOn = lights[2].mOn = lights[3].mOn = false;
            lights[0].mProfile = lights[1].mProfile = 0;
            lights[2].mProfile = lights[3].mProfile = 0;
            lights[0].mEV = -10.f;
            lights[1].mEV = last_ember_base - 0.4f + 0.3f *
                unitHash(seed, fx, cycle_start + 44, 1, 0);
            if (unitHash(seed, fx, counter, 0, 0) > 0.95f)
            {
                lights[0].mEV = -3.f + 1.5f *
                    unitHash(seed, fx, counter, 0, 1);
            }
        }
        break;
    }
    case 28: // Supernova
    {
        const S64 beat = positiveMod(step, 90);
        if (beat < 50)
        {
            const F32 u = static_cast<F32>(beat) / 50.f;
            const F32 value = -2.f + u * 4.f;
            const S32 profile = static_cast<S32>(u * 4.f);
            for (LightBase& light : lights)
            {
                light.mEV = value;
                light.mProfile = profile;
            }
        }
        else if (beat < 53)
        {
            for (LightBase& light : lights)
            {
                light.mEV = 2.f;
                light.mProfile = 3;
            }
        }
        else if (beat < 65)
        {
            const F32 u = static_cast<F32>(beat - 53) / 12.f;
            const F32 value = std::max(-10.f, 2.f - u * 13.f);
            for (LightBase& light : lights)
            {
                light.mEV = value;
                light.mProfile = 3;
            }
        }
        else if (beat < 75)
        {
            const F32 last_collapse = 2.f - (11.f / 12.f) * 13.f;
            lights[0].mEV = -3.f;
            lights[0].mProfile = 5;
            lights[1].mEV = -3.5f;
            lights[1].mProfile = 5;
            lights[2].mEV = lights[3].mEV = last_collapse;
            lights[2].mProfile = lights[3].mProfile = 3;
            lights[2].mOn = lights[3].mOn = false;
        }
        else
        {
            const F32 last_collapse = 2.f - (11.f / 12.f) * 13.f;
            lights[0].mEV = lights[1].mEV = -10.f;
            lights[0].mProfile = lights[1].mProfile = 5;
            lights[2].mEV = lights[3].mEV = last_collapse;
            lights[2].mProfile = lights[3].mProfile = 3;
            lights[2].mOn = lights[3].mOn = false;
        }
        break;
    }
    case 29: // Stage Debut -- all LSL latches expressed from beat alone
    {
        std::memset(lights, 0, sizeof(lights));
        const S64 beat = positiveMod(step, 80);
        if (beat >= 5 && beat < 70)
        {
            setLight(lights, 3, 0.f, -15.f, 1, -0.5f, 0, true);
        }
        if (beat >= 12 && beat < 70)
        {
            setLight(lights, 0, 45.f, 45.f, 3, 0.4f, 2, true);
        }
        if (beat >= 16 && beat < 70)
        {
            const F32 latched_fill_ev =
                -2.f + (4.f / 5.f) * 1.5f;
            const F32 fill_ev = beat <= 20
                ? -2.f + (static_cast<F32>(beat - 16) / 5.f) * 1.5f
                : latched_fill_ev;
            setLight(lights, 1, -40.f, 25.f, 2, fill_ev, 2, true);
        }
        if (beat >= 22 && beat < 70)
        {
            setLight(lights, 2, 150.f, 40.f, 1, -0.5f, 2, true);
        }
        if (beat > 25 && beat < 70)
        {
            lights[0].mEV = 0.4f + phaseSin((F64)beat * 0.04) * 0.1f;
        }
        break;
    }
    case 30: // Swinging Lamp
    {
        const F64 beat = positiveFmod(fs, 230.0);
        if (beat < 200.0)
        {
            const F32 amplitude = 60.f * static_cast<F32>(
                std::pow(0.978, beat));
            lights[0].mOn = true;
            lights[0].mYawDeg = phaseSin(beat * 0.18) * amplitude;
            lights[0].mPitchDeg = 75.f - amplitude / 60.f * 30.f;
            lights[0].mEV = 0.5f -
                std::fabs(lights[0].mYawDeg) / 60.f * 0.2f +
                valueNoise(seed, fx, fs, 0, 0) * 0.04f;
        }
        else if (beat < 215.0)
        {
            lights[0].mOn = true;
            lights[0].mYawDeg = 0.f;
            lights[0].mPitchDeg = 75.f;
            lights[0].mEV = 0.5f;
        }
        else
        {
            lights[0].mOn = false;
        }
        break;
    }
    case 31: // Parachute Flare
    {
        const F64 beat = positiveFmod(fs, 110.0);
        if (beat < 80.0)
        {
            const F32 descent = static_cast<F32>(beat / 80.0);
            lights[0].mOn = lights[1].mOn = true;
            lights[0].mPitchDeg = std::max(5.f, 85.f - descent * 80.f);
            lights[0].mYawDeg = phaseSin(beat * 0.18) * 15.f;
            lights[0].mEV = (1.f - descent) * 1.2f +
                valueNoise(seed, fx, fs, 0, 0) * 0.15f;
            lights[1].mPitchDeg = std::min(85.f, lights[0].mPitchDeg + 8.f);
            lights[1].mYawDeg = lights[0].mYawDeg * 0.5f;
            lights[1].mEV = (1.f - descent) * 0.3f - 0.5f +
                valueNoise(seed, fx, fs, 1, 0) * 0.1f;
            if (lights[0].mPitchDeg <= 8.f)
            {
                lights[0].mEV = -10.f +
                    valueNoise(seed, fx, fs, 0, 1) * 2.f;
                lights[1].mEV = -10.f;
            }
        }
        else
        {
            lights[0].mOn = lights[1].mOn = false;
        }
        break;
    }
    case 32: // Villain Reveal
    {
        const F64 beat = positiveFmod(fs, 100.0);
        if (beat < 20.0)
        {
            lights[0].mOn = lights[1].mOn = false;
            lights[0].mEV = lights[1].mEV = -10.f;
        }
        else if (beat < 50.0)
        {
            const F32 u = static_cast<F32>((beat - 20.0) / 30.0);
            lights[0].mOn = true;
            lights[0].mPitchDeg = -60.f + u * 40.f;
            lights[0].mEV = -2.f + u * 2.5f;
        }
        else
        {
            lights[0].mOn = true;
            lights[0].mPitchDeg = -20.f + phaseSin(beat * 0.08) * 5.f;
            lights[0].mEV = 0.3f + phaseSin(beat * 0.1) * 0.2f;
            lights[1].mOn = true;
            const F32 fade = std::min(1.f,
                static_cast<F32>((beat - 50.0) / 20.0));
            lights[1].mEV = -10.f + fade * 9.5f;
        }
        break;
    }
    default:
        break;
    }

    copyCleanLights(lights, out);
}

const char* fxName(S32 fx)
{
    return fx >= 0 && fx < FX_COUNT ? FX_NAMES[fx] : "None";
}

F32 fxInterval(S32 fx)
{
    return fx >= 0 && fx < FX_COUNT ? FX_INTERVALS[fx] : 0.1f;
}

const char* profileName(S32 index)
{
    return PROFILES[std::clamp(index, 0, PROFILE_COUNT - 1)].mName;
}

void profileSRGB(S32 index, F32 rgb[3])
{
    const Profile& profile = PROFILES[
        std::clamp(index, 0, PROFILE_COUNT - 1)];
    rgb[0] = profile.mR;
    rgb[1] = profile.mG;
    rgb[2] = profile.mB;
}

const char* beamName(S32 index)
{
    return BEAMS[std::clamp(index, 0, BEAM_COUNT - 1)].mName;
}

F32 beamFov(S32 index)
{
    return std::clamp(BEAMS[
        std::clamp(index, 0, BEAM_COUNT - 1)].mFov, 0.05f, 2.9f);
}

F32 beamFalloff(S32 index)
{
    return BEAMS[std::clamp(index, 0, BEAM_COUNT - 1)].mFalloff;
}

const char* goboName(S32 index)
{
    return GOBO_NAMES[std::clamp(index, 0, GOBO_COUNT - 1)];
}

Setup classicSetup()
{
    Setup setup;
    std::memset(&setup, 0, sizeof(setup));
    setup.mRadius = 1.5f;
    setLight(setup.mLights, 0, 45.f, 35.f, 3, 0.f, 1, true);
    setLight(setup.mLights, 1, -45.f, 5.f, 3, -2.f, 1, true);
    setLight(setup.mLights, 2, -135.f, 45.f, 4, -0.5f, 0, true);
    setLight(setup.mLights, 3, 0.f, -20.f, 3, 0.f, 0, false);
    return setup;
}
} // namespace ALCineLightRigModel
