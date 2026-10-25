/**
 * @file alrimsnapshot.h
 * Copyright (C) 2026, Alchemy Machinima
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef AL_RIM_SNAPSHOT_H
#define AL_RIM_SNAPSHOT_H

#include <cmath>
#include <limits>

namespace ALRimSnapshot
{
struct Tile
{
    int width = 0, height = 0, margin = 0;
    int internal_width = 0, internal_height = 0;
};

// Match allocateScreenBufferInternal's divisor/multiplier, including exact
// pixel phase. Unsupported fractional scales safely use the ordinary path.
inline Tile plan(int width, int height, unsigned divisor, float multiplier, int support)
{
    if (width <= 0 || height <= 0) return {};
    const double scale = divisor > 1 && divisor < unsigned(width) && divisor < unsigned(height)
        ? 1.0 / divisor : (multiplier > 0.f ? multiplier : 1.f);
    if (!std::isfinite(scale) || scale <= 0.0) return {};
    if (width * scale + 2.0 * support > std::numeric_limits<int>::max() ||
        height * scale + 2.0 * support > std::numeric_limits<int>::max()) return {};
    int alignment = 4;
    for (; alignment <= 4096; alignment += 4)
    {
        const double pixels = alignment * scale;
        if (pixels >= 4 && std::abs(pixels - std::round(pixels / 4) * 4) < 1.e-6) break;
    }
    if (alignment > 4096) return {};
    Tile tile;
    tile.width = width / alignment * alignment;
    tile.height = height / alignment * alignment;
    tile.margin = int(std::ceil(support / (scale * alignment))) * alignment;
    tile.internal_width = int(std::round(tile.width * scale));
    tile.internal_height = int(std::round(tile.height * scale));
    return tile;
}
}
#endif
