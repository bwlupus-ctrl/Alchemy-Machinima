/**
 * @file alweathermodel_test.cpp
 * @brief Adversarial tests for deterministic weather/lightning state.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../alweathermodel.h"

#include <cmath>
#include <limits>
#include <vector>

namespace tut
{
    using namespace ALWeatherModel;

    namespace
    {
        Config enabledConfig()
        {
            Config config;
            config.mEnabled = true;
            config.mRatePerMinute = 60.0;
            config.mSeed = 0x123456789abcdef0ULL;
            return config;
        }

        Position anchor()
        {
            return Position{ 128.0, 128.0, 24.0 };
        }

        void ensureFiniteFrame(const char* message, const Frame& frame)
        {
            ensure(message, std::isfinite(frame.mFlash) &&
                            std::isfinite(frame.mBolt) &&
                            std::isfinite(frame.mQualityBolt) &&
                            std::isfinite(frame.mAfterglow) &&
                            std::isfinite(frame.mColorVariation) &&
                            std::isfinite(frame.mStrikeTime) &&
                            frame.mFlash >= 0.f && frame.mFlash <= 1.f &&
                            frame.mBolt >= 0.f && frame.mBolt <= 1.f &&
                            frame.mQualityBolt >= 0.f &&
                            frame.mQualityBolt <= 1.f &&
                            frame.mAfterglow >= 0.f &&
                            frame.mAfterglow <= 1.f &&
                            frame.mColorVariation >= -1.f &&
                            frame.mColorVariation <= 1.f &&
                            frame.mStrokeIndex <= 3 &&
                            isFinite(frame.mStrikeBase) &&
                            std::isfinite(frame.mStrikeDistanceMeters) &&
                            std::isfinite(frame.mBoltHeightMeters));
        }

        struct StrikeSample
        {
            F64 mTime;
            Position mPosition;
        };

        std::vector<StrikeSample> runTimeline(
            Controller& controller, const Config& config,
            S32 frames_per_second, F64 duration)
        {
            std::vector<StrikeSample> strikes;
            U64 previous_id = 0;
            const S32 frame_count =
                static_cast<S32>(duration * frames_per_second);
            for (S32 frame = 0; frame <= frame_count; ++frame)
            {
                const F64 now =
                    static_cast<F64>(frame) / frames_per_second;
                const Frame result =
                    controller.update(now, config, anchor());
                const U64 current_id = controller.strikeCount();
                if (current_id != previous_id)
                {
                    strikes.push_back(
                        { result.mStrikeTime, result.mStrikeBase });
                    previous_id = current_id;
                }
            }
            return strikes;
        }

        void ensureSameStrikes(const char* message,
                               const std::vector<StrikeSample>& expected,
                               const std::vector<StrikeSample>& actual)
        {
            ensure_equals(message, actual.size(), expected.size());
            for (size_t i = 0; i < expected.size(); ++i)
            {
                ensure(message, actual[i].mTime == expected[i].mTime &&
                                actual[i].mPosition.mX ==
                                    expected[i].mPosition.mX &&
                                actual[i].mPosition.mY ==
                                    expected[i].mPosition.mY &&
                                actual[i].mPosition.mZ ==
                                    expected[i].mPosition.mZ);
            }
        }

        void ensureSameFrame(const char* message,
                             const Frame& expected, const Frame& actual)
        {
            ensure(message,
                   actual.mActive == expected.mActive &&
                   actual.mStrikeStarted == expected.mStrikeStarted &&
                   actual.mFlash == expected.mFlash &&
                   actual.mBolt == expected.mBolt &&
                   actual.mQualityBolt == expected.mQualityBolt &&
                   actual.mAfterglow == expected.mAfterglow &&
                   actual.mColorVariation == expected.mColorVariation &&
                   actual.mStrokeIndex == expected.mStrokeIndex &&
                   actual.mVisualSeed == expected.mVisualSeed &&
                   actual.mStrikeTime == expected.mStrikeTime &&
                   actual.mStrikeBase.mX == expected.mStrikeBase.mX &&
                   actual.mStrikeBase.mY == expected.mStrikeBase.mY &&
                   actual.mStrikeBase.mZ == expected.mStrikeBase.mZ &&
                   actual.mStrikeDistanceMeters ==
                       expected.mStrikeDistanceMeters &&
                   actual.mBoltHeightMeters == expected.mBoltHeightMeters &&
                   actual.mStrikeId == expected.mStrikeId);
        }
    }

    struct weather_model_data {};
    typedef test_group<weather_model_data> weather_model_group;
    typedef weather_model_group::object weather_model_object;
    weather_model_group weather_model_tests("ALWeatherModel");

    // Every numeric setting is sanitized at the model boundary, including
    // values that cannot be produced by the normal UI.
    template<> template<>
    void weather_model_object::test<1>()
    {
        Config input;
        input.mEnabled = true;
        input.mRatePerMinute = std::numeric_limits<F64>::quiet_NaN();
        input.mFlashDurationSeconds =
            std::numeric_limits<F64>::infinity();
        input.mBoltDurationSeconds = -100.0;
        input.mMinDistanceMeters = 9000.0;
        input.mMaxDistanceMeters = -42.0;
        input.mBoltHeightMeters =
            -std::numeric_limits<F64>::infinity();
        input.mQualityAfterglowStrength =
            std::numeric_limits<F64>::quiet_NaN();
        input.mSeed = 0;

        const Config safe = sanitizeConfig(input);
        ensure("rate finite and bounded",
               std::isfinite(safe.mRatePerMinute) &&
               safe.mRatePerMinute >= 0.0 &&
               safe.mRatePerMinute <= MAX_RATE_PER_MINUTE);
        ensure("flash duration finite and bounded",
               std::isfinite(safe.mFlashDurationSeconds) &&
               safe.mFlashDurationSeconds >= 0.05 &&
               safe.mFlashDurationSeconds <= MAX_DURATION_SECONDS);
        ensure("bolt duration follows flash duration",
               safe.mBoltDurationSeconds >= 0.02 &&
               safe.mBoltDurationSeconds <= safe.mFlashDurationSeconds);
        ensure("distance interval ordered",
               safe.mMinDistanceMeters <= safe.mMaxDistanceMeters &&
               safe.mMaxDistanceMeters <= MAX_DISTANCE_METERS);
        ensure("height finite and bounded",
               safe.mBoltHeightMeters >= 16.0 &&
               safe.mBoltHeightMeters <= MAX_BOLT_HEIGHT_METERS);
        ensure("quality afterglow finite and bounded",
               std::isfinite(safe.mQualityAfterglowStrength) &&
               safe.mQualityAfterglowStrength >= 0.0 &&
               safe.mQualityAfterglowStrength <=
                   MAX_QUALITY_AFTERGLOW_STRENGTH);
        ensure("zero seed replaced", safe.mSeed != 0);
    }

    // Disabled weather is a strict no-op and rejects manual triggers.
    template<> template<>
    void weather_model_object::test<2>()
    {
        Controller controller;
        Config config;
        const Frame frame = controller.update(1.0, config, anchor());
        ensure("disabled frame inactive", !frame.mActive);
        ensure("disabled flash zero", frame.mFlash == 0.f);
        ensure("disabled bolt zero", frame.mBolt == 0.f);
        ensure("disabled trigger rejected",
               !controller.trigger(1.0, config, anchor()));
        ensure("no strikes counted", controller.strikeCount() == 0);
    }

    // Same seed and timeline must produce byte-stable strike decisions.
    template<> template<>
    void weather_model_object::test<3>()
    {
        Controller first;
        Controller second;
        const Config config = enabledConfig();
        const Position origin = anchor();

        for (S32 i = 0; i < 200; ++i)
        {
            const F64 now = static_cast<F64>(i) * 0.1;
            const Frame a = first.update(now, config, origin);
            const Frame b = second.update(now, config, origin);
            ensure("active deterministic", a.mActive == b.mActive);
            ensure("start deterministic",
                   a.mStrikeStarted == b.mStrikeStarted);
            ensure("strike id deterministic", a.mStrikeId == b.mStrikeId);
            ensure("flash deterministic", a.mFlash == b.mFlash);
            ensure("bolt deterministic", a.mBolt == b.mBolt);
            ensure("quality bolt deterministic",
                   a.mQualityBolt == b.mQualityBolt);
            ensure("afterglow deterministic",
                   a.mAfterglow == b.mAfterglow);
            ensure("x deterministic",
                   a.mStrikeBase.mX == b.mStrikeBase.mX);
            ensure("y deterministic",
                   a.mStrikeBase.mY == b.mStrikeBase.mY);
        }
        ensure("timeline produced strikes", first.strikeCount() > 0);
    }

    // NaN/Inf time or positions fail closed and do not poison later valid work.
    template<> template<>
    void weather_model_object::test<4>()
    {
        Controller controller;
        Config config = enabledConfig();
        Position invalid = anchor();
        invalid.mX = std::numeric_limits<F64>::quiet_NaN();

        ensure("NaN time inactive",
               !controller.update(
                    std::numeric_limits<F64>::quiet_NaN(),
                    config, anchor()).mActive);
        ensure("infinite time inactive",
               !controller.update(
                    std::numeric_limits<F64>::infinity(),
                    config, anchor()).mActive);
        ensure("negative time inactive",
               !controller.update(-1.0, config, anchor()).mActive);
        ensure("NaN anchor inactive",
               !controller.update(0.0, config, invalid).mActive);
        ensure("NaN manual trigger rejected",
               !controller.trigger(0.0, config, invalid));

        ensure("valid trigger still succeeds",
               controller.trigger(1.0, config, anchor()));
        const Frame recovered = controller.update(1.0, config, anchor());
        ensure("state recovered", recovered.mActive);
        ensureFiniteFrame("recovered frame finite", recovered);
    }

    // A clock seek/reversal discards the old schedule; negative strike ages
    // never reach the output.
    template<> template<>
    void weather_model_object::test<5>()
    {
        Controller controller;
        Config config = enabledConfig();
        controller.update(100.0, config, anchor());
        controller.trigger(101.0, config, anchor());
        ensure("trigger active",
               controller.update(101.0, config, anchor()).mActive);

        const Frame reversed = controller.update(2.0, config, anchor());
        ensure("reversal clears old strike", !reversed.mActive);
        ensureFiniteFrame("reversed output finite", reversed);
    }

    // A huge hitch starts no more than one catch-up strike in the resumed frame.
    template<> template<>
    void weather_model_object::test<6>()
    {
        Controller controller;
        Config config = enabledConfig();
        controller.update(0.0, config, anchor());
        const U64 before = controller.strikeCount();
        const Frame resumed =
            controller.update(24.0 * 60.0 * 60.0, config, anchor());
        ensure("one strike after huge jump",
               controller.strikeCount() == before + 1);
        ensure("strike is marked started", resumed.mStrikeStarted);
        ensureFiniteFrame("hitch output finite", resumed);
    }

    // Manual pulse is bounded, begins immediately, and expires cleanly.
    template<> template<>
    void weather_model_object::test<7>()
    {
        Controller controller;
        Config config = enabledConfig();
        config.mRatePerMinute = 0.0;
        config.mFlashDurationSeconds = 0.35;
        config.mBoltDurationSeconds = 0.18;

        ensure("manual trigger accepted",
               controller.trigger(10.0, config, anchor()));
        const Frame start = controller.update(10.0, config, anchor());
        ensure("manual strike active at t0", start.mActive);
        ensure("flash begins at full intensity", start.mFlash == 1.f);
        ensure("bolt visible at t0", start.mBolt > 0.f);
        ensureFiniteFrame("start finite", start);

        const Frame after_bolt =
            controller.update(10.19, config, anchor());
        ensure("bolt expires independently", after_bolt.mBolt == 0.f);
        ensure("flash can remain active", after_bolt.mFlash > 0.f);

        const Frame expired =
            controller.update(10.36, config, anchor());
        ensure("manual strike expires", !expired.mActive);
        ensureFiniteFrame("expired finite", expired);
    }

    // Strike samples stay inside the configured annulus and retain the anchor Z.
    template<> template<>
    void weather_model_object::test<8>()
    {
        Controller controller;
        Config config = enabledConfig();
        config.mRatePerMinute = 0.0;
        config.mMinDistanceMeters = 40.0;
        config.mMaxDistanceMeters = 80.0;
        const Position origin = anchor();

        for (S32 i = 0; i < 64; ++i)
        {
            ensure("trigger accepted",
                   controller.trigger(static_cast<F64>(i), config, origin));
            const Frame frame =
                controller.update(static_cast<F64>(i), config, origin);
            const F64 dx = frame.mStrikeBase.mX - origin.mX;
            const F64 dy = frame.mStrikeBase.mY - origin.mY;
            const F64 distance = std::hypot(dx, dy);
            ensure("inside minimum radius", distance >= 40.0 - 1e-9);
            ensure("inside maximum radius", distance <= 80.0 + 1e-9);
            ensure("anchor z preserved",
                   frame.mStrikeBase.mZ == origin.mZ);
            ensureFiniteFrame("annulus frame finite", frame);
        }
    }

    // Seed zero cannot enter xorshift's absorbing state.
    template<> template<>
    void weather_model_object::test<9>()
    {
        Controller controller;
        Config config = enabledConfig();
        config.mSeed = 0;
        config.mRatePerMinute = 0.0;
        const Position origin = anchor();

        controller.trigger(0.0, config, origin);
        const Frame first = controller.update(0.0, config, origin);
        controller.trigger(1.0, config, origin);
        const Frame second = controller.update(1.0, config, origin);
        ensure("zero-seed strikes vary",
               first.mStrikeBase.mX != second.mStrikeBase.mX ||
               first.mStrikeBase.mY != second.mStrikeBase.mY);
    }

    // Live rate edits must replace the old schedule. In particular, zero must
    // cancel it and zero-to-nonzero must not remain dormant or catch up at once.
    template<> template<>
    void weather_model_object::test<10>()
    {
        Controller controller;
        Config config = enabledConfig();
        controller.update(0.0, config, anchor());

        config.mRatePerMinute = 0.0;
        controller.update(1.0, config, anchor());
        const Frame quiet = controller.update(5000.0, config, anchor());
        ensure("zero rate cancels scheduled strike", !quiet.mActive);
        ensure("zero rate counted no strike", controller.strikeCount() == 0);

        config.mRatePerMinute = 60.0;
        const Frame resumed = controller.update(5001.0, config, anchor());
        ensure("reenable does not catch up immediately",
               !resumed.mStrikeStarted);

        const Frame eventual = controller.update(9002.0, config, anchor());
        ensure("reenabled rate schedules a strike", eventual.mStrikeStarted);
        ensure("only one strike after long jump", controller.strikeCount() == 1);
        ensureFiniteFrame("rate-edit frame finite", eventual);
    }

    // Small per-frame modulation (for example interpolating EEP cloud cover)
    // must not continuously push the next event into the future.
    template<> template<>
    void weather_model_object::test<11>()
    {
        Controller controller;
        Config config = enabledConfig();
        controller.update(0.0, config, anchor());

        Frame frame;
        for (S32 second = 1;
             second <= 4001 && controller.strikeCount() == 0;
             ++second)
        {
            config.mRatePerMinute =
                (second & 1) ? 59.9 : 60.1;
            frame = controller.update(
                static_cast<F64>(second), config, anchor());
        }
        ensure("rate modulation cannot starve scheduler",
               controller.strikeCount() == 1);
        ensureFiniteFrame("modulated-rate frame finite", frame);
    }

    // Fixed 120 Hz simulation quanta make automatic strike times and positions
    // independent of render-frame grouping at common capture/display rates.
    template<> template<>
    void weather_model_object::test<12>()
    {
        Config config = enabledConfig();
        Controller at_30;
        Controller at_60;
        Controller at_120;
        Controller at_144;
        const std::vector<StrikeSample> reference =
            runTimeline(at_30, config, 30, 90.0);
        ensure("cross-dt timeline produced strikes", !reference.empty());
        ensureSameStrikes(
            "30/60 strike count, times, and positions",
            reference, runTimeline(at_60, config, 60, 90.0));
        ensureSameStrikes(
            "30/120 strike count, times, and positions",
            reference, runTimeline(at_120, config, 120, 90.0));
        ensureSameStrikes(
            "30/144 strike count, times, and positions",
            reference, runTimeline(at_144, config, 144, 90.0));
    }

    // Resetting to the same seed and replaying the identical presentation
    // timeline reproduces every exposed strike field bit-for-bit.
    template<> template<>
    void weather_model_object::test<13>()
    {
        Controller controller;
        const Config config = enabledConfig();
        std::vector<Frame> first;
        for (S32 frame = 0; frame <= 5400; ++frame)
        {
            first.push_back(controller.update(
                static_cast<F64>(frame) / 60.0, config, anchor()));
        }
        controller.reset(config.mSeed);
        std::vector<Frame> replay;
        for (S32 frame = 0; frame <= 5400; ++frame)
        {
            replay.push_back(controller.update(
                static_cast<F64>(frame) / 60.0, config, anchor()));
        }
        ensure("reset/replay frame counts match",
               first.size() == replay.size());
        for (size_t i = 0; i < first.size(); ++i)
        {
            ensureSameFrame(
                "reset/replay full-frame bit identity", first[i], replay[i]);
        }
        ensure("reset/replay produced strikes",
               controller.strikeCount() > 0);
    }

    // A material non-zero rate edit rescales the outstanding interval without
    // drawing from the PRNG. Timing changes, but strike index N retains the same
    // seed-derived position as a controller whose rate was never edited.
    template<> template<>
    void weather_model_object::test<14>()
    {
        Config steady_config = enabledConfig();
        Config edited_config = steady_config;
        Controller steady;
        Controller edited;
        steady.update(0.0, steady_config, anchor());
        edited.update(0.0, edited_config, anchor());

        edited_config.mRatePerMinute = 20.0;
        edited.update(0.1, edited_config, anchor());

        Frame steady_strike;
        Frame edited_strike;
        for (S32 tick = 13;
             tick <= 120000 &&
             (steady.strikeCount() == 0 || edited.strikeCount() == 0);
             ++tick)
        {
            const F64 now = static_cast<F64>(tick) / 120.0;
            if (steady.strikeCount() == 0)
            {
                steady_strike =
                    steady.update(now, steady_config, anchor());
            }
            if (edited.strikeCount() == 0)
            {
                edited_strike =
                    edited.update(now, edited_config, anchor());
            }
        }

        ensure("steady schedule struck", steady.strikeCount() == 1);
        ensure("rescaled schedule struck", edited.strikeCount() == 1);
        ensure("60->20 rate edit delayed the strike",
               edited_strike.mStrikeTime > steady_strike.mStrikeTime);
        ensure("rate edit preserved strike x",
               edited_strike.mStrikeBase.mX ==
                   steady_strike.mStrikeBase.mX);
        ensure("rate edit preserved strike y",
               edited_strike.mStrikeBase.mY ==
                   steady_strike.mStrikeBase.mY);
        ensure("rate edit preserved strike z",
               edited_strike.mStrikeBase.mZ ==
                   steady_strike.mStrikeBase.mZ);
    }

    // Quality animation uses an independent stateless hash lane. Enabling it
    // may extend the visible tail, but must not consume scheduler PRNG values
    // or alter baseline pulse/placement decisions.
    template<> template<>
    void weather_model_object::test<15>()
    {
        Controller baseline;
        Controller quality;
        Controller replay;
        Config baseline_config = enabledConfig();
        baseline_config.mRatePerMinute = 0.0;
        Config quality_config = baseline_config;
        quality_config.mQualityEnabled = true;
        quality_config.mQualityAfterglowStrength = 0.35;

        ensure("baseline trigger accepted",
               baseline.trigger(10.0, baseline_config, anchor()));
        ensure("quality trigger accepted",
               quality.trigger(10.0, quality_config, anchor()));
        ensure("quality replay trigger accepted",
               replay.trigger(10.0, quality_config, anchor()));

        const Frame baseline_start =
            baseline.update(10.0, baseline_config, anchor());
        const Frame quality_start =
            quality.update(10.0, quality_config, anchor());
        const Frame replay_start =
            replay.update(10.0, quality_config, anchor());
        ensure("quality keeps baseline flash",
               quality_start.mFlash == baseline_start.mFlash);
        ensure("quality keeps baseline bolt",
               quality_start.mBolt == baseline_start.mBolt);
        ensure("quality keeps baseline position",
               quality_start.mStrikeBase.mX == baseline_start.mStrikeBase.mX &&
               quality_start.mStrikeBase.mY == baseline_start.mStrikeBase.mY &&
               quality_start.mStrikeDistanceMeters ==
                   baseline_start.mStrikeDistanceMeters);
        ensure("quality pulse begins",
               quality_start.mQualityBolt > 0.f);
        ensureSameFrame("quality visual lane deterministic",
                        quality_start, replay_start);

        const Frame baseline_tail =
            baseline.update(10.36, baseline_config, anchor());
        const Frame quality_tail =
            quality.update(10.36, quality_config, anchor());
        ensure("baseline still expires at original duration",
               !baseline_tail.mActive);
        ensure("quality afterglow extends active tail",
               quality_tail.mActive && quality_tail.mAfterglow > 0.f);
        ensureFiniteFrame("quality tail finite", quality_tail);

        const Frame quality_expired =
            quality.update(10.43, quality_config, anchor());
        ensure("quality tail ends within 0.30 seconds of its onset",
               !quality_expired.mActive);

        ensure("second baseline trigger accepted",
               baseline.trigger(20.0, baseline_config, anchor()));
        ensure("second quality trigger accepted",
               quality.trigger(20.0, quality_config, anchor()));
        const Frame baseline_second =
            baseline.update(20.0, baseline_config, anchor());
        const Frame quality_second =
            quality.update(20.0, quality_config, anchor());
        ensure("quality consumed no placement PRNG",
               quality_second.mStrikeBase.mX ==
                   baseline_second.mStrikeBase.mX &&
               quality_second.mStrikeBase.mY ==
                   baseline_second.mStrikeBase.mY);

        Controller short_bolt;
        Controller long_bolt;
        Config short_config = quality_config;
        short_config.mFlashDurationSeconds = 0.05;
        short_config.mBoltDurationSeconds = 0.02;
        Config long_config = quality_config;
        long_config.mFlashDurationSeconds = 4.0;
        long_config.mBoltDurationSeconds = 4.0;
        ensure("short quality trigger accepted",
               short_bolt.trigger(30.0, short_config, anchor()));
        ensure("long quality trigger accepted",
               long_bolt.trigger(30.0, long_config, anchor()));
        short_bolt.update(30.0, short_config, anchor());
        long_bolt.update(30.0, long_config, anchor());
        const Frame short_at_sixty_ms =
            short_bolt.update(30.06, short_config, anchor());
        const Frame long_at_sixty_ms =
            long_bolt.update(30.06, long_config, anchor());
        ensure("short bolt has no stretched return stroke",
               short_at_sixty_ms.mQualityBolt == 0.f);
        ensure("long bolt uses millisecond return-stroke timing",
               long_at_sixty_ms.mStrokeIndex == 1 &&
               long_at_sixty_ms.mQualityBolt > 0.f);

        Controller terminal_bolt;
        ensure("terminal-envelope trigger accepted",
               terminal_bolt.trigger(40.0, short_config, anchor()));
        terminal_bolt.update(40.0, short_config, anchor());
        const Frame just_before_cutoff =
            terminal_bolt.update(40.0199, short_config, anchor());
        const Frame at_cutoff =
            terminal_bolt.update(40.0200, short_config, anchor());
        ensure("quality bolt smoothly approaches its cutoff",
               just_before_cutoff.mQualityBolt > 0.f &&
               just_before_cutoff.mQualityBolt < 0.01f);
        ensure("quality bolt is zero at its cutoff",
               at_cutoff.mQualityBolt == 0.f);
    }
}
