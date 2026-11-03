/** [ActorKey] Pure WP0 membership model, shared by production and TUT. */
#pragma once
#include <set>
#include <vector>

template<class Id> struct ALActorKeySet
{
    struct Avatar
    {
        Id id, owner, wearer;
        bool clone = false;
    };
    struct Instance { Id id, source, entity; };
    std::set<Id> selected, avatars, owners, instances;
    bool all = false, includeClones = true;

    // The sole source-membership resolver. Proxy coverage NEVER consults owners.
    bool instanceKeyed(const Instance& inst) const
    {
        return includeClones && (all || selected.count(inst.id) ||
            (inst.entity != Id{} && selected.count(inst.entity)) ||
            (inst.source != Id{} && selected.count(inst.source)));
    }
    void rebuild(const std::vector<Avatar>& live, const std::vector<Instance>& clones)
    {
        avatars.clear(); owners.clear(); instances.clear();
        for (const auto& inst : clones)
            if (instanceKeyed(inst)) instances.insert(inst.id);
        for (const auto& av : live)
        {
            bool keyed = selected.count(av.id) != 0;
            if (av.clone)
            {
                keyed = false;
                for (const auto& inst : clones)
                    if (inst.entity == av.id && instances.count(inst.id)) { keyed = true; break; }
            }
            if (!keyed) continue;
            avatars.insert(av.id);
            owners.insert(av.owner);
            if (av.wearer != Id{}) owners.insert(av.wearer);
        }
    }
    bool nativeKeyed(const Id& owner, const Id& fallback) const
    {
        return (owner != Id{} && owners.count(owner)) ||
               (fallback != Id{} && owners.count(fallback));
    }
};
