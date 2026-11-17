#include "alkeyframefit.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace ALKf
{
namespace
{
struct NullResolver final : Resolver {bool resolve(const RefSlot&,Vec3&) const override {return false;}};
FitError error(const Pose& a,const Pose& b)
{
    return {length(a.position-b.position),angle(a.rotation,b.rotation),std::abs(a.fov-b.fov)};
}
double ratio(FitError e,FitTolerance t) {return std::max({e.position/t.position,e.rotation/t.rotation,e.fov/t.fov});}
void accumulate(FitError& a,FitError b)
{
    a.position=std::max(a.position,b.position);a.rotation=std::max(a.rotation,b.rotation);a.fov=std::max(a.fov,b.fov);
}
bool validPose(const Pose& p)
{
    return finite(p.position)&&finite(p.rotation)&&dot(p.rotation,p.rotation)>1e-20 &&
           std::isfinite(p.fov)&&p.fov>0&&p.fov<pi&&std::isfinite(p.roll);
}
}
bool validateTakeHold(const TakeSource& source,std::size_t first,std::size_t last,FitTolerance tolerance)
{
    if(first>last || last>=source.size()) return false;
    auto base=source.sample(first).pose;
    for(std::size_t i=first;i<=last;++i)
        if(ratio(error(base,source.sample(i).pose),tolerance)>1) return false;
    return true;
}
FitResult fitTake(const TakeSource& source,const FitOptions& options)
{
    using Clock=std::chrono::steady_clock;auto start=Clock::now();
    FitResult result;
    auto elapsed=[&](){return std::chrono::duration<double>(Clock::now()-start).count();};
    auto refuse=[&](const char* why){result.reason=why;result.elapsedSeconds=elapsed();return result;};
    const auto tolerance=options.tolerance;
    if(!(std::isfinite(tolerance.position)&&tolerance.position>0&&std::isfinite(tolerance.rotation)&&tolerance.rotation>0&&
         std::isfinite(tolerance.fov)&&tolerance.fov>0&&std::isfinite(options.maxSeconds)&&options.maxSeconds>0)) return refuse("invalid tolerance/budget");
    std::size_t count=source.size(),cap=std::min<std::size_t>(400,options.maxKeys);
    double budget=std::min(2.,options.maxSeconds);
    if(!count || count>240000 || cap<std::min<std::size_t>(2,count)) return refuse("empty/oversized take or key cap");
    std::vector<TakeSample> samples;samples.reserve(count);
    for(std::size_t i=0;i<count;++i)
    {
        auto sample=source.sample(i);
        if(!std::isfinite(sample.time) || !validPose(sample.pose) || (i && sample.time<=samples.back().time)) return refuse("invalid original sample");
        samples.push_back(sample);
        if(elapsed()>budget*.8) return refuse("sample acquisition exceeded fit budget");
    }
    // Collapse a hold only when EVERY original sample passes all three tolerances.
    // A drift that exceeds tolerance relative to the first frame cannot be a hold.
    std::vector<std::size_t> holdEnd(count);
    for(std::size_t i=0;i<count;++i) holdEnd[i]=i;
    for(std::size_t i=0;i+2<count;)
    {
        std::size_t j=i+1;
        while(j<count && ratio(error(samples[j-1].pose,samples[j].pose),tolerance)<=1)
        {
            ++j;if(j%64==0 && elapsed()>budget*.8) return refuse("hold search exceeded fit budget");
        }
        if(j-i>=3 && samples[j-1].time-samples[i].time>=.1)
        {
            if(validateTakeHold(source,i,j-1,tolerance)) holdEnd[i]=j-1;
            i=j;
        }
        else ++i;
        if(elapsed()>budget*.8) return refuse("hold detection exceeded fit budget");
    }
    std::set<std::size_t> selected{0,count-1};
    // Validated holds are proposed only when they fit the same hard key cap.
    for(std::size_t i=0;i<count;++i) if(holdEnd[i]>i && selected.size()+2<=cap)
    {selected.insert(i);selected.insert(holdEnd[i]);}
    // Build directly from selected sample indices, skipping only the endpoint of a validated hold.
    auto build=[&]()
    {
        Track t=cameraTrack();t.settings.easeEnds=false;
        std::vector<std::size_t> indices;
        for(auto index:selected)
        {
            if(!indices.empty() && holdEnd[indices.back()]==index && holdEnd[indices.back()]>indices.back()) continue;
            indices.push_back(index);
        }
        for(std::size_t j=0;j<indices.size();++j)
        {
            auto i=indices[j],end=holdEnd[i];
            // Inserting a sample inside a previously proposed hold revokes that collapse.
            auto inside=selected.upper_bound(i);
            if(inside!=selected.end() && *inside<end) end=i;
            Key k;k.id=t.nextKeyId++;const auto& p=samples[i].pose;
            k.values={p.position,p.rotation,p.fov,p.roll};k.hold=samples[end].time-samples[i].time;
            if(j+1<indices.size()) k.out.value=samples[indices[j+1]].time-samples[end].time;
            t.keys.push_back(std::move(k));
        }
        return t;
    };
    NullResolver resolver;bool exhausted=false;FitResult lastValidated;bool haveValidated=false;
    for(;;)
    {
        result.track=build();Cache cache;std::string why;
        if(!buildCache(result.track,cache,why)) return refuse("fit cache refused (sample interval must be >=1ms)");
        double worstRatio=0;std::size_t worstIndex=0;result.worst={};result.validatedSamples=0;
        ClientRuntime runtime;
        for(std::size_t i=0;i<count;++i)
        {
            if(i%32==0 && elapsed()>budget*.9)
            {
                if(!haveValidated) return refuse("validation exceeded fit budget");
                lastValidated.verdict=FitVerdict::Approximate;lastValidated.reason="APPROXIMATE: time cap";
                lastValidated.elapsedSeconds=elapsed();return lastValidated;
            }
            Pose fitted=evalCamera(result.track,cache,samples[i].time-samples.front().time,resolver,EvalMode::Preview,runtime);
            FitError e=error(fitted,samples[i].pose);accumulate(result.worst,e);++result.validatedSamples;
            double r=ratio(e,tolerance);
            if(r>worstRatio && !selected.count(i)) {worstRatio=r;worstIndex=i;}
        }
        if(ratio(result.worst,tolerance)<=1) {result.verdict=FitVerdict::Pass;break;}
        lastValidated=result;haveValidated=true;
        if(exhausted || selected.size()>=cap || elapsed()>=budget*.8 || worstRatio<=1)
        {result.verdict=FitVerdict::Approximate;result.reason="APPROXIMATE: tolerance unmet at key/time cap";break;}
        selected.insert(worstIndex);
        exhausted=selected.size()==count;
    }
    result.elapsedSeconds=elapsed();
    if(result.elapsedSeconds>2) {result.verdict=FitVerdict::Approximate;result.reason="APPROXIMATE: final validation exceeded 2s budget";}
    return result;
}
bool appendFit(Sequence& s,const FitResult& result,std::uint32_t& id)
{
    // A sequence allows only one camera track. The page chooses a new sequence/library item
    // when a camera already exists; this function refuses rather than replacing it.
    if(result.verdict==FitVerdict::Refused) return false;
    Track t=result.track;t.id=0;t.uid="take-fit-"+std::to_string(s.nextTrackId);
    id=appendTrack(s,std::move(t));return id!=0;
}
}
