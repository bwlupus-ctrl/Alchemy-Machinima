/** @file llpluginloopseam.h
 * Bounded viewer-side loop seam diagnostics. No media paths or tokens logged.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

struct LLPluginLoopSeam
{
    enum class Kind { NONE, INDIVIDUAL, SUMMARY };
    struct Report
    {
        Kind kind = Kind::NONE;
        uint64_t count = 0;
        double min = 0., max = 0., mean = 0., excess = 0.;
    };
    unsigned individual = 0;
    uint64_t count = 0;
    double nextSummary = 0., min = 0., max = 0., sum = 0., excess = 0.;

    Report sample(double now, double gap, double frame)
    {
        if (!std::isfinite(gap) || gap < 0. || !std::isfinite(frame) || frame < 0.) return {};
        if (individual < 3)
        {
            ++individual;
            nextSummary = now + 10.;
            return {Kind::INDIVIDUAL, 1, gap, gap, gap, std::max(0., gap - frame)};
        }
        min = count ? std::min(min, gap) : gap;
        max = count ? std::max(max, gap) : gap;
        ++count;
        sum += gap;
        excess += std::max(0., gap - frame);
        return poll(now);
    }
    Report poll(double now)
    {
        if (!count || now < nextSummary) return {};
        const Report report{Kind::SUMMARY, count, min, max, sum / count, excess / count};
        count = 0; sum = 0.; excess = 0.;
        nextSummary = now + 10.;
        return report;
    }
};
