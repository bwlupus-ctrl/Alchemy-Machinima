/** [ActorKey][AvatarSilhouette] Replay domain selection; no GL work here. */
#pragma once
#include "lluuid.h"
#include "v2math.h"
class LLRenderTarget;

struct CoverPolicy
{
    LLRenderTarget& target;
    F32 bias;
    const char* tag;
    bool key = false;
    bool nativeCover(const LLUUID& owner, const LLUUID& fallback, LLVector2& cover) const;
    bool proxyCover(const LLUUID& instance, const LLUUID& owner,
                    const LLUUID& fallback, LLVector2& cover) const;
    bool sharedCover(const LLUUID& key, const LLUUID& owner,
                     const LLUUID& fallback, LLVector2& cover) const;
};
