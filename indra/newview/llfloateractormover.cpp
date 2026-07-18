/**
 * @file llfloateractormover.cpp
 * @brief Transport floater for the Actor Mover (local ghost locomotion).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llfloateractormover.h"

#include "llactormover.h"
#include "alpanelactormover.h"
#include "alpanelpatheditor.h"
#include "llavatarnamecache.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "llviewerobjectlist.h"     // gObjectList
#include "llvoavatar.h"
#include "llvoavatarself.h"         // gAgentAvatarp, isAgentAvatarValid()

LLFloaterActorMover::LLFloaterActorMover(const LLSD& key)
:   LLFloater(key)
{
}

bool LLFloaterActorMover::postBuild()
{
    mRosterList = getChild<LLScrollListCtrl>("roster_list");
    // the transport (scope, heading dial, params, Walk/Stop) is the shared
    // ALPanelActorMover the Director Console Move tab also embeds; the roster
    // list above stays this floater's own presentation
    mMoverPanel = findChild<ALPanelActorMover>("actor_mover_panel");
    mPathPanel = findChild<ALPanelPathEditor>("path_editor");
    return true;
}

LLUUID LLFloaterActorMover::selectedActor() const
{
    LLScrollListItem* item = mRosterList->getFirstSelected();
    return item ? item->getValue().asUUID() : LLUUID::null;
}

void LLFloaterActorMover::refreshRoster()
{
    // effective roster: explicit members, or my avatar when empty
    uuid_vec_t roster = LLActorMover::getRoster();
    const bool implicit_self = roster.empty();
    if (implicit_self)
    {
        if (!isAgentAvatarValid())
        {
            mRosterList->deleteAllItems();
            return;
        }
        roster.push_back(gAgentAvatarp->getID());
    }

    // membership change -> rebuild rows (rare); otherwise only re-set text
    std::vector<LLScrollListItem*> items = mRosterList->getAllData();
    bool rebuild = items.size() != roster.size();
    if (!rebuild)
    {
        for (size_t i = 0; i < roster.size(); ++i)
        {
            if (items[i]->getValue().asUUID() != roster[i])
            {
                rebuild = true;
                break;
            }
        }
    }
    if (rebuild)
    {
        const LLUUID prev_sel = selectedActor();
        mRosterList->deleteAllItems();
        for (const LLUUID& id : roster)
        {
            LLSD row;
            row["value"] = id;
            row["columns"][0]["column"] = "name";
            row["columns"][0]["value"] = "";
            row["columns"][1]["column"] = "status";
            row["columns"][1]["value"] = "";
            mRosterList->addElement(row, ADD_BOTTOM);
        }
        if (prev_sel.notNull())
        {
            mRosterList->selectByID(prev_sel);
        }
        items = mRosterList->getAllData();
    }

    const S32 name_col = mRosterList->getColumn("name")->mIndex;
    const S32 status_col = mRosterList->getColumn("status")->mIndex;
    for (LLScrollListItem* item : items)
    {
        const LLUUID id = item->getValue().asUUID();

        // display name; fall back to UUID (or an animesh tag) while pending
        std::string name;
        if (LLAvatarName av_name; LLAvatarNameCache::get(id, &av_name))
        {
            name = av_name.getCompleteName();
        }
        else
        {
            LLViewerObject* obj = gObjectList.findObject(id);
            LLVOAvatar* av = obj ? obj->asAvatar() : nullptr;
            name = (av && av->isControlAvatar())
                ? "Animesh " + id.asString().substr(0, 8)
                : id.asString();
        }
        if (implicit_self)
        {
            name += " (you)";
        }

        F32 traveled = 0.f, total = 0.f;
        std::string status;
        if (LLActorMover::instance().isWalkSuspended(id))
        {
            // TP-away: the walk is frozen (actor derezzed / left region /
            // teleported), held for a smart resume rather than shown as moving
            status = "Suspended";
        }
        else if (LLActorMover::instance().getProgress(id, traveled, total))
        {
            status = llformat("Moving %.1f/%.1f m", traveled, total);
        }
        else
        {
            status = "Idle";
        }

        // only touch the cells when the text actually changed
        if (LLScrollListText* cell = dynamic_cast<LLScrollListText*>(item->getColumn(name_col));
            cell && cell->getValue().asString() != name)
        {
            cell->setText(name);
        }
        if (LLScrollListText* cell = dynamic_cast<LLScrollListText*>(item->getColumn(status_col));
            cell && cell->getValue().asString() != status)
        {
            cell->setText(status);
        }
    }
}

void LLFloaterActorMover::draw()
{
    refreshRoster();

    // live transport state readout
    LLTextBox* status = getChild<LLTextBox>("status_text");
    if (status)
    {
        status->setText(LLActorMover::instance().anyMoving()
                            ? std::string("Moving (local only)")
                            : std::string("Idle"));
    }

    // feed the shared transport panel this floater's selection: the one roster
    // row that is selected (empty when none). The panel's Walk/Stop honor
    // ActorMoverSync -- Everyone drives startAll/stopAll and ignores this; else
    // it acts on exactly this set, so the buttons behave identically to the
    // console Move tab (which feeds its cast-list selection the same way).
    if (mMoverPanel)
    {
        uuid_vec_t sel;
        if (const LLUUID id = selectedActor(); id.notNull())
        {
            sel.push_back(id);
        }
        mMoverPanel->setSelectedActors(sel);
    }

    // point the shared path editor at the selected roster actor (implicit self
    // resolves to a concrete id in refreshRoster, so this is never null-for-self)
    if (mPathPanel)
    {
        mPathPanel->setTargetActor(selectedActor());
    }

    LLFloater::draw();
}
