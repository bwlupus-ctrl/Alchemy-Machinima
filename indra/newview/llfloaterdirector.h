/**
 * @file llfloaterdirector.h
 * @brief Director Console: one floater over the whole machinima rig.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Pure view over LLDirectorCast (D1 engine) plus the existing machinima
 * singletons: ACTION/CUT transport with arming + countdown, the persistent
 * cast column, and Move / Path / Animate / Camera / Takes tabs re-homing the
 * Actor Mover, Cinematic Camera (shared ALPanelCineCamParams), Flycam Orbit
 * (shared ALPanelFlycamOrbit) and Flycam Recorder (shared ALPanelFlycamRecorder)
 * controls. Everything underneath is the same settings/state the standalone
 * floaters use -- no forks.
 */

#ifndef LL_LLFLOATERDIRECTOR_H
#define LL_LLFLOATERDIRECTOR_H

#include "alscrollfit.h" // [DirFix] D1: mScrollFit (wrap/fit/focus-reveal/wheel guard, per instance)
#include "aluirefresh.h"
#include "llfloater.h"
#include "llframetimer.h"
#include "lltimer.h"

#include <functional>
#include <boost/signals2.hpp>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

class ALPanelActorMover;
class ALPanelAnimPreview;
class ALPanelCineCamParams;
class ALPanelLensGaze;
class ALPanelPathEditor;
class ALKitShell;
class ALKitHistory;
class ALKitBar;
class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLContextMenu;
class LLLineEditor;
class LLPanel;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLSpinCtrl;
class LLTabContainer;
class LLTextBox;
class LLView;

class LLFloaterDirector final : public LLFloater
{
public:
    LLFloaterDirector(const LLSD& key);
    ~LLFloaterDirector() override;
    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void onClose(bool app_quitting) override;
    void onFocusLost() override;
    void setMinimized(bool minimized) override;
    void draw() override;
    // Esc = CUT while the transport is running or counting down; stock
    // floater behavior otherwise
    bool handleKeyHere(KEY key, MASK mask) override;
    bool handleUnicodeCharHere(llwchar uni_char) override;
    bool hasAccelerators() const override;
    bool handleMouseDown(S32 x,S32 y,MASK mask) override;
    bool handleRightMouseDown(S32 x,S32 y,MASK mask) override;
    bool handleScrollWheel(S32 x,S32 y,LLScrollDelta delta) override;
    ALKitHistory* kitHistory();
    static void startRecoveryService();
    static void prepareRecoveryForShutdown();
    static void cancelRecoveryShutdown();
    static void removeRecoveryFile();
    // [SliderWheel] [DirFix] D1's wheel overrides are retired: the scroll-through guard is global
    // in llui (indra/llui/llsliderwheel.h).

    // [DirFix] D10: read-only public view of the cast-list selection, so the
    // Actor Gaze floater can pull it on open (see doc/DIRECTOR_CONSOLE_FIX_PLAN.md)
    uuid_vec_t currentCastSelection() const { return selectedCastIds(); }

    // The Animate-tab preview dummy + its drag-to-rotate mouse handling live in
    // the shared ALPanelAnimPreview embedded in the tab, not on this floater.

private:
    bool postBuildV2();
    void bindChrome(LLView* root);
    void bindPage(const std::string& page, LLView* root);
    void bindMovePage(LLView* root);
    void bindPathPage(LLView* root);
    void bindActorStylePage(LLView* root);
    void bindAnimatePage(LLView* root);
    void bindCameraPage(LLView* root);
    void bindOperatorPage(LLView* root);
    void bindFrameOrbitPage(LLView* root);
    void bindLauncherPage(LLView* root);
    // Look pages (generated kit pages): wire the code-bound buttons, palette side effects and
    // Lightbox links to the same ALRotoInk / ALTron / ALPanelCineLightRig calls the Lightbox uses.
    void bindRotoInkPage(LLView* root);
    void bindTronPage(LLView* root);
    void bindSteppedPage(LLView* root);
    void bindDirectorSettingsPage(LLView* root);
    // Settings > Scene recovery: the crash-backup list (newest first) with Restore / Delete.
    // Both confirm first and carry the exact backup path the dialog describes (no reselect).
    static void requestRestoreCrashBackup();
    static void requestDeleteCrashBackup();
    static void onRecoveryResponse(const LLSD& notification, const LLSD& response);
    static void onRestoreRecoveryConfirmed(const LLSD& notification, const LLSD& response);
    static void onDeleteCrashBackupConfirmed(const LLSD& notification, const LLSD& response);
    static void refreshRecoveryControls();
    void bindLightActorsPage(LLView* root);
    void bindImagePage(LLView* root);
    // Lens, World and Look-adjacent generated kit pages: code-bound buttons and preset actions
    // only; the setting-bound rows need no binder. Same calls as the v1 panels.
    void bindLensFocusPage(LLView* root);
    void bindLensFiltersPage(LLView* root);
    void bindWeatherPage(LLView* root);
    void bindAtmospherePage(LLView* root);
    void refreshWorldLensReadouts(const std::string& page);
    // ---- transport ----
    void onClickAction();               // ACTION, or CUT while counting/running
    void onClickTakePlay();
    void onClickTakeStop();
    void onClickSetMarks();
    void onClickResetMarks();
    void refreshTransport();
    void updateTransportTooltipCache();
    void showTransportToast(const std::string& text);

    // ---- scene files (transport bar) ----
    static std::string scenesDir();     // created on demand
    static std::string sanitizeSceneName(std::string name);
    static std::string scenePath(const std::string& name);
    // every settings-backed value a scene captures besides the cast/subjects
    // (which LLDirectorCast::sceneData() owns) and the CineCam mode/preset
    // (which get special ordering on load)
    static const std::vector<std::string>& sceneSettingsList();
    void refreshSceneList(const std::string& select_name = std::string());
    void onSceneSelected();             // combo commit = metadata preview only
    void onClickSceneLoad();
    void onClickSceneSave();            // reveal the inline name editor / commit
    void onClickSceneSaveAs();
    void onClickSceneRename();
    void commitSceneName();             // editor commit (Enter) or second Save
    void onClickSceneDelete();
    bool saveScene(const std::string& name);
    // recovery = true loads the crash backup at recovery_path (it is never deleted by the load).
    bool loadScene(const std::string& name, bool recovery = false,
                   const std::string& recovery_path = std::string());
    static LLSD captureSceneData();
    static bool writeSceneFile(const std::string& path, const std::string& label, bool notify_failure);
    static void initializeSceneRecovery();
    static void idleSceneRecovery(bool force_write = false);
    static void updateSceneDirty();
    static void markSceneClean();
    void retargetPathFromCastSelection();
    static void offerSceneRecovery();
    // Autosave always runs: it writes the first live slot that does not hold an unmoved
    // previous-session backup (".director_recovery.xml", then "_2", "_3", ...).
    static std::string autosavePath();
    // <scenes>/crash_backups/ (created on demand); the live files of a previous session are
    // MOVED here at login and the newest ALDirectorQol::kCrashBackupKeep are kept.
    static std::string crashBackupsDir();
    static void moveLiveBackupsAtLogin();
    static void retryPendingBackupMoves();
    // RESTORE_* result of restoring one crash backup (the file is never deleted by a restore).
    static S32 restoreCrashBackup(const std::string& path);
    static void sceneRecoveryIdle(void*);

    // ---- tabs ----
    void onTabChanged();                // remember the active tab (DirectorLastTabName + DirectorLastTab)
    LLPanel* currentLeafPanel();
    bool selectLeafTab(const std::string& name);
    std::string pageKeyAt(S32 x,S32 y) const;
    bool isTextEntryFocus() const;

    // ---- legend popover ----
    void onToggleLegend();

    // ---- cast column ----
    void refreshCastList();
    void onCastRightClick(LLUICtrl* ctrl, S32 x, S32 y);
    void onClickAddYou();
    void onClickAddAnimesh();            // add in-world-selected animesh object(s) to the cast
    void onClickFocusActor();           // frame the selected member in the camera
    uuid_vec_t selectedCastIds() const;
    LLUUID     firstSelectedCastId() const;
    const uuid_vec_t& actorTargetsForPage(const std::string& page, const uuid_vec_t& selected);
    // context-menu / button ops (act on the list selection)
    void onCastSetSubject(S32 subject);
    void onCastSetMarkHere();
    void onCastResetToMark();
    void onCastClearLocoAnim();
    // [AutoAnimate] Runtime cast ids are resolved to stable Studio instance ids.
    bool castHasGhostEntity() const;
    void onCastAutoAnim(S32 operation);
    void onCastCopyUUID();
    void onCastRemove();
    // ---- groups ----
    void onCommitGroupAssign();         // tag the selected member(s) ("" / "(none)" = ungroup)
    void onClickGroup(bool start);      // Start/Stop every member of the picked group
    void refreshGroupControls();        // combos rebuilt only when the name set changed
    // Groups management section (Move tab): list of groups with member counts
    // + start delays; selecting a row highlights its members in the cast list
    // and loads the delay spinner / rename editor
    std::string selectedManagedGroup() const;   // groups_list selection ("" = none)
    void onGroupsListSelect();
    void onCommitGroupDelay();          // spinner -> the selected group's start delay
    void onClickGroupRename();          // rename editor commit / Rename button
    void onClickGroupDissolve();

    // ---- Move tab ----
    // [DirFix] D10: the per-frame cast selection is computed once in draw() and passed in
    void refreshMoveTab(const uuid_vec_t& selected);  // feed the shared transport panel the cast selection

    // ---- Path tab ----
    void refreshPathTab(const uuid_vec_t& selected);  // point the embedded editor at the selection

    // ---- Actor FX tab ----
    void refreshActorStyleTab();        // roster + model -> controls
    void onActorStyleTargetChanged();
    void onActorStyleControlChanged();  // controls -> selected actor's style
    void onActorStyleReset();
    bool hasActorStyleTarget() const;
    LLUUID actorStyleTargetId() const;   // null is the explicit "You" row

    // ---- Animate tab ----
    void refreshAnimateTab(const uuid_vec_t& selected);
    void onAnimRightClick(LLUICtrl* ctrl, S32 x, S32 y);
    LLUUID selectedAnimId() const;      // selected row's anim asset id
    LLUUID pasteAnimId() const;         // validated paste-row UUID (null = invalid)
    void onAnimCopyUUID();
    void onAnimSetLoco();               // row -> selected member's selected locomotion role
    void onAnimPlayLocal(bool play);    // start/stopMotion, client-side only
    void onPastePlayLocal(bool play);
    void onPasteSetLoco();
    void onClickClearLoco();
    S32 selectedLocomotionRole() const;
    void onLocomotionRoleChanged();
    void onLocomotionMetadataCommit();
    void applySelectedLocomotion(const LLUUID& anim);
    void refreshLocomotionRoleEditor(const LLUUID& member_id);
    // The shared preview panel is fed the anim to show + the object playing it:
    LLUUID previewAnimId() const;                       // selected row, else pasted uuid
    LLUUID animSourceObject(const LLUUID& anim_id) const; // the object playing it on you

    // ---- Camera tab ----
    static LLUUID avatarFromSelection();
    void onClickSetSubjectFromSelection(S32 subject);
    void onClickAssignSubjects();
    void onClickClearSubject(S32 subject);
    void onOpenActorGaze();
    void onClickManagePrism();
    void onClickBuildOtsPair();
    void onClickZolly();
    void onClickHeroArc();
    void refreshGazeSelection(const uuid_vec_t& selected);
    void refreshSubjects();
    void refreshPrismDependents();

    // ---- launcher cards ----
    void addLauncherCard(const std::string& id, std::function<void()> open_fn,
                         std::function<std::string()> summary_fn);
    void refreshLauncherCards();

    // ---- Takes tab ----
    // the shared ALPanelFlycamRecorder (panel_flycam_recorder.xml) is embedded
    // in the Takes tab and owns its own wiring + refresh -- nothing here.

    // ---- status strip ----
    void refreshStatusStrip();

    // cached name for a cast id (member cache -> name cache -> animesh tag)
    static std::string castMemberName(const LLUUID& id);

    // set a tooltip only when it changed (draw()-rate friendly)
    static void setToolTipIfChanged(LLUICtrl* ctrl, const std::string& tip);

    // transport
    LLButton* mActionBtn = nullptr;
    LLButton* mCutBtn = nullptr;
    LLButton* mSetMarksBtn = nullptr;
    LLButton* mResetMarksBtn = nullptr;
    LLButton* mArmBtn = nullptr;
    LLButton* mTakePlayBtn = nullptr;
    LLButton* mTakeStopBtn = nullptr;
    LLTextBox* mTakeTimecode = nullptr;
    LLButton* mCastChip = nullptr;
    LLTextBox* mTransportToast = nullptr;
    ALKitBar* mTransportBar = nullptr;
    std::string mActionTooltip;
    std::string mArmTooltip;
    bool mActionArmed = false;
    S32 mLastTransportActionState = -1;
    S64 mTimecodeFrame = -1;
    S32 mTimecodeFps = -1;
    bool mTimecodeHot = false;
    S32 mLastCastChipCount = -1;
    S32 mTakeTooltipState = -1;
    LLFrameTimer mTransportToastTimer;

    // scene files (transport bar)
    LLComboBox*   mSceneCombo = nullptr;
    LLLineEditor* mSceneNameEditor = nullptr;   // revealed in the combo's spot
    LLButton*     mSceneSaveBtn = nullptr;
    LLButton*     mSceneDeleteBtn = nullptr;
    LLButton*     mSceneLoadBtn = nullptr;
    LLButton*     mSceneRenameBtn = nullptr;    // v2 kit_scene_popover only
    LLTextBox*    mSceneDirtyText = nullptr;
    LLTextBox*    mSceneMetadataText = nullptr;
    std::string   mSceneRenameSource;
    std::string   mSceneWriteSource;
    bool          mSceneSaveAs = false;
    struct SceneChangeStamp
    {
        U64 mSettings = 0;
        U64 mCast = 1;
        U64 mPaths = 1;
        U64 mPrism = 1;
        U64 mRig = 1;
        U64 mLightResponse = 1;
        bool operator==(const SceneChangeStamp& other) const
        {
            return mSettings == other.mSettings && mCast == other.mCast &&
                   mPaths == other.mPaths && mPrism == other.mPrism &&
                   mRig == other.mRig && mLightResponse == other.mLightResponse;
        }
        bool operator!=(const SceneChangeStamp& other) const { return !(*this == other); }
    };
    struct RecoveryServiceState
    {
        SceneChangeStamp mCleanSceneStamp;
        U64 mSettingsRevision = 1;
        bool mHaveCleanSceneStamp = false;
        bool mSceneDirty = false;
        std::string mCurrentSceneName;
        LLFrameTimer mAutosaveTimer;
        std::vector<boost::signals2::scoped_connection> mSettingConnections;
    };
    static RecoveryServiceState& recoveryServiceState();
    static SceneChangeStamp currentSceneStamp();
    std::string   mSceneMetadataTooltip;
    ALPanelCineCamParams* mCineCamPanel = nullptr;  // embedded shared params panel

    // left-rail tabs + legend popover
    LLTabContainer* mTabContainer = nullptr;
    std::vector<std::pair<LLPanel*, LLTabContainer*>> mGroups;
    // [DirFix] D1: wrap + auto-fit + focus-reveal + wheel rule for every tab (incl. nested)
    ALScrollFit::Controller mScrollFit;
    LLPanel*        mDancePage = nullptr;
    LLView*         mDancePanelView = nullptr;
    LLPanel*        mExposurePage = nullptr;
    LLView*         mExposurePanelView = nullptr;
    LLPanel*        mGhostPage = nullptr;
    LLView*         mGhostPanelView = nullptr;
    // [DirFix] D8: set on tab change / open; the hidden-tab-gated refreshers (Actor FX,
    // Animate) discard their "last written" caches the first visible pass after it
    bool            mActorStyleTabDirty = true;
    bool            mAnimateTabDirty = true;
    bool            mGroupsTabDirty = true;
    // [DirFix] D10: last cast selection pushed to the Actor Gaze floater (push on change only)
    uuid_vec_t      mLastGazePush;
    ALPanelLensGaze* mGazePanel = nullptr;
    LLButton*       mHelpBtn = nullptr;
    LLPanel*        mLegendPanel = nullptr;
    LLPanel*        mHotkeysPanel = nullptr;

    // cast column
    LLScrollListCtrl* mCastList = nullptr;
    LLTextBox*        mCastRosterHeader = nullptr;
    LLButton*         mRemoveBtn = nullptr;
    LLButton*         mFocusBtn = nullptr;
    LLTextBox*        mCastHint = nullptr;
    LLHandle<LLContextMenu> mCastMenuHandle;
    // per-row last-applied cell state, parallel to the list rows, so cells
    // are only re-set when something actually changed (v2 mover idiom)
    struct CastRowState
    {
        std::string mIcon;
        std::string mName;      // includes the "(away)" / group suffixes
        std::string mAB;
        std::string mMark;
        std::string mLook;
        bool        mInWorld = true;
    };
    std::vector<CastRowState> mRowStates;
    uuid_vec_t mPinnedActorTarget;
    // [DirFix] D7: scroll-list column indices, resolved once in postBuild (XML-declared columns)
    S32 mCastStatusCol = 0;
    S32 mCastNameCol = 0;
    S32 mCastABCol = 0;
    S32 mCastMarkCol = 0;
    S32 mCastLookCol = 0;
    S32 mAnimPrioCol = 0;
    S32 mAnimPlayingCol = 0;

    // groups: the assign combo under the cast list (free-typed or picked) and
    // the Move tab's group transport (selector + Start/Stop). Both combos are
    // rebuilt only when the distinct-name set changes (draw()-rate friendly).
    LLComboBox* mGroupAssignCombo = nullptr;
    LLComboBox* mGroupRunCombo = nullptr;
    LLButton*   mGroupStartBtn = nullptr;
    LLButton*   mGroupStopBtn = nullptr;
    std::vector<std::string> mLastGroupNames;   // last set the combos were built from
    LLUUID      mGroupShownFor;                 // whose group the assign combo shows
    // Groups management section (Move tab): the list is rebuilt only when its
    // composed name/count/delay snapshot changes (draw()-rate friendly)
    LLScrollListCtrl* mGroupsList = nullptr;
    LLSpinCtrl*       mGroupDelaySpinner = nullptr;
    LLLineEditor*     mGroupRenameEditor = nullptr;
    LLButton*         mGroupRenameBtn = nullptr;
    LLButton*         mGroupDissolveBtn = nullptr;
    std::vector<std::string> mGroupsListSnapshot;   // "name|count|delay" rows last built
    std::string       mGroupDelayShownFor;          // whose delay the spinner shows

    // Move tab -- the shared transport (scope, heading dial, params, Walk/Stop);
    // the console feeds it the cast selection each draw
    ALPanelActorMover* mMoverPanel = nullptr;

    // Path tab (dedicated) -- shared waypoint editor, targets the selection
    ALPanelPathEditor* mPathPanel = nullptr;

    // Actor FX tab -- a direct editor over LLDirectorCast::ActorStyle. The
    // selector is intentionally independent of Subjects A-D; null UUID means
    // You, while non-null entries are stable cast-member ids (away actors keep
    // their authored style for when they return).
    LLComboBox*      mActorStyleTarget = nullptr;
    LLCheckBoxCtrl*  mActorStyleEnabled = nullptr;
    LLComboBox*      mActorStyleMode = nullptr;
    LLComboBox*      mActorStyleLook = nullptr;
    LLCheckBoxCtrl*  mActorStyleUseActorHue = nullptr;
    LLSliderCtrl*    mActorStyleHue = nullptr;
    LLSliderCtrl*    mActorStyleAlpha = nullptr;
    LLSliderCtrl*    mActorStyleDissolveProgress = nullptr;
    LLSliderCtrl*    mActorStylePixel = nullptr;
    LLSliderCtrl*    mActorStyleShimmerSpeed = nullptr;
    LLSliderCtrl*    mActorStyleShimmerAmount = nullptr;
    LLSliderCtrl*    mActorStyleGlitch = nullptr;
    LLComboBox*      mActorStyleDistortion = nullptr;
    LLSliderCtrl*    mActorStyleDistortionAmount = nullptr;
    LLSliderCtrl*    mActorStyleBrightness = nullptr;
    LLSpinCtrl*      mActorStyleEffectFps = nullptr;
    LLButton*        mActorStyleReset = nullptr;
    LLTextBox*       mActorStyleStatus = nullptr;
    // [DirFix] D4: allocation-free roster compare (replaces the per-frame signature string)
    struct ActorStyleRosterRow
    {
        LLUUID      mId;
        std::string mName;      // CastMember::mLastName at build time
        bool        mInWorld = false;
    };
    std::vector<ActorStyleRosterRow> mActorStyleRoster;
    bool             mActorStyleRosterBuilt = false;    // false = next pass rebuilds (first pass / forced)
    bool             mActorStyleRosterHadSelf = false;
    LLUUID           mActorStyleRosterSelfId;
    U64              mActorStyleChoiceRevision = 0;
    std::string      mActorStyleSnapshot;
    LLUUID           mActorStyleShownFor;
    LLUUID           mActorStyleLastCastSelection;
    bool             mActorStyleHadTarget = false;
    bool             mActorStyleHaveLastCastSelection = false;
    bool             mRefreshingActorStyle = false;

    // Camera tab
    LLTextBox* mSubjectAText = nullptr;
    LLTextBox* mSubjectBText = nullptr;
    LLTextBox* mSubjectCText = nullptr;
    LLTextBox* mSubjectDText = nullptr;
    LLButton*  mSetABtn = nullptr;
    LLButton*  mSetBBtn = nullptr;
    LLButton*  mSetCBtn = nullptr;
    LLButton*  mSetDBtn = nullptr;
    LLButton*  mClearABtn = nullptr;
    LLButton*  mClearBBtn = nullptr;
    LLButton*  mClearCBtn = nullptr;
    LLButton*  mClearDBtn = nullptr;
    LLButton*  mAssignSubjectsBtn = nullptr;
    LLTextBox* mPrismSummaryText = nullptr;
    LLButton*  mPrismManageBtn = nullptr;
    LLButton*  mOtsPairBtn = nullptr;
    LLButton*  mZollyBtn = nullptr;
    LLButton*  mHeroArcBtn = nullptr;
    U64        mPrismConfigurationRevision = 0;
    U64        mPrismRuntimeRevision = 0;
    bool       mHavePrismSummary = false;
    LLFrameTimer mPrismRefreshTimer;

    struct LauncherCard
    {
        LLTextBox* mSummary = nullptr;
        std::function<std::string()> mFn;
    };
    std::vector<LauncherCard> mLauncherCards;
    ALLiveReadoutClock mLauncherClock{2.f};
    LLTimer mBuildTimer;

    // Animate tab
    LLTextBox*        mAnimateHeader = nullptr;
    LLScrollListCtrl* mAnimList = nullptr;
    LLTextBox*        mAnimHint = nullptr;
    LLButton*         mAnimCopyBtn = nullptr;
    LLButton*         mAnimSetLocoBtn = nullptr;
    LLButton*         mAnimPlayBtn = nullptr;
    LLButton*         mAnimStopBtn = nullptr;
    LLLineEditor*     mPasteEditor = nullptr;
    // [DirFix] D5: paste-row cache keyed on the full editor text (pasteAnimId() is a pure
    // function of it), so the per-frame path does one string compare, not a copy + trim + validate
    std::string       mPasteTextSeen;
    LLUUID            mPasteIdSeen;
    bool              mPasteSeenEmpty = true;
    bool              mPasteCacheValid = false;
    LLButton*         mPastePlayBtn = nullptr;
    LLButton*         mPasteStopBtn = nullptr;
    LLButton*         mPasteSetLocoBtn = nullptr;
    LLComboBox*       mLocomotionRoleCombo = nullptr;
    LLSpinCtrl*       mLocomotionSpeed = nullptr;
    LLSpinCtrl*       mLocomotionPhase = nullptr;
    LLCheckBoxCtrl*   mLocomotionAllowAO = nullptr;
    LLTextBox*        mLocoText = nullptr;
    LLButton*         mClearLocoBtn = nullptr;
    LLHandle<LLContextMenu> mAnimMenuHandle;
    LLUUID mLocomotionEditorMember;
    LLUUID mLocomotionEditorAnim;
    S32    mLocomotionEditorRole = -1;
    F32    mLocomotionEditorSpeed = -1.f;
    F32    mLocomotionEditorPhase = -2.f;
    bool   mLocomotionEditorAllowAO = false;
    bool   mLocomotionEditorLoaded = false;
    // shared preview pane + own-avatar controls (owns its own dummy); fed the
    // selected/pasted anim + its source object each draw
    ALPanelAnimPreview* mAnimPreviewPanel = nullptr;
    // change-diffing: rows rebuilt only when the shown member or their
    // signaled-animation set changed; prio/playing cells re-set only on change
    LLUUID mAnimAvatarId;
    std::vector<std::pair<LLUUID, S32>> mAnimSnapshot;
    struct AnimRowState
    {
        std::string mPrio;
        std::string mPlaying;
    };
    std::vector<AnimRowState> mAnimRowStates;

    // Takes tab: the shared ALPanelFlycamRecorder is embedded in the XML and
    // self-contained -- no widget pointers to hold here.

    // status strip
    LLTextBox* mStatusStrip = nullptr;
    LLTextBox* mRecIndicator = nullptr;
    S32        mRecState = -1;       // -1 unknown, 0 idle, 1 capturing (diffing)
    S32        mStatusMoving = -1;
    S32        mStatusQueued = -1;
    S32        mStatusCamMode = -1;
    S32        mStatusSwitcherSlot = -2;
    U64        mStatusCastRevision = 0;
    bool       mStatusCamEnabled = false;
    bool       mStatusSwitching = false;
    bool       mStatusOrbitEnabled = false;
    bool       mStatusSelectionTarget = false;
    LLUUID     mStatusTargetId;
    std::string mRoutineStatus;
    std::string mCastChipTooltip;
    ALKitShell* mShell = nullptr;
    bool mIsV2 = false;
    LLTextBox* mKitUIMs = nullptr;
    LLTimer mKitDrawTimer;
    F32 mKitDrawEMA = 0.f;
    ALLiveReadoutClock mKitDrawClock{2.f};
    LLComboBox* mCrashBackupCombo = nullptr;
    LLButton* mCrashBackupRestoreButton = nullptr;
    LLButton* mCrashBackupDeleteButton = nullptr;
    boost::signals2::scoped_connection mActorMaskTestWatch;
    boost::signals2::scoped_connection mPageBuiltConnection;
    boost::signals2::scoped_connection mPageSelectedConnection;
    boost::signals2::scoped_connection mKitUIMsConnection;
    // Kit Time page: scale commit side effect driven from the TemporalWorldScale signal.
    // Kit Operator/Frame pages: frame aspect readback from CinematicFrameAspectRatio.
    std::vector<boost::signals2::scoped_connection> mFrameAspectConnections;
    std::vector<boost::signals2::scoped_connection> mWorldLensConnections;
    std::unordered_set<std::string> mBoundPages;
};

#endif // LL_LLFLOATERDIRECTOR_H
