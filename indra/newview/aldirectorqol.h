/**
 * Small, UI-free helpers shared by Director QoL features and their tests.
 */
#ifndef AL_DIRECTOR_QOL_H
#define AL_DIRECTOR_QOL_H

#include "lluuid.h"
#include "stdtypes.h"

#include <string>
#include <vector>

namespace ALDirectorQol
{
// 0 means no match. Exact and prefix matches deliberately outrank fuzzy ones.
S32 fuzzyTokenScore(const std::string& query, const std::string& candidate);

enum class SearchField
{
    LABEL,
    CARD,
    KEYWORD,
    HELP,
    SETTING,
    WIDGET,
    SYNONYM
};

// Field-aware search scoring shared by the runtime index and its unit tests.
S32 searchTokenScore(const std::string& query, const std::string& candidate,
                     SearchField field);
bool searchResultBefore(S32 left_score, bool left_external, U32 left_order,
                        S32 right_score, bool right_external, U32 right_order);

std::vector<LLUUID> subjectAssignment(const std::vector<LLUUID>& roster_order,
                                      U32 limit = 4);

enum class RecoveryAction
{
    NONE,
    OFFER_RESTORE,
    DISCARD_CORRUPT
};

RecoveryAction recoveryAction(bool file_exists, bool parsed_scene);
} // namespace ALDirectorQol

#endif
