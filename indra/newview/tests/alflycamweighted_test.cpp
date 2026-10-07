/**
 * @file alflycamweighted_test.cpp
 * @brief Weighted Classic pure-core gates (design R3, section 11).
 */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alflycamweighted.h"
#include "../alcameradispatch.h"
#include "m3math.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <sstream>

namespace tut
{
namespace
{
constexpr F64 PI_D = 3.1415926535897932384626433832795;

void close(const char* label, F64 actual, F64 expected, F64 tolerance)
{
    if (std::fabs(actual - expected) <= tolerance) return;
    std::ostringstream message;
    message.precision(17);
    message << label << " expected " << expected << " actual " << actual;
    ensure(message.str(), false);
}

F64 magnitude(const LLVector3d& value)
{
    return std::sqrt(value.mdV[VX] * value.mdV[VX] +
                     value.mdV[VY] * value.mdV[VY] +
                     value.mdV[VZ] * value.mdV[VZ]);
}

F64 rotationDistance(const LLQuaternion& lhs, const LLQuaternion& rhs)
{
    F64 dot = 0.0, aa = 0.0, bb = 0.0;
    for (S32 index = 0; index < 4; ++index)
    {
        const F64 a = static_cast<F64>(lhs.mQ[index]);
        const F64 b = static_cast<F64>(rhs.mQ[index]);
        dot += a * b; aa += a * a; bb += b * b;
    }
    const F64 cosine = std::fabs(dot / std::sqrt(aa * bb));
    return 2.0 * std::acos(std::max(-1.0, std::min(1.0, cosine)));
}

bool exactQuaternion(const LLQuaternion& lhs, const LLQuaternion& rhs)
{
    return std::memcmp(lhs.mQ, rhs.mQ, sizeof(lhs.mQ)) == 0;
}

void exactQuaternionDiagnostic(const char* label, const LLQuaternion& actual,
                               const LLQuaternion& expected, bool blackDragon,
                               bool level, bool automatic, U32 rollDefault)
{
    if (exactQuaternion(actual, expected)) return;
    std::ostringstream message;
    message.precision(9);
    message << label << " BD=" << blackDragon << " level=" << level
            << " auto=" << automatic << " rollDefault=" << rollDefault
            << " expected (" << expected.mQ[0] << ',' << expected.mQ[1] << ','
            << expected.mQ[2] << ',' << expected.mQ[3] << ") actual ("
            << actual.mQ[0] << ',' << actual.mQ[1] << ',' << actual.mQ[2]
            << ',' << actual.mQ[3] << ')';
    ensure(message.str(), false);
}

F64 signedRoll(const LLQuaternion& rotation)
{
    const LLMatrix3 matrix(rotation);
    LLVector3 left(matrix.mMatrix[1]);
    left.normVec();
    return std::asin(std::max(-1.0, std::min(1.0,
        static_cast<F64>(left.mV[VZ]))));
}

S32 nonzeroSign(F64 value)
{
    return value > 1.e-7 ? 1 : (value < -1.e-7 ? -1 : 0);
}

ALFlycamWeighted::FrameIn basicFrame()
{
    ALFlycamWeighted::FrameIn in;
    in.frameOwner = ALFlycamPoseOwner::WEIGHTED;
    in.frameIntervalSeconds = .01;
    in.classicIm.blackDragon = false;
    in.classicIm.isZero = true;
    in.classicIm.time = .01f;
    in.classicIm.feather = 1.f;
    in.classicIn.frameIntervalSeconds = .01f;
    in.classicIn.minView = 0.f;
    in.classicIn.maxView = 3.f;
    in.settings.weight = 50.f;
    in.settings.tauMax = 1.f;
    in.settings.curve = 20.f;
    in.settings.rotationShare = .6f;
    return in;
}

ALFlycamWeighted::Target target(F64 x, F64 y = 0.0, F64 z = 0.0)
{
    ALFlycamWeighted::Target result;
    result.v.set(x, y, z);
    result.inputZero = x == 0.0 && y == 0.0 && z == 0.0;
    return result;
}

// Frozen verbatim copy of the pre-WP-W1 decision function. It intentionally
// does not call the production legacy helper, so D0 detects accidental drift.
ALFlycamFrameDecision legacyResolve(
    S32 modelSetting, bool cursor3D, bool orbitResolved,
    ALFlycamPoseOwner previousOwner, bool taggedDispatch,
    bool foreignSinceFlycamWrite)
{
    ALFlycamFrameDecision result;
    const bool droneSelected = modelSetting == 1 && !cursor3D;
    result.selectedOwner = orbitResolved ? ALFlycamPoseOwner::ORBIT :
        (droneSelected ? ALFlycamPoseOwner::DRONE : ALFlycamPoseOwner::CLASSIC);
    result.frameOwner = result.selectedOwner;
    result.foreignDroneRegain = taggedDispatch && foreignSinceFlycamWrite &&
        result.selectedOwner == ALFlycamPoseOwner::DRONE;
    if (previousOwner == result.selectedOwner)
    {
        result.discardDroneAccumulator = result.frameOwner == ALFlycamPoseOwner::ORBIT;
        result.seedDrone = result.foreignDroneRegain;
        result.resetOperator = result.foreignDroneRegain;
        return result;
    }
    if (previousOwner == ALFlycamPoseOwner::CLASSIC && result.selectedOwner == ALFlycamPoseOwner::DRONE)
    {
        result.edge = ALFlycamOwnerEdge::CLASSIC_TO_DRONE;
        result.frameOwner = result.foreignDroneRegain ? ALFlycamPoseOwner::DRONE : ALFlycamPoseOwner::CLASSIC;
        result.seedDrone = true; result.resetOperator = true;
    }
    else if (previousOwner == ALFlycamPoseOwner::DRONE && result.selectedOwner == ALFlycamPoseOwner::CLASSIC)
    {
        result.edge = ALFlycamOwnerEdge::DRONE_TO_CLASSIC;
        result.frameOwner = ALFlycamPoseOwner::DRONE; result.resetOperator = true;
    }
    else if (previousOwner == ALFlycamPoseOwner::CLASSIC && result.selectedOwner == ALFlycamPoseOwner::ORBIT)
        result.edge = ALFlycamOwnerEdge::CLASSIC_TO_ORBIT;
    else if (previousOwner == ALFlycamPoseOwner::ORBIT && result.selectedOwner == ALFlycamPoseOwner::CLASSIC)
        result.edge = ALFlycamOwnerEdge::ORBIT_TO_CLASSIC;
    else if (previousOwner == ALFlycamPoseOwner::DRONE && result.selectedOwner == ALFlycamPoseOwner::ORBIT)
    {
        result.edge = ALFlycamOwnerEdge::DRONE_TO_ORBIT;
        result.resetOperator = true; result.discardClassicShadowPose = true;
    }
    else if (previousOwner == ALFlycamPoseOwner::ORBIT && result.selectedOwner == ALFlycamPoseOwner::DRONE)
    {
        result.edge = ALFlycamOwnerEdge::ORBIT_TO_DRONE;
        result.seedDrone = true; result.resetOperator = true;
    }
    result.discardDroneAccumulator = result.frameOwner == ALFlycamPoseOwner::ORBIT;
    return result;
}

bool sameLegacyFields(const ALFlycamFrameDecision& lhs, const ALFlycamFrameDecision& rhs)
{
    return lhs.selectedOwner == rhs.selectedOwner && lhs.frameOwner == rhs.frameOwner &&
        lhs.edge == rhs.edge && lhs.resetOperator == rhs.resetOperator &&
        lhs.discardDroneAccumulator == rhs.discardDroneAccumulator &&
        lhs.discardClassicShadowPose == rhs.discardClassicShadowPose &&
        lhs.seedDrone == rhs.seedDrone && lhs.foreignDroneRegain == rhs.foreignDroneRegain;
}
}

struct weighted_data {};
typedef test_group<weighted_data> weighted_group;
typedef weighted_group::object weighted_object;
weighted_group gWeightedGroup("alflycamweighted");

// W0
template<> template<> void weighted_object::test<1>()
{
    const LLVector3d vector(0.0, 0.0, .4);
    const LLQuaternion actual = ALFlycamWeighted::rotationFromVector(vector);
    LLQuaternion expected; expected.setAngleAxis(.4f, LLVector3(0.f, 0.f, 1.f));
    ensure("W0 angle-axis convention", rotationDistance(actual, expected) < 1.e-6);
    LLQuaternion base; base.setAngleAxis(.3f, LLVector3(0.f, 1.f, 0.f));
    ensure("W0 left multiplication", rotationDistance(actual * base, expected * base) < 1.e-6);
}

// W1
template<> template<> void weighted_object::test<2>()
{
    close("W1 zero", ALFlycamWeighted::tuningForWeight(0.f, 1.f, 20.f, .6f).tauT, 0., 0.);
    close("W1 full", ALFlycamWeighted::tuningForWeight(100.f, 1.f, 20.f, .6f).tauT, 1., 1.e-12);
    const auto middle = ALFlycamWeighted::tuningForWeight(50.f, 1.f, 20.f, .6f);
    close("W1 mapping", middle.tauT, (std::sqrt(20.) - 1.) / 19., 1.e-12);
    close("W1 rotation share", middle.tauR, .6 * middle.tauT, 1.e-8);
    ensure_equals("W1 NaN clamp", ALFlycamWeighted::tuningForWeight(
        std::numeric_limits<F32>::quiet_NaN(), 1.f, 20.f, .6f).tauT, 0.0);
    // §8 range clamps: TauMax [0.1,3], Curve [1,1000], RotationShare [0,2]; non-finite -> default.
    close("W1 tauMax upper clamp", ALFlycamWeighted::tuningForWeight(100.f, 50.f, 20.f, .6f).tauT, 3., 1.e-9);
    close("W1 tauMax lower clamp", ALFlycamWeighted::tuningForWeight(100.f, 0.f, 20.f, .6f).tauT, .1, 1.e-9);
    close("W1 tauMax NaN default", ALFlycamWeighted::tuningForWeight(100.f,
        std::numeric_limits<F32>::quiet_NaN(), 20.f, .6f).tauT, 1., 1.e-9);
    close("W1 curve upper clamp", ALFlycamWeighted::tuningForWeight(50.f, 1.f, 1.e6f, .6f).tauT,
        (std::sqrt(1000.) - 1.) / 999., 1.e-9);
    close("W1 curve lower clamp (linear)", ALFlycamWeighted::tuningForWeight(50.f, 1.f, -5.f, .6f).tauT, .5, 1.e-9);
    const auto share_hi = ALFlycamWeighted::tuningForWeight(100.f, 1.f, 20.f, 9.f);
    close("W1 share upper clamp", share_hi.tauR, 2. * share_hi.tauT, 1.e-9);
    const auto share_lo = ALFlycamWeighted::tuningForWeight(100.f, 1.f, 20.f, -1.f);
    close("W1 share lower clamp", share_lo.tauR, 0., 0.);
    for (S32 preset = 0; preset < 4; ++preset)
        ensure_equals("W1 preset round trip", ALFlycamWeighted::presetForWeight(
            ALFlycamWeighted::presetWeight(preset)), preset);
    ensure_equals("W1 custom", ALFlycamWeighted::presetForWeight(37.f), 4);
}

// W2
template<> template<> void weighted_object::test<3>()
{
    ensure_equals("W2 Classic", alEffectiveFlycamModel(0, 50.f), 0);
    ensure_equals("W2 Drone", alEffectiveFlycamModel(1, 0.f), 1);
    ensure_equals("W2 Weighted", alEffectiveFlycamModel(2, 50.f), 2);
    ensure_equals("W2 bypass", alEffectiveFlycamModel(2, 0.f), 0);
    ensure_equals("W2 NaN bypass", alEffectiveFlycamModel(2,
        std::numeric_limits<F32>::quiet_NaN()), 0);
}

// W4
template<> template<> void weighted_object::test<4>()
{
    ALFlycamWeighted::State fixed, jittered;
    ALFlycamWeighted::rest(fixed); ALFlycamWeighted::rest(jittered);
    const ALFlycamWeighted::Tuning tuning{.2, .12};
    LLVector3d fixedDistance, jitteredDistance;
    for (S32 index = 0; index < 100; ++index)
        fixedDistance += ALFlycamWeighted::advance(
            fixed, target(index < 40 ? 3. : -2.), tuning, .01).dp;
    const F64 steps[] = {.003, .017, .011, .009};
    for (S32 index = 0; index < 100; ++index)
        jitteredDistance += ALFlycamWeighted::advance(
            jittered, target(index < 40 ? 3. : -2.), tuning, steps[index % 4]).dp;
    close("W4 equal elapsed", magnitude(fixedDistance - jitteredDistance), 0., 2.e-12);
}

// W5
template<> template<> void weighted_object::test<5>()
{
    ALFlycamWeighted::State one, two;
    ALFlycamWeighted::seedVelocity(one, LLVector3d(2., -.5, .2), LLVector3d(.1, 0., 0.));
    two = one;
    const auto tuning = ALFlycamWeighted::Tuning{.3, .2};
    const auto whole = ALFlycamWeighted::advance(one, target(4.), tuning, .07);
    const auto a = ALFlycamWeighted::advance(two, target(4.), tuning, .02);
    const auto b = ALFlycamWeighted::advance(two, target(4.), tuning, .05);
    close("W5 semigroup state", magnitude(one.vT - two.vT), 0., 2.e-12);
    close("W5 semigroup integral", magnitude(whole.dp - a.dp - b.dp), 0., 2.e-12);
}

// W6
template<> template<> void weighted_object::test<6>()
{
    ALFlycamWeighted::State state; ALFlycamWeighted::rest(state);
    F64 previous = 0.;
    for (S32 index = 1; index <= 300; ++index)
    {
        ALFlycamWeighted::advance(state, target(5.), {.2, .1}, .01);
        ensure("W6 monotone", state.vT.mdV[VX] >= previous && state.vT.mdV[VX] <= 5.);
        previous = state.vT.mdV[VX];
    }
}

// W7
template<> template<> void weighted_object::test<7>()
{
    ALFlycamWeighted::State state; ALFlycamWeighted::seedVelocity(state, LLVector3d(2.,0.,0.), LLVector3d());
    F64 distance = 0.;
    for (S32 index = 0; index < 4000 && !state.settled; ++index)
    {
        const auto step = ALFlycamWeighted::advance(state, target(0.), {.2,.1}, .01);
        ensure("W7 no reversal", state.vT.mdV[VX] >= 0.);
        distance += step.dp.mdV[VX];
    }
    close("W7 remaining coast 2 tau", distance, .8, 2.e-4);
    const ALFlycamWeighted::State before = state;
    const auto held = ALFlycamWeighted::advance(state, target(0.), {.2,.1}, .01);
    ensure("W7 exact hold", held.held && std::memcmp(&before, &state, sizeof(state)) == 0);
}

// W8
template<> template<> void weighted_object::test<8>()
{
    const F64 tau = .2;
    const F64 dt = .001;
    ALFlycamWeighted::State state;
    ALFlycamWeighted::seedVelocity(state, LLVector3d(1.,0.,0.), LLVector3d());
    S32 steps = 0;
    while (!state.settled && steps < 10000)
    {
        ALFlycamWeighted::advance(state, target(0.), {tau,.1}, dt);
        ++steps;
    }
    F64 expected = 0.0;
    while (tau * (2.0 + expected / tau) * std::exp(-expected / tau) >= 2.e-4)
        expected += dt;
    close("W8 measured settle time", static_cast<F64>(steps) * dt, expected, dt);
}

// W9
template<> template<> void weighted_object::test<9>()
{
    ALFlycamWeighted::State state; ALFlycamWeighted::rest(state);
    for (S32 index = 0; index < 40000; ++index)
        ALFlycamWeighted::advance(state, target(index * .001), {.2,.1}, .001);
    close("W9 ramp lag", 39.999 - state.vT.mdV[VX], .4, 1.e-3);
}

// W10
template<> template<> void weighted_object::test<10>()
{
    ALFlycamWeighted::State a, b; ALFlycamWeighted::rest(a); ALFlycamWeighted::rest(b);
    for (S32 index = 0; index < 100; ++index)
    {
        ALFlycamWeighted::advance(a, target(1.), {.2,.1}, .01);
        ALFlycamWeighted::advance(b, target(7.), {.2,.1}, .01);
    }
    close("W10 homogeneity", b.vT.mdV[VX], 7. * a.vT.mdV[VX], 2.e-12);
}

// W11
template<> template<> void weighted_object::test<11>()
{
    ALFlycamWeighted::State x, y; ALFlycamWeighted::rest(x); ALFlycamWeighted::rest(y);
    for (S32 index = 0; index < 100; ++index)
    {
        ALFlycamWeighted::advance(x, target(1.,0.,0.), {.2,.1}, .01);
        ALFlycamWeighted::advance(y, target(0.,1.,0.), {.2,.1}, .01);
    }
    close("W11 isotropy", x.vT.mdV[VX], y.vT.mdV[VY], 0.);
}

// W12
template<> template<> void weighted_object::test<12>()
{
    LLVector3d vector(0., 0., 1.2);
    LLQuaternion rotation = ALFlycamWeighted::rotationFromVector(vector);
    F32 angle = 0.f; LLVector3 axis; rotation.getAngleAxis(&angle, axis);
    close("W12 angle", angle, 1.2, 2.e-7);
    close("W12 axis", axis.mV[VZ], 1., 1.e-7);
}

// W13
template<> template<> void weighted_object::test<13>()
{
    const LLQuaternion half = ALFlycamWeighted::rotationFromVector(LLVector3d(0.,0.,PI_D));
    const LLQuaternion full = half * half;
    ensure("W13 180 finite", half.isFinite());
    ensure("W13 360 identity", rotationDistance(full, LLQuaternion()) < 1.e-5);
    ensure("W13 extreme", ALFlycamWeighted::isExtremeInput(
        ALFlycamWeighted::Target{LLVector3d(), LLVector3d(0.,0.,PI_D), false}, 1.));
}

// W14
template<> template<> void weighted_object::test<14>()
{
    const LLVector3d a(.01,0.,0.), b(0.,.01,0.);
    const LLQuaternion composed = ALFlycamWeighted::rotationFromVector(b) * ALFlycamWeighted::rotationFromVector(a);
    const LLQuaternion summed = ALFlycamWeighted::rotationFromVector(a + b);
    ensure("W14 BCH bound", rotationDistance(composed, summed) <= .5e-4 + 2.e-6);
}

// W15
template<> template<> void weighted_object::test<15>()
{
    const F32 delta[] = {.02f,0.f,0.f, 0.f,0.f,.01f};
    LLQuaternion q; q.setAngleAxis(.5f, LLVector3(0.f,0.f,1.f));
    const auto result = ALFlycamWeighted::targetFromClassic(delta, q, .01, false);
    close("W15 speed", magnitude(result.v), 2., 2.e-6);
    close("W15 angular target", result.w.mdV[VZ], 1., 2.e-5);
}

// W16
template<> template<> void weighted_object::test<16>()
{
    ALFlycamWeighted::State state; ALFlycamWeighted::rest(state);
    const auto step = ALFlycamWeighted::advance(state, target(3.), {0.,0.}, .2);
    // The pass-through path is F64, but 3.0 * 0.2 is not the same binary F64
    // value as the separately rounded literal 0.6.
    close("W16 pass-through integral", step.dp.mdV[VX], .6,
        std::numeric_limits<F64>::epsilon());
    close("W16 pass-through state", state.vT.mdV[VX], 3., 0.);
}

// W17
template<> template<> void weighted_object::test<17>()
{
    ALFlycamWeighted::State state; const auto desired = target(2.,-3.,4.);
    ALFlycamWeighted::seedAtTarget(state, desired);
    ensure("W17 active", state.active);
    close("W17 exact", magnitude(state.uT - desired.v) + magnitude(state.vT - desired.v), 0., 0.);
}

// W18
template<> template<> void weighted_object::test<18>()
{
    ALFlycamWeighted::State state;
    ALFlycamWeighted::seedVelocity(state, LLVector3d(2.,3.,4.), LLVector3d(.1,.2,.3));
    close("W18 history velocity", magnitude(state.vT - LLVector3d(2.,3.,4.)), 0., 0.);
    ALFlycamWeighted::seedVelocity(state, LLVector3d(std::numeric_limits<F64>::quiet_NaN(),0.,0.), LLVector3d());
    ensure("W18 invalid rests", state.settled && magnitude(state.vT) == 0.);
}

// W19
template<> template<> void weighted_object::test<19>()
{
    ensure("W19 translation extreme", ALFlycamWeighted::isExtremeInput(target(2000.01), .01));
    auto invalid = target(0.); invalid.w.mdV[VZ] = std::numeric_limits<F64>::infinity();
    ALFlycamWeighted::State state; ALFlycamWeighted::rest(state);
    ensure("W19 nonfinite", ALFlycamWeighted::advance(state, invalid, {.2,.1}, .01).nonFinite);
}

// W20
template<> template<> void weighted_object::test<20>()
{
    ALFlycamWeighted::State state;
    ALFlycamWeighted::seedVelocity(state, LLVector3d(1.,2.,3.), LLVector3d(4.,5.,6.));
    ALFlycamWeighted::zeroRoll(state);
    ensure_equals("W20 u roll", state.uR.mdV[VX], 0.0);
    ensure_equals("W20 v roll", state.vR.mdV[VX], 0.0);
    ensure_equals("W20 other rotation retained", state.vR.mdV[VY], 5.0);
}

// W21
template<> template<> void weighted_object::test<21>()
{
    auto in = basicFrame(); in.classicIn.autoLeveling = true; in.classicIm.feather = 2.f;
    struct Case { F64 initial; F64 first; F64 second; S32 split; S32 maximumCrossings; };
    const Case cases[] = {
        {.3, .004, .004, 30, 0},
        {.3, -.02, -.02, 30, 1},
        {.3, -.02, .02, 30, 2}
    };
    for (const Case& item : cases)
    {
        LLQuaternion pose;
        pose.setAngleAxis(static_cast<F32>(item.initial), LLVector3(1.f,0.f,0.f));
        S32 crossings = 0;
        S32 injectionChanges = 0;
        S32 previousErrorSign = nonzeroSign(signedRoll(pose));
        S32 previousInjectionSign = 0;
        for (S32 index = 0; index < 60; ++index)
        {
            const F64 injection = index < item.split ? item.first : item.second;
            const S32 injectionSign = nonzeroSign(injection);
            if (previousInjectionSign != 0 && injectionSign != previousInjectionSign)
                ++injectionChanges;
            previousInjectionSign = injectionSign;
            const LLQuaternion injected = ALFlycamWeighted::rotationFromVector(
                LLVector3d(injection,0.,0.)) * pose;
            pose = ALFlycamWeighted::classicLevel(injected, in);
            ensure("W21 leveling contracts injected roll",
                std::fabs(signedRoll(pose)) <= std::fabs(signedRoll(injected)) + 2.e-6);
            const S32 errorSign = nonzeroSign(signedRoll(pose));
            if (previousErrorSign != 0 && errorSign != 0 && errorSign != previousErrorSign)
                ++crossings;
            if (errorSign != 0)
                previousErrorSign = errorSign;
        }
        ensure("W21 specified crossing case", crossings <= item.maximumCrossings);
        ensure("W21 crossing/sign-change theorem", crossings <= 1 + injectionChanges);
    }
}

// W22
template<> template<> void weighted_object::test<22>()
{
    std::array<F32,7> last{}, delta{}; LLVector3 position; LLQuaternion rotation; F32 zoom = 1.f;
    ALFlycamClassic::Input input{}; input.frameIntervalSeconds = 1.f/60.f; input.flycamFeathering = 60.f;
    input.axisScale.fill(1.f); input.axis[0] = -1.f; input.minView = 0.f; input.maxView = 3.f;
    ALFlycamClassic::State classic{last,delta,position,rotation,zoom};
    const auto intermediate = ALFlycamClassic::integrateDeltas(input, classic);
    ALFlycamClassic::finishIntegration(input, intermediate, classic);
    const auto weightedTarget = ALFlycamWeighted::targetFromClassic(delta.data(), LLQuaternion(), 1./60., intermediate.isZero);
    ensure("W22 Classic sign parity", position.mV[VX] * weightedTarget.v.mdV[VX] > 0.);
}

// W23
template<> template<> void weighted_object::test<23>()
{
    ALFlycamWeighted::State state; ALFlycamWeighted::rest(state);
    auto quiet = target(.0019); quiet.inputZero = true;
    const auto held = ALFlycamWeighted::advance(state, quiet, {.2,.1}, .01);
    ensure("W23 below wake threshold", held.held);
    quiet.v.mdV[VX] = .002;
    const auto woke = ALFlycamWeighted::advance(state, quiet, {.2,.1}, .01);
    ensure("W23 threshold wakes", woke.woke);
}

// W24
template<> template<> void weighted_object::test<24>()
{
    ALFlycamWeighted::State state; ALFlycamWeighted::rest(state);
    ensure("W24 zero dt guarded", ALFlycamWeighted::advance(state, target(1.), {.2,.1}, 0.).nonFinite);
    ensure("W24 NaN dt guarded", ALFlycamWeighted::advance(state, target(1.), {.2,.1},
        std::numeric_limits<F64>::quiet_NaN()).nonFinite);
}

// W25
template<> template<> void weighted_object::test<25>()
{
    ALFlycamWeighted::State state; ALFlycamWeighted::rest(state); LLVector3d distance;
    for (S32 index = 0; index < 200000; ++index)
        distance += ALFlycamWeighted::advance(state, target(1.), {.2,.1}, .001).dp;
    close("W25 long-run precision", distance.mdV[VX], 199.6, 2.e-9);
}

// W26
template<> template<> void weighted_object::test<26>()
{
    for (bool blackDragon : {false, true})
    for (bool level : {false, true})
    for (bool automatic : {false, true})
    for (U32 rollDefault : {0U, 1U})
    {
        auto in = basicFrame(); in.classicIn.blackDragon = blackDragon;
        in.classicIn.levelHorizon = level; in.classicIn.autoLeveling = automatic;
        in.classicIn.rollDefault = rollDefault; in.classicIm.blackDragon = blackDragon;
        LLQuaternion q; q.setAngleAxis(.3f, LLVector3(1.f,.2f,0.f));
        std::array<F32,7> delta{};
        delta[3] = .01f; delta[4] = -.02f; delta[5] = .03f;
        delta[6] = in.delta[6];
        std::array<F32,7> last = in.lastDelta; LLVector3 position; LLQuaternion expected = q; F32 zoom = in.zoom;
        ALFlycamClassic::State scratch{last,delta,position,expected,zoom};
        ALFlycamClassic::finishIntegration(in.classicIn, in.classicIm, scratch);
        // OPEN-12 fallback oracle: start the copied leveling block at the
        // exact pre-level pose produced inside Classic. This cross-checks the
        // leveling arithmetic without reintroducing the /fp:fast identity
        // multiply that the fallback exists to avoid.
        LLQuaternion preLevel = q;
        LLVector3 preLevelPosition;
        F32 preLevelZoom = in.zoom;
        std::array<F32,7> preLevelDelta = delta;
        // finishIntegration consumes Level by zeroing the referenced roll
        // delta, so restore the scripted pre-level motion for this oracle.
        preLevelDelta[3] = .01f;
        std::array<F32,7> preLevelLast = in.lastDelta;
        ALFlycamClassic::Input noLevel = in.classicIn;
        noLevel.levelHorizon = false;
        noLevel.autoLeveling = false;
        noLevel.rollDefault = 0;
        ALFlycamClassic::State preLevelState{preLevelLast, preLevelDelta,
            preLevelPosition, preLevel, preLevelZoom};
        ALFlycamClassic::finishIntegration(noLevel, in.classicIm, preLevelState);
        exactQuaternionDiagnostic("W26 fallback is bitwise Classic leveling",
            ALFlycamWeighted::classicLevel(preLevel, in), expected,
            blackDragon, level, automatic, rollDefault);
        if (!level && !automatic && !(blackDragon && rollDefault == 1U))
            ensure("W26 leveling-off identity", exactQuaternion(ALFlycamWeighted::classicLevel(q, in), q));
    }
}

// W27
template<> template<> void weighted_object::test<27>()
{
    for (F64 hz : {60., 144.})
    {
        std::array<F32,7> shadowLast{}, shadowDelta{}, classicLast{}, classicDelta{};
        LLVector3 shadowPosition, classicPosition; LLQuaternion shadowRotation, classicRotation;
        F32 shadowZoom = 1.f, classicZoom = 1.f;
        ALFlycamClassic::Input input{};
        input.frameIntervalSeconds = static_cast<F32>(1. / hz);
        input.flycamFeathering = 8.f; input.axisScale.fill(1.f); input.axis[0] = -1.f;
        input.minView = 0.f; input.maxView = 3.f;
        ALFlycamClassic::State shadow{shadowLast,shadowDelta,shadowPosition,shadowRotation,shadowZoom};
        ALFlycamClassic::State classic{classicLast,classicDelta,classicPosition,classicRotation,classicZoom};
        ALFlycamWeighted::State weightedState; ALFlycamWeighted::rest(weightedState);
        LLVector3 weightedPosition;
        F64 weightedCruise = 0.0;
        F64 classicCruise = 0.0;
        LLVector3 previousClassic;
        for (S32 index = 0; index < static_cast<S32>(hz * 10.); ++index)
        {
            const LLQuaternion entryRotation = shadowRotation;
            const auto shadowIntermediate = ALFlycamClassic::integrateDeltas(input, shadow);
            ALFlycamClassic::finishIntegration(input, shadowIntermediate, shadow);
            const auto classicIntermediate = ALFlycamClassic::integrateDeltas(input, classic);
            ALFlycamClassic::finishIntegration(input, classicIntermediate, classic);
            ensure("W27 identical Classic delta series",
                std::memcmp(shadowDelta.data(), classicDelta.data(), sizeof(F32) * 7) == 0);
            const auto desired = ALFlycamWeighted::targetFromClassic(
                shadowDelta.data(), entryRotation, 1. / hz, shadowIntermediate.isZero);
            // Production narrows every output pose to F32 before the next
            // frame; compare Classic at that same boundary, not an artificial
            // F64 accumulator that retains precision production discards.
            weightedPosition += LLVector3(ALFlycamWeighted::advance(
                weightedState, desired, {0.,0.}, 1. / hz).dp);
            if (index >= static_cast<S32>(hz * 9.))
                classicCruise += static_cast<F64>((classicPosition - previousClassic).length());
            previousClassic = classicPosition;
        }
        close("W27 tau-zero equals Classic",
            static_cast<F64>((weightedPosition - classicPosition).length()), 0., 0.);

        ALFlycamWeighted::State massive; ALFlycamWeighted::rest(massive);
        const auto cruiseTarget = ALFlycamWeighted::targetFromClassic(
            classicDelta.data(), classicRotation, 1. / hz, false);
        for (S32 index = 0; index < static_cast<S32>(hz * 10.); ++index)
        {
            const auto step = ALFlycamWeighted::advance(massive, cruiseTarget, {.2,.12}, 1. / hz);
            if (index >= static_cast<S32>(hz * 9.))
                weightedCruise += magnitude(step.dp);
        }
        const F64 independentCruise = magnitude(cruiseTarget.v);
        close("W27 nonzero-tau cruise ratio", weightedCruise / independentCruise, 1.0, .002);
        ensure("W27 real Classic cruise was nonzero", classicCruise > 0.0);
    }
}

// W28
template<> template<> void weighted_object::test<28>()
{
    auto in = basicFrame(); in.classicIn.autoLeveling = true; in.classicIm.feather = 3.f;
    LLQuaternion pitch; pitch.setAngleAxis(.6f, LLVector3(0.f,1.f,0.f));
    LLQuaternion roll; roll.setAngleAxis(-.12f, LLVector3(1.f,0.f,0.f));
    LLQuaternion pose = roll * pitch;
    ALFlycamWeighted::State coast;
    ALFlycamWeighted::seedVelocity(coast, LLVector3d(), LLVector3d(0.,0.,1.));
    S32 crossings = 0, injectionChanges = 0;
    S32 previousErrorSign = nonzeroSign(signedRoll(pose));
    S32 previousInjectionSign = 0;
    while (!coast.settled)
    {
        const auto step = ALFlycamWeighted::advance(coast, target(0.), {.2,.1}, .01);
        const LLQuaternion injected = ALFlycamWeighted::rotationFromVector(step.dtheta) * pose;
        const F64 injection = signedRoll(injected) - signedRoll(pose);
        const S32 injectionSign = nonzeroSign(injection);
        if (previousInjectionSign != 0 && injectionSign != 0 && injectionSign != previousInjectionSign)
            ++injectionChanges;
        if (injectionSign != 0) previousInjectionSign = injectionSign;
        pose = ALFlycamWeighted::classicLevel(injected, in);
        ensure("W28 yaw-coast contraction",
            std::fabs(signedRoll(pose)) <= std::fabs(signedRoll(injected)) + 2.e-6);
        const S32 errorSign = nonzeroSign(signedRoll(pose));
        if (previousErrorSign != 0 && errorSign != 0 && errorSign != previousErrorSign) ++crossings;
        if (errorSign != 0) previousErrorSign = errorSign;
    }
    ensure("W28 yaw-coast crossing bound", crossings <= 1 + injectionChanges);
}

// W29
template<> template<> void weighted_object::test<29>()
{
    auto in = basicFrame(); in.classicIn.levelHorizon = true;
    LLQuaternion rolled; rolled.setAngleAxis(.7f, LLVector3(1.f,0.f,0.f));
    ensure("W29 instant Level", rotationDistance(ALFlycamWeighted::classicLevel(rolled, in), LLQuaternion()) < 1.e-5);
}

// W30
template<> template<> void weighted_object::test<30>()
{
    auto in = basicFrame(); in.classicIn.blackDragon = true; in.classicIm.blackDragon = true;
    in.classicIn.rollDefault = 1;
    LLQuaternion rolled; rolled.setAngleAxis(.7f, LLVector3(1.f,0.f,0.f));
    ensure("W30 BD roll default", rotationDistance(ALFlycamWeighted::classicLevel(rolled, in), LLQuaternion()) < 1.e-5);
}

// W31
template<> template<> void weighted_object::test<31>()
{
    for (F64 degrees : {0., 30., 60., 85.})
    {
        const F64 pitch = degrees * PI_D / 180.;
        LLQuaternion pitched;
        pitched.setAngleAxis(static_cast<F32>(pitch), LLVector3(0.f,1.f,0.f));
        ALFlycamWeighted::State coast;
        ALFlycamWeighted::seedVelocity(coast, LLVector3d(), LLVector3d(0.,0.,1.));
        F64 measuredTotal = 0.0, expectedTotal = 0.0, yawTotal = 0.0;
        while (!coast.settled)
        {
            const auto step = ALFlycamWeighted::advance(coast, target(0.), {.2,.1}, .01);
            const F64 deltaYaw = step.dtheta.mdV[VZ];
            const LLQuaternion injected = ALFlycamWeighted::rotationFromVector(
                LLVector3d(0.,0.,deltaYaw)) * pitched;
            const F64 measured = signedRoll(injected) - signedRoll(pitched);
            const F64 independentlyExpected = std::asin(std::sin(deltaYaw) * std::sin(pitch));
            close("W31 measured yaw-pitch injection", measured, independentlyExpected, 2.e-6);
            auto classicIn = basicFrame();
            std::array<F32,7> last{}, delta{};
            delta[5] = static_cast<F32>(deltaYaw);
            LLVector3 classicPosition;
            LLQuaternion classicRotation = pitched;
            F32 classicZoom = 1.f;
            ALFlycamClassic::State classicState{
                last,delta,classicPosition,classicRotation,classicZoom};
            ALFlycamClassic::finishIntegration(
                classicIn.classicIn,classicIn.classicIm,classicState);
            close("W31 actual coupling equals Classic", measured,
                signedRoll(classicRotation) - signedRoll(pitched),2.e-6);
            measuredTotal += std::fabs(measured);
            expectedTotal += std::fabs(independentlyExpected);
            yawTotal += std::fabs(deltaYaw);
        }
        ensure("W31 total coast independent asin bound", measuredTotal <= expectedTotal + 2.e-5);
        ensure("W31 total coast analytic bound",
            measuredTotal <= std::fabs(std::sin(pitch)) * yawTotal + 2.e-5);
    }
}

// D0
template<> template<> void weighted_object::test<32>()
{
    const ALFlycamPoseOwner owners[] = {ALFlycamPoseOwner::CLASSIC, ALFlycamPoseOwner::DRONE, ALFlycamPoseOwner::ORBIT};
    for (S32 model = 0; model <= 1; ++model) for (ALFlycamPoseOwner owner : owners)
    for (bool cursor : {false,true}) for (bool orbit : {false,true})
    for (bool tagged : {false,true}) for (bool foreign : {false,true})
    {
        const auto frozen = legacyResolve(model,cursor,orbit,owner,tagged,foreign);
        const auto current = alResolveFlycamFrame(model,cursor,orbit,owner,tagged,foreign);
        ensure("D0 frozen fields", sameLegacyFields(frozen,current));
        ensure("D0 no Weighted overlay", current.weightedEdge == ALFlycamWeightedEdge::NONE &&
            current.frameOwner != ALFlycamPoseOwner::WEIGHTED);
        ensure("D0 glue false", current.frameOwner != ALFlycamPoseOwner::WEIGHTED &&
            current.weightedEdge == ALFlycamWeightedEdge::NONE);
    }
}

// D1
template<> template<> void weighted_object::test<33>()
{
    struct Row
    {
        ALFlycamPoseOwner from;
        S32 model;
        bool orbit;
        bool foreign;
        ALFlycamPoseOwner selected;
        ALFlycamPoseOwner frame;
        ALFlycamOwnerEdge legacyEdge;
        ALFlycamWeightedEdge edge;
        bool resetOperator;
        bool discardDrone;
        bool seedDrone;
    };
    const Row rows[] = {
        {ALFlycamPoseOwner::CLASSIC,2,false,false,ALFlycamPoseOwner::WEIGHTED,ALFlycamPoseOwner::WEIGHTED,
            ALFlycamOwnerEdge::CLASSIC_TO_WEIGHTED,ALFlycamWeightedEdge::C2W,false,false,false},
        {ALFlycamPoseOwner::WEIGHTED,2,false,false,ALFlycamPoseOwner::WEIGHTED,ALFlycamPoseOwner::WEIGHTED,
            ALFlycamOwnerEdge::NONE,ALFlycamWeightedEdge::NONE,false,false,false},
        {ALFlycamPoseOwner::WEIGHTED,0,false,false,ALFlycamPoseOwner::CLASSIC,ALFlycamPoseOwner::CLASSIC,
            ALFlycamOwnerEdge::WEIGHTED_TO_CLASSIC,ALFlycamWeightedEdge::W2C,false,false,false},
        {ALFlycamPoseOwner::WEIGHTED,1,false,false,ALFlycamPoseOwner::DRONE,ALFlycamPoseOwner::WEIGHTED,
            ALFlycamOwnerEdge::CLASSIC_TO_DRONE,ALFlycamWeightedEdge::W2D,true,false,true},
        {ALFlycamPoseOwner::WEIGHTED,1,false,true,ALFlycamPoseOwner::DRONE,ALFlycamPoseOwner::DRONE,
            ALFlycamOwnerEdge::CLASSIC_TO_DRONE,ALFlycamWeightedEdge::W2D,true,false,true},
        {ALFlycamPoseOwner::DRONE,2,false,false,ALFlycamPoseOwner::WEIGHTED,ALFlycamPoseOwner::DRONE,
            ALFlycamOwnerEdge::DRONE_TO_CLASSIC,ALFlycamWeightedEdge::D2W,true,false,false},
        {ALFlycamPoseOwner::WEIGHTED,2,true,false,ALFlycamPoseOwner::ORBIT,ALFlycamPoseOwner::ORBIT,
            ALFlycamOwnerEdge::CLASSIC_TO_ORBIT,ALFlycamWeightedEdge::W2O,false,true,false},
        {ALFlycamPoseOwner::ORBIT,2,false,false,ALFlycamPoseOwner::WEIGHTED,ALFlycamPoseOwner::WEIGHTED,
            ALFlycamOwnerEdge::ORBIT_TO_CLASSIC,ALFlycamWeightedEdge::O2W,false,false,false}};
    for (const Row& row : rows)
    {
        const auto result = alResolveFlycamFrame(
            row.model,false,row.orbit,row.from,true,row.foreign);
        ensure("D1 selected", result.selectedOwner == row.selected);
        ensure("D1 frame", result.frameOwner == row.frame);
        ensure("D1 legacy edge", result.edge == row.legacyEdge);
        ensure("D1 weighted edge", result.weightedEdge == row.edge);
        ensure("D1 reset", result.resetOperator == row.resetOperator);
        ensure("D1 discard", result.discardDroneAccumulator == row.discardDrone);
        ensure("D1 seed", result.seedDrone == row.seedDrone);
        ensure("D1 foreign", result.foreignDroneRegain == row.foreign);
        ensure("D1 classic discard untouched", !result.discardClassicShadowPose);
    }
    for (bool foreign : {false,true})
    {
        const auto stay = alResolveFlycamFrame(2,false,false,
            ALFlycamPoseOwner::WEIGHTED,true,foreign);
        const auto classic = alResolveFlycamFrame(0,false,false,
            ALFlycamPoseOwner::WEIGHTED,true,foreign);
        const auto orbit = alResolveFlycamFrame(2,false,true,
            ALFlycamPoseOwner::WEIGHTED,true,foreign);
        ensure("D1 exhaustive W stay", stay.weightedEdge == ALFlycamWeightedEdge::NONE &&
            stay.frameOwner == ALFlycamPoseOwner::WEIGHTED);
        ensure("D1 exhaustive W2C", classic.weightedEdge == ALFlycamWeightedEdge::W2C &&
            classic.frameOwner == ALFlycamPoseOwner::CLASSIC);
        ensure("D1 exhaustive W2O", orbit.weightedEdge == ALFlycamWeightedEdge::W2O &&
            orbit.frameOwner == ALFlycamPoseOwner::ORBIT);
    }
}

// D2
template<> template<> void weighted_object::test<34>()
{
    for (S32 model : {0,1})
    {
        const auto edge = alResolveFlycamFrame(model,false,false,ALFlycamPoseOwner::WEIGHTED,true,false);
        const auto next = alResolveFlycamFrame(model,false,false,edge.selectedOwner,true,false);
        ensure("D2 one edge", edge.weightedEdge != ALFlycamWeightedEdge::NONE);
        ensure("D2 stored owner leaves W", edge.selectedOwner != ALFlycamPoseOwner::WEIGHTED);
        ensure("D2 glue ends", next.weightedEdge == ALFlycamWeightedEdge::NONE && next.frameOwner != ALFlycamPoseOwner::WEIGHTED);
    }
}

// D3
template<> template<> void weighted_object::test<35>()
{
    const auto result = alResolveFlycamFrame(2,true,false,ALFlycamPoseOwner::WEIGHTED,true,false);
    ensure("D3 cursor W2C", result.weightedEdge == ALFlycamWeightedEdge::W2C && result.frameOwner == ALFlycamPoseOwner::CLASSIC);
}

// D4
template<> template<> void weighted_object::test<36>()
{
    const auto decision = alResolveFlycamFrame(2,false,false,ALFlycamPoseOwner::DRONE,true,true);
    ensure("D4 D2W", decision.weightedEdge == ALFlycamWeightedEdge::D2W);
    ALFlycamWeighted::Coordinator coordinator; auto in = basicFrame();
    in.frameOwner = decision.frameOwner; in.weightedEdge = decision.weightedEdge;
    in.tagged = true; in.foreignSinceWrite = true;
    ensure("D4 pending writes nothing", !coordinator.frame(in).writePose);
    in.frameOwner = ALFlycamPoseOwner::WEIGHTED; in.weightedEdge = ALFlycamWeightedEdge::NONE;
    ensure("D4 pending REST", coordinator.frame(in).writePose && coordinator.state().settled);
}

// D5
template<> template<> void weighted_object::test<37>()
{
    for (ALFlycamPoseOwner owner : {ALFlycamPoseOwner::CLASSIC,ALFlycamPoseOwner::DRONE,ALFlycamPoseOwner::ORBIT})
    {
        const auto bypass = alResolveFlycamFrame(alEffectiveFlycamModel(2,0.f),false,false,owner,true,false);
        const auto frozen = legacyResolve(0,false,false,owner,true,false);
        ensure("D5 bypass vs frozen legacy", sameLegacyFields(bypass,frozen) &&
            bypass.weightedEdge == ALFlycamWeightedEdge::NONE);
    }
    const auto leavingWeighted = alResolveFlycamFrame(
        alEffectiveFlycamModel(2,0.f),false,false,ALFlycamPoseOwner::WEIGHTED,true,false);
    ensure("D5 independent W2C expectation",
        leavingWeighted.selectedOwner == ALFlycamPoseOwner::CLASSIC &&
        leavingWeighted.frameOwner == ALFlycamPoseOwner::CLASSIC &&
        leavingWeighted.edge == ALFlycamOwnerEdge::WEIGHTED_TO_CLASSIC &&
        leavingWeighted.weightedEdge == ALFlycamWeightedEdge::W2C);
    const auto after = alResolveFlycamFrame(0,false,false,
        leavingWeighted.selectedOwner,true,false);
    ensure("D5 post-edge is frozen Classic", sameLegacyFields(
        after, legacyResolve(0,false,false,ALFlycamPoseOwner::CLASSIC,true,false)) &&
        after.weightedEdge == ALFlycamWeightedEdge::NONE);
}

// I1
template<> template<> void weighted_object::test<38>()
{
    ALFlycamWeighted::Coordinator coordinator; auto in = basicFrame(); in.weightedEdge = ALFlycamWeightedEdge::C2W;
    in.delta[0] = .01f; in.classicIm.isZero = false;
    const auto initial = coordinator.frame(in);
    ensure("I1 initial write", initial.writePose);
    in.entryPos = initial.pos; in.entryRot = initial.rot;
    in.weightedEdge = ALFlycamWeightedEdge::NONE;
    in.delta[0] = 0.f; in.classicIm.isZero = true;
    const auto coast = coordinator.frame(in);
    ensure("I1 genuine coast", coast.writePose &&
        static_cast<F64>((coast.pos - initial.pos).length()) > 0.0);
    in.entryPos = coast.pos; in.entryRot = coast.rot;
    in.frameOwner = ALFlycamPoseOwner::CLASSIC; in.weightedEdge = ALFlycamWeightedEdge::W2C;
    std::array<F32,7> classicLast = in.lastDelta;
    std::array<F32,7> classicDelta = in.delta;
    LLVector3 classicPosition = in.entryPos;
    LLQuaternion classicRotation = in.entryRot;
    F32 classicZoom = in.zoom;
    ALFlycamClassic::State classicState{
        classicLast,classicDelta,classicPosition,classicRotation,classicZoom};
    ALFlycamClassic::finishIntegration(in.classicIn,in.classicIm,classicState);
    in.classicPos = classicPosition;
    in.classicRot = classicRotation;
    const auto bypass = coordinator.frame(in);
    LLVector3 presentedPosition = in.classicPos;
    LLQuaternion presentedRotation = in.classicRot;
    if (bypass.writePose)
    {
        presentedPosition = bypass.pos;
        presentedRotation = bypass.rot;
    }
    ensure("I1 W2C pure Classic", !bypass.writePose);
    ensure("I1 W2C position bitwise Classic", std::memcmp(
        presentedPosition.mV,classicPosition.mV,sizeof(classicPosition.mV)) == 0);
    ensure("I1 W2C rotation bitwise Classic", exactQuaternion(
        presentedRotation,classicRotation));
    in.weightedEdge = ALFlycamWeightedEdge::NONE;
    ensure("I1 later Classic ignored", !coordinator.frame(in).writePose);
}

// I2
template<> template<> void weighted_object::test<39>()
{
    std::mt19937 random(7); ALFlycamPoseOwner owner = ALFlycamPoseOwner::CLASSIC;
    for (S32 index = 0; index < 1000; ++index)
    {
        const S32 model = static_cast<S32>(random() & 1U);
        const auto decision = alResolveFlycamFrame(model,(random()&1U)!=0,(random()&1U)!=0,owner,true,false);
        ensure("I2 no FrameIn gate", decision.frameOwner != ALFlycamPoseOwner::WEIGHTED && decision.weightedEdge == ALFlycamWeightedEdge::NONE);
        owner = decision.selectedOwner;
    }
}

// I3
template<> template<> void weighted_object::test<40>()
{
    ALFlycamWeighted::Coordinator coordinator; auto in = basicFrame(); in.weightedEdge = ALFlycamWeightedEdge::C2W;
    in.delta[0] = .01f; in.classicIm.isZero = false;
    auto first = coordinator.frame(in); ensure("I3 first write", first.writePose);
    in.weightedEdge = ALFlycamWeightedEdge::NONE; in.entryPos = first.pos; in.entryRot = first.rot;
    auto second = coordinator.frame(in); ensure("I3 shadow loop accepted", second.writePose && coordinator.state().active);
}

// I4
template<> template<> void weighted_object::test<41>()
{
    ALFlycamWeighted::Coordinator coordinator; auto in = basicFrame(); in.weightedEdge = ALFlycamWeightedEdge::C2W;
    in.delta[0] = .01f; in.classicIm.isZero = false;
    const auto c2w = coordinator.frame(in);
    ensure("I4 C2W seed", c2w.writePose &&
        std::fabs(coordinator.state().vT.mdV[VX] - 1.0) < 2.e-6);
    in.entryPos = c2w.pos; in.entryRot = c2w.rot;
    in.weightedEdge = ALFlycamWeightedEdge::W2D; in.frameOwner = ALFlycamPoseOwner::WEIGHTED;
    const auto w2d = coordinator.frame(in);
    ensure("I4 outgoing W2D", w2d.writePose && !coordinator.state().active);
    close("I4 W2D position continuity", static_cast<F64>((w2d.pos - c2w.pos).length()), .01, 2.e-6);
    in.weightedEdge = ALFlycamWeightedEdge::D2W; in.frameOwner = ALFlycamPoseOwner::DRONE;
    ensure("I4 D2W pending", !coordinator.frame(in).writePose);
    in.weightedEdge = ALFlycamWeightedEdge::NONE; in.frameOwner = ALFlycamPoseOwner::WEIGHTED;
    in.prevPose.valid = in.curPose.valid = true; in.curPose.dt = .01;
    in.prevPose.positionGlobal = LLVector3d(w2d.pos);
    in.curPose.positionGlobal = in.prevPose.positionGlobal + LLVector3d(.02,0.,0.);
    in.prevPose.rotation = in.curPose.rotation = w2d.rot;
    in.entryPos = w2d.pos; in.entryRot = w2d.rot;
    in.delta.fill(0.f); in.classicIm.isZero = true;
    const auto history = coordinator.frame(in);
    ensure("I4 history seed", history.writePose);
    const auto historyTuning = ALFlycamWeighted::tuningForWeight(
        in.settings.weight, in.settings.tauMax, in.settings.curve,
        in.settings.rotationShare);
    const F64 historyRatio = in.frameIntervalSeconds / historyTuning.tauT;
    const F64 expectedHistoryVelocity = 2.0 * std::exp(-historyRatio) *
        (1.0 + historyRatio);
    // Per section 5.4, the history seed is followed by the normal same-frame
    // advance; with a zero Classic target this is the exact cascade result.
    // The tolerance covers F64 exp/multiply rounding in the two evaluations.
    close("I4 history velocity after first weighted frame",
        coordinator.state().vT.mdV[VX], expectedHistoryVelocity, 2.e-12);
    ensure("I4 history position continuous", history.pos.mV[VX] > w2d.pos.mV[VX]);
    in.weightedEdge = ALFlycamWeightedEdge::W2O; in.frameOwner = ALFlycamPoseOwner::ORBIT;
    ensure("I4 W2O", !coordinator.frame(in).writePose);
    in.weightedEdge = ALFlycamWeightedEdge::O2W; in.frameOwner = ALFlycamPoseOwner::WEIGHTED;
    in.entryPos = history.pos; in.entryRot = history.rot;
    in.delta[0] = .005f; in.classicIm.isZero = false;
    const auto o2w = coordinator.frame(in);
    ensure("I4 O2W target seed", o2w.writePose &&
        std::fabs(coordinator.state().vT.mdV[VX] - .5) < 2.e-6);
    close("I4 O2W position continuity",
        static_cast<F64>((o2w.pos - history.pos).length()), .005, 2.e-6);
    ensure("I4 edge telemetry", coordinator.telemetry().edges[0] == 1 &&
        coordinator.telemetry().edges[2] == 1 && coordinator.telemetry().edges[3] == 1 &&
        coordinator.telemetry().edges[4] == 1 && coordinator.telemetry().edges[5] == 1 &&
        coordinator.telemetry().reseedC2W == 1);
}

// I5
template<> template<> void weighted_object::test<42>()
{
    for (S32 mode = 0; mode < 3; ++mode)
    {
        ALFlycamWeighted::Coordinator coordinator; auto in = basicFrame(); in.weightedEdge = ALFlycamWeightedEdge::C2W;
        in.delta[3] = .2f; in.classicIm.isZero = false;
        if (mode == 0) in.classicIn.autoLeveling = true;
        if (mode == 1) in.classicIn.levelHorizon = true;
        if (mode == 2) { in.classicIn.blackDragon = true; in.classicIm.blackDragon = true; in.classicIn.rollDefault = 1; }
        ensure("I5 leveling path", coordinator.frame(in).writePose);
        if (mode != 0) ensure_equals("I5 roll state zero", coordinator.state().vR.mdV[VX], 0.0);
    }
}

// I6
template<> template<> void weighted_object::test<43>()
{
    ALFlycamWeighted::Coordinator coordinator; auto in = basicFrame(); in.weightedEdge = ALFlycamWeightedEdge::C2W;
    in.delta[0] = .01f; in.classicIm.isZero = false; auto out = coordinator.frame(in);
    in.weightedEdge = ALFlycamWeightedEdge::NONE; in.entryPos = out.pos; in.entryRot = out.rot;
    in.delta[0] = 0.f; in.classicIm.isZero = true;
    in.tagged = true; in.dispatchSerial = 9; in.lastWrittenSerial = 2; in.foreignSinceWrite = true;
    coordinator.frame(in); ensure("I6 FOREIGN rests", coordinator.state().settled);
    ensure_equals("I6 FOREIGN telemetry", coordinator.telemetry().reseedForeign, 1U);
}

// I7
template<> template<> void weighted_object::test<44>()
{
    ALFlycamWeighted::Coordinator coordinator; auto in = basicFrame(); in.weightedEdge = ALFlycamWeightedEdge::C2W;
    auto out = coordinator.frame(in); in.weightedEdge = ALFlycamWeightedEdge::NONE; in.entryPos = out.pos; in.entryRot = out.rot;
    in.delta[0] = 0.f; in.classicIm.isZero = true;
    in.tagged = true; in.dispatchSerial = 20; in.lastWrittenSerial = 2;
    coordinator.frame(in); ensure("I7 SELF rests", coordinator.state().settled);
    ensure_equals("I7 SELF telemetry", coordinator.telemetry().reseedSelf, 1U);
}

// I8
template<> template<> void weighted_object::test<45>()
{
    ALFlycamWeighted::Coordinator coordinator; auto in = basicFrame(); in.weightedEdge = ALFlycamWeightedEdge::C2W;
    in.delta.fill(0.f); in.classicIm.isZero = true; coordinator.frame(in);
    ensure("I8 reset replay rests", coordinator.state().settled);
}

// I9
template<> template<> void weighted_object::test<46>()
{
    ALFlycamWeighted::Coordinator coordinator; auto in = basicFrame(); in.weightedEdge = ALFlycamWeightedEdge::C2W;
    auto out = coordinator.frame(in); in.weightedEdge = ALFlycamWeightedEdge::NONE; in.entryPos = out.pos; in.entryRot = out.rot;
    in.originGlobal.set(256.,0.,0.); coordinator.frame(in);
    ensure("I9 origin change rests", coordinator.state().settled);
    ensure_equals("I9 origin telemetry", coordinator.telemetry().reseedOrigin, 1U);
}

// I10
template<> template<> void weighted_object::test<47>()
{
    ALFlycamWeighted::Coordinator internal; auto mismatch = basicFrame();
    mismatch.weightedEdge = ALFlycamWeightedEdge::C2W;
    mismatch.delta[0] = .01f; mismatch.classicIm.isZero = false;
    auto internalOut = internal.frame(mismatch);
    mismatch.weightedEdge = ALFlycamWeightedEdge::NONE;
    mismatch.entryPos = internalOut.pos + LLVector3(.01f,0.f,0.f);
    mismatch.entryRot = internalOut.rot;
    mismatch.delta[0] = 0.f; mismatch.classicIm.isZero = true;
    internal.frame(mismatch);
    ensure("I10 internal mismatch rests", internal.state().settled);
    ensure("I10 internal telemetry detail", internal.telemetry().posecutInternal == 1 &&
        !internal.telemetry().posecutLastPresented &&
        internal.telemetry().posecutLastDistance > .009);

    ALFlycamWeighted::Coordinator coordinator; auto in = basicFrame();
    in.weightedEdge = ALFlycamWeightedEdge::C2W;
    in.presentedComparable = true; auto out = coordinator.frame(in);
    in.weightedEdge = ALFlycamWeightedEdge::NONE; in.entryPos = out.pos; in.entryRot = out.rot;
    in.dispatchSerial = 8; in.lastWrittenSerial = 7;
    in.presentedPos.set(5.f,0.f,0.f); in.settings.guardArmed = false;
    const ALFlycamWeighted::State before = coordinator.state(); coordinator.frame(in);
    ensure("I10 disarmed telemetry only", std::memcmp(&before, &coordinator.state(), sizeof(before)) == 0);
    ensure("I10 presented count/detail", coordinator.telemetry().posecutPresented == 1 &&
        coordinator.telemetry().posecutLastPresented &&
        coordinator.telemetry().posecutLastDistance > 4.9 &&
        coordinator.telemetry().posecutLastSerialDelta == 1);

    ALFlycamWeighted::Coordinator guarded; auto guardedIn = basicFrame();
    guardedIn.weightedEdge = ALFlycamWeightedEdge::C2W;
    guardedIn.delta[0] = .01f; guardedIn.delta[3] = .002f;
    guardedIn.classicIm.isZero = false; guardedIn.presentedComparable = true;
    auto guardedOut = guarded.frame(guardedIn);
    guardedIn.weightedEdge = ALFlycamWeightedEdge::NONE;
    guardedIn.entryPos = guardedOut.pos; guardedIn.entryRot = guardedOut.rot;
    guardedIn.presentedPos = guardedOut.pos; guardedIn.presentedRot = guardedOut.rot;
    guardedIn.delta.fill(0.f); guardedIn.classicIm.isZero = true;
    guardedOut = guarded.frame(guardedIn);
    ensure("I10 armed-rest starts while coasting", !guarded.state().settled &&
        magnitude(guarded.state().vT) > 0.0 && magnitude(guarded.state().vR) > 0.0);
    guardedIn.entryPos = guardedOut.pos; guardedIn.entryRot = guardedOut.rot;
    guardedIn.presentedPos = guardedOut.pos + LLVector3(5.f,0.f,0.f);
    guardedIn.presentedRot = guardedOut.rot;
    guardedIn.settings.guardArmed = true;
    guarded.frame(guardedIn);
    ensure("I10 armed pose cut rests momentum", guarded.guardArmed() &&
        guarded.telemetry().posecutPresented == 1 && guarded.state().settled &&
        magnitude(guarded.state().vT) == 0.0 && magnitude(guarded.state().vR) == 0.0);

    in.settings.guardArmed = true; coordinator.frame(in);
    for (S32 index = 0; index < 11; ++index)
    {
        in.presentedPos.mV[VX] += 1.f;
        coordinator.frame(in);
    }
    ensure("I10 storm auto-disarms", !coordinator.guardArmed());
    const U32 priorNotComparable = coordinator.telemetry().posecutNotComparable;
    in.presentedComparable = false; coordinator.frame(in);
    ensure_equals("I10 n/a verification", coordinator.telemetry().posecutNotComparable,
        priorNotComparable + 1);
    ALFlycamWeighted::FrameOut report;
    for (S32 index = 0; index < 500 && !report.haveTelemetry; ++index)
    {
        const auto sample = coordinator.frame(in);
        if (sample.writePose) { in.entryPos = sample.pos; in.entryRot = sample.rot; }
        if (sample.haveTelemetry) report = sample;
    }
    ensure("I10 telemetry snapshot", report.haveTelemetry &&
        report.telemetry.posecutPresented >= 12 &&
        report.telemetry.posecutNotComparable > 0 &&
        report.telemetry.verdict == ALFlycamWeighted::FrameOut::Telemetry::Verdict::POSECUT_STORM);
    close("I10 telemetry fps", report.telemetry.fps, 100.0, 1.e-9);
    ensure("I10 telemetry settings/edges", report.telemetry.preset == 1 &&
        report.telemetry.tuning.tauT > 0.0 && report.telemetry.edges[0] == 1 &&
        report.telemetry.weight == 50.f);
    ensure("I10 summary details", report.log.find("posecut_last=presented:d=") != std::string::npos &&
        report.log.find("|n/a") != std::string::npos &&
        report.log.find("serial+1") != std::string::npos &&
        report.log.find("preset=Steadicam") != std::string::npos &&
        report.log.find(" tau_t=") != std::string::npos &&
        report.log.find(" fps=") != std::string::npos &&
        report.log.find(" vmax=") != std::string::npos &&
        report.log.find(" wmax=") != std::string::npos &&
        report.log.find(" track_err=") != std::string::npos &&
        report.log.find(" settles=") != std::string::npos &&
        report.log.find(" reseeds=") != std::string::npos &&
        report.log.find(" edges=") != std::string::npos);
}

// I11
template<> template<> void weighted_object::test<48>()
{
    ALFlycamWeighted::Coordinator coordinator; auto in = basicFrame(); in.weightedEdge = ALFlycamWeightedEdge::C2W;
    coordinator.frame(in); in.weightedEdge = ALFlycamWeightedEdge::W2C; in.frameOwner = ALFlycamPoseOwner::CLASSIC; in.classicIn.cursor3D = true;
    ensure_equals("I11 cursor one-shot", coordinator.frame(in).log, std::string("WEIGHTED INACTIVE 3d-cursor"));
    in.classicIn.cursor3D = false; in.weightedEdge = ALFlycamWeightedEdge::O2W; in.frameOwner = ALFlycamPoseOwner::WEIGHTED;
    ensure("I11 orbit return", coordinator.frame(in).writePose);
}

// I12
template<> template<> void weighted_object::test<49>()
{
    ALFlycamWeighted::Coordinator coordinator; auto in = basicFrame(); in.weightedEdge = ALFlycamWeightedEdge::C2W;
    in.delta[0] = .01f; in.delta[1] = -.002f; in.delta[5] = .003f;
    in.classicIm.isZero = false; in.zoom = 2.f;

    struct OracleLag { LLVector3d x1; LLVector3d x2; } translation, rotation;
    const F64 weight = static_cast<F64>(in.settings.weight) / 100.0;
    const F64 curve = static_cast<F64>(in.settings.curve);
    const F64 tauT = static_cast<F64>(in.settings.tauMax) *
        (std::pow(curve, weight) - 1.0) / (curve - 1.0);
    const F64 tauR = static_cast<F64>(in.settings.rotationShare) * tauT;

    auto oracleTarget = [](const ALFlycamWeighted::FrameIn& frame,
                           LLVector3d& velocity, LLVector3d& angularVelocity)
    {
        const F64 dt = frame.frameIntervalSeconds;
        velocity = LLVector3d(LLVector3(frame.delta[0], frame.delta[1], frame.delta[2]) *
            frame.entryRot) * (1.0 / dt);
        angularVelocity.setZero();
        LLQuaternion increment(LLMatrix3(frame.delta[3], frame.delta[4], frame.delta[5]));
        if (increment.mQ[VW] < 0.f) increment = -increment;
        const F64 x = increment.mQ[VX], y = increment.mQ[VY], z = increment.mQ[VZ];
        const F64 imaginaryLength = std::sqrt(x * x + y * y + z * z);
        if (imaginaryLength >= 1.e-12)
        {
            const F64 scale = 2.0 * std::atan2(imaginaryLength,
                static_cast<F64>(increment.mQ[VW])) / (imaginaryLength * dt);
            angularVelocity.set(x * scale, y * scale, z * scale);
        }
    };
    auto oracleStep = [](OracleLag& state, const LLVector3d& target,
                         F64 tau, F64 dt)
    {
        const F64 ratio = dt / tau;
        const F64 e = std::exp(-ratio);
        const F64 b = ratio * e;
        const F64 oneMinusE = -std::expm1(-ratio);
        const LLVector3d x1Error = state.x1 - target;
        const LLVector3d x2Error = state.x2 - target;
        const LLVector3d integral = target * dt + tau *
            (oneMinusE * x2Error + (oneMinusE - b) * x1Error);
        state.x1 = target + e * x1Error;
        state.x2 = target + e * x2Error + b * x1Error;
        return integral;
    };
    auto checkFrame = [&](const char* phase, bool seed)
    {
        LLVector3d velocity, angularVelocity;
        oracleTarget(in, velocity, angularVelocity);
        if (seed)
        {
            translation.x1 = translation.x2 = velocity;
            rotation.x1 = rotation.x2 = angularVelocity;
        }
        const LLVector3d expectedDp = oracleStep(translation, velocity, tauT,
            in.frameIntervalSeconds);
        const LLVector3d expectedDtheta = oracleStep(rotation, angularVelocity, tauR,
            in.frameIntervalSeconds);
        const LLVector3 expectedLocal = LLVector3(expectedDp) * ~in.entryRot;
        const auto out = coordinator.frame(in);
        const F64 f32Tolerance = 8.0 * std::numeric_limits<F32>::epsilon();
        const F64 expected[6] = { expectedLocal.mV[VX], expectedLocal.mV[VY],
            expectedLocal.mV[VZ], expectedDtheta.mdV[VX], expectedDtheta.mdV[VY],
            expectedDtheta.mdV[VZ] };
        for (S32 axis = 0; axis < 6; ++axis)
        {
            std::ostringstream label;
            label << "I12 " << phase << " weighted local axis " << axis;
            // operatorDelta is deliberately narrowed at the production F32 boundary.
            close(label.str().c_str(), out.operatorDelta[axis], expected[axis],
                f32Tolerance * std::max(1.0, std::fabs(expected[axis])));
        }
        in.entryPos = out.pos; in.entryRot = out.rot;
        return out;
    };

    const auto out = checkFrame("seed", true);
    ensure("I12 no zoom channel", out.writePose);
    in.weightedEdge = ALFlycamWeightedEdge::NONE;
    in.delta[0] = -.004f; in.delta[1] = .006f; in.delta[5] = -.001f;
    checkFrame("retarget", false);
    in.delta.fill(0.f); in.classicIm.isZero = true;
    checkFrame("coast", false);
    for (S32 index = 0; index < 1000 && !coordinator.state().settled; ++index)
    {
        const auto coast = coordinator.frame(in);
        if (coast.writePose) { in.entryPos = coast.pos; in.entryRot = coast.rot; }
    }
    ensure("I12 settle telemetry", coordinator.telemetry().settles == 1 &&
        coordinator.telemetry().lastCoastSeconds > 0.0 &&
        coordinator.telemetry().lastCoastTau > 0.0);
}

// I13
template<> template<> void weighted_object::test<50>()
{
    ALFlycamWeighted::Coordinator coordinator; auto in = basicFrame(); in.weightedEdge = ALFlycamWeightedEdge::C2W;
    in.delta[0] = .01f; in.classicIm.isZero = false; in.presentedComparable = true;
    auto out = coordinator.frame(in);
    in.weightedEdge = ALFlycamWeightedEdge::NONE; in.entryPos = out.pos; in.entryRot = out.rot;
    in.presentedPos.set(20.f,0.f,0.f); in.settings.guardArmed = false;
    const ALFlycamWeighted::State before = coordinator.state();
    ensure("I13 KeepsLookAt writes", coordinator.frame(in).writePose);
    ensure("I13 telemetry does not disturb steady momentum",
        std::memcmp(&before, &coordinator.state(), sizeof(before)) == 0);
    ALFlycamWeighted::Coordinator resetCoordinator; auto reset = basicFrame(); reset.weightedEdge = ALFlycamWeightedEdge::C2W;
    ensure("I13 reset replay rests", resetCoordinator.frame(reset).writePose && resetCoordinator.state().settled);
}
}
