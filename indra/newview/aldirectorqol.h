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
