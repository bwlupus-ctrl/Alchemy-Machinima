/**
 * @file llflycamdrone_test.cpp
 * @brief FLYCAM_DRONE_CONTROL_BRIEF rev 11, section 16.1.
 */

#include "linden_common.h"
#include "../test/lltut.h"
#include "../llflycamdrone.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace tut
{
namespace
{
constexpr F32 PI = 3.14159265358979323846f;
constexpr F32 DEG = PI / 180.f;
constexpr F32 FP = 1.000001f;
const F32 HZS[] = { 30.f, 60.f, 120.f, 240.f };

F32 mag(const LLVector3& v) { return v.length(); }
bool finite(const LLVector3& v)
{
    return std::isfinite(v.mV[VX]) && std::isfinite(v.mV[VY]) && std::isfinite(v.mV[VZ]);
}
void close(const char* what, F64 actual, F64 expected, F64 tolerance)
{
    ensure(what, std::fabs(actual - expected) <= tolerance);
}
void close_relative(const char* what,F64 actual,F64 expected,F64 relative)
{
    ensure(what,std::fabs(actual-expected)<=relative*std::max(std::fabs(expected),std::numeric_limits<F64>::min()));
}
void vector_close(const char* what, const LLVector3& a, const LLVector3& b, F32 tolerance)
{
    ensure(what, mag(a - b) <= tolerance);
}
void quaternion_close(const char* what, const LLQuaternion& a, const LLQuaternion& b, F32 radians)
{
    F32 d = std::fabs(dot(a, b));
    d = std::max(-1.f, std::min(1.f, d));
    ensure(what, 2.f * std::acos(d) <= radians);
}

LLFlycamDrone::Params compiled(std::size_t preset, F32 hz,
                               LLFlycamDrone::HumanParams* accepted = nullptr,
                               std::vector<std::string>* messages = nullptr)
{
    const auto& h = LLFlycamDrone::presets()[preset].human;
    LLFlycamDrone::Params p;
    LLFlycamDrone::Logger logger;
    if (messages)
        logger = [messages](const std::string& s) { messages->push_back(s); };
    LLFlycamDrone::compile(h, h, 1.f / hz, 2.f, p, accepted, logger);
    return p;
}

LLFlycamDrone::Input forward(F32 throttle = 1.f)
{
    LLFlycamDrone::Input in;
    in.move.set(1.f, 0.f, 0.f);
    in.throttle = throttle;
    in.droneThrottle = true;
    return in;
}

void assert_g1(const char* what, const LLFlycamDrone::State& a,
               const LLFlycamDrone::State& b, const LLFlycamDrone::Params& p, F32 h)
{
    (void)p;
    const F32 acceleration = mag(b.velocity - a.velocity) / h;
    const F32 accelerationBound = b.totalLimitMax;
    const F32 jerk = mag(b.deltaVelocity - a.deltaVelocity) / (h * h);
    const F32 jerkBound = LLFlycamDrone::effectiveJerk(b.pole, b.totalLimitMax, h);
    // G1's only FP allowance is the normative relative factor.  A zero bound
    // needs an absolute comparison because relative tolerance is undefined;
    // scale one F32 epsilon by the state magnitude involved in the difference.
    const F32 stateScale = std::max(mag(a.velocity), mag(b.velocity));
    ensure(what, accelerationBound == 0.f
        ? acceleration <= std::numeric_limits<F32>::epsilon() * stateScale
        : acceleration <= accelerationBound * FP);
    ensure("G1 jerk", jerkBound == 0.f
        ? jerk <= std::numeric_limits<F32>::epsilon() * stateScale / h
        : jerk <= jerkBound * FP);
}

LLFlycamDrone::State run_steps(const LLFlycamDrone& model, LLFlycamDrone::State s,
                               const LLFlycamDrone::Input& in, S32 count, F32 h)
{
    for (S32 i = 0; i < count; ++i) s = model.step(s, in, h);
    return s;
}

bool exact_state(const LLFlycamDrone::State& a, const LLFlycamDrone::State& b)
{
    return a.position == b.position && a.referenceVelocity == b.referenceVelocity &&
        a.smootherVelocity == b.smootherVelocity && a.velocity == b.velocity &&
        a.deltaVelocity == b.deltaVelocity && a.yaw == b.yaw &&
        a.yawVelocity == b.yawVelocity && a.tilt == b.tilt &&
        a.tiltVelocity == b.tiltVelocity && a.roll == b.roll &&
        a.rollVelocity == b.rollVelocity && a.quaternionSign == b.quaternionSign &&
        a.tiltLimit == b.tiltLimit && a.pole == b.pole &&
        a.pendingPole == b.pendingPole && a.totalLimitMax == b.totalLimitMax &&
        a.hasPendingPole == b.hasPendingPole && a.recentre == b.recentre &&
        a.snapped == b.snapped && a.settled == b.settled &&
        a.nonFiniteRecovered == b.nonFiniteRecovered;
}
bool exact_params(const LLFlycamDrone::Params& a,const LLFlycamDrone::Params& b)
{
    return a.cruise==b.cruise&&a.top==b.top&&a.acceleration==b.acceleration&&a.deceleration==b.deceleration&&
        a.brakeDeceleration==b.brakeDeceleration&&a.dragLinear==b.dragLinear&&a.dragQuadratic==b.dragQuadratic&&
        a.grip==b.grip&&a.climbSpeed==b.climbSpeed&&a.climbAcceleration==b.climbAcceleration&&a.pole==b.pole&&
        a.yawRate==b.yawRate&&a.speedReference==b.speedReference&&a.yawAcceleration==b.yawAcceleration&&
        a.tiltRate==b.tiltRate&&a.tiltAcceleration==b.tiltAcceleration&&a.rollRate==b.rollRate&&
        a.rollAcceleration==b.rollAcceleration&&a.velocityCap==b.velocityCap&&a.horizontalLimit==b.horizontalLimit&&
        a.dragLimit==b.dragLimit&&a.totalLimit==b.totalLimit&&a.gMax==b.gMax&&a.effectiveJerk==b.effectiveJerk&&
        a.tick==b.tick&&a.release==b.release&&a.throttleMode==b.throttleMode&&a.freeFly==b.freeFly&&
        a.autoLevel==b.autoLevel&&a.yawExpo==b.yawExpo&&a.gimbalExpo==b.gimbalExpo;
}

struct Crossing { F32 interpolated; F32 raw; };
Crossing crossing(const LLFlycamDrone& model, LLFlycamDrone::State s,
                  const LLFlycamDrone::Input& in, F32 target, F32 h,
                  S32 component, bool camera, bool rising)
{
    F32 previous=std::fabs((camera?s.velocity:s.referenceVelocity).mV[component]);
    for(S32 n=1;n<200000;++n)
    {
        const auto next=model.step(s,in,h);
        const F32 value=std::fabs((camera?next.velocity:next.referenceVelocity).mV[component]);
        if(rising?value>=target:value<=target)
        {
            const F32 fraction=value==previous?1.f:(target-previous)/(value-previous);
            return {(static_cast<F32>(n-1)+fraction)*h,static_cast<F32>(n)*h};
        }
        previous=value;s=next;
    }
    return {std::numeric_limits<F32>::infinity(),std::numeric_limits<F32>::infinity()};
}

void assert_timing(const char* interpolation,const char* raw,const char* camera,
                   const Crossing& mass,const Crossing& output,F32 requested,F32 onset,F32 h)
{
    ensure(interpolation,std::fabs(mass.interpolated-requested)<=.005f*requested);
    ensure(raw,std::fabs(mass.raw-requested)<=h);
    ensure(camera,std::fabs(output.interpolated-(requested+onset))<=.02f*requested+h);
}

LLVector3 global_position(const LLFlycamDrone& d)
{
    const LLVector3d& o = d.originTag();
    return d.state().position + LLVector3((F32)o.mdV[VX], (F32)o.mdV[VY], (F32)o.mdV[VZ]);
}
}

struct flycam_drone_data {};
typedef test_group<flycam_drone_data> flycam_drone_group;
typedef flycam_drone_group::object flycam_drone_object;
flycam_drone_group gFlycamDroneGroup("llflycamdrone");

// T1 frame grouping: varying held-input runs preserve cur, prev and accumulator.
template<> template<> void flycam_drone_object::test<1>()
{
    for (std::size_t pi = 0; pi < LLFlycamDrone::presets().size(); ++pi)
    for (F32 hz : HZS)
    {
        const auto p = compiled(pi, hz);
        LLFlycamDrone a(p), b(p);
        std::array<LLFlycamDrone::Input,4> inputs{{forward(.2f),forward(.9f),LLFlycamDrone::Input(),forward(.55f)}};
        inputs[1].move.set(.2f,.9f,0.f); inputs[2].brake=1.f; inputs[2].droneThrottle=true;
        const S32 lengths[] = { 37, 61, 19, 83 };
        for (S32 segment=0;segment<4;++segment)
        {
            for(S32 n=0;n<lengths[segment];++n) a.advanceRaw(p.tick,inputs[segment]);
            S32 remaining=lengths[segment];
            for(S32 group=1;remaining>0;++group)
            {
                const S32 ticks=std::min(remaining,1+(group%7));
                b.advanceRaw(ticks*p.tick,inputs[segment]); remaining-=ticks;
            }
        }
        a.advanceRaw(.37*p.tick,inputs[3]); b.advanceRaw(.37*p.tick,inputs[3]);
        ensure("T1 bit-identical state", exact_state(a.state(), b.state()));
        ensure("T1 bit-identical previous", exact_state(a.previousState(),b.previousState()));
        close("T1 identical accumulator",a.accumulator(),b.accumulator(),1.e-12);
    }
}

// T2 live held input has only the specified transition quantisation error and no drift.
template<> template<> void flycam_drone_object::test<2>()
{
    for (F32 fps : { 30.f, 60.f, 144.f })
    {
        auto p = compiled(1, 120.f); LLFlycamDrone live(p), aligned(p);
        LLFlycamDrone::Input zero, on = forward();
        const F32 transition = .37f;
        for (S32 f = 0; f < (S32)(2.f * fps); ++f)
        {
            const F32 t = f / fps;
            live.advanceRaw(1.f / fps, t >= transition ? on : zero);
            aligned.advanceRaw(1.f / fps, t + 1.f / fps >= transition ? on : zero);
        }
        ensure("T2 bounded transition error", mag(live.state().position - aligned.state().position) <=
               p.top / fps + 1.e-5f);
        for(S32 f=0;f<(S32)(10.f*fps);++f){live.advanceRaw(1.f/fps,on);aligned.advanceRaw(1.f/fps,on);ensure("T2 no unbounded drift",mag(live.state().position-aligned.state().position)<=p.top/fps+1.e-4f);}
    }
}

template<> template<> void flycam_drone_object::test<3>()
{
    auto p=compiled(1,120.f);LLFlycamDrone d(p);d.advanceRaw(2*p.tick,forward());
    for(S32 i=0;i<7;++i){d.advanceRaw(p.tick/8.f,forward());const LLVector3 a=d.previousState().position,b=d.state().position,r=d.renderPose().position;
      const LLVector3 segment=b-a;const F32 length2=segment.lengthSquared();const F32 projection=length2>0.f?((r-a)*segment)/length2:0.f;
      ensure("T3 after previous bound",projection>=0.f);ensure("T3 before current bound",projection<=1.f);
      ensure("T3 render lag <= h",mag(b-r)<=p.top*p.tick*FP);}
}

template<> template<> void flycam_drone_object::test<4>()
{
    auto p = compiled(1, 120.f); LLFlycamDrone raw(p), exact(p), clamped(p), quarter(p);
    raw.advanceRaw(2000. * p.tick, forward());
    ensure("T4 512-step cap/backlog", raw.accumulator() >= (2000 - 512) * p.tick - 1.e-7);
    LLVector3 last = raw.renderPose().position;
    while (raw.accumulator() + 1.e-9 >= p.tick)
    {
        const LLVector3 now = raw.advanceRaw(0., forward()).position;
        ensure("T4 drain continuity", mag(now - last) <= p.top * 512.f * p.tick + 1.e-4f);
        last = now;
    }
    for(S32 n=0;n<2000;++n) exact.advanceRaw(p.tick,forward());
    close("T4 total simulated time",raw.state().position.mV[VX],exact.state().position.mV[VX],1.e-6);
    close("T4 drained remainder",raw.accumulator(),exact.accumulator(),1.e-9);
    clamped.advance(2.,forward());quarter.advanceRaw(.25,forward());
    ensure("T4 .25 clamp state equivalence",exact_state(clamped.state(),quarter.state()));
    ensure("T4 .25 clamp previous equivalence",exact_state(clamped.previousState(),quarter.previousState()));
}

// T5 t_top: mass-stage 90% crossing and camera latency.
template<> template<> void flycam_drone_object::test<5>()
{
    for (std::size_t pi = 0; pi < LLFlycamDrone::presets().size(); ++pi) for (F32 hz : HZS)
    {
        auto p = compiled(pi, hz); LLFlycamDrone d(p); const auto& h = LLFlycamDrone::presets()[pi].human;
        const auto mass=crossing(d,d.state(),forward(),.9f*h.top,p.tick,VX,false,true);
        const auto camera=crossing(d,d.state(),forward(),.9f*h.top,p.tick,VX,true,true);
        assert_timing("T5 interpolated t_top","T5 raw t_top +/-h","T5 camera t_top+t_j",mass,camera,h.timeToTop,h.onset,p.tick);
    }
}

// T6 d_stop, including camera distance, exact zero reference on settle, and truncation bound.
template<> template<> void flycam_drone_object::test<6>()
{
    for (std::size_t pi = 0; pi < LLFlycamDrone::presets().size(); ++pi) for (F32 hz : HZS)
    {
        auto p = compiled(pi, hz); LLFlycamDrone d(p); LLFlycamDrone::State s = d.state();
        s.position.setZero(); s.referenceVelocity.set(p.top,0,0); s.smootherVelocity=s.velocity=s.referenceVelocity;
        s.pole=p.pole; s.totalLimitMax=p.totalLimit; LLFlycamDrone::Input stop; stop.brake=1.f;F64 massDistance=0.;
        for (S32 n=0; n<200000 && !s.settled; ++n)
        {
            s=d.step(s,stop,p.tick);
            massDistance+=s.referenceVelocity.mV[VX]*p.tick;
            if(s.settled)ensure_equals("T6 every settle has exact v_r=0",s.referenceVelocity.lengthSquared(),0.f);
            if (s.referenceVelocity.lengthSquared()==0.f) stop.brake=0.f;
        }
        const auto& h=LLFlycamDrone::presets()[pi].human;
        F32 eps=std::min(p.effectiveJerk*p.tick*p.tick/4.f,.005f)/4.f;
        F32 tolerance=std::max(.001f,1.5f*eps*h.onset);
        ensure("T6 camera d_stop lower", s.position.mV[VX] >= h.stopDistance-tolerance-1.e-6f);
        ensure("T6 camera d_stop upper", s.position.mV[VX] <= h.stopDistance+1.e-6f);
        ensure("T6 did settle",s.settled);
        ensure_equals("T6 v_r exactly zero on settle", s.referenceVelocity.lengthSquared(), 0.f);
        F64 extra=s.position.mV[VX]-massDistance;
        ensure("T6 unity-DC extra upper",extra<=p.top*h.onset+1.e-6);
        ensure("T6 unity-DC extra lower",extra>=p.top*h.onset-1.5f*eps*h.onset-1.e-6);

        LLFlycamDrone::State edge=d.state();edge.pole=p.pole;edge.totalLimitMax=p.totalLimit;
        edge.smootherVelocity.set(eps,0,0);edge.velocity.set(2.f*eps,0,0);edge.deltaVelocity.set(eps,0,0);
        auto snapped=d.step(edge,LLFlycamDrone::Input(),p.tick);
        ensure("T6 adversarial epsilon_q settle",snapped.snapped&&snapped.settled);
        const F32 adversarialTail=(eps+2.f*eps)*.5f*h.onset;
        ensure("T6 adversarial tail <= 1.5 eps_q t_j",adversarialTail<=1.5f*eps*h.onset*FP);
    }
}

template<> template<> void flycam_drone_object::test<7>()
{
    for (std::size_t pi=0; pi<LLFlycamDrone::presets().size(); ++pi) for(F32 hz:HZS)
    { auto p=compiled(pi,hz); p.release=LLFlycamDrone::ReleaseMode::HOLD; LLFlycamDrone d(p); const auto& h=LLFlycamDrone::presets()[pi].human;
      LLFlycamDrone::State s=d.state(); s.referenceVelocity.set(h.cruise,0,0); s.smootherVelocity=s.velocity=s.referenceVelocity;
      LLFlycamDrone::Input z;const auto mass=crossing(d,s,z,.1f*h.cruise,p.tick,VX,false,false);const auto camera=crossing(d,s,z,.1f*h.cruise,p.tick,VX,true,false);
      assert_timing("T7 hold interpolated","T7 hold raw +/-h","T7 camera hold+t_j",mass,camera,h.holdTime,h.onset,p.tick); }
}

template<> template<> void flycam_drone_object::test<8>()
{
    for (std::size_t pi=0; pi<LLFlycamDrone::presets().size(); ++pi) for(F32 hz:HZS)
    { auto p=compiled(pi,hz); p.release=LLFlycamDrone::ReleaseMode::DRIFT; LLFlycamDrone d(p);
      const auto& h=LLFlycamDrone::presets()[pi].human; LLFlycamDrone::State s=d.state();
      s.referenceVelocity.set(h.cruise,0,0); s.smootherVelocity=s.velocity=s.referenceVelocity;
      LLFlycamDrone::Input z;const auto mass=crossing(d,s,z,.1f*h.cruise,p.tick,VX,false,false);const auto camera=crossing(d,s,z,.1f*h.cruise,p.tick,VX,true,false);
      assert_timing("T8 coast interpolated","T8 coast raw +/-h","T8 camera coast+t_j",mass,camera,h.coastTime,h.onset,p.tick); }
}

template<> template<> void flycam_drone_object::test<9>()
{
    for(std::size_t pi=0;pi<LLFlycamDrone::presets().size();++pi)for(F32 hz:HZS)
    { auto p=compiled(pi,hz);const auto& h=LLFlycamDrone::presets()[pi].human;if(h.climb==0)continue;
      LLFlycamDrone d(p);LLFlycamDrone::Input in;in.vertical=1;const auto mass=crossing(d,d.state(),in,.9f*h.climb,p.tick,VZ,false,true);const auto camera=crossing(d,d.state(),in,.9f*h.climb,p.tick,VZ,true,true);
      assert_timing("T9 climb interpolated","T9 climb raw +/-h","T9 camera climb+t_j",mass,camera,h.climbTime,h.onset,p.tick);
      LLFlycamDrone::Params q;LLFlycamDrone::compile(h,h,p.tick,2.f,q);ensure("T9 deterministic compile all coefficients",exact_params(p,q)); }
}

template<> template<> void flycam_drone_object::test<10>()
{
    std::mt19937 rng(0x16A1u); std::uniform_real_distribution<F32> u(-1.f,1.f);
    for(S32 run=0;run<10000;++run)
    {
      auto p=compiled(run%5,HZS[run%4]);LLFlycamDrone d(p);
      if((run&1)!=0){p.throttleMode=LLFlycamDrone::ThrottleMode::PURE;d.setParams(p);}
      if((run%11)==0)d.seed(LLVector3::zero,LLQuaternion(),LLVector3(p.velocityCap*(u(rng)+1.f)*.5f,0,0));
      auto a=d.state();
      for(S32 tick=0;tick<20;++tick)
      {
        if(tick==7){p=compiled((run+3)%5,HZS[run%4]);p.release=(run&1)?LLFlycamDrone::ReleaseMode::HOLD:LLFlycamDrone::ReleaseMode::DRIFT;d.setParams(p);}
        LLFlycamDrone::Input in;
        in.droneThrottle=(tick%5)!=0; // false covers ROLL/takeover-loss/stale fallback to Assist
        if((tick%6)!=0){in.move.set(u(rng),u(rng),0);in.throttle=(u(rng)+1)/2;in.brake=(u(rng)+1)/2;in.vertical=u(rng);in.yaw=u(rng);in.tilt=u(rng);in.roll=u(rng);}
        in.recentre=(tick==3); // includes the latch and its cancellation on the next nonzero tilt sample
        auto b=d.step(a,in,p.tick);assert_g1("T10 acceleration",a,b,p,p.tick);
        ensure("T10 stage target no overshoot",b.referenceVelocity.length()<=std::max(a.referenceVelocity.length(),p.velocityCap)*FP+1.e-6f);
        a=b;
      }
    }

    // R3 counter-example class: high acceleration followed by a slightly raised target.
    auto p=compiled(4,120);LLFlycamDrone d(p);auto s=d.state();auto high=forward(.8f);
    s=run_steps(d,s,high,60,p.tick);F32 before=s.referenceVelocity.length();high.throttle=.81f;
    auto raised=d.step(s,high,p.tick);F32 target=p.cruise+(p.top-p.cruise)*high.throttle;
    ensure("T10 R3 target overshoot zero",raised.referenceVelocity.length()<=target+1.e-6f&&raised.referenceVelocity.length()>=before-1.e-6f);
    auto edge=d.state();const F32 eps=std::min(p.effectiveJerk*p.tick*p.tick/4.f,.005f)/4.f;edge.smootherVelocity.set(eps,0,0);edge.velocity.set(2*eps,0,0);edge.deltaVelocity.set(eps,0,0);auto snap=d.step(edge,LLFlycamDrone::Input(),p.tick);ensure("T10 fuzz event class includes settle snap",snap.snapped&&snap.settled);assert_g1("T10 settle G1",edge,snap,p,p.tick);
}

template<> template<> void flycam_drone_object::test<11>()
{
    for(std::size_t pi=0;pi<5;++pi)for(auto mode:{LLFlycamDrone::ReleaseMode::HOLD,LLFlycamDrone::ReleaseMode::DRIFT})
    {
      auto p=compiled(pi,120);p.release=mode;LLFlycamDrone straight(p);auto one=run_steps(straight,straight.state(),forward(),(S32)(8*LLFlycamDrone::presets()[pi].human.onset/p.tick)+1,p.tick);const LLVector3 oneRay=one.referenceVelocity/one.referenceVelocity.length();LLFlycamDrone::Input oneStop;if(mode==LLFlycamDrone::ReleaseMode::HOLD)oneStop.brake=1.f;
      for(S32 n=0;n<5000&&!one.settled;++n){one=straight.step(one,oneStop,p.tick);ensure("T11 one-direction camera no reversal",one.velocity*oneRay>=0.f);ensure("T11 one-direction stage no reversal",one.referenceVelocity*oneRay>=0.f);}
      LLFlycamDrone d(p);auto s=run_steps(d,d.state(),forward(),(S32)(8*LLFlycamDrone::presets()[pi].human.onset/p.tick)+1,p.tick);
      auto reverse=forward();reverse.move.set(-1,0,0);s=run_steps(d,s,reverse,std::max(1,(S32)(2*LLFlycamDrone::presets()[pi].human.onset/p.tick)),p.tick);
      LLVector3 ray=s.referenceVelocity/s.referenceVelocity.length();const F32 historyMax=p.velocityCap;LLFlycamDrone::Input stop;if(mode==LLFlycamDrone::ReleaseMode::DRIFT)stop.brake=0.f;else stop.brake=1.f;
      bool converged=false;F32 priorBound=historyMax;
      for(S32 n=0;n<5000&&!s.settled;++n){s=d.step(s,stop,p.tick);const F32 tail=std::pow(s.pole,(F32)(n+1))*(1.f+(n+1)*(1.f-s.pole));const F32 bound=tail*historyMax;
        ensure("T11 stage no reversal",s.referenceVelocity*ray>=0.f);ensure("T11 mixed-history tail bound",s.velocity*ray>=-bound*FP);
        ensure("T11 tail bound decays",bound<=priorBound*FP);priorBound=bound;if(std::fabs(s.velocity*ray)<1.e-4f)converged=true;}
      ensure("T11 mixed residue converges",converged);
    }
}

template<> template<> void flycam_drone_object::test<12>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);auto s=d.state();s.referenceVelocity.set(2*p.top,0,0);s.smootherVelocity=s.velocity=s.referenceVelocity;
    F32 previous=s.velocity.length();bool converged=false;for(S32 n=0;n<2000;++n){auto q=d.step(s,forward(),p.tick);ensure("T12 monotone overspeed",q.velocity.length()<=previous+1.e-6f);assert_g1("T12 G1",s,q,p,p.tick);previous=q.velocity.length();s=q;if(std::fabs(previous-p.top)<=1.e-4f){converged=true;break;}}ensure("T12 converges to target",converged);ensure("T12 no undershoot",previous>=p.top-1.e-4f);
}

template<> template<> void flycam_drone_object::test<13>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);auto s=run_steps(d,d.state(),forward(),1000,p.tick);LLFlycamDrone::Input back=forward();back.move.set(-1,0,0);S32 crossings=0,zeroBand=0;F32 previous=s.velocity.mV[VX];
    for(S32 n=0;n<3000;++n){auto q=d.step(s,back,p.tick);if(previous>0&&q.velocity.mV[VX]<=0)++crossings;if(std::fabs(q.velocity.mV[VX])<1.e-5f)++zeroBand;ensure("T13 no snap",!q.snapped);assert_g1("T13 G1",s,q,p,p.tick);previous=q.velocity.mV[VX];s=q;}ensure_equals("T13 one zero crossing",crossings,1);ensure("T13 no stall beyond natural smoother passage",zeroBand<=(S32)std::ceil(8.f*LLFlycamDrone::presets()[1].human.onset/p.tick));
}

template<> template<> void flycam_drone_object::test<14>()
{
    for(std::size_t pi=0;pi<5;++pi)for(F32 hz:HZS){auto h=LLFlycamDrone::presets()[pi].human;h.grip=.001f;LLFlycamDrone::Params p;LLFlycamDrone::HumanParams accepted;LLFlycamDrone::compile(h,LLFlycamDrone::presets()[pi].human,1.f/hz,2,p,&accepted);
      ensure_equals("T14 grip validation floor",accepted.grip,.05f);LLFlycamDrone d(p);auto s=d.state();s.referenceVelocity.set(1,1,0);LLFlycamDrone::Input along=forward();
      const F32 before=std::fabs(s.referenceVelocity.mV[VY]);auto q=d.step(s,along,p.tick);ensure("T14 finite",finite(q.referenceVelocity));
      close("T14 sideways linear G*A_max decay",before-std::fabs(q.referenceVelocity.mV[VY]),accepted.grip*p.acceleration*p.tick,2.f*std::numeric_limits<F32>::epsilon()*before);}
}

template<> template<> void flycam_drone_object::test<15>()
{
    auto normal=compiled(1,120),fpv=compiled(4,120),dolly=compiled(3,120);
    auto edited=[](LLFlycamDrone::Params base,F32 onset){base.pole=onset/(onset+2.f*base.tick);base.gMax=LLFlycamDrone::gMax(base.pole);base.effectiveJerk=LLFlycamDrone::effectiveJerk(base.pole,base.totalLimit,base.tick);return base;};
    ensure("T15 adversarial L_tot drop >=30x",fpv.totalLimit>=30.f*dolly.totalLimit);
    bool noSecondSnap=true;

    const auto exercise=[&](const char* label,const LLFlycamDrone::Params& before,const LLFlycamDrone::Params& after,bool settle,bool rebaseline,bool pending)
    {
        LLFlycamDrone d(before);auto s=d.state();s.pole=before.pole;s.totalLimitMax=rebaseline?std::max(before.totalLimit,after.totalLimit):after.totalLimit;
        if(pending)d.setParams(after);else {auto samePole=after;samePole.pole=before.pole;samePole.gMax=LLFlycamDrone::gMax(before.pole);samePole.effectiveJerk=LLFlycamDrone::effectiveJerk(before.pole,after.totalLimit,after.tick);d.setParams(samePole);}
        const F32 poleAfter=pending?after.pole:before.pole;const F32 jBefore=LLFlycamDrone::effectiveJerk(before.pole,std::max(s.totalLimitMax,after.totalLimit),after.tick);
        const F32 jAfter=LLFlycamDrone::effectiveJerk(poleAfter,after.totalLimit,after.tick);const F32 jStar=std::min(jBefore,jAfter);
        const F32 eps=std::min(jStar*after.tick*after.tick/4.f,.005f)/4.f;
        s.referenceVelocity.setZero();s.smootherVelocity.set(eps,0,0);s.velocity.set(2.f*eps,0,0);s.deltaVelocity.set(eps,0,0);
        LLFlycamDrone::Input in;if(!settle)in.yaw=.25f;
        const auto q=d.step(s,in,after.tick);ensure(label,q.snapped);ensure("T15 one atomic snap",!q.hasPendingPole);ensure_equals("T15 reset L_tot max",q.totalLimitMax,after.totalLimit);
        ensure("T15 snap tick <=3/16 Jstar",mag(q.deltaVelocity-s.deltaVelocity)/(after.tick*after.tick)<=3.f/16.f*jStar*FP);
        ensure_equals("T15 settle iff zero reference/input",q.settled,settle);
        const auto r=d.step(q,in,after.tick);noSecondSnap=noSecondSnap&&!r.snapped;
        ensure("T15 next tick <=5/8 Jafter",mag(r.deltaVelocity-q.deltaVelocity)/(after.tick*after.tick)<=5.f/8.f*jAfter*FP);
    };

    exercise("T15 real rebaseline",fpv,dolly,false,true,false);
    exercise("T15 real pole switch .05->1.0",edited(normal,.05f),edited(normal,1.f),false,false,true);
    exercise("T15 combined pending pole + rebaseline",edited(fpv,.05f),edited(dolly,1.f),false,true,true);
    exercise("T15 combined pending pole at settle",edited(normal,.05f),edited(normal,1.f),true,false,true);
    exercise("T15 settle exact epsilon_q",normal,normal,true,false,false);

    // Meeting only the looser pre-change epsilon must not authorize the snap.
    LLFlycamDrone refusal(fpv);refusal.setParams(edited(dolly,1.f));auto s=refusal.state();s.totalLimitMax=fpv.totalLimit;
    const F32 preJ=LLFlycamDrone::effectiveJerk(fpv.pole,fpv.totalLimit,dolly.tick);const F32 postJ=LLFlycamDrone::effectiveJerk(edited(dolly,1.f).pole,dolly.totalLimit,dolly.tick);
    const F32 preEps=std::min(preJ*dolly.tick*dolly.tick/4.f,.005f)/4.f,postEps=std::min(postJ*dolly.tick*dolly.tick/4.f,.005f)/4.f;
    ensure("T15 pre bound is actually looser",preEps>postEps);const F32 onlyPre=(preEps+postEps)*.5f;
    s.smootherVelocity.set(onlyPre,0,0);s.velocity.set(2.f*onlyPre,0,0);s.deltaVelocity.set(onlyPre,0,0);LLFlycamDrone::Input yaw;yaw.yaw=.2f;
    ensure("T15 refuses pre-bound-only eligibility",!refusal.step(s,yaw,dolly.tick).snapped);
    ensure("T15 no second snap on following tick",noSecondSnap);
}

template<> template<> void flycam_drone_object::test<16>()
{
    std::vector<std::string> logs;auto good=LLFlycamDrone::presets()[1].human,bad=good;bad.top=std::numeric_limits<F32>::quiet_NaN();LLFlycamDrone::Params p;bool ok=LLFlycamDrone::compile(bad,good,1.f/120,2,p,nullptr,[&](const std::string&s){logs.push_back(s);});ensure("T16 rejects bad setting",!ok&&!logs.empty());
    LLFlycamDrone d(p,[&](const std::string&s){logs.push_back(s);});auto s=d.state();s.position.mV[VX]=std::numeric_limits<F32>::quiet_NaN();auto q=d.step(s,forward(),p.tick);ensure("T16 NONFINITE recovered",q.nonFiniteRecovered&&finite(q.position));
    LLFlycamDrone::Input badInput;badInput.move.set(std::numeric_limits<F32>::quiet_NaN(),std::numeric_limits<F32>::infinity(),0);badInput.throttle=std::numeric_limits<F32>::quiet_NaN();badInput.brake=std::numeric_limits<F32>::infinity();badInput.droneThrottle=true;auto safe=d.step(d.state(),badInput,p.tick);ensure("T16 NaN/Inf input stays finite",finite(safe.referenceVelocity)&&finite(safe.velocity));
}

template<> template<> void flycam_drone_object::test<17>()
{
    for(std::size_t pi=0;pi<5;++pi){auto a=compiled(pi,30),b=compiled(pi,240);ensure("T17 near-independent jerk",std::fabs(a.effectiveJerk-b.effectiveJerk)/std::max(a.effectiveJerk,b.effectiveJerk)<.25f);for(auto p:{a,b}){LLFlycamDrone d(p);auto s=d.state();for(S32 n=0;n<1000;++n){auto in=forward((n%17)/16.f);in.move.set(std::cos(n*.13f),std::sin(n*.13f),0);auto q=d.step(s,in,p.tick);assert_g1("T17 extreme-Hz G1",s,q,p,p.tick);s=q;}}}
}

template<> template<> void flycam_drone_object::test<18>()
{
    auto p=compiled(1,120);LLFlycamDrone locked(p);locked.seed(LLVector3::zero,LLFlycamDrone::rotation(0,-80*DEG,0));auto a=run_steps(locked,locked.state(),forward(),240,p.tick);ensure("T18 horizon delta z",std::fabs(a.position.mV[VZ])<=1.e-6f);
    p.freeFly=true;LLFlycamDrone free(p);free.seed(LLVector3::zero,LLFlycamDrone::rotation(0,-80*DEG,0));auto b=run_steps(free,free.state(),forward(),240,p.tick);ensure("T18 free fly z",std::fabs(b.position.mV[VZ])>1.e-3f);
}

template<> template<> void flycam_drone_object::test<19>()
{
    std::mt19937 rng(19);std::uniform_real_distribution<F32> a(-PI,PI),t(-PI/2,PI/2);auto p=compiled(1,120);LLFlycamDrone d(p);
    for(S32 i=0;i<10000;++i){LLQuaternion q=LLFlycamDrone::rotation(a(rng),t(rng),(i&1)?a(rng):0.f);d.seed(LLVector3::zero,q);quaternion_close("T19 reconstruction with/without roll",d.renderPose().rotation,q,.05f*DEG);}
}

template<> template<> void flycam_drone_object::test<20>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);LLFlycamDrone::State prior;prior.yaw=.7f;
    LLQuaternion last;bool have=false;for(F32 deg=85;deg<=95;deg+=.1f){auto q=LLFlycamDrone::rotation(.7f,deg*DEG,.2f);if(((S32)(deg*10))&1)for(S32 i=0;i<4;++i)q.mQ[i]=-q.mQ[i];d.seed(LLVector3::zero,q,LLVector3::zero,LLVector3d::zero,&prior);if(std::fabs(deg-90.f)<.001f)ensure("T20 prior yaw at pole",std::fabs(d.state().yaw-prior.yaw)<.02f);ensure("T20 canonical quaternion sign",dot(d.renderPose().rotation,q)>=0.f);if(have)quaternion_close("T20 continuity",last,d.renderPose().rotation,.2f*DEG);last=d.renderPose().rotation;have=true;prior=d.state();}
    const auto held=d.renderPose();auto s=d.state();for(S32 n=0;n<120;++n)s=d.step(s,LLFlycamDrone::Input(),p.tick);vector_close("T20 zero-input position does not drift",s.position,d.state().position,1.e-6f);quaternion_close("T20 zero-input pose does not pop",LLFlycamDrone::rotation(s.yaw,s.tilt,s.roll),held.rotation,1.e-6f);
}

template<> template<> void flycam_drone_object::test<21>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);d.seed(LLVector3::zero,LLFlycamDrone::rotation(0,89.7f*DEG,0));ensure("T21 keeps seeded limit",d.state().tiltLimit>=89.7f*DEG);LLFlycamDrone::Input up;up.tilt=1;auto s=run_steps(d,d.state(),up,120,p.tick);ensure("T21 cannot increase",std::fabs(s.tilt)<=89.7f*DEG+1.e-6f);up.tilt=-1;for(S32 n=0;n<1000&&std::fabs(s.tilt)>88.9f*DEG;++n)s=d.step(s,up,p.tick);s=d.step(s,up,p.tick);close("T21 limit drops to 89",s.tiltLimit,89.f*DEG,1.e-6);
}

template<> template<> void flycam_drone_object::test<22>()
{
    auto prev=LLFlycamDrone::presets()[1].human,bad=prev;bad.timeToTop=.05f;bad.onset=1.f;bad.stopDistance=.01f;bad.highSpeedFloat=0;bad.grip=0;bad.climb=0.f;bad.climbTime=0.f;LLFlycamDrone::Params p;LLFlycamDrone::HumanParams accepted;std::vector<std::string> logs;
    LLFlycamDrone::compile(bad,prev,1.f/30,2,p,&accepted,[&](const std::string&s){logs.push_back(s);});
    ensure("T22 min times",accepted.timeToTop>=4.f/30.f);ensure("T22 unconditional climbTime validation",accepted.climbTime>=4.f/30.f);ensure("T22 grip floor",accepted.grip>=.05f);close("T22 exact stop floor",accepted.stopDistance,accepted.top*(accepted.onset+2.f/30.f),1.e-6);ensure("T22 logs",!logs.empty());
    LLFlycamDrone d(p);auto s=d.state();for(S32 n=0;n<2000;++n){auto in=forward((n%11)/10.f);in.move.set(std::cos(n*.17f),std::sin(n*.17f),0);auto q=d.step(s,in,p.tick);assert_g1("T22 clamped-settings G1",s,q,p,p.tick);s=q;}
}

template<> template<> void flycam_drone_object::test<23>()
{
    const F32 scale=.25f;auto h=LLFlycamDrone::presets()[1].human,hs=h;hs.cruise*=scale;hs.top*=scale;hs.climb*=scale;hs.stopDistance*=scale;
    LLFlycamDrone::Params p,q;LLFlycamDrone::compile(h,h,1.f/120,2,p);LLFlycamDrone::compile(hs,hs,1.f/120,2,q);LLFlycamDrone normal(p),scaled(q);
    auto a=run_steps(normal,normal.state(),forward(),600,p.tick),b=run_steps(scaled,scaled.state(),forward(),600,q.tick);
    close("T23 calibrated time unchanged",a.velocity.length()/p.top,b.velocity.length()/q.top,1.e-6);close("T23 distance scales by s",b.position.length(),a.position.length()*scale,1.e-4);
}

template<> template<> void flycam_drone_object::test<24>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);for(bool bd:{false,true})for(bool invert:{false,true})for(F32 axisScale:{-1.f,1.f})for(S32 axis=0;axis<2;++axis)for(F32 raw:{-1.f,1.f})
    {const F32 classicSign=raw*axisScale;LLFlycamDrone::Input in;in.move.mV[axis]=classicSign;auto q=d.step(d.state(),in,p.tick);ensure("T24 translation nonzero",q.referenceVelocity.mV[axis]!=0.f);ensure("T24 Classic BD/non-BD translation parity",q.referenceVelocity.mV[axis]*classicSign>0.f);(void)bd;(void)invert;}
    for(bool bd:{false,true})for(bool invert:{false,true})for(F32 axisScale:{-1.f,1.f})for(F32 raw:{-1.f,1.f}){const F32 classicPitch=(-raw)*(bd?1.f:(invert?1.f:-1.f))*axisScale;LLFlycamDrone::Input in;in.tilt=classicPitch;auto q=d.step(d.state(),in,p.tick);ensure("T24 pitch nonzero",q.tiltVelocity!=0.f);ensure("T24 Classic pitch parity",q.tiltVelocity*classicPitch>0.f);}
    for(F32 sign:{-1.f,1.f}){LLFlycamDrone::Input in;in.vertical=sign;auto z=d.step(d.state(),in,p.tick);ensure("T24 vertical nonzero/sign",z.referenceVelocity.mV[VZ]*sign>0.f);in=LLFlycamDrone::Input();in.yaw=sign;ensure("T24 yaw nonzero/sign",d.step(d.state(),in,p.tick).yawVelocity*sign>0.f);in=LLFlycamDrone::Input();in.roll=sign;ensure("T24 roll nonzero/sign",d.step(d.state(),in,p.tick).rollVelocity*sign>0.f);}
}

template<> template<> void flycam_drone_object::test<25>()
{
    auto p=compiled(1,120);p.throttleMode=LLFlycamDrone::ThrottleMode::PURE;LLFlycamDrone d(p);
    LLFlycamDrone::Input stale;stale.droneThrottle=false;auto stopped=d.step(d.state(),stale,p.tick);ensure_equals("T25 stale trigger is neutral",stopped.referenceVelocity.lengthSquared(),0.f);
    LLFlycamDrone::Input move=forward(0.f);move.droneThrottle=true;auto pure=d.step(d.state(),move,p.tick);ensure_equals("T25 Pure zero throttle is zero target",pure.referenceVelocity.lengthSquared(),0.f);
    move.droneThrottle=false;auto fallback=d.step(d.state(),move,p.tick);ensure("T25 Pure falls back to Assist outside Drone THROTTLE",fallback.referenceVelocity.length()>0.f);
}

template<> template<> void flycam_drone_object::test<26>()
{
    // Pure seam used by T26/T27 after LLViewerJoystick selects the outgoing pose/velocity.
    auto p=compiled(1,120);LLFlycamDrone d(p);LLVector3 v(3,1,0);d.seed(LLVector3(1,2,3),LLQuaternion(),v);vector_close("T26/T27 seed velocity seam",d.state().velocity,v,1.e-6f);vector_close("T26/T27 seed pose seam",d.renderPose().position,LLVector3(1,2,3),1.e-6f);
}
template<> template<> void flycam_drone_object::test<27>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);d.seed(LLVector3::zero,LLQuaternion(),LLVector3(2,1,.5f));auto seed=d.classicSeed(1.f/144.f);close("T28 first Classic displacement x",seed[0],2.f/144.f,1.e-6);close("T28 first Classic displacement y",seed[1],1.f/144.f,1.e-6);
}

template<> template<> void flycam_drone_object::test<28>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);LLVector3 v(2,1,.5);d.seed(LLVector3::zero,LLQuaternion(),v);auto seed=d.classicSeed(1.f/60);close("T28 displacement x",seed[0],v.mV[VX]/60.f,1.e-6);close("T28 rotation rate",seed[3],0,1.e-6);
}

template<> template<> void flycam_drone_object::test<29>()
{
    // Orbit-owner frames freeze dynamics and discard only unsimulated wall time.
    auto p=compiled(1,120);LLFlycamDrone d(p);d.advanceRaw(2.5*p.tick,forward());const auto frozen=d.state();d.discardAccumulator();ensure_equals("T29 orbit discards accumulator",d.accumulator(),0.);ensure("T29 orbit freezes state",exact_state(d.state(),frozen));
    LLVector3d a(0,0,0),b(100,0,0);d.seedFromHistory(a,LLQuaternion(),b,LLQuaternion(),.01,LLVector3d::zero);ensure("T29 falling-edge history caps momentum",d.state().velocity.length()<=p.velocityCap*FP);
}

template<> template<> void flycam_drone_object::test<30>()
{
    // Dispatch classification is viewer-owned; Stop's public reseed semantics are testable here.
    auto p=compiled(1,120);LLFlycamDrone d(p);d.seed(LLVector3(4,5,6),LLQuaternion(),LLVector3::zero);vector_close("T30 Stop pose",d.renderPose().position,LLVector3(4,5,6),1.e-6f);ensure_equals("T30 Stop velocity",d.state().velocity.lengthSquared(),0.f);
}

template<> template<> void flycam_drone_object::test<31>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);d.seed(LLVector3::zero,LLFlycamDrone::rotation(.2f,.5f,.1f));auto in=forward();in.yaw=.3f;in.roll=.2f;in.recentre=true;d.advanceRaw(50.25*p.tick,in);
    const auto before=d.previousState();const auto after=d.state();const F32 alpha=(F32)(d.accumulator()/p.tick);const auto lerp=[alpha](F32 a,F32 b){return a+(b-a)*alpha;};const auto lerpv=[alpha](const LLVector3& a,const LLVector3& b){return a+(b-a)*alpha;};const auto pose=d.renderPose();d.pauseAtRenderPose();const auto& paused=d.state();
    vector_close("T31 SELF Keep resumes rendered pose",d.renderPose().position,pose.position,1.e-6f);ensure("T31 pause prev=cur",exact_state(d.previousState(),paused));ensure_equals("T31 gap wall time discarded",d.accumulator(),0.);
    vector_close("T31 position interpolation",paused.position,lerpv(before.position,after.position),1.e-6f);vector_close("T31 reference interpolation",paused.referenceVelocity,lerpv(before.referenceVelocity,after.referenceVelocity),1.e-6f);vector_close("T31 y1 interpolation",paused.smootherVelocity,lerpv(before.smootherVelocity,after.smootherVelocity),1.e-6f);vector_close("T31 velocity interpolation",paused.velocity,lerpv(before.velocity,after.velocity),1.e-6f);vector_close("T31 delta-v interpolation",paused.deltaVelocity,lerpv(before.deltaVelocity,after.deltaVelocity),1.e-6f);
    close("T31 yaw interpolation",paused.yaw,before.yaw+std::remainder(after.yaw-before.yaw,2.f*PI)*alpha,1.e-6);close("T31 yaw-rate interpolation",paused.yawVelocity,lerp(before.yawVelocity,after.yawVelocity),1.e-6);close("T31 tilt interpolation",paused.tilt,before.tilt+std::remainder(after.tilt-before.tilt,2.f*PI)*alpha,1.e-6);close("T31 tilt-rate interpolation",paused.tiltVelocity,lerp(before.tiltVelocity,after.tiltVelocity),1.e-6);close("T31 roll interpolation",paused.roll,before.roll+std::remainder(after.roll-before.roll,2.f*PI)*alpha,1.e-6);close("T31 roll-rate interpolation",paused.rollVelocity,lerp(before.rollVelocity,after.rollVelocity),1.e-6);close("T31 tilt-limit interpolation",paused.tiltLimit,lerp(before.tiltLimit,after.tiltLimit),1.e-6);ensure_equals("T31 L_tot max preserves both endpoints",paused.totalLimitMax,std::max(before.totalLimitMax,after.totalLimitMax));
    ensure("T31 current discrete state",paused.quaternionSign==after.quaternionSign&&paused.pole==after.pole&&paused.pendingPole==after.pendingPole&&paused.hasPendingPole==after.hasPendingPole&&paused.recentre==after.recentre&&paused.snapped==after.snapped&&paused.settled==after.settled&&paused.nonFiniteRecovered==after.nonFiniteRecovered);
    const auto next=d.step(paused,in,p.tick);assert_g1("T31 next step G1 across SELF gap",paused,next,p,p.tick);
}

template<> template<> void flycam_drone_object::test<32>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);LLVector3d a(1,2,3),b(1.2,2.4,3.6);d.seedFromHistory(a,LLQuaternion(),b,LLQuaternion(),.2,LLVector3d::zero);vector_close("T32 constant-u Keep",d.state().velocity,LLVector3(1,2,3),1.e-4f);ensure_equals("T32 steady acceleration",d.state().deltaVelocity.lengthSquared(),0.f);
}

// T26/T27 edge selection and fps sampling; T29 rising-edge ownership; T30
// dispatch-serial SELF/FOREIGN/no-gap/minimized/multi-pass classification;
// T32 constant-acceleration three-sample estimation; and T33/T34 cut,
// straddle, stale and PRESENTED rejection live only inside LLViewerJoystick.
// They are intentionally deferred to the in-world/integration list instead of
// being faked by calling LLFlycamDrone::seed() directly.

template<> template<> void flycam_drone_object::test<35>()
{
    // The sampler owns the 0.0005/0.25 eligibility window; seedFromHistory can pin its hard-invalid subset.
    auto p=compiled(1,120);LLFlycamDrone d(p);F64 bad[]={0.,-1.,std::numeric_limits<F64>::quiet_NaN()};for(F64 dt:bad){d.seedFromHistory(LLVector3d::zero,LLQuaternion(),LLVector3d(1,0,0),LLQuaternion(),dt,LLVector3d::zero);ensure("T35 hard-invalid dt finite Stop",finite(d.state().velocity)&&d.state().velocity.lengthSquared()==0.f);}
}

template<> template<> void flycam_drone_object::test<36>()
{
    for(auto mode:{LLFlycamDrone::ReleaseMode::HOLD,LLFlycamDrone::ReleaseMode::DRIFT}){auto p=compiled(1,120);p.release=mode;LLFlycamDrone d(p);d.seed(LLVector3::zero,LLQuaternion(),LLVector3(5,0,0));auto s=d.state();F32 last=5;for(S32 n=0;n<2000;++n){s=d.step(s,LLFlycamDrone::Input(),p.tick);ensure("T36 no reversal",s.velocity.mV[VX]>=-1.e-6f);ensure("T36 monotone",s.velocity.length()<=last+1.e-5f);last=s.velocity.length();}}
}

template<> template<> void flycam_drone_object::test<37>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);d.seed(LLVector3(7,8,9),LLQuaternion(),LLVector3(2,0,0));d.seed(LLVector3(1,2,3),LLQuaternion());vector_close("T37 reset Stop pose",d.state().position,LLVector3(1,2,3),1.e-6f);ensure_equals("T37 reset Stop velocity",d.state().velocity.lengthSquared(),0.f);
}

template<> template<> void flycam_drone_object::test<38>()
{
    auto p=compiled(1,120);F32 y,t,r;LLQuaternion a=LLFlycamDrone::rotation(0,0,0),b=LLFlycamDrone::rotation(.1f,.05f,.02f);LLFlycamDrone::seededRates(a,b,.1,0,.05f,p,y,t,r);ensure("T38 yaw clamp",std::fabs(y)<=p.yawRate);ensure("T38 tilt clamp",std::fabs(t)<=p.tiltRate);ensure("T38 roll clamp",std::fabs(r)<=p.rollRate);
}

template<> template<> void flycam_drone_object::test<39>()
{
    // Operator ownership is outside this class; pin the uninterrupted SELF-state half of the contract.
    auto p=compiled(1,120);LLFlycamDrone d(p);auto before=run_steps(d,d.state(),forward(),100,p.tick);d.seed(before.position,LLFlycamDrone::rotation(before.yaw,before.tilt,before.roll),before.velocity,LLVector3d::zero,&before);vector_close("T42 own pose continuous",d.renderPose().position,before.position,1.e-4f);
}

template<> template<> void flycam_drone_object::test<40>()
{
    auto p=compiled(1,120);F32 py=0,pt=0,pr=0;LLQuaternion previous=LLFlycamDrone::rotation(0,84.9f*DEG,0);for(F32 deg=85;deg<=95;deg+=.1f){auto current=LLFlycamDrone::rotation(0,deg*DEG,0);F32 y,t,r;LLFlycamDrone::seededRates(previous,current,.01,0,deg*DEG,p,y,t,r);ensure("T43 no 180 rate jump",std::fabs(y-py)<PI&&std::fabs(t-pt)<PI&&std::fabs(r-pr)<PI);py=y;pt=t;pr=r;previous=current;}
}

template<> template<> void flycam_drone_object::test<41>()
{
    auto p=compiled(1,120);LLQuaternion a=LLFlycamDrone::rotation(.2f,.3f,.1f);LLVector3 omega(.1f,.2f,.3f);LLQuaternion dq;dq.setAngleAxis(omega.length()*.001f,omega/omega.length());LLQuaternion b=dq*a;F32 y,t,r;LLFlycamDrone::seededRates(a,b,.001,.2f,.3f,p,y,t,r);LLVector3 reconstructed(std::cos(.2f)*r,std::sin(.2f)*r,0);reconstructed+=LLVector3(-std::sin(.2f),std::cos(.2f),0)*t;reconstructed.mV[VZ]+=y+r*std::sin(.3f);vector_close("T44 omega projection",reconstructed,omega,1.e-4f);
}

template<> template<> void flycam_drone_object::test<42>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);d.seed(LLVector3::zero,LLFlycamDrone::rotation(0,.5f,0));LLFlycamDrone::Input in;in.recentre=true;auto s=d.step(d.state(),in,p.tick);ensure("T45 latch starts",s.recentre);auto repeat=d.step(s,in,p.tick);ensure("T45 repeated edge is idempotent",repeat.recentre);auto changed=compiled(2,120);d.setParams(changed);auto preserved=d.step(repeat,LLFlycamDrone::Input(),p.tick);ensure("T45 preset switch preserves latch",preserved.recentre);s=preserved;in.recentre=false;for(S32 n=0;n<5000&&s.recentre;++n){F32 old=std::fabs(s.tilt);s=d.step(s,in,p.tick);ensure("T45 no overshoot",std::fabs(s.tilt)<=old+1.e-5f);}ensure("T45 completes",!s.recentre&&s.tilt==0);in.recentre=true;s=d.step(s,in,p.tick);in.recentre=false;in.tilt=.1f;s=d.step(s,in,p.tick);ensure("T45 tilt cancels",!s.recentre);d.seed(LLVector3::zero,LLQuaternion());ensure("T45 reset/regain seed clears latch",!d.state().recentre);
}

template<> template<> void flycam_drone_object::test<43>()
{
    std::mt19937 rng(46);std::uniform_real_distribution<F32> top(.1f,40),onset(.05f,1),extra(.1f,100);
    for(S32 i=0;i<10000;++i){auto h=LLFlycamDrone::presets()[1].human;h.cruise=std::min(h.cruise,top(rng));h.top=std::max(h.cruise,top(rng));h.onset=onset(rng);h.stopDistance=h.top*(h.onset+2.f/120)+extra(rng);LLFlycamDrone::Params p;LLFlycamDrone::compile(h,h,1.f/120,2,p);
      close_relative("T46 tick",p.tick,1.f/120,1.e-9);close_relative("T46 cruise",p.cruise,h.cruise,1.e-9);close_relative("T46 top",p.top,h.top,1.e-9);
      close_relative("T46 acceleration",p.acceleration,h.top*std::atanh(.9f)/h.timeToTop,1.e-9);close_relative("T46 deceleration",p.deceleration,.9f*h.cruise/h.holdTime,1.e-9);close_relative("T46 pole",p.pole,h.onset/(h.onset+2.f/120),1.e-9);
      F32 r=(1-h.highSpeedFloat)*4/h.top,e=.1f*(1+r*h.cruise)/(1+.1f*r*h.cruise),linear=-std::log(e)/h.coastTime;
      close_relative("T46 drag linear",p.dragLinear,linear,1.e-9);close_relative("T46 drag quadratic",p.dragQuadratic,r*linear,1.e-9);close_relative("T46 grip",p.grip,h.grip,1.e-9);close_relative("T46 climb speed",p.climbSpeed,h.climb,1.e-9);close_relative("T46 climb acceleration",p.climbAcceleration,h.climb>0?.9f*h.climb/h.climbTime:0.f,1.e-9);
      close_relative("T46 yaw rate",p.yawRate,h.turnRateDegrees*DEG,1.e-9);close_relative("T46 speed reference",p.speedReference,h.top*(1.05f-h.turnWeight),1.e-9);close_relative("T46 yaw acceleration",p.yawAcceleration,p.yawRate/.15f,1.e-9);close_relative("T46 tilt rate",p.tiltRate,h.tiltRateDegrees*DEG,1.e-9);close_relative("T46 tilt acceleration",p.tiltAcceleration,p.tiltRate/.15f,1.e-9);close_relative("T46 roll rate",p.rollRate,2.f,1.e-9);close_relative("T46 roll acceleration",p.rollAcceleration,2.f/.15f,1.e-9);close_relative("T46 velocity cap",p.velocityCap,2.f*h.top,1.e-9);
      const F32 along=std::max(p.acceleration,std::max(p.deceleration,p.brakeDeceleration)),horizontal=std::sqrt(along*along+(p.grip*p.acceleration)*(p.grip*p.acceleration)),drag=p.dragLinear*p.velocityCap+p.dragQuadratic*p.velocityCap*p.velocityCap,total=std::max(std::sqrt(horizontal*horizontal+p.climbAcceleration*p.climbAcceleration),drag);
      close_relative("T46 horizontal limit",p.horizontalLimit,horizontal,1.e-9);close_relative("T46 drag limit",p.dragLimit,drag,1.e-9);close_relative("T46 total limit",p.totalLimit,total,1.e-9);close_relative("T46 gMax",p.gMax,LLFlycamDrone::gMax(p.pole),1.e-9);close_relative("T46 effective jerk",p.effectiveJerk,LLFlycamDrone::effectiveJerk(p.pole,p.totalLimit,p.tick),1.e-9);
      const F32 desired=h.stopDistance-h.top*h.onset;const F32 low=h.top*h.top/(2.f*(desired+h.top*p.tick)),high=h.top*h.top/(2.f*desired);ensure("T46 D_brk inside proven bracket",p.brakeDeceleration>=low&&p.brakeDeceleration<=high);
      const F64 vt=h.top,brake=p.brakeDeceleration,dt=p.tick;const S32 m=(S32)std::floor(vt/(brake*dt));F64 brute=0.;for(S32 k=1;k<=m;++k)brute+=dt*(vt-brake*dt*k);const F64 explicitSum=dt*(m*vt-brake*dt*m*(m+1.)*.5);
      close("T46 exact discrete-sum equality",brute,explicitSum,4.*m*std::numeric_limits<F64>::epsilon()*std::max(1.,std::fabs(brute)));ensure("T46 fixed 60-step bracket terminates",(high-low)*std::pow(.5,60)<std::numeric_limits<F32>::epsilon()*std::max(1.f,p.brakeDeceleration));}
}

template<> template<> void flycam_drone_object::test<44>()
{
    auto p=compiled(1,120);LLVector3d oldOrigin(1024,2048,0),newOrigin(1280,2048,0);
    for(S32 situation=0;situation<4;++situation){LLFlycamDrone d(p);d.seed(LLVector3(10,20,30),LLQuaternion(),LLVector3(3,2,1),oldOrigin);if(situation==0)d.advanceRaw(10*p.tick,forward());if(situation==1)d.advanceRaw(.5*p.tick,forward());
      const LLVector3 before=global_position(d),v=d.state().velocity;ensure("T47 shift applied",d.applyOrigin(newOrigin));vector_close("T47 global continuity all situations",global_position(d),before,1.e-4f);vector_close("T47 Keep velocity unaffected",d.state().velocity,v,1.e-6f);
      if(situation==1)d.pauseAtRenderPose();if(situation==2)d.seed(LLVector3(12,21,30),LLQuaternion(),LLVector3::zero,newOrigin);
      if(situation==3){const LLVector3d a(1300,2050,30),b(1300.2,2050.4,30.6);d.seedFromHistory(a,LLQuaternion(),b,LLQuaternion(),.2,newOrigin);vector_close("T47 global history seed unaffected",d.state().velocity,LLVector3(1,2,3),1.e-4f);}
    }
}

template<> template<> void flycam_drone_object::test<45>()
{
    // resetPending/dispatch tags are viewer-owned; public reset must discard momentum.
    auto p=compiled(1,120);LLFlycamDrone d(p);d.seed(LLVector3::zero,LLQuaternion(),LLVector3(4,0,0));d.seed(LLVector3(2,0,0),LLQuaternion());ensure_equals("T48 reset consumes with Stop",d.state().velocity.lengthSquared(),0.f);
}

template<> template<> void flycam_drone_object::test<46>()
{
    for(F32 hz:HZS)for(std::size_t from=0;from<5;++from)for(std::size_t to=0;to<5;++to)for(S32 stopKind=0;stopKind<3;++stopKind)
    {
      auto old=compiled(from,hz),now=compiled(to,hz);now.release=stopKind==0?LLFlycamDrone::ReleaseMode::DRIFT:LLFlycamDrone::ReleaseMode::HOLD;LLFlycamDrone d(old);auto s=d.state();s.referenceVelocity=s.smootherVelocity=s.velocity=LLVector3(old.top,0,0);s.totalLimitMax=old.totalLimit;d.setParams(now);LLFlycamDrone::Input in;if(stopKind==2)in.brake=1.f;
      const auto first=d.step(s,in,now.tick);if(old.top>now.velocityCap)ensure("T49 preset change never caps velocity",first.velocity.length()>now.velocityCap);s=first;
      for(S32 n=1;n<2000&&!s.settled;++n){auto q=d.step(s,in,now.tick);ensure("T49 stage-1 immediate new limit",mag(q.referenceVelocity-s.referenceVelocity)<=now.totalLimit*now.tick*FP);assert_g1("T49 G1 max old/new",s,q,now,now.tick);if(!q.snapped)ensure_equals("T49 retains max(old,new)",q.totalLimitMax,std::max(old.totalLimit,now.totalLimit));s=q;}
    }
    // Build-scale ease-out is another coefficient drop, not a separate path.
    for(F32 hz:HZS)for(std::size_t pi=0;pi<5;++pi){auto h=LLFlycamDrone::presets()[pi].human,scaled=h;scaled.cruise*=.25f;scaled.top*=.25f;scaled.climb*=.25f;scaled.stopDistance*=.25f;LLFlycamDrone::Params old=compiled(pi,hz),now;LLFlycamDrone::compile(scaled,scaled,1.f/hz,2,now);LLFlycamDrone d(old);auto s=d.state();s.referenceVelocity=s.smootherVelocity=s.velocity=LLVector3(old.top,0,0);d.setParams(now);auto q=d.step(s,LLFlycamDrone::Input(),now.tick);assert_g1("T49 build-scale ease-out G1",s,q,now,now.tick);}

    // Continuous turning prevents an exact steady point indefinitely: the old
    // bound remains authoritative, and the new bound alone is demonstrably insufficient.
    auto old=compiled(4,120),now=compiled(3,120);LLFlycamDrone d(old);auto s=d.state();s.referenceVelocity=s.smootherVelocity=s.velocity=LLVector3(old.top,0,0);s.totalLimitMax=old.totalLimit;d.setParams(now);bool exceedsNew=false;
    for(S32 n=0;n<4000;++n){LLFlycamDrone::Input in=forward();in.move.set(std::cos(n*.071f),std::sin(n*.071f),0);auto q=d.step(s,in,now.tick);assert_g1("T49 indefinite old-limit G1",s,q,now,now.tick);ensure("T49 no steady point while maneuvering",!q.snapped);ensure_equals("T49 old max held indefinitely",q.totalLimitMax,std::max(old.totalLimit,now.totalLimit));const F32 newJ=LLFlycamDrone::effectiveJerk(q.pole,now.totalLimit,now.tick);if(mag(q.deltaVelocity-s.deltaVelocity)/(now.tick*now.tick)>newJ*FP)exceedsNew=true;s=q;}
    ensure("T49 new limit alone is not a valid bound",exceedsNew);
}

template<> template<> void flycam_drone_object::test<47>()
{
    auto p=compiled(4,120);p.freeFly=true;LLFlycamDrone d(p);for(F32 mix=-1;mix<=1;mix+=.02f){LLFlycamDrone::Input in=forward();in.vertical=mix;auto a=d.state(),b=d.step(a,in,p.tick);ensure("T50 one combined clamp",mag(b.referenceVelocity-a.referenceVelocity)<=p.horizontalLimit*p.tick*FP);}auto s=run_steps(d,d.state(),forward(),100,p.tick);auto locked=p;locked.freeFly=false;d.setParams(locked);auto q=d.step(s,forward(),p.tick);assert_g1("T50 toggle G1",s,q,locked,p.tick);
    // A trigger without movement does not engage Free-fly.  In Drift it must
    // follow the exact drag flow, which distinguishes moveLength>0 from throttle>0.
    p.release=LLFlycamDrone::ReleaseMode::DRIFT;LLFlycamDrone drift(p);drift.seed(LLVector3::zero,LLQuaternion(),LLVector3(p.cruise,0,0));LLFlycamDrone::Input triggerOnly;triggerOnly.throttle=1.f;triggerOnly.droneThrottle=true;const auto before=drift.state(),after=drift.step(before,triggerOnly,p.tick);const F32 e=std::exp(-p.dragLinear*p.tick);const F32 exact=p.dragLinear*p.cruise*e/(p.dragLinear+p.dragQuadratic*p.cruise*(1.f-e));close("T50 zero-move throttle remains Drift",after.referenceVelocity.length(),exact,2.f*std::numeric_limits<F32>::epsilon()*p.cruise);
}

template<> template<> void flycam_drone_object::test<48>()
{
    for(bool freeFly:{false,true}){auto p=compiled(1,120);p.freeFly=freeFly;LLFlycamDrone d(p);LLVector3 initial(5,2,1);d.seed(LLVector3::zero,LLQuaternion(),initial);auto s=d.state();LLFlycamDrone::Input stop;stop.brake=1;stop.yaw=1;
      for(S32 n=0;n<3000;++n){stop.move.set(std::cos(n*.02f),std::sin(n*.02f),0);auto q=d.step(s,stop,p.tick);ensure("T51 H no reverse",q.referenceVelocity.mV[VX]*initial.mV[VX]+q.referenceVelocity.mV[VY]*initial.mV[VY]>=-1.e-5f);ensure("T51 Z no reverse",q.referenceVelocity.mV[VZ]*initial.mV[VZ]>=-1.e-6f);assert_g1("T51 steering stop G1",s,q,p,p.tick);s=q;}
      d.seed(LLVector3::zero,LLQuaternion(),initial);s=d.state();stop.brake=.5f;for(S32 n=0;n<500;++n){auto q=d.step(s,stop,p.tick);assert_g1("T51 partial-brake G1/G5",s,q,p,p.tick);ensure("T51 partial brake respects cap",q.referenceVelocity.length()<=std::max(s.referenceVelocity.length(),p.velocityCap)*FP);s=q;}}
}

template<> template<> void flycam_drone_object::test<49>()
{
    const auto make=[](F32 onset,std::size_t preset){auto h=LLFlycamDrone::presets()[preset].human;h.onset=onset;h.stopDistance=std::max(h.stopDistance,h.top*(onset+2.f/120));LLFlycamDrone::Params p;LLFlycamDrone::compile(h,h,1.f/120,2,p);return p;};
    for(const auto pair:{std::pair<F32,F32>(.05f,1.f),std::pair<F32,F32>(1.f,.05f)})
    {auto before=make(pair.first,1),after=make(pair.second,1);LLFlycamDrone d(before);auto s=run_steps(d,d.state(),forward(),100,before.tick);d.setParams(after);auto moving=d.step(s,forward(),before.tick);ensure("T52 old pole while moving",moving.pole==before.pole&&moving.hasPendingPole);LLFlycamDrone::Input stop;stop.brake=1;
      LLFlycamDrone::State prior=moving;for(S32 n=0;n<10000&&moving.hasPendingPole;++n){prior=moving;moving=d.step(moving,stop,before.tick);}ensure("T52 pending pole adopted",moving.pole==after.pole&&!moving.hasPendingPole&&moving.snapped);
      const F32 jStar=std::min(LLFlycamDrone::effectiveJerk(prior.pole,prior.totalLimitMax,before.tick),after.effectiveJerk);ensure("T52 snap <=3/16 Jstar",mag(moving.deltaVelocity-prior.deltaVelocity)/(before.tick*before.tick)<=3.f/16.f*jStar*FP);
      LLFlycamDrone::Input yaw;yaw.yaw=.2f;auto next=d.step(moving,yaw,before.tick);ensure("T52 no second snap",!next.snapped);ensure("T52 next <=5/8 Jafter",mag(next.deltaVelocity-moving.deltaVelocity)/(before.tick*before.tick)<=5.f/8.f*after.effectiveJerk*FP);}

    // Combined pending-pole + >=30x preset drop at re-baseline and settle.
    auto before=make(.05f,4),after=make(1.f,3);ensure("T52 combined drop >=30x",before.totalLimit>=30.f*after.totalLimit);
    for(bool settle:{false,true}){LLFlycamDrone d(before);d.setParams(after);auto s=d.state();s.totalLimitMax=before.totalLimit;const F32 jStar=std::min(LLFlycamDrone::effectiveJerk(before.pole,before.totalLimit,after.tick),after.effectiveJerk);const F32 eps=std::min(jStar*after.tick*after.tick/4.f,.005f)/4.f;s.smootherVelocity.set(eps,0,0);s.velocity.set(2*eps,0,0);s.deltaVelocity.set(eps,0,0);LLFlycamDrone::Input in;if(!settle)in.yaw=.2f;auto q=d.step(s,in,after.tick);ensure("T52 combined event one snap",q.snapped&&!q.hasPendingPole);ensure_equals("T52 combined max reset",q.totalLimitMax,after.totalLimit);ensure_equals("T52 combined settle classification",q.settled,settle);}
}
}
