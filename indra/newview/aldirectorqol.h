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
// Inputs are pre-lowercased by the search index/query boundary. 0 means no
// match. Exact and prefix matches deliberately outrank fuzzy ones.
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

// What to do at login with the live recovery file (a crash backup from the
// previous session). mode is DirectorRecoveryMode: 0 Ask, 1 Restore
// automatically, 2 Don't restore (keep the backup aside).
enum class LoginRecoveryAction
{
    NONE,
    OFFER,
    RESTORE_AUTO,
    KEEP_ASIDE,
    DISCARD_CORRUPT
};
LoginRecoveryAction loginRecoveryAction(bool file_exists, bool parsed_scene, S32 mode);

// A previous-session backup in the live recovery file that nobody has answered
// yet must not be overwritten, so it blocks autosave and the shutdown write.
// A kept (moved aside) backup lives in its own file and never blocks anything.
bool autosaveAllowed(bool live_backup_unresolved);
// The unresolved flag after a Restore / Discard / Keep step. It only clears when
// the step really succeeded, so a failed rename or delete never reports success.
bool stillUnresolved(bool was_unresolved, bool step_succeeded);

// "Restore last unsaved scene" only ever restores a crash backup from a PREVIOUS
// session: the kept file, or the live file found at login that is still
// unresolved. This session's own rolling autosave is never eligible.
enum class RestoreSource
{
    NONE,
    KEPT,
    PREVIOUS_LIVE
};
RestoreSource restoreSource(bool kept_exists, bool live_exists, bool live_unresolved);

enum class RestoreFollowUp
{
    CONSUME,    // restored: delete the source backup
    KEEP_ASIDE, // the unresolved live backup failed to restore: move it to the kept file
    LEAVE       // nothing to do (no source, or a kept backup that failed stays as it is)
};
RestoreFollowUp restoreFollowUp(RestoreSource source, bool restored);

// Older kept backups are named so that sorting ascending is oldest first. Returns
// the names to delete so that at most max_older remain (the oldest go first).
std::vector<std::string> keptBackupsToPrune(std::vector<std::string> older_names,
                                            size_t max_older);
} // namespace ALDirectorQol

#endif
