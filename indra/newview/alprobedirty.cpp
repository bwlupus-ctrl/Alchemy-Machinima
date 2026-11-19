/**
 * @file alprobedirty.cpp
 * @brief [ProbeOnDemand] Read-only dirty-event recorder (see alprobedirty.h).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alprobedirty.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "lldrawable.h"
#include "llface.h"
#include "llquaternion.h"
#include "llspatialpartition.h"
#include "llviewerobject.h"
#include "llviewertexture.h"
#include "llvovolume.h"

bool gProbeDirtyRecording = false;
U8 gProbeDirtyTag = 0;

namespace ALProbeDirty
{
namespace
{
namespace sched = ALProbeSched;

U32 sProbeResolution = 128;
U64 sSerial = 0;
F64 sNow = 0.0;
F32 sFrameDt = 1.f / 60.f;
U64 sOp = 0;
S32 sMoveDepth = 0;

sched::DiscreteNotes sDiscrete;
sched::DebounceMap sMotion;

// Frame counters (reported and reset by drain()).
U32 sDropped[TAG_COUNT] = {};
U32 sDroppedDyn = 0;
U32 sNoopMove = 0;
U32 sRebal = 0;
U32 sRaw = 0;
U32 sMotionNotes = 0;

// Octree membership churn within one frame (insert + remove of one key). Bounded:
// during teleport / login the recorder can run for many frames without a drain.
std::unordered_set<const void*> sInsertedThisFrame;
std::unordered_set<const void*> sRemovedThisFrame;
constexpr size_t kMaxMembershipSet = 16384;

// Object-cache culling: ids culled by the region's VO cache, and ids re-created
// from it that have not had a real server update since (cache-born).
sched::CacheProvenance sProvenance;

// Fan-out above this many notes becomes one global texture event.
constexpr S32 kMaxFanout = 4096;

// Global events carry no useful bounds; one key per class bit combination.
char sGlobalKeys[16] = {};

void clearAll()
{
    sDiscrete.clear();
    sMotion.clear();
    sMotion.resetStats();
    for (U32& n : sDropped)
    {
        n = 0;
    }
    sDroppedDyn = 0;
    sNoopMove = 0;
    sRebal = 0;
    sRaw = 0;
    sMotionNotes = 0;
    sMoveDepth = 0;
    sInsertedThisFrame.clear();
    sRemovedThisFrame.clear();
    sProvenance.clear();
    gProbeDirtyTag = TAG_NONE;
}

// Which part of the scene does this drawable belong to? Returns false (and
// counts) when it is not tracked: HUD, attachments / animated objects / avatars
// / particles (DYN), probe prims, and the environment objects whose changes the
// environment hash already covers.
bool classify(LLDrawable* drawable, U8& cls)
{
    LLViewerObject* vobj = drawable->getVObj();
    if (!vobj)
    {
        return false; // bridge / orphaned drawable
    }
    if (vobj->isHUDAttachment())
    {
        return false;
    }
    const LLPCode pcode = vobj->getPCode();
    if (vobj->isAttachment() || vobj->isAnimatedObject() || vobj->isAvatar() ||
        pcode == LLViewerObject::LL_VO_PART_GROUP ||
        pcode == LLViewerObject::LL_VO_HUD_PART_GROUP)
    {
        ++sDroppedDyn;
        return false;
    }
    if (vobj->isReflectionProbe())
    {
        return false;
    }
    if (pcode == LLViewerObject::LL_VO_SURFACE_PATCH || pcode == LLViewerObject::LL_VO_WATER ||
        pcode == LLViewerObject::LL_VO_VOID_WATER)
    {
        cls = static_cast<U8>(sched::C_TERRAIN_WATER);
        return true;
    }
    if (pcode == LLViewerObject::LL_VO_SKY || pcode == LLViewerObject::LL_VO_WL_SKY ||
        pcode == LLViewerObject::LL_VO_CLOUDS)
    {
        return false; // environment: covered by the environment generation
    }
    cls = static_cast<U8>(sched::C_STATIC);
    return true;
}

// World bounds: the drawable's spatial extents (min, max); inside a bridge
// partition the bridge's world extents; no group or non-finite -> a sphere
// around its agent position.
void worldBounds(LLDrawable* drawable, F32 mn[3], F32 mx[3])
{
    const LLVector4a* ext = nullptr;
    LLSpatialGroup* group = drawable->getSpatialGroup();
    if (group)
    {
        LLSpatialPartition* part = group->getSpatialPartition();
        LLSpatialBridge* bridge = part ? part->asBridge() : nullptr;
        if (bridge)
        {
            LLDrawable* bridge_drawable = bridge;
            if (bridge_drawable->getEntry())
            {
                ext = bridge_drawable->getSpatialExtents();
            }
        }
        else if (drawable->getEntry())
        {
            ext = drawable->getSpatialExtents();
        }
    }
    if (ext)
    {
        for (S32 i = 0; i < 3; ++i)
        {
            mn[i] = ext[0].getF32ptr()[i];
            mx[i] = ext[1].getF32ptr()[i];
        }
        if (sched::boxFinite(mn, mx) && !sched::boxIsEmpty(mn, mx))
        {
            return;
        }
    }
    const LLVector3 pos = drawable->getPositionAgent();
    const F32 r = 0.5f * drawable->getScale().magVec() + 0.5f;
    const F32 c[3] = { pos.mV[0], pos.mV[1], pos.mV[2] };
    sched::boxSphere(mn, mx, c, r);
}

// A note is dropped (and counted) inside a tag scope.
bool tagged()
{
    if (gProbeDirtyTag != TAG_NONE)
    {
        const U8 tag = gProbeDirtyTag;
        if (tag < TAG_COUNT)
        {
            ++sDropped[tag];
        }
        return true;
    }
    return false;
}

bool boundsDiffer(const F32 a_mn[3], const F32 a_mx[3], const F32 b_mn[3], const F32 b_mx[3])
{
    for (S32 i = 0; i < 3; ++i)
    {
        if (std::fabs(a_mn[i] - b_mn[i]) > 1e-4f || std::fabs(a_mx[i] - b_mx[i]) > 1e-4f)
        {
            return true;
        }
    }
    return false;
}

void loadOld(const LLVector4a* old_mn_mx, F32 mn[3], F32 mx[3])
{
    for (S32 i = 0; i < 3; ++i)
    {
        mn[i] = old_mn_mx[0].getF32ptr()[i];
        mx[i] = old_mn_mx[1].getF32ptr()[i];
    }
}

// The common path of every drawable note.
bool noteCommon(LLDrawable* drawable, const LLVector4a* old_mn_mx, U16 reason,
                sched::Motion motion, bool only_if_changed, U64 min_face_op = 0)
{
    if (!gProbeDirtyRecording || !drawable)
    {
        return false;
    }
    ++sRaw;
    U8 cls = 0;
    if (!classify(drawable, cls))
    {
        return false;
    }
    if (!sProvenance.bornEmpty() &&
        sProvenance.isBorn(sched::idTagOf(drawable->getVObj()->getID())))
    {
        ++sDropped[TAG_VOCACHE]; // re-created from the object cache: not a scene change
        return false;
    }
    if (tagged())
    {
        return false;
    }
    F32 mn[3], mx[3];
    worldBounds(drawable, mn, mx);
    if (old_mn_mx)
    {
        F32 omn[3], omx[3];
        loadOld(old_mn_mx, omn, omx);
        if (only_if_changed && !boundsDiffer(mn, mx, omn, omx))
        {
            return false;
        }
        if (sched::boxFinite(omn, omx) && !sched::boxIsEmpty(omn, omx))
        {
            sched::boxUnion(mn, mx, omn, omx);
        }
    }
    if (!sched::boxFinite(mn, mx))
    {
        cls = static_cast<U8>(sched::C_GLOBAL); // cannot be located: everyone
        for (S32 i = 0; i < 3; ++i)
        {
            mn[i] = 0.f;
            mx[i] = 0.f;
        }
    }
    if (motion == sched::Motion::NONE)
    {
        sDiscrete.note(drawable, mn, mx, reason, cls, sSerial, min_face_op);
    }
    else
    {
        ++sMotionNotes;
        const U64 tag = sched::idTagOf(drawable->getVObj()->getID());
        sMotion.onMotion(drawable, static_cast<U8>(motion), tag, mn, mx, reason, cls, false,
                         sSerial, sNow, sFrameDt);
    }
    return true;
}

void fanoutLight(LLVOVolume* volume, U16 reason, U64 min_face_op)
{
    if (!volume || !volume->mDrawable)
    {
        return;
    }
    if (!sProvenance.bornEmpty() && sProvenance.isBorn(sched::idTagOf(volume->getID())))
    {
        ++sDropped[TAG_VOCACHE]; // a light re-created from the object cache
        return;
    }
    const LLVector3 pos = volume->getPositionAgent();
    const F32 c[3] = { pos.mV[0], pos.mV[1], pos.mV[2] };
    F32 mn[3], mx[3];
    sched::boxSphere(mn, mx, c, volume->getLightRadius() * 1.5f);
    if (!sched::boxFinite(mn, mx))
    {
        return;
    }
    sDiscrete.note(volume, mn, mx, reason, static_cast<U8>(sched::C_LIGHT), sSerial,
                   min_face_op, volume->isLightSpotlight());
}

} // namespace

void setRecording(bool on)
{
    if (on == gProbeDirtyRecording)
    {
        return;
    }
    gProbeDirtyRecording = on;
    if (!on)
    {
        clearAll();
    }
}

void reset()
{
    clearAll();
}

void setProbeResolution(U32 resolution)
{
    sProbeResolution = resolution;
}

void setFrame(U64 serial, F64 now, F32 frame_dt)
{
    sSerial = serial;
    sNow = now;
    const F32 dt = std::min(std::max(frame_dt, 0.0005f), 5.f);
    sFrameDt = (sFrameDt <= 0.f) ? dt : 0.9f * sFrameDt + 0.1f * dt;
}

U64 nextOp()
{
    return ++sOp;
}

U64 lastOp()
{
    return sOp;
}

void enterMove()
{
    ++sMoveDepth;
}

void leaveMove()
{
    if (sMoveDepth > 0)
    {
        --sMoveDepth;
    }
}

void snapshotBounds(LLDrawable* drawable, LLVector4a out[2])
{
    F32 mn[3], mx[3];
    worldBounds(drawable, mn, mx);
    out[0].load3(mn);
    out[1].load3(mx);
}

void noteDrawable(LLDrawable* drawable, U16 reason, sched::Motion motion)
{
    noteCommon(drawable, nullptr, reason, motion, false);
}

void noteDrawableBounds(LLDrawable* drawable, const LLVector4a* old_mn_mx, U16 reason,
                        sched::Motion motion)
{
    noteCommon(drawable, old_mn_mx, reason, motion, false);
}

bool noteDrawableBoundsIfChanged(LLDrawable* drawable, const LLVector4a* old_mn_mx,
                                 U16 reason, sched::Motion motion)
{
    return noteCommon(drawable, old_mn_mx, reason, motion, true);
}

void noteNoopMove()
{
    ++sNoopMove;
}

void noteXformDelta(LLDrawable* drawable, const F32 old_pos_v[3], const F32 old_rot_v[4],
                    const F32 old_scale_v[3], const LLVector4a* old_extents,
                    const LLVector3& new_pos, const LLQuaternion& new_rot,
                    const LLVector3& new_scale)
{
    if (!gProbeDirtyRecording)
    {
        return;
    }
    const LLVector3 old_pos(old_pos_v);
    const LLVector3 old_scale(old_scale_v);
    const LLQuaternion old_rot(old_rot_v[0], old_rot_v[1], old_rot_v[2], old_rot_v[3]);
    const F32 dp = (new_pos - old_pos).magVec();
    const F32 dr = 1.f - std::fabs(dot(old_rot, new_rot));
    const F32 ds = (new_scale - old_scale).magVec();
    if (dp > 1e-4f || dr > 1e-7f || ds > 1e-4f)
    {
        noteCommon(drawable, old_extents, sched::R_GEOM, sched::Motion::XFORM, false);
    }
}

void noteInsertion(LLDrawable* drawable)
{
    if (!gProbeDirtyRecording || !drawable)
    {
        return;
    }
    if (sMoveDepth > 0)
    {
        // Inside a move the bounds change (if any) is noted by the move itself
        // (H1b): a re-bin with unchanged bounds is not motion.
        return;
    }
    if (sRemovedThisFrame.count(drawable) > 0)
    {
        ++sRebal;
    }
    else if (sInsertedThisFrame.size() < kMaxMembershipSet)
    {
        sInsertedThisFrame.insert(drawable);
    }
    noteCommon(drawable, nullptr, sched::R_GEOM, sched::Motion::NONE, false);
}

// Deletion: the removal event covers the object's last bounds merged with any
// motion it still had pending, and the pointer's debounce states are erased (the
// pointer may be reused by a different object).
void noteRemoval(LLDrawable* drawable)
{
    if (!gProbeDirtyRecording || !drawable)
    {
        return;
    }
    if (sMoveDepth > 0)
    {
        return; // see noteInsertion
    }
    if (sInsertedThisFrame.count(drawable) > 0)
    {
        ++sRebal;
    }
    else if (sRemovedThisFrame.size() < kMaxMembershipSet)
    {
        sRemovedThisFrame.insert(drawable);
    }
    ++sRaw;
    U8 cls = 0;
    sched::Event merged;
    if (!classify(drawable, cls))
    {
        sMotion.takePending(drawable, merged); // nothing to deliver for an untracked object
        return;
    }
    LLViewerObject* removed_vobj = drawable->getVObj();
    const U64 removed_tag = removed_vobj ? sched::idTagOf(removed_vobj->getID()) : 0;
    if (gProbeDirtyTag == TAG_VOCACHE)
    {
        // The region's object cache culled this object (the camera turned): not a
        // scene change. Remember it so its re-creation is recognised.
        if (removed_tag != 0)
        {
            sProvenance.noteCulled(removed_tag);
        }
        ++sDropped[TAG_VOCACHE];
        sMotion.takePending(drawable, merged);
        return;
    }
    if (tagged())
    {
        return;
    }
    if (removed_tag != 0)
    {
        sProvenance.noteAuthoritative(removed_tag); // a REAL kill ends both markers
    }
    F32 mn[3], mx[3];
    worldBounds(drawable, mn, mx);
    if (!sched::boxFinite(mn, mx))
    {
        cls = static_cast<U8>(sched::C_GLOBAL);
        for (S32 i = 0; i < 3; ++i)
        {
            mn[i] = 0.f;
            mx[i] = 0.f;
        }
    }
    merged = sched::makeEvent(mn, mx, sched::R_GEOM, cls, sSerial);
    sMotion.takePending(drawable, merged);
    sDiscrete.note(drawable, merged.mMin, merged.mMax, merged.mReason, merged.mClass,
                   merged.mFirstSerial);
}

void noteLightVolume(LLVOVolume* volume, U16 reason, U64 min_face_op)
{
    if (!gProbeDirtyRecording)
    {
        return;
    }
    ++sRaw;
    fanoutLight(volume, reason, min_face_op);
}

void noteGlobal(U16 reason, U8 cls)
{
    if (!gProbeDirtyRecording)
    {
        return;
    }
    ++sRaw;
    const F32 zero[3] = { 0.f, 0.f, 0.f };
    sDiscrete.note(&sGlobalKeys[cls & 15], zero, zero, reason, cls, sSerial);
}

// H6: fan the texture change out to every face / gobo user.
void noteTextureArrival(LLViewerFetchedTexture* texture)
{
    if (!gProbeDirtyRecording || !texture)
    {
        return;
    }
    const S32 d = texture->getDiscardLevel();
    if (d < 0)
    {
        return;
    }
    const S32 dim = std::max(texture->getFullWidth(), texture->getFullHeight());
    const S32 dp = sched::probeDiscardFloor(dim, sProbeResolution);
    sched::TexNote note;
    note.mNotedDiscard = texture->mProbeNotedDiscard;
    note.mBlurred = texture->mProbeBlurred;
    note.mBlurStamp = texture->mProbeBlurStamp;
    const sched::TexVerdict verdict = sched::texArrival(note, d, dp);
    texture->mProbeNotedDiscard = note.mNotedDiscard;
    texture->mProbeBlurred = note.mBlurred;
    texture->mProbeBlurStamp = note.mBlurStamp;
    if (!verdict.mNotify)
    {
        return;
    }

    LL_PROFILE_ZONE_NAMED_CATEGORY_TEXTURE("probe dirty tex fanout");
    S32 total = 0;
    for (U32 ch = 0; ch < LLRender::NUM_TEXTURE_CHANNELS; ++ch)
    {
        const LLViewerTexture::ll_face_list_t* faces = texture->getFaceList(ch);
        for (S32 i = 0; i < texture->getNumFaces(ch); ++i)
        {
            LLFace* face = (*faces)[static_cast<size_t>(i)];
            if (++total > kMaxFanout)
            {
                // Too many users: one global texture event instead.
                const F32 zero[3] = { 0.f, 0.f, 0.f };
                ++sRaw;
                sDiscrete.note(&sGlobalKeys[sched::C_GLOBAL & 15], zero, zero, sched::R_TEX,
                               static_cast<U8>(sched::C_GLOBAL), sSerial, verdict.mMinFaceOp);
                return;
            }
            if (!face || !face->getDrawable())
            {
                continue;
            }
            // Face notes carry the blur stamp as their face-op restriction.
            noteCommon(face->getDrawable(), nullptr, sched::R_TEX, sched::Motion::NONE, false,
                       verdict.mMinFaceOp);
        }
    }
    const LLViewerTexture::ll_volume_list_t* volumes = texture->getVolumeList(LLRender::LIGHT_TEX);
    for (S32 i = 0; i < texture->getNumVolumes(LLRender::LIGHT_TEX); ++i)
    {
        if (++total > kMaxFanout)
        {
            const F32 zero[3] = { 0.f, 0.f, 0.f };
            ++sRaw;
            sDiscrete.note(&sGlobalKeys[sched::C_GLOBAL & 15], zero, zero, sched::R_TEX,
                           static_cast<U8>(sched::C_GLOBAL), sSerial, verdict.mMinFaceOp);
            return;
        }
        ++sRaw;
        fanoutLight((*volumes)[static_cast<size_t>(i)], sched::R_TEX, verdict.mMinFaceOp);
    }
}

void noteTextureDownscale(LLViewerFetchedTexture* texture)
{
    if (!gProbeDirtyRecording || !texture)
    {
        return;
    }
    const S32 d = texture->getDiscardLevel();
    if (d < 0)
    {
        return;
    }
    const S32 dim = std::max(texture->getFullWidth(), texture->getFullHeight());
    const S32 dp = sched::probeDiscardFloor(dim, sProbeResolution);
    sched::TexNote note;
    note.mNotedDiscard = texture->mProbeNotedDiscard;
    note.mBlurred = texture->mProbeBlurred;
    note.mBlurStamp = texture->mProbeBlurStamp;
    if (sched::texDownscaleStartsBlur(note, d, dp))
    {
        // The stamp is an op: a probe face rendered before this point has a
        // smaller op and never saw the blurred texture, even in the same frame.
        sched::texDownscale(note, d, dp, nextOp());
        texture->mProbeBlurred = note.mBlurred;
        texture->mProbeBlurStamp = note.mBlurStamp;
    }
}

// The same object came back from the region's object cache after a cache cull:
// recognised by id, not by pointer.
void noteCacheCreated(const LLUUID& id)
{
    if (!gProbeDirtyRecording || sProvenance.culledEmpty())
    {
        return;
    }
    sProvenance.noteCreated(sched::idTagOf(id));
}

// A real server update, a changed / replaced cache entry or a cache miss followed by
// a full update arrived: the object's state is authoritative again, and a stale
// culled marker must not outlive it.
void noteServerUpdate(const LLUUID& id)
{
    if (!gProbeDirtyRecording)
    {
        return;
    }
    sProvenance.noteAuthoritative(sched::idTagOf(id));
}

bool isCacheBorn(const LLUUID& id)
{
    return gProbeDirtyRecording && sProvenance.isBorn(sched::idTagOf(id));
}

bool wasCulled(const LLUUID& id)
{
    return gProbeDirtyRecording && sProvenance.wasCulled(sched::idTagOf(id));
}
void shiftQueued(const LLVector4a& offset)
{
    if (!gProbeDirtyRecording)
    {
        return;
    }
    const F32* o = offset.getF32ptr();
    const F32 off[3] = { o[0], o[1], o[2] };
    sDiscrete.shift(off);
    sMotion.shift(off);
}

void markOverflow()
{
    if (gProbeDirtyRecording)
    {
        sDiscrete.markOverflow();
    }
}

void drain(Drain& out, F64 now)
{
    out.mEvents.clear();
    out.mOverflow = false;
    if (gProbeDirtyRecording)
    {
        out.mOverflow = sDiscrete.take(out.mEvents);
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("probe sched debounce");
        sMotion.drain(now, out.mEvents);
    }
    for (S32 i = 0; i < TAG_COUNT; ++i)
    {
        out.mDropped[i] = sDropped[i];
        sDropped[i] = 0;
    }
    out.mDroppedDyn = sDroppedDyn;
    out.mNoopMove = sNoopMove;
    out.mRebal = sRebal;
    out.mRaw = sRaw;
    out.mMotionNotes = sMotionNotes;
    sDroppedDyn = 0;
    sNoopMove = 0;
    sRebal = 0;
    sRaw = 0;
    sMotionNotes = 0;
    const sched::DebounceMap::Stats& st = sMotion.stats();
    out.mSettles = st.mSettles;
    out.mEarlySettles = st.mEarlySettles;
    out.mBulk = st.mBulkNotes;
    out.mBulkDeadlines = st.mBulkDeadlines;
    out.mPendingMotion = sMotion.pendingCount();
    sMotion.resetStats();
    sInsertedThisFrame.clear();
    sRemovedThisFrame.clear();
}

} // namespace ALProbeDirty
