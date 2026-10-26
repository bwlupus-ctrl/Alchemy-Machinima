/**
 * @file slweather_test.cpp
 * @brief Production CPU weather regression tests.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork; Slipstream model credited in slweather.h.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../slweather.h"
#include <cmath>

namespace tut
{
    struct slweather_data {};
    typedef test_group<slweather_data> slweather_group;
    typedef slweather_group::object slweather_object;
    slweather_group slweather_tests("SLWeather");

    template<> template<> void slweather_object::test<1>()
    {
        SLWeather a, b;
        SLWeather::Config c;
        c.master = c.local.active = true;
        c.local.seed = 77;
        c.local.setFall(SLWeather::PRECIP_RAIN, 20.f);
        c.local.fallVariation = 0.8f;
        c.local.windVariation = 0.7f;
        c.local.windSpeed = 8.f;
        c.local.storm = 1.f;
        for (U64 tick = 0; tick <= 120 * 600; ++tick)
        {
            SLWeather::Input i;
            i.tick = tick;
            i.seconds = i.syncSeconds = static_cast<F64>(tick) / 120.0;
            a.update(c, i); b.update(c, i);
            ensure("seeded rates and wind repeat", a.state().rainFall == b.state().rainFall &&
                a.state().wind[0] == b.state().wind[0] && a.state().wind[1] == b.state().wind[1]);
            ensure_equals("strike count repeat", a.strikes().size(), b.strikes().size());
            if (!a.strikes().empty()) ensure("strike hash repeat", a.strikes().back().hash == b.strikes().back().hash);
        }
        ensure("storm produced a strike", !a.strikes().empty());
    }
    template<> template<> void slweather_object::test<2>()
    {
        SLWeather weather;
        SLWeather::Config c;
        c.master = c.local.active = c.snowCoating = true;
        c.local.transitionSecs = 0.f;
        c.local.setFall(SLWeather::PRECIP_SNOW, 5.f);
        SLWeather::Input i;
        weather.update(c, i);
        ensure("starts from zero", weather.state().snowDepth == 0.f);
        for (U64 tick = 1; tick <= 72000; ++tick)
        {
            i.tick = tick;
            i.seconds = i.syncSeconds = static_cast<F64>(tick) / 120.0;
            weather.update(c, i);
        }
        const F64 expected = 5.0 / 100.0 / 3600.0 * 0.7 * 600.0;
        ensure("600 seconds of 5 cm/h accumulates from zero", std::fabs(weather.state().snowDepth - expected) < 3.e-6);
        ensure("snowfall creates no melt water", weather.state().wetness == 0.f);
    }
    template<> template<> void slweather_object::test<3>()
    {
        SLWeather weather;
        SLWeather::Config c;
        c.master = c.local.active = c.snowCoating = c.wetSurfaces = true;
        c.local.transitionSecs = 0.f;
        c.local.setFall(SLWeather::PRECIP_SNOW, 5.f);
        c.startWetness = 0.25f;
        c.startDepth = 0.02f;
        SLWeather::Input i;
        i.regionHandle = 1;
        weather.update(c, i);
        for (U64 tick = 1; tick < 1200; ++tick)
        {
            i.tick = tick; i.seconds = i.syncSeconds = tick / 120.0;
            weather.update(c, i);
        }
        const F32 before = weather.state().snowDepth;
        i.tick++; i.regionHandle = 2;
        weather.update(c, i);
        ensure("ordinary crossing preserves snow", weather.state().snowDepth == before);
        ensure("crossing does not reset patterns", weather.placeSerial() == 0);
        i.tick++; // origin rebase has no place event
        weather.update(c, i);
        ensure("origin rebase preserves snow", weather.state().snowDepth == before);
        i.tick++; i.teleportSucceeded = true; // even a 1 m same-region success
        weather.update(c, i);
        ensure("successful teleport resets patterns", weather.placeSerial() == 1);
        ensure("canonical snow depth", weather.state().snowDepth == c.startDepth);
        ensure("canonical wetness", weather.state().wetness == c.startWetness);
        ensure("teleport retains storm targets", weather.target().snowFall == 5.f);
        i.tick++; i.teleportSucceeded = false;
        weather.update(c, i);
        ensure("no success means no reset", weather.placeSerial() == 1);
        const U32 clear = weather.clearSerial();
        weather.requestReset("coincident with master off");
        c.master = false; i.tick++;
        weather.update(c, i);
        ensure("master off clears immediately", !weather.state().visible() && weather.strikes().empty());
        ensure("coincident clear increments once", weather.clearSerial() == clear + 1);
    }
    template<> template<> void slweather_object::test<4>()
    {
        SLWeather weather;
        SLWeather::Config c;
        c.master = c.local.active = true;
        c.local.transitionSecs = 20.f;
        c.local.setFall(SLWeather::PRECIP_RAIN, 80.f);
        F32 previous = 0.f;
        for (U64 tick = 0; tick < 1200; ++tick)
        {
            SLWeather::Input i;
            i.tick = tick; i.seconds = i.syncSeconds = tick / 120.0;
            weather.update(c, i);
            ensure("continuous transition", weather.state().rainFall >= previous && weather.state().rainFall - previous < 0.2f);
            previous = weather.state().rainFall;
            weather.update(c, i);
            ensure("same tick is immutable", previous == weather.state().rainFall);
        }
        ensure("transition progresses", previous > 20.f && previous < 80.f);
        for (U32 type = 0; type < 2; ++type)
        {
            F32 last = 0.f;
            for (int n = 0; n <= 2000; ++n)
            {
                const auto precip = static_cast<SLWeather::EPrecip>(type);
                const F32 rate = n * 0.1f;
                const F32 amount = SLWeather::amountFromFall(precip, rate);
                ensure("physical rate ladder monotonic", amount >= last);
                ensure("rate round trip", std::fabs(SLWeather::fallFromAmount(precip, amount) - rate) < 0.0003f);
                last = amount;
            }
        }
    }
}
