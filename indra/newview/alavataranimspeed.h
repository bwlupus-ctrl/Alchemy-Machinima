/** @file alavataranimspeed.h
 * Session/scene owner of resident avatar playback clocks. Main thread only.
 */
#ifndef AL_AVATAR_ANIM_SPEED_H
#define AL_AVATAR_ANIM_SPEED_H

#include "llsd.h"
#include "lluuid.h"
#include <map>
#include <set>

class LLVOAvatar;
class LLControlAvatar;
class LLViewerObject;

class ALAvatarAnimSpeed
{
public:
    static ALAvatarAnimSpeed& instance();
    static F32 clampSpeed(F32 speed);
    static F32 compose(F32 speed, F32 slow_factor);
    static F32 variedSpeed(F32 spread, F32 unit);
    F32 getSpeed(const LLUUID& id) const;
    void setSpeed(const LLUUID& id, F32 speed);
    void setSlowMotion(bool enabled);
    S32 varyAll(F32 spread);
    S32 resetAll();
    std::set<LLUUID> targets() const;
    void apply(LLVOAvatar& avatar) const;
    void applyAttachment(LLVOAvatar& avatar, LLViewerObject& object) const;
    void applyControl(LLControlAvatar& control) const;
    LLSD sceneData() const;
    void applySceneData(const LLSD& scene);
    U64 revision() const { return mRevision; }

private:
    ALAvatarAnimSpeed();
    static bool isGhost(const LLUUID& id);
    void applyObject(LLViewerObject& object, F32 factor) const;
    S32 resetResidents();
    std::map<LLUUID, F32> mSpeeds;
    bool mSlowMotion = false;
    U64 mRevision = 1;
};
#endif
