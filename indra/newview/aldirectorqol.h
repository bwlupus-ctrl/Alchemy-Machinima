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

// ---------------------------------------------------------------------------
// Crash recovery. Autosave always runs; a previous session's live autosave file is
// MOVED into <scenes>/crash_backups/ at login, where the newest kCrashBackupKeep are
// kept. Everything below is pure so the policy is unit-tested without the viewer.
// ---------------------------------------------------------------------------
struct BackupStamp
{
    S32 year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
};

constexpr size_t kCrashBackupKeep = 2;  // newest crash backups kept (plus the live autosave); older ones are pruned
constexpr size_t kCorruptBackupKeep = 3; // damaged (.corrupt) files kept; never counted in the 2

// Largest scene file Director writes (autosave and manual save) and, as the same constant, reads. A
// scene whose serialized XML is larger is not written: the previous file stays as it was.
constexpr size_t kDirectorSceneMaxBytes = 8u * 1024u * 1024u;
// True when a serialized scene of this many bytes may be written / read.
bool sceneSizeAllowed(size_t bytes);

// Live autosave slots: slot 1 is ".director_recovery.xml", slot N>1 is
// ".director_recovery_N.xml". Autosave writes to the first slot that holds no unmoved
// previous-session backup, so it can never overwrite one and can never be disabled.
std::string liveRecoveryFileName(S32 slot);
// 0 when the name is not a live slot file (an exact ".director_recovery[_N].xml").
S32 liveRecoverySlot(const std::string& file_name);
constexpr S32 kMaxLiveSlots = 2;         // the main slot plus ONE spare; beyond that the overflow name is used
// The lowest slot in 1..max_slot that is not in unmoved_slots, or 0 when every slot is occupied.
// NEVER returns an occupied slot (no overwrite is possible); the caller then uses an overflow name.
S32 autosaveSlot(const std::vector<S32>& unmoved_slots, S32 max_slot = kMaxLiveSlots);
// Used when every slot is occupied: ".director_recovery-<pid>-YYYYMMDDHHMMSS[-N].xml". Unique per
// process and second (counter N >= 1 for a repeat), so it can never overwrite anything.
std::string overflowRecoveryFileName(S32 pid, const BackupStamp& stamp, S32 counter = 0);
bool isOverflowRecoveryName(const std::string& file_name);
// A live autosave file of any kind: a slot file or an overflow file. The login move collects all of them.
bool isLiveRecoveryFile(const std::string& file_name);
// The previous build's ".director_recovery_kept.xml" and its ".kept-<stamp>" rotations.
bool isLegacyKeptBackupName(const std::string& file_name);

// "scene-YYYYMMDD-HHMMSS[-p<pid>][-N].xml": the pid (when > 0) keeps two viewers that move a backup in the
// same second from generating the same name; N >= 1 is the repeat counter.
std::string crashBackupFileName(const BackupStamp& stamp, S32 counter = 0, S32 pid = 0);
// The first of the plain name, then -1, -2, ... that is not in existing (compared
// case-insensitively, like the file system). Never returns an existing name.
std::string uniqueCrashBackupName(const BackupStamp& stamp, const std::vector<std::string>& existing, S32 pid = 0);

struct ParsedBackupName
{
    bool valid = false;
    U64 stamp = 0;    // YYYYMMDDHHMMSS as one integer, so it sorts chronologically
    S32 counter = 0;  // the -N suffix (0 when absent)
};
ParsedBackupName parseCrashBackupName(const std::string& file_name);
// Newest first. Sorted by the parsed timestamp, then the numeric -N counter (so -10 is
// newer than -9); names that do not parse sort as the OLDEST. Never lexical.
bool crashBackupNewer(const std::string& left, const std::string& right);
std::vector<std::string> sortCrashBackupsNewestFirst(std::vector<std::string> names);
// Names to delete so at most keep remain, OLDEST first. Names in protected_names
// (in use, just restored or just offered) are never returned.
std::vector<std::string> crashBackupsToPrune(std::vector<std::string> names, size_t keep,
                                             const std::vector<std::string>& protected_names);

// "<name>.corrupt", or "<name>.N.corrupt" when taken (case-insensitive).
std::string uniqueCorruptName(const std::string& backup_name, const std::vector<std::string>& existing);
// Same policy for the .corrupt files, ordered by the backup name they came from (oldest first).
std::vector<std::string> corruptBackupsToPrune(std::vector<std::string> names, size_t keep);

// What happens at login to the newest crash backup created this login. mode is
// DirectorRecoveryMode: 0 Ask, 1 Restore automatically, 2 Do nothing (keep the backup).
enum class LoginAction
{
    NONE,         // no backup this login
    OFFER,        // prompt
    RESTORE_AUTO, // restore with a toast
    KEEP          // leave it in crash_backups
};
LoginAction loginAction(bool have_new_backup, S32 mode);

// The prompt's buttons: 0 Restore, 1 Discard. Anything else (closed, ignored, a
// Preferences-disabled prompt with no saved response) keeps the backup.
// remember: "Don't ask me again" was ticked, so Discard KEEPS this backup (the tick only sets the mode).
// replayed: the answer was not given by the user now (the popup is disabled in Preferences and a saved
// answer was replayed): it can never restore or delete.
enum class PromptAction
{
    RESTORE,
    DISCARD, // deletes only the backup this prompt described
    KEEP
};
PromptAction promptAction(S32 option, bool remember, bool replayed);
// The "Don't ask me again" mapping: ticked Restore -> 1, ticked Discard -> 2, otherwise -1 (unchanged).
S32 rememberedRecoveryMode(S32 option, bool remember);

// A scene name that would collide with a backup or recovery file is refused in
// Save / Save as / Rename: a leading '.', "crash_backups", and the recovery names,
// compared case-insensitively (the recovery names are also covered by the leading '.').
bool isReservedSceneName(const std::string& name);

} // namespace ALDirectorQol

#endif
