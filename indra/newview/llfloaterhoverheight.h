/**
* @file   llfloaterhoverheight.h
* @brief  Controller for self avatar hover height, and (ClientHeight) the local,
*         render-only height offset of any avatar.
* @author vir@lindenlab.com
*
* $LicenseInfo:firstyear=2014&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2014, Linden Research, Inc.
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
#ifndef LL_LLFLOATERHOVERHEIGHT_H
#define LL_LLFLOATERHOVERHEIGHT_H

#include "llfloater.h"
#include "lluuid.h"

#include <set>
#include <vector>

class LLButton;
class LLComboBox;
class LLSliderCtrl;
class LLTextBox;

class LLFloaterHoverHeight: public LLFloater
{
public:
    LLFloaterHoverHeight(const LLSD& key);
    ~LLFloaterHoverHeight();
    bool postBuild();

    static void onSliderMoved(LLUICtrl* ctrl, void* userData);

    void onFinalCommit();

    static void syncFromPreferenceSetting(void *user_data, bool update_offset = true);

    void onRegionChanged();
    void onSimulatorFeaturesReceived(const LLUUID &region_id);
    void updateEditEnabled();

    /*virtual*/ void onOpen(const LLSD& key);
    /*virtual*/ void onClose(bool app_quitting);
    boost::signals2::connection mRegionChangedSlot;

private:
    // [ClientHeight] Local (client-side) avatar height. No draw() override: every refresh below is
    // event-driven (open, drop-down opening, Reset all, name-cache callback, store change).
    LLUUID targetKey() const;            // the selected store key (Me = gAgentID)
    bool   isMeSelected() const;
    void   rebuildTargets();             // combo contents; never from draw()
    void   onTargetChanged();
    void   onClientHeightMoved();        // live write to the store (cheap)
    void   onResetTarget();
    void   onResetAll();
    void   onStoreChanged();
    void   onNameResolved();
    void   refreshTargetWidgets(bool force);
    void   refreshStatus();

    LLComboBox*   mTargetCombo = nullptr;
    LLSliderCtrl* mHoverSlider = nullptr;
    LLSliderCtrl* mClientSlider = nullptr;
    LLButton*     mResetButton = nullptr;
    LLButton*     mResetAllButton = nullptr;
    LLTextBox*    mStatusText = nullptr;

    LLUUID        mTargetKey;            // null = Me
    bool          mUpdating = false;     // programmatic combo / slider writes
    boost::signals2::connection mStoreConnection;
    std::set<LLUUID>                         mNamesRequested;
    std::vector<boost::signals2::connection> mNameConnections;
};

#endif
