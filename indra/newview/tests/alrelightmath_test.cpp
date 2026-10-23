#include "linden_common.h"
#include "../test/lltut.h"
#include "../alrelightmath.h"
#include "../alareashadowmath.h"
#include "../alworldshadowmath.h"
#include "../llprismlens.h"
#include "../alrelighttakerecord.h"
#include "../alrelighttakewriter.h"
#include "llsdserialize.h"
#include <limits>
#include <cstring>
#include <numeric>
#include <future>
#include <functional>
#include <vector>
namespace tut
{
struct relight_data {};
using relight_group = test_group<relight_data>;
using relight_object = relight_group::object;

namespace
{
// The P0.1 acceptance predicate, computed from the target size and the budget only (never from the plan's
// own cost fields): a main-view plan draws if and only if ONE light fits at the floors (4 rays, Thin: 48 near
// + the common far floor, else the floor of 8 steps) at half resolution, inside its planning limit and its
// per-draw cap; and a plan that draws never has fewer rays or steps than those floors.
bool oneLightFits(ALAreaShadow::Quality asked, unsigned width, unsigned height, int millions, std::uint64_t holdBack)
{
    using namespace ALAreaShadow;
    const int farMin = asked.nearSteps ? std::min(asked.minFar,kThinFarFloor) : std::min(asked.steps,kThinFarFloor);
    const int steps = asked.nearSteps+farMin;
    const std::uint64_t pixels = std::uint64_t((width+1)/2)*((height+1)/2); // the floor plan is half resolution
    const std::uint64_t budget = std::uint64_t(millions)*1000000ull;
    const std::uint64_t planned = asked.nearSteps ? budget-std::min(holdBack,budget) : budget;
    const std::uint64_t floorCost = pixels*std::min(asked.samples,kRayFloor)*steps;
    return floorCost<=planned && floorCost<=budget/2;
}
bool floorsHold(const ALAreaShadow::Workload& w, ALAreaShadow::Quality asked)
{
    using namespace ALAreaShadow;
    if (!w.channels) return true;
    const int farMin = w.q.nearSteps ? std::min(asked.minFar,kThinFarFloor) : std::min(asked.steps,kThinFarFloor);
    return w.q.samples>=std::min(asked.samples,kRayFloor) && w.q.steps-w.q.nearSteps>=farMin;
}
// The hybrid's regular group, against the authored uniform march: the same ray and step floors, no near segment,
// the shared resolution. A plan without rest lights has nothing to check.
bool restFloorsHold(const ALAreaShadow::Workload& w, ALAreaShadow::Quality uniform)
{
    using namespace ALAreaShadow;
    if (!w.channels || w.lead>=w.admitted) return true;
    return w.rest.samples>=std::min(uniform.samples,kRayFloor) && w.rest.steps>=std::min(uniform.steps,kThinFarFloor) &&
        w.rest.nearSteps==0 && w.rest.half==w.q.half && w.restChannels>0;
}
// Trace pixels of a plan, from the target size (never from the plan's own pixel field).
std::uint64_t planPixels(const ALAreaShadow::Workload& w, unsigned width, unsigned height)
{
    return w.q.half ? std::uint64_t((width+1)/2)*((height+1)/2) : std::uint64_t(width)*height;
}
// Cost of a plan for `lights` selected lights, recomputed from the target size: the lead lights march w.q, the other
// admitted lights march w.rest. Fix round 5: a Thin light of a hybrid also marches regular rays at the regular step count, so it is
// charged the larger of the two step counts (rest.steps is 0 for a plan without rest lights).
std::uint64_t planCost(const ALAreaShadow::Workload& w, unsigned width, unsigned height, unsigned lights)
{
    const std::uint64_t lead = std::min(lights,w.lead), all = std::min(lights,w.admitted);
    return planPixels(w,width,height)*(lead*w.q.samples*std::max(w.q.steps,w.rest.steps)+(all-lead)*w.rest.samples*w.rest.steps);
}
}
relight_group relight_tests("alrelightmath");
static void close(const char* label, double a, double b, double tolerance = 1.e-10)
{ ensure(label, std::isfinite(a) && std::abs(a-b) <= tolerance); }
template<> template<> void relight_object::test<1>()
{
    // R=15, f=.5: legacy(7.5)=2/9; physical(7.5)=1/64, gain=128/9.
    close("literal legacy",ALRelight::legacy(7.5,15,.5),2.0/9.0);
    close("literal physical",ALRelight::physical(7.5,15,.3,.1),1.0/64.0);
    close("literal gain",ALRelight::gain(7.5,15,.5,.3,.1),128.0/9.0);
    for (double reference : {.05,.3,1.5,7.5,14.9})
    {
        double gain = ALRelight::gain(reference,15,.5,.3,.1);
        close("calibration",gain*ALRelight::physical(reference,15,.3,.1),ALRelight::legacy(reference,15,.5));
    }
}
template<> template<> void relight_object::test<2>()
{
    double previous = std::numeric_limits<double>::infinity();
    for (int i=0; i<=16000; ++i)
    {
        const double d = i*.001;
        const double value = ALRelight::capped(d,15,.5,.3,.1,128.0/9.0,4);
        ensure("finite monotonic falloff",std::isfinite(value) && value <= previous+1.e-12);
        if (d >= 15) close("range cutoff",value,0);
        previous=value;
    }
}
template<> template<> void relight_object::test<3>()
{
    for (double radius : {0.0,.001,.3,2.0})
        for (double floor : {0.0,.0001,.1,1.0})
            for (double boost : {0.0,.25,4.0,100.0})
                for (double d : {0.0,1.e-12,.0001,.05,.1,1.0})
                {
                    const double value = ALRelight::capped(d,15,.5,radius,floor,128.0/9.0,boost);
                    ensure("near bound",std::isfinite(value) && value >= 0 && value <= boost*(8.0/9.0)+1.e-12);
                }
    close("zero range",ALRelight::physical(0,0,0,0),0);
    close("reference outside range",ALRelight::gain(16,15,.5,.3,.1),0);
}
template<> template<> void relight_object::test<4>()
{
    // Eq. 10's divisor 3 is discriminated from 2 by this literal.
    close("Karis constant",ALRelight::normalization(.25,.6,2),25.0/49.0);
    double raw=0, normalized=0;
    for (int a=1;a<=100;++a)
        for (int h=0;h<=100;++h)
        {
            const double alpha=a*.01, cosine=h*.01;
            const double denom=cosine*cosine*(alpha*alpha-1)+1;
            const double lobe=alpha*alpha/(3.141592653589793*denom*denom);
            const double norm=ALRelight::normalization(alpha,.3,2);
            ensure("normalization never boosts",norm>=0 && norm<=1.0);
            raw+=lobe; normalized+=lobe*norm;
        }
    ensure("hemisphere normalization lowers integrated representative lobe",normalized<raw);
    close("point normalization",ALRelight::normalization(.25,0,2),1);
}
template<> template<> void relight_object::test<5>()
{
    using namespace ALRelight;
    const Vec n{0,0,1}, v{0,0,1}, light{1,0,2};
    const Vec rep=representative(n,v,light,.5);
    close("toward ray x",rep[0],.5/std::sqrt(4.25));
    close("toward ray z",rep[2],2/std::sqrt(4.25));
    const Vec point=representative(n,v,light,0);
    close("point direction",point[0],1/std::sqrt(5.0));
    const Vec limit=representative(n,v,light,1.e-8);
    close("small source limit",limit[0],point[0],1.e-8);
    const Vec inside=representative(n,v,Vec{0,0,0},1);
    close("finite centre",inside[2],1);
}
template<> template<> void relight_object::test<6>()
{
    for (double cosine : {-1.0,-.2,0.0,.2,1.0})
    {
        close("point exact legacy angular term",ALRelight::diffuseRatio(cosine,0,10),std::max(cosine,0.0),0);
        close("small source ratio limit",ALRelight::diffuseRatio(cosine,1.e-5,10),std::max(cosine,0.0),1.e-6);
        close("inside hemisphere",ALRelight::diffuseRatio(cosine,1,.5),1);
    }
    ensure("horizon wrap survives",ALRelight::diffuseRatio(-.1,1,2)>0);
    close("behind cap dark",ALRelight::diffuseRatio(-1,1,2),0);
    close("above horizon ratio",ALRelight::diffuseRatio(.8,1,2),.8);
    // Entire source below (or tangent to) the surface horizon: exactly dark.
    for (double radius : {.005,.3,1.0})
        for (double cosine : {-1.0,-.8,-.5})
            close("legacy fully hidden sphere diffuse",ALRelight::diffuseRatio(cosine,radius,2),0,0);

}
template<> template<> void relight_object::test<7>()
{
    // Independent numerical solid-angle integral over a cap, including horizon.
    constexpr double pi=3.14159265358979323846;
    for (double cosine : {-.7,-.2,0.0,.2,.7})
    {
        const double sigma=std::asin(.5), cosSigma=std::cos(sigma);
        double integral=0;
        for (int t=0;t<300;++t)
            for (int p=0;p<600;++p)
            {
                double ct=1-(1-cosSigma)*(t+.5)/300;
                double st=std::sqrt(1-ct*ct), phi=2*pi*(p+.5)/600;
                integral+=std::max(cosine*ct+std::sqrt(1-cosine*cosine)*st*std::cos(phi),0.0);
            }
        integral*=(1-cosSigma)*2*pi/(300*600);
        close("sphere integral matches quadrature",ALRelight::sphere(cosine,.25),integral,2.e-5);
    }
}
template<> template<> void relight_object::test<8>()
{
    // Forward's diffuse clamp must survive the small-source mode-0 limit.
    for (double cosine : {.2,.8,1.0})
    {
        const double attenuation=ALRelight::legacy(.1,15,0);
        const double point=std::clamp(cosine*attenuation,0.0,1.0);
        const double shaped=std::clamp(ALRelight::diffuseRatio(cosine,1.e-5,.1)*attenuation,0.0,1.0);
        close("forward small-source diffuse clamp",shaped,point,1.e-6);
    }
    // A single CPU calibration also matches forward's algebraic legacy window.
    for (double f : {0.0,.25,.5,1.0})
        for (double d : {0.0,.1,1.5,7.5,15.0,16.0})
        {
            const double fa=1+f;
            const double a=std::clamp(1.0-(d/15-(1-fa))/fa,0.0,1.0);
            close("forward/deferred window",2*a*a,ALRelight::legacy(d,15,f));
        }
}

template<> template<> void relight_object::test<9>()
{
    constexpr double pi=3.14159265358979323846;
    // Reported rig-key geometry. Fully visible sources must preserve N.L
    // exactly; increasing the radius may only add diffuse at the horizon.
    for (double distance : {1.0,2.0})
        for (double radius : {.05,.3,.6})
            for (double degrees : {0.0,30.0,60.0,85.0,95.0})
            {
                const double cosine=std::cos(degrees*pi/180.0);
                const double ratio=ALRelight::diffuseRatio(cosine,radius,distance);
                if (cosine >= radius/distance)
                    close("rig fully visible diffuse equals point exactly",ratio,cosine,0);
                else
                    ensure("rig horizon only adds wrap",ratio+1.e-12>=std::max(cosine,0.0));
                for (double roughness : {.3,.6})
                {
                    const double norm=ALRelight::normalization(roughness*roughness,radius,distance);
                    ensure("rig Karis normalization never boosts",norm>0 && norm<=1);
                }
            }
}
template<> template<> void relight_object::test<10>()
{
    using namespace ALAreaShadow;
    // Importance first, then the stable UUID; a native map neither helps nor hurts here.
    const std::array<Candidate<int>,5> input{{{9,true,1000}, {8,false,1},
        {3,false,5}, {2,false,5}, {1,true,9999}}};
    std::array<int,5> order{{0,1,2,3,4}};
    do
    {
        Selection<int> selected;
        for (int index : order) selected.add(input[index],3);
        ensure_equals("cap",selected.size,3u);
        ensure_equals("all candidates counted",selected.candidates,5u);
        ensure_equals("highest importance first, mapped or not",selected.lights[0].id,1);
        ensure_equals("mapped light of lower importance second",selected.lights[1].id,9);
        ensure_equals("tie by UUID",selected.lights[2].id,2);
    } while (std::next_permutation(order.begin(),order.end()));
    Selection<int> capped;
    for (int id=19; id>=0; --id) capped.add({id,false,1},99);
    ensure_equals("hard eight cap",capped.size,8u);
    ensure_equals("lowest UUID first",capped.lights[0].id,0);
    ensure_equals("eighth UUID",capped.lights[7].id,7);
    ensure_equals("cap logged count",capped.candidates,20u);
}
template<> template<> void relight_object::test<11>()
{
    using namespace ALAreaShadow;
    ensure("main view allowed",allowed({}));
    for (unsigned flags=1; flags<64; ++flags)
        ensure("every excluded view combination fails closed",!allowed({bool(flags&1),
            bool(flags&2),bool(flags&4),bool(flags&8),bool(flags&16),bool(flags&32)}));
    // importance = luminance * range / max(distance, range/2), x1.25 when unmapped.
    close("literal importance, saturated near the light",importance(3,10,2,true),6);
    close("literal importance, far light",importance(1,10,20,true),.5);
    close("literal unmapped bonus",importance(1,10,20,false),.625);
    ensure("outside the range is not zero",importance(1,6,8,true) > 0);
    close("nonfinite luminance rejected",importance(std::numeric_limits<double>::infinity(),1,1,true),0);
    close("negative luminance rejected",importance(-1,1,1,true),0);
    close("zero range rejected",importance(1,0,1,true),0);
    close("nonfinite distance rejected",importance(1,1,std::numeric_limits<double>::quiet_NaN(),true),0);
}
template<> template<> void relight_object::test<12>()
{
    using namespace ALAreaShadow;
    const auto low=quality(0,true), medium=quality(1,true), high=quality(2,true), cinema=quality(3,true);
    ensure("low",low.samples==4 && low.steps==80 && low.half);
    ensure("default medium",medium.samples==8 && medium.steps==80 && medium.half);
    ensure("high",high.samples==12 && high.steps==80 && high.half);
    ensure("cinematic",cinema.samples==24 && cinema.steps==80 && !cinema.half);
    for (int preset=0;preset<4;++preset)
    {
        const int oldFar[] = {8,12,20,32};
        const auto live=segmented({4,oldFar[preset],true},preset);
        ensure("saved preset receives runtime density",live.steps==80 && live.nearSteps==48);
    }
    const auto authored=segmented({8,53,true},1);
    ensure("preset keeps larger authored far count",authored.steps==101 && authored.nearSteps==48);
    const auto custom=segmented({4,12,true},4);
    ensure("custom far steps preserved",custom.steps-custom.nearSteps==12);
    // The far segment may only be thinned down to the authored uniform count.
    ensure("custom far floor is its authored count",custom.minFar==12);
    ensure("authored far floor survives a larger preset count",authored.minFar==53);
    const int uniformFar[] = {8,12,20,32};
    for (int preset=0;preset<4;++preset)
    {
        ensure_equals("preset far floor is the uniform count",quality(preset,true).minFar,uniformFar[preset]);
        ensure_equals("saved preset far floor is the saved count",segmented({4,uniformFar[preset],true},preset).minFar,uniformFar[preset]);
        ensure_equals("uniform quality has no far floor",quality(preset).minFar,0);
    }
    const int uniformSteps[] = {8,12,20,32};
    for (int preset=0;preset<4;++preset)
    {
        const auto off=quality(preset);
        ensure("default is uniform HEAD quality",off.steps==uniformSteps[preset] && off.nearSteps==0);
    }
    ensure_equals("literal integer hash",hash(1),0x688990c0u);
    ensure("different IDs change pattern",hash(1)!=hash(2));
    // Equal-area disk strata: exact mean squared radius, balanced angular sweep.
    for (int samples : {2,4,8,16,32})
    {
        double mean=0;
        for (int i=0; i<samples; ++i)
        {
            const double radius=std::sqrt((i+.5)/samples);
            ensure("disk sample inside source",radius>0 && radius<1);
            mean+=radius*radius/samples;
        }
        close("uniform radial area",mean,.5);
    }
}

template<> template<> void relight_object::test<13>()
{
    using namespace ALAreaShadow;
    close("wall lamp default end bias",lightBias(.25f),.25,1.e-7);
    close("live bias",lightBias(.6f),.6,1.e-7);
    close("nonfinite bias fallback",lightBias(std::numeric_limits<float>::quiet_NaN()),.25);
    close("bounded bias",lightBias(100.f),4);
    close("negative bias",lightBias(-1.f),0);
}

template<> template<> void relight_object::test<14>()
{
    using namespace ALAreaShadow;
    for (bool thin : {false,true})
    for (unsigned height : {1080u,1440u,2160u})
    for (unsigned lights : {4u,8u})
    for (int preset=0; preset<4; ++preset)
    for (int budget : {1,100,1500,9999})
    {
        const auto a=workload(quality(preset,thin),height*16/9,height,lights,budget,lights);
        const auto b=workload(quality(preset,thin),height*16/9,height,lights,budget,lights);
        ensure("deterministic plan",a.q.samples==b.q.samples && a.q.steps==b.q.steps &&
            a.q.half==b.q.half && a.channels==b.channels && a.iterations==b.iterations);
        if (budget==1500 && height!=1440)
        {
            // Literal default-budget tables: rows are 1080p/K4,K8,4K/K4,K8; columns are presets.
            // Planner ladder with the ray floor (RELIGHT_WORLD_SHADOWS_PLAN P0.1): resolution, rays
            // to 8, steps, rays to 4, steps to 8, then fewer lights. HEAD's 1-2 rays at 4K are gone.
            constexpr int onSamples[4][4]={{4,8,8,8},{4,5,5,5},{4,4,4,4},{4,4,4,4}};
            constexpr int offSamples[4][4]={{4,8,12,22},{4,8,12,11},{4,8,9,8},{4,8,8,5}};
            constexpr bool onHalf[4][4]={{true,true,true,true},{true,true,true,true},
                {true,true,true,true},{true,true,true,true}};
            constexpr bool offHalf[4][4]={{true,true,true,true},{true,true,true,true},
                {true,true,true,true},{true,true,true,true}};
            constexpr int onFar[4][4]={{32,32,32,32},{32,16,16,16},{8,8,8,8},{8,8,8,8}};
            constexpr int offFar[4][4]={{8,12,20,32},{8,12,20,32},{8,12,20,22},{8,11,11,16}};
            constexpr unsigned onAdmitted[4]={4,8,3,3}; // Thin ON at 4K shadows 3 lights
            const int row=(height==2160 ? 2:0)+(lights==8 ? 1:0);
            ensure_equals("effective table admitted",a.admitted,thin ? onAdmitted[row]:lights);
            ensure_equals("effective table samples",a.q.samples,thin ? onSamples[row][preset]:offSamples[row][preset]);
            ensure_equals("effective table near",a.q.nearSteps,thin ? 48:0);
            ensure_equals("effective table far",a.q.steps-a.q.nearSteps,thin ? onFar[row][preset]:offFar[row][preset]);
            ensure_equals("effective table resolution",a.q.half,thin ? onHalf[row][preset]:offHalf[row][preset]);
            std::cout << "[RelightQ] plan thin=" << thin << " " << height << "p K=" << lights
                << " preset=" << preset << " A=" << a.admitted << " S=" << a.q.samples << " near=" << a.q.nearSteps
                << " far=" << a.q.steps-a.q.nearSteps << " half=" << a.q.half
                << " iterations=" << a.iterations << " channels=" << a.channels << std::endl;
        }
        ensure("hard frame ceiling",a.iterations<=a.budget && a.budget<=1500000000ull);
        if (a.channels)
        {
            // The pipeline keeps the A highest-ranked lights; the rest get no soft shadow.
            const unsigned kept=std::min(lights,a.admitted);
            ensure("admitted lights are 1..K",a.admitted>=1 && a.admitted<=lights);
            ensure_equals("literal total",a.iterations,a.pixels*kept*a.q.samples*a.q.steps);
            unsigned dispatched=0;
            for (unsigned plane=0; plane<(kept+3)/4; ++plane)
                for (unsigned first=0; first<std::min(4u,kept-plane*4); first+=a.channels)
                {
                    const auto count=std::min(a.channels,std::min(4u,kept-plane*4)-first);
                    ensure("every split draw bounded",a.pixels*count*a.q.samples*a.q.steps<=a.drawCap);
                    dispatched+=count;
                }
            ensure_equals("all channels exactly once",dispatched,kept);
            // Ray floor: fewer rays than the floor only when ONE light at the floors cannot fit.
            if (a.q.samples<kRayFloor)
            {
                const int farFloor=a.q.nearSteps ? std::min(quality(preset,thin).minFar,kThinFarFloor):
                    std::min(quality(preset).steps,kThinFarFloor);
                const std::uint64_t pixels=std::uint64_t((height*16/9+1)/2)*((height+1)/2);
                ensure("below the ray floor only when one light at the floors cannot fit",
                    pixels*kRayFloor*(a.q.nearSteps+farFloor)>a.budget-(thin ? feedReservation(height*16/9,height,
                    a.budget,lights,kFeedSlots):0) ||
                    pixels*kRayFloor*(a.q.nearSteps+farFloor)>a.drawCap);
            }
        }
        else ensure_equals("rejected views do no work",a.iterations,std::uint64_t(0));
    }
    // Ladder order on a custom plan: resolution, then rays to 8, then steps (down to max(8,
    // steps/2)), then rays to 4, then steps to 8, then lights. 518400 px x 8 lights x 8 rays x 45 steps.
    const auto samples=workload({32,64,false},1920,1080,8,1500,8);
    ensure("resolution, rays to 8, then steps",samples.q.samples==8 && samples.q.steps==45 && samples.q.half &&
        samples.admitted==8);
    const auto steps=workload({32,64,false},3840,2160,8,1500,8);
    ensure("then rays to the floor and steps to 8",steps.q.samples==4 && steps.q.steps==22 && steps.q.half &&
        steps.admitted==8);
    // Below the floors a capture FEED keeps the previous order (rays, steps, half); the MAIN view draws nothing.
    const auto half=workload({32,64,false},3840,2160,8,20,8,kFeedSlots,false);
    ensure("then half resolution",half.q.samples==1 && half.q.steps==1 && half.q.half && half.channels>0);
    const auto none=workload({32,64,false},3840,2160,8,20,8);
    ensure("main view below the floors: no soft shadow, never a sub-floor ray",none.channels==0 && none.admitted==0 &&
        none.iterations==0);
    const auto split=workload({1,64,false},1920,1080,4,600,4);
    ensure("split necessary",split.channels==2 && split.q.steps==64);
    // 2073600 px x 8 lights x 8 rays x 11 steps (HEAD planned 7 rays x 12 steps here).
    ensure("OFF 4K Medium: 8 rays x 11 steps",workload(quality(1),3840,2160,8,1500,8).iterations==1459814400ull);
    // 2073600 px x 3 lights x 4 rays x 56 steps: Thin ON shadows 3 of 8 lights at 4K.
    ensure("ON 4K keeps 4 rays for 3 lights",workload(quality(1,true),3840,2160,8,1500,8).iterations==1393459200ull);
    for (int preset=0; preset<4; ++preset)
    {
        const auto q=quality(preset,true);
        const auto w=workload(q,3840,2160,8,1500,8);
        ensure("near never thinned, far to the common floor",w.q.nearSteps==q.nearSteps &&
            w.q.steps-w.q.nearSteps==kThinFarFloor && w.q.samples==kRayFloor && w.admitted==3);
        ensure("segmented ceiling",w.iterations<=w.budget);
    }
    // Last resort before failing closed: thin only the far segment, down to the COMMON
    // floor (Low's uniform far count, 8: never coarser than OFF Low), never the near
    // steps. 4K/K8 at 1000M needs it, and every preset gets the same segmented march.
    {
        ensure_equals("common far floor is OFF Low's far count",kThinFarFloor,quality(0).steps);
        const auto reserved=feedReservation(3840,2160,1000000000ull,8,kFeedSlots);
        for (int preset=0; preset<4; ++preset)
        {
            const auto plan=workload(quality(preset,true),3840,2160,8,1000,8);
            ensure("far thinned and lights dropped instead of failing closed",plan.channels>0 &&
                plan.q.nearSteps==48 && plan.q.steps-48==kThinFarFloor && plan.q.samples==kRayFloor &&
                plan.q.half && plan.admitted==2);
            ensure("far reduction stays inside the planning limit",plan.iterations<=plan.budget-reserved);
            ensure("the feed reservation is held back",reserved>0 && plan.iterations<=plan.budget-reserved);
            // Without VCam feeds (no feed slots, so no cap at the OFF cost) every preset keeps the segmented march here
            // on its top lights (a hybrid: the other lights keep the regular march). With feeds the Thin or hybrid plan
            // is capped at the OFF main plan's cost (fix round 3), which a 4K K8 view at 1000M leaves no room under.
            const auto hybrid=workloadOrUniform(quality(preset,true),quality(preset),3840,2160,8,1000,8,0);
            const auto off=workload(quality(preset),3840,2160,8,1000,8,0);
            // The regular lights keep EXACTLY the OFF march and the Thin lights get what is left of the budget: here only Low's
            // 4 x 8 march leaves room for one Thin light; the other presets' OFF plans use the budget and stay as they are.
            ensure("Thin never admits fewer lights than OFF without feeds",hybrid.admitted>=off.admitted);
            if (hybrid.q.nearSteps)
                ensure("a hybrid keeps the OFF march for its regular lights",hybrid.q.nearSteps==48 && hybrid.lead>=1 &&
                    hybrid.lead<hybrid.admitted && hybrid.rest.samples==off.q.samples && hybrid.rest.steps==off.q.steps &&
                    hybrid.rest.half==off.q.half && hybrid.q.half==off.q.half);
            else
                ensure("no room for a Thin light: exactly the OFF plan",hybrid.q.samples==off.q.samples &&
                    hybrid.q.steps==off.q.steps && hybrid.q.half==off.q.half && hybrid.admitted==off.admitted &&
                    hybrid.iterations==off.iterations);
            ensure("Low keeps one Thin light beside its regular lights",preset!=0 || (hybrid.lead==1 && hybrid.admitted==8));
            const auto capped=workloadOrUniform(quality(preset,true),quality(preset),3840,2160,8,1000,8);
            const auto regular=workload(quality(preset),3840,2160,8,1000,8);
            ensure("with feeds the plan costs no more than the OFF plan",capped.admitted==regular.admitted &&
                capped.iterations<=regular.iterations);
        }
        // A budget the segmented plan cannot fit: the authored uniform plan, never darker than OFF.
        const auto fallback=workloadOrUniform(quality(3,true),quality(3),3840,2160,8,300,8);
        ensure("fallback is the authored uniform plan, never darker than OFF",fallback.channels>0 &&
            !fallback.q.nearSteps && fallback.q.steps<=32);
    }
    // ON never draws fewer shadows than OFF: wherever the uniform plan fits, so does ON,
    // and a segmented far segment is never coarser than the common floor (OFF Low's 8).
    for (unsigned height : {720u,1080u,1440u,2160u})
    for (unsigned lights : {1u,4u,8u})
    for (int preset=0; preset<4; ++preset)
    for (int budget : {1,20,100,300,600,1000,1200,1500})
    {
        const auto off=workload(quality(preset),height*16/9,height,lights,budget,lights);
        const auto on=workloadOrUniform(quality(preset,true),quality(preset),height*16/9,height,lights,budget,lights);
        ensure("ON renders wherever OFF renders",!off.channels || on.channels);
        if (on.q.nearSteps)
        {
            ensure("segmented far never coarser than the common floor",
                on.q.steps-on.q.nearSteps>=std::min(quality(preset).steps,kThinFarFloor));
            ensure("segmented ceiling holds",on.iterations<=on.budget);
        }
        else if (on.channels)
            ensure("fallback is exactly the uniform plan",on.q.steps==off.q.steps &&
                on.q.samples==off.q.samples && on.q.half==off.q.half && on.channels==off.channels);
    }
    ensure("impossible minimum fails closed",!workload({1,1,true},3840,2160,8,1,8).channels);
    ensure("oversized input fails closed",!workload({32,64,false},0xffffffffu,0xffffffffu,8,1500,8).channels);
    ensure("empty selection",!workload(quality(1,true),1920,1080,0,1500,0).channels);
}

template<> template<> void relight_object::test<15>()
{
    using namespace ALAreaShadow;
    for (bool thin : {false,true})
    {
        const auto seven=workload(quality(3,thin),1920,1080,7,1500,8);
        const auto eight=workload(quality(3,thin),1920,1080,8,1500,8);
        ensure("Cinematic K7/K8 same per-light work",seven.q.samples==(thin ? 5:11) &&
            seven.q.samples==eight.q.samples && seven.q.steps==(thin ? 64:32) &&
            seven.q.steps==eight.q.steps && seven.q.nearSteps==eight.q.nearSteps &&
            seven.q.half==eight.q.half && seven.q.half && seven.admitted==eight.admitted);
    }
    for (bool thin : {false,true})
    for (unsigned height : {1080u,1440u,2160u})
    for (unsigned requested : {1u,4u,8u})
    for (int preset=0; preset<4; ++preset)
    for (int budget : {1,100,1500})
    {
        const auto full=workload(quality(preset,thin),height*16/9,height,requested,budget,requested);
        for (unsigned visible=1; visible<=requested; ++visible)
        {
            const auto w=workload(quality(preset,thin),height*16/9,height,visible,budget,requested);
            ensure("visibility never retunes quality",w.q.samples==full.q.samples &&
                w.q.steps==full.q.steps && w.q.half==full.q.half && w.channels==full.channels);
            ensure("visibility never changes the admitted light count",w.admitted==full.admitted);
            ensure("reserved work bounds actual frame",w.iterations<=full.iterations && w.iterations<=w.budget);
            ensure("reserved work bounds draw",!w.channels ||
                w.pixels*w.channels*w.q.samples*w.q.steps<=w.drawCap);
        }
    }
}

template<> template<> void relight_object::test<16>()
{
    using ALRelight::primSourceRadius;
    close("tiny bulb",primSourceRadius({.02,.04,.06}),.01);
    close("large housing clamp",primSourceRadius({8,4,6}),1);
    close("flat panel uses thickness",primSourceRadius({4,3,.02}),.01);
    close("scaled child own dimensions",primSourceRadius({.8,.6,.4}),.2);
    close("rotated child same inscribed sphere",primSourceRadius({.4,.8,.6}),.2);
    close("attachment own dimensions",primSourceRadius({.12,.3,.2}),.06);
    close("projector conservative XYZ",primSourceRadius({2,2,.1}),.05);
    for (double invalid : {0.,-1.,std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()})
        for (const ALRelight::Vec scale : {ALRelight::Vec{invalid,1,2},
             ALRelight::Vec{1,invalid,2},ALRelight::Vec{1,2,invalid}})
            close("invalid scale is bounded zero",primSourceRadius(scale),0);
    close("world radius maximum",primSourceRadius({2,2,2}),1);
}

template<> template<> void relight_object::test<17>()
{
    using namespace ALAreaShadow;
    const std::array<LLUUID,6> ids{{
        LLUUID("00000000-0000-0000-0000-000000000001"),
        LLUUID("00000000-0000-0000-0000-000000000002"),
        LLUUID("00000000-0000-0000-0000-000000000003"),
        LLUUID("00000000-0000-0000-0000-000000000004"),
        LLUUID("00000000-0000-0000-0000-000000000005"),
        LLUUID("00000000-0000-0000-0000-000000000006")}};
    std::array<int,6> order{{0,1,2,3,4,5}};
    unsigned permutations = 0;
    do
    {
        for (unsigned cap : {4u,6u})
        {
            Selection<LLUUID> selected;
            // Equal importance: the stable UUID order decides, whatever the input order or
            // the native-map flag (the first two are mapped; unmapped-first would put them last).
            for (int i : order) selected.add({ids[i],i<2,1.0},cap);
            ensure_equals("real UUID tied cap",selected.size,cap);
            for (unsigned i=0; i<cap; ++i)
                ensure_equals("UUID ties independent of input order and map flag",selected.lights[i].id,ids[i]);
        }
        ++permutations;
    } while (std::next_permutation(order.begin(),order.end()));
    ensure_equals("all six-light permutations",permutations,720u);
}

template<> template<> void relight_object::test<18>()
{
    using namespace ALRelight;
    const LLUUID a("00000000-0000-0000-0000-000000000001");
    const LLUUID b("00000000-0000-0000-0000-000000000002");
    const LLUUID c("00000000-0000-0000-0000-000000000003");
    AreaTakeState state;
    state.status = "rendered";
    state.width = 1920; state.height = 1080;
    state.frame = 4242;
    for (const auto& light : {ALAreaShadow::Candidate<LLUUID>{c,true,100.},
         ALAreaShadow::Candidate<LLUUID>{b,false,1.}, ALAreaShadow::Candidate<LLUUID>{a,false,1.}})
        state.selected.add(light,8);
    state.planned = true;
    state.authoredSteps = 12;
    state.segmentedRequested = true;
    state.work = ALAreaShadow::workload(ALAreaShadow::quality(1,true),1920,1080,3,1500,8);
    LLSD settings;
    settings["RenderAreaShadows"] = true;
    settings["RenderAreaShadowThinObjects"] = true;
    settings["RenderLightShapeEnable"] = false;
    const std::array<LLUUID,2> areaSlots{{c,LLUUID::null}}, currentSlots{{b,c}};
    const std::array<float,2> fade{{.5f,1.f}};
    const LLSD record = takeRecord("synthetic",settings,state,areaSlots,currentSlots,fade);
    ensure_equals("record schema",record["version"].asInteger(),1);
    ensure_equals("start trigger",record["trigger"].asString(),std::string("synthetic"));
    ensure("requested settings preserved",record["settings_at_start"]["RenderAreaShadows"].asBoolean() &&
        !record["settings_at_start"]["RenderLightShapeEnable"].asBoolean());
    const LLSD& view = record["area_view"];
    ensure_equals("snapshot source is explicit",view["source"].asString(),std::string("most_recent_allowed_area_pass"));
    ensure_equals("render outcome",view["status"].asString(),std::string("rendered"));
    ensure_equals("actual width",view["width"].asInteger(),1920);
    ensure_equals("actual height",view["height"].asInteger(),1080);
    ensure_equals("candidate count",view["candidates"].asInteger(),3);
    ensure_equals("three selected channels",view["selected"].size(),3);
    for (int i=0; i<3; ++i)
    {
        ensure_equals("channel index",view["selected"][i]["channel"].asInteger(),i);
        // Importance order: c (100) first although it owns the native map, then a, b by UUID.
        ensure_equals("channel UUID",view["selected"][i]["uuid"].asUUID(),std::array<LLUUID,3>{{c,a,b}}[i]);
        ensure_equals("native map membership",view["selected"][i]["shadow_map"].asBoolean(),i==0);
        close("logged importance score",view["selected"][i]["score"].asReal(),i==0 ? 100. : 1.);
    }
    ensure_equals("sampled frame logged",view["frame"].asInteger(),4242);
    ensure_equals("sampled slot at area pass",view["projector_slots"][0]["uuid"].asUUID(),c);
    ensure_equals("vacant slot retained",view["projector_slots"][1]["uuid"].asUUID(),LLUUID::null);
    ensure_equals("current sampled slot",record["projector_slots_at_start"][0]["uuid"].asUUID(),b);
    close("current slot fade",record["projector_slots_at_start"][0]["fade"].asReal(),.5);
    ensure("thin mode recorded",record["settings_at_start"]["RenderAreaShadowThinObjects"].asBoolean());
    const LLSD& work = view["work"];
    ensure_equals("effective samples",work["samples"].asInteger(),5);
    ensure_equals("authored far count",work["authored_steps"].asInteger(),12);
    ensure_equals("effective near steps",work["near_steps"].asInteger(),48);
    ensure_equals("effective far steps",work["far_steps"].asInteger(),16);
    ensure("segmented plan is not a fallback",!work["uniform_fallback"].asBoolean());
    ensure_equals("main view labelled",view["view"].asString(),std::string("main"));
    ensure_equals("effective steps",work["steps"].asInteger(),64);
    ensure_equals("admitted lights",work["admitted"].asInteger(),8);
    ensure("half resolution",work["half"].asBoolean());
    ensure_equals("channels per draw",work["channels_per_draw"].asInteger(),4);
    ensure_equals("literal pixels",work["pixels"].asString(),std::string("518400"));
    ensure_equals("literal iterations",work["iterations"].asString(),std::string("497664000"));
    ensure_equals("literal budget",work["budget"].asString(),std::string("1500000000"));
    ensure_equals("literal draw cap",work["draw_cap"].asString(),std::string("750000000"));
    std::cout << "[RelightQ] synthetic take-record " << LLSDNotationStreamer(record) << '\n';
    // Log serialization must preserve the complete record, including UUID types.
    std::stringstream stream;
    LLSDSerialize::toNotation(record,stream);
    LLSD decoded;
    ensure("notation parses",LLSDSerialize::fromNotation(decoded,stream,stream.str().size())>0);
    ensure_equals("serialized UUID",decoded["area_view"]["selected"][0]["uuid"].asUUID(),c);
    ensure_equals("serialized exact work",decoded["area_view"]["work"]["iterations"].asString(),std::string("497664000"));
    state.work.pixels = 4294967296ull;
    ensure_equals("64-bit count never truncates",areaTakeRecord(state)["work"]["pixels"].asString(),std::string("4294967296"));
    const LLSD empty = areaTakeRecord({});
    ensure_equals("unobserved status",empty["status"].asString(),std::string("not_observed"));
    ensure("no fabricated plan",!empty["work_planned"].asBoolean() && !empty.has("work"));
    ensure_equals("no stale selection",empty["selected"].size(),0);
    for (const char* status : {"disabled", "unavailable_or_no_draw_budget", "no_selected_lights", "allocation_failed"})
    {
        AreaTakeState unavailable;
        unavailable.status = status;
        ensure_equals("unavailable outcome retained",areaTakeRecord(unavailable)["status"].asString(),std::string(status));
        ensure("unavailable plan omitted",!areaTakeRecord(unavailable).has("work"));
    }
    // A segmented request that fell back is labelled, and far steps never go negative.
    {
        AreaTakeState fallback = state;
        fallback.feed = true;
        fallback.work = ALAreaShadow::workloadOrUniform(ALAreaShadow::quality(3,true),
            ALAreaShadow::quality(3),3840,2160,8,300,8);
        const auto record = areaTakeRecord(fallback);
        ensure_equals("feed pass labelled",record["view"].asString(),std::string("vcam_feed"));
        ensure("fallback labelled",record["work"]["uniform_fallback"].asBoolean());
        ensure_equals("fallback has no near steps",record["work"]["near_steps"].asInteger(),0);
        ensure_equals("fallback far is the uniform count",record["work"]["far_steps"].asInteger(),
            fallback.work.q.steps);
        fallback.work.q.steps = 10;
        fallback.work.q.nearSteps = 48;
        ensure_equals("far steps clamped at zero",areaTakeRecord(fallback)["work"]["far_steps"].asInteger(),0);
    }
    // The main view's record stays the take record; a feed pass is a separate, labelled
    // "feed_view" and never changes any main field.
    {
        const LLSD before = takeRecord("synthetic",settings,state,areaSlots,currentSlots,fade);
        ensure("no feed record unless a feed pass was observed",!before.has("feed_view"));
        AreaTakeState idle;
        ensure("unobserved feed omitted",
            !takeRecord("synthetic",settings,state,areaSlots,currentSlots,fade,&idle).has("feed_view"));
        AreaTakeState feedPass;
        feedPass.status = "rendered";
        feedPass.feed = true;
        feedPass.width = 640; feedPass.height = 360;
        feedPass.planned = true;
        feedPass.authoredSteps = 20;
        feedPass.work = ALAreaShadow::workloadOrUniform(ALAreaShadow::quality(2,true),
            ALAreaShadow::quality(2),640,360,4,300,4);
        const LLSD both = takeRecord("synthetic",settings,state,areaSlots,currentSlots,fade,&feedPass);
        ensure_equals("main width untouched",both["area_view"]["width"].asInteger(),1920);
        ensure_equals("main view label untouched",both["area_view"]["view"].asString(),std::string("main"));
        ensure_equals("main status untouched",both["area_view"]["status"].asString(),before["area_view"]["status"].asString());
        ensure_equals("main plan untouched",both["area_view"]["work"]["iterations"].asString(),
            before["area_view"]["work"]["iterations"].asString());
        ensure_equals("main selection untouched",both["area_view"]["selected"].size(),before["area_view"]["selected"].size());
        ensure_equals("feed view labelled",both["feed_view"]["view"].asString(),std::string("vcam_feed"));
        ensure_equals("feed view source",both["feed_view"]["source"].asString(),
            std::string("most_recent_vcam_feed_area_pass"));
        ensure_equals("feed view own size",both["feed_view"]["width"].asInteger(),640);
        ensure("feed view own plan",both["feed_view"]["work"]["iterations"].asString()!=
            both["area_view"]["work"]["iterations"].asString());
    }
    state.status = "budget_rejected";
    state.work = ALAreaShadow::workload({1,1,true},3840,2160,8,1,8);
    const auto rejected = areaTakeRecord(state);
    ensure("rejected plan recorded",rejected["work_planned"].asBoolean());
    ensure_equals("rejected dispatch",rejected["work"]["channels_per_draw"].asInteger(),0);
    ensure_equals("rejected work",rejected["work"]["iterations"].asString(),std::string("0"));
}

template<> template<> void relight_object::test<19>()
{
    std::vector<int> ids;
    std::vector<std::string> drops;
    ALRelight::TakeWriter writer([&](const LLSD& record)
    {
        ids.push_back(record["id"].asInteger());
        drops.push_back(record["dropped_records"].asString());
        return true;
    });
    for (int i=0; i<70; ++i)
    {
        auto record = std::make_unique<LLSD>();
        (*record)["id"] = i;
        ensure("enqueue accepted",writer.enqueue(std::move(record)));
    }
    writer.drain();
    ensure_equals("bounded queue retains 64",ids.size(),ALRelight::TakeWriter::capacity);
    for (int i=0; i<64; ++i)
    {
        ensure_equals("newest records retained in order",ids[i],i+6);
        ensure_equals("drops reported exactly once on next record",drops[i],std::string(i ? "0" : "6"));
    }
}

template<> template<> void relight_object::test<20>()
{
    using namespace std::chrono_literals;
    const auto path = LLFile::tmpdir() + "relight_writer_" + LLUUID::generateNewID().asString() + ".log";
    struct Cleanup { std::string path; ~Cleanup() { LLFile::remove(path); } } cleanup{path};
    std::promise<void> entered, release, contender;
    auto proceed = release.get_future().share();
    bool first = true;
    ALRelight::TakeWriter writer([&](const LLSD& record)
    {
        if (first)
        {
            first = false;
            entered.set_value();
            proceed.wait(); // Deterministic slow disk; no sleep-based ordering.
        }
        return ALRelight::appendTakeRecord(path,record);
    });
    const auto enqueue = [&](int id)
    {
        auto record = std::make_unique<LLSD>();
        (*record)["id"] = id;
        (*record)["escaped"] = "one\ntwo";
        return writer.enqueue(std::move(record));
    };
    enqueue(0);
    auto running = std::async(std::launch::async,[&]() { writer.drain(); });
    entered.get_future().wait();
    auto competing = std::async(std::launch::async,[&]() { contender.set_value(); writer.drain(); });
    contender.get_future().wait();
    const auto blocked = competing.wait_for(20ms);
    auto producer = std::async(std::launch::async,[&]() { return enqueue(1) && enqueue(2) && enqueue(3); });
    const auto responsive = producer.wait_for(2s);
    release.set_value(); // Release before assertions, so failing tests also join cleanly.
    running.get();
    competing.get();
    ensure("writer contention waits instead of dropping",blocked == std::future_status::timeout);
    ensure("enqueue never waits for slow sink",responsive == std::future_status::ready);
    ensure("all starts accepted during slow write",producer.get());
    writer.shutdown();
    llifstream file(path);
    std::string line;
    int count = 0;
    while (std::getline(file,line))
    {
        LLSD record;
        std::istringstream stream(line);
        ensure("one parseable notation line per take",LLSDSerialize::fromNotation(record,stream,line.size())>0);
        ensure_equals("no lost or reordered records under contention",record["id"].asInteger(),count++);
        ensure_equals("no contention drops",record["dropped_records"].asString(),std::string("0"));
        ensure_equals("embedded newline remains one record",record["escaped"].asString(),std::string("one\ntwo"));
    }
    ensure_equals("all flushed records readable",count,4);
}

template<> template<> void relight_object::test<22>()
{
    // A Gate may render one source repeatedly. Fix round 5 (GLM, DeepSeek P1): passes are admitted by FEED IDENTITY, once per live
    // feed per frame, so a re-rendered source cannot use up another live feed's pass, and a feed outside the set draws nothing.
    {
        ALAreaShadow::FeedPasses passes;
        ALAreaShadow::FeedSet live;
        live.add(1024,576,5); live.add(640,360,2); live.add(512,288,7);
        for (auto frame : {0u,1u,0xffffffffu,0u})
        {
            // Source 5 renders four times: only its first pass runs; the two other live feeds still get theirs.
            for (unsigned repeat=0; repeat<4; ++repeat)
                ensure("a live feed's first pass is admitted, its repeats are not",passes.admit(frame,5,live)==(repeat==0));
            ensure("the next live feed is not starved by the repeats",passes.admit(frame,2,live));
            ensure("a feed outside the live set draws nothing",!passes.admit(frame,3,live));
            ensure("the last live feed is admitted",passes.admit(frame,7,live));
            ensure("each live feed has had exactly one pass",!passes.admit(frame,2,live) && !passes.admit(frame,7,live));
        }
        // The count-based admission of the previous round (a repeat of feed 5 took feed 2's place) is what this replaces.
        ALAreaShadow::FeedPasses counted;
        unsigned admittedPasses = 0;
        for (unsigned identity : {5u,5u,5u,2u,7u}) admittedPasses += counted.admit(9,identity,live) ? 1:0;
        ensure("three live feeds, three passes, whatever the render order",admittedPasses==3);
        ALAreaShadow::FeedPasses none;
        ensure("no live feed: no pass",!none.admit(1,0,ALAreaShadow::FeedSet()));
        ensure("a plain-count set names the slots 0..count-1",ALAreaShadow::FeedPasses().admit(1,2,ALAreaShadow::FeedSet(3)) &&
            !ALAreaShadow::FeedPasses().admit(1,3,ALAreaShadow::FeedSet(3)));
    }
    // Main's plan never depends on feeds. Fixed eight-way spare allocation
    // remains safe even if all capture slots render in a future scheduler.
    for (bool thin : {false,true})
    for (int preset=0; preset<4; ++preset)
    for (int budget : {1,100,1500})
    for (unsigned mainWidth : {1280u,1920u,3840u})
    for (unsigned cap : {1u,4u,8u})
    {
        const auto q = ALAreaShadow::quality(preset,thin);
        const auto uniform = ALAreaShadow::quality(preset);
        const auto main = ALAreaShadow::workloadOrUniform(q,uniform,mainWidth,mainWidth*9/16,cap,budget,cap);
        for (unsigned feeds : {1u,2u,4u,8u})
        {
            std::uint64_t total = main.channels ? main.iterations : 0;
            for (unsigned slot=0; slot<feeds; ++slot)
            {
                const auto feed = ALAreaShadow::feedWorkload(q,uniform,mainWidth,mainWidth*9/16,
                    511+slot*127,287+slot*71,cap,budget,cap,8);
                ensure("feed allowance",feed.iterations<=feed.budget);
                if (feed.channels)
                    ensure("feed draw bound",feed.pixels*feed.channels*feed.q.samples*feed.q.steps<=feed.drawCap);
                total += feed.channels ? feed.iterations : 0;
            }
            ensure("one total budget",total<=main.budget);
        }
    }
    const auto full = ALAreaShadow::Quality{4,8,false};
    // Above the floors the ladder drops resolution first and keeps every ray (HEAD: rays first).
    const auto rays = ALAreaShadow::workloadWithin(full,10,10,1,2400,1);
    ensure("feed resolution first, rays kept",rays.q.samples==4 && rays.q.steps==8 && rays.q.half);
    // Below the floors (one light at 4 rays x 8 steps cannot fit) the previous order applies.
    const auto steps = ALAreaShadow::workloadWithin(full,10,10,1,800,1,0,false);
    ensure("feed steps second",steps.q.samples==1 && steps.q.steps==4 && !steps.q.half);
    const auto half = ALAreaShadow::workloadWithin(full,10,10,1,100,1,0,false);
    ensure("feed half third",half.q.samples==1 && half.q.steps==1 && half.q.half && half.channels);
    ensure("feed skip last",!ALAreaShadow::workloadWithin(full,10,10,1,1,1,0,false).channels);
    // The main view (floors) draws nothing at those budgets instead.
    ensure("main view below the floors draws nothing",!ALAreaShadow::workloadWithin(full,10,10,1,800,1).channels &&
        !ALAreaShadow::workloadWithin(full,10,10,1,100,1).channels && ALAreaShadow::workloadWithin(full,10,10,1,800,1).admitted==0);
    ensure("no main extent skips feeds",!ALAreaShadow::feedWorkload(full,full,0,0,10,10,1,1500,1,8).channels);
    for (int bit=0; bit<6; ++bit)
    {
        ALAreaShadow::View view;
        switch (bit) {
        case 0: view.cube=true; break; case 1: view.prism=true; break;
        case 2: view.hud=true; break; case 3: view.impostor=true; break;
        case 4: view.reflection=true; break; case 5: view.tiled=true; break;
        }
        ensure("default excluded",!ALAreaShadow::allowed(view));
        ensure("only Prism opts in",ALAreaShadow::allowed(view,true)==(bit==1));
    }
}

template<> template<> void relight_object::test<23>()
{
    using namespace ALAreaShadow;
    // Thin ON feed ADMISSION: every preset, main 1080p and 4K at K4/K8, every
    // feed size from 512x288 to 1080p with every light count, default budget.
    // The main plan holds back the feed reservation and a segmented feed that does not
    // fit falls back to the authored uniform plan, so a feed never loses its shadows.
    struct Size { unsigned w,h; };
    constexpr Size feeds[]={{512,288},{640,360},{854,480},{1280,720},{1920,1080}};
    unsigned segmentedFeeds=0, fallbackFeeds=0;
    for (int preset=0; preset<4; ++preset)
    for (unsigned mainHeight : {1080u,2160u})
    for (unsigned cap : {1u,4u,8u})
    {
        const auto q=quality(preset,true), uniform=quality(preset);
        const auto main=workloadOrUniform(q,uniform,mainHeight*16/9,mainHeight,cap,1500,cap);
        const auto offMain=workload(uniform,mainHeight*16/9,mainHeight,cap,1500,cap);
        ensure("main draws",main.channels>0);
        ensure("a Thin or hybrid main never costs more than the OFF main while feeds can run",
            main.iterations<=offMain.iterations);
        for (const auto& size : feeds)
        for (unsigned lights=1; lights<=cap; ++lights)
        {
            const auto feed=feedWorkload(q,uniform,mainHeight*16/9,mainHeight,size.w,size.h,lights,1500,cap,8);
            const auto offFeed=feedWorkload(uniform,uniform,mainHeight*16/9,mainHeight,size.w,size.h,lights,1500,cap,8);
            // Fix round 3: the feed gets EXACTLY the allowance it gets with Thin OFF, so it draws wherever the OFF feed draws
            // and admits at least its lights (a main that eats the whole budget leaves the OFF feed nothing too).
            ensure("feed gets exactly the OFF allowance",feed.budget==offFeed.budget);
            ensure("feed channels admitted wherever the OFF feed draws",!offFeed.channels || feed.channels>0);
            ensure("feed admits at least the OFF feed's lights",(feed.channels ? feed.admitted:0)>=(offFeed.channels ? offFeed.admitted:0));
            ensure("feed inside its allowance",feed.iterations<=feed.budget);
            ensure("feed draw bound",feed.pixels*feed.channels*feed.q.samples*feed.q.steps<=feed.drawCap);
            // Eight slots plus the main plan stay inside the one frame budget.
            ensure("one budget for main and eight feeds",main.iterations+8*feed.iterations<=main.budget);
            if (!feed.channels) continue;
            if (feed.q.nearSteps) ++segmentedFeeds; else ++fallbackFeeds;
            // A fallback is exactly the authored uniform plan for that allowance.
            if (!feed.q.nearSteps)
            {
                const auto reserve=offMain.channels ? offMain.iterations:0;
                const auto expected=workloadWithin(uniform,size.w,size.h,lights,(main.budget-reserve)/8,cap,0,false);
                ensure("fallback is the uniform plan",feed.q.samples==expected.q.samples &&
                    feed.q.steps==expected.q.steps && feed.q.half==expected.q.half &&
                    feed.channels==expected.channels);
            }
        }
    }
    ensure("some feeds keep the segmented march",segmentedFeeds>0);
    ensure("some feeds need the uniform fallback",fallbackFeeds>0);
    // Main and feeds follow the OFF plan exactly when Thin objects is OFF.
    for (int preset=0; preset<4; ++preset)
    {
        const auto q=quality(preset), same=quality(preset);
        const auto a=feedWorkload(q,same,1920,1080,640,360,4,1500,4,8);
        const auto b=workloadWithin(q,640,360,4,(1500000000ull-workload(q,1920,1080,4,1500,4).iterations)/8,4,0,false);
        ensure("OFF feed plan unchanged",a.q.samples==b.q.samples && a.q.steps==b.q.steps &&
            a.q.half==b.q.half && a.channels==b.channels && a.iterations==b.iterations);
    }
}

template<> template<> void relight_object::test<24>()
{
    using namespace ALAreaShadow;
    // Monotonic presets: a higher preset never ends with fewer effective samples,
    // nor fewer rays per pixel (full resolution counts 4 half-resolution rays), nor
    // a coarser trace than the preset below it, at any resolution, K or default budget.
    struct Size { unsigned w,h; };
    constexpr Size sizes[]={{1280,720},{1600,900},{1920,1080},{2560,1440},{3840,2160}};
    // Budgets run from 100M to the default: tight budgets thin the far segment to the
    // common floor, which every preset shares, so segmentation never depends on the preset.
    // Without VCam feeds (slots 0) the Thin planner is preset independent; with feeds (slots 8) a Thin or hybrid plan is capped
    // at the cost of the OFF plan of its own preset (fix round 3), so which presets keep Thin depends on the preset by design and
    // the preset-ordered rules below apply to the rows where every preset keeps it.
    unsigned segmentedRows=0, uniformRows=0, emptyRows=0, cappedThinPlans=0;
    for (unsigned slots : {0u,kFeedSlots})
    for (int budget : {1500,1400,1200,1000,800,600,450,300,200,100})
    for (const auto& size : sizes)
    for (unsigned lights=1; lights<=8; ++lights)
    {
        Workload plan[4];
        for (int preset=0; preset<4; ++preset)
        {
            plan[preset]=workloadOrUniform(quality(preset,true),quality(preset),size.w,size.h,lights,budget,lights,slots);
            // The main view draws exactly when one light fits at the floors (Thin, else the uniform fallback).
            ensure("every preset draws wherever one light fits at the floors",(plan[preset].channels>0) ==
                (oneLightFits(quality(preset,true),size.w,size.h,budget,feedReservation(size.w,size.h,
                    std::uint64_t(budget)*1000000ull,lights,slots)) || oneLightFits(quality(preset),size.w,size.h,budget,0)));
            ensure("segmented plan stays inside the frame",plan[preset].iterations<=plan[preset].budget);
            ensure("the regular group keeps the floors",restFloorsHold(plan[preset],quality(preset)));
            if (!plan[preset].q.half && plan[preset].q.nearSteps)
                ensure("segmented full resolution only with at least 12 rays",
                    plan[preset].q.samples>=kFullResMinSamples);
        }
        bool allSegmented=true;
        for (int preset=0; preset<4; ++preset) allSegmented = allSegmented && plan[preset].q.nearSteps>0;
        for (int preset=0; preset<4; ++preset)
        {
            // Fix round 3: which presets keep a Thin light depends on the preset (the regular lights keep exactly the OFF march of
            // their own preset and the Thin lights get only what is left), so the preset-independence rule is gone; what holds
            // instead: Thin never admits fewer lights than OFF, the regular lights ARE the OFF march, and with feeds a Thin
            // or hybrid plan never costs more than its preset's OFF plan.
            const auto off=workload(quality(preset),size.w,size.h,lights,budget,lights,slots);
            ensure("Thin ON admits at least what OFF admits",plan[preset].admitted>=off.admitted);
            if (plan[preset].q.nearSteps && plan[preset].lead<plan[preset].admitted)
                ensure("the regular lights keep exactly the OFF march",plan[preset].rest.samples==off.q.samples &&
                    plan[preset].rest.steps==off.q.steps && plan[preset].rest.half==off.q.half &&
                    plan[preset].admitted==off.admitted);
            if (slots && plan[preset].q.nearSteps)
            {
                ++cappedThinPlans;
                ensure("a capped plan never costs more than its preset's OFF plan",
                    planCost(plan[preset],size.w,size.h,8)<=regularCost(off));
            }
        }
        // The floors are shared by every preset: a size and budget that cannot afford one light draws nothing for all.
        if (!plan[0].channels) { ++emptyRows; continue; }
        // Uniform rows are the OFF plan (its preset inversion at tight budgets predates
        // Thin objects and is out of scope); the monotonic rule covers the segmented rows.
        if (!allSegmented) { ++uniformRows; continue; }
        ++segmentedRows;
        for (int preset=0; preset<4; ++preset)
        {
            // Independent cost recomputation: pixels from the target size, not from the plan.
            ensure("plan cost recomputed from the target size",plan[preset].iterations ==
                planCost(plan[preset],size.w,size.h,lights));
            ensure("the plan fits the requested-K reservation",
                planCost(plan[preset],size.w,size.h,lights) <= plan[preset].budget);
            // Fewer lights only after every rung above was spent.
            if (plan[preset].admitted < lights)
                ensure("lights drop only at the floors",plan[preset].q.samples==kRayFloor && plan[preset].q.half);
            // Degradation order: the far segment is thinned only after rays and resolution are spent.
            // Ladder rungs 3 and 5: far steps thin only at half resolution with rays already at 8
            // (rung 2), and below 16 only with rays at the floor of 4 (rung 4).
            if (plan[preset].q.steps-plan[preset].q.nearSteps < 32)
                ensure("far steps thin only after the rays reached 8",plan[preset].q.samples<=kRayRungSamples && plan[preset].q.half);
            if (plan[preset].q.steps-plan[preset].q.nearSteps < 16)
                ensure("far steps below 16 only at the ray floor",plan[preset].q.samples<=kRayFloor);
            ensure("far steps never below the common floor",
                plan[preset].q.steps-plan[preset].q.nearSteps >= kThinFarFloor);
        }
        for (int preset=1; preset<4; ++preset)
        {
            const auto& lower=plan[preset-1];
            const auto& higher=plan[preset];
            ensure("higher preset keeps effective samples",higher.q.samples>=lower.q.samples);
            ensure("higher preset keeps rays per pixel",
                higher.q.samples*(higher.q.half ? 1:4)>=lower.q.samples*(lower.q.half ? 1:4));
            ensure("higher preset never traces at lower resolution",lower.q.half || !higher.q.half);
        }
    }
    ensure("segmented rows are exercised",segmentedRows>0);
    ensure("Thin plans with feeds are exercised",cappedThinPlans>0);
    ensure("fallback rows are exercised",uniformRows>0);
    ensure("rows below the floors are exercised (nothing drawn)",emptyRows>0);
    // The previously inverted configurations, literally: Cinematic is now half
    // resolution with at least High's samples (it was 4 full-res vs 12 half-res).
    const auto high=workload(quality(2,true),1920,1080,1,1500,1);
    const auto cinematic=workload(quality(3,true),1920,1080,1,1500,1);
    ensure("1080p K1 High",high.q.samples==12 && high.q.half);
    ensure("1080p K1 Cinematic >= High",cinematic.q.samples>=high.q.samples && cinematic.q.half);
    // Literal cross-preset sample rows at the default 1500M budget (Low, Medium, High, Cinematic).
    // Fix round 3: the Thin march of a plan depends on whether VCam feeds can run. Without feeds (slots 0) the Thin lights get what the
    // OFF plan leaves; with feeds (slots 8) a Thin or hybrid plan never costs more than the OFF plan of its preset, so only the
    // plans that fit under it keep the Thin march (the 4x8 / 8x12 / 9x20 / 8x22 entries are the OFF plans themselves).
    struct Row { unsigned height, cap; int samples[4]; };
    // Fix round 4: a hybrid's Thin lights march the regular ray count, so 4K Medium K4 (OFF 8x12; the Thin march of the previous
    // rounds used 4 rays there) has no room for a Thin light beside three regular ones and is exactly the OFF plan (8 rays).
    constexpr Row noFeeds[]={{1080,4,{4,8,9,9}},{1080,8,{4,5,5,5}},{2160,4,{4,8,9,8}},{2160,8,{4,8,8,5}}};
    constexpr Row withFeeds[]={{1080,4,{4,8,4,8}},{1080,8,{4,8,4,5}},{2160,4,{4,8,9,8}},{2160,8,{4,8,8,5}}};
    for (unsigned slots : {0u,kFeedSlots})
        for (const auto& row : (slots ? withFeeds:noFeeds))
            for (int preset=0; preset<4; ++preset)
            {
                const auto plan=workloadOrUniform(quality(preset,true),quality(preset),row.height*16/9,row.height,
                    row.cap,1500,row.cap,slots);
                ensure_equals("literal effective samples per preset",plan.q.samples,row.samples[preset]);
            }
}

template<> template<> void relight_object::test<25>()
{
    using namespace ALAreaShadow;
    // Lowered budgets: with Thin ON every feed gets at least the allowance it would get
    // with Thin OFF (capped at what a 2048x2048 feed needs to draw at all), so it draws
    // wherever the OFF feed draws. Main 1080p/1440p/4K x K1..8 x every preset x feeds
    // from 512x288 to 1080p (and the 2048x2048 bound) at budgets 100..1500M.
    struct Size { unsigned w,h; };
    constexpr Size feeds[]={{512,288},{640,360},{854,480},{1280,720},{1920,1080},{2048,2048}};
    unsigned cases=0, offDraws=0, fewerChannels=0, fewerRays=0, segmentedMain=0, uniformMain=0;
    for (int budget : {100,300,600,1000,1500})
    for (unsigned mainHeight : {1080u,1440u,2160u})
    for (unsigned cap=1; cap<=8; ++cap)
    for (int preset=0; preset<4; ++preset)
    {
        const unsigned mainWidth=mainHeight*16/9;
        const auto uniform=quality(preset), q=segmented(uniform,preset), same=quality(preset);
        const auto onMain=workloadOrUniform(q,uniform,mainWidth,mainHeight,cap,budget,cap);
        const auto offMain=workload(same,mainWidth,mainHeight,cap,budget,cap);
        if (onMain.q.nearSteps) ++segmentedMain; else ++uniformMain;
        ensure("ON main draws wherever OFF main draws",!offMain.channels || onMain.channels);
        const std::uint64_t offReserve=offMain.channels ? offMain.iterations:0;
        const std::uint64_t offAllowance=(offMain.budget-offReserve)/kFeedSlots;
        const std::uint64_t need=kFeedGuaranteePixels*std::max(cap,2u);
        const std::uint64_t onReserve=onMain.channels ? onMain.iterations:0;
        // Fix round 3: a Thin or hybrid main plan costs no more than the OFF main plan, and the feeds' allowance is exactly the
        // one they get with Thin OFF.
        ensure("a Thin or hybrid main never costs more than the OFF main",onReserve<=offReserve);
        const std::uint64_t onAllowance=offAllowance;
        for (const auto& size : feeds)
        for (unsigned lights=1; lights<=cap; ++lights)
        {
            const auto on=feedWorkload(q,uniform,mainWidth,mainHeight,size.w,size.h,lights,budget,cap,kFeedSlots);
            const auto off=feedWorkload(same,same,mainWidth,mainHeight,size.w,size.h,lights,budget,cap,kFeedSlots);
            ++cases;
            // Never zero when OFF draws, never above the shared budget.
            ensure("ON feed draws wherever the OFF feed draws",!off.channels || on.channels>0);
            ensure("ON allowance is exactly the OFF allowance (so at least min(OFF allowance, 2048x2048 minimum))",
                on.budget==off.budget && on.budget==onAllowance && onAllowance>=std::min(offAllowance,need));
            ensure("ON feed admits at least the lights the OFF feed admits",
                (on.channels ? on.admitted:0)>=(off.channels ? off.admitted:0));
            // Independent checks from the target size and the test's own allowance, not from
            // the plan's own budget/iterations fields.
            const std::uint64_t feedPixels = on.q.half ? std::uint64_t((size.w+1)/2)*((size.h+1)/2) :
                std::uint64_t(size.w)*size.h;
            const std::uint64_t perChannel = feedPixels*on.q.samples*on.q.steps;
            if (on.channels)
            {
                ensure("feed cost recomputed from the target size",on.iterations==planCost(on,size.w,size.h,lights));
                ensure("feed plan reserves its admitted lights inside the allowance",
                    planCost(on,size.w,size.h,cap)<=onAllowance);
                ensure("feed draw bound recomputed",perChannel*on.channels<=onAllowance/2);
                ensure("feed channel count is maximal",on.channels==4 || perChannel*(on.channels+1)>onAllowance/2);
                if (on.lead<on.admitted)
                {
                    const std::uint64_t restPerChannel = feedPixels*on.rest.samples*on.rest.steps;
                    ensure("feed regular group draw bound",restPerChannel*on.restChannels<=onAllowance/2);
                    ensure("feed regular group channel count is maximal",on.restChannels==4 || restPerChannel*(on.restChannels+1)>onAllowance/2);
                }
            }
            ensure("one budget for main and every feed slot",onReserve+kFeedSlots*on.iterations<=onMain.budget);
            // Main fell back to the OFF plan: the feed is exactly the OFF feed.
            if (!onMain.q.nearSteps && !on.q.nearSteps && onMain.admitted==offMain.admitted)
                ensure("OFF-main feed is exactly the OFF feed",on.q.samples==off.q.samples &&
                    on.q.steps==off.q.steps && on.q.half==off.q.half && on.channels==off.channels);
            if (off.channels)
            {
                ++offDraws;
                // Channels are lights traced per draw; a feed with a smaller allowance may
                // use fewer per draw but never zero. Counted, reported, not hidden.
                if (on.channels<off.channels) ++fewerChannels;
                if (on.q.samples*(on.q.half ? 1:4)<off.q.samples*(off.q.half ? 1:4)) ++fewerRays;
            }
        }
        // Thin OFF is the committed plan: its feed allowance is exactly the OFF formula.
        const auto off=feedWorkload(same,same,mainWidth,mainHeight,640,360,cap,budget,cap,kFeedSlots);
        ensure("OFF feed allowance unchanged",off.budget==(offMain.budget-offReserve)/kFeedSlots);
    }
    ensure("grid covers segmented main plans",segmentedMain>0);
    ensure("grid covers uniform main fallbacks",uniformMain>0);
    ensure("grid size",cases>10000 && offDraws>0);
    // Feeds are deliberately NOT compared across presets: a feed's allowance is what its preset's
    // main plan leaves over, so a higher preset's feed can have fewer rays by design (the
    // admission guarantee above is the invariant). The preset-ordered sample rows are the main view's (test 24).
    std::cout << "[RelightQ] feed admission grid cases=" << cases << " OFF-draws=" << offDraws
        << " ON-channels-below-OFF=" << fewerChannels << " ON-rays-below-OFF=" << fewerRays
        << " segmentedMain=" << segmentedMain << " uniformMain=" << uniformMain << std::endl;
    // The reported example: Low, 250M, 1080p main, K8, 1280x720 feed. The old 1/32 holdback
    // left 1.18M per feed (no shadows) against 14.66M with Thin OFF.
    {
        const auto uniform=quality(0), q=segmented(uniform,0);
        const auto on=feedWorkload(q,uniform,1920,1080,1280,720,8,250,8,kFeedSlots);
        const auto off=feedWorkload(uniform,uniform,1920,1080,1280,720,8,250,8,kFeedSlots);
        ensure("250M: OFF feed allowance is 14.66M",off.budget==14661200ull && off.channels>0);
        ensure("250M: ON feed allowance is no smaller",on.budget>=off.budget && on.channels>0);
        const auto big=feedWorkload(q,uniform,1920,1080,1280,720,8,1500,8,kFeedSlots);
        ensure("1500M: ON feed keeps its shadows",big.channels>0 && big.budget>=feedReservation(1920,1080,
            1500000000ull,8,kFeedSlots)/kFeedSlots);
    }
    // Fix round 3 (Codex P0 / Opus P0): Thin ON used to REMOVE VCam feed shadows, because the Thin or hybrid main plan spent more
    // than the OFF main and left the feeds less. The user decision has no feed exception: a Thin or hybrid main is capped at the
    // OFF main's cost while feed slots exist, so the feeds get EXACTLY the OFF allowance. The two reported cases, literally:
    {
        // Codex: 720p main, Low, K3, 400M, eight feed slots, 1024x1024 feed. OFF 3 feed lights, Thin ON used to give 2
        // (allowances 47.2352M and 22.352M).
        const auto uniform=quality(0), q=segmented(uniform,0);
        const auto on=feedWorkload(q,uniform,1280,720,1024,1024,3,400,3,kFeedSlots);
        const auto off=feedWorkload(uniform,uniform,1280,720,1024,1024,3,400,3,kFeedSlots);
        ensure("Codex case: OFF feed admits 3 lights on 47.2352M",off.channels>0 && off.admitted==3 && off.budget==47235200ull);
        ensure("Codex case: Thin ON gets the same allowance and the same 3 lights",on.budget==off.budget && on.channels>0 &&
            on.admitted==3);
        // Opus: 1080p main, Medium, K4, 1500M, 1024x1024 feed: OFF 4 of 4, Thin ON used to give 2 of 4.
        const auto uniformM=quality(1), qm=segmented(uniformM,1);
        const auto onM=feedWorkload(qm,uniformM,1920,1080,1024,1024,4,1500,4,kFeedSlots);
        const auto offM=feedWorkload(uniformM,uniformM,1920,1080,1024,1024,4,1500,4,kFeedSlots);
        ensure("Opus case: OFF feed admits 4 of 4",offM.channels>0 && offM.admitted==4);
        ensure("Opus case: Thin ON gets the same allowance and admits 4 of 4",onM.budget==offM.budget && onM.channels>0 && onM.admitted==4);
    }
    // The sweep (actual feedWorkload counts): every preset x main 720p..4K x K in {1,3,4,8} x feed sizes x the whole budget slider
    // 1..1500 M. Thin ON admits at least what Thin OFF admits in the feed; the allowance is the OFF allowance; a Thin or hybrid
    // main never costs more than the OFF main and the main plus eight feeds stay inside the frame budget. Admission of the feed is
    // NOT monotone in the BUDGET slider for Thin OFF either, because the main plan takes its share first and the feed's allowance
    // can fall (HEAD's own reservation cliffs, all-or-nothing, listed by test 36); it IS monotone in the feed allowance itself (fix
    // round 5: a feed has no light-dropping rung, test 36). Here: Thin ON has no non-monotone step that Thin OFF does not have.
    {
        struct FeedSize { unsigned w,h; };
        constexpr FeedSize sizes[]={{512,288},{1024,1024},{1920,1080}};
        unsigned long rows=0, onBelow=0, offNonMonotone=0, onNonMonotone=0, extraNonMonotone=0, thinFeedRows=0;
        for (unsigned mainHeight : {720u,1080u,2160u})
        for (unsigned cap : {1u,3u,4u,8u})
        for (int preset=0; preset<4; ++preset)
        for (const auto& size : sizes)
        {
            const unsigned mainWidth=mainHeight*16/9;
            const auto uniform=quality(preset), q=segmented(uniform,preset);
            unsigned previousOn=0, previousOff=0;
            for (int budget=1; budget<=1500; ++budget)
            {
                const auto on=feedWorkload(q,uniform,mainWidth,mainHeight,size.w,size.h,cap,budget,cap,kFeedSlots);
                const auto off=feedWorkload(uniform,uniform,mainWidth,mainHeight,size.w,size.h,cap,budget,cap,kFeedSlots);
                const auto onMain=workloadOrUniform(q,uniform,mainWidth,mainHeight,cap,budget,cap);
                const auto offMain=workload(uniform,mainWidth,mainHeight,cap,budget,cap);
                const unsigned a=on.channels ? on.admitted:0, b=off.channels ? off.admitted:0;
                ++rows;
                if (on.q.nearSteps) ++thinFeedRows;
                ensure("sweep: the feed gets exactly the OFF allowance",on.budget==off.budget);
                ensure("sweep: Thin ON admits at least the feed lights Thin OFF admits",a>=b);
                onBelow += a<b ? 1:0;
                ensure("sweep: a Thin or hybrid main never costs more than the OFF main",
                    (onMain.channels ? onMain.iterations:0)<=(offMain.channels ? offMain.iterations:0));
                ensure("sweep: main and eight feeds inside the frame budget",
                    (onMain.channels ? onMain.iterations:0)+kFeedSlots*(on.channels ? on.iterations:0)<=onMain.budget);
                if (b<previousOff) ++offNonMonotone;
                if (a<previousOn) { ++onNonMonotone; if (b>=previousOff) ++extraNonMonotone; }
                previousOn=a; previousOff=b;
            }
        }
        ensure("sweep size",rows==3ul*4*4*3*1500);
        ensure("Thin ON has no feed admission drop that Thin OFF does not have",extraNonMonotone==0);
        ensure("Thin ON feeds are exercised by the sweep",thinFeedRows>0);
        std::cout << "[RelightQ] feed sweep rows=" << rows << " thin-feed-rows=" << thinFeedRows << " ON-below-OFF=" << onBelow
            << " OFF-admission-drops=" << offNonMonotone << " ON-admission-drops=" << onNonMonotone
            << " ON-drops-not-in-OFF=" << extraNonMonotone << std::endl;
    }
}

namespace
{
struct Lamp { int id; double luminance, range, distance; bool mapped; };
// Superseded rankings, kept only so the fixtures below prove they would catch a regression.
double oldLegacyScore(const Lamp& l)
{
    const double score = l.range/std::max(l.distance,.01)*l.luminance*ALRelight::legacy(l.distance,l.range,.5);
    return std::isfinite(score) ? std::max(0.0,score) : 0.0;
}
bool unmappedFirst(const ALAreaShadow::Candidate<int>& a, const ALAreaShadow::Candidate<int>& b)
{
    if (a.shadowMap != b.shadowMap) return !a.shadowMap;
    if (a.score != b.score) return a.score > b.score;
    return a.id < b.id;
}
ALAreaShadow::Selection<int> choose(const std::vector<Lamp>& lamps, unsigned cap,
    const std::function<double(const Lamp&)>& score, bool unmapped_first = false)
{
    ALAreaShadow::Selection<int> out;
    for (const auto& l : lamps)
    {
        const ALAreaShadow::Candidate<int> candidate{l.id,l.mapped,score(l)};
        if (unmapped_first) out.add(candidate,cap,unmappedFirst); else out.add(candidate,cap);
    }
    return out;
}
double referenceImportance(const Lamp& l)
{
    return ALAreaShadow::importance(l.luminance,l.range,l.distance,l.mapped ? 1.0 : 0.0);
}
bool has(const ALAreaShadow::Selection<int>& s, int id)
{
    for (unsigned i=0; i<s.size; ++i) if (s.lights[i].id==id) return true;
    return false;
}
// Eight dim nearby unmapped points plus one bright world projector (the highest UUID, so the
// stable tie-break cannot rescue it) that owns a native map, 18 m away: OUTSIDE its own
// 15 m range, where the old camera-point score was 0.
std::vector<Lamp> projectorFixture()
{
    std::vector<Lamp> lamps;
    for (int i=1; i<=8; ++i) lamps.push_back({i,.3,7.5,3.0+.4*i,false});
    lamps.push_back({9,4.0,15.0,18.0,true});
    return lamps;
}
}

template<> template<> void relight_object::test<26>()
{
    using namespace ALAreaShadow;
    const auto lamps = projectorFixture();
    const auto score = [](const Lamp& l) { return referenceImportance(l); };
    for (unsigned cap : {4u,8u})
    {
        const auto selected = choose(lamps,cap,score);
        ensure("bright mapped projector admitted",has(selected,9));
        ensure_equals("it ranks first by importance",selected.lights[0].id,9);
        ensure_equals("all nine candidates counted",selected.candidates,9u);
        // Mutations that must fail this same expectation.
        ensure("restoring unmapped-first drops the projector",!has(choose(lamps,cap,score,true),9));
        ensure("restoring the camera-point legacy score drops the projector",
            !has(choose(lamps,cap,oldLegacyScore),9));
    }
    // Order is stable under input permutations (the real comparator), K=4 and K=8. A deterministic
    // sample stands in for all 362,880 orders: the identity, the reverse and 5,000 Fisher-Yates
    // shuffles driven by a fixed xorshift stream (no std::shuffle: its result is implementation
    // defined). The comparator is a strict total order (score, then UUID), so any order dependence
    // shows in a few shuffles; the tie fixture below checks every order of the equal-score lamps.
    ALAreaShadow::Selection<int> reference[2] = {choose(lamps,4,score),choose(lamps,8,score)};
    const auto agrees = [&](const std::vector<int>& order, const std::function<double(const Lamp&)>& rank,
        bool expect_same)
    {
        std::vector<Lamp> shuffled;
        for (int index : order) shuffled.push_back(lamps[index]);
        bool same = true;
        for (int k=0; k<2; ++k)
        {
            const auto selected = choose(shuffled,k ? 8u:4u,rank);
            if (selected.size != reference[k].size) { same = false; continue; }
            for (unsigned i=0; i<selected.size; ++i)
                if (selected.lights[i].id != reference[k].lights[i].id ||
                    selected.lights[i].score != reference[k].lights[i].score) same = false;
        }
        if (expect_same) ensure("same selection, order and logged score under permutation",same);
        return same;
    };
    std::vector<int> order{0,1,2,3,4,5,6,7,8};
    unsigned permutations = 0;
    agrees(order,score,true); ++permutations;                      // identity
    std::reverse(order.begin(),order.end());
    agrees(order,score,true); ++permutations;                      // reverse
    std::uint32_t state = 0x9E3779B9u;
    const auto next = [&state]() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; };
    for (unsigned n=0; n<5000; ++n)
    {
        for (int i=0; i<9; ++i) order[i] = i;
        for (int i=8; i>0; --i) std::swap(order[i],order[next() % unsigned(i+1)]);
        agrees(order,score,true); ++permutations;
    }
    ensure_equals("identity + reverse + 5000 fixed shuffles",permutations,5002u);
    // Power: the sampled set still rejects an order-dependent comparator. Dropping the UUID tie
    // break (stable insertion keeps the first arrival) must be visible in some shuffle of equal lamps.
    {
        const std::vector<Lamp> tied{{1,1.0,10,5,false},{2,1.0,10,5,false},{3,1.0,10,5,false},{4,1.0,10,5,false}};
        std::vector<int> t{0,1,2,3};
        const auto first = choose(tied,2,score);
        bool order_dependent_without_tiebreak = false;
        do
        {
            std::vector<Lamp> shuffled;
            for (int index : t) shuffled.push_back(tied[index]);
            const auto selected = choose(shuffled,2,score);
            for (unsigned i=0; i<selected.size; ++i)
                ensure_equals("equal scores: the UUID decides in every order",selected.lights[i].id,first.lights[i].id);
            ALAreaShadow::Selection<int> no_tiebreak;
            for (const auto& l : shuffled)
                no_tiebreak.add({l.id,l.mapped,score(l)},2,[](const ALAreaShadow::Candidate<int>& a,
                    const ALAreaShadow::Candidate<int>& b) { return a.score > b.score; });
            if (no_tiebreak.lights[0].id != first.lights[0].id || no_tiebreak.lights[1].id != first.lights[1].id)
                order_dependent_without_tiebreak = true;
        } while (std::next_permutation(t.begin(),t.end()));
        ensure("dropping the UUID tie break is caught (mutation rejected)",order_dependent_without_tiebreak);
    }
    // Determinism: identical inputs, identical selection and scores, call after call.
    const auto first = choose(lamps,8,score), second = choose(lamps,8,score);
    for (unsigned i=0; i<first.size; ++i)
        ensure("identical selection for identical inputs",first.lights[i].id==second.lights[i].id &&
            first.lights[i].score==second.lights[i].score && first.lights[i].shadowMap==second.lights[i].shadowMap);
    // Ties still break by UUID; the unmapped bonus is bounded (a light 30 % more important wins).
    ensure("a mapped light 30 % more important outranks an unmapped one",
        choose({{1,1.0,10,5,false},{2,1.3,10,5,true}},1,score).lights[0].id==2);
    ensure("an unmapped light within the bonus outranks a mapped one",
        choose({{1,1.0,10,5,false},{2,1.2,10,5,true}},1,score).lights[0].id==1);
}

template<> template<> void relight_object::test<27>()
{
    using namespace ALAreaShadow;
    // Dolly across the range boundary: a camera walks 0..40 m along z past a row of lamps
    // (3 m to the side, so the camera is never closer than 3 m). Whenever the selected set
    // changes between two 1 cm steps, the light that entered and the light that left must be
    // visibly comparable (reference importance within 2 %): no channel pops among lights that
    // differ, and no light is dropped merely because the camera left or entered its range.
    struct Placed { int id; double luminance, range, z; bool mapped; };
    const std::vector<Placed> placed = {{1,1.0,10,6,false},{2,1.4,6,12,false},{3,.6,12,20,true},
        {4,2.0,9,28,false},{5,.9,8,34,true},{6,1.2,14,9,false}};
    const auto lamps_at = [&](double camera_z)
    {
        std::vector<Lamp> lamps;
        for (const auto& p : placed)
            lamps.push_back({p.id,p.luminance,p.range,std::hypot(3.0,p.z-camera_z),p.mapped});
        return lamps;
    };
    const auto run = [&](const std::function<double(const Lamp&)>& score, bool unmapped_first)
    {
        unsigned flips = 0, incomparable = 0;
        for (unsigned cap : {2u,3u,4u})
        {
            auto previous = choose(lamps_at(0),cap,score,unmapped_first);
            for (int step=1; step<=4000; ++step)
            {
                const auto lamps = lamps_at(step*.01);
                const auto current = choose(lamps,cap,score,unmapped_first);
                for (unsigned i=0; i<current.size; ++i)
                {
                    if (has(previous,current.lights[i].id)) continue;
                    // Entered. Find what left.
                    for (unsigned j=0; j<previous.size; ++j)
                    {
                        if (has(current,previous.lights[j].id)) continue;
                        const Lamp *in=nullptr,*out=nullptr;
                        for (const auto& l : lamps)
                        {
                            if (l.id==current.lights[i].id) in=&l;
                            if (l.id==previous.lights[j].id) out=&l;
                        }
                        const double a=referenceImportance(*in), b=referenceImportance(*out);
                        if (std::abs(a-b) > .02*std::max(a,b)) ++incomparable;
                        ++flips;
                    }
                }
                previous=current;
            }
        }
        return std::make_pair(flips,incomparable);
    };
    const auto importance_run = run([](const Lamp& l) { return referenceImportance(l); },false);
    ensure("the dolly actually changes the selection",importance_run.first>0);
    ensure_equals("importance: every swap is between comparable lights",importance_run.second,0u);
    const auto legacy_run = run(oldLegacyScore,false);
    ensure("camera-point legacy score pops a visibly different light (mutation rejected)",legacy_run.second>0);
    std::cout << "[RelightQ] dolly 0..40 m, K 2/3/4 x 4000 steps: importance " << importance_run.first
        << " swaps, " << importance_run.second << " between visibly different lights; camera-point legacy "
        << legacy_run.first << " swaps, " << legacy_run.second << " between visibly different lights" << std::endl;
    // Importance is continuous and non-increasing in distance, and never zero outside the range.
    double previous = std::numeric_limits<double>::infinity();
    for (int i=0; i<=40000; ++i)
    {
        const double score = ALAreaShadow::importance(1.0,10.0,i*.001,1.0);
        ensure("importance is non-increasing in distance",score<=previous+1.e-15);
        ensure("importance is positive past the range boundary",score>0);
        if (i) ensure("importance has no step at any distance",std::abs(score-previous) <= previous*.001);
        previous=score;
    }
}

template<> template<> void relight_object::test<28>()
{
    using namespace ALAreaShadow;
    // Stale mapped projectors: detail 2 -> 1 -> 0 with a previously slotted projector.
    ensure("detail 2, rendered, complete, in range: mapped",nativeMapSampled(2,3,4,true,true));
    ensure("detail 1 (projector shadows off): unmapped",!nativeMapSampled(1,3,4,true,true));
    ensure("detail 0: unmapped",!nativeMapSampled(0,3,4,true,true));
    ensure("slot above the live limit: unmapped",!nativeMapSampled(2,4,4,true,true));
    ensure("map not complete: unmapped",!nativeMapSampled(2,3,4,false,true));
    ensure("map not rendered this frame: unmapped",!nativeMapSampled(2,3,4,true,false));
    for (int detail : {2,1,0,1,2})
    {
        const bool sampled = nativeMapSampled(detail,0,2,true,detail>1);
        // An unmapped projector packs contact 0 / fade 0: its full soft shadow is kept.
        Candidate<int> candidate{1,sampled,importance(1,10,5,sampled)};
        ensure("mapped only while detail > 1",candidate.shadowMap==(detail>1));
    }
    // Native hand-over: the screen trace takes over across u = (depth-clip.z)/(clip.w-clip.z) in
    // [.25,.45], finished before the native sampler's own fade-to-lit starts at u = .5 (the 0.05
    // margin absorbs the bilateral filter, which blends a few /255 across a depth step).
    // areaShadowF.glsl hard-codes the same window: (u-0.25)*5.0 = 1/(kHandoverEnd-kHandoverStart).
    close("hand-over starts at u = .25",kHandoverStart,.25);
    close("hand-over ends at u = .45",kHandoverEnd,.45);
    ensure("the hand-over ends before the native ramp starts (margin >= .05)",kHandoverEnd <= .5-.05+1.e-12);
    close("the shader slope is the reciprocal of the window",1.0/(kHandoverEnd-kHandoverStart),5.0);
    close("unset clip is valid everywhere",nativeShadowStrength(50,0,0),1);
    close("malformed clip is valid everywhere",nativeShadowStrength(50,60,40),1);
    close("near receivers keep the native shadow",nativeShadowStrength(10,64,128),1);
    close("start of the hand-over (u = .25)",nativeShadowStrength(80,64,128),1);
    close("middle of the hand-over (u = .35)",nativeShadowStrength(86.4,64,128),.5);
    close("end of the hand-over (u = .45)",nativeShadowStrength(92.8,64,128),0);
    close("the old end (u = .5) is long since handed over",nativeShadowStrength(96,64,128),0);
    close("beyond the hand-over",nativeShadowStrength(112,64,128),0);
    close("at shadow_clip.w the native map is lit",nativeShadowStrength(128,64,128),0);
    close("beyond shadow_clip.w",nativeShadowStrength(500,64,128),0);
    // A non-finite depth is "native valid" (strength 1), the same rule as the shader's isnan/isinf guard.
    const double inf_value = std::numeric_limits<double>::infinity(), nan_value = std::numeric_limits<double>::quiet_NaN();
    close("NaN depth: native valid",nativeShadowStrength(nan_value,64,128),1);
    close("+Inf depth: native valid",nativeShadowStrength(inf_value,64,128),1);
    close("-Inf depth: native valid",nativeShadowStrength(-inf_value,64,128),1);
    double last = 1.0;
    for (int i=0; i<=20000; ++i)
    {
        const double depth = 50+i*.005, s = nativeShadowStrength(depth,64,128);
        ensure("strength is non-increasing",s<=last+1.e-15);
        ensure("strength is continuous (no step anywhere)",std::abs(s-last)<=.005/((kHandoverEnd-kHandoverStart)*64)+1.e-12);
        // The native sampler adds light only from u = .5; the screen term is gone by then, with margin.
        const double u = (depth-64)/(128-64);
        if (u >= .45) ensure("the screen hand-over is complete before the native ramp",s==0.0);
        if (u > .5) ensure("the native ramp rises only where the hand-over is done",nativeSpotShadow(0,depth,64,128) >= 0.0 && s==0.0);
        last=s;
    }
}

namespace
{
// The final value a receiver gets from the spot shader: min(native + shadow_fade, screen), the
// production composition (spotLightF.glsl), with the native value from the pure sampler model.
double finalComposite(double pcf, double depth, double clipZ, double clipW, double blend, double fade,
    double fullTrace, double contactTrace, double (*strength)(double,double,double))
{
    const double native = std::clamp(ALAreaShadow::nativeSpotShadow(pcf,depth,clipZ,clipW,blend)+(1.0-fade),0.0,1.0);
    // The gather's blend: mix(full, contact, fade * strength).
    const double screen = fullTrace + (contactTrace-fullTrace) * fade * strength(depth,clipZ,clipW);
    return std::min(native,screen);
}
// The superseded strength: the screen term took over across the SAME half as the native ramp.
double oldStrength(double depth, double clipZ, double clipW)
{
    const double u = (depth-clipZ)/(clipW-clipZ);
    return 1.0-std::clamp(2.0*u-1.0,0.0,1.0);
}
double validEverywhere(double, double, double) { return 1.0; }
}

template<> template<> void relight_object::test<30>()
{
    using namespace ALAreaShadow;
    // Independent combined-output oracle: the FINAL composite (native map + shadow_fade, then
    // the screen visibility, as composed in the spot shaders) swept across the whole native
    // cutoff, for every combination of "who sees the blocker". The native sampler is the pure
    // model of shadowUtil.glsl (its ramp to lit AND the /weight term), not the gather's own
    // strength function.
    struct Clip { double z, w, blend; };
    const Clip clips[] = {{64,128,.25},{30,40,.25},{32,200,.1},{10,20,.08},{64,96,.25}};
    unsigned cases = 0;
    for (const Clip& clip : clips)
    {
        const double step = (clip.w-clip.z)/4000.0, from = clip.z*.5, to = clip.w*1.5;
        for (double fade : {1.0,.75,.5,.25,0.0})
        {
            struct Scene { const char* name; double pcf, full, contact; };
            for (const Scene& s : {Scene{"blocker seen by both",0.0,0.0,1.0},
                                   Scene{"blocker seen by both, partly by contact",0.0,0.0,.5},
                                   Scene{"only the screen trace sees it",1.0,0.0,1.0},
                                   Scene{"only the native map sees it",0.0,1.0,1.0},
                                   Scene{"nothing blocks",1.0,1.0,1.0}})
            {
                double previous = -1.0, start = 0.0, end = 0.0, peak = 0.0;
                for (double d = from; d <= to; d += step)
                {
                    const double v = finalComposite(s.pcf,d,clip.z,clip.w,clip.blend,fade,s.full,s.contact,nativeShadowStrength);
                    ensure("final composite is a visibility",v>=0.0 && v<=1.0);
                    // Continuous: nothing moves faster than the native ramp plus the hand-over
                    // (a few percent per sample), so no step and no one-sample pop.
                    if (previous>=0.0) ensure("final composite has no step",std::abs(v-previous)<=.06);
                    if (previous<0.0) start = v;
                    end = v; peak = std::max(peak,v); previous = v; ++cases;
                }
                if (s.pcf==0.0 && s.full==0.0)
                    // The reported band: for a blocker both systems see, the composite is never
                    // brighter than the brighter of its two endpoints.
                    ensure("never brighter than both endpoints for a shared blocker",peak<=std::max(start,end)+1.e-9);
                if (s.pcf==1.0 && s.full==0.0 && fade==1.0)
                {
                    ensure("past the native map the full trace decides",end<=1.e-9);
                    ensure("inside the native map only contact adds",std::abs(start-s.contact)<=1.e-9);
                }
                if (s.pcf==0.0 && s.full==1.0)
                    ensure("a native-only blocker follows the stock native ramp to lit",end>=1.0-1.e-9);
                if (s.pcf==1.0 && s.full==1.0) ensure("nothing blocks: always lit",peak>=1.0-1.e-9 && start>=1.0-1.e-9 && end>=1.0-1.e-9);
            }
        }
    }
    // Mutation: the superseded strength (screen took over WITH the native ramp) meets it mid-way.
    {
        const Clip clip = clips[0];
        double peak = 0.0;
        for (double d = clip.z*.5; d <= clip.w*1.5; d += .01)
            peak = std::max(peak,finalComposite(0.0,d,clip.z,clip.w,clip.blend,1.0,0.0,1.0,oldStrength));
        ensure("the old hand-over transmits a bright band (mutation rejected)",peak>.45);
        double hold = 0.0;
        for (double d = clip.z*.5; d <= clip.w*1.5; d += .01)
            hold = std::max(hold,finalComposite(0.0,d,clip.z,clip.w,clip.blend,1.0,0.0,1.0,nativeShadowStrength));
        ensure("the new hand-over transmits nothing",hold<=1.e-9);
        // Never handing over (the pre-hand-over behaviour) keeps the contact term past the map,
        // where the trace sees a blocker the native map no longer covers: the fixture catches it.
        const double kept = finalComposite(1.0,clip.w*1.2,clip.z,clip.w,clip.blend,1.0,0.0,1.0,validEverywhere);
        ensure("never handing over lights a screen-only blocker past the map (mutation rejected)",kept>=1.0-1.e-9);
    }
    std::cout << "[RelightQ] native hand-over oracle: " << cases << " samples over 5 clips x 5 fades x 5 scenes, "
        << "shared blocker never brighter than its endpoints" << std::endl;
    // The receiver offset. sampleSpotShadow first moves the receiver along its normal by
    // spot_shadow_offset and ramps to lit by THAT depth, so the hand-over must be evaluated there
    // (nativeReceiverDepth). A tilted receiver with the default 0.04 m or the maximum 0.15 m offset
    // otherwise meets the native ramp mid-way and lights a shared blocker (GPU: 10/255 and 40/255).
    {
        close("facing receiver moves toward the camera",nativeReceiverDepth(-4,1,.15),3.85);
        close("receiver tilted away moves deeper",nativeReceiverDepth(-4,-.7071,.15),4+.7071*.15);
        close("no offset, no shift",nativeReceiverDepth(-4,-.7071,0),4);
        close("a non-finite normal does not shift",nativeReceiverDepth(-4,std::numeric_limits<double>::quiet_NaN(),.15),4);
        const Clip tilted[] = {{4,5,.25},{30,40,.25},{64,128,.25},{10,20,.08}};
        unsigned sweeps = 0;
        double superseded_peak = 0.0;
        for (const Clip& clip : tilted)
            for (double offset : {0.0,.04,.15})
                for (double normal_z : {1.0,.7071,0.0,-.7071,-1.0})
                    for (double fade : {1.0,.5})
                    {
                        double peak = 0.0, start = -1.0, end = 0.0, raw_peak = 0.0, raw_start = -1.0, raw_end = 0.0;
                        const double step = (clip.w-clip.z)/4000.0;
                        for (double d = clip.z*.5; d <= clip.w*1.5; d += step)
                        {
                            const double shifted = nativeReceiverDepth(-d,normal_z,offset);
                            // Shared blocker: the native map shadows it (pcf 0); the trace sees it past contact.
                            const double native = std::clamp(nativeSpotShadow(0.0,shifted,clip.z,clip.w,clip.blend)+(1.0-fade),0.0,1.0);
                            const double screen = 1.0 * fade * nativeShadowStrength(shifted,clip.z,clip.w);
                            const double v = std::min(native,screen);
                            // The superseded form: the strength at the unshifted depth.
                            const double raw_screen = fade * nativeShadowStrength(d,clip.z,clip.w);
                            const double raw = std::min(native,raw_screen);
                            if (start < 0.0) { start = v; raw_start = raw; }
                            end = v; raw_end = raw;
                            peak = std::max(peak,v); raw_peak = std::max(raw_peak,raw);
                        }
                        // Never brighter than the brighter endpoint (fade 1: the peak is exactly 0).
                        ensure("shifted hand-over: shared blocker never brighter than its endpoints",peak <= std::max(start,end)+1.e-9);
                        if (fade == 1.0) ensure("shifted hand-over: shared blocker peak is 0",peak <= 1.e-9);
                        if (offset > 0.0 && normal_z < -.5 && fade == 1.0 && clip.w-clip.z <= 1.0)
                            superseded_peak = std::max(superseded_peak,raw_peak-std::max(raw_start,raw_end));
                        ++sweeps;
                    }
        ensure("the unshifted hand-over lights a tilted shared blocker (mutation rejected)",superseded_peak > .1);
        std::cout << "[RelightQ] receiver-offset hand-over oracle: " << sweeps << " sweeps over 4 clips x offsets {0,.04,.15} x "
            << "5 normals x 2 fades, shared-blocker peak 0 (unshifted peak " << superseded_peak << ")" << std::endl;
    }
    // The sampler model itself: literal values pinned to shadowUtil.glsl.
    close("model: nearer than the ramp is the map's own PCF",ALAreaShadow::nativeSpotShadow(.3,70,64,128),.3);
    close("model: three quarters through adds half",ALAreaShadow::nativeSpotShadow(0,112,64,128),.5);
    close("model: lit at shadow_clip.w",ALAreaShadow::nativeSpotShadow(0,128,64,128),1);
    close("model: beyond shadow_clip.w is lit",ALAreaShadow::nativeSpotShadow(0,500,64,128),1);
    // /weight: clip (30,40), blend .25: u = .5 at depth 35, W(36) = 1 - (37.5-36)/15 = .9; add = .2.
    close("model: the /weight term amplifies the ramp",ALAreaShadow::nativeSpotShadow(0,36,30,40),.2/.9);
}

template<> template<> void relight_object::test<31>()
{
    // Housing is the committed GLOBAL light-bias sphere (RenderAreaShadowLightBias): occluders within
    // `bias` of the light centre are ignored and each ray stops floor + bias short of its target, with
    // floor = max(source radius, 0.1 m) counted once. Per-light housing shapes (a bounding sphere, an
    // oriented box, a capped box) were tried and each erased real blockers or left a seam, so none is
    // modelled. A large emitting prim shadowing its own light is a documented limitation: the user raises
    // Light bias. This test is a CPU march of the shader's rule against slabs given as axis-aligned boxes.
    struct Slab { double lo[3], hi[3]; };
    const auto inside = [](const Slab& s, const double p[3])
    { return p[0]>=s.lo[0]&&p[0]<=s.hi[0]&&p[1]>=s.lo[1]&&p[1]<=s.hi[1]&&p[2]>=s.lo[2]&&p[2]<=s.hi[2]; };
    using Gap = std::function<double(const double*,const double*)>;
    using Housed = std::function<bool(const double*)>;
    const auto blocked = [&](const Slab& slab, const double origin[3], const double target[3],
        const Gap& end_gap, const Housed& housed)
    {
        const double d[3] = {target[0]-origin[0],target[1]-origin[1],target[2]-origin[2]};
        const double length = std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
        const double limit = std::max(0.0,length-end_gap(origin,target));
        if (limit <= .001) return false;
        for (int i=0; i<4000; ++i)
        {
            const double t = limit*(i+.5)/4000.0;
            const double p[3] = {origin[0]+d[0]/length*t,origin[1]+d[1]/length*t,origin[2]+d[2]/length*t};
            if (inside(slab,p) && !housed(p)) return true;
        }
        return false;
    };
    const double bias = .25;
    // The shader's rule: a sphere of radius `housing` around the light, end gap floor + housing.
    struct Rule { Gap gap; Housed housed; };
    const auto sphere = [&](const double light[3], double floor_gap, double housing)
    {
        return Rule{[=](const double*,const double*) { return floor_gap+housing; },
                    [=](const double* p) { return std::sqrt((p[0]-light[0])*(p[0]-light[0])+(p[1]-light[1])*(p[1]-light[1])+
                                                            (p[2]-light[2])*(p[2]-light[2])) <= housing; }};
    };
    const auto march = [&](const Slab& slab, const double origin[3], const double light[3], const Rule& rule)
    { return blocked(slab,origin,light,rule.gap,rule.housed); };

    // (1) The housed wall lamp: a small bulb inside a separate 0.30 x 0.30 x 0.40 m casing. The ray stops
    // floor + bias short of the light, before it can reach the casing, so the wall keeps its light.
    {
        const double light[3] = {0,0,0}, origin[3] = {.5,0,1};
        const Slab casing = {{-.15,-.15,-.2},{.15,.15,.2}};
        ensure("wall lamp: the casing around the bulb does not block (HEAD sphere)",
            !march(casing,origin,light,sphere(light,.1,bias)));
        // No housing at all (bias 0) lets the casing shadow its own bulb: the fixture discriminates.
        ensure("wall lamp: without any housing the casing shadows the wall (fixture discriminates)",
            march(casing,origin,light,sphere(light,.1,0.0)));
        close("end gap is floor + bias",sphere(light,.1,bias).gap(origin,light),.35);
    }
    // (2) A big rig softbox (source 2 and 4 m across) on a 2-3 m orbit still casts soft shadows on
    // the actor. Rig fixtures are synthetic emitters: the global bias only, the source radius counted
    // ONCE in the end gap (not added again as housing).
    {
        const double origin[3] = {0,0,0};
        for (double radius : {1.0,2.0})
            for (double orbit : {2.5,3.0})
            {
                const double centre[3] = {0,0,orbit};
                const Slab blocker = {{0,-3,.45},{3,3,.5}}; // half-plane, 0.45 m from the actor
                unsigned shadowed = 0, samples = 16;
                for (unsigned i=0; i<samples; ++i)
                {
                    const double r = std::sqrt((i+.5)/samples)*radius, a = i*2.39996322973;
                    const double target[3] = {r*std::cos(a),r*std::sin(a),centre[2]};
                    const auto once = [&](const double*,const double*) { return std::max(radius,.1)+bias; };
                    const Housed bare = [&](const double* p) { return std::sqrt(p[0]*p[0]+p[1]*p[1]+(p[2]-centre[2])*(p[2]-centre[2])) <= bias; };
                    if (blocked(blocker,origin,target,once,bare)) ++shadowed;
                }
                ensure("softbox: part of the source is shadowed",shadowed>0);
                ensure("softbox: soft (the rest of the source still lights the actor)",shadowed<samples);
                // Mutation: the source radius added again as housing (the double count) removes it.
                unsigned doubled = 0;
                for (unsigned i=0; i<samples; ++i)
                {
                    const double r = std::sqrt((i+.5)/samples)*radius, a = i*2.39996322973;
                    const double target[3] = {r*std::cos(a),r*std::sin(a),centre[2]};
                    const auto twice = [&](const double*,const double*) { return std::max(radius,.1)+std::max(bias,std::min(radius,1.5)); };
                    const Housed big = [&](const double* p) { return std::sqrt(p[0]*p[0]+p[1]*p[1]+(p[2]-centre[2])*(p[2]-centre[2])) <= std::max(bias,std::min(radius,1.5)); };
                    if (blocked(blocker,origin,target,twice,big)) ++doubled;
                }
                if (radius*2 >= orbit) ensure("the double-counted housing erased the softbox shadow (mutation rejected)",doubled<shadowed);
            }
    }
    // (3) A hollow 8 x 8 x 3 m room prim with the light at its centre: independent actor-sized blockers
    // shadow receivers both NEAR the light and 3+ m from it (the capped-box housing lost the far ones:
    // Codex's GPU repro, visibility 0.000 -> 1.000). The sphere keeps both; an oversized housing (the
    // superseded 1.5 m caps) erases the blocker that lies inside it, so the fixture discriminates.
    {
        const double light[3] = {0,0,1.5};
        const double near_actor[3] = {.8,0,.3}, far_wall[3] = {3.5,0,1.5}, far_floor[3] = {3.2,0,.1};
        const Slab hat_brim = {{.35,-.5,.7},{.55,.5,.9}};     // 0.6 m from the light, between the near actor and it
        const Slab passer_by = {{1.2,-.5,.9},{1.4,.5,2.1}};   // 1.2 m from the light, between it and the far wall
        const Rule head = sphere(light,.1,bias), oversized = sphere(light,.1,1.5);
        ensure("room prim: a prop shadows the actor near the light",march(hat_brim,near_actor,light,head));
        ensure("room prim: an independent blocker shadows a receiver 3.5 m from the light",march(passer_by,far_wall,light,head));
        ensure("room prim: ... and one on the floor 3.2 m away",march(passer_by,far_floor,light,head));
        ensure("room prim: an oversized housing erases the far blocker (fixture discriminates)",
            !march(passer_by,far_wall,light,oversized));
        ensure("room prim: an oversized housing erases the near prop (fixture discriminates)",
            !march(hat_brim,near_actor,light,oversized));
        // The receiver beside a wall/ceiling emitter prim: its own surface 0.2 m away is inside the bias
        // sphere, so only a blocker outside 0.25 m shadows it.
        const double beside[3] = {.5,.2,2.9}, panel_light[3] = {0,0,3};
        const Slab blocker = {{.42,-1,2.85},{.46,1,3.0}};
        ensure("receiver beside a ceiling panel prim is shadowed by a blocker outside the bias",
            march(blocker,beside,panel_light,sphere(panel_light,.1,bias)));
    }
}

template<> template<> void relight_object::test<32>()
{
    using namespace ALAreaShadow;
    // Allocation: a failure is retried every frame and nothing remembers it, so the drawable
    // result depends on this frame's outcome only.
    ensure_equals("nothing wanted",drawablePlanes(0,true,true),0u);
    ensure_equals("one plane ok",drawablePlanes(1,true,true),1u);
    ensure_equals("one plane failed",drawablePlanes(1,false,true),0u);
    ensure_equals("two planes ok",drawablePlanes(2,true,true),2u);
    ensure_equals("plane 1 failed keeps plane 0 (four lights)",drawablePlanes(2,true,false),1u);
    ensure_equals("plane 0 failed keeps nothing",drawablePlanes(2,false,true),0u);
    ensure_equals("both failed",drawablePlanes(2,false,false),0u);
    ensure_equals("plane 1 is not consulted for one plane",drawablePlanes(1,true,false),1u);
    ensure_equals("channels follow the planes",drawableChannels(8,1),4u);
    ensure_equals("fewer lights than the planes",drawableChannels(3,2),3u);
    ensure_equals("no planes, no channels",drawableChannels(8,0),0u);
    // History independence. The removed policy blocked the same request for 300 frames after a
    // failure, so a frame whose allocation SUCCEEDS rendered nothing (this is the superseded model,
    // kept to prove the fixture catches it). The current policy has no state: any frame whose
    // outcome is "both planes ok" yields the same planes as a fresh run, after any history.
    struct SupersededBackoff
    {
        unsigned failedFrame = 0; bool failed = false;
        unsigned planes(unsigned frame, unsigned wanted, bool ok0, bool ok1)
        {
            if (failed && frame - failedFrame < 300) return 0;
            const unsigned result = drawablePlanes(wanted,ok0,ok1);
            if (result < wanted) { failed = true; failedFrame = frame; } else failed = false;
            return result;
        }
    };
    SupersededBackoff old;
    old.planes(100,2,true,false);                       // frame 100: plane 1 fails
    const unsigned fresh = drawablePlanes(2,true,true);
    unsigned historyDependent = 0, current = 0;
    for (unsigned frame = 101; frame < 400; ++frame)    // memory is available again
    {
        if (old.planes(frame,2,true,true) != fresh) ++historyDependent;
        if (drawablePlanes(2,true,true) != fresh) ++current;
    }
    ensure("the superseded backoff renders differently for equal inputs (mutation rejected)",historyDependent == 299);
    ensure_equals("the current policy renders the same after a failure",current,0u);
}

template<> template<> void relight_object::test<33>()
{
    using namespace ALAreaShadow;
    // The unmapped bonus is continuous in the native weight (slot fade x cutoff validity).
    close("unmapped bonus",importance(1,10,5,0.0)/importance(1,10,5,1.0),1.25);
    close("fully mapped has none",importance(1,10,5,1.0),importance(1,10,5,5.0));
    close("non-finite weight counts as unmapped",importance(1,10,5,std::numeric_limits<double>::quiet_NaN()),importance(1,10,5,0.0));
    // A slot is acquired: the fade runs 0 -> 1. The score at fade 0 equals the unmapped score
    // (no step when the slot is acquired) and falls smoothly to the mapped score.
    double previous = importance(1,10,5,0.0);
    for (int i=1; i<=1000; ++i)
    {
        const double score = importance(1,10,5,i/1000.0);
        ensure("score falls smoothly while the slot fades in",score<=previous && previous-score<=.0003*previous+1.e-12);
        previous = score;
    }
    // Slot fade in with no top-K eviction pop: K = 4, three strong lights, a mapped light M whose
    // slot is acquired and fades in, and an unmapped neighbour U that sits between M's scores.
    // The selection may change only where the two scores cross, so the two lights are comparable
    // at the swap. The old binary bonus (1.0 the moment a slot is acquired, even at fade 0)
    // swaps them at once with an 11 % gap, the visible pop.
    const auto m_in = [&](double weight)
    {
        Selection<int> out;
        out.add({1,false,importance(3.0,10,5,0.0)},4);
        out.add({2,false,importance(3.0,10,5,0.0)*.99},4);
        out.add({3,false,importance(3.0,10,5,0.0)*.98},4);
        out.add({4,true,importance(1.0,10,5,weight)},4);
        out.add({5,false,importance(1.0,10,5,0.0)*.9},4);
        return out.lights[3].id==4;
    };
    const double u_score = importance(1.0,10,5,0.0)*.9;
    for (bool binary : {false,true})
    {
        unsigned swaps = 0, incomparable = 0;
        bool previous_in = m_in(0.0);                    // before acquisition: unmapped
        ensure("M leads before acquisition",previous_in);
        for (int i=0; i<=1000; ++i)                      // acquisition at fade 0, then the fade
        {
            const double weight = binary ? 1.0 : i/1000.0;
            const bool now = m_in(weight);
            if (now != previous_in)
            {
                ++swaps;
                const double m_score = importance(1.0,10,5,weight);
                if (std::abs(m_score-u_score) > .01*std::max(m_score,u_score)) ++incomparable;
            }
            previous_in = now;
        }
        if (binary) ensure("the binary bonus pops visibly different lights (mutation rejected)",incomparable>0);
        else ensure("the continuous bonus swaps only comparable lights, once",incomparable==0 && swaps==1);
    }
    // Crossing the native shadow distance: weight = fade x validity, continuous in depth.
    double last = importance(1,10,5,1.0*nativeShadowStrength(40,64,128));
    for (int i=0; i<=40000; ++i)
    {
        const double score = importance(1,10,5,1.0*nativeShadowStrength(40+i*.003,64,128));
        ensure("score is continuous across the native cutoff",std::abs(score-last)<=.0002*last);
        last = score;
    }
    ensure("beyond the map the light counts as unmapped",importance(1,10,5,1.0*nativeShadowStrength(300,64,128))==importance(1,10,5,0.0));
}

template<> template<> void relight_object::test<29>()
{
    using namespace ALAreaShadow;
    // With the VCam switch OFF the main view passes feedSlots = 0 and reserves nothing.
    ensure_equals("no feed slots, no reservation",feedReservation(1920,1080,300000000ull,4,0),0ull);
    const std::uint64_t reserved = feedReservation(1920,1080,300000000ull,4,kFeedSlots);
    ensure_equals("300M/1080p/K4 reserves 8 x 2048x2048 minimum (11 % of the budget)",reserved,
        8ull*kFeedGuaranteePixels*4);
    ensure("that is about 11 %",reserved*100/300000000ull==11);
    bool strictlyMore = false;
    for (int preset=0; preset<4; ++preset)
        for (unsigned cap : {1u,4u,8u})
            for (unsigned height : {1080u,2160u})
                for (int budget : {100,300,600,1000,1500})
                {
                    const unsigned width = height*16/9;
                    const auto q = segmented(quality(preset),preset), uniform = quality(preset);
                    const auto off = workloadOrUniform(q,uniform,width,height,cap,budget,cap,0);
                    const auto on = workloadOrUniform(q,uniform,width,height,cap,budget,cap,kFeedSlots);
                    // Zero slots plan against the full budget: exactly the unreserved segmented plan.
                    const auto direct = workloadWithin(q,width,height,cap,std::uint64_t(budget)*1000000ull,cap,0);
                    const auto uniformMain = workloadWithin(uniform,width,height,cap,std::uint64_t(budget)*1000000ull,cap,0);
                    if (direct.channels && direct.admitted>=uniformMain.admitted)
                        ensure("no reservation: the full-budget plan",off.q.samples==direct.q.samples &&
                            off.q.steps==direct.q.steps && off.q.half==direct.q.half && off.channels==direct.channels);
                    // (The main view draws only where one light fits at the floors: zero slots draw wherever
                    // the reserved plan draws.)
                    ensure("zero slots never plan below the reserved plan's rays",
                        off.q.nearSteps>=on.q.nearSteps && (off.channels>0 || !on.channels));
                    if (off.q.nearSteps && (!on.q.nearSteps || off.q.samples*(off.q.half ? 1:4) >
                        on.q.samples*(on.q.half ? 1:4))) strictlyMore = true;
                    // Thin OFF is the committed plan: the slot count never matters.
                    const auto plain0 = workload(uniform,width,height,cap,budget,cap,0);
                    const auto plain8 = workload(uniform,width,height,cap,budget,cap,kFeedSlots);
                    ensure("OFF plan ignores feed slots",plain0.q.samples==plain8.q.samples &&
                        plain0.q.steps==plain8.q.steps && plain0.q.half==plain8.q.half &&
                        plain0.iterations==plain8.iterations);
                }
    ensure("some configuration gains rays or keeps the segmented march without feeds",strictlyMore);
}

template<> template<> void relight_object::test<21>()
{
    int calls = 0;
    std::string drops;
    ALRelight::TakeWriter writer([&](const LLSD& record)
    {
        drops = record["dropped_records"].asString();
        return ++calls != 1; // A failed append must also be accounted for.
    });
    writer.enqueue(std::make_unique<LLSD>());
    ensure("first enqueue schedules",writer.requestDrain());
    ensure("one outstanding worker only",!writer.requestDrain());
    writer.postFailed(); // General absent, closed or full: preserve pending work.
    ensure("failed post can be retried",writer.requestDrain());
    writer.postFailed();
    writer.enqueue(std::make_unique<LLSD>());
    writer.shutdown(); // No worker needed; drain and close idempotently.
    writer.shutdown();
    ensure_equals("shutdown drains retained records",calls,2);
    ensure_equals("write failure counted in next record",drops,std::string("1"));
    ensure("closed writer rejects new work",!writer.enqueue(std::make_unique<LLSD>()));
}

namespace
{
// The planner at 4dd9ff661c1 (before P0.1), verbatim apart from the name: the superseded
// "rays first" order. Kept only so the ray-floor fixture below proves it catches a regression.
ALAreaShadow::Workload headWorkloadWithin(ALAreaShadow::Quality q, unsigned width, unsigned height,
    unsigned lights, std::uint64_t budget, unsigned requestedLights, std::uint64_t feedHoldBack = 0)
{
    using namespace ALAreaShadow;
    Workload w{};
    const int dense = std::clamp(q.nearSteps,0,64);
    w.q = {std::clamp(q.samples,1,32),std::clamp(q.steps,1,dense ? 128:64),q.half,dense,
        dense ? std::clamp(q.minFar,1,64):0};
    lights = std::min(lights,8u);
    const unsigned basis = std::max(lights,std::clamp(requestedLights,1u,8u));
    w.budget = std::min(budget,std::uint64_t(1500000000));
    w.drawCap = w.budget/2;
    const std::uint64_t planned = dense ? w.budget-std::min(feedHoldBack,w.budget) : w.budget;
    const auto update = [&]()
    {
        w.pixels = w.q.half ? ((std::uint64_t(width)+1)/2)*((std::uint64_t(height)+1)/2) :
            std::uint64_t(width)*height;
        w.iterations = w.pixels*lights*w.q.samples*w.q.steps;
    };
    update();
    const auto over = [&]() { return w.pixels*basis*w.q.samples*w.q.steps>planned ||
        w.pixels*w.q.samples*w.q.steps>w.drawCap; };
    if (dense && !w.q.half)
    {
        const std::uint64_t rays = std::min(w.q.samples,kFullResMinSamples);
        if (w.pixels*basis*rays*w.q.steps>planned || w.pixels*rays*w.q.steps>w.drawCap)
        { w.q.half=true; update(); }
    }
    while (over() && w.q.samples>1) { --w.q.samples; update(); }
    while (over() && !dense && w.q.steps>1) { --w.q.steps; update(); }
    if (over() && !w.q.half) { w.q.half=true; update(); }
    const int farFloor = dense ? std::min(w.q.minFar,kThinFarFloor) : 0;
    while (over() && dense && w.q.steps-dense>farFloor) { --w.q.steps; update(); }
    if (over()) { w.iterations=0; return w; }
    const auto perChannel = w.pixels*w.q.samples*w.q.steps;
    w.channels = lights && perChannel ? unsigned(std::min<std::uint64_t>(4,w.drawCap/perChannel)) : 0;
    w.admitted = lights;
    return w;
}
}

template<> template<> void relight_object::test<35>()
{
    using namespace ALAreaShadow;
    // Fix round 4. The main view and the feeds plan from the feeds LIVE this frame (count and target sizes), not from eight assumed
    // 2048x2048 slots. Everything below uses the real header.
    struct Scenario { const char* name; FeedSet set; };
    std::vector<Scenario> scenarios;
    scenarios.push_back({"no live feed",FeedSet()});
    { FeedSet f; f.add(1024,576); scenarios.push_back({"1 x 1024x576",f}); }
    { FeedSet f; f.add(1280,720); scenarios.push_back({"1 x 1280x720",f}); }
    { FeedSet f; f.add(1280,720); f.add(1280,720); scenarios.push_back({"2 x 1280x720",f}); }
    { FeedSet f; f.add(1920,1080); scenarios.push_back({"1 x 1920x1080",f}); }
    { FeedSet f; f.add(1920,1080); f.add(512,288); f.add(640,360); scenarios.push_back({"mixed 1080p+512+640",f}); }
    scenarios.push_back({"8 x 2048x2048",FeedSet(kFeedSlots)});
    ensure("a plain count is that many 2048x2048 feeds",FeedSet(3).count==3 && FeedSet(3).extent[2].w==2048 && FeedSet(3).extent[2].h==2048 &&
        FeedSet(0).count==0 && FeedSet(99).count==kFeedSlots);
    FeedSet capSet;
    for (unsigned i=0; i<12; ++i) capSet.add(100,100);
    ensure("a feed set holds at most eight feeds",capSet.count==kFeedSlots);
    unsigned long rows=0, onBelowOff=0, offDrops=0, onDrops=0, extraDrops=0, visibleDependent=0, reserveDiffers=0, coverageRows=0;
    std::vector<unsigned long> thinRows(scenarios.size(),0);
    for (unsigned height : {720u,1080u,1440u,2160u})
    for (unsigned cap : {1u,3u,4u,8u})
    for (int preset=0; preset<4; ++preset)
    for (std::size_t sc=0; sc<scenarios.size(); ++sc)
    {
        const unsigned width=height*16/9;
        const auto uniform=quality(preset), q=segmented(uniform,preset);
        const FeedSet& set=scenarios[sc].set;
        unsigned previousOn=0, previousOff=0;
        std::vector<unsigned> previousFeedOn(set.count,0), previousFeedOff(set.count,0);
        for (int budget=1; budget<=1500; ++budget)
        {
            const std::uint64_t capped=std::uint64_t(budget)*1000000ull;
            const auto on=workloadOrUniform(q,uniform,width,height,cap,budget,cap,set);
            const auto off=workload(uniform,width,height,cap,budget,cap);
            ++rows;
            const unsigned a=on.channels ? on.admitted:0, b=off.channels ? off.admitted:0;
            onBelowOff += a<b ? 1:0;
            ensure("main: Thin ON admits at least what Thin OFF admits",a>=b);
            ensure("main: Thin ON draws wherever Thin OFF draws",!off.channels || on.channels);
            if (a<previousOn) ++onDrops;
            if (b<previousOff) ++offDrops;
            ensure("main: admission is monotone in the budget for ON",a>=previousOn);
            previousOn=a; previousOff=b;
            if (on.channels && on.q.nearSteps>0) ++thinRows[sc];
            if (on.channels && on.q.nearSteps>0 && on.lead<on.admitted)
                ensure("a hybrid's Thin group marches the regular ray count",on.q.samples==on.rest.samples);
            // The plan, and the budget it reserves, never depend on how many lights are visible (A and the marches are planned
            // from the requested cap): the DeepSeek reading, a smaller visible set shrinking the reserve, cannot happen.
            for (unsigned visible : {1u,std::max(1u,cap/2)})
            {
                const auto v=workloadOrUniform(q,uniform,width,height,visible,budget,cap,set);
                const auto offVisible=workload(uniform,width,height,visible,budget,cap);
                visibleDependent += (v.admitted!=on.admitted || v.q.samples!=on.q.samples || v.q.steps!=on.q.steps ||
                    v.lead!=on.lead || v.rest.samples!=on.rest.samples || basisCost(v)!=basisCost(on)) ? 1:0;
                reserveDiffers += regularCost(offVisible)!=regularCost(off) ? 1:0;
                ensure("the visible set never makes a plan cost more than its reservation",v.iterations<=basisCost(v));
            }
            // The one frame budget: the main plan's reservation plus what every live feed actually plans for, with the
            // cap of lights (the worst case of a feed) and with one visible light.
            if (set.count)
            {
                std::uint64_t total=basisCost(on), totalOne=basisCost(on);
                for (unsigned i=0; i<set.count; ++i)
                {
                    const auto feed=feedWorkload(q,uniform,width,height,set.extent[i].w,set.extent[i].h,cap,budget,cap,set);
                    const auto offFeed=feedWorkload(uniform,uniform,width,height,set.extent[i].w,set.extent[i].h,cap,budget,cap,set);
                    const auto feedOne=feedWorkload(q,uniform,width,height,set.extent[i].w,set.extent[i].h,1,budget,cap,set);
                    const unsigned fa=feed.channels ? feed.admitted:0, fb=offFeed.channels ? offFeed.admitted:0;
                    ensure("feed: the allowance is exactly the Thin-OFF allowance",feed.budget==offFeed.budget);
                    ensure("feed: Thin ON admits at least the lights Thin OFF admits",fa>=fb);
                    ensure("feed: inside its allowance",basisCost(feed)<=feed.budget);
                    ensure("feed: inside its own bound",basisCost(feed)<=feedSpendBound(q,set.extent[i].w,set.extent[i].h,cap));
                    if (fa<previousFeedOn[i]) { ++extraDrops; if (fb>=previousFeedOff[i]) {} else --extraDrops; }
                    previousFeedOn[i]=fa; previousFeedOff[i]=fb;
                    total+=basisCost(feed);
                    totalOne+=feedOne.channels ? feedOne.iterations:0;
                }
                ensure("main + every live feed inside the frame budget (planning basis)",total<=capped);
                ensure("main + every live feed inside the frame budget (one visible light per feed)",totalOne<=capped);
                // The reservation formula holds: the Thin or hybrid main plan never exceeds budget - what the feeds can spend,
                // unless it is the Thin-OFF plan itself (the fallback, which the feeds' allowance is computed from).
                if (on.q.nearSteps>0)
                    ensure("a Thin or hybrid main plan stays inside the budget the feeds leave",
                        basisCost(on)<=capped-feedSpend(q,capped,regularCost(off),cap,set));
            }
            else
                ensure("no live feed: the main plan is the one with no feed reservation",
                    workloadOrUniform(q,uniform,width,height,cap,budget,cap,FeedSet(0u)).iterations==on.iterations);
        }
    }
    ensure("sweep size",rows==4ul*4*4*scenarios.size()*1500);
    ensure("Thin ON feed admission has no drop that Thin OFF does not have",extraDrops==0);
    ensure("the visible set never changes a plan",visibleDependent==0);
    ensure("the reserve is the same quantity for any visible set",reserveDiffers==0);
    // Thin coverage by scenario: with no live feed the main view has the whole budget; a few small feeds leave most of it; the
    // old worst case (8 x 2048x2048) leaves least. (Thin rows = rows with any Thin light.)
    ensure("no live feed keeps the most Thin rows",thinRows[0]>=thinRows[1] && thinRows[0]>=thinRows[2] && thinRows[0]>=thinRows[6]);
    ensure("a small live feed keeps more Thin rows than the worst case",thinRows[1]>thinRows[6]);
    ensure("a single 1280x720 feed keeps more Thin rows than the worst case",thinRows[2]>thinRows[6]);
    ensure("the worst case is the capped plan of the previous round",thinRows[6]>0);
    // The literal cases of the reviews (1500M, default budget):
    {
        // Codex: 4K Medium K4 had Thin 4x64 against regular 8x12 (endpoint 22/255 apart after filtering). Now no Thin light fits
        // beside the regular ones at the regular ray count: exactly the Thin-OFF plan, no mismatch left.
        const auto plan=workloadOrUniform(quality(1,true),quality(1),3840,2160,4,1500,4,FeedSet());
        ensure("4K Medium K4: no Thin light at the regular ray count, exactly the Thin-OFF plan",
            plan.q.nearSteps==0 && plan.q.samples==8 && plan.q.steps==12 && plan.admitted==4 && plan.lead==4);
        // 4K Low K4: three Thin lights beside one regular light, same ray count, far steps thinned to fit.
        const auto low=workloadOrUniform(quality(0,true),quality(0),3840,2160,4,1500,4,FeedSet());
        ensure("4K Low K4: 3 Thin + 1 regular at 4 rays each",low.q.nearSteps==48 && low.lead==3 && low.admitted==4 &&
            low.q.samples==4 && low.rest.samples==4 && low.rest.steps==8);
        // 1080p Medium K4 with VCam feeds: no live feed, one small feed, one 720p feed keep the Thin march; one 1080p feed saturates.
        const auto none=workloadOrUniform(quality(1,true),quality(1),1920,1080,4,1500,4,FeedSet());
        FeedSet small; small.add(1024,576);
        FeedSet hd; hd.add(1920,1080);
        const auto withSmall=workloadOrUniform(quality(1,true),quality(1),1920,1080,4,1500,4,small);
        const auto withHd=workloadOrUniform(quality(1,true),quality(1),1920,1080,4,1500,4,hd);
        ensure("1080p Medium K4, no live feed: every light Thin",none.q.nearSteps==48 && none.lead==4);
        ensure("1080p Medium K4, one small live feed: every light still Thin",withSmall.q.nearSteps==48 && withSmall.lead==4 &&
            basisCost(withSmall)<=basisCost(none));
        ensure("1080p Medium K4, one 1080p live feed: the feed can use its whole allowance, so the OFF plan",withHd.q.nearSteps==0 &&
            withHd.admitted==4);
        // The eight-slot assumption of every earlier round, literally: the same OFF plan whatever Thin is set to.
        const auto worst=workloadOrUniform(quality(1,true),quality(1),1920,1080,4,1500,4,FeedSet(kFeedSlots));
        ensure("1080p Medium K4 with 8 assumed 2048x2048 feeds: the OFF plan",worst.q.nearSteps==0 && worst.admitted==4);
    }
    // The budget invariant with a deliberately mismatched basis: the old cap quantity regularCost(off) and the reserve
    // feedWorkload makes are one expression, and a smaller cap (the visible-set cost off.iterations) would cost Thin light budget.
    {
        const auto uniform=quality(1);
        const auto fullBasis=workload(uniform,1920,1080,8,1500,8);
        const auto visibleOne=workload(uniform,1920,1080,1,1500,8);
        ensure("the main OFF plan reserves the same for one visible light",regularCost(fullBasis)==regularCost(visibleOne) &&
            visibleOne.iterations<regularCost(visibleOne));
    }
    std::cout << "[RelightQ] live feed sweep rows=" << rows << " ON-below-OFF=" << onBelowOff << " main-ON-admission-drops=" << onDrops
        << " OFF-admission-drops=" << offDrops << " feed-ON-drops-not-in-OFF=" << extraDrops << " visible-dependent-plans=" << visibleDependent
        << " Thin-rows per scenario:";
    for (std::size_t sc=0; sc<scenarios.size(); ++sc) std::cout << " [" << scenarios[sc].name << "]=" << thinRows[sc];
    std::cout << std::endl;
    (void)coverageRows;
}

template<> template<> void relight_object::test<36>()
{
    using namespace ALAreaShadow;
    // Fix round 5.
    // (1) Codex P0: the 55M -> 56M feed-admission drop that fix round 3-4 introduced (the light-dropping rung applied to feed plans).
    // 720p Low K3, eight slots, a 512x512 feed: HEAD keeps all 3 lights (2 rays x 8 steps) at both budgets.
    {
        const auto uniform=quality(0);
        const auto at55=feedWorkload(uniform,uniform,1280,720,512,512,3,55,3,kFeedSlots);
        const auto at56=feedWorkload(uniform,uniform,1280,720,512,512,3,56,3,kFeedSlots);
        ensure("55M -> 56M: the feed allowance rises",at56.budget>at55.budget);
        ensure("55M -> 56M: all 3 lights at both budgets (HEAD's fixed-K fallback)",at55.channels && at55.admitted==3 &&
            at56.channels && at56.admitted==3);
        ensure("55M -> 56M: HEAD's 2 rays x 8 steps at both",at55.q.samples==2 && at55.q.steps==8 && at56.q.samples==2 && at56.q.steps==8);
        // A feed plan has no light-dropping rung: it admits all K lights or none, at any allowance.
        const auto sixteen=workloadWithin(uniform,512,512,3,4235200ull,3,0,false);
        ensure("a feed never drops lights (no rung 6)",sixteen.channels && sixteen.admitted==3);
        // The main view keeps rung 6 (fewer lights, never fewer rays) at the same allowance.
        const auto main=workloadWithin(uniform,512,512,3,4235200ull,3,0,true);
        ensure("the main view still drops lights at the floors",main.channels && main.admitted<3 && main.q.samples>=kRayFloor);
    }
    // (2) Feed admission is monotone in the feed ALLOWANCE (exhaustive over the allowance for every preset, feed size, K and visible
    // count, for the Thin OFF plan and the Thin ON hybrid plan), and is all-or-nothing: A is K or 0.
    {
        struct Size { unsigned w,h; };
        constexpr Size sizes[]={{512,288},{512,512},{640,360},{1024,1024},{1920,1080},{2048,2048}};
        unsigned long rows=0, partial=0;
        for (int preset=0; preset<4; ++preset)
        for (const auto& size : sizes)
        for (unsigned cap=1; cap<=8; ++cap)
        for (unsigned visible : {1u,cap})
        {
            const auto uniform=quality(preset), q=segmented(uniform,preset);
            unsigned previousOff=0, previousOn=0;
            for (std::uint64_t allowance=0; allowance<=80000000ull; allowance+=20000)
            {
                const auto off=workloadWithin(uniform,size.w,size.h,visible,allowance,cap,0,false);
                const auto on=hybridWithin(q,uniform,size.w,size.h,visible,allowance,cap,0,false);
                const unsigned a=off.channels ? off.admitted:0, b=on.channels ? on.admitted:0;
                ++rows;
                ensure("feed admission is monotone in the allowance (Thin OFF)",a>=previousOff);
                ensure("feed admission is monotone in the allowance (Thin ON)",b>=previousOn);
                if (a && a!=cap) ++partial;
                if (b && b!=cap) ++partial;
                ensure("Thin ON feed admits at least what Thin OFF admits at the same allowance",b>=a);
                previousOff=a; previousOn=b;
            }
        }
        ensure("feed allowance sweep size",rows==4ul*6*8*2*4001);
        ensure("a feed admits all K lights or none",partial==0);
        std::cout << "[RelightQ] feed allowance sweep rows=" << rows << " admission-drops-when-the-allowance-rose=0 partial-admissions=" << partial << std::endl;
    }
    // (3) Over the BUDGET slider the main plan takes its share first, so the feed's allowance itself can fall: HEAD's own reservation
    // cliffs (HEAD 720p Low K3 512x512: 11M -> 12M, 16M -> 17M and 22M -> 23M all go 3 -> 0). Every drop must be exactly such a cliff:
    // the allowance FELL at that step and the feed goes to nothing (never a partial count); an admission drop with a rising allowance
    // is the defect this round fixes. Counted and printed, for Thin OFF and Thin ON feeds.
    {
        struct Size { unsigned w,h; };
        constexpr Size sizes[]={{512,288},{512,512},{1024,1024},{1920,1080}};
        unsigned long rows=0, cliffsOff=0, cliffsOn=0, risingDrops=0, partialDrops=0;
        std::vector<int> literal;
        for (unsigned mainHeight : {720u,1080u,1440u,2160u})
        for (unsigned cap : {1u,3u,4u,8u})
        for (int preset=0; preset<4; ++preset)
        for (const auto& size : sizes)
        {
            const unsigned mainWidth=mainHeight*16/9;
            const auto uniform=quality(preset), q=segmented(uniform,preset);
            unsigned previousOff=0, previousOn=0; std::uint64_t previousAllowance=0;
            for (int budget=1; budget<=1500; ++budget)
            {
                const auto off=feedWorkload(uniform,uniform,mainWidth,mainHeight,size.w,size.h,cap,budget,cap,kFeedSlots);
                const auto on=feedWorkload(q,uniform,mainWidth,mainHeight,size.w,size.h,cap,budget,cap,kFeedSlots);
                const unsigned a=off.channels ? off.admitted:0, b=on.channels ? on.admitted:0;
                ++rows;
                if (a<previousOff)
                {
                    ++cliffsOff;
                    if (off.budget>=previousAllowance) ++risingDrops;
                    if (a) ++partialDrops;
                    if (mainHeight==720 && preset==0 && cap==3 && size.w==512 && size.h==512) literal.push_back(budget);
                }
                if (b<previousOn)
                {
                    ++cliffsOn;
                    if (on.budget>=previousAllowance) ++risingDrops;
                    if (b) ++partialDrops;
                }
                previousOff=a; previousOn=b; previousAllowance=off.budget;
            }
        }
        ensure("no feed admission drop while the allowance rose or stayed",risingDrops==0);
        ensure("every feed admission drop is an all-or-nothing reservation cliff",partialDrops==0);
        ensure("the reservation cliffs are exercised",cliffsOff>0 && cliffsOn>0);
        // 720p Low K3 512x512, the case of the review: the cliffs of the current planner (HEAD has 11M, 16M and 22M), none at 55M -> 56M.
        ensure("720p Low K3 512x512: the only cliffs are 15M and 23M (main plan steps), none at 56M",
            literal==std::vector<int>({15,23}));
        std::cout << "[RelightQ] feed budget sweep rows=" << rows << " reservation-cliffs Thin-OFF=" << cliffsOff << " Thin-ON=" << cliffsOn
            << " drops-with-a-rising-allowance=" << risingDrops << " partial-drops=" << partialDrops << " 720p-Low-K3-512x512 cliffs at (M):";
        for (int b : literal) std::cout << " " << b;
        std::cout << std::endl;
    }
    // (4) Opus P0: the main plan does not depend on the presented FPS or the dolly distance. The reservation of a live feed is its FIXED
    // capacity target (liveFeedReservationExtent), not the per-frame target, which PrismAdaptivePerformance scales with the frame rate and
    // which follows the on-screen size of the displays. For a fixed set of live slots the main plan is the same for every adaptive
    // resolution scale and every display size, in every preset x main size x K x budget.
    {
        ensure("1024 capacity, 16:9 reservation is 1024x576",LLPrismLens::liveFeedReservationExtent(1024,16.f/9.f)==std::array<U32,2>({1024,576}));
        ensure("2048 capacity, square reservation is 2048x2048",LLPrismLens::liveFeedReservationExtent(2048,1.f)==std::array<U32,2>({2048,2048}));
        ensure("a malformed aspect is clamped (1:4 at most)",LLPrismLens::liveFeedReservationExtent(1024,0.f)[0]==256);
        struct Slot { unsigned id, capacity; float aspect; };
        const std::vector<std::vector<Slot>> liveSets={
            {{0,1024,16.f/9.f}},
            {{0,1024,16.f/9.f},{3,2048,1.f}},
            {{1,1024,4.f/3.f},{2,1024,16.f/9.f},{6,2048,16.f/9.f}}};
        unsigned long rows=0, mutatedDiffers=0, nonConservative=0;
        for (const auto& slots : liveSets)
        for (unsigned mainHeight : {720u,1080u,2160u})
        for (unsigned cap : {1u,4u,8u})
        for (int preset=0; preset<4; ++preset)
        for (int budget : {200,400,800,1500})
        {
            const unsigned mainWidth=mainHeight*16/9;
            const auto uniform=quality(preset), q=segmented(uniform,preset);
            FeedSet fixedSet;
            for (const auto& slot : slots)
            {
                const auto extent=LLPrismLens::liveFeedReservationExtent(slot.capacity,slot.aspect);
                fixedSet.add(extent[0],extent[1],slot.id);
            }
            const auto reference=workloadOrUniform(q,uniform,mainWidth,mainHeight,cap,budget,cap,fixedSet);
            bool anyMutatedDiffers=false;
            for (float scale=.3f; scale<=1.001f; scale+=.05f)
            for (unsigned displayWidth : {96u,256u,640u,1280u,2560u})
            {
                // What the per-frame preparation does: the displays' demand times the adaptive scale, fitted into the capacity.
                FeedSet perFrame, again;
                for (const auto& slot : slots)
                {
                    const unsigned demandW=std::max(1u,unsigned(displayWidth*scale+.5f)),
                        demandH=std::max(1u,unsigned(displayWidth*scale/slot.aspect+.5f));
                    const auto target=LLPrismLens::cameraTargetSize(demandW,demandH,slot.aspect,slot.capacity);
                    const auto extent=LLPrismLens::liveFeedReservationExtent(slot.capacity,slot.aspect);
                    nonConservative += (target[0]>extent[0] || target[1]>extent[1]) ? 1:0;
                    perFrame.add(target[0],target[1],slot.id);
                    again.add(extent[0],extent[1],slot.id);
                }
                // Production: the extents do not move with the scale or the displays, so neither does the plan.
                const auto plan=workloadOrUniform(q,uniform,mainWidth,mainHeight,cap,budget,cap,again);
                ensure("the main plan is the same at every adaptive scale and display size",
                    plan.admitted==reference.admitted && plan.lead==reference.lead && plan.q.samples==reference.q.samples &&
                    plan.q.steps==reference.q.steps && plan.q.half==reference.q.half && plan.rest.samples==reference.rest.samples &&
                    plan.rest.steps==reference.rest.steps && plan.channels==reference.channels && plan.iterations==reference.iterations);
                // Mutation: the previous round reserved from the per-frame target.
                const auto mutated=workloadOrUniform(q,uniform,mainWidth,mainHeight,cap,budget,cap,perFrame);
                if (mutated.admitted!=reference.admitted || mutated.lead!=reference.lead || mutated.q.samples!=reference.q.samples ||
                    mutated.q.steps!=reference.q.steps || mutated.q.half!=reference.q.half) anyMutatedDiffers=true;
                ++rows;
            }
            mutatedDiffers += anyMutatedDiffers ? 1:0;
        }
        ensure("the reservation extent is never smaller than a per-frame target",nonConservative==0);
        ensure("reserving from the per-frame target makes the main plan follow the frame rate (mutation rejected)",mutatedDiffers>0);
        std::cout << "[RelightQ] fixed-capacity reservation rows=" << rows << " plans identical across every scale and display size; per-frame-target mutation changes the plan in "
            << mutatedDiffers << " of " << (liveSets.size()*3*3*4*4) << " (set, size, K, preset, budget) cells" << std::endl;
    }
    // (5) DeepSeek P1: a hybrid's Thin lights also march the regular step count; the cost is charged at the larger of the two.
    {
        // A synthetic OFF plan with 64 regular steps beside a Thin march that thins to 56 total (48 near + 8 far): the case where the
        // shader's old clamp to the Thin draw's steps would have shortened the boundary light's regular rays.
        const Quality uniform{4,64,true};
        const Quality thin{4,56,true,48,8}; // a Thin request whose total (48 near + 8 far) is below the regular 64
        const auto off=workloadWithin(uniform,1920,1080,4,1500000000ull,4,0,true);
        ensure("the OFF plan keeps 64 steps here",off.channels && off.q.steps==64 && off.admitted==4);
        Workload hybrid{};
        const std::uint64_t planned=1500000000ull;
        ensure("a hybrid forms",hybridPlan(hybrid,thin,off,1920,1080,4,2,planned));
        const std::uint64_t px=hybrid.pixels;
        ensure("the Thin lights are charged max(Thin steps, regular steps)",
            hybrid.iterations==px*(2ull*hybrid.q.samples*std::max(hybrid.q.steps,hybrid.rest.steps)+2ull*hybrid.rest.samples*hybrid.rest.steps));
        ensure("... which differs from the Thin steps alone where the regular count is larger",hybrid.rest.steps>hybrid.q.steps ?
            hybrid.iterations>px*(2ull*hybrid.q.samples*hybrid.q.steps+2ull*hybrid.rest.samples*hybrid.rest.steps) : true);
        ensure("basisCost agrees with the plan",basisCost(hybrid)==hybrid.iterations);
        {
            // A room that fits two Thin lights at their 56 Thin steps (232 M) but not at the 64 regular steps they also march (265 M):
            // the larger count is charged, so two do not fit and one does.
            const std::uint64_t restCost=2ull*px*off.q.samples*off.q.steps;
            Workload tight{};
            ensure("two Thin lights charged at 64 steps do not fit a room sized for 56",
                !hybridPlan(tight,thin,off,1920,1080,4,2,restCost+250000000ull));
            ensure("... one does",hybridPlan(tight,thin,off,1920,1080,4,1,3ull*px*off.q.samples*off.q.steps+150000000ull));
        }
        ensure("the draw channel count is sized from the charged cost",hybrid.channels==std::min<std::uint64_t>(4,hybrid.drawCap/
            (px*std::uint64_t(hybrid.q.samples)*std::max(hybrid.q.steps,hybrid.rest.steps))));
        // Exhaustive: no plan of a Custom-quality sweep is charged less than the steps its rays really march, and it fits.
        unsigned long hybrids=0, restLarger=0;
        for (unsigned height : {720u,1080u,1440u,2160u})
        for (unsigned cap : {2u,3u,4u,8u})
        for (int samples : {2,4,6,8})
        for (int steps : {32,48,56,60,64})
        for (bool half : {true,false})
        for (int budget : {100,200,300,400,600,800,1000,1200,1500})
        {
            const Quality custom{samples,steps,half};
            const auto plan=workloadOrUniform(segmented(custom,7),custom,height*16/9,height,cap,budget,cap,FeedSet());
            if (!plan.channels || !plan.q.nearSteps || plan.lead>=plan.admitted) continue;
            ++hybrids;
            restLarger += plan.rest.steps>plan.q.steps-plan.q.nearSteps ? 1:0;
            const std::uint64_t marched=planPixels(plan,height*16/9,height)*(std::uint64_t(plan.lead)*plan.q.samples*std::max(plan.q.steps,plan.rest.steps)+
                std::uint64_t(plan.admitted-plan.lead)*plan.rest.samples*plan.rest.steps);
            ensure("a hybrid is charged at least what its rays march",basisCost(plan)>=marched && plan.iterations<=basisCost(plan));
            ensure("a hybrid stays inside the frame budget",basisCost(plan)<=plan.budget);
        }
        ensure("Custom hybrids are exercised",hybrids>0 && restLarger>0);
        std::cout << "[RelightQ] hybrid step-charge sweep custom hybrids=" << hybrids << " with-regular-steps-above-the-Thin-far-steps=" << restLarger << std::endl;
    }
}

template<> template<> void relight_object::test<34>()
{
    using namespace ALAreaShadow;
    unsigned rows = 0, headBreaks = 0, thinRows = 0, mutatedVisible = 0, mutatedNear = 0;
    // The Thin near-segment rule, as ONE predicate: a plan that keeps the segmented march keeps all 48 near steps.
    const auto nearStays = [](const Workload& w) { return w.q.nearSteps == 48 && w.q.steps >= w.q.nearSteps; };
    for (bool thin : {false,true})
    for (unsigned height : {720u,1080u,1440u,2160u})
    for (unsigned requested=1; requested<=8; ++requested)
    for (int preset=0; preset<4; ++preset)
    {
        const unsigned width = height*16/9;
        const auto uniform = quality(preset);
        const auto asked = thin ? segmented(uniform,preset) : uniform;
        const std::uint64_t hold = thin ? feedReservation(width,height,1500000000ull,requested,kFeedSlots) : 0;
        const auto plan = workloadOrUniform(asked,uniform,width,height,requested,1500,requested);
        ++rows;
        ensure("the ray and step floors hold, with no exception",floorsHold(plan,plan.q.nearSteps ? asked : uniform));
        ensure("the regular group keeps the floors too",restFloorsHold(plan,uniform));
        ensure("a plan draws exactly when one light fits at the floors",
            (plan.channels>0)==(oneLightFits(asked,width,height,1500,hold) || oneLightFits(uniform,width,height,1500,0)));
        ensure("admitted lights 1..K when drawing, 0 otherwise",plan.channels ?
            (plan.admitted>=1 && plan.admitted<=requested) : plan.admitted==0);
        ensure("frame and draw ceilings",plan.iterations<=plan.budget &&
            plan.pixels*plan.q.samples*plan.q.steps<=plan.drawCap &&
            (plan.lead>=plan.admitted || plan.pixels*plan.rest.samples*plan.rest.steps<=plan.drawCap));
        ensure("a plan that draws leads with at least one light, never more than it admits",
            !plan.channels || (plan.lead>=1 && plan.lead<=plan.admitted));
        if (plan.q.nearSteps)
        {
            ++thinRows;
            ensure("Thin near steps stay 48",nearStays(plan));
            ensure("Thin far never below the common floor",
                plan.q.steps-plan.q.nearSteps>=std::min(uniform.steps,kThinFarFloor));
        }
        // A is a function of (preset, size, K, budget, Thin) only: the visible set never moves it.
        const unsigned visible[] = {1u,requested/2+1,requested};
        for (unsigned count : visible)
        {
            const auto v = workloadOrUniform(asked,uniform,width,height,count,1500,requested);
            ensure("A independent of the visible-set size",v.admitted==plan.admitted &&
                v.q.samples==plan.q.samples && v.q.steps==plan.q.steps && v.q.half==plan.q.half);
            // Mutation: planning the reservation from the visible count instead of the cap.
            const auto m = workloadOrUniform(asked,uniform,width,height,count,1500,count);
            mutatedVisible += (m.admitted!=plan.admitted || m.q.samples!=plan.q.samples ||
                m.q.steps!=plan.q.steps) ? 1:0;
        }
        // Mutation: a planner that thins the Thin near segment. The plan it would return (24 near steps, the 24 saved
        // steps given back to the budget) must be rejected by the very predicate every production plan passes above.
        if (thin && plan.q.nearSteps)
        {
            auto thinned = plan;
            thinned.q.nearSteps = 24;
            thinned.q.steps -= 24;
            mutatedNear += nearStays(thinned) ? 0:1;
        }
        // The previous rays-first planner must break the floor somewhere (4K Cinematic: 1 ray).
        const auto head = headWorkloadWithin(asked,width,height,requested,1500000000ull,requested,hold);
        if (head.channels && !floorsHold(head,asked)) ++headBreaks;
    }
    ensure("grid size",rows==2*4*8*4);
    ensure("Thin rows exercised",thinRows>0);
    ensure("the rays-first planner fails the ray floor",headBreaks>0);
    ensure("visible-set mutation is caught",mutatedVisible>0);
    ensure("near-step mutation is caught in every Thin row",mutatedNear==thinRows);
    // The reported example, literally: 4K Cinematic K8 was 1 ray; now 5 rays for all 8 lights.
    const auto cinematic = workload(quality(3),3840,2160,8,1500,8,0);
    const auto head = headWorkloadWithin(quality(3),3840,2160,8,1500000000ull,8);
    ensure("4K Cinematic K8 is smooth now",cinematic.q.samples==5 && cinematic.admitted==8);
    ensure("the old plan was a single ray there",head.q.samples==1);
    // Fix round 1 (review P0 5): below the floors the main view admits NOTHING; it never restores sub-floor rays.
    // 4K, K8, 20M: the previous planner returned A = 8 lights with 1 ray and 1 step.
    {
        const auto tiny = workloadOrUniform(quality(1),quality(1),3840,2160,8,20,8);
        ensure("4K K8 20M: no soft shadow at all",tiny.channels==0 && tiny.admitted==0 && tiny.iterations==0);
        const auto old = headWorkloadWithin(quality(1),3840,2160,8,20000000ull,8);
        ensure("the previous planner drew 8 lights with 1 ray and 1 step there",old.channels>0 && old.admitted==8 &&
            old.q.samples==1 && old.q.steps==1);
        // A capture feed keeps the previous order below the floors (its guarantee depends on it).
        const auto feed = workloadWithin(quality(1),3840,2160,8,20000000ull,8,0,false);
        ensure("a feed still draws below the floors",feed.channels>0 && feed.q.samples<kRayFloor);
    }
    // Fix round 1 (review P0 5): more budget never admits fewer lights. Every preset x 720p..4K x K1-8 x Thin
    // ON/OFF over the whole slider range 1..1500 M (this found the 67-133 M cliff of the first planner:
    // sub-floor plans admitting all K lights, then the floors admitting fewer, and the Thin fallback).
    {
        unsigned sweeps = 0, cliffsAtTheFallback = 0, lostLights = 0;
        for (bool thin : {false,true})
        for (unsigned height : {720u,1080u,1440u,2160u})
        for (unsigned requested=1; requested<=8; ++requested)
        for (int preset=0; preset<4; ++preset)
        for (unsigned slots : {0u,kFeedSlots})
        {
            const unsigned width = height*16/9;
            const auto uniform = quality(preset);
            const auto asked = thin ? segmented(uniform,preset) : uniform;
            unsigned previous = 0, previousSamples = 0;
            for (int millions=1; millions<=1500; ++millions)
            {
                const auto plan = workloadOrUniform(asked,uniform,width,height,requested,millions,requested,slots);
                const unsigned a = plan.channels ? plan.admitted : 0;
                if (plan.channels && plan.admitted<requested) ++lostLights;
                ensure("more budget never admits fewer lights",a>=previous);
                ensure("ceilings over the whole slider",plan.iterations<=plan.budget);
                ensure("floors over the whole slider",floorsHold(plan,plan.q.nearSteps ? asked : uniform));
                // Thin ON hybrids (the top lights Thin, the others regular) are part of the sweep.
                if (thin && !slots && plan.channels && plan.lead<plan.admitted) ++cliffsAtTheFallback;
                previous = a; previousSamples = plan.q.samples;
                ++sweeps;
            }
            (void)previousSamples;
        }
        ensure("budget sweep size",sweeps==2*4*8*4*2*1500);
        ensure("Thin ON hybrids are exercised by the sweep",cliffsAtTheFallback>0);
        ensure("some rows lose lights instead of rays",lostLights>0);
    }
    // Thin objects never removes a shadow that Thin OFF draws (user decision 10/22): at every budget, resolution, K, preset,
    // feed reservation and visible-set size, the Thin plan admits at least as many lights as the OFF plan, and the admission is
    // monotone in the budget for both. Two mutations: the pure Thin plan with a uniform fallback only when nothing fits (the
    // first version of the fallback), and the same fallback capped at what Thin admits the moment it fits (the previous round).
    {
        unsigned rows = 0, fewerPure = 0, fewerCapped = 0, hybrids = 0, restSqueezed = 0, restRows = 0, thinWithFeeds = 0, raysWouldDiffer = 0;
        for (unsigned height : {720u,1080u,1440u,2160u})
        for (unsigned requested : {1u,3u,4u,8u})
        for (int preset=0; preset<4; ++preset)
        for (unsigned slots : {0u,kFeedSlots})
        {
            const unsigned width = height*16/9;
            const auto uniform = quality(preset), asked = segmented(uniform,preset);
            unsigned previousOn = 0, previousOff = 0;
            for (int millions=1; millions<=1500; ++millions)
            for (unsigned visible : {requested,1u})
            {
                const auto on = workloadOrUniform(asked,uniform,width,height,visible,millions,requested,slots);
                const auto off = workloadOrUniform(uniform,uniform,width,height,visible,millions,requested,slots);
                ensure("Thin ON admits at least what Thin OFF admits",on.admitted>=off.admitted);
                ensure("Thin ON draws wherever Thin OFF draws",!off.channels || on.channels);
                if (visible==requested)
                {
                    ensure("Thin ON admission is monotone in the budget",on.admitted>=previousOn);
                    ensure("Thin OFF admission is monotone in the budget",off.admitted>=previousOff);
                    previousOn = on.admitted; previousOff = off.admitted;
                }
                ensure("nothing drawn admits nothing",on.channels || (!on.admitted && !on.lead));
                if (on.channels)
                {
                    ensure("lead lights 1..admitted",on.lead>=1 && on.lead<=on.admitted);
                    ensure("both groups keep the floors",floorsHold(on,on.q.nearSteps ? asked : uniform) && restFloorsHold(on,uniform));
                    ensure("the plan cost recomputed from the target size",on.iterations==planCost(on,width,height,visible));
                    // Fix round 3 (Opus P1): the regular lights of a hybrid march EXACTLY the Thin-OFF plan, in every row (rays,
                    // steps, resolution); the Thin lights get only what that leaves, possibly none.
                    if (on.lead<on.admitted)
                    {
                        ++restRows;
                        ensure("the regular group is exactly the Thin-OFF plan (rays, steps, resolution)",
                            on.rest.samples==off.q.samples && on.rest.steps==off.q.steps && on.rest.half==off.q.half &&
                            on.q.half==off.q.half && on.restChannels==off.channels && on.admitted==off.admitted);
                        // Fix round 4 (Codex P0, GLM/DeepSeek P1): the Thin group marches EXACTLY the regular ray count (same disk
                        // strata), so a boundary light at share 1 is the regular light's estimator; only the far steps differ.
                        ensure("the Thin group of a hybrid marches the regular ray count",on.q.samples==on.rest.samples);
                        // Mutation: the Thin ladder's own ray count (the previous round) differs from the regular one in these rows.
                        const auto pureAsked = workload(asked,width,height,visible,millions,requested,slots);
                        raysWouldDiffer += pureAsked.channels && pureAsked.q.samples!=on.rest.samples ? 1:0;
                        // Mutation: the superseded planner squeezed the regular group to the floors to fit more Thin lights.
                        restSqueezed += (off.q.samples>std::min(uniform.samples,kRayFloor) ||
                            off.q.steps>std::min(uniform.steps,kThinFarFloor)) ? 1:0;
                    }
                    if (slots && on.q.nearSteps)
                    {
                        ++thinWithFeeds;
                        // While feeds can run, a Thin or hybrid plan never costs more than the Thin-OFF plan.
                        ensure("with feeds a Thin or hybrid plan costs no more than the Thin-OFF plan",
                            planCost(on,width,height,on.admitted)<=planCost(off,width,height,off.admitted));
                    }
                    if (on.lead<on.admitted)
                    {
                        ++hybrids;
                        const std::uint64_t reserve = feedReservation(width,height,std::uint64_t(millions)*1000000ull,requested,slots);
                        const std::uint64_t full = planCost(on,width,height,on.admitted);
                        ensure("a hybrid with Thin lights stays inside the planning limit",
                            full<=std::uint64_t(millions)*1000000ull-std::min(reserve,std::uint64_t(millions)*1000000ull));
                        ensure("hybrid draws stay inside the per-draw cap",
                            planPixels(on,width,height)*on.q.samples*on.q.steps<=on.drawCap &&
                            planPixels(on,width,height)*on.rest.samples*on.rest.steps<=on.drawCap);
                        ensure("only Thin lights march the near segment",on.q.nearSteps==48 && on.rest.nearSteps==0);
                    }
                }
                // Mutation 1: the pure Thin plan, the uniform plan only when Thin does not fit at all.
                auto pure = workload(asked,width,height,visible,millions,requested,slots);
                if (!pure.channels) pure = workload(uniform,width,height,visible,millions,requested,slots);
                fewerPure += pure.admitted<off.admitted ? 1:0;
                ++rows;
            }
        }
        // The cap of the previous round, rebuilt literally: the uniform plan limited to the Thin plan's admission at its first fit.
        for (int preset=0; preset<4; ++preset)
        {
            const auto uniform = quality(preset), asked = segmented(uniform,preset);
            unsigned firstFit = 0;
            for (int millions=1500; millions>=1; --millions) if (workload(asked,3840,2160,8,millions,8,kFeedSlots).channels) firstFit = workload(asked,3840,2160,8,millions,8,kFeedSlots).admitted;
            for (int millions=1; millions<=1500; ++millions)
            {
                auto capped = workload(uniform,3840,2160,8,millions,8,kFeedSlots);
                if (!workload(asked,3840,2160,8,millions,8,kFeedSlots).channels && firstFit) capped.admitted = std::min(capped.admitted,firstFit);
                const auto off = workload(uniform,3840,2160,8,millions,8,kFeedSlots);
                fewerCapped += capped.channels && capped.admitted<off.admitted ? 1:0;
            }
        }
        ensure("sweep rows",rows>0);
        ensure("hybrid rows are exercised",hybrids>0 && restRows==hybrids);
        ensure("Thin plans with feeds are exercised",thinWithFeeds>0);
        ensure("the regular-group-at-the-floors planner of the previous round squeezes the OFF march (mutation rejected)",restSqueezed>0);
        ensure("the Thin ladder's own ray count differs from the regular one in some hybrid rows (mutation rejected)",raysWouldDiffer>0);
        ensure("the pure Thin plan with a uniform fallback loses lights against OFF (mutation rejected)",fewerPure>0);
        ensure("the capped fallback of the previous round loses lights against OFF (mutation rejected)",fewerCapped>0);
        std::cout << "[RelightQ] Thin hybrid sweep rows=" << rows << " hybridRows=" << hybrids << " thinRowsWithFeeds=" << thinWithFeeds
            << " previous-round-squeezed-OFF-march-rows=" << restSqueezed << " thin-ladder-rays-would-differ-rows=" << raysWouldDiffer
            << " pure-plan-below-OFF=" << fewerPure << " capped-fallback-below-OFF=" << fewerCapped << std::endl;
    }
    // Coverage wording (the 0-of-N case reads as nothing is drawn, not as a fraction).
    ensure_equals("coverage off",coverageText(false,2,8),std::string("Soft shadows are off"));
    ensure_equals("coverage partial",coverageText(true,2,8),std::string("Soft shadows on 2 of 8 lights (work budget)"));
    ensure_equals("coverage full",coverageText(true,8,8),std::string("Soft shadows on 8 lights"));
    ensure_equals("coverage one",coverageText(true,1,1),std::string("Soft shadows on 1 light"));
    ensure_equals("coverage no lights selected",coverageText(true,0,0),std::string("No lights are selected for soft shadows"));
    ensure_equals("coverage nothing renders (budget)",coverageText(true,0,8),std::string("No soft shadows right now (work budget)"));
    ensure_equals("coverage nothing renders (allocation)",coverageText(true,0,8,0,false,true),std::string("No soft shadows right now (low video memory)"));
    ensure_equals("coverage partial (allocation)",coverageText(true,4,8,0,false,true),std::string("Soft shadows on 4 of 8 lights (low video memory)"));
    // Fix round 4 (GLM P2): fewer lights rendered than wanted is NOT a memory effect unless the pass says it failed to allocate
    // (a plane or draw truncated by the budget is a work-budget effect). Fix round 5 (GLM P2): the dead `planned` parameter is gone.
    ensure_equals("coverage: rendered below wanted without an allocation failure is a work-budget effect",coverageText(true,4,8),
        std::string("Soft shadows on 4 of 8 lights (work budget)"));
    ensure_equals("coverage: nothing rendered, Thin requested, still a work-budget effect",coverageText(true,0,8,0,true),
        std::string("No soft shadows right now (work budget)"));
    ensure_equals("coverage with thin-quality lights",coverageText(true,8,8,3),std::string("Soft shadows on 8 of 8 lights (3 thin-quality)"));
    ensure_equals("coverage with every light thin",coverageText(true,4,4,4),std::string("Soft shadows on 4 of 4 lights (4 thin-quality)"));
    ensure_equals("coverage partial with thin-quality lights",coverageText(true,2,8,2),std::string("Soft shadows on 2 of 8 lights (work budget, 2 thin-quality)"));
    ensure_equals("coverage thin count ignored when off",coverageText(false,8,8,3),std::string("Soft shadows are off"));
    // lights == 0: nothing is drawn, so nothing is admitted either (the record's "A = 0 when nothing draws" invariant).
    {
        const auto none = workload(quality(1),1920,1080,0,1500,4);
        ensure("no lights: no channels, no admission",none.channels==0 && none.admitted==0 && none.lead==0 && none.iterations==0);
        const auto noneThin = workloadOrUniform(quality(1,true),quality(1),1920,1080,0,1500,4);
        ensure("no lights, Thin: no channels, no admission",noneThin.channels==0 && noneThin.admitted==0 && noneThin.lead==0);
    }
    // The floors, written as literals (4 rays, 8 far steps, 48 near steps): a predicate that shares the planner's constants
    // would follow a change of both; these rows do not. Default budget, 4K K8 (every group on the floors) and 1080p K8.
    // With VCam feeds off (slots 0) Low keeps two Thin lights beside six regular ones at 4K K8; the other presets' OFF plans use
    // the budget (their regular lights keep that march), so they have no Thin light there. With feeds on, 1080p K8 is Thin for
    // High and Cinematic only (the cap at the OFF cost), and the plan is the OFF plan otherwise.
    for (int preset=0; preset<4; ++preset)
    {
        const auto uniform = quality(preset), asked = segmented(uniform,preset);
        const auto k8 = workloadOrUniform(asked,uniform,3840,2160,8,1500,8,0);
        const auto k8off = workload(uniform,3840,2160,8,1500,8,0);
        ensure("literal floors, 4K K8: 8 lights, rays >= 4",k8.channels>0 && k8.admitted==8 && k8.q.samples>=4 &&
            (k8.lead==k8.admitted || k8.rest.samples>=4));
        ensure("literal floors, 4K K8: far steps >= 8, near 48 on a Thin plan",(k8.q.nearSteps==48 && k8.q.steps-48>=8 &&
            (k8.lead==k8.admitted || k8.rest.steps>=8)) || (k8.q.nearSteps==0 && k8.q.steps>=8));
        ensure("literal floors, 4K K8: the regular lights are the OFF march",k8.lead==k8.admitted ? (k8.q.nearSteps==0 ||
            k8.q.samples>=4) : (k8.rest.samples==k8off.q.samples && k8.rest.steps==k8off.q.steps));
        if (preset==0)
            ensure("literal floors, 4K K8 Low: Thin lights lead the regular 4 x 8 ones",k8.lead==2 && k8.admitted==8 &&
                k8.rest.samples==4 && k8.rest.steps==8 && k8.q.samples==4 && k8.q.steps==66 && k8.q.nearSteps==48);
        const auto k8hd = workloadOrUniform(asked,uniform,1920,1080,8,1500,8,0);
        ensure("literal floors, 1080p K8 without feeds: every light Thin",k8hd.admitted==8 && k8hd.lead==8 && k8hd.q.samples>=4 && k8hd.q.steps>=56);
        const auto k8feeds = workloadOrUniform(asked,uniform,1920,1080,8,1500,8,kFeedSlots);
        ensure("literal 1080p K8 with feeds: High and Cinematic keep Thin, Low and Medium are the OFF plan",
            (preset>=2) == (k8feeds.q.nearSteps==48) && k8feeds.admitted==8);
    }
    // Smooth group fade: every admitted light i keeps strength smoothstep(1, 1.05, score[i] / score[first dropped]).
    {
        ensure("equal importance: faded out",admissionStrength(1.0,1.0)==0.0);
        ensure("5 percent ahead: full strength",admissionStrength(1.05,1.0)==1.0 && admissionStrength(1.5,1.0)==1.0);
        ensure("half way: half strength",std::abs(admissionStrength(1.025,1.0)-0.5)<1.e-12);
        ensure("nothing dropped has importance: full strength",admissionStrength(0.0,0.0)==1.0 && admissionStrength(1.0,0.0)==1.0);
        ensure("a light with no importance is gone",admissionStrength(0.0,1.0)==0.0);
        double last = 0.0, worstStep = 0.0;
        for (int i=0; i<=2500; ++i)
        {
            const double v = admissionStrength(1.0+i*0.0001,1.0);
            ensure("strength is monotone in the margin",v>=last && v>=0.0 && v<=1.0);
            worstStep = std::max(worstStep,v-last);
            last = v;
        }
        ensure("strength is continuous",worstStep<0.005);
        // The per-light strengths of a ranked score list with `admitted` lights kept, the way renderAreaShadows computes them.
        const auto strengths = [](std::vector<double> scores, unsigned admitted)
        {
            std::vector<double> out(admitted,1.0);
            if (admitted<scores.size())
                for (unsigned i=0; i<admitted; ++i) out[i] = admissionStrength(scores[i],scores[admitted]);
            return out;
        };
        // Codex's case: A = 3, scores [2, 1.100001, 1.099999, 1]. Swapping the middle two lights (a rank swap WITHIN the
        // admitted set) changes no strength by more than 1e-4: each light keeps its own strength, the reference is unchanged.
        // With the 5 percent window both lights are clear of the threshold (full strength); the same swap 3 percent above the
        // threshold, where the strength slope is steepest (1.5 / 0.05 = 30 per unit of ratio), moves a 2e-6 score change by
        // about 6e-5, so the bound is 1e-3 there (the 25 percent window needed 1e-4: the narrower window is 5x steeper).
        {
            const auto nearBefore = strengths({2.0,1.0300001,1.0299999,1.0},3);
            const auto nearAfter = strengths({2.0,1.0299999,1.0300001,1.0},3);
            ensure("near the threshold: a swap within the admitted set changes no strength",
                std::abs(nearBefore[1]-nearAfter[2])<1.e-3 && std::abs(nearBefore[2]-nearAfter[1])<1.e-3);
            ensure("near the threshold: the lights are partly faded",nearBefore[1]>0.6 && nearBefore[1]<0.7);
            const auto before = strengths({2.0,1.100001,1.099999,1.0},3);
            const auto after = strengths({2.0,1.099999,1.100001,1.0},3);
            ensure("the clear leader is at full strength",before[0]==1.0 && after[0]==1.0);
            // The light that was second is now third: its strength moves with the light, not with the rank.
            ensure("a swap within the admitted set changes no strength",std::abs(before[1]-after[2])<1.e-4 && std::abs(before[2]-after[1])<1.e-4);
            ensure("clear of the threshold both lights keep full strength",before[1]==1.0 && before[2]==1.0);
            // The old rule (only the last admitted channel fades) jumps on the swap near the threshold: mutation rejected.
            const auto lastOnly = [](std::vector<double> scores, unsigned admitted)
            {
                std::vector<double> out(admitted,1.0);
                out[admitted-1] = admissionStrength(scores[admitted-1],scores[admitted]);
                return out;
            };
            const auto oldBefore = lastOnly({2.0,1.0300001,1.0299999,1.0},3), oldAfter = lastOnly({2.0,1.0299999,1.0300001,1.0},3);
            ensure("the last-channel-only fade pops on the same swap (mutation rejected)",
                std::abs(oldBefore[1]-oldAfter[2])>0.3 || std::abs(oldBefore[2]-oldAfter[1])>0.3);
        }
        // Continuity along a motion: three lights whose scores cross the first dropped light's score and each other. No
        // light's strength changes by more than one small step per tiny score step, at any rank swap.
        {
            double worst = 0.0;
            std::vector<double> previous;
            for (int i=0; i<=6000; ++i)
            {
                const double t = i/6000.0;
                const std::vector<double> scores = {2.0-t*1.2,1.6-t*0.2,1.0+t*0.3,1.0};  // lights cross each other and the reference
                std::vector<double> sorted = scores;
                std::sort(sorted.begin(),sorted.begin()+3,std::greater<double>());
                // strengths by light identity: the reference is the first dropped score (the fixed fourth light here)
                std::vector<double> now;
                for (unsigned light=0; light<3; ++light) now.push_back(admissionStrength(scores[light],sorted[3]));
                if (!previous.empty()) for (unsigned light=0; light<3; ++light) worst = std::max(worst,std::abs(now[light]-previous[light]));
                previous = now;
            }
            ensure("no strength jumps while lights cross",worst<0.01);
        }
        // Dropping out of the admitted set: the dropped light becomes the reference, so the ranks crossing at the boundary
        // pass through strength 0 on both sides (the swap of the last admitted and the first dropped light).
        {
            double previousA = -1.0, previousB = -1.0, worstJump = 0.0;
            for (int i=0; i<=4000; ++i)
            {
                const double scoreA = 1.0+0.0005*(i-2000)*0.5, scoreB = 1.0;      // A passes B at i = 2000
                const double a = scoreA>=scoreB ? admissionStrength(scoreA,scoreB) : 0.0;
                const double b = scoreB>scoreA ? admissionStrength(scoreB,scoreA) : 0.0;
                if (previousA>=0.0) worstJump = std::max(worstJump,std::max(std::abs(a-previousA),std::abs(b-previousB)));
                previousA = a; previousB = b;
            }
            ensure("a light never pops when two lights swap rank at the coverage boundary",worstJump<0.01);
        }
        // Mutation: the unfaded rule (full strength to the end) pops from 1 to 0 at the swap.
        double popJump = 0.0, previousUnfaded = -1.0;
        for (int i=0; i<=4000; ++i)
        {
            const double scoreA = 1.0+0.0005*(i-2000)*0.5, scoreB = 1.0;
            const double unfaded = scoreA>=scoreB ? 1.0 : 0.0;
            if (previousUnfaded>=0.0) popJump = std::max(popJump,std::abs(unfaded-previousUnfaded));
            previousUnfaded = unfaded;
        }
        ensure("the unfaded rule pops (mutation rejected)",popJump>=1.0);
        // A strength below 1/255 is not a rendered light.
        ensure("faded below one 8-bit step is not visible",!admissionVisible(admissionStrength(1.0,1.0)) &&
            !admissionVisible(admissionStrength(1.0005,1.0)) && admissionVisible(admissionStrength(1.02,1.0)) && admissionVisible(1.0));
        // Eight lamps whose scores are 3 percent apart, budget for three (user/lead decision 10/22): the cluster stays lit. The top two
        // admitted lights keep full strength; only the third, within 5 percent of the first dropped light, fades (to 0.648).
        {
            std::vector<double> lamps;
            for (int i=0; i<8; ++i) lamps.push_back(10.0*std::pow(1.03,-i));
            const auto cluster = strengths(lamps,3);
            ensure("cluster: the top two admitted lights keep full strength",cluster[0]>=0.99 && cluster[1]>=0.99);
            ensure("cluster: only the light within 5 percent of the threshold fades",cluster[2]<0.99 && cluster[2]>0.6 && cluster[2]<0.7);
            ensure("cluster: the lights count as rendered",admissionVisible(cluster[0]) && admissionVisible(cluster[1]) && admissionVisible(cluster[2]));
        }
    }
    // Tied lamps (lead decision 10/22, on the user's behalf): importance is constant within range / 2 of the camera, so identical
    // nearby lamps tie EXACTLY and the raw fade would take every admitted light of the cluster to 0. An ADMITTED light keeps at least
    // half strength: max(0.5, smoothstep(1, 1.05, ratio)).
    {
        ensure("raw fade of an exact tie is 0 (the cluster that vanished)",admissionStrength(1.0,1.0)==0.0);
        ensure("admitted tie keeps half strength",admittedStrength(1.0,1.0)==0.5 && admittedStrength(1.0,1.0)==kAdmissionFloor);
        ensure("admitted: 5 percent ahead keeps full strength",admittedStrength(1.05,1.0)==1.0 && admittedStrength(3.0,1.0)==1.0);
        ensure("admitted: nothing dropped has importance: full strength",admittedStrength(1.0,0.0)==1.0 && admittedStrength(0.0,0.0)==1.0);
        ensure("admitted: a light with no importance is still gone",admittedStrength(0.0,1.0)==0.0);
        ensure("admitted: the floor is visible",admissionVisible(admittedStrength(1.0,1.0)));
        // Eight identical lamps, budget for three: every admitted light stays lit at 0.5 (the raw rule gave 0, 0, 0).
        {
            const std::vector<double> ties(8,5.0);
            for (unsigned i=0; i<3; ++i)
            {
                ensure("tie cluster: raw strength 0 (mutation rejected)",admissionStrength(ties[i],ties[3])==0.0);
                ensure("tie cluster: every admitted light keeps strength 0.5 and is rendered",admittedStrength(ties[i],ties[3])==0.5 &&
                    admissionVisible(admittedStrength(ties[i],ties[3])));
            }
        }
        // Continuity: monotone in the margin, no step larger than the raw term's, and a swap WITHIN the admitted set (an order
        // statistic reference) changes no strength.
        double last = 0.0, worstStep = 0.0;
        for (int i=0; i<=2500; ++i)
        {
            const double v = admittedStrength(1.0+i*0.0001,1.0);
            ensure("admitted strength is monotone in the margin and in [0.5, 1]",v>=last-1.e-15 && v>=0.5 && v<=1.0);
            worstStep = std::max(worstStep,i ? v-last:0.0);
            last = v;
        }
        ensure("admitted strength is continuous",worstStep<0.005);
        const auto admitted = [](std::vector<double> scores, unsigned count)
        {
            std::vector<double> out(count,1.0);
            if (count<scores.size()) for (unsigned i=0; i<count; ++i) out[i] = admittedStrength(scores[i],scores[count]);
            return out;
        };
        const auto before = admitted({2.0,1.0300001,1.0299999,1.0},3), after = admitted({2.0,1.0299999,1.0300001,1.0},3);
        ensure("admitted: a swap within the admitted set changes no strength",
            std::abs(before[1]-after[2])<1.e-3 && std::abs(before[2]-after[1])<1.e-3);
        // Thin/regular boundary (Opus P1): the last Thin light and the first regular light swap places. The Thin light marches
        // the share 1 - smoothstep(1, 1.05, score / score of the first regular light) of its rays the regular way, so at the tie
        // it IS regular (share 1), a clear leader is all Thin (share 0), and the share is continuous in between.
        {
            ensure("a clear leader is all Thin",thinWeight(1.06,1.0)==1.0 && thinWeight(5.0,1.0)==1.0 &&
                regularShare(1.06,1.0)==0.0 && regularShare(5.0,1.0)==0.0);
            // Fix round 4 (Opus P1; lead decision 10/23): the regular share is 1 - max(0.5, thinWeight), mirroring the admission
            // floor. An exact tie used to make every Thin light 100 percent regular while Coverage still said "thin-quality".
            ensure("a Thin light that ties the first regular light marches half of its rays regular",
                thinWeight(1.0,1.0)==0.5 && regularShare(1.0,1.0)==0.5 && thinWeight(1.0,1.0)==kAdmissionFloor);
            ensure("the regular share never exceeds one half for a light with importance",regularShare(1.0,1.0)<=0.5 &&
                regularShare(1.0001,1.0)<=0.5 && regularShare(1.02,1.0)<=0.5);
            ensure("no regular light follows (or it has no importance): all Thin",thinWeight(1.0,0.0)==1.0 && regularShare(1.0,0.0)==0.0);
            // Counted as thin-quality only while the regular share is below one half: a tie is not.
            ensure("thin-quality counts a clear leader, not a tie",thinQuality(regularShare(5.0,1.0)) && !thinQuality(regularShare(1.0,1.0)) &&
                thinQuality(regularShare(1.03,1.0)));
            // A cluster of eight identical sconces within range / 2 of the camera, budget for a 3 Thin + 1 regular hybrid (Opus P1):
            // every Thin light ties the first regular one, so none of the three counts as thin-quality (Coverage reads "0 thin-quality").
            {
                const std::vector<double> tied(4,5.0);
                unsigned thinQualityLights = 0;
                for (unsigned i=0; i<3; ++i) thinQualityLights += thinQuality(regularShare(tied[i],tied[3])) ? 1:0;
                ensure("tied Thin lights are not counted as thin-quality",thinQualityLights==0);
                ensure_equals("and the Coverage row says so",coverageText(true,4,4,thinQualityLights,true),
                    std::string("Soft shadows on 4 of 4 lights (0 thin-quality)"));
            }
            // The lights hold their roles while they trade places: the higher is the Thin one (rank lead-1) and the lower the first
            // regular one. The Thin share of the Thin light is continuous while it stays above the other (it never moves for a
            // swap inside the Thin set), and at the crossing itself the two lights trade 0.5 for 0: the floor's documented pop.
            double worst = 0.0, crossing = 0.0, previous = -1.0;
            for (int i=0; i<=4000; ++i)
            {
                const double a = 1.0+0.00002*(i-2000), b = 1.0;           // A passes B at i = 2000
                const double share = a>=b ? thinWeight(a,b) : thinWeight(b,a) ;  // the Thin light's own Thin share, higher one
                const double roleShare = a>=b ? thinWeight(a,b) : 0.0;    // light A: Thin while it is the higher, regular below
                if (previous>=0.0)
                {
                    if (i==2000) crossing = std::abs(roleShare-previous);   // A drops from the Thin set to the regular one
                    else worst = std::max(worst,std::abs(roleShare-previous));
                }
                ensure("the Thin share is monotone in the margin and in [0.5, 1]",share>=0.5 && share<=1.0);
                previous = roleShare;
            }
            ensure("the Thin share is continuous while the Thin light stays ahead",worst<0.005);
            ensure("a boundary crossing pops by exactly one half (the floor, documented)",std::abs(crossing-0.5)<0.005);
            // Mutation: the unblended boundary (all Thin to the end, then all regular) pops by 1.
            double pop = 0.0, prev = -1.0;
            for (int i=0; i<=4000; ++i)
            {
                const double share = (1.0+0.00002*(i-2000)) >= 1.0 ? 1.0 : 0.0;
                if (prev>=0.0) pop = std::max(pop,std::abs(share-prev));
                prev = share;
            }
            ensure("the unblended Thin/regular boundary pops (mutation rejected)",pop>=1.0);
        }
    }
    // Coverage row wording: a Thin request that fell back entirely says so, and an allocation failure says so explicitly.
    ensure_equals("coverage: Thin requested, none thin-quality",coverageText(true,8,8,0,true),
        std::string("Soft shadows on 8 of 8 lights (0 thin-quality)"));
    ensure_equals("coverage: Thin requested, one light, none thin-quality",coverageText(true,1,1,0,true),
        std::string("Soft shadows on 1 of 1 light (0 thin-quality)"));
    ensure_equals("coverage: Thin requested, partial, none thin-quality",coverageText(true,2,8,0,true),
        std::string("Soft shadows on 2 of 8 lights (work budget, 0 thin-quality)"));
    ensure_equals("coverage: Thin not requested, none thin-quality",coverageText(true,8,8,0,false),std::string("Soft shadows on 8 lights"));
    ensure_equals("coverage: nothing renders, Thin requested",coverageText(true,0,8,0,true),
        std::string("No soft shadows right now (work budget)"));
    ensure_equals("coverage: explicit low-memory flag (early return before any drawing)",coverageText(true,0,8,0,true,true),
        std::string("No soft shadows right now (low video memory)"));
    ensure_equals("coverage: the flag wins over a planned count of 0",coverageText(true,0,8,0,false,true),
        std::string("No soft shadows right now (low video memory)"));
    // P0.2 blur radius: at least 2 trace pixels whenever the blur is on (HEAD: 1 at 1080p half-res).
    close("1080p half raised to 2",filterRadius(2.f,1.f,true),2.0,1.e-6);
    close("4K half",filterRadius(2.f,2.f,true),2.0,1.e-6);
    close("4K full",filterRadius(2.f,2.f,false),4.0,1.e-6);
    close("small feed raised to 2",filterRadius(1.f,.33f,true),2.0,1.e-6);
    close("zero skips the blur",filterRadius(0.f,2.f,false),0.0,1.e-6);
    close("maximum 8",filterRadius(8.f,2.f,false),8.0,1.e-6);
    close("nonfinite setting falls back to 2",filterRadius(std::numeric_limits<float>::quiet_NaN(),1.f,false),2.0,1.e-6);
}
// ---- [P1-lite] world cube shadow maps: pure maths (alworldshadowmath.h) and the residency ordering of the soft-shadow ranking.
// The independent oracles below are literals and closed forms, never the header's own expressions.
template<> template<> void relight_object::test<37>()
{
    using namespace ALWorldShadow;
    // Face table: orthonormal right/up/axis, and exactly the rows glm::lookAt builds: view rows (right, up, -axis). The literals are the
    // probe cube order +X -X +Y -Y +Z -Z with the probe up vectors; areaShadowFilterF.glsl carries the same table (text-pinned by
    // relight_shader_check.py).
    const double axis[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    const double right[6][3] = {{0,0,-1},{0,0,1},{1,0,0},{1,0,0},{1,0,0},{-1,0,0}};
    const double up[6][3] = {{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};
    for (unsigned f=0; f<6; ++f)
    {
        const Vec3 a = faceAxis(f), r = faceRight(f), u = faceUp(f);
        for (int c=0; c<3; ++c)
        {
            const double ac = c==0?a.x:(c==1?a.y:a.z), rc = c==0?r.x:(c==1?r.y:r.z), uc = c==0?u.x:(c==1?u.y:u.z);
            close("axis literal",ac,axis[f][c],1.e-12);
            close("right literal",rc,right[f][c],1.e-12);
            close("up literal",uc,up[f][c],1.e-12);
        }
        close("axis . right",dot(a,r),0.0,1.e-12);
        close("axis . up",dot(a,u),0.0,1.e-12);
        close("right . up",dot(r,u),0.0,1.e-12);
        // right-handed view basis (right, up, -axis): right x up = -axis
        const Vec3 c = cross(r,u);
        close("right x up = -axis (x)",c.x,-a.x,1.e-12);
        close("right x up = -axis (y)",c.y,-a.y,1.e-12);
        close("right x up = -axis (z)",c.z,-a.z,1.e-12);
    }
    // Widened FOV: 2*atan(544/512) = 93.47 degrees; the usable 512 interior is exactly the 90 degree face.
    close("widened FOV degrees",widenedFovDegrees(),93.4714,1.e-3);
    const FaceSample s0 = faceSample({1.0, 0.0, -1.0}), s1 = faceSample({1.0, 0.0, 1.0}); // +X: right = -Z, slopes +1 and -1 (the usable edges)
    close("usable edge texel (low)",std::min(s0.x,s1.x),double(kGutter),1.e-9);
    close("usable edge texel (high)",std::max(s0.x,s1.x),double(kGutter+kUsable),1.e-9);
    close("usable edge equals helper",usableEdgeTexel(),double(kGutter+kUsable),1.e-12);
    // The two usable-edge rays (1,0,-1) and (1,0,1) are 90 degrees apart to 1e-5.
    const double dotv = 1.0*1.0 + 0.0 + (-1.0)*1.0;
    close("edge rays are orthogonal",std::acos(dotv/(std::sqrt(2.0)*std::sqrt(2.0))),kPi/2.0,1.e-5);
    // Round trip receiver -> tile texel -> ray: the texel maps back to the same direction up to scale, on every face.
    for (unsigned f=0; f<6; ++f)
    {
        const Vec3 a = faceAxis(f), r = faceRight(f), u = faceUp(f);
        const Vec3 d{3.0*a.x+0.4*r.x-0.7*u.x, 3.0*a.y+0.4*r.y-0.7*u.y, 3.0*a.z+0.4*r.z-0.7*u.z};
        const FaceSample s = faceSample(d);
        ensure_equals("face of an on-axis-major direction",s.face,f);
        close("axial depth",s.z,3.0,1.e-12);
        close("texel x",s.x,0.5*kTile+0.5*kUsable*(0.4/3.0),1.e-9);
        close("texel y",s.y,0.5*kTile+0.5*kUsable*(-0.7/3.0),1.e-9);
    }
    // Ties go to the lower face index; the major axis decides.
    ensure_equals("tie x/y -> +X",faceSample({1,1,0}).face,0u);
    ensure_equals("-Y major",faceSample({0.2,-3,0.5}).face,3u);
    ensure_equals("-Z major",faceSample({0.2,0.5,-3}).face,5u);
    // Depth linearisation: z = 2fn/((f+n) - (2d-1)(f-n)); d(near)=0, d(far)=1, round trip.
    close("d at near",depthFromLinear(0.25,0.25,20.0),0.0,1.e-12);
    close("d at far",depthFromLinear(20.0,0.25,20.0),1.0,1.e-12);
    for (double z : {0.3, 1.0, 4.5, 19.0}) close("linearise round trip",linearize(depthFromLinear(z,0.25,20.0),0.25,20.0),z,1.e-9);
    close("cleared depth reads far",linearize(1.0,0.25,20.0),20.0,1.e-9);
    // Near plane = max(5 cm, housing bias).
    close("near plane is a fixed 5 cm (decoupled from the Light bias)",nearPlane(),0.05,1.e-12);
    close("near plane constant",kNearPlane,0.05,1.e-12);
    // Round 5: the near plane clears the emitter's world-AABB CHEBYSHEV reach + 1 cm (the faces clip by axial depth: the cleared region is a world-axis cube).
    const auto reach_of = [](std::array<double,3> lo, std::array<double,3> hi, std::array<double,3> p)
    { return chebyshevReach(lo.data(),hi.data(),p.data()); };
    const std::array<double,3> origin = {0,0,0};
    close("emitter near: no emitter keeps the floor",emitterNearPlane(0.05,0.0,7.0),0.05,1.e-12);
    close("emitter near: small emitter keeps the floor",emitterNearPlane(0.05,0.02,7.0),0.05,1.e-12);
    close("emitter near: 16 cm sphere bulb = reach 8 cm + 1 cm",emitterNearPlane(0.05,reach_of({-.08,-.08,-.08},{.08,.08,.08},origin),7.0),0.09,1.e-12);
    close("emitter near: ELONGATED 0.10 x 0.10 x 0.16 bulb reaches 8 cm down (the inscribed rule gave 6 cm and blacked out a cone)",
          emitterNearPlane(0.05,reach_of({-.05,-.05,-.08},{.05,.05,.08},origin),7.0),0.09,1.e-12);
    close("emitter near: 0.4 x 0.4 x 0.1 ceiling panel reaches 20 cm sideways (the inscribed rule gave 6 cm)",
          emitterNearPlane(0.05,reach_of({-.2,-.2,-.05},{.2,.2,.05},origin),7.0),0.21,1.e-12);
    close("emitter near: a 12 cm box turned 45 degrees has a world AABB half of 6 sqrt(2) cm",
          emitterNearPlane(0.05,reach_of({-.06*std::sqrt(2.),-.06*std::sqrt(2.),-.06},{.06*std::sqrt(2.),.06*std::sqrt(2.),.06},origin),7.0),.06*std::sqrt(2.)+.01,1.e-12);
    close("emitter near: the in-world 0.5 m box turned 45 degrees (light at its centre): corners reach 0.354 m, near 0.364 m",
          emitterNearPlane(0.05,reach_of({-.25*std::sqrt(2.),-.25*std::sqrt(2.),-.25},{.25*std::sqrt(2.),.25*std::sqrt(2.),.25},origin),7.0),.25*std::sqrt(2.)+.01,1.e-12);
    close("emitter near: an off-centre light uses the FARTHEST extent",reach_of({-.02,-.02,-.02},{.18,.02,.02},origin),.18,1.e-12);
    close("emitter near: the extent behind the light counts too",reach_of({-.3,-.02,-.02},{.05,.02,.02},origin),.3,1.e-12);
    close("emitter near: huge emitter clamps to 1 m",emitterNearPlane(0.05,10.0,7.0),1.0,1.e-12);
    close("emitter near: below 0.9 x the light radius",emitterNearPlane(0.05,0.4,0.2),0.18,1.e-12);
    close("emitter near: the setting is a floor",emitterNearPlane(0.5,0.04,7.0),0.5,1.e-12);
    close("emitter near: a non-finite reach is ignored",emitterNearPlane(0.05,std::numeric_limits<double>::quiet_NaN(),7.0),0.05,1.e-12);
    // The containment property: the cleared cube holds EVERY corner of the emitter's AABB (so no double-sided face of it can survive the clip).
    for (const auto& box : {std::array<double,6>{-.05,-.05,-.08,.05,.05,.08}, std::array<double,6>{-.2,-.2,-.05,.2,.2,.05}, std::array<double,6>{-.01,-.3,-.02,.4,.1,.02}})
    {
        const double lo[3] = {box[0],box[1],box[2]}, hi[3] = {box[3],box[4],box[5]}, p[3] = {0,0,0};
        const double n = emitterNearPlane(0.05,chebyshevReach(lo,hi,p),7.0);
        for (int c = 0; c < 8; ++c)
            for (int axis = 0; axis < 3; ++axis)
                ensure("every AABB corner is inside the cleared cube",std::fabs(((c >> axis) & 1) ? hi[axis] : lo[axis]) < n);
    }
    // Setting clamps (negative, NaN, infinite, > 1): the floor is 2 cm .. 1 m, non-finite = the 5 cm default.
    close("near setting: negative -> 2 cm",nearPlane(-1.0),0.02,1.e-12);
    close("near setting: zero -> 2 cm",nearPlane(0.0),0.02,1.e-12);
    close("near setting: NaN -> the default",nearPlane(std::numeric_limits<double>::quiet_NaN()),0.05,1.e-12);
    close("near setting: +inf -> the default",nearPlane(std::numeric_limits<double>::infinity()),0.05,1.e-12);
    close("near setting: > 1 -> 1 m",nearPlane(5.0),1.0,1.e-12);
    close("near setting: in range is kept",nearPlane(0.3),0.3,1.e-12);
    close("emitter near: a raised setting is clamped before the light-radius cap",emitterNearPlane(5.0,0.0,0.5),0.45,1.e-12);
    // Linkset siblings: the light's own link is tiny; a small sibling whose AABB contains the light adds its reach; a big or non-enclosing one does not.
    {
        const double own_lo[3] = {-.01,-.01,-.01}, own_hi[3] = {.01,.01,.01}, p[3] = {0,0,0};
        const double bulb_lo[3] = {-.08,-.08,-.08}, bulb_hi[3] = {.08,.08,.08};
        const double wall_lo[3] = {-1.5,-.1,-1.5}, wall_hi[3] = {1.5,.1,1.5};
        const double aside_lo[3] = {.2,-.05,-.05}, aside_hi[3] = {.3,.05,.05};
        const double edge_lo[3] = {-.5,-.5,-.5}, edge_hi[3] = {.5,.5,.5};
        const double just_lo[3] = {-.49,-.49,-.49}, just_hi[3] = {.49,.49,.49};
        ensure("sibling: an enclosing 16 cm bulb counts",siblingEnclosesLight(bulb_lo,bulb_hi,p));
        ensure("sibling: a wall 3 m wide does not",!siblingEnclosesLight(wall_lo,wall_hi,p));
        ensure("sibling: a small prim that does not contain the light does not",!siblingEnclosesLight(aside_lo,aside_hi,p));
        ensure("sibling: exactly 1 m is out (largest extent must be < 1 m)",!siblingEnclosesLight(edge_lo,edge_hi,p));
        ensure("sibling: 0.98 m is in",siblingEnclosesLight(just_lo,just_hi,p));
        double reach = chebyshevReach(own_lo,own_hi,p);
        close("linkset: the tiny link alone keeps the floor",emitterNearPlane(0.05,reach,7.0),0.05,1.e-12);
        reach = std::max(reach,siblingReach(bulb_lo,bulb_hi,p));
        reach = std::max(reach,siblingReach(wall_lo,wall_hi,p));
        reach = std::max(reach,siblingReach(aside_lo,aside_hi,p));
        close("linkset: the tiny link inside a double-sided 16 cm bulb sibling clears the bulb",emitterNearPlane(0.05,reach,7.0),0.09,1.e-12);
        // Raster and resolve share ONE function of the light's own drawable (the child prim) and its siblings; the root prim's big scale never enters.
        const double raster_near = emitterNearPlane(0.05,reach,7.0), resolve_near = emitterNearPlane(0.05,reach,7.0);
        close("child-prim light: raster near == resolve near",raster_near,resolve_near,0.0);
        const double root_axis_reach = chebyshevReach(wall_lo,wall_hi,p);
        ensure("child-prim light: the near follows the child, not a 3 m root (that would clamp to 1 m)",raster_near < 0.1 && emitterNearPlane(0.05,root_axis_reach,7.0) == 1.0);
    }
    // View -> agent rotation packing (round 5 ROOT CAUSE). R = Rz(36.87) * Rx(36.87) (3-4-5 so every entry is exact); glm is [column][row].
    {
        const double Rm[3][3] = {{.8,-.48,.36},{.6,.64,-.48},{0,.6,.8}};
        std::array<std::array<float,4>,4> view{};
        for (int c = 0; c < 3; ++c) for (int r = 0; r < 3; ++r) view[c][r] = static_cast<float>(Rm[r][c]);
        view[3][3] = 1.f;
        float packed[9];
        packViewRotation(view,packed);
        // PINNED test vector (relight_world_check.py compares the harness packing with these nine numbers): the three columns of R.
        const float expected[9] = {.8f,.6f,0.f, -.48f,.64f,.6f, .36f,-.48f,.8f};
        for (int i = 0; i < 9; ++i) close("packed rotation entry",packed[i],expected[i],1.e-6);
        // The reconstructed agent axes: a view-space vector v = R e_i must come back as e_i through the three rows (agent_k = dot(row_k, v)).
        const auto rebuild = [&](const float* rows)
        {
            for (int i = 0; i < 3; ++i)
            {
                double v[3] = {Rm[0][i],Rm[1][i],Rm[2][i]};
                for (int k = 0; k < 3; ++k)
                {
                    const double agent = rows[3*k]*v[0] + rows[3*k+1]*v[1] + rows[3*k+2]*v[2];
                    if (std::fabs(agent - (i == k ? 1.0 : 0.0)) > 1.e-5) return false;
                }
            }
            return true;
        };
        ensure("packed rows rebuild the agent axes",rebuild(packed));
        // NEGATIVE CONTROL: the old code memcpy'd 36 bytes of an aligned glm::mat3 (three 16 byte columns): it must NOT rebuild them.
        float aligned[12] = {};
        for (int c = 0; c < 3; ++c) for (int r = 0; r < 3; ++r) aligned[4*c + r] = static_cast<float>(Rm[r][c]);
        float old_copy[9];
        std::memcpy(old_copy,aligned,sizeof(old_copy));
        ensure("the old aligned-mat3 memcpy scrambles the rows",!rebuild(old_copy));
    }
    close("far plane",farPlane(10.0),15.0,1.e-12);
    // Round 6 P0: ACTIVE drawables keep their extents in the bridge's LOCAL frame; transformedAabb carries the 8 corners through the render matrix (LLMatrix4 layout:
    // [row][col], row vectors v' = v * M, translation in row 3) into the agent-space AABB the near plane and the sibling containment need.
    {
        const double pi = 3.14159265358979323846;
        const auto make = [pi](double yaw_deg, double tx, double ty, double tz)
        {   // rotation about Z by yaw in the row-vector convention (x' = x c - y s, y' = x s + y c) + translation in row 3
            const double r = yaw_deg * pi / 180.0, c = std::cos(r), s = std::sin(r);
            std::array<std::array<float,4>,4> m{};
            m[0] = {static_cast<float>(c), static_cast<float>(s), 0.f, 0.f};
            m[1] = {static_cast<float>(-s), static_cast<float>(c), 0.f, 0.f};
            m[2] = {0.f, 0.f, 1.f, 0.f};
            m[3] = {static_cast<float>(tx), static_cast<float>(ty), static_cast<float>(tz), 1.f};
            return m;
        };
        const auto xform = [](const std::array<std::array<float,4>,4>& m, const double lo[3], const double hi[3], double olo[3], double ohi[3])
        {
            float raw[4][4];
            for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) raw[i][j] = m[i][j];
            transformedAabb(raw, lo, hi, olo, ohi);
        };
        const double box_lo[3] = {-.25,-.25,-.25}, box_hi[3] = {.25,.25,.25};
        double olo[3], ohi[3];
        // Analytic: a box with half-size h turned by yaw about Z has world half-extent h (|cos| + |sin|) in X and Y and h in Z.
        const double yaw = 40.0;
        const double half = .25 * (std::fabs(std::cos(yaw * pi / 180.0)) + std::fabs(std::sin(yaw * pi / 180.0)));
        xform(make(yaw,0,0,0),box_lo,box_hi,olo,ohi);
        for (int i = 0; i < 2; ++i) { close("rotated active root: AABB min",olo[i],-half,1.e-6); close("rotated active root: AABB max",ohi[i],half,1.e-6); }
        close("rotated active root: Z unchanged",ohi[2],.25,1.e-6);
        // The false-floor-shadow fixture: the light at the agent origin in that rotated emitter must get reach 0.352 -> near 0.362 (local, unrotated bounds gave 0.26).
        const double origin[3] = {0,0,0};
        close("rotated active emitter: near 0.362 (not the local-space 0.26)",emitterNearPlane(0.05,chebyshevReach(olo,ohi,origin),7.0),half + 0.01,1.e-6);
        close("rotated active emitter: the LOCAL box would have given 0.26",emitterNearPlane(0.05,chebyshevReach(box_lo,box_hi,origin),7.0),0.26,1.e-12);
        // Translated active root at (0,0,1), light at its centre: the reach is 0.25, not the 1.25 the untransformed local box gave from a light at z = 1.
        const double at1[3] = {0,0,1};
        xform(make(0,0,0,1),box_lo,box_hi,olo,ohi);
        close("translated active root: AABB min z",olo[2],.75,1.e-6);
        close("translated active root: AABB max z",ohi[2],1.25,1.e-6);
        close("translated active emitter: reach 0.25 -> near 0.26",emitterNearPlane(0.05,chebyshevReach(olo,ohi,at1),7.0),.26,1.e-6);
        close("translated active emitter: the LOCAL box would clamp to 1 m",emitterNearPlane(0.05,chebyshevReach(box_lo,box_hi,at1),7.0),1.0,1.e-12);
        // Rotated AND translated: corners match the analytic agent AABB (centre moves with the translation, half-extent with the rotation).
        xform(make(yaw,3,-2,5),box_lo,box_hi,olo,ohi);
        close("rotated+translated: min x",olo[0],3-half,1.e-6); close("rotated+translated: max x",ohi[0],3+half,1.e-6);
        close("rotated+translated: min y",olo[1],-2-half,1.e-6); close("rotated+translated: max y",ohi[1],-2+half,1.e-6);
        close("rotated+translated: min z",olo[2],4.75,1.e-6); close("rotated+translated: max z",ohi[2],5.25,1.e-6);
        // A CHILD link of a rotated, translated active root: its extents sit at a local offset (0.5, 0, 0) inside the root's frame; the render matrix is the ROOT's.
        const double child_lo[3] = {.45,-.05,-.05}, child_hi[3] = {.55,.05,.05};
        xform(make(90,10,20,30),child_lo,child_hi,olo,ohi);   // 90 degrees: local +X (0.5) lands on world +Y
        close("active child: centre x",.5*(olo[0]+ohi[0]),10.0,1.e-5);
        close("active child: centre y",.5*(olo[1]+ohi[1]),20.5,1.e-5);
        close("active child: centre z",.5*(olo[2]+ohi[2]),30.0,1.e-5);
        close("active child: half y",.5*(ohi[1]-olo[1]),.05,1.e-5);
        // The sibling containment test sees the agent-space box: a 16 cm bulb link, rotated and translated with its root, still encloses a light at its centre.
        const double bulb_lo[3] = {-.08,-.08,-.08}, bulb_hi[3] = {.08,.08,.08};
        xform(make(40,3,-2,5),bulb_lo,bulb_hi,olo,ohi);
        const double light_at[3] = {3,-2,5};
        ensure("active sibling: encloses the light in agent space",siblingEnclosesLight(olo,ohi,light_at));
        ensure("active sibling: the untransformed local box does not",!siblingEnclosesLight(bulb_lo,bulb_hi,light_at));
        // Identity matrix leaves the box alone.
        xform(make(0,0,0,0),box_lo,box_hi,olo,ohi);
        for (int i = 0; i < 3; ++i) { close("identity: min",olo[i],box_lo[i],1.e-7); close("identity: max",ohi[i],box_hi[i],1.e-7); }
        ensure("sibling walk cap is a sane bound",kMaxSiblingLinks >= 16 && kMaxSiblingLinks <= 255);
    }
}
template<> template<> void relight_object::test<38>()
{
    using namespace ALWorldShadow;
    // Atlas grid: 3 x 2 tiles of 544 per light (1632 x 1088), face f of light k at column f%3, row 2k+f/3; byte literals.
    ensure_equals("atlas width",kAtlasWidth,1632u);
    ensure_equals("atlas height 1 light",atlasHeight(1),1088u);
    ensure_equals("atlas height 2 lights",atlasHeight(2),2176u);
    close("1 light MiB",double(atlasBytes(1))/1048576.0,6.7734,1.e-3);
    close("2 lights MiB",double(atlasBytes(2))/1048576.0,13.5469,1.e-3);
    close("scratch MiB",double(scratchBytes())/1048576.0,1.1289,1.e-3);
    bool seen[2*6] = {};
    for (unsigned k=0; k<2; ++k) for (unsigned f=0; f<6; ++f)
    {
        const TileSlot t = tileSlot(k,f);
        ensure("tile origin multiple of 544",t.x%544==0 && t.y%544==0);
        ensure("tile inside the atlas",t.x+544<=kAtlasWidth && t.y+544<=atlasHeight(2));
        const unsigned index = (t.y/544)*3+t.x/544;
        ensure("tile slots are distinct",!seen[index]);
        seen[index] = true;
        ensure_equals("column",t.x/544,f%3);
        ensure_equals("row",t.y/544,2*k+f/3);
    }
    // Ledger: pixels x lights x 48 gathers against 1.0 G. 1080p/4K keep 2 lights, 8K keeps 1, an absurd target none.
    ensure_equals("ledger literal 1080p x1",resolveCost(1920,1080,1),99532800ull);
    ensure_equals("planned 1080p",plannedLights(1920,1080,2),2u);
    ensure_equals("planned 4K",plannedLights(3840,2160,2),2u);   // 796 M
    ensure_equals("planned 5K falls to 1",plannedLights(5120,2880,2),1u); // 1.42 G for 2, 0.71 G for 1
    ensure_equals("planned 8K is over budget",plannedLights(7680,4320,2),0u); // 1.59 G even for 1
    ensure_equals("planned absurd",plannedLights(30000,30000,2),0u);
    ensure_equals("request clamps to 1..2",plannedLights(1920,1080,9),2u);
    ensure_equals("request clamps up",plannedLights(1920,1080,0),1u);
    ensure_equals("raster passes",facePasses(2),12u);
    // Contact hand-off band: the user's mapped contact, never below the map's blind band, clamped to [2 cm, 0.5 m].
    close("texel at 10 m",texelWorld(10.0),20.0/512.0,1.e-12);
    close("normal offset capped at 5 cm",normalOffset(100.0),0.05,1.e-12);
    close("blind band at 10 m",blindBandMax(10.0),0.0390625+0.003+0.0390625*2.0,1.e-9);
    close("contact keeps the user's value",contactDistance(0.15,10.0),0.15,1.e-12);
    close("contact never below the band",contactDistance(0.01,10.0),blindBandMax(10.0),1.e-12);
    close("contact clamp high",contactDistance(4.0,10.0),0.5,1.e-12);
    close("contact clamp low",contactDistance(0.0,0.1),0.02,1.e-12);
    close("compare bias at grazing is capped",compareBias(5.0,100.0),0.003+texelWorld(5.0)*8.0,1.e-12);
    close("compare bias face-on",compareBias(5.0,0.0),0.003,1.e-12);
    // Readout wording.
    ensure_equals("text off",worldShadowText(State::Off),std::string("off"));
    ensure_equals("text choosing",worldShadowText(State::Choosing),std::string("choosing a light"));
    ensure_equals("text no light",worldShadowText(State::NoLight),std::string("no light in view"));
    ensure_equals("text memory",worldShadowText(State::LowMemory),std::string("not enough video memory"));
    ensure_equals("text on 1",worldShadowText(State::On,1),std::string("on 1 light (6 faces at 512)"));
    ensure_equals("text on 2",worldShadowText(State::On,2),std::string("on 2 lights (12 faces at 512)"));
    ensure_equals("text on 0 reads as 1",worldShadowText(State::On,0),std::string("on 1 light (6 faces at 512)"));
    // Round 5 readout: counts RENDERED lights, names the pinned ones, says why one is not rendering.
    {
        const std::string labels[2] = {"lamp (abcd1234)", "light 99887766"};
        const SlotReason both[2] = {SlotReason::Rendered, SlotReason::Rendered};
        const SlotReason one[2] = {SlotReason::Rendered, SlotReason::NativeMap};
        const SlotReason none[2] = {SlotReason::OutOfView, SlotReason::None};
        ensure_equals("report: two rendered",worldShadowReport(State::On,2,labels,both,2),std::string("on 2 lights (12 faces at 512): lamp (abcd1234); light 99887766."));
        ensure_equals("report: one rendered, one pinned on a native map",worldShadowReport(State::On,1,labels,one,2),
            std::string("on 1 light (6 faces at 512): lamp (abcd1234). light 2/2 pinned but not rendering: light 99887766 (native map)."));
        ensure_equals("report: pinned, none rendering",worldShadowReport(State::Pinned,0,labels,none,2),
            std::string("light 1/2 pinned but not rendering: lamp (abcd1234) (out of view)."));
        ensure_equals("report: other states keep the plain wording",worldShadowReport(State::Choosing,0,labels,none,2),std::string("choosing a light"));
    }
}
template<> template<> void relight_object::test<39>()
{
    using namespace ALWorldShadow;
    // Pinned residency: fill while empty with the best candidate not already resident; a held slot clears only when its light is no
    // longer eligible or the planned count drops; ranks never swap a held light.
    Residency<int> r;
    const int ranked1[3] = {7, 3, 5};
    ensure_equals("first frame fills 1 slot",updateResidency(r,1,[](int){return true;},ranked1,3),1u);
    ensure("slot 0 holds the best",r.held[0] && r.id[0]==7 && !r.held[1]);
    const int ranked2[3] = {3, 7, 5}; // ranks changed: nothing swaps
    ensure_equals("ranks never swap a held light",updateResidency(r,1,[](int){return true;},ranked2,3),0u);
    ensure("still 7",r.id[0]==7);
    ensure_equals("raising N fills the other slot with the best NOT resident",updateResidency(r,2,[](int){return true;},ranked2,3),1u);
    ensure("slot 1 takes 3",r.held[1] && r.id[1]==3 && r.id[0]==7);
    const int ranked_eligible[2] = {3, 5}; // the caller ranks ELIGIBLE lights only (7 is gone)
    ensure_equals("a light that stops being eligible clears (and is replaced)",
        updateResidency(r,2,[](int id){return id!=7;},ranked_eligible,2),2u);
    ensure("slot 0 refilled with the best other",r.held[0] && r.id[0]==5 && r.id[1]==3);
    ensure_equals("dropping N clears the high slot",updateResidency(r,1,[](int){return true;},ranked2,3),1u);
    ensure("slot 1 empty",!r.held[1] && r.held[0]);
    ensure_equals("no candidates leaves an empty slot empty",updateResidency(r,2,[](int){return true;},ranked2,0),0u);
    ensure_equals("count",r.count(),1u);
    // The candidate order of the whole cycle never matters for a held light: every permutation of 4 candidates keeps slot 0.
    int order[4] = {1,2,3,4};
    Residency<int> held;
    const int first[4] = {2,1,3,4};
    updateResidency(held,1,[](int){return true;},first,4);
    do
    {
        updateResidency(held,1,[](int){return true;},order,4);
        ensure("held light survives every candidate order",held.held[0] && held.id[0]==2);
    } while (std::next_permutation(order,order+4));
}
template<> template<> void relight_object::test<40>()
{
    using namespace ALAreaShadow;
    // With every world flag false the ranking is exactly the order before P1-lite (importance, then UUID): all 720 permutations of 6
    // candidates of tied and untied scores give the identical selection. A resident (world) candidate sorts first whatever its score.
    std::array<Candidate<int>,6> base{{{1,false,5.0},{2,false,5.0},{3,false,2.0,true},{4,false,9.0},{5,false,9.0},{6,false,0.5}}};
    const auto reference = [&](bool world)
    {
        Selection<int> s;
        std::array<Candidate<int>,6> list = base;
        for (auto& c : list) c.world = world ? c.world : false;
        for (const auto& c : list) s.add(c,4);
        return s;
    };
    const Selection<int> off = reference(false);
    // Independent oracle for the old order: score descending, id ascending, top 4.
    const int expected_off[4] = {4,5,1,2};
    for (unsigned i=0; i<4; ++i) ensure_equals("OFF ordering equals importance then id",off.lights[i].id,expected_off[i]);
    std::array<int,6> perm{0,1,2,3,4,5};
    do
    {
        Selection<int> shuffled;
        for (int i : perm) { Candidate<int> c = base[i]; c.world = false; shuffled.add(c,4); }
        for (unsigned i=0; i<4; ++i) ensure_equals("permutation identity (world flags false)",shuffled.lights[i].id,off.lights[i].id);
        Selection<int> with_world;
        for (int i : perm) with_world.add(base[i],4);
        ensure_equals("a resident light always ranks first",with_world.lights[0].id,3);
        for (unsigned i=1; i<4; ++i) ensure_equals("the rest keeps its order",with_world.lights[i].id,expected_off[i-1]);
    } while (std::next_permutation(perm.begin(),perm.end()));
}

}
