/**
 * @file fsradar.cpp
 * @brief Firestorm radar implementation
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
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
 * Ported from Firestorm (I:\enve, indra/newview/fsradar.cpp) to Alchemy Machinima
 * as part of the BD/FS -> Alchemy merge campaign, item F3. See the final report
 * for the full list of stripped sub-features (lggcontactsets, fskeywords,
 * fslslbridge, lfsimfeaturehandler) and API remaps (ALAvatarActions,
 * ALDerenderList, LLRenderMuteList, region same-region alerts now handled by
 * Alchemy's existing llviewerregion.cpp::sendRadarAlert instead of being
 * duplicated here).
 */

#include "llviewerprecompiledheaders.h"

#include "fsradar.h"

// libs
#include "llavatarnamecache.h"
#include "llanimationstates.h"
#include "llcommonutils.h"
#include "llnotificationsutil.h"
#include "lleventtimer.h"

// newview
#include "alavataractions.h"
#include "alderenderlist.h"
#include "llagent.h"
#include "llavataractions.h"
#include "llfloaterimnearbychat.h"
#include "llfloaterreg.h"
#include "llmutelist.h"
#include "lltracker.h"
#include "lltrans.h"
#include "llviewercontrol.h"        // for gSavedSettings
#include "llviewerobjectlist.h"     // for gObjectList
#include "llviewerparcelmgr.h"
#include "llvoavatar.h"
#include "llvoiceclient.h"
#include "llworld.h"
#include "llspeakers.h"
#include "llviewerregion.h"
#include "rlvactions.h"
#include "rlvhandler.h"

/**
 * Periodically updates the nearby people list while the Nearby tab is active.
 *
 * The period is defined by FS_RADAR_LIST_UPDATE_INTERVAL constant.
 */

constexpr F32 FS_RADAR_LIST_UPDATE_INTERVAL = 1.f;

class FSRadarListUpdater : public FSRadar::Updater, public LLEventTimer
{
    LOG_CLASS(FSRadarListUpdater);

public:
    FSRadarListUpdater(callback_t cb)
    :   LLEventTimer(FS_RADAR_LIST_UPDATE_INTERVAL),
        FSRadar::Updater(cb)
    {
        update();
        mEventTimer.start();
    }

    bool tick() override
    {
        update();
        return false;
    }
};

//=============================================================================

// Small local stand-in for FSCommon::report_to_nearby_chat(), which does not
// exist in Alchemy. Mirrors the pattern used elsewhere in the tree (see
// llviewermessage.cpp caution-permission chat notices).
static void radar_report_to_nearby_chat(std::string_view message, const LLUUID& from_id, std::string_view from_name)
{
    if (LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLFloaterIMNearbyChat>("nearby_chat"); nearby_chat)
    {
        LLChat chat;
        chat.mText = static_cast<std::string>(message);
        chat.mSourceType = CHAT_SOURCE_SYSTEM;
        chat.mFromName = static_cast<std::string>(from_name);
        chat.mFromID = from_id;
        chat.mChatType = CHAT_TYPE_NORMAL;
        nearby_chat->addMessage(chat);
    }
}

//=============================================================================

FSRadar::FSRadar() :
    mRadarAlertRequest(false),
    mRadarFrameCount(0),
    mRadarLastRequestTime(0.f),
    mShowFriendsOnly(false),
    mShowUsernamesCallbackConnection(),
    mNameFormatCallbackConnection(),
    mAgeAlertCallbackConnection(),
    mShowFriendsOnlyCallbackConnection(),
    mShowFriendsOnlyAutoOffCallbackConnection(),
    mTeleportFinishedCallbackConnection(),
    mRegionCapabilitiesReceivedCallbackConnection(),
    mRegionChangedCallbackConnection()
{
    // Use the callback from LLAvatarNameCache here or we might update the names too early!
    LLAvatarNameCache::getInstance()->addUseDisplayNamesCallback(boost::bind(&FSRadar::updateNames, this));
    mShowUsernamesCallbackConnection = gSavedSettings.getControl("NameTagShowUsernames")->getSignal()->connect(boost::bind(&FSRadar::updateNames, this));

    mNameFormatCallbackConnection = gSavedSettings.getControl("RadarNameFormat")->getSignal()->connect(boost::bind(&FSRadar::updateNames, this));
    mAgeAlertCallbackConnection = gSavedSettings.getControl("RadarAvatarAgeAlertValue")->getSignal()->connect(boost::bind(&FSRadar::updateAgeAlertCheck, this));
    mShowFriendsOnlyCallbackConnection = gSavedSettings.getControl("FSRadarShowFriendsOnly")->getSignal()->connect(boost::bind(&FSRadar::onShowFriendsOnlySettingChanged, this));

    mRegionChangedCallbackConnection = gAgent.addRegionChangedCallback([this]() { onRegionChanged(); });
    mTeleportFinishedCallbackConnection = LLViewerParcelMgr::getInstance()->setTeleportFinishedCallback(
        [this](const LLVector3d& pos, const bool& local) { onTeleportFinished(pos, local); });
}

FSRadar::~FSRadar()
{
    gAgent.removeRegionChangedCallback(mRegionChangedCallbackConnection);
    if (mRegionCapabilitiesReceivedCallbackConnection.connected())
    {
        mRegionCapabilitiesReceivedCallbackConnection.disconnect();
    }

    if (mShowUsernamesCallbackConnection.connected())
    {
        mShowUsernamesCallbackConnection.disconnect();
    }

    if (mNameFormatCallbackConnection.connected())
    {
        mNameFormatCallbackConnection.disconnect();
    }

    if (mAgeAlertCallbackConnection.connected())
    {
        mAgeAlertCallbackConnection.disconnect();
    }

    if (mShowFriendsOnlyCallbackConnection.connected())
    {
        mShowFriendsOnlyCallbackConnection.disconnect();
    }

    if (mTeleportFinishedCallbackConnection.connected())
    {
        mTeleportFinishedCallbackConnection.disconnect();
    }
}

void FSRadar::initSingleton()
{
    mRadarListUpdater = std::make_unique<FSRadarListUpdater>(std::bind(&FSRadar::updateRadarList, this));
}

void FSRadar::radarAlertMsg(const LLUUID& agent_id, const LLAvatarName& av_name, std::string_view postMsg)
{
    // Alchemy already ships a toast-based alert system for same-region
    // enter/leave (see llviewerregion.cpp::sendRadarAlert + the
    // AlchemyRadarAlerts / AlchemyRadarAlertsToChat settings), so radar-level
    // alerts here are limited to chat-range/draw-range crossings and are
    // always reported as local system chat (FS's "Report To" toast/chat
    // switch and FSKeywords chat-keyword hook are not ported).
    radar_report_to_nearby_chat(postMsg, agent_id, FSRadarEntry::getRadarName(av_name));
}

void FSRadar::updateRadarList()
{
    //Configuration
    LLWorld* world = LLWorld::getInstance();
    LLMuteList* mutelist = LLMuteList::getInstance();
    ALDerenderList* derenderlist = ALDerenderList::getInstance();
    LLLocalSpeakerMgr* speakermgr = LLLocalSpeakerMgr::getInstance();
    LLVoiceClient* voice_client = LLVoiceClient::getInstance();
    LLViewerParcelMgr& parcelmgr = LLViewerParcelMgr::instance();
    LLUIColorTable& colortable = LLUIColorTable::instance();
    LLAvatarTracker& avatartracker = LLAvatarTracker::instance();

    // Alchemy's LLViewerRegion has no say/shout range accessors (FS gets them
    // from LFSimFeatureHandler) - use the protocol constants: 20m say (see
    // CHAT_NORMAL_RADIUS), 100m shout.
    const F32 chat_range_say = CHAT_NORMAL_RADIUS;
    const F32 chat_range_shout = 100.f;

    static const std::string str_chat_entering =            LLTrans::getString("entering_chat_range");
    static const std::string str_chat_leaving =             LLTrans::getString("leaving_chat_range");
    static const std::string str_draw_distance_entering =   LLTrans::getString("entering_draw_distance");
    static const std::string str_draw_distance_leaving =    LLTrans::getString("leaving_draw_distance");
    static const std::string str_avatar_age_alert =         LLTrans::getString("avatar_age_alert");
    static const std::string str_avatar_age_hidden =        LLTrans::getString("avatar_age_not_available");

    static LLCachedControl<bool> sRadarReportChatRangeEnter(gSavedSettings, "RadarReportChatRangeEnter");
    static LLCachedControl<bool> sRadarReportChatRangeLeave(gSavedSettings, "RadarReportChatRangeLeave");
    static LLCachedControl<bool> sRadarReportDrawRangeEnter(gSavedSettings, "RadarReportDrawRangeEnter");
    static LLCachedControl<bool> sRadarReportDrawRangeLeave(gSavedSettings, "RadarReportDrawRangeLeave");
    static LLCachedControl<bool> sRadarEnterChannelAlert(gSavedSettings, "RadarEnterChannelAlert");
    static LLCachedControl<bool> sRadarLeaveChannelAlert(gSavedSettings, "RadarLeaveChannelAlert");
    static LLCachedControl<bool> sRadarAvatarAgeAlert(gSavedSettings, "RadarAvatarAgeAlert");
    static LLCachedControl<F32> sNearMeRange(gSavedSettings, "NearMeRange");
    static LLCachedControl<bool> sLimitRange(gSavedSettings, "LimitRadarByRange");
    static LLCachedControl<F32> sRenderFarClip(gSavedSettings, "RenderFarClip");
    static LLCachedControl<bool> sFSLegacyRadarFriendColoring(gSavedSettings, "FSLegacyRadarFriendColoring");
    static LLCachedControl<bool> sFSRadarColorNamesByDistance(gSavedSettings, "FSRadarColorNamesByDistance", false);
    static LLCachedControl<bool> sFSRadarShowMutedAndDerendered(gSavedSettings, "FSRadarShowMutedAndDerendered");

    F32 drawRadius(sRenderFarClip);
    const LLVector3d& posSelf = gAgent.getPositionGlobal();
    LLUUID regionSelf;
    if (LLViewerRegion* own_reg = gAgent.getRegion())
    {
        regionSelf = own_reg->getRegionID();
    }
    bool alertScripts = mRadarAlertRequest; // save the current value, so it doesn't get changed out from under us by another thread
    time_t now = time(nullptr);

    //STEP 0: Clear model data
    mRadarEnterAlerts.clear();
    mRadarLeaveAlerts.clear();
    mRadarEntriesData.clear();
    mAvatarStats.clear();

    //STEP 1: Update our basic data model: detect Avatars & Positions in our defined range
    std::vector<LLVector3d> positions;
    uuid_vec_t avatar_ids;
    if (RlvActions::canShowNearbyAgents())
    {
        if (sLimitRange)
        {
            world->getAvatars(&avatar_ids, &positions, posSelf, sNearMeRange);
        }
        else
        {
            world->getAvatars(&avatar_ids, &positions);
        }
    }

    // Determine lists of new added and removed avatars
    uuid_vec_t current_vec, added_vec, removed_vec;
    current_vec.reserve(mEntryList.size());
    for (const auto& [av_id, entry] : mEntryList)
    {
        current_vec.emplace_back(av_id);
    }
    LLCommonUtils::computeDifference(avatar_ids, current_vec, added_vec, removed_vec);

    // Remove old avatars from our list
    for (const auto& avid : removed_vec)
    {
        if (entry_map_t::iterator found = mEntryList.find(avid); found != mEntryList.end())
        {
            mEntryList.erase(found);
        }
    }

    // Add new avatars
    for (const auto& avid : added_vec)
    {
        mEntryList.emplace(avid, std::make_shared<FSRadarEntry>(avid));
    }

    speakermgr->update(true);

    // Show Friends Only: derender any newly-seen non-friend
    if (mShowFriendsOnly)
    {
        applyShowFriendsOnly();
    }

    //STEP 2: Transform detected model list data into more flexible multimap data structure;
    //TS: Count avatars in chat range and in the same region
    U32 inChatRange{ 0U };
    U32 inSameRegion{ 0U };
    std::vector<LLVector3d>::const_iterator
        pos_it = positions.begin(),
        pos_end = positions.end();
    uuid_vec_t::const_iterator
        item_it = avatar_ids.begin(),
        item_end = avatar_ids.end();
    for (;pos_it != pos_end && item_it != item_end; ++pos_it, ++item_it)
    {
        //
        //2a. For each detected av, gather up all data we would want to display or use to drive alerts
        //
        const LLUUID& avId   = static_cast<LLUUID>(*item_it);
        LLVector3d avPos     = static_cast<LLVector3d>(*pos_it);

        if (avId == gAgentID)
        {
            continue;
        }

        // Skip modelling this avatar if its basic data is either inaccessible, or it's a dummy placeholder
        auto ent = getEntry(avId);
        if (!ent) // don't update this radar listing if data is inaccessible
        {
            continue;
        }

        // Try to get the avatar's viewer object - we will need it anyway later
        LLVOAvatar* avVo = static_cast<LLVOAvatar*>(gObjectList.findObject(avId));

        static LLUICachedControl<bool> sFSShowDummyAVsinRadar("FSShowDummyAVsinRadar", false);
        if (!sFSShowDummyAVsinRadar && avVo && avVo->mIsDummy)
        {
            continue;
        }

        const bool is_muted = mutelist->isMuted(avId);
        const bool is_derendered = derenderlist->isDerendered(ALDerenderEntry::TYPE_AVATAR, avId);
        const bool should_be_ignored = is_muted || is_derendered;
        ent->mIgnore = should_be_ignored;
        if (!sFSRadarShowMutedAndDerendered && should_be_ignored)
        {
            continue;
        }

        LLUUID avRegion;
        if (LLViewerRegion* reg = world->getRegionFromPosGlobal(avPos))
        {
            avRegion = reg->getRegionID();
        }
        const bool isInSameRegion = (avRegion == regionSelf);
        const bool isOnSameParcel = parcelmgr.inAgentParcel(avPos);
        const S32 seentime = (S32)difftime(now, ent->mFirstSeen);
        const S32 hours = (S32)(seentime / 3600);
        const S32 mins = (S32)((seentime - hours * 3600) / 60);
        const S32 secs = (S32)((seentime - hours * 3600 - mins * 60));
        const std::string avSeenStr = llformat("%d:%02d:%02d", hours, mins, secs);
        const S32& avStatusFlags     = ent->mStatus;
        ERadarPaymentInfoFlag avFlag = FSRADAR_PAYMENT_INFO_NONE;
        if (avStatusFlags & AVATAR_TRANSACTED)
        {
            avFlag = FSRADAR_PAYMENT_INFO_USED;
        }
        else if (avStatusFlags & AVATAR_IDENTIFIED)
        {
            avFlag = FSRADAR_PAYMENT_INFO_FILLED;
        }
        const S32& avAge = ent->mAge;
        const std::string& avName = ent->mName;
        // NOTE: FS enhances the Z position of avatars beyond draw distance via
        // an LSL bridge round-trip (FSRadarEnhanceByBridge). Alchemy has no
        // bridge, so we fall back to the coarse position FS already reports
        // when no bridge is available: avRange simply stays AVATAR_UNKNOWN_RANGE
        // and the UI shows ">drawRadius" (see FSPanelRadar::updateList()).
        const F32 avRange = (F32)(avPos[VZ] != AVATAR_UNKNOWN_Z_OFFSET ? dist_vec(avPos, posSelf) : AVATAR_UNKNOWN_RANGE);
        ent->mRange = avRange;
        ent->mGlobalPos = avPos;
        ent->mRegion = avRegion;

        // Double-check range here since limiting range on calling LLWorld::getAvatars does
        // not work if other avatar is beyond draw distance and above 1020m height.
        if (sLimitRange && avRange > sNearMeRange)
        {
            continue;
        }

        //
        //2b. Process newly detected avatars
        //
        radarfields_map_t::iterator last_sweep_found_it = mLastRadarSweep.find(avId);
        if (last_sweep_found_it == mLastRadarSweep.end())
        {
            // chat/draw range alerts
            if (sRadarReportChatRangeEnter && (avRange <= chat_range_say) && avRange > AVATAR_UNKNOWN_RANGE)
            {
                LLStringUtil::format_map_t args;
                args["DISTANCE"] = llformat("%3.2f", avRange);
                std::string message = str_chat_entering;
                LLStringUtil::format(message, args);
                make_ui_sound("UISndRadarChatEnter");
                LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, message));
            }
            if (sRadarReportDrawRangeEnter && (avRange <= drawRadius) && avRange > AVATAR_UNKNOWN_RANGE)
            {
                LLStringUtil::format_map_t args;
                args["DISTANCE"] = llformat("%3.2f", avRange);
                std::string message = str_draw_distance_entering;
                LLStringUtil::format(message, args);
                make_ui_sound("UISndRadarDrawEnter");
                LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, message));
            }
            if (sRadarEnterChannelAlert || (alertScripts))
            {
                // If Leave channel alerts are not set, restrict reports to same-sim only.
                if (!sRadarLeaveChannelAlert)
                {
                    if (isInSameRegion)
                    {
                        mRadarEnterAlerts.push_back(avId);
                    }
                }
                else
                {
                    mRadarEnterAlerts.push_back(avId);
                }
            }
        }

        //
        // 2c. Process previously detected avatars
        //
        else
        {
            RadarFields rf = last_sweep_found_it->second;
            if (sRadarReportChatRangeEnter || sRadarReportChatRangeLeave)
            {
                if (sRadarReportChatRangeEnter && (avRange <= chat_range_say && avRange > AVATAR_UNKNOWN_RANGE) && (rf.lastDistance > chat_range_say || rf.lastDistance == AVATAR_UNKNOWN_RANGE))
                {
                    LLStringUtil::format_map_t args;
                    args["DISTANCE"] = llformat("%3.2f", avRange);
                    std::string message = str_chat_entering;
                    LLStringUtil::format(message, args);
                    make_ui_sound("UISndRadarChatEnter");
                    LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, message));
                }
                else if (sRadarReportChatRangeLeave && (avRange > chat_range_say || avRange == AVATAR_UNKNOWN_RANGE) && (rf.lastDistance <= chat_range_say && rf.lastDistance > AVATAR_UNKNOWN_RANGE))
                {
                    make_ui_sound("UISndRadarChatLeave");
                    LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_chat_leaving));
                }
            }
            if (sRadarReportDrawRangeEnter || sRadarReportDrawRangeLeave)
            {
                if (sRadarReportDrawRangeEnter && (avRange <= drawRadius && avRange > AVATAR_UNKNOWN_RANGE) && (rf.lastDistance > drawRadius || rf.lastDistance == AVATAR_UNKNOWN_RANGE))
                {
                    LLStringUtil::format_map_t args;
                    args["DISTANCE"] = llformat("%3.2f", avRange);
                    std::string message = str_draw_distance_entering;
                    LLStringUtil::format(message, args);
                    make_ui_sound("UISndRadarDrawEnter");
                    LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, message));
                }
                else if (sRadarReportDrawRangeLeave && (avRange > drawRadius || avRange == AVATAR_UNKNOWN_RANGE) && (rf.lastDistance <= drawRadius && rf.lastDistance > AVATAR_UNKNOWN_RANGE))
                {
                    make_ui_sound("UISndRadarDrawLeave");
                    LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_draw_distance_leaving));
                }
            }
            //If we were manually asked to update an external source for all existing avatars, add them to the queue.
            if (alertScripts)
            {
                mRadarEnterAlerts.push_back(avId);
            }
        }

        //
        //2d. Prepare data for presentation view for this avatar
        //
        if (isInSameRegion)
        {
            ++inSameRegion;
        }

        LLSD entry;
        LLSD entry_options;

        entry["id"] = avId;
        entry["name"] = avName;
        entry["in_region"] = isInSameRegion;
        entry["on_parcel"] = isOnSameParcel;
        entry["flags"] = avFlag;
        entry["seen"] = avSeenStr;
        entry["range"] = (avRange > AVATAR_UNKNOWN_RANGE ? llformat("%3.2f", avRange) : llformat(">%3.2f", drawRadius));
        entry["typing"] = (avVo && avVo->isTyping());
        entry["sitting"] = (avVo && (avVo->getParent() || avVo->isMotionActive(ANIM_AGENT_SIT_GROUND) || avVo->isMotionActive(ANIM_AGENT_SIT_GROUND_CONSTRAINED)));

        if (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
        {
            entry["notes"] = ent->getNotes();
            if (avAge > -1)
                entry["age"] = llformat("%d", avAge);
            else if (avAge == -2)
                entry["age"] = str_avatar_age_hidden;
            else
                entry["age"] = "";
            if (ent->hasAlertAge())
            {
                entry_options["age_color"] = colortable.getColor("AvatarListItemAgeAlert", LLColor4::red).get().getValue();

                if (sRadarAvatarAgeAlert && !ent->hasAgeAlertPerformed())
                {
                    make_ui_sound("UISndRadarAgeAlert");
                    LLStringUtil::format_map_t args;
                    args["AGE"] = llformat("%d", avAge);
                    std::string message = str_avatar_age_alert;
                    LLStringUtil::format(message, args);
                    LLAvatarNameCache::get(avId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, message));
                }
                ent->mAgeAlertPerformed = true;
            }
        }
        else
        {
            entry["notes"] = LLStringUtil::null;
            entry["age"] = "---";
        }

        //AO: Set any range colors / styles
        LLUIColor range_color;
        if (avRange > AVATAR_UNKNOWN_RANGE)
        {
            if (avRange <= chat_range_say)
            {
                range_color = colortable.getColor("AvatarListItemChatRange", LLColor4::red);
                inChatRange++;
            }
            else if (avRange <= chat_range_shout)
            {
                range_color = colortable.getColor("AvatarListItemShoutRange", LLColor4::white);
            }
            else
            {
                range_color = colortable.getColor("AvatarListItemBeyondShoutRange", LLColor4::white);
            }
        }
        else
        {
            range_color = colortable.getColor("AvatarListItemBeyondShoutRange", LLColor4::white);
        }
        entry_options["range_color"] = range_color.get().getValue();

        // Check if avatar is in draw distance and a VOAvatar instance actually exists
        if (avRange <= drawRadius && avRange > AVATAR_UNKNOWN_RANGE && avVo)
        {
            entry_options["range_style"] = LLFontGL::BOLD;
        }
        else
        {
            entry_options["range_style"] = LLFontGL::NORMAL;
        }

        // Set friends colors / styles. NOTE: FS also recolors names via
        // lggcontactsets ("contact sets"); Alchemy has no equivalent so we
        // only keep the friend (bold) / muted (italic) styling.
        LLFontGL::StyleFlags nameCellStyle = LLFontGL::NORMAL;
        const LLRelationship* relation = avatartracker.getBuddyInfo(avId);
        if (relation && !sFSLegacyRadarFriendColoring && !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
        {
            nameCellStyle = (LLFontGL::StyleFlags)(nameCellStyle | LLFontGL::BOLD);
        }
        if (is_muted)
        {
            nameCellStyle = (LLFontGL::StyleFlags)(nameCellStyle | LLFontGL::ITALIC);
        }
        entry_options["name_style"] = nameCellStyle;

        LLColor4 name_color = colortable.getColor("AvatarListItemIconDefaultColor", LLColor4::white).get();
        if (sFSRadarColorNamesByDistance)
        {
            name_color = range_color.get();
        }
        entry_options["name_color"] = name_color.getValue();

        // Voice indicator: Alchemy's LLVoiceClient does not expose FS's
        // per-participant PTT/power-level enum, so we only report whether the
        // avatar is currently in the active voice channel; the fine-grained
        // Radar_VoicePTT_* icon states are not populated (no matching icon
        // assets shipped in Alchemy's skin).
        if (voice_client->voiceEnabled() && voice_client->isVoiceWorking())
        {
            if (LLSpeaker* speaker = speakermgr->findSpeaker(avId); speaker && speaker->isInVoiceChannel())
            {
                entry["voice_level_icon"] = "Radar_VoicePTT_On";
            }
        }

        // Save data for our listeners
        LLSD entry_data;
        entry_data["entry"] = entry;
        entry_data["options"] = entry_options;
        mRadarEntriesData.push_back(std::move(entry_data));
    } // End STEP 2, all model/presentation row processing complete.

    //
    //STEP 3, process any bulk actions that require the whole model to be known first
    //

    //
    //3a: process alerts for avatars that where here last frame, but gone this frame (ie, they left)
    //    as well as dispatch all earlier detected alerts for crossing range thresholds.
    //
    if (RlvActions::canShowNearbyAgents())
    {
        for (const auto& [prevId, rf] : mLastRadarSweep)
        {
            if ((sFSRadarShowMutedAndDerendered || !rf.lastIgnore) && mEntryList.find(prevId) == mEntryList.end())
            {
                if (sRadarReportChatRangeLeave && (rf.lastDistance <= chat_range_say) && rf.lastDistance > AVATAR_UNKNOWN_RANGE)
                {
                    make_ui_sound("UISndRadarChatLeave");
                    LLAvatarNameCache::get(prevId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_chat_leaving));
                }
                if (sRadarReportDrawRangeLeave && (rf.lastDistance <= drawRadius) && rf.lastDistance > AVATAR_UNKNOWN_RANGE)
                {
                    make_ui_sound("UISndRadarDrawLeave");
                    LLAvatarNameCache::get(prevId, boost::bind(&FSRadar::radarAlertMsg, this, _1, _2, str_draw_distance_leaving));
                }

                if (sRadarLeaveChannelAlert)
                {
                    mRadarLeaveAlerts.push_back(prevId);
                }
            }
        }
    }

    static LLCachedControl<S32> sRadarAlertChannel(gSavedSettings, "RadarAlertChannel");
    if (U32 num_entering = (U32)mRadarEnterAlerts.size(); num_entering > 0)
    {
        mRadarFrameCount++;
        U32 num_this_pass = llmin(FSRADAR_MAX_AVATARS_PER_ALERT, num_entering);
        std::string msg = llformat("%d,%d", mRadarFrameCount, num_this_pass);
        U32 loop = 0;
        while (loop < num_entering)
        {
            for (U32 i = 0; i < num_this_pass; i++)
            {
                msg = llformat("%s,%s", msg.c_str(), mRadarEnterAlerts[loop + i].asString().c_str());
            }
            LLMessageSystem* msgs = gMessageSystem;
            msgs->newMessage("ScriptDialogReply");
            msgs->nextBlock("AgentData");
            msgs->addUUID("AgentID", gAgentID);
            msgs->addUUID("SessionID", gAgentSessionID);
            msgs->nextBlock("Data");
            msgs->addUUID("ObjectID", gAgentID);
            msgs->addS32("ChatChannel", sRadarAlertChannel());
            msgs->addS32("ButtonIndex", 1);
            msgs->addString("ButtonLabel", msg.c_str());
            gAgent.sendReliableMessage();
            loop += num_this_pass;
            num_this_pass = llmin(FSRADAR_MAX_AVATARS_PER_ALERT, num_entering - loop);
            msg = llformat("%d,%d", mRadarFrameCount, num_this_pass);
        }
    }

    if (U32 num_leaving = (U32)mRadarLeaveAlerts.size(); num_leaving > 0)
    {
        mRadarFrameCount++;
        U32 num_this_pass = llmin(FSRADAR_MAX_AVATARS_PER_ALERT, num_leaving);
        std::string msg = llformat("%d,-%d", mRadarFrameCount, llmin(FSRADAR_MAX_AVATARS_PER_ALERT, num_leaving));
        U32 loop = 0;
        while (loop < num_leaving)
        {
            for (U32 i = 0; i < num_this_pass; i++)
            {
                msg = llformat("%s,%s", msg.c_str(), mRadarLeaveAlerts[loop + i].asString().c_str());
            }
            LLMessageSystem* msgs = gMessageSystem;
            msgs->newMessage("ScriptDialogReply");
            msgs->nextBlock("AgentData");
            msgs->addUUID("AgentID", gAgentID);
            msgs->addUUID("SessionID", gAgentSessionID);
            msgs->nextBlock("Data");
            msgs->addUUID("ObjectID", gAgentID);
            msgs->addS32("ChatChannel", sRadarAlertChannel());
            msgs->addS32("ButtonIndex", 1);
            msgs->addString("ButtonLabel", msg.c_str());
            gAgent.sendReliableMessage();
            loop += num_this_pass;
            num_this_pass = llmin(FSRADAR_MAX_AVATARS_PER_ALERT, num_leaving - loop);
            msg = llformat("%d,-%d", mRadarFrameCount, num_this_pass);
        }
    }

    // reset any active alert requests
    if (alertScripts)
    {
        mRadarAlertRequest = false;
    }

    //
    //STEP 4: Cache our current model data, so we can compare it with the next fresh group of model data for fast change detection.
    //

    mLastRadarSweep.clear();
    for (const auto& [avid, entry] : mEntryList)
    {
        RadarFields rf;
        rf.lastDistance = entry->mRange;
        rf.lastIgnore = entry->mIgnore;
        rf.lastRegion.setNull();
        if (!entry->mGlobalPos.isNull())
        {
            if (LLViewerRegion* lastRegion = world->getRegionFromPosGlobal(entry->mGlobalPos))
            {
                rf.lastRegion = lastRegion->getRegionID();
            }
        }

        mLastRadarSweep[entry->mID] = rf;
    }

    //
    //STEP 5: Final data updates and notification of subscribers
    //
    if (RlvActions::canShowNearbyAgents())
    {
        mAvatarStats["total"] = llformat("%d", mLastRadarSweep.size() - 1);
        mAvatarStats["region"] = llformat("%d", inSameRegion);
        mAvatarStats["chatrange"] = llformat("%d", inChatRange);
    }
    else
    {
        mAvatarStats["total"] = "-";
        mAvatarStats["region"] = "-";
        mAvatarStats["chatrange"] = "-";
    }

    checkTracking();

    // Inform our subscribers about updates
    if (!mUpdateSignal.empty())
    {
        mUpdateSignal(mRadarEntriesData, mAvatarStats);
    }
}

void FSRadar::requestRadarChannelAlertSync()
{
    if (F32 timeNow = gFrameTimeSeconds; (timeNow - FSRADAR_CHAT_MIN_SPACING) > mRadarLastRequestTime)
    {
        mRadarLastRequestTime = timeNow;
        mRadarAlertRequest = true;
    }
}

std::shared_ptr<FSRadarEntry> FSRadar::getEntry(const LLUUID& avatar_id)
{
    if (entry_map_t::iterator found = mEntryList.find(avatar_id); found != mEntryList.end())
    {
        return found->second;
    }
    return nullptr;
}

void FSRadar::teleportToAvatar(const LLUUID& targetAv)
// Teleports user to last scanned location of nearby avatar
// Note: currently teleportViaLocation is disrupted by enforced landing points set on a parcel.
{
    if (auto entry = getEntry(targetAv))
    {
        LLVector3d avpos = entry->mGlobalPos;
        if (avpos.mdV[VZ] == AVATAR_UNKNOWN_Z_OFFSET)
        {
            LLNotificationsUtil::add("TeleportToAvatarNotPossible");
        }
        else
        {
            // FIRE-20862: Teleport the configured offset toward the center of the region from the
            // avatar's reported position
            LLViewerRegion* avreg = LLWorld::getInstance()->getRegionFromPosGlobal(avpos);
            if (avreg)
            {
                LLVector3d region_center = avreg->getCenterGlobal();
                LLVector3d offset = avpos - region_center;
                LLVector3d destination;
                F32 lateral_distance = gSavedSettings.getF32("FSTeleportToOffsetLateral");
                F32 vertical_distance = gSavedSettings.getF32("FSTeleportToOffsetVertical");
                if (offset.normalize() != 0.f) // there's an actual offset
                {
                    if (lateral_distance > 0.0f)
                    {
                        offset *= lateral_distance;
                        destination = avpos - offset;
                    }
                    else
                    {
                        destination = avpos;
                    }
                }
                else // the target is exactly at the center, so the offset is 0
                {
                    destination = region_center + LLVector3d(0.f, lateral_distance, 0.f);
                }
                destination.mdV[VZ] = avpos.mdV[VZ] + vertical_distance;
                gAgent.teleportViaLocation(destination);
            }
        }
    }
    else
    {
        LLNotificationsUtil::add("TeleportToAvatarNotPossible");
    }
}

//static
void FSRadar::onRadarNameFmtClicked(const LLSD& userdata)
{
    const std::string chosen_item = userdata.asString();
    if (chosen_item == "DN")
    {
        gSavedSettings.setU32("RadarNameFormat", FSRADAR_NAMEFORMAT_DISPLAYNAME);
    }
    else if (chosen_item == "UN")
    {
        gSavedSettings.setU32("RadarNameFormat", FSRADAR_NAMEFORMAT_USERNAME);
    }
    else if (chosen_item == "DNUN")
    {
        gSavedSettings.setU32("RadarNameFormat", FSRADAR_NAMEFORMAT_DISPLAYNAME_USERNAME);
    }
    else if (chosen_item == "UNDN")
    {
        gSavedSettings.setU32("RadarNameFormat", FSRADAR_NAMEFORMAT_USERNAME_DISPLAYNAME);
    }
}

//static
bool FSRadar::radarNameFmtCheck(const LLSD& userdata)
{
    const std::string menu_item = userdata.asString();
    U32 name_format = gSavedSettings.getU32("RadarNameFormat");
    switch (name_format)
    {
        case FSRADAR_NAMEFORMAT_DISPLAYNAME:
            return (menu_item == "DN");
        case FSRADAR_NAMEFORMAT_USERNAME:
            return (menu_item == "UN");
        case FSRADAR_NAMEFORMAT_DISPLAYNAME_USERNAME:
            return (menu_item == "DNUN");
        case FSRADAR_NAMEFORMAT_USERNAME_DISPLAYNAME:
            return (menu_item == "UNDN");
        default:
            break;
    }
    return false;
}

//static
void FSRadar::derenderAvatar(const LLUUID& avatar_id, bool permanent)
{
    uuid_vec_t ids;
    ids.push_back(avatar_id);
    derenderAvatars(ids, permanent);
}

//static
void FSRadar::derenderAvatars(const uuid_vec_t& avatar_ids, bool permanent)
{
    for (const LLUUID& id : avatar_ids)
    {
        std::string name;
        if (auto entry = FSRadar::getInstance()->getEntry(id))
        {
            name = entry->getName();
        }
        ALDerenderList::instance().addAvatar(id, name, permanent);
    }
}

void FSRadar::startTracking(const LLUUID& avatar_id)
{
    if (getEntry(avatar_id))
    {
        mTrackedAvatarId = avatar_id;
        updateTracking();
    }
    else
    {
        LLNotificationsUtil::add("TrackAvatarNotPossible");
    }
}

void FSRadar::checkTracking()
{
    // Alchemy's LLTracker has no LOCATION_AVATAR type (its TRACKING_AVATAR
    // status instead delegates to LLAvatarTracker, which only works for
    // friends with server-side coarse location). We drive the beacon
    // ourselves via trackLocation()/LOCATION_ITEM and use mTrackedAvatarId as
    // the source of truth for whether we're the one steering the tracker.
    if (mTrackedAvatarId.notNull()
        && LLTracker::getTrackingStatus() == LLTracker::TRACKING_LOCATION
        && LLTracker::getTrackedLocationType() == LLTracker::LOCATION_ITEM)
    {
        updateTracking();
    }
}

void FSRadar::updateTracking()
{
    if (auto entry = getEntry(mTrackedAvatarId))
    {
        if (LLTracker::getTrackedPositionGlobal() != entry->mGlobalPos)
        {
            LLTracker::trackLocation(entry->mGlobalPos, entry->mName, "", LLTracker::LOCATION_ITEM);
        }
    }
    else
    {
        mTrackedAvatarId.setNull();
        LLTracker::stopTracking(false);
    }
}

void FSRadar::zoomAvatar(const LLUUID& avatar_id, std::string_view name)
{
    if (ALAvatarActions::canZoomIn(avatar_id))
    {
        ALAvatarActions::zoomIn(avatar_id);
    }
    else
    {
        LLStringUtil::format_map_t args;
        args["AVATARNAME"] = static_cast<std::string>(name);
        std::string message = LLTrans::getString("camera_no_focus");
        LLStringUtil::format(message, args);
        radar_report_to_nearby_chat(message, LLUUID::null, LLStringUtil::null);
    }
}

void FSRadar::updateNames()
{
    for (auto& [av_id, entry] : mEntryList)
    {
        entry->updateName();
    }
}

void FSRadar::updateName(const LLUUID& avatar_id)
{
    if (auto entry = getEntry(avatar_id))
    {
        entry->updateName();
    }
}

void FSRadar::updateAgeAlertCheck()
{
    for (auto& [av_id, entry] : mEntryList)
    {
        entry->checkAge();
    }
}

void FSRadar::updateNotes(const LLUUID& avatar_id, std::string_view notes)
{
    if (auto entry = getEntry(avatar_id))
    {
        entry->setNotes(notes);
    }
}

void FSRadar::onRegionChanged()
{
    if (mRegionCapabilitiesReceivedCallbackConnection.connected())
    {
        mRegionCapabilitiesReceivedCallbackConnection.disconnect();
    }

    if (auto region = gAgent.getRegion())
    {
        if (region->capabilitiesReceived())
        {
            for (auto& [id, entry] : mEntryList)
                entry->requestProperties();
        }
        else
        {
            mRegionCapabilitiesReceivedCallbackConnection = region->setCapabilitiesReceivedCallback(
                [this](const LLUUID&, LLViewerRegion*)
                {
                    mRegionCapabilitiesReceivedCallbackConnection.disconnect();
                    for (auto& [id, entry] : mEntryList)
                        entry->requestProperties();
                });
        }
    }
}

//=============================================================================
// Show Friends Only
//=============================================================================

void FSRadar::onShowFriendsOnlySettingChanged()
{
    setShowFriendsOnly(gSavedSettings.getBOOL("FSRadarShowFriendsOnly"));
}

void FSRadar::setShowFriendsOnly(bool enabled)
{
    if (mShowFriendsOnly == enabled)
    {
        return;
    }

    mShowFriendsOnly = enabled;

    if (mShowFriendsOnly)
    {
        applyShowFriendsOnly();
    }
    else
    {
        restoreShowFriendsOnly();
    }
}

void FSRadar::applyShowFriendsOnly()
{
    LLAvatarTracker& avatartracker = LLAvatarTracker::instance();
    ALDerenderList& derenderlist = ALDerenderList::instance();

    for (const auto& [av_id, entry] : mEntryList)
    {
        if (av_id == gAgentID || avatartracker.getBuddyInfo(av_id) != nullptr)
        {
            continue;
        }

        if (derenderlist.isDerendered(ALDerenderEntry::TYPE_AVATAR, av_id))
        {
            // Already hidden (muted / manually derendered / already tracked by us)
            if (std::find(mShowFriendsOnlyDerendered.begin(), mShowFriendsOnlyDerendered.end(), av_id) == mShowFriendsOnlyDerendered.end())
            {
                continue;
            }
        }

        if (derenderlist.addAvatar(av_id, entry->getName(), false /*not persistent*/))
        {
            mShowFriendsOnlyDerendered.push_back(av_id);
        }
    }
}

void FSRadar::restoreShowFriendsOnly()
{
    if (!mShowFriendsOnlyDerendered.empty())
    {
        ALDerenderList::instance().removeObjects(ALDerenderEntry::TYPE_AVATAR, mShowFriendsOnlyDerendered);
        mShowFriendsOnlyDerendered.clear();
    }
}

void FSRadar::onTeleportFinished(const LLVector3d& pos, const bool& local)
{
    if (mShowFriendsOnly && gSavedSettings.getBOOL("FSRadarShowFriendsOnlyAutoOff"))
    {
        gSavedSettings.setBOOL("FSRadarShowFriendsOnly", false); // fires onShowFriendsOnlySettingChanged() -> restoreShowFriendsOnly()
    }
}
