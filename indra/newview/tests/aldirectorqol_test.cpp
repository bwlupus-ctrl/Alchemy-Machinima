#include "linden_common.h"

#include "../test/lltut.h"
#include "../aldirectorqol.h"

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

template<> template<> void director_qol_object::test<3>()
{
    using Action = ALDirectorQol::RecoveryAction;
    ensure("no file is no-op", ALDirectorQol::recoveryAction(false, false) == Action::NONE);
    ensure("valid recovery is offered", ALDirectorQol::recoveryAction(true, true) == Action::OFFER_RESTORE);
    ensure("corrupt recovery is discarded", ALDirectorQol::recoveryAction(true, false) == Action::DISCARD_CORRUPT);
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

// Crash-recovery decisions: login handling per mode, keep-aside vs autosave, the
// restore button's eligibility, and the follow-up after a failed restore.
template<> template<> void director_qol_object::test<5>()
{
    using Login = ALDirectorQol::LoginRecoveryAction;
    ensure("no live file does nothing", ALDirectorQol::loginRecoveryAction(false, false, 0) == Login::NONE);
    ensure("Ask offers", ALDirectorQol::loginRecoveryAction(true, true, 0) == Login::OFFER);
    ensure("Restore automatically restores", ALDirectorQol::loginRecoveryAction(true, true, 1) == Login::RESTORE_AUTO);
    ensure("Don't restore keeps the backup aside", ALDirectorQol::loginRecoveryAction(true, true, 2) == Login::KEEP_ASIDE);
    ensure("unknown mode asks", ALDirectorQol::loginRecoveryAction(true, true, 9) == Login::OFFER);
    for (S32 mode = 0; mode <= 2; ++mode)
        ensure("a corrupt file is discarded in every mode",
               ALDirectorQol::loginRecoveryAction(true, false, mode) == Login::DISCARD_CORRUPT);
}

template<> template<> void director_qol_object::test<6>()
{
    // Login finds a crash backup: autosave is blocked until it is resolved.
    bool unresolved = true;
    ensure("unanswered backup blocks autosave", !ALDirectorQol::autosaveAllowed(unresolved));
    // Mode 2 / ticked Discard: the backup is moved to the kept file. A failed move
    // keeps the block (the backup must never be overwritten); a good one resumes autosave.
    ensure("failed keep stays blocked", !ALDirectorQol::autosaveAllowed(ALDirectorQol::stillUnresolved(unresolved, false)));
    unresolved = ALDirectorQol::stillUnresolved(unresolved, true);
    ensure("keep -> autosave resumes", ALDirectorQol::autosaveAllowed(unresolved));
    // The kept file is a separate file: with no unresolved live backup, autosave runs
    // whether or not a kept backup exists, and a resolved flag stays resolved.
    ensure("a kept file alone never blocks autosave", ALDirectorQol::autosaveAllowed(false));
    ensure("resolved stays resolved", !ALDirectorQol::stillUnresolved(false, false));
}

template<> template<> void director_qol_object::test<7>()
{
    using Source = ALDirectorQol::RestoreSource;
    using Follow = ALDirectorQol::RestoreFollowUp;
    ensure("nothing eligible", ALDirectorQol::restoreSource(false, false, false) == Source::NONE);
    ensure("this session's rolling autosave is never eligible",
           ALDirectorQol::restoreSource(false, true, false) == Source::NONE);
    ensure("a kept backup is eligible", ALDirectorQol::restoreSource(true, false, false) == Source::KEPT);
    ensure("rolling autosave does not hide a kept backup",
           ALDirectorQol::restoreSource(true, true, false) == Source::KEPT);
    ensure("an unresolved previous-session live file is eligible",
           ALDirectorQol::restoreSource(false, true, true) == Source::PREVIOUS_LIVE);
    ensure("the newer unresolved live file wins over the kept one",
           ALDirectorQol::restoreSource(true, true, true) == Source::PREVIOUS_LIVE);
    ensure("an unresolved flag without a file is not eligible",
           ALDirectorQol::restoreSource(false, false, true) == Source::NONE);

    ensure("successful restore consumes the backup",
           ALDirectorQol::restoreFollowUp(Source::PREVIOUS_LIVE, true) == Follow::CONSUME &&
           ALDirectorQol::restoreFollowUp(Source::KEPT, true) == Follow::CONSUME);
    ensure("failed auto-restore of the live backup is kept aside",
           ALDirectorQol::restoreFollowUp(Source::PREVIOUS_LIVE, false) == Follow::KEEP_ASIDE);
    ensure("failed restore of a kept backup leaves it in place",
           ALDirectorQol::restoreFollowUp(Source::KEPT, false) == Follow::LEAVE);
    ensure("no source leaves everything alone",
           ALDirectorQol::restoreFollowUp(Source::NONE, true) == Follow::LEAVE);
    // The failed auto-restore ends with autosave running again (kept, then resolved).
    ensure("failed auto-restore -> kept -> autosave resumes",
           ALDirectorQol::autosaveAllowed(ALDirectorQol::stillUnresolved(true, true)));
}

template<> template<> void director_qol_object::test<8>()
{
    const std::vector<std::string> none = ALDirectorQol::keptBackupsToPrune({"a", "b"}, 2);
    ensure("under the cap keeps everything", none.empty());
    const std::vector<std::string> doomed =
        ALDirectorQol::keptBackupsToPrune({"k-20261015T120000", "k-20261013T090000", "k-20261014T100000"}, 2);
    ensure_equals("one over the cap", doomed.size(), size_t(1));
    ensure_equals("the oldest goes first", doomed[0], std::string("k-20261013T090000"));
    ensure("a zero cap removes all",
           ALDirectorQol::keptBackupsToPrune({"x", "y"}, 0).size() == 2);
}
} // namespace tut
