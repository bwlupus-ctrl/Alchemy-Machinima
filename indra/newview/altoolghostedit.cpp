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
#include "llagent.h"                // global <-> agent (pick projection)
#include "lltoolmgr.h"
#include "llviewercamera.h"         // projectPosAgentToScreen (screen-space pick)
#include "llviewerwindow.h"         // gViewerWindow

namespace
{
constexpr F32 GHOST_PICK_PX = 18.f;     // screen radius for a ghost hit, px
} // anonymous namespace

ALToolGhostEdit::ALToolGhostEdit()
:   LLTool(std::string("GhostEdit"))
{
}

// ---------------------------------------------------------------------------
LLUUID ALToolGhostEdit::pickInstance(S32 x, S32 y) const
{
    // screen-space pick like the path tool's node pick: the ghosts are overlay
    // draws, not scene objects, so project each enabled instance's FOOT and a
    // MID-BODY point (foot + 1 m, easier to grab) and take the nearest in radius
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    LLUUID best;
    F32 best_d2 = GHOST_PICK_PX * GHOST_PICK_PX;
    for (const ALGhostStudio::Instance& inst : ALGhostStudio::instance().getInstances())
    {
        if (!inst.mEnabled)
        {
            continue;       // you pick what you can see; hidden ghosts via the list
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
    // Select the clicked ghost; the per-frame proxy tick then spawns the proxy on
    // the selection and hands off to the stock move/rotate gizmos. This only runs
    // while NO ghost is selected yet -- once one is, the stock tool is current.
    const LLUUID hit = pickInstance(x, y);
    if (hit.notNull())
    {
        ALGhostStudio::instance().setSelected(hit);
    }
    return true;        // tool mode: clicks never fall through to world selection
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
        stopEditMode();
        LLToolMgr::getInstance()->clearTransientTool();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
void ALToolGhostEdit::handleSelect()
{
    mEditModeActive = true;
    mManipProxy.begin();
    gViewerWindow->setCursor(UI_CURSOR_TOOLTRANSLATE);
}

void ALToolGhostEdit::handleDeselect()
{
    // DO NOT tear the proxy down here. Switching to the stock toolset (the
    // intentional gizmo handoff) deselects this transient tool and fires
    // handleDeselect while editing legitimately continues. Only reflect a real
    // exit -- the explicit stopEditMode() path (panel toggle / Esc) clears it.
    if (!mManipProxy.isActive())
    {
        mEditModeActive = false;
    }
}

void ALToolGhostEdit::stopEditMode(bool restore_toolset)
{
    mEditModeActive = false;
    mManipProxy.teardown(restore_toolset);
}
