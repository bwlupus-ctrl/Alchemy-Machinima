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

    // change-diffing snapshot so the list only rebuilds when the geometry/
    // summary of a node actually changed
    struct NodeSnap { LLVector3d mPos; F32 mDwell = 0.f; F32 mSpeed = 0.f; };
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
};

#endif // AL_ALPANELPATHEDITOR_H
