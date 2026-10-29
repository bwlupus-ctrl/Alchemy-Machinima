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

// [Machinima] Register the Actor Gaze random-performance auto-cycle on the idle
// callback list. Call once at viewer startup (post-settings) so cycling runs
// independent of whether the gaze floater is ever opened.
void al_gaze_register_random_perf_idle();

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
    // Random-performance cycling (DirectorGazeRandomMode) lives in the
    // file-static gazeRandomPerfTick() in the .cpp, driven by a persistent idle
    // callback registered from the ctor, so it keeps running with the floater
    // closed. (Formerly the panel method updateRandomPerformance, driven by draw.)

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
    // [Machinima] Phase 2/3/4 UI: pose composition, keyframed influence, and
    // IK/limits/lean. Per-actor structural rows commit via editGazeTargetsFor,
    // mirroring onBlendCommit/onTorsoCommit above. Influence keys commit
    // through LLDirectorCast's influence-lane accessors instead (they are not
    // GazeTarget override fields).
    void onCompositionCommit();
    void onCompositionMixCommit();
    void onInfluenceSet();
    void onInfluenceDelete();
    void onChestShareCommit();
    void onLimitModeCommit();
    void onLimitProfileCommit();
    // [Machinima] Range/IK-Lean preset choosers: stateless -- each commit
    // writes a full preset into the edited actor(s) via editGazeTargetsFor,
    // then snaps its own display back to the "Choose..." sentinel so it never
    // claims to represent the (now-Custom) resulting state.
    void onRangePresetCommit();
    void onIkLeanCommit();
    void onLeanCurveCommit();
    void onLeanThresholdCommit();
    void onLeanSoftnessCommit();
    void onLeanMaxCommit();
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
    LLComboBox* mPriority = nullptr;        // Movement Style (ownership scope)
    LLComboBox* mAnimPriority = nullptr;    // SL animation priority (yield gate)
    // [Machinima] Range preset chooser (Procedural gaze row): stateless,
    // fills mLimitProfileModeOverride/mLimitProfile/mExaggerateOverride then
    // resets to the sentinel. IK Lean preset chooser (SL anim priority row):
    // stateless, fills mLeanCurveOverride/threshold/softness/max/chest share,
    // enabled only while the resolved Movement Style is Planted spine (IK).
    LLComboBox* mRangePreset = nullptr;
    LLComboBox* mIkLean = nullptr;
    LLComboBox* mTargetDetailScope = nullptr;
    LLComboBox* mCast = nullptr;
    LLButton* mSetPoint = nullptr;
    LLButton* mPickObject = nullptr;
    LLButton* mClearObject = nullptr;
    LLSliderCtrl* mBlend = nullptr;
    LLSliderCtrl* mTorso = nullptr;
    LLSliderCtrl* mIntensity = nullptr;
    // [Machinima] Phase 2: pose composition (endpoint built when a joint is
    // allowed gaze write), orthogonal to Ownership scope / SL anim priority.
    LLComboBox* mComposition = nullptr;
    LLSliderCtrl* mCompositionMix = nullptr;
    // [Machinima] Phase 3: keyframed influence lane (LLDirectorCast
    // GazeInfluenceKeyList), a stateless presentation-time multiplier on the
    // final write weight. NOT a GazeTarget override field.
    LLSliderCtrl* mInfluence = nullptr;
    LLButton* mInfluenceSet = nullptr;
    LLButton* mInfluenceDelete = nullptr;
    // [Machinima] Phase 2: planted-spine chest split and opt-in per-joint
    // cone limit profile.
    LLSliderCtrl* mChestShare = nullptr;
    LLComboBox* mLimitMode = nullptr;
    LLSpinCtrl* mLimitEyeYaw = nullptr;
    LLSpinCtrl* mLimitEyePitch = nullptr;
    LLSpinCtrl* mLimitEyePitchUp = nullptr;
    LLSpinCtrl* mLimitHeadYaw = nullptr;
    LLSpinCtrl* mLimitHeadPitch = nullptr;
    LLSpinCtrl* mLimitHeadPitchUp = nullptr;
    LLSpinCtrl* mLimitNeckYaw = nullptr;
    LLSpinCtrl* mLimitNeckPitch = nullptr;
    LLSpinCtrl* mLimitNeckPitchUp = nullptr;
    LLSpinCtrl* mLimitSpineYaw = nullptr;
    LLSpinCtrl* mLimitSpinePitch = nullptr;
    LLSpinCtrl* mLimitSpinePitchUp = nullptr;
    LLSpinCtrl* mLimitHipsYaw = nullptr;
    LLSpinCtrl* mLimitHipsPitch = nullptr;
    LLSpinCtrl* mLimitHipsPitchUp = nullptr;
    LLSpinCtrl* mLimitEyeApplyYaw = nullptr;
    LLSpinCtrl* mLimitEyeApplyPitch = nullptr;
    LLSpinCtrl* mLimitEyeApplyPitchUp = nullptr;
    LLSpinCtrl* mLimitEyeRadial = nullptr;
    // [Machinima] Phase 4: angle-driven spine-lean curve (Planted spine only
    // in v1). DirectorGazeSoftRecruitDeg is a pure global (control_name-bound
    // in the XML) and needs no member here.
    LLComboBox* mLeanCurve = nullptr;
    LLSpinCtrl* mLeanThreshold = nullptr;
    LLSpinCtrl* mLeanSoftness = nullptr;
    LLSpinCtrl* mLeanMax = nullptr;
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
};

#endif // AL_ALPANELLENSGAZE_H
