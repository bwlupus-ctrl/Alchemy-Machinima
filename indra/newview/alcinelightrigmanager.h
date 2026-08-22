/**
 * @file alcinelightrigmanager.h
 * @brief Five-slot owner and editing-buffer router for cinematic light rigs.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_CINE_LIGHT_RIG_MANAGER_H
#define AL_CINE_LIGHT_RIG_MANAGER_H

#include "alcinelightrig.h"

#include <cmath>
#include <string>

struct ALCineLightRigParamBlob
{
    struct Light
    {
        F32 mYaw = 0.f;
        F32 mPitch = 0.f;
        S32 mProfile = 0;
        F32 mEV = 0.f;
        S32 mBeam = 0;
        S32 mGobo = 0;
        S32 mGel = 0;
        S32 mFlickerProgram = ALCineLightRigModel::FLICKER_NONE;
        F32 mFlickerAmount = 0.f;
        F32 mShadowSoft = 0.f;
        bool mOn = false;
        bool mFixtureMode = false;
        F32 mKelvin = 5600.f;
        S32 mGelSlot[ALCineLightRigModel::FIXTURE_GEL_SLOT_COUNT] = {};
        F32 mSourceSizeM = 0.10f;
        S32 mFixturePreset = 0;
    };

    // The complete selected-instance editing buffer. Keep this list in
    // settings.xml order so omissions are visible in review.
    bool mEnabled = false;                    // CineLightRigEnabled
    bool mScaleAware = true;                  // CineLightRigScaleAware
    bool mPower = true;                       // CineLightRigPower
    F32 mRadius = 1.5f;                       // CineLightRigRadius
    F32 mMasterEV = 0.f;                      // CineLightRigMasterEV
    F32 mMasterTempMired = 0.f;               // CineLightRigMasterTempMired
    F32 mOffsetZ = 0.f;                       // CineLightRigOffsetZ
    F32 mHeadroomStops = 2.f;                 // CineLightRigHeadroomStops
    bool mBounceEnabled = true;               // CineLightRigBounceEnabled
    F32 mBounceRatio = 0.45f;                 // CineLightRigBounceRatio
    F32 mTransitionSec = 0.9f;                // CineLightRigTransitionSec
    F32 mDamping = 0.f;                       // CineLightRigDamping
    S32 mTrackMode = 0;                       // CineLightRigTrackMode
    LLUUID mObjectTarget;                     // CineLightRigObjectTarget
    std::string mCookieUUID =
        "5748decc-f629-461c-9a36-a35a221fe21f"; // CineLightRigCookieUUID
    U32 mSeed = 324508639u;                   // CineLightRigSeed
    bool mMirror = false;                     // CineLightRigMirror
    F32 mOrbitYaw = 0.f;                      // CineLightRigOrbitYaw
    F32 mOrbitPitch = 0.f;                    // CineLightRigOrbitPitch
    S32 mFX = -1;                             // CineLightRigFX
    S32 mShadowMode = 1;                      // CineLightRigShadowMode
    bool mGizmo = false;                      // CineLightRigGizmo
    bool mRatioLock = false;                  // CineLightRigRatioLock
    F32 mRatio = 2.f;                         // CineLightRigRatio
    bool mCatchlight = false;                 // CineLightRigCatchlight
    F32 mCatchlightEV = -0.5f;                // CineLightRigCatchlightEV
    F32 mCatchlightSize = 0.35f;              // CineLightRigCatchlightSize
    F32 mCatchlightAngle = 120.f;             // CineLightRigCatchlightAngle
    // Per-light fields enumerate Yaw/Pitch/Profile/EV/Beam/Gobo/Gel,
    // fixture mode/preset/Kelvin/three gel slots/source size,
    // Flicker/FlickerAmount/ShadowSoft/On for Key, Fill, Rim and Bg.
    // CineLightRigKeyYaw, CineLightRigKeyPitch, CineLightRigKeyProfile,
    // CineLightRigKeyEV, CineLightRigKeyBeam, CineLightRigKeyGobo,
    // CineLightRigKeyGel, CineLightRigKeyFlicker,
    // CineLightRigKeyFlickerAmount, CineLightRigKeyShadowSoft,
    // CineLightRigKeyOn;
    // CineLightRigFillYaw, CineLightRigFillPitch, CineLightRigFillProfile,
    // CineLightRigFillEV, CineLightRigFillBeam, CineLightRigFillGobo,
    // CineLightRigFillGel, CineLightRigFillFlicker,
    // CineLightRigFillFlickerAmount, CineLightRigFillShadowSoft,
    // CineLightRigFillOn;
    // CineLightRigRimYaw, CineLightRigRimPitch, CineLightRigRimProfile,
    // CineLightRigRimEV, CineLightRigRimBeam, CineLightRigRimGobo,
    // CineLightRigRimGel, CineLightRigRimFlicker,
    // CineLightRigRimFlickerAmount, CineLightRigRimShadowSoft,
    // CineLightRigRimOn;
    // CineLightRigBgYaw, CineLightRigBgPitch, CineLightRigBgProfile,
    // CineLightRigBgEV, CineLightRigBgBeam, CineLightRigBgGobo,
    // CineLightRigBgGel, CineLightRigBgFlicker,
    // CineLightRigBgFlickerAmount, CineLightRigBgShadowSoft,
    // CineLightRigBgOn.
    Light mLights[ALCineLightRigModel::LIGHT_COUNT] = {
        { 45.f,  35.f, 3,  0.f, 1, 0, 0,
          ALCineLightRigModel::FLICKER_NONE, 0.f, 0.f, true  },
        {-45.f,   5.f, 3, -2.f, 1, 0, 0,
          ALCineLightRigModel::FLICKER_NONE, 0.f, 0.f, true  },
        {-135.f, 45.f, 4, -1.f, 0, 0, 0,
          ALCineLightRigModel::FLICKER_NONE, 0.f, 0.f, true },
        {  0.f, -20.f, 3,  0.f, 0, 0, 0,
          ALCineLightRigModel::FLICKER_NONE, 0.f, 0.f, false },
    };

    // Per-instance session state. Runtime-derived emitter/smoothing/transition
    // state deliberately remains on ALCineLightRig.
    LLUUID mAnchor;
    bool mGroupEnabled = false;
    U32 mGroupSlots = 0;
    bool mShaftEnabled[ALCineLightRigModel::LIGHT_COUNT] = {};
    bool mHeroEnabled[ALCineLightRigModel::LIGHT_COUNT] = {};
    F64 mFXPhase = 0.0;
    F64 mPendingFXPhase = -1.0;
    S32 mPendingFXId = -1;
    LLSD mCueList;

    static ALCineLightRigParamBlob fromSettings(
        const ALCineLightRig* rig = nullptr);
    void toSettings() const;
    void captureRigState(const ALCineLightRig& rig);
    void applyRigState(ALCineLightRig& rig) const;

    LLSD toLLSD() const;
    static ALCineLightRigParamBlob fromLLSD(const LLSD& data);

    template <typename Settings>
    static ALCineLightRigParamBlob fromSettingsStore(Settings& settings)
    {
        ALCineLightRigParamBlob blob;
        blob.mEnabled = settings.getBOOL("CineLightRigEnabled");
        blob.mScaleAware = settings.getBOOL("CineLightRigScaleAware");
        blob.mPower = settings.getBOOL("CineLightRigPower");
        blob.mRadius = settings.getF32("CineLightRigRadius");
        blob.mMasterEV = settings.getF32("CineLightRigMasterEV");
        blob.mMasterTempMired =
            settings.getF32("CineLightRigMasterTempMired");
        blob.mOffsetZ = settings.getF32("CineLightRigOffsetZ");
        blob.mHeadroomStops = settings.getF32("CineLightRigHeadroomStops");
        blob.mBounceEnabled = settings.getBOOL("CineLightRigBounceEnabled");
        blob.mBounceRatio = settings.getF32("CineLightRigBounceRatio");
        blob.mTransitionSec = settings.getF32("CineLightRigTransitionSec");
        blob.mDamping = settings.getF32("CineLightRigDamping");
        blob.mTrackMode = settings.getS32("CineLightRigTrackMode");
        blob.mObjectTarget.set(
            settings.getString("CineLightRigObjectTarget"), false);
        blob.mCookieUUID = settings.getString("CineLightRigCookieUUID");
        blob.mSeed = settings.getU32("CineLightRigSeed");
        blob.mMirror = settings.getBOOL("CineLightRigMirror");
        blob.mOrbitYaw = settings.getF32("CineLightRigOrbitYaw");
        blob.mOrbitPitch = settings.getF32("CineLightRigOrbitPitch");
        blob.mFX = settings.getS32("CineLightRigFX");
        blob.mShadowMode = settings.getS32("CineLightRigShadowMode");
        blob.mGizmo = settings.getBOOL("CineLightRigGizmo");
        blob.mRatioLock = settings.getBOOL("CineLightRigRatioLock");
        blob.mRatio = settings.getF32("CineLightRigRatio");
        blob.mCatchlight = settings.getBOOL("CineLightRigCatchlight");
        blob.mCatchlightEV = settings.getF32("CineLightRigCatchlightEV");
        blob.mCatchlightSize = settings.getF32("CineLightRigCatchlightSize");
        blob.mCatchlightAngle = settings.getF32("CineLightRigCatchlightAngle");

        static const char* const prefixes[
            ALCineLightRigModel::LIGHT_COUNT] = {
                "CineLightRigKey", "CineLightRigFill",
                "CineLightRigRim", "CineLightRigBg"
            };
        for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
        {
            const std::string prefix(prefixes[i]);
            blob.mLights[i].mYaw = settings.getF32(prefix + "Yaw");
            blob.mLights[i].mPitch = settings.getF32(prefix + "Pitch");
            blob.mLights[i].mProfile = settings.getS32(prefix + "Profile");
            blob.mLights[i].mEV = settings.getF32(prefix + "EV");
            blob.mLights[i].mBeam = settings.getS32(prefix + "Beam");
            blob.mLights[i].mGobo = settings.getS32(prefix + "Gobo");
            blob.mLights[i].mGel = settings.getS32(prefix + "Gel");
            blob.mLights[i].mShadowSoft =
                settings.getF32(prefix + "ShadowSoft");
            blob.mLights[i].mOn = settings.getBOOL(prefix + "On");
            blob.mLights[i].mFlickerProgram =
                settings.getS32(prefix + "Flicker");
            blob.mLights[i].mFlickerAmount =
                settings.getF32(prefix + "FlickerAmount");
            blob.mLights[i].mFixtureMode =
                settings.getBOOL(prefix + "FixtureMode");
            blob.mLights[i].mKelvin = settings.getF32(prefix + "Kelvin");
            for (S32 slot = 0;
                 slot < ALCineLightRigModel::FIXTURE_GEL_SLOT_COUNT; ++slot)
            {
                blob.mLights[i].mGelSlot[slot] = settings.getS32(
                    prefix + "GelSlot" + std::to_string(slot));
            }
            blob.mLights[i].mSourceSizeM =
                settings.getF32(prefix + "SourceSizeM");
            blob.mLights[i].mFixturePreset =
                settings.getS32(prefix + "FixturePreset");
        }
        return blob;
    }

    template <typename Settings>
    void toSettingsStore(Settings& settings) const
    {
        settings.setBOOL("CineLightRigEnabled", mEnabled);
        settings.setBOOL("CineLightRigScaleAware", mScaleAware);
        settings.setBOOL("CineLightRigPower", mPower);
        settings.setF32("CineLightRigRadius", mRadius);
        settings.setF32("CineLightRigMasterEV", mMasterEV);
        settings.setF32("CineLightRigMasterTempMired", mMasterTempMired);
        settings.setF32("CineLightRigOffsetZ", mOffsetZ);
        settings.setF32("CineLightRigHeadroomStops", mHeadroomStops);
        settings.setBOOL("CineLightRigBounceEnabled", mBounceEnabled);
        settings.setF32("CineLightRigBounceRatio", mBounceRatio);
        settings.setF32("CineLightRigTransitionSec", mTransitionSec);
        settings.setF32("CineLightRigDamping", mDamping);
        settings.setS32("CineLightRigTrackMode", mTrackMode);
        settings.setString(
            "CineLightRigObjectTarget", mObjectTarget.asString());
        settings.setString("CineLightRigCookieUUID", mCookieUUID);
        settings.setU32("CineLightRigSeed", mSeed);
        settings.setBOOL("CineLightRigMirror", mMirror);
        settings.setF32("CineLightRigOrbitYaw", mOrbitYaw);
        settings.setF32("CineLightRigOrbitPitch", mOrbitPitch);
        settings.setS32("CineLightRigFX", mFX);
        settings.setS32("CineLightRigShadowMode", mShadowMode);
        settings.setBOOL("CineLightRigGizmo", mGizmo);
        settings.setBOOL("CineLightRigRatioLock", mRatioLock);
        settings.setF32("CineLightRigRatio", mRatio);
        settings.setBOOL("CineLightRigCatchlight", mCatchlight);
        settings.setF32("CineLightRigCatchlightEV", mCatchlightEV);
        settings.setF32("CineLightRigCatchlightSize", mCatchlightSize);
        settings.setF32("CineLightRigCatchlightAngle", mCatchlightAngle);

        static const char* const prefixes[
            ALCineLightRigModel::LIGHT_COUNT] = {
                "CineLightRigKey", "CineLightRigFill",
                "CineLightRigRim", "CineLightRigBg"
            };
        for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
        {
            const std::string prefix(prefixes[i]);
            settings.setF32(prefix + "Yaw", mLights[i].mYaw);
            settings.setF32(prefix + "Pitch", mLights[i].mPitch);
            settings.setS32(prefix + "Profile", mLights[i].mProfile);
            settings.setF32(prefix + "EV", mLights[i].mEV);
            settings.setS32(prefix + "Beam", mLights[i].mBeam);
            settings.setS32(prefix + "Gobo", mLights[i].mGobo);
            settings.setS32(prefix + "Gel", mLights[i].mGel);
            settings.setF32(prefix + "ShadowSoft",
                            mLights[i].mShadowSoft);
            settings.setBOOL(prefix + "On", mLights[i].mOn);
            settings.setS32(prefix + "Flicker", mLights[i].mFlickerProgram);
            settings.setF32(
                prefix + "FlickerAmount", mLights[i].mFlickerAmount);
            settings.setBOOL(prefix + "FixtureMode", mLights[i].mFixtureMode);
            settings.setF32(prefix + "Kelvin", mLights[i].mKelvin);
            for (S32 slot = 0;
                 slot < ALCineLightRigModel::FIXTURE_GEL_SLOT_COUNT; ++slot)
            {
                settings.setS32(prefix + "GelSlot" + std::to_string(slot),
                                mLights[i].mGelSlot[slot]);
            }
            settings.setF32(prefix + "SourceSizeM", mLights[i].mSourceSizeM);
            settings.setS32(
                prefix + "FixturePreset", mLights[i].mFixturePreset);
        }
    }
};

inline LLSD ALCineLightRigParamBlob::toLLSD() const
{
    LLSD data = LLSD::emptyMap();
    data["CineLightRigEnabled"] = mEnabled;
    data["CineLightRigScaleAware"] = mScaleAware;
    data["CineLightRigPower"] = mPower;
    data["CineLightRigRadius"] = mRadius;
    data["CineLightRigMasterEV"] = mMasterEV;
    data["CineLightRigMasterTempMired"] = mMasterTempMired;
    data["CineLightRigOffsetZ"] = mOffsetZ;
    data["CineLightRigHeadroomStops"] = mHeadroomStops;
    data["CineLightRigBounceEnabled"] = mBounceEnabled;
    data["CineLightRigBounceRatio"] = mBounceRatio;
    data["CineLightRigTransitionSec"] = mTransitionSec;
    data["CineLightRigDamping"] = mDamping;
    data["CineLightRigTrackMode"] = mTrackMode;
    data["CineLightRigObjectTarget"] = mObjectTarget;
    data["CineLightRigCookieUUID"] = mCookieUUID;
    // LLSD real exactly represents every U32 and avoids signed-S32 rollover.
    data["CineLightRigSeed"] = static_cast<F64>(mSeed);
    data["CineLightRigMirror"] = mMirror;
    data["CineLightRigOrbitYaw"] = mOrbitYaw;
    data["CineLightRigOrbitPitch"] = mOrbitPitch;
    data["CineLightRigFX"] = mFX;
    data["CineLightRigShadowMode"] = mShadowMode;
    data["CineLightRigGizmo"] = mGizmo;
    data["CineLightRigRatioLock"] = mRatioLock;
    data["CineLightRigRatio"] = mRatio;
    data["CineLightRigCatchlight"] = mCatchlight;
    data["CineLightRigCatchlightEV"] = mCatchlightEV;
    data["CineLightRigCatchlightSize"] = mCatchlightSize;
    data["CineLightRigCatchlightAngle"] = mCatchlightAngle;
    data["lights"] = LLSD::emptyArray();
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        LLSD light = LLSD::emptyMap();
        light["yaw"] = mLights[i].mYaw;
        light["pitch"] = mLights[i].mPitch;
        light["profile"] = mLights[i].mProfile;
        light["ev"] = mLights[i].mEV;
        light["beam"] = mLights[i].mBeam;
        light["gobo"] = mLights[i].mGobo;
        light["gel"] = mLights[i].mGel;
        light["shadow_soft"] = mLights[i].mShadowSoft;
        light["on"] = mLights[i].mOn;
        light["flicker_program"] = mLights[i].mFlickerProgram;
        light["flicker_amount"] = mLights[i].mFlickerAmount;
        light["fixture_mode"] = mLights[i].mFixtureMode;
        light["kelvin"] = mLights[i].mKelvin;
        light["source_size_m"] = mLights[i].mSourceSizeM;
        light["fixture_preset"] = mLights[i].mFixturePreset;
        light["fixture_gel_slots"] = LLSD::emptyArray();
        for (S32 slot = 0;
             slot < ALCineLightRigModel::FIXTURE_GEL_SLOT_COUNT; ++slot)
        {
            light["fixture_gel_slots"].append(mLights[i].mGelSlot[slot]);
        }
        data["lights"].append(light);
    }
    data["anchor"] = mAnchor;
    data["group"] = mGroupEnabled;
    data["group_slots"] = static_cast<S32>(mGroupSlots);
    data["shafts"] = LLSD::emptyArray();
    data["heroes"] = LLSD::emptyArray();
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        data["shafts"].append(mShaftEnabled[i]);
        data["heroes"].append(mHeroEnabled[i]);
    }
    data["fx_phase"] = mFXPhase;
    data["pending_fx_phase"] = mPendingFXPhase;
    data["pending_fx_id"] = mPendingFXId;
    if (mCueList.isMap())
    {
        data["cue_list"] = mCueList;
    }
    return data;
}

inline ALCineLightRigParamBlob ALCineLightRigParamBlob::fromLLSD(
    const LLSD& data)
{
    ALCineLightRigParamBlob blob;
    if (!data.isMap())
    {
        return blob;
    }
#define AL_CINE_READ_BOOL(KEY, FIELD) \
    if (data.has(KEY)) FIELD = data[KEY].asBoolean()
#define AL_CINE_READ_S32(KEY, FIELD) \
    if (data.has(KEY)) FIELD = data[KEY].asInteger()
#define AL_CINE_READ_F32(KEY, FIELD) \
    if (data.has(KEY)) FIELD = static_cast<F32>(data[KEY].asReal())
    AL_CINE_READ_BOOL("CineLightRigEnabled", blob.mEnabled);
    AL_CINE_READ_BOOL("CineLightRigScaleAware", blob.mScaleAware);
    AL_CINE_READ_BOOL("CineLightRigPower", blob.mPower);
    AL_CINE_READ_F32("CineLightRigRadius", blob.mRadius);
    AL_CINE_READ_F32("CineLightRigMasterEV", blob.mMasterEV);
    AL_CINE_READ_F32("CineLightRigMasterTempMired", blob.mMasterTempMired);
    AL_CINE_READ_F32("CineLightRigOffsetZ", blob.mOffsetZ);
    AL_CINE_READ_F32("CineLightRigHeadroomStops", blob.mHeadroomStops);
    AL_CINE_READ_BOOL("CineLightRigBounceEnabled", blob.mBounceEnabled);
    AL_CINE_READ_F32("CineLightRigBounceRatio", blob.mBounceRatio);
    AL_CINE_READ_F32("CineLightRigTransitionSec", blob.mTransitionSec);
    AL_CINE_READ_F32("CineLightRigDamping", blob.mDamping);
    AL_CINE_READ_S32("CineLightRigTrackMode", blob.mTrackMode);
    if (data.has("CineLightRigObjectTarget"))
        blob.mObjectTarget = data["CineLightRigObjectTarget"].asUUID();
    if (data.has("CineLightRigCookieUUID"))
        blob.mCookieUUID = data["CineLightRigCookieUUID"].asString();
    if (data.has("CineLightRigSeed"))
        blob.mSeed = static_cast<U32>(data["CineLightRigSeed"].asReal());
    AL_CINE_READ_BOOL("CineLightRigMirror", blob.mMirror);
    AL_CINE_READ_F32("CineLightRigOrbitYaw", blob.mOrbitYaw);
    AL_CINE_READ_F32("CineLightRigOrbitPitch", blob.mOrbitPitch);
    AL_CINE_READ_S32("CineLightRigFX", blob.mFX);
    AL_CINE_READ_S32("CineLightRigShadowMode", blob.mShadowMode);
    AL_CINE_READ_BOOL("CineLightRigGizmo", blob.mGizmo);
    AL_CINE_READ_BOOL("CineLightRigRatioLock", blob.mRatioLock);
    AL_CINE_READ_F32("CineLightRigRatio", blob.mRatio);
    AL_CINE_READ_BOOL("CineLightRigCatchlight", blob.mCatchlight);
    AL_CINE_READ_F32("CineLightRigCatchlightEV", blob.mCatchlightEV);
    AL_CINE_READ_F32("CineLightRigCatchlightSize", blob.mCatchlightSize);
    AL_CINE_READ_F32("CineLightRigCatchlightAngle", blob.mCatchlightAngle);
#undef AL_CINE_READ_BOOL
#undef AL_CINE_READ_S32
#undef AL_CINE_READ_F32
    if (data["lights"].isArray() &&
        data["lights"].size() == ALCineLightRigModel::LIGHT_COUNT)
    {
        for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
        {
            const LLSD& light = data["lights"][i];
            if (!light.isMap()) continue;
            if (light.has("yaw"))
                blob.mLights[i].mYaw = static_cast<F32>(light["yaw"].asReal());
            if (light.has("pitch"))
                blob.mLights[i].mPitch = static_cast<F32>(light["pitch"].asReal());
            if (light.has("profile"))
                blob.mLights[i].mProfile = light["profile"].asInteger();
            if (light.has("ev"))
                blob.mLights[i].mEV = static_cast<F32>(light["ev"].asReal());
            if (light.has("beam"))
                blob.mLights[i].mBeam = light["beam"].asInteger();
            if (light.has("gobo"))
                blob.mLights[i].mGobo = light["gobo"].asInteger();
            if (light.has("gel"))
                blob.mLights[i].mGel = light["gel"].asInteger();
            if (light.has("shadow_soft"))
                blob.mLights[i].mShadowSoft =
                    static_cast<F32>(light["shadow_soft"].asReal());
            if (light.has("on"))
                blob.mLights[i].mOn = light["on"].asBoolean();
            if (light.has("flicker_program"))
                blob.mLights[i].mFlickerProgram =
                    light["flicker_program"].asInteger();
            if (light.has("flicker_amount"))
                blob.mLights[i].mFlickerAmount =
                    static_cast<F32>(light["flicker_amount"].asReal());
            if (light.has("fixture_mode"))
                blob.mLights[i].mFixtureMode =
                    light["fixture_mode"].asBoolean();
            if (light.has("kelvin"))
                blob.mLights[i].mKelvin =
                    static_cast<F32>(light["kelvin"].asReal());
            if (light.has("source_size_m"))
                blob.mLights[i].mSourceSizeM =
                    static_cast<F32>(light["source_size_m"].asReal());
            if (light.has("fixture_preset"))
                blob.mLights[i].mFixturePreset =
                    light["fixture_preset"].asInteger();
            if (light["fixture_gel_slots"].isArray() &&
                light["fixture_gel_slots"].size() ==
                    ALCineLightRigModel::FIXTURE_GEL_SLOT_COUNT)
            {
                for (S32 slot = 0;
                     slot < ALCineLightRigModel::FIXTURE_GEL_SLOT_COUNT;
                     ++slot)
                {
                    blob.mLights[i].mGelSlot[slot] =
                        light["fixture_gel_slots"][slot].asInteger();
                }
            }
        }
    }
    if (data.has("anchor")) blob.mAnchor = data["anchor"].asUUID();
    if (data.has("group")) blob.mGroupEnabled = data["group"].asBoolean();
    if (data.has("group_slots"))
        blob.mGroupSlots = static_cast<U32>(data["group_slots"].asInteger()) &
            ALCineLightRig::GROUP_SLOT_MASK;
    if (data["shafts"].isArray() &&
        data["shafts"].size() == ALCineLightRigModel::LIGHT_COUNT)
    {
        for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
            blob.mShaftEnabled[i] = data["shafts"][i].asBoolean();
    }
    if (data["heroes"].isArray() &&
        data["heroes"].size() == ALCineLightRigModel::LIGHT_COUNT)
    {
        for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
            blob.mHeroEnabled[i] = data["heroes"][i].asBoolean();
    }
    if (data.has("fx_phase")) blob.mFXPhase = data["fx_phase"].asReal();
    if (data.has("pending_fx_phase"))
        blob.mPendingFXPhase = data["pending_fx_phase"].asReal();
    if (data.has("pending_fx_id"))
        blob.mPendingFXId = data["pending_fx_id"].asInteger();
    if (data["cue_list"].isMap())
        blob.mCueList = data["cue_list"];
    return blob;
}

namespace ALCineLightRigManagerModel
{
constexpr F64 FOCUS_MARGIN_M = 0.75;
constexpr S32 FOCUS_DWELL_TICKS = 20;

struct FocusPoint
{
    F64 mX = 0.0;
    F64 mY = 0.0;
    F64 mZ = 0.0;
    bool mValid = false;
};

struct FocusFilterState
{
    ALCineLightRigSlot mFocus = ALCineLightRigSlot::SELF;
    ALCineLightRigSlot mChallenger = ALCineLightRigSlot::SELF;
    S32 mChallengeTicks = 0;
};

enum class TickPath : S32
{
    SHUTDOWN = 0,
    TICK_SELECTED_VERBATIM,
    TICK_FROM_BLOB
};

struct MigrationSlot
{
    ALCineLightRigSlot mSlot = ALCineLightRigSlot::SELF;
    bool mKeepLegacyAnchor = false;
};

struct AutoShadowSlotState
{
    bool mRaised = false;
    U32 mBaseline = 0;
    U32 mLastWritten = 0;
};

struct AutoShadowSlotUpdate
{
    bool mWrite = false;
    U32 mValue = 0;
};

inline bool relinquishAutoShadowSlotsIfExternallyChanged(
    AutoShadowSlotState& state, U32 current)
{
    if (!state.mRaised || current == state.mLastWritten)
    {
        return false;
    }

    state = AutoShadowSlotState();
    return true;
}

inline U32 requestedShadowSlots(const ALCineLightRigParamBlob& blob)
{
    if (!blob.mEnabled || !blob.mPower)
    {
        return 0;
    }
    const S32 mode = blob.mShadowMode < 0 ? 0
        : blob.mShadowMode > 2 ? 2 : blob.mShadowMode;
    if (mode == 1)
    {
        return blob.mLights[0].mOn ? 1u : 0u;
    }
    if (mode == 2)
    {
        U32 requested = 0;
        for (S32 light = 0; light < ALCineLightRigModel::LIGHT_COUNT;
             ++light)
        {
            requested += blob.mLights[light].mOn ? 1u : 0u;
        }
        return requested;
    }
    return 0;
}

inline U32 requestedShadowSlots(const ALCineLightRigParamBlob* blobs,
                                S32 count, U32 ceiling)
{
    U32 requested = 0;
    for (S32 i = 0; i < count && requested < ceiling; ++i)
    {
        const U32 instance_request = requestedShadowSlots(blobs[i]);
        requested = instance_request > ceiling - requested
            ? ceiling : requested + instance_request;
    }
    return requested;
}

inline AutoShadowSlotUpdate updateAutoShadowSlots(
    AutoShadowSlotState& state, bool enabled, U32 requested,
    U32 hardware_max, U32 current)
{
    AutoShadowSlotUpdate update;
    update.mValue = current;

    // A write by another owner invalidates the saved baseline. Relinquish
    // without immediately fighting that write on the same input transition.
    if (relinquishAutoShadowSlotsIfExternallyChanged(state, current))
    {
        return update;
    }

    if (!enabled || requested == 0)
    {
        if (state.mRaised)
        {
            update.mWrite = true;
            update.mValue = state.mBaseline;
        }
        state = AutoShadowSlotState();
        return update;
    }

    const U32 target = requested < hardware_max ? requested : hardware_max;
    if (target > current)
    {
        if (!state.mRaised)
        {
            state.mBaseline = current;
        }
        state.mRaised = true;
        state.mLastWritten = target;
        update.mWrite = true;
        update.mValue = target;
    }
    return update;
}

inline U32 slotToGroupBit(ALCineLightRigSlot slot)
{
    const S32 index = static_cast<S32>(slot);
    return index >= 0 && index < static_cast<S32>(ALCineLightRigSlot::COUNT)
        ? 1u << index : 0u;
}

inline ALCineLightRigSlot groupBitToSlot(U32 bit)
{
    for (S32 i = 0; i < static_cast<S32>(ALCineLightRigSlot::COUNT); ++i)
    {
        if (bit == (1u << i))
        {
            return static_cast<ALCineLightRigSlot>(i);
        }
    }
    return ALCineLightRigSlot::COUNT;
}

inline S32 enabledCount(U32 enabled_mask)
{
    S32 count = 0;
    for (S32 i = 0; i < static_cast<S32>(ALCineLightRigSlot::COUNT); ++i)
    {
        count += (enabled_mask & (1u << i)) ? 1 : 0;
    }
    return count;
}

inline ALCineLightRigSlot lowestEnabled(U32 enabled_mask)
{
    for (S32 i = 0; i < static_cast<S32>(ALCineLightRigSlot::COUNT); ++i)
    {
        if (enabled_mask & (1u << i))
        {
            return static_cast<ALCineLightRigSlot>(i);
        }
    }
    return ALCineLightRigSlot::COUNT;
}

inline ALCineLightRigSlot rawFocusSlot(
    const FocusPoint points[static_cast<S32>(ALCineLightRigSlot::COUNT)],
    U32 enabled_mask, const FocusPoint& focus_point,
    const FocusPoint& at_axis, F64* chosen_distance = nullptr)
{
    if (!focus_point.mValid || !std::isfinite(focus_point.mX) ||
        !std::isfinite(focus_point.mY) || !std::isfinite(focus_point.mZ))
    {
        return ALCineLightRigSlot::COUNT;
    }
    const F64 axis_length = std::sqrt(at_axis.mX * at_axis.mX +
        at_axis.mY * at_axis.mY + at_axis.mZ * at_axis.mZ);
    const bool have_axis = at_axis.mValid && std::isfinite(axis_length) &&
        axis_length > 0.0;
    ALCineLightRigSlot best = ALCineLightRigSlot::COUNT;
    F64 best_distance_sq = 0.0;
    F64 best_axis_score = -2.0;
    for (S32 i = 0; i < static_cast<S32>(ALCineLightRigSlot::COUNT); ++i)
    {
        const FocusPoint& point = points[i];
        if (!(enabled_mask & (1u << i)) || !point.mValid ||
            !std::isfinite(point.mX) || !std::isfinite(point.mY) ||
            !std::isfinite(point.mZ))
        {
            continue;
        }
        const F64 dx = point.mX - focus_point.mX;
        const F64 dy = point.mY - focus_point.mY;
        const F64 dz = point.mZ - focus_point.mZ;
        const F64 distance_sq = dx * dx + dy * dy + dz * dz;
        const F64 distance = std::sqrt(distance_sq);
        const F64 axis_score = have_axis && distance > 0.0
            ? (dx * at_axis.mX + dy * at_axis.mY + dz * at_axis.mZ) /
                (distance * axis_length)
            : 1.0;
        if (best == ALCineLightRigSlot::COUNT ||
            distance_sq < best_distance_sq ||
            (distance_sq == best_distance_sq && axis_score > best_axis_score))
        {
            best = static_cast<ALCineLightRigSlot>(i);
            best_distance_sq = distance_sq;
            best_axis_score = axis_score;
        }
    }
    if (chosen_distance && best != ALCineLightRigSlot::COUNT)
    {
        *chosen_distance = std::sqrt(best_distance_sq);
    }
    return best;
}

inline bool focusFilter(FocusFilterState& state, ALCineLightRigSlot raw,
                        F64 raw_distance, F64 incumbent_distance)
{
    if (raw == ALCineLightRigSlot::COUNT || raw == state.mFocus)
    {
        state.mChallenger = state.mFocus;
        state.mChallengeTicks = 0;
        return false;
    }
    if (!std::isfinite(incumbent_distance))
    {
        state.mFocus = raw;
        state.mChallenger = raw;
        state.mChallengeTicks = 0;
        return true;
    }
    if (!std::isfinite(raw_distance) ||
        raw_distance >= incumbent_distance - FOCUS_MARGIN_M)
    {
        state.mChallenger = raw;
        state.mChallengeTicks = 0;
        return false;
    }
    if (state.mChallenger != raw)
    {
        state.mChallenger = raw;
        state.mChallengeTicks = 1;
    }
    else
    {
        ++state.mChallengeTicks;
    }
    if (state.mChallengeTicks < FOCUS_DWELL_TICKS)
    {
        return false;
    }
    state.mFocus = raw;
    state.mChallenger = raw;
    state.mChallengeTicks = 0;
    return true;
}

inline U32 suppressedSlotMask(U32 enabled_mask,
                              ALCineLightRigSlot focus_slot)
{
    const U32 slot_mask =
        (1u << static_cast<S32>(ALCineLightRigSlot::COUNT)) - 1u;
    return (enabled_mask & slot_mask) & ~slotToGroupBit(focus_slot);
}

inline TickPath pathFor(U32 enabled_mask, ALCineLightRigSlot selected,
                        ALCineLightRigSlot slot)
{
    if (slot == selected)
    {
        return TickPath::TICK_SELECTED_VERBATIM;
    }
    if (!(enabled_mask & slotToGroupBit(slot)))
    {
        return TickPath::SHUTDOWN;
    }
    return TickPath::TICK_FROM_BLOB;
}

inline ALCineLightRigSlot normalizeSelected(
    U32 enabled_mask, ALCineLightRigSlot selected)
{
    return enabledCount(enabled_mask) == 1
        ? lowestEnabled(enabled_mask) : selected;
}

inline MigrationSlot migrateAnchorToSlot(
    const LLUUID& anchor, const LLUUID subject_ids[4])
{
    if (anchor.isNull())
    {
        return MigrationSlot();
    }
    for (S32 i = 0; i < 4; ++i)
    {
        if (anchor == subject_ids[i])
        {
            MigrationSlot result;
            result.mSlot = static_cast<ALCineLightRigSlot>(i + 1);
            return result;
        }
    }
    MigrationSlot result;
    result.mKeepLegacyAnchor = true;
    return result;
}
}

class ALCineLightRigManager
{
public:
    using Slot = ALCineLightRigSlot;
    using ParamBlob = ALCineLightRigParamBlob;

    static constexpr Slot SLOT_SELF = Slot::SELF;
    static constexpr Slot SLOT_A = Slot::A;
    static constexpr Slot SLOT_B = Slot::B;
    static constexpr Slot SLOT_C = Slot::C;
    static constexpr Slot SLOT_D = Slot::D;
    static constexpr S32 SLOT_COUNT = static_cast<S32>(Slot::COUNT);

    static ALCineLightRigManager& instance();

    void tick(F64 presentation_time);
    void shutdown();
    void renderGizmo() const;
    LLSD sceneData() const;
    void applySceneData(const LLSD& data);

    ALCineLightRig& selected();
    const ALCineLightRig& selected() const;
    Slot selectedSlot() const { return mSelected; }
    void setSelectedSlot(Slot slot);
    void resetAllToSelf();

    ALCineLightRig& at(Slot slot);
    const ALCineLightRig& at(Slot slot) const;
    bool isSlotEnabled(Slot slot) const;
    bool isSlotLit(Slot slot) const;
    U32 enabledMask() const;
    S32 enabledCount() const;
    U32 requestedShadowSlots(U32 ceiling) const;
    Slot cameraFocusSlot() const { return mFocusState.mFocus; }

private:
    ALCineLightRigManager();

    static bool validSlot(Slot slot);
    void updateAutoShadowSlots();
    void restoreAutoShadowSlots();
    void updateRandomCycle(F64 presentation_time);
    void resolveCameraFocus();
    void applyShadowSuppression();
    void clearShadowSuppression();
    void persist() const;
    bool restore(const LLSD& data);
    void migrateLegacyScene(const LLSD& data);
    ParamBlob captureSelected() const;

    ALCineLightRig mInstances[SLOT_COUNT];
    mutable ParamBlob mBlobs[SLOT_COUNT];
    Slot mSelected = SLOT_SELF;
    ALCineLightRigManagerModel::FocusFilterState mFocusState;
    ALCineLightRigManagerModel::AutoShadowSlotState mAutoShadowSlotState;
    U32 mLastAutoShadowRequest = 0;
    U32 mLastAutoShadowHardwareMax = 0;
    bool mLastAutoShadowEnabled = false;
    bool mAutoShadowInputsInitialized = false;
    bool mNeedsSessionRestore = false;

    // Random setup cycling. Timed off scrub-safe presentation time. The
    // "currently-active" setup used for exclusion is read from the rig's
    // loadedSetupName(), so no separate last-pick tracker is needed.
    F64 mRandomCycleNextTime = -1.0;
    F64 mRandomCycleLastTime = -1.0;
};

#endif // AL_CINE_LIGHT_RIG_MANAGER_H
