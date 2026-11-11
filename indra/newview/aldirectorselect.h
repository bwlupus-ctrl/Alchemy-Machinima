/**
 * @file aldirectorselect.h
 * @brief One helper that makes a world avatar or clone the Director's
 *        selection. Shared by the right-click "Select in Director" items and
 *        the Actor bar "Select" pick mode (altooldirectorpick.cpp).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Avatars (self, others, animesh) route to the existing Cast add path and the
 * Actor bar setter (LLDirectorCast::add / setActiveActorId). Clones route to
 * the Clones page selection when that page is current, else to the Actor bar.
 * Nothing here is saved in scenes; clones stay session-only. The decisions
 * live in alghostclonemenulogic.h so a script can test them without a viewer.
 */

#ifndef AL_ALDIRECTORSELECT_H
#define AL_ALDIRECTORSELECT_H

#include "lluuid.h"
#include "v3math.h"

#include <string>
#include <vector>

class LLPickInfo;
class LLVOAvatar;

namespace ALDirectorSelect
{
// Clones page key in the v2 Director.
extern const char* const CLONES_PAGE;

// The visible entity clone a world pick means: a hit on the clone's body or
// one of its attachments, else the screen-space foot / mid-body pick when that
// clone is nearer the camera than whatever the real pick hit. With no entity
// clone there, a free-standing overlay clone is picked by the same probe and
// nearer-than rule, unless the real hit is that overlay's own source avatar.
// Null otherwise.
LLUUID cloneAtPick(const LLPickInfo& pick);

// The selectable real avatar (or animesh / attachment wearer) under a pick,
// or null. Clones are never returned here (see cloneAtPick).
LLVOAvatar* avatarAtPick(const LLPickInfo& pick);

// Empty when the target can be selected; otherwise the reason (a strings.xml
// text) a menu item shows as its tooltip while disabled.
std::string avatarRefusal(const LLUUID& avatar_id);
std::string cloneRefusal(const LLUUID& instance_id);

// Make the avatar the Actor bar actor, adding it to the Cast through the
// existing add path first when it is not a member. Ctrl is ignored: avatars
// stay single. False (with a toast) when refused.
bool selectAvatar(const LLUUID& avatar_id, bool additive);

// Select a Studio clone (entity clone or overlay instance). Entity clone: the
// Clones page selection when that page is current, otherwise the Actor bar
// actor. additive (Ctrl) adds to the clone multi-selection instead.
bool selectClone(const LLUUID& instance_id, bool additive);

// Open (or raise) the v2 Director on the Clones page with the clone selected.
bool openCloneInDirector(const LLUUID& instance_id);

// The Director is open and visible.
bool directorVisible();

// A clone that is already part of the clone multi-selection (directly or through a selected group header)
// has its selected row promoted to the primary row (the one the detail panel, the tags and the shared
// selection follow). Every selected row stays selected; nothing is toggled, added, raised or undone.
void makePrimaryInSelection(const LLUUID& instance_id);

// The foreground rule for a world click on a clone: the clone (its click depth point, the out_probe_agent
// of ALGhostStudio::pickInstanceAtScreen) may claim the click only when it is nearer the camera than what the
// real pick hit; a HUD hit or a HUD icon always keeps the click.
bool cloneInFrontOfPick(const LLPickInfo& pick, const LLVector3& clone_probe_agent);

// Clones list ROW values (instance ids and group header ids) selected in the
// host the user last worked in while it is on screen; empty when there is none.
std::vector<LLUUID> hostRows();
}

#endif // AL_ALDIRECTORSELECT_H
