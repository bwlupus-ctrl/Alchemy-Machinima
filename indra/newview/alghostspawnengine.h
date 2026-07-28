/**
 * @file alghostspawnengine.h
 * @brief Capability-selected front end for Ghost Studio clone backends.
 * @note Wraps ALGhostStudio::addInstance()/spawnEntityClone() and their
 *       existing update, lock, and removal lifecycles.
 *
 * UNBUILT — pending Claude compile/link/in-world verification.
 */

#ifndef AL_ALGHOSTSPAWNENGINE_H
#define AL_ALGHOSTSPAWNENGINE_H

#include "llquaternion.h"
#include "lluuid.h"
#include "stdtypes.h"
#include "v3dmath.h"

#include <memory>
#include <string>
#include <vector>

class ALGhostStudio;
class LLVOAvatar;

enum class ALGhostCloneBackend : U8
{
    OVERLAY,
    ENTITY
};

enum class ALGhostSpawnStatus : U8
{
    OK,
    INVALID_REQUEST,
    SOURCE_UNAVAILABLE,
    CLONE_OF_CLONE_REJECTED,
    CAPABILITY_UNAVAILABLE,
    BACKEND_FAILED
};

enum class ALGhostRenderRoute : U8
{
    UNRESOLVED,
    OVERLAY_FORCE_FORWARD,
    OVERLAY_DEFERRED_HYBRID,
    ENTITY_SCENE_PIPELINE
};

struct ALGhostCloneRequest
{
    enum class Persistence : U8
    {
        EPHEMERAL,
        PERSISTENT
    };

    enum class Control : U8
    {
        NONE,
        TRANSFORM,
        DIRECTOR
    };

    enum class Fidelity : U8
    {
        MIRROR_EXACT,        // Accepts live source coupling.
        INDEPENDENT_SNAPSHOT // Requires immutable owned appearance data.
    };

    enum class Render : U8
    {
        FORWARD,
        DEFERRED
    };

    LLUUID mSource;
    std::string mSourceLabel;
    Persistence mPersistence = Persistence::EPHEMERAL;
    Control mControl = Control::TRANSFORM;
    Fidelity mFidelity = Fidelity::MIRROR_EXACT;
    // Preferred route unless mRequireRenderMode is true. Entity follows the
    // scene pipeline and cannot guarantee either technique per object.
    Render mRender = Render::FORWARD;

    bool mMustRejectCloneOfClone = true;
    bool mAllowBackendFallback = false;
    bool mRequireRenderMode = false;
    bool mRequirePbrExact = false;

    // Intentionally admitted into the request model now, but the first landing
    // handles one spawn only. Existing crowd/group callers retain their own
    // transaction semantics until the staged migration reaches them.
    S32 mCount = 1;
};

struct ALGhostCloneUpdate
{
    bool mHasFoot = false;
    LLVector3d mFootGlobal;
    bool mHasRotation = false;
    LLQuaternion mRotation;
    bool mHasScale = false;
    F32 mScale = 1.f;
    bool mHasEnabled = false;
    bool mEnabled = true;
};

struct ALGhostResolvedCloneSource
{
    enum class Provenance : U8
    {
        DIRECT_OR_CAST,
        UNWRAPPED_STUDIO_INSTANCE,
        UNWRAPPED_ENTITY_RUNTIME
    };

    LLUUID mRequestedHandle;
    LLUUID mCanonicalAvatarId;
    // Keeps the null/self sentinel intact; non-null aliases are canonicalized.
    LLUUID mPersistentSourceHandle;
    LLVOAvatar* mAvatar = nullptr; // Weak; valid for the current main-thread turn.
    std::string mLabel;
    Provenance mProvenance = Provenance::DIRECT_OR_CAST;
};

struct ALGhostBackendCapabilities
{
    bool mOwnsRenderable = false;
    bool mRenderSourceDependent = true;
    bool mDefaultDriveMirrorsSource = false;
    bool mIndependentTransform = true;
    bool mIndependentPose = false;
    bool mImmutableAppearanceSnapshot = false;
    bool mDirectorActor = false;
    bool mNativeSceneMaterials = false;
    bool mPbrExactVerified = false;
};

struct ALGhostSpawnResult
{
    ALGhostSpawnStatus mStatus = ALGhostSpawnStatus::INVALID_REQUEST;
    ALGhostCloneBackend mBackend = ALGhostCloneBackend::OVERLAY;
    ALGhostBackendCapabilities mCapabilities;
    ALGhostRenderRoute mEffectiveRenderRoute =
        ALGhostRenderRoute::UNRESOLVED;
    bool mEffectiveRenderRouteGuaranteed = false;
    std::vector<LLUUID> mInstanceIds;
    std::string mMessage;
    std::vector<std::string> mWarnings;

    bool succeeded() const
    {
        return mStatus == ALGhostSpawnStatus::OK &&
               !mInstanceIds.empty();
    }
};

class ALGhostSourceResolver
{
public:
    static ALGhostSpawnStatus resolve(
        ALGhostStudio& studio,
        const ALGhostCloneRequest& request,
        ALGhostResolvedCloneSource& resolved,
        std::string& diagnostic);
};

class ALGhostSpawnBackend
{
public:
    virtual ~ALGhostSpawnBackend() = default;

    virtual ALGhostCloneBackend kind() const = 0;
    virtual ALGhostBackendCapabilities capabilities() const = 0;
    virtual bool satisfies(const ALGhostCloneRequest& request,
                           std::string& reason) const = 0;
    virtual ALGhostSpawnResult spawn(
        ALGhostStudio& studio,
        const ALGhostResolvedCloneSource& source,
        const ALGhostCloneRequest& request) = 0;
    virtual bool update(ALGhostStudio& studio,
                        const LLUUID& instance_id,
                        const ALGhostCloneUpdate& update) = 0;
    virtual bool lock(ALGhostStudio& studio,
                      const LLUUID& instance_id) = 0;
    virtual bool despawn(ALGhostStudio& studio,
                         const LLUUID& instance_id) = 0;
};

class ALGhostSpawnEngine
{
public:
    explicit ALGhostSpawnEngine(ALGhostStudio& studio);

    ALGhostSpawnResult spawn(const ALGhostCloneRequest& request);
    bool update(const LLUUID& instance_id,
                const ALGhostCloneUpdate& update);
    // Backend-native pose hold: Overlay freezes its captured render pose;
    // Entity pauses drive/animation. This is not a persistence, transform, or
    // immutable-appearance lock.
    bool lock(const LLUUID& instance_id);
    bool despawn(const LLUUID& instance_id);

private:
    ALGhostCloneBackend selectBackend(
        const ALGhostCloneRequest& request) const;
    std::unique_ptr<ALGhostSpawnBackend> makeBackend(
        ALGhostCloneBackend backend) const;

    ALGhostStudio& mStudio;
};

#endif // AL_ALGHOSTSPAWNENGINE_H
