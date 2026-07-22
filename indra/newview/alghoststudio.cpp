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

#include "llactormover.h"           // GhostBatch access for the freeze snapshot
#include "llagent.h"                // agent <-> global conversion
#include "lldirectorcast.h"         // source resolution (cast id / null = self)
#include "lljoint.h"
#include "llspatialpartition.h"     // LLDrawInfo (freeze reads each batch's avatar+skin)
#include "llvoavatar.h"

#include <algorithm>

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
} // anonymous namespace

// ---------------------------------------------------------------------------
// master visibility
// ---------------------------------------------------------------------------
bool ALGhostStudio::anyEnabled() const
{
    if (!mShowAll)
    {
        return false;
    }
    for (const Instance& inst : mInstances)
    {
        if (inst.mEnabled)
        {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// instance CRUD
// ---------------------------------------------------------------------------
ALGhostStudio::Instance* ALGhostStudio::addInstance(const LLUUID& source)
{
    LLVector3 foot;
    if (!source_foot_agent(source, foot))
    {
        return nullptr;     // source not in world: nowhere sensible to spawn
    }
    Instance inst;
    inst.mId.generate();
    inst.mSource = source;
    inst.mFootGlobal = gAgent.getPosGlobalFromAgent(foot);
    mInstances.push_back(inst);
    return &mInstances.back();
}

ALGhostStudio::Instance* ALGhostStudio::duplicateInstance(const LLUUID& id)
{
    Instance* src = getInstance(id);
    if (!src)
    {
        return nullptr;
    }
    Instance copy = *src;       // includes any frozen palette snapshot
    copy.mId.generate();
    // one step to the ghost's LEFT (perpendicular to its yaw) so the copy is
    // immediately visible beside the original instead of hidden inside it
    const F32 yaw = copy.getYaw();
    copy.mFootGlobal += LLVector3d(-sinf(yaw), cosf(yaw), 0.0);
    mInstances.push_back(copy);
    return &mInstances.back();
}

void ALGhostStudio::removeInstance(const LLUUID& id)
{
    mInstances.erase(
        std::remove_if(mInstances.begin(), mInstances.end(),
                       [&id](const Instance& i) { return i.mId == id; }),
        mInstances.end());
    if (mSelected == id)
    {
        mSelected.setNull();    // never leave the shared selection dangling
    }
}

void ALGhostStudio::removeAll()
{
    mInstances.clear();
    mSelected.setNull();
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
        if (inst.mEnabled
            && std::find(out.begin(), out.end(), inst.mSource) == out.end())
        {
            out.push_back(inst.mSource);
        }
    }
}
