/**
 * @file ao.h
 * @brief Animation Explorer floater declaration
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2013, Zi Ree @ Second Life
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

// Donor: I:\enve (phoenix-reshade-XL / Firestorm, LGPL), indra/newview/animationexplorer.h.
// Ported for the BD/FS -> Alchemy merge campaign, item F7 (Animation Explorer).
// [BDMerge F7]

#ifndef ANIMATIONEXPLORER_H
#define ANIMATIONEXPLORER_H

#include "llfloater.h"
#include "llsingleton.h"

#include <boost/signals2.hpp>

// --------------------------------------------------------------------------
// RecentAnimationList: holds a list of recently placed animations to query
// by the animation explorer, so we don't have to keep the full floater
// loaded all the time.
// --------------------------------------------------------------------------

class AnimationExplorer;

class RecentAnimationList : public LLSingleton<RecentAnimationList>
{
    LLSINGLETON(RecentAnimationList);
    ~RecentAnimationList() = default;

public:
    struct AnimationEntry
    {
        LLUUID animationID; // asset ID of the animation
        LLUUID playedBy;    // object/agent who played this animation
        F64    time;        // time in seconds since viewer start when the animation started
    };

    std::deque<AnimationEntry> mAnimationList;

    void addAnimation(const LLUUID& id, const LLUUID& playedBy); // called in llviewermessage.cpp
    void requestList(AnimationExplorer* explorer);               // request animation list
};

// --------------------------------------------------------------------------
// AnimationExplorer: floater that shows recently played animations and gives
// options to preview, stop animations and revoke animation permissions
// --------------------------------------------------------------------------

class ALPanelAnimPreview;
class LLAvatarName;
class LLButton;
class LLCheckBoxCtrl;
class LLContextMenu;
class LLLineEditor;
class LLScrollListCtrl;
class LLView;
class LLViewerObject;

class AnimationExplorer : public LLFloater
{
    friend class LLFloaterReg;

private:
    AnimationExplorer(const LLSD& key);
    ~AnimationExplorer();

public:
    bool postBuild() override;
    void addAnimation(const LLUUID& id, const LLUUID& playedBy, F64 time); // called from RecentAnimationList

    // The preview pane + its mouse-drag-to-rotate handling + the own-avatar
    // Stop / Stop-and-Revoke / Blacklist / Capture-all controls now live in the
    // shared ALPanelAnimPreview (panel_anim_preview.xml), embedded below; this
    // floater just feeds it the selected animation via previewAnim().

protected:
    void onAvatarNameCallback(const LLUUID& id, const LLAvatarName& av_name);
    void onObjectPropsChanged(const LLUUID& id); // ALObjectPropertiesCache change signal
    void updateListEntry(const LLUUID& id, const std::string& name);
    void requestObjectName(LLViewerObject* vo); // provoke an ObjectProperties reply for an anonymous source

    LLScrollListCtrl* mAnimationScrollList;
    LLCheckBoxCtrl*   mNoOwnedAnimationsCheckBox;

    // [ActorMover] UUID tools: readout line + copy/handoff + list context menu
    LLLineEditor*           mAnimUUIDEditor = nullptr;
    LLHandle<LLContextMenu> mPopupMenuHandle;

    // shared preview pane + own-avatar action controls (owns its own dummy)
    ALPanelAnimPreview* mPreviewPanel = nullptr;

    LLUUID mCurrentAnimationID; // currently selected animation's asset ID
    LLUUID mCurrentObject;      // object ID that played the currently selected animation

    typedef std::map<LLUUID, boost::signals2::connection> avatar_name_cache_connection_map_t;
    avatar_name_cache_connection_map_t                    mAvatarNameCacheConnections;

    boost::signals2::connection mObjectPropsConnection; // ALObjectPropertiesCache::setChangeCallback

    void draw() override;
    void update();                          // request list update from RecentAnimationList
    void updateList(F64 current_timestamp); // update times and playing status in animation list

    void onSelectAnimation();
    void onOwnedCheckToggled();

    // [ActorMover] UUID tools
    void onCopyUUIDPressed();
    void onToActorMoverPressed();
    void onScrollListRightClicked(LLUICtrl* ctrl, S32 x, S32 y);
};

#endif // ANIMATIONEXPLORER_H
