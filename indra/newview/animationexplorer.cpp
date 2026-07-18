/**
 * @file animationexplorer.cpp
 * @brief Animation Explorer floater implementation
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

// Donor: I:\enve (phoenix-reshade-XL / Firestorm, LGPL), indra/newview/animationexplorer.cpp.
// Ported for the BD/FS -> Alchemy merge campaign, item F7 (Animation Explorer).
//
// Adaptations vs. the donor (see the campaign report for the full list):
//  - FSAssetBlacklist -> ALAssetBlocklist (alassetblocklist.h); addEntry() takes
//    an avatar/source UUID rather than a display-name string, so the "played by"
//    object/avatar id is passed instead.
//  - gAgentAvatarp->revokePermissionsOnObject() does not exist in this tree;
//    replaced with the same two-permission-bit LLAgent::sendRevokePermissions()
//    call Alchemy itself uses in llvoavatar.cpp's revoke_permissions_on_object()
//    and llagent.cpp's LLAgent::stopCurrentAnimations().
//  - The donor's homegrown ObjectSelect/ObjectDeselect + requestNameCallback()
//    dance (hooked into FS's process_object_properties in llviewermessage.cpp,
//    which does not exist in this tree) is replaced with Alchemy's existing
//    ALObjectPropertiesCache (alobjectproperties.h), which already owns the
//    ObjectProperties/ObjectPropertiesFamily handlers registered in
//    llstartup.cpp. We just read the cache and subscribe to its change signal;
//    this drops the floater's own mRequestedIDs/mKnownIDs bookkeeping.
//  - Avatar "played by" names honor RLVa @shownames (RlvStrings::getAnonym),
//    matching ALFloaterExploreSounds's convention for the same column.
//  - Localized strings are declared as <floater.string> in
//    floater_animation_explorer.xml instead of the shared strings.xml, so
//    landing this floater doesn't require a shared-file edit; getString()
//    (LLPanel/LLFloater) is used instead of LLTrans::getString() for those.
//  - Fixed a donor bug: the avatar-name-cache callback registration guard was
//    inverted (`!=` where it should be `==`), which meant it only *skipped*
//    registering a duplicate callback when one was NOT already pending, i.e.
//    the opposite of its intent, and could double-register lookups whose
//    connection handle it then failed to keep.
// [BDMerge F7]

#include "llviewerprecompiledheaders.h"
#include "animationexplorer.h"

#include "indra_constants.h"
#include "message.h"                // for gMessageSystem
#include "llagent.h"                // for gAgent
#include "llagentdata.h"            // for gAgentID, gAgentSessionID
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llclipboard.h"            // [ActorMover] Copy UUID
#include "llfloater.h"
#include "llfloaterreg.h"
#include "lllineeditor.h"           // [ActorMover] UUID readout
#include "llmenugl.h"               // [ActorMover] list context menu
#include "lluictrlfactory.h"        // [ActorMover] list context menu
#include "llviewercontrol.h"        // [ActorMover] gSavedSettings handoff
#include "llviewermenu.h"           // [ActorMover] gMenuHolder
#include "llkeyframemotion.h"       // for LLKeyframeMotion (priority readout)
#include "llscrolllistctrl.h"
#include "lltimer.h"
#include "lltrans.h"
#include "lluuid.h"
#include "llview.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"         // for gAgentAvatarp
#include "llavatarnamecache.h"

#include "alpanelanimpreview.h"     // shared preview pane + own-avatar controls
#include "alobjectproperties.h"     // ALObjectPropertiesCache (name lookups)

#include "rlvhandler.h"             // @shownames anonymization, matches ALFloaterExploreSounds

constexpr S32 MAX_ANIMATIONS = 100;

// --------------------------------------------------------------------------

RecentAnimationList::RecentAnimationList()
:   LLSingleton<RecentAnimationList>()
{
}


void RecentAnimationList::addAnimation(const LLUUID& id, const LLUUID& playedBy)
{
    AnimationEntry entry;

    entry.animationID = id;
    entry.playedBy = playedBy;
    entry.time = LLTimer::getElapsedSeconds();

    AnimationExplorer* explorer = LLFloaterReg::findTypedInstance<AnimationExplorer>("animation_explorer");

    // only remember animation when it wasn't played by ourselves or the explorer window is open,
    // so the list doesn't get polluted
    if (playedBy != gAgentAvatarp->getID() || explorer)
    {
        mAnimationList.push_back(entry);

        // only keep a certain number of entries
        if (mAnimationList.size() > MAX_ANIMATIONS)
        {
            mAnimationList.pop_front();
        }
    }

    // if the animation explorer floater is open, send this animation over immediately
    if (explorer)
    {
        explorer->addAnimation(id, playedBy, LLTimer::getElapsedSeconds());
    }
}

void RecentAnimationList::requestList(AnimationExplorer* explorer)
{
    if (explorer)
    {
        // send the list of recent animations to the given animation explorer floater
        for (std::deque<AnimationEntry>::iterator iter = mAnimationList.begin();
            iter != mAnimationList.end();
            ++iter)
        {
            AnimationEntry entry = *iter;
            explorer->addAnimation(entry.animationID, entry.playedBy, entry.time);
        }
    }
}

// --------------------------------------------------------------------------

AnimationExplorer::AnimationExplorer(const LLSD& key)
:   LLFloater(key),
    mAnimationScrollList(nullptr),
    mNoOwnedAnimationsCheckBox(nullptr)
{
}

AnimationExplorer::~AnimationExplorer()
{
    // the shared preview panel owns and releases its own dummy avatar

    // [ActorMover] list context menu
    if (auto menu = mPopupMenuHandle.get())
    {
        menu->die();
        mPopupMenuHandle.markDead();
    }

    for (const auto& cb : mAvatarNameCacheConnections)
    {
        if (cb.second.connected())
        {
            cb.second.disconnect();
        }
    }
    mAvatarNameCacheConnections.clear();

    if (mObjectPropsConnection.connected())
    {
        mObjectPropsConnection.disconnect();
    }
}

bool AnimationExplorer::postBuild()
{
    mAnimationScrollList = getChild<LLScrollListCtrl>("animation_list");
    mNoOwnedAnimationsCheckBox = getChild<LLCheckBoxCtrl>("no_owned_animations_check");

    mAnimationScrollList->setCommitCallback(boost::bind(&AnimationExplorer::onSelectAnimation, this));
    mNoOwnedAnimationsCheckBox->setCommitCallback(boost::bind(&AnimationExplorer::onOwnedCheckToggled, this));

    // [ActorMover] UUID tools: readout line, copy/handoff buttons, and a
    // right-click context menu on the animation list (same LLContextMenu
    // idiom as LLFloaterBump's bump_list menu)
    mAnimUUIDEditor = getChild<LLLineEditor>("anim_uuid_editor");
    getChild<LLButton>("copy_uuid_btn")->setCommitCallback(boost::bind(&AnimationExplorer::onCopyUUIDPressed, this));
    getChild<LLButton>("to_actor_mover_btn")->setCommitCallback(boost::bind(&AnimationExplorer::onToActorMoverPressed, this));
    mAnimationScrollList->setRightMouseDownCallback(boost::bind(&AnimationExplorer::onScrollListRightClicked, this, _1, _2, _3));
    {
        LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
        registrar.add("AnimExplorer.CopyUUID", boost::bind(&AnimationExplorer::onCopyUUIDPressed, this));
        registrar.add("AnimExplorer.ToActorMover", boost::bind(&AnimationExplorer::onToActorMoverPressed, this));
        if (LLContextMenu* menu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>(
                "menu_animation_explorer.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance()))
        {
            mPopupMenuHandle = menu->getHandle();
        }
    }

    // Notified whenever an ObjectProperties/ObjectPropertiesFamily reply lands
    // for ANY object, so a pending "played by" name can resolve without the
    // floater polling for it.
    mObjectPropsConnection = ALObjectPropertiesCache::instance().setChangeCallback(
        boost::bind(&AnimationExplorer::onObjectPropsChanged, this, _1));

    // the preview pane, its mouse-drag-to-rotate, the auto-loop, and the
    // Stop / Stop-and-Revoke / Blacklist / Capture-all controls are the shared
    // ALPanelAnimPreview embedded in the floater XML; it owns its own dummy and
    // wiring. We just feed it the selected animation (onSelectAnimation).
    mPreviewPanel = findChild<ALPanelAnimPreview>("anim_preview_panel");
    if (!mPreviewPanel)
    {
        LL_WARNS("AnimationExplorer") << "Could not find embedded anim preview panel" << LL_ENDL;
    }

    // request list of recent animations
    update();

    return true;
}

void AnimationExplorer::onSelectAnimation()
{
    LLScrollListItem* item = mAnimationScrollList->getFirstSelected();
    if (!item)
    {
        return;
    }

    S32 column = mAnimationScrollList->getColumn("animation_id")->mIndex;
    mCurrentAnimationID = item->getColumn(column)->getValue().asUUID();

    column = mAnimationScrollList->getColumn("object_id")->mIndex;
    mCurrentObject = item->getColumn(column)->getValue().asUUID();

    // [ActorMover] keep the UUID readout in sync with the selection
    if (mAnimUUIDEditor)
    {
        mAnimUUIDEditor->setText(mCurrentAnimationID.asString());
    }

    // feed the shared preview panel: it loops the anim on its dummy and points
    // its Stop / Revoke / Blacklist at the object that played it
    if (mPreviewPanel)
    {
        mPreviewPanel->previewAnim(mCurrentAnimationID, mCurrentObject);
    }
}

// [ActorMover] copy the selected animation asset UUID to the clipboard
// (same LLClipboard idiom as alviewermenu.cpp's object_copy_key())
void AnimationExplorer::onCopyUUIDPressed()
{
    if (mCurrentAnimationID.isNull())
    {
        return;
    }
    LLWString idwstr = utf8string_to_wstring(mCurrentAnimationID.asString());
    LLClipboard::instance().copyToClipboard(idwstr, 0, narrow(idwstr.size()));
}

// [ActorMover] hand the selected animation to the Actor Mover as its custom
// locomotion anim, then surface the transport floater
void AnimationExplorer::onToActorMoverPressed()
{
    if (mCurrentAnimationID.isNull())
    {
        return;
    }
    gSavedSettings.setString("ActorMoverCustomAnim", mCurrentAnimationID.asString());
    gSavedSettings.setBOOL("ActorMoverUseCustomAnim", true);
    LLFloaterReg::showInstance("actor_mover");
}

// [ActorMover] right-click on the animation list: select the row under the
// cursor (so the menu acts on what was clicked) and pop the context menu
void AnimationExplorer::onScrollListRightClicked(LLUICtrl* ctrl, S32 x, S32 y)
{
    LLScrollListItem* item = mAnimationScrollList->hitItem(x, y);
    auto menu = mPopupMenuHandle.get();
    if (item && menu)
    {
        const S32 index = mAnimationScrollList->getItemIndex(item);
        if (index >= 0)
        {
            mAnimationScrollList->selectNthItem(index);
            onSelectAnimation();
        }
        menu->buildDrawLabels();
        menu->updateParent(LLMenuGL::sMenuContainer);
        menu->show(x, y);
        LLMenuGL::showPopup(ctrl, menu, x, y);
    }
}

void AnimationExplorer::onOwnedCheckToggled()
{
    update();
    updateList(LLTimer::getElapsedSeconds());
}

void AnimationExplorer::draw()
{
    LLFloater::draw();
    // the embedded ALPanelAnimPreview blits + spins its own dummy in its draw()

    // update times and "Still playing" status in the list once every few seconds
    static F64 last_update = 0.0;
    if (F64 time = LLTimer::getElapsedSeconds(); time - last_update > 5.0)
    {
        last_update = time;
        updateList(time);
    }
}

void AnimationExplorer::update()
{
    // stop previewing when reloading the list (the shared panel loops the dummy)
    if (mPreviewPanel)
    {
        mPreviewPanel->clearPreview();
    }

    mAnimationScrollList->deleteAllItems();
    RecentAnimationList::instance().requestList(this);
}

void AnimationExplorer::updateList(F64 current_timestamp)
{
    S32 played_column = mAnimationScrollList->getColumn("played")->mIndex;
    S32 timestamp_column = mAnimationScrollList->getColumn("timestamp")->mIndex;
    S32 priority_column = mAnimationScrollList->getColumn("priority")->mIndex;
    S32 object_id_column = mAnimationScrollList->getColumn("object_id")->mIndex;
    S32 anim_id_column = mAnimationScrollList->getColumn("animation_id")->mIndex;

    // go through the full animation scroll list
    for (auto item : mAnimationScrollList->getAllData())
    {
        // get a pointer to the "Played" column text
        LLScrollListText* played_text = dynamic_cast<LLScrollListText*>(item->getColumn(played_column));

        // get the object ID from the list
        LLUUID object_id = item->getColumn(object_id_column)->getValue().asUUID();

        LLUUID anim_id = item->getColumn(anim_id_column)->getValue().asUUID();

        // assume this animation is not running first
        bool is_running = false;

        // go through the list of playing animations to find out if this animation played by
        // this object is still running
        for (const auto& [anim_object_id, anim_anim_id] : gAgentAvatarp->mAnimationSources)
        {
            // object and animation found
            if (anim_object_id == object_id && anim_anim_id == anim_id)
            {
                // set text to "Still playing" and break out of this loop
                played_text->setText(getString("animation_explorer_still_playing"));
                is_running = true;
                break;
            }
        }

        // animation was not found to be running
        if (!is_running)
        {
            // get timestamp when this animation was started
            F64 timestamp = item->getColumn(timestamp_column)->getValue().asReal();

            // update text to show the number of seconds ago when this animation was started
            LLStringUtil::format_map_t args;
            args["SECONDS"] = llformat("%d", (S32) (current_timestamp - timestamp));

            played_text->setText(getString("animation_explorer_seconds_ago", args));
        }

        std::string prio_text = getString("animation_explorer_unknown_priority");
        if (LLKeyframeMotion* motion = dynamic_cast<LLKeyframeMotion*>(gAgentAvatarp->findMotion(anim_id)); motion)
        {
            prio_text = llformat("%d", (S32)motion->getPriority());
        }
        dynamic_cast<LLScrollListText*>(item->getColumn(priority_column))->setText(prio_text);
    }
}

void AnimationExplorer::addAnimation(const LLUUID& id, const LLUUID& played_by, F64 time)
{
    // don't add animations that are played by ourselves when the filter box is checked
    if (played_by == gAgentAvatarp->getID())
    {
        if (mNoOwnedAnimationsCheckBox->getValue().asBoolean())
        {
            return;
        }
    }

    // set object name to UUID at first
    std::string playedByName = played_by.asString();

    // find out if the object is still in reach
    if (LLViewerObject* vo = gObjectList.findObject(played_by); vo)
    {
        // if it was an avatar, get the name here
        if (vo->isAvatar())
        {
            if (LLAvatarName av_name; LLAvatarNameCache::get(played_by, &av_name))
            {
                // Honor @shownames, matching ALFloaterExploreSounds's "owner" column.
                playedByName = gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)
                    ? RlvStrings::getAnonym(av_name)
                    : av_name.getCompleteName();
            }
            else
            {
                // NOTE: fixed vs. the donor, which had this condition inverted
                // (`!=`) and so only registered a lookup when one was already
                // pending, leaking an un-trackable connection in the common case.
                if (mAvatarNameCacheConnections.find(played_by) == mAvatarNameCacheConnections.end())
                {
                    boost::signals2::connection cb_connection = LLAvatarNameCache::get(played_by, boost::bind(&AnimationExplorer::onAvatarNameCallback, this, _1, _2));
                    mAvatarNameCacheConnections.insert(std::make_pair(played_by, cb_connection));
                }

                playedByName = LLTrans::getString("AvatarNameWaiting");
            }
        }
        // not an avatar, do a lookup by UUID via Alchemy's shared object-properties cache
        else
        {
            const ALObjectPropertiesCache::ServerProps* props = ALObjectPropertiesCache::instance().get(played_by);
            if (props && !props->mName.empty())
            {
                playedByName = props->mName;
            }
            else
            {
                playedByName = LLTrans::getString("AvatarNameWaiting");
                requestObjectName(vo);
            }
        }
    }

    // insert the item into the scroll list
    LLSD item;
    item["columns"][0]["column"] = "played_by";
    item["columns"][0]["value"] = playedByName;
    item["columns"][1]["column"] = "played";
    item["columns"][1]["value"] = getString("animation_explorer_still_playing");
    item["columns"][2]["column"] = "priority";
    item["columns"][2]["value"] = getString("animation_explorer_unknown_priority");
    item["columns"][3]["column"] = "timestamp";
    item["columns"][3]["value"] = time;
    item["columns"][4]["column"] = "animation_id";
    item["columns"][4]["value"] = id;
    item["columns"][5]["column"] = "object_id";
    item["columns"][5]["value"] = played_by;

    mAnimationScrollList->addElement(item, ADD_TOP);
}

void AnimationExplorer::onAvatarNameCallback(const LLUUID& id, const LLAvatarName& av_name)
{
    if (auto iter = mAvatarNameCacheConnections.find(id); iter != mAvatarNameCacheConnections.end())
    {
        if (iter->second.connected())
        {
            iter->second.disconnect();
        }
        mAvatarNameCacheConnections.erase(iter);
    }

    std::string name = gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)
        ? RlvStrings::getAnonym(av_name)
        : av_name.getCompleteName();

    updateListEntry(id, name);
}

void AnimationExplorer::requestObjectName(LLViewerObject* vo)
{
    if (!vo || vo->isDead())
    {
        return;
    }

    ALObjectPropertiesCache& cache = ALObjectPropertiesCache::instance();

    const LLUUID& id = vo->getID();
    if (cache.isPending(id))
    {
        return;
    }

    // Never disturb the user's live selection: a raw ObjectDeselect for an
    // actively edited/selected object desyncs the simulator's selection state.
    if (vo->isSelected())
    {
        return;
    }

    LLViewerRegion* region = vo->getRegion();
    if (!region)
    {
        return;
    }

    // Bulk-select to provoke an ObjectProperties reply (captured by
    // ALObjectPropertiesCache), then immediately deselect so nothing stays
    // selected on the simulator. Mirrors ALFloaterSceneExplorer's
    // drainPropsQueue()/sendObjectSelectionMessage() for a single object.
    LLMessageSystem* msg = gMessageSystem;

    msg->newMessageFast(_PREHASH_ObjectSelect);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
    msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
    msg->nextBlockFast(_PREHASH_ObjectData);
    msg->addU32Fast(_PREHASH_ObjectLocalID, vo->getLocalID());
    msg->sendReliable(region->getHost());

    msg->newMessageFast(_PREHASH_ObjectDeselect);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
    msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
    msg->nextBlockFast(_PREHASH_ObjectData);
    msg->addU32Fast(_PREHASH_ObjectLocalID, vo->getLocalID());
    msg->sendReliable(region->getHost());

    cache.markPending(id);
}

void AnimationExplorer::onObjectPropsChanged(const LLUUID& id)
{
    const ALObjectPropertiesCache::ServerProps* props = ALObjectPropertiesCache::instance().get(id);
    if (!props || props->mName.empty())
    {
        return;
    }

    updateListEntry(id, props->mName);
}

void AnimationExplorer::updateListEntry(const LLUUID& id, const std::string& name)
{
    S32 object_id_column = mAnimationScrollList->getColumn("object_id")->mIndex;
    S32 played_by_column = mAnimationScrollList->getColumn("played_by")->mIndex;

    // find all scroll list entries with this object UUID and update the names there
    for (LLScrollListItem* item : mAnimationScrollList->getAllData())
    {
        const LLUUID& list_object_id = item->getColumn(object_id_column)->getValue().asUUID();

        if (id == list_object_id)
        {
            LLScrollListText* played_by_text = (LLScrollListText*)item->getColumn(played_by_column);
            played_by_text->setText(name);
        }
    }
}
