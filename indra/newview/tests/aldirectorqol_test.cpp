#include "linden_common.h"

#include "../test/lltut.h"
#include "../aldirectorqol.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace tut
{
struct director_qol_data {};
using director_qol_group = test_group<director_qol_data>;
using director_qol_object = director_qol_group::object;
director_qol_group director_qol_tests("ALDirectorQol");

template<> template<> void director_qol_object::test<1>()
{
    ensure("exact beats prefix", ALDirectorQol::fuzzyTokenScore("rim", "rim") >
                                  ALDirectorQol::fuzzyTokenScore("rim", "rimming"));
    ensure("prefix beats typo", ALDirectorQol::fuzzyTokenScore("expo", "exposure") >
                                ALDirectorQol::fuzzyTokenScore("expsoure", "exposure"));
    ensure("transposition typo matches", ALDirectorQol::fuzzyTokenScore("expsoure", "exposure") > 0);
    ensure("subsequence matches", ALDirectorQol::fuzzyTokenScore("shdw", "shadow") > 0);
    ensure("closer typo ranks higher",
           ALDirectorQol::fuzzyTokenScore("exposore", "exposure") >
           ALDirectorQol::fuzzyTokenScore("expasore", "exposure"));
    ensure_equals("noise rejected", ALDirectorQol::fuzzyTokenScore("zzz", "exposure"), 0);
}

template<> template<> void director_qol_object::test<2>()
{
    const LLUUID a("00000000-0000-0000-0000-000000000001");
    const LLUUID b("00000000-0000-0000-0000-000000000002");
    const LLUUID c("00000000-0000-0000-0000-000000000003");
    const LLUUID d("00000000-0000-0000-0000-000000000004");
    const LLUUID e("00000000-0000-0000-0000-000000000005");
    const std::vector<LLUUID> assigned = ALDirectorQol::subjectAssignment({a, b, c, d, e});
    ensure_equals("four subjects", assigned.size(), size_t(4));
    ensure_equals("roster order A", assigned[0], a);
    ensure_equals("roster order D", assigned[3], d);
}

// Live autosave file is per process: concurrent viewers never share a file, and the login move collects every
// live file that is not this process's own.
template<> template<> void director_qol_object::test<3>()
{
    ensure_equals("live name carries the pid", ALDirectorQol::liveRecoveryFileName(4321),
                  std::string(".director_recovery-p4321.xml"));
    ensure("two processes never share a live name",
           ALDirectorQol::liveRecoveryFileName(4321) != ALDirectorQol::liveRecoveryFileName(77));
    ensure_equals("pid round-trips", ALDirectorQol::liveRecoveryPid(ALDirectorQol::liveRecoveryFileName(4321)), 4321);
    ensure_equals("pid parse is case-insensitive", ALDirectorQol::liveRecoveryPid(".Director_Recovery-P77.XML"), 77);
    for (const char* bad : {".director_recovery.xml", ".director_recovery-p.xml", ".director_recovery-p0.xml",
                            ".director_recovery-p012.xml", ".director_recovery-px.xml", ".director_recovery-p12x.xml",
                            ".director_recovery-p12.xml.tmp", ".director_recovery-p12345678901.xml",
                            ".director_recovery-p2147483648.xml", ".director_recovery-p9999999999.xml",
                            ".director_recovery-p12.xml.lock", ".director_recovery-p12-0.xml",
                            ".director_recovery-p12-.xml", ".director_recovery-p12-01.xml",
                            ".director_recovery-p12-1234567.xml", ".director_recovery-p12-x.xml",
                            ".director_recovery_kept.xml", ".director_recovery-12-20261015090703.xml", "scene.xml"})
        ensure_equals(std::string("not a pid name: ") + bad, ALDirectorQol::liveRecoveryPid(bad), 0);

    ensure("a pid file is a live file", ALDirectorQol::isLiveRecoveryFile(".director_recovery-p4321.xml"));
    ensure("the legacy plain file is a live file", ALDirectorQol::isLiveRecoveryFile(".director_recovery.xml"));
    ensure("the legacy plain file is recognised as legacy", ALDirectorQol::isLegacyLiveRecoveryName(".Director_Recovery.xml"));
    for (const char* not_live : {".director_recovery_kept.xml", ".director_recovery-p12.xml.tmp", ".director_recovery_2.xml",
                                 "director_recovery.xml", "scene.xml", ".director_recovery-p.xml"})
        ensure(std::string("not a live file: ") + not_live, !ALDirectorQol::isLiveRecoveryFile(not_live));
    ensure("a pid name is reserved as a scene name", ALDirectorQol::isReservedSceneName(".director_recovery-p4321"));

    // Login collection: other pids and the legacy name are previous-session; this process's own file is not
    // once it has written one. Before it has written (at login) even a same-pid file is a stale leftover.
    const S32 mine = 4321;
    ensure("another process's file is collected at login",
           ALDirectorQol::isPreviousSessionLiveFile(".director_recovery-p77.xml", mine, false));
    ensure("another process's file is collected even after this process wrote its own",
           ALDirectorQol::isPreviousSessionLiveFile(".director_recovery-p77.xml", mine, true));
    ensure("the legacy file is collected", ALDirectorQol::isPreviousSessionLiveFile(".director_recovery.xml", mine, true));
    ensure("a stale same-pid file at login (nothing written yet) is collected",
           ALDirectorQol::isPreviousSessionLiveFile(".director_recovery-p4321.xml", mine, false));
    ensure("this process's own written file is never collected",
           !ALDirectorQol::isPreviousSessionLiveFile(".director_recovery-p4321.xml", mine, true));
    ensure("a non-live file is never collected",
           !ALDirectorQol::isPreviousSessionLiveFile("scene.xml", mine, false) &&
           !ALDirectorQol::isPreviousSessionLiveFile(".director_recovery_kept.xml", mine, false));
}

template<> template<> void director_qol_object::test<4>()
{
    using Field = ALDirectorQol::SearchField;
    ensure("setting-name fragment matches",
           ALDirectorQol::searchTokenScore("render", "render", Field::SETTING) > 0);
    ensure("setting name is a strong field",
           ALDirectorQol::searchTokenScore("bloom", "bloom", Field::SETTING) >
           ALDirectorQol::searchTokenScore("bloom", "bloom", Field::KEYWORD));
    ensure("expanded synonym remains searchable but below a direct match",
           ALDirectorQol::searchTokenScore("brightness", "brightness", Field::SYNONYM) > 0 &&
           ALDirectorQol::searchTokenScore("brightness", "brightness", Field::SYNONYM) <
           ALDirectorQol::searchTokenScore("brightness", "brightness", Field::LABEL));
    ensure("Director wins an equal-score tie",
           ALDirectorQol::searchResultBefore(200, false, 8, 200, true, 1));
    ensure("external result does not win an equal-score tie",
           !ALDirectorQol::searchResultBefore(200, true, 1, 200, false, 8));
}

// Login handling per mode, and the prompt response -> action mapping: ignore / close keeps,
// only an explicit Discard deletes (the one backup), Restore never deletes.
template<> template<> void director_qol_object::test<5>()
{
    using Login = ALDirectorQol::LoginAction;
    ensure("no backup this login does nothing", ALDirectorQol::loginAction(false, 0) == Login::NONE);
    ensure("Ask offers", ALDirectorQol::loginAction(true, 0) == Login::OFFER);
    ensure("Restore automatically restores", ALDirectorQol::loginAction(true, 1) == Login::RESTORE_AUTO);
    ensure("Don't restore keeps the backup", ALDirectorQol::loginAction(true, 2) == Login::KEEP);
    ensure("unknown mode asks", ALDirectorQol::loginAction(true, 9) == Login::OFFER);
    ensure("no backup beats every mode", ALDirectorQol::loginAction(false, 1) == Login::NONE);

    using Prompt = ALDirectorQol::PromptAction;
    // promptAction(option, remember = "Don't ask me again" ticked, replayed = answered by LLNotifications, not the user)
    ensure("Restore button restores", ALDirectorQol::promptAction(0, false, false) == Prompt::RESTORE);
    ensure("unticked Discard deletes (only that file)", ALDirectorQol::promptAction(1, false, false) == Prompt::DISCARD);
    ensure("ticked Restore restores and sets the mode", ALDirectorQol::promptAction(0, true, false) == Prompt::RESTORE);
    ensure("ticked Discard KEEPS the backup (it only sets mode 2)", ALDirectorQol::promptAction(1, true, false) == Prompt::KEEP);
    ensure("closing the window keeps the backup", ALDirectorQol::promptAction(-1, false, false) == Prompt::KEEP);
    ensure("an ignored prompt with no saved response keeps the backup", ALDirectorQol::promptAction(2, false, false) == Prompt::KEEP);
    for (S32 option : {-1, 0, 1, 2, 7})
        for (bool remember : {false, true})
            ensure("a replayed response can never restore or delete",
                   ALDirectorQol::promptAction(option, remember, true) == Prompt::KEEP);
    for (S32 option : {-1, 0, 2, 7})
        ensure("only an unticked Discard deletes", ALDirectorQol::promptAction(option, false, false) != Prompt::DISCARD);
    ensure("a ticked Discard never deletes", ALDirectorQol::promptAction(1, true, false) != Prompt::DISCARD);

    // A replayed response changes neither the mode nor the ignore state.
    for (S32 option : {-1, 0, 1, 2})
        for (bool remember : {false, true})
            ensure_equals("a replayed response never changes the mode",
                          ALDirectorQol::rememberedRecoveryMode(option, remember, true), -1);
    ensure("a replayed response may not clear the ignore flag", !ALDirectorQol::mayClearPromptIgnore(true));
    ensure("a live response may clear the ignore flag", ALDirectorQol::mayClearPromptIgnore(false));
    ensure_equals("ticked Restore -> Restore automatically", ALDirectorQol::rememberedRecoveryMode(0, true), 1);
    ensure_equals("ticked Discard -> Don't restore", ALDirectorQol::rememberedRecoveryMode(1, true), 2);
    ensure_equals("unticked changes nothing", ALDirectorQol::rememberedRecoveryMode(0, false), -1);
    ensure_equals("a closed prompt never changes the mode", ALDirectorQol::rememberedRecoveryMode(-1, true), -1);
}

// Backup naming: scene-YYYYMMDD-HHMMSS[-N].xml, unique, never overwriting.
template<> template<> void director_qol_object::test<6>()
{
    ALDirectorQol::BackupStamp stamp;
    stamp.year = 2026; stamp.month = 10; stamp.day = 5; stamp.hour = 9; stamp.minute = 7; stamp.second = 3;
    ensure_equals("plain name", ALDirectorQol::crashBackupFileName(stamp), std::string("scene-20261005-090703.xml"));
    ensure_equals("counter name", ALDirectorQol::crashBackupFileName(stamp, 2), std::string("scene-20261005-090703-2.xml"));
    ensure_equals("a free name is used as is", ALDirectorQol::uniqueCrashBackupName(stamp, {}),
                  std::string("scene-20261005-090703.xml"));
    ensure_equals("a taken name gets -1",
                  ALDirectorQol::uniqueCrashBackupName(stamp, {"scene-20261005-090703.xml"}),
                  std::string("scene-20261005-090703-1.xml"));
    ensure_equals("then -2",
                  ALDirectorQol::uniqueCrashBackupName(stamp, {"scene-20261005-090703.xml", "scene-20261005-090703-1.xml"}),
                  std::string("scene-20261005-090703-2.xml"));
    ensure_equals("a gap is filled, an existing name is never returned",
                  ALDirectorQol::uniqueCrashBackupName(stamp, {"scene-20261005-090703.xml", "scene-20261005-090703-2.xml"}),
                  std::string("scene-20261005-090703-1.xml"));
    ensure_equals("existing names compare case-insensitively",
                  ALDirectorQol::uniqueCrashBackupName(stamp, {"SCENE-20261005-090703.XML"}),
                  std::string("scene-20261005-090703-1.xml"));

    const ALDirectorQol::ParsedBackupName plain = ALDirectorQol::parseCrashBackupName("scene-20261005-090703.xml");
    ensure("plain name parses", plain.valid && plain.counter == 0 && plain.stamp == 20261005090703ULL);
    const ALDirectorQol::ParsedBackupName counted = ALDirectorQol::parseCrashBackupName("scene-20261005-090703-12.xml");
    ensure("counter name parses", counted.valid && counted.counter == 12 && counted.stamp == plain.stamp);
    ensure_equals("pid name", ALDirectorQol::crashBackupFileName(stamp, 0, 4321), std::string("scene-20261005-090703-p4321.xml"));
    ensure_equals("pid + counter name", ALDirectorQol::crashBackupFileName(stamp, 2, 4321),
                  std::string("scene-20261005-090703-p4321-2.xml"));
    ensure_equals("a taken pid name gets -1",
                  ALDirectorQol::uniqueCrashBackupName(stamp, {"scene-20261005-090703-p4321.xml"}, 4321),
                  std::string("scene-20261005-090703-p4321-1.xml"));
    ensure_equals("two processes in the same second get different names",
                  ALDirectorQol::uniqueCrashBackupName(stamp, {"scene-20261005-090703-p4321.xml"}, 77),
                  std::string("scene-20261005-090703-p77.xml"));
    const ALDirectorQol::ParsedBackupName with_pid = ALDirectorQol::parseCrashBackupName("scene-20261005-090703-p4321-12.xml");
    ensure("pid name parses", with_pid.valid && with_pid.counter == 12 && with_pid.stamp == plain.stamp);
    ensure("pid-only name parses", ALDirectorQol::parseCrashBackupName("scene-20261005-090703-p4321.xml").valid);
    for (const char* bad : {"scene-20261005-090703-p.xml", "scene-20261005-090703-px.xml", "scene-20261005-090703-p12345678901.xml",
                            "scene-20261005-090703-p12-0.xml", "scene-20261005-090703-p12x.xml"})
        ensure(std::string("unparseable: ") + bad, !ALDirectorQol::parseCrashBackupName(bad).valid);
    for (const char* bad : {"scene.xml", "scene-20261005-090703.txt", "scene-20261305-090703.xml", "scene-20261005-250703.xml",
                            "scene-20261005-090703-0.xml", "scene-20261005-090703-.xml", "scene-2026100-090703.xml",
                            "scene-20261005-090703-x.xml", "backup.xml", "", "scene-20261005-0907031.xml"})
        ensure(std::string("unparseable: ") + bad, !ALDirectorQol::parseCrashBackupName(bad).valid);
}

// Sort / prune order: parsed timestamp then numeric -N counter, never lexical; unparseable = oldest.
template<> template<> void director_qol_object::test<7>()
{
    const std::vector<std::string> sorted = ALDirectorQol::sortCrashBackupsNewestFirst({
        "scene-20261005-090703-9.xml", "zzz-not-a-backup.xml", "scene-20261005-090703-10.xml",
        "scene-20261006-000000.xml", "scene-20261005-090703.xml", "scene-20250101-000000.xml", "a.xml"});
    const std::vector<std::string> expected = {
        "scene-20261006-000000.xml", "scene-20261005-090703-10.xml", "scene-20261005-090703-9.xml",
        "scene-20261005-090703.xml", "scene-20250101-000000.xml", "zzz-not-a-backup.xml", "a.xml"};
    ensure("newest first: timestamp, numeric -N (-10 after -9), unparseable names oldest", sorted == expected);
    ensure("equal-second -N ordering: -1 is newer than the plain name",
           ALDirectorQol::crashBackupNewer("scene-20261005-090703-1.xml", "scene-20261005-090703.xml"));
    ensure("-10 is newer than -9 (not lexical)",
           ALDirectorQol::crashBackupNewer("scene-20261005-090703-10.xml", "scene-20261005-090703-9.xml"));
    ensure("an unparseable name is older than any parseable one even when it sorts later lexically",
           ALDirectorQol::crashBackupNewer("scene-20000101-000000.xml", "zzz.xml"));

    // Prune keeps the newest N, oldest go first.
    std::vector<std::string> seven;
    for (S32 day = 1; day <= 7; ++day)
    {
        ALDirectorQol::BackupStamp stamp;
        stamp.year = 2026; stamp.month = 10; stamp.day = day; stamp.hour = 12;
        seven.push_back(ALDirectorQol::crashBackupFileName(stamp));
    }
    const std::vector<std::string> doomed = ALDirectorQol::crashBackupsToPrune(seven, 5, {});
    ensure_equals("keeps the newest 5 of 7", doomed.size(), size_t(2));
    ensure_equals("the OLDEST excess is deleted first", doomed[0], std::string("scene-20261001-120000.xml"));
    ensure_equals("then the next oldest", doomed[1], std::string("scene-20261002-120000.xml"));
    const std::vector<std::string> doomed_two = ALDirectorQol::crashBackupsToPrune(seven, ALDirectorQol::kCrashBackupKeep, {});
    ensure_equals("the real cap keeps the newest 2 of 7", doomed_two.size(), size_t(5));
    ensure_equals("oldest first with the real cap", doomed_two.front(), std::string("scene-20261001-120000.xml"));
    ensure_equals("and the 3rd newest goes last", doomed_two.back(), std::string("scene-20261005-120000.xml"));
    ensure("under the cap prunes nothing", ALDirectorQol::crashBackupsToPrune({"a.xml", "b.xml"}, 5, {}).empty());

    // 8 equal-second backups: -N ordering decides what survives.
    ALDirectorQol::BackupStamp second;
    second.year = 2026; second.month = 10; second.day = 5; second.hour = 9; second.minute = 7; second.second = 3;
    std::vector<std::string> same_second;
    for (S32 counter = 0; counter < 8; ++counter)
        same_second.push_back(ALDirectorQol::crashBackupFileName(second, counter));
    const std::vector<std::string> doomed_same = ALDirectorQol::crashBackupsToPrune(same_second, 5, {});
    ensure_equals("three of eight equal-second backups pruned", doomed_same.size(), size_t(3));
    ensure_equals("the plain (oldest) name goes first", doomed_same[0], std::string("scene-20261005-090703.xml"));
    ensure_equals("then -1, then -2 (oldest to newest)", doomed_same[2], std::string("scene-20261005-090703-2.xml"));
    ensure("-7 survives", std::find(doomed_same.begin(), doomed_same.end(), "scene-20261005-090703-7.xml") == doomed_same.end());

    // A protected file (in use / just restored) is never pruned.
    const std::vector<std::string> kept = ALDirectorQol::crashBackupsToPrune(
        seven, 5, {"SCENE-20261001-120000.XML"});
    ensure_equals("a protected backup survives pruning (case-insensitive)", kept.size(), size_t(1));
    ensure_equals("only the unprotected one is pruned", kept[0], std::string("scene-20261002-120000.xml"));
    ensure("an unparseable name is the oldest and goes first, then the older real backup",
           ALDirectorQol::crashBackupsToPrune({"scene-20261001-120000.xml", "scene-20261002-120000.xml", "junk.xml"}, 1, {}) ==
           std::vector<std::string>({"junk.xml", "scene-20261001-120000.xml"}));
    ensure("an unparseable name is pruned first",
           ALDirectorQol::crashBackupsToPrune({"scene-20261001-120000.xml", "junk.xml"}, 1, {}).front() == "junk.xml");

    // Damaged files: .corrupt, unique, max 3, separate from the 2 backups.
    ensure_equals("corrupt name", ALDirectorQol::uniqueCorruptName("scene-20261005-090703.xml", {}),
                  std::string("scene-20261005-090703.xml.corrupt"));
    ensure_equals("corrupt name is unique",
                  ALDirectorQol::uniqueCorruptName("scene-20261005-090703.xml", {"scene-20261005-090703.xml.corrupt"}),
                  std::string("scene-20261005-090703.xml.1.corrupt"));
    const std::vector<std::string> corrupt_doomed = ALDirectorQol::corruptBackupsToPrune(
        {"scene-20261001-120000.xml.corrupt", "scene-20261004-120000.xml.corrupt", "scene-20261002-120000.xml.corrupt",
         "scene-20261003-120000.xml.corrupt", "scene-20261005-120000.xml.corrupt"}, ALDirectorQol::kCorruptBackupKeep);
    ensure_equals("max 3 corrupt files kept", corrupt_doomed.size(), size_t(2));
    ensure_equals("oldest corrupt first", corrupt_doomed[0], std::string("scene-20261001-120000.xml.corrupt"));
    ensure_equals("then the next", corrupt_doomed[1], std::string("scene-20261002-120000.xml.corrupt"));
    ensure_equals("the cap is 2 backups (plus the live autosave)", ALDirectorQol::kCrashBackupKeep, size_t(2));
    ensure_equals("the cap is 3 damaged files", ALDirectorQol::kCorruptBackupKeep, size_t(3));
}

// Reserved scene names, and the legacy kept-backup migration set.
template<> template<> void director_qol_object::test<8>()
{
    for (const char* reserved : {".director_recovery", ".director_recovery_2", ".director_recovery_kept", ".hidden",
                                 "crash_backups", "Crash_Backups", "CRASH_BACKUPS ", "  crash_backups", ".Director_Recovery",
                                 "crash_backups.", ".", ".."})
        ensure(std::string("reserved: '") + reserved + "'", ALDirectorQol::isReservedSceneName(reserved));
    for (const char* fine : {"Opening shot", "recovery", "crash backups", "my_crash_backups", "crash_backups2", "scene-20261005-090703",
                             "director_recovery", "a.b"})
        ensure(std::string("allowed: '") + fine + "'", !ALDirectorQol::isReservedSceneName(fine));
    ensure("an empty name is not reserved (it just cancels)", !ALDirectorQol::isReservedSceneName(""));

    // Migration: the previous build's kept file and its rotations move into crash_backups once.
    ensure("kept file migrates", ALDirectorQol::isLegacyKeptBackupName(".director_recovery_kept.xml"));
    ensure("rotated kept file migrates",
           ALDirectorQol::isLegacyKeptBackupName(".director_recovery_kept.xml.kept-20261014T120000"));
    ensure("rotated kept file with counter migrates",
           ALDirectorQol::isLegacyKeptBackupName(".director_recovery_kept.xml.kept-20261014T120000-3"));
    ensure("the live file is not legacy", !ALDirectorQol::isLegacyKeptBackupName(".director_recovery.xml"));
    ensure("a user scene is not legacy", !ALDirectorQol::isLegacyKeptBackupName("kept.xml"));
    ensure("a bare tag is not a backup", !ALDirectorQol::isLegacyKeptBackupName(".director_recovery_kept.xml.kept-"));
    // A migrated file takes a unique name, so two kept files with the same mtime second never collide.
    ALDirectorQol::BackupStamp stamp;
    stamp.year = 2026; stamp.month = 10; stamp.day = 14; stamp.hour = 12;
    std::vector<std::string> existing;
    for (S32 file = 0; file < 3; ++file)
        existing.push_back(ALDirectorQol::uniqueCrashBackupName(stamp, existing));
    ensure("three migrated files get three distinct names",
           existing[0] != existing[1] && existing[1] != existing[2] && existing[0] != existing[2]);
}

// Scene size cap: one fixed constant (64 MB) shared by save, autosave, crash-backup reads and manual load.
template<> template<> void director_qol_object::test<9>()
{
    ensure_equals("the cap is 64 MB", ALDirectorQol::kDirectorSceneMaxBytes, size_t(64) * 1024 * 1024);
    ensure("an empty scene is allowed", ALDirectorQol::sceneSizeAllowed(0));
    ensure("a typical scene is allowed", ALDirectorQol::sceneSizeAllowed(200 * 1024));
    ensure("a 9 MB scene is allowed now", ALDirectorQol::sceneSizeAllowed(size_t(9) * 1024 * 1024));
    ensure("exactly at the cap is allowed", ALDirectorQol::sceneSizeAllowed(ALDirectorQol::kDirectorSceneMaxBytes));
    ensure("one byte over the cap is refused", !ALDirectorQol::sceneSizeAllowed(ALDirectorQol::kDirectorSceneMaxBytes + 1));
    ensure("a huge scene is refused", !ALDirectorQol::sceneSizeAllowed(size_t(65) * 1024 * 1024));
    ensure("an oversized autosave waits at least five minutes", ALDirectorQol::kOversizedAutosaveRetrySeconds >= 300.0);
}

// Corrupt-backup cache key: name + size + mtime; a damaged file is re-read only when one of them changes.
template<> template<> void director_qol_object::test<10>()
{
    const std::string base = ALDirectorQol::backupFileKey("scene-20261005-090703.xml", 1234, 1000);
    ensure("same file, same key", base == ALDirectorQol::backupFileKey("scene-20261005-090703.xml", 1234, 1000));
    ensure("case-insensitive name", base == ALDirectorQol::backupFileKey("SCENE-20261005-090703.XML", 1234, 1000));
    ensure("a size change changes the key", base != ALDirectorQol::backupFileKey("scene-20261005-090703.xml", 1235, 1000));
    ensure("a modification-time change changes the key", base != ALDirectorQol::backupFileKey("scene-20261005-090703.xml", 1234, 1001));
    ensure("a different file changes the key", base != ALDirectorQol::backupFileKey("scene-20261005-090704.xml", 1234, 1000));
    ensure("size and mtime fields cannot be confused", ALDirectorQol::backupFileKey("a.xml", 12, 3) != ALDirectorQol::backupFileKey("a.xml", 1, 23));
}

// Liveness, pid width, the own-pid stale file, and the dedupe decision.
template<> template<> void director_qol_object::test<11>()
{
    // The parser accepts the full width the writer can produce (S32 max has 10 digits), and nothing wider.
    ensure_equals("10-digit pid (S32 max) round-trips", ALDirectorQol::liveRecoveryPid(
                  ALDirectorQol::liveRecoveryFileName(ALDirectorQol::kMaxLiveRecoveryPid)), ALDirectorQol::kMaxLiveRecoveryPid);
    ensure_equals("S32 max is 10 digits", ALDirectorQol::liveRecoveryFileName(2147483647),
                  std::string(".director_recovery-p2147483647.xml"));
    ensure_equals("a 9-digit pid parses", ALDirectorQol::liveRecoveryPid(".director_recovery-p123456789.xml"), 123456789);
    ensure_equals("a 10-digit pid parses", ALDirectorQol::liveRecoveryPid(".director_recovery-p1234567890.xml"), 1234567890);
    ensure_equals("a pid above S32 max is not a name", ALDirectorQol::liveRecoveryPid(".director_recovery-p2147483648.xml"), 0);
    ensure_equals("an 11-digit pid is not a name", ALDirectorQol::liveRecoveryPid(".director_recovery-p12345678901.xml"), 0);
    ensure_equals("the -<n> variant keeps the pid", ALDirectorQol::liveRecoveryPid(".director_recovery-p4321-2.xml"), 4321);
    ensure_equals("variant name round-trips", ALDirectorQol::liveRecoveryPid(ALDirectorQol::liveRecoveryFileName(4321, 17)), 4321);
    ensure_equals("counter 0 is the plain name", ALDirectorQol::liveRecoveryFileName(4321, 0), ALDirectorQol::liveRecoveryFileName(4321));
    ensure("a variant is a live file", ALDirectorQol::isLiveRecoveryFile(".director_recovery-p4321-1.xml"));

    // Live-file classification: lock files are never live files (so never backups), and are recognised on their own.
    for (const char* lock : {".director_recovery-p4321.xml.lock", ".director_recovery-p4321-1.xml.lock",
                             ".director_recovery.xml.lock", ".Director_Recovery-P4321.XML.LOCK"})
    {
        ensure(std::string("lock is not a live file: ") + lock, !ALDirectorQol::isLiveRecoveryFile(lock));
        ensure(std::string("lock is not collected: ") + lock, !ALDirectorQol::isPreviousSessionLiveFile(lock, 1, false));
        ensure_equals(std::string("lock has no pid: ") + lock, ALDirectorQol::liveRecoveryPid(lock), 0);
        ensure(std::string("lock is a lock file: ") + lock, ALDirectorQol::isLiveRecoveryLockFile(lock));
    }
    for (const char* other : {".director_recovery-p4321.xml", ".lock", "scene.xml.lock", "scene.lock",
                              ".director_recovery_kept.xml.lock", ".director_recovery-p4321.lock"})
        ensure(std::string("not a live lock file: ") + other, !ALDirectorQol::isLiveRecoveryLockFile(other));
    ensure_equals("lock name is the live name plus .lock", ALDirectorQol::liveRecoveryLockName(".director_recovery-p4321.xml"),
                  std::string(".director_recovery-p4321.xml.lock"));
    ensure("a lock file is not a reserved-name hole: it is hidden by the leading dot",
           ALDirectorQol::isReservedSceneName(".director_recovery-p4321.xml.lock"));
    ensure("a lock file is not a crash backup name", !ALDirectorQol::parseCrashBackupName(".director_recovery-p4321.xml.lock").valid);

    // Own-pid stale file: autosave never writes over it.
    const S32 mine = 4321;
    ensure_equals("no leftover: the plain pid name", ALDirectorQol::ownLiveRecoveryName(mine, {}),
                  std::string(".director_recovery-p4321.xml"));
    ensure_equals("other viewers' files do not matter", ALDirectorQol::ownLiveRecoveryName(mine,
                  {".director_recovery-p77.xml", ".director_recovery.xml"}), std::string(".director_recovery-p4321.xml"));
    ensure_equals("a stale same-pid file moves us to -1", ALDirectorQol::ownLiveRecoveryName(mine,
                  {".director_recovery-p4321.xml"}), std::string(".director_recovery-p4321-1.xml"));
    ensure_equals("compared case-insensitively", ALDirectorQol::ownLiveRecoveryName(mine,
                  {".DIRECTOR_RECOVERY-P4321.XML"}), std::string(".director_recovery-p4321-1.xml"));
    ensure_equals("taken variants are skipped", ALDirectorQol::ownLiveRecoveryName(mine,
                  {".director_recovery-p4321.xml", ".director_recovery-p4321-1.xml", ".director_recovery-p4321-2.xml"}),
                  std::string(".director_recovery-p4321-3.xml"));
    ensure("the chosen name is a live file of our pid",
           ALDirectorQol::liveRecoveryPid(ALDirectorQol::ownLiveRecoveryName(mine, {".director_recovery-p4321.xml"})) == mine);
    ensure("the chosen name never equals an existing file", ALDirectorQol::ownLiveRecoveryName(mine,
           {".director_recovery-p4321.xml"}) != ".director_recovery-p4321.xml");
    ensure("a leftover variant is collected at the next login",
           ALDirectorQol::isPreviousSessionLiveFile(".director_recovery-p4321-1.xml", mine, false));

    // Dedupe: sizes AND contents must match. Equal size / mtime alone never deletes anything.
    ensure("equal size and equal contents is a duplicate", ALDirectorQol::backupIsDuplicate(1000, 1000, true));
    ensure("equal size but different contents is not a duplicate", !ALDirectorQol::backupIsDuplicate(1000, 1000, false));
    ensure("different sizes are never duplicates", !ALDirectorQol::backupIsDuplicate(1000, 1001, true));
    ensure("different sizes and contents are not duplicates", !ALDirectorQol::backupIsDuplicate(1000, 999, false));
    ensure("an empty pair with equal contents flag is a duplicate", ALDirectorQol::backupIsDuplicate(0, 0, true));
    ensure("an unknown size is never a duplicate", !ALDirectorQol::backupIsDuplicate(-1, -1, true));
}

// Legacy plain ".director_recovery.xml" collection: only once its mtime is strictly older than the threshold.
template<> template<> void director_qol_object::test<12>()
{
    const S64 now = 1000000;
    ensure("a legacy file older than 10 minutes is collected",
           ALDirectorQol::legacyLiveFileOldEnough(now, now - 601));
    ensure("exactly 10 minutes is not yet old enough", !ALDirectorQol::legacyLiveFileOldEnough(now, now - 600));
    ensure("a fresh legacy file (an old running viewer may own it) is left", !ALDirectorQol::legacyLiveFileOldEnough(now, now - 60));
    ensure("a modification time in the future is not old", !ALDirectorQol::legacyLiveFileOldEnough(now, now + 3600));
    ensure_equals("the threshold is 10 minutes", ALDirectorQol::kLegacyLiveRecoveryMinAgeSeconds, S64(600));
}

// Lock liveness in one process: POSIX flock conflicts across two open file descriptions, and the Windows share-0
// CreateFileW refuses a second open. Both must report HELD for a rival and free the file on release.
template<> template<> void director_qol_object::test<13>()
{
    const std::string path = "aldirectorqol_livelock_test.lock";
    std::remove(path.c_str());
    ALDirectorQol::LiveLock owner, rival;
    ensure("the first owner takes the lock", ALDirectorQol::acquireLiveLock(path, owner) == ALDirectorQol::LockResult::ACQUIRED);
    ensure("the lock reads as held", owner.held());
    ensure("a rival is refused while the owner holds the lock",
           ALDirectorQol::acquireLiveLock(path, rival) == ALDirectorQol::LockResult::HELD);
    ensure("a refused rival holds nothing", !rival.held());
    ensure("taking a held lock again changes nothing",
           ALDirectorQol::acquireLiveLock(path, owner) == ALDirectorQol::LockResult::ACQUIRED);
    ALDirectorQol::releaseLiveLock(owner);
    ensure("released lock reads as not held", !owner.held());
    ensure("after release the rival can take the lock",
           ALDirectorQol::acquireLiveLock(path, rival) == ALDirectorQol::LockResult::ACQUIRED);
    ALDirectorQol::releaseLiveLock(rival);
    std::remove(path.c_str());
}

#if !LL_WINDOWS
// Release removes the lock file only while it still names the inode that was locked: a replacement made after the
// lock was taken belongs to someone else and must survive.
template<> template<> void director_qol_object::test<14>()
{
    const std::string path = "aldirectorqol_livelock_replace.lock";
    std::remove(path.c_str());
    ALDirectorQol::LiveLock owner;
    ensure("the owner takes the lock", ALDirectorQol::acquireLiveLock(path, owner) == ALDirectorQol::LockResult::ACQUIRED);
    std::remove(path.c_str());
    if (FILE* replacement = std::fopen(path.c_str(), "w"))
    {
        std::fputs("replacement", replacement);
        std::fclose(replacement);
    }
    ALDirectorQol::releaseLiveLock(owner);
    FILE* after = std::fopen(path.c_str(), "r");
    ensure("a replacement file survives the release", after != nullptr);
    if (after)
        std::fclose(after);
    std::remove(path.c_str());
}
#endif
} // namespace tut
