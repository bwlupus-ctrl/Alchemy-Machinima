/**
 * @file alvcamobjectpovsettings.h
 * @brief Renderer-independent Object POV scene schema.
 */
#ifndef AL_VCAM_OBJECT_POV_SETTINGS_H
#define AL_VCAM_OBJECT_POV_SETTINGS_H

#include "llprismlens.h"
#include "llsd.h"

#include <cmath>

namespace ALVCamObjectPovSettings
{
inline bool valid(const LLPrismLens::ObjectPovSettings& settings)
{
    return settings.mOffset.isFinite() &&
        std::isfinite(settings.mTrimPitchDeg) &&
        settings.mTrimPitchDeg >= -180.f && settings.mTrimPitchDeg <= 180.f &&
        std::isfinite(settings.mTrimYawDeg) &&
        settings.mTrimYawDeg >= -180.f && settings.mTrimYawDeg <= 180.f &&
        std::isfinite(settings.mSmoothingSec) &&
        settings.mSmoothingSec >= 0.f && settings.mSmoothingSec <= 10.f;
}

inline LLSD serialize(const LLPrismLens::ObjectPovSettings& settings)
{
    LLSD result = LLSD::emptyMap();
    result["enabled"] = settings.mEnabled;
    result["object_id"] = settings.mObjectId;
    result["follow_yaw"] = settings.mFollowYaw;
    result["follow_pitch"] = settings.mFollowPitch;
    result["follow_roll"] = settings.mFollowRoll;
    result["offset"] = LLSD::emptyArray();
    for (S32 axis = 0; axis < 3; ++axis)
    {
        result["offset"].append(settings.mOffset.mV[axis]);
    }
    result["trim_pitch_degrees"] = settings.mTrimPitchDeg;
    result["trim_yaw_degrees"] = settings.mTrimYawDeg;
    result["smoothing_seconds"] = settings.mSmoothingSec;
    return result;
}

// Undefined means a legacy scene. Parse transactionally so invalid input cannot
// partially overwrite the caller's settings or turn an old scene attachment on.
inline bool deserialize(const LLSD& value, LLPrismLens::ObjectPovSettings& output)
{
    LLPrismLens::ObjectPovSettings settings;
    if (value.isUndefined())
    {
        output = settings;
        return true;
    }
    if (!value.isMap())
    {
        return false;
    }
    for (const char* key : {"enabled", "follow_yaw", "follow_pitch", "follow_roll"})
    {
        if (value.has(key) && !value[key].isBoolean())
        {
            return false;
        }
    }
    if (value.has("object_id"))
    {
        if (!value["object_id"].isUUID() &&
            (!value["object_id"].isString() ||
             !LLUUID::validate(value["object_id"].asString())))
        {
            return false;
        }
        settings.mObjectId = value["object_id"].asUUID();
    }
    if (value.has("enabled")) settings.mEnabled = value["enabled"].asBoolean();
    if (value.has("follow_yaw")) settings.mFollowYaw = value["follow_yaw"].asBoolean();
    if (value.has("follow_pitch")) settings.mFollowPitch = value["follow_pitch"].asBoolean();
    if (value.has("follow_roll")) settings.mFollowRoll = value["follow_roll"].asBoolean();
    if (value.has("offset"))
    {
        const LLSD& offset = value["offset"];
        if (!offset.isArray() || offset.size() != 3)
        {
            return false;
        }
        for (S32 axis = 0; axis < 3; ++axis)
        {
            if (!offset[axis].isReal() && !offset[axis].isInteger())
            {
                return false;
            }
            settings.mOffset.mV[axis] = static_cast<F32>(offset[axis].asReal());
        }
    }
    for (const char* key : {"trim_pitch_degrees", "trim_yaw_degrees", "smoothing_seconds"})
    {
        if (value.has(key) && !value[key].isReal() && !value[key].isInteger())
        {
            return false;
        }
    }
    if (value.has("trim_pitch_degrees"))
        settings.mTrimPitchDeg = static_cast<F32>(value["trim_pitch_degrees"].asReal());
    if (value.has("trim_yaw_degrees"))
        settings.mTrimYawDeg = static_cast<F32>(value["trim_yaw_degrees"].asReal());
    if (value.has("smoothing_seconds"))
        settings.mSmoothingSec = static_cast<F32>(value["smoothing_seconds"].asReal());
    if (!valid(settings))
    {
        return false;
    }
    output = settings;
    return true;
}
}

#endif
