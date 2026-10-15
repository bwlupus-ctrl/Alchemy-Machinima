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
#include "llcharacter.h"
#include "llcinematiccamera.h"
#include "llcontrolavatar.h"
#include "lldirectorcast.h"
#include "llghostavatar.h"
#include "lljoint.h"
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
    instance().updateRotoMatch();
}

ALSteppedAnim::Config ALSteppedAnimRuntime::readConfig() const
{
    static LLCachedControl<S32> scope(gSavedSettings, "StepAnimScope");
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
    ALSteppedAnim::Config c;
    c.enabled = true; c.scope = scope; c.excludeCameraMounted = exclude; c.maxAvatars = maximum;
    c.mode = mode; c.rate = finiteSetting(rate, 12.f); c.fastRate = finiteSetting(fast, 24.f);
    c.adaptThreshold = finiteSetting(threshold, 1.5f); c.adaptHysteresis = finiteSetting(hysteresis, .3f);
    c.adaptMinDwell = finiteSetting(dwell, .25f); c.backgroundRate = finiteSetting(background, 8.f);
    c.what = what; c.face = face; c.gaze = gaze; c.flexi = flexi; c.attachFX = attachment_fx;
    c.clock = clock; c.offset = offset; c.phase = finiteSetting(phase, 0.f); c.baseFps = base_fps;
    c.boil = boil; c.boilAmount = finiteSetting(boil_amount, .6f); c.boilTicks = boil_ticks;
    c.impact = impact; c.impactTicks = impact_ticks; c.impactThreshold = finiteSetting(impact_threshold, 25.f);
    c.smear = smear; c.smearStrength = finiteSetting(smear_strength, .5f);
    c.smearThreshold = finiteSetting(smear_threshold, 2.5f); c.ease = ease;
    c.easeAmount = finiteSetting(ease_amount, .5f); c.matchRoto = match_roto;
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
    std::map<LLUUID, S32> cast_order;
    for (U32 i = 0; i < cast.getCast().size(); ++i) cast_order[cast.getCast()[i].mId] = static_cast<S32>(i);
    std::set<LLUUID> bone_pov_anchors;
    const LLPrismLens::RegistrySnapshot camera_registry = LLPrismLens::registrySnapshot();
    for (U32 index = 0; index < camera_registry.mCaptureCount; ++index)
    {
        const LLPrismLens::CaptureDefinition& capture = camera_registry.mCaptures[index];
        const LLPrismLens::BonePovSettings& settings = capture.mCamera.mBonePov;
        if (capture.mMode == LLPrismLens::ECaptureMode::CAMERA_FEED &&
            capture.mCamera.mVirtual && settings.mEnabled)
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
        f.seed = (actor_id.isNull() ? gAgentID : actor_id).getDigest64();
        f.isSelf = avatar->isSelf();
        // Self is stored under gAgentID when it is a cast member (auto-added at login);
        // with an empty cast, "Whole cast" means you.
        const bool self_in_cast = f.isSelf && gAgentID.notNull() && cast.containsStored(gAgentID);
        f.inCast = f.isSelf ? (empty_cast || self_in_cast) : cast.containsStored(actor_id);
        f.isSelected = selected.isNull() ? f.isSelf : actor_id == selected;
        f.drawn = true;
        f.cameraMounted = (f.isSelf && gAgentCamera.cameraMouselook()) ||
            LLCinematicCamera::instance().isActiveBoneLockTarget(avatar->getID()) ||
            bone_pov_anchors.find(avatar->getID()) != bone_pov_anchors.end();
        f.posing = FSFloaterPoser::isEditingAvatar(avatar);
        f.cameraDistance = static_cast<F32>((avatar->getPositionGlobal() - camera).length());
        const auto order = cast_order.find(f.isSelf ? gAgentID : actor_id);
        if (order != cast_order.end()) f.castOrder = order->second;
        else if (f.isSelf && empty_cast) f.castOrder = 0;
        const LLDirectorCast::ActorStyle::SteppedOverride& ov = cast.getActorStyle(actor_id).mStepped;
        f.ov = {ov.mMode, ov.mRate, ov.mWhat, ov.mRole};
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
    for (U32 i : ordered)
    {
        if (parents[i] < 0) continue;
        const U32 wearer = static_cast<U32>(parents[i]);
        ALSteppedAnim::inheritAttachedAssignment(
            assignments[wearer], facts[wearer].seed, assignments[i], facts[i].seed);
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
            assignments[wearer], facts[wearer].seed, assignments[i], facts[i].seed);
    }
    mPlanned.clear(); mActiveActors = 0;
    for (U32 i : ordered)
        if (assignments[i].stepped)
        {
            Entry& entry = mEntries[ids[i]];
            // Copy the wearer's frame only when the wearer itself is stepped this
            // frame. A camera-mounted, capped or "Never" wearer has no planned
            // entry, so the animesh must plan its own frame (else it freezes on a
            // stale held pose while the wearer moves smoothly).
            const bool inherit_from_wearer = parents[i] >= 0 &&
                assignments[static_cast<U32>(parents[i])].stepped;
            entry.mDecisionSource = inherit_from_wearer
                ? ids[static_cast<U32>(parents[i])] : LLUUID::null;
            if (inherit_from_wearer)
            {
                Entry& wearer = mEntries[ids[static_cast<U32>(parents[i])]];
                ALSteppedAnim::inheritAttachedFrame(
                    wearer.mAssignment, wearer.mSeed, wearer.mState, wearer.mDecision,
                    entry.mAssignment, entry.mSeed, entry.mState, entry.mDecision);
            }
            else
            {
                entry.mSeed = facts[i].seed;
                entry.mAssignment = assignments[i];
                entry.mDecision = ALSteppedAnim::planFrame(entry.mState, entry.mAssignment, mConfig, mTime);
            }
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
    for (S32 i = 0; i < avatar->getCollisionVolumeJointCount(); ++i)
    {
        LLJoint* joint = avatar->getCollisionVolumeJoint(i);
        ALSteppedAnim::JointDesc d; d.name = joint->getName(); d.collisionVolume = true;
        desc.push_back(d);
    }
    entry.mClasses = ALSteppedAnim::classifyJoints(desc);
    entry.mJointCount = static_cast<U32>(desc.size());
}

void ALSteppedAnimRuntime::readPose(LLVOAvatar* avatar, Entry& entry,
                                    ALSteppedAnim::PoseBuffer& out)
{
    const U32 skeleton = static_cast<U32>(avatar->getSkeletonJointCount());
    const U32 count = skeleton + avatar->getCollisionVolumeJointCount();
    out.joints.resize(count);
    for (U32 i = 0; i < count; ++i)
    {
        LLJoint* joint = i < skeleton ? avatar->getSkeletonJoint(i) :
            avatar->getCollisionVolumeJoint(i - skeleton);
        out.joints[i].rot = joint->getRotation();
        out.joints[i].pos = joint->getPosition();
        out.joints[i].scale = joint->getScale();
    }
    out.rootPos = avatar->getRootJoint()->getPosition();
    out.rootRot = avatar->getRootJoint()->getRotation();
    if (entry.mClasses.size() != count) buildClasses(avatar, entry);
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
    const ALSteppedAnim::LimbFrame& limbs, F64, const Entry* decision_source)
{
    const bool had_previous = entry.mHaveHeld;
    if (had_previous)
    {
        entry.mPreviousHeld = entry.mHeld;
        entry.mPreviousEffectors = entry.mHeldEffectors;
        entry.mHavePreviousHeld = true;
    }
    entry.mHeld = entry.mSmooth;
    entry.mFrameEaseApplied = false;
    entry.mFrameEaseWeight = 0.f;
    if (mConfig.ease && mConfig.easeAmount > 0.f && entry.mHaveCandidate &&
        (!decision_source || decision_source->mFrameEaseApplied))
    {
        const F32 weight = decision_source
            ? decision_source->mFrameEaseWeight
            : ALSteppedAnim::easeWeight(mConfig.easeAmount, entry.mCandidateSpeed,
                ALSteppedAnim::speedAt(entry.mState.limbs, limbs.t));
        entry.mFrameEaseApplied = true;
        entry.mFrameEaseWeight = weight;
        for (U32 i = 0; i < entry.mHeld.joints.size() && i < entry.mClasses.size(); ++i)
            if (entry.mClasses[i] & (ALSteppedAnim::JC_BODY | ALSteppedAnim::JC_FACE | ALSteppedAnim::JC_GAZE))
            {
                entry.mHeld.joints[i].rot = slerp(weight, entry.mSmooth.joints[i].rot, entry.mCandidate.joints[i].rot);
                entry.mHeld.joints[i].pos = lerp(entry.mSmooth.joints[i].pos, entry.mCandidate.joints[i].pos, weight);
            }
    }
    for (U32 i = 0; i < ALSteppedAnim::kEffectors; ++i) entry.mHeldEffectors[i] = limbs.p[i];
    entry.mHaveHeld = true;
    entry.mHaveCandidate = false;
    entry.mCandidateSpeed = 0.f;
    entry.mState.candidateSub = -1;
    std::fill(std::begin(entry.mState.subSeen), std::end(entry.mState.subSeen), false);
    entry.mState.smearTick = mConfig.smear && ALSteppedAnim::smearHistoryReady(
        had_previous && entry.mHavePreviousHeld,
        entry.mPreviousHeld.joints.size(), entry.mHeld.joints.size());
}

void ALSteppedAnimRuntime::applyEffects(LLVOAvatar* avatar, Entry& entry,
                                        ALSteppedAnim::PoseBuffer& render)
{
    if (mConfig.boil)
        for (U32 i = 0; i < render.joints.size() && i < entry.mClasses.size(); ++i)
            if ((entry.mClasses[i] & entry.mAssignment.heldClasses) &&
                (entry.mClasses[i] & (ALSteppedAnim::JC_BODY | ALSteppedAnim::JC_FACE | ALSteppedAnim::JC_GAZE)))
                render.joints[i].rot *= ALSteppedAnim::boilRotation(
                    entry.mSeed, entry.mDecision.boilIndex, i, mConfig.boilAmount);

    if (!entry.mState.smearTick || !mConfig.smear) return;
    const F32 tick_dt = 1.f / llmax(entry.mDecision.rate, 2.f);
    for (U32 chain = 0; chain < entry.mSmearIndices.size(); ++chain)
    {
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
        for (S32 index : entry.mSmearIndices[chain]) if (index >= 0)
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
    const U32 skeleton = static_cast<U32>(avatar->getSkeletonJointCount());
    if (&render != &entry.mWritten) entry.mWritten = render;
    entry.mWrittenMask.assign(render.joints.size(), 0);
    entry.mLabels = LLVOAvatar::StepAnimLabelState();
    entry.mRootDelta.clear();
    for (U32 i = 0; i < render.joints.size(); ++i)
    {
        LLJoint* joint = i < skeleton ? avatar->getSkeletonJoint(i) : avatar->getCollisionVolumeJoint(i - skeleton);
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
        const U32 count = static_cast<U32>(avatar->getSkeletonJointCount() + avatar->getCollisionVolumeJointCount());
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
            if (!decision_source) entry.mDecision.capture = true;
        }
        readPose(avatar, entry, entry.mSmooth);
        const ALSteppedAnim::LimbFrame limbs = limbFrame(avatar, entry, mTime);
        if (decision_source)
        {
            entry.mState = decision_source->mState;
            entry.mDecision = decision_source->mDecision;
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
        if (entry.mDecision.capture || !entry.mHaveHeld)
            capturePose(avatar, entry, limbs, mTime, decision_source);
        ALSteppedAnim::composeRender(entry.mSmooth, entry.mHeld, entry.mClasses,
                                     entry.mAssignment.heldClasses, entry.mWritten);
        applyEffects(avatar, entry, entry.mWritten);
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
    const U32 skeleton = static_cast<U32>(avatar->getSkeletonJointCount());
    bool restored = false;
    entry.mVerifyPose = entry.mSmooth;
    entry.mVerifyMask.assign(entry.mWrittenMask.size(), 0);
    entry.mVerifyRootMask = 0;
    for (U32 i = 0; i < entry.mWrittenMask.size(); ++i)
    {
        const U8 mask = entry.mWrittenMask[i]; if (!mask) continue;
        LLJoint* joint = i < skeleton ? avatar->getSkeletonJoint(i) : avatar->getCollisionVolumeJoint(i - skeleton);
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

const ALSteppedAnim::FrameDecision* ALSteppedAnimRuntime::decisionFor(const LLVOAvatar* avatar) const
{
    if (!avatar) return nullptr;
    const auto found = mEntries.find(avatar->getID());
    return found == mEntries.end() || !found->second.mAssignment.stepped ? nullptr : &found->second.mDecision;
}

bool ALSteppedAnimRuntime::holdsFlexi(const LLVOAvatar* avatar) const
{
    const auto* decision = decisionFor(avatar);
    return decision && mConfig.flexi && decision->hold && !decision->capture;
}

bool ALSteppedAnimRuntime::holdsAttachmentFX(const LLVOAvatar* avatar) const
{
    const auto* decision = decisionFor(avatar);
    return decision && mConfig.attachFX && decision->hold && !decision->capture;
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
            const U32 skeleton = static_cast<U32>(avatar->getSkeletonJointCount());
            for (U32 i = 0; i < entry.mVerifyMask.size(); ++i)
            {
                const U8 mask = entry.mVerifyMask[i];
                if (!mask) continue;
                LLJoint* joint = i < skeleton ? avatar->getSkeletonJoint(i) : avatar->getCollisionVolumeJoint(i - skeleton);
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
