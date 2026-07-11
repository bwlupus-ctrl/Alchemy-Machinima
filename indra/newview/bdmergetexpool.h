/**
 * @file bdmergetexpool.h
 * @brief [BDMerge G5.1] Decoded-texture RAM pool: system RAM feeds VRAM.
 *
 * Holds copies of decoded texture pixel data in system RAM so that a texture
 * re-entering view (after VRAM eviction / discard churn) is served by a
 * memcpy instead of a disk read + J2K decode. Read path: RAM -> encoded disk
 * cache -> decode -> network, first hit wins. Write path is unchanged - the
 * encoded disk cache still persists everything (write-through); the pool is
 * a volatile accelerator, never the source of truth.
 *
 * Budget auto-sizes from detected physical RAM (no hardcoded GB figures),
 * evicted LRU. Entries store their own pixel copy: no LLImageRaw ref is ever
 * shared across the pool boundary, so consumer-side mutation and non-atomic
 * refcounting cannot corrupt pool state. All heavy work (copies, eviction)
 * runs on texture-fetch worker threads, never the render thread.
 *
 * Gated by BDMergeTexPoolEnable; off = stock behavior.
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

#ifndef BDMERGE_TEXPOOL_H
#define BDMERGE_TEXPOOL_H

#include "llpointer.h"
#include "lluuid.h"

#include <iosfwd>
#include <string>

class LLImageRaw;

class BDMergeTexPool
{
public:
    // Main thread only: re-read settings (LLCachedControl is not thread-safe)
    // and recompute the budget. Called from LLTextureFetch::createRequest.
    static void refreshSettings();

    // Any thread.
    static bool enabled();

    // Worker threads: return a fresh copy of the pooled image for id if one
    // exists at desired_discard or finer; entry_discard receives its discard
    // level. Null on miss.
    static LLPointer<LLImageRaw> fetch(const LLUUID& id, S32 desired_discard, S32& entry_discard);

    // Worker threads: copy raw into the pool under id at discard, replacing
    // any same-or-coarser entry (finer entries are kept instead). May evict
    // LRU entries to stay under budget.
    static void put(const LLUUID& id, S32 discard, const LLImageRaw* raw);

    // Compact one-line status for UI readouts (thread-safe).
    static std::string shortStatus();

    // Append a one-line stats summary (for the G5.1-S1 spike report).
    static void appendReport(std::ostream& out);
};

#endif // BDMERGE_TEXPOOL_H
