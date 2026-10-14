/**
 * @file aldirectoractorchoice.h
 * @brief Pure, viewer-free identity merge for Director actor/subject pickers.
 */
#ifndef AL_DIRECTOR_ACTOR_CHOICE_H
#define AL_DIRECTOR_ACTOR_CHOICE_H

#include "lluuid.h"

#include <array>
#include <string>
#include <vector>

struct ALActorChoiceSource
{
    LLUUID      mId;
    std::string mLabel;
};

struct ALActorChoice
{
    LLUUID      mId;
    std::string mLabel;
    std::string mSubjectLetters;
    U8          mSubjectMask = 0;

    bool isSubject(S32 slot) const
    {
        return slot >= 1 && slot <= 4 &&
               (mSubjectMask & (1u << (slot - 1))) != 0;
    }
};

// Self is always first (as "You"); cast and subject identities follow once each
// in stable order. Subject letters are presentation metadata, never new identities.
std::vector<ALActorChoice> alBuildActorChoices(const LLUUID& self_id,
                                               const std::array<LLUUID, 4>& subjects,
                                               const std::vector<ALActorChoiceSource>& cast);

#endif // AL_DIRECTOR_ACTOR_CHOICE_H
