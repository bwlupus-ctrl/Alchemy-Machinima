/**
 * @file alwornanimnames.h
 * @brief Session-only animation names learned from the agent's OWN worn
 *        attachments (including HUDs), fed to ALGhostAnimAssetIndex so the
 *        Ghost Studio "Source playing" list shows "<name> (xxxxxxxx)" instead
 *        of a bare short UUID.
 *
 * Event driven only: LLVOAvatarSelf::attachObject() / detachObject() call
 * onAttach() / onDetach(); task inventory arrives through the
 * LLVOInventoryListener callback (which also fires when any other viewer code
 * refreshes that object's inventory, so a changed inventory serial is picked
 * up without polling). Requests are throttled to a few per second and never
 * sent while a teleport is in progress. Never touches other residents'
 * objects. No per-frame work.
 *
 * TPV policy 2.a.i: asset ids are keyed internally only; this module logs
 * counts, never ids or names.
 */
#ifndef AL_WORNANIMNAMES_H
#define AL_WORNANIMNAMES_H

#include "llsingleton.h"
#include "lluuid.h"
#include "llvoinventorylistener.h"
#include "alghostanimdrop.h"

#include <deque>
#include <map>

class LLViewerObject;

class ALWornAnimNames final : public LLSingleton<ALWornAnimNames>,
                              public LLVOInventoryListener
{
    LLSINGLETON(ALWornAnimNames);
    ~ALWornAnimNames();
public:
    // Called by LLVOAvatarSelf for the agent's own attachments only.
    void onAttach(LLViewerObject* object);
    void onDetach(LLViewerObject* object);
    // LLViewerObject lifecycle hooks (cheap no-ops for untracked objects):
    // an invalidated task inventory re-enters the throttle queue; a prim
    // linked under a tracked HUD root later is tracked too; a dead object is
    // forgotten (teleport / region crossing recreate attachments, which then
    // re-enter through onAttach).
    void onInventoryDirty(LLViewerObject* object);
    void onChildAdded(LLViewerObject* parent, LLViewerObject* child);
    void onChildRemoved(LLViewerObject* parent, LLViewerObject* child); // unlink
    void onObjectGone(LLViewerObject* object);

    // LLVOInventoryListener
    void inventoryChanged(LLViewerObject* object,
                          LLInventoryObject::object_list_t* inventory,
                          S32 serial_num, void* user_data) override;

    const ALGhostAnimDrop::WornNameTally& tally() const { return mTally; }

    static constexpr F32 REQUEST_INTERVAL = 0.25f;  // at most 4 requests / s
    static constexpr F32 TELEPORT_RETRY = 2.f;
    static constexpr S32 MAX_HUD_CHILDREN = 32;

private:
    void track(LLViewerObject* object, bool hud_root = false);
    void trackHudChild(LLViewerObject* root, LLViewerObject* child);
    void forget(LLViewerObject* object);
    void enqueue(const LLUUID& object_id);
    void scheduleDrain(F32 seconds);
    void drain();

    struct Tracked
    {
        S32 mParsedSerial = -1;   // last task-inventory serial folded into names
        bool mHudRoot = false;    // children linked later are tracked too
        S32 mChildren = 0;        // HUD children CURRENTLY tracked under this root
        LLUUID mRoot;             // for a HUD child: its root (capacity returns on forget)
    };
    std::map<LLUUID, Tracked> mTracked;   // object id -> state (own attachments)
    std::deque<LLUUID> mQueue;            // objects awaiting a throttled request
    bool mDrainScheduled = false;
    F64 mLastRequest = 0.0;
    ALGhostAnimDrop::WornNameTally mTally;
};

#endif
