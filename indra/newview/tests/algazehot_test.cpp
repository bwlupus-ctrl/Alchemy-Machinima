/** Native contract for live hot gaze envelopes and scene-owned slot data. */
#ifndef AL_HOT_GAZE_STANDALONE
#include "linden_common.h"
#ifndef AL_HOT_GAZE_NATIVE_MAIN
#include "../test/lltut.h"
#endif
#include "../algazehot.h"
#include "../algazehotslots.h"
#endif
#include <cmath>
#include <stdexcept>

namespace
{
void hotRequire(bool pass) { if (!pass) throw std::runtime_error("hot gaze contract failed"); }
bool hotNear(F32 a, F32 b) { return std::fabs(a - b) < 1.e-5f; }
void hotGazeContract()
{
    ALGazeHot::State s;
    hotRequire(s.finished(0) && s.weight(0) == 0);
    s.fire(0, 10, .2f, 2, 1, false, false);
    hotRequire(hotNear(s.weight(10.1), .5f));
    hotRequire(s.weight(12.2) == 1 && !s.finished(12.2));
    hotRequire(hotNear(s.weight(12.7), .5f));
    hotRequire(s.finished(13.21) && s.weight(13.21) == 0);
    s.fire(1, 20, 1, 0, 2, true, false);
    const F32 partial = s.weight(20.25);
    s.release(20.25);
    hotRequire(hotNear(s.weight(20.25), partial));
    hotRequire(hotNear(s.weight(21.25), partial * .5f));
    hotRequire(!s.finished(22.24) && s.finished(22.26));
    s.fire(2, 30, 0, 0, 1, false, true);
    hotRequire(s.weight(30) == 1 && !s.finished(3000));
    s.release(31); hotRequire(s.finished(32));
    s.fire(0, 40, .2f, 2, 1, true, false);
    hotRequire(!s.finished(4000)); // Hold ignores After.
    const F32 before = s.weight(40.1);
    s.fire(0, 40.1, .4f, 2, 1, true, false); // retrigger acquisition
    hotRequire(hotNear(s.weight(40.1), before));
    s.fire(3, 40.2, .5f, 2, 1, false, false); // another slot takes over
    hotRequire(s.mSlot == 3 && s.weight(40.2) >= before);
    s.release(40.25); const F32 returning = s.weight(40.5);
    s.fire(3, 40.5, .5f, 2, 1, false, false);
    hotRequire(hotNear(s.weight(40.5), returning));
    s.release(40.6); s.release(40.8); // duplicate up does not restart return
    hotRequire(s.mRelease == 40.6 && s.finished(41.61));

    ALGazeHot::Slots slots;
    const LLUUID actor = LLUUID::generateNewID(), clone = LLUUID::generateNewID();
    const LLUUID recovered = LLUUID::generateNewID(), object = LLUUID::generateNewID();
    slots[0].mWho = actor; slots[0].mTarget.mMode = LLActorMover::GazeTarget::OBJECT;
    slots[0].mTarget.mObjectRef = object; slots[0].mTargetName = "Moving fist";
    slots[0].mTarget.mPickedSpot = true; slots[0].mTarget.mObjectOffset = LLVector3(.1f, .2f, .3f);
    slots[0].mTarget.mObjectJoint = "mWristLeft";
    slots[0].mHasTarget = true; slots[0].mSnap = .3f; slots[0].mEyes = .7f;
    slots[1].mGroup = "Fighters"; slots[1].mHold = true; slots[1].mStay = true;
    slots[1].mTarget.mMode = LLActorMover::GazeTarget::FIXED_POINT;
    slots[1].mTarget.mPointNudge = LLVector3(-.2f, .3f, .4f);
    slots[1].mTarget.mFixedPoint = LLVector3d(4, 5, 6); slots[1].mHasTarget = true;
    slots[2].mWho = clone; slots[2].mTarget.mMode = LLActorMover::GazeTarget::CAST_MEMBER;
    slots[2].mTarget.mCastRef = clone; slots[2].mHasTarget = true;
    const LLSD saved = ALGazeHot::writeSlots(slots, [&](const LLUUID& id) { return id != clone; });
    const auto loaded = ALGazeHot::readSlots(saved);
    hotRequire(loaded[0].mWho == actor && loaded[0].mTarget.mObjectRef == object);
    hotRequire(loaded[0].mTarget.mPickedSpot && loaded[0].mTarget.mObjectOffset == LLVector3(.1f, .2f, .3f));
    hotRequire(loaded[0].mTarget.mObjectJoint == "mWristLeft");
    hotRequire(loaded[1].mTarget.mPointNudge == LLVector3(-.2f, .3f, .4f));
    hotRequire(loaded[0].mTargetName == "Moving fist" && hotNear(loaded[0].mEyes, .7f));
    hotRequire(loaded[1].mGroup == "Fighters" && loaded[1].mHold && loaded[1].mStay);
    hotRequire(loaded[1].mTarget.mFixedPoint == LLVector3d(4, 5, 6));
    hotRequire(loaded[2].mWho.isNull() && !loaded[2].mHasTarget);
    hotRequire(slots[2].mWho == clone && slots[2].mHasTarget); // saving does not mutate setup
    hotRequire(ALGazeHot::rewrite(slots, clone, recovered));
    hotRequire(slots[2].mWho == recovered && slots[2].mTarget.mCastRef == recovered);
    hotRequire(ALGazeHot::rewrite(slots, recovered, LLUUID::null));
    hotRequire(slots[2].mWho.isNull() && !slots[2].mHasTarget);
    hotRequire(ALGazeHot::readSlots(LLSD())[0].mSnap == .12f);
}
}

#if defined(AL_HOT_GAZE_NATIVE_MAIN)
int main()
{
    hotGazeContract();
    return 0;
}
#elif !defined(AL_HOT_GAZE_STANDALONE)
namespace tut
{
struct algazehot_data {};
typedef test_group<algazehot_data> algazehot_group;
algazehot_group algazehot_tests("algazehot");
template<> template<> void algazehot_group::object::test<1>()
{
    set_test_name("tap/hold envelopes, takeover, release, scene round-trip and clone references");
    hotGazeContract();
}
}
#endif
