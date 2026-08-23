/**
 * @file alposepolish.h
 * @brief [Machinima] Post-blend "Pose Polish" stage owner.
 *
 * A per-avatar filter that runs AFTER SL's normal animation blend
 * (LLPoseBlender::blendAndApply, via updateMotions) and BEFORE the procedural
 * gaze layer, in LLVOAvatar::updateCharacter (see llvoavatar.cpp, the call
 * sited between updateMotions() and applyDirectorLookAt()). It repairs
 * discontinuities, preserves foot contact, and otherwise reproduces the
 * authored motion EXACTLY. It is NOT a replacement animation engine.
 *
 * Hard contract (see docs/pose_polish_integration_plan.md):
 *  - Master gate ALPolishEnabled defaults false -> run() is an immediate no-op
 *    and the displayed pose is bit-identical to today.
 *  - Never writes simulator position, mRoot world pos/rot, animation IDs,
 *    priorities, easing, or any network/message state. Render-only.
 *  - Never re-arbitrates SL joint priority; it filters the already-blended pose.
 *  - Resets all continuity/contact state on teleport, region cross, skeleton
 *    rebuild, animation pause/scrub/time-reversal, and abnormally large dt.
 *
 * The per-milestone MATH lives in its own header-only modules and operates on
 * plain joint-transform arrays (no LLVOAvatar dependency), so each stage is
 * independently testable and this owner is the only place that touches the
 * avatar. Modules are pulled in as milestones land:
 *   M1 transition inertialization  -> alposecontinuity.h
 *   M2 contact stabilization       -> alcontactstab.h
 *   M3 phase-aware locomotion      -> (llcharacter-side; wraps walk cadence)
 *   M4 loop-seam repair            -> (load-time; reuses M1 decay)
 *   M5 NLA-like layers/diagnostics -> alposelayer.h
 *   M6 procedural secondary motion -> alposesecondary.h  (incl. Gravitate lean)
 */
#ifndef LL_ALPOSEPOLISH_H
#define LL_ALPOSEPOLISH_H

#include "stdtypes.h"

#include "alposecontinuity.h"   // M1 inertializer (pure, header-only)
#include "alcontactstab.h"      // M2 contact inference (pure, header-only)
#include "lljointsolverrp3.h"   // M2 leg IK: proven two-bone solver (pulls in lljoint.h)
#include "v3math.h"

#include <vector>

class LLVOAvatar;

// [Machinima] Per-avatar Pose Polish owner. Session-only, allocated once with
// the avatar; carries the continuity/contact shadow state. All milestone state
// is aggregated here (added as milestones land) so nothing allocates per frame.
class ALPosePolish
{
public:
    // [M2] Mirror llkeyframestandmotion.cpp's constructor: build the internal
    // kinematic hierarchy once (pelvis -> hip -> knee -> ankle, per leg) so the
    // solver's world-matrix math sees a real parent chain.
    ALPosePolish()
    {
        mPelvisJoint.addChild(&mHipLeftJoint);
        mHipLeftJoint.addChild(&mKneeLeftJoint);
        mKneeLeftJoint.addChild(&mAnkleLeftJoint);
        mPelvisJoint.addChild(&mHipRightJoint);
        mHipRightJoint.addChild(&mKneeRightJoint);
        mKneeRightJoint.addChild(&mAnkleRightJoint);
    }

    // The internal IK hierarchy captures member addresses via addChild(), so a
    // copy/move would leave the copy's joints/solvers pointing into the source.
    // The instance is a stable direct member of LLVOAvatar and is never copied;
    // enforce that invariant so it can never regress.
    ALPosePolish(const ALPosePolish&) = delete;
    ALPosePolish& operator=(const ALPosePolish&) = delete;
    ALPosePolish(ALPosePolish&&) = delete;
    ALPosePolish& operator=(ALPosePolish&&) = delete;

    // Run the polish stage for @av this frame. @dt is the real frame interval
    // (gFrameIntervalSeconds) used by the inertializer's decay; presentation
    // pause/scrub is handled via reset(). No-op when ALPolishEnabled is false
    // or when there is nothing to repair. Defined in llvoavatar.cpp (needs the
    // full LLVOAvatar definition), which is the single avatar-touching site.
    void run(LLVOAvatar* av, F32 dt);

    // Drop all continuity/contact shadow state so the next frame seeds fresh.
    // Called on teleport, region cross, skeleton rebuild, and presentation
    // pause/scrub/time-reversal. A reset frame produces the raw blended pose.
    void reset();

    // True once any milestone shadow state has been seeded this session.
    bool isActive() const { return mSeeded; }

private:
    bool mSeeded = false;   // any milestone state seeded yet
    U32  mLastFrame = 0xFFFFFFFF;   // per-frame advance guard (mirror gaze)

    // M1: one inertializer shadow state per polished joint (parallel to the
    // static joint table in llvoavatar.cpp's runInertialization). Allocated
    // once on first use; reset() invalidates without freeing.
    std::vector<ALPoseContinuity::InertiaJoint> mInertiaJoints;

    // M1 sub-stage: inertialize the freshly blended local rotations of the
    // major body joints so AO/gesture swaps do not pop. Defined in
    // llvoavatar.cpp (needs LLVOAvatar). No-op unless ALPolishInertiaEnabled.
    void runInertialization(LLVOAvatar* av, F32 dt);

    // ---- M2 contact stabilizer state --------------------------------------
    // Internal LLJoint copies of the leg chains, mirroring
    // llkeyframestandmotion.h (mHip*/mKnee*/mAnkle* + mTarget* per leg under a
    // shared pelvis root). Each frame the REAL joint positions/scales/rotations
    // are propagated in, LLJointSolverRP3 solves against the contact lock, and
    // only the solved LOCAL rotations are blended back onto the real joints.
    LLJoint             mPelvisJoint;

    LLJoint             mHipLeftJoint;
    LLJoint             mKneeLeftJoint;
    LLJoint             mAnkleLeftJoint;
    LLJoint             mTargetLeft;

    LLJoint             mHipRightJoint;
    LLJoint             mKneeRightJoint;
    LLJoint             mAnkleRightJoint;
    LLJoint             mTargetRight;

    LLJointSolverRP3    mIKLeft;
    LLJointSolverRP3    mIKRight;

    bool                mContactInit = false;   // one-time pole/BAxis/setupJoints done
    ALContactStab::ContactFoot mFoot[2];        // 0 = left, 1 = right
    F32                 mContactBlend[2] = { 0.f, 0.f };  // per-leg IK ramp 0..1
    LLVector3           mContactHold[2];        // last world hold pos (ramp-out target)

    // M2 sub-stage: hold planted feet in world space via the leg IK. Defined in
    // llvoavatar.cpp (needs LLVOAvatar). No-op unless ALPolishContactEnabled;
    // grounded biped locomotion only (sit/fly/in-air skip + release).
    void runContact(LLVOAvatar* av, F32 dt);

    // Milestone shadow-state members (M6 secondary, ...) are added here as
    // each lands, so per-avatar state is allocated exactly once.
};

#endif // LL_ALPOSEPOLISH_H
