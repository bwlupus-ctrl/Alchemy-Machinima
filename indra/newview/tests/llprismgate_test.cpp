/** @file llprismgate_test.cpp @brief Simple Gate spec section g regressions. */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../llprismgate.h"
#include "llsdutil.h"
#include <algorithm>
#include <limits>
namespace tut
{
using namespace LLPrismLens;
struct gate_data
{
    GateState state;
    GateInput in;
    LLUUID a, b, c;
    gate_data()
    {
        a.generate(); b.generate(); c.generate();
        for (const auto& id : {a,b,c})
        {
            GateSourceInput source; source.id = id; source.name = id == a ? "A" : id == b ? "B" : "C";
            source.hasOutput = source.fullQuality = true; source.publication = 10;
            source.decoder = source.copiedDecoder = 50; source.currentImage = true;
            in.sources.push_back(source); in.settings.mSources.push_back(id);
        }
        in.demanded = in.hasGatePicture = true;
        state.program = state.held = state.cursor = a;
        state.text = "On air: A";
        state.copyKey = {a, 1, 10};
    }
    U32 publications = 0, cookiePreparations = 0, cookieAttempts = 0;
    GateCookieCache cookieCache;
    bool cookieAllocationOk = true;
    F64 copyRetryAfter = 0.;
    GateCookieKey cookieKey() const
    {
        GateCookieKey key; key.resolution = 512;
        return gateCookieContentKey(state, key, [&](const LLUUID& id)
        {
            GateCookieSourceKey source; source.id=id;
            for (const auto& s : in.sources) if(s.id==id)
                { source.generation=1; source.publication=s.publication; source.available=s.hasOutput && s.fullQuality; }
            return source;
        });
    }
    void refreshCookie()
    {
        if (!state.held.isNull() && !retainedOutput) return;
        cookieCache.refresh(cookieKey(),in.now,[&](bool& failed)
        {
            ++cookieAttempts;
            if (!cookieAllocationOk) { failed=true; return false; }
            ++cookiePreparations; prepareCookie(); return true;
        });
    }
    bool rollback = false, retainedOutput = true, cookieValid = true, publicationOk = true;
    LLUUID cookieHeld;
    F32 cookieAlpha = 0.f;
    void prepareCookie()
    {
        cookieValid = state.held.isNull() || retainedOutput;
        cookieHeld = state.held;
        cookieAlpha = state.alpha;
    }
    GateProposal tick(F64 dt = .016, bool ok = true)
    {
        in.now += dt;
        in.hasGatePicture = retainedOutput;
        const auto p = gatePrepareStep(state, in); // real stepGate rejects pixel work
        rollback |= p.rollback;
        in.take.setNull();
        if (!in.demanded || in.now < copyRetryAfter) { refreshCookie(); return p; }
        const auto composed = gateStep(state, in); // composeGate re-steps after producers
        const auto id = composed.copy.notNull() ? composed.copy : state.held;
        const auto source = std::find_if(in.sources.begin(), in.sources.end(), [&](const auto& s) { return s.id == id; });
        GateCopyKey key;
        bool copied = false;
        if (source != in.sources.end())
        {
            key = {id, 1, source->publication};
            copied = ok && source->hasOutput && source->fullQuality &&
                (composed.copy.notNull() || !(key == state.copyKey));
        }
        const bool accepted = composed.copy.notNull() ? copied : ok;
        if (copied || (ok && state.held.isNull())) retainedOutput = true;
        gateComposeApply(state, composed, accepted, copied, key, rollback,
            [&](bool) { if (!publicationOk || (!ok && state.held.isNull())) return false;
                ++publications; return true; });
        refreshCookie();
        return p;
    }
    void takeB() { in.take = b; tick(); }
    void freshB() { ++in.sources[1].publication; ++in.sources[1].copiedDecoder; in.sources[1].decoder = in.sources[1].copiedDecoder; }
};
typedef test_group<gate_data> gate_group;
typedef gate_group::object gate_object;
gate_group gate_tests("LLPrismGate");

template<> template<> void gate_object::test<1>()
{
    set_test_name("g1 Gate defaults and LLSD round trip strict settings");
    GateSource s, restored;
    ensure(s.mMode == EGateMode::MANUAL && s.mTransition == EGateTransition::CUT && s.mOffAir == EGateOffAir::KEEP_PLAYING);
    ensure(s.mIntervalSeconds == 8. && s.mCrossfadeSeconds == .5);
    s.mSources = {a,b}; s.mProgramId = b; s.mMode = EGateMode::AUTO_CYCLE;
    s.mTransition = EGateTransition::CROSSFADE; s.mOffAir = EGateOffAir::PAUSE;
    const auto saved = gateSourceToLLSD(s); ensure(gateSourceFromLLSD(saved, restored));
    ensure(restored.mSources == s.mSources && restored.mProgramId == b && restored.mOffAir == EGateOffAir::PAUSE);
    for (const auto& key : {"gate_interval", "gate_crossfade_seconds"})
        for (F64 value : {-1., 0., 121., std::numeric_limits<F64>::quiet_NaN()})
        { auto bad = saved; bad[key] = value; ensure(!gateSourceFromLLSD(bad, restored)); }
    for (const auto& key : {"gate_mode", "gate_transition", "gate_off_air"})
    { auto bad = saved; bad[key] = "unknown"; ensure(!gateSourceFromLLSD(bad, restored)); }
}
template<> template<> void gate_object::test<2>()
{
    set_test_name("g1 late source resolution prunes self duplicate unknown and caps seven");
    GateSource s; s.mSources = {a,a,b,c}; s.mProgramId = c;
    std::vector<std::string> notes; gateResolveSources(s,c,{b,a},notes);
    ensure(s.mSources == std::vector<LLUUID>({b,a})); ensure_equals(notes.size(),size_t(2)); ensure(s.mProgramId.isNull());
    s.mSources.clear(); std::vector<LLUUID> live;
    for (S32 i=0;i<9;++i) { LLUUID id; id.generate(); s.mSources.push_back(id); live.push_back(id); }
    gateResolveSources(s,c,live,notes); ensure_equals(s.mSources.size(),size_t(7));
    // The Gate item can precede all sources: parsing has no registry dependency.
    auto item = gateSourceToLLSD(s); GateSource parsed;
    ensure(gateSourceFromLLSD(item,parsed)); gateResolveSources(parsed,c,live,notes); ensure(parsed.mSources == s.mSources);
}
namespace
{
LLSD legacyScene(const LLUUID& a, const LLUUID& b, S32 count = 2)
{
    LLSD scene; scene["prism_captures"] = LLSD::emptyArray();
    for (S32 i=0;i<count;++i)
    { LLSD source; source["capture_id"] = i==0?a:i==1?b:LLUUID::generateNewID(); source["mode"]="camera_feed"; source["source_on_cut"]="live"; scene["prism_captures"].append(source); }
    LLSD binding; binding["binding_id"]=LLUUID::generateNewID(); binding["capture_id"]=a; binding["gate_source"]=true;
    binding["display_te"]=3; binding["fit"]="fill"; binding["anchor"][0]=.3; binding["screen_effects"]["static"]=.2;
    scene["prism_displays"].append(binding); binding["home_capture_id"]=b; scene["prism_projectors"].append(binding);
    LLSD old; old["gate_id"]=LLUUID::generateNewID(); old["active"]=true; old["prewarm_frames"]=3; old["mode"]="auto_cycle"; old["interval_seconds"]=12.; old["transition"]="crossfade"; old["crossfade_seconds"]=.7;
    old["program_arm_index"]=0;
    for (const auto& id : {a,b}) { LLSD arm; arm["capture_id"]=id; arm["enabled"]=id==a; old["armed"].append(arm); }
    scene["prism_gates"].append(old); return scene;
}
}
template<> template<> void gate_object::test<3>()
{
    set_test_name("g2 migration free slot preserves TE mapping effects and strips keys");
    auto scene=legacyScene(a,b); const auto original=scene["prism_displays"][0]; std::vector<SkippedBinding> notes;
    gateMigrateScene(scene,notes); ensure(notes.empty()); ensure_equals(scene["prism_captures"].size(),3);
    GateSource restored; ensure(gateSourceFromLLSD(scene["prism_captures"][2],restored));
    ensure(restored.mSources==std::vector<LLUUID>({a}) && restored.mProgramId==a);
    ensure(restored.mMode==EGateMode::AUTO_CYCLE && restored.mCrossfadeSeconds==.7);
    const auto id=scene["prism_captures"][2]["capture_id"].asUUID();
    ensure(scene["prism_displays"][0]["capture_id"].asUUID()==id && scene["prism_projectors"][0]["capture_id"].asUUID()==id);
    for (const auto& key : {"display_te","fit","anchor","screen_effects"}) ensure(llsd_equals(scene["prism_displays"][0][key],original[key]));
    ensure(!scene.has("prism_gates") && !scene["prism_displays"][0].has("gate_source") && !scene["prism_projectors"][0].has("home_capture_id"));
    ensure(!scene["prism_captures"][0].has("source_on_cut"));
}
template<> template<> void gate_object::test<4>()
{
    set_test_name("g2 malformed legacy record still creates an empty Gate");
    auto faces_only=legacyScene(a,b); faces_only.erase("prism_projectors");
    std::vector<SkippedBinding> face_notes; gateMigrateScene(faces_only,face_notes);
    ensure("optional projector array stays absent so the loader can supply its default",!faces_only.has("prism_projectors"));
    ensure(face_notes.empty());
    auto scene=legacyScene(a,b); scene["prism_gates"]="bad"; std::vector<SkippedBinding> notes; gateMigrateScene(scene,notes);
    GateSource restored; ensure(gateSourceFromLLSD(scene["prism_captures"][2],restored)); ensure(restored.mSources.empty());
    for (const auto& key : {"mode", "interval_seconds", "prewarm_frames", "armed"})
    {
        scene=legacyScene(a,b); scene["prism_gates"][0][key]="malformed";
        gateMigrateScene(scene,notes); ensure(gateSourceFromLLSD(scene["prism_captures"][2],restored));
        ensure(restored.mSources.empty());
    }
}
template<> template<> void gate_object::test<5>()
{
    set_test_name("g2 full scene fallback uses home then hint and reports unresolved bindings");
    auto scene=legacyScene(a,b,8); LLSD bad=scene["prism_displays"][0]; bad["capture_id"]=LLUUID::null; scene["prism_displays"].append(bad);
    std::vector<SkippedBinding> notes; gateMigrateScene(scene,notes);
    ensure_equals(scene["prism_captures"].size(),8); ensure_equals(scene["prism_displays"].size(),1); ensure_equals(notes.size(),size_t(1));
    ensure(scene["prism_displays"][0]["capture_id"].asUUID()==a && scene["prism_projectors"][0]["capture_id"].asUUID()==b);
}
template<> template<> void gate_object::test<6>()
{
    set_test_name("g3 pending holds A feeds A B and requires a post-click camera publication");
    in.take=b; auto p=tick(); ensure(state.pending==b && state.held==a && state.program==a);
    ensure(p.feed==std::vector<LLUUID>({a,b})); ensure(!tick().needsPixels);
    freshB(); p=gateStep(state,in); ensure(p.needsPixels && p.copy==b);
    const auto before=state; gateApply(state,p,false); ensure(state.program==a && state.text==before.text && gateGain(state,a)==1.f && state.copyKey==before.copyKey);
    gateApply(state,p,true); ensure(state.program==b && state.held==b && state.cutSerial==1);
}
template<> template<> void gate_object::test<7>()
{
    set_test_name("g3 C2 N1 old decoder copies cannot pass either freshness baseline");
    auto& video=in.sources[1]; video.kind=ESourceSlotKind::MEDIA_URL; video.copiedDecoder=10; video.decoder=50;
    takeB(); ++video.publication; video.copiedDecoder=50; ensure(!tick().needsPixels);
    video.copiedDecoder=51; video.decoder=51; ensure(tick().needsPixels && state.program==b);
    gate_data f; f.in.sources[1].kind=ESourceSlotKind::MEDIA_URL; f.takeB();
    ++f.in.sources[1].copiedDecoder; ensure(!f.tick().needsPixels); // decoder alone, no publication
}
template<> template<> void gate_object::test<8>()
{
    set_test_name("g3 g8 frozen and validated paused prim-face are fresh without a new publication");
    for (bool frozen : {false,true})
    {
        gate_data f; auto& source=f.in.sources[1]; source.kind=ESourceSlotKind::MEDIA_FACE;
        source.frozen=frozen; source.paused=!frozen; f.in.take=f.b;
        ensure(f.tick().needsPixels && f.state.program==f.b);
    }
    in.sources[1].kind=ESourceSlotKind::MEDIA_FACE; in.sources[1].paused=true; in.sources[1].currentImage=false;
    takeB(); ensure(state.pending==b);
}
template<> template<> void gate_object::test<9>()
{
    set_test_name("g4 timed fade feeds two sources then copies B and clears from");
    in.settings.mTransition=EGateTransition::CROSSFADE; takeB(); freshB(); tick();
    ensure(state.from==a && state.held==a && state.program==b && state.alpha==0.f);
    auto p=tick(.25); ensure(std::abs(state.alpha-.5f)<.001f); ensure(p.feed==std::vector<LLUUID>({b,a}));
    p=tick(.25); ensure(p.copy==b && state.from.isNull() && state.held==b);
}
template<> template<> void gate_object::test<10>()
{
    set_test_name("g4 first program prune recovery and unseen commit are cuts");
    in.settings.mTransition=EGateTransition::CROSSFADE; state={}; tick(); ++in.sources[0].publication; tick(); ensure(state.program==a && state.from.isNull());
    in.sources.erase(in.sources.begin()); tick(); ++in.sources[0].publication; tick(); ensure(state.program==b && state.from.isNull());
    const U64 serial=state.cutSerial;
    in.sources.clear(); const auto empty=tick();
    ensure(state.program.isNull() && state.held.isNull() && state.text=="No sources ticked");
    ensure(empty.publish && empty.rollback && empty.feed.empty());
    ensure_equals("clearing sources preserves the monotonic cut serial",state.cutSerial,serial);
    gate_data f; f.in.settings.mTransition=EGateTransition::CROSSFADE; f.in.demanded=false; f.in.take=f.b; f.tick(.3); f.tick(.3);
    ensure(f.state.program==f.b && f.state.held==f.a && f.state.from.isNull());
}
template<> template<> void gate_object::test<11>()
{
    set_test_name("g4a fresh copy failures time out to held with matching audio and named status");
    takeB(); freshB(); for (S32 i=0;i<61;++i) tick(.1,false);
    ensure_equals("timeout program",state.program,a); ensure("timeout pending cleared",state.pending.isNull()); ensure("timeout status: "+state.text,state.text.find("B has no picture")!=std::string::npos);
    ensure(gateGain(state,a)==1.f && gateGain(state,b)==0.f);
}
template<> template<> void gate_object::test<12>()
{
    set_test_name("g4b unseen Auto runs twenty seconds with no deadline and fresh copy on return");
    in.settings.mMode=EGateMode::AUTO_CYCLE; in.demanded=false;
    for (S32 i=0;i<200;++i) tick(.1);
    ensure(state.text.rfind("On air:",0)==0 && state.held==a && state.awaitingCopy);
    // Unseen Auto still advances at each interval rather than waiting for pixels.
    ensure(state.cutSerial>=2);
    in.demanded=true; const LLUUID program=state.program;
    for (auto& source:in.sources) if(source.id==program) ++source.publication;
    tick(); ensure(state.held==program && !state.awaitingCopy);
}
template<> template<> void gate_object::test<13>()
{
    set_test_name("g4b demand debounce preserves Crossfade over alternate frames");
    in.settings.mTransition=EGateTransition::CROSSFADE; takeB();
    for (S32 i=0;i<120;++i) { in.demanded=(i%2)==0; tick(.016); }
    freshB(); in.demanded=true; tick(); ensure(state.from==a && state.program==b);
    in.demanded=false; tick(1.); ensure(state.from==a); in.demanded=true; tick(); ensure(state.held==b && state.from.isNull());
}
template<> template<> void gate_object::test<14>()
{
    set_test_name("g4b long demand lapse turns pending into unseen cut with paused clock");
    takeB(); in.demanded=false; tick(.1); tick(.3); ensure(state.program==b && state.held==a && state.awaitingCopy);
    tick(30.); ensure(state.program==b); in.demanded=true; freshB(); tick(); ensure(state.held==b);
}
template<> template<> void gate_object::test<15>()
{
    set_test_name("g4c incoming loss EOF budget replacement rolls presentation and audio back even with unchanged key");
    for (S32 loss=0;loss<3;++loss)
    {
        gate_data f; f.in.settings.mTransition=EGateTransition::CROSSFADE; f.in.sources[1].kind=ESourceSlotKind::MEDIA_URL;
        f.takeB(); f.freshB(); f.tick(); f.tick(.2); const auto key=f.state.copyKey;
        f.in.sources[1].hasOutput=false; f.in.sources[1].reason=loss==0?"EOF":loss==1?"budget":"replaced";
        const auto p=f.tick(); ensure(p.rollback && p.publish && f.state.program==f.a && f.state.held==f.a && f.state.copyKey==key);
        ensure(gateGain(f.state,f.a)==1.f && gateGain(f.state,f.b)==0.f && f.state.from.isNull());
    }
}
template<> template<> void gate_object::test<16>()
{
    set_test_name("g4a fade-end copy and unseen broken program share five demanded seconds rollback");
    in.settings.mTransition=EGateTransition::CROSSFADE; takeB(); freshB(); tick(); tick(.6,false);
    for (S32 i=0;i<55;++i) tick(.1,false);
    ensure_equals("fade timeout program",state.program,a); ensure("fade cleared",state.from.isNull()); ensure("fade timeout status: "+state.text,state.text.find("B has no picture")!=std::string::npos);
    gate_data f; f.in.demanded=false; f.in.take=f.b; f.tick(.3); f.tick(.3);
    f.in.sources[1].hasOutput=false; f.in.demanded=true;
    for (S32 i=0;i<55;++i) f.tick(.1);
    ensure(f.state.program==f.a && f.state.text.find("B has no picture")!=std::string::npos);
}
template<> template<> void gate_object::test<17>()
{
    set_test_name("g5 g6 cursor skips ungranted and failed sources without livelock");
    state={}; in.sources[0].kind=ESourceSlotKind::MEDIA_URL; in.sources[0].eligible=false;
    in.sources[1].hasOutput=false; tick(); ensure(state.pending==b);
    for (S32 i=0;i<55;++i) tick(.1); tick(); ensure(state.pending==c); ++in.sources[2].publication; tick(); ensure(state.program==c);
    for (const auto& id : tick().play) ensure(id!=a);
    state.program=a; state.pending.setNull(); tick(); ensure(state.program!=a);
}
template<> template<> void gate_object::test<18>()
{
    set_test_name("g8 pending URL Play both modes off-feed Pause ignores downloads and prim faces");
    for (auto mode : {EGateMode::MANUAL,EGateMode::AUTO_CYCLE})
    {
        gate_data f; f.in.settings.mMode=mode; f.in.settings.mOffAir=EGateOffAir::PAUSE;
        f.in.sources[1].kind=ESourceSlotKind::MEDIA_URL;
        f.in.sources[2].kind=ESourceSlotKind::MEDIA_URL; f.in.sources[2].desiredPlaying=true;
        if(mode==EGateMode::MANUAL) f.in.take=f.b;
        auto p=f.tick(); ensure(p.play==std::vector<LLUUID>({f.b}) && p.pause==std::vector<LLUUID>({f.c}));
        f.in.sources[1].desiredPlaying=true; f.in.sources[2].downloading=true; p=f.tick(); ensure(p.play.empty() && p.pause.empty());
        f.in.sources[2].kind=ESourceSlotKind::MEDIA_FACE; f.in.sources[2].downloading=false; ensure(f.tick().pause.empty());
    }
}
template<> template<> void gate_object::test<19>()
{
    set_test_name("g8 refusal fails immediately program stop keeps picture and ordinary steady transport is silent");
    in.sources[1].kind=ESourceSlotKind::MEDIA_URL; takeB(); in.sources[1].playRefused=true;
    auto p=tick(); ensure(p.rollback && state.program==a && state.pending.isNull());
    in.sources[0].kind=ESourceSlotKind::MEDIA_URL; in.sources[0].stopped=true;
    p=tick(); ensure(state.held==a && state.text.find("A stopped")!=std::string::npos && p.play.empty());
}
template<> template<> void gate_object::test<20>()
{
    set_test_name("g9 URL face fades use complementary local gains camera program mutes all videos");
    for (auto from : {ESourceSlotKind::MEDIA_URL,ESourceSlotKind::MEDIA_FACE})
        for (auto to : {ESourceSlotKind::MEDIA_URL,ESourceSlotKind::MEDIA_FACE})
        {
            gate_data f; f.in.sources[0].kind=from; f.in.sources[1].kind=to;
            f.in.settings.mTransition=EGateTransition::CROSSFADE; f.takeB(); f.freshB(); f.tick(); f.tick(.125);
            ensure(std::abs(gateGain(f.state,f.a)-.75f)<.001f && std::abs(gateGain(f.state,f.b)-.25f)<.001f);
            ensure(gateGain(f.state,f.c)==0.f);
        }
    ensure(gateGain(state,b)==0.f && gateGain(state,c)==0.f);
}
template<> template<> void gate_object::test<21>()
{
    set_test_name("g10 copy keys distinguish equal serials by identity and generation");
    ensure(!(GateCopyKey{a,1,10}==GateCopyKey{b,1,10})); ensure(!(GateCopyKey{a,1,10}==GateCopyKey{a,2,10}));
    in.take=b; tick(); freshB(); const auto p=tick(); ensure(p.copy==b);
}
template<> template<> void gate_object::test<22>()
{
    set_test_name("ga manual latest click queues during a fade and Auto ignores names");
    in.settings.mTransition=EGateTransition::CROSSFADE; takeB(); freshB(); tick();
    in.take=c; tick(.1); in.take=a; tick(.1); tick(.3); tick(); ensure(state.pending==a);
    gate_data f; f.in.settings.mMode=EGateMode::AUTO_CYCLE; f.state.nextAuto=100.; f.in.take=f.c;
    f.tick(); ensure(f.state.program==f.a && f.state.pending.isNull());
}

template<> template<> void gate_object::test<23>()
{
    set_test_name("g9 shared impl max and departure gain restoration cover rebind untick deletion and VCam off");
    const auto shared = gateAudioGains({{a,0.f},{a,.7f},{b,0.f}},{});
    ensure_equals(shared.at(a),.7f); ensure_equals(shared.at(b),0.f);
    // A face rebind changes impl identity: restore the old instance and mute the new one.
    auto rebound = gateAudioGains({{c,0.f}}, {a,b});
    ensure_equals(rebound.at(a),1.f); ensure_equals(rebound.at(b),1.f); ensure_equals(rebound.at(c),0.f);
    for (const auto& old : {a,b,c})
    {
        const auto gone = gateAudioGains({}, {old});
        ensure_equals(gone.at(old),1.f);
    }
    // A Volume edit cannot change the reconciled off-air gain.
    ensure_equals(gateAudioGains({{b,gateGain(state,b)}},{}).at(b),0.f);
    ensure(gateAudioGains({},{}).empty());
}
template<> template<> void gate_object::test<24>()
{
    set_test_name("g8 on-air demand outranks private floor under two normal four low caps");
    state.program=a; state.pending=b; state.from=c;
    std::vector<std::pair<F64,LLUUID>> players;
    for (S32 i=0;i<7;++i)
    {
        const LLUUID id=i==0?a:i==1?b:i==2?c:LLUUID::generateNewID();
        players.emplace_back(gatePlayerDemand(state,id,1024.*1024.),id);
    }
    std::stable_sort(players.begin(),players.end(),[](const auto& x,const auto& y){return x.first>y.first;});
    ensure(players.back().second!=a && players.back().second!=b && players.back().second!=c);
    for (const auto& id : {a,b,c})
    {
        const auto found=std::find_if(players.begin(),players.end(),[&](const auto& player){return player.second==id;});
        ensure(found!=players.end() && found-players.begin()<6);
        ensure_equals(found->first,2.*1024.*1024.);
    }
    ensure_equals(gatePlayerDemand(state,LLUUID::generateNewID(),0.),0.);
}
template<> template<> void gate_object::test<25>()
{
    set_test_name("g8 Keep-playing user pause and Pause-to-Keep switch send no unsolicited transport");
    in.sources[1].kind=ESourceSlotKind::MEDIA_URL; in.sources[1].desiredPlaying=false; in.sources[1].paused=true;
    ensure(tick().play.empty());
    in.settings.mOffAir=EGateOffAir::PAUSE; ensure(tick().play.empty());
    in.settings.mOffAir=EGateOffAir::KEEP_PLAYING; ensure(tick().play.empty());
    in.take=b; ensure(tick().play==std::vector<LLUUID>({b}));
    in.sources[1].desiredPlaying=true; ensure(tick().play.empty());
    in.sources[1].desiredPlaying=false; // User Pause after the take is preserved.
    ensure(tick().play.empty());
}

template<> template<> void gate_object::test<26>()
{
    set_test_name("g1 restored eligible Auto program publishes its status then advances from its slot");
    state.cursor.setNull(); state.held.setNull(); state.awaitingCopy=true; state.text="Waiting for A";
    in.settings.mMode=EGateMode::AUTO_CYCLE; tick();
    ensure(state.program==a && state.pending.isNull());
    ++in.sources[0].publication; tick(); ensure(state.held==a && state.text=="On air: A");
    tick(8.1); ensure(state.pending==b); freshB(); tick(); ensure(state.program==b);
}

template<> template<> void gate_object::test<27>()
{
    set_test_name("g3 g7 latest take supersedes an unseen wait without borrowing another slot publication baseline");
    in.sources[1].publication=1000; in.demanded=false; in.take=b; tick(.3); tick(.3);
    ensure(state.program==b && state.awaitingCopy && state.held==a);
    in.demanded=true; in.take=c; const auto waiting=tick();
    ensure(state.pending==c && !state.awaitingCopy && state.held==a);
    ensure(waiting.copy.isNull());
    ++in.sources[2].publication; const auto accepted=tick();
    ensure(accepted.copy==c && state.program==c && state.held==c);
}

template<> template<> void gate_object::test<28>()
{
    set_test_name("rejected prepare replaces pending with frozen or paused take before compose");
    for (bool frozen : {false,true})
    {
        gate_data f; f.takeB(); f.tick(4.99); // Replacing intent gets its own deadline.
        f.in.now+=.02;
        auto& source=f.in.sources[2]; source.kind=ESourceSlotKind::MEDIA_FACE;
        source.frozen=frozen; source.paused=!frozen;
        f.in.take=f.c;
        auto p=gatePrepareStep(f.state,f.in);
        ensure(p.needsPixels && f.state.pending==f.c && f.state.program==f.a);
        f.in.take.setNull();
        p=gateStep(f.state,f.in); gateApply(f.state,p,false);
        ensure(f.state.pending==f.c && f.state.held==f.a);
        p=gateStep(f.state,f.in); gateApply(f.state,p,true);
        ensure(f.state.program==f.c && f.state.held==f.c);
    }
}
template<> template<> void gate_object::test<29>()
{
    set_test_name("fade-end rejected copy preserves newest queued click for next take");
    in.settings.mTransition=EGateTransition::CROSSFADE; takeB(); freshB(); tick();
    in.take=c; tick(.6,false);
    ensure(state.from==a && state.queued==c && state.program==b);
    tick(.01,true); ensure(state.held==b && state.queued==c);
    tick(); ensure(state.pending==c);
}
template<> template<> void gate_object::test<30>()
{
    set_test_name("Crossfade requires a valid Gate texture after release or VCam off");
    in.settings.mTransition=EGateTransition::CROSSFADE;
    takeB(); retainedOutput=false; gateOutputReleased(state);
    freshB(); const auto p=tick();
    ensure(p.copy==b && state.held==b && state.from.isNull());
}
template<> template<> void gate_object::test<31>()
{
    set_test_name("untick or delete holds last Gate picture until recovery source is fresh");
    in.settings.mTransition=EGateTransition::CROSSFADE;
    in.sources.erase(in.sources.begin()); const auto key=state.copyKey;
    auto p=tick();
    ensure(p.rollback && state.program.isNull() && state.held==a && state.copyKey==key && state.pending==b);
    ensure(!tick().publish && state.held==a);
    ++in.sources[0].publication; p=tick();
    ensure(p.copy==b && state.held==b && state.from.isNull());
}
template<> template<> void gate_object::test<32>()
{
    set_test_name("unticking outgoing A finishes fade to healthy incoming B despite failed copy");
    in.settings.mTransition=EGateTransition::CROSSFADE; takeB(); freshB(); tick(); tick(.1);
    in.sources.erase(in.sources.begin()); auto p=tick(.01,false);
    ensure(p.copy==b && state.program==b && state.held==a);
    p=tick(); ensure(p.copy==b && state.held==b && state.program==b && state.from.isNull());
}
template<> template<> void gate_object::test<33>()
{
    set_test_name("ineligible program advances Auto and idle states publish once");
    in.settings.mMode=EGateMode::AUTO_CYCLE; in.sources[0].eligible=false;
    auto p=tick(); ensure(p.rollback && state.program.isNull() && state.pending==b && state.held==a);
    freshB(); tick(); ensure(state.program==b);
    const U32 stable=publications;
    for(S32 i=0;i<60;++i) ensure(!tick().publish);
    ensure_equals(publications,stable);
    in.sources.clear(); ensure(tick().publish); const U32 empty=publications;
    for(S32 i=0;i<60;++i) ensure(!tick().publish);
    ensure_equals(publications,empty);
}
template<> template<> void gate_object::test<34>()
{
    set_test_name("projector-only publication callback follows copy cut fade and rollback once");
    // Composition publishes retained pixels; the content-key pass prepares cookies separately.
    takeB(); freshB(); tick(); ensure_equals(publications,1u);
    ensure(cookieCache.valid && cookieCache.key==cookieKey());
    tick(); ensure_equals(publications,1u);
    in.settings.mTransition=EGateTransition::CROSSFADE; in.sources[2].frozen=true; in.take=c;
    tick(); tick(.2); ensure_equals(publications,3u);
    bool already=false;
    const auto repeated=gateStep(state,in);
    gateComposeApply(state,repeated,true,false,{},already,[&](bool){++publications; ++cookiePreparations;return true;});
    ensure_equals("second compose in the same publication prepares no cookie",publications,3u);
    in.demanded=false; in.sources[2].hasOutput=false;
    const auto p=tick(); ensure(p.rollback && state.program==b);
    ensure("unseen rollback immediately prepares retained B",cookieValid && cookieHeld==b && cookieAlpha==0.f);
    ensure(rollback); ensure_equals(publications,3u);
    tick(); ensure(rollback); ensure_equals(publications,3u);
    in.demanded=true; tick();
    ensure(!rollback); ensure_equals(publications,4u); ensure(cookieCache.key==cookieKey());
}
template<> template<> void gate_object::test<35>()
{
    set_test_name("live tick grants then mutes before its single Play and preserves user Pause");
    U32 grants=0, plays=0; F32 gain=1.f;
    auto grant=[&](){++grants;}; auto mute=[&](){gain=0.f;};
    auto play=[&](){ensure_equals(gain,0.f);++plays;};
    gateLiveTick(EGateOffAir::KEEP_PLAYING,true,grant,mute,play);
    ensure_equals(plays,1u); ensure_equals(grants,1u);
    gateLiveTick(EGateOffAir::KEEP_PLAYING,false,grant,mute,play);
    gateLiveTick(EGateOffAir::PAUSE,true,grant,mute,play);
    ensure_equals(plays,1u);
}
template<> template<> void gate_object::test<36>()
{
    set_test_name("last consumer release invalidates a frozen held copy key");
    const GateCopyKey held=state.copyKey;
    gateOutputReleased(state); ensure(!(state.copyKey==held));
    in.sources[0].frozen=true; retainedOutput=false; in.demanded=false;
    tick(); ensure(!retainedOutput && state.copyKey.id.isNull() && publications==0);
    in.demanded=true; tick(.016,false); // actual copy failure retains the invalid key
    ensure(!retainedOutput && state.copyKey.id.isNull() && publications==0);
    tick(); // steady held-source copy restores output, without a new take/publication
    ensure(retainedOutput && state.copyKey==held && cookieValid && cookieHeld==a);
    ensure_equals(publications,1u); tick(); ensure_equals(publications,1u);
}
template<> template<> void gate_object::test<37>()
{
    set_test_name("untick on-air A selects frozen or paused D without double prune to E");
    for (bool frozen : {false,true})
    {
        gate_data f; auto& d=f.in.sources[1]; d.kind=ESourceSlotKind::MEDIA_FACE;
        d.frozen=frozen; d.paused=!frozen;
        f.in.sources.erase(f.in.sources.begin()); f.tick();
        ensure(f.state.program==f.b && f.state.held==f.b && f.state.cursor==f.b);
    }
}
template<> template<> void gate_object::test<38>()
{
    set_test_name("unseen fade end keeps incoming B until copy or actual incoming loss");
    for (bool stopped : {false,true})
    {
        gate_data f; f.in.settings.mTransition=EGateTransition::CROSSFADE;
        f.takeB(); f.freshB(); f.tick(); f.tick(.2);
        ensure(f.cookieAlpha>0.f); const U32 published=f.publications;
        f.in.demanded=false; f.tick(.1); f.tick(.6);
        ensure(f.state.fadeEndWaiting && f.state.held==f.a);
        ensure(f.cookieValid && f.cookieHeld==f.a && f.cookieAlpha==1.f);
        ensure_equals(f.publications,published);
        // First consumed frame still has no preparation demand, as in production.
        f.in.sources[1].stopped=stopped; f.tick();
        ensure(f.cookieValid && f.cookieHeld==f.a && f.cookieAlpha==(stopped ? 0.f : 1.f));
        if (stopped) ensure(f.state.program==f.a && f.rollback);
        f.in.demanded=true; f.tick();
        ensure(f.state.held==(stopped ? f.a : f.b) && f.cookieAlpha==0.f);
    }
}
template<> template<> void gate_object::test<39>()
{
    set_test_name("failed black publication commits intent but retries until picture succeeds");
    in.sources.clear();
    tick(.016,false);
    ensure(state.program.isNull() && state.held.isNull() && state.text=="No sources ticked");
    ensure(!state.emptyPublished && rollback && publications==0 && state.publishedAt<0.);
    tick();
    ensure(state.emptyPublished && !rollback && publications==1 && cookieValid);
    const U32 published=publications;
    for(U32 n=0;n<30;++n) tick();
    ensure_equals(publications,published);

    // Rollback publication failures also remain pending across preparation.
    gate_data f; f.in.settings.mTransition=EGateTransition::CROSSFADE;
    f.takeB(); f.freshB(); f.tick(); f.tick(.2);
    f.in.sources[1].hasOutput=false; f.publicationOk=false; f.tick(.016,false);
    ensure(f.state.program==f.a && f.rollback && gateGain(f.state,f.a)==1.f);
    f.publicationOk=true; f.tick(); ensure(!f.rollback && f.cookieAlpha==0.f);
}
template<> template<> void gate_object::test<40>()
{
    set_test_name("demanded thirty-frame fade prepares each cookie at most once per frame");
    in.settings.mTransition=EGateTransition::CROSSFADE; takeB(); freshB(); tick();
    for(U32 n=0;n<31;++n)
    {
        const U32 before=cookiePreparations;
        tick(1./60.);
        ensure(cookiePreparations-before<=1);
    }
    ensure(state.held==b && state.from.isNull());
}
template<> template<> void gate_object::test<41>()
{
    set_test_name("unseen fade refreshes every alpha and failed end copy keeps alpha one");
    in.settings.mTransition=EGateTransition::CROSSFADE; takeB(); freshB(); tick();
    in.demanded=false;
    const U32 before=cookiePreparations;
    for(U32 n=0;n<29;++n) tick(1./60.);
    ensure_equals(cookiePreparations,before+29);
    tick(.1);
    ensure(state.fadeEndWaiting && cookieAlpha==1.f);
    ensure_equals(cookiePreparations,before+30);
    in.demanded=true; tick(.016,false);
    ensure(state.fadeEndWaiting && state.alpha==1.f && state.held==a && cookieAlpha==1.f);
    tick(); ensure(state.held==b && state.from.isNull());

    gate_data f; f.in.settings.mTransition=EGateTransition::CROSSFADE;
    f.takeB(); f.freshB(); f.tick(); f.tick(.2);
    f.tick(.4,false); // Allocation/copy failure on a demanded fade-end frame.
    ensure(f.state.fadeEndWaiting && f.cookieAlpha==1.f && f.state.held==f.a);
}
template<> template<> void gate_object::test<42>()
{
    set_test_name("cut committed with an effect phase change renders that frame exactly once per projector");
    for (bool animated : {false,true})
    {
        GateCookieCache cookies[2]; ProjectorSettings settings;
        settings.mEffects.mStatic=animated ? .5f : 0.f;
        auto key=cookieKey(); key.phase=gateEffectPhase(settings,30.f,1.);
        U32 renders[2]={0,0};
        for(U32 i=0;i<2;++i) cookies[i].refresh(key,1.,[&](bool&){++renders[i];return true;});
        // An effect-due phase is evaluated AFTER composition commits B.
        takeB(); freshB(); tick(); ensure(state.held==b);
        key=cookieKey(); key.phase=gateEffectPhase(settings,30.f,1.1);
        for(U32 i=0;i<2;++i)
        {
            ensure(cookies[i].refresh(key,1.1,[&](bool&){++renders[i];return true;}));
            ensure(cookies[i].key.program.id==b);
            ensure(!cookies[i].refresh(key,1.1,[&](bool&){++renders[i];return true;}));
            ensure_equals(renders[i],2u);
        }
        state=GateState(); state.program=state.held=state.cursor=a; state.copyKey={a,1,10};
    }
}
template<> template<> void gate_object::test<43>()
{
    set_test_name("copy allocation failure and backoff never stall unseen or visible cookie alpha");
    in.settings.mTransition=EGateTransition::CROSSFADE; takeB(); freshB(); tick();
    const auto retained=state.copyKey;
    // Fail the production spare-output seam while retaining the old picture.
    struct Output { U32 getWidth() const{return 1024;} U32 getHeight() const{return 1024;}
        bool isComplete() const{return true;} void swapOutput(Output&){} } output,spare;
    ensure(!produceProjectorOutput(output,spare,2048,[](auto&,U32){return false;},[](auto&){return true;}));
    copyRetryAfter=gateRetryAfter(in.now);
    for(bool demanded : {true,false,true,false})
    {
        in.demanded=demanded; const U32 before=cookiePreparations;
        tick(1./60.);
        ensure_equals(cookiePreparations,before+1);
        ensure_equals(cookieAlpha,state.alpha); ensure(cookieAlpha>0.f && state.copyKey==retained);
    }
}
template<> template<> void gate_object::test<44>()
{
    set_test_name("unseen last untick renders black once then stays idle for 300 frames");
    tick(); in.demanded=false; in.sources.clear();
    const U32 before=cookiePreparations; tick();
    ensure(cookieCache.key.black && cookieValid); ensure_equals(cookiePreparations,before+1);
    for(U32 n=0;n<300;++n) tick();
    ensure_equals(cookiePreparations,before+1);
    // A failed black allocation keeps the accepted A key; changing intents cannot bypass retry.
    gate_data f; f.tick(); const auto old=f.cookieCache.key;
    f.in.demanded=false; f.in.sources.clear(); f.cookieAllocationOk=false; f.tick();
    ensure(f.cookieCache.key==old); const U32 attempts=f.cookieAttempts;
    for(U32 n=0;n<9;++n) f.tick(1./60.);
    ensure_equals(f.cookieAttempts,attempts);
    f.cookieAllocationOk=true; f.tick(.02);
    ensure_equals(f.cookieAttempts,attempts+1); ensure(f.cookieCache.key.black);
    const U32 black=f.cookiePreparations;
    for(U32 n=0;n<300;++n) f.tick();
    ensure_equals(f.cookiePreparations,black);
}
template<> template<> void gate_object::test<45>()
{
    set_test_name("each cookie dependency changes the key and failed rendering never accepts it");
    GateCookieKey base=cookieKey(); base.from={b,1,11,true}; base.gate=c;
    auto changed=[&](const GateCookieKey& key)
    {
        GateCookieCache cache; U32 renders=0;
        cache.refresh(base,0.,[&](bool&){++renders;return true;});
        ensure(!(key==base));
        ensure(!cache.refresh(key,1.,[](bool&){return false;})); ensure(cache.key==base);
        ensure(cache.refresh(key,1.,[&](bool&){++renders;return true;}));
        ensure(!cache.refresh(key,1.,[&](bool&){++renders;return true;})); ensure_equals(renders,2u);
    };
    auto k=base; k.program.id=b; changed(k);
    k=base; ++k.program.generation; changed(k); k=base; ++k.program.publication; changed(k);
    k=base; k.program.available=false; changed(k);
    k=base; k.from.id=c; changed(k); k=base; ++k.from.publication; changed(k);
    k=base; ++k.from.generation; changed(k); k=base; k.from.available=false; changed(k);
    k=base; ++k.retained.publication; changed(k); k=base; ++k.retained.generation; changed(k);
    k=base; k.retained.id=b; changed(k); k=base; k.alpha=.3f; changed(k);
    k=base; k.black=true; changed(k); k=base; ++k.phase; changed(k);
    k=base; k.effectHz=15.f; changed(k); k=base; k.destinationAspect=2.f; changed(k); k=base; ++k.settingsRevision; changed(k);
    k=base; k.resolution=2048; changed(k); k=base; ++k.gateGeneration; changed(k);
    ProjectorSettings effects; ensure_equals(gateEffectPhase(effects,30.f,10.),U64(0));
    effects.mEffects.mStatic=.5f;
    ensure_equals(gateEffectPhase(effects,30.f,1.001),gateEffectPhase(effects,30.f,1.02));
    ensure(gateEffectPhase(effects,30.f,1.04)>gateEffectPhase(effects,30.f,1.02));
}

template<> template<> void gate_object::test<46>()
{
    set_test_name("black ignores obsolete sources and animation; changed keys cannot bypass allocation backoff");
    auto key=cookieKey(); key.phase=42; key.effectHz=30.f;
    state.held.setNull(); const auto black=gateCookieContentKey(state,key,[](const auto&){return GateCookieSourceKey();});
    ensure(black.black && black.program.id.isNull() && black.from.id.isNull() &&
        black.retained.id.isNull() && black.alpha==0.f && black.phase==0 && black.effectHz==0.f);
    GateCookieCache cache; U32 attempts=0;
    cache.refresh(key,0.,[](bool&){return true;}); const auto accepted=cache.key;
    cache.refresh(black,1.,[&](bool& failed){++attempts;failed=true;return false;});
    for(U32 n=1;n<10;++n)
    {
        auto next=black; next.alpha=.01f*n; next.phase=n;
        ensure(!cache.refresh(next,1.+n/60.,[&](bool&){++attempts;return true;}));
        ensure(cache.key==accepted);
    }
    ensure_equals(attempts,1u);
    ensure(cache.refresh(black,1.2,[&](bool&){++attempts;return true;}));
    for(U32 n=0;n<300;++n)
    {
        state.program=LLUUID::generateNewID(); state.from=state.program; state.alpha=.5f;
        key.phase=n; key.effectHz=15.f;
        const auto empty=gateCookieContentKey(state,key,[](const auto&){return GateCookieSourceKey();});
        ensure(empty==black);
        ensure(!cache.refresh(empty,2.+n/60.,[&](bool&){++attempts;return true;}));
    }
    ensure_equals(attempts,2u);
}

template<> template<> void gate_object::test<47>()
{
    set_test_name("single ticked on-air URL restarts after nonloop EOF and waits for fresh pixels");
    in.sources.resize(1); in.settings.mSources = {a};
    auto& source = in.sources[0]; source.kind = ESourceSlotKind::MEDIA_URL;
    source.desiredPlaying = true;
    in.take = a; auto playing = tick();
    ensure(playing.play.empty() && state.pending.isNull());
    source.stopped = true; source.desiredPlaying = false; source.reason = "Ended";
    tick(); in.take = a; const auto restart = tick();
    ensure(restart.play == std::vector<LLUUID>({a}));
    ensure(state.pending == a); ensure_equals(state.baselinePublication, source.publication);
    const auto cuts = state.cutSerial;
    source.stopped = false; source.desiredPlaying = true;
    tick(); ensure(state.pending == a && state.cutSerial == cuts);
    ++source.publication; ++source.decoder; source.copiedDecoder = source.decoder;
    tick(); ensure(state.pending.isNull() && state.program == a);
    ensure_equals(state.cutSerial, cuts+1);
    in.take = a; ensure(tick().play.empty()); ensure(state.pending.isNull());
}

template<> template<> void gate_object::test<48>()
{
    set_test_name("last untick presents constant black during failed black target publication and retry");
    in.sources.clear(); in.settings.mSources.clear(); publicationOk = false;
    tick(.016, false);
    ensure(!state.emptyPublished && retainedOutput && state.held.isNull());
    for (U32 retry=0; retry<12; ++retry)
    {
        CompositeState face; face.mGateAlpha=.5f; face.mBarColorLinear[0]=1.f;
        ensure(gateBlackComposite(state.held.isNull(), 2, face));
        ensure(face.mBlack && face.mCaptureSlot==2 && face.mGateAlpha==0.f);
        ensure(face.mLetterbox==1 && face.mDisplayToCaptureScale[0]==0.f &&
            face.mDisplayToCaptureOffset[0]<0.f && face.mBarColorLinear[0]==0.f);
        ensure(cookieKey().black);
        tick(.016, false);
    }
}

}
