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

#include "llfloater.h"

#include <string>
#include <utility>
#include <vector>

class ALPanelActorMover;
class ALPanelAnimPreview;
class ALPanelCineCamParams;
class ALPanelPathEditor;
class LLButton;
class LLComboBox;
class LLContextMenu;
class LLLineEditor;
class LLPanel;
class LLScrollListCtrl;
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
    void draw() override;
    // Esc = CUT while the transport is running or counting down; stock
    // floater behavior otherwise
    bool handleKeyHere(KEY key, MASK mask) override;

    // The Animate-tab preview dummy + its drag-to-rotate mouse handling live in
    // the shared ALPanelAnimPreview embedded in the tab, not on this floater.

private:
    // ---- transport ----
    void onClickAction();               // ACTION, or CUT while counting/running
    void onClickSetMarks();
    void onClickResetMarks();
    void refreshTransport();

    // ---- scene files (transport bar) ----
    static std::string scenesDir();     // created on demand
    static std::string scenePath(const std::string& name);
    // every settings-backed value a scene captures besides the cast/subjects
    // (which LLDirectorCast::sceneData() owns) and the CineCam mode/preset
    // (which get special ordering on load)
    static const std::vector<std::string>& sceneSettingsList();
    void refreshSceneList(const std::string& select_name = std::string());
    void onSceneSelected();             // combo commit = load
    void onClickSceneSave();            // reveal the inline name editor / commit
    void commitSceneName();             // editor commit (Enter) or second Save
    void onClickSceneDelete();
    void saveScene(const std::string& name);
    void loadScene(const std::string& name);

    // ---- tabs ----
    void onTabChanged();                // remember the active tab (DirectorLastTab)

    // ---- legend popover ----
    void onToggleLegend();

    // ---- cast column ----
    void refreshCastList();
    void onCastRightClick(LLUICtrl* ctrl, S32 x, S32 y);
    void onClickAddYou();
    void onClickFocusActor();           // frame the selected member in the camera
    uuid_vec_t selectedCastIds() const;
    LLUUID     firstSelectedCastId() const;
    // context-menu / button ops (act on the list selection)
    void onCastSetSubject(bool subject_a);
    void onCastSetMarkHere();
    void onCastResetToMark();
    void onCastClearLocoAnim();
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
    void refreshMoveTab();              // feed the shared transport panel the cast selection

    // ---- Path tab ----
    void refreshPathTab();              // point the embedded editor at the selection

    // ---- Animate tab ----
    void refreshAnimateTab();
    void onAnimRightClick(LLUICtrl* ctrl, S32 x, S32 y);
    LLUUID selectedAnimId() const;      // selected row's anim asset id
    LLUUID pasteAnimId() const;         // validated paste-row UUID (null = invalid)
    void onAnimCopyUUID();
    void onAnimSetLoco();               // row -> selected member's loco anim
    void onAnimPlayLocal(bool play);    // start/stopMotion, client-side only
    void onPastePlayLocal(bool play);
    void onPasteSetLoco();
    void onClickClearLoco();
    // The shared preview panel is fed the anim to show + the object playing it:
    LLUUID previewAnimId() const;                       // selected row, else pasted uuid
    LLUUID animSourceObject(const LLUUID& anim_id) const; // the object playing it on you

    // ---- Camera tab ----
    static LLUUID avatarFromSelection();
    void onClickSetSubjectFromSelection(S32 subject);
    void onClickClearSubject(S32 subject);
    void refreshCameraTab();

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

    // scene files (transport bar)
    LLComboBox*   mSceneCombo = nullptr;
    LLLineEditor* mSceneNameEditor = nullptr;   // revealed in the combo's spot
    LLButton*     mSceneSaveBtn = nullptr;
    LLButton*     mSceneDeleteBtn = nullptr;
    ALPanelCineCamParams* mCineCamPanel = nullptr;  // embedded shared params panel

    // left-rail tabs + legend popover
    LLTabContainer* mTabContainer = nullptr;
    LLButton*       mHelpBtn = nullptr;
    LLPanel*        mLegendPanel = nullptr;

    // cast column
    LLScrollListCtrl* mCastList = nullptr;
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
        bool        mInWorld = true;
    };
    std::vector<CastRowState> mRowStates;

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

    // Animate tab
    LLTextBox*        mAnimateHeader = nullptr;
    LLScrollListCtrl* mAnimList = nullptr;
    LLTextBox*        mAnimHint = nullptr;
    LLButton*         mAnimCopyBtn = nullptr;
    LLButton*         mAnimSetLocoBtn = nullptr;
    LLButton*         mAnimPlayBtn = nullptr;
    LLButton*         mAnimStopBtn = nullptr;
    LLLineEditor*     mPasteEditor = nullptr;
    LLButton*         mPastePlayBtn = nullptr;
    LLButton*         mPasteStopBtn = nullptr;
    LLButton*         mPasteSetLocoBtn = nullptr;
    LLTextBox*        mLocoText = nullptr;
    LLButton*         mClearLocoBtn = nullptr;
    LLHandle<LLContextMenu> mAnimMenuHandle;
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
};

#endif // LL_LLFLOATERDIRECTOR_H
