/**
 * @file llghostavatar.cpp
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

#include "llviewerprecompiledheaders.h"
#include "llghostavatar.h"

#include "llagent.h"
#include "llviewerobjectlist.h"
#include "llviewerjointattachment.h"
#include "llvoavatarself.h"
#include "llvovolume.h"
#include "llmeshrepository.h"
#include "llskinningutil.h"
#include "llviewerregion.h"
#include "llvolumemgr.h"
#include "llgltfmaterial.h"
#include "llspatialpartition.h"
#include "llface.h"
#include "lldrawable.h"
#include "indra_constants.h"
#include "pipeline.h"

// [GhostStudio] Ghosts spawned by the /ghosttest harness, so it can clean up
// after itself. Not the production Ghost Studio store.
//
// Deliberately UUIDs, not LLPointers: a file-static strong reference can
// outlive gObjectList's logout/shutdown cleanup and drag an LLVOAvatar's
// destruction into static-destruction time, after pipeline/texture globals
// are gone. Storing ids leaves lifetime entirely to gObjectList.
static std::vector<LLUUID> sTestGhostIds;

LLGhostAvatar::LLGhostAvatar(const LLUUID& id, const LLPCode pcode, LLViewerRegion* regionp) :
    LLVOAvatar(id, pcode, regionp),
    mMarkedForDeath(false)
{
    // A ghost renders through the REAL scene avatar path -- that is the whole
    // point (deferred lighting, shadows, fog, tonemap, ReShade). Unlike
    // LLControlAvatar / LLUIAvatar it must NOT be a dummy. mIsDummy defaults to
    // false (LLAvatarAppearance), but keep it explicit so the intent is local.
    mIsDummy = false;
    mIsGhostAvatar = true;

    // The default motion controller would overwrite any pose we place on this
    // skeleton, and a clone's "animation" is driven externally. Same reasoning
    // as LLControlAvatar (llcontrolavatar.cpp:57).
    mEnableDefaultMotions = false;

    // Undo the base ctor's saved render-policy lookup: a ghost's id is
    // synthetic, so any saved-visual-mute hit would be a STRANGER's setting.
    // Use the LOCAL (non-persisting) setter -- setVisualMuteSettings() would
    // write the synthetic id back into LLRenderMuteList. (Control/UI avatars
    // keep the base lookup, exactly as they always did.)
    setVisualMuteSettingsLocal(LLVOAvatar::AV_RENDER_NORMALLY);
}

// virtual
LLGhostAvatar::~LLGhostAvatar()
{
}

// virtual
void LLGhostAvatar::initInstance()
{
    LLVOAvatar::initInstance();

    // Mirrors LLUIAvatar::initInstance(): a client-only avatar is never sent an
    // ObjectUpdate, so it must build its own drawable and be placed by fiat.
    createDrawable(&gPipeline);

    // DELIBERATELY NOT LLUIAvatar's setPositionAgent(zero) + slamPosition().
    // LLVOAvatar::slamPosition() begins with gAgent.setPositionAgent()
    // (llvoavatar.cpp:4038) -- it is the SELF avatar's teleport helper. Calling
    // it here would drag the REAL agent to the ghost on every spawn and every
    // move. ghostSlamPosition() is that function minus its first line.
    //
    // Also note setPositionAgent() dereferences getRegion() unguarded
    // (llviewerobject.cpp), so placement only happens with a live region.
    if (getRegion())
    {
        ghostSlamPosition(getPositionAgent());
    }

    updateJointLODs();
    updateGeometry(mDrawable);

    mInitFlags |= 1 << 3;
}

void LLGhostAvatar::ghostSlamPosition(const LLVector3& pos_agent)
{
    // Set the REAL object position, not just the render root. LLActorMover's
    // applyOverride() paints mRoot only (llactormover.cpp:1894) and explicitly
    // leaves the drawable position alone, which is tolerable for a puppeteered
    // real avatar but not here: a ghost's culling, pixel area, LOD, impostor
    // selection and picking all evaluate off the drawable. If those disagree
    // with where it is drawn, it flickers out at the wrong distances.
    setPositionAgent(pos_agent);

    // The remainder is LLVOAvatar::slamPosition() (llvoavatar.cpp:4036) with
    // the gAgent teleport omitted.
    mRoot->setWorldPosition(pos_agent);
    setChanged(TRANSLATED);
    if (mDrawable.notNull())
    {
        gPipeline.updateMoveNormalAsync(mDrawable);
    }
    mRoot->updateWorldMatrixChildren();
}

void LLGhostAvatar::setGhostPosition(const LLVector3& pos_agent)
{
    if (!getRegion())
    {
        LL_WARNS("Avatar") << "setGhostPosition with no region; ignoring" << LL_ENDL;
        return;
    }
    ghostSlamPosition(pos_agent);
}

//static
bool LLGhostAvatar::isGhostId(const LLUUID& id)
{
    if (id.isNull())
    {
        return false;
    }
    LLViewerObject* obj = gObjectList.findObject(id);
    return obj && obj->asAvatar() && obj->asAvatar()->isGhostAvatar();
}

bool LLGhostAvatar::cloneAppearanceFrom(LLVOAvatar* source)
{
    return copyAppearanceFrom(source, true);
}

// ---------------------------------------------------------------------------
// Attachment duplication
//
// The appearance message carries shape + baked textures and nothing else, so a
// clone built from it alone is a naked system body. A modern avatar IS its worn
// mesh, so we duplicate the source's attachment objects as CLIENT-ONLY copies.
//
// Adapted from LLLocalMeshMgr's client-only linkset recipe (lllocalmesh.cpp
// spawnLinkset/attachPreviewToAvatar), generalised from gAgentAvatarp to an
// arbitrary target avatar.
//
// ⚠️ SAFETY, DO NOT REORDER: `mIsLocalOnly` must be set the instant the object
// exists, BEFORE any parenting. LLViewerJointAttachment::addObject() takes a
// branch for NON-local objects that hunts for an existing attachment with the
// same inventory item id, KILLS it, and sends ObjectDetach TO THE SIMULATOR
// (llviewerjointattachment.cpp:183-199). Getting this wrong would strip the
// user's real worn items off them. Local-only objects skip that branch.
// ---------------------------------------------------------------------------

namespace
{
    // Duplicate one prim's renderable state onto a fresh client-only volume.
    // Returns false if the source has no volume yet (asset still loading) --
    // there is nothing faithful to copy in that case.
    bool copy_prim_state(LLVOVolume* dst, LLVOVolume* src)
    {
        const LLVolume* src_vol = src->getVolume();
        if (!src_vol)
        {
            LL_WARNS("GhostStudio") << "copy_prim_state: source " << src->getID()
                                    << " has no volume yet; skipping" << LL_ENDL;
            return false;
        }

        dst->setScale(src->getScale(), false);

        // Same sculpt/mesh UUID => the mesh repository hands back the SAME
        // asset (LLVolume + skin info) from cache: no re-download, no re-decode.
        // The drawable and a real LOD must exist first or setVolume builds a
        // placeholder cube instead (lllocalmesh.cpp:1559).
        dst->setVolume(src_vol->getParams(), LLVolumeLODGroup::NUM_LODS - 1);

        const U8 num_tes = src->getNumTEs();
        for (U8 i = 0; i < num_tes; ++i)
        {
            const LLTextureEntry* src_te = src->getTE(i);
            if (!src_te)
            {
                continue;
            }
            // Bulk semantic copy: diffuse id+colour, alpha/fullbright/glow/
            // bump/shiny, UV transform, legacy Blinn-Phong material params and
            // the base GLTF material reference. Deliberately NOT setTETexture/
            // setTEColor -- those only touch the legacy DIFFUSE channel, and a
            // PBR face keeps its colour on the material's BASE COLOUR slot.
            dst->setTE(i, *src_te);

            // GLTF overrides are a separate layer and need a deep copy.
            if (const LLGLTFMaterial* ov = src_te->getGLTFMaterialOverride())
            {
                LLPointer<LLGLTFMaterial> ovp = new LLGLTFMaterial(*ov);
                dst->setTEGLTFMaterialOverride(i, ovp);
            }
        }
        dst->markForUpdate();
        return true;
    }

    // Allocate the viewer object ONLY -- no drawable, no geometry.
    //
    // ⚠️ THE DRAWABLE IS DELIBERATELY NOT CREATED HERE. That was the bug behind
    // the in-world "exploded" clones (2026-07-21). A drawable created while the
    // prim is still an object-tree ROOT, with attachment state already set, is
    // eligible for its OWN LLAvatarBridge (lldrawable.cpp:1204/1247). Doing that
    // per child meant a 248-child linkset spawned 248 independent avatar bridges,
    // each positioning its prim on its own. A real linkset child instead inherits
    // its parent's spatial partition (lldrawable.cpp:1262), and REBUILD_ALL
    // cannot repair topology -- it rebuilds geometry, not object/drawable trees.
    //
    // So: build the whole object hierarchy FIRST, then create drawables in
    // hierarchy order (see cloneAttachmentsFrom).
    LLVOVolume* alloc_local_copy(LLViewerRegion* region, U8 attach_state)
    {
        LLViewerObject* obj = gObjectList.createObjectViewer(LL_PCODE_VOLUME, region);
        LLVOVolume* dst = dynamic_cast<LLVOVolume*>(obj);
        if (!dst)
        {
            if (obj)
            {
                obj->markDead();
            }
            return nullptr;
        }

        // FIRST. See the safety note above -- without this,
        // LLViewerJointAttachment::addObject() can send ObjectDetach to the sim.
        dst->mIsLocalOnly = true;
        dst->mbCanSelect  = false;

        // Before any geometry build: a face is classified RIGGED only when skin
        // info exists AND isAttachment() is true (llvovolume.cpp:6072), and
        // isAttachment() is literally mAttachmentState != 0 (:4015).
        dst->setAttachmentState(attach_state);
        return dst;
    }

    // Decisive attach verification. The point is to make the log state a
    // VERDICT rather than leave us interpreting geometry.
    //
    // NOTE: drawable->getParent() is NOT the joint test for an attachment root.
    // setupDrawable() reparents the drawable's mXform directly
    // (llviewerjointattachment.cpp:115) and never sets LLDrawable::mParent, so
    // a correctly attached root may legitimately have a null drawable parent.
    // Test mXform.getParent() against the attachment joint's xform.
    // Returns false if ANY structural check failed, so the caller can record it
    // and stop /ghostverify from ever printing PASS on this ghost.
    bool log_attach_verify(LLVOAvatar* ghost,
                           LLVOVolume* dst_root,
                           LLVOVolume* src_root,
                           const std::vector<LLVOVolume*>& dst_children)
    {
        LLViewerJointAttachment* target = ghost->getTargetAttachmentPoint(dst_root);
        const bool listed = target &&
            std::find(target->mAttachedObjects.begin(),
                      target->mAttachedObjects.end(),
                      dst_root) != target->mAttachedObjects.end();

        // getParent() returns the base LLXform*; the joint's getXform() is an
        // LLXformMatrix* which upcasts to it for the comparison.
        LLXform* xform_parent = dst_root->mDrawable.notNull()
            ? dst_root->mDrawable->mXform.getParent() : nullptr;
        LLXform* expect_xform = target ? target->getXform() : nullptr;
        LLSpatialBridge* bridge = dst_root->mDrawable.notNull()
            ? dst_root->mDrawable->getSpatialBridge() : nullptr;

        const bool joint_ok  = (xform_parent && xform_parent == expect_xform);
        const bool avatar_ok = (dst_root->getAvatar() == ghost);
        const bool attach_ok = dst_root->isAttachment() && listed && avatar_ok && joint_ok;

        LL_INFOS("GhostStudio")
            << "ATTACH-VERIFY src=" << src_root->getID()
            << " state=" << (S32)dst_root->getAttachmentState()
            << " isAttachment=" << dst_root->isAttachment()
            << " avatar_is_ghost=" << avatar_ok
            << " listed=" << listed
            << " JOINT_PARENT_OK=" << joint_ok
            << " root_bridge=" << (void*)bridge
            << " children=" << dst_children.size()
            << LL_ENDL;

        // TOPOLOGY. The defect that produced the exploded clones was a CHILD
        // owning its own LLAvatarBridge.
        //
        // NOTE the test is "child owns ANY bridge", NOT "child bridge differs
        // from the root's". getSpatialBridge() returns the drawable's OWN
        // mSpatialBridge (lldrawable.h:199) -- a CORRECT child owns none while
        // the root owns the LLAvatarBridge, so a !=-comparison against the root
        // is what CORRECT looks like and would have flagged a working clone as
        // broken. Compare resolved PARTITIONS for the inheritance check instead.
        // PASSIVE checks only -- getSpatialPartition() validates and CORRECTS
        // topology (lldrawable.cpp:1204), so using it here would let the check
        // repair the defect it is meant to catch.
        S32 kids_with_own_bridge = 0, kids_bad_parent = 0;
        for (LLVOVolume* c : dst_children)
        {
            if (c->mDrawable.isNull())
            {
                continue;
            }
            if (c->mDrawable->getSpatialBridge() != nullptr)
            {
                kids_with_own_bridge++;
            }
            if (c->mDrawable->getParent() != dst_root->mDrawable)
            {
                kids_bad_parent++;
            }
        }
        const bool topology_ok = (kids_with_own_bridge == 0 && kids_bad_parent == 0);
        if (!topology_ok)
        {
            LL_WARNS("GhostStudio")
                << "ATTACH-TOPOLOGY src=" << src_root->getID()
                << " children_with_own_bridge=" << kids_with_own_bridge
                << " children_bad_drawable_parent=" << kids_bad_parent
                << " (THIS IS THE EXPLODED-CLONE DEFECT)" << LL_ENDL;
        }
        if (!attach_ok)
        {
            LL_WARNS("GhostStudio")
                << "ATTACH-VERIFY src=" << src_root->getID()
                << " FAILED structural checks -- attachment registration or joint "
                   "parenting did not take" << LL_ENDL;
        }
        return attach_ok && topology_ok;
    }

    // Force a full geometry rebuild so any face built before the object was
    // parented to the ghost gets re-classified against its final state.
    void force_rebuild(LLViewerObject* obj)
    {
        if (!obj || obj->isDead())
        {
            return;
        }
        obj->markForUpdate();
        if (obj->mDrawable.notNull())
        {
            gPipeline.markRebuild(obj->mDrawable, LLDrawable::REBUILD_ALL);
        }
    }
}

S32 LLGhostAvatar::cloneAttachmentsFrom(LLVOAvatar* source)
{
    if (!source || source == this)
    {
        return 0;
    }
    LLViewerRegion* region = getRegion();
    if (!region)
    {
        LL_WARNS("GhostStudio") << "cloneAttachmentsFrom: ghost has no region" << LL_ENDL;
        return 0;
    }

    S32 cloned = 0;
    for (const auto& ap : source->mAttachmentPoints)
    {
        LLViewerJointAttachment* attachment = ap.second;
        if (!attachment)
        {
            continue;
        }
        for (LLViewerObject* src_obj : attachment->mAttachedObjects)
        {
            if (!src_obj || src_obj->isDead())
            {
                continue;   // dead entries are legitimately ignorable
            }

            // Count EVERY LIVE root, BEFORE the type check. Counting only the
            // ones we can handle would make PASS mean "everything supported was
            // cloned" rather than "the outfit was cloned" -- an unsupported
            // root would vanish from both the expected and the failed tallies
            // and a surviving volume attachment could still report PASS.
            mCloneExpectedRoots++;

            LLVOVolume* src_root = dynamic_cast<LLVOVolume*>(src_obj);
            if (!src_root)
            {
                LL_WARNS("GhostStudio") << "attachment root " << src_obj->getID()
                                        << " is not an LLVOVolume; cannot clone it"
                                        << LL_ENDL;
                mCloneFailures++;
                continue;
            }

            // Read the attachment point up front: every duplicated prim needs
            // it set BEFORE its geometry is built, not after.
            const U8 attach_state = src_root->getAttachmentState();

            // Preserve the source's ATTACHMENT-JOINT-LOCAL transform. NOT
            // getRenderPosition()/setPositionAgent(), which are world space.
            const LLVector3    local_pos = src_root->getPosition();
            const LLQuaternion local_rot = src_root->getRotation();

            // ============================================================
            // ORDER BELOW IS THE FIX. Do not reorder without re-reading the
            // note in alloc_local_copy(). Build the ENTIRE object tree before
            // any drawable exists, then create drawables in hierarchy order,
            // then attach, then finally assign geometry.
            // ============================================================

            // -- 1. Allocate root + children as bare objects (no drawables).
            LLVOVolume* dst_root = alloc_local_copy(region, attach_state);
            if (!dst_root)
            {
                LL_WARNS("GhostStudio") << "root allocation failed for "
                                        << src_root->getID() << LL_ENDL;
                mCloneFailures++;
                continue;
            }

            std::vector<LLVOVolume*> src_children;
            std::vector<LLVOVolume*> dst_children;
            bool alloc_ok = true;
            for (LLViewerObject* src_child : src_root->getChildren())
            {
                if (!src_child || src_child->isDead())
                {
                    continue;   // dead entries are legitimately ignorable
                }
                LLVOVolume* src_cv = dynamic_cast<LLVOVolume*>(src_child);
                if (!src_cv)
                {
                    // A live child we cannot reproduce means the linkset can
                    // never be a faithful copy. Fail it rather than quietly
                    // shipping something with a piece missing.
                    LL_WARNS("GhostStudio") << "attachment child " << src_child->getID()
                                            << " is not an LLVOVolume; cannot clone it"
                                            << LL_ENDL;
                    alloc_ok = false;
                    break;
                }
                LLVOVolume* dst_cv = alloc_local_copy(region, attach_state);
                if (!dst_cv)
                {
                    // Do NOT silently skip: that quietly produces an incomplete
                    // linkset that never reaches the copy-failure handling and
                    // can still be reported as a success.
                    alloc_ok = false;
                    break;
                }
                src_children.push_back(src_cv);
                dst_children.push_back(dst_cv);
            }
            if (!alloc_ok)
            {
                LL_WARNS("GhostStudio") << "child allocation failed for "
                                        << src_root->getID()
                                        << "; discarding the whole linkset" << LL_ENDL;
                // Nothing is attached or drawable-backed yet at this point, so
                // plain markDead on each allocated object is sufficient.
                for (LLVOVolume* c : dst_children)
                {
                    c->markDead();
                }
                dst_root->markDead();
                mCloneFailures++;
                continue;
            }

            // -- 2. Object hierarchy FIRST, so no child is ever a drawable root.
            for (size_t ci = 0; ci < dst_children.size(); ++ci)
            {
                dst_children[ci]->setPosition(src_children[ci]->getPosition());
                dst_children[ci]->setRotation(src_children[ci]->getRotation());
                dst_root->addChild(dst_children[ci]);
            }

            // -- 3. Root drawable. Must exist BEFORE addChild(): LLVOAvatar::
            //       addChild only calls attachObject() immediately when a
            //       drawable is present, else it defers via mPendingAttachment
            //       (llvoavatar.cpp:7838/7850).
            gPipeline.createObject(dst_root);
            dst_root->setLOD(LLVolumeLODGroup::NUM_LODS - 1);

            // -- 4. Child drawables, now that each already has dst_root as its
            //       object parent, so they inherit its partition instead of
            //       minting their own bridge.
            //       No explicit setDrawableParent() needed: createObject() sees
            //       the established object parent and wires the drawable parent
            //       itself (pipeline.cpp:2578). The load-bearing part is simply
            //       that the object parent and root drawable already exist.
            for (LLVOVolume* c : dst_children)
            {
                gPipeline.createObject(c);
                c->setLOD(LLVolumeLODGroup::NUM_LODS - 1);
            }

            // -- 5. Attach the completed linkset to the ghost. This runs
            //       attachObject() -> setupDrawable(), which reparents the root
            //       drawable's mXform to the attachment joint.
            addChild(dst_root);

            // -- 6. Restore the joint-local transform. setupDrawable() derived
            //       one from the object's meaningless initial region-space
            //       position, so overwriting it here is required, not harmful.
            dst_root->setPosition(local_pos);
            dst_root->setRotation(local_rot);

            // -- 7. ONLY NOW assign geometry. The first geometry build then
            //       sees correct attachment state, avatar ancestor, drawable
            //       parent and a single shared bridge -- so faces classify
            //       rigged on the first pass instead of needing repair.
            //
            //       Failures here are NOT ignorable: an uncopied volume yields
            //       an invisible or placeholder prim, and counting that as a
            //       successful clone muddies the in-world result.
            //       ANY copy failure -- root or child -- discards the WHOLE
            //       linkset. A partially cloned linkset is worse than none for
            //       an acceptance harness: the prims that DID copy can verify
            //       correctly and print PASS while a missing child is silently
            //       absent, and a placeholder child may read as "static" and
            //       escape both the pending and the not-rigged counters.
            //       PASS has to mean the whole thing cloned.
            bool copy_ok = copy_prim_state(dst_root, src_root);
            size_t failed_child = 0;
            if (copy_ok)
            {
                for (size_t ci = 0; ci < dst_children.size(); ++ci)
                {
                    if (!copy_prim_state(dst_children[ci], src_children[ci]))
                    {
                        copy_ok = false;
                        failed_child = ci + 1;
                        break;
                    }
                }
            }
            if (!copy_ok)
            {
                LL_WARNS("GhostStudio")
                    << "volume copy failed for " << src_root->getID()
                    << (failed_child ? llformat(" (child %d of %d)", (S32)failed_child,
                                                (S32)dst_children.size())
                                     : std::string(" (root)"))
                    << " -- source assets not loaded; discarding the whole linkset"
                    << LL_ENDL;
                // Clear attachment state BEFORE removeChild so removeObject()'s
                // makeStatic() is effective and the drawables re-home cleanly
                // instead of staying active on the avatar's spatial bridge
                // (lllocalmesh.cpp:1403-1425).
                dst_root->setAttachmentState(0);
                for (LLVOVolume* c : dst_children)
                {
                    c->setAttachmentState(0);
                }
                removeChild(dst_root);
                dst_root->markDead();   // cascades to children, llviewerobject.cpp:464
                mCloneFailures++;
                continue;
            }

            // -- 8. Insurance, not the fix.
            force_rebuild(dst_root);
            for (LLVOVolume* c : dst_children)
            {
                force_rebuild(c);
            }

            ClonedLinkset record;
            record.mRoot = dst_root->getID();
            record.mChildren.reserve(dst_children.size());
            for (LLVOVolume* c : dst_children)
            {
                record.mChildren.push_back(c->getID());
            }
            mClonedLinksets.push_back(record);
            cloned++;

            // Structural failures (attachment registration, joint parenting,
            // child bridge/partition topology) must reach the verifier -- the
            // face-level pass cannot see any of them.
            if (!log_attach_verify(this, dst_root, src_root, dst_children))
            {
                mCloneFailures++;
            }
        }
    }

    if (cloned > 0)
    {
        // One explicit override rebuild after the whole linkset is in place.
        // addAttachmentOverridesForObject rejects objects whose getAvatar() is
        // not this avatar (llvoavatar.cpp:7032), so this mutates the GHOST's
        // skeleton and never the source's.
        updateAttachmentOverrides();
    }

    LL_INFOS("GhostStudio") << "cloneAttachmentsFrom: duplicated " << cloned
                            << " attachment root(s)" << LL_ENDL;
    return cloned;
}

void LLGhostAvatar::releaseClonedAttachments()
{
    for (const ClonedLinkset& linkset : mClonedLinksets)
    {
        LLViewerObject* root = gObjectList.findObject(linkset.mRoot);
        if (!root || root->isDead())
        {
            continue;
        }
        // Clear attachment state on the WHOLE linkset FIRST: the makeStatic()
        // inside removeObject() is guarded on !isAttachment(), so with state
        // still set it no-ops and the drawables stay ACTIVE on the avatar's
        // spatial bridge -- rendering adrift from their bounding boxes
        // (lllocalmesh.cpp:1403-1425).
        root->setAttachmentState(0);
        for (LLViewerObject* child : root->getChildren())
        {
            if (child)
            {
                child->setAttachmentState(0);
            }
        }
        removeChild(root);
        if (root->mDrawable.notNull())
        {
            root->mDrawable->mXform.setParent(NULL);
        }
        root->markDead();
    }
    mClonedLinksets.clear();

    // Reset the transactional counters WITH the roots they describe. Clearing
    // the roots but keeping the expected/failure counts would leave a later,
    // perfectly good re-clone on this ghost permanently INCOMPLETE. (Do NOT
    // instead reset at the START of a clone call: that call does not release
    // existing roots, so the recorded roots and the new expected count would
    // then describe different transactions.)
    mCloneExpectedRoots = 0;
    mCloneFailures      = 0;
}

void LLGhostAvatar::markForDeath()
{
    // Deferred death, mirroring LLControlAvatar. markDead() must never run
    // inside a pipeline traversal, and spawn/destroy must stay on the viewer
    // thread outside any sAllowInstancesChange == false window.
    mMarkedForDeath = true;
}

// virtual
void LLGhostAvatar::idleUpdate(LLAgent &agent, const F64 &time)
{
    if (mMarkedForDeath)
    {
        markDead();
        mMarkedForDeath = false;
        return;
    }

    LLVOAvatar::idleUpdate(agent, time);
}

// ---------------------------------------------------------------------------
// Milestone 1 acceptance gate
// ---------------------------------------------------------------------------

namespace
{
    // Usable rigged skin on one object, or null.
    const LLMeshSkinInfo* skin_of(LLViewerObject* obj)
    {
        LLVOVolume* vol = dynamic_cast<LLVOVolume*>(obj);
        if (!vol || !vol->isRiggedMesh())
        {
            return nullptr;
        }
        const LLMeshSkinInfo* skin = vol->getSkinInfo();
        return (skin && !skin->mJointNames.empty()) ? skin : nullptr;
    }

    // First rigged skin we can find on the source's attachments. Any ONE
    // shared LLMeshSkinInfo is enough -- the point is that both ghosts resolve
    // the SAME skin pointer against their OWN skeletons.
    const LLMeshSkinInfo* find_shared_skin(LLVOAvatar* source)
    {
        if (!source)
        {
            return nullptr;
        }
        for (const auto& ap : source->mAttachmentPoints)
        {
            LLViewerJointAttachment* attachment = ap.second;
            if (!attachment)
            {
                continue;
            }
            for (LLViewerObject* obj : attachment->mAttachedObjects)
            {
                if (!obj)
                {
                    continue;
                }
                // Check the attachment root AND its linkset children: a linked
                // attachment can have an unrigged root with rigged child prims,
                // which would otherwise look like "no rigged mesh worn".
                const LLMeshSkinInfo* skin = skin_of(obj);
                if (skin)
                {
                    return skin;
                }
                for (LLViewerObject* child : obj->getChildren())
                {
                    skin = skin_of(child);
                    if (skin)
                    {
                        return skin;
                    }
                }
            }
        }
        return nullptr;
    }

    // Pick a joint that is ACTUALLY IN THIS SKIN. Rotating a joint the skin
    // does not reference would leave the palette untouched and produce a
    // false negative. Prefer a non-root joint so the change is a real pose
    // difference rather than a whole-body transform.
    std::string pick_test_joint(const LLMeshSkinInfo* skin, LLVOAvatar* av)
    {
        if (!skin || !av)
        {
            return std::string();
        }
        // CRITICAL: only the FIRST getMeshJointCount() names get a palette
        // entry -- updateSkinInfoMatrixPalette sizes the palette with exactly
        // that count (llvoavatar.cpp:10507), and getMeshJointCount caps at
        // LL_MAX_JOINTS_PER_MESH_OBJECT (llskinningutil.cpp:95). A joint past
        // the cap resolves fine on the skeleton but has NO matrix in the
        // palette, so rotating it would change nothing and the test would
        // report a false FAIL.
        const U32 palette_joints = LLSkinningUtil::getMeshJointCount(skin);
        const size_t limit = llmin((size_t)palette_joints, skin->mJointNames.size());

        std::string fallback;
        for (size_t i = 0; i < limit; ++i)
        {
            const std::string& name = skin->mJointNames[i];
            if (!av->getJoint(name))
            {
                continue;
            }
            if (fallback.empty())
            {
                fallback = name;
            }
            if (name != "mPelvis" && name != "mRoot" && name != "mTorso")
            {
                return name;
            }
        }
        return fallback;
    }

    // Returns false if the joint could not be posed. The caller MUST treat
    // that as INCONCLUSIVE, never as a test result: if a freshly created
    // ghost's skeleton is not built yet, getJoint() returns null, the pose
    // silently no-ops, and every palette comes out identical -- which would
    // print FAIL and look exactly like broken pose isolation.
    bool pose_joint(LLVOAvatar* av, const std::string& joint_name, F32 radians)
    {
        if (!av || joint_name.empty())
        {
            return false;
        }
        LLJoint* joint = av->getJoint(joint_name);
        if (!joint)
        {
            LL_WARNS("GhostStudio") << "pose_joint: ghost has no joint '" << joint_name
                                    << "' -- skeleton not built?" << LL_ENDL;
            return false;
        }
        LLQuaternion rot;
        rot.setAngleAxis(radians, 0.f, 1.f, 0.f);
        joint->setRotation(rot);

        // Confirm it actually took, rather than trusting the setter.
        const LLQuaternion actual = joint->getRotation();
        if (radians != 0.f && actual == LLQuaternion())
        {
            LL_WARNS("GhostStudio") << "pose_joint: rotation on '" << joint_name
                                    << "' did not stick" << LL_ENDL;
            return false;
        }
        return true;
    }

    // Every joint the palette will index must resolve on this avatar. If any
    // does not, initSkinningMatrixPalette takes its fallback branch
    // (llskinningutil.cpp:155) and the palette stops being a faithful function
    // of the pose -- so a comparison built on it means nothing. Validating
    // joint resolution directly is definitive; comparing against "pure
    // invBind" would be weaker and could misclassify a legitimate bind pose.
    //
    // Checked by NAME, not by skin->mJointNums: those are filled in lazily by
    // initJointNums() on the FIRST palette build, so before that they are -1
    // and a num-based check would fail spuriously.
    bool palette_joints_resolve(LLVOAvatar* av, const LLMeshSkinInfo* skin)
    {
        if (!av || !skin || !av->isBuilt())
        {
            return false;
        }
        const U32 count = LLSkinningUtil::getMeshJointCount(skin);
        for (U32 j = 0; j < count && j < skin->mJointNames.size(); ++j)
        {
            if (!av->getJoint(skin->mJointNames[j]))
            {
                LL_WARNS("GhostStudio") << "palette joint '" << skin->mJointNames[j]
                                        << "' does not resolve on ghost" << LL_ENDL;
                return false;
            }
        }
        return true;
    }

    size_t count_differing(const std::vector<F32>& a, const std::vector<F32>& b)
    {
        size_t n = 0;
        for (size_t i = 0; i < a.size() && i < b.size(); ++i)
        {
            if (a[i] != b[i])
            {
                n++;
            }
        }
        return n;
    }
}

//static
bool LLGhostAvatar::runPaletteIsolationTest()
{
    LLViewerRegion* region = gAgent.getRegion();
    if (!region)
    {
        LL_WARNS("GhostStudio") << "/ghosttest: no region" << LL_ENDL;
        return false;
    }
    if (!isAgentAvatarValid())
    {
        LL_WARNS("GhostStudio") << "/ghosttest: no agent avatar to clone" << LL_ENDL;
        return false;
    }
    LLVOAvatar* source = (LLVOAvatar*)gAgentAvatarp;

    const LLMeshSkinInfo* skin = find_shared_skin(source);
    if (!skin)
    {
        LL_WARNS("GhostStudio") << "/ghosttest: source has no rigged mesh attachment; "
                                   "wear rigged mesh and retry" << LL_ENDL;
        return false;
    }

    const std::string joint_name = pick_test_joint(skin, source);
    if (joint_name.empty())
    {
        LL_WARNS("GhostStudio") << "/ghosttest: no usable joint in skin; INCONCLUSIVE" << LL_ENDL;
        return false;
    }

    // ---------------------------------------------------------------------
    // THREE ghosts, ALL AT THE SAME WORLD POSITION.
    //
    // The position part is not a detail -- it is the whole validity of the
    // measurement. The palette is invBind * joint WORLD matrix, so ghosts at
    // different positions produce different palettes no matter what their
    // poses are. Comparing palettes across separated ghosts would "pass"
    // even if pose isolation were completely broken. Same transform, vary
    // ONLY the pose.
    //
    //   A, B : identical pose  -> NEGATIVE CONTROL, palettes must MATCH
    //   C    : different pose  -> THE TEST,        palette must DIFFER from A
    //
    // The control is what makes this decisive: it proves the comparison can
    // return "same", so a "differs" result is attributable to the pose alone.
    // Each ghost's palette is read exactly ONCE, because
    // updateSkinInfoMatrixPalette caches per gFrameCount and a second read in
    // the same frame would return the first result.
    // ---------------------------------------------------------------------
    const LLVector3 test_pos = source->getPositionAgent();
    LLGhostAvatar* ghosts[3] = { nullptr, nullptr, nullptr };
    std::vector<LLUUID> spawned;

    // Roll back every ghost created by THIS run if we bail part-way, so a
    // failed run does not leave debris that accumulates across retries.
    auto rollback = [&spawned]()
    {
        for (const LLUUID& id : spawned)
        {
            LLViewerObject* obj = gObjectList.findObject(id);
            LLGhostAvatar* g = dynamic_cast<LLGhostAvatar*>(obj);
            if (g && !g->isDead())
            {
                g->markForDeath();
            }
        }
    };

    for (S32 i = 0; i < 3; ++i)
    {
        LLGhostAvatar* ghost = (LLGhostAvatar*)gObjectList.createObjectViewer(
            LL_PCODE_LEGACY_AVATAR, region, LLViewerObject::CO_FLAG_GHOST_AVATAR);
        if (!ghost)
        {
            LL_WARNS("GhostStudio") << "/ghosttest: failed to create ghost " << i << LL_ENDL;
            rollback();
            return false;
        }
        spawned.push_back(ghost->getID());

        if (!ghost->cloneAppearanceFrom(source))
        {
            LL_WARNS("GhostStudio") << "/ghosttest: appearance copy failed for ghost "
                                    << i << LL_ENDL;
            rollback();
            return false;
        }
        ghost->setGhostPosition(test_pos);

        // NOTE: deliberately NO attachments here. /ghosttest is the palette
        // gate and wants to stay cheap; duplicating a full outfit onto three
        // ghosts is heavy. It also protects the negative control: attachment
        // joint-position overrides mutate the skeleton, so attachments must be
        // applied to ALL ghosts or NONE, or A and B stop being comparable.
        // Use /ghostdress for the dressed-clone test.
        ghosts[i] = ghost;
    }

    // Pose AFTER appearance (appearance application can touch the skeleton).
    // If ANY pose fails to apply, the comparison is meaningless -- bail as
    // INCONCLUSIVE rather than printing a FAIL we cannot trust.
    const bool posed = pose_joint(ghosts[0], joint_name, 0.0f)
                     & pose_joint(ghosts[1], joint_name, 0.0f)   // control: same as A
                     & pose_joint(ghosts[2], joint_name, 1.2f);  // test:    different
    if (!posed)
    {
        LL_WARNS("GhostStudio") << "/ghosttest: INCONCLUSIVE -- could not pose joint '"
                                << joint_name << "' on all three ghosts. This is NOT a "
                                   "failure of pose isolation; the skeletons were not "
                                   "ready. Retry once the ghosts have built." << LL_ENDL;
        rollback();
        return false;
    }

    // Guard the palette's validity BEFORE reading it. Codex verified the
    // skeleton is built synchronously inside createObjectViewer today, so
    // this should always pass -- it exists so that if construction ever
    // becomes deferred, this harness reports INCONCLUSIVE instead of
    // silently degrading into a confident FAIL.
    for (S32 i = 0; i < 3; ++i)
    {
        if (!palette_joints_resolve(ghosts[i], skin))
        {
            LL_WARNS("GhostStudio") << "/ghosttest: INCONCLUSIVE -- ghost " << i
                                    << " is not built or has unresolved palette joints. "
                                       "NOT a pose-isolation failure." << LL_ENDL;
            rollback();
            return false;
        }
    }

    const MatrixPaletteCache& palA = ghosts[0]->updateSkinInfoMatrixPalette(skin);
    const MatrixPaletteCache& palB = ghosts[1]->updateSkinInfoMatrixPalette(skin);
    const MatrixPaletteCache& palC = ghosts[2]->updateSkinInfoMatrixPalette(skin);

    const size_t n = palA.mGLMp.size();
    if (n == 0 || palB.mGLMp.size() != n || palC.mGLMp.size() != n)
    {
        LL_WARNS("GhostStudio") << "/ghosttest: INCONCLUSIVE -- palette sizes "
                                << n << ", " << palB.mGLMp.size() << ", "
                                << palC.mGLMp.size() << LL_ENDL;
        rollback();
        return false;
    }

    const size_t control_diff = count_differing(palA.mGLMp, palB.mGLMp);
    const size_t test_diff    = count_differing(palA.mGLMp, palC.mGLMp);

    LL_INFOS("GhostStudio") << "/ghosttest: skin hash " << skin->mHash
                            << " joints " << skin->mJointNames.size()
                            << " joint '" << joint_name << "'"
                            << " palette floats " << n
                            << " | control diff " << control_diff
                            << " | test diff " << test_diff << LL_ENDL;

    // Commit to the harness store only once the run completed.
    sTestGhostIds.insert(sTestGhostIds.end(), spawned.begin(), spawned.end());

    if (control_diff != 0)
    {
        LL_WARNS("GhostStudio") << "/ghosttest: INCONCLUSIVE -- the CONTROL pair differs ("
                                << control_diff << " floats). Two identically posed, "
                                   "identically placed ghosts should be byte-identical; "
                                   "something else is varying." << LL_ENDL;
        return false;
    }
    if (test_diff == 0)
    {
        LL_WARNS("GhostStudio") << "/ghosttest: FAIL -- differently posed ghosts hold "
                                   "IDENTICAL palettes. Either the pose did not take or "
                                   "the palette is not per-entity." << LL_ENDL;
        return true; // test RAN; verdict is FAIL
    }

    LL_INFOS("GhostStudio") << "/ghosttest: PASS -- control identical, posed ghost differs in "
                            << test_diff << " floats. Per-entity pose isolation for a SHARED "
                               "skin is confirmed." << LL_ENDL;
    return true;
}

//static
bool LLGhostAvatar::spawnDressedGhost()
{
    LLViewerRegion* region = gAgent.getRegion();
    if (!region)
    {
        LL_WARNS("GhostStudio") << "/ghostdress: no region" << LL_ENDL;
        return false;
    }
    if (!isAgentAvatarValid())
    {
        LL_WARNS("GhostStudio") << "/ghostdress: no agent avatar to clone" << LL_ENDL;
        return false;
    }
    LLVOAvatar* source = (LLVOAvatar*)gAgentAvatarp;

    LLGhostAvatar* ghost = (LLGhostAvatar*)gObjectList.createObjectViewer(
        LL_PCODE_LEGACY_AVATAR, region, LLViewerObject::CO_FLAG_GHOST_AVATAR);
    if (!ghost)
    {
        LL_WARNS("GhostStudio") << "/ghostdress: failed to create ghost" << LL_ENDL;
        return false;
    }
    if (!ghost->cloneAppearanceFrom(source))
    {
        LL_WARNS("GhostStudio") << "/ghostdress: appearance copy failed" << LL_ENDL;
        ghost->markForDeath();
        return false;
    }

    // Stand it a couple of metres in front of the agent so it is actually
    // inspectable, rather than co-located and interpenetrating.
    ghost->setGhostPosition(source->getPositionAgent() + gAgent.getAtAxis() * 2.5f);

    const S32 n = ghost->cloneAttachmentsFrom(source);

    sTestGhostIds.push_back(ghost->getID());
    LL_INFOS("GhostStudio") << "/ghostdress: dressed ghost spawned with " << n
                            << " attachment root(s). /ghostclear to remove." << LL_ENDL;
    return true;
}

//static
bool LLGhostAvatar::recheckStructure(LLGhostAvatar* ghost,
                                     LLViewerObject* root,
                                     const ClonedLinkset& linkset)
{
    LLVOVolume* vroot = dynamic_cast<LLVOVolume*>(root);
    if (!vroot)
    {
        return false;
    }

    // STRICT attachment-point lookup, NOT getTargetAttachmentPoint().
    // That helper warns and FALLS BACK TO CHEST on an invalid point, so a
    // corrupted-but-nonzero attachment state could still satisfy
    // isAttachment() + listed + joint-parent and produce a false PASS.
    // Decode the id and require it to resolve directly.
    const S32 point = ATTACHMENT_ID_FROM_STATE(vroot->getAttachmentState());
    attachment_map_t::const_iterator it = ghost->mAttachmentPoints.find(point);
    LLViewerJointAttachment* target =
        (it != ghost->mAttachmentPoints.end()) ? it->second : nullptr;

    const bool listed = target &&
        std::find(target->mAttachedObjects.begin(),
                  target->mAttachedObjects.end(),
                  vroot) != target->mAttachedObjects.end();

    LLXform* xform_parent = vroot->mDrawable.notNull()
        ? vroot->mDrawable->mXform.getParent() : nullptr;
    LLXform* expect_xform = target ? target->getXform() : nullptr;

    const bool attach_ok = vroot->isAttachment() && listed
                        && (vroot->getAvatar() == ghost)
                        && (xform_parent && xform_parent == expect_xform);

    // PASSIVE topology only. Do NOT call getSpatialPartition() here: it is not
    // an observational getter -- it validates and CORRECTS topology, and for a
    // root it can destroy a bad bridge and mint a fresh LLAvatarBridge
    // (lldrawable.cpp:1204). Calling it would let the verifier repair the very
    // defect it exists to detect and then report PASS on it.
    LLSpatialBridge* root_bridge = vroot->mDrawable.notNull()
        ? vroot->mDrawable->getSpatialBridge() : nullptr;

    bool topology_ok = (root_bridge != nullptr);
    for (const LLUUID& cid : linkset.mChildren)
    {
        LLViewerObject* c = gObjectList.findObject(cid);
        if (!c || c->isDead() || c->mDrawable.isNull())
        {
            continue;   // counted separately as missing
        }
        // A child must own NO bridge of its own, and must hang off the root's
        // drawable. Both are readable without provoking a repair.
        if (c->mDrawable->getSpatialBridge() != nullptr
            || c->mDrawable->getParent() != vroot->mDrawable)
        {
            topology_ok = false;
            break;
        }
    }

    if (!attach_ok || !topology_ok)
    {
        LL_WARNS("GhostStudio")
            << "RECHECK root=" << linkset.mRoot
            << " decoded_point=" << point
            << " target=" << (void*)target
            << " listed=" << listed
            << " attach_ok=" << attach_ok
            << " topology_ok=" << topology_ok
            << " (structure degraded since cloning)" << LL_ENDL;
    }
    return attach_ok && topology_ok;
}

//static
void LLGhostAvatar::verifyClonedAttachments()
{
    // Deliberately a SEPARATE, USER-TRIGGERED pass rather than part of cloning.
    //
    // setVolume() only SCHEDULES work and force_rebuild() only MARKS a rebuild;
    // rigged classification and face->mAvatar are assigned later, during the
    // pipeline's geometry rebuild (llvovolume.cpp:6072/6131). Checking faces in
    // the cloning frame legitimately sees zero faces and would print what looks
    // like total failure. Run this a moment after /ghostdress.
    S32 ghosts_seen = 0;
    for (const LLUUID& gid : sTestGhostIds)
    {
        LLGhostAvatar* ghost = dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(gid));
        if (!ghost || ghost->isDead())
        {
            continue;
        }
        ghosts_seen++;

        S32 prims = 0, prims_pending = 0;
        S32 faces_total = 0, faces_rigged = 0, faces_wrong_avatar = 0;
        S32 skinned_prims = 0, skinned_prims_unrigged = 0;
        S32 missing_roots = 0, missing_children = 0, reparented_children = 0;
        S32 structure_now_bad = 0;

        for (const LLGhostAvatar::ClonedLinkset& linkset : ghost->mClonedLinksets)
        {
            LLViewerObject* root = gObjectList.findObject(linkset.mRoot);
            if (!root || root->isDead())
            {
                // A recorded root that has since vanished is a real failure,
                // not something to skip past on the way to a PASS.
                missing_roots++;
                continue;
            }

            // Walk the RECORDED prims, not root->getChildren(). Rebuilding the
            // linkset from whoever is still alive is how a dead or detached
            // child disappears without moving any counter -- the survivors then
            // satisfy PASS on a clone that has quietly lost pieces.
            std::vector<LLViewerObject*> chain;
            chain.push_back(root);
            for (const LLUUID& cid : linkset.mChildren)
            {
                LLViewerObject* c = gObjectList.findObject(cid);
                if (!c || c->isDead())
                {
                    missing_children++;
                    continue;
                }
                if (c->getParent() != root)
                {
                    reparented_children++;   // still alive, but left its linkset
                    continue;
                }
                chain.push_back(c);
            }

            // Re-check structure NOW, not just at clone time. Attachment
            // registration, joint parenting and partition topology can all be
            // disturbed after cloning, and a stale one-time pass would hide it.
            if (!recheckStructure(ghost, root, linkset))
            {
                structure_now_bad++;
            }

            for (LLViewerObject* obj : chain)
            {
                LLVOVolume* vol = dynamic_cast<LLVOVolume*>(obj);
                if (!vol)
                {
                    continue;
                }
                prims++;

                if (vol->mDrawable.isNull())
                {
                    prims_pending++;   // geometry not built yet -- NOT a failure
                    continue;
                }

                // Only a prim WITH skin info is required to be rigged; static
                // prims in the same linkset are legitimately unrigged.
                const bool expect_rigged = (vol->getSkinInfo() != nullptr);
                bool saw_rigged = false;
                S32  non_null_faces = 0;

                for (S32 f = 0; f < vol->mDrawable->getNumFaces(); ++f)
                {
                    LLFace* face = vol->mDrawable->getFace(f);
                    if (!face)
                    {
                        continue;   // slot exists but the face is not populated yet
                    }
                    non_null_faces++;
                    faces_total++;
                    if (face->isState(LLFace::RIGGED))
                    {
                        faces_rigged++;
                        saw_rigged = true;
                        if (face->mAvatar != ghost)
                        {
                            faces_wrong_avatar++;
                        }
                    }
                }

                // Count NON-NULL faces, not face SLOTS: a drawable can hold
                // slots before the faces are populated, and calling that a
                // failure would be a transient false FAIL.
                if (non_null_faces == 0)
                {
                    prims_pending++;
                    continue;
                }
                if (expect_rigged)
                {
                    skinned_prims++;
                    if (!saw_rigged)
                    {
                        skinned_prims_unrigged++;
                    }
                }
            }
        }

        const S32 recorded  = (S32)ghost->mClonedLinksets.size();
        const S32 shortfall = ghost->mCloneExpectedRoots - recorded;

        LL_INFOS("GhostStudio")
            << "GHOSTVERIFY ghost=" << gid
            << " roots expected=" << ghost->mCloneExpectedRoots
            << " recorded=" << recorded
            << " missing_roots=" << missing_roots
            << " missing_children=" << missing_children
            << " reparented_children=" << reparented_children
            << " structure_degraded=" << structure_now_bad
            << " clone_failures=" << ghost->mCloneFailures
            << " | prims=" << prims
            << " pending(no geometry yet)=" << prims_pending
            << " faces=" << faces_total
            << " rigged=" << faces_rigged
            << " rigged_wrong_avatar=" << faces_wrong_avatar
            << " skinned_prims=" << skinned_prims
            << " skinned_prims_NOT_rigged=" << skinned_prims_unrigged
            << LL_ENDL;

        // Completeness gates PASS. Checked BEFORE the face verdict: a clone
        // that dropped an attachment must never be able to report PASS just
        // because the pieces that survived happen to be correct.
        if (ghost->mCloneFailures > 0 || shortfall > 0 || missing_roots > 0
            || missing_children > 0 || reparented_children > 0 || structure_now_bad > 0)
        {
            LL_WARNS("GhostStudio")
                << "GHOSTVERIFY: INCOMPLETE -- " << ghost->mCloneFailures
                << " clone failure(s), " << shortfall << " attachment(s) never recorded, "
                << missing_roots << " recorded root(s) now missing, "
                << missing_children << " recorded child(ren) now missing, "
                << reparented_children << " child(ren) left their linkset, "
                << structure_now_bad << " linkset(s) structurally degraded. Cannot PASS: this "
                   "clone is not a faithful copy of the source, whatever the surviving prims say."
                // Surface face-level corruption too, so INCOMPLETE does not
                // hide something the reader would still want to know about.
                << " [also: rigged_wrong_avatar=" << faces_wrong_avatar
                << " skinned_prims_NOT_rigged=" << skinned_prims_unrigged << "]"
                << LL_ENDL;
            continue;
        }

        if (prims_pending > 0)
        {
            LL_INFOS("GhostStudio")
                << "GHOSTVERIFY: PENDING -- " << prims_pending << " prim(s) have no geometry "
                   "yet. Not a failure; wait and re-run /ghostverify." << LL_ENDL;
        }
        else if (skinned_prims == 0)
        {
            LL_WARNS("GhostStudio")
                << "GHOSTVERIFY: INCONCLUSIVE -- no prim carries skin info, so nothing "
                   "here tests rigged skinning." << LL_ENDL;
        }
        else if (skinned_prims_unrigged == 0 && faces_wrong_avatar == 0)
        {
            LL_INFOS("GhostStudio")
                << "GHOSTVERIFY: PASS -- every skinned prim has rigged faces and every "
                   "rigged face skins to THIS ghost." << LL_ENDL;
        }
        else
        {
            LL_WARNS("GhostStudio")
                << "GHOSTVERIFY: FAIL -- " << skinned_prims_unrigged
                << " skinned prim(s) produced no rigged face; " << faces_wrong_avatar
                << " rigged face(s) bound to the WRONG avatar." << LL_ENDL;
        }
    }

    if (ghosts_seen == 0)
    {
        LL_WARNS("GhostStudio") << "/ghostverify: no live ghosts. Run /ghostdress first."
                                << LL_ENDL;
    }
}

//static
void LLGhostAvatar::clearTestGhosts()
{
    S32 released = 0;
    for (const LLUUID& id : sTestGhostIds)
    {
        LLGhostAvatar* ghost = dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(id));
        if (ghost && !ghost->isDead())
        {
            // Explicit attachment teardown before killing the ghost: markDead()
            // does cascade to children, but detaching properly also removes the
            // joint overrides and keeps spatial-partition bookkeeping honest.
            ghost->releaseClonedAttachments();
            ghost->markForDeath();
            released++;
        }
    }
    sTestGhostIds.clear();
    LL_INFOS("GhostStudio") << "/ghostclear: released " << released << " test ghost(s)" << LL_ENDL;
}
