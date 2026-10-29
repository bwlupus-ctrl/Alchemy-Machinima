/**
 * @file llghostavatar.cpp
 * @brief Client-only avatar used as a scene-lit Ghost Studio clone
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "llghostavatar.h"

// [AutoAnimate] Cache facts are copied; no cache-owned pointers survive a call.
#include "llkeyframemotion.h"
#include "llframetimer.h"
#include "llcharacter.h"
#include "llmotioncontroller.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef> // [AutoAnimate]
#include <ratio>
#include <sstream>
#include <utility>

#include "alghoststudio.h"
#include "alghostattachmentenumerator.h"
#include "alghostmaterialresolver.h"
#include "llactormover.h"          // walk ownership arbitration: applyOverride() vs the foot-lock
#include "llagent.h"
#include "llanimationstates.h"
#include "llviewerobjectlist.h"
#include "llviewerjointattachment.h"
#include "llvoavatarself.h"
#include "llvovolume.h"
#include "llmeshrepository.h"
#include "llskinningutil.h"
#include "llviewerregion.h"
#include "llviewercontrol.h"
#include "llviewertextureanim.h"
#include "llvolumemgr.h"
#include "llmaterial.h"
#include "llgltfmaterial.h"
#include "llfetchedgltfmaterial.h"
#include "llcontrolavatar.h"
#include "llspatialpartition.h"

#include <boost/unordered_map.hpp>   // clone-prim -> source-prim LOD index
#include "llface.h"
#include "lldrawable.h"
#include "lldrawpool.h"
#include "indra_constants.h"
#include "pipeline.h"

// [GhostStudio] Ghosts spawned by the /ghosttest harness, so it can clean up
// after itself. Not the production Ghost Studio store.
//
// Deliberately UUIDs, not LLPointers: a file-static strong reference can
// outlive gObjectList's logout/shutdown cleanup and drag an LLVOAvatar's
// destruction into static-destruction time, after pipeline/texture globals
// are gone. Storing ids leaves lifetime entirely to gObjectList.
static std::vector<LLUUID> sPaletteTestHarnessGhostIds;

// Clone prim id -> the simulator-known SOURCE prim it was cloned from.
//
// getClonedSourceLOD() runs from LLVOVolume::calcLOD() for every local-only
// prim whose LOD is re-evaluated, which for animating clones is close to every
// frame. Searching every ghost's linkset graph for the answer is O(ghosts x
// linksets x children) with a heap allocation per call -- at 50 clones that is
// the only super-linear term in the clone path. The mapping never changes
// between attach and detach, so record it once here instead of re-deriving it.
//
// Populated in cloneAttachmentsFrom() as each linkset record is built, and
// erased in releaseClonedAttachments() (which every teardown path funnels
// through, including the palette test harness). Entries are plain ids, so a
// stale one simply fails to resolve and behaves exactly as a miss.
static boost::unordered_map<LLUUID, LLUUID> sClonePrimToSourcePrim;

// [AutoAnimate] Everything below is entered only for an enabled instance.
namespace
{
using namespace ALGhostAutoAnim;
F64 sAutoAnimRecordMicros = 0.0, sAutoAnimScheduleMicros = 0.0;
struct AutoAnimCostScope
{
    explicit AutoAnimCostScope(F64& total) : mTotal(total), mStart(std::chrono::steady_clock::now()) {}
    ~AutoAnimCostScope()
    { mTotal += std::chrono::duration<F64, std::micro>(std::chrono::steady_clock::now() - mStart).count(); }
    F64& mTotal;
    std::chrono::steady_clock::time_point mStart;
};
bool facts_for(const LLUUID& id, AnimFacts& facts)
{
    const LLKeyframeMotion::JointMotionList* data = LLKeyframeDataCache::getKeyframeData(id);
    if (!data) return false;
    facts.loop_asset = data->mLoop; facts.dur = data->mDuration;
    facts.ease_in = data->mEaseInDuration; facts.prio = static_cast<S32>(data->mBasePriority);
    std::vector<std::string> joints;
    joints.reserve(data->mJointMotionArray.size());
    for (const auto* joint : data->mJointMotionArray) if (joint && joint->mUsage) joints.push_back(joint->mJointName);
    facts.layer = classifyLayer(joints);
    return true;
}
bool autoAnimExcluded(const LLUUID& id)
{
    if (!gAnimLibrary.animStateToString(id)) return false;
    for (S32 i = 0; i < NUM_AGENT_STAND_ANIMS; ++i) if (id == AGENT_STAND_ANIMS[i]) return false;
    return true;
}
const char* autoAnimGated(LLVOAvatar* source)
{
    if (!source || source->isDead()) return "";
    const auto& signals = source->mSignaledAnimations;
    if (source->isSitting()) return "sit";
    for (const LLUUID& id : {ANIM_AGENT_SIT, ANIM_AGENT_SIT_FEMALE, ANIM_AGENT_SIT_GENERIC,
         ANIM_AGENT_SIT_GROUND, ANIM_AGENT_SIT_GROUND_CONSTRAINED, ANIM_AGENT_SIT_TO_STAND, ANIM_AGENT_STANDUP})
        if (signals.count(id)) return "sit";
    if (source->getID() == gAgent.getID() && gAgent.getFlying()) return "flying";
    for (const LLUUID& id : {ANIM_AGENT_FLY, ANIM_AGENT_FLYSLOW, ANIM_AGENT_HOVER, ANIM_AGENT_HOVER_UP,
         ANIM_AGENT_HOVER_DOWN, ANIM_AGENT_PRE_JUMP, ANIM_AGENT_JUMP, ANIM_AGENT_LAND, ANIM_AGENT_MEDIUM_LAND, ANIM_AGENT_FALLDOWN})
        if (signals.count(id)) return "air";
    if (source->getVelocity().length() > 0.3f) return "speed";
    // [AutoAnimate] AGENT_WALK_ANIMS also contains stationary turns.
    for (S32 i = 0; i < NUM_AGENT_WALK_ANIMS; ++i)
        if (AGENT_WALK_ANIMS[i] != ANIM_AGENT_TURNLEFT && AGENT_WALK_ANIMS[i] != ANIM_AGENT_TURNRIGHT &&
            signals.count(AGENT_WALK_ANIMS[i])) return "loco";
    return "";
}
void autoAnimPrefetch(const ChannelPool* pool, LLCharacter* character, const std::string& channel)
{
    if (!pool || !character) return;
    // Process-wide, including crowds. Failed fetches can retry after 60 seconds.
    static std::map<LLUUID, F64> requested;
    const F64 now = LLFrameTimer::getTotalSeconds();
    for (auto it = requested.begin(); it != requested.end();)
    {
        if (LLKeyframeDataCache::getKeyframeData(it->first) || now - it->second >= 60.0) it = requested.erase(it);
        else ++it;
    }
    for (const Entry& entry : pool->entries)
    {
        if (LLKeyframeDataCache::getKeyframeData(entry.id) || requested.count(entry.id)) continue;
        requested.emplace(entry.id, now);
        character->createMotion(entry.id); // Created stopped: fetch without activation.
        LL_INFOS("GhostStudio") << "AUTOANIM-FETCH id=" << entry.id << " ch=" << channel << LL_ENDL;
    }
}
void autoAnimSkip(const LLUUID& id, const char* reason)
{
    static std::map<std::pair<LLUUID, std::string>, F64> logged;
    const F64 now = LLFrameTimer::getTotalSeconds();
    const auto key = std::make_pair(id, std::string(reason));
    const auto found = logged.find(key);
    if (found == logged.end() || now - found->second >= 60.0)
    {
        logged[key] = now;
        LL_INFOS("GhostStudio") << "AUTOANIM-SKIP " << id << ' ' << reason << LL_ENDL;
    }
}
bool autoAnimResident(const LLUUID& id)
{
    if (LLKeyframeDataCache::getKeyframeData(id)) return true;
    autoAnimSkip(id, "unloaded");
    return false;
}
F32 autoAnimLossGrace()
{
    static LLCachedControl<F32> grace(gSavedSettings, "GhostAutoAnimateLossGrace", 1.5f);
    const F32 value = grace;
    return std::isfinite(value) ? llclamp(value, 0.25f, 10.f) : 1.5f;
}
void autoAnimMerge(AnimMap& merged, const AnimMap& prim)
{
    for (const auto& animation : prim)
    {
        auto found = merged.find(animation.first);
        if (found == merged.end()) merged.insert(animation);
        else found->second = std::max(found->second, animation.second);
    }
}
}

struct LLGhostAvatar::AutoAnimRuntime
{
    struct Channel
    {
        LinksetKey key;
        std::string name = "body";
        std::unique_ptr<ChannelRecorder> recorder;
        ChannelScheduler scheduler;
        ChannelClock clock;
        AnimMap last, merged;
        std::map<LLUUID, U32> refcounts;
        std::set<LLUUID> changedIds; // Keep the MSVC tree sentinel across frames.
        std::vector<AnimMap> primLast;
        std::vector<LLUUID> pending;
        std::vector<LLUUID> removed;
        std::vector<LLUUID> retired; // [AutoAnimate] One-shot retirements are excluded from the POP audit.
        U8 autonomy = AUTONOMY_NONE;
        F64 absentSince = -1.0, presentSince = -1.0;
        F64 fetchDue = 0.0;
        bool baseline = false, paused = false, bindPending = false, cold = false;
        bool bound = false, noPoolLogged = false;
        bool overflowLogged = false;
        std::string kind = "manual", absent = "null";
        const ChannelPool* pool = nullptr;
        AnimMap adoption;
        F64 handoffTime = 0.0; // [AutoAnimate] Real-time audit includes the following 2 s.
        U32 oursStopped = 0;
        U32 carried = 0;
        bool auditHandoff = false;
        std::string clockName;
        mutable U8 detailAutonomy = 255; // [AutoAnimate] Cached tooltip inputs.
        mutable bool detailThin = false;
    };
    Config config;
    std::shared_ptr<const Pool> pool, published, incoming;
    Channel body;
    std::map<LLUUID, Channel> links;
    std::vector<Action> actions;
    bool learning = false, simloss = false, gated = false;
    F32 observed = 0.f, checkpoint = 0.f;
    F64 lastTick = 0.0, settleUntil = 0.0;
    std::string gateReason;
    mutable std::string statusDetail, statusGate;
    mutable S32 statusMode = -1;
    mutable size_t statusChannels = 0;

    void startRecorder(Channel& channel)
    {
        channel.recorder.reset(new ChannelRecorder(static_cast<U32>(llclamp(gSavedSettings.getS32("GhostAutoAnimateMaxAnims"), 8, 64))));
        if (channel.pool) channel.recorder->seedFrom(*channel.pool);
        channel.last.clear(); channel.pending.clear(); channel.baseline = true; // [AutoAnimate] First starts are unknown.
        channel.paused = false; channel.noPoolLogged = false; channel.overflowLogged = false;
    }
    void retryPending(Channel& channel, bool sealing)
    {
        for (auto it = channel.pending.begin(); it != channel.pending.end();)
        {
            AnimFacts facts;
            if (facts_for(*it, facts))
            {
                channel.recorder->resolveFacts(*it, facts);
                it = channel.pending.erase(it);
            }
            else if (sealing || !channel.recorder->hasPendingFacts(*it))
            {
                autoAnimSkip(*it, "uncached");
                it = channel.pending.erase(it);
            }
            else ++it;
        }
    }
    void record(Channel& channel, const AnimMap& signals, F64 now, F32 dt, bool present, bool baselineOnly)
    {
        if (!channel.recorder) return;
        const bool pause = !present || gated || channel.autonomy != AUTONOMY_NONE;
        if (pause)
        {
            if (!channel.paused) channel.recorder->onGateEnter(now);
            channel.paused = true; channel.baseline = true; channel.pending.clear(); return;
        }
        if (channel.paused) { channel.recorder->onGateExit(); channel.paused = false; }
        channel.recorder->addObserved(dt);
        if (!channel.pending.empty()) retryPending(channel, false); // [AutoAnimate] Cache arrivals need no signal diff.
        if (baselineOnly || channel.baseline)
        {
            if (!baselineOnly) for (const auto& signal : signals)
            {
                if (autoAnimExcluded(signal.first)) continue;
                AnimFacts facts;
                if (facts_for(signal.first, facts)) channel.recorder->onBaseline(signal.first, facts, now);
                else if (channel.pending.size() < 16)
                {
                    channel.pending.push_back(signal.first);
                    channel.recorder->onPending(signal.first, now, false, true);
                }
            }
            if (channel.last != signals) channel.last = signals;
            channel.baseline = baselineOnly; return;
        }
        if (channel.last != signals)
        {
            for (const auto& old : channel.last)
                if (!signals.count(old.first) && !autoAnimExcluded(old.first)) channel.recorder->onStop(old.first, now);
            for (const auto& signal : signals)
            {
                if (autoAnimExcluded(signal.first)) continue;
                const auto old = channel.last.find(signal.first);
                if (old != channel.last.end() && old->second == signal.second) continue;
                AnimFacts facts;
                const U8 kind = old == channel.last.end() ? U8(0) : U8(2);
                if (facts_for(signal.first, facts))
                {
                    if (kind == 0) channel.recorder->onStart(signal.first, facts, now);
                    else channel.recorder->onRetrigger(signal.first, facts, now);
                }
                else
                {
                    const bool pending = std::find(channel.pending.begin(), channel.pending.end(), signal.first) != channel.pending.end();
                    if (pending || channel.pending.size() < 16)
                    {
                        if (!pending) channel.pending.push_back(signal.first);
                        channel.recorder->onPending(signal.first, now, kind == 2);
                    }
                }
            }
            channel.last = signals;
        }
        channel.recorder->flush(now);
        if (channel.recorder->takeOverflow() && !channel.overflowLogged)
        {
            channel.overflowLogged = true;
            LL_WARNS("GhostStudio") << "AUTOANIM-GATE buffer overflow: oldest event folded early ch=" << channel.name << LL_ENDL;
        }
    }
    std::shared_ptr<const Pool> snapshot(F64 now, bool seal, const char* reason, const LLUUID& runtime)
    {
        if (!learning) return pool;
        auto result = std::make_shared<Pool>(); result->mObservedSeconds = observed; result->mSealed = seal;
        auto capture = [&](Channel& channel)
        {
            retryPending(channel, seal);
            channel.recorder->flush(now, seal);
            ChannelPool saved = channel.recorder->snapshot(now);
            if (seal) // [AutoAnimate] Verdicts describe a real seal, never a checkpoint.
            {
                U32 states = 0, oneshots = 0;
                for (const Entry& entry : saved.entries) { if (entry.state) ++states; else ++oneshots; }
                LL_INFOS("GhostStudio") << "AUTOANIM-SEAL ch=" << channel.name << " states=" << states
                    << " oneshots=" << oneshots << " groups=" << saved.groups.size() << " switches=" << saved.switches
                    << " observed=" << saved.observed_s << LL_ENDL;
                if (saved.observed_s == 0.f) LL_INFOS("GhostStudio") << "AUTOANIM-NODATA ch=" << channel.name << LL_ENDL;
                if (saved.thin) LL_INFOS("GhostStudio") << "AUTOANIM-POOL THIN ch=" << channel.name
                    << " reason=too few replacements; AO timer probably longer than window" << LL_ENDL;
            }
            else LL_INFOS("GhostStudio") << "AUTOANIM-CHECKPOINT ch=" << channel.name
                << " observed=" << saved.observed_s << " reason=" << reason << LL_ENDL;
            return saved;
        };
        result->mBody = capture(body);
        for (auto& pair : links) result->mAnimesh.emplace_back(pair.second.key, capture(pair.second));
        pool = result; published = result;
        // Repoint channel facts while retaining existing scheduler bindings until their handoff.
        body.pool = &pool->mBody;
        for (auto& pair : links) pair.second.pool = pool->findAnimesh(pair.second.key);
        if (seal)
        {
            learning = false; body.recorder.reset();
            for (auto& pair : links) pair.second.recorder.reset();
            LL_INFOS("GhostStudio") << "AUTOANIM-LEARN seal inst=" << config.mInstanceId << " rt=" << runtime
                << " window=" << config.mWindowSeconds << " observed=" << observed << " reason=" << reason << LL_ENDL;
        }
        return result;
    }
    void presence(Channel& channel, bool present, const char* absence, F64 now, const LLUUID& runtime)
    {
        static LLCachedControl<bool> recouple(gSavedSettings, "GhostAutoAnimateRecouple", true);
        if (present)
        {
            if (channel.presentSince < 0.0) channel.presentSince = now;
            if (channel.autonomy == AUTONOMY_AUTO_LOST && recouple &&
                now - channel.presentSince >= RECOUPLE_DEBOUNCE)
            {
                LL_INFOS("GhostStudio") << "AUTOANIM-RECOUPLE ch=" << channel.name << " absent="
                    << (channel.absentSince >= 0.0 ? now - channel.absentSince : 0.0) << LL_ENDL;
                channel.autonomy = AUTONOMY_NONE; channel.bound = false; channel.bindPending = false;
                channel.cold = false;
                channel.auditHandoff = false; // [AutoAnimate] Recouple permits the ensuing mirror stops.
                channel.scheduler.unbind();
                channel.baseline = true; channel.noPoolLogged = false;
                if (learning)
                    LL_INFOS("GhostStudio") << "AUTOANIM-LEARN resume inst=" << config.mInstanceId << " rt=" << runtime
                        << " window=" << config.mWindowSeconds << " observed=" << observed << " reason=window" << LL_ENDL;
            }
            if (channel.autonomy == AUTONOMY_NONE) { channel.absentSince = -1.0; channel.noPoolLogged = false; }
            return;
        }
        channel.presentSince = -1.0;
        if (channel.absentSince < 0.0) channel.absentSince = now;
        channel.absent = absence;
        if (channel.autonomy != AUTONOMY_NONE || channel.noPoolLogged ||
            now - channel.absentSince < autoAnimLossGrace()) return;
        snapshot(now, false, "loss", runtime);
        if (!channel.pool || channel.pool->entries.empty())
        {
            channel.noPoolLogged = true;
            LL_INFOS("GhostStudio") << "AUTOANIM-HANDOFF ch=" << channel.name
                << " kind=auto verdict=NOPOOL carried=" << channel.carried << " ours_stopped=0 clock="
                << (channel.name == "body" ? "body" : "cav") << " absent=" << absence << LL_ENDL;
            return;
        }
        channel.autonomy = AUTONOMY_AUTO_LOST; channel.bindPending = true; channel.kind = "auto";
    }
    void bind(Channel& channel, const AnimMap& current, F64 now, LLCharacter* character, const std::string& clockName)
    {
        if (!channel.bindPending) return;
        channel.bindPending = false;
        channel.adoption.clear(); channel.handoffTime = LLFrameTimer::getTotalSeconds(); channel.oursStopped = 0;
        // [AutoAnimate] Natural expiry is not evidence of a stop by our code.
        // Snapshot only motions actually playing at handoff, then audit stop requests.
        for (const auto& animation : current)
            if (character->isMotionActive(animation.first)) channel.adoption.insert(animation);
        channel.carried = static_cast<U32>(current.size());
        channel.auditHandoff = true; channel.clockName = clockName;
        const char* verdict = !channel.pool || channel.pool->entries.empty() ? "NOPOOL" :
            channel.cold ? "COLD" : current.empty() ? "EMPTY-ADOPT" : "ok";
        channel.bound = channel.pool && !channel.pool->entries.empty();
        if (channel.bound)
        {
            const std::string seedKey = channel.name == "body" ? channel.name : channel.name + "/" + channel.key.item.asString();
            channel.scheduler.bind(std::shared_ptr<const ChannelPool>(pool, channel.pool), config.mInstanceId, config.mSeedSalt, seedKey);
            channel.scheduler.adopt(current, now, facts_for);
            if (channel.cold) channel.scheduler.markCold();
            autoAnimPrefetch(channel.pool, character, channel.name); channel.fetchDue = LLFrameTimer::getTotalSeconds() + 60.0;
        }
        LL_INFOS("GhostStudio") << "AUTOANIM-HANDOFF ch=" << channel.name << " kind=" << channel.kind
            << " verdict=" << verdict << " carried=" << current.size() << " ours_stopped=0 clock=" << clockName
            << " absent=" << channel.absent << LL_ENDL;
        channel.cold = false;
    }
    void disarmAudit() // [AutoAnimate] User transitions permit stops on every channel.
    {
        body.auditHandoff = false;
        for (auto& pair : links) pair.second.auditHandoff = false;
    }
    void auditStop(Channel& channel, const LLUUID& id, LLCharacter& character)
    {
        if (!channel.auditHandoff) return;
        if (LLFrameTimer::getTotalSeconds() - channel.handoffTime > 2.0)
        { channel.auditHandoff = false; channel.adoption.clear(); return; }
        if (channel.adoption.count(id) && character.isMotionActive(id)) // [AutoAnimate] Check at the stop, not just adoption.
        {
            ++channel.oursStopped;
            LL_WARNS("GhostStudio") << "AUTOANIM-HANDOFF ch=" << channel.name << " kind=" << channel.kind
                << " verdict=POP carried=" << channel.carried << " ours_stopped=" << channel.oursStopped
                << " clock=" << channel.clockName << " absent=" << channel.absent << LL_ENDL;
        }
    }
};

void LLGhostAvatar::setEntityAutoAnimate(const ALGhostAutoAnim::Config& config)
{
    if (!config.mEnabled)
    {
        if (!mAutoAnim) return;
        mAutoAnim->disarmAudit(); // [AutoAnimate] Toggle-off permits the next mirror sync's stops.
        for (ClonedLinkset& linkset : mClonedLinksets) linkset.mAutonomy = AUTONOMY_NONE;
        mAutoAnim.reset(); return;
    }
    if (mAutoAnim)
    {
        if (config.mPool && config.mPool != mAutoAnim->config.mPool &&
            config.mPool != mAutoAnim->pool && config.mPool != mAutoAnim->incoming)
            mAutoAnim->incoming = config.mPool;
        mAutoAnim->config = config; return;
    }
    mAutoAnim.reset(new AutoAnimRuntime);
    AutoAnimRuntime& runtime = *mAutoAnim;
    runtime.config = config; runtime.pool = config.mPool;
    runtime.observed = runtime.pool ? runtime.pool->mObservedSeconds : 0.f;
    runtime.checkpoint = runtime.observed; runtime.lastTick = LLFrameTimer::getTotalSeconds();
    runtime.learning = !runtime.pool || !runtime.pool->mSealed;
    runtime.body.pool = runtime.pool ? &runtime.pool->mBody : nullptr;
    autoAnimRebindLinksets();
    if (runtime.learning)
    {
        runtime.startRecorder(runtime.body);
        LL_INFOS("GhostStudio") << "AUTOANIM-LEARN " << (runtime.pool ? "resume" : "start")
            << " inst=" << config.mInstanceId << " rt=" << getID() << " window=" << config.mWindowSeconds
            << " observed=" << runtime.observed << " reason=" << (runtime.pool ? "refresh" : "window") << LL_ENDL;
    }
    if (config.mManual)
    {
        runtime.actions.push_back(Action::GoAutonomous); runtime.body.cold = true;
        for (auto& pair : runtime.links) pair.second.cold = true;
    }
}
void LLGhostAvatar::autoAnimRebindLinksets()
{
    AutoAnimRuntime& runtime = *mAutoAnim;
    for (const ClonedLinkset& linkset : mClonedLinksets)
    {
        LLViewerObject* root = gObjectList.findObject(linkset.mRoot);
        if (!root || !root->isAnimatedObject() || runtime.links.size() >= 32) continue;
        auto& channel = runtime.links[linkset.mRoot];
        channel.key.point = linkset.mAttachPoint; channel.key.item = linkset.mSourceItemId;
        channel.key.prims = static_cast<U32>(1 + linkset.mChildren.size());
        for (const ClonedLinkset& preceding : mClonedLinksets)
        {
            if (preceding.mRoot == linkset.mRoot) break;
            if (preceding.mAttachPoint == linkset.mAttachPoint && preceding.mSourceItemId == linkset.mSourceItemId &&
                preceding.mChildren.size() == linkset.mChildren.size()) ++channel.key.ordinal;
        }
        channel.name = channel.key.text(); channel.primLast.resize(1 + linkset.mSourceChildren.size());
        bool weakMatch = false;
        channel.pool = runtime.pool ? runtime.pool->findAnimesh(channel.key, &weakMatch) : nullptr;
        if (weakMatch) LL_INFOS("GhostStudio") << "AUTOANIM-LOAD keyweak key=" << channel.name << LL_ENDL;
        if (runtime.learning) runtime.startRecorder(channel);
    }
}
F32 LLGhostAvatar::getAutoAnimWindow() const
{
    return mAutoAnim ? mAutoAnim->config.mWindowSeconds : 0.f;
}
void LLGhostAvatar::requestAutoAnimAction(ALGhostAutoAnim::Action action, bool cold)
{
    if (!mAutoAnim) return;
    // [AutoAnimate] TRUE_MIRROR's manual transition belongs only to this pending action.
    if (action == Action::GoAutonomous && cold) mAutoAnim->body.cold = true;
    if (action == Action::Relearn) mAutoAnim->published.reset();
    mAutoAnim->actions.push_back(action);
}
bool LLGhostAvatar::takeAutoAnimPublishedPool(std::shared_ptr<const ALGhostAutoAnim::Pool>& pool)
{
    if (!mAutoAnim || !mAutoAnim->published) return false;
    pool = std::move(mAutoAnim->published); return true;
}
std::shared_ptr<const ALGhostAutoAnim::Pool> LLGhostAvatar::exportAutoAnimProgress()
{
    if (!mAutoAnim) return {};
    if (std::find(mAutoAnim->actions.begin(), mAutoAnim->actions.end(), Action::Relearn) != mAutoAnim->actions.end()) return {};
    return mAutoAnim->snapshot(LLFrameTimer::getTotalSeconds(), false, "refresh", getID());
}
void LLGhostAvatar::autoAnimRecordTick()
{
    AutoAnimRuntime& runtime = *mAutoAnim;
    const F64 now = LLFrameTimer::getTotalSeconds();
    const F32 dt = static_cast<F32>(std::max(0.0, now - runtime.lastTick)); runtime.lastTick = now;
    auto expireAudit = [](AutoAnimRuntime::Channel& channel)
    {
        if (channel.auditHandoff && LLFrameTimer::getTotalSeconds() - channel.handoffTime > 2.0)
        { channel.auditHandoff = false; channel.adoption.clear(); }
    };
    expireAudit(runtime.body); for (auto& pair : runtime.links) expireAudit(pair.second);
    LLViewerObject* object = gObjectList.findObject(mAnimationSourceId);
    LLVOAvatar* source = object ? object->asAvatar() : nullptr;
    if (runtime.simloss) source = nullptr;
    const std::string gate = autoAnimGated(source);
    if (runtime.gated != !gate.empty())
    {
        U32 discarded = 0;
        runtime.gated = !gate.empty();
        if (runtime.gated)
        {
            runtime.gateReason = gate;
            auto gateChannel = [&](AutoAnimRuntime::Channel& channel)
            {
                if (channel.recorder) discarded += channel.recorder->onGateEnter(now);
                channel.pending.clear(); channel.paused = true; channel.baseline = true;
            };
            gateChannel(runtime.body); for (auto& pair : runtime.links) gateChannel(pair.second);
        }
        else runtime.settleUntil = now + 1.0;
        LL_INFOS("GhostStudio") << "AUTOANIM-GATE " << (runtime.gated ? "on" : "off")
            << " reason=" << runtime.gateReason << " discarded=" << discarded << LL_ENDL;
    }
    if (runtime.incoming)
    {
        runtime.pool = std::move(runtime.incoming); runtime.published = runtime.pool;
        runtime.learning = false; runtime.observed = runtime.pool->mObservedSeconds;
        auto load = [&](AutoAnimRuntime::Channel& channel, const ChannelPool* pool)
        {
            channel.recorder.reset(); channel.pending.clear(); channel.pool = pool;
            if (channel.autonomy != AUTONOMY_NONE) { channel.bindPending = true; channel.kind = "load"; }
        };
        load(runtime.body, &runtime.pool->mBody);
        for (auto& pair : runtime.links)
        {
            auto& channel = pair.second; bool weakMatch = false;
            load(channel, runtime.pool->findAnimesh(channel.key, &weakMatch));
            if (weakMatch) LL_INFOS("GhostStudio") << "AUTOANIM-LOAD keyweak key=" << channel.name << LL_ENDL;
        }
        for (const auto& saved : runtime.pool->mAnimesh)
            if (std::none_of(runtime.links.begin(), runtime.links.end(), [&saved](const auto& pair) { return pair.second.key.matches(saved.first); }))
                LL_INFOS("GhostStudio") << "AUTOANIM-LOAD skip key=" << saved.first.text() << LL_ENDL;
    }
    for (Action action : runtime.actions)
    {
        if (action == Action::SimLossToggle)
        {
            runtime.simloss = !runtime.simloss;
            LL_INFOS("GhostStudio") << "AUTOANIM-SIMLOSS " << (runtime.simloss ? "on" : "off") << " rt=" << getID() << LL_ENDL;
            continue;
        }
        if (action == Action::StopLearning || action == Action::GoAutonomous)
            runtime.snapshot(now, true, action == Action::GoAutonomous ? "manual" : "stopnow", getID());
        if (action == Action::Relearn)
        {
            runtime.pool.reset(); runtime.published.reset(); runtime.observed = 0.f; runtime.checkpoint = 0.f;
            runtime.learning = true;
            LL_INFOS("GhostStudio") << "AUTOANIM-LEARN start inst=" << runtime.config.mInstanceId << " rt=" << getID()
                << " window=" << runtime.config.mWindowSeconds << " observed=0 reason=window" << LL_ENDL;
        }
        auto act = [&](AutoAnimRuntime::Channel& channel)
        {
            if (action == Action::FollowSource || action == Action::Relearn)
            {
                channel.autonomy = AUTONOMY_NONE; channel.bound = false; channel.bindPending = false;
                channel.cold = false;
                channel.auditHandoff = false; // [AutoAnimate] Follow/Relearn permit the ensuing mirror stops.
                channel.scheduler.unbind();
                channel.absentSince = -1.0; channel.presentSince = -1.0; channel.noPoolLogged = false;
                if (action == Action::Relearn) { channel.pool = nullptr; runtime.startRecorder(channel); }
            }
            else if (action == Action::GoAutonomous)
            { channel.autonomy = AUTONOMY_MANUAL; channel.bindPending = true; channel.kind = "manual"; }
            else if (action == Action::Reseed && channel.autonomy != AUTONOMY_NONE)
            { channel.bindPending = true; channel.kind = "manual"; }
        };
        act(runtime.body); for (auto& pair : runtime.links) act(pair.second);
    }
    runtime.actions.clear();
    if (!runtime.learning) return;
    AutoAnimCostScope cost(sAutoAnimRecordMicros);
    const bool mirror = mEntityDriveMode == ALGhostStudio::DRIVE_MIRROR || mEntityDriveMode == ALGhostStudio::DRIVE_TRUE_MIRROR;
    const bool present = mirror && source && !source->isDead();
    const bool settling = now < runtime.settleUntil;
    if (present && !source->mSignaledAnimations.empty() && !runtime.gated && !settling && runtime.body.autonomy == AUTONOMY_NONE)
        runtime.observed = std::min(3600.f, runtime.observed + dt);
    static const AnimMap empty;
    // Empty signals must still produce stop events during grace (gap-style AOs).
    runtime.record(runtime.body, present ? source->mSignaledAnimations : empty, now,
        settling || !present || source->mSignaledAnimations.empty() ? 0.f : dt, present, settling);
    const auto& objects = LLObjectSignaledAnimationMap::instance().getMap();
    for (const ClonedLinkset& linkset : mClonedLinksets)
    {
        auto found = runtime.links.find(linkset.mRoot); if (found == runtime.links.end()) continue;
        auto& channel = found->second;
        auto& changedIds = channel.changedIds;
        auto read = [&](const LLUUID& id, size_t index)
        {
            const auto signal = objects.find(id);
            const AnimMap& map = signal == objects.end() ? empty : signal->second;
            AnimMap& previous = channel.primLast[index];
            if (previous == map) return;
            for (const auto& old : previous)
            {
                const auto current = map.find(old.first);
                if (current == map.end())
                {
                    auto count = channel.refcounts.find(old.first);
                    if (count != channel.refcounts.end() && count->second) --count->second;
                    changedIds.insert(old.first);
                }
                else if (old.second != current->second) changedIds.insert(old.first);
            }
            for (const auto& current : map)
                if (!previous.count(current.first)) { ++channel.refcounts[current.first]; changedIds.insert(current.first); }
            previous = map;
        };
        read(linkset.mSourceRoot, 0);
        for (size_t i = 0; i < linkset.mSourceChildren.size(); ++i) read(linkset.mSourceChildren[i], i + 1);
        for (const LLUUID& id : changedIds)
        {
            const auto count = channel.refcounts.find(id);
            if (count == channel.refcounts.end() || !count->second)
            { channel.merged.erase(id); channel.refcounts.erase(id); continue; }
            bool first = true;
            S32 sequence = 0;
            for (const AnimMap& prim : channel.primLast)
            {
                const auto signal = prim.find(id);
                if (signal != prim.end()) { sequence = first ? signal->second : std::max(sequence, signal->second); first = false; }
            }
            channel.merged[id] = sequence;
        }
        changedIds.clear();
        LLViewerObject* root = gObjectList.findObject(linkset.mSourceRoot);
        runtime.record(channel, channel.merged, now, settling || channel.merged.empty() ? 0.f : dt,
            present && root && !root->isDead(), settling);
    }
    if (runtime.observed >= runtime.config.mWindowSeconds) runtime.snapshot(now, true, "window", getID());
    else if (runtime.observed - runtime.checkpoint >= 30.f)
    {
        runtime.snapshot(now, false, "checkpoint", getID()); runtime.checkpoint = runtime.observed;
    }
}
void LLGhostAvatar::autoAnimBodyPresence(LLVOAvatar* source)
{
    bool nonempty = false;
    if (source && !source->isDead())
        for (const auto& signal : source->mSignaledAnimations)
            if (signal.first != ANIM_AGENT_SIT_GROUND && signal.first != ANIM_AGENT_SIT_GROUND_CONSTRAINED) { nonempty = true; break; }
    const bool present = source && !source->isDead() && (nonempty || *autoAnimGated(source));
    const char* absence = !source ? "null" : source->isDead() ? "dead" : "empty";
    mAutoAnim->body.carried = static_cast<U32>(mCloneDesiredAnimations.size());
    mAutoAnim->presence(mAutoAnim->body, present, absence, LLFrameTimer::getTotalSeconds(), getID());
}
void LLGhostAvatar::autoAnimBodyStep()
{
    AutoAnimCostScope cost(sAutoAnimScheduleMicros);
    auto& channel = mAutoAnim->body;
    const F64 now = getMotionController().getAnimTime();
    mAutoAnim->bind(channel, mCloneDesiredAnimations, now, this, "body");
    if (!channel.bound) return;
    if (LLFrameTimer::getTotalSeconds() >= channel.fetchDue)
    { autoAnimPrefetch(channel.pool, this, channel.name); channel.fetchDue = LLFrameTimer::getTotalSeconds() + 60.0; }
    const bool changed = channel.scheduler.tick(now, autoAnimResident, channel.removed, &channel.retired);
    // [AutoAnimate] Retired shots leave the audit ledger before any explicit or implicit stop.
    for (const LLUUID& id : channel.retired) channel.adoption.erase(id);
    channel.cold = false;
    if (changed)
    {
        for (const LLUUID& id : channel.removed)
        {
            if (channel.adoption.count(id)) mAutoAnim->auditStop(channel, id, *this); // [AutoAnimate] Excludes retirements.
            LLCharacter::stopMotion(id, false);
            mClonePlayingAnimations.erase(id); mCloneDesiredAnimations.erase(id);
        }
        AnimMap desired = channel.scheduler.desired();
        // [AutoAnimate] HEAD clears the ledger on a COLD body exit. Animations
        // running at handoff but absent from the pool are dropped at COLD;
        // they cannot be reissued from a learned pick. Live foreign ids stay held.
        for (auto it = desired.begin(); it != desired.end();)
        {
            if (!mCloneDesiredAnimations.count(it->first) &&
                (!channel.scheduler.owns(it->first) || !LLKeyframeDataCache::getKeyframeData(it->first))) it = desired.erase(it);
            else ++it;
        }
        autoAnimSyncBody(desired);
    }
}
bool LLGhostAvatar::autoAnimLinksetStep(ClonedLinkset& linkset)
{
    auto found = mAutoAnim->links.find(linkset.mRoot);
    if (found == mAutoAnim->links.end()) return false;
    auto& channel = found->second;
    const auto& objects = LLObjectSignaledAnimationMap::instance().getMap();
    LLViewerObject* sourceRoot = gObjectList.findObject(linkset.mSourceRoot);
    bool signal = false;
    auto nonempty = [&](const LLUUID& id)
    { const auto entry = objects.find(id); return entry != objects.end() && !entry->second.empty(); };
    signal = nonempty(linkset.mSourceRoot);
    for (const LLUUID& id : linkset.mSourceChildren) signal = signal || nonempty(id);
    const bool gone = mAutoAnim->simloss || !sourceRoot || sourceRoot->isDead();
    if ((gone || !signal) && channel.autonomy == AUTONOMY_NONE && !channel.noPoolLogged &&
        channel.absentSince >= 0.0 && LLFrameTimer::getTotalSeconds() - channel.absentSince >=
        autoAnimLossGrace())
    {
        AnimMap current;
        auto count = [&](const LLUUID& id)
        { const auto entry = objects.find(id); if (entry != objects.end()) autoAnimMerge(current, entry->second); };
        count(linkset.mRoot); for (const LLUUID& id : linkset.mChildren) count(id);
        channel.carried = static_cast<U32>(current.size());
    }
    mAutoAnim->presence(channel, !mAutoAnim->simloss && (mAutoAnim->gated || (!gone && signal)), gone ? "root" : "signal",
        LLFrameTimer::getTotalSeconds(), getID());
    linkset.mAutonomy = channel.autonomy;
    if (channel.autonomy == AUTONOMY_NONE) return false;
    AutoAnimCostScope cost(sAutoAnimScheduleMicros);
    LLViewerObject* root = gObjectList.findObject(linkset.mRoot);
    LLControlAvatar* control = root ? root->getControlAvatar() : nullptr;
    if (!control || control->isDead()) return true;
    if (channel.clock.sourceId() != control->getID())
    {
        LL_INFOS("GhostStudio") << "AUTOANIM-CLOCK rebase ch=" << channel.name << " cav=" << control->getID().asString().substr(0, 8) << LL_ENDL;
        autoAnimPrefetch(channel.pool, control, channel.name);
    }
    const F64 now = channel.clock.sample(control->getMotionController().getAnimTime(), control->getID());
    if (channel.bindPending)
    {
        AnimMap current;
        auto read = [&](const LLUUID& id) { const auto entry = objects.find(id); if (entry != objects.end()) autoAnimMerge(current, entry->second); };
        read(linkset.mRoot); for (const LLUUID& id : linkset.mChildren) read(id);
        mAutoAnim->bind(channel, current, now, control, "cav:" + control->getID().asString().substr(0, 8));
    }
    if (!channel.bound) return true;
    if (LLFrameTimer::getTotalSeconds() >= channel.fetchDue)
    { autoAnimPrefetch(channel.pool, control, channel.name); channel.fetchDue = LLFrameTimer::getTotalSeconds() + 60.0; }
    const bool changed = channel.scheduler.tick(now, autoAnimResident, channel.removed, &channel.retired);
    // [AutoAnimate] Also excludes retirements from the later animesh loop's sync audit.
    for (const LLUUID& id : channel.retired) channel.adoption.erase(id);
    channel.cold = false;
    if (changed)
    {
        applyAnimeshDesired(linkset, channel.scheduler.desired());
    }
    return true;
}
void LLGhostAvatar::applyAnimeshDesired(const ClonedLinkset& linkset, const ALGhostAutoAnim::AnimMap& desired)
{
    auto& objects = LLObjectSignaledAnimationMap::instance().getMap();
    AnimMap current;
    auto read = [&](const LLUUID& id) { const auto entry = objects.find(id); if (entry != objects.end()) autoAnimMerge(current, entry->second); };
    read(linkset.mRoot); for (const LLUUID& id : linkset.mChildren) read(id);
    bool changed = false;
    auto erase = [&](const LLUUID& prim, const LLUUID& id)
    { auto found = objects.find(prim); if (found != objects.end()) changed = found->second.erase(id) != 0 || changed; };
    for (const auto& animation : current)
        if (!desired.count(animation.first))
        { erase(linkset.mRoot, animation.first); for (const LLUUID& child : linkset.mChildren) erase(child, animation.first); }
    for (const auto& animation : desired)
    {
        auto old = current.find(animation.first);
        if (old == current.end() || old->second != animation.second)
        {
            const auto channel = mAutoAnim->links.find(linkset.mRoot);
            if (channel == mAutoAnim->links.end() || !channel->second.scheduler.owns(animation.first) ||
                !LLKeyframeDataCache::getKeyframeData(animation.first)) continue;
            for (const LLUUID& child : linkset.mChildren) erase(child, animation.first);
            objects[linkset.mRoot][animation.first] = animation.second; changed = true;
        }
    }
    LLViewerObject* root = gObjectList.findObject(linkset.mRoot);
    if (changed && root && !root->isDead())
    {
        autoAnimAuditLinksetSync(linkset); // [AutoAnimate] Includes implicit stock sync stops.
        root->updateControlAvatar();
    }
}
// [AutoAnimate] Audit at callers so the protected ledger/sync implementations stay unchanged.
void LLGhostAvatar::autoAnimSyncBody(const ALGhostAutoAnim::AnimMap& desired)
{
    if (mAutoAnim && mAutoAnim->body.auditHandoff)
        for (const auto& playing : mClonePlayingAnimations)
            if (!desired.count(playing.first) && mAutoAnim->body.adoption.count(playing.first))
                mAutoAnim->auditStop(mAutoAnim->body, playing.first, *this); // [AutoAnimate]
    synchronizeCloneAnimations(desired);
}
void LLGhostAvatar::autoAnimAuditBodyStop(const LLUUID& id)
{
    if (mAutoAnim) mAutoAnim->auditStop(mAutoAnim->body, id, *this); // [AutoAnimate]
}
void LLGhostAvatar::autoAnimAuditLinksetSync(const ClonedLinkset& linkset, bool clearing)
{
    if (!mAutoAnim) return;
    const auto found = mAutoAnim->links.find(linkset.mRoot);
    if (found == mAutoAnim->links.end() || !found->second.auditHandoff) return;
    LLViewerObject* root = gObjectList.findObject(linkset.mRoot);
    LLControlAvatar* control = root ? root->getControlAvatar() : nullptr;
    if (!control || control->isDead()) return;
    AnimMap desired;
    if (!clearing)
    {
        const auto& objects = LLObjectSignaledAnimationMap::instance().getMap();
        auto read = [&](const LLUUID& prim)
        { const auto entry = objects.find(prim); if (entry != objects.end()) autoAnimMerge(desired, entry->second); };
        read(linkset.mRoot); for (const LLUUID& child : linkset.mChildren) read(child);
    }
    // Stock processAnimationStateChanges stops exactly these playing ledger ids.
    for (const auto& playing : control->mPlayingAnimations)
        if (!desired.count(playing.first) && found->second.adoption.count(playing.first))
            mAutoAnim->auditStop(found->second, playing.first, *control); // [AutoAnimate]
}
void LLGhostAvatar::restartEntityAnimationAudited()
{
    if (mAutoAnim)
    {
        if (mEntityDriveMode == ALGhostStudio::DRIVE_DIRECTED && mEntityDirectedAnim.notNull())
            autoAnimAuditBodyStop(mEntityDirectedAnim);
        else if (mEntityDriveMode == ALGhostStudio::DRIVE_MIRROR)
            for (const auto& playing : mClonePlayingAnimations)
                if (mCloneDesiredAnimations.count(playing.first)) autoAnimAuditBodyStop(playing.first);
    }
    restartEntityAnimation();
}
void LLGhostAvatar::autoAnimOnDriveModeChanged(S32 old_mode, S32 mode)
{
    if (!mAutoAnim || old_mode == mode) return;
    auto cold = [&](AutoAnimRuntime::Channel& channel, bool affected)
    {
        if (!affected) return;
        // Dormant modes do not observe continuous presence/absence edges.
        channel.absentSince = -1.0; channel.presentSince = -1.0;
        if (channel.autonomy == AUTONOMY_NONE) return;
        channel.scheduler.markCold(); channel.cold = true;
        // Preserve the post-switch desired set, including any foreign ids.
        channel.fetchDue = 0.0;
        LL_INFOS("GhostStudio") << "AUTOANIM-HANDOFF ch=" << channel.name
            << " kind=" << (channel.autonomy == AUTONOMY_MANUAL ? "manual" : "auto")
            << " verdict=COLD carried=" << channel.scheduler.desired().size() << " ours_stopped=0 clock="
            << (channel.name == "body" ? "body" : "cav") << " absent=mode" << LL_ENDL;
    };
    cold(mAutoAnim->body, old_mode == ALGhostStudio::DRIVE_MIRROR || mode == ALGhostStudio::DRIVE_MIRROR);
    for (auto& pair : mAutoAnim->links) cold(pair.second,
        mode == ALGhostStudio::DRIVE_DIRECTED || old_mode == ALGhostStudio::DRIVE_DIRECTED ||
        mode == ALGhostStudio::DRIVE_FROZEN || old_mode == ALGhostStudio::DRIVE_FROZEN);
}
void LLGhostAvatar::getAutoAnimStatus(ALGhostAutoAnim::LearnStatus& status) const
{
    status = LearnStatus(); if (!mAutoAnim) return;
    const auto& runtime = *mAutoAnim;
    status.mOn = true; status.mLearning = runtime.learning; status.mGated = runtime.gated;
    status.mObserved = runtime.observed; status.mWindow = runtime.config.mWindowSeconds;
    status.mBodyAutonomous = runtime.body.autonomy != AUTONOMY_NONE;
    status.mManual = runtime.body.autonomy == AUTONOMY_MANUAL;
    // [AutoAnimate] Counting is cheap; format channel details only on a change.
    static const std::string noGate;
    const std::string& gate = runtime.gated ? runtime.gateReason : noGate;
    bool detailChanged = runtime.statusMode != mEntityDriveMode || runtime.statusGate != gate ||
        runtime.statusChannels != runtime.links.size();
    auto count = [&](const AutoAnimRuntime::Channel& channel, bool body)
    {
        if (channel.recorder) status.mLiveEntries += channel.recorder->entryCount();
        if (channel.pool)
        {
            status.mThin = status.mThin || channel.pool->thin;
            if (body) for (const Entry& entry : channel.pool->entries) { if (entry.state) ++status.mBodyStates; else ++status.mBodyOneShots; }
        }
        const bool thin = channel.pool && channel.pool->thin;
        detailChanged = detailChanged || channel.detailAutonomy != channel.autonomy || channel.detailThin != thin;
        channel.detailAutonomy = channel.autonomy; channel.detailThin = thin;
    };
    count(runtime.body, true);
    for (const auto& pair : runtime.links)
    {
        ++status.mAnimeshChannels;
        if (pair.second.autonomy != AUTONOMY_NONE) ++status.mAnimeshAutonomous;
        status.mManual = status.mManual || pair.second.autonomy == AUTONOMY_MANUAL;
        count(pair.second, false);
    }
    if (detailChanged)
    {
        std::ostringstream detail;
        auto append = [&](const AutoAnimRuntime::Channel& channel)
        {
            detail << channel.name << ": " << (channel.autonomy == AUTONOMY_MANUAL ? "manual" :
                channel.autonomy == AUTONOMY_AUTO_LOST ? "automatic" : "source");
            if (channel.detailThin) detail << " THIN: AO timer probably longer than window";
            detail << '\n';
        };
        append(runtime.body); for (const auto& pair : runtime.links) append(pair.second);
        if (runtime.gated) detail << "Paused: source " << runtime.gateReason << '\n';
        if (mEntityDriveMode == ALGhostStudio::DRIVE_DIRECTED) detail << "(dormant: Directed)";
        if (mEntityDriveMode == ALGhostStudio::DRIVE_FROZEN) detail << "(dormant: Frozen)";
        runtime.statusDetail = detail.str(); runtime.statusMode = mEntityDriveMode;
        runtime.statusGate = gate; runtime.statusChannels = runtime.links.size();
    }
    status.mDetail = runtime.statusDetail;
}
void LLGhostAvatar::countAutoAnimActivity(U32& learners, U32& players) const
{
    if (!mAutoAnim) return;
    if (mAutoAnim->learning) ++learners;
    if (mAutoAnim->body.autonomy != AUTONOMY_NONE ||
        std::any_of(mAutoAnim->links.begin(), mAutoAnim->links.end(), [](const auto& pair)
            { return pair.second.autonomy != AUTONOMY_NONE; })) ++players;
}
void LLGhostAvatar::logAutoAnimCost(U32 learners, U32 players)
{
    static F64 last = LLFrameTimer::getTotalSeconds();
    const F64 now = LLFrameTimer::getTotalSeconds();
    if (now - last < 60.0) return;
    if ((learners || players) && gSavedSettings.getBOOL("GhostAutoAnimateCostLog"))
        LL_INFOS("GhostStudio") << "AUTOANIM-COST rec=" << sAutoAnimRecordMicros / (now - last)
            << " sched=" << sAutoAnimScheduleMicros / (now - last) << " learners=" << learners << " players=" << players << LL_ENDL;
    last = now; sAutoAnimRecordMicros = 0.0; sAutoAnimScheduleMicros = 0.0;
}

LLGhostAvatar::LLGhostAvatar(const LLUUID& id, const LLPCode pcode, LLViewerRegion* regionp) :
    LLVOAvatar(id, pcode, regionp),
    mMarkedForDeath(false),
    mEntityCloneVisible(true)
{
    // A ghost renders through the REAL scene avatar path -- that is the whole
    // point (deferred lighting, shadows, fog, tonemap, ReShade). Unlike
    // LLControlAvatar / LLUIAvatar it must NOT be a dummy. mIsDummy defaults to
    // false (LLAvatarAppearance), but keep it explicit so the intent is local.
    mIsDummy = false;
    mIsGhostAvatar = true;
    mIsLocalOnly = true;
    setClientOuterTransform(new LLClientOuterTransform);

    // The default motion controller would overwrite any pose we place on this
    // skeleton, and a clone's "animation" is driven externally. Same reasoning
    // as LLControlAvatar (llcontrolavatar.cpp:57).
    mEnableDefaultMotions = false;

    // Undo the base ctor's saved render-policy lookup: a ghost's id is
    // synthetic, so any saved-visual-mute hit would be a STRANGER's setting.
    // Use the LOCAL (non-persisting) setter -- setVisualMuteSettings() would
    // write the synthetic id back into LLRenderMuteList. (Control/UI avatars
    // keep the base lookup, exactly as they always did.)
    setVisualMuteSettingsLocal(LLVOAvatar::AV_RENDER_NORMALLY);
}

// virtual
LLGhostAvatar::~LLGhostAvatar()
{
}

// virtual
void LLGhostAvatar::initInstance()
{
    LLVOAvatar::initInstance();

    // Mirrors LLUIAvatar::initInstance(): a client-only avatar is never sent an
    // ObjectUpdate, so it must build its own drawable and be placed by fiat.
    createDrawable(&gPipeline);

    // DELIBERATELY NOT LLUIAvatar's setPositionAgent(zero) + slamPosition().
    // LLVOAvatar::slamPosition() begins with gAgent.setPositionAgent()
    // (llvoavatar.cpp:4038) -- it is the SELF avatar's teleport helper. Calling
    // it here would drag the REAL agent to the ghost on every spawn and every
    // move. ghostSlamPosition() is that function minus its first line.
    //
    // Also note setPositionAgent() dereferences getRegion() unguarded
    // (llviewerobject.cpp), so placement only happens with a live region.
    if (getRegion())
    {
        ghostSlamPosition(getPositionAgent());
    }

    updateJointLODs();
    updateGeometry(mDrawable);

    mInitFlags |= 1 << 3;
}

void LLGhostAvatar::ghostSlamPosition(const LLVector3& pos_agent)
{
    // Set the REAL object position, not just the render root. LLActorMover's
    // applyOverride() paints mRoot only (llactormover.cpp:1894) and explicitly
    // leaves the drawable position alone, which is tolerable for a puppeteered
    // real avatar but not here: a ghost's culling, pixel area, LOD, impostor
    // selection and picking all evaluate off the drawable. If those disagree
    // with where it is drawn, it flickers out at the wrong distances.
    setPositionAgent(pos_agent);

    // The remainder is LLVOAvatar::slamPosition() (llvoavatar.cpp:4036) with
    // the gAgent teleport omitted.
    mRoot->setWorldPosition(pos_agent);
    setChanged(TRANSLATED);
    if (mDrawable.notNull())
    {
        gPipeline.updateMoveNormalAsync(mDrawable);
    }
    mRoot->updateWorldMatrixChildren();
}

void LLGhostAvatar::setGhostPosition(const LLVector3& pos_agent)
{
    // This API deliberately keeps its historical root-position contract.
    // A direct root placement supersedes any Studio foot lock.
    mDesiredGhostFootValid = false;
    if (!getRegion())
    {
        LL_WARNS("Avatar") << "setGhostPosition with no region; ignoring" << LL_ENDL;
        return;
    }
    ghostSlamPosition(pos_agent);
}

void LLGhostAvatar::setGhostFootPosition(const LLVector3& foot_agent)
{
    if (!foot_agent.isFinite())
    {
        LL_WARNS("Avatar")
            << "setGhostFootPosition with a non-finite foot; ignoring"
            << LL_ENDL;
        return;
    }
    const LLVector3d desired_global =
        gAgent.getPosGlobalFromAgent(foot_agent);
    if (!desired_global.isFinite())
    {
        LL_WARNS("Avatar")
            << "setGhostFootPosition produced a non-finite global foot; ignoring"
            << LL_ENDL;
        return;
    }
    mDesiredGhostFootGlobal = desired_global;
    mDesiredGhostFootValid = true;
    applyDesiredGhostFootPosition();
    updateEntityOuterTransform();
}

void LLGhostAvatar::applyDesiredGhostFootPosition()
{
    if (!mDesiredGhostFootValid || !getRegion() || !mRoot)
    {
        return;
    }

    F32 pelvis_to_foot = getPelvisToFoot();
    if (!llfinite(pelvis_to_foot))
    {
        pelvis_to_foot = 0.f;
    }
    LLVector3 root_agent =
        gAgent.getPosAgentFromGlobal(mDesiredGhostFootGlobal);
    root_agent.mV[VZ] += llmax(0.f, pelvis_to_foot);
    if (!root_agent.isFinite())
    {
        return;
    }

    // Newly cloned appearance data can change pelvis-to-foot over several
    // frames. Only slam when that derived root actually changes so the
    // persistent foot lock does not enqueue redundant drawable moves forever.
    if (dist_vec_squared(getPositionAgent(), root_agent) <=
            F_APPROXIMATELY_ZERO &&
        dist_vec_squared(mRoot->getWorldPosition(), root_agent) <=
            F_APPROXIMATELY_ZERO)
    {
        return;
    }
    ghostSlamPosition(root_agent);
}

void LLGhostAvatar::syncGhostObjectToMovingRoot()
{
    // Actor Mover owns the skeleton root while it drives this ghost, but
    // applyOverride() writes the ROOT JOINT only (llactormover.cpp:1929) --
    // deliberately for a real puppeteered avatar, whose sim-true object
    // position must stay untouched. A ghost has NO sim-true position: its
    // culling, pixel-area LOD, picking, nametag anchor and render-position
    // queries all evaluate off the viewer-object/drawable (getRenderPosition()
    // returns mDrawable's position for a root avatar). Left at the authored
    // foot, a ghost walked tens of metres away is mis-culled and mis-LODed.
    // Chase the CURRENT moving root with the same object + drawable +
    // child-matrix contract as the foot lock (ghostSlamPosition), but WITHOUT
    // touching the stored authored foot (mDesiredGhostFoot*): the walk is
    // transient, and stopping it must still return the clone to its authored
    // Studio placement via the ordinary foot lock.
    if (!getRegion() || !mRoot)
    {
        return;
    }
    const LLVector3 root_agent = mRoot->getWorldPosition();
    if (!root_agent.isFinite())
    {
        return;
    }
    // Same redundant-move guard as the foot lock above: a mover HOLD (placeAt
    // pin, dwell, arrival settle) repaints the same root every frame -- do not
    // enqueue drawable moves forever while effectively stationary. Compared
    // against the OBJECT position, so slow walks self-limit: drift accumulates
    // to at most ~sqrt(F_APPROXIMATELY_ZERO) (~3 mm) before a sync fires.
    if (dist_vec_squared(getPositionAgent(), root_agent) <=
            F_APPROXIMATELY_ZERO)
    {
        return;
    }
    // Reuses ghostSlamPosition wholesale: the root-joint re-set is idempotent
    // (applyOverride just wrote the same world position) and the trailing
    // updateWorldMatrixChildren() re-propagates attachments after the
    // post-motion re-assert, exactly like the authored slam path.
    ghostSlamPosition(root_agent);
}

void LLGhostAvatar::setGhostRotation(const LLQuaternion& rotation)
{
    if (!mRoot)
    {
        return;
    }
    // Viewer-local synthetic avatar: do not use any object-update helper.
    setRotation(rotation, false);
    mRoot->setWorldRotation(rotation);
    mRoot->updateWorldMatrixChildren();
    setChanged(ROTATED);
    if (mDrawable.notNull())
    {
        gPipeline.updateMoveNormalAsync(mDrawable);
    }
}

void LLGhostAvatar::updateEntityOuterTransform()
{
    LLClientOuterTransform* outer = getClientOuterTransform();
    if (!outer || !mRoot)
    {
        return;
    }

    LLVector3 foot = mRoot->getWorldPosition();
    // Same hazard as the overlay pivot (drawGeometryGhost): a non-standard or
    // still-loading skeleton can report a negative or non-finite pelvis-to-foot.
    // Used raw it skews the scale pivot (clone scales down through the ground) and
    // can bake NaN into the outer matrix. Clamp to the feet-below-pelvis
    // convention and bail on a non-finite placement so the driver never sees a
    // degenerate transform.
    F32 p2f = getPelvisToFoot();
    if (!llfinite(p2f)) p2f = 0.f;
    foot.mV[VZ] -= llmax(0.f, p2f);   // no upper cap: a giant custom skeleton is legit
    if (!foot.isFinite())
    {
        return;
    }
    if (outer->mEnabled &&
        is_approx_equal(outer->mScale, mEntityScale) &&
        dist_vec_squared(outer->mFootPivot, foot) < F_APPROXIMATELY_ZERO)
    {
        return;
    }

    outer->mScale = mEntityScale;
    outer->mFootPivot = foot;
    outer->mEnabled = true;
    outer->mCurrent.setIdentity();
    outer->mInverse.setIdentity();
    for (S32 axis = VX; axis <= VZ; ++axis)
    {
        outer->mCurrent.mMatrix[axis][axis] = mEntityScale;
        outer->mCurrent.mMatrix[VW][axis] =
            (1.f - mEntityScale) * foot.mV[axis];
        const F32 inv_scale = 1.f / mEntityScale;
        outer->mInverse.mMatrix[axis][axis] = inv_scale;
        outer->mInverse.mMatrix[VW][axis] =
            (1.f - inv_scale) * foot.mV[axis];
    }
    ++outer->mRevision;
    setNeedsExtentUpdate(true);
    LLRenderPass::invalidateModelMatrixCache();

    // setNeedsExtentUpdate above recomputes the BODY box this frame, but the worn
    // attachments live in separate spatial bridges that are NOT otherwise told the
    // outer scale changed -- their scaled culling box (LLSpatialBridge::updateSpatialExtents)
    // is only rebuilt when the bridge is incidentally marked moved (animation / motion /
    // octree churn). A single large TYPED scale jump then leaves the bridge's cull box
    // stale/mispositioned while the geometry already renders scaled, so the attachments
    // get culled and the whole clone vanishes; a gradual drag churns the bridge enough to
    // converge. Force the attachment drawables + their bridges to re-extent this frame so
    // a typed jump behaves like a drag. Only reached on a real scale/pivot change (guarded
    // above), so there is no per-frame cost for a static clone.
    for (const auto& ap : mAttachmentPoints)
    {
        LLViewerJointAttachment* attachment = ap.second;
        if (!attachment)
        {
            continue;
        }
        for (LLViewerObject* obj : attachment->mAttachedObjects)
        {
            if (!obj || obj->isDead() || obj->mDrawable.isNull())
            {
                continue;
            }
            gPipeline.markMoved(obj->mDrawable, false);
            if (LLSpatialBridge* bridge = obj->mDrawable->getSpatialBridge())
            {
                gPipeline.markMoved(bridge, false);
            }
        }
    }
}

void LLGhostAvatar::stampEntityOuterTransform(LLViewerObject* object)
{
    if (!object || object->isDead())
    {
        return;
    }
    object->setClientOuterTransform(getClientOuterTransform());
    for (LLViewerObject* child : object->getChildren())
    {
        stampEntityOuterTransform(child);
    }
}

void LLGhostAvatar::setEntityScale(F32 scale)
{
    mEntityScale = llclamp(scale, GHOST_SCALE_MIN, GHOST_SCALE_MAX);
    updateEntityOuterTransform();
}

void LLGhostAvatar::neutralizeEntityPhysicsParams()
{
    // Physics writes visual params, not pose rotations, so stopping the motion
    // does not undo its last morph. These are the eight driven physics params
    // in avatar_lad.xml, all centered on their default weight.
    static const S32 physics_driven_param_ids[] =
    {
        1200, 1201, 1202, 1203, 1204, 1205, 1206, 1207
    };
    for (S32 id : physics_driven_param_ids)
    {
        if (LLVisualParam* param = getVisualParam(id))
        {
            setVisualParamWeight(param, param->getDefaultWeight());
        }
    }
    updateVisualParams();
}

void LLGhostAvatar::neutralizeEntityEyeParams()
{
    // LLEyeMotion::onDeactivate restores joints but not a blink caught between
    // frames. Reopen both lids explicitly before relinquishing the motion.
    setVisualParamWeight("Blink_Left", 0.f);
    setVisualParamWeight("Blink_Right", 0.f);
    updateVisualParams();
}

void LLGhostAvatar::setEntityCloneVisible(bool visible)
{
    if (visible == mEntityCloneVisible)
    {
        return;
    }

    mEntityCloneVisible = visible;
    if (visible)
    {
        if (mEntityPhysicsEnabled &&
            !isMotionActive(ANIM_AGENT_PHYSICS_MOTION))
        {
            LLCharacter::startMotion(ANIM_AGENT_PHYSICS_MOTION);
        }
        if (mEntityEyeMotionEnabled && !isMotionActive(ANIM_AGENT_EYE))
        {
            LLCharacter::startMotion(ANIM_AGENT_EYE);
        }
    }
    else
    {
        autoAnimAuditBodyStop(ANIM_AGENT_PHYSICS_MOTION); // [AutoAnimate]
        LLCharacter::stopMotion(ANIM_AGENT_PHYSICS_MOTION, true);
        neutralizeEntityPhysicsParams();
        autoAnimAuditBodyStop(ANIM_AGENT_EYE); // [AutoAnimate]
        LLCharacter::stopMotion(ANIM_AGENT_EYE, true);
        neutralizeEntityEyeParams();
    }
}

void LLGhostAvatar::setEntityEyeMotionEnabled(bool enabled)
{
    if (enabled == mEntityEyeMotionEnabled)
    {
        if (enabled && mEntityCloneVisible &&
            !isMotionActive(ANIM_AGENT_EYE))
        {
            LLCharacter::startMotion(ANIM_AGENT_EYE);
        }
        return;
    }

    mEntityEyeMotionEnabled = enabled;
    if (enabled && mEntityCloneVisible)
    {
        LLCharacter::startMotion(ANIM_AGENT_EYE);
    }
    else
    {
        autoAnimAuditBodyStop(ANIM_AGENT_EYE); // [AutoAnimate]
        LLCharacter::stopMotion(ANIM_AGENT_EYE, true);
        neutralizeEntityEyeParams();
    }
}

void LLGhostAvatar::setEntityPhysicsEnabled(bool enabled)
{
    if (enabled == mEntityPhysicsEnabled)
    {
        // The member defaults on, but default motions are deliberately
        // disabled. Ensure a fresh clone actually starts its physics motion.
        if (enabled && mEntityCloneVisible &&
            !isMotionActive(ANIM_AGENT_PHYSICS_MOTION))
        {
            LLCharacter::startMotion(ANIM_AGENT_PHYSICS_MOTION);
        }
        return;
    }

    mEntityPhysicsEnabled = enabled;
    if (enabled && mEntityCloneVisible)
    {
        // Do not call startDefaultMotions(): head/eye/noise/breathe motions
        // would fight the clone-owned mirrored or directed animation.
        LLCharacter::startMotion(ANIM_AGENT_PHYSICS_MOTION);
    }
    else
    {
        autoAnimAuditBodyStop(ANIM_AGENT_PHYSICS_MOTION); // [AutoAnimate]
        LLCharacter::stopMotion(ANIM_AGENT_PHYSICS_MOTION, true);
        if (!enabled)
        {
            neutralizeEntityPhysicsParams();
        }
    }
}

void LLGhostAvatar::setEntityLook(S32 look, F32 alpha)
{
    // Clone-scoped render-style hook.  The enum/state is intentionally kept on
    // the synthetic avatar so future shader/material passes never consult a
    // global setting and therefore cannot restyle real avatars.
    mEntityLook = look;
    mEntityLookAlpha = llclamp(alpha, 0.f, 1.f);
}

void LLGhostAvatar::clearClonedObjectAnimations()
{
    // [AnimeshRepeat] Nothing latched survives an erase of every clone entry.
    resetHeldAnimeshRepeat();
    object_signaled_animation_map_t& object_anims =
        LLObjectSignaledAnimationMap::instance().getMap();
    for (const ClonedLinkset& linkset : mClonedLinksets)
    {
        bool changed = object_anims.erase(linkset.mRoot) != 0;
        for (const LLUUID& child_id : linkset.mChildren)
        {
            changed = object_anims.erase(child_id) != 0 || changed;
        }
        LLViewerObject* root = gObjectList.findObject(linkset.mRoot);
        if (changed && root && !root->isDead() && root->isAnimatedObject())
        {
            root->updateControlAvatar();
        }
    }
}

void LLGhostAvatar::synchronizeCloneAnimations(
    const signaled_animation_map_t& desired_animations)
{
    // This is intentionally narrower than LLVOAvatar's animation-state
    // machinery: it performs a pure ledger diff and can only start/stop
    // motions in this synthetic avatar's own controller.  It has no agent,
    // simulator, sound, notification, sit, AO, or avatar-state branches.
    // Start/stop semantics mirror LLVOAvatar::processAnimationStateChanges():
    // an animation is stopped ONLY when the source stops signaling it.  A new
    // sequence id for a still-signaled animation (AOs routinely re-assert their
    // override every few seconds) must not stop it: stopMotion(immediate) then
    // startMotion re-activates the motion at t=0, replaying a sit/lie intro
    // forever ("stand, sit, stand...") while the source -- whose startMotion on
    // an already-active motion keeps playing -- simply holds the pose.
    for (auto playing = mClonePlayingAnimations.begin();
         playing != mClonePlayingAnimations.end();)
    {
        if (desired_animations.find(playing->first) == desired_animations.end())
        {
            LLCharacter::stopMotion(playing->first, true);
            playing = mClonePlayingAnimations.erase(playing);
        }
        else
        {
            ++playing;
        }
    }

    for (const auto& desired : desired_animations)
    {
        // Signaled but not playing, or a new sequence id: (re)request exactly
        // like the stock path.  startMotion() does not rewind an active motion;
        // on a finished one-shot it replays it -- the same thing the source does.
        const auto playing = mClonePlayingAnimations.find(desired.first);
        if (playing == mClonePlayingAnimations.end() ||
            playing->second != desired.second)
        {
            if (LLCharacter::startMotion(desired.first))
            {
                mClonePlayingAnimations[desired.first] = desired.second;
            }
        }
    }
    mCloneDesiredAnimations = desired_animations;
}

void LLGhostAvatar::setEntityDriveMode(S32 mode, const LLUUID& directed_anim)
{
    mode = llclamp(mode, (S32)ALGhostStudio::DRIVE_MIRROR,
                         (S32)ALGhostStudio::DRIVE_TRUE_MIRROR);
    if (mode == mEntityDriveMode && directed_anim == mEntityDirectedAnim)
    {
        return;
    }

    // [AutoAnimate] Disarm before this transition issues any body or linkset stops.
    // Keep the existing post-transition COLD handling in autoAnimOnDriveModeChanged.
    if (mAutoAnim) mAutoAnim->disarmAudit();

    // [AnimeshRepeat] A drive-mode change re-derives every linkset's hold
    // state from scratch on the next idleUpdate(); stale replay records must
    // not carry a pending/seen flag across it.
    resetHeldAnimeshRepeat();

    // FROZEN and TRUE_MIRROR both hold the wearer's controller paused
    // (TRUE_MIRROR never pauses the animesh control avatars, so that vector
    // is simply empty for it).
    if (mEntityDriveMode == ALGhostStudio::DRIVE_FROZEN ||
        mEntityDriveMode == ALGhostStudio::DRIVE_TRUE_MIRROR)
    {
        mEntityPauseRequest = nullptr;
        mEntityControlPauseRequests.clear();
    }
    if (mEntityDriveMode == ALGhostStudio::DRIVE_DIRECTED &&
        mEntityDirectedAnim.notNull())
    {
        autoAnimAuditBodyStop(mEntityDirectedAnim); // [AutoAnimate]
        LLCharacter::stopMotion(mEntityDirectedAnim, true);
    }
    if (mEntityDriveMode == ALGhostStudio::DRIVE_MIRROR &&
        mode != ALGhostStudio::DRIVE_MIRROR)
    {
        autoAnimSyncBody(signaled_animation_map_t()); // [AutoAnimate] Audit implicit stops.
    }

    const S32 old_mode = mEntityDriveMode; // [AutoAnimate]
    mEntityDriveMode = mode;
    mEntityDirectedAnim = directed_anim;
    mEntityDirectedWasActive = false;
    if (mode == ALGhostStudio::DRIVE_DIRECTED)
    {
        autoAnimSyncBody(signaled_animation_map_t()); // [AutoAnimate] Audit implicit stops.
        if (mAutoAnim) for (const ClonedLinkset& linkset : mClonedLinksets)
            autoAnimAuditLinksetSync(linkset, true); // [AutoAnimate] Audit the clear before it syncs.
        clearClonedObjectAnimations();
    }
    if (mode == ALGhostStudio::DRIVE_DIRECTED && directed_anim.notNull())
    {
        LLCharacter::startMotion(directed_anim);
        mEntityDirectedStarted = true;
    }
    else if (mode == ALGhostStudio::DRIVE_FROZEN)
    {
        mEntityPauseRequest = requestPause();
        for (const ClonedLinkset& linkset : mClonedLinksets)
        {
            LLVOVolume* root =
                dynamic_cast<LLVOVolume*>(gObjectList.findObject(linkset.mRoot));
            LLControlAvatar* control = root ? root->getControlAvatar() : nullptr;
            if (control && !control->isDead())
            {
                mEntityControlPauseRequests.push_back(control->requestPause());
            }
        }
    }
    else if (mode == ALGhostStudio::DRIVE_TRUE_MIRROR)
    {
        // [TrueMirror] The post-object-list stamp must be the ONLY writer of
        // this clone's pose. Suppress every competing driver on the clone:
        //  - its own motion blend: pause the wearer controller (the ledger was
        //    emptied above when leaving MIRROR; physics stays "active" but a
        //    paused controller never runs it -- the source's physics-driven
        //    morphs are stamped instead when clone physics is on);
        //  - Pose Polish: LLVOAvatar::updateCharacter resets it while paused;
        //  - its own eye/blink motion: stop it (blink lids come from the
        //    source's Blink_* params, eye joints from the source skeleton);
        //  - Actor Mover root override + gaze layer: LLActorMover checks
        //    isTrueMirrorDriven() and returns early;
        //  - Ghost Studio keep-facing / turn-to / crowd facing: skipped in
        //    ALGhostStudio via trueMirrorOwnsFacing().
        // Animesh attachments are NOT paused: they keep the ObjectAnimation
        // ledger mirror (see idleUpdate), since their control avatars are
        // separate skeletons the stamp does not cover.
        setEntityEyeMotionEnabled(false);
        mEntityPauseRequest = requestPause();
        mTrueMirrorSourceValid = false;
        mTrueMirrorLastStampFrame = 0;
    }
    autoAnimOnDriveModeChanged(old_mode, mode); // [AutoAnimate]
}

bool LLGhostAvatar::isTrueMirrorDriven() const
{
    return mEntityDriveMode == ALGhostStudio::DRIVE_TRUE_MIRROR;
}

void LLGhostAvatar::applyTrueMirrorStamp()
{
    if (mEntityDriveMode != ALGhostStudio::DRIVE_TRUE_MIRROR ||
        isDead() || !isBuilt() || !mRoot)
    {
        return;
    }
    // Once per frame, whatever the caller.
    const U32 frame = LLFrameTimer::getFrameCount();
    if (mTrueMirrorLastStampFrame == frame)
    {
        return;
    }
    mTrueMirrorLastStampFrame = frame;

    // Resolve the source through gObjectList every frame (never retained).
    // A ghost source is allowed: its final pose is evaluated in the object
    // list like any avatar (a TRUE_MIRROR ghost source is itself stamped in
    // the same post-pass, in Studio instance order, so a chain may lag by one
    // frame per link). Fallback for a missing / dead / unbuilt source or a
    // self-reference is HOLD: nothing is written, and the paused controller
    // keeps the last stamped pose until the source is back.
    LLViewerObject* source_obj = gObjectList.findObject(mAnimationSourceId);
    LLVOAvatar* source = source_obj ? source_obj->asAvatar() : nullptr;
    const bool source_ok = source && source != this && !source->isDead() &&
                           source->isBuilt() && source->getRootJoint();
    if (source_ok != mTrueMirrorSourceValid)
    {
        mTrueMirrorSourceValid = source_ok;
        LL_INFOS("GhostStudio") << "TRUE mirror clone " << getID()
            << (source_ok ? " stamping from source "
                          : " holding last pose; source unavailable ")
            << mAnimationSourceId << LL_ENDL;
    }
    if (!source_ok)
    {
        return;
    }

    static LLCachedControl<bool> copy_facing(
        gSavedSettings, "GhostTrueMirrorCopyFacing", true);
    static LLCachedControl<bool> copy_face(
        gSavedSettings, "GhostTrueMirrorCopyFace", true);

    stampTrueMirrorSkeleton(source, copy_facing);
    if (copy_face)
    {
        stampTrueMirrorFace(source);
    }

    // Same trailing contract as updateCharacter(): re-propagate the world
    // matrices now so everything downstream this frame (attachment drawable
    // moves, rigged matrix palettes, system-mesh reskin via mNeedsSkin --
    // already set by the clone's own updateCharacter this frame) reads the
    // stamped pose rather than a dirty-flagged stale one.
    mRoot->updateWorldMatrixChildren();
}

void LLGhostAvatar::stampTrueMirrorSkeleton(LLVOAvatar* source, bool copy_facing)
{
    // Root: mRoot is NOT in mSkeleton (mSkeleton[0] is mPelvis, parented
    // under mRoot -- LLAvatarAppearance::buildSkeleton). Its local rotation IS
    // its world rotation (no parent), so copying it mirrors the source's body
    // facing, including any procedural body turn written to the root. Its
    // POSITION is deliberately left alone: the clone keeps its own placement
    // (Studio foot lock) and mirrors the pose where it stands.
    if (copy_facing)
    {
        const LLQuaternion root_rot = source->getRootJoint()->getRotation();
        if (root_rot.isFinite())
        {
            mRoot->setRotation(root_rot);
        }
    }

    // Skeleton joints: both avatars are built from the same avatar_skeleton.xml
    // so the lists are index-parallel; the name check is a cheap guard against
    // a mismatched/partial skeleton, with a by-name fallback. Rotation always;
    // position too, so animated/overridden joint offsets (pelvis height from
    // the anim, mesh joint position overrides, Bento translations) mirror as
    // well. Scale is NOT copied: it is the clone's own shape/appearance.
    // setPosition(pos, false) writes mXform directly, bypassing the clone's
    // attachment overrides -- intended: the source's final (already
    // override-resolved) position is what we want to reproduce.
    const avatar_joint_list_t& src_skel = source->getSkeleton();
    const avatar_joint_list_t& dst_skel = getSkeleton();
    const size_t count = llmin(src_skel.size(), dst_skel.size());
    for (size_t i = 0; i < count; ++i)
    {
        LLJoint* src = src_skel[i];
        LLJoint* dst = dst_skel[i];
        if (!src || !dst)
        {
            continue;
        }
        if (src->getName() != dst->getName())
        {
            dst = getJoint(src->getName());
            if (!dst)
            {
                continue;
            }
        }
        dst->setRotation(src->getRotation());
        dst->setPosition(src->getPosition());
    }
}

void LLGhostAvatar::buildTrueMirrorParamSets()
{
    // Scoped deliberately: VISUAL_PARAM_GROUP_ANIMATABLE (group 1) covers
    // every driven shape sub-morph too (hundreds of params that are pure
    // appearance), so copying the whole group would be both expensive and
    // wrong if the clone's appearance snapshot ever diverges from the source.
    // The ANIMATED face state is exactly: LLEmote expressions (Express_*),
    // LLEyeMotion blink lids (Blink_*), voice lipsync (Lipsync_*), plus the
    // eight LLPhysicsMotion driven morphs (ids 1200..1207, the same set
    // neutralizeEntityPhysicsParams() resets).
    mTrueMirrorFaceParamIds.clear();
    mTrueMirrorPhysicsParamIds.clear();
    for (LLVisualParam* param = getFirstVisualParam(); param;
         param = getNextVisualParam())
    {
        if (!param->getInfo())
        {
            continue;
        }
        const S32 id = param->getID();
        const std::string& name = param->getName();
        if (name.rfind("Express_", 0) == 0 ||
            name.rfind("Blink_", 0) == 0 ||
            name.rfind("Lipsync_", 0) == 0)
        {
            mTrueMirrorFaceParamIds.push_back(id);
        }
        else if (id >= 1200 && id <= 1207)
        {
            mTrueMirrorPhysicsParamIds.push_back(id);
        }
    }
    mTrueMirrorParamSetsBuilt = true;
}

void LLGhostAvatar::stampTrueMirrorFace(LLVOAvatar* source)
{
    if (!mTrueMirrorParamSetsBuilt)
    {
        buildTrueMirrorParamSets();
    }

    bool changed = false;
    auto copy_param = [this, source, &changed](S32 id)
    {
        LLVisualParam* dst = getVisualParam(id);
        LLVisualParam* src = source->getVisualParam(id);
        // Never fight an in-flight appearance animation on the clone.
        if (!dst || !src || dst->isAnimating())
        {
            return;
        }
        const F32 weight = src->getWeight();
        if (weight != dst->getWeight())
        {
            setVisualParamWeight(dst, weight);
            changed = true;
        }
    };
    for (S32 id : mTrueMirrorFaceParamIds)
    {
        copy_param(id);
    }
    // Clone physics OFF keeps its neutralized morphs (setEntityPhysicsEnabled);
    // ON mirrors the source's jiggle since the clone's own physics motion is
    // paused under TRUE_MIRROR.
    if (mEntityPhysicsEnabled)
    {
        for (S32 id : mTrueMirrorPhysicsParamIds)
        {
            copy_param(id);
        }
    }
    if (changed)
    {
        // Same apply path LLEmote / LLEyeMotion use each frame: applies only
        // the params whose effective weight changed, then dirtyMesh().
        updateVisualParams();
    }
}

void LLGhostAvatar::setEntityLoopMode(S32 mode)
{
    mEntityLoopMode = llclamp(mode, (S32)ALGhostStudio::LOOP_RETRIGGER,
                                   (S32)ALGhostStudio::LOOP_PLAY_ONCE);
}

void LLGhostAvatar::setEntityAnimTimeFactor(F32 factor)
{
    mEntityAnimTimeFactor = factor;
    // The clone's own (wearer) motion controller.
    LLCharacter::setAnimTimeFactor(factor);

    // Each animesh attachment is a SEPARATE LLControlAvatar with its own motion
    // controller, so the wearer's time factor never reaches it -- without this loop
    // only the wearer (and any mesh drawn through it) responds to the anim-speed
    // spinner and multi-animesh clones desync. Mirrors the pause traversal in
    // setEntityDriveMode (the pattern that already works). Dedup guards the unusual
    // shared/root-edit case.
    std::set<LLControlAvatar*> seen;
    for (const ClonedLinkset& linkset : mClonedLinksets)
    {
        LLVOVolume* root =
            dynamic_cast<LLVOVolume*>(gObjectList.findObject(linkset.mRoot));
        LLControlAvatar* control = root ? root->getControlAvatar() : nullptr;
        if (control && !control->isDead() && seen.insert(control).second)
        {
            control->setAnimTimeFactor(factor);
        }
    }
}

void LLGhostAvatar::restartEntityAnimation()
{
    if (mEntityDriveMode == ALGhostStudio::DRIVE_DIRECTED)
    {
        if (mEntityDirectedAnim.notNull())
        {
            LLCharacter::stopMotion(mEntityDirectedAnim, true);
            LLCharacter::startMotion(mEntityDirectedAnim, 0.f);
            mEntityDirectedStarted = true;
            mEntityDirectedWasActive = false;
        }
        return;
    }
    if (mEntityDriveMode != ALGhostStudio::DRIVE_MIRROR)
    {
        return;
    }

    // Only the clone-owned mirrored ledger is cut. The source avatar is
    // resolved nowhere on this path and its controller remains untouched.
    const signaled_animation_map_t desired = mCloneDesiredAnimations;
    for (const auto& playing : mClonePlayingAnimations)
    {
        if (desired.find(playing.first) != desired.end())
        {
            LLCharacter::stopMotion(playing.first, true);
        }
    }
    mClonePlayingAnimations.clear();
    synchronizeCloneAnimations(desired);
}

//static
bool LLGhostAvatar::isGhostId(const LLUUID& id)
{
    if (id.isNull())
    {
        return false;
    }
    LLViewerObject* obj = gObjectList.findObject(id);
    return obj && obj->asAvatar() && obj->asAvatar()->isGhostAvatar();
}

bool LLGhostAvatar::cloneAppearanceFrom(LLVOAvatar* source)
{
    if (!source)
    {
        return false;
    }
    if (!copyAppearanceFrom(source, true))
    {
        return false;
    }

    // A synthetic ghost never receives AvatarAnimation messages addressed to
    // its UUID. Remember the real avatar whose already-received animation
    // state we should mirror locally from idleUpdate().
    mAnimationSourceId = source->getID();
    return true;
}

// ---------------------------------------------------------------------------
// Attachment duplication
//
// The appearance message carries shape + baked textures and nothing else, so a
// clone built from it alone is a naked system body. A modern avatar IS its worn
// mesh, so we duplicate the source's attachment objects as CLIENT-ONLY copies.
//
// Adapted from LLLocalMeshMgr's client-only linkset recipe (lllocalmesh.cpp
// spawnLinkset/attachPreviewToAvatar), generalised from gAgentAvatarp to an
// arbitrary target avatar.
//
// ⚠️ SAFETY, DO NOT REORDER: `mIsLocalOnly` must be set the instant the object
// exists, BEFORE any parenting. LLViewerJointAttachment::addObject() takes a
// branch for NON-local objects that hunts for an existing attachment with the
// same inventory item id, KILLS it, and sends ObjectDetach TO THE SIMULATOR
// (llviewerjointattachment.cpp:183-199). Getting this wrong would strip the
// user's real worn items off them. Local-only objects skip that branch.
// ---------------------------------------------------------------------------

namespace
{
    LLUUID volume_mesh_id(const LLVOVolume* volume)
    {
        const LLVolume* decoded = volume ? volume->getVolume() : nullptr;
        return decoded ? decoded->getParams().getSculptID() : LLUUID::null;
    }

    std::string attachment_joint_name(LLVOVolume* volume)
    {
        if (!volume)
        {
            return "<missing>";
        }

        const S32 point =
            ATTACHMENT_ID_FROM_STATE(volume->getAttachmentState());
        LLVOAvatar* avatar = volume->getAvatarAncestor();
        if (avatar)
        {
            LLVOAvatar::attachment_map_t::const_iterator found =
                avatar->mAttachmentPoints.find(point);
            if (found != avatar->mAttachmentPoints.end() && found->second)
            {
                return found->second->getName();
            }
        }
        return llformat("%d", point);
    }

    bool has_rigged_face(const LLVOVolume* volume)
    {
        if (!volume || volume->mDrawable.isNull())
        {
            return false;
        }
        for (S32 face_index = 0;
             face_index < volume->mDrawable->getNumFaces(); ++face_index)
        {
            LLFace* face = volume->mDrawable->getFace(face_index);
            if (face && face->isState(LLFace::RIGGED))
            {
                return true;
            }
        }
        return false;
    }

    void log_ghost_rig(LLVOVolume* clone, LLVOVolume* source)
    {
        LLVOAvatar* source_avatar =
            source ? source->getAvatarAncestor() : nullptr;
        LL_INFOS("GhostStudio")
            << "GHOSTRIG clone=" << (clone ? clone->getID() : LLUUID::null)
            << " src=" << (source ? source->getID() : LLUUID::null)
            << " phase=copy_pre_cav"
            << " clone_isRoot=" << (clone && clone->isRootEdit())
            << " clone_root="
            << (clone ? clone->getRootEdit()->getID() : LLUUID::null)
            << " clone_isMesh=" << (clone && clone->isMesh())
            << " clone_hasSkin=" << (clone && clone->getSkinInfo() != nullptr)
            << " clone_isRigged=" << (clone && clone->isRiggedMesh())
            << " clone_isAnimesh=" << (clone && clone->isAnimatedObject())
            << " clone_hasCav="
            << (clone && clone->getControlAvatar() != nullptr)
            << " src_isMesh=" << (source && source->isMesh())
            << " src_hasSkin=" << (source && source->getSkinInfo() != nullptr)
            << " src_isAnimesh=" << (source && source->isAnimatedObject())
            << " src_is_ghost="
            << (source_avatar && source_avatar->isGhostAvatar())
            << " clone_mesh_uuid=" << volume_mesh_id(clone)
            << " src_mesh_uuid=" << volume_mesh_id(source)
            << " attach_joint=" << attachment_joint_name(clone)
            << LL_ENDL;
    }

    // Mirror the simulator-delivered texture-animation object state onto a
    // client-only clone. LLViewerTextureAnim registers itself with the global
    // per-frame updater, so copying these semantic fields is enough for the
    // viewer to animate the clone without a script or an ObjectUpdate.
    void copy_texture_animation(LLVOVolume* dst, const LLVOVolume* src)
    {
        if (src->mTextureAnimp)
        {
            if (!dst->mTextureAnimp)
            {
                dst->mTextureAnimp = new LLViewerTextureAnim(dst);
            }
            else
            {
                dst->mTextureAnimp->reset();
            }

            dst->mTextureAnimp->mMode = src->mTextureAnimp->mMode;
            dst->mTextureAnimp->mFace = src->mTextureAnimp->mFace;
            dst->mTextureAnimp->mSizeX = src->mTextureAnimp->mSizeX;
            dst->mTextureAnimp->mSizeY = src->mTextureAnimp->mSizeY;
            dst->mTextureAnimp->mStart = src->mTextureAnimp->mStart;
            dst->mTextureAnimp->mLength = src->mTextureAnimp->mLength;
            dst->mTextureAnimp->mRate = src->mTextureAnimp->mRate;
            dst->mTexAnimMode = 0;
            return;
        }

        // Match LLVOVolume's absent-TextureAnim-block path. This matters when
        // an existing destination is re-copied after the source animation was
        // stopped: retaining either the animator or a face texture matrix
        // would leave stale UV animation/render state on the clone.
        if (!dst->mTextureAnimp)
        {
            return;
        }

        delete dst->mTextureAnimp;
        dst->mTextureAnimp = nullptr;
        dst->mTexAnimMode = 0;

        if (dst->mDrawable)
        {
            for (S32 i = 0; i < dst->mDrawable->getNumFaces(); ++i)
            {
                LLFace* face = dst->mDrawable->getFace(i);
                if (face && face->mTextureMatrix)
                {
                    delete face->mTextureMatrix;
                    face->mTextureMatrix = nullptr;
                }
            }
            gPipeline.markTextured(dst->mDrawable);
        }
        dst->faceMappingChanged();
    }

    // Duplicate one prim's renderable state onto a fresh client-only volume.
    // Returns false if the source has no volume yet (asset still loading) --
    // there is nothing faithful to copy in that case.
    bool copy_prim_state(LLVOVolume* dst, LLVOVolume* src)
    {
        const LLVolume* src_vol = src->getVolume();
        if (!src_vol)
        {
            LL_WARNS("GhostStudio") << "copy_prim_state: source " << src->getID()
                                    << " has no volume yet; skipping" << LL_ENDL;
            return false;
        }

        dst->setScale(src->getScale(), false);

        // A rigged mesh is recognised as a mesh ONLY through its PARAMS_SCULPT
        // extra-param block (the mesh UUID + LL_SCULPT_TYPE_MESH), which the sim
        // normally delivers in the ObjectUpdate. A client-only clone never
        // receives that message, so copy the source's block here. Without it
        // LLVOVolume::isSculpted() is false and setVolume() below SKIPS the
        // entire skin/rig acquisition path (llvovolume.cpp ~1167), leaving
        // mSkinInfo null -- so every rigged piece draws as a STATIC prim at its
        // attachment joint (the "exploded", untextured clone). This must run
        // BEFORE setVolume so the skin path fires on the first call.
        //
        // Safe for a client-only object: setParameterEntry -> parameterChanged
        // only sends an ObjectExtraParams message when (local_origin &&
        // !isLocalOnly()) (llviewerobject.cpp:6578). We pass local_origin=false
        // AND the clone is mIsLocalOnly, so nothing is ever sent to the sim.
        const LLVolumeParams& src_params = src_vol->getParams();
        const bool source_is_mesh =
            (src_params.getSculptType() & LL_SCULPT_TYPE_MASK) ==
                LL_SCULPT_TYPE_MESH;
        if (source_is_mesh)
        {
            // The decoded volume is the state we are about to clone.  Derive
            // its mesh marker from those same params rather than conditionally
            // trusting a second accessor: if the source extra-param is
            // temporarily unavailable during cache/update churn, silently
            // omitting it is permanent for a client-only object because no
            // later ObjectUpdate will repair the clone.
            LLSculptParams mesh_marker;
            mesh_marker.setSculptTexture(src_params.getSculptID(),
                                         src_params.getSculptType());
            dst->setParameterEntry(LLNetworkData::PARAMS_SCULPT,
                                   mesh_marker, false);
        }
        else if (const LLSculptParams* src_sculpt = src->getSculptParams())
        {
            dst->setParameterEntry(LLNetworkData::PARAMS_SCULPT, *src_sculpt, false);
        }

        // Animated-object identity is another simulator-delivered extra param.
        // Copy it before geometry is assigned so isAnimatedObject() and the
        // eventual control-avatar decision see the source's real state.
        if (const LLExtendedMeshParams* src_ext = src->getExtendedMeshParams())
        {
            dst->setParameterEntry(LLNetworkData::PARAMS_EXTENDED_MESH, *src_ext, false);
        }

        // Same sculpt/mesh UUID => the mesh repository hands back the SAME
        // asset (LLVolume + skin info) from cache: no re-download, no re-decode.
        // The drawable and a real LOD must exist first or setVolume builds a
        // placeholder cube instead (lllocalmesh.cpp:1559).
        dst->setVolume(src_params, LLVolumeLODGroup::NUM_LODS - 1);

        // setVolume normally finds this same repository-owned skin through the
        // mesh cache.  Do not leave correctness dependent on that side effect:
        // a source that is already drawing rigged has the exact immutable skin
        // descriptor this clone needs.  notifySkinInfoLoaded() is the normal
        // repository callback, and because the linkset is already attached it
        // also schedules geometry plus attachment-joint overrides against THIS
        // ghost.  If the source skin is still loading, setVolume registered the
        // clone for the ordinary asynchronous callback instead.
        if (const LLMeshSkinInfo* src_skin = src->getSkinInfo())
        {
            if (src_skin->mMeshID != src_params.getSculptID())
            {
                LL_WARNS("GhostStudio")
                    << "copy_prim_state: source " << src->getID()
                    << " has skin " << src_skin->mMeshID
                    << " but volume mesh " << src_params.getSculptID()
                    << "; refusing an incoherent clone" << LL_ENDL;
                return false;
            }
            if (dst->getSkinInfo() != src_skin)
            {
                dst->notifySkinInfoLoaded(src_skin);
            }
            if (dst->getSkinInfo() != src_skin)
            {
                LL_WARNS("GhostStudio")
                    << "copy_prim_state: clone " << dst->getID()
                    << " did not retain source skin " << src_skin->mMeshID
                    << LL_ENDL;
                return false;
            }
        }

        static LLCachedControl<bool> use_source_resolved_materials(
            gSavedSettings,
            "GhostUnifiedSourceResolvedMaterials",
            false);
        if (use_source_resolved_materials)
        {
            const ALGhostResolvedMaterialObject material_snapshot =
                ALGhostMaterialResolver::capture(src);
            if (!ALGhostMaterialResolver::apply(dst, material_snapshot))
            {
                return false;
            }
            copy_texture_animation(dst, src);
            dst->markForUpdate();
            return true;
        }

        const U8 num_tes = src->getNumTEs();
        S32 source_pbr_faces = 0;
        LLUUID first_source_material;
        U8 first_source_material_te = 0;
        for (U8 i = 0; i < num_tes; ++i)
        {
            const LLTextureEntry* src_te = src->getTE(i);
            if (!src_te)
            {
                continue;
            }
            // Bulk semantic copy: diffuse id+colour, alpha/fullbright/glow/
            // bump/shiny, UV transform, legacy Blinn-Phong material params and
            // the base GLTF material reference. Deliberately NOT setTETexture/
            // setTEColor -- those only touch the legacy DIFFUSE channel, and a
            // PBR face keeps its colour on the material's BASE COLOUR slot.
            dst->setTE(i, *src_te);

            const LLUUID& source_material = src->getRenderMaterialID(i);
            if (source_material.notNull())
            {
                if (first_source_material.isNull())
                {
                    first_source_material = source_material;
                    first_source_material_te = i;
                }
                ++source_pbr_faces;
            }

            // Keep the source's already-resolved diffuse binding. This is
            // especially important for baked magic ids: resolving those
            // against a just-created ghost can transiently produce IMG_DEFAULT
            // until its baked state is established. changeTEImage changes only
            // the viewer-side binding and deliberately preserves the semantic
            // TE id (and therefore its UV transform/material identity).
            if (LLViewerTexture* src_image = src->getTEImage(i))
            {
                dst->changeTEImage(i, src_image);
            }
        }

        // Per-face PBR asset ids are stored in PARAMS_RENDER_MATERIAL, not in
        // LLTextureEntry. Copying the live source block both preserves every
        // TE-to-material mapping and marks the clone's block in use.
        // parameterChanged() then calls setRenderMaterialIDs(), which resolves
        // each base asset through gGLTFMaterialList and installs it on the TE.
        //
        // Calling setRenderMaterialID() on a clone with no live block is not
        // sufficient: createNewParameterEntry() initializes the block's
        // in-use flag to false, so getRenderMaterialParams() and
        // getRenderMaterialID() cannot see it afterward.
        //
        // This is viewer-local: local_origin=false suppresses ObjectExtraParams,
        // each material bind uses update_server=false, and mIsLocalOnly is an
        // additional guard against any simulator send.
        if (const LLRenderMaterialParams* src_render_params =
                src->getRenderMaterialParams())
        {
            dst->setParameterEntry(LLNetworkData::PARAMS_RENDER_MATERIAL,
                                   *src_render_params,
                                   /*local_origin=*/false);
        }
        LL_INFOS("GhostStudio")
            << "GHOSTMATCOPY src_obj=" << src->getID()
            << " clone_obj=" << dst->getID()
            << " render_params=" << (src->getRenderMaterialParams() != nullptr)
            << " pbr_faces=" << source_pbr_faces
            << " first_material=" << first_source_material
            << " clone_first_material="
            << (first_source_material.notNull()
                    ? dst->getRenderMaterialID(first_source_material_te)
                    : LLUUID::null)
            << LL_ENDL;

        // GLTF overrides are a separate layer. Apply deep copies only after
        // the base material ids above have been bound.
        for (U8 i = 0; i < num_tes; ++i)
        {
            const LLTextureEntry* src_te = src->getTE(i);
            if (!src_te)
            {
                continue;
            }
            if (const LLGLTFMaterial* ov = src_te->getGLTFMaterialOverride())
            {
                LLPointer<LLGLTFMaterial> ovp = new LLGLTFMaterial(*ov);
                dst->setTEGLTFMaterialOverride(i, ovp);
            }
        }

        copy_texture_animation(dst, src);
        dst->markForUpdate();
        return true;
    }

    // Allocate the viewer object ONLY -- no drawable, no geometry.
    //
    // ⚠️ THE DRAWABLE IS DELIBERATELY NOT CREATED HERE. That was the bug behind
    // the in-world "exploded" clones (2026-07-21). A drawable created while the
    // prim is still an object-tree ROOT, with attachment state already set, is
    // eligible for its OWN LLAvatarBridge (lldrawable.cpp:1204/1247). Doing that
    // per child meant a 248-child linkset spawned 248 independent avatar bridges,
    // each positioning its prim on its own. A real linkset child instead inherits
    // its parent's spatial partition (lldrawable.cpp:1262), and REBUILD_ALL
    // cannot repair topology -- it rebuilds geometry, not object/drawable trees.
    //
    // So: build the whole object hierarchy FIRST, then create drawables in
    // hierarchy order (see cloneAttachmentsFrom).
    LLVOVolume* alloc_local_copy(LLViewerRegion* region, U8 attach_state)
    {
        LLViewerObject* obj = gObjectList.createObjectViewer(LL_PCODE_VOLUME, region);
        LLVOVolume* dst = dynamic_cast<LLVOVolume*>(obj);
        if (!dst)
        {
            if (obj)
            {
                obj->markDead();
            }
            return nullptr;
        }

        // FIRST. See the safety note above -- without this,
        // LLViewerJointAttachment::addObject() can send ObjectDetach to the sim.
        dst->mIsLocalOnly = true;
        dst->mbCanSelect  = false;

        // Before any geometry build: a face is classified RIGGED only when skin
        // info exists AND isAttachment() is true (llvovolume.cpp:6072), and
        // isAttachment() is literally mAttachmentState != 0 (:4015).
        dst->setAttachmentState(attach_state);
        return dst;
    }

    // Decisive attach verification. The point is to make the log state a
    // VERDICT rather than leave us interpreting geometry.
    //
    // NOTE: drawable->getParent() is NOT the joint test for an attachment root.
    // setupDrawable() reparents the drawable's mXform directly
    // (llviewerjointattachment.cpp:115) and never sets LLDrawable::mParent, so
    // a correctly attached root may legitimately have a null drawable parent.
    // Test mXform.getParent() against the attachment joint's xform.
    // Returns false if ANY structural check failed, so the caller can record it
    // and stop /ghostverify from ever printing PASS on this ghost.
    bool log_attach_verify(LLVOAvatar* ghost,
                           LLVOVolume* dst_root,
                           LLVOVolume* src_root,
                           const std::vector<LLVOVolume*>& dst_children)
    {
        LLViewerJointAttachment* target = ghost->getTargetAttachmentPoint(dst_root);
        const bool listed = target &&
            std::find(target->mAttachedObjects.begin(),
                      target->mAttachedObjects.end(),
                      dst_root) != target->mAttachedObjects.end();

        // getParent() returns the base LLXform*; the joint's getXform() is an
        // LLXformMatrix* which upcasts to it for the comparison.
        LLXform* xform_parent = dst_root->mDrawable.notNull()
            ? dst_root->mDrawable->mXform.getParent() : nullptr;
        LLXform* expect_xform = target ? target->getXform() : nullptr;
        LLSpatialBridge* bridge = dst_root->mDrawable.notNull()
            ? dst_root->mDrawable->getSpatialBridge() : nullptr;
        const bool control_bridge =
            dynamic_cast<LLControlAVBridge*>(bridge) != nullptr;

        const bool joint_ok  = (xform_parent && xform_parent == expect_xform);
        // An attached animated object intentionally skins to its private
        // LLControlAvatar. Its attachment owner is still the ghost, exposed by
        // getAvatarAncestor(); getAvatar() is deliberately the wrong API for
        // this structural test.
        const bool avatar_ok = (dst_root->getAvatarAncestor() == ghost);
        const bool attach_ok = dst_root->isAttachment() && listed && avatar_ok && joint_ok;

        LL_INFOS("GhostStudio")
            << "ATTACH-VERIFY src=" << src_root->getID()
            << " state=" << (S32)dst_root->getAttachmentState()
            << " isAttachment=" << dst_root->isAttachment()
            << " avatar_is_ghost=" << avatar_ok
            << " listed=" << listed
            << " JOINT_PARENT_OK=" << joint_ok
            << " root_bridge=" << (void*)bridge
            << " control_bridge=" << control_bridge
            << " animated=" << dst_root->isAnimatedObject()
            << " cav=" << (dst_root->getControlAvatar() != nullptr)
            << " children=" << dst_children.size()
            << LL_ENDL;

        // TOPOLOGY. The defect that produced the exploded clones was a CHILD
        // owning its own LLAvatarBridge.
        //
        // NOTE the test is "child owns ANY bridge", NOT "child bridge differs
        // from the root's". getSpatialBridge() returns the drawable's OWN
        // mSpatialBridge (lldrawable.h:199) -- a CORRECT child owns none while
        // the root owns the LLAvatarBridge, so a !=-comparison against the root
        // is what CORRECT looks like and would have flagged a working clone as
        // broken. Compare resolved PARTITIONS for the inheritance check instead.
        // PASSIVE checks only -- getSpatialPartition() validates and CORRECTS
        // topology (lldrawable.cpp:1204), so using it here would let the check
        // repair the defect it is meant to catch.
        S32 kids_with_own_bridge = 0, kids_bad_parent = 0;
        for (LLVOVolume* c : dst_children)
        {
            if (c->mDrawable.isNull())
            {
                continue;
            }
            if (c->mDrawable->getSpatialBridge() != nullptr)
            {
                kids_with_own_bridge++;
            }
            if (c->mDrawable->getParent() != dst_root->mDrawable)
            {
                kids_bad_parent++;
            }
        }
        const bool topology_ok = (kids_with_own_bridge == 0 && kids_bad_parent == 0);
        if (!topology_ok)
        {
            LL_WARNS("GhostStudio")
                << "ATTACH-TOPOLOGY src=" << src_root->getID()
                << " children_with_own_bridge=" << kids_with_own_bridge
                << " children_bad_drawable_parent=" << kids_bad_parent
                << " (THIS IS THE EXPLODED-CLONE DEFECT)" << LL_ENDL;
        }
        if (!attach_ok)
        {
            LL_WARNS("GhostStudio")
                << "ATTACH-VERIFY src=" << src_root->getID()
                << " FAILED structural checks -- attachment registration or joint "
                   "parenting did not take" << LL_ENDL;
        }
        return attach_ok && topology_ok;
    }

    // Force a full geometry rebuild so any face built before the object was
    // parented to the ghost gets re-classified against its final state.
    void force_rebuild(LLViewerObject* obj)
    {
        if (!obj || obj->isDead())
        {
            return;
        }
        obj->markForUpdate();
        if (obj->mDrawable.notNull())
        {
            gPipeline.markRebuild(obj->mDrawable, LLDrawable::REBUILD_ALL);
        }
    }

    bool finalize_control_avatar(LLVOVolume* root,
                                 const std::vector<LLVOVolume*>& children)
    {
        if (!root || root->isDead())
        {
            return false;
        }

        bool contains_skin = root->getSkinInfo() != nullptr;
        for (LLVOVolume* child : children)
        {
            contains_skin =
                contains_skin || (child && child->getSkinInfo() != nullptr);
        }

        const bool animated = root->isAnimatedObject();

        // Geometry rebuilds normally make this decision, but this client-only
        // linkset has just changed from an ordinary attachment into an animated
        // object after it was attached. Decide synchronously now that every
        // prim's extended-mesh and skin state is present.
        root->updateControlAvatar();
        LLControlAvatar* cav = root->getControlAvatar();
        const bool cav_expected = animated && contains_skin;

        LLSpatialBridge* bridge =
            root->mDrawable.notNull() ? root->mDrawable->getSpatialBridge()
                                      : nullptr;
        bool control_bridge = dynamic_cast<LLControlAVBridge*>(bridge) != nullptr;

        if (cav_expected && cav && root->mDrawable.notNull())
        {
            // addChild() attached the root before copy_prim_state() installed
            // its animated-object extra parameter, so setupDrawable() initially
            // created an LLAvatarBridge. getSpatialPartition() is the normal
            // bridge validator (lldrawable.cpp:1192); invoke it here instead of
            // depending on a later partition-queue pass that a client-only
            // object may never enter.
            root->mDrawable->getSpatialPartition();
            bridge = root->mDrawable->getSpatialBridge();
            control_bridge =
                dynamic_cast<LLControlAVBridge*>(bridge) != nullptr;

            // The control avatar was created from the now-final linkset. Match
            // it after the correct bridge exists so its world transform and
            // palette origin agree with the attachment root on the first draw.
            cav->matchVolumeTransform();
        }

        LL_INFOS("GhostStudio")
            << "GHOSTCAV root=" << root->getID()
            << " animated=" << animated
            << " contains_skin=" << contains_skin
            << " cav_expected=" << cav_expected
            << " cav=" << (cav != nullptr)
            << " cav_playing=" << (cav && cav->mPlaying)
            << " control_bridge=" << control_bridge
            << " children=" << children.size()
            << LL_ENDL;

        if (cav_expected)
        {
            return cav && cav->mPlaying && control_bridge;
        }
        return cav == nullptr;
    }

    const char* legacy_alpha_mode_name(U8 mode)
    {
        switch (mode)
        {
            case LLMaterial::DIFFUSE_ALPHA_MODE_BLEND: return "blend";
            case LLMaterial::DIFFUSE_ALPHA_MODE_MASK:  return "mask";
            case LLMaterial::DIFFUSE_ALPHA_MODE_NONE:  return "none";
            default:                                   return "emissive";
        }
    }

    std::string face_alpha_mode(const LLTextureEntry* te)
    {
        if (!te)
        {
            return "missing";
        }
        if (const LLGLTFMaterial* gltf = te->getGLTFRenderMaterial())
        {
            return llformat("pbr-%s", gltf->getAlphaMode());
        }
        const LLMaterial* material = te->getMaterialParams().get();
        return material
            ? legacy_alpha_mode_name(material->getDiffuseAlphaMode())
            : "none";
    }

    F32 face_alpha_cutoff(const LLTextureEntry* te)
    {
        if (!te)
        {
            return 0.f;
        }
        if (const LLGLTFMaterial* gltf = te->getGLTFRenderMaterial())
        {
            return gltf->mAlphaCutoff;
        }
        const LLMaterial* material = te->getMaterialParams().get();
        return material
            ? material->getAlphaMaskCutoff() * (1.f / 255.f)
            : 0.f;
    }

    const char* pool_name(U32 pool)
    {
        switch (pool)
        {
            case LLDrawPool::POOL_SIMPLE:                return "simple";
            case LLDrawPool::POOL_FULLBRIGHT:            return "fullbright";
            case LLDrawPool::POOL_MATERIALS:             return "materials";
            case LLDrawPool::POOL_GLTF_PBR:              return "pbr";
            case LLDrawPool::POOL_GLTF_PBR_ALPHA_MASK:   return "pbr-mask";
            case LLDrawPool::POOL_ALPHA_MASK:            return "mask";
            case LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK: return "fb-mask";
            case LLDrawPool::POOL_AVATAR:                return "avatar";
            case LLDrawPool::POOL_CONTROL_AV:            return "control-av";
            case LLDrawPool::POOL_ALPHA_PRE_WATER:       return "alpha-pre";
            case LLDrawPool::POOL_ALPHA_POST_WATER:      return "alpha-post";
            case LLDrawPool::POOL_ALPHA:                 return "alpha";
            default:                                     return "other";
        }
    }

    void log_texture_face(LLVOVolume* source, LLVOVolume* clone,
                          LLFace* clone_face, S32 face_index)
    {
        if (!clone_face)
        {
            return;
        }
        const U8 te_index = clone_face->getTEOffset();
        const LLTextureEntry* source_te =
            source && te_index < source->getNumTEs() ? source->getTE(te_index) : nullptr;
        const LLTextureEntry* clone_te =
            te_index < clone->getNumTEs() ? clone->getTE(te_index) : nullptr;
        LLFace* source_face =
            source && source->mDrawable.notNull() &&
            face_index < source->mDrawable->getNumFaces()
                ? source->mDrawable->getFace(face_index) : nullptr;
        LLViewerTexture* source_texture =
            source && te_index < source->getNumTEs()
                ? source->getTEImage(te_index) : nullptr;
        LLViewerTexture* clone_texture =
            te_index < clone->getNumTEs()
                ? clone->getTEImage(te_index) : nullptr;

        const bool source_baked_magic = source_te &&
            LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::isBakedImageId(
                source_te->getID());
        const bool clone_baked_magic = clone_te &&
            LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::isBakedImageId(
                clone_te->getID());
        LLViewerTexture* source_resolved_bake =
            source_baked_magic
                ? source->getBakedTextureForMagicId(source_te->getID()) : nullptr;
        LLViewerTexture* resolved_bake =
            clone_baked_magic
                ? clone->getBakedTextureForMagicId(clone_te->getID()) : nullptr;

        const LLFetchedGLTFMaterial* fetched = clone_te
            ? dynamic_cast<const LLFetchedGLTFMaterial*>(
                clone_te->getGLTFRenderMaterial()) : nullptr;
        const LLFetchedGLTFMaterial* source_fetched = source_te
            ? dynamic_cast<const LLFetchedGLTFMaterial*>(
                source_te->getGLTFRenderMaterial()) : nullptr;
        LLViewerTexture* base_color =
            fetched && fetched->mBaseColorTexture.notNull()
                ? fetched->mBaseColorTexture.get() : nullptr;
        LLViewerTexture* source_base_color =
            source_fetched && source_fetched->mBaseColorTexture.notNull()
                ? source_fetched->mBaseColorTexture.get() : nullptr;
        const bool drawable_invisible =
            clone->mDrawable.notNull() &&
            clone->mDrawable->isState(LLDrawable::INVISIBLE |
                                      LLDrawable::FORCE_INVISIBLE);
        const bool in_draw_pass = clone_face->mDrawInfo != nullptr;
        const bool draw_ready =
            in_draw_pass && clone_face->getVertexBuffer() != nullptr &&
            clone_face->getGeomCount() > 0 &&
            clone_face->getIndicesCount() > 0 && !drawable_invisible;

        LL_INFOS("GhostStudio")
            << "GHOSTTEX src_obj=" << (source ? source->getID() : LLUUID::null)
            << " clone_obj=" << clone->getID()
            << " face=" << face_index << " te=" << (S32)te_index
            << " src_te=" << (source_te ? source_te->getID() : LLUUID::null)
            << " clone_te=" << (clone_te ? clone_te->getID() : LLUUID::null)
            << " src_face_tex=" << (source_texture ? source_texture->getID() : LLUUID::null)
            << " clone_face_tex=" << (clone_texture ? clone_texture->getID() : LLUUID::null)
            << " same_binding=" << (source_texture == clone_texture)
            << " gl=" << (clone_texture && clone_texture->hasGLTexture())
            << " discard=" << (clone_texture ? clone_texture->getDiscardLevel() : -99)
            << " missing=" << (clone_texture && clone_texture->isMissingAsset())
            << " src_material="
            << (source_te ? source->getRenderMaterialID(te_index) : LLUUID::null)
            << " src_base_color="
            << (source_base_color ? source_base_color->getID() : LLUUID::null)
            << " src_base_gl="
            << (source_base_color && source_base_color->hasGLTexture())
            << " material="
            << (clone_te ? clone->getRenderMaterialID(te_index) : LLUUID::null)
            << " base_color=" << (base_color ? base_color->getID() : LLUUID::null)
            << " base_gl=" << (base_color && base_color->hasGLTexture())
            << " base_discard=" << (base_color ? base_color->getDiscardLevel() : -99)
            << " src_baked_magic=" << source_baked_magic
            << " src_baked_resolved="
            << (source_resolved_bake
                    ? source_resolved_bake->getID() : LLUUID::null)
            << " src_baked_default="
            << (source_resolved_bake &&
                source_resolved_bake->getID() == IMG_DEFAULT)
            << " baked_magic=" << clone_baked_magic
            << " baked_resolved="
            << (resolved_bake ? resolved_bake->getID() : LLUUID::null)
            << " baked_default="
            << (resolved_bake && resolved_bake->getID() == IMG_DEFAULT)
            << " binding_default="
            << (clone_texture && clone_texture->getID() == IMG_DEFAULT)
            << " src_alpha_mode=" << face_alpha_mode(source_te)
            << " src_alpha_cutoff=" << face_alpha_cutoff(source_te)
            << " src_can_render_as_mask="
            << (source_face && source_face->canRenderAsMask())
            << " src_color_alpha="
            << (source_te ? source_te->getColor().mV[3] : -1.f)
            << " src_fullbright=" << (source_te && source_te->getFullbright())
            << " src_pool="
            << (source_face ? pool_name(source_face->getPoolType()) : "missing")
            << " src_in_draw_pass="
            << (source_face && source_face->mDrawInfo != nullptr)
            << " alpha_mode=" << face_alpha_mode(clone_te)
            << " alpha_cutoff=" << face_alpha_cutoff(clone_te)
            << " can_render_as_mask=" << clone_face->canRenderAsMask()
            << " alpha_mask_policy_same="
            << (!source_face ||
                source_face->canRenderAsMask() ==
                    clone_face->canRenderAsMask())
            << " color_alpha=" << (clone_te ? clone_te->getColor().mV[3] : -1.f)
            << " fullbright=" << (clone_te && clone_te->getFullbright())
            << " pool=" << pool_name(clone_face->getPoolType())
            << "(" << clone_face->getPoolType() << ")"
            << " in_draw_pass=" << in_draw_pass
            << " draw_ready=" << draw_ready
            << " drawable_invisible=" << drawable_invisible
            << " vb=" << (clone_face->getVertexBuffer() != nullptr)
            << " geom=" << clone_face->getGeomCount()
            << " indices=" << clone_face->getIndicesCount()
            << LL_ENDL;
    }
}

S32 LLGhostAvatar::cloneAttachmentsFrom(LLVOAvatar* source)
{
    if (!source || source == this)
    {
        return 0;
    }
    LLViewerRegion* region = getRegion();
    if (!region)
    {
        LL_WARNS("GhostStudio") << "cloneAttachmentsFrom: ghost has no region" << LL_ENDL;
        ++mCloneFailures;
        return 0;
    }

    static LLCachedControl<bool> exclude_temporary(
        gSavedSettings, "GhostUnifiedExcludeTemporaryAttachments", false);
    const ALGhostAttachmentPlan plan =
        ALGhostAttachmentEnumerator::enumerateWorldRoots(
            source,
            exclude_temporary
                ? ALGhostTempAttachmentPolicy::EXCLUDE
                : ALGhostTempAttachmentPolicy::INCLUDE,
            /*emit_log=*/true);
    if (!plan.mValidSource)
    {
        ++mCloneFailures;
        return 0;
    }

    S32 cloned = 0;
    mCloneExpectedRoots += plan.expectedRoots();
    for (const ALGhostAttachmentRoot& root_entry : plan.mRoots)
    {
        LLViewerObject* src_obj = root_entry.mRoot.get();
        if (!src_obj || src_obj->isDead())
        {
            LL_WARNS("GhostStudio")
                << "captured attachment root disappeared during clone"
                << LL_ENDL;
            mCloneFailures++;
            continue;
        }

            LLVOVolume* src_root = dynamic_cast<LLVOVolume*>(src_obj);
            if (!src_root)
            {
                LL_WARNS("GhostStudio") << "attachment root " << src_obj->getID()
                                        << " is not an LLVOVolume; cannot clone it"
                                        << LL_ENDL;
                mCloneFailures++;
                continue;
            }

            // Read the attachment point up front: every duplicated prim needs
            // it set BEFORE its geometry is built, not after.
            const U8 attach_state = src_root->getAttachmentState();

            // Preserve the source's ATTACHMENT-JOINT-LOCAL transform. NOT
            // getRenderPosition()/setPositionAgent(), which are world space.
            const LLVector3    local_pos = src_root->getPosition();
            const LLQuaternion local_rot = src_root->getRotation();

            // ============================================================
            // ORDER BELOW IS THE FIX. Do not reorder without re-reading the
            // note in alloc_local_copy(). Build the ENTIRE object tree before
            // any drawable exists, then create drawables in hierarchy order,
            // then attach, then finally assign geometry.
            // ============================================================

            // -- 1. Allocate root + children as bare objects (no drawables).
            LLVOVolume* dst_root = alloc_local_copy(region, attach_state);
            if (!dst_root)
            {
                LL_WARNS("GhostStudio") << "root allocation failed for "
                                        << src_root->getID() << LL_ENDL;
                mCloneFailures++;
                continue;
            }

            std::vector<LLVOVolume*> src_children;
            std::vector<LLVOVolume*> dst_children;
            bool alloc_ok = true;
            for (const LLPointer<LLViewerObject>& src_child_ptr :
                    root_entry.mChildren)
            {
                LLViewerObject* src_child = src_child_ptr.get();
                if (!src_child || src_child->isDead())
                {
                    alloc_ok = false;
                    break;
                }
                LLVOVolume* src_cv = dynamic_cast<LLVOVolume*>(src_child);
                if (!src_cv)
                {
                    // A live child we cannot reproduce means the linkset can
                    // never be a faithful copy. Fail it rather than quietly
                    // shipping something with a piece missing.
                    LL_WARNS("GhostStudio") << "attachment child " << src_child->getID()
                                            << " is not an LLVOVolume; cannot clone it"
                                            << LL_ENDL;
                    alloc_ok = false;
                    break;
                }
                LLVOVolume* dst_cv = alloc_local_copy(region, attach_state);
                if (!dst_cv)
                {
                    // Do NOT silently skip: that quietly produces an incomplete
                    // linkset that never reaches the copy-failure handling and
                    // can still be reported as a success.
                    alloc_ok = false;
                    break;
                }
                src_children.push_back(src_cv);
                dst_children.push_back(dst_cv);
            }
            if (!alloc_ok)
            {
                LL_WARNS("GhostStudio") << "child allocation failed for "
                                        << src_root->getID()
                                        << "; discarding the whole linkset" << LL_ENDL;
                // Nothing is attached or drawable-backed yet at this point, so
                // plain markDead on each allocated object is sufficient.
                for (LLVOVolume* c : dst_children)
                {
                    c->markDead();
                }
                dst_root->markDead();
                mCloneFailures++;
                continue;
            }

            // -- 2. Object hierarchy FIRST, so no child is ever a drawable root.
            for (size_t ci = 0; ci < dst_children.size(); ++ci)
            {
                dst_children[ci]->setPosition(src_children[ci]->getPosition());
                dst_children[ci]->setRotation(src_children[ci]->getRotation());
                dst_root->addChild(dst_children[ci]);
            }

            // -- 3. Root drawable. Must exist BEFORE addChild(): LLVOAvatar::
            //       addChild only calls attachObject() immediately when a
            //       drawable is present, else it defers via mPendingAttachment
            //       (llvoavatar.cpp:7838/7850).
            gPipeline.createObject(dst_root);
            dst_root->setLOD(LLVolumeLODGroup::NUM_LODS - 1);

            // -- 4. Child drawables, now that each already has dst_root as its
            //       object parent, so they inherit its partition instead of
            //       minting their own bridge.
            //       No explicit setDrawableParent() needed: createObject() sees
            //       the established object parent and wires the drawable parent
            //       itself (pipeline.cpp:2578). The load-bearing part is simply
            //       that the object parent and root drawable already exist.
            for (LLVOVolume* c : dst_children)
            {
                gPipeline.createObject(c);
                c->setLOD(LLVolumeLODGroup::NUM_LODS - 1);
            }

            // -- 5. Attach the completed linkset to the ghost. This runs
            //       attachObject() -> setupDrawable(), which reparents the root
            //       drawable's mXform to the attachment joint.
            addChild(dst_root);

            // -- 6. Restore the joint-local transform. setupDrawable() derived
            //       one from the object's meaningless initial region-space
            //       position, so overwriting it here is required, not harmful.
            dst_root->setPosition(local_pos);
            dst_root->setRotation(local_rot);

            // -- 7. ONLY NOW assign geometry. The first geometry build then
            //       sees correct attachment state, avatar ancestor, drawable
            //       parent and a single shared bridge -- so faces classify
            //       rigged on the first pass instead of needing repair.
            //
            //       Failures here are NOT ignorable: an uncopied volume yields
            //       an invisible or placeholder prim, and counting that as a
            //       successful clone muddies the in-world result.
            //       ANY copy failure -- root or child -- discards the WHOLE
            //       linkset. A partially cloned linkset is worse than none for
            //       an acceptance harness: the prims that DID copy can verify
            //       correctly and print PASS while a missing child is silently
            //       absent, and a placeholder child may read as "static" and
            //       escape both the pending and the not-rigged counters.
            //       PASS has to mean the whole thing cloned.
            bool copy_ok = copy_prim_state(dst_root, src_root);
            log_ghost_rig(dst_root, src_root);
            size_t failed_child = 0;
            if (copy_ok)
            {
                for (size_t ci = 0; ci < dst_children.size(); ++ci)
                {
                    const bool child_copy_ok =
                        copy_prim_state(dst_children[ci], src_children[ci]);
                    log_ghost_rig(dst_children[ci], src_children[ci]);
                    if (!child_copy_ok)
                    {
                        copy_ok = false;
                        failed_child = ci + 1;
                        break;
                    }
                }
            }
            if (!copy_ok)
            {
                LL_WARNS("GhostStudio")
                    << "volume copy failed for " << src_root->getID()
                    << (failed_child ? llformat(" (child %d of %d)", (S32)failed_child,
                                                (S32)dst_children.size())
                                     : std::string(" (root)"))
                    << " -- source assets not loaded; discarding the whole linkset"
                    << LL_ENDL;
                // Clear attachment state BEFORE removeChild so removeObject()'s
                // makeStatic() is effective and the drawables re-home cleanly
                // instead of staying active on the avatar's spatial bridge
                // (lllocalmesh.cpp:1403-1425).
                dst_root->setAttachmentState(0);
                for (LLVOVolume* c : dst_children)
                {
                    c->setAttachmentState(0);
                }
                removeChild(dst_root);
                dst_root->markDead();   // cascades to children, llviewerobject.cpp:464
                mCloneFailures++;
                continue;
            }

            // The linkset was attached before its simulator-delivered animated
            // flag and skin state were copied. Finalize the control avatar and
            // bridge only after the entire state graph exists.
            if (!finalize_control_avatar(dst_root, dst_children))
            {
                LL_WARNS("GhostStudio")
                    << "control-avatar finalization failed for "
                    << src_root->getID()
                    << "; discarding the whole linkset" << LL_ENDL;
                dst_root->setAttachmentState(0);
                for (LLVOVolume* c : dst_children)
                {
                    c->setAttachmentState(0);
                }
                removeChild(dst_root);
                dst_root->markDead();
                mCloneFailures++;
                continue;
            }

            // Every descendant carries the same explicit render owner. This
            // covers static children and animesh without relying on joint-scale
            // inheritance (which LLXform intentionally drops).
            stampEntityOuterTransform(dst_root);

            // -- 8. Rebuild only after the final skinning avatar and spatial
            //       bridge are known. Faces created earlier must not retain the
            //       ordinary attachment owner or static classification.
            force_rebuild(dst_root);
            for (LLVOVolume* c : dst_children)
            {
                force_rebuild(c);
            }

            ClonedLinkset record;
            record.mRoot = dst_root->getID();
            record.mSourceRoot = src_root->getID();
            record.mAttachPoint = ATTACHMENT_ID_FROM_STATE(attach_state); // [AutoAnimate]
            record.mSourceItemId = src_root->getAttachmentItemID(); // [AutoAnimate]
            record.mChildren.reserve(dst_children.size());
            record.mSourceChildren.reserve(src_children.size());
            for (LLVOVolume* c : dst_children)
            {
                record.mChildren.push_back(c->getID());
            }
            for (LLVOVolume* c : src_children)
            {
                record.mSourceChildren.push_back(c->getID());
            }
            // Index this linkset for getClonedSourceLOD(). Children are paired
            // positionally with mSourceChildren, exactly as the search path
            // did; a short source list simply leaves the tail unindexed, which
            // resolves as a miss just like the old code's bounds check.
            sClonePrimToSourcePrim[record.mRoot] = record.mSourceRoot;
            for (size_t i = 0; i < record.mChildren.size() &&
                               i < record.mSourceChildren.size(); ++i)
            {
                sClonePrimToSourcePrim[record.mChildren[i]] =
                    record.mSourceChildren[i];
            }

            mClonedLinksets.push_back(record);
            cloned++;

            // Structural failures (attachment registration, joint parenting,
            // child bridge/partition topology) must reach the verifier -- the
            // face-level pass cannot see any of them.
            if (!log_attach_verify(this, dst_root, src_root, dst_children))
            {
                mCloneFailures++;
            }
    }

    if (cloned > 0)
    {
        // One explicit override rebuild after the whole linkset is in place.
        // addAttachmentOverridesForObject rejects objects whose getAvatar() is
        // not this avatar (llvoavatar.cpp:7032), so this mutates the GHOST's
        // skeleton and never the source's.
        updateAttachmentOverrides();
    }

    LL_INFOS("GhostStudio") << "cloneAttachmentsFrom: duplicated " << cloned
                            << " attachment root(s)" << LL_ENDL;
    return cloned;
}

void LLGhostAvatar::releaseClonedAttachments()
{
    // [AnimeshRepeat] The records go away with mClonedLinksets below; clear
    // explicitly so the teardown paths (Refresh, despawn, markDead) share
    // one reset with the other lifecycle points.
    resetHeldAnimeshRepeat();
    object_signaled_animation_map_t& object_anims =
        LLObjectSignaledAnimationMap::instance().getMap();
    for (const ClonedLinkset& linkset : mClonedLinksets)
    {
        object_anims.erase(linkset.mRoot);
        for (const LLUUID& child_id : linkset.mChildren)
        {
            object_anims.erase(child_id);
        }
        LLViewerObject* root = gObjectList.findObject(linkset.mRoot);
        if (!root || root->isDead())
        {
            continue;
        }
        // Clear attachment state on the WHOLE linkset FIRST: the makeStatic()
        // inside removeObject() is guarded on !isAttachment(), so with state
        // still set it no-ops and the drawables stay ACTIVE on the avatar's
        // spatial bridge -- rendering adrift from their bounding boxes
        // (lllocalmesh.cpp:1403-1425).
        root->setAttachmentState(0);
        for (LLViewerObject* child : root->getChildren())
        {
            if (child)
            {
                child->setAttachmentState(0);
            }
        }
        removeChild(root);
        if (root->mDrawable.notNull())
        {
            root->mDrawable->mXform.setParent(NULL);
        }
        root->markDead();
    }
    // Drop this ghost's entries from the shared clone->source index before the
    // records that name them go away. Erase by id (not clear()) so other live
    // ghosts keep theirs.
    for (const ClonedLinkset& linkset : mClonedLinksets)
    {
        sClonePrimToSourcePrim.erase(linkset.mRoot);
        for (const LLUUID& child : linkset.mChildren)
        {
            sClonePrimToSourcePrim.erase(child);
        }
    }
    mClonedLinksets.clear();
    mRigHealedLogged.clear();

    // Reset the transactional counters WITH the roots they describe. Clearing
    // the roots but keeping the expected/failure counts would leave a later,
    // perfectly good re-clone on this ghost permanently INCOMPLETE. (Do NOT
    // instead reset at the START of a clone call: that call does not release
    // existing roots, so the recorded roots and the new expected count would
    // then describe different transactions.)
    mCloneExpectedRoots = 0;
    mCloneFailures      = 0;
}

std::string LLGhostAvatar::getRigDiagnosticText() const
{
    std::string text =
        "       id       mesh skin rig anim cav attach\n"
        "  face te diffuse  bind     magic/default alpha/cut  fb pool        pass/ready\n";
    S32 prim_index = 0;

    auto append_prim = [&text, &prim_index](const LLUUID& clone_id,
                                            const LLUUID& source_id)
    {
        LLVOVolume* clone = dynamic_cast<LLVOVolume*>(
            gObjectList.findObject(clone_id));
        LLVOVolume* source = dynamic_cast<LLVOVolume*>(
            gObjectList.findObject(source_id));
        const std::string clone_short =
            clone_id.asString().substr(0, 8);
        const std::string source_short =
            source_id.asString().substr(0, 8);

        text += llformat(
            "%03d C  %s   %d    %d    %d   %d   %d  %s\n",
            prim_index, clone_short.c_str(),
            clone && clone->isMesh(),
            clone && clone->getSkinInfo() != nullptr,
            clone && clone->isRiggedMesh(),
            clone && clone->isAnimatedObject(),
            clone && clone->getControlAvatar() != nullptr,
            attachment_joint_name(clone).c_str());
        text += llformat(
            "    S  %s   %d    %d    %d   %d   %d  %s\n",
            source_short.c_str(),
            source && source->isMesh(),
            source && source->getSkinInfo() != nullptr,
            source && source->isRiggedMesh(),
            source && source->isAnimatedObject(),
            source && source->getControlAvatar() != nullptr,
            attachment_joint_name(source).c_str());

        if (clone && clone->mDrawable.notNull())
        {
            for (S32 face_index = 0;
                 face_index < clone->mDrawable->getNumFaces(); ++face_index)
            {
                LLFace* face = clone->mDrawable->getFace(face_index);
                if (!face)
                {
                    continue;
                }

                const U8 te_index = face->getTEOffset();
                const LLTextureEntry* te =
                    te_index < clone->getNumTEs()
                        ? clone->getTE(te_index) : nullptr;
                LLViewerTexture* binding =
                    te_index < clone->getNumTEs()
                        ? clone->getTEImage(te_index) : nullptr;
                LLViewerTexture* source_binding =
                    source && te_index < source->getNumTEs()
                        ? source->getTEImage(te_index) : nullptr;
                const bool magic = te &&
                    LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::
                        isBakedImageId(te->getID());
                LLViewerTexture* resolved =
                    magic
                        ? clone->getBakedTextureForMagicId(te->getID())
                        : nullptr;
                const bool resolved_default =
                    resolved && resolved->getID() == IMG_DEFAULT;
                const bool binding_default =
                    binding && binding->getID() == IMG_DEFAULT;
                const bool drawable_invisible =
                    clone->mDrawable->isState(
                        LLDrawable::INVISIBLE |
                        LLDrawable::FORCE_INVISIBLE);
                const bool in_draw_pass = face->mDrawInfo != nullptr;
                const bool draw_ready =
                    in_draw_pass && face->getVertexBuffer() != nullptr &&
                    face->getGeomCount() > 0 &&
                    face->getIndicesCount() > 0 && !drawable_invisible;
                const std::string diffuse_short =
                    te ? te->getID().asString().substr(0, 8) : "missing ";
                const std::string binding_short =
                    binding ? binding->getID().asString().substr(0, 8)
                            : "missing ";
                const std::string source_binding_short =
                    source_binding
                        ? source_binding->getID().asString().substr(0, 8)
                        : "missing ";
                const LLTextureEntry* source_te =
                    source && te_index < source->getNumTEs()
                        ? source->getTE(te_index) : nullptr;

                text += llformat(
                    "  F%03d %02d %-8s %-8s %d/%d%d %-9s %.2f %d %-11s %d/%d"
                    " src=%s same=%d sa=%s/%.2f a=%.2f\n",
                    face_index, (S32)te_index,
                    diffuse_short.c_str(), binding_short.c_str(),
                    magic, resolved_default, binding_default,
                    face_alpha_mode(te).c_str(), face_alpha_cutoff(te),
                    te && te->getFullbright(),
                    pool_name(face->getPoolType()),
                    in_draw_pass, draw_ready,
                    source_binding_short.c_str(),
                    binding == source_binding,
                    face_alpha_mode(source_te).c_str(),
                    face_alpha_cutoff(source_te),
                    te ? te->getColor().mV[3] : -1.f);
            }
        }
        ++prim_index;
    };

    for (const ClonedLinkset& linkset : mClonedLinksets)
    {
        append_prim(linkset.mRoot, linkset.mSourceRoot);
        for (size_t i = 0; i < linkset.mChildren.size(); ++i)
        {
            append_prim(
                linkset.mChildren[i],
                i < linkset.mSourceChildren.size()
                    ? linkset.mSourceChildren[i] : LLUUID::null);
        }
    }

    if (prim_index == 0)
    {
        text += "No cloned attachment prims recorded.";
    }
    return text;
}

void LLGhostAvatar::markForDeath()
{
    // Deferred death, mirroring LLControlAvatar. markDead() must never run
    // inside a pipeline traversal, and spawn/destroy must stay on the viewer
    // thread outside any sAllowInstancesChange == false window.
    mMarkedForDeath = true;
}

// virtual
void LLGhostAvatar::markDead()
{
    // Cover object-list/region teardown as well as Studio-initiated removal.
    // This is idempotent and erases clone-local ObjectAnimation entries before
    // the base avatar death cascades through any remaining child objects.
    releaseClonedAttachments();
    LLVOAvatar::markDead();
}

// [AnimeshRepeat] A latched ledger entry keeps a LOOPING asset running, but a
// ONE-SHOT animesh animation (the usual script-retriggered tail/idle) plays
// out once and freezes: the source's script was the thing restarting it every
// cycle, and once the source is detached nothing does. With the opt-in
// GhostMirrorRepeatHeldAnimesh on, stand in for that script on the clone's
// OWN control avatar: when a held one-shot has finished, start it again.
//
// Lever: LLCharacter::startMotion(id) on the linkset root's control avatar
// (one per root-edit linkset; children resolve to it). On an inactive, loaded
// motion that is activateMotionInstance(), which samples onUpdate(0.f) --
// no preceding stop, no LLMotion::activate() (bypasses controller
// bookkeeping), no setLoop() (mutates the shared cached asset). The ledger
// is never written, so LLVOAvatar::mPlayingAnimations keeps its sequence-id
// provenance: a returning live signal or a later erase still starts/stops
// the animation through processAnimationStateChanges() exactly as before.
//
// Eligibility: the id must be signaled only by LATCHED clone prims (a LIVE
// prim's signal takes precedence and is the source's business), the motion
// must be loaded (not in mLoadingMotions -- an unloaded LLKeyframeMotion
// reports getLoop()==false and getDuration()==0), have positive duration and
// getLoop()==false. A looping asset is never restarted.
//
// Transition logic (per id, ClonedLinkset::RepeatState): observe ACTIVE ->
// mSeenActive; observe INACTIVE with mSeenActive -> replay once, clear
// mSeenActive, set mPending; observe ACTIVE again -> clear mPending, set
// mSeenActive (armed for the next completion). A replay that never becomes
// active (mPending and still inactive/not loading) counts a failure and is
// retried at most MAX_REPLAY_FAILURES times. Absent record == armed, so a
// clip that had already finished when the hold began replays once.
//
// Honesty: each replay samples from frame 0 after the previous instance has
// eased out to rest -- FUNCTIONAL repetition with a visible seam per cycle,
// not seamless looping. Also a genuinely one-shot action (a mechanism
// closing, a reaction) now repeats, which is why this is opt-in.
void LLGhostAvatar::repeatHeldAnimesh()
{
    static LLCachedControl<bool> repeat_held_animesh(
        gSavedSettings, "GhostMirrorRepeatHeldAnimesh", false);
    static LLCachedControl<bool> hold_on_source_change(
        gSavedSettings, "GhostMirrorHoldOnSourceChange", true);
    constexpr U32 MAX_REPLAY_FAILURES = 3;

    if (!repeat_held_animesh || !hold_on_source_change ||
        (mEntityDriveMode != ALGhostStudio::DRIVE_MIRROR &&
         mEntityDriveMode != ALGhostStudio::DRIVE_TRUE_MIRROR))
    {
        return;
    }

    const object_signaled_animation_map_t& object_anims =
        LLObjectSignaledAnimationMap::instance().getMap();

    for (ClonedLinkset& linkset : mClonedLinksets)
    {
        if (linkset.mAutonomy != 0) continue; // [AutoAnimate]
        if (!linkset.mAnimeshHeld)
        {
            continue; // live mirror: the source's script retriggers
        }
        LLViewerObject* root = gObjectList.findObject(linkset.mRoot);
        if (!root || root->isDead() || !root->isAnimatedObject())
        {
            continue;
        }
        LLControlAvatar* cav = root->getControlAvatar();
        if (!cav || cav->isDead())
        {
            continue;
        }
        if (linkset.mRepeatControlAvatarId != cav->getID())
        {
            // First sight, or the control avatar was re-created (lazy skin
            // arrival, ObjectAnimation-driven relink): its motion instances
            // are new, so any pending/seen state is meaningless.
            linkset.mRepeatControlAvatarId = cav->getID();
            linkset.mRepeatStates.clear();
        }
        LLMotionController& controller = cav->getMotionController();
        // The CONTROL AVATAR's own pause (Studio FROZEN, selection pause),
        // not the wearer's: TRUE_MIRROR pauses the wearer but leaves the
        // animesh path live. startMotion() while paused records nothing
        // useful and the motion would stay stopped. Jellydolled control
        // avatars shelve every animation; leave them alone too.
        if (controller.isPaused() ||
            cav->getOverallAppearance() != LLVOAvatar::AOA_NORMAL)
        {
            continue;
        }

        // Partition the linkset's signaled ids by contributor.
        std::set<LLUUID> latched_ids;
        std::set<LLUUID> live_ids;
        auto collect = [&](const LLUUID& clone_id)
        {
            object_signaled_animation_map_t::const_iterator entry =
                object_anims.find(clone_id);
            if (entry == object_anims.end())
            {
                return;
            }
            const bool latched =
                std::find(linkset.mAnimeshLatchedPrims.begin(),
                          linkset.mAnimeshLatchedPrims.end(),
                          clone_id) != linkset.mAnimeshLatchedPrims.end();
            for (const auto& anim : entry->second)
            {
                (latched ? latched_ids : live_ids).insert(anim.first);
            }
        };
        collect(linkset.mRoot);
        for (const LLUUID& child_id : linkset.mChildren)
        {
            collect(child_id);
        }

        for (const LLUUID& anim_id : latched_ids)
        {
            if (live_ids.count(anim_id))
            {
                continue; // a live contributor owns this id
            }
            ClonedLinkset::RepeatState& state = linkset.mRepeatStates[anim_id];
            if (state.mFailures >= MAX_REPLAY_FAILURES)
            {
                continue;
            }
            LLMotion* motion = cav->findMotion(anim_id);
            if (!motion || controller.isMotionLoading(motion))
            {
                // Never instantiated, or still fetching (a replay may be
                // pending on it): nothing observable yet.
                continue;
            }
            if (cav->isMotionActive(anim_id))
            {
                state.mSeenActive = true;
                state.mPending = false;
                continue;
            }
            // Inactive and loaded: attributes are now trustworthy.
            if (motion->getLoop() || motion->getDuration() <= 0.f)
            {
                continue; // looping assets keep going on their own
            }
            if (state.mPending)
            {
                // The replay we issued never activated. Count it; retry
                // below while under the cap so a transient miss recovers
                // without a per-frame storm.
                state.mPending = false;
                ++state.mFailures;
                if (state.mFailures >= MAX_REPLAY_FAILURES)
                {
                    LL_WARNS("GhostStudio")
                        << "AnimeshRepeat giving up on " << anim_id
                        << " root=" << linkset.mRoot
                        << " after " << state.mFailures
                        << " replays that never activated" << LL_ENDL;
                    continue;
                }
                // fall through: retry this frame, still under the cap
            }
            else if (!state.mSeenActive && state.mFailures > 0)
            {
                // Past failures but no replay in flight and no activation
                // seen since: wait for the source-of-truth (an observed
                // activation) rather than probing every frame.
                continue;
            }
            // Consume this completion: one replay per ACTIVE -> INACTIVE
            // edge (or one at hold entry for a clip that finished before the
            // hold began -- a fresh record starts unarmed-but-absent, which
            // the map insert above treats as armed).
            state.mSeenActive = false;
            if (cav->LLCharacter::startMotion(anim_id))
            {
                state.mPending = true;
                LL_DEBUGS("GhostStudio")
                    << "AnimeshRepeat replay " << anim_id
                    << " root=" << linkset.mRoot
                    << " cav=" << cav->getID() << LL_ENDL;
            }
            else
            {
                ++state.mFailures;
            }
        }
    }
}

void LLGhostAvatar::resetHeldAnimeshRepeat()
{
    for (ClonedLinkset& linkset : mClonedLinksets)
    {
        linkset.mAnimeshHeld = false;
        linkset.mAnimeshLatchedPrims.clear();
        linkset.mRepeatControlAvatarId.setNull();
        linkset.mRepeatStates.clear();
    }
}

// virtual
void LLGhostAvatar::idleUpdate(LLAgent &agent, const F64 &time)
{
    if (mMarkedForDeath)
    {
        markDead();
        mMarkedForDeath = false;
        return;
    }

    // AvatarAnimation is delivered only for simulator-known avatar UUIDs.
    // Mirror the source's already-received state into the clone-only ledger
    // before LLVOAvatar::idleUpdate() runs updateMotions().
    // GhostMirrorHoldOnSourceChange: keep the clone performing across a transient
    // source change (outfit re-rez / de-rez / imposter / AO gap) instead of
    // mirroring the momentary gap. Gates both the bento and animesh holds below.
    static LLCachedControl<bool> hold_on_source_change(
        gSavedSettings, "GhostMirrorHoldOnSourceChange", true);
    const bool hold_eff = mAutoAnim != nullptr || hold_on_source_change; // [AutoAnimate]
    if (mAutoAnim) autoAnimRecordTick(); // [AutoAnimate]
    if (mEntityDriveMode == ALGhostStudio::DRIVE_MIRROR)
    {
        LLViewerObject* source_obj = gObjectList.findObject(mAnimationSourceId);
        LLVOAvatar* source = source_obj ? source_obj->asAvatar() : nullptr;
        // [AutoAnimate] Absence is evaluated even when the resolved pointer is null.
        if (mAutoAnim)
        {
            if (mAutoAnim->simloss) source = nullptr;
            autoAnimBodyPresence(source);
        }
        if (mAutoAnim && mAutoAnim->body.autonomy != AUTONOMY_NONE)
        {
            autoAnimBodyStep();
        }
        else if (source && !source->isDead())
        {
            // The clone-only synchronizer makes this filter unnecessary for
            // simulator safety.  Retain it solely as a behavioral choice:
            // ghosts do not adopt ground-sit poses, while ordinary prim-sit
            // animations continue to mirror.
            signaled_animation_map_t mirrored_animations =
                source->mSignaledAnimations;
            mirrored_animations.erase(ANIM_AGENT_SIT_GROUND);
            mirrored_animations.erase(ANIM_AGENT_SIT_GROUND_CONSTRAINED);
            // A source change (outfit swap, brief de-rez, out-of-view imposter, or
            // AO gap) transiently empties the source's signaled set. With
            // GhostMirrorHoldOnSourceChange on, mirroring that empty would stop the
            // clone's whole bento performance, so HOLD the last mirror when the
            // source momentarily contributes nothing. A genuine switch to a different
            // animation is still non-empty and mirrors normally. Option off = faithful
            // live mirror (an empty source set stops the clone too).
            if ((!hold_eff || !mirrored_animations.empty()) &&
                mCloneDesiredAnimations != mirrored_animations)
            {
                autoAnimSyncBody(mirrored_animations); // [AutoAnimate] Audit recouple/follow sync too.
            }
        }
    }
    else if (mEntityDriveMode == ALGhostStudio::DRIVE_DIRECTED &&
             mEntityDirectedAnim.notNull())
    {
        bool active = isMotionActive(mEntityDirectedAnim);
        const F32 anim_time = getMotionController().getAnimTime();
        if (active && !mEntityDirectedWasActive)
        {
            mEntityDirectedStartTime = anim_time;
        }
        if (active && mEntityLoopMode == ALGhostStudio::LOOP_PLAY_ONCE)
        {
            LLMotion* motion = findMotion(mEntityDirectedAnim);
            if (motion && motion->getDuration() > 0.f &&
                anim_time - mEntityDirectedStartTime >= motion->getDuration())
            {
                // A baked-loop asset cannot be mutated per instance because
                // its keyframe data is shared. Stop this clone's canonical
                // motion at one duration instead.
                autoAnimAuditBodyStop(mEntityDirectedAnim); // [AutoAnimate]
                LLCharacter::stopMotion(mEntityDirectedAnim, true);
                active = false;
            }
        }
        if (!active &&
            (mEntityLoopMode == ALGhostStudio::LOOP_RETRIGGER ||
             !mEntityDirectedStarted))
        {
            LLCharacter::startMotion(mEntityDirectedAnim);
            mEntityDirectedStarted = true;
        }
        mEntityDirectedWasActive = active;
    }

    // ObjectAnimation messages are indexed by simulator object UUID. Clone
    // prims have synthetic UUIDs, so mirror each source entry onto its matching
    // local prim before refreshing the linkset's LLControlAvatar.
    object_signaled_animation_map_t& object_anims =
        LLObjectSignaledAnimationMap::instance().getMap();
    // [TrueMirror] animesh control avatars are separate skeletons the joint
    // stamp does not cover, so TRUE_MIRROR keeps the ledger mirror for them.
    if (mEntityDriveMode == ALGhostStudio::DRIVE_MIRROR ||
        mEntityDriveMode == ALGhostStudio::DRIVE_TRUE_MIRROR)
    {
      for (ClonedLinkset& linkset : mClonedLinksets)
      {
        if (mAutoAnim && autoAnimLinksetStep(linkset)) continue; // [AutoAnimate]
        LLViewerObject* root = gObjectList.findObject(linkset.mRoot);
        if (!root || root->isDead() || !root->isAnimatedObject())
        {
            continue;
        }

        bool changed = false;
        // Clone prims whose ledger entry was LATCHED (kept, not mirrored)
        // this frame. Non-empty == the linkset's animesh is HELD; recorded on
        // the linkset for repeatHeldAnimesh() (runs after this loop).
        std::vector<LLUUID> latched_prims;

        // Hold the clone's animesh when the SOURCE linkset is gone. A wearer's
        // outfit change re-rezzes its animesh with fresh UUIDs, so the source-id
        // lookups below would all miss and mirror_one would erase (stop) the
        // clone's animation - freezing the mesh on a transient source change.
        // Skipping keeps the last performance playing. Gated by
        // GhostMirrorHoldOnSourceChange (off = faithful live mirror).
        bool source_gone = false;
        if (hold_eff)
        {
            LLViewerObject* source_root =
                gObjectList.findObject(linkset.mSourceRoot);
            source_gone = !source_root || source_root->isDead();
            if (mAutoAnim && mAutoAnim->simloss) source_gone = true; // [AutoAnimate]
        }

        if (source_gone)
        {
            // Every prim keeps its current entry.
            latched_prims.push_back(linkset.mRoot);
            latched_prims.insert(latched_prims.end(),
                                 linkset.mChildren.begin(),
                                 linkset.mChildren.end());
        }
        else
        {
            // [AutoAnimate] Capture the effective hold; the disabled value is
            // exactly the existing GhostMirrorHoldOnSourceChange setting.
            auto mirror_one = [&object_anims, &changed, &latched_prims, hold_eff](
                                  const LLUUID& source_id,
                                  const LLUUID& clone_id)
            {
                object_signaled_animation_map_t::const_iterator found =
                    object_anims.find(source_id);
                if (found == object_anims.end() || found->second.empty())
                {
                    // [AnimeshDetachHold] A detach empties the source prim's
                    // signaled set (its script's attach(NULL_KEY) stop and/or
                    // the sim's final ObjectAnimation) BEFORE the prim dies,
                    // so this branch -- not the dead-source skip above -- is
                    // what stopped the clone's tail on a source detach. Mirror
                    // the bento hold (`!mirrored_animations.empty()` above):
                    // with the hold on, an empty/missing source set is a
                    // source change to ride out, so LATCH the clone's existing
                    // entry instead of erasing it. The control avatar keeps a
                    // looping asset running for as long as the entry stands;
                    // a finished one-shot is replayed only by the opt-in
                    // repeatHeldAnimesh(). A switch to a different set is
                    // non-empty and still mirrors normally. Off = faithful
                    // live mirror.
                    if (hold_eff)
                    {
                        latched_prims.push_back(clone_id);
                        return;
                    }
                    // Missing and empty both mean "no requested animations";
                    // do not leave an empty synthetic-prim entry in the
                    // process-wide map.
                    object_signaled_animation_map_t::iterator clone =
                        object_anims.find(clone_id);
                    if (clone != object_anims.end())
                    {
                        // Erasing an already-empty entry changes no animation
                        // state.
                        changed = !clone->second.empty() || changed;
                        object_anims.erase(clone);
                    }
                    return;
                }

                object_signaled_animation_map_t::iterator clone =
                    object_anims.find(clone_id);
                if (clone == object_anims.end())
                {
                    object_anims.emplace(clone_id, found->second);
                    changed = true;
                }
                else if (clone->second != found->second)
                {
                    clone->second = found->second;
                    changed = true;
                }
            };

            mirror_one(linkset.mSourceRoot, linkset.mRoot);
            const size_t count = llmin(linkset.mChildren.size(),
                                       linkset.mSourceChildren.size());
            for (size_t i = 0; i < count; ++i)
            {
                mirror_one(linkset.mSourceChildren[i], linkset.mChildren[i]);
            }
        }

        // [AnimeshRepeat] Publish this frame's hold state for
        // repeatHeldAnimesh(). On the held->live edge drop the replay
        // records unconditionally (not tied to `changed`): the returning
        // live signal owns the animation from here, and the ledger mirror
        // above already handed it over through the normal sequence-id path.
        const bool held = !latched_prims.empty();
        if (held != linkset.mAnimeshHeld)
        {
            linkset.mAnimeshHeld = held;
            linkset.mRepeatStates.clear();
            linkset.mRepeatControlAvatarId.setNull();
        }
        linkset.mAnimeshLatchedPrims.swap(latched_prims);

        if (changed)
        {
            if (mAutoAnim) autoAnimAuditLinksetSync(linkset); // [AutoAnimate] Live mirror sync stops.
            root->updateControlAvatar();
        }
      }
    }

    // Re-assert the Studio anim-speed on EVERY animesh control avatar, every
    // frame, in every drive mode. Animesh control avatars are created LAZILY:
    // skin can arrive frames after the clone spawned (finalize accepts an
    // animated linkset with no control avatar yet), and the geometry rebuild
    // then builds the control avatar at the default 1x (llvovolume.cpp:6103);
    // a sim ObjectAnimation refresh routed through updateControlAvatar() above
    // can likewise (re)create one mid-session. A creation-event-gated re-stamp
    // misses the lazy path, leaving that animesh at normal speed while the
    // wearer runs the Studio speed. setAnimTimeFactor is a bare assignment, so
    // the unconditional re-assert is cheaper than tracking creation events.
    if (mEntityAnimTimeFactor != 1.f)
    {
        for (const ClonedLinkset& linkset : mClonedLinksets)
        {
            LLVOVolume* linkset_root = dynamic_cast<LLVOVolume*>(
                gObjectList.findObject(linkset.mRoot));
            LLControlAvatar* control =
                linkset_root ? linkset_root->getControlAvatar() : nullptr;
            if (control && !control->isDead())
            {
                control->setAnimTimeFactor(mEntityAnimTimeFactor);
            }
        }
    }

    // [AnimeshRepeat] After the ledger loop (which published each linkset's
    // hold state) and after the speed re-assert, so a replay starts on a
    // control avatar already running at the Studio speed.
    repeatHeldAnimesh();

    LLVOAvatar::idleUpdate(agent, time);

    // A client-only prim has no later simulator ObjectUpdate to repair it.
    // Record the first frame on which each clone nevertheless has both skin
    // and at least one rigged face, so a delayed repository acquisition is
    // distinguishable from a prim that remains permanently static.
    for (const ClonedLinkset& linkset : mClonedLinksets)
    {
        auto log_healed_once = [this](const LLUUID& clone_id,
                                      const LLUUID& source_id)
        {
            LLVOVolume* clone = dynamic_cast<LLVOVolume*>(
                gObjectList.findObject(clone_id));
            if (!clone || clone->isDead() ||
                mRigHealedLogged.find(clone_id) != mRigHealedLogged.end() ||
                !clone->getSkinInfo() || !has_rigged_face(clone))
            {
                return;
            }

            LLVOVolume* source = dynamic_cast<LLVOVolume*>(
                gObjectList.findObject(source_id));
            log_ghost_rig(clone, source);
            mRigHealedLogged.insert(clone_id);
        };

        log_healed_once(linkset.mRoot, linkset.mSourceRoot);
        const size_t count = llmin(linkset.mChildren.size(),
                                   linkset.mSourceChildren.size());
        for (size_t i = 0; i < count; ++i)
        {
            log_healed_once(linkset.mChildren[i],
                            linkset.mSourceChildren[i]);
        }
    }

    // Re-derive root height after skeleton/appearance evaluation. The desired
    // Studio coordinate is the feet, not whatever transient pelvis height the
    // clone happened to report on its spawn frame.
    //
    // Ownership arbitration (fixes the ee120ac86a7 walk regression): while Actor
    // Mover is actively driving this ghost along a path it OWNS the root, so
    // re-assert its (per-frame idempotent, cached) moving root and SKIP the
    // authored-foot slam -- otherwise the body snaps back to the stationary
    // authored position while the attachments, already baked at the moving root,
    // keep traveling. applyOverride() returns false for a stationary / suspended /
    // dead actor, so a non-walking ghost still gets the persistent foot-lock and
    // keeps its unrigged-attachment-float fix.
    if (!LLActorMover::instance().applyOverride(this))
    {
        applyDesiredGhostFootPosition();

        // Actor Mover owns the WEARER's motion-controller time factor while it
        // drives this ghost (gait cadence keyed to ground speed) and resets it
        // to 1x when the walk stops / suspends / arrives (llactormover.cpp
        // stop()/stopAll()/enterSuspend()/arrival). Those resets know nothing
        // of the Studio anim-speed, so a 2x clone would come back from a walk
        // with its body stuck at 1x while its animesh (re-stamped above) stay
        // at 2x. The mover is not driving on this branch, so re-assert the
        // Studio speed -- a bare assignment, idempotent per frame. During an
        // active walk (the branch below) the gait cadence deliberately wins:
        // never fight Actor Mover mid-walk.
        LLCharacter::setAnimTimeFactor(mEntityAnimTimeFactor);
    }
    else
    {
        // Actor Mover drove the root this frame: chase the viewer-object /
        // drawable to the CURRENT moving root so culling, pixel-area LOD and
        // picking follow the walk instead of evaluating at the stale authored
        // foot. Leaves mDesiredGhostFoot* untouched -- when the walk ends the
        // foot lock above restores the authored Studio placement.
        syncGhostObjectToMovingRoot();
    }
    updateEntityOuterTransform();
}

// ---------------------------------------------------------------------------
// Milestone 1 acceptance gate
// ---------------------------------------------------------------------------

namespace
{
    // Usable rigged skin on one object, or null.
    const LLMeshSkinInfo* skin_of(LLViewerObject* obj)
    {
        LLVOVolume* vol = dynamic_cast<LLVOVolume*>(obj);
        if (!vol || !vol->isRiggedMesh())
        {
            return nullptr;
        }
        const LLMeshSkinInfo* skin = vol->getSkinInfo();
        return (skin && !skin->mJointNames.empty()) ? skin : nullptr;
    }

    // First rigged skin we can find on the source's attachments. Any ONE
    // shared LLMeshSkinInfo is enough -- the point is that both ghosts resolve
    // the SAME skin pointer against their OWN skeletons.
    const LLMeshSkinInfo* find_shared_skin(LLVOAvatar* source)
    {
        if (!source)
        {
            return nullptr;
        }
        for (const auto& ap : source->mAttachmentPoints)
        {
            LLViewerJointAttachment* attachment = ap.second;
            if (!attachment)
            {
                continue;
            }
            for (LLViewerObject* obj : attachment->mAttachedObjects)
            {
                if (!obj)
                {
                    continue;
                }
                // Check the attachment root AND its linkset children: a linked
                // attachment can have an unrigged root with rigged child prims,
                // which would otherwise look like "no rigged mesh worn".
                const LLMeshSkinInfo* skin = skin_of(obj);
                if (skin)
                {
                    return skin;
                }
                for (LLViewerObject* child : obj->getChildren())
                {
                    skin = skin_of(child);
                    if (skin)
                    {
                        return skin;
                    }
                }
            }
        }
        return nullptr;
    }

    // Pick a joint that is ACTUALLY IN THIS SKIN. Rotating a joint the skin
    // does not reference would leave the palette untouched and produce a
    // false negative. Prefer a non-root joint so the change is a real pose
    // difference rather than a whole-body transform.
    std::string pick_test_joint(const LLMeshSkinInfo* skin, LLVOAvatar* av)
    {
        if (!skin || !av)
        {
            return std::string();
        }
        // CRITICAL: only the FIRST getMeshJointCount() names get a palette
        // entry -- updateSkinInfoMatrixPalette sizes the palette with exactly
        // that count (llvoavatar.cpp:10507), and getMeshJointCount caps at
        // LL_MAX_JOINTS_PER_MESH_OBJECT (llskinningutil.cpp:95). A joint past
        // the cap resolves fine on the skeleton but has NO matrix in the
        // palette, so rotating it would change nothing and the test would
        // report a false FAIL.
        const U32 palette_joints = LLSkinningUtil::getMeshJointCount(skin);
        const size_t limit = llmin((size_t)palette_joints, skin->mJointNames.size());

        std::string fallback;
        for (size_t i = 0; i < limit; ++i)
        {
            const std::string& name = skin->mJointNames[i];
            if (!av->getJoint(name))
            {
                continue;
            }
            if (fallback.empty())
            {
                fallback = name;
            }
            if (name != "mPelvis" && name != "mRoot" && name != "mTorso")
            {
                return name;
            }
        }
        return fallback;
    }

    // Returns false if the joint could not be posed. The caller MUST treat
    // that as INCONCLUSIVE, never as a test result: if a freshly created
    // ghost's skeleton is not built yet, getJoint() returns null, the pose
    // silently no-ops, and every palette comes out identical -- which would
    // print FAIL and look exactly like broken pose isolation.
    bool pose_joint(LLVOAvatar* av, const std::string& joint_name, F32 radians)
    {
        if (!av || joint_name.empty())
        {
            return false;
        }
        LLJoint* joint = av->getJoint(joint_name);
        if (!joint)
        {
            LL_WARNS("GhostStudio") << "pose_joint: ghost has no joint '" << joint_name
                                    << "' -- skeleton not built?" << LL_ENDL;
            return false;
        }
        LLQuaternion rot;
        rot.setAngleAxis(radians, 0.f, 1.f, 0.f);
        joint->setRotation(rot);

        // Confirm it actually took, rather than trusting the setter.
        const LLQuaternion actual = joint->getRotation();
        if (radians != 0.f && actual == LLQuaternion())
        {
            LL_WARNS("GhostStudio") << "pose_joint: rotation on '" << joint_name
                                    << "' did not stick" << LL_ENDL;
            return false;
        }
        return true;
    }

    // Every joint the palette will index must resolve on this avatar. If any
    // does not, initSkinningMatrixPalette takes its fallback branch
    // (llskinningutil.cpp:155) and the palette stops being a faithful function
    // of the pose -- so a comparison built on it means nothing. Validating
    // joint resolution directly is definitive; comparing against "pure
    // invBind" would be weaker and could misclassify a legitimate bind pose.
    //
    // Checked by NAME, not by skin->mJointNums: those are filled in lazily by
    // initJointNums() on the FIRST palette build, so before that they are -1
    // and a num-based check would fail spuriously.
    bool palette_joints_resolve(LLVOAvatar* av, const LLMeshSkinInfo* skin)
    {
        if (!av || !skin || !av->isBuilt())
        {
            return false;
        }
        const U32 count = LLSkinningUtil::getMeshJointCount(skin);
        for (U32 j = 0; j < count && j < skin->mJointNames.size(); ++j)
        {
            if (!av->getJoint(skin->mJointNames[j]))
            {
                LL_WARNS("GhostStudio") << "palette joint '" << skin->mJointNames[j]
                                        << "' does not resolve on ghost" << LL_ENDL;
                return false;
            }
        }
        return true;
    }

    size_t count_differing(const std::vector<F32>& a, const std::vector<F32>& b)
    {
        size_t n = 0;
        for (size_t i = 0; i < a.size() && i < b.size(); ++i)
        {
            if (a[i] != b[i])
            {
                n++;
            }
        }
        return n;
    }
}

//static
bool LLGhostAvatar::runPaletteIsolationTest()
{
    LLViewerRegion* region = gAgent.getRegion();
    if (!region)
    {
        LL_WARNS("GhostStudio") << "/ghosttest: no region" << LL_ENDL;
        return false;
    }
    if (!isAgentAvatarValid())
    {
        LL_WARNS("GhostStudio") << "/ghosttest: no agent avatar to clone" << LL_ENDL;
        return false;
    }
    LLVOAvatar* source = (LLVOAvatar*)gAgentAvatarp;

    const LLMeshSkinInfo* skin = find_shared_skin(source);
    if (!skin)
    {
        LL_WARNS("GhostStudio") << "/ghosttest: source has no rigged mesh attachment; "
                                   "wear rigged mesh and retry" << LL_ENDL;
        return false;
    }

    const std::string joint_name = pick_test_joint(skin, source);
    if (joint_name.empty())
    {
        LL_WARNS("GhostStudio") << "/ghosttest: no usable joint in skin; INCONCLUSIVE" << LL_ENDL;
        return false;
    }

    // ---------------------------------------------------------------------
    // THREE ghosts, ALL AT THE SAME WORLD POSITION.
    //
    // The position part is not a detail -- it is the whole validity of the
    // measurement. The palette is invBind * joint WORLD matrix, so ghosts at
    // different positions produce different palettes no matter what their
    // poses are. Comparing palettes across separated ghosts would "pass"
    // even if pose isolation were completely broken. Same transform, vary
    // ONLY the pose.
    //
    //   A, B : identical pose  -> NEGATIVE CONTROL, palettes must MATCH
    //   C    : different pose  -> THE TEST,        palette must DIFFER from A
    //
    // The control is what makes this decisive: it proves the comparison can
    // return "same", so a "differs" result is attributable to the pose alone.
    // Each ghost's palette is read exactly ONCE, because
    // updateSkinInfoMatrixPalette caches per gFrameCount and a second read in
    // the same frame would return the first result.
    // ---------------------------------------------------------------------
    const LLVector3 test_pos = source->getPositionAgent();
    LLGhostAvatar* ghosts[3] = { nullptr, nullptr, nullptr };
    std::vector<LLUUID> spawned;

    // Roll back every ghost created by THIS run if we bail part-way, so a
    // failed run does not leave debris that accumulates across retries.
    auto rollback = [&spawned]()
    {
        for (const LLUUID& id : spawned)
        {
            LLViewerObject* obj = gObjectList.findObject(id);
            LLGhostAvatar* g = dynamic_cast<LLGhostAvatar*>(obj);
            if (g && !g->isDead())
            {
                g->markForDeath();
            }
        }
    };

    for (S32 i = 0; i < 3; ++i)
    {
        LLGhostAvatar* ghost = (LLGhostAvatar*)gObjectList.createObjectViewer(
            LL_PCODE_LEGACY_AVATAR, region, LLViewerObject::CO_FLAG_GHOST_AVATAR);
        if (!ghost)
        {
            LL_WARNS("GhostStudio") << "/ghosttest: failed to create ghost " << i << LL_ENDL;
            rollback();
            return false;
        }
        spawned.push_back(ghost->getID());

        if (!ghost->cloneAppearanceFrom(source))
        {
            LL_WARNS("GhostStudio") << "/ghosttest: appearance copy failed for ghost "
                                    << i << LL_ENDL;
            rollback();
            return false;
        }
        ghost->setGhostPosition(test_pos);

        // NOTE: deliberately NO attachments here. /ghosttest is the palette
        // gate and wants to stay cheap; duplicating a full outfit onto three
        // ghosts is heavy. It also protects the negative control: attachment
        // joint-position overrides mutate the skeleton, so attachments must be
        // applied to ALL ghosts or NONE, or A and B stop being comparable.
        // Use /ghostdress for the dressed-clone test.
        ghosts[i] = ghost;
    }

    // Pose AFTER appearance (appearance application can touch the skeleton).
    // If ANY pose fails to apply, the comparison is meaningless -- bail as
    // INCONCLUSIVE rather than printing a FAIL we cannot trust.
    const bool posed = pose_joint(ghosts[0], joint_name, 0.0f)
                     & pose_joint(ghosts[1], joint_name, 0.0f)   // control: same as A
                     & pose_joint(ghosts[2], joint_name, 1.2f);  // test:    different
    if (!posed)
    {
        LL_WARNS("GhostStudio") << "/ghosttest: INCONCLUSIVE -- could not pose joint '"
                                << joint_name << "' on all three ghosts. This is NOT a "
                                   "failure of pose isolation; the skeletons were not "
                                   "ready. Retry once the ghosts have built." << LL_ENDL;
        rollback();
        return false;
    }

    // Guard the palette's validity BEFORE reading it. Codex verified the
    // skeleton is built synchronously inside createObjectViewer today, so
    // this should always pass -- it exists so that if construction ever
    // becomes deferred, this harness reports INCONCLUSIVE instead of
    // silently degrading into a confident FAIL.
    for (S32 i = 0; i < 3; ++i)
    {
        if (!palette_joints_resolve(ghosts[i], skin))
        {
            LL_WARNS("GhostStudio") << "/ghosttest: INCONCLUSIVE -- ghost " << i
                                    << " is not built or has unresolved palette joints. "
                                       "NOT a pose-isolation failure." << LL_ENDL;
            rollback();
            return false;
        }
    }

    const MatrixPaletteCache& palA = ghosts[0]->updateSkinInfoMatrixPalette(skin);
    const MatrixPaletteCache& palB = ghosts[1]->updateSkinInfoMatrixPalette(skin);
    const MatrixPaletteCache& palC = ghosts[2]->updateSkinInfoMatrixPalette(skin);

    const size_t n = palA.mGLMp.size();
    if (n == 0 || palB.mGLMp.size() != n || palC.mGLMp.size() != n)
    {
        LL_WARNS("GhostStudio") << "/ghosttest: INCONCLUSIVE -- palette sizes "
                                << n << ", " << palB.mGLMp.size() << ", "
                                << palC.mGLMp.size() << LL_ENDL;
        rollback();
        return false;
    }

    const size_t control_diff = count_differing(palA.mGLMp, palB.mGLMp);
    const size_t test_diff    = count_differing(palA.mGLMp, palC.mGLMp);

    LL_INFOS("GhostStudio") << "/ghosttest: skin hash " << skin->mHash
                            << " joints " << skin->mJointNames.size()
                            << " joint '" << joint_name << "'"
                            << " palette floats " << n
                            << " | control diff " << control_diff
                            << " | test diff " << test_diff << LL_ENDL;

    // Commit to the harness store only once the run completed.
    sPaletteTestHarnessGhostIds.insert(sPaletteTestHarnessGhostIds.end(),
                                       spawned.begin(), spawned.end());

    if (control_diff != 0)
    {
        LL_WARNS("GhostStudio") << "/ghosttest: INCONCLUSIVE -- the CONTROL pair differs ("
                                << control_diff << " floats). Two identically posed, "
                                   "identically placed ghosts should be byte-identical; "
                                   "something else is varying." << LL_ENDL;
        return false;
    }
    if (test_diff == 0)
    {
        LL_WARNS("GhostStudio") << "/ghosttest: FAIL -- differently posed ghosts hold "
                                   "IDENTICAL palettes. Either the pose did not take or "
                                   "the palette is not per-entity." << LL_ENDL;
        return true; // test RAN; verdict is FAIL
    }

    LL_INFOS("GhostStudio") << "/ghosttest: PASS -- control identical, posed ghost differs in "
                            << test_diff << " floats. Per-entity pose isolation for a SHARED "
                               "skin is confirmed." << LL_ENDL;
    return true;
}

//static
bool LLGhostAvatar::recheckStructure(LLGhostAvatar* ghost,
                                     LLViewerObject* root,
                                     const ClonedLinkset& linkset)
{
    LLVOVolume* vroot = dynamic_cast<LLVOVolume*>(root);
    if (!vroot)
    {
        return false;
    }

    // STRICT attachment-point lookup, NOT getTargetAttachmentPoint().
    // That helper warns and FALLS BACK TO CHEST on an invalid point, so a
    // corrupted-but-nonzero attachment state could still satisfy
    // isAttachment() + listed + joint-parent and produce a false PASS.
    // Decode the id and require it to resolve directly.
    const S32 point = ATTACHMENT_ID_FROM_STATE(vroot->getAttachmentState());
    attachment_map_t::const_iterator it = ghost->mAttachmentPoints.find(point);
    LLViewerJointAttachment* target =
        (it != ghost->mAttachmentPoints.end()) ? it->second : nullptr;

    const bool listed = target &&
        std::find(target->mAttachedObjects.begin(),
                  target->mAttachedObjects.end(),
                  vroot) != target->mAttachedObjects.end();

    LLXform* xform_parent = vroot->mDrawable.notNull()
        ? vroot->mDrawable->mXform.getParent() : nullptr;
    LLXform* expect_xform = target ? target->getXform() : nullptr;

    // Animated attachments correctly return their LLControlAvatar from
    // getAvatar(), because that is the skeleton used for skinning. Structure
    // belongs to the avatar ancestor, which remains the ghost.
    const bool attach_ok = vroot->isAttachment() && listed
                        && (vroot->getAvatarAncestor() == ghost)
                        && (xform_parent && xform_parent == expect_xform);

    // PASSIVE topology only. Do NOT call getSpatialPartition() here: it is not
    // an observational getter -- it validates and CORRECTS topology, and for a
    // root it can destroy a bad bridge and mint a fresh LLAvatarBridge
    // (lldrawable.cpp:1204). Calling it would let the verifier repair the very
    // defect it exists to detect and then report PASS on it.
    LLSpatialBridge* root_bridge = vroot->mDrawable.notNull()
        ? vroot->mDrawable->getSpatialBridge() : nullptr;

    bool topology_ok = (root_bridge != nullptr);
    S32 children_with_bridge = 0;
    S32 children_bad_parent = 0;
    for (const LLUUID& cid : linkset.mChildren)
    {
        LLViewerObject* c = gObjectList.findObject(cid);
        if (!c || c->isDead() || c->mDrawable.isNull())
        {
            continue;   // counted separately as missing
        }
        // A child must own NO bridge of its own, and must hang off the root's
        // drawable. Both are readable without provoking a repair.
        if (c->mDrawable->getSpatialBridge() != nullptr)
        {
            ++children_with_bridge;
            topology_ok = false;
        }
        if (c->mDrawable->getParent() != vroot->mDrawable)
        {
            ++children_bad_parent;
            topology_ok = false;
        }
    }

    if (!attach_ok || !topology_ok)
    {
        LL_WARNS("GhostStudio")
            << "RECHECK root=" << linkset.mRoot
            << " decoded_point=" << point
            << " target=" << (void*)target
            << " listed=" << listed
            << " attach_ok=" << attach_ok
            << " topology_ok=" << topology_ok
            << " joint_ok=" << (xform_parent && xform_parent == expect_xform)
            << " root_bridge=" << (void*)root_bridge
            << " control_bridge="
            << (dynamic_cast<LLControlAVBridge*>(root_bridge) != nullptr)
            << " animated=" << vroot->isAnimatedObject()
            << " cav=" << (vroot->getControlAvatar() != nullptr)
            << " children_with_bridge=" << children_with_bridge
            << " children_bad_parent=" << children_bad_parent
            << " (structure degraded since cloning)" << LL_ENDL;
    }
    return attach_ok && topology_ok;
}

//static
void LLGhostAvatar::verifyClonedAttachments(bool include_test_harness)
{
    // Deliberately a SEPARATE, USER-TRIGGERED pass rather than part of cloning.
    //
    // setVolume() only SCHEDULES work and force_rebuild() only MARKS a rebuild;
    // rigged classification and face->mAvatar are assigned later, during the
    // pipeline's geometry rebuild (llvovolume.cpp:6072/6131). Checking faces in
    // the cloning frame legitimately sees zero faces and would print what looks
    // like total failure. Run this a moment after /ghostdress.
    S32 ghosts_seen = 0;
    std::vector<LLUUID> verify_ids;
    for (const ALGhostStudio::Instance& inst : ALGhostStudio::instance().getInstances())
    {
        if (inst.mKind == ALGhostStudio::BACKING_ENTITY_CLONE && inst.mEntityId.notNull())
        {
            verify_ids.push_back(inst.mEntityId);
        }
    }
    if (include_test_harness)
    {
        verify_ids.insert(verify_ids.end(), sPaletteTestHarnessGhostIds.begin(),
                          sPaletteTestHarnessGhostIds.end());
    }
    for (const LLUUID& gid : verify_ids)
    {
        LLGhostAvatar* ghost = dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(gid));
        if (!ghost || ghost->isDead())
        {
            continue;
        }
        ghosts_seen++;

        LLViewerObject* appearance_source_obj =
            gObjectList.findObject(ghost->mAnimationSourceId);
        LLVOAvatar* appearance_source =
            appearance_source_obj ? appearance_source_obj->asAvatar() : nullptr;
        for (U8 baked_index = 0; baked_index < LLAvatarAppearanceDefines::BAKED_NUM_INDICES; ++baked_index)
        {
            LLViewerTexture* source_bake =
                appearance_source ? appearance_source->getBakedTexture(baked_index) : nullptr;
            LLViewerTexture* ghost_bake = ghost->getBakedTexture(baked_index);
            LL_INFOS("GhostStudio")
                << "GHOSTBAKE ghost=" << gid
                << " index=" << (S32)baked_index
                << " src=" << (source_bake ? source_bake->getID() : LLUUID::null)
                << " clone=" << (ghost_bake ? ghost_bake->getID() : LLUUID::null)
                << " gl=" << (ghost_bake && ghost_bake->hasGLTexture())
                << " discard=" << (ghost_bake ? ghost_bake->getDiscardLevel() : -99)
                << " missing=" << (ghost_bake && ghost_bake->isMissingAsset())
                << " default=" << (ghost_bake && ghost_bake->getID() == IMG_DEFAULT)
                << LL_ENDL;
        }

        S32 prims = 0, prims_pending = 0;
        S32 faces_total = 0, faces_rigged = 0, faces_wrong_avatar = 0;
        S32 faces_control_avatar = 0;
        S32 skinned_prims = 0, skinned_prims_unrigged = 0;
        S32 missing_roots = 0, missing_children = 0, reparented_children = 0;
        S32 structure_now_bad = 0;
        S32 material_pending = 0, material_mismatches = 0;
        S32 material_unmanaged = 0;
        S32 effective_alpha_divergences = 0;

        for (const LLGhostAvatar::ClonedLinkset& linkset : ghost->mClonedLinksets)
        {
            LLViewerObject* root = gObjectList.findObject(linkset.mRoot);
            if (!root || root->isDead())
            {
                // A recorded root that has since vanished is a real failure,
                // not something to skip past on the way to a PASS.
                missing_roots++;
                continue;
            }

            // Walk the RECORDED prims, not root->getChildren(). Rebuilding the
            // linkset from whoever is still alive is how a dead or detached
            // child disappears without moving any counter -- the survivors then
            // satisfy PASS on a clone that has quietly lost pieces.
            std::vector<LLViewerObject*> chain;
            std::vector<LLViewerObject*> source_chain;
            chain.push_back(root);
            source_chain.push_back(gObjectList.findObject(linkset.mSourceRoot));
            for (size_t child_index = 0;
                 child_index < linkset.mChildren.size(); ++child_index)
            {
                const LLUUID& cid = linkset.mChildren[child_index];
                LLViewerObject* c = gObjectList.findObject(cid);
                if (!c || c->isDead())
                {
                    missing_children++;
                    continue;
                }
                if (c->getParent() != root)
                {
                    reparented_children++;   // still alive, but left its linkset
                    continue;
                }
                chain.push_back(c);
                source_chain.push_back(
                    child_index < linkset.mSourceChildren.size()
                        ? gObjectList.findObject(linkset.mSourceChildren[child_index])
                        : nullptr);
            }

            // Re-check structure NOW, not just at clone time. Attachment
            // registration, joint parenting and partition topology can all be
            // disturbed after cloning, and a stale one-time pass would hide it.
            if (!recheckStructure(ghost, root, linkset))
            {
                structure_now_bad++;
            }

            for (size_t object_index = 0; object_index < chain.size(); ++object_index)
            {
                LLViewerObject* obj = chain[object_index];
                LLVOVolume* vol = dynamic_cast<LLVOVolume*>(obj);
                LLVOVolume* source_vol = object_index < source_chain.size()
                    ? dynamic_cast<LLVOVolume*>(source_chain[object_index]) : nullptr;
                if (!vol)
                {
                    continue;
                }
                prims++;

                const LLVolume* source_mesh = source_vol ? source_vol->getVolume() : nullptr;
                const LLVolume* clone_mesh = vol->getVolume();
                const S32 source_max_lod = source_mesh
                    ? gMeshRepo.getActualMeshLOD(source_mesh->getParams(),
                                                 LLVolumeLODGroup::NUM_LODS - 1)
                    : -1;
                const S32 clone_max_lod = clone_mesh
                    ? gMeshRepo.getActualMeshLOD(clone_mesh->getParams(),
                                                 LLVolumeLODGroup::NUM_LODS - 1)
                    : -1;
                LLVOAvatar* source_rig_avatar =
                    source_vol ? source_vol->getAvatar() : nullptr;
                LLVOAvatar* clone_rig_avatar = vol->getAvatar();
                LL_INFOS("GhostStudio")
                    << "GHOSTLOD src_obj="
                    << (source_vol ? source_vol->getID() : LLUUID::null)
                    << " clone_obj=" << vol->getID()
                    << " src_lod=" << (source_vol ? source_vol->getLOD() : -1)
                    << " clone_lod=" << vol->getLOD()
                    << " src_max_available=" << source_max_lod
                    << " clone_max_available=" << clone_max_lod
                    << " src_animesh="
                    << (source_vol && source_vol->isAnimatedObject())
                    << " clone_animesh=" << vol->isAnimatedObject()
                    << " src_control_avatar="
                    << (source_vol && source_vol->getControlAvatar() != nullptr)
                    << " clone_control_avatar="
                    << (vol->getControlAvatar() != nullptr)
                    << " src_rig_pixel_area="
                    << (source_rig_avatar ? source_rig_avatar->getPixelArea() : -1.f)
                    << " clone_rig_pixel_area="
                    << (clone_rig_avatar ? clone_rig_avatar->getPixelArea() : -1.f)
                    << " src_rig_distance="
                    << (source_rig_avatar && source_rig_avatar->mDrawable.notNull()
                            ? source_rig_avatar->mDrawable->mDistanceWRTCamera : -1.f)
                    << " clone_rig_distance="
                    << (clone_rig_avatar && clone_rig_avatar->mDrawable.notNull()
                            ? clone_rig_avatar->mDrawable->mDistanceWRTCamera : -1.f)
                    << LL_ENDL;

                static LLCachedControl<bool> verify_resolved_materials(
                    gSavedSettings,
                    "GhostUnifiedSourceResolvedMaterials",
                    false);
                if (verify_resolved_materials)
                {
                    if (vol->getGhostResolvedMaterialBindingCount() == 0)
                    {
                        // The setting may have been enabled after this clone
                        // was created. Absence of a snapshot is not corruption.
                        ++material_unmanaged;
                    }
                    else
                    {
                        const ALGhostMaterialResolver::VerifyStatus status =
                            ALGhostMaterialResolver::verifyBindings(
                                source_vol, vol);
                        if (status ==
                            ALGhostMaterialResolver::VerifyStatus::PENDING)
                        {
                            ++material_pending;
                        }
                        else if (status ==
                                 ALGhostMaterialResolver::VerifyStatus::FAIL)
                        {
                            ++material_mismatches;
                        }
                    }
                }

                if (vol->mDrawable.isNull())
                {
                    prims_pending++;   // geometry not built yet -- NOT a failure
                    continue;
                }

                // Source skin is the expectation. Reading the clone here would
                // declare the exact missing-skin failure class legitimately
                // static and allow a false PASS.
                const bool expect_rigged =
                    source_vol && source_vol->getSkinInfo() != nullptr;
                // Normal attachments skin to the ghost. Animated-object
                // linksets skin to their own control avatar by design.
                LLVOAvatar* expected_rig_avatar =
                    vol->isAnimatedObject() ? static_cast<LLVOAvatar*>(vol->getControlAvatar())
                                            : static_cast<LLVOAvatar*>(ghost);
                bool saw_rigged = false;
                S32  non_null_faces = 0;

                for (S32 f = 0; f < vol->mDrawable->getNumFaces(); ++f)
                {
                    LLFace* face = vol->mDrawable->getFace(f);
                    if (!face)
                    {
                        continue;   // slot exists but the face is not populated yet
                    }
                    non_null_faces++;
                    faces_total++;
                    log_texture_face(source_vol, vol, face, f);
                    LLFace* source_face =
                        source_vol && source_vol->mDrawable.notNull() &&
                        f < source_vol->mDrawable->getNumFaces()
                            ? source_vol->mDrawable->getFace(f) : nullptr;
                    if (source_face &&
                        source_face->canRenderAsMask() != face->canRenderAsMask())
                    {
                        ++effective_alpha_divergences;
                    }
                    if (face->isState(LLFace::RIGGED))
                    {
                        faces_rigged++;
                        saw_rigged = true;
                        if (face->mAvatar != expected_rig_avatar)
                        {
                            faces_wrong_avatar++;
                        }
                        else if (expected_rig_avatar != ghost)
                        {
                            faces_control_avatar++;
                        }
                    }
                }

                // Count NON-NULL faces, not face SLOTS: a drawable can hold
                // slots before the faces are populated, and calling that a
                // failure would be a transient false FAIL.
                if (non_null_faces == 0)
                {
                    prims_pending++;
                    continue;
                }
                if (expect_rigged)
                {
                    skinned_prims++;
                    if (!saw_rigged)
                    {
                        skinned_prims_unrigged++;
                    }
                }
            }
        }

        const S32 recorded  = (S32)ghost->mClonedLinksets.size();
        const S32 shortfall = ghost->mCloneExpectedRoots - recorded;

        LL_INFOS("GhostStudio")
            << "GHOSTVERIFY ghost=" << gid
            << " roots expected=" << ghost->mCloneExpectedRoots
            << " recorded=" << recorded
            << " missing_roots=" << missing_roots
            << " missing_children=" << missing_children
            << " reparented_children=" << reparented_children
            << " structure_degraded=" << structure_now_bad
            << " clone_failures=" << ghost->mCloneFailures
            << " | prims=" << prims
            << " pending(no geometry yet)=" << prims_pending
            << " faces=" << faces_total
            << " rigged=" << faces_rigged
            << " rigged_wrong_avatar=" << faces_wrong_avatar
            << " rigged_control_avatar=" << faces_control_avatar
            << " skinned_prims=" << skinned_prims
            << " skinned_prims_NOT_rigged=" << skinned_prims_unrigged
            << " material_mismatches=" << material_mismatches
            << " material_pending=" << material_pending
            << " material_unmanaged=" << material_unmanaged
            << " effective_alpha_divergences="
            << effective_alpha_divergences
            << LL_ENDL;

        // Completeness gates PASS. Checked BEFORE the face verdict: a clone
        // that dropped an attachment must never be able to report PASS just
        // because the pieces that survived happen to be correct.
        if (ghost->mCloneFailures > 0 || shortfall > 0 || missing_roots > 0
            || missing_children > 0 || reparented_children > 0 || structure_now_bad > 0)
        {
            LL_WARNS("GhostStudio")
                << "GHOSTVERIFY: INCOMPLETE -- " << ghost->mCloneFailures
                << " clone failure(s), " << shortfall << " attachment(s) never recorded, "
                << missing_roots << " recorded root(s) now missing, "
                << missing_children << " recorded child(ren) now missing, "
                << reparented_children << " child(ren) left their linkset, "
                << structure_now_bad << " linkset(s) structurally degraded. Cannot PASS: this "
                   "clone is not a faithful copy of the source, whatever the surviving prims say."
                // Surface face-level corruption too, so INCOMPLETE does not
                // hide something the reader would still want to know about.
                << " [also: rigged_wrong_avatar=" << faces_wrong_avatar
                << " skinned_prims_NOT_rigged=" << skinned_prims_unrigged << "]"
                << LL_ENDL;
            continue;
        }

        if (prims_pending > 0)
        {
            LL_INFOS("GhostStudio")
                << "GHOSTVERIFY: PENDING -- " << prims_pending
                << " prim(s) have no geometry yet. Wait for geometry and "
                   "re-run /ghostverify."
                << LL_ENDL;
        }
        else if (material_mismatches > 0)
        {
            LL_WARNS("GhostStudio")
                << "GHOSTVERIFY: FAIL -- " << material_mismatches
                << " prim material snapshot mismatch(es)."
                << LL_ENDL;
        }
        else if (material_pending > 0 || material_unmanaged > 0)
        {
            LL_INFOS("GhostStudio")
                << "GHOSTVERIFY: PENDING -- " << material_pending
                << " prim(s) have unresolved captured channels and "
                << material_unmanaged
                << " prim(s) predate material capture. Retry/respawn after "
                   "source loading completes, then re-run /ghostverify."
                << LL_ENDL;
        }
        else if (skinned_prims == 0)
        {
            LL_WARNS("GhostStudio")
                << "GHOSTVERIFY: INCONCLUSIVE -- no prim carries skin info, so nothing "
                   "here tests rigged skinning." << LL_ENDL;
        }
        else if (skinned_prims_unrigged == 0 && faces_wrong_avatar == 0)
        {
            LL_INFOS("GhostStudio")
                << "GHOSTVERIFY: PASS (structure/rig/material scope) -- every "
                   "skinned prim has rigged faces, every rigged face skins to "
                   "THIS ghost, and no enabled captured-material check failed. "
                   "Effective alpha-mask divergence and GHOSTTEX draw enrollment "
                   "remain diagnostic, not PASS gates."
                << LL_ENDL;
        }
        else
        {
            LL_WARNS("GhostStudio")
                << "GHOSTVERIFY: FAIL -- " << skinned_prims_unrigged
                << " skinned prim(s) produced no rigged face; " << faces_wrong_avatar
                << " rigged face(s) bound to the WRONG avatar."
                << LL_ENDL;
        }
    }

    if (ghosts_seen == 0)
    {
        LL_WARNS("GhostStudio") << "/ghostverify: no live ghosts. Run /ghostdress first."
                                << LL_ENDL;
    }
}

//static
bool LLGhostAvatar::getClonedSourceLOD(const LLVOVolume* volume, S32& source_lod)
{
    if (!volume || !volume->isLocalOnly())
    {
        return false;
    }

    // One hash probe instead of scanning every ghost's linkset graph. The
    // index is maintained by cloneAttachmentsFrom()/releaseClonedAttachments();
    // an id that is not a cloned prim simply misses, exactly as the old search
    // fell through to `return false`.
    const auto it = sClonePrimToSourcePrim.find(volume->getID());
    if (it == sClonePrimToSourcePrim.end() || it->second.isNull())
    {
        return false;
    }

    LLVOVolume* source =
        dynamic_cast<LLVOVolume*>(gObjectList.findObject(it->second));
    if (!source || source->isDead())
    {
        // Source derezzed or the entry is stale: same outcome as a miss.
        return false;
    }

    source_lod = source->getLOD();
    return source_lod >= 0 && source_lod < LLVolumeLODGroup::NUM_LODS;
}

//static
S32 LLGhostAvatar::clearTestHarnessGhosts()
{
    S32 released = 0;
    for (const LLUUID& id : sPaletteTestHarnessGhostIds)
    {
        LLGhostAvatar* ghost = dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(id));
        if (ghost && !ghost->isDead())
        {
            // Explicit attachment teardown before killing the ghost: markDead()
            // does cascade to children, but detaching properly also removes the
            // joint overrides and keeps spatial-partition bookkeeping honest.
            ghost->releaseClonedAttachments();
            ghost->markForDeath();
            released++;
        }
    }
    sPaletteTestHarnessGhostIds.clear();
    LL_INFOS("GhostStudio") << "/ghostclear test: released " << released
                            << " harness ghost(s)" << LL_ENDL;
    return released;
}
