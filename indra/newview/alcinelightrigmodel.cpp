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
constexpr F32 MAX_RATIO_STOPS = 5.f;
constexpr F32 MIN_DERIVED_EV = MIN_EV - MAX_RATIO_STOPS;
constexpr F32 CATCHLIGHT_RADIAL_M = 0.10f;
constexpr F64 MAX_FX_SECONDS = 1.0e12;
constexpr F64 TWO_PI = 6.283185307179586476925286766559;
constexpr F32 DEGREES_TO_RADIANS =
    static_cast<F32>(TWO_PI / 360.0);
constexpr F32 RADIANS_TO_DEGREES =
    static_cast<F32>(360.0 / TWO_PI);
constexpr F32 PROJECTOR_FOV_MAX = 3.f;
constexpr F32 FIXTURE_REFERENCE_SOURCE_SIZE_M = 0.10f;

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

struct Gel
{
    const char* mName;
    bool mColourTemperature;
    F32 mMiredShift;
    F32 mMultiplier[3];
};

struct FixtureGel
{
    const char* mName;
    bool mColourTemperature;
    F32 mMiredShift;
    F32 mMultiplier[3];
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

// Temperature gels are relative mired trims around the same 6500 K pivot as
// the rig master trim. Party/correction gels are linear-sRGB multipliers.
const Gel GELS[GEL_COUNT] = {
    { "None",          false,    0.f, { 1.00f, 1.00f, 1.00f } },
    { "CTO",            true,  135.f, { 1.00f, 1.00f, 1.00f } },
    { "1/2 CTO",        true,   70.f, { 1.00f, 1.00f, 1.00f } },
    { "1/4 CTO",        true,   35.f, { 1.00f, 1.00f, 1.00f } },
    { "CTB",            true, -110.f, { 1.00f, 1.00f, 1.00f } },
    { "1/2 CTB",        true,  -70.f, { 1.00f, 1.00f, 1.00f } },
    { "1/4 CTB",        true,  -35.f, { 1.00f, 1.00f, 1.00f } },
    { "Plus Green",    false,    0.f, { 0.78f, 1.00f, 0.76f } },
    { "Minus Green",   false,    0.f, { 1.00f, 0.72f, 1.00f } },
    { "Bastard Amber", false,    0.f, { 1.00f, 0.55f, 0.18f } },
    { "Steel Blue",    false,    0.f, { 0.22f, 0.48f, 1.00f } },
    { "Congo Blue",    false,    0.f, { 0.015f, 0.020f, 0.55f } },
    { "Primary Red",   false,    0.f, { 1.00f, 0.01f, 0.01f } },
    { "Primary Green", false,    0.f, { 0.01f, 1.00f, 0.02f } },
    { "Primary Blue",  false,    0.f, { 0.01f, 0.03f, 1.00f } },
};

// Fixture-mode correction values live in a separate table so the legacy gel
// indices and multipliers remain byte-for-byte stable.
const FixtureGel FIXTURE_GELS[FIXTURE_GEL_COUNT] = {
    { "None",                 false,    0.f, { 1.00f, 1.00f, 1.00f } },
    { "Full CTO (L204)",       true,  159.f, { 1.00f, 1.00f, 1.00f } },
    { "1/2 CTO (L205)",        true,  109.f, { 1.00f, 1.00f, 1.00f } },
    { "1/4 CTO (L206)",        true,   64.f, { 1.00f, 1.00f, 1.00f } },
    { "1/8 CTO (L223)",        true,   30.f, { 1.00f, 1.00f, 1.00f } },
    { "Full CTB (L201)",       true, -137.f, { 1.00f, 1.00f, 1.00f } },
    { "1/2 CTB (L202)",        true,  -78.f, { 1.00f, 1.00f, 1.00f } },
    { "1/4 CTB (L203)",        true,  -49.f, { 1.00f, 1.00f, 1.00f } },
    { "1/8 CTB (L218)",        true,  -26.f, { 1.00f, 1.00f, 1.00f } },
    { "Plus Green (L244)",    false,    0.f, { 0.78f, 1.00f, 0.76f } },
    { "1/2 Plus Green (L245)", false,   0.f, { 0.89f, 1.00f, 0.88f } },
    { "Minus Green (L247)",   false,    0.f, { 1.00f, 0.72f, 1.00f } },
    { "1/2 Minus Green (L248)", false,  0.f, { 1.00f, 0.86f, 1.00f } },
    { "Bastard Amber (R02)",  false,    0.f, { 1.00f, 0.55f, 0.18f } },
    { "Steel Blue (R64)",     false,    0.f, { 0.22f, 0.48f, 1.00f } },
    { "Congo Blue (L181)",    false,    0.f, { 0.015f, 0.020f, 0.55f } },
    { "Primary Red",          false,    0.f, { 1.00f, 0.01f, 0.01f } },
    { "Primary Green",        false,    0.f, { 0.01f, 1.00f, 0.02f } },
    { "Primary Blue",         false,    0.f, { 0.01f, 0.03f, 1.00f } },
};

const FixturePreset FIXTURES[FIXTURE_PRESET_COUNT] = {
    { "Tungsten Fresnel (650W)", 3200.f, 0.12f, 0, { 0, 0, 0 } },
    { "Big Fresnel (5K)",        3200.f, 0.30f, 0, { 0, 0, 0 } },
    { "Open-face (Redhead)",     3200.f, 0.18f, 1, { 0, 0, 0 } },
    { "HMI (M18)",               5600.f, 0.20f, 0, { 0, 0, 0 } },
    { "LED bi-color panel",      5600.f, 0.45f, 1, { 0, 0, 0 } },
    { "Kino / fluoro bank",      4300.f, 1.20f, 1, { 10, 0, 0 } },
    { "2 ft China ball",         3000.f, 0.60f, 1, { 0, 0, 0 } },
    { "3 ft Softbox",            5600.f, 0.90f, 1, { 0, 0, 0 } },
    { "5 ft Octabox",            5600.f, 1.50f, 1, { 0, 0, 0 } },
    { "Book light",              5600.f, 2.40f, 1, { 0, 0, 0 } },
    { "Practical bulb",          2700.f, 0.05f, 1, { 0, 0, 0 } },
    { "Sodium vapor street",     2200.f, 0.25f, 1, { 9, 0, 0 } },
    { "Moonlight (cheated)",     6500.f, 1.00f, 1, { 6, 0, 0 } },
};

const char* const GOBO_NAMES[GOBO_COUNT] = {
    "Default", "Venetian Blinds", "Window Panes", "Prison Bars",
    "Slats", "Grid", "Soft Dapple", "Branches",
    "Arched Window", "French Door", "Curtain Edge", "Stairwell Rail",
    "Door Crack", "Dense Foliage", "Palm Dapple", "Water Caustics",
    "Classic Cucoloris", "Fine Celo", "Scrim Wave", "Smoke Drift",
    "Chain Link", "Industrial Grate", "Rotating Fan", "Neon Sign Mask",
};

const S32 GOBO_CATEGORIES[GOBO_COUNT] = {
    0, 1, 1, 1, 1, 4, 2, 2,
    1, 1, 1, 1, 1, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4,
};

const char* const GOBO_CATEGORY_NAMES[GOBO_CATEGORY_COUNT] = {
    "User / Default", "Architectural", "Organic", "Grip", "Graphic / Hard",
};

const char* const FLICKER_PROGRAM_NAMES[FLICKER_COUNT] = {
    "None", "Firelight", "TV / Monitor", "Fluorescent", "Neon",
    "Candle", "Passing Headlights", "Police Lightbar",
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
    "Parachute Flare", "Villain Reveal", "Neon Buzz", "Welding Arc",
    "Candle Draft", "Sunrise Sweep", "Dying Bulb", "Rave Chase",
    "Lighthouse", "Night Train", "Fireworks Finale", "Passing Clouds",
    "Will-o'-Wisp", "Hologram Glitch", "Signal Lamp", "Breathing Swell",
    "Arcane Orbit",
    "Rock With You",
    "Boogie Floor", "Shootout", "Chopper Hunt", "Plasma Globe",
    "Dimensional Rift", "Kawoosh", "Time Circuits", "Jacob's Ladder",
    "Build & Drop", "Biolume Tide", "Mount Doom", "Vaporwave Sunset",
    "Carousel Waltz", "Five Tones", "Candy Orbit", "Rimwave",
    "Afterhours Drift", "Something Behind You",
};

const F32 FX_INTERVALS[FX_COUNT] = {
    0.15f, 0.10f, 0.20f, 0.20f, 0.10f, 0.10f, 0.20f, 0.20f,
    0.10f, 0.10f, 0.20f, 0.10f, 0.10f, 0.15f, 0.05f, 0.10f,
    0.10f, 0.10f, 0.05f, 0.20f, 0.05f, 0.10f, 0.20f, 0.10f,
    0.05f, 0.10f, 0.07f, 0.08f, 0.08f, 0.15f, 0.07f, 0.12f,
    0.12f, 0.05f, 0.05f, 0.20f, 0.25f, 0.10f, 0.125f, 0.10f,
    0.10f, 0.08f, 0.25f, 0.10f, 0.06f, 0.15f, 0.20f, 0.10f,
    0.10f,
    0.125f, 0.05f, 0.10f, 0.05f, 0.10f, 0.10f, 0.10f, 0.05f,
    0.10f, 0.20f, 0.20f, 0.25f, 0.10f, 0.15f,
    0.10f, 0.10f, 0.20f, 0.10f,
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

LightBase cleanLight(const LightBase& input, F32 min_ev = MIN_EV)
{
    LightBase output;
    std::memset(&output, 0, sizeof(output));
    output.mYawDeg = wrap180(finiteOr(input.mYawDeg, 0.f));
    output.mPitchDeg = clampPitch(input.mPitchDeg);
    output.mProfile = std::clamp(input.mProfile, 0, PROFILE_COUNT - 1);
    output.mEV = std::clamp(finiteOr(input.mEV, 0.f), min_ev, MAX_EV);
    output.mBeam = std::clamp(input.mBeam, 0, BEAM_COUNT - 1);
    output.mOn = input.mOn;
    output.mGobo = std::clamp(input.mGobo, 0, GOBO_COUNT - 1);
    output.mGel = std::clamp(input.mGel, 0, GEL_COUNT - 1);
    output.mFlickerProgram = std::clamp(
        input.mFlickerProgram, (S32)FLICKER_NONE, (S32)FLICKER_COUNT - 1);
    output.mFlickerAmount = std::clamp(
        finiteOr(input.mFlickerAmount, 0.f), 0.f, 1.f);
    output.mFixtureMode = input.mFixtureMode;
    output.mKelvin = std::clamp(
        finiteOr(input.mKelvin, 5600.f),
        FIXTURE_KELVIN_MIN, FIXTURE_KELVIN_MAX);
    for (S32 slot = 0; slot < FIXTURE_GEL_SLOT_COUNT; ++slot)
    {
        output.mGelSlot[slot] = std::clamp(
            input.mGelSlot[slot], 0, FIXTURE_GEL_COUNT - 1);
    }
    output.mSourceSizeM = std::clamp(
        finiteOr(input.mSourceSizeM, FIXTURE_REFERENCE_SOURCE_SIZE_M),
        FIXTURE_SOURCE_SIZE_MIN, FIXTURE_SOURCE_SIZE_MAX);
    output.mFixturePreset = std::clamp(
        input.mFixturePreset, 0, FIXTURE_PRESET_COUNT - 1);
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
        output[i].mGel = safe.mGel;
        output[i].mFlickerProgram = safe.mFlickerProgram;
        output[i].mFlickerAmount = safe.mFlickerAmount;
        output[i].mFixtureMode = safe.mFixtureMode;
        output[i].mKelvin = safe.mKelvin;
        for (S32 slot = 0; slot < FIXTURE_GEL_SLOT_COUNT; ++slot)
        {
            output[i].mGelSlot[slot] = safe.mGelSlot[slot];
        }
        output[i].mSourceSizeM = safe.mSourceSizeM;
        output[i].mFixturePreset = safe.mFixturePreset;
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
    case 33:
        setLight(lights, 0, 60.f, 20.f, 15, 0.f, 0, true);
        setLight(lights, 2, -120.f, 25.f, 16, -1.f, 0, true);
        break;
    case 34:
        setLight(lights, 0, 30.f, -25.f, 6, -10.f, 0, true);
        setLight(lights, 1, 10.f, -35.f, 13, -2.5f, 1, true);
        break;
    case 35:
        setLight(lights, 0, 10.f, -20.f, 0, 0.f, 0, true);
        setLight(lights, 1, -25.f, -10.f, 13, -2.f, 1, true);
        break;
    case 36:
        setLight(lights, 0, 170.f, 2.f, 0, -2.5f, 0, true);
        setLight(lights, 1, -10.f, 10.f, 8, -3.5f, 1, true);
        setLight(lights, 3, 0.f, -20.f, 5, -3.f, 0, true);
        break;
    case 37:
        setLight(lights, 0, 0.f, 65.f, 1, 0.3f, 0, true);
        break;
    case 38:
    {
        static const F32 yaws[LIGHT_COUNT] = {
            45.f, -45.f, 135.f, -135.f
        };
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            setLight(lights, i, yaws[i], 20.f, 7, -10.f, 0, true);
        }
        break;
    }
    case 39:
        setLight(lights, 0, 0.f, 8.f, 23, -8.f, 2, true);
        setLight(lights, 1, -20.f, 10.f, 8, -3.5f, 1, true);
        break;
    case 40:
        setLight(lights, 0, 90.f, 15.f, 2, -10.f, 0, true);
        lights[0].mGobo = 4;
        setLight(lights, 1, 90.f, -15.f, 4, -10.f, 1, true);
        break;
    case 41:
        setLight(lights, 0, 40.f, 60.f, 7, -10.f, 0, false);
        setLight(lights, 1, -40.f, 60.f, 7, -10.f, 0, false);
        setLight(lights, 3, 180.f, 30.f, 20, -3.f, 0, true);
        break;
    case 42:
        setLight(lights, 0, 40.f, 55.f, 3, 0.5f, 0, true);
        setLight(lights, 1, -40.f, 20.f, 4, -2.f, 1, true);
        break;
    case 43:
        setLight(lights, 0, 0.f, 20.f, 21, -0.5f, 2, true);
        setLight(lights, 1, 0.f, 10.f, 14, -3.f, 1, true);
        break;
    case 44:
        setLight(lights, 0, 15.f, 10.f, 16, 0.f, 0, true);
        setLight(lights, 1, -15.f, 5.f, 10, -10.f, 0, false);
        break;
    case 45:
        setLight(lights, 0, 165.f, 5.f, 2, -10.f, 2, true);
        setLight(lights, 3, 0.f, -20.f, 20, -2.5f, 0, true);
        break;
    case 46:
        setLight(lights, 0, 45.f, 35.f, 1, 0.f, 1, true);
        setLight(lights, 1, -45.f, 10.f, 1, -1.5f, 1, true);
        setLight(lights, 2, -135.f, 40.f, 5, -1.5f, 0, true);
        setLight(lights, 3, 0.f, -20.f, 1, -2.5f, 0, true);
        break;
    case 47:
        setLight(lights, 0, 0.f, 15.f, 22, -0.2f, 0, true);
        setLight(lights, 1, 180.f, 15.f, 16, -0.2f, 0, true);
        setLight(lights, 3, 0.f, -20.f, 20, -2.5f, 0, true);
        break;
    case 48:
        setLight(lights, 0, 0.f, 82.f, 4, 0.5f, 2, true);
        setLight(lights, 1, -45.f, 10.f, 20, -10.f, 0, false);
        setLight(lights, 2, 90.f, 5.f, 14, -0.5f, 2, true);
        setLight(lights, 3, -90.f, 8.f, 16, -0.5f, 2, true);
        break;
    case 49:
        setLight(lights, 0, 0.f, -25.f, 15, -10.f, 0, true);
        setLight(lights, 1, -120.f, -20.f, 16, -10.f, 0, true);
        setLight(lights, 2, 120.f, -20.f, 22, -10.f, 0, true);
        setLight(lights, 3, 180.f, 10.f, 18, -3.5f, 1, true);
        break;
    case 50:
    {
        static const F32 yaws[LIGHT_COUNT] = {
            30.f, -60.f, 150.f, -120.f
        };
        static const F32 pitches[LIGHT_COUNT] = {
            5.f, 0.f, 10.f, 0.f
        };
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            setLight(lights, i, yaws[i], pitches[i], 23, -10.f, 2, true);
        }
        break;
    }
    case 51:
        setLight(lights, 0, 0.f, 68.f, 3, -0.5f, 2, true);
        setLight(lights, 2, 0.f, 75.f, 11, -10.f, 2, true);
        setLight(lights, 3, 180.f, -15.f, 20, -3.f, 0, true);
        break;
    case 52:
        setLight(lights, 0, 20.f, 10.f, 22, -1.f, 2, true);
        setLight(lights, 1, -30.f, -5.f, 18, -1.2f, 2, true);
        setLight(lights, 3, 180.f, 0.f, 12, -3.f, 1, true);
        break;
    case 53:
        setLight(lights, 0, 180.f, 10.f, 19, -1.f, 1, true);
        setLight(lights, 1, 180.f, 0.f, 22, -10.f, 0, false);
        setLight(lights, 3, 0.f, -15.f, 8, -3.5f, 1, true);
        break;
    case 54:
        setLight(lights, 0, 0.f, 10.f, 13, -10.f, 2, true);
        setLight(lights, 1, 0.f, 0.f, 16, -10.f, 0, true);
        setLight(lights, 3, 180.f, -10.f, 20, -3.f, 0, true);
        break;
    case 55:
        setLight(lights, 0, 40.f, 30.f, 4, -10.f, 0, true);
        setLight(lights, 1, -40.f, 30.f, 4, -10.f, 0, true);
        setLight(lights, 2, 180.f, 45.f, 4, -10.f, 0, true);
        setLight(lights, 3, 0.f, -30.f, 13, -10.f, 0, false);
        break;
    case 56:
        setLight(lights, 0, 25.f, -30.f, 22, -10.f, 0, true);
        setLight(lights, 3, 180.f, 20.f, 6, -4.f, 1, true);
        break;
    case 57:
        setLight(lights, 0, 60.f, 25.f, 15, -10.f, 0, true);
        setLight(lights, 1, -60.f, 25.f, 16, -10.f, 0, true);
        setLight(lights, 2, 150.f, 30.f, 22, -10.f, 0, true);
        setLight(lights, 3, -150.f, 30.f, 14, -10.f, 0, true);
        break;
    case 58:
    {
        static const F32 yaws[LIGHT_COUNT] = {
            45.f, -45.f, 135.f, -135.f
        };
        static const S32 profiles[LIGHT_COUNT] = {
            16, 14, 16, 14
        };
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            setLight(lights, i, yaws[i], -10.f, profiles[i], -3.f, 1, true);
        }
        break;
    }
    case 59:
        setLight(lights, 0, 10.f, -35.f, 19, -1.f, 1, true);
        setLight(lights, 1, -30.f, -25.f, 13, -1.5f, 1, true);
        setLight(lights, 3, 180.f, 20.f, 19, -3.5f, 0, true);
        break;
    case 60:
        setLight(lights, 0, 180.f, 25.f, 17, 0.5f, 0, true);
        setLight(lights, 2, -100.f, 12.f, 15, -1.5f, 2, true);
        setLight(lights, 3, 0.f, -25.f, 22, -2.f, 1, true);
        break;
    case 61:
        setLight(lights, 0, 0.f, 18.f, 0, -0.8f, 0, true);
        setLight(lights, 1, 180.f, 18.f, 0, -0.8f, 0, true);
        setLight(lights, 3, 0.f, -20.f, 12, -3.f, 1, true);
        break;
    case 62:
        setLight(lights, 0, 0.f, 35.f, 10, -10.f, 2, true);
        setLight(lights, 1, 40.f, 50.f, 10, -10.f, 0, true);
        setLight(lights, 2, -40.f, 55.f, 10, -10.f, 0, true);
        setLight(lights, 3, 180.f, 60.f, 10, -10.f, 0, true);
        break;
    case FX_CANDY_ORBIT:
        setLight(lights, 0, 30.f, 25.f, 15, -2.6f, 1, true);
        setLight(lights, 1, 0.f, 0.f, 20, -10.f, 0, false);
        setLight(lights, 2, 150.f, 30.f, 16, -2.6f, 1, true);
        setLight(lights, 3, -150.f, 20.f, 17, -2.6f, 1, true);
        break;
    case FX_RIMWAVE:
        setLight(lights, 0, 145.f, 25.f, 15, -0.65f, 2, true);
        setLight(lights, 1, 0.f, 0.f, 20, -10.f, 0, false);
        setLight(lights, 2, -145.f, 25.f, 16, -0.65f, 2, true);
        setLight(lights, 3, 180.f, -10.f, 22, -3.f, 0, true);
        break;
    case FX_AFTERHOURS_DRIFT:
        setLight(lights, 0, 65.f, 48.f, 5, -0.8f, 1, true);
        lights[0].mGobo = 10;
        setLight(lights, 1, 0.f, 0.f, 20, -10.f, 0, false);
        setLight(lights, 2, -155.f, 22.f, 13, -2.8f, 2, true);
        setLight(lights, 3, 180.f, -15.f, 15, -3.5f, 0, true);
        lights[3].mGobo = 23;
        break;
    case FX_SOMETHING_BEHIND_YOU:
        setLight(lights, 0, 95.f, 18.f, 23, -4.f, 2, true);
        lights[0].mGobo = 7;
        setLight(lights, 1, 0.f, 0.f, 20, -10.f, 0, false);
        setLight(lights, 2, -160.f, 25.f, 19, -10.f, 2, true);
        setLight(lights, 3, 180.f, -20.f, 20, -4.f, 0, true);
        break;
    default:
        break;
    }
}
} // namespace

bool easyRimOn(S32 presence)
{
    return presence != 0;
}

F32 easyRimEV(S32 presence)
{
    switch (std::clamp(presence, 0, 3))
    {
        case 1: return -2.5f;
        case 2: return -1.f;
        case 3: return 0.5f;
        default: return 0.f;
    }
}

bool easyBgOn(S32 presence)
{
    return presence != 0;
}

F32 easyBgEV(S32 presence)
{
    switch (std::clamp(presence, 0, 3))
    {
        case 1: return -3.5f;
        case 2: return -2.f;
        case 3: return -0.5f;
        default: return 0.f;
    }
}

S32 rimPresenceFromEV(bool on, F32 ev)
{
    if (!on)
    {
        return 0;
    }
    return ev < -1.75f ? 1 : ev < -0.25f ? 2 : 3;
}

S32 bgPresenceFromEV(bool on, F32 ev)
{
    if (!on)
    {
        return 0;
    }
    return ev < -2.75f ? 1 : ev < -1.25f ? 2 : 3;
}

F32 easyBrightnessClamp(F32 ev)
{
    return std::clamp(ev, -MAX_MASTER_EV, MAX_MASTER_EV);
}

F32 easyDramaClamp(F32 stops)
{
    return std::clamp(stops, 0.f, MAX_RATIO_STOPS);
}

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
    output.mRatioLock = setup.mRatioLock;
    output.mRatioStops = std::clamp(
        finiteOr(setup.mRatioStops, 0.f), 0.f, MAX_RATIO_STOPS);
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

F32 objectRadiusToSubjectScale(F32 radius_m)
{
    const F32 radius = std::isfinite(radius_m)
        ? std::max(0.f, radius_m) : 0.f;
    return std::clamp(radius / AVATAR_REF_RADIUS,
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

F32 ease(F32 t, U32 mode)
{
    if (!std::isfinite(t) || t <= 0.f)
    {
        return 0.f;
    }
    if (t >= 1.f)
    {
        return 1.f;
    }
    switch (mode)
    {
        case 0: // Linear
            return t;
        case 1: // Smoothstep
            return t * t * (3.f - 2.f * t);
        case 3: // Ease-Out (cubic)
        {
            const F32 inv = 1.f - t;
            return 1.f - inv * inv * inv;
        }
        case 2: // Ease-In-Out (cubic) - default
        default:
            break;
    }
    // Mode 2: Ease-In-Out cubic. Kept identical to the historical hardcoded
    // curve so default transitions are unchanged.
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
        out[i].mGel = base.mGel;
        out[i].mFlickerProgram = base.mFlickerProgram;
        out[i].mFlickerAmount = base.mFlickerAmount;
        out[i].mFixtureMode = base.mFixtureMode;
        out[i].mKelvin = base.mKelvin;
        for (S32 slot = 0; slot < FIXTURE_GEL_SLOT_COUNT; ++slot)
        {
            out[i].mGelSlot[slot] = base.mGelSlot[slot];
        }
        out[i].mSourceSizeM = base.mSourceSizeM;
        out[i].mFixturePreset = base.mFixturePreset;
    }
    if (safe_setup.mRatioLock)
    {
        // Fill is derived from the already-sanitized key and ratio domains.
        // Its lower bound is therefore MIN_EV - MAX_RATIO_STOPS, not the
        // user-entered EV floor applied by sanitizeSetup().
        out[1].mEV = out[0].mEV - safe_setup.mRatioStops;
    }
}

LightBase blendLight(const LightBase& start, const LightBase& target,
                      F32 eased)
{
    if (std::isnan(eased) || eased <= 0.f)
    {
        return start;
    }
    if (eased >= 1.f)
    {
        return target;
    }

    const LightBase safe_start = cleanLight(start, MIN_DERIVED_EV);
    const LightBase safe_target = cleanLight(target, MIN_DERIVED_EV);

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
    output.mGel = eased > 0.5f
        ? safe_target.mGel : safe_start.mGel;
    output.mFlickerProgram = eased > 0.5f
        ? safe_target.mFlickerProgram : safe_start.mFlickerProgram;
    output.mFlickerAmount = safe_start.mFlickerAmount +
        (safe_target.mFlickerAmount - safe_start.mFlickerAmount) * eased;
    output.mFixtureMode = eased > 0.5f
        ? safe_target.mFixtureMode : safe_start.mFixtureMode;
    const F32 start_mired = 1.0e6f / safe_start.mKelvin;
    const F32 target_mired = 1.0e6f / safe_target.mKelvin;
    const F32 blended_mired = start_mired +
        (target_mired - start_mired) * eased;
    output.mKelvin = 1.0e6f / blended_mired;
    for (S32 slot = 0; slot < FIXTURE_GEL_SLOT_COUNT; ++slot)
    {
        output.mGelSlot[slot] = eased > 0.5f
            ? safe_target.mGelSlot[slot] : safe_start.mGelSlot[slot];
    }
    output.mSourceSizeM = safe_start.mSourceSizeM +
        (safe_target.mSourceSizeM - safe_start.mSourceSizeM) * eased;
    output.mFixturePreset = eased > 0.5f
        ? safe_target.mFixturePreset : safe_start.mFixturePreset;
    return output;
}

Globals blendGlobals(const Globals& start, const Globals& target, F32 weight)
{
    const Globals a = sanitizeGlobals(start);
    const Globals b = sanitizeGlobals(target);
    const F32 t = std::clamp(finiteOr(weight, 0.f), 0.f, 1.f);
    Globals output;
    output.mMasterEV = a.mMasterEV + (b.mMasterEV - a.mMasterEV) * t;
    output.mHeadroomStops = a.mHeadroomStops +
        (b.mHeadroomStops - a.mHeadroomStops) * t;
    output.mBounceRatio = a.mBounceRatio +
        (b.mBounceRatio - a.mBounceRatio) * t;
    output.mTransitionSec = a.mTransitionSec +
        (b.mTransitionSec - a.mTransitionSec) * t;
    output.mBounceEnabled = t > 0.5f ? b.mBounceEnabled : a.mBounceEnabled;
    output.mPower = t > 0.5f ? b.mPower : a.mPower;
    output.mSeed = t > 0.5f ? b.mSeed : a.mSeed;
    output.mSubjectScale = a.mSubjectScale +
        (b.mSubjectScale - a.mSubjectScale) * t;
    // This field is already a mired delta, so linear interpolation is the
    // correct reciprocal-temperature transition.
    output.mMasterTempMired = a.mMasterTempMired +
        (b.mMasterTempMired - a.mMasterTempMired) * t;
    return sanitizeGlobals(output);
}

Transforms blendTransforms(const Transforms& start, const Transforms& target,
                           F32 weight)
{
    const Transforms a = sanitizeTransforms(start);
    const Transforms b = sanitizeTransforms(target);
    const F32 t = std::clamp(finiteOr(weight, 0.f), 0.f, 1.f);
    Transforms output;
    output.mMirror = t > 0.5f ? b.mMirror : a.mMirror;
    output.mFacingAzimuthDeg = wrap180(a.mFacingAzimuthDeg +
        wrap180(b.mFacingAzimuthDeg - a.mFacingAzimuthDeg) * t);
    output.mYawDeg = wrap180(
        a.mYawDeg + wrap180(b.mYawDeg - a.mYawDeg) * t);
    output.mPitchDeg = a.mPitchDeg + (b.mPitchDeg - a.mPitchDeg) * t;
    return sanitizeTransforms(output);
}

Cue sanitizeCue(const Cue& input)
{
    Cue output = input;
    if (output.mLabel.size() > 128)
    {
        output.mLabel.resize(128);
    }
    if (output.mProvenance.size() > 256)
    {
        output.mProvenance.resize(256);
    }
    output.mFadeSec = std::clamp(finiteOr(output.mFadeSec, 3.f), 0.f, 3600.f);
    output.mDelaySec = std::clamp(finiteOr(output.mDelaySec, 0.f), 0.f, 3600.f);
    output.mProfile = std::clamp(
        output.mProfile, 0, CUE_FADE_PROFILE_COUNT - 1);
    output.mFollow = std::clamp(output.mFollow, -1, 86400);
    output.mAtSec = std::clamp(finiteOr(output.mAtSec, 0.0), 0.0, 86400.0);
    output.mSetup = sanitizeSetup(output.mSetup);
    output.mGlobals = sanitizeGlobals(output.mGlobals);
    output.mTransforms = sanitizeTransforms(output.mTransforms);
    output.mFX = std::clamp(output.mFX, -1, FX_COUNT - 1);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        const F32 value = finiteOr(output.mShadowSoftOverride[i], -1.f);
        output.mShadowSoftOverride[i] = value < 0.f
            ? -1.f : std::clamp(value, 0.f, 8.f);
    }
    return output;
}

CueList sanitizeCueList(const CueList& input)
{
    CueList output;
    output.mName = input.mName.substr(0, 128);
    output.mTimecodeMode = input.mTimecodeMode;
    const std::size_t count = std::min<std::size_t>(input.mCues.size(), 256);
    output.mCues.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        output.mCues.push_back(sanitizeCue(input.mCues[i]));
    }
    return output;
}

CueState cueTargetState(const Cue& input)
{
    const Cue cue = sanitizeCue(input);
    CueState output;
    output.mSetup = cue.mSetup;
    output.mGlobals = cue.mGlobals;
    output.mTransforms = cue.mTransforms;
    output.mFX = cue.mFX;
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        output.mShadowSoftOverride[i] = cue.mShadowSoftOverride[i];
    }
    return output;
}

F32 cueFadeWeight(S32 profile, F64 presentation_time, F64 go_time,
                  F32 delay_sec, F32 fade_sec)
{
    const F64 now = finiteOr(presentation_time, 0.0);
    const F64 start = finiteOr(go_time, now) +
        std::clamp(static_cast<F64>(finiteOr(delay_sec, 0.f)), 0.0, 3600.0);
    if (now < start)
    {
        return 0.f;
    }
    const S32 safe_profile = std::clamp(
        profile, 0, CUE_FADE_PROFILE_COUNT - 1);
    const F32 duration = std::clamp(finiteOr(fade_sec, 0.f), 0.f, 3600.f);
    if (safe_profile == CUE_FADE_SNAP || duration <= 0.f)
    {
        return 1.f;
    }
    const F32 linear = std::clamp(
        static_cast<F32>((now - start) / duration), 0.f, 1.f);
    return safe_profile == CUE_FADE_LINEAR
        ? linear
        : linear * linear * (3.f - 2.f * linear);
}

CueState evaluateCueTransition(const CueState& input_start,
                               const Cue& input_target,
                               F64 presentation_time, F64 go_time)
{
    const Cue target = sanitizeCue(input_target);
    CueState start = input_start;
    start.mSetup = sanitizeSetup(start.mSetup);
    start.mGlobals = sanitizeGlobals(start.mGlobals);
    start.mTransforms = sanitizeTransforms(start.mTransforms);
    start.mFX = std::clamp(start.mFX, -1, FX_COUNT - 1);
    const F32 weight = cueFadeWeight(
        target.mProfile, presentation_time, go_time,
        target.mDelaySec, target.mFadeSec);
    CueState output;
    output.mSetup.mRadius = start.mSetup.mRadius +
        (target.mSetup.mRadius - start.mSetup.mRadius) * weight;
    output.mSetup.mRatioLock = weight > 0.5f
        ? target.mSetup.mRatioLock : start.mSetup.mRatioLock;
    output.mSetup.mRatioStops = start.mSetup.mRatioStops +
        (target.mSetup.mRatioStops - start.mSetup.mRatioStops) * weight;
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        output.mSetup.mLights[i] = blendLight(
            start.mSetup.mLights[i], target.mSetup.mLights[i], weight);
        const F32 a = start.mShadowSoftOverride[i];
        const F32 b = target.mShadowSoftOverride[i];
        output.mShadowSoftOverride[i] = a >= 0.f && b >= 0.f
            ? a + (b - a) * weight
            : (weight > 0.5f ? b : a);
    }
    output.mSetup = sanitizeSetup(output.mSetup);
    output.mGlobals = blendGlobals(start.mGlobals, target.mGlobals, weight);
    output.mTransforms = blendTransforms(
        start.mTransforms, target.mTransforms, weight);
    const F64 arm_time = finiteOr(go_time, presentation_time) + target.mDelaySec;
    output.mFX = presentation_time >= arm_time ? target.mFX : start.mFX;
    return output;
}

static S32 timecodeCueIndexSanitized(const CueList& list,
                                     F64 presentation_time)
{
    const F64 now = finiteOr(presentation_time, 0.0);
    S32 best = -1;
    F64 best_at = -1.0;
    for (S32 i = 0; i < static_cast<S32>(list.mCues.size()); ++i)
    {
        const F64 at = list.mCues[i].mAtSec;
        if (at <= now && (best < 0 || at > best_at ||
                         (at == best_at && i > best)))
        {
            best = i;
            best_at = at;
        }
    }
    return best;
}

S32 timecodeCueIndex(const CueList& input, F64 presentation_time)
{
    return timecodeCueIndexSanitized(
        sanitizeCueList(input), presentation_time);
}

CueState evaluateTimecodeCueList(const CueList& input,
                                 const CueState& released_state,
                                 F64 presentation_time,
                                 S32* active_index)
{
    const CueList list = sanitizeCueList(input);
    const S32 current = timecodeCueIndexSanitized(list, presentation_time);
    if (active_index)
    {
        *active_index = current;
    }
    if (current < 0)
    {
        return released_state;
    }
    S32 previous = -1;
    F64 previous_at = -1.0;
    for (S32 i = 0; i < static_cast<S32>(list.mCues.size()); ++i)
    {
        const F64 at = list.mCues[i].mAtSec;
        // Mirror the later-row tie-break used to pick `current`: among cues
        // sharing an earlier timestamp, the LATER row is what is actually
        // displayed during that window, so the fade must start from it.
        if (i != current && at < list.mCues[current].mAtSec &&
            (at > previous_at || (at == previous_at && i > previous)))
        {
            previous = i;
            previous_at = at;
        }
    }
    const CueState start = previous >= 0
        ? cueTargetState(list.mCues[previous]) : released_state;
    return evaluateCueTransition(
        start, list.mCues[current], presentation_time,
        list.mCues[current].mAtSec);
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

void applyGel(S32 index, F32 linear_rgb[3])
{
    if (!linear_rgb || index <= 0 || index >= GEL_COUNT)
    {
        return;
    }
    const Gel& gel = GELS[index];
    F32 multiplier[3] = {
        gel.mMultiplier[0], gel.mMultiplier[1], gel.mMultiplier[2]
    };
    if (gel.mColourTemperature)
    {
        masterTempGain(gel.mMiredShift, multiplier);
    }
    for (S32 channel = 0; channel < 3; ++channel)
    {
        linear_rgb[channel] = std::clamp(
            finiteOr(linear_rgb[channel], 0.f) * multiplier[channel],
            0.f, 1.f);
    }
}

const char* gelName(S32 index)
{
    return GELS[std::clamp(index, 0, GEL_COUNT - 1)].mName;
}

bool gelIsColourTemperature(S32 index)
{
    return index > 0 && index < GEL_COUNT &&
           GELS[index].mColourTemperature;
}

F32 gelMiredShift(S32 index)
{
    return index > 0 && index < GEL_COUNT &&
           GELS[index].mColourTemperature
        ? GELS[index].mMiredShift : 0.f;
}

const char* fixtureGelName(S32 index)
{
    return FIXTURE_GELS[
        std::clamp(index, 0, FIXTURE_GEL_COUNT - 1)].mName;
}

bool fixtureGelIsColourTemperature(S32 index)
{
    return index > 0 && index < FIXTURE_GEL_COUNT &&
           FIXTURE_GELS[index].mColourTemperature;
}

F32 fixtureGelMiredShift(S32 index)
{
    return fixtureGelIsColourTemperature(index)
        ? FIXTURE_GELS[index].mMiredShift : 0.f;
}

void fixtureWhite(F32 kelvin,
                  const S32 gel_slots[FIXTURE_GEL_SLOT_COUNT],
                  F32 master_mired, F32 out_linear_rgb[3])
{
    if (!out_linear_rgb)
    {
        return;
    }

    const F64 safe_kelvin = std::clamp(
        static_cast<F64>(finiteOr(kelvin, 5600.f)),
        static_cast<F64>(FIXTURE_KELVIN_MIN),
        static_cast<F64>(FIXTURE_KELVIN_MAX));
    F64 total_mired = 1.0e6 / safe_kelvin;
    total_mired += std::clamp(
        finiteOr(master_mired, 0.f),
        MASTER_TEMP_MIRED_MIN, MASTER_TEMP_MIRED_MAX);
    if (gel_slots)
    {
        for (S32 slot = 0; slot < FIXTURE_GEL_SLOT_COUNT; ++slot)
        {
            total_mired += fixtureGelMiredShift(gel_slots[slot]);
        }
    }
    total_mired = std::clamp(total_mired, 40.0, 500.0);

    F64 white[3];
    planckianLinearRGB(1.0e6 / total_mired, white);
    const F64 green = std::max(white[1], 1.0e-6);
    F64 normalized[3] = {
        white[0] / green,
        1.0,
        white[2] / green,
    };
    if (gel_slots)
    {
        for (S32 slot = 0; slot < FIXTURE_GEL_SLOT_COUNT; ++slot)
        {
            const S32 index = std::clamp(
                gel_slots[slot], 0, FIXTURE_GEL_COUNT - 1);
            const FixtureGel& gel = FIXTURE_GELS[index];
            if (gel.mColourTemperature)
            {
                continue;
            }
            for (S32 channel = 0; channel < 3; ++channel)
            {
                normalized[channel] *= gel.mMultiplier[channel];
            }
        }
    }
    for (S32 channel = 0; channel < 3; ++channel)
    {
        out_linear_rgb[channel] = static_cast<F32>(
            std::clamp(normalized[channel], 0.0, 1.0));
    }
}

const FixturePreset& fixturePreset(S32 index)
{
    return FIXTURES[std::clamp(index, 0, FIXTURE_PRESET_COUNT - 1)];
}

const char* fixturePresetName(S32 index)
{
    return fixturePreset(index).mName;
}

F32 penumbraSoftness(F32 source_size_m, F32 distance_m)
{
    const F32 source = std::clamp(
        finiteOr(source_size_m, FIXTURE_REFERENCE_SOURCE_SIZE_M),
        FIXTURE_SOURCE_SIZE_MIN, FIXTURE_SOURCE_SIZE_MAX);
    const F32 distance = std::max(finiteOr(distance_m, 0.25f), 0.25f);
    const F32 theta = 2.f * std::atan(source / (2.f * distance));
    const F32 theta_degrees = theta * RADIANS_TO_DEGREES;
    const F32 normalized = std::clamp(
        (theta_degrees - 0.5f) / (60.f - 0.5f), 0.f, 1.f);
    return 8.f * std::sqrt(normalized);
}

void catchlightRadialOffset(F32 subject_scale, F32 angle_degrees,
                            F32 out_right_up[2])
{
    if (!out_right_up)
    {
        return;
    }
    const F32 scale = sanitizeSubjectScale(subject_scale);
    const F32 angle = wrap180(finiteOr(angle_degrees, 120.f)) *
                      DEGREES_TO_RADIANS;
    out_right_up[0] = std::cos(angle) * CATCHLIGHT_RADIAL_M * scale;
    out_right_up[1] = std::sin(angle) * CATCHLIGHT_RADIAL_M * scale;
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
    out.mCatchlightSizeScale = 1.f;

    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        const LightBase light = cleanLight(live[i], MIN_DERIVED_EV);
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
        if (light.mFixtureMode)
        {
            F32 linear_rgb[3];
            fixtureWhite(light.mKelvin, light.mGelSlot,
                         safe_globals.mMasterTempMired, linear_rgb);
            for (S32 channel = 0; channel < 3; ++channel)
            {
                rgb[channel] = linearChannelToSRGB(linear_rgb[channel]);
            }
        }
        else
        {
            // Keep the full legacy profile/master/single-gel route isolated so
            // fixture-off frames retain the shipped 1.x arithmetic and order.
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
            if (light.mGel != 0)
            {
                F32 linear_rgb[3] = {
                    srgbChannelToLinear(rgb[0]),
                    srgbChannelToLinear(rgb[1]),
                    srgbChannelToLinear(rgb[2])
                };
                applyGel(light.mGel, linear_rgb);
                for (S32 channel = 0; channel < 3; ++channel)
                {
                    rgb[channel] = linearChannelToSRGB(linear_rgb[channel]);
                }
            }
        }

        const F32 softness = light.mFixtureMode
            ? penumbraSoftness(light.mSourceSizeM, effective_radius) : 0.f;
        const F32 softness_ratio = softness / 8.f;
        out.mDerivedShadowSoftness[i] = softness;
        if (i == 0 && light.mFixtureMode)
        {
            out.mCatchlightSizeScale = std::sqrt(
                light.mSourceSizeM / FIXTURE_REFERENCE_SOURCE_SIZE_M);
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
        const F32 base_falloff = beamFalloff(light.mBeam);
        projector.mFalloff = light.mFixtureMode
            ? base_falloff + (0.35f - base_falloff) * softness_ratio
            : base_falloff;
        const F32 base_fov = beamFov(light.mBeam);
        projector.mFovRad = light.mFixtureMode
            ? std::clamp(base_fov * (1.f + 0.18f * softness_ratio),
                         0.f, PROJECTOR_FOV_MAX)
            : base_fov;
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
            raw_intensity * safe_globals.mBounceRatio *
            (light.mFixtureMode ? 1.f + 0.5f * softness_ratio : 1.f);
        const bool omni_on = light.mOn && safe_globals.mPower &&
            safe_globals.mBounceEnabled &&
            pre_headroom * safe_globals.mBounceRatio *
                (light.mFixtureMode ? 1.f + 0.5f * softness_ratio : 1.f) >
                0.001f;
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
    case 33: // Neon Buzz
    {
        const F64 cycle = positiveFmod(fs, 160.0);
        lights[2].mEV = -1.f + 0.05f * phaseSin(fs * 1.2);
        if (cycle < 12.0)
        {
            lights[0].mEV = (step & 1) ? 0.5f : -10.f;
        }
        else
        {
            const F32 r = unitHash(seed, fx, counter, 0, 0);
            if (r > 0.35f)
            {
                lights[0].mEV = 0.3f + 0.15f *
                    unitHash(seed, fx, counter, 0, 1);
            }
            else if (r > 0.20f)
            {
                lights[0].mEV = -2.f;
            }
            else
            {
                lights[0].mEV = -10.f;
            }
        }
        break;
    }
    case 34: // Welding Arc
    {
        const F64 work = positiveFmod(fs, 120.0);
        if (work < 70.0)
        {
            const F32 r = unitHash(seed, fx, counter, 0, 0);
            lights[0].mEV = r > 0.25f
                ? 1.8f + 0.7f * unitHash(seed, fx, counter, 0, 1)
                : -10.f;
            lights[1].mEV = -2.5f + static_cast<F32>(
                std::min(1.5, work / 70.0 * 1.5)) +
                0.1f * unitHash(seed, fx, counter, 1, 0);
        }
        else
        {
            lights[0].mEV = -10.f;
            lights[1].mEV = -1.f -
                static_cast<F32>((work - 70.0) / 50.0) * 2.f;
        }
        break;
    }
    case 35: // Candle Draft
    {
        const F32 n = valueNoise(seed, fx, fs * 0.9, 0, 0);
        lights[0].mEV = -0.4f + 0.8f * n;
        lights[0].mPitchDeg = -20.f + 6.f *
            (valueNoise(seed, fx, fs * 0.6, 0, 1) - 0.5f);
        lights[0].mYawDeg = 10.f + 8.f *
            (valueNoise(seed, fx, fs * 0.6, 0, 2) - 0.5f);
        const F32 g = valueNoise(seed, fx, fs * 0.15, 0, 3);
        if (g > 0.8f)
        {
            lights[0].mEV -= (g - 0.8f) * 7.5f;
        }
        lights[1].mEV = -2.f + 0.32f * n;
        break;
    }
    case 36: // Sunrise Sweep
    {
        const F64 u = positiveFmod(fs, 240.0) / 240.0;
        lights[0].mPitchDeg = 2.f + static_cast<F32>(u) * 48.f;
        lights[0].mEV = -2.5f + static_cast<F32>(u) * 3.f;
        lights[0].mProfile = u < 0.20 ? 0 : u < 0.45 ? 17 :
            u < 0.70 ? 1 : 3;
        lights[1].mEV = -3.5f + static_cast<F32>(u) * 2.5f;
        lights[1].mProfile = u < 0.60 ? 8 : 4;
        lights[3].mEV = -3.f + static_cast<F32>(u) * 2.f;
        break;
    }
    case 37: // Dying Bulb
    {
        const F64 b = positiveFmod(fs, 200.0);
        if (b < 120.0)
        {
            lights[0].mEV = 0.3f - static_cast<F32>(b / 120.0) * 0.9f +
                0.1f * (valueNoise(seed, fx, fs * 0.5, 0, 0) - 0.5f);
        }
        else if (b < 170.0)
        {
            const F32 r = unitHash(seed, fx, counter, 0, 0);
            if (r > 0.6f)
            {
                lights[0].mEV = -0.6f -
                    static_cast<F32>((b - 120.0) / 50.0) * 1.5f;
            }
            else if (r > 0.3f)
            {
                lights[0].mEV = -3.f;
            }
            else
            {
                lights[0].mEV = -10.f;
            }
        }
        else if (b < 172.0)
        {
            lights[0].mEV = 1.5f;
            lights[0].mProfile = 23;
        }
        else if (b < 180.0)
        {
            lights[0].mProfile = 11;
            lights[0].mEV = -4.f - static_cast<F32>(b - 172.0) * 0.75f;
        }
        else
        {
            lights[0].mOn = false;
        }
        break;
    }
    case 38: // Rave Chase
    {
        const S64 active = positiveMod(step, 4);
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            lights[i].mProfile = 7 + static_cast<S32>(
                positiveMod(step, 16));
            lights[i].mEV = i == active ? 1.f : -10.f;
        }
        if (positiveMod(step, 32) == 31)
        {
            for (LightBase& light : lights)
            {
                light.mEV = 1.2f;
            }
        }
        break;
    }
    case 39: // Lighthouse
    {
        const F64 theta = positiveFmod(6.0 * fs, 360.0);
        lights[0].mYawDeg = wrap180(static_cast<F32>(theta));
        const F32 c = std::max(
            0.f, phaseCos(theta * DEGREES_TO_RADIANS));
        lights[0].mEV = -8.f + 9.5f * std::pow(c, 24.f);
        break;
    }
    case 40: // Night Train
    {
        const F64 b = positiveFmod(fs, 100.0);
        if (b < 70.0)
        {
            const F32 env = phaseSin((TWO_PI * 0.5) * b / 70.0);
            const F64 w = positiveFmod(fs, 3.0);
            lights[0].mEV = w < 1.5 ? -1.f + 2.f * env : -6.f;
            lights[1].mEV = -3.5f + env;
        }
        else
        {
            lights[0].mOn = false;
            lights[1].mOn = false;
        }
        break;
    }
    case 41: // Fireworks Finale
    {
        const U64 cycle = static_cast<U64>(step / 25);
        const S64 local = step - static_cast<S64>(cycle) * 25;
        for (S32 i = 0; i < 2; ++i)
        {
            if (unitHash(seed, fx, cycle, i, 0) < 0.7f)
            {
                const S64 offset = static_cast<S64>(
                    unitHash(seed, fx, cycle, i, 1) * 10.f);
                if (local >= offset)
                {
                    const F32 ev = 1.8f -
                        static_cast<F32>(local - offset) * 0.28f;
                    if (ev > -6.f)
                    {
                        lights[i].mOn = true;
                        lights[i].mEV = ev;
                        lights[i].mProfile = 7 + static_cast<S32>(16.f *
                            unitHash(seed, fx, cycle, i, 2));
                        lights[i].mYawDeg = wrap180(lights[i].mYawDeg +
                            60.f * (unitHash(seed, fx, cycle, i, 3) - 0.5f));
                    }
                }
            }
        }
        break;
    }
    case 42: // Passing Clouds
    {
        const F32 cover = valueNoise(seed, fx, fs * 0.05, 0, 0);
        lights[0].mEV = 0.5f - cover * 2.2f;
        if (cover > 0.8f)
        {
            lights[0].mProfile = 4;
        }
        lights[1].mEV = -2.f - cover * 0.5f;
        break;
    }
    case 43: // Will-o'-Wisp
        lights[0].mYawDeg = 360.f *
            valueNoise(seed, fx, fs * 0.07, 0, 0) - 180.f;
        lights[0].mPitchDeg = -10.f + 50.f *
            valueNoise(seed, fx, fs * 0.09, 0, 1);
        lights[0].mEV = -0.5f + 0.8f * phaseSin(fs * 0.5) +
            0.4f * (valueNoise(seed, fx, fs * 0.3, 0, 2) - 0.5f);
        lights[1].mYawDeg = lights[0].mYawDeg * 0.5f;
        lights[1].mEV = -3.f + 0.3f * phaseSin(fs * 0.5);
        break;
    case 44: // Hologram Glitch
    {
        lights[0].mEV = 0.15f * phaseSin(fs * 3.0);
        const F32 g = unitHash(seed, fx, counter, 0, 0);
        if (g > 0.92f)
        {
            const F32 jump = 30.f *
                (unitHash(seed, fx, counter, 0, 1) - 0.5f);
            lights[0].mYawDeg += jump;
            lights[0].mEV += 0.6f;
            lights[1].mOn = true;
            lights[1].mYawDeg = -15.f - jump;
            lights[1].mEV = -0.5f;
        }
        else if (g < 0.04f)
        {
            lights[0].mOn = false;
        }
        break;
    }
    case 45: // Signal Lamp
    {
        static const U64 SOS_MASK =
            (1ULL << 0) | (1ULL << 2) | (1ULL << 4) |
            (7ULL << 8) | (7ULL << 12) | (7ULL << 16) |
            (1ULL << 22) | (1ULL << 24) | (1ULL << 26);
        const U64 b = static_cast<U64>(positiveMod(step, 36));
        lights[0].mEV = ((SOS_MASK >> b) & 1ULL) ? 1.2f : -10.f;
        break;
    }
    case 46: // Breathing Swell
    {
        const F32 s = phaseSin(fs * 0.157);
        for (LightBase& light : lights)
        {
            light.mEV += 0.6f * s;
        }
        lights[0].mPitchDeg = 35.f + 2.f * s;
        break;
    }
    case 47: // Arcane Orbit
    {
        lights[0].mYawDeg = wrap180(static_cast<F32>(
            positiveFmod(7.2 * fs, 360.0)));
        lights[1].mYawDeg = wrap180(static_cast<F32>(
            positiveFmod(-7.2 * fs + 180.0, 360.0)));
        const F32 base = -0.2f + 0.3f * phaseSin(fs * 0.3);
        lights[0].mEV = lights[1].mEV = base;
        const F64 d = positiveFmod(14.4 * fs, 180.0);
        if (d < 10.0 || d > 170.0)
        {
            lights[0].mEV = base + 1.f;
            lights[1].mEV = base + 1.f;
        }
        break;
    }
    case 48: // Rock With You
    {
        lights[0].mEV = 0.5f + 0.15f * phaseSin(fs * 0.157);

        lights[2].mYawDeg = wrap180(static_cast<F32>(
            positiveFmod(9.0 * fs, 360.0)));
        lights[2].mPitchDeg = 5.f + 18.f * phaseSin(fs * 0.35);

        lights[3].mYawDeg = wrap180(static_cast<F32>(
            positiveFmod(-12.0 * fs + 180.0, 360.0)));
        lights[3].mPitchDeg = 8.f + 14.f * phaseCos(fs * 0.27);

        lights[2].mEV = -0.5f + 0.4f *
            (valueNoise(seed, fx, fs * 0.8, 2, 0) - 0.5f);
        lights[3].mEV = -0.5f + 0.4f *
            (valueNoise(seed, fx, fs * 0.8, 3, 0) - 0.5f);

        const F64 d = positiveFmod(21.0 * fs, 180.0);
        if (d < 6.0 || d > 174.0)
        {
            lights[2].mEV += 0.8f;
            lights[3].mEV += 0.8f;
        }

        if (positiveMod(step / 80, 2) == 1)
        {
            lights[2].mProfile = 16;
            lights[3].mProfile = 14;
        }
        break;
    }
    case 49: // Boogie Floor
    {
        const F64 beatpos = positiveFmod(fs, 4.0) / 4.0;
        const F64 env = (1.0 - beatpos) * (1.0 - beatpos);
        const F32 push = positiveMod(step / 4, 4) == 0 ? 0.6f : 0.f;
        static const S32 FLOOR[4] = { 15, 16, 22, 14 };
        for (S32 i = 0; i < 3; ++i)
        {
            lights[i].mProfile = FLOOR[positiveMod(step / 16 + i, 4)];
            lights[i].mEV = -2.5f +
                (2.8f + push) * static_cast<F32>(env);
        }
        lights[3].mEV = -3.5f + 0.4f * static_cast<F32>(env);
        break;
    }
    case 50: // Shootout
    {
        const U64 window = static_cast<U64>(step / 40);
        const S64 local = step - static_cast<S64>(window) * 40;
        if (unitHash(seed, fx, window, 0, 0) < 0.75f)
        {
            const S32 shooter = static_cast<S32>(4.f *
                unitHash(seed, fx, window, 0, 1)) & 3;
            const S32 rounds = 4 + static_cast<S32>(6.f *
                unitHash(seed, fx, window, 0, 2));
            const S64 start = static_cast<S64>(10.f *
                unitHash(seed, fx, window, 0, 3));
            const S64 k = local - start;
            if (k >= 0 && k < rounds * 2 && (k & 1) == 0)
            {
                lights[shooter].mEV = 2.f + 0.4f *
                    unitHash(seed, fx, counter, shooter, 4);
                lights[shooter].mYawDeg = wrap180(
                    lights[shooter].mYawDeg + 6.f *
                    (unitHash(seed, fx, counter, shooter, 5) - 0.5f));
            }
            if (unitHash(seed, fx, window, 1, 0) < 0.5f)
            {
                const S32 replier = (shooter + 2) & 3;
                const S64 j = local - 20 - static_cast<S64>(8.f *
                    unitHash(seed, fx, window, 1, 1));
                if (j >= 0 && j < 8 && (j & 1) == 0)
                {
                    lights[replier].mEV = 1.8f;
                }
            }
        }
        break;
    }
    case 51: // Chopper Hunt
    {
        const F64 a = positiveFmod(4.0 * fs, 360.0);
        lights[0].mYawDeg = wrap180(static_cast<F32>(a + 25.f *
            (valueNoise(seed, fx, fs * 0.3, 0, 0) - 0.5f)));
        lights[0].mPitchDeg = 68.f + 12.f *
            (valueNoise(seed, fx, fs * 0.35, 0, 1) - 0.5f);
        F32 c = phaseCos(a * DEGREES_TO_RADIANS);
        if (c < 0.f)
        {
            c = 0.f;
        }
        lights[0].mEV = -0.5f + 2.f * std::pow(c, 6.f);
        if (step & 1)
        {
            lights[0].mEV -= 0.3f;
        }
        lights[2].mYawDeg = lights[0].mYawDeg;
        lights[2].mEV = positiveMod(step, 10) == 0 ? 0.2f : -10.f;
        break;
    }
    case 52: // Plasma Globe
    {
        for (S32 i = 0; i < 2; ++i)
        {
            lights[i].mYawDeg = 360.f *
                valueNoise(seed, fx, fs * 0.6, i, 0) - 180.f;
            lights[i].mPitchDeg = 60.f *
                valueNoise(seed, fx, fs * 0.7, i, 1) - 20.f;
            lights[i].mEV = -1.2f + 0.8f *
                valueNoise(seed, fx, fs * 1.5, i, 2);
            lights[i].mProfile = i == 0 ? 22 : 18;
        }
        const F32 r = unitHash(seed, fx, counter, 0, 3);
        if (r > 0.94f)
        {
            const S32 j = r > 0.97f ? 1 : 0;
            lights[j].mEV = 1.6f;
            lights[j].mProfile = 23;
        }
        lights[3].mEV = -3.f + 0.3f * phaseSin(fs * 0.25);
        break;
    }
    case 53: // Dimensional Rift
        lights[0].mEV = -1.f + 0.6f * phaseSin(fs * 0.5) +
            0.4f * phaseSin(fs * 0.13) + 0.5f *
            (valueNoise(seed, fx, fs * 0.4, 0, 0) - 0.5f);
        lights[0].mYawDeg = wrap180(180.f + 20.f *
            (valueNoise(seed, fx, fs * 0.15, 0, 1) - 0.5f));
        if (unitHash(seed, fx, counter, 0, 2) > 0.96f)
        {
            lights[1].mOn = true;
            lights[1].mEV = 1.3f;
            lights[3].mEV = -2.f;
        }
        break;
    case 54: // Kawoosh
    {
        const F64 b = positiveFmod(fs, 120.0);
        if (b < 56.0)
        {
            const S64 chev = static_cast<S64>(b / 8.0);
            const F64 lk = positiveFmod(b, 8.0);
            lights[0].mYawDeg = wrap180(-135.f +
                45.f * static_cast<F32>(chev));
            lights[0].mEV = lk < 3.0 ? 0.8f : -2.5f;
        }
        else if (b < 60.0)
        {
            lights[0].mEV = -10.f;
            lights[1].mEV = 2.2f;
            lights[1].mProfile = 23;
        }
        else if (b < 105.0)
        {
            lights[1].mProfile = 16;
            lights[1].mEV = -0.6f + 0.5f *
                (valueNoise(seed, fx, fs * 0.9, 1, 0) - 0.5f) +
                0.2f * phaseSin(fs * 1.7);
            lights[3].mEV = -2.5f;
        }
        else
        {
            lights[0].mEV = -10.f;
            lights[1].mEV = -0.6f -
                static_cast<F32>((b - 105.0) / 15.0) * 9.4f;
        }
        break;
    }
    case 55: // Time Circuits
    {
        const F64 b = positiveFmod(fs, 100.0);
        if (b < 80.0)
        {
            const F64 p = 0.004 * b * b;
            const bool gate = positiveFmod(p, 1.0) < 0.5;
            const S64 active = positiveMod(
                static_cast<S64>(p * 3.0), 3);
            for (S32 i = 0; i < 3; ++i)
            {
                lights[i].mEV = gate && i == static_cast<S32>(active) ?
                    0.9f : -10.f;
            }
        }
        else if (b < 84.0)
        {
            for (S32 i = 0; i < 3; ++i)
            {
                lights[i].mEV = 2.5f;
                lights[i].mProfile = 23;
            }
        }
        else if (b < 95.0)
        {
            for (S32 i = 0; i < 3; ++i)
            {
                lights[i].mEV = -10.f;
            }
            lights[3].mOn = true;
            lights[3].mEV = 0.5f -
                static_cast<F32>(b - 84.0) * 0.55f;
        }
        break;
    }
    case 56: // Jacob's Ladder
    {
        const F64 b = positiveFmod(fs, 40.0);
        if (b < 30.0)
        {
            const F64 u = b / 30.0;
            lights[0].mPitchDeg = -30.f +
                100.f * static_cast<F32>(u);
            const F32 r = unitHash(seed, fx, counter, 0, 0);
            lights[0].mEV = r > 0.3f ? 0.6f + 0.5f *
                unitHash(seed, fx, counter, 0, 1) : -3.f;
            lights[0].mProfile = r > 0.8f ? 23 : 22;
            lights[3].mEV = -4.f + 1.2f * static_cast<F32>(u);
        }
        else
        {
            lights[0].mEV = -10.f;
        }
        break;
    }
    case 57: // Build & Drop
    {
        const F64 b = positiveFmod(fs, 160.0);
        if (b < 80.0)
        {
            const F64 p = 0.003 * b * b;
            const bool gate = positiveFmod(p, 1.0) < 0.5;
            const F32 ev = -2.f +
                static_cast<F32>(b / 80.0) * 3.f;
            lights[0].mEV = gate ? ev : -10.f;
            lights[1].mEV = gate ? -10.f : ev;
            lights[2].mEV = lights[3].mEV = -10.f;
        }
        else if (b < 90.0)
        {
            for (LightBase& light : lights)
            {
                light.mEV = -10.f;
            }
        }
        else
        {
            const bool on = positiveMod(step, 2) == 0;
            const S32 profile = 7 + static_cast<S32>(
                positiveMod(step / 2, 16));
            for (LightBase& light : lights)
            {
                light.mProfile = profile;
                light.mEV = on ? 1.5f : -10.f;
            }
        }
        break;
    }
    case 58: // Biolume Tide
    {
        const F64 w = fs * 0.35;
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            F32 crest = phaseCos(w - static_cast<F64>(i) *
                (TWO_PI / 4.0));
            if (crest < 0.f)
            {
                crest = 0.f;
            }
            lights[i].mEV = -3.f + 1.6f * crest + 0.5f *
                (valueNoise(seed, fx, fs * 0.8, i, 0) - 0.5f);
            if (unitHash(seed, fx, counter, i, 1) > 0.985f)
            {
                lights[i].mEV = 0.8f;
            }
        }
        break;
    }
    case 59: // Mount Doom
    {
        const F32 surge = valueNoise(seed, fx, fs * 0.06, 0, 0);
        lights[0].mEV = -1.5f + 2.f * surge;
        lights[1].mEV = -2.f + 1.5f *
            valueNoise(seed, fx, fs * 0.3, 1, 0);
        lights[3].mEV = -3.5f + surge;
        if (unitHash(seed, fx, counter, 0, 1) > 0.95f)
        {
            lights[1].mEV = 0.9f;
        }
        break;
    }
    case 60: // Vaporwave Sunset
    {
        const F64 u = positiveFmod(fs, 160.0) / 160.0;
        lights[0].mPitchDeg = 25.f - 30.f * static_cast<F32>(u);
        lights[0].mEV = 0.5f - 0.8f * static_cast<F32>(u);
        lights[0].mProfile = u < 0.33 ? 17 : u < 0.66 ? 15 : 18;
        lights[2].mEV = -1.5f + 0.4f * phaseSin(fs * 0.8);
        lights[3].mEV = -2.f + 0.6f * phaseSin(fs * 0.4);
        if (positiveFmod(fs, 16.0) < 0.5)
        {
            lights[0].mEV -= 1.f;
        }
        break;
    }
    case 61: // Carousel Waltz
    {
        const F64 a = positiveFmod(6.0 * fs, 360.0);
        lights[0].mYawDeg = wrap180(static_cast<F32>(a));
        lights[1].mYawDeg = wrap180(static_cast<F32>(a + 180.0));
        const F32 bob = phaseSin(fs * 0.9);
        lights[0].mPitchDeg = 18.f + 6.f * bob;
        lights[1].mPitchDeg = 18.f - 6.f * bob;
        const F32 accent = positiveMod(step, 15) < 5 ? 0.5f : 0.f;
        const F32 shimmer = 0.15f * phaseSin(fs * 3.1);
        lights[0].mEV = lights[1].mEV = -0.8f + accent + shimmer;
        break;
    }
    case 62: // Five Tones
    {
        const S64 b = positiveMod(step, 80);
        const S32 note = static_cast<S32>(positiveMod(step / 4, 5));
        static const S32 NOTES[5] = { 10, 13, 22, 7, 12 };
        const bool gate = positiveMod(step, 4) < 3;
        if (b < 20)
        {
            lights[0].mProfile = NOTES[note];
            lights[0].mEV = gate ? -0.5f : -10.f;
        }
        else if (b < 40)
        {
            lights[0].mProfile = lights[1].mProfile = NOTES[note];
            lights[0].mEV = lights[1].mEV = gate ? 1.f : -10.f;
        }
        else if (b < 48)
        {
            lights[3].mProfile = 20;
            lights[3].mEV = -3.f;
        }
        else if (b < 68)
        {
            for (S32 i = 0; i < LIGHT_COUNT; ++i)
            {
                lights[i].mProfile = NOTES[positiveMod(note + i, 5)];
                lights[i].mEV = gate ? 1.4f : -1.5f;
            }
        }
        else
        {
            for (LightBase& light : lights)
            {
                light.mProfile = 23;
                light.mEV = 2.4f - static_cast<F32>(b - 68) * 0.25f;
            }
        }
        break;
    }
    case FX_CANDY_ORBIT: // Candy Orbit
    {
        static const S32 ACTIVE[3] = { 0, 2, 3 };
        static const F32 BASE_YAW[3] = { 30.f, 150.f, -150.f };
        for (S32 n = 0; n < 3; ++n)
        {
            const F64 phase = TWO_PI * static_cast<F64>(n) / 3.0;
            const F32 crest = 0.5f + 0.5f *
                phaseCos(fs * 0.07 - phase);
            LightBase& light = lights[ACTIVE[n]];
            light.mEV = -2.6f + 2.4f * crest;
            light.mYawDeg = wrap180(BASE_YAW[n] + 8.f *
                phaseSin(fs * 0.025 + phase));
        }
        break;
    }
    case FX_RIMWAVE: // Rimwave
    {
        const F32 sweep = 0.5f + 0.5f * phaseSin(fs * 0.08);
        const F32 drift = 10.f * phaseSin(fs * 0.025);
        lights[0].mEV = -1.5f + 1.7f * sweep;
        lights[2].mEV = -1.5f + 1.7f * (1.f - sweep);
        lights[0].mYawDeg = wrap180(145.f + drift);
        lights[2].mYawDeg = wrap180(-145.f - drift);
        lights[3].mEV = -3.f + 0.25f * phaseSin(fs * 0.04);
        break;
    }
    case FX_AFTERHOURS_DRIFT: // Afterhours Drift
    {
        const F64 slow = fs * 0.025;
        lights[0].mYawDeg = wrap180(65.f + 22.f * phaseSin(slow));
        lights[0].mPitchDeg = 48.f + 7.f * phaseCos(slow * 0.7);
        lights[0].mEV = -0.8f + 0.35f * phaseSin(fs * 0.045);

        const F32 q = std::max(0.f, phaseSin(fs * 0.08));
        const F32 hump = q * q * q * q;
        lights[2].mEV = -2.8f + 1.6f * hump;
        lights[3].mEV = -3.5f + 0.25f * phaseSin(fs * 0.17) +
            0.15f * phaseSin(fs * 0.047);
        break;
    }
    case FX_SOMETHING_BEHIND_YOU: // Something Behind You
    {
        const F64 b = positiveFmod(fs, 180.0) / 180.0;
        lights[0].mYawDeg = wrap180(95.f - 190.f * static_cast<F32>(b));
        lights[0].mPitchDeg = 18.f + 6.f * phaseSin(TWO_PI * b);
        lights[0].mEV = b >= 0.82
            ? -10.f
            : -4.f + 2.4f * phaseSin(0.5 * TWO_PI * b);

        const F64 u = std::clamp((b - 0.62) / 0.20, 0.0, 1.0);
        F32 reveal = b >= 0.62 && b <= 0.82
            ? phaseSin(0.5 * TWO_PI * u)
            : 0.f;
        reveal *= reveal;
        lights[2].mEV = -10.f + 10.5f * reveal;
        lights[3].mEV = -4.f + 0.2f * phaseSin(TWO_PI * b);
        break;
    }
    default:
        break;
    }

    copyCleanLights(lights, out);
}

U64 flickerLightSeed(U64 rig_seed, S32 light_index)
{
    const U64 seed = rig_seed ? rig_seed : DEFAULT_SEED;
    const S32 safe_index = std::clamp(light_index, 0, LIGHT_COUNT - 1);
    return splitMix64(seed ^
        (0xd1b54a32d192ed03ULL * static_cast<U64>(safe_index + 1)));
}

void evalFlicker(S32 program, U64 light_seed, F64 t_seconds, F32 amount,
                 F32& out_intensity_mul, F32 out_color_mul[3])
{
    out_intensity_mul = 1.f;
    if (!out_color_mul)
    {
        return;
    }
    out_color_mul[0] = 1.f;
    out_color_mul[1] = 1.f;
    out_color_mul[2] = 1.f;

    const F32 safe_amount = std::clamp(finiteOr(amount, 0.f), 0.f, 1.f);
    if (program <= FLICKER_NONE || program >= FLICKER_COUNT ||
        safe_amount <= 0.f)
    {
        return;
    }

    const U64 seed = light_seed ? light_seed : DEFAULT_SEED;
    const F64 seconds = std::clamp(
        finiteOr(t_seconds, 0.0), 0.0, MAX_FX_SECONDS);
    const S32 hash_domain = FX_LEGACY_COUNT + program;
    const auto hash = [seed, hash_domain](U64 counter, S32 draw)
    {
        return unitHash(seed, hash_domain, counter, 0, draw);
    };
    const auto noise = [seed, hash_domain](F64 coordinate, S32 draw)
    {
        return valueNoise(seed, hash_domain, coordinate, 0, draw);
    };
    const auto centredNoise = [&noise](F64 coordinate, S32 draw)
    {
        return noise(coordinate, draw) * 2.f - 1.f;
    };

    F32 intensity = 1.f;
    F32 color[3] = { 1.f, 1.f, 1.f };
    switch (program)
    {
    case FLICKER_FIRELIGHT:
    {
        // A strong 8--14 Hz component rides above two lower octaves, producing
        // a cheap 1/f-like spectrum without maintaining noise state.
        const F64 frequency = 8.0 + 6.0 * hash(0, 0);
        const F32 fractal =
            0.55f * centredNoise(seconds * frequency, 0) +
            0.30f * centredNoise(seconds * frequency * 0.5, 1) +
            0.15f * centredNoise(seconds * frequency * 0.25, 2);
        intensity = 0.88f + 0.20f * fractal;
        const F64 pop_coordinate = seconds / 0.20;
        const U64 pop_step = static_cast<U64>(std::floor(pop_coordinate));
        if (hash(pop_step, 3) > 0.94f)
        {
            const F32 pop_phase = static_cast<F32>(
                pop_coordinate - std::floor(pop_coordinate));
            const F32 pop = phaseSin(pop_phase * TWO_PI * 0.5);
            intensity += 0.35f * pop * pop;
        }
        const F32 bright = std::clamp((intensity - 0.75f) / 0.68f, 0.f, 1.f);
        color[0] = 1.08f;
        color[1] = 1.02f - 0.04f * bright;
        color[2] = 0.88f - 0.14f * bright;
        break;
    }
    case FLICKER_TV_MONITOR:
    {
        // Each light gets a seeded one-to-four-second cut cadence and phase;
        // each new virtual scene draws a fresh discontinuous luminance.
        const F64 cut_interval = 1.0 + 3.0 * hash(0, 0);
        const F64 cut_phase = cut_interval * hash(0, 1);
        const U64 scene = static_cast<U64>(
            std::floor((seconds + cut_phase) / cut_interval));
        const F32 scene_luma = 0.64f + 0.70f * hash(scene, 2);
        const F32 shimmer = 0.035f * phaseSin(seconds * TWO_PI * 11.3) +
            0.035f * centredNoise(seconds * 18.0, 3);
        intensity = scene_luma + shimmer;
        const F32 hue = centredNoise(seconds * 0.16, 4);
        color[0] = 0.78f + 0.07f * hue;
        color[1] = 0.98f - 0.03f * hue;
        color[2] = 1.18f - 0.04f * hue;
        break;
    }
    case FLICKER_FLUORESCENT:
    {
        const F64 block_seconds = 7.0;
        const U64 block = static_cast<U64>(std::floor(seconds / block_seconds));
        const F64 local = positiveFmod(seconds, block_seconds);
        const F64 mains_hz = hash(0, 0) < 0.5f ? 100.0 : 120.0;
        intensity = 0.98f + 0.025f * phaseSin(
            seconds * TWO_PI * mains_hz + hash(0, 1) * TWO_PI);
        if (hash(block, 2) > 0.70f)
        {
            const F64 start = 0.6 + 5.2 * hash(block, 3);
            const F64 duration = 0.18 + 0.52 * hash(block, 4);
            if (local >= start && local < start + duration)
            {
                const S64 strobe = static_cast<S64>(
                    std::floor((local - start) * 34.0));
                intensity *= positiveMod(strobe, 4) == 0 ? 0.55f : 0.04f;
            }
        }
        color[0] = 0.92f;
        color[1] = 1.04f;
        color[2] = 0.94f;
        break;
    }
    case FLICKER_NEON:
    {
        // Presentation time zero is the deterministic tube warm-up origin.
        intensity = ease(static_cast<F32>(seconds / 0.8));
        const F64 shimmer_hz = 15.0 + 4.0 * hash(0, 4);
        intensity *= 0.98f + 0.025f * phaseSin(
            seconds * TWO_PI * shimmer_hz + hash(0, 5) * TWO_PI);
        const F64 block_seconds = 8.0;
        const U64 block = static_cast<U64>(std::floor(seconds / block_seconds));
        const F64 local = positiveFmod(seconds, block_seconds);
        if (hash(block, 0) > 0.72f)
        {
            const F64 start = 1.0 + 5.8 * hash(block, 1);
            const F64 duration = 0.35 + 0.25 * hash(block, 2);
            if (local >= start && local < start + duration)
            {
                const F32 chatter = phaseSin((local - start) * TWO_PI * 21.0);
                intensity *= chatter > 0.65f ? 0.30f : 0.045f;
            }
        }
        if (hash(0, 3) < 0.5f)
        {
            color[0] = 0.46f;
            color[1] = 1.08f;
            color[2] = 1.18f;
        }
        else
        {
            color[0] = 1.18f;
            color[1] = 0.42f;
            color[2] = 1.06f;
        }
        break;
    }
    case FLICKER_CANDLE:
    {
        const F32 gutter = 0.68f * centredNoise(seconds * 2.1, 0) +
            0.32f * centredNoise(seconds * 0.65, 1);
        intensity = 0.88f + 0.15f * gutter;
        const F64 block_seconds = 8.0;
        const U64 block = static_cast<U64>(std::floor(seconds / block_seconds));
        const F64 local = positiveFmod(seconds, block_seconds);
        if (hash(block, 2) > 0.82f)
        {
            const F64 start = 0.8 + 5.8 * hash(block, 3);
            const F64 duration = 0.45 + 0.65 * hash(block, 4);
            if (local >= start && local < start + duration)
            {
                const F32 phase = static_cast<F32>((local - start) / duration);
                const F32 dip = phaseSin(phase * TWO_PI * 0.5);
                intensity -= 0.55f * dip * dip;
            }
        }
        color[0] = 1.10f;
        color[1] = 0.96f;
        color[2] = 0.76f;
        break;
    }
    case FLICKER_PASSING_HEADLIGHTS:
    {
        const F64 period = 5.5 + 4.0 * hash(0, 0);
        const F64 offset = period * hash(0, 1);
        const F64 phase = positiveFmod(seconds + offset, period) / period;
        constexpr F64 SWEEP_FRACTION = 0.30;
        F32 sweep = 0.f;
        if (phase < SWEEP_FRACTION)
        {
            const F32 pass_phase = static_cast<F32>(phase / SWEEP_FRACTION);
            const F32 hump = phaseSin(pass_phase * TWO_PI * 0.5);
            sweep = hump * hump;
        }
        intensity = 0.04f + 1.36f * sweep;
        color[0] = 0.92f;
        color[1] = 1.02f;
        color[2] = 1.16f;
        break;
    }
    case FLICKER_POLICE_LIGHTBAR:
    {
        const F64 rate = 4.5 + 2.5 * hash(0, 0);
        const F64 flashes = seconds * rate + hash(0, 1);
        const S64 flash = static_cast<S64>(std::floor(flashes));
        const F32 phase = static_cast<F32>(flashes - std::floor(flashes));
        const F32 first = phase < 0.38f
            ? phaseSin((phase / 0.38f) * TWO_PI * 0.5) : 0.f;
        const F32 second = phase > 0.55f
            ? phaseSin(((phase - 0.55f) / 0.45f) * TWO_PI * 0.5) : 0.f;
        intensity = 0.18f + 1.20f * std::max(first * first, second * second);
        if ((flash & 1) == 0)
        {
            color[0] = 1.25f;
            color[1] = 0.08f;
            color[2] = 0.08f;
        }
        else
        {
            color[0] = 0.08f;
            color[1] = 0.18f;
            color[2] = 1.25f;
        }
        break;
    }
    default:
        return;
    }

    intensity = std::clamp(finiteOr(intensity, 1.f),
                           0.f, FLICKER_INTENSITY_MUL_MAX);
    out_intensity_mul = 1.f + (intensity - 1.f) * safe_amount;
    for (S32 channel = 0; channel < 3; ++channel)
    {
        color[channel] = std::clamp(finiteOr(color[channel], 1.f),
                                    0.f, FLICKER_COLOR_MUL_MAX);
        out_color_mul[channel] =
            1.f + (color[channel] - 1.f) * safe_amount;
    }
}

const char* flickerProgramName(S32 program)
{
    return program >= FLICKER_NONE && program < FLICKER_COUNT
        ? FLICKER_PROGRAM_NAMES[program] : FLICKER_PROGRAM_NAMES[FLICKER_NONE];
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

S32 easyConeWidthToBeam(S32 width)
{
    static constexpr S32 BEAM_BY_WIDTH[] = { 2, 0, 1 };
    return BEAM_BY_WIDTH[std::clamp(width, 0, 2)];
}

S32 easyConeWidthFromBeam(S32 beam)
{
    switch (std::clamp(beam, 0, BEAM_COUNT - 1))
    {
        case 2: return 0; // Snoot
        case 1: return 2; // Softbox
        default: return 1; // Standard
    }
}

const char* goboName(S32 index)
{
    return GOBO_NAMES[std::clamp(index, 0, GOBO_COUNT - 1)];
}

S32 goboCategory(S32 index)
{
    return GOBO_CATEGORIES[std::clamp(index, 0, GOBO_COUNT - 1)];
}

const char* goboCategoryName(S32 category)
{
    return GOBO_CATEGORY_NAMES[
        std::clamp(category, 0, GOBO_CATEGORY_COUNT - 1)];
}

S32 goboBlurBucket(F32 softness)
{
    const F32 safe = std::isfinite(softness)
        ? std::clamp(softness, 0.f, 8.f) : 0.f;
    return safe < 2.5f ? 0 : (safe < 5.5f ? 1 : 2);
}

bool goboIsAnimated(S32 index)
{
    return index == GOBO_ROTATING_FAN;
}

Setup classicSetup()
{
    Setup setup;
    std::memset(&setup, 0, sizeof(setup));
    setup.mRadius = 1.5f;
    // Easy-native: Key anchored at EV 0, ratio-locked at 2 stops (Fill derives
    // to -2, unchanged), Rim on the Subtle bucket (-1.0) so the compiled
    // Classic rig opens in Easy Mode rather than being forced to Advanced.
    setup.mRatioLock = true;
    setup.mRatioStops = 2.f;
    setLight(setup.mLights, 0, 45.f, 35.f, 3, 0.f, 1, true);
    setLight(setup.mLights, 1, -45.f, 5.f, 3, -2.f, 1, true);
    setLight(setup.mLights, 2, -135.f, 45.f, 4, -1.0f, 0, true);
    setLight(setup.mLights, 3, 0.f, -20.f, 3, 0.f, 0, false);
    return setup;
}
} // namespace ALCineLightRigModel
