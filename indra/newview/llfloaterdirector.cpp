/**
 * @file llfloaterdirector.cpp
 * @brief Director Console floater -- see llfloaterdirector.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "aldirectortabmigration.h"
#include "aldirectorqol.h"
#include "aldirectortheme.h"
#include "alcameracurve.h"
#include "alkitshell.h"
#include "alkitbinding.h"
#include "alkitbar.h"
#include "alkithistory.h"
#include "alkitpage.h"
#include "alkitpopover.h"
#include "alkitrow.h"
#include "alkitstyle.h"

#include "llfloaterdirector.h"
#include "alghoststudio.h" // [AutoAnimate]
#include "aluirefresh.h"            // [UIPerfC1] diff-guarded text writes

#include "indra_constants.h"        // KEY_ESCAPE / MASK_NONE, MASK_ALT

#include "alfloateractorgaze.h"
#include "alfloaterlightbox.h"
#include "alavatarlightresponse.h"
#include "alpanelactormover.h"      // embedded shared Actor Mover transport
#include "alpanelanimpreview.h"     // embedded shared preview pane + own-avatar controls
#include "alpanelcinecamparams.h"   // embedded shared panel (scene preset hooks)
#include "alpanellensgaze.h"
#include "alpanelpatheditor.h"      // embedded shared Actor Pathing editor
#include "alpanelsteppedanim.h"
#include "alsteppedanim.h"
#include "alcinelightrigmanager.h"
#include "alrotoink.h"
#include "altron.h"
#include "alkitpresets.h"
#include "alpanelcinelightrig.h"
#include "alcinehaze.h"                // Atmosphere > Depth Haze reset and reference height
#include "allenslooks.h"               // Lens > Filters infrared and soft-edge preset actions
#include "alpanelweathersettings.h"    // World > Weather strike and reset-all
#include "alprojectorshaftpresets.h"   // Atmosphere projector-shaft presets
#include <boost/signals2.hpp>
#include <cmath>
#include "llviewercamera.h"
#include "aldirectorswitcher.h"
#include "llactormover.h"
#include "llagent.h"                   // stable self id while avatar rebuilds
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "llcheckboxctrl.h"
#include "llclipboard.h"
#include "llcombobox.h"
#include "llcontrolavatar.h"        // animesh control-avatar resolution (Add Animesh to Cast)
#include "lldir.h"                  // gDirUtilp (scene files)
#include "lldirectorcast.h"
#include "lldiriterator.h"          // scene file listing
#include "lldndbutton.h"
#include "llcinematiccamera.h"
#include "llfile.h"                 // LLFile::mkdir/remove, ll*fstream
#include "llfloaterreg.h"
#include "llflycamrecorder.h"
#include "llghostavatar.h"
#include "llkeyframemotion.h"       // priority readout (signaled-anim list)
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"    // scene delete confirm
#include "llpresentationtime.h"
#include "llprismlens.h"
#include "llviewerjoystick.h"       // drone preset apply/markCustom for the kit Takes page
#include "llscrolllistctrl.h"
#include "llsdserialize.h"          // scene LLSD XML files
#include "llselectmgr.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"             // group start-delay spinner (Groups section)
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltooldraganddrop.h"
#include "lluictrlfactory.h"
#include "lluri.h"                  // LLURI::escape scene filenames
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl
#include "llviewermenu.h"           // gMenuHolder, LLViewerMenuHolderGL
#include "llviewerobject.h"         // getRootEdit/isAnimatedObject/getControlAvatar
#include "llviewerobjectlist.h"     // gObjectList
#include "llmotion.h"               // LLMotion::setPriorityOverride (play-local priority)
#include "llvoavatar.h"
#include "llvoavatarself.h"         // gAgentAvatarp, isAgentAvatarValid()
#include "llvovolume.h"
#include "pipeline.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <set>
#include <sstream>
#include <unordered_map>

#if LL_WINDOWS
#include "llwin32headers.h"   // MoveFileExW, GetCurrentProcessId
#else
#include <errno.h>
#include <fcntl.h>            // AT_FDCWD, open
#include <stdio.h>            // renamex_np / RENAME_EXCL (macOS)
#include <sys/file.h>         // flock (live-file ownership locks)
#include <sys/syscall.h>      // SYS_renameat2 (Linux)
#include <unistd.h>           // link / unlink / getpid / syscall
#endif

namespace
{
// cast-row status glyphs (existing skin icons, poser iconography rule)
constexpr char ICON_MOVING[]   = "Move_Walk_Off";
constexpr char ICON_IDLE[]     = "Profile_Friend_Online";
constexpr char ICON_GONE[]     = "Profile_Friend_Offline";
constexpr char ICON_MARK[]     = "Flag";

constexpr F32 PRISM_SNAPSHOT_POLL_SECONDS = 0.25f;

LLHandle<LLFloater> sRecoveryDirectorHandle;
bool sRecoveryServiceStarted = false;
bool sRecoveryOfferActive = false;
bool sRecoveryOfferPending = false;
bool sRecoveryShutdownPrepared = false;
// Autosave always runs, into this process's own ".director_recovery-p<pid>.xml". Every other live file
// (another pid, the legacy plain name) is MOVED into crash_backups at login; one that could not be moved
// yet (a transient lock) waits here (full paths) and is retried on later ticks. Autosave never touches it.
std::vector<std::string> sUnmovedLiveBackups;
S32 sMoveRetriesLeft = 0;
// Live-slot backups moved into crash_backups at this login (base names); the newest is offered.
std::vector<std::string> sLoginMovedNames;
// Base names pruning must never delete (the offered / just restored backup).
std::vector<std::string> sProtectedBackups;
// This session's own autosave file (empty until written): removed when the scene is saved or on a clean exit.
std::string sOwnLivePath;
// Settings list change detection (names + display data), so the 1 s refresh is cheap.
std::string sCrashBackupSignature;

constexpr const char* OLD_FLAT_TABS[] = {
    "move_tab", "path_tab", "ghosts_tab", "props_tab", "animate_tab", "camera_tab",
    "takes_tab", "projector_volumetrics_tab", "cine_light_rig_tab", "weather_tab",
    "atmospheric_volumes_tab", "temporal_tab", "actor_style_tab"
};

std::string migrate_tab_name(const std::string& name)
{
    return ALDirectorTabMigration::migrateTabName(name);
}

std::string legacy_tab_name(const std::string& name)
{
    if (name == "shot_tab") return "camera_tab";
    if (name == "atmosphere_tab") return "projector_volumetrics_tab";
    return name;
}

bool can_start_manual_subject_shot(std::string* reason)
{
    if (!LLDirectorCast::instance().resolveSubjectA())
    {
        if (reason) *reason = "Set a live Director Subject A first";
        return false;
    }
    if (ALDirectorSwitcher::instance().isDrivingCamera())
    {
        if (reason) *reason =
            "Stop the Director Switcher before starting a manual shot";
        return false;
    }
    if (reason) reason->clear();
    return true;
}

// scene files live beside the cinematic presets, same idiom
constexpr char SCENE_SUBDIR[]  = "director_scenes";
// Crash backups: the previous session's live autosave file is MOVED here at login.
constexpr char CRASH_BACKUP_SUBDIR[] = "crash_backups";
constexpr S32 MOVE_RETRIES = 10;     // one attempt per 1 s tick

// Results of restoreCrashBackup().
constexpr S32 RESTORE_OK = 0;
constexpr S32 RESTORE_MISSING = 1;     // the file is gone
constexpr S32 RESTORE_UNREADABLE = 2;  // could not be opened or read: kept untouched
constexpr S32 RESTORE_DAMAGED = 3;     // opened, read fully and does not parse: set aside as .corrupt
constexpr S32 RESTORE_FAILED = 4;      // parsed but the scene would not load: kept
constexpr S32 RESTORE_TOO_LARGE = 5;   // over ALDirectorQol::kDirectorSceneMaxBytes: never read, kept

enum class ReadResult
{
    OK,
    UNREADABLE, // could not open or finish reading: never treated as corrupt, never deleted
    TOO_LARGE,  // over ALDirectorQol::kDirectorSceneMaxBytes: never parsed, never deleted
    CORRUPT     // opened, read fully, and it does not parse as a scene
};

// "64 MB" for messages, from the one cap constant.
std::string sceneCapLabel()
{
    return llformat("%u MB", (U32)(ALDirectorQol::kDirectorSceneMaxBytes / (1024u * 1024u)));
}

// Read and parse a scene file. The whole file is read into memory first (open, size, byte count and
// stream state all checked) and the stream is closed before returning, so the caller may rename or
// delete it (an open handle makes that fail on Windows). Anything short of a complete read is UNREADABLE;
// only a parse failure on fully read bytes is CORRUPT. (The XML parser reads through a streambuf, so
// the stream's own bad() flag after a parse proves nothing about the read.)
ReadResult readSceneFileClosed(const std::string& path, LLSD& scene_out)
{
    std::string bytes;
    {
        llstat status;
        if (LLFile::stat(path, &status) != 0)
            return ReadResult::UNREADABLE;
        const S64 size = (S64)status.st_size;
        if (size < 0)
            return ReadResult::UNREADABLE;
        if (!ALDirectorQol::sceneSizeAllowed((size_t)size))
            return ReadResult::TOO_LARGE;
        llifstream in(path.c_str(), std::ios::in | std::ios::binary);
        if (!in.is_open())
            return ReadResult::UNREADABLE;
        bytes.resize((size_t)size);
        if (size > 0)
        {
            in.read(&bytes[0], (std::streamsize)size);
            if (in.bad() || in.gcount() != (std::streamsize)size)
                return ReadResult::UNREADABLE;
        }
        if (in.bad() || in.peek() != std::char_traits<char>::eof())
            return ReadResult::UNREADABLE; // the file changed size while it was read
    }
    std::istringstream stream(bytes);
    const S32 parsed = LLSDSerialize::fromXML(scene_out, stream);
    return (parsed != LLSDParser::PARSE_FAILURE && scene_out.isMap()) ? ReadResult::OK
                                                                       : ReadResult::CORRUPT;
}

bool fileModifiedTm(const std::string& path, std::tm& local_time)
{
    llstat status;
    if (LLFile::stat(path, &status) != 0)
        return false;
    const time_t modified = (time_t)status.st_mtime;
#if LL_WINDOWS
    return localtime_s(&local_time, &modified) == 0;
#else
    return localtime_r(&modified, &local_time) != nullptr;
#endif
}

ALDirectorQol::BackupStamp stampFromTm(const std::tm& local_time)
{
    ALDirectorQol::BackupStamp stamp;
    stamp.year = local_time.tm_year + 1900;
    stamp.month = local_time.tm_mon + 1;
    stamp.day = local_time.tm_mday;
    stamp.hour = local_time.tm_hour;
    stamp.minute = local_time.tm_min;
    stamp.second = llmin(local_time.tm_sec, 59);
    return stamp;
}

// "3:23 PM" (12-hour clock, no leading zero).
std::string clockLabel(S32 hour24, S32 minute)
{
    const S32 hour12 = hour24 % 12 == 0 ? 12 : hour24 % 12;
    return llformat("%d:%02d %s", hour12, minute, hour24 < 12 ? "AM" : "PM");
}

// "3:23 PM" from a file's modification time, or "an unknown time".
std::string fileTimeLabel(const std::string& path)
{
    std::tm local_time;
    if (!fileModifiedTm(path, local_time))
        return "an unknown time";
    return clockLabel(local_time.tm_hour, local_time.tm_min);
}

// "Oct 15, 3:23 PM": from the backup's name when it parses, else its modification time.
std::string backupWhenLabel(const std::string& path)
{
    static const char* const MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    ALDirectorQol::BackupStamp stamp;
    const ALDirectorQol::ParsedBackupName parsed =
        ALDirectorQol::parseCrashBackupName(gDirUtilp->getBaseFileName(path, false));
    if (parsed.valid)
    {
        const U64 value = parsed.stamp; // YYYYMMDDHHMMSS
        stamp.year = (S32)(value / 10000000000ULL);
        stamp.month = (S32)(value / 100000000ULL % 100);
        stamp.day = (S32)(value / 1000000ULL % 100);
        stamp.hour = (S32)(value / 10000ULL % 100);
        stamp.minute = (S32)(value / 100ULL % 100);
        stamp.second = (S32)(value % 100);
    }
    else
    {
        std::tm local_time;
        if (!fileModifiedTm(path, local_time))
            return "an unknown time";
        stamp = stampFromTm(local_time);
    }
    return llformat("%s %d, %s", MONTHS[llclamp(stamp.month, 1, 12) - 1], stamp.day,
                    clockLabel(stamp.hour, stamp.minute).c_str());
}

// Delete a backup file; true when it is really gone. Never assumes success.
bool removeBackupFile(const std::string& path)
{
    if (!gDirUtilp->fileExists(path))
        return true;
    LLFile::remove(path, ENOENT);
    if (gDirUtilp->fileExists(path))
    {
        LL_WARNS("Director") << "Could not delete Director backup " << path << LL_ENDL;
        return false;
    }
    return true;
}

std::vector<std::string> listFileNames(const std::string& dir, const std::string& pattern)
{
    std::vector<std::string> names;
    if (!gDirUtilp->fileExists(dir))
        return names;
    LLDirIterator dir_iter(dir, pattern);
    std::string file;
    while (dir_iter.next(file))
        names.push_back(file);
    return names;
}

S32 currentProcessId()
{
#if LL_WINDOWS
    return (S32)GetCurrentProcessId();
#else
    return (S32)getpid();
#endif
}

// ---------------------------------------------------------------------------
// Live-file ownership locks (liveness of another viewer's autosave).
//
// scenesDir() is shared by every viewer on the machine, so a live autosave file that is not ours may belong
// to a viewer that is still RUNNING; it must not be collected into crash_backups (that would even auto-restore
// one viewer's scene into another). A pid check cannot tell (pids are reused), so ownership is a held lock:
// each viewer creates "<its live file>.lock" and keeps it open EXCLUSIVELY for its whole session. The OS
// drops the lock when the process dies, however it dies. The collector tries to take the same lock: if it
// cannot, a running viewer owns the file and it is SKIPPED; if it can, the owner is dead and the file is
// collected. Lock files are never backups (isLiveRecoveryFile does not match them).
//   Windows: CreateFileW with dwShareMode = 0 and FILE_FLAG_DELETE_ON_CLOSE (the lock file vanishes when the
//            handle closes, including when the process dies).
//   POSIX:   open + flock(LOCK_EX | LOCK_NB); the file is unlinked while the lock is still held.
// Any failure leans toward SKIPPING a file for now (it is collected at a later login), never toward
// collecting a live viewer's file. Failing to take OUR lock only logs: autosave still runs.
// ---------------------------------------------------------------------------
// The lock type and its acquire/release are ALDirectorQol helpers (aldirectorqol.cpp), unit-tested there.
using ALDirectorQol::LockResult;
using ALDirectorQol::LiveLock;
using ALDirectorQol::acquireLiveLock;
using ALDirectorQol::releaseLiveLock;

// This session's live file name (empty until the service starts) and the lock held on it.
std::string sOwnLiveName;
LiveLock sOwnLock;

// Take our lock if we do not hold it (called at login and before every autosave write). Failure never stops
// autosave. Every distinct failure (another lock file, or held vs. could-not-create) is logged; a repeat of the
// same failure is logged at most once a minute, since autosave calls this on every write.
void ensureOwnLiveLock(const std::string& live_path)
{
    if (sOwnLock.held())
        return;
    const std::string lock_path = ALDirectorQol::liveRecoveryLockName(live_path);
    const LockResult result = acquireLiveLock(lock_path, sOwnLock);
    if (result == LockResult::ACQUIRED)
        return;
    static std::string sLastWarnKey;
    static F64 sLastWarnTime = 0.0;
    const std::string key = lock_path + (result == LockResult::HELD ? "|held" : "|failed");
    const F64 now = LLTimer::getTotalSeconds();
    if (key != sLastWarnKey || now - sLastWarnTime >= 60.0)
    {
        sLastWarnKey = key;
        sLastWarnTime = now;
        LL_WARNS("Director") << "Could not lock the Director autosave file " << live_path << " ("
                             << (result == LockResult::HELD ? "held by another process" : "lock file unavailable")
                             << "); autosave continues, but another viewer could mistake it for a crashed one" << LL_ENDL;
    }
}

// Choose this session's live file name and lock it. Runs once at login, BEFORE moveLiveBackupsAtLogin: the lock is
// taken first, so a collector in another viewer can never hold this session's future slot. A stale same-pid
// leftover (a crashed process whose pid this one reused) is still present at this point, so autosave must not
// write over it: a unique "-<n>" variant is used instead, and the leftover is collected by this login's move
// (or waits for a later login when it cannot be moved yet).
void beginOwnLiveSession(const std::string& scenes_dir)
{
    std::vector<std::string> live_names;
    for (const std::string& file : listFileNames(scenes_dir, ".director_recovery*"))
        if (ALDirectorQol::isLiveRecoveryFile(file))
            live_names.push_back(file);
    const S32 pid = currentProcessId();
    sOwnLiveName = ALDirectorQol::ownLiveRecoveryName(pid, live_names);
    if (sOwnLiveName != ALDirectorQol::liveRecoveryFileName(pid))
        LL_WARNS("Director") << "A stale Director autosave file with this process id could not be moved; autosave uses "
                             << sOwnLiveName << " instead" << LL_ENDL;
    ensureOwnLiveLock(gDirUtilp->add(scenes_dir, sOwnLiveName));
}

// May a legacy plain ".director_recovery.xml" (never locked by its writer) be collected now? Only when it is old
// enough that no running older viewer is autosaving it AND no other viewer process of this executable is running.
// Logs the reason when it may not. path is the full path, file its base name.
bool legacyLiveFileCollectable(const std::string& path, const std::string& file)
{
    llstat legacy_stat;
    if (LLFile::stat(path, &legacy_stat) != 0 ||
        !ALDirectorQol::legacyLiveFileOldEnough(static_cast<S64>(time(nullptr)), static_cast<S64>(legacy_stat.st_mtime)))
    {
        LL_INFOS("Director") << "Legacy Director autosave " << file << " is too recent to collect; left for a later login"
                             << LL_ENDL;
        return false;
    }
    if (ALDirectorQol::otherViewerProcessRunning())
    {
        LL_INFOS("Director") << "Legacy Director autosave " << file << " may belong to a running viewer; left for a later login"
                             << LL_ENDL;
        return false;
    }
    return true;
}

enum class MoveResult
{
    MOVED,
    EXISTS, // the target name is taken: nothing was changed, pick another name
    FAILED  // the source is left exactly where it was
};

// Rename that can NEVER replace an existing target, atomically (no exists()-then-rename gap that two
// viewers starting in the same second could both pass).
//   Windows: MoveFileExW without MOVEFILE_REPLACE_EXISTING fails if the target exists.
//   Linux:   renameat2(RENAME_NOREPLACE); macOS: renamex_np(RENAME_EXCL). Both are atomic and fail with
//            EEXIST when the target exists.
//   Fallback (call unsupported by the kernel / file system): link() fails with EEXIST if the target exists,
//            then the source is unlinked. If that unlink fails the destination is KEPT (the data is safe in
//            crash_backups) and the source is reported as MOVED_SOURCE_KEPT; the next login recognises it by a
//            byte-for-byte content comparison (identicalBackupName) and does not migrate it twice.
//            A file system with neither atomic call nor hard links
//            FAILS the move (retried later): a plain replacing rename is never used.
enum class MoveKind
{
    MOVED,
    MOVED_SOURCE_KEPT // the destination exists and is complete; removing the source failed
};
MoveResult renameNoReplace(const std::string& source, const std::string& target, MoveKind& kind)
{
    kind = MoveKind::MOVED;
#if LL_WINDOWS
    if (MoveFileExW(ll_convert_string_to_wide(source).c_str(), ll_convert_string_to_wide(target).c_str(), 0) != 0)
        return MoveResult::MOVED;
    const DWORD error = GetLastError();
    return (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS) ? MoveResult::EXISTS : MoveResult::FAILED;
#else
    {
        int atomic_result = -1;
        bool atomic_available = false;
#if LL_DARWIN && defined(RENAME_EXCL)
        atomic_available = true;
        atomic_result = ::renamex_np(source.c_str(), target.c_str(), RENAME_EXCL);
#elif LL_LINUX && defined(SYS_renameat2)
        atomic_available = true;
        atomic_result = (int)::syscall(SYS_renameat2, AT_FDCWD, source.c_str(), AT_FDCWD, target.c_str(),
                                       1u /* RENAME_NOREPLACE */);
#endif
        if (atomic_available)
        {
            if (atomic_result == 0)
                return MoveResult::MOVED;
            if (errno == EEXIST)
                return MoveResult::EXISTS;
            if (errno != ENOSYS && errno != EINVAL && errno != ENOTSUP && errno != EOPNOTSUPP && errno != EPERM)
                return MoveResult::FAILED;
            // unsupported here: fall back to link()
        }
    }
    if (::link(source.c_str(), target.c_str()) == 0)
    {
        if (::unlink(source.c_str()) != 0)
        {
            LL_WARNS("Director") << "Moved Director backup to " << target << " but could not remove " << source
                                 << "; the copy is kept and the source will not be migrated again" << LL_ENDL;
            kind = MoveKind::MOVED_SOURCE_KEPT;
        }
        return MoveResult::MOVED;
    }
    if (errno == EEXIST)
        return MoveResult::EXISTS;
    return MoveResult::FAILED; // no atomic rename and no hard links: retry later rather than risk an overwrite
#endif
}

// Byte-for-byte comparison of two files of the same known size (both capped at kDirectorSceneMaxBytes by the
// caller), streamed in 64 KB blocks. Any open/read problem or length mismatch is "not equal".
bool fileContentsEqual(const std::string& path_a, const std::string& path_b, S64 expected_size)
{
    llifstream in_a(path_a.c_str(), std::ios::in | std::ios::binary);
    llifstream in_b(path_b.c_str(), std::ios::in | std::ios::binary);
    if (!in_a.is_open() || !in_b.is_open())
        return false;
    constexpr std::streamsize BLOCK = 64 * 1024;
    std::vector<char> block_a((size_t)BLOCK), block_b((size_t)BLOCK);
    S64 total = 0;
    for (;;)
    {
        in_a.read(block_a.data(), BLOCK);
        in_b.read(block_b.data(), BLOCK);
        const std::streamsize got_a = in_a.gcount();
        const std::streamsize got_b = in_b.gcount();
        if (got_a != got_b || std::memcmp(block_a.data(), block_b.data(), (size_t)got_a) != 0)
            return false;
        total += got_a;
        if (got_a < BLOCK)
            break;
    }
    return !in_a.bad() && !in_b.bad() && total == expected_size;
}

// Only the POSIX link() fallback of renameNoReplace can leave a source whose content already sits in
// crash_backups (linked, but the source could not be unlinked). Nowhere else is a file deleted for being a
// copy of another.
constexpr bool DEDUPE_LINK_FALLBACK_SOURCE = !LL_WINDOWS;

// The name of a crash backup whose CONTENT equals source (same size and every byte equal), looked for only in
// the POSIX link-fallback case; empty otherwise. A size / modification-time match alone never counts. Such a
// source is not migrated twice.
std::string identicalBackupName(const std::string& source, const std::string& crash_dir,
                                const std::vector<std::string>& existing_names)
{
    if (!DEDUPE_LINK_FALLBACK_SOURCE)
        return std::string();
    llstat source_status;
    if (LLFile::stat(source, &source_status) != 0 || (S64)source_status.st_size < 0 ||
        !ALDirectorQol::sceneSizeAllowed((size_t)source_status.st_size))
        return std::string();
    for (const std::string& name : existing_names)
    {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (lower.size() < 4 || lower.compare(lower.size() - 4, 4, ".xml") != 0)
            continue;
        const std::string candidate = gDirUtilp->add(crash_dir, name);
        llstat status;
        if (LLFile::stat(candidate, &status) != 0 || (S64)status.st_size != (S64)source_status.st_size)
            continue;
        const bool equal = fileContentsEqual(source, candidate, (S64)source_status.st_size);
        if (ALDirectorQol::backupIsDuplicate((S64)source_status.st_size, (S64)status.st_size, equal))
            return name;
    }
    return std::string();
}

// MOVE a live / legacy-kept file into the crash-backup folder under a unique
// scene-YYYYMMDD-HHMMSS-p<pid>[-N].xml name stamped with its last-write time. Never overwrites an
// existing file (renameNoReplace; a taken name just moves on to the next -N). Returns the new base
// name, or an empty string when the move failed (the source is left exactly where it was) -- the
// caller retries later.
std::string moveIntoCrashBackups(const std::string& source, const std::string& crash_dir,
                                 std::vector<std::string>& existing_names)
{
    std::tm local_time;
    if (!fileModifiedTm(source, local_time))
    {
        const time_t now = time(nullptr);
#if LL_WINDOWS
        localtime_s(&local_time, &now);
#else
        localtime_r(&now, &local_time);
#endif
    }
    const ALDirectorQol::BackupStamp stamp = stampFromTm(local_time);
    const S32 pid = currentProcessId();
    // Already migrated (a previous move linked it but could not remove the source): do not duplicate it.
    const std::string identical = identicalBackupName(source, crash_dir, existing_names);
    if (!identical.empty())
    {
        LL_INFOS("Director") << "Director backup " << source << " is already in crash_backups/" << identical << LL_ENDL;
        removeBackupFile(source);
        return identical;
    }
    for (S32 attempt = 0; attempt < 16; ++attempt)
    {
        const std::string name = ALDirectorQol::uniqueCrashBackupName(stamp, existing_names, pid);
        const std::string target = gDirUtilp->add(crash_dir, name);
        MoveKind kind;
        switch (renameNoReplace(source, target, kind))
        {
        case MoveResult::MOVED:
            existing_names.push_back(name);
            return name;
        case MoveResult::EXISTS:
            existing_names.push_back(name); // appeared since the listing; pick the next -N
            continue;
        case MoveResult::FAILED:
            LL_WARNS("Director") << "Could not move Director backup " << source << " -> " << target << LL_ENDL;
            return std::string();
        }
    }
    return std::string();
}

// Keep the newest ALDirectorQol::kCrashBackupKeep backups; never prune a protected one.
void pruneCrashBackups(const std::string& crash_dir)
{
    for (const std::string& doomed : ALDirectorQol::crashBackupsToPrune(
             listFileNames(crash_dir, "*.xml"), ALDirectorQol::kCrashBackupKeep, sProtectedBackups))
    {
        LL_INFOS("Director") << "Pruning old Director crash backup " << doomed << LL_ENDL;
        removeBackupFile(gDirUtilp->add(crash_dir, doomed));
    }
}

// A file that opened, read fully and failed to parse is renamed to <name>.corrupt (never
// deleted); at most kCorruptBackupKeep of those are kept and they never count toward the 5.
bool quarantineCorruptBackup(const std::string& crash_dir, const std::string& path)
{
    const std::string name = gDirUtilp->getBaseFileName(path, false);
    const std::string corrupt = ALDirectorQol::uniqueCorruptName(name, listFileNames(crash_dir, "*.corrupt"));
    const std::string target = gDirUtilp->add(crash_dir, corrupt);
    const bool moved = !gDirUtilp->fileExists(target) && LLFile::rename(path, target, ENOENT) == 0 &&
                       !gDirUtilp->fileExists(path);
    if (!moved)
    {
        LL_WARNS("Director") << "Could not set the damaged Director backup " << path << " aside" << LL_ENDL;
        return false;
    }
    LL_WARNS("Director") << "Set damaged Director backup aside as " << target << LL_ENDL;
    for (const std::string& doomed : ALDirectorQol::corruptBackupsToPrune(
             listFileNames(crash_dir, "*.corrupt"), ALDirectorQol::kCorruptBackupKeep))
        removeBackupFile(gDirUtilp->add(crash_dir, doomed));
    return true;
}

void recoveryToast(const std::string& message)
{
    LLNotificationsUtil::add("SystemMessageTip", LLSD().with("MESSAGE", message));
}

// Save / Save as / Rename refuse names that collide with backup or recovery files.
bool refuseReservedSceneName(const std::string& name)
{
    if (!ALDirectorQol::isReservedSceneName(name))
        return false;
    LL_WARNS("Director") << "Refused reserved Director scene name '" << name << "'" << LL_ENDL;
    recoveryToast("'" + name + "' can't be used as a scene name: it is reserved for Director backup files. "
                  "Choose a different name.");
    return true;
}

// Toast for a restore attempt. The backup file is kept in every case.
void restoreResultToast(S32 result, const std::string& when, bool automatic)
{
    switch (result)
    {
    case RESTORE_OK:
        recoveryToast((automatic ? "Restored your unsaved Director scene from " : "Restored the crash backup from ") +
                      when + ".");
        break;
    case RESTORE_MISSING:
        recoveryToast("That crash backup no longer exists.");
        break;
    case RESTORE_DAMAGED:
        recoveryToast("The crash backup from " + when +
                      " is damaged. It was set aside as a .corrupt file in the crash_backups folder.");
        break;
    case RESTORE_TOO_LARGE:
        recoveryToast("This crash backup is larger than " + sceneCapLabel() + " and can't be restored.");
        break;
    default:
        recoveryToast("Couldn't restore your unsaved Director scene; the backup was kept. "
                      "You can try again from Director > Settings.");
        break;
    }
}

// Scene name shown in the Settings list, read once per backup. A transiently failed read is retried every few
// seconds until it succeeds (the list signature includes the readable state, so the row updates then). A file
// found damaged or oversized is NOT re-read until its (name, size, mtime) key changes.
struct BackupSceneName
{
    std::string name;
    F64 next_try = 0.0;
    bool done = false;
    std::string failed_key; // key of the file last classified CORRUPT / TOO_LARGE
};
std::unordered_map<std::string, BackupSceneName> sBackupSceneNames;

// True once the backup's scene name has been read: part of the list signature.
bool crashBackupNameReadable(const std::string& file_name)
{
    auto it = sBackupSceneNames.find(file_name);
    return it != sBackupSceneNames.end() && it->second.done;
}

const std::string& crashBackupSceneName(const std::string& crash_dir, const std::string& file_name)
{
    BackupSceneName& entry = sBackupSceneNames[file_name];
    const F64 now = LLTimer::getTotalSeconds();
    if (!entry.done && now >= entry.next_try)
    {
        const std::string path = gDirUtilp->add(crash_dir, file_name);
        std::string key;
        llstat status;
        if (LLFile::stat(path, &status) == 0)
            key = ALDirectorQol::backupFileKey(file_name, (S64)status.st_size, (S64)status.st_mtime);
        if (!key.empty() && key == entry.failed_key)
            return entry.name; // already classified damaged / oversized and unchanged: do not re-parse
        entry.next_try = now + 5.0;
        LLSD scene;
        switch (readSceneFileClosed(path, scene))
        {
        case ReadResult::OK:
            entry.name = scene["scene_name"].isString() ? scene["scene_name"].asString() : std::string();
            entry.done = true;
            entry.failed_key.clear();
            break;
        case ReadResult::CORRUPT:
        case ReadResult::TOO_LARGE:
            entry.failed_key = key;
            break;
        case ReadResult::UNREADABLE:
            break; // transient: retried on the slow timer
        }
    }
    return entry.name;
}

constexpr S32  SCENE_VERSION   = 6;

const std::string TOOLTIP_COUNTDOWN("Counting down. Click to cancel");
const std::string TOOLTIP_CUT("Stop everything ACTION started");
const std::string TOOLTIP_SET_MARKS("Snapshot every cast member's current position as their mark");
const std::string TOOLTIP_ADD_CAST("Add cast members first");
const std::string TOOLTIP_RESET_MARKS("Snap every marked cast member back to their mark (local only)");
const std::string TOOLTIP_SET_MARKS_FIRST("Set marks first");
const std::string TOOLTIP_SCENE_SAVE("Save the whole setup as a scene: cast, marks, loco anims, subjects, arming, camera and move parameters. Opens an inline name box; re-saving the selected scene keeps its name");
const std::string TOOLTIP_SCENE_WRITE("Write the scene file (Enter in the name box saves too; an empty name cancels)");
const std::string TOOLTIP_SCENE_CHOOSE("Choose a scene to preview it; press Load to apply it");
const std::string TOOLTIP_SCENE_EMPTY("No saved scenes yet. Save one first");
const std::string TOOLTIP_SCENE_SELECT("Select a scene first");
const std::string TOOLTIP_SCENE_LOAD("Load the selected scene");
const std::string TOOLTIP_SCENE_LOAD_DIRTY("Load the selected scene; unsaved changes require confirmation");
const std::string TOOLTIP_SCENE_DELETE("Delete the selected scene file (asks to confirm)");
const std::string TOOLTIP_SCENE_NAMING("Finish naming first");
constexpr const char* COUNTDOWN_LABELS[] = {
    "0\xE2\x80\xA6", "1\xE2\x80\xA6", "2\xE2\x80\xA6", "3\xE2\x80\xA6",
    "4\xE2\x80\xA6", "5\xE2\x80\xA6", "6\xE2\x80\xA6", "7\xE2\x80\xA6",
    "8\xE2\x80\xA6", "9\xE2\x80\xA6", "10\xE2\x80\xA6"
};

// the assign combo's explicit "ungroup" row: discoverable equivalent of
// committing an empty name (which still works)
constexpr char GROUP_NONE_LABEL[] = "(none)";

enum SubjectMark : S32
{
    SUBJECT_A,
    SUBJECT_B,
    SUBJECT_C,
    SUBJECT_D,
};

LLUUID subject_mark_id(const LLDirectorCast& cast, S32 subject)
{
    switch (subject)
    {
        case ALDirectorSwitcher::SUBJECT_A: return cast.getSubjectA();
        case ALDirectorSwitcher::SUBJECT_B: return cast.getSubjectB();
        case ALDirectorSwitcher::SUBJECT_C: return cast.getSubjectC();
        case ALDirectorSwitcher::SUBJECT_D: return cast.getSubjectD();
        default: return LLUUID::null;
    }
}

void clear_subject(LLDirectorCast& cast, S32 subject)
{
    switch (subject)
    {
        case SUBJECT_A: cast.setSubjectA(LLUUID::null); break;
        case SUBJECT_B: cast.setSubjectB(LLUUID::null); break;
        case SUBJECT_C: cast.setSubjectC(LLUUID::null); break;
        case SUBJECT_D: cast.setSubjectD(LLUUID::null); break;
        default: break;
    }
}

void assign_subject(LLDirectorCast& cast, S32 subject, const LLUUID& id)
{
    if (subject < SUBJECT_A || subject > SUBJECT_D)
    {
        return;
    }
    // One cast member carries at most one subject mark. With C/D empty this
    // produces exactly the existing A/B reassignment behavior.
    if (cast.getSubjectA() == id) cast.setSubjectA(LLUUID::null);
    if (cast.getSubjectB() == id) cast.setSubjectB(LLUUID::null);
    if (cast.getSubjectC() == id) cast.setSubjectC(LLUUID::null);
    if (cast.getSubjectD() == id) cast.setSubjectD(LLUUID::null);
    switch (subject)
    {
        case SUBJECT_A: cast.setSubjectA(id); break;
        case SUBJECT_B: cast.setSubjectB(id); break;
        case SUBJECT_C: cast.setSubjectC(id); break;
        case SUBJECT_D: cast.setSubjectD(id); break;
        default: break;
    }
}

// Stable per-group tint for cast rows: hash the name to a hue, keep it a
// readable pastel (low saturation, full value) so the text stays legible on
// the dark list while two groups a row apart still read as different colors.
// Same spirit as LLActorMover::actorPathColor, keyed by name instead of id.
LLColor4 group_tint(const std::string& name)
{
    U32 h = 2166136261u;            // FNV-1a over the name bytes
    for (unsigned char c : name)
    {
        h = (h ^ c) * 16777619u;
    }
    const F32 hue = (h % 360u) / 360.f;
    LLColor4 col;
    col.setHSL(hue, 0.55f, 0.78f);  // pastel: light, moderately saturated
    col.mV[VW] = 1.f;
    return col;
}
// playing-marker glyph for the Animate tab's list
constexpr char GLYPH_PLAYING[] = "\xE2\x96\xB6";    // BLACK RIGHT-POINTING TRIANGLE

// CinematicCamMode value -> display name (matches the mode combo labels)
const char* cinecam_mode_name(S32 mode)
{
    return LLCinematicCamera::modeName(mode);
}

const char* locomotion_role_label(S32 role)
{
    using Cast = LLDirectorCast;
    switch (role)
    {
        case Cast::LOCO_IDLE:          return "Idle";
        case Cast::LOCO_WALK_FORWARD:  return "Walk forward";
        case Cast::LOCO_RUN_FORWARD:   return "Run forward";
        case Cast::LOCO_TURN_LEFT:     return "Turn left";
        case Cast::LOCO_TURN_RIGHT:    return "Turn right";
        case Cast::LOCO_WALK_BACKWARD: return "Walk backward";
        case Cast::LOCO_STRAFE_LEFT:   return "Strafe left";
        case Cast::LOCO_STRAFE_RIGHT:  return "Strafe right";
        case Cast::LOCO_FLY:           return "Fly";
        case Cast::LOCO_HOVER:         return "Hover";
        case Cast::LOCO_TAKEOFF:       return "Takeoff";
        case Cast::LOCO_LAND:          return "Land";
        default:                       return "Unknown";
    }
}

F32 default_locomotion_speed(S32 role)
{
    using Cast = LLDirectorCast;
    switch (role)
    {
        case Cast::LOCO_RUN_FORWARD:
        case Cast::LOCO_FLY:
            return 6.f;
        case Cast::LOCO_WALK_FORWARD:
        case Cast::LOCO_WALK_BACKWARD:
        case Cast::LOCO_STRAFE_LEFT:
        case Cast::LOCO_STRAFE_RIGHT:
            return 3.f;
        default:
            return 1.f;
    }
}

} // anonymous namespace

namespace
{
// Kit preset providers for the two Look pages. The ids are the Lightbox combo values
// (floater_lightbox_settings.xml ri_preset_combo / tr_preset_combo, 63 Roto Ink and 15 Tron looks),
// and apply() calls the same ALRotoInk / ALTron entry point the Lightbox combo calls, so the kit
// and the Lightbox write identical values. Separator items (value -1) are not looks and are skipped.
const char* const kRotoLooks[][2] = {
    {"comic", "Comic Ink"},
    {"bold", "Bold Comic"},
    {"manga", "Manga Fine Line"},
    {"anime", "Anime Cel Line"},
    {"pencil", "Pencil Sketch"},
    {"charcoal", "Charcoal"},
    {"boil", "Rotoscope Boil"},
    {"storyboard", "Storyboard"},
    {"chalk", "Chalk on Dark"},
    {"neon", "Neon Line"},
    {"tron", "Tron Glow"},
    {"inkwash", "Ink Wash"},
    {"blueprint", "Blueprint"},
    {"stainedglass", "Stained Glass"},
    {"holoscan", "Hologram Scan"},
    {"chase", "Chase Light"},
    {"matchglow", "Light-Matched Glow"},
    {"wireshimmer", "Wire Shimmer"},
    {"woodcut", "Woodcut"},
    {"ballpoint", "Ballpoint Doodle"},
    {"ants", "Marching Ants"},
    {"dashblue", "Dashed Blueprint"},
    {"doubleline", "Double-Line Cartoon"},
    {"engraving", "Cross-hatch Engraving"},
    {"pointillist", "Dotted Pointillist"},
    {"candy", "Two-Tone Candy"},
    {"brush", "Breathing Brush"},
    {"speedlines", "Comic Speed Lines"},
    {"heartbeat", "Neon Heartbeat"},
    {"writeon", "Ink Write-On"},
    {"ripple", "Ripple Pulse"},
    {"shockwave", "Shockwave"},
    {"painton", "Paint-On Bottom-Up"},
    {"radial", "Radial Reveal"},
    {"tempo120", "Tempo Pulse 120"},
    {"electric", "Electric Arc"},
    {"glitch", "Glitch Line"},
    {"strobe", "Strobe Poster"},
    {"neonbuzz", "Neon Tube Buzz"},
    {"barber", "Barber Pole"},
    {"lightning", "Lightning Storm"},
    {"oldfilm", "Old Film Jitter"},
    {"pixel8", "8-bit Pixel"},
    {"morse", "Morse Code"},
    {"dissolve", "Dissolve Sketch"},
    {"fire", "Fire Lick"},
    {"smoke", "Smoke Wisps"},
    {"drip", "Ink Drip"},
    {"heatshimmer", "Heat Shimmer"},
    {"jelly", "Jelly Wobble"},
    {"splatter", "Splatter Pop"},
    {"sparkle", "Sparkle Trail"},
    {"fireflies", "Orbit Fireflies"},
    {"constellation", "Constellation"},
    {"rainbow", "Rainbow Cycle"},
    {"holoflicker", "Hologram Flicker"},
    {"villain", "Villain Aura"},
    {"waverunner", "Wave Runner"},
    {"tron_legacy", "Tron Legacy Ink"},
    {"tron_ares", "Tron Ares Ink"},
    {"tron_1982", "Classic 1982 Ink"},
    {"tron_recognizer", "Recognizer Ink"},
    {"tron_uprising", "Uprising White Ink"},
};

const char* const kTronLooks[][2] = {
    {"legacy", "Tron Legacy Cyan"},
    {"ares", "Tron Ares Red"},
    {"classic82", "Classic 1982"},
    {"recognizer", "Recognizer Orange"},
    {"uprising", "Uprising White"},
    {"rinzler", "Rinzler"},
    {"siren", "Siren White"},
    {"quorra", "Quorra"},
    {"clugold", "Clu Gold"},
    {"seaofsim", "Sea of Simulation"},
    {"circuit", "Circuit City"},
    {"arena", "Disc Arena"},
    {"gridonly", "Grid Only (no grade)"},
    {"neonsuit", "Neon Suit Only"},
    {"lightcycle", "Light-Cycle Run"},
};

template <size_t N>
std::vector<ALKitPresetProvider::Entry> looksFrom(const char* const (&table)[N][2])
{
    std::vector<ALKitPresetProvider::Entry> out;
    out.reserve(N);
    for (size_t i = 0; i < N; ++i)
    {
        out.push_back({ table[i][0], table[i][1], true });
    }
    return out;
}

void applyRotoLook(const std::string& id) { ALRotoInk::applyPreset(id); }
void applyTronLook(const std::string& id) { ALTron::applyPreset(id); }

class ALLookPresetProvider final : public ALKitPresetProvider
{
public:
    ALLookPresetProvider(std::string key, std::vector<Entry> looks, void (*apply)(const std::string&),
                         std::vector<std::string> tracked) :
        mKey(std::move(key)),
        mLooks(std::move(looks)),
        mApply(apply),
        mTracked(std::move(tracked))
    {}

    const std::string& key() const override { return mKey; }

    void list(std::vector<Entry>& out) const override
    {
        out.insert(out.end(), mLooks.begin(), mLooks.end());
    }

    bool apply(const std::string& id) override
    {
        for (const Entry& look : mLooks)
        {
            if (look.id == id)
            {
                mApply(id);
                return true;
            }
        }
        return false;
    }

    U64 revision() const override { return 0; }

    const std::vector<std::string>& trackedSettings() const override { return mTracked; }

private:
    std::string                mKey;
    std::vector<Entry>         mLooks;
    void                       (*mApply)(const std::string&);
    std::vector<std::string>   mTracked;
};

class ALWeatherPresetProvider final : public ALKitPresetProvider
{
public:
    const std::string& key() const override { return mKey; }
    void list(std::vector<Entry>& out) const override
    {
        for (const std::string& name : ALPanelWeatherSettings::presetNames())
        {
            out.push_back({ "u:" + name, name, false });
        }
    }
    bool apply(const std::string& id) override
    {
        return id.compare(0, 2, "u:") == 0 && ALPanelWeatherSettings::applyNamedPreset(id.substr(2));
    }
    bool canSaveUser() const override { return true; }
    bool saveUser(const std::string& name) override { return ALPanelWeatherSettings::saveNamedPreset(name); }
    bool canDelete(const std::string& id) const override { return id.compare(0, 2, "u:") == 0; }
    bool deleteUser(const std::string& id) override
    {
        return canDelete(id) && ALPanelWeatherSettings::deleteNamedPreset(id.substr(2));
    }
    U64 revision() const override { return 0; }
    const std::vector<std::string>& trackedSettings() const override
    {
        return ALPanelWeatherSettings::trackedSettings();
    }

private:
    std::string mKey = "weather";
};

class ALProjectorShaftPresetProvider final : public ALKitPresetProvider
{
public:
    ALProjectorShaftPresetProvider() : mStore("projector_shafts", ALProjectorShaftPresets::trackedSettings()) {}
    const std::string& key() const override { return mKey; }
    void list(std::vector<Entry>& out) const override
    {
        static const char* const labels[] = {
            "Fast Preview", "Balanced", "Cinematic", "Hazy Stage",
            "Clear Air", "Searchlight", "Dust Storm", "Dream Mist"
        };
        for (S32 preset = ALProjectorShaftPresets::FAST_PREVIEW;
             preset <= ALProjectorShaftPresets::DREAM_MIST; ++preset)
        {
            out.push_back({ llformat("%d", preset), labels[preset], true });
        }
        for (const std::string& name : mStore.names())
        {
            out.push_back({ "u:" + name, name, false });
        }
    }
    bool apply(const std::string& id) override
    {
        if (id.size() == 1 && id[0] >= '0' && id[0] <= '7')
        {
            ALProjectorShaftPresets::apply(id[0] - '0');
            return true;
        }
        return id.compare(0, 2, "u:") == 0 && mStore.load(id.substr(2));
    }
    bool canSaveUser() const override { return true; }
    bool saveUser(const std::string& name) override { return mStore.save(name); }
    bool canDelete(const std::string& id) const override { return id.compare(0, 2, "u:") == 0; }
    bool deleteUser(const std::string& id) override { return canDelete(id) && mStore.remove(id.substr(2)); }
    U64 revision() const override { return mStore.revision(); }
    const std::vector<std::string>& trackedSettings() const override
    {
        return ALProjectorShaftPresets::trackedSettings();
    }

private:
    std::string                mKey = "projector_shafts";
    mutable ALKitSnapshotStore mStore;
};

void applySteppedConfig(const ALSteppedAnim::Config& c)
{
    gSavedSettings.setS32("StepAnimScope", c.scope);
    gSavedSettings.setS32("StepAnimMode", c.mode);
    gSavedSettings.setF32("StepAnimRate", c.rate);
    gSavedSettings.setF32("StepAnimFastRate", c.fastRate);
    gSavedSettings.setF32("StepAnimAdaptThreshold", c.adaptThreshold);
    gSavedSettings.setF32("StepAnimAdaptHysteresis", c.adaptHysteresis);
    gSavedSettings.setF32("StepAnimAdaptMinDwell", c.adaptMinDwell);
    gSavedSettings.setF32("StepAnimBackgroundRate", c.backgroundRate);
    gSavedSettings.setS32("StepAnimWhatSteps", c.what);
    gSavedSettings.setBOOL("StepAnimFace", c.face);
    gSavedSettings.setBOOL("StepAnimGaze", c.gaze);
    gSavedSettings.setBOOL("StepAnimFlexi", c.flexi);
    gSavedSettings.setBOOL("StepAnimAttachFX", c.attachFX);
    gSavedSettings.setS32("StepAnimClock", c.clock);
    gSavedSettings.setS32("StepAnimOffsetMode", c.offset);
    gSavedSettings.setF32("StepAnimPhase", c.phase);
    gSavedSettings.setS32("StepAnimBaseFPS", c.baseFps);
    gSavedSettings.setBOOL("StepAnimBoil", c.boil);
    gSavedSettings.setF32("StepAnimBoilAmount", c.boilAmount);
    gSavedSettings.setS32("StepAnimBoilTicks", c.boilTicks);
    gSavedSettings.setBOOL("StepAnimImpactHold", c.impact);
    gSavedSettings.setS32("StepAnimImpactTicks", c.impactTicks);
    gSavedSettings.setF32("StepAnimImpactThreshold", c.impactThreshold);
    gSavedSettings.setBOOL("StepAnimSmear", c.smear);
    gSavedSettings.setF32("StepAnimSmearStrength", c.smearStrength);
    gSavedSettings.setF32("StepAnimSmearThreshold", c.smearThreshold);
    gSavedSettings.setBOOL("StepAnimEase", c.ease);
    gSavedSettings.setF32("StepAnimEaseAmount", c.easeAmount);
    gSavedSettings.setBOOL("StepAnimMatchRotoInk", c.matchRoto);
}

class ALSteppedPresetProvider final : public ALKitPresetProvider
{
public:
    ALSteppedPresetProvider() : mStore("stepped_anim", ALSteppedAnim::presetSettings()) {}
    const std::string& key() const override { return mKey; }
    void list(std::vector<Entry>& out) const override
    {
        for (const ALSteppedAnim::Preset& preset : ALSteppedAnim::builtInPresets())
            out.push_back({ "b:" + std::string(preset.id), preset.label, true });
        for (const std::string& name : mStore.names())
            out.push_back({ "u:" + name, name, false });
    }
    bool apply(const std::string& id) override
    {
        if (id.compare(0, 2, "b:") == 0)
        {
            for (const ALSteppedAnim::Preset& preset : ALSteppedAnim::builtInPresets())
            {
                if (id.substr(2) == preset.id)
                {
                    applySteppedConfig(preset.values);
                    gSavedSettings.setBOOL("StepAnimEnabled", true);
                    return true;
                }
            }
            return false;
        }
        return id.compare(0, 2, "u:") == 0 && mStore.load(id.substr(2));
    }
    bool canSaveUser() const override { return true; }
    bool saveUser(const std::string& name) override { return mStore.save(name); }
    bool canDelete(const std::string& id) const override { return id.compare(0, 2, "u:") == 0; }
    bool deleteUser(const std::string& id) override { return canDelete(id) && mStore.remove(id.substr(2)); }
    U64 revision() const override { return mStore.revision(); }
    // Preset-owned only: StepAnimParticles / StepAnimTextureAnim are independent of presets.
    const std::vector<std::string>& trackedSettings() const override { return ALSteppedAnim::presetSettings(); }

private:
    std::string                mKey = "stepped_anim";
    mutable ALKitSnapshotStore mStore;
};

void registerLookPresetProviders()
{
    static bool registered = false;
    if (registered)
    {
        return;
    }
    registered = true;
    ALKitPresetRegistry::instance().add(std::make_unique<ALLookPresetProvider>(
        "roto_ink", looksFrom(kRotoLooks), applyRotoLook, std::vector<std::string>(ALRotoInk::settings())));
    ALKitPresetRegistry::instance().add(std::make_unique<ALLookPresetProvider>(
        "tron_look", looksFrom(kTronLooks), applyTronLook, std::vector<std::string>(ALTron::lookSettings())));
    ALKitPresetRegistry::instance().add(std::make_unique<ALSteppedPresetProvider>());
    ALKitPresetRegistry::instance().add(std::make_unique<ALWeatherPresetProvider>());
    ALKitPresetRegistry::instance().add(std::make_unique<ALProjectorShaftPresetProvider>());
}
} // anonymous namespace

LLFloaterDirector::LLFloaterDirector(const LLSD& key)
:   LLFloater(key)
{
    registerLookPresetProviders();
    ALTron::initializeSettingSync();
    mBuildTimer.start();
}

LLFloaterDirector::~LLFloaterDirector()
{
    if (sRecoveryDirectorHandle.get() == this)
    {
        sRecoveryDirectorHandle.markDead();
    }
    // the embedded ALPanelAnimPreview owns and releases its own dummy

    // AnimationExplorer idiom: menus were parented to gMenuHolder, not us
    if (LLContextMenu* menu = mCastMenuHandle.get())
    {
        menu->die();
        mCastMenuHandle.markDead();
    }
    if (LLContextMenu* menu = mAnimMenuHandle.get())
    {
        menu->die();
        mAnimMenuHandle.markDead();
    }
}

void LLFloaterDirector::onClose(bool app_quitting)
{
    if (mShell)
        mShell->setSearchPopupArmed(nullptr, false);
    if(mShell)if(ALKitPage* page=mShell->currentKitPage())page->saveState();
    // Closing can merely hide this reusable floater, so its child panel may
    // remain alive. Explicitly release a live bullet-time cut here as well.
    if (!app_quitting)
    {
        ALDirectorSwitcher::instance().cancelEaseWorldTime();
        if (ALAvatarLightResponse::instanceExists())
            ALAvatarLightResponse::instance().flushIfDirty();
    }
}

void LLFloaterDirector::onFocusLost()
{
    if (mShell)
        mShell->setSearchPopupArmed(nullptr, false);
    LLFloater::onFocusLost();
}

void LLFloaterDirector::setMinimized(bool minimized)
{
    if (minimized && mShell)
        mShell->setSearchPopupArmed(nullptr, false);
    LLFloater::setMinimized(minimized);
}

bool LLFloaterDirector::postBuild()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director postBuild");

    mShell=findChild<ALKitShell>("director_shell");
    mIsV2=mShell!=nullptr;
    if(mIsV2)
    {
        return postBuildV2();
    }

    // ---- transport ----
    mActionBtn = getChild<LLButton>("btn_action");
    mCutBtn = getChild<LLButton>("btn_cut");
    mSetMarksBtn = getChild<LLButton>("btn_set_marks");
    mResetMarksBtn = getChild<LLButton>("btn_reset_marks");
    mActionBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAction(); });
    mCutBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAction(); });
    mSetMarksBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetMarks(); });
    mResetMarksBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickResetMarks(); });
    // Enter anywhere sensible fires ACTION (the default button); while it is
    // hidden (running/counting) Enter does nothing -- Esc is CUT (handleKeyHere)
    setDefaultBtn(mActionBtn);

    // ---- scene files (transport bar) ----
    mSceneCombo = getChild<LLComboBox>("scene_combo");
    mSceneNameEditor = getChild<LLLineEditor>("scene_name_editor");
    mSceneSaveBtn = getChild<LLButton>("btn_scene_save");
    mSceneDeleteBtn = getChild<LLButton>("btn_scene_delete");
    // v1 floater_director.xml has no Load button (v2 only); keep the lookup null-safe
    mSceneLoadBtn = findChild<LLButton>("btn_scene_load");
    mSceneDirtyText = findChild<LLTextBox>("scene_dirty_indicator");
    if (mSceneLoadBtn)
        mSceneLoadBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSceneLoad(); });
    mSceneCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSceneSelected(); });
    mSceneNameEditor->setCommitCallback([this](LLUICtrl*, const LLSD&) { commitSceneName(); });
    mSceneSaveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSceneSave(); });
    mSceneDeleteBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSceneDelete(); });
    refreshSceneList();

    // ---- left-rail tabs (remember the active tab, icon each rail button) ----
    mTabContainer = getChild<LLTabContainer>("director_tabs");
    mTabContainer->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTabChanged(); });
    struct GroupInfo { const char* page; const char* tabs; const char* icon; } groups[] = {
        { "cast_group",        "cast_tabs",        "Command_People_Icon" },
        { "performance_group", "performance_tabs", "Command_Poser_Icon" },
        { "camera_group",      "camera_tabs",      "Command_View_Icon" },
        { "lens_group",        "lens_tabs",        "Command_Search_Icon" },
        { "light_group",       "light_tabs",       "Command_PersonalLighting_Icon" },
        { "look_group",        "look_tabs",        "Command_Lightbox_Icon" },
        { "world_group",       "world_tabs",       "Command_Environments_Icon" },
        { "capture_group",     "capture_tabs",     "Command_Snapshot_Icon" },
    };
    for (const GroupInfo& info : groups)
    {
        LLPanel* page = mTabContainer->getPanelByName(info.page);
        LLTabContainer* tabs = page ? page->findChild<LLTabContainer>(info.tabs, false) : nullptr;
        if (page && tabs)
        {
            mGroups.emplace_back(page, tabs);
            mTabContainer->setTabImage(page, info.icon);
            tabs->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTabChanged(); });
        }
    }
    // [DirFix] D14: LLFloater::setMinimized() never toggles child visibility, so
    // the Path panel's own reveal claim cannot see an un-minimize; if the other
    // host claimed while this console was minimized, restoring it re-feeds +
    // re-claims here (revealFor stores only while another tab is current).
    setMinimizeCallback([this](LLUICtrl*, const LLSD& minimized)
    {
        if (!minimized.asBoolean() && mPathPanel) retargetPathFromCastSelection();
    });
    // ---- legend popover ----
    mHelpBtn = getChild<LLButton>("btn_help");
    mLegendPanel = getChild<LLPanel>("legend_panel");
    mHelpBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onToggleLegend(); });
    if(!mIsV2)
    {
        mLegendPanel->getChild<LLButton>("btn_legend_close")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { mLegendPanel->setVisible(false); });
    }

    // ---- cast column ----
    mCastList = getChild<LLScrollListCtrl>("cast_list");
    mCastHint = getChild<LLTextBox>("cast_hint");
    mRemoveBtn = getChild<LLButton>("btn_remove");
    mFocusBtn = getChild<LLButton>("btn_focus");
    // [DirFix] D7: columns are XML-declared; resolve their indices once (same unchecked deref as before)
    mCastStatusCol = mCastList->getColumn("status")->mIndex;
    mCastNameCol = mCastList->getColumn("name")->mIndex;
    mCastABCol = mCastList->getColumn("ab")->mIndex;
    mCastMarkCol = mCastList->getColumn("mark")->mIndex;
    mCastLookCol = mCastList->getColumn("look")->mIndex;
    getChild<LLButton>("btn_add_you")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickAddYou(); });
    getChild<LLButton>("btn_add_animesh")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickAddAnimesh(); });
    mRemoveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCastRemove(); });
    mFocusBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickFocusActor(); });
    mCastList->setRightMouseDownCallback(
        [this](LLUICtrl* ctrl, S32 x, S32 y, MASK) { onCastRightClick(ctrl, x, y); });
    // double-click a cast row = Set Subject A (mirrors the right-click path)
    mCastList->setDoubleClickCallback([this]() { onCastSetSubject(SUBJECT_A); });
    // [DirFix] D14: a cast selection is a user event; feed + claim the Path host
    // synchronously (claims only while the Path tab is visible). draw()'s
    // refreshPathTab stays ownership-neutral (setTargetActor).
    mCastList->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        if (mPathPanel) retargetPathFromCastSelection();
        if(mShell)mShell->setCastSelection(selectedCastIds());
    });
    {
        // same LLContextMenu idiom as the Animation Explorer's list menu
        LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
        // [AutoAnimate] Keep enable registration alive through menu construction.
        LLUICtrl::EnableCallbackRegistry::ScopedRegistrar autoanim_enables;
        autoanim_enables.add("Director.CastHasGhostEntity", [this](LLUICtrl*, const LLSD&) { return castHasGhostEntity(); });
        registrar.add("Director.AutoAnimToggle", [this](LLUICtrl*, const LLSD&) { onCastAutoAnim(0); });
        registrar.add("Director.AutoAnimGo", [this](LLUICtrl*, const LLSD&) { onCastAutoAnim(1); });
        registrar.add("Director.AutoAnimFollow", [this](LLUICtrl*, const LLSD&) { onCastAutoAnim(2); });
        registrar.add("Director.SetSubjectA", [this](LLUICtrl*, const LLSD&) { onCastSetSubject(SUBJECT_A); });
        registrar.add("Director.SetSubjectB", [this](LLUICtrl*, const LLSD&) { onCastSetSubject(SUBJECT_B); });
        registrar.add("Director.SetSubjectC", [this](LLUICtrl*, const LLSD&) { onCastSetSubject(SUBJECT_C); });
        registrar.add("Director.SetSubjectD", [this](LLUICtrl*, const LLSD&) { onCastSetSubject(SUBJECT_D); });
        registrar.add("Director.SetMarkHere", [this](LLUICtrl*, const LLSD&) { onCastSetMarkHere(); });
        registrar.add("Director.ResetToMark", [this](LLUICtrl*, const LLSD&) { onCastResetToMark(); });
        registrar.add("Director.ClearLocoAnim", [this](LLUICtrl*, const LLSD&) { onCastClearLocoAnim(); });
        registrar.add("Director.CopyUUID", [this](LLUICtrl*, const LLSD&) { onCastCopyUUID(); });
        // [AvatarLightResponse] cast row > Light Response...: null cast id means You
        registrar.add("Director.LightResponse", [this](LLUICtrl*, const LLSD&)
        {
            LLUUID id = firstSelectedCastId();
            if (id.isNull())
            {
                id = gAgentID;
            }
            LLFloaterReg::showInstance("avatar_light_response", LLSD(id));
        });
        registrar.add("Director.RemoveFromCast", [this](LLUICtrl*, const LLSD&) { onCastRemove(); });
        // Animate tab's list menu shares the registrar scope
        registrar.add("Director.AnimCopyUUID", [this](LLUICtrl*, const LLSD&) { onAnimCopyUUID(); });
        registrar.add("Director.AnimSetLoco", [this](LLUICtrl*, const LLSD&) { onAnimSetLoco(); });
        registrar.add("Director.AnimPlayLocal", [this](LLUICtrl*, const LLSD&) { onAnimPlayLocal(true); });
        registrar.add("Director.AnimStopLocal", [this](LLUICtrl*, const LLSD&) { onAnimPlayLocal(false); });
        if (LLContextMenu* menu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>(
                "menu_director_cast.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance()))
        {
            mCastMenuHandle = menu->getHandle();
        }
        if (LLContextMenu* menu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>(
                "menu_director_anims.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance()))
        {
            mAnimMenuHandle = menu->getHandle();
        }
    }

    // ---- groups ----
    // assign combo (cast column): tags every selected member; free-typed names
    // create groups, picking an existing name reuses it, an empty name ungroups.
    // The embedded editor commits on Enter / item pick only (never focus-lost),
    // so a half-typed name can never tag a freshly clicked row by accident.
    mGroupAssignCombo = getChild<LLComboBox>("group_assign_combo");
    mGroupAssignCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitGroupAssign(); });
    // group transport (Move tab): pick a group, Start/Stop every member in it
    mGroupRunCombo = getChild<LLComboBox>("group_run_combo");
    mGroupStartBtn = getChild<LLButton>("btn_group_start");
    mGroupStopBtn = getChild<LLButton>("btn_group_stop");
    mGroupStartBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroup(true); });
    mGroupStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroup(false); });
    // Groups management section (Move tab): list every group with member count
    // + start delay; select a row to highlight its members in the cast list
    // and edit its delay / name. Dissolve only untags -- nobody leaves the cast.
    mGroupsList = getChild<LLScrollListCtrl>("groups_list");
    mGroupDelaySpinner = getChild<LLSpinCtrl>("group_delay_spinner");
    mGroupRenameEditor = getChild<LLLineEditor>("group_rename_editor");
    mGroupRenameBtn = getChild<LLButton>("btn_group_rename");
    mGroupDissolveBtn = getChild<LLButton>("btn_group_dissolve");
    mGroupsList->setCommitCallback([this](LLUICtrl*, const LLSD&) { onGroupsListSelect(); });
    mGroupDelaySpinner->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitGroupDelay(); });
    mGroupRenameEditor->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroupRename(); });
    mGroupRenameBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroupRename(); });
    mGroupDissolveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroupDissolve(); });

    // ---- Move tab ----
    // the transport (scope, heading dial, params, Walk/Stop) is the shared
    // ALPanelActorMover the standalone Actor Mover floater also embeds; the
    // console feeds it the cast selection each draw (refreshMoveTab)
    mMoverPanel = findChild<ALPanelActorMover>("actor_mover_panel");

    // ---- Path tab ----
    // the waypoint editor lives in its own tab now; always visible, its target
    // actor re-pointed each draw to the console's cast selection
    mPathPanel = findChild<ALPanelPathEditor>("path_editor");

    // ---- Actor FX tab ----
    // This is a view over LLDirectorCast's per-actor model. It deliberately
    // does not share Ghost Studio's selected instance: actor styles attach to
    // stable cast ids, with a separate null-id record for You.
    mActorStyleTarget = getChild<LLComboBox>("actor_style_target");
    mActorStyleEnabled = getChild<LLCheckBoxCtrl>("actor_style_enabled");
    mActorStyleMode = getChild<LLComboBox>("actor_style_mode");
    mActorStyleLook = getChild<LLComboBox>("actor_style_look");
    mActorStyleUseActorHue = getChild<LLCheckBoxCtrl>("actor_style_use_actor_hue");
    mActorStyleHue = getChild<LLSliderCtrl>("actor_style_hue");
    mActorStyleAlpha = getChild<LLSliderCtrl>("actor_style_alpha");
    mActorStyleDissolveProgress = getChild<LLSliderCtrl>("actor_style_dissolve_progress");
    mActorStylePixel = getChild<LLSliderCtrl>("actor_style_pixel");
    mActorStyleShimmerSpeed = getChild<LLSliderCtrl>("actor_style_shimmer_speed");
    mActorStyleShimmerAmount = getChild<LLSliderCtrl>("actor_style_shimmer_amount");
    mActorStyleGlitch = getChild<LLSliderCtrl>("actor_style_glitch");
    mActorStyleDistortion = getChild<LLComboBox>("actor_style_distortion");
    mActorStyleDistortionAmount = getChild<LLSliderCtrl>("actor_style_distortion_amount");
    mActorStyleBrightness = getChild<LLSliderCtrl>("actor_style_brightness");
    mActorStyleEffectFps = getChild<LLSpinCtrl>("actor_style_effect_fps");
    mActorStyleReset = getChild<LLButton>("actor_style_reset");
    mActorStyleStatus = getChild<LLTextBox>("actor_style_status");

    mActorStyleTarget->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onActorStyleTargetChanged(); });
    for (LLUICtrl* control : {
            static_cast<LLUICtrl*>(mActorStyleEnabled),
            static_cast<LLUICtrl*>(mActorStyleMode),
            static_cast<LLUICtrl*>(mActorStyleLook),
            static_cast<LLUICtrl*>(mActorStyleUseActorHue),
            static_cast<LLUICtrl*>(mActorStyleHue),
            static_cast<LLUICtrl*>(mActorStyleAlpha),
            static_cast<LLUICtrl*>(mActorStyleDissolveProgress),
            static_cast<LLUICtrl*>(mActorStylePixel),
            static_cast<LLUICtrl*>(mActorStyleShimmerSpeed),
            static_cast<LLUICtrl*>(mActorStyleShimmerAmount),
            static_cast<LLUICtrl*>(mActorStyleGlitch),
            static_cast<LLUICtrl*>(mActorStyleDistortion),
            static_cast<LLUICtrl*>(mActorStyleDistortionAmount),
            static_cast<LLUICtrl*>(mActorStyleBrightness),
            static_cast<LLUICtrl*>(mActorStyleEffectFps) })
    {
        control->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onActorStyleControlChanged(); });
    }
    mActorStyleReset->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onActorStyleReset(); });

    // ---- Animate tab ----
    mAnimateHeader = getChild<LLTextBox>("animate_header");
    mAnimList = getChild<LLScrollListCtrl>("anim_list");
    mAnimHint = getChild<LLTextBox>("anim_hint");
    // [DirFix] D7: XML-declared columns, resolved once
    mAnimPrioCol = mAnimList->getColumn("prio")->mIndex;
    mAnimPlayingCol = mAnimList->getColumn("playing")->mIndex;
    mAnimCopyBtn = getChild<LLButton>("btn_anim_copy");
    mAnimSetLocoBtn = getChild<LLButton>("btn_anim_setloco");
    mAnimPlayBtn = getChild<LLButton>("btn_anim_play");
    mAnimStopBtn = getChild<LLButton>("btn_anim_stop");
    mPasteEditor = getChild<LLLineEditor>("paste_uuid_editor");
    mPastePlayBtn = getChild<LLButton>("btn_paste_play");
    mPasteStopBtn = getChild<LLButton>("btn_paste_stop");
    mPasteSetLocoBtn = getChild<LLButton>("btn_paste_setloco");
    mLocomotionRoleCombo = getChild<LLComboBox>("locomotion_role_combo");
    mLocomotionSpeed = getChild<LLSpinCtrl>("locomotion_speed");
    mLocomotionPhase = getChild<LLSpinCtrl>("locomotion_phase");
    mLocomotionAllowAO = getChild<LLCheckBoxCtrl>("locomotion_allow_ao");
    mLocomotionRoleCombo->setValue(LLDirectorCast::LOCO_WALK_FORWARD);
    mLocoText = getChild<LLTextBox>("loco_text");
    mClearLocoBtn = getChild<LLButton>("btn_clear_loco");
    mAnimCopyBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAnimCopyUUID(); });
    mAnimSetLocoBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAnimSetLoco(); });
    mAnimPlayBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAnimPlayLocal(true); });
    mAnimStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAnimPlayLocal(false); });
    mPastePlayBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPastePlayLocal(true); });
    mPasteStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPastePlayLocal(false); });
    mPasteSetLocoBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPasteSetLoco(); });
    mClearLocoBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClearLoco(); });
    mLocomotionRoleCombo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onLocomotionRoleChanged(); });
    for (LLUICtrl* control : { static_cast<LLUICtrl*>(mLocomotionSpeed),
                               static_cast<LLUICtrl*>(mLocomotionPhase),
                               static_cast<LLUICtrl*>(mLocomotionAllowAO) })
        control->setCommitCallback([this](LLUICtrl*, const LLSD&) { onLocomotionMetadataCommit(); });
    mAnimList->setRightMouseDownCallback(
        [this](LLUICtrl* ctrl, S32 x, S32 y, MASK) { onAnimRightClick(ctrl, x, y); });
    getChild<LLButton>("btn_open_animexp")->setCommitCallback(
        [](LLUICtrl*, const LLSD&) { LLFloaterReg::showInstance("animation_explorer"); });

    // embedded shared preview pane + own-avatar controls (the same
    // ALPanelAnimPreview the standalone Animation Explorer floater embeds). It
    // owns its dummy, mouse-drag-to-rotate, auto-loop, and the Stop / Stop-and-
    // Revoke / Blacklist / Capture-all controls; refreshAnimateTab feeds it the
    // selected/pasted anim + its source object each draw.
    mAnimPreviewPanel = findChild<ALPanelAnimPreview>("anim_preview_panel");

    // [DirFix] D1: the four explicit ALScrollFocus installs (Move / Animate / Actor FX /
    // Shafts) moved into mScrollFit.installForTabs() at the end of postBuild, which covers
    // every tab-level container.

    // ---- Camera tab ----
    mSubjectAText = getChild<LLTextBox>("subject_a_text");
    mSubjectBText = getChild<LLTextBox>("subject_b_text");
    mSubjectCText = getChild<LLTextBox>("subject_c_text");
    mSubjectDText = getChild<LLTextBox>("subject_d_text");
    mSetABtn = getChild<LLButton>("btn_set_a");
    mSetBBtn = getChild<LLButton>("btn_set_b");
    mSetCBtn = getChild<LLButton>("btn_set_c");
    mSetDBtn = getChild<LLButton>("btn_set_d");
    mClearABtn = getChild<LLButton>("btn_clear_a");
    mClearBBtn = getChild<LLButton>("btn_clear_b");
    mClearCBtn = getChild<LLButton>("btn_clear_c");
    mClearDBtn = getChild<LLButton>("btn_clear_d");
    mSetABtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetSubjectFromSelection(SUBJECT_A); });
    mSetBBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetSubjectFromSelection(SUBJECT_B); });
    mSetCBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetSubjectFromSelection(SUBJECT_C); });
    mSetDBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetSubjectFromSelection(SUBJECT_D); });
    mClearABtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClearSubject(SUBJECT_A); });
    mClearBBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClearSubject(SUBJECT_B); });
    mClearCBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClearSubject(SUBJECT_C); });
    mClearDBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClearSubject(SUBJECT_D); });
    getChild<LLButton>("btn_actor_gaze")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onOpenActorGaze(); });
    mGazePanel = findChild<ALPanelLensGaze>("director_gaze_panel");
    getChild<LLButton>("dance_popout")->setCommitCallback(
        [](LLUICtrl*, const LLSD&) { LLFloaterReg::showInstance("dance_ball"); });
    getChild<LLButton>("polish_popout")->setCommitCallback(
        [](LLUICtrl*, const LLSD&) { LLFloaterReg::showInstance("pose_polish"); });
    getChild<LLButton>("exposure_popout")->setCommitCallback(
        [](LLUICtrl*, const LLSD&) { LLFloaterReg::showInstance("exposure_hub"); });
    getChild<LLButton>("light_response_popout")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            const LLUUID actor = firstSelectedCastId();
            LLFloaterReg::showInstance("avatar_light_response",
                LLSD(actor.notNull() ? actor : gAgentID));
        });
    mPrismSummaryText = getChild<LLTextBox>("prism_summary");
    mPrismManageBtn = getChild<LLButton>("btn_prism_manage");
    mOtsPairBtn = getChild<LLButton>("btn_build_ots_pair");
    mZollyBtn = getChild<LLButton>("btn_start_zolly");
    mHeroArcBtn = getChild<LLButton>("btn_start_hero_arc");
    mPrismManageBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickManagePrism(); });
    mOtsPairBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickBuildOtsPair(); });
    mZollyBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickZolly(); });
    mHeroArcBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickHeroArc(); });
    // embedded shared params panel: scene files read its selected preset and
    // apply presets through it on load
    mCineCamPanel = findChild<ALPanelCineCamParams>("cinecam_params_embedded");

    addLauncherCard("gaze_cues",
        [this]
        {
            LLUUID actor = firstSelectedCastId();
            if (actor.isNull()) actor = gAgentID;
            LLFloaterReg::showInstance("gaze_cues", LLSD(actor.asString()));
        },
        [this]
        {
            LLUUID actor = firstSelectedCastId();
            if (actor.isNull()) actor = gAgentID;
            const S32 count = (S32)LLDirectorCast::instance().getGazeCues(actor).size();
            return llformat("%s: %d gaze cue%s", castMemberName(actor).c_str(), count, count == 1 ? "" : "s");
        });
    addLauncherCard("diopter",
        [] { LLFloaterReg::showInstance("ultimate_diopter"); },
        []
        {
            static LLCachedControl<bool> enabled(gSavedSettings, "CineDiopterEnabled", false);
            return std::string("Diopter: ") + (enabled ? "on" : "off");
        });
    addLauncherCard("practicals",
        [] { ALFloaterLightBox::showTab("sellight_settings"); },
        []
        {
            LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();
            LLViewerObject* object = selection->getPrimaryObject();
            if (!object) object = selection->getFirstRootObject();
            LLVOVolume* volume = dynamic_cast<LLVOVolume*>(object);
            if (!volume) return std::string(object ? "Selection is not a light" : "Select a projector in world");
            if (!volume->getIsLight()) return std::string("Selection is not a light (or its light is off)");
            if (volume->isLightSpotlight()) return std::string("Projector selected");
            return std::string("Point light selected (needs a projector)");
        });
    addLauncherCard("actor_mask",
        [] { ALFloaterLightBox::showTab("actor_mask_settings"); },
        []
        {
            static LLCachedControl<bool> disabled(gSavedSettings, "CineLightRigActorMaskDisable", false);
            return std::string("Actor-only lights: ") + (disabled ? "disabled" : "active");
        });
    addLauncherCard("light_cues",
        [] { LLFloaterReg::showInstance("cine_light_cues"); },
        []
        {
            const auto& cues = ALCineLightRigManager::instance().selected().cueList();
            const S32 count = (S32)cues.mCues.size();
            if (cues.mName.empty()) return llformat("%d cue%s", count, count == 1 ? "" : "s");
            return llformat("%d cue%s \xC2\xB7 %s", count, count == 1 ? "" : "s", cues.mName.c_str());
        });
    addLauncherCard("haze", [] { ALFloaterLightBox::showTab("cinehaze_settings"); }, []
        { static LLCachedControl<bool> on(gSavedSettings, "CineHazeEnable", false); return std::string(on ? "On" : "Off"); });
    addLauncherCard("shadows_probes", [] { ALFloaterLightBox::showTab("shadow_settings"); }, []
        {
            static LLCachedControl<S32> detail(gSavedSettings, "RenderShadowDetail", 0);
            switch ((S32)detail) { case 1: return std::string("Sun/moon"); case 2: return std::string("Sun/moon + projectors"); default: return std::string("Off"); }
        });
    addLauncherCard("snapshot", [] { LLFloaterReg::showInstance("snapshot"); }, []
        { return std::string("Stills and high-resolution frames"); });

    // ---- Takes tab ----
    // the transport + controls are the shared ALPanelFlycamRecorder embedded in
    // the Takes tab (panel_flycam_recorder.xml); it owns its own wiring and
    // self-refreshes, so there is nothing to hook up here. Same panel the
    // standalone Flycam Recorder floater uses -> one singleton, no fork.

    // ---- status strip ----
    mStatusStrip = getChild<LLTextBox>("status_strip");
    mRecIndicator = getChild<LLTextBox>("rec_indicator");
    mKitUIMs=findChild<LLTextBox>("kit_ui_ms");
    if(mKitUIMs)mKitUIMs->setVisible(gSavedSettings.getBOOL("KitShowUIMs"));

    // [DirFix] D1: after every getChild and after the tab commit callback above (onTabChanged
    // and the helper's refit both stay on the commit signal; the refit is connected later,
    // neither reads what the other writes). Wraps the tabs that have no scroller (Ghosts,
    // Props, Takes, Time), installs focus-reveal on every tab-level container (incl. the
    // nested Ghost Studio sub-tabs), fits every document from its children (the hand-typed
    // XML heights are gone) and enables wheel adjust on every slider.
    mScrollFit.setUnwrappedPages({"cast_group", "performance_group", "camera_group", "lens_group",
        "light_group", "look_group", "world_group", "capture_group", "dance_tab", "exposure_tab", "ghosts_tab"});
    mScrollFit.installForTabs(mTabContainer, this);
    mDancePage = findChild<LLPanel>("dance_tab");
    mDancePanelView = findChild<LLView>("director_dance_ball_panel");
    mExposurePage = findChild<LLPanel>("exposure_tab");
    mExposurePanelView = findChild<LLView>("director_exposure_panel");
    mGhostPage = findChild<LLPanel>("ghosts_tab");
    mGhostPanelView = findChild<LLView>("ghost_studio_panel");
    ALScrollFit::fillPage(mDancePage, mDancePanelView, 30);
    ALScrollFit::fillPage(mExposurePage, mExposurePanelView, 30);
    ALScrollFit::fillPage(mGhostPage, mGhostPanelView, 8);

    const F32 build_ms = mBuildTimer.getElapsedTimeF32() * 1000.f;
    LL_INFOS("Director") << "Director build " << build_ms << " ms "
                         << (build_ms < 250.f ? "PASS" : "LAZY-NEEDED") << LL_ENDL;

    initializeSceneRecovery();
    return true;
}

bool LLFloaterDirector::postBuildV2()
{
    bindChrome(this);
    LLHandle<LLFloaterDirector> handle = getDerivedHandle<LLFloaterDirector>();
    mPageBuiltConnection = mShell->setPageBuiltCallback(
        [handle](const std::string& page, LLPanel* panel)
        {
            if (LLFloaterDirector* director = handle.get())
            {
                director->bindPage(page, panel);
            }
        });
    mPageSelectedConnection = mShell->setPageSelectedCallback(
        [handle](const std::string& page)
        {
            LLFloaterDirector* director = handle.get();
            if (!director)
            {
                return;
            }
            if (page == "look_group/image_tab")
            {
                ALColorGrade::populateLUTCombo(director->findChild<LLComboBox>("colorlut_combo"));
            }
            director->refreshWorldLensReadouts(page);
        });
    mScrollFit.setUnwrappedPages(mShell->unwrappedPages());
    mScrollFit.installForTabs(mTabContainer, this);
    mShell->setScrollFit(&mScrollFit);
    mShell->setReady();
    initializeSceneRecovery();
    const F32 build_ms = mBuildTimer.getElapsedTimeF32() * 1000.f;
    LL_INFOS("Director") << "Director v2 chrome build " << build_ms << " ms" << LL_ENDL;
    return true;
}

void LLFloaterDirector::bindChrome(LLView* root)
{
    // Keep this wiring in lockstep with the v1 postBuild block above: v2 calls
    // the same callbacks against lazily re-homed controls, while v1 retains
    // its original statement order for parity.
    mActionBtn = root->getChild<LLButton>("btn_action");
    mCutBtn = root->getChild<LLButton>("btn_cut");
    mArmBtn = root->getChild<LLButton>("btn_arm");
    mTakePlayBtn = root->getChild<LLButton>("btn_take_play");
    mTakeStopBtn = root->getChild<LLButton>("btn_take_stop");
    mTakeTimecode = root->getChild<LLTextBox>("take_timecode");
    mCastChip = root->getChild<LLButton>("btn_cast_chip");
    mTransportToast = root->getChild<LLTextBox>("transport_toast");
    mTransportBar = dynamic_cast<ALKitBar*>(root->getChild<LLView>("kit_transport"));
    mSetMarksBtn = root->getChild<LLButton>("btn_set_marks");
    mResetMarksBtn = root->getChild<LLButton>("btn_reset_marks");
    mActionBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAction(); });
    mCutBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAction(); });
    mTakePlayBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickTakePlay(); });
    mTakeStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickTakeStop(); });
    mSetMarksBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetMarks(); });
    mResetMarksBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickResetMarks(); });
    setDefaultBtn(mActionBtn);

    mSceneCombo = root->getChild<LLComboBox>("scene_combo");
    mSceneNameEditor = root->getChild<LLLineEditor>("scene_name_editor");
    mSceneLoadBtn = root->getChild<LLButton>("btn_scene_load");
    mSceneDirtyText = root->findChild<LLTextBox>("scene_dirty_indicator");
    mSceneMetadataText = root->findChild<LLTextBox>("scene_metadata_text");
    mSceneLoadBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSceneLoad(); });
    mSceneSaveBtn = root->getChild<LLButton>("btn_scene_save");
    mSceneDeleteBtn = root->getChild<LLButton>("btn_scene_delete");
    mSceneCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSceneSelected(); });
    mSceneNameEditor->setCommitCallback([this](LLUICtrl*, const LLSD&) { commitSceneName(); });
    mSceneSaveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSceneSave(); });
    mSceneDeleteBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSceneDelete(); });
    root->getChild<LLButton>("btn_scene_save_as")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickSceneSaveAs(); });
    mSceneRenameBtn = root->getChild<LLButton>("btn_scene_rename");
    mSceneRenameBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickSceneRename(); });
    refreshSceneList();
    updateTransportTooltipCache();

    mTabContainer = root->getChild<LLTabContainer>("director_tabs");
    mTabContainer->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTabChanged(); });
    struct GroupInfo { const char* page; const char* tabs; const char* icon; };
    static const GroupInfo groups[] = {
        { "quick_group", "quick_tabs", "Command_View_Icon" },
        { "cast_group", "cast_tabs", "Command_People_Icon" },
        { "performance_group", "performance_tabs", "Command_Poser_Icon" },
        { "camera_group", "camera_tabs", "Command_View_Icon" },
        { "lens_group", "lens_tabs", "Command_Search_Icon" },
        { "light_group", "light_tabs", "Command_PersonalLighting_Icon" },
        { "look_group", "look_tabs", "Command_Lightbox_Icon" },
        { "world_group", "world_tabs", "Command_Environments_Icon" },
        { "capture_group", "capture_tabs", "Command_Snapshot_Icon" },
    };
    for (const GroupInfo& info : groups)
    {
        LLPanel* page = mTabContainer->getPanelByName(info.page);
        LLTabContainer* tabs = page ? page->findChild<LLTabContainer>(info.tabs, false) : nullptr;
        if (page && tabs)
        {
            mGroups.emplace_back(page, tabs);
            mTabContainer->setTabImage(page, info.icon);
            tabs->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTabChanged(); });
        }
    }
    setMinimizeCallback([this](LLUICtrl*, const LLSD& minimized)
    {
        if (!minimized.asBoolean() && mPathPanel)
        {
            retargetPathFromCastSelection();
        }
    });

    mHelpBtn = root->getChild<LLButton>("btn_help");
    mLegendPanel = root->getChild<LLPanel>("legend_panel");
    mHotkeysPanel = root->getChild<LLPanel>("kit_hotkeys_popover");
    mHelpBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onToggleLegend(); });
    // btn_legend_close is wired by ALKitPopover::postBuild(); its hide path clears the shell's open-popover tracking.
    mLegendPanel->getChild<LLButton>("btn_legend_hotkeys")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            mLegendPanel->setVisible(false);
            if (ALKitPopover* popover = dynamic_cast<ALKitPopover*>(mHotkeysPanel))
                popover->show();
        });

    mCastList = root->getChild<LLScrollListCtrl>("cast_list");
    mCastRosterHeader = root->findChild<LLTextBox>("cast_roster_header");
    mCastHint = root->getChild<LLTextBox>("cast_hint");
    mRemoveBtn = root->getChild<LLButton>("btn_remove");
    mFocusBtn = root->getChild<LLButton>("btn_focus");
    mCastStatusCol = mCastList->getColumn("status")->mIndex;
    mCastNameCol = mCastList->getColumn("name")->mIndex;
    mCastABCol = mCastList->getColumn("ab")->mIndex;
    mCastMarkCol = mCastList->getColumn("mark")->mIndex;
    mCastLookCol = mCastList->getColumn("look")->mIndex;
    root->getChild<LLButton>("btn_add_you")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickAddYou(); });
    root->getChild<LLButton>("btn_add_animesh")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickAddAnimesh(); });
    mRemoveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCastRemove(); });
    mFocusBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickFocusActor(); });
    mCastList->setRightMouseDownCallback(
        [this](LLUICtrl* ctrl, S32 x, S32 y, MASK) { onCastRightClick(ctrl, x, y); });
    mCastList->setDoubleClickCallback([this]() { onCastSetSubject(SUBJECT_A); });
    mCastList->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        if (mPathPanel)
        {
            retargetPathFromCastSelection();
        }
        mShell->setCastSelection(selectedCastIds());
    });
    mGroupAssignCombo = root->getChild<LLComboBox>("group_assign_combo");
    mGroupAssignCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitGroupAssign(); });

    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
    LLUICtrl::EnableCallbackRegistry::ScopedRegistrar autoanim_enables;
    autoanim_enables.add("Director.CastHasGhostEntity", [this](LLUICtrl*, const LLSD&) { return castHasGhostEntity(); });
    registrar.add("Director.AutoAnimToggle", [this](LLUICtrl*, const LLSD&) { onCastAutoAnim(0); });
    registrar.add("Director.AutoAnimGo", [this](LLUICtrl*, const LLSD&) { onCastAutoAnim(1); });
    registrar.add("Director.AutoAnimFollow", [this](LLUICtrl*, const LLSD&) { onCastAutoAnim(2); });
    registrar.add("Director.SetSubjectA", [this](LLUICtrl*, const LLSD&) { onCastSetSubject(SUBJECT_A); });
    registrar.add("Director.SetSubjectB", [this](LLUICtrl*, const LLSD&) { onCastSetSubject(SUBJECT_B); });
    registrar.add("Director.SetSubjectC", [this](LLUICtrl*, const LLSD&) { onCastSetSubject(SUBJECT_C); });
    registrar.add("Director.SetSubjectD", [this](LLUICtrl*, const LLSD&) { onCastSetSubject(SUBJECT_D); });
    registrar.add("Director.SetMarkHere", [this](LLUICtrl*, const LLSD&) { onCastSetMarkHere(); });
    registrar.add("Director.ResetToMark", [this](LLUICtrl*, const LLSD&) { onCastResetToMark(); });
    registrar.add("Director.ClearLocoAnim", [this](LLUICtrl*, const LLSD&) { onCastClearLocoAnim(); });
    registrar.add("Director.CopyUUID", [this](LLUICtrl*, const LLSD&) { onCastCopyUUID(); });
    registrar.add("Director.LightResponse", [this](LLUICtrl*, const LLSD&)
    {
        LLUUID id = firstSelectedCastId();
        if (id.isNull())
        {
            id = gAgentID;
        }
        LLFloaterReg::showInstance("avatar_light_response", LLSD(id));
    });
    registrar.add("Director.RemoveFromCast", [this](LLUICtrl*, const LLSD&) { onCastRemove(); });
    registrar.add("Director.AnimCopyUUID", [this](LLUICtrl*, const LLSD&) { onAnimCopyUUID(); });
    registrar.add("Director.AnimSetLoco", [this](LLUICtrl*, const LLSD&) { onAnimSetLoco(); });
    registrar.add("Director.AnimPlayLocal", [this](LLUICtrl*, const LLSD&) { onAnimPlayLocal(true); });
    registrar.add("Director.AnimStopLocal", [this](LLUICtrl*, const LLSD&) { onAnimPlayLocal(false); });
    if (LLContextMenu* menu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>(
            "menu_director_cast.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance()))
    {
        mCastMenuHandle = menu->getHandle();
    }
    if (LLContextMenu* menu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>(
            "menu_director_anims.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance()))
    {
        mAnimMenuHandle = menu->getHandle();
    }

    mStatusStrip = root->getChild<LLTextBox>("status_strip");
    mRecIndicator = root->getChild<LLTextBox>("rec_indicator");
    mKitUIMs = root->findChild<LLTextBox>("kit_ui_ms");
    if (mKitUIMs)
    {
        if(ALKitBar* bar=dynamic_cast<ALKitBar*>(mKitUIMs->getParent()))
        {
            bar->setCollapsibleVisible(mKitUIMs,gSavedSettings.getBOOL("KitShowUIMs"));
        }
        else mKitUIMs->setVisible(gSavedSettings.getBOOL("KitShowUIMs"));
        LLHandle<LLFloaterDirector> handle=getDerivedHandle<LLFloaterDirector>();
        mKitUIMsConnection=ALKitSettingWatch::instance().watch("KitShowUIMs",
            [handle](const LLSD& value,const LLSD&)
            {
                if(LLFloaterDirector* director=handle.get())
                {
                    if(director->mKitUIMs)
                    {
                        if(ALKitBar* bar=dynamic_cast<ALKitBar*>(director->mKitUIMs->getParent()))
                        {
                            bar->setCollapsibleVisible(director->mKitUIMs,value.asBoolean());
                        }
                        else director->mKitUIMs->setVisible(value.asBoolean());
                    }
                }
            });
    }
    if(mShell)
    {
        mShell->history().watchSettings("chrome",{
            "DirectorArmMoves","DirectorArmCamera","DirectorArmRecorderPlay",
            "DirectorArmRecorderCapture","DirectorActionDelay","ActorMoverShowHeading"});
    }
}

void LLFloaterDirector::bindPage(const std::string& page, LLView* root)
{
    if (!root || !mBoundPages.insert(page).second)
    {
        return;
    }
    // One-off mapping from the v1 leaf panels in floater_director.xml to the
    // controls re-homed by each generated lazy page. Pages containing only
    // setting-bound kit rows need no Director member binder.
    using Binder = void (LLFloaterDirector::*)(LLView*);
    struct Binding { const char* page; Binder binder; };
    static const Binding bindings[] = {
        { "move_tab", &LLFloaterDirector::bindMovePage },
        { "path_tab", &LLFloaterDirector::bindPathPage },
        { "actor_style_tab", &LLFloaterDirector::bindActorStylePage },
        { "stylize_tab", &LLFloaterDirector::bindRotoInkPage },
        { "tron_tab", &LLFloaterDirector::bindTronPage },
        { "stepped_tab", &LLFloaterDirector::bindSteppedPage },
        { "director_settings_tab", &LLFloaterDirector::bindDirectorSettingsPage },
        { "image_tab", &LLFloaterDirector::bindImagePage },
        { "animate_tab", &LLFloaterDirector::bindAnimatePage },
        { "shot_tab", &LLFloaterDirector::bindCameraPage },
        { "moves_tab", &LLFloaterDirector::bindCameraPage },
        { "gaze_tab", &LLFloaterDirector::bindCameraPage },
        { "dance_tab", &LLFloaterDirector::bindCameraPage },
        { "ghosts_tab", &LLFloaterDirector::bindCameraPage },
        { "polish_tab", &LLFloaterDirector::bindCameraPage },
        { "exposure_tab", &LLFloaterDirector::bindCameraPage },
        { "actors_tab", &LLFloaterDirector::bindLightActorsPage },
        { "gaze_cues_tab", &LLFloaterDirector::bindLauncherPage },
        { "practicals_tab", &LLFloaterDirector::bindLauncherPage },
        { "light_cues_tab", &LLFloaterDirector::bindLauncherPage },
        { "focus_tab", &LLFloaterDirector::bindLensFocusPage },
        { "filters_tab", &LLFloaterDirector::bindLensFiltersPage },
        { "weather_tab", &LLFloaterDirector::bindWeatherPage },
        { "atmosphere_tab", &LLFloaterDirector::bindAtmospherePage },
        { "operator_tab", &LLFloaterDirector::bindOperatorPage },
        { "frame_orbit_tab", &LLFloaterDirector::bindFrameOrbitPage },
    };
    for (const Binding& binding : bindings)
    {
        if (page == binding.page)
        {
            (this->*binding.binder)(root);
        }
    }
}

void LLFloaterDirector::bindDirectorSettingsPage(LLView* root)
{
    mCrashBackupCombo = root->getChild<LLComboBox>("settings_crash_backups");
    mCrashBackupRestoreButton = root->getChild<LLButton>("settings_restore_backup");
    mCrashBackupRestoreButton->setCommitCallback([](LLUICtrl*, const LLSD&)
    {
        requestRestoreCrashBackup();
    });
    mCrashBackupDeleteButton = root->getChild<LLButton>("settings_delete_backup");
    mCrashBackupDeleteButton->setCommitCallback([](LLUICtrl*, const LLSD&)
    {
        requestDeleteCrashBackup();
    });
    // Theme choices come from the theme files at runtime, so a theme file added after the kit was
    // generated still appears. The generated items are only the fallback (and the default order).
    if (LLComboBox* theme_combo = root->findChild<LLComboBox>("settings_theme"))
    {
        const std::vector<ALDirectorTheme::ThemeInfo> themes = ALDirectorTheme::listThemes();
        if (!themes.empty())
        {
            theme_combo->clearRows(); // ui-draw-lint: allow(raw-write) runs once when the Settings page is bound
            for (const ALDirectorTheme::ThemeInfo& info : themes)
                theme_combo->add(info.label, LLSD(info.id));
            if (!theme_combo->setSelectedByValue(LLSD(gSavedSettings.getString(ALDirectorTheme::kSettingName)), true))
                theme_combo->setSelectedByValue(LLSD(std::string(ALDirectorTheme::kDefaultId)), true);
        }
    }
    root->getChild<LLButton>("settings_reset_prompts")->setCommitCallback([](LLUICtrl*, const LLSD&)
    {
        gSavedSettings.setS32("DirectorRecoveryMode", 0);
        for (const auto& entry : LLNotifications::instance().getTemplateNames())
        {
            if (entry.compare(0, 8, "Director") == 0)
            {
                LLNotifications::instance().setIgnored(entry, false);
                LLNotifications::instance().clearSavedResponse(entry); // no-op unless it remembers an answer
            }
        }
    });
    sCrashBackupSignature.clear(); // a rebuilt page repopulates its list
    refreshRecoveryControls();
}

// Look > Roto Ink (generated kit page). The look list is the kit preset strip (provider roto_ink);
// these are the page's code-bound buttons, and each calls the Lightbox's own handler target.
void LLFloaterDirector::bindRotoInkPage(LLView* root)
{
    if (LLButton* reset = root->findChild<LLButton>("ri_reset_all"))
    {
        // LightBox.RotoInkPreset "reset" -> ALRotoInk::resetToDefaults()
        reset->setCommitCallback([](LLUICtrl*, const LLSD&) { ALRotoInk::resetToDefaults(); });
    }
}

// Look > Tron (generated kit page). The look list is the kit preset strip (provider tron_look).
void LLFloaterDirector::bindTronPage(LLView* root)
{
    if (LLButton* reset = root->findChild<LLButton>("tr_reset_all"))
    {
        // LightBox.TronPreset "reset" -> ALTron::resetToDefaults()
        reset->setCommitCallback([](LLUICtrl*, const LLSD&) { ALTron::resetToDefaults(); });
    }
    // Palette/colour side effects are setting-signal driven in ALTron so
    // reset, paste, preset, undo and Lightbox writes behave identically.
    if (LLButton* snap = root->findChild<LLButton>("tr_grid_origin_snap_btn"))
    {
        // LightBox.TronGridOriginSnap
        snap->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            if (LLViewerCamera* camera = LLViewerCamera::getInstance())
            {
                gSavedSettings.setVector3d("TronGridOriginGlobal", gAgent.getPosGlobalFromAgent(camera->getOrigin()));
            }
        });
    }
    if (LLButton* clear = root->findChild<LLButton>("tr_grid_origin_clear_btn"))
    {
        // LightBox.TronGridOriginClear
        clear->setCommitCallback([](LLUICtrl*, const LLSD&) { gSavedSettings.setVector3d("TronGridOriginGlobal", LLVector3d::zero); });
    }
    if (LLButton* trails = root->findChild<LLButton>("tr_trail_clear_btn"))
    {
        // LightBox.TronClearTrails
        trails->setCommitCallback([](LLUICtrl*, const LLSD&) { ALTron::clearTrails(); });
    }
    if (LLButton* roto = root->findChild<LLButton>("tr_apply_roto_preset_btn"))
    {
        // LightBox.TronApplyRotoPreset
        roto->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            const std::string& key = ALTron::lastPresetRotoKey();
            if (!key.empty())
            {
                ALRotoInk::applyPreset(key);
            }
        });
    }
    if (LLButton* rim = root->findChild<LLButton>("tr_apply_rigrim_preset_btn"))
    {
        // LightBox.TronApplyRigRimPreset
        rim->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            const std::string& label = ALTron::lastPresetRigRimLabel();
            if (!label.empty() && ALPanelCineLightRig::applyRigRimPresetByName(label))
            {
                gSavedSettings.setS32("CineRigRimTronMode", ALTron::lastPresetRigRimTronMode());
            }
        });
    }
}

// Light > Actors (generated kit page rooted as panel_avatar_light_response). The panel's own
// postBuild binds the list and every per-avatar row; this binds the Director-only rows.
void LLFloaterDirector::bindLightActorsPage(LLView* root)
{
    if (LLButton* b = root->findChild<LLButton>("light_response_popout", true))
        b->setCommitCallback([this](LLUICtrl*, const LLSD&)
        {
            const LLUUID actor = firstSelectedCastId();
            LLFloaterReg::showInstance("avatar_light_response", LLSD(actor.notNull() ? actor : gAgentID));
        });
    if (LLButton* b = root->findChild<LLButton>("am_reset_CineLightRigActorMaskBias", true))
        b->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            if (LLControlVariable* c = gSavedSettings.getControl("CineLightRigActorMaskBias").get())
                c->resetToDefault(true);
        });
    if (LLButton* b = root->findChild<LLButton>("am_test_trigger", true))
    {
        const LLHandle<LLView> handle = b->getHandle();
        auto sync = [handle]()
        {
            if (LLButton* btn = dynamic_cast<LLButton*>(handle.get()))
                btn->setLabel(gSavedSettings.getU32("CineLightRigActorMaskTest") == 1 ? "Cancel test" : "Run test");
        };
        b->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            ALKitHistory::ScopedSuspend suspend;   // a command, never an undo step
            const U32 now = gSavedSettings.getU32("CineLightRigActorMaskTest");
            gSavedSettings.setU32("CineLightRigActorMaskTest", now == 1 ? 2 : 1);   // 1 arm, 2 cancel; self-resets to 0
        });
        mActorMaskTestWatch = ALKitSettingWatch::instance().watch("CineLightRigActorMaskTest",
            [sync](const LLSD&, const LLSD&) { sync(); });   // event-driven label, no per-frame read
        sync();
    }
}

void LLFloaterDirector::bindSteppedPage(LLView* root)
{
    if (!root)
    {
        return;
    }
    ALPanelSteppedAnim* panel = dynamic_cast<ALPanelSteppedAnim*>(root);
    if (!panel)
    {
        panel = root->findChild<ALPanelSteppedAnim>("stepped_tab_logic", true);
    }
    if (panel)
    {
        panel->bindSteppedPage(root);
    }
}

// Look > Image (generated kit page). The tone-mapper, grade, finish and operator rows are setting-bound
// in the kit; only the reset and Lightbox links are code.
void LLFloaterDirector::bindImagePage(LLView* root)
{
    ALColorGrade::populateLUTCombo(root->findChild<LLComboBox>("colorlut_combo"));
    if (LLButton* reset = root->findChild<LLButton>("exposure_default_btn"))
    {
        // "Reset tone" resets the tone-operator sliders this page owns. The exposure, tonemapper type,
        // legacy-skies and S-Log EV rows are homed on the Exposure page and are not reset here.
        reset->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            static const char* const tone_settings[] = {
                "AlchemyToneMapSaturation", "AlchemyToneMapContrast",
                "AlchemyToneMapKhronosDesaturation", "AlchemyToneMapKhronosStartCompression",
                "AlchemyToneMapAMDHDRMax", "AlchemyToneMapAMDContrast",
                "AlchemyToneMapAMDSaturationR", "AlchemyToneMapAMDSaturationG", "AlchemyToneMapAMDSaturationB",
                "AlchemyToneMapSLogCurve", "AlchemyToneMapSLogRange", "AlchemyToneMapSLogToe" };
            for (const char* name : tone_settings)
            {
                if (LLControlVariable* control = gSavedSettings.getControl(name).get())
                {
                    control->resetToDefault(true);
                }
            }
        });
    }
}

// Lens > Filters (generated kit page). The infrared and soft-edge preset actions and the avatar tag
// mirror ALPanelLensLooks::postBuild (alpanellenslooks.cpp:47-70): apply the preset, then snap the
// action combo back to its "Choose preset..." entry (value 0).
void LLFloaterDirector::bindLensFiltersPage(LLView* root)
{
    if (LLComboBox* ir = root->findChild<LLComboBox>("ir_preset_action"))
    {
        ir->setCommitCallback([](LLUICtrl* control, const LLSD&)
        {
            ALLensLooks::applyInfraredPreset(control->getValue().asInteger());
            control->setValue(LLSD("0"));
        });
    }
    if (LLComboBox* soft = root->findChild<LLComboBox>("softedge_preset_action"))
    {
        soft->setCommitCallback([](LLUICtrl* control, const LLSD&)
        {
            ALLensLooks::applySoftEdgePreset(control->getValue().asInteger());
            control->setValue(LLSD("0"));
        });
    }
    if (LLButton* tag = root->findChild<LLButton>("ir_enable_tag"))
    {
        tag->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            LLNotificationsUtil::add("EnableGBufferAvatarTagConfirm", LLSD(), LLSD(),
                [](const LLSD& notification, const LLSD& response)
                {
                    if (LLNotificationsUtil::getSelectedOption(notification, response) == 0)
                    {
                        gSavedSettings.setBOOL("RenderGBufferAvatarTag", true);
                    }
                    return false;
                });
        });
        LLHandle<LLButton> handle = tag->getDerivedHandle<LLButton>();
        const auto refresh_tag = [handle]
        {
            if (LLButton* button = handle.get())
            {
                button->setVisible(!gSavedSettings.getBOOL("RenderGBufferAvatarTag"));
            }
        };
        refresh_tag();
        mWorldLensConnections.emplace_back(ALKitSettingWatch::instance().watch(
            "RenderGBufferAvatarTag", [refresh_tag](const LLSD&, const LLSD&) { refresh_tag(); }));
    }
}

// Lens > Focus (generated kit page). CameraDoFResScale is an F32 and kit combos bind only S32/U32/String,
// so the Quality combo is code-bound: the commit writes the F32 and the setting signal keeps the combo in
// step (Lightbox and preset writes included). The item values are the generator iv list in order.
void LLFloaterDirector::bindLensFocusPage(LLView* root)
{
    LLComboBox* combo = root->findChild<LLComboBox>("dof_res_combo");
    if (!combo)
    {
        return;
    }
    combo->setCommitCallback([](LLUICtrl* control, const LLSD&)
    {
        gSavedSettings.setF32("CameraDoFResScale", (F32)control->getValue().asReal());
    });
    static const F32 kScales[] = { 0.25f, 0.5f, 0.7f, 1.0f };
    static const char* const kItems[] = { "0.25", "0.5", "0.7", "1.0" };
    auto select_scale = [](LLComboBox* box, F32 scale)
    {
        for (size_t i = 0; i < LL_ARRAY_SIZE(kScales); ++i)
        {
            if (std::fabs(kScales[i] - scale) < 1e-4f)
            {
                box->setValue(LLSD(kItems[i]));
                return;
            }
        }
    };
    LLHandle<LLComboBox> handle = combo->getDerivedHandle<LLComboBox>();
    select_scale(combo, gSavedSettings.getF32("CameraDoFResScale"));
    if (LLControlVariable* setting = gSavedSettings.getControl("CameraDoFResScale").get())
    {
        mWorldLensConnections.emplace_back(setting->getSignal()->connect(
            [handle, select_scale](LLControlVariable*, const LLSD& value, const LLSD&)
            {
                if (LLComboBox* box = handle.get())
                {
                    select_scale(box, (F32)value.asReal());
                }
            }));
    }
}

// World > Weather (generated kit page). Strike and Reset All are the same ALPanelWeatherSettings calls
// the v1 panel makes (alpanelweathersettings.cpp:135-138).
void LLFloaterDirector::bindWeatherPage(LLView* root)
{
    refreshWorldLensReadouts("world_group/weather_tab");
    LLHandle<LLFloaterDirector> handle = getDerivedHandle<LLFloaterDirector>();
    mWorldLensConnections.emplace_back(ALKitSettingWatch::instance().watch(
        "RenderDeferred", [handle](const LLSD&, const LLSD&)
        {
            if (LLFloaterDirector* director = handle.get())
            {
                director->refreshWorldLensReadouts("world_group/weather_tab");
            }
        }));
    if (LLButton* trigger = root->findChild<LLButton>("weather_trigger"))
    {
        trigger->setCommitCallback([](LLUICtrl*, const LLSD&) { ALPanelWeatherSettings::strikeLightningNow(); });
    }
    if (LLButton* reset = root->findChild<LLButton>("weather_reset_all"))
    {
        reset->setCommitCallback([](LLUICtrl*, const LLSD&) { ALPanelWeatherSettings::confirmResetAll(); });
    }
}

// Atmosphere > Depth Haze (generated kit page). The From-camera and Reset-all buttons call the same
// ALCineHaze helpers the Lightbox haze tab uses (alcinehaze.cpp:289-301). The Lightbox tab link is a
// kit button action (target lightbox, target_tab cinehaze_settings).
void LLFloaterDirector::bindAtmospherePage(LLView* root)
{
    refreshWorldLensReadouts("world_group/atmosphere_tab");
    if (LLButton* from_camera = root->findChild<LLButton>("ch_ref_from_camera"))
    {
        from_camera->setCommitCallback([](LLUICtrl*, const LLSD&) { ALCineHaze::setReferenceHeightFromCamera(); });
    }
    if (LLButton* reset = root->findChild<LLButton>("ch_reset_all"))
    {
        reset->setCommitCallback([](LLUICtrl*, const LLSD&) { ALCineHaze::resetToDefaults(); });
    }
}

void LLFloaterDirector::refreshWorldLensReadouts(const std::string& page)
{
    if (page == "world_group/weather_tab")
    {
        if (LLTextBox* note = findChild<LLTextBox>("weather_deferred_note"))
        {
            setTextIfChanged(note, "Weather requires Advanced Lighting Model (deferred rendering).");
            setVisibleIfChanged(note, !LLPipeline::sRenderDeferred);
        }
    }
    else if (page == "world_group/atmosphere_tab")
    {
        if (LLTextBox* requirement = findChild<LLTextBox>("localfog_froxel_requirement"))
        {
            setTextIfChanged(requirement, "Requires Froxel Air (Lightbox > Froxel Air).");
            setVisibleIfChanged(requirement, true);
        }
        if (LLTextBox* hint = findChild<LLTextBox>("localfog_hint"))
        {
            setTextIfChanged(hint, "Numeric placement and the in-world cage are local render tools. No volume is rezzed or sent to the simulator.");
            setVisibleIfChanged(hint, true);
        }
    }
}

void LLFloaterDirector::bindMovePage(LLView* root)
{
    mGroupRunCombo = root->findChild<LLComboBox>("group_run_combo");
    mGroupStartBtn = root->findChild<LLButton>("btn_group_start");
    mGroupStopBtn = root->findChild<LLButton>("btn_group_stop");
    mGroupsList = root->findChild<LLScrollListCtrl>("groups_list");
    mGroupDelaySpinner = root->findChild<LLSpinCtrl>("group_delay_spinner");
    mGroupRenameEditor = root->findChild<LLLineEditor>("group_rename_editor");
    mGroupRenameBtn = root->findChild<LLButton>("btn_group_rename");
    mGroupDissolveBtn = root->findChild<LLButton>("btn_group_dissolve");
    if (mGroupRunCombo && mGroupStartBtn && mGroupStopBtn && mGroupsList &&
        mGroupDelaySpinner && mGroupRenameEditor && mGroupRenameBtn && mGroupDissolveBtn)
    {
        mGroupStartBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroup(true); });
        mGroupStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroup(false); });
        mGroupsList->setCommitCallback([this](LLUICtrl*, const LLSD&) { onGroupsListSelect(); });
        mGroupDelaySpinner->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitGroupDelay(); });
        mGroupRenameEditor->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroupRename(); });
        mGroupRenameBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroupRename(); });
        mGroupDissolveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroupDissolve(); });
    }
    mMoverPanel = root->findChild<ALPanelActorMover>("actor_mover_panel");
}

void LLFloaterDirector::bindPathPage(LLView* root)
{
    mPathPanel = root->findChild<ALPanelPathEditor>("path_editor");
    if (mPathPanel)
    {
        retargetPathFromCastSelection();
    }
}

void LLFloaterDirector::bindActorStylePage(LLView* root)
{
    mActorStyleTarget = root->findChild<LLComboBox>("actor_style_target");
    mActorStyleEnabled = root->findChild<LLCheckBoxCtrl>("actor_style_enabled");
    mActorStyleMode = root->findChild<LLComboBox>("actor_style_mode");
    mActorStyleLook = root->findChild<LLComboBox>("actor_style_look");
    mActorStyleUseActorHue = root->findChild<LLCheckBoxCtrl>("actor_style_use_actor_hue");
    mActorStyleHue = root->findChild<LLSliderCtrl>("actor_style_hue");
    mActorStyleAlpha = root->findChild<LLSliderCtrl>("actor_style_alpha");
    mActorStyleDissolveProgress = root->findChild<LLSliderCtrl>("actor_style_dissolve_progress");
    mActorStylePixel = root->findChild<LLSliderCtrl>("actor_style_pixel");
    mActorStyleShimmerSpeed = root->findChild<LLSliderCtrl>("actor_style_shimmer_speed");
    mActorStyleShimmerAmount = root->findChild<LLSliderCtrl>("actor_style_shimmer_amount");
    mActorStyleGlitch = root->findChild<LLSliderCtrl>("actor_style_glitch");
    mActorStyleDistortion = root->findChild<LLComboBox>("actor_style_distortion");
    mActorStyleDistortionAmount = root->findChild<LLSliderCtrl>("actor_style_distortion_amount");
    mActorStyleBrightness = root->findChild<LLSliderCtrl>("actor_style_brightness");
    mActorStyleEffectFps = root->findChild<LLSpinCtrl>("actor_style_effect_fps");
    mActorStyleReset = root->findChild<LLButton>("actor_style_reset");
    mActorStyleStatus = root->findChild<LLTextBox>("actor_style_status");
    if (!mActorStyleTarget || !mActorStyleEnabled || !mActorStyleMode || !mActorStyleLook ||
        !mActorStyleUseActorHue || !mActorStyleHue || !mActorStyleAlpha ||
        !mActorStyleDissolveProgress || !mActorStylePixel || !mActorStyleShimmerSpeed ||
        !mActorStyleShimmerAmount || !mActorStyleGlitch || !mActorStyleDistortion ||
        !mActorStyleDistortionAmount || !mActorStyleBrightness || !mActorStyleEffectFps ||
        !mActorStyleReset || !mActorStyleStatus)
    {
        mActorStyleTarget = nullptr;
        mActorStyleEnabled = nullptr;
        mActorStyleMode = nullptr;
        mActorStyleLook = nullptr;
        mActorStyleUseActorHue = nullptr;
        mActorStyleHue = nullptr;
        mActorStyleAlpha = nullptr;
        mActorStyleDissolveProgress = nullptr;
        mActorStylePixel = nullptr;
        mActorStyleShimmerSpeed = nullptr;
        mActorStyleShimmerAmount = nullptr;
        mActorStyleGlitch = nullptr;
        mActorStyleDistortion = nullptr;
        mActorStyleDistortionAmount = nullptr;
        mActorStyleBrightness = nullptr;
        mActorStyleEffectFps = nullptr;
        mActorStyleReset = nullptr;
        mActorStyleStatus = nullptr;
        return;
    }
    mActorStyleTarget->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onActorStyleTargetChanged(); });
    for (LLUICtrl* control : {
            static_cast<LLUICtrl*>(mActorStyleEnabled),
            static_cast<LLUICtrl*>(mActorStyleMode),
            static_cast<LLUICtrl*>(mActorStyleLook),
            static_cast<LLUICtrl*>(mActorStyleUseActorHue),
            static_cast<LLUICtrl*>(mActorStyleHue),
            static_cast<LLUICtrl*>(mActorStyleAlpha),
            static_cast<LLUICtrl*>(mActorStyleDissolveProgress),
            static_cast<LLUICtrl*>(mActorStylePixel),
            static_cast<LLUICtrl*>(mActorStyleShimmerSpeed),
            static_cast<LLUICtrl*>(mActorStyleShimmerAmount),
            static_cast<LLUICtrl*>(mActorStyleGlitch),
            static_cast<LLUICtrl*>(mActorStyleDistortion),
            static_cast<LLUICtrl*>(mActorStyleDistortionAmount),
            static_cast<LLUICtrl*>(mActorStyleBrightness),
            static_cast<LLUICtrl*>(mActorStyleEffectFps) })
    {
        control->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onActorStyleControlChanged(); });
    }
    mActorStyleReset->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onActorStyleReset(); });
    mActorStyleTabDirty = true;
}

void LLFloaterDirector::bindAnimatePage(LLView* root)
{
    mAnimateHeader = root->findChild<LLTextBox>("animate_header");
    mAnimList = root->findChild<LLScrollListCtrl>("anim_list");
    mAnimHint = root->findChild<LLTextBox>("anim_hint");
    mAnimCopyBtn = root->findChild<LLButton>("btn_anim_copy");
    mAnimSetLocoBtn = root->findChild<LLButton>("btn_anim_setloco");
    mAnimPlayBtn = root->findChild<LLButton>("btn_anim_play");
    mAnimStopBtn = root->findChild<LLButton>("btn_anim_stop");
    mPasteEditor = root->findChild<LLLineEditor>("paste_uuid_editor");
    mPastePlayBtn = root->findChild<LLButton>("btn_paste_play");
    mPasteStopBtn = root->findChild<LLButton>("btn_paste_stop");
    mPasteSetLocoBtn = root->findChild<LLButton>("btn_paste_setloco");
    mLocomotionRoleCombo = root->findChild<LLComboBox>("locomotion_role_combo");
    mLocomotionSpeed = root->findChild<LLSpinCtrl>("locomotion_speed");
    mLocomotionPhase = root->findChild<LLSpinCtrl>("locomotion_phase");
    mLocomotionAllowAO = root->findChild<LLCheckBoxCtrl>("locomotion_allow_ao");
    mLocoText = root->findChild<LLTextBox>("loco_text");
    mClearLocoBtn = root->findChild<LLButton>("btn_clear_loco");
    mAnimPreviewPanel = root->findChild<ALPanelAnimPreview>("anim_preview_panel");
    if (!mAnimateHeader || !mAnimList || !mAnimHint || !mAnimCopyBtn || !mAnimSetLocoBtn || !mAnimPlayBtn || !mAnimStopBtn ||
        !mPasteEditor || !mPastePlayBtn || !mPasteStopBtn || !mPasteSetLocoBtn ||
        !mLocomotionRoleCombo || !mLocomotionSpeed || !mLocomotionPhase ||
        !mLocomotionAllowAO || !mLocoText || !mClearLocoBtn)
    {
        mAnimateHeader = nullptr;
        mAnimList = nullptr;
        mAnimHint = nullptr;
        mAnimCopyBtn = nullptr;
        mAnimSetLocoBtn = nullptr;
        mAnimPlayBtn = nullptr;
        mAnimStopBtn = nullptr;
        mPasteEditor = nullptr;
        mPastePlayBtn = nullptr;
        mPasteStopBtn = nullptr;
        mPasteSetLocoBtn = nullptr;
        mLocomotionRoleCombo = nullptr;
        mLocomotionSpeed = nullptr;
        mLocomotionPhase = nullptr;
        mLocomotionAllowAO = nullptr;
        mLocoText = nullptr;
        mClearLocoBtn = nullptr;
        mAnimPreviewPanel = nullptr;
        return;
    }
    mAnimPrioCol = mAnimList->getColumn("prio")->mIndex;
    mAnimPlayingCol = mAnimList->getColumn("playing")->mIndex;
    mLocomotionRoleCombo->setValue(LLDirectorCast::LOCO_WALK_FORWARD);
    mAnimCopyBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAnimCopyUUID(); });
    mAnimSetLocoBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAnimSetLoco(); });
    mAnimPlayBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAnimPlayLocal(true); });
    mAnimStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAnimPlayLocal(false); });
    mPastePlayBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPastePlayLocal(true); });
    mPasteStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPastePlayLocal(false); });
    mPasteSetLocoBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPasteSetLoco(); });
    mClearLocoBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClearLoco(); });
    mLocomotionRoleCombo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onLocomotionRoleChanged(); });
    for (LLUICtrl* control : { static_cast<LLUICtrl*>(mLocomotionSpeed),
                               static_cast<LLUICtrl*>(mLocomotionPhase),
                               static_cast<LLUICtrl*>(mLocomotionAllowAO) })
    {
        control->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onLocomotionMetadataCommit(); });
    }
    mAnimList->setRightMouseDownCallback(
        [this](LLUICtrl* ctrl, S32 x, S32 y, MASK) { onAnimRightClick(ctrl, x, y); });
    if (LLButton* open = root->findChild<LLButton>("btn_open_animexp"))
    {
        open->setCommitCallback(
            [](LLUICtrl*, const LLSD&) { LLFloaterReg::showInstance("animation_explorer"); });
    }
    mAnimateTabDirty = true;
}

void LLFloaterDirector::bindCameraPage(LLView* root)
{
    if (LLTextBox* control = root->findChild<LLTextBox>("subject_a_text", true))
    {
        mSubjectAText = control;
    }
    if (LLTextBox* control = root->findChild<LLTextBox>("subject_b_text", true))
    {
        mSubjectBText = control;
    }
    if (LLTextBox* control = root->findChild<LLTextBox>("subject_c_text", true))
    {
        mSubjectCText = control;
    }
    if (LLTextBox* control = root->findChild<LLTextBox>("subject_d_text", true))
    {
        mSubjectDText = control;
    }
    auto bind_subject = [this, root](const char* name, LLButton*& member, S32 subject, bool clear)
    {
        if (LLButton* control = root->findChild<LLButton>(name, true))
        {
            member = control;
            control->setCommitCallback([this, subject, clear](LLUICtrl*, const LLSD&)
            {
                if (clear)
                {
                    onClickClearSubject(subject);
                }
                else
                {
                    onClickSetSubjectFromSelection(subject);
                }
            });
            if (!clear)
                if (LLDragAndDropButton* drop = dynamic_cast<LLDragAndDropButton*>(control))
                    drop->setDragAndDropHandler(
                        [subject](S32, S32, MASK, bool commit, EDragAndDropType type,
                                  void* cargo, EAcceptance* accept, std::string&)
                        {
                            if (type != DAD_PERSON || !cargo)
                            {
                                *accept = ACCEPT_NO;
                                return false;
                            }
                            LLToolDragAndDrop* tool = LLToolDragAndDrop::getInstance();
                            const bool batch = subject == SUBJECT_A && tool &&
                                tool->getCargoCount() > 1;
                            *accept = batch ? ACCEPT_YES_MULTI : ACCEPT_YES_SINGLE;
                            if (commit)
                            {
                                LLDirectorCast& cast = LLDirectorCast::instance();
                                const S32 index = batch ? tool->getCargoIndex() : 0;
                                if (batch && index == 0)
                                    for (S32 mark = SUBJECT_A; mark <= SUBJECT_D; ++mark)
                                        clear_subject(cast, mark);
                                if (!batch || index < 4)
                                {
                                    const LLUUID& id = *static_cast<LLUUID*>(cargo);
                                    cast.add(id);
                                    assign_subject(cast, subject + index, id);
                                }
                            }
                            return true;
                        });
        }
    };
    bind_subject("btn_set_a", mSetABtn, SUBJECT_A, false);
    bind_subject("btn_set_b", mSetBBtn, SUBJECT_B, false);
    bind_subject("btn_set_c", mSetCBtn, SUBJECT_C, false);
    bind_subject("btn_set_d", mSetDBtn, SUBJECT_D, false);
    bind_subject("btn_clear_a", mClearABtn, SUBJECT_A, true);
    bind_subject("btn_clear_b", mClearBBtn, SUBJECT_B, true);
    bind_subject("btn_clear_c", mClearCBtn, SUBJECT_C, true);
    bind_subject("btn_clear_d", mClearDBtn, SUBJECT_D, true);
    if (LLButton* control = root->findChild<LLButton>("btn_assign_subjects_ad", true))
    {
        mAssignSubjectsBtn = control;
        control->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAssignSubjects(); });
    }
    if (LLButton* control = root->findChild<LLButton>("btn_actor_gaze", true))
    {
        control->setCommitCallback([this](LLUICtrl*, const LLSD&) { onOpenActorGaze(); });
    }
    if (ALPanelLensGaze* control = root->findChild<ALPanelLensGaze>("director_gaze_panel", true))
    {
        mGazePanel = control;
        mGazePanel->setSelectedActors(selectedCastIds());
    }
    if (LLButton* control = root->findChild<LLButton>("dance_popout"))
    {
        control->setCommitCallback([](LLUICtrl*, const LLSD&) { LLFloaterReg::showInstance("dance_ball"); });
    }
    if (LLButton* control = root->findChild<LLButton>("polish_popout"))
    {
        control->setCommitCallback([](LLUICtrl*, const LLSD&) { LLFloaterReg::showInstance("pose_polish"); });
    }
    if (LLButton* control = root->findChild<LLButton>("exposure_popout"))
    {
        control->setCommitCallback([](LLUICtrl*, const LLSD&) { LLFloaterReg::showInstance("exposure_hub"); });
    }
    if (LLTextBox* control = root->findChild<LLTextBox>("prism_summary"))
    {
        mPrismSummaryText = control;
    }
    if (LLButton* control = root->findChild<LLButton>("btn_prism_manage"))
    {
        mPrismManageBtn = control;
        control->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickManagePrism(); });
    }
    if (LLButton* control = root->findChild<LLButton>("btn_build_ots_pair"))
    {
        mOtsPairBtn = control;
        control->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickBuildOtsPair(); });
    }
    if (LLButton* control = root->findChild<LLButton>("btn_start_zolly"))
    {
        mZollyBtn = control;
        control->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickZolly(); });
    }
    if (LLButton* control = root->findChild<LLButton>("btn_start_hero_arc"))
    {
        mHeroArcBtn = control;
        control->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickHeroArc(); });
    }
    // Shot > Reset controls: the v1 panel's own handlers, reached through its statics
    // (the kit page has no ALPanelCineCamParams instance).
    if (LLButton* control = root->findChild<LLButton>("btn_reset_mode"))
    {
        control->setCommitCallback([](LLUICtrl*, const LLSD&) { ALPanelCineCamParams::resetModeToDefaults(); });
    }
    if (LLButton* control = root->findChild<LLButton>("btn_reset_all"))
    {
        control->setCommitCallback([](LLUICtrl*, const LLSD&) { ALPanelCineCamParams::confirmResetAll(); });
    }
    if (ALPanelCineCamParams* control = root->findChild<ALPanelCineCamParams>("cinecam_params_embedded"))
    {
        mCineCamPanel = control;
    }
    if (LLView* dance = root->findChild<LLView>("director_dance_ball_panel"))
    {
        mDancePanelView = dance;
        mDancePage = dynamic_cast<LLPanel*>(dance->getParent());
        ALScrollFit::fillPage(mDancePage, mDancePanelView, 30);
    }
    if (LLView* exposure = root->findChild<LLView>("director_exposure_panel"))
    {
        mExposurePanelView = exposure;
        mExposurePage = dynamic_cast<LLPanel*>(exposure->getParent());
        ALScrollFit::fillPage(mExposurePage, mExposurePanelView, 30);
    }
    if (LLView* ghosts = root->findChild<LLView>("ghost_studio_panel"))
    {
        mGhostPanelView = ghosts;
        mGhostPage = dynamic_cast<LLPanel*>(ghosts->getParent());
        ALScrollFit::fillPage(mGhostPage, mGhostPanelView, 8);
    }
}

// Operator (generated kit page). The built-in look combo runs the v1 panel's shake preset through its
// static applyShakePreset(); the kit page has no ALPanelCineCamParams instance.
void LLFloaterDirector::bindOperatorPage(LLView* root)
{
    if (LLComboBox* combo = root->findChild<LLComboBox>("shake_preset_combo"))
    {
        combo->setCommitCallback([](LLUICtrl* ctrl, const LLSD&)
        {
            if (LLComboBox* box = dynamic_cast<LLComboBox*>(ctrl))
            {
                ALPanelCineCamParams::applyShakePreset(box->getValue().asInteger());
            }
        });
    }
}

// Set while the frame-aspect combo mirrors the setting, so that the mirror does not write back.
static bool sFrameAspectSyncing = false;

// Frame & Orbit (generated kit page). Re-solve now was not bound in v2; it calls the same
// LLCinematicCamera::requestAutoReframe() the v1 button used. Frame aspect is an F32 setting: the combo
// writes the chosen ratio and mirrors the setting back (reset, paste, preset) under a recursion guard.
void LLFloaterDirector::bindFrameOrbitPage(LLView* root)
{
    if (LLButton* resolve = root->findChild<LLButton>("auto_frame_resolve", true))
    {
        resolve->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            LLCinematicCamera::instance().requestAutoReframe();
        });
    }
    LLComboBox* aspect = root->findChild<LLComboBox>("frame_aspect_ratio", true);
    if (!aspect)
    {
        return;
    }
    aspect->setCommitCallback([](LLUICtrl* ctrl, const LLSD&)
    {
        LLComboBox* box = dynamic_cast<LLComboBox*>(ctrl);
        if (box && !sFrameAspectSyncing)
        {
            gSavedSettings.setF32("CinematicFrameAspectRatio", (F32)box->getValue().asReal());
        }
    });
    auto mirror = [](LLComboBox* box, F32 value)
    {
        if (!box || box->getItemCount() <= 0)
        {
            return;
        }
        // Pick the item nearest to the stored ratio (values are float strings, e.g. 1.7777778).
        // The guard covers the probing selects too, so no commit can write the setting back.
        sFrameAspectSyncing = true;
        S32 best = 0;
        F64 best_diff = 1.0e30;
        for (S32 i = 0; i < box->getItemCount(); ++i)
        {
            box->setCurrentByIndex(i);
            const F64 diff = std::fabs(box->getValue().asReal() - (F64)value);
            if (diff < best_diff)
            {
                best_diff = diff;
                best = i;
            }
        }
        box->setCurrentByIndex(best);
        sFrameAspectSyncing = false;
    };
    if (LLControlVariable* setting = gSavedSettings.getControl("CinematicFrameAspectRatio"))
    {
        mirror(aspect, (F32)setting->getValue().asReal());
        mFrameAspectConnections.emplace_back(setting->getSignal()->connect(
            [aspect, mirror](LLControlVariable*, const LLSD& value, const LLSD&)
            {
                mirror(aspect, (F32)value.asReal());
            }));
    }

    const LLHandle<LLView> root_handle = root->getHandle();
    const auto update_settle_controls = [root_handle]()
    {
        LLView* page = root_handle.get();
        if (!page)
            return;
        const bool auto_frame = gSavedSettings.getBOOL("CinematicAutoFrameEnabled");
        const bool post_cut = auto_frame &&
            gSavedSettings.getS32("CinematicAutoFrameSettleMode") == 2;
        for (const char* name : { "auto_frame_settle_seconds",
                                  "auto_frame_settle_seconds_reset" })
            if (LLView* control = page->findChild<LLView>(name, true))
                control->setEnabled(post_cut);

        const bool custom = auto_frame &&
            gSavedSettings.getS32("CinematicAutoFrameSettleCurve") ==
                ALCameraCurve::CUSTOM_BEZIER;
        for (const char* name : { "auto_frame_settle_bezier_x1",
                                  "auto_frame_settle_bezier_y1",
                                  "auto_frame_settle_bezier_x2",
                                  "auto_frame_settle_bezier_y2",
                                  "auto_frame_settle_bezier_x1_reset",
                                  "auto_frame_settle_bezier_y1_reset",
                                  "auto_frame_settle_bezier_x2_reset",
                                  "auto_frame_settle_bezier_y2_reset" })
            if (LLView* control = page->findChild<LLView>(name, true))
                control->setEnabled(custom);
    };
    update_settle_controls();
    for (const char* setting_name : { "CinematicAutoFrameEnabled",
                                      "CinematicAutoFrameSettleMode",
                                      "CinematicAutoFrameSettleCurve" })
        if (LLControlVariable* setting = gSavedSettings.getControl(setting_name))
            mFrameAspectConnections.emplace_back(setting->getCommitSignal()->connect(
                [update_settle_controls](LLControlVariable*, const LLSD&, const LLSD&)
                { update_settle_controls(); }));
}

void LLFloaterDirector::bindLauncherPage(LLView* root)
{
    if (root->findChild<LLButton>("card_gaze_cues_open"))
    {
        addLauncherCard("gaze_cues",
            [this]
            {
                LLUUID actor = firstSelectedCastId();
                if (actor.isNull()) actor = gAgentID;
                LLFloaterReg::showInstance("gaze_cues", LLSD(actor.asString()));
            },
            [this]
            {
                LLUUID actor = firstSelectedCastId();
                if (actor.isNull()) actor = gAgentID;
                const S32 count = (S32)LLDirectorCast::instance().getGazeCues(actor).size();
                return llformat("%s: %d gaze cue%s", castMemberName(actor).c_str(), count, count == 1 ? "" : "s");
            });
    }
    if (root->findChild<LLButton>("card_diopter_open"))
    {
        addLauncherCard("diopter", [] { LLFloaterReg::showInstance("ultimate_diopter"); }, []
        {
            static LLCachedControl<bool> enabled(gSavedSettings, "CineDiopterEnabled", false);
            return std::string("Diopter: ") + (enabled ? "on" : "off");
        });
    }
    if (root->findChild<LLButton>("card_practicals_open"))
    {
        addLauncherCard("practicals", [] { ALFloaterLightBox::showTab("sellight_settings"); }, []
        {
            LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();
            LLViewerObject* object = selection->getPrimaryObject();
            if (!object) object = selection->getFirstRootObject();
            LLVOVolume* volume = dynamic_cast<LLVOVolume*>(object);
            if (!volume) return std::string(object ? "Selection is not a light" : "Select a projector in world");
            if (!volume->getIsLight()) return std::string("Selection is not a light (or its light is off)");
            if (volume->isLightSpotlight()) return std::string("Projector selected");
            return std::string("Point light selected (needs a projector)");
        });
    }
    if (root->findChild<LLButton>("card_light_cues_open"))
    {
        addLauncherCard("light_cues", [] { LLFloaterReg::showInstance("cine_light_cues"); }, []
        {
            const auto& cues = ALCineLightRigManager::instance().selected().cueList();
            const S32 count = (S32)cues.mCues.size();
            if (cues.mName.empty()) return llformat("%d cue%s", count, count == 1 ? "" : "s");
            return llformat("%d cue%s \xC2\xB7 %s", count, count == 1 ? "" : "s", cues.mName.c_str());
        });
    }
    if (root->findChild<LLButton>("card_haze_open"))
    {
        addLauncherCard("haze", [] { ALFloaterLightBox::showTab("cinehaze_settings"); }, []
        {
            static LLCachedControl<bool> on(gSavedSettings, "CineHazeEnable", false);
            return std::string(on ? "On" : "Off");
        });
    }
    if (root->findChild<LLButton>("card_shadows_probes_open"))
    {
        addLauncherCard("shadows_probes", [] { ALFloaterLightBox::showTab("shadow_settings"); }, []
        {
            static LLCachedControl<S32> detail(gSavedSettings, "RenderShadowDetail", 0);
            switch ((S32)detail)
            {
                case 1: return std::string("Sun/moon");
                case 2: return std::string("Sun/moon + projectors");
                default: return std::string("Off");
            }
        });
    }
    if (root->findChild<LLButton>("card_snapshot_open"))
    {
        addLauncherCard("snapshot", [] { LLFloaterReg::showInstance("snapshot"); }, []
        {
            return std::string("Stills and high-resolution frames");
        });
    }
}

void LLFloaterDirector::onOpen(const LLSD& key)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director onOpen");
    if (mShell && mShell->currentPageKey() == "look_group/image_tab")
    {
        ALColorGrade::populateLUTCombo(findChild<LLComboBox>("colorlut_combo"));
    }
    sRecoveryDirectorHandle = getHandle();
    if (mSceneDirtyText)
    {
        setTextIfChanged(mSceneDirtyText,
            recoveryServiceState().mSceneDirty ? "*" : "");
    }
    if(mIsV2&&!gSavedSettings.getBOOL("DirectorV2RectBumped")&&
       (getRect().getWidth()<ALKitStyle::FLOATER_DEFAULT_W||getRect().getHeight()<ALKitStyle::FLOATER_DEFAULT_H))
    {
        LLRect rect=getRect();
        S32 width=llmax(rect.getWidth(),ALKitStyle::FLOATER_DEFAULT_W);
        S32 height=llmax(rect.getHeight(),ALKitStyle::FLOATER_DEFAULT_H);
        if(gFloaterView)
        {
            width=llmin(width,gFloaterView->getLocalRect().getWidth());
            height=llmin(height,gFloaterView->getLocalRect().getHeight());
        }
        rect.setLeftTopAndSize(rect.mLeft,rect.mTop,width,height);
        setShape(rect);
        if(gFloaterView)translateIntoRect(gFloaterView->getLocalRect());
        gSavedSettings.setBOOL("DirectorV2RectBumped",true);
    }
    if(mShell)mShell->startSearchLoad();
    mLauncherClock.forceDue();
    // pick up scenes saved by an earlier session without losing the current
    // combo selection
    if (mSceneCombo)
    {
        refreshSceneList(mSceneCombo->getSelectedItemLabel());
    }

    // return to the tab left showing last time. [DirFix] D11: by tab NAME (a reorder of the
    // <panel> nodes no longer changes which tab reopens); fall back to the legacy index
    // (clamped: tab set can shrink) and migrate it. No early return: the legend reset and
    // LLFloater::onOpen below must always run.
    if (mTabContainer)
    {
        // Restore both tab levels without fitting each commit separately;
        // once the leaf is known, fit exactly that newly visible page.
        mScrollFit.setCommitFitEnabled(false);
        const std::string persisted_name = gSavedSettings.getString("DirectorLastTabName");
        const std::string name = ALDirectorTabMigration::restoreTabName(
            persisted_name, gSavedSettings.getS32("DirectorLastTab"));
        bool restored = !name.empty() && selectLeafTab(name);
        if (!restored)
        {
            const S32 last = llclamp(gSavedSettings.getS32("DirectorLastTab"), 0, 12);
            restored = selectLeafTab(migrate_tab_name(OLD_FLAT_TABS[last]));
        }
        if (!restored) selectLeafTab("move_tab");
        mScrollFit.setCommitFitEnabled(true);
        mScrollFit.fitPage(currentLeafPanel());
        if (LLPanel* leaf = currentLeafPanel())
            gSavedSettings.setString("DirectorLastTabName", leaf->getName());

        // [DirFix] D8: refreshers gated on tab visibility did not run while the console was
        // closed or the tab hidden; discard their caches on the first visible pass. (The
        // selectTab above fires onTabChanged, which sets these too; belt and braces.)
        mActorStyleTabDirty = true;
        mAnimateTabDirty = true;
        mGroupsTabDirty = true;

        // The tab commits above refit only the newly visible pages. Keep the
        // two opt-in unwrapped shared panels filled without another tree fit.
        ALScrollFit::fillPage(mDancePage, mDancePanelView, 30);
        ALScrollFit::fillPage(mExposurePage, mExposurePanelView, 30);
        ALScrollFit::fillPage(mGhostPage, mGhostPanelView, 8);
    }

    const uuid_vec_t selected = selectedCastIds();
    mLastGazePush = selected;
    ALFloaterActorGaze::updateSelection(ALFloaterActorGaze::ESelectionSource::DIRECTOR, selected);
    if (mGazePanel) mGazePanel->setSelectedActors(selected);

    // [DirFix] D14: the console is visible here (openFloater shows before onOpen);
    // if the restored tab is Path, feed + claim the current cast selection NOW so
    // the first painted frame already has the highlight / edit selection (the
    // panel's own reveal claim ran before any draw supplied the actor). Hidden
    // Path tab: revealFor only stores the actor.
    if (mPathPanel)
    {
        retargetPathFromCastSelection();
    }

    // a reopened console never resurfaces the legend
    if (mLegendPanel)
    {
        mLegendPanel->setVisible(false);
    }
    LLFloater::onOpen(key);
}

// ---------------------------------------------------------------------------
// tabs + legend
// ---------------------------------------------------------------------------
LLPanel* LLFloaterDirector::currentLeafPanel()
{
    if (!mTabContainer) return nullptr;
    LLPanel* group = mTabContainer->getCurrentPanel();
    for (const auto& entry : mGroups)
    {
        if (entry.first == group) return entry.second->getCurrentPanel();
    }
    return nullptr;
}

bool LLFloaterDirector::selectLeafTab(const std::string& name)
{
    if(mIsV2&&mShell)
    {
        return mShell->selectPage(name);
    }
    for (const auto& entry : mGroups)
    {
        if (LLPanel* leaf = entry.second->getPanelByName(name))
        {
            mTabContainer->selectTabPanel(entry.first);
            entry.second->selectTabPanel(leaf);
            return true;
        }
    }
    return false;
}

std::string LLFloaterDirector::pageKeyAt(S32 x,S32 y) const
{
    if(!mShell)return std::string();
    S32 shell_x=0;
    S32 shell_y=0;
    const_cast<LLFloaterDirector*>(this)->localPointToOtherView(x,y,&shell_x,&shell_y,mShell);
    return mShell->pageKeyAt(shell_x,shell_y);
}

bool LLFloaterDirector::isTextEntryFocus() const
{
    LLView* focus=dynamic_cast<LLView*>(gFocusMgr.getKeyboardFocus());
    for(LLView* view=focus;view;view=view->getParent())
    {
        if(dynamic_cast<LLLineEditor*>(view)||dynamic_cast<LLTextEditor*>(view)||dynamic_cast<LLSpinCtrl*>(view))return true;
        if(view==this)break;
    }
    return false;
}

void LLFloaterDirector::onTabChanged()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director onTabChanged");
    mLauncherClock.forceDue(); // newly shown launcher summaries fill in on the next draw
    if(mIsV2&&mShell)mShell->syncSelection();
    if (mTabContainer)
    {
        // [DirFix] D11: save by name AND keep writing the legacy index for one release
        // (older AlchemyTest.pre-*.exe backups read the index).
        if (LLPanel* panel = currentLeafPanel())
        {
            gSavedSettings.setString("DirectorLastTabName", panel->getName());
            const std::string legacy = legacy_tab_name(panel->getName());
            for (S32 i = 0; i < 13; ++i)
            {
                if (legacy == OLD_FLAT_TABS[i])
                {
                    gSavedSettings.setS32("DirectorLastTab", i);
                    break;
                }
            }
        }
    }
    // [DirFix] D8: a tab was revealed (and another hidden): the gated refreshers re-sync on
    // their next visible pass. Only flags are written here; nothing reads layout.
    mActorStyleTabDirty = true;
    mAnimateTabDirty = true;
    mGroupsTabDirty = true;
    // [DirFix] D14: Path tab revealed by a tab switch -> feed + claim before paint
    // (no-op store when another tab is now current).
    if (mPathPanel)
    {
        retargetPathFromCastSelection();
    }
}

void LLFloaterDirector::onToggleLegend()
{
    if (mLegendPanel)
    {
        if(ALKitPopover* popover=dynamic_cast<ALKitPopover*>(mLegendPanel))
        {
            if(popover->getVisible())popover->hide();
            else popover->show();
        }
        else
        {
            mLegendPanel->setVisible(!mLegendPanel->getVisible());
        }
    }
}

void LLFloaterDirector::addLauncherCard(const std::string& id,
                                        std::function<void()> open_fn,
                                        std::function<std::string()> summary_fn)
{
    LLButton* open=findChild<LLButton>("card_"+id+"_open");
    LLTextBox* summary=findChild<LLTextBox>("card_"+id+"_summary");
    if(!open||!summary)return;
    open->setCommitCallback([open_fn=std::move(open_fn)](LLUICtrl*,const LLSD&)
    {
        open_fn();
    });
    mLauncherCards.push_back({summary,std::move(summary_fn)});
}

void LLFloaterDirector::refreshLauncherCards()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director refreshLauncherCards");
    if (!mLauncherClock.due()) return;
    for (const LauncherCard& card : mLauncherCards)
    {
        if (isWidgetOnScreen(card.mSummary)) setTextIfChanged(card.mSummary, card.mFn());
    }
}

void LLFloaterDirector::draw()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("LLFloaterDirector draw");
    if(mIsV2)mKitDrawTimer.reset();
    refreshCastList();
    refreshGroupControls();
    refreshTransport();
    // One roster-order selection snapshot feeds every actor-targeted page.
    const uuid_vec_t selected = selectedCastIds();
    refreshMoveTab(selected);
    refreshPathTab(selected);
    refreshActorStyleTab();     // [DirFix] D8: gated on its tab being visible (inside)
    refreshAnimateTab(selected); // [DirFix] D8: gated on its tab being visible (inside)
    refreshGazeSelection(selected);
    refreshSubjects();
    refreshPrismDependents();
    refreshLauncherCards();
    refreshStatusStrip();
    LLFloater::draw();
    if(mIsV2&&mKitUIMs)
    {
        const F32 elapsed=mKitDrawTimer.getElapsedTimeF32()*1000.f;
        mKitDrawEMA=mKitDrawEMA==0.f?elapsed:lerp(mKitDrawEMA,elapsed,.1f);
        if(mKitDrawClock.due())setTextIfChanged(mKitUIMs,llformat("UI %.1f ms",mKitDrawEMA));
    }
    // the embedded ALPanelAnimPreview blits + spins its own dummy in its draw()
}

// [SliderWheel] [DirFix] D1's wheel rule (wheel over a slider adjusts it, except within 0.5 s of
// a scroller really scrolling) is global in llui now (indra/llui/llsliderwheel.h); the
// per-floater WheelGuard and the two overrides that were here are retired. Sliders stay
// explicit ON via installForTabs -> ALScrollFit::enableSliderWheel (legacy step).

bool LLFloaterDirector::handleKeyHere(KEY key, MASK mask)
{
    if(mIsV2)
    {
        LLView* focus=dynamic_cast<LLView*>(gFocusMgr.getKeyboardFocus());
        const bool text_focus=isTextEntryFocus();
        if((mask&MASK_CONTROL)&&!(mask&MASK_SHIFT)&&!text_focus)
        {
            if(key=='Z')
            {
                mShell->history().undo();
                return true;
            }
            if(key=='Y')
            {
                mShell->history().redo();
                return true;
            }
        }
        if(key==KEY_ESCAPE&&mask==MASK_NONE)
        {
            if(mShell->closePopover())return true;
            if(!mShell->search().queryText().empty())
            {
                mShell->clearSearch();
                return true;
            }
            if(LLLineEditor* editor=dynamic_cast<LLLineEditor*>(focus))
            {
                if(editor->getName().size()>=7&&editor->getName().compare(editor->getName().size()-7,7,"_search")==0)
                {
                    editor->setFocus(false);
                    return true;
                }
            }
        }
        if(key==KEY_F1&&mask==MASK_NONE&&!text_focus)
        {
            if(ALKitPopover* popover=dynamic_cast<ALKitPopover*>(mHotkeysPanel))popover->toggle();
            return true;
        }
        if((key==KEY_PAGE_UP||key==KEY_PAGE_DOWN)&&!(mask&MASK_CONTROL))
        {
            const S32 delta=key==KEY_PAGE_UP?-1:1;
            if(mask&MASK_ALT)
            {
                if(mShell->cycleGroup(delta))return true;
            }
            else if(mask&MASK_SHIFT)
            {
                if(mShell->cyclePage(delta))return true;
            }
            else if(mask==MASK_NONE&&mShell->cycleChip(delta))return true;
        }
        if(key==KEY_HOME&&mask==MASK_NONE)
        {
            for(LLView* view=focus;view;view=view->getParent())
            {
                if(ALKitRow* row=dynamic_cast<ALKitRow*>(view))
                {
                    if(row->resettable())
                    {
                        row->reset();
                        return true;
                    }
                    break;
                }
                if(view==this)break;
            }
        }
    }
    if (key == KEY_ESCAPE && mask == MASK_NONE)
    {
        // Esc = CUT, but ONLY while the transport is live; an idle floater
        // keeps the stock Esc behavior (defocus/close handling upstream)
        LLDirectorCast& cast = LLDirectorCast::instance();
        if (cast.isRunning() || cast.isCountingDown())
        {
            cast.cut();
            return true;
        }
    }
    return LLFloater::handleKeyHere(key, mask);
}

bool LLFloaterDirector::handleUnicodeCharHere(llwchar uni_char)
{
    if(mIsV2&&uni_char=='?'&&!isTextEntryFocus())
    {
        if(ALKitPopover* popover=dynamic_cast<ALKitPopover*>(mHotkeysPanel))popover->toggle();
        return true;
    }
    if(mIsV2&&uni_char=='/')
    {
        if(isTextEntryFocus())return LLFloater::handleUnicodeCharHere(uni_char);
        if(ALKitPage* page=mShell->currentKitPage())
        {
            std::string prefix=page->pageKey();
            const size_t slash=prefix.rfind('/');
            if(slash!=std::string::npos)prefix=prefix.substr(slash+1);
            if(prefix.size()>4&&prefix.compare(prefix.size()-4,4,"_tab")==0)prefix.erase(prefix.size()-4);
            if(LLLineEditor* search=page->findChild<LLLineEditor>(prefix+"_search"))
            {
                search->setFocus(true);
                return true;
            }
        }
    }
    return LLFloater::handleUnicodeCharHere(uni_char);
}

bool LLFloaterDirector::hasAccelerators()const
{
    return mIsV2;
}

ALKitHistory* LLFloaterDirector::kitHistory()
{
    return mShell?&mShell->history():nullptr;
}

bool LLFloaterDirector::handleMouseDown(S32 x,S32 y,MASK mask)
{
    if(mShell)
    {
        mShell->history().beginGesture(ALKitHistory::MOUSE,pageKeyAt(x,y));
        // A click on the floater chrome (title bar, drag handle) never reaches
        // the shell and doesn't move keyboard focus, so close the search popup here.
        S32 shell_x = 0, shell_y = 0;
        localPointToOtherView(x, y, &shell_x, &shell_y, mShell);
        if (!mShell->pointInView(shell_x, shell_y))
            mShell->setSearchPopupArmed(nullptr, false);
    }
    return LLFloater::handleMouseDown(x,y,mask);
}

bool LLFloaterDirector::handleRightMouseDown(S32 x,S32 y,MASK mask)
{
    if(mShell)mShell->history().beginGesture(ALKitHistory::MOUSE,pageKeyAt(x,y));
    return LLFloater::handleRightMouseDown(x,y,mask);
}

bool LLFloaterDirector::handleScrollWheel(S32 x,S32 y,LLScrollDelta delta)
{
    if(mShell)mShell->history().beginGesture(ALKitHistory::WHEEL,pageKeyAt(x,y));
    return LLFloater::handleScrollWheel(x,y,delta);
}

//static
void LLFloaterDirector::setToolTipIfChanged(LLUICtrl* ctrl, const std::string& tip)
{
    if (ctrl && ctrl->getToolTip() != tip)
    {
        ctrl->setToolTip(tip);
    }
}

// ---------------------------------------------------------------------------
// transport
// ---------------------------------------------------------------------------
void LLFloaterDirector::onClickAction()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    if (cast.isRunning() || cast.isCountingDown())
    {
        cast.cut();
    }
    else
    {
        cast.action();
    }
}

void LLFloaterDirector::onClickTakePlay()
{
    LLFlycamRecorder::instance().togglePlayback();
}

void LLFloaterDirector::onClickTakeStop()
{
    LLFlycamRecorder& recorder = LLFlycamRecorder::instance();
    if (recorder.getState() == LLFlycamRecorder::STATE_RECORDING)
        recorder.stopRecording();
    else
        recorder.stopPlayback();
}

void LLFloaterDirector::showTransportToast(const std::string& text)
{
    if (!mTransportToast || !mTransportBar)
        return;
    setTextIfChanged(mTransportToast, text);
    mTransportToastTimer.reset();
    if (!mTransportToast->getVisible())
        mTransportBar->setCollapsibleVisible(mTransportToast, true);
}

void LLFloaterDirector::onClickSetMarks()
{
    LLDirectorCast::instance().setMarks();
}

void LLFloaterDirector::onClickResetMarks()
{
    LLDirectorCast::instance().resetToMarks();
}

void LLFloaterDirector::updateTransportTooltipCache()
{
    static LLCachedControl<bool> arm_moves(gSavedSettings, "DirectorArmMoves", true);
    static LLCachedControl<bool> arm_camera(gSavedSettings, "DirectorArmCamera", true);
    static LLCachedControl<bool> arm_play(gSavedSettings, "DirectorArmRecorderPlay", false);
    static LLCachedControl<bool> arm_capture(gSavedSettings, "DirectorArmRecorderCapture", false);
    static LLCachedControl<F32> delay(gSavedSettings, "DirectorActionDelay", 0.f);
    std::string fires;
    if (arm_moves) fires += "actor moves, ";
    if (arm_camera) fires += "cinematic camera, ";
    if (arm_capture) fires += "recorder capture, ";
    else if (arm_play) fires += "recorder playback, ";
    mActionArmed = !fires.empty();
    if (!mActionArmed)
    {
        mActionTooltip = "Nothing armed: check at least one 'Action fires' box";
        mArmTooltip = "Choose what ACTION starts. " + mActionTooltip;
        return;
    }
    mActionTooltip = "Fires: " + fires.substr(0, fires.size() - 2);
    if ((F32)delay > 0.01f)
        mActionTooltip += llformat(" after a %.1f s countdown", (F32)delay);
    mArmTooltip = "Choose what ACTION starts. " + mActionTooltip;
}

void LLFloaterDirector::refreshTransport()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director refreshTransport"); // [DirFix] D9
    LLDirectorCast& cast = LLDirectorCast::instance();
    const bool counting = cast.isCountingDown();
    const bool running = cast.isRunning();
    const bool transport_live = counting || running;

    const S32 action_state = transport_live ? 1 : 0;
    if (mIsV2 && mLastTransportActionState != action_state)
    {
        mLastTransportActionState = action_state;
        ALKitStyle::styleTransportAction(mActionBtn, mCutBtn, mArmBtn, transport_live);
    }

    // ACTION morphs to CUT (danger styling from XML) while counting/running;
    // the countdown shows on the button itself
    mActionBtn->setVisible(!counting && !running);
    mCutBtn->setVisible(counting || running);
    if (counting)
    {
        const S32 seconds = llclamp((S32)ceilf(cast.countdownRemaining()), 0, 10);
        mCutBtn->setLabel(LLStringExplicit(COUNTDOWN_LABELS[seconds])); // ui-draw-lint: allow(raw-write) bounded cached label
        setToolTipIfChanged(mCutBtn, TOOLTIP_COUNTDOWN);
    }
    else if (running)
    {
        mCutBtn->setLabel(LLStringExplicit("CUT"));
        setToolTipIfChanged(mCutBtn, TOOLTIP_CUT);
    }

    mActionBtn->setEnabled(mActionArmed);
    setToolTipIfChanged(mActionBtn, mActionTooltip);
    setToolTipIfChanged(mArmBtn, mArmTooltip);

    if (mIsV2)
    {
        if (mTransportToast && mTransportToast->getVisible() &&
            mTransportToastTimer.getElapsedTimeF32() >= 2.5f)
        {
            mTransportBar->setCollapsibleVisible(mTransportToast, false);
        }
        LLFlycamRecorder& recorder = LLFlycamRecorder::instance();
        const LLFlycamRecorder::EState recorder_state = recorder.getState();
        const bool recording = recorder_state == LLFlycamRecorder::STATE_RECORDING;
        const bool playing = recorder_state == LLFlycamRecorder::STATE_PLAYING;
        const bool have_take = recorder.getNumKeyframes() > 0;
        mTakePlayBtn->setEnabled(have_take && !recording);
        mTakeStopBtn->setEnabled(recording || recorder.isPlaybackActive());
        const S32 take_tooltip_state = (have_take ? 1 : 0) | (playing ? 2 : 0) |
                                       (recording ? 4 : 0);
        if (take_tooltip_state != mTakeTooltipState)
        {
            mTakeTooltipState = take_tooltip_state;
            setToolTipIfChanged(mTakePlayBtn, !have_take ? "Record or load a take first" :
                (playing ? "Pause the current camera take" : "Play the current camera take"));
            setToolTipIfChanged(mTakeStopBtn, recording ? "Stop recording the current camera take" :
                "Stop playback and return the take to frame zero");
        }

        static LLCachedControl<F32> recorder_fps(gSavedSettings, "FlycamRecSampleRate", 30.f);
        const S32 fps = llclamp(ll_round((F32)recorder_fps), 1, 120);
        const F32 seconds = recording ? recorder.getDuration() : recorder.getPlayhead();
        const S64 frame = llmax((S64)0, (S64)floorf(llmax(0.f, seconds) * (F32)fps));
        const bool fps_changed = fps != mTimecodeFps;
        if (frame != mTimecodeFrame || fps_changed)
        {
            mTimecodeFrame = frame;
            mTimecodeFps = fps;
            const S64 total_seconds = frame / fps;
            const S32 frames = (S32)(frame % fps);
            const S32 hours = (S32)(total_seconds / 3600);
            const S32 minutes = (S32)((total_seconds / 60) % 60);
            const S32 secs = (S32)(total_seconds % 60);
            setTextIfChanged(mTakeTimecode, llformat("%02d:%02d:%02d:%02d", hours, minutes, secs, frames));
        }
        if (fps_changed)
            setToolTipIfChanged(mTakeTimecode, llformat("Camera-take timecode at %d fps", fps));
        if (mTimecodeHot != transport_live)
        {
            mTimecodeHot = transport_live;
            const LLUIColor& color = ALKitStyle::color(transport_live ? ALKitStyle::RECORD_TEXT : ALKitStyle::TEXT_STRONG);
            mTakeTimecode->setColor(color);
            mTakeTimecode->setReadOnlyColor(color);
        }
    }

    // marks
    const bool have_cast = !cast.getCast().empty();
    bool any_marked = false;
    for (const auto& m : cast.getCast())
    {
        if (m.mHasMark) { any_marked = true; break; }
    }
    mSetMarksBtn->setEnabled(have_cast);
    setToolTipIfChanged(mSetMarksBtn, have_cast ? TOOLTIP_SET_MARKS : TOOLTIP_ADD_CAST);
    mResetMarksBtn->setEnabled(any_marked);
    setToolTipIfChanged(mResetMarksBtn, any_marked ? TOOLTIP_RESET_MARKS : TOOLTIP_SET_MARKS_FIRST);

    // scene controls
    const bool naming = mSceneNameEditor->getVisible();
    const std::string scene_sel = mSceneCombo->getSelectedItemLabel();
    setToolTipIfChanged(mSceneSaveBtn, naming ? TOOLTIP_SCENE_WRITE : TOOLTIP_SCENE_SAVE);
    setToolTipIfChanged(mSceneCombo, !mSceneMetadataTooltip.empty() ? mSceneMetadataTooltip :
        (mSceneCombo->getItemCount() > 0
            ? TOOLTIP_SCENE_CHOOSE : TOOLTIP_SCENE_EMPTY));
    if (mSceneLoadBtn)
    {
        mSceneLoadBtn->setEnabled(!scene_sel.empty() && !naming);
        setToolTipIfChanged(mSceneLoadBtn, scene_sel.empty() ? TOOLTIP_SCENE_SELECT :
            (recoveryServiceState().mSceneDirty ? TOOLTIP_SCENE_LOAD_DIRTY : TOOLTIP_SCENE_LOAD));
    }
    mSceneDeleteBtn->setEnabled(!scene_sel.empty() && !naming);
    setToolTipIfChanged(mSceneDeleteBtn, scene_sel.empty() ? TOOLTIP_SCENE_SELECT :
        naming ? TOOLTIP_SCENE_NAMING : TOOLTIP_SCENE_DELETE);
    if (mSceneRenameBtn)
        mSceneRenameBtn->setEnabled(!scene_sel.empty() && !naming);
}

// ---------------------------------------------------------------------------
// scene files: one LLSD XML per scene in user_settings/director_scenes/,
// same folder/escape/list idiom as the cinematic camera presets
// ---------------------------------------------------------------------------
//static
std::string LLFloaterDirector::scenesDir()
{
    std::string dir = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, SCENE_SUBDIR);
    if (!gDirUtilp->fileExists(dir))
    {
        LLFile::mkdir(dir);
    }
    return dir;
}

//static
std::string LLFloaterDirector::sanitizeSceneName(std::string name)
{
    LLStringUtil::trim(name);
    return name;
}

//static
std::string LLFloaterDirector::scenePath(const std::string& name)
{
    // same reversible sanitization the graphics/cinematic presets use
    return gDirUtilp->add(scenesDir(), LLURI::escape(name) + ".xml");
}

//static
std::string LLFloaterDirector::crashBackupsDir()
{
    std::string dir = gDirUtilp->add(scenesDir(), CRASH_BACKUP_SUBDIR);
    if (!gDirUtilp->fileExists(dir))
    {
        LLFile::mkdir(dir);
    }
    return dir;
}

//static
std::string LLFloaterDirector::autosavePath()
{
    // One live file per viewer process: concurrent viewers never share a slot, and this viewer only ever
    // writes and deletes its own live file. A previous session's file (any other pid, the legacy plain
    // name) is never touched here; the next login moves it into crash_backups. sOwnLiveName is the plain pid
    // name, or a "-<n>" variant when a stale same-pid leftover could not be moved at login (beginOwnLiveSession):
    // autosave never writes over it.
    return gDirUtilp->add(scenesDir(), sOwnLiveName.empty()
                                           ? ALDirectorQol::liveRecoveryFileName(currentProcessId())
                                           : sOwnLiveName);
}

//static
void LLFloaterDirector::removeRecoveryFile()
{
    // Only this session's own autosave file: a previous session's unmoved backup is never ours to delete.
    if (sOwnLivePath.empty())
        return;
    LLFile::remove(sOwnLivePath, ENOENT);
    if (!gDirUtilp->fileExists(sOwnLivePath))
        sOwnLivePath.clear();
}

//static
const std::vector<std::string>& LLFloaterDirector::sceneSettingsList()
{
    // CinematicCamMode is NOT here: it is stored separately so a scene's
    // mode can be applied AFTER its named preset (the preset apply path
    // writes the mode too, and the scene's own mode must win)
    static const std::vector<std::string> settings = {
        // transport arming + countdown
        "DirectorArmMoves",
        "DirectorArmCamera",
        "DirectorArmRecorderPlay",
        "DirectorArmRecorderCapture",
        "DirectorActionDelay",
        // Render-only real-avatar gaze (per-member selection lives in cast data)
        "DirectorLookAtCameraEnabled",
        "DirectorLookAtCameraMode",
        "DirectorLookAtCameraStrength",
        "DirectorLookAtCameraHeadEye",
        "DirectorLookAtCameraTorso",
        "DirectorLookAtCameraTorsoAmount",
        "DirectorLookAtCameraSmoothing",
        "DirectorLookAtCameraEaseTime",
        // Switcher program data. Armed/live slot stay transient: loading a
        // scene is data application and must never seize the live camera.
        "DirectorSwitcherAuto",
        "DirectorSwitcherBank",
        "DirectorSwitcherEaseCuts",
        "DirectorSwitcherEaseSec",
        "DirectorSwitcherIntervalSec",
        "DirectorSwitcherJitterSec",
        "DirectorSwitcherSeed",
        "DirectorSwitcherSequence",
        // Flycam Orbit rig
        "FlycamOrbitEnabled",
        "FlycamOrbitLevel",
        "FlycamOrbitMinRadius",
        "FlycamOrbitMaxRadius",
        "FlycamOrbitOffsetUp",
        "FlycamOrbitOffsetLeft",
        "FlycamOrbitZoom",
        "FlycamOrbitSmoothing",
        // Cinematic target format, lens, and non-destructive screen guide
        "CinematicAutoFrameEnabled",
        "CinematicAutoFrameFill",
        "CinematicAutoFrameComposeLine",
        "CinematicAutoFrameDistanceTrim",
        "CinematicCamFrameOffsetUp",
        "CinematicFrameAspectRatio",
        "CinematicFrameCustomRatio",
        "CinematicFrameFocalLengthMM",
        "CinematicFrameGuideEnabled",
        "CinematicFrameGuideOpacity",
        "CinematicFrameGuideStyle",
        "CinematicFrameLensEnabled",
        // Move-tab (Actor Mover) parameters
        "ActorMoverSpeed",
        "ActorMoverDistance",
        "ActorMoverHeading",
        "ActorMoverWalkNominal",
        "ActorMoverEndMode",
        "ActorMoverSync",
        "ActorMoverUseCustomAnim",
        "ActorMoverCustomAnim",
        // Temporal Capture (World Time Scale): requested mode / scale / drive
        // gates only -- NOT effective scale, paused state, or Freeze World, which
        // is runtime-only and independently owned (see the brief, scene-save note)
        "TemporalMode",
        "TemporalWorldScale",
        "TemporalOutputFPS",
        "TemporalDriveAnimation",
        "TemporalDriveObjects",
        "TemporalDriveTextureAnim",
        "TemporalDriveParticles",
        "TemporalDriveCamera",
        // Surface Lens appearance is creative scene intent. Adaptive controls,
        // aggregate budget, master enable and debug remain global preferences.
        "PrismLensZoom",
        // Client-only cinematic light rig. Anchor and FX phase are stored in
        // the structured light_rig block; every persisted control is included
        // here so a scene remains a complete settings snapshot.
        "CineLightRigEnabled",
        "CineLightRigScaleAware",
        "CineLightRigRadius",
        "CineLightRigMasterEV",
        "CineLightRigMasterTempMired",
        "CineLightRigOffsetZ",
        "CineLightRigHeadroomStops",
        "CineLightRigBounceEnabled",
        "CineLightRigBounceRatio",
        "CineLightRigTransitionSec",
        "CineLightRigDamping",
        "CineLightRigTrackMode",
        "CineLightRigCookieUUID",
        "CineLightRigSeed",
        "CineLightRigMirror",
        "CineLightRigOrbitYaw",
        "CineLightRigOrbitPitch",
        "CineLightRigFX",
        "CineLightRigShadowMode",
        "CineLightRigGizmo",
        "CineLightRigKeyYaw",
        "CineLightRigKeyPitch",
        "CineLightRigKeyOffsetZ",
        "CineLightRigKeyProfile",
        "CineLightRigKeyEV",
        "CineLightRigKeyBeam",
        "CineLightRigKeyGobo",
        "CineLightRigKeyOn",
        "CineLightRigFillYaw",
        "CineLightRigFillPitch",
        "CineLightRigFillOffsetZ",
        "CineLightRigFillProfile",
        "CineLightRigFillEV",
        "CineLightRigFillBeam",
        "CineLightRigFillGobo",
        "CineLightRigFillOn",
        "CineLightRigRimYaw",
        "CineLightRigRimPitch",
        "CineLightRigRimOffsetZ",
        "CineLightRigRimProfile",
        "CineLightRigRimEV",
        "CineLightRigRimBeam",
        "CineLightRigRimGobo",
        "CineLightRigRimOn",
        "CineLightRigBgYaw",
        "CineLightRigBgPitch",
        "CineLightRigBgOffsetZ",
        "CineLightRigBgProfile",
        "CineLightRigBgEV",
        "CineLightRigBgBeam",
        "CineLightRigBgGobo",
        "CineLightRigBgOn",
        // [RigRim] Rig Rim Light (Phase A).
        "CineRigRimEnabled",
        "CineRigRimMasterGain",
        "CineRigRimRoughnessSoften",
        "CineRigRimBackSoftness",
        "CineRigRimTint",
        "CineRigRimShadow", // [RigRim]
        "CineRigRimIncludeAlpha",
        "CineRigRimIncludeProbes",
        "CineRigRimIncludeBounce",
        "CineRigRimSmoothness", "CineRigRimAntiShimmer", "CineRigRimSilhouette",
        "CineRigRimSilhouetteWidth", "CineRigRimSilhouetteUnits", "CineRigRimSilhouetteSoft",
        "CineRigRimSilhouetteGap", "CineRigRimSilhouetteSeedThreshold", "CineRigRimSilhouetteSeedSoft",
        "CineRigRimActorsOnly", "CineRigRimActorsOnlyAlpha", "CineRigRimWrapEnabled",
        "CineRigRimWrapStrength", "CineRigRimWrapWidth", "CineRigRimWrapBlur",
        "CineRigRimWrapSaturation", "CineRigRimWrapClamp",
        // [RigRim] P2-4: CineRigRimDebugRimOnly is deliberately NOT part of
        // scene state (Persist 0 in settings.xml) — a debug view should
        // never be saved into, or restored from, a Director scene.
        "CineRigRimKeyGain",
        "CineRigRimKeySharpness",
        "CineRigRimKeyWrap",
        "CineRigRimKeyBackBias",
        "CineRigRimFillGain",
        "CineRigRimFillSharpness",
        "CineRigRimFillWrap",
        "CineRigRimFillBackBias",
        "CineRigRimRimGain",
        "CineRigRimRimSharpness",
        "CineRigRimRimWrap",
        "CineRigRimRimBackBias",
        "CineRigRimBgGain",
        "CineRigRimBgSharpness",
        "CineRigRimBgWrap",
        "CineRigRimBgBackBias",
    };
    // [RotoInk] Append every CineOutline* key (Enabled + all [RotoInk]
    // settings) from ALRotoInk::settings() -- the single source of truth
    // shared with the Lightbox tab's preset/reset code -- rather than
    // duplicating the list here where it could drift.
    // [TronT0] Same pattern for every Tron* key, from ALTron::settings().
    static const std::vector<std::string> settings_with_roto = []
    {
        std::vector<std::string> v = settings;
        const std::vector<std::string>& roto = ALRotoInk::settings();
        v.insert(v.end(), roto.begin(), roto.end());
        const std::vector<std::string>& tron = ALTron::settings();
        v.insert(v.end(), tron.begin(), tron.end());
        return v;
    }();
    return settings_with_roto;
}

void LLFloaterDirector::refreshSceneList(const std::string& select_name)
{
    if (!mSceneCombo)
    {
        return;
    }
    mSceneCombo->clearRows();

    std::vector<std::string> names;
    {
        LLDirIterator dir_iter(scenesDir(), "*.xml");
        std::string file;
        while (dir_iter.next(file))
        {
            const std::string base_file = gDirUtilp->getBaseFileName(file, false);
            if (ALDirectorQol::isLiveRecoveryFile(base_file) ||
                ALDirectorQol::isLegacyKeptBackupName(base_file))
                continue;
            names.emplace_back(LLURI::unescape(
                gDirUtilp->getBaseFileName(file, /*strip_exten=*/true)));
        }
    }
    std::sort(names.begin(), names.end());
    for (const std::string& name : names)
    {
        mSceneCombo->add(name);
    }

    if (!select_name.empty() && mSceneCombo->setSelectedByValue(select_name, true))
    {
        onSceneSelected();
        return;
    }
    mSceneCombo->setLabel(LLStringExplicit("Scenes"));
    mSceneMetadataTooltip.clear();
    if (mSceneMetadataText)
        setTextIfChanged(mSceneMetadataText, "Select a scene");
}

void LLFloaterDirector::onSceneSelected()
{
    const std::string name = mSceneCombo->getSelectedItemLabel();
    if (name.empty())
        return;
    llstat status;
    std::string saved = "unknown time";
    if (LLFile::stat(scenePath(name), &status) == 0)
    {
        std::tm local_time;
#if LL_WINDOWS
        localtime_s(&local_time, &status.st_mtime);
#else
        localtime_r(&status.st_mtime, &local_time);
#endif
        char buffer[32];
        if (std::strftime(buffer, sizeof(buffer), "%I:%M %p", &local_time))
        {
            saved = buffer;
            if (!saved.empty() && saved[0] == '0')
                saved.erase(0, 1);
        }
    }
    mSceneMetadataTooltip = name + "\nSaved " + saved +
        "\nContains cast, marks, subjects, camera, Prism, rig, and scene settings.\nSelect previews only; press Load to apply.";
    if (mSceneMetadataText)
        setTextIfChanged(mSceneMetadataText, name + " \xC2\xB7 " + saved);
    setToolTipIfChanged(mSceneCombo, mSceneMetadataTooltip);
}

void LLFloaterDirector::onClickSceneLoad()
{
    const std::string name = mSceneCombo->getSelectedItemLabel();
    if (name.empty())
        return;
    updateSceneDirty();
    if (!recoveryServiceState().mSceneDirty)
    {
        loadScene(name);
        return;
    }
    LLSD args;
    args["NAME"] = name;
    LLHandle<LLFloater> handle = getHandle();
    LLNotificationsUtil::add("DirectorConfirmLoadScene", args, LLSD(),
        [handle, name](const LLSD& notification, const LLSD& response)
        {
            if (LLNotificationsUtil::getSelectedOption(notification, response) == 0)
                if (LLFloaterDirector* self = static_cast<LLFloaterDirector*>(handle.get()))
                    self->loadScene(name);
        });
}

void LLFloaterDirector::onClickSceneSave()
{
    if (!mSceneNameEditor->getVisible())
    {
        mSceneRenameSource.clear();
        mSceneSaveAs = false;
        // Only the scene that is actually LOADED may be re-saved without a
        // confirm; a combo selection is just a preview of another file.
        const std::string& current = recoveryServiceState().mCurrentSceneName;
        mSceneWriteSource = current;
        // reveal the inline name box in the combo's spot (usability rule 2:
        // no naming popups), preloaded so "tweak and re-save" is Save-Enter
        mSceneNameEditor->setText(current.empty() ? mSceneCombo->getSelectedItemLabel() : current);
        mSceneCombo->setVisible(false);
        mSceneNameEditor->setVisible(true);
        mSceneNameEditor->setFocus(true);
        mSceneNameEditor->selectAll();
    }
    else
    {
        commitSceneName();
    }
}

void LLFloaterDirector::onClickSceneSaveAs()
{
    if (mShell) mShell->closePopover();
    mSceneRenameSource.clear();
    mSceneSaveAs = true;
    mSceneWriteSource.clear();
    mSceneNameEditor->setText(std::string());
    mSceneCombo->setVisible(false);
    mSceneNameEditor->setVisible(true);
    mSceneNameEditor->setFocus(true);
}

void LLFloaterDirector::onClickSceneRename()
{
    // close the menu on every path, including the no-selection early return
    if (mShell) mShell->closePopover();
    const std::string name = mSceneCombo->getSelectedItemLabel();
    if (name.empty())
        return;
    mSceneRenameSource = name;
    mSceneSaveAs = false;
    mSceneWriteSource.clear();
    mSceneNameEditor->setText(name);
    mSceneCombo->setVisible(false);
    mSceneNameEditor->setVisible(true);
    mSceneNameEditor->setFocus(true);
    mSceneNameEditor->selectAll();
}

void LLFloaterDirector::commitSceneName()
{
    const std::string name = sanitizeSceneName(mSceneNameEditor->getText());
    mSceneNameEditor->setVisible(false);
    mSceneCombo->setVisible(true);
    if (name.empty())
    {
        mSceneRenameSource.clear();
        mSceneWriteSource.clear();
        mSceneSaveAs = false;
        return;     // empty name = cancel, nothing written
    }

    if (refuseReservedSceneName(name))
    {
        const std::string source = mSceneRenameSource;
        mSceneRenameSource.clear();
        mSceneWriteSource.clear();
        mSceneSaveAs = false;
        refreshSceneList(source);
        return;
    }

    if (!mSceneRenameSource.empty())
    {
        const std::string source = mSceneRenameSource;
        mSceneRenameSource.clear();
        mSceneSaveAs = false;
        if (source == name)
        {
            refreshSceneList(name);
            return;
        }
        const std::string target_path = scenePath(name);
        // Case-only renames ("Foo" -> "foo") hit the same file on Windows; allow them.
        const bool case_only = LLStringUtil::compareInsensitive(source, name) == 0;
        if (!case_only && gDirUtilp->fileExists(target_path))
        {
            LLNotificationsUtil::add("SystemMessageTip",
                LLSD().with("MESSAGE", "Scene was not renamed: '" + name + "' already exists."));
            refreshSceneList(source);
            return;
        }
        const std::string source_path = scenePath(source);
        if (LLFile::rename(source_path, target_path) != 0)
        {
            LL_WARNS("Director") << "Cannot rename scene file " << source_path
                                 << " to " << target_path << LL_ENDL;
            LLNotificationsUtil::add("SystemMessageTip",
                LLSD().with("MESSAGE", "Scene was not renamed because the file could not be moved."));
            refreshSceneList(source);
            return;
        }
        RecoveryServiceState& service = recoveryServiceState();
        if (service.mCurrentSceneName == source)
            service.mCurrentSceneName = name;
        refreshSceneList(name);
        showTransportToast("Renamed: " + name);
        return;
    }

    const bool confirm_overwrite = gDirUtilp->fileExists(scenePath(name)) &&
        (mSceneSaveAs || name != mSceneWriteSource);
    mSceneWriteSource.clear();
    mSceneSaveAs = false;
    if (confirm_overwrite)
    {
        LLHandle<LLFloater> handle = getHandle();
        LLSD args;
        args["MESSAGE"] = "A Director scene named '" + name + "' already exists. Replace it?";
        LLNotificationsUtil::add("GenericAlertYesCancel", args, LLSD(),
            [handle, name](const LLSD& notification, const LLSD& response)
            {
                if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
                    return;
                if (LLFloaterDirector* self = static_cast<LLFloaterDirector*>(handle.get()))
                    if (self->saveScene(name))
                        self->refreshSceneList(name);
            });
        return;
    }
    if (saveScene(name))
        refreshSceneList(name);
}

void LLFloaterDirector::onClickSceneDelete()
{
    if (mShell) mShell->closePopover();
    const std::string name = mSceneCombo->getSelectedItemLabel();
    if (name.empty())
    {
        return;
    }
    LLSD args;
    args["NAME"] = name;
    LLHandle<LLFloater> handle = getHandle();
    LLNotificationsUtil::add("DirectorConfirmDeleteScene", args, LLSD(),
        [handle, name](const LLSD& notification, const LLSD& response)
        {
            if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
            {
                return;
            }
            const std::string path = scenePath(name);
            const bool removed = LLFile::remove(path) == 0;
            if (!removed)
            {
                LL_WARNS("Director") << "Cannot delete scene file " << path << LL_ENDL;
            }
            if (LLFloaterDirector* self = static_cast<LLFloaterDirector*>(handle.get()))
            {
                RecoveryServiceState& service = recoveryServiceState();
                if (removed && service.mCurrentSceneName == name)
                    service.mCurrentSceneName.clear();
                self->refreshSceneList();
                self->showTransportToast("Deleted: " + name);
            }
        });
}

LLSD LLFloaterDirector::captureSceneData()
{
    LLSD scene = LLDirectorCast::instanceExists()
        ? LLDirectorCast::instance().sceneData() : LLSD::emptyMap();
    scene["version"] = SCENE_VERSION;
    scene["scene_name"] = recoveryServiceState().mCurrentSceneName;
    if (LLPrismLens::instanceExists())
    {
        const LLSD prism = LLPrismLens::sceneData();
        scene["prism_captures"] = prism["prism_captures"];
        scene["prism_displays"] = prism["prism_displays"];
        scene["prism_projectors"] = prism["prism_projectors"];
        scene["prism_gates"] = prism["prism_gates"];
    }
    if (ALCineLightRigManager::instanceExists())
        scene["light_rig"] = ALCineLightRigManager::instance().sceneData();

    // ... plus everything settings-backed
    LLSD settings = LLSD::emptyMap();
    for (const std::string& setting : sceneSettingsList())
    {
        if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
        {
            settings[setting] = setting == "CineLightRigFX"
                ? LLSD(ALCineLightRigPersistence::legacyFXForStorage(
                    ctrl->getValue().asInteger()))
                : ctrl->getValue();
        }
    }
    scene["settings"] = settings;

    // CineCam: active mode, and the ACTIVE named preset when one is selected
    // in the shared params panel (stored by name; applied via the panel on load)
    static LLCachedControl<S32> cinecam_mode(gSavedSettings, "CinematicCamMode", 0);
    scene["cinecam_mode"] = (S32)cinecam_mode;
    LLFloaterDirector* director = static_cast<LLFloaterDirector*>(sRecoveryDirectorHandle.get());
    if (director && director->mCineCamPanel)
    {
        const std::string preset = director->mCineCamPanel->getSelectedPresetName();
        if (!preset.empty())
        {
            scene["cinecam_preset"] = preset;
        }
    }
    return scene;
}

bool LLFloaterDirector::saveScene(const std::string& name)
{
    const std::string clean_name = sanitizeSceneName(name);
    if (clean_name.empty() || refuseReservedSceneName(clean_name))
        return false;
    RecoveryServiceState& service = recoveryServiceState();
    const std::string previous_name = service.mCurrentSceneName;
    service.mCurrentSceneName = clean_name;
    const bool saved = writeSceneFile(scenePath(clean_name), clean_name, true);
    if (saved)
    {
        markSceneClean();
        recoveryServiceState().mAutosaveTimer.reset();
        // This session's rolling autosave is obsolete once the scene is saved. Crash backups
        // (and any previous-session file still waiting to be moved) are never touched here.
        removeRecoveryFile();
        LL_INFOS("Director") << "Saved scene '" << clean_name << "'" << LL_ENDL;
        if (LLFloaterDirector* director = static_cast<LLFloaterDirector*>(sRecoveryDirectorHandle.get()))
            director->showTransportToast("Saved: " + clean_name);
    }
    else
        service.mCurrentSceneName = previous_name;
    return saved;
}

bool LLFloaterDirector::writeSceneFile(const std::string& path,
                                       const std::string& label,
                                       bool notify_failure,
                                       bool* oversized_out)
{
    if (oversized_out)
        *oversized_out = false;
    const LLSD scene = captureSceneData();
    // Serialize beside the destination, then replace it only after the stream
    // has flushed and closed cleanly. This keeps the previous scene intact on
    // disk-full, serialization, close, or replacement failure.
    const std::string temp_path = path + "." +
        LLUUID::generateNewID().asString() + ".tmp";
    const auto report_save_failure = [&label, notify_failure]()
    {
        if (!notify_failure)
            return;
        LLNotificationsUtil::add(
            "GenericAlert",
            LLSD().with("MESSAGE", llformat(
                "Unable to save Director scene '%s'. The previous saved scene, if any, was left unchanged.",
                label.c_str())));
    };
    // Serialize to memory first so the size cap and any formatter failure are decided before any file is
    // touched. A stringstream (not .str()) so the bytes are written straight from its buffer, with no
    // second full copy of a large scene.
    std::stringstream serialized;
    const S32 serialized_count = LLSDSerialize::toPrettyXML(scene, serialized);
    serialized.flush();
    const std::streamoff serialized_bytes = serialized.good() ? (std::streamoff)serialized.tellp() : (std::streamoff)-1;
    if (serialized_count <= 0 || !serialized.good() || serialized_bytes <= 0)
    {
        LL_WARNS("Director") << "Could not serialize the Director scene (count " << serialized_count
                             << ", stream " << (serialized.good() ? "good" : "failed")
                             << "); not written, previous file kept: " << path << LL_ENDL;
        report_save_failure();
        return false;
    }
    if (!ALDirectorQol::sceneSizeAllowed((size_t)serialized_bytes))
    {
        // Too large to write: keep the previous file. Autosave warns once per session; a manual save says so.
        if (oversized_out)
            *oversized_out = true;
        LL_WARNS("Director") << "Director scene is " << serialized_bytes << " bytes, over the "
                             << ALDirectorQol::kDirectorSceneMaxBytes << " byte cap; not written: " << path << LL_ENDL;
        if (notify_failure)
        {
            LLNotificationsUtil::add("GenericAlert",
                LLSD().with("MESSAGE", llformat(
                    "Director scene '%s' was not saved: it is larger than %s. The previous saved scene, if any, was left unchanged.",
                    label.c_str(), sceneCapLabel().c_str())));
        }
        else
        {
            static bool sWarnedAutosaveTooLarge = false;
            if (!sWarnedAutosaveTooLarge)
            {
                sWarnedAutosaveTooLarge = true;
                recoveryToast("Director autosave skipped: the scene is larger than " + sceneCapLabel() + ".");
            }
        }
        return false;
    }

    llofstream out(temp_path.c_str(), std::ios::out | std::ios::binary);
    if (!out.is_open())
    {
        LL_WARNS("Director") << "Cannot open temporary scene file "
                             << temp_path << LL_ENDL;
        LLFile::remove(temp_path, ENOENT);
        report_save_failure();
        return false;
    }

    out << serialized.rdbuf();
    out.flush();
    const bool write_succeeded = out.good();
    out.close();
    if (!write_succeeded || out.fail())
    {
        LL_WARNS("Director") << "Failed writing temporary scene file "
                             << temp_path << LL_ENDL;
        LLFile::remove(temp_path, ENOENT);
        report_save_failure();
        return false;
    }

    if (LLFile::rename(temp_path, path) != 0)
    {
        LL_WARNS("Director") << "Cannot replace scene file " << path
                             << " with completed temporary file" << LL_ENDL;
        LLFile::remove(temp_path, ENOENT);
        report_save_failure();
        return false;
    }

    return true;
}

// ui-draw-lint: allow-function(setting-write) loadScene runs only from the Load button and the recovery Restore alert callback, never per frame
bool LLFloaterDirector::loadScene(const std::string& name, bool recovery,
                                  const std::string& recovery_path)
{
    if (recovery && recovery_path.empty())
        return false;
    const std::string path = recovery ? recovery_path : scenePath(name);
    // Same path as crash-backup reads: the whole file is read into memory first (size cap, byte count and
    // stream state checked), then parsed from memory.
    LLSD scene;
    switch (readSceneFileClosed(path, scene))
    {
    case ReadResult::OK:
        break;
    case ReadResult::TOO_LARGE:
        LL_WARNS("Director") << "Scene file " << path << " is over the " << ALDirectorQol::kDirectorSceneMaxBytes
                             << " byte cap; not loaded" << LL_ENDL;
        if (!recovery)
            LLNotificationsUtil::add(
                "SystemMessageTip",
                LLSD().with("MESSAGE", "Scene was not loaded: the scene file is larger than " + sceneCapLabel() + "."));
        return false;
    case ReadResult::UNREADABLE:
        LL_WARNS("Director") << "Cannot read scene file " << path << LL_ENDL;
        if (!recovery)
            LLNotificationsUtil::add(
                "SystemMessageTip",
                LLSD().with("MESSAGE", "Scene was not loaded: the scene file could not be read."));
        return false;
    case ReadResult::CORRUPT:
        LL_WARNS("Director") << "Malformed scene file " << path << LL_ENDL;
        if (!recovery)
            LLNotificationsUtil::add(
                "SystemMessageTip",
                LLSD().with("MESSAGE", "Scene was not loaded: the scene file is malformed or incomplete."));
        return false;
    }

    ALKitHistory::ScopedSuspend history_suspend;

    const bool has_version = scene.has("version");
    U32 prism_offline_bindings = 0;
    const bool malformed_version = has_version && !scene["version"].isInteger();
    const S32 version = has_version && !malformed_version
        ? scene["version"].asInteger() : 1;
    if (malformed_version)
    {
        LL_WARNS("Director") << "Scene '" << name
                             << "' has a non-integer version; preserving live "
                                "Prism configuration and loading only tolerant "
                                "legacy scene fields" << LL_ENDL;
    }
    else if (version >= 3 && version <= SCENE_VERSION)
    {
        // v3-v6 share the same Prism schema. Later versions add cast locomotion
        // and actor-path data without changing Prism's atomic registry contract.
        // Validate and atomically replace Prism before mutating transport, cast,
        // settings, or CineCam state. A malformed payload leaves the live scene
        // and retained Prism outputs wholly untouched.
        std::string prism_reason;
        const LLPrismLens::ApplyReport prism_report =
            LLPrismLens::applySceneData(scene, &prism_reason);
        if (!prism_report)
        {
            LL_WARNS("Director") << "Rejected Prism data in scene '" << name
                                 << "': " << prism_reason << LL_ENDL;
            LLSD args;
            args["MESSAGE"] = "Scene was not loaded: " + prism_reason;
            LLNotificationsUtil::add("SystemMessageTip", args);
            return false;
        }
        if (!prism_report.mSkippedBindings.empty())
        {
            for (const LLPrismLens::SkippedBinding& skipped :
                 prism_report.mSkippedBindings)
            {
                ++prism_offline_bindings;
                LL_WARNS("Director") << skipped.mReason << ": object "
                    << skipped.mObjectId << " face " << skipped.mTextureEntry
                    << LL_ENDL;
            }
        }
    }
    else if (version != 1)
    {
        // No version-2 schema was ever shipped. Preserve the current registry
        // for unsupported/future files while allowing their legacy scene data
        // to load through its existing tolerant paths.
        LL_WARNS("Director") << "Scene '" << name << "' uses unsupported version "
                             << version << "; preserving live Prism configuration"
                             << LL_ENDL;
    }

    // A loaded scene starts from a clean transport and a released switcher.
    // Armed is deliberately not scene data: loading must not seize a camera.
    gSavedSettings.setBOOL("DirectorSwitcherArmed", false);
    ALDirectorSwitcher::instance().tick(
        LLPresentationTime::currentFrame().presentation_time);

    LLDirectorCast& cast = LLDirectorCast::instance();
    if (cast.isRunning() || cast.isCountingDown())
    {
        cast.cut();
    }

    // cast, marks, loco anims, subjects -- data only, nothing moves; members
    // not in world stay in the cast grayed "(away)"
    cast.applySceneData(scene);

    // only apply keys this floater owns: a scene file is data, not commands
    if (scene["settings"].isMap())
    {
        for (const std::string& setting : sceneSettingsList())
        {
            if (scene["settings"].has(setting))
            {
                if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
                {
                    ctrl->setValue(scene["settings"][setting]);
                }
            }
        }
    }

    // [RigRim] P1-C fix: a scene saved before Rig Rim existed (or any scene
    // whose settings map simply omits it) has no "CineRigRimEnabled" key.
    // The loop above only ever SETS a key it finds, so leaving the rest
    // untouched would let a PREVIOUSLY loaded scene's rim state (enabled,
    // custom per-light values) leak into this one. Reset every CineRigRim*
    // control to its default (master Enabled -> off, per-light values -> the
    // contract per-light defaults) so an old scene renders exactly as it was
    // authored, with Rig Rim off.
    if (!scene["settings"].isMap() ||
        !scene["settings"].has("CineRigRimEnabled"))
    {
        for (const std::string& setting : sceneSettingsList())
        {
            if (setting.rfind("CineRigRim", 0) == 0)
            {
                if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
                {
                    ctrl->resetToDefault(true);
                }
            }
        }
    }
    else
    {
        for (const std::string& setting : sceneSettingsList())
        {
            // A scene that already has the original Rig Rim block predates
            // only these quality controls. Preserve the historical behaviour
            // of every pre-existing omitted key; default only newly-added
            // quality keys so they cannot leak from the previously loaded scene.
            const bool new_quality_key =
                setting == "CineRigRimSmoothness" ||
                setting == "CineRigRimAntiShimmer" ||
                setting.rfind("CineRigRimSilhouette", 0) == 0 ||
                setting.rfind("CineRigRimActorsOnly", 0) == 0 ||
                setting.rfind("CineRigRimWrap", 0) == 0;
            if (new_quality_key && !scene["settings"].has(setting))
            {
                if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
                    ctrl->resetToDefault(true);
            }
        }
    }

    // [RotoInk] Same old-scene fix as the CineRigRim block above: a scene
    // saved before Rotoscope Ink existed has no "CineOutlineEnabled" key, so
    // reset every CineOutline* control to its default rather than let a
    // previously loaded scene's ink state leak into this one.
    if (!scene["settings"].isMap() ||
        !scene["settings"].has("CineOutlineEnabled"))
    {
        for (const std::string& setting : sceneSettingsList())
        {
            if (setting.rfind("CineOutline", 0) == 0)
            {
                if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
                {
                    ctrl->resetToDefault(true);
                }
            }
        }
    }
    else
    {
        // [RotoInk Anim Round-A review fix, P2 Opus] The scene HAS
        // CineOutlineEnabled (saved before Round-A's 23 look / 11 subject
        // keys existed, or otherwise just missing a few), so the all-reset
        // above did not fire -- but the per-key loop further up only ever
        // SETS a key the scene actually carries, leaving any key it lacks at
        // whatever the live session (or a previously loaded scene) left it
        // at. Reset to default every CineOutline* key this SPECIFIC scene
        // lacks; keys it does carry were already applied above and must
        // stay untouched.
        for (const std::string& setting : sceneSettingsList())
        {
            if (setting.rfind("CineOutline", 0) == 0 && !scene["settings"].has(setting))
            {
                if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
                {
                    ctrl->resetToDefault(true);
                }
            }
        }
    }

    // [TronT0] Same old-scene fix as the CineOutline block above: a scene
    // saved before the Tron effects package existed has no "TronEnabled"
    // key, so reset every Tron* control to its default rather than let a
    // previously loaded scene's Tron state leak into this one.
    if (!scene["settings"].isMap() ||
        !scene["settings"].has("TronEnabled"))
    {
        for (const std::string& setting : sceneSettingsList())
        {
            if (setting.rfind("Tron", 0) == 0)
            {
                if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
                {
                    ctrl->resetToDefault(true);
                }
            }
        }
    }
    else
    {
        // Scene HAS TronEnabled but may predate a later Tron key (grade/
        // grid/trace/rim/trail keys land in T1-T3): reset to default every
        // Tron* key this SPECIFIC scene lacks; keys it does carry were
        // already applied above and must stay untouched.
        for (const std::string& setting : sceneSettingsList())
        {
            if (setting.rfind("Tron", 0) == 0 && !scene["settings"].has(setting))
            {
                if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
                {
                    ctrl->resetToDefault(true);
                }
            }
        }
    }

    // [TronT3] A scene load is a hard edit: light-cycle trails laid under the
    // previous scene's cast/set have no business surviving into this one.
    ALTron::clearTrails();

    // Structured state carries the session-only anchor and the deterministic
    // FX phase, plus a denormalized base setup for self-contained scenes.
    ALCineLightRigManager::instance().applySceneData(scene["light_rig"]);

    // named preset first (its apply path writes CinematicCam* including the
    // mode), THEN the scene's explicit mode wins
    if (scene["cinecam_preset"].isString() && mCineCamPanel)
    {
        const std::string preset = scene["cinecam_preset"].asString();
        if (!mCineCamPanel->applyPresetByName(preset))
        {
            LL_WARNS("Director") << "Scene '" << name
                                 << "' references missing cinematic preset '"
                                 << preset << "'" << LL_ENDL;
        }
    }
    if (scene["cinecam_mode"].isInteger())
    {
        gSavedSettings.setS32("CinematicCamMode", scene["cinecam_mode"].asInteger());
    }
    if(mShell)
    {
        mShell->history().barrier("scene loaded");
    }
    if (prism_offline_bindings > 0)
    {
        LLSD args;
        args["MESSAGE"] = llformat(
            "Scene loaded; %u unavailable or invalid monitor face(s) retained for validation retry",
            prism_offline_bindings);
        LLNotificationsUtil::add("SystemMessageTip", args);
    }
    LL_INFOS("Director") << "Loaded scene '" << name << "'" << LL_ENDL;
    if (!recovery)
    {
        recoveryServiceState().mCurrentSceneName = sanitizeSceneName(name);
        markSceneClean();
    }
    else
    {
        RecoveryServiceState& service = recoveryServiceState();
        service.mCurrentSceneName = scene["scene_name"].isString()
            ? sanitizeSceneName(scene["scene_name"].asString()) : std::string();
        service.mCleanSceneStamp = SceneChangeStamp();
        service.mHaveCleanSceneStamp = true;
        service.mSceneDirty = true;
    }
    if (mSceneDirtyText)
        setTextIfChanged(mSceneDirtyText, recovery ? "*" : "");
    recoveryServiceState().mAutosaveTimer.reset();
    // A recovery load never deletes its source: a restored crash backup ages out through
    // pruning (restoreCrashBackup).
    if (!recovery)
        showTransportToast("Loaded: " + name);
    return true;
}

LLFloaterDirector::RecoveryServiceState& LLFloaterDirector::recoveryServiceState()
{
    static RecoveryServiceState service;
    return service;
}

LLFloaterDirector::SceneChangeStamp LLFloaterDirector::currentSceneStamp()
{
    SceneChangeStamp stamp;
    stamp.mSettings = recoveryServiceState().mSettingsRevision;
    if (LLDirectorCast::instanceExists())
        stamp.mCast = LLDirectorCast::instance().getSceneRevision();
    if (LLActorMover::instanceExists())
        stamp.mPaths = LLActorMover::instance().getPathRevision();
    if (LLPrismLens::instanceExists())
        stamp.mPrism = LLPrismLens::configurationRevision();
    if (ALCineLightRigManager::instanceExists())
        stamp.mRig = ALCineLightRigManager::instance().configurationRevision();
    if (ALAvatarLightResponse::instanceExists())
        stamp.mLightResponse = ALAvatarLightResponse::instance().revision();
    return stamp;
}

void LLFloaterDirector::initializeSceneRecovery()
{
    RecoveryServiceState& service = recoveryServiceState();
    if (!service.mSettingConnections.empty())
        return;
    for (const std::string& setting : sceneSettingsList())
    {
        if (LLControlVariable* control = gSavedSettings.getControl(setting))
        {
            service.mSettingConnections.emplace_back(control->getSignal()->connect(
                [](LLControlVariable*, const LLSD&, const LLSD&)
                {
                    ++recoveryServiceState().mSettingsRevision;
                    if (LLFloater* floater = sRecoveryDirectorHandle.get())
                    {
                        static_cast<LLFloaterDirector*>(floater)->updateTransportTooltipCache();
                    }
                }));
        }
    }
    if (LLControlVariable* control = gSavedSettings.getControl("CinematicCamMode"))
    {
        service.mSettingConnections.emplace_back(control->getSignal()->connect(
            [](LLControlVariable*, const LLSD&, const LLSD&)
            {
                ++recoveryServiceState().mSettingsRevision;
            }));
    }
    if (LLControlVariable* control = gSavedSettings.getControl("DirectorRecoveryMode"))
    {
        service.mSettingConnections.emplace_back(control->getSignal()->connect(
            [](LLControlVariable*, const LLSD&, const LLSD&)
            {
                // The Settings combo owns the preference; a previous notification ignore and its
                // remembered answer must never bypass a newly selected mode.
                LLNotifications::instance().setIgnored("DirectorRestoreRecovery", false);
                LLNotifications::instance().clearSavedResponse("DirectorRestoreRecovery");
            }));
    }
    markSceneClean();
}

void LLFloaterDirector::markSceneClean()
{
    RecoveryServiceState& service = recoveryServiceState();
    service.mCleanSceneStamp = currentSceneStamp();
    service.mHaveCleanSceneStamp = true;
    service.mSceneDirty = false;
    if (LLFloater* floater = sRecoveryDirectorHandle.get())
    {
        LLFloaterDirector* director = static_cast<LLFloaterDirector*>(floater);
        if (director->mSceneDirtyText)
            setTextIfChanged(director->mSceneDirtyText, "");
    }
}

void LLFloaterDirector::updateSceneDirty()
{
    RecoveryServiceState& service = recoveryServiceState();
    if (!service.mHaveCleanSceneStamp)
    {
        markSceneClean();
        return;
    }
    const bool dirty = currentSceneStamp() != service.mCleanSceneStamp;
    if (dirty == service.mSceneDirty)
        return;
    service.mSceneDirty = dirty;
    if (dirty)
        service.mAutosaveTimer.reset();
    if (LLFloater* floater = sRecoveryDirectorHandle.get())
    {
        LLFloaterDirector* director = static_cast<LLFloaterDirector*>(floater);
        if (director->mSceneDirtyText)
            setTextIfChanged(director->mSceneDirtyText, dirty ? "*" : "");
    }
}

void LLFloaterDirector::idleSceneRecovery(bool force_write)
{
    // Autosave ALWAYS runs on its timer and at shutdown: no recovery state can disable it.
    updateSceneDirty();
    RecoveryServiceState& service = recoveryServiceState();
    static LLCachedControl<S32> autosave_minutes(gSavedSettings, "DirectorAutosaveMinutes", 5);
    const S32 minutes = autosave_minutes;
    if (!service.mSceneDirty || (!force_write && (minutes <= 0 ||
        service.mAutosaveTimer.getElapsedTimeF32() < (F32)minutes * 60.f)))
    {
        return;
    }
    // An oversized scene was refused: do not serialize it again every tick. Retry once the scene changes
    // or kOversizedAutosaveRetrySeconds have passed.
    static bool sOversizeRefused = false;
    static SceneChangeStamp sOversizeStamp;
    static LLTimer sOversizeTimer;
    if (sOversizeRefused)
    {
        if (currentSceneStamp() == sOversizeStamp &&
            sOversizeTimer.getElapsedTimeF64() < ALDirectorQol::kOversizedAutosaveRetrySeconds)
        {
            service.mAutosaveTimer.reset();
            return;
        }
        sOversizeRefused = false;
    }
    const std::string path = autosavePath();
    ensureOwnLiveLock(path); // normally held since login; retried here if taking it failed then
    bool oversized = false;
    if (writeSceneFile(path, "recovery", false, &oversized))
    {
        LL_INFOS("Director") << "Updated Director recovery snapshot" << LL_ENDL;
        sOwnLivePath = path;
    }
    else if (oversized)
    {
        sOversizeRefused = true;
        sOversizeStamp = currentSceneStamp();
        sOversizeTimer.reset();
    }
    service.mAutosaveTimer.reset();
}

void LLFloaterDirector::sceneRecoveryIdle(void*)
{
    // Stop once shutdown/disconnect has begun: no autosave or offer then.
    if (sRecoveryShutdownPrepared || gDisconnected)
        return;

    // The login offer waits until every previous-session live file has been moved into
    // crash_backups (or the retries ran out), so the prompt names a file that exists there.
    if (sRecoveryOfferPending)
    {
        bool live_waiting = false;
        for (const std::string& path : sUnmovedLiveBackups)
            live_waiting = live_waiting || ALDirectorQol::isLiveRecoveryFile(gDirUtilp->getBaseFileName(path, false));
        if (!live_waiting || sMoveRetriesLeft <= 0)
        {
            sRecoveryOfferPending = false;
            offerSceneRecovery();
        }
    }

    // Real-time cadence (1 s), not a frame count: dirty-stamp polling, pending backup moves and
    // the Settings list refresh do not need display-frame precision.
    static LLTimer tick;
    if (tick.getElapsedTimeF32() < 1.f)
        return;
    tick.reset();
    retryPendingBackupMoves();
    idleSceneRecovery();
    refreshRecoveryControls();
}

void LLFloaterDirector::startRecoveryService()
{
    if (sRecoveryServiceStarted)
        return;
    // UI self-test runs (UIPerfSelfTest, non-persistent) must never prompt, autosave
    // or touch the user's crash backups: the modal blocks the harness and the
    // test session's scene isn't the user's. Leave the service off entirely.
    static LLCachedControl<std::string> self_test(gSavedSettings, "UIPerfSelfTest", "");
    if (!self_test().empty())
    {
        LL_INFOS("Director") << "UI self-test run: Director scene recovery service disabled" << LL_ENDL;
        return;
    }
    sRecoveryServiceStarted = true;
    initializeSceneRecovery();
    beginOwnLiveSession(scenesDir()); // own lock first: a collector must never hold our future slot's lock
    moveLiveBackupsAtLogin(); // once per session, before the first autosave can be written
    gIdleCallbacks.addFunction(&LLFloaterDirector::sceneRecoveryIdle, nullptr);
}

//static
void LLFloaterDirector::moveLiveBackupsAtLogin()
{
    sUnmovedLiveBackups.clear();
    sLoginMovedNames.clear();
    const std::string scenes = scenesDir();
    std::vector<std::string> found;
    bool live_found = false;
    const S32 my_pid = currentProcessId();
    const std::vector<std::string> listing = listFileNames(scenes, ".director_recovery*");
    for (const std::string& file : listing)
    {
        // This session's own live file was chosen and locked by beginOwnLiveSession before this runs: never collected.
        if (file == sOwnLiveName)
            continue;
        // Every live file that is not this process's own (other pids, the legacy plain name). A same-pid file under
        // any other name is a stale leftover of a crashed process whose pid this one reused.
        const bool live = ALDirectorQol::isPreviousSessionLiveFile(file, my_pid, false);
        // A legacy plain file (older builds never lock it) is collected only when it is old enough and no other viewer
        // process is running (see legacyLiveFileCollectable). Otherwise it is left for a later login.
        if (live && ALDirectorQol::isLegacyLiveRecoveryName(file) && !legacyLiveFileCollectable(gDirUtilp->add(scenes, file), file))
            continue;
        // The previous build's kept backups migrate into crash_backups too, once (they are
        // never offered: they were already answered).
        if (live || ALDirectorQol::isLegacyKeptBackupName(file))
            found.push_back(file);
        else if (ALDirectorQol::isLiveRecoveryLockFile(file))
        {
            // A lock file whose live file is gone (POSIX leftover of a crashed viewer): delete it when its
            // owner is dead (we can take it), leave it alone while a running viewer holds it.
            const std::string live_name = file.substr(0, file.size() - 5 /* ".lock" */);
            bool sibling_present = false;
            for (const std::string& other : listing)
                sibling_present = sibling_present || (other.size() == live_name.size() &&
                    ALDirectorQol::isLiveRecoveryFile(other) &&
                    std::equal(other.begin(), other.end(), live_name.begin(),
                               [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); }));
            if (!sibling_present)
            {
                LiveLock orphan;
                if (acquireLiveLock(gDirUtilp->add(scenes, file), orphan) == LockResult::ACQUIRED)
                    releaseLiveLock(orphan); // deletes the file
            }
        }
    }
    if (!found.empty())
    {
        const std::string crash_dir = crashBackupsDir();
        std::vector<std::string> existing = listFileNames(crash_dir, "*");
        for (const std::string& file : found)
        {
            const std::string path = gDirUtilp->add(scenes, file);
            const bool is_live = ALDirectorQol::isLiveRecoveryFile(file);
            // A live file is only ours to collect when its owner is dead: hold its lock through the move.
            // If a running viewer holds it, it is that viewer's active autosave: leave it completely alone.
            LiveLock claim;
            if (is_live)
            {
                const LockResult claimed = acquireLiveLock(ALDirectorQol::liveRecoveryLockName(path), claim);
                if (claimed != LockResult::ACQUIRED)
                {
                    LL_INFOS("Director") << "Director autosave " << file << " belongs to a running viewer (or could not be checked); not collected"
                                         << LL_ENDL;
                    continue;
                }
                live_found = true;
            }
            const std::string name = moveIntoCrashBackups(path, crash_dir, existing);
            releaseLiveLock(claim); // the owner is dead and the file is moved or queued: drop the stale .lock
            if (name.empty())
            {
                // A transient lock: retry on later ticks. Autosave writes its own live file meanwhile.
                sUnmovedLiveBackups.push_back(path);
                continue;
            }
            LL_INFOS("Director") << "Moved Director backup " << file << " to crash_backups/" << name << LL_ENDL;
            if (is_live)
                sLoginMovedNames.push_back(name);
        }
    }
    sMoveRetriesLeft = sUnmovedLiveBackups.empty() ? 0 : MOVE_RETRIES;
    sRecoveryOfferPending = live_found;
    if (found.empty())
        return;
    const std::string crash_dir = crashBackupsDir();
    if (!sLoginMovedNames.empty())
        sProtectedBackups.push_back(ALDirectorQol::sortCrashBackupsNewestFirst(sLoginMovedNames).front());
    pruneCrashBackups(crash_dir);
}

//static
void LLFloaterDirector::retryPendingBackupMoves()
{
    if (sUnmovedLiveBackups.empty() || sMoveRetriesLeft <= 0)
        return;
    --sMoveRetriesLeft;
    const std::string crash_dir = crashBackupsDir();
    std::vector<std::string> existing = listFileNames(crash_dir, "*");
    bool moved_any = false;
    for (auto it = sUnmovedLiveBackups.begin(); it != sUnmovedLiveBackups.end();)
    {
        if (!gDirUtilp->fileExists(*it))
        {
            it = sUnmovedLiveBackups.erase(it); // gone meanwhile
            continue;
        }
        const std::string file = gDirUtilp->getBaseFileName(*it, false);
        // A queued legacy plain file gets the same checks as at login. One that now fails them (touched since, or another
        // viewer process is running) belongs to a running viewer: drop it from the queue; a later login decides again.
        if (ALDirectorQol::isLegacyLiveRecoveryName(file) && !legacyLiveFileCollectable(*it, file))
        {
            it = sUnmovedLiveBackups.erase(it);
            continue;
        }
        // Re-check the owner is still dead (never collect a running viewer's autosave on a retry either).
        LiveLock claim;
        if (ALDirectorQol::isLiveRecoveryFile(file))
        {
            const LockResult claimed = acquireLiveLock(ALDirectorQol::liveRecoveryLockName(*it), claim);
            if (claimed == LockResult::HELD)
            {
                it = sUnmovedLiveBackups.erase(it); // a running viewer owns it now
                continue;
            }
            if (claimed != LockResult::ACQUIRED)
            {
                ++it;
                continue;
            }
        }
        const std::string name = moveIntoCrashBackups(*it, crash_dir, existing);
        releaseLiveLock(claim);
        if (name.empty())
        {
            ++it;
            continue;
        }
        LL_INFOS("Director") << "Moved Director backup " << file << " to crash_backups/" << name
                             << " (retry)" << LL_ENDL;
        if (ALDirectorQol::isLiveRecoveryFile(file))
            sLoginMovedNames.push_back(name);
        moved_any = true;
        it = sUnmovedLiveBackups.erase(it);
    }
    if (moved_any)
    {
        if (!sLoginMovedNames.empty())
            sProtectedBackups.push_back(ALDirectorQol::sortCrashBackupsNewestFirst(sLoginMovedNames).front());
        pruneCrashBackups(crash_dir);
        refreshRecoveryControls();
    }
    if (sUnmovedLiveBackups.empty())
        sMoveRetriesLeft = 0;
    else if (sMoveRetriesLeft <= 0)
        LL_WARNS("Director") << "Giving up moving " << sUnmovedLiveBackups.size()
                             << " Director backup file(s) until the next login; autosave keeps using its own file" << LL_ENDL;
}

void LLFloaterDirector::prepareRecoveryForShutdown()
{
    if (!sRecoveryServiceStarted || sRecoveryShutdownPrepared)
        return;
    sRecoveryShutdownPrepared = true;

    // Autosave runs at shutdown too. Only this session's own live files are ever removed;
    // crash backups and any previous-session file still waiting to move are left alone.
    updateSceneDirty();
    if (recoveryServiceState().mSceneDirty)
        idleSceneRecovery(true);
    else
        removeRecoveryFile();
    // The ownership lock is NOT released here. A quit can still be cancelled after this point (unsaved notecard or
    // script prompt), and a released lock would let another viewer collect this live autosave meanwhile. It is held
    // until the process exits: the OS drops it then (Windows also deletes the lock file). On POSIX the lock file is
    // left behind and the next login's sweep removes it.
}

void LLFloaterDirector::cancelRecoveryShutdown()
{
    // A quit was aborted (unsaved notecard/script prompt): the real quit must run the final recovery write/delete
    // again. The ownership lock was never released, so this is a no-op unless taking it failed earlier, in which case
    // it is retried.
    sRecoveryShutdownPrepared = false;
    if (sRecoveryServiceStarted)
        ensureOwnLiveLock(autosavePath());
}

// static
void LLFloaterDirector::refreshRecoveryControls()
{
    LLFloater* floater = sRecoveryDirectorHandle.get();
    if (!floater)
        return;
    LLFloaterDirector* self = static_cast<LLFloaterDirector*>(floater);
    LLComboBox* combo = self->mCrashBackupCombo;
    if (!combo)
        return; // the Settings page has not been built yet

    const std::string dir = crashBackupsDir();
    const std::vector<std::string> names =
        ALDirectorQol::sortCrashBackupsNewestFirst(listFileNames(dir, "*.xml"));
    // The signature covers each file's name, size, modification time and whether it can be read,
    // so a file whose first read failed shows its scene name once it becomes readable.
    std::string signature;
    for (const std::string& name : names)
    {
        const std::string path = gDirUtilp->add(dir, name);
        crashBackupSceneName(dir, name); // (re)tries an unread file on a slow timer
        llstat status;
        const bool have_stat = LLFile::stat(path, &status) == 0;
        signature += name + "|" + (have_stat ? std::to_string((S64)status.st_size) : std::string("-")) + "|" +
                     (have_stat ? std::to_string((S64)status.st_mtime) : std::string("-")) + "|" +
                     (crashBackupNameReadable(name) ? "r" : "u") + "\n";
    }
    if (signature != sCrashBackupSignature)
    {
        sCrashBackupSignature = signature;
        const std::string previous = combo->getSelectedValue().asString();
        combo->clearRows(); // ui-draw-lint: allow(raw-write) gated on the backup-list signature; runs on a 1 s tick, only when the files change
        for (const std::string& name : names)
        {
            const std::string path = gDirUtilp->add(dir, name);
            std::string label = ALDirectorQol::parseCrashBackupName(name).valid ? backupWhenLabel(path) : name;
            const std::string& scene_name = crashBackupSceneName(dir, name);
            if (!scene_name.empty())
                label += " - " + scene_name;
            combo->add(label, LLSD(name));
        }
        for (auto it = sBackupSceneNames.begin(); it != sBackupSceneNames.end();)
        {
            if (std::find(names.begin(), names.end(), it->first) == names.end())
                it = sBackupSceneNames.erase(it);
            else
                ++it;
        }
        if (names.empty())
            combo->setLabel(LLStringExplicit("No crash backups")); // ui-draw-lint: allow(raw-write) same signature gate
        else if (previous.empty() || !combo->setSelectedByValue(LLSD(previous), true))
            combo->selectFirstItem();
    }
    const bool available = !names.empty();
    if (combo->getEnabled() != available)
        combo->setEnabled(available);
    if (self->mCrashBackupRestoreButton && self->mCrashBackupRestoreButton->getEnabled() != available)
        self->mCrashBackupRestoreButton->setEnabled(available);
    if (self->mCrashBackupDeleteButton && self->mCrashBackupDeleteButton->getEnabled() != available)
        self->mCrashBackupDeleteButton->setEnabled(available);
}

// static
S32 LLFloaterDirector::restoreCrashBackup(const std::string& path)
{
    // Re-validated right before acting: the file may have been pruned or deleted since the
    // prompt / list was shown.
    if (path.empty() || !gDirUtilp->fileExists(path))
    {
        refreshRecoveryControls();
        return RESTORE_MISSING;
    }
    {
        // Read it fully first (stream closed before any rename): an unreadable file is left
        // alone; only one that opens, reads and fails to parse is set aside as .corrupt.
        LLSD probe;
        switch (readSceneFileClosed(path, probe))
        {
        case ReadResult::TOO_LARGE:
            LL_WARNS("Director") << "Director crash backup " << path << " is over the " << sceneCapLabel()
                                 << " cap and cannot be restored; keeping it" << LL_ENDL;
            return RESTORE_TOO_LARGE;
        case ReadResult::UNREADABLE:
            LL_WARNS("Director") << "Cannot read Director crash backup " << path << "; keeping it" << LL_ENDL;
            return RESTORE_UNREADABLE;
        case ReadResult::CORRUPT:
        {
            const bool set_aside = quarantineCorruptBackup(crashBackupsDir(), path);
            sCrashBackupSignature.clear();
            refreshRecoveryControls();
            return set_aside ? RESTORE_DAMAGED : RESTORE_FAILED;
        }
        case ReadResult::OK:
            break;
        }
    }
    // The load never deletes its source: a restored backup ages out through pruning.
    bool restored = false;
    if (LLFloaterDirector* self = LLFloaterReg::showTypedInstance<LLFloaterDirector>("director"))
        restored = self->loadScene("recovery", true, path);
    if (restored)
        sProtectedBackups.push_back(gDirUtilp->getBaseFileName(path, false));
    refreshRecoveryControls();
    return restored ? RESTORE_OK : RESTORE_FAILED;
}

namespace
{
// Full path of the Settings list's selection, or empty.
std::string selectedCrashBackupPath(LLComboBox* combo, const std::string& crash_dir)
{
    if (!combo)
        return std::string();
    const std::string name = combo->getSelectedValue().asString();
    return name.empty() ? std::string() : gDirUtilp->add(crash_dir, name);
}
} // namespace

// static
void LLFloaterDirector::requestRestoreCrashBackup()
{
    LLFloater* floater = sRecoveryDirectorHandle.get();
    if (!floater)
        return;
    const std::string path = selectedCrashBackupPath(
        static_cast<LLFloaterDirector*>(floater)->mCrashBackupCombo, crashBackupsDir());
    if (path.empty())
    {
        refreshRecoveryControls();
        return;
    }
    if (!gDirUtilp->fileExists(path))
    {
        recoveryToast("That crash backup no longer exists.");
        refreshRecoveryControls();
        return;
    }
    // The dialog carries the exact path it describes: the callback acts on it, never on a reselect.
    LLNotificationsUtil::add("DirectorConfirmRestoreRecovery",
        LLSD().with("TIME", backupWhenLabel(path)), LLSD().with("path", path),
        &LLFloaterDirector::onRestoreRecoveryConfirmed);
}

// static
void LLFloaterDirector::onRestoreRecoveryConfirmed(const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
        return;
    const std::string path = notification["payload"]["path"].asString();
    const std::string when = backupWhenLabel(path);
    const S32 result = restoreCrashBackup(path);
    restoreResultToast(result, when, false);
}

// static
void LLFloaterDirector::requestDeleteCrashBackup()
{
    LLFloater* floater = sRecoveryDirectorHandle.get();
    if (!floater)
        return;
    const std::string path = selectedCrashBackupPath(
        static_cast<LLFloaterDirector*>(floater)->mCrashBackupCombo, crashBackupsDir());
    if (path.empty())
    {
        refreshRecoveryControls();
        return;
    }
    if (!gDirUtilp->fileExists(path))
    {
        recoveryToast("That crash backup no longer exists.");
        refreshRecoveryControls();
        return;
    }
    LLNotificationsUtil::add("DirectorConfirmDeleteCrashBackup",
        LLSD().with("TIME", backupWhenLabel(path)), LLSD().with("path", path),
        &LLFloaterDirector::onDeleteCrashBackupConfirmed);
}

// static
void LLFloaterDirector::onDeleteCrashBackupConfirmed(const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
        return;
    const std::string path = notification["payload"]["path"].asString();
    if (path.empty() || !gDirUtilp->fileExists(path))
        recoveryToast("That crash backup no longer exists.");
    else if (!removeBackupFile(path))
        recoveryToast("Couldn't delete that crash backup.");
    refreshRecoveryControls();
}

void LLFloaterDirector::offerSceneRecovery()
{
    if (sRecoveryOfferActive || sLoginMovedNames.empty())
        return;
    // The newest crash backup created at this login (older ones stay listed in Director > Settings).
    const std::string path = gDirUtilp->add(
        crashBackupsDir(), ALDirectorQol::sortCrashBackupsNewestFirst(sLoginMovedNames).front());
    // Re-validated: it may have been pruned or deleted since it was moved.
    if (!gDirUtilp->fileExists(path))
        return;
    static LLCachedControl<S32> recovery_mode(gSavedSettings, "DirectorRecoveryMode");
    const S32 mode = recovery_mode;
    switch (ALDirectorQol::loginAction(true, mode))
    {
    case ALDirectorQol::LoginAction::NONE:
    case ALDirectorQol::LoginAction::KEEP:
        return; // the backup stays in crash_backups
    case ALDirectorQol::LoginAction::RESTORE_AUTO:
    {
        const std::string when = backupWhenLabel(path);
        restoreResultToast(restoreCrashBackup(path), when, true);
        return;
    }
    case ALDirectorQol::LoginAction::OFFER:
        break;
    }
    // Never offer a file that cannot be restored: an unreadable one is kept untouched, a
    // damaged one (opened, read fully, failed to parse) is set aside as .corrupt.
    {
        LLSD probe;
        switch (readSceneFileClosed(path, probe))
        {
        case ReadResult::TOO_LARGE:
            recoveryToast("Your unsaved Director scene backup is larger than " + sceneCapLabel() +
                          " and can't be restored; it was kept.");
            return;
        case ReadResult::UNREADABLE:
            recoveryToast("Couldn't read your unsaved Director scene backup; it was kept. "
                          "You can try again from Director > Settings.");
            return;
        case ReadResult::CORRUPT:
            if (quarantineCorruptBackup(crashBackupsDir(), path))
            {
                sCrashBackupSignature.clear();
                refreshRecoveryControls();
                recoveryToast("Your last unsaved Director scene backup is damaged. It was set aside as a "
                              ".corrupt file in the crash_backups folder.");
            }
            return;
        case ReadResult::OK:
            break;
        }
    }
    sRecoveryOfferActive = true;
    // When the popup is disabled in Preferences LLNotifications answers it by replaying a saved
    // response, with no user involved: remember that so the handler can never restore or delete then.
    const bool replayed = LLNotifications::instance().getIgnored("DirectorRestoreRecovery");
    LLNotificationsUtil::add("DirectorRestoreRecovery",
        LLSD().with("TIME", fileTimeLabel(path)), LLSD().with("path", path).with("replayed", replayed),
        &LLFloaterDirector::onRecoveryResponse);
}

void LLFloaterDirector::onRecoveryResponse(const LLSD& notification, const LLSD& response)
{
    sRecoveryOfferActive = false;
    const S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    const std::string path = notification["payload"]["path"].asString();
    const bool replayed = notification["payload"]["replayed"].asBoolean();
    const bool remember = response["ignore"].asBoolean();
    // A ticked "Don't ask me again" only sets the mode (Restore -> 1, Discard -> 2).
    // A replayed response was not given by the user now: it changes neither the mode nor the ignore state.
    const S32 remembered = ALDirectorQol::rememberedRecoveryMode(option, remember, replayed);
    if (remembered >= 0)
        gSavedSettings.setS32("DirectorRecoveryMode", remembered);
    // The mode setting carries "don't ask again". Do not also let LLNotifications disable the popup and
    // store this answer for replay (save_option): a stored answer could otherwise act without the user.
    // (Never for a replay: that would re-enable a popup the user disabled in Preferences.)
    if (ALDirectorQol::mayClearPromptIgnore(replayed))
        if (LLNotificationPtr shown = LLNotifications::instance().find(notification["id"].asUUID()))
            shown->setIgnored(false);
    switch (ALDirectorQol::promptAction(option, remember, replayed))
    {
    case ALDirectorQol::PromptAction::RESTORE:
        {
            const std::string when = backupWhenLabel(path);
            const S32 result = restoreCrashBackup(path);
            if (result != RESTORE_OK)
                restoreResultToast(result, when, true);
        }
        break;
    case ALDirectorQol::PromptAction::DISCARD:
        // Only an unticked Discard click deletes, and only the one backup this prompt described.
        if (!path.empty() && gDirUtilp->fileExists(path) && !removeBackupFile(path))
            recoveryToast("Couldn't delete that crash backup.");
        break;
    case ALDirectorQol::PromptAction::KEEP:
        break; // closed / ticked Discard / replayed: the backup stays in crash_backups
    }
    refreshRecoveryControls();
}

// ---------------------------------------------------------------------------
// cast column
// ---------------------------------------------------------------------------
// [DirFix] D6: memo of the 8-char UUID prefix used in fallback names. A UUID's text never
// changes, so this is a pure cache; cleared when it grows past a handful of entries.
static const std::string& dirfix_short_id(const LLUUID& id)
{
    static std::unordered_map<LLUUID, std::string> sShortIdCache;
    auto it = sShortIdCache.find(id);
    if (it == sShortIdCache.end())
    {
        if (sShortIdCache.size() > 256)
        {
            sShortIdCache.clear();
        }
        it = sShortIdCache.emplace(id, id.asString().substr(0, 8)).first;
    }
    return it->second;
}

//static
std::string LLFloaterDirector::castMemberName(const LLUUID& id)
{
    if (LLDirectorCast::isSelf(id))
    {
        return "You";
    }
    if (const LLDirectorCast::CastMember* m = LLDirectorCast::instance().getMember(id);
        m && !m->mLastName.empty())
    {
        return m->mLastName;
    }
    if (LLGhostAvatar::isGhostId(id))
    {
        return "Entity clone " + dirfix_short_id(id);
    }
    if (LLAvatarName av_name; LLAvatarNameCache::get(id, &av_name))
    {
        return av_name.getCompleteName();
    }
    LLViewerObject* obj = gObjectList.findObject(id);
    LLVOAvatar* av = obj ? obj->asAvatar() : nullptr;
    return (av && av->isControlAvatar())
        ? "Animesh " + dirfix_short_id(id)
        : dirfix_short_id(id);
}

void LLFloaterDirector::refreshCastList()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director refreshCastList"); // [DirFix] D9
    LLDirectorCast& cast = LLDirectorCast::instance();
    const uuid_vec_t& ids = cast.getIds();

    if (mCastRosterHeader)
        setTextIfChanged(mCastRosterHeader, llformat("CAST %d", (S32)ids.size()));

    mCastHint->setVisible(ids.empty());

    // membership change -> rebuild rows (rare); otherwise only re-set cells
    std::vector<LLScrollListItem*> items = mCastList->getAllData();
    bool rebuild = items.size() != ids.size();
    if (!rebuild)
    {
        for (size_t i = 0; i < ids.size(); ++i)
        {
            if (items[i]->getValue().asUUID() != ids[i])
            {
                rebuild = true;
                break;
            }
        }
    }
    if (rebuild)
    {
        const LLUUID prev_sel = firstSelectedCastId();
        mCastList->deleteAllItems();
        mRowStates.clear();
        mRowStates.resize(ids.size());
        for (const LLUUID& id : ids)
        {
            LLSD row;
            row["value"] = id;
            row["columns"][0]["column"] = "status";
            row["columns"][0]["type"] = "icon";
            row["columns"][0]["value"] = "";
            row["columns"][1]["column"] = "name";
            row["columns"][1]["value"] = "";
            row["columns"][2]["column"] = "ab";
            row["columns"][2]["value"] = "";
            row["columns"][3]["column"] = "mark";
            row["columns"][3]["type"] = "icon";
            row["columns"][3]["value"] = "";
            row["columns"][4]["column"] = "look";
            row["columns"][4]["value"] = "";
            mCastList->addElement(row, ADD_BOTTOM);
        }
        if (prev_sel.notNull())
        {
            mCastList->selectByID(prev_sel);
        }
        items = mCastList->getAllData();
    }

    const S32 status_col = mCastStatusCol;     // [DirFix] D7 cached in postBuild
    const S32 name_col = mCastNameCol;
    const S32 ab_col = mCastABCol;
    const S32 mark_col = mCastMarkCol;
    const S32 look_col = mCastLookCol;

    LLActorMover& mover = LLActorMover::instance();
    for (size_t i = 0; i < items.size(); ++i)
    {
        LLScrollListItem* item = items[i];
        CastRowState& state = mRowStates[i];
        const LLUUID id = item->getValue().asUUID();

        LLVOAvatar* av = cast.resolve(id);      // also refreshes cached name
        const bool in_world = av != nullptr;
        const bool moving = in_world && mover.isMoving(id);

        const std::string icon = !in_world ? ICON_GONE
                               : moving    ? ICON_MOVING
                                           : ICON_IDLE;
        const LLDirectorCast::CastMember* m = cast.getMember(id);

        std::string name = castMemberName(id);
        if (!in_world)
        {
            name += " (away)";
        }
        if (m && !m->mGroup.empty())
        {
            // the group tag rides the name cell as a middle-dot suffix -- the
            // narrow cast list has no room for a fifth column, and the diffed
            // CastRowState::mName below keys on the combined string anyway
            name += " \xC2\xB7 " + m->mGroup;
        }
        std::string ab;
        if (cast.getSubjectA() == id) ab = "A";
        else if (cast.getSubjectB() == id) ab = "B";
        else if (cast.getSubjectC() == id) ab = "C";
        else if (cast.getSubjectD() == id) ab = "D";
        const std::string mark = (m && m->mHasMark) ? ICON_MARK : "";
        const std::string look = cast.isLookAtCamera(id) ? "L" : "";

        if (state.mIcon != icon)
        {
            if (LLScrollListCell* cell = item->getColumn(status_col))
            {
                cell->setValue(icon);
            }
            state.mIcon = icon;
        }
        if (state.mName != name || state.mInWorld != in_world)
        {
            if (auto* cell = dynamic_cast<LLScrollListText*>(item->getColumn(name_col)))
            {
                cell->setText(name);
                // out-of-world members stay in the cast, grayed; grouped
                // members tint to their group's stable pastel hue so the
                // grouping reads at a glance (the diffed mName includes the
                // group suffix, so a retag re-applies the color)
                cell->setColor(!in_world ? LLColor4::grey
                             : (m && !m->mGroup.empty()) ? group_tint(m->mGroup)
                                                         : LLColor4::white);
            }
            state.mName = name;
            state.mInWorld = in_world;
        }
        if (state.mAB != ab)
        {
            if (auto* cell = dynamic_cast<LLScrollListText*>(item->getColumn(ab_col)))
            {
                cell->setText(ab);
            }
            state.mAB = ab;
        }
        if (state.mMark != mark)
        {
            if (LLScrollListCell* cell = item->getColumn(mark_col))
            {
                cell->setValue(mark);
            }
            state.mMark = mark;
        }
        if (state.mLook != look)
        {
            if (auto* cell = dynamic_cast<LLScrollListText*>(item->getColumn(look_col)))
            {
                cell->setText(look);
            }
            state.mLook = look;
        }
    }

    // remove needs a selection (tooltip says so, usability rule 1)
    const bool have_sel = mCastList->getFirstSelected() != nullptr;
    mRemoveBtn->setEnabled(have_sel);
    setToolTipIfChanged(mRemoveBtn, have_sel
        ? std::string("Remove the selected member(s) from the cast")
        : std::string("Select a cast member first"));

    // focus needs a selection that is actually in world (reason-tooltip otherwise)
    const LLUUID focus_id = firstSelectedCastId();
    const bool   focus_in_world = focus_id.notNull() && cast.resolve(focus_id) != nullptr;
    mFocusBtn->setEnabled(focus_in_world);
    setToolTipIfChanged(mFocusBtn, focus_id.isNull()
        ? std::string("Select a cast member first")
        : focus_in_world ? "Frame " + castMemberName(focus_id) + " in the camera"
                         : castMemberName(focus_id) + " is not in world");
}

uuid_vec_t LLFloaterDirector::selectedCastIds() const
{
    uuid_vec_t ids;
    for (LLScrollListItem* item : mCastList->getAllSelected())
    {
        ids.push_back(item->getValue().asUUID());
    }
    return ids;
}

LLUUID LLFloaterDirector::firstSelectedCastId() const
{
    LLScrollListItem* item = mCastList->getFirstSelected();
    return item ? item->getValue().asUUID() : LLUUID::null;
}

void LLFloaterDirector::retargetPathFromCastSelection()
{
    if (!mPathPanel)
        return;
    if (mShell)
    {
        ALKitPage* page = mShell->findBuiltPage("cast_group/path_tab");
        if (page && page->whoPinned())
            return;
    }
    mPathPanel->revealFor(firstSelectedCastId());
}

const uuid_vec_t& LLFloaterDirector::actorTargetsForPage(const std::string& page,
                                                          const uuid_vec_t& selected)
{
    if (!mShell)
        return selected;
    ALKitPage* kit_page = mShell->currentKitPage();
    if (!kit_page || kit_page->pageKey() != page || !kit_page->whoPinned())
        return selected;
    const LLUUID target = kit_page->actorTarget();
    if (kit_page->actorTargetAway() || target.isNull())
    {
        mPinnedActorTarget.clear();
        return mPinnedActorTarget;
    }
    if (mPinnedActorTarget.size() != 1 || mPinnedActorTarget.front() != target)
    {
        mPinnedActorTarget.clear();
        mPinnedActorTarget.push_back(target);
    }
    return mPinnedActorTarget;
}

void LLFloaterDirector::onCastRightClick(LLUICtrl* ctrl, S32 x, S32 y)
{
    LLScrollListItem* item = mCastList->hitItem(x, y);
    LLContextMenu* menu = mCastMenuHandle.get();
    if (item && menu)
    {
        if (!item->getSelected())
        {
            const S32 index = mCastList->getItemIndex(item);
            if (index >= 0)
            {
                mCastList->selectNthItem(index);
                // [DirFix] D14: programmatic select (no list commit) on a user
                // right-click -> feed + claim the Path host synchronously
                if (mPathPanel) retargetPathFromCastSelection();
            }
        }
        menu->buildDrawLabels();
        menu->updateParent(LLMenuGL::sMenuContainer);
        menu->show(x, y);
        LLMenuGL::showPopup(ctrl, menu, x, y);
    }
}

void LLFloaterDirector::onClickAddYou()
{
    if (isAgentAvatarValid())
    {
        LLDirectorCast::instance().add(gAgentAvatarp->getID());
    }
}

// Add every in-world-selected animesh object to the cast, resolving each linkset
// to its control avatar (the same id the right-click Director > Add to Cast uses).
// Non-animesh selections are skipped; add() is idempotent, so re-adds are no-ops.
void LLFloaterDirector::onClickAddAnimesh()
{
    LLObjectSelectionHandle sel = LLSelectMgr::getInstance()->getSelection();
    LLDirectorCast& cast = LLDirectorCast::instance();
    for (LLObjectSelection::valid_iterator it = sel->valid_begin(), end = sel->valid_end();
         it != end; ++it)
    {
        LLViewerObject* obj = (*it)->getObject();
        if (!obj)
        {
            continue;
        }
        LLViewerObject* root = obj->getRootEdit();
        if (!root)
        {
            root = obj;
        }
        if (!root->isAnimatedObject())
        {
            continue;
        }
        if (LLControlAvatar* cav = root->getControlAvatar())
        {
            if (!cav->isDead() && cav->getID().notNull())
            {
                cast.add(cav->getID()); // idempotent
            }
        }
    }
}

void LLFloaterDirector::onClickFocusActor()
{
    const LLUUID id = firstSelectedCastId();
    if (id.isNull())
    {
        return;
    }
    // reuse the viewer's "Zoom In" focus path; a no-op (returns false) when the
    // actor isn't a reachable in-world object
    if (LLDirectorCast::instance().resolve(id))
    {
        handle_zoom_to_object(id);
    }
}

void LLFloaterDirector::onCastSetSubject(S32 subject)
{
    const LLUUID id = firstSelectedCastId();
    if (id.isNull())
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    assign_subject(cast, subject, id);
}

void LLFloaterDirector::onCastSetMarkHere()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    for (const LLUUID& id : selectedCastIds())
        cast.setMark(id);
}

void LLFloaterDirector::onCastResetToMark()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    for (const LLUUID& id : selectedCastIds())
    {
        if (const LLDirectorCast::CastMember* m = cast.getMember(id); m && m->mHasMark)
        {
            LLActorMover::instance().placeAt(id, m->mMark);
        }
    }
}

void LLFloaterDirector::onCastClearLocoAnim()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    for (const LLUUID& id : selectedCastIds())
    {
        if (LLDirectorCast::CastMember* m = cast.getMember(id))
        {
            m->mLocoAnim.setNull();
            m->mLocomotionSet.mRoles.erase(LLDirectorCast::LOCO_WALK_FORWARD);
            cast.touchScene();
        }
    }
}

// [AutoAnimate] Do not mutate the cast model or rely on the Studio list selection.
bool LLFloaterDirector::castHasGhostEntity() const
{
    const ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& runtime : selectedCastIds()) if (studio.findInstanceByRuntime(runtime)) return true;
    return false;
}
void LLFloaterDirector::onCastAutoAnim(S32 operation)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::vector<LLUUID> instances;
    bool enable = false;
    for (const LLUUID& runtime : selectedCastIds())
        if (const auto* inst = studio.findInstanceByRuntime(runtime))
        { instances.push_back(inst->mId); enable = enable || !inst->mAutoAnimate; }
    for (const LLUUID& id : instances)
    {
        if (operation == 0) studio.setInstanceAutoAnimate(id, enable);
        else studio.autoAnimateAction(id, operation == 1 ? ALGhostAutoAnim::Action::GoAutonomous : ALGhostAutoAnim::Action::FollowSource);
    }
}

void LLFloaterDirector::onCastCopyUUID()
{
    const LLUUID id = firstSelectedCastId();
    if (id.isNull())
    {
        return;
    }
    // same LLClipboard idiom as the Animation Explorer's Copy UUID
    LLWString idwstr = utf8string_to_wstring(id.asString());
    LLClipboard::instance().copyToClipboard(idwstr, 0, narrow(idwstr.size()));
}

void LLFloaterDirector::onCastRemove()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    for (const LLUUID& id : selectedCastIds())
    {
        cast.remove(id);
    }
}

// ---------------------------------------------------------------------------
// groups: assign combo (cast column) + group transport (Move tab)
// ---------------------------------------------------------------------------
void LLFloaterDirector::onCommitGroupAssign()
{
    // typed name or picked row; whitespace trims away so " guards " and
    // "guards" are one group, and an empty commit ungroups. The "(none)" row
    // is the discoverable spelling of the empty commit.
    std::string name = mGroupAssignCombo->getSimple();
    LLStringUtil::trim(name);
    if (name == GROUP_NONE_LABEL)
    {
        name.clear();
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    for (const LLUUID& id : selectedCastIds())
    {
        cast.setGroup(id, name);    // multi-select tags a whole crowd at once
    }
    // force the next refreshGroupControls() to re-mirror the (possibly brand
    // new) name set + the selected member's tag into both combos
    mLastGroupNames.assign(1, std::string());   // impossible value ("" is never listed)
    mGroupShownFor.setNull();
}

void LLFloaterDirector::onClickGroup(bool start)
{
    const std::string name = mGroupRunCombo->getSelectedItemLabel();
    if (name.empty())
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    if (start)
    {
        // delay-aware: the whole group shares one delay, so with one set the
        // members queue and step off together N seconds from now; without one
        // this is exactly the old per-member start loop (each start() captures
        // its own parameters, like pressing Walk on each member)
        cast.startMovesStaggered(cast.membersInGroup(name));
    }
    else
    {
        // stop() also disarms each member's queued staggered start
        LLActorMover& mover = LLActorMover::instance();
        for (const LLUUID& id : cast.membersInGroup(name))
        {
            mover.stop(id);
        }
    }
}

// ---------------------------------------------------------------------------
// Groups management section (Move tab)
// ---------------------------------------------------------------------------
std::string LLFloaterDirector::selectedManagedGroup() const
{
    LLScrollListItem* item = mGroupsList->getFirstSelected();
    return item ? item->getValue().asString() : std::string();
}

void LLFloaterDirector::onGroupsListSelect()
{
    const std::string name = selectedManagedGroup();
    if (name.empty())
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    // highlight the group's members in the cast list (cheap: selectMultiple
    // walks the rows once), so "who is in this group" is one click. This also
    // makes the batch ops to the left (assign, marks, Move-tab Walk on the
    // selection) act on exactly this group.
    mCastList->selectMultiple(cast.membersInGroup(name));
    // [DirFix] D14: programmatic cast selection (no list commit) on a user click
    // in the groups list -> feed + claim the Path host synchronously
    if (mPathPanel) retargetPathFromCastSelection();
    // load the delay spinner + prefill the rename editor with the current name
    mGroupDelayShownFor = name;
    mGroupDelaySpinner->setValue(cast.getGroupDelay(name));
    mGroupRenameEditor->setText(name);
}

void LLFloaterDirector::onCommitGroupDelay()
{
    const std::string name = selectedManagedGroup();
    if (!name.empty())
    {
        LLDirectorCast::instance().setGroupDelay(name, (F32)mGroupDelaySpinner->getValue().asReal());
    }
}

void LLFloaterDirector::onClickGroupRename()
{
    const std::string old_name = selectedManagedGroup();
    std::string new_name = mGroupRenameEditor->getText();
    LLStringUtil::trim(new_name);
    if (old_name.empty() || new_name.empty() || new_name == GROUP_NONE_LABEL)
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    if (cast.renameGroup(old_name, new_name))
    {
        // rebuild everything group-shaped next draw and follow the selection
        // onto the new name so the operator's context is not lost
        mLastGroupNames.assign(1, std::string());
        mGroupShownFor.setNull();
        mGroupsListSnapshot.clear();
        mGroupDelayShownFor.clear();
        refreshGroupControls();
        mGroupsList->setSelectedByValue(new_name, true);
    }
}

void LLFloaterDirector::onClickGroupDissolve()
{
    const std::string name = selectedManagedGroup();
    if (name.empty())
    {
        return;
    }
    // untag only: every member stays in the cast; the delay goes with the tag
    LLDirectorCast::instance().dissolveGroup(name);
    mLastGroupNames.assign(1, std::string());
    mGroupShownFor.setNull();
    mGroupsListSnapshot.clear();
    mGroupDelayShownFor.clear();
}

void LLFloaterDirector::refreshGroupControls()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director refreshGroupControls"); // [DirFix] D9
    if (!mGroupAssignCombo || !mCastList)
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();

    // rebuild both combos only when the distinct-name set changed (rare)
    const std::vector<std::string> names = cast.getGroupNames();
    if (names != mLastGroupNames)
    {
        mLastGroupNames = names;
        const std::string run_sel = mGroupRunCombo ? mGroupRunCombo->getSelectedItemLabel() : std::string();
        mGroupAssignCombo->clearRows();
        if (mGroupRunCombo)
        {
            mGroupRunCombo->clearRows();
        }
        // "(none)" first: the discoverable way to clear one member's tag
        // (commits as the empty name; typing nothing still works too)
        mGroupAssignCombo->add(GROUP_NONE_LABEL);
        for (const std::string& name : names)
        {
            mGroupAssignCombo->add(name);
            if (mGroupRunCombo)
            {
                mGroupRunCombo->add(name);
            }
        }
        // keep the run combo on the group the operator had picked, when it
        // still exists; otherwise fall back to the prompt label
        if (mGroupRunCombo && (run_sel.empty() || !mGroupRunCombo->setSimple(LLStringExplicit(run_sel))))
        {
            mGroupRunCombo->setLabel(LLStringExplicit("Group"));
        }
    }

    // mirror the first selected member's tag into the assign box -- but never
    // over the operator's in-progress typing (focus covers the embedded editor)
    const LLUUID sel = firstSelectedCastId();
    if (sel != mGroupShownFor && !mGroupAssignCombo->hasFocus())
    {
        mGroupShownFor = sel;
        mGroupAssignCombo->setTextEntry(LLStringExplicit(cast.getGroup(sel)));
    }
    const bool have_sel = sel.notNull();
    setEnabledIfChanged(mGroupAssignCombo, have_sel);
    setToolTipIfChanged(mGroupAssignCombo, have_sel
        ? std::string("Group tag for the selected member(s): type a new name or pick an existing one, Enter commits. Empty ungroups")
        : std::string("Select a cast member first"));

    if (!mGroupsList || !mGroupRunCombo || !mGroupStartBtn || !mGroupStopBtn ||
        !mGroupDelaySpinner || !mGroupRenameEditor || !mGroupRenameBtn || !mGroupDissolveBtn)
    {
        return;
    }

    if (!mGroupsList->isInVisibleChain()) return;
    if (mGroupsTabDirty)
    {
        mGroupsTabDirty = false;
        mGroupsListSnapshot.clear();
        mGroupDelayShownFor.clear();
    }

    // group transport: Start/Stop need a picked group that still has members
    const std::string run_sel = mGroupRunCombo->getSelectedItemLabel();
    const bool have_group = !run_sel.empty()
                         && !cast.membersInGroup(run_sel).empty();
    setEnabledIfChanged(mGroupRunCombo, !names.empty());
    setToolTipIfChanged(mGroupRunCombo, names.empty()
        ? std::string("No groups yet: tag cast members via the Group box under the cast list")
        : std::string("The group Start/Stop act on"));
    setEnabledIfChanged(mGroupStartBtn, have_group);
    setEnabledIfChanged(mGroupStopBtn, have_group);
    const std::string need_group_tip = names.empty()
        ? std::string("No groups yet: tag cast members via the Group box under the cast list")
        : std::string("Pick a group first");
    const F32 run_delay = have_group ? cast.getGroupDelay(run_sel) : 0.f;
    setToolTipIfChanged(mGroupStartBtn, have_group
        ? (run_delay > 0.01f
            ? llformat("Start every member of '%s' after its %.1f s delay (staggered)", run_sel.c_str(), run_delay)
            : "Start a move for every member of '" + run_sel + "' (same as pressing Walk on each)")
        : need_group_tip);
    setToolTipIfChanged(mGroupStopBtn, have_group
        ? "Stop every member of '" + run_sel + "' (also disarms queued staggered starts)"
        : need_group_tip);

    // ---- Groups management section (Move tab) ----
    // rebuild the list only when its composed snapshot (name, member count,
    // delay) changed; keyed rows keep the selection across rebuilds
    std::vector<std::string> snapshot;
    snapshot.reserve(names.size());
    for (const std::string& name : names)
    {
        snapshot.push_back(llformat("%s|%d|%.1f", name.c_str(),
                                    (S32)cast.membersInGroup(name).size(),
                                    cast.getGroupDelay(name)));
    }
    if (snapshot != mGroupsListSnapshot)
    {
        mGroupsListSnapshot = snapshot;
        const std::string prev_sel = selectedManagedGroup();
        mGroupsList->deleteAllItems();
        for (const std::string& name : names)
        {
            const F32 d = cast.getGroupDelay(name);
            LLSD row;
            row["value"] = name;
            row["columns"][0]["column"] = "group";
            row["columns"][0]["value"] = name;
            row["columns"][1]["column"] = "members";
            row["columns"][1]["value"] = llformat("%d", (S32)cast.membersInGroup(name).size());
            row["columns"][2]["column"] = "delay";
            row["columns"][2]["value"] = d > 0.01f ? llformat("%.1f s", d)
                                                   : std::string("\xE2\x80\x94");   // em dash
            mGroupsList->addElement(row, ADD_BOTTOM);
        }
        if (!prev_sel.empty())
        {
            mGroupsList->setSelectedByValue(prev_sel, true);
        }
    }

    // spinner mirrors the selected group's delay -- but never over the
    // operator's in-progress edit (focus check, same rule as the assign combo)
    const std::string managed = selectedManagedGroup();
    const bool have_managed = !managed.empty();
    if (managed != mGroupDelayShownFor && !mGroupDelaySpinner->hasFocus())
    {
        mGroupDelayShownFor = managed;
        mGroupDelaySpinner->setValue(have_managed ? cast.getGroupDelay(managed) : 0.f);
    }
    setEnabledIfChanged(mGroupDelaySpinner, have_managed);
    setEnabledIfChanged(mGroupRenameEditor, have_managed);
    setEnabledIfChanged(mGroupRenameBtn, have_managed);
    setEnabledIfChanged(mGroupDissolveBtn, have_managed);
    setToolTipIfChanged(mGroupDelaySpinner, have_managed
        ? llformat("Start delay for '%s' (s): ACTION and Start group launch its members this many seconds late, so groups can enter in waves. 0 = immediate. Saved with the scene", managed.c_str())
        : std::string("Select a group in the list first"));
    setToolTipIfChanged(mGroupDissolveBtn, have_managed
        ? llformat("Dissolve '%s': every member is untagged (nobody leaves the cast); its delay is dropped", managed.c_str())
        : std::string("Select a group in the list first"));
    setToolTipIfChanged(mGroupRenameBtn, have_managed
        ? llformat("Rename '%s' to the name on the left (retags every member; renaming onto an existing group merges them)", managed.c_str())
        : std::string("Select a group in the list first"));
}

// ---------------------------------------------------------------------------
// Move tab: the shared transport panel, targeting the cast selection
// ---------------------------------------------------------------------------
void LLFloaterDirector::refreshMoveTab(const uuid_vec_t& selected)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director refreshMoveTab"); // [DirFix] D9
    // the shared ALPanelActorMover owns the scope radio, heading dial, params,
    // and Walk/Stop; the console's only job is to hand it the current cast
    // selection each draw. Walk/Stop honor ActorMoverSync internally -- Everyone
    // drives startAll/stopAll, Selected acts on exactly this set, so the buttons
    // behave identically to the standalone floater (which feeds its roster
    // selection the same way).
    if (mMoverPanel)
    {
        mMoverPanel->setSelectedActors(actorTargetsForPage("cast_group/move_tab", selected));
    }
}

// ---------------------------------------------------------------------------
// Path tab: the shared waypoint editor, targeting the cast selection
// ---------------------------------------------------------------------------
void LLFloaterDirector::refreshPathTab(const uuid_vec_t& selected)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director refreshPathTab"); // [DirFix] D9
    // the waypoint editor now owns its own tab (always visible); keep it pointed
    // at the console's cast selection every draw (cheap; the panel diffs its
    // own state internally)
    if (mPathPanel)
    {
        // [DirFix] D10: front() == firstSelectedCastId() (both walk the list in order)
        const uuid_vec_t& targets = actorTargetsForPage("cast_group/path_tab", selected);
        mPathPanel->setTargetActor(targets.empty() ? LLUUID::null : targets.front());
    }
}

// ---------------------------------------------------------------------------
// Actor FX tab: per-actor live-render styling
// ---------------------------------------------------------------------------
bool LLFloaterDirector::hasActorStyleTarget() const
{
    return mActorStyleTarget && mActorStyleTarget->getCurrentIndex() >= 0;
}

LLUUID LLFloaterDirector::actorStyleTargetId() const
{
    return hasActorStyleTarget()
        ? mActorStyleTarget->getSelectedValue().asUUID()
        : LLUUID::null;
}

void LLFloaterDirector::onActorStyleTargetChanged()
{
    if (mRefreshingActorStyle)
        return;
    // Null is both a legitimate target (You) and LLUUID's empty value, so the
    // separate mActorStyleHadTarget flag distinguishes it from no combo row.
    mActorStyleHadTarget = false;
    mActorStyleSnapshot.clear();
    refreshActorStyleTab();
}

void LLFloaterDirector::onActorStyleControlChanged()
{
    if (mRefreshingActorStyle || !hasActorStyleTarget())
    {
        return;
    }

    LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID id = actorStyleTargetId();
    // Start from the model record so future fields this UI does not know about
    // survive an edit to one of today's controls.
    LLDirectorCast::ActorStyle style = cast.getActorStyle(id);
    style.mEnabled = mActorStyleEnabled->get();
    style.mMode = static_cast<LLDirectorCast::EActorStyleMode>(llclamp(
        mActorStyleMode->getValue().asInteger(),
        static_cast<S32>(LLDirectorCast::ACTOR_STYLE_LAYER),
        static_cast<S32>(LLDirectorCast::ACTOR_STYLE_REPLACE)));
    style.mStyle = llclamp(
        mActorStyleLook->getValue().asInteger(),
        static_cast<S32>(LLDirectorCast::ACTOR_LOOK_DEFAULT),
        static_cast<S32>(LLDirectorCast::ACTOR_LOOK_MAX));
    style.mUseActorHue = mActorStyleUseActorHue->get();
    style.mHue = (F32)mActorStyleHue->getValue().asReal();
    style.mAlpha = (F32)mActorStyleAlpha->getValue().asReal();
    style.mDissolveProgress =
        (F32)mActorStyleDissolveProgress->getValue().asReal();
    style.mPixelSize = (F32)mActorStylePixel->getValue().asReal();
    style.mShimmerSpeed = (F32)mActorStyleShimmerSpeed->getValue().asReal();
    style.mShimmerAmount = (F32)mActorStyleShimmerAmount->getValue().asReal();
    style.mGlitch = (F32)mActorStyleGlitch->getValue().asReal();
    style.mDistortion = llclamp(
        mActorStyleDistortion->getValue().asInteger(), 0, 8);
    style.mDistortionAmount =
        (F32)mActorStyleDistortionAmount->getValue().asReal();
    style.mBrightness = (F32)mActorStyleBrightness->getValue().asReal();
    style.mEffectFps = (F32)mActorStyleEffectFps->getValue().asReal();
    cast.setActorStyle(id, style);

    // Refresh dependent enablement (master, actor hue, distortion amount) on
    // the next draw without hammering values during a slider drag.
    mActorStyleSnapshot.clear();
}

void LLFloaterDirector::onActorStyleReset()
{
    if (!hasActorStyleTarget())
    {
        return;
    }
    const LLUUID id = actorStyleTargetId();
    const LLDirectorCast::ActorStyle::SteppedOverride stepped =
        LLDirectorCast::instance().getActorStyle(id).mStepped;
    LLDirectorCast::ActorStyle reset;
    reset.mStepped = stepped;
    LLDirectorCast::instance().setActorStyle(id, reset);
    mActorStyleSnapshot.clear();
}

void LLFloaterDirector::refreshActorStyleTab()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director refreshActorStyleTab"); // [DirFix] D9
    if (!mActorStyleTarget)
    {
        return;
    }

    // [DirFix] D8: a pure-UI mirror of LLDirectorCast with no model writes and no readers of
    // its caches while hidden (the tab's own controls are the only entry points), so skip
    // it while the Actor FX tab is not in the visible chain. onTabChanged / onOpen set
    // mActorStyleTabDirty; the first visible pass drops every "last written" cache so the
    // reveal frame rebuilds the roster and rewrites all controls (force) before paint.
    if (!mActorStyleTarget->isInVisibleChain())
    {
        return;
    }
    const bool reveal = mActorStyleTabDirty;
    if (reveal)
    {
        mActorStyleTabDirty = false;
        mActorStyleRosterBuilt = false;     // Phase A flag: next pass rebuilds the roster
        mActorStyleRoster.clear();
        mActorStyleHadTarget = false;       // next pass rewrites every control
    }

    LLDirectorCast& cast = LLDirectorCast::instance();
    const bool have_self = isAgentAvatarValid();
    // De-duplicate against the stable agent id even during a transient avatar
    // rebuild. If self is momentarily invalid, do not expose the same body as a
    // CastMember record whose style would disappear when null-id You returns.
    const LLUUID self_id = gAgent.getID();

    // Rebuild only when membership, display names, or world availability
    // changes. The explicit You row maps to null; skip the agent's real UUID
    // when it is also enrolled in Cast so the UI cannot offer two controls for
    // one rendered body.
    //
    // [DirFix] D4: compared element-wise against the stored roster (same fields the old
    // signature string encoded: self flag/id, member id, mLastName, in-world) -- no
    // per-frame allocation. cast.resolve() is still called for every member each pass
    // (it refreshes the cached name), exactly as the signature loop did.
    bool roster_changed = !mActorStyleRosterBuilt
        || have_self != mActorStyleRosterHadSelf
        || (have_self && self_id != mActorStyleRosterSelfId)
        || mActorStyleChoiceRevision != cast.getActorChoiceRevision();
    size_t roster_count = 0;
    for (const LLDirectorCast::CastMember& member : cast.getCast())
    {
        if (self_id.notNull() && member.mId == self_id)
        {
            continue;
        }
        const bool in_world = cast.resolve(member.mId) != nullptr;
        if (!roster_changed)
        {
            if (roster_count >= mActorStyleRoster.size())
            {
                roster_changed = true;
            }
            else
            {
                const ActorStyleRosterRow& row = mActorStyleRoster[roster_count];
                roster_changed = row.mId != member.mId
                              || row.mInWorld != in_world
                              || row.mName != member.mLastName;
            }
        }
        ++roster_count;
    }
    if (!roster_changed && roster_count != mActorStyleRoster.size())
    {
        roster_changed = true;
    }

    if (roster_changed)
    {
        mActorStyleRosterBuilt = true;
        mActorStyleRosterHadSelf = have_self;
        mActorStyleRosterSelfId = self_id;
        mActorStyleChoiceRevision = cast.getActorChoiceRevision();
        mActorStyleRoster.clear();
        const bool had_previous = mActorStyleTarget->getCurrentIndex() >= 0;
        const LLSD previous = mActorStyleTarget->getSelectedValue();

        mRefreshingActorStyle = true;
        mActorStyleTarget->clearRows();
        for (const LLDirectorCast::ActorChoice& choice : cast.getActorChoices())
        {
            if (LLDirectorCast::isSelf(choice.mId))
            {
                if (have_self)
                {
                    mActorStyleTarget->add(choice.mLabel, LLSD(LLUUID::null));
                }
                continue;
            }
            const bool in_world = cast.resolve(choice.mId) != nullptr;
            const LLDirectorCast::CastMember* member = cast.getMember(choice.mId);
            mActorStyleRoster.push_back({ choice.mId,
                member ? member->mLastName : choice.mLabel, in_world }); // [DirFix] D4
            std::string label = choice.mLabel;
            if (!in_world)
            {
                label += " (away)";
            }
            mActorStyleTarget->add(label, LLSD(choice.mId));
        }
        if (!had_previous ||
            !mActorStyleTarget->setSelectedByValue(previous, true))
        {
            mActorStyleTarget->selectFirstItem();
        }
        mRefreshingActorStyle = false;
        mActorStyleHadTarget = false;
        mActorStyleSnapshot.clear();
    }

    if (mShell)
    {
        ALKitPage* page = mShell->currentKitPage();
        if (page && page->pageKey() == "look_group/actor_style_tab")
        {
            const bool pinned = page->whoPinned();
            const LLUUID cast_selection = firstSelectedCastId();
            const bool selection_changed = !mActorStyleHaveLastCastSelection ||
                cast_selection != mActorStyleLastCastSelection;
            if (!pinned && selection_changed)
            {
                mActorStyleHaveLastCastSelection = true;
                mActorStyleLastCastSelection = cast_selection;
                mRefreshingActorStyle = true;
                if (mActorStyleTarget->setSelectedByValue(LLSD(cast_selection), true))
                {
                    mActorStyleHadTarget = false;
                    mActorStyleSnapshot.clear();
                }
                mRefreshingActorStyle = false;
            }
            else if (pinned &&
                     mActorStyleTarget->getSelectedValue().asUUID() != page->actorTarget())
            {
                mRefreshingActorStyle = true;
                if (mActorStyleTarget->setSelectedByValue(LLSD(page->actorTarget()), true))
                {
                    mActorStyleHadTarget = false;
                    mActorStyleSnapshot.clear();
                }
                mRefreshingActorStyle = false;
            }
        }
    }

    const bool have_target = hasActorStyleTarget();
    // [DirFix] D4: pure early-out helpers (same end state, no widget touch when unchanged)
    setEnabledIfChanged(mActorStyleTarget, mActorStyleTarget->getItemCount() > 0);
    if (!have_target)
    {
        setEnabledIfChanged(mActorStyleEnabled, false);
        setEnabledIfChanged(mActorStyleMode, false);
        setEnabledIfChanged(mActorStyleLook, false);
        setEnabledIfChanged(mActorStyleUseActorHue, false);
        setEnabledIfChanged(mActorStyleHue, false);
        setEnabledIfChanged(mActorStyleAlpha, false);
        setEnabledIfChanged(mActorStyleDissolveProgress, false);
        mActorStyleDissolveProgress->setVisible(false);
        setEnabledIfChanged(mActorStylePixel, false);
        setEnabledIfChanged(mActorStyleShimmerSpeed, false);
        setEnabledIfChanged(mActorStyleShimmerAmount, false);
        setEnabledIfChanged(mActorStyleGlitch, false);
        setEnabledIfChanged(mActorStyleDistortion, false);
        setEnabledIfChanged(mActorStyleDistortionAmount, false);
        setEnabledIfChanged(mActorStyleBrightness, false);
        setEnabledIfChanged(mActorStyleEffectFps, false);
        setEnabledIfChanged(mActorStyleReset, false);
        setTextIfChanged(mActorStyleStatus, std::string(
            "You is unavailable and the Director cast is empty."));
        mActorStyleHadTarget = false;
        mActorStyleSnapshot.clear();
        return;
    }

    const LLUUID id = actorStyleTargetId();
    const LLDirectorCast::ActorStyle& style = cast.getActorStyle(id);
    const std::string snapshot = llformat(
        "%d|%d|%d|%d|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f|%d|%.4f|%.4f|%.4f",
        style.mEnabled ? 1 : 0, static_cast<S32>(style.mMode), style.mStyle,
        style.mUseActorHue ? 1 : 0, style.mHue, style.mAlpha,
        style.mDissolveProgress, style.mPixelSize, style.mShimmerSpeed,
        style.mShimmerAmount,
        style.mGlitch, style.mDistortion, style.mDistortionAmount,
        style.mBrightness, style.mEffectFps);

    if (!mActorStyleHadTarget || id != mActorStyleShownFor ||
        snapshot != mActorStyleSnapshot)
    {
        // [DirFix] D4: widget-exact writes. `force` (actor identity changed) bypasses the
        // focus guard; otherwise a control the user is dragging/typing into is left alone
        // (V12) and the snapshot is NOT advanced, so the next frame retries. Check boxes
        // always force (their "focus" after a click is meaningless, aluirefresh.h).
        // [DirFix] D8: on the reveal pass force is also true (`reveal` = mActorStyleTabDirty as
        // it was at entry; the flag itself is consumed above so the reset runs only once).
        const bool force = (id != mActorStyleShownFor) || reveal;
        bool skipped = false;
        auto write = [&](auto* w, const LLSD& v, bool always_force)
        {
            const bool f = force || always_force;
            if (!f && alIsBeingEdited(w) && !alValueMatches(w, w->getValue(), v))
            {
                skipped = true;     // a focus-guarded write that was actually needed
                return;
            }
            setValueIfChanged(w, v, f);
        };
        mRefreshingActorStyle = true;
        write(mActorStyleEnabled, LLSD(style.mEnabled), true);
        write(mActorStyleMode, LLSD(static_cast<S32>(style.mMode)), false);
        write(mActorStyleLook, LLSD(style.mStyle), false);
        write(mActorStyleUseActorHue, LLSD(style.mUseActorHue), true);
        write(mActorStyleHue, LLSD(style.mHue), false);
        write(mActorStyleAlpha, LLSD(style.mAlpha), false);
        write(mActorStyleDissolveProgress, LLSD(style.mDissolveProgress), false);
        write(mActorStylePixel, LLSD(style.mPixelSize), false);
        write(mActorStyleShimmerSpeed, LLSD(style.mShimmerSpeed), false);
        write(mActorStyleShimmerAmount, LLSD(style.mShimmerAmount), false);
        write(mActorStyleGlitch, LLSD(style.mGlitch), false);
        write(mActorStyleDistortion, LLSD(style.mDistortion), false);
        write(mActorStyleDistortionAmount, LLSD(style.mDistortionAmount), false);
        write(mActorStyleBrightness, LLSD(style.mBrightness), false);
        write(mActorStyleEffectFps, LLSD(style.mEffectFps), false);
        mRefreshingActorStyle = false;
        if (!skipped)
        {
            mActorStyleShownFor = id;
            mActorStyleHadTarget = true;
            mActorStyleSnapshot = snapshot;
        }
    }

    setEnabledIfChanged(mActorStyleEnabled, true);
    const bool active = style.mEnabled;
    setEnabledIfChanged(mActorStyleMode, active);
    setEnabledIfChanged(mActorStyleLook, active);
    setEnabledIfChanged(mActorStyleUseActorHue, active);
    setEnabledIfChanged(mActorStyleHue, active && !style.mUseActorHue);
    // Effect opacity belongs to the shared replay in both modes. Layer
    // composites it over authored beauty; Cover uses it for the replacement
    // itself (including the intentional fully-invisible zero endpoint).
    setEnabledIfChanged(mActorStyleAlpha, active);
    const bool dissolve = style.mStyle == 10;
    mActorStyleDissolveProgress->setVisible(dissolve);
    setEnabledIfChanged(mActorStyleDissolveProgress, active && dissolve);
    setEnabledIfChanged(mActorStylePixel, active);
    setEnabledIfChanged(mActorStyleShimmerSpeed, active);
    setEnabledIfChanged(mActorStyleShimmerAmount, active);
    setEnabledIfChanged(mActorStyleGlitch, active);
    setEnabledIfChanged(mActorStyleDistortion, active);
    setEnabledIfChanged(mActorStyleDistortionAmount,
        active && style.mDistortion != 0);
    setEnabledIfChanged(mActorStyleBrightness, active);
    setEnabledIfChanged(mActorStyleEffectFps, active);
    setEnabledIfChanged(mActorStyleReset, true);

    std::string who = id.isNull() ? std::string("You")
                                  : castMemberName(id);
    if (id.notNull() && cast.resolve(id) == nullptr)
    {
        who += " (away)";
    }
    const char* mode = style.mMode == LLDirectorCast::ACTOR_STYLE_REPLACE
        ? "Opaque cover preview"
        : "Layer";
    setTextIfChanged(mActorStyleStatus,
        style.mEnabled ? who + " \xC2\xB7 " + mode + " style active"
                       : who + " \xC2\xB7 styling off");     // [DirFix] D4
}

// ---------------------------------------------------------------------------
// Animate tab: live signaled-animation list for the selected cast member
// ---------------------------------------------------------------------------
LLUUID LLFloaterDirector::selectedAnimId() const
{
    LLScrollListItem* item = mAnimList->getFirstSelected();
    return item ? item->getValue().asUUID() : LLUUID::null;
}

LLUUID LLFloaterDirector::pasteAnimId() const
{
    std::string text = mPasteEditor->getText();
    LLStringUtil::trim(text);
    if (!LLUUID::validate(text))
    {
        return LLUUID::null;
    }
    return LLUUID(text);    // an all-zero uuid also comes back null-equivalent
}

void LLFloaterDirector::onAnimRightClick(LLUICtrl* ctrl, S32 x, S32 y)
{
    LLScrollListItem* item = mAnimList->hitItem(x, y);
    LLContextMenu* menu = mAnimMenuHandle.get();
    if (item && menu)
    {
        if (!item->getSelected())
        {
            const S32 index = mAnimList->getItemIndex(item);
            if (index >= 0)
            {
                mAnimList->selectNthItem(index);
            }
        }
        menu->buildDrawLabels();
        menu->updateParent(LLMenuGL::sMenuContainer);
        menu->show(x, y);
        LLMenuGL::showPopup(ctrl, menu, x, y);
    }
}

void LLFloaterDirector::onAnimCopyUUID()
{
    const LLUUID anim = selectedAnimId();
    if (anim.isNull())
    {
        return;
    }
    // same LLClipboard idiom as the Animation Explorer's Copy UUID
    LLWString idwstr = utf8string_to_wstring(anim.asString());
    LLClipboard::instance().copyToClipboard(idwstr, 0, narrow(idwstr.size()));
}

S32 LLFloaterDirector::selectedLocomotionRole() const
{
    const S32 role = mLocomotionRoleCombo
        ? mLocomotionRoleCombo->getValue().asInteger()
        : LLDirectorCast::LOCO_WALK_FORWARD;
    switch (role)
    {
        case LLDirectorCast::LOCO_IDLE:
        case LLDirectorCast::LOCO_WALK_FORWARD:
        case LLDirectorCast::LOCO_RUN_FORWARD:
        case LLDirectorCast::LOCO_FLY:
        case LLDirectorCast::LOCO_HOVER:
            return role;
        default:
            return LLDirectorCast::LOCO_WALK_FORWARD;
    }
}

void LLFloaterDirector::applySelectedLocomotion(const LLUUID& anim)
{
    LLDirectorCast::CastMember* m =
        LLDirectorCast::instance().getMember(firstSelectedCastId());
    if (anim.isNull() || !m)
    {
        return;
    }

    const S32 role = selectedLocomotionRole();
    auto& entry = m->mLocomotionSet.edit(
        static_cast<LLDirectorCast::ELocomotionRole>(role));
    entry.mAnim = anim;
    entry.mNominalSpeed = llclamp(
        (F32)mLocomotionSpeed->getValue().asReal(), 0.05f, 20.f);
    entry.mManualLeftPlantPhase = llclamp(
        (F32)mLocomotionPhase->getValue().asReal(), -1.f, 1.f);
    entry.mAllowAO = mLocomotionAllowAO->get();
    if (role == LLDirectorCast::LOCO_WALK_FORWARD)
    {
        m->mLocoAnim = anim;
    }
    LLDirectorCast::instance().touchScene();
    mLocomotionEditorLoaded = false;
}

void LLFloaterDirector::onAnimSetLoco()
{
    applySelectedLocomotion(selectedAnimId());
}

// Apply the client-side play-local priority (DirectorAnimPlayLocalPriority) to a
// just-started local anim. -1 = baked (no override, byte-identical); 0..4 force
// LOW..HIGHEST. Per-instance (see LLMotion::setPriorityOverride) so other avatars
// are unaffected; cleared automatically when the anim stops.
static void apply_play_local_priority(LLVOAvatar* av, const LLUUID& anim)
{
    if (!av || anim.isNull())
    {
        return;
    }
    static LLCachedControl<S32> prio(gSavedSettings, "DirectorAnimPlayLocalPriority", -1);
    if (prio < 0)
    {
        return;
    }
    if (LLMotion* m = av->findMotion(anim))
    {
        m->setPriorityOverride(prio);
    }
}

void LLFloaterDirector::onAnimPlayLocal(bool play)
{
    const LLUUID anim = selectedAnimId();
    LLVOAvatar* av = LLDirectorCast::instance().resolve(firstSelectedCastId());
    if (anim.isNull() || !av)
    {
        return;
    }
    // client-side only: nothing is sent to the sim
    play ? (void)av->startMotion(anim) : (void)av->stopMotion(anim);
    if (play)
    {
        apply_play_local_priority(av, anim);
    }
}

void LLFloaterDirector::onPastePlayLocal(bool play)
{
    const LLUUID anim = pasteAnimId();
    LLVOAvatar* av = LLDirectorCast::instance().resolve(firstSelectedCastId());
    if (anim.isNull() || !av)
    {
        return;
    }
    play ? (void)av->startMotion(anim) : (void)av->stopMotion(anim);
    if (play)
    {
        apply_play_local_priority(av, anim);
    }
}

void LLFloaterDirector::onPasteSetLoco()
{
    applySelectedLocomotion(pasteAnimId());
}

void LLFloaterDirector::onClickClearLoco()
{
    if (LLDirectorCast::CastMember* m =
            LLDirectorCast::instance().getMember(firstSelectedCastId()))
    {
        const S32 role = selectedLocomotionRole();
        m->mLocomotionSet.mRoles.erase(role);
        if (role == LLDirectorCast::LOCO_WALK_FORWARD)
        {
            m->mLocoAnim.setNull();
        }
        LLDirectorCast::instance().touchScene();
        mLocomotionEditorLoaded = false;
    }
}

void LLFloaterDirector::onLocomotionRoleChanged()
{
    mLocomotionEditorLoaded = false;
    refreshLocomotionRoleEditor(firstSelectedCastId());
}

void LLFloaterDirector::onLocomotionMetadataCommit()
{
    LLDirectorCast::CastMember* member =
        LLDirectorCast::instance().getMember(firstSelectedCastId());
    if (!member)
    {
        return;
    }
    const S32 role = selectedLocomotionRole();
    auto& entry = member->mLocomotionSet.mRoles[role];
    entry.mNominalSpeed = llclamp(
        (F32)mLocomotionSpeed->getValue().asReal(), 0.05f, 20.f);
    entry.mManualLeftPlantPhase = llclamp(
        (F32)mLocomotionPhase->getValue().asReal(), -1.f, 1.f);
    entry.mAllowAO = mLocomotionAllowAO->get();
    LLDirectorCast::instance().touchScene();
    mLocomotionEditorLoaded = false;
}

void LLFloaterDirector::refreshLocomotionRoleEditor(const LLUUID& member_id)
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    const LLDirectorCast::CastMember* member = cast.getMember(member_id);
    const S32 role = selectedLocomotionRole();
    const LLDirectorCast::LocomotionRoleEntry* entry = member
        ? member->mLocomotionSet.get(
              static_cast<LLDirectorCast::ELocomotionRole>(role))
        : nullptr;
    const LLUUID anim = entry ? entry->mAnim : LLUUID::null;
    const bool configured = entry && anim.notNull();
    const F32 speed = entry ? entry->mNominalSpeed
                            : default_locomotion_speed(role);
    const F32 phase = entry ? entry->mManualLeftPlantPhase : -1.f;
    const bool allow_ao = entry && entry->mAllowAO;
    const bool changed = !mLocomotionEditorLoaded ||
        member_id != mLocomotionEditorMember ||
        role != mLocomotionEditorRole || anim != mLocomotionEditorAnim ||
        speed != mLocomotionEditorSpeed || phase != mLocomotionEditorPhase ||
        allow_ao != mLocomotionEditorAllowAO;
    if (changed)
    {
        mLocomotionSpeed->setValue(speed);
        mLocomotionPhase->setValue(phase);
        mLocomotionAllowAO->set(allow_ao);
        mLocomotionEditorMember = member_id;
        mLocomotionEditorRole = role;
        mLocomotionEditorAnim = anim;
        mLocomotionEditorSpeed = speed;
        mLocomotionEditorPhase = phase;
        mLocomotionEditorAllowAO = allow_ao;
        mLocomotionEditorLoaded = true;
    }

    const bool have_member = member != nullptr;
    mLocomotionRoleCombo->setEnabled(have_member);
    mLocomotionSpeed->setEnabled(have_member);
    mLocomotionPhase->setEnabled(have_member);
    mLocomotionAllowAO->setEnabled(have_member);

    std::string status = "Select a cast member";
    if (member && !configured)
    {
        status = std::string(locomotion_role_label(role)) +
            (entry ? ": fallback; metadata draft" : ": fallback");
    }
    else if (configured)
    {
        status = std::string(locomotion_role_label(role)) +
            (cast.resolve(member_id) ? ": not loaded; starts on ACTION"
                                     : ": configured; actor away");
        if (LLVOAvatar* av = cast.resolve(member_id))
        {
            if (LLKeyframeMotion* motion = dynamic_cast<LLKeyframeMotion*>(
                    av->findMotion(entry->mAnim)))
            {
                if (!motion->isLoaded())
                {
                    status = std::string(locomotion_role_label(role)) +
                        ": loading";
                }
                else if (entry->mManualLeftPlantPhase >= 0.f)
                {
                    status = std::string(locomotion_role_label(role)) +
                        ": manual phase";
                }
                else
                {
                    const auto& info = motion->getGenericLocomotionPhaseInfo();
                    switch (info.mStatus)
                    {
                        case LLKeyframeMotion::LocomotionPhaseInfo::VALID:
                            status = llformat("%s: auto phase %.0f%%",
                                locomotion_role_label(role),
                                info.mConfidence * 100.f);
                            break;
                        case LLKeyframeMotion::LocomotionPhaseInfo::MISSING_ANKLES:
                            status = std::string(locomotion_role_label(role)) +
                                ": no ankle curves; loop-in fallback";
                            break;
                        case LLKeyframeMotion::LocomotionPhaseInfo::NO_ALTERNATING_PLANTS:
                            status = std::string(locomotion_role_label(role)) +
                                ": phase unclear; loop-in fallback";
                            break;
                        default:
                            status = std::string(locomotion_role_label(role)) +
                                ": non-looping; loop-in fallback";
                            break;
                    }
                }
            }
        }
    }
    if (mLocoText->getText() != status)
    {
        mLocoText->setText(status);
    }
}

void LLFloaterDirector::refreshAnimateTab(const uuid_vec_t& selected)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director refreshAnimateTab"); // [DirFix] D9
    // [DirFix] D8: a pure-UI mirror (rows, hint, tooltips, locomotion editor, the embedded
    // preview feed): nothing reads its caches while the tab is hidden (the click handlers read
    // the widgets directly) and the preview panel does not draw hidden. Skip it, and on the
    // first visible pass after onTabChanged / onOpen drop every "last written" cache so the
    // reveal frame is fully re-synced before paint.
    if (!mAnimList || !mAnimList->isInVisibleChain())
    {
        return;
    }
    const bool reveal = mAnimateTabDirty;
    if (reveal)
    {
        mAnimateTabDirty = false;
        mAnimAvatarId.setNull();
        mAnimSnapshot.clear();
        mLocomotionEditorLoaded = false;
    }

    LLDirectorCast& cast = LLDirectorCast::instance();
    const uuid_vec_t& animate_targets = actorTargetsForPage("performance_group/animate_tab", selected);
    const LLUUID sel = animate_targets.empty() ? LLUUID::null : animate_targets.front();
    LLVOAvatar* av = sel.notNull() ? cast.resolve(sel) : nullptr;
    const std::string member_name = sel.notNull() ? castMemberName(sel)
                                                  : std::string();

    // header: who the tab is showing
    setTextIfChanged(mAnimateHeader, sel.isNull()   // [UIPerfC1]
        ? std::string("No cast member selected")
        : av ? member_name
             : member_name + " (away)");

    // snapshot the signaled set (std::map iteration = stable id order)
    std::vector<std::pair<LLUUID, S32>> snap;
    if (av)
    {
        snap.assign(av->mSignaledAnimations.begin(), av->mSignaledAnimations.end());
    }

    // membership change -> rebuild rows (rare); otherwise only re-set cells.
    // [DirFix] D8: `reveal` forces the rebuild too: with nothing selected the nulled caches
    // above equal the "no member" state, so the rows left over from before the tab was
    // hidden would otherwise survive.
    if (reveal || sel != mAnimAvatarId || snap != mAnimSnapshot)
    {
        const LLUUID prev_sel = selectedAnimId();
        mAnimAvatarId = sel;
        mAnimSnapshot = snap;
        mAnimList->deleteAllItems();
        mAnimRowStates.clear();
        mAnimRowStates.resize(snap.size());
        for (const auto& entry : snap)
        {
            const LLUUID& anim_id = entry.first;
            LLSD row;
            row["value"] = anim_id;
            row["columns"][0]["column"] = "anim_id";
            row["columns"][0]["value"] = anim_id.asString();
            row["columns"][0]["font"] = "Monospace";
            row["columns"][1]["column"] = "prio";
            row["columns"][1]["value"] = "?";
            row["columns"][2]["column"] = "playing";
            row["columns"][2]["value"] = "";
            mAnimList->addElement(row, ADD_BOTTOM);
        }
        if (prev_sel.notNull())
        {
            mAnimList->selectByID(prev_sel);
        }
    }

    // per-row priority / playing marker, diffed (draw()-rate friendly)
    if (av)
    {
        const S32 prio_col = mAnimPrioCol;          // [DirFix] D7 cached in postBuild
        const S32 playing_col = mAnimPlayingCol;
        std::vector<LLScrollListItem*> items = mAnimList->getAllData();
        for (size_t i = 0; i < items.size() && i < mAnimRowStates.size(); ++i)
        {
            LLScrollListItem* item = items[i];
            AnimRowState& state = mAnimRowStates[i];
            const LLUUID anim_id = item->getValue().asUUID();

            // priority when resolvable via findMotion (AnimationExplorer idiom)
            std::string prio = "?";
            if (auto* motion = dynamic_cast<LLKeyframeMotion*>(av->findMotion(anim_id)))
            {
                prio = llformat("%d", (S32)motion->getPriority());
            }
            const std::string playing =
                av->mPlayingAnimations.find(anim_id) != av->mPlayingAnimations.end()
                    ? GLYPH_PLAYING : "";

            if (state.mPrio != prio)
            {
                if (auto* cell = dynamic_cast<LLScrollListText*>(item->getColumn(prio_col)))
                {
                    cell->setText(prio);
                }
                state.mPrio = prio;
            }
            if (state.mPlaying != playing)
            {
                if (auto* cell = dynamic_cast<LLScrollListText*>(item->getColumn(playing_col)))
                {
                    cell->setText(playing);
                }
                state.mPlaying = playing;
            }
        }
    }

    // empty-state hint over the list
    std::string hint;
    if (sel.isNull())
    {
        hint = "Select a cast member to see the animations playing on them";
    }
    else if (!av)
    {
        hint = member_name + " is not in world right now";
    }
    else if (snap.empty())
    {
        hint = "No signaled animations on " + member_name;
    }
    // [DirFix] D5: setText on this word-wrapped box reflowed every frame in the idle state
    setVisibleIfChanged(mAnimHint, !hint.empty());
    if (!hint.empty())
    {
        setTextIfChanged(mAnimHint, hint);
    }

    // row-action buttons (reason-tooltips when disabled, usability rule 1)
    const bool have_row = mAnimList->getFirstSelected() != nullptr;
    const std::string no_row_tip = "Select an animation row first";

    mAnimCopyBtn->setEnabled(have_row);
    setToolTipIfChanged(mAnimCopyBtn, have_row
        ? std::string("Copy the animation asset UUID to the clipboard")
        : no_row_tip);
    mAnimSetLocoBtn->setEnabled(have_row && sel.notNull());
    setToolTipIfChanged(mAnimSetLocoBtn, !have_row ? no_row_tip
        : sel.isNull() ? std::string("Select a cast member first")
        : "Use this animation for " + member_name + "'s selected locomotion role");
    const std::string not_in_world_tip = sel.isNull()
        ? std::string("Select a cast member first")
        : member_name + " is not in world";
    mAnimPlayBtn->setEnabled(have_row && av);
    setToolTipIfChanged(mAnimPlayBtn, !have_row ? no_row_tip
        : !av ? not_in_world_tip
        : "Play this animation on " + member_name + " now (client-side only)");
    mAnimStopBtn->setEnabled(have_row && av);
    setToolTipIfChanged(mAnimStopBtn, !have_row ? no_row_tip
        : !av ? not_in_world_tip
        : "Stop this animation on " + member_name + " (client-side only)");

    // paste row
    // [DirFix] D5: pasteAnimId() is a pure function of the editor text, so recompute it
    // (copy + trim + validate) only when the full text differs from the last one seen.
    const std::string& paste_raw = mPasteEditor->getText();
    if (!mPasteCacheValid || paste_raw != mPasteTextSeen)
    {
        mPasteTextSeen = paste_raw;
        mPasteIdSeen = pasteAnimId();
        std::string trimmed = paste_raw;
        LLStringUtil::trim(trimmed);
        mPasteSeenEmpty = trimmed.empty();
        mPasteCacheValid = true;
    }
    const LLUUID pasted = mPasteIdSeen;
    const bool paste_text_empty = mPasteSeenEmpty;
    const std::string bad_uuid_tip = paste_text_empty
        ? std::string("Enter an animation asset UUID first")
        : std::string("That is not a valid UUID");
    mPastePlayBtn->setEnabled(pasted.notNull() && av);
    setToolTipIfChanged(mPastePlayBtn, pasted.isNull() ? bad_uuid_tip
        : !av ? not_in_world_tip
        : "Play the pasted animation on " + member_name + " now (client-side only)");
    mPasteStopBtn->setEnabled(pasted.notNull() && av);
    setToolTipIfChanged(mPasteStopBtn, pasted.isNull() ? bad_uuid_tip
        : !av ? not_in_world_tip
        : "Stop the pasted animation on " + member_name + " (client-side only)");
    mPasteSetLocoBtn->setEnabled(pasted.notNull() && sel.notNull());
    setToolTipIfChanged(mPasteSetLocoBtn, pasted.isNull() ? bad_uuid_tip
        : sel.isNull() ? std::string("Select a cast member first")
        : "Use the pasted animation for " + member_name + "'s selected locomotion role");

    // Sparse locomotion-role authoring and live analysis status.
    const LLDirectorCast::CastMember* m = cast.getMember(sel);
    refreshLocomotionRoleEditor(sel);
    const S32 selected_role = selectedLocomotionRole();
    const bool have_loco = m && m->mLocomotionSet.mRoles.find(selected_role) !=
        m->mLocomotionSet.mRoles.end();
    mClearLocoBtn->setEnabled(have_loco);
    setToolTipIfChanged(mClearLocoBtn, !m
        ? std::string("Select a cast member first")
        : have_loco ? std::string("Clear this role and restore its deterministic fallback")
                    : std::string("This role already uses its fallback"));

    // feed the shared preview panel: the selected row (else the pasted UUID) on
    // loop, with its Stop / Revoke / Blacklist pointed at whatever object is
    // playing it on your own avatar. The panel owns the dummy, the mouse
    // handling, and those buttons -- one copy, embedded in both hosts.
    if (mAnimPreviewPanel)
    {
        // [DirFix] D5: previewAnimId() == selected row, else the pasted UUID; reuse the
        // paste cache refreshed above (same editor text, same frame)
        const LLUUID row_anim = selectedAnimId();
        const LLUUID focus = row_anim.notNull() ? row_anim : pasted;
        mAnimPreviewPanel->previewAnim(focus, animSourceObject(focus));
    }
}

// ---------------------------------------------------------------------------
// Animate tab: what the shared preview panel is fed
// ---------------------------------------------------------------------------
// The animation the preview shows and the panel's own-avatar controls act on:
// the selected signaled-animation row, else the pasted UUID.
LLUUID LLFloaterDirector::previewAnimId() const
{
    const LLUUID sel = selectedAnimId();
    return sel.notNull() ? sel : pasteAnimId();
}

// The in-world object playing a given animation on your own avatar (revoke /
// blacklist target), or null when nothing on you is playing it. Host-specific:
// the standalone Explorer instead feeds the "played by" object from its list.
LLUUID LLFloaterDirector::animSourceObject(const LLUUID& anim_id) const
{
    if (anim_id.isNull() || !isAgentAvatarValid())
    {
        return LLUUID::null;
    }
    for (const auto& [object_id, source_anim_id] : gAgentAvatarp->mAnimationSources)
    {
        if (source_anim_id == anim_id)
        {
            return object_id;
        }
    }
    return LLUUID::null;
}

// ---------------------------------------------------------------------------
// Camera tab
// ---------------------------------------------------------------------------
//static
LLUUID LLFloaterDirector::avatarFromSelection()
{
    LLViewerObject* obj = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    if (obj)
    {
        LLVOAvatar* av = obj->getAvatar();  // avatar itself or attachment parent
        if (av && !av->isDead())
        {
            return av->getID();
        }
    }
    return LLUUID::null;
}

void LLFloaterDirector::onClickSetSubjectFromSelection(S32 subject)
{
    const LLUUID id = avatarFromSelection();
    if (id.isNull())
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    cast.add(id);       // subjects are ids into the cast; no-op if present
    assign_subject(cast, subject, id);
}

void LLFloaterDirector::onClickAssignSubjects()
{
    const uuid_vec_t selected = selectedCastIds();
    if (selected.empty())
    {
        // Never wipe A-D on an empty selection.
        LLSD args;
        args["MESSAGE"] = "Select cast rows first (roster order fills A, B, C, D).";
        LLNotificationsUtil::add("GenericAlert", args);
        return;
    }
    const std::vector<LLUUID> ordered = ALDirectorQol::subjectAssignment(selected);
    LLDirectorCast& cast = LLDirectorCast::instance();
    for (S32 subject = SUBJECT_A; subject <= SUBJECT_D; ++subject)
        clear_subject(cast, subject);
    // Subjects are 1-based (SUBJECT_A == 1); the ordered list is 0-based.
    for (S32 index = 0; index < (S32)ordered.size() && SUBJECT_A + index <= SUBJECT_D; ++index)
        assign_subject(cast, SUBJECT_A + index, ordered[index]);
    LLSD args;
    args["MESSAGE"] = selected.size() > 4
        ? "Assigned the first four selected cast rows to Subjects A-D; extra selections were ignored."
        : llformat("Assigned %d selected cast row%s to Subjects A-D in roster order.",
                   (S32)ordered.size(), ordered.size() == 1 ? "" : "s");
    LLNotificationsUtil::add("SystemMessageTip", args);
}

void LLFloaterDirector::onClickClearSubject(S32 subject)
{
    clear_subject(LLDirectorCast::instance(), subject);
}

void LLFloaterDirector::onOpenActorGaze()
{
    ALFloaterActorGaze::showForSelection(
        ALFloaterActorGaze::ESelectionSource::DIRECTOR,
        selectedCastIds());
}

void LLFloaterDirector::onClickManagePrism()
{
    LLFloaterReg::showInstance("prism_manager");
}

void LLFloaterDirector::onClickBuildOtsPair()
{
    std::string reason;
    if (!LLPrismLens::buildOtsPairFromSubjects(nullptr, nullptr, &reason))
    {
        LLNotificationsUtil::add(
            "GenericAlert",
            LLSD().with("MESSAGE", reason.empty()
                ? "Unable to build the OTS camera pair." : reason));
        return;
    }
    mHavePrismSummary = false;
}

void LLFloaterDirector::onClickZolly()
{
    std::string reason;
    if (!can_start_manual_subject_shot(&reason))
    {
        LLNotificationsUtil::add(
            "GenericAlert", LLSD().with("MESSAGE", reason));
        return;
    }

    const F32 first = gSavedSettings.getF32("CinematicCamVertigoStartDist");
    const F32 second = gSavedSettings.getF32("CinematicCamVertigoEndDist");
    const F32 near_distance = llmin(first, second);
    const F32 far_distance = llmax(first, second);
    const bool dolly_in =
        gSavedSettings.getS32("CinematicCamVertigoDirection") != 0;
    gSavedSettings.setF32("CinematicCamVertigoStartDist",
                          dolly_in ? far_distance : near_distance);
    gSavedSettings.setF32("CinematicCamVertigoEndDist",
                          dolly_in ? near_distance : far_distance);
    gSavedSettings.setS32("CinematicCamVertigoEndMode", 0);
    LLCinematicCamera::instance().triggerMode(
        LLCinematicCamera::MODE_DOLLY_ZOOM, true);
}

void LLFloaterDirector::onClickHeroArc()
{
    std::string reason;
    if (!can_start_manual_subject_shot(&reason))
    {
        LLNotificationsUtil::add(
            "GenericAlert", LLSD().with("MESSAGE", reason));
        return;
    }

    const F32 angle = llclamp(
        gSavedSettings.getF32("CinematicCamArcGeneratorAngle"), 30.f, 180.f);
    gSavedSettings.setF32("CinematicCamArcFrom", -0.5f * angle);
    gSavedSettings.setF32("CinematicCamArcTo", 0.5f * angle);
    gSavedSettings.setS32("CinematicCamArcEndMode", 0);
    LLCinematicCamera::instance().triggerMode(
        LLCinematicCamera::MODE_ARC, true);
}

void LLFloaterDirector::refreshGazeSelection(const uuid_vec_t& selected)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director refreshGazeSelection");
    // [DirFix] D10: push to the Actor Gaze floater on selection change only (the registry
    // lookup used to run every frame); a reopened gaze floater pulls on open.
    const uuid_vec_t& targets = actorTargetsForPage("performance_group/gaze_tab", selected);
    if (targets != mLastGazePush)
    {
        mLastGazePush = targets;
        ALFloaterActorGaze::updateSelection(
            ALFloaterActorGaze::ESelectionSource::DIRECTOR, targets);
        if (mGazePanel) mGazePanel->setSelectedActors(targets);
    }
}

void LLFloaterDirector::refreshSubjects()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director refreshSubjects");
    if (!mSubjectAText || !mSubjectBText || !mSubjectCText || !mSubjectDText ||
        !mSetABtn || !mSetBBtn || !mSetCBtn || !mSetDBtn ||
        !mClearABtn || !mClearBBtn || !mClearCBtn || !mClearDBtn)
    {
        return;
    }
    if (!mSubjectAText->isInVisibleChain()) return;
    LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID a = cast.getSubjectA();
    const LLUUID b = cast.getSubjectB();
    const LLUUID c = cast.getSubjectC();
    const LLUUID d = cast.getSubjectD();

    // [UIPerfC1] diff-guarded: identical text is never rewritten
    setTextIfChanged(mSubjectAText, "A: " + (a.notNull() ? castMemberName(a)
                                                : std::string("\xE2\x80\x94")));
    setTextIfChanged(mSubjectBText, "B: " + (b.notNull() ? castMemberName(b)
                                                 : std::string("\xE2\x80\x94")));
    setTextIfChanged(mSubjectCText, "C: " + (c.notNull() ? castMemberName(c)
                                                 : std::string("\xE2\x80\x94")));
    setTextIfChanged(mSubjectDText, "D: " + (d.notNull() ? castMemberName(d)
                                                 : std::string("\xE2\x80\x94")));

    const bool have_sel_av = avatarFromSelection().notNull();
    for (LLButton* button : { mSetABtn, mSetBBtn, mSetCBtn, mSetDBtn })
    {
        setEnabledIfChanged(button, have_sel_av);
    }
    const std::string set_tip = have_sel_av
        ? std::string("Make the selected avatar this subject (adds it to the cast)")
        : std::string("Select an avatar or animesh in world first");
    setToolTipIfChanged(mSetABtn, set_tip);
    setToolTipIfChanged(mSetBBtn, set_tip);
    setToolTipIfChanged(mSetCBtn, set_tip);
    setToolTipIfChanged(mSetDBtn, set_tip);

    setEnabledIfChanged(mClearABtn, a.notNull());
    setToolTipIfChanged(mClearABtn, a.notNull()
        ? std::string("Clear Subject A; the camera falls back to follow-target / selection / you")
        : std::string("Subject A is not set"));
    setEnabledIfChanged(mClearBBtn, b.notNull());
    setToolTipIfChanged(mClearBBtn, b.notNull()
        ? std::string("Clear Subject B; Two-Shot/OTS fall back to you + target")
        : std::string("Subject B is not set"));
    setEnabledIfChanged(mClearCBtn, c.notNull());
    setToolTipIfChanged(mClearCBtn, c.notNull()
        ? std::string("Clear Subject C")
        : std::string("Subject C is not set"));
    setEnabledIfChanged(mClearDBtn, d.notNull());
    setToolTipIfChanged(mClearDBtn, d.notNull()
        ? std::string("Clear Subject D")
        : std::string("Subject D is not set"));

}

void LLFloaterDirector::refreshPrismDependents()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director refreshPrismDependents");
    if (!mOtsPairBtn || !mPrismSummaryText || !mZollyBtn || !mHeroArcBtn)
    {
        return;
    }
    // The Director refreshes all tab models while it is open. Avoid taking
    // Prism registry/gate snapshots every frame unless Camera is the visible
    // tab; the controls will refresh immediately when their visible chain is
    // restored.
    if (!mOtsPairBtn->isInVisibleChain() && !mPrismSummaryText->isInVisibleChain())
    {
        return;
    }

    LLDirectorCast& cast = LLDirectorCast::instance();

    LLVOAvatar* live_a = cast.resolveSubjectA();
    LLVOAvatar* live_b = cast.resolveSubjectB();
    const bool distinct_live_pair = live_a && live_b &&
        live_a->getID() != live_b->getID();
    if (!distinct_live_pair)
    {
        setEnabledIfChanged(mOtsPairBtn, false);
        setToolTipIfChanged(mOtsPairBtn,
            !live_a || !live_b
                ? std::string("Set live Director Subjects A and B first")
                : std::string("Subjects A and B must be different avatars"));
    }

    std::string subject_shot_reason;
    const bool can_trigger_subject_shot =
        can_start_manual_subject_shot(&subject_shot_reason);
    setEnabledIfChanged(mZollyBtn, can_trigger_subject_shot);
    setEnabledIfChanged(mHeroArcBtn, can_trigger_subject_shot);
    const std::string subject_shot_tip = can_trigger_subject_shot
        ? std::string("Start this one-shot move locked to Subject A's head")
        : subject_shot_reason;
    setToolTipIfChanged(mZollyBtn, subject_shot_tip);
    setToolTipIfChanged(mHeroArcBtn, subject_shot_tip);

    if (mHavePrismSummary &&
        mPrismRefreshTimer.getElapsedTimeF32() < PRISM_SNAPSHOT_POLL_SECONDS)
    {
        return;
    }
    mPrismRefreshTimer.reset();

    const LLPrismLens::RegistrySnapshot ots_registry =
        LLPrismLens::registrySnapshot();
    const LLPrismLens::GateSnapshot ots_gate = LLPrismLens::gateSnapshot();
    std::set<LLUUID> replaceable_ots_captures;
    std::size_t replaceable_ots_arms = 0;
    for (const LLPrismLens::GateArmedCamera& armed :
         ots_gate.mSettings.mArmed)
    {
        if (LLPrismLens::isOtsPairArmLabel(armed.mLabel))
        {
            ++replaceable_ots_arms;
            replaceable_ots_captures.insert(armed.mCaptureId);
        }
    }
    U32 replaceable_ots_capture_count = 0;
    for (U32 index = 0; index < ots_registry.mCaptureCount; ++index)
    {
        if (replaceable_ots_captures.count(
                ots_registry.mCaptures[index].mHandle.mId) != 0)
        {
            ++replaceable_ots_capture_count;
        }
    }
    const bool ots_capacity =
        ots_registry.mCaptureCount - replaceable_ots_capture_count <=
            LLPrismLens::MAX_CAPTURES - 2 &&
        ots_gate.mSettings.mArmed.size() - replaceable_ots_arms <=
            LLPrismLens::MAX_CAPTURES - 2;
    setEnabledIfChanged(mOtsPairBtn, distinct_live_pair && ots_capacity);
    setToolTipIfChanged(mOtsPairBtn,
        !live_a || !live_b
            ? std::string("Set live Director Subjects A and B first")
            : !distinct_live_pair
                ? std::string("Subjects A and B must be different avatars")
                : !ots_capacity
                    ? std::string("Two free VCam capture and Gate arm slots are required")
                    : std::string("Create reciprocal matched-lens OTS A/B virtual cameras and arm both in the Gate"));

    const U64 prism_configuration_revision =
        ots_registry.mConfigurationRevision;
    const U64 prism_runtime_revision = ots_registry.mRuntimeRevision;
    if (!mHavePrismSummary ||
        prism_configuration_revision != mPrismConfigurationRevision ||
        prism_runtime_revision != mPrismRuntimeRevision)
    {
        const LLPrismLens::RegistrySnapshot& snapshot = ots_registry;
        U32 capture_attention = 0;
        U32 display_attention = 0;
        U32 updating = 0;
        for (U32 index = 0; index < snapshot.mCaptureCount; ++index)
        {
            const LLPrismLens::CaptureDefinition& capture =
                snapshot.mCaptures[index];
            capture_attention += capture.mRuntime.mHealth !=
                LLPrismLens::ECaptureHealth::READY;
            updating +=
                capture.mRuntime.mActivity == LLPrismLens::EActivityState::LIVE ||
                capture.mRuntime.mActivity == LLPrismLens::EActivityState::THROTTLED;
        }
        for (U32 index = 0; index < snapshot.mDisplayCount; ++index)
        {
            display_attention += snapshot.mDisplays[index].mRuntime.mHealth !=
                LLPrismLens::EDisplayHealth::READY;
        }
        const bool needs_attention =
            capture_attention != 0 || display_attention != 0;

        setTextIfChanged(mPrismSummaryText, llformat(
            "Prism %u/%u captures | %u/%u faces | %u live%s",
            snapshot.mCaptureCount, LLPrismLens::MAX_CAPTURES,
            snapshot.mDisplayCount, LLPrismLens::MAX_DISPLAY_BINDINGS,
            updating, needs_attention ? " | attention" : ""));
        setToolTipIfChanged(mPrismSummaryText, llformat(
            "%u captures and %u display bindings; attention: %u capture%s, %u display%s. Open Virtual Cam Manager for sources, faces, optics, picture FPS, and adaptive performance.",
            snapshot.mCaptureCount, snapshot.mDisplayCount,
            capture_attention, capture_attention == 1 ? "" : "s",
            display_attention, display_attention == 1 ? "" : "s"));
        mPrismConfigurationRevision = prism_configuration_revision;
        mPrismRuntimeRevision = prism_runtime_revision;
        mHavePrismSummary = true;
    }
}

// ---------------------------------------------------------------------------
// status strip
// ---------------------------------------------------------------------------
void LLFloaterDirector::refreshStatusStrip()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director refreshStatusStrip"); // [DirFix] D9
    static LLCachedControl<bool> cam_enabled(gSavedSettings, "CinematicCamEnabled", false);
    static LLCachedControl<S32>  cam_mode(gSavedSettings, "CinematicCamMode", 0);
    static LLCachedControl<bool> orbit_enabled(gSavedSettings, "FlycamOrbitEnabled", false);

    LLDirectorCast& cast = LLDirectorCast::instance();
    LLActorMover& mover = LLActorMover::instance();

    S32 moving = 0;
    for (const LLUUID& id : cast.getIds())
    {
        if (mover.isMoving(id))
        {
            ++moving;
        }
    }

    const ALDirectorSwitcher& switcher = ALDirectorSwitcher::instance();
    const bool switching = switcher.isDrivingCamera();
    const S32 effective_mode =
        switching ? switcher.activeMode() : (S32)cam_mode;
    const bool camera_active = cam_enabled && effective_mode > 0;
    const S32 switcher_slot = switching ? switcher.activeSlot() : -1;
    LLUUID target_id;
    bool selection_target = false;
    if (camera_active)
    {
        // Name the framing target. A live per-slot mark wins while switching;
        // an empty/dead mark mirrors CineCam's fallback to the legacy chain.
        static LLCachedControl<bool> use_selected(gSavedSettings, "CinematicCamUseSelected", false);
        const LLUUID slot_subject = switching
            ? subject_mark_id(cast, switcher.activePrimarySubject())
            : LLUUID::null;
        if (slot_subject.notNull() && cast.resolve(slot_subject))
            target_id = slot_subject;
        else if (cast.getSubjectA().notNull())
            target_id = cast.getSubjectA();
        else if (use_selected)
        {
            target_id = avatarFromSelection();
            selection_target = target_id.isNull();
        }
    }

    const S32 queued_count = cast.hasPendingStarts() ? cast.pendingStartCount() : 0;
    const U64 cast_revision = cast.getCastRevision();
    if (moving != mStatusMoving || queued_count != mStatusQueued ||
        effective_mode != mStatusCamMode || switcher_slot != mStatusSwitcherSlot ||
        cast_revision != mStatusCastRevision || camera_active != mStatusCamEnabled ||
        switching != mStatusSwitching || (bool)orbit_enabled != mStatusOrbitEnabled ||
        selection_target != mStatusSelectionTarget || target_id != mStatusTargetId)
    {
        mStatusMoving = moving;
        mStatusQueued = queued_count;
        mStatusCamMode = effective_mode;
        mStatusSwitcherSlot = switcher_slot;
        mStatusCastRevision = cast_revision;
        mStatusCamEnabled = camera_active;
        mStatusSwitching = switching;
        mStatusOrbitEnabled = orbit_enabled;
        mStatusSelectionTarget = selection_target;
        mStatusTargetId = target_id;

        std::string cam = "CineCam: off";
        if (camera_active)
        {
            const std::string target = target_id.notNull()
                ? castMemberName(target_id) : (selection_target ? "selection" : "you");
            cam = switching
                ? llformat("Switcher %d: %s \xE2\x86\x92 %s", switcher_slot + 1,
                           cinecam_mode_name(effective_mode), target.c_str())
                : llformat("CineCam: %s \xE2\x86\x92 %s",
                           cinecam_mode_name(effective_mode), target.c_str());
        }
        const std::string queued = queued_count > 0
            ? llformat(" \xC2\xB7 %d queued", queued_count) : std::string();
        mRoutineStatus = llformat("%d moving%s \xC2\xB7 %s \xC2\xB7 Orbit %s",
                                  moving, queued.c_str(), cam.c_str(), orbit_enabled ? "on" : "off");
        mCastChipTooltip = mRoutineStatus + ". Open cast tools and member list.";
    }
    const bool problem = mIsV2 && !mActionArmed;
    if (mIsV2 && problem)
    {
        setTextIfChanged(mStatusStrip, "ACTION has nothing armed");
        setToolTipIfChanged(mStatusStrip, "Open ACTION starts and arm at least one item");
    }
    if (mIsV2)
    {
        if (mTransportBar && mStatusStrip->getVisible() != problem)
            mTransportBar->setCollapsibleVisible(mStatusStrip, problem);
        else if (!mTransportBar)
            mStatusStrip->setVisible(problem);
    }
    else
    {
        setTextIfChanged(mStatusStrip, mRoutineStatus);
        setToolTipIfChanged(mStatusStrip, mRoutineStatus);
    }

    if (mCastChip)
    {
        const S32 cast_count = (S32)cast.getCast().size();
        if (mLastCastChipCount != cast_count)
        {
            mLastCastChipCount = cast_count;
            mCastChip->setLabel(LLStringExplicit(llformat("Cast %d  ^", cast_count))); // ui-draw-lint: allow(raw-write) only when the cast count changed
        }
        setToolTipIfChanged(mCastChip, mCastChipTooltip);
    }

    // REC indicator: red danger dot while capturing, muted circle when idle
    // (diffed so the color/text are only re-set on a state change)
    const bool rec = LLFlycamRecorder::instance().getState() == LLFlycamRecorder::STATE_RECORDING;
    const S32 rec_state = rec ? 1 : 0;
    if (mRecState != rec_state)
    {
        mRecState = rec_state;
        const LLColor4 rec_color = mIsV2
            ? (rec ? LLColor4::white : ALKitStyle::color(ALKitStyle::FAINT).get())
            : (rec ? LLColor4(0.85f, 0.25f, 0.25f, 1.f) : LLColor4(0.5f, 0.5f, 0.5f, 1.f));
        if (!mIsV2)
            mRecIndicator->setText(rec ? std::string("REC \xE2\x97\x8F")
                                       : std::string("REC \xE2\x97\x8B"));
        mRecIndicator->setColor(rec_color);
        mRecIndicator->setReadOnlyColor(rec_color);
        if (mTransportBar) mTransportBar->setRecording(rec);
        setToolTipIfChanged(mRecIndicator, rec ? "Recording the current camera take" :
            "Recorder idle. Arm Recorder capture in ACTION starts, or use the Takes page.");
    }
}
