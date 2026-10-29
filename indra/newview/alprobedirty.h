/**
 * @file alprobedirty.h
 * @brief [ProbeOnDemand] Read-only dirty-event recorder for on-demand
 *        reflection probe updates.
 *
 * Hooks in the drawable / spatial / texture code call the note functions below
 * ONLY while recording() is true (RenderProbeOnDemand ON); with it OFF every
 * hook costs one branch on gProbeDirtyRecording and nothing else runs. Hooks are
 * read-only: they never write render, drawable, group or pipeline state.
 * Main thread only. The reflection manager drains the queue once per update().
 *
 * See doc/PROBE_UPDATE_ON_DEMAND_BRIEF_V6.md section 4.2 / 4.4 / 4.10.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_PROBE_DIRTY_H
#define AL_PROBE_DIRTY_H

#include "stdtypes.h"
#include "alprobeschedule.h"

#include <vector>

class LLDrawable;
class LLVOVolume;
class LLViewerFetchedTexture;
class LLVector3;
class LLVector4a;
class LLQuaternion;

// Defined ONCE in alprobedirty.cpp.
extern bool gProbeDirtyRecording;
// Current ScopedTag (ALProbeDirty::Tag), TAG_NONE outside any tag scope.
extern U8 gProbeDirtyTag;

namespace ALProbeDirty
{

inline bool recording()
{
    return gProbeDirtyRecording;
}

// Turning recording OFF discards every queued event and debounce state.
void setRecording(bool on);
// Discards every queued event and debounce state without changing the recording
// state (probe schedule reset).
void reset();
void setProbeResolution(U32 resolution);
// Called once per manager frame (flush step a): the flush serial, the frame
// time and the frame interval (smoothed internally).
void setFrame(U64 serial, F64 now, F32 frame_dt);

// Monotonic op counter: orders probe-face captures against texture downscales
// (a face counts as exposed to a blur iff its op is greater than the blur stamp).
U64 nextOp();
U64 lastOp();

enum Tag : U8
{
    TAG_NONE,
    TAG_LOD_VOLUME,
    TAG_LOD_MESH,
    TAG_LOD_TERRAIN,
    TAG_LOD_TREE,
    TAG_LOD_GRASS,
    TAG_TEXANIM_TOGGLE,
    TAG_REGION_SHIFT,    // region-crossing coordinate shift of static drawables
    TAG_VOCACHE,         // object-cache culling kills (the camera turned)
    TAG_COUNT
};

// The tag currently in effect (TAG_NONE outside any scope). Lets a conditional
// scope keep an enclosing tag: ScopedTag t(cond ? TAG_LOD_MESH : currentTag()).
inline Tag currentTag()
{
    return static_cast<Tag>(gProbeDirtyTag);
}

// Notes made inside a tag scope are camera/LOD-driven rebuild noise and are
// dropped (and counted per tag). Nests; restores the previous tag.
struct ScopedTag
{
    explicit ScopedTag(Tag tag) : mActive(gProbeDirtyRecording), mPrev(gProbeDirtyTag)
    {
        if (mActive)
        {
            gProbeDirtyTag = static_cast<U8>(tag);
        }
    }
    ~ScopedTag()
    {
        if (mActive)
        {
            gProbeDirtyTag = mPrev;
        }
    }
    ScopedTag(const ScopedTag&) = delete;
    ScopedTag& operator=(const ScopedTag&) = delete;

private:
    bool mActive;
    U8 mPrev;
};

// H1b: octree membership notes (H2) made inside a move are motion, not
// creation/deletion.
void enterMove();
void leaveMove();
struct ScopedMove
{
    explicit ScopedMove(bool active) : mActive(active && gProbeDirtyRecording)
    {
        if (mActive)
        {
            enterMove();
        }
    }
    ~ScopedMove()
    {
        if (mActive)
        {
            leaveMove();
        }
    }
    ScopedMove(const ScopedMove&) = delete;
    ScopedMove& operator=(const ScopedMove&) = delete;

private:
    bool mActive;
};

// World bounds of a drawable (a drawable inside a bridge partition uses the
// bridge's world extents; no group / non-finite -> a sphere around its agent
// position). out[0] = min, out[1] = max.
void snapshotBounds(LLDrawable* drawable, LLVector4a out[2]);

// Discrete note (Motion::NONE) or motion note (debounced). `reason` is an
// ALProbeSched::Reason mask.
void noteDrawable(LLDrawable* drawable, U16 reason, ALProbeSched::Motion motion);
// As above over the union of the drawable's current bounds and `old_mn_mx`
// (2 vectors: min, max). Always notes.
void noteDrawableBounds(LLDrawable* drawable, const LLVector4a* old_mn_mx, U16 reason,
                        ALProbeSched::Motion motion);
// As above but only when the union is larger than the old bounds by > 1e-4 m.
// Returns whether a note was made.
bool noteDrawableBoundsIfChanged(LLDrawable* drawable, const LLVector4a* old_mn_mx,
                                 U16 reason, ALProbeSched::Motion motion);
void noteNoopMove();
// H1a: a transform delta of an active drawable (thresholds: position 1e-4 m,
// rotation 1 - |dot| 1e-7, scale 1e-4).
// The old transform arrives as raw arrays (position xyz, rotation xyzw, scale xyz)
// so the caller's snapshot costs nothing when recording is off.
void noteXformDelta(LLDrawable* drawable, const F32 old_pos[3], const F32 old_rot[4],
                    const F32 old_scale[3], const LLVector4a* old_extents,
                    const LLVector3& new_pos, const LLQuaternion& new_rot,
                    const LLVector3& new_scale);
// H2: octree membership (creation / deletion outside a move, motion inside one).
void noteInsertion(LLDrawable* drawable);
void noteRemoval(LLDrawable* drawable);
// A light prim: sphere(pos, radius * 1.5), discrete C_LIGHT.
void noteLightVolume(LLVOVolume* volume, U16 reason, U64 min_face_op);
void noteGlobal(U16 reason, U8 cls);
// H6: a texture arrived / refined / was re-sharpened. Fans out to every face
// and gobo user. DS: a VRAM downscale (records a blur stamp, never notifies).
void noteTextureArrival(LLViewerFetchedTexture* texture);
void noteTextureDownscale(LLViewerFetchedTexture* texture);
// Object-cache culling churn. Objects the region's VO cache culls (removals made
// inside a ScopedTag(TAG_VOCACHE)) are remembered by id; when the same object is
// re-created from the cache (noteCacheCreated) it is "cache-born": its events
// (insertion, first builds, first mesh / texture arrivals) are dropped until a
// REAL server update (noteServerUpdate) or a real kill arrives. First-time cache
// creations, server updates and real kills stay events.
void noteCacheCreated(const LLUUID& id);
void noteServerUpdate(const LLUUID& id);
bool isCacheBorn(const LLUUID& id);
bool wasCulled(const LLUUID& id);
// Region crossing: queued events and debounce bounds move with the world.
void shiftQueued(const LLVector4a& offset);
void markOverflow();

struct Drain
{
    std::vector<ALProbeSched::Event> mEvents;
    bool mOverflow = false;
    U32 mDropped[TAG_COUNT] = {};
    U32 mDroppedDyn = 0;
    U32 mNoopMove = 0;
    U32 mRebal = 0;
    U32 mRaw = 0;
    U32 mMotionNotes = 0;
    U32 mSettles = 0;
    U32 mEarlySettles = 0;
    U32 mBulk = 0;
    U32 mBulkDeadlines = 0;
    U32 mPendingMotion = 0;
};

// Moves everything queued this frame into `out` (clearing it first), including
// the settle events that are due at `now`.
void drain(Drain& out, F64 now);

} // namespace ALProbeDirty

#endif // AL_PROBE_DIRTY_H
