/**
 * @file llsliderwheelcore_test.cpp
 * @brief [SliderWheel] Unit tests for the pure logic of the viewer-wide slider mouse wheel.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Design: doc/SLIDER_WHEEL_GLOBAL_DESIGN.md (v7) section 8 "Unit (pure, injected clock)".
 * Covers what needs no LLUI: nice125, plainStep, wheelStep, the resolution/participation table,
 * the guard (arm, 0.5 s expiry, keep-alive, owner == v, ancestor, non-ancestor, dead handle), the
 * settle state machine (one entry per slider, deadline push, invalidations, captured tick defers,
 * fire once, swap-out re-entrancy, close-flush selection and re-insertion).
 *
 * NOT covered here (needs a live LLFloater/LLSliderCtrl tree, i.e. the in-world or an ASan UI
 * harness): close re-entrancy (a)-(e), the callback-lifetime matrix (ctrl / child / ancestor
 * delete+die at every foreign-code point), LLSliderCtrl::setValue hardening, preview-only
 * finalisation counters, hosted tabs, cancelAllForQuit against real sliders, and the in-world list.
 */

#include "linden_common.h"

#include "../llsliderwheelcore.h"
#include "lltut.h"

using namespace LLSliderWheel;

namespace
{
// Minimal stand-ins for LLView / LLHandle<LLView>.
struct FakeNode
{
    FakeNode* parent = nullptr;
    FakeNode* getParent() const { return parent; }
};

struct FakeHandle
{
    FakeNode* node = nullptr;
    FakeNode* get() const { return node; } // null = the owner is gone
    bool operator==(const FakeHandle& o) const { return node == o.node; }
};

typedef GuardState<FakeHandle, FakeNode> fake_guard_t;
typedef SettleTable<FakeHandle>          fake_table_t;

FakeHandle H(FakeNode& n)
{
    FakeHandle h;
    h.node = &n;
    return h;
}
}

namespace tut
{
    struct LLSliderWheelCoreData
    {
    };

    typedef test_group<LLSliderWheelCoreData> factory;
    typedef factory::object object;
}

namespace
{
    tut::factory tf("LLSliderWheelCore");
}

namespace tut
{
    template<> template<>
    void object::test<1>()
    {
        // nice125: largest of {1,2,5} x 10^k <= x, minimum 1
        ensure_approximately_equals_range("0.5", nice125(0.5f), 1.f, 0.f);
        ensure_approximately_equals_range("1",   nice125(1.f), 1.f, 0.f);
        ensure_approximately_equals_range("1.9", nice125(1.9f), 1.f, 0.f);
        ensure_approximately_equals_range("2",   nice125(2.f), 2.f, 0.f);
        ensure_approximately_equals_range("4.9", nice125(4.9f), 2.f, 0.f);
        ensure_approximately_equals_range("5",   nice125(5.f), 5.f, 0.f);
        ensure_approximately_equals_range("9",   nice125(9.f), 5.f, 0.f);
        ensure_approximately_equals_range("60",  nice125(60.f), 50.f, 0.f);
        ensure_approximately_equals_range("1440", nice125(1440.f), 1000.f, 0.f);
        ensure_approximately_equals_range("0",   nice125(0.f), 1.f, 0.f);
        ensure_approximately_equals_range("-3",  nice125(-3.f), 1.f, 0.f);
    }

    template<> template<>
    void object::test<2>()
    {
        // plainStep: always a multiple of the increment
        ensure_approximately_equals_range("hover -3..3 / 0.001", plainStep(-3.f, 3.f, 0.001f), 0.05f, 1e-6f);
        ensure_approximately_equals_range("0..1 / 0.01",         plainStep(0.f, 1.f, 0.01f), 0.01f, 1e-7f);
        ensure_approximately_equals_range("-720..720 / 1",       plainStep(-720.f, 720.f, 1.f), 10.f, 0.f);
        ensure_approximately_equals_range("local offset -2..2 / 0.005", plainStep(-2.f, 2.f, 0.005f), 0.025f, 1e-6f);
        // exact-threshold quotients must not fall a step low through F32 rounding of the increment
        ensure_approximately_equals_range("0..20 / 0.1 -> 2 steps of 0.1", plainStep(0.f, 20.f, 0.1f), 0.2f, 1e-6f);
        ensure_approximately_equals_range("0..5 / 0.01 -> 5 steps",       plainStep(0.f, 5.f, 0.01f), 0.05f, 1e-6f);

        const F32 inc = 0.005f;
        const F32 k = plainStep(-2.f, 2.f, inc) / inc;
        ensure_approximately_equals_range("k*inc: k is an integer", k, (F32)(S32)(k + 0.5f), 1e-3f);
        ensure("bad increment returned unchanged", plainStep(0.f, 1.f, 0.f) == 0.f);
    }

    template<> template<>
    void object::test<3>()
    {
        // wheelStep. UNSET horizontal: nice step, Shift = increment (wins), Ctrl/Alt = x10.
        const F32 mn = -3.f, mx = 3.f, inc = 0.001f;
        ensure_approximately_equals_range("plain",  wheelStep(mn, mx, inc, false, false, false), 0.05f, 1e-6f);
        ensure_approximately_equals_range("shift",  wheelStep(mn, mx, inc, false, true,  false), 0.001f, 1e-7f);
        ensure_approximately_equals_range("ctrl",   wheelStep(mn, mx, inc, false, false, true),  0.5f, 1e-5f);
        ensure_approximately_equals_range("shift wins over ctrl", wheelStep(mn, mx, inc, false, true, true), 0.001f, 1e-7f);
        // legacy (explicit ON / vertical): increment, Ctrl/Alt x10, Shift ignored
        ensure_approximately_equals_range("legacy plain", wheelStep(mn, mx, inc, true, false, false), 0.001f, 1e-7f);
        ensure_approximately_equals_range("legacy ctrl",  wheelStep(mn, mx, inc, true, false, true),  0.01f, 1e-6f);
        ensure_approximately_equals_range("legacy shift ignored", wheelStep(mn, mx, inc, true, true, false), 0.001f, 1e-7f);
    }

    template<> template<>
    void object::test<4>()
    {
        // resolution / participation table: state x orientation x switch
        struct Row { EWheelAdjust st; bool vertical; bool sw; bool part; bool wheel; bool hwheel; };
        const Row rows[] = {
            // explicit ON ignores the switch
            { WHEEL_ON,    false, true,  true,  true,  true  },
            { WHEEL_ON,    false, false, true,  true,  true  },
            { WHEEL_ON,    true,  true,  true,  true,  true  },
            { WHEEL_ON,    true,  false, true,  true,  true  },
            // explicit OFF declines everything, whatever the orientation or switch
            { WHEEL_OFF,   false, true,  false, false, false },
            { WHEEL_OFF,   false, false, false, false, false },
            { WHEEL_OFF,   true,  true,  false, false, false },
            { WHEEL_OFF,   true,  false, false, false, false },
            // UNSET follows the switch; switch off = stock (vertical only takes the V-wheel)
            { WHEEL_UNSET, false, true,  true,  true,  true  },
            { WHEEL_UNSET, true,  true,  true,  true,  true  },
            { WHEEL_UNSET, false, false, false, false, true  },
            { WHEEL_UNSET, true,  false, false, true,  true  },
        };
        for (const Row& r : rows)
        {
            const Resolution res = resolve(r.st, r.vertical, r.sw);
            ensure_equals("participates", res.participates,  r.part);
            ensure_equals("wheelEnabled", res.wheelEnabled,  r.wheel);
            ensure_equals("hwheelEnabled", res.hwheelEnabled, r.hwheel);
        }
    }

    template<> template<>
    void object::test<5>()
    {
        // guard: arm, expiry, keep-alive, owner == v, ancestor, non-ancestor, dead handle
        FakeNode root, scroller, doc, slider, other;
        scroller.parent = &root;
        doc.parent = &scroller;
        slider.parent = &doc;
        other.parent = &root;

        fake_guard_t g;
        ensure("never armed: not guarded", !g.guarded(&slider, 100.0));

        g.note(H(scroller), 100.0);
        ensure("armed: slider under the scroller is guarded", g.guarded(&slider, 100.1));
        ensure("owner == v is guarded", g.guarded(&scroller, 100.1));
        ensure("an ancestor of the owner is not guarded (owner must be v or above v)", !g.guarded(&root, 100.1));
        ensure("a view in another branch is not guarded", !g.guarded(&other, 100.1));

        ensure("still guarded just under 0.5 s", g.guarded(&slider, 100.0 + GUARD_SEC - 0.001));
        ensure("expired at 0.5 s", !g.guarded(&slider, 100.0 + GUARD_SEC));

        // keep-alive extends from the keep-alive time
        g.keepAlive(100.4);
        ensure("kept alive past the original expiry", g.guarded(&slider, 100.8));
        ensure("expires 0.5 s after the keep-alive", !g.guarded(&slider, 100.9));

        // an outer scroller stamped: a slider in a nested inner scroller is guarded (any ancestor)
        g.note(H(root), 200.0);
        ensure("any-ancestor scope", g.guarded(&slider, 200.1));

        // a dead handle never guards
        FakeHandle dead;
        g.note(dead, 300.0);
        ensure("dead handle never guards", !g.guarded(&slider, 300.1));

        g.note(H(scroller), 400.0);
        g.reset();
        ensure("reset clears", !g.guarded(&slider, 400.1));
    }

    template<> template<>
    void object::test<6>()
    {
        // settle state machine (per slider)
        SettleState s;
        ensure("idle: nothing to consume", !s.consume(0));

        s.arm();
        ensure("pending after arm", s.pending);
        ensure("wrong generation does not consume", !s.consume(s.gen + 1));
        ensure("still pending", s.pending);
        const U32 armed_gen = s.gen;
        ensure("consumes once", s.consume(armed_gen));
        ensure("and only once", !s.consume(armed_gen));

        // every invalidation (capturing mouse-down, captured mouse-up, editor commit, dtor) cancels
        s.arm();
        const U32 g0 = s.gen;
        s.invalidate();
        ensure("invalidate bumps the generation", s.gen != g0);
        ensure("invalidate clears pending", !s.pending);
        ensure("stale generation never fires", !s.consume(g0));

        // re-arm after invalidation: only the new generation fires
        s.arm();
        ensure("new generation fires", s.consume(s.gen));

        ensure("schedule: normal", shouldSchedule(true, false, false));
        ensure("schedule: no mouse-up consumer", !shouldSchedule(false, false, false));
        ensure("schedule: captured (the drag's real mouse-up reports)", !shouldSchedule(true, true, false));
        ensure("schedule: quitting", !shouldSchedule(true, false, true));
    }

    template<> template<>
    void object::test<7>()
    {
        // registry: ONE entry per slider, a later notch moves the deadline
        FakeNode a, b;
        fake_table_t t;
        t.upsert(H(a), 1, 10.0);
        t.upsert(H(b), 1, 10.1);
        ensure_equals("two sliders, two entries", t.size(), (size_t)2);
        t.upsert(H(a), 1, 11.0);
        ensure_equals("same slider stays one entry", t.size(), (size_t)2);

        fake_table_t::Entry first = t.takeAll()[0];
        ensure("deadline was pushed", first.deadline == 11.0);
        ensure("takeAll swaps the table out", t.empty());

        // insertMerge: newest gen wins, tie keeps the existing (callback-added) entry
        t.upsert(H(a), 5, 20.0);
        fake_table_t::Entry old_entry; old_entry.handle = H(a); old_entry.gen = 4; old_entry.deadline = 10.0;
        t.insertMerge(old_entry);
        ensure_equals("re-insert of an older gen merges away", t.size(), (size_t)1);
        fake_table_t::Entry kept = t.takeAll()[0];
        ensure_equals("newer gen kept", kept.gen, (U32)5);
        ensure("newer deadline kept", kept.deadline == 20.0);

        fake_table_t::Entry newer; newer.handle = H(a); newer.gen = 6; newer.deadline = 30.0;
        t.upsert(H(a), 5, 20.0);
        t.insertMerge(newer);
        ensure_equals("re-insert of a newer gen replaces", t.takeAll()[0].gen, (U32)6);

        fake_table_t::Entry tie; tie.handle = H(a); tie.gen = 5; tie.deadline = 99.0;
        t.upsert(H(a), 5, 20.0);
        t.insertMerge(tie);
        ensure("tie keeps the existing entry", t.takeAll()[0].deadline == 20.0);
    }

    template<> template<>
    void object::test<8>()
    {
        // tick decision table
        const F64 now = 10.0, deadline = 10.3;
        ensure("dead slider drops",             tickDecision(false, true,  true,  false, now, deadline) == TickAction::Drop);
        ensure("generation mismatch drops",     tickDecision(true,  false, true,  false, now, deadline) == TickAction::Drop);
        ensure("not pending drops",             tickDecision(true,  true,  false, false, now, deadline) == TickAction::Drop);
        ensure("captured defers",               tickDecision(true,  true,  true,  true,  now, deadline) == TickAction::Defer);
        ensure("before the deadline waits",     tickDecision(true,  true,  true,  false, now, deadline) == TickAction::Wait);
        ensure("at the deadline fires",         tickDecision(true,  true,  true,  false, 10.3, deadline) == TickAction::Fire);
        ensure("after the deadline fires",      tickDecision(true,  true,  true,  false, 11.0, deadline) == TickAction::Fire);
        ensure("captured beats the deadline",   tickDecision(true,  true,  true,  true,  11.0, deadline) == TickAction::Defer);
    }

    template<> template<>
    void object::test<9>()
    {
        // Simulated lifecycle with the real state machine: notch, notch, tick, tick, fire once.
        FakeNode node;
        SettleState st;
        fake_table_t table;
        int fired = 0;
        bool captured = false;

        auto notch = [&](F64 t)
        {
            st.arm();
            table.upsert(H(node), st.gen, t + SETTLE_SEC);
        };
        // one timer tick: swap out, decide, fire / re-insert
        auto tick = [&](F64 t)
        {
            std::vector<fake_table_t::Entry> work = table.takeAll();
            for (fake_table_t::Entry& e : work)
            {
                switch (tickDecision(true, st.gen == e.gen, st.pending, captured, t, e.deadline))
                {
                case TickAction::Drop:  break;
                case TickAction::Defer: e.deadline = t + SETTLE_SEC; table.insertMerge(e); break;
                case TickAction::Wait:  table.insertMerge(e); break;
                case TickAction::Fire:  if (st.consume(e.gen)) ++fired; break;
                }
            }
        };

        notch(0.0);
        notch(0.1);
        notch(0.2);
        ensure_equals("one entry per slider after three notches", table.size(), (size_t)1);
        tick(0.3);
        ensure_equals("not yet: the last notch pushed the deadline", fired, 0);
        tick(0.45);
        ensure_equals("still waiting", fired, 0);
        tick(0.55);
        ensure_equals("fired exactly once", fired, 1);
        ensure("registry empty after the fire", table.empty());
        tick(0.6);
        tick(5.0);
        ensure_equals("never fires twice", fired, 1);

        // a drag (capturing mouse-down) cancels: the pending settle never fires
        notch(10.0);
        st.invalidate();
        tick(11.0);
        ensure_equals("cancelled by a capturing mouse-down / real mouse-up / editor commit", fired, 1);

        // a captured tick defers, then fires after release
        notch(20.0);
        captured = true;
        tick(21.0);
        ensure_equals("captured: deferred, not fired", fired, 1);
        ensure_equals("captured: entry kept", table.size(), (size_t)1);
        captured = false;
        tick(21.1);
        ensure_equals("deferred deadline not reached", fired, 1);
        tick(21.4);
        ensure_equals("fires once after release", fired, 2);

        // a callback that schedules again during the fire is merged, not lost (swap-out re-entrancy)
        notch(30.0);
        std::vector<fake_table_t::Entry> work = table.takeAll();
        table.upsert(H(node), st.gen + 1, 31.0);            // callback added a newer entry meanwhile
        table.insertMerge(work[0]);                         // the tick re-inserts its older copy
        ensure_equals("merged to one entry", table.size(), (size_t)1);
        ensure_equals("newest gen wins", table.takeAll()[0].gen, st.gen + 1);
    }

    template<> template<>
    void object::test<10>()
    {
        // close flush selection: descendants of the closing floater only; the rest stay registered.
        FakeNode floater, panel, in_floater, other_floater, in_other;
        panel.parent = &floater;
        in_floater.parent = &panel;
        in_other.parent = &other_floater;

        fake_table_t table;
        table.upsert(H(in_floater), 1, 1.0);
        table.upsert(H(in_other), 1, 1.0);
        table.upsert(H(floater), 1, 1.0);   // a slider that IS the root counts too

        std::vector<fake_table_t::Entry> mine = table.extractIf([&](const fake_table_t::Entry& e)
        {
            return isSelfOrAncestor<FakeNode>(&floater, e.handle.get());
        });
        ensure_equals("descendants + root selected", mine.size(), (size_t)2);
        ensure_equals("other floater's entry stays", table.size(), (size_t)1);
        ensure("and it is the right one", table.contains(H(in_other)));

        // superseded mid-flush: the unfired remainder goes back for the tick, nothing is lost
        table.insertMerge(mine[1]);
        ensure_equals("unfired entry re-inserted", table.size(), (size_t)2);
        ensure("re-inserted entry is registered", table.contains(mine[1].handle));

        // app_quitting: cancel = bump + never re-register
        SettleState s1, s2;
        s1.arm(); s2.arm();
        const U32 g1 = s1.gen, g2 = s2.gen;
        s1.invalidate(); s2.invalidate();
        ensure("quit cancel: neither fires", !s1.consume(g1) && !s2.consume(g2));
    }
}
