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

    // Shared with the Director Gaze Cues page (alpanelgazecues.cpp) so the page and this editor
    // build new cues and list rows the same way.
    static LLDirectorCast::GazeCue makeDefaultCue(const LLUUID& actor_id);
    static std::string targetLabel(const LLDirectorCast::GazeCue& cue);
    static const char* macroLabel(ALGazeMath::EGazeCueMacro macro);
    // Select one cue of this actor's list and load it into the editor (the page opens the floater here).
    void selectCueIndex(S32 index);

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

    // [Machinima] Phase 3: influence lane -- add/update/delete over a full
    // presentation-time key list, mirroring the cue list's own add/edit/
    // delete path (a separate scroll_list/editor pair; the influence track is
    // stateless/independent, not embedded in cue targets). Uses its own
    // "seen revision" against the SAME shared LLDirectorCast::mGazeCueRevision
    // counter the cue list already watches, so either lane changing (from
    // this floater, the panel's inline Set/Update/Delete, or a scene load)
    // refreshes independent of floater lifetime.
    LLDirectorCast::GazeInfluenceKey defaultInfluenceKey() const;
    S32 selectedInfluenceIndex() const;
    void refreshInfluenceList(bool force = false);
    void loadInfluenceEditor(const LLDirectorCast::GazeInfluenceKey& key);
    LLDirectorCast::GazeInfluenceKey readInfluenceEditor() const;
    void selectInfluenceKey(const LLDirectorCast::GazeInfluenceKey& key);

    void onInfluenceSelected();
    void onInfluenceAdd();
    void onInfluenceUpdate();
    void onInfluenceDelete();

    LLUUID mActorId;
    U64 mSeenRevision = U64(-1);
    U64 mSeenInfluenceRevision = U64(-1);
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

    LLScrollListCtrl* mInfluenceList = nullptr;
    LLButton* mInfluenceAdd = nullptr;
    LLButton* mInfluenceUpdate = nullptr;
    LLButton* mInfluenceDelete = nullptr;
    LLSpinCtrl* mInfluenceTime = nullptr;
    LLSpinCtrl* mInfluenceValue = nullptr;
    LLComboBox* mInfluenceInterp = nullptr;
};

#endif // AL_ALFLOATERGAZECUES_H
