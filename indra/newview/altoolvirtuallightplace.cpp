/**
 * @file altoolvirtuallightplace.cpp
 * @brief One-shot virtual light placement / aim pick -- see altoolvirtuallightplace.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "altoolvirtuallightplace.h"

#include "alcleanframe.h"
#include "alvirtuallights.h"
#include "indra_constants.h"
#include "llagent.h"
#include "llsurface.h"
#include "lltoolmgr.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"

#include <utility>

ALToolVirtualLightPlace::ALToolVirtualLightPlace()
:   LLTool(std::string("VirtualLightPlace"))
{
}

void ALToolVirtualLightPlace::start(Mode mode, DoneCallback done)
{
    disarm(true);
    mMode = mode;
    mDone = std::move(done);
}

bool ALToolVirtualLightPlace::armPlace(DoneCallback done)
{
    if (ALCleanFrame::active())
    {
        if (done) done(false, ALVirtualLightsModel::refusalText(ALVirtualLightsModel::Refusal::CLEAN_FRAME));
        return false;
    }
    start(Mode::PLACE, std::move(done));
    mLightKey.setNull();
    LLToolMgr::getInstance()->setTransientTool(this);
    return true;
}

bool ALToolVirtualLightPlace::armAim(const LLUUID& light_key, DoneCallback done)
{
    if (light_key.isNull() || !ALVirtualLights::instance().find(light_key) || ALCleanFrame::active())
    {
        if (done) done(false, "Pick a virtual light first.");
        return false;
    }
    start(Mode::AIM, std::move(done));
    mLightKey = light_key;
    LLToolMgr::getInstance()->setTransientTool(this);
    return true;
}

void ALToolVirtualLightPlace::cancel()
{
    LLToolMgr* tool_mgr = LLToolMgr::getInstance();
    if (tool_mgr->usingTransientTool() && tool_mgr->getCurrentTool() == this)
        tool_mgr->clearTransientTool();   // handleDeselect() -> disarm(true)
    else
        disarm(true);
}

void ALToolVirtualLightPlace::disarm(bool notify_cancel)
{
    DoneCallback done = std::move(mDone);
    const bool was_armed = mMode != Mode::NONE;
    mDone = nullptr;
    mMode = Mode::NONE;
    mLightKey.setNull();
    if (notify_cancel && was_armed && done) done(false, "Cancelled");
}

bool ALToolVirtualLightPlace::handleMouseDown(S32 x, S32 y, MASK)
{
    if (mMode == Mode::NONE) return true;
    // pickImmediate (synchronous): mPosGlobal, never mIntersection (zero on a terrain hit).
    const LLPickInfo pick = gViewerWindow->pickImmediate(x, y, false, false);
    if (pick.mPosGlobal.isExactlyZero() || !pick.mPosGlobal.isFinite())
        return true;   // sky / void: stay armed

    ALVirtualLights& lights = ALVirtualLights::instance();
    ALVirtualLights::flushPending();
    const LLSD before = lights.capture();
    // Take the callback out first so clearing the transient tool cannot report a second cancel.
    auto finish = [this](bool ok, const std::string& text)
    {
        DoneCallback done = std::move(mDone);
        mDone = nullptr;
        mMode = Mode::NONE;
        mLightKey.setNull();
        LLToolMgr::getInstance()->clearTransientTool();
        if (done) done(ok, text);
    };

    if (mMode == Mode::PLACE)
    {
        LLVector3 normal = pick.mNormal;
        LLViewerRegion* region = gAgent.getRegion();
        if (pick.mObjectID.isNull() || normal.isExactlyZero())
        {
            // Land: the surface normal comes from the terrain, not from the pick.
            normal = region ? region->getLand().resolveNormalGlobal(pick.mPosGlobal)
                            : LLVector3(0.f, 0.f, 1.f);
        }
        std::string reason;
        const LLUUID key = lights.placeAtGlobal(pick.mPosGlobal, normal, &reason);
        if (key.isNull())
        {
            // Outside the region is retryable (pick another spot); the cap or a clean frame ends the arm.
            if (reason == ALVirtualLightsModel::refusalText(ALVirtualLightsModel::Refusal::OUTSIDE_REGION))
                return true;
            finish(false, reason);
            return true;
        }
        ALVirtualLights::recordUndo(before, "Place virtual light");
        finish(true, lights.label(key));
        return true;
    }

    // AIM: an object under the pointer is followed; a bare spot is aimed at once.
    const LLUUID key = mLightKey;
    LLViewerObject* object = pick.mObjectID.notNull() ? gObjectList.findObject(pick.mObjectID) : nullptr;
    if (object && lights.isVirtualObject(object->getID())) object = nullptr;
    bool changed = false;
    if (object)
    {
        LLViewerObject* root = object->getRootEdit();
        changed = lights.setAimObject(key, root ? root->getID() : object->getID());
    }
    else if (LLViewerRegion* region = gAgent.getRegion())
    {
        if (region->pointInRegionGlobal(pick.mPosGlobal))
        {
            const LLVector3 local = region->getPosRegionFromGlobal(pick.mPosGlobal);
            const F32 point[3] = {local.mV[0], local.mV[1], local.mV[2]};
            changed = lights.edit(key, [&](ALVirtualLights::Light& light)
            {
                light.mAimObject.setNull();
                ALVirtualLightsModel::aimAt(light, point);
            });
        }
    }
    if (changed) ALVirtualLights::recordUndo(before, "Aim virtual light");
    finish(true, object ? "Aiming at the object" : "Aimed at the spot");
    return true;
}

bool ALToolVirtualLightPlace::handleHover(S32, S32, MASK)
{
    gViewerWindow->setCursor(UI_CURSOR_PIPETTE);
    return true;
}

bool ALToolVirtualLightPlace::handleRightMouseDown(S32, S32, MASK)
{
    LLToolMgr::getInstance()->clearTransientTool();   // right click = cancel
    return true;
}

bool ALToolVirtualLightPlace::handleKey(KEY key, MASK)
{
    if (key == KEY_ESCAPE)
    {
        LLToolMgr::getInstance()->clearTransientTool();
        return true;
    }
    return false;
}

void ALToolVirtualLightPlace::handleSelect()
{
    gViewerWindow->setCursor(UI_CURSOR_PIPETTE);
}

void ALToolVirtualLightPlace::handleDeselect()
{
    disarm(true);
}
