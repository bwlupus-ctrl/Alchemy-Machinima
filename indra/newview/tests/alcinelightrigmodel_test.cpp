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
#include "../alcinelightrigmanager.h"
#include "../alcinelightrigmodel.h"
#include "../alcineliveproberefresh.h" // [LiveProbeRefresh]
#include "../alprobeschedule.h"        // [ProbeOnDemand]
#include "../llviewercamera.h"
#include "../pipeline.h"
#include "../../llrender/llshadermgr.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace tut
{
using namespace ALCineLightRigModel;

namespace
{
class FakeRigSettings
{
public:
    bool getBOOL(const std::string& name) { return mValues[name].asBoolean(); }
    F32 getF32(const std::string& name)
    {
        return static_cast<F32>(mValues[name].asReal());
    }
    S32 getS32(const std::string& name) { return mValues[name].asInteger(); }
    U32 getU32(const std::string& name)
    {
        return static_cast<U32>(mValues[name].asReal());
    }
    std::string getString(const std::string& name)
    {
        return mValues[name].asString();
    }
    void setBOOL(const std::string& name, bool value) { mValues[name] = value; }
    void setF32(const std::string& name, F32 value) { mValues[name] = value; }
    void setS32(const std::string& name, S32 value) { mValues[name] = value; }
    void setU32(const std::string& name, U32 value)
    {
        mValues[name] = static_cast<F64>(value);
    }
    void setString(const std::string& name, const std::string& value)
    {
        mValues[name] = value;
    }

private:
    std::map<std::string, LLSD> mValues;
};

ALCineLightRigParamBlob distinctiveBlob()
{
    ALCineLightRigParamBlob blob;
    blob.mEnabled = true;
    blob.mScaleAware = false;
    blob.mPower = false;
    blob.mRadius = 1.25f;
    blob.mMasterEV = -2.5f;
    blob.mMasterTempMired = 37.5f;
    blob.mOffsetZ = -3.75f;
    blob.mHeadroomStops = 4.5f;
    blob.mBounceEnabled = false;
    blob.mBounceRatio = 0.375f;
    blob.mTransitionSec = 1.75f;
    blob.mDamping = 2.25f;
    blob.mTrackMode = 1;
    blob.mCookieUUID = "01234567-89ab-cdef-0123-456789abcdef";
    blob.mSeed = 0xfedcba98u;
    blob.mMirror = true;
    blob.mOrbitYaw = -123.5f;
    blob.mOrbitPitch = 47.25f;
    blob.mFX = 3;
    blob.mShadowMode = 2;
    blob.mGizmo = true;
    blob.mRatioLock = true;
    blob.mRatio = 3.25f;
    blob.mCatchlight = true;
    blob.mCatchlightEV = 1.75f;
    blob.mCatchlightSize = 0.22f;
    blob.mCatchlightAngle = 137.5f;
    blob.mObjectTarget.set("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", false);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        blob.mLights[i].mYaw = -80.f + i * 17.f;
        blob.mLights[i].mPitch = 60.f - i * 11.f;
        blob.mLights[i].mProfile = 5 + i;
        blob.mLights[i].mEV = -3.f + i * 0.5f;
        blob.mLights[i].mBeam = 2 - (i / 2);
        blob.mLights[i].mGobo = 7 - i;
        blob.mLights[i].mGel = 4 + i;
        blob.mLights[i].mShadowSoft = 0.5f + i * 0.75f;
        blob.mLights[i].mOn = i == 3;
        blob.mLights[i].mFlickerProgram = i + 1;
        blob.mLights[i].mFlickerAmount = 0.2f * (i + 1);
        blob.mLights[i].mFixtureMode = (i & 1) == 0;
        blob.mLights[i].mKelvin = 2700.f + i * 1100.f;
        blob.mLights[i].mGelSlot[0] = 1 + i;
        blob.mLights[i].mGelSlot[1] = 9 + i;
        blob.mLights[i].mGelSlot[2] = 15 + i;
        blob.mLights[i].mSourceSizeM = 0.05f * (i + 1);
        blob.mLights[i].mFixturePreset = 2 + i;
        // [RigRim] per-light rim params (Phase A).
        blob.mLights[i].mRimGain = 0.1f + i * 0.2f;
        blob.mLights[i].mRimSharpness = 1.f + i * 0.5f;
        blob.mLights[i].mRimWrap = 0.07f + i * 0.1f; // [RigRim] avoid 0.35 == kDefaultRimWrap
        blob.mLights[i].mRimBackBias = -0.5f + i * 0.3f;
        blob.mShaftEnabled[i] = (i & 1) != 0;
        blob.mHeroEnabled[i] = i >= 2;
    }
    blob.mAnchor.set("11111111-2222-3333-4444-555555555555", false);
    blob.mGroupEnabled = true;
    blob.mGroupSlots = ALCineLightRig::GROUP_SLOT_A |
                       ALCineLightRig::GROUP_SLOT_C;
    blob.mFXPhase = 12.125;
    blob.mPendingFXPhase = 7.75;
    blob.mPendingFXId = 4;
    return blob;
}

void ensureSameBlob(const ALCineLightRigParamBlob& expected,
                    const ALCineLightRigParamBlob& actual,
                    bool include_session)
{
    ensure_equals("blob Enabled", actual.mEnabled, expected.mEnabled);
    ensure_equals("blob ScaleAware", actual.mScaleAware, expected.mScaleAware);
    ensure_equals("blob Power", actual.mPower, expected.mPower);
    ensure_equals("blob Radius", actual.mRadius, expected.mRadius);
    ensure_equals("blob MasterEV", actual.mMasterEV, expected.mMasterEV);
    ensure_equals("blob MasterTemp", actual.mMasterTempMired, expected.mMasterTempMired);
    ensure_equals("blob OffsetZ", actual.mOffsetZ, expected.mOffsetZ);
    ensure_equals("blob Headroom", actual.mHeadroomStops, expected.mHeadroomStops);
    ensure_equals("blob BounceEnabled", actual.mBounceEnabled, expected.mBounceEnabled);
    ensure_equals("blob BounceRatio", actual.mBounceRatio, expected.mBounceRatio);
    ensure_equals("blob Transition", actual.mTransitionSec, expected.mTransitionSec);
    ensure_equals("blob Damping", actual.mDamping, expected.mDamping);
    ensure_equals("blob TrackMode", actual.mTrackMode, expected.mTrackMode);
    ensure_equals("blob CookieUUID", actual.mCookieUUID, expected.mCookieUUID);
    ensure_equals("blob Seed", actual.mSeed, expected.mSeed);
    ensure_equals("blob Mirror", actual.mMirror, expected.mMirror);
    ensure_equals("blob OrbitYaw", actual.mOrbitYaw, expected.mOrbitYaw);
    ensure_equals("blob OrbitPitch", actual.mOrbitPitch, expected.mOrbitPitch);
    ensure_equals("blob FX", actual.mFX, expected.mFX);
    ensure_equals("blob ShadowMode", actual.mShadowMode, expected.mShadowMode);
    ensure_equals("blob Gizmo", actual.mGizmo, expected.mGizmo);
    ensure_equals("blob RatioLock", actual.mRatioLock, expected.mRatioLock);
    ensure_equals("blob Ratio", actual.mRatio, expected.mRatio);
    ensure_equals("blob Catchlight", actual.mCatchlight, expected.mCatchlight);
    ensure_equals("blob CatchlightEV", actual.mCatchlightEV, expected.mCatchlightEV);
    ensure_equals("blob CatchlightSize", actual.mCatchlightSize, expected.mCatchlightSize);
    ensure_equals("blob CatchlightAngle", actual.mCatchlightAngle, expected.mCatchlightAngle);
    ensure_equals("blob ObjectTarget", actual.mObjectTarget, expected.mObjectTarget);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        ensure_equals("blob light Yaw", actual.mLights[i].mYaw, expected.mLights[i].mYaw);
        ensure_equals("blob light Pitch", actual.mLights[i].mPitch, expected.mLights[i].mPitch);
        ensure_equals("blob light Profile", actual.mLights[i].mProfile, expected.mLights[i].mProfile);
        ensure_equals("blob light EV", actual.mLights[i].mEV, expected.mLights[i].mEV);
        ensure_equals("blob light Beam", actual.mLights[i].mBeam, expected.mLights[i].mBeam);
        ensure_equals("blob light Gobo", actual.mLights[i].mGobo, expected.mLights[i].mGobo);
        ensure_equals("blob light Gel", actual.mLights[i].mGel, expected.mLights[i].mGel);
        ensure_equals("blob light ShadowSoft", actual.mLights[i].mShadowSoft, expected.mLights[i].mShadowSoft);
        ensure_equals("blob light On", actual.mLights[i].mOn, expected.mLights[i].mOn);
        ensure_equals("blob light FlickerProgram", actual.mLights[i].mFlickerProgram, expected.mLights[i].mFlickerProgram);
        ensure_equals("blob light FlickerAmount", actual.mLights[i].mFlickerAmount, expected.mLights[i].mFlickerAmount);
        ensure_equals("blob light FixtureMode", actual.mLights[i].mFixtureMode, expected.mLights[i].mFixtureMode);
        ensure_equals("blob light Kelvin", actual.mLights[i].mKelvin, expected.mLights[i].mKelvin);
        for (S32 slot = 0; slot < FIXTURE_GEL_SLOT_COUNT; ++slot)
        {
            ensure_equals("blob light fixture gel slot",
                          actual.mLights[i].mGelSlot[slot],
                          expected.mLights[i].mGelSlot[slot]);
        }
        ensure_equals("blob light SourceSize", actual.mLights[i].mSourceSizeM, expected.mLights[i].mSourceSizeM);
        ensure_equals("blob light FixturePreset", actual.mLights[i].mFixturePreset, expected.mLights[i].mFixturePreset);
        // [RigRim] per-light rim params (Phase A).
        ensure_equals("blob light RimGain", actual.mLights[i].mRimGain, expected.mLights[i].mRimGain);
        ensure_equals("blob light RimSharpness", actual.mLights[i].mRimSharpness, expected.mLights[i].mRimSharpness);
        ensure_equals("blob light RimWrap", actual.mLights[i].mRimWrap, expected.mLights[i].mRimWrap);
        ensure_equals("blob light RimBackBias", actual.mLights[i].mRimBackBias, expected.mLights[i].mRimBackBias);
    }
    if (!include_session)
    {
        return;
    }
    ensure_equals("blob Anchor", actual.mAnchor, expected.mAnchor);
    ensure_equals("blob GroupEnabled", actual.mGroupEnabled, expected.mGroupEnabled);
    ensure_equals("blob GroupSlots", actual.mGroupSlots, expected.mGroupSlots);
    ensure_equals("blob FX phase", actual.mFXPhase, expected.mFXPhase);
    ensure_equals("blob pending FX phase", actual.mPendingFXPhase, expected.mPendingFXPhase);
    ensure_equals("blob pending FX id", actual.mPendingFXId, expected.mPendingFXId);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        ensure_equals("blob shaft", actual.mShaftEnabled[i], expected.mShaftEnabled[i]);
        ensure_equals("blob hero", actual.mHeroEnabled[i], expected.mHeroEnabled[i]);
    }
}

void seedSettingsIndependently(FakeRigSettings& settings,
                               const ALCineLightRigParamBlob& value)
{
    settings.setBOOL("CineLightRigEnabled", value.mEnabled);
    settings.setBOOL("CineLightRigScaleAware", value.mScaleAware);
    settings.setBOOL("CineLightRigPower", value.mPower);
    settings.setF32("CineLightRigRadius", value.mRadius);
    settings.setF32("CineLightRigMasterEV", value.mMasterEV);
    settings.setF32("CineLightRigMasterTempMired", value.mMasterTempMired);
    settings.setF32("CineLightRigOffsetZ", value.mOffsetZ);
    settings.setF32("CineLightRigHeadroomStops", value.mHeadroomStops);
    settings.setBOOL("CineLightRigBounceEnabled", value.mBounceEnabled);
    settings.setF32("CineLightRigBounceRatio", value.mBounceRatio);
    settings.setF32("CineLightRigTransitionSec", value.mTransitionSec);
    settings.setF32("CineLightRigDamping", value.mDamping);
    settings.setS32("CineLightRigTrackMode", value.mTrackMode);
    settings.setString("CineLightRigCookieUUID", value.mCookieUUID);
    settings.setU32("CineLightRigSeed", value.mSeed);
    settings.setBOOL("CineLightRigMirror", value.mMirror);
    settings.setF32("CineLightRigOrbitYaw", value.mOrbitYaw);
    settings.setF32("CineLightRigOrbitPitch", value.mOrbitPitch);
    settings.setS32("CineLightRigFX", value.mFX);
    settings.setS32("CineLightRigShadowMode", value.mShadowMode);
    settings.setBOOL("CineLightRigGizmo", value.mGizmo);
    settings.setBOOL("CineLightRigRatioLock", value.mRatioLock);
    settings.setF32("CineLightRigRatio", value.mRatio);
    settings.setBOOL("CineLightRigCatchlight", value.mCatchlight);
    settings.setF32("CineLightRigCatchlightEV", value.mCatchlightEV);
    settings.setF32("CineLightRigCatchlightSize", value.mCatchlightSize);
    settings.setF32("CineLightRigCatchlightAngle", value.mCatchlightAngle);
    settings.setString("CineLightRigObjectTarget", value.mObjectTarget.asString());
    static const char* const prefixes[LIGHT_COUNT] = {
        "CineLightRigKey", "CineLightRigFill",
        "CineLightRigRim", "CineLightRigBg"
    };
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        const std::string prefix(prefixes[i]);
        settings.setF32(prefix + "Yaw", value.mLights[i].mYaw);
        settings.setF32(prefix + "Pitch", value.mLights[i].mPitch);
        settings.setS32(prefix + "Profile", value.mLights[i].mProfile);
        settings.setF32(prefix + "EV", value.mLights[i].mEV);
        settings.setS32(prefix + "Beam", value.mLights[i].mBeam);
        settings.setS32(prefix + "Gobo", value.mLights[i].mGobo);
        settings.setS32(prefix + "Gel", value.mLights[i].mGel);
        settings.setF32(prefix + "ShadowSoft", value.mLights[i].mShadowSoft);
        settings.setBOOL(prefix + "On", value.mLights[i].mOn);
        settings.setS32(prefix + "Flicker", value.mLights[i].mFlickerProgram);
        settings.setF32(prefix + "FlickerAmount", value.mLights[i].mFlickerAmount);
        settings.setBOOL(prefix + "FixtureMode", value.mLights[i].mFixtureMode);
        settings.setF32(prefix + "Kelvin", value.mLights[i].mKelvin);
        for (S32 slot = 0; slot < FIXTURE_GEL_SLOT_COUNT; ++slot)
        {
            settings.setS32(prefix + "GelSlot" + std::to_string(slot),
                            value.mLights[i].mGelSlot[slot]);
        }
        settings.setF32(prefix + "SourceSizeM", value.mLights[i].mSourceSizeM);
        settings.setS32(prefix + "FixturePreset", value.mLights[i].mFixturePreset);
    }
    // [RigRim] per-light rim params (Phase A): second prefix table.
    static const char* const rim_prefixes[LIGHT_COUNT] = {
        "CineRigRimKey", "CineRigRimFill",
        "CineRigRimRim", "CineRigRimBg"
    };
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        const std::string rim_prefix(rim_prefixes[i]);
        settings.setF32(rim_prefix + "Gain", value.mLights[i].mRimGain);
        settings.setF32(rim_prefix + "Sharpness", value.mLights[i].mRimSharpness);
        settings.setF32(rim_prefix + "Wrap", value.mLights[i].mRimWrap);
        settings.setF32(rim_prefix + "BackBias", value.mLights[i].mRimBackBias);
    }
}

void ensureSettingsMatchIndependently(
    FakeRigSettings& settings, const ALCineLightRigParamBlob& expected)
{
    ALCineLightRigParamBlob actual;
    actual.mEnabled = settings.getBOOL("CineLightRigEnabled");
    actual.mScaleAware = settings.getBOOL("CineLightRigScaleAware");
    actual.mPower = settings.getBOOL("CineLightRigPower");
    actual.mRadius = settings.getF32("CineLightRigRadius");
    actual.mMasterEV = settings.getF32("CineLightRigMasterEV");
    actual.mMasterTempMired = settings.getF32("CineLightRigMasterTempMired");
    actual.mOffsetZ = settings.getF32("CineLightRigOffsetZ");
    actual.mHeadroomStops = settings.getF32("CineLightRigHeadroomStops");
    actual.mBounceEnabled = settings.getBOOL("CineLightRigBounceEnabled");
    actual.mBounceRatio = settings.getF32("CineLightRigBounceRatio");
    actual.mTransitionSec = settings.getF32("CineLightRigTransitionSec");
    actual.mDamping = settings.getF32("CineLightRigDamping");
    actual.mTrackMode = settings.getS32("CineLightRigTrackMode");
    actual.mCookieUUID = settings.getString("CineLightRigCookieUUID");
    actual.mSeed = settings.getU32("CineLightRigSeed");
    actual.mMirror = settings.getBOOL("CineLightRigMirror");
    actual.mOrbitYaw = settings.getF32("CineLightRigOrbitYaw");
    actual.mOrbitPitch = settings.getF32("CineLightRigOrbitPitch");
    actual.mFX = settings.getS32("CineLightRigFX");
    actual.mShadowMode = settings.getS32("CineLightRigShadowMode");
    actual.mGizmo = settings.getBOOL("CineLightRigGizmo");
    actual.mRatioLock = settings.getBOOL("CineLightRigRatioLock");
    actual.mRatio = settings.getF32("CineLightRigRatio");
    actual.mCatchlight = settings.getBOOL("CineLightRigCatchlight");
    actual.mCatchlightEV = settings.getF32("CineLightRigCatchlightEV");
    actual.mCatchlightSize = settings.getF32("CineLightRigCatchlightSize");
    actual.mCatchlightAngle = settings.getF32("CineLightRigCatchlightAngle");
    actual.mObjectTarget.set(
        settings.getString("CineLightRigObjectTarget"), false);
    static const char* const prefixes[LIGHT_COUNT] = {
        "CineLightRigKey", "CineLightRigFill",
        "CineLightRigRim", "CineLightRigBg"
    };
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        const std::string prefix(prefixes[i]);
        actual.mLights[i].mYaw = settings.getF32(prefix + "Yaw");
        actual.mLights[i].mPitch = settings.getF32(prefix + "Pitch");
        actual.mLights[i].mProfile = settings.getS32(prefix + "Profile");
        actual.mLights[i].mEV = settings.getF32(prefix + "EV");
        actual.mLights[i].mBeam = settings.getS32(prefix + "Beam");
        actual.mLights[i].mGobo = settings.getS32(prefix + "Gobo");
        actual.mLights[i].mGel = settings.getS32(prefix + "Gel");
        actual.mLights[i].mShadowSoft = settings.getF32(prefix + "ShadowSoft");
        actual.mLights[i].mOn = settings.getBOOL(prefix + "On");
        actual.mLights[i].mFlickerProgram = settings.getS32(prefix + "Flicker");
        actual.mLights[i].mFlickerAmount =
            settings.getF32(prefix + "FlickerAmount");
        actual.mLights[i].mFixtureMode =
            settings.getBOOL(prefix + "FixtureMode");
        actual.mLights[i].mKelvin = settings.getF32(prefix + "Kelvin");
        for (S32 slot = 0; slot < FIXTURE_GEL_SLOT_COUNT; ++slot)
        {
            actual.mLights[i].mGelSlot[slot] = settings.getS32(
                prefix + "GelSlot" + std::to_string(slot));
        }
        actual.mLights[i].mSourceSizeM =
            settings.getF32(prefix + "SourceSizeM");
        actual.mLights[i].mFixturePreset =
            settings.getS32(prefix + "FixturePreset");
    }
    // [RigRim] per-light rim params (Phase A): second prefix table.
    static const char* const rim_prefixes[LIGHT_COUNT] = {
        "CineRigRimKey", "CineRigRimFill",
        "CineRigRimRim", "CineRigRimBg"
    };
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        const std::string rim_prefix(rim_prefixes[i]);
        actual.mLights[i].mRimGain = settings.getF32(rim_prefix + "Gain");
        actual.mLights[i].mRimSharpness =
            settings.getF32(rim_prefix + "Sharpness");
        actual.mLights[i].mRimWrap = settings.getF32(rim_prefix + "Wrap");
        actual.mLights[i].mRimBackBias =
            settings.getF32(rim_prefix + "BackBias");
    }
    ensureSameBlob(expected, actual, false);
}

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
    out.mCatchlightSizeScale = 1.f;
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
// [RigRim] Round-2 P2-A fix: tut::test_group defaults MaxTestsInGroup to 50
// (tut.hpp ~130) and registers test<n> for n = MaxTestsInGroup..1 by
// recursive template instantiation starting AT that ceiling -- test<51>
// through test<54> below compiled but were never registered/run. Raised to
// 64 for headroom. [LiveProbeRefresh] adds test<55>..test<69>, so the ceiling
// is now 72 (same trap: anything above it compiles but silently never runs).
// [ProbeOnDemand] adds test<72>..test<90> (S1-S18 + the S15b recount bound),
// so the ceiling is now 96 (test<91> adds the cache-provenance S19).
typedef test_group<cine_light_rig_model_data, 96> cine_light_rig_model_group;
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
        setup.mLights[i].mFlickerProgram = 1000;
        setup.mLights[i].mFlickerAmount =
            std::numeric_limits<F32>::quiet_NaN();
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
        ensure("flicker program clamped",
               safe.mLights[i].mFlickerProgram >= FLICKER_NONE &&
               safe.mLights[i].mFlickerProgram < FLICKER_COUNT);
        ensure("flicker amount finite and clamped",
               std::isfinite(safe.mLights[i].mFlickerAmount) &&
               safe.mLights[i].mFlickerAmount >= 0.f &&
               safe.mLights[i].mFlickerAmount <= 1.f);
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
        1, 2, 5, 6, 9, 11, 13, 14, 17, 20, 25, 27, 30, 31,
        33, 34, 35, 37, 41, 42, 43, 44, 48,
        50, 51, 52, 53, 54, 56, 58, 59,
        68, 70
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
    ensure_approximately_equals_range(
        "Stage fill latch reaches the ramp endpoint",
        sampleFXBeat(29, 21, 1).mEV,
        sampleFXBeat(29, 20, 1).mEV, 1e-6f);
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
    ensure_equals("Explosion preserves hidden latched profile",
                  explosion_afterglow.mProfile, explosion_hidden.mProfile);
    ensure_approximately_equals_range(
        "Explosion preserves hidden latched EV",
        explosion_afterglow.mEV, explosion_hidden.mEV, 1e-6f);
    ensure("Explosion beat 45 leaves key only",
           sampleFXBeat(27, 45, 0).mOn &&
           !sampleFXBeat(27, 45, 1).mOn);
    const F32 explosion_ember = sampleFXBeat(27, 44, 1).mEV;
    ensure_approximately_equals_range(
        "Explosion beat 45 replays beat-44 ember",
        sampleFXBeat(27, 45, 1).mEV, explosion_ember, 1e-6f);
    ensure_approximately_equals_range(
        "Explosion afterglow does not re-roll ember",
        sampleFXBeat(27, 63, 1).mEV, explosion_ember, 1e-6f);
    const F32 second_cycle_ember = sampleFXBeat(27, 65 + 44, 1).mEV;
    ensure_approximately_equals_range(
        "Explosion cycle 1 replays its beat-44 ember",
        sampleFXBeat(27, 65 + 45, 1).mEV,
        second_cycle_ember, 1e-6f);
    ensure_approximately_equals_range(
        "Explosion cycle 1 keeps its ember through afterglow",
        sampleFXBeat(27, 65 + 63, 1).mEV,
        second_cycle_ember, 1e-6f);

    ensure_equals("Supernova beat 0 begins at -2 EV",
                  sampleFXBeat(28, 0, 0).mEV, -2.f);
    ensure_equals("Supernova beat 50 flashes",
                  sampleFXBeat(28, 50, 0).mEV, 2.f);
    ensure_equals("Supernova beat 53 begins collapse",
                  sampleFXBeat(28, 53, 0).mEV, 2.f);
    ensure("Supernova beat 65 is two-light remnant",
           sampleFXBeat(28, 65, 0).mOn &&
           !sampleFXBeat(28, 65, 2).mOn);
    ensure_approximately_equals_range(
        "Supernova dark tail preserves hidden collapse state",
        sampleFXBeat(28, 75, 2).mEV,
        sampleFXBeat(28, 65, 2).mEV, 1e-6f);
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
    ensure_equals("FX name row count", FX_COUNT, 71);
    ensure_equals("FX interval row count", FX_COUNT, 71);

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
        "Slow Dance", "Seance Circle", "Strobe Runway", "Ember Wind",
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
        0.12f, 0.05f, 0.05f, 0.20f, 0.25f, 0.10f, 0.125f, 0.10f,
        0.10f, 0.08f, 0.25f, 0.10f, 0.06f, 0.15f, 0.20f, 0.10f,
        0.10f,
        0.125f, 0.05f, 0.10f, 0.05f, 0.10f, 0.10f, 0.10f, 0.05f,
        0.10f, 0.20f, 0.20f, 0.25f, 0.10f, 0.15f,
        0.10f, 0.10f, 0.20f, 0.10f,
        0.25f, 0.15f, 0.06f, 0.10f,
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

    // 5-6. The derived ceiling and floor are respected within float precision
    // and keep projector reach
    // within the underlying 20 m viewer clamp.
    ensure_approximately_equals_range(
        "scaled radius reaches its ceiling",
        giant.mProj[0].mOffX, SCALED_RADIUS_CEIL, 1e-5f);
    ensure("projector reach stays within 20 metres",
           giant.mProj[0].mLightRadius <= 20.f);
    globals.mSubjectScale = 0.05f;
    RigFrame doll;
    render(1.5f, live, globals, doll);
    ensure_approximately_equals_range(
        "scaled radius reaches its floor",
        doll.mProj[0].mOffX, SCALED_RADIUS_FLOOR, 1e-6f);

    // 7. The min/max-bounded clamp never shrinks a large nominal at scale-up,
    // while retaining proportional scale-down and the small-nominal floor.
    globals.mSubjectScale = 1.f;
    RigFrame large_one;
    render(12.f, live, globals, large_one);
    ensure_approximately_equals_range(
        "large nominal survives scale one",
        large_one.mProj[0].mOffX, 12.f, 1e-5f);
    globals.mSubjectScale = 2.f;
    RigFrame large_up;
    render(12.f, live, globals, large_up);
    ensure_approximately_equals_range(
        "large nominal is not shrunk on scale-up",
        large_up.mProj[0].mOffX, 12.f, 1e-5f);
    globals.mSubjectScale = 0.5f;
    RigFrame large_down;
    render(12.f, live, globals, large_down);
    ensure_approximately_equals_range(
        "large nominal scales down proportionally",
        large_down.mProj[0].mOffX, 6.f, 1e-5f);
    globals.mSubjectScale = 0.05f;
    RigFrame small_floor;
    render(0.5f, live, globals, small_floor);
    ensure_approximately_equals_range(
        "minimum nominal observes scaled floor",
        small_floor.mProj[0].mOffX, SCALED_RADIUS_FLOOR, 1e-6f);

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
    ensure_equals("gobo table row count", GOBO_COUNT, 24);
    Setup clamp_setup = classicSetup();
    clamp_setup.mLights[0].mGobo = -5;
    clamp_setup.mLights[1].mGobo = 99;
    const Setup clamped = sanitizeSetup(clamp_setup);
    ensure_equals("negative gobo clamps to literal zero",
                  clamped.mLights[0].mGobo, 0);
    ensure_equals("high gobo clamps to literal twenty-three",
                  clamped.mLights[1].mGobo, 23);

    Setup setup;
    setup.mRadius = 1.5f;
    const S32 round_trip_gobos[LIGHT_COUNT] = { 0, 8, 17, 23 };
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
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        ensure_equals("sanitize preserves each in-range gobo",
                      sanitized.mLights[i].mGobo, round_trip_gobos[i]);
    }

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
                S32 expected_gobo = 0;
                if (fx == 40 && i == 0)
                {
                    expected_gobo = 4; // Night Train: Slats on Key.
                }
                else if (fx == FX_AFTERHOURS_DRIFT && i == 0)
                {
                    expected_gobo = 10; // Curtain Edge on Key.
                }
                else if (fx == FX_AFTERHOURS_DRIFT && i == 3)
                {
                    expected_gobo = 23; // Neon Sign Mask on Background.
                }
                else if (fx == FX_SOMETHING_BEHIND_YOU && i == 0)
                {
                    expected_gobo = 7; // Branches on Key.
                }
                ensure_equals("FX gobo matches the FX contract",
                              fx_lights[i].mGobo, expected_gobo);
            }
        }
    }

    const char* const gobo_names[GOBO_COUNT] = {
        "Default", "Venetian Blinds", "Window Panes", "Prison Bars",
        "Slats", "Grid", "Soft Dapple", "Branches",
        "Arched Window", "French Door", "Curtain Edge", "Stairwell Rail",
        "Door Crack", "Dense Foliage", "Palm Dapple", "Water Caustics",
        "Classic Cucoloris", "Fine Celo", "Scrim Wave", "Smoke Drift",
        "Chain Link", "Industrial Grate", "Rotating Fan", "Neon Sign Mask",
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
                  std::string(goboName(99)), std::string("Neon Sign Mask"));
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

template<> template<>
void cine_light_rig_model_object::test<16>()
{
    set_test_name("multi-instance slot and frozen group-bit mapping");
    using namespace ALCineLightRigManagerModel;
    const U32 expected_bits[5] = { 0x01u, 0x02u, 0x04u, 0x08u, 0x10u };
    const U32 shipped_bits[5] = {
        ALCineLightRig::GROUP_SLOT_SELF, ALCineLightRig::GROUP_SLOT_A,
        ALCineLightRig::GROUP_SLOT_B, ALCineLightRig::GROUP_SLOT_C,
        ALCineLightRig::GROUP_SLOT_D
    };
    for (S32 i = 0; i < 5; ++i)
    {
        const ALCineLightRigSlot slot = static_cast<ALCineLightRigSlot>(i);
        ensure_equals("shipped group bit retains its frozen numeric value",
                      shipped_bits[i], expected_bits[i]);
        ensure_equals("slot maps to the shipped group bit",
                      slotToGroupBit(slot), expected_bits[i]);
        ensure_equals("shipped group bit maps back to the same slot",
                      static_cast<S32>(groupBitToSlot(expected_bits[i])), i);
    }
    ensure_equals("multi-bit value has no reverse slot",
        static_cast<S32>(groupBitToSlot(ALCineLightRig::GROUP_SLOT_A |
                                       ALCineLightRig::GROUP_SLOT_B)),
        static_cast<S32>(ALCineLightRigSlot::COUNT));
}

template<> template<>
void cine_light_rig_model_object::test<17>()
{
    set_test_name("multi-instance blob LLSD round-trip is field-exact");
    const ALCineLightRigParamBlob expected = distinctiveBlob();
    const ALCineLightRigParamBlob actual =
        ALCineLightRigParamBlob::fromLLSD(expected.toLLSD());
    ensureSameBlob(expected, actual, true);
}

template<> template<>
void cine_light_rig_model_object::test<18>()
{
    set_test_name("multi-instance 100-key settings mapping round-trip");
    const ALCineLightRigParamBlob expected = distinctiveBlob();
    FakeRigSettings settings;
    seedSettingsIndependently(settings, expected);
    const ALCineLightRigParamBlob captured =
        ALCineLightRigParamBlob::fromSettingsStore(settings);
    ensureSameBlob(expected, captured, false);

    // Destroy every value in the fixture before restoring the capture. This
    // catches a missing setter as well as a missing/transposed getter.
    ALCineLightRigParamBlob defaults;
    seedSettingsIndependently(settings, defaults);
    captured.toSettingsStore(settings);
    ensureSettingsMatchIndependently(settings, expected);
}

template<> template<>
void cine_light_rig_model_object::test<19>()
{
    set_test_name("camera-focus argmin, tie break, and disabled exclusion");
    using namespace ALCineLightRigManagerModel;
    FocusPoint points[5] = {
        {10.0, 0.0, 0.0, true},
        { 2.0, 0.0, 0.0, true},
        { 1.0, 0.0, 0.0, true},
        { 4.0, 0.0, 0.0, true},
        { 5.0, 0.0, 0.0, true},
    };
    const FocusPoint focus = {0.0, 0.0, 0.0, true};
    const FocusPoint axis = {1.0, 0.0, 0.0, true};
    ensure_equals("nearest enabled slot wins",
        static_cast<S32>(rawFocusSlot(points, 0x1fu, focus, axis)),
        static_cast<S32>(ALCineLightRigSlot::B));
    ensure_equals("disabled nearest slot cannot win",
        static_cast<S32>(rawFocusSlot(
            points, 0x1fu & ~ALCineLightRig::GROUP_SLOT_B, focus, axis)),
        static_cast<S32>(ALCineLightRigSlot::A));

    points[1] = {-2.0, 0.0, 0.0, true};
    points[2] = { 2.0, 0.0, 0.0, true};
    ensure_equals("equal distance prefers the smaller view-axis angle",
        static_cast<S32>(rawFocusSlot(points,
            ALCineLightRig::GROUP_SLOT_A |
            ALCineLightRig::GROUP_SLOT_B, focus, axis)),
        static_cast<S32>(ALCineLightRigSlot::B));
    points[1] = {2.0, 1.0, 0.0, true};
    points[2] = {2.0, 1.0, 0.0, true};
    ensure_equals("full geometric tie resolves by lower slot order",
        static_cast<S32>(rawFocusSlot(points,
            ALCineLightRig::GROUP_SLOT_A |
            ALCineLightRig::GROUP_SLOT_B, focus, axis)),
        static_cast<S32>(ALCineLightRigSlot::A));
}

template<> template<>
void cine_light_rig_model_object::test<20>()
{
    set_test_name("camera-focus hysteresis blocks thrash and hands off once");
    using namespace ALCineLightRigManagerModel;
    FocusFilterState state;
    state.mFocus = ALCineLightRigSlot::SELF;
    S32 handoffs = 0;
    for (S32 tick = 0; tick < FOCUS_DWELL_TICKS * 3; ++tick)
    {
        const ALCineLightRigSlot raw = (tick & 1)
            ? ALCineLightRigSlot::A : ALCineLightRigSlot::B;
        handoffs += focusFilter(state, raw, 9.5, 10.0) ? 1 : 0;
        ensure_equals("sub-margin oscillation never changes focus",
                      static_cast<S32>(state.mFocus),
                      static_cast<S32>(ALCineLightRigSlot::SELF));
    }
    ensure_equals("anti-thrash sequence has no handoff", handoffs, 0);

    for (S32 tick = 1; tick <= FOCUS_DWELL_TICKS; ++tick)
    {
        const bool changed = focusFilter(
            state, ALCineLightRigSlot::A, 8.0, 10.0);
        if (tick < FOCUS_DWELL_TICKS)
        {
            ensure("clear challenger waits for the full dwell", !changed);
            ensure_equals("incumbent remains before dwell",
                          static_cast<S32>(state.mFocus),
                          static_cast<S32>(ALCineLightRigSlot::SELF));
        }
        else
        {
            ensure("handoff occurs on the dwell-th tick", changed);
            ++handoffs;
        }
    }
    ensure_equals("sustained challenge produces one clean handoff",
                  handoffs, 1);
    ensure_equals("challenger owns focus after handoff",
                  static_cast<S32>(state.mFocus),
                  static_cast<S32>(ALCineLightRigSlot::A));
}

template<> template<>
void cine_light_rig_model_object::test<21>()
{
    set_test_name("production suppression keeps only focus projectors eligible");
    using namespace ALCineLightRigManagerModel;
    for (U32 enabled_mask = 1; enabled_mask <= 0x1fu; ++enabled_mask)
    {
        const ALCineLightRigSlot focus = lowestEnabled(enabled_mask);
        U32 expected_suppressed = 0;
        for (S32 i = 0; i < static_cast<S32>(ALCineLightRigSlot::COUNT); ++i)
        {
            const U32 bit = 1u << i;
            if ((enabled_mask & bit) &&
                static_cast<ALCineLightRigSlot>(i) != focus)
            {
                expected_suppressed |= bit;
            }
        }

        const U32 suppressed = suppressedSlotMask(enabled_mask, focus);
        ensure_equals("production helper suppresses every enabled non-focus slot",
                      suppressed, expected_suppressed);
        ensure("focus slot is never suppressed",
               !(suppressed & slotToGroupBit(focus)));
        for (S32 i = 0; i < static_cast<S32>(ALCineLightRigSlot::COUNT); ++i)
        {
            const U32 bit = 1u << i;
            if (!(enabled_mask & bit))
            {
                ensure("disabled slots are absent from suppression", !(suppressed & bit));
            }
            else if (static_cast<ALCineLightRigSlot>(i) != focus)
            {
                ensure("each enabled non-focus slot is suppressed", suppressed & bit);
            }
        }

        S32 eligible_slots = 0;
        const U32 unsuppressed = enabled_mask & ~suppressed;
        for (S32 i = 0; i < static_cast<S32>(ALCineLightRigSlot::COUNT); ++i)
        {
            eligible_slots += (unsuppressed & (1u << i)) ? 1 : 0;
        }
        const S32 eligible_projectors = eligible_slots * LIGHT_COUNT;
        ensure_equals("enabled focus rig contributes exactly four projectors",
                      eligible_projectors, LIGHT_COUNT);
        ensure("rig candidates stay at or below four", eligible_projectors <= 4);
        ensure("rig candidates stay below the ten-slot pipeline cap",
               eligible_projectors <= 10);
    }
}

template<> template<>
void cine_light_rig_model_object::test<22>()
{
    set_test_name("single-enabled routing and selection normalization");
    using namespace ALCineLightRigManagerModel;
    for (S32 enabled = 0; enabled < 5; ++enabled)
    {
        const U32 mask = 1u << enabled;
        const ALCineLightRigSlot enabled_slot =
            static_cast<ALCineLightRigSlot>(enabled);
        ensure_equals("selected sole-enabled rig uses verbatim path",
            static_cast<S32>(pathFor(mask, enabled_slot, enabled_slot)),
            static_cast<S32>(TickPath::TICK_SELECTED_VERBATIM));
        for (S32 selected = 0; selected < 5; ++selected)
        {
            ensure_equals("sole-enabled load normalizes selection",
                static_cast<S32>(normalizeSelected(mask,
                    static_cast<ALCineLightRigSlot>(selected))),
                static_cast<S32>(enabled_slot));
        }
    }
    ensure_equals("selected disabled slot still runs its selected gate",
        static_cast<S32>(pathFor(ALCineLightRig::GROUP_SLOT_SELF,
            ALCineLightRigSlot::A, ALCineLightRigSlot::A)),
        static_cast<S32>(TickPath::TICK_SELECTED_VERBATIM));
    ensure_equals("the lone unselected enabled rig uses the blob path",
        static_cast<S32>(pathFor(ALCineLightRig::GROUP_SLOT_SELF,
            ALCineLightRigSlot::A, ALCineLightRigSlot::SELF)),
        static_cast<S32>(TickPath::TICK_FROM_BLOB));
}

template<> template<>
void cine_light_rig_model_object::test<23>()
{
    set_test_name("legacy anchor migration table");
    using namespace ALCineLightRigManagerModel;
    LLUUID subjects[4];
    subjects[0].set("aaaaaaaa-0000-0000-0000-000000000001", false);
    subjects[1].set("aaaaaaaa-0000-0000-0000-000000000002", false);
    subjects[2].set("aaaaaaaa-0000-0000-0000-000000000003", false);
    subjects[3].set("aaaaaaaa-0000-0000-0000-000000000004", false);

    MigrationSlot result = migrateAnchorToSlot(LLUUID::null, subjects);
    ensure_equals("null legacy anchor migrates to SELF",
                  static_cast<S32>(result.mSlot),
                  static_cast<S32>(ALCineLightRigSlot::SELF));
    ensure("null legacy anchor needs no compatibility carrier",
           !result.mKeepLegacyAnchor);
    for (S32 i = 0; i < 4; ++i)
    {
        result = migrateAnchorToSlot(subjects[i], subjects);
        ensure_equals("subject anchor migrates to its fixed slot",
                      static_cast<S32>(result.mSlot), i + 1);
        ensure("known subject anchor needs no compatibility carrier",
               !result.mKeepLegacyAnchor);
    }
    LLUUID unknown;
    unknown.set("bbbbbbbb-0000-0000-0000-000000000099", false);
    result = migrateAnchorToSlot(unknown, subjects);
    ensure_equals("unknown legacy anchor falls back to SELF",
                  static_cast<S32>(result.mSlot),
                  static_cast<S32>(ALCineLightRigSlot::SELF));
    ensure("unknown legacy anchor retains the compatibility carrier",
           result.mKeepLegacyAnchor);
}

template<> template<>
void cine_light_rig_model_object::test<24>()
{
    set_test_name("key-fill ratio lock derives only the fill EV");
    Setup setup = classicSetup();
    setup.mLights[0].mEV = 3.25f;
    setup.mLights[1].mEV = -6.25f;
    setup.mRatioLock = true;
    setup.mRatioStops = 1.75f;

    LightBase live[LIGHT_COUNT];
    computeLive(setup, Transforms(), live);
    ensure_equals("locked fill is key minus independent stop literal",
                  live[1].mEV, 1.5f);
    ensure_equals("ratio does not alter the key", live[0].mEV, 3.25f);

    setup.mLights[0].mEV = -20.f;
    setup.mRatioStops = 5.f;
    computeLive(setup, Transforms(), live);
    ensure_equals("minimum key and maximum ratio preserve the derived fill",
                  live[1].mEV, -25.f);
    live[1] = blendLight(live[1], live[1], 0.5f);
    ensure_equals("transition blend preserves the derived fill floor",
                  live[1].mEV, -25.f);
    RigFrame boundary_frame;
    Globals boundary_globals;
    render(setup.mRadius, live, boundary_globals, boundary_frame);
    ensure_equals("render chain preserves the below-input-floor fill EV",
                  boundary_frame.mProj[1].mIntensity,
                  intensityFromEV(-25.f, boundary_globals.mHeadroomStops,
                                  nullptr));

    setup.mRatioLock = false;
    setup.mLights[1].mEV = -6.25f;
    computeLive(setup, Transforms(), live);
    ensure_equals("unlocked fill retains its independent input",
                  live[1].mEV, -6.25f);
    ensure("unlocked fill EV is bitwise the input EV",
           std::memcmp(&live[1].mEV, &setup.mLights[1].mEV,
                       sizeof(F32)) == 0);
}

template<> template<>
void cine_light_rig_model_object::test<25>()
{
    set_test_name("gel zero is a bitwise no-op");
    F32 colour[3] = { 0.1234567f, -0.f, 0.9876543f };
    F32 original[3];
    std::memcpy(original, colour, sizeof(colour));
    applyGel(0, colour);
    ensure("none gel leaves every float bit untouched",
           std::memcmp(original, colour, sizeof(colour)) == 0);
}

template<> template<>
void cine_light_rig_model_object::test<26>()
{
    set_test_name("CT gel uses the master Planckian mired transform");
    ensure("half CTO is classified as colour temperature",
           gelIsColourTemperature(2));
    ensure_equals("half CTO owns the independent 70-mired shift",
                  gelMiredShift(2), 70.f);

    F32 colour[3] = { 0.25f, 0.5f, 0.75f };
    applyGel(2, colour);
    const F32 expected[3] = {
        0.32636893f, 0.5f, 0.48742402f,
    };
    for (S32 channel = 0; channel < 3; ++channel)
    {
        ensure("CT gel output matches independently frozen values",
               closeRelative(colour[channel], expected[channel], 2.e-6f));
    }

    F32 master_gain[3];
    masterTempGain(70.f, master_gain);
    const F32 expected_gain[3] = {
        1.3054757f, 1.f, 0.6498987f,
    };
    for (S32 channel = 0; channel < 3; ++channel)
    {
        ensure("master and gel share the independently pinned locus",
               closeRelative(master_gain[channel],
                             expected_gain[channel], 2.e-6f));
        ensure("gel equals that master gain on the known colour",
               closeRelative(colour[channel],
                   std::clamp(expected_gain[channel] *
                              (0.25f + channel * 0.25f), 0.f, 1.f),
                   2.e-6f));
    }
}

template<> template<>
void cine_light_rig_model_object::test<27>()
{
    set_test_name("party gel multiplier is the curated linear value");
    F32 white[3] = { 1.f, 1.f, 1.f };
    applyGel(9, white);
    ensure_equals("Bastard Amber red", white[0], 1.f);
    ensure_equals("Bastard Amber green", white[1], 0.55f);
    ensure_equals("Bastard Amber blue", white[2], 0.18f);
}

template<> template<>
void cine_light_rig_model_object::test<28>()
{
    set_test_name("catchlight radial placement composes with subject scale");
    F32 nominal[2];
    F32 triple[2];
    F32 upper[2];
    catchlightRadialOffset(1.f, 0.f, nominal);
    catchlightRadialOffset(3.f, 0.f, triple);
    catchlightRadialOffset(1.f, 90.f, upper);
    ensure("nominal right offset is independently fixed at ten cm",
           closeRelative(nominal[0], 0.1f, 1.e-6f));
    ensure("zero-degree nominal up offset is zero",
           std::fabs(nominal[1]) <= 1.e-6f);
    ensure("three-times subject scales the right offset to thirty cm",
           closeRelative(triple[0], 0.3f, 1.e-6f));
    ensure("ninety-degree offset has no right component",
           std::fabs(upper[0]) <= 1.e-6f);
    ensure("ninety-degree offset lands ten cm upward",
           closeRelative(upper[1], 0.1f, 1.e-6f));
}

template<> template<>
void cine_light_rig_model_object::test<29>()
{
    set_test_name("projector-shadow hardware tiers and runtime clamp");
    struct TierCase
    {
        U32 mUnits;
        U32 mExpected;
    };
    const TierCase tiers[] = {
        {16u, 6u}, {27u, 6u}, {28u, 8u},
        {31u, 8u}, {32u, 10u}, {64u, 10u},
    };
    for (const TierCase& item : tiers)
    {
        ensure_equals("texture-unit tier has an independently fixed ceiling",
            LLPipeline::maxSpotShadowsForTextureUnits(item.mUnits),
            item.mExpected);
    }
    ensure_equals("requests below the supported minimum clamp to two",
        LLPipeline::clampSpotShadowCount(0u, 32u), 2u);
    ensure_equals("legacy request remains unchanged on constrained hardware",
        LLPipeline::clampSpotShadowCount(5u, 16u), 5u);
    ensure_equals("31-unit hardware caps a ten-slot request at eight",
        LLPipeline::clampSpotShadowCount(10u, 31u), 8u);
    ensure_equals("32-unit hardware admits the ten-slot request",
        LLPipeline::clampSpotShadowCount(10u, 32u), 10u);
    ensure_equals("oversized requests clamp to the compile-time ceiling",
        LLPipeline::clampSpotShadowCount(99u, 64u), 10u);
}

template<> template<>
void cine_light_rig_model_object::test<30>()
{
    set_test_name("sun and spot slots map to samplers and collision-free camera ids");
    const U32 expected_sun_maps[] = { 0u, 1u, 2u, 3u };
    const S32 sun_sampler_uniforms[] = {
        LLShaderMgr::DEFERRED_SHADOW0, LLShaderMgr::DEFERRED_SHADOW1,
        LLShaderMgr::DEFERRED_SHADOW2, LLShaderMgr::DEFERRED_SHADOW3,
    };
    const U32 expected_spot_maps[] = {
        4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u,
    };
    const S32 spot_sampler_uniforms[] = {
        LLShaderMgr::DEFERRED_SHADOW4, LLShaderMgr::DEFERRED_SHADOW5,
        LLShaderMgr::DEFERRED_SHADOW6, LLShaderMgr::DEFERRED_SHADOW7,
        LLShaderMgr::DEFERRED_SHADOW8, LLShaderMgr::DEFERRED_SHADOW9,
        LLShaderMgr::DEFERRED_SHADOW10, LLShaderMgr::DEFERRED_SHADOW11,
        LLShaderMgr::DEFERRED_SHADOW12, LLShaderMgr::DEFERRED_SHADOW13,
    };
    const S32 camera_ids[] = {
        LLViewerCamera::CAMERA_SPOT_SHADOW0,
        LLViewerCamera::CAMERA_SPOT_SHADOW1,
        LLViewerCamera::CAMERA_SPOT_SHADOW2,
        LLViewerCamera::CAMERA_SPOT_SHADOW3,
        LLViewerCamera::CAMERA_SPOT_SHADOW4,
        LLViewerCamera::CAMERA_SPOT_SHADOW5,
        LLViewerCamera::CAMERA_SPOT_SHADOW6,
        LLViewerCamera::CAMERA_SPOT_SHADOW7,
        LLViewerCamera::CAMERA_SPOT_SHADOW8,
        LLViewerCamera::CAMERA_SPOT_SHADOW9,
    };
    static_assert(LL_ARRAY_SIZE(expected_sun_maps) == 4u,
                  "sun map literals must cover all four sun slots");
    static_assert(LL_ARRAY_SIZE(sun_sampler_uniforms) == 4u,
                  "sun sampler literals must cover all four sun slots");
    static_assert(LL_ARRAY_SIZE(expected_spot_maps) ==
                      LLPipeline::MAX_SPOT_SHADOWS,
                  "spot dispatch literals must cover every spot slot");
    static_assert(LL_ARRAY_SIZE(spot_sampler_uniforms) ==
                      LLPipeline::MAX_SPOT_SHADOWS,
                  "spot sampler literals must cover every spot slot");
    static_assert(LL_ARRAY_SIZE(camera_ids) == LLPipeline::MAX_SPOT_SHADOWS,
                  "camera literals must cover every spot slot");

    for (U32 slot = 0; slot < LL_ARRAY_SIZE(expected_sun_maps); ++slot)
    {
        ensure_equals("sun slot selects the independently enumerated shadow map",
            sun_sampler_uniforms[slot],
            static_cast<S32>(LLShaderMgr::DEFERRED_SHADOW0 +
                             expected_sun_maps[slot]));
    }
    for (U32 slot = 0; slot < LLPipeline::MAX_SPOT_SHADOWS; ++slot)
    {
        ensure_equals("slot selects the independently enumerated shadow map",
            LLPipeline::spotShadowMapIndex(slot), expected_spot_maps[slot]);
        ensure_equals("reserved sampler uniforms remain contiguous",
            spot_sampler_uniforms[slot],
            static_cast<S32>(LLShaderMgr::DEFERRED_SHADOW0 +
                             expected_spot_maps[slot]));
        ensure_equals("spot camera ids remain contiguous",
            camera_ids[slot],
            static_cast<S32>(LLViewerCamera::CAMERA_SPOT_SHADOW0 + slot));
    }
    ensure_equals("water cameras begin after all ten spot cameras",
        static_cast<S32>(LLViewerCamera::CAMERA_WATER0),
        static_cast<S32>(LLViewerCamera::CAMERA_SPOT_SHADOW0 + 10));
}

template<> template<>
void cine_light_rig_model_object::test<31>()
{
    set_test_name("requested shadow slots sum across all rig instances");
    using namespace ALCineLightRigManagerModel;
    ALCineLightRigParamBlob blobs[5];

    blobs[0].mEnabled = true;
    blobs[0].mPower = true;
    blobs[0].mShadowMode = 1;
    blobs[0].mLights[0].mOn = true;

    blobs[1].mEnabled = true;
    blobs[1].mPower = true;
    blobs[1].mShadowMode = 2;
    for (S32 light = 0; light < LIGHT_COUNT; ++light)
    {
        blobs[1].mLights[light].mOn = true;
    }

    blobs[2].mEnabled = true;
    blobs[2].mPower = true;
    blobs[2].mShadowMode = 2;
    const bool third_mask[LIGHT_COUNT] = { true, false, true, true };
    for (S32 light = 0; light < LIGHT_COUNT; ++light)
    {
        blobs[2].mLights[light].mOn = third_mask[light];
    }

    blobs[3].mEnabled = false;
    blobs[3].mPower = true;
    blobs[3].mShadowMode = 2;
    blobs[4].mEnabled = true;
    blobs[4].mPower = false;
    blobs[4].mShadowMode = 2;

    ensure_equals("legacy power flags cannot form a second master gate",
        requestedShadowSlots(blobs, 5, 10u), 10u);
    blobs[4].mPower = true;
    ensure_equals("the five-instance total caps at the ten-slot ceiling",
        requestedShadowSlots(blobs, 5, 10u), 10u);
    blobs[0].mShadowMode = 0;
    blobs[1].mEnabled = false;
    blobs[2].mEnabled = false;
    blobs[4].mEnabled = false;
    ensure_equals("none and master-disabled rigs are fully dark",
        requestedShadowSlots(blobs, 5, 10u), 0u);
}

template<> template<>
void cine_light_rig_model_object::test<32>()
{
    set_test_name("auto shadow slots raise only and restore guarded baseline");
    using namespace ALCineLightRigManagerModel;
    AutoShadowSlotState state;

    AutoShadowSlotUpdate update = updateAutoShadowSlots(
        state, true, 4u, 10u, 2u);
    ensure("first request raises the setting", update.mWrite);
    ensure_equals("first request writes four", update.mValue, 4u);
    ensure_equals("first raise captures the independent baseline",
                  state.mBaseline, 2u);

    update = updateAutoShadowSlots(state, true, 8u, 10u, 4u);
    ensure("larger request raises again", update.mWrite);
    ensure_equals("larger request writes eight", update.mValue, 8u);
    ensure_equals("later raises preserve the original baseline",
                  state.mBaseline, 2u);

    update = updateAutoShadowSlots(state, true, 3u, 10u, 8u);
    ensure("smaller live request never lowers the setting", !update.mWrite);
    ensure("raise ownership survives a smaller request", state.mRaised);

    update = updateAutoShadowSlots(state, true, 0u, 10u, 8u);
    ensure("fully dark rigs restore an unchanged auto value", update.mWrite);
    ensure_equals("dark restore returns to the pre-raise baseline",
                  update.mValue, 2u);
    ensure("restore releases ownership", !state.mRaised);

    update = updateAutoShadowSlots(state, true, 9u, 8u, 2u);
    ensure("hardware cap still permits a bounded raise", update.mWrite);
    ensure_equals("hardware cap limits the auto write to eight",
                  update.mValue, 8u);
    update = updateAutoShadowSlots(state, true, 0u, 8u, 7u);
    ensure("an intervening external write blocks baseline restore",
           !update.mWrite);
    ensure_equals("external value is left untouched", update.mValue, 7u);
    ensure("external write relinquishes ownership", !state.mRaised);

    AutoShadowSlotState manual_state;
    update = updateAutoShadowSlots(manual_state, true, 8u, 10u, 4u);
    ensure("auto owns its eight-slot raise", update.mWrite &&
           manual_state.mRaised);
    ensure("manual eight-to-ten write relinquishes ownership while lit",
           relinquishAutoShadowSlotsIfExternallyChanged(manual_state, 10u));
    ensure("manual write leaves no auto ownership", !manual_state.mRaised);
    ensure_equals("manual write drops the captured baseline",
                  manual_state.mBaseline, 0u);
    update = updateAutoShadowSlots(manual_state, true, 8u, 10u, 8u);
    ensure("manual return to eight does not reacquire ownership",
           !update.mWrite && !manual_state.mRaised);
    update = updateAutoShadowSlots(manual_state, true, 0u, 10u, 8u);
    ensure("dark rig does not restore the relinquished baseline",
           !update.mWrite);
    ensure_equals("manual eight remains after the rig goes dark",
                  update.mValue, 8u);

    update = updateAutoShadowSlots(state, true, 4u, 10u, 6u);
    ensure("auto mode never lowers a higher user value", !update.mWrite);
    ensure("no raise means no baseline ownership", !state.mRaised);

    update = updateAutoShadowSlots(state, true, 5u, 10u, 2u);
    ensure("a fresh auto request reacquires raise ownership", update.mWrite);
    update = updateAutoShadowSlots(state, false, 5u, 10u, 5u);
    ensure("turning auto mode off restores its unchanged value",
           update.mWrite);
    ensure_equals("auto-off restore returns to the captured baseline",
                  update.mValue, 2u);
}

template<> template<>
void cine_light_rig_model_object::test<33>()
{
    set_test_name("Easy Mode four-way presence helpers round-trip");
    for (S32 presence = 0; presence < 4; ++presence)
    {
        ensure_equals("rim write and read recover the same presence",
            rimPresenceFromEV(easyRimOn(presence), easyRimEV(presence)),
            presence);
        ensure_equals("background write and read recover the same presence",
            bgPresenceFromEV(easyBgOn(presence), easyBgEV(presence)),
            presence);
    }
}

template<> template<>
void cine_light_rig_model_object::test<34>()
{
    set_test_name("Easy Mode presence thresholds and dial clamps");
    ensure_equals("rim off maps to Off", rimPresenceFromEV(false, 20.f), 0);
    ensure_equals("rim -2.5 maps to Faint", rimPresenceFromEV(true, -2.5f), 1);
    ensure_equals("rim below first midpoint stays Faint",
                  rimPresenceFromEV(true, -1.751f), 1);
    ensure_equals("rim first midpoint selects Subtle",
                  rimPresenceFromEV(true, -1.75f), 2);
    ensure_equals("rim -1 maps to Subtle", rimPresenceFromEV(true, -1.f), 2);
    ensure_equals("rim second midpoint selects Strong",
                  rimPresenceFromEV(true, -0.25f), 3);
    ensure_equals("rim +0.5 maps to Strong", rimPresenceFromEV(true, 0.5f), 3);

    ensure_equals("background off maps to Off",
                  bgPresenceFromEV(false, 20.f), 0);
    ensure_equals("background -3.5 maps to Faint",
                  bgPresenceFromEV(true, -3.5f), 1);
    ensure_equals("background below first midpoint stays Faint",
                  bgPresenceFromEV(true, -2.751f), 1);
    ensure_equals("background first midpoint selects Subtle",
                  bgPresenceFromEV(true, -2.75f), 2);
    ensure_equals("background -2 maps to Subtle",
                  bgPresenceFromEV(true, -2.f), 2);
    ensure_equals("background second midpoint selects Strong",
                  bgPresenceFromEV(true, -1.25f), 3);
    ensure_equals("background -0.5 maps to Strong",
                  bgPresenceFromEV(true, -0.5f), 3);

    ensure_equals("brightness clamps below -16",
                  easyBrightnessClamp(-20.f), -16.f);
    ensure_equals("brightness preserves an in-range value",
                  easyBrightnessClamp(3.25f), 3.25f);
    ensure_equals("brightness clamps above +16",
                  easyBrightnessClamp(20.f), 16.f);
    ensure_equals("drama clamps below zero", easyDramaClamp(-1.f), 0.f);
    ensure_equals("drama preserves an in-range value",
                  easyDramaClamp(2.75f), 2.75f);
    ensure_equals("drama clamps above five", easyDramaClamp(8.f), 5.f);
}

template<> template<>
void cine_light_rig_model_object::test<35>()
{
    set_test_name("Easy Brightness and Warmth remain per instance");
    FakeRigSettings settings;
    ALCineLightRigParamBlob instance_a = distinctiveBlob();
    instance_a.mMasterEV = -1.25f;
    instance_a.mMasterTempMired = 35.f;
    seedSettingsIndependently(settings, instance_a);
    instance_a = ALCineLightRigParamBlob::fromSettingsStore(settings);

    ALCineLightRigParamBlob instance_b = instance_a;
    instance_b.mMasterEV = 4.5f;
    instance_b.mMasterTempMired = -70.f;
    instance_b.toSettingsStore(settings);
    instance_b = ALCineLightRigParamBlob::fromSettingsStore(settings);

    instance_a.toSettingsStore(settings);
    ensure_equals("returning to A restores A Brightness",
                  settings.getF32("CineLightRigMasterEV"), -1.25f);
    ensure_equals("returning to A restores A Warmth",
                  settings.getF32("CineLightRigMasterTempMired"), 35.f);

    instance_b.toSettingsStore(settings);
    ensure_equals("returning to B restores B Brightness",
                  settings.getF32("CineLightRigMasterEV"), 4.5f);
    ensure_equals("returning to B restores B Warmth",
                  settings.getF32("CineLightRigMasterTempMired"), -70.f);
}

template<> template<>
void cine_light_rig_model_object::test<36>()
{
    set_test_name("object bounding radius maps to subject scale and clamps");
    ensure_equals("two-metre object maps to avatar scale",
                  objectRadiusToSubjectScale(2.f * 0.5f), 1.f);
    ensure_equals("eight-metre object maps to four avatar scales",
                  objectRadiusToSubjectScale(8.f * 0.5f), 4.f);
    ensure_equals("zero radius clamps to minimum subject scale",
                  objectRadiusToSubjectScale(0.f), SUBJECT_SCALE_MIN);
    ensure_equals("small radius clamps to minimum subject scale",
                  objectRadiusToSubjectScale(SUBJECT_SCALE_MIN * 0.5f),
                  SUBJECT_SCALE_MIN);
    ensure_equals("large radius clamps to maximum subject scale",
                  objectRadiusToSubjectScale(SUBJECT_SCALE_MAX * 2.f),
                  SUBJECT_SCALE_MAX);
}

template<> template<>
void cine_light_rig_model_object::test<37>()
{
    set_test_name("object target UUID round-trips additively");
    ALCineLightRigParamBlob expected;
    expected.mObjectTarget.set(
        "01234567-89ab-cdef-fedc-ba9876543210", false);

    const LLSD data = expected.toLLSD();
    ensure("object target is stored as an LLSD UUID",
           data["CineLightRigObjectTarget"].isUUID());
    ensure_equals("LLSD object target round-trip",
        ALCineLightRigParamBlob::fromLLSD(data).mObjectTarget,
        expected.mObjectTarget);

    FakeRigSettings settings;
    expected.toSettingsStore(settings);
    ensure_equals("settings object target round-trip",
        ALCineLightRigParamBlob::fromSettingsStore(settings).mObjectTarget,
        expected.mObjectTarget);

    LLSD legacy = data;
    legacy.erase("CineLightRigObjectTarget");
    ensure("missing legacy object target defaults to avatar behavior",
        ALCineLightRigParamBlob::fromLLSD(legacy).mObjectTarget.isNull());
}

template<> template<>
void cine_light_rig_model_object::test<38>()
{
    set_test_name("practical flicker is deterministic and light-decorrelated");
    const U64 rig_seed = 0x123456789abcdef0ULL;
    for (S32 program = FLICKER_FIRELIGHT;
         program < FLICKER_COUNT; ++program)
    {
        for (S32 light = 0; light < LIGHT_COUNT; ++light)
        {
            const U64 seed = flickerLightSeed(rig_seed, light);
            const F64 times[] = { 0.0, 0.137, 1.0, 3.875, 47.25, 1.0e7 };
            for (F64 time : times)
            {
                F32 first_intensity = 0.f;
                F32 second_intensity = 0.f;
                F32 first_color[3] = {};
                F32 second_color[3] = {};
                evalFlicker(program, seed, time, 0.73f,
                            first_intensity, first_color);
                evalFlicker(program, seed, time, 0.73f,
                            second_intensity, second_color);
                ensure_equals("same seed/time has identical intensity",
                              first_intensity, second_intensity);
                ensure("same seed/time has identical color",
                    std::memcmp(first_color, second_color,
                                sizeof(first_color)) == 0);
            }
        }
    }

    for (S32 program = FLICKER_FIRELIGHT;
         program < FLICKER_COUNT; ++program)
    {
        for (S32 first = 0; first < LIGHT_COUNT; ++first)
        {
            for (S32 second = first + 1; second < LIGHT_COUNT; ++second)
            {
                const U64 first_seed = flickerLightSeed(rig_seed, first);
                const U64 second_seed = flickerLightSeed(rig_seed, second);
                ensure("per-light seed derivation is unique",
                       first_seed != second_seed);
                bool separated = false;
                for (S32 sample = 0; sample < 128 && !separated; ++sample)
                {
                    F32 first_intensity = 0.f;
                    F32 second_intensity = 0.f;
                    F32 first_color[3] = {};
                    F32 second_color[3] = {};
                    const F64 time = sample * 0.071 + 0.013;
                    evalFlicker(program, first_seed, time, 1.f,
                                first_intensity, first_color);
                    evalFlicker(program, second_seed, time, 1.f,
                                second_intensity, second_color);
                    separated = first_intensity != second_intensity ||
                        std::memcmp(first_color, second_color,
                                    sizeof(first_color)) != 0;
                }
                ensure("different light indices decorrelate every program",
                       separated);
            }
        }
    }
}

template<> template<>
void cine_light_rig_model_object::test<39>()
{
    set_test_name("practical flicker outputs are finite, bounded, and inert at zero");
    const U64 rig_seed = 0xfeedface12345678ULL;
    for (S32 program = FLICKER_FIRELIGHT;
         program < FLICKER_COUNT; ++program)
    {
        for (S32 light = 0; light < LIGHT_COUNT; ++light)
        {
            for (S32 sample = 0; sample < 512; ++sample)
            {
                F32 intensity = 0.f;
                F32 color[3] = {};
                evalFlicker(program, flickerLightSeed(rig_seed, light),
                            sample * 0.137, 1.f, intensity, color);
                ensure("flicker intensity is finite", std::isfinite(intensity));
                ensure("flicker intensity is bounded",
                    intensity >= 0.f &&
                    intensity <= FLICKER_INTENSITY_MUL_MAX);
                for (F32 channel : color)
                {
                    ensure("flicker color is finite", std::isfinite(channel));
                    ensure("flicker color is bounded",
                        channel >= 0.f && channel <= FLICKER_COLOR_MUL_MAX);
                }
            }

            F32 intensity = 0.f;
            F32 color[3] = {};
            evalFlicker(program, flickerLightSeed(rig_seed, light),
                        12.5, 0.f, intensity, color);
            ensure_equals("amount zero preserves intensity", intensity, 1.f);
            ensure("amount zero preserves color",
                   color[0] == 1.f && color[1] == 1.f && color[2] == 1.f);
        }
    }

    F32 intensity = 0.f;
    F32 color[3] = {};
    evalFlicker(FLICKER_NONE, rig_seed, 42.0, 1.f, intensity, color);
    ensure_equals("None preserves intensity", intensity, 1.f);
    ensure("None preserves color",
           color[0] == 1.f && color[1] == 1.f && color[2] == 1.f);
    evalFlicker(FLICKER_FIRELIGHT, rig_seed,
                std::numeric_limits<F64>::quiet_NaN(),
                std::numeric_limits<F32>::quiet_NaN(), intensity, color);
    ensure_equals("non-finite amount is inert", intensity, 1.f);
    ensure("non-finite amount preserves color",
           color[0] == 1.f && color[1] == 1.f && color[2] == 1.f);
}

template<> template<>
void cine_light_rig_model_object::test<40>()
{
    set_test_name("fixture white uses additive mired CT and multiplicative tint gels");
    const S32 none[FIXTURE_GEL_SLOT_COUNT] = { 0, 0, 0 };
    const S32 full_ctb[FIXTURE_GEL_SLOT_COUNT] = { 5, 0, 0 };
    F32 corrected[3] = {};
    F32 reference[3] = {};
    fixtureWhite(3200.f, full_ctb, 0.f, corrected);
    fixtureWhite(1.0e6f / (1.0e6f / 3200.f - 137.f),
                 none, 0.f, reference);
    for (S32 channel = 0; channel < 3; ++channel)
    {
        ensure_approximately_equals_range(
            "Full CTB matches the equivalent effective Kelvin",
            corrected[channel], reference[channel], 1e-5f);
    }

    const S32 stacked_cto[FIXTURE_GEL_SLOT_COUNT] = { 3, 4, 0 };
    fixtureWhite(5600.f, stacked_cto, 0.f, corrected);
    fixtureWhite(1.0e6f / (1.0e6f / 5600.f + 64.f + 30.f),
                 none, 0.f, reference);
    for (S32 channel = 0; channel < 3; ++channel)
    {
        ensure_approximately_equals_range(
            "CT gel slots add in reciprocal color temperature",
            corrected[channel], reference[channel], 1e-5f);
    }

    const S32 tint_forward[FIXTURE_GEL_SLOT_COUNT] = { 9, 11, 13 };
    const S32 tint_reverse[FIXTURE_GEL_SLOT_COUNT] = { 13, 11, 9 };
    fixtureWhite(4300.f, tint_forward, 0.f, corrected);
    fixtureWhite(4300.f, tint_reverse, 0.f, reference);
    for (S32 channel = 0; channel < 3; ++channel)
    {
        ensure_equals("tint gel order is immaterial",
                      corrected[channel], reference[channel]);
        ensure("fixture color is finite and renderer-bounded",
               std::isfinite(corrected[channel]) &&
               corrected[channel] >= 0.f && corrected[channel] <= 1.f);
    }
}

template<> template<>
void cine_light_rig_model_object::test<41>()
{
    set_test_name("modeled source angular size maps monotonically to softness");
    const F32 practical = penumbraSoftness(0.05f, 2.f);
    const F32 fresnel = penumbraSoftness(0.12f, 2.f);
    const F32 hmi = penumbraSoftness(0.20f, 2.f);
    const F32 china = penumbraSoftness(0.60f, 2.f);
    const F32 octa = penumbraSoftness(1.50f, 2.f);
    const F32 book = penumbraSoftness(2.40f, 2.f);
    ensure_approximately_equals_range(
        "practical example follows the implemented formula",
        practical, 1.0f, 0.02f);
    ensure("larger sources get progressively softer",
           practical < fresnel && fresnel < hmi && hmi < china &&
           china < octa && octa < book);
    ensure_approximately_equals_range(
        "book light reaches the softness ceiling", book, 8.f, 1e-6f);
    ensure("pulling a source away hardens its shadow",
           penumbraSoftness(1.50f, 4.f) < octa);
    ensure("invalid and extreme inputs remain bounded",
           penumbraSoftness(std::numeric_limits<F32>::quiet_NaN(),
                            -100.f) >= 0.f &&
           penumbraSoftness(1000.f, 0.f) <= 8.f);
}

template<> template<>
void cine_light_rig_model_object::test<42>()
{
    set_test_name("fixture rendering is additive and preserves the legacy-off path");
    Setup setup = classicSetup();
    LightBase baseline_live[LIGHT_COUNT];
    computeLive(setup, Transforms(), baseline_live);
    Globals globals;
    globals.mBounceRatio = 0.4f;
    RigFrame baseline;
    render(setup.mRadius, baseline_live, globals, baseline);

    LightBase legacy_live[LIGHT_COUNT];
    std::memcpy(legacy_live, baseline_live, sizeof(legacy_live));
    legacy_live[0].mKelvin = 2000.f;
    legacy_live[0].mGelSlot[0] = 15;
    legacy_live[0].mSourceSizeM = 4.f;
    legacy_live[0].mFixturePreset = FIXTURE_PRESET_COUNT - 1;
    RigFrame legacy;
    render(setup.mRadius, legacy_live, globals, legacy);
    ensure("fixture-only fields are inert while mode is off",
           std::memcmp(&baseline, &legacy, sizeof(RigFrame)) == 0);

    legacy_live[0].mFixtureMode = true;
    legacy_live[0].mKelvin = 3200.f;
    legacy_live[0].mGelSlot[0] = 0;
    legacy_live[0].mSourceSizeM = 1.5f;
    RigFrame fixture;
    render(setup.mRadius, legacy_live, globals, fixture);
    F32 fixture_linear[3] = {};
    fixtureWhite(legacy_live[0].mKelvin, legacy_live[0].mGelSlot,
                 globals.mMasterTempMired, fixture_linear);
    const auto to_srgb = [](F32 value)
    {
        return value <= 0.0031308f
            ? value * 12.92f
            : 1.055f * std::pow(value, 1.f / 2.4f) - 0.055f;
    };
    ensure_approximately_equals_range(
        "linear fixture white is converted to the emitter sRGB contract",
        fixture.mProj[0].mSB, to_srgb(fixture_linear[2]), 1e-6f);
    ensure_approximately_equals_range(
        "render exposes the derived projector softness",
        fixture.mDerivedShadowSoftness[0],
        penumbraSoftness(1.5f, setup.mRadius), 1e-6f);
    ensure("soft fixture feathers falloff and FOV",
           fixture.mProj[0].mFalloff != baseline.mProj[0].mFalloff &&
           fixture.mProj[0].mFovRad > baseline.mProj[0].mFovRad);
    ensure("soft fixture increases only its bounce feed",
           fixture.mOmni[0].mIntensity > baseline.mOmni[0].mIntensity);
    ensure("key fixture scales the single catchlight",
           fixture.mCatchlightSizeScale > 1.f);
}

template<> template<>
void cine_light_rig_model_object::test<43>()
{
    set_test_name("fixture transitions interpolate Kelvin in mired space");
    LightBase start;
    start.mFixtureMode = true;
    start.mKelvin = 3200.f;
    start.mSourceSizeM = 0.1f;
    start.mGelSlot[0] = 1;
    LightBase target = start;
    target.mKelvin = 5600.f;
    target.mSourceSizeM = 1.5f;
    target.mGelSlot[0] = 6;
    target.mFixturePreset = 8;

    const LightBase halfway = blendLight(start, target, 0.5f);
    const F32 expected_kelvin = 1.0e6f /
        ((1.0e6f / 3200.f + 1.0e6f / 5600.f) * 0.5f);
    ensure_approximately_equals_range(
        "half fade is the reciprocal-temperature midpoint",
        halfway.mKelvin, expected_kelvin, 1e-3f);
    ensure_approximately_equals_range(
        "source diameter fades continuously",
        halfway.mSourceSizeM, 0.8f, 1e-6f);
    ensure_equals("discrete gel keeps the start at the midpoint",
                  halfway.mGelSlot[0], 1);
    ensure_equals("discrete gel snaps after the midpoint",
                  blendLight(start, target, 0.51f).mGelSlot[0], 6);

    for (S32 index = 0; index < FIXTURE_PRESET_COUNT; ++index)
    {
        const FixturePreset& preset = fixturePreset(index);
        ensure("fixture preset has a name",
               preset.mName && preset.mName[0] != '\0');
        ensure("fixture preset Kelvin is in range",
               preset.mKelvin >= FIXTURE_KELVIN_MIN &&
               preset.mKelvin <= FIXTURE_KELVIN_MAX);
        ensure("fixture preset source is in range",
               preset.mSourceSizeM >= FIXTURE_SOURCE_SIZE_MIN &&
               preset.mSourceSizeM <= FIXTURE_SOURCE_SIZE_MAX);
    }

    Setup dirty;
    dirty.mLights[0].mFixtureMode = true;
    dirty.mLights[0].mKelvin = std::numeric_limits<F32>::quiet_NaN();
    dirty.mLights[0].mGelSlot[0] = -100;
    dirty.mLights[0].mGelSlot[1] = FIXTURE_GEL_COUNT + 100;
    dirty.mLights[0].mSourceSizeM =
        std::numeric_limits<F32>::infinity();
    dirty.mLights[0].mFixturePreset = FIXTURE_PRESET_COUNT + 100;
    const Setup clean = sanitizeSetup(dirty);
    ensure_equals("non-finite Kelvin falls back safely",
                  clean.mLights[0].mKelvin, 5600.f);
    ensure_equals("low gel index clamps",
                  clean.mLights[0].mGelSlot[0], 0);
    ensure_equals("high gel index clamps",
                  clean.mLights[0].mGelSlot[1], FIXTURE_GEL_COUNT - 1);
    ensure_equals("non-finite source size falls back safely",
                  clean.mLights[0].mSourceSizeM, 0.10f);
    ensure_equals("fixture provenance clamps",
                  clean.mLights[0].mFixturePreset,
                  FIXTURE_PRESET_COUNT - 1);
}

template<> template<>
void cine_light_rig_model_object::test<44>()
{
    set_test_name("gobo library categories blur buckets and animation contract");

    ensure_equals("gobo category count", GOBO_CATEGORY_COUNT, 5);
    const S32 expected_categories[GOBO_COUNT] = {
        0, 1, 1, 1, 1, 4, 2, 2,
        1, 1, 1, 1, 1, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4,
    };
    for (S32 i = 0; i < GOBO_COUNT; ++i)
    {
        ensure_equals("gobo category golden", goboCategory(i),
                      expected_categories[i]);
        ensure_equals("only rotating fan is animated", goboIsAnimated(i),
                      i == GOBO_ROTATING_FAN);
    }
    ensure_equals("category low clamp",
                  std::string(goboCategoryName(-99)),
                  std::string("User / Default"));
    ensure_equals("category high clamp",
                  std::string(goboCategoryName(99)),
                  std::string("Graphic / Hard"));

    ensure_equals("negative softness selects sharp", goboBlurBucket(-1.f), 0);
    ensure_equals("below first edge selects sharp", goboBlurBucket(2.499f), 0);
    ensure_equals("first edge selects medium", goboBlurBucket(2.5f), 1);
    ensure_equals("below second edge selects medium", goboBlurBucket(5.499f), 1);
    ensure_equals("second edge selects heavy", goboBlurBucket(5.5f), 2);
    ensure_equals("upper clamp selects heavy", goboBlurBucket(99.f), 2);
    ensure_equals("NaN has deterministic sharp fallback", goboBlurBucket(
        std::numeric_limits<F32>::quiet_NaN()), 0);
}

template<> template<>
void cine_light_rig_model_object::test<45>()
{
    set_test_name("cue fade profiles honor delay and presentation time");

    ensure_equals("linear remains at start before delay",
        cueFadeWeight(CUE_FADE_LINEAR, 11.999, 10.0, 2.f, 4.f), 0.f);
    ensure_equals("linear starts exactly after delay",
        cueFadeWeight(CUE_FADE_LINEAR, 12.0, 10.0, 2.f, 4.f), 0.f);
    ensure_approximately_equals_range("linear midpoint",
        cueFadeWeight(CUE_FADE_LINEAR, 14.0, 10.0, 2.f, 4.f), 0.5f, 1e-6f);
    ensure_equals("linear reaches exact target",
        cueFadeWeight(CUE_FADE_LINEAR, 16.0, 10.0, 2.f, 4.f), 1.f);
    ensure_approximately_equals_range("ease uses smooth console curve",
        cueFadeWeight(CUE_FADE_EASE, 13.0, 10.0, 2.f, 4.f),
        0.15625f, 1e-6f);
    ensure_equals("snap waits for delay",
        cueFadeWeight(CUE_FADE_SNAP, 11.999, 10.0, 2.f, 99.f), 0.f);
    ensure_equals("snap lands at delay boundary",
        cueFadeWeight(CUE_FADE_SNAP, 12.0, 10.0, 2.f, 99.f), 1.f);
    ensure_equals("zero-duration ease is an exact snap",
        cueFadeWeight(CUE_FADE_EASE, 10.0, 10.0, 0.f, 0.f), 1.f);
}

template<> template<>
void cine_light_rig_model_object::test<46>()
{
    set_test_name("cue transition fades the complete fixture and global state");

    CueState start;
    start.mSetup.mRadius = 1.f;
    start.mSetup.mLights[0].mOn = true;
    start.mSetup.mLights[0].mEV = -2.f;
    start.mSetup.mLights[0].mGobo = 1;
    start.mSetup.mLights[0].mFixtureMode = true;
    start.mSetup.mLights[0].mKelvin = 3200.f;
    start.mSetup.mLights[0].mSourceSizeM = 0.1f;
    start.mGlobals.mMasterEV = -1.f;
    start.mGlobals.mMasterTempMired = -80.f;
    start.mGlobals.mBounceRatio = 0.2f;
    start.mTransforms.mYawDeg = 170.f;
    start.mShadowSoftOverride[0] = 1.f;
    start.mShadowSoftOverride[1] = 3.f;
    start.mFX = 2;

    Cue target;
    target.mFadeSec = 4.f;
    target.mDelaySec = 2.f;
    target.mProfile = CUE_FADE_LINEAR;
    target.mSetup = start.mSetup;
    target.mSetup.mRadius = 3.f;
    target.mSetup.mLights[0].mEV = 2.f;
    target.mSetup.mLights[0].mGobo = GOBO_ROTATING_FAN;
    target.mSetup.mLights[0].mKelvin = 5600.f;
    target.mSetup.mLights[0].mSourceSizeM = 1.5f;
    target.mGlobals = start.mGlobals;
    target.mGlobals.mMasterEV = 1.f;
    target.mGlobals.mMasterTempMired = 120.f;
    target.mGlobals.mBounceRatio = 0.8f;
    target.mTransforms = start.mTransforms;
    target.mTransforms.mYawDeg = -170.f;
    target.mShadowSoftOverride[0] = 7.f;
    target.mShadowSoftOverride[1] = -1.f;
    target.mFX = 9;

    const CueState before_arm = evaluateCueTransition(start, target, 11.0, 10.0);
    ensure_equals("delay preserves prior FX", before_arm.mFX, 2);
    ensure_equals("delay preserves prior EV",
                  before_arm.mSetup.mLights[0].mEV, -2.f);

    const CueState halfway = evaluateCueTransition(start, target, 14.0, 10.0);
    ensure_approximately_equals_range("radius fades", halfway.mSetup.mRadius,
                                      2.f, 1e-6f);
    ensure_approximately_equals_range("fixture EV fades",
        halfway.mSetup.mLights[0].mEV, 0.f, 1e-6f);
    const F32 expected_kelvin = 1.0e6f /
        ((1.0e6f / 3200.f + 1.0e6f / 5600.f) * 0.5f);
    ensure_approximately_equals_range("cue Kelvin fades in mired space",
        halfway.mSetup.mLights[0].mKelvin, expected_kelvin, 1e-3f);
    ensure_approximately_equals_range("cue source diameter fades",
        halfway.mSetup.mLights[0].mSourceSizeM, 0.8f, 1e-6f);
    ensure_equals("gobo is still start value at exact midpoint",
                  halfway.mSetup.mLights[0].mGobo, 1);
    ensure_approximately_equals_range("master EV fades",
        halfway.mGlobals.mMasterEV, 0.f, 1e-6f);
    ensure_approximately_equals_range("master mired fades",
        halfway.mGlobals.mMasterTempMired, 20.f, 1e-6f);
    ensure_approximately_equals_range("bounce fades",
        halfway.mGlobals.mBounceRatio, 0.5f, 1e-6f);
    ensure_approximately_equals_range("explicit shadow softness fades",
        halfway.mShadowSoftOverride[0], 4.f, 1e-6f);
    ensure_equals("auto-shadow sentinel stays discrete at midpoint",
                  halfway.mShadowSoftOverride[1], 3.f);
    ensure_approximately_equals_range("yaw takes shortest wrapped path",
        halfway.mTransforms.mYawDeg, 180.f, 1e-6f);
    ensure_equals("FX arms at delay boundary, independent of fade",
                  halfway.mFX, 9);

    const CueState after_midpoint = evaluateCueTransition(
        start, target, 14.1, 10.0);
    ensure_equals("gobo block swaps after midpoint",
                  after_midpoint.mSetup.mLights[0].mGobo,
                  GOBO_ROTATING_FAN);
    ensure_equals("auto-shadow sentinel swaps after midpoint",
                  after_midpoint.mShadowSoftOverride[1], -1.f);
}

template<> template<>
void cine_light_rig_model_object::test<47>()
{
    set_test_name("absolute cue timecode evaluation is scrub-back pure");

    CueState released;
    released.mSetup.mLights[0].mOn = false;
    released.mSetup.mLights[0].mEV = -8.f;

    Cue first;
    first.mLabel = "First";
    first.mAtSec = 10.0;
    first.mFadeSec = 4.f;
    first.mProfile = CUE_FADE_LINEAR;
    first.mSetup.mLights[0].mOn = true;
    first.mSetup.mLights[0].mEV = 0.f;

    Cue second = first;
    second.mLabel = "Second";
    second.mAtSec = 20.0;
    second.mFadeSec = 2.f;
    second.mSetup.mLights[0].mEV = 4.f;

    Cue same_time = second;
    same_time.mLabel = "Same-time later row";
    same_time.mSetup.mLights[0].mEV = 6.f;

    CueList list;
    list.mTimecodeMode = true;
    list.mCues.push_back(second);
    list.mCues.push_back(first);
    list.mCues.push_back(same_time);

    ensure_equals("before first cue has no active index",
                  timecodeCueIndex(list, 9.0), -1);
    ensure_equals("unsorted list finds first absolute cue",
                  timecodeCueIndex(list, 12.0), 1);
    ensure_equals("duplicate time picks later row deterministically",
                  timecodeCueIndex(list, 20.0), 2);

    const CueState pre_roll = evaluateTimecodeCueList(list, released, 9.0);
    ensure_equals("pre-roll returns released state",
                  pre_roll.mSetup.mLights[0].mOn, false);
    const CueState forward = evaluateTimecodeCueList(list, released, 12.0);
    ensure_approximately_equals_range("first cue halfway from release",
        forward.mSetup.mLights[0].mEV, -4.f, 1e-6f);
    const CueState later = evaluateTimecodeCueList(list, released, 21.0);
    ensure_approximately_equals_range("same-time selected cue is halfway",
        later.mSetup.mLights[0].mEV, 3.f, 1e-6f);
    const CueState scrubbed_back = evaluateTimecodeCueList(list, released, 12.0);
    ensure_approximately_equals_range("scrub back reproduces exact EV",
        scrubbed_back.mSetup.mLights[0].mEV,
        forward.mSetup.mLights[0].mEV, 1e-6f);
    ensure_equals("scrub back reproduces discrete state",
                  scrubbed_back.mSetup.mLights[0].mOn,
                  forward.mSetup.mLights[0].mOn);
}

template<> template<>
void cine_light_rig_model_object::test<48>()
{
    set_test_name("cue sanitizer bounds malformed show data");

    Cue dirty;
    dirty.mLabel.assign(200, 'L');
    dirty.mProvenance.assign(400, 'P');
    dirty.mFadeSec = std::numeric_limits<F32>::quiet_NaN();
    dirty.mDelaySec = std::numeric_limits<F32>::infinity();
    dirty.mProfile = 99;
    dirty.mFollow = 999999;
    dirty.mAtSec = std::numeric_limits<F64>::infinity();
    dirty.mFX = FX_COUNT + 100;
    dirty.mShadowSoftOverride[0] = std::numeric_limits<F32>::quiet_NaN();
    dirty.mShadowSoftOverride[1] = 99.f;
    const Cue clean = sanitizeCue(dirty);
    ensure_equals("label bounded", clean.mLabel.size(), std::size_t(128));
    ensure_equals("provenance bounded", clean.mProvenance.size(),
                  std::size_t(256));
    ensure_equals("NaN fade has safe default", clean.mFadeSec, 3.f);
    ensure_equals("infinite delay has safe default", clean.mDelaySec, 0.f);
    ensure_equals("profile clamps", clean.mProfile, CUE_FADE_SNAP);
    ensure_equals("follow clamps", clean.mFollow, 86400);
    ensure_equals("infinite absolute time has safe default", clean.mAtSec, 0.0);
    ensure_equals("FX clamps", clean.mFX, FX_COUNT - 1);
    ensure_equals("NaN shadow override becomes auto",
                  clean.mShadowSoftOverride[0], -1.f);
    ensure_equals("shadow override clamps", clean.mShadowSoftOverride[1], 8.f);

    CueList oversized;
    oversized.mName.assign(200, 'N');
    oversized.mCues.assign(300, dirty);
    const CueList bounded = sanitizeCueList(oversized);
    ensure_equals("cue-list name bounded", bounded.mName.size(),
                  std::size_t(128));
    ensure_equals("cue-list row count bounded", bounded.mCues.size(),
                  std::size_t(256));
}

template<> template<>
void cine_light_rig_model_object::test<49>()
{
    set_test_name("multi-instance storage preserves embedded cue lists");

    ALCineLightRigParamBlob original = distinctiveBlob();
    LLSD cue_list = LLSD::emptyMap();
    cue_list["version"] = 1;
    cue_list["name"] = "Show A";
    cue_list["timecode_mode"] = true;
    cue_list["cues"] = LLSD::emptyArray();
    original.mCueList = cue_list;

    const LLSD encoded = original.toLLSD();
    ensure("blob emits optional cue-list map", encoded["cue_list"].isMap());
    const ALCineLightRigParamBlob decoded =
        ALCineLightRigParamBlob::fromLLSD(encoded);
    ensure_equals("cue-list payload round-trips exactly",
                  decoded.mCueList, cue_list);

    LLSD legacy = encoded;
    legacy.erase("cue_list");
    ensure("old instance blobs retain the inert undefined default",
           ALCineLightRigParamBlob::fromLLSD(legacy).mCueList.isUndefined());
}

template<> template<>
void cine_light_rig_model_object::test<50>()
{
    set_test_name("easy cone width orders and clamps authored beam presets");

    ensure_equals("narrow selects Snoot", easyConeWidthToBeam(0), 2);
    ensure_equals("medium selects Standard", easyConeWidthToBeam(1), 0);
    ensure_equals("wide selects Softbox", easyConeWidthToBeam(2), 1);
    ensure_equals("low width clamps", easyConeWidthToBeam(-99), 2);
    ensure_equals("high width clamps", easyConeWidthToBeam(99), 1);

    ensure_equals("Snoot displays narrow", easyConeWidthFromBeam(2), 0);
    ensure_equals("Standard displays medium", easyConeWidthFromBeam(0), 1);
    ensure_equals("Softbox displays wide", easyConeWidthFromBeam(1), 2);
    for (S32 width = 0; width < 3; ++width)
    {
        ensure_equals("easy cone mapping round-trips",
                      easyConeWidthFromBeam(easyConeWidthToBeam(width)), width);
    }
}

template<> template<>
void cine_light_rig_model_object::test<51>()
{
    set_test_name("new nightlife and horror FX preserve their authored motion");

    LightBase lights[LIGHT_COUNT];
    evalFX(FX_CANDY_ORBIT, 123, 0.0, lights);
    S32 active = 0;
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        active += lights[i].mOn ? 1 : 0;
    }
    ensure_equals("Candy Orbit has three active lights", active, 3);
    ensure("Candy Orbit keeps Fill off", !lights[1].mOn);
    for (S32 i : { 0, 2, 3 })
    {
        ensure("Candy Orbit EV stays in its authored range",
               lights[i].mEV >= -2.6001f && lights[i].mEV <= -0.1999f);
    }

    for (F64 time : { 0.0, 1.9634954084936207, 137.0 })
    {
        evalFX(FX_RIMWAVE, 123, time, lights);
        ensure("Rimwave keeps Fill off", !lights[1].mOn);
        ensure_approximately_equals_range(
            "Rimwave opposed-edge EV sum is constant",
            lights[0].mEV + lights[2].mEV, -1.3f, 1e-5f);
    }

    for (F64 time : { 0.0, 3.4, 47.0, 500.0 })
    {
        evalFX(FX_AFTERHOURS_DRIFT, 123, time, lights);
        ensure_equals("Afterhours Drift keeps Curtain Edge on Key",
                      lights[0].mGobo, 10);
        ensure_equals("Afterhours Drift keeps Neon Sign Mask on Background",
                      lights[3].mGobo, 23);
        for (const LightBase& light : lights)
        {
            ensure("Afterhours Drift output stays finite",
                   std::isfinite(light.mYawDeg) &&
                   std::isfinite(light.mPitchDeg) &&
                   std::isfinite(light.mEV));
        }
    }

    LightBase horror_mid[LIGHT_COUNT];
    evalFX(FX_SOMETHING_BEHIND_YOU, 123, 9.0, horror_mid);
    ensure_approximately_equals_range("horror midpoint Key yaw",
                                      horror_mid[0].mYawDeg, 0.f, 1e-5f);
    ensure_approximately_equals_range("horror midpoint Key pitch",
                                      horror_mid[0].mPitchDeg, 18.f, 1e-5f);
    ensure_approximately_equals_range("horror midpoint Key EV",
                                      horror_mid[0].mEV, -1.6f, 1e-5f);

    evalFX(FX_SOMETHING_BEHIND_YOU, 123, 12.96, lights);
    ensure_approximately_equals_range("horror blood-rim reveal peaks",
                                      lights[2].mEV, 0.5f, 1e-4f);
    evalFX(FX_SOMETHING_BEHIND_YOU, 123, 15.3, lights);
    ensure_equals("horror dark tail turns Key down", lights[0].mEV, -10.f);
    ensure_equals("horror dark tail turns Rim down", lights[2].mEV, -10.f);

    evalFX(FX_SOMETHING_BEHIND_YOU, 123, 9.0, lights);
    ensureSameLights("horror backward scrub reproduces the midpoint",
                     horror_mid, lights);

    const S32 new_fx[] = {
        FX_CANDY_ORBIT, FX_RIMWAVE, FX_AFTERHOURS_DRIFT,
        FX_SOMETHING_BEHIND_YOU,
    };
    for (S32 fx : new_fx)
    {
        for (F64 time : { 0.0, 0.001, 12.96, 18.0, 137.0 })
        {
            evalFX(fx, 123, time, lights);
            for (const LightBase& light : lights)
            {
                ensure("new FX output is finite and bounded",
                       std::isfinite(light.mYawDeg) &&
                       std::isfinite(light.mPitchDeg) &&
                       std::isfinite(light.mEV) &&
                       std::fabs(light.mYawDeg) <= 180.f &&
                       std::fabs(light.mPitchDeg) <= PITCH_LIMIT_DEG &&
                       light.mProfile >= 0 && light.mProfile < PROFILE_COUNT &&
                       light.mGobo >= 0 && light.mGobo < GOBO_COUNT);
            }
        }
    }
}

template<> template<>
void cine_light_rig_model_object::test<52>()
{
    set_test_name("live probe bounce keep scales only its target rig bounce");
    using ALCineLightRigManagerModel::liveProbeBounceKeepScale;

    ensure_equals("disabled probe preserves bounce",
        liveProbeBounceKeepScale(false, 0.f, true, 1.f), 1.f);
    ensure_equals("keep=1 never darkens the fill",
        liveProbeBounceKeepScale(true, 1.f, true, 1.f), 1.f);
    ensure_equals("non-target rig preserves bounce",
        liveProbeBounceKeepScale(true, 0.f, false, 1.f), 1.f);
    ensure_equals("cold probe preserves bounce",
        liveProbeBounceKeepScale(true, 0.f, true, 0.f), 1.f);
    ensure_equals("half-ready probe with keep=0 halves synthetic bounce",
        liveProbeBounceKeepScale(true, 0.f, true, 0.5f), 0.5f);
    ensure_equals("ready probe with keep=0 fully replaces (legacy Replace bounce)",
        liveProbeBounceKeepScale(true, 0.f, true, 1.f), 0.f);
    ensure_equals("ready probe with keep=0.5 keeps half",
        liveProbeBounceKeepScale(true, 0.5f, true, 1.f), 0.5f);
    ensure_equals("half-ready probe with keep=0.5 keeps three quarters",
        liveProbeBounceKeepScale(true, 0.5f, true, 0.5f), 0.75f);
    ensure_equals("fade clamps below zero",
        liveProbeBounceKeepScale(true, 0.f, true, -1.f), 1.f);
    ensure_equals("fade clamps above one",
        liveProbeBounceKeepScale(true, 0.f, true, 2.f), 0.f);
    ensure_equals("keep clamps above one",
        liveProbeBounceKeepScale(true, 2.f, true, 1.f), 1.f);
    ensure_equals("keep clamps below zero",
        liveProbeBounceKeepScale(true, -1.f, true, 1.f), 0.f);
    ensure_equals("non-finite fade fails open",
        liveProbeBounceKeepScale(true, 0.f, true,
            std::numeric_limits<F32>::quiet_NaN()), 1.f);
    ensure_equals("non-finite keep fails open",
        liveProbeBounceKeepScale(true,
            std::numeric_limits<F32>::quiet_NaN(), true, 1.f), 1.f);
}

template<> template<>
void cine_light_rig_model_object::test<53>()
{
    set_test_name("live probe scene codec is atomic and preset-independent");
    using namespace ALCineLightRigManagerModel;

    LiveProbeConfig authored;
    authored.mEnabled = true;
    authored.mTarget = 3;
    authored.mRadius = 7.5f;
    authored.mOffsetZ = -0.75f;
    authored.mAmbiance = 1.25f;
    authored.mBounceKeep = 0.35f;
    authored.mGizmo = true;

    LiveProbeConfig decoded;
    ensure("complete scene block decodes",
        liveProbeConfigFromLLSD(liveProbeConfigToLLSD(authored), decoded));
    ensure("enabled round-trips", decoded.mEnabled);
    ensure_equals("target round-trips", decoded.mTarget, 3);
    ensure_equals("radius round-trips", decoded.mRadius, 7.5f);
    ensure_equals("offset round-trips", decoded.mOffsetZ, -0.75f);
    ensure_equals("ambiance round-trips", decoded.mAmbiance, 1.25f);
    ensure_equals("bounce keep round-trips", decoded.mBounceKeep, 0.35f);
    ensure("gizmo round-trips", decoded.mGizmo);

    decoded = authored;
    ensure("absent legacy block migrates cleanly",
        liveProbeConfigFromLLSD(LLSD(), decoded));
    ensure("legacy migration disables probe", !decoded.mEnabled);
    ensure_equals("legacy migration restores radius default",
        decoded.mRadius, 3.f);

    LLSD malformed = liveProbeConfigToLLSD(authored);
    malformed["radius"] = "invalid";
    decoded = authored;
    ensure("malformed block is rejected atomically",
        !liveProbeConfigFromLLSD(malformed, decoded));
    ensure("rejected block cannot retain prior enable", !decoded.mEnabled);
    ensure_equals("rejected block cannot retain prior radius",
        decoded.mRadius, 3.f);

    // Scenes saved before bounce_keep existed carry only the legacy bool.
    LLSD legacy = liveProbeConfigToLLSD(authored);
    legacy.erase("bounce_keep");
    legacy["replace_bounce"] = true;
    ensure("legacy replace_bounce=true decodes", liveProbeConfigFromLLSD(legacy, decoded));
    ensure_equals("legacy full replace migrates to keep 0", decoded.mBounceKeep, 0.f);
    legacy["replace_bounce"] = false;
    ensure("legacy replace_bounce=false decodes", liveProbeConfigFromLLSD(legacy, decoded));
    ensure_equals("legacy opt-out migrates to keep 1", decoded.mBounceKeep, 1.f);

    LLSD bad_keep = liveProbeConfigToLLSD(authored);
    bad_keep["bounce_keep"] = "invalid";
    ensure("malformed bounce_keep is rejected", !liveProbeConfigFromLLSD(bad_keep, decoded));

    ensure("encoder writes the legacy key for older builds",
        liveProbeConfigToLLSD(authored).has("replace_bounce"));

    const LLSD rig_blob = ALCineLightRigParamBlob().toLLSD();
    ensure("rig Setup/instance payload excludes scene-level probe",
        !rig_blob.has("live_probe") &&
        !rig_blob.has("CineLightRigLiveProbeEnabled"));
}

// [RigRim] P1-B / P2-2 regression coverage: a scene blob saved before Rig
// Rim existed (or before a given per-light key was added) must round-trip
// to the CONTRACT per-light default, not the Light struct's own
// index-independent default (which would silently zero the RIM fixture's
// default gain instead of leaving it at 1.0). Also covers NaN/inf
// sanitization on the values that ARE present.
template<> template<>
void cine_light_rig_model_object::test<54>()
{
    set_test_name("rim params: absent LLSD keys fall back to the per-light "
                  "contract default, not the struct default");
    const ALCineLightRigParamBlob expected = distinctiveBlob();
    LLSD data = expected.toLLSD();
    ensure("rim gain is stored per light",
        data["lights"][2].has("rim_gain"));

    // A pre-Rig-Rim scene: strip every rim key from every light.
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        data["lights"][i].erase("rim_gain");
        data["lights"][i].erase("rim_sharpness");
        data["lights"][i].erase("rim_wrap");
        data["lights"][i].erase("rim_back_bias");
    }
    const ALCineLightRigParamBlob legacy = ALCineLightRigParamBlob::fromLLSD(data);
    static const F32 kExpectedDefaultGain[LIGHT_COUNT] = { 0.f, 0.f, 1.f, 0.f };
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        ensure_equals("absent rim_gain uses the per-light contract default",
            legacy.mLights[i].mRimGain, kExpectedDefaultGain[i]);
        ensure_equals("absent rim_sharpness uses the contract default",
            legacy.mLights[i].mRimSharpness, 3.f);
        ensure_equals("absent rim_wrap uses the contract default",
            legacy.mLights[i].mRimWrap, 0.35f);
        ensure_equals("absent rim_back_bias uses the contract default",
            legacy.mLights[i].mRimBackBias, 0.f);
    }
    // Matches ALPanelCineLightRig's settings.xml defaults for
    // CineRigRim{Key,Fill,Rim,Bg}Gain: only the RIM fixture defaults on.
    ensure_equals("RIM is the only fixture with a nonzero default gain",
        ALCineLightRigParamBlob::defaultRimGain(2), 1.f);
    ensure_equals("KEY default gain is off", ALCineLightRigParamBlob::defaultRimGain(0), 0.f);
    ensure_equals("FILL default gain is off", ALCineLightRigParamBlob::defaultRimGain(1), 0.f);
    ensure_equals("BG default gain is off", ALCineLightRigParamBlob::defaultRimGain(3), 0.f);

    // [RigRim] Round-2 P2-C: a bare default-constructed blob (no fromLLSD /
    // fromSettingsStore involved at all -- e.g. a non-migrated
    // ALCineLightRigManager slot reset to `ParamBlob()`) must ALSO start
    // with the per-light contract default, not the Light struct's flat
    // 0-default.
    const ALCineLightRigParamBlob bare_default;
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        ensure_equals("bare default-constructed blob matches the per-light "
                      "contract default gain",
            bare_default.mLights[i].mRimGain, kExpectedDefaultGain[i]);
    }

    // An explicit value is still honoured (absent != "present but zero").
    LLSD explicit_zero = expected.toLLSD();
    explicit_zero["lights"][2]["rim_gain"] = 0.0;
    ensure_equals("an explicit zero rim_gain is NOT replaced by the default",
        ALCineLightRigParamBlob::fromLLSD(explicit_zero).mLights[2].mRimGain, 0.f);

    // [RigRim] P2-2: NaN/inf in a present key must not survive the read.
    LLSD non_finite = expected.toLLSD();
    non_finite["lights"][2]["rim_gain"] =
        std::numeric_limits<double>::quiet_NaN();
    non_finite["lights"][0]["rim_sharpness"] =
        std::numeric_limits<double>::infinity();
    const ALCineLightRigParamBlob sanitized =
        ALCineLightRigParamBlob::fromLLSD(non_finite);
    ensure("NaN rim_gain falls back to the per-light default",
        std::isfinite(sanitized.mLights[2].mRimGain));
    ensure_equals("NaN rim_gain falls back to the RIM default specifically",
        sanitized.mLights[2].mRimGain, 1.f);
    ensure("infinite rim_sharpness falls back to the contract default",
        std::isfinite(sanitized.mLights[0].mRimSharpness));
    ensure_equals("infinite rim_sharpness falls back to 3.0",
        sanitized.mLights[0].mRimSharpness, 3.f);
}

// ---------------------------------------------------------------------------
// [LiveProbeRefresh] Model tests M1-M15 (test<55>..test<69>).
// These prove only the scheduling / hashing model driven by synthetic H values.
// Timestamps use dt = 1/64 s (exactly representable), so the 5.0 s watchdog and
// 0.5 s settle boundaries are exact rather than subject to rounding.
// Not provable here: real frame order, scratch isolation, pixel identity of the
// moved Every-frame body (those are the in-world checks in the design doc).
// ---------------------------------------------------------------------------
namespace lpr = ALCineLiveProbeRefresh;

namespace
{
struct LiveProbeSim
{
    struct PassLog
    {
        S32 mFrame = 0;
        lpr::PassEnd mEnd = lpr::PassEnd::NONE;
    };

    lpr::State mState;
    lpr::Params mParams;
    int mProbeTag = 0;
    S32 mCube = 7;
    bool mReady = true;
    bool mAnimating = false;
    bool mFullRadiance = false;
    S32 mFrame = 0;
    S32 mLastBurstFaces = 0; // faces actually captured by the last step
    lpr::Decision mDecision;
    S32 mIrrPubs = 0;
    S32 mRadPubs = 0;
    S32 mLastRadFrame = -1;
    S32 mMaxRadGap = 0;
    std::vector<PassLog> mLog;

    static F64 timeOf(S32 frame) { return static_cast<F64>(frame) / 64.0; }
    F64 now() const { return timeOf(mFrame); }

    void note(lpr::PassEnd end)
    {
        PassLog entry;
        entry.mFrame = mFrame;
        entry.mEnd = end;
        mLog.push_back(entry);
        if (end == lpr::PassEnd::IRRADIANCE)
        {
            ++mIrrPubs;
        }
        else if (end == lpr::PassEnd::RADIANCE)
        {
            ++mRadPubs;
            if (mLastRadFrame >= 0)
            {
                mMaxRadGap = std::max(mMaxRadGap, mFrame - mLastRadFrame);
            }
            mLastRadFrame = mFrame;
        }
    }

    // One manager step: decide, then act exactly like the reflection manager.
    lpr::Decision step(U64 h)
    {
        ++mFrame;
        mDecision = lpr::decide(mState, mParams, h, mAnimating, mReady, now(),
                                &mProbeTag, mCube);
        mLastBurstFaces = 0;
        if (mDecision.mPath == lpr::Path::BUDGET)
        {
            lpr::noteFrameH(mState, h);
            for (S32 n = 0; n < mDecision.mFaces; ++n)
            {
                // Mirrors updateCinematicBudget: only Manual continues past a pass end.
                if (!mState.mActive && !lpr::continueManualPass(mState, now()))
                {
                    break;
                }
                ++mLastBurstFaces;
                const lpr::PassEnd end = lpr::advanceFace(mState, now());
                if (end != lpr::PassEnd::NONE)
                {
                    note(end);
                }
            }
        }
        else if (mDecision.mPath == lpr::Path::FULL)
        {
            // Mirrors the manager: the FULL cursor is resynced with the
            // scheduler (next pass is radiance iff the last completed pass was
            // irradiance), since budget passes never advance it.
            mFullRadiance = mState.mLastPassIrr;
            const bool radiance = mFullRadiance;
            lpr::onFullPass(mState, radiance, h, now());
            note(radiance ? lpr::PassEnd::RADIANCE : lpr::PassEnd::IRRADIANCE);
        }
        return mDecision;
    }
};

typedef std::function<void(lpr::Signature&, const F32*)> SigAdder;

U64 sampleSticky(lpr::StickyHash& sticky, const SigAdder& add, const F32* v)
{
    lpr::Signature sig;
    add(sig, v);
    return sticky.update(sig);
}

// Deterministic pseudo-noise in [-1, 1].
F32 lprNoise(S32 i)
{
    return static_cast<F32>((i * 37) % 201 - 100) / 100.f;
}

// The visible change must move H; bounded noise around the same base for 1000
// frames must never move it.
void checkFieldTolerance(const std::string& name, const SigAdder& add,
                         const F32* base, const F32* changed,
                         const F32* noise_amp)
{
    lpr::StickyHash noise_sticky;
    const U64 h0 = sampleSticky(noise_sticky, add, base);
    for (S32 i = 0; i < 1000; ++i)
    {
        F32 v[3];
        for (S32 c = 0; c < 3; ++c)
        {
            v[c] = base[c] + noise_amp[c] * lprNoise(i * 3 + c);
        }
        const U64 h = sampleSticky(noise_sticky, add, v);
        if (h != h0)
        {
            fail("bounded noise moved H for field: " + name);
        }
    }
    lpr::StickyHash change_sticky;
    const U64 hb = sampleSticky(change_sticky, add, base);
    const U64 hc = sampleSticky(change_sticky, add, changed);
    ensure("visible change must move H for field: " + name, hb != hc);
}
} // namespace

// M1
template<> template<>
void cine_light_rig_model_object::test<55>()
{
    set_test_name("[LiveProbeRefresh] M1 sanitizeMode bounds");
    ensure("-1 -> ON_CHANGE", lpr::sanitizeMode(-1) == lpr::Mode::ON_CHANGE);
    ensure("0 -> EVERY_FRAME", lpr::sanitizeMode(0) == lpr::Mode::EVERY_FRAME);
    ensure("1 -> BALANCED", lpr::sanitizeMode(1) == lpr::Mode::BALANCED);
    ensure("2 -> ECONOMY", lpr::sanitizeMode(2) == lpr::Mode::ECONOMY);
    ensure("3 -> ON_CHANGE", lpr::sanitizeMode(3) == lpr::Mode::ON_CHANGE);
    ensure("4 -> MANUAL", lpr::sanitizeMode(4) == lpr::Mode::MANUAL); // [ProbeManualRate]
    ensure("5 -> ON_CHANGE", lpr::sanitizeMode(5) == lpr::Mode::ON_CHANGE);
    ensure("100 -> ON_CHANGE", lpr::sanitizeMode(100) == lpr::Mode::ON_CHANGE);
}

// M2
template<> template<>
void cine_light_rig_model_object::test<56>()
{
    set_test_name("[LiveProbeRefresh] M2 not ready -> FULL / WARMUP in every budget mode");
    const lpr::Mode modes[3] = {
        lpr::Mode::BALANCED, lpr::Mode::ECONOMY, lpr::Mode::ON_CHANGE };
    for (const lpr::Mode mode : modes)
    {
        LiveProbeSim sim;
        sim.mParams.mMode = mode;
        sim.mReady = false;
        for (S32 f = 0; f < 5; ++f)
        {
            const lpr::Decision d = sim.step(9);
            ensure("not ready is FULL", d.mPath == lpr::Path::FULL);
            ensure("not ready reason is WARMUP", d.mReason == lpr::Reason::WARMUP);
        }
    }
}

// M3
template<> template<>
void cine_light_rig_model_object::test<57>()
{
    set_test_name("[LiveProbeRefresh] M3 Balanced 3-frame alternating passes, never idle");
    LiveProbeSim sim;
    sim.mParams.mMode = lpr::Mode::BALANCED;
    for (S32 f = 1; f <= 12; ++f)
    {
        const lpr::Decision d = sim.step(0);
        ensure("Balanced is never IDLE", d.mPath == lpr::Path::BUDGET);
        ensure_equals("Balanced takes 2 faces per frame", d.mFaces, 2);
    }
    ensure_equals("four passes finished in 12 frames", static_cast<S32>(sim.mLog.size()), 4);
    ensure_equals("pass 1 ends on frame 3", sim.mLog[0].mFrame, 3);
    ensure("pass 1 is irradiance", sim.mLog[0].mEnd == lpr::PassEnd::IRRADIANCE);
    ensure_equals("pass 2 ends on frame 6", sim.mLog[1].mFrame, 6);
    ensure("pass 2 is radiance", sim.mLog[1].mEnd == lpr::PassEnd::RADIANCE);
    ensure_equals("pass 3 ends on frame 9", sim.mLog[2].mFrame, 9);
    ensure("pass 3 is irradiance", sim.mLog[2].mEnd == lpr::PassEnd::IRRADIANCE);
    ensure_equals("pass 4 ends on frame 12", sim.mLog[3].mFrame, 12);
    ensure("pass 4 is radiance", sim.mLog[3].mEnd == lpr::PassEnd::RADIANCE);
}

// M4
template<> template<>
void cine_light_rig_model_object::test<58>()
{
    set_test_name("[LiveProbeRefresh] M4 Economy 6-frame alternating passes, never idle");
    LiveProbeSim sim;
    sim.mParams.mMode = lpr::Mode::ECONOMY;
    for (S32 f = 1; f <= 18; ++f)
    {
        const lpr::Decision d = sim.step(0);
        ensure("Economy is never IDLE", d.mPath == lpr::Path::BUDGET);
        ensure_equals("Economy takes 1 face per frame", d.mFaces, 1);
    }
    ensure_equals("three passes finished in 18 frames", static_cast<S32>(sim.mLog.size()), 3);
    ensure_equals("pass 1 ends on frame 6", sim.mLog[0].mFrame, 6);
    ensure("pass 1 is irradiance", sim.mLog[0].mEnd == lpr::PassEnd::IRRADIANCE);
    ensure_equals("pass 2 ends on frame 12", sim.mLog[1].mFrame, 12);
    ensure("pass 2 is radiance", sim.mLog[1].mEnd == lpr::PassEnd::RADIANCE);
    ensure_equals("pass 3 ends on frame 18", sim.mLog[2].mFrame, 18);
    ensure("pass 3 is irradiance", sim.mLog[2].mEnd == lpr::PassEnd::IRRADIANCE);
}

// M5
template<> template<>
void cine_light_rig_model_object::test<59>()
{
    set_test_name("[LiveProbeRefresh] M5 On change, constant H: clean pair then IDLE");
    LiveProbeSim sim;
    sim.mParams.mMode = lpr::Mode::ON_CHANGE;
    const U64 h = 42;
    for (S32 f = 1; f <= 6; ++f)
    {
        const lpr::Decision d = sim.step(h);
        ensure("first six frames build the pair", d.mPath == lpr::Path::BUDGET);
    }
    ensure_equals("one irradiance pass", sim.mIrrPubs, 1);
    ensure_equals("one radiance pass", sim.mRadPubs, 1);
    ensure("converged after the clean pair", lpr::converged(sim.mState, h));
    for (S32 f = 7; f <= 200; ++f)
    {
        const lpr::Decision d = sim.step(h);
        ensure("idle once converged", d.mPath == lpr::Path::IDLE);
    }
    ensure_equals("no further irradiance passes", sim.mIrrPubs, 1);
    ensure_equals("no further radiance passes", sim.mRadPubs, 1);
    ensure("still converged", lpr::converged(sim.mState, h));
}

// M6
template<> template<>
void cine_light_rig_model_object::test<60>()
{
    set_test_name("[LiveProbeRefresh] M6 H changes inside an irradiance pass -> dirty, then clean pair");
    LiveProbeSim sim;
    sim.mParams.mMode = lpr::Mode::ON_CHANGE;
    sim.step(1);
    sim.step(2); // H changes mid-pass
    sim.step(2);
    ensure_equals("irradiance pass ended", sim.mIrrPubs, 1);
    ensure("the dirty irradiance pass does not count", !sim.mState.mIrrOk);
    // Cycling continues: radiance (dirty pairing), then irradiance, then radiance.
    for (S32 f = 4; f <= 12; ++f)
    {
        const lpr::Decision d = sim.step(2);
        ensure("keeps cycling while not converged", d.mPath == lpr::Path::BUDGET);
    }
    ensure("two clean passes on the stable H converge", lpr::converged(sim.mState, 2));
    ensure("idle afterwards", sim.step(2).mPath == lpr::Path::IDLE);
}

// M7
template<> template<>
void cine_light_rig_model_object::test<61>()
{
    set_test_name("[LiveProbeRefresh] M7 A->B->A inside a radiance pass -> dirty, another pass runs");
    LiveProbeSim sim;
    sim.mParams.mMode = lpr::Mode::ON_CHANGE;
    sim.step(1);
    sim.step(1);
    sim.step(1); // clean irradiance at H=1
    ensure("clean irradiance", sim.mState.mIrrOk);
    sim.step(1); // radiance pass starts at H=1
    sim.step(2); // transient change
    sim.step(1); // back to the start value; the pass ends
    ensure_equals("radiance pass ended", sim.mRadPubs, 1);
    ensure("A->B->A pass is dirty", !sim.mState.mRadOk);
    ensure("not converged", !lpr::converged(sim.mState, 1));
    ensure("another pass runs", sim.step(1).mPath == lpr::Path::BUDGET);
    for (S32 f = 8; f <= 12; ++f)
    {
        sim.step(1);
    }
    ensure("a later clean pair converges", lpr::converged(sim.mState, 1));
}

// M8
template<> template<>
void cine_light_rig_model_object::test<62>()
{
    set_test_name("[LiveProbeRefresh] M8 clean irradiance at H1 then H2: radiance still next, not counted");
    LiveProbeSim sim;
    sim.mParams.mMode = lpr::Mode::ON_CHANGE;
    sim.step(1);
    sim.step(1);
    sim.step(1);
    ensure("clean irradiance at H1", sim.mState.mIrrOk && sim.mState.mIrrH == 1);
    const lpr::Decision d = sim.step(2); // H2 before the radiance pass
    ensure("next pass starts", d.mPath == lpr::Path::BUDGET);
    ensure("the next pass is still radiance (alternation)", sim.mState.mRadiance);
    sim.step(2);
    sim.step(2);
    ensure_equals("radiance pass ended", sim.mRadPubs, 1);
    ensure("radiance on H2 after irradiance on H1 is not counted", !sim.mState.mRadOk);
    for (S32 f = 7; f <= 12; ++f)
    {
        sim.step(2);
    }
    ensure("the irradiance -> radiance pair on H2 converges", lpr::converged(sim.mState, 2));
}

// M9
template<> template<>
void cine_light_rig_model_object::test<63>()
{
    set_test_name("[LiveProbeRefresh] M9 watchdog: exactly one pair every 5.0 s; 0 disables");
    LiveProbeSim sim;
    sim.mParams.mMode = lpr::Mode::ON_CHANGE;
    sim.mParams.mWatchdogSec = 5.f;
    const U64 h = 7;
    for (S32 f = 1; f <= 6; ++f)
    {
        sim.step(h);
    }
    ensure("converged at frame 6", lpr::converged(sim.mState, h));
    // Converged at frame 6 (t = 6/64). The watchdog fires when now - t >= 5.0,
    // i.e. frame 326.
    for (S32 f = 7; f <= 325; ++f)
    {
        ensure("IDLE until the watchdog interval elapses",
            sim.step(h).mPath == lpr::Path::IDLE);
    }
    const lpr::Decision start = sim.step(h); // frame 326
    ensure("watchdog starts a pass", start.mPath == lpr::Path::BUDGET);
    ensure("watchdog reason", start.mReason == lpr::Reason::WATCHDOG);
    sim.step(h);
    sim.step(h); // irradiance ends (frame 328)
    const lpr::Decision second = sim.step(h); // radiance starts (frame 329)
    ensure("second half of the pair keeps the WATCHDOG reason",
        second.mReason == lpr::Reason::WATCHDOG);
    sim.step(h);
    sim.step(h); // radiance ends (frame 331)
    ensure_equals("exactly one extra irradiance pass", sim.mIrrPubs, 2);
    ensure_equals("exactly one extra radiance pass", sim.mRadPubs, 2);
    ensure("IDLE again after the pair", sim.step(h).mPath == lpr::Path::IDLE);

    LiveProbeSim never;
    never.mParams.mMode = lpr::Mode::ON_CHANGE;
    never.mParams.mWatchdogSec = 0.f;
    for (S32 f = 1; f <= 2000; ++f)
    {
        never.step(h);
    }
    ensure_equals("WatchdogSec = 0 never re-checks (irradiance)", never.mIrrPubs, 1);
    ensure_equals("WatchdogSec = 0 never re-checks (radiance)", never.mRadPubs, 1);
}

// M10
template<> template<>
void cine_light_rig_model_object::test<64>()
{
    set_test_name("[LiveProbeRefresh] M10 manual request while IDLE -> exactly one pair");
    LiveProbeSim sim;
    sim.mParams.mMode = lpr::Mode::ON_CHANGE;
    const U64 h = 11;
    for (S32 f = 1; f <= 8; ++f)
    {
        sim.step(h);
    }
    ensure("idle before the request", sim.step(h).mPath == lpr::Path::IDLE);
    sim.mState.mManual = true;
    const lpr::Decision d = sim.step(h);
    ensure("manual request starts a pass", d.mPath == lpr::Path::BUDGET);
    ensure("manual reason", d.mReason == lpr::Reason::MANUAL);
    ensure("request consumed", !sim.mState.mManual);
    for (S32 f = 0; f < 5; ++f)
    {
        sim.step(h);
    }
    ensure_equals("one extra irradiance pass", sim.mIrrPubs, 2);
    ensure_equals("one extra radiance pass", sim.mRadPubs, 2);
    ensure("idle again", sim.step(h).mPath == lpr::Path::IDLE);
    ensure_equals("still only one extra pair", sim.mIrrPubs, 2);
}

// M11
template<> template<>
void cine_light_rig_model_object::test<65>()
{
    set_test_name("[LiveProbeRefresh] M11 animating -> FULL/ANIMATED, then SETTLING, then IDLE");
    LiveProbeSim sim;
    sim.mParams.mMode = lpr::Mode::ON_CHANGE;
    sim.mParams.mSettleSec = 0.5f;
    const U64 h = 5;
    sim.mAnimating = true;
    for (S32 f = 1; f <= 9; ++f)
    {
        const lpr::Decision d = sim.step(h);
        ensure("animating is FULL", d.mPath == lpr::Path::FULL);
        ensure("animating reason", d.mReason == lpr::Reason::ANIMATED);
    }
    sim.mAnimating = false;
    // Last animating frame was frame 9. Settling lasts while now - t < 0.5 s,
    // i.e. 31 more frames at 1/64 s.
    for (S32 k = 1; k <= 31; ++k)
    {
        const lpr::Decision d = sim.step(h);
        ensure("settling is FULL", d.mPath == lpr::Path::FULL);
        ensure("settling reason", d.mReason == lpr::Reason::SETTLING);
    }
    ensure("the last FULL pair matched the current H", lpr::converged(sim.mState, h));
    ensure("IDLE once settled", sim.step(h).mPath == lpr::Path::IDLE);

    LiveProbeSim exact;
    exact.mParams.mMode = lpr::Mode::ON_CHANGE;
    exact.mParams.mSettleSec = 0.f;
    exact.mAnimating = true;
    for (S32 f = 1; f <= 3; ++f)
    {
        ensure("animating is FULL (SettleSec 0)",
            exact.step(h).mPath == lpr::Path::FULL);
    }
    exact.mAnimating = false;
    ensure("SettleSec = 0 is FULL exactly while animating",
        exact.step(h).mPath != lpr::Path::FULL);
}

// M12
template<> template<>
void cine_light_rig_model_object::test<66>()
{
    set_test_name("[LiveProbeRefresh] M12 probe / cube index / mode change resets the state");
    const U64 h = 3;
    {
        LiveProbeSim sim;
        sim.mParams.mMode = lpr::Mode::ON_CHANGE;
        for (S32 f = 1; f <= 8; ++f)
        {
            sim.step(h);
        }
        ensure("control: unchanged identity stays IDLE",
            lpr::decide(sim.mState, sim.mParams, h, false, true,
                        LiveProbeSim::timeOf(9), &sim.mProbeTag, sim.mCube).mPath ==
                lpr::Path::IDLE);
        int other_probe = 0;
        const lpr::Decision d = lpr::decide(sim.mState, sim.mParams, h, false, true,
            LiveProbeSim::timeOf(10), &other_probe, sim.mCube);
        ensure("new probe pointer resets and cycles", d.mPath == lpr::Path::BUDGET);
        ensure("new probe pointer stored", sim.mState.mProbe == &other_probe);
        ensure("convergence forgotten", !sim.mState.mIrrOk && !sim.mState.mRadOk);
    }
    {
        LiveProbeSim sim;
        sim.mParams.mMode = lpr::Mode::ON_CHANGE;
        for (S32 f = 1; f <= 8; ++f)
        {
            sim.step(h);
        }
        const lpr::Decision d = lpr::decide(sim.mState, sim.mParams, h, false, true,
            LiveProbeSim::timeOf(9), &sim.mProbeTag, sim.mCube + 1);
        ensure("new cube index resets and cycles", d.mPath == lpr::Path::BUDGET);
        ensure_equals("new cube index stored", sim.mState.mCubeIndex, sim.mCube + 1);
    }
    {
        LiveProbeSim sim;
        sim.mParams.mMode = lpr::Mode::ON_CHANGE;
        for (S32 f = 1; f <= 8; ++f)
        {
            sim.step(h);
        }
        lpr::Params balanced = sim.mParams;
        balanced.mMode = lpr::Mode::BALANCED;
        const lpr::Decision d = lpr::decide(sim.mState, balanced, 0, false, true,
            LiveProbeSim::timeOf(9), &sim.mProbeTag, sim.mCube);
        ensure("mode change resets and cycles", d.mPath == lpr::Path::BUDGET);
        ensure("mode change reason", d.mReason == lpr::Reason::CONTINUOUS);
        ensure("mode change stored", sim.mState.mMode == lpr::Mode::BALANCED);
        ensure("mode change forgot convergence", !sim.mState.mIrrOk && !sim.mState.mRadOk);
    }
}

// M13
template<> template<>
void cine_light_rig_model_object::test<67>()
{
    set_test_name("[LiveProbeRefresh] M13 StickyHash: noise stable, drift = single transitions, layout/id changes fire");
    const SigAdder scalar = [](lpr::Signature& sig, const F32* v)
    {
        sig.addAbs(v[0], 0.01f);
        sig.addExact(true);
    };

    // Noise below tolerance never moves H (1000 frames).
    {
        lpr::StickyHash sticky;
        const F32 base[1] = { 1.f };
        const U64 h0 = sampleSticky(sticky, scalar, base);
        for (S32 i = 0; i < 1000; ++i)
        {
            const F32 v[1] = { 1.f + 0.004f * lprNoise(i) };
            ensure("sub-tolerance noise leaves H constant",
                sampleSticky(sticky, scalar, v) == h0);
        }
    }

    // Slow monotonic drift above tolerance: a few single transitions, and H never
    // returns to an earlier value (no oscillation).
    {
        lpr::StickyHash sticky;
        const SigAdder drift_add = [](lpr::Signature& sig, const F32* v)
        {
            sig.addAbs(v[0], 0.02f);
        };
        std::vector<U64> distinct;
        U64 previous = 0;
        S32 transitions = 0;
        for (S32 i = 0; i <= 100; ++i)
        {
            const F32 v[1] = { 0.001f * static_cast<F32>(i) };
            const U64 h = sampleSticky(sticky, drift_add, v);
            if (i == 0 || h != previous)
            {
                if (i > 0)
                {
                    ++transitions;
                }
                ensure("H never returns to an earlier value",
                    std::find(distinct.begin(), distinct.end(), h) == distinct.end());
                distinct.push_back(h);
            }
            previous = h;
        }
        ensure("drift produced transitions", transitions >= 3);
        ensure("drift produced only single transitions (about one per tolerance)",
            transitions <= 6);
    }

    // A size change moves H.
    {
        lpr::StickyHash sticky;
        lpr::Signature one;
        one.addAbs(1.f, 0.01f);
        const U64 h1 = sticky.update(one);
        lpr::Signature two;
        two.addAbs(1.f, 0.01f);
        two.addAbs(1.f, 0.01f);
        ensure("a size change changes H", sticky.update(two) != h1);
    }

    // An exact id change and an exact bool change move H.
    {
        lpr::StickyHash sticky;
        const LLUUID id_a("11111111-1111-1111-1111-111111111111");
        const LLUUID id_b("22222222-2222-2222-2222-222222222222");
        lpr::Signature a;
        a.addAbs(1.f, 0.01f);
        a.addExact(id_a);
        const U64 ha = sticky.update(a);
        lpr::Signature b;
        b.addAbs(1.f, 0.01f);
        b.addExact(id_b);
        const U64 hb = sticky.update(b);
        ensure("an exact id change changes H", ha != hb);
        lpr::Signature c;
        c.addAbs(1.f, 0.01f);
        c.addExact(id_b);
        ensure("the same exact id is stable", sticky.update(c) == hb);
        lpr::Signature d;
        d.addAbs(1.f, 0.01f);
        d.addExact(id_b);
        d.addExact(true);
        const U64 hd = sticky.update(d);
        lpr::Signature e;
        e.addAbs(1.f, 0.01f);
        e.addExact(id_b);
        e.addExact(false);
        ensure("an exact bool change changes H", sticky.update(e) != hd);
    }
}

// M14
template<> template<>
void cine_light_rig_model_object::test<68>()
{
    set_test_name("[LiveProbeRefresh] M14 continuous H change: publications alternate, radiance gap <= 6, never idle");
    LiveProbeSim sim;
    sim.mParams.mMode = lpr::Mode::ON_CHANGE;
    sim.mParams.mChangeFaces = 2;
    for (S32 f = 1; f <= 600; ++f)
    {
        const U64 h = static_cast<U64>(f);
        const lpr::Decision d = sim.step(h);
        ensure("continuous change is never IDLE", d.mPath != lpr::Path::IDLE);
        ensure("continuous change is never converged", !lpr::converged(sim.mState, h));
    }
    ensure("publications happened", sim.mLog.size() >= 4);
    for (size_t i = 0; i < sim.mLog.size(); ++i)
    {
        const lpr::PassEnd expected = (i % 2 == 0)
            ? lpr::PassEnd::IRRADIANCE : lpr::PassEnd::RADIANCE;
        ensure("irradiance and radiance publications strictly alternate",
            sim.mLog[i].mEnd == expected);
    }
    ensure("the gap between radiance publications is at most 6 frames",
        sim.mMaxRadGap <= 6);
    ensure("radiance really published under continuous change", sim.mRadPubs >= 50);

    // Once H is stable it converges within one in-flight pass + a clean pair.
    S32 frames_to_converge = -1;
    for (S32 f = 1; f <= 30; ++f)
    {
        sim.step(600);
        if (lpr::converged(sim.mState, 600))
        {
            frames_to_converge = f;
            break;
        }
    }
    ensure("converges after H stabilises", frames_to_converge > 0);
    ensure("converges within 12 frames", frames_to_converge <= 12);
}

// M15
template<> template<>
void cine_light_rig_model_object::test<69>()
{
    set_test_name("[LiveProbeRefresh] M15 per-field tolerances: visible change fires, bounded noise does not");

    {   // densityMultiplier Rel(0.01, 1e-7): 1.0e-4 -> 1.1e-4 vs +-0.4 %
        const SigAdder add = [](lpr::Signature& sig, const F32* v)
        { sig.addRel(v[0], 0.01f, 1e-7f); };
        const F32 base[3] = { 1.0e-4f, 0.f, 0.f };
        const F32 changed[3] = { 1.1e-4f, 0.f, 0.f };
        const F32 noise[3] = { 0.004f * 1.0e-4f, 0.f, 0.f };
        checkFieldTolerance("densityMultiplier", add, base, changed, noise);
    }
    {   // water height Abs(0.01) at 20 m: +0.02 vs +-0.004
        const SigAdder add = [](lpr::Signature& sig, const F32* v)
        { sig.addAbs(v[0], 0.01f); };
        const F32 base[3] = { 20.f, 0.f, 0.f };
        const F32 changed[3] = { 20.02f, 0.f, 0.f };
        const F32 noise[3] = { 0.004f, 0.f, 0.f };
        checkFieldTolerance("water height", add, base, changed, noise);
    }
    {   // sun direction Ang(0.1 deg): rotated 0.2 deg vs +-0.04 deg (about Y)
        const SigAdder add = [](lpr::Signature& sig, const F32* v)
        { sig.addAngle(v, 0.1f); };
        const F64 deg_to_rad = 0.017453292519943295;
        // This row builds explicit unit directions rotated about Y.
        const auto direction = [deg_to_rad](F64 degrees, F32* out)
        {
            const F64 a = degrees * deg_to_rad;
            out[0] = static_cast<F32>(0.6 * std::cos(a) + 0.8 * std::sin(a));
            out[1] = 0.f;
            out[2] = static_cast<F32>(-0.6 * std::sin(a) + 0.8 * std::cos(a));
        };
        F32 base[3];
        F32 changed[3];
        direction(0.0, base);
        direction(0.2, changed);
        lpr::StickyHash noise_sticky;
        const U64 h0 = sampleSticky(noise_sticky, add, base);
        for (S32 i = 0; i < 1000; ++i)
        {
            F32 v[3];
            direction(0.04 * static_cast<F64>(lprNoise(i)), v);
            ensure("sun direction noise of 0.04 deg must not move H",
                sampleSticky(noise_sticky, add, v) == h0);
        }
        lpr::StickyHash change_sticky;
        const U64 hb = sampleSticky(change_sticky, add, base);
        ensure("sun direction 0.2 deg rotation must move H",
            sampleSticky(change_sticky, add, changed) != hb);
    }
    {   // colour Col(0.005, 1e-4): +1 % in one channel at 1.0 vs +-0.2 %
        const SigAdder add = [](lpr::Signature& sig, const F32* v)
        { sig.addColor(v, 0.005f, 1e-4f); };
        const F32 base[3] = { 1.f, 0.5f, 0.25f };
        const F32 changed[3] = { 1.01f, 0.5f, 0.25f };
        const F32 noise[3] = { 0.002f, 0.001f, 0.0005f };
        checkFieldTolerance("colour +1% at 1.0", add, base, changed, noise);
    }
    {   // colour Col(0.005, 1e-4): 0 -> 3e-4 vs +-5e-5
        const SigAdder add = [](lpr::Signature& sig, const F32* v)
        { sig.addColor(v, 0.005f, 1e-4f); };
        const F32 base[3] = { 0.f, 0.f, 0.f };
        const F32 changed[3] = { 3e-4f, 0.f, 0.f };
        const F32 noise[3] = { 5e-5f, 5e-5f, 5e-5f };
        checkFieldTolerance("colour 0 -> 3e-4", add, base, changed, noise);
    }
    {   // emitter position Abs(0.05): +0.06 m vs +-0.02 m
        const SigAdder add = [](lpr::Signature& sig, const F32* v)
        { sig.addAbs3(v, 0.05f); };
        const F32 base[3] = { 10.f, 20.f, 30.f };
        const F32 changed[3] = { 10.06f, 20.f, 30.f };
        const F32 noise[3] = { 0.02f, 0.02f, 0.02f };
        checkFieldTolerance("emitter position", add, base, changed, noise);
    }
    {   // emitter scale Abs(0.005): +0.01 vs +-0.002
        const SigAdder add = [](lpr::Signature& sig, const F32* v)
        { sig.addAbs3(v, 0.005f); };
        const F32 base[3] = { 0.1f, 0.1f, 0.1f };
        const F32 changed[3] = { 0.11f, 0.1f, 0.1f };
        const F32 noise[3] = { 0.002f, 0.002f, 0.002f };
        checkFieldTolerance("emitter scale", add, base, changed, noise);
    }
    {   // haze horizon Abs(1e-3): +0.003 vs +-0.0004
        const SigAdder add = [](lpr::Signature& sig, const F32* v)
        { sig.addAbs(v[0], 1e-3f); };
        const F32 base[3] = { 0.19f, 0.f, 0.f };
        const F32 changed[3] = { 0.193f, 0.f, 0.f };
        const F32 noise[3] = { 0.0004f, 0.f, 0.f };
        checkFieldTolerance("haze horizon", add, base, changed, noise);
    }
    {   // sun / moon scale Rel(0.005, 1e-3): +2 % vs +-0.2 %
        const SigAdder add = [](lpr::Signature& sig, const F32* v)
        { sig.addRel(v[0], 0.005f, 1e-3f); };
        const F32 base[3] = { 1.f, 0.f, 0.f };
        const F32 changed[3] = { 1.02f, 0.f, 0.f };
        const F32 noise[3] = { 0.002f, 0.f, 0.f };
        checkFieldTolerance("sun scale", add, base, changed, noise);
    }
}

// Manual request landing right after an irradiance pass keeps its reason on
// every pass it triggers (not relabelled "changed"), then converges.
template<> template<>
void cine_light_rig_model_object::test<70>()
{
    set_test_name("[LiveProbeRefresh] manual right after an irradiance pass keeps the MANUAL reason");
    LiveProbeSim sim;
    sim.mParams.mMode = lpr::Mode::ON_CHANGE;
    const U64 h = 21;
    sim.step(h);
    sim.step(h);
    sim.step(h); // irradiance pass ends (frame 3)
    ensure_equals("irradiance ended", sim.mIrrPubs, 1);
    sim.mState.mManual = true;
    ensure("request starts a pass",
        sim.step(h).mReason == lpr::Reason::MANUAL); // frame 4: radiance starts
    sim.step(h);
    sim.step(h); // radiance ends (frame 6), discarded by the request
    const lpr::Decision next = sim.step(h); // frame 7: irradiance
    ensure("follow-up pass runs", next.mPath == lpr::Path::BUDGET);
    ensure("follow-up pass keeps MANUAL", next.mReason == lpr::Reason::MANUAL);
    sim.step(h);
    sim.step(h);
    const lpr::Decision rad = sim.step(h); // frame 10: radiance
    ensure("second follow-up keeps MANUAL", rad.mReason == lpr::Reason::MANUAL);
    sim.step(h);
    sim.step(h);
    ensure("converged after the manual sequence", lpr::converged(sim.mState, h));
    ensure("idle afterwards", sim.step(h).mPath == lpr::Path::IDLE);
}

// Budget <-> animation handoff keeps strict irradiance / radiance alternation.
template<> template<>
void cine_light_rig_model_object::test<71>()
{
    set_test_name("[LiveProbeRefresh] budget -> animation handoff alternates pass kinds");
    const U64 h = 8;
    {   // After a completed budget irradiance pass the first FULL pass is radiance.
        LiveProbeSim sim;
        sim.mParams.mMode = lpr::Mode::ON_CHANGE;
        sim.step(h);
        sim.step(h);
        sim.step(h); // irradiance done
        sim.mAnimating = true;
        sim.step(h); // FULL
        ensure("first FULL pass after an irradiance pass is radiance",
            sim.mLog.back().mEnd == lpr::PassEnd::RADIANCE);
        ensure("the pair converged", lpr::converged(sim.mState, h));
    }
    {   // After a completed radiance pass the first FULL pass is irradiance, then
        // radiance again (the pair alternates across the handoff).
        LiveProbeSim sim;
        sim.mParams.mMode = lpr::Mode::ON_CHANGE;
        for (S32 f = 1; f <= 6; ++f)
        {
            sim.step(h);
        }
        ensure("budget pair converged", lpr::converged(sim.mState, h));
        sim.mAnimating = true;
        sim.step(h);
        ensure("first FULL pass after a radiance pass is irradiance",
            sim.mLog.back().mEnd == lpr::PassEnd::IRRADIANCE);
        sim.step(h);
        ensure("the next FULL pass is radiance",
            sim.mLog.back().mEnd == lpr::PassEnd::RADIANCE);
        ensure("converged again on the same H", lpr::converged(sim.mState, h));
    }
    {   // An interrupted partial pass does not count: the FULL pass repeats its kind.
        LiveProbeSim sim;
        sim.mParams.mMode = lpr::Mode::ON_CHANGE;
        sim.step(h);
        sim.step(h); // 4 of 6 faces of the first irradiance pass
        sim.mAnimating = true;
        sim.step(h);
        ensure("an abandoned pass does not advance the cursor",
            sim.mLog.back().mEnd == lpr::PassEnd::IRRADIANCE);
    }
}

// ---------------------------------------------------------------------------
// [ProbeOnDemand] Model tests S1-S18 (test<72>..test<89>).
// They prove only the pure scheduling / dirtiness / debounce / barrier /
// signature model (alprobeschedule.h) on synthetic input. Not provable here:
// the viewer hooks, the real frame order and the in-world T0-T58 checks.
// Times use multiples of 1/64 s so boundaries are exact.
// ---------------------------------------------------------------------------
namespace aps = ALProbeSched;

namespace
{
struct PodBox
{
    F32 mMin[3];
    F32 mMax[3];
    PodBox(F32 x0, F32 x1, F32 y = 0.f, F32 z = 0.f)
    {
        mMin[0] = x0;
        mMin[1] = y;
        mMin[2] = z;
        mMax[0] = x1;
        mMax[1] = y + 1.f;
        mMax[2] = z + 1.f;
    }
};

F64 podTime(S32 frame)
{
    return static_cast<F64>(frame) / 64.0;
}

aps::Event podEvent(F32 x0, F32 x1, U16 reason, U8 cls, U64 first_serial)
{
    const PodBox b(x0, x1);
    return aps::makeEvent(b.mMin, b.mMax, reason, cls, first_serial);
}

// Note one motion sample for key `key` on a debounce map.
void podNote(aps::DebounceMap& map, const void* key, U64 tag, F32 x0, F32 x1,
             F64 now, F32 dt, U64 serial)
{
    const PodBox b(x0, x1);
    map.onMotion(key, static_cast<U8>(aps::Motion::XFORM), tag, b.mMin, b.mMax,
                 static_cast<U16>(aps::R_GEOM), static_cast<U8>(aps::C_STATIC), false,
                 serial, now, dt);
}

U32 podCountSettles(const std::vector<aps::Event>& events)
{
    U32 n = 0;
    for (const aps::Event& e : events)
    {
        if (e.mReason & aps::R_SETTLE)
        {
            ++n;
        }
    }
    return n;
}

// A light sample at x with default photometry.
aps::LightSample podLight(S32 id_byte, F32 x, F32 radius)
{
    aps::LightSample s;
    s.mId.mData[0] = static_cast<U8>(id_byte);
    s.mEligible = true;
    s.mPos[0] = x;
    s.mPos[1] = 5.f;
    s.mPos[2] = 3.f;
    s.mColor[0] = 1.f;
    s.mColor[1] = 0.5f;
    s.mColor[2] = 0.25f;
    s.mRadius = radius;
    s.mFalloff = 0.5f;
    s.mScale[0] = 0.1f;
    s.mScale[1] = 0.1f;
    s.mScale[2] = 0.1f;
    return s;
}

// Distinct, non-trivial environment values.
aps::EnvSample podEnv()
{
    aps::EnvSample e;
    aps::SkySample& s = e.mSky;
    s.mValid = true;
    F32 v = 0.11f;
    aps::V3* vecs[11] = { &s.mSunDir, &s.mMoonDir, &s.mSunlight, &s.mMoonlight,
        &s.mCloudColor, &s.mAmbient, &s.mBlueDensity, &s.mBlueHorizon, &s.mGlow,
        &s.mCloudPosDensity1, &s.mCloudPosDensity2 };
    for (aps::V3* p : vecs)
    {
        for (S32 c = 0; c < 3; ++c)
        {
            p->mV[c] = v;
            v += 0.07f;
        }
    }
    s.mHazeDensity = 0.31f;
    s.mDensityMultiplier = 0.32f;
    s.mDistanceMultiplier = 0.33f;
    s.mCloudScale = 0.34f;
    s.mStarBrightness = 0.35f;
    s.mDropletRadius = 0.36f;
    s.mMaxY = 4000.f;
    s.mHazeHorizon = 0.41f;
    s.mCloudShadow = 0.42f;
    s.mCloudVariance = 0.43f;
    s.mMoonBrightness = 0.44f;
    s.mMoistureLevel = 0.45f;
    s.mIceLevel = 0.46f;
    s.mReflectionAmbiance = 0.47f;
    s.mGamma = 2.2f;
    s.mSunMoonGlowFactor = 0.48f;
    s.mSunScale = 1.5f;
    s.mMoonScale = 1.25f;
    s.mBlendFactor = 0.5f;
    s.mIsSunUp = true;
    s.mSunTex.mData[0] = 1;
    s.mMoonTex.mData[0] = 2;
    s.mCloudNoiseTex.mData[0] = 3;
    s.mBloomTex.mData[0] = 4;
    s.mRainbowTex.mData[0] = 5;
    s.mHaloTex.mData[0] = 6;
    s.mNextSunTex.mData[0] = 7;
    s.mNextMoonTex.mData[0] = 8;
    s.mNextCloudNoiseTex.mData[0] = 9;

    aps::WaterSample& w = e.mWater;
    w.mValid = true;
    w.mFogColor.mV[0] = 0.1f;
    w.mFogColor.mV[1] = 0.2f;
    w.mFogColor.mV[2] = 0.3f;
    w.mFogDensity = 0.51f;
    w.mFogMod = 0.52f;
    w.mFresnelScale = 0.53f;
    w.mFresnelOffset = 0.54f;
    w.mScaleAbove = 0.55f;
    w.mScaleBelow = 0.56f;
    w.mBlendFactor = 0.57f;
    w.mBlurMultiplier = 0.58f;
    w.mWave1[0] = 0.61f;
    w.mWave1[1] = 0.62f;
    w.mWave2[0] = 0.63f;
    w.mWave2[1] = 0.64f;
    w.mNormalScale.mV[0] = 2.f;
    w.mNormalScale.mV[1] = 3.f;
    w.mNormalScale.mV[2] = 4.f;
    w.mRenderWaterHeight = 20.f;
    w.mNormalMap.mData[0] = 11;
    w.mNextNormalMap.mData[0] = 12;
    w.mTransparent.mData[0] = 13;
    w.mNextTransparent.mData[0] = 14;

    aps::EnvSettings& t = e.mSet;
    t.mSunEV = 0.7f;
    t.mMoonEV = -0.3f;
    t.mLocalLightEV = 0.2f;
    t.mShadowLiftEV = 0.1f;
    t.mSunKelvin = 5600.f;
    t.mSunTint[0] = 0.9f;
    t.mSunTint[1] = 0.8f;
    t.mSunTint[2] = 0.7f;
    t.mMoonTint[0] = 0.6f;
    t.mMoonTint[1] = 0.5f;
    t.mMoonTint[2] = 0.4f;
    t.mSunTintStrength = 0.95f;
    t.mMoonTintStrength = 0.85f;
    t.mMoonLinked = false;
    t.mLocalLightIncludeRig = true;
    t.mShadowDetail = 2;
    t.mLocalLightCount = 128;
    t.mAutoAdjustLegacy = true;
    return e;
}

// An independent copy of the Live probe's H sections 3-5 as they stood at HEAD
// (llreflectionmapmanager.cpp, sampleCinematicH) -- written out literally so
// the extracted builder is compared against the original statement order.
void podReferenceHeadEnv(lpr::Signature& sig, const aps::EnvSample& e)
{
    const aps::SkySample& sky = e.mSky;
    sig.addExact(true);
    sig.addAngle(sky.mSunDir.mV, 0.1f);
    sig.addAngle(sky.mMoonDir.mV, 0.1f);
    sig.addColor(sky.mSunlight.mV, 0.005f, 1e-4f);
    sig.addColor(sky.mMoonlight.mV, 0.005f, 1e-4f);
    sig.addColor(sky.mCloudColor.mV, 0.005f, 1e-4f);
    sig.addColor(sky.mAmbient.mV, 0.005f, 1e-4f);
    sig.addColor(sky.mBlueDensity.mV, 0.005f, 1e-4f);
    sig.addColor(sky.mBlueHorizon.mV, 0.005f, 1e-4f);
    sig.addColor(sky.mGlow.mV, 0.005f, 1e-4f);
    sig.addColor(sky.mCloudPosDensity1.mV, 0.005f, 1e-3f);
    sig.addColor(sky.mCloudPosDensity2.mV, 0.005f, 1e-3f);
    sig.addRel(sky.mHazeDensity, 0.005f, 1e-4f);
    sig.addRel(sky.mDensityMultiplier, 0.01f, 1e-7f);
    sig.addRel(sky.mDistanceMultiplier, 0.005f, 1e-3f);
    sig.addRel(sky.mCloudScale, 0.005f, 1e-4f);
    sig.addRel(sky.mStarBrightness, 0.005f, 1e-3f);
    sig.addRel(sky.mDropletRadius, 0.005f, 0.01f);
    sig.addRel(sky.mMaxY, 0.005f, 1.f);
    sig.addAbs(sky.mHazeHorizon, 1e-3f);
    sig.addAbs(sky.mCloudShadow, 1e-3f);
    sig.addAbs(sky.mCloudVariance, 1e-3f);
    sig.addAbs(sky.mMoonBrightness, 1e-3f);
    sig.addAbs(sky.mMoistureLevel, 1e-3f);
    sig.addAbs(sky.mIceLevel, 1e-3f);
    sig.addAbs(sky.mReflectionAmbiance, 1e-3f);
    sig.addAbs(sky.mGamma, 1e-3f);
    sig.addAbs(sky.mSunMoonGlowFactor, 1e-3f);
    sig.addRel(sky.mSunScale, 0.005f, 1e-3f);
    sig.addRel(sky.mMoonScale, 0.005f, 1e-3f);
    sig.addAbs(sky.mBlendFactor, 1e-3f);
    sig.addExact(sky.mIsSunUp);
    sig.addExact(sky.mSunTex);
    sig.addExact(sky.mMoonTex);
    sig.addExact(sky.mCloudNoiseTex);
    sig.addExact(sky.mBloomTex);
    sig.addExact(sky.mRainbowTex);
    sig.addExact(sky.mHaloTex);
    sig.addExact(sky.mNextSunTex);
    sig.addExact(sky.mNextMoonTex);
    sig.addExact(sky.mNextCloudNoiseTex);

    const aps::WaterSample& water = e.mWater;
    sig.addExact(true);
    sig.addColor(water.mFogColor.mV, 0.005f, 1e-4f);
    sig.addRel(water.mFogDensity, 0.005f, 1e-4f);
    sig.addAbs(water.mFogMod, 1e-3f);
    sig.addAbs(water.mFresnelScale, 1e-3f);
    sig.addAbs(water.mFresnelOffset, 1e-3f);
    sig.addAbs(water.mScaleAbove, 1e-3f);
    sig.addAbs(water.mScaleBelow, 1e-3f);
    sig.addAbs(water.mBlendFactor, 1e-3f);
    sig.addAbs(water.mBlurMultiplier, 1e-4f);
    sig.addAbs(water.mWave1[0], 1e-3f);
    sig.addAbs(water.mWave1[1], 1e-3f);
    sig.addAbs(water.mWave2[0], 1e-3f);
    sig.addAbs(water.mWave2[1], 1e-3f);
    sig.addAbs3(water.mNormalScale.mV, 1e-3f);
    sig.addAbs(water.mRenderWaterHeight, 0.01f);
    sig.addExact(water.mNormalMap);
    sig.addExact(water.mNextNormalMap);
    sig.addExact(water.mTransparent);
    sig.addExact(water.mNextTransparent);

    const aps::EnvSettings& st = e.mSet;
    sig.addAbs(st.mSunEV, 0.01f);
    sig.addAbs(st.mMoonEV, 0.01f);
    sig.addAbs(st.mLocalLightEV, 0.01f);
    sig.addAbs(st.mShadowLiftEV, 0.01f);
    sig.addAbs(st.mSunKelvin, 10.f);
    sig.addAbs3(st.mSunTint, 1.f / 512.f);
    sig.addAbs3(st.mMoonTint, 1.f / 512.f);
    sig.addAbs(st.mSunTintStrength, 1e-3f);
    sig.addAbs(st.mMoonTintStrength, 1e-3f);
    sig.addExact(st.mMoonLinked);
    sig.addExact(st.mLocalLightIncludeRig);
    sig.addExact(static_cast<U64>(static_cast<U32>(st.mShadowDetail)));
    sig.addExact(static_cast<U64>(static_cast<U32>(st.mLocalLightCount)));
    sig.addExact(st.mAutoAdjustLegacy);
}

struct PodRow
{
    lpr::Tol mTol;
    U8 mCount;
    F32 mA;
    F32 mB;
};

// The Live probe H layout of sections 3-5, field by field (what HEAD emits).
std::vector<PodRow> podExpectedEnvLayout(F32 sun_moon_deg)
{
    std::vector<PodRow> rows;
    const lpr::Tol angle = lpr::Tol::ANGLE;
    const lpr::Tol color = lpr::Tol::COLOR;
    const lpr::Tol rel = lpr::Tol::REL;
    const lpr::Tol absv = lpr::Tol::ABS;
    // sky: 2 angles (6 floats), 9 colours (27), 7 rel, 9 abs, 2 rel, 1 abs
    // = 6 + 27 + 7 + 9 + 2 + 1 = 52 floats (the brief's "50" is a miscount of
    // HEAD :786-817)
    rows.push_back({ angle, 3, sun_moon_deg, 0.f });
    rows.push_back({ angle, 3, sun_moon_deg, 0.f });
    for (S32 i = 0; i < 7; ++i)
    {
        rows.push_back({ color, 3, 0.005f, 1e-4f });
    }
    rows.push_back({ color, 3, 0.005f, 1e-3f });
    rows.push_back({ color, 3, 0.005f, 1e-3f });
    rows.push_back({ rel, 1, 0.005f, 1e-4f });
    rows.push_back({ rel, 1, 0.01f, 1e-7f });
    rows.push_back({ rel, 1, 0.005f, 1e-3f });
    rows.push_back({ rel, 1, 0.005f, 1e-4f });
    rows.push_back({ rel, 1, 0.005f, 1e-3f });
    rows.push_back({ rel, 1, 0.005f, 0.01f });
    rows.push_back({ rel, 1, 0.005f, 1.f });
    for (S32 i = 0; i < 9; ++i)
    {
        rows.push_back({ absv, 1, 1e-3f, 0.f });
    }
    rows.push_back({ rel, 1, 0.005f, 1e-3f });
    rows.push_back({ rel, 1, 0.005f, 1e-3f });
    rows.push_back({ absv, 1, 1e-3f, 0.f }); // sky blend factor
    // water: 19 floats
    rows.push_back({ color, 3, 0.005f, 1e-4f });
    rows.push_back({ rel, 1, 0.005f, 1e-4f });
    for (S32 i = 0; i < 5; ++i)
    {
        rows.push_back({ absv, 1, 1e-3f, 0.f });
    }
    rows.push_back({ absv, 1, 1e-3f, 0.f }); // water blend factor
    rows.push_back({ absv, 1, 1e-4f, 0.f });
    for (S32 i = 0; i < 4 + 3; ++i)
    {
        rows.push_back({ absv, 1, 1e-3f, 0.f });
    }
    rows.push_back({ absv, 1, 0.01f, 0.f });
    // settings: 13 floats
    for (S32 i = 0; i < 4; ++i)
    {
        rows.push_back({ absv, 1, 0.01f, 0.f });
    }
    rows.push_back({ absv, 1, 10.f, 0.f });
    for (S32 i = 0; i < 6; ++i)
    {
        rows.push_back({ absv, 1, 1.f / 512.f, 0.f });
    }
    rows.push_back({ absv, 1, 1e-3f, 0.f });
    rows.push_back({ absv, 1, 1e-3f, 0.f });
    return rows;
}

// Everything a Live ON-path H sample needs, from a set of light entries.
struct PodLiveH
{
    lpr::StickyHash mSticky;
    lpr::Signature mSig;
    aps::LiveHInput mIn;
    aps::EnvSample mEnv;
    std::vector<aps::LiveToken> mTokens;

    PodLiveH() : mEnv(podEnv()) {}

    U64 sample(const std::vector<const aps::LightEntry*>& entries)
    {
        mSig.clear();
        mTokens.clear();
        for (const aps::LightEntry* e : entries)
        {
            mTokens.push_back(aps::liveTokenOf(*e));
        }
        aps::buildLiveOnSignature(mSig, mIn, mTokens, mEnv);
        return mSticky.update(mSig);
    }
};
}

// S1
template<> template<>
void cine_light_rig_model_object::test<72>()
{
    set_test_name("[ProbeOnDemand] S1 an event mid-transaction stays dirty after completion");
    aps::Record r;
    r.mId = 1;
    aps::hit(r, aps::R_GEOM, 3, 1.0);
    ensure("dirty before the start", aps::dirty(r));
    aps::onTxnStart(r, 5, 1, 2.0, aps::R_GEOM, 4);
    aps::onTxnIrradianceDone(r, 1);
    aps::hit(r, aps::R_TEX, 6, 2.5);
    ensure("the pair is acked", aps::onTxnComplete(r, 1, 4, 3.0, 7));
    ensure("the mid-transaction event survives the ack", aps::dirty(r));
    ensure_equals("only the new reason stays pending",
        static_cast<S32>(r.mPending), static_cast<S32>(aps::R_TEX));
    ensure_equals("its first-dirty time is kept", r.mFirstDirty, 2.5);
    ensure_equals("ack serial = transaction serial", r.mAckSerial, static_cast<U64>(5));
    ensure_equals("the acked reasons are reported", static_cast<S32>(r.mLastReasons),
        static_cast<S32>(aps::R_GEOM));
}

// S2
template<> template<>
void cine_light_rig_model_object::test<73>()
{
    set_test_name("[ProbeOnDemand] S2 an event noted in the transaction's own frame is acked");
    aps::Record r;
    r.mId = 2;
    aps::hit(r, aps::R_GEOM, 5, 1.0);
    aps::onTxnStart(r, 5, 1, 1.0, aps::R_GEOM, 3);
    aps::onTxnIrradianceDone(r, 1);
    ensure("acked", aps::onTxnComplete(r, 1, 3, 2.0, 9));
    ensure("a same-frame pre-start event does not survive", !aps::dirty(r));
    ensure_equals("pending cleared", static_cast<S32>(r.mPending), 0);
    ensure("first-dirty cleared", r.mFirstDirty < 0.0);
    ensure_equals("complete time recorded", r.mLastComplete, 2.0);
}

// S3
template<> template<>
void cine_light_rig_model_object::test<74>()
{
    set_test_name("[ProbeOnDemand] S3 MinInterval keeps a dirty probe pending");
    aps::Record r;
    r.mId = 3;
    r.mLastStart = 9.0;
    r.mLastComplete = 9.5;
    aps::Policy p;
    p.mMinInterval = 1.f;
    p.mMaxAge = 60.f;
    aps::View v;
    v.mAllocated = true;
    v.mComplete = true;
    aps::hit(r, aps::R_GEOM, 4, 9.6);
    ensure("inside the interval a dirty probe waits",
        aps::evaluate(r, v, p, 9.9) == aps::Why::WAIT_INTERVAL);
    ensure("the interval boundary releases it",
        aps::evaluate(r, v, p, 10.0) == aps::Why::DIRTY);
    ensure("DIRTY is eligible", aps::eligible(aps::Why::DIRTY));
    ensure("WAIT_INTERVAL is not eligible", !aps::eligible(aps::Why::WAIT_INTERVAL));

    aps::Record clean;
    clean.mLastStart = 9.0;
    clean.mLastComplete = 9.5;
    ensure("inside the interval a clean probe has nothing to do",
        aps::evaluate(clean, v, p, 9.9) == aps::Why::NONE);
    aps::View dyn = v;
    dyn.mDynamic = true;
    ensure("a dynamic probe waits too",
        aps::evaluate(clean, dyn, p, 9.9) == aps::Why::WAIT_INTERVAL);
    ensure("then runs as DYNAMIC", aps::evaluate(clean, dyn, p, 10.0) == aps::Why::DYNAMIC);
    ensure("an unallocated probe is never scheduled",
        aps::evaluate(r, aps::View(), p, 10.0) == aps::Why::NONE);
    aps::View incomplete;
    incomplete.mAllocated = true;
    ensure("an incomplete probe is WARMUP (ignores the interval)",
        aps::evaluate(r, incomplete, p, 9.9) == aps::Why::WARMUP);
}

// S4
template<> template<>
void cine_light_rig_model_object::test<75>()
{
    set_test_name("[ProbeOnDemand] S4 safety fires at MaxAge; MaxAge cannot be disabled");
    aps::Record r;
    r.mId = 4;
    r.mLastStart = 0.0;
    r.mLastComplete = 0.0;
    aps::View v;
    v.mAllocated = true;
    v.mComplete = true;
    const aps::Policy p = aps::policyFromSettings(1.f, 10.f);
    ensure_equals("MaxAge 10 kept", p.mMaxAge, 10.f);
    ensure("not due just before MaxAge",
        aps::evaluate(r, v, p, 9.99) == aps::Why::NONE);
    ensure("due at MaxAge", aps::evaluate(r, v, p, 10.0) == aps::Why::SAFETY);

    ensure_equals("a 0 MaxAge is clamped to 10 (no permanent disable)",
        aps::policyFromSettings(1.f, 0.f).mMaxAge, 10.f);
    ensure_equals("a negative MaxAge is clamped to 10",
        aps::policyFromSettings(1.f, -5.f).mMaxAge, 10.f);
    ensure_equals("MaxAge is capped at 600", aps::policyFromSettings(1.f, 9999.f).mMaxAge, 600.f);
    ensure_equals("a NaN MaxAge falls back to 60",
        aps::policyFromSettings(1.f, std::numeric_limits<F32>::quiet_NaN()).mMaxAge, 60.f);
    ensure_equals("MinInterval is capped at 10",
        aps::policyFromSettings(50.f, 60.f).mMinInterval, 10.f);
    ensure_equals("MinInterval below 0 is 0",
        aps::policyFromSettings(-1.f, 60.f).mMinInterval, 0.f);
    // A complete probe that never completed a transaction gets grace, not an
    // immediate safety refresh.
    aps::Record fresh;
    aps::graceInit(fresh, true, 100.0);
    ensure_equals("grace-init stamps the completion time", fresh.mLastComplete, 100.0);
    ensure("no immediate safety",
        aps::evaluate(fresh, v, p, 100.0) == aps::Why::NONE);
}

// S5
template<> template<>
void cine_light_rig_model_object::test<76>()
{
    set_test_name("[ProbeOnDemand] S5 DEFERRED keeps dirt; BARRIER overrides occlusion and interval");
    aps::Record r;
    r.mLastStart = 9.5;
    r.mLastComplete = 9.6;
    aps::Policy p;
    aps::View v;
    v.mAllocated = true;
    v.mComplete = true;
    v.mOccluded = true;
    ensure("occluded + clean + not due: nothing",
        aps::evaluate(r, v, p, 10.0) == aps::Why::NONE);
    aps::hit(r, aps::R_GEOM, 3, 9.7);
    ensure("occluded + dirty: DEFERRED", aps::evaluate(r, v, p, 10.0) == aps::Why::DEFERRED);
    ensure("DEFERRED is not eligible", !aps::eligible(aps::Why::DEFERRED));
    ensure("the dirt is kept while deferred", aps::dirty(r));
    aps::Record due;
    due.mLastComplete = 0.0;
    ensure("occluded + safety due: DEFERRED",
        aps::evaluate(due, v, p, 100.0) == aps::Why::DEFERRED);
    aps::View barrier = v;
    barrier.mBarrier = true;
    ensure("BARRIER overrides occlusion and the interval (recent start)",
        aps::evaluate(r, barrier, p, 10.0) == aps::Why::BARRIER);
    ensure("BARRIER is eligible", aps::eligible(aps::Why::BARRIER));
    aps::View incomplete = barrier;
    incomplete.mComplete = false;
    ensure("an incomplete member is still WARMUP first",
        aps::evaluate(r, incomplete, p, 10.0) == aps::Why::WARMUP);
}

// S6
template<> template<>
void cine_light_rig_model_object::test<77>()
{
    set_test_name("[ProbeOnDemand] S6 footprint: sphere, terrain global, capsule on miss, masks, face-op skip");
    const F32 origin[3] = { 0.f, 0.f, 0.f };
    const F32 rcap = 131.f;
    const U8 ordinary = static_cast<U8>(aps::C_STATIC | aps::C_TERRAIN_WATER | aps::C_LIGHT | aps::C_GLOBAL);
    const U8 dflt = static_cast<U8>(aps::C_TERRAIN_WATER | aps::C_LIGHT | aps::C_GLOBAL);
    aps::Record rec;
    rec.mRecountDue = false;
    aps::ShadowDirs off;
    aps::ShadowDirs on;
    on.mEnabled = true;
    on.mSunValid = true;
    on.mSun[0] = 1.f;
    on.mSun[1] = 0.f;
    on.mSun[2] = 0.f; // sun on the +x horizon: shadows fall along -x
    const S32 cap = 256;

    ensure("a static event inside the sphere hits",
        aps::hitsCapture(podEvent(50.f, 51.f, aps::R_GEOM, aps::C_STATIC, 1), rec, origin, rcap, ordinary, cap, off));
    ensure("a static event outside the sphere misses (no shadows)",
        !aps::hitsCapture(podEvent(300.f, 301.f, aps::R_GEOM, aps::C_STATIC, 1), rec, origin, rcap, ordinary, cap, off));
    ensure("terrain / water edits are global",
        aps::hitsCapture(podEvent(5000.f, 5001.f, aps::R_GEOM, aps::C_TERRAIN_WATER, 1), rec, origin, rcap, ordinary, cap, off));
    ensure("a global event always hits",
        aps::hitsCapture(podEvent(5000.f, 5001.f, aps::R_ENV, aps::C_GLOBAL, 1), rec, origin, rcap, dflt, cap, off));

    // capsule only on a sphere miss: an object at +x whose shadow falls onto
    // the probe's capture.
    const aps::Event shadow_hit = podEvent(200.f, 201.f, aps::R_GEOM, aps::C_STATIC, 1);
    const aps::Event shadow_miss = podEvent(-201.f, -200.f, aps::R_GEOM, aps::C_STATIC, 1);
    ensure("without shadows the outside object misses",
        !aps::hitsCapture(shadow_hit, rec, origin, rcap, ordinary, cap, off));
    ensure("its sun shadow reaches the capture",
        aps::hitsCapture(shadow_hit, rec, origin, rcap, ordinary, cap, on));
    ensure("an object whose shadow falls away misses",
        !aps::hitsCapture(shadow_miss, rec, origin, rcap, ordinary, cap, on));
    aps::ShadowDirs moon_only;
    moon_only.mEnabled = true;
    moon_only.mMoonValid = true;
    moon_only.mMoon[0] = 1.f;
    moon_only.mMoon[1] = 0.f;
    moon_only.mMoon[2] = 0.f;
    ensure("the moon capsule works the same way",
        aps::hitsCapture(shadow_hit, rec, origin, rcap, ordinary, cap, moon_only));

    // class masks
    ensure("the default probe does not render static prims",
        !aps::hitsCapture(podEvent(50.f, 51.f, aps::R_GEOM, aps::C_STATIC, 1), rec, origin, rcap, dflt, cap, on));
    ensure("the default probe does render lights",
        aps::hitsCapture(podEvent(50.f, 51.f, aps::R_LIGHT, aps::C_LIGHT, 1), rec, origin, rcap, dflt, cap, off));
    ensure("a distant light misses an under-cap probe",
        !aps::hitsCapture(podEvent(900.f, 901.f, aps::R_LIGHT, aps::C_LIGHT, 1), rec, origin, rcap, ordinary, cap, off));

    // face-op skip
    aps::Event blurred = podEvent(50.f, 51.f, aps::R_TEX, aps::C_STATIC, 1);
    blurred.mMinFaceOp = 10;
    rec.mLastFaceOp = 10;
    ensure("a probe that rendered at (not after) the blur op is skipped",
        !aps::hitsCapture(blurred, rec, origin, rcap, ordinary, cap, off));
    rec.mLastFaceOp = 11;
    ensure("a probe that rendered after the blur is hit",
        aps::hitsCapture(blurred, rec, origin, rcap, ordinary, cap, off));
    rec.mLastFaceOp = 0;
    ensure("a probe that never rendered is skipped",
        !aps::hitsCapture(blurred, rec, origin, rcap, ordinary, cap, off));
}

// S7
template<> template<>
void cine_light_rig_model_object::test<78>()
{
    set_test_name("[ProbeOnDemand] S7 transaction contract: refusals, newer serials, overflow");
    {   // no transaction at all
        aps::Record r;
        ensure("no transaction: nack", !aps::onTxnComplete(r, 1, 3, 1.0, 5));
        ensure("a nack leaves the probe dirty (resync)", aps::dirty(r));
        ensure("with an R_RESYNC reason", (r.mPending & aps::R_RESYNC) != 0);
    }
    {   // irradiance never reported
        aps::Record r;
        aps::onTxnStart(r, 5, 1, 1.0, aps::R_GEOM, 3);
        ensure("no irradiance pass: nack", !aps::onTxnComplete(r, 1, 3, 2.0, 6));
        ensure("the transaction is dropped", !r.mInTxn);
        ensure("dirty again", aps::dirty(r));
        ensure("the reasons covered by the dropped transaction come back",
            (r.mPending & aps::R_GEOM) != 0);
    }
    {   // epoch mismatch
        aps::Record r;
        aps::onTxnStart(r, 5, 1, 1.0, 0, 3);
        aps::onTxnIrradianceDone(r, 1);
        ensure("epoch mismatch: nack", !aps::onTxnComplete(r, 2, 3, 2.0, 6));
    }
    {   // a stale irradiance-done report from another epoch is ignored
        aps::Record r;
        aps::onTxnStart(r, 5, 1, 1.0, 0, 3);
        aps::onTxnIrradianceDone(r, 9);
        ensure("a foreign-epoch irradiance report is ignored", !r.mTxnIrrDone);
    }
    {   // cube changed under the transaction
        aps::Record r;
        aps::onTxnStart(r, 5, 1, 1.0, 0, 3);
        aps::onTxnIrradianceDone(r, 1);
        ensure("cube mismatch: nack", !aps::onTxnComplete(r, 1, 4, 2.0, 6));
    }
    {   // the redo after a nack is acked and clears the resync
        aps::Record r;
        ensure("nack first", !aps::onTxnComplete(r, 1, 3, 1.0, 5));
        aps::onTxnStart(r, 6, 1, 2.0, aps::R_RESYNC, 3);
        aps::onTxnIrradianceDone(r, 1);
        ensure("the redo acks", aps::onTxnComplete(r, 1, 3, 3.0, 7));
        ensure("and the resync is gone (never an infinite refresh loop)", !aps::dirty(r));
    }
    {   // newer serials are never cleared by an ack
        aps::Record r;
        aps::onTxnStart(r, 5, 1, 1.0, 0, 3);
        aps::onTxnIrradianceDone(r, 1);
        aps::hit(r, aps::R_TEX, 8, 1.5);
        ensure("acked", aps::onTxnComplete(r, 1, 3, 2.0, 9));
        ensure("serial 8 > 5 survives", aps::dirty(r));
        ensure_equals("the dirty serial is intact", r.mDirtySerial, static_cast<U64>(8));
    }
    {   // an overflow-style resync hit mid-transaction never clears the transaction
        aps::Record r;
        aps::onTxnStart(r, 5, 1, 1.0, 0, 3);
        aps::hit(r, aps::R_RESYNC, 6, 1.2); // overflow marks everything dirty
        ensure("still in the transaction", r.mInTxn);
        aps::onTxnIrradianceDone(r, 1);
        ensure("the transaction still completes", aps::onTxnComplete(r, 1, 3, 2.0, 7));
        ensure("the overflow dirt remains", aps::dirty(r));
    }
}

// S8
template<> template<>
void cine_light_rig_model_object::test<79>()
{
    set_test_name("[ProbeOnDemand] S8 verdict priority and the UNSETTLED streak");
    aps::VerdictInput in;
    ensure("quiet is OK", aps::verdict(in) == aps::Verdict::OK);
    in.mStarvedCount = 3;
    in.mStarvedWorst = 7.0;
    ensure("starved", aps::verdict(in) == aps::Verdict::STARVED);
    in.mUnsettled = true;
    in.mUnsettledId = 12;
    in.mUnsettledReasons = static_cast<U16>(aps::R_GEOM | aps::R_TEX);
    ensure("UNSETTLED beats STARVED", aps::verdict(in) == aps::Verdict::UNSETTLED);
    ensure("the text names the probe and reasons",
        aps::verdictText(in) == "UNSETTLED id=12 reasons=GT");
    in.mPaused = true;
    ensure("PAUSED beats UNSETTLED", aps::verdict(in) == aps::Verdict::PAUSED);
    in.mOverBudget = true;
    ensure("OVER BUDGET beats everything", aps::verdict(in) == aps::Verdict::OVER_BUDGET);
    in.mOn = false;
    ensure("scheduler off is OFF-BASELINE", aps::verdict(in) == aps::Verdict::OFF_BASELINE);
    aps::VerdictInput starved;
    starved.mStarvedCount = 2;
    starved.mStarvedWorst = 6.4;
    ensure("STARVED text", aps::verdictText(starved) == "STARVED 2 6s");

    // streak: 3 consecutive windows with reasons outside S/W/R/B
    aps::Record r;
    ensure("window 1: not yet", !aps::noteStartWindow(r, 1, aps::R_GEOM));
    ensure("window 2: not yet", !aps::noteStartWindow(r, 2, aps::R_TEX));
    ensure("window 3: unsettled", aps::noteStartWindow(r, 3, aps::R_LIGHT));
    aps::Record gap;
    aps::noteStartWindow(gap, 1, aps::R_GEOM);
    aps::noteStartWindow(gap, 2, aps::R_GEOM);
    ensure("a skipped window restarts the streak", !aps::noteStartWindow(gap, 4, aps::R_GEOM));
    aps::Record benign;
    aps::noteStartWindow(benign, 1, aps::R_SAFETY);
    aps::noteStartWindow(benign, 2, aps::R_WARMUP);
    ensure("safety / warm-up / resync / barrier never build a streak",
        !aps::noteStartWindow(benign, 3, static_cast<U16>(aps::R_RESYNC | aps::R_BARRIER)));
    aps::Record broken;
    aps::noteStartWindow(broken, 1, aps::R_GEOM);
    aps::noteStartWindow(broken, 2, aps::R_GEOM);
    aps::noteStartWindow(broken, 3, aps::R_SAFETY);
    ensure("a benign window resets the streak", !aps::noteStartWindow(broken, 4, aps::R_GEOM));
    ensure("frame budget: ord 1 is fine",
        !aps::frameOverBudget(aps::SchedFrame{ 1, 0, 0, false }, 2));
    ensure("frame budget: ord 2 is over", aps::frameOverBudget(aps::SchedFrame{ 2, 0, 0, false }, 2));
    ensure("frame budget: rt 7 is over", aps::frameOverBudget(aps::SchedFrame{ 0, 7, 0, false }, 2));
    ensure("frame budget: sliced 3 > N 2 is over",
        aps::frameOverBudget(aps::SchedFrame{ 0, 3, 3, false }, 2));
    ensure("frame budget: sliced 2 <= N 2 is fine",
        !aps::frameOverBudget(aps::SchedFrame{ 0, 2, 2, false }, 2));
    ensure("reason letters", aps::reasonLetters(static_cast<U16>(aps::R_GEOM | aps::R_BARRIER)) == "GB");
}

// S9
template<> template<>
void cine_light_rig_model_object::test<80>()
{
    set_test_name("[ProbeOnDemand] S9 slice cursor: N faces, alternation, identity reset, blocked frame");
    aps::SliceCursor c;
    aps::sliceBind(c, 5, 7);
    // N = 2: three frames per pass, pass kinds alternate irradiance / radiance
    S32 frame = 0;
    std::vector<lpr::PassEnd> ends;
    std::vector<S32> end_frames;
    for (S32 f = 0; f < 12; ++f)
    {
        ++frame;
        aps::sliceBind(c, 5, 7);
        aps::sliceBegin(c, static_cast<U64>(frame));
        for (S32 n = 0; n < 2 && c.mActive; ++n)
        {
            const lpr::PassEnd end = aps::sliceAdvance(c);
            if (end != lpr::PassEnd::NONE)
            {
                ends.push_back(end);
                end_frames.push_back(frame);
            }
        }
    }
    ensure_equals("four passes in 12 frames at N=2", static_cast<S32>(ends.size()), 4);
    ensure("pass 1 irradiance", ends[0] == lpr::PassEnd::IRRADIANCE);
    ensure("pass 2 radiance", ends[1] == lpr::PassEnd::RADIANCE);
    ensure("pass 3 irradiance", ends[2] == lpr::PassEnd::IRRADIANCE);
    ensure("pass 4 radiance", ends[3] == lpr::PassEnd::RADIANCE);
    ensure_equals("pass 1 ends on frame 3", end_frames[0], 3);
    ensure_equals("pass 2 ends on frame 6", end_frames[1], 6);

    // N = 6 completes a pass per frame; N = 1 takes six frames
    aps::SliceCursor six;
    aps::sliceBind(six, 1, 1);
    aps::sliceBegin(six, 1);
    lpr::PassEnd last = lpr::PassEnd::NONE;
    for (S32 n = 0; n < 6 && six.mActive; ++n)
    {
        last = aps::sliceAdvance(six);
    }
    ensure("N=6 finishes the pass in one frame", last == lpr::PassEnd::IRRADIANCE);
    aps::sliceBegin(six, 2);
    ensure("and the next pass is radiance", six.mRadiance);

    // identity change drops the partial pass; the next pass starts at face 0
    aps::SliceCursor id;
    aps::sliceBind(id, 3, 4);
    aps::sliceBegin(id, 1);
    aps::sliceAdvance(id);
    aps::sliceAdvance(id);
    ensure_equals("mid-pass", static_cast<S32>(id.mFace), 2);
    aps::sliceBind(id, 9, 4);
    ensure("a new probe id resets the cursor", !id.mActive && id.mFace == 0 && id.mId == 9);
    aps::sliceBegin(id, 5);
    ensure("the pass restarts at face 0", id.mActive && id.mFace == 0);
    ensure_equals("and records its start serial", id.mPassStartSerial, static_cast<U64>(5));
    aps::sliceBind(id, 9, 5);
    ensure("a cube change resets as well", !id.mActive && id.mCube == 5);

    // a blocked frame (the probe is the ordinary updating probe) keeps the cursor
    aps::SliceCursor blocked;
    aps::sliceBind(blocked, 1, 1);
    aps::sliceBegin(blocked, 1);
    aps::sliceAdvance(blocked);
    aps::sliceAdvance(blocked);
    const U8 face_before = blocked.mFace;
    aps::sliceBind(blocked, 1, 1); // the manager still binds, but captures nothing
    ensure_equals("a blocked frame keeps the face", static_cast<S32>(blocked.mFace),
        static_cast<S32>(face_before));
    ensure("and the pass stays active", blocked.mActive);

    // Regression (T22): a partial sliced pass, then the Live probe writes the
    // secondary scratch, then the same dynamic probe is realtime again. The cursor
    // must NOT resume at face k on top of Live's faces; the pass restarts at face 0.
    aps::SliceCursor handoff;
    aps::sliceBind(handoff, 4, 2);
    aps::sliceBegin(handoff, 10);
    aps::sliceAdvance(handoff);
    aps::sliceAdvance(handoff);
    aps::sliceAdvance(handoff);
    ensure_equals("mid-pass at face 3", static_cast<S32>(handoff.mFace), 3);
    aps::SliceCursor without_fix = handoff;
    aps::sliceBind(without_fix, 4, 2);
    ensure("(without the invalidation the same id / cube would resume mid-pass)",
        without_fix.mActive && without_fix.mFace == 3);
    aps::sliceInvalidate(handoff); // Live took the scratch
    aps::sliceBind(handoff, 4, 2); // the same probe is realtime again
    ensure("the cursor is idle after the handoff", !handoff.mActive && handoff.mFace == 0);
    aps::sliceBegin(handoff, 20);
    ensure("a new pass starts at face 0", handoff.mActive && handoff.mFace == 0);
    ensure_equals("with a fresh start serial", handoff.mPassStartSerial, static_cast<U64>(20));
    ensure("and the pass kind restarts as irradiance", !handoff.mRadiance);
}

// S10
template<> template<>
void cine_light_rig_model_object::test<81>()
{
    set_test_name("[ProbeOnDemand] S10 grace-init and the ON-edge clearTxn");
    aps::Record never;
    aps::graceInit(never, false, 5.0);
    ensure("an incomplete probe gets no grace stamp", never.mLastComplete < 0.0);
    aps::graceInit(never, true, 5.0);
    ensure_equals("a complete probe is stamped", never.mLastComplete, 5.0);
    aps::graceInit(never, true, 99.0);
    ensure_equals("an existing stamp is never overwritten", never.mLastComplete, 5.0);

    // ON edge: any in-flight transaction from a previous ON period is dropped
    // and the record is resynced, so a late completion cannot ack it.
    aps::Record r;
    aps::onTxnStart(r, 5, 1, 1.0, aps::R_GEOM, 3);
    aps::onTxnIrradianceDone(r, 1);
    aps::clearTxn(r);
    aps::hit(r, aps::R_RESYNC, 6, 2.0);
    ensure("the stale completion is refused", !aps::onTxnComplete(r, 1, 3, 3.0, 7));
    ensure("and the probe is dirty", aps::dirty(r));
    // the redo
    aps::onTxnStart(r, 8, 1, 4.0, aps::R_RESYNC, 3);
    aps::onTxnIrradianceDone(r, 1);
    ensure("a fresh transaction acks", aps::onTxnComplete(r, 1, 3, 5.0, 9));
    ensure("and clears everything", !aps::dirty(r));
}

// S11
template<> template<>
void cine_light_rig_model_object::test<82>()
{
    set_test_name("[ProbeOnDemand] S11 ownership excludes LIVE / REALTIME records from selection");
    aps::Policy p;
    aps::View v;
    v.mAllocated = true;
    v.mComplete = true;
    const aps::Owner owners[3] = { aps::Owner::ORDINARY, aps::Owner::LIVE, aps::Owner::REALTIME };
    for (S32 i = 0; i < 3; ++i)
    {
        aps::Record r;
        r.mOwner = owners[i];
        r.mLastComplete = 100.0;
        aps::hit(r, aps::R_GEOM, 2, 100.5);
        ensure(i == 0 ? "an ORDINARY dirty record is selectable"
                      : "a LIVE / REALTIME record is never selected",
            aps::selectable(r, v, p, 101.0) == (i == 0));
    }
    aps::Record warm;
    warm.mOwner = aps::Owner::LIVE;
    aps::View incomplete;
    incomplete.mAllocated = true;
    ensure("even warm-up never selects a LIVE record",
        !aps::selectable(warm, incomplete, p, 1.0));
}

// S12
template<> template<>
void cine_light_rig_model_object::test<83>()
{
    set_test_name("[ProbeOnDemand] S12 texture notification rule (H6 / DS)");
    // integer Dp
    ensure_equals("1024 / 128 -> 3", aps::probeDiscardFloor(1024, 128), 3);
    ensure_equals("512 / 128 -> 2", aps::probeDiscardFloor(512, 128), 2);
    ensure_equals("128 / 128 -> 0", aps::probeDiscardFloor(128, 128), 0);
    ensure_equals("smaller than the probe -> 0", aps::probeDiscardFloor(100, 128), 0);
    ensure_equals("zero dimension -> 0", aps::probeDiscardFloor(0, 128), 0);
    ensure_equals("zero probe resolution -> 0", aps::probeDiscardFloor(1024, 0), 0);
    ensure_equals("non power of two uses integer shifts", aps::probeDiscardFloor(1000, 128), 2);

    const S32 dp = 3;
    aps::TexNote n;
    // the d < 0 guard
    ensure("d < 0 never notifies", !aps::texArrival(n, -1, dp).mNotify);
    ensure_equals("and changes nothing", static_cast<S32>(n.mNotedDiscard), -1);
    // first image
    aps::TexVerdict v = aps::texArrival(n, 5, dp);
    ensure("the first image notifies", v.mNotify);
    ensure_equals("unconditionally", v.mMinFaceOp, static_cast<U64>(0));
    ensure_equals("noted = 5", static_cast<S32>(n.mNotedDiscard), 5);
    // coarse refinement while still coarser than Dp
    ensure("5 -> 4 (noted > Dp) notifies", aps::texArrival(n, 4, dp).mNotify);
    ensure("4 -> 3 (noted > Dp) notifies", aps::texArrival(n, 3, dp).mNotify);
    // nothing beyond Dp
    ensure("3 -> 2 (noted == Dp) does not notify", !aps::texArrival(n, 2, dp).mNotify);
    ensure("2 -> 0 does not notify", !aps::texArrival(n, 0, dp).mNotify);
    ensure_equals("noted tracks the minimum", static_cast<S32>(n.mNotedDiscard), 0);
    // a downgrade never notifies
    ensure("a downgrade does not notify", !aps::texArrival(n, 5, dp).mNotify);
    ensure_equals("noted never rises", static_cast<S32>(n.mNotedDiscard), 0);

    // blur / re-sharpen, first stamp kept across repeated downscales
    aps::texDownscale(n, 5, dp, 100);
    ensure("a blur is recorded", n.mBlurred);
    ensure_equals("with its stamp", n.mBlurStamp, static_cast<U64>(100));
    aps::texDownscale(n, 6, dp, 200);
    ensure_equals("the FIRST outstanding stamp is kept", n.mBlurStamp, static_cast<U64>(100));
    aps::TexNote shallow;
    shallow.mNotedDiscard = 2;
    aps::texDownscale(shallow, 3, dp, 50); // 3 is not beyond Dp
    ensure("a downscale that stays within Dp is not a blur", !shallow.mBlurred);
    aps::TexNote unseen;
    aps::texDownscale(unseen, 9, dp, 1);
    ensure("a never-noted texture that is downscaled counts as blurred (conservative)", unseen.mBlurred);
    v = aps::texArrival(n, 2, dp);
    ensure("a re-sharpen notifies", v.mNotify);
    ensure_equals("carrying the blur stamp as the minimum face op", v.mMinFaceOp,
        static_cast<U64>(100));
    ensure("and clears the blur", !n.mBlurred);
    ensure("a second arrival at the same depth does not notify", !aps::texArrival(n, 2, dp).mNotify);

    // op ordering: a face captured before the downscale stamp is not exposed
    ensure("face op N, stamp N+1: not exposed", !aps::faceExposed(10, 11));
    ensure("stamp N, face op N+1: exposed", aps::faceExposed(12, 11));
    ensure("equal ops are not exposed", !aps::faceExposed(11, 11));
    aps::Record before;
    before.mLastFaceOp = 10;
    aps::Record after;
    after.mLastFaceOp = 12;
    aps::Event ev = podEvent(50.f, 51.f, aps::R_TEX, aps::C_STATIC, 1);
    ev.mMinFaceOp = 11;
    const F32 origin[3] = { 0.f, 0.f, 0.f };
    const U8 mask = static_cast<U8>(aps::C_STATIC | aps::C_GLOBAL | aps::C_LIGHT | aps::C_TERRAIN_WATER);
    const aps::ShadowDirs sd = aps::ShadowDirs();
    ensure("the pre-blur probe is not hit",
        !aps::hitsCapture(ev, before, origin, 131.f, mask, 256, sd));
    ensure("the post-blur probe is hit",
        aps::hitsCapture(ev, after, origin, 131.f, mask, 256, sd));
}

// S13
template<> template<>
void cine_light_rig_model_object::test<84>()
{
    set_test_name("[ProbeOnDemand] S13 environment builder: layout table + HEAD-order oracle");
    const aps::EnvSample env = podEnv();

    // (i) layout table
    lpr::Signature sig;
    aps::appendEnvironmentFields(sig, env, 0.1f, false);
    const std::vector<PodRow> rows = podExpectedEnvLayout(0.1f);
    ensure_equals("field count matches the table", static_cast<S32>(sig.mField.size()),
        static_cast<S32>(rows.size()));
    S32 floats = 0;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const lpr::Field& f = sig.mField[i];
        ensure("tolerance kind of field " + std::to_string(i), f.mTol == rows[i].mTol);
        ensure("count of field " + std::to_string(i), f.mCount == rows[i].mCount);
        ensure("a of field " + std::to_string(i), f.mA == rows[i].mA);
        ensure("b of field " + std::to_string(i), f.mB == rows[i].mB);
        floats += static_cast<S32>(f.mCount);
    }
    ensure_equals("52 sky + 19 water + 13 settings floats", floats, 84);
    ensure_equals("the value vector matches the field counts",
        static_cast<S32>(sig.mVal.size()), floats);

    // (ii) the H equals the original statement order on the same values
    lpr::Signature reference;
    podReferenceHeadEnv(reference, env);
    lpr::StickyHash sticky_a;
    lpr::StickyHash sticky_b;
    ensure("builder == HEAD-order oracle", sticky_a.update(sig) == sticky_b.update(reference));
    // The reviewer's independently computed golden H (a Python FNV re-implementation
    // of StickyHash::update over podEnv() in HEAD order: 60 fields, 84 floats). A
    // zero or missing golden is a FAILURE, never a skip.
    const U64 kGoldenH = 0x1E859BA4513A0291ULL;
    ensure("a golden H must be supplied", kGoldenH != 0);
    {
        lpr::StickyHash fresh;
        ensure("builder == reviewer golden H", fresh.update(sig) == kGoldenH);
    }
    // stable across repeated sampling
    ensure("same input, same H", sticky_a.update(sig) == sticky_b.update(reference));

    // the two documented differences
    lpr::Signature half;
    aps::appendEnvironmentFields(half, env, 0.5f, false);
    ensure("the direction tolerance parameter replaces the 0.1 literal",
        half.mField[0].mA == 0.5f && half.mField[1].mA == 0.5f &&
        half.mField[2].mA == 0.005f);

    aps::EnvSample steady = env;
    steady.mSky.mNextSunTex = steady.mSky.mSunTex;
    steady.mSky.mNextMoonTex = steady.mSky.mMoonTex;
    steady.mSky.mNextCloudNoiseTex = steady.mSky.mCloudNoiseTex;
    steady.mWater.mNextNormalMap = steady.mWater.mNormalMap;
    steady.mWater.mNextTransparent = steady.mWater.mTransparent;
    lpr::Signature mixing_off;
    aps::appendEnvironmentFields(mixing_off, steady, 0.1f, true);
    lpr::Signature mixing_off_verbatim;
    aps::appendEnvironmentFields(mixing_off_verbatim, steady, 0.1f, false);
    // sky blend is value index 33 + 7 + 9 + 2 = 51; water blend index 52 + 3 + 1 + 5 = 61
    ensure("sky blend zeroed when the textures do not mix", mixing_off.mVal[51] == 0.f);
    ensure("water blend zeroed when the textures do not mix", mixing_off.mVal[61] == 0.f);
    ensure("verbatim mode keeps the blends",
        mixing_off_verbatim.mVal[51] == 0.5f && mixing_off_verbatim.mVal[61] == 0.57f);
    lpr::Signature mixing_on;
    aps::appendEnvironmentFields(mixing_on, env, 0.1f, true);
    ensure("sky blend kept while the textures mix", mixing_on.mVal[51] == 0.5f);
    ensure("water blend kept while the textures mix", mixing_on.mVal[61] == 0.57f);

    // a missing sky / water contributes only the exact false
    aps::EnvSample none = env;
    none.mSky.mValid = false;
    none.mWater.mValid = false;
    lpr::Signature none_sig;
    aps::appendEnvironmentFields(none_sig, none, 0.1f, false);
    ensure_equals("missing sky + water leave only the 13 settings floats",
        static_cast<S32>(none_sig.mVal.size()), 13);

    // ordinary tail: fixed layout whatever the rim state
    aps::OrdTailSample tail;
    tail.mSlotCount = 5;
    tail.mLightCount = 4;
    lpr::Signature plain;
    aps::appendOrdinaryTail(plain, tail);
    tail.mRimEnabled = true;
    tail.mRimIncludeProbes = true;
    tail.mSlotEnabled[1] = true;
    tail.mRimParams[1][2][3] = 0.8f;
    lpr::Signature rimmed;
    aps::appendOrdinaryTail(rimmed, tail);
    ensure("the tail layout is fixed", plain.mField.size() == rimmed.mField.size());
    lpr::StickyHash ha;
    lpr::StickyHash hb;
    ensure("rim state changes the tail", ha.update(plain) != hb.update(rimmed));
    tail.mRimIncludeProbes = false;
    lpr::Signature excluded;
    aps::appendOrdinaryTail(excluded, tail);
    lpr::Signature excluded_plain;
    aps::OrdTailSample tail_plain;
    tail_plain.mSlotCount = 5;
    tail_plain.mLightCount = 4;
    tail_plain.mRimEnabled = true; // exact flag differs: include-probes is off in both
    aps::appendOrdinaryTail(excluded_plain, tail_plain);
    lpr::StickyHash hc;
    lpr::StickyHash hd;
    ensure("with probes excluded the rim parameters are zeroed (no churn)",
        hc.update(excluded) == hd.update(excluded_plain));
}

// S14
template<> template<>
void cine_light_rig_model_object::test<85>()
{
    set_test_name("[ProbeOnDemand] S14 motion debounce, teardown, cap policy, discrete bypass, shift");
    const F32 dt = 1.f / 64.f;
    int key_a = 0;
    int key_b = 0;

    {   // continuous motion at 64 fps: nothing until 0.5 s quiet, then ONE settle
        aps::DebounceMap map;
        std::vector<aps::Event> out;
        for (S32 f = 0; f <= 128; ++f)
        {
            podNote(map, &key_a, 1, static_cast<F32>(f), static_cast<F32>(f) + 1.f, podTime(f), dt, 3);
            map.drain(podTime(f), out);
        }
        ensure_equals("no event while it moves", static_cast<S32>(out.size()), 0);
        map.drain(podTime(128) + 0.4921875, out); // 63 frames after the last note: not yet
        ensure_equals("not before the quiet window", static_cast<S32>(out.size()), 0);
        map.drain(podTime(128) + 0.5, out);
        ensure_equals("one settle after 0.5 s of quiet", static_cast<S32>(out.size()), 1);
        ensure("it is a settle", (out[0].mReason & aps::R_SETTLE) != 0);
        ensure("carrying the geometry reason", (out[0].mReason & aps::R_GEOM) != 0);
        ensure("over the union bounds", out[0].mMin[0] == 0.f && out[0].mMax[0] == 129.f);
        ensure_equals("and the serial the motion began at", out[0].mFirstSerial, static_cast<U64>(3));
        map.drain(podTime(128) + 5.0, out);
        ensure_equals("exactly one", static_cast<S32>(out.size()), 1);
    }
    {   // 2 s periodic source: frozen from its second cycle
        aps::DebounceMap map;
        std::vector<aps::Event> out;
        for (S32 c = 0; c <= 10; ++c)
        {
            const F64 t = 2.0 * static_cast<F64>(c);
            podNote(map, &key_a, 1, 0.f, 1.f, t, dt, 1);
            map.drain(t, out);
            map.drain(t + 0.5, out); // the 0.5 s mark inside the cycle
        }
        ensure_equals("only the first cycle produced a settle", static_cast<S32>(out.size()), 1);
        map.drain(20.0 + 3.9, out);
        ensure_equals("still frozen just before 2x the gap after the last note",
            static_cast<S32>(out.size()), 1);
        map.drain(20.0 + 4.0, out);
        ensure_equals("one settle after it stops", static_cast<S32>(out.size()), 2);
    }
    {   // a gap longer than 60 s forgets the estimate
        aps::DebounceMap map;
        std::vector<aps::Event> out;
        podNote(map, &key_a, 1, 0.f, 1.f, 0.0, dt, 1);
        podNote(map, &key_a, 1, 0.f, 1.f, 2.0, dt, 1);
        map.drain(10.0, out); // settles (the 2 s estimate gives a 4 s quiet)
        const size_t before = out.size();
        podNote(map, &key_a, 1, 0.f, 1.f, 100.0, dt, 1);
        map.drain(100.5, out);
        ensure_equals("treated as fresh: 0.5 s quiet", static_cast<S32>(out.size()),
            static_cast<S32>(before + 1));
    }
    {   // low fps stretches the quiet window
        aps::DebounceMap map;
        std::vector<aps::Event> out;
        const F32 slow = 1.f / 3.f;
        podNote(map, &key_a, 1, 0.f, 1.f, 0.0, slow, 1);
        map.drain(0.9, out);
        ensure_equals("3 fps: not at 0.9 s", static_cast<S32>(out.size()), 0);
        map.drain(1.0, out);
        ensure_equals("3 fps: quiet is 3 frame times (1 s)", static_cast<S32>(out.size()), 1);
    }
    {   // alternating keys debounce independently
        aps::DebounceMap map;
        std::vector<aps::Event> out;
        for (S32 f = 0; f < 128; ++f)
        {
            podNote(map, &key_a, 1, 0.f, 1.f, podTime(f), dt, 1);
            if (f < 64)
            {
                podNote(map, &key_b, 2, 10.f, 11.f, podTime(f), dt, 1);
            }
            map.drain(podTime(f), out);
        }
        // B stopped at frame 63 -> settles at 63/64 + 0.5; A is still moving.
        map.drain(podTime(63) + 0.5, out);
        ensure_equals("B settled while A keeps moving", static_cast<S32>(out.size()), 1);
        ensure("and it is B's bounds", out[0].mMin[0] == 10.f);
    }
    {   // saturating count
        aps::DebounceConfig cfg;
        aps::Debounce d;
        const PodBox b(0.f, 1.f);
        for (S32 i = 0; i < 70000; ++i)
        {
            aps::debOnMotion(cfg, d, 0.0, dt, b.mMin, b.mMax, static_cast<U16>(aps::R_GEOM),
                static_cast<U8>(aps::C_STATIC), false, 1);
        }
        ensure_equals("the count saturates", static_cast<S32>(d.mCount), 0xFFFF);
    }
    {   // an id-tag mismatch emits the OLD pending union first, then resets
        aps::DebounceMap map;
        std::vector<aps::Event> out;
        podNote(map, &key_a, 1, 0.f, 1.f, 0.0, dt, 1);
        podNote(map, &key_a, 99, 500.f, 501.f, 0.25, dt, 2); // pointer reuse
        map.drain(0.25, out);
        ensure_equals("the old object's pending bounds were emitted", static_cast<S32>(out.size()), 1);
        ensure("and they are the old bounds", out[0].mMin[0] == 0.f && out[0].mMax[0] == 1.f);
        ensure_equals("the stat counts it", static_cast<S32>(map.stats().mIdMismatchEmits), 1);
        map.drain(0.75, out);
        ensure_equals("the new object settles on its own", static_cast<S32>(out.size()), 2);
        ensure("with only its own bounds", out[1].mMin[0] == 500.f && out[1].mMax[0] == 501.f);
    }
    {   // teardown: pending states merge into the removal event and are erased
        aps::DebounceMap map;
        std::vector<aps::Event> out;
        podNote(map, &key_a, 1, 0.f, 1.f, 0.0, dt, 4);
        const PodBox moved(20.f, 21.f);
        map.onMotion(&key_a, static_cast<U8>(aps::Motion::TEXANIM), 1, moved.mMin, moved.mMax,
            static_cast<U16>(aps::R_TEX), static_cast<U8>(aps::C_STATIC), false, 5, 0.1, dt);
        aps::Event removal = podEvent(30.f, 31.f, aps::R_GEOM, aps::C_STATIC, 9);
        ensure("something pending was merged", map.takePending(&key_a, removal));
        ensure("the removal covers the old path", removal.mMin[0] == 0.f && removal.mMax[0] == 31.f);
        ensure("and carries the texture reason too", (removal.mReason & aps::R_TEX) != 0);
        ensure_equals("with the earliest serial", removal.mFirstSerial, static_cast<U64>(4));
        ensure_equals("the states are erased", static_cast<S32>(map.size()), 0);
        map.drain(10.0, out);
        ensure_equals("no stale settle follows", static_cast<S32>(out.size()), 0);
        aps::Event nothing = podEvent(0.f, 1.f, aps::R_GEOM, aps::C_STATIC, 1);
        ensure("nothing pending: not merged", !map.takePending(&key_b, nothing));
    }
    {   // cap policy with a small cap (T54): history first, early settle of quiet keys,
        // then the bulk state with its hard deadline. T54 has TWO separate expectations:
        //  (A) a TRACKED key that stops settles within kQmin of stopping (here: it is
        //      delivered at the cap-pressure early settle, never held by the movers);
        //  (B) a key that was already FOLDED INTO BULK settles by the bulk hard deadline
        //      (10 s after the bulk's first pending note), NOT by kQmin.
        aps::DebounceConfig cfg;
        cfg.mMapCap = 4;
        aps::DebounceMap map(cfg);
        std::vector<aps::Event> out;
        int keys[10] = {};
        // key 0 settles and becomes idle history
        podNote(map, &keys[0], 1, 0.f, 1.f, 0.0, dt, 1);
        map.drain(1.0, out);
        ensure_equals("key 0 settled and is now history", static_cast<S32>(out.size()), 1);
        podNote(map, &keys[1], 1, 10.f, 11.f, 2.0, dt, 1);
        podNote(map, &keys[2], 1, 20.f, 21.f, 2.0, dt, 1);
        podNote(map, &keys[3], 1, 30.f, 31.f, 2.0, dt, 1);
        ensure_equals("map is full", static_cast<S32>(map.size()), 4);
        podNote(map, &keys[4], 1, 40.f, 41.f, 2.0, dt, 1);
        ensure_equals("(1) idle history was evicted first", static_cast<S32>(map.stats().mEvictions), 1);
        ensure_equals("no bulk", static_cast<S32>(map.stats().mBulkNotes), 0);
        ensure_equals("still full", static_cast<S32>(map.size()), 4);
        out.clear();

        // (2) = expectation (A): keys 1 and 2 keep their last note at t=2 and then stop
        // (they are tracked); keys 3 and 4 keep moving
        for (S32 f = 0; f < 192; ++f)
        {
            const F64 t = 2.5 + podTime(f);
            podNote(map, &keys[3], 1, 30.f, 31.f, t, dt, 1);
            podNote(map, &keys[4], 1, 40.f, 41.f, t, dt, 1);
        }
        const F64 t_new = 2.5 + podTime(191);
        podNote(map, &keys[5], 1, 50.f, 51.f, t_new, dt, 1); // t = 5.48: 1 and 2 stopped long ago
        ensure("(2) the stopped keys were settled early to make room",
            map.stats().mEarlySettles >= 2);
        ensure_equals("(2) the new key is tracked, not bulked", static_cast<S32>(map.stats().mBulkNotes), 0);
        map.drain(t_new, out);
        S32 stopped_delivered = 0;
        for (const aps::Event& e : out)
        {
            if (e.mMin[0] == 10.f || e.mMin[0] == 20.f)
            {
                ++stopped_delivered;
            }
        }
        ensure_equals("tracked stopped keys are delivered without waiting for the movers",
            stopped_delivered, 2);
        out.clear();

        // (3) = expectation (B): every tracked key is moving, so new keys fold into bulk;
        // their area is delivered by the bulk deadline (not kQmin)
        std::vector<int> bulk_keys(static_cast<size_t>(12 * 64 + 16));
        podNote(map, &keys[6], 1, 80.f, 81.f, t_new, dt, 1); // still fits (3 -> 4 keys)
        podNote(map, &keys[7], 1, 70.f, 71.f, t_new, dt, 1); // full, all moving -> bulk
        ensure("(3) all tracked keys moving: the new key went to bulk", map.stats().mBulkNotes >= 1);
        ensure("bulk is pending", map.bulkPending());
        // Fresh keys keep arriving (and the tracked keys keep moving) for 12 s.
        // Bulk must emit within 10 s of its first pending note even though its
        // notes never stop, and the tracked movers never get settled.
        const F64 bulk_start = t_new;
        F64 first_bulk = -1.0;
        for (S32 f = 1; f <= 12 * 64; ++f)
        {
            const F64 t = bulk_start + podTime(f);
            podNote(map, &keys[3], 1, 30.f, 31.f, t, dt, 1);
            podNote(map, &keys[4], 1, 40.f, 41.f, t, dt, 1);
            podNote(map, &keys[5], 1, 50.f, 51.f, t, dt, 1);
            podNote(map, &keys[6], 1, 80.f, 81.f, t, dt, 1);
            podNote(map, &bulk_keys[static_cast<size_t>(f)], 1, 60.f, 61.f, t, dt, 1);
            std::vector<aps::Event> tick;
            map.drain(t, tick);
            for (const aps::Event& e : tick)
            {
                ensure("a tracked mover is never settled while it moves",
                    !(e.mMin[0] == 30.f || e.mMin[0] == 40.f || e.mMin[0] == 50.f || e.mMin[0] == 80.f));
                if (e.mMin[0] == 60.f && e.mMax[0] >= 71.f && first_bulk < 0.0)
                {
                    first_bulk = t;
                }
            }
        }
        ensure("(3) the bulk area refreshed", first_bulk >= 0.0);
        ensure("(3) ... no earlier than its deadline (it never went quiet)",
            first_bulk >= bulk_start + 10.0 - 1e-9);
        ensure("(3) ... and no later than 10 s after its first pending note",
            first_bulk <= bulk_start + 10.0 + 0.05);
        ensure_equals("(3) the deadline path emitted it", static_cast<S32>(map.stats().mBulkDeadlines), 1);
    }
    {   // expectation (B) again: a key already folded into bulk is only GUARANTEED by the
        // bulk hard deadline (10 s), not by kQmin; it is never held hostage by tracked
        // movers (the loop runs 11 s with both tracked keys moving the whole time)
        aps::DebounceConfig cfg;
        cfg.mMapCap = 2;
        aps::DebounceMap map(cfg);
        std::vector<aps::Event> out;
        int keys[4] = {};
        podNote(map, &keys[0], 1, 0.f, 1.f, 0.0, dt, 1);
        podNote(map, &keys[1], 1, 5.f, 6.f, 0.0, dt, 1);
        podNote(map, &keys[2], 1, 90.f, 91.f, 0.0, dt, 1); // folded into bulk
        ensure("folded into bulk", map.stats().mBulkNotes == 1);
        for (S32 f = 1; f < 64 * 11; ++f)
        {
            const F64 t = podTime(f);
            podNote(map, &keys[0], 1, 0.f, 1.f, t, dt, 1);
            podNote(map, &keys[1], 1, 5.f, 6.f, t, dt, 1);
            map.drain(t, out);
        }
        bool bulk_seen = false;
        for (const aps::Event& e : out)
        {
            bulk_seen = bulk_seen || e.mMin[0] == 90.f;
        }
        ensure("the folded key's area refreshed within the hard deadline", bulk_seen);
    }
    {   // discrete notes never enter the debounce
        aps::DebounceMap map;
        aps::DiscreteNotes discrete(3);
        std::vector<aps::Event> out;
        for (S32 f = 0; f < 128; ++f)
        {
            podNote(map, &key_a, 1, 0.f, 1.f, podTime(f), dt, 1);
        }
        const PodBox tex(40.f, 41.f);
        discrete.note(&key_b, tex.mMin, tex.mMax, static_cast<U16>(aps::R_TEX),
            static_cast<U8>(aps::C_STATIC), 7);
        const PodBox tex2(42.f, 43.f);
        discrete.note(&key_b, tex2.mMin, tex2.mMax, static_cast<U16>(aps::R_GEOM),
            static_cast<U8>(aps::C_STATIC), 7);
        ensure_equals("same-key notes coalesce", static_cast<S32>(discrete.size()), 1);
        std::vector<aps::Event> events;
        ensure("no overflow", !discrete.take(events));
        ensure_equals("one discrete event", static_cast<S32>(events.size()), 1);
        ensure("union of bounds", events[0].mMin[0] == 40.f && events[0].mMax[0] == 43.f);
        ensure("OR of reasons",
            (events[0].mReason & aps::R_TEX) != 0 && (events[0].mReason & aps::R_GEOM) != 0);
        ensure("a discrete event is not a settle", (events[0].mReason & aps::R_SETTLE) == 0);
        map.drain(podTime(127), out);
        ensure_equals("the moving key stays frozen while the discrete event was delivered",
            static_cast<S32>(out.size()), 0);
        int keys[5] = {};
        for (S32 i = 0; i < 5; ++i)
        {
            discrete.note(&keys[i], tex.mMin, tex.mMax, static_cast<U16>(aps::R_TEX),
                static_cast<U8>(aps::C_STATIC), 1);
        }
        std::vector<aps::Event> capped;
        ensure("beyond the cap the overflow flag is raised", discrete.take(capped));
        ensure_equals("and the cap is honoured", static_cast<S32>(capped.size()), 3);
    }
    {   // shift offsets queued bounds
        aps::DebounceMap map;
        std::vector<aps::Event> out;
        podNote(map, &key_a, 1, 0.f, 1.f, 0.0, dt, 1);
        const F32 off[3] = { 256.f, 0.f, 0.f };
        map.shift(off);
        map.drain(1.0, out);
        ensure_equals("settled once", static_cast<S32>(out.size()), 1);
        ensure("bounds moved with the world", out[0].mMin[0] == 256.f && out[0].mMax[0] == 257.f);
        ensure_equals("settle counter", static_cast<S32>(podCountSettles(out)), 1);
    }
}

// S15
template<> template<>
void cine_light_rig_model_object::test<86>()
{
    set_test_name("[ProbeOnDemand] S15 cap rule: history, recount, amortised budget, spots");
    const F32 origin[3] = { 0.f, 0.f, 0.f };
    const U8 mask = static_cast<U8>(aps::C_STATIC | aps::C_TERRAIN_WATER | aps::C_LIGHT | aps::C_GLOBAL);
    const aps::ShadowDirs sd = aps::ShadowDirs();
    const S32 cap = 256;
    aps::Record rec;
    rec.mRecountDue = false;
    const aps::Event far_light = podEvent(900.f, 901.f, aps::R_LIGHT, aps::C_LIGHT, 20);
    ensure("under cap, far, not due: a distant light event misses",
        !aps::hitsCapture(far_light, rec, origin, 131.f, mask, cap, sd));
    rec.mLocalLights = static_cast<U16>(cap + 1);
    ensure("over cap: the distance order can change anywhere (current count)",
        aps::hitsCapture(far_light, rec, origin, 131.f, mask, cap, sd));
    // cap + 1 -> cap transition (a distant spot removed): the previous count keeps hitting
    rec.mLocalLightsPrev = rec.mLocalLights;
    rec.mLocalLights = static_cast<U16>(cap);
    ensure("cap+1 -> cap: the previous count still hits",
        aps::hitsCapture(far_light, rec, origin, 131.f, mask, cap, sd));
    rec.mLocalLightsPrev = static_cast<U16>(cap);
    ensure("cap -> cap: no longer over", !aps::hitsCapture(far_light, rec, origin, 131.f, mask, cap, sd));

    // history: the probe was over cap at serial 10; a settle whose motion began at 9 is still global
    rec.mOverCapSerial = 10;
    aps::Event delayed = podEvent(900.f, 901.f, static_cast<U16>(aps::R_LIGHT | aps::R_SETTLE), aps::C_LIGHT, 9);
    ensure("over-cap history keeps a delayed settle global after the recount dropped under cap",
        aps::hitsCapture(delayed, rec, origin, 131.f, mask, cap, sd));
    delayed.mFirstSerial = 11;
    ensure("a motion that began after the probe was last over cap does not",
        !aps::hitsCapture(delayed, rec, origin, 131.f, mask, cap, sd));
    aps::Record never;
    never.mRecountDue = false;
    aps::Event unset = podEvent(900.f, 901.f, aps::R_LIGHT, aps::C_LIGHT, 0);
    ensure("a probe that was never over cap is not hit by an event with serial 0",
        !aps::hitsCapture(unset, never, origin, 131.f, mask, cap, sd));

    // a probe whose recount is pending counts as over cap
    aps::Record due;
    ensure("recount-due defaults to true", due.mRecountDue);
    ensure("a recount-pending probe is treated as over cap",
        aps::hitsCapture(far_light, due, origin, 131.f, mask, cap, sd));

    // spot events follow the same rule (T57)
    aps::Event spot = podEvent(500.f, 501.f, static_cast<U16>(aps::R_LIGHT | aps::R_SETTLE), aps::C_LIGHT, 30);
    spot.mSpot = true;
    aps::Record under;
    under.mRecountDue = false;
    under.mLocalLights = 3;
    under.mLocalLightsPrev = 3;
    ensure("an under-cap probe is not hit by a distant spot",
        !aps::hitsCapture(spot, under, origin, 131.f, mask, cap, sd));
    under.mLocalLights = static_cast<U16>(cap + 2);
    ensure("an over-cap probe is hit by the same spot",
        aps::hitsCapture(spot, under, origin, 131.f, mask, cap, sd));

    // The recount: pairs, cursor, budget.
    const S32 num_lights = 5000;
    std::vector<aps::LightEntry> storage(static_cast<size_t>(num_lights));
    std::vector<const aps::LightEntry*> lights;
    for (S32 i = 0; i < num_lights; ++i)
    {
        aps::LightEntry& e = storage[static_cast<size_t>(i)];
        e.mSeq = static_cast<U64>(i + 1);
        e.mEligible = (i % 7) != 0;
        e.mSpot = (i % 500) == 0;
        e.mPos[0] = static_cast<F32>(i % 400);
        e.mPos[1] = 0.f;
        e.mPos[2] = 0.f;
        e.mRadius = 2.f;
        e.mReach = 3.f;
        lights.push_back(&e);
    }
    std::vector<aps::Record> recs(3);
    std::vector<aps::RecountProbe> probes;
    for (U32 i = 0; i < 3; ++i)
    {
        recs[i].mId = i + 1;
        aps::RecountProbe p;
        p.mRec = &recs[i];
        p.mOrigin[0] = static_cast<F32>(i) * 100.f;
        p.mMaxDist = 150.f;
        probes.push_back(p);
    }
    aps::Recount recount;
    U64 serial = 100;
    U32 frames = 0;
    bool all_recounted = false;
    for (; frames < 40 && !all_recounted; ++frames)
    {
        const U32 used = recount.run(probes, lights, aps::kRecountPairBudget, ++serial, cap);
        ensure("the per-frame pair budget is honoured", used <= aps::kRecountPairBudget);
        all_recounted = recs[0].mLastRecountSerial != 0 && recs[1].mLastRecountSerial != 0 &&
                        recs[2].mLastRecountSerial != 0;
    }
    ensure("all three probes were recounted", all_recounted);
    ensure("amortised: 3 x 5000 pairs does not fit one 8192-pair frame", frames > 1);
    for (U32 i = 0; i < 3; ++i)
    {
        U32 expected = 0;
        for (const aps::LightEntry* l : lights)
        {
            if (!l->mEligible)
            {
                continue;
            }
            const F32 dx = l->mPos[0] - probes[i].mOrigin[0];
            if (l->mSpot || std::fabs(dx) - l->mReach < 150.f)
            {
                ++expected;
            }
        }
        ensure_equals("recount matches the brute-force count", static_cast<S32>(recs[i].mLocalLights),
            static_cast<S32>(expected));
        ensure("and the probe is no longer pending", !recs[i].mRecountDue);
    }
}

// S15b: spec correction 2 -- resumable, starvation-free recount.
template<> template<>
void cine_light_rig_model_object::test<87>()
{
    set_test_name("[ProbeOnDemand] S15b recount: bounded progress with 10000 lights and continuous re-dirtying");
    const S32 num_lights = 10000;
    const S32 num_probes = 8;
    std::vector<aps::LightEntry> storage(static_cast<size_t>(num_lights));
    std::vector<const aps::LightEntry*> lights;
    for (S32 i = 0; i < num_lights; ++i)
    {
        aps::LightEntry& e = storage[static_cast<size_t>(i)];
        e.mSeq = static_cast<U64>(i + 1);
        e.mEligible = true;
        e.mPos[0] = static_cast<F32>(i % 300);
        e.mReach = 3.f;
        lights.push_back(&e);
    }
    std::vector<aps::Record> recs(static_cast<size_t>(num_probes));
    std::vector<aps::RecountProbe> probes;
    for (S32 i = 0; i < num_probes; ++i)
    {
        recs[static_cast<size_t>(i)].mId = static_cast<U32>(i + 1);
        aps::RecountProbe p;
        p.mRec = &recs[static_cast<size_t>(i)];
        p.mOrigin[0] = static_cast<F32>(i) * 40.f;
        p.mMaxDist = 100.f;
        probes.push_back(p);
    }
    ensure("one probe-pass over 10000 lights cannot fit one frame",
        num_lights > static_cast<S32>(aps::kRecountPairBudget));

    // Every frame EVERY probe is re-dirtied by light events (the priority queue
    // never empties). Each probe must still complete a recount within a bound.
    aps::Recount recount;
    std::vector<U64> first_done(static_cast<size_t>(num_probes), 0);
    const U64 bound = static_cast<U64>(
        (2 * num_lights * num_probes + static_cast<S32>(aps::kRecountPairBudget) - 1) /
            static_cast<S32>(aps::kRecountPairBudget) + num_probes + 4);
    U64 serial = 1000;
    for (U64 frame = 1; frame <= bound; ++frame)
    {
        for (aps::Record& r : recs)
        {
            aps::markRecountDue(r);
        }
        const U32 used = recount.run(probes, lights, aps::kRecountPairBudget, ++serial, 256);
        ensure("budget honoured under churn", used <= aps::kRecountPairBudget);
        for (S32 i = 0; i < num_probes; ++i)
        {
            const aps::Record& r = recs[static_cast<size_t>(i)];
            if (first_done[static_cast<size_t>(i)] == 0 && r.mLastRecountSerial != 0)
            {
                first_done[static_cast<size_t>(i)] = frame;
            }
        }
    }
    for (S32 i = 0; i < num_probes; ++i)
    {
        ensure("every probe completed a recount within the bound under continuous re-dirtying",
            first_done[static_cast<size_t>(i)] != 0 && first_done[static_cast<size_t>(i)] <= bound);
    }
    ensure("the cursor resumed across frames (no probe restarted forever)",
        recount.completed() >= static_cast<U32>(num_probes));
    // While re-dirtied the due flag stays set (treated as over cap: conservative)
    ensure("the due flag is conservative while re-dirtying continues", recs[0].mRecountDue);

    // Once the churn stops every probe settles and the counts are exact.
    for (U64 frame = 0; frame < bound; ++frame)
    {
        recount.run(probes, lights, aps::kRecountPairBudget, ++serial, 256);
    }
    for (S32 i = 0; i < num_probes; ++i)
    {
        U32 expected = 0;
        for (const aps::LightEntry* l : lights)
        {
            const F32 dx = l->mPos[0] - probes[static_cast<size_t>(i)].mOrigin[0];
            if (std::fabs(dx) - l->mReach < 100.f)
            {
                ++expected;
            }
        }
        const aps::Record& r = recs[static_cast<size_t>(i)];
        ensure_equals("exact count once the churn stops", static_cast<S32>(r.mLocalLights),
            static_cast<S32>(std::min<U32>(expected, 0xFFFFu)));
        ensure("and the due flag clears", !r.mRecountDue);
    }

    // The regular job is not starved by a stream of priority work: with only one
    // probe ever marked due, the others are still recounted in bounded time.
    std::vector<aps::Record> recs2(static_cast<size_t>(num_probes));
    std::vector<aps::RecountProbe> probes2;
    for (S32 i = 0; i < num_probes; ++i)
    {
        recs2[static_cast<size_t>(i)].mId = static_cast<U32>(i + 1);
        recs2[static_cast<size_t>(i)].mRecountDue = false;
        aps::RecountProbe p;
        p.mRec = &recs2[static_cast<size_t>(i)];
        p.mOrigin[0] = static_cast<F32>(i) * 40.f;
        p.mMaxDist = 100.f;
        probes2.push_back(p);
    }
    aps::Recount recount2;
    U64 serial2 = 5000;
    for (U64 frame = 1; frame <= bound; ++frame)
    {
        aps::markRecountDue(recs2[0]);
        recount2.run(probes2, lights, aps::kRecountPairBudget, ++serial2, 256);
    }
    for (S32 i = 1; i < num_probes; ++i)
    {
        ensure("a non-priority probe is recounted despite constant priority work",
            recs2[static_cast<size_t>(i)].mLastRecountSerial != 0);
    }

    // Budget-driven (not "3 probes per frame"): 256 probes x 50 lights = 12800 pair
    // tests complete in ceil(12800 / 8192) = 2 frames.
    {
        const S32 many_probes = 256;
        const S32 few_lights = 50;
        std::vector<aps::LightEntry> few(static_cast<size_t>(few_lights));
        std::vector<const aps::LightEntry*> few_ptrs;
        for (S32 i = 0; i < few_lights; ++i)
        {
            aps::LightEntry& e = few[static_cast<size_t>(i)];
            e.mSeq = static_cast<U64>(i + 1);
            e.mEligible = true;
            e.mPos[0] = static_cast<F32>(i);
            e.mReach = 3.f;
            few_ptrs.push_back(&e);
        }
        std::vector<aps::Record> many(static_cast<size_t>(many_probes));
        std::vector<aps::RecountProbe> many_probes_v;
        for (S32 i = 0; i < many_probes; ++i)
        {
            many[static_cast<size_t>(i)].mId = static_cast<U32>(i + 1);
            aps::RecountProbe p;
            p.mRec = &many[static_cast<size_t>(i)];
            p.mMaxDist = 100.f;
            many_probes_v.push_back(p);
        }
        aps::Recount big;
        S32 frames_needed = 0;
        bool all_done = false;
        for (U64 f = 1; f <= 6 && !all_done; ++f)
        {
            const U32 used = big.run(many_probes_v, few_ptrs, aps::kRecountPairBudget, 9000 + f, 256);
            ensure("the budget is honoured", used <= aps::kRecountPairBudget);
            ++frames_needed;
            all_done = true;
            for (const aps::Record& r : many)
            {
                all_done = all_done && r.mLastRecountSerial != 0;
            }
        }
        ensure("all 256 probes were recounted", all_done);
        ensure("within ceil(P*L / budget) = 2 frames (+1 slack)", frames_needed <= 3);
    }
}

// S16
template<> template<>
void cine_light_rig_model_object::test<88>()
{
    set_test_name("[ProbeOnDemand] S16 stickyDigest == StickyHash::update; shifted accumulators");
    lpr::StickyHash sticky;
    lpr::Signature sig;
    const F32 pos[3] = { 1.f, 2.f, 3.f };
    const F32 fwd[3] = { 0.f, 0.f, -1.f };
    const F32 col[3] = { 1.f, 0.5f, 0.25f };
    sig.addAbs3(pos, 0.05f);
    sig.addAngle(fwd, 0.25f);
    sig.addColor(col, 0.01f, 1e-3f);
    sig.addRel(3.f, 0.005f, 1e-3f);
    sig.addExact(true);
    sig.addExact(static_cast<U64>(77));
    const U64 h = sticky.update(sig);
    ensure("digest == update for an unchanged signature", aps::stickyDigest(sticky, sig.mExact) == h);
    for (S32 i = 0; i < 10; ++i)
    {
        ensure("and it stays equal", sticky.update(sig) == h && aps::stickyDigest(sticky, sig.mExact) == h);
    }

    // a light entry across a region crossing
    aps::DebounceConfig dc;
    std::vector<aps::Event> out;
    aps::LightEntry e;
    aps::LightSample s = podLight(1, 100.f, 10.f);
    s.mPos[1] = 40.f;
    aps::lightStep(e, s, true, 1, 0.0, 1.f / 64.f, dc, out);
    const size_t events_before = out.size();
    ensure("a new eligible light is one discrete event", events_before == 1);
    ensure("its XFORM hash is the digest of its accumulators",
        e.mH[aps::LS_XFORM] == aps::stickyDigest(e.mSticky[aps::LS_XFORM], e.mExact[aps::LS_XFORM]));

    const F32 off[3] = { -256.f, 0.f, 0.f };
    aps::lightShift(e, off);
    ensure("the shifted accumulator equals the shifted position",
        e.mSticky[aps::LS_XFORM].mAcc[0] == 100.f - 256.f);
    ensure("the published position moved too", e.mPubPos[0] == 100.f - 256.f);
    // the sample after the crossing is the same light at the shifted position
    aps::LightSample crossed = s;
    crossed.mPos[0] += off[0];
    const U64 stored = e.mH[aps::LS_XFORM];
    const U64 stored_pub = e.mPubH[0];
    const U8 res = aps::lightStep(e, crossed, false, 2, 1.0, 1.f / 64.f, dc, out);
    ensure_equals("a crossing produces no light flags", static_cast<S32>(res), 0);
    ensure_equals("and no light event", static_cast<S32>(out.size()), static_cast<S32>(events_before));
    ensure("the digest after the shift equals the next update() result", e.mH[aps::LS_XFORM] == stored);
    ensure("the published digest is consistent with its accumulators",
        stored_pub == aps::stickyDigest(e.mPubAcc[0], e.mSticky[aps::LS_XFORM].mLayout,
                                        e.mExact[aps::LS_XFORM]));
    ensure("nothing is pending", !e.mDeb[0].mPending && !e.mDeb[1].mPending);

    // without the digest fix a crossing WOULD fire: positions shifted but the
    // stored hash recomputed from stale accumulators differ.
    aps::LightEntry stale;
    aps::lightStep(stale, s, true, 1, 0.0, 1.f / 64.f, dc, out);
    const U64 before_hash = stale.mH[aps::LS_XFORM];
    ALCineLiveProbeRefresh::StickyHash& sx = stale.mSticky[aps::LS_XFORM];
    sx.mAcc[0] += off[0]; // shift the accumulator only
    ensure("recomputing after a shift yields a different hash than before it",
        aps::stickyDigest(sx, stale.mExact[aps::LS_XFORM]) != before_hash);
}

// S17
template<> template<>
void cine_light_rig_model_object::test<89>()
{
    set_test_name("[ProbeOnDemand] S17 refresh-all barrier state machine");
    typedef aps::Barrier B;
    {   // membership by id; ordinary needs a post-arm ack; realtime a post-arm pair; Live post-arm ops
        B b;
        const std::vector<U32> ids = { 1, 2, 3 };
        aps::barrierArm(b, 10, 100.0, ids, B::LiveState::PENDING, 7, 50);
        ensure("armed", b.mActive && aps::barrierTotal(b) == 3);
        ensure("members are pending", aps::barrierIsMember(b, 1) && aps::barrierIsMember(b, 3));
        ensure("unknown ids are not members", !aps::barrierIsMember(b, 99));
        aps::barrierMemberAck(b, 1, 9);
        ensure("an ack of a pre-arm transaction does not count", aps::barrierIsMember(b, 1));
        aps::barrierMemberAck(b, 1, 10);
        ensure("a post-arm ack does", !aps::barrierIsMember(b, 1));
        // REALTIME member
        aps::barrierRealtimePass(b, 2, true, 11);
        ensure("a radiance pass before any irradiance does not finish it", aps::barrierIsMember(b, 2));
        aps::barrierRealtimePass(b, 2, false, 9);
        aps::barrierRealtimePass(b, 2, true, 9);
        ensure("pre-arm passes never count", aps::barrierIsMember(b, 2));
        aps::barrierRealtimePass(b, 2, false, 10);
        aps::barrierRealtimePass(b, 2, true, 11);
        ensure("a post-arm irradiance then radiance pair finishes it", !aps::barrierIsMember(b, 2));
        // dropped member
        aps::barrierMemberDropped(b, 3);
        ensure("a dropped member no longer holds the barrier", !aps::barrierIsMember(b, 3));
        // Live: pre-arm / at-arm passes excluded, wrong id excluded
        aps::barrierLivePass(b, 7, false, 49);
        aps::barrierLivePass(b, 7, false, 50);
        ensure("passes at or before the arm op do not count", b.mLive == B::LiveState::PENDING);
        aps::barrierLivePass(b, 8, false, 60);
        ensure("a pass of a different probe id never counts", b.mLive == B::LiveState::PENDING);
        aps::barrierLivePass(b, 7, true, 61);
        ensure("a radiance pass before the irradiance does not count", b.mLive == B::LiveState::PENDING);
        aps::barrierLivePass(b, 7, false, 62);
        ensure("a post-arm irradiance pass", b.mLive == B::LiveState::IRR_DONE);
        aps::barrierTick(b, 101.0, false);
        ensure("Live still pending: not done", b.mActive);
        aps::barrierLivePass(b, 7, true, 63);
        ensure("then radiance", b.mLive == B::LiveState::DONE);
        aps::barrierTick(b, 102.0, false);
        ensure("terminal DONE", !b.mActive && b.mTerminal == B::Terminal::DONE);
        ensure("the readout says what happened",
            aps::barrierReadout(b) ==
                "All remaining probes refreshed (2 of 3, 1 dropped), Live: refreshed");
    }
    {   // all done, nothing dropped, no Live
        B b;
        const std::vector<U32> ids = { 4, 5 };
        aps::barrierArm(b, 3, 10.0, ids, B::LiveState::NONE, 0, 1);
        aps::barrierMemberAck(b, 4, 3);
        aps::barrierMemberAck(b, 5, 4);
        aps::barrierTick(b, 11.0, false);
        ensure("All probes refreshed (n)", aps::barrierReadout(b) == "All probes refreshed (2)");
    }
    {   // Live replacement: the re-arm is an OP, so a same-frame pass after it counts
        B b;
        const std::vector<U32> ids;
        aps::barrierArm(b, 5, 0.0, ids, B::LiveState::PENDING, 7, 100);
        aps::barrierLivePass(b, 7, false, 101);
        ensure("old probe irradiance counted", b.mLive == B::LiveState::IRR_DONE);
        aps::barrierLiveDesignation(b, true, true, 9, 110);
        ensure("replaced -> re-armed", b.mLive == B::LiveState::PENDING && b.mLiveId == 9 &&
                                           b.mLiveRearms == 1);
        ensure("the readout says so",
            aps::barrierReadout(b).find("Live: replaced -> re-armed") != std::string::npos);
        aps::barrierLivePass(b, 7, false, 111);
        ensure("a pass of the OLD probe never counts after the replacement",
            b.mLive == B::LiveState::PENDING);
        aps::barrierLivePass(b, 9, false, 105);
        ensure("the new probe's earlier pass never counts", b.mLive == B::LiveState::PENDING);
        aps::barrierLivePass(b, 9, false, 111);
        ensure("a pass that starts later in the SAME frame (op > re-arm op) counts",
            b.mLive == B::LiveState::IRR_DONE);
        aps::barrierLivePass(b, 9, true, 112);
        ensure("and the radiance finishes it", b.mLive == B::LiveState::DONE);
        // a replacement with no cube / relevance is UNAVAILABLE
        B c;
        aps::barrierArm(c, 5, 0.0, ids, B::LiveState::PENDING, 7, 100);
        aps::barrierLiveDesignation(c, true, false, 9, 110);
        ensure("a replacement without a cube is unavailable", c.mLive == B::LiveState::UNAVAILABLE);
    }
    {   // Every-frame single-frame passes are counted by kind
        B b;
        const std::vector<U32> ids;
        aps::barrierArm(b, 5, 0.0, ids, B::LiveState::PENDING, 7, 10);
        aps::barrierLivePass(b, 7, true, 11);
        ensure("a radiance single-frame pass first: not counted", b.mLive == B::LiveState::PENDING);
        aps::barrierLivePass(b, 7, false, 12);
        aps::barrierLivePass(b, 7, true, 13);
        ensure("irradiance then radiance single-frame passes finish Live", b.mLive == B::LiveState::DONE);
    }
    {   // UNAVAILABLE and REMOVED never hang the barrier
        B b;
        const std::vector<U32> ids;
        aps::barrierArm(b, 5, 0.0, ids, B::LiveState::UNAVAILABLE, 0, 10);
        aps::barrierTick(b, 1.0, false);
        ensure("unavailable Live does not hold the barrier", !b.mActive && b.mTerminal == B::Terminal::DONE);
        ensure("and is reported", aps::barrierReadout(b).find("Live: unavailable") != std::string::npos);
        B r;
        aps::barrierArm(r, 5, 0.0, ids, B::LiveState::PENDING, 7, 10);
        aps::barrierLiveDesignation(r, false, false, 0, 20);
        ensure("an undesignated Live is REMOVED", r.mLive == B::LiveState::REMOVED);
        aps::barrierTick(r, 1.0, false);
        ensure("and the barrier completes", !r.mActive && r.mTerminal == B::Terminal::DONE);
    }
    {   // pause freezes the timeout; the timeout is incomplete, never "All"
        B b;
        const std::vector<U32> ids = { 1 };
        aps::barrierArm(b, 5, 0.0, ids, B::LiveState::NONE, 0, 10);
        aps::barrierTick(b, 29.0, false);
        ensure("29 s: still waiting", b.mActive);
        for (S32 t = 30; t < 100; ++t)
        {
            aps::barrierTick(b, static_cast<F64>(t), true);
        }
        ensure("a long pause does not time the barrier out", b.mActive);
        ensure("the readout says paused", aps::barrierReadout(b).find("(paused)") != std::string::npos);
        aps::barrierTick(b, 99.5, false);
        ensure("the clock resumes (29.5 unpaused seconds)", b.mActive);
        aps::barrierTick(b, 100.0, false);
        ensure("30 unpaused seconds: incomplete", !b.mActive && b.mTerminal == B::Terminal::INCOMPLETE);
        ensure("Incomplete readout, never 'All'",
            aps::barrierReadout(b) == "Incomplete: 0/1 refreshed");
    }
    {   // cancel, then re-arm
        B b;
        const std::vector<U32> ids = { 1, 2 };
        aps::barrierArm(b, 5, 0.0, ids, B::LiveState::NONE, 0, 10);
        aps::barrierCancel(b, false, 1.0);
        ensure("cancelled (disabled)", aps::barrierReadout(b) == "Refresh all: cancelled (disabled)");
        aps::barrierArm(b, 6, 2.0, ids, B::LiveState::NONE, 0, 11);
        ensure("pressing again re-arms", b.mActive && b.mTerminal == B::Terminal::NONE);
        aps::barrierCancel(b, true, 3.0);
        ensure("cancelled (reset)", aps::barrierReadout(b) == "Refresh all: cancelled (reset)");
        // members deleted mid-barrier: dropped, never a hang
        aps::barrierArm(b, 7, 4.0, ids, B::LiveState::NONE, 0, 12);
        aps::barrierMemberDropped(b, 1);
        aps::barrierMemberAck(b, 2, 7);
        aps::barrierTick(b, 5.0, false);
        ensure("a dropped + a done member completes", !b.mActive && b.mTerminal == B::Terminal::DONE);
        ensure("with the dropped count", aps::barrierReadout(b) ==
            "All remaining probes refreshed (1 of 2, 1 dropped)");
    }
}

// S18
template<> template<>
void cine_light_rig_model_object::test<90>()
{
    set_test_name("[ProbeOnDemand] S18 Live ON-path H: published values, STRUCT, range, scene serial");
    aps::DebounceConfig dc;
    const F32 dt = 1.f / 64.f;
    std::vector<aps::Event> events;
    aps::LightEntry l1;
    aps::LightEntry l2;
    aps::lightStep(l1, podLight(1, 10.f, 5.f), true, 1, 0.0, dt, dc, events);
    aps::lightStep(l2, podLight(2, 20.f, 5.f), true, 1, 0.0, dt, dc, events);
    PodLiveH live;
    live.mIn.mOrigin[0] = 12.f;
    live.mIn.mRadius = 8.f;
    std::vector<const aps::LightEntry*> both;
    both.push_back(&l1);
    both.push_back(&l2);

    const U64 h0 = live.sample(both);
    for (S32 f = 0; f < 1000; ++f)
    {
        ensure("an unchanged state gives a constant H", live.sample(both) == h0);
    }

    // XFORM motion of a light: debounce pending, published values frozen -> H unchanged
    F64 now = 1.0;
    for (S32 f = 1; f <= 20; ++f)
    {
        now = 1.0 + podTime(f);
        const U8 res = aps::lightStep(l1, podLight(1, 10.f + static_cast<F32>(f), 5.f), false, 2, now, dt, dc, events);
        ensure("the move is a motion note", (res & aps::LSR_MOTION) != 0);
        aps::lightSettleDue(l1, now, events);
        ensure("H stays unchanged while the light moves", live.sample(both) == h0);
    }
    ensure("the debounce is pending", l1.mDeb[0].mPending);
    // it stops: one settle, then H changes exactly once
    U64 changes = 0;
    U64 last = h0;
    for (S32 f = 21; f < 21 + 64; ++f)
    {
        now = 1.0 + podTime(f);
        aps::lightStep(l1, podLight(1, 30.f, 5.f), false, 3, now, dt, dc, events);
        aps::lightSettleDue(l1, now, events);
        const U64 h = live.sample(both);
        if (h != last)
        {
            ++changes;
            last = h;
        }
    }
    ensure_equals("H changed exactly once, at the settle", changes, static_cast<U64>(1));
    ensure("the published position is the settled one", l1.mPubPos[0] == 30.f);

    // PHOTO motion (flicker): same behaviour
    changes = 0;
    const U64 h_photo0 = live.sample(both);
    for (S32 f = 0; f < 40; ++f)
    {
        now = 5.0 + podTime(f);
        aps::LightSample flick = podLight(1, 30.f, 5.f);
        flick.mColor[0] = (f % 2) ? 1.f : 3.f;
        aps::lightStep(l1, flick, false, 4, now, dt, dc, events);
        aps::lightSettleDue(l1, now, events);
        ensure("flicker does not move H while it runs", live.sample(both) == h_photo0);
    }
    aps::LightSample steady = podLight(1, 30.f, 5.f);
    steady.mColor[0] = 3.f;
    U64 photo_last = h_photo0;
    for (S32 f = 0; f < 64; ++f)
    {
        now = 6.0 + podTime(f);
        aps::lightStep(l1, steady, false, 5, now, dt, dc, events);
        aps::lightSettleDue(l1, now, events);
        const U64 h = live.sample(both);
        if (h != photo_last)
        {
            ++changes;
            photo_last = h;
        }
    }
    ensure_equals("one change at the PHOTO settle", changes, static_cast<U64>(1));

    // STRUCT change (radius): immediate
    const U64 before_struct = live.sample(both);
    const U8 sres = aps::lightStep(l2, podLight(2, 20.f, 9.f), false, 6, 8.0, dt, dc, events);
    ensure("a radius change is a STRUCT change", (sres & aps::LSR_STRUCT) != 0);
    ensure("H changes immediately", live.sample(both) != before_struct);

    // a light entering / leaving the Live range via its published position
    const F32 origin[3] = { 12.f, 0.f, 0.f };
    aps::LightEntry far_light;
    aps::lightStep(far_light, podLight(3, 400.f, 2.f), true, 1, 0.0, dt, dc, events);
    ensure("a far light is not selected", !aps::liveSelects(far_light, origin, 64.f, false, false));
    ensure("unless pinned", aps::liveSelects(far_light, origin, 64.f, true, false));
    ensure("a pinned but ignored light is excluded", !aps::liveSelects(far_light, origin, 64.f, true, true));
    aps::LightSample spot = podLight(3, 400.f, 2.f);
    spot.mSpot = true;
    aps::LightEntry spot_entry;
    aps::lightStep(spot_entry, spot, true, 1, 0.0, dt, dc, events);
    ensure("a spot is always a candidate", aps::liveSelects(spot_entry, origin, 64.f, false, false));
    const U64 without = live.sample(both);
    std::vector<const aps::LightEntry*> with_far = both;
    with_far.push_back(&far_light);
    ensure("adding a light to the token set changes H once",
        live.sample(with_far) != without && live.sample(with_far) == live.sample(with_far));

    // scene serial
    const U64 serial_before = live.sample(both);
    live.mIn.mSceneSerial += 1;
    ensure("a scene serial bump changes H", live.sample(both) != serial_before);

    // probe origin: raw, with the sticky tolerance
    const U64 origin_before = live.sample(both);
    live.mIn.mOrigin[0] += 0.01f;
    ensure("a sub-tolerance origin drift does not change H", live.sample(both) == origin_before);
    live.mIn.mOrigin[0] += 1.0f;
    ensure("a real origin move does", live.sample(both) != origin_before);

    // order independence
    PodLiveH a;
    PodLiveH b;
    std::vector<const aps::LightEntry*> forward;
    forward.push_back(&l1);
    forward.push_back(&l2);
    forward.push_back(&far_light);
    std::vector<const aps::LightEntry*> reversed;
    reversed.push_back(&far_light);
    reversed.push_back(&l2);
    reversed.push_back(&l1);
    ensure("re-sorting the same token set by id is order independent",
        a.sample(forward) == b.sample(reversed));

    // fewer than one effective light: no tokens at all
    PodLiveH none;
    none.mIn.mEffectiveCount = 0;
    const U64 none_h = none.sample(both);
    ensure("with the light count at 0 the lights do not matter",
        none_h == none.sample(forward));
}

// S19: VO-cache provenance (cache cull / re-creation vs authoritative changes)
template<> template<>
void cine_light_rig_model_object::test<91>()
{
    set_test_name("[ProbeOnDemand] S19 cache provenance: unchanged re-creation only; authoritative clears both");
    const U64 obj = 0x1111;
    const U64 other = 0x2222;
    {   // unchanged cull + re-creation is cache-born; it stays so until a real update
        aps::CacheProvenance p;
        p.noteCulled(obj);
        ensure("culled", p.wasCulled(obj));
        ensure("an id that was never culled is not cache-born when created", !p.noteCreated(other));
        ensure("an unchanged re-creation of the culled id is cache-born", p.noteCreated(obj));
        ensure("cache-born", p.isBorn(obj));
        ensure("the culled marker was consumed", !p.wasCulled(obj));
        p.noteAuthoritative(obj); // real server update
        ensure("a real update ends the cache-born state", !p.isBorn(obj));
    }
    {   // (a) changed while culled (CRC change / replaced entry / miss + full update):
        // the authoritative note arrives BEFORE the re-creation -> not cache-born
        aps::CacheProvenance p;
        p.noteCulled(obj);
        p.noteAuthoritative(obj);
        ensure("a changed object is not cache-born when re-created", !p.noteCreated(obj));
        ensure("so its edits are never hidden", !p.isBorn(obj));
        // already cache-born, then a real change arrives: suppression ends
        aps::CacheProvenance q;
        q.noteCulled(obj);
        q.noteCreated(obj);
        ensure("cache-born", q.isBorn(obj));
        q.noteAuthoritative(obj);
        ensure("a real change of a cache-born object ends suppression", !q.isBorn(obj));
    }
    {   // (b) a stale culled marker must not outlive an authoritative update or a real kill
        aps::CacheProvenance p;
        p.noteCulled(obj);
        p.noteAuthoritative(obj); // server update or real kill
        ensure("no stale culled marker (a later genuine removal is not silenced)", !p.wasCulled(obj));
        ensure("and no stale born marker", !p.isBorn(obj));
        p.noteCulled(other);
        ensure("other ids are untouched", p.wasCulled(other));
    }
    {   // bounded
        aps::CacheProvenance p(4);
        for (U64 i = 1; i <= 10; ++i)
        {
            p.noteCulled(i);
        }
        ensure("the culled set is bounded", !p.culledEmpty() && !p.wasCulled(1));
    }
}

// [ProbeManualRate] Manual cadence (test<92>..test<94>): the pure scheduler only;
// the manager's capture of the faces it asks for is an in-world check.
template<> template<>
void cine_light_rig_model_object::test<92>()
{
    set_test_name("[ProbeManualRate] manual N=1 faces=6 matches Every frame face counts");
    LiveProbeSim sim;
    sim.mParams.mMode = lpr::Mode::MANUAL;
    sim.mParams.mManualEvery = 1;
    sim.mParams.mManualFaces = 6;
    S32 faces = 0;
    for (S32 f = 1; f <= 12; ++f)
    {
        const lpr::Decision d = sim.step(0);
        ensure("manual N=1 renders on every frame", d.mPath == lpr::Path::BUDGET);
        ensure_equals("six faces per frame, like Every frame", d.mFaces, 6);
        faces += d.mFaces;
    }
    ensure_equals("72 faces in 12 frames (6 per frame)", faces, 72);
    ensure_equals("one pass publishes per frame", static_cast<S32>(sim.mLog.size()), 12);
    for (S32 i = 0; i < 12; ++i)
    {
        ensure_equals("pass lands on its own frame", sim.mLog[i].mFrame, i + 1);
        ensure("irradiance / radiance alternate",
            sim.mLog[i].mEnd == ((i % 2 == 0) ? lpr::PassEnd::IRRADIANCE
                                              : lpr::PassEnd::RADIANCE));
    }
}

template<> template<>
void cine_light_rig_model_object::test<93>()
{
    set_test_name("[ProbeManualRate] manual N=4 faces=1 renders one face on every 4th frame only");
    LiveProbeSim sim;
    sim.mParams.mMode = lpr::Mode::MANUAL;
    sim.mParams.mManualEvery = 4;
    sim.mParams.mManualFaces = 1;
    S32 faces = 0;
    for (S32 f = 1; f <= 48; ++f)
    {
        // The hash, the animation flag and an idle-worthy constant H must all be ignored.
        sim.mAnimating = (f % 5) == 0;
        const lpr::Decision d = sim.step(static_cast<U64>(f % 3));
        if (f % 4 == 0)
        {
            ensure("a burst frame is BUDGET", d.mPath == lpr::Path::BUDGET);
            ensure_equals("one face per burst", d.mFaces, 1);
            faces += 1;
        }
        else
        {
            ensure("a frame in between renders nothing", d.mPath == lpr::Path::IDLE);
            ensure_equals("no faces in between", d.mFaces, 0);
        }
    }
    ensure_equals("12 faces in 48 frames", faces, 12);
    ensure_equals("two full passes in 48 frames", static_cast<S32>(sim.mLog.size()), 2);
    ensure_equals("irradiance publishes on the 6th burst", sim.mLog[0].mFrame, 24);
    ensure("first pass is irradiance", sim.mLog[0].mEnd == lpr::PassEnd::IRRADIANCE);
    ensure_equals("radiance publishes on the 12th burst", sim.mLog[1].mFrame, 48);
    ensure("second pass is radiance", sim.mLog[1].mEnd == lpr::PassEnd::RADIANCE);
    // never idles for good: the cadence continues indefinitely
    for (S32 f = 49; f <= 96; ++f)
    {
        const lpr::Decision d = sim.step(0);
        ensure("keeps cycling forever", (d.mPath == lpr::Path::BUDGET) == (f % 4 == 0));
    }
    ensure_equals("four passes after 96 frames", static_cast<S32>(sim.mLog.size()), 4);
}

template<> template<>
void cine_light_rig_model_object::test<94>()
{
    set_test_name("[ProbeManualRate] manual clamps, warm-up and Refresh now turbo");
    {   // every < 1 -> 1 ; faces < 1 -> 1
        LiveProbeSim sim;
        sim.mParams.mMode = lpr::Mode::MANUAL;
        sim.mParams.mManualEvery = -7;
        sim.mParams.mManualFaces = 0;
        for (S32 f = 1; f <= 6; ++f)
        {
            const lpr::Decision d = sim.step(0);
            ensure("every clamps up to 1", d.mPath == lpr::Path::BUDGET);
            ensure_equals("faces clamps up to 1", d.mFaces, 1);
        }
    }
    {   // faces > 6 -> 6 ; every > 120 -> 120
        LiveProbeSim sim;
        sim.mParams.mMode = lpr::Mode::MANUAL;
        sim.mParams.mManualEvery = 100000;
        sim.mParams.mManualFaces = 99;
        for (S32 f = 1; f <= 240; ++f)
        {
            const lpr::Decision d = sim.step(0);
            if (f % 120 == 0)
            {
                ensure("every clamps down to 120", d.mPath == lpr::Path::BUDGET);
                ensure_equals("faces clamps down to 6", d.mFaces, 6);
            }
            else
            {
                ensure("nothing between bursts", d.mPath == lpr::Path::IDLE);
            }
        }
    }
    {   // not ready: the shared one-time warm-up, not the manual cadence
        LiveProbeSim sim;
        sim.mParams.mMode = lpr::Mode::MANUAL;
        sim.mParams.mManualEvery = 50;
        sim.mReady = false;
        const lpr::Decision d = sim.step(0);
        ensure("not ready warms up FULL", d.mPath == lpr::Path::FULL);
    }
    {   // Refresh now: both cubes rebuilt at 6 faces/frame, then the cadence resumes
        LiveProbeSim sim;
        sim.mParams.mMode = lpr::Mode::MANUAL;
        sim.mParams.mManualEvery = 10;
        sim.mParams.mManualFaces = 1;
        for (S32 f = 1; f <= 13; ++f)
        {
            sim.step(0); // frame 10 left a pass mid-way (1 of 6 faces)
        }
        const size_t before = sim.mLog.size();
        sim.mState.mManual = true;
        for (S32 k = 0; k < 2; ++k)
        {
            const lpr::Decision d = sim.step(0);
            ensure("turbo frame renders", d.mPath == lpr::Path::BUDGET);
            ensure_equals("turbo is capped at the 6-face Every frame budget", d.mFaces, 6);
        }
        ensure_equals("two clean passes landed", static_cast<S32>(sim.mLog.size() - before), 2);
        ensure("turbo publishes irradiance first",
            sim.mLog[before].mEnd == lpr::PassEnd::IRRADIANCE);
        ensure("then radiance", sim.mLog[before + 1].mEnd == lpr::PassEnd::RADIANCE);
        ensure("request consumed", !sim.mState.mManual);
        for (S32 f = 0; f < 9; ++f)
        {
            const lpr::Decision d = sim.step(0);
            ensure("back to nothing between bursts", d.mPath == lpr::Path::IDLE);
        }
        const lpr::Decision d = sim.step(0);
        ensure("cadence resumes at N frames after the turbo", d.mPath == lpr::Path::BUDGET);
        ensure_equals("with the configured face count", d.mFaces, 1);
    }
}

// [ProbeManualRate] P2 fixes: exact K faces per burst across pass boundaries,
// and the turbo's fixed irradiance-then-radiance order.
template<> template<>
void cine_light_rig_model_object::test<95>()
{
    set_test_name("[ProbeManualRate] manual K=4 and K=5 deliver exactly K faces per burst across pass ends");
    {
        LiveProbeSim sim;
        sim.mParams.mMode = lpr::Mode::MANUAL;
        sim.mParams.mManualEvery = 1;
        sim.mParams.mManualFaces = 4;
        for (S32 f = 1; f <= 6; ++f)
        {
            sim.step(0);
            ensure_equals("K=4 burst is exactly 4 faces", sim.mLastBurstFaces, 4);
        }
        ensure_equals("24 faces = 4 passes", static_cast<S32>(sim.mLog.size()), 4);
        const S32 frames[4] = { 2, 3, 5, 6 };
        for (S32 i = 0; i < 4; ++i)
        {
            ensure_equals("K=4 pass end frame", sim.mLog[i].mFrame, frames[i]);
            ensure("K=4 passes alternate",
                sim.mLog[i].mEnd == ((i % 2 == 0) ? lpr::PassEnd::IRRADIANCE
                                                  : lpr::PassEnd::RADIANCE));
        }
    }
    {
        LiveProbeSim sim;
        sim.mParams.mMode = lpr::Mode::MANUAL;
        sim.mParams.mManualEvery = 3;
        sim.mParams.mManualFaces = 5;
        S32 bursts = 0;
        for (S32 f = 1; f <= 18; ++f)
        {
            sim.step(0);
            if (f % 3 == 0)
            {
                ++bursts;
                ensure_equals("K=5 burst is exactly 5 faces", sim.mLastBurstFaces, 5);
            }
            else
            {
                ensure_equals("no faces between bursts", sim.mLastBurstFaces, 0);
            }
        }
        ensure_equals("six bursts", bursts, 6);
        ensure_equals("30 faces = 5 passes", static_cast<S32>(sim.mLog.size()), 5);
        const S32 frames[5] = { 6, 9, 12, 15, 18 };
        for (S32 i = 0; i < 5; ++i)
        {
            ensure_equals("K=5 pass end frame", sim.mLog[i].mFrame, frames[i]);
            ensure("K=5 passes alternate",
                sim.mLog[i].mEnd == ((i % 2 == 0) ? lpr::PassEnd::IRRADIANCE
                                                  : lpr::PassEnd::RADIANCE));
        }
    }
}

template<> template<>
void cine_light_rig_model_object::test<96>()
{
    set_test_name("[ProbeManualRate] turbo from irradiance-done + radiance-partial runs irradiance, then radiance");
    LiveProbeSim sim;
    sim.mParams.mMode = lpr::Mode::MANUAL;
    sim.mParams.mManualEvery = 1;
    sim.mParams.mManualFaces = 6;
    sim.step(0); // irradiance completes
    ensure_equals("one pass", static_cast<S32>(sim.mLog.size()), 1);
    ensure("irradiance first", sim.mLog[0].mEnd == lpr::PassEnd::IRRADIANCE);
    sim.mParams.mManualFaces = 1;
    sim.step(0);
    sim.step(0); // radiance partial: 2 of 6
    ensure("radiance still partial", sim.mState.mActive && sim.mState.mRadiance);
    ensure_equals("no extra pass yet", static_cast<S32>(sim.mLog.size()), 1);
    sim.mState.mManual = true; // Refresh now
    sim.step(0);
    ensure_equals("turbo frame 1 is a full burst", sim.mLastBurstFaces, 6);
    ensure_equals("turbo frame 1 published", static_cast<S32>(sim.mLog.size()), 2);
    ensure("turbo publishes irradiance first", sim.mLog[1].mEnd == lpr::PassEnd::IRRADIANCE);
    sim.step(0);
    ensure_equals("turbo frame 2 published", static_cast<S32>(sim.mLog.size()), 3);
    ensure("then radiance", sim.mLog[2].mEnd == lpr::PassEnd::RADIANCE);
    for (S32 f = 0; f < 6; ++f)
    {
        sim.step(0); // normal cadence, 1 face per frame
    }
    ensure_equals("next normal pass completed", static_cast<S32>(sim.mLog.size()), 4);
    ensure("alternation continues with irradiance", sim.mLog[3].mEnd == lpr::PassEnd::IRRADIANCE);
}
} // namespace tut
