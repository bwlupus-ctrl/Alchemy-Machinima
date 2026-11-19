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
#include "alghostplacementadapter.h"
#include "altoolghostedit.h"
#include "llagent.h"
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
    // Work-plane fallback height: the target instance's own current foot Z
    // when we have one (this is the "source-foot height" the resolver ladder
    // wants), otherwise the agent's own height for the facing-point picker
    // (which has no instance yet).
    ALGhostStudio& studio = ALGhostStudio::instance();
    LLVector3d unit_foot;
    LLQuaternion unit_rotation;
    F32 unit_scale = 1.f;
    const bool have_unit_transform = mInstance.notNull() &&
        studio.getUnitTransform(mInstance, unit_foot, unit_rotation, unit_scale);
    const F64 work_plane_z = have_unit_transform
        ? unit_foot.mdV[VZ] : gAgent.getPositionGlobal().mdV[VZ];

    // Shared placement resolver: a sky/air render-pick miss now falls
    // through to an independent terrain ray, then a bounded work plane, then
    // a bounded camera-depth point -- open-space placement (over water, off
    // in the void, above a terrain dip the render pick skipped) is no longer
    // simply impossible. Only legality can still block the resolved anchor.
    const ALGhostPlacementResolver::ALGhostPlacementHit hit =
        ALGhostPlacementResolver::resolve(
            ALGhostPlacementAdapter::screenRequest(x, y, work_plane_z),
            ALGhostPlacementAdapter::realProbes(x, y));
    if (!hit.mValid)
    {
        return true;    // blocked (or truly unresolvable): consumed, still armed
    }

    // The crowd-facing picker is intentionally a data-only branch. Move the
    // callback out before invoking it so clearing the transient tool cannot
    // report a second, spurious cancellation from handleDeselect().
    if (mPointPickCallback)
    {
        PointPickCallback accepted = std::move(mPointPickCallback);
        accepted(true, hit.mPointGlobal);
    }
    else
    {
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(mInstance);
        const bool group_key =
            group && group->mId == mInstance;
        if (!have_unit_transform)
        {
            LLToolMgr::getInstance()->clearTransientTool();
            return true;
        }
        if (mFacingTarget)
        {
            const LLVector3d delta = hit.mPointGlobal - unit_foot;
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
                    member->mLookPointGlobal = hit.mPointGlobal;
                }
            }
        }
        else if (group_key)
        {
            studio.transformGroup(
                mInstance, hit.mPointGlobal,
                unit_rotation, unit_scale);
        }
        else
        {
            studio.transformUnit(
                mInstance, hit.mPointGlobal,
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
