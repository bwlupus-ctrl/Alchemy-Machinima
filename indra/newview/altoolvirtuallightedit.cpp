/**
 * @file altoolvirtuallightedit.cpp
 * @brief In-world drag tool for virtual lights -- see altoolvirtuallightedit.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "altoolvirtuallightedit.h"

#include "alcleanframe.h"
#include "alkitpage.h"
#include "alkitshell.h"
#include "alvirtuallights.h"
#include "indra_constants.h"
#include "llagent.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "lltoolmgr.h"
#include "llviewercamera.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"

namespace
{
constexpr S32 PICK_RADIUS_PX = 16;
constexpr char PRACTICALS_PAGE[] = "light_group/practicals_tab";

bool allowed()
{
    return !ALCleanFrame::active() && gAgent.getRegion() != nullptr;
}

ALKitPage* practicalsPage()
{
    LLFloater* director = LLFloaterReg::findInstance("director");
    ALKitShell* shell = director ? director->findChild<ALKitShell>("director_shell") : nullptr;
    return shell ? dynamic_cast<ALKitPage*>(shell->ensurePageBuilt(PRACTICALS_PAGE)) : nullptr;
}
}

ALToolVirtualLightEdit::ALToolVirtualLightEdit()
:   LLTool(std::string("VirtualLightEdit"))
{
}

bool ALToolVirtualLightEdit::handleMouseDown(S32 x, S32 y, MASK)
{
    if (!allowed()) return true;
    ALVirtualLights& lights = ALVirtualLights::instance();
    std::vector<ALVirtualLights::Pick> picks;
    lights.collectPicks(picks);
    // The tip handle wins a tie with its own body: it is the smaller target.
    S32 best = PICK_RADIUS_PX * PICK_RADIUS_PX;
    const ALVirtualLights::Pick* nearest = nullptr;
    for (const auto& pick : picks)
    {
        LLCoordGL screen;
        if (!LLViewerCamera::getInstance()->projectPosAgentToScreen(pick.mAgentPos, screen, false)) continue;
        const S32 d = (screen.mX - x) * (screen.mX - x) + (screen.mY - y) * (screen.mY - y);
        if (d < best || (d == best && nearest && pick.mHandle > nearest->mHandle))
        {
            best = d;
            nearest = &pick;
        }
    }
    if (!nearest) return true;

    mKey = nearest->mKey;
    mHandle = nearest->mHandle;
    mStartGlobal = gAgent.getPosGlobalFromAgent(nearest->mAgentPos);
    lights.setFocus(mKey);
    LLVector3d hit;
    if (!gViewerWindow->mousePointOnPlaneGlobal(hit, x, y, mStartGlobal, LLViewerCamera::getInstance()->getAtAxis()))
        return true;
    mPlaneHit = hit;
    ALVirtualLights::flushPending();
    mBefore = lights.capture();
    mEpoch = lights.sceneEpoch();
    mMoved = false;
    mDragging = true;
    setMouseCapture(true);
    return true;
}

bool ALToolVirtualLightEdit::handleHover(S32 x, S32 y, MASK mask)
{
    ALVirtualLights& lights = ALVirtualLights::instance();
    if (mDragging && (lights.sceneEpoch() != mEpoch || !allowed()))
    {
        cancelGesture();   // region change / clear / clean frame mid-drag
        return true;
    }
    if (mDragging && hasMouseCapture())
    {
        LLVector3d hit;
        LLViewerRegion* region = gAgent.getRegion();
        if (region && gViewerWindow->mousePointOnPlaneGlobal(hit, x, y, mStartGlobal, LLViewerCamera::getInstance()->getAtAxis()))
        {
            LLVector3d delta = hit - mPlaneHit;
            if (mHandle == 0 && (mask & MASK_CONTROL))
            {
                delta.mdV[VX] = 0.0;   // Ctrl: vertical only (L10)
                delta.mdV[VY] = 0.0;
            }
            const LLVector3d target = mStartGlobal + delta;
            if (region->pointInRegionGlobal(target))
            {
                const LLVector3 local = region->getPosRegionFromGlobal(target);
                const F32 point[3] = {local.mV[0], local.mV[1], local.mV[2]};
                const S32 handle = mHandle;
                if (lights.edit(mKey, [&](ALVirtualLights::Light& light)
                    {
                        if (handle == 0)
                        {
                            for (S32 i = 0; i < 3; ++i) light.mPos[i] = point[i];
                        }
                        else
                        {
                            light.mAimObject.setNull();
                            ALVirtualLightsModel::aimAt(light, point);
                        }
                    }))
                    mMoved = true;
            }
        }
    }
    gViewerWindow->setCursor(UI_CURSOR_TOOLGRAB);
    return true;
}

bool ALToolVirtualLightEdit::handleMouseUp(S32, S32, MASK)
{
    if (mDragging)
    {
        const bool moved = mMoved;
        const bool same_scene = ALVirtualLights::instance().sceneEpoch() == mEpoch;
        mDragging = false;
        if (moved && same_scene)
            ALVirtualLights::recordUndo(mBefore, mHandle == 0 ? "Move virtual light" : "Aim virtual light");
    }
    mDragging = false;
    if (hasMouseCapture()) setMouseCapture(false);
    return true;
}

void ALToolVirtualLightEdit::cancelGesture()
{
    if (mDragging && mMoved && ALVirtualLights::instance().sceneEpoch() == mEpoch)
    {
        std::string reason;
        ALVirtualLights::instance().restore(mBefore, reason);
    }
    mDragging = false;
    mMoved = false;
    if (hasMouseCapture()) setMouseCapture(false);
}

bool ALToolVirtualLightEdit::handleKey(KEY key, MASK mask)
{
    if (key == KEY_ESCAPE)
    {
        cancelGesture();
        return true;
    }
    if ((mask & MASK_CONTROL) && (key == 'Z' || key == 'Y'))
    {
        cancelGesture();
        if (ALKitPage* page = practicalsPage()) page->undoOnPage(key == 'Y');
        return true;
    }
    if ((key == KEY_DELETE || key == KEY_BACKSPACE) && allowed())
    {
        cancelGesture();
        ALVirtualLights& lights = ALVirtualLights::instance();
        const LLUUID focus = lights.focusKey();
        if (focus.notNull())
        {
            ALVirtualLights::flushPending();
            const LLSD before = lights.capture();
            if (lights.remove(focus)) ALVirtualLights::recordUndo(before, "Delete virtual light");
        }
        return true;
    }
    return false;
}

void ALToolVirtualLightEdit::handleSelect()
{
    ALVirtualLights::instance().setEditTool(true);
    gViewerWindow->setCursor(UI_CURSOR_TOOLGRAB);
}

void ALToolVirtualLightEdit::handleDeselect()
{
    cancelGesture();
    ALVirtualLights::instance().setEditTool(false);
}
