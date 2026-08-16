/**
 * @file alpanelpatheditor.cpp
 * @brief Shared Actor Pathing editor panel -- see alpanelpatheditor.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelpatheditor.h"

#include "altoolpathedit.h"
#include "llactormover.h"
#include "llagent.h"                 // gAgent camera origin <-> global (Set camera here)
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldirectorcast.h"          // names + Subject A for "face target" (UI reads engine)
#include "llflycamrecorder.h"        // sync-to-take status (take loaded? duration)
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llpathcamera.h"            // static Preview of a node's stored camera
#include "llscrolllistctrl.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltoolmgr.h"
#include "llviewercamera.h"          // capture the live render camera pose

// both the Director Console Move tab and the standalone Actor Mover embed this
// via <panel class="panel_path_editor" filename="panel_path_editor.xml"/>
static LLPanelInjector<ALPanelPathEditor> t_panel_path_editor("panel_path_editor");

namespace
{
// End-behavior combo values (encode mEndMode + arrival facing in one control)
constexpr S32 END_STOP       = 0;   // mEndMode 0, arrival facing none
constexpr S32 END_STOP_FACE  = 1;   // mEndMode 0, arrival facing = Subject A
constexpr S32 END_LOOP       = 2;   // mEndMode 1
constexpr S32 END_PINGPONG   = 3;   // mEndMode 2
} // anonymous namespace

ALPanelPathEditor::ALPanelPathEditor()
{
}

ALPanelPathEditor::~ALPanelPathEditor()
{
    // never leave the shared tool armed or a camera preview latched if the
    // panel is torn down
    exitEditMode();
    LLPathCamera::instance().stopPreview();
}

bool ALPanelPathEditor::postBuild()
{
    mHeader        = getChild<LLTextBox>("path_header");
    mEditModeCheck = getChild<LLCheckBoxCtrl>("edit_mode_check");
    mList          = getChild<LLScrollListCtrl>("waypoint_list");
    mAddBtn        = getChild<LLButton>("btn_wp_add");
    mInsertBtn     = getChild<LLButton>("btn_wp_insert");
    mDeleteBtn     = getChild<LLButton>("btn_wp_delete");
    mClearBtn      = getChild<LLButton>("btn_wp_clear");

    mReadout       = getChild<LLTextBox>("path_readout");
    mUndoBtn       = getChild<LLButton>("btn_edit_undo");
    mRedoBtn       = getChild<LLButton>("btn_edit_redo");
    mReverseBtn    = getChild<LLButton>("btn_edit_reverse");
    mMirrorBtn     = getChild<LLButton>("btn_edit_mirror");
    mLoopCloseBtn  = getChild<LLButton>("btn_edit_loopclose");
    mWalkHereBtn   = getChild<LLButton>("btn_walk_here");
    mCopyToCombo   = getChild<LLComboBox>("copy_to_combo");
    mCopyBtn       = getChild<LLButton>("btn_copy_to");
    mNodeHeight    = getChild<LLSpinCtrl>("node_height_spinner");
    mNodeDwell     = getChild<LLSpinCtrl>("node_dwell_spinner");
    mNodeSpeed     = getChild<LLSpinCtrl>("node_speed_spinner");
    mNodeAnim      = getChild<LLLineEditor>("node_anim_editor");
    mPathSpeed     = getChild<LLSpinCtrl>("path_speed_spinner");
    mTension       = getChild<LLSliderCtrl>("path_tension_slider");
    mEaseIn        = getChild<LLSpinCtrl>("path_easein_spinner");
    mEaseOut       = getChild<LLSpinCtrl>("path_easeout_spinner");
    mGroundFollow  = getChild<LLCheckBoxCtrl>("path_groundfollow_check");
    mPitch         = getChild<LLCheckBoxCtrl>("path_pitch_check");
    mEndCombo      = getChild<LLComboBox>("path_end_combo");
    mColorSwatch   = getChild<LLPanel>("color_swatch");

    mSetCamBtn     = getChild<LLButton>("btn_node_setcam");
    mClearCamBtn   = getChild<LLButton>("btn_node_clearcam");
    mPreviewBtn    = getChild<LLButton>("btn_node_preview");
    mCamTransCombo = getChild<LLComboBox>("node_cam_transition_combo");
    mCamStatus     = getChild<LLTextBox>("node_cam_status");

    mSyncCheck     = getChild<LLCheckBoxCtrl>("sync_take_check");
    mSyncOffset    = getChild<LLSpinCtrl>("sync_offset_spinner");
    mSyncStatus    = getChild<LLTextBox>("sync_status");
    mFollowCombo   = getChild<LLComboBox>("follow_leader_combo");
    mStopFollowBtn = getChild<LLButton>("btn_stop_following");
    mFollowOffset  = getChild<LLSpinCtrl>("follow_offset_spinner");
    mFollowStatus  = getChild<LLTextBox>("follow_status");

    mHint             = getChild<LLTextBox>("hint");
    mSuspendBanner    = getChild<LLPanel>("suspend_banner");
    mSuspendStatus    = getChild<LLTextBox>("suspend_status");
    mResumeBtn        = getChild<LLButton>("btn_suspend_resume");
    mReanchorBtn      = getChild<LLButton>("btn_suspend_reanchor");
    mCancelSuspendBtn = getChild<LLButton>("btn_suspend_cancel");

    mEditModeCheck->setCommitCallback([this](LLUICtrl*, const LLSD&) { onToggleEditMode(); });
    mList->setCommitCallback([this](LLUICtrl*, const LLSD&) { onListSelect(); });
    mAddBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAdd(); });
    mInsertBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickInsert(); });
    mDeleteBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickDelete(); });
    mClearBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClear(); });

    mUndoBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickUndo(); });
    mRedoBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickRedo(); });
    mReverseBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickReverse(); });
    mMirrorBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickMirror(); });
    mLoopCloseBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickLoopClose(); });
    mWalkHereBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickWalkHere(); });
    mCopyBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickCopyTo(); });

    mNodeHeight->setCommitCallback([this](LLUICtrl*, const LLSD&) { onNodeHeightCommit(); });
    mNodeDwell->setCommitCallback([this](LLUICtrl*, const LLSD&) { onNodeDwellCommit(); });
    mNodeSpeed->setCommitCallback([this](LLUICtrl*, const LLSD&) { onNodeSpeedCommit(); });
    mNodeAnim->setCommitCallback([this](LLUICtrl*, const LLSD&) { onNodeAnimCommit(); });

    mPathSpeed->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPathSpeedCommit(); });
    mTension->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPathTensionCommit(); });
    mEaseIn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPathEaseInCommit(); });
    mEaseOut->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPathEaseOutCommit(); });
    mGroundFollow->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPathGroundFollowCommit(); });
    mPitch->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPathPitchCommit(); });
    mEndCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPathEndCommit(); });

    mSetCamBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetCam(); });
    mClearCamBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClearCam(); });
    mPreviewBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickPreview(); });
    mCamTransCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCamTransitionCommit(); });

    mResumeBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickResume(); });
    mReanchorBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickReanchor(); });
    mCancelSuspendBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickCancelSuspend(); });

    mSyncCheck->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSyncToggle(); });
    mSyncOffset->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSyncOffsetCommit(); });
    mFollowCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onFollowCommit(); });
    mStopFollowBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onStopFollow(); });
    mFollowOffset->setCommitCallback([this](LLUICtrl*, const LLSD&) { onFollowOffsetCommit(); });

    if (mColorSwatch)
    {
        mColorSwatch->setBackgroundVisible(true);
        mColorSwatch->setBackgroundOpaque(true);    // show the solid per-actor color
    }
    return true;
}

// ---------------------------------------------------------------------------
void ALPanelPathEditor::setTargetActor(const LLUUID& actor_id)
{
    if (actor_id == mActor)
    {
        return;
    }
    mActor = actor_id;
    // force a full list + selection re-sync on the next draw
    mSnap.clear();
    mLastEngineNode = -2;
}

bool ALPanelPathEditor::targetHasWalkablePath() const
{
    return mActor.notNull() && LLActorMover::instance().hasWalkablePath(mActor);
}

//static
std::string ALPanelPathEditor::actorName(const LLUUID& id)
{
    if (id.isNull())
    {
        return std::string();
    }
    if (const LLDirectorCast::CastMember* m = LLDirectorCast::instance().getMember(id))
    {
        if (!m->mLastName.empty())
        {
            return m->mLastName;
        }
    }
    if (LLAvatarName av_name; LLAvatarNameCache::get(id, &av_name))
    {
        return av_name.getCompleteName();
    }
    return id.asString().substr(0, 8);
}

// ---------------------------------------------------------------------------
void ALPanelPathEditor::draw()
{
    // while visible, this panel owns the shared edit-selection actor: it drives
    // the in-world node highlight and the edit tool's target
    LLActorMover::instance().setEditActor(mActor);

    // if our transient tool was taken away (build tools, another picker, focus
    // loss), reflect that in the toggle instead of showing a stale "on"
    if (mEditMode && !LLToolMgr::getInstance()->usingTransientTool())
    {
        mEditMode = false;
        mEditModeCheck->set(false);
    }

    refreshHeader();
    refreshList();
    refreshInspector();
    refreshPathControls();
    refreshCameraControls();
    refreshSuspendBanner();
    refreshReadout();
    refreshEditButtons();
    refreshCopyCombo();
    refreshChoreography();

    LLPanel::draw();
}

void ALPanelPathEditor::onVisibilityChange(bool new_visibility)
{
    if (!new_visibility)
    {
        // leaving the tab / hiding the panel must never strand the user in the
        // tool or in a camera preview, and must drop the in-world highlight
        exitEditMode();
        LLPathCamera::instance().stopPreview();
        LLActorMover::instance().setEditActor(LLUUID::null);
    }
    LLPanel::onVisibilityChange(new_visibility);
}

// ---------------------------------------------------------------------------
void ALPanelPathEditor::refreshHeader()
{
    const std::string name = actorName(mActor);
    const std::string want = mActor.isNull()
        ? std::string("Path \xE2\x80\x94 select a cast member")
        : ("Path \xE2\x80\x94 " + (name.empty() ? std::string("actor") : name));
    if (mHeader->getValue().asString() != want)
    {
        mHeader->setText(want);
    }
    mEditModeCheck->setEnabled(mActor.notNull());
}

void ALPanelPathEditor::refreshList()
{
    const LLActorMover::Path* path = LLActorMover::instance().getPath(mActor);
    const S32 n = path ? (S32)path->mNodes.size() : 0;
    const S32 endmode = path ? path->mEndMode : 0;

    // diff against the last snapshot: rebuild only when a node's geometry /
    // dwell / speed changed, or the end mode flipped (roles: a loop has no
    // distinct End, so every role label can shift)
    bool changed = ((S32)mSnap.size() != n) || (endmode != mSnapEndMode);
    for (S32 i = 0; !changed && i < n; ++i)
    {
        if (mSnap[i].mPos != path->mNodes[i].mPosGlobal ||
            mSnap[i].mDwell != path->mNodes[i].mDwell ||
            mSnap[i].mSpeed != path->mNodes[i].mSpeedOverride ||
            mSnap[i].mHasCam != path->mNodes[i].mHasCam)
        {
            changed = true;
        }
    }

    if (changed)
    {
        mSnapEndMode = endmode;
        mSnap.clear();
        mList->deleteAllItems();
        const bool loop = (endmode == 1);
        for (S32 i = 0; i < n; ++i)
        {
            const LLActorMover::Waypoint& w = path->mNodes[i];
            const std::string role = (i == 0)
                ? std::string("Start")
                : (((i == n - 1) && !loop) ? std::string("End") : std::string("Node"));
            std::string detail;
            if (w.mDwell > 0.f)
            {
                detail = llformat("dwell %.1fs", w.mDwell);
            }
            if (w.mSpeedOverride > 0.f)
            {
                if (!detail.empty()) { detail += "  "; }
                detail += llformat("%.2f m/s", w.mSpeedOverride);
            }
            if (w.mHasCam)
            {
                if (!detail.empty()) { detail += "  "; }
                // a camera icon glyph so a shot node reads at a glance in the list
                detail += (w.mCamTransition == 0) ? "\xF0\x9F\x93\xB7 cut"
                                                  : "\xF0\x9F\x93\xB7 ease";
            }

            LLSD row;
            row["value"] = i;
            row["columns"][0]["column"] = "num";
            row["columns"][0]["value"]  = llformat("%d", i + 1);
            row["columns"][1]["column"] = "role";
            row["columns"][1]["value"]  = role;
            row["columns"][2]["column"] = "detail";
            row["columns"][2]["value"]  = detail;
            mList->addElement(row, ADD_BOTTOM);
            mSnap.push_back({ w.mPosGlobal, w.mDwell, w.mSpeedOverride, w.mHasCam });
        }
        mLastEngineNode = -2;       // force a selection re-sync below
    }

    syncListSelectionFromEngine();
}

void ALPanelPathEditor::syncListSelectionFromEngine()
{
    const S32 en = LLActorMover::instance().getEditNode();
    if (en == mLastEngineNode)
    {
        return;         // nothing changed since we last mirrored it
    }
    mLastEngineNode = en;
    if (en >= 0 && en < mList->getItemCount())
    {
        mList->selectNthItem(en);
        mList->scrollToShowSelected();
    }
    else
    {
        mList->deselectAllItems(true);
    }
}

S32 ALPanelPathEditor::listSelectedNode() const
{
    LLScrollListItem* item = mList->getFirstSelected();
    return item ? item->getValue().asInteger() : -1;
}

// ---------------------------------------------------------------------------
void ALPanelPathEditor::refreshInspector()
{
    const LLActorMover::Path* path = LLActorMover::instance().getPath(mActor);
    const S32 sel = listSelectedNode();
    const bool have = path && sel >= 0 && sel < (S32)path->mNodes.size();

    mNodeHeight->setEnabled(have);
    mNodeDwell->setEnabled(have);
    mNodeSpeed->setEnabled(have);
    mNodeAnim->setEnabled(have);

    if (!have)
    {
        return;
    }
    const LLActorMover::Waypoint& w = path->mNodes[sel];

    // focus-guarded sets so we never fight the user mid-type
    if (!mNodeHeight->hasFocus() &&
        fabsf((F32)mNodeHeight->getValue().asReal() - w.mGroundOffset) > 0.001f)
    {
        mNodeHeight->setValue(w.mGroundOffset);
    }
    if (!mNodeDwell->hasFocus() &&
        fabsf((F32)mNodeDwell->getValue().asReal() - w.mDwell) > 0.001f)
    {
        mNodeDwell->setValue(w.mDwell);
    }
    if (!mNodeSpeed->hasFocus() &&
        fabsf((F32)mNodeSpeed->getValue().asReal() - w.mSpeedOverride) > 0.001f)
    {
        mNodeSpeed->setValue(w.mSpeedOverride);
    }
    if (!mNodeAnim->hasFocus())
    {
        const std::string s = w.mAnim.isNull() ? std::string() : w.mAnim.asString();
        if (mNodeAnim->getText() != s)
        {
            mNodeAnim->setText(s);
        }
    }
}

void ALPanelPathEditor::refreshPathControls()
{
    const LLActorMover::Path* path = LLActorMover::instance().getPath(mActor);
    const bool have_actor = mActor.notNull();

    mPathSpeed->setEnabled(have_actor);
    mTension->setEnabled(have_actor);
    mEaseIn->setEnabled(have_actor);
    mEaseOut->setEnabled(have_actor);
    mGroundFollow->setEnabled(have_actor);
    mPitch->setEnabled(have_actor);
    mEndCombo->setEnabled(have_actor);

    // engine defaults when no path yet (a Path struct's own defaults)
    const F32  speed  = path ? path->mSpeed        : 1.f;
    const F32  tens   = path ? path->mTension      : 0.5f;
    const F32  ein    = path ? path->mEaseIn       : 0.f;
    const F32  eout   = path ? path->mEaseOut      : 0.f;
    const bool gfoll  = path ? path->mGroundFollow : false;
    const bool pit    = path ? path->mPitchToSlope : false;
    const S32  emode  = path ? path->mEndMode      : 0;
    const S32  aface  = path ? path->mArrivalFacingMode : 0;

    if (!mPathSpeed->hasFocus() &&
        fabsf((F32)mPathSpeed->getValue().asReal() - speed) > 0.001f)
    {
        mPathSpeed->setValue(speed);
    }
    if (!mTension->hasFocus() &&
        fabsf((F32)mTension->getValue().asReal() - tens) > 0.001f)
    {
        mTension->setValue(tens);
    }
    if (!mEaseIn->hasFocus() &&
        fabsf((F32)mEaseIn->getValue().asReal() - ein) > 0.001f)
    {
        mEaseIn->setValue(ein);
    }
    if (!mEaseOut->hasFocus() &&
        fabsf((F32)mEaseOut->getValue().asReal() - eout) > 0.001f)
    {
        mEaseOut->setValue(eout);
    }
    if ((mGroundFollow->getValue().asBoolean()) != gfoll)
    {
        mGroundFollow->set(gfoll);
    }
    if ((mPitch->getValue().asBoolean()) != pit)
    {
        mPitch->set(pit);
    }

    // combo <- (end mode + arrival facing)
    S32 combo = END_STOP;
    if (emode == 1)      { combo = END_LOOP; }
    else if (emode == 2) { combo = END_PINGPONG; }
    else                 { combo = (aface != 0) ? END_STOP_FACE : END_STOP; }
    if (mEndCombo->getValue().asInteger() != combo)
    {
        mEndCombo->setValue(combo);
    }

    // read-only per-actor color swatch
    if (mColorSwatch)
    {
        mColorSwatch->setBackgroundColor(have_actor
            ? LLActorMover::actorPathColor(mActor)
            : LLColor4(0.4f, 0.4f, 0.4f, 1.f));
    }

    // button + tooltip enable state
    const S32 n = path ? (S32)path->mNodes.size() : 0;
    const S32 sel = listSelectedNode();
    mAddBtn->setEnabled(have_actor);
    mInsertBtn->setEnabled(have_actor && sel >= 0);
    mDeleteBtn->setEnabled(have_actor && sel >= 0);
    mClearBtn->setEnabled(have_actor && n > 0);
    mInsertBtn->setToolTip(sel >= 0
        ? std::string("Insert a waypoint just after the selected one")
        : std::string("Select a waypoint first"));
    mDeleteBtn->setToolTip(sel >= 0
        ? std::string("Delete the selected waypoint")
        : std::string("Select a waypoint first"));
    mClearBtn->setToolTip(n > 0
        ? std::string("Remove every waypoint from this path")
        : std::string("This actor has no waypoints yet"));
}

// ---------------------------------------------------------------------------
// P3 per-node camera row: enable/tooltip + status + cut/ease reflect the
// selected node's stored camera; Preview reflects LLPathCamera's live state.
// ---------------------------------------------------------------------------
void ALPanelPathEditor::refreshCameraControls()
{
    const LLActorMover::Path* path = LLActorMover::instance().getPath(mActor);
    const S32  sel  = listSelectedNode();
    const bool have = path && sel >= 0 && sel < (S32)path->mNodes.size();
    const bool previewing = LLPathCamera::instance().isPreviewing();

    // read the selected node's camera. getNodeCamera fills the cut/ease
    // preference (ctrans) even when it returns false (no camera yet), so one
    // call covers both the "has camera" and "remembered preference" cases.
    LLVector3d cpos; LLQuaternion crot; F32 cfov = 0.f; S32 ctrans = 1;
    const bool has_cam = have &&
        LLActorMover::instance().getNodeCamera(mActor, sel, cpos, crot, cfov, ctrans);

    // Set is blocked while previewing (the render camera is the frozen preview
    // pose -- capturing it would be a no-op)
    mSetCamBtn->setEnabled(have && !previewing);
    mSetCamBtn->setToolTip(!have
        ? std::string("Select a waypoint first")
        : (previewing
            ? std::string("Exit preview first so you can aim the camera")
            : std::string("Capture the current camera position, aim and FOV into this node")));

    mClearCamBtn->setEnabled(has_cam && !previewing);
    mClearCamBtn->setToolTip(!have
        ? std::string("Select a waypoint first")
        : (has_cam ? std::string("Remove the camera from this node")
                   : std::string("This node has no camera yet")));

    mCamTransCombo->setEnabled(has_cam);
    mCamTransCombo->setToolTip(has_cam
        ? std::string("Cut: snap to this shot when the actor reaches the node. Ease: glide from the previous camera node")
        : std::string("Set a camera on this node first"));
    if (has_cam && !mCamTransCombo->hasFocus() &&
        mCamTransCombo->getValue().asInteger() != ctrans)
    {
        mCamTransCombo->setValue(ctrans);
    }

    // Preview toggles: enabled when the node has a camera, or always available
    // to exit while previewing
    mPreviewBtn->setEnabled(has_cam || previewing);
    mPreviewBtn->setLabel(previewing ? std::string("Exit preview")
                                     : std::string("Preview"));
    mPreviewBtn->setToolTip(previewing
        ? std::string("Return the camera to normal control")
        : (has_cam ? std::string("Jump the camera to this node's shot to check the framing")
                   : std::string("Set a camera on this node first")));

    if (previewing)
    {
        mCamStatus->setText(std::string("Previewing \xE2\x80\x94 camera is held on this shot"));
    }
    else if (has_cam)
    {
        const S32 fov_deg = (S32)llround(cfov * RAD_TO_DEG);
        mCamStatus->setText(llformat("Camera set \xE2\x80\x94 %s \xC2\xB7 FOV %d\xC2\xB0",
                                     (ctrans == 0) ? "cut" : "ease", fov_deg));
    }
    else if (have)
    {
        mCamStatus->setText(std::string("No camera on this node"));
    }
    else
    {
        mCamStatus->setText(std::string("Select a waypoint to author its camera"));
    }
}

// ---------------------------------------------------------------------------
// TP-away suspend banner: shown only while the selected actor's walk is
// suspended (actor derezzed / left the region / teleported). Resume is offered
// when the actor is resolvable; Re-anchor (path walks) translates the whole
// path to the actor's current position; Cancel drops the walk. The engine's
// idle state machine auto-resumes a near return, so the "moved" case is what
// surfaces here.
// ---------------------------------------------------------------------------
void ALPanelPathEditor::refreshSuspendBanner()
{
    LLActorMover& m = LLActorMover::instance();
    const bool suspended = mActor.notNull() && m.isWalkSuspended(mActor);

    if (suspended != mSuspendShown)
    {
        mSuspendShown = suspended;
        mSuspendBanner->setVisible(suspended);
        // the in-world placement hint shares the banner's band; hide it while
        // the banner is up so they never overlap
        if (mHint)
        {
            mHint->setVisible(!suspended);
        }
    }
    if (!suspended)
    {
        return;
    }

    const bool resolvable = m.suspendedActorResolvable(mActor);
    const bool is_near     = m.suspendedActorNearAnchor(mActor);
    const bool has_path    = m.hasWalkablePath(mActor);

    mResumeBtn->setEnabled(resolvable);
    mResumeBtn->setToolTip(resolvable
        ? std::string("Resume the walk from where it left off")
        : std::string("The actor isn't here yet \xE2\x80\x94 waiting for it to return"));

    mReanchorBtn->setEnabled(resolvable && has_path);
    mReanchorBtn->setToolTip(!resolvable
        ? std::string("The actor isn't here yet \xE2\x80\x94 waiting for it to return")
        : (has_path
            ? std::string("Move the whole path to the actor's current position and resume from here (keep shooting on this sim)")
            : std::string("Re-anchor applies to a waypoint path")));

    mCancelSuspendBtn->setEnabled(true);     // always able to drop the walk

    std::string status;
    if (!resolvable)
    {
        status = "Walk suspended \xE2\x80\x94 actor away (held for return)";
    }
    else if (is_near)
    {
        status = "Actor is back \xE2\x80\x94 resuming\xE2\x80\xA6";
    }
    else
    {
        status = "Walk suspended \xE2\x80\x94 actor moved. Resume, re-anchor, or cancel";
    }
    if (mSuspendStatus->getValue().asString() != status)
    {
        mSuspendStatus->setText(status);
    }
}

void ALPanelPathEditor::onClickResume()
{
    if (mActor.notNull())
    {
        LLActorMover::instance().resumeWalk(mActor);
    }
}

void ALPanelPathEditor::onClickReanchor()
{
    if (mActor.notNull())
    {
        LLActorMover::instance().reanchorWalk(mActor);
    }
}

void ALPanelPathEditor::onClickCancelSuspend()
{
    if (mActor.notNull())
    {
        LLActorMover::instance().cancelSuspended(mActor);
    }
}

// ---------------------------------------------------------------------------
// list + buttons
// ---------------------------------------------------------------------------
void ALPanelPathEditor::onListSelect()
{
    const S32 n = listSelectedNode();
    LLActorMover::instance().setEditNode(n);
    mLastEngineNode = n;        // we set it; don't let the sync fight us
    refreshInspector();
}

void ALPanelPathEditor::onClickAdd()
{
    if (mActor.isNull())
    {
        return;
    }
    LLActorMover& m = LLActorMover::instance();
    m.snapshotForUndo(mActor);
    m.appendWaypointHere(mActor);       // ground-snaps at the actor's feet
    if (const LLActorMover::Path* p = m.getPath(mActor); p && !p->mNodes.empty())
    {
        m.setEditNode((S32)p->mNodes.size() - 1);
    }
}

void ALPanelPathEditor::onClickInsert()
{
    LLActorMover& m = LLActorMover::instance();
    const LLActorMover::Path* p = m.getPath(mActor);
    const S32 sel = listSelectedNode();
    if (!p || sel < 0)
    {
        return;
    }
    m.snapshotForUndo(mActor);
    const S32 n = (S32)p->mNodes.size();
    if (sel + 1 < n)
    {
        // between the selected node and the next: drop a bend at the midpoint
        const LLVector3d mid =
            (p->mNodes[sel].mPosGlobal + p->mNodes[sel + 1].mPosGlobal) * 0.5;
        const S32 at = m.insertWaypoint(mActor, sel + 1, mid);
        m.setEditNode(at);
    }
    else
    {
        // selected is the last node: appending at the actor's feet puts a fresh
        // node right after it (the common "extend the path" case)
        m.appendWaypointHere(mActor);
        if (const LLActorMover::Path* np = m.getPath(mActor); np && !np->mNodes.empty())
        {
            m.setEditNode((S32)np->mNodes.size() - 1);
        }
    }
}

void ALPanelPathEditor::onClickDelete()
{
    const S32 sel = listSelectedNode();
    if (mActor.isNull() || sel < 0)
    {
        return;
    }
    LLActorMover& m = LLActorMover::instance();
    m.snapshotForUndo(mActor);
    if (m.deleteWaypoint(mActor, sel))
    {
        const LLActorMover::Path* p = m.getPath(mActor);
        const S32 cnt = p ? (S32)p->mNodes.size() : 0;
        m.setEditNode(cnt > 0 ? llclamp(sel, 0, cnt - 1) : -1);
    }
}

void ALPanelPathEditor::onClickClear()
{
    const LLActorMover::Path* p = LLActorMover::instance().getPath(mActor);
    const S32 n = p ? (S32)p->mNodes.size() : 0;
    if (mActor.isNull() || n <= 0)
    {
        return;     // confirm only when there is something to clear
    }
    LLSD args;
    args["MESSAGE"] = llformat("Remove all %d waypoint%s from this path?",
                               n, (n == 1 ? "" : "s"));
    LLHandle<ALPanelPathEditor> handle = getDerivedHandle<ALPanelPathEditor>();
    LLNotificationsUtil::add("GenericAlertYesCancel", args, LLSD(),
        [handle](const LLSD& notification, const LLSD& response)
        {
            if (ALPanelPathEditor* self = handle.get())
            {
                self->clearCallback(notification, response);
            }
        });
}

bool ALPanelPathEditor::clearCallback(const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;       // Cancel
    }
    LLActorMover& m = LLActorMover::instance();
    m.snapshotForUndo(mActor);
    m.clearPath(mActor);
    m.setEditNode(-1);
    return false;
}

// ---------------------------------------------------------------------------
// edit-mode toggle (the in-world tool)
// ---------------------------------------------------------------------------
void ALPanelPathEditor::onToggleEditMode()
{
    const bool want = mEditModeCheck->get();
    LLToolMgr* tm = LLToolMgr::getInstance();
    if (want)
    {
        if (mActor.isNull())
        {
            mEditModeCheck->set(false);     // nothing to edit; bounce it off
            mEditMode = false;
            return;
        }
        // edit mode places nodes by picking against the render camera; a held
        // preview would freeze that camera, so the two are mutually exclusive
        LLPathCamera::instance().stopPreview();
        LLActorMover::instance().setEditActor(mActor);
        tm->setTransientTool(ALToolPathEdit::getInstance());
        mEditMode = true;
    }
    else
    {
        exitEditMode();
    }
}

void ALPanelPathEditor::exitEditMode()
{
    if (mEditMode)
    {
        mEditMode = false;
        LLToolMgr* tm = LLToolMgr::getInstance();
        if (tm->usingTransientTool())
        {
            tm->clearTransientTool();       // restores the prior tool (camera control)
        }
    }
    if (mEditModeCheck)
    {
        mEditModeCheck->set(false);
    }
}

// ---------------------------------------------------------------------------
// P3 QOL edit ops. Structural ops snapshot undo then delegate; each is a no-op
// (guarded here and in the engine) when there is no walkable path or a walk is
// active, matching the button enable/tooltip state below.
// ---------------------------------------------------------------------------
void ALPanelPathEditor::onClickUndo()
{
    if (mActor.notNull())
    {
        LLActorMover::instance().undoPath(mActor);
    }
}

void ALPanelPathEditor::onClickRedo()
{
    if (mActor.notNull())
    {
        LLActorMover::instance().redoPath(mActor);
    }
}

void ALPanelPathEditor::onClickReverse()
{
    LLActorMover& m = LLActorMover::instance();
    if (mActor.isNull() || m.isPathWalking(mActor) || !m.hasWalkablePath(mActor))
    {
        return;
    }
    m.snapshotForUndo(mActor);
    m.reversePath(mActor);
}

void ALPanelPathEditor::onClickMirror()
{
    LLActorMover& m = LLActorMover::instance();
    if (mActor.isNull() || m.isPathWalking(mActor) || !m.hasWalkablePath(mActor))
    {
        return;
    }
    m.snapshotForUndo(mActor);
    m.mirrorPath(mActor);
}

void ALPanelPathEditor::onClickLoopClose()
{
    LLActorMover& m = LLActorMover::instance();
    if (mActor.isNull() || m.isPathWalking(mActor) || !m.hasWalkablePath(mActor))
    {
        return;
    }
    m.snapshotForUndo(mActor);
    m.loopClosePath(mActor);
}

void ALPanelPathEditor::onClickCopyTo()
{
    LLActorMover& m = LLActorMover::instance();
    if (mActor.isNull() || !m.hasWalkablePath(mActor) || !mCopyToCombo)
    {
        return;
    }
    const LLUUID dst(mCopyToCombo->getSelectedValue().asString());
    if (dst.isNull() || dst == mActor || m.isPathWalking(dst))
    {
        return;
    }
    m.snapshotForUndo(dst);     // the destination's overwrite is undoable on that actor
    m.copyPathTo(mActor, dst);
}

void ALPanelPathEditor::onClickWalkHere()
{
    if (mActor.isNull())
    {
        return;
    }
    // arming picks against the render camera, so hand back any held preview and
    // drop edit mode; the tool disarms + returns the camera after the click
    LLPathCamera::instance().stopPreview();
    exitEditMode();
    LLActorMover::instance().setEditActor(mActor);
    ALToolPathEdit* tool = ALToolPathEdit::getInstance();
    tool->armWalkTo();
    LLToolMgr::getInstance()->setTransientTool(tool);
}

// ---------------------------------------------------------------------------
// P3 QOL refreshers: readout, edit-op enable/tooltips, copy picker
// ---------------------------------------------------------------------------
void ALPanelPathEditor::refreshReadout()
{
    if (!mReadout)
    {
        return;
    }
    LLActorMover& m = LLActorMover::instance();
    const LLActorMover::Path* path = m.getPath(mActor);
    const S32 nodes = path ? (S32)path->mNodes.size() : 0;

    std::string txt;
    F32 len = 0.f, dur = 0.f;
    if (m.getPathStats(mActor, len, dur))
    {
        const S32 emode = path ? path->mEndMode : 0;
        const char* suffix = (emode == 1) ? " / lap"
                           : (emode == 2) ? " each way" : "";
        // middot U+00B7, em-dash U+2014 as raw UTF-8
        txt = llformat("%.1f m \xC2\xB7 ~%.1f s%s \xC2\xB7 %d nodes",
                       len, dur, suffix, nodes);
    }
    else if (nodes == 1)
    {
        txt = "1 node \xE2\x80\x94 add another to make a path";
    }
    else
    {
        txt = "No path yet";
    }
    if (mReadout->getValue().asString() != txt)
    {
        mReadout->setText(txt);
    }
}

void ALPanelPathEditor::refreshEditButtons()
{
    LLActorMover& m = LLActorMover::instance();
    const bool have_actor = mActor.notNull();
    const bool walking  = have_actor && m.isPathWalking(mActor);
    const bool walkable = have_actor && m.hasWalkablePath(mActor);
    const bool can_undo = have_actor && m.canUndoPath(mActor);
    const bool can_redo = have_actor && m.canRedoPath(mActor);

    mUndoBtn->setEnabled(can_undo && !walking);
    mUndoBtn->setToolTip(walking
        ? std::string("Stop the walk to undo path edits")
        : (can_undo ? std::string("Undo the last path edit")
                    : std::string("Nothing to undo")));
    mRedoBtn->setEnabled(can_redo && !walking);
    mRedoBtn->setToolTip(walking
        ? std::string("Stop the walk to redo path edits")
        : (can_redo ? std::string("Redo the last undone edit")
                    : std::string("Nothing to redo")));

    // one shared reason for the structural ops (walk active / no walkable path)
    const std::string reason = walking
        ? std::string("Stop the walk to edit the path")
        : (!walkable ? std::string("Needs a path with at least two waypoints")
                     : std::string());
    const bool ops_ok = walkable && !walking;

    mReverseBtn->setEnabled(ops_ok);
    mReverseBtn->setToolTip(reason.empty()
        ? std::string("Reverse the path direction (per-node timing and cameras stay attached)")
        : reason);
    mMirrorBtn->setEnabled(ops_ok);
    mMirrorBtn->setToolTip(reason.empty()
        ? std::string("Mirror the path left-to-right across its line of travel (positions and cameras)")
        : reason);
    mLoopCloseBtn->setEnabled(ops_ok);
    mLoopCloseBtn->setToolTip(reason.empty()
        ? std::string("Snap the last waypoint onto the first and set the path to loop")
        : reason);

    mWalkHereBtn->setEnabled(have_actor);
    mWalkHereBtn->setToolTip(have_actor
        ? std::string("Arm one ground click: the actor gets a straight path to that spot and walks it")
        : std::string("Select a cast member first"));
}

void ALPanelPathEditor::refreshCopyCombo()
{
    if (!mCopyToCombo || !mCopyBtn)
    {
        return;
    }
    LLActorMover& m = LLActorMover::instance();
    const uuid_vec_t& ids = LLDirectorCast::instance().getIds();

    // rebuild the picker only when the cast membership or target changes
    std::string sig = mActor.asString();
    for (const LLUUID& id : ids)
    {
        sig += id.asString();
    }
    if (sig != mCopySig)
    {
        mCopySig = sig;
        mCopyToCombo->removeall();
        for (const LLUUID& id : ids)
        {
            if (id != mActor)
            {
                mCopyToCombo->add(actorName(id), LLSD(id.asString()));
            }
        }
        if (mCopyToCombo->getItemCount() > 0)
        {
            mCopyToCombo->selectFirstItem();
        }
    }

    const bool walkable = mActor.notNull() && m.hasWalkablePath(mActor);
    const bool have_dst = mCopyToCombo->getItemCount() > 0;
    mCopyToCombo->setEnabled(have_dst);

    LLUUID dst;
    if (have_dst)
    {
        dst = LLUUID(mCopyToCombo->getSelectedValue().asString());
    }
    const bool dst_walking = dst.notNull() && m.isPathWalking(dst);
    mCopyBtn->setEnabled(walkable && have_dst && !dst_walking);
    mCopyBtn->setToolTip(!walkable
        ? std::string("This actor has no path to copy")
        : (!have_dst ? std::string("Add another cast member to copy the path to")
        : (dst_walking ? std::string("That actor is walking \xE2\x80\x94 stop it first")
                       : std::string("Copy this path onto the chosen cast member (overlays at the same spot)"))));
}

// ---------------------------------------------------------------------------
// P3 choreography: sync-to-take + follow-the-leader. Both reflect engine state
// and are reason-tooltipped; the combo excludes self and cycle-forming leaders.
// ---------------------------------------------------------------------------
void ALPanelPathEditor::refreshChoreography()
{
    LLActorMover& m = LLActorMover::instance();
    const bool have_actor = mActor.notNull();
    const LLActorMover::Path* path = m.getPath(mActor);

    // ---- Sync to camera take ----
    const bool sync_on = path ? path->mSyncToTake : false;
    mSyncCheck->setEnabled(have_actor);
    mSyncCheck->setToolTip(have_actor
        ? std::string("Drive this path from the Flycam Recorder playhead: play or scrub the take and the actor moves in lockstep (deterministic actor+lens takes). No take loaded = no change to the walk.")
        : std::string("Select a cast member first"));
    if (mSyncCheck->getValue().asBoolean() != sync_on)
    {
        mSyncCheck->set(sync_on);
    }
    mSyncOffset->setEnabled(have_actor && sync_on);
    mSyncOffset->setToolTip(std::string("Lead (+) or trail (-) the lens by this many seconds along the take"));
    const F32 lead = path ? path->mSyncLeadTrail : 0.f;
    if (!mSyncOffset->hasFocus() &&
        fabsf((F32)mSyncOffset->getValue().asReal() - lead) > 0.001f)
    {
        mSyncOffset->setValue(lead);
    }

    // status reflects whether a take is actually loaded to drive the walk
    LLFlycamRecorder& rec = LLFlycamRecorder::instance();
    const bool take_loaded = rec.getNumKeyframes() > 0 && rec.getDuration() > 0.001f;
    std::string sstat;
    if (!have_actor)
    {
        sstat = "Select a cast member";
    }
    else if (!sync_on)
    {
        sstat = "Off \xE2\x80\x94 the walk uses its own clock";
    }
    else if (!take_loaded)
    {
        sstat = "Sync on \xE2\x80\x94 record or load a take to drive the walk";
    }
    else
    {
        sstat = llformat("Driven by recorder playhead (%.1fs take)", rec.getDuration());
    }
    if (mSyncStatus->getValue().asString() != sstat)
    {
        mSyncStatus->setText(sstat);
    }

    // ---- Follow the leader ----
    // rebuild the leader picker only when the cast, the target, or the set of
    // cycle-forming candidates changes. "(not following)" first, then every cast
    // member that is not self and would not loop the procession.
    const uuid_vec_t& ids = LLDirectorCast::instance().getIds();
    std::string sig = "F" + mActor.asString();
    for (const LLUUID& id : ids)
    {
        if (id != mActor && !m.wouldFollowCycle(mActor, id))
        {
            sig += id.asString();
        }
    }
    if (sig != mFollowSig)
    {
        mFollowSig = sig;
        mFollowCombo->removeall();
        mFollowCombo->add("(not following)", LLSD(std::string()));
        for (const LLUUID& id : ids)
        {
            if (id != mActor && !m.wouldFollowCycle(mActor, id))
            {
                mFollowCombo->add(actorName(id), LLSD(id.asString()));
            }
        }
    }

    LLUUID leader; S32 mode = 0; F32 offset = 3.f;
    const bool following = have_actor && m.getFollow(mActor, leader, mode, offset);

    if (!mFollowCombo->hasFocus())
    {
        const std::string want = following ? leader.asString() : std::string();
        if (mFollowCombo->getSelectedValue().asString() != want)
        {
            if (!mFollowCombo->setSelectedByValue(LLSD(want), true))
            {
                mFollowCombo->selectFirstItem();    // leader unpickable -> (none)
            }
        }
    }
    mFollowCombo->setEnabled(have_actor && mFollowCombo->getItemCount() > 1);

    mFollowOffset->setEnabled(following);
    mFollowOffset->setToolTip(std::string("Metres the follower stays behind the leader along the path"));
    if (following && !mFollowOffset->hasFocus() &&
        fabsf((F32)mFollowOffset->getValue().asReal() - offset) > 0.001f)
    {
        mFollowOffset->setValue(offset);
    }

    mStopFollowBtn->setEnabled(following);
    mStopFollowBtn->setToolTip(following
        ? std::string("Stop following; this actor keeps its own path (if any)")
        : std::string("This actor isn't following anyone"));

    std::string fstat;
    if (!have_actor)
    {
        fstat = "Select a cast member";
    }
    else if (following)
    {
        fstat = llformat("Following %s \xE2\x80\x94 %.1f m behind",
                         actorName(leader).c_str(), offset);
    }
    else if (mFollowCombo->getItemCount() <= 1)
    {
        fstat = "Add another cast member to lead";
    }
    else
    {
        fstat = "Not following";
    }
    if (mFollowStatus->getValue().asString() != fstat)
    {
        mFollowStatus->setText(fstat);
    }
}

void ALPanelPathEditor::onSyncToggle()
{
    if (mActor.isNull())
    {
        return;
    }
    // sync is a path-wide authored flag (like ground-follow); it does not change
    // the geometry, so no arc-length rebuild
    LLActorMover::instance().editPath(mActor).mSyncToTake = mSyncCheck->get();
}

void ALPanelPathEditor::onSyncOffsetCommit()
{
    if (mActor.isNull())
    {
        return;
    }
    LLActorMover::instance().editPath(mActor).mSyncLeadTrail =
        (F32)mSyncOffset->getValue().asReal();
}

void ALPanelPathEditor::onFollowCommit()
{
    if (mActor.isNull())
    {
        return;
    }
    LLActorMover& m = LLActorMover::instance();
    const std::string val = mFollowCombo->getSelectedValue().asString();
    if (val.empty())
    {
        m.clearFollow(mActor);      // "(not following)"
        return;
    }
    // distance mode (0) only; time offset is deferred. setFollow refuses cycles.
    m.setFollow(mActor, LLUUID(val), 0, (F32)mFollowOffset->getValue().asReal());
}

void ALPanelPathEditor::onFollowOffsetCommit()
{
    if (mActor.isNull())
    {
        return;
    }
    LLActorMover& m = LLActorMover::instance();
    LLUUID leader; S32 mode = 0; F32 offset = 0.f;
    if (m.getFollow(mActor, leader, mode, offset))
    {
        m.setFollow(mActor, leader, mode, (F32)mFollowOffset->getValue().asReal());
    }
}

void ALPanelPathEditor::onStopFollow()
{
    if (mActor.notNull())
    {
        LLActorMover::instance().clearFollow(mActor);
    }
}

// ---------------------------------------------------------------------------
// P3 per-node camera authoring
// ---------------------------------------------------------------------------
void ALPanelPathEditor::onClickSetCam()
{
    const S32 sel = listSelectedNode();
    if (mActor.isNull() || sel < 0 || LLPathCamera::instance().isPreviewing())
    {
        return;
    }
    // capture the CURRENT render camera pose: origin -> global (studio cameras
    // are world-anchored, like the flycam recorder), full orientation, and the
    // vertical FOV. Reuse the node's remembered cut/ease if it already had a
    // camera, else take the combo's current selection.
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    const LLVector3d pos_global = gAgent.getPosGlobalFromAgent(cam->getOrigin());
    const LLQuaternion rot      = cam->getQuaternion();
    const F32 fov               = cam->getView();

    LLVector3d p; LLQuaternion r; F32 f = 0.f; S32 trans = 1;
    if (!LLActorMover::instance().getNodeCamera(mActor, sel, p, r, f, trans))
    {
        trans = mCamTransCombo ? mCamTransCombo->getValue().asInteger() : 1;
    }
    LLActorMover::instance().setNodeCamera(mActor, sel, pos_global, rot, fov,
                                           llclamp(trans, 0, 1));
}

void ALPanelPathEditor::onClickClearCam()
{
    const S32 sel = listSelectedNode();
    if (mActor.isNull() || sel < 0)
    {
        return;
    }
    // if we were previewing this shot, hand the camera back before removing it
    LLPathCamera::instance().stopPreview();
    LLActorMover::instance().clearNodeCamera(mActor, sel);
}

void ALPanelPathEditor::onCamTransitionCommit()
{
    const S32 sel = listSelectedNode();
    if (mActor.notNull() && sel >= 0)
    {
        LLActorMover::instance().setNodeCamTransition(mActor, sel,
            mCamTransCombo->getValue().asInteger());
    }
}

void ALPanelPathEditor::onClickPreview()
{
    // toggle: if already previewing, hand the camera back
    if (LLPathCamera::instance().isPreviewing())
    {
        LLPathCamera::instance().stopPreview();
        return;
    }
    const S32 sel = listSelectedNode();
    if (mActor.isNull() || sel < 0)
    {
        return;
    }
    LLVector3d pos; LLQuaternion rot; F32 fov = 0.f; S32 trans = 1;
    if (!LLActorMover::instance().getNodeCamera(mActor, sel, pos, rot, fov, trans))
    {
        return;         // nothing to preview
    }
    // previewing and the in-world edit tool both want the camera; drop edit mode
    exitEditMode();
    LLPathCamera::instance().startPreview(pos, rot, fov);
}

// ---------------------------------------------------------------------------
// per-node inspector commits
// ---------------------------------------------------------------------------
void ALPanelPathEditor::onNodeHeightCommit()
{
    const S32 sel = listSelectedNode();
    if (mActor.notNull() && sel >= 0)
    {
        LLActorMover::instance().snapshotForUndo(mActor);
        LLActorMover::instance().setNodeGroundOffset(mActor, sel, (F32)mNodeHeight->getValue().asReal());
    }
}

void ALPanelPathEditor::onNodeDwellCommit()
{
    const S32 sel = listSelectedNode();
    if (mActor.notNull() && sel >= 0)
    {
        LLActorMover::instance().snapshotForUndo(mActor);
        LLActorMover::instance().setNodeDwell(mActor, sel, (F32)mNodeDwell->getValue().asReal());
    }
}

void ALPanelPathEditor::onNodeSpeedCommit()
{
    const S32 sel = listSelectedNode();
    if (mActor.notNull() && sel >= 0)
    {
        LLActorMover::instance().snapshotForUndo(mActor);
        LLActorMover::instance().setNodeSpeed(mActor, sel, (F32)mNodeSpeed->getValue().asReal());
    }
}

void ALPanelPathEditor::onNodeAnimCommit()
{
    const S32 sel = listSelectedNode();
    if (mActor.isNull() || sel < 0)
    {
        return;
    }
    std::string s = mNodeAnim->getText();
    LLStringUtil::trim(s);
    LLUUID id;
    if (s.empty())
    {
        id.setNull();
    }
    else if (!LLUUID::parseUUID(s, &id))
    {
        return;         // not a valid UUID: leave the node anim unchanged
    }
    LLActorMover::instance().snapshotForUndo(mActor);
    LLActorMover::instance().setNodeAnim(mActor, sel, id);
}

// ---------------------------------------------------------------------------
// path-wide commits (drive the Path struct directly)
// ---------------------------------------------------------------------------
void ALPanelPathEditor::onPathSpeedCommit()
{
    if (mActor.isNull()) { return; }
    LLActorMover::Path& p = LLActorMover::instance().editPath(mActor);
    p.mSpeed = llmax(0.05f, (F32)mPathSpeed->getValue().asReal());
    p.markDirty();
}

void ALPanelPathEditor::onPathTensionCommit()
{
    if (mActor.isNull()) { return; }
    LLActorMover::Path& p = LLActorMover::instance().editPath(mActor);
    p.mTension = llclamp((F32)mTension->getValue().asReal(), 0.f, 1.f);
    p.markDirty();      // geometry shape changed -> arc-length rebuild
}

void ALPanelPathEditor::onPathEaseInCommit()
{
    if (mActor.isNull()) { return; }
    LLActorMover::Path& p = LLActorMover::instance().editPath(mActor);
    p.mEaseIn = llmax(0.f, (F32)mEaseIn->getValue().asReal());
    p.markDirty();
}

void ALPanelPathEditor::onPathEaseOutCommit()
{
    if (mActor.isNull()) { return; }
    LLActorMover::Path& p = LLActorMover::instance().editPath(mActor);
    p.mEaseOut = llmax(0.f, (F32)mEaseOut->getValue().asReal());
    p.markDirty();
}

void ALPanelPathEditor::onPathGroundFollowCommit()
{
    if (mActor.isNull()) { return; }
    LLActorMover::Path& p = LLActorMover::instance().editPath(mActor);
    p.mGroundFollow = mGroundFollow->get();
    p.markDirty();
}

void ALPanelPathEditor::onPathPitchCommit()
{
    if (mActor.isNull()) { return; }
    LLActorMover::Path& p = LLActorMover::instance().editPath(mActor);
    p.mPitchToSlope = mPitch->get();
    p.markDirty();
}

void ALPanelPathEditor::onPathEndCommit()
{
    if (mActor.isNull()) { return; }
    LLActorMover::Path& p = LLActorMover::instance().editPath(mActor);
    switch (mEndCombo->getValue().asInteger())
    {
        case END_LOOP:
            p.mEndMode = 1;
            p.mArrivalFacingMode = 0;
            break;
        case END_PINGPONG:
            p.mEndMode = 2;
            p.mArrivalFacingMode = 0;
            break;
        case END_STOP_FACE:
        {
            p.mEndMode = 0;
            // face Subject A on arrival; if no Subject A is set, degrade to a
            // plain stop (facing the travel tangent) rather than a fixed bearing
            const LLUUID subj = LLDirectorCast::instance().getSubjectA();
            p.mArrivalTarget = subj;
            p.mArrivalFacingMode = subj.notNull() ? 2 : 0;
            break;
        }
        case END_STOP:
        default:
            p.mEndMode = 0;
            p.mArrivalFacingMode = 0;
            break;
    }
    p.markDirty();
}
