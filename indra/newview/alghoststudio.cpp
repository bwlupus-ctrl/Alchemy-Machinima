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
#include "alcinelightrigmanager.h"
#include "alreflectmath.h"
#include "altoolghostedit.h"
#include "alghostanimassetindex.h" // [GhostPanelPerf] shared animation library

// [AutoAnimate] Bounded LLSD XML persistence.
#include "llsdserialize.h"
#include "llfile.h"
#include <sstream>
#include <cerrno>   // [FileSafety] EEXIST
#include <cstdio>

#include "alghostnameplates.h"
#include "alghostspawnengine.h"
#include "alworldoverlayviz.h"
#include "llghostavatar.h"
#include "llcontrolavatar.h"
#include "alghostmotionmath.h"       // [GhostMotionModes] pure per-mode evaluator
#include "llanimationstates.h"      // [GhostMotionModes] built-in hover/jump cues
#include "llpresentationtime.h"     // [GhostMotionModes] scrub-safe motion clock
#include "llactormover.h"           // GhostBatch access for the freeze snapshot
#include "llagent.h"                // agent <-> global conversion
#include "lldirectorcast.h"         // source resolution (cast id / null = self)
#include "llcinematiccamera.h"
#include "lljoint.h"
#include "lldrawable.h"          // FORCE_INVISIBLE (entity-clone show/hide)
#include "llspatialpartition.h"     // LLDrawInfo (freeze reads each batch's avatar+skin)
#include "llvoavatar.h"
#include "llsurface.h"
#include "llsurfacepatch.h"
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

// [AutoAnimate] Zero is an instance sentinel, never a runtime window.
F32 autoanim_window(const ALGhostStudio::Instance& inst)
{
    static LLCachedControl<F32> default_window(gSavedSettings, "GhostAutoAnimateWindow", 120.f);
    const F32 window = inst.mAutoAnimWindow == 0.f ? static_cast<F32>(default_window) : inst.mAutoAnimWindow;
    return std::isfinite(window) ? llclamp(window, 15.f, 600.f) : 120.f;
}
ALGhostAutoAnim::Config autoanim_config(const ALGhostStudio::Instance& inst)
{
    ALGhostAutoAnim::Config config;
    config.mEnabled = inst.mAutoAnimate; config.mInstanceId = inst.mId;
    config.mSeedSalt = inst.mAutoAnimSeedSalt; config.mPool = inst.mAutoAnimPool; config.mManual = inst.mAutoAnimManual;
    // No settings lookup/cache initialization on the disabled runtime path.
    if (inst.mAutoAnimate) config.mWindowSeconds = autoanim_window(inst);
    return config;
}

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
    out_foot = av->getBaseRootPosition();   // [ClientHeight] a clone placed here copies the BASE foot
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
void set_entity_clone_visible(LLGhostAvatar* ghost, bool visible, bool render_only = false)
{
    if (!ghost || ghost->isDead())
    {
        return;
    }
    // Persistent local gate for the avatar pool. FORCE_INVISIBLE alone is not
    // sufficient for already-enrolled rigged faces and can be cleared by
    // ordinary viewer-object update bookkeeping.
    ghost->setEntityCloneVisible(visible || render_only);
    ghost->setRenderOnlyHidden(render_only);
    if (visible || render_only)
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
    if (source.mSourceRemoved) return reject("The prototype source was removed.");
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
    // Setting-based reset, paste, Quick and undo reach the same live update as
    // widget commits. The source seed belongs to the session crowd, not a scene.
    for (const char* name : {"GhostStudioStartMode", "GhostStudioStartSpread", "GhostStudioRippleFrom",
             "GhostStudioLooksMode", "GhostStudioLookSpread", "GhostStudioMixLook1", "GhostStudioMixLook2",
             "GhostStudioMixLook3", "GhostStudioMixLook4", "GhostStudioMixLook5", "GhostStudioMixLook6"})
        if (LLControlVariable* control = gSavedSettings.getControl(name))
            mVariationConnections.emplace_back(control->getSignal()->connect(
                [this, name](LLControlVariable*, const LLSD&, const LLSD&)
                {
                    if (std::string(name).find("GhostStudioMixLook") == 0)
                    {
                        const S32 old = gSavedSettings.getS32(name);
                        const S32 look = remapRetiredOverlayStyle(old);
                        if (old != look)
                        {
                            gSavedSettings.setS32(name, look);
                            return; // the canonical value emits the normal update
                        }
                    }
                    LLUUID id = mSelected;
                    if (const auto* group = groupForMember(id)) id = mGroups.representative(group->mId);
                    if (const auto* inst = getInstance(id)) applyCrowdVariation(id, inst->mCrowdSeed, std::string(name).find("GhostStudioStart") == 0 || std::string(name) == "GhostStudioRippleFrom", std::string(name).find("GhostStudioStart") != 0 && std::string(name) != "GhostStudioRippleFrom");
                }));

    for (S32 i = 1; i <= 6; ++i)
    {
        const std::string name = "GhostStudioMixLook" + std::to_string(i);
        const S32 old = gSavedSettings.getS32(name);
        const S32 look = remapRetiredOverlayStyle(old);
        if (old != look) gSavedSettings.setS32(name, look);
    }

    // Runtime UUID consumers enroll here. Replacement and removal always walk
    // this registry, so adding a new consumer cannot require editing every
    // refresh/death path.
    mRuntimeConsumers.emplace_back(
        [this](const LLUUID& stable_id, const LLUUID& old_id,
               const LLUUID& new_id, bool removing_instance)
        {
            LLDirectorCast& cast = LLDirectorCast::instance();
            const LLUUID destination = removing_instance ? LLUUID::null : new_id.notNull() ? new_id : stable_id;
            // Cast owns the single rekey transaction, including mover/path
            // state. Absence uses the stable id for every authored store.
            cast.replaceActorId(old_id.notNull() ? old_id : stable_id, destination, stable_id);
            if (new_id.notNull() && !removing_instance) cast.add(new_id);
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
    LLVOAvatar* source = resolveCopiedAvatar(inst);
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
    ghost->setDirectedMirror(inst.mDirectedMirror);
    ghost->setStartOffset(inst.mStartOffset);
    if (inst.mHasAnimationSource)
    {
        LLVOAvatar* animation = inst.mAnimationSourceRemoved ? nullptr : LLDirectorCast::instance().resolve(inst.mAnimationSource);
        ghost->setAnimationSource(animation ? animation->getID() : LLUUID::null);
    }
    ghost->setEntityLook(inst.mLook, inst.mLookAlpha);
    set_entity_clone_visible(ghost, isVisible(inst), mShowAll && inst.mEnabled && mSolo && !inst.mSoloVisible);

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
    // [AutoAnimate] The disabled path does not allocate runtime state.
    ghost->setEntityAutoAnimate(autoanim_config(inst));
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
    // [AutoAnimate] Transfer the old runtime's latest progress before applying the new one.
    if (inst.mAutoAnimate)
    {
        if (LLGhostAvatar* old = resolveEntityClone(stable_id))
        {
            auto progress = old->exportAutoAnimProgress();
            if (progress) inst.mAutoAnimPool = std::move(progress);
        }
    }
    if (new_ghost)
    {
        // [ClientHeight] Stamp the STABLE id before the foot is placed:
        // applyEntityRuntimeState() places the foot BEFORE inst.mEntityId is published
        // below, so findInstanceByRuntime() would miss on that first slam.
        new_ghost->setClientHeightKey(stable_id);
        applyEntityRuntimeState(inst, new_ghost);
    }
    mIndicesDirty = true;
    inst.mEntityId = new_runtime;
    ++mInstanceRevision; // [GhostPanelPerf] runtime swap: lifecycle/state/rows may differ
    // Release body-owned gaze captures before the Cast consumer rekeys all
    // authored state. The fresh body's drive mode is already installed, so
    // migrating a live walk can restart its locomotion on that body.
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
            LLDirectorCast::instance().touchCast(); // [GhostPanelPerf]
        }
    }
}

// [AutoAnimate] Studio owns published pools; runtime owns all active decisions.
const ALGhostStudio::Instance* ALGhostStudio::findInstanceByRuntime(const LLUUID& runtime) const
{
    if (runtime.isNull()) return nullptr;
    rebuildIndices();
    const auto it = mRuntimeIndex.find(runtime);
    return it != mRuntimeIndex.end() ? &mInstances[it->second] : nullptr;
}
bool ALGhostStudio::setInstanceAutoAnimate(const LLUUID& id, bool enabled)
{
    Instance* inst = getInstance(id);
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE) return false;
    // Preserve a just-published seal even if OFF arrives before the next Studio frame.
    if (!enabled)
        if (LLGhostAvatar* ghost = resolveEntityClone(id)) ghost->takeAutoAnimPublishedPool(inst->mAutoAnimPool);
    inst->mAutoAnimate = enabled;
    if (!enabled) inst->mAutoAnimManual = false;
    if (LLGhostAvatar* ghost = resolveEntityClone(id)) ghost->setEntityAutoAnimate(autoanim_config(*inst));
    return true;
}
bool ALGhostStudio::setInstanceAutoAnimWindow(const LLUUID& id, F32 seconds)
{
    Instance* inst = getInstance(id);
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE || !std::isfinite(seconds)) return false;
    inst->mAutoAnimWindow = seconds == 0.f ? 0.f : llclamp(seconds, 15.f, 600.f);
    if (LLGhostAvatar* ghost = resolveEntityClone(id)) ghost->setEntityAutoAnimate(autoanim_config(*inst));
    return true;
}
bool ALGhostStudio::autoAnimateAction(const LLUUID& id, ALGhostAutoAnim::Action action)
{
    using ALGhostAutoAnim::Action;
    bool cold = false; // [AutoAnimate] Only an explicit manual TRUE_MIRROR handoff is COLD.
    Instance* inst = getInstance(id);
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE) return false;
    if (action == Action::GoAutonomous)
    {
        ALGhostAutoAnim::LearnStatus status; getAutoAnimateStatus(id, status);
        bool entries = status.mLiveEntries != 0;
        if (inst->mAutoAnimPool)
        {
            entries = entries || !inst->mAutoAnimPool->mBody.entries.empty();
            for (const auto& channel : inst->mAutoAnimPool->mAnimesh) entries = entries || !channel.second.entries.empty();
        }
        if (!inst->mAutoAnimate || !entries || (inst->mDriveMode != DRIVE_MIRROR && inst->mDriveMode != DRIVE_TRUE_MIRROR)) return false;
        if (inst->mDriveMode == DRIVE_TRUE_MIRROR)
        {
            cold = true;
            setInstanceDriveMode(id, DRIVE_MIRROR);
            inst = getInstance(id); if (!inst) return false;
        }
        inst->mAutoAnimManual = true;
    }
    else if (action == Action::FollowSource || action == Action::Relearn) inst->mAutoAnimManual = false;
    if (action == Action::Relearn) { inst->mAutoAnimPool.reset(); inst->mAutoAnimate = true; }
    if (action == Action::Reseed) ++inst->mAutoAnimSeedSalt;
    if (!inst->mAutoAnimate) return false;
    if (LLGhostAvatar* ghost = resolveEntityClone(id))
    {
        ghost->setEntityAutoAnimate(autoanim_config(*inst));
        ghost->requestAutoAnimAction(action, cold);
    }
    return true;
}
bool ALGhostStudio::saveAutoAnimate(const LLUUID& id, const std::string& path, bool exclusive, bool* out_exists)
{
    if (out_exists) *out_exists = false;
    Instance* inst = getInstance(id);
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE) return false;
    if (LLGhostAvatar* ghost = resolveEntityClone(id))
    {
        auto progress = ghost->exportAutoAnimProgress();
        if (progress) inst->mAutoAnimPool = std::move(progress);
    }
    if (!inst->mAutoAnimPool) return false;
    std::ostringstream encoded;
    LLSDSerialize::toPrettyXML(ALGhostAutoAnim::toLLSD(*inst->mAutoAnimPool), encoded);
    const std::string bytes = encoded.str();
    if (bytes.size() > 1024U * 1024U)
    {
        LL_WARNS("GhostStudio") << "AUTOANIM-SAVE reject reason=pool exceeds 1 MB file limit" << LL_ENDL;
        return false;
    }
    if (exclusive)
    {
        // [FileSafety] Atomic create-new: fails (EEXIST) instead of truncating an existing file,
        // so a sibling that appears between the caller's check and now is never lost. Text mode
        // ("wx") matches the llofstream path below.
#if LL_WINDOWS
        FILE* fp = _wfopen(ll_convert_string_to_wide(path).c_str(), L"wx");
#else
        FILE* fp = fopen(path.c_str(), "wx");
#endif
        if (!fp)
        {
            if (out_exists && errno == EEXIST) *out_exists = true;
            return false;
        }
        const bool wrote = fwrite(bytes.data(), 1, bytes.size(), fp) == bytes.size();
        const bool closed = fclose(fp) == 0;
        if (!(wrote && closed))
        {
            LLFile::remove(path); // our own just-created file; do not leave a truncated one
            return false;
        }
        return true;
    }
    llofstream output(path.c_str());
    if (!output.is_open()) return false;
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close(); return !output.fail();
}
bool ALGhostStudio::loadAutoAnimate(const LLUUID& id, const std::string& path)
{
    Instance* inst = getInstance(id);
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE) return false;
    llifstream input(path.c_str(), std::ios::binary | std::ios::ate);
    const auto size = input.is_open() ? input.tellg() : std::streampos(-1);
    std::string reason;
    ALGhostAutoAnim::Pool candidate;
    bool valid = false;
    if (size <= std::streampos(0) || size > std::streampos(1024 * 1024)) reason = "file missing/empty or exceeds 1 MB";
    else
    {
        input.seekg(0, std::ios::beg);
        // Read only the measured bytes, even if another process grows the file.
        std::string bytes(static_cast<size_t>(static_cast<std::streamoff>(size)), '\0');
        input.read(&bytes[0], static_cast<std::streamsize>(bytes.size()));
        std::istringstream bounded(bytes); LLSD data;
        if (input.fail() || LLSDSerialize::fromXML(data, bounded) == LLSDParser::PARSE_FAILURE) reason = "invalid XML";
        else valid = ALGhostAutoAnim::fromLLSD(data, candidate, reason);
    }
    if (!valid)
    {
        LL_WARNS("GhostStudio") << "AUTOANIM-LOAD reject reason=" << reason << LL_ENDL; return false;
    }
    inst->mAutoAnimPool = std::make_shared<const ALGhostAutoAnim::Pool>(std::move(candidate));
    inst->mAutoAnimate = true;
    if (LLGhostAvatar* ghost = resolveEntityClone(id)) ghost->setEntityAutoAnimate(autoanim_config(*inst));
    LL_INFOS("GhostStudio") << "AUTOANIM-LOAD ok inst=" << id << LL_ENDL; return true;
}
void ALGhostStudio::getAutoAnimateStatus(const LLUUID& id, ALGhostAutoAnim::LearnStatus& status) const
{
    status = ALGhostAutoAnim::LearnStatus();
    const Instance* inst = getInstance(id);
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE) return;
    if (LLGhostAvatar* ghost = resolveEntityClone(id)) ghost->getAutoAnimStatus(status);
    status.mOn = inst->mAutoAnimate;
    status.mWindow = autoanim_window(*inst);
    if (!status.mLearning && inst->mAutoAnimPool)
    {
        status.mObserved = inst->mAutoAnimPool->mObservedSeconds;
        status.mBodyStates = 0; status.mBodyOneShots = 0;
        for (const auto& entry : inst->mAutoAnimPool->mBody.entries)
            if (entry.state) ++status.mBodyStates; else ++status.mBodyOneShots;
        status.mAnimeshChannels = static_cast<U32>(inst->mAutoAnimPool->mAnimesh.size());
    }
}
void ALGhostStudio::updateAutoAnimate()
{
    U32 learners = 0, players = 0;
    bool active = false;
    for (Instance& inst : mInstances)
    {
        if (!inst.mAutoAnimate || inst.mKind != BACKING_ENTITY_CLONE) continue;
        active = true;
        if (LLGhostAvatar* ghost = resolveEntityClone(inst.mId))
        {
            ghost->takeAutoAnimPublishedPool(inst.mAutoAnimPool);
            // [AutoAnimate] For an enabled instance using the default window, this also self-heals
            // a missing AutoAnimate runtime: getAutoAnimWindow() returns 0 and the setter recreates it.
            if (inst.mAutoAnimWindow == 0.f && ghost->getAutoAnimWindow() != autoanim_window(inst))
                ghost->setEntityAutoAnimate(autoanim_config(inst)); // [AutoAnimate] Only propagate a changed default.
            ghost->countAutoAnimActivity(learners, players);
        }
    }
    if (active) LLGhostAvatar::logAutoAnimCost(learners, players);
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
    mSolo = false;
    ++mInstanceRevision;
    for (const Instance& inst : mInstances)
    {
        if (inst.mKind == BACKING_ENTITY_CLONE)
        {
            LLGhostAvatar* ghost =
                dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(inst.mEntityId));
            if (ghost && !ghost->isDead())
            {
                set_entity_clone_visible(ghost, isVisible(inst), mShowAll && inst.mEnabled && mSolo && !inst.mSoloVisible);
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
        if (inst.mKind == BACKING_OVERLAY && isVisible(inst))
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
        const std::string candidate = llformat("Clone %d", number);
        const bool used = std::any_of(mInstances.begin(), mInstances.end(),
            [&candidate](const Instance& inst) { return inst.mName == candidate; });
        if (!used)
        {
            return candidate;
        }
    }
}

bool ALGhostStudio::copyAvatarPosition(const LLUUID& source)
{
    LLVector3 foot;
    LLVOAvatar* avatar = LLDirectorCast::instance().resolve(source);
    if (!avatar || !source_foot_agent(source, foot)) return false;
    const LLQuaternion rotation = avatar->getRootJoint()->getWorldRotation();
    if (!rotation.isFinite()) return false;
    mAvatarPositionFoot = gAgent.getPosGlobalFromAgent(foot);
    mAvatarPositionRotation = rotation;
    mHasAvatarPosition = true;
    return true;
}

bool ALGhostStudio::getAvatarPosition(LLVector3d& foot, LLQuaternion& rotation) const
{
    if (!mHasAvatarPosition) return false;
    foot = mAvatarPositionFoot;
    rotation = mAvatarPositionRotation;
    return true;
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
    mIndicesDirty = true;
    mInstances.push_back(inst);
    beginAppearance(inst.mId);
    ++mInstanceRevision; // [GhostPanelPerf]
    return getInstance(inst.mId);
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
    mIndicesDirty = true;
    mInstances.push_back(record);
    ++mInstanceRevision; // [GhostPanelPerf]
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
    beginAppearance(stable_id);
    ++mInstanceRevision; // [GhostPanelPerf]
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
    LLVOAvatar* source = resolveCopiedAvatar(*inst);
    LLGhostAvatar* old_ghost = resolveEntityClone(id);
    if (!source || !gAgent.getRegion())
    {
        ++mInstanceRevision; // [GhostPanelPerf]
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
            ++mInstanceRevision; // [GhostPanelPerf]
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
    ++mInstanceRevision; // [GhostPanelPerf]
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
    copy.mOverlayAvatar.setNull();
    copy.mDeleting = false;
    copy.mFadeAlpha = 1.f;
    copy.mOffsetPose.reset();
    copy.mName = makeDefaultName();
    copy.mGroupId.setNull();
    copy.mCrowdId.setNull();
    copy.mLockMode = LOCK_OFF;
    // one step to the ghost's LEFT (perpendicular to its yaw) so the copy is
    // immediately visible beside the original instead of hidden inside it
    const F32 yaw = copy.getYaw();
    copy.mFootGlobal += LLVector3d(-sinf(yaw), cosf(yaw), 0.0);
    mIndicesDirty = true;
    mInstances.push_back(copy);
    beginAppearance(copy.mId);
    ++mInstanceRevision; // [GhostPanelPerf]
    return getInstance(copy.mId);
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
    if (proto.mDeleting) return nullptr;
    if (proto.mKind == BACKING_OVERLAY)
    {
        Instance copy = proto;
        copy.mId.generate();
        copy.mOverlayAvatar.setNull();
        copy.mDeleting = false;
        copy.mFadeAlpha = 1.f;
        copy.mOffsetPose.reset();
        copy.mName = makeDefaultName();
        copy.mGroupId.setNull();
    copy.mCrowdId.setNull();
        copy.mLockMode = LOCK_OFF;
        mIndicesDirty = true;
    mInstances.push_back(copy);
        beginAppearance(copy.mId);
        ++mInstanceRevision; // [GhostPanelPerf]
        return getInstance(copy.mId);
    }

    // Entity records cannot share mEntityId: create a fresh local clone, then
    // transfer the authored instance state while preserving its runtime keys.
    if (proto.mSourceRemoved) return nullptr;
    if (proto.mHasAnimationSource && proto.mAppearanceSourceRemoved) return nullptr;
    Instance* copy = spawnEntityClone(proto.mHasAnimationSource ? proto.mAppearanceSource : proto.mSource, proto.mSourceLabel);
    if (!copy)
    {
        return nullptr;
    }
    const LLUUID new_id = copy->mId;
    const LLUUID entity_id = copy->mEntityId;
    const ELifecycleState state = copy->mState;
    const std::string new_name = copy->mName; // spawn already called makeDefaultName()
    *copy = proto;
    copy->mReflectMirrorPosition = false;
    copy->mReflectFloor = false;
    copy->mMirrorLightRig = false;
    copy->mReflectPlaneValid = false;
    copy->mReflectHasLastPlane = false;
    copy->mReflectWasDriven = false;
    copy->mId = new_id;
    copy->mDeleting = false;
    copy->mFadeAlpha = 1.f;
    copy->mOffsetPose.reset();
    mIndicesDirty = true;
    copy->mEntityId = entity_id;
    copy->mState = state;
    copy->mName = new_name;
    copy->mGroupId.setNull();
    copy->mCrowdId.setNull();
    copy->mLockMode = LOCK_OFF;
    ++mInstanceRevision; // [GhostPanelPerf] the record was overwritten with the prototype's row fields
    if (entity_clone_count(mInstances) > ENTITY_PHYSICS_CROWD_THRESHOLD)
    {
        copy->mPhysicsEnabled = false;
    }
    copy->mWasDirectorSubjectA = false;
    copy->mWasDirectorSubjectB = false;
    copy->mWasDirectorSubjectC = false;
    copy->mWasDirectorSubjectD = false;
    copy->setTransform(proto.mFootGlobal, proto.mRotation);
    copy->setScale(proto.mScale);
    // [AutoAnimate] Copies share only sealed pools, before any runtime configuration.
    if (copy->mAutoAnimPool && !copy->mAutoAnimPool->mSealed) copy->mAutoAnimPool.reset();
    if (LLGhostAvatar* ghost = resolveEntityClone(copy->mId))
    {
        applyEntityRuntimeState(*copy, ghost);
    }
    beginAppearance(new_id);
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
    ++mInstanceRevision; // [GhostPanelPerf] a group now exists in the model

    const ELockMode lock_mode =
        source_group.mMode == ALGhostGroupModel::MODE_MOVE_FACE
            ? LOCK_MOVE_FACE : LOCK_RIGID_UNIT;
    for (const LLUUID& created_id : created)
    {
        Instance* copy = getInstance(created_id);
        if (!copy)
        {
            mGroups.eraseGroup(clone_group_id);
            ++mInstanceRevision; // [GhostPanelPerf]
            rollback();
            return result;
        }
        copy->mGroupId = clone_group_id;
        if (copy->mCrowdId.notNull()) copy->mCrowdId = clone_group_id;
        copy->mLockMode = lock_mode;
    }
    ++mInstanceRevision; // [GhostPanelPerf] group created + members adopted
    if (!applyGroupTransforms(clone_group_id, nullptr))
    {
        mGroups.eraseGroup(clone_group_id);
        ++mInstanceRevision; // [GhostPanelPerf] group erased on rollback
        for (const LLUUID& created_id : created)
        {
            if (Instance* copy = getInstance(created_id))
            {
                copy->mGroupId.setNull();
    copy->mCrowdId.setNull();
                copy->mLockMode = LOCK_OFF;
            }
        }
        rollback();
        return result;
    }
    result = created;
    ++mInstanceRevision; // [GhostPanelPerf]
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
    ++mInstanceRevision; // [GhostPanelPerf]
    if (inst->mKind == BACKING_ENTITY_CLONE && inst->mEntityId.notNull())
    {
        if (LLDirectorCast::CastMember* member =
                LLDirectorCast::instance().getMember(inst->mEntityId))
        {
            member->mLastName = name;
            LLDirectorCast::instance().touchCast(); // [GhostPanelPerf]
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
                if (item->mEnabled != enabled)
                {
                    ++mInstanceRevision; // [GhostPanelPerf] only on a real change (callers may re-assert per frame)
                }
                item->mEnabled = enabled;
                if (item->mKind == BACKING_ENTITY_CLONE)
                    if (LLGhostAvatar* ghost = resolveEntityClone(member))
                        set_entity_clone_visible(ghost, isVisible(*item), mShowAll && item->mEnabled && mSolo && !item->mSoloVisible);
                changed = true;
            }
        return changed;
    }
    if (inst->mEnabled != enabled)
    {
        ++mInstanceRevision; // [GhostPanelPerf] enable dot
    }
    inst->mEnabled = enabled;
    if (inst->mKind == BACKING_ENTITY_CLONE)
    {
        LLGhostAvatar* ghost =
            dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(inst->mEntityId));
        if (!ghost || ghost->isDead())
        {
            onEntityRuntimeReplaced(*inst, LLUUID::null);
            ++mInstanceRevision; // [GhostPanelPerf] state -> recoverable
            inst->mState = STATE_RECOVERABLE;
            return false;
        }
        set_entity_clone_visible(ghost, isVisible(*inst), mShowAll && inst->mEnabled && mSolo && !inst->mSoloVisible);
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

static F32 clone_capture_radius(LLVOAvatar* av, const LLVector3& pivot)
{
    const LLVector3* ext = av->getLastAnimExtents();
    if (!ext[0].isFinite() || !ext[1].isFinite()) return 2.f;
    return llmax(2.f, ((ext[0] + ext[1]) * 0.5f - pivot).length()
                       + (ext[1] - ext[0]).length() * 0.5f);
}

bool ALGhostStudio::refreshPoseCadence(const LLUUID& id, F64 now)
{
    Instance* inst = getInstance(id);
    if (!inst || inst->mPose != POSE_LIVE || inst->mPoseRateHz <= 0.f)
        return false;
    const bool need_bounds = inst->mStyle == 64 || inst->mStyle == 75;
    // Changing from a historical look to phase 2 refreshes an uncaptured bound
    // once, rather than culling against a smaller current live pose.
    if (!inst->mCadencePalettes.empty() && now < inst->mNextPoseRefresh
        && (!need_bounds || inst->mCadenceBoundsRadius > 0.f)) return true;
    LLVOAvatar* av = resolveOverlayAvatar(*inst);
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
    const LLVector3* ext = av->getLastAnimExtents();
    inst->mCadenceBounds[0] = ext[0];
    inst->mCadenceBounds[1] = ext[1];
    inst->mCadenceRotation = av->getRotation();
    inst->mCadenceAttachMats.clear();
    if (const auto* statics = LLActorMover::instance().ghostStaticFacesFor(av->getID()))
        for (const auto& face : *statics)
            if (face.mFace) inst->mCadenceAttachMats[face.mObjectId] = face.mFace->getRenderMatrix();
    const LLVector3 root = av->getRootJoint()->getWorldPosition();
    F32 pelvis_to_foot = av->getPelvisToFoot();
    if (!llfinite(pelvis_to_foot))
    {
        pelvis_to_foot = 0.f;
    }
    inst->mCadenceFootAgent.set(root.mV[VX], root.mV[VY],
                                root.mV[VZ] - llmax(0.f, pelvis_to_foot));
    inst->mCadenceBoundsRadius = need_bounds
        ? clone_capture_radius(av, inst->mCadenceFootAgent) : 0.f;
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
        ++mInstanceRevision; // [GhostPanelPerf]
        // Parked clones have no clock to update. The authored value above is
        // still valid and recovery reads it when it creates the next body.
        return true;
    }
    const F32 chaos_factor =
        1.f + (seeded_unit(inst->mId, 5) * 2.f - 1.f) *
                  0.15f * inst->mChaosAmount;
    ghost->setEntityAnimTimeFactor(
        llclamp(inst->mAnimSpeed * chaos_factor, 0.05f, 4.f));
    ++mInstanceRevision; // External Anim speed controls must refresh Studio too.
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
        ++mInstanceRevision; // [GhostPanelPerf]
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
    ghost->restartEntityAnimationAudited(); // [AutoAnimate] Audit stops without changing restart semantics.
    return true;
}

LLGhostAvatar* ALGhostStudio::resolveEntityClone(const LLUUID& id) const
{
    const Instance* match = getInstance(id);
    if (!match) match = findInstanceByRuntime(id);
    if (match && match->mKind != BACKING_ENTITY_CLONE) return nullptr;
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
    // Floor owns the actual root, including after the post-pass (proxy pushes).
    if (!(inst->mReflectFloor && (ghost->getEntityDriveMode() == DRIVE_REFLECT ||
        (ghost->getEntityDriveMode() == DRIVE_FROZEN && ghost->hasFloorRootHold()))))
        ghost->setGhostFootPosition(gAgent.getPosAgentFromGlobal(inst->mFootGlobal));
    if (!stampOwnsFacing(*inst)) ghost->setGhostRotation(inst->mRotation);
    // Gizmo writes run after the post-pose pass. Avatar async moves can reuse
    // translated extents, which cannot represent a rotated seated body.
    ghost->refreshVisualBounds(true);
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
            const LLQuaternion member_rotation = member->withHeading(
                atan2f((F32)member_delta.mdV[VY], (F32)member_delta.mdV[VX]));
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
        inst->withHeading(atan2f((F32)delta.mdV[VY], (F32)delta.mdV[VX])),
        inst->mScale);
}

void ALGhostStudio::rewriteActorReferences(const LLUUID& old_id, const LLUUID& new_id)
{
    if (old_id.isNull() || old_id == new_id) return;
    // Actor sources use the same live/parked Cast key as their path, geometry
    // and animation consumers. Only explicit GHOST targets stay instance keyed.
    LLGhostAvatar::rewriteAnimationSourceReferences(old_id, new_id);
    for (auto& inst : mInstances)
    {
        if (inst.mSource == old_id)
        {
            inst.mSource = new_id;
            inst.mSourceRemoved = new_id.isNull();
            ++mInstanceRevision;
        }
        if (inst.mHasAnimationSource && inst.mAppearanceSource == old_id)
        {
            inst.mAppearanceSource = new_id;
            inst.mAppearanceSourceRemoved = new_id.isNull();
        }
        if (inst.mHasAnimationSource && inst.mAnimationSource == old_id)
        {
            inst.mAnimationSource = new_id;
            // Deleted animation sources hold their last pose; null must not mean You.
            inst.mAnimationSourceRemoved = new_id.isNull();
        }
        // GHOST targets are stable instance IDs and only retire on deletion;
        // ACTOR targets follow the Cast identity through runtime/parked IDs.
        if (inst.mLookTargetId == old_id &&
            (inst.mLookTarget == LOOK_TARGET_ACTOR ||
             (new_id.isNull() && inst.mLookTarget == LOOK_TARGET_GHOST)))
        {
            inst.mLookTargetId = new_id;
            if (new_id.isNull())
            {
                inst.mLookTarget = LOOK_TARGET_POINT;
                inst.mLookPointGlobal = inst.mFootGlobal;
                inst.mKeepFacing = false;
            }
        }
        if (inst.mTurnTargetId == old_id &&
            (inst.mTurnTarget == LOOK_TARGET_ACTOR ||
             (new_id.isNull() && inst.mTurnTarget == LOOK_TARGET_GHOST)))
        {
            inst.mTurnTargetId = new_id;
            if (new_id.isNull()) inst.mTurnMode = TURN_MODE_OFF;
        }
    }
    if (mCrowdDraft.mActive)
    {
        auto& source = mCrowdDraft.mSource;
        if (new_id.isNull() &&
            (source.mSourceId == old_id || source.mEntityId == old_id))
        {
            mCrowdDraft = CrowdPlacementDraft();
        }
        else
        {
            if (source.mSourceId == old_id) source.mSourceId = new_id;
            // mEntityId is the captured backing version, not a logical actor
            // reference. Leave it intact so confirmation rejects stale drafts.
        }
    }
}

LLVOAvatar* ALGhostStudio::resolveSourceAvatar(const Instance& inst)
{
    return inst.mSourceRemoved ? nullptr : LLDirectorCast::instance().resolve(inst.mSource);
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
    if (!inst || inst->mTurnMode == TURN_MODE_OFF ||
        ALAvatarAnimationTime::holdWorldTurn(dt, LLMotionController::getGlobalTimeFactor()))
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
        // overlay duplication paths all write mFootGlobal directly. Epsilon
        // (not bit-exact) compare: sub-millimetre floating-point jitter from
        // an unrelated system must not spuriously re-arm the turn.
        if ((inst->mFootGlobal - inst->mTurnSettledFoot).lengthSquared() <=
            1.0e-6)
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
    const F32 have = (F32)inst->rotationAngles()[0];
    // Shortest signed arc, so a turn never takes the long way round.
    F32 diff = want - have;
    while (diff >  F_PI) diff -= F_TWO_PI;
    while (diff < -F_PI) diff += F_TWO_PI;

    if (fabsf(diff) < 0.0015f)          // ~0.086 deg
    {
        transformUnit(id, inst->mFootGlobal,
                      inst->withHeading(want), inst->mScale);
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
        inst->withHeading(have + (diff > 0.f ? step : -step)),
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
        if (inst.mKeepFacing && inst.mTurnMode == TURN_MODE_OFF &&
            !stampOwnsFacing(inst))
        {
            if (inst.mGroupId.notNull() &&
                !tracking_groups.insert(inst.mGroupId).second) continue;
            tracking.push_back(inst.mId);
        }
    }
    for (const LLUUID& id : tracking)
    {
        const auto dt = ALAvatarAnimationTime::forWorld(gFrameIntervalSeconds.value());
        if (dt.mSeconds > 0.f) faceInstance(id);
    }
}

// Same placement as LLVOAvatar::idleCalcNameTagPosition(): a camera-facing lift
// off the head joint by the body ellipsoid, plus the fixed on-screen gap, so a
// clone's tag clears the head and hair like a real avatar's from any angle.
static LLVector3 clone_name_tag_lift(const LLVector3& head_agent, const LLVector3& body_size,
                                     const LLQuaternion& body_rot, F32 scale)
{
    LLViewerCamera* camera = LLViewerCamera::getInstance();
    LLVector3 camera_to_body = head_agent - camera->getOrigin();
    camera_to_body.normalize();
    const LLQuaternion inv_rot = ~body_rot;
    const LLVector3 ellipsoid(body_size.mV[VX] * 0.4f * scale,
                              body_size.mV[VY] * 0.4f * scale,
                              body_size.mV[VZ] * 0.17f * scale);
    LLVector3 local_at = camera_to_body * inv_rot;
    LLVector3 local_up = camera_to_body % camera->getLeftAxis();
    local_up.normalize();
    local_up = local_up * inv_rot;
    local_up.scaleVec(ellipsoid);
    local_at.scaleVec(ellipsoid);
    LLVector3 pixel_up;
    LLVector3 pixel_right;
    camera->getPixelVectors(head_agent, pixel_up, pixel_right);
    return (local_up * body_rot) - projected_vec(local_at * body_rot, camera_to_body) + pixel_up * 25.f;
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
                    LLJoint* root = ghost->getRootJoint();
                    anchor_agent += clone_name_tag_lift(
                        anchor_agent, ghost->mBodySize,
                        root ? root->getWorldRotation() : inst.mRotation,
                        llclamp(inst.mScale, GHOST_SCALE_MIN, GHOST_SCALE_MAX));
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

    LLVOAvatar* source = resolveSourceAvatar(inst);
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
        anchor_agent += clone_name_tag_lift(
            anchor_agent, source->mBodySize, inst.mRotation,
            llclamp(inst.mScale, GHOST_SCALE_MIN, GHOST_SCALE_MAX));
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
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI; // [GhostPanelPerf]
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
            if (!inst || !isVisible(*inst) || inst->mDeleting)
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
                if (!inst || !isVisible(*inst) || inst->mDeleting ||
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
        if (!isVisible(inst) || inst.mDeleting ||
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

void ALGhostStudio::applyStepAnimNameplateOffsets(
    const std::function<LLVector3(const LLUUID&)>& avatar_offset)
{
    if (!mNameplates || !avatar_offset) return;
    mNameplates->applyOffsets([this, &avatar_offset](const LLUUID& key)
    {
        if (const Instance* instance = getInstance(key))
            return instance->mSourceRemoved && instance->mEntityId.isNull() ? LLVector3::zero
                : avatar_offset(instance->mEntityId.notNull() ? instance->mEntityId : instance->mSource);
        if (const ALGhostGroupModel::Group* group = mGroups.findGroup(key))
        {
            LLVector3 sum;
            S32 count = 0;
            for (const ALGhostGroupModel::Member& member : group->mMembers)
                if (const Instance* instance = getInstance(member.mInstanceId))
                {
                    if (instance->mSourceRemoved && instance->mEntityId.isNull()) continue;
                    sum += avatar_offset(instance->mEntityId.notNull() ? instance->mEntityId : instance->mSource);
                    ++count;
                }
            if (count) sum /= static_cast<F32>(count);
            return sum;
        }
        return LLVector3::zero;
    });
}

void ALGhostStudio::restoreStepAnimNameplateOffsets()
{
    if (mNameplates) mNameplates->restoreOffsets();
}

void ALGhostStudio::scanAnimLibrary()
{
    // [GhostPanelPerf] one whole-inventory walk, cached for every panel host
    mAnimLibrary.clear();
    for (const auto& entry : ALGhostAnimAssetIndex::instance().refreshInventory())
    {
        mAnimLibrary.emplace_back(entry.mName, entry.mAssetId);
    }
    ++mAnimLibraryRevision;
}

const std::string& ALGhostStudio::animationName(const LLUUID& id)
{
    return mAnimNames.resolve(id, mAnimLibrary, mAnimLibraryRevision,
        [](const LLUUID& asset) { return gAnimLibrary.animStateToString(asset); },
        ALGhostAnimAssetIndex::instance().sessionName(id),
        ALGhostAnimAssetIndex::instance().metadataRevision());
}

void ALGhostStudio::advanceMotionClock()
{
    const LLTemporalFrameContext& frame = LLPresentationTime::currentFrame();
    if (frame.generation == mMotionClockGeneration)
    {
        return;
    }
    mMotionClockGeneration = frame.generation;
    const F64 delta = LLPresentationTime::drives(LLTemporalFeature::ANIMATION)
        ? frame.presentation_delta : frame.wall_delta;
    if (ALGhostMotionMath::isFiniteBits(delta) && delta > 0.0)
    {
        // Same 0.25s hitch policy as body turns, for wall and presentation
        // deltas alike: a stalled frame cannot add its entire delay.
        mMotionClock += llmin(delta, 0.25);
    }
}

void ALGhostStudio::updatePerFrame()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI; // [GhostPanelPerf]
    assert_main_thread();
    if (!mFades.empty()) updateFades();
    if (!mPrivateOverlayRuntimes.empty() && LLTimer::getTotalSeconds() >= mNextOverlayRecovery)
    {
        mNextOverlayRecovery = LLTimer::getTotalSeconds()+0.5;
        std::vector<LLUUID> missing;
        for (const auto& entry : mPrivateOverlayRuntimes)
        {
            LLViewerObject* object = gObjectList.findObject(entry.second);
            if (!object || object->isDead()) missing.push_back(entry.first);
        }
        for (const auto& id : missing)
            if (Instance* inst = getInstance(id); inst && !inst->mDeleting)
            {
                const bool solo_visible = inst->mSoloVisible;
                mOverlayHistory.erase(id);
                releaseOverlayHelper(id);
                inst = getInstance(id);
                if (!inst) continue;
                inst->mOffsetPose.reset();
                beginAppearance(id, false);
                if (Instance* current = getInstance(id)) current->mSoloVisible = solo_visible;
            }
    }
    for (const auto& id : mOffsetInstances)
        if (LLGhostAvatar* ghost = resolveEntityClone(id)) ghost->restoreOffsetPose();
    advanceMotionClock(); // [GhostMotionModes] before any early return
    if (mAnimLibraryScanRequested)
    {
        // [GhostPanelPerf] the lazy whole-inventory scan, outside any widget draw()
        mAnimLibraryScanRequested = false;
        if (!animLibraryScanned())
        {
            scanAnimLibrary();
        }
    }
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ghost refreshLifecycleStates"); // [GhostPanelPerf]
        refreshLifecycleStates();
    }
    finishPendingRuntimeFreezes();
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ghost updateAutoAnimate"); // [GhostPanelPerf]
        updateAutoAnimate(); // [AutoAnimate]
    }
    updateNameplates();
    const F64 facing_now = LLTimer::getTotalSeconds();
    std::set<LLUUID> dynamic_facing_groups;
    for (Instance& inst : mInstances)
    {
        // The stamp or formation motion owns facing; avoid a redundant
        // crowd-facing transform push before the motion's write this frame.
        if (stampOwnsFacing(inst) || motionOwnsRotation(inst.mMotion))
        {
            continue;
        }
        const auto facing_dt = ALAvatarAnimationTime::forWorld(gFrameIntervalSeconds.value());
        const F64 facing_time = inst.mCrowdFacingClock.sample(
            facing_now - inst.mCrowdFacingStart, facing_dt,
            LLMotionController::getGlobalTimeFactor() == 1.f);
        if (ALAvatarAnimationTime::frozen(facing_dt)) continue;
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
                    // The authored facing-offset bias must survive every
                    // live update, not just the preview -- otherwise the
                    // first runtime tick snapped the member away from the
                    // heading the user actually authored.
                    const LLQuaternion rotation = inst.withHeading(
                        atan2f((F32)delta.mdV[VY], (F32)delta.mdV[VX]) +
                            inst.mCrowdFacingYawOffset);
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
            const F32 phase = (F32)facing_time *
                              2.5f - (F32)inst.mCrowdSlot * 0.65f;
            LLQuaternion rotation = inst.withHeading(
                inst.mCrowdBaseYaw + inst.mCrowdFacingYawOffset +
                    sinf(phase) * (35.f * DEG_TO_RAD));
            rotation.normalize();
            inst.mRotation = rotation;
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
        // [GhostMotionModes] Formation motion runs on mMotionClock: 1:1 with
        // real time when live, scaled/paused with skeletons when Temporal
        // Capture drives ANIMATION. Freeze strips stay on real time.
        updateFormationMotion(mMotionClock);
    }

    // Precedence: procedural motion owns position. Spin and the rotating
    // motion modes own facing; otherwise keep-facing is last and owns yaw.
    // Chaos is folded into motion below as a stable additive variation, so
    // the two never overwrite each other.
    std::vector<LLUUID> tracking;
    std::set<LLUUID> tracking_groups;
    for (const Instance& inst : mInstances)
    {
        if (inst.mKeepFacing && inst.mMotion != MOTION_SPIN
            && !motionOwnsRotation(inst.mMotion) // [GhostMotionModes]
            && inst.mTurnMode == TURN_MODE_OFF
            && !stampOwnsFacing(inst))
        {
            if (inst.mGroupId.notNull() &&
                !tracking_groups.insert(inst.mGroupId).second) continue;
            tracking.push_back(inst.mId);
        }
    }
    for (const LLUUID& id : tracking)
    {
        const auto dt = ALAvatarAnimationTime::forWorld(gFrameIntervalSeconds.value());
        if (dt.mSeconds > 0.f) faceInstance(id);
    }
    stepAllTurns();
}

namespace
{
LLVector3d reflectSourceFoot(LLVOAvatar* source, bool floor = false)
{
    LLVector3 foot = floor ? source->getBaseRootPosition() : source->getRootJoint()->getWorldPosition();
    F32 pelvis_to_foot = source->getPelvisToFoot();
    if (floor && !llfinite(pelvis_to_foot)) pelvis_to_foot = 0.f;
    foot.mV[VZ] -= floor ? llmax(0.f, pelvis_to_foot) : pelvis_to_foot;
    return gAgent.getPosGlobalFromAgent(foot);
}
}

void ALGhostStudio::invalidateReflectPlane(const LLUUID& id)
{
    if (Instance* inst = getInstance(id)) inst->mReflectPlaneValid = false;
}

std::string ALGhostStudio::reflectStatus(const Instance& inst) const
{
    LLGhostAvatar* ghost = resolveEntityClone(inst.mId);
    if (!ghost || ghost->getEntityDriveMode() != DRIVE_REFLECT)
        return "paused: Director switcher re-drive";
    if (inst.mReflectFloor)
    {
        if (!ghost->reflectSource()) return "holding: source missing";
        if (!inst.mReflectPlaneValid) return "holding: plane not captured";
        return inst.mReflectPlaneMode == 1 ? "Reflect: floor live, sole to sole" : "Reflect: floor fixed, mirroring pose and position";
    }
    if (groupForMember(inst.mId))
        return inst.mReflectMirrorPosition ? "position held: locked group; Face pair unavailable" : "Reflect: pose only; Face pair unavailable: locked group";
    if (!ghost->reflectSource()) return "holding: source missing";
    if (!inst.mReflectPlaneValid) return "holding: plane not captured";
    if (!inst.mReflectMirrorPosition)
        return inst.mReflectPlaneMode == 1 ? "Reflect: plane live, mirroring pose" : "Reflect: plane fixed, mirroring pose";
    if (inst.mReflectPlaneMode != 0) return "position held: Fixed plane required";
    if (inst.mMotion != MOTION_OFF) return "position held: path motion owns placement";
    if (groupForMember(inst.mId)) return "position held: locked group owns placement";
    if (inst.mChaosEnabled) return "position held: Chaos owns placement";
    if (ALToolGhostEdit::getInstance()->getManipProxy().isDraggingInstance(inst.mId))
        return "position held: Ghost Edit drag owns placement";
    return "Reflect: 50 bone pairs, plane fixed, mirroring position";
}

bool ALGhostStudio::reflectOwnsPosition(const Instance& inst) const
{
    LLGhostAvatar* ghost = resolveEntityClone(inst.mId);
    if (inst.mReflectFloor)
        return ghost && ghost->getEntityDriveMode() == DRIVE_REFLECT && ghost->reflectSource() &&
            inst.mReflectPlaneValid && inst.mReflectPlanePoint.isFinite() &&
            inst.mReflectPlaneNormal == LLVector3::z_axis;
    return ghost && ghost->getEntityDriveMode() == DRIVE_REFLECT &&
        inst.mReflectMirrorPosition && inst.mReflectPlaneMode == 0 &&
        inst.mReflectPlaneValid && inst.mReflectPlanePoint.isFinite() &&
        inst.mReflectPlaneNormal.isFinite() && fabsf(inst.mReflectPlaneNormal.mV[VZ]) < 1e-5f &&
        fabsf(inst.mReflectPlaneNormal.length() - 1.f) < .001f && ghost->reflectSource() &&
        inst.mMotion == MOTION_OFF && !groupForMember(inst.mId) &&
        !inst.mChaosEnabled &&
        !ALToolGhostEdit::getInstance()->getManipProxy().isDraggingInstance(inst.mId);
}

bool ALGhostStudio::facePair(const LLUUID& id)
{
    Instance* inst = getInstance(id);
    LLGhostAvatar* ghost = resolveEntityClone(id);
    LLVOAvatar* source = ghost ? ghost->reflectSource() : nullptr;
    if (!inst || !source || inst->mReflectFloor || groupForMember(id)) return false;
    LLVector3 forward = LLVector3::x_axis * source->getRootJoint()->getWorldRotation();
    forward.mV[VZ] = 0.f;
    if (!forward.isFinite() || forward.length() < .2f) return false;
    forward.normalize();
    static LLCachedControl<F32> distance(gSavedSettings, "GhostReflectFacePairDistance", 2.f);
    const LLVector3d foot = reflectSourceFoot(source);
    if (!foot.isFinite() || !std::isfinite((F32)distance)) return false;
    const LLVector3d clone = foot + LLVector3d(forward * llclamp((F32)distance, .5f, 10.f));
    inst->setTransform(clone, LLQuaternion(atan2f(-forward.mV[VY], -forward.mV[VX]), LLVector3::z_axis));
    inst->mReflectPlaneMode = 0;
    inst->mReflectPlanePoint = (foot + clone) * .5;
    inst->mReflectPlaneNormal = forward;
    inst->mReflectPlaneValid = true;
    inst->mReflectHasLastPlane = true;
    inst->mReflectPlaneDegenerate = false;
    // Push the helper's authored facing even with copy-facing disabled.
    ghost->setGhostRotation(inst->mRotation);
    if (!setInstanceDriveMode(id, DRIVE_REFLECT)) return false;
    applyEntityTransform(id);
    return true;
}

bool ALGhostStudio::stampOwnsFacing(const Instance& inst) const
{
    static LLCachedControl<bool> copy_facing(
        gSavedSettings, "GhostTrueMirrorCopyFacing", true);
    if (inst.mKind != BACKING_ENTITY_CLONE) return false;
    const LLGhostAvatar* ghost = resolveEntityClone(inst.mId);
    const bool reflect = ghost && ghost->getEntityDriveMode() == DRIVE_REFLECT;
    const bool floor_hold = inst.mReflectFloor && ghost &&
        ghost->getEntityDriveMode() == DRIVE_FROZEN && ghost->hasFloorRootHold();
    return (inst.mDriveMode == DRIVE_TRUE_MIRROR && copy_facing) ||
           (reflect && (copy_facing || inst.mReflectFloor)) || floor_hold;
}

bool ALGhostStudio::canApplyMotionTilt(const Instance& inst, const LLGhostAvatar* ghost) const
{
    static LLCachedControl<bool> copy_facing(
        gSavedSettings, "GhostTrueMirrorCopyFacing", true);
    // Retain Reflect/frozen-floor ownership and account for a Director re-drive
    // whose runtime True Mirror mode differs from the Studio's authored drive.
    return ghost && !stampOwnsFacing(inst) &&
        !(ghost->isTrueMirrorDriven() && copy_facing) &&
        !LLActorMover::instance().isDriving(ghost->getID());
}

void ALGhostStudio::updatePostObjectList(bool hot_gaze)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI; // [GhostPanelPerf]
    assert_main_thread();
    if (hot_gaze)
    {
        // A second pass copies only inherited hot rotations, after Stepped.
        // Solo is render-only: hidden bodies still feed source dependencies.
        // Preserve held poses and the QoL private overlay body domain.
        std::vector<Instance*> hot_order;
        auto hot_body = [&](const Instance& inst) -> LLGhostAvatar*
        {
            return inst.mKind == BACKING_ENTITY_CLONE ? resolveEntityClone(inst.mId) :
                dynamic_cast<LLGhostAvatar*>(resolveOverlayAvatar(inst));
        };
        {
            std::map<LLUUID, Instance*> instances;
            std::map<LLUUID, U8> visited;
            for (Instance& inst : mInstances)
            {
                instances[inst.mId] = &inst;
                if (inst.mEntityId.notNull()) instances[inst.mEntityId] = &inst;
                if (inst.mOverlayAvatar.notNull()) instances[inst.mOverlayAvatar] = &inst;
            }
            std::function<void(Instance&)> visit = [&](Instance& inst)
            {
                if (visited[inst.mId]) return; // also stop malformed source cycles
                visited[inst.mId] = 1;
                if (LLGhostAvatar* ghost = hot_body(inst))
                {
                    auto parent = instances.find(ghost->getAnimationSourceId());
                    if (parent != instances.end()) visit(*parent->second);
                }
                hot_order.push_back(&inst);
            };
            for (Instance& inst : mInstances) visit(inst);
        }
        std::set<LLUUID> stamped_helpers;
        for (Instance* inst : hot_order)
        {
            if (inst->mDeleting ||
                (inst->mKind == BACKING_OVERLAY &&
                    (inst->mOverlayAvatar.isNull() || inst->mPose == POSE_FROZEN))) continue;
            LLGhostAvatar* ghost = hot_body(*inst);
            if (!ghost) continue;
            const bool delayed = inst->mKind == BACKING_ENTITY_CLONE && inst->mStartOffset > 0.f;
            const bool source_hot = LLActorMover::instance().hasHotGazePose(ghost->reflectSource());
            if (!source_hot && !(delayed && ghost->hasOffsetHotGaze())) continue;
            if (!stamped_helpers.insert(ghost->getID()).second) continue;
            const S32 mode = ghost->getEntityDriveMode();
            if (mode != DRIVE_REFLECT && mode != DRIVE_TRUE_MIRROR) continue;
            if (delayed) LLActorMover::instance().captureHotGazeCopy(nullptr, ghost, true);
            if (source_hot && mode == DRIVE_REFLECT) ghost->applyReflectStamp(inst->mReflectPlaneNormal, true);
            else if (source_hot && mode == DRIVE_TRUE_MIRROR)
            {
                ghost->applyTrueMirrorStamp(true);
                if (ALGhostAnimMirrorMath::eligible(inst->mDriveMode,
                        inst->mDirectedMirrorFreezeDrive, inst->mDirectedMirror, mode))
                    ghost->applyDirectedMirrorStamp(inst->mDriveMode, inst->mDirectedMirrorFreezeDrive, true);
            }
            if (delayed) ghost->applyOffsetHistory(LLTimer::getTotalSeconds(), true);
            // A directly targeted clone owns its own final layer, then passes
            // that presented pose to any downstream clone in the same chain.
            LLActorMover::instance().applyHotGaze(ghost);
        }
        return;
    }

    // Hook point: called from LLAppViewer::idle() immediately after
    // gObjectList.update(gAgent). By then every avatar -- source and clone
    // alike -- has run LLVOAvatar::updateCharacter() for this frame
    // (updateMotions -> Pose Polish -> Director/ActorMover gaze), so the
    // source's joints hold its FINAL pose and nothing else writes the clone's
    // joints before the render passes consume them (system-mesh reskin,
    // rigged matrix palettes and attachment drawable moves all read the joint
    // state later in the frame). mActiveObjects is swap-removed on
    // deactivation, so a same-frame copy from inside the clone's own
    // idleUpdate() could not guarantee source-before-clone ordering; this
    // post-pass does, with zero frame lag.
    //
    // The ghost self-gates on ITS OWN drive mode, not the instance's: the
    // Director anim switcher can temporarily re-drive a ghost (DIRECTED /
    // FROZEN) without touching the Studio model, and must not be stamped over.
    std::set<LLUUID> stamped_helpers;
    for (Instance& inst : mInstances)
    {
        if (inst.mKind != BACKING_ENTITY_CLONE && inst.mOverlayAvatar.isNull())
        {
            continue;
        }
        if (LLGhostAvatar* ghost = inst.mKind == BACKING_ENTITY_CLONE ? resolveEntityClone(inst.mId) :
            dynamic_cast<LLGhostAvatar*>(resolveOverlayAvatar(inst)))
        {
            if (inst.mKind == BACKING_OVERLAY)
            {
                if (!stamped_helpers.insert(ghost->getID()).second) continue;
                rememberOverlayDomain(ghost);
            }
            const S32 mode = ghost->getEntityDriveMode();
            if (mode == DRIVE_REFLECT)
            {
                inst.mReflectWasDriven = true;
                LLVOAvatar* source = ghost->reflectSource();
                if (!source)
                {
                    ghost->applyReflectStamp(inst.mReflectPlaneNormal); // INCONCLUSIVE verdict; no writes
                    continue; // hold plane, placement and pose together
                }
                const LLVector3d foot = reflectSourceFoot(source, inst.mReflectFloor);
                ALReflect::Plane plane;
                plane.valid = inst.mReflectPlaneValid || inst.mReflectHasLastPlane;
                plane.degenerate = inst.mReflectPlaneDegenerate;
                plane.point = inst.mReflectPlanePoint;
                plane.normal = inst.mReflectPlaneNormal;
                if (!inst.mReflectPlaneValid || (inst.mReflectPlaneMode == 1 && (inst.mReflectFloor || !inst.mReflectMirrorPosition)))
                {
                    S32 tier = 0;
                    if (inst.mReflectFloor) ALReflect::updateFloorPlane(plane, foot);
                    else tier = ALReflect::updatePlane(plane, foot, inst.mFootGlobal,
                        LLVector3::x_axis * source->getRootJoint()->getWorldRotation(), inst.mReflectPlaneValid && inst.mReflectPlaneMode == 1);
                    if (!inst.mReflectPlaneValid)
                        LL_INFOS("GhostReflect") << (inst.mReflectFloor ? "floor " : "wall ") << "plane capture tier=" << tier
                            << " (0 bisector, 1 forward, 2 last, 3 world +X): "
                            << (plane.valid && tier >= 0 ? "PASS" : "INCONCLUSIVE") << LL_ENDL;
                    inst.mReflectPlaneValid = plane.valid;
                    inst.mReflectHasLastPlane = plane.valid;
                    inst.mReflectPlaneDegenerate = plane.degenerate;
                    inst.mReflectPlanePoint = plane.point;
                    inst.mReflectPlaneNormal = plane.normal;
                }
                if (reflectOwnsPosition(inst))
                {
                    const LLVector3d mirrored = inst.mReflectFloor ? ALReflect::floorPoint(foot, plane.point.mdV[VZ]) :
                        ALReflect::planeFoot(foot, plane.point, plane.normal);
                    if (mirrored.isFinite())
                    {
                        inst.mFootGlobal = mirrored; // deliberately no proxy revision bump
                        inst.mChaosHasBase = false;
                        if (inst.mReflectFloor)
                        {
                            const LLVector3d root = ALReflect::floorPoint(
                                gAgent.getPosGlobalFromAgent(source->getRootJoint()->getWorldPosition()), plane.point.mdV[VZ]);
                            if (root.isFinite()) ghost->setGhostFloorPosition(
                                gAgent.getPosAgentFromGlobal(root), gAgent.getPosAgentFromGlobal(mirrored));
                        }
                        else ghost->setGhostFootPosition(gAgent.getPosAgentFromGlobal(mirrored));
                    }
                }
                if (plane.valid) ghost->applyReflectStamp(plane.normal);
            }
            else
            {
                if (inst.mReflectWasDriven && mode != DRIVE_FROZEN)
                {
                    inst.mReflectPlaneValid = false;
                    inst.mReflectWasDriven = false;
                }
                if (inst.mDriveMode == DRIVE_TRUE_MIRROR || inst.mKind == BACKING_OVERLAY) ghost->applyTrueMirrorStamp();
                // Flip the clone's evaluated pose in place: after its controller
                // for Mirror/Directed, or after the source stamp for True mirror.
                // Matching runtime ownership leaves Director overrides in charge.
                if (ALGhostAnimMirrorMath::eligible(inst.mDriveMode,
                        inst.mDirectedMirrorFreezeDrive, inst.mDirectedMirror, mode))
                    ghost->applyDirectedMirrorStamp(inst.mDriveMode, inst.mDirectedMirrorFreezeDrive);
            }
            if (inst.mKind == BACKING_ENTITY_CLONE && inst.mStartOffset > 0.f) ghost->applyOffsetHistory(LLTimer::getTotalSeconds());
        }
    }

    // [GhostMotionModes] LLVOAvatar::updateCharacter() -> updateOrientation()
    // slerps every non-seated root toward an UPRIGHT pose built from the
    // object's forward and world up, and it ran after this frame's motion
    // write. Re-assert the motion's full rotation now, before any render-side
    // consumer, so hover tilt, float tumble and pendulum swing survive.
    // Known, accepted: gaze, avatar physics and attachment solves inside
    // updateCharacter saw the root after that partial slerp (u = dt/lag, ~2.5%
    // of the tilt at 60 fps) and are not re-solved -- the same contract as the
    // True Mirror stamp above.
    // Skipped when another owner holds the root: a True Mirror stamp with
    // copy-facing (gated on the GHOST's own drive, like the stamp itself), or
    // Actor Mover walking the clone (it wrote the root inside updateCharacter),
    // or Reflect/frozen-floor root ownership (the current stamp contract).
    for (const Instance& inst : mInstances)
    {
        if (inst.mKind != BACKING_ENTITY_CLONE || !inst.mMotionHasBase ||
            !motionOwnsTilt(inst.mMotion))
        {
            continue;
        }
        LLGhostAvatar* ghost = resolveEntityClone(inst.mId);
        if (!canApplyMotionTilt(inst, ghost))
        {
            continue;
        }
        ghost->reassertTiltRotation();
    }

    // Selection and culling consume the same final rendered skeleton bounds.
    // Classify all entity clones, but force final-pose extents/partition moves
    // only for selected or fitted clones; others keep native extent throttling.
    for (const Instance& inst : mInstances)
    {
        if (inst.mKind == BACKING_ENTITY_CLONE)
            if (LLGhostAvatar* ghost = resolveEntityClone(inst.mId))
                ghost->refreshVisualBounds();
    }
}

void ALGhostStudio::stepAllTurns()
{
    // Body turn runs AFTER look-at and after motion, so it is the last word on
    // yaw for any instance that has a turn target -- except SPIN, which owns
    // yaw outright and would otherwise fight it every frame.
    const F32 wall_dt = gFrameIntervalSeconds.value();
    std::vector<LLUUID> turning;
    std::set<LLUUID> turning_groups;
    for (const Instance& inst : mInstances)
    {
        if (inst.mTurnMode != TURN_MODE_OFF && inst.mMotion != MOTION_SPIN
            && !motionOwnsRotation(inst.mMotion) // [GhostMotionModes]
            && !stampOwnsFacing(inst))
        {
            if (inst.mGroupId.notNull() &&
                !turning_groups.insert(inst.mGroupId).second) continue;
            turning.push_back(inst.mId);
        }
    }
    for (const LLUUID& id : turning)
    {
        const F32 dt = ALAvatarAnimationTime::placementSeconds(wall_dt, LLMotionController::getGlobalTimeFactor(),
            LLPresentationTime::presentationDelta(), LLPresentationTime::drives(LLTemporalFeature::ANIMATION), false);
        stepTurn(id, dt);
    }
}

// [GhostMotionModes]
static_assert(ALGhostStudio::MOTION_BOUNCE - ALGhostStudio::MOTION_HOVER ==
                  ALGhostMotionMath::KIND_BOUNCE,
              "EMotion procedural modes must stay index-parallel with ALGhostMotionMath::Kind");
static_assert(ALGhostStudio::MOTION_COUNT - ALGhostStudio::MOTION_HOVER ==
                  ALGhostMotionMath::KIND_COUNT,
              "EMotion procedural modes must stay index-parallel with ALGhostMotionMath::Kind");

namespace
{
bool motion_math_kind(ALGhostStudio::EMotion motion, ALGhostMotionMath::Kind& kind)
{
    if (motion < ALGhostStudio::MOTION_HOVER || motion >= ALGhostStudio::MOTION_COUNT)
    {
        return false;
    }
    kind = static_cast<ALGhostMotionMath::Kind>(motion - ALGhostStudio::MOTION_HOVER);
    return true;
}

// Built-in animation for a motion cue; null for CUE_NONE.
const LLUUID* motion_cue_animation(ALGhostMotionMath::AnimCue cue)
{
    switch (cue)
    {
    case ALGhostMotionMath::CUE_HOVER:    return &ANIM_AGENT_HOVER;
    case ALGhostMotionMath::CUE_HOVER_UP: return &ANIM_AGENT_HOVER_UP;
    case ALGhostMotionMath::CUE_PRE_JUMP: return &ANIM_AGENT_PRE_JUMP;
    case ALGhostMotionMath::CUE_JUMP:     return &ANIM_AGENT_JUMP;
    case ALGhostMotionMath::CUE_LAND:     return &ANIM_AGENT_LAND;
    default:                              return nullptr;
    }
}
} // namespace

// static
bool ALGhostStudio::motionOwnsRotation(EMotion motion)
{
    ALGhostMotionMath::Kind kind;
    return motion_math_kind(motion, kind) && ALGhostMotionMath::ownsRotation(kind);
}

// static
bool ALGhostStudio::motionOwnsTilt(EMotion motion)
{
    ALGhostMotionMath::Kind kind;
    return motion_math_kind(motion, kind) && ALGhostMotionMath::ownsTilt(kind);
}

S32 ALGhostStudio::setFormationMotion(const std::vector<LLUUID>& ids,
                                      EMotion motion, F32 speed, F32 amplitude)
{
    assert_main_thread();
    // [GhostMotionModes] An out-of-range value (a stale UI or a future combo
    // entry) is treated as Off rather than reaching the per-frame switch.
    if (motion < MOTION_OFF || motion >= MOTION_COUNT)
    {
        motion = MOTION_OFF;
    }
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
    // [GhostMotionModes] Same clock as updateFormationMotion().
    advanceMotionClock();
    const F64 now = mMotionClock;
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
                // [GhostMotionModes] Release any hover/jump cue the motion
                // started on the clone's own controller.
                if (inst->mKind == BACKING_ENTITY_CLONE)
                {
                    if (LLGhostAvatar* ghost = resolveEntityClone(id))
                        ghost->setFormationAnimations(std::vector<LLUUID>());
                }
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
        // Authored or cached bases can outlive their original writer. Keep a
        // bad quaternion out of the procedural composition under /fp:fast.
        if (!ALGhostMotionMath::isFiniteBits(inst->mMotionBaseRotation))
            inst->mMotionBaseRotation = LLQuaternion::DEFAULT;
        inst->mMotionBaseRotation.normalize();
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
        bool path_source_removed = inst->mSourceRemoved;
        if (inst->mGroupId.notNull())
        {
            const std::vector<LLUUID> unit = groupMembers(inst->mId);
            if (!unit.empty())
                if (Instance* anchor = getInstance(unit.front()))
                {
                    path_src = anchor->mSource;
                    path_source_removed = anchor->mSourceRemoved;
                }
        }
        inst->mMotionPathYaw = 0.f;
        if (const LLActorMover::Path* path =
                path_source_removed ? nullptr : LLActorMover::instance().getPath(path_src))
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
        ALGhostMotionMath::AnimCue cue = ALGhostMotionMath::CUE_NONE;
        ALGhostMotionMath::Kind kind;
        if (motion_math_kind(inst.mMotion, kind))
        {
            // [GhostMotionModes] Hover, jump, space walk, float, ascend,
            // vortex, pendulum, bounce: one pure evaluator, see
            // alghostmotionmath.h. Offsets are relative to the authored foot;
            // the local rotation is applied in the clone's own frame first.
            ALGhostMotionMath::Input in;
            in.mKind = kind;
            in.mElapsed = now - inst.mMotionStart;
            in.mSpeed = inst.mMotionSpeed;
            in.mAmplitude = inst.mMotionAmplitude;
            in.mSlot = inst.mMotionSlot;
            for (S32 i = 0; i < ALGhostMotionMath::SEED_COUNT; ++i)
            {
                in.mSeeds[i] = seeded_unit(inst.mId, 20u + (U32)i);
            }
            in.mRadial = LLVector3(radial);
            in.mBaseForward = LLVector3::x_axis * inst.mMotionBaseRotation;
            const ALGhostMotionMath::Output out = ALGhostMotionMath::evaluate(in);
            inst.mFootGlobal = inst.mMotionBaseFoot + LLVector3d(out.mOffset);
            inst.mRotation = out.mLocalRotation * inst.mMotionBaseRotation;
            inst.mRotation.normalize();
            cue = out.mCue;
        }
        else if (inst.mMotion == MOTION_ORBIT)
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
            bool path_source_removed = inst.mSourceRemoved;
            if (inst.mGroupId.notNull())
            {
                const std::vector<LLUUID> unit = groupMembers(inst.mId);
                if (!unit.empty())
                    if (Instance* anchor = getInstance(unit.front()))
                    {
                        path_src = anchor->mSource;
                        path_source_removed = anchor->mSourceRemoved;
                    }
            }
            const LLActorMover::Path* path =
                path_source_removed ? nullptr : LLActorMover::instance().getPath(path_src);
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
            LLGhostAvatar* ghost = resolveEntityClone(inst.mId);
            if (ghost)
                ghost->setEntityScale(inst.mScale);
            if (motionOwnsTilt(inst.mMotion) && canApplyMotionTilt(inst, ghost))
            {
                // [GhostMotionModes] Pitch/roll/tumble: the root takes the full
                // rotation, the object a heading-only one, and the post-pass
                // re-asserts the root after the avatar update (which otherwise
                // slerps every root back toward upright). Always this path for
                // tilt modes when no stamp or Actor Mover owns the root, so
                // the object's forward is never near-vertical for updateOrientation().
                ghost->setGhostFootPosition(
                    gAgent.getPosAgentFromGlobal(inst.mFootGlobal));
                ghost->setGhostTiltRotation(inst.mRotation);
            }
            else
            {
                if (ghost) ghost->clearGhostTiltRotation();
                applyEntityTransform(inst.mId);
            }
            if (ghost)
            {
                static LLCachedControl<bool> motion_anims(
                    gSavedSettings, "GhostMotionAnimations", true);
                std::vector<LLUUID> anims;
                if (motion_anims)
                {
                    if (const LLUUID* anim = motion_cue_animation(cue))
                        anims.push_back(*anim);
                }
                ghost->setFormationAnimations(anims);
            }
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
        ++mInstanceRevision; // [GhostPanelPerf]
        inst->mState = STATE_ERROR;
        return false;
    }
    const LLUUID next_anim = mode == DRIVE_DIRECTED ? directed_anim : LLUUID::null;
    if (inst->mDriveMode != mode || inst->mDirectedAnim != next_anim)
    {
        ++mInstanceRevision; // roster drive/Directed animation display changed
    }
    if (mode != DRIVE_REFLECT && mode != DRIVE_FROZEN) inst->mReflectPlaneValid = false;
    // Mirror eligibility remembers the frozen pose owner separately. Playback
    // Resume records retain their pre-feature, Pause-only behavior.
    if (mode == DRIVE_FROZEN && inst->mDriveMode != DRIVE_FROZEN)
    {
        inst->mDirectedMirrorFreezeDrive = inst->mDriveMode;
    }
    inst->mDriveMode = mode;
    inst->mDirectedAnim = next_anim;
    ghost->setEntityDriveMode(mode, inst->mDirectedAnim);
    ghost->setEntityLoopMode(inst->mLoopMode);
    return true;
}

void ALGhostStudio::setInstanceDirectedMirror(const LLUUID& id, bool enabled)
{
    assert_main_thread();
    Instance* inst = getInstance(id);
    if (!inst || inst->mKind != BACKING_ENTITY_CLONE || inst->mDirectedMirror == enabled) return;
    inst->mDirectedMirror = enabled;
    ++mInstanceRevision;
    if (LLGhostAvatar* ghost = resolveEntityClone(id)) ghost->setDirectedMirror(enabled);
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
        // [GhostPanelPerf] Bump ONLY when the state actually changes: this runs
        // every frame, and a per-frame bump would rebuild the Studio list every frame.
        const ELifecycleState previous_state = inst.mState;
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
            inst.mState = resolveCopiedAvatar(inst)
                ? STATE_READY : STATE_SOURCE_MISSING;
        }
        if (inst.mState != previous_state)
        {
            ++mInstanceRevision;
        }
    }
}

void ALGhostStudio::removeInstance(const LLUUID& id)
{
    assert_main_thread();
    const LLUUID retiring_id = id; // callers may pass a vector-backed Instance::mId
    if (mCrowdDraft.mActive && mCrowdDraft.mSource.mInstanceId == retiring_id)
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
    Instance* inst = getInstance(retiring_id);
    if (!inst)
    {
        return;
    }
    ++mInstanceRevision; // [GhostPanelPerf] row removed (groups/recursion below also bump)
    if (ALCineLightRigManager::instanceExists())
        ALCineLightRigManager::instance().removeMirror(inst->mEntityId);
    if (inst->mGroupId.notNull())
    {
        const LLUUID group_id = inst->mGroupId;
        const std::vector<LLUUID> unit = groupMembers(retiring_id);
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
    const LLUUID history_key = retiring_id;
    mFades.erase(retiring_id);
    mOffsetInstances.erase(retiring_id);
    releaseOverlayHelper(retiring_id);
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
    else
    {
        // Overlay identity is its stable id. Retire its outside references at
        // the same final boundary as entities, including deferred fade removal.
        LLDirectorCast::instance().replaceActorId(retiring_id, LLUUID::null, retiring_id);
        if (mSelected == retiring_id) setSelected(LLUUID::null);
    }
    mIndicesDirty = true;
    mInstances.erase(
        std::remove_if(mInstances.begin(), mInstances.end(),
                       [&retiring_id](const Instance& i) { return i.mId == retiring_id; }),
        mInstances.end());
    ++mInstanceRevision; // [GhostPanelPerf]
    mOverlayHistory.erase(history_key);
    if (mSolo && std::none_of(mInstances.begin(), mInstances.end(),
        [](const Instance& item) { return item.mSoloVisible && !item.mDeleting; })) setSolo({});
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
    if (isDeleting(id)) return;
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
    ++mInstanceRevision; // [GhostPanelPerf] group created
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
    ++mInstanceRevision; // [GhostPanelPerf] group dissolved
    for (const LLUUID& member : ids)
        if (Instance* inst = getInstance(member))
        {
            inst->mGroupId.setNull();
            inst->mLockMode = LOCK_OFF;
            // Dynamic crowd-facing drivers (Wave/Track subject) only run
            // once mGroupId is null (see updatePerFrame()) and rotate the
            // instance every frame. Left dangling after ungroup, that
            // perpetual rotation keeps bumping mTransformRevision on what
            // may become a NEW crowd's prototype, which used to trip
            // crowdPlacementSourceUnchanged()'s staleness check on the very
            // next frame -- making placement effectively impossible.
            inst->mCrowdFacing = FACING_AUTHOR;
            inst->mCrowdSlot = 0;
            inst->mCrowdBaseYaw = 0.f;
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
            ++mInstanceRevision; // [GhostPanelPerf] group mode + member lock modes
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
    const bool renamed = group && mGroups.renameGroup(group->mId, name);
    if (renamed)
    {
        ++mInstanceRevision; // [GhostPanelPerf]
    }
    return renamed;
}

bool ALGhostStudio::setGroupEditMembers(const LLUUID& id, bool editing)
{
    ALGhostGroupModel::Group* group = groupForMember(id);
    if (!group || !mGroups.setEditMembers(group->mId, editing))
    {
        return false;
    }
    for (const LLUUID& member_id : groupMembers(group->mId))
        if (Instance* member = getInstance(member_id)) ++member->mRotationResetRevision;
    ++mInstanceRevision; // [GhostPanelPerf] expand/collapse arrow + child rows
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
    if (Instance* member = getInstance(id))
    {
        ++member->mRotationResetRevision;
        member->mHeadingReferenceValid = false;
    }
    ++mInstanceRevision; // [GhostPanelPerf] resetting an offset clears the member's pin flag
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
    ++mInstanceRevision; // [GhostPanelPerf] member pin flags are part of the group row signature
    // "Pinned" means keep this custom member-local offset. Unpinning is an
    // explicit return to the authored formation slot, so the toggle has an
    // immediate, visible meaning even before a future group reflow.
    if (!pinned)
    {
        if (Instance* member = getInstance(id))
        {
            ++member->mRotationResetRevision;
            member->mHeadingReferenceValid = false;
        }
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
    // [GhostPanelPerf] NO revision bump here on purpose: this runs every frame
    // during gizmo drags and dynamic-facing crowds, and a per-frame bump would
    // rebuild the Studio list every frame. Row-visible group changes (create,
    // mode, name, expand, pins, membership) bump in their own mutators.

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
    mOverlayHistory.clear();
    mSolo = false;
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
                return inst.mKind == BACKING_ENTITY_CLONE && !inst.mDeleting;
            });
    };
    const S32 before = entity_count();

    std::vector<LLUUID> standalone;
    std::set<LLUUID> group_ids;
    for (const Instance& inst : mInstances)
    {
        if (inst.mKind == BACKING_ENTITY_CLONE && !inst.mDeleting)
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
    // literal instead of letting requestRemoveInstance() delete the entire group.
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
            requestRemoveInstance(entities.front());
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
                requestRemoveInstance(entity_id);
            }
        }
    }

    for (const LLUUID& id : standalone)
    {
        const Instance* current = getInstance(id);
        if (current && current->mKind == BACKING_ENTITY_CLONE &&
            current->mGroupId.isNull())
        {
            requestRemoveInstance(id);
        }
    }
    return llmax(0, before - entity_count());
}

void ALGhostStudio::rebuildIndices() const
{
    if (!mIndicesDirty) return;
    mStableIndex.clear(); mRuntimeIndex.clear();
    for (size_t i=0; i<mInstances.size(); ++i)
    {
        mStableIndex[mInstances[i].mId] = i;
        if (mInstances[i].mEntityId.notNull()) mRuntimeIndex[mInstances[i].mEntityId] = i;
    }
    mIndicesDirty = false;
}
ALGhostStudio::Instance* ALGhostStudio::getInstance(const LLUUID& id)
{
    rebuildIndices();
    const auto it = mStableIndex.find(id);
    return it != mStableIndex.end() ? &mInstances[it->second] : nullptr;
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
    if (!inst || inst->mDeleting) return false;
    if (inst->mPose == POSE_FROZEN && !inst->mFrozenPalettes.empty()) return true;
    if (inst->mOffsetPose && inst->mStartOffset > 0.f)
    {
        const auto pose = inst->mOffsetPose;
        inst->mFrozenPalettes = pose->mFrozenPalettes;
        inst->mFrozenAttachMats = pose->mFrozenAttachMats;
        inst->mFrozenBounds[0] = pose->mFrozenBounds[0];
        inst->mFrozenBounds[1] = pose->mFrozenBounds[1];
        inst->mFrozenFootAgent = pose->mFrozenFootAgent;
        inst->mFrozenBoundsRadius = pose->mFrozenBoundsRadius;
        inst->mFrozenHeadAgent = pose->mFrozenHeadAgent;
        inst->mFrozenHeadValid = pose->mFrozenHeadValid;
        inst->mFrozenJointRotations = pose->mFrozenJointRotations;
        inst->mFrozenJointPositions = pose->mFrozenJointPositions;
        inst->mPose = POSE_FROZEN;
    }
    else if (!captureOverlayPose(*inst)) return false;
    if (auto found = mOverlayHistory.find(id); found != mOverlayHistory.end())
    {
        found->second.mPausedAt = LLTimer::getTotalSeconds();
        found->second.mPaused = true;
    }
    ++mInstanceRevision;
    return true;
}

bool ALGhostStudio::captureOverlayPose(Instance& record)
{
    Instance* inst = &record;
    LLVOAvatar* av = resolveOverlayAvatar(*inst);
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

    // A swapped/recovered frozen presentation has new skin keys. Reharvest
    // its held skeleton temporarily; the shared helper keeps independent live input.
    struct RestoreSkeleton
    {
        LLVOAvatar* avatar = nullptr;
        std::vector<LLQuaternion> rotations;
        std::vector<LLVector3> positions;
        void changed()
        {
            avatar->getRootJoint()->updateWorldMatrixChildren();
            avatar->invalidateSkinPalettesForOverlay();
            avatar->invalidateSkinPalettesAfterOverlay();
            if (auto* helper = dynamic_cast<LLGhostAvatar*>(avatar)) helper->refreshOverlayAttachments();
        }
        ~RestoreSkeleton()
        {
            if (!avatar) return;
            const auto& joints = avatar->getSkeleton();
            for (size_t i=0; i<joints.size(); ++i) if (joints[i])
            {
                joints[i]->setRotation(rotations[i]);
                joints[i]->setPosition(positions[i]);
            }
            changed();
        }
    } restore;
    if (inst->mPose == POSE_FROZEN && inst->mFrozenPalettes.empty() && !inst->mFrozenJointRotations.empty())
    {
        restore.avatar = av;
        const auto& joints = av->getSkeleton();
        for (size_t i=0; i<joints.size(); ++i)
        {
            restore.rotations.push_back(joints[i] ? joints[i]->getRotation() : LLQuaternion());
            restore.positions.push_back(joints[i] ? joints[i]->getPosition() : LLVector3::zero);
            if (joints[i] && i<inst->mFrozenJointRotations.size() && i<inst->mFrozenJointPositions.size())
            {
                joints[i]->setRotation(inst->mFrozenJointRotations[i]);
                joints[i]->setPosition(inst->mFrozenJointPositions[i]);
            }
        }
        restore.changed();
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
    inst->mFrozenJointRotations.clear();
    inst->mFrozenJointPositions.clear();
    for (LLJoint* joint : av->getSkeleton())
    {
        inst->mFrozenJointRotations.push_back(joint ? joint->getRotation() : LLQuaternion());
        inst->mFrozenJointPositions.push_back(joint ? joint->getPosition() : LLVector3::zero);
    }

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
    inst->mFrozenBoundsRadius = clone_capture_radius(av, foot);
    const LLVector3* ext = av->getLastAnimExtents();
    inst->mFrozenBounds[0] = ext[0];
    inst->mFrozenBounds[1] = ext[1];
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
        ++mInstanceRevision; // [GhostPanelPerf]
        if (auto found = mOverlayHistory.find(id); found != mOverlayHistory.end() && found->second.mPaused)
        {
            auto& history = found->second;
            const F64 now = LLTimer::getTotalSeconds();
            const F64 paused = llmax(0., now-history.mPausedAt);
            for (auto& frame : history.mFrames) frame.first += paused;
            history.mNextCapture += paused;
            inst->mNextPoseRefresh += paused; // cadence snapshot remains latched until its shifted deadline
            history.mPaused = false;
        }
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
        const LLActorMover::Path* source_path = p.mSourceRemoved ? nullptr : LLActorMover::instance().getPath(p.mSource);
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
            source ? resolveSourceAvatar(*source) : nullptr;
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
        // [AutoAnimate] Strips retain HEAD's live-source snapshot semantics.
        snap->mAutoAnimManual = false;
        snap->mAutoAnimPool.reset();
        setInstanceAutoAnimate(snap_id, false);
        snap = getInstance(snap_id);
        if (!snap) { ++it; continue; }
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
        copy.mOverlayAvatar.setNull();
        copy.mDeleting = false;
        copy.mFadeAlpha = 1.f;
        copy.mOffsetPose.reset();
            copy.mName = makeDefaultName();
            copy.mGroupId.setNull();
    copy.mCrowdId.setNull();
            copy.mLockMode = LOCK_OFF;
            copy.mFootGlobal = s.mFoot;
            if (s.mAuthorYaw)
            {
                copy.setYaw(s.mYaw);
            }
            mIndicesDirty = true;
    mInstances.push_back(copy);
        beginAppearance(copy.mId);
            ++mInstanceRevision; // [GhostPanelPerf]
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
    next.mPatternYaw = yaw;
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
    mCrowdDraft.mPatternYaw = yaw;
    mCrowdDraft.mHeight = height;
    mCrowdDraft.mRevision = mNextCrowdDraftRevision++;
    return resolveCrowdPlacement();
}

bool ALGhostStudio::setCrowdPlacementFacingOffset(
    const LLUUID& owner_id, F32 offset_rad)
{
    if (mCrowdCommitInProgress || !mCrowdDraft.mActive ||
        owner_id != mCrowdDraft.mOwnerId || !llfinite(offset_rad))
    {
        return false;
    }
    mCrowdDraft.mFacingYawOffset = offset_rad;
    mCrowdDraft.mRevision = mNextCrowdDraftRevision++;
    return resolveCrowdPlacement();
}

bool ALGhostStudio::setCrowdPlacementAnchorInfo(
    const LLUUID& owner_id,
    ALGhostPlacementResolver::EGhostPlacementBasis basis,
    bool valid,
    const std::string& warning)
{
    if (!mCrowdDraft.mActive || owner_id != mCrowdDraft.mOwnerId)
    {
        return false;
    }
    mCrowdDraft.mAnchorBasis = basis;
    mCrowdDraft.mAnchorValid = valid;
    mCrowdDraft.mAnchorWarning = warning;
    return true;
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
    // Position/rotation/scale are the real gate, via epsilon compares -- a
    // dynamic crowd-facing driver (Wave/Track subject) left running on this
    // instance (e.g. right after ungrouping it, before that path was fixed to
    // clear the driver) would genuinely rotate it every frame and correctly
    // still fail here. A BARE mTransformRevision bump with no material
    // position/rotation/scale change is advisory only during preview: it
    // used to be a hard blocker on its own, which made an otherwise
    // perfectly placeable prototype "stale" the instant anything (even an
    // unrelated system) re-authored an identical-looking transform.
    if (delta.lengthSquared() > 1.0e-10 ||
        !same_rotation || fabsf(source->mScale - snap.mScale) > 0.00001f)
    {
        return reject("The prototype moved or changed scale; stage it again.");
    }
    if (snap.mGroupId.notNull())
    {
        return reject("Ungroup the prototype before building a crowd.");
    }
    if (source->mTransformRevision != snap.mTransformRevision)
    {
        if (reason)
        {
            *reason =
                "The prototype's transform revision changed since staging; "
                "re-stage before committing if this crowd looks wrong.";
        }
        return true;
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
    // A missing/invalid facing target for these modes used to hard-fail the
    // entire placement. It now falls back to the formation heading (see the
    // per-slot switch below) with a warning instead -- a "Blocked" failure
    // should mean a genuine blocker (missing prototype, invalid spec,
    // legality, clone allocation), not "you have not picked a point yet".
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
    request.mYawRadians = draft.mPatternYaw;
    request.mGridColumns = draft.mSpec.mGridColumns;
    request.mGridRows = draft.mSpec.mGridRows;
    const bool crowd_axes = shape == Shape::Grid || shape == Shape::StaggeredRows;
    request.mColumnSpacingMeters = crowd_axes
        ? draft.mSpec.mWidthSpacing : draft.mSpec.mFileSpacing;
    request.mRowSpacingMeters = crowd_axes
        ? draft.mSpec.mDepthSpacing : draft.mSpec.mRankSpacing;
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
        // A zero footprint let the edge-gap setting protect nothing: any
        // positive gap value still let members overlap because the solver
        // saw every member as a mathematical point. Estimate a real
        // avatar-sized clearance radius instead, scaled with the prototype.
        member.mFootprintRadiusMeters =
            (F64)llmax(0.30f, 0.30f * draft.mSource.mScale);
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
        !llfinite(draft.mPatternYaw) || !llfinite(draft.mFacingYawOffset))
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
    // FACING_AUTHOR ("Rotate with formation"): body facing = pattern yaw +
    // the independent facing-offset bias. Zero offset reproduces the
    // pre-split behaviour exactly (body facing tracks the pattern 1:1).
    LLQuaternion authored(
        draft.mPatternYaw + draft.mFacingYawOffset, LLVector3::z_axis);
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
        slot.mRowIndex = source_slot.mRowIndex;
        slot.mLocalX = source_slot.mLocalX*cos(draft.mPatternYaw) + source_slot.mLocalY*sin(draft.mPatternYaw);
        slot.mLocalY = -source_slot.mLocalX*sin(draft.mPatternYaw) + source_slot.mLocalY*cos(draft.mPatternYaw);
        slot.mFoot = draft.mAnchor +
            LLVector3d(source_slot.mLocalX, source_slot.mLocalY, 0.0);
        slot.mFoot.mdV[VZ] += draft.mHeight;
        slot.mScale = draft.mSource.mScale;
        slot.mRole = formationRole(draft.mSpec.mFormation,
                                  source_slot.mInputIndex);

        if (draft.mSpec.mTerrainConform)
        {
            LLVector3 agent = gAgent.getPosAgentFromGlobal(slot.mFoot);
            LLViewerRegion* region =
                LLWorld::getInstance()->getRegionFromPosAgent(agent);
            // A region existing is not proof its terrain data has actually
            // arrived -- LLSurface's height array starts at Z=0 everywhere
            // until real patch data streams in, so without checking
            // getHasReceivedData() an unloaded patch silently placed the
            // slot at sea level instead of falling back to the formation
            // plane.
            LLSurfacePatch* patch = region
                ? region->getLand().resolvePatchRegion(
                    region->getPosRegionFromGlobal(slot.mFoot))
                : nullptr;
            if (region && patch && patch->getHasReceivedData())
            {
                agent.mV[VZ] =
                    LLWorld::getInstance()->resolveLandHeightAgent(agent) +
                    draft.mHeight;
                slot.mFoot = gAgent.getPosGlobalFromAgent(agent);
            }
            else
            {
                // Best-effort: one region with no loaded terrain data must
                // not invalidate the ENTIRE crowd. Leave this single slot on
                // the flat formation plane (already computed above) and
                // surface a warning instead of failing the whole placement.
                validation.mPartial = true;
                if (validation.mMessage.empty())
                {
                    validation.mMessage =
                        "Terrain data is unavailable for one or more slots; "
                        "they were kept on the formation plane.";
                }
            }
        }

        F32 facing_yaw = draft.mPatternYaw;
        bool pure_yaw = false;
        bool full_quaternion_set = false;
        switch (draft.mSpec.mFacing)
        {
        case FACING_SOURCE:
            if (draft.mSpec.mPreserveSourceTilt)
            {
                // Explicit opt-in: reproduce the prototype's FULL
                // pitched/rolled quaternion, with the independent
                // facing-offset bias composed on top as an ADDITIONAL
                // world-Z yaw (applied after/on top of the preserved tilt,
                // in the v*q convention this codebase uses -- same-axis
                // yaw-only composition would be ambiguous once real
                // pitch/roll is involved, so this is a genuine rotation
                // composition, not just adding two yaw scalars). Without
                // this the offset spinner was silently a no-op whenever
                // preserve-tilt was checked.
                slot.mRotation = draft.mSource.mRotation *
                    LLQuaternion(draft.mFacingYawOffset, LLVector3::z_axis);
                full_quaternion_set = true;
            }
            else
            {
                // "Source yaw" now means what its label says: a pure
                // world-Z yaw extraction (same formula as
                // Instance::getYaw()), never a smuggled-in pitch/roll.
                const LLVector3 source_forward =
                    LLVector3::x_axis * draft.mSource.mRotation;
                facing_yaw =
                    atan2f(source_forward.mV[VY], source_forward.mV[VX]);
                pure_yaw = true;
            }
            break;
        case FACING_CAMERA:
            facing_yaw = yaw_to(slot.mFoot, camera_global, draft.mPatternYaw);
            pure_yaw = true;
            break;
        case FACING_CENTROID_IN:
            facing_yaw = yaw_to(slot.mFoot, draft.mAnchor, draft.mPatternYaw);
            pure_yaw = true;
            break;
        case FACING_OUTWARD:
            facing_yaw = yaw_to(draft.mAnchor, slot.mFoot, draft.mPatternYaw);
            pure_yaw = true;
            break;
        case FACING_WORLD_POINT:
        case FACING_TARGET_ACTOR:
        case FACING_TRACK_SUBJECT:
            if (draft.mSpec.mHasFacingPoint)
            {
                facing_yaw = yaw_to(slot.mFoot,
                                    draft.mSpec.mFacingPointGlobal,
                                    draft.mPatternYaw);
                pure_yaw = true;
            }
            else
            {
                // Missing facing target: fall back to the formation heading
                // (facing_yaw/pure_yaw already default to that) instead of
                // failing the whole placement -- surfaced as a warning.
                validation.mPartial = true;
                if (validation.mMessage.empty())
                {
                    validation.mMessage =
                        "No facing target is set; using the formation "
                        "heading instead.";
                }
            }
            break;
        case FACING_AISLE:
        case FACING_FACE_ACROSS:
        {
            const LLVector3d axis(
                cosf(draft.mPatternYaw), sinf(draft.mPatternYaw), 0.0);
            const LLVector3d relative = slot.mFoot - draft.mAnchor;
            const LLVector3d nearest = draft.mAnchor +
                axis * (relative.mdV[VX] * axis.mdV[VX] +
                        relative.mdV[VY] * axis.mdV[VY]);
            facing_yaw = yaw_to(slot.mFoot, nearest, draft.mPatternYaw);
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
            facing_yaw = yaw_to(slot.mFoot, centroid, draft.mPatternYaw);
            pure_yaw = true;
            break;
        }
        case FACING_LOOSE:
            facing_yaw = yaw_to(slot.mFoot, draft.mAnchor, draft.mPatternYaw) +
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
                    draft.mPatternYaw);
                pure_yaw = true;
            }
            break;
        case FACING_WAVE:
            facing_yaw = draft.mPatternYaw +
                sinf((F32)source_slot.mInputIndex * 0.65f) *
                (35.f * DEG_TO_RAD);
            pure_yaw = true;
            break;
        case FACING_MIRROR_SOURCE:
            facing_yaw = draft.mPatternYaw + F_PI;
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
        if (!full_quaternion_set)
        {
            // The independent facing-offset bias applies on top of every
            // yaw-based resolution (including the formation-heading
            // fallback carried by `authored`), never to a preserved full
            // source quaternion.
            slot.mRotation = pure_yaw
                ? LLQuaternion(facing_yaw + draft.mFacingYawOffset,
                              LLVector3::z_axis)
                : authored;
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
        // A per-slot best-effort warning (terrain data unavailable / no
        // facing target set) accumulated in validation.mMessage above must
        // survive here -- overwriting it unconditionally with the generic
        // success stats discarded the ONLY place that explained which slots
        // fell back and why, even though mPartial correctly stayed set.
        const std::string partial_warning =
            validation.mPartial ? validation.mMessage : std::string();
        validation.mMessage = llformat(
            "%u previewed · fitted radius %.2f m · edge gap %.2f m",
            validation.mPlacedCount, validation.mPlacedRadius,
            validation.mActualMinEdgeGap);
        if (!partial_warning.empty())
        {
            validation.mMessage += " \xC2\xB7 " + partial_warning;
        }
        // The solver's axis/count advisories must survive the success summary.
        // partial_warning already includes this diagnostic when it is retained.
        if (partial_warning.empty() && crowd_axes &&
            solved.mDiagnostic.find("; ") != std::string::npos)
        {
            validation.mMessage += solved.mDiagnostic.substr(solved.mDiagnostic.find("; "));
        }
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
    // Epsilon transform comparison only -- NOT a bit-exact revision-arithmetic
    // gate. apply_slot(0) bumps the revision by exactly 1, but so does any
    // other setYaw()/setFootGlobal()/setTransform()/setScale() call a runtime
    // consumer happens to make with the SAME resulting values (harmless: the
    // epsilon checks below already say "still exactly where we put it"). The
    // old `mTransformRevision == draft.mSource.mTransformRevision + 1` hard
    // requirement failed in exactly that harmless case and rolled back a
    // successful placement over nothing.
    const bool source_still_exact =
        placed_source &&
        placed_source->mGroupId.isNull() &&
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
        // only the copies belong to this failed transaction. apply_slot(0)
        // above already succeeded and moved the prototype (source_was_changed
        // is true), so THIS rollback must also restore the prototype's
        // original pre-commit transform -- passing a hardcoded false here
        // used to leave the prototype moved even though every copy was
        // removed.
        rollback(source_was_changed);
        result.mMessage =
            "The prototype changed during crowd creation; copies were removed.";
        return result;
    }

    for (size_t i = 0; i < ids.size(); ++i)
    {
        Instance* member = getInstance(ids[i]);
        if (!member) continue;
        // Always clear whatever crowd-facing driver this member may have
        // INHERITED (e.g. duplicated from a prototype that itself used to be
        // a Wave/Track-subject crowd member) before applying this crowd's
        // own facing policy. Without this, a member silently kept animating
        // under the old formation's driver even though the new formation
        // never asked for it.
        member->mCrowdFacing = FACING_AUTHOR;
        member->mCrowdSlot = (U32)i;
        member->mCrowdId = draft.mSessionId;
        member->mCrowdRow = draft.mSlots[i].mRowIndex;
        member->mCrowdLocalX = draft.mSlots[i].mLocalX;
        member->mCrowdLocalY = draft.mSlots[i].mLocalY;
        member->mCrowdBaseYaw = draft.mPatternYaw;
        // Persist the independent facing-offset bias onto the runtime
        // driver too -- the preview already composed it in (see the
        // resolveCrowdPlacement() facing switch), but only mCrowdBaseYaw was
        // being stored here, so a live Track/Wave update on first tick threw
        // the authored offset away.
        member->mCrowdFacingYawOffset = draft.mFacingYawOffset;
        if (draft.mSpec.mFacing == FACING_TRACK_SUBJECT ||
            draft.mSpec.mFacing == FACING_WAVE)
        {
            member->mCrowdFacing = draft.mSpec.mFacing;
            member->mCrowdFacingStart = LLTimer::getTotalSeconds();
            member->mCrowdFacingClock = {0.0, false, true};
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

    applyCrowdVariation(ids.front(), draft.mSpec.mSeed, true, true, true);
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
                mCrowdDraft.mPatternYaw, LLVector3::z_axis);
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
        if (inst.mKind == BACKING_OVERLAY && isVisible(inst) && resolveOverlayAvatar(inst)
            && std::find(out.begin(), out.end(), inst.mOverlayAvatar.notNull() ? inst.mOverlayAvatar : inst.mSource) == out.end())
        {
            out.push_back(inst.mOverlayAvatar.notNull() ? inst.mOverlayAvatar : inst.mSource);
        }
    }
}


bool ALGhostStudio::isDeleting(const LLUUID& actor) const
{
    const LLUUID owner = actorOwner(actor);
    if (const Instance* inst = getInstance(owner)) return inst->mDeleting;
    if (const Instance* inst = findInstanceByRuntime(owner)) return inst->mDeleting;
    if (const auto* group = groupForMember(actor))
        for (const auto& member : group->mMembers)
            if (const auto* inst = getInstance(member.mInstanceId); inst && inst->mDeleting) return true;
    return false;
}
void ALGhostStudio::refreshSoloVisibility()
{
    ++mInstanceRevision;
    for (const auto& inst : mInstances)
        if (LLGhostAvatar* ghost = resolveEntityClone(inst.mId))
            set_entity_clone_visible(ghost, isVisible(inst), mShowAll && inst.mEnabled && mSolo && !inst.mSoloVisible);
}
void ALGhostStudio::setSolo(const std::vector<LLUUID>& ids)
{
    std::set<LLUUID> members;
    for (const auto& id : ids)
        for (const auto& member : groupMembers(id))
            if (!isDeleting(member)) members.insert(member);
    mSolo = !members.empty();
    for (auto& inst : mInstances) inst.mSoloVisible = members.count(inst.mId) != 0;
    refreshSoloVisibility();
}
void ALGhostStudio::beginAppearance(const LLUUID& id, bool fade)
{
    Instance* inst = getInstance(id);
    if (!inst) return;
    if (inst->mStartOffset > 0.f) mOffsetInstances.insert(id);
    inst->mSoloVisible = true; // additions join the session's solo set
    if (LLGhostAvatar* ghost = resolveEntityClone(id)) set_entity_clone_visible(ghost, isVisible(*inst));
    static LLCachedControl<F32> duration(gSavedSettings, "GhostStudioFadeIn", 0.f);
    const F32 seconds = llclamp(F32(duration), 0.f, 5.f);
    if (inst->mKind == BACKING_OVERLAY && inst->mHasAnimationSource && inst->mOverlayAvatar.isNull())
    {
        acquireOverlayHelper(id);
        inst = getInstance(id); // creation may pump observers
        if (!inst) return;
    }
    if (!fade) return; // region recovery restores the prior visibility without a fade
    if (seconds > 0.f)
    {
        inst->mFadeAlpha = 0.f;
        if (gAgent.getRegion()) mFadeRegion = gAgent.getRegion()->getRegionID();
        mFades[id].mFade = {LLTimer::getTotalSeconds(), seconds, 0.f, false};
        if (inst->mEntityId.notNull()) LLDirectorCast::instance().setCloneFadeAlpha(inst->mEntityId, 0.f);
    }
}
void ALGhostStudio::requestRemoveInstance(const LLUUID& id)
{
    const auto members = groupMembers(id);
    static LLCachedControl<F32> duration(gSavedSettings, "GhostStudioFadeOut", 0.f);
    const F32 seconds = llclamp(F32(duration), 0.f, 5.f);
    if (seconds <= 0.f) { removeInstance(id); return; }
    const F64 now = LLTimer::getTotalSeconds();
    if (gAgent.getRegion()) mFadeRegion = gAgent.getRegion()->getRegionID();
    for (const LLUUID& member : members)
        if (Instance* inst = getInstance(member); inst && !inst->mDeleting)
        {
            inst->mDeleting = true;
            inst->mState = STATE_TEARING_DOWN;
            mFades[member].mFade = {now, seconds, inst->mFadeAlpha, true};
        }
    if (isDeleting(mSelected)) setSelected(LLUUID::null);
    ++mInstanceRevision;
    LLDirectorCast& cast = LLDirectorCast::instance();
    if (isDeleting(cast.getActiveActorId())) cast.setActiveActorId(LLUUID::null);
    cast.touchCast();
    if (mSolo && std::none_of(mInstances.begin(), mInstances.end(),
        [](const Instance& item) { return item.mSoloVisible && !item.mDeleting; })) setSolo({});
}
void ALGhostStudio::updateFades()
{
    const F64 now = LLTimer::getTotalSeconds();
    const bool interrupted = !gAgent.getRegion() || gAgent.getRegion()->getRegionID() != mFadeRegion ||
        gAgent.getTeleportState() != LLAgent::TELEPORT_NONE;
    std::vector<LLUUID> finished;
    for (auto it = mFades.begin(); it != mFades.end(); )
    {
        Instance* inst = getInstance(it->first);
        if (!inst) { it = mFades.erase(it); continue; }
        const bool missing = inst->mKind == BACKING_ENTITY_CLONE && !resolveEntityClone(inst->mId);
        const bool done = interrupted || missing || it->second.mFade.done(now);
        inst->mFadeAlpha = done ? (it->second.mFade.removing ? 0.f : 1.f) : it->second.mFade.alpha(now);
        if (inst->mEntityId.notNull())
            LLDirectorCast::instance().setCloneFadeAlpha(inst->mEntityId, inst->mFadeAlpha);
        if (!done) { ++it; continue; }
        if (it->second.mFade.removing) finished.push_back(it->first);
        it = mFades.erase(it);
    }
    // Whole groups were retired together, with identical duration; removeInstance
    // owns their authoritative final rekey/deletion transaction.
    for (const auto& id : finished) removeInstance(id);
}

bool ALGhostStudio::swapSource(const LLUUID& id, const LLUUID& source_id, const std::string& label)
{
    mSwapStatus = "Swap source unavailable: source or clone is not ready";
    Instance* inst = getInstance(id);
    if (!inst || inst->mDeleting) return false;
    const LLUUID source_owner = actorOwner(source_id);
    const Instance* source_instance = findInstanceByRuntime(source_owner);
    if (source_owner == id || (source_instance && source_instance->mId == id) || isPrivateOverlayRuntime(source_id)) return false;
    const LLUUID current_source = inst->mHasAnimationSource ? inst->mAppearanceSource : inst->mSource;
    const bool removed = inst->mHasAnimationSource ? inst->mAppearanceSourceRemoved : inst->mSourceRemoved;
    if (!removed && (LLDirectorCast::canonicalActorId(current_source) == LLDirectorCast::canonicalActorId(source_id) ||
        (LLDirectorCast::isSelf(current_source) && LLDirectorCast::isSelf(source_id)))) return true;
    LLVOAvatar* source = LLDirectorCast::instance().resolve(source_id);
    if (!source || source->isDead() || !source->isBuilt() || !source->isFullyLoaded() || isDeleting(source_id)) return false;
    const Instance before = *inst;
    LLVOAvatar* old_draw = resolveOverlayAvatar(before);
    const LLUUID tint_seed = before.mHasAnimationSource ? before.mAppearanceSwapTintSeed :
        old_draw ? old_draw->getID() : before.mSource;
    Instance candidate = before;
    if (!candidate.mHasAnimationSource)
    {
        candidate.mHasAnimationSource = true;
        candidate.mAnimationSource = before.mSource;
        candidate.mAnimationSourceRemoved = before.mSourceRemoved;
    }
    candidate.mAppearanceSource = source_id;
    candidate.mAppearanceSourceRemoved = false;
    candidate.mSourceLabel = label;
    S32 attachments = 0;
    LLGhostAvatar* replacement = before.mKind == BACKING_ENTITY_CLONE ? createEntityRuntime(candidate, attachments) : nullptr;
    const LLUUID overlay = before.mKind == BACKING_OVERLAY ? reserveOverlayHelper(candidate) : LLUUID::null;
    inst = getInstance(id);
    if ((before.mKind == BACKING_OVERLAY && overlay.isNull()) || (before.mKind == BACKING_ENTITY_CLONE && !replacement) || !inst || inst->mDeleting || inst->mEntityId != before.mEntityId || inst->mSource != before.mSource ||
        inst->mOverlayAvatar != before.mOverlayAvatar || inst->mAppearanceSource != before.mAppearanceSource)
    {
        if (replacement) { replacement->releaseClonedAttachments(); replacement->markForDeath(); }
        if (overlay.notNull()) releaseOverlayReference(overlay);
        mSwapStatus = "Swap source failed: copied body or attachments incomplete; previous appearance retained";
        return false;
    }
    // Preserve authored state on the stable record. The transaction performs the
    // only entity identity change, so all outside references follow atomically.
    inst->mAppearanceSource = candidate.mAppearanceSource;
    inst->mAppearanceSourceRemoved = false;
    inst->mSourceLabel = label;
    inst->mAnimationSource = candidate.mAnimationSource;
    inst->mAnimationSourceRemoved = candidate.mAnimationSourceRemoved;
    inst->mAppearanceSwapTintSeed = tint_seed;
    inst->mHasAnimationSource = true;
    if (before.mKind == BACKING_ENTITY_CLONE)
    {
        LLGhostAvatar* old = resolveEntityClone(id);
        onEntityRuntimeReplaced(*inst, replacement->getID(), replacement);
        if (old && before.mDriveMode == DRIVE_FROZEN)
        {
            replacement->copyHeldPoseFrom(old);
            if (Instance* current = getInstance(id)) current->mPendingFreezeFrames = 0;
        }
        if (old) { set_entity_clone_visible(old, false); old->releaseClonedAttachments(); old->markForDeath(); }
    }
    else
    {
        releaseOverlayHelper(id);
        inst->mOverlayAvatar = overlay;
        mPrivateOverlayRuntimes[id] = overlay;
        invalidateOverlayPose(*inst);
    }
    if (Instance* current = getInstance(id))
    {
        current->mOffsetPose.reset();
        current->mState = STATE_READY;
        beginAppearance(id);
    }
    mOverlayHistory.erase(id);
    mSwapStatus = "Source swapped";
    ++mInstanceRevision;
    return true;
}

bool ALGhostStudio::setStartOffset(const LLUUID& id, F32 seconds)
{
    Instance* inst = getInstance(id);
    if (!inst || inst->mDeleting || !std::isfinite(seconds)) return false;
    inst->mStartOffset = llclamp(seconds, 0.f, 10.f);
    if (inst->mStartOffset > 0.f) mOffsetInstances.insert(id);
    else mOffsetInstances.erase(id);
    if (LLGhostAvatar* ghost = resolveEntityClone(id)) ghost->setStartOffset(inst->mStartOffset);
    if (seconds == 0.f)
    {
        inst->mOffsetPose.reset();
        const LLUUID key = id;
        mOverlayHistory.erase(key);
    }
    ++mInstanceRevision;
    return true;
}
std::string ALGhostStudio::offsetStatus(const Instance& inst) const
{
    if (inst.mStartOffset <= 0.f) return "Start offset off";
    if (inst.mKind == BACKING_ENTITY_CLONE && inst.mDriveMode == DRIVE_DIRECTED)
        return llformat("Animation phase +%.2fs (loops wrap)", inst.mStartOffset);
    if (inst.mDriveMode == DRIVE_FROZEN || inst.mPose == POSE_FROZEN)
        return "Start offset held while frozen";
    if (inst.mKind == BACKING_OVERLAY && !inst.mOffsetPose)
        return "Offset inactive: no renderable rigged pose history (source may still be loading)";
    F32 available = 0.f;
    if (LLGhostAvatar* ghost = resolveEntityClone(inst.mId)) available = ghost->offsetHistoryLength();
    else if (const auto found = mOverlayHistory.find(inst.mId); found != mOverlayHistory.end() && found->second.mFrames.size() > 1)
        available = F32(found->second.mFrames.back().first-found->second.mFrames.front().first);
    return llformat("Live pose delay %.2fs; history cap 2s (%.2fs available)",
        llmin(llmin(inst.mStartOffset, 2.f), available), available);
}
void ALGhostStudio::syncOffsets(const std::vector<LLUUID>& ids)
{
    std::set<LLUUID> selected(ids.begin(), ids.end());
    std::set<LLUUID> crowds;
    for (const auto& id : ids)
        if (const Instance* inst = getInstance(id); inst && inst->mCrowdId.notNull()) crowds.insert(inst->mCrowdId);
    for (const auto& crowd : crowds)
    {
        bool whole = true;
        for (const auto& inst : mInstances)
            if (inst.mCrowdId == crowd && !inst.mDeleting && !selected.count(inst.mId)) whole = false;
        if (whole) gSavedSettings.setS32("GhostStudioStartMode", 0);
    }
    for (const auto& id : ids) setStartOffset(id, 0.f);
}
void ALGhostStudio::applyCrowdVariation(const LLUUID& member_id, U32 seed, bool timing, bool assign_looks, bool building)
{
    const Instance* member = getInstance(member_id);
    if (!member || member->mCrowdId.isNull()) return;
    const LLUUID crowd = member->mCrowdId;
    const S32 mode = gSavedSettings.getS32("GhostStudioStartMode");
    const S32 direction = gSavedSettings.getS32("GhostStudioRippleFrom");
    const F32 spread = llclamp(gSavedSettings.getF32("GhostStudioStartSpread"), 0.f, 10.f);
    const bool mix = gSavedSettings.getS32("GhostStudioLooksMode") == 1;
    const bool by_row = gSavedSettings.getS32("GhostStudioLookSpread") == 1;
    std::vector<S32> looks;
    for (S32 i=1; i<=6; ++i)
    {
        const S32 look = remapRetiredOverlayStyle(gSavedSettings.getS32("GhostStudioMixLook"+std::to_string(i)));
        if (isOverlayStyle(look) && std::find(looks.begin(), looks.end(), look) == looks.end()) looks.push_back(look);
    }
    F64 low = std::numeric_limits<F64>::max(), high = -low;
    for (const auto& inst : mInstances)
        if (inst.mCrowdId == crowd && !inst.mDeleting)
        {
            const F64 v = ALCloneQol::ripple(inst.mCrowdLocalX, inst.mCrowdLocalY, direction);
            low = llmin(low, v); high = llmax(high, v);
        }
    std::vector<std::pair<LLUUID, F32>> offsets;
    for (auto& inst : mInstances)
        if (inst.mCrowdId == crowd && !inst.mDeleting)
        {
            inst.mCrowdSeed = seed;
            U64 stable = 14695981039346656037ULL;
            for (U8 byte : inst.mId.mData) stable = (stable ^ byte)*1099511628211ULL;
            const F64 unit = mode == 1 ? ALCloneQol::randomUnit(seed, stable) : mode == 2 ?
                ALCloneQol::normalise(ALCloneQol::ripple(inst.mCrowdLocalX, inst.mCrowdLocalY, direction), low, high) : 0.;
            if (timing && !(building && mode == 0)) offsets.emplace_back(inst.mId, F32(unit*spread));
            if (assign_looks && inst.mKind == BACKING_OVERLAY)
            {
                if (mix && !looks.empty())
                {
                    if (!inst.mCrowdMixed) inst.mCrowdSingleStyle = resolveOverlayStyle(inst.mStyle);
                    inst.mCrowdMixed = true;
                    inst.mStyle = looks[ALCloneQol::lookIndex(seed, stable, inst.mCrowdRow, by_row, (U32)looks.size())];
                }
                else if (inst.mCrowdMixed)
                {
                    inst.mStyle = resolveOverlayStyle(inst.mCrowdSingleStyle);
                    inst.mCrowdMixed = false;
                }
            }
        }
    for (const auto& offset : offsets) setStartOffset(offset.first, offset.second);
    ++mInstanceRevision;
}
void ALGhostStudio::refreshOverlayOffset(Instance& inst, F64 now)
{
    if (inst.mStartOffset <= 0.f || inst.mPose == POSE_FROZEN) return;
    auto& history = mOverlayHistory[inst.mId];
    if (now >= history.mNextCapture || history.mFrames.empty())
    {
        auto pose = std::make_shared<Instance>();
        pose->mSource = inst.mSource;
        pose->mOverlayAvatar = inst.mOverlayAvatar;
        if (captureOverlayPose(*pose))
        {
            if (inst.mPoseRateHz > 0.f && !inst.mCadencePalettes.empty())
            {
                pose->mFrozenPalettes = inst.mCadencePalettes;
                pose->mFrozenBounds[0] = inst.mCadenceBounds[0];
                pose->mFrozenBounds[1] = inst.mCadenceBounds[1];
                pose->mFrozenAttachMats = inst.mCadenceAttachMats;
                pose->mFrozenFootAgent = inst.mCadenceFootAgent;
                pose->mFrozenBoundsRadius = inst.mCadenceBoundsRadius;
                pose->mFrozenHeadAgent = inst.mCadenceHeadAgent;
                pose->mFrozenHeadValid = inst.mCadenceHeadValid;
            }
            if (inst.mPoseRateHz > 0.f && !inst.mCadencePalettes.empty()) pose->mRotation = inst.mCadenceRotation;
            else if (LLVOAvatar* source = resolveOverlayAvatar(inst)) pose->mRotation = source->getRotation();
            history.mFrames.emplace_back(now, std::move(pose));
            while (history.mFrames.size() > 241 ||
                (history.mFrames.size() > 1 && history.mFrames.front().first < now-2.)) history.mFrames.pop_front();
        }
        history.mNextCapture = now+1./120.; // every frame at the 4K/60 recording rate
    }
    const F64 target = now-llmin(inst.mStartOffset, 2.f);
    for (const auto& frame : history.mFrames)
    {
        inst.mOffsetPose = frame.second;
        if (frame.first >= target) break;
    }
}

LLVOAvatar* ALGhostStudio::resolveOverlayAvatar(const Instance& inst)
{
    if (inst.mOverlayAvatar.notNull())
    {
        LLViewerObject* object = gObjectList.findObject(inst.mOverlayAvatar);
        LLVOAvatar* avatar = object ? object->asAvatar() : nullptr;
        return avatar && !avatar->isDead() ? avatar : nullptr;
    }
    if (inst.mHasAnimationSource && inst.mKind == BACKING_OVERLAY) return nullptr;
    return resolveSourceAvatar(inst);
}

LLUUID ALGhostStudio::actorOwner(const LLUUID& actor) const
{
    LLViewerObject* object = gObjectList.findObject(actor);
    if (!object) object = gObjectList.findObject(LLDirectorCast::canonicalActorId(actor));
    if (LLVOAvatar* avatar = object ? object->asAvatar() : nullptr)
        if (avatar->isControlAvatar() && avatar->getAttachedAvatar()) object = avatar->getAttachedAvatar();
    for (LLViewerObject* current = object; current; current = static_cast<LLViewerObject*>(current->getParent()))
        if (current->isAvatar()) return current->getID();
    return actor;
}
bool ALGhostStudio::isPrivateOverlayRuntime(const LLUUID& actor) const
{
    if (actor.isNull()) return false;
    if (mPrivateOverlayActors.count(actor)) return true;
    const LLUUID owner = actorOwner(actor);
    const auto* ghost = dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(owner));
    return (ghost && ghost->mOverlayHelper) || mOverlayHelperRefs.count(owner) != 0;
}

void ALGhostStudio::invalidateOverlayPose(Instance& inst)
{
    inst.mCadencePalettes.clear();
    if (inst.mPose == POSE_FROZEN) { inst.mFrozenPalettes.clear(); inst.mFrozenAttachMats.clear(); }
    inst.mCadenceFootAgent = LLVector3::zero;
    inst.mCadenceHeadValid = false;
    inst.mNextPoseRefresh = 0.;
    inst.mOffsetPose.reset();
    mOverlayHistory.erase(inst.mId);
}
void ALGhostStudio::releaseOverlayHelper(const LLUUID& id)
{
    const auto entry = mPrivateOverlayRuntimes.find(id);
    if (entry == mPrivateOverlayRuntimes.end()) return;
    const LLUUID runtime = entry->second;
    mPrivateOverlayRuntimes.erase(entry);
    if (Instance* inst = getInstance(id)) inst->mOverlayAvatar.setNull();
    releaseOverlayReference(runtime);
}
void ALGhostStudio::releaseOverlayReference(const LLUUID& runtime)
{
    const auto ref = mOverlayHelperRefs.find(runtime);
    if (ref == mOverlayHelperRefs.end() || --ref->second != 0) return;
    if (auto* helper = dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(runtime)))
    {
        helper->releaseClonedAttachments();
        helper->markForDeath();
    }
    mOverlayHelperRefs.erase(ref);
}
bool ALGhostStudio::acquireOverlayHelper(const LLUUID& id)
{
    Instance* inst = getInstance(id);
    if (!inst) return false;
    invalidateOverlayPose(*inst);
    mPrivateOverlayRuntimes[id] = LLUUID::null; // failed attempts remain recoverable
    const LLUUID helper = reserveOverlayHelper(*inst);
    inst = getInstance(id);
    if (helper.isNull()) return false;
    if (!inst) { releaseOverlayReference(helper); return false; }
    inst->mOverlayAvatar = helper;
    mPrivateOverlayRuntimes[id] = helper;
    return true;
}
LLUUID ALGhostStudio::reserveOverlayHelper(const Instance& candidate)
{
    // Creation may pump observers: copy every input before entering that boundary.
    Instance state = candidate;
    for (const auto& other : mInstances)
        if (other.mId != state.mId && other.mHasAnimationSource && other.mOverlayAvatar.notNull() &&
            other.mAppearanceSource == state.mAppearanceSource && other.mAnimationSource == state.mAnimationSource &&
            other.mAppearanceSourceRemoved == state.mAppearanceSourceRemoved && other.mAnimationSourceRemoved == state.mAnimationSourceRemoved && resolveOverlayAvatar(other))
        {
            ++mOverlayHelperRefs[other.mOverlayAvatar];
            return other.mOverlayAvatar;
        }
    S32 attachments = 0;
    LLGhostAvatar* helper = createEntityRuntime(state, attachments);
    if (!helper) return LLUUID::null;
    state.mStartOffset = 0.f;
    state.mDriveMode = DRIVE_TRUE_MIRROR;
    state.mPose = POSE_LIVE;
    state.mScale = 1.f;
    helper->mOverlayHelper = true;
    rememberOverlayDomain(helper);
    applyEntityRuntimeState(state, helper);
    helper->setRenderOnlyHidden(true);
    helper->clearDrawableState(LLDrawable::FORCE_INVISIBLE, true);
    ++mOverlayHelperRefs[helper->getID()];
    return helper->getID();
}

LLVOAvatar* ALGhostStudio::resolveCopiedAvatar(const Instance& inst)
{
    if (inst.mHasAnimationSource)
        return inst.mAppearanceSourceRemoved ? nullptr : LLDirectorCast::instance().resolve(inst.mAppearanceSource);
    return resolveSourceAvatar(inst);
}

void ALGhostStudio::rememberOverlayDomain(LLViewerObject* helper)
{
    std::vector<LLViewerObject*> pending{helper};
    while (!pending.empty())
    {
        LLViewerObject* object = pending.back(); pending.pop_back();
        if (!object) continue;
        mPrivateOverlayActors.insert(object->getID());
        mPrivateOverlayActors.insert(LLDirectorCast::canonicalActorId(object->getID()));
        if (LLControlAvatar* control = object->getControlAvatar()) mPrivateOverlayActors.insert(control->getID());
        for (const auto& child : object->getChildren()) pending.push_back(child);
    }
}
