/**
 * @file lldirectorcast.h
 * @brief Director Console engine: the Cast, Subjects A/B/C/D, marks, ACTION/CUT.
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
 *     body when set. Switcher slots can explicitly address A/B/C/D. Flycam
 *     Orbit rides resolveAnchor() and therefore Subject A automatically.
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

#include "algazemath.h"
#include "llactormover.h"
#include "llsd.h"
#include "lluuid.h"
#include "v3math.h"

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class LLVOAvatar;
class LLEventTimer;

class LLDirectorCast
{
public:
    enum EGazeCueStyle : S32
    {
        GAZE_CUE_STYLE_DEFAULT = 0,
        GAZE_CUE_STYLE_SNAP,
        GAZE_CUE_STYLE_DRIFT,
        GAZE_CUE_STYLE_CASUAL
    };

    struct GazeCue
    {
        F64 mStartSec = 0.0;
        F64 mDurationSec = 0.0; // zero = until the next cue / hold
        LLActorMover::GazeTarget mTarget;
        EGazeCueStyle mAcquireStyle = GAZE_CUE_STYLE_DEFAULT;
        EGazeCueStyle mReleaseStyle = GAZE_CUE_STYLE_DEFAULT;
        ALGazeMath::EGazeCueMacro mMacro = ALGazeMath::GAZE_MACRO_NONE;
        F32 mMacroGapSec = 0.35f;
        F32 mMacroDwellSec = 0.60f;
        S32 mHoldFramesPast = 2;
        bool mThoughtResidue = false;
    };
    using GazeCueList = std::vector<GazeCue>;

    // [Machinima] Keyframable gaze influence: a stateless presentation-time lane
    // parallel to GazeCueList. Its evaluated value multiplies the FINAL gaze
    // write envelope (never the owned target), so influence 0 performs NO write
    // and the animation shows through, while the aim/motor state stays warm. An
    // empty list evaluates to exactly 1 and the caller keeps its current formula
    // branch (byte-identical). Interpolation is the OUTGOING segment's mode.
    enum EGazeKeyInterpolation : S32
    {
        GAZE_KEY_STEP       = 0,
        GAZE_KEY_LINEAR     = 1,
        GAZE_KEY_SMOOTHSTEP = 2
    };

    struct GazeInfluenceKey
    {
        F64 mTimeSec = 0.0;
        F32 mValue = 1.f;                                   // clamped [0,1]
        EGazeKeyInterpolation mInterpolation = GAZE_KEY_SMOOTHSTEP;
    };
    using GazeInfluenceKeyList = std::vector<GazeInfluenceKey>;

    // Pure, scrub-stable evaluation. Empty list -> 1. Before the first key hold
    // its value; after the last hold its value. Between keys use the PREVIOUS
    // key's interpolation. Duplicate-time keys resolve to the last at that time.
    // Non-finite times/values are sanitized; result is clamped [0,1].
    static F32 evaluateGazeInfluence(const GazeInfluenceKeyList& keys,
                                     F64 presentation_time);

    struct GazeCueEvaluation
    {
        // A target mode cannot encode a crossfade by itself, so the effective
        // result carries its from/to targets plus the pure direction blend.
        // mEffectiveTarget is the cue target whose persona/config is active.
        bool mHasOverride = false;
        bool mHasFromTarget = false;
        bool mHasFromBaseTarget = false;
        S32 mCueIndex = -1;
        LLActorMover::GazeTarget mFromBaseTarget;
        LLActorMover::GazeTarget mFromTarget;
        LLActorMover::GazeTarget mToTarget;
        LLActorMover::GazeTarget mEffectiveTarget;
        F32 mFromTargetBlend = 1.f;
        F32 mTargetBlend = 0.f;
        F32 mEyeWeight = 1.f;
        F32 mHeadWeight = 1.f;
        F32 mBodyWeight = 1.f;
        F32 mLidWiden = 0.f;
        F32 mHeadRecoilPitch = 0.f;
        F32 mAcquireSec = 0.25f;
        F32 mReleaseSec = 0.60f;
        ALGazeMath::EGazeCuePhase mPhase = ALGazeMath::GAZE_CUE_BEFORE;
    };

    // Client-only presentation styling for a live Director actor.  The style
    // ids intentionally mirror Actor Mover / Ghost Studio's 0..27 shader
    // looks, without making the cast model depend on either renderer.
    enum EActorStyleMode : S32
    {
        ACTOR_STYLE_LAYER = 0,
        ACTOR_STYLE_REPLACE
    };

    struct ActorStyle
    {
        bool            mEnabled = false;
        EActorStyleMode mMode = ACTOR_STYLE_LAYER;
        S32             mStyle = 0;
        bool            mUseActorHue = true;
        F32             mHue = 200.f;
        F32             mAlpha = 0.6f;
        F32             mPixelSize = 0.f;
        F32             mShimmerSpeed = 1.f;
        F32             mShimmerAmount = 0.f;
        F32             mGlitch = 0.f;
        S32             mDistortion = 0;
        F32             mDistortionAmount = 0.5f;
        F32             mBrightness = 1.f;
        F32             mEffectFps = 0.f;
    };

    struct CastMember
    {
        CastMember()
        {
            // Director gaze means camera-facing unless a scene or operator
            // explicitly authors another target. Actor Mover's standalone
            // GazeTarget default remains MOTION.
            mGazeTarget.mMode = LLActorMover::GazeTarget::CAMERA;
        }

        LLUUID                  mId;             // avatar or control-avatar id
        std::string             mLastName;       // cached display name (survives region exit)
        LLVector3               mMark = LLVector3::zero;    // region coords; unset unless mHasMark
        bool                    mHasMark = false;
        LLUUID                  mLocoAnim;       // per-actor locomotion override (null = stock walk)
        std::string             mGroup;          // production group tag ("" = ungrouped; session-only)
        LLActorMover::GazeTarget mGazeTarget;    // per-cast authored gaze target config
        bool                    mEyeGazeTargetEnabled = false;
        LLActorMover::GazeTarget mEyeGazeTarget; // optional eyes-only target
        GazeCueList             mGazeCues;        // ordered presentation-time performance track
        GazeInfluenceKeyList    mGazeInfluenceKeys; // parallel influence lane (empty = influence 1)
        ActorStyle              mActorStyle;      // client-only live actor presentation
    };

    static LLDirectorCast& instance();
    // Residents use their avatar UUID; animated objects use their persistent
    // linkset-root UUID rather than the regenerated LLControlAvatar UUID.
    static LLUUID canonicalActorId(const LLUUID& id);

    // ---- membership (ordered, session-only) ----
    void add(const LLUUID& id);
    void remove(const LLUUID& id);
    void toggle(const LLUUID& id);
    bool contains(const LLUUID& id) const;
    // Direct lookup for renderer-owned IDs that are already canonical. This is
    // the hot-path variant and deliberately never touches gObjectList.
    bool containsStored(const LLUUID& id) const;
    // Resolve a canonical renderer owner and its wearer fallback together.
    // The primary member-index probe supplies the style directly when it hits,
    // avoiding the former containsStored()+getActorStyle() double lookup.
    const ActorStyle& resolveStoredActorStyle(const LLUUID& primary,
                                              const LLUUID& fallback,
                                              LLUUID& resolved_id) const;
    const std::vector<CastMember>& getCast() const { return mCast; }
    // parallel id list, kept in membership order; this is what
    // LLActorMover::getRoster() hands out (empty = "my avatar")
    const uuid_vec_t& getIds() const { return mIds; }
    CastMember*       getMember(const LLUUID& id);
    const CastMember* getMember(const LLUUID& id) const;

    // ---- per-actor presentation style ----
    // LLUUID::null (and the agent avatar's runtime id) address You. Invalid or
    // non-cast ids read as the disabled default and ignore writes.
    const ActorStyle& getActorStyle(const LLUUID& id) const;
    void              setActorStyle(const LLUUID& id, const ActorStyle& style);

    // ---- render-only real-avatar camera gaze selection ----
    // A subset of the cast, shared by the Camera-tab multi-select widget and
    // the post-animation render hook. Session/scene data only; it is never
    // copied to an avatar, animation message, or simulator update.
    void setLookAtCamera(const LLUUID& id, bool selected);
    bool isLookAtCamera(const LLUUID& id) const;

    // ---- per-cast / slot gaze targeting ----
    void                     setGazeTarget(const LLUUID& id, const LLActorMover::GazeTarget& target);
    LLActorMover::GazeTarget getGazeTarget(const LLUUID& id) const;
    void                     setEyeGazeTarget(const LLUUID& id, const LLActorMover::GazeTarget& target);
    void                     clearEyeGazeTarget(const LLUUID& id);
    bool                     hasEyeGazeTarget(const LLUUID& id) const;
    LLActorMover::GazeTarget getEyeGazeTarget(const LLUUID& id) const;
    void                     setGazeCues(const LLUUID& id, const GazeCueList& cues);
    const GazeCueList&       getGazeCues(const LLUUID& id) const;
    void                     setGazeInfluenceKeys(const LLUUID& id, const GazeInfluenceKeyList& keys);
    const GazeInfluenceKeyList& getGazeInfluenceKeys(const LLUUID& id) const;
    U64                      getGazeCueRevision() const { return mGazeCueRevision; }

    // Pure track evaluation. Empty lists (and times before the first cue)
    // return mHasOverride=false; callers then retain their live-target path.
    static GazeCueEvaluation evaluateGazeCues(
        const GazeCueList& cues, U64 persona_seed, F64 presentation_time);

    // ---- resolution ----
    // null id = my avatar; a stale id (actor left the region) resolves to
    // nullptr rather than silently falling back to self, so multi-actor
    // starts never double up on the agent avatar. Refreshes the member's
    // cached mLastName whenever a name is available.
    LLVOAvatar* resolve(const LLUUID& id);

    // ---- Subjects A / B / C / D (ids into the cast; session-only) ----
    // A doubles as the CineCam/orbit anchor; B is the second body for
    // legacy Two-Shot/OTS. C/D are additional switcher-addressable marks.
    // resolveSubject*() returns nullptr when unset or dead, which every
    // consumer treats as "fall back to stock behavior".
    void setSubjectA(const LLUUID& id);
    void setSubjectB(const LLUUID& id);
    void setSubjectC(const LLUUID& id);
    void setSubjectD(const LLUUID& id);
    const LLUUID& getSubjectA() const { return mSubjectA; }
    const LLUUID& getSubjectB() const { return mSubjectB; }
    const LLUUID& getSubjectC() const { return mSubjectC; }
    const LLUUID& getSubjectD() const { return mSubjectD; }
    LLVOAvatar* resolveSubjectA();
    LLVOAvatar* resolveSubjectB();
    LLVOAvatar* resolveSubjectC();
    LLVOAvatar* resolveSubjectD();

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

    // group MANAGEMENT (console Groups section). renameGroup retags every
    // member and carries the group's start delay across; renaming onto an
    // existing group MERGES the two (the surviving delay is the target's when
    // it has one, else the source's). dissolveGroup clears every member's tag
    // -- nobody leaves the cast -- and drops the delay. Both return false on
    // an empty/unknown source name (rename also refuses an empty new name).
    bool renameGroup(const std::string& old_name, const std::string& new_name);
    bool dissolveGroup(const std::string& name);

    // per-group START DELAY (staggered starts): seconds after ACTION / "Start
    // group" before the group's members begin their moves, so crowds can enter
    // in waves off one ACTION. Stored keyed by group name -- the tag on the
    // members IS the group; this map only annotates it, and a name that loses
    // its last member sheds its delay too (same no-stale-registry rule as the
    // tags). Serialized with the scene alongside the tags.
    void setGroupDelay(const std::string& name, F32 seconds);
    F32  getGroupDelay(const std::string& name) const;     // 0 when none set

    // ---- staggered starts (pending-start queue) ----
    // startMovesStaggered() is the delay-aware replacement for the plain
    // LLActorMover start loops: each id whose group carries a delay is QUEUED
    // and started by a self-deleting one-shot timer when due; everyone else
    // starts immediately (a zero-delay call is behavior-identical to the old
    // per-member start loop). An empty id list means "my avatar" (mirrors
    // startAll()). The cancels drop queued members WITHOUT touching running
    // moves; CUT and every deliberate stop path call them alongside the stops
    // (LLActorMover::stop/stopAll cancel pending starts themselves, so no
    // stop button can leave a surprise walk armed).
    void startMovesStaggered(const uuid_vec_t& ids);
    void cancelPendingStarts();
    void cancelPendingStart(const LLUUID& id);
    bool hasPendingStarts() const { return !mPendingStarts.empty(); }
    S32  pendingStartCount() const { return (S32)mPendingStarts.size(); }

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
    // True only while this ACTION owns the false->true transition of
    // CinematicCamEnabled. Camera clients use this to avoid snapshotting the
    // transport's temporary value as an operator-owned baseline.
    bool ownsCameraEnable() const
    {
        return mRunning && mFiredCamera && !mCameraWasEnabled;
    }
    bool isCountingDown() const;
    F32  countdownRemaining() const;    // seconds; 0 when not counting down

private:
    LLDirectorCast();

    void fireAction();
    void cancelCountdown();
    void rebuildMemberIndex();

    // staggered starts: fire every queued start that has come due, then prune
    // dead (already-fired, self-deleted) timer pointers from mStaggerTimers
    void servicePendingStarts();
    // drop a group's delay once its last member is gone/retagged (the
    // no-stale-registry rule); safe on names that never had a delay
    void pruneGroupDelay(const std::string& name);

    std::vector<CastMember> mCast;
    uuid_vec_t              mIds;       // mirrors mCast order
    std::unordered_map<LLUUID, std::size_t> mMemberIndex;
    std::set<LLUUID>        mLookAtCameraIds;
    bool                    mSelfLookAtCamera = false;

    LLUUID mSubjectA;
    LLUUID mSubjectB;
    LLUUID mSubjectC;
    LLUUID mSubjectD;
    LLActorMover::GazeTarget mSelfGazeTarget;
    bool mSelfEyeGazeTargetEnabled = false;
    LLActorMover::GazeTarget mSelfEyeGazeTarget;
    GazeCueList mSelfGazeCues;
    GazeInfluenceKeyList mSelfGazeInfluenceKeys;
    ActorStyle mSelfActorStyle;
    U64 mGazeCueRevision = 0;

    // transport state: what THIS action() run started, so cut() undoes
    // only that
    bool mRunning = false;
    bool mFiredMoves = false;
    bool mFiredCamera = false;
    bool mFiredPlay = false;
    bool mFiredCapture = false;
    bool mCameraWasEnabled = false;     // CinematicCamEnabled before action()
    bool mCameraEnableTransient = false; // switcher lease: don't alter save value

    // countdown one-shot; non-null exactly while pending (the fire lambda
    // nulls it before the timer self-deletes, cancelCountdown() deletes it)
    LLEventTimer* mCountdownTimer = nullptr;
    F64           mCountdownEndsAt = 0.0;   // LLTimer::getElapsedSeconds() deadline

    // ---- staggered starts ----
    std::map<std::string, F32> mGroupDelays;    // group name -> start delay, s

    struct PendingStart
    {
        LLUUID mId;         // cast member queued to start
        F64    mDueAt;      // LLTimer::getElapsedSeconds() deadline
    };
    std::vector<PendingStart> mPendingStarts;
    // self-deleting one-shot fire timers (LLEventTimer::run_after contract),
    // one per distinct delay per staggered call. Every pointer here is live
    // by construction: a firing timer's lambda removes its own pointer BEFORE
    // servicing the queue (the same discipline as mCountdownTimer), so
    // cancelPendingStarts() can delete the rest without a liveness probe.
    std::vector<LLEventTimer*> mStaggerTimers;
};

#endif // LL_LLDIRECTORCAST_H
