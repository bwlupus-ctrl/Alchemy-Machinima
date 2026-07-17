/**
 * @file llactormover.h
 * @brief Local ("ghost") locomotion rig: moves an avatar's RENDERED body along
 *        a client-side path at arbitrary speed with cadence-locked walk anims.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The sim is never told: the avatar's true position stays wherever the server
 * thinks it is, and only this client renders the motion (same local-only
 * philosophy as the any-avatar poser). That is the point -- LSL movement tools
 * are quantized by sim ticks and keyframe granularity; this evaluates per
 * rendered frame at any speed, and locks the walk-cycle clock to the actual
 * ground speed so foot plants never slide.
 */

#ifndef LL_LLACTORMOVER_H
#define LL_LLACTORMOVER_H

#include "lluuid.h"
#include "v3math.h"

#include <map>

class LLVOAvatar;

class LLActorMover
{
public:
    static LLActorMover& instance();

    // right-click targeting (same session-lock idiom as CineCam Follow):
    // toggling the same avatar clears it; null target means "my avatar"
    static void toggleTarget(const LLUUID& id);
    static bool isTarget(const LLUUID& id);

    // transport, driven by the Actor Mover floater. start() begins a move for
    // the current target (self when none) from its CURRENT rendered pose.
    void start();
    void stop();
    bool anyMoving() const { return !mMoves.empty(); }
    bool isMoving(const LLUUID& id) const { return mMoves.count(id) != 0; }

    // called from the avatar root-placement site each frame; returns true and
    // writes mRoot world pos/rot when this avatar is being ghost-moved
    bool applyOverride(LLVOAvatar* av);

private:
    LLActorMover() = default;

    struct Move
    {
        LLVector3 mOrigin;      // rendered start position (agent region coords)
        F32       mHeading = 0.f;   // world yaw of travel, radians
        F32       mSpeed = 1.f;     // m/s
        F32       mDistance = 5.f;  // path length, m
        S32       mEndMode = 0;     // 0 hold at end, 1 loop, 2 ping-pong
        F32       mT = 0.f;         // elapsed seconds
    };

    std::map<LLUUID, Move> mMoves;
};

#endif // LL_LLACTORMOVER_H
