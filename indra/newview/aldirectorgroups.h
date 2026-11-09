/** Pure value helpers for scene-persisted Director cast groups. */
#ifndef AL_DIRECTOR_GROUPS_H
#define AL_DIRECTOR_GROUPS_H

#include "llstring.h"
#include "lluuid.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <utility>

namespace ALDirectorGroups
{
inline std::string normalizeGroupName(std::string name)
{
    LLStringUtil::trim(name);
    name = utf8str_symbol_truncate(name, 32);
    LLStringUtil::trim(name);
    return utf8str_tolower(name) == "(none)" ? std::string() : name;
}

inline std::string existingGroupName(const std::vector<std::string>& names, const std::string& name)
{
    const std::string key = utf8str_tolower(name);
    for (const auto& existing : names)
        if (utf8str_tolower(existing) == key) return existing;
    return {};
}

// Existing destinations retain their exact stored spelling and legacy shape.
inline std::string resolveGroupName(const std::vector<std::string>& names, const std::string& requested)
{
    if (std::find(names.begin(), names.end(), requested) != names.end()) return requested;
    const std::string name = normalizeGroupName(requested);
    const std::string existing = existingGroupName(names, name);
    return existing.empty() ? name : existing;
}

inline std::string uniqueGroupName(const std::vector<std::string>& names, const std::string& requested = {})
{
    const std::string name = normalizeGroupName(requested);
    if (!name.empty() && existingGroupName(names, name).empty()) return name;
    for (size_t n = 1; ; ++n)
    {
        const std::string candidate = "Group " + std::to_string(n);
        if (existingGroupName(names, candidate).empty()) return candidate;
    }
}

struct GroupSnapshot
{
    std::map<LLUUID, std::string> tags;
    std::map<std::string, F32> delays;
    std::map<std::string, S32> rates;
    std::map<S32, std::pair<std::string, LLUUID>> hotGroups;

    bool operator==(const GroupSnapshot& other) const
    { return tags == other.tags && delays == other.delays && rates == other.rates && hotGroups == other.hotGroups; }
    bool operator!=(const GroupSnapshot& other) const { return !(*this == other); }

    void rewriteActor(const LLUUID& old_id, const LLUUID& new_id)
    {
        auto found = tags.find(old_id);
        if (found != tags.end())
        {
            const std::string tag = found->second;
            tags.erase(found);
            if (new_id.notNull()) tags[new_id] = tag;
        }
        for (auto& binding : hotGroups)
            if (binding.second.second == old_id) binding.second.second = new_id;
    }
};

// Compare and patch only keys changed by this edit, including erased keys.
template<class Map>
bool patchDelta(Map& current, const Map& expected, const Map& target, bool apply)
{
    Map keys = expected;
    keys.insert(target.begin(), target.end());
    for (const auto& key : keys)
    {
        const auto old = expected.find(key.first), next = target.find(key.first);
        if (old != expected.end() && next != target.end() && old->second == next->second) continue;
        const auto now = current.find(key.first);
        if ((now == current.end()) != (old == expected.end()) ||
            (now != current.end() && now->second != old->second)) return false;
        if (apply)
        {
            if (next == target.end()) current.erase(key.first);
            else current[key.first] = next->second;
        }
    }
    return true;
}

// Hot bindings are ancillary: a later rebind must not invalidate membership undo.
inline bool patchDelta(GroupSnapshot& current, const GroupSnapshot& expected,
                       const GroupSnapshot& target, bool apply, bool* skipped_hot = nullptr)
{
    if (!patchDelta(current.tags, expected.tags, target.tags, apply) ||
        !patchDelta(current.delays, expected.delays, target.delays, apply) ||
        !patchDelta(current.rates, expected.rates, target.rates, apply)) return false;
    if (apply)
        for (const auto& binding : expected.hotGroups)
        {
            const auto next = target.hotGroups.find(binding.first);
            if (next == target.hotGroups.end() || next->second == binding.second) continue;
            const auto now = current.hotGroups.find(binding.first);
            if (now != current.hotGroups.end() && now->second == binding.second)
                now->second = next->second;
            else if (skipped_hot) *skipped_hot = true;
        }
    return true;
}
} // namespace ALDirectorGroups
#endif
