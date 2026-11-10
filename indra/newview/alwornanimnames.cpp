/**
 * @file alwornanimnames.cpp
 * @brief Session-only animation names learned from the agent's own worn
 *        attachments. See alwornanimnames.h.
 */
#include "llviewerprecompiledheaders.h"

#include "alwornanimnames.h"

#include "alghostanimassetindex.h"
#include "llagent.h"
#include "llcallbacklist.h"
#include "llframetimer.h"
#include "llinventory.h"
#include "llpermissionsflags.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"

ALWornAnimNames::ALWornAnimNames()
{
}

ALWornAnimNames::~ALWornAnimNames()
{
    // Objects that outlive this singleton drop their callback entry on their
    // own (LLViewerObject::clearInventoryListeners at destruction); detach
    // them here so no dangling listener pointer survives a late shutdown.
    for (const auto& tracked : mTracked)
    {
        if (LLViewerObject* object = gObjectList.findObject(tracked.first))
        {
            object->removeInventoryListener(this);
        }
    }
    // The base class removes mListenerVObject, which this class never sets.
    clearVOInventoryListener();
}

void ALWornAnimNames::onAttach(LLViewerObject* object)
{
    // Own attachments only: LLVOAvatarSelf calls this, and the owner check
    // keeps a mis-routed call from ever touching someone else's object.
    if (!object || object->isDead() || object->isLocalOnly() ||
        !object->permYouOwner())
    {
        return;
    }
    // Animations driving an AO usually live in the HUD root, sometimes in a
    // child prim; body attachments keep theirs in the root. Child prims of a
    // non-HUD attachment are not fetched (a mesh body can have hundreds).
    const bool hud = object->isHUDAttachment();
    track(object, hud);
    if (hud)
    {
        for (const LLPointer<LLViewerObject>& child : object->getChildren())
        {
            trackHudChild(object, child.get());
        }
    }
}

void ALWornAnimNames::onDetach(LLViewerObject* object)
{
    if (!object) return;
    forget(object);
    for (const LLPointer<LLViewerObject>& child : object->getChildren())
    {
        forget(child.get());
    }
}

void ALWornAnimNames::onInventoryDirty(LLViewerObject* object)
{
    if (!object || mTracked.empty() || mTracked.find(object->getID()) == mTracked.end())
    {
        return;
    }
    enqueue(object->getID()); // deduplicated, throttled refetch
}

void ALWornAnimNames::onChildAdded(LLViewerObject* parent, LLViewerObject* child)
{
    if (!parent || !child || mTracked.empty()) return;
    const auto tracked = mTracked.find(parent->getID());
    if (tracked == mTracked.end() || !tracked->second.mHudRoot) return;
    trackHudChild(parent, child);
}

void ALWornAnimNames::onChildRemoved(LLViewerObject* parent, LLViewerObject* child)
{
    if (!parent || !child || mTracked.empty()) return;
    const auto tracked = mTracked.find(child->getID());
    if (tracked == mTracked.end() || tracked->second.mRoot != parent->getID()) return;
    forget(child); // listener + queued work go; the root's capacity comes back
}

void ALWornAnimNames::onObjectGone(LLViewerObject* object)
{
    if (!object || mTracked.empty()) return;
    forget(object);
}

void ALWornAnimNames::track(LLViewerObject* object, bool hud_root)
{
    const LLUUID& id = object->getID();
    auto tracked = mTracked.find(id);
    if (tracked == mTracked.end())
    {
        tracked = mTracked.emplace(id, Tracked()).first;
        object->registerInventoryListener(this, nullptr);
    }
    tracked->second.mHudRoot = tracked->second.mHudRoot || hud_root;
    enqueue(id);
}

void ALWornAnimNames::trackHudChild(LLViewerObject* root, LLViewerObject* child)
{
    auto tracked = mTracked.find(root->getID());
    if (tracked == mTracked.end() || tracked->second.mChildren >= MAX_HUD_CHILDREN ||
        !child || child->isDead() || !child->permYouOwner() ||
        mTracked.find(child->getID()) != mTracked.end())
    {
        return;
    }
    ++tracked->second.mChildren;
    track(child);
    mTracked[child->getID()].mRoot = root->getID();
}

void ALWornAnimNames::forget(LLViewerObject* object)
{
    if (!object) return;
    const auto tracked = mTracked.find(object->getID());
    if (tracked == mTracked.end()) return;
    object->removeInventoryListener(this);
    if (tracked->second.mRoot.notNull())
    {
        // Membership is per root: an unlinked or dead child returns its slot,
        // so a replacement prim after many link/unlink cycles is still found.
        const auto owner = mTracked.find(tracked->second.mRoot);
        if (owner != mTracked.end() && owner->second.mChildren > 0)
        {
            --owner->second.mChildren;
        }
    }
    mTracked.erase(tracked);
    for (auto queued = mQueue.begin(); queued != mQueue.end(); )
    {
        queued = *queued == object->getID() ? mQueue.erase(queued) : queued + 1;
    }
}

void ALWornAnimNames::enqueue(const LLUUID& object_id)
{
    for (const LLUUID& queued : mQueue)
    {
        if (queued == object_id) return;
    }
    mQueue.push_back(object_id);
    // Throttle: the first request goes out as soon as the interval since the
    // previous one has elapsed; the rest follow one per interval.
    const F64 since = LLFrameTimer::getTotalSeconds() - mLastRequest;
    scheduleDrain(since >= REQUEST_INTERVAL ? 0.f : (F32)(REQUEST_INTERVAL - since));
}

void ALWornAnimNames::scheduleDrain(F32 seconds)
{
    if (mDrainScheduled) return;
    mDrainScheduled = true;
    // Event-driven: one idle callback per request, nothing per frame between.
    doAfterInterval([]() { ALWornAnimNames::instance().drain(); }, llmax(seconds, 0.f));
}

void ALWornAnimNames::drain()
{
    mDrainScheduled = false;
    if (mQueue.empty()) return;
    if (gAgent.getTeleportState() != LLAgent::TELEPORT_NONE)
    {
        scheduleDrain(TELEPORT_RETRY);   // never request mid-teleport
        return;
    }
    const LLUUID id = mQueue.front();
    mQueue.pop_front();
    LLViewerObject* object = gObjectList.findObject(id);
    if (object && !object->isDead() && mTracked.find(id) != mTracked.end() &&
        object->permYouOwner())
    {
        mLastRequest = LLFrameTimer::getTotalSeconds();
        // Serves a cached copy synchronously when present, else one fetch;
        // duplicate in-flight requests are dropped by the object itself.
        object->requestInventory();
    }
    if (!mQueue.empty())
    {
        scheduleDrain(REQUEST_INTERVAL);
    }
}

void ALWornAnimNames::inventoryChanged(LLViewerObject* object,
                                       LLInventoryObject::object_list_t* inventory,
                                       S32 serial_num, void*)
{
    if (!object || !inventory) return;
    const auto tracked = mTracked.find(object->getID());
    if (tracked == mTracked.end()) return;
    if (tracked->second.mParsedSerial == serial_num) return; // same serial: nothing new
    tracked->second.mParsedSerial = serial_num;

    ALGhostAnimDrop::WornNameTally tally;
    ALGhostAnimAssetIndex& index = ALGhostAnimAssetIndex::instance();
    for (const LLPointer<LLInventoryObject>& entry : *inventory)
    {
        const LLInventoryItem* item =
            entry.notNull() ? dynamic_cast<const LLInventoryItem*>(entry.get()) : nullptr;
        if (!item) continue;
        const bool is_animation = item->getType() == LLAssetType::AT_ANIMATION;
        const LLUUID& asset_id = item->getAssetUUID();
        if (!ALGhostAnimDrop::acceptWornEntry(is_animation, asset_id.notNull(),
                                              item->getName(), tally))
        {
            continue;
        }
        const U32 owner_mask = item->getPermissions().getMaskOwner();
        const bool exposable =
            (owner_mask & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED;
        index.noteSessionName(asset_id, item->getName(), exposable);
    }
    mTally.mNamed += tally.mNamed;
    mTally.mMasked += tally.mMasked;
    if (tally.mNamed || tally.mMasked)
    {
        // Verdict for the in-world test, counts only (never an id or a name):
        // "masked" entries are animations whose asset id the server nulled, so
        // their names cannot be matched to anything playing.
        LL_INFOS("GhostAnimNames") << "Worn attachment task inventory: "
            << tally.mNamed << " animation name(s) recorded, "
            << tally.mMasked << " masked (null asset id)" << LL_ENDL;
    }
}
