/**
 * @file altoolghostedit.cpp
 * @brief Persistent Ghost Studio edit tool -- see altoolghostedit.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "altoolghostedit.h"

#include "indra_constants.h"        // KEY_DELETE / KEY_BACKSPACE / KEY_ESCAPE
#include "alghoststudio.h"
#include "llagent.h"                // global <-> agent conversion
#include "lltoolmgr.h"
#include "llviewercamera.h"         // projectPosAgentToScreen (screen-space pick)
#include "llviewerwindow.h"         // gViewerWindow, pickImmediate

namespace
{
constexpr F32 GHOST_PICK_PX = 18.f;     // screen radius for a ghost hit, px
// yaw sensitivity for Shift-drag: full turn in ~360 px of horizontal travel
constexpr F32 YAW_PER_PX = 0.0175f;     // radians (~1 degree per pixel)
} // anonymous namespace

ALToolGhostEdit::ALToolGhostEdit()
:   LLTool(std::string("GhostEdit"))
{
}

// ---------------------------------------------------------------------------
bool ALToolGhostEdit::groundPointAt(S32 x, S32 y, LLVector3d& out_global) const
{
    // a normal world pick: land or prim surface, avatars excluded (rigged
    // false) so the ray falls through a body to the floor -- ALToolPathEdit's
    // exact pick (ghosts themselves are an overlay and never intercept it)
    LLPickInfo pick = gViewerWindow->pickImmediate(x, y, /*transparent*/ false, /*rigged*/ false);
    if (pick.mPosGlobal.isExactlyZero())
    {
        return false;       // missed the world (sky)
    }
    out_global = pick.mPosGlobal;
    return true;
}

LLUUID ALToolGhostEdit::pickInstance(S32 x, S32 y) const
{
    // screen-space pick like the path tool's node pick: the ghosts are overlay
    // draws, not scene objects, so project each enabled instance's FOOT and a
    // MID-BODY point (foot + 1 m, easier to grab than the ground marker) and
    // take the nearest within the radius
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    LLUUID best;
    F32 best_d2 = GHOST_PICK_PX * GHOST_PICK_PX;
    for (const ALGhostStudio::Instance& inst : ALGhostStudio::instance().getInstances())
    {
        if (!inst.mEnabled)
        {
            continue;       // you drag what you can see; hidden ghosts via the list
        }
        const LLVector3 foot = gAgent.getPosAgentFromGlobal(inst.mFootGlobal);
        LLVector3 probes[2] = { foot, foot };
        probes[1].mV[VZ] += 1.f * inst.mScale;      // mid-body, scale-aware
        for (const LLVector3& p : probes)
        {
            LLCoordGL screen;
            if (!cam->projectPosAgentToScreen(p, screen, false))
            {
                continue;   // behind the camera
            }
            const F32 dx = (F32)(screen.mX - x);
            const F32 dy = (F32)(screen.mY - y);
            const F32 d2 = dx * dx + dy * dy;
            if (d2 < best_d2)
            {
                best_d2 = d2;
                best = inst.mId;
            }
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
bool ALToolGhostEdit::handleMouseDown(S32 x, S32 y, MASK mask)
{
    ALGhostStudio& studio = ALGhostStudio::instance();

    // click a ghost -> select + begin a drag (Shift = yaw mode)
    const LLUUID hit = pickInstance(x, y);
    if (hit.notNull())
    {
        studio.setSelected(hit);
        mDragInstance = hit;
        mYawDrag = (mask & MASK_SHIFT) != 0;
        if (mYawDrag)
        {
            mYawAnchorX = x;
            if (const ALGhostStudio::Instance* inst = studio.getInstance(hit))
            {
                mYawStart = inst->mYaw;
            }
        }
        setMouseCapture(true);
        return true;
    }

    // empty ground with a selection -> re-place the selected ghost there
    if (studio.getSelected().notNull())
    {
        LLVector3d gp;
        if (groundPointAt(x, y, gp))
        {
            if (ALGhostStudio::Instance* inst = studio.getInstance(studio.getSelected()))
            {
                inst->mFootGlobal = gp;
            }
        }
        return true;    // consumed either way (a sky-miss just does nothing)
    }
    return true;        // tool mode: clicks never fall through to selection
}

bool ALToolGhostEdit::handleHover(S32 x, S32 y, MASK mask)
{
    if (mDragInstance.notNull() && hasMouseCapture())
    {
        ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(mDragInstance);
        if (inst)
        {
            if (mYawDrag)
            {
                // horizontal travel turns the ghost; position untouched
                inst->mYaw = mYawStart + (F32)(x - mYawAnchorX) * YAW_PER_PX;
            }
            else
            {
                LLVector3d gp;
                if (groundPointAt(x, y, gp))
                {
                    inst->mFootGlobal = gp;
                }
            }
        }
        gViewerWindow->setCursor(mYawDrag ? UI_CURSOR_TOOLROTATE : UI_CURSOR_TOOLGRAB);
        return true;
    }
    gViewerWindow->setCursor(UI_CURSOR_TOOLTRANSLATE);
    return true;
}

bool ALToolGhostEdit::handleMouseUp(S32 x, S32 y, MASK mask)
{
    if (mDragInstance.notNull())
    {
        mDragInstance.setNull();
        mYawDrag = false;
        setMouseCapture(false);
        return true;
    }
    return false;
}

bool ALToolGhostEdit::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    // let the normal right-drag camera work -- never trap the user without
    // camera control (the path tool's rule)
    return false;
}

bool ALToolGhostEdit::handleKey(KEY key, MASK mask)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    if ((key == KEY_DELETE || key == KEY_BACKSPACE) && studio.getSelected().notNull())
    {
        studio.removeInstance(studio.getSelected());    // also clears the selection
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
void ALToolGhostEdit::handleSelect()
{
    gViewerWindow->setCursor(UI_CURSOR_TOOLTRANSLATE);
}

void ALToolGhostEdit::handleDeselect()
{
    if (hasMouseCapture())
    {
        setMouseCapture(false);
    }
    mDragInstance.setNull();
    mYawDrag = false;
}

void ALToolGhostEdit::onMouseCaptureLost()
{
    mDragInstance.setNull();
    mYawDrag = false;
}
