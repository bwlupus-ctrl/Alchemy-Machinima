/**
 * @file alavatarkey.cpp
 * @brief [ClientHeight] Shared per-avatar store key convention.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alavatarkey.h"

#include "alghoststudio.h"
#include "llagentdata.h"
#include "llghostavatar.h"
#include "llvoavatar.h"

namespace ALAvatarKey
{
LLUUID forAvatar(const LLVOAvatar* av, EKind* out_kind, bool use_clone_stamp)
{
    EKind kind = KIND_RESIDENT;
    LLUUID key;
    if (av)
    {
        if (av->isSelf())
        {
            kind = KIND_SELF;
            key = gAgentID;
        }
        else if (av->isGhostAvatar())
        {
            kind = KIND_CLONE;
            if (use_clone_stamp)
            {
                key = static_cast<const LLGhostAvatar*>(av)->getClientHeightKey();
            }
            if (key.isNull())
            {
                const ALGhostStudio::Instance* inst =
                    ALGhostStudio::instance().findInstanceByRuntime(av->getID());
                if (inst)
                {
                    key = inst->mId;
                }
            }
        }
        else if (av->isControlAvatar())
        {
            kind = KIND_ANIMESH;
            key = av->getActorFxOwnerId();
        }
        else
        {
            key = av->getID();
        }
    }
    if (out_kind)
    {
        *out_kind = kind;
    }
    return key;
}
}
