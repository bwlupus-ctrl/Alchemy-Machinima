// [AutoAnimate] Learned performance model. No viewer, motion or rendering dependencies.
#ifndef AL_GHOST_AUTO_ANIM_MODEL_H
#define AL_GHOST_AUTO_ANIM_MODEL_H

#include "stdtypes.h"
#include "lluuid.h"
#include "llsd.h"
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ALGhostAutoAnim
{
constexpr F32 OVERLAP_TOL = 0.5f;
constexpr F32 REPLACE_GAP = 2.f;
constexpr F32 MIN_FIRST_DWELL = 2.f;
constexpr F32 FALLBACK_DWELL = 30.f;
constexpr F32 RECOUPLE_DEBOUNCE = 0.5f;
constexpr U32 MIN_ONESHOT_OBS = 3;
enum ELayer : U8 { LAYER_BODY, LAYER_UPPER, LAYER_HANDS, LAYER_FACE, LAYER_EXTRA, LAYER_COUNT };
enum Autonomy : U8 { AUTONOMY_NONE, AUTONOMY_AUTO_LOST, AUTONOMY_MANUAL };
enum class Action : U8 { GoAutonomous, FollowSource, StopLearning, Relearn, Reseed, SimLossToggle };
using AnimMap = std::map<LLUUID, S32>;
struct AnimFacts
{
    bool loop_asset = false;
    F32 dur = 0.f, ease_in = 0.f;
    S32 prio = 0;
    U8 layer = LAYER_EXTRA;
};
U8 classifyLayer(const std::vector<std::string>& joints);
struct Reservoir
{
    std::array<F32, 16> values{};
    U32 count = 0, seen = 0;
    void add(F32 value);
    F32 median() const;
};
struct Entry : AnimFacts
{
    LLUUID id;
    U32 starts = 0;
    Reservoir dwell, inter;
    U64 co_mask = 0;
    U16 fires_without_state = 0;
    F32 max_censored_dwell = 0.f;
    bool state = false;
};
struct Edge
{
    U8 a = 0, b = 0;
    U16 replace = 0, coexist = 0;
};
struct ChannelPool
{
    F32 observed_s = 0.f;
    U32 switches = 0;
    std::vector<Entry> entries;
    std::vector<Edge> edges;
    std::vector<std::vector<U8>> groups;
    bool thin = true;
    void finalize();
    S32 find(const LLUUID& id) const;
};
struct LinksetKey
{
    S32 point = 0;
    LLUUID item;
    U32 prims = 0, ordinal = 0;
    bool matches(const LinksetKey& other) const;
    std::string text() const;
};
struct Pool
{
    U32 mVersion = 1;
    bool mSealed = false;
    F32 mObservedSeconds = 0.f;
    ChannelPool mBody;
    std::vector<std::pair<LinksetKey, ChannelPool>> mAnimesh;
    const ChannelPool* findAnimesh(const LinksetKey& key, bool* weakMatch = nullptr) const;
};
struct Config
{
    bool mEnabled = false;
    LLUUID mInstanceId;
    U32 mSeedSalt = 0;
    F32 mWindowSeconds = 120.f;
    std::shared_ptr<const Pool> mPool;
    bool mManual = false;
};
struct LearnStatus
{
    bool mOn = false, mLearning = false, mGated = false;
    F32 mObserved = 0.f, mWindow = 120.f;
    U32 mLiveEntries = 0, mBodyStates = 0, mBodyOneShots = 0;
    U32 mAnimeshChannels = 0, mAnimeshAutonomous = 0;
    bool mBodyAutonomous = false, mManual = false, mThin = false;
    std::string mDetail;
};
class ChannelRecorder
{
public:
    explicit ChannelRecorder(U32 cap = 64);
    void reset();
    void seedFrom(const ChannelPool& pool);
    void onStart(const LLUUID& id, const AnimFacts& facts, F64 now);
    void onBaseline(const LLUUID& id, const AnimFacts& facts, F64 now); // [AutoAnimate] Start unknown.
    void onStop(const LLUUID& id, F64 now);
    void onRetrigger(const LLUUID& id, const AnimFacts& facts, F64 now);
    void onPending(const LLUUID& id, F64 now, bool retrigger, bool baseline = false);
    void resolveFacts(const LLUUID& id, const AnimFacts& facts);
    bool hasPendingFacts(const LLUUID& id) const; // [AutoAnimate] Drop timed-out retry ids too.
    U32 onGateEnter(F64 now);
    void onGateExit();
    void addObserved(F32 dt);
    void flush(F64 now, bool sealing = false);
    ChannelPool snapshot(F64 now);
    U32 entryCount() const { return static_cast<U32>(mPool.entries.size()); }
    bool takeOverflow();
private:
    struct Event { LLUUID id; AnimFacts facts; F64 time = 0.0; U8 kind = 0; };
    struct Open { F64 start = 0.0; bool startKnown = true; };
    void enqueue(const Event& event);
    void fold(const Event& event);
    void edge(U8 a, U8 b, bool coexist);
    void censor(F64 now);
    ChannelPool mPool;
    U32 mCap;
    bool mGated = false, mOverflow = false;
    std::vector<Event> mBuffer;
    std::map<U8, Open> mOpen;
    std::map<U8, F64> mLastArrival;
    std::array<S32, LAYER_COUNT> mStopped{};
    std::array<F64, LAYER_COUNT> mStopTime{};
};
class ChannelClock
{
public:
    F64 sample(F64 sourceTime, const LLUUID& sourceId);
    F64 time() const { return mLast; }
    const LLUUID& sourceId() const { return mSource; }
private:
    LLUUID mSource;
    F64 mOffset = 0.0, mLast = 0.0;
};
class ChannelScheduler
{
public:
    using Resident = std::function<bool(const LLUUID&)>;
    using Facts = std::function<bool(const LLUUID&, AnimFacts&)>;
    void bind(const ChannelPool& pool, const LLUUID& instance, U32 salt, const std::string& key);
    void bind(std::shared_ptr<const ChannelPool> pool, const LLUUID& instance, U32 salt, const std::string& key);
    void unbind();
    void adopt(const AnimMap& current, F64 now, const Facts& facts);
    void markCold();
    // [AutoAnimate] Retirements remain in removed; expose them separately for the POP audit.
    bool tick(F64 now, const Resident& resident, std::vector<LLUUID>& removed,
        std::vector<LLUUID>* retired = nullptr);
    const AnimMap& desired() const { return mDesired; }
    U64 activeStateMask() const;
    bool owns(const LLUUID& id) const;
    bool coldPending() const { return mCold; }
private:
    struct State { LLUUID id; S32 entry = -1, group = -1; U8 layer = 0; F64 due = 0.0; bool reissue = false; };
    struct Removal { LLUUID id; F64 due = 0.0; };
    U64 random();
    F32 unit();
    F32 dwell(S32 entry);
    F32 inter(const Entry& entry);
    S32 choose(const std::vector<U8>& choices, const LLUUID& except, const Resident& resident);
    S32 groupFor(S32 entry, U8 layer) const;
    void issue(const LLUUID& id);
    std::shared_ptr<const ChannelPool> mPool;
    AnimMap mDesired;
    std::vector<State> mStates;
    std::map<S32, std::vector<LLUUID>> mAdoptedAlternates;
    std::vector<Removal> mRemovals;
    std::map<U8, F64> mOneDue;
    std::map<U8, F64> mOneEnds; // [AutoAnimate] Retire completed shots from desired.
    bool mRescheduleOneShots = false;
    U64 mRng = 0;
    S32 mNextSeq = -1;
    F64 mNextEventTime = 0.0;
    bool mCold = false, mFirstTick = false;
};
LLSD toLLSD(const Pool& pool);
bool fromLLSD(const LLSD& data, Pool& pool, std::string& reason);
}
#endif
