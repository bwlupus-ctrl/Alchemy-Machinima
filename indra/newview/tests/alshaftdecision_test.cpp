#include "linden_common.h"
#include "../test/lltut.h"
#include "../alshaftdecision.h"
#include "../alshadowslotmath.h"
#include "../alshaftstatusconsumer.h"

namespace tut
{
struct shaft_decision_data {};
using shaft_decision_group = test_group<shaft_decision_data>;
using shaft_decision_object = shaft_decision_group::object;
shaft_decision_group shaft_decision_tests("alshaftdecision");

template<> template<> void shaft_decision_object::test<1>()
{
    using namespace ALShaftDecision;
    const int flagged = tier(false, false, promoted(true, false, false, false, false));
    ensure("flagged world beats larger world", outranks(true, flagged, false, 1, tier(false,false,false), 10000));
    ensure("world cannot steal flagged", !outranks(true, World, false, 10000, flagged, 1));
    ensure("rig still beats flagged", outranks(true, tier(true,false,true), true, 0, flagged, 10000));
    ensure("flagged cannot steal rig", !outranks(true, flagged, false, 10000, Rig, 0));
    ensure("actor-only rig is lowest", !outranks(true, tier(true,true,true), true, 10000, World, 1));
    ensure("world beats actor rig", outranks(true, World, false, 1, ActorRig, 10000));
}
template<> template<> void shaft_decision_object::test<2>()
{
    using namespace ALShaftDecision;
    ensure("Cast Shadows opt-out never promoted", !promoted(true,false,true,false,true));
    ensure("VCam auto-enable unchanged", !promoted(true,true,false,false,false));
    ensure("auto child shadows explicit root", !promoted(true,true,true,false,false));
    ensure("explicit child shadows auto root", promoted(true,false,true,true,false));
    ensure("root fallback promotes", promoted(false,false,true,false,false));
    ensure("root auto not promoted", !promoted(false,false,true,true,false));
    ensure("unflagged unchanged", !promoted(false,false,false,false,false));
}
template<> template<> void shaft_decision_object::test<3>()
{
    using namespace ALShaftDecision;
    ensure("positive world fills empty slot", outranks(false, World, false, 1, Rig, 100));
    ensure("zero world cannot fill empty slot", !outranks(false, World, false, 0, World, 0));
    ensure("promotion preserves empty-slot numeric floor", !outranks(false, FlaggedWorld, false, 0, World, 0));
    ensure("zero rig still fills empty slot", outranks(false, Rig, true, 0, World, 0));
    ensure("zero actor rig still fills empty slot", outranks(false, ActorRig, true, 0, World, 0));
    ensure("equal-tier larger first", outranks(true, FlaggedWorld, false, 101, FlaggedWorld, 100));
    ensure("equal sizes never swap", !outranks(true, FlaggedWorld, false, 100, FlaggedWorld, 100));
}
template<> template<> void shaft_decision_object::test<4>()
{
    using namespace ALShaftDecision;
    std::array<ALShadowSlot::Light<int>,2> owners{{{1,100,FlaggedWorld},{2,90,FlaggedWorld}}};
    std::array<ALShadowSlot::Challenge<int>,2> states{};
    for (unsigned frame=0; frame<180; ++frame)
    {
        const bool crosses = frame%2;
        const std::array<ALShadowSlot::Light<int>,2> ranked = crosses
            ? std::array<ALShadowSlot::Light<int>,2>{{{2,101,FlaggedWorld},{1,100,FlaggedWorld}}} : owners;
        const auto chosen = ALShadowSlot::retainOwners(owners, ranked, 2, states, frame/60., frame);
        ensure_equals("two flagged retain slot identity through size crossover", chosen[0], 1);
        ensure_equals("second flagged does not thrash", chosen[1], 2);
    }
    std::array<ALShadowSlot::Light<int>,2> ranked{{{3,1,Rig},{2,90,FlaggedWorld}}};
    auto chosen = ALShadowSlot::retainOwners(owners,ranked,2,states,4.,181);
    ensure_equals("rig eviction bypasses hold",chosen[0],3);
    owners={{{1,100,World},{2,90,ActorRig}}};
    ranked={{{3,1,FlaggedWorld},{1,100,World}}};
    chosen=ALShadowSlot::retainOwners(owners,ranked,2,states,4.1,182);
    ensure_equals("flagged evicts weakest actor first",chosen[1],3);
    owners={{{1,100,FlaggedWorld},{}}};
    ranked={{{2,200,FlaggedWorld},{1,100,FlaggedWorld}}};
    chosen=ALShadowSlot::retainOwners(owners,ranked,2,states,4.2,183);
    ensure_equals("vacancy filled before owner challenged",chosen[1],2);
    ensure_equals("existing owner stays put with spare slot",chosen[0],1);
}
template<> template<> void shaft_decision_object::test<5>()
{
    using namespace ALShaftDecision;
    ensure("class3 live program ready", shaderReady(true,3));
    ensure("class1 black stub rejected", !shaderReady(true,1));
    ensure("unloaded shader rejected", !shaderReady(false,3));
    ensure("failed permutation rejected", !shaderReady(true,3,true));
}
template<> template<> void shaft_decision_object::test<6>()
{
    using namespace ALShaftDecision;
    // Enable every earlier gate one at a time. Later blockers must never hide it.
    Status s;
    auto check=[&](Reason expected) { ensure("first blocking reason wins",reason(s)==expected); };
    check(Reason::NotProjector); s.projector=true;
    check(Reason::NotFlagged); s.flagged=true;
    check(Reason::DeferredOff); s.deferred=true;
    check(Reason::ShadowsOff); s.shadows=true;
    s.actorOnly=true; s.suppressed=true;
    check(Reason::ActorOnly); s.actorOnly=false;
    check(Reason::Suppressed); s.suppressed=false; s.light=false; s.shaft=false;
    check(Reason::ZeroLight); s.light=true;
    s.froxel=true; s.drawn=true;
    check(Reason::Froxel); s.froxel=false;
    check(Reason::MasterOff); s.master=true;
    check(Reason::ShaderUnavailable); s.shader=true;
    check(Reason::ZeroShaft); s.shaft=true;
    check(Reason::Drawing); s.drawn=false; s.waiting=true; s.occupied=true;
    check(Reason::Waiting); s.waiting=false;
    check(Reason::Occupied); s.occupied=false;
    check(Reason::Filtered);
}
template<> template<> void shaft_decision_object::test<7>()
{
    using namespace ALShaftDecision;
    Status s;
    s.projector = s.flagged = s.deferred = s.shadows = s.froxel = true;
    ensure("confirmed froxel draw survives per-cone master off", reason(s) == Reason::Froxel);
    s.froxel = false;
    ensure("per-cone route still needs its master", reason(s) == Reason::MasterOff);
}

template<> template<> void shaft_decision_object::test<8>()
{
    using namespace ALShaftDecision;
    ProbeLifetime state;
    ensure("live probe cannot retire", !state.canDestroy());
    {
        ProbeLifetime::Lease observe(state);
        state.done = true; // second click, panel destruction, teleport, reload, resize
        ensure("cancel cannot delete observer's probe", !state.canDestroy());
        {
            ProbeLifetime::Lease timer(state);
            ensure("nested timer callback cannot delete probe", !state.canDestroy());
        }
        ensure("timer exit keeps outer observer alive", !state.canDestroy());
    }
    ensure("sole owner may delete only after all callbacks leave", state.canDestroy());
}

template<> template<> void shaft_decision_object::test<9>()
{
    using namespace ALShaftDecision;
    ensure("hidden and idle means zero observation", !observe(false, false));
    ensure("visible status consumes observations", observe(true, false));
    ensure("self-test observes without status widget", observe(false, true));
    using namespace ALShaftStatus;
    ensure("no initial draw", !recentlyDrawn(0));
    markDrawn(100);
    ensure("current draw", recentlyDrawn(100));
    ensure("previous draw", recentlyDrawn(101));
    ensure("hidden consumers expire", !recentlyDrawn(102));
    markDrawn(103); markDrawn(103);
    ensure("multiple pages share the heartbeat", recentlyDrawn(104));
    ensure("close needs no notification", !recentlyDrawn(105));
    markDrawn(~0u);
    ensure("frame wrap still accepts previous frame", recentlyDrawn(0));
    ensure("frame wrap expires", !recentlyDrawn(1));
}

template<> template<> void shaft_decision_object::test<10>()
{
    // Model the volume's missing light-parameter block: setters are no-ops until
    // enabled. Exercise the exact configuration helper used by the local probe.
    struct Light
    {
        bool enabled = false;
        float color = 0, intensity = 0, radius = 10, falloff = .75f;
        void setIsLight(bool on) { enabled = on; }
        void setLightSRGBColor(float v) { if (enabled) color = v; }
        void setLightIntensity(float v) { if (enabled) intensity = v; }
        void setLightRadius(float v) { if (enabled) radius = v; }
        void setLightFalloff(float v) { if (enabled) falloff = v; }
    } light;
    ALShaftDecision::configureProbeLight(light, 1.f);
    ensure("light block created", light.enabled);
    ensure_equals("requested colour", light.color, 1.f);
    ensure_equals("requested intensity", light.intensity, 1.f);
    ensure_equals("requested radius", light.radius, 12.f);
    ensure_equals("requested falloff", light.falloff, 1.f);
}

}
