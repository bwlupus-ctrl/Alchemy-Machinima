/**
 * @file alghostanimassetindex.h
 * @brief Session-local, read-only inventory animation metadata index.
 */
#ifndef AL_GHOSTANIMASSETINDEX_H
#define AL_GHOSTANIMASSETINDEX_H

#include "llsingleton.h"
#include "lluuid.h"

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
    void requestMetadata(const LLUUID& asset_id);

private:
    static void onAssetLoaded(const LLUUID& asset_id, LLAssetType::EType type,
                              void* user_data, S32 status, LLExtStat ext_status);
    void finishMetadata(const LLUUID& asset_id, S32 status);

    std::vector<Entry> mEntries;
    std::map<LLUUID, Entry> mMetadata;
};

#endif
