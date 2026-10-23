/** Read-only Relight take diagnostics. Never consumed by rendering. */
#ifndef AL_RELIGHT_TAKE_RECORD_H
#define AL_RELIGHT_TAKE_RECORD_H
#include "alareashadowmath.h"
#include "llsd.h"
#include "lluuid.h"
#include <string>

namespace ALRelight
{
struct AreaTakeState
{
    const char* status = "not_observed";
    unsigned width = 0, height = 0;
    unsigned frame = 0;    // gFrameCount of the sampled pass: a reader can tell a stale record.
    bool planned = false;
    int authoredSteps = 0; // Saved uniform/far count read by the sampled area pass itself.
    bool feed = false;     // The sampled pass was a VCam feed (own plan), not the main view.
    bool segmentedRequested = false; // Thin objects asked for a segmented plan.
    ALAreaShadow::Selection<LLUUID> selected;
    ALAreaShadow::Workload work{};
};

inline LLSD areaTakeRecord(const AreaTakeState& state)
{
    LLSD out;
    out["source"] = "most_recent_allowed_area_pass";
    out["status"] = state.status;
    out["width"] = static_cast<int>(state.width);
    out["height"] = static_cast<int>(state.height);
    out["frame"] = static_cast<int>(state.frame);
    out["candidates"] = static_cast<int>(state.selected.candidates);
    out["selected"] = LLSD::emptyArray();
    for (unsigned channel = 0; channel < state.selected.size; ++channel)
    {
        LLSD light;
        light["channel"] = static_cast<int>(channel);
        light["uuid"] = state.selected.lights[channel].id;
        light["shadow_map"] = state.selected.lights[channel].shadowMap;
        light["score"] = state.selected.lights[channel].score;
        out["selected"].append(light);
    }
    out["view"] = state.feed ? "vcam_feed" : "main";
    out["work_planned"] = state.planned;
    if (state.planned)
    {
        const auto& work = state.work;
        LLSD& plan = out["work"];
        plan["samples"] = work.q.samples;
        plan["authored_steps"] = state.authoredSteps;
        plan["near_steps"] = work.q.nearSteps;
        plan["far_steps"] = std::max(0,work.q.steps-work.q.nearSteps);
        // Segmented was requested but the effective plan is the uniform march.
        plan["uniform_fallback"] = state.segmentedRequested && !work.q.nearSteps;
        plan["steps"] = work.q.steps; // Effective total, retained for record readers.
        plan["half"] = work.q.half;
        plan["channels_per_draw"] = static_cast<int>(work.channels);
        plan["admitted"] = static_cast<int>(work.admitted); // lights that keep a soft-shadow channel
        // Thin objects hybrid: the first `lead` lights march the plan above, the rest march `rest_*` (the regular march).
        plan["lead"] = static_cast<int>(work.lead);
        if (work.lead < work.admitted)
        {
            plan["rest_samples"] = work.rest.samples;
            plan["rest_steps"] = work.rest.steps;
            plan["rest_channels_per_draw"] = static_cast<int>(work.restChannels);
        }
        // LLSD integers are signed 32-bit. Preserve exact 64-bit counts as text.
        plan["pixels"] = std::to_string(work.pixels);
        plan["iterations"] = std::to_string(work.iterations);
        plan["budget"] = std::to_string(work.budget);
        plan["draw_cap"] = std::to_string(work.drawCap);
    }
    return out;
}

// The main view's record is "area_view". The most recent VCam feed pass, if any, is a
// separate labelled "feed_view" and never touches the main view's fields.
template<std::size_t N> inline LLSD takeRecord(const char* trigger, const LLSD& settings,
    const AreaTakeState& area, const std::array<LLUUID, N>& areaSlots,
    const std::array<LLUUID, N>& currentSlots, const std::array<float, N>& fade,
    const AreaTakeState* feed = nullptr)
{
    LLSD out;
    out["version"] = 1;
    out["trigger"] = trigger;
    out["settings_at_start"] = settings;
    out["area_view"] = areaTakeRecord(area);
    out["area_view"]["projector_slots"] = LLSD::emptyArray();
    if (feed && std::string(feed->status) != "not_observed")
    {
        out["feed_view"] = areaTakeRecord(*feed);
        out["feed_view"]["source"] = "most_recent_vcam_feed_area_pass";
        out["feed_view"]["view"] = "vcam_feed";
    }
    out["projector_slots_at_start"] = LLSD::emptyArray();
    for (std::size_t i = 0; i < N; ++i)
    {
        LLSD slot;
        slot["slot"] = static_cast<int>(i);
        slot["uuid"] = areaSlots[i];
        out["area_view"]["projector_slots"].append(slot);
        slot["uuid"] = currentSlots[i];
        slot["fade"] = fade[i];
        out["projector_slots_at_start"].append(slot);
    }
    return out;
}
}
#endif
