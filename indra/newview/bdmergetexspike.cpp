/**
 * @file bdmergetexspike.cpp
 * @brief [BDMerge G5.1-S1] Texture-pipeline latency attribution. See header.
 *
 * $LicenseInfo:firstyear=2026&license=lgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, bwlupus-ctrl (machinima fork, spec item G5.1 Stage 1)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "bdmergetexspike.h"

#include "llmutex.h"
#include "lltimer.h"
#include "llviewercontrol.h"

#include <atomic>
#include <unordered_set>

namespace
{

// Log-scale latency histogram; approximate percentiles reported as the upper
// bound of the bucket the percentile falls in.
constexpr F32 BUCKET_UPPER_MS[] = { 0.5f, 1.f, 2.f, 5.f, 10.f, 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f };
constexpr S32 NUM_BUCKETS = sizeof(BUCKET_UPPER_MS) / sizeof(BUCKET_UPPER_MS[0]) + 1; // +1 = overflow

struct Histogram
{
    U64 mCount = 0;
    F64 mTotal = 0.0;
    F32 mMax = 0.f;
    U64 mBuckets[NUM_BUCKETS] = { 0 };

    void record(F32 seconds)
    {
        mCount++;
        mTotal += seconds;
        mMax = llmax(mMax, seconds);
        F32 ms = seconds * 1000.f;
        S32 b = 0;
        while (b < NUM_BUCKETS - 1 && ms > BUCKET_UPPER_MS[b]) b++;
        mBuckets[b]++;
    }

    F32 percentileMS(F32 pct) const
    {
        if (mCount == 0) return 0.f;
        U64 threshold = (U64)(mCount * pct);
        U64 seen = 0;
        for (S32 b = 0; b < NUM_BUCKETS; b++)
        {
            seen += mBuckets[b];
            if (seen > threshold)
            {
                return b < NUM_BUCKETS - 1 ? BUCKET_UPPER_MS[b] : mMax * 1000.f;
            }
        }
        return mMax * 1000.f;
    }

    std::string row(const char* label) const
    {
        F64 mean_ms = mCount ? mTotal * 1000.0 / mCount : 0.0;
        return llformat("    %-12s %9llu %10.1f %9.2f %8.1f %8.1f %8.0f",
                        label, mCount, mTotal, mean_ms,
                        percentileMS(0.5f), percentileMS(0.95f), mMax * 1000.f);
    }
};

// Must match LLTextureFetchWorker::e_state order (lltexturefetch.cpp) — that
// enum is private to the worker, and this is a debug reporter, so a name
// table is used instead of widening the worker's API.
const char* STATE_NAMES[] =
{
    "INVALID", "INIT", "LOAD_FROM_TEXTURE_CACHE", "CACHE_POST",
    "LOAD_FROM_NETWORK", "WAIT_HTTP_RESOURCE", "WAIT_HTTP_RESOURCE2",
    "SEND_HTTP_REQ", "WAIT_HTTP_REQ", "DECODE_IMAGE", "DECODE_IMAGE_UPDATE",
    "WRITE_TO_CACHE", "WAIT_ON_WRITE", "DONE"
};
constexpr S32 NUM_STATES = sizeof(STATE_NAMES) / sizeof(STATE_NAMES[0]);

LLMutex sMutex;
std::atomic<bool> sEnabled{ true }; // refreshed from settings on the main thread

// All below guarded by sMutex.
LLTimer sSessionTimer;
LLTimer sDumpTimer;
Histogram sCacheRead;
Histogram sDecode;
Histogram sCacheWrite;
Histogram sFetchTotal;
Histogram sUpload;
F64 sStateTotals[NUM_STATES] = { 0.0 };
U64 sRequests = 0;
U64 sCacheHits = 0;
F64 sBytesFetched = 0.0;
F64 sBytesDecoded = 0.0;
std::unordered_set<LLUUID> sDecodedIDs;
U64 sRepeatDecodes = 0;
F64 sRepeatDecodeTime = 0.0;
F64 sRepeatCacheReadTime = 0.0;

void dump()
{
    F32 elapsed = sSessionTimer.getElapsedTimeF32();
    std::ostringstream out;
    out << llformat("[BDMerge G5.1-S1] texture pipeline spike report — %.0fs elapsed, %llu completed requests, cache hit %.1f%%",
                    elapsed, sRequests, sRequests ? 100.0 * sCacheHits / sRequests : 0.0) << "\n";
    out << llformat("    %-12s %9s %10s %9s %8s %8s %8s", "stage", "count", "total_s", "mean_ms", "p50_ms", "p95_ms", "max_ms") << "\n";
    out << sCacheRead.row("cache_read") << "\n";
    out << sDecode.row("decode") << "\n";
    out << sCacheWrite.row("cache_write") << "\n";
    out << sUpload.row("gl_upload") << "\n";
    out << sFetchTotal.row("fetch_total") << "\n";
    out << "    time-in-state (worker state machine, all requests):\n";
    for (S32 i = 0; i < NUM_STATES; i++)
    {
        if (sStateTotals[i] > 0.05)
        {
            out << llformat("      %-22s %10.1fs", STATE_NAMES[i], sStateTotals[i]) << "\n";
        }
    }
    F64 decode_mbps = sDecode.mTotal > 0.0 ? (sBytesDecoded / (1024.0 * 1024.0)) / sDecode.mTotal : 0.0;
    out << llformat("    bytes fetched %.1f MB, decoded %.1f MB (encoded J2K in, %.1f MB/s through decode)",
                    sBytesFetched / (1024.0 * 1024.0), sBytesDecoded / (1024.0 * 1024.0), decode_mbps) << "\n";
    out << llformat("    repeat decodes (same texture decoded again this session — what a decoded RAM pool eliminates): %llu of %llu decodes (%.1f%%), %.1fs decode + %.1fs cache-read spent on repeats",
                    sRepeatDecodes, sDecode.mCount,
                    sDecode.mCount ? 100.0 * sRepeatDecodes / sDecode.mCount : 0.0,
                    sRepeatDecodeTime, sRepeatCacheReadTime);
    LL_INFOS("BDMergeTexSpike") << out.str() << LL_ENDL;
}

} // anonymous namespace

//static
void BDMergeTexSpike::recordFetch(const LLUUID& id, bool from_cache, S32 file_size,
                                  F32 cache_read_time, F32 decode_time, F32 cache_write_time,
                                  F32 fetch_time, const std::map<S32, F32>& state_timers)
{
    static LLCachedControl<bool> spike_log(gSavedSettings, "BDMergeTexSpikeLog", true);
    static LLCachedControl<F32> spike_interval(gSavedSettings, "BDMergeTexSpikeLogInterval", 300.f);
    sEnabled.store(spike_log, std::memory_order_relaxed);
    if (!spike_log)
    {
        return;
    }

    LLMutexLock lock(&sMutex);
    sRequests++;
    if (from_cache)
    {
        sCacheHits++;
    }
    if (file_size > 0)
    {
        sBytesFetched += file_size;
    }
    if (cache_read_time > 0.f) sCacheRead.record(cache_read_time);
    if (cache_write_time > 0.f) sCacheWrite.record(cache_write_time);
    if (fetch_time > 0.f) sFetchTotal.record(fetch_time);
    if (decode_time > 0.f)
    {
        sDecode.record(decode_time);
        if (file_size > 0)
        {
            sBytesDecoded += file_size;
        }
        if (!sDecodedIDs.insert(id).second)
        {
            sRepeatDecodes++;
            sRepeatDecodeTime += decode_time;
            sRepeatCacheReadTime += llmax(cache_read_time, 0.f);
        }
    }
    for (const auto& st : state_timers)
    {
        if (st.first >= 0 && st.first < NUM_STATES)
        {
            sStateTotals[st.first] += st.second;
        }
    }

    if (sDumpTimer.getElapsedTimeF32() > spike_interval)
    {
        sDumpTimer.reset();
        dump();
    }
}

//static
void BDMergeTexSpike::recordUpload(F32 seconds)
{
    if (!sEnabled.load(std::memory_order_relaxed))
    {
        return;
    }
    LLMutexLock lock(&sMutex);
    sUpload.record(seconds);
}
