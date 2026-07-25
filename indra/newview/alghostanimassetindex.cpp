/**
 * @file alghostanimassetindex.cpp
 * @brief Session-local, read-only inventory animation metadata index.
 */
#include "llviewerprecompiledheaders.h"

#include "alghostanimassetindex.h"

#include "llagent.h"
#include "lldatapacker.h"
#include "llfilesystem.h"
#include "llhandmotion.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "llkeyframemotion.h"
#include "llviewerassetstorage.h"
#include "llviewerinventory.h"

#include <algorithm>

ALGhostAnimAssetIndex::ALGhostAnimAssetIndex()
{
}

const std::vector<ALGhostAnimAssetIndex::Entry>&
ALGhostAnimAssetIndex::refreshInventory()
{
    LLInventoryModel::cat_array_t cats;
    LLInventoryModel::item_array_t items;
    LLIsType animations(LLAssetType::AT_ANIMATION);
    gInventory.collectDescendentsIf(gInventory.getRootFolderID(), cats, items,
                                    LLInventoryModel::EXCLUDE_TRASH, animations);

    std::vector<Entry> fresh;
    fresh.reserve(items.size());
    for (const LLPointer<LLViewerInventoryItem>& item_ptr : items)
    {
        LLViewerInventoryItem* item = item_ptr.get();
        if (!item || item->getPermissions().getOwner() != gAgent.getID() ||
            item->getAssetUUID().isNull())
        {
            continue;
        }
        Entry entry;
        entry.mAssetId = item->getAssetUUID();
        entry.mItemId = item->getUUID();
        entry.mName = item->getName();
        auto cached = mMetadata.find(entry.mAssetId);
        if (cached != mMetadata.end())
        {
            const LLUUID item_id = entry.mItemId;
            const std::string name = entry.mName;
            entry = cached->second;
            entry.mItemId = item_id;
            entry.mName = name;
        }
        fresh.push_back(entry);
    }
    std::sort(fresh.begin(), fresh.end(),
              [](const Entry& a, const Entry& b)
              {
                  return LLStringUtil::compareInsensitive(a.mName, b.mName) < 0;
              });
    mEntries.swap(fresh);
    return mEntries;
}

const ALGhostAnimAssetIndex::Entry*
ALGhostAnimAssetIndex::find(const LLUUID& asset_id) const
{
    auto found = mMetadata.find(asset_id);
    if (found != mMetadata.end())
    {
        return &found->second;
    }
    for (const Entry& entry : mEntries)
    {
        if (entry.mAssetId == asset_id)
        {
            return &entry;
        }
    }
    return nullptr;
}

void ALGhostAnimAssetIndex::requestMetadata(const LLUUID& asset_id)
{
    const Entry* indexed = find(asset_id);
    if (!indexed || indexed->mAvailability != AVAILABLE_PENDING ||
        mMetadata.find(asset_id) != mMetadata.end() || !gAssetStorage)
    {
        return;
    }
    mMetadata[asset_id] = *indexed; // pending also deduplicates requests
    gAssetStorage->getAssetData(asset_id, LLAssetType::AT_ANIMATION,
                                onAssetLoaded, nullptr, false);
}

void ALGhostAnimAssetIndex::onAssetLoaded(const LLUUID& asset_id,
                                          LLAssetType::EType,
                                          void*, S32 status, LLExtStat)
{
    instance().finishMetadata(asset_id, status);
}

void ALGhostAnimAssetIndex::finishMetadata(const LLUUID& asset_id, S32 status)
{
    auto found = mMetadata.find(asset_id);
    if (found == mMetadata.end())
    {
        return;
    }
    Entry& entry = found->second;
    entry.mAvailability = AVAILABLE_UNAVAILABLE;
    if (status != 0)
    {
        return;
    }

    LLFileSystem file(asset_id, LLAssetType::AT_ANIMATION, LLFileSystem::READ);
    const S32 size = file.getSize();
    if (size <= 0)
    {
        return;
    }
    std::vector<U8> bytes(size);
    if (!file.read(bytes.data(), size))
    {
        return;
    }
    LLDataPackerBinaryBuffer dp(bytes.data(), size);
    LLKeyframeMotion motion(asset_id);
    if (!motion.deserialize(dp, asset_id, false))
    {
        return;
    }
    entry.mDuration = motion.getDuration();
    entry.mLoop = motion.getLoop();
    entry.mLoopIn = motion.getLoopIn();
    entry.mLoopOut = motion.getLoopOut();
    entry.mEaseIn = motion.getEaseInDuration();
    entry.mEaseOut = motion.getEaseOutDuration();
    entry.mPriority = motion.getPriority();
    entry.mJoints = motion.getJointMotionNames();
    entry.mHandPose = motion.getHandPose();
    entry.mHandPoseName = LLHandMotion::getHandPoseName(motion.getHandPose());
    entry.mEmoteId = motion.getEmoteID();
    entry.mEmoteName = motion.getEmoteName();
    entry.mAvailability = AVAILABLE_READY;
}
