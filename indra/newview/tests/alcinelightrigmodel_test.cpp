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
#include "../llviewercamera.h"
#include "../pipeline.h"
#include "../../llrender/llshadermgr.h"

#include <algorithm>
#include <cmath>
#include <cstring>
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
        50, 51, 52, 53, 54, 56, 58, 59
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
    ensure_equals("FX name row count", FX_COUNT, 67);
    ensure_equals("FX interval row count", FX_COUNT, 67);

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
} // namespace tut
