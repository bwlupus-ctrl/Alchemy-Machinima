#include "linden_common.h"
#include "../test/lltut.h"
#include "../alvramrecovery.h"
#include <numeric>
#include <cmath>

namespace tut
{
struct vram_recovery_data
{
    ALVRAMRecovery::QualityLatch latch;
    ALVRAMRecovery::QualityInput in;
    vram_recovery_data()
    {
        in.enabled = true;
        in.capture = true;
        in.manual_pin = true;
        in.tracked = 12000.f;
        in.effective_cap = 32607.f;
        in.reserve = ALVRAMRecovery::driverReserve(32607.f);
        in.available = 1000.f;
        in.pressure = true;
        in.driver_sample = true;
    }
    void trip()
    {
        ensure("pressure denies quality", !latch.update(in));
        ensure("pressure suspends armed request", latch.suspended);
        ensure_equals("records full demand", latch.peak_tracked, 12000.f);
    }
    void fit(double time)
    {
        in.pressure = false;
        in.tracked = 4000.f;
        in.available = 12000.f;
        in.now = time;
    }
};
typedef test_group<vram_recovery_data> vram_recovery_group;
typedef vram_recovery_group::object vram_recovery_object;
vram_recovery_group vram_recovery_tests("alvramrecovery");

template<> template<> void vram_recovery_object::test<1>()
{
    using ALVRAMRecovery::visitAllowed;
    ensure("floor even after budget", visitAllowed(63, 10.0));
    ensure("stop at budget and floor", !visitAllowed(64, 0.001));
    ensure("use remaining time", visitAllowed(64, 0.0009));
    ALVRAMRecovery::Purge<int> purge;
    std::vector<int> entries(150);
    std::iota(entries.begin(), entries.end(), 0);
    purge.request(entries, [](int key) { return key; });
    const auto capacity = purge.keys.capacity();
    std::vector<int> visited;
    double now = 0.0;
    auto clock = [&] { now += 0.002; return now; };
    auto visit = [&](int key) { visited.push_back(key); };
    ensure_equals("first frame floor", purge.advance(clock, visit), std::size_t(64));
    purge.request(entries, [](int key) { return key; });
    ensure_equals("new emergency keeps continuation", purge.keys.size(), std::size_t(86));
    ensure_equals("second frame", purge.advance(clock, visit), std::size_t(64));
    ensure_equals("last frame drains short tail", purge.advance(clock, visit), std::size_t(22));
    ensure_equals("no repeat or skipped key", visited.size(), std::size_t(150));
    for (int i = 0; i < 150; ++i)
        ensure_equals("snapshot visited once", visited[static_cast<std::size_t>(i)], 149 - i);
    ensure_equals("warm sweep retains allocation", purge.keys.capacity(), capacity);
    purge.request(entries, [](int key) { return key; });
    ensure_equals("same-size restart reuses vector", purge.keys.capacity(), capacity);
    // A cheap clock allows more than the floor, but still stops at 1 ms.
    now = 0.0;
    auto cheap_clock = [&] { now += 0.00001; return now; };
    const auto count = purge.advance(cheap_clock, [](int) {});
    ensure("time budget allows work beyond floor", count > 64 && count <= 100);
}

template<> template<> void vram_recovery_object::test<2>()
{
    using ALVRAMRecovery::lazyFlushUnreferenced;
    struct Row { bool unused, flush, sliced, branch, reset, deleted; };
    const Row rows[] = {
        { false, false, false, false, true, false },
        { false, false, true,  false, true, false },
        { false, true,  false, false, true, false },
        { false, true,  true,  false, true, false },
        { true,  false, false, false, true, false }, // exact legacy timer reset
        { true,  false, true,  true, false, false }, // preserve unused emergency wait
        { true,  true,  false, true, false, true },
        { true,  true,  true,  true, false, true }
    };
    for (const Row& row : rows)
    {
        const bool branch = lazyFlushUnreferenced(row.unused, row.flush, row.sliced);
        ensure_equals("complete lazy-flush decision table", branch, row.branch);
        ensure_equals("only flushing may delete an expired texture", branch && row.flush, row.deleted);
        ensure_equals("reference timer reset decision", !branch, row.reset);
    }
}

template<> template<> void vram_recovery_object::test<3>()
{
    trip();
    fit(1.0);
    in.available = 11000.f;
    ensure("Normal alone is insufficient", !latch.update(in));
    ensure_equals("insufficient refill restarts hold", latch.ready_since, -1.0);
    in.available = latch.peak_tracked + in.reserve - in.tracked;
    ensure("exact refill equality starts hold", !latch.update(in));
    in.now = 30.9;
    ensure("continuous 30 second hold required", !latch.update(in));
    in.now = 31.0;
    ensure("fit for 30 seconds resumes", latch.update(in));
    ensure("latch cleared", !latch.suspended);
}

template<> template<> void vram_recovery_object::test<4>()
{
    trip();
    fit(1.0);
    latch.update(in);
    in.now = 30.0;
    in.eviction = true;
    latch.update(in);
    ensure_equals("eviction breaks continuous hold", latch.ready_since, -1.0);
    in.eviction = false;
    in.now = 31.0;
    latch.update(in);
    in.now = 60.0;
    ensure("wait again after eviction", !latch.update(in));
    in.now = 61.0;
    in.driver_sample = false;
    ensure("cached telemetry cannot complete hold", !latch.update(in));
    in.driver_sample = true;
    ensure("fresh sample completes hold", latch.update(in));
}

template<> template<> void vram_recovery_object::test<5>()
{
    in.cap_pressure = true;
    trip();
    fit(1.0);
    latch.update(in);
    in.now = 1000.0;
    ensure("cap trip cannot self-release on downrez", !latch.update(in));
    in.effective_cap = 40000.f;
    in.now = 1001.0;
    ensure("cap change still needs driver hold", !latch.update(in));
    in.now = 1031.0;
    ensure("cap change unlocks release", latch.update(in));
    // Driver-only trip also needs the original demand to fit the recovery cap.
    latch = ALVRAMRecovery::QualityLatch();
    in.pressure = true;
    in.cap_pressure = false;
    in.tracked = 12000.f;
    in.effective_cap = 14000.f;
    trip();
    fit(2000.0);
    latch.update(in);
    in.now = 3000.0;
    ensure("refill exceeding recovery cap stays suspended", !latch.update(in));
    in.effective_cap = 15000.f;
    in.now = 3001.0;
    latch.update(in);
    in.now = 3031.0;
    ensure("recovery cap equality accepted", latch.update(in));
}

template<> template<> void vram_recovery_object::test<6>()
{
    trip();
    fit(1.0);
    in.available = -1.f;
    latch.update(in);
    in.available = 20000.f;
    in.now = 1000.0;
    ensure("missing telemetry requires re-toggle even if later restored", !latch.update(in));
    in.now = 1030.0;
    ensure("restored telemetry cannot auto-release after the hold", !latch.update(in));
    in.manual_pin = false;
    in.capture = false;
    ensure("withdraw clears suspension", latch.update(in));
    ensure("withdrawn request unlatched", !latch.suspended);
    in.manual_pin = true;
    in.capture = true;
    in.hold = true;
    ensure("re-toggle still obeys quality hold", !latch.update(in));
    in.hold = false;
    ensure("explicit rearm allowed", latch.update(in));
    in.pressure = true;
    latch.update(in);
    ensure("rearm may suspend again", latch.suspended);
    in.enabled = false;
    in.background = true;
    in.hold = true;
    ensure("governor off bypasses governor restrictions", latch.update(in));
    ensure("governor disable resets suspension", !latch.suspended);
    ensure_equals("governor disable resets doubling", latch.last_suspend, -1.0);
}

template<> template<> void vram_recovery_object::test<7>()
{
    trip();
    for (double expected : { 60.0, 120.0, 240.0, 480.0, 600.0, 600.0 })
    {
        fit(in.now + 1.0);
        in.manual_pin = !in.manual_pin;
        latch.update(in);
        in.pressure = true;
        in.now += 1.0;
        latch.update(in);
        ensure_equals("repeat within ten minutes doubles to ceiling", latch.hold_seconds, expected);
    }
    in.manual_pin = !in.manual_pin;
    in.pressure = false;
    latch.update(in);
    in.now += 601.0;
    in.pressure = true;
    latch.update(in);
    ensure_equals("quiet interval resets doubling", latch.hold_seconds, 30.0);
}

template<> template<> void vram_recovery_object::test<8>()
{
    in.capture = false;
    in.manual_pin = false;
    in.fullres = true;
    trip();
    fit(1.0);
    in.fullres = false;
    ensure("fullres withdrawal clears latch", latch.update(in));
    in.fullres = true;
    in.hold = true;
    ensure("fullres rearm shares quality hold", !latch.update(in));
    in.hold = false;
    in.background = true;
    ensure("background release blocks fullres and pin permission", !latch.update(in));
    in.background = false;
    ensure("foreground recovered permission", latch.update(in));
    in.pressure = true;
    latch.update(in);
    in.pressure = false;
    in.background = true;
    latch.update(in);
    in.background = false;
    in.hold = true;
    ensure("refocus cannot clear latch", !latch.update(in));
    ensure("refocus retains suspension", latch.suspended);
}

template<> template<> void vram_recovery_object::test<9>()
{
    ALVRAMRecovery::EvictionEpisode episode;
    // Already Critical from cap-only pressure: no eviction has occurred yet.
    ensure("cap-only critical does not consume edge", !episode.update(true, false, false));
    ensure("first eviction during cap-only Critical", episode.update(true, false, true));
    ensure("consecutive eviction suppressed", !episode.update(true, false, true));
    ensure("unsampled frame retains episode", !episode.update(true, false, false));
    ensure("later eviction suppressed", !episode.update(true, false, true));
    ensure("system-low edge remains independent",
        ALVRAMRecovery::emergencyRescan(true, episode.update(true, false, true)));
    ensure("eviction edge alone rescans", ALVRAMRecovery::emergencyRescan(false, true));
    ensure("quiet frame does not rescan", !ALVRAMRecovery::emergencyRescan(false, false));
    episode.update(true, true, false);
    ensure("hysteretic Normal clears episode", episode.update(true, false, true));
    episode.update(false, false, false);
    ensure("disable clears episode", episode.update(true, false, true));
}

template<> template<> void vram_recovery_object::test<10>()
{
    using ALVRAMRecovery::driverReserve;
    ensure_equals("small GPU retains 256 MB minimum", driverReserve(1024.f), 256.f);
    ensure_equals("4 GB retains ten percent", driverReserve(4096.f), 409.6f);
    ensure("32 GB card reserve about 3261 MB", std::abs(driverReserve(32607.f) - 3260.7f) < 0.01f);
    ensure_equals("maximum raised only to 4096 MB", driverReserve(100000.f), 4096.f);
}

template<> template<> void vram_recovery_object::test<11>()
{
    in.capture = false;
    in.manual_pin = false;
    ensure("ordinary quality yields during pressure", !latch.update(in));
    ensure("no override request never suspends", !latch.suspended);
    in.pressure = false;
    in.hold = true;
    ensure("ordinary hold blocks override permission", !latch.update(in));
    in.hold = false;
    ensure("normal recovered permission", latch.update(in));
    in.capture = true; // automatic capture with no manual pin
    in.pressure = true;
    latch.update(in);
    ensure("automatic capture also suspends", latch.suspended);
    in.capture = false;
    latch.update(in);
    ensure("actual request withdrawal clears automatic suspension", !latch.suspended);
}


template<> template<> void vram_recovery_object::test<12>()
{
    trip();
    for (double expected : { 60.0, 120.0, 240.0, 480.0, 600.0, 600.0 })
    {
        fit(in.now + 1.0);
        latch.update(in);
        in.now += latch.hold_seconds;
        ensure("fit held long enough resumes automatically", latch.update(in));
        in.now += 1.0;
        in.pressure = true;
        latch.update(in);
        ensure_equals("immediate repeat after a long hold retains backoff", latch.hold_seconds, expected);
    }
}

}
