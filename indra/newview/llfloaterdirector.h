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
 * Actor Mover, Cinematic Camera (shared ALPanelCineCamParams instance),
 * Flycam Orbit and Flycam Recorder controls. Everything underneath is the
 * same settings/state the standalone floaters use -- no forks.
 */

#ifndef LL_LLFLOATERDIRECTOR_H
#define LL_LLFLOATERDIRECTOR_H

#include "llfloater.h"

#include <string>
#include <utility>
#include <vector>

class ALCompassDial;
class ALPanelCineCamParams;
class ALPanelPathEditor;
class LLButton;
class LLComboBox;
class LLContextMenu;
class LLLineEditor;
class LLPanel;
class LLRadioGroup;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLTextBox;

class LLFloaterDirector final : public LLFloater
{
public:
    LLFloaterDirector(const LLSD& key);
    ~LLFloaterDirector() override;
    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void draw() override;
    // Esc = CUT while the transport is running or counting down; stock
    // floater behavior otherwise
    bool handleKeyHere(KEY key, MASK mask) override;

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

    // ---- cast column ----
    void refreshCastList();
    void onCastRightClick(LLUICtrl* ctrl, S32 x, S32 y);
    void onClickAddYou();
    uuid_vec_t selectedCastIds() const;
    LLUUID     firstSelectedCastId() const;
    // context-menu / button ops (act on the list selection)
    void onCastSetSubject(bool subject_a);
    void onCastSetMarkHere();
    void onCastResetToMark();
    void onCastClearLocoAnim();
    void onCastCopyUUID();
    void onCastRemove();

    // ---- Move tab ----
    void onScopeCommit();
    void onDialCommit();
    void onClickWalk();
    void onClickStop();
    void refreshMoveTab();

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

    // ---- Camera tab ----
    static LLUUID avatarFromSelection();
    void onClickSetSubjectFromSelection(bool subject_a);
    void onClickClearSubject(bool subject_a);
    void refreshCameraTab();

    // ---- Takes tab ----
    void onTakeRecord();
    void onTakePlayPause();
    void onTakeStop();
    void onTakeScrub();
    void refreshTakesTab();

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

    // cast column
    LLScrollListCtrl* mCastList = nullptr;
    LLButton*         mRemoveBtn = nullptr;
    LLTextBox*        mCastHint = nullptr;
    LLHandle<LLContextMenu> mCastMenuHandle;
    // per-row last-applied cell state, parallel to the list rows, so cells
    // are only re-set when something actually changed (v2 mover idiom)
    struct CastRowState
    {
        std::string mIcon;
        std::string mName;
        std::string mAB;
        std::string mMark;
        bool        mInWorld = true;
    };
    std::vector<CastRowState> mRowStates;

    // Move tab
    LLRadioGroup*      mScopeRadio = nullptr;
    ALCompassDial*     mHeadingDial = nullptr;
    LLButton*          mWalkBtn = nullptr;
    LLButton*          mStopBtn = nullptr;

    // Path tab (dedicated) -- shared waypoint editor, targets the selection
    ALPanelPathEditor* mPathPanel = nullptr;

    // Camera tab
    LLTextBox* mSubjectAText = nullptr;
    LLTextBox* mSubjectBText = nullptr;
    LLButton*  mSetABtn = nullptr;
    LLButton*  mSetBBtn = nullptr;
    LLButton*  mClearABtn = nullptr;
    LLButton*  mClearBBtn = nullptr;

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

    // Takes tab
    LLButton*     mTakeRecordBtn = nullptr;
    LLButton*     mTakePlayBtn = nullptr;
    LLButton*     mTakeStopBtn = nullptr;
    LLSliderCtrl* mTakeScrub = nullptr;
    LLTextBox*    mTakeTimeText = nullptr;
    LLTextBox*    mTakeStatusText = nullptr;

    // status strip
    LLTextBox* mStatusStrip = nullptr;
};

#endif // LL_LLFLOATERDIRECTOR_H
