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
}
