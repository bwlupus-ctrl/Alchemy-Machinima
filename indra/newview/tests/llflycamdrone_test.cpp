/**
 * @file llflycamdrone_test.cpp
 * @brief FLYCAM_DRONE_CONTROL_BRIEF rev 11, section 16.1.
 */

#include "linden_common.h"
#include "../test/lltut.h"
#include "../llflycamdrone.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <string>
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
    return in;
}

void assert_g1(const char* what, const LLFlycamDrone::State& a,
               const LLFlycamDrone::State& b, const LLFlycamDrone::Params& p, F32 h)
{
    ensure(what, mag(b.velocity - a.velocity) / h <= b.totalLimitMax * FP + 1.e-5f);
    ensure("G1 jerk", mag(b.deltaVelocity - a.deltaVelocity) / (h * h) <=
                       LLFlycamDrone::effectiveJerk(b.pole, b.totalLimitMax, h) * FP + 1.e-4f);
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

F32 stage_crossing(const LLFlycamDrone& model, LLFlycamDrone::State s,
                   const LLFlycamDrone::Input& in, F32 target, F32 h, S32 component)
{
    F32 previous = std::fabs(s.referenceVelocity.mV[component]);
    for (S32 n = 1; n < 200000; ++n)
    {
        LLFlycamDrone::State next = model.step(s, in, h);
        F32 value = std::fabs(next.referenceVelocity.mV[component]);
        if (value >= target)
        {
            const F32 fraction = value == previous ? 1.f : (target - previous) / (value - previous);
            return (static_cast<F32>(n - 1) + fraction) * h;
        }
        previous = value;
        s = next;
    }
    return std::numeric_limits<F32>::infinity();
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

// T1 frame grouping: a held per-tick sequence is independent of frame grouping.
template<> template<> void flycam_drone_object::test<1>()
{
    for (std::size_t pi = 0; pi < LLFlycamDrone::presets().size(); ++pi)
    for (F32 hz : HZS)
    {
        const auto p = compiled(pi, hz);
        LLFlycamDrone a(p), b(p);
        const auto in = forward(.7f);
        for (S32 i = 0; i < 240; ++i) a.advanceRaw(p.tick, in);
        const F64 frames[] = { 1. / 30., 1. / 60., 1. / 144., .004, .012, .006, .011 };
        F64 supplied = 0.;
        for (S32 i = 0; supplied + 1.e-12 < 240. * p.tick; ++i)
        {
            const F64 dt = std::min(frames[i % 7], 240. * p.tick - supplied);
            b.advanceRaw(dt, in); supplied += dt;
        }
        ensure("T1 bit-identical state", exact_state(a.state(), b.state()));
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
    }
}

template<> template<> void flycam_drone_object::test<3>()
{
    auto p = compiled(1, 120.f); LLFlycamDrone d(p); d.advanceRaw(p.tick, forward());
    const LLVector3 prev = d.previousState().position, cur = d.state().position;
    for (S32 i = 0; i < 8; ++i)
    {
        d.advanceRaw(p.tick / 8.f, forward());
        const LLVector3 r = d.renderPose().position;
        ensure("T3 interpolation segment", mag(r - prev) <= mag(cur - prev) + 1.e-6f ||
               mag(r - cur) <= p.top * p.tick + 1.e-6f);
    }
}

template<> template<> void flycam_drone_object::test<4>()
{
    auto p = compiled(1, 120.f); LLFlycamDrone raw(p), clamped(p);
    raw.advanceRaw(2000. * p.tick, forward());
    ensure("T4 512-step cap/backlog", raw.accumulator() >= (2000 - 512) * p.tick - 1.e-7);
    LLVector3 last = raw.renderPose().position;
    while (raw.accumulator() + 1.e-9 >= p.tick)
    {
        const LLVector3 now = raw.advanceRaw(0., forward()).position;
        ensure("T4 drain continuity", mag(now - last) <= p.top * 512.f * p.tick + 1.e-4f);
        last = now;
    }
    close("T4 total input time drained", raw.accumulator(), 0., 1.e-9);
    clamped.advance(2., forward());
    ensure("T4 public advance clamps .25s", clamped.state().position.mV[VX] <= p.top * .25f + 1.e-5f);
}

// T5 t_top: mass-stage 90% crossing and camera latency.
template<> template<> void flycam_drone_object::test<5>()
{
    for (std::size_t pi = 0; pi < LLFlycamDrone::presets().size(); ++pi) for (F32 hz : HZS)
    {
        auto p = compiled(pi, hz); LLFlycamDrone d(p); const auto& h = LLFlycamDrone::presets()[pi].human;
        F32 t = stage_crossing(d, d.state(), forward(), .9f * h.top, p.tick, VX);
        ensure("T5 interpolated t_top .5%", std::fabs(t - h.timeToTop) <= .005f * h.timeToTop);
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
    }
}

template<> template<> void flycam_drone_object::test<7>()
{
    for (std::size_t pi=0; pi<LLFlycamDrone::presets().size(); ++pi) for(F32 hz:HZS)
    { auto p=compiled(pi,hz); p.release=LLFlycamDrone::ReleaseMode::HOLD; LLFlycamDrone d(p); const auto& h=LLFlycamDrone::presets()[pi].human;
      LLFlycamDrone::State s=d.state(); s.referenceVelocity.set(h.cruise,0,0); s.smootherVelocity=s.velocity=s.referenceVelocity;
      LLFlycamDrone::Input z; F32 previous=h.cruise,t=0.f;
      for(S32 n=1;n<200000;++n){auto q=d.step(s,z,p.tick);F32 v=q.referenceVelocity.mV[VX];if(v<=.1f*h.cruise){t=(n-1+(previous-.1f*h.cruise)/(previous-v))*p.tick;break;}previous=v;s=q;}
      ensure("T7 hold crossing .5%", std::fabs(t-h.holdTime)<=.005f*h.holdTime+p.tick); }
}

template<> template<> void flycam_drone_object::test<8>()
{
    for (std::size_t pi=0; pi<LLFlycamDrone::presets().size(); ++pi) for(F32 hz:HZS)
    { auto p=compiled(pi,hz); p.release=LLFlycamDrone::ReleaseMode::DRIFT; LLFlycamDrone d(p);
      const auto& h=LLFlycamDrone::presets()[pi].human; LLFlycamDrone::State s=d.state();
      s.referenceVelocity.set(h.cruise,0,0); s.smootherVelocity=s.velocity=s.referenceVelocity;
      F32 previous=h.cruise,t=0; LLFlycamDrone::Input z;
      for(S32 n=1;n<200000;++n){auto q=d.step(s,z,p.tick);F32 v=q.referenceVelocity.length();if(v<=.1f*h.cruise){t=(n-1+(previous-.1f*h.cruise)/(previous-v))*p.tick;break;}previous=v;s=q;}
      ensure("T8 coast crossing .5%",std::fabs(t-h.coastTime)<=.005f*h.coastTime+p.tick); }
}

template<> template<> void flycam_drone_object::test<9>()
{
    for(std::size_t pi=0;pi<LLFlycamDrone::presets().size();++pi)for(F32 hz:HZS)
    { auto p=compiled(pi,hz);const auto& h=LLFlycamDrone::presets()[pi].human;if(h.climb==0)continue;
      LLFlycamDrone d(p);LLFlycamDrone::Input in;in.vertical=1;F32 t=stage_crossing(d,d.state(),in,.9f*h.climb,p.tick,VZ);
      ensure("T9 climb crossing .5%",std::fabs(t-h.climbTime)<=.005f*h.climbTime+p.tick);
      LLFlycamDrone::Params q;LLFlycamDrone::compile(h,h,p.tick,2.f,q);ensure("T9 deterministic compile",p.acceleration==q.acceleration&&p.deceleration==q.deceleration&&p.brakeDeceleration==q.brakeDeceleration&&p.dragLinear==q.dragLinear&&p.dragQuadratic==q.dragQuadratic&&p.pole==q.pole&&p.effectiveJerk==q.effectiveJerk); }
}

template<> template<> void flycam_drone_object::test<10>()
{
    std::mt19937 rng(0x16A1u); std::uniform_real_distribution<F32> u(-1.f,1.f);
    for(S32 run=0;run<10000;++run)
    {
      auto p=compiled(run%5,HZS[run%4]);LLFlycamDrone d(p);
      if((run%11)==0)d.seed(LLVector3::zero,LLQuaternion(),LLVector3(p.velocityCap*(u(rng)+1.f)*.5f,0,0));
      auto a=d.state();
      for(S32 tick=0;tick<20;++tick)
      {
        if(tick==7){p=compiled((run+3)%5,HZS[run%4]);p.release=(run&1)?LLFlycamDrone::ReleaseMode::HOLD:LLFlycamDrone::ReleaseMode::DRIFT;d.setParams(p);}
        LLFlycamDrone::Input in;
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
}

template<> template<> void flycam_drone_object::test<11>()
{
    for(std::size_t pi=0;pi<5;++pi){auto p=compiled(pi,120);LLFlycamDrone d(p);auto s=run_steps(d,d.state(),forward(),(S32)(8*LLFlycamDrone::presets()[pi].human.onset/p.tick)+1,p.tick);
      LLVector3 ray=s.velocity/s.velocity.length();LLFlycamDrone::Input stop;stop.brake=1;
      for(S32 n=0;n<5000&&!s.settled;++n){s=d.step(s,stop,p.tick);ensure("T11 no reversal",s.velocity*ray>=-1.e-6f);ensure("T11 stage no reversal",s.referenceVelocity*ray>=-1.e-6f);} }
}

template<> template<> void flycam_drone_object::test<12>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);auto s=d.state();s.referenceVelocity.set(2*p.top,0,0);s.smootherVelocity=s.velocity=s.referenceVelocity;
    F32 previous=s.velocity.length();for(S32 n=0;n<2000&&previous>p.top+1.e-5f;++n){auto q=d.step(s,forward(),p.tick);ensure("T12 monotone overspeed",q.velocity.length()<=previous+1.e-6f);assert_g1("T12 G1",s,q,p,p.tick);previous=q.velocity.length();s=q;}ensure("T12 no undershoot",previous>=p.top-1.e-4f);
}

template<> template<> void flycam_drone_object::test<13>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);auto s=run_steps(d,d.state(),forward(),1000,p.tick);LLFlycamDrone::Input back=forward();back.move.set(-1,0,0);S32 crossings=0;F32 previous=s.velocity.mV[VX];
    for(S32 n=0;n<3000;++n){auto q=d.step(s,back,p.tick);if(previous>0&&q.velocity.mV[VX]<=0)++crossings;ensure("T13 no snap",!q.snapped);assert_g1("T13 G1",s,q,p,p.tick);previous=q.velocity.mV[VX];s=q;}ensure_equals("T13 one zero crossing",crossings,1);
}

template<> template<> void flycam_drone_object::test<14>()
{
    auto h=LLFlycamDrone::presets()[1].human;h.grip=.001f;LLFlycamDrone::Params p;LLFlycamDrone::HumanParams accepted;LLFlycamDrone::compile(h,LLFlycamDrone::presets()[1].human,1.f/120,2,p,&accepted);
    ensure_equals("T14 grip validation floor",accepted.grip,.05f);LLFlycamDrone d(p);auto s=d.state();s.referenceVelocity.set(1,1,0);auto q=d.step(s,forward(),p.tick);ensure("T14 finite",finite(q.referenceVelocity));
}

template<> template<> void flycam_drone_object::test<15>()
{
    auto p=compiled(4,120);LLFlycamDrone d(p);auto s=d.state();s.referenceVelocity=s.smootherVelocity=s.velocity=LLVector3::zero;s.deltaVelocity.setZero();s.totalLimitMax=p.totalLimit*30.f;
    auto low=compiled(3,120);d.setParams(low);auto q=d.step(s,LLFlycamDrone::Input(),low.tick);ensure("T15 atomic snap",q.snapped);ensure("T15 max reset",q.totalLimitMax==low.totalLimit);ensure("T15 pending adopted",!q.hasPendingPole&&q.pole==low.pole);
    F32 js=std::min(LLFlycamDrone::effectiveJerk(s.pole,s.totalLimitMax,low.tick),low.effectiveJerk);ensure("T15 snap jerk 3/16",mag(q.deltaVelocity-s.deltaVelocity)/(low.tick*low.tick)<=3.f/16.f*js*FP+1.e-5f);
}

template<> template<> void flycam_drone_object::test<16>()
{
    std::vector<std::string> logs;auto good=LLFlycamDrone::presets()[1].human,bad=good;bad.top=std::numeric_limits<F32>::quiet_NaN();LLFlycamDrone::Params p;bool ok=LLFlycamDrone::compile(bad,good,1.f/120,2,p,nullptr,[&](const std::string&s){logs.push_back(s);});ensure("T16 rejects bad setting",!ok&&!logs.empty());
    LLFlycamDrone d(p,[&](const std::string&s){logs.push_back(s);});auto s=d.state();s.position.mV[VX]=std::numeric_limits<F32>::quiet_NaN();auto q=d.step(s,forward(),p.tick);ensure("T16 NONFINITE recovered",q.nonFiniteRecovered&&finite(q.position));
}

template<> template<> void flycam_drone_object::test<17>()
{
    for(std::size_t pi=0;pi<5;++pi){auto a=compiled(pi,30),b=compiled(pi,240);ensure("T17 near-independent jerk",std::fabs(a.effectiveJerk-b.effectiveJerk)/std::max(a.effectiveJerk,b.effectiveJerk)<.25f);}
}

template<> template<> void flycam_drone_object::test<18>()
{
    auto p=compiled(1,120);LLFlycamDrone locked(p);locked.seed(LLVector3::zero,LLFlycamDrone::rotation(0,-80*DEG,0));auto a=run_steps(locked,locked.state(),forward(),240,p.tick);ensure("T18 horizon delta z",std::fabs(a.position.mV[VZ])<=1.e-6f);
    p.freeFly=true;LLFlycamDrone free(p);free.seed(LLVector3::zero,LLFlycamDrone::rotation(0,-80*DEG,0));auto b=run_steps(free,free.state(),forward(),240,p.tick);ensure("T18 free fly z",std::fabs(b.position.mV[VZ])>1.e-3f);
}

template<> template<> void flycam_drone_object::test<19>()
{
    std::mt19937 rng(19);std::uniform_real_distribution<F32> a(-PI,PI),t(-PI/2,PI/2);auto p=compiled(1,120);LLFlycamDrone d(p);
    for(S32 i=0;i<10000;++i){LLQuaternion q=LLFlycamDrone::rotation(a(rng),t(rng),a(rng));d.seed(LLVector3::zero,q);quaternion_close("T19 reconstruction",d.renderPose().rotation,q,.05f*DEG);}
}

template<> template<> void flycam_drone_object::test<20>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);LLFlycamDrone::State prior;prior.yaw=.7f;
    LLQuaternion last;bool have=false;for(F32 deg=85;deg<=95;deg+=.1f){auto q=LLFlycamDrone::rotation(.7f,deg*DEG,.2f);d.seed(LLVector3::zero,q,LLVector3::zero,LLVector3d::zero,&prior);if(std::fabs(deg-90.f)<.001f)ensure("T20 prior yaw at pole",std::fabs(d.state().yaw-prior.yaw)<.02f);if(have)quaternion_close("T20 continuity",last,d.renderPose().rotation,.2f*DEG);last=d.renderPose().rotation;have=true;prior=d.state();}
}

template<> template<> void flycam_drone_object::test<21>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);d.seed(LLVector3::zero,LLFlycamDrone::rotation(0,89.7f*DEG,0));ensure("T21 keeps seeded limit",d.state().tiltLimit>=89.7f*DEG);LLFlycamDrone::Input up;up.tilt=1;auto s=run_steps(d,d.state(),up,120,p.tick);ensure("T21 cannot increase",std::fabs(s.tilt)<=89.7f*DEG+1.e-6f);up.tilt=-1;for(S32 n=0;n<1000&&std::fabs(s.tilt)>88.9f*DEG;++n)s=d.step(s,up,p.tick);s=d.step(s,up,p.tick);close("T21 limit drops to 89",s.tiltLimit,89.f*DEG,1.e-6);
}

template<> template<> void flycam_drone_object::test<22>()
{
    auto prev=LLFlycamDrone::presets()[1].human,bad=prev;bad.timeToTop=.05f;bad.onset=1.f;bad.stopDistance=.01f;bad.highSpeedFloat=0;bad.grip=0;LLFlycamDrone::Params p;LLFlycamDrone::HumanParams accepted;std::vector<std::string> logs;
    auto start=std::chrono::steady_clock::now();LLFlycamDrone::compile(bad,prev,1.f/30,2,p,&accepted,[&](const std::string&s){logs.push_back(s);});auto elapsed=std::chrono::duration<F64,std::milli>(std::chrono::steady_clock::now()-start).count();
    ensure("T22 min times",accepted.timeToTop>=4.f/30.f);ensure("T22 grip floor",accepted.grip>=.05f);close("T22 exact stop floor",accepted.stopDistance,accepted.top*(accepted.onset+2.f/30.f),1.e-6);ensure("T22 logs",!logs.empty());ensure("T22 compile <1ms",elapsed<1.0);
}

template<> template<> void flycam_drone_object::test<23>()
{
    // FlycamBuildModeScale is caller-owned; the pure model must not alter calibrated timing.
    auto p=compiled(1,120);LLFlycamDrone normal(p),scaled(p);auto a=run_steps(normal,normal.state(),forward(),600,p.tick);auto b=run_steps(scaled,scaled.state(),forward(),600,p.tick);close("T23 calibrated time unchanged",a.velocity.length(),b.velocity.length(),1.e-6);close("T23 pure-model distance unchanged",a.position.length(),b.position.length(),1.e-6);
}

template<> template<> void flycam_drone_object::test<24>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);for(S32 axis=0;axis<2;++axis)for(F32 sign:{-1.f,1.f}){LLFlycamDrone::Input in;in.move.mV[axis]=sign;auto q=d.step(d.state(),in,p.tick);ensure("T24 translation sign",q.referenceVelocity.mV[axis]*sign>=0.f);}
    for(F32 sign:{-1.f,1.f}){LLFlycamDrone::Input in;in.vertical=sign;auto z=d.step(d.state(),in,p.tick);ensure("T24 vertical sign",z.referenceVelocity.mV[VZ]*sign>=0.f);in=LLFlycamDrone::Input();in.yaw=sign;ensure("T24 yaw sign",d.step(d.state(),in,p.tick).yawVelocity*sign>=0.f);in=LLFlycamDrone::Input();in.tilt=sign;ensure("T24 tilt sign",d.step(d.state(),in,p.tick).tiltVelocity*sign>=0.f);in=LLFlycamDrone::Input();in.roll=sign;ensure("T24 roll sign",d.step(d.state(),in,p.tick).rollVelocity*sign>=0.f);}
}

template<> template<> void flycam_drone_object::test<25>()
{
    // Trigger shaping itself is pinned in alxinputmath_test; the public Drone input owns only shaped values.
    auto p=compiled(1,120);LLFlycamDrone d(p);LLFlycamDrone::Input stale;auto q=d.step(d.state(),stale,p.tick);ensure_equals("T25 stale trigger is neutral",q.referenceVelocity.lengthSquared(),0.f);
}

template<> template<> void flycam_drone_object::test<26>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);LLVector3 v(3,1,0);d.seed(LLVector3(1,2,3),LLQuaternion(),v);vector_close("T26 BD transition velocity",d.state().velocity,v,1.e-6f);vector_close("T26 no pop",d.renderPose().position,LLVector3(1,2,3),1.e-6f);
}
template<> template<> void flycam_drone_object::test<27>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);LLVector3 v(-2,1,.25f);d.seed(LLVector3(-1,2,4),LLQuaternion(),v);vector_close("T27 non-BD transition velocity",d.state().velocity,v,1.e-6f);vector_close("T27 no pop",d.renderPose().position,LLVector3(-1,2,4),1.e-6f);
}

template<> template<> void flycam_drone_object::test<28>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);LLVector3 v(2,1,.5);d.seed(LLVector3::zero,LLQuaternion(),v);auto seed=d.classicSeed(1.f/60);close("T28 displacement x",seed[0],v.mV[VX]/60.f,1.e-6);close("T28 rotation rate",seed[3],0,1.e-6);
}

template<> template<> void flycam_drone_object::test<29>()
{
    // Public-interface portion: the falling-edge history seed is pose-continuous and caps momentum.
    auto p=compiled(1,120);LLFlycamDrone d(p);LLVector3d a(0,0,0),b(100,0,0);d.seedFromHistory(a,LLQuaternion(),b,LLQuaternion(),.01,LLVector3d::zero);vector_close("T29 pose held",d.state().position,LLVector3(100,0,0),1.e-6f);ensure("T29 momentum cap",d.state().velocity.length()<=p.velocityCap*FP);
}

template<> template<> void flycam_drone_object::test<30>()
{
    // Dispatch classification is viewer-owned; Stop's public reseed semantics are testable here.
    auto p=compiled(1,120);LLFlycamDrone d(p);d.seed(LLVector3(4,5,6),LLQuaternion(),LLVector3::zero);vector_close("T30 Stop pose",d.renderPose().position,LLVector3(4,5,6),1.e-6f);ensure_equals("T30 Stop velocity",d.state().velocity.lengthSquared(),0.f);
}

template<> template<> void flycam_drone_object::test<31>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);auto s=run_steps(d,d.state(),forward(),300,p.tick);LLVector3 pose=d.renderPose().position,v=s.velocity;d.seed(pose,d.renderPose().rotation,v,LLVector3d::zero,&s);vector_close("T31 Keep pose",d.renderPose().position,pose,1.e-6f);vector_close("T31 Keep velocity",d.state().velocity,v,1.e-6f);
}

template<> template<> void flycam_drone_object::test<32>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);LLVector3d a(1,2,3),b(1.2,2.4,3.6);d.seedFromHistory(a,LLQuaternion(),b,LLQuaternion(),.2,LLVector3d::zero);vector_close("T32 constant-u Keep",d.state().velocity,LLVector3(1,2,3),1.e-4f);ensure_equals("T32 steady acceleration",d.state().deltaVelocity.lengthSquared(),0.f);
}

template<> template<> void flycam_drone_object::test<33>()
{
    // Cut classification/reason belongs to the unexposed final-camera sampler.
    auto p=compiled(1,120);LLFlycamDrone d(p);d.seedFromHistory(LLVector3d::zero,LLQuaternion(),LLVector3d(100,0,0),LLQuaternion(),.01,LLVector3d::zero);ensure("T33 public history seed remains bounded",d.state().velocity.length()<=p.velocityCap*FP);
}

template<> template<> void flycam_drone_object::test<34>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);d.seedFromHistory(LLVector3d::zero,LLQuaternion(),LLVector3d(1,0,0),LLQuaternion(),0,LLVector3d::zero);ensure_equals("T34 stale history stops",d.state().velocity.lengthSquared(),0.f);
}

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
    auto p=compiled(1,120);LLFlycamDrone d(p);d.seed(LLVector3::zero,LLFlycamDrone::rotation(0,.5f,0));LLFlycamDrone::Input in;in.recentre=true;auto s=d.step(d.state(),in,p.tick);ensure("T45 latch starts",s.recentre);in.recentre=false;for(S32 n=0;n<5000&&s.recentre;++n){F32 old=std::fabs(s.tilt);s=d.step(s,in,p.tick);ensure("T45 no overshoot",std::fabs(s.tilt)<=old+1.e-5f);}ensure("T45 completes",!s.recentre&&s.tilt==0);in.recentre=true;s=d.step(s,in,p.tick);in.recentre=false;in.tilt=.1f;s=d.step(s,in,p.tick);ensure("T45 tilt cancels",!s.recentre);
}

template<> template<> void flycam_drone_object::test<43>()
{
    std::mt19937 rng(46);std::uniform_real_distribution<F32> top(.1f,40),onset(.05f,1),extra(.1f,100);
    for(S32 i=0;i<10000;++i){auto h=LLFlycamDrone::presets()[1].human;h.cruise=std::min(h.cruise,top(rng));h.top=std::max(h.cruise,top(rng));h.onset=onset(rng);h.stopDistance=h.top*(h.onset+2.f/120)+extra(rng);LLFlycamDrone::Params p;LLFlycamDrone::compile(h,h,1.f/120,2,p);
      close("T46 acceleration formula",p.acceleration,h.top*std::atanh(.9f)/h.timeToTop,std::fabs(p.acceleration)*1.e-9+1.e-9);close("T46 deceleration formula",p.deceleration,.9f*h.cruise/h.holdTime,std::fabs(p.deceleration)*1.e-9+1.e-9);close("T46 pole formula",p.pole,h.onset/(h.onset+2.f/120),std::fabs(p.pole)*1.e-9+1.e-9);
      F32 r=(1-h.highSpeedFloat)*4/h.top,e=.1f*(1+r*h.cruise)/(1+.1f*r*h.cruise);close("T46 drag linear formula",p.dragLinear,-std::log(e)/h.coastTime,std::fabs(p.dragLinear)*1.e-9+1.e-9);close("T46 drag quadratic formula",p.dragQuadratic,r*p.dragLinear,std::fabs(p.dragQuadratic)*1.e-9+1.e-9);
      ensure("T46 brake finite positive",std::isfinite(p.brakeDeceleration)&&p.brakeDeceleration>0);F64 m=std::floor((F64)h.top/(p.brakeDeceleration*p.tick));F64 sum=p.tick*(m*h.top-p.brakeDeceleration*p.tick*m*(m+1.)*.5);F64 desired=h.stopDistance-h.top*h.onset;ensure("T46 discrete brake solve",std::fabs(sum-desired)<=h.top*p.tick+1.e-5);}
}

template<> template<> void flycam_drone_object::test<44>()
{
    auto p=compiled(1,120);LLFlycamDrone d(p);LLVector3d oldOrigin(1024,2048,0),newOrigin(1280,2048,0);d.seed(LLVector3(10,20,30),LLQuaternion(),LLVector3(3,2,1),oldOrigin);LLVector3 before=global_position(d),v=d.state().velocity;ensure("T47 shift applied",d.applyOrigin(newOrigin));vector_close("T47 global continuity",global_position(d),before,1.e-4f);vector_close("T47 velocity unchanged",d.state().velocity,v,1.e-6f);
}

template<> template<> void flycam_drone_object::test<45>()
{
    // resetPending/dispatch tags are viewer-owned; public reset must discard momentum.
    auto p=compiled(1,120);LLFlycamDrone d(p);d.seed(LLVector3::zero,LLQuaternion(),LLVector3(4,0,0));d.seed(LLVector3(2,0,0),LLQuaternion());ensure_equals("T48 reset consumes with Stop",d.state().velocity.lengthSquared(),0.f);
}

template<> template<> void flycam_drone_object::test<46>()
{
    auto old=compiled(4,120),low=compiled(3,120);LLFlycamDrone d(old);auto s=d.state();s.referenceVelocity=s.smootherVelocity=s.velocity=LLVector3(old.top,0,0);s.totalLimitMax=old.totalLimit;d.setParams(low);LLFlycamDrone::Input in=forward();
    for(S32 n=0;n<1000;++n){auto q=d.step(s,in,low.tick);ensure("T49 reference limit",mag(q.referenceVelocity-s.referenceVelocity)<=low.totalLimit*low.tick*FP+1.e-5f);assert_g1("T49 G1 max old/new",s,q,low,low.tick);if(!q.snapped)ensure("T49 retains old max",q.totalLimitMax==std::max(old.totalLimit,low.totalLimit));s=q;}
}

template<> template<> void flycam_drone_object::test<47>()
{
    auto p=compiled(4,120);p.freeFly=true;LLFlycamDrone d(p);for(F32 mix=-1;mix<=1;mix+=.02f){LLFlycamDrone::Input in=forward();in.vertical=mix;auto a=d.state(),b=d.step(a,in,p.tick);ensure("T50 one combined clamp",mag(b.referenceVelocity-a.referenceVelocity)<=p.horizontalLimit*p.tick*FP+1.e-5f);}auto s=run_steps(d,d.state(),forward(),100,p.tick);auto locked=p;locked.freeFly=false;d.setParams(locked);auto q=d.step(s,forward(),p.tick);assert_g1("T50 toggle G1",s,q,locked,p.tick);
}

template<> template<> void flycam_drone_object::test<48>()
{
    for(bool freeFly:{false,true}){auto p=compiled(1,120);p.freeFly=freeFly;LLFlycamDrone d(p);LLVector3 initial(5,2,1);d.seed(LLVector3::zero,LLQuaternion(),initial);auto s=d.state();LLFlycamDrone::Input stop;stop.brake=1;stop.yaw=1;
      for(S32 n=0;n<3000;++n){stop.move.set(std::cos(n*.02f),std::sin(n*.02f),0);auto q=d.step(s,stop,p.tick);ensure("T51 H no reverse",q.referenceVelocity.mV[VX]*initial.mV[VX]+q.referenceVelocity.mV[VY]*initial.mV[VY]>=-1.e-5f);ensure("T51 Z no reverse",q.referenceVelocity.mV[VZ]*initial.mV[VZ]>=-1.e-6f);s=q;}}
}

template<> template<> void flycam_drone_object::test<49>()
{
    auto fast=compiled(1,120);auto h=LLFlycamDrone::presets()[1].human;h.onset=1.f;LLFlycamDrone::Params slow;LLFlycamDrone::compile(h,h,1.f/120,2,slow);LLFlycamDrone d(fast);auto s=run_steps(d,d.state(),forward(),100,fast.tick);d.setParams(slow);auto moving=d.step(s,forward(),fast.tick);ensure("T52 old pole while moving",moving.pole==fast.pole&&moving.hasPendingPole);LLFlycamDrone::Input stop;stop.brake=1;for(S32 n=0;n<10000&&moving.hasPendingPole;++n)moving=d.step(moving,stop,fast.tick);ensure("T52 pending pole adopted",moving.pole==slow.pole&&!moving.hasPendingPole&&moving.snapped);
}
}
