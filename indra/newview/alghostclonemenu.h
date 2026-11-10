/**
 * @file alghostclonemenu.h
 * @brief Right-click menus for Studio clones.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 *  - Entity clone menu (menu_ghost_clone.xml): opens on a right-click (press
 *    and release without a drag) on an entity clone, with the default pie tool
 *    or the clone edit tool. Every item calls the same ALGhostStudio action the
 *    Clones page uses, on the picked clone or, when it belongs to the Clones
 *    list multi-selection, on the whole selection.
 *  - Clone overlay submenu: lives in the four avatar menus (menu_avatar_other,
 *    menu_avatar_self, menu_attachment_other, menu_attachment_self) and is
 *    hidden unless the picked avatar carries a Studio overlay.
 *  - Select in Director items in the same menus.
 * Callbacks are registered with the XUI registries (registerCallbacks) before
 * the menus load. Session-only; nothing is saved in scenes.
 */

#ifndef AL_ALGHOSTCLONEMENU_H
#define AL_ALGHOSTCLONEMENU_H

#include "lluuid.h"
#include "stdtypes.h"

class LLContextMenu;
class LLPickInfo;
class LLVOAvatar;

namespace ALGhostCloneMenu
{
// Registers every enable / commit callback the menus name. Called from
// ALViewerMenu::initialize_menus(), before any menu XML is loaded.
void registerCallbacks();

// Loads menu_ghost_clone.xml into the menu holder. Called from init_menus().
void createMenu();
void destroyMenu();

// Right button went down at (x, y) with `pick` under it. True when an entity
// clone was hit: the caller must not open the world menu; the clone menu opens
// on release unless the pointer dragged.
bool pressRight(S32 x, S32 y, const LLPickInfo& pick);
void cancelRight();
// A right-click on a Clones list row (the panel hands over its row value and the click's screen
// point): the same menu, with the same targets (that row, or the whole list selection when the row is
// in it). This is how a hidden clone, which has no body to click, is reached. False when the row is not
// a live clone.
bool openForListRow(const LLUUID& row, S32 screen_x, S32 screen_y);
// Whether that row can open the menu at all; the panel asks BEFORE it changes the selection or the active host.
bool canOpenForListRow(const LLUUID& row);
void noteMouseMove(S32 x, S32 y);
void releaseRight(S32 x, S32 y);

// Call right before an avatar / attachment context menu is shown. Stores the
// picked avatar, shows the Clone overlay submenu only when that avatar has a
// Studio overlay, and hides it otherwise. `menu` may not be null.
void prepareAvatarMenu(LLContextMenu* menu, LLVOAvatar* avatar, bool hud_attachment = false);
}

#endif // AL_ALGHOSTCLONEMENU_H
