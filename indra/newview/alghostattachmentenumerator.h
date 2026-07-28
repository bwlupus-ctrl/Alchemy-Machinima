/**
 * @file alghostattachmentenumerator.h
 * @brief Shared source-domain enumeration for Ghost Studio clone backends.
 * @note Wraps the outer world-root walks in
 *       LLGhostAvatar::cloneAttachmentsFrom() and
 *       LLActorMover::walkGhostSourceGeometry().
 *
 * UNBUILT — pending Claude compile/link/in-world verification.
 */

#ifndef AL_ALGHOSTATTACHMENTENUMERATOR_H
#define AL_ALGHOSTATTACHMENTENUMERATOR_H

#include "llpointer.h"
#include "lluuid.h"
#include "stdtypes.h"

#include <functional>
#include <vector>

class LLVOAvatar;
class LLViewerObject;

enum class ALGhostTempAttachmentPolicy : U8
{
    INCLUDE, // Behavior-preserving default.
    EXCLUDE
};

struct ALGhostAttachmentRoot
{
    // Pin the eligible root and its ordered live child domain for the
    // synchronous entity-copy transaction. A later dead/missing member fails
    // the linkset instead of silently shrinking the captured denominator.
    LLPointer<LLViewerObject> mRoot;
    std::vector<LLPointer<LLViewerObject>> mChildren;
    S32 mAttachmentPointId = 0;
    bool mTemporary = false;
};

struct ALGhostAttachmentPlan
{
    bool mValidSource = false;
    LLUUID mSourceId;
    std::vector<ALGhostAttachmentRoot> mRoots;

    S32 mSkippedNullOrDead = 0;
    S32 mSkippedHud = 0;
    S32 mSkippedTemporary = 0;

    S32 expectedRoots() const
    {
        return static_cast<S32>(mRoots.size());
    }
};

class ALGhostAttachmentEnumerator
{
public:
    using root_visitor_t =
        std::function<void(LLViewerObject*, S32, bool)>;

    // Defines the transaction's eligible source domain. This method MUST NOT
    // dynamic_cast to LLVOVolume or perform any backend capability filtering.
    // Unsupported eligible roots are expected roots and must fail the entity
    // transaction rather than silently disappearing from its denominator.
    static ALGhostAttachmentPlan enumerateWorldRoots(
        LLVOAvatar* source,
        ALGhostTempAttachmentPolicy temporary_policy =
            ALGhostTempAttachmentPolicy::INCLUDE,
        bool emit_log = false);

    // Plan-free/non-owning hot-path view for Overlay collection. The callback
    // may not retain the raw root beyond the call. Existing per-root geometry
    // collection can still allocate its own temporary vectors.
    static void visitWorldRoots(
        LLVOAvatar* source,
        ALGhostTempAttachmentPolicy temporary_policy,
        const root_visitor_t& visitor);
};

#endif // AL_ALGHOSTATTACHMENTENUMERATOR_H
