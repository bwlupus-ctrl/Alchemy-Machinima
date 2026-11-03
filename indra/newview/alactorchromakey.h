/** [ActorKey] WP0 model. WP2 owns the eventual presentation decision. */
#pragma once
#include "lluuid.h"
#include "alactorkeyset.h"
#include "v3color.h"
#include <string>
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
    enum class StudioPolicy { Look, Skip, Fill };
    static StudioPolicy studioPolicy(const LLUUID& instance);
    static StudioPolicy requestedStudioPolicy(const LLUUID& instance);
    static U32 mode();
    static LLColor3 colour();
    static bool alphaMatte();
    static bool matteInvert();
    static void preflightControls();
    static bool hideHUD();
    static bool sceneGlass();
    static bool keyedThisFrame();
    static bool keyGuardAllows(); // WP3 supplies the live ReShade guard; snapshots bypass it.
    static U64 finalizeSerial();
    static void beginFinalize();
    static void refuse(const char* reason);
    static const std::string& reason();
    static void provision(S32 width, S32 height);
    static void studioFailure(const char* reason);
    static void finishStudioFrame(U64 serial);
    static bool snapshotComplete(U64 before, S32 width, S32 height);
    static bool snapshotRequestAllowed(S32 width, S32 height);
    static void snapshotRefused(S32 width, S32 height);
    static const std::string& snapshotPlaceholder();
    static void explicitSnapshotRequest();
    static bool snapshotRequestExplicit();
    struct SnapshotRequestScope
    {
        explicit SnapshotRequestScope(bool explicit_request);
        ~SnapshotRequestScope();
        bool mPrevious;
    };
    struct SnapshotScope
    {
        SnapshotScope();
        ~SnapshotScope();
        bool mActive, mPrevious;
    };
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
