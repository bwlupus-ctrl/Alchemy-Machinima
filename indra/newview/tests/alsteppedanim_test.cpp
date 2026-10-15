/**
 * @file alsteppedanim_test.cpp
 * @brief Stepped-animation pure-core tests (timing, particle catch-up and R1 gates).
 */
#include "linden_common.h"
#include "llmath.h"
#include "../test/lltut.h"
#include "../alsteppedanim.h"

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
    ensure_equals("T12 tracked count", ALSteppedAnim::trackedSettings().size(), size_t(32));
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

}
