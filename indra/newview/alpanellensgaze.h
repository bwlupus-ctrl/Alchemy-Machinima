/**
 * @file alpanellensgaze.h
 * @brief Shared Actor Gaze controls for the standalone floater.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALPANELLENSGAZE_H
#define AL_ALPANELLENSGAZE_H

#include "llpanel.h"
#include "lluuid.h"

#include <map>
#include <string>

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLSliderCtrl;
class LLSpinCtrl;
class LLTextBox;

class ALPanelLensGaze final : public LLPanel
{
public:
    ALPanelLensGaze();
    ~ALPanelLensGaze() override = default;

    bool postBuild() override;
    void draw() override;

    // Commits address every selected actor. Empty selection deliberately means
    // the agent avatar; refresh presents the first selected actor's state.
    void setSelectedActors(const uuid_vec_t& ids) { mSelected = ids; }

private:
    LLUUID slotActor(S32 slot_index) const;
    LLUUID activeSlotActor() const;
    LLUUID displayActor() const;
    uuid_vec_t commitActors() const;
    // The set of cast actors the edit rows write to: the single edited actor
    // (specific slot) or every populated actor (All scope). The display actor
    // is a single representative used to read+present current values.
    uuid_vec_t editActors() const;
    LLUUID     editDisplayActor() const;
    void refreshCastCombo();
    void refreshEditActorCombo();
    void refreshControls();
    // Random performance cycling (DirectorGazeRandomMode): retargets each
    // edited actor to a deterministically random preset roughly every
    // DirectorGazeRandomInterval seconds through the same blended entry point
    // manual preset commits use. Driven from draw() on the presentation clock.
    void updateRandomPerformance();

    void onMasterEnableCommit();
    void onEditActorCommit();
    void onSlotLookAtCommit(S32 slot_index);
    void onEnableCommit();
    void onTargetCommit();
    void onEyeTargetCommit();
    void onPriorityCommit();
    void onAnimPriorityCommit();
    void onTargetDetailScopeCommit();
    void onCastCommit();
    void onSetPoint();
    void onPickObject();
    void onClearObject();
    void onBlendCommit();
    void onTorsoCommit();
    void onIntensityCommit();
    void onSmoothingCommit();
    void onBreakoffCommit();
    void onEyelineCommit();
    void onPersonaCommit();
    void onPerformanceCommit();
    void onResetPerformance();
    void onMicroLifeCommit();
    void onBlinksCommit();
    void onVariationCommit();
    void onBreakFreqCommit();
    void onEaseAcquireCommit();
    void onEaseReleaseCommit();
    void onDeadZoneCommit();
    void onCameraRollCommit();
    void onExaggerateCommit();
    void onModeCommit();
    void onGazeCues();

    uuid_vec_t mSelected;
    S32 mActiveSlot = 0;
    // Editing scope: true broadcasts edit rows to every populated actor; false
    // edits only the mActiveSlot actor (You / A-D).
    bool mEditAll = false;
    std::string mCastSignature;
    std::string mEditActorSignature;

    LLTextBox* mStatus = nullptr;
    LLComboBox* mEditActor = nullptr;
    LLCheckBoxCtrl* mMasterEnable = nullptr;
    LLCheckBoxCtrl* mSlotYou = nullptr;
    LLCheckBoxCtrl* mSlotA = nullptr;
    LLCheckBoxCtrl* mSlotB = nullptr;
    LLCheckBoxCtrl* mSlotC = nullptr;
    LLCheckBoxCtrl* mSlotD = nullptr;

    LLCheckBoxCtrl* mEnable = nullptr;
    LLComboBox* mTarget = nullptr;
    LLComboBox* mEyeTarget = nullptr;
    LLComboBox* mPriority = nullptr;        // ownership scope
    LLComboBox* mAnimPriority = nullptr;    // SL animation priority (yield gate)
    LLComboBox* mTargetDetailScope = nullptr;
    LLComboBox* mCast = nullptr;
    LLButton* mSetPoint = nullptr;
    LLButton* mPickObject = nullptr;
    LLButton* mClearObject = nullptr;
    LLSliderCtrl* mBlend = nullptr;
    LLSliderCtrl* mTorso = nullptr;
    LLSliderCtrl* mIntensity = nullptr;
    LLSliderCtrl* mSmoothing = nullptr;
    LLSliderCtrl* mMicroLife = nullptr;
    LLCheckBoxCtrl* mBlinks = nullptr;
    LLSliderCtrl* mVariation = nullptr;
    LLSliderCtrl* mBreakFreq = nullptr;
    LLSpinCtrl* mEaseAcquire = nullptr;
    LLSpinCtrl* mEaseRelease = nullptr;
    LLSpinCtrl* mDeadZone = nullptr;
    LLCheckBoxCtrl* mBreakoff = nullptr;
    LLSpinCtrl* mBreakoffAngle = nullptr;
    LLSliderCtrl* mEyelineYaw = nullptr;
    LLSliderCtrl* mEyelinePitch = nullptr;
    LLComboBox* mPerformance = nullptr;
    LLSliderCtrl* mPersonaDominance = nullptr;
    LLSliderCtrl* mPersonaAffection = nullptr;
    LLSliderCtrl* mPersonaAnxiety = nullptr;
    LLSliderCtrl* mCameraRoll = nullptr;
    LLSliderCtrl* mExaggerate = nullptr;
    LLComboBox* mMode = nullptr;
    LLButton* mGazeCues = nullptr;
    bool mEditingEyeTarget = false;
    // Last random-cycle index applied per actor (presentation-clock derived,
    // so scrubbing to the same time re-derives the same cycle and preset).
    std::map<LLUUID, U64> mRandomCycleSeen;
};

#endif // AL_ALPANELLENSGAZE_H
