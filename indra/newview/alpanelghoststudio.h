/**
 * @file alpanelghoststudio.h
 * @brief Shared Ghost Studio panel (instance list + full per-ghost controls).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * A reusable LLPanel (class "panel_ghost_studio") over the ALGhostStudio data
 * model: the instance list, source picker + Add/Duplicate/Delete, per-instance
 * placement (numeric X/Y/Z/yaw/scale, one-click in-world Place, snap-to-actor/
 * snap-to-me), look (style / tint / alpha / shimmer / pixelation / glitch),
 * the LIVE|FROZEN pose controls, and the line/ring array helper. Embedded by
 * BOTH the standalone Ghost Studio floater and the Director Console's Ghosts
 * tab, exactly like ALPanelPathEditor -- fully self-contained (it carries its
 * own cast-source picker), self-refreshing in draw(), and change-diffed so
 * the per-frame cost is trivial.
 */

#ifndef AL_ALPANELGHOSTSTUDIO_H
#define AL_ALPANELGHOSTSTUDIO_H

#include "llpanel.h"
#include "alghoststudio.h"

#include "lluuid.h"
#include "llframetimer.h"
#include "v3dmath.h"
#include "llquaternion.h"

#include <string>
#include <vector>

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLF32UICtrl;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLSpinCtrl;
class LLTextBox;
class LLTextEditor;
class ALCompassDial;

class ALPanelGhostStudio final : public LLPanel
{
public:
    ALPanelGhostStudio();
    ~ALPanelGhostStudio() override;

    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool new_visibility) override;

private:
    // ---- refreshers (draw-rate, all change-diffed) ----
    void refreshSourceCombo();      // "You" + cast members (rebuilt on cast change)
    void refreshLookTargetCombo();  // camera/me + cast + other ghosts
    void refreshList();             // instance rows (rebuilt on composed-sig change)
    void refreshDetail();           // selected-instance widgets + enables
    void refreshAnimationLibrary();
    void populateAnimationLibrary();   // explicit inventory re-scan (Refresh)
    void refreshStatus();           // bottom status line
    void refreshRigDiagnostics();   // selected entity clone/source prim state

    LLUUID selectedInstance() const;    // list selection -> instance id (null = none)
    LLUUID selectedListValue() const;   // raw row value (group id for headers)
    LLUUID selectedUnitId() const;      // group header id, otherwise instance id
    std::vector<LLUUID> selectedInstances() const;
    std::vector<LLUUID> selectedUnitIds() const;
    bool selectedUnitTransform(LLVector3d& foot, LLQuaternion& rotation,
                               F32& scale, bool& collapsed_group) const;
    bool applySelectedUnitTransform(const LLVector3d& foot,
                                    const LLQuaternion& rotation, F32 scale);
    bool applyUnitTransform(const LLUUID& unit_id, const LLVector3d& foot,
                            const LLQuaternion& rotation, F32 scale);

    // ---- list + CRUD ----
    void onListSelect();
    void onListMouseUp(S32 x, S32 y, MASK mask);
    void onListDoubleClick();       // toggles the row's enable
    void onClickAdd();
    void onNameCommit();
    void onClickDuplicate();
    void onClickDuplicateInPlace();
    void onClickDelete();
    void onClickRefresh();

    // ---- placement ----
    void onPosCommit();             // X/Y/Z spinners -> foot position
    void onHeadingDialCommit();     // live dial -> yaw + exact-entry spinner
    void onYawCommit();
    void onScaleCommit();
    void onChaosCommit();
    void onAnimSpeedCommit();
    void onPhysicsCommit();
    void onClickAnimPause();
    void onClickAnimResume();
    void onDriveModeCommit();
    void onDirectedAnimCommit();
    void onAnimationLibraryCommit();
    void onLoopModeCommit();
    void onClickAnimSync();
    void onLensGazeToggle();
    void onClickLensGazeSelection();
    void onLensGazeTorsoCommit();
    void onLensGazeSettingsCommit();
    void populateLensGazeCastTargets();
    void onClickPlace();            // arm the one-shot in-world placement tool
    void onClickToActor();          // snap to the source's current feet
    void onClickToMe();             // snap to my avatar's current feet
    void onClickDrop();
    void onClickAlignFeet();
    void onClickUpright();
    void onClickCopyTransform();
    void onClickPasteTransform();
    void onLookTargetCommit();
    void onClickFaceNow();
    void onKeepFacingCommit();
    void exitPlaceMode();           // drop the transient tool if it is ours

    // ---- [R2-3] in-world edit mode (persistent ALToolGhostEdit) ----
    void onToggleEditMode();        // checkbox -> arm / disarm the edit tool
    void exitEditMode();            // drop the tool if it is ours + uncheck

    // ---- look ----
    void onStyleCommit();
    void onActorTintToggle();
    void onHueCommit();
    void onAlphaCommit();
    void onShimmerSpeedCommit();
    void onShimmerAmountCommit();
    void onPixelCommit();
    void onGlitchCommit();
    void onDistortCommit();
    void onDistortAmountCommit();
    void onBrightnessCommit();      // [R2-1] night-scene output dimmer
    void onEffectFpsCommit();
    void onEntityLookCommit();

    // ---- pose ----
    void onClickFreeze();           // "Grab pose now" -> FROZEN snapshot
    void onClickLive();             // "Follow live" -> drop the snapshot

    // ---- crowd placement + legacy freeze-strip layout ----
    void onClickArray(ALGhostStudio::EFormation formation);
    void onClickBuildArray();
    void onClickCrowdConfirm();
    void onClickCrowdUnpin();
    void onClickCrowdCancel();
    void onFormationCommit();
    void captureFormationParameter();
    void configureFormationParameter(ALGhostStudio::EFormation formation);
    void importFormationParameters(
        const ALGhostStudio::FormationSpec& spec);
    bool formationSupportsFreezeStrip(
        ALGhostStudio::EFormation formation) const;
    ALGhostStudio::FormationSpec capturePlacementSpec() const;
    void updateOwnedCrowdPlacement();
    void refreshCrowdPlacement();
    void releaseOwnedCrowdPlacement();
    void onClickStartStrip();
    void onClickCancelStrip();
    void onMotionCommit();
    void onClickPickFacingTarget();
    void onCrowdFacingPointPicked(const LLUUID& prototype_id, bool accepted,
                                  const LLVector3d& point_global);
    void syncCrowdFacingPointPrototype();
    void onClickGroup();
    void onClickUngroup();
    void onLockModeCommit();
    void onGroupEditMembersCommit();
    void onGroupMemberPinnedCommit();
    void onClickGroupMemberReset();
    void onNameplateModeCommit();
    void onPoseRateCommit();

    // ---- master toggle ----
    void onShowAllToggle();
    void onResetNumeric(const std::string& target_name);

    // cached display name for a source id ("You" for null/self)
    static std::string sourceName(const LLUUID& id);

    // ---- change-diffing state ----
    std::string mListSig;           // composed instance-list signature last built
    // Cast signature the source combo was built from. Seeded to a sentinel that no
    // real signature can equal (a real one is "" for an empty cast, or ends in '|'),
    // so refreshSourceCombo() builds at least once -- otherwise a fresh login (empty
    // cast, sig == "") would early-return and never add the default "You" entry,
    // leaving the source dropdown blank.
    std::string mSourceSig{"<unbuilt>"};
    std::string mLookTargetSig;
    bool        mAnimationLibraryPopulated = false;  // filled once; Refresh re-scans
    LLUUID      mShownFor;          // instance the detail widgets were last loaded for
    U64         mObservedSelectionRevision = 0;
    bool        mEditMode = false;  // [R2-3] our transient edit tool is armed
    bool        mHintEdit = false;  // which hint string the header line shows

    // ---- widgets ----
    LLCheckBoxCtrl*   mShowAllCheck = nullptr;
    LLCheckBoxCtrl*   mEditModeCheck = nullptr;     // [R2-3]
    LLTextBox*        mHint = nullptr;              // [R2-3] swaps in edit mode
    LLScrollListCtrl* mList = nullptr;
    LLComboBox*       mSourceCombo = nullptr;
    LLComboBox*       mCloneTypeCombo = nullptr;
    LLButton*         mAddBtn = nullptr;
    LLButton*         mDupBtn = nullptr;
    LLButton*         mDelBtn = nullptr;
    LLButton*         mRefreshBtn = nullptr;
    LLTextBox*        mTypeText = nullptr;
    LLLineEditor*     mNameEdit = nullptr;

    LLSpinCtrl*       mPosX = nullptr;
    LLSpinCtrl*       mPosY = nullptr;
    LLSpinCtrl*       mPosZ = nullptr;
    ALCompassDial*    mHeadingDial = nullptr;
    LLSpinCtrl*       mYawSpin = nullptr;
    LLSpinCtrl*       mScaleSpin = nullptr;
    LLCheckBoxCtrl*   mChaosCheck = nullptr;
    LLSliderCtrl*     mChaosSlider = nullptr;
    LLSpinCtrl*       mAnimSpeedSpin = nullptr;
    LLCheckBoxCtrl*   mPhysicsCheck = nullptr;
    LLButton*         mAnimPauseBtn = nullptr;
    LLButton*         mAnimResumeBtn = nullptr;
    LLComboBox*       mDriveModeCombo = nullptr;
    LLLineEditor*     mDirectedAnimEdit = nullptr;
    LLComboBox*       mAnimationLibraryCombo = nullptr;
    LLButton*         mAnimationLibraryRefreshBtn = nullptr;
    LLComboBox*       mLoopModeCombo = nullptr;
    LLButton*         mAnimSyncBtn = nullptr;
    LLCheckBoxCtrl*   mLensGazeCheck = nullptr;
    LLButton*         mLensGazeSelectionBtn = nullptr;
    LLSliderCtrl*     mLensGazeTorsoSlider = nullptr;
    LLComboBox*       mLensGazeTargetMode = nullptr;
    LLComboBox*       mLensGazeCastTarget = nullptr;
    LLSliderCtrl*     mLensGazeHeadEyeSlider = nullptr;
    LLSliderCtrl*     mLensGazeIntensitySlider = nullptr;
    LLSliderCtrl*     mLensGazeSmoothingSlider = nullptr;
    LLTextBox*        mLensGazeStatus = nullptr;
    LLTextBox*        mAnimMetadataText = nullptr;
    LLView*           mLookSection = nullptr;
    LLTextBox*        mEntityStyleHint = nullptr;
    LLButton*         mPlaceBtn = nullptr;
    LLButton*         mToActorBtn = nullptr;
    LLButton*         mToMeBtn = nullptr;
    LLButton*         mDropBtn = nullptr;
    LLButton*         mAlignFeetBtn = nullptr;
    LLButton*         mUprightBtn = nullptr;
    LLButton*         mDupInPlaceBtn = nullptr;
    LLButton*         mCopyTransformBtn = nullptr;
    LLButton*         mPasteTransformBtn = nullptr;
    LLComboBox*       mLookTargetCombo = nullptr;
    LLButton*         mFaceNowBtn = nullptr;
    LLCheckBoxCtrl*   mKeepFacingCheck = nullptr;

    LLComboBox*       mStyleCombo = nullptr;
    LLCheckBoxCtrl*   mActorTintCheck = nullptr;
    LLSliderCtrl*     mHueSlider = nullptr;
    LLSliderCtrl*     mAlphaSlider = nullptr;
    LLSliderCtrl*     mShimmerSpeedSlider = nullptr;
    LLSliderCtrl*     mShimmerAmountSlider = nullptr;
    LLSliderCtrl*     mPixelSlider = nullptr;
    LLSliderCtrl*     mGlitchSlider = nullptr;
    LLComboBox*       mDistortCombo = nullptr;
    LLSliderCtrl*     mDistortAmountSlider = nullptr;
    LLSliderCtrl*     mBrightnessSlider = nullptr;    // [R2-1]
    LLSpinCtrl*       mEffectFps = nullptr;
    LLComboBox*       mEntityLookCombo = nullptr;

    LLButton*         mFreezeBtn = nullptr;
    LLButton*         mLiveBtn = nullptr;
    LLTextBox*        mPoseStatus = nullptr;
    LLTextEditor*     mRigDiagnostics = nullptr;
    LLFrameTimer      mRigDiagnosticsTimer;
    LLUUID            mRigDiagnosticsSelection;

    LLSpinCtrl*       mArrayCount = nullptr;
    LLSpinCtrl*       mArraySpacing = nullptr;
    LLButton*         mArrayLineBtn = nullptr;
    LLButton*         mArrayRingBtn = nullptr;
    LLComboBox*       mFormationCombo = nullptr;
    LLSpinCtrl*       mFormationParam = nullptr;
    LLButton*         mFormationParamReset = nullptr;
    LLSpinCtrl*       mFormationSeed = nullptr;
    LLSpinCtrl*       mFormationJitter = nullptr;
    LLComboBox*       mFormationGuideShape = nullptr;
    LLSpinCtrl*       mFormationGuideSize = nullptr;
    LLSpinCtrl*       mFormationEdgeGap = nullptr;
    LLSpinCtrl*       mBrigadeColumns = nullptr;
    LLSpinCtrl*       mBrigadeRows = nullptr;
    LLSpinCtrl*       mBrigadeFileSpacing = nullptr;
    LLSpinCtrl*       mBrigadeRankSpacing = nullptr;
    LLComboBox*       mFormationFacing = nullptr;
    LLCheckBoxCtrl*   mFormationTerrain = nullptr;
    // Body-facing bias, independent of the pattern's own wheel-driven
    // rotation (CrowdPlacementDraft::mFacingYawOffset).
    LLSpinCtrl*       mFacingYawOffset = nullptr;
    // FACING_SOURCE opt-in: keep the prototype's full pitched/rolled
    // quaternion instead of the default pure world-Z yaw extraction.
    LLCheckBoxCtrl*   mPreserveSourceTilt = nullptr;
    LLComboBox*       mCrowdCopiesAs = nullptr;
    LLComboBox*       mCrowdCreateMode = nullptr;
    LLComboBox*       mLockMode = nullptr;
    LLButton*         mPickFacingTarget = nullptr;
    LLButton*         mArrayBuildBtn = nullptr;
    LLButton*         mCrowdConfirmBtn = nullptr;
    LLButton*         mCrowdUnpinBtn = nullptr;
    LLButton*         mCrowdCancelBtn = nullptr;
    LLTextBox*        mCrowdPlacementStatus = nullptr;
    LLTextBox*        mCrowdInstructions = nullptr;
    LLComboBox*       mNameplateMode = nullptr;
    LLCheckBoxCtrl*   mGroupEditMembersCheck = nullptr;
    LLCheckBoxCtrl*   mGroupMemberPinnedCheck = nullptr;
    LLButton*         mGroupMemberResetBtn = nullptr;
    LLSpinCtrl*       mStripCount = nullptr;
    LLSpinCtrl*       mStripInterval = nullptr;
    LLButton*         mStripStartBtn = nullptr;
    LLButton*         mStripCancelBtn = nullptr;
    LLTextBox*        mStripStatus = nullptr;
    LLComboBox*       mMotionCombo = nullptr;
    LLSpinCtrl*       mMotionSpeed = nullptr;
    LLSpinCtrl*       mMotionAmplitude = nullptr;
    LLButton*         mMotionApplyBtn = nullptr;
    LLButton*         mGroupBtn = nullptr;
    LLButton*         mUngroupBtn = nullptr;
    LLSpinCtrl*       mPoseRate = nullptr;
    U32               mActiveStrip = 0;

    LLUUID            mPlacementOwner;
    U64               mLastCrowdDraftRevision = 0;
    bool              mApplyingCrowdDraft = false;
    bool              mSawOwnedCrowdDraft = false;
    std::string       mLastCrowdMessage;
    ALGhostStudio::EFormation mConfiguredFormation =
        ALGhostStudio::FORMATION_LINE;
    U32               mFormationGridColumns = 0;
    U32               mBrigadeColumnCount = 5;
    U32               mBrigadeRowCount = 4;
    F32               mBrigadeFileGap = 1.5f;
    F32               mBrigadeRankGap = 1.5f;
    F32               mFormationArcSweep = 120.f;
    F32               mFormationLaneGap = 2.f;
    F32               mFormationChevronAngle = 90.f;
    F32               mFormationHorseshoeOpening = 90.f;
    F32               mFormationSpiralTurns = 2.f;
    F32               mFormationAspectRatio = 1.f;
    U32               mFormationRayCount = 8;
    U32               mFormationZigzagColumns = 4;
    F32               mFormationStaggerOffset = 0.5f;
    LLUUID            mCrowdFacingPrototype;
    LLVector3d        mCrowdFacingPointGlobal;
    bool              mCrowdFacingPointValid = false;

    bool              mHasTransformClipboard = false;
    LLVector3d        mClipboardFoot;
    LLQuaternion      mClipboardRotation;
    F32               mClipboardScale = 1.f;

    LLTextBox*        mStatusText = nullptr;
};

#endif // AL_ALPANELGHOSTSTUDIO_H
