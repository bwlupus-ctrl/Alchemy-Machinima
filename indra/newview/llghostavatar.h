/**
 * @file llghostavatar.h
 * @brief Client-only avatar used as a scene-lit Ghost Studio clone
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLGHOSTAVATAR_H
#define LL_LLGHOSTAVATAR_H

#include "llvoavatar.h"

class LLVOVolume;
class ALGhostStudio;

// [GhostStudio] A client-only avatar that renders through the REAL scene path.
//
// Both existing client-only avatars (LLControlAvatar, LLUIAvatar) set
// mIsDummy = true, which buys them out of the full avatar pipeline. A ghost
// must NOT do that: taking the real path is the entire point, because that is
// what earns deferred lighting, cast/received shadows, fog, tonemap and
// ReShade visibility. See doc/ notes and the scene-lit clone design.
//
// Consequences of mIsDummy == false that are handled elsewhere (and that are
// the reason this class exists as its own type rather than a flag on
// LLControlAvatar): world enumeration, name cache, mute/render policy,
// autotune accounting, footstep/typing sounds, and server avatar-texture
// requests must all be suppressed for a synthetic id. isGhostAvatar() is the
// hook those sites test.
class LLGhostAvatar : public LLVOAvatar
{
    LL_ALIGN_NEW;
    LOG_CLASS(LLGhostAvatar);

public:
    LLGhostAvatar(const LLUUID& id, const LLPCode pcode, LLViewerRegion* regionp);
    virtual ~LLGhostAvatar();

    virtual void initInstance();

    // NOTE: isGhostAvatar() is not overridden here -- the base returns
    // mIsGhostAvatar, which the constructor sets.

    // Ghosts are placed by fiat, never by the simulator. Requires a live
    // region: setPositionAgent() dereferences getRegion() unguarded.
    void setGhostPosition(const LLVector3& pos_agent);
    void setGhostRotation(const LLQuaternion& rotation);

    // LLVOAvatar::slamPosition() MINUS its opening gAgent.setPositionAgent()
    // (llvoavatar.cpp:4038), which would teleport the real agent onto the ghost.
    // Never call the base slamPosition() on a ghost.
    void ghostSlamPosition(const LLVector3& pos_agent);

    // Copy shape + baked textures from a source avatar. Thin wrapper over
    // LLVOAvatar::copyAppearanceFrom, kept here so callers read intent.
    // MANDATORY, not cosmetic: without a first appearance message the avatar
    // renders invisible (llvoavatar.cpp:5340).
    //
    // NOTE: this copies the appearance MESSAGE only -- shape params and baked
    // textures. It carries NO attachment data, so on its own it produces a
    // naked system body. Worn mesh comes from cloneAttachmentsFrom().
    bool cloneAppearanceFrom(LLVOAvatar* source);

    // Duplicate the source's worn attachments onto this ghost as client-only
    // objects. Call AFTER cloneAppearanceFrom (attachment joint overrides must
    // land on top of a settled appearance -- LLPolySkeletalDistortion::apply
    // is additive on joint scale, so re-slamming appearance afterwards would
    // corrupt proportions). Returns the number of attachment roots duplicated.
    S32 cloneAttachmentsFrom(LLVOAvatar* source);
    bool clonedAttachmentsComplete() const
    {
        return mCloneFailures == 0
            && mClonedLinksets.size() == static_cast<size_t>(mCloneExpectedRoots);
    }

    // Detach + destroy everything cloneAttachmentsFrom() created.
    void releaseClonedAttachments();

    // Deferred death, mirroring LLControlAvatar's discipline: never markDead()
    // from inside a graphics-pipeline traversal.
    void markForDeath();
    void markDead() override;
    virtual void idleUpdate(LLAgent &agent, const F64 &time);

    void setEntityCloneVisible(bool visible) { mEntityCloneVisible = visible; }
    bool isEntityCloneVisible() const { return mEntityCloneVisible; }

    // Viewer-local entity controls. These only alter this synthetic avatar's
    // skeleton/motion controller; neither path touches simulator object state.
    void setEntityScale(F32 scale);
    F32 getUniformScale() const override { return mEntityScale; }
    void setEntityPhysicsEnabled(bool enabled);
    void setEntityDriveMode(S32 mode, const LLUUID& directed_anim);
    void setEntityLoopMode(S32 mode);
    void restartEntityAnimation();
    void setEntityLook(S32 look, F32 alpha);

    virtual bool isImpostor() { return false; }
    virtual bool isBuddy() const { return false; }

    // True if `id` resolves to a live client-only ghost clone. Convenience for the
    // many "don't treat this UUID as a resident" guards across the viewer (pay,
    // friendship, tracking, etc.) -- resolves via gObjectList so callers need only
    // this header.
    static bool isGhostId(const LLUUID& id);

    // ---------------------------------------------------------------------
    // MILESTONE 1 ACCEPTANCE GATE (/ghosttest)
    //
    // The load-bearing question for the whole scene-lit clone architecture:
    // can two avatars pose the SAME shared LLMeshSkinInfo differently in the
    // same frame? mMatrixPaletteCache is a per-instance member keyed by skin
    // hash (llvoavatar.h), so in principle yes -- this proves it in practice.
    //
    // Spawns two ghosts from one source, poses their skeletons differently,
    // and compares their palettes for one shared skin. Logs PASS/FAIL under
    // the "GhostStudio" log tag. Returns false if the test could not be run
    // (no source, no rigged attachment, spawn failure) as distinct from a
    // genuine FAIL.
    static bool runPaletteIsolationTest();

    // (/ghostverify) Face-level acceptance check for cloned attachments.
    // MUST be a separate, later pass: cloning only SCHEDULES geometry work, so
    // checking faces in the same frame sees zero faces and looks like failure.
    // Reports PASS / FAIL / PENDING / INCONCLUSIVE as distinct outcomes.
    static void verifyClonedAttachments(bool include_test_harness = false);

    // If volume is one of this ghost's client-only attachment prims, return
    // the live simulator-known source prim's current rendered LOD.
    static bool getClonedSourceLOD(const LLVOVolume* volume, S32& source_lod);

    // Destroy every ghost spawned by the palette-isolation test harness.
    static S32 clearTestHarnessGhosts();

private:
    void updateEntityOuterTransform();
    void stampEntityOuterTransform(LLViewerObject* object);
    void clearClonedObjectAnimations();
    void synchronizeCloneAnimations(
        const std::map<LLUUID, S32>& desired_animations);

    bool mMarkedForDeath;
    bool mEntityCloneVisible;
    F32 mEntityScale = 1.f;
    bool mEntityPhysicsEnabled = true;
    S32 mEntityDriveMode = 0; // ALGhostStudio::DRIVE_MIRROR (avoid header cycle)
    S32 mEntityLoopMode = 0;  // ALGhostStudio::LOOP_RETRIGGER
    S32 mEntityLook = 0;
    F32 mEntityLookAlpha = 1.f;
    LLUUID mEntityDirectedAnim;
    bool mEntityDirectedStarted = false;
    bool mEntityDirectedWasActive = false;
    F32 mEntityDirectedStartTime = 0.f;
    // Holding the pause handle keeps LLCharacter::updateMotions() from
    // automatically unpausing on the next visible frame.
    LLAnimPauseRequest mEntityPauseRequest;
    std::vector<LLAnimPauseRequest> mEntityControlPauseRequests;
    // Live avatar whose simulator-driven animation state this client-only
    // entity mirrors. The UUID is resolved through gObjectList each frame so
    // the ghost never owns or extends the source avatar's lifetime.
    LLUUID mAnimationSourceId;
    // Positive-isolation ledger for avatar animations.  Neither map is the
    // inherited simulator animation state; synchronization can only call the
    // LLCharacter motion-controller API on this clone.
    std::map<LLUUID, S32> mCloneDesiredAnimations;
    std::map<LLUUID, S32> mClonePlayingAnimations;

    // The client-only linksets we attached.
    //
    // Records EVERY cloned prim, not just roots. Tracking roots alone let the
    // verifier reconstruct a linkset from whatever children happened to be
    // alive at verification time -- so a cloned child that later died or
    // detached vanished without moving any counter, and the survivors could
    // still report PASS on a degraded clone.
    struct ClonedLinkset
    {
        LLUUID              mRoot;
        std::vector<LLUUID> mChildren;
        // Simulator-known source ids corresponding one-for-one to the clone
        // ids above. Used only to mirror ObjectAnimation state; UUIDs avoid
        // extending the source objects' lifetimes.
        LLUUID              mSourceRoot;
        std::vector<LLUUID> mSourceChildren;
    };
    std::vector<ClonedLinkset> mClonedLinksets;

    // Re-run the structural checks at VERIFY time. A member (not a free
    // helper) because it takes the private ClonedLinkset type.
    static bool recheckStructure(LLGhostAvatar* ghost,
                                 LLViewerObject* root,
                                 const ClonedLinkset& linkset);

    // Transactional clone state. Without this, /ghostverify can only inspect
    // what was RECORDED and has no idea what SHOULD have been: a discarded
    // linkset simply vanishes, the surviving ones verify fine, and it prints
    // PASS on an outfit that is missing a piece. PASS must mean the whole
    // clone succeeded, so any structural failure is remembered here and
    // suppresses PASS outright.
    S32 mCloneExpectedRoots = 0;    // attachment roots we attempted
    S32 mCloneFailures      = 0;    // alloc/copy/attach/topology failures
};

#endif // LL_LLGHOSTAVATAR_H
