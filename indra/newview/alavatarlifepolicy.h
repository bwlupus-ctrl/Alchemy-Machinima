/**
 * @file alavatarlifepolicy.h
 * @brief Frame-cached viewer policy for ambient avatar life clocks.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef AL_AVATARLIFEPOLICY_H
#define AL_AVATARLIFEPOLICY_H

#include "alavataranimationtime.h"
#include "llviewercontrol.h"

namespace ALAvatarLifePolicy
{
inline void update()
{
    static U32 last_frame = 0xFFFFFFFF;
    const U32 frame = LLFrameTimer::getFrameCount();
    if (last_frame != frame)
    {
        static LLCachedControl<bool> keep_life(gSavedSettings, "WorldPauseKeepAvatarLife", false);
        ALAvatarAnimationTime::sKeepLife = keep_life;
        last_frame = frame;
    }
}
}
#endif
