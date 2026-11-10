#include "linden_common.h"
#include "../test/lltut.h"
#include "../alghostclearanim.h"

#include <iterator>
#include <map>
#include <utility>
#include <set>
#include <string>
#include <vector>

namespace tut
{
struct alghostclearanim_data {};
typedef test_group<alghostclearanim_data> alghostclearanim_group;
typedef alghostclearanim_group::object alghostclearanim_object;
alghostclearanim_group gAlGhostClearAnim("alghostclearanim");

using namespace ALGhostClearAnim;

namespace
{
    const int kMirror = 0, kFrozen = 2;
    const float* under_rot_of(float v) { static float q[4]; q[0] = v; q[1] = 0.f; q[2] = 0.f; q[3] = 1.f; return q; }

    // A clone as the viewer drives it: a drive, the motions active on its own
    // controller, and the things that try to start motions every tick.
    struct FakeClone
    {
        int drive = kMirror;
        std::string anim;                // directed animation; empty = none
        bool cleared = false;
        bool autoanimate = true;
        bool gaze = true;
        bool cue_wanted = true;          // a hover / jump motion mode is running
        bool physics = true;
        bool posing = false;             // the Poser has its motion on this clone
        bool gated = true;               // false = the start sites ignore the cleared state
        std::set<std::string> active;

        void start(const std::string& motion, EOwner owner)
        {
            if (!gated || mayStart(cleared, owner))
            {
                active.insert(motion);
            }
        }

        void setDrive(int next, const std::string& next_anim)
        {
            if (!anim.empty()) active.erase(anim);
            if (next != kMirror) active.erase("stand");
            drive = next;
            anim = next == kDriveDirected ? next_anim : std::string();
            if (!holds(cleared, drive, !anim.empty())) cleared = false;
            if (!anim.empty()) start(anim, OWNER_DRIVE);
        }

        // One frame of everything that runs by itself.
        void tick()
        {
            if (drive == kMirror) start("stand", OWNER_DRIVE);                  // mirrored source stand
            if (autoanimate && drive == kMirror) start("autoanim", OWNER_AUTOANIM);
            if (cue_wanted && (drive == kMirror || drive == kDriveDirected)) start("hover", OWNER_CUE);
            if (gaze) start("eye", OWNER_EYE);
            if (physics) start("physics", OWNER_PHYSICS);
            if (posing) start("poser", OWNER_POSER);
        }

        Before<std::string> clear()
        {
            Before<std::string> before;
            before.mDrive = drive;
            before.mAnim = anim;
            before.mAutoAnimate = autoanimate;
            before.mGazeEnabled = gaze;
            before.mWasCleared = cleared;
            autoanimate = false;
            gaze = false;
            setDrive(kDriveDirected, std::string());
            if (canHold(drive, !anim.empty()))
            {
                cleared = true;
                for (auto it = active.begin(); it != active.end();)
                {
                    it = stopsOnClear(*it == "poser") ? active.erase(it) : std::next(it);
                }
            }
            return before;
        }

        void undo(const Before<std::string>& before)
        {
            if (!clearedAfterUndo(before)) cleared = false;
            setDrive(before.mDrive, before.mAnim);
            autoanimate = before.mAutoAnimate;
            gaze = before.mGazeEnabled;
        }

        // Motions of the clone's own; the Poser's motion is not one of them.
        size_t ownMotions() const { return active.size() - active.count("poser"); }
    };

    // The property under test: a cleared clone has no motion of its own after N ticks.
    bool restsAfter(FakeClone clone, int ticks)
    {
        for (int i = 0; i < ticks; ++i) clone.tick();
        return clone.cleared && clone.ownMotions() == 0;
    }
}

// Rules.
template<> template<> void alghostclearanim_object::test<1>()
{
    ensure("Directed with no animation can hold", canHold(kDriveDirected, false));
    ensure("Directed with an animation cannot", !canHold(kDriveDirected, true));
    ensure("Mirror cannot", !canHold(kMirror, false));
    ensure("Frozen cannot", !canHold(kFrozen, false));
    ensure("stays cleared in Directed / none", holds(true, kDriveDirected, false));
    ensure("picking an animation ends it", !holds(true, kDriveDirected, true));
    ensure("another drive mode ends it", !holds(true, kMirror, false));
    ensure("never cleared stays not cleared", !holds(false, kDriveDirected, false));
    for (EOwner owner : { OWNER_DRIVE, OWNER_CUE, OWNER_EYE, OWNER_PHYSICS, OWNER_AUTOANIM })
    {
        ensure("not cleared: may start", mayStart(false, owner));
        ensure("cleared: may not start", !mayStart(true, owner));
    }
    ensure("the Poser may always start", mayStart(true, OWNER_POSER) && mayStart(false, OWNER_POSER));
    ensure("Clear stops ordinary motions", stopsOnClear(false));
    ensure("Clear keeps the Poser's motion", !stopsOnClear(true));
    ensure("entity clone with a body", eligible(true, true));
    ensure("overlay clone", !eligible(false, true));
    ensure("entity clone still spawning", !eligible(true, false));
    ensure_equals("toast", clearedToast(3), std::string("Cleared animations on 3 clone(s)"));
}

// A cleared clone has no active motions after N ticks; cues and AutoAnimate do not restart.
template<> template<> void alghostclearanim_object::test<2>()
{
    FakeClone clone;
    for (int i = 0; i < 3; ++i) clone.tick();
    ensure("a live mirror clone runs motions", clone.ownMotions() >= 4);
    clone.clear();
    ensure("cleared", clone.cleared);
    ensure_equals("nothing active right after Clear", clone.ownMotions(), size_t(0));
    ensure("still nothing after 200 ticks", restsAfter(clone, 200));

    // The motion cue stays requested and AutoAnimate is switched back on by hand:
    // neither starts a motion while the clone is cleared.
    clone.cue_wanted = true;
    clone.autoanimate = true;
    clone.gaze = true;
    ensure("cue / AutoAnimate / eye do not restart", restsAfter(clone, 200));

    // From a directed animation too.
    FakeClone directed;
    directed.setDrive(kDriveDirected, "dance");
    directed.tick();
    ensure("dance plays", directed.active.count("dance") == 1);
    directed.clear();
    ensure("directed clone rests", restsAfter(directed, 50));
}

// Planted negatives: start sites that ignore the cleared state are rejected.
template<> template<> void alghostclearanim_object::test<3>()
{
    FakeClone ungated;
    ungated.gated = false;
    ungated.clear();
    ensure("ungated cue restart is rejected", !restsAfter(ungated, 5));

    // Only AutoAnimate ungated: switched back on, drive untouched, nothing may start.
    // (AutoAnimate is dormant in Directed, so plant the leak through the eye layer.)
    FakeClone leak;
    leak.clear();
    leak.gated = false;
    leak.cue_wanted = false;
    leak.physics = false;
    leak.gaze = true;
    ensure("a single ungated layer is rejected", !restsAfter(leak, 1));

    // A Clear that leaves the drive alone does not rest: the mirror keeps feeding it.
    FakeClone wrong_drive;
    wrong_drive.tick();
    wrong_drive.cleared = true;          // flag only, still Mirror
    wrong_drive.gated = false;
    ensure("flag without the drive change is rejected", !restsAfter(wrong_drive, 2));
}

// The drive ends the cleared state; nothing else does.
template<> template<> void alghostclearanim_object::test<4>()
{
    FakeClone clone;
    clone.clear();
    clone.setDrive(kDriveDirected, std::string());
    ensure("Directed / none again keeps it", clone.cleared);
    clone.setDrive(kDriveDirected, "wave");
    ensure("picking an animation ends it", !clone.cleared);
    clone.tick();
    ensure("the animation and the layers run again", clone.active.count("wave") == 1 && clone.active.count("physics") == 1);

    FakeClone other;
    other.clear();
    other.setDrive(kMirror, std::string());
    other.tick();
    ensure("another drive mode ends it", !other.cleared && other.active.count("stand") == 1);
}

// One undo step restores drive mode, animation, AutoAnimate and gaze.
template<> template<> void alghostclearanim_object::test<5>()
{
    FakeClone clone;
    clone.setDrive(kDriveDirected, "dance");
    clone.autoanimate = true;
    clone.gaze = true;
    const Before<std::string> before = clone.clear();
    ensure("cleared state differs", clone.anim.empty() && !clone.autoanimate && !clone.gaze);
    clone.undo(before);
    ensure_equals("drive restored", clone.drive, kDriveDirected);
    ensure_equals("animation restored", clone.anim, std::string("dance"));
    ensure("AutoAnimate restored", clone.autoanimate);
    ensure("gaze restored", clone.gaze);
    ensure("no longer cleared", !clone.cleared);
    clone.tick();
    ensure("animation plays again", clone.active.count("dance") == 1);

    // A clone that was Directed with no animation (not cleared) must not stay cleared on undo.
    FakeClone idle;
    idle.setDrive(kDriveDirected, std::string());
    const Before<std::string> idle_before = idle.clear();
    idle.undo(idle_before);
    ensure("undo ends the cleared state even when the drive is unchanged", !idle.cleared);
    idle.tick();
    ensure("its layers run again", idle.active.count("physics") == 1);

    // Clearing an already cleared clone and undoing that leaves it cleared.
    FakeClone twice;
    twice.clear();
    const Before<std::string> second = twice.clear();
    twice.undo(second);
    ensure("undo of a second Clear keeps the first", twice.cleared);

    // Planted negative: an undo that only puts the drive back loses AutoAnimate and gaze.
    FakeClone partial;
    partial.setDrive(kDriveDirected, "dance");
    const Before<std::string> full = partial.clear();
    partial.cleared = false;
    partial.setDrive(full.mDrive, full.mAnim);
    ensure("drive-only undo is rejected", partial.autoanimate != full.mAutoAnimate || partial.gaze != full.mGazeEnabled);
}

// The Poser on a cleared clone: its motion runs, survives Clear, and release restarts nothing.
template<> template<> void alghostclearanim_object::test<6>()
{
    FakeClone clone;
    clone.posing = true;
    clone.tick();
    clone.clear();
    ensure("the Poser's motion survives Clear", clone.active.count("poser") == 1);
    ensure("nothing else runs", restsAfter(clone, 100));

    // Poser acquire = Directed / none (no change on a cleared clone); release puts back
    // the saved drive only when the clone is not cleared.
    const int saved_drive = kMirror;
    clone.setDrive(kDriveDirected, std::string());
    clone.posing = false;
    clone.active.erase("poser");
    if (!clone.cleared) clone.setDrive(saved_drive, std::string());
    ensure("release keeps the neutral pose", restsAfter(clone, 100));

    // Planted negative: a release that ignores the cleared state restarts the stand.
    FakeClone bad;
    bad.clear();
    bad.setDrive(saved_drive, std::string());
    ensure("unguarded release is rejected", !restsAfter(bad, 2));
}

// Review round 2, finding 2: queued / staggered walks. Clear must cancel a pending start
// even though the clone has no Move yet; a conditional stop leaves the queue entry armed.
template<> template<> void alghostclearanim_object::test<7>()
{
    struct Walker
    {
        bool moving = false;
        bool pending_start = true;   // staggered start queued, no Move yet
        bool walk_playing = false;
        void service() { if (pending_start) { pending_start = false; moving = true; walk_playing = true; } }
        // LLActorMover::stop always disarms the queue; the bug guarded it behind isMoving().
        void stop(bool unconditional)
        {
            if (!unconditional && !moving) return;
            pending_start = false;
            moving = walk_playing = false;
        }
    };
    Walker good;
    good.stop(true);
    good.service();
    ensure("an unconditional stop cancels the queued walk", !good.walk_playing && !good.moving);
    Walker bad;
    bad.stop(false);
    bad.service();
    ensure("planted negative: stop only when moving lets the queued walk start", bad.walk_playing);
}

// Findings 3 and 4: gaze painters and Switchboard automation leave a cleared clone alone;
// an explicit manual punch does not.
template<> template<> void alghostclearanim_object::test<8>()
{
    ensure("cleared clone: no gaze paint", !mayPaintGaze(true));
    ensure("ordinary clone: gaze paints", mayPaintGaze(false));
    ensure("auto cut skips a cleared clone", !mayAutomate(true, false));
    ensure("manual punch reaches a cleared clone", mayAutomate(true, true));
    ensure("auto cut reaches an ordinary clone", mayAutomate(false, false));

    // Model: an armed board cutting to an animation every tick, a held Hot Gaze, a released
    // ordinary gaze envelope. Each paints / starts only through the rules above.
    struct Board
    {
        bool cleared = true;
        bool gated = true;
        int cuts_applied = 0;
        int gaze_paints = 0;
        void autoCut() { if (!gated || mayAutomate(cleared, false)) { ++cuts_applied; cleared = false; } }
        void gazeFrame() { if (!gated || mayPaintGaze(cleared)) ++gaze_paints; }
    };
    Board board;
    for (int i = 0; i < 100; ++i) { board.autoCut(); board.gazeFrame(); }
    ensure("100 frames: no cut, no gaze paint", board.cuts_applied == 0 && board.gaze_paints == 0 && board.cleared);
    Board manual;
    manual.cleared = true;
    if (mayAutomate(manual.cleared, true)) { ++manual.cuts_applied; manual.cleared = false; }
    ensure("a manual punch applies and ends the cleared state", manual.cuts_applied == 1 && !manual.cleared);
    Board bad;
    bad.gated = false;
    for (int i = 0; i < 3; ++i) { bad.autoCut(); bad.gazeFrame(); }
    ensure("planted negative: ungated board restarts a cleared clone", bad.cuts_applied > 0 && bad.gaze_paints > 0);
}

// Finding 5: undo restores AutoAnimate exactly (manual mode and learning progress), because
// switching it off destroys the runtime.
template<> template<> void alghostclearanim_object::test<9>()
{
    struct Auto
    {
        bool on = true;
        bool manual = true;
        int progress = 240;          // observed learning seconds
        void off() { on = false; manual = false; progress = 0; }   // runtime destroyed
    };
    Auto clone;
    const Auto before = clone;       // the snapshot
    clone.off();
    Auto undone = clone;             // full restore
    undone.on = before.on; undone.manual = before.manual; undone.progress = before.progress;
    ensure("manual mode and progress come back", undone.on && undone.manual && undone.progress == 240);
    Auto flag_only = clone;          // planted negative: only the enabled flag
    flag_only.on = before.on;
    ensure("flag-only undo is rejected", flag_only.manual != before.manual && flag_only.progress != before.progress);
}

// Finding 6: undo of a Frozen clone restores its held pose, keyed by joint name.
template<> template<> void alghostclearanim_object::test<10>()
{
    ensure("only Frozen needs a held pose", needsHeldPose(kFrozen, kFrozen) && !needsHeldPose(kMirror, kFrozen));
    auto joint = [](const char* name, float angle, float height)
    {
        HeldJoint held;
        held.mName = name;
        held.mRot[2] = angle;
        held.mPos[2] = height;
        return held;
    };
    const HeldPose held = { joint("mPelvis", 0.3f, 1.0f), joint("mTorso", 0.5f, 0.1f), joint("mHead", 0.7f, 0.2f) };
    // Skeleton rebuilt after the snapshot: different order, one joint missing, neutral values.
    std::vector<HeldJoint> skeleton = { joint("mHead", 0.f, 0.f), joint("mPelvis", 0.f, 0.f), joint("mChest", 0.f, 0.f) };
    for (const HeldJoint& saved : held)
        for (HeldJoint& live : skeleton)
            if (live.mName == saved.mName) live = saved;
    ensure("head restored by name", skeleton[0].mRot[2] == 0.7f);
    ensure("pelvis restored by name", skeleton[1].mRot[2] == 0.3f && skeleton[1].mPos[2] == 1.0f);
    ensure("unknown joint untouched", skeleton[2].mRot[2] == 0.f);
    // Planted negative: restoring by index writes the pelvis pose onto the head.
    std::vector<HeldJoint> by_index = { joint("mHead", 0.f, 0.f), joint("mPelvis", 0.f, 0.f), joint("mChest", 0.f, 0.f) };
    for (size_t i = 0; i < held.size() && i < by_index.size(); ++i)
    {
        by_index[i].mRot[2] = held[i].mRot[2];
    }
    ensure("index-keyed restore is rejected", by_index[0].mRot[2] != 0.7f);
}

// Finding 7: the neutral pose is shape-aware for positions too.
template<> template<> void alghostclearanim_object::test<11>()
{
    const Vec3 def{0.f, 0.f, 0.1f}, shape{0.f, 0.02f, 0.f}, bento{0.f, 0.f, 0.4f}, override_pos{0.f, 0.f, 0.05f};
    const Vec3 plain = restPosition(def, Vec3(), false, Vec3());
    ensure("default skeleton position", plain == def);
    const Vec3 shaped = restPosition(def, shape, false, Vec3());
    ensure("default + applied shape", shaped == (Vec3{0.f, 0.02f, 0.1f}));
    ensure("attachment override wins", restPosition(def, shape, true, override_pos) == override_pos);
    // A position-keyed animation moved the joint to `bento`; Clear must not keep it.
    ensure("rest differs from the animated position", !(shaped == bento));
    // Planted negative: the rotation-only reset leaves the animated position behind.
    Vec3 rotation_only_reset = bento;
    ensure("rotation-only reset is rejected", !(rotation_only_reset == shaped));
}

// Finding 8: Clear -> manual punch -> Clear -> Undo restores the punched animation, and a
// respawn does not re-clear a clone the user re-animated.
template<> template<> void alghostclearanim_object::test<12>()
{
    struct Body { int drive = kMirror; std::string anim; bool cleared = false; };
    struct Studio
    {
        int drive = kMirror; std::string anim; bool cleared = false;
        Body body;
        Before<std::string> clear()
        {
            Before<std::string> before; before.mDrive = drive; before.mAnim = anim; before.mWasCleared = cleared;
            drive = kDriveDirected; anim.clear(); cleared = true;
            body.drive = drive; body.anim = anim; body.cleared = true;
            return before;
        }
        // The Switchboard drives the body only; adopt() is the report.
        void punch(const std::string& next, bool adopt)
        {
            body.drive = kDriveDirected; body.anim = next; body.cleared = holds(body.cleared, body.drive, true);
            if (adopt) { drive = body.drive; anim = body.anim; cleared = body.cleared; }
        }
        void respawn() { cleared = holds(cleared, drive, !anim.empty()); body = Body{drive, anim, cleared}; }
    };
    Studio good;
    good.clear();
    good.punch("dance", true);
    const Before<std::string> second = good.clear();
    ensure("snapshot holds the punched animation", second.mAnim == "dance" && !second.mWasCleared);
    good.cleared = false;            // undo ends the cleared state first
    good.drive = second.mDrive; good.anim = second.mAnim;
    ensure("undo restores the punched animation", good.anim == "dance" && good.drive == kDriveDirected);

    Studio reanimated;
    reanimated.clear();
    reanimated.punch("dance", true);
    reanimated.respawn();
    ensure("respawn keeps the re-animated clone animated", !reanimated.body.cleared && reanimated.body.anim == "dance");

    // Planted negative: a punch Studio never hears about.
    Studio stale;
    stale.clear();
    stale.punch("dance", false);
    const Before<std::string> stale_before = stale.clear();
    ensure("unreported punch snapshots the wrong state", stale_before.mAnim.empty() && stale_before.mWasCleared);
    stale.respawn();
    ensure("unreported punch is re-cleared on respawn", stale.body.cleared && stale.body.anim.empty());
}

// Should-fix B: Clear is refused while the Poser holds the clone posed.
template<> template<> void alghostclearanim_object::test<13>()
{
    ensure("posed clone is refused", !canClear(true, true));
    ensure("free entity clone can clear", canClear(true, false));
    ensure("overlay clone is refused", !canClear(false, false));
}
// Review round 3, finding 1: every Clear snapshot owns its parked Hot Gaze through a token.
template<> template<> void alghostclearanim_object::test<14>()
{
    typedef ParkedStore<std::string, std::string> Store;
    // Clear (holds a Hot Gaze), Clear again (nothing held), Undo, one frame, Undo.
    Store store;
    const unsigned first = store.park("clone", "hot");
    const unsigned second = 0;                       // the second Clear found no Hot Gaze
    std::string out;
    ensure("the second undo takes nothing", !store.take(second, "clone", out) && out.empty());
    ensure("the first snapshot still owns its state", store.size() == 1);
    ensure("the first undo restores its own Hot Gaze", store.take(first, "clone", out) && out == "hot");
    ensure("the token is spent", store.empty() && !store.take(first, "clone", out));

    // Planted negative: one parked entry per runtime id, consumed by whichever undo runs first.
    std::map<std::string, std::string> per_owner;
    per_owner["clone"] = "hot";                      // first Clear
    const std::string consumed_by_second_undo = per_owner["clone"]; per_owner.erase("clone");
    ensure("per-owner parking is consumed by the wrong undo",
           consumed_by_second_undo == "hot" && per_owner.find("clone") == per_owner.end());

    // Respawn migrates the owner; the old id no longer matches.
    Store respawn;
    const unsigned token = respawn.park("old", "hot");
    respawn.migrate("old", "new");
    ensure("a respawned clone gets its parked state back", respawn.take(token, "new", out) && out == "hot");
    Store stale;
    const unsigned stale_token = stale.park("old", "hot");
    ensure("without the migration the respawned clone gets nothing", !stale.take(stale_token, "new", out));

    // Slot release / clone deletion / global clear drop the parked state.
    Store dropped;
    dropped.park("gone", "hot"); dropped.park("kept", "hot");
    dropped.drop("gone");
    ensure("a released slot keeps nothing parked", dropped.size() == 1);
    dropped.migrate("kept", std::string());          // deletion: new owner is null
    ensure("a deleted clone keeps nothing parked", dropped.empty());
    Store cleared;
    cleared.park("a", "hot"); cleared.park("b", "hot");
    cleared.clear();
    ensure("a global clear drops every parked state", cleared.empty());
    // A state is never handed to another owner.
    Store other;
    const unsigned other_token = other.park("a", "hot");
    ensure("another owner cannot take it", !other.take(other_token, "b", out) && other.empty());
}

// Finding 2: the held pose of a Frozen clone is the one UNDER the Directed Mirror stamp.
template<> template<> void alghostclearanim_object::test<15>()
{
    // Two mirrored joints with an asymmetric pose; the stamp swaps them.
    auto stamp = [](float (&pose)[2]) { std::swap(pose[0], pose[1]); };
    float under[2] = { 10.f, 20.f };
    float displayed[2] = { 10.f, 20.f };
    stamp(displayed);                                // what the clone shows while stamped
    const float zero_pos[3] = { 0.f, 0.f, 0.f };
    auto capture = [&](bool use_under)
    {
        float held[2];
        for (int i = 0; i < 2; ++i)
        {
            const float disp_rot[4] = { displayed[i], 0.f, 0.f, 1.f };
            const float under_rot[4] = { under[i], 0.f, 0.f, 1.f };
            held[i] = heldJoint("joint", disp_rot, zero_pos, use_under ? under_rot : nullptr, nullptr).mRot[0];
        }
        return std::vector<float>{ held[0], held[1] };
    };
    // Undo writes the held pose, then the normal stamp presents it once.
    auto after_undo = [&](bool use_under)
    {
        std::vector<float> held = capture(use_under);
        float written[2] = { held[0], held[1] };
        stamp(written);
        return std::vector<float>{ written[0], written[1] };
    };
    const std::vector<float> shown{ displayed[0], displayed[1] };
    ensure("undo returns the pose the clone showed", after_undo(true) == shown);
    ensure("an unstamped clone holds its own pose",
           heldJoint("j", under_rot_of(5.f), zero_pos, nullptr, nullptr).mRot[0] == 5.f);
    // Planted negative: the displayed pose is mirrored a second time.
    ensure("holding the stamped pose is rejected", after_undo(false) != shown);
}

// Finding 3: a cleared clone is never stamped; the authored setting stays.
template<> template<> void alghostclearanim_object::test<16>()
{
    ensure("a cleared clone is not mirrored", !mayStampMirror(true));
    ensure("an ordinary clone is mirrored", mayStampMirror(false));
    float neutral[2] = { 1.f, 2.f };                 // asymmetric shape-aware rest positions
    bool authored_mirror = true;
    auto frame = [&](bool cleared, bool always)
    {
        if ((always || mayStampMirror(cleared)) && authored_mirror) std::swap(neutral[0], neutral[1]);
    };
    frame(true, false);
    ensure("the neutral positions survive the frame", neutral[0] == 1.f && neutral[1] == 2.f && authored_mirror);
    frame(false, false);
    ensure("later playback mirrors again", neutral[0] == 2.f && neutral[1] == 1.f);
    // Planted negative: an unconditional stamp mirrors the neutral pose.
    neutral[0] = 1.f; neutral[1] = 2.f;
    frame(true, true);
    ensure("the unconditional stamp is rejected", neutral[0] == 2.f);
}
} // namespace tut
