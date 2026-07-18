/**
 * @file llpathcamera.cpp
 * @brief Per-node path camera source -- see llpathcamera.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llpathcamera.h"

#include "llactormover.h"       // path data + eased/cut camera track eval
#include "llagent.h"            // gAgent global<->agent coordinate conversion
#include "lldirectorcast.h"     // Subject A = the camera-driving actor
#include "llviewercamera.h"     // the render camera we write
#include "llviewercontrol.h"    // gSavedSettings (master toggle)
#include "llvoavatar.h"
#include "m3math.h"             // LLMatrix3 (quaternion -> camera axes)

// ---------------------------------------------------------------------------
LLPathCamera& LLPathCamera::instance()
{
    static LLPathCamera sInstance;
    return sInstance;
}

// ---------------------------------------------------------------------------
// Ownership gate. Re-checked every frame; the moment it returns false the idle
// dispatch falls through to the next source and finally the agent camera, so a
// released camera is handed back the SAME frame -- never latched.
// ---------------------------------------------------------------------------
bool LLPathCamera::isActive() const
{
    // Preview owns the camera regardless of the master toggle so a director can
    // check framing before enabling the take.
    if (mPreview)
    {
        return true;
    }

    static LLCachedControl<bool> enabled(gSavedSettings, "PathCameraEnabled", false);
    if (!enabled)
    {
        return false;               // default OFF -> byte-identical no-op
    }

    // Subject A resolved every frame: an unset / derezzed / out-of-region
    // subject releases the camera immediately (the teleport-away fix).
    LLVOAvatar* subject = LLDirectorCast::instance().resolveSubjectA();
    if (!subject)
    {
        return false;
    }
    return LLActorMover::instance().hasActivePathCamera(subject->getID());
}

// ---------------------------------------------------------------------------
void LLPathCamera::updateCamera()
{
    LLVector3d   pos_global;
    LLQuaternion rot;
    F32          fov  = 0.f;
    bool         have = false;

    if (mPreview)
    {
        pos_global = mPreviewPos;
        rot        = mPreviewRot;
        fov        = mPreviewFov;
        have       = true;
    }
    else if (LLVOAvatar* subject = LLDirectorCast::instance().resolveSubjectA())
    {
        have = LLActorMover::instance().getPathCameraPose(subject->getID(),
                                                          pos_global, rot, fov);
    }

    if (!have)
    {
        // Defensive: never write a garbage pose. Ownership is released next
        // frame through isActive(); holding last frame's camera for one frame
        // is preferable to a pop.
        return;
    }

    LLViewerCamera* cam = LLViewerCamera::getInstance();
    const F32 out_fov = (fov > 0.01f) ? llclamp(fov, 0.1f, 2.9f)
                                      : cam->getDefaultFOV();
    const LLVector3 out_pos = gAgent.getPosAgentFromGlobal(pos_global);
    const LLMatrix3 axes(rot);

    cam->setView(out_fov);
    cam->setOrigin(out_pos);
    cam->mXAxis = LLVector3(axes.mMatrix[0]);
    cam->mYAxis = LLVector3(axes.mMatrix[1]);
    cam->mZAxis = LLVector3(axes.mMatrix[2]);
}

// ---------------------------------------------------------------------------
void LLPathCamera::startPreview(const LLVector3d& pos_global,
                                const LLQuaternion& rot, F32 fov)
{
    mPreviewPos = pos_global;
    mPreviewRot = rot;
    mPreviewFov = fov;
    mPreview    = true;
}

void LLPathCamera::stopPreview()
{
    mPreview = false;
}
