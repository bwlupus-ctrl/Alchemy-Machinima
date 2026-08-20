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
    void refreshCastCombo();
    void refreshControls();

    void onMasterEnableCommit();
    void onSlotCommit(S32 slot_index);
    void onEnableCommit();
    void onTargetCommit();
    void onEyeTargetCommit();
    void onPriorityCommit();
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
    void onGazeCues();

    uuid_vec_t mSelected;
    S32 mActiveSlot = 0;
    std::string mCastSignature;

    LLTextBox* mStatus = nullptr;
    LLCheckBoxCtrl* mMasterEnable = nullptr;
    LLCheckBoxCtrl* mSlotYou = nullptr;
    LLCheckBoxCtrl* mSlotA = nullptr;
    LLCheckBoxCtrl* mSlotB = nullptr;
    LLCheckBoxCtrl* mSlotC = nullptr;
    LLCheckBoxCtrl* mSlotD = nullptr;

    LLCheckBoxCtrl* mEnable = nullptr;
    LLComboBox* mTarget = nullptr;
    LLComboBox* mEyeTarget = nullptr;
    LLComboBox* mPriority = nullptr;
    LLComboBox* mTargetDetailScope = nullptr;
    LLComboBox* mCast = nullptr;
    LLButton* mSetPoint = nullptr;
    LLButton* mPickObject = nullptr;
    LLButton* mClearObject = nullptr;
    LLSliderCtrl* mBlend = nullptr;
    LLSliderCtrl* mTorso = nullptr;
    LLSliderCtrl* mIntensity = nullptr;
    LLSliderCtrl* mSmoothing = nullptr;
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
    LLButton* mGazeCues = nullptr;
    bool mEditingEyeTarget = false;
};

#endif // AL_ALPANELLENSGAZE_H
