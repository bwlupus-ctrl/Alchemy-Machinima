/**
 * @file altoolghostplace.cpp
 * @brief One-shot Ghost Studio placement tool -- see altoolghostplace.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "altoolghostplace.h"

#include "indra_constants.h"        // KEY_ESCAPE
#include "alghoststudio.h"
#include "lltoolmgr.h"
#include "llviewerwindow.h"         // gViewerWindow, pickImmediate

ALToolGhostPlace::ALToolGhostPlace()
:   LLTool(std::string("GhostPlace"))
{
}

// ---------------------------------------------------------------------------
bool ALToolGhostPlace::handleMouseDown(S32 x, S32 y, MASK mask)
{
    // normal world pick, avatars excluded (rigged false) so the ray falls
    // through a body to the floor behind it -- same pick as the path tool
    LLPickInfo pick = gViewerWindow->pickImmediate(x, y, /*transparent*/ false, /*rigged*/ false);
    if (pick.mPosGlobal.isExactlyZero())
    {
        return true;    // sky-miss: consumed, still armed for another try
    }
    if (ALGhostStudio::Instance* inst = ALGhostStudio::instance().getInstance(mInstance))
    {
        inst->mFootGlobal = pick.mPosGlobal;    // feet onto the picked surface
    }
    // one-shot done (even if the instance vanished meanwhile): camera back
    LLToolMgr::getInstance()->clearTransientTool();
    return true;
}

bool ALToolGhostPlace::handleHover(S32 x, S32 y, MASK mask)
{
    gViewerWindow->setCursor(UI_CURSOR_TOOLCREATE);
    return true;
}

bool ALToolGhostPlace::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    // right-click = cancel the placement (and let go of the camera); never
    // trap the user in a mode they can't see how to leave
    LLToolMgr::getInstance()->clearTransientTool();
    return true;
}

bool ALToolGhostPlace::handleKey(KEY key, MASK mask)
{
    if (key == KEY_ESCAPE)
    {
        LLToolMgr::getInstance()->clearTransientTool();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
void ALToolGhostPlace::handleSelect()
{
    gViewerWindow->setCursor(UI_CURSOR_TOOLCREATE);
}

void ALToolGhostPlace::handleDeselect()
{
    mInstance.setNull();    // never leave a stale arm pointing at an old ghost
}
