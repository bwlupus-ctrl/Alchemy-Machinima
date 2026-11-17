// Executable specification. Built only by keyframe_core_native_check.py.
#include "../alkeyframecore.h"
#include "../alkeyframehistoryadapter.h"
#include "../alkeyframeinputgate.h"
#include "../alkeyframetransport.h"
#include "../alkeyframecamera.h"
#include "../alkeyframeplayer.h"
#include "../alkeyframelibrary.h"
#include "../alcleanframe.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <random>
#include <unordered_map>

using namespace ALKf;
static std::size_t allocationCount=0;
void* operator new(std::size_t size) {++allocationCount;if(void* p=std::malloc(size?size:1)) return p;throw std::bad_alloc();}
void* operator new[](std::size_t size) {return ::operator new(size);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
namespace
{
void require(bool condition,const char* name)
{
    if(!condition) {std::cerr<<"FAIL "<<name<<'\n';std::exit(1);}
}
bool closeEnough(double a,double b,double tolerance=1e-9) {return std::abs(a-b)<=tolerance;}
struct FakeResolver final : Resolver
{
    mutable unsigned calls=0;
    std::unordered_map<unsigned,Vec3> targets;
    bool resolve(const RefSlot& r,Vec3& p) const override
    {
        ++calls;auto i=targets.find(unsigned(r.id[0])+256*unsigned(r.id[1]));
        if(i==targets.end()) return false;p=i->second;return true;
    }
};
Key key(Vec3 p,Quat q={},double fov=60*degree,double roll=0)
{
    Key k;k.values={p,q,fov,roll};return k;
}
Track track(std::initializer_list<Vec3> positions)
{
    Track t=cameraTrack();for(auto p:positions) {auto k=key(p);k.id=t.nextKeyId++;t.keys.push_back(k);}return t;
}
Cache cache(const Track& t,const char* name="cache valid")
{
    Cache c;std::string why;
    if(!buildCache(t,c,why)) {std::cerr<<"cache reason: "<<why<<'\n';require(false,name);}return c;
}
Pose eval(const Track& t,const Cache& c,double time)
{
    FakeResolver r;ClientRuntime runtime;return evalCamera(t,c,time,r,EvalMode::Preview,runtime);
}
void F1()
{
    auto t=track({{0,0,0},{1,2,0},{3,-1,4},{5,0,1}});
    for(std::size_t i=0;i<t.keys.size();++i)
    {
        t.keys[i].values[1]=axisAngle({0,0,1},double(i)*.6);
        t.keys[i].values[2]=(30+double(i)*10)*degree;t.keys[i].values[3]=double(i)*.2;
    }
    auto c=cache(t);
    for(std::size_t i=0;i<t.keys.size();++i)
    {
        auto p=eval(t,c,c.arrival[i]);
        require(length(p.position-std::get<Vec3>(t.keys[i].values[0]))<1e-10,"F1 position at key");
        require(angle(p.rotation,std::get<Quat>(t.keys[i].values[1]))<1e-10,"F1 rotation at key");
        require(closeEnough(p.fov,std::get<double>(t.keys[i].values[2]))&&closeEnough(p.roll,std::get<double>(t.keys[i].values[3])),"F1 scalar at key");
    }
}
// Independent composite Simpson reference: no production integration/table calls.
double referenceLength(const Bezier& b,double u)
{
    constexpr int n=20000;double sum=0;
    for(int i=0;i<=n;++i) sum+=(i==0 || i==n ? 1:i%2?4:2)*length(b.derivative(u*i/n));
    return sum*u/(3*n);
}
void F2()
{
    for(auto t:{track({{0,0,0},{1,0,0},{3,0,0}}),
                track({{0,0,0},{0,0,0},{1,0,0},{1,0,0},{1,0,0},{2,1,0},{2,1,0}}),
                track({{2,3,4},{2,3,4},{2,3,4}}),
                track({{0,0,0},{.0005,0,0},{.0011,0,0},{1,0,0}})})
    {
        auto c=cache(t,"F2 finite");
        for(double time=0;time<=c.end;time+=.017) require(finite(eval(t,c,time).position),"F2 finite");
        for(const auto& arc:c.channels[0].spatial)
        {
            require(arc.table.size()<=4096,"F2 table cap");
            for(int j=0;j<=10;++j)
            {
                double s=arc.total*j/10,u=arc.inverse(s);
                require(std::abs(referenceLength(arc.curve,u)-s)<=.0001,"F2 inverse <=0.1mm");
            }
        }
    }
    for(Bezier b:{Bezier{{Vec3{},Vec3{},Vec3{},Vec3{1,0,0}}},
                  Bezier{{Vec3{},Vec3{1,1,0},Vec3{-1,-1,0},Vec3{0,0,0}}}})
    {
        ALKf::Arc a;a.build(b);
        for(int i=0;i<=100;++i)
        {
            double s=a.total*i/100,u=a.inverse(s);
            require(std::isfinite(u),"F2 finite");
            require(std::abs(referenceLength(b,u)-s)<=.0001,"F2 inverse <=0.1mm");
        }
    }
}
void F3()
{
    auto t=track({{0,0,0},{1,0,0},{11,0,0}});t.settings.evenSpeed=true;t.settings.total=12;
    auto c=cache(t);
    for(std::size_t i=0;i<c.timing.size();++i)
    {
        auto tm=c.timing[i];require(tm.duration-2*tm.window>=tm.duration/2-1e-12,"F3 cruise >=T/2");
        double a=c.departure[i]+tm.window,b=c.arrival[i+1]-tm.window;
        for(int j=1;j<=8;++j)
        {
            double x=a+(b-a)*(j-1)/8,y=a+(b-a)*j/8;
            double speed=length(eval(t,c,y).position-eval(t,c,x).position)/(y-x);
            require(std::abs(speed/tm.cruise-1)<=1e-9,"F3 cruise flat");
            require(std::abs(c.speedAt((x+y)/2)/tm.cruise-1)<=1e-9,"F3 distance cruise flat");
        }
    }
}
void F4()
{
    auto t=track({{0,0,0},{2,0,0},{3,0,0}});t.keys[1].hold=2;
    t.keys[1].values[2]=40*degree;t.keys[1].values[3]=.7;auto c=cache(t);
    for(int i=0;i<=100;++i)
    {
        auto p=eval(t,c,c.arrival[1]+2.*i/100);require(length(p.position-Vec3{2,0,0})<1e-12,"F4 hold constant");
        require(std::abs(p.fov-40*degree)<1e-12&&std::abs(p.roll-.7)<1e-12,"F4 hold FOV and roll constant");
    }
    for(std::size_t i=0;i<c.eased.size();++i) if(c.eased[i]) require(std::abs(c.speedAt(c.arrival[i]))<1e-12,"F4 eased slope zero");
    double previous=-1;
    for(int j=0;j<=10000;++j) {double s=c.distanceAt(c.end*j/10000);require(s>=previous-1e-12,"F4 monotone distance");previous=s;}
}
void F5()
{
    for(double ratio:{20.,100.}) for(bool stationary:{false,true})
    {
        auto t=track({{0,0,0},{1,0,0},{2,0,0},{3,0,0}});t.settings.easeEnds=false;
        t.keys[0].out.value=1;t.keys[1].out.value=ratio;t.keys[2].out.value=1;
        if(stationary) t.keys[2].values[0]=Vec3{1,0,0};
        auto c=cache(t);
        for(std::size_t j=1;j+1<c.knots.size();++j)
        {
            double x=c.knots[j].t;
            double a=c.speedAt(std::nextafter(x,-std::numeric_limits<double>::infinity()));
            double b=c.speedAt(std::nextafter(x,std::numeric_limits<double>::infinity()));
            require(std::abs(a-b)<=1e-9,"F5 C1");
        }
        // Actual vector endpoint velocities, not merely the scalar pacing clock.
        const auto& arcs=c.channels[0].spatial;
        for(std::size_t i=1;i+1<t.keys.size();++i)
        {
            auto before=unit(arcs[i-1].curve.derivative(1),{0,0,0})*c.speedAt(c.arrival[i]);
            auto after=unit(arcs[i].curve.derivative(0),{0,0,0})*c.speedAt(c.departure[i]);
            require(length(before-after)<=1e-9,"F5 vector C1");
        }
    }
}
void F6()
{
    auto d=durationTiming(10,4,2,.6);require(closeEnough(d.duration,4)&&closeEnough(d.window,.6)&&closeEnough(d.cruise,10/3.4),"F6 Duration formula");
    for(double l:{.01,10.}) for(double e:{.1,.6,3.}) for(unsigned n:{0u,1u,2u})
    {
        auto s=speedTiming(l,1,n,e);
        require(closeEnough(s.window,std::min(e,s.duration/4),1e-12)&&closeEnough(s.duration,l+n*s.window/2,1e-12),"F6 fixed point");
    }
    require(closeEnough(speedTiming(.01,1,2,.6).duration,1./75,1e-14),"F6c T=1/75");
    require(speedTiming(1,1,2,.1).window==.1&&speedTiming(1,1,2,3).window<3,"F6 branch switch");
    auto t=track({{0,0,0},{1,0,0},{11,0,0}});t.settings.evenSpeed=true;t.settings.total=7;
    auto c=cache(t);require(closeEnough(c.end,7,1e-9),"F6 Even total bisection");
    t.settings.evenSpeed=false;t.keys[0].hold=.2;t.keys[1].out={PaceMode::Speed,.9};
    std::string why;require(editTotal(t,9,why)&&closeEnough(cache(t).end,9,1e-9),"F6 Total edit bisection");
    Track before=t;require(!editTotal(t,.1,why)&&t==before,"F6 infeasible atomic refusal");
}
void F7()
{
    auto wrap=track({{0,0,0},{1,0,0},{2,0,0}});wrap.settings.easeEnds=false;
    wrap.keys[0].values[1]=axisAngle({0,0,1},179*degree);wrap.keys[1].values[1]=-axisAngle({0,0,1},181*degree);
    wrap.keys[2].values[1]=axisAngle({0,0,1},190*degree);wrap.keys[0].values[3]=179*degree;wrap.keys[1].values[3]=-179*degree;
    auto cw=cache(wrap);require(angle(eval(wrap,cw,1.5).rotation,axisAngle({0,0,1},180*degree))<2*degree,"F7 quaternion wrap/sign");
    require(std::abs(eval(wrap,cw,1.5).roll-180*degree)<degree,"F7 roll wrap");
    for(double delta:{120.,170.})
    {
        // Opposing incident rotations force maximal OUTWARD lifts on the middle segment.
        auto adversarial=track({{0,0,0},{1,0,0},{2,0,0},{3,0,0}});adversarial.settings.easeEnds=false;
        adversarial.keys[0].out.value=.001;adversarial.keys[1].out.value=3;adversarial.keys[2].out.value=.001;
        adversarial.keys[0].values[1]=axisAngle({0,0,1},delta*degree);
        adversarial.keys[1].values[1]=Quat{};adversarial.keys[2].values[1]=axisAngle({0,0,1},delta*degree);
        adversarial.keys[3].values[1]=Quat{};
        auto ca=cache(adversarial);const auto& ra=ca.channels[1].rotation[1];
        require(angle(ra.q[1],ra.q[2])<=175*degree+1e-12&&dot(ra.q[3],unit(std::get<Quat>(adversarial.keys[2].values[1])))>=0,"F7 ladder <=175deg");
        auto t=track({{0,0,0},{1,0,0}});t.settings.easeEnds=false;t.keys[0].values[1]=Quat{};
        t.keys[1].values[1]=axisAngle({0,0,1},delta*degree);auto c=cache(t);const auto& r=c.channels[1].rotation[0];
        require(angle(r.q[0],r.q[1])<=45*degree+1e-12&&angle(r.q[2],r.q[3])<=45*degree+1e-12,"F7 controls <=45deg");
        require(angle(r.q[1],r.q[2])<=175*degree+1e-12,"F7 ladder <=175deg");
        for(int i=0;i<=100;++i)
        {
            double u=i/100.;auto a=slerp(r.q[0],r.q[1],u),b=slerp(r.q[1],r.q[2],u),d=slerp(r.q[2],r.q[3],u);
            require(angle(a,b)<=175*degree+1e-12&&angle(b,d)<=175*degree+1e-12&&
                angle(slerp(a,b,u),slerp(b,d,u))<=175*degree+1e-12,"F7 ladder <=175deg");
        }
    }
    for(double delta:{171.,179.,180.})
    {
        auto t=track({{0,0,0},{1,0,0},{2,0,0},{3,0,0}});t.settings.easeEnds=false;
        t.keys[0].values[1]=axisAngle({0,0,1},-20*degree);t.keys[1].values[1]=Quat{};
        t.keys[2].values[1]=axisAngle({0,0,1},delta*degree);t.keys[3].values[1]=axisAngle({0,0,1},(delta+20)*degree);
        t.keys[0].out.value=.3;t.keys[1].out.value=4;t.keys[2].out.value=.7;auto c=cache(t);
        const auto& r=c.channels[1].rotation;
        require(angle(r[0].q[2],r[0].q[3])<1e-12&&angle(r[1].q[0],r[1].q[1])<1e-12&&
                angle(r[1].q[2],r[1].q[3])<1e-12&&angle(r[2].q[0],r[2].q[1])<1e-12,"F7 shared tangent");
    }
    auto t=track({{0,0,0},{1,0,0},{2,0,0}});t.settings.easeEnds=false;
    t.keys[0].out.value=.3;t.keys[1].out.value=5;t.keys[1].values[1]=axisAngle({1,1,0},.4);t.keys[2].values[1]=axisAngle({0,0,1},.7);
    auto c=cache(t);const auto& a=c.channels[1].rotation[0];const auto& b=c.channels[1].rotation[1];
    auto wl=log(a.q[3].inverse()*a.q[2])*(-6/.3),wr=log(b.q[0].inverse()*b.q[1])*(6/5.);
    require(length(wl-wr)<1e-10,"F7 angular physical C1");
}
Vec3 crossing(double t)
{
    double a=(-45+90*t)*degree;return {std::sin(a),0,std::cos(a)};
}
Quat runAim(int hz,double end=1)
{
    AimState state;state.initialized=true;state.presented=levelAim(crossing(0),60*degree);
    int n=int(std::round(hz*end));for(int i=1;i<=n;++i) state.update(crossing(double(i)/hz),0,1./hz);return state.presented;
}
void F8time()
{
    for(double dt:{.001,.017,.7})
    {
        AimState s;s.initialized=true;Vec3 pole{std::sin(5*degree),0,std::cos(5*degree)};s.presented=levelAim(pole,100*degree);auto before=s.presented;
        s.update(pole,0,dt);require(std::abs(s.integratedTime-dt)<1e-12,"F8 sum(h)==dt");
        require(std::abs(angle(before,s.presented)-30*degree*dt)<1e-6,"F8 rate times elapsed");
    }
    AimState s;s.initialized=true;Vec3 pole{std::sin(5*degree),0,std::cos(5*degree)};s.presented=levelAim(pole,100*degree);auto before=s.presented;double total=0;
    for(double dt:{.003,.011,.023,.007,.041,.7}) {s.update(pole,0,dt);total+=dt;}
    require(std::abs(angle(before,s.presented)-30*degree*total)<1e-6,"F8 irregular elapsed");
    for(double sign:{-1.,1.})
    {
        Vec3 f{std::sin(5*degree),0,sign*std::cos(5*degree)};std::array<Quat,2> results;
        for(int j=0;j<2;++j)
        {
            AimState tie;tie.initialized=true;tie.presented=levelAim(f,pi+(j?1:-1)*1e-13);auto initial=tie.presented;
            results[j]=tie.update(f,0,.001);
            require(log(initial.inverse()*results[j]).x>0,"F8 antipodal roll tie direction");
        }
        require(angle(results[0],results[1])<1e-10,"F8 antipodal roll tie agreement");
    }
    AimState stalled;stalled.initialized=true;stalled.presented={};
    require(angle(stalled.update({-1,0,0},0,1.1),levelAim({-1,0,0},0))<1e-12,"F8 >1s stall snaps before transport guard");
    stalled.presented=axisAngle({1,0,0},.3);auto held=stalled.presented;
    require(angle(stalled.update({0,0,1},0,1.1),held)<1e-12,"F8 >1s pole stall holds");
}
void F8()
{
    // Both poles, and continuous azimuth crossing the retired 150 degree reference boundary.
    for(double sign:{-1.,1.})
    {
        AimState state;Vec3 start{std::sin(-.7),0,sign*std::cos(-.7)};state.presented=levelAim(start,60*degree);state.initialized=true;
        for(int i=1;i<=1400;++i)
        {
            double a=-.7+i*.001;Vec3 f{std::sin(a),0,sign*std::cos(a)};Quat old=state.presented;
            state.update(f,0,.001);require(angle(old,state.presented)<=2*degree,"F8 max step");
        }
        require(angle(state.presented,levelAim({std::sin(.7),0,sign*std::cos(.7)},0))<.01*degree,"F8 level convergence");
    }
    AimState boundary;boundary.initialized=true;Vec3 f=unit(Vec3{.08*std::cos(149*degree),.08*std::sin(149*degree),1});boundary.presented=levelAim(f,100*degree);
    for(int i=0;i<1000;++i)
    {
        double az=(149+2.*i/999)*degree;Vec3 next{.08*std::cos(az),.08*std::sin(az),1};auto old=boundary.presented;
        boundary.update(next,-50*degree,.001);require(angle(old,boundary.presented)<=2*degree,"F8 max step");
    }
    double agreement=0;
    for(int i=1;i<=24;++i)
    {
        // Compare at shared sample times throughout the fast traversal, not just its end.
        AimState a,b;a.initialized=b.initialized=true;a.presented=b.presented=levelAim(crossing(0),60*degree);
        for(int j=1;j<=i;++j) a.update(crossing(j/24.),0,1./24);
        for(int j=1;j<=i*6;++j) b.update(crossing(j/144.),0,1./144);
        agreement=std::max(agreement,angle(a.presented,b.presented));
    }
    std::cout<<"F8 fast crossing 24/144 max_deg="<<agreement/degree<<'\n';
    require(agreement<=degree,"F8 24/144 agreement");
    auto laid=axisAngle({0,1,0},.5);
    require(angle(previewAim({},Vec3{.001,0,0},laid,0),laid)<1e-12,"F8 zero target laid");
    Quat previous=previewAim({},Vec3{-.06,0,0},laid,0);
    for(int j=1;j<=12000;++j)
    {
        auto now=previewAim({},Vec3{-.06+j*.00001,0,0},laid,0);
        require(finite(now)&&angle(previous,now)<=2*degree,"F8 through target point");previous=now;
    }
    auto t=track({{0,0,0}});t.keys[0].aim.mode=AimMode::OwnTarget;t.keys[0].aim.target.kind=RefKind::Object;
    auto c=cache(t);FakeResolver resolver;resolver.targets[0]={-.06,0,0};ClientRuntime runtime;runtime.wallDelta=.001;
    previous=evalCamera(t,c,0,resolver,EvalMode::Play,runtime).rotation;
    for(int j=1;j<=12000;++j)
    {
        resolver.targets[0]={-.06+j*.00001,0,0};
        auto now=evalCamera(t,c,0,resolver,EvalMode::Play,runtime).rotation;
        require(angle(previous,now)<=2*degree,"F8 Play target crossing max step");previous=now;
    }
    require(angle(previous,Quat{})<degree,"F8 Play recovers after target crossing");
    resolver.targets[0]={std::sin(5*degree),0,std::cos(5*degree)};runtime.reset();runtime.wallDelta=.001;
    t.keys[0].values[3]=60*degree;++t.revision;c=cache(t);
    auto initial=previewAim({},resolver.targets[0],Quat{},60*degree);
    for(int j=0;j<1000;++j) previous=evalCamera(t,c,0,resolver,EvalMode::Play,runtime).rotation;
    require(std::abs(angle(initial,previous)-30*degree)<1e-6,"F8 Play pole key roll");
}
void F8disc(int hz)
{
    for(double degrees:{151.,180.})
    {
        AimState aim;aim.initialized=true;auto old=aim.presented;
        auto forward=axisAngle({0,0,1},degrees*degree).rotate({1,0,0});
        for(int frame=0;frame<int(std::ceil(.25*hz))+1;++frame)
        {
            auto now=aim.update(forward,0,1./hz);
            require(angle(old,now)<=1440*degree/hz+1e-8,"F8jump discontinuity smooth bound");old=now;
        }
        require(dot(old.rotate({1,0,0}),forward)>1-1e-8,"F8jump discontinuity recovers within bound");
    }
}
void F8jump()
{
    // Pass through a fixed target fast enough to skip its entire blend zone.
    // 180 degrees at 720 deg/s must recover within .25 s after leaving
    // the 5 cm laid-pose blend zone (.025 s here), plus one frame.
    for(int hz:{24,30,60,144})
    {
        auto t=track({{-1,0,0},{1,0,0}});t.settings.easeEnds=false;
        t.keys[0].out.value=1;
        for(auto& key:t.keys) key.aim.mode=AimMode::FollowLock;
        t.lock.kind=RefKind::Point;t.lock.fallback={};auto c=cache(t);
        FakeResolver resolver;ClientRuntime runtime;runtime.wallDelta=1./hz;
        auto previous=evalCamera(t,c,0,resolver,EvalMode::Play,runtime).rotation;
        for(int frame=1;frame<=hz;++frame)
        {
            double time=double(frame)/hz;
            auto now=evalCamera(t,c,time,resolver,EvalMode::Play,runtime).rotation;
            // Forward transport and roll correction each have a 720 deg/s ceiling.
            require(finite(now)&&angle(previous,now)<=1440*degree/hz+1e-8,"F8jump crossing smooth bound");
            if(time>=.5+.025+.25+1./hz)
            {
                require(dot(now.rotate({1,0,0}),Vec3{-1,0,0})>1-1e-8,"F8jump crossing recovers within bound");
            }
            previous=now;
        }
        // Opposing laid orientations: the zero-distance assignment and the near-target blend
        // must obey the same orientation bound as the transport. Offsets 0 and 3 cm land the
        // crossing inside the blend zone (0 also inside the 1 cm laid assignment).
        for(double offset:{0.,.03,.2})
        {
            for(double laidDegrees:{180.,90.,0.})
            {
                auto lt=track({{-1,0,0},{1,0,0}});lt.settings.easeEnds=false;lt.keys[0].out.value=1;
                for(auto& key:lt.keys) {key.aim.mode=AimMode::FollowLock;key.values[1]=axisAngle({0,0,1},laidDegrees*degree);}
                lt.lock.kind=RefKind::Point;lt.lock.fallback={0,offset,0};auto lc=cache(lt);
                FakeResolver lresolver;ClientRuntime lruntime;lruntime.wallDelta=1./hz;
                auto before=evalCamera(lt,lc,0,lresolver,EvalMode::Play,lruntime).rotation;
                for(int frame=1;frame<=hz;++frame)
                {
                    auto now=evalCamera(lt,lc,double(frame)/hz,lresolver,EvalMode::Play,lruntime).rotation;
                    require(finite(now)&&angle(before,now)<=1440*degree/hz+1e-8,"F8jump laid crossing smooth bound");
                    before=now;
                }
                Vec3 expected=unit(Vec3{-1,offset,0});
                require(dot(before.rotate({1,0,0}),expected)>1-1e-8,"F8jump laid crossing recovers target facing");
            }
        }
        // Authored rotation must not be delayed by the aim-correction bound: two coincident
        // camera keys, a coincident point Lock and a 180 degree authored turn over 0.1 s,
        // then a hold. Play must equal Visit (the authored orientation) at every frame,
        // reach the endpoint at the authored time and not keep turning in the hold.
        {
            auto at=track({{0,0,0},{0,0,0}});at.settings.easeEnds=false;
            at.keys[0].out.mode=PaceMode::Duration;at.keys[0].out.value=.1;at.keys[1].hold=.5;
            at.keys[1].values[1]=axisAngle({0,0,1},pi);
            for(auto& key:at.keys) key.aim.mode=AimMode::FollowLock;
            at.lock.kind=RefKind::Point;at.lock.fallback={0,0,0};auto ac=cache(at);
            FakeResolver aresolver;ClientRuntime aruntime;aruntime.wallDelta=1./hz;
            double worst=0;
            for(int frame=0;double(frame)/hz<=ac.end+1e-9;++frame)
            {
                double time=double(frame)/hz;
                auto play=evalCamera(at,ac,time,aresolver,EvalMode::Play,aruntime).rotation;
                auto visit=eval(at,ac,time).rotation;
                worst=std::max(worst,angle(play,visit));
            }
            require(worst<1e-8,"F8jump authored turn matches Visit at every frame");
        }
        F8disc(hz);
    }
}
void F9()
{
    auto t=track({{0,0,0},{1,0,0},{2,0,0},{3,0,0}});t.settings.easeEnds=false;
    for(int i=0;i<4;++i) t.keys[i].values[2]=(i==0?20:i==1?80:i==2?81:82)*degree;
    auto c=cache(t);double last=0;
    for(int i=0;i<=10000;++i)
    {
        double f=eval(t,c,c.end*i/10000).fov;require(f>=last-1e-12&&f>=20*degree-1e-12&&f<=82*degree+1e-12,"F9 overshoot");last=f;
    }
}
void F10()
{
    auto t=track({{0,0,0},{1,2,0},{3,1,0}});auto c=cache(t);auto reference=eval(t,c,2.25);
    for(int hz:{24,30,60,144})
    {
        for(int j=0;j<hz*3;++j) (void)eval(t,c,double(j)/hz);
        auto p=eval(t,c,2.25);require(p.position==reference.position&&p.rotation==reference.rotation,"F10 identical eval same t");
        Smoother s;s.update({},true,0);
        for(int j=1;j<=hz;++j) s.update({double(j)/hz,0,0},true,1./hz);
        require(length(s.filtered-Vec3{1-.15+.15*std::exp(-1/.15),0,0})<1e-12,"F10 linear exact");
    }
    std::mt19937 random(17);Smoother irregular;irregular.update({},true,0);double time=0;
    while(time<1) {double dt=std::min(1-time,.001+double(random()%500)/10000);time+=dt;irregular.update({time,0,0},true,dt);}
    require(length(irregular.filtered-Vec3{1-.15+.15*std::exp(-1/.15),0,0})<1e-12,"F10 random linear exact");
    auto circle=[](int hz)
    {
        Smoother s;s.update({1,0,0},true,0);
        for(int j=1;j<=hz;++j) {double t=double(j)/hz;s.update({std::cos(t),std::sin(t),0},true,1./hz);}return s.filtered;
    };
    for(int hz:{24,30,60,144}) require(length(circle(hz)-circle(10000))<=.002,"F10 circle <=2mm");
    std::vector<Vec3> ends;
    for(int hz:{24,30,60})
    {
        Smoother s;s.update({},true,0);s.update({},false,1./hz);
        for(int j=0;j<hz/2;++j)
        {
            Vec3 old=s.filtered;s.update({3,0,0},true,1./hz);
            double bound=length(Vec3{3,0,0}-old)*(1-std::exp(-1./hz/.15));
            require(length(s.filtered-old)<=bound+1e-12,"F10 no snap");
        }
        ends.push_back(s.filtered);
    }
    for(auto e:ends) require(length(e-ends[0])<=.01,"F10 reacquire agreement <=1cm");
    auto targeted=track({{0,0,0},{1,0,0},{2,0,0},{3,0,0}});
    for(int j=0;j<4;++j)
    {
        targeted.keys[j].aim.mode=AimMode::OwnTarget;auto& r=targeted.keys[j].aim.target;r.kind=RefKind::Object;
        r.id[0]=std::uint8_t(j);r.label=std::string(1024,'a');
    }
    auto tc=cache(targeted);FakeResolver resolver;
    for(unsigned j=0;j<4;++j) resolver.targets[j]={double(j),2,1};
    ClientRuntime runtime;runtime.wallDelta=1./60;
    auto before=allocationCount;(void)evalCamera(targeted,tc,1,resolver,EvalMode::Play,runtime);
    (void)evalCamera(targeted,tc,4,resolver,EvalMode::Play,runtime);
    require(allocationCount==before,"F10 eval allocates nothing");
    require(resolver.calls==4,"F10 at most two target resolutions");
    resolver.calls=0;targeted.keys[1].aim.target=targeted.keys[0].aim.target;
    (void)evalCamera(targeted,tc,1,resolver,EvalMode::Play,runtime);
    require(resolver.calls==1,"F10 duplicate refs resolve once");
}
void F11()
{
    auto empty=cameraTrack();require(cache(empty).end==0,"F11 empty");auto one=track({{1,2,3}});auto c=cache(one);
    require(eval(one,c,100).position==Vec3{1,2,3},"F11 single holds");
    auto tiny=track({{0,0,0},{1,0,0}});tiny.keys[0].out.value=.001;require(closeEnough(cache(tiny).end,.001),"F11 1ms");
    auto bad=tiny;bad.keys[0].values[0]=Vec3{std::numeric_limits<double>::quiet_NaN(),0,0};std::string why;
    require(!buildCache(bad,c,why),"F11 NaN refused");
}
void F12()
{
    History h;Sequence s;
    for(int i=0;i<110;++i) {auto before=s;s.name=std::to_string(i);++s.revision;h.commit(before,s,"edit",i+1);}
    require(h.size(false)==100,"F12 cap 100");
    require(h.replay(false,s)&&h.size(true)==1,"F12 undo");auto before=s;++s.revision;s.name="new";h.commit(before,s,"new",120);
    require(h.size(true)==0,"F12 redo cleared");++s.revision;require(!h.ready(false,h.sequence(false),s,false),"F12 stale revision");--s.revision;
    h.beginGesture(s);s.name="drag";require(h.cancelGesture(s)&&s.name=="new","F12 gesture cancel");
    History refs;Sequence live;auto t=track({{0,0,0}});t.lock.kind=RefKind::Clone;t.lock.id[0]=1;appendTrack(live,t);
    for(int i=0;i<3;++i) {auto b=live;live.name=std::to_string(i);++live.revision;refs.commit(b,live,"ref",i+1);}
    refs.replay(false,live);refs.rekey(live,[](RefSlot& r){if(r.kind==RefKind::Clone) r.id[0]=42;});
    require(refs.replay(false,live)&&live.tracks[0].lock.id[0]==42,"F12 undo rekey");
    require(refs.replay(true,live)&&refs.replay(true,live)&&live.tracks[0].lock.id[0]==42,"F12 redo rekey");
    History identity;Sequence ids;auto initial=ids;auto oldId=appendTrack(ids,t);identity.commit(initial,ids,"add",1);
    auto oldRevision=ids.revision;require(identity.replay(false,ids),"F12 identity undo");identity.discardRedo();
    require(appendTrack(ids,t)>oldId&&ids.revision>oldRevision,"F12 IDs and revisions never reused");
}
void F12cancel()
{
    // Both Escape and capture loss use this production cancellation path.
    for(int exit=0;exit<2;++exit)
    {
        History h;Sequence live;auto t=track({{0,0,0}});
        t.lock.kind=RefKind::Clone;t.lock.id[0]=1;appendTrack(live,t);
        for(int i=0;i<3;++i) {auto before=live;live.name=std::to_string(i);++live.revision;h.commit(before,live,"edit",i+1);}
        require(h.replay(false,live),"F12cancel setup redo");
        auto saved=live;h.beginGesture(live);live.name="drag";++live.revision;++live.tracks[0].revision;
        for(int id:{42,43}) h.rekey(live,[&](RefSlot& ref){if(ref.kind==RefKind::Clone)ref.id[0]=id;});
        require(h.cancelGesture(live)&&live.name==saved.name&&live.tracks[0].lock.id[0]==43,"F12cancel restores rewritten gesture");
        require(h.ready(false,h.sequence(false),live,false)&&h.ready(true,h.sequence(true),live,false),"F12cancel rekey keeps both history boundaries");
        require(h.size(false)==2&&h.size(true)==1,"F12cancel redo retained");
        auto revision=live.revision;
        require(h.replay(true,live)&&live.name=="2"&&live.tracks[0].lock.id[0]==43&&live.revision>revision,"F12cancel redo works with fresh revision");
        require(h.replay(false,live)&&h.replay(false,live)&&live.name=="0"&&live.tracks[0].lock.id[0]==43,"F12cancel undo works");
    }
}
void F12b()
{
    alUndoSequenceClock()=0;alUndoSequenceGroup()=0;Sequence live;History h;bool owns=false;int edited=0;
    ALKeyframeHistoryAdapter a(h,live,[&](){return owns;},[&](){++edited;});
    auto commit=[&](const char* name){auto before=live;live.name=name;++live.revision;a.commit(before,name);};
    commit("key1");U64 key1=a.sequence(false),kit=alNextUndoSequence();commit("key2");U64 key2=a.sequence(false),setting=alNextUndoSequence();
    require(key1<kit&&kit<key2&&key2<setting,"F12b interleave order");
    owns=true;require(!a.ready(false,key2),"F12b ready playing refusal");owns=false;++live.revision;
    require(!a.ready(false,key2),"F12b ready revision refusal");--live.revision;
    require(!a.ready(false,kit),"F12b ready sequence refusal");
    a.replay(false);require(live.name=="key1"&&a.sequence(true)>setting,"F12b replay order");a.replay(true);require(live.name=="key2","F12b redo order");
    U64 replayedKey2=a.sequence(false);
    {ALScopedUndoSequence group;U64 groupedKit=alNextUndoSequence();{ALScopedUndoSequence nested;commit("grouped");}
     require(a.group_sequence(false)==groupedKit&&a.sequence(false)==groupedKit,"F12b group sequence");}
    a.discard_step(false,a.sequence(false));require(a.sequence(false)==replayedKey2,"F12b discard step");
    a.replay(false);a.discard_redo();require(a.sequence(true)==0,"F12b discard redo");require(edited==3,"F12b historyEdited commit only");
    // Execute a model of the page arbitration against three real clock consumers.
    // ALKitPage itself still needs the explicitly required in-world check.
    struct Simulated
    {
        std::vector<U64> undo,redo;
        void edit() {undo.push_back(alNextUndoSequence());redo.clear();}
        U64 sequence(bool r) const {const auto& s=r?redo:undo;return s.empty()?0:s.back();}
        void replay(bool r) {auto& from=r?redo:undo;auto& to=r?undo:redo;to.push_back(alNextUndoSequence());from.pop_back();}
    } kitHistory,settingsHistory;
    Sequence interleaved;History ih;ALKeyframeHistoryAdapter ia(ih,interleaved,[](){return false;},[](){});
    auto edit=[&](const char* label){auto b=interleaved;interleaved.name=label;++interleaved.revision;ia.commit(b,label);};
    edit("first");kitHistory.edit();settingsHistory.edit();edit("last");
    {ALScopedUndoSequence group;kitHistory.edit();edit("group partial");edit("group");}
    std::string undoOrder,redoOrder;
    auto replayTop=[&](bool redo)
    {
        auto seq=ia.sequence(redo),ks=kitHistory.sequence(redo),ss=settingsHistory.sequence(redo);
        U64 chosen=std::max({seq,ks,ss}); // exact ALKitPage policy for BOTH undo and redo
        std::string& log=redo?redoOrder:undoOrder;
        ALScopedUndoSequence replayGroup;
        if(seq==chosen) {require(ia.ready(redo,seq),"F12b arbitration ready");ia.replay(redo);log+='K';}
        if(ks==chosen) {kitHistory.replay(redo);log+='P';}
        if(ss==chosen) {settingsHistory.replay(redo);log+='S';}
        log+='|';
    };
    for(int i=0;i<5;++i) replayTop(false);
    for(int i=0;i<5;++i) replayTop(true);
    require(undoOrder=="KP|K|S|P|K|"&&redoOrder=="K|P|S|K|KP|","F12b interleave replay order");
}
void F13()
{
    Sequence s;s.name="roundtrip";s.uid="sequence";s.revision=18446744073709551000ull;
    auto t=track({{0,0,0},{1,0,0}});t.keys[1].values[3]=.456;t.lock.kind=RefKind::Clone;
    for(int i=0;i<16;++i) {t.lock.id[i]=std::uint8_t(i*13);t.lock.stableId[i]=std::uint8_t(255-i*11);}
    t.clientPayload=Doc(Doc::Object{{"opaque",Doc(std::string("future"))}});appendTrack(s,t);Sequence out;std::string why;
    require(decode(encode(s),out,why)&&out==s,"F13 round-trip");require(out.tracks[0].lock.id==t.lock.id&&out.tracks[0].lock.stableId==t.lock.stableId,"F13b clone bytes");
    auto unknown=s;unknown.tracks[0].kind="future-client";require(decode(encode(unknown),out,why)&&out==unknown,"F13 unknown kind preserved");
    auto bad=encode(s);std::get<Doc::Object>(bad.value)["version"]=Doc(999.);auto old=out;
    require(!decode(bad,out,why)&&out==old,"F13 atomic invalid load");
}
void F14()
{
    Sequence s;auto t=track({{0,0,0},{1,0,0}});auto id=appendTrack(s,t);require(id!=0&&appendTrack(s,t)==0,"F14 one camera");
    Track light;light.kind="light";light.schema={{"power",ChannelType::Scalar,Interp::Smooth,Space::Linear},{"enabled",ChannelType::Step,Interp::Step,Space::Linear}};
    light.keys={{1,0,{PaceMode::Duration,1},{0.,std::int64_t(0)}},{2,0,{PaceMode::Duration,1},{1.,std::int64_t(1)}}};light.nextKeyId=3;
    auto lid=appendTrack(s,light);require(lid>id,"F14 generic track identity");auto a=cache(s.tracks[0]),b=cache(s.tracks[1]);
    require(sequenceEnd({a,b})==3&&std::get<double>(evalChannel(s.tracks[1],b,0,2))==1,"F14 short track holds");
    require(std::get<std::int64_t>(evalChannel(s.tracks[1],b,1,.5))==0,"F14 generic step");
    light.schema[0].name="position";require(cache(light).position==-1,"F14 generic reserved names type checked");
}
void F15()
{
    auto slot=[](const char* name){SessionSlot s;s.id=name;s.seq.name=name;return s;};
    auto saved=[](const Sequence& s){return s.name[0]=='s';}; // 's*' = saved/empty, 'u*' = unsaved
    std::vector<SessionSlot> v{slot("u1"),slot("s1"),slot("u2"),slot("s2"),slot("u3")};
    std::vector<std::string> dropped;
    auto over=pruneSessions(v,3,saved,dropped);
    require(v.size()==3&&v[0].id=="u1"&&v[1].id=="u2"&&v[2].id=="u3","F15 unsaved never dropped");
    require(dropped==std::vector<std::string>{"s1","s2"}&&over==0,"F15 oldest disposable dropped first");
    v.push_back(slot("u4"));dropped.clear();
    over=pruneSessions(v,3,saved,dropped);
    require(dropped.empty()&&v.size()==4&&over==1,"F15 unsaved never dropped when over cap");
    v={slot("s1"),slot("s2")};dropped.clear();
    require(pruneSessions(v,16,saved,dropped)==0&&dropped.empty()&&v.size()==2,"F15 under cap untouched");
    // REAL disposal predicate (isDisposableSession) against a fake library of decoded files.
    {
        Sequence base;base.name="song";base.uid="uid-a";
        Track t=track({{1,2,3},{4,5,6}});t.lock.kind=RefKind::Clone;t.lock.id[0]=9; // clone ref survives save as stored
        require(appendTrack(base,t)!=0,"F15 fixture track valid");base.revision=7;base.tracks[0].revision=5;
        std::map<std::string,Sequence> library;
        auto decoded=[&](const Sequence& s){Sequence d;std::string why;require(decode(encode(s),d,why),"F15 decode");return d;};
        library["song"]=decoded(base); // saved file, decoded WITHOUT load-time reference rewriting
        auto loader=[&](const std::string& n,Sequence& out){auto f=library.find(n);if(f==library.end())return false;out=f->second;return true;};
        auto disposable=[&](const Sequence& s){return isDisposableSession(s,loader);};
        require(disposable(base),"F15 saved session disposable");
        Sequence restored=base;restored.uid="uid-b";restored.revision=99;restored.tracks[0].revision=42; // undo-restored: only bookkeeping differs
        require(disposable(restored),"F15 undo-restored session disposable");
        Sequence edited=base;edited.tracks[0].keys.pop_back();++edited.tracks[0].revision;
        require(!disposable(edited),"F15 edited session retained");
        Sequence renamed=base;renamed.name="other";
        require(!disposable(renamed),"F15 unsaved name retained");
        Sequence empty;empty.name="x";empty.tracks.push_back(cameraTrack());
        require(disposable(empty),"F15 empty session disposable");
    }
}
void B1()
{
    FakeResolver resolver;for(unsigned i=0;i<10000;++i) resolver.targets[i]={.1+double(i%3)*.01,0,1};
    volatile double consumed=0;
    for(int count:{200,2000})
    {
        Track t=cameraTrack();t.settings.easeEnds=false;
        for(int i=0;i<count;++i)
        {
            auto k=key({double(i)*.01,0,0});k.id=t.nextKeyId++;k.out.value=.1;k.aim.mode=AimMode::OwnTarget;
            k.aim.target.kind=RefKind::Object;k.aim.target.id[0]=std::uint8_t(i%256);k.aim.target.id[1]=std::uint8_t(i/256);
            // Keep every target in the pole band relative to its key.
            resolver.targets[unsigned(i)]={double(i)*.01+.05,0,1};t.keys.push_back(k);
        }
        auto c=cache(t);ClientRuntime runtime;runtime.wallDelta=1./24;std::vector<double> times;times.reserve(10000);
        std::mt19937 random(2026);
        for(int i=0;i<11000;++i)
        {
            double time=(double(random())/random.max())*(c.end-.1);
            auto begin=std::chrono::steady_clock::now();auto p=evalCamera(t,c,time,resolver,EvalMode::Play,runtime);
            auto end=std::chrono::steady_clock::now();consumed=consumed+p.fov;
            if(i>=1000) times.push_back(std::chrono::duration<double,std::milli>(end-begin).count());
        }
        std::sort(times.begin(),times.end());double p50=times[times.size()/2],p99=times[times.size()*99/100];
        std::cout<<"B1 keys="<<count<<" samples="<<times.size()<<" p50_ms="<<p50<<" p99_ms="<<p99<<" resolver_calls="<<resolver.calls<<'\n';
        require(p99<=.2,"B1 p99 <=0.2ms");
    }
    require(consumed>0,"B1 consumed output");
}
}
int main(int argc,char** argv)
{
    std::string selected=argc>1?argv[1]:"all";
    for(auto test:{std::pair{"F1",F1},{"F2",F2},{"F3",F3},{"F4",F4},{"F5",F5},{"F6",F6},{"F7",F7},
                   {"F8",F8},{"F8time",F8time},{"F8jump",F8jump},{"F8disc",[]{for(int hz:{24,30,60,144})F8disc(hz);}},{"F9",F9},{"F10",F10},{"F11",F11},{"F12",F12},{"F12cancel",F12cancel},{"F12b",F12b},{"F13",F13},{"F14",F14},{"F15",F15},{"B1",B1}})
        if(selected=="all" || selected==test.first) {test.second();std::cout<<"PASS "<<test.first<<'\n';}
}
