/**
 * @file bdmergemeshpool.h
 * @brief [BDMerge G5.1-2b] Decoded-mesh RAM pool: unpacked LLVolume faces
 *        kept in system RAM so a mesh LOD re-entering draw distance skips
 *        the disk read AND the zlib-inflate + LLSD parse + face build.
 *
 * Same design contract as the decoded-texture pool (bdmergetexpool.h):
 * entries are exclusively-owned deep copies — no LLVolume ref ever crosses
 * the pool boundary (consumers mutate volumes: cacheOptimize, rigging), so
 * hits serve fresh copies. Budget auto-sizes from detected physical RAM,
 * LRU-evicted. The encoded disk mesh cache persists as before; the pool is
 * a volatile accelerator, never the source of truth. All copies run on mesh
 * repo / worker threads, never the render thread.
 *
 * Gated by BDMergeMeshPoolEnable; off = stock behavior.
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

#ifndef BDMERGE_MESHPOOL_H
#define BDMERGE_MESHPOOL_H

#include "lluuid.h"

#include <iosfwd>

class LLVolume;
class LLVolumeParams;

class BDMergeMeshPool
{
public:
    // Main thread only (LLMeshRepository::notifyLoadedMeshes).
    static void refreshSettings();

    // Any thread.
    static bool enabled();

    // Worker threads: on hit, deep-copies the pooled faces into dest (a
    // freshly constructed LLVolume for the same params/detail) and returns
    // true. dest is untouched on miss.
    static bool fetch(const LLUUID& mesh_id, S32 lod, LLVolume* dest);

    // Worker threads: store a deep copy of src's unpacked faces under
    // (mesh_id, lod). May evict LRU entries to stay under budget.
    static void put(const LLUUID& mesh_id, S32 lod, const LLVolumeParams& params, const LLVolume* src);

    // Append a one-line stats summary (for the G5.1-S1 spike report).
    static void appendReport(std::ostream& out);
};

#endif // BDMERGE_MESHPOOL_H
