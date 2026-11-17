#ifndef AL_KEYFRAME_PARTICLES_H
#define AL_KEYFRAME_PARTICLES_H
#include <array>
#include <cstdint>
#include <set>
// Fixed, bounded-probe provenance at the spatial draw-batch boundary. A very long
// lease can exhaust coverage, but can never cause an unbounded scan or false PASS.
class ALCleanParticleBatches
{
public:
    static constexpr unsigned Capacity=16384, MaxProbes=32;
    struct Batch { std::uintptr_t group=0; bool forbidden=false; };
    inline static std::array<Batch,Capacity> batches{};
    inline static bool covered=true;
    static void reset() { batches={}; covered=true; }
    static bool complete() { return covered; }
    static bool visible(bool effect,bool clean) { return !clean || !effect; }
    static void built(std::uintptr_t group,bool forbidden)
    {
        const auto slot=(group>>4)%Capacity;
        for(unsigned n=0;n<MaxProbes;++n)
        {
            auto& batch=batches[(slot+n)%Capacity];
            if(!batch.group || batch.group==group) { batch={group,forbidden}; return; }
        }
        covered=false;
    }
    static bool forbidden(std::uintptr_t group)
    {
        const auto slot=(group>>4)%Capacity;
        for(unsigned n=0;n<MaxProbes;++n)
        {
            const auto& batch=batches[(slot+n)%Capacity];
            if(!batch.group) return false;
            if(batch.group==group) return batch.forbidden;
        }
        covered=false; return false; // verdict becomes INCONCLUSIVE
    }
    template<class Range,class Effect,class Group,class Prepare,class Rebuild>
    static void transition(const Range& objects,bool clean,Effect effect,Group groupOf,Prepare prepare,Rebuild rebuild)
    {
        reset();
        std::set<std::uintptr_t> groups; // transition-only; no frame allocations
        for(const auto& object:objects)
        {
            if(!effect(object)) continue;
            const auto oldGroup=groupOf(object);
            // Prebuilt geometry is suspect UNTIL the actual draw-map rebuild completes.
            if(clean && oldGroup) built(oldGroup,true);
            prepare(object);
            const auto group=groupOf(object);
            if(group) groups.insert(group);
        }
        for(auto group:groups) rebuild(group);
    }
};
#endif
