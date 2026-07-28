/**
 * @file alghostattachmentenumerator.cpp
 * @brief Shared source-domain enumeration for Ghost Studio clone backends.
 * @note Implements the shared domain used by cloneAttachmentsFrom() and the
 *       Overlay geometry harvest without moving linkset construction.
 *
 * UNBUILT — pending Claude compile/link/in-world verification.
 */

#include "llviewerprecompiledheaders.h"

#include "alghostattachmentenumerator.h"

#include "llviewerjointattachment.h"
#include "llviewerobject.h"
#include "llvoavatar.h"

namespace
{
void walk_world_roots(
    LLVOAvatar* source,
    ALGhostTempAttachmentPolicy temporary_policy,
    const ALGhostAttachmentEnumerator::root_visitor_t& visitor,
    ALGhostAttachmentPlan* plan)
{
    for (const auto& point_entry : source->mAttachmentPoints)
    {
        const S32 point_id = point_entry.first;
        LLViewerJointAttachment* point = point_entry.second;
        if (!point)
        {
            if (plan) ++plan->mSkippedNullOrDead;
            continue;
        }
        if (point->getIsHUDAttachment())
        {
            if (plan)
            {
                plan->mSkippedHud +=
                    static_cast<S32>(point->mAttachedObjects.size());
            }
            continue;
        }

        for (LLViewerObject* root : point->mAttachedObjects)
        {
            if (!root || root->isDead())
            {
                if (plan) ++plan->mSkippedNullOrDead;
                continue;
            }
            if (root->isHUDAttachment())
            {
                if (plan) ++plan->mSkippedHud;
                continue;
            }

            const bool temporary = root->isTempAttachment();
            if (temporary &&
                temporary_policy == ALGhostTempAttachmentPolicy::EXCLUDE)
            {
                if (plan) ++plan->mSkippedTemporary;
                continue;
            }
            visitor(root, point_id, temporary);
        }
    }
}
}

ALGhostAttachmentPlan ALGhostAttachmentEnumerator::enumerateWorldRoots(
    LLVOAvatar* source,
    ALGhostTempAttachmentPolicy temporary_policy,
    bool emit_log)
{
    ALGhostAttachmentPlan plan;
    if (!source || source->isDead())
    {
        LL_WARNS("GhostStudio")
            << "GHOSTENUM invalid source" << LL_ENDL;
        return plan;
    }

    plan.mValidSource = true;
    plan.mSourceId = source->getID();

    // Preserve the source avatar's existing point/root iteration order.
    walk_world_roots(
        source, temporary_policy,
        [&plan](LLViewerObject* root, S32 point_id, bool temporary)
        {
            ALGhostAttachmentRoot entry;
            entry.mRoot = root;
            for (LLViewerObject* child : root->getChildren())
            {
                if (child && !child->isDead())
                {
                    entry.mChildren.emplace_back(child);
                }
            }
            entry.mAttachmentPointId = point_id;
            entry.mTemporary = temporary;
            plan.mRoots.push_back(entry);
        },
        &plan);

    if (emit_log)
    {
        LL_INFOS("GhostStudio")
            << "GHOSTENUM source=" << plan.mSourceId
            << " expected_world_roots=" << plan.expectedRoots()
            << " skipped_null_or_dead=" << plan.mSkippedNullOrDead
            << " skipped_hud=" << plan.mSkippedHud
            << " skipped_temporary=" << plan.mSkippedTemporary
            << " temporary_policy="
            << (temporary_policy == ALGhostTempAttachmentPolicy::INCLUDE
                    ? "include" : "exclude")
            << LL_ENDL;
    }
    return plan;
}

void ALGhostAttachmentEnumerator::visitWorldRoots(
    LLVOAvatar* source,
    ALGhostTempAttachmentPolicy temporary_policy,
    const root_visitor_t& visitor)
{
    if (!source || source->isDead() || !visitor)
    {
        return;
    }
    walk_world_roots(
        source, temporary_policy, visitor, /*plan=*/nullptr);
}
