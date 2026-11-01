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
        const LLVector3 up = LLVector3::y_axis * q;
        ensure("up perpendicular to beam", std::fabs(up * direction) < 2.e-4f);
        ensure("unit up", std::fabs(up.length() - 1.f) < 2.e-4f);
    }
}
template<> template<> void projector_track_object::test<2>()
{
    // Full outside -> cone -> pole -> cone -> outside sweep at fine steps.
    // Bound adjacent up changes relative to beam travel, including noise near
    // both former hard thresholds, on both sides of both poles.
    for (F32 sign : {-1.f, 1.f})
    {
        LLVector3 old_beam(.5f, 0, sign);
        old_beam.normalize();
        LLQuaternion q = aim(old_beam, LLQuaternion());
        auto step = [&](LLVector3 beam)
        {
            beam.normalize();
            const LLVector3 old_up = LLVector3::y_axis * q;
            q = aim(beam, q);
            const LLVector3 up = LLVector3::y_axis * q;
            const F32 beam_angle = std::atan2((old_beam % beam).length(), old_beam * beam);
            const F32 up_angle = std::atan2((old_up % up).length(), old_up * up);
            ensure("roll bounded by beam travel", up_angle <= 3.1f * beam_angle + 2.e-5f);
            projector_track_data::expectVector("beam through pole", LLVector3(0,0,-1) * q, beam);
            old_beam = beam;
        };
        for (S32 n = 499; n >= -500; --n) step(LLVector3(n * .001f, 0, sign));
        for (F32 side : {-1.f, 1.f})
        {
            for (S32 n = -499; n <= 500; ++n) step(LLVector3(n * .001f * side, 0, sign));
            for (S32 n = 499; n >= 203; --n) step(LLVector3(n * .001f * side, 0, sign));
            for (S32 n = 0; n < 1000; ++n)
                step(LLVector3(side * (.20306f + .0003f * std::sin(n * .1f)),
                              .0001f * std::sin(n * .07f), sign));
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

template<> template<> void projector_track_object::test<7>()
{
    const LLVector3 foot(10,20,1), native(10,20,3), animated(.2f,-.1f,.3f);
    LLMatrix4 outer;
    outer.initAll(LLVector3(2,2,2), LLQuaternion(), -foot);
    const LLVector3 visible = renderedJoint(native, outer);
    expectVector("scaled rendered joint", visible, foot + (native - foot) * 2.f);
    expectVector("animated displacement scales too", renderedJoint(native + animated, outer) - visible, animated * 2.f);
    const Pose frame{visible, LLQuaternion(.4f, LLVector3::z_axis)};
    const Pose light{visible + LLVector3(1,2,3), LLQuaternion()};
    const Pose offset = relative(light, frame);
    const Pose moved{renderedJoint(native + animated, outer), frame.rotation};
    expectVector("capture and tick share outer transform", apply(offset, moved).position, light.position + animated * 2.f);
}
template<> template<> void projector_track_object::test<8>()
{
    ensure("root region conversion allowed", restorableFrame(false, false, false));
    ensure("child region propagation preserves locals", restorableFrame(true, true, true));
    ensure("orphan reunion with packet parent allowed", restorableFrame(true, true, true));
    ensure("unresolved packet parent stays guarded", !restorableFrame(true, false, false));
    ensure("different parent stays guarded", !restorableFrame(true, true, false));
    ensure("root snapshot cannot restore into child", !restorableFrame(false, true, false));
    ensure("child snapshot cannot restore into root", !restorableFrame(true, false, true));
}
template<> template<> void projector_track_object::test<9>()
{
    const LLQuaternion yaw(.6f, LLVector3::z_axis);
    const LLQuaternion body = LLQuaternion(.2f, LLVector3::x_axis) * yaw;
    sameRotation(bodyYaw(body), yaw);
}
}
