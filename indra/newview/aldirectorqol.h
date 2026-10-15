/**
 * Small, UI-free helpers shared by Director QoL features and their tests.
 */
#ifndef AL_DIRECTOR_QOL_H
#define AL_DIRECTOR_QOL_H

#include "lluuid.h"
#include "stdtypes.h"

#include <cstdint>
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

// Largest scene file Director writes (manual save, autosave) and reads (manual load, crash-backup reads). A
// scene whose serialized XML is larger is not written: the previous file stays as it was. Fixed, not a setting.
constexpr size_t kDirectorSceneMaxBytes = 64u * 1024u * 1024u;
// True when a serialized scene of this many bytes may be written / read.
bool sceneSizeAllowed(size_t bytes);
// After an oversized autosave refusal no new attempt is made until the scene changes or this long passes.
constexpr double kOversizedAutosaveRetrySeconds = 300.0;

// Live autosave file, one per viewer process: ".director_recovery-p<pid>.xml", or
// ".director_recovery-p<pid>-<n>.xml" (n >= 1) when the plain name was taken by a stale leftover of a crashed
// process whose pid this one reused (see ownLiveRecoveryName). A viewer only ever writes and deletes its own
// live file, so concurrent viewers never share a slot. The pre-pid name ".director_recovery.xml" is legacy:
// it is only ever collected (never written).
std::string liveRecoveryFileName(S32 pid, S32 counter = 0);
// Largest pid the name can carry: S32 max, 10 digits.
constexpr S32 kMaxLiveRecoveryPid = 2147483647;
// The pid of a ".director_recovery-p<pid>[-<n>].xml" name (pid 1..kMaxLiveRecoveryPid, 1 to 10 digits, no
// leading zero; n 1 to 6 digits, no leading zero), or 0 when the name is not one.
S32 liveRecoveryPid(const std::string& file_name);
// The live file name THIS process writes this session: its plain pid name, or the first "-<n>" variant that
// is not in existing_live_names (the live files still present after the login move, compared
// case-insensitively). A stale file under the plain name that could not be moved is therefore never
// overwritten; it stays for the next login's collection.
std::string ownLiveRecoveryName(S32 pid, const std::vector<std::string>& existing_live_names);
// Each live file has a companion "<live file name>.lock", held open exclusively by the viewer that owns the
// live file for its whole session. A lock file is never itself a live file or a backup.
std::string liveRecoveryLockName(const std::string& live_file_name);
// True for a "<live file name>.lock" name (a pid file, a pid file with -<n>, or the legacy plain name).
bool isLiveRecoveryLockFile(const std::string& file_name);
// Dedupe decision for a live file whose content may already sit in crash_backups (only the POSIX link()
// fallback can leave such a copy: the destination was linked but the source could not be removed). True only
// when the sizes are equal AND the caller's byte-for-byte comparison found the contents equal. Size and
// modification time alone never justify deleting a file.
bool backupIsDuplicate(S64 source_size, S64 candidate_size, bool contents_equal);
bool isLegacyLiveRecoveryName(const std::string& file_name);
// A live autosave file of any kind: a pid file or the legacy plain name.
bool isLiveRecoveryFile(const std::string& file_name);
// True when a live file belongs to a previous session and must be moved into crash_backups at login: every
// legacy file, and every pid file whose pid is not this process. This process's own pid file counts as
// previous-session until it has written one (own_file_written == false): at login such a file can only be a
// stale leftover of a crashed process whose pid was reused.
bool isPreviousSessionLiveFile(const std::string& file_name, S32 my_pid, bool own_file_written);
// The previous build's ".director_recovery_kept.xml" and its ".kept-<stamp>" rotations.
bool isLegacyKeptBackupName(const std::string& file_name);
// A legacy plain ".director_recovery.xml" (written by builds that never lock) is collected at login only when its
// modification time is older than kLegacyLiveRecoveryMinAgeSeconds: an older running viewer autosaves at least
// every 5 minutes, so a fresher file may belong to one. Unix seconds; a modification time in the future is not old.
constexpr S64 kLegacyLiveRecoveryMinAgeSeconds = 600;
bool legacyLiveFileOldEnough(S64 now_seconds, S64 mtime_seconds);

// Ownership locks on live autosave files (protocol in llfloaterdirector.cpp). A lock is held by the viewer that
// owns the live file; the collector takes the same lock to prove that owner is dead.
enum class LockResult
{
    ACQUIRED,
    HELD,   // another process holds it: the file's owner is running
    FAILED  // could not tell (e.g. the lock file cannot be created); the collector treats it like HELD
};
struct LiveLock
{
    // Windows HANDLE or POSIX fd; -1 when not held. Opaque, so this header needs no platform headers.
    intptr_t native = -1;
    std::string path;
    bool held() const { return native != -1; }
};
// Takes the exclusive lock on lock_path, creating the lock file when missing. Calling it on a lock already held
// returns ACQUIRED and changes nothing.
LockResult acquireLiveLock(const std::string& lock_path, LiveLock& lock);
// Drops the lock and removes the lock file, but only while that path still names the file that was locked.
// Safe on an unheld lock.
void releaseLiveLock(LiveLock& lock);

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
// A replayed response never changes the mode (the user did not answer now).
S32 rememberedRecoveryMode(S32 option, bool remember, bool replayed = false);
// Whether the handler may call setIgnored(false) on the shown prompt: never for a replayed response (that
// would re-enable a popup the user disabled in Preferences).
bool mayClearPromptIgnore(bool replayed);

// Cache key of a crash-backup file (name, size, mtime): a file found damaged or oversized is not re-read
// until the key changes.
std::string backupFileKey(const std::string& file_name, S64 size, S64 mtime);

// A scene name that would collide with a backup or recovery file is refused in
// Save / Save as / Rename: a leading '.', "crash_backups", and the recovery names,
// compared case-insensitively (the recovery names are also covered by the leading '.').
bool isReservedSceneName(const std::string& name);

} // namespace ALDirectorQol

#endif
