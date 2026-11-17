/**
 * @file alclonetransition_test.cpp
 * @brief Clone transitions: the pure maths (complement, ease, seed determinism) and the production
 *        lifecycle state machine (ALCloneTransition::Machine) driven by test doubles.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#include "linden_common.h"

#include "../test/lltut.h"

#include "../alclonetransition.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace tut
{
using namespace ALCloneTransition;

namespace
{
typedef int Id; // the ordered id the Machine is templated on (the viewer uses LLUUID)

// Test double of the manager host: counts every outcome per clone, models validity, and can re-enter.
struct DoubleHost
{
    std::map<Id, bool> cloneOk, bodyOk;
    std::map<Id, int> commits, rollbacks, discards, started;
    std::vector<std::string> log;
    std::function<void(const Record<Id>&)> onFinalizeCallback; // re-entrancy probe
    bool cloneValid(const Id& id) const { auto i = cloneOk.find(id); return i == cloneOk.end() || i->second; }
    bool bodyValid(const Record<Id>& r) const { auto i = bodyOk.find(r.clone); return i == bodyOk.end() || i->second; }
    std::function<void(const Record<Id>&)> onStartCallback;     // started-order probe
    void onStarted(const Record<Id>& r) { ++started[r.clone]; log.push_back("start" + std::to_string(r.clone)); if (onStartCallback) onStartCallback(r); }
    void onCommit(const Record<Id>& r) { ++commits[r.clone]; log.push_back("commit" + std::to_string(r.clone)); if (onFinalizeCallback) onFinalizeCallback(r); }
    void onRollback(const Record<Id>& r) { ++rollbacks[r.clone]; log.push_back("rollback" + std::to_string(r.clone)); if (onFinalizeCallback) onFinalizeCallback(r); }
    void onDiscard(const Record<Id>& r) { ++discards[r.clone]; log.push_back("discard" + std::to_string(r.clone)); if (onFinalizeCallback) onFinalizeCallback(r); }
    int total(Id id) const
    {
        auto get = [id](const std::map<Id, int>& m) { auto i = m.find(id); return i == m.end() ? 0 : i->second; };
        return get(commits) + get(rollbacks) + get(discards);
    }
};

Request request(int32_t kind = KIND_ENTITY_TO_OVERLAY_LOOK, double duration = 1.5)
{
    Request rq;
    rq.kind = kind; rq.fromLook = kind == KIND_LOOK_TO_LOOK ? 1 : -1; rq.toLook = 10;
    rq.style = STYLE_DISSOLVE; rq.duration = duration;
    return rq;
}

RenderParams params(int32_t style, int32_t side, float p, uint32_t seed = 7u, int32_t dir = DIR_UP)
{
    RenderParams r;
    r.active = true; r.style = style; r.side = side; r.p = p; r.seed = seed; r.direction = dir;
    return r;
}
}

struct alclonetransition_data {};
typedef test_group<alclonetransition_data> alclonetransition_group;
typedef alclonetransition_group::object alclonetransition_object;
alclonetransition_group gALCloneTransition("alclonetransition");

// 1. Complement: on matched geometry every point is kept by exactly ONE half, for every style and p.
template<> template<>
void alclonetransition_object::test<1>()
{
    const int32_t styles[] = {STYLE_DISSOLVE, STYLE_WIPE, STYLE_CROSSFADE};
    const float ps[] = {0.f, 0.1f, 0.25f, 0.4999f, 0.5f, 0.75f, 0.9f, 1.f};
    for (int32_t style : styles)
        for (int32_t dir : {DIR_UP, DIR_DOWN})
            for (float p : ps)
            {
                RenderParams out = params(style, SIDE_OUTGOING, p, 12345u, dir), in = out;
                in.side = SIDE_INCOMING;
                for (int ix = -6; ix <= 6; ++ix)
                    for (int iy = -4; iy <= 4; ++iy)
                        for (int iz = -1; iz <= 20; ++iz)
                        {
                            const float pos[3] = {ix * 0.037f, iy * 0.051f, iz * 0.093f};
                            const float field = fieldValue(out, pos);
                            ensure("field in [0,1)", field >= 0.f && field < 1.f);
                            const bool a = keeps(out, field), b = keeps(in, field);
                            ensure("exactly one half keeps every point", a != b);
                        }
            }
}

// 2. Endpoints: p == 0 is only outgoing, p == 1 only incoming; inactive params keep everything.
template<> template<>
void alclonetransition_object::test<2>()
{
    for (int32_t style : {STYLE_DISSOLVE, STYLE_WIPE, STYLE_CROSSFADE})
        for (int i = 0; i < 200; ++i)
        {
            const float field = (i % 100) / 100.f;
            ensure("p=0 outgoing keeps", keeps(params(style, SIDE_OUTGOING, 0.f), field));
            ensure("p=0 incoming drops", !keeps(params(style, SIDE_INCOMING, 0.f), field));
            ensure("p=1 outgoing drops", !keeps(params(style, SIDE_OUTGOING, 1.f), field));
            ensure("p=1 incoming keeps", keeps(params(style, SIDE_INCOMING, 1.f), field));
        }
    RenderParams off = params(STYLE_DISSOLVE, SIDE_INCOMING, 0.f);
    off.active = false;
    ensure("inactive keeps", keeps(off, 0.5f));
    ensure("no edge when inactive", edgeWeight(off, 0.01f) == 0.f);
    ensure("no edge for crossfade", edgeWeight(params(STYLE_CROSSFADE, SIDE_OUTGOING, 0.5f), 0.5f) == 0.f);
    ensure("edge at the cut", edgeWeight(params(STYLE_DISSOLVE, SIDE_OUTGOING, 0.5f), 0.5f) > 0.99f);
}

// 3. Ease: endpoints, monotone, midpoint, clamp.
template<> template<>
void alclonetransition_object::test<3>()
{
    for (int32_t mode : {EASE_SMOOTH, EASE_LINEAR})
    {
        ensure("ease(0)=0", ease(mode, 0.f) == 0.f);
        ensure("ease(1)=1", ease(mode, 1.f) == 1.f);
        ensure("clamp low", ease(mode, -3.f) == 0.f);
        ensure("clamp high", ease(mode, 9.f) == 1.f);
        float prev = -1.f;
        for (int i = 0; i <= 100; ++i)
        {
            const float v = ease(mode, i / 100.f);
            ensure("monotone", v >= prev);
            prev = v;
        }
    }
    ensure("smooth midpoint", std::fabs(ease(EASE_SMOOTH, 0.5f) - 0.5f) < 1e-6f);
    ensure("linear identity", std::fabs(ease(EASE_LINEAR, 0.3f) - 0.3f) < 1e-6f);
    ensure("smooth is slower at the start", ease(EASE_SMOOTH, 0.1f) < ease(EASE_LINEAR, 0.1f));
}

// 4. Seed determinism: same seed => same field everywhere; the id-derived seed is stable and id-dependent.
template<> template<>
void alclonetransition_object::test<4>()
{
    const unsigned char a[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    unsigned char b[16];
    std::copy(a, a + 16, b);
    b[15] ^= 1;
    ensure("seed stable", seedFromBytes(a, 16) == seedFromBytes(a, 16));
    ensure("seed depends on the id", seedFromBytes(a, 16) != seedFromBytes(b, 16));
    const RenderParams one = params(STYLE_DISSOLVE, SIDE_OUTGOING, 0.5f, 99u), two = params(STYLE_DISSOLVE, SIDE_INCOMING, 0.5f, 99u),
        other = params(STYLE_DISSOLVE, SIDE_OUTGOING, 0.5f, 100u);
    int differing = 0;
    for (int i = 0; i < 400; ++i)
    {
        const float pos[3] = {i * 0.0173f, i * 0.0091f - 1.f, i * 0.0231f};
        ensure("both halves read the same field", fieldValue(one, pos) == fieldValue(two, pos));
        if (fieldValue(one, pos) != fieldValue(other, pos)) ++differing;
    }
    ensure("a different seed changes the pattern", differing > 300);
}

// 5. Start: one per clone, at most 2 running (D20), validity of the request.
template<> template<>
void alclonetransition_object::test<5>()
{
    DoubleHost host;
    Machine<Id, DoubleHost> m(host);
    const char* why = nullptr;
    ensure("first starts", m.start(1, 101, request(), 1u, why) != 0);
    ensure("same clone refused", m.start(1, 101, request(), 1u, why) == 0 && why);
    ensure("second starts", m.start(2, 102, request(), 2u, why) != 0);
    ensure("third refused (D20)", m.start(3, 103, request(), 3u, why) == 0 && why);
    ensure_equals("running", m.running(), 2);
    ensure_equals("third never started", host.started[3], 0);
    Request bad = request();
    bad.duration = 0.f;
    Machine<Id, DoubleHost> m2(host);
    ensure("zero duration refused", m2.start(4, 104, bad, 1u, why) == 0);
    bad = request(); bad.duration = 99.0;
    ensure("huge duration refused", m2.start(4, 104, bad, 1u, why) == 0);
    bad = request(); bad.style = STYLE_NONE;
    ensure("style none refused", m2.start(4, 104, bad, 1u, why) == 0);
    host.cloneOk[5] = false;
    ensure("invalid clone refused", m2.start(5, 105, request(), 1u, why) == 0);
    ensure_equals("nothing running in m2", m2.running(), 0);
}

// 6. Every cancellation-table reason, finalized TWICE, with every outcome: exactly one host callback.
template<> template<>
void alclonetransition_object::test<6>()
{
    for (int32_t reason = 0; reason < R_COUNT; ++reason)
        for (int32_t outcome : {OUT_COMMIT, OUT_ROLLBACK, OUT_DISCARD})
        {
            DoubleHost host;
            Machine<Id, DoubleHost> m(host);
            const char* why = nullptr;
            const uint32_t gen = m.start(7, 70, request(), 1u, why);
            ensure("started", gen != 0);
            ensure("first finalize acts", m.finalize(7, gen, reason, outcome));
            ensure("second finalize is a no-op", !m.finalize(7, gen, reason, outcome));
            ensure("third (any generation) is a no-op", !m.finalize(7, 0, reason, outcome));
            ensure_equals("exactly one release", host.total(7), 1);
            ensure_equals("nothing running", m.running(), 0);
            ensure("name exists", std::string(reasonName(reason)) != "unknown");
        }
}

// 7. Finalize in every state: valid, body invalid, clone invalid, both invalid.
template<> template<>
void alclonetransition_object::test<7>()
{
    struct Case { bool clone, body; int32_t request; int32_t expect; };
    const Case cases[] = {
        {true, true, OUT_COMMIT, OUT_COMMIT}, {true, true, OUT_ROLLBACK, OUT_ROLLBACK}, {true, true, OUT_DISCARD, OUT_DISCARD},
        {true, false, OUT_COMMIT, OUT_ROLLBACK}, {true, false, OUT_ROLLBACK, OUT_ROLLBACK},
        {false, true, OUT_COMMIT, OUT_ROLLBACK}, {false, true, OUT_ROLLBACK, OUT_ROLLBACK},
        {false, false, OUT_COMMIT, OUT_DISCARD}, {false, false, OUT_ROLLBACK, OUT_DISCARD}, {false, false, OUT_DISCARD, OUT_DISCARD}};
    for (const Case& c : cases)
    {
        DoubleHost host;
        Machine<Id, DoubleHost> m(host);
        const char* why = nullptr;
        ensure("started", m.start(1, 10, request(), 1u, why) != 0);
        host.cloneOk[1] = c.clone;
        host.bodyOk[1] = c.body;
        ensure("finalized", m.finalize(1, 0, R_CANCEL, c.request));
        ensure_equals("commit count", host.commits[1], c.expect == OUT_COMMIT ? 1 : 0);
        ensure_equals("rollback count", host.rollbacks[1], c.expect == OUT_ROLLBACK ? 1 : 0);
        ensure_equals("discard count", host.discards[1], c.expect == OUT_DISCARD ? 1 : 0);
        ensure("second no-op", !m.finalize(1, 0, R_CANCEL, c.request));
        ensure_equals("one release", host.total(1), 1);
    }
}

// 8. Generation mismatch: a stale or foreign generation never finalizes the current record.
template<> template<>
void alclonetransition_object::test<8>()
{
    DoubleHost host;
    Machine<Id, DoubleHost> m(host);
    const char* why = nullptr;
    const uint32_t first = m.start(1, 10, request(), 1u, why);
    ensure("finalize first", m.finalize(1, first, R_CANCEL, OUT_ROLLBACK));
    const uint32_t second = m.start(1, 10, request(), 1u, why);
    ensure("generations differ", second != first && second != 0);
    ensure("stale generation is a no-op", !m.finalize(1, first, R_CANCEL, OUT_ROLLBACK));
    ensure_equals("second still running", m.running(), 1);
    ensure("wrong generation", !m.finalize(1, second + 5, R_CANCEL, OUT_COMMIT));
    ensure("unknown clone", !m.finalize(99, 0, R_CANCEL, OUT_COMMIT));
    ensure("current generation acts", m.finalize(1, second, R_COMPLETE, OUT_COMMIT));
    ensure_equals("two releases in total (one per record)", host.total(1), 2);
}

// 9. Clock: completes once at the duration; Freeze (zero delta) holds; invalid body rolls back.
template<> template<>
void alclonetransition_object::test<9>()
{
    DoubleHost host;
    Machine<Id, DoubleHost> m(host);
    const char* why = nullptr;
    m.start(1, 10, request(KIND_ENTITY_TO_OVERLAY_LOOK, 1.0), 1u, why);
    m.tick(0.0);
    m.tick(0.0);
    ensure("frozen: p unchanged", m.find(1) && m.find(1)->p == 0.f && m.find(1)->elapsed == 0.0);
    m.tick(0.5);
    ensure("halfway smooth p = 0.5", m.find(1) && std::fabs(m.find(1)->p - 0.5f) < 1e-6f);
    m.tick(0.25);
    ensure("still running before the end", m.running() == 1 && host.total(1) == 0);
    m.tick(0.25);
    ensure_equals("committed once", host.commits[1], 1);
    ensure_equals("record gone", m.running(), 0);
    m.tick(1.0);
    ensure_equals("no second commit", host.total(1), 1);
    // body goes invalid mid-run
    m.start(2, 20, request(), 2u, why);
    m.tick(0.3);
    host.bodyOk[2] = false;
    m.tick(0.1);
    ensure_equals("rolled back", host.rollbacks[2], 1);
    ensure_equals("no commit", host.commits[2], 0);
    // both invalid
    m.start(3, 30, request(), 3u, why);
    host.cloneOk[3] = false;
    host.bodyOk[3] = false;
    m.tick(0.1);
    ensure_equals("discarded", host.discards[3], 1);
}

// 10. finalizeAll (shutdown / ALL OFF / teleport): everything exactly once, safe to repeat.
template<> template<>
void alclonetransition_object::test<10>()
{
    DoubleHost host;
    Machine<Id, DoubleHost> m(host);
    const char* why = nullptr;
    m.start(1, 10, request(), 1u, why);
    m.start(2, 20, request(KIND_LOOK_TO_LOOK), 2u, why);
    m.finalizeAll(R_SHUTDOWN, OUT_DISCARD);
    m.finalizeAll(R_SHUTDOWN, OUT_DISCARD);
    ensure_equals("one discard each", host.discards[1] + host.discards[2], 2);
    ensure_equals("none running", m.running(), 0);
    m.start(1, 10, request(), 1u, why);
    m.start(2, 20, request(), 2u, why);
    m.finalizeAll(R_ALL_OFF, OUT_COMMIT);
    ensure_equals("ALL OFF snaps to the end", host.commits[1] + host.commits[2], 2);
}

// 11. Re-entrancy: a host callback that finalizes the same clone again must not release twice.
template<> template<>
void alclonetransition_object::test<11>()
{
    DoubleHost host;
    Machine<Id, DoubleHost> m(host);
    host.onFinalizeCallback = [&](const Record<Id>& r)
    {
        m.finalize(r.clone, 0, R_RECOVERY, OUT_ROLLBACK);
        m.finalize(r.clone, r.generation, R_CANCEL, OUT_COMMIT);
    };
    const char* why = nullptr;
    m.start(1, 10, request(), 1u, why);
    m.finalize(1, 0, R_CANCEL, OUT_ROLLBACK);
    ensure_equals("one release despite re-entry", host.total(1), 1);
    m.start(2, 20, request(), 2u, why);
    m.tick(10.0);
    ensure_equals("one release (completion) despite re-entry", host.total(2), 1);
}

// 12. Transition lock (D18): an edit rolls the transition back FIRST, then applies.
template<> template<>
void alclonetransition_object::test<12>()
{
    DoubleHost host;
    Machine<Id, DoubleHost> m(host);
    const char* why = nullptr;
    m.start(1, 10, request(), 1u, why);
    // the Studio edit pattern: cancelFor(id, R_EDIT_LOCK) then the edit itself
    m.finalize(1, 0, R_EDIT_LOCK, OUT_ROLLBACK);
    host.log.push_back("edit1");
    ensure("rollback happened before the edit", host.log.size() == 3 && host.log[1] == "rollback1" && host.log[2] == "edit1");
    // an edit with nothing running is free
    m.finalize(1, 0, R_EDIT_LOCK, OUT_ROLLBACK);
    ensure_equals("still one release", host.total(1), 1);
}

// 13. Hardening: seeds and records are per clone; two clones never share state.
template<> template<>
void alclonetransition_object::test<13>()
{
    DoubleHost host;
    Machine<Id, DoubleHost> m(host);
    const char* why = nullptr;
    m.start(1, 10, request(KIND_ENTITY_TO_OVERLAY_LOOK, 2.0), 11u, why);
    m.start(2, 20, request(KIND_LOOK_TO_LOOK, 1.0), 22u, why);
    m.tick(0.5);
    ensure("independent progress", m.find(1)->elapsed == 0.5 && m.find(2)->elapsed == 0.5);
    ensure("independent seeds", m.find(1)->seed == 11u && m.find(2)->seed == 22u);
    ensure("independent kinds", m.find(1)->kind == KIND_ENTITY_TO_OVERLAY_LOOK && m.find(2)->kind == KIND_LOOK_TO_LOOK);
    m.finalize(1, 0, R_CANCEL, OUT_ROLLBACK);
    ensure("other clone unaffected", m.find(2) && m.running() == 1);
}

// 14. Order: onStarted runs with the record ALREADY visible to the machine (running() == 1, find() works), so a host
// that reads the published running state during the callback (keepsBodyShown) sees this transition (fix M1).
template<> template<>
void alclonetransition_object::test<14>()
{
    DoubleHost host;
    Machine<Id, DoubleHost> m(host);
    int seenRunning = -1; bool seenRecord = false;
    host.onStartCallback = [&](const Record<Id>& r) { seenRunning = m.running(); seenRecord = m.find(r.clone) != nullptr; };
    const char* why = nullptr;
    ensure("start accepted", m.start(1, 10, request(KIND_OVERLAY_LOOK_TO_ENTITY), 1u, why) != 0);
    ensure_equals("running published inside onStarted", seenRunning, 1);
    ensure("record findable inside onStarted", seenRecord);
}

// 15. A refused request never modifies state (fix M2 / S7): an unknown kind, and a second start for a clone that is
// already running, leave the running record and the host untouched.
template<> template<>
void alclonetransition_object::test<15>()
{
    DoubleHost host;
    Machine<Id, DoubleHost> m(host);
    const char* why = nullptr;
    ensure("unknown kind refused", m.start(9, 90, request(7), 1u, why) == 0 && why && m.running() == 0);
    ensure("negative kind refused", m.start(9, 90, request(-1), 1u, why) == 0 && m.running() == 0);
    ensure_equals("no host callback for refusals", host.started.count(9), size_t(0));
    const uint32_t gen = m.start(1, 10, request(KIND_ENTITY_TO_OVERLAY_LOOK, 2.0), 5u, why);
    ensure("first start accepted", gen != 0);
    m.tick(0.5);
    ensure("duplicate refused", m.start(1, 10, request(KIND_ENTITY_TO_OVERLAY_LOOK, 4.0), 6u, why) == 0 && why);
    const Record<Id>* r = m.find(1);
    ensure("running record untouched by the refused duplicate",
           r && r->generation == gen && r->duration == 2.0 && r->seed == 5u && r->elapsed == 0.5);
    ensure_equals("onStarted not repeated", host.started[1], 1);
}
// 16. The manager clock (T1): a frozen world holds every transition (delta 0, no progress, no commit); otherwise
// presentation delta when the world-time drive owns the clock, else wall; non-finite / negative => 0; 0.25 s cap.
template<> template<>
void alclonetransition_object::test<16>()
{
    ensure_equals("frozen world: no advance (wall clock)", clockDelta(true, false, 0.0, 0.016), 0.0);
    ensure_equals("frozen world: no advance (presentation clock)", clockDelta(true, true, 0.016, 0.016), 0.0);
    ensure_equals("running world uses wall delta", clockDelta(false, false, 0.5, 0.016), 0.016);
    ensure_equals("world-time drive uses presentation delta", clockDelta(false, true, 0.008, 0.016), 0.008);
    ensure_equals("stalled frame capped", clockDelta(false, false, 0.0, 3.0), 0.25);
    ensure_equals("negative delta ignored", clockDelta(false, false, 0.0, -1.0), 0.0);
    ensure_equals("NaN delta ignored", clockDelta(false, false, 0.0, std::nan("")), 0.0);
    DoubleHost host;
    Machine<Id, DoubleHost> m(host);
    const char* why = nullptr;
    m.start(1, 10, request(KIND_ENTITY_TO_OVERLAY_LOOK, 1.0), 1u, why);
    for (int i = 0; i < 100; ++i) m.tick(clockDelta(true, false, 0.0, 0.5)); // frozen for 50 s of wall time
    ensure("a frozen transition neither progresses nor commits", m.find(1) && m.find(1)->elapsed == 0.0);
    for (int i = 0; i < 10; ++i) m.tick(clockDelta(false, false, 0.0, 0.25));
    ensure("thawed transition completes", !m.find(1));
}

// 17. System-part visibility (M6 / T2): the native avatar draw rule.
template<> template<>
void alclonetransition_object::test<17>()
{
    SystemPartInputs none;
    ensure("no system part visible: nothing to draw", !systemPartsDrawn(none));
    SystemPartInputs masked; // mesh body with alpha-masked system layers: every bake is IMG_INVISIBLE
    masked.skirtWorn = true;
    ensure("alpha-masked system body is NOT refused", !systemPartsDrawn(masked));
    SystemPartInputs bom; // Bakes on Mesh: the bakes are visible but the mesh body hides the system meshes
    bom.headBake = bom.upperBake = bom.lowerBake = bom.eyesBake = bom.hairBake = true;
    bom.bomHead = bom.bomUpper = bom.bomLower = bom.bomEyes = bom.bomHair = true;
    ensure("BOM mesh body (system meshes hidden) is NOT refused", !systemPartsDrawn(bom));
    SystemPartInputs visible; // a standard avatar body
    visible.upperBake = true;
    ensure("visible system body IS refused", systemPartsDrawn(visible));
    SystemPartInputs eyes; eyes.eyesBake = true;
    ensure("visible system eyes ARE refused", systemPartsDrawn(eyes));
    eyes.bomEyes = true;
    ensure("BOM-hidden eyes are not", !systemPartsDrawn(eyes));
    SystemPartInputs skirt; skirt.skirtBake = true;
    ensure("a skirt bake without the wearable draws nothing", !systemPartsDrawn(skirt));
    skirt.skirtWorn = true;
    ensure("a worn visible skirt is refused", systemPartsDrawn(skirt));
    SystemPartInputs invisible = visible; invisible.invisibleAppearance = true;
    ensure("an invisible avatar draws nothing", !systemPartsDrawn(invisible));
    SystemPartInputs partial = bom; partial.upperBake = true; partial.bomUpper = false; // only the upper mesh still draws
    ensure("one system part still drawn is enough to refuse", systemPartsDrawn(partial));
}
}
