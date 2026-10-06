/**
 * @file alavatarkey.h
 * @brief [ClientHeight] Shared per-avatar store key convention (factored out of
 *        ALAvatarLightResponse::keyForAvatar so the Light Response trim and the
 *        client-side avatar height use the same identity rules).
 *
 * self -> gAgentID; clone -> Ghost Studio Instance::mId (stable across runtime
 * respawn); animesh -> linkset root (getActorFxOwnerId); resident -> avatar id.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_AVATAR_KEY_H
#define AL_AVATAR_KEY_H

#include "lluuid.h"

class LLVOAvatar;

namespace ALAvatarKey
{
// Values match ALAvatarLightResponse::EKind.
enum EKind
{
    KIND_RESIDENT = 0,
    KIND_SELF,
    KIND_ANIMESH,
    KIND_CLONE
};

// use_clone_stamp = false is the historical Light Response behaviour: a clone is
// resolved only through ALGhostStudio::findInstanceByRuntime(). true prefers the
// stable id Ghost Studio stamped on the clone (LLGhostAvatar::getClientHeightKey)
// and falls back to the instance lookup.
LLUUID forAvatar(const LLVOAvatar* av, EKind* out_kind, bool use_clone_stamp);
}

#endif // AL_AVATAR_KEY_H
