/**
 * @file bdmergememorybudget.cpp
 * @brief Shared pressure-aware budget for decoded texture and mesh pools.
 *
 * The two caches used to take independent fractions of installed RAM. That
 * double-counted the same headroom and could drive process commit to the
 * system limit after a dense-region teleport. This governor treats them as
 * one optional/reclaimable cache and sizes it from live allocation headroom.
 */

#include "llviewerprecompiledheaders.h"

#include "bdmergememorybudget.h"

#include "bdmergemeshpool.h"
#include "bdmergetexpool.h"
#include "llframetimer.h"
#include "llmemory.h"
#include "llsys.h"
#include "llviewercontrol.h"

#include <atomic>
#include <limits>
#include <ostream>

namespace
{
constexpr U64 KIB = 1024ull;
constexpr U64 MIB = 1024ull * KIB;
constexpr U64 GIB = 1024ull * MIB;
constexpr F32 REFRESH_SECONDS = 1.f;

enum PressureState : S32
{
    PRESSURE_NORMAL = 0,
    PRESSURE_SOFT,
    PRESSURE_CRITICAL
};

std::atomic<S32> sPressure{ PRESSURE_NORMAL };
std::atomic<U64> sEffectiveAvailable{ 0 };
std::atomic<U64> sPhysicalAvailable{ 0 };
std::atomic<U64> sCommitAvailable{ 0 };
std::atomic<U64> sPrivateCommit{ 0 };
std::atomic<U64> sReserve{ 0 };
std::atomic<U64> sTextureBudget{ 0 };
std::atomic<U64> sMeshBudget{ 0 };
std::atomic<U64> sDecodedBytes{ 0 };

LLFrameTimer sRefreshTimer;
bool sInitialized = false; // main thread only

const char* pressureName(S32 state)
{
    switch (state)
    {
        case PRESSURE_SOFT: return "soft";
        case PRESSURE_CRITICAL: return "critical";
        default: return "normal";
    }
}

U64 bytesFromMB(U32 value)
{
    return U64(value) * MIB;
}

U64 proportionalShare(U64 total, U64 part, U64 whole)
{
    if (total == 0 || part == 0 || whole == 0)
    {
        return 0;
    }
    // Avoid overflowing a U64 product on high-memory workstations.
    return U64((long double)total * (long double)part / (long double)whole);
}

U64 desiredPoolBudget(bool enabled, U32 manual_mb, U32 floor_mb,
                      F32 fraction, F32 minimum_fraction, F32 maximum_fraction,
                      U64 usable_bytes)
{
    if (!enabled || usable_bytes == 0)
    {
        return 0;
    }

    U64 desired = manual_mb > 0
        ? bytesFromMB(manual_mb)
        : U64((long double)usable_bytes *
              (long double)llclamp(fraction, minimum_fraction, maximum_fraction));

    desired = llmax(desired, bytesFromMB(floor_mb));
    return llmin(desired, usable_bytes);
}
} // anonymous namespace

//static
void BDMergeMemoryBudget::refresh()
{
    if (sInitialized && sRefreshTimer.getElapsedTimeF32() < REFRESH_SECONDS)
    {
        return;
    }
    sRefreshTimer.reset();

    static LLCachedControl<bool> texture_enabled(gSavedSettings, "BDMergeTexPoolEnable", true);
    static LLCachedControl<F32> texture_fraction(gSavedSettings, "BDMergeTexPoolFraction", 0.5f);
    static LLCachedControl<U32> texture_max_mb(gSavedSettings, "BDMergeTexPoolMaxMB", 0);
    static LLCachedControl<U32> texture_floor_mb(gSavedSettings, "BDMergeTexPoolFloorMB", 1024);
    static LLCachedControl<bool> mesh_enabled(gSavedSettings, "BDMergeMeshPoolEnable", true);
    static LLCachedControl<F32> mesh_fraction(gSavedSettings, "BDMergeMeshPoolFraction", 0.1f);
    static LLCachedControl<U32> mesh_max_mb(gSavedSettings, "BDMergeMeshPoolMaxMB", 0);
    static LLCachedControl<U32> mesh_floor_mb(gSavedSettings, "BDMergeMeshPoolFloorMB", 256);
    static LLCachedControl<U32> reserve_mb(gSavedSettings, "BDMergePoolReserveMB", 6144);
    static LLCachedControl<F32> reserve_fraction(gSavedSettings, "BDMergePoolReserveFraction", 0.25f);

    LLMemory::updateMemoryInfo();

    const U64 installed = U64(gSysMemory.getPhysicalMemoryKB().value()) * KIB;
    const U64 absolute_floor = llmin(bytesFromMB(reserve_mb), installed / 2u);
    const U64 fractional_floor = U64((long double)installed *
                                     (long double)llclamp((F32)reserve_fraction, 0.10f, 0.50f));
    const U64 reserve = llmax(absolute_floor, fractional_floor);
    const U64 usable = installed > reserve ? installed - reserve : 0;

    U64 desired_texture = desiredPoolBudget(texture_enabled, texture_max_mb,
                                            texture_floor_mb, texture_fraction,
                                            0.05f, 0.90f, usable);
    U64 desired_mesh = desiredPoolBudget(mesh_enabled, mesh_max_mb,
                                         mesh_floor_mb, mesh_fraction,
                                         0.01f, 0.50f, usable);

    // Manual targets and fractions are preferences, not permission to count
    // the same post-reserve RAM twice.
    U64 desired_total = desired_texture + desired_mesh;
    if (desired_total > usable && desired_total > 0)
    {
        const U64 normalized_texture = proportionalShare(usable, desired_texture, desired_total);
        desired_texture = normalized_texture;
        desired_mesh = usable - normalized_texture;
        desired_total = usable;
    }

    const U64 physical_available = U64(LLMemory::getAvailablePhysicalMemKB().value()) * KIB;
    const U64 commit_available = U64(LLMemory::getAvailableCommitKB().value()) * KIB;
    const U64 effective_available = U64(LLMemory::getAvailableMemKB().value()) * KIB;
    const U64 private_commit = U64(LLMemory::getAllocatedPrivateMemKB().value()) * KIB;
    const U64 texture_bytes = BDMergeTexPool::getBytes();
    const U64 mesh_bytes = BDMergeMeshPool::getBytes();
    const U64 decoded_bytes = texture_bytes + mesh_bytes;

    // Existing cache bytes are reclaimable. Add only availability above the
    // reserve, or shrink by the reserve deficit when the machine is pressured.
    U64 live_cap = 0;
    if (effective_available >= reserve)
    {
        const U64 growth = effective_available - reserve;
        const U64 maximum = std::numeric_limits<U64>::max();
        live_cap = decoded_bytes > maximum - growth ? maximum : decoded_bytes + growth;
    }
    else
    {
        const U64 deficit = reserve - effective_available;
        live_cap = decoded_bytes > deficit ? decoded_bytes - deficit : 0;
    }

    const U64 combined_budget = llmin(desired_total, live_cap);
    U64 texture_budget = proportionalShare(combined_budget, desired_texture, desired_total);
    U64 mesh_budget = combined_budget - texture_budget;
    if (!texture_enabled)
    {
        texture_budget = 0;
        mesh_budget = combined_budget;
    }
    if (!mesh_enabled)
    {
        mesh_budget = 0;
        texture_budget = combined_budget;
    }

    const U64 critical_threshold = llmax(reserve / 2u, GIB);
    const U64 recovery_margin = llmax(reserve / 8u, GIB);
    const S32 previous = sPressure.load(std::memory_order_relaxed);
    S32 next = PRESSURE_NORMAL;

    if (LLMemory::isSystemMemoryLow() || effective_available < critical_threshold)
    {
        next = PRESSURE_CRITICAL;
    }
    else if (previous == PRESSURE_CRITICAL &&
             effective_available < critical_threshold + recovery_margin)
    {
        next = PRESSURE_CRITICAL;
    }
    else if (effective_available < reserve)
    {
        next = PRESSURE_SOFT;
    }
    else if (previous == PRESSURE_SOFT &&
             effective_available < reserve + recovery_margin)
    {
        next = PRESSURE_SOFT;
    }

    const bool allow_inserts = next == PRESSURE_NORMAL;
    BDMergeTexPool::configure(texture_enabled, texture_budget, allow_inserts);
    BDMergeMeshPool::configure(mesh_enabled, mesh_budget, allow_inserts);

    sPressure.store(next, std::memory_order_relaxed);
    sEffectiveAvailable.store(effective_available, std::memory_order_relaxed);
    sPhysicalAvailable.store(physical_available, std::memory_order_relaxed);
    sCommitAvailable.store(commit_available, std::memory_order_relaxed);
    sPrivateCommit.store(private_commit, std::memory_order_relaxed);
    sReserve.store(reserve, std::memory_order_relaxed);
    sTextureBudget.store(texture_budget, std::memory_order_relaxed);
    sMeshBudget.store(mesh_budget, std::memory_order_relaxed);
    sDecodedBytes.store(decoded_bytes, std::memory_order_relaxed);

    if (!sInitialized || next != previous)
    {
        LL_INFOS("BDMergeMemory")
            << (sInitialized ? "Memory pressure changed to " : "Memory governor initialized at ")
            << pressureName(next)
            << "; effective available " << llformat("%.1f GB", effective_available / (F64)GIB)
            << ", physical " << llformat("%.1f GB", physical_available / (F64)GIB)
            << ", commit " << llformat("%.1f GB", commit_available / (F64)GIB)
            << ", private " << llformat("%.1f GB", private_commit / (F64)GIB)
            << ", reserve " << llformat("%.1f GB", reserve / (F64)GIB)
            << ", decoded pools " << llformat("%.1f/%.1f GB", decoded_bytes / (F64)GIB,
                                                combined_budget / (F64)GIB)
            << LL_ENDL;
    }
    sInitialized = true;
}

//static
bool BDMergeMemoryBudget::isCritical()
{
    return sPressure.load(std::memory_order_relaxed) == PRESSURE_CRITICAL;
}

//static
void BDMergeMemoryBudget::appendReport(std::ostream& out)
{
    const F64 gb = (F64)GIB;
    out << llformat("    memory governor: %s, effective %.2f GB (physical %.2f, commit %.2f), private %.2f GB, reserve %.2f GB, decoded %.2f GB, budgets texture %.2f + mesh %.2f GB",
                    pressureName(sPressure.load(std::memory_order_relaxed)),
                    sEffectiveAvailable.load(std::memory_order_relaxed) / gb,
                    sPhysicalAvailable.load(std::memory_order_relaxed) / gb,
                    sCommitAvailable.load(std::memory_order_relaxed) / gb,
                    sPrivateCommit.load(std::memory_order_relaxed) / gb,
                    sReserve.load(std::memory_order_relaxed) / gb,
                    sDecodedBytes.load(std::memory_order_relaxed) / gb,
                    sTextureBudget.load(std::memory_order_relaxed) / gb,
                    sMeshBudget.load(std::memory_order_relaxed) / gb);
}
