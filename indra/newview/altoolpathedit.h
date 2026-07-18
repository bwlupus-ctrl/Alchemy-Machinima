/**
 * @file altoolpathedit.h
 * @brief In-world edit tool-mode for Actor Pathing (P2).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * A lightweight transient LLTool the path-editor panel activates with its
 * "Edit mode" toggle. While active it edits the path of the actor named by
 * LLActorMover's edit-selection model:
 *
 *   - left-click a node marker  -> select it (+ begin a drag)
 *   - left-drag a node          -> moveWaypoint (cursor projected to the ground)
 *   - left-click on a segment   -> insert a node there (a new bend)
 *   - left-click empty ground   -> append a node at the click
 *   - right-click a node        -> node context menu (delete / insert after)
 *
 * The beacons are not real objects, so node picking is SCREEN-SPACE: every
 * node's world position is projected to the screen and the nearest within a
 * pixel threshold wins. Ground placement is a normal world pick (pickImmediate)
 * whose global hit point snaps to terrain/prim. Deactivating (unchecking, or
 * leaving the tab / closing the floater) restores the prior tool, so the user
 * is never stranded without camera control.
 */

#ifndef AL_ALTOOLPATHEDIT_H
#define AL_ALTOOLPATHEDIT_H

#include "lltool.h"
#include "llsingleton.h"
#include "lluuid.h"
#include "llhandle.h"

class LLContextMenu;

class ALToolPathEdit final : public LLTool, public LLSingleton<ALToolPathEdit>
{
    LLSINGLETON(ALToolPathEdit);
    ~ALToolPathEdit() override;

public:
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleMouseUp(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleKey(KEY key, MASK mask) override;

    void handleSelect() override;
    void handleDeselect() override;
    void onMouseCaptureLost() override;

private:
    // the actor whose path we edit: the shared edit-selection actor
    LLUUID targetActor() const;

    // nearest node to (x,y) within a pixel threshold; -1 = none
    S32  pickNode(S32 x, S32 y, const LLUUID& actor) const;
    // nearest path SEGMENT (returns the start-node index) within a pixel
    // threshold; -1 = none. Used to insert a bend where the line was clicked.
    S32  pickSegment(S32 x, S32 y, const LLUUID& actor) const;
    // world pick -> global surface point; false when the pick misses (sky)
    bool groundPointAt(S32 x, S32 y, LLVector3d& out_global) const;

    // node context menu (built lazily on first activation)
    void ensureMenu();
    void onMenuDelete();
    void onMenuInsertAfter();

    S32  mDragNode = -1;    // node being dragged (-1 = none)
    LLHandle<LLContextMenu> mMenuHandle;
};

#endif // AL_ALTOOLPATHEDIT_H
