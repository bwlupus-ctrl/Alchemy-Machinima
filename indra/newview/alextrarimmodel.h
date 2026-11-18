/**
 * @file alextrarimmodel.h
 * @brief Pure direction, placement and scene model for session-only extra rims.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef AL_EXTRA_RIM_MODEL_H
#define AL_EXTRA_RIM_MODEL_H

#include "alcinelightrigmodel.h"
#include "llsd.h"
#include "lluuid.h"
#include "v3dmath.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ALExtraRimModel
{
constexpr S32 kMaxExtraRims = 4;
constexpr S32 kSourceCount = 5;

enum class SourceDark { NONE, RIG_OFF, RIM_OFF };
struct Source
{
    S32 mSlot = 0;
    SourceDark mDark = SourceDark::NONE;
    bool lit() const { return mDark == SourceDark::NONE; }
};

inline S32 sourceSlot(S32 picked)
{
    return picked >= 0 && picked < kSourceCount ? picked : 0;
}

inline Source resolveSource(S32 picked, U32 lit_mask, U32 rim_on_mask)
{
    const S32 slot = sourceSlot(picked);
    const U32 bit = 1u << slot;
    // Manual source only: a dark pick never borrows another rig's direction.
    if (!(lit_mask & bit)) return {slot, SourceDark::RIG_OFF};
    if (!(rim_on_mask & bit)) return {slot, SourceDark::RIM_OFF};
    return {slot, SourceDark::NONE};
}

inline F32 effectiveRadius(F32 radius, F32 scale)
{
    if (!std::isfinite(radius) || radius <= 0.f) radius = 1.f;
    if (!std::isfinite(scale) || scale <= 0.f) scale = 1.f;
    // Match tickShared/emitterBoxEdge, including nominal radii outside the
    // ordinary scale clamp: scale 1 must preserve the authored radius.
    return std::clamp(radius * scale,
        std::min(radius, ALCineLightRigModel::SCALED_RADIUS_FLOOR),
        std::max(radius, ALCineLightRigModel::SCALED_RADIUS_CEIL));
}

struct Placement
{
    LLVector3d mPosition;
    F32 mScale = 1.f;
};
inline Placement placement(const LLVector3d& src_pos, const LLVector3d& src_centre,
                           F32 src_radius, F32 src_scale,
                           const LLVector3d& dst_centre, F32 dst_scale)
{
    const F32 k = effectiveRadius(src_radius, dst_scale) /
                  effectiveRadius(src_radius, src_scale);
    return {dst_centre + (src_pos - src_centre) * static_cast<F64>(k), k};
}

struct Item
{
    LLUUID mActor;
    F32 mTrim = 1.f;
};
struct State
{
    S32 mSource = 0;
    std::vector<Item> mItems;
};
inline bool validTrim(F64 trim)
{
    return std::isfinite(trim) && trim >= 0. && trim <= 2.;
}
inline bool add(State& state, const LLUUID& actor, F32 trim = 1.f)
{
    if (actor.isNull() || !validTrim(trim) || state.mItems.size() >= kMaxExtraRims)
        return false;
    for (const auto& item : state.mItems)
        if (item.mActor == actor) return false;
    state.mItems.push_back({actor, trim});
    return true;
}
inline bool remove(State& state, const LLUUID& actor)
{
    const auto found = std::find_if(state.mItems.begin(), state.mItems.end(),
        [&actor](const Item& item) { return item.mActor == actor; });
    if (found == state.mItems.end()) return false;
    state.mItems.erase(found);
    return true;
}

inline LLSD toLLSD(const State& state)
{
    LLSD block = LLSD::emptyMap();
    block["version"] = 1;
    block["source"] = sourceSlot(state.mSource);
    block["items"] = LLSD::emptyArray();
    State clean;
    for (const auto& item : state.mItems)
    {
        if (!add(clean, item.mActor, item.mTrim)) continue;
        LLSD row = LLSD::emptyMap();
        row["actor"] = item.mActor;
        row["trim"] = item.mTrim;
        block["items"].append(row);
    }
    return block;
}

// A rejected version leaves the caller's live state unchanged. An absent
// block is the legacy-scene case and clears the session list and source.
inline bool fromLLSD(const LLSD& block, State& state)
{
    if (!block.isUndefined() && (!block.isMap() ||
        !block.has("version") || !block["version"].isInteger() ||
        block["version"].asInteger() != 1))
        return false;
    State parsed;
    if (block["source"].isInteger())
        parsed.mSource = sourceSlot(block["source"].asInteger());
    if (block["items"].isArray())
    {
        for (LLSD::array_const_iterator it = block["items"].beginArray();
             it != block["items"].endArray(); ++it)
        {
            const LLSD& row = *it;
            if (!row.isMap() || (!row["actor"].isUUID() && !row["actor"].isString())) continue;
            F64 trim = 1.;
            if (row.has("trim"))
            {
                if (!row["trim"].isReal() && !row["trim"].isInteger()) continue;
                trim = row["trim"].asReal();
            }
            // Validate before F32 conversion so near-boundary doubles and
            // NaN/inf cannot become a valid strength through narrowing.
            if (!validTrim(trim)) continue;
            add(parsed, row["actor"].asUUID(), static_cast<F32>(trim));
        }
    }
    state = parsed;
    return true;
}
} // namespace ALExtraRimModel
#endif // AL_EXTRA_RIM_MODEL_H
