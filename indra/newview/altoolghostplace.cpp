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
#include "altoolghostedit.h"
#include "lltoolmgr.h"
#include "llviewerwindow.h"         // gViewerWindow, pickImmediate

#include <utility>

ALToolGhostPlace::ALToolGhostPlace()
:   LLTool(std::string("GhostPlace"))
{
}

bool ALToolGhostPlace::armFor(
    const LLUUID& owner_id, const LLUUID& instance_id, bool facing_target)
{
    if (owner_id.isNull() || instance_id.isNull() ||
        (mOwnerId.notNull() && mOwnerId != owner_id) ||
        ALGhostStudio::instance().getCrowdPlacementDraft().mActive ||
        (ALToolGhostEdit::instanceExists() &&
         ALToolGhostEdit::getInstance()->isEditModeActive()))
    {
        return false;
    }
    disarm(true);
    if (mOwnerId.notNull() ||
        ALGhostStudio::instance().getCrowdPlacementDraft().mActive ||
        (ALToolGhostEdit::instanceExists() &&
         ALToolGhostEdit::getInstance()->isEditModeActive()))
    {
        // A cancellation callback re-armed the singleton. Never overwrite that
        // newer ownership claim on return from user code.
        return false;
    }
    mOwnerId = owner_id;
    mInstance = instance_id;
    mFacingTarget = facing_target;
    return true;
}

bool ALToolGhostPlace::armForPointPick(
    const LLUUID& owner_id, PointPickCallback callback)
{
    if (owner_id.isNull() || !callback ||
        (mOwnerId.notNull() && mOwnerId != owner_id) ||
        ALGhostStudio::instance().getCrowdPlacementDraft().mActive ||
        (ALToolGhostEdit::instanceExists() &&
         ALToolGhostEdit::getInstance()->isEditModeActive()))
    {
        return false;
    }
    disarm(true);
    if (mOwnerId.notNull() ||
        ALGhostStudio::instance().getCrowdPlacementDraft().mActive ||
        (ALToolGhostEdit::instanceExists() &&
         ALToolGhostEdit::getInstance()->isEditModeActive()))
    {
        return false;
    }
    mOwnerId = owner_id;
    mInstance.setNull();
    mFacingTarget = false;
    mPointPickCallback = std::move(callback);
    return true;
}

bool ALToolGhostPlace::cancelForOwner(const LLUUID& owner_id)
{
    if (owner_id.isNull() || mOwnerId != owner_id)
    {
        return false;
    }
    LLToolMgr* tool_mgr = LLToolMgr::getInstance();
    if (tool_mgr->usingTransientTool() &&
        tool_mgr->getCurrentTool() == this)
    {
        tool_mgr->clearTransientTool();
    }
    else
    {
        disarm(true);
    }
    return true;
}

void ALToolGhostPlace::disarm(bool notify_cancel)
{
    PointPickCallback callback = std::move(mPointPickCallback);
    mOwnerId.setNull();
    mInstance.setNull();
    mFacingTarget = false;
    if (notify_cancel && callback)
    {
        callback(false, LLVector3d());
    }
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

    // The crowd-facing picker is intentionally a data-only branch. Move the
    // callback out before invoking it so clearing the transient tool cannot
    // report a second, spurious cancellation from handleDeselect().
    if (mPointPickCallback)
    {
        PointPickCallback accepted = std::move(mPointPickCallback);
        accepted(true, pick.mPosGlobal);
    }
    else
    {
        ALGhostStudio& studio = ALGhostStudio::instance();
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(mInstance);
        const bool group_key =
            group && group->mId == mInstance;
        LLVector3d unit_foot;
        LLQuaternion unit_rotation;
        F32 unit_scale = 1.f;
        if (!studio.getUnitTransform(
                mInstance, unit_foot, unit_rotation, unit_scale))
        {
            LLToolMgr::getInstance()->clearTransientTool();
            return true;
        }
        if (mFacingTarget)
        {
            const LLVector3d delta = pick.mPosGlobal - unit_foot;
            if (delta.mdV[VX] * delta.mdV[VX] + delta.mdV[VY] * delta.mdV[VY] >= 0.000001)
            {
                const LLQuaternion facing(
                    atan2f((F32)delta.mdV[VY], (F32)delta.mdV[VX]),
                    LLVector3::z_axis);
                if (group_key)
                {
                    studio.transformGroup(
                        mInstance, unit_foot, facing, unit_scale);
                }
                else
                {
                    studio.transformUnit(
                        mInstance, unit_foot, facing, unit_scale);
                }
            }
            for (const LLUUID& member_id : studio.groupMembers(mInstance))
            {
                if (ALGhostStudio::Instance* member = studio.getInstance(member_id))
                {
                    member->mLookTarget = ALGhostStudio::LOOK_TARGET_POINT;
                    member->mLookTargetId.setNull();
                    member->mLookPointGlobal = pick.mPosGlobal;
                }
            }
        }
        else if (group_key)
        {
            studio.transformGroup(
                mInstance, pick.mPosGlobal,
                unit_rotation, unit_scale);
        }
        else
        {
            studio.transformUnit(
                mInstance, pick.mPosGlobal,
                unit_rotation, unit_scale);
        }
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
    // Never leave a stale arm pointing at an old ghost, and report a point-pick
    // cancellation exactly once if tool replacement ended the session.
    disarm(true);
}
