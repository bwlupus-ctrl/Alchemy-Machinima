#ifndef AL_RIG_GROUP_SUBJECT_H
#define AL_RIG_GROUP_SUBJECT_H

#include "lluuid.h"
#include <array>

struct ALRigBorrowChoice
{
    S32 mSlot = -1; // zero based A-D; -1 means ask, never silently replace
    S32 mReplacement = 0;
};

// Pure borrowing policy. Subjects are read-only, including when every rig is busy.
inline ALRigBorrowChoice alRigBorrowChoice(const std::array<LLUUID, 4>& subjects,
    const std::array<LLUUID, 4>& anchors, const std::array<bool, 4>& enabled,
    const std::array<U64, 4>& used, const LLUUID& self, const LLUUID& actor)
{
    ALRigBorrowChoice result;
    if (actor.isNull() || actor == self) return result;
    for (S32 slot = 0; slot < 4; ++slot)
        if (anchors[slot] == actor) { result.mSlot = slot; return result; }
    for (S32 slot = 0; slot < 4; ++slot)
        if (anchors[slot].isNull() && subjects[slot] == actor)
            { result.mSlot = slot; return result; }
    for (S32 slot = 0; slot < 4; ++slot)
        if (!enabled[slot] && subjects[slot].isNull() && anchors[slot].isNull())
            { result.mSlot = slot; return result; }
    for (S32 slot = 1; slot < 4; ++slot)
        if (used[slot] < used[result.mReplacement]) result.mReplacement = slot;
    return result;
}

#endif
