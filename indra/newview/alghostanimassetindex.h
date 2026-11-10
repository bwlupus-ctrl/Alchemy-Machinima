/**
 * @file alghostanimassetindex.h
 * @brief Session-local, read-only inventory animation metadata index.
 */
#ifndef AL_GHOSTANIMASSETINDEX_H
#define AL_GHOSTANIMASSETINDEX_H

#include "alghostanimdrop.h"
#include "llsingleton.h"
#include "lluuid.h"

class LLViewerInventoryItem;

#include <map>
#include <string>
#include <vector>

class ALGhostAnimAssetIndex final : public LLSingleton<ALGhostAnimAssetIndex>
{
    LLSINGLETON(ALGhostAnimAssetIndex);
public:
    enum EAvailability { AVAILABLE_PENDING, AVAILABLE_READY, AVAILABLE_UNAVAILABLE };

    struct Entry
    {
        LLUUID mAssetId;
        LLUUID mItemId;
        std::string mName;
        // TPV policy 2.a.i: the asset id of a non-full-perm item may be used to
        // play but never shown. true = full perm (modify+copy+transfer).
        bool mExposable = true;
        EAvailability mAvailability = AVAILABLE_PENDING;
        F32 mDuration = 0.f;
        bool mLoop = false;
        F32 mLoopIn = 0.f;
        F32 mLoopOut = 0.f;
        F32 mEaseIn = 0.f;
        F32 mEaseOut = 0.f;
        S32 mPriority = 0;
        std::vector<std::string> mJoints;
        S32 mHandPose = 0;
        std::string mHandPoseName;
        LLUUID mEmoteId;
        std::string mEmoteName;
    };

    const std::vector<Entry>& refreshInventory();
    const Entry* find(const LLUUID& asset_id) const;
    U64 metadataRevision() const { return mMetadataRevision; }
    // Names already retained with requested metadata, else names learned from
    // the agent's own worn attachments (noteSessionName). Never walks inventory.
    const std::string& sessionName(const LLUUID& asset_id) const
    {
        static const std::string empty;
        const auto found = mMetadata.find(asset_id);
        if (found != mMetadata.end() && !found->second.mName.empty())
        {
            return found->second.mName;
        }
        const auto learned = mSessionNames.find(asset_id);
        return learned != mSessionNames.end() ? learned->second.mName : empty;
    }
    // Record a session-only display name for an asset (worn-attachment task
    // inventory). Bumps metadataRevision() when the shown name changes. Never
    // requests asset data and never blocks a later requestMetadata().
    void noteSessionName(const LLUUID& asset_id, const std::string& name, bool exposable);
    // Never-expose rule (TPV policy 2.a.i), a monotonic session ledger: true
    // when `asset_id` is backed by any non-full-perm item seen this session
    // (inventory scan, a dropped / menu / linked item, worn-attachment
    // contents). Every entry point records the item BEFORE playing or
    // formatting, so an animation acquired after the scan is covered too.
    bool isAssetIdHidden(const LLUUID& asset_id) const { return mRestrictions.hidden(asset_id); }
    void noteRestriction(const LLUUID& asset_id, bool exposable);
    void noteItemRestriction(const LLViewerInventoryItem* item); // links resolved
    U64 restrictionRevision() const { return mRestrictions.revision(); }
    const ALGhostAnimDrop::RestrictionLedger& restrictions() const { return mRestrictions; }
    void requestMetadata(const LLUUID& asset_id);

private:
    static void onAssetLoaded(const LLUUID& asset_id, LLAssetType::EType type,
                              void* user_data, S32 status, LLExtStat ext_status);
    void finishMetadata(const LLUUID& asset_id, S32 status);

    struct SessionName
    {
        std::string mName;
    };

    std::vector<Entry> mEntries;
    std::map<LLUUID, Entry> mMetadata;
    std::map<LLUUID, SessionName> mSessionNames;
    ALGhostAnimDrop::RestrictionLedger mRestrictions;
    U64 mMetadataRevision = 0;
};

#endif
