/**
 * @file algazenearby.h
 * @brief Pure selection rules for the "Nearby" section of the Director gaze target pickers.
 *
 * An actor's gaze target is stored exactly like a cast target (GAZE_CAST plus an agent id);
 * resolving that id never needs cast membership. These rules only decide WHICH real avatars
 * the pickers offer after the cast members. They hold no viewer state, so they run natively.
 *
 * TPV policy: the entries carry a display name and never a UUID. A person with no name yet is
 * not offered, and a name that is itself a UUID is dropped.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_GAZE_NEARBY_H
#define AL_GAZE_NEARBY_H

#include "lluuid.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace ALGazeNearby
{
constexpr std::size_t MAX_ENTRIES = 16;

// The disabled "Nearby" header row. Its value is neither empty (the "no cast member" placeholder)
// nor a UUID, so a commit that reaches it by any selection path is recognised and ignored.
constexpr const char* HEADER_LABEL = "Nearby";
constexpr const char* HEADER_VALUE = "nearby-header";

struct Entry
{
    LLUUID mId;
    std::string mName;      // display name only (never a user or legacy name, never a UUID)
    F64 mPos[3] = { 0.0, 0.0, 0.0 };   // global position
};

inline F64 distanceSquared(const F64* a, const F64* b)
{
    const F64 dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

// Same people with the same names, in the same order (collectors sort by id first).
// Positions are deliberately ignored: a moving crowd must not rebuild a combo.
inline bool sameMembers(const std::vector<Entry>& a, const std::vector<Entry>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].mId != b[i].mId || a[i].mName != b[i].mName) return false;
    return true;
}

// Keep real, named, in-range people who are not the actor and not already offered as cast
// members; nearest to order_ref (the actor) first; at most `cap`. `keep` is the saved target: a
// present, named keep person stays listed past the range and the cap, so a target who is here is
// never presented as away.
inline void select(std::vector<Entry>& people, const uuid_vec_t& cast_ids, const LLUUID& actor,
                   const F64* order_ref, const F64* range_ref, F64 range,
                   const LLUUID& keep = LLUUID(), std::size_t cap = MAX_ENTRIES)
{
    const F64 range_sq = range * range;
    people.erase(std::remove_if(people.begin(), people.end(), [&](const Entry& e)
    {
        return e.mId.isNull() || e.mId == actor || e.mName.empty() ||
            LLUUID::validate(e.mName) ||
            std::find(cast_ids.begin(), cast_ids.end(), e.mId) != cast_ids.end() ||
            (e.mId != keep && distanceSquared(e.mPos, range_ref) > range_sq);
    }), people.end());
    std::stable_sort(people.begin(), people.end(), [&](const Entry& a, const Entry& b)
    {
        const F64 da = distanceSquared(a.mPos, order_ref), db = distanceSquared(b.mPos, order_ref);
        if (da != db) return da < db;
        if (a.mName != b.mName) return a.mName < b.mName;
        return a.mId < b.mId;
    });
    if (people.size() > cap)
    {
        const auto at = std::find_if(people.begin(), people.end(), [&](const Entry& e) { return e.mId == keep; });
        if (!keep.isNull() && at != people.end() && static_cast<std::size_t>(at - people.begin()) >= cap)
        {
            const Entry saved = *at;
            people.erase(at);
            people.resize(cap - 1);
            people.push_back(saved);
        }
        else
        {
            people.resize(cap);
        }
    }
}

// Label for the saved target when that person is no longer nearby, so the combo keeps showing
// the selection instead of silently changing it. Never a UUID.
inline std::string awayLabel(const std::string& name)
{
    return (name.empty() || LLUUID::validate(name) ? std::string("Unknown person") : name) + " (away)";
}
}

#endif // AL_GAZE_NEARBY_H
