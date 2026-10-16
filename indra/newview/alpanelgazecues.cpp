/**
 * @file alpanelgazecues.cpp
 * @brief Director > Performance > Gaze Cues page logic -- see alpanelgazecues.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelgazecues.h"

#include "alfloatergazecues.h"
#include "alkithistory.h"
#include "alkitshell.h"
#include "aluirefresh.h"
#include "llactormover.h"
#include "llagent.h"
#include "llbutton.h"
#include "llfloaterreg.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"

#include <cmath>

// The injector name must match class="panel_gaze_cues" in the generated page XML.
static LLPanelInjector<ALPanelGazeCues> t_panel_gaze_cues("panel_gaze_cues");

ALPanelGazeCues::~ALPanelGazeCues()
{
    endPreview();
}

void ALPanelGazeCues::endPreview()
{
    // A preview started here must not outlive the page (hidden tab, closed Director, destroy).
    if (mPreviewActor.notNull() && LLActorMover::instanceExists())
    {
        LLActorMover::instance().clearGazeCuePreview(mPreviewActor);
    }
    mPreviewActor.setNull();
}

void ALPanelGazeCues::onVisibilityChange(bool new_visibility)
{
    if (!new_visibility)
    {
        endPreview();
    }
    LLPanel::onVisibilityChange(new_visibility);
}

bool ALPanelGazeCues::postBuild()
{
    mCueList = getChild<LLScrollListCtrl>("cue_list");
    mAddBtn = getChild<LLButton>("btn_cue_add");
    mRemoveBtn = getChild<LLButton>("btn_cue_delete");
    mPlayBtn = getChild<LLButton>("gaze_cue_play");
    mStopBtn = getChild<LLButton>("gaze_cue_stop");

    // A click only selects (so Remove / Play act on the row); a double-click opens the floater.
    mCueList->setCommitCallback([this](LLUICtrl*, const LLSD&) { refreshButtons(); });
    mCueList->setDoubleClickCallback([this]() { onOpen(); });
    mAddBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAdd(); });
    mRemoveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onRemove(); });
    mPlayBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPlay(); });
    mStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onStop(); });

    refreshRows(true);
    refreshButtons();
    return LLPanel::postBuild();
}

void ALPanelGazeCues::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    // Event-driven: the list rebuilds only when the cue revision or the actor changes; the
    // buttons are diff-guarded.
    refreshRows(false);
    refreshButtons();
    LLPanel::draw();
}

LLUUID ALPanelGazeCues::actorId() const
{
    if (ALKitShell* shell = ALKitShell::find(const_cast<ALPanelGazeCues*>(this)))
    {
        const ALKitShell::uuid_vec_t& selected = shell->castSelection();
        if (!selected.empty())
        {
            return selected.front();
        }
    }
    return gAgentID;
}

S32 ALPanelGazeCues::selectedCueIndex() const
{
    LLScrollListItem* item = mCueList ? mCueList->getFirstSelected() : nullptr;
    return item ? item->getValue().asInteger() : -1;
}

void ALPanelGazeCues::refreshRows(bool force)
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID actor = actorId();
    const U64 revision = cast.getGazeCueRevision();
    if (!force && mSeenRevision == revision && mSeenActor == actor)
    {
        return;
    }
    if (actor != mSeenActor)
    {
        endPreview(); // Stop only acts on the shown actor, so the old one's preview ends here
    }
    const S32 old_selection = actor == mSeenActor ? selectedCueIndex() : -1;
    mSeenRevision = revision;
    mSeenActor = actor;
    mCueList->deleteAllItems();
    const LLDirectorCast::GazeCueList& cues = cast.getGazeCues(actor);
    for (std::size_t i = 0; i < cues.size(); ++i)
    {
        LLSD row;
        row["value"] = static_cast<S32>(i);
        row["columns"][0]["column"] = "gc_time";
        row["columns"][0]["value"] = llformat("%.3f", cues[i].mStartSec);
        row["columns"][1]["column"] = "gc_target";
        row["columns"][1]["value"] = ALFloaterGazeCues::targetLabel(cues[i]);
        row["columns"][2]["column"] = "gc_macro";
        row["columns"][2]["value"] = ALFloaterGazeCues::macroLabel(cues[i].mMacro);
        mCueList->addElement(row, ADD_BOTTOM);
    }
    if (old_selection >= 0 && old_selection < mCueList->getItemCount())
    {
        mCueList->selectNthItem(old_selection);
    }
}

void ALPanelGazeCues::refreshButtons()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID actor = actorId();
    const LLDirectorCast::GazeCueList& cues = cast.getGazeCues(actor);
    const S32 index = selectedCueIndex();
    const bool valid = index >= 0 && index < static_cast<S32>(cues.size());
    const bool in_cast = cast.getMember(actor) != nullptr;
    const bool previewing = LLActorMover::instance().hasGazeCuePreview(actor);
    setEnabledIfChanged(mRemoveBtn, valid);
    setEnabledIfChanged(mPlayBtn, valid && in_cast);
    setEnabledIfChanged(mStopBtn, previewing);
    setToolTipIfChanged(mRemoveBtn, valid ? std::string("Remove the selected cue")
                                          : std::string("Select a cue first"));
    setToolTipIfChanged(mPlayBtn, !valid ? std::string("Select a cue first")
        : !in_cast ? std::string("Add this actor to the cast to preview a cue")
        : !cast.isLookAtCamera(actor)
            ? std::string("Preview the selected cue. It drives actors that have Look at camera on")
            : std::string("Preview the selected cue on this actor"));
    setToolTipIfChanged(mStopBtn, previewing ? std::string("End the cue preview and return to the live gaze target")
                                             : std::string("No cue preview is playing"));
}

void ALPanelGazeCues::onOpen()
{
    const LLUUID actor = actorId();
    const S32 index = selectedCueIndex();
    const LLDirectorCast::GazeCueList& cues = LLDirectorCast::instance().getGazeCues(actor);
    if (index < 0 || index >= static_cast<S32>(cues.size()))
    {
        return;
    }
    // One floater per actor (key = actor id); select the same cue there.
    const LLSD key(actor.asString());
    LLFloaterReg::showInstance("gaze_cues", key);
    if (ALFloaterGazeCues* floater = LLFloaterReg::findTypedInstance<ALFloaterGazeCues>("gaze_cues", key))
    {
        floater->selectCueIndex(index);
    }
}

void ALPanelGazeCues::commitListEdit(const LLUUID& actor, const LLDirectorCast::GazeCueList& after,
                                     const std::string& label)
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    // The edit renumbers cues, so a running preview would point at a different one.
    LLActorMover::instance().clearGazeCuePreview(actor);
    const LLDirectorCast::GazeCueList before = cast.getGazeCues(actor);
    cast.setGazeCues(actor, after);
    // setGazeCues sanitizes and sorts; the undo step stores what the controller actually holds.
    const LLDirectorCast::GazeCueList stored = cast.getGazeCues(actor);
    if (ALKitShell* shell = ALKitShell::find(this))
    {
        shell->history().recordCustom("gaze_cue_list", label,
            [actor, before, stored](bool redo, std::string& reason)
            {
                LLDirectorCast& cast = LLDirectorCast::instance();
                if (!cast.getMember(actor) && actor != gAgentID)
                {
                    reason = "That actor is no longer in the cast";
                    return false;
                }
                cast.setGazeCues(actor, redo ? stored : before);
                return true;
            });
    }
    refreshRows(true);
}

void ALPanelGazeCues::onAdd()
{
    const LLUUID actor = actorId();
    LLDirectorCast::GazeCueList cues = LLDirectorCast::instance().getGazeCues(actor);
    const LLDirectorCast::GazeCue cue = ALFloaterGazeCues::makeDefaultCue(actor);
    cues.push_back(cue);
    commitListEdit(actor, cues, "Add gaze cue");
    // The controller sorts by start time; select the cue we just added.
    const LLDirectorCast::GazeCueList& stored = LLDirectorCast::instance().getGazeCues(actor);
    for (S32 i = 0; i < static_cast<S32>(stored.size()); ++i)
    {
        if (std::fabs(stored[i].mStartSec - cue.mStartSec) <= 1e-6 && stored[i].mMacro == cue.mMacro)
        {
            mCueList->selectNthItem(i);
            break;
        }
    }
    refreshButtons();
}

void ALPanelGazeCues::onRemove()
{
    const LLUUID actor = actorId();
    const S32 index = selectedCueIndex();
    LLDirectorCast::GazeCueList cues = LLDirectorCast::instance().getGazeCues(actor);
    if (index < 0 || index >= static_cast<S32>(cues.size()))
    {
        return;
    }
    cues.erase(cues.begin() + index);
    commitListEdit(actor, cues, "Remove gaze cue");
    if (!cues.empty())
    {
        mCueList->selectNthItem(llmin(index, static_cast<S32>(cues.size()) - 1));
    }
    refreshButtons();
}

void ALPanelGazeCues::onPlay()
{
    const LLUUID actor = actorId();
    if (mPreviewActor.notNull() && mPreviewActor != actor)
    {
        endPreview();
    }
    if (LLActorMover::instance().setGazeCuePreview(actor, selectedCueIndex()))
    {
        mPreviewActor = actor;
    }
    refreshButtons();
}

void ALPanelGazeCues::onStop()
{
    LLActorMover::instance().clearGazeCuePreview(actorId());
    mPreviewActor.setNull();
    refreshButtons();
}
