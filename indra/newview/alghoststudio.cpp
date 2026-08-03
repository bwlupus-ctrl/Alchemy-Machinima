/**
 * @file alghoststudio.cpp
 * @brief Ghost Studio data model -- see alghoststudio.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "llpresentationtime.h"    // [Temporal Capture]

#include "alghoststudio.h"

#include "alghostnameplates.h"
#include "alghostspawnengine.h"
#include "alworldoverlayviz.h"
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
#include "llworld.h"
#include "llviewercamera.h"
#include "llfloaterreg.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "pipeline.h"
#include "lltimer.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <set>

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
    F32 pelvis_to_foot = av->getPelvisToFoot();
    if (!llfinite(pelvis_to_foot))
    {
        pelvis_to_foot = 0.f;
    }
    out_foot.mV[VZ] -= llmax(0.f, pelvis_to_foot);
    return out_foot.isFinite();
}

bool group_scale_limits(
    const ALGhostGroupModel::Group& group,
    F32& minimum_scale, F32& maximum_scale)
{
    minimum_scale = 0.0001f;
    maximum_scale = 1.0e30f;
    F64 minimum_local_pair = std::numeric_limits<F64>::max();
    for (const ALGhostGroupModel::Member& member : group.mMembers)
    {
        if (!llfinite(member.mLocal.mScale) ||
            member.mLocal.mScale <= 0.f)
        {
            return false;
        }
        minimum_scale = llmax(
            minimum_scale, GHOST_SCALE_MIN / member.mLocal.mScale);
        maximum_scale = llmin(
            maximum_scale, GHOST_SCALE_MAX / member.mLocal.mScale);
    }
    for (size_t a = 0; a + 1 < group.mMembers.size(); ++a)
    {
        for (size_t b = a + 1; b < group.mMembers.size(); ++b)
        {
            const F64 distance = (
                group.mMembers[a].mLocal.mFoot -
                group.mMembers[b].mLocal.mFoot).length();
            if (std::isfinite(distance) && distance > 0.0001)
            {
                minimum_local_pair =
                    llmin(minimum_local_pair, distance);
            }
        }
    }
    if (minimum_local_pair < std::numeric_limits<F64>::max())
    {
        const F64 spacing_minimum = 0.1 / minimum_local_pair;
        if (!std::isfinite(spacing_minimum) ||
            spacing_minimum > (F64)std::numeric_limits<F32>::max())
        {
            return false;
        }
        minimum_scale = llmax(minimum_scale, (F32)spacing_minimum);
    }
    return llfinite(minimum_scale) && llfinite(maximum_scale) &&
           minimum_scale > 0.f && minimum_scale <= maximum_scale;
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

bool valid_crowd_source(
    const ALGhostStudio::Instance& source, std::string* reason = nullptr)
{
    auto reject = [reason](const std::string& text)
    {
        if (reason)
        {
            *reason = text;
        }
        return false;
    };
    if (!source.mEnabled)
    {
        return reject(
            "Enable the selected prototype before building a visible crowd.");
    }
    if (!source.mFootGlobal.isFinite())
    {
        return reject("The prototype position is invalid.");
    }
    const F32 quaternion_norm_squared =
        dot(source.mRotation, source.mRotation);
    if (!source.mRotation.isFinite() ||
        !llfinite(quaternion_norm_squared) ||
        fabsf(quaternion_norm_squared - 1.f) > 0.001f)
    {
        return reject(
            "The prototype rotation is invalid; reset its transform and stage again.");
    }
    if (!llfinite(source.mScale) ||
        source.mScale < GHOST_SCALE_MIN || source.mScale > GHOST_SCALE_MAX)
    {
        return reject(llformat(
            "The prototype scale must be between %g and %g.",
            GHOST_SCALE_MIN, GHOST_SCALE_MAX));
    }
    if (source.mMotion != ALGhostStudio::MOTION_OFF ||
        source.mMotionHasBase || source.mChaosEnabled ||
        source.mChaosHasBase)
    {
        return reject(
            "Stop formation motion and Chaos before using this prototype.");
    }
    if (reason)
    {
        reason->clear();
    }
    return true;
}
} // anonymous namespace

ALGhostStudio::ALGhostStudio()
:   mNameplates(std::make_unique<ALGhostNameplates>())
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
            const bool subject_c =
                (old_id.notNull() && cast.getSubjectC() == old_id) ||
                (inst && inst->mWasDirectorSubjectC);
            const bool subject_d =
                (old_id.notNull() && cast.getSubjectD() == old_id) ||
                (inst && inst->mWasDirectorSubjectD);
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
            if (subject_c)
            {
                cast.setSubjectC(new_id);
            }
            if (subject_d)
            {
                cast.setSubjectD(new_id);
            }
            if (inst)
            {
                inst->mWasDirectorSubjectA =
                    !removing_instance && subject_a;
                inst->mWasDirectorSubjectB =
                    !removing_instance && subject_b;
                inst->mWasDirectorSubjectC =
                    !removing_instance && subject_c;
                inst->mWasDirectorSubjectD =
                    !removing_instance && subject_d;
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
                setSelected(LLUUID::null);
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

ALGhostStudio::~ALGhostStudio() = default;

ALGhostSpawnResult ALGhostStudio::spawnClone(
    const ALGhostCloneRequest& request)
{
    return ALGhostSpawnEngine(*this).spawn(request);
}

bool ALGhostStudio::updateClone(
    const LLUUID& id, const ALGhostCloneUpdate& update)
{
    return ALGhostSpawnEngine(*this).update(id, update);
}

bool ALGhostStudio::lockClone(const LLUUID& id)
{
    return ALGhostSpawnEngine(*this).lock(id);
}

bool ALGhostStudio::despawnClone(const LLUUID& id)
{
    return ALGhostSpawnEngine(*this).despawn(id);
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
    ghost->setGhostFootPosition(
        gAgent.getPosAgentFromGlobal(inst.mFootGlobal));
    ghost->setGhostRotation(inst.mRotation);
    ghost->setEntityScale(inst.mScale);
    const F32 chaos_factor =
        1.f + (seeded_unit(inst.mId, 5) * 2.f - 1.f) *
                  0.15f * inst.mChaosAmount;
    ghost->setEntityAnimTimeFactor(
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
    // Runtime consumer calls can pump observer code. Capture every value used
    // after that boundary so vector mutation cannot leave this function
    // dereferencing an invalidated Instance reference.
    const LLUUID stable_id = inst.mId;
    const std::string stable_name = inst.mName;
    const LLUUID old_runtime = inst.mEntityId;
    if (new_ghost)
    {
        applyEntityRuntimeState(inst, new_ghost);
    }
    inst.mEntityId = new_runtime;
    // [GhostWalk] Actor Mover keys walks / paths / suspend-resume state by the
    // RUNTIME uuid: migrate them to the replacement (or park them across a
    // despawn, or drop them on final removal) BEFORE the consumer walk, so any
    // observer a consumer pumps -- the Director-cast add/remove below -- already
    // sees the mover consistent under the new id. Without this, a refreshed or
    // despawn-recovered WALKING clone left its walk permanently suspended under
    // the dead uuid, unreachable by RESUME_NEAR and the Path tab alike. Runs
    // AFTER applyEntityRuntimeState so the fresh runtime's drive mode is set
    // before the migration restarts a live walk's loco anim on it.
    LLActorMover::instance().onActorRuntimeReplaced(
        stable_id, old_runtime, new_runtime, removing_instance);
    for (const runtime_consumer_t& consumer : mRuntimeConsumers)
    {
        consumer(stable_id, old_runtime, new_runtime, removing_instance);
    }
    if (new_runtime.notNull())
    {
        if (LLDirectorCast::CastMember* member =
                LLDirectorCast::instance().getMember(new_runtime))
        {
            member->mLastName = stable_name;
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
    // mRotation is an absolute world orientation for every backing kind.
    // Overlay geometry is collected in the source avatar's already-rotated
    // frame; its renderer removes that source rotation before applying this
    // authored orientation.
    if (LLVOAvatar* avatar = LLDirectorCast::instance().resolve(source))
    {
        inst.mRotation = avatar->getRotation();
    }
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
    LLVector3 source_foot;
    if (!source_foot_agent(source_id, source_foot))
    {
        LL_WARNS("GhostStudio")
            << "entity spawn: source foot unavailable" << LL_ENDL;
        return nullptr;
    }
    LLVector3 horizontal_offset = gAgent.getAtAxis() * 2.5f;
    horizontal_offset.mV[VZ] = 0.f;
    record.mFootGlobal =
        gAgent.getPosGlobalFromAgent(source_foot + horizontal_offset);
    record.mRotation = source->getRotation();
    mInstances.push_back(record);
    const LLUUID stable_id = record.mId;

    Instance* committed = getInstance(stable_id);
    S32 attachments = 0;
    LLGhostAvatar* ghost = committed
        ? createEntityRuntime(*committed, attachments) : nullptr;
    // Appearance/attachment cloning can pump viewer observers. Reacquire the
    // vector-backed record before either the success or failure path uses it.
    committed = getInstance(stable_id);
    const bool baseline_unchanged =
        committed &&
        committed->mKind == BACKING_ENTITY_CLONE &&
        committed->mSource == source_id &&
        committed->mState == STATE_SPAWNING &&
        committed->mEntityId.isNull() &&
        committed->mGroupId.isNull();
    if (!ghost || !baseline_unchanged)
    {
        if (ghost && !ghost->isDead())
        {
            set_entity_clone_visible(ghost, false);
            ghost->releaseClonedAttachments();
            ghost->markForDeath();
        }
        if (!ghost && baseline_unchanged)
        {
            removeInstance(stable_id);
        }
        LL_WARNS("GhostStudio") << "entity spawn rolled back" << LL_ENDL;
        return nullptr;
    }

    onEntityRuntimeReplaced(*committed, ghost->getID(), ghost);
    committed = getInstance(stable_id);
    if (!committed)
    {
        if (!ghost->isDead())
        {
            set_entity_clone_visible(ghost, false);
            ghost->releaseClonedAttachments();
            ghost->markForDeath();
        }
        LL_WARNS("GhostStudio")
            << "entity spawn invalidated by a runtime consumer" << LL_ENDL;
        return nullptr;
    }
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

    const LLUUID stable_id = inst->mId;
    const LLUUID expected_runtime = inst->mEntityId;
    const LLUUID expected_source = inst->mSource;
    S32 attachments = 0;
    LLGhostAvatar* replacement = createEntityRuntime(*inst, attachments);
    // Runtime creation can reallocate/remove the vector record.
    inst = getInstance(stable_id);
    const bool baseline_unchanged =
        inst &&
        inst->mKind == BACKING_ENTITY_CLONE &&
        inst->mSource == expected_source &&
        inst->mEntityId == expected_runtime &&
        inst->mState != STATE_TEARING_DOWN;
    if (!replacement)
    {
        if (baseline_unchanged)
        {
            LLGhostAvatar* current_runtime = resolveEntityClone(stable_id);
            inst->mState =
                current_runtime ? STATE_ERROR : STATE_RECOVERABLE;
        }
        LL_WARNS("GhostStudio")
            << "refreshEntityClone: replacement attachment clone incomplete for "
            << id << "; existing clone preserved" << LL_ENDL;
        return false;
    }
    if (!baseline_unchanged)
    {
        if (!replacement->isDead())
        {
            set_entity_clone_visible(replacement, false);
            replacement->releaseClonedAttachments();
            replacement->markForDeath();
        }
        return false;
    }

    // Resolve the current old runtime only after proving nobody replaced it
    // while the new appearance was being cloned.
    old_ghost = resolveEntityClone(stable_id);
    onEntityRuntimeReplaced(*inst, replacement->getID(), replacement);
    inst = getInstance(stable_id);
    if (!inst)
    {
        if (!replacement->isDead())
        {
            set_entity_clone_visible(replacement, false);
            replacement->releaseClonedAttachments();
            replacement->markForDeath();
        }
        return false;
    }
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
    copy.mGroupId.setNull();
    copy.mLockMode = LOCK_OFF;
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
    return duplicateInstanceSnapshotInPlace(*src);
}

ALGhostStudio::Instance* ALGhostStudio::duplicateInstanceSnapshotInPlace(
    Instance proto)
{
    if (proto.mKind == BACKING_OVERLAY)
    {
        Instance copy = proto;
        copy.mId.generate();
        copy.mName = makeDefaultName();
        copy.mGroupId.setNull();
        copy.mLockMode = LOCK_OFF;
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
    copy->mGroupId.setNull();
    copy->mLockMode = LOCK_OFF;
    if (entity_clone_count(mInstances) > ENTITY_PHYSICS_CROWD_THRESHOLD)
    {
        copy->mPhysicsEnabled = false;
    }
    copy->mWasDirectorSubjectA = false;
    copy->mWasDirectorSubjectB = false;
    copy->mWasDirectorSubjectC = false;
    copy->mWasDirectorSubjectD = false;
    copy->mWasCinematicFollow = false;
    copy->setTransform(proto.mFootGlobal, proto.mRotation);
    copy->setScale(proto.mScale);
    if (LLGhostAvatar* ghost = resolveEntityClone(copy->mId))
    {
        applyEntityRuntimeState(*copy, ghost);
    }
    return copy;
}

std::vector<LLUUID> ALGhostStudio::duplicateGroup(
    const LLUUID& id, bool in_place)
{
    assert_main_thread();
    std::vector<LLUUID> result;
    const ALGhostGroupModel::Group* live_group = groupForMember(id);
    if (!live_group || live_group->mMembers.empty())
    {
        return result;
    }
    const ALGhostGroupModel::Group source_group = *live_group;

    std::vector<Instance> prototypes;
    prototypes.reserve(source_group.mMembers.size());
    for (const ALGhostGroupModel::Member& member : source_group.mMembers)
    {
        const Instance* source = getInstance(member.mInstanceId);
        if (!source || source->mGroupId != source_group.mId)
        {
            return result;
        }
        prototypes.push_back(*source);
    }

    std::vector<LLUUID> created;
    created.reserve(prototypes.size());
    auto rollback = [this, &created]()
    {
        for (auto it = created.rbegin(); it != created.rend(); ++it)
        {
            removeInstance(*it);
        }
        created.clear();
    };
    for (const Instance& prototype : prototypes)
    {
        Instance* copy = duplicateInstanceSnapshotInPlace(prototype);
        if (!copy)
        {
            rollback();
            return result;
        }
        created.push_back(copy->mId);
    }

    const ALGhostGroupModel::Group* unchanged_group =
        mGroups.findGroup(source_group.mId);
    bool source_hierarchy_unchanged =
        unchanged_group &&
        unchanged_group->mRevision == source_group.mRevision &&
        unchanged_group->mMembers.size() ==
            source_group.mMembers.size();
    for (size_t i = 0;
         source_hierarchy_unchanged &&
         i < source_group.mMembers.size(); ++i)
    {
        source_hierarchy_unchanged =
            unchanged_group->mMembers[i].mInstanceId ==
                source_group.mMembers[i].mInstanceId;
    }
    if (!source_hierarchy_unchanged)
    {
        // Entity runtime creation can pump consumers. Never combine immutable
        // backing snapshots with a hierarchy edited during that allocation.
        rollback();
        return result;
    }

    LLVector3d offset;
    if (!in_place)
    {
        LLVector3 left =
            LLVector3::y_axis * source_group.mWorld.mRotation;
        if (!left.isFinite())
        {
            rollback();
            return result;
        }
        F32 horizontal_length =
            sqrtf(left.mV[VX] * left.mV[VX] +
                  left.mV[VY] * left.mV[VY]);
        if (!llfinite(horizontal_length) ||
            horizontal_length <= 0.000001f)
        {
            // A heavily pitched group can point local-left almost vertically.
            // Derive a horizontal left from local-forward, with a stable world
            // fallback when both local axes are vertical.
            const LLVector3 forward =
                LLVector3::x_axis * source_group.mWorld.mRotation;
            horizontal_length =
                sqrtf(forward.mV[VX] * forward.mV[VX] +
                      forward.mV[VY] * forward.mV[VY]);
            if (forward.isFinite() &&
                llfinite(horizontal_length) &&
                horizontal_length > 0.000001f)
            {
                left.setVec(
                    -forward.mV[VY] / horizontal_length,
                    forward.mV[VX] / horizontal_length, 0.f);
            }
            else
            {
                left.setVec(0.f, 1.f, 0.f);
            }
        }
        else
        {
            left.setVec(
                left.mV[VX] / horizontal_length,
                left.mV[VY] / horizontal_length, 0.f);
        }
        offset.set((F64)left.mV[VX], (F64)left.mV[VY], 0.0);
    }
    LLUUID clone_group_id;
    clone_group_id.generate();
    if (!mGroups.cloneGroup(
            source_group.mId, clone_group_id, created, offset))
    {
        rollback();
        return result;
    }

    const ELockMode lock_mode =
        source_group.mMode == ALGhostGroupModel::MODE_MOVE_FACE
            ? LOCK_MOVE_FACE : LOCK_RIGID_UNIT;
    for (const LLUUID& created_id : created)
    {
        Instance* copy = getInstance(created_id);
        if (!copy)
        {
            mGroups.eraseGroup(clone_group_id);
            rollback();
            return result;
        }
        copy->mGroupId = clone_group_id;
        copy->mLockMode = lock_mode;
    }
    if (!applyGroupTransforms(clone_group_id, nullptr))
    {
        mGroups.eraseGroup(clone_group_id);
        for (const LLUUID& created_id : created)
        {
            if (Instance* copy = getInstance(created_id))
            {
                copy->mGroupId.setNull();
                copy->mLockMode = LOCK_OFF;
            }
        }
        rollback();
        return result;
    }
    result = created;
    return result;
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
    const ALGhostGroupModel::Group* group = groupForMember(id);
    if (group && !group->mEditMembers)
    {
        bool changed = false;
        for (const LLUUID& member : groupMembers(id))
            if (Instance* item = getInstance(member))
            {
                item->mEnabled = enabled;
                if (item->mKind == BACKING_ENTITY_CLONE)
                    if (LLGhostAvatar* ghost = resolveEntityClone(member))
                        set_entity_clone_visible(ghost, mShowAll && enabled);
                changed = true;
            }
        return changed;
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
    scale = llclamp(scale, GHOST_SCALE_MIN, GHOST_SCALE_MAX);
    return transformUnit(id, inst->mFootGlobal, inst->mRotation, scale);
}

bool ALGhostStudio::setPoseRate(const LLUUID& id, F32 hz)
{
    Instance* anchor = getInstance(id);
    if (!anchor) return false;
    hz = (hz >= 29.5f || hz <= 0.f) ? 0.f : llclamp(hz, 2.f, 30.f);
    const std::vector<LLUUID> ids = groupMembers(id);
    for (const LLUUID& member_id : ids)
    {
        if (Instance* inst = getInstance(member_id))
        {
            inst->mPoseRateHz = hz;
            inst->mNextPoseRefresh = 0.0;
            if (hz == 0.f)
            {
                inst->mCadencePalettes.clear();
                inst->mCadenceHeadValid = false;
            }
        }
    }
    return true;
}

bool ALGhostStudio::setEffectFps(const LLUUID& id, F32 fps)
{
    Instance* inst = getInstance(id);
    if (!inst) return false;
    inst->mEffectFps = fps <= 0.f ? 0.f : llclamp(floorf(fps + 0.5f), 1.f, 30.f);
    return true;
}

bool ALGhostStudio::refreshPoseCadence(const LLUUID& id, F64 now)
{
    Instance* inst = getInstance(id);
    if (!inst || inst->mPose != POSE_LIVE || inst->mPoseRateHz <= 0.f)
        return false;
    if (!inst->mCadencePalettes.empty() && now < inst->mNextPoseRefresh)
        return true;
    LLVOAvatar* av = LLDirectorCast::instance().resolve(inst->mSource);
    const std::vector<LLActorMover::GhostBatch>* batches =
        av ? LLActorMover::instance().ghostBatchesFor(av->getID()) : nullptr;
    if (!av || !batches || batches->empty()) return !inst->mCadencePalettes.empty();
    palette_map_t palette;
    for (const LLActorMover::GhostBatch& gb : *batches)
    {
        LLVOAvatar* draw_av = gb.mInfo ? gb.mInfo->mAvatar.get() : nullptr;
        LLMeshSkinInfo* skin = gb.mInfo ? gb.mInfo->mSkinInfo.get() : nullptr;
        if (!draw_av || !skin) continue;
        const std::pair<LLUUID, U64> key(draw_av->getID(), skin->mHash);
        if (palette.find(key) != palette.end()) continue;
        const LLVOAvatar::MatrixPaletteCache& mpc =
            draw_av->updateSkinInfoMatrixPalette(skin);
        if (!mpc.mGLMp.empty()) palette[key] = mpc.mGLMp;
    }
    if (palette.empty()) return !inst->mCadencePalettes.empty();
    inst->mCadencePalettes.swap(palette);
    const LLVector3 root = av->getRenderPosition();
    F32 pelvis_to_foot = av->getPelvisToFoot();
    if (!llfinite(pelvis_to_foot))
    {
        pelvis_to_foot = 0.f;
    }
    inst->mCadenceFootAgent.set(root.mV[VX], root.mV[VY],
                                root.mV[VZ] - llmax(0.f, pelvis_to_foot));
    inst->mCadenceHeadValid = false;
    if (LLJoint* head = av->getJoint("mHead"))
    {
        const LLVector3 position = head->getWorldPosition();
        if (position.isFinite())
        {
            inst->mCadenceHeadAgent = position;
            inst->mCadenceHeadValid = true;
        }
    }
    inst->mNextPoseRefresh = now + 1.0 / (F64)inst->mPoseRateHz;
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
    ghost->setEntityAnimTimeFactor(
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
    ghost->setGhostFootPosition(
        gAgent.getPosAgentFromGlobal(inst->mFootGlobal));
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
    if (ALGhostGroupModel::Group* group = groupForMember(id))
    {
        const ALGhostGroupModel::Group before = *group;
        bool aimed = false;
        for (const LLUUID& member_id : groupMembers(id))
        {
            Instance* member = getInstance(member_id);
            if (!member)
            {
                mGroups.restoreGroupSnapshot(before);
                return false;
            }
            const LLVector3d member_delta =
                target_global - member->mFootGlobal;
            if (member_delta.mdV[VX] * member_delta.mdV[VX] +
                member_delta.mdV[VY] * member_delta.mdV[VY] < 0.000001)
            {
                continue;
            }
            const LLQuaternion member_rotation(
                atan2f((F32)member_delta.mdV[VY],
                       (F32)member_delta.mdV[VX]),
                LLVector3::z_axis);
            if (!mGroups.setMemberWorldRotation(
                    member_id, member_rotation))
            {
                mGroups.restoreGroupSnapshot(before);
                return false;
            }
            aimed = true;
        }
        return aimed && applyGroupTransforms(before.mId, &before);
    }
    const LLVector3d delta = target_global - inst->mFootGlobal;
    if (delta.mdV[VX] * delta.mdV[VX] + delta.mdV[VY] * delta.mdV[VY] < 0.000001)
    {
        return false; // coincident horizontally: retain the last meaningful yaw
    }
    return transformUnit(id, inst->mFootGlobal,
        LLQuaternion(atan2f((F32)delta.mdV[VY], (F32)delta.mdV[VX]),
                     LLVector3::z_axis),
        inst->mScale);
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
        transformUnit(id, inst->mFootGlobal,
                      LLQuaternion(want, LLVector3::z_axis), inst->mScale);
        inst->mTurnSettled = true;
        inst->mTurnSettledFoot = inst->mFootGlobal;
        return true;
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
    return transformUnit(id, inst->mFootGlobal,
        LLQuaternion(have + (diff > 0.f ? step : -step), LLVector3::z_axis),
        inst->mScale);
}

void ALGhostStudio::setLookTarget(const LLUUID& id, ELookTarget target,
                                  const LLUUID& target_id, bool keep_facing)
{
    for (const LLUUID& member_id : groupMembers(id))
    {
        if (Instance* member = getInstance(member_id))
        {
            member->mLookTarget = target;
            member->mLookTargetId = target_id;
            member->mKeepFacing = keep_facing;
        }
    }
}

void ALGhostStudio::updateLookAt()
{
    assert_main_thread();
    // faceInstance does not mutate the vector, but setYaw bumps transform
    // revisions; IDs keep this loop robust if implementation later changes.
    std::vector<LLUUID> tracking;
    std::set<LLUUID> tracking_groups;
    for (const Instance& inst : mInstances)
    {
        // Turn owns the body when it is armed. Both write mRotation, so running
        // keep-facing as well would just be overwritten by stepTurn a moment
        // later -- worse, it would fight it every frame.
        if (inst.mKeepFacing && inst.mTurnMode == TURN_MODE_OFF)
        {
            if (inst.mGroupId.notNull() &&
                !tracking_groups.insert(inst.mGroupId).second) continue;
            tracking.push_back(inst.mId);
        }
    }
    for (const LLUUID& id : tracking)
    {
        faceInstance(id);
    }
}

bool ALGhostStudio::nameplateAnchor(
    const Instance& inst, LLVector3& anchor_agent) const
{
    // Entity clones own a real skeleton. Use its current head joint so shaped,
    // posed, pitched, and scaled bodies get the same semantic anchor as an
    // ordinary avatar rather than a fixed two-metre guess.
    if (inst.mKind == BACKING_ENTITY_CLONE)
    {
        if (LLGhostAvatar* ghost = resolveEntityClone(inst.mId))
        {
            if (LLJoint* head = ghost->getJoint("mHead"))
            {
                anchor_agent = head->getWorldPosition();
                if (anchor_agent.isFinite())
                {
                    anchor_agent.mV[VZ] +=
                        0.16f * llclamp(inst.mScale, GHOST_SCALE_MIN, GHOST_SCALE_MAX);
                    return anchor_agent.isFinite();
                }
            }
            const LLVector3* extents = ghost->getLastAnimExtents();
            if (extents && extents[0].isFinite() && extents[1].isFinite() &&
                extents[0].mV[VZ] <= extents[1].mV[VZ])
            {
                anchor_agent = (extents[0] + extents[1]) * 0.5f;
                anchor_agent.mV[VZ] = extents[1].mV[VZ] + 0.12f;
                return anchor_agent.isFinite();
            }
        }
        return false;
    }

    LLVOAvatar* source = LLDirectorCast::instance().resolve(inst.mSource);
    if (!source || !source->getRootJoint())
    {
        return false;
    }

    LLVector3 source_foot;
    LLVector3 source_head;
    bool have_head = false;
    if (inst.mPose == POSE_FROZEN && inst.mFrozenHeadValid)
    {
        source_foot = inst.mFrozenFootAgent;
        source_head = inst.mFrozenHeadAgent;
        have_head = true;
    }
    else if (inst.mPoseRateHz > 0.f &&
             !inst.mCadencePalettes.empty() &&
             inst.mCadenceHeadValid)
    {
        source_foot = inst.mCadenceFootAgent;
        source_head = inst.mCadenceHeadAgent;
        have_head = true;
    }
    else
    {
        source_foot = source->getRootJoint()->getWorldPosition();
        F32 pelvis_to_foot = source->getPelvisToFoot();
        if (!llfinite(pelvis_to_foot))
        {
            pelvis_to_foot = 0.f;
        }
        source_foot.mV[VZ] -= llmax(0.f, pelvis_to_foot);
        if (LLJoint* head = source->getJoint("mHead"))
        {
            source_head = head->getWorldPosition();
            have_head = source_head.isFinite();
        }
    }
    if (!source_foot.isFinite())
    {
        return false;
    }

    // Overlay geometry is already in the source avatar's world frame. Apply
    // the exact source->instance delta used by drawGeometryGhost(), about the
    // same live/frozen/cadence foot pivot.
    LLQuaternion source_to_instance =
        ~source->getRotation() * inst.mRotation;
    source_to_instance.normalize();
    if (!source_to_instance.isFinite())
    {
        return false;
    }
    const LLVector3 placed_foot =
        gAgent.getPosAgentFromGlobal(inst.mFootGlobal);
    if (have_head)
    {
        LLVector3 offset = (source_head - source_foot) *
                           llclamp(inst.mScale, GHOST_SCALE_MIN, GHOST_SCALE_MAX);
        offset *= source_to_instance;
        anchor_agent = placed_foot + offset;
        anchor_agent.mV[VZ] +=
            0.16f * llclamp(inst.mScale, GHOST_SCALE_MIN, GHOST_SCALE_MAX);
        return anchor_agent.isFinite();
    }

    // Skeletons can be temporarily incomplete while an outfit loads. Transform
    // the source's actual animated upper bound instead of reverting to a fixed
    // human height.
    const LLVector3* extents = source->getLastAnimExtents();
    if (!extents || !extents[0].isFinite() || !extents[1].isFinite() ||
        extents[0].mV[VZ] > extents[1].mV[VZ])
    {
        return false;
    }
    LLVector3 top((extents[0].mV[VX] + extents[1].mV[VX]) * 0.5f,
                  (extents[0].mV[VY] + extents[1].mV[VY]) * 0.5f,
                  extents[1].mV[VZ]);
    LLVector3 offset = (top - source_foot) *
                       llclamp(inst.mScale, GHOST_SCALE_MIN, GHOST_SCALE_MAX);
    offset *= source_to_instance;
    anchor_agent = placed_foot + offset;
    anchor_agent.mV[VZ] += 0.12f;
    return anchor_agent.isFinite();
}

void ALGhostStudio::updateNameplates()
{
    if (!mNameplates)
    {
        return;
    }
    static LLCachedControl<S32> mode(
        gSavedSettings, "GhostStudioNameplateMode", 1);
    const bool ui_visible = gViewerWindow && gViewerWindow->getUIVisibility() &&
        gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);
    if (!mShowAll || mode <= 0 || !ui_visible)
    {
        mNameplates->update({}, false);
        return;
    }

    std::vector<ALGhostNameplates::Label> labels;
    std::set<LLUUID> grouped_members;
    for (const auto& entry : mGroups.groups())
    {
        const ALGhostGroupModel::Group& group = entry.second;
        LLVector3 centre;
        F32 top = -1.0e30f;
        S32 visible_members = 0;
        for (const ALGhostGroupModel::Member& member : group.mMembers)
        {
            grouped_members.insert(member.mInstanceId);
            const Instance* inst = getInstance(member.mInstanceId);
            if (!inst || !inst->mEnabled)
            {
                continue;
            }
            LLVector3 member_anchor;
            if (!nameplateAnchor(*inst, member_anchor))
            {
                continue;
            }
            centre += member_anchor;
            top = llmax(top, member_anchor.mV[VZ]);
            ++visible_members;
        }
        if (visible_members == 0)
        {
            continue;
        }
        centre /= (F32)visible_members;
        centre.mV[VZ] = top + 0.15f;
        ALGhostNameplates::Label label;
        label.mKey = group.mId;
        label.mText = llformat("%s \xC2\xB7 %d",
            group.mName.c_str(), visible_members);
        label.mPositionAgent = centre;
        const Instance* selected_instance = getInstance(mSelected);
        label.mSelected = mSelected == group.mId ||
            (selected_instance &&
             selected_instance->mGroupId == group.mId);
        labels.push_back(label);

        if (group.mEditMembers)
        {
            for (const ALGhostGroupModel::Member& member : group.mMembers)
            {
                const Instance* inst = getInstance(member.mInstanceId);
                if (!inst || !inst->mEnabled ||
                    (mode == 1 && inst->mId != mSelected))
                {
                    continue;
                }
                ALGhostNameplates::Label member_label;
                member_label.mKey = inst->mId;
                member_label.mText = inst->mName;
                if (!nameplateAnchor(*inst,
                                     member_label.mPositionAgent))
                {
                    continue;
                }
                member_label.mSelected = inst->mId == mSelected;
                labels.push_back(member_label);
            }
        }
    }

    for (const Instance& inst : mInstances)
    {
        if (!inst.mEnabled ||
            grouped_members.find(inst.mId) != grouped_members.end())
        {
            continue;
        }
        ALGhostNameplates::Label label;
        label.mKey = inst.mId;
        label.mText = inst.mName;
        if (!nameplateAnchor(inst, label.mPositionAgent))
        {
            continue;
        }
        label.mSelected = inst.mId == mSelected;
        labels.push_back(label);
    }
    mNameplates->update(labels, true);
}

void ALGhostStudio::updatePerFrame()
{
    assert_main_thread();
    refreshLifecycleStates();
    finishPendingRuntimeFreezes();
    updateNameplates();
    const F64 facing_now = LLTimer::getTotalSeconds();
    std::set<LLUUID> dynamic_facing_groups;
    for (Instance& inst : mInstances)
    {
        if (inst.mCrowdFacing == FACING_TRACK_SUBJECT)
        {
            LLVector3d target;
            if (resolveTargetGlobal(inst, inst.mLookTarget,
                    inst.mLookTargetId, inst.mLookPointGlobal, target))
            {
                const LLVector3d delta = target - inst.mFootGlobal;
                if (delta.mdV[VX] * delta.mdV[VX] +
                    delta.mdV[VY] * delta.mdV[VY] >= 0.000001)
                {
                    LLQuaternion rotation(
                        atan2f((F32)delta.mdV[VY], (F32)delta.mdV[VX]),
                        LLVector3::z_axis);
                    if (inst.mGroupId.notNull())
                    {
                        if (mGroups.setMemberWorldRotation(inst.mId, rotation))
                            dynamic_facing_groups.insert(inst.mGroupId);
                    }
                    else
                    {
                        inst.mRotation = rotation;
                        ++inst.mTransformRevision;
                        if (inst.mKind == BACKING_ENTITY_CLONE)
                            applyEntityTransform(inst.mId);
                    }
                }
            }
        }
        if (inst.mCrowdFacing == FACING_WAVE && inst.mGroupId.isNull())
        {
            const F32 phase = (F32)(facing_now - inst.mCrowdFacingStart) *
                              2.5f - (F32)inst.mCrowdSlot * 0.65f;
            inst.mRotation = LLQuaternion(
                inst.mCrowdBaseYaw + sinf(phase) * (35.f * DEG_TO_RAD),
                LLVector3::z_axis);
            inst.mRotation.normalize();
            ++inst.mTransformRevision;
            if (inst.mKind == BACKING_ENTITY_CLONE)
                applyEntityTransform(inst.mId);
        }
    }
    for (const LLUUID& group_id : dynamic_facing_groups)
        applyGroupTransforms(group_id);
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
    std::set<LLUUID> tracking_groups;
    for (const Instance& inst : mInstances)
    {
        if (inst.mKeepFacing && inst.mMotion != MOTION_SPIN
            && inst.mTurnMode == TURN_MODE_OFF)
        {
            if (inst.mGroupId.notNull() &&
                !tracking_groups.insert(inst.mGroupId).second) continue;
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
    // [Temporal Capture] Animation drive: step body turns on the presentation
    // clock so ghost turning slows with gait/world. 0x -> 0 holds; the clamp
    // guards a fast-scale hitch from snapping the yaw.
    F32 dt = gFrameIntervalSeconds;
    if (LLPresentationTime::drives(LLTemporalFeature::ANIMATION))
    {
        // presentation-scaled, capped at the uniform 0.25s hitch cap (stepTurn also
        // clamps internally); coupling with gait is exact under the cap
        dt = llclamp((F32)LLPresentationTime::presentationDelta(), 0.f, 0.25f);
    }
    std::vector<LLUUID> turning;
    std::set<LLUUID> turning_groups;
    for (const Instance& inst : mInstances)
    {
        if (inst.mTurnMode != TURN_MODE_OFF && inst.mMotion != MOTION_SPIN)
        {
            if (inst.mGroupId.notNull() &&
                !turning_groups.insert(inst.mGroupId).second) continue;
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
    // A committed crowd has one authoritative hierarchical transform. The
    // legacy per-instance procedural writer predates that model and would make
    // members visibly diverge from their stored locals. Keep the operation
    // explicit and safe until motion is represented as a group-level layer.
    for (const LLUUID& id : ids)
    {
        if (groupForMember(id))
        {
            return 0;
        }
    }
    std::vector<LLUUID> effective;
    for (const LLUUID& id : ids)
        for (const LLUUID& member : groupMembers(id))
            if (std::find(effective.begin(), effective.end(), member) == effective.end())
                effective.push_back(member);
    LLVector3d centre;
    S32 valid = 0;
    for (const LLUUID& id : effective)
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
    for (const LLUUID& id : effective)
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
        // Locked group: baseline the path yaw from the ANCHOR member's source
        // path (the same source the march update uses), so a mixed-source unit
        // shares one path frame instead of each member diverging from its own
        // baseline. A consistent default keeps a pathless member from retaining
        // a stale per-member yaw.
        LLUUID path_src = inst->mSource;
        if (inst->mGroupId.notNull())
        {
            const std::vector<LLUUID> unit = groupMembers(inst->mId);
            if (!unit.empty())
                if (Instance* anchor = getInstance(unit.front()))
                    path_src = anchor->mSource;
        }
        inst->mMotionPathYaw = 0.f;
        if (const LLActorMover::Path* path =
                LLActorMover::instance().getPath(path_src))
        {
            LLVector3d pos, tangent;
            path->evalAtDistance(0.f, pos, tangent);
            inst->mMotionPathYaw = atan2f((F32)tangent.mdV[VY],
                                         (F32)tangent.mdV[VX]);
        }
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
        else if (inst.mMotion == MOTION_PATH)
        {
            // A locked group marches on ONE shared path -- the anchor member's
            // source path -- so a mixed-source unit keeps its rigid offsets
            // instead of each member evaluating a different path and drifting apart.
            LLUUID path_src = inst.mSource;
            if (inst.mGroupId.notNull())
            {
                const std::vector<LLUUID> unit = groupMembers(inst.mId);
                if (!unit.empty())
                    if (Instance* anchor = getInstance(unit.front()))
                        path_src = anchor->mSource;
            }
            const LLActorMover::Path* path =
                LLActorMover::instance().getPath(path_src);
            if (path && path->mNodes.size() >= 2 && path->mTotalLength > 0.f)
            {
                LLVector3d path_pos, tangent;
                const F32 distance = fmodf((F32)(now - inst.mMotionStart) *
                                           inst.mMotionSpeed, path->mTotalLength);
                path->evalAtDistance(distance, path_pos, tangent);
                const F32 path_yaw = atan2f((F32)tangent.mdV[VY],
                                            (F32)tangent.mdV[VX]);
                const F32 delta = path_yaw - inst.mMotionPathYaw;
                const F64 c = cosf(delta), s = sinf(delta);
                inst.mFootGlobal = path_pos +
                    LLVector3d(radial.mdV[VX] * c - radial.mdV[VY] * s,
                               radial.mdV[VX] * s + radial.mdV[VY] * c,
                               radial.mdV[VZ]);
                inst.mRotation =
                    LLQuaternion(delta, LLVector3::z_axis) * inst.mMotionBaseRotation;
            }
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
                 0.12f * inst.mChaosAmount), GHOST_SCALE_MIN, GHOST_SCALE_MAX);
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
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE ||
        groupForMember(id))
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
    inst->mScale = llclamp(inst->mChaosBaseScale * scale_factor,
                           GHOST_SCALE_MIN, GHOST_SCALE_MAX);
    ++inst->mTransformRevision;

    LLGhostAvatar* ghost = resolveEntityClone(id);
    if (!ghost)
    {
        return false;
    }
    ghost->setEntityScale(inst->mScale);
    const bool transformed = applyEntityTransform(id);
    setInstanceAnimSpeed(id, inst->mAnimSpeed);
    if (amount == 0.f)
    {
        inst->mChaosHasBase = false;
    }
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
    if (mCrowdDraft.mActive && mCrowdDraft.mSource.mInstanceId == id)
    {
        // A draft can never outlive its prototype. The placement tool notices
        // the session loss and restores the prior base tool on its next event.
        // User-facing cancellation is guarded during commit, but authoritative
        // source deletion must still invalidate the session immediately.
        if (mCrowdCommitInProgress)
        {
            mCrowdDraft = CrowdPlacementDraft();
        }
        else
        {
            cancelCrowdPlacement();
        }
    }
    Instance* inst = getInstance(id);
    if (!inst)
    {
        return;
    }
    if (inst->mGroupId.notNull())
    {
        const LLUUID group_id = inst->mGroupId;
        const std::vector<LLUUID> unit = groupMembers(id);
        if (mSelected == group_id)
        {
            setSelected(LLUUID::null);
        }
        mGroups.eraseGroup(group_id);
        for (const LLUUID& member : unit)
            if (Instance* item = getInstance(member)) item->mGroupId.setNull();
        for (const LLUUID& member : unit) removeInstance(member);
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
        setSelected(LLUUID::null);
    }
    mInstances.erase(
        std::remove_if(mInstances.begin(), mInstances.end(),
                       [&id](const Instance& i) { return i.mId == id; }),
        mInstances.end());
}

std::vector<LLUUID> ALGhostStudio::groupMembers(const LLUUID& id) const
{
    if (const ALGhostGroupModel::Group* group = groupForMember(id))
    {
        return mGroups.authoringUnitMembers(group->mId);
    }

    std::vector<LLUUID> ids;
    if (getInstance(id))
    {
        ids.push_back(id);
    }
    return ids;
}

const ALGhostGroupModel::Group* ALGhostStudio::groupForMember(
    const LLUUID& id) const
{
    if (const ALGhostGroupModel::Group* group = mGroups.findGroup(id))
    {
        return group;
    }
    return mGroups.findGroupForMember(id);
}

ALGhostGroupModel::Group* ALGhostStudio::groupForMember(const LLUUID& id)
{
    if (ALGhostGroupModel::Group* group = mGroups.findGroup(id))
    {
        return group;
    }
    return mGroups.findGroupForMember(id);
}

void ALGhostStudio::setSelected(const LLUUID& id)
{
    ++mSelectionRevision;
    if (const ALGhostGroupModel::Group* group = groupForMember(id);
        group && !group->mEditMembers)
    {
        mSelected = mGroups.representative(group->mId);
        return;
    }
    mSelected = id;
}

S32 ALGhostStudio::lockGroup(const std::vector<LLUUID>& ids, ELockMode mode)
{
    if ((mode != LOCK_RIGID_UNIT && mode != LOCK_MOVE_FACE) ||
        ids.empty())
    {
        return 0;
    }

    std::vector<ALGhostGroupModel::WorldMember> authored;
    std::set<LLUUID> unique;
    authored.reserve(ids.size());
    for (const LLUUID& id : ids)
    {
        const Instance* inst = getInstance(id);
        if (!inst || !unique.insert(id).second || inst->mGroupId.notNull())
        {
            // Regrouping is deliberately explicit: silently dissolving an old
            // crowd made a collapsed group lose its siblings when used as a
            // prototype. Ungroup first, then make the new authoring unit.
            return 0;
        }
        if (inst->mMotion != MOTION_OFF || inst->mMotionHasBase ||
            inst->mChaosEnabled || inst->mChaosHasBase)
        {
            // Group-local motion is not authored yet. Reject instead of
            // creating a group whose visual records can drift away from its
            // authoritative hierarchy.
            return 0;
        }
        ALGhostGroupModel::WorldMember member;
        member.mInstanceId = id;
        member.mWorld.mFoot = inst->mFootGlobal;
        member.mWorld.mRotation = inst->mRotation;
        member.mWorld.mScale = inst->mScale;
        authored.push_back(member);
    }

    LLUUID group;
    group.generate();
    const std::string name = llformat("Crowd %u", mNextGroupNumber);
    const ALGhostGroupModel::EMode group_mode =
        mode == LOCK_MOVE_FACE ? ALGhostGroupModel::MODE_MOVE_FACE
                               : ALGhostGroupModel::MODE_RIGID;
    if (!mGroups.createGroup(group, name, group_mode, authored))
    {
        return 0;
    }

    for (const LLUUID& id : ids)
    {
        Instance* inst = getInstance(id);
        inst->mGroupId = group;
        inst->mLockMode = mode;
    }
    ++mNextGroupNumber;
    if (std::find(ids.begin(), ids.end(), mSelected) != ids.end())
    {
        setSelected(ids.front());
    }
    return (S32)ids.size();
}

S32 ALGhostStudio::ungroup(const LLUUID& id)
{
    const ALGhostGroupModel::Group* group = groupForMember(id);
    if (!group)
    {
        return 0;
    }
    const LLUUID group_id = group->mId;
    const std::vector<LLUUID> ids = mGroups.members(group_id);
    const bool selected_header = mSelected == group_id;
    mGroups.eraseGroup(group_id);
    for (const LLUUID& member : ids)
        if (Instance* inst = getInstance(member))
        {
            inst->mGroupId.setNull();
            inst->mLockMode = LOCK_OFF;
        }
    if (selected_header)
    {
        setSelected(ids.empty() ? LLUUID::null : ids.front());
    }
    return (S32)ids.size();
}

S32 ALGhostStudio::setLockMode(const LLUUID& id, ELockMode mode)
{
    Instance* inst = getInstance(id);
    if (!inst) return 0;
    switch (mode)
    {
    case LOCK_OFF:
        return ungroup(id);
    case LOCK_RIGID_UNIT:
    case LOCK_MOVE_FACE:
        if (inst->mGroupId.isNull())
        {
            return 0;
        }
        {
            const std::vector<LLUUID> members = groupMembers(id);
            const ALGhostGroupModel::EMode group_mode =
                mode == LOCK_MOVE_FACE ? ALGhostGroupModel::MODE_MOVE_FACE
                                       : ALGhostGroupModel::MODE_RIGID;
            if (!mGroups.setMode(inst->mGroupId, group_mode))
            {
                return 0;
            }
            for (const LLUUID& member : members)
                if (Instance* item = getInstance(member)) item->mLockMode = mode;
            if (mSelected.notNull())
            {
                const Instance* selected = getInstance(mSelected);
                if (selected && selected->mGroupId == inst->mGroupId)
                    setSelected(selected->mId);
            }
            return (S32)members.size();
        }
    }
    return 0;
}

bool ALGhostStudio::renameGroup(const LLUUID& id, const std::string& name)
{
    ALGhostGroupModel::Group* group = groupForMember(id);
    return group && mGroups.renameGroup(group->mId, name);
}

bool ALGhostStudio::setGroupEditMembers(const LLUUID& id, bool editing)
{
    ALGhostGroupModel::Group* group = groupForMember(id);
    if (!group || !mGroups.setEditMembers(group->mId, editing))
    {
        return false;
    }
    if (!editing)
    {
        setSelected(mGroups.representative(group->mId));
    }
    return true;
}

bool ALGhostStudio::resetGroupMemberOffset(const LLUUID& id)
{
    const ALGhostGroupModel::Group* group = groupForMember(id);
    if (!group)
    {
        return false;
    }
    const ALGhostGroupModel::Group before = *group;
    return mGroups.resetMemberOffset(id) &&
           applyGroupTransforms(before.mId, &before);
}

bool ALGhostStudio::setGroupMemberPinned(const LLUUID& id, bool pinned)
{
    const ALGhostGroupModel::Group* group = groupForMember(id);
    if (!group)
    {
        return false;
    }
    const LLUUID group_id = group->mId;
    const ALGhostGroupModel::Group before = *group;
    // "Pinned" means keep this custom member-local offset. Unpinning is an
    // explicit return to the authored formation slot, so the toggle has an
    // immediate, visible meaning even before a future group reflow.
    if (!pinned)
    {
        return mGroups.resetMemberOffset(id) &&
               applyGroupTransforms(group_id, &before);
    }
    return mGroups.setMemberPinned(id, true);
}

bool ALGhostStudio::applyGroupTransforms(
    const LLUUID& group_id,
    const ALGhostGroupModel::Group* rollback_model)
{
    const ALGhostGroupModel::Group* group = mGroups.findGroup(group_id);
    if (!group)
    {
        return false;
    }

    struct Application
    {
        Instance* mInstance = nullptr;
        ALGhostGroupModel::Transform mWorld;
        LLVector3d mPreviousFoot;
        LLQuaternion mPreviousRotation;
        F32 mPreviousScale = 1.f;
        ELockMode mPreviousLockMode = LOCK_OFF;
        U64 mPreviousRevision = 0;
        LLGhostAvatar* mRuntime = nullptr;
    };
    std::vector<Application> plan;
    plan.reserve(group->mMembers.size());

    auto restore_model = [this, rollback_model]()
    {
        return !rollback_model ||
               mGroups.restoreGroupSnapshot(*rollback_model);
    };

    // Resolve and validate the complete unit before touching any visible
    // record. Entity runtimes are main-thread objects, so retaining these raw
    // pointers through the immediately following application is safe.
    for (const ALGhostGroupModel::Member& member : group->mMembers)
    {
        Instance* inst = getInstance(member.mInstanceId);
        ALGhostGroupModel::Transform world;
        if (!inst || inst->mGroupId != group_id ||
            !mGroups.resolveMember(member.mInstanceId, world) ||
            !world.isFinite() ||
            world.mScale < GHOST_SCALE_MIN || world.mScale > GHOST_SCALE_MAX ||
            inst->mMotion != MOTION_OFF || inst->mMotionHasBase ||
            inst->mChaosEnabled || inst->mChaosHasBase)
        {
            restore_model();
            return false;
        }
        Application application;
        application.mInstance = inst;
        application.mWorld = world;
        application.mPreviousFoot = inst->mFootGlobal;
        application.mPreviousRotation = inst->mRotation;
        application.mPreviousScale = inst->mScale;
        application.mPreviousLockMode = inst->mLockMode;
        application.mPreviousRevision = inst->mTransformRevision;
        if (inst->mKind == BACKING_ENTITY_CLONE)
        {
            application.mRuntime = resolveEntityClone(inst->mId);
            if (!application.mRuntime)
            {
                restore_model();
                return false;
            }
        }
        plan.push_back(application);
    }

    auto restore_records = [this, &plan]()
    {
        for (Application& application : plan)
        {
            Instance& inst = *application.mInstance;
            inst.mFootGlobal = application.mPreviousFoot;
            inst.mRotation = application.mPreviousRotation;
            inst.mScale = application.mPreviousScale;
            inst.mLockMode = application.mPreviousLockMode;
            inst.mTransformRevision = application.mPreviousRevision;
            if (application.mRuntime)
            {
                application.mRuntime->setEntityScale(inst.mScale);
                applyEntityTransform(inst.mId);
            }
        }
    };

    for (Application& application : plan)
    {
        Instance& inst = *application.mInstance;
        inst.mFootGlobal = application.mWorld.mFoot;
        inst.mRotation = application.mWorld.mRotation;
        inst.mScale = application.mWorld.mScale;
        inst.mLockMode =
            group->mMode == ALGhostGroupModel::MODE_MOVE_FACE
                ? LOCK_MOVE_FACE : LOCK_RIGID_UNIT;
        ++inst.mTransformRevision;
        if (application.mRuntime)
        {
            application.mRuntime->setEntityScale(inst.mScale);
            if (!applyEntityTransform(inst.mId))
            {
                restore_records();
                restore_model();
                return false;
            }
        }
    }
    return plan.size() == group->mMembers.size();
}

bool ALGhostStudio::getUnitTransform(
    const LLUUID& id, LLVector3d& foot, LLQuaternion& rotation, F32& scale) const
{
    const ALGhostGroupModel::Group* direct_group = mGroups.findGroup(id);
    if (const ALGhostGroupModel::Group* group =
            direct_group ? direct_group : groupForMember(id);
        group && (direct_group || !group->mEditMembers))
    {
        foot = group->mWorld.mFoot;
        rotation = group->mWorld.mRotation;
        scale = group->mWorld.mScale;
        return foot.isFinite() && rotation.isFinite() && llfinite(scale);
    }
    const Instance* inst = getInstance(id);
    if (!inst)
    {
        return false;
    }
    foot = inst->mFootGlobal;
    rotation = inst->mRotation;
    scale = inst->mScale;
    return foot.isFinite() && rotation.isFinite() && llfinite(scale);
}

bool ALGhostStudio::getGroupScaleLimits(
    const LLUUID& id, F32& minimum, F32& maximum) const
{
    const ALGhostGroupModel::Group* group = groupForMember(id);
    return group && group_scale_limits(*group, minimum, maximum);
}

bool ALGhostStudio::transformGroup(
    const LLUUID& id, const LLVector3d& foot,
    const LLQuaternion& rotation, F32 scale)
{
    ALGhostGroupModel::Group* direct_group = mGroups.findGroup(id);
    ALGhostGroupModel::Group* group =
        direct_group ? direct_group : groupForMember(id);
    if (!group || (!direct_group && group->mEditMembers) ||
        !foot.isFinite() ||
        !rotation.isFinite() || !llfinite(scale))
    {
        return false;
    }

    F32 minimum_scale = 0.f;
    F32 maximum_scale = 0.f;
    if (!group_scale_limits(*group, minimum_scale, maximum_scale) ||
        scale < minimum_scale || scale > maximum_scale)
    {
        return false;
    }

    ALGhostGroupModel::Transform requested;
    requested.mFoot = foot;
    requested.mRotation = rotation;
    requested.mScale = scale;
    const ALGhostGroupModel::Group before = *group;
    return mGroups.setGroupTransform(group->mId, requested) &&
           applyGroupTransforms(before.mId, &before);
}

bool ALGhostStudio::transformUnit(const LLUUID& id, const LLVector3d& foot,
                                  const LLQuaternion& rotation, F32 scale)
{
    Instance* anchor = getInstance(id);
    if (!anchor || !foot.isFinite() || !rotation.isFinite() || !llfinite(scale))
        return false;
    LLQuaternion requested_rot = rotation;
    requested_rot.normalize();
    if (!requested_rot.isFinite())
    {
        return false;
    }
    const F32 clamped_scale = llclamp(scale, GHOST_SCALE_MIN, GHOST_SCALE_MAX);
    ALGhostGroupModel::Group* group = groupForMember(id);
    if (!group)
    {
        anchor->setTransform(foot, requested_rot);
        anchor->setScale(clamped_scale);
        if (anchor->mKind == BACKING_ENTITY_CLONE)
        {
            if (LLGhostAvatar* ghost = resolveEntityClone(anchor->mId))
                ghost->setEntityScale(anchor->mScale);
            applyEntityTransform(anchor->mId);
        }
        return true;
    }

    ALGhostGroupModel::Transform requested;
    requested.mFoot = foot;
    requested.mRotation = requested_rot;
    requested.mScale = clamped_scale;
    if (group->mEditMembers)
    {
        const ALGhostGroupModel::Group before = *group;
        return mGroups.setMemberWorld(id, requested, true) &&
               applyGroupTransforms(before.mId, &before);
    }

    if (group->mMode == ALGhostGroupModel::MODE_RIGID)
    {
        const F32 desired_ratio =
            clamped_scale / llmax(GHOST_SCALE_MIN, anchor->mScale);
        F32 minimum_ratio = 0.f;
        F32 maximum_ratio = 1.0e30f;
        F32 minimum_pair = 1.0e30f;
        std::vector<LLVector3d> feet;
        for (const LLUUID& member_id : groupMembers(id))
        {
            if (const Instance* member = getInstance(member_id))
            {
                minimum_ratio = llmax(
                    minimum_ratio,
                    GHOST_SCALE_MIN / llmax(GHOST_SCALE_MIN, member->mScale));
                maximum_ratio = llmin(
                    maximum_ratio,
                    GHOST_SCALE_MAX / llmax(GHOST_SCALE_MIN, member->mScale));
                feet.push_back(member->mFootGlobal);
            }
        }
        for (size_t a = 0; a + 1 < feet.size(); ++a)
        {
            for (size_t b = a + 1; b < feet.size(); ++b)
            {
                const F32 distance = (F32)(feet[a] - feet[b]).length();
                if (distance > 0.0001f)
                {
                    minimum_pair = llmin(minimum_pair, distance);
                }
            }
        }
        if (desired_ratio < 1.f)
        {
            minimum_ratio = llmax(
                minimum_ratio,
                minimum_pair < 1.0e30f
                    ? llmin(1.f, 0.1f / minimum_pair) : 1.f);
        }
        const F32 effective_ratio = minimum_ratio <= maximum_ratio
            ? llclamp(desired_ratio, minimum_ratio, maximum_ratio) : 1.f;
        requested.mScale = anchor->mScale * effective_ratio;
    }
    const ALGhostGroupModel::Group before = *group;
    if (!mGroups.transformFromMember(id, requested))
    {
        return false;
    }
    return applyGroupTransforms(before.mId, &before);
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
    const auto entity_count = [this]()
    {
        return (S32)std::count_if(
            mInstances.begin(), mInstances.end(),
            [](const Instance& inst)
            {
                return inst.mKind == BACKING_ENTITY_CLONE;
            });
    };
    const S32 before = entity_count();

    std::vector<LLUUID> standalone;
    std::set<LLUUID> group_ids;
    for (const Instance& inst : mInstances)
    {
        if (inst.mKind == BACKING_ENTITY_CLONE)
        {
            if (inst.mGroupId.notNull())
            {
                group_ids.insert(inst.mGroupId);
            }
            else
            {
                standalone.push_back(inst.mId);
            }
        }
    }

    // A mixed backing group is first dissolved, then only its entity records
    // are removed. This makes the command's "overlays preserved" contract
    // literal instead of letting removeInstance() delete the entire group.
    for (const LLUUID& group_id : group_ids)
    {
        const ALGhostGroupModel::Group* group =
            mGroups.findGroup(group_id);
        if (!group)
        {
            continue;
        }
        const std::vector<LLUUID> members =
            mGroups.members(group_id);
        std::vector<LLUUID> entities;
        for (const LLUUID& member_id : members)
        {
            const Instance* member = getInstance(member_id);
            if (member && member->mKind == BACKING_ENTITY_CLONE)
            {
                entities.push_back(member_id);
            }
        }
        if (entities.empty())
        {
            continue;
        }
        if (entities.size() == members.size())
        {
            // Whole-entity crowds retain normal atomic group deletion.
            removeInstance(entities.front());
            continue;
        }

        if (ungroup(group_id) != (S32)members.size())
        {
            continue;
        }
        for (const LLUUID& entity_id : entities)
        {
            const Instance* current = getInstance(entity_id);
            if (current && current->mKind == BACKING_ENTITY_CLONE &&
                current->mGroupId.isNull())
            {
                removeInstance(entity_id);
            }
        }
    }

    for (const LLUUID& id : standalone)
    {
        const Instance* current = getInstance(id);
        if (current && current->mKind == BACKING_ENTITY_CLONE &&
            current->mGroupId.isNull())
        {
            removeInstance(id);
        }
    }
    return llmax(0, before - entity_count());
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
    F32 pelvis_to_foot = av->getPelvisToFoot();
    if (!llfinite(pelvis_to_foot))
    {
        pelvis_to_foot = 0.f;
    }
    foot.mV[VZ] -= llmax(0.f, pelvis_to_foot);
    inst->mFrozenFootAgent = foot;
    inst->mFrozenHeadValid = false;
    if (LLJoint* head = av->getJoint("mHead"))
    {
        const LLVector3 position = head->getWorldPosition();
        if (position.isFinite())
        {
            inst->mFrozenHeadAgent = position;
            inst->mFrozenHeadValid = true;
        }
    }
    inst->mPose = POSE_FROZEN;
    return true;
}

void ALGhostStudio::unfreezeInstance(const LLUUID& id)
{
    if (Instance* inst = getInstance(id))
    {
        inst->mPose = POSE_LIVE;
        inst->mFrozenHeadValid = false;
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
    EFormation formation, F32 parameter, const FormationOptions& options) const
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

    auto finish = [&](FormationSlot value, const LLVector3d& tangent = LLVector3d())
    {
        // Organic wobble is layered after the regular shape. Separate hash
        // channels make changing facing independent of position.
        if (slot > 0 && options.mJitter > 0.f)
        {
            const F32 ja = F_TWO_PI * seeded_unit(p.mId,
                options.mSeed * 131u + 700u + (U32)slot * 2u);
            const F32 jr = spacing * llclamp(options.mJitter, 0.f, 1.f) *
                seeded_unit(p.mId, options.mSeed * 131u + 701u + (U32)slot * 2u);
            value.mFoot += LLVector3d(cosf(ja), sinf(ja), 0.0) * (F64)jr;
        }
        if (options.mTerrainConform && formation != FORMATION_STAIRCASE)
        {
            LLVector3 agent = gAgent.getPosAgentFromGlobal(value.mFoot);
            // Presence of a region, rather than a non-zero height, distinguishes
            // unavailable land data from perfectly valid sea-level terrain.
            if (LLWorld::getInstance()->getRegionFromPosAgent(agent))
            {
                agent.mV[VZ] = LLWorld::getInstance()->resolveLandHeightAgent(agent);
                value.mFoot = gAgent.getPosGlobalFromAgent(agent);
            }
        }
        const LLVector3d delta = value.mFoot - p.mFootGlobal;
        auto yaw_to = [](const LLVector3d& d, F32 fallback)
        {
            return (fabs(d.mdV[VX]) + fabs(d.mdV[VY]) > 0.0001)
                ? atan2f((F32)d.mdV[VY], (F32)d.mdV[VX]) : fallback;
        };
        switch (options.mFacing)
        {
        case FACING_SOURCE: value.mYaw = yaw; value.mAuthorYaw = false; break;
        case FACING_CAMERA:
        {
            const LLVector3d camera = gAgent.getPosGlobalFromAgent(
                LLViewerCamera::getInstance()->getOrigin());
            value.mYaw = yaw_to(camera - value.mFoot, yaw); value.mAuthorYaw = true; break;
        }
        case FACING_CENTROID_IN:
            value.mYaw = yaw_to(p.mFootGlobal - value.mFoot, yaw); value.mAuthorYaw = true; break;
        case FACING_OUTWARD:
            value.mYaw = yaw_to(delta, yaw); value.mAuthorYaw = true; break;
        case FACING_WORLD_POINT:
            // Null/unpicked is a deliberate no-target state: preserve source
            // yaw instead of accidentally aiming every clone at global origin.
            value.mYaw = p.mLookPointGlobal.isExactlyZero()
                ? yaw : yaw_to(p.mLookPointGlobal - value.mFoot, yaw);
            value.mAuthorYaw = true; break;
        case FACING_PATH_HEADING:
            value.mYaw = yaw_to(tangent, value.mYaw); value.mAuthorYaw = true; break;
        case FACING_MIRROR_SOURCE:
            value.mYaw = yaw + F_PI; value.mAuthorYaw = true; break;
        case FACING_TARGET_ACTOR:
        {
            LLVector3d target;
            if (resolveTargetGlobal(p, p.mLookTarget, p.mLookTargetId,
                                    p.mLookPointGlobal, target))
                value.mYaw = yaw_to(target - value.mFoot, yaw);
            value.mAuthorYaw = true; break;
        }
        case FACING_RANDOM:
            value.mYaw = F_TWO_PI * seeded_unit(p.mId,
                900u + (U32)slot);
            value.mAuthorYaw = true; break;
        case FACING_AUTHOR:
        default: break;
        }
        return value;
    };

    if (slot <= 0 && formation != FORMATION_PERIMETER_LINE)
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
        return finish(r);
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
        return finish(r);
    }
    case FORMATION_GRID:
    {
        const S32 cols = llmax(1, (S32)ceilf(sqrtf((F32)count)));
        r.mFoot += forward * (F64)(spacing * (F32)(slot / cols)) +
                   left    * (F64)(spacing * (F32)(slot % cols));
        r.mAuthorYaw = false;
        return finish(r);
    }
    case FORMATION_STAGGERED_ROWS:
    {
        const S32 cols = llmax(1, (S32)ceilf(sqrtf((F32)count)));
        const S32 row = slot / cols;
        const S32 column = slot % cols;
        const F32 stagger = (row & 1) ? 0.5f : 0.f;
        r.mFoot += forward * (F64)(spacing * (F32)row) +
                   left * (F64)(spacing * ((F32)column + stagger));
        r.mAuthorYaw = false;
        return finish(r);
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
        return finish(r);
    }
    case FORMATION_SPIRAL:
    {
        // Golden angle: successive slots never line up into visible spokes.
        const F32 golden = 2.39996323f;
        const F32 a = yaw + golden * (F32)slot;
        const F32 rad = spacing * sqrtf((F32)slot);
        r.mFoot += LLVector3d(cosf(a), sinf(a), 0.0) * (F64)rad;
        r.mYaw = a;
        return finish(r);
    }
    case FORMATION_STAIRCASE:
    {
        // parameter != 0 (not > 0) so a NEGATIVE rise gives a DESCENDING
        // staircase. The old > 0 test made descending stairs impossible.
        const F32 rise = (parameter != 0.f) ? parameter : spacing * 0.5f;
        r.mFoot += forward * (F64)(spacing * (F32)slot) +
                   LLVector3d(0.0, 0.0, (F64)(rise * (F32)slot));
        r.mAuthorYaw = false;
        return finish(r);
    }
    case FORMATION_TUNNEL:
    {
        const F32 side = (slot & 1) ? 1.f : -1.f;
        const S32 rank = (slot + 1) / 2;
        r.mFoot += forward * (F64)((F32)rank * spacing) +
                   left    * (F64)(side * spacing * 0.5f);
        r.mYaw = yaw - side * F_PI_BY_TWO;   // rows face inward at each other
        return finish(r);
    }
    case FORMATION_SCATTER:
    {
        const F32 radius = parameter > 0.f ? parameter : spacing * sqrtf((F32)count);
        // Vogel/sunflower distribution: even area filling, with seed rotating
        // the whole deterministic pattern rather than returning to clumpy RNG.
        const F32 phase = F_TWO_PI * seeded_unit(p.mId, options.mSeed + 100u);
        const F32 a = phase + 2.39996323f * (F32)slot;
        F32 u = ((F32)slot + 0.5f) / (F32)llmax(1, count);
        if (options.mCenterWeighted) u *= u;
        F32 rad = radius * sqrtf(u);
        if (options.mMinDistance > 0.f)
            rad = llmax(rad, options.mMinDistance * sqrtf((F32)slot));
        r.mFoot += LLVector3d(cosf(a), sinf(a), 0.0) * (F64)rad;
        r.mAuthorYaw = false;
        return finish(r);
    }
    case FORMATION_AMPHITHEATER:
    {
        const S32 row = (S32)floorf(sqrtf((F32)slot));
        const S32 first = row * row;
        const S32 in_row = slot - first;
        const S32 row_count = llmax(1, row * 2 + 1);
        const F32 sweep = (parameter > 0.f ? parameter : 140.f) * DEG_TO_RAD;
        const F32 a = yaw + F_PI + sweep *
            ((F32)in_row / (F32)llmax(1, row_count - 1) - 0.5f);
        const F32 rad = spacing * (F32)(row + 1);
        r.mFoot += LLVector3d(cosf(a), sinf(a), 0.0) * (F64)rad;
        r.mFoot.mdV[VZ] += spacing * 0.35 * row;
        r.mYaw = yaw; r.mAuthorYaw = true;
        return finish(r);
    }
    case FORMATION_WEDGE:
    {
        const S32 row = (S32)floorf((sqrtf(8.f * slot + 1.f) - 1.f) * 0.5f);
        const S32 first = row * (row + 1) / 2;
        const F32 across = (F32)(slot - first) - (F32)row * 0.5f;
        r.mFoot += forward * (F64)(spacing * row) + left * (F64)(spacing * across);
        r.mAuthorYaw = false; return finish(r);
    }
    case FORMATION_CONCENTRIC_RINGS:
    {
        const S32 ring = llmax(1, (S32)ceilf(sqrtf((F32)slot)));
        const S32 before = (ring - 1) * (ring - 1);
        const S32 on_ring = llmax(1, ring * ring - before);
        const F32 a = yaw + F_TWO_PI * (F32)(slot - before) / (F32)on_ring;
        r.mFoot += LLVector3d(cosf(a), sinf(a), 0.0) * (F64)(spacing * ring);
        r.mYaw = a; return finish(r);
    }
    case FORMATION_CHECKERBOARD:
    {
        const S32 cols = llmax(1, (S32)ceilf(sqrtf((F32)count * 2.f)));
        const S32 n = slot * 2;
        r.mFoot += forward * (F64)(spacing * (n / cols)) +
                   left * (F64)(spacing * (n % cols));
        r.mAuthorYaw = false; return finish(r);
    }
    case FORMATION_CRESCENT:
    {
        const F32 sweep = (parameter > 0.f ? parameter : 210.f) * DEG_TO_RAD;
        const F32 t = (F32)slot / (F32)llmax(1, count - 1);
        const F32 a = yaw + F_PI - sweep * 0.5f + sweep * t;
        const F32 rad = spacing * (2.f + sinf(F_PI * t));
        r.mFoot += LLVector3d(cosf(a), sinf(a), 0.0) * (F64)rad;
        r.mYaw = a + F_PI; return finish(r);
    }
    case FORMATION_PERIMETER_LINE:
    {
        const F32 side = spacing * (F32)count * 0.25f;
        const F32 half = side * 0.5f;
        const F32 distance = spacing * (F32)slot;
        const S32 edge = llmin(3, (S32)floorf(distance / side));
        const F32 along = distance - side * (F32)edge;
        if (edge == 0) r.mFoot += forward * (F64)half + left * (F64)(-half + along);
        else if (edge == 1) r.mFoot += forward * (F64)(half - along) + left * (F64)half;
        else if (edge == 2) r.mFoot += forward * (F64)-half + left * (F64)(half - along);
        else r.mFoot += forward * (F64)(-half + along) + left * (F64)-half;
        return finish(r);
    }
    case FORMATION_PATH:
    {
        LLActorMover::Path path;
        const LLActorMover::Path* source_path = LLActorMover::instance().getPath(p.mSource);
        if (!source_path || source_path->mNodes.size() < 2) return finish(r);
        path = *source_path; path.rebuild();
        LLVector3d tangent;
        path.evalAtDistance(path.mTotalLength * (F32)slot /
                            (F32)llmax(1, count - 1), r.mFoot, tangent);
        r.mYaw = atan2f((F32)tangent.mdV[VY], (F32)tangent.mdV[VX]);
        return finish(r, tangent);
    }
    case FORMATION_CLUSTERS:
    {
        const S32 groups = llclamp((S32)(parameter > 0.f ? parameter : 3.f), 1, count);
        const S32 group = slot % groups, rank = slot / groups;
        const F32 ca = yaw + F_TWO_PI * (F32)group / (F32)groups;
        const LLVector3d centre = p.mFootGlobal +
            LLVector3d(cosf(ca), sinf(ca), 0.0) * (F64)(spacing * 2.5f);
        const F32 a = 2.39996323f * rank +
            F_TWO_PI * seeded_unit(p.mId, 1200u + group);
        r.mFoot = centre + LLVector3d(cosf(a), sinf(a), 0.0) *
            (F64)(spacing * 0.45f * sqrtf((F32)rank));
        return finish(r);
    }
    case FORMATION_LINE:
    default:
        r.mFoot += forward * (F64)(spacing * (F32)slot);
        r.mAuthorYaw = false;
        return finish(r);
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
                                          std::vector<FormationSlot>& out,
                                          const FormationOptions& options) const
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
        out.push_back(formationSlotAt(*src, i, count, spacing, formation, parameter, options));
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
                             EFormation formation, F32 parameter,
                             const FormationOptions& options)
{
    Instance* src = getInstance(id);
    if (!src || count < 2 || spacing < 0.05f)
    {
        return 0;
    }
    // capture by value: push_back below reallocates and would dangle `src`
    const Instance proto = *src;
    std::vector<LLUUID> created;
    created.push_back(id);
    if (formation == FORMATION_PERIMETER_LINE)
    {
        const FormationSlot first =
            formationSlotAt(proto, 0, count, spacing, formation, parameter, options);
        src->setFootGlobal(first.mFoot);
        if (first.mAuthorYaw) src->setYaw(first.mYaw);
        if (src->mKind == BACKING_ENTITY_CLONE) applyEntityTransform(src->mId);
    }
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
                formationSlotAt(proto, i, count, spacing, formation, parameter, options);
            Instance copy = proto;
            copy.mId.generate();
            copy.mName = makeDefaultName();
            copy.mGroupId.setNull();
            copy.mLockMode = LOCK_OFF;
            copy.mFootGlobal = s.mFoot;
            if (s.mAuthorYaw)
            {
                copy.setYaw(s.mYaw);
            }
            mInstances.push_back(copy);
            created.push_back(copy.mId);
            ++made;
        }
        if (options.mLockMode != LOCK_OFF) lockGroup(created, options.mLockMode);
        return made;
    }

    auto append = [this, &proto, &made, &created](const LLVector3d& foot, F32 yaw,
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
        created.push_back(copy->mId);
    };
    // Every formation now comes from formationSlotAt(), the SINGLE source of
    // truth that the in-world preview also uses. Previously this function had
    // its own inline geometry per formation and formationSlot() had a second,
    // DIFFERENT one -- so a preview could never have matched the build.
    for (S32 i = 1; i < count; ++i)
    {
        const FormationSlot s =
            formationSlotAt(proto, i, count, spacing, formation, parameter, options);
        append(s.mFoot, s.mYaw, s.mAuthorYaw);
    }
    if (options.mLockMode != LOCK_OFF) lockGroup(created, options.mLockMode);
    return made;
}

bool ALGhostStudio::solverShape(EFormation formation,
                                ALFormationSolver::Shape& shape)
{
    using Shape = ALFormationSolver::Shape;
    switch (formation)
    {
    case FORMATION_LINE:              shape = Shape::Row; return true;
    case FORMATION_GRID:              shape = Shape::Grid; return true;
    case FORMATION_STAGGERED_ROWS:    shape = Shape::StaggeredRows; return true;
    case FORMATION_RING:              shape = Shape::Ring; return true;
    case FORMATION_ARC:               shape = Shape::Arc; return true;
    case FORMATION_CONCENTRIC_RINGS:  shape = Shape::ConcentricRings; return true;
    case FORMATION_SCATTER:           shape = Shape::Organic; return true;
    case FORMATION_SPIRAL:            shape = Shape::Spiral; return true;
    case FORMATION_SPLIT_ROW:         shape = Shape::SplitRow; return true;
    case FORMATION_SOUL_TRAIN:        shape = Shape::SoulTrain; return true;
    case FORMATION_CHEVRON:           shape = Shape::Chevron; return true;
    case FORMATION_ZIGZAG:            shape = Shape::Zigzag; return true;
    case FORMATION_HORSESHOE:         shape = Shape::Horseshoe; return true;
    case FORMATION_INFINITY:          shape = Shape::Infinity; return true;
    case FORMATION_PENTAGRAM:         shape = Shape::Pentagram; return true;
    case FORMATION_STAR_OUTLINE:      shape = Shape::StarOutline; return true;
    case FORMATION_DIAMOND:           shape = Shape::Diamond; return true;
    case FORMATION_CROSS:             shape = Shape::Cross; return true;
    case FORMATION_ARROW:             shape = Shape::Arrow; return true;
    case FORMATION_HEART:             shape = Shape::Heart; return true;
    case FORMATION_SUNBURST:          shape = Shape::Sunburst; return true;
    case FORMATION_BRIGADE:           shape = Shape::Brigade; return true;
    case FORMATION_SOUL_TRAIN_MILITARY:
        shape = Shape::SoulTrainMilitary; return true;
    case FORMATION_STAGGERED_MILITARY:
        shape = Shape::StaggeredMilitary; return true;
    case FORMATION_RING_BRIGADE:
        shape = Shape::RingBrigade; return true;

    // Persisted legacy values remain valid enum values for freeze strips, but
    // they are deliberately not selectable in the reliable crowd builder.
    case FORMATION_V:
    case FORMATION_STAIRCASE:
    case FORMATION_TUNNEL:
    case FORMATION_AMPHITHEATER:
    case FORMATION_WEDGE:
    case FORMATION_CHECKERBOARD:
    case FORMATION_CRESCENT:
    case FORMATION_PERIMETER_LINE:
    case FORMATION_PATH:
    case FORMATION_CLUSTERS:
        return false;
    }
    return false;
}

std::string ALGhostStudio::formationRole(EFormation formation, U32 slot)
{
    const char* family = "Slot";
    switch (formation)
    {
    case FORMATION_LINE:             family = "Row"; break;
    case FORMATION_GRID:             family = "Grid"; break;
    case FORMATION_STAGGERED_ROWS:   family = "Stagger"; break;
    case FORMATION_SPLIT_ROW:        family = "Split row"; break;
    case FORMATION_SOUL_TRAIN:       family = "Soul Train"; break;
    case FORMATION_RING:             family = "Ring"; break;
    case FORMATION_ARC:              family = "Arc"; break;
    case FORMATION_CONCENTRIC_RINGS: family = "Ring"; break;
    case FORMATION_SCATTER:          family = "Organic"; break;
    case FORMATION_SPIRAL:           family = "Spiral"; break;
    case FORMATION_CHEVRON:          family = "Chevron"; break;
    case FORMATION_ZIGZAG:           family = "Zigzag"; break;
    case FORMATION_HORSESHOE:        family = "Horseshoe"; break;
    case FORMATION_INFINITY:         family = "Infinity"; break;
    case FORMATION_PENTAGRAM:        family = "Pentagram"; break;
    case FORMATION_STAR_OUTLINE:     family = "Star"; break;
    case FORMATION_DIAMOND:          family = "Diamond"; break;
    case FORMATION_CROSS:            family = "Cross"; break;
    case FORMATION_ARROW:            family = "Arrow"; break;
    case FORMATION_HEART:            family = "Heart"; break;
    case FORMATION_SUNBURST:         family = "Ray"; break;
    case FORMATION_BRIGADE:          family = "Brigade"; break;
    case FORMATION_SOUL_TRAIN_MILITARY: family = "Corridor"; break;
    case FORMATION_STAGGERED_MILITARY:  family = "Rank"; break;
    case FORMATION_RING_BRIGADE:        family = "Ring"; break;
    default: break;
    }
    return llformat("%s %u", family, slot + 1);
}

bool ALGhostStudio::beginCrowdPlacement(
    const LLUUID& owner_id, const LLUUID& prototype_id,
    const FormationSpec& spec, const LLVector3d& anchor,
    F32 yaw, F32 height)
{
    assert_main_thread();
    const Instance* source = getInstance(prototype_id);
    if (mCrowdCommitInProgress || owner_id.isNull() || !source ||
        !anchor.isFinite() ||
        !llfinite(yaw) || !llfinite(height))
    {
        return false;
    }
    // The active owner retains acquisition. A same-owner call is an explicit
    // restart with a fresh immutable snapshot/session; another panel must
    // cancel or wait for the owner to finish.
    if (mCrowdDraft.mActive && mCrowdDraft.mOwnerId != owner_id)
    {
        return false;
    }

    CrowdPlacementDraft next;
    next.mActive = true;
    next.mOwnerId = owner_id;
    next.mSessionId.generate();
    next.mSpec = spec;
    next.mAnchor = anchor;
    next.mYaw = yaw;
    next.mHeight = height;
    next.mRevision = mNextCrowdDraftRevision++;
    next.mSource.mInstanceId = source->mId;
    next.mSource.mSourceId = source->mSource;
    next.mSource.mEntityId = source->mEntityId;
    next.mSource.mGroupId = source->mGroupId;
    next.mSource.mKind = source->mKind;
    next.mSource.mFoot = source->mFootGlobal;
    next.mSource.mRotation = source->mRotation;
    next.mSource.mScale = source->mScale;
    next.mSource.mTransformRevision = source->mTransformRevision;
    mCrowdDraft = std::move(next);

    if (source->mGroupId.notNull())
    {
        mCrowdDraft.mValidation.mMessage =
            "Ungroup this crowd before using one member as a prototype.";
        return true;
    }
    std::string source_reason;
    if (!valid_crowd_source(*source, &source_reason))
    {
        mCrowdDraft.mValidation.mMessage = source_reason;
        return true;
    }
    resolveCrowdPlacement();
    return true;
}

bool ALGhostStudio::updateCrowdPlacementSpec(
    const LLUUID& owner_id, const FormationSpec& spec)
{
    if (mCrowdCommitInProgress || !mCrowdDraft.mActive ||
        owner_id != mCrowdDraft.mOwnerId)
    {
        return false;
    }
    mCrowdDraft.mSpec = spec;
    mCrowdDraft.mRevision = mNextCrowdDraftRevision++;
    return resolveCrowdPlacement();
}

bool ALGhostStudio::updateCrowdPlacementTransform(
    const LLUUID& owner_id, const LLVector3d& anchor, F32 yaw, F32 height)
{
    if (mCrowdCommitInProgress || !mCrowdDraft.mActive ||
        owner_id != mCrowdDraft.mOwnerId ||
        !anchor.isFinite() || !llfinite(yaw) || !llfinite(height))
    {
        return false;
    }
    mCrowdDraft.mAnchor = anchor;
    mCrowdDraft.mYaw = yaw;
    mCrowdDraft.mHeight = height;
    mCrowdDraft.mRevision = mNextCrowdDraftRevision++;
    return resolveCrowdPlacement();
}

bool ALGhostStudio::setCrowdPlacementPinned(
    const LLUUID& owner_id, bool pinned)
{
    if (mCrowdCommitInProgress || !mCrowdDraft.mActive ||
        owner_id != mCrowdDraft.mOwnerId)
    {
        return false;
    }
    mCrowdDraft.mPinned = pinned;
    mCrowdDraft.mRevision = mNextCrowdDraftRevision++;
    return true;
}

bool ALGhostStudio::cancelCrowdPlacement(const LLUUID& owner_id)
{
    if (mCrowdCommitInProgress || !mCrowdDraft.mActive ||
        (owner_id.notNull() && owner_id != mCrowdDraft.mOwnerId))
    {
        return false;
    }
    mCrowdDraft = CrowdPlacementDraft();
    return true;
}

bool ALGhostStudio::crowdPlacementSourceUnchanged(std::string* reason) const
{
    auto reject = [reason](const char* text)
    {
        if (reason) *reason = text;
        return false;
    };
    if (!mCrowdDraft.mActive)
    {
        return reject("No crowd placement is active.");
    }
    if (!mShowAll)
    {
        return reject(
            "Turn on Show all before building a visible crowd.");
    }
    const PlacementSourceSnapshot& snap = mCrowdDraft.mSource;
    const Instance* source = getInstance(snap.mInstanceId);
    if (!source)
    {
        return reject("The prototype was removed.");
    }
    std::string invalid_reason;
    if (!valid_crowd_source(*source, &invalid_reason))
    {
        if (reason)
        {
            *reason = invalid_reason;
        }
        return false;
    }
    if (source->mSource != snap.mSourceId ||
        source->mEntityId != snap.mEntityId ||
        source->mKind != snap.mKind)
    {
        return reject("The prototype backing changed; stage it again.");
    }
    if (source->mGroupId != snap.mGroupId)
    {
        return reject("The prototype group changed; stage it again.");
    }
    const LLVector3d delta = source->mFootGlobal - snap.mFoot;
    LLQuaternion current_rotation = source->mRotation;
    LLQuaternion captured_rotation = snap.mRotation;
    current_rotation.normalize();
    captured_rotation.normalize();
    const bool same_rotation =
        current_rotation.isFinite() && captured_rotation.isFinite() &&
        fabsf(dot(current_rotation, captured_rotation)) >= 0.999999f;
    if (source->mTransformRevision != snap.mTransformRevision ||
        delta.lengthSquared() > 1.0e-10 ||
        !same_rotation || fabsf(source->mScale - snap.mScale) > 0.00001f)
    {
        return reject("The prototype moved or changed scale; stage it again.");
    }
    if (snap.mGroupId.notNull())
    {
        return reject("Ungroup the prototype before building a crowd.");
    }
    if (reason) reason->clear();
    return true;
}

bool ALGhostStudio::resolveCrowdPlacement()
{
    using namespace ALFormationSolver;
    CrowdPlacementDraft& draft = mCrowdDraft;
    draft.mSlots.clear();
    draft.mOverflowMemberIds.clear();
    draft.mInputFingerprint = 0;
    draft.mSolutionFingerprint = 0;
    draft.mValidation = PlacementValidation();
    if (!draft.mActive || draft.mSource.mGroupId.notNull())
    {
        draft.mValidation.mMessage = draft.mSource.mGroupId.notNull()
            ? "Ungroup this crowd before using one member as a prototype."
            : "No crowd placement is active.";
        return false;
    }
    std::string source_reason;
    if (!crowdPlacementSourceUnchanged(&source_reason))
    {
        draft.mValidation.mMessage = source_reason;
        return false;
    }
    if (draft.mSpec.mCount < 2 ||
        draft.mSpec.mCount > (S32)MAX_MEMBER_COUNT)
    {
        draft.mValidation.mSolverStatus = Status::InvalidInput;
        draft.mValidation.mRequestedCount =
            draft.mSpec.mCount > 0 ? (U32)draft.mSpec.mCount : 0;
        draft.mValidation.mMessage = llformat(
            "Crowd size must be between 2 and %u.",
            MAX_MEMBER_COUNT);
        return false;
    }
    if ((draft.mSpec.mGuideShape != GUIDE_OFF &&
         draft.mSpec.mGuideShape != GUIDE_CIRCLE &&
         draft.mSpec.mGuideShape != GUIDE_SQUARE) ||
        !llfinite(draft.mSpec.mGuideSize) ||
        draft.mSpec.mGuideSize < 0.f ||
        draft.mSpec.mGuideSize > (F32)MAX_INPUT_METERS)
    {
        draft.mValidation.mSolverStatus = Status::InvalidInput;
        draft.mValidation.mRequestedCount = (U32)draft.mSpec.mCount;
        draft.mValidation.mMessage =
            "The visual guide shape or size is invalid.";
        return false;
    }
    switch (draft.mSpec.mFacing)
    {
    case FACING_AUTHOR:
    case FACING_SOURCE:
    case FACING_CAMERA:
    case FACING_CENTROID_IN:
    case FACING_OUTWARD:
    case FACING_WORLD_POINT:
    case FACING_PATH_HEADING:
    case FACING_MIRROR_SOURCE:
    case FACING_TARGET_ACTOR:
    case FACING_RANDOM:
    case FACING_AISLE:
    case FACING_TRACK_SUBJECT:
    case FACING_FACE_ACROSS:
    case FACING_CLUSTER_IN:
    case FACING_LOOSE:
    case FACING_CHAIN:
    case FACING_WAVE:
        break;
    default:
        draft.mValidation.mSolverStatus = Status::InvalidInput;
        draft.mValidation.mMessage = "The selected facing mode is invalid.";
        return false;
    }
    if (draft.mSpec.mLockMode != LOCK_OFF &&
        draft.mSpec.mLockMode != LOCK_RIGID_UNIT &&
        draft.mSpec.mLockMode != LOCK_MOVE_FACE)
    {
        draft.mValidation.mSolverStatus = Status::InvalidInput;
        draft.mValidation.mMessage = "The selected group mode is invalid.";
        return false;
    }
    if ((draft.mSpec.mFacing == FACING_WORLD_POINT ||
         draft.mSpec.mFacing == FACING_TARGET_ACTOR ||
         draft.mSpec.mFacing == FACING_TRACK_SUBJECT) &&
        (!draft.mSpec.mHasFacingPoint ||
         !draft.mSpec.mFacingPointGlobal.isFinite()))
    {
        draft.mValidation.mSolverStatus = Status::InvalidInput;
        draft.mValidation.mMessage =
            "Pick a valid facing point before placing this crowd.";
        return false;
    }
    if (draft.mSpec.mFacing == FACING_PATH_HEADING)
    {
        draft.mValidation.mSolverStatus = Status::UnsupportedShape;
        draft.mValidation.mMessage =
            "Path heading is not available in reliable crowd placement yet.";
        return false;
    }

    Shape shape = Shape::Row;
    if (!solverShape(draft.mSpec.mFormation, shape))
    {
        draft.mValidation.mSolverStatus = Status::UnsupportedShape;
        draft.mValidation.mRequestedCount = (U32)draft.mSpec.mCount;
        draft.mValidation.mMessage =
            "That legacy formation is not available in reliable crowd placement.";
        return false;
    }

    Request request;
    request.mShape = shape;
    // Crowd staging always fits the complete requested count. The guide is a
    // visual measuring aid and can never turn into a clipping envelope.
    request.mPolicy = EnvelopePolicy::FitCount;
    request.mRadiusMeters = 0.0;
    request.mCenterSpacingMeters = draft.mSpec.mCenterSpacing;
    request.mEdgeGapMeters = draft.mSpec.mEdgeGap;
    request.mJitterMeters = draft.mSpec.mJitter;
    request.mYawRadians = draft.mYaw;
    request.mGridColumns = draft.mSpec.mGridColumns;
    request.mGridRows = draft.mSpec.mGridRows;
    request.mColumnSpacingMeters = draft.mSpec.mFileSpacing;
    request.mRowSpacingMeters = draft.mSpec.mRankSpacing;
    request.mArcSweepRadians = draft.mSpec.mArcSweepDegrees * DEG_TO_RAD;
    request.mLaneGapMeters = draft.mSpec.mLaneGap;
    request.mChevronAngleDegrees = draft.mSpec.mChevronAngleDegrees;
    request.mHorseshoeOpeningDegrees =
        draft.mSpec.mHorseshoeOpeningDegrees;
    request.mSpiralTurns = draft.mSpec.mSpiralTurns;
    request.mAspectRatio = draft.mSpec.mAspectRatio;
    request.mRayCount = draft.mSpec.mRayCount;
    request.mZigzagColumns = draft.mSpec.mZigzagColumns;
    request.mRankOffsetFraction = draft.mSpec.mRankOffsetFraction;
    request.mSeed = draft.mSpec.mSeed;
    request.mMembers.reserve((size_t)draft.mSpec.mCount);
    for (S32 i = 0; i < draft.mSpec.mCount; ++i)
    {
        Member member;
        member.mId = (U64)i + 1;
        member.mFootprintRadiusMeters = 0.0;
        request.mMembers.push_back(member);
    }

    const Result solved = solve(request);
    PlacementValidation& validation = draft.mValidation;
    validation.mSolverStatus = solved.mStatus;
    validation.mRequestedCount = solved.mRequestedCount;
    validation.mPlacedCount = solved.mPlacedCount;
    validation.mOverflowCount = solved.mOverflowCount;
    validation.mEnvelopeRadius = (F32)solved.mEnvelopeRadiusMeters;
    validation.mRequiredRadius = (F32)solved.mRequiredRadiusMeters;
    validation.mPlacedRadius = (F32)solved.mPlacedRadiusMeters;
    validation.mActualMinEdgeGap = solved.mHasPairwiseGap
        ? (F32)solved.mActualMinEdgeGapMeters : 0.f;
    validation.mPartial = false;
    validation.mMessage = solved.mDiagnostic;
    draft.mInputFingerprint = solved.mInputFingerprint;
    draft.mSolutionFingerprint = solved.mSolutionFingerprint;
    draft.mOverflowMemberIds = solved.mOverflowMemberIds;

    const U32 requested = (U32)draft.mSpec.mCount;
    if (solved.mStatus != Status::Ok ||
        solved.mRequestedCount != requested ||
        solved.mPlacedCount != requested ||
        solved.mOverflowCount != 0 ||
        solved.mSlots.size() != (size_t)requested ||
        !solved.mOverflowMemberIds.empty())
    {
        if (validation.mMessage.empty() ||
            solved.mStatus == Status::Ok)
        {
            validation.mMessage =
                "The solver did not return the complete requested crowd.";
        }
        draft.mOverflowMemberIds.clear();
        return false;
    }
    if (!draft.mAnchor.isFinite() || !llfinite(draft.mHeight) ||
        !llfinite(draft.mYaw))
    {
        validation.mMessage = "The placement anchor is not finite.";
        return false;
    }

    const LLVector3d camera_global = gAgent.getPosGlobalFromAgent(
        LLViewerCamera::getInstance()->getOrigin());
    // The cluster's authored orientation is a PURE world-up (Z) heading. The
    // feet pattern is already a flat, vertically-spun layout (the solver's yaw
    // is an in-plane rotation), so forcing the base body orientation to Z as
    // well makes the wheel/middle-drag spin the whole cluster strictly about
    // world vertical, around the anchor pivot -- never about a tilted body
    // axis. Previously the base was draft.mSource.mRotation composed with the
    // heading delta, which retained the prototype's pitch/roll: a clone that
    // was free-rotated with the 3-axis manip (or whose source root was not
    // upright) leaked that tilt into every member and made the cluster appear
    // to rotate off-axis / wobble as the heading changed. FACING_SOURCE
    // (handled below) remains the explicit opt-in that reproduces the
    // prototype's full pitch/roll.
    LLQuaternion authored(draft.mYaw, LLVector3::z_axis);
    authored.normalize();

    auto yaw_to = [this](const LLVector3d& from, const LLVector3d& to,
                         F32 fallback)
    {
        const LLVector3d delta = to - from;
        return fabs(delta.mdV[VX]) + fabs(delta.mdV[VY]) > 0.000001
            ? atan2f((F32)delta.mdV[VY], (F32)delta.mdV[VX])
            : fallback;
    };

    draft.mSlots.reserve(solved.mSlots.size());
    for (const Slot& source_slot : solved.mSlots)
    {
        ResolvedFormationSlot slot;
        slot.mMemberId = source_slot.mMemberId;
        slot.mInputIndex = source_slot.mInputIndex;
        slot.mFoot = draft.mAnchor +
            LLVector3d(source_slot.mLocalX, source_slot.mLocalY, 0.0);
        slot.mFoot.mdV[VZ] += draft.mHeight;
        slot.mScale = draft.mSource.mScale;
        slot.mRole = formationRole(draft.mSpec.mFormation,
                                  source_slot.mInputIndex);

        if (draft.mSpec.mTerrainConform)
        {
            LLVector3 agent = gAgent.getPosAgentFromGlobal(slot.mFoot);
            if (!LLWorld::getInstance()->getRegionFromPosAgent(agent))
            {
                draft.mSlots.clear();
                validation.mCanCommit = false;
                validation.mMessage =
                    "Terrain data is unavailable for one or more slots.";
                return false;
            }
            agent.mV[VZ] =
                LLWorld::getInstance()->resolveLandHeightAgent(agent) +
                draft.mHeight;
            slot.mFoot = gAgent.getPosGlobalFromAgent(agent);
        }

        F32 facing_yaw = draft.mYaw;
        bool pure_yaw = false;
        switch (draft.mSpec.mFacing)
        {
        case FACING_SOURCE:
            slot.mRotation = draft.mSource.mRotation;
            break;
        case FACING_CAMERA:
            facing_yaw = yaw_to(slot.mFoot, camera_global, draft.mYaw);
            pure_yaw = true;
            break;
        case FACING_CENTROID_IN:
            facing_yaw = yaw_to(slot.mFoot, draft.mAnchor, draft.mYaw);
            pure_yaw = true;
            break;
        case FACING_OUTWARD:
            facing_yaw = yaw_to(draft.mAnchor, slot.mFoot, draft.mYaw);
            pure_yaw = true;
            break;
        case FACING_WORLD_POINT:
        case FACING_TARGET_ACTOR:
        case FACING_TRACK_SUBJECT:
            if (draft.mSpec.mHasFacingPoint)
            {
                facing_yaw = yaw_to(slot.mFoot,
                                    draft.mSpec.mFacingPointGlobal,
                                    draft.mYaw);
                pure_yaw = true;
            }
            break;
        case FACING_AISLE:
        case FACING_FACE_ACROSS:
        {
            const LLVector3d axis(cosf(draft.mYaw), sinf(draft.mYaw), 0.0);
            const LLVector3d relative = slot.mFoot - draft.mAnchor;
            const LLVector3d nearest = draft.mAnchor +
                axis * (relative.mdV[VX] * axis.mdV[VX] +
                        relative.mdV[VY] * axis.mdV[VY]);
            facing_yaw = yaw_to(slot.mFoot, nearest, draft.mYaw);
            pure_yaw = true;
            break;
        }
        case FACING_CLUSTER_IN:
        {
            const U32 first = (source_slot.mInputIndex / 4U) * 4U;
            const U32 last = llmin(first + 4U, (U32)solved.mSlots.size());
            LLVector3d centroid;
            for (U32 member = first; member < last; ++member)
            {
                centroid += draft.mAnchor + LLVector3d(
                    solved.mSlots[member].mLocalX,
                    solved.mSlots[member].mLocalY, 0.0);
            }
            centroid /= (F64)(last - first);
            facing_yaw = yaw_to(slot.mFoot, centroid, draft.mYaw);
            pure_yaw = true;
            break;
        }
        case FACING_LOOSE:
            facing_yaw = yaw_to(slot.mFoot, draft.mAnchor, draft.mYaw) +
                (seeded_unit(draft.mSource.mInstanceId,
                    draft.mSpec.mSeed + 1300u + source_slot.mInputIndex) -
                 0.5f) * (30.f * DEG_TO_RAD);
            pure_yaw = true;
            break;
        case FACING_CHAIN:
            if (source_slot.mInputIndex > 0)
            {
                const Slot& previous =
                    solved.mSlots[source_slot.mInputIndex - 1];
                facing_yaw = yaw_to(slot.mFoot,
                    draft.mAnchor + LLVector3d(previous.mLocalX,
                                              previous.mLocalY, 0.0),
                    draft.mYaw);
                pure_yaw = true;
            }
            break;
        case FACING_WAVE:
            facing_yaw = draft.mYaw +
                sinf((F32)source_slot.mInputIndex * 0.65f) *
                (35.f * DEG_TO_RAD);
            pure_yaw = true;
            break;
        case FACING_MIRROR_SOURCE:
            facing_yaw = draft.mYaw + F_PI;
            pure_yaw = true;
            break;
        case FACING_RANDOM:
            facing_yaw = F_TWO_PI * seeded_unit(
                draft.mSource.mInstanceId,
                draft.mSpec.mSeed + 900u + source_slot.mInputIndex);
            pure_yaw = true;
            break;
        case FACING_PATH_HEADING:
        case FACING_AUTHOR:
        default:
            break;
        }
        if (draft.mSpec.mFacing != FACING_SOURCE)
        {
            slot.mRotation = pure_yaw
                ? LLQuaternion(facing_yaw, LLVector3::z_axis) : authored;
        }
        slot.mRotation.normalize();
        const F32 slot_rotation_norm =
            dot(slot.mRotation, slot.mRotation);
        if (!slot.mFoot.isFinite() || !slot.mRotation.isFinite() ||
            !llfinite(slot_rotation_norm) ||
            fabsf(slot_rotation_norm - 1.f) > 0.001f ||
            !llfinite(slot.mScale) ||
            slot.mScale < GHOST_SCALE_MIN || slot.mScale > GHOST_SCALE_MAX)
        {
            draft.mSlots.clear();
            validation.mMessage =
                "A resolved placement transform was invalid.";
            return false;
        }
        draft.mSlots.push_back(std::move(slot));
    }

    validation.mCanCommit =
        validation.mSolverStatus == Status::Ok &&
        validation.mPlacedCount == validation.mRequestedCount &&
        validation.mRequestedCount == (U32)draft.mSpec.mCount &&
        validation.mOverflowCount == 0 &&
        draft.mSlots.size() == (size_t)validation.mRequestedCount &&
        draft.mOverflowMemberIds.empty();
    if (!validation.mCanCommit)
    {
        validation.mMessage = "The complete crowd preview is not available.";
    }
    else
    {
        validation.mMessage = llformat(
            "%u previewed · fitted radius %.2f m · edge gap %.2f m",
            validation.mPlacedCount, validation.mPlacedRadius,
            validation.mActualMinEdgeGap);
    }
    return validation.mCanCommit;
}

ALGhostStudio::PlacementCommitResult ALGhostStudio::commitCrowdPlacement(
    const LLUUID& owner_id, const LLUUID& session_id)
{
    assert_main_thread();
    PlacementCommitResult result;
    if (mCrowdCommitInProgress)
    {
        result.mMessage = "Crowd creation is already in progress.";
        return result;
    }
    if (!mCrowdDraft.mActive || owner_id != mCrowdDraft.mOwnerId ||
        session_id != mCrowdDraft.mSessionId)
    {
        result.mMessage = "This placement session is no longer active.";
        return result;
    }
    if (!mCrowdDraft.mValidation.mCanCommit ||
        mCrowdDraft.mValidation.mSolverStatus !=
            ALFormationSolver::Status::Ok ||
        mCrowdDraft.mValidation.mPlacedCount !=
            mCrowdDraft.mValidation.mRequestedCount ||
        mCrowdDraft.mValidation.mOverflowCount != 0 ||
        mCrowdDraft.mSlots.size() !=
            (size_t)mCrowdDraft.mValidation.mRequestedCount)
    {
        result.mMessage = mCrowdDraft.mValidation.mMessage.empty()
            ? "The placement is not valid." : mCrowdDraft.mValidation.mMessage;
        return result;
    }
    std::string stale_reason;
    if (!crowdPlacementSourceUnchanged(&stale_reason))
    {
        result.mMessage = stale_reason;
        return result;
    }

    mCrowdCommitInProgress = true;
    struct CommitGuard
    {
        bool& mFlag;
        ~CommitGuard() { mFlag = false; }
    } commit_guard{mCrowdCommitInProgress};

    // Copy the complete immutable draft before any vector reallocation.
    const CrowdPlacementDraft draft = mCrowdDraft;
    const Instance* live_prototype =
        getInstance(draft.mSource.mInstanceId);
    if (!live_prototype)
    {
        result.mMessage = "The prototype was removed.";
        return result;
    }
    const Instance prototype = *live_prototype;
    std::vector<LLUUID> ids;
    ids.reserve(draft.mSlots.size());
    ids.push_back(draft.mSource.mInstanceId);
    std::vector<LLUUID> created;
    created.reserve(draft.mSlots.size() - 1);

    auto restore_source = [this, &draft]()
    {
        Instance* source = getInstance(draft.mSource.mInstanceId);
        if (!source) return;
        // Transaction-only restoration: do not rewrite motion/chaos authoring
        // baselines and do not invent a revision for a failed operation.
        source->mFootGlobal = draft.mSource.mFoot;
        source->mRotation = draft.mSource.mRotation;
        source->mScale = draft.mSource.mScale;
        source->mTransformRevision = draft.mSource.mTransformRevision;
        if (source->mKind == BACKING_ENTITY_CLONE)
        {
            if (LLGhostAvatar* ghost = resolveEntityClone(source->mId))
            {
                ghost->setEntityScale(source->mScale);
            }
            applyEntityTransform(source->mId);
        }
    };
    auto rollback = [this, &draft, &created, &restore_source](
                        bool source_was_changed)
    {
        if (source_was_changed)
        {
            restore_source();
        }
        for (auto it = created.rbegin(); it != created.rend(); ++it)
        {
            removeInstance(*it);
        }
    };

    // Allocate every backing first. A failed entity-clone spawn leaves the
    // prototype untouched and removes all earlier allocations.
    for (size_t i = 1; i < draft.mSlots.size(); ++i)
    {
        Instance* copy = duplicateInstanceSnapshotInPlace(prototype);
        if (!copy)
        {
            rollback(false);
            result.mMessage = llformat(
                "Crowd creation stopped at member %u; no changes were kept.",
                (U32)i + 1);
            return result;
        }
        created.push_back(copy->mId);
        ids.push_back(copy->mId);
    }

    // Runtime creation may pump viewer consumers. Placement-draft mutations
    // are guarded, but source edits are independently revalidated before any
    // stored transform is applied.
    if (!mCrowdDraft.mActive ||
        mCrowdDraft.mOwnerId != owner_id ||
        mCrowdDraft.mSessionId != session_id ||
        !crowdPlacementSourceUnchanged(&stale_reason))
    {
        rollback(false);
        result.mMessage = stale_reason.empty()
            ? "The placement session changed during crowd creation."
            : stale_reason;
        return result;
    }

    // Apply only stored world transforms. Copies are positioned first so any
    // copy/runtime failure leaves the prototype byte-for-byte untouched.
    bool source_was_changed = false;
    auto apply_slot = [this, &ids, &draft, &source_was_changed](
                          size_t i) -> bool
    {
        Instance* member = getInstance(ids[i]);
        const ResolvedFormationSlot& slot = draft.mSlots[i];
        const F32 rotation_norm = dot(slot.mRotation, slot.mRotation);
        if (!member || !slot.mFoot.isFinite() ||
            !slot.mRotation.isFinite() || !llfinite(rotation_norm) ||
            fabsf(rotation_norm - 1.f) > 0.001f ||
            !llfinite(slot.mScale) ||
            slot.mScale < GHOST_SCALE_MIN || slot.mScale > GHOST_SCALE_MAX)
        {
            return false;
        }
        member->mFootGlobal = slot.mFoot;
        member->mRotation = slot.mRotation;
        member->mScale = slot.mScale;
        ++member->mTransformRevision;
        if (i == 0)
        {
            source_was_changed = true;
        }
        if (member->mKind == BACKING_ENTITY_CLONE)
        {
            LLGhostAvatar* ghost = resolveEntityClone(member->mId);
            if (!ghost)
            {
                return false;
            }
            ghost->setEntityScale(member->mScale);
            if (!applyEntityTransform(member->mId))
            {
                return false;
            }
        }
        return true;
    };
    for (size_t i = 1; i < ids.size(); ++i)
    {
        if (!apply_slot(i))
        {
            rollback(false);
            result.mMessage =
                "A crowd copy could not be positioned; no changes were kept.";
            return result;
        }
    }
    if (!mCrowdDraft.mActive ||
        mCrowdDraft.mOwnerId != owner_id ||
        mCrowdDraft.mSessionId != session_id ||
        !crowdPlacementSourceUnchanged(&stale_reason))
    {
        rollback(false);
        result.mMessage = stale_reason.empty()
            ? "The placement session changed before the prototype moved."
            : stale_reason;
        return result;
    }
    if (!apply_slot(0))
    {
        rollback(source_was_changed);
        result.mMessage =
            "The prototype could not be positioned; no changes were kept.";
        return result;
    }
    const Instance* placed_source =
        getInstance(draft.mSource.mInstanceId);
    const ResolvedFormationSlot& source_slot = draft.mSlots.front();
    LLQuaternion placed_rotation = placed_source
        ? placed_source->mRotation : LLQuaternion();
    LLQuaternion expected_rotation = source_slot.mRotation;
    const F32 placed_rotation_magnitude = placed_rotation.normalize();
    const F32 expected_rotation_magnitude = expected_rotation.normalize();
    const bool source_still_exact =
        placed_source &&
        placed_source->mGroupId.isNull() &&
        placed_source->mTransformRevision ==
            draft.mSource.mTransformRevision + 1 &&
        (placed_source->mFootGlobal - source_slot.mFoot).lengthSquared() <=
            1.0e-10 &&
        llfinite(placed_rotation_magnitude) &&
        llfinite(expected_rotation_magnitude) &&
        fabsf(dot(placed_rotation, expected_rotation)) >= 0.999999f &&
        fabsf(placed_source->mScale - source_slot.mScale) <= 0.00001f;
    if (!source_still_exact ||
        !mCrowdDraft.mActive ||
        mCrowdDraft.mOwnerId != owner_id ||
        mCrowdDraft.mSessionId != session_id)
    {
        // A runtime consumer edited or replaced the prototype after our
        // stored transform was applied. Preserve that newer external state;
        // only the copies belong to this failed transaction.
        rollback(false);
        result.mMessage =
            "The prototype changed during crowd creation; copies were removed.";
        return result;
    }

    if (draft.mSpec.mFacing == FACING_TRACK_SUBJECT ||
        draft.mSpec.mFacing == FACING_WAVE)
    {
        for (size_t i = 0; i < ids.size(); ++i)
        {
            Instance* member = getInstance(ids[i]);
            if (!member) continue;
            member->mCrowdFacing = draft.mSpec.mFacing;
            member->mCrowdSlot = (U32)i;
            member->mCrowdBaseYaw = draft.mYaw;
            member->mCrowdFacingStart = LLTimer::getTotalSeconds();
            if (draft.mSpec.mFacing == FACING_TRACK_SUBJECT)
            {
                member->mKeepFacing = false;
            }
        }
    }

    if (draft.mSpec.mLockMode != LOCK_OFF)
    {
        if (lockGroup(ids, draft.mSpec.mLockMode) != (S32)ids.size())
        {
            rollback(source_was_changed);
            result.mMessage =
                "The crowd could not be grouped; no changes were kept.";
            return result;
        }
        if (const Instance* first = getInstance(ids.front()))
        {
            result.mGroupId = first->mGroupId;
        }
    }

    result.mSuccess = true;
    result.mPlacedCount = (S32)ids.size();
    result.mCreatedCount = (S32)created.size();
    result.mInstanceIds = ids;
    result.mMessage = llformat(
        "Placed %d crowd members.", result.mPlacedCount);
    if (mCrowdDraft.mActive &&
        mCrowdDraft.mOwnerId == owner_id &&
        mCrowdDraft.mSessionId == session_id)
    {
        mCrowdDraft = CrowdPlacementDraft();
    }
    return result;
}

void ALGhostStudio::renderFormationPreview()
{
    static LLCachedControl<bool> show(
        gSavedSettings, "GhostStudioShowFormationPreview", true);
    static LLCachedControl<F32> marker_scale(
        gSavedSettings, "GhostStudioFormationPreviewScale", 1.f);
    static LLCachedControl<F32> stalk_height(
        gSavedSettings, "GhostStudioFormationPreviewHeight", 1.25f);
    static LLCachedControl<F32> guide_width(
        gSavedSettings, "GhostStudioGuideWidthPixels", 4.f);
    static LLCachedControl<S32> overlay_palette(
        gSavedSettings, "GhostStudioOverlayPalette", 0);
    if (!show || !mCrowdDraft.mActive)
    {
        return;
    }

    std::string stale_reason;
    const bool valid =
        mCrowdDraft.mValidation.mCanCommit &&
        crowdPlacementSourceUnchanged(&stale_reason);
    const LLColor4 white(1.f, 1.f, 1.f, 0.98f);
    const LLColor4 dark(0.005f, 0.005f, 0.01f, 1.f);
    // Settings-selectable high-contrast palette for the member markers/numbers
    // and the prototype highlight. The guide stays white and every stroke keeps
    // the thick dark outline below for legibility over any set. Index 0 is the
    // current neon-green look, so the default preview is unchanged.
    LLColor4 member(0.2f, 1.f, 0.2f, 1.f);
    LLColor4 proto(1.f, 0.95f, 0.1f, 1.f);
    switch (llclamp((S32)overlay_palette, 0, 4))
    {
    case 1: // Neon yellow
        member.set(1.f, 0.95f, 0.1f, 1.f);
        proto.set(1.f, 0.55f, 0.05f, 1.f);
        break;
    case 2: // Cyan
        member.set(0.15f, 0.92f, 1.f, 1.f);
        proto.set(1.f, 0.95f, 0.1f, 1.f);
        break;
    case 3: // White
        member.set(1.f, 1.f, 1.f, 1.f);
        proto.set(1.f, 0.6f, 0.12f, 1.f);
        break;
    case 4: // Magenta
        member.set(1.f, 0.2f, 0.85f, 1.f);
        proto.set(1.f, 0.95f, 0.1f, 1.f);
        break;
    case 0: // Neon green (default; unchanged look)
    default:
        break;
    }
    const F32 width = llclamp((F32)guide_width, 7.f, 16.f);
    const F32 scale = llclamp((F32)marker_scale, 0.5f, 3.f);
    const F32 stalk = llclamp((F32)stalk_height, 0.5f, 3.f) * scale;
    const F32 lift = 0.04f;

    ALWorldOverlayViz::ScopedRenderer viz;
    if (!viz.isReady())
    {
        return;
    }
    const ALWorldOverlayViz::StrokeStyle guide(
        white, width, dark, 5.f);
    const ALWorldOverlayViz::StrokeStyle slot_stroke(
        member, width, dark, 5.f);
    const ALWorldOverlayViz::StrokeStyle prototype(
        proto, width + 2.f, dark, 5.5f);
    const ALWorldOverlayViz::FillStyle slot_fill(
        LLColor4(member.mV[VRED], member.mV[VGREEN],
                 member.mV[VBLUE], 0.62f),
        dark, 5.f);
    const ALWorldOverlayViz::FillStyle prototype_fill(
        LLColor4(proto.mV[VRED], proto.mV[VGREEN],
                 proto.mV[VBLUE], 0.72f),
        dark, 5.5f);
    const ALWorldOverlayViz::FillStyle guide_label(
        LLColor4(white.mV[VRED], white.mV[VGREEN],
                 white.mV[VBLUE], 0.34f),
        dark, 3.f);

    LLVector3 guide_center =
        gAgent.getPosAgentFromGlobal(mCrowdDraft.mAnchor);
    guide_center.mV[VZ] += mCrowdDraft.mHeight + lift;
    const F32 guide_size =
        llfinite(mCrowdDraft.mSpec.mGuideSize)
            ? llmax(0.f, mCrowdDraft.mSpec.mGuideSize) : 0.f;
    if (guide_size > 0.f)
    {
        switch (mCrowdDraft.mSpec.mGuideShape)
        {
        case GUIDE_CIRCLE:
            viz.drawRing(guide_center, LLVector3::z_axis,
                         guide_size, guide, 72);
            viz.drawBillboardLabel(
                llformat("GUIDE R %.2fM", guide_size),
                guide_center + LLVector3(
                    guide_size, 0.f, 0.35f * scale),
                13.f * scale, guide_label);
            break;
        case GUIDE_SQUARE:
        {
            const LLQuaternion guide_rotation(
                mCrowdDraft.mYaw, LLVector3::z_axis);
            const LLVector3 x =
                LLVector3(guide_size, 0.f, 0.f) * guide_rotation;
            const LLVector3 y =
                LLVector3(0.f, guide_size, 0.f) * guide_rotation;
            std::vector<LLVector3> square;
            square.reserve(4);
            square.push_back(guide_center - x - y);
            square.push_back(guide_center + x - y);
            square.push_back(guide_center + x + y);
            square.push_back(guide_center - x + y);
            viz.drawPolyline(square, guide, true);
            viz.drawBillboardLabel(
                llformat("GUIDE %.2fM", guide_size),
                guide_center + x + LLVector3(
                    0.f, 0.f, 0.35f * scale),
                13.f * scale, guide_label);
            break;
        }
        case GUIDE_OFF:
        default:
            break;
        }
    }

    for (size_t i = 0; i < mCrowdDraft.mSlots.size(); ++i)
    {
        const ResolvedFormationSlot& slot = mCrowdDraft.mSlots[i];
        LLVector3 foot = gAgent.getPosAgentFromGlobal(slot.mFoot);
        foot.mV[VZ] += lift;
        const bool is_prototype = i == 0;
        const ALWorldOverlayViz::StrokeStyle& stroke =
            is_prototype ? prototype : slot_stroke;
        const ALWorldOverlayViz::FillStyle& fill =
            is_prototype ? prototype_fill : slot_fill;
        const F32 footprint = llmax(0.08f, 0.30f * slot.mScale);
        viz.drawFootprintDisc(
            foot, LLVector3::z_axis, footprint, fill, 40);
        viz.drawRing(
            foot, LLVector3::z_axis, footprint, stroke, 40);
        viz.drawSegment(foot, foot + LLVector3(0.f, 0.f, stalk), stroke);
        const LLVector3 direction =
            LLVector3::x_axis * slot.mRotation;
        viz.drawFacingArrow(
            foot + LLVector3(0.f, 0.f, 0.10f),
            direction, 0.85f * scale, stroke,
            16.f * scale, 13.f * scale);
        viz.drawBillboardNumber(
            (S32)i + 1,
            foot + LLVector3(0.f, 0.f, stalk * 0.55f),
            14.f * scale, fill);
    }

    const std::string status = valid
        ? llformat("PREVIEW %u/%u",
                   mCrowdDraft.mValidation.mPlacedCount,
                   mCrowdDraft.mValidation.mRequestedCount)
        : "PREVIEW INVALID";
    viz.drawBillboardLabel(
        status, guide_center + LLVector3(0.f, 0.f, 1.8f * scale),
        15.f * scale, valid ? slot_fill :
            ALWorldOverlayViz::FillStyle(
                LLColor4(member.mV[VRED], member.mV[VGREEN],
                         member.mV[VBLUE], 0.40f),
                dark, 3.f));
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
