#include "linden_common.h"
#include "../test/lltut.h"
#include "../alprojectortrackmath.h"

namespace tut
{
using namespace ALProjectorTrackMath;
struct projector_track_data
{
    static void expectVector(const std::string& why, const LLVector3& actual, const LLVector3& expected)
    {
        ensure(why, (actual - expected).length() < 2.e-4f);
    }
    static void sameRotation(const LLQuaternion& actual, const LLQuaternion& expected)
    {
        expectVector("rotation X", LLVector3::x_axis * actual, LLVector3::x_axis * expected);
        expectVector("rotation Y", LLVector3::y_axis * actual, LLVector3::y_axis * expected);
        expectVector("rotation Z", LLVector3::z_axis * actual, LLVector3::z_axis * expected);
    }
};
typedef test_group<projector_track_data> projector_track_group;
typedef projector_track_group::object projector_track_object;
projector_track_group projector_track_tests("alprojectortrackmath");

template<> template<> void projector_track_object::test<1>()
{
    for (LLVector3 direction : {LLVector3(1,0,0), LLVector3(-1,0,0), LLVector3(0,1,0),
            LLVector3(0,-1,0), LLVector3(0,0,1), LLVector3(0,0,-1), LLVector3(1,2,3)})
    {
        direction.normalize();
        const LLQuaternion q = aim(direction, LLQuaternion());
        ensure("finite aim", q.isFinite());
        expectVector("beam is local -Z", LLVector3(0,0,-1) * q, direction);
        if (std::fabs(direction.mV[2]) < .98f)
        {
            LLVector3 up = LLVector3::z_axis - direction * direction.mV[2];
            up.normalize();
            expectVector("gobo up is projected world Z", LLVector3::y_axis * q, up);
        }
    }
}
template<> template<> void projector_track_object::test<2>()
{
    // Cross directly over each pole: tiny positional noise must not spin the gobo.
    for (F32 sign : {-1.f, 1.f})
    {
        LLQuaternion q = aim(LLVector3(.02f,0,sign), LLQuaternion());
        for (S32 step = 19; step >= -20; --step)
        {
            const LLVector3 old_up = LLVector3::y_axis * q;
            LLVector3 direction(step * .001f, 0, sign);
            direction.normalize();
            q = aim(direction, q);
            expectVector("near-vertical beam", LLVector3(0,0,-1) * q, direction);
            ensure("continuous roll through pole", old_up * (LLVector3::y_axis * q) > .999f);
        }
    }
    const LLQuaternion previous(.7f, LLVector3::x_axis);
    sameRotation(aim(LLVector3::zero, previous), previous);
}
template<> template<> void projector_track_object::test<3>()
{
    const Pose frame{{10,20,3}, LLQuaternion(F_PI_BY_TWO, LLVector3::z_axis)};
    // Body-local forward offset (2,0,1) is world (0,2,1) at 90 degrees yaw.
    const Pose light{{10,22,4}, LLQuaternion(.4f, LLVector3::x_axis) * frame.rotation};
    const Pose offset = relative(light, frame);
    expectVector("captured body offset", offset.position, LLVector3(2,0,1));
    sameRotation(offset.rotation, LLQuaternion(.4f, LLVector3::x_axis));
    const Pose turned{{20,30,5}, LLQuaternion(F_PI, LLVector3::z_axis)};
    const Pose result = apply(offset, turned);
    expectVector("follows translated joint with turned body", result.position, LLVector3(18,30,6));
    // A local light axis rolls with the body, independent of any wrist twist.
    sameRotation(result.rotation, LLQuaternion(.4f, LLVector3::x_axis) * turned.rotation);
}
template<> template<> void projector_track_object::test<4>()
{
    const Pose parent{{50,-20,8}, LLQuaternion(.9f, LLVector3(1,2,3))};
    const Pose desired{{-3,9,14}, LLQuaternion(-.7f, LLVector3(3,-2,1))};
    const Pose child = relative(desired, parent);
    expectVector("child local position", child.position * parent.rotation + parent.position, desired.position);
    sameRotation(child.rotation * parent.rotation, desired.rotation);
    const Pose identity{{0,0,0}, LLQuaternion()};
    expectVector("root position unchanged by identity parent", relative(desired, identity).position, desired.position);
}
template<> template<> void projector_track_object::test<5>()
{
    const Pose initial{{0,0,0}, LLQuaternion()};
    const Pose target{{10,-5,3}, LLQuaternion(2.f, LLVector3::z_axis)};
    Pose at30 = initial, at144 = initial;
    for (S32 n = 0; n < 30; ++n) at30 = smooth(at30, target, 1.f/30.f, .15f);
    for (S32 n = 0; n < 144; ++n) at144 = smooth(at144, target, 1.f/144.f, .15f);
    const F32 expected = 1.f - std::exp(-1.f / .15f);
    expectVector("analytic exponential position", at30.position, target.position * expected);
    expectVector("frame-rate independent position", at30.position, at144.position);
    sameRotation(at30.rotation, at144.rotation);
    sameRotation(at30.rotation, LLQuaternion(2.f * expected, LLVector3::z_axis));
}
template<> template<> void projector_track_object::test<6>()
{
    const Pose initial{{1,2,3}, LLQuaternion(.2f, LLVector3::x_axis)};
    const Pose target{{7,8,9}, LLQuaternion(.8f, LLVector3::y_axis)};
    const Pose locked = smooth(initial, target, 1.f/144.f, 0.f);
    expectVector("zero smoothing locks position", locked.position, target.position);
    sameRotation(locked.rotation, target.rotation);
    const Pose held = smooth(initial, target, 0.f, .15f);
    expectVector("zero dt holds position", held.position, initial.position);
    sameRotation(held.rotation, initial.rotation);
}
}
