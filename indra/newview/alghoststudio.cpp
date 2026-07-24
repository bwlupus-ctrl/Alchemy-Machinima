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
#include "lljoint.h"
#include "lldrawable.h"          // FORCE_INVISIBLE (entity-clone show/hide)
#include "llspatialpartition.h"     // LLDrawInfo (freeze reads each batch's avatar+skin)
#include "llvoavatar.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"

#include <algorithm>
#include <cstdint>

// ---------------------------------------------------------------------------
ALGhostStudio& ALGhostStudio::instance()
{
    static ALGhostStudio sInstance;
    return sInstance;
}

namespace
{
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
    LLViewerRegion* region = gAgent.getRegion();
    LLVOAvatar* source = LLDirectorCast::instance().resolve(source_id);
    if (!region || !source)
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
    record.mState = STATE_SPAWNING;
    record.mStyle = 1; // informational: normal scene-lit clone
    mInstances.push_back(record);
    const LLUUID stable_id = record.mId;

    LLGhostAvatar* ghost = (LLGhostAvatar*)gObjectList.createObjectViewer(
        LL_PCODE_LEGACY_AVATAR, region, LLViewerObject::CO_FLAG_GHOST_AVATAR);
    if (!ghost || !ghost->cloneAppearanceFrom(source))
    {
        if (ghost)
        {
            ghost->markForDeath();
        }
        removeInstance(stable_id);
        LL_WARNS("GhostStudio") << "entity spawn rolled back" << LL_ENDL;
        return nullptr;
    }

    const LLVector3 pos = source->getPositionAgent() + gAgent.getAtAxis() * 2.5f;
    ghost->setGhostPosition(pos);
    const S32 attachments = ghost->cloneAttachmentsFrom(source);
    if (!ghost->clonedAttachmentsComplete())
    {
        ghost->releaseClonedAttachments();
        ghost->markForDeath();
        removeInstance(stable_id);
        LL_WARNS("GhostStudio") << "entity spawn rolled back: attachment clone incomplete"
                                << LL_ENDL;
        return nullptr;
    }
    Instance* committed = getInstance(stable_id);
    if (!committed)
    {
        ghost->releaseClonedAttachments();
        ghost->markForDeath();
        return nullptr;
    }
    committed->mEntityId = ghost->getID();
    committed->mFootGlobal = gAgent.getPosGlobalFromAgent(pos);
    committed->mRotation = source->getRotation();
    committed->mState = STATE_READY;
    ghost->setGhostRotation(committed->mRotation);
    ghost->setEntityScale(committed->mScale);
    ghost->setEntityDriveMode(committed->mDriveMode, committed->mDirectedAnim);
    ghost->setEntityLook(committed->mLook, committed->mLookAlpha);
    set_entity_clone_visible(ghost, mShowAll && committed->mEnabled);
    LLDirectorCast::instance().add(committed->mEntityId);
    if (LLDirectorCast::CastMember* member =
            LLDirectorCast::instance().getMember(committed->mEntityId))
    {
        member->mLastName = committed->mName;
    }
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
    LLGhostAvatar* old_ghost = resolveEntityClone(id);
    LLVOAvatar* source = LLDirectorCast::instance().resolve(inst->mSource);
    LLViewerRegion* region = gAgent.getRegion();
    if (!old_ghost || old_ghost->isDead() || !source || !region)
    {
        inst->mState = STATE_SOURCE_MISSING;
        LL_WARNS("GhostStudio") << "refreshEntityClone: ghost or source unavailable for "
                                << id << LL_ENDL;
        return false;
    }

    LLGhostAvatar* replacement = (LLGhostAvatar*)gObjectList.createObjectViewer(
        LL_PCODE_LEGACY_AVATAR, region, LLViewerObject::CO_FLAG_GHOST_AVATAR);
    if (!replacement || !replacement->cloneAppearanceFrom(source))
    {
        if (replacement)
        {
            replacement->markForDeath();
        }
        inst->mState = STATE_ERROR;
        LL_WARNS("GhostStudio") << "refreshEntityClone: appearance re-clone failed for "
                                << id << LL_ENDL;
        return false;
    }
    const S32 attachments = replacement->cloneAttachmentsFrom(source);
    if (!replacement->clonedAttachmentsComplete())
    {
        replacement->releaseClonedAttachments();
        replacement->markForDeath();
        inst->mState = STATE_ERROR;
        LL_WARNS("GhostStudio")
            << "refreshEntityClone: replacement attachment clone incomplete for "
            << id << "; existing clone preserved" << LL_ENDL;
        return false;
    }

    // Commit the replacement runtime object to the existing stable record.
    const LLUUID old_runtime_id = inst->mEntityId;
    LLDirectorCast& cast = LLDirectorCast::instance();
    const bool was_subject_a = cast.getSubjectA() == old_runtime_id;
    const bool was_subject_b = cast.getSubjectB() == old_runtime_id;
    inst->mEntityId = replacement->getID();
    replacement->setEntityScale(inst->mScale);
    replacement->setEntityDriveMode(inst->mDriveMode, inst->mDirectedAnim);
    replacement->setEntityLook(inst->mLook, inst->mLookAlpha);
    set_entity_clone_visible(replacement, mShowAll && inst->mEnabled);
    applyEntityTransform(id);

    cast.remove(old_runtime_id);
    cast.add(inst->mEntityId);
    // Director stores entity subjects by their runtime object UUID. A
    // transactional refresh replaces that UUID, so transfer the subject role
    // after remove() clears the old one; otherwise resolveTarget() silently
    // falls back to the unscaled agent avatar.
    if (was_subject_a)
    {
        cast.setSubjectA(inst->mEntityId);
    }
    if (was_subject_b)
    {
        cast.setSubjectB(inst->mEntityId);
    }
    if (LLDirectorCast::CastMember* member =
            LLDirectorCast::instance().getMember(inst->mEntityId))
    {
        member->mLastName = inst->mName;
    }
    set_entity_clone_visible(old_ghost, false);
    old_ghost->releaseClonedAttachments();
    old_ghost->markForDeath();

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
            inst->mState = STATE_ERROR;
            inst->mEntityId.setNull();
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
    return applyEntityTransform(id);
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
            inst.mEntityId.setNull();
            inst.mState = STATE_ERROR;
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
    if (mSelected == id)
    {
        mSelected.setNull();
    }

    // Director transport and actor-reference teardown hooks are intentionally
    // empty until those systems enroll Studio ids (ideas #2+).
    if (inst->mKind == BACKING_ENTITY_CLONE && inst->mEntityId.notNull())
    {
        const LLUUID runtime_id = inst->mEntityId;
        LLDirectorCast::instance().remove(runtime_id);
        LLGhostAvatar* ghost =
            dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(runtime_id));
        if (ghost && !ghost->isDead())
        {
            set_entity_clone_visible(ghost, false);
            ghost->releaseClonedAttachments();
            ghost->markForDeath();
        }
        inst->mEntityId.setNull();
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

// ---------------------------------------------------------------------------
// array helper
// ---------------------------------------------------------------------------
S32 ALGhostStudio::makeArray(const LLUUID& id, S32 count, F32 spacing, bool ring)
{
    Instance* src = getInstance(id);
    if (!src || count < 2 || spacing < 0.05f)
    {
        return 0;
    }
    // capture by value: push_back below reallocates and would dangle `src`
    const Instance proto = *src;
    S32 made = 0;
    if (ring)
    {
        // ring of `count` TOTAL ghosts centred on the prototype; the prototype
        // occupies slot 0, so it stays put and count-1 copies fill the circle.
        // Radius from the chord: spacing between neighbours on the circle.
        const F32 step = F_TWO_PI / (F32)count;
        const F32 radius = spacing / (2.f * sinf(step * 0.5f));
        for (S32 i = 1; i < count; ++i)
        {
            Instance copy = proto;
            copy.mId.generate();
            copy.mName = makeDefaultName();
            const F32 proto_yaw = proto.getYaw();
            const F32 a = proto_yaw + step * (F32)i;
            copy.mFootGlobal += LLVector3d(cosf(a) - cosf(proto_yaw),
                                           sinf(a) - sinf(proto_yaw), 0.0) * (F64)radius;
            // each ghost faces outward from the ring centre, like a crowd
            copy.setYaw(a);
            mInstances.push_back(copy);
            ++made;
        }
    }
    else
    {
        // line of `count` total along the prototype's facing direction
        const F32 proto_yaw = proto.getYaw();
        const LLVector3d dir(cosf(proto_yaw), sinf(proto_yaw), 0.0);
        for (S32 i = 1; i < count; ++i)
        {
            Instance copy = proto;
            copy.mId.generate();
            copy.mName = makeDefaultName();
            copy.mFootGlobal += dir * (F64)(spacing * (F32)i);
            mInstances.push_back(copy);
            ++made;
        }
    }
    return made;
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
