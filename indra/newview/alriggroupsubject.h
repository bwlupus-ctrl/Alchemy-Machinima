#ifndef AL_RIG_GROUP_SUBJECT_H
#define AL_RIG_GROUP_SUBJECT_H

#include "lluuid.h"
#include <array>

// Reuse an existing Subject, otherwise the first empty slot. Never replace one.
inline S32 alRigGroupSubject(const std::array<LLUUID, 4>& subjects,
                             const LLUUID& self, const LLUUID& avatar)
{
    if (avatar.isNull() || avatar == self)
        return -1;
    for (S32 slot = 0; slot < 4; ++slot)
        if (subjects[slot] == avatar)
            return slot;
    for (S32 slot = 0; slot < 4; ++slot)
        if (subjects[slot].isNull())
            return slot;
    return -1;
}

#endif
