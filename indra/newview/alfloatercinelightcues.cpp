/**
 * @file alfloatercinelightcues.cpp
 * @brief Theatrical lighting cue-list editor and transport.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alfloatercinelightcues.h"

#include "alcinelightrig.h"
#include "alcinelightrigmanager.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfocusmgr.h"
#include "lllineeditor.h"
#include "llpresentationtime.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llspinctrl.h"
#include "lltextbox.h"

#include <algorithm>

namespace
{
using namespace ALCineLightRigModel;

F64 presentationTime()
{
    return std::max(
        LLPresentationTime::currentFrame().presentation_time, 0.0);
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
}

ALFloaterCineLightCues::ALFloaterCineLightCues(const LLSD& key)
  : LLFloater(key)
{
}

bool ALFloaterCineLightCues::postBuild()
{
    mListName = getChild<LLComboBox>("cue_list_name");
    mTimecodeMode = getChild<LLCheckBoxCtrl>("cue_timecode_mode");
    mCueRows = getChild<LLScrollListCtrl>("cue_rows");
    mLabel = getChild<LLLineEditor>("cue_label");
    mAtSec = getChild<LLSpinCtrl>("cue_at_sec");
    mFadeSec = getChild<LLSpinCtrl>("cue_fade_sec");
    mDelaySec = getChild<LLSpinCtrl>("cue_delay_sec");
    mFollowSec = getChild<LLSpinCtrl>("cue_follow_sec");
    mProfile = getChild<LLComboBox>("cue_profile");
    mFX = getChild<LLComboBox>("cue_fx");
    mUpdate = getChild<LLButton>("cue_update");
    mDeleteCue = getChild<LLButton>("cue_delete");
    mStatus = getChild<LLTextBox>("cue_status");

    mProfile->add("Ease", LLSD(CUE_FADE_EASE));
    mProfile->add("Linear", LLSD(CUE_FADE_LINEAR));
    mProfile->add("Snap", LLSD(CUE_FADE_SNAP));
    mFX->add("None", LLSD(-1));
    for (S32 fx = 0; fx < FX_COUNT; ++fx)
    {
        mFX->add(fxName(fx), LLSD(fx));
    }

    getChild<LLButton>("cue_list_load")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onListLoad(); });
    getChild<LLButton>("cue_list_save")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onListSave(); });
    getChild<LLButton>("cue_list_delete")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onListDelete(); });
    mCueRows->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onCueSelected(); });
    getChild<LLButton>("cue_capture")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onCapture(); });
    mUpdate->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onUpdate(); });
    mDeleteCue->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onDeleteCue(); });
    mTimecodeMode->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onTimecodeMode(); });
    getChild<LLButton>("cue_go")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onGo(); });
    getChild<LLButton>("cue_back")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onBack(); });
    getChild<LLButton>("cue_goto")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onGoto(false); });
    getChild<LLButton>("cue_snap_goto")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onGoto(true); });
    getChild<LLButton>("cue_release")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onRelease(); });

    Cue defaults = ALCineLightRigManager::instance().selected().captureCue("Cue 1");
    loadEditor(defaults);
    refreshListNames(true);
    refreshCueRows(true);
    return LLFloater::postBuild();
}

S32 ALFloaterCineLightCues::selectedCueIndex() const
{
    LLScrollListItem* item = mCueRows->getFirstSelected();
    return item ? item->getValue().asInteger() : -1;
}

void ALFloaterCineLightCues::refreshListNames(bool force)
{
    ALCineLightRig& rig = ALCineLightRigManager::instance().selected();
    const std::vector<std::string> names = rig.cueListNames();
    std::string signature;
    for (const std::string& name : names)
    {
        signature += name;
        signature.push_back('\n');
    }
    if (!force && signature == mListSignature)
    {
        return;
    }
    mListSignature = signature;
    const std::string current = mListName->getSimple();
    mListName->removeall();
    for (const std::string& name : names)
    {
        mListName->add(name, LLSD(name));
    }
    const std::string desired = !rig.cueList().mName.empty()
        ? rig.cueList().mName : current;
    if (!desired.empty())
    {
        mListName->setTextEntry(desired);
    }
}

void ALFloaterCineLightCues::refreshCueRows(bool force)
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
    mTimecodeMode->set(list.mTimecodeMode);
    for (S32 i = 0; i < static_cast<S32>(list.mCues.size()); ++i)
    {
        const Cue& cue = list.mCues[i];
        LLSD row;
        row["value"] = i;
        row["columns"][0]["column"] = "number";
        row["columns"][0]["value"] = llformat("%d", i + 1);
        row["columns"][1]["column"] = "label";
        row["columns"][1]["value"] = cue.mLabel;
        row["columns"][2]["column"] = "at";
        row["columns"][2]["value"] = llformat("%.3f", cue.mAtSec);
        row["columns"][3]["column"] = "fade";
        row["columns"][3]["value"] = llformat("%.2f", cue.mFadeSec);
        row["columns"][4]["column"] = "delay";
        row["columns"][4]["value"] = llformat("%.2f", cue.mDelaySec);
        row["columns"][5]["column"] = "curve";
        row["columns"][5]["value"] = cueProfileName(cue.mProfile);
        row["columns"][6]["column"] = "follow";
        row["columns"][6]["value"] = cue.mFollow < 0
            ? "-" : llformat("%d", cue.mFollow);
        mCueRows->addElement(row, ADD_BOTTOM);
    }
    if (old_selection >= 0 && old_selection < mCueRows->getItemCount())
    {
        mCueRows->selectNthItem(old_selection);
        loadEditor(list.mCues[old_selection]);
    }
    const bool selected = selectedCueIndex() >= 0;
    mUpdate->setEnabled(selected);
    mDeleteCue->setEnabled(selected);
    refreshStatus();
}

void ALFloaterCineLightCues::refreshStatus()
{
    ALCineLightRig& rig = ALCineLightRigManager::instance().selected();
    const S32 active = rig.activeCue();
    if (rig.cueList().mTimecodeMode)
    {
        mStatus->setText(active >= 0
            ? llformat("TIMECODE  Cue %d: %s", active + 1,
                       rig.cueList().mCues[active].mLabel.c_str())
            : "TIMECODE  Released before first cue");
    }
    else if (rig.cuePlaybackActive())
    {
        mStatus->setText(active >= 0
            ? llformat("LIVE  Cue %d: %s", active + 1,
                       rig.cueList().mCues[active].mLabel.c_str())
            : "LIVE  Released");
    }
    else
    {
        mStatus->setText(std::string("Idle"));
    }
}

void ALFloaterCineLightCues::loadEditor(const Cue& cue)
{
    mLabel->setText(cue.mLabel);
    mAtSec->setValue(cue.mAtSec);
    mFadeSec->setValue(cue.mFadeSec);
    mDelaySec->setValue(cue.mDelaySec);
    mFollowSec->setValue(cue.mFollow);
    mProfile->setValue(cue.mProfile);
    mFX->setValue(cue.mFX);
}

Cue ALFloaterCineLightCues::readEditor(const Cue& base) const
{
    Cue cue = base;
    cue.mLabel = mLabel->getText();
    cue.mAtSec = mAtSec->getValue().asReal();
    cue.mFadeSec = static_cast<F32>(mFadeSec->getValue().asReal());
    cue.mDelaySec = static_cast<F32>(mDelaySec->getValue().asReal());
    cue.mFollow = mFollowSec->getValue().asInteger();
    cue.mProfile = mProfile->getValue().asInteger();
    cue.mFX = mFX->getValue().asInteger();
    return sanitizeCue(cue);
}

void ALFloaterCineLightCues::onListLoad()
{
    if (ALCineLightRigManager::instance().selected().loadCueList(
            mListName->getSimple()))
    {
        refreshCueRows(true);
        refreshListNames(true);
    }
}

void ALFloaterCineLightCues::onListSave()
{
    if (ALCineLightRigManager::instance().selected().saveCueList(
            mListName->getSimple()))
    {
        refreshListNames(true);
        refreshCueRows(true);
    }
}

void ALFloaterCineLightCues::onListDelete()
{
    if (ALCineLightRigManager::instance().selected().deleteCueList(
            mListName->getSimple()))
    {
        refreshListNames(true);
    }
}

void ALFloaterCineLightCues::onCueSelected()
{
    const S32 index = selectedCueIndex();
    const CueList& list = ALCineLightRigManager::instance().selected().cueList();
    const bool valid = index >= 0 && index < static_cast<S32>(list.mCues.size());
    mUpdate->setEnabled(valid);
    mDeleteCue->setEnabled(valid);
    if (valid)
    {
        loadEditor(list.mCues[index]);
    }
}

void ALFloaterCineLightCues::onCapture()
{
    ALCineLightRig& rig = ALCineLightRigManager::instance().selected();
    CueList list = rig.cueList();
    Cue cue = rig.captureCue(mLabel->getText());
    cue = readEditor(cue);
    list.mCues.push_back(cue);
    rig.setCueList(list);
    refreshCueRows(true);
    mCueRows->selectNthItem(static_cast<S32>(list.mCues.size()) - 1);
    onCueSelected();
}

void ALFloaterCineLightCues::onUpdate()
{
    const S32 index = selectedCueIndex();
    ALCineLightRig& rig = ALCineLightRigManager::instance().selected();
    CueList list = rig.cueList();
    if (index < 0 || index >= static_cast<S32>(list.mCues.size()))
    {
        return;
    }
    // Update deliberately recaptures the complete live rig; cues are snapshots,
    // never references to mutable presets.
    Cue cue = rig.captureCue(mLabel->getText());
    list.mCues[index] = readEditor(cue);
    rig.setCueList(list);
    refreshCueRows(true);
    mCueRows->selectNthItem(index);
    onCueSelected();
}

void ALFloaterCineLightCues::onDeleteCue()
{
    const S32 index = selectedCueIndex();
    ALCineLightRig& rig = ALCineLightRigManager::instance().selected();
    CueList list = rig.cueList();
    if (index < 0 || index >= static_cast<S32>(list.mCues.size()))
    {
        return;
    }
    list.mCues.erase(list.mCues.begin() + index);
    rig.setCueList(list);
    refreshCueRows(true);
}

void ALFloaterCineLightCues::onTimecodeMode()
{
    ALCineLightRig& rig = ALCineLightRigManager::instance().selected();
    CueList list = rig.cueList();
    list.mTimecodeMode = mTimecodeMode->get();
    rig.setCueList(list);
    refreshCueRows(true);
}

void ALFloaterCineLightCues::onGo()
{
    ALCineLightRigManager::instance().selected().cueGo(presentationTime());
    refreshStatus();
}

void ALFloaterCineLightCues::onBack()
{
    ALCineLightRigManager::instance().selected().cueBack(presentationTime());
    refreshStatus();
}

void ALFloaterCineLightCues::onGoto(bool snap)
{
    ALCineLightRigManager::instance().selected().cueGoto(
        selectedCueIndex(), presentationTime(), snap);
    refreshStatus();
}

void ALFloaterCineLightCues::onRelease()
{
    ALCineLightRigManager::instance().selected().cueRelease(presentationTime());
    refreshStatus();
}

void ALFloaterCineLightCues::draw()
{
    refreshListNames();
    refreshCueRows();
    refreshStatus();
    const bool live_transport = !mTimecodeMode->get();
    getChild<LLButton>("cue_go")->setEnabled(live_transport);
    getChild<LLButton>("cue_back")->setEnabled(live_transport);
    getChild<LLButton>("cue_goto")->setEnabled(live_transport);
    getChild<LLButton>("cue_snap_goto")->setEnabled(live_transport);
    getChild<LLButton>("cue_release")->setEnabled(live_transport);
    LLFloater::draw();
}

bool ALFloaterCineLightCues::handleKeyHere(KEY key, MASK mask)
{
    if (key == ' ' && mask == MASK_NONE &&
        !gFocusMgr.childHasKeyboardFocus(mListName) &&
        !gFocusMgr.childHasKeyboardFocus(mLabel))
    {
        onGo();
        return true;
    }
    return LLFloater::handleKeyHere(key, mask);
}
