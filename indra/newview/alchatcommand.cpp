/**
* @file alchatcommand.cpp
* @brief ALChatCommand implementation for chat input commands
*
* $LicenseInfo:firstyear=2013&license=viewerlgpl$
* Copyright (C) Rye Mutt <rye@alchemyviewer.org>
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
* $/LicenseInfo$
**/

#include "llviewerprecompiledheaders.h"

#include "alchatcommand.h"

// lib includes
#include "llcalc.h"
#include "llparcel.h"
#include "llstring.h"
#include "material_codes.h"
#include "object_flags.h"

// viewer includes
#include "aoengine.h"
#include "alobjectpathmover.h"  // [ObjectPath] /objpath* harness (props on splines)
#include "llactormover.h"       // [Pathing] /pathadd /pathwalk /pathclear /pathloop test harness
#include "llghostavatar.h"      // [GhostStudio] /ghosttest /ghostclear milestone-1 harness
#include "llclonefidelityaudit.h"   // [CloneFidelity] /clonefidelity source-vs-clone audit
#include "llagent.h"
#include "llagentcamera.h"
#include "llagentui.h"
#include "llcommandhandler.h"
#include "llfloaterimnearbychat.h"
#include "llfloaterreg.h"
#include "llfloaterregioninfo.h"
#include "llnotificationsutil.h"
#include "llregioninfomodel.h"
#include "llstartup.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"
#include "llviewernetwork.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llvolume.h"
#include "llvolumemessage.h"

// [ObjectPath] /objpath* feedback straight to nearby chat, so a command is
// never a silent no-op the operator has to dig out of the log (the mistake
// the prototype shipped with). Same local stand-in idiom as fsradar.cpp's
// radar_report_to_nearby_chat.
static void objpath_report(const std::string& message)
{
    if (LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLFloaterIMNearbyChat>("nearby_chat"))
    {
        LLChat chat;
        chat.mText = "[Prop Mover] " + message;
        chat.mSourceType = CHAT_SOURCE_SYSTEM;
        chat.mChatType = CHAT_TYPE_NORMAL;
        nearby_chat->addMessage(chat);
    }
}

bool ALChatCommand::parseCommand(std::string data)
{
    static LLCachedControl<bool> enableChatCmd(gSavedSettings, "AlchemyChatCommandEnable", true);
    if (enableChatCmd)
    {
        data = utf8str_tolower(data);   // [BDMerge fix] utf8str_tolower RETURNS the
        // lowercased string - it does not mutate in place. The result was being
        // discarded, so any capitalized command (/Calc, /PLAT) failed to match and
        // got sent as chat. Assign it back so commands are case-insensitive as intended.
        std::istringstream input(data);
        std::string cmd;

        if (!(input >> cmd))    return false;

        static LLCachedControl<std::string> sDrawDistanceCommand(gSavedSettings, "AlchemyChatCommandDrawDistance", "/dd");
        static LLCachedControl<std::string> sHeightCommand(gSavedSettings, "AlchemyChatCommandHeight", "/gth");
        static LLCachedControl<std::string> sGroundCommand(gSavedSettings, "AlchemyChatCommandGround", "/flr");
        static LLCachedControl<std::string> sPosCommand(gSavedSettings, "AlchemyChatCommandPos", "/pos");
        static LLCachedControl<std::string> sRezPlatCommand(gSavedSettings, "AlchemyChatCommandRezPlat", "/plat");
        static LLCachedControl<std::string> sHomeCommand(gSavedSettings, "AlchemyChatCommandHome", "/home");
        static LLCachedControl<std::string> sSetHomeCommand(gSavedSettings, "AlchemyChatCommandSetHome", "/sethome");
        static LLCachedControl<std::string> sCalcCommand(gSavedSettings, "AlchemyChatCommandCalc", "/calc");
        static LLCachedControl<std::string> sMaptoCommand(gSavedSettings, "AlchemyChatCommandMapto", "/mapto");
        static LLCachedControl<std::string> sClearCommand(gSavedSettings, "AlchemyChatCommandClearNearby", "/clr");
        static LLCachedControl<std::string> sRegionMsgCommand(gSavedSettings, "AlchemyChatCommandRegionMessage", "/regionmsg");
        static LLCachedControl<std::string> sSetNearbyChatChannelCmd(gSavedSettings, "AlchemyChatCommandSetChatChannel", "/setchannel");
        static LLCachedControl<std::string> sResyncAnimCommand(gSavedSettings, "AlchemyChatCommandResyncAnim", "/resync");
        static LLCachedControl<std::string> sTeleportToCam(gSavedSettings, "AlchemyChatCommandTeleportToCam", "/tp2cam");
        static LLCachedControl<std::string> sHoverHeight(gSavedSettings, "AlchemyChatCommandHoverHeight", "/hover");
        static LLCachedControl<std::string> sAOCommand(gSavedSettings, "AlchemyChatCommandAnimationOverride", "/ao");

        if (cmd == utf8str_tolower(sDrawDistanceCommand()))  // dd
        {
            F32 dist;
            if (input >> dist)
            {
                dist = llclamp(dist, 16.f, 512.f);
                gSavedSettings.setF32("RenderFarClip", dist);
                gAgentCamera.mDrawDistance = dist;
                return true;
            }
        }
        else if (cmd == utf8str_tolower(sHeightCommand()))  // gth
        {
            F64 z;
            if (input >> z)
            {
                LLVector3d pos_global = gAgent.getPositionGlobal();
                pos_global.mdV[VZ] = z;
                gAgent.teleportViaLocation(pos_global);
                return true;
            }
        }
        else if (cmd == utf8str_tolower(sGroundCommand()))  // flr
        {
            LLVector3d pos_global = gAgent.getPositionGlobal();
            pos_global.mdV[VZ] = 0.0;
            gAgent.teleportViaLocation(pos_global);
            return true;
        }
        else if (cmd == utf8str_tolower(sPosCommand()))  // pos
        {
            F64 x, y, z;
            if ((input >> x) && (input >> y) && (input >> z))
            {
                LLViewerRegion* regionp = gAgent.getRegion();
                if (regionp)
                {
                    LLVector3d target_pos = regionp->getPosGlobalFromRegion(LLVector3((F32) x, (F32) y, (F32) z));
                    gAgent.teleportViaLocation(target_pos);
                }
                return true;
            }
        }
        else if (cmd == utf8str_tolower(sRezPlatCommand()))  // plat
        {
            F32 size;
            if (!(input >> size))
                size = static_cast<F32>(gSavedSettings.getF32("AlchemyChatCommandRezPlatSize"));

            const LLVector3& agent_pos = gAgent.getPositionAgent();
            const LLVector3 rez_pos(agent_pos.mV[VX], agent_pos.mV[VY], agent_pos.mV[VZ] - ((gAgentAvatarp->getScale().mV[VZ] / 2.f) + 0.25f + (gAgent.getVelocity().magVec() * 0.333f)));

            LLMessageSystem* msg = gMessageSystem;
            msg->newMessageFast(_PREHASH_ObjectAdd);
            msg->nextBlockFast(_PREHASH_AgentData);
            msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
            msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
            LLUUID group_id = gAgent.getGroupForRezzing();
            msg->addUUIDFast(_PREHASH_GroupID, group_id);
            msg->nextBlockFast(_PREHASH_ObjectData);
            msg->addU8Fast(_PREHASH_PCode, LL_PCODE_VOLUME);
            msg->addU8Fast(_PREHASH_Material, LL_MCODE_STONE);
            msg->addU32Fast(_PREHASH_AddFlags, agent_pos.mV[VZ] > 4096.f ? FLAGS_CREATE_SELECTED : 0U);

            LLVolumeParams volume_params;
            volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
            volume_params.setBeginAndEndS(0.f, 1.f);
            volume_params.setBeginAndEndT(0.f, 1.f);
            volume_params.setRatio(1.f, 1.f);
            volume_params.setShear(0.f, 0.f);
            LLVolumeMessage::packVolumeParams(&volume_params, msg);

            msg->addVector3Fast(_PREHASH_Scale, LLVector3(size, size, 0.25f));
            msg->addQuatFast(_PREHASH_Rotation, LLQuaternion());
            msg->addVector3Fast(_PREHASH_RayStart, rez_pos);
            msg->addVector3Fast(_PREHASH_RayEnd, rez_pos);
            msg->addUUIDFast(_PREHASH_RayTargetID, LLUUID::null);
            msg->addU8Fast(_PREHASH_BypassRaycast, true);
            msg->addU8Fast(_PREHASH_RayEndIsIntersection, false);
            msg->addU8Fast(_PREHASH_State, false);
            msg->sendReliable(gAgent.getRegionHost());

            return true;
        }
        else if (cmd == utf8str_tolower(sHomeCommand()))  // home
        {
            gAgent.teleportHome();
            return true;
        }
        else if (cmd == utf8str_tolower(sSetHomeCommand()))  // sethome
        {
            gAgent.setStartPosition(START_LOCATION_ID_HOME);
            return true;
        }
        else if (cmd == utf8str_tolower(sCalcCommand()))  // calc
        {
            if (data.length() > cmd.length() + 1)
            {
                F32 result = 0.f;
                std::string expr = data.substr(cmd.length() + 1);
                LLStringUtil::toUpper(expr);
                if (LLCalc::getInstance()->evalString(expr, result))
                {
                    LLSD args;
                    args["EXPRESSION"] = expr;
                    args["RESULT"] = result;
                    LLNotificationsUtil::add("ChatCommandCalc", args);
                    return true;
                }
                LLNotificationsUtil::add("ChatCommandCalcFailed");
                return true;
            }
        }
        else if (cmd == utf8str_tolower(sMaptoCommand()))  // mapto
        {
            const std::string::size_type length = cmd.length() + 1;
            if (data.length() > length)
            {
                const LLVector3d& pos = gAgent.getPositionGlobal();
                LLSD params;
                params.append(data.substr(length));
                params.append(fmodf(static_cast<F32>(pos.mdV[VX]), REGION_WIDTH_METERS));
                params.append(fmodf(static_cast<F32>(pos.mdV[VY]), REGION_WIDTH_METERS));
                params.append(static_cast<F32>(pos.mdV[VZ]));
                LLCommandDispatcher::dispatch("teleport", params, LLSD(), LLGridManager::getInstance()->getGrid(), nullptr, "clicked", true);
                return true;
            }
        }
        else if (cmd == utf8str_tolower(sClearCommand()))
        {
            LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::findTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
            if (nearby_chat)
            {
                nearby_chat->reloadMessages(true);
            }
            return true;
        }
        else if (cmd == "/droll")
        {
            S32 dice_sides;
            if (!(input >> dice_sides))
                dice_sides = 6;
            LLSD args;
            args["RESULT"] = (ll_rand(dice_sides) + 1);
            LLNotificationsUtil::add("ChatCommandDiceRoll", args);
            return true;
        }
        else if (cmd == utf8str_tolower(sRegionMsgCommand())) // Region Message / Dialog
        {
            if (data.length() > cmd.length() + 1)
            {
                std::string notification_message = data.substr(cmd.length() + 1);
                std::vector<std::string> strings(5, "-1");
                // [0] grid_x, unused here
                // [1] grid_y, unused here
                strings[2] = gAgentID.asString(); // [2] agent_id of sender
                // [3] senter name
                std::string name;
                LLAgentUI::buildFullname(name);
                strings[3] = name;
                strings[4] = notification_message; // [4] message
                LLRegionInfoModel::sendEstateOwnerMessage(gMessageSystem, "simulatormessage", LLFloaterRegionInfo::getLastInvoice(), strings);
                return true;
            }
        }
        else if (cmd == utf8str_tolower(sSetNearbyChatChannelCmd()))  // Set nearby chat channel
        {
            S32 chan;
            if (input >> chan)
            {
                gSavedSettings.setS32("AlchemyNearbyChatChannel", chan);
                return true;
            }
        }
        else if (cmd == utf8str_tolower(sTeleportToCam()))
        {
            gAgent.teleportViaLocation(gAgentCamera.getCameraPositionGlobal());
            return true;
        }
        else if (cmd == utf8str_tolower(sHoverHeight()))  // Hover height
        {
            F32 height;
            if (input >> height)
            {
                gSavedPerAccountSettings.set("AvatarHoverOffsetZ",
                                             llclamp<F32>(height, MIN_HOVER_Z, MAX_HOVER_Z));
                return true;
            }
        }
        else if (cmd == utf8str_tolower(sResyncAnimCommand()))  // Resync Animations
        {
            for (S32 i = 0; i < gObjectList.getNumObjects(); i++)
            {
                LLViewerObject* object = gObjectList.getObject(i);
                if (object && object->isAvatar())
                {
                    LLVOAvatar* avatarp = (LLVOAvatar*)object;
                    if (avatarp)
                    {
                        for (const std::pair<LLUUID, S32> playpair : avatarp->mPlayingAnimations)
                        {
                            avatarp->stopMotion(playpair.first, true);
                            avatarp->startMotion(playpair.first);
                        }
                    }
                }
            }
            return true;
        }
        else if (cmd == utf8str_tolower(sAOCommand()))
        {
            std::string subcmd;
            if (input >> subcmd)
            {
                if (subcmd == "on")
                {
                    gSavedPerAccountSettings.setBOOL("AlchemyAOEnable", true);
                    return true;
                }
                else if (subcmd == "off")
                {
                    gSavedPerAccountSettings.setBOOL("AlchemyAOEnable", false);
                    return true;
                }
                else if (subcmd == "sit")
                {
                    auto ao_set = AOEngine::instance().getSetByName(AOEngine::instance().getCurrentSetName());
                    if (input >> subcmd)
                    {
                        if (subcmd == "on")
                        {
                            AOEngine::instance().setOverrideSits(ao_set, true);

                        }
                        else if (subcmd == "off")
                        {
                            AOEngine::instance().setOverrideSits(ao_set, false);
                        }
                    }
                    else
                    {
                        AOEngine::instance().setOverrideSits(ao_set, !ao_set->getSitOverride());
                    }
                    return true;
                }
            }
        }
        else if (cmd == "/sendmenu")
        {
            S32 channel;
            if (!(input >> channel))
                return false;
            std::string button;
            if (!(input >> button))
                return false;
            LLMessageSystem* msg = gMessageSystem;
            msg->newMessageFast(_PREHASH_ScriptDialogReply);
            msg->nextBlockFast(_PREHASH_AgentData);
            msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
            msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
            msg->nextBlockFast(_PREHASH_Data);
            msg->addUUIDFast(_PREHASH_ObjectID, gAgent.getID());
            msg->addS32(_PREHASH_ChatChannel, channel);
            msg->addS32Fast(_PREHASH_ButtonIndex, 0);
            msg->addStringFast(_PREHASH_ButtonLabel, button);
            gAgent.sendReliableMessage();
            return true;
        }
        // -------------------------------------------------------------------
        // [Pathing] Actor pathing P1 test harness (until the P2 in-world
        // editor exists). All four operate on MY avatar's session path, so a
        // solo operator can seed, walk, and clear a 3D Catmull-Rom path and
        // A/B the engine (slopes, stairs, turning) before any UI lands. A path
        // with >= 2 nodes makes the Actor Mover Walk button curve along it;
        // 0/1 node falls back to the legacy straight walk.
        // -------------------------------------------------------------------
        else if (cmd == "/pathadd")     // append a ground-snapped waypoint here
        {
            LLActorMover::instance().appendWaypointHere(LLUUID::null);
            return true;
        }
        else if (cmd == "/pathwalk")    // walk my path (needs >= 2 nodes)
        {
            if (LLActorMover::instance().hasWalkablePath(LLUUID::null))
            {
                LLActorMover::instance().start(LLUUID::null);
            }
            else
            {
                LL_WARNS("ActorMover") << "/pathwalk: fewer than 2 waypoints; "
                                          "use /pathadd first" << LL_ENDL;
            }
            return true;
        }
        // -------------------------------------------------------------------
        // [GhostStudio] Scene-lit clone milestone-1 acceptance gate. Spawns
        // two LLGhostAvatars from my avatar, poses them differently, and
        // checks they hold DISTINCT matrix palettes for the SAME shared skin.
        // That is the load-bearing proof for the entity architecture. Watch
        // the log under the "GhostStudio" tag. Requires rigged mesh worn.
        // -------------------------------------------------------------------
        else if (cmd == "/ghosttest")
        {
            LLGhostAvatar::runPaletteIsolationTest();
            return true;
        }
        else if (cmd == "/ghostdress")  // ONE dressed clone in front of me
        {
            LLGhostAvatar::spawnDressedGhost();
            return true;
        }
        else if (cmd == "/ghostverify") // face-level check, run AFTER /ghostdress
        {
            LLGhostAvatar::verifyClonedAttachments();
            return true;
        }
        else if (cmd == "/ghostclear")  // release the test ghosts
        {
            LLGhostAvatar::clearTestGhosts();
            return true;
        }
        else if (cmd == "/clonefidelity")   // [CloneFidelity] source-vs-clone data audit
        {
            // /clonefidelity            -> selected, else nearest enabled clone
            // /clonefidelity all        -> every enabled clone instance
            // /clonefidelity <uuid>     -> a specific instance
            std::string argument;
            input >> argument;
            std::vector<LLUUID> targets;
            if (!LLCloneFidelityAudit::instance().selectTargets(argument, targets))
            {
                LLCloneFidelityAudit::report("No enabled Clone-style instance matched the request.");
                return true;
            }
            LLCloneFidelityAudit::instance().arm(targets);
            return true;
        }
        else if (cmd == "/pathclear")   // stop + drop all my waypoints
        {
            LLActorMover::instance().stop(LLUUID::null);
            LLActorMover::instance().clearPath(LLUUID::null);
            return true;
        }
        else if (cmd == "/pathloop")    // toggle loop end-mode on my path
        {
            LLActorMover::Path& path = LLActorMover::instance().editPath(LLUUID::null);
            path.mEndMode = (path.mEndMode == 1) ? 0 : 1;    // loop <-> stop
            path.markDirty();   // loop changes the segment count -> arc table
            LL_INFOS("ActorMover") << "/pathloop: end mode now "
                                   << (path.mEndMode == 1 ? "loop" : "stop") << LL_ENDL;
            return true;
        }
        // -------------------------------------------------------------------
        // [ObjectPath] object path mover harness (prototype UI; see
        // doc/OBJECT_PATHING.md). Enroll props via right-click > Director >
        // "Path Object"; these commands then author + drive EVERY enrolled
        // object at once (each object keeps its own path, so a two-car chase
        // still authors per object as they sit staged apart).
        //   /objpathadd            append a node at each object's current spot
        //   /objpathdrive          drive every enrolled object with a path
        //   /objpathstop           release every drive (objects stay put)
        //   /objpathclear          stop + drop every enrolled object's nodes
        //   /objpathloop           toggle loop end-mode on the enrolled paths
        //   /objpathspeed <m/s>    set the enrolled paths' nominal speed
        //   /objpathskid <deg>     set the LAST node's skid yaw offset
        // -------------------------------------------------------------------
        else if (cmd == "/objpathadd")
        {
            ALObjectPathMover& opm = ALObjectPathMover::instance();
            if (opm.getRoster().empty())
            {
                objpath_report("No props enrolled. Right-click an object > Director > "
                               "Path Object, or use World > Prop Mover.");
                return true;
            }
            S32 added = 0;
            for (const LLUUID& id : opm.getRoster())
            {
                added += opm.appendWaypointHere(id) ? 1 : 0;
            }
            objpath_report(added
                ? llformat("Node added to %d prop%s.", added, added == 1 ? "" : "s")
                : "No enrolled prop is resolvable right now (out of view or derezzed).");
            return true;
        }
        else if (cmd == "/objpathdrive")
        {
            ALObjectPathMover& opm = ALObjectPathMover::instance();
            if (opm.getRoster().empty())
            {
                objpath_report("No props enrolled. Right-click an object > Director > "
                               "Path Object, or use World > Prop Mover.");
                return true;
            }
            opm.startAll();
            S32 driving = 0;
            for (const LLUUID& id : opm.getRoster())
            {
                driving += opm.isDriving(id) ? 1 : 0;
            }
            objpath_report(driving
                ? llformat("Driving %d prop%s.", driving, driving == 1 ? "" : "s")
                : "Nothing drivable: each prop needs at least 2 nodes (/objpathadd).");
            return true;
        }
        else if (cmd == "/objpathstop")
        {
            ALObjectPathMover::instance().stopAll();
            objpath_report("All drives stopped.");
            return true;
        }
        else if (cmd == "/objpathclear")
        {
            ALObjectPathMover& opm = ALObjectPathMover::instance();
            opm.stopAll();
            for (const LLUUID& id : opm.getRoster())
            {
                LLActorMover::instance().clearPath(id);
            }
            objpath_report("All prop paths cleared (props stay enrolled).");
            return true;
        }
        else if (cmd == "/objpathloop")
        {
            S32 looping = 0;
            for (const LLUUID& id : ALObjectPathMover::instance().getRoster())
            {
                LLActorMover::Path& path = LLActorMover::instance().editPath(id);
                path.mEndMode = (path.mEndMode == 1) ? 0 : 1;
                path.markDirty();
                looping += (path.mEndMode == 1) ? 1 : 0;
            }
            objpath_report(llformat("Loop toggled: %d prop path%s now loop.",
                                    looping, looping == 1 ? "" : "s"));
            return true;
        }
        else if (cmd == "/objpathspeed")
        {
            F32 speed;
            if (input >> speed)
            {
                speed = llclamp(speed, 0.05f, 50.f);
                for (const LLUUID& id : ALObjectPathMover::instance().getRoster())
                {
                    LLActorMover::Path& path = LLActorMover::instance().editPath(id);
                    path.mSpeed = speed;
                    path.markDirty();
                }
                objpath_report(llformat("Path speed set to %.2f m/s on every enrolled prop.", speed));
            }
            else
            {
                objpath_report("Usage: /objpathspeed <m/s>");
            }
            return true;
        }
        else if (cmd == "/objpathskid")
        {
            F32 deg;
            if (input >> deg)
            {
                S32 set = 0;
                LLActorMover& mover = LLActorMover::instance();
                for (const LLUUID& id : ALObjectPathMover::instance().getRoster())
                {
                    const LLActorMover::Path* path = mover.getPath(id);
                    if (path && !path->mNodes.empty())
                    {
                        mover.setNodeYawOffset(id, (S32)path->mNodes.size() - 1,
                                               deg * DEG_TO_RAD);
                        ++set;
                    }
                }
                objpath_report(set
                    ? llformat("Skid %.0f deg set on the last node of %d path%s.",
                               deg, set, set == 1 ? "" : "s")
                    : "No prop has nodes yet (/objpathadd first).");
            }
            else
            {
                objpath_report("Usage: /objpathskid <degrees>");
            }
            return true;
        }
    }
    return false;
}
