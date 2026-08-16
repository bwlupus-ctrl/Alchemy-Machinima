/**
 * @file alcinelightrigmodel_test.cpp
 * @brief Adversarial tests for cinematic light rig math and FX replay.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../alcinelightrigmodel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace tut
{
using namespace ALCineLightRigModel;

namespace
{
void ensureSameLights(const char* message,
                      const LightBase expected[LIGHT_COUNT],
                      const LightBase actual[LIGHT_COUNT])
{
    ensure(message, std::memcmp(expected, actual,
                                sizeof(LightBase) * LIGHT_COUNT) == 0);
}

bool lightsDiffer(const LightBase first[LIGHT_COUNT],
                  const LightBase second[LIGHT_COUNT])
{
    return std::memcmp(first, second,
                       sizeof(LightBase) * LIGHT_COUNT) != 0;
}

bool allOff(const LightBase lights[LIGHT_COUNT])
{
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        if (lights[i].mOn)
        {
            return false;
        }
    }
    return true;
}

LightBase sampleFXBeat(S32 fx, S32 beat, S32 light)
{
    LightBase result[LIGHT_COUNT];
    const F64 interval = fxInterval(fx);
    evalFX(fx, 0x123456789abcdef0ULL,
           (beat + 0.01) * interval, result);
    return result[light];
}

bool closeRelative(F32 actual, F32 expected, F32 tolerance)
{
    return std::fabs(actual - expected) <= tolerance *
        std::max(1.f, std::fabs(expected));
}

F32 testSRGBToLinear(F32 value)
{
    return value < 0.04045f
        ? value / 12.92f
        : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

// Frozen pre-scale render path. This is intentionally independent of the new
// effective-radius branch so a one-ULP change at subject scale 1 fails loudly.
void renderScaleOneGolden(F32 radius, const LightBase live[LIGHT_COUNT],
                          const Globals& globals, RigFrame& out)
{
    constexpr F64 two_pi = 6.283185307179586476925286766559;
    constexpr F32 degrees_to_radians =
        static_cast<F32>(two_pi / 360.0);
    const Globals safe_globals = sanitizeGlobals(globals);
    const F32 distance_ev = std::log2(radius / 1.5f);
    std::memset(&out, 0, sizeof(out));
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        const LightBase& light = live[i];
        const F32 yaw = light.mYawDeg * degrees_to_radians;
        const F32 pitch = light.mPitchDeg * degrees_to_radians;
        const F32 cos_pitch = std::cos(pitch);
        const F32 off_x = radius * cos_pitch * std::cos(yaw);
        const F32 off_y = radius * cos_pitch * std::sin(yaw);
        const F32 off_z = radius * std::sin(pitch);
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

        EmitterState& projector = out.mProj[i];
        projector.mOffX = off_x;
        projector.mOffY = off_y;
        projector.mOffZ = off_z;
        projector.mAimX = -off_x / radius;
        projector.mAimY = -off_y / radius;
        projector.mAimZ = -off_z / radius;
        projector.mSR = rgb[0];
        projector.mSG = rgb[1];
        projector.mSB = rgb[2];
        projector.mIntensity = intensity;
        projector.mLightRadius = radius * 2.2f;
        projector.mFalloff = beamFalloff(light.mBeam);
        projector.mFovRad = beamFov(light.mBeam);
        projector.mOn = on;
        projector.mClipped = on && clipped;
        projector.mGobo = light.mGobo;

        const F32 omni_pitch_deg = std::max(
            light.mPitchDeg - 45.f, -PITCH_LIMIT_DEG);
        const F32 omni_pitch = omni_pitch_deg * degrees_to_radians;
        const F32 omni_cos = std::cos(omni_pitch);
        EmitterState& omni = out.mOmni[i];
        omni.mOffX = radius * omni_cos * std::cos(yaw);
        omni.mOffY = radius * omni_cos * std::sin(yaw);
        omni.mOffZ = radius * std::sin(omni_pitch);
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
        omni.mLightRadius = radius * 1.5f;
        omni.mFalloff = 0.75f;
        omni.mFovRad = 0.f;
        omni.mOn = omni_on;
        omni.mClipped = omni_on && raw_omni_intensity > 1.f;
    }
}
}

struct cine_light_rig_model_data {};
typedef test_group<cine_light_rig_model_data> cine_light_rig_model_group;
typedef cine_light_rig_model_group::object cine_light_rig_model_object;
cine_light_rig_model_group cine_light_rig_model_tests(
    "ALCineLightRigModel");

// Derived state is always recomputed from pristine base data.
template<> template<>
void cine_light_rig_model_object::test<1>()
{
    Setup setup = classicSetup();
    setup.mLights[0].mYawDeg = 180.f;
    setup.mLights[1].mYawDeg = -180.f;
    setup.mLights[2].mYawDeg = 179.75f;
    setup.mLights[3].mYawDeg = -179.75f;

    LightBase identity[LIGHT_COUNT];
    computeLive(setup, Transforms(), identity);

    Transforms mirror;
    mirror.mMirror = true;
    LightBase once[LIGHT_COUNT];
    computeLive(setup, mirror, once);
    Setup mirrored_setup = setup;
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        mirrored_setup.mLights[i] = once[i];
    }
    LightBase twice[LIGHT_COUNT];
    computeLive(mirrored_setup, mirror, twice);
    ensureSameLights("mirror twice is bitwise identity", identity, twice);

    Transforms orbit;
    orbit.mYawDeg = 15.f;
    LightBase orbited[LIGHT_COUNT];
    computeLive(setup, orbit, orbited);
    ensure("orbit actually changed the live pose",
           lightsDiffer(identity, orbited));
    LightBase reset[LIGHT_COUNT];
    computeLive(setup, Transforms(), reset);
    ensureSameLights("orbit then reset returns exact base", identity, reset);

    Setup second = classicSetup();
    second.mLights[0].mYawDeg = 12.f;
    second.mLights[1].mPitchDeg = -23.f;
    Transforms aim;
    aim.mMirror = true;
    aim.mYawDeg = 30.f;
    aim.mPitchDeg = 7.f;
    const Transforms saved_aim = aim;
    LightBase switched[LIGHT_COUNT];
    computeLive(second, aim, switched);
    ensure_equals("new setup receives preserved mirror/yaw",
                  switched[0].mYawDeg, -42.f);
    ensure_equals("new setup receives preserved pitch",
                  switched[1].mPitchDeg, -16.f);
    ensure("computeLive does not mutate transforms",
           aim.mMirror == saved_aim.mMirror &&
           aim.mYawDeg == saved_aim.mYawDeg &&
           aim.mPitchDeg == saved_aim.mPitchDeg);

    second.mLights[0].mPitchDeg = 80.f;
    second.mLights[1].mPitchDeg = -80.f;
    aim.mMirror = false;
    aim.mPitchDeg = 20.f;
    computeLive(second, aim, switched);
    ensure_equals("positive pitch clamps", switched[0].mPitchDeg, 85.f);
    aim.mPitchDeg = -20.f;
    computeLive(second, aim, switched);
    ensure_equals("negative pitch clamps", switched[1].mPitchDeg, -85.f);

    ensure_equals("positive full turn wraps", wrap180(360.f), 0.f);
    ensure_equals("negative full turn wraps", wrap180(-360.f), 0.f);
    ensure_equals("minus 180 maps into open lower bound",
                  wrap180(-180.f), 180.f);
    ensure_equals("boot rim yaw survives", wrap180(-135.f), -135.f);
}

// Cubic easing and transition blending preserve the LSL endpoint semantics.
template<> template<>
void cine_light_rig_model_object::test<2>()
{
    ensure_equals("ease zero", ease(0.f), 0.f);
    ensure_equals("ease half", ease(0.5f), 0.5f);
    ensure_equals("ease one", ease(1.f), 1.f);
    F32 previous = ease(0.f);
    for (S32 i = 1; i <= 100; ++i)
    {
        const F32 value = ease((F32)i / 100.f);
        ensure("ease monotone", value >= previous);
        previous = value;
    }

    LightBase start;
    std::memset(&start, 0, sizeof(start));
    start.mYawDeg = 170.f;
    start.mPitchDeg = -10.f;
    start.mProfile = 1;
    start.mEV = -2.f;
    start.mBeam = 0;
    start.mOn = true;
    LightBase target;
    std::memset(&target, 0, sizeof(target));
    target.mYawDeg = -170.f;
    target.mPitchDeg = 30.f;
    target.mProfile = 12;
    target.mEV = 2.f;
    target.mBeam = 2;
    target.mOn = false;

    const LightBase midpoint = blendLight(start, target, 0.5f);
    ensure_equals("yaw uses the 20 degree arc", midpoint.mYawDeg, 180.f);
    ensure_equals("discrete profile stays at exactly half",
                  midpoint.mProfile, start.mProfile);
    ensure_equals("discrete beam stays at exactly half",
                  midpoint.mBeam, start.mBeam);
    ensure("source-on remains on during blend", midpoint.mOn);

    start.mYawDeg = 170.f;
    target.mYawDeg = -160.f;
    const LightBase canonical = blendLight(start, target, 0.5f);
    ensure("mid-blend yaw remains canonical",
           canonical.mYawDeg > -180.f && canonical.mYawDeg <= 180.f);

    const LightBase before = blendLight(start, target, 0.49f);
    const LightBase after = blendLight(start, target, 0.51f);
    ensure_equals("profile before swap", before.mProfile, start.mProfile);
    ensure_equals("beam before swap", before.mBeam, start.mBeam);
    ensure_equals("profile after swap", after.mProfile, target.mProfile);
    ensure_equals("beam after swap", after.mBeam, target.mBeam);

    const LightBase endpoint = blendLight(start, target, 1.f);
    ensure("endpoint is exact target",
           std::memcmp(&endpoint, &target, sizeof(target)) == 0);
}

// Exposure is stop-linear, radius-compensated, clipped explicitly, and keeps
// the -10 EV semantic independent of the headroom re-base.
template<> template<>
void cine_light_rig_model_object::test<3>()
{
    bool clipped = false;
    const F32 low = intensityFromEV(-4.f, 2.f, &clipped);
    ensure("low exposure not clipped", !clipped);
    const F32 high = intensityFromEV(-3.f, 2.f, &clipped);
    ensure_equals("one stop doubles", high, low * 2.f);
    F32 previous = intensityFromEV(-12.f, 2.f, nullptr);
    for (S32 ev = -11; ev <= 2; ++ev)
    {
        const F32 current = intensityFromEV((F32)ev, 2.f, nullptr);
        ensure("exposure is monotone", current >= previous);
        previous = current;
    }
    intensityFromEV(8.f, 2.f, &clipped);
    ensure("ceiling reports clipping", clipped);

    Setup setup = classicSetup();
    setup.mLights[0].mEV = -4.f;
    LightBase live[LIGHT_COUNT];
    computeLive(setup, Transforms(), live);
    Globals globals;
    RigFrame near_frame;
    RigFrame far_frame;
    render(1.5f, live, globals, near_frame);
    render(3.f, live, globals, far_frame);
    ensure_equals("doubling radius adds one exposure stop",
                  far_frame.mProj[0].mIntensity,
                  near_frame.mProj[0].mIntensity * 2.f);

    setup.mLights[0].mEV = -10.f;
    computeLive(setup, Transforms(), live);
    for (F32 headroom : { 0.f, 2.f, 4.f })
    {
        globals.mHeadroomStops = headroom;
        RigFrame frame;
        render(1.5f, live, globals, frame);
        ensure("-10 EV remains logically off", !frame.mProj[0].mOn);
    }

    setup.mLights[0].mEV = 4.f;
    computeLive(setup, Transforms(), live);
    globals.mHeadroomStops = 2.f;
    globals.mBounceRatio = 0.45f;
    RigFrame clipped_frame;
    render(1.5f, live, globals, clipped_frame);
    ensure_equals("clipped projector reaches ceiling",
                  clipped_frame.mProj[0].mIntensity, 1.f);
    ensure_equals("bounce clamps after applying ratio",
                  clipped_frame.mOmni[0].mIntensity, 1.f);
    ensure("bounce owns its clipped flag", clipped_frame.mOmni[0].mClipped);

    setup.mLights[0].mEV = 0.f;
    computeLive(setup, Transforms(), live);
    render(1.5f, live, globals, clipped_frame);
    ensure_equals("bounce uses post-headroom unclamped intensity",
                  clipped_frame.mOmni[0].mIntensity, 0.1125f);

    setup.mLights[0].mEV = -8.5f;
    computeLive(setup, Transforms(), live);
    render(1.5f, live, globals, clipped_frame);
    ensure("bounce on gate is evaluated before headroom",
           clipped_frame.mOmni[0].mOn);

    setup.mLights[0].mEV = 3.f;
    computeLive(setup, Transforms(), live);
    render(1.5f, live, globals, clipped_frame);
    ensure("projector clips independently", clipped_frame.mProj[0].mClipped);
    ensure("sub-ceiling bounce does not inherit projector clipping",
           !clipped_frame.mOmni[0].mClipped);

    setup.mLights[0].mEV = -9.f;
    computeLive(setup, Transforms(), live);
    render(1.5f, live, globals, clipped_frame);
    ensure("projector threshold remains on", clipped_frame.mProj[0].mOn);
    ensure("bounce has its own threshold", !clipped_frame.mOmni[0].mOn);
    globals.mBounceRatio = 0.f;
    render(1.5f, live, globals, clipped_frame);
    ensure("zero-ratio bounce is not published", !clipped_frame.mOmni[0].mOn);
}

// Corrupt persisted data cannot pass NaN, infinity, indices, or a zero seed.
template<> template<>
void cine_light_rig_model_object::test<4>()
{
    Setup setup;
    setup.mRadius = -std::numeric_limits<F32>::infinity();
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        setup.mLights[i].mYawDeg = std::numeric_limits<F32>::quiet_NaN();
        setup.mLights[i].mPitchDeg = std::numeric_limits<F32>::infinity();
        setup.mLights[i].mProfile = 1000;
        setup.mLights[i].mEV = -std::numeric_limits<F32>::infinity();
        setup.mLights[i].mBeam = -1000;
    }
    const Setup safe = sanitizeSetup(setup);
    ensure("radius finite and clamped",
           std::isfinite(safe.mRadius) && safe.mRadius >= MIN_RADIUS);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        ensure("light numerics finite",
               std::isfinite(safe.mLights[i].mYawDeg) &&
               std::isfinite(safe.mLights[i].mPitchDeg) &&
               std::isfinite(safe.mLights[i].mEV));
        ensure("profile index clamped",
               safe.mLights[i].mProfile >= 0 &&
               safe.mLights[i].mProfile < PROFILE_COUNT);
        ensure("beam index clamped",
               safe.mLights[i].mBeam >= 0 &&
               safe.mLights[i].mBeam < BEAM_COUNT);
    }

    Transforms transforms;
    transforms.mYawDeg = std::numeric_limits<F32>::infinity();
    transforms.mPitchDeg = std::numeric_limits<F32>::quiet_NaN();
    const Transforms safe_transforms = sanitizeTransforms(transforms);
    ensure("transform numerics finite",
           std::isfinite(safe_transforms.mYawDeg) &&
           std::isfinite(safe_transforms.mPitchDeg));

    Globals globals;
    globals.mMasterEV = std::numeric_limits<F32>::quiet_NaN();
    globals.mHeadroomStops = std::numeric_limits<F32>::infinity();
    globals.mBounceRatio = -std::numeric_limits<F32>::infinity();
    globals.mTransitionSec = std::numeric_limits<F32>::quiet_NaN();
    globals.mSeed = 0;
    const Globals safe_globals = sanitizeGlobals(globals);
    ensure("globals finite",
           std::isfinite(safe_globals.mMasterEV) &&
           std::isfinite(safe_globals.mHeadroomStops) &&
           std::isfinite(safe_globals.mBounceRatio) &&
           std::isfinite(safe_globals.mTransitionSec));
    ensure("zero seed replaced", safe_globals.mSeed != 0);
    ensure("beam fov model-enforced",
           beamFov(-100) >= 0.05f && beamFov(100) <= 2.9f);
}

// Every effect is a byte-stable free function, including boundaries and a
// very large presentation timestamp; evaluation order cannot affect replay.
template<> template<>
void cine_light_rig_model_object::test<5>()
{
    const U64 seed = 0xfeedface12345678ULL;
    for (S32 fx = 0; fx < FX_COUNT; ++fx)
    {
        const F64 interval = fxInterval(fx);
        const F64 boundary = 17.0 * interval;
        const std::vector<F64> times = {
            0.0,
            interval * 0.125,
            std::nextafter(boundary, 0.0),
            boundary,
            std::nextafter(boundary, std::numeric_limits<F64>::infinity()),
            7.25,
            1.0e7,
        };
        for (F64 time : times)
        {
            LightBase first[LIGHT_COUNT];
            LightBase second[LIGHT_COUNT];
            evalFX(fx, seed, time, first);
            evalFX(fx, seed, time, second);
            ensureSameLights("bitwise FX replay", first, second);
        }

        std::vector<LightBase> ordered(times.size() * LIGHT_COUNT);
        std::vector<LightBase> scrubbed(times.size() * LIGHT_COUNT);
        for (size_t i = 0; i < times.size(); ++i)
        {
            evalFX(fx, seed, times[i], &ordered[i * LIGHT_COUNT]);
        }
        for (size_t reverse = times.size(); reverse-- > 0; )
        {
            evalFX(fx, seed, times[reverse],
                   &scrubbed[reverse * LIGHT_COUNT]);
        }
        ensure("scrub order independent",
               std::memcmp(ordered.data(), scrubbed.data(),
                           ordered.size() * sizeof(LightBase)) == 0);
    }
}

// Hash-using effects separate seeds; analytic effects ignore them.
template<> template<>
void cine_light_rig_model_object::test<6>()
{
    const S32 random_fx[] = {
        1, 2, 5, 6, 9, 11, 13, 14, 17, 20, 25, 27, 30, 31
    };
    for (S32 fx : random_fx)
    {
        bool separated = false;
        for (S32 sample = 0; sample < 256 && !separated; ++sample)
        {
            LightBase first[LIGHT_COUNT];
            LightBase second[LIGHT_COUNT];
            const F64 time = (sample + 0.25) * fxInterval(fx);
            evalFX(fx, 111, time, first);
            evalFX(fx, 222, time, second);
            separated = lightsDiffer(first, second);
        }
        ensure("random effect separates seeds", separated);
    }

    bool is_random[FX_COUNT] = {};
    for (S32 fx : random_fx)
    {
        is_random[fx] = true;
    }
    for (S32 fx = 0; fx < FX_COUNT; ++fx)
    {
        if (is_random[fx])
        {
            continue;
        }
        LightBase first[LIGHT_COUNT];
        LightBase second[LIGHT_COUNT];
        evalFX(fx, 111, 13.375, first);
        evalFX(fx, 222, 13.375, second);
        ensureSameLights("analytic effect is seed invariant", first, second);
    }
}

// Discrete effects are half-open step functions.
template<> template<>
void cine_light_rig_model_object::test<7>()
{
    LightBase start[LIGHT_COUNT];
    LightBase inside[LIGHT_COUNT];
    LightBase boundary[LIGHT_COUNT];
    const F64 interval = fxInterval(0);
    evalFX(0, 7, 4.01 * interval, start);
    evalFX(0, 7, 4.99 * interval, inside);
    evalFX(0, 7, 5.01 * interval, boundary);
    ensureSameLights("constant inside discrete step", start, inside);
    ensure("changes at next discrete step", lightsDiffer(start, boundary));
}

// Closed-form latch rewrites pin the phase edges that otherwise silently keep
// state from a prior evaluation.
template<> template<>
void cine_light_rig_model_object::test<8>()
{
    LightBase lights[LIGHT_COUNT];
    evalFX(29, 1, 0.01 * fxInterval(29), lights);
    ensure("Stage beat 0 is dark", allOff(lights));
    ensure("Stage beat 6 has background only",
           sampleFXBeat(29, 6, 3).mOn &&
           !sampleFXBeat(29, 6, 0).mOn);
    ensure("Stage beat 13 keeps background and adds key",
           sampleFXBeat(29, 13, 3).mOn &&
           sampleFXBeat(29, 13, 0).mOn);
    ensure("Stage beat 18 has fill with fixed Snoot",
           sampleFXBeat(29, 18, 1).mOn &&
           sampleFXBeat(29, 18, 1).mBeam == 2);
    ensure("Stage beat 23 has all four roles",
           sampleFXBeat(29, 23, 0).mOn &&
           sampleFXBeat(29, 23, 1).mOn &&
           sampleFXBeat(29, 23, 2).mOn &&
           sampleFXBeat(29, 23, 3).mOn);
    ensure_equals("Stage fill latch is bit-identical to ramp endpoint",
                  sampleFXBeat(29, 21, 1).mEV,
                  sampleFXBeat(29, 20, 1).mEV);
    ensure("Stage beat 40 retains latched poses",
           sampleFXBeat(29, 40, 0).mOn &&
           sampleFXBeat(29, 40, 2).mOn);
    evalFX(29, 1, 75.01 * fxInterval(29), lights);
    ensure("Stage beat 75 is dark", allOff(lights));

    evalFX(25, 1, 1.01 * fxInterval(25), lights);
    for (const LightBase& light : lights)
    {
        ensure_equals("Elevator's invalid LSL beam maps to Snoot",
                      light.mBeam, 2);
    }

    ensure_equals("Explosion beat 0 flash", sampleFXBeat(27, 0, 0).mEV, 2.f);
    ensure_equals("Explosion beat 3 enters amber decay",
                  sampleFXBeat(27, 3, 0).mProfile, 13);
    ensure("Explosion beat 12 drops rim/background",
           !sampleFXBeat(27, 12, 2).mOn &&
           !sampleFXBeat(27, 12, 3).mOn);
    const LightBase explosion_hidden = sampleFXBeat(27, 12, 2);
    const LightBase explosion_afterglow = sampleFXBeat(27, 45, 2);
    ensure("Explosion preserves hidden latched state",
           explosion_hidden.mProfile == explosion_afterglow.mProfile &&
           explosion_hidden.mEV == explosion_afterglow.mEV);
    ensure("Explosion beat 45 leaves key only",
           sampleFXBeat(27, 45, 0).mOn &&
           !sampleFXBeat(27, 45, 1).mOn);
    const F32 explosion_ember = sampleFXBeat(27, 44, 1).mEV;
    ensure_equals("Explosion beat 45 replays beat-44 ember",
                  sampleFXBeat(27, 45, 1).mEV, explosion_ember);
    ensure_equals("Explosion afterglow does not re-roll ember",
                  sampleFXBeat(27, 63, 1).mEV, explosion_ember);
    const F32 second_cycle_ember = sampleFXBeat(27, 65 + 44, 1).mEV;
    ensure_equals("Explosion cycle 1 replays its beat-44 ember",
                  sampleFXBeat(27, 65 + 45, 1).mEV,
                  second_cycle_ember);
    ensure_equals("Explosion cycle 1 keeps its ember through afterglow",
                  sampleFXBeat(27, 65 + 63, 1).mEV,
                  second_cycle_ember);

    ensure_equals("Supernova beat 0 begins at -2 EV",
                  sampleFXBeat(28, 0, 0).mEV, -2.f);
    ensure_equals("Supernova beat 50 flashes",
                  sampleFXBeat(28, 50, 0).mEV, 2.f);
    ensure_equals("Supernova beat 53 begins collapse",
                  sampleFXBeat(28, 53, 0).mEV, 2.f);
    ensure("Supernova beat 65 is two-light remnant",
           sampleFXBeat(28, 65, 0).mOn &&
           !sampleFXBeat(28, 65, 2).mOn);
    ensure_equals("Supernova dark tail preserves hidden collapse state",
                  sampleFXBeat(28, 75, 2).mEV,
                  sampleFXBeat(28, 65, 2).mEV);
    ensure_equals("Supernova beat 75 is threshold-dark",
                  sampleFXBeat(28, 75, 0).mEV, -10.f);
}

// A broad deterministic corpus catches numerical regressions. Golden poses and
// positional table rows below ensure output sanitization cannot hide bad FX or
// shifted table data.
template<> template<>
void cine_light_rig_model_object::test<9>()
{
    U64 state = 0x9e3779b97f4a7c15ULL;
    for (S32 sample = 0; sample < 10000; ++sample)
    {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        const S32 fx = sample % FX_COUNT;
        const F64 time = static_cast<F64>(state % 1000000000ULL) / 100.0;
        LightBase lights[LIGHT_COUNT];
        evalFX(fx, state, time, lights);
        for (const LightBase& light : lights)
        {
            ensure("FX output finite",
                   std::isfinite(light.mYawDeg) &&
                   std::isfinite(light.mPitchDeg) &&
                   std::isfinite(light.mEV));
            ensure("FX pitch bounded",
                   std::fabs(light.mPitchDeg) <= PITCH_LIMIT_DEG);
            ensure("FX profile in range",
                   light.mProfile >= 0 && light.mProfile < PROFILE_COUNT);
            ensure("FX beam in range",
                   light.mBeam >= 0 && light.mBeam < BEAM_COUNT);
        }
    }

    ensure_equals("profile row count", PROFILE_COUNT, 24);
    ensure_equals("beam row count", BEAM_COUNT, 3);
    ensure_equals("FX name row count", FX_COUNT, 33);
    ensure_equals("FX interval row count", FX_COUNT, 33);

    for (S32 i = 0; i < PROFILE_COUNT; ++i)
    {
        const char* name = profileName(i);
        ensure("profile row pointer valid", name != nullptr);
        ensure("profile row named", name && name[0] != '\0');
    }
    for (S32 i = 0; i < BEAM_COUNT; ++i)
    {
        const char* name = beamName(i);
        ensure("beam row pointer valid", name != nullptr);
        ensure("beam row named", name && name[0] != '\0');
    }
    for (S32 i = 0; i < FX_COUNT; ++i)
    {
        const char* name = fxName(i);
        ensure("FX row pointer valid", name != nullptr);
        ensure("FX row named", name && name[0] != '\0');
    }

    const char* const profile_names[PROFILE_COUNT] = {
        "2700K Incan", "3200K Tung", "4500K Neut", "5600K Day",
        "6500K Cool", "8000K Moon", "10000K Sky", "Full CTO",
        "Full CTB", "Plus Green", "Magenta", "Rosco Red",
        "Congo Blue", "Deep Amber", "Emerald", "Cyber Pink",
        "Sci-Fi Cyan", "Golden Hour", "Vaporwave", "Blood Red",
        "Deep Space", "Toxic Waste", "Neon Purple", "Pure White",
    };
    const F32 profile_rgb[PROFILE_COUNT][3] = {
        { 1.00f, 0.55f, 0.15f }, { 1.00f, 0.92f, 0.72f },
        { 1.00f, 1.00f, 0.95f }, { 0.85f, 0.92f, 1.00f },
        { 0.70f, 0.85f, 1.00f }, { 0.45f, 0.55f, 1.00f },
        { 0.30f, 0.40f, 1.00f }, { 1.00f, 0.65f, 0.05f },
        { 0.30f, 0.60f, 1.00f }, { 0.60f, 1.00f, 0.60f },
        { 1.00f, 0.20f, 0.80f }, { 1.00f, 0.02f, 0.02f },
        { 0.05f, 0.05f, 1.00f }, { 1.00f, 0.40f, 0.00f },
        { 0.00f, 1.00f, 0.10f }, { 1.00f, 0.05f, 0.80f },
        { 0.05f, 0.85f, 1.00f }, { 1.00f, 0.40f, 0.05f },
        { 0.80f, 0.00f, 1.00f }, { 0.60f, 0.00f, 0.00f },
        { 0.00f, 0.00f, 0.20f }, { 0.20f, 1.00f, 0.00f },
        { 0.50f, 0.00f, 1.00f }, { 1.00f, 1.00f, 1.00f },
    };
    for (S32 i = 0; i < PROFILE_COUNT; ++i)
    {
        F32 rgb[3];
        profileSRGB(i, rgb);
        ensure_equals("profile name golden", std::string(profileName(i)),
                      std::string(profile_names[i]));
        ensure_equals("profile red golden", rgb[0], profile_rgb[i][0]);
        ensure_equals("profile green golden", rgb[1], profile_rgb[i][1]);
        ensure_equals("profile blue golden", rgb[2], profile_rgb[i][2]);
    }

    const char* const beam_names[BEAM_COUNT] = {
        "Standard", "Softbox", "Snoot"
    };
    const F32 beam_fovs[BEAM_COUNT] = { 1.5f, 2.8f, 0.2f };
    const F32 beam_falloffs[BEAM_COUNT] = { 1.f, 1.5f, 0.5f };
    for (S32 i = 0; i < BEAM_COUNT; ++i)
    {
        ensure_equals("beam name golden", std::string(beamName(i)),
                      std::string(beam_names[i]));
        ensure_equals("beam fov golden", beamFov(i), beam_fovs[i]);
        ensure_equals("beam falloff golden", beamFalloff(i),
                      beam_falloffs[i]);
    }
    const char* const fx_names[FX_COUNT] = {
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
    for (S32 i = 0; i < FX_COUNT; ++i)
    {
        ensure_equals("FX name golden", std::string(fxName(i)),
                      std::string(fx_names[i]));
    }
    const F32 fx_intervals[FX_COUNT] = {
        0.15f, 0.10f, 0.20f, 0.20f, 0.10f, 0.10f, 0.20f, 0.20f,
        0.10f, 0.10f, 0.20f, 0.10f, 0.10f, 0.15f, 0.05f, 0.10f,
        0.10f, 0.10f, 0.05f, 0.20f, 0.05f, 0.10f, 0.20f, 0.10f,
        0.05f, 0.10f, 0.07f, 0.08f, 0.08f, 0.15f, 0.07f, 0.12f,
        0.12f,
    };
    for (S32 i = 0; i < FX_COUNT; ++i)
    {
        ensure_equals("FX interval golden", fxInterval(i), fx_intervals[i]);
    }

    LightBase golden[LIGHT_COUNT];
    evalFX(0, 7, 0.0, golden);
    ensure_equals("Police even key EV", golden[0].mEV, 1.f);
    ensure_equals("Police even fill EV", golden[1].mEV, -10.f);
    evalFX(0, 7, fxInterval(0), golden);
    ensure_equals("Police odd key EV", golden[0].mEV, -10.f);
    ensure_equals("Police odd fill EV", golden[1].mEV, 1.f);
    evalFX(10, 7, 0.0, golden);
    const F32 gamer_yaws[LIGHT_COUNT] = { 45.f, -45.f, 135.f, -135.f };
    const S32 gamer_profiles[LIGHT_COUNT] = { 7, 11, 15, 19 };
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        ensure_equals("RGB Gamer authored yaw", golden[i].mYawDeg,
                      gamer_yaws[i]);
        ensure_equals("RGB Gamer authored profile", golden[i].mProfile,
                      gamer_profiles[i]);
    }

    for (S32 i = 0; i < PROFILE_COUNT; ++i)
    {
        F32 rgb[3];
        profileSRGB(i, rgb);
        ensure("profile row RGB bounded",
               rgb[0] >= 0.f && rgb[0] <= 1.f &&
               rgb[1] >= 0.f && rgb[1] <= 1.f &&
               rgb[2] >= 0.f && rgb[2] <= 1.f);
    }
    for (S32 i = 0; i < BEAM_COUNT; ++i)
    {
        ensure("beam fov bounded", beamFov(i) >= 0.05f && beamFov(i) <= 2.9f);
        ensure("beam falloff bounded",
               beamFalloff(i) >= 0.f && beamFalloff(i) <= 2.f);
    }
    for (S32 i = 0; i < FX_COUNT; ++i)
    {
        ensure("FX interval positive", fxInterval(i) > 0.f);
    }
}

// Subject scale changes spatial geometry only. The unchanged tests above run
// through the default scale-1 path; this cluster pins the new scale boundaries,
// proportional regime, exposure invariance, and degenerate-input fallback.
template<> template<>
void cine_light_rig_model_object::test<10>()
{
    Setup setup = classicSetup();
    LightBase live[LIGHT_COUNT];
    computeLive(setup, Transforms(), live);

    // 1. The explicit scale-1 path is byte-identical to the default path for
    // every beam and representative nominal radius, including out-of-band ones.
    const F32 nominal_radii[] = { 0.5f, 1.5f, 9.1f, 12.f, 512.f };
    for (S32 beam = 0; beam < BEAM_COUNT; ++beam)
    {
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            live[i].mBeam = beam;
        }
        for (F32 radius : nominal_radii)
        {
            Globals default_globals;
            Globals explicit_globals;
            explicit_globals.mSubjectScale = 1.f;
            RigFrame golden_frame;
            RigFrame default_frame;
            RigFrame explicit_frame;
            renderScaleOneGolden(
                radius, live, default_globals, golden_frame);
            render(radius, live, default_globals, default_frame);
            render(radius, live, explicit_globals, explicit_frame);
            ensure("default scale preserves the pre-change golden",
                   std::memcmp(&golden_frame, &default_frame,
                               sizeof(RigFrame)) == 0);
            ensure("explicit scale 1 is a bitwise no-op",
                   std::memcmp(&default_frame, &explicit_frame,
                               sizeof(RigFrame)) == 0);
        }
    }

    const F32 scale_test_yaws[LIGHT_COUNT] = {
        40.f, -35.f, 120.f, -140.f
    };
    const F32 scale_test_pitches[LIGHT_COUNT] = {
        30.f, 15.f, -25.f, 10.f
    };
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        setup.mLights[i].mYawDeg = scale_test_yaws[i];
        setup.mLights[i].mPitchDeg = scale_test_pitches[i];
        setup.mLights[i].mOn = true;
    }
    computeLive(setup, Transforms(), live);

    // 2. Powers-of-two scales preserve exact proportionality in-band.
    Globals globals;
    RigFrame nominal;
    render(1.5f, live, globals, nominal);
    for (F32 scale : { 0.5f, 2.f, 4.f })
    {
        globals.mSubjectScale = scale;
        RigFrame scaled;
        render(1.5f, live, globals, scaled);
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            ensure_equals("projector X scales proportionally",
                          scaled.mProj[i].mOffX,
                          nominal.mProj[i].mOffX * scale);
            ensure_equals("projector Y scales proportionally",
                          scaled.mProj[i].mOffY,
                          nominal.mProj[i].mOffY * scale);
            ensure_equals("projector Z scales proportionally",
                          scaled.mProj[i].mOffZ,
                          nominal.mProj[i].mOffZ * scale);
            ensure_equals("projector falloff radius scales",
                          scaled.mProj[i].mLightRadius,
                          nominal.mProj[i].mLightRadius * scale);
            ensure_equals("omni X scales proportionally",
                          scaled.mOmni[i].mOffX,
                          nominal.mOmni[i].mOffX * scale);
            ensure_equals("omni Y scales proportionally",
                          scaled.mOmni[i].mOffY,
                          nominal.mOmni[i].mOffY * scale);
            ensure_equals("omni Z scales proportionally",
                          scaled.mOmni[i].mOffZ,
                          nominal.mOmni[i].mOffZ * scale);
            ensure_equals("omni falloff radius scales",
                          scaled.mOmni[i].mLightRadius,
                          nominal.mOmni[i].mLightRadius * scale);
        }
    }

    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        setup.mLights[i].mYawDeg = 0.f;
        setup.mLights[i].mPitchDeg = 0.f;
    }
    computeLive(setup, Transforms(), live);

    // 3. Scale never enters the nominal-radius EV term, including at both
    // geometry clamps and around the intensity clipping boundary.
    const F32 exposure_scales[] = { 0.05f, 0.5f, 1.f, 2.f, 6.f, 150.f };
    for (F32 ev : { -9.f, -4.f, 0.f, 2.f, 4.f })
    {
        setup.mLights[0].mEV = ev;
        computeLive(setup, Transforms(), live);
        Globals reference_globals;
        reference_globals.mSubjectScale = 1.f;
        RigFrame reference;
        render(1.5f, live, reference_globals, reference);
        for (F32 scale : exposure_scales)
        {
            Globals scaled_globals = reference_globals;
            scaled_globals.mSubjectScale = scale;
            RigFrame scaled;
            render(1.5f, live, scaled_globals, scaled);
            ensure_equals("projector intensity is scale invariant",
                          scaled.mProj[0].mIntensity,
                          reference.mProj[0].mIntensity);
            ensure_equals("projector clipping is scale invariant",
                          scaled.mProj[0].mClipped,
                          reference.mProj[0].mClipped);
            ensure_equals("omni intensity is scale invariant",
                          scaled.mOmni[0].mIntensity,
                          reference.mOmni[0].mIntensity);
            ensure_equals("omni clipping is scale invariant",
                          scaled.mOmni[0].mClipped,
                          reference.mOmni[0].mClipped);
        }
    }

    setup.mLights[0].mEV = 0.f;
    computeLive(setup, Transforms(), live);
    globals = Globals();
    RigFrame scale_one;
    render(1.5f, live, globals, scale_one);

    // 4. Co-scaled orbit and falloff retain their attenuation ratio even when
    // the giant-subject ceiling is active.
    globals.mSubjectScale = 150.f;
    RigFrame giant;
    render(1.5f, live, globals, giant);
    ensure("giant orbit reaches the independent ceiling lower bound",
           giant.mProj[0].mOffX >= 9.f);
    ensure("giant orbit stays within the independent ceiling upper bound",
           giant.mProj[0].mOffX <= 9.1f);
    ensure("projector reach is maximal near the 20 metre clamp",
           giant.mProj[0].mLightRadius >= 19.9f);
    ensure("projector reach stays within the 20 metre clamp",
           giant.mProj[0].mLightRadius <= 20.f);
    const F32 ratio_left = giant.mProj[0].mOffX *
                           scale_one.mProj[0].mLightRadius;
    const F32 ratio_right = scale_one.mProj[0].mOffX *
                            giant.mProj[0].mLightRadius;
    ensure("projector attenuation ratio is invariant",
           std::fabs(ratio_left - ratio_right) <=
               std::numeric_limits<F32>::epsilon() *
               std::max(std::fabs(ratio_left), std::fabs(ratio_right)) * 2.f);
    const F32 scale_one_omni_offset = std::sqrt(
        scale_one.mOmni[0].mOffX * scale_one.mOmni[0].mOffX +
        scale_one.mOmni[0].mOffY * scale_one.mOmni[0].mOffY +
        scale_one.mOmni[0].mOffZ * scale_one.mOmni[0].mOffZ);
    const F32 giant_omni_offset = std::sqrt(
        giant.mOmni[0].mOffX * giant.mOmni[0].mOffX +
        giant.mOmni[0].mOffY * giant.mOmni[0].mOffY +
        giant.mOmni[0].mOffZ * giant.mOmni[0].mOffZ);
    const F32 omni_ratio_left = giant_omni_offset *
                                scale_one.mOmni[0].mLightRadius;
    const F32 omni_ratio_right = scale_one_omni_offset *
                                 giant.mOmni[0].mLightRadius;
    ensure("omni attenuation ratio is invariant",
           std::fabs(omni_ratio_left - omni_ratio_right) <=
               std::numeric_limits<F32>::epsilon() *
               std::max(std::fabs(omni_ratio_left),
                        std::fabs(omni_ratio_right)) * 4.f);

    // 5-6. The derived ceiling and floor are exact and keep projector reach
    // within the underlying 20 m viewer clamp.
    ensure_equals("scaled radius ceiling exact",
                  giant.mProj[0].mOffX, SCALED_RADIUS_CEIL);
    ensure("projector reach stays within 20 metres",
           giant.mProj[0].mLightRadius <= 20.f);
    globals.mSubjectScale = 0.05f;
    RigFrame doll;
    render(1.5f, live, globals, doll);
    ensure_equals("scaled radius floor exact",
                  doll.mProj[0].mOffX, SCALED_RADIUS_FLOOR);

    // 7. The min/max-bounded clamp never shrinks a large nominal at scale-up,
    // while retaining proportional scale-down and the small-nominal floor.
    globals.mSubjectScale = 1.f;
    RigFrame large_one;
    render(12.f, live, globals, large_one);
    ensure_equals("large nominal survives scale one",
                  large_one.mProj[0].mOffX, 12.f);
    globals.mSubjectScale = 2.f;
    RigFrame large_up;
    render(12.f, live, globals, large_up);
    ensure_equals("large nominal is not shrunk on scale-up",
                  large_up.mProj[0].mOffX, 12.f);
    globals.mSubjectScale = 0.5f;
    RigFrame large_down;
    render(12.f, live, globals, large_down);
    ensure_equals("large nominal scales down proportionally",
                  large_down.mProj[0].mOffX, 6.f);
    globals.mSubjectScale = 0.05f;
    RigFrame small_floor;
    render(0.5f, live, globals, small_floor);
    ensure_equals("minimum nominal observes scaled floor",
                  small_floor.mProj[0].mOffX, SCALED_RADIUS_FLOOR);

    // 8. Invalid, infinite, and non-normal scales collapse to the exact
    // scale-1 output before clamping.
    globals.mSubjectScale = 1.f;
    RigFrame valid_one;
    render(1.5f, live, globals, valid_one);
    const F32 invalid_scales[] = {
        0.f,
        -1.f,
        std::numeric_limits<F32>::quiet_NaN(),
        std::numeric_limits<F32>::infinity(),
        std::numeric_limits<F32>::min(),
    };
    for (F32 scale : invalid_scales)
    {
        globals.mSubjectScale = scale;
        RigFrame invalid;
        render(1.5f, live, globals, invalid);
        ensure("degenerate scale is bitwise scale one",
               std::memcmp(&valid_one, &invalid, sizeof(RigFrame)) == 0);
    }

    // 9. Effective radius is monotone over the supported subject-scale range.
    const F32 scale_grid[] = {
        0.05f, 0.075f, 0.1f, 0.15f, 0.25f, 0.5f, 0.75f, 1.f,
        1.5f, 2.f, 3.f, 4.f, 6.f, 8.f, 12.f, 20.f, 40.f, 80.f, 150.f,
    };
    for (F32 radius : { 0.5f, 1.5f, 12.f })
    {
        F32 previous = -1.f;
        for (F32 scale : scale_grid)
        {
            globals.mSubjectScale = scale;
            RigFrame frame;
            render(radius, live, globals, frame);
            ensure("effective radius is monotone",
                   frame.mProj[0].mOffX >= previous);
            previous = frame.mProj[0].mOffX;
        }
    }

    // 10. Sanitization pins the public scale contract and its default.
    Globals defaults;
    ensure_equals("subject scale defaults to one",
                  defaults.mSubjectScale, 1.f);
    Globals low_scale;
    low_scale.mSubjectScale = 0.01f;
    // Literals intentionally pin the GHOST_SCALE_MIN/MAX mirror
    // (alghoststudio.h) -- do not replace with the model symbol.
    ensure_equals("subject scale lower clamp",
                  sanitizeGlobals(low_scale).mSubjectScale,
                  0.05f);
    Globals high_scale;
    high_scale.mSubjectScale = 151.f;
    ensure_equals("subject scale upper clamp",
                  sanitizeGlobals(high_scale).mSubjectScale,
                  150.f);

    // 11. The model-exported emitter-box clamp used by the controller is exact
    // at scale one and bounded at both optics-safety limits.
    ensure_equals("emitter box scale-one edge",
                  emitterBoxEdgeFromRatio(1.f), 0.25f);
    ensure_equals("emitter box lower clamp",
                  emitterBoxEdgeFromRatio(0.001f), 0.01f);
    ensure_equals("emitter box upper clamp",
                  emitterBoxEdgeFromRatio(100.f), 1.f);
}

// Mirror reflects world-authored yaw across the subject's live facing axis.
template<> template<>
void cine_light_rig_model_object::test<11>()
{
    Setup setup = classicSetup();
    const F32 spread[LIGHT_COUNT] = { 45.f, -45.f, 135.f, -135.f };
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        setup.mLights[i].mYawDeg = spread[i];
    }

    Transforms mirror;
    mirror.mMirror = true;
    LightBase reflected[LIGHT_COUNT];
    computeLive(setup, mirror, reflected);
    LightBase old_reflection[LIGHT_COUNT];
    computeLive(setup, Transforms(), old_reflection);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        old_reflection[i].mYawDeg = wrap180(-spread[i]);
        ensure_equals("facing zero preserves the original reflection",
                      reflected[i].mYawDeg, wrap180(-spread[i]));
    }
    ensureSameLights("facing zero is bitwise identical to the old mirror",
                     old_reflection, reflected);

    mirror.mFacingAzimuthDeg = 90.f;
    computeLive(setup, mirror, reflected);
    ensure_equals("45 reflects to 135 across facing 90",
                  reflected[0].mYawDeg, 135.f);
    ensure_equals("135 reflects to 45 across facing 90",
                  reflected[2].mYawDeg, 45.f);
    ensure_equals("-45 reflects to -135 across facing 90",
                  reflected[1].mYawDeg, -135.f);

    mirror.mFacingAzimuthDeg = 37.f;
    computeLive(setup, mirror, reflected);
    ensure_equals("nontrivial facing changes the first reflection",
                  reflected[0].mYawDeg, 29.f);
    Setup reflected_setup = setup;
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        reflected_setup.mLights[i] = reflected[i];
    }
    LightBase reflected_twice[LIGHT_COUNT];
    computeLive(reflected_setup, mirror, reflected_twice);
    LightBase identity[LIGHT_COUNT];
    computeLive(setup, Transforms(), identity);
    ensureSameLights("mirror twice is identity at facing 37",
                     identity, reflected_twice);

    Transforms unwrapped = mirror;
    unwrapped.mFacingAzimuthDeg = 720.f;
    ensure_equals("two facing turns wrap to zero",
                  sanitizeTransforms(unwrapped).mFacingAzimuthDeg, 0.f);
    unwrapped.mFacingAzimuthDeg = 450.f;
    ensure_equals("unwrapped facing retains its azimuth",
                  sanitizeTransforms(unwrapped).mFacingAzimuthDeg, 90.f);
    computeLive(setup, unwrapped, reflected);
    ensure_equals("wrapped facing participates in reflection",
                  reflected[0].mYawDeg, 135.f);

    Transforms invalid = mirror;
    invalid.mFacingAzimuthDeg = std::numeric_limits<F32>::quiet_NaN();
    ensure_equals("non-finite facing sanitizes to zero",
                  sanitizeTransforms(invalid).mFacingAzimuthDeg, 0.f);
    computeLive(setup, invalid, reflected);
    ensure_equals("non-finite facing uses the original reflection",
                  reflected[0].mYawDeg, -45.f);
}

// Orbit is part of the world azimuth reflected across the subject's facing.
template<> template<>
void cine_light_rig_model_object::test<12>()
{
    Setup setup = classicSetup();
    LightBase reflected[LIGHT_COUNT];
    Transforms mirror;
    mirror.mMirror = true;

    setup.mLights[0].mYawDeg = 45.f;
    mirror.mFacingAzimuthDeg = 0.f;
    mirror.mYawDeg = 90.f;
    computeLive(setup, mirror, reflected);
    ensure_equals("orbit 90 is reflected before facing-zero mirror",
                  reflected[0].mYawDeg, -135.f);

    setup.mLights[0].mYawDeg = 30.f;
    mirror.mFacingAzimuthDeg = 90.f;
    mirror.mYawDeg = 45.f;
    computeLive(setup, mirror, reflected);
    ensure_equals("orbit 45 is reflected across facing 90",
                  reflected[0].mYawDeg, 105.f);

    const F32 spread[LIGHT_COUNT] = { 45.f, -45.f, 135.f, -135.f };
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        setup.mLights[i].mYawDeg = spread[i];
    }
    mirror.mFacingAzimuthDeg = 37.f;
    mirror.mYawDeg = 60.f;
    computeLive(setup, mirror, reflected);
    ensure_equals("nonzero facing and orbit use the oriented yaw",
                  reflected[0].mYawDeg, -31.f);
    Setup reflected_setup = setup;
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        reflected_setup.mLights[i] = reflected[i];
    }
    LightBase reflected_twice[LIGHT_COUNT];
    computeLive(reflected_setup, mirror, reflected_twice);
    LightBase identity[LIGHT_COUNT];
    computeLive(setup, Transforms(), identity);
    ensureSameLights("mirror twice is identity with facing and orbit",
                     identity, reflected_twice);

    setup.mLights[0].mYawDeg = 45.f;
    mirror.mFacingAzimuthDeg = 90.f;
    mirror.mYawDeg = 0.f;
    computeLive(setup, mirror, reflected);
    ensure_equals("orbit zero retains face-relative mirror behavior",
                  reflected[0].mYawDeg, 135.f);
}

// Master temperature is a relative linear-RGB gain with a byte-exact off path.
template<> template<>
void cine_light_rig_model_object::test<13>()
{
    const S32 profiles[] = { 0, 2, 5, 11, 23 };
    const F32 radii[] = { 0.5f, 1.5f, 9.1f, 512.f };
    const F32 master_evs[] = { -4.f, 0.f, 2.f };
    for (S32 profile : profiles)
    {
        for (S32 beam = 0; beam < BEAM_COUNT; ++beam)
        {
            Setup setup = classicSetup();
            for (S32 i = 0; i < LIGHT_COUNT; ++i)
            {
                setup.mLights[i].mProfile = profile;
                setup.mLights[i].mBeam = beam;
                setup.mLights[i].mOn = true;
            }
            LightBase live[LIGHT_COUNT];
            computeLive(setup, Transforms(), live);
            for (F32 radius : radii)
            {
                for (F32 master_ev : master_evs)
                {
                    Globals globals;
                    globals.mMasterEV = master_ev;
                    globals.mMasterTempMired = 0.f;
                    RigFrame golden;
                    RigFrame actual;
                    renderScaleOneGolden(radius, live, globals, golden);
                    render(radius, live, globals, actual);
                    ensure("zero master temp is a bitwise no-op",
                           std::memcmp(&golden, &actual,
                                       sizeof(RigFrame)) == 0);
                }
            }
        }
    }

    Setup setup = classicSetup();
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        setup.mLights[i].mOn = true;
    }
    LightBase live[LIGHT_COUNT];
    computeLive(setup, Transforms(), live);
    Globals zero_globals;
    RigFrame zero_frame;
    render(setup.mRadius, live, zero_globals, zero_frame);
    for (F32 invalid : {
             std::numeric_limits<F32>::quiet_NaN(),
             std::numeric_limits<F32>::infinity(),
             -std::numeric_limits<F32>::infinity() })
    {
        Globals invalid_globals;
        invalid_globals.mMasterTempMired = invalid;
        RigFrame invalid_frame;
        render(setup.mRadius, live, invalid_globals, invalid_frame);
        ensure("non-finite master temp falls back bitwise to zero",
               std::memcmp(&zero_frame, &invalid_frame,
                           sizeof(RigFrame)) == 0);
    }
    Globals cold_limit;
    cold_limit.mMasterTempMired = -110.f;
    Globals cold_extreme;
    cold_extreme.mMasterTempMired = -10000.f;
    RigFrame cold_limit_frame;
    RigFrame cold_extreme_frame;
    render(setup.mRadius, live, cold_limit, cold_limit_frame);
    render(setup.mRadius, live, cold_extreme, cold_extreme_frame);
    ensure("cold master temp clamp is exactly -110",
           std::memcmp(&cold_limit_frame, &cold_extreme_frame,
                       sizeof(RigFrame)) == 0);
    Globals warm_limit;
    warm_limit.mMasterTempMired = 150.f;
    Globals warm_extreme;
    warm_extreme.mMasterTempMired = 10000.f;
    RigFrame warm_limit_frame;
    RigFrame warm_extreme_frame;
    render(setup.mRadius, live, warm_limit, warm_limit_frame);
    render(setup.mRadius, live, warm_extreme, warm_extreme_frame);
    ensure("warm master temp clamp is exactly 150",
           std::memcmp(&warm_limit_frame, &warm_extreme_frame,
                       sizeof(RigFrame)) == 0);
    ensure_equals("sanitized cold limit is literal -110",
                  sanitizeGlobals(cold_extreme).mMasterTempMired, -110.f);
    ensure_equals("sanitized warm limit is literal 150",
                  sanitizeGlobals(warm_extreme).mMasterTempMired, 150.f);

    F32 gain[3];
    masterTempGain(100.f, gain);
    ensure("+100 mired red gain golden",
           closeRelative(gain[0], 1.466342f, 0.002f));
    ensure_equals("+100 mired green gain exact", gain[1], 1.f);
    ensure("+100 mired blue gain golden",
           closeRelative(gain[2], 0.534902f, 0.002f));
    masterTempGain(-50.f, gain);
    ensure("-50 mired red gain golden",
           closeRelative(gain[0], 0.838483f, 0.002f));
    ensure_equals("-50 mired green gain exact", gain[1], 1.f);
    ensure("-50 mired blue gain golden",
           closeRelative(gain[2], 1.338201f, 0.002f));

    setup.mLights[0].mProfile = 5;
    computeLive(setup, Transforms(), live);
    Globals warm;
    warm.mMasterTempMired = 100.f;
    RigFrame composed;
    render(setup.mRadius, live, warm, composed);
    ensure("8000K +100 red composition golden",
           std::fabs(composed.mProj[0].mSR - 0.5373f) <= 0.002f);
    ensure("8000K +100 green composition golden",
           std::fabs(composed.mProj[0].mSG - 0.55f) <= 0.002f);
    ensure("8000K +100 blue composition golden",
           std::fabs(composed.mProj[0].mSB - 0.7579f) <= 0.002f);

    setup.mLights[0].mProfile = 0;
    setup.mLights[1].mProfile = 5;
    computeLive(setup, Transforms(), live);
    for (F32 shift : { -110.f, -50.f, 50.f, 150.f })
    {
        Globals shifted;
        shifted.mMasterTempMired = shift;
        RigFrame frame;
        render(setup.mRadius, live, shifted, frame);
        const F32 warm_ratio = testSRGBToLinear(frame.mProj[0].mSR) /
            std::max(testSRGBToLinear(frame.mProj[0].mSB), 1.0e-6f);
        const F32 cool_ratio = testSRGBToLinear(frame.mProj[1].mSR) /
            std::max(testSRGBToLinear(frame.mProj[1].mSB), 1.0e-6f);
        ensure("relative warm/cool gel ordering survives the trim",
               warm_ratio > cool_ratio);
        ensure("cool gel remains at least as blue as warm gel",
               frame.mProj[1].mSB >= frame.mProj[0].mSB);
    }

    for (S32 profile : { 12, 23 })
    {
        setup.mLights[0].mProfile = profile;
        computeLive(setup, Transforms(), live);
        for (F32 shift : { -110.f, 150.f })
        {
            Globals shifted;
            shifted.mMasterTempMired = shift;
            RigFrame frame;
            render(setup.mRadius, live, shifted, frame);
            const EmitterState& state = frame.mProj[0];
            ensure("temperature output remains finite and bounded",
                   std::isfinite(state.mSR) && state.mSR >= 0.f &&
                   state.mSR <= 1.f && std::isfinite(state.mSG) &&
                   state.mSG >= 0.f && state.mSG <= 1.f &&
                   std::isfinite(state.mSB) && state.mSB >= 0.f &&
                   state.mSB <= 1.f);
        }
    }
    setup.mLights[0].mProfile = 23;
    computeLive(setup, Transforms(), live);
    render(setup.mRadius, live, warm_limit, composed);
    ensure("unit red saturates instead of wrapping",
           std::fabs(composed.mProj[0].mSR - 1.f) <= 2e-3f);
    setup.mLights[0].mProfile = 12;
    computeLive(setup, Transforms(), live);
    render(setup.mRadius, live, cold_limit, composed);
    ensure("unit blue saturates instead of wrapping",
           std::fabs(composed.mProj[0].mSB - 1.f) <= 2e-3f);

    F32 previous_red = -1.f;
    F32 previous_blue = std::numeric_limits<F32>::infinity();
    for (S32 sample = 0; sample <= 26; ++sample)
    {
        const F32 shift = -110.f + sample * 10.f;
        masterTempGain(shift, gain);
        ensure("red gain is strictly increasing", gain[0] > previous_red);
        ensure("blue gain is strictly decreasing", gain[2] < previous_blue);
        previous_red = gain[0];
        previous_blue = gain[2];
    }

    setup = classicSetup();
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        setup.mLights[i].mOn = true;
    }
    for (F32 ev : { -9.f, -4.f, 0.f, 2.f, 3.f, 4.f })
    {
        setup.mLights[0].mEV = ev;
        computeLive(setup, Transforms(), live);
        Globals reference_globals;
        RigFrame reference;
        render(setup.mRadius, live, reference_globals, reference);
        for (F32 shift : { -110.f, -50.f, 50.f, 100.f, 150.f })
        {
            Globals shifted = reference_globals;
            shifted.mMasterTempMired = shift;
            RigFrame frame;
            render(setup.mRadius, live, shifted, frame);
            for (S32 i = 0; i < LIGHT_COUNT; ++i)
            {
                ensure_equals("temperature leaves projector intensity exact",
                              frame.mProj[i].mIntensity,
                              reference.mProj[i].mIntensity);
                ensure_equals("temperature leaves projector clip exact",
                              frame.mProj[i].mClipped,
                              reference.mProj[i].mClipped);
                ensure_equals("temperature leaves omni intensity exact",
                              frame.mOmni[i].mIntensity,
                              reference.mOmni[i].mIntensity);
                ensure_equals("temperature leaves omni clip exact",
                              frame.mOmni[i].mClipped,
                              reference.mOmni[i].mClipped);
            }
        }
    }
}

// Gobo indices survive every pure-model copy and remain projector-only.
template<> template<>
void cine_light_rig_model_object::test<14>()
{
    ensure_equals("gobo table row count", GOBO_COUNT, 8);
    Setup clamp_setup = classicSetup();
    clamp_setup.mLights[0].mGobo = -5;
    clamp_setup.mLights[1].mGobo = 99;
    const Setup clamped = sanitizeSetup(clamp_setup);
    ensure_equals("negative gobo clamps to literal zero",
                  clamped.mLights[0].mGobo, 0);
    ensure_equals("high gobo clamps to literal seven",
                  clamped.mLights[1].mGobo, 7);

    Setup setup;
    std::memset(&setup, 0, sizeof(setup));
    setup.mRadius = 1.5f;
    const S32 round_trip_gobos[LIGHT_COUNT] = { 0, 3, 7, 3 };
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        setup.mLights[i].mYawDeg = static_cast<F32>(i * 20);
        setup.mLights[i].mPitchDeg = static_cast<F32>(i * 5);
        setup.mLights[i].mProfile = i;
        setup.mLights[i].mEV = static_cast<F32>(-i);
        setup.mLights[i].mBeam = i % BEAM_COUNT;
        setup.mLights[i].mOn = true;
        setup.mLights[i].mGobo = round_trip_gobos[i];
    }
    const Setup sanitized = sanitizeSetup(setup);
    ensure("sanitize preserves in-range gobos bitwise",
           std::memcmp(&setup, &sanitized, sizeof(Setup)) == 0);

    const S32 live_gobos[LIGHT_COUNT] = { 1, 2, 3, 4 };
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        setup.mLights[i].mGobo = live_gobos[i];
    }
    Transforms transforms;
    transforms.mMirror = true;
    transforms.mFacingAzimuthDeg = 37.f;
    transforms.mYawDeg = 60.f;
    transforms.mPitchDeg = 10.f;
    LightBase live[LIGHT_COUNT];
    computeLive(setup, transforms, live);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        ensure_equals("computeLive carries each distinct gobo",
                      live[i].mGobo, live_gobos[i]);
    }

    RigFrame frame;
    render(setup.mRadius, live, Globals(), frame);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        ensure_equals("projector receives its light gobo",
                      frame.mProj[i].mGobo, live_gobos[i]);
        ensure_equals("omni never receives a gobo",
                      frame.mOmni[i].mGobo, 0);
    }

    LightBase start;
    start.mGobo = 2;
    LightBase target;
    target.mGobo = 5;
    ensure_equals("gobo holds before discrete midpoint",
                  blendLight(start, target, 0.49f).mGobo, 2);
    ensure_equals("gobo swaps after discrete midpoint",
                  blendLight(start, target, 0.51f).mGobo, 5);
    ensure_equals("gobo start endpoint is exact",
                  blendLight(start, target, 0.f).mGobo, 2);
    ensure_equals("gobo target endpoint is exact",
                  blendLight(start, target, 1.f).mGobo, 5);

    for (S32 fx = 0; fx < FX_COUNT; ++fx)
    {
        for (F64 time : { 0.0, 1.25, 137.0 })
        {
            LightBase fx_lights[LIGHT_COUNT];
            evalFX(fx, 0x123456789abcdef0ULL, time, fx_lights);
            for (S32 i = 0; i < LIGHT_COUNT; ++i)
            {
                ensure_equals("FX always uses the default gobo",
                              fx_lights[i].mGobo, 0);
            }
        }
    }

    const char* const gobo_names[GOBO_COUNT] = {
        "Default", "Venetian Blinds", "Window Panes", "Prison Bars",
        "Slats", "Grid", "Soft Dapple", "Branches",
    };
    for (S32 i = 0; i < GOBO_COUNT; ++i)
    {
        ensure("gobo name pointer is valid", goboName(i) != nullptr);
        ensure_equals("gobo name golden", std::string(goboName(i)),
                      std::string(gobo_names[i]));
    }
    ensure_equals("low out-of-range gobo name is safe",
                  std::string(goboName(-5)), std::string("Default"));
    ensure_equals("high out-of-range gobo name is safe",
                  std::string(goboName(99)), std::string("Branches"));
}

template<> template<>
void cine_light_rig_model_object::test<15>()
{
    set_test_name("multi-anchor group geometry and exposure invariants");

    // 1. Literal goldens pin the shipped V13 rules independently of the
    // sanitizeGlobals delegation.
    ensure_equals("sanitize NaN -> 1", sanitizeSubjectScale(
        std::numeric_limits<F32>::quiet_NaN()), 1.f);
    ensure_equals("sanitize 0 -> 1", sanitizeSubjectScale(0.f), 1.f);
    ensure_equals("sanitize FLT_MIN -> 1", sanitizeSubjectScale(
        std::numeric_limits<F32>::min()), 1.f);
    ensure_equals("sanitize 0.04 -> 0.05",
                  sanitizeSubjectScale(0.04f), 0.05f);
    ensure_equals("sanitize 151 -> 150",
                  sanitizeSubjectScale(151.f), 150.f);
    ensure_equals("sanitize 1.5 passthrough",
                  sanitizeSubjectScale(1.5f), 1.5f);
    Globals delegated_scale;
    delegated_scale.mSubjectScale = 0.04f;
    ensure_equals("sanitizeGlobals delegates subject scale",
        sanitizeGlobals(delegated_scale).mSubjectScale, 0.05f);

    // 2-3. One point is an exact fast path; min/max, not a centroid, owns
    // the two-shot and clustered-three-subject centre.
    const F32 one_point[1][3] = { { 3.f, -2.f, 7.f } };
    F32 centre[3];
    groupBoundsCentre(one_point, 1, centre);
    ensure("single group centre is bitwise the source point",
           std::memcmp(one_point[0], centre, sizeof(centre)) == 0);

    const F32 two_points[2][3] = {
        { -2.f, 4.f, 0.f }, { 2.f, 8.f, 4.f },
    };
    groupBoundsCentre(two_points, 2, centre);
    ensure_equals("two-shot centre x", centre[0], 0.f);
    ensure_equals("two-shot centre y", centre[1], 6.f);
    ensure_equals("two-shot centre z", centre[2], 2.f);

    const F32 clustered[3][3] = {
        { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f }, { 4.f, 0.f, 0.f },
    };
    groupBoundsCentre(clustered, 3, centre);
    ensure_equals("clustered bounds midpoint differs from centroid",
                  centre[0], 2.f);
    ensure_equals("clustered centre y", centre[1], 0.f);
    ensure_equals("clustered centre z", centre[2], 0.f);

    // 4. Repeating a member cannot weight either aggregate.
    const F32 unique_points[2][3] = {
        { -1.f, 0.f, 0.f }, { 3.f, 2.f, 0.f },
    };
    const F32 duplicate_points[3][3] = {
        { -1.f, 0.f, 0.f }, { 3.f, 2.f, 0.f }, { -1.f, 0.f, 0.f },
    };
    const F32 unique_scales[2] = { 0.5f, 1.25f };
    const F32 duplicate_scales[3] = { 0.5f, 1.25f, 0.5f };
    F32 unique_centre[3];
    F32 duplicate_centre[3];
    groupBoundsCentre(unique_points, 2, unique_centre);
    groupBoundsCentre(duplicate_points, 3, duplicate_centre);
    ensure("duplicate leaves bounds centre bitwise unchanged",
           std::memcmp(unique_centre, duplicate_centre,
                       sizeof(unique_centre)) == 0);
    ensure_equals("duplicate leaves group scale unchanged",
        groupSubjectScale(unique_points, unique_scales, 2,
                          unique_centre, 2.f),
        groupSubjectScale(duplicate_points, duplicate_scales, 3,
                          duplicate_centre, 2.f));

    // 5. Count one returns before reading points, centre, or radius.
    const F32 solo_scale[1] = { 0.7f };
    ensure_equals("single-member scale fast path ignores other arguments",
        groupSubjectScale(nullptr, solo_scale, 1, nullptr,
                          std::numeric_limits<F32>::quiet_NaN()),
        0.7f);

    // 6. Exact power-of-two formula anchors, including the design's
    // three-metre two-shot.
    const F32 formula_points[2][3] = {
        { -1.f, 0.f, 0.f }, { 1.f, 0.f, 0.f },
    };
    const F32 formula_scales[2] = { 1.f, 0.05f };
    const F32 origin[3] = { 0.f, 0.f, 0.f };
    ensure_equals("group scale exact formula", groupSubjectScale(
        formula_points, formula_scales, 2, origin, 2.f), 1.5f);
    const F32 three_metre_points[2][3] = {
        { -1.5f, 0.f, 0.f }, { 1.5f, 0.f, 0.f },
    };
    const F32 unit_scales[2] = { 1.f, 1.f };
    ensure_equals("three-metre two-shot scale", groupSubjectScale(
        three_metre_points, unit_scales, 2, origin, 1.5f), 2.f);

    // 7. Extent and every member scale are monotonic inputs.
    F32 previous_scale = 0.f;
    for (S32 step = 0; step <= 20; ++step)
    {
        const F32 moving_points[2][3] = {
            { 0.f, 0.f, 0.f }, { static_cast<F32>(step), 0.f, 0.f },
        };
        F32 moving_centre[3];
        groupBoundsCentre(moving_points, 2, moving_centre);
        const F32 scale = groupSubjectScale(
            moving_points, unit_scales, 2, moving_centre, 2.f);
        ensure("group scale is non-decreasing with spread",
               scale >= previous_scale);
        previous_scale = scale;
    }
    F32 changing_scales[2] = { 0.05f, 0.05f };
    previous_scale = 0.f;
    for (S32 step = 1; step <= 20; ++step)
    {
        changing_scales[1] = static_cast<F32>(step) * 0.1f;
        const F32 scale = groupSubjectScale(
            formula_points, changing_scales, 2, origin, 2.f);
        ensure("group scale is non-decreasing with member scale",
               scale >= previous_scale);
        previous_scale = scale;
    }

    // 8. The group owns the same outer clamp and degenerate-scale rules.
    const F32 enormous_points[2][3] = {
        { -1000.f, 0.f, 0.f }, { 1000.f, 0.f, 0.f },
    };
    ensure_equals("enormous group spread clamps to maximum",
        groupSubjectScale(enormous_points, unit_scales, 2, origin, 0.5f),
        SUBJECT_SCALE_MAX);
    const F32 invalid_scales[2] = {
        std::numeric_limits<F32>::quiet_NaN(), -1.f,
    };
    const F32 coincident_points[2][3] = {
        { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f },
    };
    ensure_equals("degenerate group scales become nominal",
        groupSubjectScale(coincident_points, invalid_scales, 2,
                          origin, 1.5f), 1.f);

    // 9. Group-representative geometry scales may not leak into either
    // intensity channel or either clipped flag.
    Setup setup = classicSetup();
    LightBase live[LIGHT_COUNT];
    computeLive(setup, Transforms(), live);
    const F32 group_scales[] = { 1.5f, 1.667f, 2.f, 2.886f, 3.7f };
    for (F32 ev : { -8.f, -1.f, 0.f, 3.f, 8.f })
    {
        live[0].mEV = ev;
        Globals reference_globals;
        reference_globals.mSubjectScale = 1.f;
        RigFrame reference;
        render(setup.mRadius, live, reference_globals, reference);
        for (F32 scale : group_scales)
        {
            Globals group_globals = reference_globals;
            group_globals.mSubjectScale = scale;
            RigFrame group_frame;
            render(setup.mRadius, live, group_globals, group_frame);
            for (S32 i = 0; i < LIGHT_COUNT; ++i)
            {
                ensure_equals("projector intensity ignores group extent",
                    group_frame.mProj[i].mIntensity,
                    reference.mProj[i].mIntensity);
                ensure_equals("projector clipping ignores group extent",
                    group_frame.mProj[i].mClipped,
                    reference.mProj[i].mClipped);
                ensure_equals("omni intensity ignores group extent",
                    group_frame.mOmni[i].mIntensity,
                    reference.mOmni[i].mIntensity);
                ensure_equals("omni clipping ignores group extent",
                    group_frame.mOmni[i].mClipped,
                    reference.mOmni[i].mClipped);
            }
        }
    }

    // 10. Pin the coverage guarantee through render outputs, so removing
    // scale from render fails on |offset| + spread, not on copied algebra.
    struct CoverageCase
    {
        F32 mRadius;
        F32 mMemberScale;
        F32 mSpread;
    };
    const CoverageCase coverage_cases[] = {
        { 1.5f, 1.f, 2.f },
        { 1.5f, 1.f, 3.f },
        { 2.f, 0.5f, 2.5f },
        { 0.5f, 2.f, 1.f },
    };
    LightBase coverage_lights[LIGHT_COUNT] = {};
    coverage_lights[0].mOn = true;
    for (const CoverageCase& item : coverage_cases)
    {
        const F32 points[2][3] = {
            { -item.mSpread, 0.f, 0.f },
            { item.mSpread, 0.f, 0.f },
        };
        const F32 scales[2] = {
            item.mMemberScale, item.mMemberScale,
        };
        const F32 group_scale = groupSubjectScale(
            points, scales, 2, origin, item.mRadius);
        Globals coverage_globals;
        coverage_globals.mSubjectScale = group_scale;
        RigFrame coverage;
        render(item.mRadius, coverage_lights,
               coverage_globals, coverage);
        const F32 effective_orbit = std::sqrt(
            coverage.mProj[0].mOffX * coverage.mProj[0].mOffX +
            coverage.mProj[0].mOffY * coverage.mProj[0].mOffY +
            coverage.mProj[0].mOffZ * coverage.mProj[0].mOffZ);
        ensure("render output covers every member inside projector reach",
               effective_orbit + item.mSpread <=
                   coverage.mProj[0].mLightRadius *
                       (1.f / 1.1f + 1.e-5f));
    }
}
} // namespace tut
