#include "../alkeyframefit.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace ALKf;
namespace
{
void require(bool condition,const char* name) {if(!condition) {std::cerr<<"FAIL "<<name<<'\n';std::exit(1);}}
struct Take final : TakeSource
{
    std::vector<TakeSample> samples;
    std::size_t size() const override {return samples.size();}
    TakeSample sample(std::size_t i) const override {return samples.at(i);}
    Pose evaluate(double t) const override
    {
        auto it=std::upper_bound(samples.begin(),samples.end(),t,[](double x,const TakeSample& s){return x<s.time;});
        if(it==samples.begin()) return samples.front().pose;if(it==samples.end()) return samples.back().pose;
        auto b=*it,a=*(it-1);double u=(t-a.time)/(b.time-a.time);
        return {a.pose.position+(b.pose.position-a.pose.position)*u,slerp(a.pose.rotation,b.pose.rotation,u),
                a.pose.fov+(b.pose.fov-a.pose.fov)*u,a.pose.roll+(b.pose.roll-a.pose.roll)*u};
    }
};
struct ResolverFake final : Resolver {bool resolve(const RefSlot&,Vec3&) const override {return false;}};
Take take120(bool wave=false)
{
    Take t;for(int i=0;i<=240;++i)
    {
        double time=i/120.;Pose p;p.position={time,wave?std::sin(time*12):0,0};p.rotation=axisAngle({1,0,0},time*.3);p.roll=time*.3;p.fov=(60+time)*degree;
        t.samples.push_back({10+time,p});
    }
    return t;
}
FitError independentlyMeasure(const Take& source,const Track& t)
{
    Cache c;std::string why;require(buildCache(t,c,why),"WP2 fitted cache");ResolverFake resolver;ClientRuntime rt;FitError error;
    for(const auto& sample:source.samples)
    {
        auto p=evalCamera(t,c,sample.time-source.samples.front().time,resolver,EvalMode::Preview,rt);
        error.position=std::max(error.position,length(p.position-sample.pose.position));error.rotation=std::max(error.rotation,angle(p.rotation,sample.pose.rotation));
        error.fov=std::max(error.fov,std::abs(p.fov-sample.pose.fov));
    }
    return error;
}
void drift()
{
    auto t=take120();for(auto& s:t.samples) {s.pose.position={.05*(s.time-10),0,0};s.pose.rotation={};s.pose.fov=60*degree;}
    FitOptions cap;cap.maxKeys=2;auto fitted=fitTake(t,cap);
    require(fitted.verdict!=FitVerdict::Refused&&fitted.track.keys.size()>1,"WP2 drift hold rejected");
    for(auto& s:t.samples) s.pose.position={};t.samples[119].pose.rotation=axisAngle({1,0,0},degree);
    require(!validateTakeHold(t,0,t.size()-1,{}),"WP2 hold rotation tolerance");
    t.samples[119].pose.rotation={};t.samples[119].pose.fov+=degree;
    require(!validateTakeHold(t,0,t.size()-1,{}),"WP2 hold FOV tolerance");
}
void timing()
{
    Take t;
    for(double time:{7.,7.01,7.04,7.2,7.43,7.9,8.4,9.})
    {
        Pose p;p.position={time-7,0,0};p.rotation=axisAngle({1,0,0},(time-7)*.2);p.roll=(time-7)*.2;t.samples.push_back({time,p});
    }
    auto result=fitTake(t);Cache c;std::string why;require(buildCache(result.track,c,why)&&std::abs(c.end-2)<1e-9,"WP2 exact timing");
    require(result.verdict==FitVerdict::Pass,"WP2 irregular times pass");
    auto e=independentlyMeasure(t,result.track);require(e.position<=.02&&e.rotation<=.5*degree&&e.fov<=.2*degree,"WP2 irregular original errors");
    // Pure hold collapse: duration and every original's rotation/FOV remain exact.
    for(auto& s:t.samples) s.pose=t.samples.front().pose;
    result=fitTake(t);require(result.verdict==FitVerdict::Pass&&result.track.keys.size()==1&&std::abs(result.track.keys[0].hold-2)<1e-9,"WP2 validated hold timing");
}
void rate120()
{
    auto t=take120();FitOptions options;options.maxKeys=2;t.samples[119].pose.position.y=1;
    auto result=fitTake(t,options);require(result.validatedSamples==241,"WP2 all 120Hz originals");
    auto e=independentlyMeasure(t,result.track);
    require(std::abs(e.position-result.worst.position)<1e-12,"WP2 original peak measured");
}
void honesty()
{
    auto t=take120(true);FitOptions options;options.maxKeys=2;
    auto r=fitTake(t,options);auto e=independentlyMeasure(t,r.track);
    require(r.verdict==FitVerdict::Approximate&&e.position>.02,"WP2 APPROXIMATE honesty");
    require(std::abs(r.worst.position-e.position)<1e-12&&std::abs(r.worst.rotation-e.rotation)<1e-12&&std::abs(r.worst.fov-e.fov)<1e-12,"WP2 worst errors honest");
    r=fitTake(t);std::cout<<"WP2 fitted keys="<<r.track.keys.size()<<" elapsed_s="<<r.elapsedSeconds<<" worst_m="<<r.worst.position
        <<" worst_rotation_deg="<<r.worst.rotation/degree<<" worst_fov_deg="<<r.worst.fov/degree<<'\n';
    require(r.track.keys.size()<=400&&r.elapsedSeconds<=2,"WP2 400keys/2s cap");
    auto measured=independentlyMeasure(t,r.track);
    require(r.verdict!=FitVerdict::Pass||(measured.position<=.02&&measured.rotation<=.5*degree&&measured.fov<=.2*degree),"WP2 final PASS valid");
    Sequence s;std::uint32_t id=0;require(appendFit(s,r,id)&&id!=0,"WP2 new track");auto before=s;
    require(!appendFit(s,r,id)&&s==before,"WP2 never overwrite existing camera");
}
}
int main(int argc,char** argv)
{
    std::string selected=argc>1?argv[1]:"all";
    for(auto f:{std::pair{"drift",drift},{"timing",timing},{"rate120",rate120},{"honesty",honesty}})
        if(selected=="all"||selected==f.first) {f.second();std::cout<<"PASS WP2 "<<f.first<<'\n';}
}
