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

#include "aluirefresh.h"            // [GhostPanelPerf] revision gates + live clocks
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
    // ---- refreshers ----
    // [GhostPanelPerf] draw() only consults revisions / clocks and runs diff-guarded
    // writes (CLAUDE.md "UI PANELS MUST NOT DO WORK IN draw()"; aluirefresh.h).
    void refreshSourceCombo();      // "You" + cast members (rebuilt on cast revision)
    void refreshLookTargetCombo();  // camera/me + cast + other ghosts (cast + instance revisions)
    // instance rows: rebuilt on (instance, cast) revision. force_now = a handler
    // that just mutated the model wants the rows NOW (skips the >50-row 2 Hz cap).
    void refreshList(bool force_now = false);
    void refreshDetail();           // selected-instance widgets + enables (key-gated + 10 Hz)
    // [AutoAnimate]
    void refreshAutoAnimate(const ALGhostStudio::Instance* inst);
    void onAutoAnimToggle();
    void onAutoAnimWindow();
    void onAutoAnimAction(ALGhostAutoAnim::Action action);
    void onAutoAnimFile(bool save);
    static void autoAnimFileReply(const std::vector<LLUUID>& ids, bool save,
                                 const std::vector<std::string>& filenames);
    LLCheckBoxCtrl* mAutoAnimCheck = nullptr;
    LLSpinCtrl* mAutoAnimWindow = nullptr;
    LLTextBox* mAutoAnimStatus = nullptr;
    // [AutoAnimate] Resolve controls once, and cache the displayed status inputs.
    LLButton* mAutoAnimReset = nullptr;
    LLButton* mAutoAnimStop = nullptr;
    LLButton* mAutoAnimRelearn = nullptr;
    LLButton* mAutoAnimReseed = nullptr;
    LLButton* mAutoAnimGo = nullptr;
    LLButton* mAutoAnimFollow = nullptr;
    LLButton* mAutoAnimSave = nullptr;
    LLButton* mAutoAnimLoad = nullptr;
    ALGhostAutoAnim::LearnStatus mAutoAnimDisplay;
    S32 mAutoAnimDisplayMode = -1;
    bool mAutoAnimDisplayValid = false;
    void populateAnimationLibrary();   // mirror the shared ALGhostStudio animation library into the combo
    void refreshStatus();           // bottom status line (4 Hz)
    void refreshRigDiagnostics();   // selected entity clone/source prim state: on demand / revision, on screen only
    void onClickRigDiagRefresh();   // Refresh button: one snapshot WITH live render/cull columns
    // Legacy signature builders: used ONLY while GhostStudioRefreshAlways is on.
    std::string listSignature() const;
    std::string sourceSignature() const;
    std::string lookTargetSignature() const;

    // [GhostPanelPerf] Cached list selection, rebuilt only when the list selection,
    // the shared selection or the instance model moved (see selection()).
    struct SelectionCache
    {
        U64 mEpoch = ~0ULL;
        U64 mInstanceRev = ~0ULL;
        U64 mSharedRev = ~0ULL;
        S32 mNumSelected = -1;
        S32 mFirstIndex = -2;
        U64 mStamp = 0;                 // changes every time the cache is rebuilt
        LLUUID mRow;                    // raw first-selected row value (group id for headers)
        LLUUID mInstance;               // selectedInstance()
        std::vector<LLUUID> mInstances; // selectedInstances()
        bool mAnyGroup = false;
        bool mAnyEntity = false;        // any selected entity clone with a runtime id
    };
    const SelectionCache& selection();
    void noteListSelectionChanged() { ++mSelectionEpoch; }
    SelectionCache mSel;
    U64 mSelectionEpoch = 1;

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

    // ---- change-diffing state ([GhostPanelPerf]: revision gates, no per-frame strings) ----
    // A gate is "unprimed" until its first changed(), so the first draw always builds
    // (a fresh login with an empty cast must still add the default "You" entry), and
    // reset() in onVisibilityChange(true) forces a full refresh on re-show.
    ALRevisionGate mListGate;           // (instance revision, cast revision)
    ALRevisionGate mSourceGate;         // (cast revision)
    ALRevisionGate mLookTargetGate;     // (cast revision, instance revision)
    ALRevisionGate mDetailGate;         // refreshDetail key
    ALRevisionGate mCrowdGate;          // refreshCrowdPlacement key
    ALRevisionGate mSelectionGate;      // selection() cache (built-in 1 s fallback revalidates it)
    ALLiveReadoutClock mReadoutClock{10.f};    // live numeric readouts + detail tick
    ALLiveReadoutClock mStatusClock{4.f};      // status strings / gaze status
    ALLiveReadoutClock mListCapClock{2.f};     // >50-row list rebuild cap
    ALLiveReadoutClock mRigDiagPollClock{0.5f}; // 0.5 Hz structure re-snapshot while the editor is on screen
    bool mDetailDirty = true;           // a commit handler wants the detail block now
    bool mCrowdDirty = true;
    // Clock ticks sampled ONCE per draw() so several refreshers can share one tick.
    bool mReadoutTick = true;           // 10 Hz
    bool mStatusTick = true;            // 4 Hz
    S32  mParamConfigShown = -1;        // crowd parameter descriptor kind last applied to mFormationParam
    ALGhostStudio::EFormation mPendingFormation = ALGhostStudio::FORMATION_LINE; // requested while the spinner was edited
    bool mParamConfigPending = false;   // its configuration was deferred while the spinner was being edited
    U64  mAnimLibraryShownRev = 0;      // ALGhostStudio::animLibraryRevision() last shown in mAnimationLibraryCombo
    bool mCrowdImportPending = false;   // a draft import skipped an edited control; retry until all synced
    S32  mShownLookTarget = -1;         // look-target model values last formatted into mLookTargetCombo
    LLUUID mShownLookTargetId;
    U64  mStatusInstanceRev = 0;
    // Legacy signatures, touched ONLY while GhostStudioRefreshAlways is on.
    std::string mListSig;
    std::string mSourceSig{"<unbuilt>"};
    std::string mLookTargetSig;
    // Scale-spinner state last applied (limits/precision/tooltip are not rewritten per tick).
    bool mScaleCollapsedShown = false;
    bool mScaleStateValid = false;
    bool mScalePrecisionPending = true;
    LLUUID mDirectedAnimShown;          // directed-anim id last formatted into mDirectedAnimEdit
    F32  mScaleMinShown = 0.f;
    F32  mScaleMaxShown = 0.f;
    // Rig diagnostics: what the text widget currently shows, and why it may be stale.
    LLUUID mRigDiagShownSelection;
    LLUUID mRigDiagShownEntity;         // runtime id of the clone the text describes
    U64  mRigDiagShownCloneRev = 0;
    bool mRigDiagShown = false;         // false until a first build
    bool mRigDiagRefreshRequested = false;   // Refresh button: build once WITH live state
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
    LLButton*         mRigDiagRefreshBtn = nullptr;
    LLFrameTimer      mRigDiagnosticsTimer;     // GhostStudioRefreshAlways 0.5 s cadence only

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
