#ifndef AL_VRAM_RECOVERY_H
#define AL_VRAM_RECOVERY_H

#include <algorithm>
#include <cstddef>
#include <vector>

// Pure policy shared by the viewer and the standalone TUT tests. Times are seconds,
// allocations are MB. No GL queries, settings reads or texture references live here.
namespace ALVRAMRecovery
{
constexpr float MIN_DRIVER_RESERVE_MB = 256.f;
constexpr float MAX_DRIVER_RESERVE_MB = 4096.f;
constexpr float DRIVER_RESERVE_PERCENTAGE = 0.10f;
inline float driverReserve(float detected)
{
    return std::clamp(detected * DRIVER_RESERVE_PERCENTAGE,
                      MIN_DRIVER_RESERVE_MB, MAX_DRIVER_RESERVE_MB);
}

inline bool visitAllowed(std::size_t visited, double elapsed)
{
    return visited < 64 || elapsed < 0.001;
}

template<class Key> struct Purge
{
    // pop_back retains capacity; an in-flight sweep never restarts on a new request.
    std::vector<Key> keys;
    template<class Range, class KeyOf> void request(const Range& range, KeyOf key_of)
    {
        if (keys.empty())
        {
            for (const auto& entry : range)
            {
                keys.emplace_back(key_of(entry));
            }
        }
    }
    template<class Clock, class Visit> std::size_t advance(Clock clock, Visit visit)
    {
        const double start = clock();
        std::size_t visited = 0;
        while (!keys.empty() && visitAllowed(visited, clock() - start))
        {
            const Key key = keys.back();
            keys.pop_back();
            visit(key);
            ++visited;
        }
        return visited;
    }
};

inline bool lazyFlushUnreferenced(bool unreferenced, bool flush_images, bool sliced)
{
    // false selects the original referenced branch (including its timer reset).
    return unreferenced && (flush_images || sliced);
}

inline bool emergencyRescan(bool system_low_started, bool eviction_started)
{
    return system_low_started || eviction_started;
}

struct EvictionEpisode
{
    bool seen = false;
    bool update(bool enabled, bool normal, bool eviction)
    {
        if (!enabled || normal)
        {
            seen = false;
        }
        const bool started = enabled && eviction && !seen;
        seen = seen || started;
        return started;
    }
};

struct QualityInput
{
    bool enabled = false;
    bool capture = false;
    bool fullres = false;
    bool manual_pin = false;
    bool pressure = false;
    bool cap_pressure = false;
    bool eviction = false;
    bool hold = false;
    bool background = false;
    bool driver_sample = false;
    double now = 0.0;
    float tracked = 0.f;
    float available = -1.f;
    float reserve = 0.f;
    float effective_cap = 0.f;
    float recovery_cap_percentage = 0.80f;
};

struct QualityLatch
{
    bool suspended = false;
    bool cap_blocked = false;
    bool telemetry_missing = false;
    bool previous_pin = false;
    bool previous_fullres = false;
    float peak_tracked = 0.f;
    float suspension_cap = 0.f;
    double ready_since = -1.0;
    double last_suspend = -1.0;
    double last_release = -1.0;
    double hold_seconds = 30.0;

    bool update(const QualityInput& in)
    {
        if (!in.enabled)
        {
            *this = QualityLatch();
            return true;
        }
        const bool rearmed = in.manual_pin != previous_pin || in.fullres != previous_fullres;
        previous_pin = in.manual_pin;
        previous_fullres = in.fullres;
        const bool requested = in.capture || in.fullres;
        if (!requested || rearmed)
        {
            if (suspended)
            {
                last_release = in.now;
            }
            suspended = false;
            cap_blocked = false;
            telemetry_missing = false;
            ready_since = -1.0;
        }
        if (requested && in.pressure && !suspended)
        {
            // Count quiet time after release, so a ten-minute hold cannot
            // itself reset the backoff before an immediate repeat suspension.
            hold_seconds = last_release >= 0.0 && in.now - last_release <= 600.0
                ? std::min(hold_seconds * 2.0, 600.0) : 30.0;
            last_suspend = in.now;
            peak_tracked = in.tracked;
            suspension_cap = in.effective_cap;
            cap_blocked = in.cap_pressure;
            telemetry_missing = in.available < 0.f;
            ready_since = -1.0;
            suspended = true;
        }
        if (suspended)
        {
            // Losing telemetry cannot certify that a refill fits. Require re-toggle.
            telemetry_missing = telemetry_missing || in.available < 0.f;
            if (in.effective_cap != suspension_cap)
            {
                cap_blocked = false;
            }
            const bool fits = !in.pressure && !in.eviction && !cap_blocked && !telemetry_missing
                && peak_tracked <= in.recovery_cap_percentage * in.effective_cap
                && peak_tracked + in.reserve <= in.tracked + in.available;
            if (!fits)
            {
                ready_since = -1.0;
            }
            else if (in.driver_sample)
            {
                // Only fresh driver samples can start or complete the continuous hold.
                if (ready_since < 0.0)
                {
                    ready_since = in.now;
                }
                else if (in.now - ready_since >= hold_seconds)
                {
                    suspended = false;
                    last_release = in.now;
                    ready_since = -1.0;
                }
            }
        }
        return !in.pressure && !in.hold && !in.background && !suspended;
    }
};
}
#endif
