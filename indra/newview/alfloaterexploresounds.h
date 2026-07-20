/**
 * @file alfloaterexploresounds.h
 */

#ifndef AL_ALFLOATEREXPLORESOUNDS_H
#define AL_ALFLOATEREXPLORESOUNDS_H

#include "llfloater.h"
#include "lleventtimer.h"
#include "llaudioengine.h"
#include "llavatarnamecache.h"
#include "llhandle.h"

class LLCheckBoxCtrl;
class LLContextMenu;
class LLScrollListCtrl;
class LLUICtrl;

class ALFloaterExploreSounds final
: public LLFloater, public LLEventTimer
{
public:
    ALFloaterExploreSounds(const LLSD& key);
    bool postBuild();

    bool tick();

    LLSoundHistoryItem getItem(const LLUUID& itemID);

private:
    virtual ~ALFloaterExploreSounds();
    void handlePlayLocally();
    void handleLookAt();
    void handleStop();
    void handleStopLocally();
    void handleSelection();
    void blacklistSound();

    // right-click context menu on the sound list: reveal the selected sound's
    // decoded cache file (cache/sounds/<asset>.dsf) in the OS file manager.
    void onScrollListRightClicked(LLUICtrl* ctrl, S32 x, S32 y);
    void showSelectedInCacheFolder();

    LLHandle<LLContextMenu> mPopupMenuHandle;

    LLScrollListCtrl*   mHistoryScroller;
    LLCheckBoxCtrl*     mCollisionSounds;
    LLCheckBoxCtrl*     mRepeatedAssets;
    LLCheckBoxCtrl*     mAvatarSounds;
    LLCheckBoxCtrl*     mObjectSounds;
    LLCheckBoxCtrl*     mPaused;
    LLButton*           mStopLocalButton = nullptr;

    std::list<LLSoundHistoryItem> mLastHistory;

    uuid_vec_t mLocalPlayingAudioSourceIDs;

    typedef std::map<LLUUID, boost::signals2::connection> blacklist_avatar_name_cache_connection_map_t;
    blacklist_avatar_name_cache_connection_map_t mBlacklistAvatarNameCacheConnections;

    void onBlacklistAvatarNameCacheCallback(const LLUUID& av_id, const LLAvatarName& av_name, const LLUUID& asset_id, const std::string& region_name);
};

#endif
