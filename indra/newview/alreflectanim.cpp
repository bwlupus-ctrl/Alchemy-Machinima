#include "llviewerprecompiledheaders.h"
#include "alreflectanim.h"
#include "llagent.h"
#include "llappviewer.h"
#include "llcallbacklist.h"
#include "lldatapacker.h"
#include "llfilesystem.h"
#include "llkeyframemotion.h"
#include "llnotificationsutil.h"
#include "llviewercontrol.h"
#include "llvoavatarself.h"
#include <set>

namespace
{
const LLUUID REFLECT_NAMESPACE("d79aa32b-8df6-44c1-9302-d37cb3ec9204");
struct Fetch { LLUUID avatar; U64 generation; };
// Every scratch motion, including deserialize/validation failures, leaves the
// controller before its cache list is freed. No scratch id can be fetched later.
struct Scratch
{
    LLUUID id;
    LLKeyframeMotion* motion = nullptr;
    Scratch() { id.generate(); motion = dynamic_cast<LLKeyframeMotion*>(gAgentAvatarp->createMotion(id)); }
    ~Scratch() { gAgentAvatarp->removeMotion(id); LLKeyframeDataCache::removeKeyframeData(id); }
    bool decode(std::vector<U8>& bytes, const LLUUID& original)
    {
        LLDataPackerBinaryBuffer dp(bytes.data(), (S32)bytes.size());
        return motion && motion->deserialize(dp, original, true);
    }
    auto list() { return LLKeyframeDataCache::getKeyframeData(id); }
};

bool roundTrip(std::vector<U8> original, std::vector<U8> mirrored, const LLUUID& original_id)
{
    Scratch expected, actual;
    if (!expected.decode(original, original_id) || !actual.decode(mirrored, original_id)) return false;
    const auto* a = expected.list(); const auto* b = actual.list();
    if (!a || !b || a->mJointMotionArray.size() != b->mJointMotionArray.size() || a->mConstraints.size() != b->mConstraints.size()) return false;
    const F32 time_code = llmax(a->mDuration, .001f) / 65535.f + 1e-6f;
    for (size_t i = 0; i < a->mJointMotionArray.size(); ++i)
    {
        const auto* src = a->mJointMotionArray[i]; const auto* dst = b->mJointMotionArray[i];
        if (ALReflect::partner(src->mJointName) != dst->mJointName ||
            src->mRotationCurve.mKeys.size() != dst->mRotationCurve.mKeys.size() ||
            src->mPositionCurve.mKeys.size() != dst->mPositionCurve.mKeys.size()) return false;
        auto r = dst->mRotationCurve.mKeys.begin();
        for (const auto& key : src->mRotationCurve.mKeys)
        {
            const LLQuaternion q = ALReflect::rotation(key.second.mRotation);
            if (fabsf(key.second.mTime - r->second.mTime) > time_code) return false;
            for (S32 k = 0; k < 3; ++k)
                if (fabsf(q.mQ[k] - r->second.mRotation.mQ[k]) > 4.f / 65535.f + 1e-6f) return false;
            ++r;
        }
        auto p = dst->mPositionCurve.mKeys.begin();
        for (const auto& key : src->mPositionCurve.mKeys)
        {
            const LLVector3 v = ALReflect::vector(key.second.mPosition);
            if (fabsf(key.second.mTime - p->second.mTime) > time_code) return false;
            for (S32 k = 0; k < 3; ++k)
                if (fabsf(v.mV[k] - p->second.mPosition.mV[k]) > 20.f / 65535.f + 1e-6f) return false;
            ++p;
        }
    }
    auto other = b->mConstraints.begin();
    for (const auto* c : a->mConstraints)
    {
        const auto* d = *other++;
        auto paired = [](S32 src, S32 dst)
        {
            LLJoint* s = gAgentAvatarp->findCollisionVolume(src);
            LLJoint* t = gAgentAvatarp->findCollisionVolume(dst);
            return s && t && ALReflect::partner(s->getName()) == t->getName();
        };
        if (!paired(c->mSourceConstraintVolume, d->mSourceConstraintVolume)) return false;
        if (c->mConstraintTargetType == CONSTRAINT_TARGET_TYPE_BODY &&
            !paired(c->mTargetConstraintVolume, d->mTargetConstraintVolume)) return false;
    }
    return true;
}
}

ALReflectAnim::ALReflectAnim() { gIdleCallbacks.addFunction(idle, this); }
ALReflectAnim::~ALReflectAnim() { gIdleCallbacks.deleteFunction(idle, this); clear(); }
void ALReflectAnim::clear()
{
    ++mGeneration;
    mIntents.clear();
    for (const auto& pair : mAssets)
    {
        if (isAgentAvatarValid() && gAgentAvatarp->getID() == mAvatar)
            gAgentAvatarp->removeMotion(pair.second.derived);
        LLKeyframeDataCache::removeKeyframeData(pair.second.derived);
    }
    mAssets.clear(); mAvatar.setNull();
}
bool ALReflectAnim::isDerived(const LLUUID& id) const
{
    for (const auto& pair : mAssets) if (pair.second.derived == id) return true;
    return false;
}
LLUUID ALReflectAnim::originalFor(const LLUUID& id) const
{
    for (const auto& pair : mAssets) if (pair.second.derived == id) return pair.first;
    return id;
}
LLUUID ALReflectAnim::previewID(const LLUUID& id) const
{
    const LLUUID original = originalFor(id);
    auto asset = mAssets.find(original);
    if (asset != mAssets.end() && (contains(original) || isDerived(id)))
        return asset->second.state == Build::FAILED ? original : asset->second.derived;
    return id;
}
bool ALReflectAnim::finished(const LLUUID& original) const
{
    const auto asset = mAssets.find(original);
    return asset != mAssets.end() && asset->second.stopped;
}
bool ALReflectAnim::inworld(const LLUUID& original) const
{
    const auto found = mIntents.find(original);
    return found != mIntents.end() && found->second.route == ALReflect::Route::INWORLD;
}
ALReflect::Decision ALReflectAnim::decision(const LLUUID& original, ALReflect::Event event) const
{
    const auto found = mIntents.find(original);
    const F64 now = LLTimer::getTotalSeconds();
    return ALReflect::decide(found == mIntents.end() ? ALReflect::Intent::NONE : found->second.intent,
        found == mIntents.end() ? 0. : now - found->second.since, event);
}
bool ALReflectAnim::pending(const LLUUID& id) const
{
    const LLUUID original = originalFor(id);
    const auto asset = mAssets.find(original);
    if (!contains(original) || asset == mAssets.end()) return false;
    if (asset->second.state == Build::PENDING) return true;
    const auto* motion = gAgentAvatarp->findMotion(previewID(original));
    return asset->second.state == Build::READY && (!motion || motion->isStopped());
}
bool ALReflectAnim::play(const LLUUID& original, ALReflect::Route route, const std::string& name, bool enabled)
{
    if (!isAgentAvatarValid() || gDisconnected || isDerived(original)) return false;
    if (mAvatar != gAgentAvatarp->getID()) { clear(); mAvatar = gAgentAvatarp->getID(); }
    const bool member = contains(original);
    if (!member && !enabled)
    {
        mIntents.erase(original); // A later stock Play supersedes a previous Stop too.
        return false;
    }
    // A duplicate preview joins the same play. A Stop is always overwritten,
    // regardless of how long ago it happened or which echoes were coalesced.
    if (member && inworld(original)) route = ALReflect::Route::INWORLD;
    mIntents[original] = {decision(original, ALReflect::Event::USER_PLAY).intent,
        LLTimer::getTotalSeconds(), route};
    auto asset = mAssets.find(original);
    if (asset != mAssets.end()) asset->second.stopped = false;
    // Hide the original throughout the build; READY is reconciled below/at idle.
    gAgentAvatarp->stopMotion(original, true);
    if (asset == mAssets.end()) request(original, name);
    ensure(original);
    return true;
}
void ALReflectAnim::request(const LLUUID& original, const std::string& name)
{
    Asset asset; asset.name = name; original.combine(REFLECT_NAMESPACE, asset.derived);
    mAssets.emplace(original, std::move(asset));
    LLFileSystem file(original, LLAssetType::AT_ANIMATION, LLFileSystem::READ);
    if (file.getSize() > 0) build(original, true);
    else if (gAssetStorage)
        gAssetStorage->getAssetData(original, LLAssetType::AT_ANIMATION, loaded, new Fetch{mAvatar, mGeneration}, false);
    else build(original, false);
}
void ALReflectAnim::loaded(const LLUUID& original, LLAssetType::EType, void* data, S32 status, LLExtStat)
{
    std::unique_ptr<Fetch> fetch(static_cast<Fetch*>(data));
    if (instanceExists() && isAgentAvatarValid() && !gDisconnected && getInstance()->mAvatar == fetch->avatar && getInstance()->mGeneration == fetch->generation)
        getInstance()->build(original, status == 0);
}
bool ALReflectAnim::mirror(const LLUUID& original, Asset& asset)
{
    LLFileSystem file(original, LLAssetType::AT_ANIMATION, LLFileSystem::READ);
    const S32 size = file.getSize();
    auto fail = [&](const std::string& why) { asset.failure = why; return false; };
    if (size <= 0 || size > 64 * 1024 * 1024) return fail("empty or oversized asset");
    std::vector<U8> bytes(size);
    if (!file.read(bytes.data(), size)) return fail("short cache read");
    const F64 began = LLTimer::getTotalSeconds();
    U32 unresolved = 0, paired = 0, centre = 0, ground = 0, constraints = 0, joints = 0;
    S32 old_hand = 0, new_hand = 0;
    {
        Scratch probe;
        if (!probe.decode(bytes, original) || !probe.list()) return fail("original decode failed");
        auto* list = probe.list();
        std::map<std::string, std::set<std::string>> origins;
        for (auto* joint : list->mJointMotionArray)
        {
            const std::string name = joint->mJointName;
            joint->mJointName = ALReflect::partner(name);
            origins[joint->mJointName].insert(name);
            if (!gAgentAvatarp->getJoint(joint->mJointName)) ++unresolved; // unknown joints accepted like stock
            if (origins[joint->mJointName].size() > 1) return fail("renaming introduced duplicate joint: " + joint->mJointName);
            if (name == joint->mJointName) ++centre; else ++paired;
            for (auto& key : joint->mRotationCurve.mKeys)
            {
                key.second.mRotation = ALReflect::rotation(key.second.mRotation);
                if (!key.second.mRotation.isFinite()) return fail("non-finite rotation");
            }
            for (auto& key : joint->mPositionCurve.mKeys)
            {
                key.second.mPosition = ALReflect::vector(key.second.mPosition);
                if (!key.second.mPosition.isFinite()) return fail("non-finite position");
            }
        }
        auto remap = [](S32 index)
        {
            LLJoint* volume = index >= 0 ? gAgentAvatarp->findCollisionVolume(index) : nullptr;
            if (!volume) return -1;
            std::string name = ALReflect::partner(volume->getName());
            return gAgentAvatarp->getCollisionVolumeID(name);
        };
        for (auto* c : list->mConstraints)
        {
            ++constraints;
            c->mSourceConstraintVolume = remap(c->mSourceConstraintVolume);
            if (c->mSourceConstraintVolume < 0) return fail("unresolved source constraint volume");
            const bool body = c->mConstraintTargetType == CONSTRAINT_TARGET_TYPE_BODY;
            if (body)
            {
                c->mTargetConstraintVolume = remap(c->mTargetConstraintVolume);
                if (c->mTargetConstraintVolume < 0) return fail("unresolved BODY target constraint volume");
            }
            else if (c->mTargetConstraintOffset.mV[0] != 0.f || c->mTargetConstraintOffset.mV[1] != 0.f)
            {
                ++ground;
                LL_INFOS("GhostReflect") << "GROUND XY offset kept in agent frame" << LL_ENDL;
            }
            c->mSourceConstraintOffset = ALReflect::vector(c->mSourceConstraintOffset);
            c->mTargetConstraintOffset = ALReflect::constraintOffset(c->mTargetConstraintOffset, !body);
            c->mTargetConstraintDir = ALReflect::vector(c->mTargetConstraintDir);
            if (!c->mSourceConstraintOffset.isFinite() || !c->mTargetConstraintOffset.isFinite() || !c->mTargetConstraintDir.isFinite()) return fail("non-finite constraint vector");
        }
        old_hand = list->mHandPose; new_hand = ALReflect::handPose(old_hand);
        if (old_hand == 10 || old_hand == 12 || old_hand == 13)
            LL_INFOS("GhostReflect") << "one-sided hand pose kept: " << old_hand << LL_ENDL;
        list->mHandPose = (LLHandMotion::eHandPose)new_hand;
        joints = (U32)list->mJointMotionArray.size();
        asset.bytes.resize(probe.motion->getFileSize());
        LLDataPackerBinaryBuffer dp(asset.bytes.data(), (S32)asset.bytes.size());
        if (!probe.motion->serialize(dp)) return fail("mirrored serialize failed");
    } // probe removed, THEN its cache list; no derived motion exists yet
    static LLCachedControl<bool> verify(gSavedSettings, "GhostReflectVerify", true);
    if (verify)
    {
        const bool ok = roundTrip(bytes, asset.bytes, original);
        LL_INFOS("GhostReflect") << "round-trip " << original << ": " << (ok ? "PASS" : "FAIL") << LL_ENDL;
        if (!ok) return fail("independent round-trip mismatch");
    }
    LLKeyframeMotion* derived = dynamic_cast<LLKeyframeMotion*>(gAgentAvatarp->createMotion(asset.derived));
    LLDataPackerBinaryBuffer dp(asset.bytes.data(), (S32)asset.bytes.size());
    if (!derived || !derived->deserialize(dp, original, true))
    {
        gAgentAvatarp->removeMotion(asset.derived);
        LLKeyframeDataCache::removeKeyframeData(asset.derived);
        return fail("derived registration failed");
    }
    LL_INFOS("GhostReflect") << "anim " << original << " -> " << asset.derived << ": joints " << joints
        << " (" << paired << " paired, " << centre << " centre, " << unresolved << " unresolved/stock-accepted), constraints " << constraints
        << " (" << ground << " ground-offset notes), hand pose " << old_hand << "->" << new_hand
        << ", built in " << (LLTimer::getTotalSeconds() - began) * 1000. << " ms -> PASS" << LL_ENDL;
    return true;
}
void ALReflectAnim::build(const LLUUID& original, bool fetched)
{
    auto found = mAssets.find(original);
    if (found == mAssets.end() || found->second.state != Build::PENDING) return;
    Asset& asset = found->second;
    const bool ok = fetched && mirror(original, asset);
    asset.state = ok ? Build::READY : Build::FAILED;
    if (!ok)
        LL_WARNS("GhostReflect") << "anim " << original << " -> " << asset.derived
            << ": FAIL (" << (fetched ? asset.failure : "asset fetch failed") << "); local stock fallback only while signaled" << LL_ENDL;
    ensure(original);
}
void ALReflectAnim::ensure(const LLUUID& original)
{
    const auto asset = mAssets.find(original);
    if (!contains(original) || asset == mAssets.end()) return;
    const bool signaled = gAgentAvatarp->mSignaledAnimations.count(original) != 0;
    if (asset->second.state == Build::FAILED)
    {
        const auto result = ALReflect::failedDecision(mIntents.at(original).intent,
            mIntents.at(original).route, signaled);
        if (result.action == ALReflect::Action::STOP_LOCAL) stop(original);
        if (result.action == ALReflect::Action::PLAY_ORIGINAL)
        {
            LLMotion* motion = gAgentAvatarp->findMotion(original);
            if (!motion || motion->isStopped() || !gAgentAvatarp->isMotionActive(original))
                gAgentAvatarp->startMotion(original); // Local only: never resend START.
        }
        if (!asset->second.notified)
        {
            asset->second.notified = true;
            LLNotificationsUtil::add("SystemMessage", LLSD().with("MESSAGE", "Couldn't reflect " + asset->second.name + "; playing it normally."));
        }
        return;
    }
    // No unmirrored flash, even when a START arrives during an async build.
    LLMotion* original_motion = gAgentAvatarp->findMotion(original);
    if (original_motion && !original_motion->isStopped()) gAgentAvatarp->stopMotion(original, true);
    if (asset->second.state != Build::READY) return;
    LLMotion* motion = gAgentAvatarp->findMotion(asset->second.derived);
    const bool active = motion && !motion->isStopped() && gAgentAvatarp->isMotionActive(asset->second.derived);
    const bool loading = motion && gAgentAvatarp->getMotionController().isMotionLoading(motion);
    auto& playback = mIntents.at(original);
    const F64 now = LLTimer::getTotalSeconds();
    const auto result = ALReflect::readyDecision(playback, active, loading, now);
    if (result.action == ALReflect::Action::DEFER_STOP) { stop(original, true); return; }
    if (result.action != ALReflect::Action::ENSURE_DERIVED || (inworld(original) && !signaled)) return;
    playback.requested = true;
    playback.requestedAt = now;
    gAgentAvatarp->startMotion(asset->second.derived);
    // Success can mean queued in mLoadingMotions. isMotionActive delegates to
    // isActive(), set by LLMotion::activate(), unlike setStopped(false) there.
    // Observe running playback on the next ensure; never infer it from success.
    LL_INFOS("GhostReflect") << "self start " << original << " as " << asset->second.derived << LL_ENDL;
}
void ALReflectAnim::stopLocal(const LLUUID& original)
{
    gAgentAvatarp->stopMotion(original, true);
    const auto asset = mAssets.find(original);
    if (asset != mAssets.end())
    {
        asset->second.stopped = true;
        gAgentAvatarp->stopMotion(asset->second.derived, true);
    }
}
bool ALReflectAnim::echo(const LLUUID& original, bool begin)
{
    const auto result = decision(original, begin ? ALReflect::Event::REGION_START : ALReflect::Event::REGION_STOP);
    if (result.intent == ALReflect::Intent::NONE) mIntents.erase(original);
    switch (result.action)
    {
    case ALReflect::Action::ENSURE_DERIVED:
        // Normally requested by Play; also covers a missing registry entry.
        if (!mAssets.count(original)) request(original, original.asString());
        ensure(original);
        return true;
    case ALReflect::Action::CONSUME:
    case ALReflect::Action::STOP_LOCAL:
        stopLocal(original);
        return true;
    default: return false; // Includes an expired STOP: caller runs stock start/stop.
    }
}
void ALReflectAnim::stop(const LLUUID& original, bool natural)
{
    auto found = mIntents.find(original);
    if (found == mIntents.end()) return;
    const auto result = decision(original, natural ? ALReflect::Event::NATURAL_END : ALReflect::Event::USER_STOP);
    if (result.action == ALReflect::Action::NONE) return;
    found->second.intent = result.intent;
    found->second.since = LLTimer::getTotalSeconds();
    found->second.deferredStop = natural;
    if (natural) found->second.completed = true;
    const auto asset = mAssets.find(original);
    if (asset != mAssets.end()) asset->second.stopped = true;
    if (natural) return; // Never mutate the controller's active list in H3.
    stopLocal(original);
    if (inworld(original)) gAgent.sendAnimationRequest(original, ANIM_REQUEST_STOP);
}
void ALReflectAnim::idle(void* data)
{
    auto& self = *static_cast<ALReflectAnim*>(data);
    if (!isAgentAvatarValid() || gDisconnected || (!self.mAvatar.isNull() && self.mAvatar != gAgentAvatarp->getID()))
    { self.clear(); return; }
    for (auto it = self.mIntents.begin(); it != self.mIntents.end(); )
    {
        const LLUUID original = it->first;
        if (it->second.deferredStop)
        {
            it->second.deferredStop = false;
            self.stopLocal(original);
            gAgent.onAnimStop(original);
            if (self.inworld(original))
            {
                gAgent.sendAnimationRequest(original, ANIM_REQUEST_STOP);
                LL_INFOS("GhostReflect") << "natural end: STOP(original) sent " << original << LL_ENDL;
            }
        }
        const auto result = self.decision(original, ALReflect::Event::IDLE);
        if (result.intent == ALReflect::Intent::NONE)
        {
            it = self.mIntents.erase(it);
            continue;
        }
        if (result.action == ALReflect::Action::ENSURE_DERIVED)
        {
            // Level triggered: Play/READY must work even without a fresh H2
            // callback when the region coalesces START into an existing signal.
            self.ensure(original);
        }
        ++it;
    }
}
