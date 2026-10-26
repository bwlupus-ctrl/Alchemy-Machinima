/**
 * @file alweatherclock_test.cpp
 * @brief Weather clock partition, pause, seek and backlog tests.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alweatherclock.h"
#include <limits>
namespace tut
{
    struct weather_clock_data {};
    typedef test_group<weather_clock_data> weather_clock_group;
    typedef weather_clock_group::object weather_clock_object;
    weather_clock_group weather_clock_tests("ALWeatherClock");
    template<> template<> void weather_clock_object::test<1>()
    {
        for (U32 fps : {30U, 60U, 120U, 144U})
        {
            ALWeatherClock clock;
            clock.advance(250.0);
            for (U32 f = 1; f <= fps * 10; ++f)
            {
                clock.advance(250.0 + static_cast<F64>(f) / fps);
                while (clock.step()) {}
            }
            ensure("all frame partitions consume 1200 ticks", clock.tick() == 1200);
            clock.advance(260.0);
            ensure("pause adds no ticks", !clock.step());
            clock.advance(252.0);
            ensure("seek resets time", clock.tick() == 0 && clock.sample() == 0.0);
            ensure("NaN input rejected", !clock.advance(std::numeric_limits<F64>::quiet_NaN()));
        }
    }
    template<> template<> void weather_clock_object::test<2>()
    {
        ALWeatherClock clock;
        clock.advance(0.0); clock.advance(10.0);
        for (U32 i = 0; i < 240; ++i)
        {
            ensure("bounded step available", clock.step());
            ensure("every fixed tick visited", clock.tick() == i + 1);
        }
        ensure("backlog reported", clock.catchingUp());
        while (clock.step()) {}
        ensure("no hitch ticks dropped", clock.tick() == 1200);
    }
}
