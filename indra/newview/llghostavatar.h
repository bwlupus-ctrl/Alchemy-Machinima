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

    // Detach + destroy everything cloneAttachmentsFrom() created.
    void releaseClonedAttachments();

    // Deferred death, mirroring LLControlAvatar's discipline: never markDead()
    // from inside a graphics-pipeline traversal.
    void markForDeath();
    virtual void idleUpdate(LLAgent &agent, const F64 &time);

    virtual bool isImpostor() { return false; }
    virtual bool isBuddy() const { return false; }

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

    // MILESTONE 2 (/ghostdress): spawn ONE ghost in front of the agent with
    // appearance AND duplicated attachments. Separate from the palette gate
    // so the visual test stays cheap to iterate on.
    static bool spawnDressedGhost();

    // (/ghostverify) Face-level acceptance check for cloned attachments.
    // MUST be a separate, later pass: cloning only SCHEDULES geometry work, so
    // checking faces in the same frame sees zero faces and looks like failure.
    // Reports PASS / FAIL / PENDING / INCONCLUSIVE as distinct outcomes.
    static void verifyClonedAttachments();

    // Destroy every ghost spawned by the test harness.
    static void clearTestGhosts();

private:
    bool mMarkedForDeath;

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
