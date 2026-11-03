/** [ActorKey] WP0 model. WP2 owns the eventual presentation decision. */
#pragma once
#include "lluuid.h"
#include "alactorkeyset.h"
#include <vector>
class LLVOAvatar;

class ALActorChromaKey
{
public:
    static bool sActive;
    static void initialize();
    static void tick();
    static std::vector<LLVOAvatar*> gatherKeyRecipients();
    static bool keepsLive(const LLVOAvatar* av);
    static bool ownerKeyed(const LLUUID& owner, const LLUUID& fallback);
    static bool instanceKeyed(const LLUUID& instance);
    static U64 revision() { return sRevision; }
    // WP2 interface: the 360 capture caller will own this scope.
    struct SuppressScope
    {
        SuppressScope() { ++sSuppress; }
        ~SuppressScope() { --sSuppress; }
        SuppressScope(const SuppressScope&) = delete;
        SuppressScope& operator=(const SuppressScope&) = delete;
    };
    static bool suppressed() { return sSuppress != 0; }
private:
    static ALActorKeySet<LLUUID> sKeyed;
    static U64 sRevision;
    static U32 sSuppress;
};
