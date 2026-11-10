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
#include "algazenearbyname.h"

#include "llagent.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfloaterreg.h"
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
        // Cast members keep their existing behaviour exactly: the stored name, else the name
        // cache's complete name.
        if (!member->mLastName.empty())
        {
            return member->mLastName;
        }
        if (LLAvatarName name; LLAvatarNameCache::get(id, &name))
        {
            return name.getCompleteName();
        }
        return "Unknown";
    }
    // Not a cast member: a Nearby gaze target can reach the cue editor, so the shared rule
    // applies (display name only, withheld under RLV name hiding and under @shownearby, never
    // a UUID).
    const std::string name = ALGazeNearby::nonCastTargetName(id);
    return name.empty() ? std::string("Unknown") : name;
}

// Select a cue-combo row by value AND sync LLComboBox's remembered selection: setSelectedByValue
// leaves mLastSelectedIndex stale, and hideList() re-selects that index later (a dismissed list
// would silently pick another row).
bool selectComboByValue(LLComboBox* combo, const std::string& value)
{
    if (!combo->setSelectedByValue(LLSD(value), true))
    {
        return false;
    }
    combo->setCurrentByIndex(combo->getCurrentIndex());
    return true;
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

const char* influenceInterpName(LLDirectorCast::EGazeKeyInterpolation interp)
{
    switch (interp)
    {
        case LLDirectorCast::GAZE_KEY_STEP:   return "Step";
        case LLDirectorCast::GAZE_KEY_LINEAR: return "Linear";
        case LLDirectorCast::GAZE_KEY_SMOOTHSTEP:
        default:                              return "Smoothstep";
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

void ALFloaterGazeCues::rewriteActorReferences(const LLUUID& old_id, const LLUUID& new_id)
{
    if (old_id.isNull() || old_id == new_id) return;
    // Copy the registry list: deleting an owner's editor removes it from the registry.
    const auto floaters = LLFloaterReg::getFloaterList("gaze_cues");
    for (LLFloater* base : floaters)
    {
        auto* floater = dynamic_cast<ALFloaterGazeCues*>(base);
        if (!floater) continue;
        if (floater->mActorId == old_id)
        {
            if (new_id.isNull())
            {
                LLFloaterReg::destroyInstance("gaze_cues", floater->getKey());
                continue;
            }
            floater->mActorId = new_id;
            floater->setKey(LLSD(new_id.asString()));
            if (floater->mSubject) floater->mSubject->setText("Track: " + actorName(new_id));
        }
        if (!floater->mCastTarget) continue; // not built yet
        // Include the current widget selection, which may be an uncommitted edit.
        auto& target = floater->mEditorTarget;
        target.mMode = static_cast<LLActorMover::GazeTarget::EMode>(floater->mTargetMode->getValue().asInteger());
        target.mCastRef = LLUUID(floater->mCastTarget->getSelectedValue().asString());
        LLActorMover::rewriteGazeTarget(target, old_id, new_id);
        floater->mTargetMode->setValue(static_cast<S32>(target.mMode));
        floater->refreshCastCombo(true);
        floater->refreshEditorVisibility();
    }
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

    mInfluenceList = getChild<LLScrollListCtrl>("influence_list");
    mInfluenceAdd = getChild<LLButton>("btn_influence_add");
    mInfluenceUpdate = getChild<LLButton>("btn_influence_update");
    mInfluenceDelete = getChild<LLButton>("btn_influence_delete");
    mInfluenceTime = getChild<LLSpinCtrl>("influence_time");
    mInfluenceValue = getChild<LLSpinCtrl>("influence_value");
    mInfluenceInterp = getChild<LLComboBox>("influence_interp");

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

    mInfluenceList->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onInfluenceSelected(); });
    mInfluenceAdd->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onInfluenceAdd(); });
    mInfluenceUpdate->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onInfluenceUpdate(); });
    mInfluenceDelete->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onInfluenceDelete(); });

    refreshCastCombo(true);
    refreshCueList(true);
    if (mCueList->getItemCount() == 0)
    {
        loadEditor(defaultCue());
    }
    refreshInfluenceList(true);
    if (mInfluenceList->getItemCount() == 0)
    {
        loadInfluenceEditor(defaultInfluenceKey());
    }
    return LLFloater::postBuild();
}

void ALFloaterGazeCues::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    // Names RLV now hides (or that finally arrived) are redacted/healed on a 2 s tick, with no
    // cue revision change: the cast combo's extra row and the cue list's target labels.
    // Label-only: rows are relabelled IN PLACE and the combo keeps the user's current value, so a
    // tick never reloads the editor or discards a pending edit (no loadEditor on this path).
    const bool names_due = mNameClock.due();
    refreshCastCombo(false, names_due);
    if (names_due)
    {
        const LLDirectorCast::GazeCueList& cues = LLDirectorCast::instance().getGazeCues(mActorId);
        const std::string signature = targetLabelSignature(cues);
        if (signature != mCueLabelSignature)
        {
            if (relabelRows(mCueList, cues, 1))
            {
                mCueLabelSignature = signature;
            }
            else
            {
                refreshCueList(true); // the row set itself differs: a real model reload
            }
        }
    }
    refreshCueList();
    refreshEditorVisibility();
    refreshInfluenceList();
    LLFloater::draw();
}

LLDirectorCast::GazeCue ALFloaterGazeCues::defaultCue() const
{
    return makeDefaultCue(mActorId);
}

//static
const char* ALFloaterGazeCues::macroLabel(ALGazeMath::EGazeCueMacro macro)
{
    return macroName(macro);
}

//static
std::string ALFloaterGazeCues::targetLabel(const LLDirectorCast::GazeCue& cue)
{
    LLActorMover::GazeTarget display_target = cue.mTarget;
    if (cue.mMacro == ALGazeMath::GAZE_MACRO_BUTTON_LOOK)
    {
        display_target.mMode = LLActorMover::GazeTarget::CAMERA;
    }
    return targetName(display_target);
}

void ALFloaterGazeCues::selectCueIndex(S32 index)
{
    refreshCueList(true);
    if (index >= 0 && index < mCueList->getItemCount())
    {
        mCueList->selectNthItem(index);
        onCueSelected();
    }
}

//static
LLDirectorCast::GazeCue ALFloaterGazeCues::makeDefaultCue(const LLUUID& actor_id)
{
    LLDirectorCast::GazeCue cue;
    cue.mStartSec = llmax(
        LLPresentationTime::currentFrame().presentation_time, 0.0);
    cue.mDurationSec = llmax(
        static_cast<F64>(gSavedSettings.getF32("DirectorGazeCueDefaultDurationSec")),
        0.0);
    cue.mTarget = LLDirectorCast::instance().getGazeTarget(actor_id);
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

// Rewrite the target column of existing rows in place (names RLV hid, or that arrived). Nothing
// is cleared, reselected or reloaded. False when the rows do not match the cues (caller reloads).
//static
bool ALFloaterGazeCues::relabelRows(LLScrollListCtrl* list, const LLDirectorCast::GazeCueList& cues, S32 column)
{
    if (list->getItemCount() != static_cast<S32>(cues.size()))
    {
        return false;
    }
    // Rows may be sorted: each row's VALUE (not its position) is the index of its cue.
    for (S32 row = 0; row < list->getItemCount(); ++row)
    {
        LLScrollListItem* item = list->getItemByIndex(row);
        const S32 cue_index = item ? item->getValue().asInteger() : -1;
        LLScrollListCell* cell = item ? item->getColumn(column) : nullptr;
        if (!cell || cue_index < 0 || cue_index >= static_cast<S32>(cues.size()))
        {
            return false;
        }
        cell->setValue(LLSD(targetLabel(cues[cue_index]))); // ui-draw-lint: allow(raw-write) Runs only on the 2 s name tick when a rendered label changed.
    }
    if (list->getSortColumnIndex() == column)
    {
        list->setNeedsSort(true); // the sorted column's text changed: re-sort on the next draw
    }
    return true;
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
    mCueLabelSignature = targetLabelSignature(cues);
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

// Label of the saved gaze target when it is NOT a cast member (picked from the Gaze page's Nearby
// list): its display name (RLV-gated), the generic "Unknown person" when no name is known, and
// "(away)" appended when the person is not here. Empty when the target is unset, the actor
// itself or a cast member (those already have rows). Never a UUID.
std::string ALFloaterGazeCues::extraTargetLabel() const
{
    const LLUUID ref = mEditorTarget.mCastRef;
    if (ref.isNull() || ref == mActorId)
    {
        return std::string();
    }
    const uuid_vec_t& ids = LLDirectorCast::instance().getIds();
    if (std::find(ids.begin(), ids.end(), ref) != ids.end())
    {
        return std::string();
    }
    if (LLDirectorCast::isSelf(ref))
    {
        return "You";
    }
    const std::string name = ALGazeNearby::nonCastTargetName(ref);
    if (LLDirectorCast::instance().resolve(ref, false))
    {
        return name.empty() ? std::string("Unknown person") : name;
    }
    return ALGazeNearby::awayLabel(name);
}

std::string ALFloaterGazeCues::targetLabelSignature(const LLDirectorCast::GazeCueList& cues)
{
    std::string signature;
    for (const LLDirectorCast::GazeCue& cue : cues)
    {
        signature += targetLabel(cue);
        signature += '\n';
    }
    return signature;
}

// `force` is a model load (a cue was loaded): rows rebuild and the cue's own target is selected.
// Otherwise this is a context or label refresh: the rows rebuild only if the cast ids, the actor,
// the saved target id or the extra row's (id, label) changed, and the user's CURRENT combo value
// is kept when its row still exists. The extra row's id is stored beside its label (mExtraRef), so
// a label can never sit on the wrong id, and it vanishes when the target joins the cast.
void ALFloaterGazeCues::refreshCastCombo(bool force, bool relabel)
{
    std::string signature = mActorId.asString();
    for (const LLUUID& id : LLDirectorCast::instance().getIds())
    {
        signature += id.asString();
    }
    signature += mEditorTarget.mCastRef.asString();
    const bool context_changed = signature != mCastSignature;
    if (!force && !relabel && !context_changed)
    {
        return;
    }
    // Re-derived on a load, a context change and the 2 s tick, never per frame.
    const std::string label = extraTargetLabel();
    const LLUUID extra_ref = label.empty() ? LLUUID::null : mEditorTarget.mCastRef;
    const bool rows_changed = force || context_changed || label != mExtraLabel || extra_ref != mExtraRef;
    mCastSignature = signature;
    mExtraLabel = label;
    mExtraRef = extra_ref;
    if (!rows_changed)
    {
        return;
    }
    const std::string keep_value = force ? std::string() : mCastTarget->getSelectedValue().asString();
    if (mCastTarget->isListVisible())
    {
        mCastTarget->hideList(); // a redaction must not leave the old rows open
    }
    mCastTarget->removeall();
    mCastTarget->add("(unset)", LLSD(std::string()));
    for (const LLUUID& id : LLDirectorCast::instance().getIds())
    {
        if (id != mActorId)
        {
            mCastTarget->add(actorName(id), LLSD(id.asString()));
        }
    }
    if (!mExtraLabel.empty())
    {
        // The saved non-cast target, so editing the cue (or adding one from it) keeps it.
        mCastTarget->add(mExtraLabel, LLSD(mExtraRef.asString()));
    }
    if (!force && selectComboByValue(mCastTarget, keep_value))
    {
        return; // the user's current choice (a pending, uncommitted edit) is kept
    }
    if (mEditorTarget.mCastRef.isNull() ||
        !selectComboByValue(mCastTarget, mEditorTarget.mCastRef.asString()))
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
    refreshCastCombo(true); // offers the cue's saved non-cast target as a row before it is selected
    mTime->setValue(cue.mStartSec);
    mDuration->setValue(cue.mDurationSec);
    mTargetMode->setValue(static_cast<S32>(cue.mTarget.mMode));
    if (cue.mTarget.mCastRef.isNull() ||
        !selectComboByValue(mCastTarget, cue.mTarget.mCastRef.asString()))
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

// [Machinima] Phase 3: influence-lane editor. Mirrors the cue list's
// default/select/refresh/add/edit/delete pattern above, but reads/writes
// LLDirectorCast's separate GazeInfluenceKeyList via
// get/setGazeInfluenceKeys(), not GazeCueList.
LLDirectorCast::GazeInfluenceKey ALFloaterGazeCues::defaultInfluenceKey() const
{
    LLDirectorCast::GazeInfluenceKey key;
    key.mTimeSec = llmax(
        LLPresentationTime::currentFrame().presentation_time, 0.0);
    key.mValue = 1.f;
    key.mInterpolation = LLDirectorCast::GAZE_KEY_SMOOTHSTEP;
    return key;
}

S32 ALFloaterGazeCues::selectedInfluenceIndex() const
{
    LLScrollListItem* selected = mInfluenceList->getFirstSelected();
    return selected ? selected->getValue().asInteger() : -1;
}

void ALFloaterGazeCues::refreshInfluenceList(bool force)
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    // Same shared revision counter the cue list watches (mGazeCueRevision is
    // bumped by both setGazeCues() and setGazeInfluenceKeys()); tracked with
    // an independent "seen" value so this list's own staleness check does not
    // depend on refreshCueList() having run first this frame.
    const U64 revision = cast.getGazeCueRevision();
    if (!force && revision == mSeenInfluenceRevision)
    {
        return;
    }
    mSeenInfluenceRevision = revision;
    const S32 old_selection = selectedInfluenceIndex();
    mInfluenceList->deleteAllItems();
    const LLDirectorCast::GazeInfluenceKeyList& keys =
        cast.getGazeInfluenceKeys(mActorId);
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        LLSD row;
        row["value"] = static_cast<S32>(i);
        row["columns"][0]["column"] = "time";
        row["columns"][0]["value"] = llformat("%.3f", keys[i].mTimeSec);
        row["columns"][1]["column"] = "value";
        row["columns"][1]["value"] = llformat("%.2f", keys[i].mValue);
        row["columns"][2]["column"] = "curve";
        row["columns"][2]["value"] = influenceInterpName(keys[i].mInterpolation);
        mInfluenceList->addElement(row, ADD_BOTTOM);
    }
    if (old_selection >= 0 && old_selection < mInfluenceList->getItemCount())
    {
        mInfluenceList->selectNthItem(old_selection);
        loadInfluenceEditor(keys[old_selection]);
    }
    const bool selected = selectedInfluenceIndex() >= 0;
    mInfluenceUpdate->setEnabled(selected);
    mInfluenceDelete->setEnabled(selected);
}

void ALFloaterGazeCues::loadInfluenceEditor(
    const LLDirectorCast::GazeInfluenceKey& key)
{
    mInfluenceTime->setValue(key.mTimeSec);
    mInfluenceValue->setValue(key.mValue);
    mInfluenceInterp->setValue(static_cast<S32>(key.mInterpolation));
}

LLDirectorCast::GazeInfluenceKey ALFloaterGazeCues::readInfluenceEditor() const
{
    LLDirectorCast::GazeInfluenceKey key;
    key.mTimeSec = llmax(mInfluenceTime->getValue().asReal(), 0.0);
    key.mValue = llclamp((F32)mInfluenceValue->getValue().asReal(), 0.f, 1.f);
    key.mInterpolation = static_cast<LLDirectorCast::EGazeKeyInterpolation>(
        llclamp(mInfluenceInterp->getValue().asInteger(),
                static_cast<S32>(LLDirectorCast::GAZE_KEY_STEP),
                static_cast<S32>(LLDirectorCast::GAZE_KEY_SMOOTHSTEP)));
    return key;
}

void ALFloaterGazeCues::selectInfluenceKey(
    const LLDirectorCast::GazeInfluenceKey& wanted)
{
    const LLDirectorCast::GazeInfluenceKeyList& keys =
        LLDirectorCast::instance().getGazeInfluenceKeys(mActorId);
    for (S32 i = static_cast<S32>(keys.size()) - 1; i >= 0; --i)
    {
        if (std::fabs(keys[i].mTimeSec - wanted.mTimeSec) <= 1e-6)
        {
            mInfluenceList->selectNthItem(i);
            onInfluenceSelected();
            return;
        }
    }
}

void ALFloaterGazeCues::onInfluenceSelected()
{
    const S32 index = selectedInfluenceIndex();
    const LLDirectorCast::GazeInfluenceKeyList& keys =
        LLDirectorCast::instance().getGazeInfluenceKeys(mActorId);
    const bool valid = index >= 0 && index < static_cast<S32>(keys.size());
    mInfluenceUpdate->setEnabled(valid);
    mInfluenceDelete->setEnabled(valid);
    if (valid)
    {
        loadInfluenceEditor(keys[index]);
    }
}

void ALFloaterGazeCues::onInfluenceAdd()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    LLDirectorCast::GazeInfluenceKey key = readInfluenceEditor();
    LLDirectorCast::GazeInfluenceKeyList keys =
        cast.getGazeInfluenceKeys(mActorId);
    keys.push_back(key);
    cast.setGazeInfluenceKeys(mActorId, keys);
    refreshInfluenceList(true);
    selectInfluenceKey(key);
}

void ALFloaterGazeCues::onInfluenceUpdate()
{
    const S32 index = selectedInfluenceIndex();
    LLDirectorCast& cast = LLDirectorCast::instance();
    LLDirectorCast::GazeInfluenceKeyList keys =
        cast.getGazeInfluenceKeys(mActorId);
    if (index < 0 || index >= static_cast<S32>(keys.size()))
    {
        return;
    }
    const LLDirectorCast::GazeInfluenceKey key = readInfluenceEditor();
    keys[index] = key;
    cast.setGazeInfluenceKeys(mActorId, keys);
    refreshInfluenceList(true);
    selectInfluenceKey(key);
}

void ALFloaterGazeCues::onInfluenceDelete()
{
    const S32 index = selectedInfluenceIndex();
    LLDirectorCast& cast = LLDirectorCast::instance();
    LLDirectorCast::GazeInfluenceKeyList keys =
        cast.getGazeInfluenceKeys(mActorId);
    if (index < 0 || index >= static_cast<S32>(keys.size()))
    {
        return;
    }
    keys.erase(keys.begin() + index);
    cast.setGazeInfluenceKeys(mActorId, keys);
    refreshInfluenceList(true);
    if (!keys.empty())
    {
        const S32 next = llmin(index, static_cast<S32>(keys.size()) - 1);
        mInfluenceList->selectNthItem(next);
        onInfluenceSelected();
    }
    else
    {
        loadInfluenceEditor(defaultInfluenceKey());
    }
}
