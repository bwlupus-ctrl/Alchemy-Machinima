/**
 * @file alworldwinddown_test.cpp
 * @brief Registered TUT integration tests of the production wall-time wind-down model.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"
#include "../test/lltut.h"
#include "../alworldwinddown.h"
#include "../alworldwinddowncontrol.h"

#include <cstdio>
#include <cstdlib>
#include <array>

namespace
{
void check(bool condition, const char* message)
{
    tut::ensure(message, condition);
}

void ensureClose(F32 actual, F32 expected, const char* message, F32 tolerance = .00002f)
{
    check(std::isfinite(actual) && fabsf(actual - expected) <= tolerance, message);
}
}

namespace tut
{
struct world_wind_down_data {};
typedef test_group<world_wind_down_data> world_wind_down_group;
typedef world_wind_down_group::object world_wind_down_object;
world_wind_down_group world_wind_down_tests("ALWorldWindDown");

template<> template<>
void world_wind_down_object::test<1>()
{
    using namespace ALWorldWindDown;
    for (S32 curve : { TURNTABLE, TAPE_STOP, CUSTOM })
    {
        for (Clock clock : { Clock{0, 3.75f}, Clock{1, .25f}, Clock{1, 8.f}, Clock{1, 0.f} })
        {
            Config config;
            config.mCurve = curve;
            Ramp ramp;
            ramp.toggle(10.0, clock, config);
            check(ramp.speed(10.0) == clock.effective(), "no jump at start");
            ensureClose(ramp.speed(10.0000001), clock.effective(), "continuous just after start");
            ensureClose(ramp.speed(11.4999999), 0.f, "continuous just before stop");
            F32 previous = clock.effective();
            for (S32 i = 1; i <= 1000; ++i)
            {
                const F64 now = 10.0 + 1.5 * static_cast<F64>(i) / 1000.0;
                const F32 speed = ramp.speed(now);
                check(speed <= previous + .000002f && speed >= 0.f, "monotone down");
                previous = speed;
                ramp.update(now);
            }
            ensureClose(ramp.speed(11.499999), 0.f, "continuous into stop");
            check(ramp.speed(11.5) == 0.f && ramp.state() == State::FROZEN,
                  "exact zero at stop time");
            check(ramp.speed(1000.0) == 0.f, "holds zero on advancing wall time");
            ramp.toggle(1000.0, clock, config);
            check(ramp.speed(1000.0) == 0.f, "spin-up starts at zero");
            ensureClose(ramp.speed(1000.0000001), 0.f, "continuous just after spin-up start");
            ensureClose(ramp.speed(1000.9999999), clock.effective(), "continuous just before restore");
            previous = 0.f;
            for (S32 i = 1; i <= 1000; ++i)
            {
                const F64 now = 1000.0 + static_cast<F64>(i) / 1000.0;
                const F32 speed = ramp.speed(now);
                check(speed + .000002f >= previous && speed <= clock.effective() + .000002f,
                      "monotone up");
                previous = speed;
                ramp.update(now);
            }
            check(!ramp.active() && ramp.speed(1001.0) == clock.effective(), "exact restore speed");
            check(ramp.restoreClock().mMode == clock.mMode &&
                  ramp.restoreClock().mScale == clock.mScale, "exact Live/Manual and dormant scale restore");
            ensureClose(ramp.speed(1000.999999), clock.effective(), "continuous after release");
        }

        Config config;
        config.mCurve = curve;
        const Clock clock{1, 2.5f};
        Ramp reversing;
        reversing.toggle(0.0, clock, config);
        const F32 before = reversing.speed(.63);
        reversing.toggle(.63, clock, config);
        check(reversing.speed(.63) == before, "down-to-up reversal exact continuity");
        ensureClose(reversing.speed(.630001), before, "continuous after reversal");
        const F32 back = reversing.speed(.91);
        reversing.toggle(.91, clock, config);
        check(reversing.speed(.91) == back, "up-to-down reversal exact continuity");
        check(reversing.speed(2.41) == 0.f, "reversed down reaches exact zero");
        reversing.update(2.41);
        reversing.toggle(2.5, clock, config);
        ensureClose(reversing.speed(3.499999), 2.5f, "continuous into restore");
        check(reversing.speed(3.5) == 2.5f, "reversal retains original restore target");

        Ramp fps30, fps144;
        fps30.toggle(20.0, clock, config);
        fps144.toggle(20.0, clock, config);
        std::array<F32, 46> down30{};
        std::array<F32, 217> down144{};
        for (size_t i = 0; i < down30.size(); ++i)
        {
            const F64 now = 20.0 + static_cast<F64>(i) / 30.0;
            fps30.update(now);
            down30[i] = fps30.speed(now);
        }
        for (size_t i = 0; i < down144.size(); ++i)
        {
            const F64 now = 20.0 + static_cast<F64>(i) / 144.0;
            fps144.update(now);
            down144[i] = fps144.speed(now);
        }
        // Every sixth of a second is common to the two independently sampled runs.
        for (size_t i = 0; i <= 9; ++i)
            check(down30[i * 5] == down144[i * 24], "30/144 fps sample independence");
        check(fps30.speed(21.5) == 0.f && fps144.speed(21.5) == 0.f, "both frame rates stop exactly");
        fps30.toggle(30.0, clock, config);
        fps144.toggle(30.0, clock, config);
        std::array<F32, 31> up30{};
        std::array<F32, 145> up144{};
        for (size_t i = 0; i < up30.size(); ++i)
        {
            const F64 now = 30.0 + static_cast<F64>(i) / 30.0;
            fps30.update(now);
            up30[i] = fps30.speed(now);
        }
        for (size_t i = 0; i < up144.size(); ++i)
        {
            const F64 now = 30.0 + static_cast<F64>(i) / 144.0;
            fps144.update(now);
            up144[i] = fps144.speed(now);
        }
        for (size_t i = 0; i <= 6; ++i)
            check(up30[i * 5] == up144[i * 24], "spin-up sample independence");
        check(!fps30.active() && !fps144.active(), "wall time releases frozen world");

        Ramp mirrored;
        mirrored.toggle(0.0, clock, config);
        for (S32 i = 0; i <= 100; ++i)
        {
            const F32 u = static_cast<F32>(i) / 100.f;
            const F32 down = mirrored.speed(static_cast<F64>(1.f - u) * 1.5);
            Ramp up;
            up.toggle(0.0, clock, config);
            up.update(1.5);
            up.toggle(1.5, clock, config);
            ensureClose(up.speed(1.5 + static_cast<F64>(u)), down, "spin-up mirrors down curve");
        }
        reversing.cancel();
        check(!reversing.active(), "external clock edit cancels envelope");
    }

    Config custom;
    custom.mCurve = CUSTOM;
    for (ALCameraCurve::Bezier handles : {
             ALCameraCurve::Bezier{.1f, .9f, .7f, .2f},
             ALCameraCurve::Bezier{0.f, -2.f, 0.f, 3.f},
             ALCameraCurve::Bezier{1.f, 3.f, 1.f, -2.f} })
    {
        custom.mX1 = handles.mX1; custom.mY1 = handles.mY1;
        custom.mX2 = handles.mX2; custom.mY2 = handles.mY2;
        for (S32 i = 0; i <= 1000; ++i)
        {
            const F32 u = static_cast<F32>(i) / 1000.f;
            const F32 switcher = llclamp(ALCameraCurve::evalFeathered(
                ALCameraCurve::CUSTOM_BEZIER, u, custom.mX1, custom.mY1,
                custom.mX2, custom.mY2, 0.f), 0.f, 1.f);
            check(reduction(custom, u) == switcher, "Custom matches switcher evaluator and clamp");
        }
    }
    Config turntable, tape;
    tape.mCurve = TAPE_STOP;
    check(reduction(turntable, .5f) < .5f, "turntable starts gently");
    check(reduction(tape, .5f) > .9f, "tape drops quickly and crawls");

    Ramp latched;
    latched.toggle(0.0, Clock{}, turntable);
    const F32 original = latched.speed(.7);
    turntable.mCurve = TAPE_STOP;
    turntable.mDownSeconds = 10.f;
    check(latched.speed(.7) == original, "live configuration edits cannot jump a latched leg");
    check(seconds(-1.f, 1.f) == .2f && seconds(100.f, 1.f) == 10.f, "duration bounds");
    std::puts("PASS: world wind-down continuity, exact endpoints, monotonicity, Bezier parity, reversals, restore, fps independence, wall time, cancellation and latching");
}

// The production binding observes real controls, not a change-signal mock.
template<> template<>
void world_wind_down_object::test<2>()
{
    using namespace ALWorldWindDown;
    LLControlVariable mode("TemporalMode", TYPE_S32, LLSD(0), "fixture");
    LLControlVariable scale("TemporalWorldScale", TYPE_F32, LLSD(1.0), "fixture");
    Ramp ramp;
    S32 writes = 0;
    S32 changes = 0;
    auto cancel = [&]() { ++writes; ramp.cancel(); };
    boost::signals2::scoped_connection mode_write = observeClockWrites(&mode, cancel);
    boost::signals2::scoped_connection scale_write = observeClockWrites(&scale, cancel);
    boost::signals2::scoped_connection mode_change = mode.getSignal()->connect(
        [&](LLControlVariable*, const LLSD&, const LLSD&) { ++changes; });
    boost::signals2::scoped_connection scale_change = scale.getSignal()->connect(
        [&](LLControlVariable*, const LLSD&, const LLSD&) { ++changes; });
    const std::function<void()> operations[] = {
        [&]() { mode.setValue(LLSD(0)); },
        [&]() { scale.setValue(LLSD(1.0)); },
        [&]() { mode.setValue(LLSD(0), false); },
        [&]() { scale.setValue(LLSD(1.0), false); },
        [&]() { mode.resetToDefault(true); },
        [&]() { scale.resetToDefault(true); },
        [&]() { mode.resetToDefault(); }, // event API / script reset (no change signal)
        [&]() { scale.resetToDefault(); },
        [&]() { mode.setDefaultValue(LLSD(0)); },
        [&]() { scale.setDefaultValue(LLSD(1.0)); }
    };
    for (const auto& write : operations)
    {
        ramp.toggle(0.0, Clock{}, Config{});
        ramp.update(1.5);
        check(ramp.state() == State::FROZEN, "same-value scene fixture starts frozen");
        const S32 before = writes;
        write();
        check(!ramp.active(), "every same-value clock write/reset cancels");
        check(writes == before + 1, "one intent per explicit write, no nested reset notification");
        check(changes == 0, "existing change-only signal contract is unchanged");
    }
    check(!observeClockWrites(nullptr, cancel).connected(), "missing control binding is safe");
    boost::signals2::scoped_connection reject = mode.getValidateSignal()->connect(
        [](LLControlVariable*, const LLSD&) { return false; });
    ramp.toggle(0.0, Clock{}, Config{});
    mode.setValue(LLSD(1));
    check(ramp.active() && mode.getValue().asInteger() == 0, "rejected write preserves overlay and value");
    reject.disconnect();
    const S32 before = writes;
    mode.setValue(LLSD(1));
    check(!ramp.active() && writes == before + 1 && changes == 1, "changed writes notify once too");
    ramp.toggle(0.0, Clock{}, Config{});
    mode.resetToDefault(true);
    check(!ramp.active() && changes == 2, "changed reset keeps original change notification");
}

// Deterministic lease fixture uses the same start guard as the real switcher
// and real controls/binding for its transient and restore writes.
template<> template<>
void world_wind_down_object::test<3>()
{
    using namespace ALWorldWindDown;
    for (Clock original : {Clock{0, 3.75f}, Clock{1, .5f}})
    {
        for (F32 factor : {0.f, .15f})
        {
            LLControlVariable mode("TemporalMode", TYPE_S32, LLSD(original.mMode), "fixture");
            LLControlVariable scale("TemporalWorldScale", TYPE_F32, LLSD(original.mScale), "fixture");
            Ramp ramp;
            boost::signals2::scoped_connection mode_write = observeClockWrites(&mode, [&]() { ramp.cancel(); });
            boost::signals2::scoped_connection scale_write = observeClockWrites(&scale, [&]() { ramp.cancel(); });
            Clock saved;
            bool ease_active = false;
            auto start_cut = [&]()
            {
                if (!switcherMayEase(ramp.state())) return false;
                saved = {mode.getValue().asInteger(), static_cast<F32>(scale.getValue().asReal())};
                ease_active = true;
                scale.setValue(LLSD(static_cast<F64>(original.effective() * factor)), false);
                mode.setValue(LLSD(1), false);
                return true;
            };
            ramp.toggle(0.0, original, Config{});
            const F32 winding_speed = ramp.speed(.5);
            check(!start_cut() && ramp.speed(.5) == winding_speed && !ease_active,
                  "wind-down first: mid-ramp cut never acquires or jumps clock");
            check(saved.mMode == 0 && saved.mScale == 1.f, "skipped cut never latches a baseline");
            ramp.update(1.5);
            check(!start_cut() && ramp.speed(2.0) == 0.f && ramp.state() == State::FROZEN,
                  "wind-down first: frozen cut never thaws world");
            ramp.toggle(2.0, original, Config{});
            check(!start_cut(), "spinning-up also owns the clock");
            ramp.update(3.0);
            check(start_cut(), "switcher can acquire after wind-down releases");
            const F32 published_speed = original.effective() * factor;
            // Same ordering as releaseWorldTimeForWindDown: keep published
            // speed + saved baseline, restore all cut settings, then acquire.
            scale.setValue(LLSD(static_cast<F64>(saved.mScale)), false);
            mode.setValue(LLSD(saved.mMode), false);
            ease_active = false;
            ramp.toggle(4.0, saved, Config{}, published_speed);
            check(ramp.speed(4.0) == published_speed, "cut first: handoff has no speed jump");
            check(!ease_active && !start_cut(), "cut first: released cut cannot reacquire mid-ramp");
            ramp.update(5.5);
            check(!start_cut() && ramp.speed(5.5) == 0.f, "cut first: wind-down holds freeze");
            ramp.toggle(6.0, saved, Config{});
            ramp.update(7.0);
            check(ramp.speed(7.0) == original.effective(), "cut first: restores baseline, never transient scale");
            check(ramp.restoreClock().mMode == original.mMode && ramp.restoreClock().mScale == original.mScale,
                  "cut first: original Live/Manual and dormant scale restored exactly");
            check(mode.getValue().asInteger() == original.mMode &&
                  static_cast<F32>(scale.getValue().asReal()) == original.mScale,
                  "handoff leaves underlying clock settings at switcher baseline");
        }
    }
}

template<> template<>
void world_wind_down_object::test<4>()
{
    using namespace ALWorldWindDown;
    check(alreadyFrozen(Clock{1, 0.f}), "manual zero is already frozen");
    check(!alreadyFrozen(Clock{0, 0.f}), "Live with dormant zero is not frozen");
    check(!alreadyFrozen(Clock{1, .25f}), "nonzero manual speed may wind down");
}
} // namespace tut
