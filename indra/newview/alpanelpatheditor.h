/**
 * @file alpanelpatheditor.h
 * @brief Shared Actor Pathing editor panel (P2).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * A reusable LLPanel (class "panel_path_editor") that edits ONE actor's
 * LLActorMover::Path: a waypoint list, per-node inspector, path-wide controls,
 * and an "Edit mode" toggle that arms the in-world tool (ALToolPathEdit).
 * Embedded by BOTH the Director Console's Move tab and the standalone Actor
 * Mover floater, exactly like ALPanelCineCamParams. The host tells the panel
 * which actor to target each frame via setTargetActor(); the panel is otherwise
 * self-refreshing (draw()) and drives the engine through its P2 editing API and
 * the shared edit-selection model. LLDirectorCast stays UI-free -- the panel is
 * the UI and reads names/subjects from it.
 */

#ifndef AL_ALPANELPATHEDITOR_H
#define AL_ALPANELPATHEDITOR_H

#include "llpanel.h"

#include "lluuid.h"
#include "v3dmath.h"

#include <vector>

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLSpinCtrl;
class LLTextBox;

class ALPanelPathEditor final : public LLPanel
{
public:
    ALPanelPathEditor();
    ~ALPanelPathEditor() override;

    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool new_visibility) override;

    // host -> panel: which cast/roster actor to edit (null = nothing selected,
    // which disables editing and clears the in-world highlight)
    void setTargetActor(const LLUUID& actor_id);
    const LLUUID& getTargetActor() const { return mActor; }

    // host convenience: does the target already have a walkable path? (the Move
    // tab uses this to auto-pick Path mode) -- without reaching into the engine
    bool targetHasWalkablePath() const;

    // leave edit mode cleanly (host calls this on hide/close so the user is
    // never stranded in the tool with no camera control)
    void exitEditMode();

private:
    void refreshHeader();
    void refreshList();
    void refreshInspector();
    void refreshPathControls();
    void refreshCameraControls();           // P3 per-node camera row state
    void refreshSuspendBanner();            // TP-away suspend/resume banner
    void refreshReadout();                  // P3 QOL: length + est. duration line
    void refreshEditButtons();              // P3 QOL: undo/redo/reverse/mirror/loop/walk enable
    void refreshCopyCombo();                // P3 QOL: cast picker for "copy path to"
    void syncListSelectionFromEngine();     // engine edit node -> list row

    S32  listSelectedNode() const;          // selected row -> node index (-1 none)

    // waypoint list + buttons
    void onListSelect();
    void onClickAdd();
    void onClickInsert();
    void onClickDelete();
    void onClickClear();
    bool clearCallback(const LLSD& notification, const LLSD& response);

    // edit-mode toggle (the in-world tool)
    void onToggleEditMode();

    // P3 QOL edit ops (each is a no-op with a reason-tooltip when unsafe / empty)
    void onClickUndo();
    void onClickRedo();
    void onClickReverse();
    void onClickMirror();
    void onClickLoopClose();
    void onClickCopyTo();
    void onClickWalkHere();     // arms a single ground click via ALToolPathEdit

    // TP-away suspend banner actions (resume where it left off / translate the
    // path to the actor's current spot and resume / drop the suspended walk)
    void onClickResume();
    void onClickReanchor();
    void onClickCancelSuspend();

    // P3 per-node camera authoring: capture the live render camera into the
    // selected node, clear it, flip cut/ease, and static-preview its framing
    void onClickSetCam();
    void onClickClearCam();
    void onCamTransitionCommit();
    void onClickPreview();      // toggles LLPathCamera preview on the selected node

    // per-node inspector commits
    void onNodeHeightCommit();
    void onNodeDwellCommit();
    void onNodeSpeedCommit();
    void onNodeAnimCommit();

    // path-wide commits (drive the Path struct directly; defaults = engine defaults)
    void onPathSpeedCommit();
    void onPathTensionCommit();
    void onPathEaseInCommit();
    void onPathEaseOutCommit();
    void onPathGroundFollowCommit();
    void onPathPitchCommit();
    void onPathEndCommit();

    // cached name for the header
    static std::string actorName(const LLUUID& id);

    LLUUID mActor;              // current target (raw cast/roster id; null = none)
    bool   mEditMode = false;   // our transient tool is armed
    bool   mSuspendShown = false;   // banner visibility last set (avoid churn)

    // change-diffing snapshot so the list only rebuilds when the geometry/
    // summary of a node actually changed
    struct NodeSnap { LLVector3d mPos; F32 mDwell = 0.f; F32 mSpeed = 0.f; bool mHasCam = false; };
    std::vector<NodeSnap> mSnap;
    S32    mSnapEndMode = -999;    // end mode the current rows were built for
    S32    mLastEngineNode = -2;   // last edit-node we mirrored into the list

    // widgets
    LLTextBox*         mHeader = nullptr;
    LLCheckBoxCtrl*    mEditModeCheck = nullptr;
    LLScrollListCtrl*  mList = nullptr;
    LLButton*          mAddBtn = nullptr;
    LLButton*          mInsertBtn = nullptr;
    LLButton*          mDeleteBtn = nullptr;
    LLButton*          mClearBtn = nullptr;

    // P3 QOL: readout + edit-op row + copy picker
    LLTextBox*         mReadout = nullptr;
    LLButton*          mUndoBtn = nullptr;
    LLButton*          mRedoBtn = nullptr;
    LLButton*          mReverseBtn = nullptr;
    LLButton*          mMirrorBtn = nullptr;
    LLButton*          mLoopCloseBtn = nullptr;
    LLButton*          mWalkHereBtn = nullptr;
    LLComboBox*        mCopyToCombo = nullptr;
    LLButton*          mCopyBtn = nullptr;
    std::string        mCopySig;   // cast+target signature so the combo rebuilds only on change
    LLSpinCtrl*        mNodeHeight = nullptr;
    LLSpinCtrl*        mNodeDwell = nullptr;
    LLSpinCtrl*        mNodeSpeed = nullptr;
    LLLineEditor*      mNodeAnim = nullptr;
    LLSpinCtrl*        mPathSpeed = nullptr;
    LLSliderCtrl*      mTension = nullptr;
    LLSpinCtrl*        mEaseIn = nullptr;
    LLSpinCtrl*        mEaseOut = nullptr;
    LLCheckBoxCtrl*    mGroundFollow = nullptr;
    LLCheckBoxCtrl*    mPitch = nullptr;
    LLComboBox*        mEndCombo = nullptr;
    LLPanel*           mColorSwatch = nullptr;

    // P3 per-node camera row
    LLButton*          mSetCamBtn = nullptr;
    LLButton*          mClearCamBtn = nullptr;
    LLButton*          mPreviewBtn = nullptr;
    LLComboBox*        mCamTransCombo = nullptr;
    LLTextBox*         mCamStatus = nullptr;

    // in-world placement hint (hidden while the suspend banner is up)
    LLTextBox*         mHint = nullptr;

    // TP-away suspend/resume banner (shown only when the selected actor's walk
    // is suspended -- actor derezzed / left region / teleported)
    LLPanel*           mSuspendBanner = nullptr;
    LLTextBox*         mSuspendStatus = nullptr;
    LLButton*          mResumeBtn = nullptr;
    LLButton*          mReanchorBtn = nullptr;
    LLButton*          mCancelSuspendBtn = nullptr;
};

#endif // AL_ALPANELPATHEDITOR_H
