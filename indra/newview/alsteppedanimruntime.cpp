/**
 * @file alsteppedanimruntime.cpp
 * @brief Viewer bridge for the render-window stepped-animation overlay.
 */
#include "llviewerprecompiledheaders.h"

#include "alsteppedanimruntime.h"

#include "alghoststudio.h"
#include "alvcambonepov.h"
#include "fsfloaterposer.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llcallingcard.h"
#include "llcharacter.h"
#include "llcinematiccamera.h"
#include "llcontrolavatar.h"
#include "lldirectorcast.h"
#include "llghostavatar.h"
#include "lljoint.h"
#include "llviewerjointattachment.h"
#include "pipeline.h"
#include "llpresentationtime.h"
#include "llprismlens.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerpartsim.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>

bool ALSteppedAnimRuntime::sActive = false;
bool ALSteppedAnimRuntime::sHasOverlay = false;
bool ALSteppedAnimRuntime::sStepParticles = false;
bool ALSteppedAnimRuntime::sStepTextureAnim = false;

namespace
{
constexpr U8 WRITE_ROT = 1;
constexpr U8 WRITE_POS = 2;
constexpr U8 WRITE_SCALE = 4;

bool rotEqual(const LLQuaternion& a, const LLQuaternion& b)
{
    return std::memcmp(a.mQ, b.mQ, sizeof(a.mQ)) == 0;
}
bool vecEqual(const LLVector3& a, const LLVector3& b)
{
    return std::memcmp(a.mV, b.mV, sizeof(a.mV)) == 0;
}

LLUUID runtimeActorId(const LLVOAvatar* avatar)
{
    if (!avatar || avatar->isSelf()) return LLUUID::null;
    return LLDirectorCast::canonicalActorId(avatar->getID());
}

F32 finiteSetting(F32 value, F32 fallback)
{
    return std::isfinite(value) ? value : fallback;
}
}

// Local friend-list snapshot for the Friends scope: rebuilt only when the tracker reports a
// change (add / remove / online), never per actor per frame. Nothing is sent anywhere.
class ALSteppedFriendCache final : public LLFriendObserver
{
public:
    ALSteppedFriendCache() { LLAvatarTracker::instance().addObserver(this); }
    // The tracker is a namespace-scope static built before this lazily-created runtime, so it
    // outlives it at exit.
    ~ALSteppedFriendCache() override { LLAvatarTracker::instance().removeObserver(this); }
    void changed(U32) override { mDirty = true; }
    bool isFriend(const LLUUID& id)
    {
        if (mDirty)
        {
            LLAvatarTracker::buddy_map_t buddies;
            LLAvatarTracker::instance().copyBuddyList(buddies);
            mFriends.clear();
            for (const auto& buddy : buddies) mFriends.insert(buddy.first);
            mDirty = false;
        }
        return mFriends.find(id) != mFriends.end();
    }

private:
    std::set<LLUUID> mFriends;
    bool mDirty = true;
};

ALSteppedAnimRuntime::~ALSteppedAnimRuntime() = default;

void ALSteppedAnimRuntime::refreshCastCache(const LLDirectorCast& cast)
{
    const U64 scene = cast.getSceneRevision();
    const U64 members = cast.getCastRevision();
    if (mCastCache.valid && mCastCache.sceneRevision == scene && mCastCache.castRevision == members) return;
    mCastCache.valid = true;
    mCastCache.sceneRevision = scene;
    mCastCache.castRevision = members;
    mCastCache.order.clear();
    mCastCache.groupOffsets.clear();
    const auto& roster = cast.getCast();
    for (U32 i = 0; i < roster.size(); ++i)
    {
        mCastCache.order[roster[i].mId] = static_cast<S32>(i);
        if (roster[i].mGroup.empty()) continue;
        const S32 offset = cast.getGroupRate(roster[i].mGroup);
        if (offset > 0) mCastCache.groupOffsets[roster[i].mId] = offset;
    }
}

ALSteppedAnimRuntime& ALSteppedAnimRuntime::instance()
{
    static ALSteppedAnimRuntime runtime;
    return runtime;
}

void ALSteppedAnimRuntime::initialize()
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    // Called after initConfiguration(): preserve the shipped attachment-particle hold once.
    bool particles = gSavedSettings.getBOOL("StepAnimParticles");
    bool migrated = gSavedSettings.getBOOL("StepAnimParticlesMigrated");
    if (ALSteppedAnim::migrateParticleSetting(gSavedSettings.getBOOL("StepAnimAttachFX"), particles, migrated))
    {
        gSavedSettings.setBOOL("StepAnimParticles", particles);
        gSavedSettings.setBOOL("StepAnimParticlesMigrated", migrated);
    }
    sActive = gSavedSettings.getBOOL("StepAnimEnabled");
    auto refresh = [](LLControlVariable*, const LLSD& value, const LLSD&)
    {
        ALSteppedAnimRuntime::sActive = value.asBoolean();
        if (!ALSteppedAnimRuntime::sActive && ALSteppedAnimRuntime::sHasOverlay)
            ALSteppedAnimRuntime::instance().endFrameRestore();
        if (!ALSteppedAnimRuntime::sActive)
            ALSteppedAnimRuntime::instance().deactivate();
    };
    gSavedSettings.getControl("StepAnimEnabled")->getSignal()->connect(refresh);
    gSavedSettings.getControl("StepAnimDebugVerify")->getSignal()->connect(
        [](LLControlVariable*, const LLSD& value, const LLSD&)
        {
            if (value.asBoolean()) return;
            for (LLCharacter* character : LLCharacter::sInstances)
                static_cast<LLVOAvatar*>(character)->clearStepAnimRenderState();
        });
    const char* const roto_controls[] = {
        "StepAnimEnabled", "StepAnimMatchRotoInk", "StepAnimRate", "StepAnimMode"
    };
    for (const char* name : roto_controls)
        gSavedSettings.getControl(name)->getSignal()->connect(
            [](LLControlVariable*, const LLSD&, const LLSD&)
            { ALSteppedAnimRuntime::instance().updateRotoMatch(); });
    gSavedSettings.getControl("StepAnimParticles")->getSignal()->connect(
        [](LLControlVariable*, const LLSD& value, const LLSD&)
        {
            if (value.asBoolean()) return;
            ALSteppedAnimRuntime::sStepParticles = false;
            LLViewerPartSim::clearStepAnimParticleDebt();
        });
    gSavedSettings.getControl("StepAnimTextureAnim")->getSignal()->connect(
        [](LLControlVariable*, const LLSD& value, const LLSD&)
        {
            if (!value.asBoolean()) ALSteppedAnimRuntime::sStepTextureAnim = false;
        });
    instance().updateRotoMatch();
}

ALSteppedAnim::Config ALSteppedAnimRuntime::readConfig() const
{
    static LLCachedControl<S32> scope(gSavedSettings, "StepAnimScope");
    static LLCachedControl<bool> include_ghosts(gSavedSettings, "StepAnimIncludeGhosts");
    static LLCachedControl<bool> exclude(gSavedSettings, "StepAnimExcludeCameraMounted");
    static LLCachedControl<S32> maximum(gSavedSettings, "StepAnimMaxAvatars");
    static LLCachedControl<S32> mode(gSavedSettings, "StepAnimMode");
    static LLCachedControl<F32> rate(gSavedSettings, "StepAnimRate");
    static LLCachedControl<F32> fast(gSavedSettings, "StepAnimFastRate");
    static LLCachedControl<F32> threshold(gSavedSettings, "StepAnimAdaptThreshold");
    static LLCachedControl<F32> hysteresis(gSavedSettings, "StepAnimAdaptHysteresis");
    static LLCachedControl<F32> dwell(gSavedSettings, "StepAnimAdaptMinDwell");
    static LLCachedControl<F32> background(gSavedSettings, "StepAnimBackgroundRate");
    static LLCachedControl<S32> what(gSavedSettings, "StepAnimWhatSteps");
    static LLCachedControl<bool> face(gSavedSettings, "StepAnimFace");
    static LLCachedControl<bool> gaze(gSavedSettings, "StepAnimGaze");
    static LLCachedControl<bool> flexi(gSavedSettings, "StepAnimFlexi");
    static LLCachedControl<bool> attachment_fx(gSavedSettings, "StepAnimAttachFX");
    static LLCachedControl<bool> particles(gSavedSettings, "StepAnimParticles");
    static LLCachedControl<bool> texture_anim(gSavedSettings, "StepAnimTextureAnim");
    static LLCachedControl<S32> clock(gSavedSettings, "StepAnimClock");
    static LLCachedControl<S32> offset(gSavedSettings, "StepAnimOffsetMode");
    static LLCachedControl<F32> phase(gSavedSettings, "StepAnimPhase");
    static LLCachedControl<S32> base_fps(gSavedSettings, "StepAnimBaseFPS");
    static LLCachedControl<bool> boil(gSavedSettings, "StepAnimBoil");
    static LLCachedControl<F32> boil_amount(gSavedSettings, "StepAnimBoilAmount");
    static LLCachedControl<S32> boil_ticks(gSavedSettings, "StepAnimBoilTicks");
    static LLCachedControl<bool> impact(gSavedSettings, "StepAnimImpactHold");
    static LLCachedControl<S32> impact_ticks(gSavedSettings, "StepAnimImpactTicks");
    static LLCachedControl<F32> impact_threshold(gSavedSettings, "StepAnimImpactThreshold");
    static LLCachedControl<bool> smear(gSavedSettings, "StepAnimSmear");
    static LLCachedControl<F32> smear_strength(gSavedSettings, "StepAnimSmearStrength");
    static LLCachedControl<F32> smear_threshold(gSavedSettings, "StepAnimSmearThreshold");
    static LLCachedControl<bool> ease(gSavedSettings, "StepAnimEase");
    static LLCachedControl<F32> ease_amount(gSavedSettings, "StepAnimEaseAmount");
    static LLCachedControl<bool> match_roto(gSavedSettings, "StepAnimMatchRotoInk");
    static LLCachedControl<F32> nearby_radius(gSavedSettings, "StepAnimNearbyRadius");
    static LLCachedControl<bool> limit_distance(gSavedSettings, "StepAnimLimitDistance");
    static LLCachedControl<bool> include_self(gSavedSettings, "StepAnimIncludeSelf");
    static LLCachedControl<U32> seed(gSavedSettings, "StepAnimSeed");
    static LLCachedControl<F32> phase_spread(gSavedSettings, "StepAnimPhaseSpread");
    static LLCachedControl<F32> rate_variation(gSavedSettings, "StepAnimRateVariation");
    static LLCachedControl<F32> timing_jitter(gSavedSettings, "StepAnimTimingJitter");
    static LLCachedControl<S32> part_head(gSavedSettings, "StepAnimPartHead");
    static LLCachedControl<S32> part_face(gSavedSettings, "StepAnimPartFace");
    static LLCachedControl<S32> part_arms(gSavedSettings, "StepAnimPartArms");
    static LLCachedControl<S32> part_hands(gSavedSettings, "StepAnimPartHands");
    static LLCachedControl<S32> part_legs(gSavedSettings, "StepAnimPartLegs");
    static LLCachedControl<S32> part_extras(gSavedSettings, "StepAnimPartExtras");
    static LLCachedControl<S32> part_attach(gSavedSettings, "StepAnimPartAttach");
    ALSteppedAnim::Config c;
    c.includeGhosts = include_ghosts;
    c.enabled = true; c.scope = scope; c.excludeCameraMounted = exclude; c.maxAvatars = maximum;
    c.mode = mode; c.rate = finiteSetting(rate, 12.f); c.fastRate = finiteSetting(fast, 24.f);
    c.adaptThreshold = finiteSetting(threshold, 1.5f); c.adaptHysteresis = finiteSetting(hysteresis, .3f);
    c.adaptMinDwell = finiteSetting(dwell, .25f); c.backgroundRate = finiteSetting(background, 8.f);
    c.what = what; c.face = face; c.gaze = gaze; c.flexi = flexi; c.attachFX = attachment_fx;
    c.particles = particles; c.textureAnim = texture_anim;
    c.clock = clock; c.offset = offset; c.phase = finiteSetting(phase, 0.f); c.baseFps = base_fps;
    c.boil = boil; c.boilAmount = finiteSetting(boil_amount, .6f); c.boilTicks = boil_ticks;
    c.impact = impact; c.impactTicks = impact_ticks; c.impactThreshold = finiteSetting(impact_threshold, 25.f);
    c.smear = smear; c.smearStrength = finiteSetting(smear_strength, .5f);
    c.smearThreshold = finiteSetting(smear_threshold, 2.5f); c.ease = ease;
    c.easeAmount = finiteSetting(ease_amount, .5f); c.matchRoto = match_roto;
    c.nearbyRadius = finiteSetting(nearby_radius, 20.f); c.limitDistance = limit_distance;
    c.includeSelf = include_self; c.seed = seed;
    c.phaseSpread = finiteSetting(phase_spread, 1.f); c.rateVariation = finiteSetting(rate_variation, 0.f);
    c.timingJitter = finiteSetting(timing_jitter, 0.f);
    c.parts[ALSteppedAnim::PART_HEAD] = part_head; c.parts[ALSteppedAnim::PART_FACE] = part_face;
    c.parts[ALSteppedAnim::PART_ARMS] = part_arms; c.parts[ALSteppedAnim::PART_HANDS] = part_hands;
    c.parts[ALSteppedAnim::PART_LEGS] = part_legs; c.parts[ALSteppedAnim::PART_EXTRAS] = part_extras;
    c.parts[ALSteppedAnim::PART_ATTACH] = part_attach;
    return ALSteppedAnim::sanitize(c);
}

bool ALSteppedAnimRuntime::eligible(const LLVOAvatar* avatar) const
{
    // getRootJoint()/isImpostor() are non-const in LLAvatarAppearance/LLVOAvatar.
    LLVOAvatar* mutable_avatar = const_cast<LLVOAvatar*>(avatar);
    return avatar && !avatar->isDead() && avatar->isBuilt() && mutable_avatar->getRootJoint() &&
        avatar->isVisible() && !mutable_avatar->isImpostor() &&
        !avatar->isUIAvatar() && avatar->mSpecialRenderMode == 0;
}

void ALSteppedAnimRuntime::collectActors()
{
    std::vector<ALSteppedAnim::ActorFacts> facts;
    std::vector<LLUUID> ids;
    std::vector<LLUUID> attached_wearers;
    const LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID selected = cast.getSteppedActorId();
    const bool empty_cast = cast.getIds().empty();
    refreshCastCache(cast);
    const std::map<LLUUID, S32>& cast_order = mCastCache.order;
    const std::map<LLUUID, S32>& group_offsets = mCastCache.groupOffsets;
    const bool friends_scope = mConfig.scope == ALSteppedAnim::SCOPE_FRIENDS;
    if (!friends_scope) mFriends.reset();
    std::set<LLUUID> bone_pov_anchors;
    const LLPrismLens::RegistrySnapshot camera_registry = LLPrismLens::registrySnapshot();
    for (U32 index = 0; index < camera_registry.mSlotCount; ++index)
    {
        const LLPrismLens::CaptureDefinition& capture = camera_registry.mCaptures[index];
        if (!capture.isCameraFeed()) continue;
        const LLPrismLens::BonePovSettings& settings = capture.cameraData()->mSettings.mBonePov;
        if (capture.isCameraFeed() &&
            capture.cameraData()->mSettings.mVirtual && settings.mEnabled)
        {
            LLVOAvatar* anchor = ALVCamBonePov::resolveAnchorAvatar(settings.mAnchorSlot);
            if (anchor) bone_pov_anchors.insert(anchor->getID());
        }
    }
    const LLVector3d camera = gAgent.getPosGlobalFromAgent(LLViewerCamera::getInstance()->getOrigin());
    for (LLCharacter* character : LLCharacter::sInstances)
    {
        LLVOAvatar* avatar = static_cast<LLVOAvatar*>(character);
        if (!eligible(avatar)) continue;
        const LLUUID actor_id = runtimeActorId(avatar);
        ALSteppedAnim::ActorFacts f;
        f.seed = ALSteppedAnim::mixSeed((actor_id.isNull() ? gAgentID : actor_id).getDigest64(), mConfig.seed);
        f.isSelf = avatar->isSelf();
        // Self is stored under gAgentID when it is a cast member (auto-added at login);
        // with an empty cast, "Whole cast" means you.
        const bool self_in_cast = f.isSelf && gAgentID.notNull() && cast.containsStored(gAgentID);
        f.inCast = f.isSelf ? (empty_cast || self_in_cast) : cast.containsStored(actor_id);
        f.isSelected = selected.isNull() ? f.isSelf : actor_id == selected;
        f.drawn = true;
        if (avatar->isGhostAvatar())
        {
            const S32 drive = static_cast<LLGhostAvatar*>(avatar)->getEntityDriveMode();
            // Only a clone that PLAYS its own animation (Directed) counts. Mirror and
            // True mirror copy the source; Frozen holds one pose (dormant), so it has
            // nothing to step and must not take a StepAnimMaxAvatars slot.
            f.ghostOwnAnimation = drive == ALGhostStudio::DRIVE_DIRECTED;
        }
        f.cameraMounted = (f.isSelf && gAgentCamera.cameraMouselook()) ||
            LLCinematicCamera::instance().isActiveBoneLockTarget(avatar->getID()) ||
            bone_pov_anchors.find(avatar->getID()) != bone_pov_anchors.end();
        f.posing = FSFloaterPoser::isEditingAvatar(avatar);
        f.cameraDistance = static_cast<F32>((avatar->getPositionGlobal() - camera).length());
        if (friends_scope && !f.isSelf)
        {
            if (!mFriends) mFriends.reset(new ALSteppedFriendCache);
            f.isFriend = mFriends->isFriend(avatar->getID());
        }
        const auto order = cast_order.find(f.isSelf ? gAgentID : actor_id);
        if (order != cast_order.end()) f.castOrder = order->second;
        else if (f.isSelf && empty_cast) f.castOrder = 0;
        const LLDirectorCast::ActorStyle::SteppedOverride& ov = cast.getActorStyle(actor_id).mStepped;
        f.ov.mode = ov.mMode; f.ov.rate = ov.mRate; f.ov.what = ov.mWhat; f.ov.role = ov.mRole;
        f.ov.rateOffset = ov.mRateOffset;
        f.ov.parts = ov.mParts;
        if (!group_offsets.empty())
        {
            const auto group = group_offsets.find(f.isSelf ? gAgentID : actor_id);
            if (group != group_offsets.end()) f.groupOffset = group->second;
        }
        ids.push_back(avatar->getID());
        attached_wearers.push_back(avatar->isControlAvatar() && avatar->getAttachedAvatar()
            ? avatar->getAttachedAvatar()->getID() : LLUUID::null);
        facts.push_back(f);
    }
    std::vector<ALSteppedAnim::Assignment> assignments;
    assignments.reserve(facts.size());
    const F64 roto_phase = mConfig.matchRoto
        ? std::fmod(gPipeline.getRotoAnimClockOffset() * mConfig.rate, 1.) : 0.;
    for (const auto& f : facts) assignments.push_back(ALSteppedAnim::resolveActor(mConfig, f, roto_phase));

    std::map<LLUUID, U32> actor_indices;
    for (U32 i = 0; i < ids.size(); ++i) actor_indices[ids[i]] = i;
    std::vector<S32> parents(ids.size(), -1);
    std::vector<std::vector<U32>> children(ids.size());
    for (U32 i = 0; i < ids.size(); ++i)
    {
        const auto wearer = actor_indices.find(attached_wearers[i]);
        if (wearer == actor_indices.end()) continue;
        parents[i] = static_cast<S32>(wearer->second);
        children[wearer->second].push_back(i);
    }
    std::vector<U32> ordered;
    std::vector<bool> emitted(ids.size(), false);
    ordered.reserve(ids.size());
    for (U32 i = 0; i < ids.size(); ++i)
        if (parents[i] < 0)
        {
            emitted[i] = true;
            ordered.push_back(i);
        }
    for (U32 cursor = 0; cursor < ordered.size(); ++cursor)
        for (U32 child : children[ordered[cursor]])
        {
            if (emitted[child]) continue;
            emitted[child] = true;
            ordered.push_back(child);
        }
    // Malformed attachment cycles are not expected, but retain their actors
    // deterministically without recursive traversal.
    for (U32 i = 0; i < ids.size(); ++i)
        if (!emitted[i]) ordered.push_back(i);

    for (U32 i : ordered)
    {
        LLViewerObject* object = gObjectList.findObject(ids[i]);
        LLVOAvatar* avatar = object ? object->asAvatar() : nullptr;
        if (!avatar || !avatar->isGhostAvatar()) continue;
        LLGhostAvatar* ghost = static_cast<LLGhostAvatar*>(avatar);
        const LLUUID source = ghost->getAnimationSourceId();
        const auto source_id = actor_indices.find(source);
        if (source_id != actor_indices.end())
        {
            const U32 source_index = source_id->second;
            if (assignments[source_index].stepped &&
                ALSteppedAnim::ghostMayInherit(mConfig, facts[i]))
            {
                assignments[i] = assignments[source_index];
                facts[i].seed = facts[source_index].seed;
                assignments[i].phase = assignments[source_index].phase;
            }
        }
    }
    // A worn animesh follows its wearer on the wearer's Attachments part rate (Same: the wearer's
    // grid exactly; +1 / +2: that much coarser; Smooth: left smooth).
    for (U32 i : ordered)
    {
        if (parents[i] < 0) continue;
        const U32 wearer = static_cast<U32>(parents[i]);
        ALSteppedAnim::inheritAttachedAssignment(
            assignments[wearer], facts[wearer].seed, assignments[i], facts[i].seed,
            assignments[wearer].parts[ALSteppedAnim::PART_ATTACH], mConfig.baseFps);
    }
    const S32 before = static_cast<S32>(std::count_if(assignments.begin(), assignments.end(),
        [](const ALSteppedAnim::Assignment& a) { return a.stepped; }));
    ALSteppedAnim::applyActorCap(mConfig, facts, assignments);
    // A worn animesh is part of its wearer, not an independently capped crowd
    // actor. Re-apply the dependency after the actor cap so the cap boundary can
    // never leave the visible attachment animating smoothly on a stepped wearer.
    for (U32 i : ordered)
    {
        if (parents[i] < 0) continue;
        const U32 wearer = static_cast<U32>(parents[i]);
        ALSteppedAnim::inheritAttachedAssignment(
            assignments[wearer], facts[wearer].seed, assignments[i], facts[i].seed,
            assignments[wearer].parts[ALSteppedAnim::PART_ATTACH], mConfig.baseFps);
    }
    mPlanned.clear(); mActiveActors = 0;
    for (U32 i : ordered)
        if (assignments[i].stepped)
        {
            Entry& entry = mEntries[ids[i]];
            if (!entry.mParticleEpoch) entry.mParticleEpoch = ++mNextParticleEpoch;
            // Copy the wearer's frame only when the wearer itself is stepped this
            // frame. A camera-mounted, capped or "Never" wearer has no planned
            // entry, so the animesh must plan its own frame (else it freezes on a
            // stale held pose while the wearer moves smoothly).
            // On a coarser Attachments part the animesh still takes its frames from the wearer
            // (attachedDecision: the wearer's Attachments grid, Adaptive choice and impact holds).
            const bool inherit_from_wearer = parents[i] >= 0 &&
                assignments[static_cast<U32>(parents[i])].stepped;
            entry.mDecisionSource = inherit_from_wearer
                ? ids[static_cast<U32>(parents[i])] : LLUUID::null;
            if (inherit_from_wearer)
            {
                Entry& wearer = mEntries[ids[static_cast<U32>(parents[i])]];
                ALSteppedAnim::inheritAttachedFrame(
                    wearer.mAssignment, wearer.mSeed, wearer.mState, wearer.mDecision,
                    entry.mAssignment, entry.mSeed, entry.mState, entry.mDecision,
                    wearer.mAssignment.parts[ALSteppedAnim::PART_ATTACH], mConfig.baseFps);
            }
            else
            {
                entry.mSeed = facts[i].seed;
                entry.mAssignment = assignments[i];
                entry.mDecision = ALSteppedAnim::planFrame(entry.mState, entry.mAssignment, mConfig, mTime);
            }
            if (mConfig.particles && !entry.mDecision.hold)
                entry.mParticleEpoch = ++mNextParticleEpoch;
            mPlanned.push_back(ids[i]);
            ++mActiveActors;
        }
    mSkippedActors = before - mActiveActors;
    for (auto it = mEntries.begin(); it != mEntries.end();)
    {
        if (std::find(mPlanned.begin(), mPlanned.end(), it->first) == mPlanned.end())
            it = mEntries.erase(it);
        else
            ++it;
    }
}

void ALSteppedAnimRuntime::beginFrame()
{
    LL_PROFILE_ZONE_NAMED("StepAnim::beginFrame");
    mPaletteRebuilds = 0;
    static LLCachedControl<bool> verify(gSavedSettings, "StepAnimDebugVerify");
    if (verify) verifyRestored("idle-top");
    mConfig = readConfig();
    const LLTemporalFrameContext& frame = LLPresentationTime::currentFrame();
    mTime = mConfig.clock == ALSteppedAnim::CLOCK_REAL ? frame.wall_time : frame.presentation_time;
    collectActors();
    const bool step_particles = mConfig.particles && mActiveActors > 0;
    if (sStepParticles && !step_particles) LLViewerPartSim::clearStepAnimParticleDebt();
    sStepParticles = step_particles;
    sStepTextureAnim = mConfig.textureAnim && mActiveActors > 0;
}

void ALSteppedAnimRuntime::buildClasses(LLVOAvatar* avatar, Entry& entry)
{
    std::vector<ALSteppedAnim::JointDesc> desc;
    desc.reserve(avatar->getSkeletonJointCount() + avatar->getCollisionVolumeJointCount());
    std::map<const LLJoint*, S32> indices;
    entry.mEffectorIndices.fill(-1);
    for (auto& chain : entry.mSmearIndices) chain.fill(-1);
    for (auto& chain : entry.mSmearChildIndices) chain.fill(-1);
    entry.mPelvisIndex = -1;
    static const char* const effectors[ALSteppedAnim::kEffectors] = {
        "mWristLeft", "mWristRight", "mAnkleLeft", "mAnkleRight", "mHead"
    };
    static const char* const smear_joints[4][2] = {
        {"mShoulderLeft", "mElbowLeft"}, {"mShoulderRight", "mElbowRight"},
        {"mHipLeft", "mKneeLeft"}, {"mHipRight", "mKneeRight"}
    };
    static const char* const smear_children[4][2] = {
        {"mElbowLeft", "mWristLeft"}, {"mElbowRight", "mWristRight"},
        {"mKneeLeft", "mAnkleLeft"}, {"mKneeRight", "mAnkleRight"}
    };
    for (S32 i = 0; i < static_cast<S32>(avatar->getSkeletonJointCount()); ++i)
    {
        LLJoint* joint = avatar->getSkeletonJoint(i);
        const std::string& name = joint->getName();
        if (name == "mPelvis") entry.mPelvisIndex = i;
        for (U32 e = 0; e < ALSteppedAnim::kEffectors; ++e)
            if (name == effectors[e]) entry.mEffectorIndices[e] = i;
        for (U32 chain = 0; chain < 4; ++chain)
            for (U32 j = 0; j < 2; ++j)
            {
                if (name == smear_joints[chain][j]) entry.mSmearIndices[chain][j] = i;
                if (name == smear_children[chain][j]) entry.mSmearChildIndices[chain][j] = i;
            }
        indices[joint] = i;
        ALSteppedAnim::JointDesc d;
        d.name = joint->getName();
        const auto parent = indices.find(joint->getParent());
        d.parent = parent == indices.end() ? -1 : parent->second;
        desc.push_back(d);
    }
    // Collision volumes follow their parent bone's part group.
    for (S32 i = 0; i < avatar->getCollisionVolumeJointCount(); ++i)
    {
        LLJoint* joint = avatar->getCollisionVolumeJoint(i);
        ALSteppedAnim::JointDesc d; d.name = joint->getName(); d.collisionVolume = true;
        const auto parent = indices.find(joint->getParent());
        d.parent = parent == indices.end() ? -1 : parent->second;
        desc.push_back(d);
    }
    // Non-HUD attachment points (the Attachments part), in attachment-id order.
    entry.mAttachPointIds.clear();
    for (const auto& item : avatar->mAttachmentPoints)
    {
        LLViewerJointAttachment* point = item.second;
        if (!point || point->getIsHUDAttachment()) continue;
        entry.mAttachPointIds.push_back(item.first);
        ALSteppedAnim::JointDesc d; d.name = point->getName(); d.attachment = true;
        const auto parent = indices.find(point->getParent());
        d.parent = parent == indices.end() ? -1 : parent->second;
        desc.push_back(d);
    }
    entry.mClasses = ALSteppedAnim::classifyJoints(desc);
    entry.mParents.resize(desc.size());
    for (size_t i = 0; i < desc.size(); ++i) entry.mParents[i] = desc[i].parent;
    entry.mAttachHeld.assign(desc.size(), ALSteppedAnim::JointLocal());
    entry.mJointCount = static_cast<U32>(desc.size());
}

U32 ALSteppedAnimRuntime::attachPointCount(const LLVOAvatar* avatar)
{
    U32 count = 0;
    for (const auto& item : avatar->mAttachmentPoints)
        if (item.second && !item.second->getIsHUDAttachment()) ++count;
    return count;
}

U32 ALSteppedAnimRuntime::poseJointCount(const LLVOAvatar* avatar)
{
    return static_cast<U32>(avatar->getSkeletonJointCount()) +
        static_cast<U32>(avatar->getCollisionVolumeJointCount()) + attachPointCount(avatar);
}

// Pose-buffer index -> joint: skeleton, collision volumes, then the entry's attachment points.
LLJoint* ALSteppedAnimRuntime::poseJoint(LLVOAvatar* avatar, const Entry& entry, U32 index)
{
    const U32 skeleton = static_cast<U32>(avatar->getSkeletonJointCount());
    const U32 volumes = static_cast<U32>(avatar->getCollisionVolumeJointCount());
    if (index < skeleton) return avatar->getSkeletonJoint(static_cast<S32>(index));
    if (index < skeleton + volumes) return avatar->getCollisionVolumeJoint(static_cast<S32>(index - skeleton));
    const U32 attach = index - skeleton - volumes;
    if (attach >= entry.mAttachPointIds.size()) return nullptr;
    const auto found = avatar->mAttachmentPoints.find(entry.mAttachPointIds[attach]);
    return found == avatar->mAttachmentPoints.end() ? nullptr : found->second;
}

void ALSteppedAnimRuntime::readPose(LLVOAvatar* avatar, Entry& entry,
                                    ALSteppedAnim::PoseBuffer& out)
{
    const U32 count = poseJointCount(avatar);
    if (entry.mClasses.size() != count) buildClasses(avatar, entry);
    out.joints.resize(count);
    for (U32 i = 0; i < count; ++i)
    {
        LLJoint* joint = poseJoint(avatar, entry, i);
        if (!joint) continue;
        out.joints[i].rot = joint->getRotation();
        out.joints[i].pos = joint->getPosition();
        out.joints[i].scale = joint->getScale();
    }
    out.rootPos = avatar->getRootJoint()->getPosition();
    out.rootRot = avatar->getRootJoint()->getRotation();
}

ALSteppedAnim::LimbFrame ALSteppedAnimRuntime::limbFrame(
    LLVOAvatar* avatar, const Entry& entry, F64 time) const
{
    ALSteppedAnim::LimbFrame frame; frame.t = time;
    LLJoint* pelvis = entry.mPelvisIndex >= 0 ? avatar->getSkeletonJoint(entry.mPelvisIndex) : nullptr;
    const LLVector3 pelvis_pos = pelvis ? pelvis->getWorldPosition() : avatar->getRootJoint()->getWorldPosition();
    const LLQuaternion inverse_root = ~avatar->getRootJoint()->getWorldRotation();
    for (U32 i = 0; i < ALSteppedAnim::kEffectors; ++i)
    {
        LLJoint* joint = entry.mEffectorIndices[i] >= 0 ? avatar->getSkeletonJoint(entry.mEffectorIndices[i]) : nullptr;
        frame.p[i] = joint ? (joint->getWorldPosition() - pelvis_pos) * inverse_root : LLVector3::zero;
    }
    return frame;
}

void ALSteppedAnimRuntime::updateEaseCandidate(Entry& entry,
    const ALSteppedAnim::LimbFrame&, F64 time)
{
    if (!mConfig.ease || entry.mDecision.rate < 2.f)
    {
        entry.mLastEaseTick = std::numeric_limits<S64>::min();
        return;
    }
    const F64 phase = entry.mAssignment.phase;
    const S64 current_tick = static_cast<S64>(std::floor(
        (time * entry.mDecision.rate + phase + ALSteppedAnim::kTickEps) *
        ALSteppedAnim::ActorState::kEaseSubsteps));
    if (entry.mLastEaseTick == std::numeric_limits<S64>::min())
    {
        entry.mLastEaseTick = current_tick;
        return;
    }
    const S64 final_tick = current_tick - (entry.mDecision.capture ? 1 : 0);
    const S64 first_tick = llmax(entry.mLastEaseTick + 1,
        final_tick - static_cast<S64>(ALSteppedAnim::ActorState::kEaseSubsteps) + 1);
    for (S64 sub_tick = first_tick; sub_tick <= final_tick; ++sub_tick)
    {
        const S32 sub = static_cast<S32>((sub_tick % ALSteppedAnim::ActorState::kEaseSubsteps +
            ALSteppedAnim::ActorState::kEaseSubsteps) % ALSteppedAnim::ActorState::kEaseSubsteps);
        const F64 t_sub = (static_cast<F64>(sub_tick) /
            ALSteppedAnim::ActorState::kEaseSubsteps - phase) / entry.mDecision.rate;
        const F32 speed = ALSteppedAnim::speedAt(entry.mState.limbs, t_sub);
        entry.mState.subSeen[sub] = true;
        entry.mState.subSpeed[sub] = speed;
        if (!entry.mHaveCandidate || speed < entry.mCandidateSpeed)
        {
            // The first rendered pose at/after this fixed clock-grid sub-step.
            entry.mCandidate = entry.mSmooth;
            entry.mCandidateSpeed = speed;
            entry.mState.candidateSub = sub;
            entry.mHaveCandidate = true;
            ++entry.mEaseCandidateSerial;
        }
    }
    entry.mLastEaseTick = current_tick;
}

void ALSteppedAnimRuntime::capturePose(LLVOAvatar*, Entry& entry,
    const ALSteppedAnim::LimbFrame& limbs, F64, const Entry* decision_source, U16 groups, U16 forced)
{
    using namespace ALSteppedAnim;
    // The first capture (or a skeleton change) fills every group; afterwards only the parts in
    // `groups` are refreshed, so a coarser part keeps its held joints across torso captures.
    if (!entry.mHaveHeld || entry.mHeld.joints.size() != entry.mSmooth.joints.size()) groups = kGroupBits;
    // The Torso (base grid) capture is the full one: it keeps the previous pose for smear,
    // refreshes the effectors and consumes the ease candidate. A part-only refresh copies joints.
    const bool full = (groups & groupBit(PART_TORSO)) != 0;
    const bool had_previous = entry.mHaveHeld;
    // Smear history is per part: the previous held pose and effectors advance ONLY for the parts
    // captured now (a +1 / +2 limb measures its smear from its own last tick, never from a torso
    // tick); a part whose offset just changed (`forced`) gets previous = new held below, so it never
    // smears toward a stale hold.
    const auto chainPart = [&entry](U32 chain) -> S32
    {
        const S32 index = entry.mSmearIndices[chain][0];
        return index >= 0 && static_cast<size_t>(index) < entry.mClasses.size()
            ? static_cast<S32>(partOf(entry.mClasses[index])) : -1;
    };
    // An effector's part: the four limb chains, then the head (Head part); -1 = torso grid.
    const auto effectorPart = [&entry, &chainPart](U32 e) -> S32
    {
        if (e < entry.mSmearIndices.size()) return chainPart(e);
        const S32 index = entry.mEffectorIndices[e];
        return index >= 0 && static_cast<size_t>(index) < entry.mClasses.size()
            ? static_cast<S32>(partOf(entry.mClasses[index])) : -1;
    };
    const auto inGroups = [](U16 set, S32 part) { return part < 0 || (set & groupBit(static_cast<U32>(part))) != 0; };
    if (had_previous)
    {
        advancePartHistory(entry.mHeld, entry.mClasses, groups, full, entry.mPreviousHeld);
        for (U32 e = 0; e < kEffectors; ++e)
            if (inGroups(groups, effectorPart(e))) entry.mPreviousEffectors[e] = entry.mHeldEffectors[e];
        entry.mHavePreviousHeld = true;
    }
    const bool have_previous = had_previous && entry.mHavePreviousHeld &&
        entry.mPreviousHeld.joints.size() == entry.mSmooth.joints.size();
    if (groups == kGroupBits)
        entry.mHeld = entry.mSmooth;
    else
    {
        for (U32 i = 0; i < entry.mHeld.joints.size() && i < entry.mClasses.size(); ++i)
            if (entry.mClasses[i] & groups & kGroupBits) entry.mHeld.joints[i] = entry.mSmooth.joints[i];
        if (full)
        {
            entry.mHeld.rootPos = entry.mSmooth.rootPos;
            entry.mHeld.rootRot = entry.mSmooth.rootRot;
        }
    }
    if (groups & groupBit(PART_ATTACH))
        captureAttachmentHolds(entry.mSmooth, entry.mClasses, entry.mParents,
                               entry.mAssignment.heldClasses, entry.mAttachHeld);
    // Effectors of the parts captured now (an effector off any part rides the torso grid).
    for (U32 e = 0; e < kEffectors; ++e)
        if (inGroups(groups, effectorPart(e)) && (full || effectorPart(e) >= 0)) entry.mHeldEffectors[e] = limbs.p[e];
    // Smear flags and the forced seed (previous = new held, zero displacement).
    for (U32 part = 0; part < kPartGroups; ++part)
        if (groups & groupBit(part)) entry.mPartSmearTick[part] = mConfig.smear && have_previous;
    const U16 seeded = static_cast<U16>(groups & forced);
    if (seeded && have_previous)
    {
        advancePartHistory(entry.mHeld, entry.mClasses, seeded, false, entry.mPreviousHeld);
        for (U32 e = 0; e < kEffectors; ++e)
            if (effectorPart(e) >= 0 && (seeded & groupBit(static_cast<U32>(effectorPart(e)))))
                entry.mPreviousEffectors[e] = entry.mHeldEffectors[e];
        for (U32 part = 0; part < kPartGroups; ++part)
            if (seeded & groupBit(part)) entry.mPartSmearTick[part] = false;
    }
    if (!full) return;
    entry.mFrameEaseApplied = false;
    entry.mFrameEaseWeight = 0.f;
    if (mConfig.ease && mConfig.easeAmount > 0.f && entry.mHaveCandidate &&
        (!decision_source || decision_source->mFrameEaseApplied))
    {
        const F32 weight = decision_source
            ? decision_source->mFrameEaseWeight
            : easeWeight(mConfig.easeAmount, entry.mCandidateSpeed, speedAt(entry.mState.limbs, limbs.t));
        entry.mFrameEaseApplied = true;
        entry.mFrameEaseWeight = weight;
        for (U32 i = 0; i < entry.mHeld.joints.size() && i < entry.mClasses.size(); ++i)
            if ((entry.mClasses[i] & groups & kGroupBits) &&
                (entry.mClasses[i] & (JC_BODY | JC_FACE | JC_GAZE)))
            {
                entry.mHeld.joints[i].rot = slerp(weight, entry.mSmooth.joints[i].rot, entry.mCandidate.joints[i].rot);
                entry.mHeld.joints[i].pos = lerp(entry.mSmooth.joints[i].pos, entry.mCandidate.joints[i].pos, weight);
            }
    }
    entry.mHaveHeld = true;
    entry.mHaveCandidate = false;
    entry.mCandidateSpeed = 0.f;
    entry.mState.candidateSub = -1;
    std::fill(std::begin(entry.mState.subSeen), std::end(entry.mState.subSeen), false);
    entry.mState.smearTick = mConfig.smear && smearHistoryReady(
        had_previous && entry.mHavePreviousHeld,
        entry.mPreviousHeld.joints.size(), entry.mHeld.joints.size());
    // Same parts share the torso's smear tick (a forced part keeps its cleared flag); a coarser part
    // that did not tick keeps its own flag until its next tick.
    for (U32 part = 0; part < kPartGroups; ++part)
        if ((groups & groupBit(part)) && !(seeded & groupBit(part))) entry.mPartSmearTick[part] = entry.mState.smearTick;
}

void ALSteppedAnimRuntime::applyEffects(LLVOAvatar* avatar, Entry& entry,
                                        ALSteppedAnim::PoseBuffer& render)
{
    if (mConfig.boil)
        for (U32 i = 0; i < render.joints.size() && i < entry.mClasses.size(); ++i)
            if (ALSteppedAnim::jointHeld(entry.mClasses[i], entry.mAssignment.heldClasses) &&
                (entry.mClasses[i] & (ALSteppedAnim::JC_BODY | ALSteppedAnim::JC_FACE | ALSteppedAnim::JC_GAZE)))
            {
                // A coarser part boils on its own grid; Same parts (and a worn animesh copying its
                // wearer's decision) on the actor's base boil index.
                const U32 part = ALSteppedAnim::partOf(entry.mClasses[i]);
                const S32 offset = entry.mAssignment.parts[part];
                const S64 boil_index = offset == ALSteppedAnim::RATE_PLUS1 || offset == ALSteppedAnim::RATE_PLUS2
                    ? entry.mDecision.partBoil[part] : entry.mDecision.boilIndex;
                render.joints[i].rot *= ALSteppedAnim::boilRotation(entry.mSeed, boil_index, i, mConfig.boilAmount);
            }

    if (!mConfig.smear) return;
    for (U32 chain = 0; chain < entry.mSmearIndices.size(); ++chain)
    {
        // Each chain smears on its own part's grid and only while that part is held (a Smooth
        // limb is never rewritten, a coarser limb only on its own ticks).
        const S32 root_index = entry.mSmearIndices[chain][0];
        if (root_index < 0 || static_cast<size_t>(root_index) >= entry.mClasses.size()) continue;
        const U16 chain_class = entry.mClasses[root_index];
        const F32 tick_dt = ALSteppedAnim::smearChainDt(entry.mAssignment, chain_class, entry.mState.smearTick,
            entry.mPartSmearTick[ALSteppedAnim::partOf(chain_class)], entry.mDecision.rate, mConfig.baseFps);
        if (tick_dt <= 0.f) continue;
        F32 length = 0.f, minimum = std::numeric_limits<F32>::max();
        for (U32 j = 0; j < 2; ++j)
        {
            const S32 child_index = entry.mSmearChildIndices[chain][j];
            LLJoint* child = child_index >= 0 ? avatar->getSkeletonJoint(child_index) : nullptr;
            const F32 bone = child ? child->getPosition().length() : 0.f;
            length += bone; minimum = llmin(minimum, bone);
        }
        const LLVector3 displacement = entry.mHeldEffectors[chain] - entry.mPreviousEffectors[chain];
        const auto smear = ALSteppedAnim::smearFor(displacement.length(), tick_dt, length, minimum,
            2, mConfig.smearStrength, mConfig.smearThreshold);
        if (!smear.active) continue;
        LLVector3 direction = displacement; direction.normalize();
        for (S32 index : entry.mSmearIndices[chain])
            if (index >= 0 && static_cast<size_t>(index) < entry.mClasses.size() &&
                ALSteppedAnim::jointHeld(entry.mClasses[index], entry.mAssignment.heldClasses) &&
                static_cast<size_t>(index) < entry.mPreviousHeld.joints.size())
        {
            render.joints[index].rot = slerp(smear.lagT,
                entry.mPreviousHeld.joints[index].rot, entry.mHeld.joints[index].rot);
            LLJoint* joint = avatar->getSkeletonJoint(index);
            const LLVector3 local_direction = direction * avatar->getRootJoint()->getWorldRotation() *
                ~joint->getWorldRotation();
            const LLVector3 axis = ALSteppedAnim::smearAxisScale(smear.stretch, local_direction);
            render.joints[index].scale.scaleVec(axis);
        }
    }
}

void ALSteppedAnimRuntime::writeOverlay(LLVOAvatar* avatar, Entry& entry,
                                        const ALSteppedAnim::PoseBuffer& render)
{
    if (&render != &entry.mWritten) entry.mWritten = render;
    entry.mWrittenMask.assign(render.joints.size(), 0);
    entry.mLabels = LLVOAvatar::StepAnimLabelState();
    entry.mRootDelta.clear();
    for (U32 i = 0; i < render.joints.size(); ++i)
    {
        LLJoint* joint = poseJoint(avatar, entry, i);
        if (!joint) continue;
        const ALSteppedAnim::JointLocal& target = render.joints[i];
        if (!rotEqual(joint->getRotation(), target.rot)) { joint->setRotation(target.rot); entry.mWrittenMask[i] |= WRITE_ROT; }
        if (!vecEqual(joint->getPosition(), target.pos)) { joint->setPosition(target.pos, false); entry.mWrittenMask[i] |= WRITE_POS; }
        if (!vecEqual(joint->getScale(), target.scale)) { joint->setScale(target.scale); entry.mWrittenMask[i] |= WRITE_SCALE; }
    }
    entry.mRootWrittenMask = 0;
    if (entry.mAssignment.heldClasses & ALSteppedAnim::JC_ROOT)
    {
        LLJoint* root = avatar->getRootJoint();
        if (!rotEqual(root->getRotation(), render.rootRot)) { root->setRotation(render.rootRot); entry.mRootWrittenMask |= WRITE_ROT; }
        if (!vecEqual(root->getPosition(), render.rootPos)) { root->setPosition(render.rootPos, false); entry.mRootWrittenMask |= WRITE_POS; }
    }
    if (!ALSteppedAnim::hasOverlayWrites(entry.mWrittenMask, entry.mRootWrittenMask))
    {
        entry.mWrittenMask.clear();
        entry.mRootWrittenMask = 0;
        mPaletteRebuilds += avatar->invalidateSkinPalettesForOverlay();
        entry.mPaletteOverlay = true;
        static LLCachedControl<bool> verify(gSavedSettings, "StepAnimDebugVerify");
        if (verify) avatar->captureStepAnimRenderState();
        sHasOverlay = true;
        return;
    }
    entry.mWriteEpoch = avatar->getJointOverrideEpoch();
    avatar->getRootJoint()->updateWorldMatrixChildren();
    avatar->markStepAnimPoseChanged();
    mPaletteRebuilds += avatar->invalidateSkinPalettesForOverlay();
    entry.mPaletteOverlay = true;
    {
        LL_PROFILE_ZONE_NAMED("StepAnim::attachmentsApply");
        avatar->syncStepAnimAttachmentMatrices();
    }
    static LLCachedControl<bool> verify(gSavedSettings, "StepAnimDebugVerify");
    if (verify) avatar->captureStepAnimRenderState();
    if (entry.mRootWrittenMask)
    {
        entry.mRootDelta = render.rootPos - entry.mSmooth.rootPos;
        avatar->shiftStepAnimLabels(entry.mRootDelta, entry.mLabels);
    }
    sHasOverlay = true;
}

void ALSteppedAnimRuntime::applyHeld()
{
    LL_PROFILE_ZONE_NAMED("StepAnim::applyHeld");
    sHasOverlay = false;
    const U64 agent_region = gAgent.getRegion() ? gAgent.getRegion()->getHandle() : 0;
    const LLVector3d agent_origin = gAgent.getPosGlobalFromAgent(LLVector3::zero);
    const bool agent_origin_changed = mHaveAgentOrigin &&
        (mAgentRegionHandle != agent_region || mAgentOriginGlobal != agent_origin);
    if (agent_origin_changed)
        for (auto& item : mEntries)
            if (item.second.mAssignment.heldClasses & ALSteppedAnim::JC_ROOT)
                resetEntryHistory(item.second);
    mAgentRegionHandle = agent_region;
    mAgentOriginGlobal = agent_origin;
    mHaveAgentOrigin = true;
    for (const LLUUID& id : mPlanned)
    {
        LLViewerObject* object = gObjectList.findObject(id);
        LLVOAvatar* avatar = object ? object->asAvatar() : nullptr;
        if (!eligible(avatar)) continue;
        Entry& entry = mEntries[id];
        Entry* decision_source = nullptr;
        if (entry.mDecisionSource.notNull())
        {
            const auto source = mEntries.find(entry.mDecisionSource);
            if (source != mEntries.end()) decision_source = &source->second;
        }
        const U32 count = poseJointCount(avatar);
        const U64 region = avatar->getRegion() ? avatar->getRegion()->getHandle() : 0;
        const bool invalid = entry.mSkeletonSerial != avatar->getSteppedSkeletonSerial() ||
            entry.mJointCount != count || entry.mOverrideEpoch != avatar->getJointOverrideEpoch() ||
            entry.mRegionHandle != region;
        if (invalid)
        {
            resetEntryHistory(entry);
            entry.mSkeletonSerial = avatar->getSteppedSkeletonSerial();
            entry.mOverrideEpoch = avatar->getJointOverrideEpoch(); entry.mRegionHandle = region;
            buildClasses(avatar, entry);
            // A rebuilt actor recaptures every part now; the force bits make a worn animesh on a
            // coarser Attachments grid (attachedDecision) recapture with it instead of waiting for
            // its next tick.
            if (!decision_source)
            {
                entry.mDecision.capture = true;
                entry.mDecision.captureGroups = ALSteppedAnim::kGroupBits;
                entry.mDecision.forceGroups = ALSteppedAnim::kGroupBits;
            }
        }
        readPose(avatar, entry, entry.mSmooth);
        const ALSteppedAnim::LimbFrame limbs = limbFrame(avatar, entry, mTime);
        if (decision_source)
        {
            entry.mState = decision_source->mState;
            // Attachments Same copies the wearer's frame; +1 / +2 takes the wearer's Attachments grid.
            entry.mDecision = ALSteppedAnim::attachedDecision(entry.mAssignment, decision_source->mState,
                decision_source->mDecision, decision_source->mAssignment.parts[ALSteppedAnim::PART_ATTACH],
                mConfig.baseFps);
            if (entry.mInheritedEaseCandidateSerial != decision_source->mEaseCandidateSerial)
            {
                entry.mCandidate = entry.mSmooth;
                entry.mCandidateSpeed = decision_source->mCandidateSpeed;
                entry.mHaveCandidate = true;
                entry.mInheritedEaseCandidateSerial = decision_source->mEaseCandidateSerial;
            }
        }
        else
        {
            entry.mFrameEaseApplied = false;
            ALSteppedAnim::observe(entry.mState, mConfig, limbs);
            updateEaseCandidate(entry, limbs, mTime);
        }
        // Each part group refreshes its held joints on its own grid (all parts Same: on `capture`).
        const U16 groups = entry.mHaveHeld
            ? ALSteppedAnim::refreshGroups(entry.mAssignment, entry.mDecision) : ALSteppedAnim::kGroupBits;
        if (groups) capturePose(avatar, entry, limbs, mTime, decision_source, groups, entry.mDecision.forceGroups);
        ALSteppedAnim::composeRender(entry.mSmooth, entry.mHeld, entry.mClasses,
                                     entry.mAssignment.heldClasses, entry.mWritten);
        applyEffects(avatar, entry, entry.mWritten);
        // Attachment points on their own grid (or re-placed from the smooth pose) under the FINAL
        // rendered body (after boil and smear moved the parents); nothing is touched while the
        // Attachments part is Same.
        ALSteppedAnim::applyAttachmentHolds(entry.mClasses, entry.mParents, entry.mAssignment.heldClasses,
                                            entry.mAttachHeld, entry.mWritten);
        writeOverlay(avatar, entry, entry.mWritten);
    }
    if (sHasOverlay)
        ALGhostStudio::instance().applyStepAnimNameplateOffsets([this](const LLUUID& id)
        {
            const auto found = mEntries.find(id);
            return found == mEntries.end() ? LLVector3::zero : found->second.mRootDelta;
        });
    LL_PROFILE_PLOT("StepAnim actors", mActiveActors);
    LL_PROFILE_PLOT("StepAnim skipped", mSkippedActors);
    LL_PROFILE_PLOT("paletteRebuilds", mPaletteRebuilds);
}

void ALSteppedAnimRuntime::restoreOverlay(LLVOAvatar* avatar, Entry& entry)
{
    static LLCachedControl<bool> verify(gSavedSettings, "StepAnimDebugVerify");
    const bool epoch_changed = avatar->getJointOverrideEpoch() != entry.mWriteEpoch;
    bool restored = false;
    entry.mVerifyPose = entry.mSmooth;
    entry.mVerifyMask.assign(entry.mWrittenMask.size(), 0);
    entry.mVerifyRootMask = 0;
    for (U32 i = 0; i < entry.mWrittenMask.size(); ++i)
    {
        const U8 mask = entry.mWrittenMask[i]; if (!mask) continue;
        LLJoint* joint = poseJoint(avatar, entry, i);
        if (!joint) continue;
        if (verify && (((mask & WRITE_ROT) && !rotEqual(joint->getRotation(), entry.mWritten.joints[i].rot)) ||
                       ((mask & WRITE_POS) && !vecEqual(joint->getPosition(), entry.mWritten.joints[i].pos)) ||
                       ((mask & WRITE_SCALE) && !vecEqual(joint->getScale(), entry.mWritten.joints[i].scale))))
            LL_INFOS("StepAnim") << "Foreign in-window joint write retained for " << avatar->getID()
                << " joint " << i << LL_ENDL;
        if ((mask & WRITE_ROT) && rotEqual(joint->getRotation(), entry.mWritten.joints[i].rot))
            { joint->setRotation(entry.mSmooth.joints[i].rot); entry.mVerifyMask[i] |= WRITE_ROT; restored = true; }
        if ((mask & WRITE_POS) && vecEqual(joint->getPosition(), entry.mWritten.joints[i].pos))
            { joint->setPosition(entry.mSmooth.joints[i].pos, false); entry.mVerifyMask[i] |= WRITE_POS; restored = true; }
        if ((mask & WRITE_SCALE) && vecEqual(joint->getScale(), entry.mWritten.joints[i].scale))
            { joint->setScale(entry.mSmooth.joints[i].scale); entry.mVerifyMask[i] |= WRITE_SCALE; restored = true; }
    }
    LLJoint* root = avatar->getRootJoint();
    if ((entry.mRootWrittenMask & WRITE_ROT) && rotEqual(root->getRotation(), entry.mWritten.rootRot))
        { root->setRotation(entry.mSmooth.rootRot); entry.mVerifyRootMask |= WRITE_ROT; restored = true; }
    if ((entry.mRootWrittenMask & WRITE_POS) && vecEqual(root->getPosition(), entry.mWritten.rootPos))
        { root->setPosition(entry.mSmooth.rootPos, false); entry.mVerifyRootMask |= WRITE_POS; restored = true; }
    if (restored)
    {
        root->updateWorldMatrixChildren();
        avatar->markStepAnimPoseChanged();
        LL_PROFILE_ZONE_NAMED("StepAnim::attachmentsRestore");
        avatar->syncStepAnimAttachmentMatrices();
    }
    avatar->restoreStepAnimLabels(entry.mLabels);
    if (entry.mPaletteOverlay) avatar->invalidateSkinPalettesAfterOverlay();
    entry.mPaletteOverlay = false;
    entry.mWrittenMask.clear(); entry.mRootWrittenMask = 0; entry.mRootDelta.clear();
    if (epoch_changed) entry.mDropAfterRestore = true;
}

void ALSteppedAnimRuntime::endFrameRestore()
{
    LL_PROFILE_ZONE_NAMED("StepAnim::endFrameRestore");
    static LLCachedControl<bool> verify(gSavedSettings, "StepAnimDebugVerify");
    if (verify)
    {
        for (const LLUUID& id : mPlanned)
        {
            Entry& entry = mEntries[id];
            if (!entry.mPaletteOverlay) continue;
            LLViewerObject* object = gObjectList.findObject(id);
            LLVOAvatar* avatar = object ? object->asAvatar() : nullptr;
            if (avatar) avatar->verifyStepAnimRenderState(gFrameCount);
        }
    }
    ALGhostStudio::instance().restoreStepAnimNameplateOffsets();
    for (auto it = mPlanned.rbegin(); it != mPlanned.rend(); ++it)
    {
        const auto found = mEntries.find(*it);
        if (found == mEntries.end()) continue;
        Entry& entry = found->second;
        if (!entry.mWrittenMask.empty() || entry.mRootWrittenMask || entry.mPaletteOverlay)
        {
            LLViewerObject* object = gObjectList.findObject(*it);
            LLVOAvatar* avatar = object ? object->asAvatar() : nullptr;
            if (avatar && !avatar->isDead() && avatar->getRootJoint()) restoreOverlay(avatar, entry);
        }
    }
    for (auto it = mEntries.begin(); it != mEntries.end();)
        if (it->second.mDropAfterRestore) it = mEntries.erase(it); else ++it;
    sHasOverlay = false;
    if (verify) verifyRestored("frame-end");
}

const ALSteppedAnim::FrameDecision* ALSteppedAnimRuntime::decisionFor(const LLVOAvatar* avatar, U64* particle_epoch) const
{
    if (!avatar) return nullptr;
    const auto found = mEntries.find(avatar->getID());
    if (found == mEntries.end() || !found->second.mAssignment.stepped) return nullptr;
    if (particle_epoch) *particle_epoch = found->second.mParticleEpoch;
    return &found->second.mDecision;
}

bool ALSteppedAnimRuntime::holdsFlexi(const LLVOAvatar* avatar) const
{
    const auto* decision = decisionFor(avatar);
    return decision && mConfig.flexi && decision->hold && !decision->capture;
}

bool ALSteppedAnimRuntime::holdsFace(const LLVOAvatar* avatar) const
{
    if (!avatar) return false;
    const auto found = mEntries.find(avatar->getID());
    if (found == mEntries.end() || !found->second.mAssignment.stepped) return false;
    const Entry& entry = found->second;
    // One control with the face bones: held only while they are (Face part not Smooth, StepAnimFace
    // on), and released on the Face part's own refresh frames. All parts Same: hold && !capture.
    if (!ALSteppedAnim::jointHeld(ALSteppedAnim::JC_FACE | ALSteppedAnim::groupBit(ALSteppedAnim::PART_FACE),
                                  entry.mAssignment.heldClasses))
        return false;
    return entry.mDecision.hold &&
        !(ALSteppedAnim::refreshGroups(entry.mAssignment, entry.mDecision) & ALSteppedAnim::groupBit(ALSteppedAnim::PART_FACE));
}

bool ALSteppedAnimRuntime::holdsAttachmentFX(const LLVOAvatar* avatar) const
{
    const auto* decision = decisionFor(avatar);
    return decision && mConfig.attachFX && decision->hold && !decision->capture;
}

bool ALSteppedAnimRuntime::holdsTextureAnim(const LLVOAvatar* avatar) const
{
    const auto* decision = decisionFor(avatar);
    return decision && mConfig.textureAnim && decision->hold && !decision->capture;
}

F32 ALSteppedAnimRuntime::attachmentOmegaDt(LLViewerObject* object, F32 dt)
{
    if (!object || !object->isAttachment()) return dt;
    LLVOAvatar* avatar = object->getAvatar();
    const ALSteppedAnim::FrameDecision* decision = decisionFor(avatar);
    if (!decision || !mConfig.attachFX)
    {
        const auto found = mOmegaDebt.find(object->getID());
        if (found == mOmegaDebt.end()) return dt;
        const F32 result = dt + found->second;
        mOmegaDebt.erase(found);
        return result;
    }
    F32& debt = mOmegaDebt[object->getID()];
    if (decision->hold && !decision->capture)
    {
        debt += dt;
        return 0.f;
    }
    const F32 result = dt + debt;
    mOmegaDebt.erase(object->getID());
    return result;
}

F32 ALSteppedAnimRuntime::selectedLimbSpeed() const
{
    const LLUUID selected = LLDirectorCast::instance().getSteppedActorId();
    const LLUUID runtime = selected.isNull() && gAgentAvatarp ? gAgentAvatarp->getID() : selected;
    const auto found = mEntries.find(runtime);
    return found == mEntries.end() ? 0.f : found->second.mState.limbs.speed();
}

void ALSteppedAnimRuntime::verifyRestored(const char* point)
{
    const bool consume = std::strcmp(point, "idle-top") == 0;
    for (auto& item : mEntries)
    {
        Entry& entry = item.second;
        if (!entry.mWrittenMask.empty() || entry.mRootWrittenMask)
            LL_WARNS("StepAnim") << "Overlay remained open at " << point << " for " << item.first << LL_ENDL;
        LLViewerObject* object = gObjectList.findObject(item.first);
        LLVOAvatar* avatar = object ? object->asAvatar() : nullptr;
        if (avatar && entry.mVerifyMask.size() == entry.mVerifyPose.joints.size())
        {
            for (U32 i = 0; i < entry.mVerifyMask.size(); ++i)
            {
                const U8 mask = entry.mVerifyMask[i];
                if (!mask) continue;
                LLJoint* joint = poseJoint(avatar, entry, i);
                if (!joint) continue;
                const ALSteppedAnim::JointLocal& expected = entry.mVerifyPose.joints[i];
                if (((mask & WRITE_ROT) && !rotEqual(joint->getRotation(), expected.rot)) ||
                    ((mask & WRITE_POS) && !vecEqual(joint->getPosition(), expected.pos)) ||
                    ((mask & WRITE_SCALE) && !vecEqual(joint->getScale(), expected.scale)))
                    LL_WARNS("StepAnim") << "Unexpected post-restore joint change at " << point
                        << " for " << item.first << " joint " << i << LL_ENDL;
            }
            LLJoint* root = avatar->getRootJoint();
            if (root && (((entry.mVerifyRootMask & WRITE_ROT) && !rotEqual(root->getRotation(), entry.mVerifyPose.rootRot)) ||
                         ((entry.mVerifyRootMask & WRITE_POS) && !vecEqual(root->getPosition(), entry.mVerifyPose.rootPos))))
                LL_WARNS("StepAnim") << "Unexpected post-restore root change at " << point
                    << " for " << item.first << LL_ENDL;
        }
        if (consume)
        {
            entry.mVerifyMask.clear();
            entry.mVerifyRootMask = 0;
        }
    }
}

void ALSteppedAnimRuntime::updateRotoMatch()
{
    if (!gSavedSettings.getBOOL("StepAnimEnabled") || !gSavedSettings.getBOOL("StepAnimMatchRotoInk")) return;
    const ALSteppedAnim::RotoMatch match = ALSteppedAnim::mapRotoInk(gSavedSettings.getF32("StepAnimRate"));
    gSavedSettings.setF32("CineOutlineSketchFPS", match.sketchFps);
    if (match.setMotionStep) gSavedSettings.setS32("CineOutlineMotionStep", match.motionStep);
}

void ALSteppedAnimRuntime::deactivate()
{
    sStepParticles = false;
    sStepTextureAnim = false;
    LLViewerPartSim::instance().clearStepAnimDeferredSources();
    mPlanned.clear();
    mOmegaDebt.clear();
    mActiveActors = 0;
    mSkippedActors = 0;
    for (auto& item : mEntries)
    {
        item.second.mAssignment = ALSteppedAnim::Assignment();
        item.second.mDecision = ALSteppedAnim::FrameDecision();
        resetEntryHistory(item.second);
    }
    mHaveAgentOrigin = false;
}

void ALSteppedAnimRuntime::resetEntryHistory(Entry& entry)
{
    entry.mState.hasCapture = false;
    entry.mState.smearTick = false;
    entry.mHaveHeld = false;
    entry.mHavePreviousHeld = false;
    entry.mHaveCandidate = false;
    entry.mPreviousHeld.joints.clear();
    entry.mPreviousEffectors.fill(LLVector3::zero);
    entry.mLastEaseTick = std::numeric_limits<S64>::min();
}
