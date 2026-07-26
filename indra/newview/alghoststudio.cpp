/**
 * @file alghoststudio.cpp
 * @brief Ghost Studio data model -- see alghoststudio.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alghoststudio.h"

#include "llghostavatar.h"
#include "llactormover.h"           // GhostBatch access for the freeze snapshot
#include "llagent.h"                // agent <-> global conversion
#include "lldirectorcast.h"         // source resolution (cast id / null = self)
#include "llcinematiccamera.h"
#include "lljoint.h"
#include "lldrawable.h"          // FORCE_INVISIBLE (entity-clone show/hide)
#include "llspatialpartition.h"     // LLDrawInfo (freeze reads each batch's avatar+skin)
#include "llvoavatar.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewercamera.h"
#include "llfloaterreg.h"
#include "llviewercontrol.h"
#include "lltimer.h"

#include <algorithm>
#include <cstdint>
#include <cmath>

// ---------------------------------------------------------------------------
ALGhostStudio& ALGhostStudio::instance()
{
    static ALGhostStudio sInstance;
    return sInstance;
}

namespace
{
constexpr S32 ENTITY_PHYSICS_CROWD_THRESHOLD = 20;

S32 entity_clone_count(const std::vector<ALGhostStudio::Instance>& instances)
{
    return (S32)std::count_if(
        instances.begin(), instances.end(),
        [](const ALGhostStudio::Instance& inst)
        {
            return inst.mKind == ALGhostStudio::BACKING_ENTITY_CLONE;
        });
}

// the source's current rendered FOOT position (agent frame); false when the
// source is unresolvable or has no skeleton yet. Same foot convention as the
// path ghosts: root minus pelvisToFoot, so a ghost placed here stands exactly
// where the actor stands.
bool source_foot_agent(const LLUUID& source, LLVector3& out_foot)
{
    LLVOAvatar* av = LLDirectorCast::instance().resolve(source);
    if (!av || !av->getRootJoint())
    {
        return false;
    }
    out_foot = av->getRootJoint()->getWorldPosition();
    out_foot.mV[VZ] -= av->getPelvisToFoot();
    return true;
}

// Show/hide a client-only entity clone (avatar body + its cloned attachment
// objects) WITHOUT killing it: FORCE_INVISIBLE on the avatar drawable, applied
// recursively to attachment children; clearState restores. Entirely viewer-
// local -- no sim traffic. (Entity clones render through the real-avatar path,
// so the old overlay show/hide gate does not cover them.)
void set_entity_clone_visible(LLGhostAvatar* ghost, bool visible)
{
    if (!ghost || ghost->isDead())
    {
        return;
    }
    // Persistent local gate for the avatar pool. FORCE_INVISIBLE alone is not
    // sufficient for already-enrolled rigged faces and can be cleared by
    // ordinary viewer-object update bookkeeping.
    ghost->setEntityCloneVisible(visible);
    if (visible)
    {
        ghost->clearDrawableState(LLDrawable::FORCE_INVISIBLE, true);
    }
    else
    {
        ghost->setDrawableState(LLDrawable::FORCE_INVISIBLE, true);
    }
}

F32 seeded_unit(const LLUUID& id, U32 channel)
{
    // Stable FNV-1a over the instance UUID text plus a channel discriminator.
    // No clock or RNG: the same instance always receives the same variation.
    U32 hash = 2166136261u ^ channel;
    for (const char c : id.asString())
    {
        hash ^= (U8)c;
        hash *= 16777619u;
    }
    return ((hash >> 8) & 0x00ffffffu) / 16777215.f;
}
} // anonymous namespace

ALGhostStudio::ALGhostStudio()
{
    // Runtime UUID consumers enroll here. Replacement and removal always walk
    // this registry, so adding a new consumer cannot require editing every
    // refresh/death path.
    mRuntimeConsumers.emplace_back(
        [this](const LLUUID& stable_id, const LLUUID& old_id,
               const LLUUID& new_id, bool removing_instance)
        {
            LLDirectorCast& cast = LLDirectorCast::instance();
            Instance* inst = getInstance(stable_id);
            const bool subject_a =
                (old_id.notNull() && cast.getSubjectA() == old_id) ||
                (inst && inst->mWasDirectorSubjectA);
            const bool subject_b =
                (old_id.notNull() && cast.getSubjectB() == old_id) ||
                (inst && inst->mWasDirectorSubjectB);
            if (old_id.notNull())
            {
                cast.remove(old_id);
            }
            if (new_id.notNull())
            {
                cast.add(new_id);
            }
            if (subject_a)
            {
                cast.setSubjectA(new_id);
            }
            if (subject_b)
            {
                cast.setSubjectB(new_id);
            }
            if (inst)
            {
                inst->mWasDirectorSubjectA =
                    !removing_instance && subject_a;
                inst->mWasDirectorSubjectB =
                    !removing_instance && subject_b;
            }
        });
    mRuntimeConsumers.emplace_back(
        [this](const LLUUID& stable_id, const LLUUID&, const LLUUID&,
               bool removing_instance)
        {
            // Panel/tool selection is stable-ID based. It survives replacement
            // and is cleared by the same transaction on final removal.
            if (removing_instance && mSelected == stable_id)
            {
                mSelected.setNull();
            }
        });
    mRuntimeConsumers.emplace_back(
        [this](const LLUUID& stable_id, const LLUUID& old_id,
               const LLUUID& new_id, bool removing_instance)
        {
            Instance* inst = getInstance(stable_id);
            const bool followed =
                LLCinematicCamera::isFollowTarget(old_id) ||
                (inst && inst->mWasCinematicFollow);
            if (followed)
            {
                LLCinematicCamera::onRuntimeTargetReplaced(old_id, new_id);
                if (old_id.isNull() && new_id.notNull())
                {
                    LLCinematicCamera::toggleFollowTarget(new_id);
                }
            }
            if (inst)
            {
                inst->mWasCinematicFollow =
                    !removing_instance && followed;
            }
        });
}

LLGhostAvatar* ALGhostStudio::createEntityRuntime(Instance& inst,
                                                  S32& attachments)
{
    LLViewerRegion* region = gAgent.getRegion();
    LLVOAvatar* source = LLDirectorCast::instance().resolve(inst.mSource);
    if (!region || !source)
    {
        return nullptr;
    }

    LLGhostAvatar* ghost = (LLGhostAvatar*)gObjectList.createObjectViewer(
        LL_PCODE_LEGACY_AVATAR, region, LLViewerObject::CO_FLAG_GHOST_AVATAR);
    if (!ghost || !ghost->cloneAppearanceFrom(source))
    {
        if (ghost)
        {
            ghost->markForDeath();
        }
        return nullptr;
    }
    attachments = ghost->cloneAttachmentsFrom(source);
    if (!ghost->clonedAttachmentsComplete())
    {
        ghost->releaseClonedAttachments();
        ghost->markForDeath();
        return nullptr;
    }
    return ghost;
}

void ALGhostStudio::applyEntityRuntimeState(Instance& inst,
                                             LLGhostAvatar* ghost)
{
    ghost->setGhostPosition(gAgent.getPosAgentFromGlobal(inst.mFootGlobal));
    ghost->setGhostRotation(inst.mRotation);
    ghost->setEntityScale(inst.mScale);
    const F32 chaos_factor =
        1.f + (seeded_unit(inst.mId, 5) * 2.f - 1.f) *
                  0.15f * inst.mChaosAmount;
    ghost->setAnimTimeFactor(
        llclamp(inst.mAnimSpeed * chaos_factor, 0.05f, 4.f));
    ghost->setEntityPhysicsEnabled(inst.mPhysicsEnabled);
    ghost->setEntityLoopMode(inst.mLoopMode);
    ghost->setEntityLook(inst.mLook, inst.mLookAlpha);
    set_entity_clone_visible(ghost, mShowAll && inst.mEnabled);

    // A fresh skeleton has no evaluated pose to hold. Let its stored resume
    // mode evaluate first, then finish the requested frozen state from the
    // model update after two viewer frames.
    if (inst.mDriveMode == DRIVE_FROZEN)
    {
        const EDriveMode resume = inst.mResumeDriveMode == DRIVE_FROZEN
            ? DRIVE_MIRROR : inst.mResumeDriveMode;
        ghost->setEntityDriveMode(resume, inst.mResumeDirectedAnim);
        inst.mPendingFreezeFrames = 2;
    }
    else
    {
        ghost->setEntityDriveMode(inst.mDriveMode, inst.mDirectedAnim);
        inst.mPendingFreezeFrames = 0;
    }
}

void ALGhostStudio::onEntityRuntimeReplaced(Instance& inst,
                                             const LLUUID& new_runtime,
                                             LLGhostAvatar* new_ghost,
                                             bool removing_instance)
{
    const LLUUID old_runtime = inst.mEntityId;
    if (new_ghost)
    {
        applyEntityRuntimeState(inst, new_ghost);
    }
    inst.mEntityId = new_runtime;
    for (const runtime_consumer_t& consumer : mRuntimeConsumers)
    {
        consumer(inst.mId, old_runtime, new_runtime, removing_instance);
    }
    if (new_runtime.notNull())
    {
        if (LLDirectorCast::CastMember* member =
                LLDirectorCast::instance().getMember(new_runtime))
        {
            member->mLastName = inst.mName;
        }
    }
}

void ALGhostStudio::finishPendingRuntimeFreezes()
{
    for (Instance& inst : mInstances)
    {
        if (!inst.mPendingFreezeFrames)
        {
            continue;
        }
        LLGhostAvatar* ghost = resolveEntityClone(inst.mId);
        if (!ghost)
        {
            inst.mPendingFreezeFrames = 0;
            continue;
        }
        if (--inst.mPendingFreezeFrames == 0)
        {
            ghost->setEntityDriveMode(DRIVE_FROZEN, LLUUID::null);
        }
    }
}

// ---------------------------------------------------------------------------
// master visibility
// ---------------------------------------------------------------------------
void ALGhostStudio::setShowAll(bool on)
{
    assert_main_thread();
    mShowAll = on;
    for (const Instance& inst : mInstances)
    {
        if (inst.mKind == BACKING_ENTITY_CLONE)
        {
            LLGhostAvatar* ghost =
                dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(inst.mEntityId));
            if (ghost && !ghost->isDead())
            {
                set_entity_clone_visible(ghost, on && inst.mEnabled);
            }
        }
    }
}

bool ALGhostStudio::anyEnabled() const
{
    if (!mShowAll)
    {
        return false;
    }
    for (const Instance& inst : mInstances)
    {
        if (inst.mKind == BACKING_OVERLAY && inst.mEnabled)
        {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// instance CRUD
// ---------------------------------------------------------------------------
std::string ALGhostStudio::makeDefaultName() const
{
    for (S32 number = 1; ; ++number)
    {
        const std::string candidate = llformat("Ghost %d", number);
        const bool used = std::any_of(mInstances.begin(), mInstances.end(),
            [&candidate](const Instance& inst) { return inst.mName == candidate; });
        if (!used)
        {
            return candidate;
        }
    }
}

ALGhostStudio::Instance* ALGhostStudio::addInstance(const LLUUID& source)
{
    LLVector3 foot;
    if (!source_foot_agent(source, foot))
    {
        return nullptr;     // source not in world: nowhere sensible to spawn
    }
    Instance inst;
    inst.mId.generate();
    inst.mName = makeDefaultName();
    inst.mSource = source;
    inst.mFootGlobal = gAgent.getPosGlobalFromAgent(foot);
    mInstances.push_back(inst);
    return &mInstances.back();
}

ALGhostStudio::Instance* ALGhostStudio::spawnEntityClone(
    const LLUUID& source_id, const std::string& source_label)
{
    assert_main_thread();
    LLVOAvatar* source = LLDirectorCast::instance().resolve(source_id);
    if (!gAgent.getRegion() || !source)
    {
        LL_WARNS("GhostStudio") << "entity spawn: source/region unavailable" << LL_ENDL;
        return nullptr;
    }

    Instance record;
    record.mId.generate();
    record.mName = makeDefaultName();
    record.mSource = source_id;
    record.mSourceLabel = source_label;
    record.mKind = BACKING_ENTITY_CLONE;
    // Physics is useful for hero clones but disproportionately costly for
    // crowds. At the threshold, new clones start off and remain opt-in.
    record.mPhysicsEnabled =
        entity_clone_count(mInstances) < ENTITY_PHYSICS_CROWD_THRESHOLD;
    record.mState = STATE_SPAWNING;
    record.mStyle = 1; // informational: normal scene-lit clone
    const LLVector3 pos = source->getPositionAgent() + gAgent.getAtAxis() * 2.5f;
    record.mFootGlobal = gAgent.getPosGlobalFromAgent(pos);
    record.mRotation = source->getRotation();
    mInstances.push_back(record);
    const LLUUID stable_id = record.mId;

    Instance* committed = getInstance(stable_id);
    S32 attachments = 0;
    LLGhostAvatar* ghost = committed
        ? createEntityRuntime(*committed, attachments) : nullptr;
    if (!ghost)
    {
        removeInstance(stable_id);
        LL_WARNS("GhostStudio") << "entity spawn rolled back" << LL_ENDL;
        return nullptr;
    }

    onEntityRuntimeReplaced(*committed, ghost->getID(), ghost);
    committed->mState = STATE_READY;
    LL_INFOS("GhostStudio") << "entity instance " << stable_id
                            << " committed runtime=" << committed->mEntityId
                            << " attachments=" << attachments << LL_ENDL;
    return committed;
}

// Re-pull the source avatar's CURRENT appearance + worn attachments onto an
// entity clone (e.g. after the source changed outfit/shape), keeping the
// instance id, placement, scale, look and drive mode. Build a replacement
// client-only avatar transactionally so a failed attachment copy leaves the
// currently-live clone untouched. Nothing is sent to the sim.
bool ALGhostStudio::refreshEntityClone(const LLUUID& id)
{
    assert_main_thread();
    Instance* inst = getInstance(id);
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE)
    {
        return false;
    }
    LLVOAvatar* source = LLDirectorCast::instance().resolve(inst->mSource);
    LLGhostAvatar* old_ghost = resolveEntityClone(id);
    if (!source || !gAgent.getRegion())
    {
        inst->mState = old_ghost ? STATE_SOURCE_MISSING : STATE_RECOVERABLE;
        LL_WARNS("GhostStudio") << "refreshEntityClone: ghost or source unavailable for "
                                << id << LL_ENDL;
        return false;
    }

    S32 attachments = 0;
    LLGhostAvatar* replacement = createEntityRuntime(*inst, attachments);
    if (!replacement)
    {
        inst->mState = old_ghost ? STATE_ERROR : STATE_RECOVERABLE;
        LL_WARNS("GhostStudio")
            << "refreshEntityClone: replacement attachment clone incomplete for "
            << id << "; existing clone preserved" << LL_ENDL;
        return false;
    }

    const LLUUID old_runtime_id = inst->mEntityId;
    onEntityRuntimeReplaced(*inst, replacement->getID(), replacement);
    if (old_ghost)
    {
        set_entity_clone_visible(old_ghost, false);
        old_ghost->releaseClonedAttachments();
        old_ghost->markForDeath();
    }

    inst->mState = STATE_READY;
    LL_INFOS("GhostStudio") << "refreshEntityClone: replaced entity clone " << id
                            << " runtime=" << inst->mEntityId
                            << " attachments=" << attachments << LL_ENDL;
    return true;
}

ALGhostStudio::Instance* ALGhostStudio::duplicateInstance(const LLUUID& id)
{
    Instance* src = getInstance(id);
    if (!src || src->mKind == BACKING_ENTITY_CLONE)
    {
        return nullptr;
    }
    Instance copy = *src;       // includes any frozen palette snapshot
    copy.mId.generate();
    copy.mName = makeDefaultName();
    // one step to the ghost's LEFT (perpendicular to its yaw) so the copy is
    // immediately visible beside the original instead of hidden inside it
    const F32 yaw = copy.getYaw();
    copy.mFootGlobal += LLVector3d(-sinf(yaw), cosf(yaw), 0.0);
    mInstances.push_back(copy);
    return &mInstances.back();
}

ALGhostStudio::Instance* ALGhostStudio::duplicateInstanceInPlace(const LLUUID& id)
{
    Instance* src = getInstance(id);
    if (!src)
    {
        return nullptr;
    }
    const Instance proto = *src;
    if (proto.mKind == BACKING_OVERLAY)
    {
        Instance copy = proto;
        copy.mId.generate();
        copy.mName = makeDefaultName();
        mInstances.push_back(copy);
        return &mInstances.back();
    }

    // Entity records cannot share mEntityId: create a fresh local clone, then
    // transfer the authored instance state while preserving its runtime keys.
    Instance* copy = spawnEntityClone(proto.mSource, proto.mSourceLabel);
    if (!copy)
    {
        return nullptr;
    }
    const LLUUID new_id = copy->mId;
    const LLUUID entity_id = copy->mEntityId;
    const ELifecycleState state = copy->mState;
    const std::string new_name = copy->mName; // spawn already called makeDefaultName()
    *copy = proto;
    copy->mId = new_id;
    copy->mEntityId = entity_id;
    copy->mState = state;
    copy->mName = new_name;
    if (entity_clone_count(mInstances) > ENTITY_PHYSICS_CROWD_THRESHOLD)
    {
        copy->mPhysicsEnabled = false;
    }
    copy->mWasDirectorSubjectA = false;
    copy->mWasDirectorSubjectB = false;
    copy->mWasCinematicFollow = false;
    copy->setTransform(proto.mFootGlobal, proto.mRotation);
    copy->setScale(proto.mScale);
    if (LLGhostAvatar* ghost = resolveEntityClone(copy->mId))
    {
        applyEntityRuntimeState(*copy, ghost);
    }
    return copy;
}

bool ALGhostStudio::renameInstance(const LLUUID& id, const std::string& name)
{
    Instance* inst = getInstance(id);
    if (!inst)
    {
        return false;
    }
    inst->mName = name; // Deliberately no uniqueness check: mId remains identity.
    if (inst->mKind == BACKING_ENTITY_CLONE && inst->mEntityId.notNull())
    {
        if (LLDirectorCast::CastMember* member =
                LLDirectorCast::instance().getMember(inst->mEntityId))
        {
            member->mLastName = name;
        }
    }
    return true;
}

bool ALGhostStudio::setInstanceEnabled(const LLUUID& id, bool enabled)
{
    assert_main_thread();
    Instance* inst = getInstance(id);
    if (!inst || inst->mState == STATE_TEARING_DOWN)
    {
        return false;
    }
    inst->mEnabled = enabled;
    if (inst->mKind == BACKING_ENTITY_CLONE)
    {
        LLGhostAvatar* ghost =
            dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(inst->mEntityId));
        if (!ghost || ghost->isDead())
        {
            onEntityRuntimeReplaced(*inst, LLUUID::null);
            inst->mState = STATE_RECOVERABLE;
            return false;
        }
        set_entity_clone_visible(ghost, mShowAll && enabled);
    }
    return true;
}

bool ALGhostStudio::setInstanceScale(const LLUUID& id, F32 scale)
{
    assert_main_thread();
    Instance* inst = getInstance(id);
    if (!inst)
    {
        return false;
    }
    scale = llclamp(scale, 0.05f, 10.f);
    inst->setScale(scale);
    if (inst->mKind == BACKING_ENTITY_CLONE)
    {
        LLGhostAvatar* ghost =
            dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(inst->mEntityId));
        if (!ghost || ghost->isDead())
        {
            inst->mState = STATE_ERROR;
            return false;
        }
        ghost->setEntityScale(scale);
    }
    return true;
}

bool ALGhostStudio::setInstanceAnimSpeed(const LLUUID& id, F32 speed)
{
    assert_main_thread();
    Instance* inst = getInstance(id);
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE)
    {
        return false;
    }
    inst->mAnimSpeed = llclamp(speed, 0.05f, 4.f);
    LLGhostAvatar* ghost = resolveEntityClone(id);
    if (!ghost)
    {
        inst->mState = STATE_ERROR;
        return false;
    }
    const F32 chaos_factor =
        1.f + (seeded_unit(inst->mId, 5) * 2.f - 1.f) *
                  0.15f * inst->mChaosAmount;
    ghost->setAnimTimeFactor(
        llclamp(inst->mAnimSpeed * chaos_factor, 0.05f, 4.f));
    return true;
}

bool ALGhostStudio::setInstancePhysicsEnabled(const LLUUID& id, bool enabled)
{
    assert_main_thread();
    Instance* inst = getInstance(id);
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE)
    {
        return false;
    }
    inst->mPhysicsEnabled = enabled;
    LLGhostAvatar* ghost = resolveEntityClone(id);
    if (!ghost)
    {
        inst->mState = STATE_ERROR;
        return false;
    }
    ghost->setEntityPhysicsEnabled(enabled);
    return true;
}

bool ALGhostStudio::setInstancePaused(const LLUUID& id, bool paused)
{
    Instance* inst = getInstance(id);
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE)
    {
        return false;
    }
    if (paused)
    {
        if (inst->mDriveMode != DRIVE_FROZEN)
        {
            inst->mResumeDriveMode = inst->mDriveMode;
            inst->mResumeDirectedAnim = inst->mDirectedAnim;
        }
        return setInstanceDriveMode(id, DRIVE_FROZEN);
    }
    if (inst->mDriveMode != DRIVE_FROZEN)
    {
        return true;
    }
    const EDriveMode resume_mode = inst->mResumeDriveMode == DRIVE_FROZEN
        ? DRIVE_MIRROR : inst->mResumeDriveMode;
    const bool restart = inst->mRestartOnResume;
    inst->mRestartOnResume = false;
    const bool resumed =
        setInstanceDriveMode(id, resume_mode, inst->mResumeDirectedAnim);
    if (resumed && restart)
    {
        restartInstanceAnimation(id);
    }
    return resumed;
}

bool ALGhostStudio::setInstanceLoopMode(const LLUUID& id, ELoopMode mode)
{
    Instance* inst = getInstance(id);
    LLGhostAvatar* ghost = inst ? resolveEntityClone(id) : nullptr;
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE || !ghost)
    {
        return false;
    }
    inst->mLoopMode = (ELoopMode)llclamp((S32)mode,
        (S32)LOOP_RETRIGGER, (S32)LOOP_PLAY_ONCE);
    ghost->setEntityLoopMode(inst->mLoopMode);
    return true;
}

bool ALGhostStudio::restartInstanceAnimation(const LLUUID& id)
{
    Instance* inst = getInstance(id);
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE)
    {
        return false;
    }
    if (inst->mDriveMode == DRIVE_FROZEN)
    {
        inst->mRestartOnResume = true;
        return true;
    }
    LLGhostAvatar* ghost = resolveEntityClone(id);
    if (!ghost)
    {
        return false;
    }
    ghost->restartEntityAnimation();
    return true;
}

LLGhostAvatar* ALGhostStudio::resolveEntityClone(const LLUUID& id) const
{
    const Instance* match = nullptr;
    for (const Instance& inst : mInstances)
    {
        if (inst.mKind == BACKING_ENTITY_CLONE &&
            (inst.mId == id || inst.mEntityId == id))
        {
            match = &inst;
            break;
        }
    }
    if (!match)
    {
        return nullptr;
    }
    LLGhostAvatar* ghost =
        dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(match->mEntityId));
    return ghost && !ghost->isDead() ? ghost : nullptr;
}

bool ALGhostStudio::applyEntityTransform(const LLUUID& id)
{
    assert_main_thread();
    Instance* inst = getInstance(id);
    LLGhostAvatar* ghost = inst ? resolveEntityClone(id) : nullptr;
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE || !ghost)
    {
        return false;
    }
    ghost->setGhostPosition(gAgent.getPosAgentFromGlobal(inst->mFootGlobal));
    ghost->setGhostRotation(inst->mRotation);
    return true;
}

bool ALGhostStudio::aimInstanceAt(const LLUUID& id,
                                  const LLVector3d& target_global)
{
    assert_main_thread();
    Instance* inst = getInstance(id);
    if (!inst)
    {
        return false;
    }
    const LLVector3d delta = target_global - inst->mFootGlobal;
    if (delta.mdV[VX] * delta.mdV[VX] + delta.mdV[VY] * delta.mdV[VY] < 0.000001)
    {
        return false; // coincident horizontally: retain the last meaningful yaw
    }
    inst->setYaw(atan2f((F32)delta.mdV[VY], (F32)delta.mdV[VX]));
    return inst->mKind != BACKING_ENTITY_CLONE || applyEntityTransform(id);
}

bool ALGhostStudio::resolveTargetGlobal(const Instance& inst, ELookTarget target,
                                        const LLUUID& target_id,
                                        const LLVector3d& point_global,
                                        LLVector3d& out) const
{
    // ONE resolver for both look-at and body-turn. They stay independent by
    // each passing their OWN target fields, not by having separate code.
    switch (target)
    {
    case LOOK_TARGET_CAMERA:
        out = gAgent.getPosGlobalFromAgent(
            LLViewerCamera::getInstance()->getOrigin());
        return true;
    case LOOK_TARGET_ME:
    case LOOK_TARGET_ACTOR:
    {
        LLVector3 foot;
        const LLUUID actor = (target == LOOK_TARGET_ME) ? LLUUID::null : target_id;
        if (!source_foot_agent(actor, foot))
        {
            return false;
        }
        out = gAgent.getPosGlobalFromAgent(foot);
        return true;
    }
    case LOOK_TARGET_GHOST:
    {
        const Instance* other = getInstance(target_id);
        if (!other || other->mId == inst.mId)
        {
            return false;
        }
        out = other->mFootGlobal;
        return true;
    }
    case LOOK_TARGET_POINT:
        // A bare world position. The TARGET TYPE is the validity signal -- a
        // global (0,0,0) is a legitimate coordinate and is NOT the same thing
        // as a region origin, so rejecting it would silently drop a valid shot.
        out = point_global;
        return true;
    default:
        return false;
    }
}

bool ALGhostStudio::faceInstance(const LLUUID& id)
{
    Instance* inst = getInstance(id);
    if (!inst)
    {
        return false;
    }
    // Turn owns the body yaw when armed. Enforced HERE, at the one function
    // that aims the body at the look target, so every caller is covered --
    // the per-frame loops, "Face now", the keep-facing checkbox and
    // /ghostlook -- instead of each remembering the rule.
    if (inst->mTurnMode != TURN_MODE_OFF)
    {
        return false;
    }
    LLVector3d target;
    if (!resolveTargetGlobal(*inst, inst->mLookTarget, inst->mLookTargetId,
                             inst->mLookPointGlobal, target))
    {
        return false;
    }
    return aimInstanceAt(id, target);
}

void ALGhostStudio::setTurnTarget(const LLUUID& id, ETurnMode mode,
                                  ELookTarget target, const LLUUID& target_id,
                                  const LLVector3d& point_global)
{
    if (Instance* inst = getInstance(id))
    {
        inst->mTurnMode = mode;
        inst->mTurnTarget = target;
        inst->mTurnTargetId = target_id;
        inst->mTurnPointGlobal = point_global;
        // Re-arm: a fresh target must turn even if a previous ONCE had settled.
        inst->mTurnSettled = false;
    }
}

bool ALGhostStudio::stepTurn(const LLUUID& id, F32 dt)
{
    Instance* inst = getInstance(id);
    if (!inst || inst->mTurnMode == TURN_MODE_OFF)
    {
        return false;
    }
    // ONCE stops asking once it has arrived, so hand-rotating the clone
    // afterwards is not immediately undone on the next frame.
    if (inst->mTurnMode == TURN_MODE_ONCE && inst->mTurnSettled)
    {
        // Re-arm if the clone has MOVED since it settled: the bearing to the
        // target is then different and the old facing is stale. Detected here
        // rather than in the setters because formation motion, chaos and the
        // overlay duplication paths all write mFootGlobal directly.
        if (inst->mFootGlobal == inst->mTurnSettledFoot)
        {
            return false;
        }
        inst->mTurnSettled = false;
    }

    LLVector3d target;
    if (!resolveTargetGlobal(*inst, inst->mTurnTarget, inst->mTurnTargetId,
                             inst->mTurnPointGlobal, target))
    {
        return false;
    }
    const LLVector3d delta = target - inst->mFootGlobal;
    if (delta.mdV[VX] * delta.mdV[VX] + delta.mdV[VY] * delta.mdV[VY] < 0.000001)
    {
        return false;   // coincident horizontally: keep the last meaningful yaw
    }

    static LLCachedControl<F32> turn_rate(gSavedSettings, "GhostStudioTurnRate", 180.f);
    static LLCachedControl<F32> turn_ease(gSavedSettings, "GhostStudioTurnEase", 0.25f);

    const F32 want = atan2f((F32)delta.mdV[VY], (F32)delta.mdV[VX]);
    const F32 have = inst->getYaw();
    // Shortest signed arc, so a turn never takes the long way round.
    F32 diff = want - have;
    while (diff >  F_PI) diff -= F_TWO_PI;
    while (diff < -F_PI) diff += F_TWO_PI;

    if (fabsf(diff) < 0.0015f)          // ~0.086 deg
    {
        inst->setYaw(want);
        inst->mTurnSettled = true;
        inst->mTurnSettledFoot = inst->mFootGlobal;
        return inst->mKind != BACKING_ENTITY_CLONE || applyEntityTransform(id);
    }

    // Rate limit, then ease IN toward the target over the last stretch so the
    // clone decelerates into frame instead of stopping dead -- a hard stop is
    // the thing that reads as a bug.
    // A configured rate of 0 means STOPPED, not "creep at 1 deg/s".
    const F32 rate = llmax(0.f, (F32)turn_rate);
    if (rate <= 0.f)
    {
        return false;
    }
    const F32 max_step = rate * DEG_TO_RAD * llclamp(dt, 0.f, 0.25f);
    const F32 ease_band = llmax(0.01f, (F32)turn_ease);
    const F32 eased = llmin(1.f, fabsf(diff) / ease_band);
    F32 step = llmin(max_step * eased, fabsf(diff));
    inst->setYaw(have + (diff > 0.f ? step : -step));
    return inst->mKind != BACKING_ENTITY_CLONE || applyEntityTransform(id);
}

void ALGhostStudio::setLookTarget(const LLUUID& id, ELookTarget target,
                                  const LLUUID& target_id, bool keep_facing)
{
    if (Instance* inst = getInstance(id))
    {
        inst->mLookTarget = target;
        inst->mLookTargetId = target_id;
        inst->mKeepFacing = keep_facing;
    }
}

void ALGhostStudio::updateLookAt()
{
    assert_main_thread();
    // faceInstance does not mutate the vector, but setYaw bumps transform
    // revisions; IDs keep this loop robust if implementation later changes.
    std::vector<LLUUID> tracking;
    for (const Instance& inst : mInstances)
    {
        // Turn owns the body when it is armed. Both write mRotation, so running
        // keep-facing as well would just be overwritten by stepTurn a moment
        // later -- worse, it would fight it every frame.
        if (inst.mKeepFacing && inst.mTurnMode == TURN_MODE_OFF)
        {
            tracking.push_back(inst.mId);
        }
    }
    for (const LLUUID& id : tracking)
    {
        faceInstance(id);
    }
}

void ALGhostStudio::updatePerFrame()
{
    assert_main_thread();
    refreshLifecycleStates();
    finishPendingRuntimeFreezes();
    if (mFreezeStrips.empty() && !mHasMotion)
    {
        updateLookAt();
        stepAllTurns();
        return;
    }
    const F64 now = LLTimer::getTotalSeconds();
    if (!mFreezeStrips.empty())
    {
        updateFreezeStrips(now);
    }
    if (mHasMotion)
    {
        updateFormationMotion(now);
    }

    // Precedence: procedural motion owns position. Spin owns yaw; otherwise
    // keep-facing is last and owns yaw. Chaos is folded into motion below as a
    // stable additive variation, so the two never overwrite each other.
    std::vector<LLUUID> tracking;
    for (const Instance& inst : mInstances)
    {
        if (inst.mKeepFacing && inst.mMotion != MOTION_SPIN
            && inst.mTurnMode == TURN_MODE_OFF)
        {
            tracking.push_back(inst.mId);
        }
    }
    for (const LLUUID& id : tracking)
    {
        faceInstance(id);
    }
    stepAllTurns();
}

void ALGhostStudio::stepAllTurns()
{
    // Body turn runs AFTER look-at and after motion, so it is the last word on
    // yaw for any instance that has a turn target -- except SPIN, which owns
    // yaw outright and would otherwise fight it every frame.
    const F32 dt = gFrameIntervalSeconds;
    std::vector<LLUUID> turning;
    for (const Instance& inst : mInstances)
    {
        if (inst.mTurnMode != TURN_MODE_OFF && inst.mMotion != MOTION_SPIN)
        {
            turning.push_back(inst.mId);
        }
    }
    for (const LLUUID& id : turning)
    {
        stepTurn(id, dt);
    }
}

S32 ALGhostStudio::setFormationMotion(const std::vector<LLUUID>& ids,
                                      EMotion motion, F32 speed, F32 amplitude)
{
    assert_main_thread();
    LLVector3d centre;
    S32 valid = 0;
    for (const LLUUID& id : ids)
    {
        if (Instance* inst = getInstance(id))
        {
            const LLVector3d foot = inst->mMotionHasBase
                ? inst->mMotionBaseFoot
                : (inst->mChaosHasBase ? inst->mChaosBaseFoot : inst->mFootGlobal);
            centre += foot;
            ++valid;
        }
    }
    if (!valid) return 0;
    centre /= (F64)valid;
    const F64 now = LLTimer::getTotalSeconds();
    S32 slot = 0;
    for (const LLUUID& id : ids)
    {
        Instance* inst = getInstance(id);
        if (!inst) continue;
        if (motion == MOTION_OFF)
        {
            if (inst->mMotionHasBase)
            {
                inst->mFootGlobal = inst->mMotionBaseFoot;
                inst->mRotation = inst->mMotionBaseRotation;
                inst->mScale = inst->mMotionBaseScale;
                inst->mMotionHasBase = false;
                inst->mMotion = MOTION_OFF;
                ++inst->mTransformRevision;
                if (inst->mChaosEnabled)
                {
                    setInstanceChaos(id, inst->mChaosAmount);
                    continue;
                }
                if (inst->mKind == BACKING_ENTITY_CLONE)
                {
                    if (LLGhostAvatar* ghost = resolveEntityClone(id))
                        ghost->setEntityScale(inst->mScale);
                    applyEntityTransform(id);
                }
            }
            continue;
        }
        if (!inst->mMotionHasBase)
        {
            inst->mMotionBaseFoot = inst->mChaosHasBase
                ? inst->mChaosBaseFoot : inst->mFootGlobal;
            inst->mMotionBaseRotation = inst->mChaosHasBase
                ? inst->mChaosBaseRotation : inst->mRotation;
            inst->mMotionBaseScale = inst->mChaosHasBase
                ? inst->mChaosBaseScale : inst->mScale;
            inst->mMotionHasBase = true;
        }
        inst->mMotion = motion;
        inst->mMotionSpeed = llclamp(speed, 0.f, 10.f);
        inst->mMotionAmplitude = llclamp(amplitude, 0.f, 100.f);
        inst->mMotionCentre = centre;
        inst->mMotionSlot = slot++;
        inst->mMotionStart = now;
    }
    mHasMotion = std::any_of(mInstances.begin(), mInstances.end(),
        [](const Instance& i) { return i.mMotion != MOTION_OFF; });
    return valid;
}

void ALGhostStudio::updateFormationMotion(F64 now)
{
    mHasMotion = false;
    for (Instance& inst : mInstances)
    {
        if (inst.mMotion == MOTION_OFF || !inst.mMotionHasBase) continue;
        mHasMotion = true;
        const F32 phase = F_TWO_PI * inst.mMotionSpeed *
                          (F32)(now - inst.mMotionStart);
        const LLVector3d radial = inst.mMotionBaseFoot - inst.mMotionCentre;
        inst.mFootGlobal = inst.mMotionBaseFoot;
        inst.mRotation = inst.mMotionBaseRotation;
        inst.mScale = inst.mMotionBaseScale;
        if (inst.mMotion == MOTION_ORBIT)
        {
            const F32 a = phase * inst.mMotionAmplitude;
            const F64 c = cosf(a), s = sinf(a);
            inst.mFootGlobal = inst.mMotionCentre +
                LLVector3d(radial.mdV[VX] * c - radial.mdV[VY] * s,
                           radial.mdV[VX] * s + radial.mdV[VY] * c,
                           radial.mdV[VZ]);
        }
        else if (inst.mMotion == MOTION_SPIN)
        {
            inst.mRotation = LLQuaternion(phase * inst.mMotionAmplitude,
                LLVector3::z_axis) * inst.mMotionBaseRotation;
        }
        else if (inst.mMotion == MOTION_BREATHE)
        {
            inst.mFootGlobal = inst.mMotionCentre +
                radial * (F64)(1.f + inst.mMotionAmplitude * sinf(phase));
        }
        else if (inst.mMotion == MOTION_RIPPLE)
        {
            inst.mFootGlobal.mdV[VZ] += inst.mMotionAmplitude *
                sinf(phase - (F32)inst.mMotionSlot * 0.7f);
        }
        if (inst.mChaosEnabled)
        {
            inst.mFootGlobal += LLVector3d(
                (seeded_unit(inst.mId, 3) * 2.f - 1.f) * 0.35f * inst.mChaosAmount,
                (seeded_unit(inst.mId, 4) * 2.f - 1.f) * 0.35f * inst.mChaosAmount, 0.0);
            if (inst.mMotion != MOTION_SPIN)
            {
                const F32 yaw = (seeded_unit(inst.mId, 1) * 2.f - 1.f) *
                                15.f * DEG_TO_RAD * inst.mChaosAmount;
                inst.mRotation = LLQuaternion(yaw, LLVector3::z_axis) * inst.mRotation;
            }
            inst.mScale = llclamp(inst.mMotionBaseScale *
                (1.f + (seeded_unit(inst.mId, 2) * 2.f - 1.f) *
                 0.12f * inst.mChaosAmount), 0.05f, 10.f);
        }
        if (inst.mKind == BACKING_ENTITY_CLONE)
        {
            if (LLGhostAvatar* ghost = resolveEntityClone(inst.mId))
                ghost->setEntityScale(inst.mScale);
            applyEntityTransform(inst.mId);
        }
    }
}

bool ALGhostStudio::setInstanceChaos(const LLUUID& id, F32 amount)
{
    Instance* inst = getInstance(id);
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE)
    {
        return false;
    }
    amount = llclamp(amount, 0.f, 1.f);
    if (!inst->mChaosHasBase)
    {
        inst->mChaosBaseFoot = inst->mFootGlobal;
        inst->mChaosBaseRotation = inst->mRotation;
        inst->mChaosBaseScale = inst->mScale;
        inst->mChaosHasBase = true;
    }

    inst->mChaosEnabled = amount > 0.f;
    inst->mChaosAmount = amount;
    const F32 yaw = (seeded_unit(inst->mId, 1) * 2.f - 1.f) *
                    (15.f * DEG_TO_RAD) * amount;
    const F32 scale_factor = 1.f + (seeded_unit(inst->mId, 2) * 2.f - 1.f) *
                                   0.12f * amount;
    const LLVector3d jitter(
        (seeded_unit(inst->mId, 3) * 2.f - 1.f) * 0.35f * amount,
        (seeded_unit(inst->mId, 4) * 2.f - 1.f) * 0.35f * amount,
        0.0);
    inst->mFootGlobal = inst->mChaosBaseFoot + jitter;
    inst->mRotation = LLQuaternion(yaw, LLVector3::z_axis) * inst->mChaosBaseRotation;
    inst->mScale = llclamp(inst->mChaosBaseScale * scale_factor, 0.05f, 10.f);
    ++inst->mTransformRevision;

    LLGhostAvatar* ghost = resolveEntityClone(id);
    if (!ghost)
    {
        return false;
    }
    ghost->setEntityScale(inst->mScale);
    const bool transformed = applyEntityTransform(id);
    setInstanceAnimSpeed(id, inst->mAnimSpeed);
    return transformed;
}

bool ALGhostStudio::setInstanceLook(const LLUUID& id, EGhostLook look)
{
    Instance* inst = getInstance(id);
    LLGhostAvatar* ghost = inst ? resolveEntityClone(id) : nullptr;
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE || !ghost)
    {
        return false;
    }
    inst->mLook = look;
    ghost->setEntityLook(look, inst->mLookAlpha);
    return true;
}

bool ALGhostStudio::setInstanceDriveMode(const LLUUID& id, EDriveMode mode,
                                         const LLUUID& directed_anim)
{
    assert_main_thread();
    Instance* inst = getInstance(id);
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE)
    {
        return false;
    }
    LLGhostAvatar* ghost =
        dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(inst->mEntityId));
    if (!ghost || ghost->isDead())
    {
        inst->mState = STATE_ERROR;
        return false;
    }
    inst->mDriveMode = mode;
    inst->mDirectedAnim = mode == DRIVE_DIRECTED ? directed_anim : LLUUID::null;
    ghost->setEntityDriveMode(mode, inst->mDirectedAnim);
    ghost->setEntityLoopMode(inst->mLoopMode);
    return true;
}

void ALGhostStudio::refreshLifecycleStates()
{
    assert_main_thread();
    for (Instance& inst : mInstances)
    {
        if (inst.mKind != BACKING_ENTITY_CLONE ||
            inst.mState == STATE_SPAWNING || inst.mState == STATE_TEARING_DOWN ||
            inst.mState == STATE_LOCKED)
        {
            continue;
        }
        LLGhostAvatar* ghost =
            dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(inst.mEntityId));
        if (!ghost || ghost->isDead())
        {
            if (inst.mEntityId.notNull())
            {
                onEntityRuntimeReplaced(inst, LLUUID::null);
            }
            inst.mPendingFreezeFrames = 0;
            inst.mState = STATE_RECOVERABLE;
        }
        else
        {
            inst.mState = LLDirectorCast::instance().resolve(inst.mSource)
                ? STATE_READY : STATE_SOURCE_MISSING;
        }
    }
}

void ALGhostStudio::removeInstance(const LLUUID& id)
{
    assert_main_thread();
    Instance* inst = getInstance(id);
    if (!inst)
    {
        return;
    }
    inst->mState = STATE_TEARING_DOWN;
    inst->mEnabled = false;
    if (inst->mKind == BACKING_ENTITY_CLONE)
    {
        const LLUUID runtime_id = inst->mEntityId;
        LLGhostAvatar* ghost =
            dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(runtime_id));
        onEntityRuntimeReplaced(*inst, LLUUID::null, nullptr, true);
        if (ghost && !ghost->isDead())
        {
            set_entity_clone_visible(ghost, false);
            ghost->releaseClonedAttachments();
            ghost->markForDeath();
        }
    }
    else if (mSelected == id)
    {
        // Overlay instances have no runtime transaction.
        mSelected.setNull();
    }
    mInstances.erase(
        std::remove_if(mInstances.begin(), mInstances.end(),
                       [&id](const Instance& i) { return i.mId == id; }),
        mInstances.end());
}

void ALGhostStudio::removeAll()
{
    while (!mInstances.empty())
    {
        removeInstance(mInstances.back().mId);
    }
}

S32 ALGhostStudio::removeEntityClones()
{
    std::vector<LLUUID> ids;
    for (const Instance& inst : mInstances)
    {
        if (inst.mKind == BACKING_ENTITY_CLONE)
        {
            ids.push_back(inst.mId);
        }
    }
    for (const LLUUID& id : ids)
    {
        removeInstance(id);
    }
    return (S32)ids.size();
}

ALGhostStudio::Instance* ALGhostStudio::getInstance(const LLUUID& id)
{
    for (Instance& inst : mInstances)
    {
        if (inst.mId == id)
        {
            return &inst;
        }
    }
    return nullptr;
}

const ALGhostStudio::Instance* ALGhostStudio::getInstance(const LLUUID& id) const
{
    // Same lookup, const-correct. Delegating through a const_cast keeps ONE
    // implementation rather than two that can drift.
    return const_cast<ALGhostStudio*>(this)->getInstance(id);
}

// ---------------------------------------------------------------------------
// FROZEN pose (the out-of-sync feature)
// ---------------------------------------------------------------------------
bool ALGhostStudio::freezeInstance(const LLUUID& id)
{
    Instance* inst = getInstance(id);
    if (!inst)
    {
        return false;
    }
    LLVOAvatar* av = LLDirectorCast::instance().resolve(inst->mSource);
    if (!av || !av->getRootJoint())
    {
        return false;
    }
    // the frame's collected ghost batches for this wearer are the exact set
    // the ghost draw will iterate, so snapshotting THEIR (drawing avatar,
    // skin hash) palettes guarantees the frozen upload finds a matching,
    // count-correct palette per batch -- including animesh attachments, whose
    // drawing avatar is the attachment's own control avatar
    const std::vector<LLActorMover::GhostBatch>* batches =
        LLActorMover::instance().ghostBatchesFor(av->getID());
    if (!batches || batches->empty())
    {
        return false;   // the pipeline is not rendering this source this frame
    }

    palette_map_t frozen;
    for (const LLActorMover::GhostBatch& gb : *batches)
    {
        LLVOAvatar* draw_av = gb.mInfo->mAvatar.get();
        LLMeshSkinInfo* skin = gb.mInfo->mSkinInfo.get();
        if (!draw_av || !skin)
        {
            continue;
        }
        const std::pair<LLUUID, U64> key(draw_av->getID(), skin->mHash);
        if (frozen.find(key) != frozen.end())
        {
            continue;   // one snapshot per skin
        }
        // same palette the live upload would send this frame (world-space
        // invBind * joint world, GL-ready 3x4 floats)
        const LLVOAvatar::MatrixPaletteCache& mpc = draw_av->updateSkinInfoMatrixPalette(skin);
        if (!mpc.mGLMp.empty())
        {
            frozen[key] = mpc.mGLMp;
        }
    }
    if (frozen.empty())
    {
        return false;   // skins not loaded yet: nothing usable to hold
    }

    inst->mFrozenPalettes.swap(frozen);

    // [R2-2] freeze the NON-RIGGED attachment placement too: capture each
    // collected static face's owning-object render matrix (capture agent
    // frame, consistent with the palettes + anchor) so a frozen body's collar
    // holds with the pose instead of riding the live skeleton. A flexi prim's
    // matrix freezes here too, but its VERTICES stay live-deformed -- the
    // documented flexi limitation (see the freeze tooltip).
    inst->mFrozenAttachMats.clear();
    if (const std::vector<LLActorMover::GhostStaticFace>* statics =
            LLActorMover::instance().ghostStaticFacesFor(av->getID()))
    {
        for (const LLActorMover::GhostStaticFace& gf : *statics)
        {
            if (gf.mFace
                && inst->mFrozenAttachMats.find(gf.mObjectId) == inst->mFrozenAttachMats.end())
            {
                inst->mFrozenAttachMats[gf.mObjectId] = gf.mFace->getRenderMatrix();
            }
        }
    }

    // frame-matched anchor: the foot in the SAME agent frame the palettes
    // bake into (see the header for why this survives region crossings)
    LLVector3 foot = av->getRootJoint()->getWorldPosition();
    foot.mV[VZ] -= av->getPelvisToFoot();
    inst->mFrozenFootAgent = foot;
    inst->mPose = POSE_FROZEN;
    return true;
}

void ALGhostStudio::unfreezeInstance(const LLUUID& id)
{
    if (Instance* inst = getInstance(id))
    {
        inst->mPose = POSE_LIVE;
        inst->mFrozenPalettes.clear();
        inst->mFrozenAttachMats.clear();
    }
}

U32 ALGhostStudio::startFreezeStrip(const LLUUID& source_id, S32 count,
                                    F32 interval, F32 spacing,
                                    EFormation formation, F32 parameter)
{
    assert_main_thread();
    if (!getInstance(source_id) || count < 1)
    {
        return 0;
    }
    FreezeStrip job;
    job.mId = mNextStripId++;
    job.mSourceId = source_id;
    job.mCount = llclamp(count, 1, 64);
    job.mInterval = llclamp(interval, 0.01f, 60.f);
    job.mSpacing = llmax(spacing, 0.05f);
    job.mFormation = formation;
    job.mParameter = parameter;
    job.mNextCapture = LLTimer::getTotalSeconds(); // first pose on next frame
    mFreezeStrips.push_back(job);
    mLastStripStatus = llformat("Strip %u: capturing 0/%d", job.mId, job.mCount);
    return job.mId;
}

bool ALGhostStudio::cancelFreezeStrip(U32 strip_id)
{
    const auto found = std::find_if(mFreezeStrips.begin(), mFreezeStrips.end(),
        [strip_id](const FreezeStrip& j) { return j.mId == strip_id; });
    if (found == mFreezeStrips.end()) return false;
    mLastStripStatus = llformat("Strip %u cancelled at %d/%d",
        found->mId, found->mCaptured, found->mCount);
    mFreezeStrips.erase(found);
    return true;
}

std::string ALGhostStudio::freezeStripStatus() const
{
    if (!mFreezeStrips.empty())
    {
        const FreezeStrip& job = mFreezeStrips.back();
        return llformat("Strip %u: capturing %d/%d",
            job.mId, job.mCaptured, job.mCount);
    }
    return mLastStripStatus;
}

ALGhostStudio::FormationSlot ALGhostStudio::formationSlotAt(
    const Instance& p, S32 slot, S32 count, F32 spacing,
    EFormation formation, F32 parameter) const
{
    // THE single source of truth for formation geometry. makeArray() consumes
    // this; so does the freeze strip; so does the in-world preview. If you
    // change a formation, change it HERE and everything follows.
    //
    // Facing policy: Line/Grid/Staircase/Scatter inherit the prototype's full
    // rotation (mAuthorYaw = false). Ring/Arc/Spiral face outward. V angles
    // along its arm. Tunnel's parallel rows face inward at each other.
    FormationSlot r;
    const F32 yaw = p.getYaw();
    const LLVector3d forward(cosf(yaw), sinf(yaw), 0.0);
    const LLVector3d left(-sinf(yaw), cosf(yaw), 0.0);
    r.mFoot = p.mFootGlobal;
    r.mYaw = yaw;
    r.mAuthorYaw = true;

    if (slot <= 0)
    {
        // Slot 0 is always the untouched prototype, even where that makes it
        // the intentional exception to the formation's own facing rule.
        r.mAuthorYaw = false;
        return r;
    }

    switch (formation)
    {
    case FORMATION_RING:
    {
        const F32 step = F_TWO_PI / (F32)llmax(2, count);
        const F32 radius = spacing / (2.f * sinf(step * 0.5f));
        const F32 a = yaw + step * (F32)slot;
        r.mFoot += LLVector3d(cosf(a) - cosf(yaw), sinf(a) - sinf(yaw), 0.0) * (F64)radius;
        r.mYaw = a;
        return r;
    }
    case FORMATION_ARC:
    {
        const F32 sweep = (parameter > 0.f ? parameter : 120.f) * DEG_TO_RAD;
        const F32 step = sweep / (F32)llmax(1, count - 1);
        const F32 radius = spacing / (2.f * sinf(llmax(0.001f, step * 0.5f)));
        const F32 a0 = yaw - sweep * 0.5f;
        const F32 a = a0 + step * (F32)slot;
        r.mFoot += LLVector3d(cosf(a) - cosf(a0), sinf(a) - sinf(a0), 0.0) * (F64)radius;
        r.mYaw = a;
        return r;
    }
    case FORMATION_GRID:
    {
        const S32 cols = llmax(1, (S32)ceilf(sqrtf((F32)count)));
        r.mFoot += forward * (F64)(spacing * (F32)(slot / cols)) +
                   left    * (F64)(spacing * (F32)(slot % cols));
        r.mAuthorYaw = false;
        return r;
    }
    case FORMATION_V:
    {
        const F32 half = (parameter > 0.f ? parameter : 60.f) * DEG_TO_RAD * 0.5f;
        const F32 side = (slot & 1) ? 1.f : -1.f;
        const S32 rank = (slot + 1) / 2;
        // The ARM extends BEHIND the prototype (+F_PI) -- a V opens away from
        // the point, like geese. The member still FACES forward along its arm.
        const F32 arm_yaw = yaw + F_PI + side * half;
        r.mFoot += LLVector3d(cosf(arm_yaw), sinf(arm_yaw), 0.0) * (F64)((F32)rank * spacing);
        r.mYaw = yaw + side * half;
        return r;
    }
    case FORMATION_SPIRAL:
    {
        // Golden angle: successive slots never line up into visible spokes.
        const F32 golden = 2.39996323f;
        const F32 a = yaw + golden * (F32)slot;
        const F32 rad = spacing * sqrtf((F32)slot);
        r.mFoot += LLVector3d(cosf(a), sinf(a), 0.0) * (F64)rad;
        r.mYaw = a;
        return r;
    }
    case FORMATION_STAIRCASE:
    {
        // parameter != 0 (not > 0) so a NEGATIVE rise gives a DESCENDING
        // staircase. The old > 0 test made descending stairs impossible.
        const F32 rise = (parameter != 0.f) ? parameter : spacing * 0.5f;
        r.mFoot += forward * (F64)(spacing * (F32)slot) +
                   LLVector3d(0.0, 0.0, (F64)(rise * (F32)slot));
        r.mAuthorYaw = false;
        return r;
    }
    case FORMATION_TUNNEL:
    {
        const F32 side = (slot & 1) ? 1.f : -1.f;
        const S32 rank = (slot + 1) / 2;
        r.mFoot += forward * (F64)((F32)rank * spacing) +
                   left    * (F64)(side * spacing * 0.5f);
        r.mYaw = yaw - side * F_PI_BY_TWO;   // rows face inward at each other
        return r;
    }
    case FORMATION_SCATTER:
    {
        const F32 radius = parameter > 0.f ? parameter : spacing * sqrtf((F32)count);
        // Seeded from the PROTOTYPE id, so a rebuild from the same prototype
        // reproduces the same scatter. Rebuilding from a DIFFERENT prototype
        // deliberately gives a different pattern.
        const F32 a = F_TWO_PI * seeded_unit(p.mId, 100u + (U32)slot * 2u);
        const F32 rad = radius * sqrtf(seeded_unit(p.mId, 101u + (U32)slot * 2u));
        r.mFoot += LLVector3d(cosf(a), sinf(a), 0.0) * (F64)rad;
        r.mAuthorYaw = false;
        return r;
    }
    case FORMATION_LINE:
    default:
        r.mFoot += forward * (F64)(spacing * (F32)slot);
        r.mAuthorYaw = false;
        return r;
    }
}

LLVector3d ALGhostStudio::formationSlot(const Instance& p, S32 slot, S32 count,
                                        F32 spacing, EFormation formation,
                                        F32 parameter) const
{
    return formationSlotAt(p, slot, count, spacing, formation, parameter).mFoot;
}

void ALGhostStudio::formationPreviewSlots(const LLUUID& id, S32 count, F32 spacing,
                                          EFormation formation, F32 parameter,
                                          std::vector<FormationSlot>& out) const
{
    out.clear();
    const Instance* src = getInstance(id);
    // Same guards makeArray() applies, so the preview appears exactly when a
    // build would actually do something.
    if (!src || count < 2 || spacing < 0.05f)
    {
        return;
    }
    out.reserve((size_t)count);
    for (S32 i = 0; i < count; ++i)
    {
        out.push_back(formationSlotAt(*src, i, count, spacing, formation, parameter));
    }
}

void ALGhostStudio::updateFreezeStrips(F64 now)
{
    for (auto it = mFreezeStrips.begin(); it != mFreezeStrips.end(); )
    {
        FreezeStrip& job = *it;
        Instance* source = getInstance(job.mSourceId);
        LLVOAvatar* source_avatar =
            source ? LLDirectorCast::instance().resolve(source->mSource) : nullptr;
        if (!source || !source_avatar || source_avatar->isDead())
        {
            mLastStripStatus = llformat("Strip %u stopped: source missing (%d/%d kept)",
                job.mId, job.mCaptured, job.mCount);
            it = mFreezeStrips.erase(it);
            continue;
        }
        if (now < job.mNextCapture)
        {
            ++it;
            continue;
        }
        const Instance proto = *source;
        Instance* snap = duplicateInstanceInPlace(job.mSourceId);
        if (!snap)
        {
            mLastStripStatus = llformat("Strip %u stopped: duplicate failed (%d/%d kept)",
                job.mId, job.mCaptured, job.mCount);
            it = mFreezeStrips.erase(it);
            continue;
        }
        const LLUUID snap_id = snap->mId;
        snap->setFootGlobal(formationSlot(proto, job.mCaptured, job.mCount,
            job.mSpacing, job.mFormation, job.mParameter));
        renameInstance(snap_id, llformat("Strip %u \xC2\xB7 %d/%d",
            job.mId, job.mCaptured + 1, job.mCount));
        const bool held = snap->mKind == BACKING_ENTITY_CLONE
            ? setInstancePaused(snap_id, true) : freezeInstance(snap_id);
        if (!held)
        {
            removeInstance(snap_id);
            mLastStripStatus = llformat("Strip %u waiting for a renderable pose (%d/%d)",
                job.mId, job.mCaptured, job.mCount);
            // Retry rather than advancing: batch collection can lag duplication.
            job.mNextCapture = now + 0.05;
            ++it;
            continue;
        }
        if (snap->mKind == BACKING_ENTITY_CLONE) applyEntityTransform(snap_id);
        ++job.mCaptured;
        mLastStripStatus = llformat("Strip %u: capturing %d/%d",
            job.mId, job.mCaptured, job.mCount);
        // Never "catch up" missed deadlines with captures on adjacent frames:
        // successive poses must remain separated by the authored real interval.
        job.mNextCapture = now + job.mInterval;
        if (job.mCaptured >= job.mCount)
        {
            mLastStripStatus = llformat("Strip %u complete: %d/%d",
                job.mId, job.mCaptured, job.mCount);
            it = mFreezeStrips.erase(it);
        }
        else ++it;
    }
}

// ---------------------------------------------------------------------------
// array helper
// ---------------------------------------------------------------------------
S32 ALGhostStudio::makeArray(const LLUUID& id, S32 count, F32 spacing,
                             EFormation formation, F32 parameter)
{
    Instance* src = getInstance(id);
    if (!src || count < 2 || spacing < 0.05f)
    {
        return 0;
    }
    // capture by value: push_back below reallocates and would dangle `src`
    const Instance proto = *src;
    S32 made = 0;

    // Preserve the original overlay Line/Ring implementation literally:
    // transform values, revision counts, naming order and floating-point
    // operation order remain byte-identical to the pre-enum helper.
    if (proto.mKind == BACKING_OVERLAY &&
        (formation == FORMATION_LINE || formation == FORMATION_RING))
    {
        // Overlay ghosts keep their own copy/naming mechanics (push_back with a
        // fresh id and default name, rather than duplicateInstanceInPlace), but
        // the GEOMETRY now comes from formationSlotAt() like everything else.
        // Keeping a second inline implementation here is exactly how the
        // preview and the build drifted apart in the first place.
        for (S32 i = 1; i < count; ++i)
        {
            const FormationSlot s =
                formationSlotAt(proto, i, count, spacing, formation, parameter);
            Instance copy = proto;
            copy.mId.generate();
            copy.mName = makeDefaultName();
            copy.mFootGlobal = s.mFoot;
            if (s.mAuthorYaw)
            {
                copy.setYaw(s.mYaw);
            }
            mInstances.push_back(copy);
            ++made;
        }
        return made;
    }

    auto append = [this, &proto, &made](const LLVector3d& foot, F32 yaw,
                                        bool author_yaw = true)
    {
        Instance* copy = duplicateInstanceInPlace(proto.mId);
        if (!copy)
        {
            return;
        }
        copy->setFootGlobal(foot);
        if (author_yaw)
        {
            copy->setYaw(yaw);
        }
        if (copy->mKind == BACKING_ENTITY_CLONE)
        {
            applyEntityTransform(copy->mId);
        }
        ++made;
    };
    // Every formation now comes from formationSlotAt(), the SINGLE source of
    // truth that the in-world preview also uses. Previously this function had
    // its own inline geometry per formation and formationSlot() had a second,
    // DIFFERENT one -- so a preview could never have matched the build.
    for (S32 i = 1; i < count; ++i)
    {
        const FormationSlot s =
            formationSlotAt(proto, i, count, spacing, formation, parameter);
        append(s.mFoot, s.mYaw, s.mAuthorYaw);
    }
    return made;
}

void ALGhostStudio::setFormationPreview(const LLUUID& proto_id, S32 count,
                                       F32 spacing, EFormation formation,
                                       F32 parameter)
{
    mPreviewProto     = proto_id;
    mPreviewCount     = count;
    mPreviewSpacing   = spacing;
    mPreviewFormation = formation;
    mPreviewParameter = parameter;
}

void ALGhostStudio::renderFormationPreview()
{
    // Cost nothing unless the director is actually staging a formation. Same
    // gating shape as renderHeadingPreview(): setting on, an operator floater
    // open, and a valid prototype -- checked BEFORE any allocation.
    static LLCachedControl<bool> show(gSavedSettings, "GhostStudioShowFormationPreview", true);
    if (!show || mPreviewProto.isNull() || mPreviewCount < 2)
    {
        return;
    }
    bool operator_open = false;
    for (const char* name : { "ghost_studio", "director" })
    {
        LLFloater* floaterp = LLFloaterReg::findInstance(name);
        if (floaterp && floaterp->getVisible())
        {
            operator_open = true;
            break;
        }
    }
    if (!operator_open)
    {
        return;
    }

    std::vector<FormationSlot> slots;
    formationPreviewSlots(mPreviewProto, mPreviewCount, mPreviewSpacing,
                          mPreviewFormation, mPreviewParameter, slots);
    if (slots.size() < 2)
    {
        return;
    }

    // Client-side overlay, exactly like the beacon/heading passes: UI shader,
    // no texture, no depth write, so markers read over any ground.
    LLGLSUIDefault gls_ui;
    gUIProgram.bind();
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

    const F32 LIFT = 0.05f;             // sit just above the ground described
    const F32 R    = 0.28f;             // slot marker radius, m
    const F32 TICK = 0.55f;             // facing tick length, m
    const LLColor4 col_slot(0.25f, 0.75f, 1.f, 0.85f);   // staged slots
    const LLColor4 col_proto(1.f, 0.80f, 0.15f, 0.95f);  // slot 0 = prototype
    const LLColor4 col_perim(0.25f, 0.75f, 1.f, 0.35f);

    // centroid + max radius for the enclosing perimeter, computed from the
    // SAME slots that will be built -- not from the formation's nominal shape,
    // so Scatter's actual spread is what gets drawn.
    LLVector3 centre(0.f, 0.f, 0.f);
    std::vector<LLVector3> agent_pos;
    agent_pos.reserve(slots.size());
    for (const FormationSlot& s : slots)
    {
        const LLVector3 a = gAgent.getPosAgentFromGlobal(s.mFoot);
        agent_pos.push_back(a);
        centre += a;
    }
    centre *= 1.f / (F32)agent_pos.size();

    F32 perim_r = 0.f;
    for (const LLVector3& a : agent_pos)
    {
        const F32 dx = a.mV[VX] - centre.mV[VX];
        const F32 dy = a.mV[VY] - centre.mV[VY];
        perim_r = llmax(perim_r, sqrtf(dx * dx + dy * dy));
    }
    perim_r += R * 2.f;

    auto ring = [](const LLVector3& c, F32 r, const LLColor4& col, S32 segs)
    {
        gGL.color4fv(col.mV);
        gGL.begin(LLRender::LINES);
        for (S32 i = 0; i < segs; ++i)
        {
            const F32 a0 = F_TWO_PI * (F32)i / (F32)segs;
            const F32 a1 = F_TWO_PI * (F32)(i + 1) / (F32)segs;
            gGL.vertex3f(c.mV[VX] + cosf(a0) * r, c.mV[VY] + sinf(a0) * r, c.mV[VZ]);
            gGL.vertex3f(c.mV[VX] + cosf(a1) * r, c.mV[VY] + sinf(a1) * r, c.mV[VZ]);
        }
        gGL.end();
    };

    // enclosing perimeter -- the "where will they land" answer at a glance
    LLVector3 pc = centre;
    pc.mV[VZ] += LIFT;
    ring(pc, perim_r, col_perim, 64);

    for (size_t i = 0; i < agent_pos.size(); ++i)
    {
        LLVector3 a = agent_pos[i];
        a.mV[VZ] += LIFT;
        const LLColor4& col = (i == 0) ? col_proto : col_slot;
        ring(a, R, col, 20);

        // facing tick: which way that slot will END UP pointing. Slots that
        // inherit the prototype's rotation still show it, because "they all
        // face the same way" is itself information the director needs.
        gGL.color4fv(col.mV);
        gGL.begin(LLRender::LINES);
        gGL.vertex3f(a.mV[VX], a.mV[VY], a.mV[VZ]);
        gGL.vertex3f(a.mV[VX] + cosf(slots[i].mYaw) * TICK,
                     a.mV[VY] + sinf(slots[i].mYaw) * TICK,
                     a.mV[VZ]);
        gGL.end();
    }
}

// ---------------------------------------------------------------------------
// render-side queries
// ---------------------------------------------------------------------------
void ALGhostStudio::getWantedSources(uuid_vec_t& out) const
{
    if (!mShowAll)
    {
        return;
    }
    for (const Instance& inst : mInstances)
    {
        if (inst.mKind == BACKING_OVERLAY && inst.mEnabled
            && std::find(out.begin(), out.end(), inst.mSource) == out.end())
        {
            out.push_back(inst.mSource);
        }
    }
}
