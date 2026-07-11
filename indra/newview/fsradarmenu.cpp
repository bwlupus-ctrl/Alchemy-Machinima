/**
 * @file fsradarmenu.cpp
 * @brief Menu used by Firestorm radar
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 *
 * Ported from Firestorm (I:\enve, indra/newview/fsradarmenu.cpp) to Alchemy
 * Machinima as part of the BD/FS -> Alchemy merge campaign, item F3.
 *
 * API remaps (see final report for the full list):
 *  - LLAvatarActions::{zoomIn,canZoomIn,derender*,landEject*,landFreeze*,
 *    estateKick*,estateBan*,estateTeleportHome*,canLandFreezeOrEject*,
 *    canEstateKickOrTeleportHome*,report,track} (FS extensions) ->
 *    ALAvatarActions equivalents, or FSRadar::derenderAvatar(s)/ALDerenderList
 *    for derender, or FSRadar::startTracking for tracking.
 *  - "Add to Set" (lggcontactsets), "Get Script Info" (LSL bridge),
 *    "Face towards avatar" (fsfloateravataralign) and the minimap "Mark..."
 *    submenu (LLNetMap::setAvatarMarkColor, not present in Alchemy) are
 *    dropped; see menu_fs_radar*.xml for the corresponding entry removals.
 *  - Render Settings (Do Not Render / Always Render) now persists via
 *    Alchemy's LLRenderMuteList instead of FS's FSAvatarRenderPersistence.
 */

#include "llviewerprecompiledheaders.h"

// libs
#include "llmenugl.h"

#include "fsradarmenu.h"

// newview
#include "alavataractions.h"
#include "fsradar.h"
#include "llagent.h"
#include "llavataractions.h"
#include "llcallingcard.h"          // for LLAvatarTracker
#include "lllogchat.h"
#include "llmutelist.h"             // for LLRenderMuteList
#include "llfloaterreg.h"
#include "llviewermenu.h"           // for gMenuHolder
#include "llvoavatar.h"
#include "rlvactions.h"
#include "rlvhandler.h"

namespace FSFloaterRadarMenu
{

FSRadarMenu gFSRadarMenu;

//== NearbyMenu ===============================================================

LLContextMenu* FSRadarMenu::createMenu()
{
    // set up the callbacks for all of the avatar menu items
    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
    LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

    if ( mUUIDs.size() == 1 )
    {
        // Set up for one person selected menu

        const LLUUID& id = mUUIDs.front();
        registrar.add("Avatar.Profile",                         boost::bind(&LLAvatarActions::showProfile,                  id));
        registrar.add("Avatar.AddFriend",                       boost::bind(&LLAvatarActions::requestFriendshipDialog,      id));
        registrar.add("Avatar.RemoveFriend",                    boost::bind(&LLAvatarActions::removeFriendDialog,           id));
        registrar.add("Avatar.IM",                              boost::bind(&LLAvatarActions::startIM,                      id));
        registrar.add("Avatar.Call",                            boost::bind(&LLAvatarActions::startCall,                    id));
        registrar.add("Avatar.OfferTeleport",                   boost::bind(&FSRadarMenu::offerTeleport,                    this));
        registrar.add("Avatar.TeleportRequest",                 boost::bind(&LLAvatarActions::teleportRequest,              id));
        registrar.add("Avatar.GroupInvite",                     boost::bind(&LLAvatarActions::inviteToGroup,                id));
        registrar.add("Avatar.ShowOnMap",                       boost::bind(&LLAvatarActions::showOnMap,                    id));
        registrar.add("Avatar.Share",                           boost::bind(&LLAvatarActions::share,                        id));
        registrar.add("Avatar.Pay",                             boost::bind(&LLAvatarActions::pay,                          id));
        registrar.add("Avatar.BlockUnblock",                    boost::bind(&LLAvatarActions::toggleBlock,                  id));
        registrar.add("Avatar.ZoomIn",                          boost::bind(&ALAvatarActions::zoomIn,                       id));
        registrar.add("Avatar.Report",                          boost::bind(&ALAvatarActions::reportAbuse,                  id));
        registrar.add("Avatar.Eject",                           boost::bind(&ALAvatarActions::parcelEject,                  id));
        registrar.add("Avatar.Freeze",                          boost::bind(&ALAvatarActions::parcelFreeze,                 id));
        registrar.add("Avatar.Kick",                            boost::bind(&ALAvatarActions::estateKick,                   id));
        registrar.add("Avatar.TeleportHome",                    boost::bind(&ALAvatarActions::estateTeleportHome,           id));
        registrar.add("Avatar.EstateBan",                       boost::bind(&ALAvatarActions::estateBan,                    id));
        registrar.add("Avatar.Derender",                        boost::bind(&FSRadar::derenderAvatar,                       id, false));
        registrar.add("Avatar.DerenderPermanent",                boost::bind(&FSRadar::derenderAvatar,                      id, true));
        registrar.add("Avatar.Calllog",                         boost::bind(&LLAvatarActions::viewChatHistory,              id));
        registrar.add("Nearby.People.TeleportToAvatar",         boost::bind(&FSRadarMenu::teleportToAvatar,                 this));
        registrar.add("Nearby.People.TrackAvatar",              boost::bind(&FSRadarMenu::onTrackAvatarMenuItemClick,       this));
        registrar.add("Nearby.People.SetRenderMode",            boost::bind(&FSRadarMenu::onSetRenderMode,                  this, _2));

        enable_registrar.add("Avatar.EnableItem",               boost::bind(&FSRadarMenu::enableContextMenuItem,            this, _2));
        enable_registrar.add("Avatar.CheckItem",                boost::bind(&FSRadarMenu::checkContextMenuItem,             this, _2));
        enable_registrar.add("Avatar.VisibleZoomIn",            boost::bind(&ALAvatarActions::canZoomIn,                    id));
        enable_registrar.add("Avatar.VisibleFreezeEject",       boost::bind(&ALAvatarActions::canFreezeEject,               id));
        enable_registrar.add("Avatar.VisibleKickTeleportHome",  boost::bind(&ALAvatarActions::canManageAvatarsEstate,       id));
        enable_registrar.add("Nearby.People.CheckRenderMode",   boost::bind(&FSRadarMenu::checkSetRenderMode,               this, _2));

        // create the context menu from the XUI
        return createFromFile("menu_fs_radar.xml");
    }
    else
    {
        // Set up for multi-selected People

        registrar.add("Avatar.IM",                              boost::bind(&LLAvatarActions::startConference,                      mUUIDs, LLUUID::null));
        registrar.add("Avatar.Call",                            boost::bind(&LLAvatarActions::startAdhocCall,                       mUUIDs, LLUUID::null));
        registrar.add("Avatar.OfferTeleport",                   boost::bind(&FSRadarMenu::offerTeleport,                            this));
        registrar.add("Avatar.RemoveFriend",                    boost::bind(&LLAvatarActions::removeFriendsDialog,                  mUUIDs));
        registrar.add("Avatar.Eject",                           boost::bind(&ALAvatarActions::parcelEjectMultiple,                  mUUIDs));
        registrar.add("Avatar.Freeze",                          boost::bind(&ALAvatarActions::parcelFreezeMultiple,                 mUUIDs));
        registrar.add("Avatar.Kick",                            boost::bind(&ALAvatarActions::estateKickMultiple,                   mUUIDs));
        registrar.add("Avatar.TeleportHome",                    boost::bind(&ALAvatarActions::estateTeleportHomeMultiple,           mUUIDs));
        registrar.add("Avatar.EstateBan",                       boost::bind(&ALAvatarActions::estateBanMultiple,                    mUUIDs));
        registrar.add("Avatar.Derender",                        boost::bind(&FSRadar::derenderAvatars,                              mUUIDs, false));
        registrar.add("Avatar.DerenderPermanent",                boost::bind(&FSRadar::derenderAvatars,                             mUUIDs, true));
        registrar.add("Nearby.People.SetRenderMode",            boost::bind(&FSRadarMenu::onSetRenderMode,                          this, _2));

        enable_registrar.add("Avatar.EnableItem",               boost::bind(&FSRadarMenu::enableContextMenuItem,                    this, _2));
        enable_registrar.add("Avatar.VisibleFreezeEject",       boost::bind(&ALAvatarActions::canFreezeEjectMultiple,               mUUIDs));
        enable_registrar.add("Avatar.VisibleKickTeleportHome",  boost::bind(&ALAvatarActions::canManageAvatarsEstateMultiple,       mUUIDs));

        // create the context menu from the XUI
        return createFromFile("menu_fs_radar_multiselect.xml");
    }
}

bool FSRadarMenu::enableContextMenuItem(const LLSD& userdata)
{
    std::string item = userdata.asString();

    // Note: can_block and can_delete is used only for one person selected menu
    // so we don't need to go over all uuids.

    if (item == std::string("can_block"))
    {
        const LLUUID& id = mUUIDs.front();
        return LLAvatarActions::canBlock(id);
    }
    else if (item == std::string("can_add"))
    {
        // We can add friends if:
        // - there are selected people
        // - and there are no friends among selection yet.
        if(mUUIDs.size() > 1)
        {
            return false;
        }

        bool result = (mUUIDs.size() > 0);

        uuid_vec_t::const_iterator
            id = mUUIDs.begin(),
            uuids_end = mUUIDs.end();

        for (;id != uuids_end; ++id)
        {
            if ( LLAvatarActions::isFriend(*id) )
            {
                result = false;
                break;
            }
        }

        return result;
    }
    else if (item == std::string("can_delete"))
    {
        // We can remove friends if:
        // - there are selected people
        // - and there are only friends among selection.
        bool result = (mUUIDs.size() > 0);

        uuid_vec_t::const_iterator
            id = mUUIDs.begin(),
            uuids_end = mUUIDs.end();

        for (;id != uuids_end; ++id)
        {
            if ( !LLAvatarActions::isFriend(*id) )
            {
                result = false;
                break;
            }
        }

        return result;
    }
    else if (item == std::string("can_call"))
    {
        return LLAvatarActions::canCall();
    }
    else if (item == std::string("can_show_on_map"))
    {
        const LLUUID& id = mUUIDs.front();

        return (LLAvatarTracker::instance().isBuddyOnline(id) && is_agent_mappable(id))
                    || gAgent.isGodlike();
    }
    else if(item == std::string("can_offer_teleport"))
    {
        return LLAvatarActions::canOfferTeleport(mUUIDs);
    }
    else if(item == std::string("can_request_teleport"))
    {
        // Alchemy has no direct equivalent of FS's canRequestTeleport() gate;
        // allow it whenever exactly one avatar is selected.
        return mUUIDs.size() == 1;
    }
    else if (item == std::string("can_open_inventory"))
    {
        return (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWINV));
    }
    else if (item == std::string("can_pay"))
    {
        const LLUUID& id = mUUIDs.front();
        return RlvActions::canPayAvatar(id);
    }
    else if (item == std::string("can_callog"))
    {
        return LLLogChat::isTranscriptExist(mUUIDs.front());
    }
    return false;
}

bool FSRadarMenu::checkContextMenuItem(const LLSD& userdata)
{
    std::string item = userdata.asString();
    const LLUUID& id = mUUIDs.front();

    if (item == std::string("is_blocked"))
    {
        return LLAvatarActions::isBlocked(id);
    }

    return false;
}

void FSRadarMenu::offerTeleport()
{
    // boost::bind cannot recognize overloaded method LLAvatarActions::offerTeleport(),
    // so we have to use a wrapper.
    LLAvatarActions::offerTeleport(mUUIDs);
}

void FSRadarMenu::teleportToAvatar()
// AO: wrapper for functionality managed by the radar floater, because it manages the avatar list.
// Will only work for avatars within radar range.
{
    ALAvatarActions::teleportTo(mUUIDs.front());
}

// Ansariel: Avatar tracking feature
void FSRadarMenu::onTrackAvatarMenuItemClick()
{
    FSRadar::getInstance()->startTracking(mUUIDs.front());
}

void FSRadarMenu::onSetRenderMode(const LLSD& userdata)
{
    LLVOAvatar::VisualMuteSettings render_setting;
    U32 mode = userdata.asInteger();
    switch (mode)
    {
        case 0:
            render_setting = LLVOAvatar::AV_RENDER_NORMALLY;
            break;
        case 1:
            render_setting = LLVOAvatar::AV_DO_NOT_RENDER;
            break;
        case 2:
            render_setting = LLVOAvatar::AV_ALWAYS_RENDER;
            break;
        default:
            LL_WARNS() << "Unknown value for visual mute settings: " << mode << LL_ENDL;
            return;
    }

    bool needs_culling = false;
    for (uuid_vec_t::const_iterator it = mUUIDs.begin(); it != mUUIDs.end(); ++it)
    {
        const LLUUID& avatar_id = *it;

        LLVOAvatar *avatarp = dynamic_cast<LLVOAvatar*>(gObjectList.findObject(avatar_id));
        if (avatarp)
        {
            avatarp->setVisualMuteSettings(render_setting);
            needs_culling = true;
        }

        LLRenderMuteList::getInstance()->saveVisualMuteSetting(avatar_id, (S32)render_setting);
    }

    if (needs_culling)
    {
        LLVOAvatar::cullAvatarsByPixelArea();
    }
}

bool FSRadarMenu::checkSetRenderMode(const LLSD& userdata)
{
    LLVOAvatar::VisualMuteSettings render_setting;
    U32 mode = userdata.asInteger();
    switch (mode)
    {
        case 0:
            render_setting = LLVOAvatar::AV_RENDER_NORMALLY;
            break;
        case 1:
            render_setting = LLVOAvatar::AV_DO_NOT_RENDER;
            break;
        case 2:
            render_setting = LLVOAvatar::AV_ALWAYS_RENDER;
            break;
        default:
            LL_WARNS() << "Unknown value for visual mute settings: " << mode << LL_ENDL;
            return false;
    }

    return LLRenderMuteList::getInstance()->getSavedVisualMuteSetting(mUUIDs.front()) == (S32)render_setting;
}

} // namespace FSFloaterRadarMenu
