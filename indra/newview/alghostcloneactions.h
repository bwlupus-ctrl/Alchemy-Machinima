/**
 * @file alghostcloneactions.h
 * @brief The Clones page actions that act on a set of list rows, shared by the
 *        page's buttons and the in-world clone / overlay menus so both take
 *        exactly the same path (unit planning, group rules, setters, limits).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * A "row" is a Clones list row value: an instance id, or a group header id.
 * The page reads its rows from the list; the menus carry the rows that were
 * selected (or the picked unit) when they opened. Nothing here touches a
 * widget. The page handlers keep only their own widget work (rebuilding the
 * list, restoring the selection) around these calls.
 */

#ifndef AL_ALGHOSTCLONEACTIONS_H
#define AL_ALGHOSTCLONEACTIONS_H

#include "alghoststudio.h"
#include "lluuid.h"

#include <string>
#include <utility>
#include <vector>

namespace ALGhostCloneActions
{
// Instances the rows stand for: a selected group header expands to its
// members; duplicates and instances being deleted are dropped (page
// selectedInstances()).
std::vector<LLUUID> instances(const std::vector<LLUUID>& rows);

// Authoring units the rows stand for: a collapsed group (or its header) is one
// unit, an expanded member is its own; a selected header swallows its selected
// children (page selectedUnitIds()).
std::vector<LLUUID> units(const std::vector<LLUUID>& rows);

// The row a clone has in the list: its collapsed group's header, else itself.
LLUUID rowFor(const LLUUID& instance_id);

// Scale spinner commit: a group goes through transformGroup (limits checked,
// never clamped), anything else through transformUnit (page applyUnitTransform()).
bool applyUnitTransform(const LLUUID& unit_id, const LLVector3d& foot,
                        const LLQuaternion& rotation, F32 scale);

// Pause / Resume buttons.
void setPaused(const std::vector<LLUUID>& rows, bool paused);

// Drive-mode combo (entity clones only; a group header reaches every member).
void setDriveMode(const std::vector<LLUUID>& rows, ALGhostStudio::EDriveMode mode,
                  const LLUUID& directed_anim);

// Style combo (overlay looks).
void setOverlayStyle(const std::vector<LLUUID>& rows, S32 style);

// "Grab pose now" for overlay clones. True when any row failed to freeze.
bool freezeOverlays(const std::vector<LLUUID>& rows);

// Delete button. True when anything was removed; skipped_members is set when an
// expanded group member was passed over (never an implicit whole-crowd delete).
bool deleteRows(const std::vector<LLUUID>& rows, bool* skipped_members = nullptr);

struct DuplicateResult
{
    std::vector<LLUUID> mRows;     // rows of the copies, in order
    LLUUID mFirst;                 // shared selection for the first copy
};
// Duplicate (in_place = false, overlays and group headers) and Duplicate in
// place (in_place = true, every clone) buttons.
DuplicateResult duplicateRows(const std::vector<LLUUID>& rows, bool in_place);

// Solo button: toggles solo for these rows' instances.
void toggleSolo(const std::vector<LLUUID>& rows);

// The collapsed (solid) group an instance belongs to, else null. An expanded
// group's members and ungrouped clones have none.
const ALGhostGroupModel::Group* collapsedGroup(const LLUUID& instance_id);

// Animation drop on a clone: every live entity clone of its collapsed group,
// else only the clone itself (page unit planner: instances({rowFor(hit)})).
struct AnimationDropPlan
{
    std::vector<LLUUID> mTargets;
    bool mCollapsedGroup = false;
    std::string mGroupName;
};
AnimationDropPlan planAnimationDrop(const LLUUID& hit_instance);

// Sync button: zero the start offsets (and the Start offset mode when the whole
// crowd is included), then restart each animation. Body of the page handler.
void syncRows(const std::vector<LLUUID>& rows);

// The Crowd tab's Start offset math (ALGhostStudio::applyCrowdVariation, which
// stays untouched: pinned, and its fixtures extract it) over an EXPLICIT member
// list: mode 0 Off / 1 Random / 2 Ripple, the current Spread and Ripple from.
// The same expressions, in the same order; director_clone_menu_check.py compares
// the two text for text. No setting is written, no selection is touched, nothing
// outside `ids` changes. False (changing nothing) when `ids` is empty or holds a
// missing / deleting instance.
bool applyStartOffsets(const std::vector<LLUUID>& ids, S32 mode, U32 seed);

// Stagger (the clone menu's Start offset for a COLLAPSED group). It uses the
// Crowd tab's math (ALGhostStudio::applyStartOffsets over the group's live
// entity members, current Spread and Ripple from) and never writes the global
// Start offset setting, never changes the selection, never touches a clone
// outside the group. 0 In sync (Off + the Sync path's animation restart),
// 1 Random, 2 Ripple.
struct StaggerState
{
    std::vector<std::pair<LLUUID, F32>> mOffsets;     // each live member's start offset
    LLUUID mGroup;                                    // session-only group state key
    S32 mMode = -1;                                   // mode last applied to THIS group (-1 unknown)
};
// Why Stagger cannot act on this group (a strings.xml key), or null when it can:
// not a collapsed group, no live entity member, or members not all in one crowd.
const char* staggerRefusal(const LLUUID& instance_id);
bool staggerAvailable(const LLUUID& instance_id);
// Check mark for the group: 0 In sync (every offset zero), else the mode last
// applied to this group (1 / 2), else -1 (no check).
S32 staggerCurrent(const LLUUID& instance_id);
StaggerState snapshotStagger(const LLUUID& instance_id);
bool setStagger(const LLUUID& instance_id, S32 mode);
// Undo / redo: each surviving saved member gets its start offset back (and its
// animation restarted so the phase matches). False, changing nothing, when no
// saved member survives. Never touches the selection or any setting.
bool restoreStagger(const StaggerState& state);

// ---- Look / turn (the page's "Look at" combo with Keep facing + Face now, and the
// /ghostturn chat command: ALGhostStudio::setLookTarget / setTurnTarget) ----------
// `target` is LOOK_TARGET_CAMERA, LOOK_TARGET_ME or LOOK_TARGET_ACTOR. ACTOR takes any
// avatar's agent id: ALGhostStudio::resolveTargetGlobal resolves it through
// LLDirectorCast::resolve, which finds any avatar in gObjectList by id (a Director cast
// member is not required). When the target leaves, the clone holds its last facing.
// Look and turn act on a whole authoring unit: a collapsed group, a group header row or an ungrouped
// clone. A single member of an EXPANDED group cannot be addressed alone (the Studio's look / turn paths
// address the whole group), so any target set that holds one is REFUSED: nothing is written.
// Per-member look / turn inside an expanded group is a separate feature.
bool hasExpandedMember(const std::vector<LLUUID>& rows);
// Look at: first Turn off on every live instance of the rows (stopTurning: an armed Turn
// owns body yaw and would hold the clone, so the look would not show), then the unit's look
// target + Keep facing, then face it now (the page's path). A collapsed group is one unit
// (its first live member stands for it, as on the page). True when any unit was set (nothing, not
// even Turn, is changed when none was, or when the rows hold an expanded group member).
bool lookAt(const std::vector<LLUUID>& rows, ALGhostStudio::ELookTarget target, const LLUUID& target_id);
// Keep facing off for those units (the look target is kept, as the page does).
void stopLooking(const std::vector<LLUUID>& rows);
// Turn to face once (TURN_MODE_ONCE: rotate to the target, then hold) on every live
// instance of the rows (the /ghostturn path). True when any instance was set.
bool turnTo(const std::vector<LLUUID>& rows, ALGhostStudio::ELookTarget target, const LLUUID& target_id);
// Turn off (/ghostturn off).
void stopTurning(const std::vector<LLUUID>& rows);

// ---- Animation ---------------------------------------------------------------
// Restart the animation of every entity clone of the rows (restartInstanceAnimation,
// the call the Sync path makes after zeroing offsets). True when any restarted.
bool restartRows(const std::vector<LLUUID>& rows);

// Inventory animations played on clones this session, newest first, one per
// animation, capped. Session-only memory: never saved, never logged. The asset id is
// kept only so the menu can play the entry again (the drop path does the same); the
// name is the only thing ever shown, and is empty when it would have carried the id.
struct RecentAnimation
{
    LLUUID mAsset;
    std::string mName;
};
// Called by the one funnel every play goes through (the drop, Play on selected clones,
// the Inventory combo, this menu): ALPanelGhostStudio::playAnimationOnClonesRecorded.
void noteRecentAnimation(const LLUUID& asset_id, const std::string& anim_name);
const std::vector<RecentAnimation>& recentAnimations();
// Forget an entry (its item is gone from the inventory).
void removeRecentAnimation(const LLUUID& asset_id);

// ---- Head gaze (the Clones page Motion > Head gaze controls and the clone menu Gaze submenu) --
// Entity ids (LLActorMover actors) of the live entity clones the rows stand for.
std::vector<LLUUID> gazeEntities(const std::vector<LLUUID>& rows);
// What the page Head gaze target combo / Gaze toggle write: the target MODE (LLActorMover
// GAZE_*; negative leaves it as it is), the cast target when `cast_target` is not null (GAZE_CAST), and the enabled flag.
// Order and calls are the page's onLensGazeToggle / onClickLensGazeSelection bodies.
void applyGazeTarget(const std::vector<LLUUID>& entity_ids, S32 mode, const LLUUID& cast_target, bool enabled);
// Undo / redo support for the menu: each row's gaze before / after.
struct GazeBefore
{
    LLUUID mId;             // the clone instance (its entity id may change on a respawn)
    bool mEnabled = false;
    S32 mMode = 0;
    LLUUID mCast;
};
std::vector<GazeBefore> snapshotGaze(const std::vector<LLUUID>& rows);
// Rewrite a target clone's retired id in saved snapshots (new_id null = deleted: the mode falls
// back to travel direction, like the mover's own rewrite).
void rewriteGazeReferences(std::vector<GazeBefore>& state, const LLUUID& old_id, const LLUUID& new_id);
// False (changing nothing) when no saved clone survives.
bool restoreGaze(const std::vector<GazeBefore>& state);

// ---- Animation speed (the Clones page Anim speed spinner and the clone menu Speed submenu) ----
// The spinner's commit body: the speed (clamped 0.05..4) on every entity clone of the rows
// (ALGhostStudio::setInstanceAnimSpeed).
void setAnimSpeed(const std::vector<LLUUID>& rows, F32 speed);
// Faster (direction > 0) / Slower (< 0): every entity target moves to the next rung of
// ALGhostCloneMenuLogic::steppedAnimSpeed relative to its OWN current speed.
void stepAnimSpeed(const std::vector<LLUUID>& rows, int direction);
struct SpeedBefore
{
    LLUUID mId;
    F32 mSpeed = 1.f;
};
std::vector<SpeedBefore> snapshotAnimSpeed(const std::vector<LLUUID>& rows);
// False (changing nothing) when no saved clone survives.
bool restoreAnimSpeed(const std::vector<SpeedBefore>& state);

// ---- Appearance --------------------------------------------------------------
// Swap source button: swap every entity / overlay clone of the rows to `source` (null =
// You). Returns the status text the page shows (empty on success). Body of the page handler.
std::string swapSource(const std::vector<LLUUID>& rows, const LLUUID& source, const std::string& label);
// Render look combo (entity clones only). True when any clone took it.
bool setEntityLook(const std::vector<LLUUID>& rows, ALGhostStudio::EGhostLook look);

// Fade out / Fade in: hide an existing ENTITY clone (or show it again) with the Crowd tab's Fade
// out / Fade in duration, through ALGhostStudio::fadeInstance (the updateFades ramp). Hidden-only:
// the clone is never deleted, stays hidden (mUserHidden) until Fade in or the session ends, and
// nothing is saved. Overlays are not faded.
struct FadeBefore
{
    LLUUID mId;
    bool mHidden = false;   // the RESTING state before the action (undo restores it, never a mid-ramp alpha)
};
// Live entity instances of the rows a fade applies to: Fade out takes those not user-hidden, Fade in
// those that are (ALGhostCloneMenuLogic::fadeEligible).
std::vector<LLUUID> fadeTargets(const std::vector<LLUUID>& rows, bool fade_out);
// Fades the eligible instances; returns what each was before (empty = nothing to do).
std::vector<FadeBefore> fadeRows(const std::vector<LLUUID>& rows, bool fade_out);
// Undo (jump back to the saved resting state) / redo (the same fade again). False when no saved
// clone survives.
bool restoreFade(const std::vector<FadeBefore>& before, bool redo, bool fade_out);
// ---- Flip pose left/right (the Clones page "Flip pose L/R" row and the clone menu's Animation > Flip left/right) --
// ONE writer for both: only entity clones the page row is enabled for take it (Mirror, True mirror or Directed, or
// Frozen from those: ALGhostAnimMirrorMath::directedMirrorActive); every other clone of the rows is left alone.
// Session-only state (ALGhostStudio::Instance::mDirectedMirror through setInstanceDirectedMirror).
bool directedMirrorEligible(const LLUUID& instance_id);
struct FlipBefore
{
    LLUUID mId;             // the clone instance (stable across a respawn of its entity)
    bool mOn = false;
};
// Eligible clones of the rows with their current flip, in order.
std::vector<FlipBefore> snapshotDirectedMirror(const std::vector<LLUUID>& rows);
// Writes `enabled` on every eligible clone of the rows. True when any value changed.
bool setDirectedMirror(const std::vector<LLUUID>& rows, bool enabled);
// History preflight: true when at least one saved clone is replayable NOW (the same directedMirrorEligible
// predicate restoreDirectedMirror applies), so a step that replay could not apply is refused before any write.
bool directedMirrorReplayable(const std::vector<FlipBefore>& state);
// Undo / redo: each saved clone that is still eligible gets its flip back (a partial step applies to the replayable
// clones, like restoreGaze). False (changing nothing) when none is replayable.
bool restoreDirectedMirror(const std::vector<FlipBefore>& state);
}

#endif // AL_ALGHOSTCLONEACTIONS_H
