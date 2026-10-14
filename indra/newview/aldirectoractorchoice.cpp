/**
 * @file aldirectoractorchoice.cpp
 * @brief Pure, viewer-free identity merge for Director actor/subject pickers.
 */
#include "linden_common.h"

#include "aldirectoractorchoice.h"

#include <algorithm>

std::vector<ALActorChoice> alBuildActorChoices(const LLUUID& self_id,
                                               const std::array<LLUUID, 4>& subjects,
                                               const std::vector<ALActorChoiceSource>& cast)
{
    std::vector<ALActorChoice> result;
    result.reserve(cast.size() + 5);

    const auto same_identity = [&self_id](const LLUUID& left, const LLUUID& right)
    {
        const bool left_self = left.isNull() || (self_id.notNull() && left == self_id);
        const bool right_self = right.isNull() || (self_id.notNull() && right == self_id);
        return (left_self && right_self) || left == right;
    };
    const auto find_choice = [&result, &same_identity](const LLUUID& id) -> ALActorChoice*
    {
        const auto found = std::find_if(result.begin(), result.end(),
            [&id, &same_identity](const ALActorChoice& choice)
            { return same_identity(choice.mId, id); });
        return found == result.end() ? nullptr : &*found;
    };

    result.push_back({ self_id, "You", std::string(), 0 });
    for (const ALActorChoiceSource& source : cast)
    {
        if (source.mId.isNull() || find_choice(source.mId))
        {
            continue;
        }
        result.push_back({ source.mId,
            source.mLabel.empty() ? source.mId.asString() : source.mLabel,
            std::string(), 0 });
    }
    for (S32 slot = 0; slot < 4; ++slot)
    {
        const LLUUID& id = subjects[slot];
        if (id.isNull())
        {
            continue;
        }
        ALActorChoice* choice = find_choice(id);
        if (!choice)
        {
            result.push_back({ id, std::string(), std::string(), 0 });
            choice = &result.back();
        }
        choice->mSubjectMask |= U8(1u << slot);
        if (!choice->mSubjectLetters.empty())
        {
            choice->mSubjectLetters += ", ";
        }
        choice->mSubjectLetters += char('A' + slot);
    }
    for (ALActorChoice& choice : result)
    {
        if (!choice.mSubjectLetters.empty())
        {
            if (choice.mLabel.empty())
            {
                choice.mLabel = choice.mSubjectLetters.size() == 1
                    ? "Subject " + choice.mSubjectLetters
                    : "Subjects " + choice.mSubjectLetters;
            }
            else
            {
                choice.mLabel += choice.mSubjectLetters.size() == 1
                    ? " (Subject " + choice.mSubjectLetters + ")"
                    : " (Subjects " + choice.mSubjectLetters + ")";
            }
        }
    }
    return result;
}
