/**
 * @file alvirtuallights.h
 * @brief Runtime for session-only, client-only "virtual lights".
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * A virtual light is a simple light "prim" only this viewer sees: placed by clicking a spot in
 * the world, at most eight, never sent to the simulator, never saved to settings. To the
 * renderer it is an ordinary WORLD light (a local-only invisible volume of kind
 * LOCAL_OBJECT_VIRTUAL_LIGHT); every Practicals control reaches it through the same LLPipeline
 * entry points a real projector uses. Director scenes save it; login and crash recovery do not
 * bring it back; a real region change removes it.
 *
 * The marker (a small diamond, an aim line and a tip handle) is drawn only in the guide
 * composite, never into the live output.
 */
#ifndef AL_VIRTUAL_LIGHTS_H
#define AL_VIRTUAL_LIGHTS_H

#include "alprojectortrackmath.h"
#include "alvirtuallightsmodel.h"
#include "llpointer.h"
#include "llsd.h"
#include "lluuid.h"
#include "v3math.h"

#include <functional>
#include <string>
#include <vector>

class LLViewerObject;
class LLViewerRegion;
class LLVOVolume;

class ALVirtualLights
{
public:
    using Light = ALVirtualLightsModel::Light;

    static ALVirtualLights& instance();
    // Cheap, never constructs the singleton: the feature-off path of every hook.
    static bool anyPlaced();
    static bool wantsGuides();
    static void drawGuides();

    // ---- lifecycle ---------------------------------------------------------------------
    void tick(F32 dt);
    // Removes every light (objects, VCam bindings, pipeline flags). Idempotent, dead-safe.
    void clearAll(const char* reason);
    LLSD sceneData();
    // `recovery` = this is crash/login recovery: L7, lights are NOT brought back (and the live
    // ones are left alone). Otherwise the block replaces the live lights.
    void applySceneData(const LLSD& block, bool recovery);

    // ---- queries -----------------------------------------------------------------------
    S32  count() const { return static_cast<S32>(mEntries.size()); }
    U64  revision() const { return mRevision; }        // any data change (list, fields)
    U64  sceneEpoch() const { return mEpoch; }         // bumped by clear / scene load: undo guard
    U64  listRevision() const { return mListRevision; } // list shape / on / type only (not position drags)
    std::vector<LLUUID> keys() const;
    const Light* find(const LLUUID& key) const;
    bool isVirtualObject(const LLUUID& object_id) const;
    LLUUID keyForObject(const LLUUID& object_id) const;
    LLUUID objectIdFor(const LLUUID& key) const;
    LLVOVolume* volumeFor(const LLUUID& key) const;
    std::string label(const LLUUID& key) const;        // "Virtual light 2"
    LLVector3 agentPositionOf(const LLUUID& key) const;

    // ---- commands ----------------------------------------------------------------------
    // Click-to-place: hit and normal are region-local. Returns the new key (null + reason on refusal).
    LLUUID place(const F32 hit[3], const F32 normal[3], std::string* reason);
    // Same, from a global pick point (region membership checked here).
    LLUUID placeAtGlobal(const LLVector3d& point_global, const LLVector3& normal, std::string* reason);
    bool remove(const LLUUID& key);
    // The one write path for light fields: `change` mutates a copy, which is sanitised and stored.
    bool edit(const LLUUID& key, const std::function<void(Light&)>& change);
    bool setAimObject(const LLUUID& key, const LLUUID& object_id);
    // VCam feed: null capture = Off. Reason filled on refusal.
    // `settings` (a saved Prism projector settings block) is applied after the bind: a recreated light keeps
    // its authored fit / exposure / lamp power / flips / effects / look.
    bool setVCam(const LLUUID& key, const LLUUID& capture_id, std::string* reason, const LLSD& settings = LLSD());
    LLUUID vcamOf(const LLUUID& key) const;            // the live registry binding, not the request

    // ---- Practicals / Track (virtual backend) ------------------------------------------
    void notePracticalsEdit(const LLUUID& key);
    void configureTrack(const LLUUID& key, const LLUUID& subject, bool has_subject,
                        S32 joint, S32 mode, F32 smoothing);
    void enableTrack(const LLUUID& key, bool enabled);
    void captureTrack(const LLUUID& key);
    const std::string& trackStatus(const LLUUID& key) const;

    // ---- focus (the light the Practicals page edits) -------------------------------------
    const LLUUID& focusKey() const { return mFocusKey; }
    void setFocus(const LLUUID& key);
    void clearFocus();
    // The Practicals page (or the edit tool) calls this each frame it is visible.
    void noteEditorVisible();
    // The edit tool is active: the marker shows even when the Practicals page is not on screen.
    void setEditTool(bool active);
    // Synchronous release of the edit tool AND the guide ownership: Practicals page hide, Director hide /
    // minimise / close, teardown. After it returns no frame can draw the marker.
    static void releaseEditor();

    // ---- undo / scene snapshots ----------------------------------------------------------
    LLSD capture();                                    // array of light maps (Practicals synced)
    bool restore(const LLSD& snapshot, std::string& reason);
    // Records one undo step on the Practicals page (no-op when before == after).
    static void recordUndo(const LLSD& before, const std::string& label);
    // The Practicals card coalesces a burst of edits into one undo step; a tool that is about to
    // snapshot "before" asks it to close that step first so the two never interleave.
    static void setPendingFlusher(std::function<void()> flusher);
    static void flushPending();

    // Clone replacement: a Track subject / aim object that named the retired actor follows the new one.
    void rewriteActorReferences(const LLUUID& old_id, const LLUUID& new_id);

    // ---- marker picking (edit tool) ------------------------------------------------------
    struct Pick { LLUUID mKey; S32 mHandle; LLVector3 mAgentPos; };   // handle 0 = body, 1 = aim tip
    void collectPicks(std::vector<Pick>& out) const;

private:
    ALVirtualLights();
    struct Entry
    {
        Light mLight;
        LLPointer<LLVOVolume> mObject;
        LLViewerRegion* mRegion = nullptr;
        bool mDirty = true;
        LLQuaternion mRot;
        ALProjectorTrackMath::Pose mCurrent, mOffset;
        bool mOffsetCaptured = false;
        bool mCapturePending = false;
        F64 mMissingSince = 0.;
        std::string mTrackStatus = "Track off";
    };
    Entry* findEntry(const LLUUID& key);
    const Entry* findEntry(const LLUUID& key) const;
    Entry* findByObject(const LLUUID& object_id);
    bool createObject(Entry& entry);
    void destroyEntry(Entry& entry);
    void applyEntry(Entry& entry);
    bool verifyApplied(const Entry& entry, const char* event);
    void applyPracticals(Entry& entry);
    void syncPracticals(Entry& entry);
    void syncVCam(Entry& entry);
    void unbindVCam(const LLViewerObject* object);
    void updateTracking(Entry& entry, F32 dt, F64 now);
    void bump() { ++mRevision; }
    void connectSelection();
    static void logEvent(const char* event, const std::string& detail, const char* verdict);

    std::vector<Entry> mEntries;
    LLUUID mFocusKey;
    LLUUID mLastPrimaryId;   // the primary selection the focus rule last saw
    U64 mRevision = 1;
    U64 mListRevision = 1;
    U64 mEpoch = 1;
    bool mSelectionConnected = false;
    bool mRestoring = false;
};

#endif // AL_VIRTUAL_LIGHTS_H
