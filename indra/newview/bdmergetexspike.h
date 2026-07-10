/**
 * @file bdmergetexspike.h
 * @brief [BDMerge G5.1-S1] Texture-pipeline latency attribution for the
 *        decoded-texture RAM pool measurement spike.
 *
 * Session-level aggregation of per-request stage timings the fetch pipeline
 * already measures (cache read / J2K decode / cache write / HTTP states /
 * total), plus GL upload time and repeat-decode tracking. Periodically dumps
 * a summary block to the viewer log. Log-only: no behavior change to
 * fetching, decoding, or rendering. Gated by BDMergeTexSpikeLog.
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

#ifndef BDMERGE_TEXSPIKE_H
#define BDMERGE_TEXSPIKE_H

#include "lluuid.h"

#include <map>

class BDMergeTexSpike
{
public:
    // Called on the main thread from LLTextureFetch::getRequestFinished for
    // each completed request. state_timers is keyed by
    // LLTextureFetchWorker::e_state.
    static void recordFetch(const LLUUID& id,
                            bool from_cache,
                            S32 file_size,
                            F32 cache_read_time,
                            F32 decode_time,
                            F32 cache_write_time,
                            F32 fetch_time,
                            const std::map<S32, F32>& state_timers);

    // Called from LLViewerFetchedTexture::createTexture — may run on the
    // LLImageGL worker thread.
    static void recordUpload(F32 seconds);
};

#endif // BDMERGE_TEXSPIKE_H
