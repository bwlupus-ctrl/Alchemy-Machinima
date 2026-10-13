#include "linden_common.h"

#include "aldirectorqol.h"

#include "llstring.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace
{
S32 editDistance(const std::string& left, const std::string& right, S32 ceiling)
{
    constexpr size_t MAX_FUZZY_TOKEN = 128;
    if (std::abs((S32)left.size() - (S32)right.size()) > ceiling)
        return ceiling + 1;
    if (left.size() >= MAX_FUZZY_TOKEN || right.size() >= MAX_FUZZY_TOKEN)
        return ceiling + 1;
    std::array<S32, MAX_FUZZY_TOKEN> previous{}, current{};
    for (S32 column = 0; column <= (S32)right.size(); ++column)
        previous[column] = column;
    for (S32 row = 1; row <= (S32)left.size(); ++row)
    {
        current[0] = row;
        S32 row_best = current[0];
        for (S32 column = 1; column <= (S32)right.size(); ++column)
        {
            current[column] = llmin(previous[column] + 1,
                                    llmin(current[column - 1] + 1,
                                          previous[column - 1] +
                                              (left[row - 1] == right[column - 1] ? 0 : 1)));
            row_best = llmin(row_best, current[column]);
        }
        if (row_best > ceiling)
            return ceiling + 1;
        previous.swap(current);
    }
    // previous holds the last DP row; its live width is right.size() + 1.
    return previous[right.size()];
}

bool isSubsequence(const std::string& needle, const std::string& haystack)
{
    size_t at = 0;
    for (char character : haystack)
        if (at < needle.size() && needle[at] == character)
            ++at;
    return at == needle.size();
}
} // namespace

S32 ALDirectorQol::fuzzyTokenScore(const std::string& query_value,
                                   const std::string& candidate_value)
{
    std::string query = query_value;
    std::string candidate = candidate_value;
    LLStringUtil::toLower(query);
    LLStringUtil::toLower(candidate);
    if (query.empty() || candidate.empty())
        return 0;
    if (query == candidate)
        return 120;
    if (candidate.compare(0, query.size(), query) == 0)
        return 105;
    if (candidate.find(query) != std::string::npos)
        return 90;
    if (query.size() >= 3 && isSubsequence(query, candidate))
        return 65 - llmin(20, (S32)candidate.size() - (S32)query.size());
    const S32 ceiling = query.size() < 5 ? 1 : 2;
    const S32 distance = editDistance(query, candidate, ceiling);
    return distance <= ceiling ? 58 - distance * 8 : 0;
}

S32 ALDirectorQol::searchTokenScore(const std::string& query,
                                    const std::string& candidate,
                                    SearchField field)
{
    static constexpr S32 WEIGHTS[] = { 80, 55, 45, 25, 90, 15, 5 };
    const S32 fuzzy = fuzzyTokenScore(query, candidate);
    return fuzzy ? WEIGHTS[static_cast<U32>(field)] + fuzzy : 0;
}

bool ALDirectorQol::searchResultBefore(S32 left_score, bool left_external,
                                       U32 left_order, S32 right_score,
                                       bool right_external, U32 right_order)
{
    if (left_score != right_score)
        return left_score > right_score;
    if (left_external != right_external)
        return !left_external;
    return left_order < right_order;
}

std::vector<LLUUID> ALDirectorQol::subjectAssignment(
    const std::vector<LLUUID>& roster_order, U32 limit)
{
    std::vector<LLUUID> result;
    result.reserve(llmin(limit, (U32)roster_order.size()));
    for (const LLUUID& id : roster_order)
    {
        if (id.notNull() && std::find(result.begin(), result.end(), id) == result.end())
            result.push_back(id);
        if (result.size() == limit)
            break;
    }
    return result;
}

ALDirectorQol::RecoveryAction ALDirectorQol::recoveryAction(bool file_exists,
                                                             bool parsed_scene)
{
    if (!file_exists)
        return RecoveryAction::NONE;
    return parsed_scene ? RecoveryAction::OFFER_RESTORE : RecoveryAction::DISCARD_CORRUPT;
}
