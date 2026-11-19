/**
 * @file allocalfogmanager.cpp
 * @brief Client-only atmospheric fog-volume bank and renderer bridge.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "allocalfogmanager.h"

#include "alworldoverlayviz.h"
#include "llglslshader.h"
#include "llmath.h"
#include "llshadermgr.h"
#include "llstring.h"
#include "llviewercontrol.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace
{
constexpr F32 FOG_SIZE_MIN = 0.05f;
constexpr F32 FOG_SIZE_MAX = 256.f;

F32 finite_clamp(F32 value, F32 fallback, F32 minimum, F32 maximum)
{
    return std::isfinite(value) ? llclamp(value, minimum, maximum) : fallback;
}

void append_ellipse(std::vector<LLVector3>& points,
                    const ALLocalFogManager::Volume& volume,
                    S32 first_axis, S32 second_axis)
{
    constexpr S32 SEGMENTS = 48;
    points.clear();
    points.reserve(SEGMENTS);
    for (S32 segment = 0; segment < SEGMENTS; ++segment)
    {
        const F32 angle = F_TWO_PI * (F32)segment / (F32)SEGMENTS;
        LLVector3 local;
        local.mV[first_axis] = cosf(angle) * volume.mSize.mV[first_axis];
        local.mV[second_axis] = sinf(angle) * volume.mSize.mV[second_axis];
        points.push_back(volume.mCenter + local * volume.mRotation);
    }
}
} // anonymous namespace

//static
ALLocalFogManager& ALLocalFogManager::instance()
{
    static ALLocalFogManager sInstance;
    return sInstance;
}

ALLocalFogManager::ALLocalFogManager()
:   mVolumes(loadBank())
{
}

//static
ALLocalFogManager::Bank ALLocalFogManager::defaultBank()
{
    return std::vector<Volume>(MAX_VOLUMES);
}

//static
void ALLocalFogManager::sanitizeVolume(Volume& volume)
{
    volume.mLabel = utf8str_symbol_truncate(volume.mLabel, 40);
    if (!volume.mCenter.isFinite())
    {
        volume.mCenter.clear();
    }
    for (S32 axis = 0; axis < 3; ++axis)
    {
        volume.mSize.mV[axis] = finite_clamp(
            volume.mSize.mV[axis], 2.f, FOG_SIZE_MIN, FOG_SIZE_MAX);
        volume.mColor.mV[axis] = finite_clamp(
            volume.mColor.mV[axis], 0.5f, 0.f, 4.f);
    }
    if (!volume.mRotation.isFinite())
    {
        volume.mRotation.loadIdentity();
    }
    volume.mRotation.normalize();
    volume.mDensity = finite_clamp(volume.mDensity, 0.1f, 0.f, 4.f);
    volume.mFeather = finite_clamp(volume.mFeather, 0.5f, 0.f, 1.f);
    volume.mHeightFalloff = finite_clamp(volume.mHeightFalloff, 0.f, 0.f, 16.f);
    volume.mNoiseScale = finite_clamp(volume.mNoiseScale, 0.5f, 0.f, 8.f);
    volume.mNoiseSpeed = finite_clamp(volume.mNoiseSpeed, 0.05f, -8.f, 8.f);
    volume.mShape = llclamp(volume.mShape, (S32)SHAPE_BOX, (S32)SHAPE_ELLIPSOID);
}

//static
ALLocalFogManager::Bank ALLocalFogManager::loadBank()
{
    std::vector<Volume> bank = defaultBank();
    const LLSD data = gSavedSettings.getLLSD("LocalFogBank");
    if (!data.isMap() || data["version"].asInteger() != BANK_VERSION ||
        !data["volumes"].isArray())
    {
        return bank;
    }

    const LLSD& volumes = data["volumes"];
    const S32 count = llmin((S32)volumes.size(), MAX_VOLUMES);
    for (S32 i = 0; i < count; ++i)
    {
        const LLSD& item = volumes[i];
        if (!item.isMap())
        {
            continue;
        }
        Volume& volume = bank[i];
        if (item.has("enabled")) volume.mEnabled = item["enabled"].asBoolean();
        if (item["label"].isString()) volume.mLabel = item["label"].asString();
        if (item["center"].isArray()) volume.mCenter.setValue(item["center"]);
        if (item["size"].isArray()) volume.mSize.setValue(item["size"]);
        if (item["rotation"].isArray()) volume.mRotation.setValue(item["rotation"]);
        if (item["color"].isArray()) volume.mColor.setValue(item["color"]);
        if (item.has("density")) volume.mDensity = (F32)item["density"].asReal();
        if (item.has("feather")) volume.mFeather = (F32)item["feather"].asReal();
        if (item.has("height_falloff")) volume.mHeightFalloff = (F32)item["height_falloff"].asReal();
        if (item.has("noise_scale")) volume.mNoiseScale = (F32)item["noise_scale"].asReal();
        if (item.has("noise_speed")) volume.mNoiseSpeed = (F32)item["noise_speed"].asReal();
        if (item.has("shape")) volume.mShape = item["shape"].asInteger();
        sanitizeVolume(volume);
    }
    return bank;
}

void ALLocalFogManager::saveBank()
{
    mVolumes.resize(MAX_VOLUMES);
    for (Volume& volume : mVolumes)
    {
        sanitizeVolume(volume);
    }

    LLSD data = LLSD::emptyMap();
    data["version"] = BANK_VERSION;
    data["volumes"] = LLSD::emptyArray();
    for (const Volume& volume : mVolumes)
    {
        LLSD item = LLSD::emptyMap();
        item["enabled"] = volume.mEnabled;
        item["label"] = volume.mLabel;
        item["center"] = volume.mCenter.getValue();
        item["size"] = volume.mSize.getValue();
        item["rotation"] = volume.mRotation.getValue();
        item["color"] = volume.mColor.getValue();
        item["density"] = (F64)volume.mDensity;
        item["feather"] = (F64)volume.mFeather;
        item["height_falloff"] = (F64)volume.mHeightFalloff;
        item["noise_scale"] = (F64)volume.mNoiseScale;
        item["noise_speed"] = (F64)volume.mNoiseSpeed;
        item["shape"] = volume.mShape;
        data["volumes"].append(item);
    }
    gSavedSettings.setLLSD("LocalFogBank", data);
    ++mRevision;
}

void ALLocalFogManager::tick(F64 presentation_time)
{
    if (std::isfinite(presentation_time))
    {
        mPresentationTime = std::fmod(presentation_time, 3600.0);
    }
}

void ALLocalFogManager::setSelected(S32 volume)
{
    mSelectedVolume = llclamp(volume, 0, MAX_VOLUMES - 1);
}

void ALLocalFogManager::uploadToFroxel(LLGLSLShader& shader) const
{
    F32 centers[MAX_VOLUMES * 3];
    F32 extents[MAX_VOLUMES * 3];
    F32 inv_rotations[MAX_VOLUMES * 9];
    F32 params[MAX_VOLUMES * 4];
    F32 tints[MAX_VOLUMES * 3];
    F32 noise[MAX_VOLUMES * 4];
    S32 count = 0;

    for (const Volume& volume : mVolumes)
    {
        if (!volume.mEnabled || count >= MAX_VOLUMES)
        {
            continue;
        }
        const S32 v3 = count * 3;
        const S32 v4 = count * 4;
        const S32 m3 = count * 9;
        for (S32 axis = 0; axis < 3; ++axis)
        {
            centers[v3 + axis] = volume.mCenter.mV[axis];
            extents[v3 + axis] = volume.mSize.mV[axis];
            tints[v3 + axis] = volume.mColor.mV[axis];
        }
        params[v4] = volume.mDensity;
        params[v4 + 1] = volume.mFeather;
        params[v4 + 2] = (F32)volume.mShape;
        params[v4 + 3] = volume.mHeightFalloff;
        noise[v4] = volume.mNoiseScale;
        noise[v4 + 1] = volume.mNoiseSpeed;
        noise[v4 + 2] = 0.f;
        noise[v4 + 3] = 0.f;

        const LLQuaternion& q = volume.mRotation;
        const glm::quat rotation(q.mQ[VW], q.mQ[VX], q.mQ[VY], q.mQ[VZ]);
        const glm::mat3 inv_rotation = glm::mat3_cast(glm::conjugate(rotation));
        memcpy(inv_rotations + m3, glm::value_ptr(inv_rotation), sizeof(F32) * 9);
        ++count;
    }

    shader.uniform1i(LLShaderMgr::LOCALFOG_COUNT, count);
    if (count == 0)
    {
        return;
    }
    shader.uniform3fv(LLShaderMgr::LOCALFOG_CENTER, count, centers);
    shader.uniform3fv(LLShaderMgr::LOCALFOG_EXTENTS, count, extents);
    shader.uniformMatrix3fv(LLShaderMgr::LOCALFOG_INV_ROT, count, GL_FALSE, inv_rotations);
    shader.uniform4fv(LLShaderMgr::LOCALFOG_PARAMS, count, params);
    shader.uniform3fv(LLShaderMgr::LOCALFOG_TINT, count, tints);
    shader.uniform4fv(LLShaderMgr::LOCALFOG_NOISE, count, noise);
}

void ALLocalFogManager::renderOverlay() const
{
    if (!gSavedSettings.getBOOL("BDMergeLocalFogVolumes"))
    {
        return;
    }

    if (std::none_of(mVolumes.begin(), mVolumes.end(),
                     [](const Volume& volume) { return volume.mEnabled; }))
    {
        return;
    }
    ALWorldOverlayViz::ScopedRenderer renderer;
    if (!renderer.isReady())
    {
        return;
    }

    static const S32 BOX_EDGES[12][2] = {
        {0,1}, {0,2}, {0,4}, {1,3}, {1,5}, {2,3},
        {2,6}, {3,7}, {4,5}, {4,6}, {5,7}, {6,7}
    };
    std::vector<LLVector3> ellipse;
    for (S32 index = 0; index < (S32)mVolumes.size(); ++index)
    {
        const Volume& volume = mVolumes[index];
        if (!volume.mEnabled)
        {
            continue;
        }
        const bool selected = index == mSelectedVolume;
        LLColor4 color(volume.mColor, selected ? 1.f : 0.72f);
        if (selected)
        {
            color.mV[VRED] = llmin(color.mV[VRED] * 1.35f + 0.15f, 1.f);
            color.mV[VGREEN] = llmin(color.mV[VGREEN] * 1.35f + 0.15f, 1.f);
            color.mV[VBLUE] = llmin(color.mV[VBLUE] * 1.35f + 0.15f, 1.f);
        }
        const ALWorldOverlayViz::StrokeStyle style(
            color, selected ? 3.5f : 2.25f, LLColor4(0.f, 0.f, 0.f, 0.78f), 1.25f);

        if (volume.mShape == SHAPE_BOX)
        {
            LLVector3 corners[8];
            for (S32 corner = 0; corner < 8; ++corner)
            {
                LLVector3 local(
                    (corner & 1) ? volume.mSize.mV[VX] : -volume.mSize.mV[VX],
                    (corner & 2) ? volume.mSize.mV[VY] : -volume.mSize.mV[VY],
                    (corner & 4) ? volume.mSize.mV[VZ] : -volume.mSize.mV[VZ]);
                corners[corner] = volume.mCenter + local * volume.mRotation;
            }
            for (const auto& edge : BOX_EDGES)
            {
                renderer.drawSegment(corners[edge[0]], corners[edge[1]], style);
            }
        }
        else
        {
            const bool sphere = fabsf(volume.mSize.mV[VX] - volume.mSize.mV[VY]) < 1e-4f &&
                                fabsf(volume.mSize.mV[VX] - volume.mSize.mV[VZ]) < 1e-4f;
            if (sphere)
            {
                renderer.drawRing(volume.mCenter,
                                  LLVector3::x_axis * volume.mRotation,
                                  volume.mSize.mV[VX], style, 48);
                renderer.drawRing(volume.mCenter,
                                  LLVector3::y_axis * volume.mRotation,
                                  volume.mSize.mV[VY], style, 48);
                renderer.drawRing(volume.mCenter,
                                  LLVector3::z_axis * volume.mRotation,
                                  volume.mSize.mV[VZ], style, 48);
            }
            else
            {
                // Three transformed orthogonal elliptical rings form the
                // general non-uniform ellipsoid cage.
                append_ellipse(ellipse, volume, VX, VY);
                renderer.drawPolyline(ellipse, style, true);
                append_ellipse(ellipse, volume, VX, VZ);
                renderer.drawPolyline(ellipse, style, true);
                append_ellipse(ellipse, volume, VY, VZ);
                renderer.drawPolyline(ellipse, style, true);
            }
        }
    }
}
