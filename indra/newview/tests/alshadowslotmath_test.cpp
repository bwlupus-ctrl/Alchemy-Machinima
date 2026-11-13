#include "linden_common.h"
#include "../test/lltut.h"
#include "../alshadowslotmath.h"
#include "../alareashadowmath.h"
#include "../alworldshadowmath.h"
#include <limits>

namespace tut
{
struct shadow_slot_data {};
using shadow_slot_group = test_group<shadow_slot_data>;
using shadow_slot_object = shadow_slot_group::object;
shadow_slot_group shadow_slot_tests("alshadowslotmath");

template<> template<> void shadow_slot_object::test<1>()
{
    for (unsigned fps : {30u, 60u})
    {
        ALShadowSlot::Challenge<int> state;
        for (unsigned i = 0; i < fps * 3; ++i)
            ensure("1.2x never steals", ALShadowSlot::retain(state, true, true, 1, 2,
                100.0, 120.0, 1, 1, double(i) / fps, i));
        state = {};
        for (unsigned i = 0; i < fps / 5; ++i)
            ensure("2x must hold at either frame rate", ALShadowSlot::retain(state, true, true,
                1, 2, 100.0, 200.0, 1, 1, double(i) / fps, i));
        ensure("2x wins after 0.2 seconds", !ALShadowSlot::retain(state, true, true,
            1, 2, 100.0, 200.0, 1, 1, 0.201, fps / 5));
        ensure("invalid owner releases immediately", !ALShadowSlot::retain(state, true, false,
            1, 2, 100.0, 120.0, 1, 1, 0.202, fps / 5 + 1));
    }
}

template<> template<> void shadow_slot_object::test<2>()
{
    ALShadowSlot::Challenge<int> s;
    const auto keep = [&](int challenger, double score, double now, unsigned frame)
    { return ALShadowSlot::retain(s, true, true, 1, challenger, 100.0, score, 1, 1, now, frame); };
    ensure("challenge starts", keep(2,200,0,1));
    ensure("margin dip resets", keep(2,120,0.05,2));
    ensure("recovery starts over", keep(2,200,0.1,3));
    ensure("new challenger starts over", keep(3,200,0.15,4));
    ensure("frame gap breaks continuous challenge", keep(3,200,0.2,6));
    ensure("same-frame repeat cannot shortcut hold", keep(3,200,0.25,6));
    ensure("hitch cannot complete hold", keep(3,200,0.5,7));
    ensure("long pause resets hold", keep(3,200,3.0,8));
    for (unsigned i=1;i<=4;++i) ensure("fresh continuous hold",keep(3,200,3.0+i*0.04,8+i));
    ensure("continuous winner eventually steals", !keep(3,200,3.201,13));
    ensure("higher categorical tier remains immediate", !ALShadowSlot::retain(s,true,true,1,2,100,1,1,2,4,14));
    ensure("lower categorical tier cannot steal", ALShadowSlot::retain(s,true,true,1,2,1,1000,2,1,5,15));
}

template<> template<> void shadow_slot_object::test<3>()
{
    ALShadowSlot::Challenge<int> s;
    ensure("on retains", ALShadowSlot::retain(s, true, true, 1, 2, 100.0, 120.0, 1, 1, 0.0, 1));
    ensure("off bypasses retention", !ALShadowSlot::retain(s, false, true, 1, 2, 100.0, 120.0, 1, 1, 1.0, 2));
    ensure("off clears state", !s.hold.tracking);
    ensure_equals("off preserves old velocity fade bit for bit", ALShadowSlot::fadeStep(8.0, 1.0 / 60.0, false), 8.0);
    for (unsigned fps : {30u, 60u})
    {
        double fade = 1.0;
        unsigned frames = 0;
        while (fade > 0.0)
        { fade -= ALShadowSlot::fadeStep(100.0, 1.0 / fps, true); ++frames; }
        ensure("fast dolly full fade takes at least 0.25 seconds", double(frames) / fps >= 0.25);
    }
    ensure_equals("slow camera existing fade unchanged", ALShadowSlot::fadeStep(0.01, 0.01, true), 0.01);
    ensure("bit classification catches NaN under fp:fast", !ALShadowSlot::finite(std::numeric_limits<double>::quiet_NaN()));
    ensure("bit classification catches infinity", !ALShadowSlot::finite(std::numeric_limits<double>::infinity()));
}

template<> template<> void shadow_slot_object::test<4>()
{
    const auto winner = [](double challenger, bool enabled)
    {
        ALAreaShadow::Selection<int> ranked;
        const auto incumbent = [](int id) { return id == 1; };
        ranked.addIncumbent({1, false, 100.0}, 1, incumbent, enabled);
        ranked.addIncumbent({2, false, challenger}, 1, incumbent, enabled);
        return ranked.lights[0].id;
    };
    ensure_equals("near-equal incumbent keeps channel", winner(120.0, true), 1);
    ensure_equals("clear winner still wins", winner(140.0, true), 2);
    ensure_equals("off restores old winner", winner(120.0, false), 2);
    // Independent original ranking oracle over every collector permutation, including ties and residents.
    std::array<ALAreaShadow::Candidate<int>, 5> inputs{{
        {1, false, 100.0, false}, {2, true, 110.0, false}, {3, false, 100.0, false},
        {4, false, 1.0, true}, {5, true, 125.0, false}}};
    std::array<int, 5> order{{0, 1, 2, 3, 4}};
    do
    {
        ALAreaShadow::Selection<int> original, off;
        for (int i : order)
        {
            auto c = inputs[i];
            original.add(c, 4, [](const auto& a, const auto& b)
            { return a.world != b.world ? a.world : (a.score != b.score ? a.score > b.score : a.id < b.id); });
            c.score = ALAreaShadow::incumbentScore(c.score, c.id % 2 == 0, false);
            off.add(c, 4);
        }
        ensure_equals("off count unchanged", off.size, original.size);
        for (unsigned i = 0; i < original.size; ++i)
        {
            ensure_equals("off exact old order", off.lights[i].id, original.lights[i].id);
            ensure_equals("off exact old score", off.lights[i].score, original.lights[i].score);
        }
    } while (std::next_permutation(order.begin(), order.end()));
}

template<> template<> void shadow_slot_object::test<5>()
{
    ALAreaShadow::ChannelMemory<int> m;
    m.ids[0]=7; m.count=1; m.frame=10; m.time=1.0;
    ensure("same-frame selection",m.contains(7,10,1.0));
    ensure("previous main selection",m.contains(7,11,1.02));
    ensure("six skipped frames survive at 30 Hz",m.contains(7,16,1.2));
    ensure("short time gap survives more than six fast frames",m.contains(7,18,1.09));
    ensure("many quick frames preserve history",m.contains(7,17,1.21));
    ensure("one long frame preserves incumbency",m.contains(7,11,3.0));
    ensure("six missed frames survive a long wall gap",m.contains(7,16,3.0));
    ensure("both new gap bounds exceeded expires",!m.contains(7,17,3.0));
    ensure("unknown light never incumbent",!m.contains(8,11,1.02));
    m={}; ensure("resize reset removes incumbency",!m.contains(7,11,1.02));
}

template<> template<> void shadow_slot_object::test<6>()
{
    for (unsigned fps : {30u, 60u})
    {
        ALWorldShadow::NativeRelease<int> r;
        for (unsigned i = 0; i < fps; ++i)
        {
            ensure("native flicker keeps resident cube pin", r.eligible(1, true, false, true, double(i) / fps, i));
            ensure("both upkeep sites share one real-time hold", r.eligible(1, true, false, true, double(i) / fps, i));
        }
        ensure("one continuous second releases pin", !r.eligible(1, true, false, true, 1.001, fps));
        ensure("two missed samples reset debounce", r.eligible(1, false, false, true, 1.04, fps + 2));
        ensure("new native stint starts over", r.eligible(1, true, false, true, 1.06, fps + 3));
        ensure("picked resident never releases for native", r.eligible(1, true, true, true, 3.0, fps + 3));
        ensure("changed resident gets its own hold", r.eligible(2, true, false, true, 4.0, fps + 4));
    }
}

template<> template<> void shadow_slot_object::test<7>()
{
    ALWorldShadow::NativeRelease<int> r;
    for (bool native : {false, true})
        for (bool picked : {false, true})
            ensure_equals("off exactly old residency eligibility", r.eligible(1, native, picked, false, 0.0, 1),
                ALWorldShadow::worldEligibleWithNative(native, picked));
    ALWorldShadow::Residency<int> resident;
    resident.id[0] = 1; resident.held[0] = true;
    ALWorldShadow::updateResidency(resident, 1, [](int) { return false; }, static_cast<const int*>(nullptr), 0);
    ensure("invalid light immediately releases regardless of native grace", !resident.held[0]);
    ensure("initial native hold",r.eligible(1,true,false,true,0.0,1));
    ensure("missing main frame restarts hold",r.eligible(1,true,false,true,2.0,3));
    ensure("hitch cannot finish release",r.eligible(1,true,false,true,2.3,4));
    ensure("long pause restarts release",r.eligible(1,true,false,true,4.0,5));
    for (unsigned i=1;i<60;++i)
        ensure("release waits after gap",r.eligible(1,true,false,true,4.0+double(i)/60,5+i));
    ensure("continuous second then releases",!r.eligible(1,true,false,true,5.001,65));
}
template<> template<> void shadow_slot_object::test<8>()
{
    using Light = ALShadowSlot::Light<int>;
    std::array<Light, 2> owners{{{1, 100.0, 1}, {}}};
    std::array<Light, 2> ranked{{{3, 300.0, 1}, {2, 200.0, 1}}};
    std::array<ALShadowSlot::Challenge<int>, 2> states{};
    auto chosen = ALShadowSlot::retainOwners(owners, ranked, 2, states, 0.0, 1);
    ensure_equals("empty slot immediately gets strongest challenger", chosen[1], 3);
    ensure_equals("remaining challenger must hold against owner", chosen[0], 1);
    for (unsigned i=1;i<=5;++i) chosen = ALShadowSlot::retainOwners(owners, ranked, 2, states, double(i)*0.042, 1+i);
    ensure_equals("second challenger wins after hold", chosen[0], 2);
    ensure_equals("vacancy winner remains unique", chosen[1], 3);
    owners = {{{1, 150.0, 1}, {2, 100.0, 1}}};
    ranked = {{{3, 200.0, 1}, {4, 50.0, 1}}};
    states = {};
    ALShadowSlot::retainOwners(owners, ranked, 2, states, 1.0, 3);
    for (unsigned i=1;i<=5;++i) chosen = ALShadowSlot::retainOwners(owners, ranked, 2, states, 1.0+double(i)*0.042, 3+i);
    ensure_equals("weakest owner is challenged first", chosen[1], 3);
    ensure_equals("stronger owner survives", chosen[0], 1);
    owners[0] = {};
    chosen = ALShadowSlot::retainOwners(owners, ranked, 2, states, 1.22, 5);
    ensure_equals("invalid owner vacates immediately without hold", chosen[0], 3);
    ensure("reconciliation never assigns duplicate targets", chosen[0] != chosen[1]);
    chosen = ALShadowSlot::retainOwners(owners, ranked, 1, states, 1.23, 6);
    ensure_equals("lowered cap clears excess target", chosen[1], 0);
}
template<> template<> void shadow_slot_object::test<9>()
{
    ALAreaShadow::Selection<int> on, off;
    const auto incumbent = [](int id) { return id == 1; };
    for (const auto c : {ALAreaShadow::Candidate<int>{1,false,102.0}, {2,false,100.0}})
    { on.addIncumbent(c,2,incumbent,true); off.addIncumbent(c,2,incumbent,false); }
    for (unsigned i = 0; i < 2; ++i)
    {
        ensure_equals("unchanged order", on.lights[i].id, off.lights[i].id);
        ensure_equals("stored score remains raw", on.lights[i].score, off.lights[i].score);
    }
    ensure_equals("retained admission uses its ordering key without rewriting raw scores",
        ALAreaShadow::incumbentAdmittedStrength(on.lights[0].score,on.lights[1].score,true,true),1.0);
    ensure_equals("ordering boost never alters regular share",
        ALAreaShadow::regularShare(on.lights[0].score,on.lights[1].score),
        ALAreaShadow::regularShare(off.lights[0].score,off.lights[1].score));
    on = {};
    on.addIncumbent({1,false,10000.0,false},1,incumbent,true);
    on.addIncumbent({2,false,1.0,true},1,incumbent,true);
    ensure_equals("resident precedence survives huge incumbent score",on.lights[0].id,2);
}

template<> template<> void shadow_slot_object::test<10>()
{
    ALShadowSlot::ChannelRamps<int> r;
    const auto live=[](int id) { return ALShadowSlot::RampStatus{id!=0,false,false}; };
    std::array<int,8> desired{{1}};
    auto out=r.update(desired,1,1,true,true,0.0,1,live);
    ensure_equals("hard-cut first selection is settled",out.fade[0],1.0);
    desired[0]=2;
    out=r.update(desired,1,1,true,true,0.05,2,live);
    ensure_equals("temporary exit channel",out.size,2u);
    ensure_equals("outgoing starts full",out.fade[0],1.0);
    ensure("outgoing marked",out.exiting[0]);
    ensure_equals("real newcomer starts zero",out.fade[1],0.0);
    for(unsigned i=1;i<=5;++i) out=r.update(desired,1,1,true,true,0.05+double(i)*0.025,2+i,live);
    ensure("linear exit midpoint",out.fade[0]>0.499 && out.fade[0]<0.501);
    ensure("linear entry midpoint",out.fade[1]>0.499 && out.fade[1]<0.501);
    for(unsigned i=6;i<=11;++i) out=r.update(desired,1,1,true,true,0.05+double(i)*0.025,2+i,live);
    ensure_equals("exit relinquishes spare",out.size,1u);
    ensure_equals("newcomer remains",out.ids[0],2);
    ensure_equals("newcomer reaches full",out.fade[0],1.0);
    out=r.update(desired,1,1,true,true,0.35,14,[](int) {return ALShadowSlot::RampStatus{};});
    ensure_equals("invalid drops immediately",out.size,0u);
    out=r.update(desired,1,1,true,false,0.36,15,live);
    ensure_equals("off keeps desired",out.ids[0],2);
    ensure_equals("off identity",out.fade[0],1.0);
    ensure("off clears history",!r.initialized);
}

template<> template<> void shadow_slot_object::test<11>()
{
    ALShadowSlot::ChannelRamps<int> r;
    const auto live = [](int) { return ALShadowSlot::RampStatus{true,false,false}; };
    std::array<int,8> desired{{1,2,3,4,5,6,7,8}};
    r.update(desired,8,8,false,true,0.0,1,live);
    desired[0]=9;
    auto out=r.update(desired,8,8,false,true,0.05,2,live);
    ensure_equals("hard maximum never exceeded",out.size,8u);
    ensure("cap-eight admits ranked newcomer immediately",std::find(out.ids.begin(),out.ids.end(),9)!=out.ids.end());
    ensure("capacity-full finishes outgoing",std::find(out.ids.begin(),out.ids.end(),1)==out.ids.end());
    r={}; desired={{1}};
    r.update(desired,1,1,false,true,0.0,1,live);
    desired[0]=2;
    out=r.update(desired,1,1,false,true,0.05,2,live);
    ensure_equals("budget-rejected spare skips outgoing ramp",out.size,1u);
    ensure_equals("normal-budget newcomer retained",out.ids[0],2);
    ensure_equals("budget rejection swaps symmetrically at full strength",out.fade[0],1.0);
    auto w=ALAreaShadow::workload({8,12,true},100,100,1,1,1,ALAreaShadow::FeedSet{});
    const auto original=w;
    ensure("extra work cannot exceed available allowance",!ALAreaShadow::rampWorkload(w,2,original.iterations));
    ensure_equals("failed expansion leaves original plan intact",w.iterations,original.iterations);
    ensure("extra work allowed when exact quality fits",ALAreaShadow::rampWorkload(w,2,original.iterations*2));
    ensure_equals("no ray downgrade for ramp",w.q.samples,original.q.samples);
    ensure_equals("no step downgrade for ramp",w.q.steps,original.q.steps);
    ensure_equals("no resolution downgrade for ramp",w.q.half,original.q.half);
    auto ladder = original;
    ladder.admitted = ladder.lead = 2;
    ensure("ladder admission at same quality allows ramp",ALAreaShadow::rampPlanMatches(original,ladder,2));
    ladder.admitted = 1;
    ensure("ladder light rejection forbids spare",!ALAreaShadow::rampPlanMatches(original,ladder,2));
    ladder.admitted = 2; --ladder.q.samples;
    ensure("ladder quality downgrade forbids spare",!ALAreaShadow::rampPlanMatches(original,ladder,2));
}

template<> template<> void shadow_slot_object::test<12>()
{
    ensure_equals("incumbent has wider frustum sphere",ALAreaShadow::candidateRadius(4.0,true,true),8.0);
    ensure_equals("non-incumbent retains old sphere",ALAreaShadow::candidateRadius(4.0,false,true),6.0);
    ensure_equals("off retains old sphere",ALAreaShadow::candidateRadius(4.0,true,false),6.0);
    ensure("grazing incumbent survives",ALAreaShadow::candidateRadius(4.0,true,true)>7.0);
    ensure("same grazing newcomer stays excluded",ALAreaShadow::candidateRadius(4.0,false,true)<7.0);
}

template<> template<> void shadow_slot_object::test<13>()
{
    ALShadowSlot::Challenge<int> state;
    for (unsigned i=0;i<60;++i)
        ensure("alternating challengers never pool hold time",ALShadowSlot::retain(state,true,true,
            1,2+int(i%2),100.0,200.0,1,1,double(i)*0.05,i));
    ensure("new continuous challenger starts its own hold",ALShadowSlot::retain(state,true,true,1,2,100.0,200.0,1,1,3.0,60));
    for(unsigned i=1;i<=4;++i) ensure("same challenger holds independently",ALShadowSlot::retain(state,true,true,1,2,100.0,200.0,1,1,3.0+i*0.04,60+i));
    ensure("same challenger finally wins after its own hold",!ALShadowSlot::retain(state,true,true,1,2,100.0,200.0,1,1,3.21,65));
    std::array<int,3> order{{0,1,2}};
    const std::array<ALShadowSlot::Light<int>,3> collected{{{3,200,1},{4,240,1},{5,120,1}}};
    const std::array<ALShadowSlot::Light<int>,2> owners{{{1,100,1},{2,180,1}}};
    do
    {
        // Reproduce the stock numeric insertion auction without any incumbent margin.
        std::array<ALShadowSlot::Light<int>,2> raw{};
        for (int index:order)
        {
            auto potential=collected[index];
            for (auto& target:raw)
                if (potential.priority>target.priority) std::swap(target,potential);
        }
        std::array<ALShadowSlot::Challenge<int>,2> holds{};
        auto chosen=ALShadowSlot::retainOwners(owners,raw,2,holds,0.0,1);
        ensure_equals("collector order does not invent an incumbent",chosen[0],1);
        for(unsigned i=1;i<=5;++i) chosen=ALShadowSlot::retainOwners(owners,raw,2,holds,i*0.042,1+i);
        ensure_equals("challenger compared against real weak owner",chosen[0],4);
        ensure_equals("displaced accumulator gains no margin",chosen[1],2);
    } while(std::next_permutation(order.begin(),order.end()));
}

template<> template<> void shadow_slot_object::test<14>()
{
    ALShadowSlot::ChannelRamps<int> r;
    std::array<int,8> desired{{1}};
    ALShadowSlot::RampStatus status{true,false,false,true};
    const auto live=[&](int) {return status;};
    r.update(desired,1,1,true,true,0.0,1,live);
    status.cubePending=true;
    auto out=r.update(desired,1,1,true,true,0.01,2,live);
    ensure_equals("settled light acquiring pin keeps its screen shadow",out.fade[0],1.0);
    ensure("pending pin is tracked",r.entries[0].pending);
    for(unsigned i=1;i<=20;++i)
    {
        out=r.update(desired,1,1,true,true,0.01+i*0.001,2+i,live);
        ensure_equals("never-rendering settled pin never dims",out.fade[0],1.0);
    }
    ensure("pending clears at six frames",!r.entries[0].pending);
    r={}; status={true,false,false,true};
    r.update(desired,1,1,true,true,0.4,25,live);
    status.cubePending=true;
    r.update(desired,1,1,true,true,0.41,26,live);
    status.cubePending=false; status.cubeReady=true;
    r.update(desired,1,1,true,true,0.42,27,live);
    ensure("first rendered cube clears pending immediately",!r.entries[0].pending);
    r={}; status={true,false,false,true};
    r.update(desired,1,1,true,true,1.0,30,live);
    status.cubePending=true;
    r.update(desired,1,1,true,true,1.01,31,live);
    r.update(desired,1,1,true,true,1.111,32,live);
    ensure("pending clears after 0.1 seconds independently of frame count",!r.entries[0].pending);
    r={}; status={true,false,false,true};
    r.update(desired,1,1,true,true,2.0,40,live);
    status.cubePending=true; status.cubeRendered=false;
    out=r.update(desired,1,1,true,true,2.01,41,live);
    ensure("non-Rendered slot reason clears pending immediately",!r.entries[0].pending);
    ensure_equals("failed cube keeps settled screen fallback",out.fade[0],1.0);
    for(unsigned i=1;i<=20;++i)
    {
        status.cubeReady=i%2==0; status.cubePending=!status.cubeReady;
        out=r.update(desired,1,1,true,true,2.01+i*0.02,41+i,live);
        ensure_equals("alternating cube readiness stays settled",out.fade[0],1.0);
    }
    r={}; status={true,true,false,true};
    out=r.update(desired,1,1,true,true,3.0,70,live);
    ensure_equals("teleport pending cube is settled",out.fade[0],1.0);
    // A real paired channel entry still ramps, even if its cube NEVER renders.
    desired[0]=2;
    out=r.update(desired,1,1,true,true,3.02,71,live);
    ensure_equals("paired entry starts zero",out.fade[1],0.0);
    for(unsigned i=1;i<=14;++i)
    {
        out=r.update(desired,1,1,true,true,3.02+i*0.025,71+i,live);
        if(i==1) ensure("pending cube never stalls an entry ramp",out.fade[1]>0.09);
    }
    ensure_equals("never-rendering pin cannot hold entry below full",out.size,1u);
    ensure_equals("screen fallback completes entry regardless of pending",out.fade[0],1.0);
}

template<> template<> void shadow_slot_object::test<15>()
{
    ALShadowSlot::ChannelRamps<int> r;
    const auto live=[](int) {return ALShadowSlot::RampStatus{true,false,false};};
    std::array<int,8> desired{{1,2}};
    r.update(desired,2,2,false,true,0.0,1,live);
    std::array<int,8> rendered{{1}};
    r.retainRendered(rendered,1);
    ensure_equals("undrawn entry is forgotten",r.entries[1].id,0);
    auto out=r.update(desired,2,2,false,true,0.05,2,live);
    ensure_equals("surviving plane remains settled",out.fade[0],1.0);
    ensure_equals("unpaired recovery is settled",out.fade[1],1.0);
    r.retainRendered(rendered,0);
    out=r.update(desired,2,2,false,true,0.1,3,live);
    ensure_equals("full allocation recovery seeds settled",out.fade[0],1.0);
    desired[0]=3;
    out=r.update(desired,2,2,true,true,0.12,4,live);
    ensure_equals("real swap enters at zero",out.fade[out.size-1],0.0);
    out=r.update(desired,2,2,true,true,0.32,10,live);
    ensure_equals("bounded skipped passes preserve incumbent history",out.ids[0],1);
    ensure("hitch cannot complete entry or exit",out.fade[out.size-1]<0.267 && out.fade[0]>0.733);
    const auto before_stall=out;
    ALAreaShadow::ChannelMemory<int> memory;
    memory.ids[0]=1; memory.count=1; memory.frame=10; memory.time=0.32;
    memory.strength[0]=out.fade[0];
    ensure("0.45s single-frame stall keeps selection history",memory.contains(1,11,0.77));
    ensure("0.45s single-frame stall keeps ramp incumbency",r.contains(1,11,0.77));
    out=r.update(desired,2,2,true,true,0.77,11,live);
    ensure_equals("single long frame preserves channel count",out.size,before_stall.size);
    for(unsigned i=0;i<out.size;++i)
        ensure_equals("single long frame preserves UUIDs",out.ids[i],before_stall.ids[i]);
    ensure("exit continues with bounded step",std::abs(out.fade[0]-(before_stall.fade[0]-4.0/15.0))<1e-12);
    ensure("entry continues with bounded step",std::abs(out.fade[out.size-1]-(before_stall.fade[out.size-1]+4.0/15.0))<1e-12);
    out=r.update(desired,2,2,true,true,1.3,18,live);
    ensure_equals("long multi-frame absence drops the exit",out.size,2u);
    for(unsigned i=0;i<out.size;++i) ensure_equals("both gap bounds exceeded re-seeds at full",out.fade[i],1.0);
}


template<> template<> void shadow_slot_object::test<16>()
{
    const double visibility=0.2, admission=0.5, fade=0.5;
    // Gather admission has its old meaning; ONLY final resolve applies the exit.
    const double gather=1.0-(1.0-visibility)*admission;
    const double resolved=1.0-(1.0-gather)*fade;
    ensure("composed exit is linear, not squared",std::abs(resolved-0.8)<1e-12);
    for(unsigned count=0;count<=8;++count)
        ensure_equals("only an already drawn plane can have a spare",ALShadowSlot::spareChannel(count),
            count>0 && count<8 && count!=4);
    const auto live=[](int) {return ALShadowSlot::RampStatus{true,false,false};};
    for(unsigned count : {4u,8u})
    {
        ALShadowSlot::ChannelRamps<int> r;
        std::array<int,8> desired{{1,2,3,4,5,6,7,8}};
        r.update(desired,count,count,true,true,0.0,1,live);
        desired[0]=9;
        const auto out=r.update(desired,count,count,true,true,0.02,2,live);
        ensure_equals("ramp cannot add another plane even if caller allows it",out.size,count);
        for(unsigned i=0;i<out.size;++i) ensure_equals("full-plane swap has no coverage hole",out.fade[i],1.0);
    }
}

template<> template<> void shadow_slot_object::test<17>()
{
    ALWorldShadow::NativeRelease<int> r;
    for(unsigned i=0;i<60;++i)
    {
        const bool sampled=i%3!=1;
        ensure("single missed frames preserve release window",r.eligible(1,sampled,false,true,i/60.0,i));
        ensure("second upkeep never double-counts",r.eligible(1,sampled,false,true,i/60.0,i));
    }
    ensure("native grace releases despite isolated misses",!r.eligible(1,true,false,true,1.001,60));
    r={};
    r.eligible(1,true,false,true,0.0,1);
    r.eligible(1,false,false,true,0.02,2);
    ensure("two consecutive misses reset release",r.eligible(1,false,false,true,0.04,3));
    ensure("hitch cannot release pin",r.eligible(1,true,false,true,0.3,4));
    r={};
    for(unsigned i=0;i<=48;++i)
        ensure("sampled stint before double miss",r.eligible(1,true,false,true,i/60.0,i));
    ensure("first missed sample keeps stint",r.eligible(1,false,false,true,49.0/60,49));
    ensure("second missed sample clears stint",r.eligible(1,false,false,true,50.0/60,50));
    for(unsigned i=51;i<=65;++i)
        ensure("two misses require a fresh second, not pooled time",r.eligible(1,true,false,true,i/60.0,i));
    r={};
    for(unsigned i=0;i<=5;++i)
        ensure("repeated release hitches cannot pool unclamped wall time",r.eligible(1,true,false,true,i*0.3,i));
    ensure("hold step bounded",ALShadowSlot::timeStep(0.3)<=1.0/15.0);
    ensure("native fade hitch bounded",ALShadowSlot::fadeStep(100,0.3,true)<=4.0/15.0);
}

template<> template<> void shadow_slot_object::test<18>()
{
    ALShadowSlot::ChannelRamps<int> r;
    r.initialized=true; r.time=1; r.frame=10;
    r.entries[0]={1,0.8,false,false,false};
    r.entries[1]={2,0.2,false,false,false};
    r.entries[2]={3,0.5,false,false,false};
    std::array<int,8> desired{{4,5}};
    auto out=r.update(desired,2,2,true,true,1.01,11,
        [](int) {return ALShadowSlot::RampStatus{true,false,false};});
    ensure_equals("full exiting capacity admits ranked light immediately",out.size,3u);
    ensure("lowest-fade exit finishes first",std::find(out.ids.begin(),out.ids.end(),2)==out.ids.end());
    ensure("stronger outgoing kept",std::find(out.ids.begin(),out.ids.end(),1)!=out.ids.end());
    ensure("ranked newcomer present",std::find(out.ids.begin(),out.ids.end(),4)!=out.ids.end());
    for(unsigned i=0;i<out.size;++i)
        if(out.ids[i]==4 || out.ids[i]==5) ensure_equals("forced cuts seed incoming at full",out.fade[i],1.0);
}
template<> template<> void shadow_slot_object::test<19>()
{
    const auto prior=ALAreaShadow::incumbentAdmittedStrength(100,0,true,true);
    ensure_equals("1.2x appearing challenger does not halve retained strength",
        ALAreaShadow::incumbentAdmittedStrength(100,120,true,true),prior);
    ensure_equals("non-incumbent strength unchanged",ALAreaShadow::incumbentAdmittedStrength(100,120,false,true),
        ALAreaShadow::admittedStrength(100,120));
    ensure_equals("OFF strength unchanged",ALAreaShadow::incumbentAdmittedStrength(100,120,true,false),
        ALAreaShadow::admittedStrength(100,120));
    bool seed=true;
    ensure("reset frame bypasses reconciliation even with stale auction stamp",!ALShadowSlot::retainNativeFrame(true,false,seed));
    ensure("stock bypass consumes only the first frame",!seed);
    ensure("following frame resumes retention",ALShadowSlot::retainNativeFrame(true,false,seed));
    ensure("OFF bypass remains exact",!ALShadowSlot::retainNativeFrame(false,false,seed));
    ensure("stable spots unchanged",!ALShadowSlot::retainNativeFrame(true,true,seed));
}

template<> template<> void shadow_slot_object::test<20>()
{
    ALShadowSlot::ChannelRamps<int> r;
    const auto live=[](int) {return ALShadowSlot::RampStatus{true,false,false};};
    std::array<int,8> desired{{1}};
    r.update(desired,1,1,true,true,0.0,1,live);
    desired[0]=2;
    auto out=r.update(desired,1,1,true,true,0.02,2,live);
    ensure_equals("paired swap has spare exit",out.size,2u);
    ensure_equals("paired entry starts zero",out.fade[1],0.0);
    out=r.update(desired,1,1,false,true,0.04,3,live);
    ensure_equals("budget loss cuts exit",out.size,1u);
    ensure_equals("budget loss settles incoming symmetrically",out.fade[0],1.0);
    desired[1]=3;
    out=r.update(desired,2,2,true,true,0.06,4,live);
    ensure_equals("new unpaired light is full immediately",out.fade[1],1.0);
}
template<> template<> void shadow_slot_object::test<21>()
{
    // Keep one settled light while two OTHER lights swap. A residency defect must
    // not be masked by the symmetric fallback used when no exit is present.
    ALShadowSlot::ChannelRamps<int> r;
    ALShadowSlot::RampStatus pin{true,false,false,true};
    const auto live=[&](int id) {return id==1 ? pin : ALShadowSlot::RampStatus{true,false,false};};
    std::array<int,8> desired{{1,3}};
    r.update(desired,2,2,true,true,0.0,1,live);
    desired[1]=2;
    r.update(desired,2,2,true,true,0.02,2,live);
    pin.cubePending=true;
    for(unsigned frame=3;frame<10;++frame)
    {
        const auto out=r.update(desired,2,2,true,true,0.02+(frame-2)*0.01,frame,live);
        ensure_equals("other outgoing shadow retains its channel",out.size,3u);
        for(unsigned i=0;i<out.size;++i)
            if(out.ids[i]==1) ensure_equals("pin transition preserves settled fade during another swap",out.fade[i],1.0);
    }
}

template<> template<> void shadow_slot_object::test<22>()
{
    // Exact auction crossover: incumbent 0.8R, challenger through its 1.3x key.
    ALAreaShadow::ChannelMemory<int> memory;
    ALShadowSlot::ChannelRamps<int> state;
    double previous = 1.0;
    unsigned swap = 0;
    double exit_start = 0.0;
    for (unsigned frame=1; frame<=28; ++frame)
    {
        const double now = (frame-1)/60.0;
        const auto prior = memory;
        const auto incumbent = [&](int id) { return prior.contains(id,frame,now); };
        ALAreaShadow::Selection<int> ranked;
        const double challenger = frame<=7 ? 1.0+(frame-1)*0.01 : 1.06;
        ranked.addIncumbent({1,false,0.8},2,incumbent,true);
        if(frame>1) ranked.addIncumbent({2,false,challenger},2,incumbent,true);
        std::array<int,8> desired{{ranked.lights[0].id}};
        const auto ramps=state.update(desired,1,1,true,true,now,frame,[&](int id)
        { return ALShadowSlot::RampStatus{true,false,false,true,prior.effectiveStrength(id,frame,now)}; });
        const double boundary=ALAreaShadow::droppedScore(ranked,1,ramps);
        const auto selected=ALAreaShadow::rampSelection(ranked,1,ramps,[&](int id)
        { for(unsigned i=0;i<ranked.size;++i) if(ranked.lights[i].id==id) return ranked.lights[i]; return ALAreaShadow::Candidate<int>{}; });
        memory.count=selected.size; memory.frame=frame; memory.time=now;
        double strength=0.0;
        for(unsigned i=0;i<selected.size;++i)
            for(unsigned j=0;j<ramps.size;++j) if(selected.lights[i].id==ramps.ids[j])
            {
                const double admission=ramps.exiting[j] ? 1.0 : ALAreaShadow::incumbentAdmittedStrength(
                    selected.lights[i].score,boundary,incumbent(selected.lights[i].id),true);
                memory.ids[i]=selected.lights[i].id; memory.strength[i]=admission*ramps.fade[j];
                if(selected.lights[i].id==1) strength=memory.strength[i];
            }
        if(frame==6) ensure("just before crossover admission approaches half",previous<0.51);
        if(desired[0]==2 && !swap)
        {
            swap=frame; exit_start=strength;
            ensure_equals("exit is skipped as the dropped admission boundary",boundary,0.0);
            ensure("swap preserves the previous half-strength shadow",std::abs(strength-previous)<1e-12);
        }
        if(swap) ensure("outgoing effective shadow never increases",strength<=previous+1e-12);
        if(swap && frame<swap+15)
            ensure("partial exit keeps its full quarter-second curve",std::abs(strength-exit_start*(1.0-(frame-swap)/15.0))<1e-12);
        if(swap && frame>=swap+15) ensure_equals("outgoing reaches zero within 0.25s",strength,0.0);
        if(swap && frame<swap+15)
            for(unsigned i=0;i<memory.count;++i) if(memory.ids[i]==2)
                ensure("paired entry keeps its quarter-second ramp",std::abs(memory.strength[i]-(frame-swap)/15.0)<1e-12);
        previous=strength;
    }
    ensure("fixture crosses the incumbency key",swap>0);
}

template<> template<> void shadow_slot_object::test<23>()
{
    ALAreaShadow::Selection<int> desired;
    desired.lights[0]={2,false,100}; desired.lights[1]={3,false,90}; desired.lights[2]={4,false,60};
    desired.size=3; desired.candidates=4;
    ALShadowSlot::ChannelRamps<int>::Output ramps;
    ramps.ids={{1,3,2}}; ramps.exiting={{true,false,false}}; ramps.size=3;
    const auto selected=ALAreaShadow::rampSelection(desired,2,ramps,
        [](int id) {return ALAreaShadow::Candidate<int>{id,false,200,true};});
    const unsigned lead=1;
    ensure_equals("Thin lead stays first despite stronger resident exit",selected.lights[0].id,2);
    ensure_equals("desired regular boundary stays first regular",selected.lights[lead].id,3);
    ensure_equals("exit appended after every desired light",selected.lights[2].id,1);
    for(unsigned i=0;i<selected.size;++i)
        if(selected.lights[i].id==1) ensure("exit cannot occupy Thin lead",i>=lead);
    ensure_equals("regular mix preserved",ALAreaShadow::regularShare(selected.lights[0].score,selected.lights[lead].score),
        ALAreaShadow::regularShare(desired.lights[0].score,desired.lights[lead].score));
    ensure_equals("drop scan skips exit but finds next real dropped light",ALAreaShadow::droppedScore(desired,2,ramps),60.0);
    desired.lights[2].id=1;
    desired.lights[3]={4,false,40}; desired.size=4;
    ensure_equals("scan continues past exit to first real dropped light",ALAreaShadow::droppedScore(desired,2,ramps),40.0);
    desired.size=3;
    ensure_equals("only dropped exit means no admission boundary",ALAreaShadow::droppedScore(desired,2,ramps),0.0);
    ALAreaShadow::ChannelMemory<int> memory;
    memory.ids[0]=2; memory.strength[0]=0.37; memory.count=1; memory.frame=4; memory.time=1.0;
    ensure_equals("strength remembered by UUID",memory.effectiveStrength(2,5,1.01),0.37);
    ensure_equals("absent strength is settled",memory.effectiveStrength(3,5,1.01),1.0);
    ensure_equals("expired strength is settled",memory.effectiveStrength(2,40,2.0),1.0);
}

template<> template<> void shadow_slot_object::test<24>()
{
    // Selected membership confers the admission key immediately, even after a hard reset.
    ALAreaShadow::ChannelMemory<int> history;
    ALShadowSlot::ChannelRamps<int> ramps;
    const std::array<int,8> desired{{9}};
    const auto live=[](int) {return ALShadowSlot::RampStatus{true,false,false};};
    double previous=0.0;
    for(unsigned frame=1;frame<=4;++frame)
    {
        if(frame==3) {history={};ramps={};} // resetShadowSlotHistory's channel fields.
        const auto out=ramps.update(desired,1,1,false,true,frame/60.0,frame,live);
        ensure_equals("unpaired admission is settled after reset",out.fade[0],1.0);
        const double admission=ALAreaShadow::incumbentAdmittedStrength(131,127,true,true);
        ensure_equals("first admitted frame gets its full ordering key",admission,1.0);
        if(frame==2 || frame==4) ensure_equals("admission unchanged at N and N+1",admission,previous);
        history.ids[0]=9;history.count=1;history.frame=frame;history.time=frame/60.0;
        previous=admission;
    }
    ensure_equals("OFF admission still uses raw score",ALAreaShadow::incumbentAdmittedStrength(131,127,true,false),
        ALAreaShadow::admittedStrength(131,127));
}

template<> template<> void shadow_slot_object::test<25>()
{
    const auto live=[](int) {return ALShadowSlot::RampStatus{true,false,false};};
    for(unsigned kept : {1u,2u})
    {
        ensure_equals("spare uses only stock retained capacity",ALShadowSlot::spareChannel(4,kept),kept==2);
        ALShadowSlot::ChannelRamps<int> r;
        std::array<int,8> desired{{1,2,3,4}};
        r.update(desired,4,4,true,true,0.0,1,live,kept);
        desired[3]=5;
        auto out=r.update(desired,4,4,true,true,0.02,2,live,kept);
        ensure_equals("cap4 symmetric or retained-plane crossfade",out.size,kept==2 ? 5u : 4u);
        for(unsigned i=0;i<out.size;++i)
            if(out.ids[i]==5) ensure_equals("newcomer follows retained-plane policy",out.fade[i],kept==2 ? 0.0 : 1.0);
        out=r.update(desired,4,4,false,true,0.04,3,live,kept);
        ensure_equals("retained plane still needs budget admission",out.size,4u);
        for(unsigned i=0;i<out.size;++i) ensure_equals("budget loss remains symmetric",out.fade[i],1.0);
    }
    ensure("even retained planes cannot exceed eight channels",!ALShadowSlot::spareChannel(8,2));
}

template<> template<> void shadow_slot_object::test<26>()
{
    ALShadowSlot::ChannelRamps<int> r;
    bool culled=false;
    const auto live=[&](int id) {return ALShadowSlot::RampStatus{!culled || id!=1,false,false};};
    std::array<int,8> desired{{1}};
    r.update(desired,1,1,true,true,0.0,1,live);
    desired[0]=2;
    r.update(desired,1,1,true,true,0.02,2,live);
    auto out=r.update(desired,1,1,true,true,0.07,3,live);
    ensure("paired entry is partway through ramp",out.fade[1]>0.19 && out.fade[1]<0.21);
    culled=true;
    for(unsigned frame=4;frame<=8;++frame)
    {
        const double now=0.07+(frame-3)*0.04;
        out=r.update(desired,1,1,true,true,now,frame,live);
        ensure_equals("culled exit is removed immediately",out.size,1u);
        ensure_equals("entry remains selected",out.ids[0],2);
        const double expected=std::min((now-0.02)*4.0,1.0);
        ensure("entry finishes its own ramp after eligibility loss",std::abs(out.fade[0]-expected)<1e-12);
    }
    ensure_equals("entry completes at 0.25s",out.fade[0],1.0);
}

// Native projector slots are independent of the eight area-shadow channels.
template<> template<> void shadow_slot_object::test<27>()
{
    std::array<ALShadowSlot::Light<int>,16> owners{}, ranked{};
    std::array<ALShadowSlot::Challenge<int>,16> states{};
    for(unsigned i=0;i<16;++i)
        owners[i]=ranked[i]={static_cast<int>(i+1),100.0-i,1};
    auto chosen=ALShadowSlot::retainOwners(owners,ranked,16,states,0.0,1);
    for(unsigned i=0;i<16;++i) ensure_equals("all sixteen owners retained",chosen[i],static_cast<int>(i+1));
    owners[15]={}; ranked[15]={17,120.0,1};
    chosen=ALShadowSlot::retainOwners(owners,ranked,16,states,0.02,2);
    ensure_equals("sixteenth vacancy filled without eviction",chosen[15],17);
    for(unsigned i=0;i<15;++i) ensure_equals("earlier owners unchanged",chosen[i],static_cast<int>(i+1));
}

}
