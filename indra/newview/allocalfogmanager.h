/**
 * @file allocalfogmanager.h
 * @brief Client-only atmospheric fog-volume bank and renderer bridge.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALLOCALFOGMANAGER_H
#define AL_ALLOCALFOGMANAGER_H

#include "llquaternion.h"
#include "stdtypes.h"
#include "v3color.h"
#include "v3math.h"

#include <string>
#include <vector>

class LLGLSLShader;

class ALLocalFogManager
{
public:
    static constexpr S32 MAX_VOLUMES = 8;
    static constexpr S32 BANK_VERSION = 1;

    enum EShape : S32
    {
        SHAPE_BOX = 0,
        SHAPE_ELLIPSOID = 1,
    };

    struct Volume
    {
        bool         mEnabled = false;
        std::string  mLabel;
        LLVector3    mCenter;
        LLVector3    mSize{ 2.f, 2.f, 2.f }; // half-extents
        LLQuaternion mRotation;
        LLColor3     mColor{ 0.5f, 0.5f, 0.5f };
        F32          mDensity = 0.1f;
        F32          mFeather = 0.5f;
        F32          mHeightFalloff = 0.f;
        F32          mNoiseScale = 0.5f;
        F32          mNoiseSpeed = 0.05f;
        S32          mShape = SHAPE_BOX;
    };

    using Bank = std::vector<Volume>;

    static ALLocalFogManager& instance();

    const Bank& volumes() const { return mVolumes; }
    Bank& volumes() { return mVolumes; }
    U32 revision() const { return mRevision; }
    void saveBank();

    void tick(F64 presentation_time);
    S32 selectedVolume() const { return mSelectedVolume; }
    void setSelected(S32 volume);

    void uploadToFroxel(LLGLSLShader& media_shader) const;
    void renderOverlay() const;

private:
    ALLocalFogManager();

    static Bank defaultBank();
    static Bank loadBank();
    static void sanitizeVolume(Volume& volume);

    Bank mVolumes;
    S32 mSelectedVolume = 0;
    U32 mRevision = 0;
    F64 mPresentationTime = 0.0;
};

#endif // AL_ALLOCALFOGMANAGER_H
