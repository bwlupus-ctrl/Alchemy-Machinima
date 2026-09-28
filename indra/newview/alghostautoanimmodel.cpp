// [AutoAnimate] Pure recorder, replacement graph, scheduler and validated persistence.
#include "linden_common.h"
#include "alghostautoanimmodel.h"
#include "llsdutil.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <numeric>
#include <set>
#include <sstream>

namespace ALGhostAutoAnim
{
namespace
{
U64 mix(U64 value)
{
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
U64 hash(const std::string& value)
{
    U64 result = 1469598103934665603ULL;
    for (unsigned char c : value) { result = (result ^ c) * 1099511628211ULL; }
    return result;
}
bool prefix(const std::string& value, const char* start) { return value.find(start) == 0; }
void increment(U16& value) { if (value < 65535) ++value; }
}

U8 classifyLayer(const std::vector<std::string>& joints)
{
    std::array<U32, LAYER_COUNT> counts{};
    for (const std::string& joint : joints)
    {
        if (joint == "mPelvis" || joint == "mTorso" || joint == "mChest" ||
            prefix(joint, "mSpine") || prefix(joint, "mHip") || prefix(joint, "mKnee") ||
            prefix(joint, "mAnkle") || prefix(joint, "mFoot") || prefix(joint, "mToe")) ++counts[LAYER_BODY];
        else if (prefix(joint, "mFace") || prefix(joint, "mEye")) ++counts[LAYER_FACE];
        else if (prefix(joint, "mHand")) ++counts[LAYER_HANDS];
        else if (joint == "mNeck" || joint == "mHead" || joint == "mSkull" ||
                 prefix(joint, "mCollar") || prefix(joint, "mShoulder") ||
                 prefix(joint, "mElbow") || prefix(joint, "mWrist")) ++counts[LAYER_UPPER];
        else ++counts[LAYER_EXTRA];
    }
    if (counts[LAYER_BODY]) return LAYER_BODY;
    U8 result = LAYER_FACE;
    for (U8 layer : { U8(LAYER_HANDS), U8(LAYER_UPPER), U8(LAYER_EXTRA) })
        if (counts[layer] > counts[result]) result = layer;
    return joints.empty() ? U8(LAYER_EXTRA) : result;
}
void Reservoir::add(F32 value)
{
    if (!std::isfinite(value) || value < 0.f) return;
    value = std::min(value, 3600.f);
    if (seen < std::numeric_limits<U32>::max()) ++seen;
    if (count < values.size()) values[count++] = value;
    else
    {
        const U64 index = mix(seen) % seen;
        if (index < values.size()) values[static_cast<size_t>(index)] = value;
    }
}
F32 Reservoir::median() const
{
    if (!count) return 0.f;
    auto sorted = values;
    std::sort(sorted.begin(), sorted.begin() + count);
    return count % 2 ? sorted[count / 2] : (sorted[count / 2 - 1] + sorted[count / 2]) * 0.5f;
}
S32 ChannelPool::find(const LLUUID& id) const
{
    for (size_t i = 0; i < entries.size(); ++i) if (entries[i].id == id) return static_cast<S32>(i);
    return -1;
}
void ChannelPool::finalize()
{
    groups.clear();
    std::vector<size_t> component(entries.size());
    std::iota(component.begin(), component.end(), size_t(0));
    for (Entry& entry : entries) entry.state = entry.loop_asset || entry.dwell.median() > entry.dur + 1.f;
    auto ordered = edges;
    std::sort(ordered.begin(), ordered.end(), [](const Edge& a, const Edge& b)
    {
        if (a.replace != b.replace) return a.replace > b.replace;
        return std::make_pair(a.a, a.b) < std::make_pair(b.a, b.b);
    });
    for (const Edge& edge : ordered)
    {
        if (!edge.replace || !entries[edge.a].state || !entries[edge.b].state ||
            entries[edge.a].layer != entries[edge.b].layer) continue;
        const size_t ca = component[edge.a], cb = component[edge.b];
        if (ca == cb) continue;
        bool blocked = false;
        for (const Edge& evidence : edges)
        {
            if (evidence.coexist &&
                ((component[evidence.a] == ca && component[evidence.b] == cb) ||
                 (component[evidence.a] == cb && component[evidence.b] == ca))) { blocked = true; break; }
        }
        if (!blocked) for (size_t& c : component) if (c == cb) c = ca;
    }
    std::map<size_t, std::vector<U8>> merged;
    std::array<U32, LAYER_COUNT> states{}, replacements{};
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].state) { merged[component[i]].push_back(static_cast<U8>(i)); ++states[entries[i].layer]; }
    for (const Edge& edge : edges)
        if (edge.replace && entries[edge.a].state && entries[edge.b].state &&
            entries[edge.a].layer == entries[edge.b].layer) ++replacements[entries[edge.a].layer];
    for (auto& pair : merged) groups.push_back(std::move(pair.second));
    thin = switches < 2;
    for (size_t i = 0; i < states.size(); ++i) thin = thin || (states[i] > 1 && !replacements[i]);
}
bool LinksetKey::matches(const LinksetKey& other) const
{
    return point == other.point && prims == other.prims && ordinal == other.ordinal &&
           (item == other.item || item.isNull() || other.item.isNull());
}
std::string LinksetKey::text() const
{
    return "ap:" + std::to_string(point) + "/" + item.asString().substr(0, 8) + "/" +
           std::to_string(prims) + "#" + std::to_string(ordinal);
}
const ChannelPool* Pool::findAnimesh(const LinksetKey& key, bool* weakMatch) const
{
    if (weakMatch) *weakMatch = false;
    for (const auto& channel : mAnimesh)
        if (key.matches(channel.first))
        {
            if (weakMatch) *weakMatch = key.item.isNull() || channel.first.item.isNull();
            return &channel.second;
        }
    return nullptr;
}
ChannelRecorder::ChannelRecorder(U32 cap) : mCap(std::max(8U, std::min(64U, cap))) { reset(); }
void ChannelRecorder::reset()
{
    mPool = ChannelPool(); mBuffer.clear(); mBuffer.reserve(32); mOpen.clear(); mLastArrival.clear();
    mStopped.fill(-1); mStopTime.fill(0.0); mGated = false; mOverflow = false;
}
void ChannelRecorder::seedFrom(const ChannelPool& pool) { reset(); mPool = pool; }
void ChannelRecorder::enqueue(const Event& event)
{
    if (mGated) return;
    if (mBuffer.size() == 32) { fold(mBuffer.front()); mBuffer.erase(mBuffer.begin()); mOverflow = true; }
    mBuffer.push_back(event);
}
void ChannelRecorder::onStart(const LLUUID& id, const AnimFacts& facts, F64 now) { enqueue({id, facts, now, 0}); }
// [AutoAnimate] Preserve presence without inventing an arrival or complete dwell.
void ChannelRecorder::onBaseline(const LLUUID& id, const AnimFacts& facts, F64 now) { enqueue({id, facts, now, 3}); }
void ChannelRecorder::onStop(const LLUUID& id, F64 now) { enqueue({id, AnimFacts(), now, 1}); }
void ChannelRecorder::onRetrigger(const LLUUID& id, const AnimFacts& facts, F64 now)
{
    const S32 index = mPool.find(id);
    if (!facts.loop_asset && (index < 0 || !mPool.entries[index].state)) enqueue({id, facts, now, 2});
}
void ChannelRecorder::onPending(const LLUUID& id, F64 now, bool retrigger, bool baseline)
{
    AnimFacts unknown; unknown.layer = LAYER_COUNT;
    enqueue({id, unknown, now, baseline ? U8(3) : retrigger ? U8(2) : U8(0)});
}
void ChannelRecorder::resolveFacts(const LLUUID& id, const AnimFacts& facts)
{
    // Preserve original event order, start times and co-occurrence evidence.
    // Events discarded by a gate/overflow are never resurrected here.
    for (Event& event : mBuffer)
        if (event.id == id && event.kind != 1 && event.facts.layer == LAYER_COUNT) event.facts = facts;
}
bool ChannelRecorder::hasPendingFacts(const LLUUID& id) const
{
    return std::any_of(mBuffer.begin(), mBuffer.end(), [&id](const Event& event)
        { return event.id == id && event.kind != 1 && event.facts.layer == LAYER_COUNT; });
}
void ChannelRecorder::edge(U8 a, U8 b, bool coexist)
{
    if (a == b || mPool.entries[a].layer != mPool.entries[b].layer) return;
    if (a > b) std::swap(a, b);
    auto found = std::find_if(mPool.edges.begin(), mPool.edges.end(), [a,b](const Edge& e) { return e.a == a && e.b == b; });
    if (found == mPool.edges.end())
    {
        if (mPool.edges.size() == 256) return;
        mPool.edges.push_back({a, b, 0, 0}); found = mPool.edges.end() - 1;
    }
    if (coexist) increment(found->coexist);
    else { increment(found->replace); if (mPool.switches < 2147483647U) ++mPool.switches; }
}
void ChannelRecorder::fold(const Event& event)
{
    if (event.kind != 1 && event.facts.layer >= LAYER_COUNT) return;
    S32 index = mPool.find(event.id);
    if (event.kind == 1)
    {
        if (index < 0) return;
        const U8 idx = static_cast<U8>(index);
        auto open = mOpen.find(idx);
        if (open == mOpen.end()) return; // Discarded/unknown start is NOT replacement evidence.
        if (!open->second.startKnown) { mOpen.erase(open); return; } // [AutoAnimate] Baseline stop is censored.
        Entry& entry = mPool.entries[index];
        entry.dwell.add(static_cast<F32>(event.time - open->second.start));
        for (const auto& other : mOpen)
        {
            if (other.first == idx) continue;
            const F64 overlap = event.time - std::max(open->second.start, other.second.start);
            if (overlap > OVERLAP_TOL) edge(idx, other.first, true);
            else if (other.second.startKnown && other.second.start >= open->second.start) edge(idx, other.first, false);
        }
        mStopped[entry.layer] = index; mStopTime[entry.layer] = event.time;
        mOpen.erase(open);
        return;
    }
    if (index < 0)
    {
        if (event.id.isNull() || mPool.entries.size() >= mCap) return;
        Entry entry;
        static_cast<AnimFacts&>(entry) = event.facts; entry.id = event.id;
        entry.state = entry.loop_asset;
        mPool.entries.push_back(entry); index = static_cast<S32>(mPool.entries.size() - 1);
    }
    Entry& entry = mPool.entries[index];
    if (event.kind == 3) { mOpen[static_cast<U8>(index)] = {event.time, false}; return; }
    if (event.kind == 2 && (entry.loop_asset || entry.dwell.median() > entry.dur + 1.f)) return;
    const U8 idx = static_cast<U8>(index);
    const auto previous = mLastArrival.find(idx);
    if (previous != mLastArrival.end()) entry.inter.add(static_cast<F32>(event.time - previous->second));
    mLastArrival[idx] = event.time;
    if (entry.starts < 2147483647U) ++entry.starts;
    if (!entry.loop_asset)
    {
        U64 mask = 0;
        for (const auto& active : mOpen)
        {
            const Entry& candidate = mPool.entries[active.first];
            if (candidate.loop_asset || candidate.dwell.median() > candidate.dur + 1.f) mask |= U64(1) << active.first;
        }
        entry.co_mask |= mask;
        if (!mask) increment(entry.fires_without_state);
    }
    if (event.kind == 0)
    {
        const S32 stopped = mStopped[entry.layer];
        if (stopped >= 0 && event.time - mStopTime[entry.layer] <= REPLACE_GAP)
            edge(static_cast<U8>(stopped), idx, false);
        mStopped[entry.layer] = -1; // Even a non-state same-layer start consumes the gap.
    }
    mOpen[idx] = {event.time, true};
}
void ChannelRecorder::censor(F64 now)
{
    for (const auto& open : mOpen)
    {
        Entry& entry = mPool.entries[open.first];
        entry.max_censored_dwell = std::max(entry.max_censored_dwell,
            std::min(3600.f, static_cast<F32>(std::max(0.0, now - open.second.start))));
        for (const auto& other : mOpen)
            if (open.first < other.first && now - std::max(open.second.start, other.second.start) > OVERLAP_TOL)
                edge(open.first, other.first, true);
    }
}
U32 ChannelRecorder::onGateEnter(F64 now)
{
    const U32 discarded = static_cast<U32>(mBuffer.size());
    mBuffer.clear(); censor(now); mOpen.clear(); mLastArrival.clear(); mStopped.fill(-1); mGated = true;
    return discarded;
}
void ChannelRecorder::onGateExit() { mGated = false; }
void ChannelRecorder::addObserved(F32 dt) { if (!mGated) mPool.observed_s = std::min(3600.f, mPool.observed_s + dt); }
void ChannelRecorder::flush(F64 now, bool sealing)
{
    while (!mBuffer.empty() && now - mBuffer.front().time >= 1.0)
    {
        // [AutoAnimate] Facts get the same 1 s grace as the event buffer.
        // An uncached start is dropped if its facts do not arrive within 1 s;
        // that occurrence is never learned, even if the facts arrive later.
        // A cache miss cannot hold ready events hostage or reorder replacement evidence.
        fold(mBuffer.front()); mBuffer.erase(mBuffer.begin());
    }
    if (sealing) mBuffer.clear();
}
ChannelPool ChannelRecorder::snapshot(F64 now)
{
    flush(now); censor(now);
    ChannelPool result = mPool; result.finalize(); return result;
}
bool ChannelRecorder::takeOverflow() { const bool result = mOverflow; mOverflow = false; return result; }
F64 ChannelClock::sample(F64 sourceTime, const LLUUID& sourceId)
{
    if (sourceId.isNull()) return mLast;
    if (mSource != sourceId) { mOffset = mLast - sourceTime; mSource = sourceId; }
    mLast = sourceTime + mOffset; return mLast;
}

U64 ChannelScheduler::random() { mRng += 0x9e3779b97f4a7c15ULL; return mix(mRng); }
F32 ChannelScheduler::unit() { return static_cast<F32>(random() >> 40) / 16777216.f; }
F32 ChannelScheduler::dwell(S32 index)
{
    if (index < 0) return FALLBACK_DWELL;
    const Entry& entry = mPool->entries[index];
    if (!entry.dwell.count) return std::max(entry.max_censored_dwell * 1.25f, FALLBACK_DWELL);
    return entry.dwell.values[static_cast<size_t>(random() % entry.dwell.count)] * (0.85f + unit() * 0.3f);
}
F32 ChannelScheduler::inter(const Entry& entry)
{
    const F32 sample = entry.inter.count ? entry.inter.values[static_cast<size_t>(random() % entry.inter.count)] : FALLBACK_DWELL;
    return sample * (0.8f + unit() * 0.4f);
}
void ChannelScheduler::bind(const ChannelPool& pool, const LLUUID& instance, U32 salt, const std::string& key)
{
    auto copy = std::make_shared<ChannelPool>(pool); copy->finalize();
    bind(std::move(copy), instance, salt, key);
}
void ChannelScheduler::bind(std::shared_ptr<const ChannelPool> pool, const LLUUID& instance, U32 salt, const std::string& key)
{
    mPool = std::move(pool); mRng = mix(hash(instance.asString()) ^ hash(key) ^ salt);
    mStates.clear(); mAdoptedAlternates.clear(); mRemovals.clear(); mOneDue.clear(); mOneEnds.clear(); mRescheduleOneShots = false; mDesired.clear(); mCold = false;
    // Sequence counter belongs to the channel, not a particular pool binding.
}
void ChannelScheduler::unbind()
{
    mPool.reset(); mDesired.clear(); mStates.clear(); mAdoptedAlternates.clear(); mRemovals.clear();
    mOneDue.clear(); mOneEnds.clear(); mRescheduleOneShots = false; mCold = false; mFirstTick = false; mNextEventTime = 0.0;
}
S32 ChannelScheduler::groupFor(S32 entry, U8 layer) const
{
    S32 fallback = -1;
    for (size_t i = 0; i < mPool->groups.size(); ++i)
    {
        const auto& group = mPool->groups[i];
        if (entry >= 0 && std::find(group.begin(), group.end(), static_cast<U8>(entry)) != group.end()) return static_cast<S32>(i);
        if (entry < 0 && mPool->entries[group.front()].layer == layer)
        {
            if (group.size() > 1) return static_cast<S32>(i);
            if (fallback < 0) fallback = static_cast<S32>(i);
        }
    }
    return fallback;
}
void ChannelScheduler::adopt(const AnimMap& current, F64 now, const Facts& facts)
{
    mDesired = current; mStates.clear(); mAdoptedAlternates.clear(); mRemovals.clear(); mOneDue.clear(); mOneEnds.clear(); mRescheduleOneShots = false; mNextEventTime = now;
    mFirstTick = !current.empty(); mCold = current.empty();
    for (const auto& animation : current)
    {
        const S32 index = mPool->find(animation.first);
        AnimFacts info;
        if (index >= 0) info = mPool->entries[index];
        else if (!facts(animation.first, info)) continue;
        if (index >= 0 && !mPool->entries[index].state)
        {
            // [AutoAnimate] Age at adoption is unknown; allow its full duration.
            // Retirement's stop is a no-op if the shot has already ended naturally.
            mOneEnds[static_cast<U8>(index)] = now + std::max(MIN_FIRST_DWELL, info.dur);
            continue;
        }
        const S32 group = groupFor(index, info.layer);
        if (group >= 0)
        {
            auto same = std::find_if(mStates.begin(), mStates.end(), [group](const State& state) { return state.group == group; });
            if (same == mStates.end())
                mStates.push_back({animation.first, index, group, info.layer, now + std::max(MIN_FIRST_DWELL, dwell(index)), false});
            else
            {
                // A source transition can be adopted with both alternatives signaled.
                // Keep both until the first dwell; give the group only one decision clock.
                const S32 previousSeq = current.at(same->id);
                const bool newer = animation.second < 0 && previousSeq < 0
                    ? animation.second < previousSeq : animation.second > previousSeq;
                mAdoptedAlternates[group].push_back(newer ? same->id : animation.first);
                if (newer) { same->id = animation.first; same->entry = index; }
            }
        }
    }
    for (size_t i = 0; i < mPool->entries.size(); ++i)
        if (!mPool->entries[i].state && mPool->entries[i].starts >= MIN_ONESHOT_OBS)
            mOneDue[static_cast<U8>(i)] = now + inter(mPool->entries[i]);
}
void ChannelScheduler::markCold()
{
    for (const Removal& removal : mRemovals) mDesired.erase(removal.id);
    for (const auto& group : mAdoptedAlternates) for (const LLUUID& id : group.second) mDesired.erase(id);
    mAdoptedAlternates.clear();
    for (State& state : mStates) state.reissue = true;
    // [AutoAnimate] COLD reissues current states only, never a burst of old shots.
    for (const auto& end : mOneEnds) mDesired.erase(mPool->entries[end.first].id);
    mOneEnds.clear(); mRescheduleOneShots = true;
    mRemovals.clear(); mCold = true; mFirstTick = false; mNextEventTime = 0.0;
}
void ChannelScheduler::issue(const LLUUID& id)
{
    if (mNextSeq == std::numeric_limits<S32>::min()) return;
    mDesired[id] = mNextSeq--;
}
S32 ChannelScheduler::choose(const std::vector<U8>& choices, const LLUUID& except, const Resident& resident)
{
    U64 total = 0;
    for (U8 index : choices)
        if (mPool->entries[index].id != except && resident(mPool->entries[index].id)) total += std::max(1U, mPool->entries[index].starts);
    if (!total) return -1;
    U64 draw = random() % total;
    for (U8 index : choices)
    {
        const Entry& entry = mPool->entries[index];
        if (entry.id == except || !resident(entry.id)) continue;
        const U32 weight = std::max(1U, entry.starts);
        if (draw < weight) return index;
        draw -= weight;
    }
    return -1;
}
U64 ChannelScheduler::activeStateMask() const
{
    U64 result = 0;
    for (const State& state : mStates) if (state.entry >= 0) result |= U64(1) << state.entry;
    for (const auto& group : mAdoptedAlternates)
        for (const LLUUID& id : group.second)
        { const S32 entry = mPool->find(id); if (entry >= 0) result |= U64(1) << entry; }
    return result;
}
bool ChannelScheduler::owns(const LLUUID& id) const
{
    for (const State& state : mStates) if (state.id == id) return true;
    for (const auto& group : mAdoptedAlternates)
        if (std::find(group.second.begin(), group.second.end(), id) != group.second.end()) return true;
    for (const Removal& removal : mRemovals) if (removal.id == id) return true;
    for (const auto& due : mOneDue) if (mPool->entries[due.first].id == id) return true;
    return false;
}
bool ChannelScheduler::tick(F64 now, const Resident& resident, std::vector<LLUUID>& removed,
    std::vector<LLUUID>* retired)
{
    removed.clear();
    if (retired) retired->clear(); // [AutoAnimate] Audit-only classification for this tick.
    if (!mPool) return false;
    if (mFirstTick) { mFirstTick = false; return false; }
    if (now < mNextEventTime) return false;
    const AnimMap previous = mDesired;
    const bool coldChanged = mRescheduleOneShots; // [AutoAnimate] markCold may already have pruned desired.
    if (mRescheduleOneShots)
    {
        for (auto& due : mOneDue) due.second = now + inter(mPool->entries[due.first]);
        mRescheduleOneShots = false;
    }
    if (mCold)
    {
        bool retry = false;
        for (State& state : mStates)
        {
            if (!state.reissue) continue;
            if (resident(state.id))
            {
                issue(state.id); state.reissue = false;
                state.due = now + std::max(MIN_FIRST_DWELL, dwell(state.entry));
            }
            else { retry = true; state.due = now + 5.0; }
        }
        for (U8 layer = 0; layer < LAYER_COUNT; ++layer)
        {
            if (std::any_of(mStates.begin(), mStates.end(), [layer](const State& s) { return s.layer == layer; })) continue;
            std::vector<U8> candidates;
            U32 states = 0;
            for (const Entry& entry : mPool->entries) if (entry.state && entry.layer == layer) ++states;
            for (size_t i = 0; i < mPool->entries.size(); ++i)
            {
                const Entry& entry = mPool->entries[i];
                if (!entry.state || entry.layer != layer) continue;
                const bool edged = std::any_of(mPool->edges.begin(), mPool->edges.end(), [i](const Edge& e)
                    { return e.replace && (static_cast<size_t>(e.a) == i || static_cast<size_t>(e.b) == i); });
                if (edged || states == 1) candidates.push_back(static_cast<U8>(i));
            }
            const S32 index = choose(candidates, LLUUID::null, resident);
            if (index >= 0)
            {
                const Entry& entry = mPool->entries[index]; issue(entry.id);
                mStates.push_back({entry.id, index, groupFor(index, layer), layer, now + std::max(MIN_FIRST_DWELL, dwell(index)), false});
            }
            else if (!candidates.empty()) retry = true;
        }
        mCold = retry;
    }
    for (auto it = mRemovals.begin(); it != mRemovals.end();)
    {
        if (now >= it->due) { removed.push_back(it->id); mDesired.erase(it->id); it = mRemovals.erase(it); }
        else ++it;
    }
    for (State& state : mStates)
    {
        if (now < state.due || state.group < 0 || state.reissue) continue;
        const S32 next = choose(mPool->groups[state.group], state.id, resident);
        if (next < 0 || mPool->entries[next].id == state.id) { state.due = now + dwell(state.entry); continue; }
        const Entry& entry = mPool->entries[next]; issue(entry.id);
        mRemovals.push_back({state.id, now + std::max(entry.ease_in, 0.2f)});
        auto extras = mAdoptedAlternates.find(state.group);
        if (extras != mAdoptedAlternates.end())
        {
            for (const LLUUID& id : extras->second)
                if (id != entry.id) mRemovals.push_back({id, now + std::max(entry.ease_in, 0.2f)});
            mAdoptedAlternates.erase(extras);
        }
        state.id = entry.id; state.entry = next; state.due = now + std::max(dwell(next), std::max(entry.ease_in, 0.2f));
    }
    // [AutoAnimate] Retire completed shots before considering their next fire.
    for (auto it = mOneEnds.begin(); it != mOneEnds.end();)
    {
        if (now >= it->second)
        {
            const LLUUID& id = mPool->entries[it->first].id;
            if (retired) retired->push_back(id); // [AutoAnimate] Natural retirement is not POP.
            mDesired.erase(id); removed.push_back(id); it = mOneEnds.erase(it);
        }
        else ++it;
    }
    for (auto& due : mOneDue)
    {
        if (now < due.second) continue;
        const Entry& entry = mPool->entries[due.first];
        if (!entry.fires_without_state && !(activeStateMask() & entry.co_mask)) { due.second = now + 2.0; continue; }
        if (!resident(entry.id)) { due.second = now + 5.0; continue; }
        issue(entry.id); due.second = now + inter(entry);
        mOneEnds[due.first] = now + entry.dur;
        // A long frame can retire and re-fire the same shot; do not stop the new fire.
        removed.erase(std::remove(removed.begin(), removed.end(), entry.id), removed.end());
        if (retired) retired->erase(std::remove(retired->begin(), retired->end(), entry.id), retired->end()); // [AutoAnimate]
    }
    mNextEventTime = mCold ? now + 5.0 : std::numeric_limits<F64>::max();
    for (const State& state : mStates) mNextEventTime = std::min(mNextEventTime, state.due);
    for (const Removal& removal : mRemovals) mNextEventTime = std::min(mNextEventTime, removal.due);
    for (const auto& due : mOneDue) mNextEventTime = std::min(mNextEventTime, due.second);
    for (const auto& end : mOneEnds) mNextEventTime = std::min(mNextEventTime, end.second);
    return coldChanged || previous != mDesired;
}

namespace
{
LLSD reservoirData(const Reservoir& samples)
{
    LLSD data = LLSD::emptyArray();
    for (U32 i = 0; i < samples.count; ++i) data.append(samples.values[i]);
    return data;
}
LLSD channelData(const ChannelPool& channel)
{
    LLSD data; data["observed_s"] = channel.observed_s; data["switches"] = static_cast<S32>(channel.switches);
    data["entries"] = LLSD::emptyArray(); data["edges"] = LLSD::emptyArray();
    for (const Entry& entry : channel.entries)
    {
        LLSD e; e["id"] = entry.id; e["loop"] = entry.loop_asset; e["dur"] = entry.dur;
        e["ease_in"] = entry.ease_in; e["prio"] = entry.prio; e["layer"] = entry.layer;
        e["starts"] = static_cast<S32>(entry.starts); e["dwell"] = reservoirData(entry.dwell); e["inter"] = reservoirData(entry.inter);
        std::ostringstream mask; mask << std::hex << std::setfill('0') << std::setw(16) << entry.co_mask;
        e["co_mask"] = mask.str(); e["no_state_fires"] = entry.fires_without_state; e["max_censored"] = entry.max_censored_dwell;
        data["entries"].append(e);
    }
    for (const Edge& edge : channel.edges)
    {
        LLSD e; e["a"] = edge.a; e["b"] = edge.b; e["replace"] = edge.replace; e["coexist"] = edge.coexist;
        data["edges"].append(e);
    }
    return data;
}
bool floatData(const LLSD& data, F32& value)
{
    if (!data.isReal() && !data.isInteger()) return false;
    const F64 number = data.asReal();
    if (!std::isfinite(number) || number < 0.0 || number > 3600.0) return false;
    value = static_cast<F32>(number); return true;
}
bool integerData(const LLSD& data, S32 lower, S32 upper)
{ return data.isInteger() && data.asInteger() >= lower && data.asInteger() <= upper; }
bool readReservoir(const LLSD& data, Reservoir& result)
{
    if (!data.isArray() || data.size() > 16) return false;
    for (const LLSD& item : llsd::inArray(data)) { F32 value = 0.f; if (!floatData(item, value)) return false; result.add(value); }
    return true;
}
bool readChannel(const LLSD& data, ChannelPool& result, std::string& reason)
{
    reason = "channel fields/counts";
    if (!data.isMap() || !floatData(data["observed_s"], result.observed_s) ||
        !integerData(data["switches"], 0, 2147483647) || !data["entries"].isArray() || data["entries"].size() > 64 ||
        !data["edges"].isArray() || data["edges"].size() > 256) return false;
    result.switches = static_cast<U32>(data["switches"].asInteger());
    std::set<LLUUID> ids;
    for (const LLSD& item : llsd::inArray(data["entries"]))
    {
        Entry entry; reason = "entry fields/ranges";
        if (!item.isMap() || !item["id"].isUUID() || !item["loop"].isBoolean()) return false;
        entry.id = item["id"].asUUID();
        if (entry.id.isNull() || !ids.insert(entry.id).second || !floatData(item["dur"], entry.dur) ||
            !floatData(item["ease_in"], entry.ease_in) || !floatData(item["max_censored"], entry.max_censored_dwell) ||
            !integerData(item["prio"], -1, 7) || !integerData(item["layer"], 0, 4) ||
            !integerData(item["starts"], 0, 2147483647) || !integerData(item["no_state_fires"], 0, 65535) ||
            !readReservoir(item["dwell"], entry.dwell) || !readReservoir(item["inter"], entry.inter)) return false;
        entry.loop_asset = item["loop"].asBoolean(); entry.prio = item["prio"].asInteger();
        entry.layer = static_cast<U8>(item["layer"].asInteger()); entry.starts = static_cast<U32>(item["starts"].asInteger());
        entry.fires_without_state = static_cast<U16>(item["no_state_fires"].asInteger());
        const std::string mask = item["co_mask"].asString();
        if (!item["co_mask"].isString() || mask.size() != 16 || mask.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return false;
        std::istringstream input(mask); input >> std::hex >> entry.co_mask;
        result.entries.push_back(entry);
    }
    const U64 valid = result.entries.size() == 64 ? ~U64(0) : (U64(1) << result.entries.size()) - 1;
    for (Entry& entry : result.entries) entry.co_mask &= valid;
    std::set<std::pair<U8, U8>> pairs;
    for (const LLSD& item : llsd::inArray(data["edges"]))
    {
        reason = "edge indices/ranges";
        const S32 last = static_cast<S32>(result.entries.size()) - 1;
        if (!item.isMap() || !integerData(item["a"], 0, last) || !integerData(item["b"], 0, last) ||
            !integerData(item["replace"], 0, 65535) || !integerData(item["coexist"], 0, 65535)) return false;
        Edge edge{static_cast<U8>(item["a"].asInteger()), static_cast<U8>(item["b"].asInteger()),
                  static_cast<U16>(item["replace"].asInteger()), static_cast<U16>(item["coexist"].asInteger())};
        if (edge.a == edge.b) return false;
        if (edge.a > edge.b) std::swap(edge.a, edge.b);
        if (!pairs.insert({edge.a, edge.b}).second) return false;
        result.edges.push_back(edge);
    }
    result.finalize(); return true;
}
}
LLSD toLLSD(const Pool& pool)
{
    LLSD data; data["kind"] = "GhostAutoAnimPool"; data["version"] = static_cast<S32>(pool.mVersion);
    data["sealed"] = pool.mSealed; data["observed_s"] = pool.mObservedSeconds; data["body"] = channelData(pool.mBody);
    data["animesh"] = LLSD::emptyArray();
    for (const auto& channel : pool.mAnimesh)
    {
        LLSD item; item["key"]["point"] = channel.first.point; item["key"]["item"] = channel.first.item;
        item["key"]["prims"] = static_cast<S32>(channel.first.prims); item["key"]["ordinal"] = static_cast<S32>(channel.first.ordinal);
        item["channel"] = channelData(channel.second); data["animesh"].append(item);
    }
    return data;
}
bool fromLLSD(const LLSD& data, Pool& pool, std::string& reason)
{
    Pool candidate; reason = "kind/version/sealed";
    if (!data.isMap() || data["kind"].asString() != "GhostAutoAnimPool" ||
        !integerData(data["version"], 1, 1) || !data["sealed"].isBoolean()) return false;
    reason = "observed/channels";
    if (!floatData(data["observed_s"], candidate.mObservedSeconds) || !data["animesh"].isArray() || data["animesh"].size() > 32) return false;
    if (!readChannel(data["body"], candidate.mBody, reason)) return false;
    for (const LLSD& item : llsd::inArray(data["animesh"]))
    {
        const LLSD& key = item["key"]; LinksetKey binding; ChannelPool channel; reason = "linkset key";
        if (!item.isMap() || !key.isMap() || !integerData(key["point"], 0, 255) || !key["item"].isUUID() ||
            !integerData(key["prims"], 1, 65535) || !integerData(key["ordinal"], 0, 31)) return false;
        binding.point = key["point"].asInteger(); binding.item = key["item"].asUUID();
        binding.prims = static_cast<U32>(key["prims"].asInteger()); binding.ordinal = static_cast<U32>(key["ordinal"].asInteger());
        for (const auto& previous : candidate.mAnimesh) if (previous.first.matches(binding)) return false;
        if (!readChannel(item["channel"], channel, reason)) return false;
        candidate.mAnimesh.emplace_back(binding, std::move(channel));
    }
    candidate.mSealed = true; pool = std::move(candidate); reason.clear(); return true;
}
}
