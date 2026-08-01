/**
 * @file aldirectorswitchermodel_test.cpp
 * @brief Adversarial tests for the deterministic Director switch scheduler.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../alcameracurve.h"
#include "../aldirectorswitchermodel.h"

#include <cmath>
#include <limits>
#include <vector>

namespace tut
{
    using namespace ALDirectorSwitcherModel;

    namespace
    {
        Config automaticConfig()
        {
            Config config;
            config.mAuto = true;
            config.mIntervalSeconds = 5.0;
            config.mSeed = 0x123456789abcdef0ULL;
            config.mEnabled[1] = true;
            config.mEnabled[4] = true;
            config.mEnabled[9] = true;
            return config;
        }

        void ensureSameFrame(
            const char* message, const Frame& expected, const Frame& actual)
        {
            ensure(message,
                   actual.mCut == expected.mCut &&
                   actual.mSlot == expected.mSlot &&
                   actual.mBoundary == expected.mBoundary &&
                   actual.mEventIndex == expected.mEventIndex &&
                   actual.mEventsElapsed == expected.mEventsElapsed);
        }
    }

    struct director_switcher_model_data {};
    typedef test_group<director_switcher_model_data>
        director_switcher_model_group;
    typedef director_switcher_model_group::object
        director_switcher_model_object;
    director_switcher_model_group director_switcher_model_tests(
        "ALDirectorSwitcherModel");

    // Numeric settings are finite and bounded at the model boundary.
    template<> template<>
    void director_switcher_model_object::test<1>()
    {
        Config input;
        input.mIntervalSeconds =
            std::numeric_limits<F64>::quiet_NaN();
        input.mJitterSeconds =
            std::numeric_limits<F64>::infinity();
        input.mSeed = 0;
        Config safe = sanitizeConfig(input);
        ensure("interval fallback",
               safe.mIntervalSeconds == DEFAULT_INTERVAL_SECONDS);
        ensure("infinite jitter fallback", safe.mJitterSeconds == 0.0);
        ensure("zero seed replaced", safe.mSeed != 0);

        input.mIntervalSeconds = 10.0;
        input.mJitterSeconds = 100.0;
        safe = sanitizeConfig(input);
        ensure("jitter capped at 45 percent",
               safe.mJitterSeconds == 4.5);

        input.mIntervalSeconds = -100.0;
        input.mJitterSeconds = -2.0;
        safe = sanitizeConfig(input);
        ensure("interval lower bound",
               safe.mIntervalSeconds == MIN_INTERVAL_SECONDS);
        ensure("negative jitter clamped", safe.mJitterSeconds == 0.0);
    }

    // Auto off and an empty enabled set are strict no-cut states.
    template<> template<>
    void director_switcher_model_object::test<2>()
    {
        Controller controller;
        Config config;
        controller.update(0.0, config);
        ensure("auto off after large time",
               !controller.update(1000000.0, config).mCut);
        ensure("no active slot", controller.activeSlot() == -1);

        config.mAuto = true;
        ensure("auto transition rebases",
               !controller.update(1000001.0, config).mCut);
        ensure("empty bank remains quiet",
               !controller.update(2000000.0, config).mCut);
    }

    // Boundaries are absolute, and evaluating one twice never repeats a cut.
    template<> template<>
    void director_switcher_model_object::test<3>()
    {
        Controller controller;
        Config config = automaticConfig();
        controller.update(0.0, config);
        ensure("before first boundary",
               !controller.update(4.999, config).mCut);
        const Frame first = controller.update(5.0, config);
        ensure("first boundary cuts", first.mCut);
        ensure("first event index", first.mEventIndex == 0);
        ensure("first elapsed count", first.mEventsElapsed == 1);
        ensure("exact first boundary", first.mBoundary == 5.0);
        ensure("same timestamp is idempotent",
               !controller.update(5.0, config).mCut);
    }

    // Sequence mode walks bank order, skipping disabled slots and wrapping.
    template<> template<>
    void director_switcher_model_object::test<4>()
    {
        Controller controller;
        Config config = automaticConfig();
        config.mSequence = true;
        controller.update(0.0, config);
        const S32 expected[] = { 1, 4, 9, 1, 4, 9 };
        for (U32 i = 0; i < 6; ++i)
        {
            const Frame frame =
                controller.update((i + 1) * 5.0, config);
            ensure("sequence boundary cuts", frame.mCut);
            ensure_equals("sequence skips disabled slots",
                          frame.mSlot, expected[i]);
        }
    }

    // A manual punch may target an auto-disabled slot and restarts the interval.
    template<> template<>
    void director_switcher_model_object::test<5>()
    {
        Controller controller;
        Config config = automaticConfig();
        config.mSequence = true;
        controller.update(0.0, config);
        ensure("manual disabled slot accepted",
               controller.manualPunch(3, 2.0));
        ensure("manual slot becomes active", controller.activeSlot() == 3);
        ensure("old boundary cancelled",
               !controller.update(5.0, config).mCut);
        const Frame next = controller.update(7.0, config);
        ensure("new boundary cuts", next.mCut);
        ensure("sequence resumes after manual slot", next.mSlot == 4);
        ensure("manual rebase resets event index", next.mEventIndex == 0);
        ensure("invalid slot rejected",
               !controller.manualPunch(12, 8.0));
        ensure("invalid time rejected",
               !controller.manualPunch(
                   2, std::numeric_limits<F64>::quiet_NaN()));
        ensure("invalid punch preserves active", controller.activeSlot() == 4);
    }

    // A backward presentation-time seek discards the old schedule.
    template<> template<>
    void director_switcher_model_object::test<6>()
    {
        Controller controller;
        Config config = automaticConfig();
        config.mSequence = true;
        controller.update(0.0, config);
        ensure("pre-seek cut", controller.update(5.0, config).mCut);
        ensure("seek frame rebases", !controller.update(2.0, config).mCut);
        ensure("old future boundary discarded",
               !controller.update(6.999, config).mCut);
        const Frame replay = controller.update(7.0, config);
        ensure("new post-seek boundary cuts", replay.mCut);
        ensure("post-seek index reset", replay.mEventIndex == 0);
    }

    // Effective config and auto-mode changes rebase instead of catching up.
    template<> template<>
    void director_switcher_model_object::test<7>()
    {
        Controller controller;
        Config config = automaticConfig();
        controller.update(0.0, config);
        controller.update(4.0, config);

        config.mSeed ^= 0x55aa55aaULL;
        ensure("seed edit rebases", !controller.update(4.0, config).mCut);
        ensure("edited schedule waits a full interval",
               !controller.update(8.999, config).mCut);
        ensure("edited schedule reaches boundary",
               controller.update(9.0, config).mCut);

        config.mAuto = false;
        ensure("auto off rebases", !controller.update(9.1, config).mCut);
        ensure("auto off cannot catch up",
               !controller.update(100.0, config).mCut);
        config.mAuto = true;
        ensure("auto on rebases", !controller.update(100.0, config).mCut);
        ensure("auto on waits full interval",
               !controller.update(104.999, config).mCut);
        ensure("auto resumes at new boundary",
               controller.update(105.0, config).mCut);
    }

    // Identical seeds/config/times produce bit-identical decisions.
    template<> template<>
    void director_switcher_model_object::test<8>()
    {
        Controller first;
        Controller second;
        Config config = automaticConfig();
        config.mJitterSeconds = 2.0;
        for (U32 tick = 0; tick <= 2000; ++tick)
        {
            const F64 now = tick * 0.05;
            ensureSameFrame(
                "matched controllers",
                first.update(now, config),
                second.update(now, config));
            ensure("matched active slots",
                   first.activeSlot() == second.activeSlot());
        }
    }

    // Random mode emits a full permutation per cycle without adjacent repeats.
    template<> template<>
    void director_switcher_model_object::test<9>()
    {
        Controller controller;
        Config config;
        config.mAuto = true;
        config.mIntervalSeconds = 1.0;
        config.mSeed = 0xfeedfacecafebeefULL;
        const S32 enabled[] = { 0, 2, 3, 7, 11 };
        for (S32 slot : enabled)
        {
            config.mEnabled[slot] = true;
        }
        controller.update(0.0, config);

        S32 previous = -1;
        for (U32 cycle = 0; cycle < 4; ++cycle)
        {
            bool seen[SLOT_COUNT] = {};
            for (U32 item = 0; item < 5; ++item)
            {
                const U32 event = cycle * 5 + item;
                const Frame frame =
                    controller.update(event + 1.0, config);
                ensure("random event cuts", frame.mCut);
                ensure("random slot enabled",
                       config.mEnabled[frame.mSlot]);
                ensure("slot appears once in cycle", !seen[frame.mSlot]);
                seen[frame.mSlot] = true;
                ensure("no adjacent repeat", frame.mSlot != previous);
                previous = frame.mSlot;
            }
            for (S32 slot : enabled)
            {
                ensure("cycle is full permutation", seen[slot]);
            }
        }

        Controller pair_controller;
        Config pair_config;
        pair_config.mAuto = true;
        pair_config.mIntervalSeconds = 1.0;
        pair_config.mEnabled[2] = true;
        pair_config.mEnabled[5] = true;
        pair_controller.update(0.0, pair_config);
        previous = -1;
        for (U32 event = 0; event < 8; ++event)
        {
            const Frame frame =
                pair_controller.update(event + 1.0, pair_config);
            ensure("two-slot event cuts", frame.mCut);
            ensure("two-slot boundary has no repeat",
                   frame.mSlot != previous);
            previous = frame.mSlot;
        }
    }

    // The first random auto event cannot repeat the manually punched framing.
    template<> template<>
    void director_switcher_model_object::test<10>()
    {
        Controller controller;
        Config config;
        config.mAuto = true;
        config.mIntervalSeconds = 1.0;
        config.mEnabled[1] = true;
        config.mEnabled[3] = true;
        config.mEnabled[5] = true;
        controller.update(0.0, config);
        ensure("manual punch accepted", controller.manualPunch(3, 0.25));
        const Frame first = controller.update(1.25, config);
        ensure("first random event cuts", first.mCut);
        ensure("first random event differs from manual", first.mSlot != 3);
    }

    // Boundary displacement is deterministic and never exceeds 45% interval.
    template<> template<>
    void director_switcher_model_object::test<11>()
    {
        Controller controller;
        Config config = automaticConfig();
        config.mIntervalSeconds = 10.0;
        config.mJitterSeconds = 100.0; // sanitized to 4.5
        controller.update(0.0, config);
        F64 previous_boundary = 0.0;
        for (U32 event = 0; event < 20; ++event)
        {
            const F64 nominal = (event + 1.0) * 10.0;
            const Frame frame =
                controller.update(nominal + 4.500001, config);
            ensure("jittered event cuts", frame.mCut);
            ensure("jittered event index",
                   frame.mEventIndex == event);
            ensure("boundary within jitter cap",
                   std::fabs(frame.mBoundary - nominal) <= 4.5);
            const F64 interval = frame.mBoundary - previous_boundary;
            ensure("actual interval respects negative jitter bound",
                   interval >= 5.5);
            ensure("actual interval respects positive jitter bound",
                   interval <= 14.5);
            previous_boundary = frame.mBoundary;
            ensure("one newly elapsed boundary",
                   frame.mEventsElapsed == 1);
        }
    }

    // A large hitch resolves directly to the same final event as fine updates.
    template<> template<>
    void director_switcher_model_object::test<12>()
    {
        Config config;
        config.mAuto = true;
        config.mIntervalSeconds = 1.0;
        config.mJitterSeconds = 0.45;
        config.mSeed = 0x0ddc0ffeebadf00dULL;
        for (S32 slot = 0; slot < 6; ++slot)
        {
            config.mEnabled[slot] = true;
        }

        Controller hitch;
        hitch.update(0.0, config);
        const Frame jumped = hitch.update(1000.0, config);
        ensure("hitch emits one final cut", jumped.mCut);
        ensure("hitch reports skipped events",
               jumped.mEventsElapsed == jumped.mEventIndex + 1ULL);
        ensure("same hitch time is idempotent",
               !hitch.update(1000.0, config).mCut);

        Controller fine;
        fine.update(0.0, config);
        Frame last;
        for (U32 tick = 1; tick <= 20000; ++tick)
        {
            const Frame frame = fine.update(tick * 0.05, config);
            if (frame.mCut)
            {
                last = frame;
            }
        }
        ensure("fine timeline produced cuts", last.mCut);
        ensure("hitch final event index matches fine",
               jumped.mEventIndex == last.mEventIndex);
        ensure("hitch final boundary matches fine",
               jumped.mBoundary == last.mBoundary);
        ensure("hitch final slot matches fine",
               jumped.mSlot == last.mSlot);
        ensure("hitch active slot matches fine",
               hitch.activeSlot() == fine.activeSlot());
    }

    // Invalid times never poison valid state; explicit rebase/reset are clean.
    template<> template<>
    void director_switcher_model_object::test<13>()
    {
        Controller controller;
        Config config = automaticConfig();
        config.mSequence = true;
        controller.update(0.0, config);
        ensure("NaN update rejected",
               !controller.update(
                   std::numeric_limits<F64>::quiet_NaN(), config).mCut);
        ensure("negative update rejected",
               !controller.update(-1.0, config).mCut);
        ensure("valid boundary survives invalid input",
               controller.update(5.0, config).mCut);

        ensure("explicit rebase accepted", controller.rebase(10.0));
        ensure("rebase cancels old timing",
               !controller.update(14.999, config).mCut);
        ensure("rebase reaches new boundary",
               controller.update(15.0, config).mCut);
        ensure("invalid rebase rejected",
               !controller.rebase(
                   std::numeric_limits<F64>::infinity()));

        controller.reset();
        ensure("reset clears active slot", controller.activeSlot() == -1);
        ensure("first update after reset rebases",
               !controller.update(100.0, config).mCut);
    }

    // A punch may establish the absolute anchor before the first config sample.
    template<> template<>
    void director_switcher_model_object::test<14>()
    {
        Controller controller;
        Config config = automaticConfig();
        config.mSequence = true;
        ensure("pre-config manual punch accepted",
               controller.manualPunch(3, 2.0));
        ensure("pre-config anchor is retained",
               !controller.update(6.999, config).mCut);
        const Frame frame = controller.update(7.0, config);
        ensure("pre-config anchor reaches boundary", frame.mCut);
        ensure("sequence advances from manual slot", frame.mSlot == 4);
        ensure("pre-config schedule begins at event zero",
               frame.mEventIndex == 0);
    }

    // Curve ids are persisted. Lock their representative values, exact
    // endpoints, and legacy-default arithmetic alongside the scheduler's
    // existing deterministic contract.
    template<> template<>
    void director_switcher_model_object::test<15>()
    {
        const F32 progress[] = { 0.25f, 0.5f, 0.75f };
        const F32 golden[33][3] = {
            { 0.103515625f, 0.500000000f, 0.896484375f },
            { 0.250000000f, 0.500000000f, 0.750000000f },
            { 0.408510593f, 0.802403388f, 0.960459073f },
            { 0.093464651f, 0.315356813f, 0.621861869f },
            { 0.378138131f, 0.684643187f, 0.906535349f },
            { 0.129161901f, 0.500000000f, 0.870838099f },
            { 0.062500000f, 0.250000000f, 0.562500000f },
            { 0.437500000f, 0.750000000f, 0.937500000f },
            { 0.125000000f, 0.500000000f, 0.875000000f },
            { 0.015625000f, 0.125000000f, 0.421875000f },
            { 0.578125000f, 0.875000000f, 0.984375000f },
            { 0.062500000f, 0.500000000f, 0.937500000f },
            { 0.003906250f, 0.062500000f, 0.316406250f },
            { 0.683593750f, 0.937500000f, 0.996093750f },
            { 0.031250000f, 0.500000000f, 0.968750000f },
            { 0.000976563f, 0.031250000f, 0.237304688f },
            { 0.762695313f, 0.968750000f, 0.999023438f },
            { 0.015625000f, 0.500000000f, 0.984375000f },
            { 0.005524272f, 0.031250000f, 0.176776695f },
            { 0.823223305f, 0.968750000f, 0.994475728f },
            { 0.015625000f, 0.500000000f, 0.984375000f },
            { 0.031754163f, 0.133974596f, 0.338562172f },
            { 0.661437828f, 0.866025404f, 0.968245837f },
            { 0.066987298f, 0.500000000f, 0.933012702f },
            { -0.064136563f, -0.087697500f, 0.182590312f },
            { 0.817409688f, 1.087697500f, 1.064136563f },
            { -0.099681844f, 0.500000000f, 1.099681844f },
            { -0.005524272f, -0.015625000f, 0.088388348f },
            { 0.911611652f, 1.015625000f, 1.005524272f },
            { 0.011969444f, 0.500000000f, 0.988030556f },
            { 0.065429688f, 0.367187500f, 0.711914063f },
            { 0.288085938f, 0.632812500f, 0.934570313f },
            { 0.183593750f, 0.500000000f, 0.816406250f },
        };

        for (S32 id = ALCameraCurve::SMOOTHERSTEP;
             id <= ALCameraCurve::BOUNCE_IN_OUT; ++id)
        {
            ensure_equals("preset start endpoint",
                          ALCameraCurve::eval(id, 0.f), 0.f);
            ensure_equals("preset end endpoint",
                          ALCameraCurve::eval(id, 1.f), 1.f);
            for (S32 sample = 0; sample < 3; ++sample)
            {
                ensure("preset golden sample",
                       fabsf(ALCameraCurve::eval(id, progress[sample]) -
                             golden[id][sample]) <= 0.00002f);
            }
        }

        for (F32 u : progress)
        {
            F32 legacy_u = llclamp(u, 0.f, 1.f);
            const F32 legacy = legacy_u * legacy_u * legacy_u *
                (legacy_u * (legacy_u * 6.f - 15.f) + 10.f);
            ensure_equals("id zero is bit-identical legacy smootherstep",
                          ALCameraCurve::eval(0, u), legacy);
            ensure_equals("linear preset is exact",
                          ALCameraCurve::eval(1, u), u);
            ensure_equals("identity bezier is exact",
                          ALCameraCurve::evalBezier(
                              u, 0.f, 0.f, 1.f, 1.f), u);
            ensure_equals("zero feather is exact unfeathered evaluation",
                          ALCameraCurve::evalFeathered(
                              ALCameraCurve::LINEAR, u,
                              0.42f, 0.f, 0.58f, 1.f, 0.f),
                          ALCameraCurve::eval(ALCameraCurve::LINEAR, u));
        }
        const F32 near_start = 0.01f;
        const F32 near_end = 0.99f;
        const F32 feathered_start = ALCameraCurve::evalFeathered(
            ALCameraCurve::LINEAR, near_start,
            0.42f, 0.f, 0.58f, 1.f, 1.f);
        const F32 feathered_end = ALCameraCurve::evalFeathered(
            ALCameraCurve::LINEAR, near_end,
            0.42f, 0.f, 0.58f, 1.f, 1.f);
        ensure("full feather reduces the linear start secant slope",
               feathered_start / near_start < 1.f);
        ensure("full feather reduces the linear end secant slope",
               (1.f - feathered_end) / (1.f - near_end) < 1.f);
        ensure_equals("feather leaves the selected curve midpoint exact",
                      ALCameraCurve::evalFeathered(
                          ALCameraCurve::BOUNCE_OUT, 0.5f,
                          0.42f, 0.f, 0.58f, 1.f, 1.f),
                      ALCameraCurve::eval(ALCameraCurve::BOUNCE_OUT, 0.5f));
        ensure_equals("feathered start endpoint is exact",
                      ALCameraCurve::evalFeathered(
                          ALCameraCurve::LINEAR, 0.f,
                          0.42f, 0.f, 0.58f, 1.f, 1.f), 0.f);
        ensure_equals("feathered end endpoint is exact",
                      ALCameraCurve::evalFeathered(
                          ALCameraCurve::LINEAR, 1.f,
                          0.42f, 0.f, 0.58f, 1.f, 1.f), 1.f);
        ensure_equals("custom start endpoint",
                      ALCameraCurve::eval(100, 0.f), 0.f);
        ensure_equals("custom end endpoint",
                      ALCameraCurve::eval(100, 1.f), 1.f);
        ensure_equals("unknown id falls back to smootherstep",
                      ALCameraCurve::eval(99, 0.25f),
                      ALCameraCurve::eval(0, 0.25f));
        ensure_equals("non-finite progress completes safely",
                      ALCameraCurve::eval(
                          0, std::numeric_limits<F32>::quiet_NaN()), 1.f);
        ensure_equals("non-finite feather is safely off",
                      ALCameraCurve::evalFeathered(
                          ALCameraCurve::LINEAR, 0.25f,
                          0.42f, 0.f, 0.58f, 1.f,
                          std::numeric_limits<F32>::quiet_NaN()),
                      ALCameraCurve::eval(ALCameraCurve::LINEAR, 0.25f));
        ensure("non-finite bezier controls are sanitized",
               std::isfinite(ALCameraCurve::evalBezier(
                   0.5f, std::numeric_limits<F32>::quiet_NaN(),
                   std::numeric_limits<F32>::infinity(),
                   -std::numeric_limits<F32>::infinity(),
                   std::numeric_limits<F32>::quiet_NaN())));
    }
}
