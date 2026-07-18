/**
 * @file altoolpathedit.cpp
 * @brief In-world edit tool-mode for Actor Pathing -- see altoolpathedit.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "altoolpathedit.h"

#include "indra_constants.h"        // KEY_DELETE / KEY_BACKSPACE / KEY_ESCAPE
#include "llactormover.h"
#include "llagent.h"                // global <-> agent conversion
#include "llmenugl.h"
#include "lltoolmgr.h"
#include "lluictrlfactory.h"
#include "llviewercamera.h"         // projectPosAgentToScreen (screen-space node pick)
#include "llviewermenu.h"           // gMenuHolder, LLViewerMenuHolderGL
#include "llviewerwindow.h"         // gViewerWindow, pickImmediate

namespace
{
constexpr F32 NODE_PICK_PX = 16.f;      // screen radius for a node hit, px
constexpr F32 SEG_PICK_PX  = 10.f;      // screen distance to a segment, px
} // anonymous namespace

ALToolPathEdit::ALToolPathEdit()
:   LLTool(std::string("ActorPathEdit"))
{
}

ALToolPathEdit::~ALToolPathEdit()
{
    if (LLContextMenu* menu = mMenuHandle.get())
    {
        menu->die();
        mMenuHandle.markDead();
    }
}

// ---------------------------------------------------------------------------
LLUUID ALToolPathEdit::targetActor() const
{
    return LLActorMover::instance().getEditActor();
}

bool ALToolPathEdit::groundPointAt(S32 x, S32 y, LLVector3d& out_global) const
{
    // a normal world pick: land or prim surface, avatars excluded (pick_rigged
    // false) so the ray falls through a body to the floor behind it
    LLPickInfo pick = gViewerWindow->pickImmediate(x, y, /*transparent*/ false, /*rigged*/ false);
    if (pick.mPosGlobal.isExactlyZero())
    {
        return false;       // missed the world (sky)
    }
    out_global = pick.mPosGlobal;
    return true;
}

S32 ALToolPathEdit::pickNode(S32 x, S32 y, const LLUUID& actor) const
{
    const LLActorMover::Path* path = LLActorMover::instance().getPath(actor);
    if (!path)
    {
        return -1;
    }
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    S32 best = -1;
    F32 best_d2 = NODE_PICK_PX * NODE_PICK_PX;
    for (S32 i = 0; i < (S32)path->mNodes.size(); ++i)
    {
        LLVector3 agent = gAgent.getPosAgentFromGlobal(path->mNodes[i].mPosGlobal);
        LLCoordGL screen;
        if (!cam->projectPosAgentToScreen(agent, screen, false))
        {
            continue;       // behind the camera
        }
        const F32 dx = (F32)(screen.mX - x);
        const F32 dy = (F32)(screen.mY - y);
        const F32 d2 = dx * dx + dy * dy;
        if (d2 < best_d2)
        {
            best_d2 = d2;
            best = i;
        }
    }
    return best;
}

S32 ALToolPathEdit::pickSegment(S32 x, S32 y, const LLUUID& actor) const
{
    const LLActorMover::Path* path = LLActorMover::instance().getPath(actor);
    if (!path || path->mNodes.size() < 2)
    {
        return -1;
    }
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    const F32 px = (F32)x, py = (F32)y;
    S32 best = -1;
    F32 best_d = SEG_PICK_PX;
    for (S32 i = 0; i + 1 < (S32)path->mNodes.size(); ++i)
    {
        LLCoordGL a, b;
        LLVector3 pa = gAgent.getPosAgentFromGlobal(path->mNodes[i].mPosGlobal);
        LLVector3 pb = gAgent.getPosAgentFromGlobal(path->mNodes[i + 1].mPosGlobal);
        if (!cam->projectPosAgentToScreen(pa, a, false) ||
            !cam->projectPosAgentToScreen(pb, b, false))
        {
            continue;
        }
        // distance from (px,py) to the screen segment a->b
        const F32 ax = (F32)a.mX, ay = (F32)a.mY;
        const F32 bx = (F32)b.mX, by = (F32)b.mY;
        const F32 vx = bx - ax, vy = by - ay;
        const F32 len2 = vx * vx + vy * vy;
        F32 t = (len2 > 1e-3f) ? ((px - ax) * vx + (py - ay) * vy) / len2 : 0.f;
        t = llclamp(t, 0.f, 1.f);
        const F32 cx = ax + t * vx, cy = ay + t * vy;
        const F32 d = sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy));
        if (d < best_d)
        {
            best_d = d;
            best = i;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
bool ALToolPathEdit::handleMouseDown(S32 x, S32 y, MASK mask)
{
    const LLUUID actor = targetActor();
    LLActorMover& mover = LLActorMover::instance();

    // "Walk to here" one-shot: the next ground click builds a fresh straight
    // path and walks it (startWalkTo snapshots undo itself). A sky-miss keeps
    // the arm so the user can click again; a hit hands the camera back.
    if (mWalkToArmed)
    {
        LLVector3d gp;
        if (groundPointAt(x, y, gp))
        {
            mover.startWalkTo(actor, gp);
            mWalkToArmed = false;
            LLToolMgr::getInstance()->clearTransientTool();
        }
        return true;
    }

    // click a node -> select + begin drag (works even if the actor can't
    // resolve: the path data is keyed by id, independent of the avatar). The
    // pre-drag undo snapshot is deferred to the first actual move (handleHover)
    // so a plain select-click leaves no empty undo entry.
    const S32 node = pickNode(x, y, actor);
    if (node >= 0)
    {
        mover.setEditNode(node);
        mDragNode = node;
        mDragDidSnapshot = false;
        setMouseCapture(true);
        return true;
    }

    // otherwise a ground placement
    LLVector3d gp;
    if (!groundPointAt(x, y, gp))
    {
        return true;        // consumed but nothing to place (clicked the sky)
    }
    mover.snapshotForUndo(actor);       // a discrete placement is one undo step
    const S32 seg = pickSegment(x, y, actor);
    if (seg >= 0)
    {
        // clicked the line between node seg and seg+1: insert a bend there
        const S32 at = mover.insertWaypoint(actor, seg + 1, gp);
        mover.setEditNode(at);
    }
    else
    {
        const S32 at = mover.appendWaypointAt(actor, gp);
        mover.setEditNode(at);
    }
    return true;
}

bool ALToolPathEdit::handleHover(S32 x, S32 y, MASK mask)
{
    if (mDragNode >= 0 && hasMouseCapture())
    {
        LLVector3d gp;
        if (groundPointAt(x, y, gp))
        {
            LLActorMover& mover = LLActorMover::instance();
            // snapshot the pre-drag state once, on the first real move, so the
            // whole drag collapses into a single undo entry
            if (!mDragDidSnapshot)
            {
                mover.snapshotForUndo(targetActor());
                mDragDidSnapshot = true;
            }
            mover.moveWaypoint(targetActor(), mDragNode, gp);
        }
        gViewerWindow->setCursor(UI_CURSOR_TOOLGRAB);
        return true;
    }
    gViewerWindow->setCursor(UI_CURSOR_CROSS);
    return true;
}

bool ALToolPathEdit::handleMouseUp(S32 x, S32 y, MASK mask)
{
    if (mDragNode >= 0)
    {
        mDragNode = -1;
        setMouseCapture(false);
        return true;
    }
    return false;
}

bool ALToolPathEdit::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    const S32 node = pickNode(x, y, targetActor());
    if (node < 0)
    {
        // not on a node: let the normal right-drag camera work (never trap the
        // user without camera control)
        return false;
    }
    LLActorMover::instance().setEditNode(node);
    ensureMenu();
    if (LLContextMenu* menu = mMenuHandle.get())
    {
        menu->buildDrawLabels();
        menu->updateParent(LLMenuGL::sMenuContainer);
        menu->show(x, y);
    }
    return true;
}

bool ALToolPathEdit::handleKey(KEY key, MASK mask)
{
    // Delete/Backspace removes the selected node; Esc leaves the tool cleanly
    LLActorMover& mover = LLActorMover::instance();
    if ((key == KEY_DELETE || key == KEY_BACKSPACE) && mover.getEditNode() >= 0)
    {
        const S32 n = mover.getEditNode();
        mover.snapshotForUndo(mover.getEditActor());
        if (mover.deleteWaypoint(mover.getEditActor(), n))
        {
            mover.setEditNode(-1);
        }
        return true;
    }
    if (key == KEY_ESCAPE)
    {
        LLToolMgr::getInstance()->clearTransientTool();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
void ALToolPathEdit::handleSelect()
{
    gViewerWindow->setCursor(UI_CURSOR_CROSS);
}

void ALToolPathEdit::handleDeselect()
{
    if (hasMouseCapture())
    {
        setMouseCapture(false);
    }
    mDragNode = -1;
    mDragDidSnapshot = false;
    mWalkToArmed = false;       // never leave a pending walk-to armed off-tool
}

void ALToolPathEdit::onMouseCaptureLost()
{
    mDragNode = -1;
    mDragDidSnapshot = false;
}

// ---------------------------------------------------------------------------
// node context menu
// ---------------------------------------------------------------------------
void ALToolPathEdit::ensureMenu()
{
    if (mMenuHandle.get())
    {
        return;
    }
    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
    registrar.add("PathNode.Delete", [this](LLUICtrl*, const LLSD&) { onMenuDelete(); });
    registrar.add("PathNode.InsertAfter", [this](LLUICtrl*, const LLSD&) { onMenuInsertAfter(); });
    if (LLContextMenu* menu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>(
            "menu_director_path_node.xml", gMenuHolder,
            LLViewerMenuHolderGL::child_registry_t::instance()))
    {
        mMenuHandle = menu->getHandle();
    }
}

void ALToolPathEdit::onMenuDelete()
{
    LLActorMover& mover = LLActorMover::instance();
    const S32 n = mover.getEditNode();
    if (n >= 0)
    {
        mover.snapshotForUndo(mover.getEditActor());
        if (mover.deleteWaypoint(mover.getEditActor(), n))
        {
            mover.setEditNode(-1);
        }
    }
}

void ALToolPathEdit::onMenuInsertAfter()
{
    LLActorMover& mover = LLActorMover::instance();
    const LLUUID actor = mover.getEditActor();
    const LLActorMover::Path* path = mover.getPath(actor);
    const S32 n = mover.getEditNode();
    if (!path || n < 0 || n >= (S32)path->mNodes.size())
    {
        return;
    }
    mover.snapshotForUndo(actor);
    // midpoint toward the next node (or an offset past the last node), so the
    // menu insert needs no second click and never lands on top of a neighbor
    LLVector3d mid;
    if (n + 1 < (S32)path->mNodes.size())
    {
        mid = (path->mNodes[n].mPosGlobal + path->mNodes[n + 1].mPosGlobal) * 0.5;
    }
    else if (n - 1 >= 0)
    {
        mid = path->mNodes[n].mPosGlobal
            + (path->mNodes[n].mPosGlobal - path->mNodes[n - 1].mPosGlobal) * 0.5;
    }
    else
    {
        mid = path->mNodes[n].mPosGlobal + LLVector3d(1.0, 0.0, 0.0);
    }
    const S32 at = mover.insertWaypoint(actor, n + 1, mid);
    mover.setEditNode(at);
}
