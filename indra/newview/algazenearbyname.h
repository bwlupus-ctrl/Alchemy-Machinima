/**
 * @file algazenearbyname.h
 * @brief Shared person-name rule for gaze targets that may not be cast members.
 *
 * A gaze target (a cast-ref) can be any avatar, so every picker, label and cue list that names one
 * goes through this: the name cache's DISPLAY name only (never a user or legacy name, never a
 * UUID), withheld while RLV name hiding applies. Empty means "unknown": callers show a generic
 * word, never the id.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_GAZE_NEARBY_NAME_H
#define AL_GAZE_NEARBY_NAME_H

#include "algazenearby.h"
#include "llavatarname.h"
#include "llavatarnamecache.h"
#include "rlvactions.h"

namespace ALGazeNearby
{
inline std::string personDisplayName(const LLUUID& id)
{
    LLAvatarName name;
    if (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, id) || !LLAvatarNameCache::get(id, &name))
    {
        return std::string();
    }
    return name.getDisplayName(true);
}

// RLV @shownearby is independent of name hiding: while it applies, no nearby person is enumerated
// or offered by any picker, and a saved non-cast target is shown only by a neutral label.
inline bool nearbyAllowed()
{
    return RlvActions::canShowNearbyAgents();
}

// Name of a gaze target who is NOT a cast member (a Nearby pick): the display name, withheld under
// name hiding AND under @shownearby. Empty means unknown (callers use a generic label).
inline std::string nonCastTargetName(const LLUUID& id)
{
    return nearbyAllowed() ? personDisplayName(id) : std::string();
}
}

#endif // AL_GAZE_NEARBY_NAME_H
