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
#include "lldirectorcast.h"         // [Director] roster storage + per-actor loco anim
#include "llfloaterreg.h"           // heading preview only draws with the floater open
#include "llframetimer.h"           // per-frame idempotency for applyOverride()
#include "llgl.h"
#include "llglstates.h"             // LLGLSUIDefault (heading preview)
#include "lljoint.h"
#include "llrender.h"               // gGL (heading preview)
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl
#include "llviewershadermgr.h"      // gUIProgram (heading preview)
#include "llvoavatar.h"
#include "llvoavatarself.h"         // gAgentAvatarp, isAgentAvatarValid()

namespace
{
// [Director] roster storage moved into LLDirectorCast (the roster IS the
// cast); this stays the single resolution path for mover call sites.
// null id = my avatar; a stale roster id (actor left the region) resolves to
// nullptr rather than silently falling back to self, so multi-actor starts
// never double up on the agent avatar.
LLVOAvatar* resolve_actor(const LLUUID& id)
{
    return LLDirectorCast::instance().resolve(id);
}

// the locomotion anim a new Move should start for THIS actor: the cast
// member's per-actor override when set, else the shared custom override
// UUID, else ANIM_AGENT_WALK (all local playback only)
LLUUID locomotion_anim(const LLUUID& actor_id)
{
    // [Director] per-actor loco anim wins over the shared setting
    if (const LLDirectorCast::CastMember* member = LLDirectorCast::instance().getMember(actor_id))
    {
        if (member->mLocoAnim.notNull())
        {
            return member->mLocoAnim;
        }
    }
    static LLCachedControl<bool> use_custom(gSavedSettings, "ActorMoverUseCustomAnim", false);
    static LLCachedControl<std::string> custom(gSavedSettings, "ActorMoverCustomAnim", "");
    if (use_custom)
    {
        LLUUID id;
        if (LLUUID::parseUUID(custom, &id) && id.notNull())
        {
            return id;
        }
    }
    return ANIM_AGENT_WALK;
}
} // anonymous namespace

// ---------------------------------------------------------------------------
LLActorMover& LLActorMover::instance()
{
    static LLActorMover sInstance;
    return sInstance;
}

// [Director] the roster API is now an alias for Director cast membership
//static
void LLActorMover::toggleTarget(const LLUUID& id)
{
    if (id.isNull())
    {
        return;
    }
    LLDirectorCast::instance().toggle(id);
}

//static
bool LLActorMover::isTarget(const LLUUID& id)
{
    return id.notNull() && LLDirectorCast::instance().contains(id);
}

//static
const uuid_vec_t& LLActorMover::getRoster()
{
    return LLDirectorCast::instance().getIds();
}

// ---------------------------------------------------------------------------
void LLActorMover::start(const LLUUID& actor_id)
{
    static LLCachedControl<F32> speed(gSavedSettings, "ActorMoverSpeed", 1.0f);
    static LLCachedControl<F32> distance(gSavedSettings, "ActorMoverDistance", 6.f);
    static LLCachedControl<F32> heading(gSavedSettings, "ActorMoverHeading", 0.f);      // deg, rel facing
    static LLCachedControl<S32> end_mode(gSavedSettings, "ActorMoverEndMode", 0);
    static LLCachedControl<F32> nominal(gSavedSettings, "ActorMoverWalkNominal", 3.0f); // anim design speed

    LLVOAvatar* av = resolve_actor(actor_id);
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
    mv.mAnim     = locomotion_anim(av->getID());

    // restarting an actor mid-move with a different anim: stop the old one so
    // it doesn't keep looping under the new cycle (a placeAt() hold has no
    // anim, hence the null guard)
    auto old_it = mMoves.find(av->getID());
    if (old_it != mMoves.end() && old_it->second.mAnim.notNull()
        && old_it->second.mAnim != mv.mAnim)
    {
        av->stopMotion(old_it->second.mAnim);
    }

    mMoves[av->getID()] = mv;

    // cadence lock: walk-cycle clock scaled to actual ground speed, so foot
    // plants match the traversal at ANY speed. Local motion + local clock:
    // nothing is sent to the sim (custom anims play locally too).
    av->startMotion(mv.mAnim);
    av->setAnimTimeFactor(mv.mSpeed / llmax((F32)nominal, 0.5f));

    LL_INFOS("ActorMover") << "ghost move: " << av->getID() << " speed " << mv.mSpeed
                           << " dist " << mv.mDistance << " anim " << mv.mAnim << LL_ENDL;
}

void LLActorMover::startAll()
{
    const uuid_vec_t& roster = getRoster();
    if (roster.empty())
    {
        start(LLUUID::null);
        return;
    }
    for (const LLUUID& id : roster)
    {
        start(id);
    }
}

void LLActorMover::stop(const LLUUID& actor_id)
{
    LLVOAvatar* av = resolve_actor(actor_id);
    if (av)
    {
        auto it = mMoves.find(av->getID());
        if (it != mMoves.end())
        {
            const LLUUID anim = it->second.mAnim;
            mMoves.erase(it);
            if (anim.notNull())     // placeAt() holds carry no anim
            {
                av->stopMotion(anim);
            }
            av->setAnimTimeFactor(1.f);
        }
    }
}

void LLActorMover::stopAll()
{
    for (auto& pair : mMoves)
    {
        if (LLVOAvatar* mav = resolve_actor(pair.first))
        {
            if (pair.second.mAnim.notNull())    // placeAt() holds carry no anim
            {
                mav->stopMotion(pair.second.mAnim);
            }
            mav->setAnimTimeFactor(1.f);
        }
    }
    mMoves.clear();
}

// [Director] Reset-to-Marks snap-back: pin the rendered root at a mark with
// a zero-speed hold Move (same override path as a walk, so it wins against
// LLControlAvatar::matchVolumeTransform() for animesh too). The hold keeps
// the actor's current facing; a later start() walks FROM the mark because
// start() captures the CURRENT rendered pose as its origin.
void LLActorMover::placeAt(const LLUUID& actor_id, const LLVector3& pos)
{
    LLVOAvatar* av = resolve_actor(actor_id);
    if (!av || !av->getRootJoint())
    {
        return;
    }

    // a reset while moving stops the move (and its anim/cadence lock) first
    stop(actor_id);

    LLVector3 at = LLVector3(1.f, 0.f, 0.f) * av->getRenderRotation();

    Move mv;
    mv.mOrigin   = pos;
    mv.mHeading  = atan2f(at.mV[VY], at.mV[VX]);
    mv.mSpeed    = 0.f;         // zero speed + zero distance = pinned hold
    mv.mDistance = 0.f;
    mv.mEndMode  = 0;           // hold
    mv.mT        = 0.f;
    mv.mAnim.setNull();         // no locomotion anim for a hold

    mMoves[av->getID()] = mv;

    LL_INFOS("ActorMover") << "hold at mark: " << av->getID() << " pos " << pos << LL_ENDL;
}

// ---------------------------------------------------------------------------
//static
F32 LLActorMover::pathParam(const Move& mv, F32* face)
{
    // path parameter + travel direction (ping-pong reverses facing on the
    // return leg so the actor walks back rather than moonwalking)
    const F32 total = mv.mSpeed * mv.mT;
    F32 s;
    F32 f = mv.mHeading;
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
                f = mv.mHeading + F_PI;
            }
            break;
        }
        default:    // hold at end
            s = llmin(total, mv.mDistance);
            break;
    }
    if (face)
    {
        *face = f;
    }
    return s;
}

bool LLActorMover::getProgress(const LLUUID& actor_id, F32& traveled, F32& total) const
{
    LLUUID key = actor_id;
    if (key.isNull())
    {
        if (!isAgentAvatarValid())
        {
            return false;
        }
        key = gAgentAvatarp->getID();
    }
    auto it = mMoves.find(key);
    if (it == mMoves.end())
    {
        return false;
    }
    traveled = pathParam(it->second, nullptr);
    total = it->second.mDistance;
    return true;
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

    // advance the path clock only once per frame; later calls in the same
    // frame (e.g. LLControlAvatar::matchVolumeTransform() re-syncing an
    // animesh root to its linkset) just re-assert the cached override pose
    const U32 frame = LLFrameTimer::getFrameCount();
    if (mv.mLastFrame != frame)
    {
        mv.mLastFrame = frame;
        mv.mT += llclamp(gFrameIntervalSeconds.value(), 0.f, 0.25f);

        F32 face;
        const F32 s = pathParam(mv, &face);

        if (mv.mEndMode == 0 && s >= mv.mDistance && av->findMotion(mv.mAnim))
        {
            // arrived: settle into a stand instead of walking in place
            av->stopMotion(mv.mAnim);
            av->setAnimTimeFactor(1.f);
        }

        const LLVector3 dir(cosf(mv.mHeading), sinf(mv.mHeading), 0.f);
        mv.mCurPos = mv.mOrigin + dir * s;
        mv.mCurRot.setEulerAngles(0.f, 0.f, face);
    }

    av->getRootJoint()->setWorldPosition(mv.mCurPos);
    av->getRootJoint()->setWorldRotation(mv.mCurRot);
    return true;
}

// ---------------------------------------------------------------------------
void LLActorMover::renderHeadingPreview()
{
    static LLCachedControl<bool> show(gSavedSettings, "ActorMoverShowHeading", false);
    if (!show)
    {
        return;
    }
    LLFloater* floaterp = LLFloaterReg::findInstance("actor_mover");
    if (!floaterp || !floaterp->getVisible())
    {
        return;
    }

    static LLCachedControl<F32> distance(gSavedSettings, "ActorMoverDistance", 6.f);
    static LLCachedControl<F32> heading(gSavedSettings, "ActorMoverHeading", 0.f);
    const F32 dist = llmax((F32)distance, 0.1f);

    // same beacon-style local debug lines as renderObjectBeacons(): UI shader,
    // no texture, no depth writes -- strictly a client-side overlay
    LLGLSUIDefault gls_ui;
    gUIProgram.bind();
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    gGL.setLineWidth(2.f);
    gGL.begin(LLRender::LINES);
    gGL.color4f(1.f, 0.75f, 0.2f, 0.9f);

    uuid_vec_t roster = getRoster();
    if (roster.empty())
    {
        roster.push_back(LLUUID::null);     // my avatar
    }
    for (const LLUUID& id : roster)
    {
        LLVOAvatar* av = resolve_actor(id);
        if (!av || !av->getRootJoint())
        {
            continue;
        }

        // planned bearing = same math as start(): current facing + trim
        LLVector3 at = LLVector3(1.f, 0.f, 0.f) * av->getRenderRotation();
        const F32 yaw = atan2f(at.mV[VY], at.mV[VX]) + (F32)heading * DEG_TO_RAD;
        const LLVector3 dir(cosf(yaw), sinf(yaw), 0.f);

        LLVector3 origin = av->getRootJoint()->getWorldPosition();
        origin.mV[VZ] += 0.15f - av->getPelvisToFoot();     // just above ground
        const LLVector3 end = origin + dir * dist;

        // main segment (ping-pong/loop share the same single leg)
        gGL.vertex3fv(origin.mV);
        gGL.vertex3fv(end.mV);

        // arrowhead barbs + a small vertical tick at the endpoint
        const F32 barb_len = llmin(0.4f, dist * 0.25f);
        for (F32 side : { 1.f, -1.f })
        {
            const F32 b = yaw + side * 2.6f;    // ~150 deg back from travel
            gGL.vertex3fv(end.mV);
            gGL.vertex3f(end.mV[VX] + barb_len * cosf(b),
                         end.mV[VY] + barb_len * sinf(b),
                         end.mV[VZ]);
        }
        gGL.vertex3fv(end.mV);
        gGL.vertex3f(end.mV[VX], end.mV[VY], end.mV[VZ] + 0.3f);
    }

    gGL.end();
    gGL.setLineWidth(1.f);
    gGL.flush();
}
