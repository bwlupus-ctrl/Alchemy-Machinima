/** @file aldirectoractorstate.h
 * Field-level authorship for reconciling two identities of one Director actor.
 */
#ifndef AL_DIRECTOR_ACTOR_STATE_H
#define AL_DIRECTOR_ACTOR_STATE_H

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>

template<class Value>
struct ALDirectorActorState
{
    using Values = std::map<std::string, Value>;
    Values mValues;
    std::map<std::string, std::uint64_t> mAuthored;

    void observe(const Values& values, const Values& defaults, std::uint64_t order)
    {
        // Include disappeared optional fields: clearing a target/override is
        // an authored edit too. Value{} is the undefined/default sentinel.
        Values keys = mValues;
        keys.insert(values.begin(), values.end());
        for (const auto& key : keys)
        {
            auto value = values.find(key.first);
            const Value now = value == values.end() ? Value{} : value->second;
            auto previous = mValues.find(key.first);
            auto def = defaults.find(key.first);
            const Value before = previous != mValues.end() ? previous->second
                : def != defaults.end() ? def->second : Value{};
            if (now != before) mAuthored[key.first] = order;
            mValues[key.first] = now;
        }
    }

    template<class Log>
    void merge(const ALDirectorActorState& other, Log log)
    {
        for (const auto& field : other.mValues)
        {
            const auto incoming = other.mAuthored.find(field.first);
            const auto current = mAuthored.find(field.first);
            const auto in_order = incoming == other.mAuthored.end() ? 0 : incoming->second;
            const auto old_order = current == mAuthored.end() ? 0 : current->second;
            auto value = mValues.find(field.first);
            if (in_order && old_order && value != mValues.end() && value->second != field.second)
                log(field.first, old_order, in_order);
            if (value == mValues.end() || in_order > old_order)
            {
                mValues[field.first] = field.second;
                if (in_order) mAuthored[field.first] = in_order;
            }
        }
    }
};
#endif
