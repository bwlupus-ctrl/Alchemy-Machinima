/**
 * @file alghostspawnengine.cpp
 * @brief Capability-selected front end for Ghost Studio clone backends.
 * @note Adapts the existing Overlay and Entity transactions; it does not
 *       replace their renderable representations.
 *
 * UNBUILT — pending Claude compile/link/in-world verification.
 */

#include "llviewerprecompiledheaders.h"

#include "alghostspawnengine.h"

#include "alghostmaterialresolver.h"
#include "alghoststudio.h"
#include "lldirectorcast.h"
#include "llghostavatar.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"

#include <set>

namespace
{
const char* backend_name(ALGhostCloneBackend backend)
{
    return backend == ALGhostCloneBackend::OVERLAY ? "overlay" : "entity";
}

ALGhostStudio::ERenderIntent render_intent(
    const ALGhostCloneRequest& request)
{
    return request.mRender == ALGhostCloneRequest::Render::DEFERRED
        ? ALGhostStudio::RENDER_REQUEST_DEFERRED
        : ALGhostStudio::RENDER_FORCE_FORWARD;
}

void apply_request_metadata(ALGhostStudio::Instance& instance,
                            const ALGhostResolvedCloneSource& source,
                            const ALGhostCloneRequest& request)
{
    instance.mSource = source.mPersistentSourceHandle;
    instance.mSourceLabel = source.mLabel;
    instance.mRenderIntent = render_intent(request);
}

bool apply_update(ALGhostStudio& studio,
                  const LLUUID& instance_id,
                  const ALGhostCloneUpdate& update)
{
    ALGhostStudio::Instance* instance = studio.getInstance(instance_id);
    if (!instance)
    {
        return false;
    }

    const bool has_transform =
        update.mHasFoot || update.mHasRotation || update.mHasScale;
    // Keep this singular API honest: Studio's unit helpers deliberately
    // expand grouped members, and a two-operation update cannot roll back its
    // first mutation if the second fails.
    if (instance->mGroupId.notNull() ||
        (has_transform && update.mHasEnabled))
    {
        return false;
    }
    // transformUnit() historically reports true for a non-group Entity even
    // when its runtime vanished. Do not let the unified result overstate that
    // case or mutate only the Studio record.
    if ((has_transform || update.mHasEnabled) &&
        instance->mKind == ALGhostStudio::BACKING_ENTITY_CLONE &&
        !studio.resolveEntityClone(instance_id))
    {
        return false;
    }
    if (has_transform &&
        !studio.transformUnit(
            instance_id,
            update.mHasFoot ? update.mFootGlobal : instance->mFootGlobal,
            update.mHasRotation ? update.mRotation : instance->mRotation,
            update.mHasScale ? update.mScale : instance->mScale))
    {
        return false;
    }
    if (update.mHasEnabled &&
        !studio.setInstanceEnabled(instance_id, update.mEnabled))
    {
        return false;
    }
    return true;
}

class ALGhostOverlayBackend final : public ALGhostSpawnBackend
{
public:
    ALGhostCloneBackend kind() const override
    {
        return ALGhostCloneBackend::OVERLAY;
    }

    ALGhostBackendCapabilities capabilities() const override
    {
        ALGhostBackendCapabilities result;
        result.mOwnsRenderable = false;
        result.mRenderSourceDependent = true;
        result.mDefaultDriveMirrorsSource = false;
        result.mIndependentTransform = true;
        result.mIndependentPose = false;
        result.mDirectorActor = false;
        result.mNativeSceneMaterials = false;
        result.mPbrExactVerified = false;
        return result;
    }

    bool satisfies(const ALGhostCloneRequest& request,
                   std::string& reason) const override
    {
        if (request.mPersistence !=
                ALGhostCloneRequest::Persistence::EPHEMERAL ||
            request.mControl == ALGhostCloneRequest::Control::DIRECTOR ||
            request.mFidelity !=
                ALGhostCloneRequest::Fidelity::MIRROR_EXACT ||
            request.mRequirePbrExact)
        {
            reason =
                "overlay cannot satisfy persistence, Director, independent "
                "snapshot, or verified exact-PBR requirements";
            return false;
        }
        if (request.mRequireRenderMode &&
            request.mRender == ALGhostCloneRequest::Render::DEFERRED)
        {
            reason =
                "overlay Deferred is a hybrid route with per-face Forward "
                "fallback and cannot be guaranteed";
            return false;
        }
        return true;
    }

    ALGhostSpawnResult spawn(
        ALGhostStudio& studio,
        const ALGhostResolvedCloneSource& source,
        const ALGhostCloneRequest& request) override
    {
        ALGhostSpawnResult result;
        result.mBackend = kind();
        result.mCapabilities = capabilities();

        ALGhostStudio::Instance* instance =
            studio.addInstance(source.mPersistentSourceHandle);
        if (!instance)
        {
            result.mStatus = ALGhostSpawnStatus::BACKEND_FAILED;
            result.mMessage = "overlay backend could not create an instance";
            return result;
        }

        // LLActorMover keeps EGhostStyle file-local; value 1 is its
        // GHOST_STYLE_CLONE and is required by the deferred clone queue.
        instance->mStyle = 1;
        apply_request_metadata(*instance, source, request);
        result.mStatus = ALGhostSpawnStatus::OK;
        result.mEffectiveRenderRoute =
            request.mRender == ALGhostCloneRequest::Render::FORWARD
                ? ALGhostRenderRoute::OVERLAY_FORCE_FORWARD
                : ALGhostRenderRoute::OVERLAY_DEFERRED_HYBRID;
        result.mEffectiveRenderRouteGuaranteed =
            request.mRender == ALGhostCloneRequest::Render::FORWARD;
        result.mInstanceIds.push_back(instance->mId);
        result.mMessage = "overlay clone committed";
        return result;
    }

    bool update(ALGhostStudio& studio,
                const LLUUID& instance_id,
                const ALGhostCloneUpdate& update_request) override
    {
        return apply_update(studio, instance_id, update_request);
    }

    bool lock(ALGhostStudio& studio,
              const LLUUID& instance_id) override
    {
        // This freezes palette/attachment transforms; it does not turn the
        // overlay into an owned or source-independent entity.
        return studio.freezeInstance(instance_id);
    }

    bool despawn(ALGhostStudio& studio,
                 const LLUUID& instance_id) override
    {
        studio.removeInstance(instance_id);
        return studio.getInstance(instance_id) == nullptr;
    }
};

class ALGhostEntityBackend final : public ALGhostSpawnBackend
{
public:
    ALGhostCloneBackend kind() const override
    {
        return ALGhostCloneBackend::ENTITY;
    }

    ALGhostBackendCapabilities capabilities() const override
    {
        ALGhostBackendCapabilities result;
        result.mOwnsRenderable = true;
        result.mRenderSourceDependent = false;
        result.mDefaultDriveMirrorsSource = true;
        result.mIndependentTransform = true;
        result.mIndependentPose = true;
        result.mDirectorActor = true;
        result.mNativeSceneMaterials = true;
        result.mPbrExactVerified = false;
        return result;
    }

    bool satisfies(const ALGhostCloneRequest& request,
                   std::string& reason) const override
    {
        if (request.mRequirePbrExact)
        {
            reason =
                "native scene materials are supported, but exact PBR has not "
                "passed the clone acceptance matrix";
            return false;
        }
        if (request.mFidelity ==
            ALGhostCloneRequest::Fidelity::INDEPENDENT_SNAPSHOT)
        {
            reason =
                "independent appearance snapshots require owned copies of "
                "mutable local/BoM composites; pointer pinning is not immutable";
            return false;
        }
        if (request.mRequireRenderMode)
        {
            reason =
                "entity objects follow the scene pipeline and cannot guarantee "
                "a per-object Forward/Deferred technique";
            return false;
        }
        if (request.mPersistence ==
                ALGhostCloneRequest::Persistence::EPHEMERAL &&
            request.mControl == ALGhostCloneRequest::Control::DIRECTOR)
        {
            reason =
                "Director Entity requests must declare persistent lifecycle "
                "semantics";
            return false;
        }
        return true;
    }

    ALGhostSpawnResult spawn(
        ALGhostStudio& studio,
        const ALGhostResolvedCloneSource& source,
        const ALGhostCloneRequest& request) override
    {
        ALGhostSpawnResult result;
        result.mBackend = kind();
        result.mCapabilities = capabilities();

        ALGhostStudio::Instance* instance =
            studio.spawnEntityClone(source.mPersistentSourceHandle,
                                    source.mLabel);
        if (!instance)
        {
            result.mStatus = ALGhostSpawnStatus::BACKEND_FAILED;
            result.mMessage =
                "entity backend rolled back an incomplete clone";
            return result;
        }

        apply_request_metadata(*instance, source, request);
        result.mStatus = ALGhostSpawnStatus::OK;
        result.mEffectiveRenderRoute =
            ALGhostRenderRoute::ENTITY_SCENE_PIPELINE;
        result.mInstanceIds.push_back(instance->mId);
        result.mMessage = "entity clone transaction committed";
        result.mWarnings.push_back(
            "entity render intent follows the viewer scene pipeline; it is "
            "not a per-object Forward/Deferred force");
        if (request.mFidelity ==
            ALGhostCloneRequest::Fidelity::MIRROR_EXACT)
        {
            result.mWarnings.push_back(
                "MirrorExact accepts live source coupling; the Entity keeps "
                "owned state if the source disappears, but continuing parity "
                "then ends");
        }
        return result;
    }

    bool update(ALGhostStudio& studio,
                const LLUUID& instance_id,
                const ALGhostCloneUpdate& update_request) override
    {
        return apply_update(studio, instance_id, update_request);
    }

    bool lock(ALGhostStudio& studio,
              const LLUUID& instance_id) override
    {
        return studio.setInstancePaused(instance_id, true);
    }

    bool despawn(ALGhostStudio& studio,
                 const LLUUID& instance_id) override
    {
        studio.removeInstance(instance_id);
        return studio.getInstance(instance_id) == nullptr;
    }
};

const ALGhostStudio::Instance* find_entity_runtime_owner(
    const ALGhostStudio& studio,
    const LLUUID& runtime_id)
{
    for (const ALGhostStudio::Instance& instance : studio.getInstances())
    {
        if (instance.mKind == ALGhostStudio::BACKING_ENTITY_CLONE &&
            instance.mEntityId == runtime_id)
        {
            return &instance;
        }
    }
    return nullptr;
}
}

ALGhostSpawnStatus ALGhostSourceResolver::resolve(
    ALGhostStudio& studio,
    const ALGhostCloneRequest& request,
    ALGhostResolvedCloneSource& resolved,
    std::string& diagnostic)
{
    resolved = ALGhostResolvedCloneSource();
    resolved.mRequestedHandle = request.mSource;

    LLUUID candidate = request.mSource;
    std::set<LLUUID> visited;
    ALGhostResolvedCloneSource::Provenance provenance =
        ALGhostResolvedCloneSource::Provenance::DIRECT_OR_CAST;

    for (S32 depth = 0; depth < 16; ++depth)
    {
        if (candidate.notNull() && !visited.insert(candidate).second)
        {
            diagnostic = "clone source chain contains a cycle";
            return ALGhostSpawnStatus::INVALID_REQUEST;
        }

        if (const ALGhostStudio::Instance* instance =
                studio.getInstance(candidate))
        {
            if (request.mMustRejectCloneOfClone)
            {
                diagnostic =
                    "source handle names a Ghost Studio instance";
                return ALGhostSpawnStatus::CLONE_OF_CLONE_REJECTED;
            }
            candidate = instance->mSource;
            provenance =
                ALGhostResolvedCloneSource::Provenance::
                    UNWRAPPED_STUDIO_INSTANCE;
            continue;
        }

        LLVOAvatar* avatar = nullptr;
        if (LLViewerObject* object = gObjectList.findObject(candidate))
        {
            avatar = object->asAvatar();
        }
        if (!avatar)
        {
            avatar = LLDirectorCast::instance().resolve(candidate);
        }
        if (!avatar || avatar->isDead())
        {
            diagnostic = "source avatar is not currently resolvable";
            return ALGhostSpawnStatus::SOURCE_UNAVAILABLE;
        }

        if (avatar->isGhostAvatar())
        {
            if (request.mMustRejectCloneOfClone)
            {
                diagnostic =
                    "resolved source is a client-only ghost avatar";
                return ALGhostSpawnStatus::CLONE_OF_CLONE_REJECTED;
            }
            const ALGhostStudio::Instance* owner =
                find_entity_runtime_owner(studio, avatar->getID());
            if (!owner)
            {
                diagnostic =
                    "orphan ghost runtime cannot be safely unwrapped";
                return ALGhostSpawnStatus::CLONE_OF_CLONE_REJECTED;
            }
            candidate = owner->mSource;
            provenance =
                ALGhostResolvedCloneSource::Provenance::
                    UNWRAPPED_ENTITY_RUNTIME;
            continue;
        }

        resolved.mCanonicalAvatarId = avatar->getID();
        resolved.mPersistentSourceHandle =
            candidate.isNull() ? LLUUID::null : avatar->getID();
        resolved.mAvatar = avatar;
        resolved.mLabel = request.mSourceLabel.empty()
            ? avatar->getFullname() : request.mSourceLabel;
        resolved.mProvenance = provenance;
        diagnostic.clear();
        return ALGhostSpawnStatus::OK;
    }

    diagnostic = "clone source chain exceeded the depth limit";
    return ALGhostSpawnStatus::INVALID_REQUEST;
}

ALGhostSpawnEngine::ALGhostSpawnEngine(ALGhostStudio& studio)
:   mStudio(studio)
{
}

ALGhostCloneBackend ALGhostSpawnEngine::selectBackend(
    const ALGhostCloneRequest& request) const
{
    if (request.mPersistence ==
            ALGhostCloneRequest::Persistence::PERSISTENT ||
        request.mControl == ALGhostCloneRequest::Control::DIRECTOR ||
        request.mFidelity ==
            ALGhostCloneRequest::Fidelity::INDEPENDENT_SNAPSHOT)
    {
        return ALGhostCloneBackend::ENTITY;
    }
    return ALGhostCloneBackend::OVERLAY;
}

std::unique_ptr<ALGhostSpawnBackend> ALGhostSpawnEngine::makeBackend(
    ALGhostCloneBackend backend) const
{
    if (backend == ALGhostCloneBackend::ENTITY)
    {
        return std::make_unique<ALGhostEntityBackend>();
    }
    return std::make_unique<ALGhostOverlayBackend>();
}

ALGhostSpawnResult ALGhostSpawnEngine::spawn(
    const ALGhostCloneRequest& request)
{
    ALGhostSpawnResult result;
    ALGhostCloneRequest effective_request = request;
    if (request.mCount != 1)
    {
        result.mStatus = ALGhostSpawnStatus::CAPABILITY_UNAVAILABLE;
        result.mMessage =
            "first migration stage accepts one clone; existing crowd "
            "transactions are intentionally not redefined";
        return result;
    }

    ALGhostResolvedCloneSource source;
    std::string source_diagnostic;
    result.mStatus = ALGhostSourceResolver::resolve(
        mStudio, request, source, source_diagnostic);
    if (result.mStatus != ALGhostSpawnStatus::OK)
    {
        result.mMessage = source_diagnostic;
        LL_WARNS("GhostStudio")
            << "GHOSTSPAWN rejected requested_source=" << request.mSource
            << " reason=" << source_diagnostic << LL_ENDL;
        return result;
    }

    ALGhostCloneBackend selected = selectBackend(request);
    std::vector<std::string> selection_warnings;

    // GhostDeferredEnable is a global overlay-only experimental kill switch.
    // It is not an entity render-mode selector.
    static LLCachedControl<bool> overlay_deferred_enabled(
        gSavedSettings, "GhostDeferredEnable", false);
    if (selected == ALGhostCloneBackend::OVERLAY &&
        request.mRender == ALGhostCloneRequest::Render::DEFERRED &&
        !overlay_deferred_enabled)
    {
        if (request.mRequireRenderMode)
        {
            result.mStatus =
                ALGhostSpawnStatus::CAPABILITY_UNAVAILABLE;
            result.mMessage =
                "required overlay Deferred route needs the global experimental "
                "GhostDeferredEnable gate";
            return result;
        }
        if (request.mAllowBackendFallback)
        {
            selected = ALGhostCloneBackend::ENTITY;
            selection_warnings.push_back(
                "selected Entity scene-pipeline route because overlay "
                "Deferred is globally disabled");
        }
        else
        {
            effective_request.mRender =
                ALGhostCloneRequest::Render::FORWARD;
            selection_warnings.push_back(
                "overlay Deferred preference is globally disabled; using "
                "the guaranteed Overlay Forward route");
        }
    }

    static LLCachedControl<bool> exclude_temporary(
        gSavedSettings, "GhostUnifiedExcludeTemporaryAttachments", false);
    ALGhostMaterialResolver::auditSource(
        source.mAvatar, backend_name(selected), exclude_temporary);

    std::unique_ptr<ALGhostSpawnBackend> backend =
        makeBackend(selected);
    std::string capability_reason;
    if (!backend->satisfies(effective_request, capability_reason))
    {
        result.mStatus = ALGhostSpawnStatus::CAPABILITY_UNAVAILABLE;
        result.mBackend = selected;
        result.mCapabilities = backend->capabilities();
        result.mMessage = capability_reason;
        return result;
    }
    result = backend->spawn(mStudio, source, effective_request);
    result.mWarnings.insert(result.mWarnings.begin(),
                            selection_warnings.begin(),
                            selection_warnings.end());
    LL_INFOS("GhostStudio")
        << "GHOSTSPAWN requested_source=" << request.mSource
        << " canonical_source=" << source.mCanonicalAvatarId
        << " backend=" << backend_name(selected)
        << " render_route="
        << static_cast<S32>(result.mEffectiveRenderRoute)
        << " render_route_guaranteed="
        << result.mEffectiveRenderRouteGuaranteed
        << " status=" << static_cast<S32>(result.mStatus)
        << " message=" << result.mMessage
        << LL_ENDL;
    return result;
}

bool ALGhostSpawnEngine::update(
    const LLUUID& instance_id,
    const ALGhostCloneUpdate& update_request)
{
    const ALGhostStudio::Instance* instance =
        mStudio.getInstance(instance_id);
    if (!instance)
    {
        return false;
    }
    if (instance->mGroupId.notNull())
    {
        return false;
    }
    return makeBackend(
        instance->mKind == ALGhostStudio::BACKING_ENTITY_CLONE
            ? ALGhostCloneBackend::ENTITY
            : ALGhostCloneBackend::OVERLAY)
        ->update(mStudio, instance_id, update_request);
}

bool ALGhostSpawnEngine::lock(const LLUUID& instance_id)
{
    const ALGhostStudio::Instance* instance =
        mStudio.getInstance(instance_id);
    if (!instance)
    {
        return false;
    }
    if (instance->mGroupId.notNull())
    {
        return false;
    }
    return makeBackend(
        instance->mKind == ALGhostStudio::BACKING_ENTITY_CLONE
            ? ALGhostCloneBackend::ENTITY
            : ALGhostCloneBackend::OVERLAY)
        ->lock(mStudio, instance_id);
}

bool ALGhostSpawnEngine::despawn(const LLUUID& instance_id)
{
    const ALGhostStudio::Instance* instance =
        mStudio.getInstance(instance_id);
    if (!instance)
    {
        return false;
    }
    if (instance->mGroupId.notNull())
    {
        LL_WARNS("GhostStudio")
            << "GHOSTSPAWN despawn rejected grouped instance="
            << instance_id
            << " because singular despawn must not delete a whole Studio group"
            << LL_ENDL;
        return false;
    }
    return makeBackend(
        instance->mKind == ALGhostStudio::BACKING_ENTITY_CLONE
            ? ALGhostCloneBackend::ENTITY
            : ALGhostCloneBackend::OVERLAY)
        ->despawn(mStudio, instance_id);
}
