/**
 * @file alghostanimassetindex.cpp
 * @brief Session-local, read-only inventory animation metadata index.
 */
#include "llviewerprecompiledheaders.h"

#include "alghostanimassetindex.h"

#include "llagent.h"
#include "llanimationstates.h"
#include "llbvhconsts.h"
#include "llcharacter.h"
#include "lldatapacker.h"
#include "llfilesystem.h"
#include "llhandmotion.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "llkeyframemotion.h"
#include "llviewerassetstorage.h"
#include "llviewerinventory.h"

#include <algorithm>

namespace
{
struct GhostAnimMetadata
{
    F32 mDuration = 0.f;
    bool mLoop = false;
    F32 mLoopIn = 0.f;
    F32 mLoopOut = 0.f;
    F32 mEaseIn = 0.f;
    F32 mEaseOut = 0.f;
    S32 mPriority = 0;
    std::vector<std::string> mJoints;
    S32 mHandPose = 0;
    LLUUID mEmoteId;
    std::string mEmoteName;
};

// Decode inventory metadata without constructing a runtime motion.  This
// deliberately has no LLCharacter input, allocates no LLJointState, performs
// no pose setup, and cannot read or write LLKeyframeDataCache.  Unknown joint
// names are retained: existing inventory content uses the same tolerant
// contract as LLKeyframeMotion::deserialize(..., allow_invalid_joints=true).
bool decodeGhostAnimMetadata(LLDataPacker& dp, const LLUUID& asset_id,
                             GhostAnimMetadata& metadata)
{
    U16 version = 0;
    U16 sub_version = 0;
    if (!dp.unpackU16(version, "version") ||
        !dp.unpackU16(sub_version, "sub_version"))
    {
        return false;
    }
    const bool old_version = version == 0 && sub_version == 1;
    if (!old_version &&
        (version != KEYFRAME_MOTION_VERSION ||
         sub_version != KEYFRAME_MOTION_SUBVERSION))
    {
        return false;
    }

    if (!dp.unpackS32(metadata.mPriority, "base_priority") ||
        metadata.mPriority < LLJoint::USE_MOTION_PRIORITY)
    {
        return false;
    }
    if (metadata.mPriority >= LLJoint::ADDITIVE_PRIORITY)
    {
        metadata.mPriority = LLJoint::ADDITIVE_PRIORITY - 1;
    }
    if (!dp.unpackF32(metadata.mDuration, "duration") ||
        !llfinite(metadata.mDuration) || metadata.mDuration > MAX_ANIM_DURATION)
    {
        return false;
    }
    if (!dp.unpackString(metadata.mEmoteName, "emote_name"))
    {
        return false;
    }
    if (!metadata.mEmoteName.empty())
    {
        if (metadata.mEmoteName == asset_id.asString())
        {
            return false;
        }
        if (metadata.mEmoteName == "Closed_Mouth")
        {
            metadata.mEmoteName.clear();
        }
        else
        {
            metadata.mEmoteId =
                gAnimLibrary.stringToAnimState(metadata.mEmoteName);
            if (metadata.mEmoteId.isNull())
            {
                metadata.mEmoteName.clear();
            }
        }
    }

    S32 loop = 0;
    if (!dp.unpackF32(metadata.mLoopIn, "loop_in_point") ||
        !llfinite(metadata.mLoopIn) ||
        !dp.unpackF32(metadata.mLoopOut, "loop_out_point") ||
        !llfinite(metadata.mLoopOut) ||
        !dp.unpackS32(loop, "loop") ||
        !dp.unpackF32(metadata.mEaseIn, "ease_in_duration") ||
        !llfinite(metadata.mEaseIn) ||
        !dp.unpackF32(metadata.mEaseOut, "ease_out_duration") ||
        !llfinite(metadata.mEaseOut))
    {
        return false;
    }
    metadata.mLoop = loop != 0;
    static const LLUUID female_land("ca1baf4d-0a18-5a1f-0330-e4bd1e71f09e");
    static const LLUUID formal_female_land("6a9a173b-61fa-3ad5-01fa-a851cfc5f66a");
    if (asset_id == female_land || asset_id == formal_female_land)
    {
        metadata.mLoop = false;
    }

    U32 hand_pose = 0;
    U32 num_joints = 0;
    if (!dp.unpackU32(hand_pose, "hand_pose") ||
        hand_pose > LLHandMotion::NUM_HAND_POSES ||
        !dp.unpackU32(num_joints, "num_joints") ||
        num_joints == 0 || num_joints > LL_CHARACTER_MAX_ANIMATED_JOINTS)
    {
        return false;
    }
    metadata.mHandPose = static_cast<S32>(hand_pose);
    metadata.mJoints.reserve(num_joints);

    for (U32 joint_index = 0; joint_index < num_joints; ++joint_index)
    {
        std::string joint_name;
        S32 joint_priority = 0;
        S32 num_keys = 0;
        if (!dp.unpackString(joint_name, "joint_name") ||
            joint_name == "mScreen" || joint_name == "mRoot" ||
            !dp.unpackS32(joint_priority, "joint_priority") ||
            joint_priority < LLJoint::USE_MOTION_PRIORITY ||
            !dp.unpackS32(num_keys, "num_rot_keys") || num_keys < 0)
        {
            return false;
        }
        metadata.mJoints.push_back(joint_name);

        for (S32 key = 0; key < num_keys; ++key)
        {
            F32 time = 0.f;
            U16 word = 0;
            LLVector3 vector;
            if (old_version)
            {
                if (!dp.unpackF32(time, "time") || !llfinite(time) ||
                    !dp.unpackVector3(vector, "rot_angles") ||
                    !vector.isFinite())
                {
                    return false;
                }
            }
            else if (!dp.unpackU16(word, "time") ||
                     !dp.unpackU16(word, "rot_angle_x") ||
                     !dp.unpackU16(word, "rot_angle_y") ||
                     !dp.unpackU16(word, "rot_angle_z"))
            {
                return false;
            }
        }

        if (!dp.unpackS32(num_keys, "num_pos_keys") || num_keys < 0)
        {
            return false;
        }
        for (S32 key = 0; key < num_keys; ++key)
        {
            F32 time = 0.f;
            U16 word = 0;
            LLVector3 vector;
            if (old_version)
            {
                if (!dp.unpackF32(time, "time") || !llfinite(time) ||
                    !dp.unpackVector3(vector, "pos") || !vector.isFinite())
                {
                    return false;
                }
            }
            else if (!dp.unpackU16(word, "time") ||
                     !dp.unpackU16(word, "pos_x") ||
                     !dp.unpackU16(word, "pos_y") ||
                     !dp.unpackU16(word, "pos_z"))
            {
                return false;
            }
        }
    }

    S32 num_constraints = 0;
    if (!dp.unpackS32(num_constraints, "num_constraints") ||
        num_constraints < 0 || num_constraints > 10)
    {
        return false;
    }
    for (S32 constraint = 0; constraint < num_constraints; ++constraint)
    {
        U8 byte = 0;
        U8 volume[16];
        LLVector3 vector;
        F32 value = 0.f;
        if (!dp.unpackU8(byte, "chain_length") || byte > num_joints ||
            !dp.unpackU8(byte, "constraint_type") ||
            byte >= NUM_CONSTRAINT_TYPES ||
            !dp.unpackBinaryDataFixed(volume, 16, "source_volume") ||
            !dp.unpackVector3(vector, "source_offset") || !vector.isFinite() ||
            !dp.unpackBinaryDataFixed(volume, 16, "target_volume") ||
            !dp.unpackVector3(vector, "target_offset") || !vector.isFinite() ||
            !dp.unpackVector3(vector, "target_dir") || !vector.isFinite() ||
            !dp.unpackF32(value, "ease_in_start") || !llfinite(value) ||
            !dp.unpackF32(value, "ease_in_stop") || !llfinite(value) ||
            !dp.unpackF32(value, "ease_out_start") || !llfinite(value) ||
            !dp.unpackF32(value, "ease_out_stop") || !llfinite(value))
        {
            return false;
        }
    }
    return true;
}
}

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
    GhostAnimMetadata metadata;
    if (!decodeGhostAnimMetadata(dp, asset_id, metadata))
    {
        return;
    }
    entry.mDuration = metadata.mDuration;
    entry.mLoop = metadata.mLoop;
    entry.mLoopIn = metadata.mLoopIn;
    entry.mLoopOut = metadata.mLoopOut;
    entry.mEaseIn = metadata.mEaseIn;
    entry.mEaseOut = metadata.mEaseOut;
    entry.mPriority = metadata.mPriority;
    entry.mJoints = std::move(metadata.mJoints);
    entry.mHandPose = metadata.mHandPose;
    entry.mHandPoseName = LLHandMotion::getHandPoseName(
        static_cast<LLHandMotion::eHandPose>(metadata.mHandPose));
    entry.mEmoteId = metadata.mEmoteId;
    entry.mEmoteName = std::move(metadata.mEmoteName);
    entry.mAvailability = AVAILABLE_READY;
}
