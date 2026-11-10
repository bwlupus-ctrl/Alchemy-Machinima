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
}

#endif // AL_ALGHOSTCLONEACTIONS_H
