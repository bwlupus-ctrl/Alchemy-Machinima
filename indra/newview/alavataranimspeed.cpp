/** @file alavataranimspeed.cpp */
#include "llviewerprecompiledheaders.h"
#include "alavataranimspeed.h"
#include "alghoststudio.h"
#include "llagent.h"
#include "llcharacter.h"
#include "llcontrolavatar.h"
#include "lldirectorcast.h"
#include "llghostavatar.h"
#include "llsdutil.h"
#include "llviewercontrol.h"
#include "llviewerjointattachment.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"
#include "llvovolume.h"
#include <algorithm>
#include <cmath>
#include <random>

ALAvatarAnimSpeed& ALAvatarAnimSpeed::instance()
{
    static ALAvatarAnimSpeed owner;
    return owner;
}

ALAvatarAnimSpeed::ALAvatarAnimSpeed()
    : mSlowMotion(gSavedSettings.getBOOL("SlowMotionAnimation"))
{
}

F32 ALAvatarAnimSpeed::clampSpeed(F32 speed)
{
    return std::clamp(speed, 0.05f, 4.f);
}

F32 ALAvatarAnimSpeed::compose(F32 speed, F32 slow_factor)
{
    // Clamp the USER value, never the product: .05x with slow motion is .01x.
    return speed * slow_factor;
}

F32 ALAvatarAnimSpeed::variedSpeed(F32 spread, F32 unit)
{
    return 1.f + (2.f * unit - 1.f) * std::clamp(spread, 0.f, 50.f) * 0.01f;
}

bool ALAvatarAnimSpeed::isGhost(const LLUUID& id)
{
    return ALGhostStudio::instance().findInstanceByRuntime(id) ||
        dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(id));
}

F32 ALAvatarAnimSpeed::getSpeed(const LLUUID& id) const
{
    if (const auto* inst = ALGhostStudio::instance().findInstanceByRuntime(id))
        return inst->mAnimSpeed;
    const auto found = mSpeeds.find(id);
    return found == mSpeeds.end() ? 1.f : found->second;
}

void ALAvatarAnimSpeed::setSpeed(const LLUUID& id, F32 speed)
{
    if (id.isNull() || !std::isfinite(speed)) return;
    speed = clampSpeed(speed);
    // Studio remains the sole clone writer, including its Chaos composition.
    if (const auto* inst = ALGhostStudio::instance().findInstanceByRuntime(id))
    {
        ALGhostStudio::instance().setInstanceAnimSpeed(inst->mId, speed);
        ++mRevision;
        return;
    }
    // Overlay/dummy avatars mirror a source pose and never get a clock override.
    auto* avatar = dynamic_cast<LLVOAvatar*>(gObjectList.findObject(id));
    if (isGhost(id) || (avatar && (avatar->mIsDummy || avatar->isControlAvatar() || avatar->isUIAvatar()))) return;
    if (getSpeed(id) == speed) return;
    if (speed == 1.f) mSpeeds.erase(id);
    else mSpeeds[id] = speed;
    ++mRevision;
    if (LLDirectorCast::instanceExists()) LLDirectorCast::instance().touchScene();
    if (avatar) apply(*avatar);
}

void ALAvatarAnimSpeed::setSlowMotion(bool enabled)
{
    mSlowMotion = enabled;
    if (auto* avatar = dynamic_cast<LLVOAvatar*>(gObjectList.findObject(gAgentID))) apply(*avatar);
}

void ALAvatarAnimSpeed::applyObject(LLViewerObject& object, F32 factor) const
{
    if (LLControlAvatar* control = object.getControlAvatar(); control && !control->isDead())
        control->setAnimTimeFactor(factor);
    for (const auto& child : object.getChildren())
        if (child && !child->isDead()) applyObject(*child.get(), factor);
}

void ALAvatarAnimSpeed::apply(LLVOAvatar& avatar) const
{
    // No scan, timer or idle callback. Called only on edits/rez/attachment events.
    if (avatar.isDead() || avatar.mIsDummy || avatar.isControlAvatar() || avatar.isUIAvatar() ||
        dynamic_cast<LLGhostAvatar*>(&avatar)) return;
    const F32 factor = compose(getSpeed(avatar.getID()),
        avatar.getID() == gAgentID && mSlowMotion ? 0.2f : 1.f);
    avatar.setAnimTimeFactor(factor);
    for (const auto& point : avatar.mAttachmentPoints)
        for (const auto& object : point.second->mAttachedObjects)
            if (object && !object->isDead()) applyObject(*object.get(), factor);
}

void ALAvatarAnimSpeed::applyControl(LLControlAvatar& control) const
{
    // Clone linksets can acquire a control avatar before they have a wearer.
    // Studio owns these local copies from their creation, even while unparented.
    if (control.mRootVolp && control.mRootVolp->isLocalOnly() &&
        !control.mRootVolp->isLocalMeshPreview()) return;
    LLVOAvatar* wearer = control.getAttachedAvatar();
    if (wearer && (wearer->mIsDummy || dynamic_cast<LLGhostAvatar*>(wearer))) return;
    const F32 factor = wearer ? compose(getSpeed(wearer->getID()),
        wearer->getID() == gAgentID && mSlowMotion ? 0.2f : 1.f) : 1.f;
    control.setAnimTimeFactor(factor);
}

std::set<LLUUID> ALAvatarAnimSpeed::targets() const
{
    std::set<LLUUID> ids;
    if (gAgentID.notNull()) ids.insert(gAgentID);
    for (const auto& member : LLDirectorCast::instance().getCast())
        if (member.mId.notNull()) ids.insert(member.mId);
    for (const auto& inst : ALGhostStudio::instance().getInstances())
        if (inst.mKind == ALGhostStudio::BACKING_ENTITY_CLONE && inst.mEntityId.notNull())
            ids.insert(inst.mEntityId);
    for (LLCharacter* character : LLCharacter::sInstances)
        if (auto* avatar = dynamic_cast<LLVOAvatar*>(character);
            avatar && !avatar->isDead() && !avatar->isControlAvatar() && !avatar->mIsDummy && !avatar->isUIAvatar())
            ids.insert(avatar->getID());
    return ids;
}

S32 ALAvatarAnimSpeed::varyAll(F32 spread)
{
    if (!std::isfinite(spread)) return 0;
    static std::mt19937 random(std::random_device{}());
    std::uniform_real_distribution<F32> unit(0.f, 1.f);
    S32 changed = 0;
    for (const LLUUID& id : targets())
    {
        const F32 speed = variedSpeed(spread, unit(random));
        const F32 previous = getSpeed(id);
        setSpeed(id, speed);
        if (getSpeed(id) != previous) ++changed;
    }
    return changed;
}

S32 ALAvatarAnimSpeed::resetResidents()
{
    const auto previous = mSpeeds;
    mSpeeds.clear();
    for (const auto& entry : previous)
        if (auto* avatar = dynamic_cast<LLVOAvatar*>(gObjectList.findObject(entry.first))) apply(*avatar);
    if (!previous.empty())
    {
        ++mRevision;
        if (LLDirectorCast::instanceExists()) LLDirectorCast::instance().touchScene();
    }
    return static_cast<S32>(previous.size());
}

S32 ALAvatarAnimSpeed::resetAll()
{
    S32 changed = resetResidents();
    for (const LLUUID& id : targets())
        if (isGhost(id) && getSpeed(id) != 1.f)
        {
            setSpeed(id, 1.f);
            ++changed;
        }
    return changed;
}

LLSD ALAvatarAnimSpeed::sceneData() const
{
    LLSD data = LLSD::emptyArray();
    for (const auto& [id, speed] : mSpeeds)
        if (!isGhost(id))
            data.append(LLSD().with("avatar", id).with("speed", speed));
    return data;
}

void ALAvatarAnimSpeed::applySceneData(const LLSD& scene)
{
    if (!scene.has("avatar_anim_speeds")) return; // old scenes leave session state unchanged
    const LLSD& data = scene["avatar_anim_speeds"];
    if (!data.isArray()) return;
    // Validate the entire block before replacing the resident overrides.
    std::map<LLUUID, F32> restored;
    for (const LLSD& entry : llsd::inArray(data))
    {
        if (!entry.isMap() || !entry["avatar"].isUUID() ||
            !(entry["speed"].isReal() || entry["speed"].isInteger())) return;
        const LLUUID id = entry["avatar"].asUUID();
        const F64 speed = entry["speed"].asReal();
        if (id.isNull() || !std::isfinite(speed)) return;
        if (!isGhost(id)) restored[id] = clampSpeed(static_cast<F32>(speed));
    }
    resetResidents();
    for (const auto& [id, speed] : restored) setSpeed(id, speed);
}
