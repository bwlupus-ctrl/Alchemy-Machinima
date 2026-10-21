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
    bool planned = false;
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
    out["work_planned"] = state.planned;
    if (state.planned)
    {
        const auto& work = state.work;
        LLSD& plan = out["work"];
        plan["samples"] = work.q.samples;
        plan["steps"] = work.q.steps;
        plan["half"] = work.q.half;
        plan["channels_per_draw"] = static_cast<int>(work.channels);
        // LLSD integers are signed 32-bit. Preserve exact 64-bit counts as text.
        plan["pixels"] = std::to_string(work.pixels);
        plan["iterations"] = std::to_string(work.iterations);
        plan["budget"] = std::to_string(work.budget);
        plan["draw_cap"] = std::to_string(work.drawCap);
    }
    return out;
}

template<std::size_t N> inline LLSD takeRecord(const char* trigger, const LLSD& settings,
    const AreaTakeState& area, const std::array<LLUUID, N>& areaSlots,
    const std::array<LLUUID, N>& currentSlots, const std::array<float, N>& fade)
{
    LLSD out;
    out["version"] = 1;
    out["trigger"] = trigger;
    out["settings_at_start"] = settings;
    out["area_view"] = areaTakeRecord(area);
    out["area_view"]["projector_slots"] = LLSD::emptyArray();
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
