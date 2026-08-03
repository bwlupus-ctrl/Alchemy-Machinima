/**
 * @file bdmergetexpool.cpp
 * @brief [BDMerge G5.1] Decoded-texture RAM pool. See header.
 *
 * $LicenseInfo:firstyear=2026&license=lgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, bwlupus-ctrl (machinima fork, spec item G5.1 Stage 2)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "bdmergetexpool.h"

#include "llagent.h"
#include "llimage.h"
#include "llmemory.h"
#include "llmutex.h"
#include "llsys.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"

#include <atomic>
#include <list>
#include <memory>
#include <unordered_map>

namespace
{

struct Entry
{
    std::unique_ptr<U8[]> mData;
    S32 mDataSize = 0;
    U16 mWidth = 0;
    U16 mHeight = 0;
    S8 mComponents = 0;
    S8 mDiscard = -1;
    F64 mLastHit = 0.0;       // [B-2b.1] last put/fetch time (LLTimer::getTotalSeconds)
    U64 mRegion = 0;          // [B-2b.1] agent region handle at insert time
    std::list<LLUUID>::iterator mLRUIt;
};

// Settings mirror, written on the main thread in refreshSettings(), read on
// worker threads.
std::atomic<bool> sEnabled{ false };
std::atomic<U64> sBudgetBytes{ 0 };
// [B-2b.1] tier parameters, mirrored on the main thread like the rest
std::atomic<U64> sCurrentRegion{ 0 };
std::atomic<F32> sRegionGraceTTL{ 900.f };
std::atomic<F32> sRecencyWindow{ 300.f };

LLMutex sMutex;
// Guarded by sMutex.
std::unordered_map<LLUUID, Entry> sEntries;
std::list<LLUUID> sLRU; // front = most recently used
U64 sBytes = 0;

// Stats (guarded by sMutex).
U64 sHits = 0;
U64 sMisses = 0;
U64 sInserts = 0;
U64 sReplacedFinerSkips = 0;
U64 sEvictions = 0;
F64 sHitBytes = 0.0;

// [B-2b.1] tiered eviction, oldest-first within each pass:
//   pass 0 - departed-region entries past the region grace TTL
//   pass 1 - any entry idle past the recency window
//   pass 2 - plain LRU tail (stock behavior, budget backstop)
// Region returns within the TTL and recently-visible content survive
// pressure that would otherwise dump them in pure-LRU order.
void evictPassLocked(U64 budget, S32 pass)
{
    if (sLRU.empty() || sBytes <= budget)
    {
        return;
    }
    const F64 now = LLTimer::getTotalSeconds();
    const U64 region = sCurrentRegion.load(std::memory_order_relaxed);
    const F64 grace = sRegionGraceTTL.load(std::memory_order_relaxed);
    const F64 recency = sRecencyWindow.load(std::memory_order_relaxed);

    auto it = std::prev(sLRU.end());
    while (sBytes > budget)
    {
        const bool at_begin = (it == sLRU.begin());
        auto cur = it;
        if (!at_begin)
        {
            --it;
        }

        auto eit = sEntries.find(*cur);
        bool victim = true;
        if (eit == sEntries.end())
        { // stale key; drop from the list either way
            sLRU.erase(cur);
            if (at_begin) break;
            continue;
        }
        else if (pass == 0)
        {
            victim = (eit->second.mRegion != region) && (now - eit->second.mLastHit > grace);
        }
        else if (pass == 1)
        {
            victim = (now - eit->second.mLastHit > recency);
        }

        if (victim)
        {
            sBytes -= eit->second.mDataSize;
            sEntries.erase(eit);
            sLRU.erase(cur);
            sEvictions++;
        }
        if (at_begin) break;
    }
}

void evictToBudgetLocked(U64 budget)
{
    for (S32 pass = 0; pass < 3 && sBytes > budget; pass++)
    {
        evictPassLocked(budget, pass);
    }
}

} // anonymous namespace

//static
void BDMergeTexPool::refreshSettings()
{
    static LLCachedControl<bool> pool_enable(gSavedSettings, "BDMergeTexPoolEnable", true);
    static LLCachedControl<F32> pool_fraction(gSavedSettings, "BDMergeTexPoolFraction", 0.5f);
    static LLCachedControl<U32> pool_max_mb(gSavedSettings, "BDMergeTexPoolMaxMB", 0);
    static LLCachedControl<U32> pool_floor_mb(gSavedSettings, "BDMergeTexPoolFloorMB", 1024);
    static LLCachedControl<U32> pool_reserve_mb(gSavedSettings, "BDMergePoolReserveMB", 6144);

    static LLCachedControl<F32> region_grace(gSavedSettings, "BDMergeTexPoolRegionGraceTTL", 900.f);
    static LLCachedControl<F32> recency_window(gSavedSettings, "BDMergeTexPoolRecencyWindow", 300.f);
    sRegionGraceTTL.store(llmax((F32)region_grace, 0.f), std::memory_order_relaxed);
    sRecencyWindow.store(llmax((F32)recency_window, 0.f), std::memory_order_relaxed);
    if (LLViewerRegion* regionp = gAgent.getRegion())
    {
        sCurrentRegion.store(regionp->getHandle(), std::memory_order_relaxed);
    }

    bool enable = pool_enable;
    U64 budget;
    if (pool_max_mb > 0)
    { // manual override
        budget = U64(pool_max_mb) * 1024u * 1024u;
    }
    else
    { // Auto-size from detected physical RAM. Take the fraction of the RAM left
      // AFTER reserving headroom for the OS + the viewer's own (non-pool) working
      // set -- NOT of total RAM. A flat fraction of total starves low-RAM machines:
      // 50% of 12GB is 6GB of decoded textures on top of the OS and the rest of the
      // viewer -> thrash/OOM. On a big rig the reserve is a rounding error so the
      // pool is effectively unchanged; on a small one it shrinks hard. The pool is
      // never allowed to exceed the headroom-adjusted available, even past the floor.
        const U64 phys_bytes    = U64(gSysMemory.getPhysicalMemoryKB().value()) * 1024u;
        const U64 reserve_bytes = U64(pool_reserve_mb) * 1024u * 1024u;
        // Available = what's left after the reserve, but never less than a quarter
        // of physical RAM. The phys/4 floor keeps the sizing monotonic: without it,
        // a machine reporting just over the reserve (an 8GB box with an iGPU
        // carve-out reports ~6.3GB) would get a near-zero pool while a strictly
        // smaller machine fell back to phys/4. For phys >= 4/3 * reserve this is
        // exactly (phys - reserve), so bigger machines are unaffected.
        const U64 headroom_bytes = (phys_bytes > reserve_bytes)
                                       ? (phys_bytes - reserve_bytes)
                                       : U64(0);
        const U64 avail_bytes   = llmax(headroom_bytes, phys_bytes / 4u);
        const F32 fraction      = llclamp((F32)pool_fraction, 0.05f, 0.9f);
        U64 want = U64((F64)avail_bytes * (F64)fraction);
        want = llmax(want, U64(pool_floor_mb) * 1024u * 1024u); // at least the floor...
        budget = llmin(want, avail_bytes);                      // ...but capped at avail
    }
    sBudgetBytes.store(budget, std::memory_order_relaxed);

    bool was_enabled = sEnabled.exchange(enable, std::memory_order_relaxed);
    if (was_enabled && !enable)
    { // gate turned off: drop everything so off == stock memory footprint
        LLMutexLock lock(&sMutex);
        sEntries.clear();
        sLRU.clear();
        sBytes = 0;
    }
    else if (enable)
    { // budget may have shrunk
        LLMutexLock lock(&sMutex);
        evictToBudgetLocked(sBudgetBytes.load(std::memory_order_relaxed));
    }
}

//static
bool BDMergeTexPool::enabled()
{
    return sEnabled.load(std::memory_order_relaxed);
}

//static
LLPointer<LLImageRaw> BDMergeTexPool::fetch(const LLUUID& id, S32 desired_discard, S32& entry_discard)
{
    if (!enabled())
    {
        return nullptr;
    }

    LLMutexLock lock(&sMutex);
    auto it = sEntries.find(id);
    // serve only entries at the desired discard or finer (lower = finer)
    if (it == sEntries.end() || desired_discard < 0 || it->second.mDiscard > desired_discard)
    {
        sMisses++;
        return nullptr;
    }

    Entry& e = it->second;
    LLPointer<LLImageRaw> raw = new LLImageRaw(e.mWidth, e.mHeight, e.mComponents);
    if (raw->isBufferInvalid() || raw->getDataSize() != e.mDataSize)
    {
        sMisses++;
        return nullptr;
    }
    memcpy(raw->getData(), e.mData.get(), e.mDataSize);
    entry_discard = e.mDiscard;

    sLRU.splice(sLRU.begin(), sLRU, e.mLRUIt); // touch
    e.mLastHit = LLTimer::getTotalSeconds();
    e.mRegion = sCurrentRegion.load(std::memory_order_relaxed);
    sHits++;
    sHitBytes += e.mDataSize;
    return raw;
}

//static
void BDMergeTexPool::put(const LLUUID& id, S32 discard, const LLImageRaw* raw)
{
    if (!enabled() || !raw || raw->isBufferInvalid() || discard < 0)
    {
        return;
    }

    const S32 data_size = raw->getDataSize();
    const U64 budget = sBudgetBytes.load(std::memory_order_relaxed);
    if (data_size <= 0 || U64(data_size) > budget)
    {
        return;
    }

    // copy outside the lock
    std::unique_ptr<U8[]> data(new(std::nothrow) U8[data_size]);
    if (!data)
    {
        return;
    }
    memcpy(data.get(), raw->getData(), data_size);

    LLMutexLock lock(&sMutex);
    auto it = sEntries.find(id);
    if (it != sEntries.end())
    {
        if (it->second.mDiscard < discard)
        { // existing entry is finer than the incoming one; keep it
            sReplacedFinerSkips++;
            return;
        }
        sBytes -= it->second.mDataSize;
        sLRU.erase(it->second.mLRUIt);
        sEntries.erase(it);
    }

    sLRU.push_front(id);
    Entry& e = sEntries[id];
    e.mData = std::move(data);
    e.mDataSize = data_size;
    e.mWidth = (U16)raw->getWidth();
    e.mHeight = (U16)raw->getHeight();
    e.mComponents = (S8)raw->getComponents();
    e.mDiscard = (S8)discard;
    e.mLastHit = LLTimer::getTotalSeconds();
    e.mRegion = sCurrentRegion.load(std::memory_order_relaxed);
    e.mLRUIt = sLRU.begin();
    sBytes += data_size;
    sInserts++;

    evictToBudgetLocked(budget);
}

//static
std::string BDMergeTexPool::shortStatus()
{
    if (!enabled())
    {
        return "off";
    }
    U64 entries, bytes, hits, misses;
    {
        LLMutexLock lock(&sMutex);
        entries = sEntries.size();
        bytes = sBytes;
        hits = sHits;
        misses = sMisses;
    }
    const F64 GB = 1024.0 * 1024.0 * 1024.0;
    return llformat("%.1f / %.0f GB - %llu textures - %.1f%% hits",
                    bytes / GB, sBudgetBytes.load(std::memory_order_relaxed) / GB,
                    entries, (hits + misses) ? 100.0 * hits / (hits + misses) : 0.0);
}

//static
void BDMergeTexPool::appendReport(std::ostream& out)
{
    U64 entries, bytes, hits, misses, inserts, evictions;
    F64 hit_bytes;
    {
        LLMutexLock lock(&sMutex);
        entries = sEntries.size();
        bytes = sBytes;
        hits = sHits;
        misses = sMisses;
        inserts = sInserts;
        evictions = sEvictions;
        hit_bytes = sHitBytes;
    }
    const F64 MB = 1024.0 * 1024.0;
    out << llformat("    decoded RAM pool: %s, %llu entries, %.1f/%.1f GB, hits %llu / lookups %llu (%.1f%%), %.1f MB served from RAM, inserts %llu, evictions %llu",
                    enabled() ? "on" : "off", entries,
                    bytes / (MB * 1024.0), sBudgetBytes.load(std::memory_order_relaxed) / (MB * 1024.0),
                    hits, hits + misses,
                    (hits + misses) ? 100.0 * hits / (hits + misses) : 0.0,
                    hit_bytes / MB, inserts, evictions);
}
