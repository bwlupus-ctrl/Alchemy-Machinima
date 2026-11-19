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
#include "llviewerjoystick.h"   // preview must yield to an active flycam
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
    // Preview owns the camera so a director can check framing before enabling
    // the take -- but it is a TRANSIENT check and must NEVER starve an
    // explicitly-enabled flycam (the user directly grabbing the camera, e.g.
    // Flycam Orbit). If the flycam is overriding, yield: otherwise this source
    // sits above the flycam in the idle dispatch and freezes it.
    if (mPreview)
    {
        return !LLViewerJoystick::getInstance()->getOverrideCamera();
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

    if (!have || !pos_global.isFinite() || !rot.isFinite() ||
        !std::isfinite(fov))
    {
        // Defensive: never write a garbage pose. Keep the last valid camera
        // frame rather than poisoning the render camera.
        return;
    }

    LLViewerCamera* cam = LLViewerCamera::getInstance();
    const F32 out_fov = (fov > 0.01f) ? llclamp(fov, 0.1f, 2.9f)
                                      : cam->getDefaultFOV();
    const LLVector3 out_pos = gAgent.getPosAgentFromGlobal(pos_global);
    const LLMatrix3 axes(rot);
    const LLVector3 out_x(axes.mMatrix[0]);
    const LLVector3 out_y(axes.mMatrix[1]);
    const LLVector3 out_z(axes.mMatrix[2]);
    if (!out_pos.isFinite() || !out_x.isFinite() || !out_y.isFinite() ||
        !out_z.isFinite())
    {
        // A finite global pose can still overflow during global-to-agent or
        // quaternion-to-basis conversion. Never pass that through to camera.
        return;
    }

    cam->setView(out_fov);
    cam->setOrigin(out_pos);
    cam->mXAxis = out_x;
    cam->mYAxis = out_y;
    cam->mZAxis = out_z;
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
