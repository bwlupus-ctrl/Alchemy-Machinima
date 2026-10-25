/**
 * @file alweathershelter_test.cpp
 * @brief WP2 production fixed-step, coverage and probe revision contracts.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alweathershelter.h"
#include <cmath>
namespace tut
{
    struct surface_data {};
    typedef test_group<surface_data> surface_group;
    typedef surface_group::object surface_object;
    surface_group surface_tests("ALWeatherSurfaces");
    template<> template<> void surface_object::test<1>()
    {
        using namespace ALWeatherShelter;
        ensure("terrain-only traversal publishes its submitted terrain", classify(true, true, true, true, 1) == Coverage::COMPLETE_COVERED);
        ensure("empty map is COMPLETE_EMPTY", classify(true, true, true, true, 0) == Coverage::COMPLETE_EMPTY);
        ensure("roof traversal is complete", classify(true, true, true, true, 1) == Coverage::COMPLETE_COVERED);
        ensure("failed allocation never publishes", !usable(classify(false, true, true, true, 1)));
        ensure("failed clear never publishes", !usable(classify(true, false, true, true, 1)));
        ensure("incomplete terrain traversal never publishes", !usable(classify(true, true, false, true, 1)));
        ensure("incomplete pass never publishes", !usable(classify(true, true, true, false, 1)));
        ensure_equals("default two D32F maps are 2 MiB", coverBytes(512), U64(2097152));
    }
    template<> template<> void surface_object::test<2>()
    {
        ALWeatherBridge::Settings s;
        s.enabled = s.local = s.persistentWet = true;
        s.mode = 1; s.rainRate = 5.f; s.transition = s.bands = 0.f;
        s.wetSeconds = 120.f; s.drySeconds = 300.f;
        ALWeatherBridge::Controller a, b;
        ALWeatherModel::Config lightning;
        for (U64 f = 0; f <= 3600; ++f) a.update(f, f / 60., s, 0, 0, 1, lightning, {});
        for (U64 f = 0; f <= 8640; ++f) b.update(f, f / 144., s, 0, 0, 1, lightning, {});
        ensure("production 120 Hz soak reaches half at sixty seconds", std::fabs(a.state().wetness - 0.5f) < 0.001f);
        ensure_equals("soak is display-FPS independent", a.state().wetness, b.state().wetness);
        s.rainRate = 0.f;
        for (U64 f = 3601; f <= 12600; ++f) a.update(f, f / 60., s, 0, 0, 1, lightning, {});
        ensure("Stop precipitation dries without a reset", a.state().wetness < 0.001f);
        s.startWetness = 0.8f; a.applyStartingSurfaces();
        a.update(12601, 210., s, 0, 0, 1, lightning, {});
        ensure("apply starting surfaces works while clock paused", std::fabs(a.state().wetness - 0.8f) < 0.001f);
        const auto rain = a.state().rainFall;
        a.clearSurfaces(); a.update(12602, 210., s, 0, 0, 1, lightning, {});
        ensure("clear has observable effect and preserves storm", a.state().wetness == 0.f && a.state().rainFall == rain);
        s.mode = 0; s.legacyRain = true; s.legacyIntensity = SLWeather::amountFromFall(SLWeather::PRECIP_RAIN, 5.f);
        for (U64 f = 12603; f <= 16203; ++f) a.update(f, f / 60., s, 0, 0, 1, lightning, {});
        ensure("Existing rain integrates persistent wetness", a.state().wetness > 0.45f && !a.active());
        const F32 legacy_wet = a.state().wetness;
        ++s.seed;
        a.update(16204, 270.05, s, 0, 0, 1, lightning, {});
        ensure("unused library seed cannot reset Existing rain surfaces", a.state().wetness == legacy_wet);
        s.enabled = false; a.update(16205, 270.05, s, 0, 0, 1, lightning, {});
        ensure("master off clears immediately", a.state().wetness == 0.f);
    }
    template<> template<> void surface_object::test<3>()
    {
        using namespace ALWeatherShelter;
        ALWeatherBridge::Settings s; s.enabled = s.local = true; s.mode = 1;
        ALWeatherBridge::Controller a; ALWeatherModel::Config lightning;
        a.update(0, 0., s, 0, 0, 1, lightning, {});
        ensure_equals("pure rain cannot dirty material probes", materialSignature(a), U64(0));
        s.persistentWet = true; s.transition = s.bands = 0.f;
        a.update(1, 1., s, 0, 0, 1, lightning, {});
        const U64 dry = materialSignature(a);
        ProbeRevision ordinary, cinematic;
        ordinary.update(dry, 1.); cinematic.update(dry, 1.);
        for (U64 f = 2; f <= 121; ++f) a.update(f, static_cast<F64>(f), s, 0, 0, 1, lightning, {});
        const U64 wet = materialSignature(a);
        ensure("stationary wetting changes probe signature", wet != dry);
        ensure("on-demand and On-change invalidate", ordinary.update(wet, 121.) && cinematic.update(wet, 121.));
        ensure("repeated identical state stays clean", !ordinary.update(wet, 121.05));
        ensure("coalesce changes at two Hz", !ordinary.update(dry, 121.1));
        ensure("coalesced change is not lost", ordinary.update(dry, 121.5));
        ensure("presentation restart cannot hold a stale probe revision", cinematic.update(dry, 0.));
        ensure("restart retains the refresh cap", !cinematic.update(wet, 0.1));
        ensure("restart coalescing releases the pending state", cinematic.update(wet, 0.5));
        s.rainRate = 0.f;
        for (U64 f = 122; f <= 271; ++f) a.update(f, static_cast<F64>(f), s, 0, 0, 1, lightning, {});
        ensure("stationary drying invalidates", materialSignature(a) != wet);
        const auto before = materialSignature(a);
        a.teleportSucceeded(); a.update(272, 272., s, 0, 0, 1, lightning, {});
        ensure("place revision invalidates", materialSignature(a) != before);
        a.clearSurfaces(); a.update(273, 272., s, 0, 0, 1, lightning, {});
        ensure("clear command invalidates even on dry ground", materialSignature(a) != before);
    }
}
