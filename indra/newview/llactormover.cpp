/**
 * @file llactormover.cpp
 * @brief Local ("ghost") locomotion rig -- see llactormover.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llactormover.h"

#include "llanimationstates.h"      // ANIM_AGENT_WALK
#include "llappviewer.h"            // gFrameIntervalSeconds
#include "lljoint.h"
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl
#include "llviewerobjectlist.h"     // gObjectList
#include "llvoavatar.h"
#include "llvoavatarself.h"         // gAgentAvatarp, isAgentAvatarValid()

namespace
{
LLUUID sMoverTarget;    // session-only; null = my avatar

LLVOAvatar* resolve_actor(const LLUUID& id)
{
    if (id.notNull())
    {
        LLViewerObject* obj = gObjectList.findObject(id);
        LLVOAvatar* av = obj ? obj->asAvatar() : nullptr;
        if (av && !av->isDead())
        {
            return av;
        }
    }
    return isAgentAvatarValid() ? (LLVOAvatar*)gAgentAvatarp : nullptr;
}
} // anonymous namespace

// ---------------------------------------------------------------------------
LLActorMover& LLActorMover::instance()
{
    static LLActorMover sInstance;
    return sInstance;
}

//static
void LLActorMover::toggleTarget(const LLUUID& id)
{
    sMoverTarget = (sMoverTarget == id) ? LLUUID::null : id;
}

//static
bool LLActorMover::isTarget(const LLUUID& id)
{
    return id.notNull() && sMoverTarget == id;
}

// ---------------------------------------------------------------------------
void LLActorMover::start()
{
    static LLCachedControl<F32> speed(gSavedSettings, "ActorMoverSpeed", 1.0f);
    static LLCachedControl<F32> distance(gSavedSettings, "ActorMoverDistance", 6.f);
    static LLCachedControl<F32> heading(gSavedSettings, "ActorMoverHeading", 0.f);      // deg, rel facing
    static LLCachedControl<S32> end_mode(gSavedSettings, "ActorMoverEndMode", 0);
    static LLCachedControl<F32> nominal(gSavedSettings, "ActorMoverWalkNominal", 3.0f); // anim design speed

    LLVOAvatar* av = resolve_actor(sMoverTarget);
    if (!av || !av->getRootJoint())
    {
        return;
    }

    // travel bearing = actor's current facing plus the heading trim
    LLVector3 at = LLVector3(1.f, 0.f, 0.f) * av->getRenderRotation();
    const F32 yaw = atan2f(at.mV[VY], at.mV[VX]) + (F32)heading * DEG_TO_RAD;

    Move mv;
    mv.mOrigin   = av->getRootJoint()->getWorldPosition();   // rendered pose, incl. a prior move
    mv.mHeading  = yaw;
    mv.mSpeed    = llclamp((F32)speed, 0.05f, 10.f);
    mv.mDistance = llmax((F32)distance, 0.1f);
    mv.mEndMode  = llclamp((S32)end_mode, 0, 2);
    mv.mT        = 0.f;
    mMoves[av->getID()] = mv;

    // cadence lock: walk-cycle clock scaled to actual ground speed, so foot
    // plants match the traversal at ANY speed. Local motion + local clock:
    // nothing is sent to the sim.
    av->startMotion(ANIM_AGENT_WALK);
    av->setAnimTimeFactor(mv.mSpeed / llmax((F32)nominal, 0.5f));

    LL_INFOS("ActorMover") << "ghost move: " << av->getID() << " speed " << mv.mSpeed
                           << " dist " << mv.mDistance << LL_ENDL;
}

void LLActorMover::stop()
{
    LLVOAvatar* av = resolve_actor(sMoverTarget);
    if (av)
    {
        auto it = mMoves.find(av->getID());
        if (it != mMoves.end())
        {
            mMoves.erase(it);
            av->stopMotion(ANIM_AGENT_WALK);
            av->setAnimTimeFactor(1.f);
            return;
        }
    }
    // fallback: stop everything (target may have changed mid-move)
    for (auto& pair : mMoves)
    {
        if (LLVOAvatar* mav = resolve_actor(pair.first))
        {
            mav->stopMotion(ANIM_AGENT_WALK);
            mav->setAnimTimeFactor(1.f);
        }
    }
    mMoves.clear();
}

// ---------------------------------------------------------------------------
bool LLActorMover::applyOverride(LLVOAvatar* av)
{
    if (mMoves.empty() || !av)
    {
        return false;
    }
    auto it = mMoves.find(av->getID());
    if (it == mMoves.end())
    {
        return false;
    }
    if (av->isDead() || !av->getRootJoint())
    {
        mMoves.erase(it);
        return false;
    }

    Move& mv = it->second;
    mv.mT += llclamp(gFrameIntervalSeconds.value(), 0.f, 0.25f);

    // path parameter + travel direction (ping-pong reverses facing on the
    // return leg so the actor walks back rather than moonwalking)
    const F32 total = mv.mSpeed * mv.mT;
    F32 s;
    F32 face = mv.mHeading;
    switch (mv.mEndMode)
    {
        case 1:     // loop
            s = fmodf(total, mv.mDistance);
            break;
        case 2:     // ping-pong
        {
            const F32 c = fmodf(total, 2.f * mv.mDistance);
            if (c < mv.mDistance)
            {
                s = c;
            }
            else
            {
                s = 2.f * mv.mDistance - c;
                face = mv.mHeading + F_PI;
            }
            break;
        }
        default:    // hold at end
            s = llmin(total, mv.mDistance);
            if (s >= mv.mDistance && av->findMotion(ANIM_AGENT_WALK))
            {
                // arrived: settle into a stand instead of walking in place
                av->stopMotion(ANIM_AGENT_WALK);
                av->setAnimTimeFactor(1.f);
            }
            break;
    }

    const LLVector3 dir(cosf(mv.mHeading), sinf(mv.mHeading), 0.f);
    const LLVector3 pos = mv.mOrigin + dir * s;

    LLQuaternion rot;
    rot.setEulerAngles(0.f, 0.f, face);

    av->getRootJoint()->setWorldPosition(pos);
    av->getRootJoint()->setWorldRotation(rot);
    return true;
}
