/**
 * @file alfloatergazecues.cpp
 * @brief Minimal presentation-time gaze cue editor.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alfloatergazecues.h"

#include "llagent.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llpresentationtime.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llselectmgr.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llvoavatarself.h"

#include <cmath>

namespace
{
std::string actorName(const LLUUID& id)
{
    if (isAgentAvatarValid() && id == gAgentAvatarp->getID())
    {
        return "You";
    }
    if (const LLDirectorCast::CastMember* member =
            LLDirectorCast::instance().getMember(id))
    {
        if (!member->mLastName.empty())
        {
            return member->mLastName;
        }
    }
    if (LLAvatarName name; LLAvatarNameCache::get(id, &name))
    {
        return name.getCompleteName();
    }
    return id.asString().substr(0, 8);
}

const char* macroName(ALGazeMath::EGazeCueMacro macro)
{
    switch (macro)
    {
        case ALGazeMath::GAZE_MACRO_DOUBLE_TAKE:    return "Double-Take";
        case ALGazeMath::GAZE_MACRO_BUTTON_LOOK:   return "Button Look";
        case ALGazeMath::GAZE_MACRO_CREEP_TURN:    return "Creep Turn";
        case ALGazeMath::GAZE_MACRO_OBJECT_GLANCE: return "Object Glance";
        case ALGazeMath::GAZE_MACRO_NONE:
        default:                                    return "None";
    }
}

std::string targetName(const LLActorMover::GazeTarget& target)
{
    switch (target.mMode)
    {
        case LLActorMover::GazeTarget::CAMERA:
            return "Camera";
        case LLActorMover::GazeTarget::CAST_MEMBER:
            return target.mCastRef.notNull()
                ? actorName(target.mCastRef) : "Cast (unset)";
        case LLActorMover::GazeTarget::FIXED_POINT:
            return "Fixed point";
        case LLActorMover::GazeTarget::OBJECT:
            return target.mObjectRef.notNull()
                ? "Object " + target.mObjectRef.asString().substr(0, 8)
                : "Object (unset)";
        case LLActorMover::GazeTarget::MOTION:
        default:
            return "Motion";
    }
}
} // anonymous namespace

ALFloaterGazeCues::ALFloaterGazeCues(const LLSD& key)
    : LLFloater(key),
      mActorId(key.asUUID())
{
}

bool ALFloaterGazeCues::postBuild()
{
    mSubject = getChild<LLTextBox>("cue_subject");
    mCueList = getChild<LLScrollListCtrl>("cue_list");
    mAdd = getChild<LLButton>("btn_cue_add");
    mEdit = getChild<LLButton>("btn_cue_edit");
    mDelete = getChild<LLButton>("btn_cue_delete");
    mTime = getChild<LLSpinCtrl>("cue_time");
    mDuration = getChild<LLSpinCtrl>("cue_duration");
    mTargetMode = getChild<LLComboBox>("cue_target_mode");
    mCastTarget = getChild<LLComboBox>("cue_cast_target");
    mSetPoint = getChild<LLButton>("btn_cue_set_point");
    mPickObject = getChild<LLButton>("btn_cue_pick_object");
    mClearObject = getChild<LLButton>("btn_cue_clear_object");
    mMacro = getChild<LLComboBox>("cue_macro");
    mAcquireStyle = getChild<LLComboBox>("cue_acquire_style");
    mReleaseStyle = getChild<LLComboBox>("cue_release_style");
    mGap = getChild<LLSpinCtrl>("cue_gap");
    mDwell = getChild<LLSpinCtrl>("cue_dwell");
    mHoldFrames = getChild<LLSpinCtrl>("cue_hold_frames");
    mResidue = getChild<LLCheckBoxCtrl>("cue_residue");

    mSubject->setText("Track: " + actorName(mActorId));
    mCueList->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onCueSelected(); });
    mAdd->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAdd(); });
    mEdit->setCommitCallback([this](LLUICtrl*, const LLSD&) { onEdit(); });
    mDelete->setCommitCallback([this](LLUICtrl*, const LLSD&) { onDelete(); });
    mSetPoint->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSetPoint(); });
    mPickObject->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPickObject(); });
    mClearObject->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClearObject(); });
    mTargetMode->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { refreshEditorVisibility(); });
    mMacro->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { refreshEditorVisibility(); });

    getChild<LLButton>("btn_insert_double_take")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) {
            onQuickMacro(ALGazeMath::GAZE_MACRO_DOUBLE_TAKE); });
    getChild<LLButton>("btn_insert_button_look")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) {
            onQuickMacro(ALGazeMath::GAZE_MACRO_BUTTON_LOOK); });
    getChild<LLButton>("btn_insert_creep_turn")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) {
            onQuickMacro(ALGazeMath::GAZE_MACRO_CREEP_TURN); });
    getChild<LLButton>("btn_insert_object_glance")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) {
            onQuickMacro(ALGazeMath::GAZE_MACRO_OBJECT_GLANCE); });

    refreshCastCombo(true);
    refreshCueList(true);
    if (mCueList->getItemCount() == 0)
    {
        loadEditor(defaultCue());
    }
    return LLFloater::postBuild();
}

void ALFloaterGazeCues::draw()
{
    refreshCastCombo();
    refreshCueList();
    refreshEditorVisibility();
    LLFloater::draw();
}

LLDirectorCast::GazeCue ALFloaterGazeCues::defaultCue() const
{
    LLDirectorCast::GazeCue cue;
    cue.mStartSec = llmax(
        LLPresentationTime::currentFrame().presentation_time, 0.0);
    cue.mDurationSec = llmax(
        static_cast<F64>(gSavedSettings.getF32("DirectorGazeCueDefaultDurationSec")),
        0.0);
    cue.mTarget = LLDirectorCast::instance().getGazeTarget(mActorId);
    cue.mTarget.mPersonaOverride = false;
    // Snapshot the current global transition defaults into new cue targets.
    // Evaluation then remains independent of mutable settings and scrub-safe.
    if (cue.mTarget.mEaseAcquireOverride < 0.f)
    {
        cue.mTarget.mEaseAcquireOverride =
            gSavedSettings.getF32("DirectorGazeEaseAcquireSec");
    }
    if (cue.mTarget.mEaseReleaseOverride < 0.f)
    {
        cue.mTarget.mEaseReleaseOverride =
            gSavedSettings.getF32("DirectorGazeEaseReleaseSec");
    }
    cue.mMacroGapSec =
        gSavedSettings.getF32("DirectorGazeCueDoubleTakeGapSec");
    cue.mMacroDwellSec =
        gSavedSettings.getF32("DirectorGazeCueObjectDwellSec");
    cue.mHoldFramesPast =
        gSavedSettings.getS32("DirectorGazeCueButtonHoldFramesPast");
    cue.mThoughtResidue =
        gSavedSettings.getBOOL("DirectorGazeCueThoughtResidue");
    return cue;
}

S32 ALFloaterGazeCues::selectedCueIndex() const
{
    LLScrollListItem* selected = mCueList->getFirstSelected();
    return selected ? selected->getValue().asInteger() : -1;
}

void ALFloaterGazeCues::refreshCueList(bool force)
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    const U64 revision = cast.getGazeCueRevision();
    if (!force && revision == mSeenRevision)
    {
        return;
    }
    mSeenRevision = revision;
    const S32 old_selection = selectedCueIndex();
    mCueList->deleteAllItems();
    const LLDirectorCast::GazeCueList& cues = cast.getGazeCues(mActorId);
    for (std::size_t i = 0; i < cues.size(); ++i)
    {
        LLSD row;
        row["value"] = static_cast<S32>(i);
        row["columns"][0]["column"] = "time";
        row["columns"][0]["value"] = llformat("%.3f", cues[i].mStartSec);
        row["columns"][1]["column"] = "target";
        LLActorMover::GazeTarget display_target = cues[i].mTarget;
        if (cues[i].mMacro == ALGazeMath::GAZE_MACRO_BUTTON_LOOK)
        {
            display_target.mMode = LLActorMover::GazeTarget::CAMERA;
        }
        row["columns"][1]["value"] = targetName(display_target);
        row["columns"][2]["column"] = "macro";
        row["columns"][2]["value"] = macroName(cues[i].mMacro);
        mCueList->addElement(row, ADD_BOTTOM);
    }
    if (old_selection >= 0 && old_selection < mCueList->getItemCount())
    {
        mCueList->selectNthItem(old_selection);
        loadEditor(cues[old_selection]);
    }
    const bool selected = selectedCueIndex() >= 0;
    mEdit->setEnabled(selected);
    mDelete->setEnabled(selected);
}

void ALFloaterGazeCues::refreshCastCombo(bool force)
{
    std::string signature = mActorId.asString();
    for (const LLUUID& id : LLDirectorCast::instance().getIds())
    {
        signature += id.asString();
    }
    if (!force && signature == mCastSignature)
    {
        return;
    }
    mCastSignature = signature;
    mCastTarget->removeall();
    mCastTarget->add("(unset)", LLSD(std::string()));
    for (const LLUUID& id : LLDirectorCast::instance().getIds())
    {
        if (id != mActorId)
        {
            mCastTarget->add(actorName(id), LLSD(id.asString()));
        }
    }
    if (mEditorTarget.mCastRef.isNull() ||
        !mCastTarget->setSelectedByValue(
            LLSD(mEditorTarget.mCastRef.asString()), true))
    {
        mCastTarget->selectFirstItem();
    }
}

void ALFloaterGazeCues::refreshEditorVisibility()
{
    const S32 mode = mTargetMode->getValue().asInteger();
    mCastTarget->setVisible(mode == LLActorMover::GazeTarget::CAST_MEMBER);
    mSetPoint->setVisible(mode == LLActorMover::GazeTarget::FIXED_POINT);
    mPickObject->setVisible(mode == LLActorMover::GazeTarget::OBJECT);
    mClearObject->setVisible(mode == LLActorMover::GazeTarget::OBJECT);

    const S32 macro = mMacro->getValue().asInteger();
    mGap->setEnabled(macro == ALGazeMath::GAZE_MACRO_DOUBLE_TAKE);
    mDwell->setEnabled(
        macro == ALGazeMath::GAZE_MACRO_DOUBLE_TAKE ||
        macro == ALGazeMath::GAZE_MACRO_OBJECT_GLANCE);
    mHoldFrames->setEnabled(macro == ALGazeMath::GAZE_MACRO_BUTTON_LOOK);
    mResidue->setEnabled(macro == ALGazeMath::GAZE_MACRO_OBJECT_GLANCE);
}

void ALFloaterGazeCues::loadEditor(const LLDirectorCast::GazeCue& cue)
{
    mEditorTarget = cue.mTarget;
    mTime->setValue(cue.mStartSec);
    mDuration->setValue(cue.mDurationSec);
    mTargetMode->setValue(static_cast<S32>(cue.mTarget.mMode));
    if (cue.mTarget.mCastRef.isNull() ||
        !mCastTarget->setSelectedByValue(
            LLSD(cue.mTarget.mCastRef.asString()), true))
    {
        mCastTarget->selectFirstItem();
    }
    mMacro->setValue(static_cast<S32>(cue.mMacro));
    mAcquireStyle->setValue(static_cast<S32>(cue.mAcquireStyle));
    mReleaseStyle->setValue(static_cast<S32>(cue.mReleaseStyle));
    mGap->setValue(cue.mMacroGapSec);
    mDwell->setValue(cue.mMacroDwellSec);
    mHoldFrames->setValue(cue.mHoldFramesPast);
    mResidue->set(cue.mThoughtResidue);
    refreshEditorVisibility();
}

LLDirectorCast::GazeCue ALFloaterGazeCues::readEditor(
    const LLDirectorCast::GazeCue& base) const
{
    LLDirectorCast::GazeCue cue = base;
    cue.mStartSec = llmax(mTime->getValue().asReal(), 0.0);
    cue.mDurationSec = llmax(mDuration->getValue().asReal(), 0.0);
    cue.mTarget = mEditorTarget;
    cue.mTarget.mMode = static_cast<LLActorMover::GazeTarget::EMode>(
        llclamp(mTargetMode->getValue().asInteger(),
                static_cast<S32>(LLActorMover::GazeTarget::MOTION),
                static_cast<S32>(LLActorMover::GazeTarget::OBJECT)));
    const std::string cast_value = mCastTarget->getSelectedValue().asString();
    cue.mTarget.mCastRef = cast_value.empty()
        ? LLUUID::null : LLUUID(cast_value);
    cue.mMacro = static_cast<ALGazeMath::EGazeCueMacro>(
        llclamp(mMacro->getValue().asInteger(),
                static_cast<S32>(ALGazeMath::GAZE_MACRO_NONE),
                static_cast<S32>(ALGazeMath::GAZE_MACRO_OBJECT_GLANCE)));
    cue.mAcquireStyle = static_cast<LLDirectorCast::EGazeCueStyle>(
        llclamp(mAcquireStyle->getValue().asInteger(),
                static_cast<S32>(LLDirectorCast::GAZE_CUE_STYLE_DEFAULT),
                static_cast<S32>(LLDirectorCast::GAZE_CUE_STYLE_CASUAL)));
    cue.mReleaseStyle = static_cast<LLDirectorCast::EGazeCueStyle>(
        llclamp(mReleaseStyle->getValue().asInteger(),
                static_cast<S32>(LLDirectorCast::GAZE_CUE_STYLE_DEFAULT),
                static_cast<S32>(LLDirectorCast::GAZE_CUE_STYLE_CASUAL)));
    cue.mMacroGapSec = static_cast<F32>(mGap->getValue().asReal());
    cue.mMacroDwellSec = static_cast<F32>(mDwell->getValue().asReal());
    cue.mHoldFramesPast = mHoldFrames->getValue().asInteger();
    cue.mThoughtResidue = mResidue->get();
    if (cue.mMacro == ALGazeMath::GAZE_MACRO_BUTTON_LOOK)
    {
        cue.mTarget.mMode = LLActorMover::GazeTarget::CAMERA;
    }
    return cue;
}

void ALFloaterGazeCues::selectCue(const LLDirectorCast::GazeCue& wanted)
{
    const LLDirectorCast::GazeCueList& cues =
        LLDirectorCast::instance().getGazeCues(mActorId);
    for (S32 i = static_cast<S32>(cues.size()) - 1; i >= 0; --i)
    {
        if (std::fabs(cues[i].mStartSec - wanted.mStartSec) <= 1e-6 &&
            cues[i].mMacro == wanted.mMacro)
        {
            mCueList->selectNthItem(i);
            onCueSelected();
            return;
        }
    }
}

void ALFloaterGazeCues::onCueSelected()
{
    const S32 index = selectedCueIndex();
    const LLDirectorCast::GazeCueList& cues =
        LLDirectorCast::instance().getGazeCues(mActorId);
    const bool valid = index >= 0 && index < static_cast<S32>(cues.size());
    mEdit->setEnabled(valid);
    mDelete->setEnabled(valid);
    if (valid)
    {
        loadEditor(cues[index]);
    }
}

void ALFloaterGazeCues::onAdd()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    LLDirectorCast::GazeCue cue = readEditor(defaultCue());
    LLDirectorCast::GazeCueList cues = cast.getGazeCues(mActorId);
    cues.push_back(cue);
    cast.setGazeCues(mActorId, cues);
    refreshCueList(true);
    selectCue(cue);
}

void ALFloaterGazeCues::onEdit()
{
    const S32 index = selectedCueIndex();
    LLDirectorCast& cast = LLDirectorCast::instance();
    LLDirectorCast::GazeCueList cues = cast.getGazeCues(mActorId);
    if (index < 0 || index >= static_cast<S32>(cues.size()))
    {
        return;
    }
    const LLDirectorCast::GazeCue cue = readEditor(cues[index]);
    cues[index] = cue;
    cast.setGazeCues(mActorId, cues);
    refreshCueList(true);
    selectCue(cue);
}

void ALFloaterGazeCues::onDelete()
{
    const S32 index = selectedCueIndex();
    LLDirectorCast& cast = LLDirectorCast::instance();
    LLDirectorCast::GazeCueList cues = cast.getGazeCues(mActorId);
    if (index < 0 || index >= static_cast<S32>(cues.size()))
    {
        return;
    }
    cues.erase(cues.begin() + index);
    cast.setGazeCues(mActorId, cues);
    refreshCueList(true);
    if (!cues.empty())
    {
        const S32 next = llmin(index, static_cast<S32>(cues.size()) - 1);
        mCueList->selectNthItem(next);
        onCueSelected();
    }
    else
    {
        loadEditor(defaultCue());
    }
}

void ALFloaterGazeCues::onSetPoint()
{
    mEditorTarget.mFixedPoint = gAgent.getPosGlobalFromAgent(
        LLViewerCamera::getInstance()->getOrigin());
}

void ALFloaterGazeCues::onPickObject()
{
    LLViewerObject* object =
        LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    if (!object)
    {
        object = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject();
    }
    if (object)
    {
        mEditorTarget.mObjectRef = object->getRootEdit()
            ? object->getRootEdit()->getID() : object->getID();
    }
}

void ALFloaterGazeCues::onClearObject()
{
    mEditorTarget.mObjectRef.setNull();
}

void ALFloaterGazeCues::onQuickMacro(ALGazeMath::EGazeCueMacro macro)
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    LLDirectorCast::GazeCue cue = readEditor(defaultCue());
    cue.mStartSec = llmax(
        LLPresentationTime::currentFrame().presentation_time, 0.0);
    cue.mMacro = macro;
    switch (macro)
    {
        case ALGazeMath::GAZE_MACRO_DOUBLE_TAKE:
            cue.mAcquireStyle = LLDirectorCast::GAZE_CUE_STYLE_SNAP;
            cue.mReleaseStyle = LLDirectorCast::GAZE_CUE_STYLE_CASUAL;
            break;
        case ALGazeMath::GAZE_MACRO_BUTTON_LOOK:
            cue.mTarget.mMode = LLActorMover::GazeTarget::CAMERA;
            cue.mAcquireStyle = LLDirectorCast::GAZE_CUE_STYLE_SNAP;
            break;
        case ALGazeMath::GAZE_MACRO_CREEP_TURN:
            cue.mAcquireStyle = LLDirectorCast::GAZE_CUE_STYLE_DRIFT;
            if (cue.mDurationSec <= 0.0)
            {
                cue.mDurationSec = 3.0;
            }
            break;
        case ALGazeMath::GAZE_MACRO_OBJECT_GLANCE:
            cue.mAcquireStyle = LLDirectorCast::GAZE_CUE_STYLE_SNAP;
            cue.mReleaseStyle = LLDirectorCast::GAZE_CUE_STYLE_CASUAL;
            break;
        case ALGazeMath::GAZE_MACRO_NONE:
        default:
            break;
    }
    LLDirectorCast::GazeCueList cues = cast.getGazeCues(mActorId);
    cues.push_back(cue);
    cast.setGazeCues(mActorId, cues);
    refreshCueList(true);
    selectCue(cue);
}
