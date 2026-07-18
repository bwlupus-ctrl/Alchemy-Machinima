/**
 * @file llpathcamera.h
 * @brief Per-node path camera: drive the render camera from the cameras a
 *        director authored on an actor's path, so a walk plays as ONE take.
 *
 * A path-camera SOURCE, in the same family as LLCinematicCamera and
 * LLFlycamRecorder playback: it owns the render camera through a branch in the
 * idle camera dispatch (llappviewer.cpp), guarded by isActive(), and writes the
 * camera in updateCamera() instead of gAgentCamera.
 *
 * OWNERSHIP / precedence. The camera-driving path is the path of the Subject-A
 * actor -- there is only ever one Subject A, so EXACTLY ONE path drives the
 * camera and two path sources can never fight. In the dispatch the path camera
 * sits BELOW recorder playback (a pre-baked lens is the more explicit take and
 * the two are alternatives) and ABOVE LLCinematicCamera / flycam orbit (an
 * authored take beats the always-available auto-patterns):
 *
 *   agent pilot > recorder playback > PATH CAMERA > cinematic camera >
 *   joystick flycam > agent camera
 *
 * RELEASE. isActive() is re-evaluated every frame and goes false -- handing the
 * render camera straight back to the agent camera the SAME frame -- the moment
 * the walk stops / finishes / is cut (LLActorMover::hasActivePathCamera), the
 * master toggle turns off, the path loses its camera nodes, or Subject A becomes
 * unresolvable (derez / region change). It never latches: this is the fix for
 * the teleport-away "can't take control of the camera" failure mode, built here
 * as the first-class release path rather than a symptom patch.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef LL_LLPATHCAMERA_H
#define LL_LLPATHCAMERA_H

#include "stdtypes.h"
#include "v3dmath.h"
#include "llquaternion.h"

class LLPathCamera
{
public:
    static LLPathCamera& instance();

    // idle camera dispatch hooks, same contract as the other camera sources:
    // isActive() gates ownership; updateCamera() writes the render camera
    // INSTEAD of gAgentCamera.updateCamera() when active.
    bool isActive() const;
    void updateCamera();

    // ---- static Preview (check a node's framing without walking) -------------
    // Latch a fixed camera pose (a node's stored camera, GLOBAL coords). While
    // previewing the path camera owns the render camera regardless of the master
    // toggle, so the director can inspect a shot before pressing ACTION; exiting
    // hands control back to the agent camera the same frame.
    void startPreview(const LLVector3d& pos_global, const LLQuaternion& rot, F32 fov);
    void stopPreview();
    bool isPreviewing() const { return mPreview; }

private:
    LLPathCamera() = default;

    bool         mPreview = false;
    LLVector3d   mPreviewPos;
    LLQuaternion mPreviewRot;
    F32          mPreviewFov = 0.f;
};

#endif // LL_LLPATHCAMERA_H
