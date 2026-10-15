/**
 * @file alsteppedanim_test.cpp
 * @brief Stepped-animation pure-core tests (timing, particle catch-up and R1 gates).
 */
#include "linden_common.h"
#include "llmath.h"
#include "../test/lltut.h"
#include "../alsteppedanim.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

namespace tut
{
namespace
{
void close(const char* label, F64 actual, F64 expected, F64 tolerance)
{
    std::ostringstream message;
    message << label << " expected " << expected << " actual " << actual;
    ensure(message.str(), std::fabs(actual - expected) <= tolerance);
}

bool exactQuaternion(const LLQuaternion& a, const LLQuaternion& b)
{
    return std::memcmp(a.mQ, b.mQ, sizeof(a.mQ)) == 0;
}

ALSteppedAnim::Assignment assignment(F32 rate = 12.f)
{
    ALSteppedAnim::Assignment a;
    a.stepped = true;
    a.rate = rate;
    a.heldClasses = ALSteppedAnim::JC_BODY | ALSteppedAnim::JC_ROOT;
    return a;
}

bool sameTrackedConfig(const ALSteppedAnim::Config& a, const ALSteppedAnim::Config& b)
{
    return a.includeGhosts == b.includeGhosts && a.scope == b.scope && a.mode == b.mode && a.rate == b.rate && a.fastRate == b.fastRate &&
        a.adaptThreshold == b.adaptThreshold && a.adaptHysteresis == b.adaptHysteresis &&
        a.adaptMinDwell == b.adaptMinDwell && a.backgroundRate == b.backgroundRate && a.what == b.what &&
        a.face == b.face && a.gaze == b.gaze && a.flexi == b.flexi && a.attachFX == b.attachFX &&
        a.particles == b.particles && a.textureAnim == b.textureAnim &&
        a.clock == b.clock && a.offset == b.offset && a.phase == b.phase && a.baseFps == b.baseFps &&
        a.boil == b.boil && a.boilAmount == b.boilAmount && a.boilTicks == b.boilTicks &&
        a.impact == b.impact && a.impactTicks == b.impactTicks && a.impactThreshold == b.impactThreshold &&
        a.smear == b.smear && a.smearStrength == b.smearStrength && a.smearThreshold == b.smearThreshold &&
        a.ease == b.ease && a.easeAmount == b.easeAmount && a.matchRoto == b.matchRoto;
}
}

struct stepped_data {};
typedef test_group<stepped_data> stepped_group;
typedef stepped_group::object stepped_object;
stepped_group gSteppedGroup("alsteppedanim");

// T1: analytic grid gives exact twos and threes for an hour, including large/non-grid starts.
template<> template<> void stepped_object::test<1>()
{
    for (F64 start : {0., .37, 100000.})
        for (F64 rate : {12., 8.})
        {
            const S32 expected_gap = rate == 12. ? 2 : 3;
            S64 previous = ALSteppedAnim::tickIndex(start, rate, 0.);
            S32 last_capture = 0;
            for (S32 frame = 1; frame < 24 * 3600; ++frame)
            {
                const S64 tick = ALSteppedAnim::tickIndex(start + frame / 24., rate, 0.);
                if (tick != previous)
                {
                    ensure("T1 capture spacing", frame - last_capture == expected_gap || last_capture == 0);
                    last_capture = frame;
                    previous = tick;
                }
            }
        }
}

// T2: absolute-time clock is independent of render cadence and jitter.
template<> template<> void stepped_object::test<2>()
{
    for (S32 fps : {60, 144})
    {
        F64 t = 0.; S64 previous = 0; S32 captures = 0;
        F64 last_capture = 0.;
        for (S32 frame = 0; t < 10.; ++frame)
        {
            const F64 jitter = 1. + .2 * std::sin(frame * .731);
            t += jitter / fps;
            const S64 tick = ALSteppedAnim::tickIndex(t, 12., 0.);
            if (tick != previous)
            {
                if (captures)
                    ensure("T2 capture gap", t - last_capture <= 1. / 12. + 1.2 / fps + 1.e-9);
                ++captures; previous = tick; last_capture = t;
            }
        }
        ensure("T2 capture count", captures >= 119 && captures <= 121);
    }
}

// T3: no accumulated-dt drift; capture sample is the current smooth scalar bit-for-bit.
template<> template<> void stepped_object::test<3>()
{
    ALSteppedAnim::ActorState state;
    ALSteppedAnim::Config config; config.enabled = true;
    F64 t = 0., held = -1.;
    for (S32 frame = 0; frame < 60 * 3600; ++frame)
    {
        t += (1. + .15 * std::sin(frame * .13)) / 60.;
        const auto decision = ALSteppedAnim::planFrame(state, assignment(), config, t);
        ensure_equals("T3 analytic tick", decision.tick, ALSteppedAnim::tickIndex(t, 12., 0.));
        if (decision.capture) { held = t; ensure("T3 sample equality", std::memcmp(&held, &t, sizeof(t)) == 0); }
    }
}

// T4: random, locked, manual and Roto phase rules.
template<> template<> void stepped_object::test<4>()
{
    ALSteppedAnim::Config c;
    const F64 a = ALSteppedAnim::actorPhase(c, 42, 0.);
    ensure_equals("T4 deterministic", a, ALSteppedAnim::actorPhase(c, 42, 0.));
    ensure("T4 spread", a != ALSteppedAnim::actorPhase(c, 43, 0.) && a >= 0. && a < 1.);
    c.offset = ALSteppedAnim::OFFSET_LOCKED;
    ensure_equals("T4 locked", ALSteppedAnim::actorPhase(c, 42, 0.), 0.);
    c.offset = ALSteppedAnim::OFFSET_MANUAL; c.phase = .35f;
    close("T4 manual", ALSteppedAnim::actorPhase(c, 42, 0.), .35, 1.e-6);
    close("T4 roto", ALSteppedAnim::actorPhase(c, 42, .8), .15, 1.e-6);
}

// T5: strict adaptive hysteresis, dwell, forced capture and smooth-fast behavior.
template<> template<> void stepped_object::test<5>()
{
    ALSteppedAnim::Config c; c.enabled = true; c.mode = ALSteppedAnim::MODE_ADAPTIVE;
    c.adaptThreshold = 1.5f; c.adaptHysteresis = .05f; c.adaptMinDwell = .05f;
    ALSteppedAnim::ActorState s; s.stateSince = 0.;
    ALSteppedAnim::LimbFrame f; f.t = .2; s.limbs.mSmoothedSpeed = 1.5f;
    ALSteppedAnim::observe(s, c, f); ensure("T5 equality stays slow", !s.fast);
    f.t = .3; for (LLVector3& p : f.p) p.set(.5f, 0.f, 0.f);
    s.limbs.mSmoothedSpeed = 1.51f; ALSteppedAnim::observe(s, c, f);
    ensure("T5 enters fast", s.fast && s.forceCapture);
    auto a = assignment(); a.adaptive = true; a.fastRate = 24.f;
    ensure("T5 fast forces capture", ALSteppedAnim::planFrame(s, a, c, .31).capture);
    for (S32 i = 0; i < 10000; ++i)
    {
        f.t += 1. / 144.; s.limbs.mCount = 0; s.limbs.mSmoothedSpeed = 1.5f;
        ALSteppedAnim::observe(s, c, f);
    }
    ensure("T5 threshold equality cannot toggle", s.fast);
    for (S32 i = 0; i < 100; ++i)
    {
        f.t += .1; s.limbs.mCount = 0;
        s.limbs.mSmoothedSpeed = (i & 1) ? c.adaptThreshold : c.adaptThreshold * .98f;
        ALSteppedAnim::observe(s, c, f);
    }
    ensure("T5 hysteresis-band dither cannot toggle", s.fast);
    ALSteppedAnim::ActorState blocked; ALSteppedAnim::LimbFrame bf; bf.t = .04;
    blocked.limbs.mSmoothedSpeed = 2.f; ALSteppedAnim::observe(blocked, c, bf);
    ensure("T5 one-grid dwell blocks early switch", !blocked.fast);
    bf.t = .09; blocked.limbs.mCount = 0; blocked.limbs.mSmoothedSpeed = 2.f;
    ALSteppedAnim::observe(blocked, c, bf); ensure("T5 dwell eventually permits switch", blocked.fast);
    ALSteppedAnim::Config raw; raw.adaptHysteresis = 0.f; raw.adaptMinDwell = 0.f;
    const auto clean = ALSteppedAnim::sanitize(raw);
    close("T5 hysteresis floor", clean.adaptHysteresis, .05, 1.e-6);
    close("T5 dwell floor", clean.adaptMinDwell, .05, 1.e-6);
    a.fastRate = 1.9f; s.fast = true;
    const auto smooth = ALSteppedAnim::planFrame(s, a, c, f.t + .1);
    ensure("T5 smooth captures each frame", smooth.capture && !smooth.hold);
}

// T6: boil is deterministic, bounded and indexed only from the base grid.
template<> template<> void stepped_object::test<6>()
{
    const LLQuaternion first = ALSteppedAnim::boilRotation(7, 3, 11, .6f);
    ensure("T6 deterministic bits", exactQuaternion(first, ALSteppedAnim::boilRotation(7, 3, 11, .6f)));
    ensure("T6 seed differs", !exactQuaternion(first, ALSteppedAnim::boilRotation(8, 3, 11, .6f)));
    for (U32 i = 0; i < 100000; ++i)
    {
        const LLQuaternion q = ALSteppedAnim::boilRotation(i, i / 2, i % 160, .6f);
        // 2*atan2(|xyz|, |w|) is accurate for small angles; acos(w) near w=1 loses
        // ~5e-4 rad in single precision, far above the bound's 1e-5 tolerance.
        const F32 vec_len = std::sqrt(q.mQ[VX] * q.mQ[VX] + q.mQ[VY] * q.mQ[VY] + q.mQ[VZ] * q.mQ[VZ]);
        const F32 angle = 2.f * std::atan2(vec_len, std::fabs(q.mQ[VW]));
        ensure("T6 bounded angle", angle <= .6f * DEG_TO_RAD + 1.e-5f);
    }
    ALSteppedAnim::ActorState s; ALSteppedAnim::Config c; c.enabled = true; c.boilTicks = 2;
    auto a = assignment(); a.adaptive = true; a.fastRate = 24.f; s.fast = true;
    ensure_equals("T6 base boil index", ALSteppedAnim::planFrame(s, a, c, 1.).boilIndex, 6);
}

// T7: impact captures next frame, suppresses exactly N boundaries, then cools down.
template<> template<> void stepped_object::test<7>()
{
    ALSteppedAnim::Config c; c.enabled = true; c.impact = true; c.impactTicks = 3;
    ALSteppedAnim::ActorState spike;
    for (S32 sample_index = 0; sample_index < 3; ++sample_index)
    {
        ALSteppedAnim::LimbFrame sample; sample.t = sample_index * .04;
        const F32 x = sample_index == 0 ? 0.f : .16f;
        for (LLVector3& p : sample.p) p.set(x, 0.f, 0.f);
        ALSteppedAnim::observe(spike, c, sample);
    }
    ensure("T7 4m/s stop triggers", spike.impactPending);
    ALSteppedAnim::ActorState s;
    ALSteppedAnim::planFrame(s, assignment(), c, 0.);
    s.impactPending = true;
    ensure("T7 impact capture", ALSteppedAnim::planFrame(s, assignment(), c, .01).capture);
    for (S32 i = 1; i <= 3; ++i)
        ensure("T7 suppressed boundary", !ALSteppedAnim::planFrame(s, assignment(), c, i / 12.).capture);
    ensure("T7 release capture", ALSteppedAnim::planFrame(s, assignment(), c, 4. / 12.).capture);
    ensure("T7 cooldown released", !s.impactCooldown);
    ALSteppedAnim::ActorState gentle;
    gentle.limbs.mCount = 1; gentle.limbs.mFrames[0].t = 1.;
    gentle.limbs.mPreviousVelocity[0] = 4.f; gentle.limbs.mCurrentVelocity[0] = 3.4f;
    gentle.limbs.mVelocityDt = .1;
    ALSteppedAnim::LimbFrame same; same.t = 1.; ALSteppedAnim::observe(gentle, c, same);
    ensure("T7 gentle deceleration ignored", !gentle.impactPending);
}

// T8: smear guards, lag, whole-chain clamp, monotonicity, direction and zero strength.
template<> template<> void stepped_object::test<8>()
{
    for (U32 n : {1u, 2u, 3u})
    {
        const auto smear = ALSteppedAnim::smearFor(.5f, .1f, .5f, .1f, n, 1.f, 2.f);
        ensure("T8 active", smear.active);
        close("T8 lag", smear.lagT, .5, 1.e-6);
        ensure("T8 chain clamp", std::pow(smear.stretch, static_cast<F32>(n)) <= 1.60001f);
    }
    const auto weak = ALSteppedAnim::smearFor(.1f, .1f, .5f, .1f, 2, 1.f, .5f);
    const auto strong = ALSteppedAnim::smearFor(.5f, .1f, .5f, .1f, 2, 1.f, .5f);
    ensure("T8 monotonic", strong.stretch >= weak.stretch);
    const LLVector3 axes = ALSteppedAnim::smearAxisScale(1.4f, LLVector3::x_axis);
    close("T8 along", axes[VX], 1.4, 1.e-6); close("T8 orthogonal", axes[VY], 1., 1.e-6);
    ensure("T8 zero bone guard", !ALSteppedAnim::smearFor(.5f, .1f, .5f, 0.f, 2, 1.f, 1.f).active);
    ensure("T8 chain guard", !ALSteppedAnim::smearFor(.5f, .1f, .009f, .006f, 2, 1.f, 1.f).active);
    ensure("T8 displacement guard", !ALSteppedAnim::smearFor(.00001f, .1f, .5f, .1f, 2, 1.f, 1.f).active);
    ensure("T8 dt guard", !ALSteppedAnim::smearFor(.5f, 0.f, .5f, .1f, 2, 1.f, 1.f).active);
    const auto identity = ALSteppedAnim::smearFor(.5f, .1f, .5f, .1f, 2, 0.f, 1.f);
    ensure("T8 strength zero identity", !identity.active && identity.lagT == 1.f && identity.stretch == 1.f);
    const auto below = ALSteppedAnim::smearFor(.1f, .1f, .5f, .1f, 2, 1.f, 2.f);
    ensure("T8 below threshold identity", !below.active && below.lagT == 1.f && below.stretch == 1.f);
    ensure("T8 invalidated history cannot smear", !ALSteppedAnim::smearHistoryReady(false, 160, 160));
    ensure("T8 matching valid history can smear", ALSteppedAnim::smearHistoryReady(true, 160, 160));
}

// T9: ease identity, bounds and fixed sub-step speed selection.
template<> template<> void stepped_object::test<9>()
{
    ensure_equals("T9 zero identity", ALSteppedAnim::easeWeight(0.f, 1.f, 4.f), 0.f);
    ensure("T9 bounded", ALSteppedAnim::easeWeight(.4f, 1.f, 4.f) <= .4f);
    auto sample = [](S32 fps)
    {
        ALSteppedAnim::LimbTracker tracker;
        F64 t = 0.; S32 frame = 0;
        while (t <= 1. / 12. + 1.2 / fps)
        {
            ALSteppedAnim::LimbFrame f; f.t = t;
            for (LLVector3& p : f.p) p.clear();
            tracker.push(f);
            t += (1. + .2 * std::sin(frame++ * .731)) / fps;
        }
        return tracker;
    };
    const ALSteppedAnim::LimbTracker sixty = sample(60), one_forty_four = sample(144);
    auto choice = [](const ALSteppedAnim::LimbTracker& tracker)
    {
        S32 selected = 0; F32 minimum = std::numeric_limits<F32>::max();
        for (S32 sub = 0; sub < 4; ++sub)
        {
            const F32 speed = ALSteppedAnim::speedAt(tracker, sub / 48.);
            if (speed < minimum) { minimum = speed; selected = sub; }
        }
        const F32 capture = ALSteppedAnim::speedAt(tracker, 1. / 12.);
        return std::pair<S32, F32>(selected, ALSteppedAnim::easeWeight(.7f, minimum, capture));
    };
    const auto c60 = choice(sixty), c144 = choice(one_forty_four);
    ensure_equals("T9 cadence-independent candidate", c60.first, c144.first);
    close("T9 cadence-independent weight", c60.second, c144.second, 1.e-4);
    ALSteppedAnim::ActorState state; state.subSeen[0] = true; state.candidateSub = 0;
    state = ALSteppedAnim::ActorState(); ensure("T9 window reset", state.candidateSub == -1 && !state.subSeen[0]);

    ALSteppedAnim::LimbTracker ramp;
    for (S32 i = 0; i < 3; ++i)
    {
        ALSteppedAnim::LimbFrame f; f.t = static_cast<F64>(i);
        for (LLVector3& p : f.p) p.clear();
        f.p[0].set(i == 0 ? 0.f : (i == 1 ? 1.f : 4.f), 0.f, 0.f);
        ramp.push(f);
    }
    close("T9 linear speed interpolation", ALSteppedAnim::speedAt(ramp, 1.5), 2., 1.e-6);
}

// T10 and unit-testable R1: scope matrix, override precedence, exclusions and disabled no-op.
template<> template<> void stepped_object::test<10>()
{
    ALSteppedAnim::Config c; c.enabled = true;
    ALSteppedAnim::ActorFacts f; f.drawn = true; f.inCast = true;
    ensure("T10 cast", ALSteppedAnim::resolveActor(c, f, 0.).stepped);
    f.inCast = false;
    ensure("T10 cast excludes outsider", !ALSteppedAnim::resolveActor(c, f, 0.).stepped);
    c.scope = ALSteppedAnim::SCOPE_VIEW;
    ensure("T10 view includes drawn outsider", ALSteppedAnim::resolveActor(c, f, 0.).stepped);
    c.mode = ALSteppedAnim::MODE_ADAPTIVE;
    const auto adaptive = ALSteppedAnim::resolveActor(c, f, 0.);
    ensure("T10 adaptive view uses rate", adaptive.stepped && adaptive.adaptive && adaptive.rate == c.rate);
    c.mode = ALSteppedAnim::MODE_FIXED; f.inCast = true;
    c.scope = ALSteppedAnim::SCOPE_SELECTED; ensure("T10 selected excludes", !ALSteppedAnim::resolveActor(c, f, 0.).stepped);
    f.isSelected = true; ensure("T10 selected includes", ALSteppedAnim::resolveActor(c, f, 0.).stepped);
    f.ov.mode = 1; ensure("T10 never wins", !ALSteppedAnim::resolveActor(c, f, 0.).stepped);
    f.ov.mode = 2; f.isSelected = false; ensure("T10 own wins", ALSteppedAnim::resolveActor(c, f, 0.).stepped);
    f.posing = true; ensure("T10 poser exclusion", !ALSteppedAnim::resolveActor(c, f, 0.).stepped);
    f.posing = false; f.cameraMounted = true; ensure("T10 camera exclusion", !ALSteppedAnim::resolveActor(c, f, 0.).stepped);
    c.excludeCameraMounted = false; ensure("T10 camera exclusion can be disabled", ALSteppedAnim::resolveActor(c, f, 0.).stepped);
    f.cameraMounted = false; f.excludedAlways = true; ensure("T10 hard exclusion", !ALSteppedAnim::resolveActor(c, f, 0.).stepped);
    f.excludedAlways = false; f.drawn = false; ensure("T10 not-drawn exclusion", !ALSteppedAnim::resolveActor(c, f, 0.).stepped);
    f.drawn = true;
    c.enabled = false; f.cameraMounted = false; ensure("R1 disabled no assignment", !ALSteppedAnim::resolveActor(c, f, 0.).stepped);

    c.enabled = true; f.drawn = true;
    f.ov.mode = 1; ensure("T10 Never ghost cannot inherit", !ALSteppedAnim::ghostMayInherit(c, f));
    f.ov.mode = 0; f.posing = true; ensure("T10 posed ghost cannot inherit", !ALSteppedAnim::ghostMayInherit(c, f));
    f.posing = false; f.cameraMounted = true; c.excludeCameraMounted = true;
    ensure("T10 camera ghost cannot inherit", !ALSteppedAnim::ghostMayInherit(c, f));
    f.cameraMounted = false; ensure("T10 eligible ghost can inherit", ALSteppedAnim::ghostMayInherit(c, f));

    ALSteppedAnim::Assignment wearer;
    wearer.stepped = true; wearer.rate = 8.f; wearer.phase = .375;
    wearer.adaptive = true; wearer.fastRate = 24.f;
    wearer.heldClasses = ALSteppedAnim::JC_ROOT | ALSteppedAnim::JC_BODY | ALSteppedAnim::JC_FACE;
    ALSteppedAnim::Assignment attached;
    U64 attached_seed = 91;
    ensure("T10 attached animesh inherits a stepped wearer",
        ALSteppedAnim::inheritAttachedAssignment(wearer, 42, attached, attached_seed));
    ensure("T10 attached animesh inherits assignment, seed and phase",
        attached.stepped && attached.rate == wearer.rate && attached.phase == wearer.phase &&
        attached.heldClasses == (wearer.heldClasses & ~ALSteppedAnim::JC_ROOT) && attached_seed == 42);
    ensure("T10 attached animesh never holds its own root",
        !(attached.heldClasses & ALSteppedAnim::JC_ROOT));

    ALSteppedAnim::ActorState wearer_state, attached_state;
    wearer_state.hasCapture = true;
    wearer_state.fast = true;
    wearer_state.captureTick = 73;
    wearer_state.captureRate = 24.;
    wearer_state.candidateSub = 2;
    ALSteppedAnim::FrameDecision wearer_decision, attached_decision;
    wearer_decision.capture = true;
    wearer_decision.hold = true;
    wearer_decision.tick = 74;
    wearer_decision.boilIndex = 19;
    wearer_decision.rate = 24.f;
    ALSteppedAnim::inheritAttachedFrame(wearer, 42, wearer_state, wearer_decision,
        attached, attached_seed, attached_state, attached_decision);
    ensure("T10 attached animesh inherits the exact per-frame decision",
        attached_decision.capture == wearer_decision.capture &&
        attached_decision.hold == wearer_decision.hold &&
        attached_decision.tick == wearer_decision.tick &&
        attached_decision.boilIndex == wearer_decision.boilIndex &&
        attached_decision.rate == wearer_decision.rate &&
        attached_state.fast == wearer_state.fast &&
        attached_state.captureTick == wearer_state.captureTick &&
        attached_state.captureRate == wearer_state.captureRate &&
        attached_state.candidateSub == wearer_state.candidateSub &&
        attached_seed == 42);
    wearer.stepped = false;
    ensure("T10 smooth wearer is not inherited",
        !ALSteppedAnim::inheritAttachedAssignment(wearer, 7, attached, attached_seed));

    c = ALSteppedAnim::Config(); c.enabled = true; c.mode = ALSteppedAnim::MODE_MIXED;
    f = ALSteppedAnim::ActorFacts(); f.drawn = true; f.inCast = true;
    c.scope = ALSteppedAnim::SCOPE_SELECTED;
    auto mixed = ALSteppedAnim::resolveActor(c, f, 0.);
    ensure("T10 mixed selected includes cast background", mixed.stepped && mixed.rate == c.backgroundRate);
    f.isSelected = true; mixed = ALSteppedAnim::resolveActor(c, f, 0.);
    ensure("T10 mixed selected target rate", mixed.stepped && mixed.rate == c.rate);
    c.scope = ALSteppedAnim::SCOPE_CAST; f.inCast = false;
    mixed = ALSteppedAnim::resolveActor(c, f, 0.);
    ensure("T10 mixed selected outside cast", mixed.stepped && mixed.rate == c.rate);
    f.isSelected = false; ensure("T10 mixed cast excludes outsider", !ALSteppedAnim::resolveActor(c, f, 0.).stepped);
    c.scope = ALSteppedAnim::SCOPE_VIEW;
    mixed = ALSteppedAnim::resolveActor(c, f, 0.);
    ensure("T10 mixed view background", mixed.stepped && mixed.rate == c.backgroundRate);
    f.ov.role = 1; mixed = ALSteppedAnim::resolveActor(c, f, 0.);
    ensure("T10 forced target role", mixed.rate == c.rate);
    f.ov.role = 2; f.isSelected = true; mixed = ALSteppedAnim::resolveActor(c, f, 0.);
    ensure("T10 forced background role", mixed.rate == c.backgroundRate);

    c.mode = ALSteppedAnim::MODE_FIXED; c.scope = ALSteppedAnim::SCOPE_VIEW; c.maxAvatars = 3;
    std::vector<ALSteppedAnim::ActorFacts> cap_facts(5);
    std::vector<ALSteppedAnim::Assignment> cap_assignments(5, assignment());
    for (auto& fact : cap_facts) fact.drawn = true;
    cap_facts[0].cameraDistance = 1.f;
    cap_facts[1].inCast = true; cap_facts[1].castOrder = 1;
    cap_facts[2].ov.mode = 2;
    cap_facts[3].isSelected = true;
    cap_facts[4].inCast = true; cap_facts[4].castOrder = 0;
    ALSteppedAnim::applyActorCap(c, cap_facts, cap_assignments);
    ensure("T10 cap keeps selected own and first cast", cap_assignments[3].stepped &&
        cap_assignments[2].stepped && cap_assignments[4].stepped);
    ensure("T10 cap drops lower priorities", !cap_assignments[0].stepped && !cap_assignments[1].stepped);
}

// T11 and R1d: joint classes, render composition, owned/foreign/epoch restore.
template<> template<> void stepped_object::test<11>()
{
    using namespace ALSteppedAnim;
    // Parent paths and names are a direct fixture from character/avatar_skeleton.xml;
    // mRoot is the viewer-owned transform that sits above that file's mPelvis root.
    const std::vector<JointDesc> desc = {{"mRoot", -1, false}, {"mPelvis", 0, false},
        {"mSpine1", 1, false}, {"mSpine2", 2, false}, {"mTorso", 3, false},
        {"mSpine3", 4, false}, {"mSpine4", 5, false}, {"mChest", 6, false},
        {"mNeck", 7, false}, {"mHead", 8, false}, {"mEyeLeft", 9, false},
        {"mEyeRight", 9, false}, {"mFaceRoot", 9, false},
        {"mFaceJaw", 12, false}, {"mFaceTeethLower", 13, false},
        {"mFaceLipLowerLeft", 14, false}, {"mFaceEyeAltRight", 12, false},
        {"mFaceEyeAltLeft", 12, false}, {"PELVIS", 1, true}, {"HEAD", 9, true}};
    const auto classes = classifyJoints(desc);
    ensure_equals("T11 root", classes[0], static_cast<U8>(JC_ROOT));
    ensure_equals("T11 nested face", classes[15], static_cast<U8>(JC_FACE));
    ensure_equals("T11 gaze", classes[10], static_cast<U8>(JC_GAZE));
    ensure_equals("T11 alt gaze", classes[16], static_cast<U8>(JC_GAZE));
    ensure_equals("T11 phys", classes[18], static_cast<U8>(JC_PHYS));
    PoseBuffer smooth, held, rendered; smooth.joints.resize(desc.size()); held = smooth;
    smooth.joints[1].pos.set(1.f, 0.f, 0.f); held.joints[1].pos.set(2.f, 0.f, 0.f);
    smooth.joints[15].pos.set(3.f, 0.f, 0.f); held.joints[15].pos.set(4.f, 0.f, 0.f);
    composeRender(smooth, held, classes, JC_BODY, rendered);
    ensure_equals("T11 held class", rendered.joints[1].pos[VX], 2.f);
    ensure_equals("T11 smooth class", rendered.joints[15].pos[VX], 3.f);
    JointLocal written = held.joints[1], current = written;
    ensure("T11 owned restores", bitEqual(restoreValue(current, written, smooth.joints[1], false), smooth.joints[1]));
    current.pos[VX] = 9.f;
    ensure_equals("T11 foreign stays", restoreValue(current, written, smooth.joints[1], false).pos[VX], 9.f);
    current = written;
    JointLocal restored = restoreValue(current, written, smooth.joints[1], true);
    ensure("R1d epoch restores untouched position/scale", bitEqual(restored, smooth.joints[1]));
    current.pos[VX] = 9.f; current.scale[VY] = 7.f;
    restored = restoreValue(current, written, smooth.joints[1], true);
    ensure("R1d epoch keeps foreign position and scale",
        restored.pos[VX] == 9.f && restored.scale[VY] == 7.f);

    ensure_equals("R1 palette targets about-to-render frame",
        overlayRenderFrame(41), U32(42));
    ensure("R3 idle-built palette is invalidated even when the step pose needs no writes",
        paletteNeedsOverlayRebuild(42, 41));
    ensure("R3 prior rendered palette is not mistaken for an idle-built current palette",
        !paletteNeedsOverlayRebuild(41, 41));
    ensure("R1 palette preserves contiguous rendered hold",
        preservePreviousOverlayPalette(41, 42, 42));
    ensure("R1 palette does not preserve stale frame",
        !preservePreviousOverlayPalette(40, 42, 42));
    ensure("R1 restore recognizes rendered overlay",
        paletteContainsOverlay(42, 42, 42));
    ensure("R1 prior-frame smooth palette is forced to rebuild for overlay",
        paletteReadyForOverlay(41, 42, 42, 8, 9));
    ensure("R1 held palette is accepted only at the current pose epoch",
        paletteReadyForOverlay(42, 42, 42, 9, 9));
    ensure("R1 same-frame smooth palette cannot be reused by overlay",
        !paletteReadyForOverlay(42, 0xFFFFFFFF, 42, 8, 9));
    ensure("R1 stale held palette epoch cannot be reused by overlay",
        !paletteReadyForOverlay(42, 42, 42, 8, 9));
    ensure("R1 all-zero write vector is empty bookkeeping",
        !hasOverlayWrites(std::vector<U8>(160, 0), 0));
    ensure("R1 root write is non-empty bookkeeping",
        hasOverlayWrites(std::vector<U8>(), 2));
    ensure("R3 no-write capture still needs the about-to-render palette boundary",
        !paletteReadyForOverlay(42, 0xFFFFFFFF, 42, 9, 9));
    ensure("R3 tagged no-write capture retains a valid real-motion palette",
        paletteReadyForOverlay(42, 42, 42, 9, 9));
}

// Ghost clones independently join every scope, but retain all safety gates.
template<> template<> void stepped_object::test<17>()
{
    ALSteppedAnim::Config c;
    c.enabled = true;
    ALSteppedAnim::ActorFacts ghost;
    ghost.drawn = true;
    ghost.ghostOwnAnimation = true;
    ensure("ghost option defaults off", !c.includeGhosts);
    ensure("off keeps outsider smooth", !ALSteppedAnim::resolveActor(c, ghost, 0.).stepped);
    ensure("off preserves prior inheritance", ALSteppedAnim::ghostMayInherit(c, ghost));
    c.includeGhosts = true;
    ensure("sanitize preserves ghost option", ALSteppedAnim::sanitize(c).includeGhosts);
    for (S32 scope : {ALSteppedAnim::SCOPE_SELECTED, ALSteppedAnim::SCOPE_CAST, ALSteppedAnim::SCOPE_VIEW})
        for (S32 mode : {ALSteppedAnim::MODE_FIXED, ALSteppedAnim::MODE_ADAPTIVE, ALSteppedAnim::MODE_MIXED})
        {
            c.scope = scope; c.mode = mode;
            ensure("own-animation clone joins every scope/mode", ALSteppedAnim::resolveActor(c, ghost, 0.).stepped);
        }
    ensure("own animation keeps its own assignment", !ALSteppedAnim::ghostMayInherit(c, ghost));
    ghost.ov.mode = 1;
    ensure("Smooth override wins", !ALSteppedAnim::resolveActor(c, ghost, 0.).stepped);
    ghost.ov.mode = 0; ghost.posing = true;
    ensure("posing/edit exclusion wins", !ALSteppedAnim::resolveActor(c, ghost, 0.).stepped);
    ghost.posing = false; ghost.cameraMounted = true;
    ensure("camera exclusion wins", !ALSteppedAnim::resolveActor(c, ghost, 0.).stepped);
    ghost.cameraMounted = false; ghost.drawn = false;
    ensure("visibility exclusion wins", !ALSteppedAnim::resolveActor(c, ghost, 0.).stepped);
    ghost.drawn = true; c.enabled = false;
    ensure("master off wins", !ALSteppedAnim::resolveActor(c, ghost, 0.).stepped);
    c.enabled = true; c.maxAvatars = 1;
    ALSteppedAnim::ActorFacts selected;
    selected.drawn = true; selected.isSelected = true;
    const std::vector<ALSteppedAnim::ActorFacts> facts = {selected, ghost};
    std::vector<ALSteppedAnim::Assignment> assignments;
    for (const auto& f : facts) assignments.push_back(ALSteppedAnim::resolveActor(c, f, 0.));
    ALSteppedAnim::applyActorCap(c, facts, assignments);
    ensure("ghost remains subject to actor cap", assignments[0].stepped && !assignments[1].stepped);
    ghost.ghostOwnAnimation = false;
    c.scope = ALSteppedAnim::SCOPE_CAST; c.mode = ALSteppedAnim::MODE_FIXED;
    ensure("mirror/copy ghost still inherits", ALSteppedAnim::ghostMayInherit(c, ghost));
    ensure("ordinary actors do not bypass scope", !ALSteppedAnim::resolveActor(c, ghost, 0.).stepped);
}

// T12: preset completeness/config clamps/effective rates/Roto mapping.
template<> template<> void stepped_object::test<12>()
{
    const auto& presets = ALSteppedAnim::builtInPresets();
    ensure_equals("T12 preset count", presets.size(), size_t(14));
    ensure_equals("T12 tracked count", ALSteppedAnim::trackedSettings().size(), size_t(39));
    std::set<std::string> ids;
    for (const auto& p : presets)
    {
        ensure("T12 unique id", ids.insert(p.id).second);

        const auto clean = ALSteppedAnim::sanitize(p.values);
        ensure("T12 complete preset survives sanitize", sameTrackedConfig(clean, p.values));
    }
    const auto& preset_names = ALSteppedAnim::presetSettings();
    const std::set<std::string> preset_settings(preset_names.begin(), preset_names.end());
    ensure("T12 built-in and saved preset whitelist excludes independent options",
        preset_settings.size() == 29 && !preset_settings.count("StepAnimParticles") &&
        !preset_settings.count("StepAnimTextureAnim") && !preset_settings.count("StepAnimIncludeGhosts"));
    ensure("T12 retired StepAnimGroupRates is no longer tracked (group rates are scene data)",
        std::find(ALSteppedAnim::trackedSettings().begin(), ALSteppedAnim::trackedSettings().end(),
                  "StepAnimGroupRates") == ALSteppedAnim::trackedSettings().end() &&
        !preset_settings.count("StepAnimGroupRates"));
    for (const char* name : {"StepAnimNearbyRadius", "StepAnimLimitDistance", "StepAnimIncludeSelf", "StepAnimSeed",
                             "StepAnimPhaseSpread", "StepAnimRateVariation", "StepAnimTimingJitter"})
        ensure("T12 targeting/variation tracked but never preset-owned",
            !preset_settings.count(name) &&
            std::find(ALSteppedAnim::trackedSettings().begin(), ALSteppedAnim::trackedSettings().end(), name) !=
                ALSteppedAnim::trackedSettings().end());
    const ALSteppedAnim::Config defaults;
    ensure("T12 new options default off", !defaults.particles && !defaults.textureAnim && !defaults.includeGhosts);
    const auto& tracked = ALSteppedAnim::trackedSettings();
    const std::set<std::string> settings(tracked.begin(), tracked.end());
    ensure("T12 all independent options tracked", settings.count("StepAnimParticles") == 1 &&
        settings.count("StepAnimTextureAnim") == 1 && settings.count("StepAnimIncludeGhosts") == 1 && settings.size() == tracked.size());
    for (bool particles : {false, true})
        for (bool textures : {false, true})
        {
            ALSteppedAnim::Config parsed;
            parsed.particles = particles;
            parsed.textureAnim = textures;
            const auto clean_options = ALSteppedAnim::sanitize(parsed);
            ensure("T12 config preserves independent option values",
                clean_options.particles == particles && clean_options.textureAnim == textures &&
                !clean_options.attachFX);
        }
    ALSteppedAnim::Config bad; bad.scope = -9; bad.mode = 9; bad.what = 9; bad.clock = 9; bad.offset = 9;
    bad.rate = std::numeric_limits<F32>::quiet_NaN(); bad.fastRate = 1.9f;
    bad.adaptThreshold = 99.f; bad.adaptHysteresis = 0.f; bad.adaptMinDwell = 0.f;
    bad.backgroundRate = 99.f; bad.phase = 1.f; bad.baseFps = 27; bad.maxAvatars = 1000;
    bad.boilAmount = 99.f; bad.boilTicks = 0; bad.impactTicks = 99; bad.impactThreshold = 0.f;
    bad.smearStrength = 99.f; bad.smearThreshold = 0.f; bad.easeAmount = 99.f;
    const auto clean = ALSteppedAnim::sanitize(bad);
    ensure_equals("T12 NaN default", clean.rate, 12.f);
    ensure_equals("T12 smooth", clean.fastRate, 0.f);
    ensure_equals("T12 phase wrap", clean.phase, 0.f);
    ensure_equals("T12 cap", clean.maxAvatars, 100);
    ensure("T12 enum clamps", clean.scope == 0 && clean.mode == 2 && clean.what == 1 &&
        clean.clock == 1 && clean.offset == 2 && clean.baseFps == 25);
    ensure("T12 adaptive clamps", clean.adaptThreshold == 10.f && clean.adaptHysteresis == .05f &&
        clean.adaptMinDwell == .05f && clean.backgroundRate == 30.f);
    ensure("T12 effect clamps", clean.boilAmount == 3.f && clean.boilTicks == 1 &&
        clean.impactTicks == 12 && clean.impactThreshold == 5.f && clean.smearStrength == 1.f &&
        clean.smearThreshold == .5f && clean.easeAmount == 1.f);
    ensure_equals("T12 effective", ALSteppedAnim::effectiveRate(1.9f), 0.f);
    const std::pair<F32, S32> mappings[] = {{12.f, 1}, {8.f, 2}, {6.f, 3}};
    for (const auto& expected : mappings)
    {
        const auto match = ALSteppedAnim::mapRotoInk(expected.first);
        ensure("T12 roto exact", match.setMotionStep && match.motionStep == expected.second);
    }
    ensure("T12 roto unchanged", !ALSteppedAnim::mapRotoInk(10.f).setMotionStep);
    ensure_equals("T12 roto clamp", ALSteppedAnim::mapRotoInk(30.f).sketchFps, 24.f);
}

// T13: run the same frame claim, debt and expiry gates as the particle loop.
// A second group visit must change neither simulation state nor accrued debt.
template<> template<> void stepped_object::test<13>()
{
    ALSteppedAnim::ParticleStepDebt debt;
    ALSteppedAnim::FrameDecision decision;
    decision.hold = true;
    const F32 dt = 1.f / 64.f;
    F32 age = 0.f, position = 0.f, velocity = 2.f;
    const F32 acceleration = 4.f;
    const F32 max_age = 4.f * dt;
    bool alive = true;
    auto visit_group = [&](U64 frame)
    {
        if (!alive || !ALSteppedAnim::claimParticleFrame(debt, frame)) return;
        const F32 elapsed = ALSteppedAnim::particleStepDt(dt, debt, 1, &decision);
        if (decision.hold && !decision.capture) return;
        position += elapsed * velocity + .5f * elapsed * elapsed * acceleration;
        velocity += elapsed * acceleration;
        age += elapsed;
        alive = !ALSteppedAnim::particleAgeExpired(age, max_age);
    };
    for (U64 frame = 1; frame <= 5; ++frame)
    {
        visit_group(frame);
        visit_group(frame); // same particle encountered after a group transfer
        close("hold debt accrues once per frame", debt.seconds, frame * dt, 1.e-7);
        ensure("held particle remains alive and drawable past its future expiry",
            alive && age == 0.f && position == 0.f && velocity == 2.f);
    }
    decision.capture = true;
    visit_group(6);
    close("tick age includes all debt once", age, 6.f * dt, 1.e-7);
    close("ballistic position at accumulated time", position, .205078125, 1.e-7);
    close("ballistic velocity at accumulated time", velocity, 2.375, 1.e-7);
    ensure("lifetime crossing during hold is removed on next tick", !alive);
    ensure_equals("capture consumes the debt", debt.seconds, 0.f);
    // Repeat the capture visit on a surviving particle to exercise dedupe itself.
    alive = true;
    visit_group(6);
    close("second group cannot apply capture twice", age, 6.f * dt, 1.e-7);
    close("second group cannot move particle twice", position, .205078125, 1.e-7);
    ensure_equals("next frame cannot replay consumed debt",
        ALSteppedAnim::particleStepDt(dt, debt, 1, &decision), dt);
    decision.capture = false;
    ALSteppedAnim::particleStepDt(dt, debt, 1, &decision);
    ensure_equals("scope/source release discards debt", ALSteppedAnim::particleStepDt(dt, debt, 1, nullptr), dt);
    ALSteppedAnim::particleStepDt(dt, debt, 1, &decision);
    decision.hold = false;
    ensure_equals("smooth adaptive decision discards debt", ALSteppedAnim::particleStepDt(dt, debt, 1, &decision), dt);
    decision.hold = true;
    ALSteppedAnim::particleStepDt(dt, debt, 1, &decision);
    decision.capture = true;
    ensure_equals("disable/re-enable epoch cannot replay stale debt",
        ALSteppedAnim::particleStepDt(dt, debt, 2, &decision), dt);
    decision.capture = false;
    ALSteppedAnim::particleStepDt(dt, debt, 2, &decision, 7);
    decision.capture = true;
    ensure_equals("leaving and re-entering scope cannot replay a prior session",
        ALSteppedAnim::particleStepDt(dt, debt, 2, &decision, 8), dt);
    ALSteppedAnim::ParticleStepDebt newborn;
    ensure_equals("new particle has no inherited source debt",
        ALSteppedAnim::particleStepDt(dt, newborn, 2, &decision), dt);
}


// Offscreen extra passes consume elapsed time once, including mid-group births.
template<> template<> void stepped_object::test<14>()
{
    const F32 dt = 1.f / 64.f;
    F32 skipped = 0.f, offset = 0.f, age = 0.f;
    F32 newborn_offset = 0.f, newborn_age = 0.f;
    ALSteppedAnim::ParticleStepDebt debt, newborn_debt;
    ALSteppedAnim::FrameDecision decision;
    decision.hold = true;
    for (S32 frame = 0; frame < 24; ++frame)
    {
        const bool step_only = frame % 8 != 7;
        decision.capture = frame % 4 == 3;
        const F32 elapsed = ALSteppedAnim::particleGroupDt(dt, skipped, offset, step_only);
        close("offscreen interval counted once", elapsed, dt, 1.e-7);
        age += ALSteppedAnim::particleStepDt(elapsed, debt, 1, &decision);
        if (frame == 3) newborn_offset = skipped; // addPart records the group's skipped time
        if (frame >= 3)
        {
            const F32 newborn_elapsed = ALSteppedAnim::particleGroupDt(dt, skipped, newborn_offset, step_only);
            close("birth excludes pre-birth skipped time", newborn_elapsed, dt, 1.e-7);
            newborn_age += ALSteppedAnim::particleStepDt(newborn_elapsed, newborn_debt, 1, &decision);
        }
        skipped = step_only ? skipped + dt : 0.f;
        if (decision.capture)
            close("offscreen particle reaches age on each avatar tick", age, (frame + 1) * dt, 1.e-7);
    }
    close("offscreen lifetime uses elapsed time", age, 24.f * dt, 1.e-7);
    close("newborn lifetime uses only its own elapsed time", newborn_age, 21.f * dt, 1.e-7);
}

// Migration is independent of the master switch and respects subsequent opt-outs.
template<> template<> void stepped_object::test<15>()
{
    for (bool attach_fx : {false, true})
        for (bool initial : {false, true})
        {
            bool particles = initial, migrated = false;
            ensure("first startup applies migration",
                ALSteppedAnim::migrateParticleSetting(attach_fx, particles, migrated));
            ensure("migration preserves prior particle hold or explicit enabled choice",
                migrated && particles == (initial || attach_fx));
            particles = false;
            ensure("later startup does not undo user's opt-out",
                !ALSteppedAnim::migrateParticleSetting(attach_fx, particles, migrated) && !particles);
        }
    ensure("avatar source is eligible", ALSteppedAnim::particleSourceEligible(true, false, false));
    ensure("attachment source is eligible", ALSteppedAnim::particleSourceEligible(false, true, false));
    ensure("HUD attachment is excluded", !ALSteppedAnim::particleSourceEligible(false, true, true));
    ensure("HUD always wins", !ALSteppedAnim::particleSourceEligible(true, true, true));
    ensure("world source is excluded", !ALSteppedAnim::particleSourceEligible(false, false, false));
}

// Low-rate ticks retain all elapsed time with bounded nonlinear integration.
template<> template<> void stepped_object::test<16>()
{
    for (F32 dt : {1.f / 6.f, 1.f / 4.f, .5f, 2.f})
    {
        const auto steps = ALSteppedAnim::particleSubsteps(dt);
        ensure("force substeps are bounded", steps.count >= 1 && steps.count <= 16 && steps.dt <= 1.f / 30.f);
        close("substeps and ballistic tail retain exact elapsed time",
            steps.count * steps.dt + steps.ballisticDt, dt, 1.e-7);
        if (dt <= .5f) ensure_equals("2-6 fps has no force-integration tail", steps.ballisticDt, 0.f);
    }
}


// T18: scope filtering for Friends / Nearby / limit-distance / include-self (T1-T4).
template<> template<> void stepped_object::test<18>()
{
    using namespace ALSteppedAnim;
    Config c; c.enabled = true; c.scope = SCOPE_FRIENDS;
    ActorFacts friend_actor; friend_actor.drawn = true; friend_actor.isFriend = true; friend_actor.cameraDistance = 50.f;
    ActorFacts stranger; stranger.drawn = true; stranger.cameraDistance = 5.f;
    ActorFacts self; self.drawn = true; self.isSelf = true; self.isSelected = true; self.inCast = true;
    ensure("friends scope keeps friends", scopeIncludes(c, friend_actor) && resolveActor(c, friend_actor, 0.).stepped);
    ensure("friends scope drops strangers", !scopeIncludes(c, stranger) && !resolveActor(c, stranger, 0.).stepped);
    ensure("friends scope with Include me steps you", scopeIncludes(c, self));
    c.includeSelf = false;
    ensure("friends scope without Include me skips you", !scopeIncludes(c, self) && !resolveActor(c, self, 0.).stepped);
    ensure("friends scope still keeps friends without Include me", scopeIncludes(c, friend_actor));
    friend_actor.drawn = false;
    ensure("friends must be in view", !scopeIncludes(c, friend_actor));

    c = Config(); c.enabled = true; c.scope = SCOPE_NEARBY; c.nearbyRadius = 20.f;
    ActorFacts near_actor; near_actor.drawn = true; near_actor.cameraDistance = 19.9f;
    ActorFacts edge_actor; edge_actor.drawn = true; edge_actor.cameraDistance = 20.f;
    ActorFacts far_actor; far_actor.drawn = true; far_actor.cameraDistance = 20.1f;
    ensure("nearby keeps inside the radius", scopeIncludes(c, near_actor) && scopeIncludes(c, edge_actor));
    ensure("nearby drops outside the radius", !scopeIncludes(c, far_actor) && !resolveActor(c, far_actor, 0.).stepped);
    c.nearbyRadius = 1000.f;
    ActorFacts distant; distant.drawn = true; distant.cameraDistance = 300.f;
    ensure("radius clamps to 256", !scopeIncludes(c, distant) && sanitize(c).nearbyRadius == 256.f);
    c.nearbyRadius = 20.f; self.cameraDistance = 0.f;
    ensure("nearby includes you by default", scopeIncludes(c, self));
    c.includeSelf = false;
    ensure("nearby without Include me skips you", !scopeIncludes(c, self));

    // Limit distance caps every other scope.
    c = Config(); c.enabled = true; c.limitDistance = true; c.nearbyRadius = 20.f;
    ActorFacts cast_far; cast_far.drawn = true; cast_far.inCast = true; cast_far.cameraDistance = 30.f;
    ActorFacts cast_near = cast_far; cast_near.cameraDistance = 10.f;
    for (S32 scope : {SCOPE_SELECTED, SCOPE_CAST, SCOPE_VIEW})
    {
        c.scope = scope;
        cast_far.isSelected = cast_near.isSelected = true;
        ensure("limit distance drops far actors in every scope", !resolveActor(c, cast_far, 0.).stepped);
        ensure("limit distance keeps near actors in every scope", resolveActor(c, cast_near, 0.).stepped);
    }
    c.limitDistance = false; c.scope = SCOPE_CAST;
    ensure("limit distance off leaves far cast alone", resolveActor(c, cast_far, 0.).stepped);
    c.limitDistance = true; cast_far.ov.mode = 2;
    ensure("Limit distance also caps per-actor Own settings", !resolveActor(c, cast_far, 0.).stepped);
    cast_near.ov.mode = 2;
    ensure("Own settings inside the limit still step", resolveActor(c, cast_near, 0.).stepped);
    c.limitDistance = false; c.scope = SCOPE_SELECTED; cast_far.isSelected = false; cast_far.inCast = false;
    ensure("Own settings still step outside the scope when the limit is off", resolveActor(c, cast_far, 0.).stepped);
    c.limitDistance = true;
    ensure("Own settings outside the scope are capped by the limit", !resolveActor(c, cast_far, 0.).stepped);
    c.scope = SCOPE_CAST; cast_far.isSelected = true; cast_far.inCast = true;
    cast_far.ov.mode = 1;
    ensure("per-actor Smooth still wins", !resolveActor(c, cast_far, 0.).stepped);

    // Cast and Selected keep their meaning for you; Include me only governs broad scopes.
    c = Config(); c.enabled = true; c.includeSelf = false; c.scope = SCOPE_CAST;
    ensure("cast scope follows the cast for you", resolveActor(c, self, 0.).stepped);
    c.scope = SCOPE_SELECTED;
    ensure("selected scope follows the selection for you", resolveActor(c, self, 0.).stepped);
    c.scope = SCOPE_VIEW;
    ensure("everyone without Include me skips you", !resolveActor(c, self, 0.).stepped);
    c.includeSelf = true;
    ensure("everyone with Include me steps you", resolveActor(c, self, 0.).stepped);

    // Mixed mode keeps the new scopes' membership (background rate for non-targets).
    c = Config(); c.enabled = true; c.mode = MODE_MIXED; c.scope = SCOPE_FRIENDS;
    ActorFacts mixed_friend; mixed_friend.drawn = true; mixed_friend.isFriend = true;
    const auto mixed = resolveActor(c, mixed_friend, 0.);
    ensure("mixed friends step at the background rate", mixed.stepped && mixed.rate == c.backgroundRate);
    ensure("mixed friends still drops strangers", !resolveActor(c, stranger, 0.).stepped);

    // The posing / camera / ghost rules and the cap still apply after the scope filter, nearest first.
    c = Config(); c.enabled = true; c.scope = SCOPE_NEARBY; c.maxAvatars = 2;
    std::vector<ActorFacts> crowd(4);
    for (U32 i = 0; i < crowd.size(); ++i) { crowd[i].drawn = true; crowd[i].cameraDistance = 4.f - i; }
    crowd[0].posing = true;
    std::vector<Assignment> assigned;
    for (const auto& f : crowd) assigned.push_back(resolveActor(c, f, 0.));
    ensure("posed actor excluded after scope", !assigned[0].stepped);
    applyActorCap(c, crowd, assigned);
    ensure("cap keeps the nearest two", assigned[3].stepped && assigned[2].stepped && !assigned[1].stepped);
    ActorFacts ghost; ghost.drawn = true; ghost.ghostOwnAnimation = true; ghost.cameraDistance = 90.f;
    c.includeGhosts = true;
    ensure("ghost clones still join an out-of-radius scope", resolveActor(c, ghost, 0.).stepped);
    c.limitDistance = true;
    ensure("limit distance also caps ghost clones", !resolveActor(c, ghost, 0.).stepped);
}

// T19: seed mixing, phase spread, rate variation, imperfect timing, rate offsets.
template<> template<> void stepped_object::test<19>()
{
    using namespace ALSteppedAnim;
    ensure_equals("seed 0 keeps the digest", mixSeed(0x1234abcdULL, 0), U64(0x1234abcdULL));
    ensure("same seed same mix", mixSeed(77, 5) == mixSeed(77, 5));
    ensure("different seed different mix", mixSeed(77, 5) != mixSeed(77, 6) && mixSeed(77, 5) != 77);
    ensure("different actor different mix", mixSeed(78, 5) != mixSeed(77, 5));
    Config c; c.enabled = true; c.scope = SCOPE_VIEW; c.rateVariation = 1.f;
    ActorFacts a; a.drawn = true;
    S32 changed = 0;
    for (U32 actor = 1; actor <= 64; ++actor)
    {
        a.seed = mixSeed(actor * 7919ULL, 11);
        const auto first = resolveActor(c, a, 0.), again = resolveActor(c, a, 0.);
        ensure("same seed gives the same per-actor values", first.phase == again.phase && first.rate == again.rate);
        a.seed = mixSeed(actor * 7919ULL, 12);
        if (resolveActor(c, a, 0.).phase != first.phase) ++changed;
    }
    ensure("a different seed re-rolls the look", changed >= 60);

    // Phase spread: 0 = lockstep, 1 = today's values, in between scales.
    Config spread; spread.offset = OFFSET_RANDOM;
    std::vector<F64> today;
    for (U64 seed = 1; seed <= 32; ++seed)
        today.push_back(static_cast<F64>(unitHash(seed, 0x7068617365LL, 0, 0)));
    for (U64 seed = 1; seed <= 32; ++seed)
    {
        spread.phaseSpread = 1.f;
        ensure_equals("spread 1 reproduces today's phase", actorPhase(spread, seed, 0.), today[seed - 1]);
        spread.phaseSpread = 0.f;
        ensure_equals("spread 0 is lockstep", actorPhase(spread, seed, 0.), 0.);
        spread.phaseSpread = .5f;
        close("spread scales the random phase", actorPhase(spread, seed, 0.), today[seed - 1] * .5, 1.e-7);
    }
    spread.offset = OFFSET_MANUAL; spread.phase = .35f; spread.phaseSpread = 0.f;
    close("manual phase ignores spread", actorPhase(spread, 3, 0.), .35, 1.e-6);
    spread.offset = OFFSET_LOCKED;
    ensure_equals("locked phase ignores spread", actorPhase(spread, 3, 0.), 0.);

    // Rate variation: factor within [1, 1+v]; the step length is multiplied on the actor's own
    // grid (whole base frames for twos/threes/..., whole own steps for 10 or 30 fps), never faster.
    for (F32 v : {.01f, .1f, .5f, 1.f})
        for (F32 base_rate : {24.f, 12.f, 8.f, 6.f, 10.f, 2.f, 30.f, 5.f})
        {
            S32 shapes[32] = {};
            const F32 own_frames = 24.f / base_rate;
            const bool whole_frames = std::fabs(own_frames - std::round(own_frames)) < 1.e-3f;
            for (U64 seed = 1; seed <= 400; ++seed)
            {
                const F32 factor = rateFactor(seed, v);
                ensure("factor within [1, 1+v]", factor >= 1.f && factor <= 1.f + v + 1.e-6f);
                const F32 rate = varyRate(base_rate, seed, v, 24);
                ensure("never faster than the actor's own rate", rate <= base_rate + 1.e-5f);
                ensure("never below 2 fps", rate >= 2.f - 1.e-5f);
                ensure("deterministic", rate == varyRate(base_rate, seed, v, 24));
                const F32 frames = 24.f / rate;
                if (whole_frames)
                    ensure("whole-frame actors land on whole base frames", std::fabs(frames - std::round(frames)) < 1.e-4f);
                else
                {
                    const F32 steps = base_rate / rate;
                    ensure("other rates lengthen by a whole number of their own steps",
                        std::fabs(steps - std::round(steps)) < 1.e-4f);
                    ensure("no snap to the base: rate is the actor's own or a whole division of it",
                        rate == base_rate || std::fabs(rate - base_rate / std::round(steps)) < 1.e-4f);
                }
                if (frames >= 2.f && frames < 32.f) ++shapes[static_cast<S32>(std::round(frames))];
            }
            if (v == 1.f && base_rate == 12.f)
                ensure("some actors land on threes and fours when the base is twos", shapes[3] > 0 && shapes[4] > 0);
        }
    // Low-fps edge: 10 fps on a 24 base with a tiny spread keeps 10 (not a whole-frame fallback)
    // and a full spread can double the step, never a rate between.
    for (U64 seed = 1; seed <= 400; ++seed)
    {
        const F32 tiny = varyRate(10.f, seed, .01f, 24);
        ensure_equals("10 fps with a tiny spread keeps its own step", tiny, 10.f);
        const F32 full = varyRate(10.f, seed, 1.f, 24);
        ensure("10 fps with a full spread is 10 or 5", full == 10.f || std::fabs(full - 5.f) < 1.e-4f);
    }
    // Rate above the base (30 on 24) is varied on its own grid, never snapped down to 24.
    {
        bool saw_thirty = false, saw_fifteen = false;
        for (U64 seed = 1; seed <= 400; ++seed)
        {
            const F32 r = varyRate(30.f, seed, 1.f, 24);
            ensure("30 fps stays 30 or halves to 15", std::fabs(r - 30.f) < 1.e-4f || std::fabs(r - 15.f) < 1.e-4f);
            ensure("30 fps never snaps to the base", std::fabs(r - 24.f) > .01f);
            saw_thirty = saw_thirty || r == 30.f;
            saw_fifteen = saw_fifteen || std::fabs(r - 15.f) < 1.e-4f;
            ensure_equals("30 fps with a tiny spread is untouched", varyRate(30.f, seed, .01f, 24), 30.f);
        }
        ensure("30 fps varies across actors", saw_thirty && saw_fifteen);
    }
    ensure_equals("variation 0 is the identity", varyRate(12.f, 99, 0.f, 24), 12.f);
    ensure_equals("smooth stays smooth", varyRate(0.f, 99, 1.f, 24), 0.f);

    // Variation respects fixed, adaptive and mixed rates.
    Config modes; modes.enabled = true; modes.scope = SCOPE_VIEW; modes.rateVariation = 1.f;
    ActorFacts f; f.drawn = true;
    for (U64 seed = 1; seed <= 64; ++seed)
    {
        f.seed = seed;
        modes.mode = MODE_ADAPTIVE;
        const auto adaptive = resolveActor(modes, f, 0.);
        ensure("adaptive: both rates varied but never faster", adaptive.rate <= modes.rate && adaptive.fastRate <= modes.fastRate &&
            adaptive.fastRate >= 2.f);
        modes.mode = MODE_MIXED;
        ensure("mixed: background varies from its own rate", resolveActor(modes, f, 0.).rate <= modes.backgroundRate);
    }

    // Group / per-actor rate offsets: +1 step holds one more base frame; precedence actor > group.
    close("+1 step: twos become threes", offsetRate(12.f, 1, 24), 8., 1.e-4);
    close("+2 steps: twos become fours", offsetRate(12.f, 2, 24), 6., 1.e-4);
    ensure_equals("offset 0 is the identity", offsetRate(12.f, 0, 24), 12.f);
    ensure("offset never goes below 2 fps", offsetRate(2.f, 2, 24) >= 2.f);
    Config o; o.enabled = true; o.scope = SCOPE_VIEW;
    ActorFacts g; g.drawn = true; g.seed = 5;
    g.groupOffset = 1;
    close("group +1 coarsens the rate", resolveActor(o, g, 0.).rate, 8., 1.e-4);
    g.ov.rateOffset = 2;
    close("per-actor offset beats the group", resolveActor(o, g, 0.).rate, 6., 1.e-4);
    g.ov.rateOffset = 0;
    ensure_equals("per-actor Same beats the group", resolveActor(o, g, 0.).rate, 12.f);
    g.ov.rateOffset = -1; g.groupOffset = 3;
    ensure("group Smooth keeps the whole group smooth", !resolveActor(o, g, 0.).stepped);
    g.ov.rateOffset = 1;
    close("per-actor offset beats group Smooth", resolveActor(o, g, 0.).rate, 8., 1.e-4);
    g.ov.rateOffset = -1; g.groupOffset = 0;
    ensure_equals("untagged actors use the base rate", resolveActor(o, g, 0.).rate, 12.f);
    g.groupOffset = 1; g.ov.mode = 2; g.ov.rate = 10.f; g.ov.rateOffset = -1;
    ensure_equals("Own settings ignores the group offset", resolveActor(o, g, 0.).rate, 10.f);
    o.mode = MODE_ADAPTIVE; g.ov.mode = 0; g.groupOffset = 1;
    const auto grouped = resolveActor(o, g, 0.);
    close("adaptive group offset coarsens the fast rate too", grouped.fastRate, 1.f / (1.f / 24.f + 1.f / 24.f), 1.e-4);

    // Match Roto Ink keeps stepped actors on the Roto cadence: variation and offsets are switched off.
    Config roto; roto.enabled = true; roto.scope = SCOPE_VIEW; roto.matchRoto = true; roto.rateVariation = 1.f;
    ActorFacts rf; rf.drawn = true; rf.groupOffset = 2;
    for (U64 seed = 1; seed <= 64; ++seed)
    {
        rf.seed = seed;
        ensure_equals("Roto Ink match: no offset, no variation", resolveActor(roto, rf, 0.).rate, roto.rate);
    }
    rf.groupOffset = 3;
    ensure("Roto Ink match: a Smooth group is still smooth", !resolveActor(roto, rf, 0.).stepped);
    const RateChoice locked = resolveRate(roto, ActorOverride(), true, 2);
    ensure("Roto lock reports what it switched off", locked.rotoLocked && locked.rotoSuppressed &&
        locked.offset == RATE_SAME && locked.variation == 0.f);
    roto.rateVariation = 0.f;
    ensure("Roto lock with nothing to suppress says nothing", !resolveRate(roto, ActorOverride(), true, 0).rotoSuppressed);

    // resolveRate is what resolveActor uses: mixed role, adaptive fast rate, own settings, base fps.
    {
        Config pure; pure.enabled = true; pure.mode = MODE_MIXED; pure.rate = 12.f; pure.backgroundRate = 8.f;
        ActorOverride none;
        ensure_equals("mixed selected actor uses the target rate", resolveRate(pure, none, true, 0).rate, 12.f);
        ensure_equals("mixed unselected actor uses the background rate", resolveRate(pure, none, false, 0).rate, 8.f);
        ActorOverride background; background.role = 2;
        ensure_equals("an own Background role beats selection", resolveRate(pure, background, true, 0).rate, 8.f);
        pure.mode = MODE_ADAPTIVE; pure.fastRate = 24.f;
        const RateChoice fast = resolveRate(pure, none, true, 1);
        close("adaptive keeps both rates and offsets the fast one", fast.fastRate, 12., 1.e-4);
        ensure("adaptive choice is flagged", fast.adaptive && fast.stepped);
        ActorOverride never; never.mode = 1;
        ensure("Never is smooth", !resolveRate(pure, never, true, 0).stepped);
        ActorOverride own; own.mode = 2; own.rate = 10.f;
        ensure_equals("own settings ignore the page rate", resolveRate(pure, own, true, 1).rate, 10.f);
        pure.baseFps = 30;
        close("offsets use the sanitised base fps", resolveRate(pure, none, true, 1).rate,
            1.f / (1.f / 12.f + 1.f / 30.f), 1.e-4);
        pure.baseFps = 26; // sanitises to 25
        close("a stray base fps sanitises first", resolveRate(pure, none, true, 1).rate,
            1.f / (1.f / 12.f + 1.f / 25.f), 1.e-4);
        pure.rateVariation = .5f;
        ensure_equals("variation is reported, not applied", resolveRate(pure, none, true, 0).variation, .5f);
    }

    // Imperfect timing.
    ensure("jitter 0 never holds extra", [&]
    {
        for (S64 tick = 0; tick < 5000; ++tick) if (extraHold(42, tick, 0.f, false)) return false;
        return true;
    }());
    ensure("a hold is never followed by another", [&]
    {
        for (S64 tick = 0; tick < 5000; ++tick) if (extraHold(42, tick, 1.f, true)) return false;
        return true;
    }());
    S32 holds = 0;
    for (S64 tick = 0; tick < 20000; ++tick) if (extraHold(42, tick, 1.f, false)) ++holds;
    ensure("probability is jitter * 0.25", holds > 4400 && holds < 5600);
    ensure("deterministic per actor + tick", extraHold(42, 17, 1.f, false) == extraHold(42, 17, 1.f, false));
    for (F32 jitter : {.2f, 1.f})
    {
        Config jc; jc.enabled = true; jc.timingJitter = jitter;
        Assignment as = assignment(); as.seed = 91;
        ActorState state; S64 last_tick = -1; S32 consecutive = 0, extra_holds = 0, max_run = 0;
        for (S32 frame = 0; frame < 24 * 600; ++frame)
        {
            const auto d = planFrame(state, as, jc, frame / 24.);
            if (frame > 0 && d.tick != last_tick)
            {
                if (d.capture) consecutive = 0;
                else { ++extra_holds; max_run = std::max(max_run, ++consecutive); }
            }
            last_tick = d.tick;
        }
        ensure("imperfect timing adds extra holds", extra_holds > 0);
        ensure("never two extra holds in a row", max_run == 1);
    }
    Config jc0; jc0.enabled = true;
    ActorState s0; Assignment as0 = assignment(); as0.seed = 91;
    for (S32 frame = 0; frame < 24 * 60; ++frame)
    {
        const auto d = planFrame(s0, as0, jc0, frame / 24.);
        ensure("jitter 0 captures every boundary", d.capture == (frame == 0 || frame % 2 == 0));
    }
}

// T20: defaults reproduce today's decisions exactly.
template<> template<> void stepped_object::test<20>()
{
    using namespace ALSteppedAnim;
    const Config defaults;
    ensure("new defaults", defaults.nearbyRadius == 20.f && !defaults.limitDistance && defaults.includeSelf &&
        defaults.seed == 0 && defaults.phaseSpread == 1.f && defaults.rateVariation == 0.f && defaults.timingJitter == 0.f);
    ensure("sanitize keeps the new defaults", [&]
    {
        const Config clean = sanitize(defaults);
        return clean.nearbyRadius == 20.f && !clean.limitDistance && clean.includeSelf && clean.seed == 0 &&
            clean.phaseSpread == 1.f && clean.rateVariation == 0.f && clean.timingJitter == 0.f;
    }());
    Config bad; bad.scope = 9; bad.nearbyRadius = 0.f; bad.phaseSpread = 9.f; bad.rateVariation = -2.f;
    bad.timingJitter = std::numeric_limits<F32>::quiet_NaN();
    const Config clean = sanitize(bad);
    ensure("scope clamps to 0-4", clean.scope == SCOPE_NEARBY);
    ensure("new ranges clamp", clean.nearbyRadius == 1.f && clean.phaseSpread == 1.f && clean.rateVariation == 0.f &&
        clean.timingJitter == 0.f);
    ensure("existing scope values keep their meaning",
        SCOPE_SELECTED == 0 && SCOPE_CAST == 1 && SCOPE_VIEW == 2 && SCOPE_FRIENDS == 3 && SCOPE_NEARBY == 4);

    // Assignments with default config equal the pre-feature formulas.
    for (S32 scope : {SCOPE_SELECTED, SCOPE_CAST, SCOPE_VIEW})
        for (S32 mode : {MODE_FIXED, MODE_ADAPTIVE, MODE_MIXED})
            for (S32 offset : {OFFSET_RANDOM, OFFSET_LOCKED, OFFSET_MANUAL})
                for (U64 seed = 1; seed <= 8; ++seed)
                {
                    Config c; c.enabled = true; c.scope = scope; c.mode = mode; c.offset = offset; c.phase = .25f;
                    ActorFacts f; f.drawn = true; f.inCast = true; f.isSelected = (seed & 1) != 0; f.seed = seed;
                    f.cameraDistance = 500.f;
                    const Assignment now = resolveActor(c, f, 0.);
                    ensure("default scope membership",
                        now.stepped == (scope == SCOPE_SELECTED && mode != MODE_MIXED ? f.isSelected : true));
                    if (!now.stepped) continue;
                    const F64 expected_phase = offset == OFFSET_RANDOM ? static_cast<F64>(unitHash(seed, 0x7068617365LL, 0, 0))
                        : (offset == OFFSET_MANUAL ? .25 : 0.);
                    ensure_equals("default phase", now.phase, expected_phase);
                    const F32 expected_rate = mode == MODE_MIXED ? (f.isSelected ? c.rate : c.backgroundRate) : c.rate;
                    ensure_equals("default rate", now.rate, expected_rate);
                    ensure_equals("default fast rate", now.fastRate, mode == MODE_ADAPTIVE ? c.fastRate : 0.f);
                    ensure_equals("default held classes", now.heldClasses, heldClasses(sanitize(c), now.what));
                }

    // Default planFrame: capture exactly on the first call and whenever the analytic tick changes.
    Config c; c.enabled = true;
    ActorState state; Assignment as = assignment(); as.seed = 1234; as.phase = .3;
    S64 previous = -1000;
    for (S32 frame = 0; frame < 24 * 600; ++frame)
    {
        const F64 t = frame / 24. + .0037 * std::sin(frame * .3);
        const auto d = planFrame(state, as, c, t);
        const S64 tick = tickIndex(t, 12., .3);
        ensure("default tick", d.tick == tick);
        ensure("default capture", d.capture == (frame == 0 || tick != previous));
        ensure("default hold", d.hold);
        previous = tick;
    }
    ensure("default no extra hold state", !state.jitterHeld);
}


// T21: cast-group rates and the per-actor override travel with the scene.
template<> template<> void stepped_object::test<21>()
{
    using namespace ALSteppedAnim;
    const std::vector<std::string> live = {"Hunters", "Demons"};
    GroupRates rates;
    setGroupRate(rates, "Hunters", 1);
    setGroupRate(rates, "Demons", 3);
    setGroupRate(rates, "Ghosts", 2); // no members: never written
    setGroupRate(rates, "", 2);       // an empty tag is never a group
    ensure_equals("empty tag ignored", rates.count(""), size_t(0));
    setGroupRate(rates, "Wide", 9);
    ensure_equals("offsets clamp to 0-3", groupRate(rates, "Wide"), S32(3));
    setGroupRate(rates, "Wide", 0);
    ensure_equals("same erases the entry", rates.count("Wide"), size_t(0));

    // scene round trip: only live groups, defaults (same) are not stored
    const LLSD saved = writeGroupRates(rates, live);
    ensure("group_rates is a map", saved.isMap());
    ensure_equals("only live groups saved", saved.size(), 2);
    ensure("no stale group saved", !saved.has("Ghosts"));
    const GroupRates loaded = readGroupRates(saved, live);
    ensure_equals("Hunters survives the scene", groupRate(loaded, "Hunters"), S32(1));
    ensure_equals("Demons survives the scene", groupRate(loaded, "Demons"), S32(3));
    ensure_equals("unknown group reads as same", groupRate(loaded, "Ghosts"), S32(0));
    ensure("round trip is stable", readGroupRates(writeGroupRates(loaded, live), live) == loaded);
    ensure("an older scene without group_rates reads empty", readGroupRates(LLSD(), live).empty());
    // a later scene reusing other names never inherits the earlier scene's rates
    ensure("another scene's live groups drop rates it does not carry",
        readGroupRates(saved, std::vector<std::string>{"Crowd"}).empty());

    // rename keeps the rate; a merge keeps the target's; the source name is gone
    GroupRates r;
    setGroupRate(r, "Hunters", 1);
    renameGroupRate(r, "Hunters", "Wolves");
    ensure("rename carries the rate", groupRate(r, "Wolves") == 1 && !r.count("Hunters"));
    setGroupRate(r, "Demons", 2);
    renameGroupRate(r, "Wolves", "Demons");
    ensure("merge keeps the target's own rate", groupRate(r, "Demons") == 2 && !r.count("Wolves"));
    setGroupRate(r, "Hunters", 3);
    renameGroupRate(r, "Hunters", "Crowd");
    ensure("rename onto a name without a rate carries the source's", groupRate(r, "Crowd") == 3);
    renameGroupRate(r, "Nobody", "Crowd");
    renameGroupRate(r, "Crowd", "");
    renameGroupRate(r, "Crowd", "Crowd");
    ensure("unknown / empty / same renames change nothing", groupRate(r, "Crowd") == 3 && r.size() == 2);

    // prune: a group that lost its last member sheds its rate; one with members keeps it
    pruneGroupRate(r, "Crowd", true);
    ensure("a group with members keeps its rate", groupRate(r, "Crowd") == 3);
    pruneGroupRate(r, "Crowd", false);
    ensure("a group without members drops its rate", !r.count("Crowd") && groupRate(r, "Demons") == 2);
    pruneGroupRate(r, "", false);
    ensure("an empty tag prunes nothing", groupRate(r, "Demons") == 2);

    // per-actor override scene round trip, including rate_offset
    for (S32 offset : {-1, 0, 1, 2})
    {
        ActorOverride ov; ov.mode = 2; ov.rate = 9.5f; ov.what = 1; ov.role = 2; ov.rateOffset = offset;
        const ActorOverride back = readOverride(writeOverride(ov));
        ensure("override round trip", back.mode == 2 && back.rate == 9.5f && back.what == 1 && back.role == 2 &&
            back.rateOffset == offset);
    }
    ActorOverride offset_only; offset_only.rateOffset = 2;
    const LLSD offset_data = writeOverride(offset_only);
    ensure("a rate-offset-only override is saved", offset_data.isMap() && offset_data["rate_offset"].asInteger() == 2);
    ensure("an all-default override saves nothing", !writeOverride(ActorOverride()).isDefined());
    ActorOverride same_only; same_only.rateOffset = 0;
    ensure("same (0) is a saved choice, not the default", writeOverride(same_only).isDefined());
    LLSD old_scene = LLSD::emptyMap();
    old_scene["mode"] = 2; old_scene["rate"] = 8.0;
    ensure_equals("a scene from before rate_offset reads as inherit", readOverride(old_scene).rateOffset, S32(-1));
    LLSD hostile = LLSD::emptyMap();
    hostile["rate_offset"] = 77; hostile["mode"] = 9; hostile["rate"] = 1000.0;
    const ActorOverride clamped = readOverride(hostile);
    ensure("hostile override values clamp", clamped.rateOffset == 2 && clamped.mode == 2 && clamped.rate == 30.f);
}

}
