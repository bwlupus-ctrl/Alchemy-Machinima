#include "linden_common.h"

#include "../test/lltut.h"
#include "../aldirectorqol.h"

#include <algorithm>
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

// Autosave is never disabled: it always has a slot to write, and that slot is never an
// unmoved previous-session backup.
template<> template<> void director_qol_object::test<3>()
{
    ensure_equals("slot 1 is the plain live file", ALDirectorQol::liveRecoveryFileName(1),
                  std::string(".director_recovery.xml"));
    ensure_equals("slot 2 is the alternate live file", ALDirectorQol::liveRecoveryFileName(2),
                  std::string(".director_recovery_2.xml"));
    ensure_equals("nothing unmoved -> slot 1", ALDirectorQol::autosaveSlot({}), 1);
    ensure_equals("an unmoved slot-1 backup -> autosave moves to slot 2", ALDirectorQol::autosaveSlot({1}), 2);
    ensure_equals("slots 1 and 2 unmoved -> slot 3", ALDirectorQol::autosaveSlot({1, 2}), 3);
    ensure_equals("a free slot below an unmoved one is reused", ALDirectorQol::autosaveSlot({2}), 1);
    for (S32 count = 0; count < 40; ++count)
    {
        std::vector<S32> unmoved;
        for (S32 slot = 1; slot <= count; ++slot)
            unmoved.push_back(slot);
        const S32 chosen = ALDirectorQol::autosaveSlot(unmoved);
        ensure("autosave always has a slot (never disabled)", chosen >= 1);
        ensure("and it is never an unmoved backup",
               std::find(unmoved.begin(), unmoved.end(), chosen) == unmoved.end());
    }
    ensure_equals("slot name round-trips", ALDirectorQol::liveRecoverySlot(ALDirectorQol::liveRecoveryFileName(7)), 7);
    ensure_equals("slot 1 name parses", ALDirectorQol::liveRecoverySlot(".director_recovery.xml"), 1);
    ensure_equals("case-insensitive", ALDirectorQol::liveRecoverySlot(".Director_Recovery_2.XML"), 2);
    ensure_equals("the legacy kept file is not a live slot", ALDirectorQol::liveRecoverySlot(".director_recovery_kept.xml"), 0);
    ensure_equals("_1 is not a slot name", ALDirectorQol::liveRecoverySlot(".director_recovery_1.xml"), 0);
    ensure_equals("a user scene is not a slot", ALDirectorQol::liveRecoverySlot("director_recovery_2.xml"), 0);
    ensure_equals("a temp file is not a slot", ALDirectorQol::liveRecoverySlot(".director_recovery.xml.tmp"), 0);
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
    ensure("Restore button restores", ALDirectorQol::promptAction(0) == Prompt::RESTORE);
    ensure("Discard button discards", ALDirectorQol::promptAction(1) == Prompt::DISCARD);
    ensure("closing the window keeps the backup", ALDirectorQol::promptAction(-1) == Prompt::KEEP);
    ensure("an ignored prompt with no saved response keeps the backup", ALDirectorQol::promptAction(2) == Prompt::KEEP);
    ensure("only Discard deletes",
           ALDirectorQol::promptAction(-1) != Prompt::DISCARD && ALDirectorQol::promptAction(0) != Prompt::DISCARD &&
           ALDirectorQol::promptAction(7) != Prompt::DISCARD);

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
    const std::vector<std::string> doomed = ALDirectorQol::crashBackupsToPrune(seven, ALDirectorQol::kCrashBackupKeep, {});
    ensure_equals("keeps the newest 5 of 7", doomed.size(), size_t(2));
    ensure_equals("oldest is pruned first", doomed[0], std::string("scene-20261002-120000.xml"));
    ensure_equals("then the next oldest", doomed[1], std::string("scene-20261001-120000.xml"));
    ensure("under the cap prunes nothing", ALDirectorQol::crashBackupsToPrune({"a.xml", "b.xml"}, 5, {}).empty());

    // 8 equal-second backups: -N ordering decides what survives.
    ALDirectorQol::BackupStamp second;
    second.year = 2026; second.month = 10; second.day = 5; second.hour = 9; second.minute = 7; second.second = 3;
    std::vector<std::string> same_second;
    for (S32 counter = 0; counter < 8; ++counter)
        same_second.push_back(ALDirectorQol::crashBackupFileName(second, counter));
    const std::vector<std::string> doomed_same = ALDirectorQol::crashBackupsToPrune(same_second, 5, {});
    ensure_equals("three of eight equal-second backups pruned", doomed_same.size(), size_t(3));
    ensure_equals("the plain (oldest) name goes first", doomed_same[0], std::string("scene-20261005-090703-2.xml"));
    ensure("-7 survives", std::find(doomed_same.begin(), doomed_same.end(), "scene-20261005-090703-7.xml") == doomed_same.end());
    ensure("the plain name is among the pruned",
           std::find(doomed_same.begin(), doomed_same.end(), "scene-20261005-090703.xml") != doomed_same.end());

    // A protected file (in use / just restored) is never pruned.
    const std::vector<std::string> kept = ALDirectorQol::crashBackupsToPrune(
        seven, 5, {"SCENE-20261001-120000.XML"});
    ensure_equals("a protected backup survives pruning (case-insensitive)", kept.size(), size_t(1));
    ensure_equals("only the unprotected one is pruned", kept[0], std::string("scene-20261002-120000.xml"));
    ensure("an unparseable name is pruned first",
           ALDirectorQol::crashBackupsToPrune({"scene-20261001-120000.xml", "junk.xml"}, 1, {}).front() == "junk.xml");

    // Damaged files: .corrupt, unique, max 3, separate from the 5.
    ensure_equals("corrupt name", ALDirectorQol::uniqueCorruptName("scene-20261005-090703.xml", {}),
                  std::string("scene-20261005-090703.xml.corrupt"));
    ensure_equals("corrupt name is unique",
                  ALDirectorQol::uniqueCorruptName("scene-20261005-090703.xml", {"scene-20261005-090703.xml.corrupt"}),
                  std::string("scene-20261005-090703.xml.1.corrupt"));
    const std::vector<std::string> corrupt_doomed = ALDirectorQol::corruptBackupsToPrune(
        {"scene-20261001-120000.xml.corrupt", "scene-20261004-120000.xml.corrupt", "scene-20261002-120000.xml.corrupt",
         "scene-20261003-120000.xml.corrupt", "scene-20261005-120000.xml.corrupt"}, ALDirectorQol::kCorruptBackupKeep);
    ensure_equals("max 3 corrupt files kept", corrupt_doomed.size(), size_t(2));
    ensure_equals("oldest corrupt first", corrupt_doomed[0], std::string("scene-20261002-120000.xml.corrupt"));
    ensure_equals("then the next", corrupt_doomed[1], std::string("scene-20261001-120000.xml.corrupt"));
    ensure_equals("the cap is 5 backups", ALDirectorQol::kCrashBackupKeep, size_t(5));
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
} // namespace tut
