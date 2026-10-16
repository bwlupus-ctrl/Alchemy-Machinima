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
#include <utility>
#include <unordered_set>

class LLButton;
class LLAccordionCtrl;
class LLAccordionCtrlTab;
class ALKitCard;
class ALKitPage;
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
    bool handleMouseDown(S32 x, S32 y, MASK mask) override; // [DirFix]

    // host -> panel: which cast/roster actor to edit (null = nothing selected,
    // which disables editing and clears the in-world highlight)
    void setTargetActor(const LLUUID& actor_id);
    const LLUUID& getTargetActor() const { return mActor; }
    // [DirFix] host EVENT paths only (floater open, tab change, list selection):
    // feed the current actor AND claim it for this host when visible (a null
    // actor releases this host's claim). Unlike setTargetActor, which is
    // draw-safe and ownership-neutral, this transfers ownership, so it must never
    // be reached from a draw() path.
    void revealFor(const LLUUID& actor_id);

    // host convenience: does the target already have a walkable path? (the Move
    // tab uses this to auto-pick Path mode) -- without reaching into the engine
    bool targetHasWalkablePath() const;

    // leave edit mode cleanly (host calls this on hide/close so the user is
    // never stranded in the tool with no camera control)
    void exitEditMode();

private:
    ALKitPage* mKitPage = nullptr;
    void claimEdit();        // [DirFix] claim/release owned singleton state
    bool ownsEdit() const;
    bool ownsTool() const;
    void installOwnerHooks();
    void requestDocumentLayout();
    void layoutDocument();

    void refreshHeader();
    void refreshList();
    void refreshInspector();
    void refreshPathControls();
    void refreshCameraControls();           // P3 per-node camera row state
    void refreshSuspendBanner();            // TP-away suspend/resume banner
    void refreshReadout();                  // P3 QOL: length + est. duration line
    void refreshEditButtons();              // P3 QOL: undo/redo/reverse/mirror/loop/walk enable
    void refreshCopyCombo();                // P3 QOL: cast picker for "copy path to"
    void refreshChoreography();             // P3 sync-to-take + follow-the-leader row
    void refreshResetButtons();             // mini reset enable state follows its control
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
    void onClickPlayPath();
    void onClickStopPath();
    void onClickWalkHere();     // arms a single ground click via ALToolPathEdit
    void onClickCenterPrimitive();

    // TP-away suspend banner actions (resume where it left off / translate the
    // path to the actor's current spot and resume / drop the suspended walk)
    void onClickResume();
    void onClickReanchor();
    void onClickCancelSuspend();

    // P3 choreography: sync the path to the recorder take, and follow another
    // cast member's path (procession). Each is a no-op / reason-tooltip when
    // unavailable; both compose with the existing walk + suspend/resume.
    void onSyncToggle();
    void onSyncOffsetCommit();
    void onFollowCommit();          // combo pick -> set/clear the follow relationship
    void onFollowOffsetCommit();
    void onStopFollow();

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
    void onNodeCornerRadiusCommit();
    void onNodeAnimCommit();

    // path-wide commits (drive the Path struct directly; defaults = engine defaults)
    void onPathSpeedCommit();
    void onPathCadenceCommit();
    void onPathTensionCommit();
    void onPathEaseInCommit();
    void onPathEaseOutCommit();
    void onPathGroundFollowCommit();
    void onPathPitchCommit();
    void onPathAirborneCommit();
    void onPathEndCommit();
    void onPathShapeCommit();
    void onPrimitiveCommit();
    void onClickFitHalfArc();
    void onClickFitSine();
    void onResetControl(const std::string& key);
    // Director kit page: the kit rows own the reset glyphs, so route each code-bound row
    // through a binding whose reset is onResetControl (same path as the v1 icon buttons).
    void registerKitResets();

    // cached name for the header
    static std::string actorName(const LLUUID& id);

    LLUUID mActor;              // current target (raw cast/roster id; null = none)
    LLUUID mEditOwner;          // [DirFix] unique owner token for this panel host
    std::unordered_set<const LLView*> mOwnerHooked; // [DirFix] idempotent focus hooks
    bool   mEditMode = false;   // our transient tool is armed
    bool   mSuspendShown = false;   // banner visibility last set (avoid churn)
    bool   mDocumentLayoutPending = false;

    // change-diffing snapshot so the list only rebuilds when the geometry/
    // summary of a node actually changed
    struct NodeSnap
    {
        LLVector3d mPos;
        F32 mDwell = 0.f;
        F32 mSpeed = 0.f;
        F32 mCornerRadius = 0.f;
        bool mHasCam = false;
    };
    std::vector<NodeSnap> mSnap;
    S32    mSnapEndMode = -999;    // end mode the current rows were built for
    S32    mLastEngineNode = -2;   // last edit-node we mirrored into the list

    // widgets
    LLAccordionCtrl*   mAccordion = nullptr;
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
    LLButton*          mPlayPathBtn = nullptr;
    LLButton*          mStopPathBtn = nullptr;
    LLComboBox*        mCopyToCombo = nullptr;
    LLButton*          mCopyBtn = nullptr;
    std::string        mCopySig;   // cast+target signature so the combo rebuilds only on change
    LLSpinCtrl*        mNodeHeight = nullptr;
    LLSpinCtrl*        mNodeDwell = nullptr;
    LLSpinCtrl*        mNodeSpeed = nullptr;
    LLSpinCtrl*        mNodeCornerRadius = nullptr;
    LLLineEditor*      mNodeAnim = nullptr;
    LLSpinCtrl*        mPathSpeed = nullptr;
    LLSliderCtrl*      mCadence = nullptr;
    LLSliderCtrl*      mTension = nullptr;
    LLSpinCtrl*        mEaseIn = nullptr;
    LLSpinCtrl*        mEaseOut = nullptr;
    LLCheckBoxCtrl*    mGroundFollow = nullptr;
    LLCheckBoxCtrl*    mPitch = nullptr;
    LLCheckBoxCtrl*    mAirborne = nullptr;
    LLComboBox*        mEndCombo = nullptr;
    LLPanel*           mColorSwatch = nullptr;
    LLComboBox*        mShapeCombo = nullptr;
    LLButton*          mCenterPrimitiveBtn = nullptr;
    LLButton*          mFitHalfArcBtn = nullptr;
    LLButton*          mFitSineBtn = nullptr;
    LLSpinCtrl*        mPrimitiveRadiusX = nullptr;
    LLSpinCtrl*        mPrimitiveRadiusY = nullptr;
    LLSpinCtrl*        mPrimitiveStart = nullptr;
    LLSpinCtrl*        mPrimitiveSweep = nullptr;
    LLSpinCtrl*        mPrimitiveYaw = nullptr;
    LLSpinCtrl*        mPrimitivePitch = nullptr;
    LLSpinCtrl*        mPrimitiveRoll = nullptr;
    LLSpinCtrl*        mPrimitiveRise = nullptr;

    // P3 per-node camera row
    LLButton*          mSetCamBtn = nullptr;
    LLButton*          mClearCamBtn = nullptr;
    LLButton*          mPreviewBtn = nullptr;
    LLComboBox*        mCamTransCombo = nullptr;
    LLTextBox*         mCamStatus = nullptr;

    // P3 choreography: sync-to-take + follow-the-leader
    LLCheckBoxCtrl*    mSyncCheck = nullptr;
    LLSpinCtrl*        mSyncOffset = nullptr;
    LLTextBox*         mSyncStatus = nullptr;
    LLComboBox*        mFollowCombo = nullptr;
    LLButton*          mStopFollowBtn = nullptr;
    LLSpinCtrl*        mFollowOffset = nullptr;
    LLTextBox*         mFollowStatus = nullptr;
    std::string        mFollowSig;   // cast+target+cycle signature (rebuild on change)

    // Icon-only reset buttons mirror their paired editor control. Pointers are
    // non-owning; XUI owns every child for the panel lifetime.
    std::vector<std::pair<LLButton*, LLUICtrl*>> mResetBindings;

    // in-world placement hint (hidden while the suspend banner is up)
    LLTextBox*         mHint = nullptr;

    // TP-away suspend/resume banner (shown only when the selected actor's walk
    // is suspended -- actor derezzed / left region / teleported)
    LLPanel*           mSuspendBanner = nullptr;   // v1 XML only
    ALKitCard*         mSuspendCard = nullptr;     // Director kit page: the "Paused walk" card
    LLTextBox*         mSuspendStatus = nullptr;
    LLButton*          mResumeBtn = nullptr;
    LLButton*          mReanchorBtn = nullptr;
    LLButton*          mCancelSuspendBtn = nullptr;
};

#endif // AL_ALPANELPATHEDITOR_H
