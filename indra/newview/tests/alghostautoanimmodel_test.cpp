// [AutoAnimate] Pure model tests. Written for the later BUILD_TESTING build; no viewer stubs.
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alghostautoanimmodel.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace tut
{
using namespace ALGhostAutoAnim;
struct alghostautoanimmodel_data
{
    LLUUID a{"00000000-0000-0000-0000-000000000001"};
    LLUUID b{"00000000-0000-0000-0000-000000000002"};
    LLUUID c{"00000000-0000-0000-0000-000000000003"};
    LLUUID d{"00000000-0000-0000-0000-000000000004"};
    static AnimFacts facts(bool loop = true, U8 layer = LAYER_BODY)
    { AnimFacts f; f.loop_asset = loop; f.dur = 1.f; f.ease_in = 0.4f; f.prio = 4; f.layer = layer; return f; }
    static bool resident(const LLUUID&) { return true; }
    static bool unknown(const LLUUID&, AnimFacts&) { return false; }
    Entry entry(const LLUUID& id, bool loop = true, U8 layer = LAYER_BODY) const
    {
        Entry e; static_cast<AnimFacts&>(e) = facts(loop, layer); e.id = id; e.starts = 3;
        e.dwell.add(0.5f); e.inter.add(1.f); e.fires_without_state = 1; return e;
    }
    ChannelPool pairPool() const
    {
        ChannelPool p; p.entries = {entry(a), entry(b)}; p.edges.push_back({0, 1, 2, 0}); p.switches = 2; p.finalize(); return p;
    }
    static U32 replacement(const ChannelPool& p, const LLUUID& x, const LLUUID& y)
    {
        const S32 xi = p.find(x), yi = p.find(y);
        for (const Edge& e : p.edges) if ((e.a == xi && e.b == yi) || (e.a == yi && e.b == xi)) return e.replace;
        return 0;
    }
};
typedef test_group<alghostautoanimmodel_data> alghostautoanimmodel_group;
typedef alghostautoanimmodel_group::object alghostautoanimmodel_object;
alghostautoanimmodel_group gAutoAnimModel("alghostautoanimmodel");

template<> template<> void alghostautoanimmodel_object::test<1>()
{
    ChannelRecorder r; r.onStart(a, facts(), 0.0); r.onStart(b, facts(), 9.7); r.onStop(a, 10.0);
    r.onStart(c, facts(), 19.7); r.onStop(b, 20.0); r.onStop(c, 30.0);
    const auto p = r.snapshot(32.0);
    ensure_equals("overlapping stands form one group", p.groups.size(), size_t(1));
    ensure_equals("three stands", p.groups[0].size(), size_t(3));
    ensure_equals("two switches", p.switches, U32(2)); ensure("not thin", !p.thin);
}
template<> template<> void alghostautoanimmodel_object::test<2>()
{
    ChannelRecorder r; r.onStart(a, facts(), 0.0); r.onStart(c, facts(true, LAYER_FACE), 2.0);
    r.onStop(c, 8.0); r.onStart(b, facts(), 9.7); r.onStop(a, 10.0);
    const auto p = r.snapshot(12.0);
    ensure_equals("face is a separate group", p.groups.size(), size_t(2));
    ensure_equals("face layer", p.entries[p.find(c)].layer, U8(LAYER_FACE));
}
template<> template<> void alghostautoanimmodel_object::test<3>()
{
    ChannelRecorder r; r.onStart(a, facts(), 0.0); r.onStart(c, facts(), 2.0);
    r.onStart(b, facts(), 9.7); r.onStop(a, 10.0); r.onStop(c, 12.0);
    const auto p = r.snapshot(14.0);
    ensure_equals("coexistence blocks union", p.groups.size(), size_t(2));
    for (const auto& group : p.groups)
        ensure("co-active state never joins stand alternatives", group.size() == 1 ||
            std::find(group.begin(), group.end(), static_cast<U8>(p.find(c))) == group.end());
}
template<> template<> void alghostautoanimmodel_object::test<4>()
{
    ChannelPool p; p.entries = {entry(a, false), entry(b, false)}; p.entries[0].starts = 2; p.finalize();
    ChannelScheduler s; s.bind(p, c, 0, "body"); s.adopt({}, 0.0, unknown);
    std::vector<LLUUID> removed; s.tick(2.0, resident, removed);
    ensure("two observations excluded", !s.desired().count(a)); ensure("three observations eligible", s.desired().count(b) != 0);
}
template<> template<> void alghostautoanimmodel_object::test<5>()
{
    ChannelRecorder r; r.onStart(a, facts(), 0.0); r.onStart(b, facts(false, LAYER_FACE), 0.0);
    r.onRetrigger(a, facts(), 2.0); r.onRetrigger(b, facts(false, LAYER_FACE), 2.0);
    const auto p = r.snapshot(4.0);
    ensure_equals("loop seq ignored", p.entries[p.find(a)].starts, U32(1));
    ensure_equals("one-shot seq arrival", p.entries[p.find(b)].starts, U32(2));
}
template<> template<> void alghostautoanimmodel_object::test<6>()
{
    ChannelRecorder r; r.onStart(a, facts(), 0.0); r.flush(2.0); r.onGateEnter(5.0);
    r.onStart(b, facts(), 6.0); r.onStop(a, 7.0); r.onGateExit(); r.onStop(a, 8.0);
    const auto p = r.snapshot(10.0);
    ensure_equals("gated start ignored", p.entries.size(), size_t(1));
    ensure_equals("straddling dwell censored", p.entries[0].dwell.count, U32(0));
    ensure("censor bound retained", p.entries[0].max_censored_dwell >= 5.f);
    ensure_equals("no replacement", p.switches, U32(0));
}
template<> template<> void alghostautoanimmodel_object::test<7>()
{
    ChannelRecorder r; r.onStart(a, facts(), 0.0); r.addObserved(20.f);
    const auto p = r.snapshot(20.0);
    ensure_equals("open interval not a dwell sample", p.entries[0].dwell.count, U32(0));
    ensure_equals("censor duration", p.entries[0].max_censored_dwell, 20.f);
    ensure("no switches is thin", p.thin);
}
template<> template<> void alghostautoanimmodel_object::test<8>()
{
    auto p = pairPool(); p.entries.push_back(entry(c)); p.edges.push_back({1, 2, 3, 0}); p.finalize();
    auto choices = [&](const LLUUID& id)
    {
        ChannelScheduler s; s.bind(p, id, 42, "body"); s.adopt({}, 0.0, unknown);
        std::vector<LLUUID> sequence, removed;
        for (S32 i = 0; i < 40; ++i)
        {
            s.tick(static_cast<F64>(i) * 3.0, resident, removed);
            const U64 mask = s.activeStateMask();
            for (size_t j = 0; j < p.entries.size(); ++j) if (mask & (U64(1) << j)) sequence.push_back(p.entries[j].id);
        }
        return sequence;
    };
    ensure("same seed/events", choices(a) == choices(a)); ensure("instance differentiates crowd", choices(a) != choices(b));
}
template<> template<> void alghostautoanimmodel_object::test<9>()
{
    auto p = pairPool(); ChannelScheduler s; s.bind(p, c, 0, "body");
    const AnimMap adopted{{a, 41}, {d, 99}}; s.adopt(adopted, 0.0, unknown);
    std::vector<LLUUID> removed;
    ensure("first tick untouched", !s.tick(0.0, resident, removed)); ensure("exact adoption", s.desired() == adopted);
    s.tick(1.99, resident, removed); ensure("minimum first dwell", s.desired() == adopted);
    s.tick(2.0, resident, removed); s.tick(3.0, resident, removed);
    ensure_equals("foreign seq preserved", s.desired().at(d), 99);
    ensure("foreign never removed", std::find(removed.begin(), removed.end(), d) == removed.end());
}
template<> template<> void alghostautoanimmodel_object::test<10>()
{
    auto p = pairPool(); ChannelScheduler s; s.bind(p, c, 0, "body"); s.adopt({}, 0.0, unknown);
    std::vector<LLUUID> removed; S32 previous = 0;
    for (S32 i = 0; i < 20; ++i)
    {
        s.tick(static_cast<F64>(i) * 3.0, resident, removed);
        const U64 mask = s.activeStateMask();
        const LLUUID& id = mask & 1 ? a : b;
        const S32 seq = s.desired().at(id); ensure("strictly decreasing negative sequence", seq < previous); previous = seq;
    }
    s.bind(p, c, 1, "body"); s.adopt({}, 61.0, unknown); s.tick(61.0, resident, removed);
    for (const auto& anim : s.desired()) ensure("rebind preserves decreasing sequence", anim.second < previous);
}
template<> template<> void alghostautoanimmodel_object::test<11>()
{
    Pool p; p.mSealed = true; p.mBody = pairPool(); p.mObservedSeconds = 20.f;
    const LLSD valid = toLLSD(p); Pool decoded; std::string reason;
    ensure("round trip", fromLLSD(valid, decoded, reason));
    ensure("same entries", decoded.mBody.entries[0].id == a && decoded.mBody.entries[1].id == b);
    auto reject = [&](LLSD data) { ensure("reject malformed pool", !fromLLSD(data, decoded, reason)); ensure("reason provided", !reason.empty()); };
    LLSD bad = valid; bad["version"] = 2; reject(bad);
    bad = valid; bad["observed_s"] = std::numeric_limits<F64>::quiet_NaN(); reject(bad);
    bad = valid; bad["body"]["entries"][0]["dur"] = std::numeric_limits<F64>::infinity(); reject(bad);
    bad = valid; bad["body"]["entries"][0]["id"] = LLUUID::null; reject(bad);
    bad = valid; bad["body"]["entries"][1]["id"] = a; reject(bad);
    bad = valid; bad["body"]["edges"][0]["b"] = 2; reject(bad);
    bad = valid; bad["body"]["entries"][0]["layer"] = 5; reject(bad);
    bad = valid; bad["body"]["entries"][0]["dwell"] = LLSD::emptyArray();
    for (S32 i = 0; i < 17; ++i) bad["body"]["entries"][0]["dwell"].append(1.0); reject(bad);
    bad = valid; for (S32 i = 2; i < 65; ++i) bad["body"]["entries"].append(valid["body"]["entries"][0]); reject(bad);
    bad = valid; bad["body"]["entries"][0]["co_mask"] = "ffffffffffffffff";
    ensure("mask accepted and sanitized", fromLLSD(bad, decoded, reason)); ensure_equals("mask bits", decoded.mBody.entries[0].co_mask, U64(3));
}
template<> template<> void alghostautoanimmodel_object::test<12>()
{
    ensure_equals("core wins", classifyLayer({"mHipLeft", "mFaceLipUpperLeft", "mEyeLeft"}), U8(LAYER_BODY));
    ensure_equals("spine", classifyLayer({"mSpine4"}), U8(LAYER_BODY));
    ensure_equals("upper", classifyLayer({"mNeck", "mHead", "mWristLeft"}), U8(LAYER_UPPER));
    ensure_equals("hands", classifyLayer({"mHandIndex1Left"}), U8(LAYER_HANDS));
    ensure_equals("face tie wins", classifyLayer({"mFaceJaw", "mHandIndex1Left"}), U8(LAYER_FACE));
    ensure_equals("hands beats upper tie", classifyLayer({"mHead", "mHandThumb1Right"}), U8(LAYER_HANDS));
    ensure_equals("extra", classifyLayer({"mTail1", "mWing1Left"}), U8(LAYER_EXTRA));
    ensure_equals("empty", classifyLayer({}), U8(LAYER_EXTRA));
}
template<> template<> void alghostautoanimmodel_object::test<13>()
{
    for (S32 priority : {4, 2})
    {
        auto p = pairPool(); p.entries[1].prio = priority; p.entries[1].ease_in = 0.8f;
        p.entries[1].dwell = Reservoir(); p.entries[1].dwell.add(5.f);
        ChannelScheduler s; s.bind(p, c, 0, "body"); s.adopt({{a, 12}}, 0.0, unknown);
        std::vector<LLUUID> removed; s.tick(0.0, resident, removed); s.tick(2.0, resident, removed);
        ensure("add next before removal", s.desired().count(a) && s.desired().count(b) && removed.empty());
        s.tick(2.79, resident, removed); ensure("ease in respected", s.desired().count(a) != 0);
        s.tick(2.81, resident, removed); ensure("outgoing removed", !s.desired().count(a));
        ensure("explicit eased-stop instruction", std::find(removed.begin(), removed.end(), a) != removed.end());
    }
}
template<> template<> void alghostautoanimmodel_object::test<14>()
{
    auto record = [&](bool other, bool same, F64 gap)
    {
        ChannelRecorder r; r.onStart(a, facts(), 0.0); r.onStop(a, 10.0);
        if (other) r.onStart(c, facts(true, LAYER_FACE), 10.5);
        if (same) r.onStart(d, facts(), 10.7);
        r.onStart(b, facts(), 10.0 + gap); return r.snapshot(15.0);
    };
    ensure_equals("gap edge", replacement(record(false, false, 1.5), a, b), U32(1));
    ensure_equals("other layer does not consume gap", replacement(record(true, false, 1.5), a, b), U32(1));
    const auto intervening = record(false, true, 1.5);
    ensure_equals("first same-layer start only", replacement(intervening, a, d), U32(1));
    ensure_equals("later same-layer start excluded", replacement(intervening, a, b), U32(0));
    ensure_equals("gap too long", replacement(record(false, false, 2.5), a, b), U32(0));
}
template<> template<> void alghostautoanimmodel_object::test<15>()
{
    ChannelRecorder r; r.onStart(a, facts(), 0.0); r.flush(0.99);
    ensure_equals("buffered", r.entryCount(), U32(0));
    ensure_equals("furniture event discarded by late sit gate", r.onGateEnter(0.9), U32(1));
    r.onGateExit(); r.flush(2.0); ensure_equals("discard survives gate exit", r.entryCount(), U32(0));
    r.onStart(b, facts(), 3.0); r.flush(4.0); ensure_equals("one second folds", r.entryCount(), U32(1));
    r.onStart(c, facts(), 4.5); r.flush(5.0, true); r.flush(7.0);
    ensure_equals("seal discards young events", r.entryCount(), U32(1));
}
template<> template<> void alghostautoanimmodel_object::test<16>()
{
    ChannelClock clock;
    ensure_equals("first source rebased", clock.sample(100.0, a), 0.0);
    ensure_equals("clock advances", clock.sample(105.0, a), 5.0);
    ensure_equals("paused source freezes", clock.sample(105.0, a), 5.0);
    ensure_equals("new control avatar continuity", clock.sample(1.0, b), 5.0);
    ensure_equals("new clock advances", clock.sample(2.0, b), 6.0);
    ensure_equals("no source no tick", clock.sample(900.0, LLUUID::null), 6.0);
}
template<> template<> void alghostautoanimmodel_object::test<17>()
{
    Pool p; p.mBody = pairPool(); Pool decoded; std::string reason;
    ensure("unsealed field encoded", !toLLSD(p)["sealed"].asBoolean());
    ensure("unsealed checkpoint can load", fromLLSD(toLLSD(p), decoded, reason)); ensure("loads sealed", decoded.mSealed);
    p.mSealed = true; ensure("sealed field encoded", toLLSD(p)["sealed"].asBoolean());
    ensure("sealed round trip", fromLLSD(toLLSD(p), decoded, reason) && decoded.mSealed);
}
template<> template<> void alghostautoanimmodel_object::test<18>()
{
    Pool p; LinksetKey key; key.point = 4; key.prims = 3; key.ordinal = 1;
    p.mAnimesh.emplace_back(key, pairPool()); LinksetKey query = key; query.item = a;
    ensure("null item fallback", p.findAnimesh(query) != nullptr);
    query.ordinal = 0; ensure("ordinal protects other attachment", !p.findAnimesh(query));
    p.mAnimesh[0].first.item = b; query = key; query.item = a;
    ensure("non-null item mismatch", !p.findAnimesh(query));
    p.mAnimesh[0].first.item.setNull(); Pool decoded; std::string reason;
    ensure("attachment key XML model round trip", fromLLSD(toLLSD(p), decoded, reason));
    query = key; query.item = a; ensure("loaded weak key matches", decoded.findAnimesh(query) != nullptr);
}
template<> template<> void alghostautoanimmodel_object::test<19>()
{
    ChannelScheduler s; s.bind(pairPool(), c, 0, "body"); const AnimMap original{{a, 42}};
    s.adopt(original, 0.0, unknown); std::vector<LLUUID> removed;
    const auto onlyA = [this](const LLUUID& id) { return id == a; };
    s.tick(0.0, onlyA, removed); s.tick(3.0, onlyA, removed);
    ensure("M1 unavailable alternative preserves only stand", s.desired() == original && removed.empty());
    s.tick(10.0, onlyA, removed); ensure("repeated dwell redraw still preserves it", s.desired() == original && removed.empty());
}
template<> template<> void alghostautoanimmodel_object::test<20>()
{
    ChannelRecorder r; r.onStart(a, facts(), 0.0); r.onGateEnter(0.5); r.onGateExit();
    r.onStop(a, 3.0); r.onStart(b, facts(), 4.0);
    const auto p = r.snapshot(6.0); ensure_equals("discarded start not learned", p.find(a), -1);
    ensure_equals("orphan stop has no edge", p.edges.size(), size_t(0));
    // Same rule for an already known id whose new start was discarded.
    r.seedFrom(pairPool()); r.onStart(a, facts(), 7.0); r.onGateEnter(7.5); r.onGateExit();
    r.onStop(a, 10.0); r.onStart(b, facts(), 11.0); const auto known = r.snapshot(13.0);
    ensure_equals("known orphan adds no dwell", known.entries[0].dwell.count, U32(1));
    ensure_equals("known orphan adds no edge", known.switches, U32(2));
}
template<> template<> void alghostautoanimmodel_object::test<21>()
{
    ChannelScheduler s; s.bind(pairPool(), c, 0, "body"); s.adopt({{a, 1}}, 0.0, unknown);
    std::vector<LLUUID> removed; s.tick(0.0, resident, removed); s.tick(2.0, resident, removed);
    ensure("crossfade in progress", s.desired().count(a) && s.desired().count(b));
    s.markCold(); s.tick(3.0, resident, removed);
    ensure("COLD reissues only incoming", !s.desired().count(a) && s.desired().count(b));
    ensure("old pending removal cancelled", removed.empty());
    ensure("cold issue has a fresh negative seq", s.desired().at(b) < -1);
    ChannelPool shot; shot.entries.push_back(entry(c, false)); shot.finalize();
    s.bind(shot, a, 0, "face"); s.adopt({{c, 7}, {d, 99}}, 0.0, unknown);
    s.tick(0.0, resident, removed); s.markCold();
    ensure("COLD publishes pruned desired even without loops", s.tick(0.1, resident, removed));
    ensure("COLD does not reissue an adopted one-shot", !s.desired().count(c));
    s.tick(0.8, resident, removed);
    ensure("one-shot waits for its rescheduled inter-arrival", !s.desired().count(c));
    s.tick(1.4, resident, removed);
    ensure("one-shot fires later on its own schedule", s.desired().at(c) < 0);
    s.markCold(); s.tick(1.5, resident, removed);
    ensure("COLD does not replay a previously fired shot", !s.desired().count(c));
    ensure("foreign stays unowned", !s.owns(d)); ensure_equals("foreign seq unchanged", s.desired().at(d), 99);
}
template<> template<> void alghostautoanimmodel_object::test<22>()
{
    ChannelRecorder r;
    r.onPending(a, 0.0, false); r.flush(0.5);
    ensure_equals("unknown facts get buffer grace", r.entryCount(), U32(0));
    r.resolveFacts(a, facts()); r.onStart(b, facts(), 9.7); r.onStop(a, 10.0);
    const auto p = r.snapshot(12.0);
    ensure_equals("late facts preserve original dwell", p.entries[p.find(a)].dwell.median(), 10.f);
    ensure_equals("late facts preserve overlap edge", p.switches, U32(1));
    r.reset(); r.onPending(a, 0.0, false); r.onGateEnter(0.5); r.onGateExit();
    r.resolveFacts(a, facts()); r.onStop(a, 3.0);
    ensure_equals("late cache cannot resurrect gated start", r.snapshot(5.0).entries.size(), size_t(0));
    // [AutoAnimate] An asset that never loads must not block a ready stand.
    r.reset(); r.onPending(a, 0.0, false); r.onStart(b, facts(), 0.1); r.onStop(a, 0.2);
    r.flush(1.2); ensure_equals("ready event passes timed-out facts", r.entryCount(), U32(1));
    ensure("timed-out fact leaves retry set", !r.hasPendingFacts(a));
    r.resolveFacts(a, facts()); const auto timed = r.snapshot(2.0);
    ensure_equals("timeout cannot resurrect unknown start", timed.find(a), -1);
    ensure_equals("timeout stop creates no replacement", timed.switches, U32(0));
}
template<> template<> void alghostautoanimmodel_object::test<23>()
{
    ChannelScheduler s; s.bind(pairPool(), c, 0, "body"); const AnimMap original{{a, 10}, {b, 11}, {d, 12}};
    s.adopt(original, 0.0, unknown); std::vector<LLUUID> removed;
    s.tick(0.0, resident, removed); ensure("overlap adoption exact", s.desired() == original);
    s.tick(2.0, resident, removed); s.tick(2.41, resident, removed);
    ensure("one group has one clock and one surviving stand", s.desired().count(a) + s.desired().count(b) == 1);
    ensure_equals("foreign still untouched", s.desired().at(d), 12);
}
// [AutoAnimate] Baseline presence is useful, but its first stop is not a full dwell.
template<> template<> void alghostautoanimmodel_object::test<24>()
{
    ChannelRecorder r; r.onBaseline(a, facts(), 0.0); r.flush(1.0);
    r.onStop(a, 5.0); r.onStart(b, facts(), 5.1);
    const auto p = r.snapshot(7.0);
    ensure_equals("baseline state retained", p.find(a), 0);
    ensure_equals("baseline creates no arrival", p.entries[0].starts, U32(0));
    ensure_equals("first stop has no dwell sample", p.entries[0].dwell.count, U32(0));
    ensure_equals("first stop has no replacement edge", p.switches, U32(0));
    r.onStart(a, facts(), 8.0); r.onStop(a, 12.0);
    const auto observed = r.snapshot(14.0);
    ensure_equals("later complete dwell learned", observed.entries[0].dwell.median(), 4.f);
}
template<> template<> void alghostautoanimmodel_object::test<25>()
{
    ChannelPool shot; Entry e = entry(c, false); e.inter = Reservoir(); e.inter.add(10.f);
    shot.entries.push_back(e); shot.finalize();
    ChannelScheduler s; s.bind(shot, a, 0, "face"); s.adopt({}, 0.0, unknown);
    std::vector<LLUUID> removed, retired; s.tick(12.0, resident, removed, &retired); // [AutoAnimate]
    ensure("scheduled shot starts", s.desired().count(c));
    s.tick(13.1, resident, removed, &retired);
    ensure("finished shot leaves desired", !s.desired().count(c));
    ensure("finished shot has one removal", removed.size() == 1 && removed[0] == c);
    ensure("finished shot excluded from POP audit", retired.size() == 1 && retired[0] == c); // [AutoAnimate]
    s.markCold(); s.tick(14.0, resident, removed);
    ensure("finished shot stays absent at COLD", !s.desired().count(c));
}
// [AutoAnimate] Accelerated anim time can retire an adopted shot inside the real-time audit window.
template<> template<> void alghostautoanimmodel_object::test<26>()
{
    auto p = pairPool(); Entry shot = entry(c, false, LAYER_FACE);
    p.entries[1].dwell = Reservoir(); p.entries[1].dwell.add(5.f);
    shot.dur = 2.5f; shot.inter = Reservoir(); shot.inter.add(10.f);
    p.entries.push_back(shot); p.finalize();
    ChannelScheduler s; s.bind(p, d, 0, "body"); s.adopt({{a, 10}, {c, 11}}, 0.0, unknown);
    std::vector<LLUUID> removed, retired{d}; s.tick(0.0, resident, removed, &retired);
    ensure("first tick clears retirement output", retired.empty());
    s.tick(2.0, resident, removed, &retired);
    ensure("adopted shot kept for its duration", s.desired().count(c) && retired.empty());
    s.tick(2.5, resident, removed, &retired); // 1.25 s real time at speed 2.
    ensure("loop ease-out and shot retirement both still stop", removed.size() == 2 &&
        std::find(removed.begin(), removed.end(), a) != removed.end() &&
        std::find(removed.begin(), removed.end(), c) != removed.end());
    ensure("only the one-shot is exempt from POP", retired.size() == 1 && retired[0] == c);
    ensure("retired shot leaves desired", !s.desired().count(c));
    s.tick(2.5, resident, removed, &retired);
    ensure("no-event tick clears retirement output", removed.empty() && retired.empty());
}
// [AutoAnimate] A shot retired and re-fired on one long frame is not a retirement stop.
template<> template<> void alghostautoanimmodel_object::test<27>()
{
    ChannelPool p; Entry shot = entry(c, false); shot.inter = Reservoir(); shot.inter.add(10.f);
    p.entries.push_back(shot); p.finalize();
    ChannelScheduler s; s.bind(p, a, 0, "face"); s.adopt({{c, 7}}, 0.0, unknown);
    std::vector<LLUUID> removed, retired; s.tick(0.0, resident, removed, &retired);
    s.tick(12.0, resident, removed, &retired);
    ensure("long frame re-fires shot", s.desired().at(c) < 0);
    ensure("re-fire cancels both stop and retirement classification", removed.empty() && retired.empty());
    s.unbind(); retired.push_back(c); s.tick(13.0, resident, removed, &retired);
    ensure("unbound tick clears retirement output", retired.empty());
}
}
