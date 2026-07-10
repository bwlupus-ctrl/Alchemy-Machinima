/**
 * @file bdmergemeshpool.cpp
 * @brief [BDMerge G5.1-2b] Decoded-mesh RAM pool. See header.
 *
 * $LicenseInfo:firstyear=2026&license=lgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, bwlupus-ctrl (machinima fork, spec item G5.1 Stage 2b)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "bdmergemeshpool.h"

#include "llmemory.h"
#include "llmutex.h"
#include "llsys.h"
#include "llviewercontrol.h"
#include "llvolume.h"
#include "llvolumemgr.h"

#include <atomic>
#include <list>
#include <unordered_map>

namespace
{

struct Key
{
    LLUUID mID;
    S32 mLOD;
    bool operator==(const Key& o) const { return mLOD == o.mLOD && mID == o.mID; }
};

struct KeyHash
{
    size_t operator()(const Key& k) const
    {
        return std::hash<LLUUID>()(k.mID) ^ (size_t(k.mLOD) * 0x9e3779b97f4a7c15ull);
    }
};

struct Entry
{
    // exclusively owned by the pool; refcount only ever touched under sMutex
    LLPointer<LLVolume> mVolume;
    U64 mBytes = 0;
    std::list<Key>::iterator mLRUIt;
};

U64 estimateVolumeBytes(const LLVolume* v)
{
    U64 bytes = 0;
    S32 num_faces = v->getNumVolumeFaces();
    for (S32 i = 0; i < num_faces; i++)
    {
        const LLVolumeFace& f = v->getVolumeFace(i);
        U64 verts = (U64)llmax(f.mNumVertices, 0);
        U64 inds = (U64)llmax(f.mNumIndices, 0);
        U64 per_vert = 16 /*pos*/ + 16 /*norm*/ + 8 /*tc*/;
        if (f.mTangents) per_vert += 16;
        if (f.mWeights) per_vert += 16;
        bytes += verts * per_vert + inds * 2 + sizeof(LLVolumeFace);
    }
    return bytes;
}

std::atomic<bool> sEnabled{ false };
std::atomic<U64> sBudgetBytes{ 0 };

LLMutex sMutex;
// Guarded by sMutex.
std::unordered_map<Key, Entry, KeyHash> sEntries;
std::list<Key> sLRU; // front = most recently used
U64 sBytes = 0;

// Stats (guarded by sMutex).
U64 sHits = 0;
U64 sMisses = 0;
U64 sInserts = 0;
U64 sEvictions = 0;
F64 sHitBytes = 0.0;

void evictToBudgetLocked(U64 budget)
{
    while (sBytes > budget && !sLRU.empty())
    {
        auto it = sEntries.find(sLRU.back());
        if (it != sEntries.end())
        {
            sBytes -= it->second.mBytes;
            sEntries.erase(it);
            sEvictions++;
        }
        sLRU.pop_back();
    }
}

} // anonymous namespace

//static
void BDMergeMeshPool::refreshSettings()
{
    static LLCachedControl<bool> pool_enable(gSavedSettings, "BDMergeMeshPoolEnable", true);
    static LLCachedControl<F32> pool_fraction(gSavedSettings, "BDMergeMeshPoolFraction", 0.1f);
    static LLCachedControl<U32> pool_max_mb(gSavedSettings, "BDMergeMeshPoolMaxMB", 0);
    static LLCachedControl<U32> pool_floor_mb(gSavedSettings, "BDMergeMeshPoolFloorMB", 256);

    bool enable = pool_enable;
    U64 budget;
    if (pool_max_mb > 0)
    {
        budget = U64(pool_max_mb) * 1024u * 1024u;
    }
    else
    {
        const U64 phys_bytes = U64(gSysMemory.getPhysicalMemoryKB().value()) * 1024u;
        const F32 fraction = llclamp((F32)pool_fraction, 0.01f, 0.5f);
        budget = llmax(U64(phys_bytes * fraction), U64(pool_floor_mb) * 1024u * 1024u);
    }
    sBudgetBytes.store(budget, std::memory_order_relaxed);

    bool was_enabled = sEnabled.exchange(enable, std::memory_order_relaxed);
    if (was_enabled && !enable)
    {
        LLMutexLock lock(&sMutex);
        sEntries.clear();
        sLRU.clear();
        sBytes = 0;
    }
    else if (enable)
    {
        LLMutexLock lock(&sMutex);
        evictToBudgetLocked(sBudgetBytes.load(std::memory_order_relaxed));
    }
}

//static
bool BDMergeMeshPool::enabled()
{
    return sEnabled.load(std::memory_order_relaxed);
}

//static
bool BDMergeMeshPool::fetch(const LLUUID& mesh_id, S32 lod, LLVolume* dest)
{
    if (!enabled() || !dest)
    {
        return false;
    }

    LLMutexLock lock(&sMutex);
    auto it = sEntries.find(Key{ mesh_id, lod });
    if (it == sEntries.end())
    {
        sMisses++;
        return false;
    }

    Entry& e = it->second;
    dest->copyVolumeFaces(e.mVolume); // deep copy (LLVolumeFace::operator=)
    sLRU.splice(sLRU.begin(), sLRU, e.mLRUIt); // touch
    sHits++;
    sHitBytes += e.mBytes;
    return true;
}

//static
void BDMergeMeshPool::put(const LLUUID& mesh_id, S32 lod, const LLVolumeParams& params, const LLVolume* src)
{
    if (!enabled() || !src || src->getNumVolumeFaces() <= 0)
    {
        return;
    }

    const U64 bytes = estimateVolumeBytes(src);
    const U64 budget = sBudgetBytes.load(std::memory_order_relaxed);
    if (bytes == 0 || bytes > budget)
    {
        return;
    }

    // deep copy outside the lock
    LLPointer<LLVolume> master = new LLVolume(params, LLVolumeLODGroup::getVolumeScaleFromDetail(lod));
    master->copyVolumeFaces(src);

    Key key{ mesh_id, lod };
    LLMutexLock lock(&sMutex);
    auto it = sEntries.find(key);
    if (it != sEntries.end())
    { // refresh existing (mesh content for a UUID is immutable in SL, but a
      // re-decode may carry newly generated data such as tangents)
        sBytes -= it->second.mBytes;
        sLRU.erase(it->second.mLRUIt);
        sEntries.erase(it);
    }

    sLRU.push_front(key);
    Entry& e = sEntries[key];
    e.mVolume = master;
    e.mBytes = bytes;
    e.mLRUIt = sLRU.begin();
    sBytes += bytes;
    sInserts++;

    evictToBudgetLocked(budget);
}

//static
void BDMergeMeshPool::appendReport(std::ostream& out)
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
    out << llformat("    decoded mesh pool: %s, %llu entries, %.2f/%.2f GB, hits %llu / lookups %llu (%.1f%%), %.1f MB served from RAM, inserts %llu, evictions %llu",
                    enabled() ? "on" : "off", entries,
                    bytes / (MB * 1024.0), sBudgetBytes.load(std::memory_order_relaxed) / (MB * 1024.0),
                    hits, hits + misses,
                    (hits + misses) ? 100.0 * hits / (hits + misses) : 0.0,
                    hit_bytes / MB, inserts, evictions);
}
