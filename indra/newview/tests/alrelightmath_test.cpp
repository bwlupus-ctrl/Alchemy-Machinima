#include "linden_common.h"
#include "../test/lltut.h"
#include "../alrelightmath.h"
#include "../alareashadowmath.h"
#include "../alrelighttakerecord.h"
#include "../alrelighttakewriter.h"
#include "llsdserialize.h"
#include <limits>
#include <future>
namespace tut
{
struct relight_data {};
using relight_group = test_group<relight_data>;
using relight_object = relight_group::object;
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
    const std::array<Candidate<int>,5> input{{{9,true,1000}, {8,false,1},
        {3,false,5}, {2,false,5}, {1,true,9999}}};
    std::array<int,5> order{{0,1,2,3,4}};
    do
    {
        Selection<int> selected;
        for (int index : order) selected.add(input[index],3);
        ensure_equals("cap",selected.size,3u);
        ensure_equals("all candidates counted",selected.candidates,5u);
        ensure_equals("contribution then UUID",selected.lights[0].id,2);
        ensure_equals("tie by UUID",selected.lights[1].id,3);
        ensure_equals("leaking beats shadow map",selected.lights[2].id,8);
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
    close("literal projected contribution",contribution(4,2,3,.5),3);
    close("nonfinite contribution rejected",contribution(1,1,std::numeric_limits<double>::infinity(),1),0);
    close("negative contribution rejected",contribution(1,1,-1,1),0);
}
template<> template<> void relight_object::test<12>()
{
    using namespace ALAreaShadow;
    const auto low=quality(0), medium=quality(1), high=quality(2), cinema=quality(3);
    ensure("low",low.samples==4 && low.steps==8 && low.half);
    ensure("default medium",medium.samples==8 && medium.steps==12 && medium.half);
    ensure("high",high.samples==12 && high.steps==20 && high.half);
    ensure("cinematic",cinema.samples==24 && cinema.steps==32 && !cinema.half);
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
    for (unsigned height : {1080u,1440u,2160u})
    for (unsigned lights : {4u,8u})
    for (int preset=0; preset<4; ++preset)
    for (int budget : {1,100,1500,9999})
    {
        const auto a=workload(quality(preset),height*16/9,height,lights,budget,lights);
        const auto b=workload(quality(preset),height*16/9,height,lights,budget,lights);
        ensure("deterministic plan",a.q.samples==b.q.samples && a.q.steps==b.q.steps &&
            a.q.half==b.q.half && a.channels==b.channels && a.iterations==b.iterations);
        ensure("hard frame ceiling",a.iterations<=a.budget && a.budget<=1500000000ull);
        if (a.channels)
        {
            ensure_equals("literal total",a.iterations,a.pixels*lights*a.q.samples*a.q.steps);
            unsigned dispatched=0;
            for (unsigned plane=0; plane<(lights+3)/4; ++plane)
                for (unsigned first=0; first<std::min(4u,lights-plane*4); first+=a.channels)
                {
                    const auto count=std::min(a.channels,std::min(4u,lights-plane*4)-first);
                    ensure("every split draw bounded",a.pixels*count*a.q.samples*a.q.steps<=a.drawCap);
                    dispatched+=count;
                }
            ensure_equals("all channels exactly once",dispatched,lights);
        }
        else ensure_equals("rejected views do no work",a.iterations,std::uint64_t(0));
    }
    const auto samples=workload({32,64,false},1920,1080,8,1500,8);
    ensure("reduce samples first",samples.q.samples==1 && samples.q.steps==64 && !samples.q.half);
    const auto steps=workload({32,64,false},3840,2160,8,1500,8);
    ensure("then steps",steps.q.samples==1 && steps.q.steps==22 && !steps.q.half);
    const auto half=workload({32,64,false},3840,2160,8,20,8);
    ensure("then half resolution",half.q.samples==1 && half.q.steps==1 && half.q.half && half.channels>0);
    const auto split=workload({1,64,false},1920,1080,4,600,4);
    ensure("split necessary",split.channels==2 && split.q.steps==64);
    ensure("default 4K below ceiling",workload(quality(1),3840,2160,8,1500,8).iterations==1393459200ull);
    ensure("impossible minimum fails closed",!workload({1,1,true},3840,2160,8,1,8).channels);
    ensure("oversized input fails closed",!workload({32,64,false},0xffffffffu,0xffffffffu,8,1500,8).channels);
    ensure("empty selection",!workload(quality(1),1920,1080,0,1500,0).channels);
}

template<> template<> void relight_object::test<15>()
{
    using namespace ALAreaShadow;
    const auto seven=workload(quality(3),1920,1080,7,1500,8);
    const auto eight=workload(quality(3),1920,1080,8,1500,8);
    ensure("Cinematic K7/K8 same per-light work",seven.q.samples==2 &&
        seven.q.samples==eight.q.samples && seven.q.steps==32 &&
        seven.q.steps==eight.q.steps && seven.q.half==eight.q.half);
    for (unsigned height : {1080u,1440u,2160u})
    for (unsigned requested : {1u,4u,8u})
    for (int preset=0; preset<4; ++preset)
    for (int budget : {1,100,1500})
    {
        const auto full=workload(quality(preset),height*16/9,height,requested,budget,requested);
        for (unsigned visible=1; visible<=requested; ++visible)
        {
            const auto w=workload(quality(preset),height*16/9,height,visible,budget,requested);
            ensure("visibility never retunes quality",w.q.samples==full.q.samples &&
                w.q.steps==full.q.steps && w.q.half==full.q.half && w.channels==full.channels);
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
            // Mapped UUIDs are LOWER: sorting by UUID alone must fail this test.
            for (int i : order) selected.add({ids[i],i<2,1.0},cap);
            ensure_equals("real UUID tied cap",selected.size,cap);
            for (unsigned i=0; i<cap; ++i)
                ensure_equals("unmapped first, UUID ties independent of input order",selected.lights[i].id,ids[(i+2)%6]);
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
    for (const auto& light : {ALAreaShadow::Candidate<LLUUID>{c,true,100.},
         ALAreaShadow::Candidate<LLUUID>{b,false,1.}, ALAreaShadow::Candidate<LLUUID>{a,false,1.}})
        state.selected.add(light,8);
    state.planned = true;
    state.work = ALAreaShadow::workload(ALAreaShadow::quality(1),1920,1080,3,1500,8);
    LLSD settings;
    settings["RenderAreaShadows"] = true;
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
        ensure_equals("channel UUID",view["selected"][i]["uuid"].asUUID(),std::array<LLUUID,3>{{a,b,c}}[i]);
        ensure_equals("native map membership",view["selected"][i]["shadow_map"].asBoolean(),i==2);
    }
    ensure_equals("sampled slot at area pass",view["projector_slots"][0]["uuid"].asUUID(),c);
    ensure_equals("vacant slot retained",view["projector_slots"][1]["uuid"].asUUID(),LLUUID::null);
    ensure_equals("current sampled slot",record["projector_slots_at_start"][0]["uuid"].asUUID(),b);
    close("current slot fade",record["projector_slots_at_start"][0]["fade"].asReal(),.5);
    const LLSD& work = view["work"];
    ensure_equals("effective samples",work["samples"].asInteger(),8);
    ensure_equals("effective steps",work["steps"].asInteger(),12);
    ensure("half resolution",work["half"].asBoolean());
    ensure_equals("channels per draw",work["channels_per_draw"].asInteger(),4);
    ensure_equals("literal pixels",work["pixels"].asString(),std::string("518400"));
    ensure_equals("literal iterations",work["iterations"].asString(),std::string("149299200"));
    ensure_equals("literal budget",work["budget"].asString(),std::string("1500000000"));
    ensure_equals("literal draw cap",work["draw_cap"].asString(),std::string("750000000"));
    std::cout << "[RelightQ] synthetic take-record " << LLSDNotationStreamer(record) << '\n';
    // Log serialization must preserve the complete record, including UUID types.
    std::stringstream stream;
    LLSDSerialize::toNotation(record,stream);
    LLSD decoded;
    ensure("notation parses",LLSDSerialize::fromNotation(decoded,stream,stream.str().size())>0);
    ensure_equals("serialized UUID",decoded["area_view"]["selected"][0]["uuid"].asUUID(),a);
    ensure_equals("serialized exact work",decoded["area_view"]["work"]["iterations"].asString(),std::string("149299200"));
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
    // A Gate may render one source repeatedly; bound actual passes, not IDs.
    ALAreaShadow::FeedPasses passes;
    for (auto frame : {0u,1u,0xffffffffu,0u})
    {
        for (unsigned pass=0; pass<24; ++pass)
            ensure("actual feed pass cap",passes.admit(frame,8)==(pass<8));
    }
    ALAreaShadow::FeedPasses none;
    ensure("zero feed slots",!none.admit(1,0));
    // Main's plan never depends on feeds. Fixed eight-way spare allocation
    // remains safe even if all capture slots render in a future scheduler.
    for (int preset=0; preset<4; ++preset)
    for (int budget : {1,100,1500})
    for (unsigned mainWidth : {1280u,1920u,3840u})
    for (unsigned cap : {1u,4u,8u})
    {
        const auto q = ALAreaShadow::quality(preset);
        const auto main = ALAreaShadow::workload(q,mainWidth,mainWidth*9/16,cap,budget,cap);
        for (unsigned feeds : {1u,2u,4u,8u})
        {
            std::uint64_t total = main.channels ? main.iterations : 0;
            for (unsigned slot=0; slot<feeds; ++slot)
            {
                const auto feed = ALAreaShadow::feedWorkload(q,mainWidth,mainWidth*9/16,
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
    const auto rays = ALAreaShadow::workloadWithin(full,10,10,1,2400,1);
    ensure("feed rays first",rays.q.samples==1 && rays.q.steps==8 && !rays.q.half);
    const auto steps = ALAreaShadow::workloadWithin(full,10,10,1,800,1);
    ensure("feed steps second",steps.q.samples==1 && steps.q.steps==4 && !steps.q.half);
    const auto half = ALAreaShadow::workloadWithin(full,10,10,1,100,1);
    ensure("feed half third",half.q.samples==1 && half.q.steps==1 && half.q.half && half.channels);
    ensure("feed skip last",!ALAreaShadow::workloadWithin(full,10,10,1,1,1).channels);
    ensure("no main extent skips feeds",!ALAreaShadow::feedWorkload(full,0,0,10,10,1,1500,1,8).channels);
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

}
