/**
 * @file alpanellightcues.cpp
 * @brief Director > Light > Light Cues page logic -- see alpanellightcues.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanellightcues.h"

#include "alcinelightrig.h"
#include "alcinelightrigmanager.h"
#include "alkithistory.h"
#include "alkitshell.h"
#include "aluirefresh.h"
#include "llbutton.h"
#include "llfloaterreg.h"
#include "llnotificationsutil.h"
#include "llsdutil.h"
#include "llpresentationtime.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"

#include <algorithm>
#include <memory>

// The injector name must match class="panel_light_cues" in the generated page XML.
static LLPanelInjector<ALPanelLightCues> t_panel_light_cues("panel_light_cues");

namespace
{
using namespace ALCineLightRigModel;

F64 presentationTime()
{
    return std::max(LLPresentationTime::currentFrame().presentation_time, 0.0);
}

const char* cueProfileName(S32 profile)
{
    switch (profile)
    {
        case CUE_FADE_LINEAR: return "Linear";
        case CUE_FADE_SNAP:   return "Snap";
        case CUE_FADE_EASE:
        default:              return "Ease";
    }
}
} // anonymous namespace

bool ALPanelLightCues::postBuild()
{
    mCueRows = getChild<LLScrollListCtrl>("cue_rows");
    mAddBtn = getChild<LLButton>("cue_capture");
    mRemoveBtn = getChild<LLButton>("cue_delete");
    mPlayBtn = getChild<LLButton>("cue_goto");
    mStopBtn = getChild<LLButton>("cue_release");

    // A click only selects (so Remove / Play act on the row); a double-click opens the floater.
    mCueRows->setCommitCallback([this](LLUICtrl*, const LLSD&) { refreshButtons(); });
    mCueRows->setDoubleClickCallback([this]() { onOpen(); });
    mAddBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAdd(); });
    mRemoveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onRemove(); });
    mPlayBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPlay(); });
    mStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onStop(); });

    refreshRows(true);
    refreshButtons();
    return LLPanel::postBuild();
}

void ALPanelLightCues::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    // The cue list changes through the floater, a rig-slot switch or a list load; the revision
    // and slot compare is the whole per-frame cost.
    refreshRows(false);
    refreshButtons();
    LLPanel::draw();
}

S32 ALPanelLightCues::selectedCueIndex() const
{
    LLScrollListItem* item = mCueRows ? mCueRows->getFirstSelected() : nullptr;
    return item ? item->getValue().asInteger() : -1;
}

void ALPanelLightCues::refreshRows(bool force)
{
    ALCineLightRigManager& manager = ALCineLightRigManager::instance();
    ALCineLightRig& rig = manager.selected();
    const S32 slot = static_cast<S32>(manager.selectedSlot());
    if (!force && mSeenRevision == rig.cueRevision() && mSeenSlot == slot)
    {
        return;
    }
    mSeenRevision = rig.cueRevision();
    mSeenSlot = slot;
    const S32 old_selection = selectedCueIndex();
    mCueRows->deleteAllItems();
    const CueList& list = rig.cueList();
    for (S32 i = 0; i < static_cast<S32>(list.mCues.size()); ++i)
    {
        const Cue& cue = list.mCues[i];
        LLSD row;
        row["value"] = i;
        row["columns"][0]["column"] = "lc_number";
        row["columns"][0]["value"] = llformat("%d", i + 1);
        row["columns"][1]["column"] = "lc_label";
        row["columns"][1]["value"] = cue.mLabel;
        row["columns"][2]["column"] = "lc_at";
        row["columns"][2]["value"] = llformat("%.3f", cue.mAtSec);
        row["columns"][3]["column"] = "lc_fade";
        row["columns"][3]["value"] = llformat("%.2f", cue.mFadeSec);
        row["columns"][4]["column"] = "lc_delay";
        row["columns"][4]["value"] = llformat("%.2f", cue.mDelaySec);
        row["columns"][5]["column"] = "lc_curve";
        row["columns"][5]["value"] = cueProfileName(cue.mProfile);
        row["columns"][6]["column"] = "lc_follow";
        row["columns"][6]["value"] = cue.mFollow < 0 ? "-" : llformat("%d", cue.mFollow);
        mCueRows->addElement(row, ADD_BOTTOM);
    }
    if (old_selection >= 0 && old_selection < mCueRows->getItemCount())
    {
        mCueRows->selectNthItem(old_selection);
    }
}

void ALPanelLightCues::refreshButtons()
{
    const CueList& list = ALCineLightRigManager::instance().selected().cueList();
    const S32 index = selectedCueIndex();
    const bool valid = index >= 0 && index < static_cast<S32>(list.mCues.size());
    // Timecode mode evaluates cues from the presentation clock, so live transport stays off,
    // exactly as in the Lighting Cue Stack floater.
    const bool live_transport = !list.mTimecodeMode;
    setEnabledIfChanged(mRemoveBtn, valid);
    setEnabledIfChanged(mPlayBtn, valid && live_transport);
    setEnabledIfChanged(mStopBtn, live_transport);
    setToolTipIfChanged(mPlayBtn, !live_transport
        ? std::string("Timecode mode plays cues from the clock; turn it off in the Lighting Cue Stack to use Play")
        : valid ? std::string("Run the selected cue through the cue transport")
                : std::string("Select a cue first"));
    setToolTipIfChanged(mRemoveBtn, valid ? std::string("Remove the selected cue")
                                          : std::string("Select a cue first"));
}

void ALPanelLightCues::onOpen()
{
    const S32 index = selectedCueIndex();
    const CueList& list = ALCineLightRigManager::instance().selected().cueList();
    if (index < 0 || index >= static_cast<S32>(list.mCues.size()))
    {
        return;
    }
    // The floater is single-instance; its onOpen() selects this cue and loads the editor.
    LLFloaterReg::showInstance("cine_light_cues", LLSD(index));
}

void ALPanelLightCues::commitListEdit(const CueList& after, const std::string& label)
{
    ALCineLightRigManager& manager = ALCineLightRigManager::instance();
    ALCineLightRig& rig = manager.selected();
    const CueList before = rig.cueList();
    const LLSD before_data = rig.cueListData();
    const ALCineLightRigSlot slot = manager.selectedSlot();
    rig.setCueList(after);
    // Read back the sanitized list so the later content compare sees what the rig really holds.
    const LLSD after_data = rig.cueListData();
    if (ALKitShell* shell = ALKitShell::find(this))
    {
        // The step restores a whole-list snapshot, so it is only valid while the list still has
        // the content this step (or its own undo/redo) left it with. Staleness is decided by
        // CONTENT, not by cue revision: every setCueList bumps the revision, so a per-step
        // revision would mark the second undo of a chain as stale. Undo applies only when the
        // current list equals this step's after-list, redo only when it equals the before-list;
        // otherwise the Lighting Cue Stack floater (or a load/save) has edited the list since,
        // and the step is dropped as a no-op with a notice instead of overwriting those edits.
        shell->history().recordCustom(this, "light_cue_list", label,
            [slot, before, after, before_data, after_data](bool redo, std::string& reason)
            {
                if (!ALCineLightRigManager::instanceExists())
                {
                    reason = "The light rig is gone";
                    return false;
                }
                ALCineLightRig& target = ALCineLightRigManager::instance().at(slot);
                const LLSD& expected = redo ? before_data : after_data;
                if (!llsd_equals(target.cueListData(), expected))
                {
                    LLSD args;
                    args["MESSAGE"] = "The light cues were edited elsewhere since this step, so it was dropped";
                    LLNotificationsUtil::add("SystemMessageTip", args);
                    return true; // consumed as a no-op: it must not block earlier undo steps
                }
                target.setCueList(redo ? after : before);
                return true;
            });
    }
    refreshRows(true);
}

void ALPanelLightCues::onAdd()
{
    ALCineLightRig& rig = ALCineLightRigManager::instance().selected();
    CueList list = rig.cueList();
    // A complete snapshot of the live rig with the default timing; detailed timing is edited in
    // the Lighting Cue Stack.
    const Cue cue = sanitizeCue(rig.captureCue(llformat("Cue %d", static_cast<S32>(list.mCues.size()) + 1)));
    list.mCues.push_back(cue);
    commitListEdit(list, "Add light cue");
    mCueRows->selectNthItem(static_cast<S32>(list.mCues.size()) - 1);
    refreshButtons();
}

void ALPanelLightCues::onRemove()
{
    const S32 index = selectedCueIndex();
    CueList list = ALCineLightRigManager::instance().selected().cueList();
    if (index < 0 || index >= static_cast<S32>(list.mCues.size()))
    {
        return;
    }
    list.mCues.erase(list.mCues.begin() + index);
    commitListEdit(list, "Remove light cue");
    refreshButtons();
}

void ALPanelLightCues::onPlay()
{
    ALCineLightRigManager::instance().selected().cueGoto(selectedCueIndex(), presentationTime(), false);
}

void ALPanelLightCues::onStop()
{
    ALCineLightRigManager::instance().selected().cueRelease(presentationTime());
}
