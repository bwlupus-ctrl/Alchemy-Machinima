/**
 * @file lldirectorcast.h
 * @brief Director Console engine: the Cast, Subjects A/B, marks, ACTION/CUT.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Session-scoped singleton, the single source of truth for "who is in the
 * scene". The machinima tools each keep their public API but share this
 * state underneath:
 *
 *   - LLActorMover's roster IS the cast (toggleTarget/getRoster delegate
 *     here), so right-click "Actor Mover Target" and "Add to Cast" toggle
 *     the same membership.
 *   - LLCinematicCamera resolves Subject A ahead of its follow-target ->
 *     selected -> self chain, and Two-Shot/OTS use Subject B as the second
 *     body when set. Flycam Orbit rides resolveAnchor() and therefore
 *     Subject A automatically.
 *
 * ACTION/CUT is the shared transport: action() starts everything armed via
 * the persisted DirectorArm* settings (actor moves, cinematic camera,
 * recorder playback/capture), optionally after a DirectorActionDelay
 * countdown; cut() stops exactly what action() started and restores the
 * camera-enable state it changed.
 *
 * Engine only -- no UI includes here. The Director Console floater (D2) is
 * a pure view over this. Cast membership, subjects and marks are
 * session-only; nothing persists except the Director arming/delay settings.
 */

#ifndef LL_LLDIRECTORCAST_H
#define LL_LLDIRECTORCAST_H

#include "llsd.h"
#include "lluuid.h"
#include "v3math.h"

#include <string>
#include <vector>

class LLVOAvatar;
class LLEventTimer;

class LLDirectorCast
{
public:
    struct CastMember
    {
        LLUUID      mId;             // avatar or control-avatar id
        std::string mLastName;       // cached display name (survives region exit)
        LLVector3   mMark = LLVector3::zero;    // region coords; unset unless mHasMark
        bool        mHasMark = false;
        LLUUID      mLocoAnim;       // per-actor locomotion override (null = stock walk)
        std::string mGroup;          // production group tag ("" = ungrouped; session-only)
    };

    static LLDirectorCast& instance();

    // ---- membership (ordered, session-only) ----
    void add(const LLUUID& id);
    void remove(const LLUUID& id);
    void toggle(const LLUUID& id);
    bool contains(const LLUUID& id) const;
    const std::vector<CastMember>& getCast() const { return mCast; }
    // parallel id list, kept in membership order; this is what
    // LLActorMover::getRoster() hands out (empty = "my avatar")
    const uuid_vec_t& getIds() const { return mIds; }
    CastMember*       getMember(const LLUUID& id);
    const CastMember* getMember(const LLUUID& id) const;

    // ---- resolution ----
    // null id = my avatar; a stale id (actor left the region) resolves to
    // nullptr rather than silently falling back to self, so multi-actor
    // starts never double up on the agent avatar. Refreshes the member's
    // cached mLastName whenever a name is available.
    LLVOAvatar* resolve(const LLUUID& id);

    // ---- Subjects A / B (ids into the cast; session-only) ----
    // A doubles as the CineCam/orbit anchor; B is the second body for
    // Two-Shot/OTS. resolveSubject*() returns nullptr when unset or dead,
    // which every consumer treats as "fall back to stock behavior".
    void setSubjectA(const LLUUID& id) { mSubjectA = id; }
    void setSubjectB(const LLUUID& id) { mSubjectB = id; }
    const LLUUID& getSubjectA() const { return mSubjectA; }
    const LLUUID& getSubjectB() const { return mSubjectB; }
    LLVOAvatar* resolveSubjectA();
    LLVOAvatar* resolveSubjectB();

    // ---- groups (session-only production tags) ----
    // Free-form label per member ("guards", "crowd B", ...) so console ops can
    // address a subset of the cast at once (group-scoped Start/Stop today;
    // group-scoped triggering later). A group exists only as the union of the
    // members carrying its tag -- there is no separate registry to hold stale
    // names, so removing/re-tagging the last member retires the group.
    void        setGroup(const LLUUID& id, const std::string& name);   // "" = ungroup
    std::string getGroup(const LLUUID& id) const;                      // "" when none/unknown
    // distinct non-empty group names in first-appearance cast order (stable,
    // so a combo rebuilt from this does not shuffle under the operator)
    std::vector<std::string> getGroupNames() const;
    // every member tagged with this group, in cast order (empty name -> empty)
    uuid_vec_t membersInGroup(const std::string& name) const;

    // ---- marks ----
    // setMarks() snapshots every resolvable member's current rendered root;
    // resetToMarks() snaps marked members back (ActorMover-style local root
    // placement hold; an active move is stopped first).
    void setMarks();
    void resetToMarks();

    // Per-member variants of the above (UI-free): right-click "Set Mark Here"
    // and "Reset to Mark" drive these on a single clicked actor. setMark()
    // snapshots that member's current rendered root; resetToMark() snaps it
    // back. Both no-op and return false when the id is not a resolvable cast
    // member (setMark) or has no mark yet (resetToMark).
    bool setMark(const LLUUID& id);
    bool resetToMark(const LLUUID& id);

    // ---- scene serialization (UI-free; the Director Console owns the
    //      files and everything settings-backed) ----
    // sceneData() captures cast membership, cached names, marks, loco anims,
    // groups and Subjects A/B as one LLSD map. applySceneData() replaces the cast
    // wholesale from such a map; members not currently in world stay in the
    // cast (they render "(away)" and revive on return). Nothing is moved:
    // marks come back as data only.
    LLSD sceneData() const;
    void applySceneData(const LLSD& data);

    // ---- ACTION / CUT transport ----
    // action() fires everything armed by the DirectorArm* settings, after
    // the DirectorActionDelay countdown when nonzero. cut() cancels a
    // pending countdown and stops exactly what action() started (the
    // cinematic camera enable is restored, not hammered off).
    void action();
    void cut();
    bool isRunning() const { return mRunning; }
    bool isCountingDown() const;
    F32  countdownRemaining() const;    // seconds; 0 when not counting down

private:
    LLDirectorCast() = default;

    void fireAction();
    void cancelCountdown();

    std::vector<CastMember> mCast;
    uuid_vec_t              mIds;       // mirrors mCast order

    LLUUID mSubjectA;
    LLUUID mSubjectB;

    // transport state: what THIS action() run started, so cut() undoes
    // only that
    bool mRunning = false;
    bool mFiredMoves = false;
    bool mFiredCamera = false;
    bool mFiredPlay = false;
    bool mFiredCapture = false;
    bool mCameraWasEnabled = false;     // CinematicCamEnabled before action()

    // countdown one-shot; non-null exactly while pending (the fire lambda
    // nulls it before the timer self-deletes, cancelCountdown() deletes it)
    LLEventTimer* mCountdownTimer = nullptr;
    F64           mCountdownEndsAt = 0.0;   // LLTimer::getElapsedSeconds() deadline
};

#endif // LL_LLDIRECTORCAST_H
