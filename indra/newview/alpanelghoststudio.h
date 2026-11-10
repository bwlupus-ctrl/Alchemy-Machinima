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
#include "alghostrotation.h"

#include "alscrollfit.h"
#include "aluirefresh.h"            // [GhostPanelPerf] revision gates + live clocks
#include "lluuid.h"
#include "llframetimer.h"
#include "v3dmath.h"
#include "llquaternion.h"

#include <map>
#include <string>
#include <vector>

class ALKitCard;

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLF32UICtrl;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLSpinCtrl;
class LLTabContainer;
class LLTextBox;
class LLTextEditor;
class ALCompassDial;

class ALPanelGhostStudio final : public LLPanel
{
public:
    ALPanelGhostStudio();
    ~ALPanelGhostStudio() override;

    bool postBuild() override;
    void reshape(S32 width, S32 height, bool called_from_parent = true) override;
    void draw() override;
    void onVisibilityChange(bool new_visibility) override;
    static void rewriteActorReferences(const LLUUID& old_id, const LLUUID& new_id);

    // [CloneAnimDrop] an inventory animation dropped on the Animation asset
    // card plays on the selected clones through the Inventory combo path.
    bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
                           EDragAndDropType cargo_type, void* cargo_data,
                           EAcceptance* accept, std::string& tooltip_msg) override;
    // Inventory context menu: play `item_id` (links resolved; owned
    // animations only) on the selected clones. False when nothing playable.
    static bool playInventoryAnimationOnSelectedClones(const LLUUID& item_id);
    // Enables that menu item: an entity clone is selected in the active host,
    // else in the shared Studio selection.
    static bool anyEntityCloneSelected();
    // Appends the Studio instance ids THIS panel's controls would act on: its own list
    // selection (selection().mInstances is already expanded: selectedInstances() replaces each
    // group row by its members), else the shared Studio selection when its list is empty. Director tag highlight reads this from the Director's own
    // Clones panel (never the global active host, which may be the standalone window).
    void appendSelectedInstances(std::vector<LLUUID>& out);
    // The host the user last worked in (shown, list selection, drop), while it
    // is still visible; the inventory menu applies to ITS selection.
    static ALPanelGhostStudio* activeHost();
    // Play on clones with ONE undo step on the active host's shell that
    // restores every clone's previous drive mode AND animation. Returns how
    // many clones now play it. Label carries the animation name only.
    // director_fallback: the world drop has no visible Clones host to own the undo
    // step; it asks for the Director shell as owner so the step is always recorded.
    static S32 playAnimationOnClonesRecorded(const std::vector<LLUUID>& ids,
                                             const LLUUID& asset_id,
                                             const std::string& anim_name,
                                             bool director_fallback = false);
    // [ClearAnim] Clear animations on these clones (entity clones only) and record
    // ONE undo step that restores each clone's drive mode, animation, AutoAnimate
    // and gaze. Returns how many clones were cleared.
    // director_fallback: the in-world menu has no visible Clones host to own the
    // undo step; it asks for the Director shell as owner. The page never does.
    static S32 clearAnimationsOnClonesRecorded(const std::vector<LLUUID>& ids,
                                               bool director_fallback = false);

private:
    friend struct ALCloneRotationProbe;
    friend struct ALGhostCloneMenuAccess;   // world-click clone selection (aldirectorselect.cpp)
    bool mSuppressLibraryScan = false;      // a hidden-host row refresh must not start the inventory scan
    ALScrollFit::Controller mScrollFit;

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
    void refreshSourceAnimations(const ALGhostStudio::Instance& inst);
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
        LLUUID mRow;                    // primary row value (group id for headers)
        std::vector<LLUUID> mRows;      // every selected row value, in list order
        LLUUID mInstance;               // selectedInstance()
        std::vector<LLUUID> mInstances; // selectedInstances()
        bool mAnyGroup = false;
        bool mAnyEntity = false;        // any selected entity clone with a runtime id
    };
    const SelectionCache& selection();
    void noteListSelectionChanged() { ++mSelectionEpoch; mRotationSelectionDirty = true; }
    SelectionCache mSel;
    U64 mSelectionEpoch = 1;

    // [CloneAnimDrop] shared apply for the Inventory combo, the card drop and
    // the inventory menu: Directed clones in the selection, else every selected
    // entity clone switched to Directed; toasts, never the asset id.
    static void applyInventoryAnimationTo(const std::vector<ALGhostAnimDrop::SelectedClone>& selected,
                                          const LLUUID& asset_id, const std::string& anim_name);
    void applyInventoryAnimation(const LLUUID& asset_id, const std::string& anim_name);
    void applyDirectedAnimToSelection(const LLUUID& anim);   // Directed members only
    std::vector<ALGhostAnimDrop::SelectedClone> selectedEntityClonesForApply() const;
    bool pointOverAnimAssetCard(S32 x, S32 y) const;
    // Never-expose rule: the "Anim UUID" editor text for a Directed asset.
    std::string directedAnimEditorText(const LLUUID& asset_id);
    void showDirectedAnim(const LLUUID& asset_id, bool force = false);
    ALKitCard* mAnimAssetCard = nullptr;
    U64 mDirectedAnimShownLibraryRev = ~0ULL;   // library / session-name / restriction
    U64 mDirectedAnimShownNameRev = ~0ULL;      // revisions the shown editor text was
    U64 mDirectedAnimShownRestrictionRev = ~0ULL; // formatted with
    void noteActiveHost();
    static ALPanelGhostStudio* sActiveHost;

    LLUUID selectedInstance() const;    // list selection -> instance id (null = none)
    // Primary row value (group id for headers): the last clicked row while it is still
    // selected, otherwise the first selected row. The list may hold more selected rows.
    LLUUID selectedListValue() const;
    std::vector<LLUUID> selectedRowValues() const; // every selected row value, in list order
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
    void onRotationCommit(S32 axis, F64 degrees);
    LLQuaternion rotationEditFrame(const LLUUID& id) const;
    ALGhostRotation::Angles rotationAngles(const LLUUID& id, const LLQuaternion& world);
    struct RotationReference
    {
        ALGhostRotation::Angles angles {};
        LLQuaternion frame;
        U64 resetRevision = 0;
    };
    std::map<LLUUID, RotationReference> mRotationReferences;
    bool mRotationSelectionDirty = false;
    ALGhostRotation::Angles mDisplayedRotation {}; // backing anchor before a native commit
    void reseedRotationReferences();
    void onScaleCommit();
    void onChaosCommit();
    void onAnimSpeedCommit();
    void onPhysicsCommit();
    void onClickAnimPause();
    void onClickAnimResume();
    void onDriveModeCommit();
    void onDirectedAnimCommit();
    void onAnimationLibraryCommit();
    void onDirectedMirrorCommit();
    void onSourceAnimationCommit();
    void onLoopModeCommit();
    void onClickAnimSync();
    void onCrowdVariationCommit();
    void refreshCloneQol(const ALGhostStudio::Instance* inst);
    LLButton* mSwapSourceBtn = nullptr;
    LLButton* mSoloBtn = nullptr;
    LLSpinCtrl* mStartOffset = nullptr;
    LLTextBox* mOffsetStatus = nullptr;
    std::string mSwapStatus;
    LLComboBox* mCrowdStartMode = nullptr;
    LLSpinCtrl* mCrowdStartSpread = nullptr;
    LLComboBox* mCrowdRippleFrom = nullptr;
    LLComboBox* mCrowdLooksMode = nullptr;
    LLComboBox* mCrowdLookSpread = nullptr;
    std::vector<LLComboBox*> mCrowdMixLooks;
    std::map<LLView*, LLView*> mQolLabels;
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
    void onClickPastePosition();
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
    void onSliceCommit();
    void onDropoutCommit();
    void onPowerFlickerCommit();
    void onWipeCommit();
    void onInterlaceCommit();
    void onPositionJitterCommit();
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
    // Formation "Parameter" reset (the floater's icon button and the kit row's reset glyph).
    void onFormationParamReset();
    void registerKitParameterReset();

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
    U64  mStatusSelStamp = 0;           // selection().mStamp the Count readout last reflected
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
    LLUUID      mPrimaryRow;        // [ListMultiSelect] last clicked row; see selectedListValue()
    bool        mEditMode = false;  // [R2-3] our transient edit tool is armed
    bool        mHintEdit = false;  // which hint string the header line shows

    // ---- widgets ----
    LLCheckBoxCtrl*   mShowAllCheck = nullptr;
    LLCheckBoxCtrl*   mEditModeCheck = nullptr;     // [R2-3]
    LLTextBox*        mHint = nullptr;              // [R2-3] swaps in edit mode
    LLScrollListCtrl* mList = nullptr;
    LLComboBox*       mSourceCombo = nullptr;
    bool             mSourceSelectionRemoved = false;
    LLComboBox*       mCloneTypeCombo = nullptr;
    LLButton*         mAddBtn = nullptr;
    LLButton*         mDupBtn = nullptr;
    LLButton*         mClearAnimBtn = nullptr;  // [ClearAnim] kit page only
    void onClickClearAnimations();
    LLButton*         mDelBtn = nullptr;
    LLButton*         mRefreshBtn = nullptr;
    LLTextBox*        mTypeText = nullptr;
    LLLineEditor*     mNameEdit = nullptr;

    LLSpinCtrl*       mPosX = nullptr;
    LLSpinCtrl*       mPosY = nullptr;
    LLSpinCtrl*       mPosZ = nullptr;
    ALCompassDial*    mHeadingDial = nullptr;
    LLSpinCtrl*       mYawSpin = nullptr;
    LLSpinCtrl*       mPitchSpin = nullptr;
    LLSpinCtrl*       mRollSpin = nullptr;
    LLSpinCtrl*       mScaleSpin = nullptr;
    LLCheckBoxCtrl*   mChaosCheck = nullptr;
    LLSliderCtrl*     mChaosSlider = nullptr;
    LLSpinCtrl*       mAnimSpeedSpin = nullptr;
    LLCheckBoxCtrl*   mPhysicsCheck = nullptr;
    LLButton*         mAnimPauseBtn = nullptr;
    LLButton*         mAnimResumeBtn = nullptr;
    LLComboBox*       mReflectPlane = nullptr;
    LLComboBox*       mReflectOrientation = nullptr;
    LLCheckBoxCtrl*   mReflectPosition = nullptr;
    LLCheckBoxCtrl*   mMirrorLightRig = nullptr;
    LLButton*         mReflectPair = nullptr;
    LLButton*         mReflectRecapture = nullptr;
    LLTextBox*        mReflectStatus = nullptr;
    LLComboBox*       mDriveModeCombo = nullptr;
    LLLineEditor*     mDirectedAnimEdit = nullptr;
    LLCheckBoxCtrl*   mDirectedMirrorCheck = nullptr;
    LLComboBox*       mSourceAnimationsCombo = nullptr;
    LLUUID mSourceAnimationsSource;
    U64 mSourceAnimationsLibraryRev = ~0ull;
    U64 mSourceAnimationsMetadataRev = ~0ull;
    std::vector<LLUUID> mSourceAnimationsShown;
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
    LLView*           mLookSection = nullptr;      // floater: the panel holding the overlay style controls
    std::vector<LLView*> mLookControls;            // kit page: the same controls, hidden with their rows
    std::vector<ALKitCard*> mLookCards;            // kit page: overlay-only card shells
    LLTabContainer*   mTabs = nullptr;             // floater only; null on the Director kit page
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
    LLButton*         mPastePositionBtn = nullptr;
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
    LLSliderCtrl*     mSliceSlider = nullptr;
    LLSliderCtrl*     mDropoutSlider = nullptr;
    LLSliderCtrl*     mPowerFlickerSlider = nullptr;
    LLSliderCtrl*     mWipeSlider = nullptr;
    LLSliderCtrl*     mInterlaceSlider = nullptr;
    LLSliderCtrl*     mPositionJitterSlider = nullptr;
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
    LLSpinCtrl*       mArrayWidth = nullptr;
    LLSpinCtrl*       mArrayDepth = nullptr;
    LLSpinCtrl*       mArrayColumns = nullptr;
    LLSpinCtrl*       mArrayRows = nullptr;
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
