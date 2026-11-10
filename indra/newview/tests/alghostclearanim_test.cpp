#include "linden_common.h"
#include "../test/lltut.h"
#include "../alghostclearanim.h"

#include <iterator>
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
} // namespace tut
