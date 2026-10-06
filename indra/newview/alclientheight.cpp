/**
 * @file alclientheight.cpp
 * @brief [ClientHeight] Session-only store of local avatar height offsets.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alclientheight.h"

ALClientHeight::ALClientHeight()
{
}

// static
LLUUID ALClientHeight::keyForAvatar(const LLVOAvatar* av, ALAvatarKey::EKind* out_kind)
{
    return ALAvatarKey::forAvatar(av, out_kind, true);
}

bool ALClientHeight::set(const LLUUID& key, F32 z)
{
    if (!mStore.set(key, z))
    {
        return false;
    }
    mChangeSignal();
    return true;
}

bool ALClientHeight::reset(const LLUUID& key)
{
    if (!mStore.reset(key))
    {
        return false;
    }
    mChangeSignal();
    return true;
}

bool ALClientHeight::resetAll()
{
    if (!mStore.resetAll())
    {
        return false;
    }
    mChangeSignal();
    return true;
}
