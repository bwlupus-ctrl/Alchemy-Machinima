/**
 * @file alweatherbridge_test.cpp
 * @brief Production adapter determinism, scheduler and rate tests.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alweatherbridge.h"
#include <cmath>
#include <limits>
namespace tut
{
    struct weather_bridge_data {};
    typedef test_group<weather_bridge_data> weather_bridge_group;
    typedef weather_bridge_group::object weather_bridge_object;
    weather_bridge_group weather_bridge_tests("ALWeatherBridge");
    template<> template<> void weather_bridge_object::test<1>()
    {
        ALWeatherBridge::Settings s;
        s.enabled = s.local = true; s.mode = s.windSource = 1;
        s.lightningTiming = 1; s.storm = 1.f; s.bands = 0.6f;
        s.rainRate = 80.f;
        ALWeatherBridge::Controller a, b;
        ALWeatherModel::Config lightning;
        lightning.mEnabled = true;
        bool saw_strike = false;
        for (U64 frame = 0; frame <= 36000; ++frame)
        {
            const F64 now = frame / 60.0;
            a.update(frame, now, s, 1.f, 2.f, 1, lightning, {});
            b.update(frame, now, s, 1.f, 2.f, 1, lightning, {});
            ensure("bridge rain deterministic", a.state().rainFall == b.state().rainFall);
            ensure("bridge strike deterministic", a.lightning().mStrikeId == b.lightning().mStrikeId && a.lightning().mFlash == b.lightning().mFlash);
            saw_strike = saw_strike || a.lightning().mActive;
            const F32 rate = a.state().rainFall;
            a.update(frame, now + 5.0, s, 9.f, 9.f, 1, lightning, {});
            ensure("two views cannot double-step", rate == a.state().rainFall);
        }
        ensure("library storm scheduled strikes", saw_strike);
    }
    template<> template<> void weather_bridge_object::test<2>()
    {
        ALWeatherBridge::Settings s;
        s.enabled = s.local = true; s.mode = 1; s.transition = s.bands = 0.f;
        ALWeatherBridge::Controller a, b;
        ALWeatherModel::Config lightning;
        for (U64 f = 0; f <= 600; ++f) a.update(f, f / 60.0, s, 3.f, 4.f, 1, lightning, {});
        for (U64 f = 0; f <= 1440; ++f) b.update(f, f / 144.0, s, 3.f, 4.f, 1, lightning, {});
        ensure("fixed-step bridge partitions match", a.state().rainFall == b.state().rainFall && a.state().wind[0] == b.state().wind[0]);
        ensure("simulator wind follows selected source", std::fabs(a.state().wind[0] - 3.f) < 1.e-5 && std::fabs(a.state().wind[1] - 4.f) < 1.e-5);
        ensure("unshipped surface consumers have no accumulated state",
               a.state().wetness == 0.f && a.state().snowDepth == 0.f && a.state().runoff == 0.f);
        F32 intensity = -1.f, density = -1.f;
        for (U32 rate = 0; rate <= 200; ++rate)
        {
            SLWeather::State state; state.rainFall = static_cast<F32>(rate);
            const auto rain = ALWeatherBridge::rain(state, 0.55f, 0.65f);
            ensure("rate adapter monotonic", rain.intensity >= intensity && rain.density >= density);
            intensity = rain.intensity; density = rain.density;
        }
        a.teleportSucceeded(); a.update(601, 10.0, s, 3.f, 4.f, 1, lightning, {});
        ensure("same-region teleport delivered", a.placeSerial() == 1);
        s.enabled = false;
        a.update(602, 10.0, s, 3.f, 4.f, 1, lightning, {});
        ensure("off is immediate even paused", !a.active() && !a.state().visible());
        s.enabled = true; s.mode = 0;
        a.update(603, 11.0, s, 3.f, 4.f, 1, lightning, {});
        ensure("mode zero never runs library rain", !a.active() && a.state().rainFall == 0.f);
        ensure("mode zero library clock is inert", a.clock().tick() == 0 && !a.catchingUp());
        s.rainRate = std::numeric_limits<F32>::quiet_NaN();
        s.windSpeed = std::numeric_limits<F32>::infinity();
        s.transition = -10.f;
        s.storm = 99.f;
        const auto safe = ALWeatherBridge::sanitize(s);
        ensure("invalid settings are finite and bounded", safe.rainRate == 5.f &&
               safe.windSpeed == 3.f && safe.transition == 0.f && safe.storm == 1.f);
    }
    template<> template<> void weather_bridge_object::test<3>()
    {
        ALWeatherBridge::Settings s;
        s.enabled = s.local = true; s.mode = 1; s.transition = 0.f;
        s.lightningTiming = 1; s.storm = 0.f;
        ALWeatherBridge::Controller bridge;
        ALWeatherModel::Config lightning; lightning.mEnabled = true; lightning.mRatePerMinute = 60.0;
        for (U64 f = 0; f < 7200; ++f)
        {
            bridge.update(f, f / 60.0, s, 0.f, 0.f, 1, lightning, {});
            ensure("no native events during external schedule", !bridge.lightning().mActive);
        }
        ensure("external timing excludes native automatic schedule", bridge.lightning().mStrikeId == 0);
        bridge.update(7200, 120.0, s, 0.f, 0.f, 1, lightning, {}, true);
        ensure("manual strike works in external timing", bridge.lightning().mActive);
        lightning.mEnabled = false;
        bridge.update(7201, 120.0, s, 0.f, 0.f, 1, lightning, {});
        ensure("lightning disabled immediately", !bridge.lightning().mActive);
    }
    template<> template<> void weather_bridge_object::test<4>()
    {
        ALWeatherBridge::Settings s;
        s.enabled = s.local = true; s.mode = s.windSource = s.lightningTiming = 1;
        s.storm = 1.f; s.transition = 0.f; s.seed = 123456U;
        ALWeatherBridge::Controller bridge;
        ALWeatherModel::Config lightning;
        lightning.mEnabled = lightning.mQualityEnabled = true;
        std::vector<ALWeatherModel::Frame> first;
        U64 iteration = 0;
        for (U64 f = 0; f <= 7200; ++f)
        {
            bridge.update(iteration++, f / 60., s, 0, 0, 1, lightning, {});
            first.push_back(bridge.lightning());
        }
        s.enabled = false;
        bridge.update(iteration++, 120., s, 0, 0, 1, lightning, {});
        s.enabled = true;
        bool saw_strike = false;
        for (U64 f = 0; f <= 7200; ++f)
        {
            // Presentation time continues across the master toggle; storm time restarts.
            bridge.update(iteration++, 121. + f / 60., s, 0, 0, 1, lightning, {});
            const auto& a = first[f]; const auto& b = bridge.lightning();
            saw_strike = saw_strike || a.mActive;
            ensure("seeded storm restart preserves strike visuals", a.mActive == b.mActive &&
                a.mVisualSeed == b.mVisualSeed && a.mStrokeIndex == b.mStrokeIndex &&
                a.mFlash == b.mFlash && a.mBolt == b.mBolt && a.mQualityBolt == b.mQualityBolt &&
                a.mAfterglow == b.mAfterglow && a.mColorVariation == b.mColorVariation &&
                a.mStrikeBase.mX == b.mStrikeBase.mX && a.mStrikeBase.mY == b.mStrikeBase.mY);
        }
        ensure("restart regression observes active lightning", saw_strike);
    }
    template<> template<> void weather_bridge_object::test<5>()
    {
        ALWeatherBridge::Settings s;
        s.enabled = s.local = s.snowEnabled = true;
        s.mode = s.precipType = 1; s.transition = s.bands = 0.f;
        s.snowRate = 12.f; s.snowSpeed = 2.f;
        ALWeatherBridge::Controller a, b;
        for (U64 f = 0; f <= 600; ++f) a.update(f, f / 60., s, 3, 4, 1, {}, {});
        for (U64 f = 0; f <= 1440; ++f) b.update(f, f / 144., s, 3, 4, 1, {}, {});
        ensure("snow wind drift independent of display partitions", a.snowDrift() == b.snowDrift());
        ensure("fall follows fixed weather clock", std::fabs(a.snowDrift()[2] + a.clock().seconds() * 2.) < 1.e-9);
        ensure("snow never leaks into rain", a.state().rainFall == 0.f && ALWeatherBridge::rain(a.state(), 1, 1).intensity == 0.f);
        ensure("snowfall does not accumulate unshipped ground snow", a.state().snowDepth == 0.f && a.state().snowCover == 0.f);
        const auto drift = a.snowDrift();
        a.update(600, 99., s, 10, 10, 1, {}, {});
        ensure("auxiliary order cannot move flakes twice", drift == a.snowDrift());
        SLWeather::State mixed; mixed.rainFall = 200.f; mixed.snowFall = 360.f;
        ensure("mixed visual components normalized", ALWeatherBridge::rain(mixed, 1, 1).intensity + ALWeatherBridge::snowAmount(mixed) <= 1.f);
        s.precipType = 2;
        const auto config = ALWeatherBridge::configuration(s, 3, 4);
        ensure("mixed physical rates retained independently", config.local.rainFall == s.rainRate && config.local.snowFall == s.snowRate);
        s.enabled = false; a.update(601, 11., s, 3, 4, 1, {}, {});
        ensure("master off releases precipitation state", !a.precipitationActive() && a.snowDrift() == std::array<F64,3>{});
    }
    template<> template<> void weather_bridge_object::test<6>()
    {
        ALWeatherBridge::Settings s;
        s.snowRate = std::numeric_limits<F32>::quiet_NaN();
        s.flakeSize = std::numeric_limits<F32>::infinity();
        s.snowRange = -1.f; s.snowSpeed = 9.f; s.snowLimit = ~0U;
        s.visibilityAmount = -2.f; s.precipType = ~0U;
        s = ALWeatherBridge::sanitize(s);
        ensure("snow parameters finite and bounded", s.snowRate == 1.5f && s.flakeSize == 0.015f &&
            s.snowRange == 16.f && s.snowSpeed == 4.f && s.snowLimit == 131072U && s.visibilityAmount == 0.f && s.precipType == 2);
        ensure("new expensive defaults off", !s.snowEnabled && !s.visibility);
    }
    template<> template<> void weather_bridge_object::test<7>()
    {
        ALWeatherBridge::Settings s;
        s.enabled = s.local = true; s.mode = s.lightningTiming = 1;
        s.storm = 1.f; s.rainRate = 80.f;
        ALWeatherModel::Config lightning;
        lightning.mEnabled = true;
        lightning.mMinDistanceMeters = 45.; lightning.mMaxDistanceMeters = 140.;
        ALWeatherBridge::Controller controller;
        bool saw_strike = false;
        for (U64 frame = 0; frame <= 7200; ++frame)
        {
            controller.update(frame, frame / 60., s, 0, 0, 1, lightning, {});
            const auto& strike = controller.lightning();
            if (!strike.mActive) continue;
            saw_strike = true;
            ensure("storm respects native cinematic range", strike.mStrikeDistanceMeters >= 45. && strike.mStrikeDistanceMeters <= 140.);
        }
        ensure("storm starts within bounded test time", saw_strike);
        controller.update(7201, 120.1, s, 0, 0, 1, lightning, {}, true);
        ensure("manual strike works with storm scheduler", controller.lightning().mActive);
        s.lightningTiming = 0;
        controller.update(7202, 120.2, s, 0, 0, 1, lightning, {}, true);
        ensure("manual strike works with native scheduler", controller.lightning().mActive);
        lightning.mEnabled = false;
        controller.update(7203, 120.3, s, 0, 0, 1, lightning, {}, true);
        ensure("manual strike preserves lightning enable gate", !controller.lightning().mActive);
    }
    template<> template<> void weather_bridge_object::test<8>()
    {
        ALWeatherBridge::Settings s;
        s.enabled = s.local = s.snowEnabled = s.persistentWet = true;
        s.mode = s.precipType = 1;
        ALWeatherModel::Config lightning;
        lightning.mEnabled = true;
        ALWeatherBridge::Controller controller;
        controller.update(1, 1., s, 3, 4, 1, lightning, {}, true);
        ensure("active before transit", controller.precipitationActive() && controller.surfacesActive() && controller.lightning().mActive);
        const U32 place = controller.placeSerial();
        controller.suspend();
        controller.suspend();
        ensure("transit disables every bridge consumer", !controller.active() && !controller.precipitationActive() &&
            !controller.surfacesActive() && !controller.lightning().mActive && controller.snowDrift() == std::array<F64, 3>{});
        controller.update(2, 1000., s, 3, 4, 1, lightning, {});
        ensure("same-region arrival changes place", controller.placeSerial() > place);
        ensure("arrival starts clean without transit catchup or old strike", controller.clock().seconds() == 0. &&
            !controller.catchingUp() && !controller.lightning().mActive && controller.precipitationActive());
        const U32 arrived = controller.placeSerial();
        controller.suspend();
        controller.update(3, 1001., s, 3, 4, 2, lightning, {});
        ensure("inter-region teleport also starts a fresh place", controller.placeSerial() > arrived && controller.surfacesActive());
    }
    template<> template<> void weather_bridge_object::test<9>()
    {
        for (U32 mode : {0U, 1U}) for (U32 timing : {0U, 1U})
        {
            ALWeatherBridge::Settings s;
            s.enabled = s.local = s.snowEnabled = s.persistentWet = true;
            s.mode = mode; s.lightningTiming = timing; s.precipType = 2;
            s.rainRate = 80.f; s.snowRate = 8.f; s.storm = 1.f;
            s.transition = 20.f; s.startWetness = 0.7f;
            ALWeatherModel::Config lightning; lightning.mEnabled = true; lightning.mRatePerMinute = 60.;
            ALWeatherBridge::Controller crossed, reference;
            crossed.applyStartingSurfaces(); reference.applyStartingSurfaces();
            const ALWeatherModel::Position offset{-256., -256., 0.};
            U32 place = 0, surfaces = 0, cleared = 0;
            for (U64 frame = 0; frame <= 7200; ++frame)
            {
                if (frame == 120) { s.rainRate = 120.f; s.snowRate = 12.f; }
                if (frame == 180)
                {
                    place = crossed.placeSerial(); surfaces = crossed.surfaceSerial(); cleared = crossed.clearSerial();
                    const auto clock = crossed.clock().seconds();
                    const auto wetness = crossed.state().wetness;
                    const auto drift = crossed.snowDrift();
                    crossed.rebase(offset);
                    ensure("origin shift leaves clock/wetness/drift untouched", crossed.clock().seconds() == clock &&
                        crossed.state().wetness == wetness && crossed.snowDrift() == drift);
                    ensure("crossing retains wet surfaces", crossed.surfacesActive() && wetness > 0.6f);
                }
                const ALWeatherModel::Position anchor = frame >= 180 ? offset : ALWeatherModel::Position{};
                crossed.update(frame, frame / 60., s, 3, 4, frame >= 180 ? 2 : 1, lightning, anchor, frame == 179);
                reference.update(frame, frame / 60., s, 3, 4, 1, lightning, {}, frame == 179);
                ensure("crossing retains clock and transition trajectory", crossed.clock().seconds() == reference.clock().seconds() &&
                    crossed.state().rainFall == reference.state().rainFall && crossed.state().snowFall == reference.state().snowFall);
                ensure("crossing retains wetness and snow drift", crossed.state().wetness == reference.state().wetness &&
                    crossed.snowDrift() == reference.snowDrift());
                const auto& a = crossed.lightning(); const auto& b = reference.lightning();
                ensure("crossing retains active flash and future strike schedule", a.mActive == b.mActive &&
                    a.mStrikeId == b.mStrikeId && a.mStrikeTime == b.mStrikeTime && a.mFlash == b.mFlash);
                if (frame >= 180)
                {
                    ensure("crossing never resets place or surfaces", crossed.placeSerial() == place &&
                        crossed.surfaceSerial() == surfaces && crossed.clearSerial() == cleared);
                    if (a.mActive) ensure("active and future bolts retain their world position", a.mStrikeBase.mX == b.mStrikeBase.mX + offset.mX &&
                        a.mStrikeBase.mY == b.mStrikeBase.mY + offset.mY && a.mStrikeBase.mZ == b.mStrikeBase.mZ);
                }
            }
        }
    }
}
