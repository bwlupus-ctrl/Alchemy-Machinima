/**
 * @file alfloatergazecues.h
 * @brief Minimal presentation-time gaze cue editor.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALFLOATERGAZECUES_H
#define AL_ALFLOATERGAZECUES_H

#include "lldirectorcast.h"
#include "llfloater.h"

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLScrollListCtrl;
class LLSpinCtrl;
class LLTextBox;

class ALFloaterGazeCues final : public LLFloater
{
public:
    ALFloaterGazeCues(const LLSD& key);
    ~ALFloaterGazeCues() override = default;

    bool postBuild() override;
    void draw() override;

private:
    LLDirectorCast::GazeCue defaultCue() const;
    S32 selectedCueIndex() const;
    void refreshCueList(bool force = false);
    void refreshCastCombo(bool force = false);
    void refreshEditorVisibility();
    void loadEditor(const LLDirectorCast::GazeCue& cue);
    LLDirectorCast::GazeCue readEditor(
        const LLDirectorCast::GazeCue& base) const;
    void selectCue(const LLDirectorCast::GazeCue& cue);

    void onCueSelected();
    void onAdd();
    void onEdit();
    void onDelete();
    void onSetPoint();
    void onPickObject();
    void onClearObject();
    void onQuickMacro(ALGazeMath::EGazeCueMacro macro);

    LLUUID mActorId;
    U64 mSeenRevision = U64(-1);
    std::string mCastSignature;
    LLActorMover::GazeTarget mEditorTarget;

    LLTextBox* mSubject = nullptr;
    LLScrollListCtrl* mCueList = nullptr;
    LLButton* mAdd = nullptr;
    LLButton* mEdit = nullptr;
    LLButton* mDelete = nullptr;
    LLSpinCtrl* mTime = nullptr;
    LLSpinCtrl* mDuration = nullptr;
    LLComboBox* mTargetMode = nullptr;
    LLComboBox* mCastTarget = nullptr;
    LLButton* mSetPoint = nullptr;
    LLButton* mPickObject = nullptr;
    LLButton* mClearObject = nullptr;
    LLComboBox* mMacro = nullptr;
    LLComboBox* mAcquireStyle = nullptr;
    LLComboBox* mReleaseStyle = nullptr;
    LLSpinCtrl* mGap = nullptr;
    LLSpinCtrl* mDwell = nullptr;
    LLSpinCtrl* mHoldFrames = nullptr;
    LLCheckBoxCtrl* mResidue = nullptr;
};

#endif // AL_ALFLOATERGAZECUES_H
