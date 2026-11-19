/**
 * @file alvirtuallightsmodel.h
 * @brief Pure model for session-only, client-only "virtual lights".
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * A virtual light is a simple light "prim" that only this viewer sees. This header holds
 * everything about it that needs no viewer: the per-light record, the prim ranges, the
 * sanitiser, aim maths, the placement rules, the list cap and the scene block format. The
 * runtime (alvirtuallights.cpp) owns the objects; nothing here touches the world, the
 * simulator, settings or the disk.
 */
#ifndef AL_VIRTUAL_LIGHTS_MODEL_H
#define AL_VIRTUAL_LIGHTS_MODEL_H

#include "llsd.h"
#include "llsdutil.h"   // LLSD operator==
#include "lluuid.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ALVirtualLightsModel
{
constexpr S32 kMaxLights = 8;
// Prim ranges (lead decision L9): the same ceilings the Build floater enforces.
constexpr F32 kMinRadius = 0.1f;
constexpr F32 kMaxRadius = 20.f;
constexpr F32 kMaxIntensity = 1.f;
constexpr F32 kMinFalloff = 0.01f;
constexpr F32 kMaxFalloff = 2.f;
constexpr F32 kMinFov = 0.1f;
constexpr F32 kMaxFov = 3.f;
constexpr F32 kMaxFocus = 20.f;
constexpr F32 kMaxAmbiance = 1.f;
constexpr F32 kMaxCoordinate = 4096.f;   // region-local; refuses nonsense, never clamps a real spot
constexpr F32 kMinCoordinate = -64.f;
constexpr F32 kMaxHeight = 4096.f;
constexpr F32 kLiftM = 0.25f;            // placement lift along the surface normal (L4)
constexpr F32 kDefaultIntensity = 1.f;
constexpr F32 kDefaultRadius = 10.f;
constexpr F32 kDefaultFalloff = 0.75f;
constexpr F32 kDefaultFov = 1.5f;
constexpr S32 kJointCount = 7;
constexpr F32 kPi = 3.14159265358979f;
constexpr char kDefaultCookie[] = "5748decc-f629-461c-9a36-a35a221fe21f";

inline F32 finiteOr(F32 value, F32 fallback)
{
    return std::isfinite(value) ? value : fallback;
}
inline F32 clampTo(F32 value, F32 lo, F32 hi, F32 fallback)
{
    return std::clamp(finiteOr(value, fallback), lo, hi);
}

// Local Track state of one light (the Practicals Track card, virtual backend).
struct Track
{
    bool  mEnabled = false;
    bool  mHasSubject = false;
    LLUUID mSubject;
    S32   mJoint = 0;
    S32   mMode = 0;          // 0 = Aim, 1 = Follow
    F32   mSmoothing = 0.15f;
};

struct Light
{
    LLUUID mKey;                    // stable identity: scenes, undo and rebinding use it, never the object id
    bool   mOn = true;
    bool   mSpot = false;
    F32    mColor[3] = {1.f, 1.f, 1.f};   // sRGB
    F32    mIntensity = kDefaultIntensity;
    F32    mRadius = kDefaultRadius;
    F32    mFalloff = kDefaultFalloff;
    F32    mPos[3] = {128.f, 128.f, 30.f}; // region-local metres
    F32    mYawDeg = 0.f;
    F32    mPitchDeg = -45.f;
    LLUUID mTexture;                // null = the default cookie
    F32    mFov = kDefaultFov;
    F32    mFocus = 0.f;
    F32    mAmbiance = 0.f;
    LLUUID mVCamCapture;            // VCam feed (capture id), null = none
    LLUUID mAimObject;              // aim at this object every tick, null = use yaw/pitch
    Track  mTrack;
    // Practicals state that LLPipeline keeps by object id: no_shadow, volumetric,
    // gobo{...}, shaft{...}. Stored here so a recreated light gets it back.
    LLSD   mPracticals;
};

inline S32 normalisedJoint(S32 joint)
{
    return std::clamp(joint, S32(0), S32(kJointCount - 1));
}

inline void sanitize(Light& light)
{
    for (F32& c : light.mColor) c = clampTo(c, 0.f, 1.f, 1.f);
    light.mIntensity = clampTo(light.mIntensity, 0.f, kMaxIntensity, kDefaultIntensity);
    light.mRadius = clampTo(light.mRadius, kMinRadius, kMaxRadius, kDefaultRadius);
    light.mFalloff = clampTo(light.mFalloff, kMinFalloff, kMaxFalloff, kDefaultFalloff);
    light.mPos[0] = clampTo(light.mPos[0], kMinCoordinate, kMaxCoordinate, 128.f);
    light.mPos[1] = clampTo(light.mPos[1], kMinCoordinate, kMaxCoordinate, 128.f);
    light.mPos[2] = clampTo(light.mPos[2], -kMaxHeight, kMaxHeight, 30.f);
    light.mYawDeg = std::fmod(finiteOr(light.mYawDeg, 0.f), 360.f);
    light.mPitchDeg = clampTo(light.mPitchDeg, -90.f, 90.f, -45.f);
    light.mFov = clampTo(light.mFov, kMinFov, kMaxFov, kDefaultFov);
    light.mFocus = clampTo(light.mFocus, 0.f, kMaxFocus, 0.f);
    light.mAmbiance = clampTo(light.mAmbiance, 0.f, kMaxAmbiance, 0.f);
    light.mTrack.mJoint = normalisedJoint(light.mTrack.mJoint);
    light.mTrack.mMode = std::clamp(light.mTrack.mMode, S32(0), S32(1));
    light.mTrack.mSmoothing = clampTo(light.mTrack.mSmoothing, 0.f, 2.f, 0.15f);
    if (!light.mTrack.mHasSubject) light.mTrack.mEnabled = false;
    if (!light.mPracticals.isMap()) light.mPracticals = LLSD::emptyMap();
}

// Aim direction in region axes: yaw counter-clockwise from +X, pitch up from the horizon.
inline void directionFromAngles(F32 yaw_deg, F32 pitch_deg, F32 out[3])
{
    const F32 yaw = yaw_deg * kPi / 180.f;
    const F32 pitch = pitch_deg * kPi / 180.f;
    out[0] = std::cos(pitch) * std::cos(yaw);
    out[1] = std::cos(pitch) * std::sin(yaw);
    out[2] = std::sin(pitch);
}

// False (outputs untouched) for a zero or non-finite direction.
inline bool anglesFromDirection(const F32 dir[3], F32& yaw_deg, F32& pitch_deg)
{
    const F32 length = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (!std::isfinite(length) || length < 1.e-6f) return false;
    const F32 z = std::clamp(dir[2] / length, -1.f, 1.f);
    pitch_deg = std::asin(z) * 180.f / kPi;
    if (std::fabs(z) < 0.999999f) yaw_deg = std::atan2(dir[1], dir[0]) * 180.f / kPi;
    return true;
}

// Aim point -> yaw/pitch from the light position. False when the point is on the light.
inline bool aimAt(Light& light, const F32 point[3])
{
    const F32 dir[3] = {point[0] - light.mPos[0], point[1] - light.mPos[1], point[2] - light.mPos[2]};
    F32 yaw = light.mYawDeg, pitch = light.mPitchDeg;
    if (!anglesFromDirection(dir, yaw, pitch)) return false;
    light.mYawDeg = yaw;
    light.mPitchDeg = pitch;
    return true;
}

// Click-to-place: the click point lifted along the surface normal (a zero normal lifts straight up).
inline void liftedPosition(const F32 hit[3], const F32 normal[3], F32 out[3])
{
    F32 n[3] = {normal[0], normal[1], normal[2]};
    F32 length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (!std::isfinite(length) || length < 1.e-4f) { n[0] = 0.f; n[1] = 0.f; n[2] = 1.f; length = 1.f; }
    for (S32 i = 0; i < 3; ++i) out[i] = hit[i] + n[i] / length * kLiftM;
}

enum class Refusal { NONE, AT_CAP, OUTSIDE_REGION, CLEAN_FRAME, NO_REGION };

inline Refusal placementRefusal(S32 count, bool have_region, bool inside_region, bool clean_frame)
{
    if (clean_frame) return Refusal::CLEAN_FRAME;
    if (!have_region) return Refusal::NO_REGION;
    if (count >= kMaxLights) return Refusal::AT_CAP;
    if (!inside_region) return Refusal::OUTSIDE_REGION;
    return Refusal::NONE;
}

inline const char* refusalText(Refusal refusal)
{
    switch (refusal)
    {
    case Refusal::AT_CAP: return "Eight virtual lights are already placed. Delete one first.";
    case Refusal::OUTSIDE_REGION: return "Place the light inside the region you are in.";
    case Refusal::CLEAN_FRAME: return "Virtual lights cannot be placed during a clean-frame take.";
    case Refusal::NO_REGION: return "Not in a region.";
    case Refusal::NONE: break;
    }
    return "";
}

// The list: capped, ordered, keyed.
inline bool containsKey(const std::vector<Light>& lights, const LLUUID& key)
{
    return std::any_of(lights.begin(), lights.end(), [&](const Light& l) { return l.mKey == key; });
}
inline S32 indexOfKey(const std::vector<Light>& lights, const LLUUID& key)
{
    for (size_t i = 0; i < lights.size(); ++i)
        if (lights[i].mKey == key) return static_cast<S32>(i);
    return -1;
}

// ---- LLSD (scene block, undo snapshots) -------------------------------------------------
inline LLSD vec3ToLLSD(const F32 v[3])
{
    LLSD out = LLSD::emptyArray();
    for (S32 i = 0; i < 3; ++i) out.append(LLSD::Real(v[i]));
    return out;
}
inline void vec3FromLLSD(const LLSD& data, F32 out[3])
{
    if (!data.isArray() || data.size() != 3) return;
    for (S32 i = 0; i < 3; ++i) out[i] = static_cast<F32>(data[i].asReal());
}

inline LLSD toLLSD(const Light& light)
{
    LLSD out = LLSD::emptyMap();
    out["key"] = light.mKey;
    out["on"] = light.mOn;
    out["spot"] = light.mSpot;
    out["color"] = vec3ToLLSD(light.mColor);
    out["intensity"] = LLSD::Real(light.mIntensity);
    out["radius"] = LLSD::Real(light.mRadius);
    out["falloff"] = LLSD::Real(light.mFalloff);
    out["pos"] = vec3ToLLSD(light.mPos);
    out["yaw"] = LLSD::Real(light.mYawDeg);
    out["pitch"] = LLSD::Real(light.mPitchDeg);
    out["texture"] = light.mTexture;
    out["fov"] = LLSD::Real(light.mFov);
    out["focus"] = LLSD::Real(light.mFocus);
    out["ambiance"] = LLSD::Real(light.mAmbiance);
    out["vcam_capture"] = light.mVCamCapture;
    out["aim_object"] = light.mAimObject;
    LLSD track = LLSD::emptyMap();
    track["enabled"] = light.mTrack.mEnabled;
    track["has_subject"] = light.mTrack.mHasSubject;
    track["subject"] = light.mTrack.mSubject;
    track["joint"] = light.mTrack.mJoint;
    track["mode"] = light.mTrack.mMode;
    track["smoothing"] = LLSD::Real(light.mTrack.mSmoothing);
    out["track"] = track;
    out["practicals"] = light.mPracticals.isMap() ? light.mPracticals : LLSD::emptyMap();
    return out;
}

// Tolerant: a missing field keeps its default, the result is always sanitised. A non-map is
// rejected (false). The key is NOT generated here, the caller decides (scene load mints a new one
// when the file has none or a duplicate).
inline bool fromLLSD(const LLSD& data, Light& out)
{
    if (!data.isMap()) return false;
    Light light;
    light.mKey = data["key"].asUUID();
    if (data.has("on")) light.mOn = data["on"].asBoolean();
    if (data.has("spot")) light.mSpot = data["spot"].asBoolean();
    vec3FromLLSD(data["color"], light.mColor);
    if (data.has("intensity")) light.mIntensity = static_cast<F32>(data["intensity"].asReal());
    if (data.has("radius")) light.mRadius = static_cast<F32>(data["radius"].asReal());
    if (data.has("falloff")) light.mFalloff = static_cast<F32>(data["falloff"].asReal());
    vec3FromLLSD(data["pos"], light.mPos);
    if (data.has("yaw")) light.mYawDeg = static_cast<F32>(data["yaw"].asReal());
    if (data.has("pitch")) light.mPitchDeg = static_cast<F32>(data["pitch"].asReal());
    light.mTexture = data["texture"].asUUID();
    if (data.has("fov")) light.mFov = static_cast<F32>(data["fov"].asReal());
    if (data.has("focus")) light.mFocus = static_cast<F32>(data["focus"].asReal());
    if (data.has("ambiance")) light.mAmbiance = static_cast<F32>(data["ambiance"].asReal());
    light.mVCamCapture = data["vcam_capture"].asUUID();
    light.mAimObject = data["aim_object"].asUUID();
    const LLSD& track = data["track"];
    if (track.isMap())
    {
        light.mTrack.mEnabled = track["enabled"].asBoolean();
        light.mTrack.mHasSubject = track["has_subject"].asBoolean();
        light.mTrack.mSubject = track["subject"].asUUID();
        light.mTrack.mJoint = track["joint"].asInteger();
        light.mTrack.mMode = track["mode"].asInteger();
        if (track.has("smoothing")) light.mTrack.mSmoothing = static_cast<F32>(track["smoothing"].asReal());
    }
    if (data["practicals"].isMap()) light.mPracticals = data["practicals"];
    sanitize(light);
    out = light;
    return true;
}

// A scene block is an array under "virtual_lights". Reading it never trusts the file: at most
// kMaxLights entries are looked at, non-maps are skipped, a null or duplicate key gets a fresh
// one from `mint`. Returns the sanitised, capped list. An old scene (no block) yields empty.
template <class Mint>
inline std::vector<Light> parseScene(const LLSD& block, Mint mint)
{
    std::vector<Light> lights;
    if (!block.isArray()) return lights;
    for (S32 i = 0; i < block.size() && static_cast<S32>(lights.size()) < kMaxLights; ++i)
    {
        Light light;
        if (!fromLLSD(block[i], light)) continue;
        if (light.mKey.isNull() || containsKey(lights, light.mKey)) light.mKey = mint();
        lights.push_back(std::move(light));
    }
    return lights;
}

inline LLSD sceneBlock(const std::vector<Light>& lights)
{
    LLSD out = LLSD::emptyArray();
    for (size_t i = 0; i < lights.size() && static_cast<S32>(i) < kMaxLights; ++i)
        out.append(toLLSD(lights[i]));
    return out;
}

// Practicals state keys. The runtime fills them from LLPipeline and replays them on a recreate.
constexpr char kPracticalNoShadow[] = "no_shadow";
constexpr char kPracticalVolumetric[] = "volumetric";
constexpr char kPracticalGobo[] = "gobo";
constexpr char kPracticalShaft[] = "shaft";

// Undo-delta helper: true when two lights differ in anything an edit can change.
inline bool sameLight(const Light& a, const Light& b)
{
    return toLLSD(a) == toLLSD(b);
}
}

#endif // AL_VIRTUAL_LIGHTS_MODEL_H
