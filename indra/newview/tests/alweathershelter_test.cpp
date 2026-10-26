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
    template<> template<> void surface_object::test<4>()
    {
        ALWeatherShelter::Exposure main, auxiliary, other;
        main.update(0., 1.f); auxiliary.update(0., 1.f); other.update(0., 1.f);
        F32 previous = 1.f;
        for (U32 frame = 1; frame <= 30; ++frame)
        {
            const F64 now = frame / 60.;
            const F32 exposure = auxiliary.update(now, 0.f);
            ensure("aux roof crossing fades continuously", exposure > 0.f && exposure < previous);
            ensure_equals("aux and main use identical response", exposure, main.update(now, 0.f));
            ensure_equals("other auxiliary remains independent", other.update(now, 1.f), 1.f);
            previous = exposure;
        }
        auxiliary.reset();
        ensure_equals("camera lifecycle clears exposure", auxiliary.update(1., 0.f), 0.f);
        ensure_equals("presentation rewind resets exposure", main.update(0., 1.f), 1.f);
    }
    template<> template<> void surface_object::test<5>()
    {
        using namespace ALWeatherShelter;
        const auto shear = fallShear(14.f, 0.f, 28.f, true);
        ensure_equals("fall ray preserves transverse coordinate", shear[0], 0.5f);
        ensure_equals("vertical choice", fallShear(14.f, 0.f, 28.f, false)[0], 0.f);
        const auto bounded = fallShear(40.f, 40.f, 1.f, true);
        ensure("45 degree fall bound", std::hypot(bounded[0], bounded[1]) <= 1.000001f);
        const auto a = plan({256., 512., 4.}, 32.f, 1024, shear);
        const auto b = plan({256.001, 512.001, 4.}, 32.f, 1024, shear);
        ensure("sub-cell motion cannot move the world lattice", a == b);
        Updates updates;
        const std::array<F64, 3> origin{256., 512., 0.};
        ensure("initial update", updates.schedule(a, a, 1, origin, 0., 30, 1));
        for (U32 tile = 0; tile < 64; ++tile)
        {
            ensure_equals("initialization is one tile", updates.tile().size, 1U);
            updates.complete();
            if (tile != 63) ensure("next initialization tile", updates.schedule(a, a, 1, origin, 0., 30, 1));
        }
        ensure("static maps have no periodic traversal", !updates.schedule(a, a, 1, origin, 1., 30, 1));
        updates.dirty({4096.,4096.,0.}, {4100.,4100.,10.});
        ensure("outside motion stays clean", !updates.schedule(a, a, 1, origin, 1.01, 30, 1));
        updates.dirty({255.,511.,0.}, {257.,513.,8.});
        ensure("moving roof region", updates.schedule(a, a, 1, origin, 1.02, 30, 1));
        ensure("warm work is bounded to a quarter map", updates.tile().size <= 4);
        updates.complete();
        ensure("coalesce new dirty revisions", !updates.schedule(a, a, 1, origin, 1.03, 30, 1));
        ensure("capture post-flush geometry once", updates.schedule(a, a, 1, origin, 1.06, 30, 1));
        updates.complete();
        for (U32 frame = 0; frame < 128; ++frame)
            if (updates.schedule(a, a, 1, origin, 1.2 + frame/60., 30, 1)) updates.complete();
        ensure("settling never becomes permanent polling", !updates.schedule(a, a, 1, origin, 4., 30, 1));
        U32 jobs = 0;
        for (U32 frame = 0; frame < 120; ++frame)
        {
            updates.dirty({255.,511.,0.}, {257.,513.,8.});
            if (updates.schedule(a, a, 1, origin, 5.+frame/60., 30, 1))
            {
                ensure("sustained invalidation never becomes a full traversal", !updates.tile().full());
                ensure("sustained work cap", updates.tile().size <= 4);
                ++jobs; updates.complete();
            }
        }
        ensure("sustained refresh cadence", jobs >= 55 && jobs <= 60);
        ensure("origin rebase invalidates immediately", updates.schedule(a, a, 1, {0.,0.,0.}, 8.01, 30, 1));
        updates.complete();
        ensure("place never reuses previous roof", updates.schedule(a, a, 2, {0.,0.,0.}, 8.02, 30, 1));
    }
    template<> template<> void surface_object::test<6>()
    {
        using namespace ALWeatherShelter;
        ALWeatherBridge::Controller weather;
        ALWeatherBridge::Settings settings;
        settings.enabled = settings.local = settings.persistentWet = true;
        settings.mode = 1;
        weather.update(1, 0., settings, 0, 0, 1, {}, {});
        const U64 basic = materialSignature(weather);
        settings.advancedShelter = true;
        weather.update(2, 0., settings, 0, 0, 1, {}, {});
        const U64 tilted = materialSignature(weather);
        ensure("advanced shelter invalidates frozen material probes", basic != tilted);
        settings.shelterNearExtent = 64.f;
        weather.update(3, 0., settings, 0, 0, 1, {}, {});
        ensure("shelter coverage configuration invalidates probes", tilted != materialSignature(weather));
        settings.advancedShelter = false;
        weather.update(4, 0., settings, 0, 0, 1, {}, {});
        ensure_equals("off shelter restores the basic material signature", materialSignature(weather), basic);
    }
    template<> template<> void surface_object::test<7>()
    {
        using namespace ALWeatherShelter;
        ensure("default legacy auxiliary remains inert", !auxiliaryAllowed(false, true, false));
        ensure("advanced legacy auxiliary rain is admitted", auxiliaryAllowed(false, false, true));
        ensure("inactive library cannot borrow legacy admission", !auxiliaryAllowed(true, false, true));
        ensure("active library auxiliary remains admitted", auxiliaryAllowed(true, true, false));
        ALWeatherModel::Config config; config.mEnabled = true;
        ALWeatherModel::Controller sampled, reference;
        sampled.update(0., config, {}); reference.update(0., config, {});
        sampled.trigger(0., config, {}); reference.trigger(0., config, {});
        const U64 count = sampled.strikeCount();
        const auto view = sampled.sample(0.1, config);
        ensure_equals("auxiliary rain never schedules a strike", sampled.strikeCount(), count);
        ensure("auxiliary read never re-emits a strike event", !view.mStrikeStarted);
        const auto a = sampled.update(0.2, config, {}), b = reference.update(0.2, config, {});
        ensure_equals("view sampling preserves the main scheduler", a.mStrikeId, b.mStrikeId);
        ensure_equals("view sampling preserves the main flash", a.mFlash, b.mFlash);
    }
    template<> template<> void surface_object::test<9>()
    {
        using namespace ALWeatherShelter;
        for (F32 scale : {0.f, 1.f})
        {
            const auto wind = effectiveWind(14.f, -7.f, scale);
            const auto shear = fallShear(wind[0], wind[1], 28.f, true);
            ensure_equals("streak and shelter share scaled horizontal wind", wind[0], 14.f*scale);
            ensure_equals("streak and shelter share scaled vertical wind", wind[1], -7.f*scale);
            ensure_equals("zero Wind scale leaves shelter vertical", shear[0], 0.5f*scale);
            ensure_equals("unit Wind scale follows precipitation", shear[1], -0.25f*scale);
        }
        ensure("fully transparent faces cannot shelter rain", !visibleSurface(0.f, false));
        ensure("invisible texture cannot shelter rain", !visibleSurface(1.f, true));
        ensure("opaque roof shelters rain", visibleSurface(1.f, false));
        ensure("ray policy starts outside picking override", !VisibleSurfaceRay::active());
        {
            VisibleSurfaceRay ray;
            ensure("weather ray uses visible surfaces", VisibleSurfaceRay::active());
            { VisibleSurfaceRay nested; }
            ensure("nested ray restores outer policy", VisibleSurfaceRay::active());
        }
        ensure("weather ray restores picking policy", !VisibleSurfaceRay::active());
    }
    template<> template<> void surface_object::test<8>()
    {
        using namespace ALWeatherShelter;
        Updates updates;
        const auto p = plan({0., 0., 0.}, 128.f, 512, {});
        U64 visited[2]{};
        for (U32 frame = 0; frame < 256; ++frame)
        {
            // Sustained whole-volume churn must not reset either cursor or
            // prevent the distant map from completing its initial publication.
            updates.dirty({-128., -128., -64.}, {128., 128., 64.});
            if (!updates.schedule(p, p, 1, {}, frame/60., 60)) continue;
            const auto tile = updates.tile();
            ensure_equals("hard per-job tile cap", tile.size, 1U);
            visited[updates.selectedLayer()] |= U64(1) << (tile.y*8 + tile.x);
            updates.noteWork(1);
            updates.complete();
        }
        ensure_equals("close dirty rotation cannot starve tiles", visited[0], ~U64(0));
        ensure_equals("distant dirty rotation cannot starve tiles", visited[1], ~U64(0));
        ensure("live queue has a finite reported bound", updates.stalenessSeconds() > 0. &&
            updates.stalenessSeconds() <= 256./15.);
    }
    template<> template<> void surface_object::test<10>()
    {
        ALWeatherShelter::Exposure main, auxiliary;
        main.update(1., 0.f);
        auxiliary.update(1., 1.f);
        ALWeatherShelter::Exposure::invalidateAll();
        ensure_equals("arrival drops main shelter history", main.update(20., 1.f), 1.f);
        ensure_equals("hidden auxiliary drops shelter history on next draw", auxiliary.update(20., 0.f), 0.f);
        ensure("normal fading resumes after arrival", auxiliary.update(20.01, 1.f) > 0.f && auxiliary.update(20.01, 1.f) < 1.f);
    }
}
