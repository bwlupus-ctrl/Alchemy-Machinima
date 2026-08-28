/**
 * @file aldiopterpresetbank_test.cpp
 * @brief §6.3 state-machine tests for the Ultimate Diopter Custom preset bank.
 *
 * Covers (doc/DIOPTER_SMART_UI_DESIGN.md §6.3):
 *   - the four-transition lifecycle (Custom->non-Custom snapshot,
 *     non-Custom->non-Custom no-op, non-Custom->Custom restore under
 *     reason=USER, non-Custom->Custom re-snapshot under reason=AUTO);
 *   - the seven snapshot-failure-path assertions from piece 2f, including
 *     restore-still-refused-after-simulated-restart, the
 *     entered-exactly-once re-entry counter, and the abort path's checked
 *     rename actually renaming a pre-existing bank;
 *   - schema seed-missing / preserve-unknown / atomic-upgrade (piece 2d);
 *   - the two banks' independence (kind-scoped invalidation).
 *
 * This file drives the REAL production transition function,
 * alDiopterHandlePresetTransition (aldiopterpresetbank.h), via a real
 * signal connected to the real LLControlVariable for
 * "CineDiopter[Kal]Preset" -- the exact same function
 * llviewerfloaterreg.cpp's selection listeners call, not a hand-copied
 * mirror of it (Codex review finding F6: a mirror can drift from production
 * while staying green). The only thing this test does NOT exercise is the
 * final `LLPipeline::materializeDiopterPreset/KaleidoPreset` call a real
 * listener makes when the action is AL_TRANSITION_MATERIALIZE -- that
 * requires the full rendering-pipeline dependency tree, so this file checks
 * for the returned action instead of the slider side effects.
 *
 * LLNotificationsUtil is stubbed (see aldiopterpresetbank_test_stubs.cpp)
 * since the real one needs the full LLNotifications singleton and template
 * loading; the real LLDir/gDirUtilp come from the already-linked
 * `llfilesystem` library (CMakeLists.txt test_libs) -- this test never
 * takes that code path anyway, since it always calls
 * ALDiopterPresetBankTest::setTestDir() first (Codex review finding F1:
 * this file used to also define LLDir/gDirUtilp itself, colliding with the
 * real ones already linked in and producing LNK2005).
 */

// Precompiled header
#include "../llviewerprecompiledheaders.h"
// associated header
#include "../aldiopterpresetbank.h"

#include "../test/lltut.h"

#include "llcontrol.h"
#include "llfile.h"
#include "llsd.h"
#include "llsdserialize.h"

#include <boost/signals2.hpp>
#include <string>
#include <vector>

// The real global gSavedSettings that aldiopterpresetbank.cpp operates on --
// defined here directly rather than linking the (enormous) llviewercontrol.cpp,
// exactly as llcameraoperator_test.cpp does for the same reason.
LLControlGroup gSavedSettings("DiopterPresetBankTest");

namespace tut
{
namespace
{
    const char* const PRESET_DIOPTER = "CineDiopterPreset";
    const char* const PRESET_KALEIDO = "CineDiopterKalPreset";

    void declareSettings()
    {
        static bool initialized = false;
        if (initialized)
        {
            return;
        }
        initialized = true;

        for (const std::string& name : alDiopterOwnedControlNames(AL_BANK_DIOPTER))
        {
            gSavedSettings.declareF32(name, 0.f, "", LLControlVariable::PERSIST_NONDFT);
        }
        for (const std::string& name : alDiopterOwnedControlNames(AL_BANK_KALEIDO))
        {
            gSavedSettings.declareF32(name, 0.f, "", LLControlVariable::PERSIST_NONDFT);
        }
        gSavedSettings.declareU32(PRESET_DIOPTER, 0, "", LLControlVariable::PERSIST_NONDFT);
        gSavedSettings.declareU32(PRESET_KALEIDO, 0, "", LLControlVariable::PERSIST_NONDFT);
    }

    // Formulaic, distinctive per-field values so a round-trip can be checked
    // field-by-field without hand-listing all 44/49 names.
    void setDistinctiveValues(ALDiopterBankKind kind, F32 base)
    {
        F32 v = base;
        for (const std::string& name : alDiopterOwnedControlNames(kind))
        {
            gSavedSettings.setF32(name, v);
            v += 1.f;
        }
    }

    bool valuesMatch(ALDiopterBankKind kind, F32 base)
    {
        F32 v = base;
        for (const std::string& name : alDiopterOwnedControlNames(kind))
        {
            if (gSavedSettings.getF32(name) != v)
            {
                return false;
            }
            v += 1.f;
        }
        return true;
    }

    // Ensures a writable scratch directory (relative to the test binary's
    // working directory) unique to one test, cleaned of any bank/.stale
    // leftovers from a prior run of the same test.
    std::string scratchDir(const std::string& leaf)
    {
        LLFile::mkdir("aldiopterpresetbank_test_scratch");
        const std::string dir = "aldiopterpresetbank_test_scratch/" + leaf;
        LLFile::mkdir(dir);
        for (const char* name : { "diopter_custom.llsd", "diopter_custom.llsd.stale",
                                   "diopter_kal_custom.llsd", "diopter_kal_custom.llsd.stale" })
        {
            LLFile::remove(dir + "/" + name, 1 /*suppress_warning*/);
        }
        return dir;
    }

    // A directory whose PARENT does not exist -- llofstream cannot open a
    // temp file under it, so writeAtomic() fails immediately. Portable way
    // to simulate a disk-full/permissions snapshot failure without
    // depending on OS-specific ACL manipulation. (There is no similarly
    // portable way to force THIS SAME failure while a valid bank already
    // sits at the target path -- see test<10> for how the abort function's
    // checked-rename-of-an-existing-file branch is covered instead.)
    std::string unwritableDirUnder(const std::string& leaf)
    {
        return scratchDir(leaf) + "/missing_parent/leaf";
    }

    // Thin wrapper connecting the REAL alDiopterHandlePresetTransition
    // (aldiopterpresetbank.h) to a real LLControlVariable signal -- exactly
    // what llviewerfloaterreg.cpp's selection listeners do, minus the
    // materialize call (which needs LLPipeline). Records every returned
    // action so a test can assert both "how many times the signal fired"
    // and "how many of those calls actually did work".
    struct ScopedTransitionConnection
    {
        boost::signals2::connection mConn;
        std::vector<ALPresetTransitionAction> mActions;

        ScopedTransitionConnection(ALDiopterBankKind kind, const char* preset_name)
        {
            LLControlVariable* c = gSavedSettings.getControl(preset_name);
            mConn = c->getSignal()->connect(
                [this, kind, preset_name](LLControlVariable*, const LLSD& newv, const LLSD& oldv)
                {
                    mActions.push_back(alDiopterHandlePresetTransition(
                        kind, preset_name, (U32)newv.asInteger(), (U32)oldv.asInteger()));
                });
        }
        ~ScopedTransitionConnection() { mConn.disconnect(); }
    };

    LLSD readRawBank(const std::string& path)
    {
        LLSD root;
        llifstream in(path.c_str());
        if (in.is_open())
        {
            LLSDSerialize::fromXML(root, in);
        }
        return root;
    }

    void writeRawBank(const std::string& path, const LLSD& root)
    {
        llofstream out(path.c_str());
        LLSDSerialize::toPrettyXML(root, out);
    }
} // anonymous namespace

struct aldiopterpresetbank_data
{
    aldiopterpresetbank_data()
    {
        declareSettings();
        ALDiopterPresetBankTest::resetTransitionState();
    }
    ~aldiopterpresetbank_data()
    {
        ALDiopterPresetBankTest::setTestDir(""); // don't leak a dir across tests
        ALDiopterPresetBankTest::resetTransitionState();
    }
};
typedef test_group<aldiopterpresetbank_data> aldiopterpresetbank_group;
typedef aldiopterpresetbank_group::object aldiopterpresetbank_object;
aldiopterpresetbank_group aldiopterpresetbank_tests("aldiopterpresetbank");

// ---- 1: no bank on disk -> refuse; restore is a no-op ----------------------
template<> template<>
void aldiopterpresetbank_object::test<1>()
{
    set_test_name("no bank on disk: hasValidBank false, restore no-op, settings untouched");
    ALDiopterPresetBankTest::setTestDir(scratchDir("t1"));
    alDiopterLoadBanks();

    ensure("no valid bank at all on first run", !alDiopterHasValidBank(AL_BANK_DIOPTER));

    setDistinctiveValues(AL_BANK_DIOPTER, 42.f);
    ensure("restore refuses with no bank present", !alDiopterRestoreCustomBank(AL_BANK_DIOPTER));
    ensure("restore-refusal leaves settings untouched", valuesMatch(AL_BANK_DIOPTER, 42.f));
}

// ---- 2: snapshot/restore round-trip ----------------------------------------
template<> template<>
void aldiopterpresetbank_object::test<2>()
{
    set_test_name("snapshot captures current values; restore reproduces them exactly");
    ALDiopterPresetBankTest::setTestDir(scratchDir("t2"));
    alDiopterLoadBanks();

    setDistinctiveValues(AL_BANK_DIOPTER, 10.f);
    ensure("snapshot succeeds", alDiopterSnapshotCustomBank(AL_BANK_DIOPTER));
    ensure("bank now valid", alDiopterHasValidBank(AL_BANK_DIOPTER));
    ensure("bank file exists on disk",
           LLFile::isfile(ALDiopterPresetBankTest::bankPath(AL_BANK_DIOPTER)));

    setDistinctiveValues(AL_BANK_DIOPTER, 999.f); // simulate a preset overwrite
    ensure("restore succeeds", alDiopterRestoreCustomBank(AL_BANK_DIOPTER));
    ensure("restore reproduces the snapshotted values exactly",
           valuesMatch(AL_BANK_DIOPTER, 10.f));
}

// ---- 3: four-transition lifecycle, incl. AUTO-after-edit -------------------
// Driven through the REAL alDiopterHandlePresetTransition (via
// ScopedTransitionConnection), the same function
// llviewerfloaterreg.cpp's listener calls.
template<> template<>
void aldiopterpresetbank_object::test<3>()
{
    set_test_name("four transitions: snapshot, no-op, restore-on-USER, resnapshot-on-AUTO");
    ALDiopterPresetBankTest::setTestDir(scratchDir("t3"));
    alDiopterLoadBanks();
    // Force a known Custom(0) baseline BEFORE connecting, regardless of what
    // an earlier test in this process left CineDiopterPreset at -- gSavedSettings
    // is one global for the whole test binary.
    gSavedSettings.setU32(PRESET_DIOPTER, 0);
    ScopedTransitionConnection conn(AL_BANK_DIOPTER, PRESET_DIOPTER);

    // Custom(0) -> non-Custom(5): snapshot BEFORE materializing.
    setDistinctiveValues(AL_BANK_DIOPTER, 1.f);
    gSavedSettings.setU32(PRESET_DIOPTER, 5);
    ensure_equals("Custom->non-Custom action is MATERIALIZE",
                  conn.mActions.back(), AL_TRANSITION_MATERIALIZE);
    ensure("Custom->non-Custom snapshots the bank", alDiopterHasValidBank(AL_BANK_DIOPTER));
    ensure("snapshot captured the Custom-look values", valuesMatch(AL_BANK_DIOPTER, 1.f));

    // non-Custom(5) -> non-Custom(7): no re-snapshot -- change values first,
    // and confirm the bank on disk still holds the ORIGINAL Custom look.
    setDistinctiveValues(AL_BANK_DIOPTER, 2.f);
    gSavedSettings.setU32(PRESET_DIOPTER, 7);
    ensure_equals("non-Custom->non-Custom action is also MATERIALIZE (no snapshot)",
                  conn.mActions.back(), AL_TRANSITION_MATERIALIZE);
    ensure("restore after a non-Custom->non-Custom transition still returns the "
           "original Custom look (bank was NOT re-snapshotted from mode 7's junk)",
           alDiopterRestoreCustomBank(AL_BANK_DIOPTER) && valuesMatch(AL_BANK_DIOPTER, 1.f));

    // non-Custom(7) -> 0, reason=USER_SELECTED_CUSTOM: RESTORE the bank.
    setDistinctiveValues(AL_BANK_DIOPTER, 3.f); // stand-in for preset 7's look
    alDiopterSetPresetExitReason(AL_EXIT_USER_SELECTED_CUSTOM);
    gSavedSettings.setU32(PRESET_DIOPTER, 0);
    ensure_equals("user-selected Custom action is RESTORED",
                  conn.mActions.back(), AL_TRANSITION_RESTORED);
    ensure("user-selected Custom restores the bank, discarding preset 7's look",
           valuesMatch(AL_BANK_DIOPTER, 1.f));

    // Custom(0) -> non-Custom(9), then an owned-control edit auto-flips back
    // to 0 with reason=AUTO_AFTER_EDIT: RE-SNAPSHOT (the edit IS the new
    // Custom look), never restore the old one over it.
    gSavedSettings.setU32(PRESET_DIOPTER, 9);
    setDistinctiveValues(AL_BANK_DIOPTER, 4.f); // stand-in for the user's fresh edit
    alDiopterSetPresetExitReason(AL_EXIT_AUTO_AFTER_EDIT);
    gSavedSettings.setU32(PRESET_DIOPTER, 0);
    ensure_equals("AUTO-after-edit action is RESNAPSHOTTED",
                  conn.mActions.back(), AL_TRANSITION_RESNAPSHOTTED);
    ensure("AUTO-after-edit re-snapshots instead of restoring",
           valuesMatch(AL_BANK_DIOPTER, 4.f));
}

// ---- 4: snapshot failure leaves no partial write ---------------------------
template<> template<>
void aldiopterpresetbank_object::test<4>()
{
    set_test_name("snapshot failure: no partial bank file, bank stays invalid");
    ALDiopterPresetBankTest::setTestDir(unwritableDirUnder("t4"));
    alDiopterLoadBanks();

    ensure("snapshot fails when the temp file cannot be opened",
           !alDiopterSnapshotCustomBank(AL_BANK_DIOPTER));
    ensure("no bank file left behind by the failed write",
           !LLFile::isfile(ALDiopterPresetBankTest::bankPath(AL_BANK_DIOPTER)));
    ensure("bank remains invalid after a failed snapshot",
           !alDiopterHasValidBank(AL_BANK_DIOPTER));
}

// ---- 5: invalidate -> .stale on disk; refused this session AND after a
//         simulated restart; a later successful snapshot re-enables restore
//         (the 3 assertions rev 4 could not make, plus assertion 7) --------
template<> template<>
void aldiopterpresetbank_object::test<5>()
{
    set_test_name("invalidate marks .stale on disk; restore refuses now and after restart");
    const std::string dir = scratchDir("t5");
    ALDiopterPresetBankTest::setTestDir(dir);
    alDiopterLoadBanks();

    setDistinctiveValues(AL_BANK_DIOPTER, 5.f);
    ensure("initial snapshot succeeds", alDiopterSnapshotCustomBank(AL_BANK_DIOPTER));

    alDiopterInvalidateBank(AL_BANK_DIOPTER);
    ensure("bank path no longer exists after invalidate",
           !LLFile::isfile(ALDiopterPresetBankTest::bankPath(AL_BANK_DIOPTER)));
    ensure(".stale marker exists after invalidate",
           LLFile::isfile(ALDiopterPresetBankTest::stalePath(AL_BANK_DIOPTER)));
    ensure("hasValidBank false immediately after invalidate",
           !alDiopterHasValidBank(AL_BANK_DIOPTER));

    setDistinctiveValues(AL_BANK_DIOPTER, 6.f);
    ensure("restore refuses in the same session", !alDiopterRestoreCustomBank(AL_BANK_DIOPTER));
    ensure("refused restore leaves settings untouched", valuesMatch(AL_BANK_DIOPTER, 6.f));

    // Simulated restart: clear ALL in-memory state (setTestDir does this)
    // while pointing at the SAME on-disk directory, then reload from disk.
    ALDiopterPresetBankTest::setTestDir(dir);
    alDiopterLoadBanks();
    ensure("bank is still invalid after a simulated restart (the .stale marker "
           "reached disk, so the refusal survives)", !alDiopterHasValidBank(AL_BANK_DIOPTER));
    ensure("restore is still a no-op after the simulated restart",
           !alDiopterRestoreCustomBank(AL_BANK_DIOPTER));

    // Only a fresh, SUCCESSFUL snapshot clears the marker and re-enables restore.
    setDistinctiveValues(AL_BANK_DIOPTER, 7.f);
    ensure("a subsequent successful snapshot succeeds",
           alDiopterSnapshotCustomBank(AL_BANK_DIOPTER));
    ensure(".stale marker is gone after a successful snapshot",
           !LLFile::isfile(ALDiopterPresetBankTest::stalePath(AL_BANK_DIOPTER)));
    ensure("bank valid again", alDiopterHasValidBank(AL_BANK_DIOPTER));
    setDistinctiveValues(AL_BANK_DIOPTER, 999.f);
    ensure("restore now works and reproduces the new snapshot",
           alDiopterRestoreCustomBank(AL_BANK_DIOPTER) && valuesMatch(AL_BANK_DIOPTER, 7.f));
}

// ---- 6: guard prevents the write-back revert from re-entering the
//         transition handler -- assertions 1, 2 and 6 of the seven
//         failure-path assertions, through the REAL production function
//         (alDiopterHandlePresetTransition + alDiopterAbortPresetSelection)
// -----------------------------------------------------------------------
template<> template<>
void aldiopterpresetbank_object::test<6>()
{
    set_test_name("failed transition: preset reverted, owned settings untouched, "
                   "handler entered exactly once");
    ALDiopterPresetBankTest::setTestDir(scratchDir("t6")); // writable, for loadBanks()
    alDiopterLoadBanks();
    ScopedTransitionConnection conn(AL_BANK_DIOPTER, PRESET_DIOPTER);

    setDistinctiveValues(AL_BANK_DIOPTER, 11.f);
    gSavedSettings.setU32(PRESET_DIOPTER, 0); // start from a known Custom state
    conn.mActions.clear();

    // Now make every snapshot attempt fail, and try to leave Custom.
    ALDiopterPresetBankTest::setTestDir(unwritableDirUnder("t6b"));
    gSavedSettings.setU32(PRESET_DIOPTER, 3);

    ensure_equals("assertion 1: CineDiopterPreset equals its prior value",
                  gSavedSettings.getU32(PRESET_DIOPTER), 0);
    ensure("assertion 2: no owned setting changed", valuesMatch(AL_BANK_DIOPTER, 11.f));

    // The signal fires TWICE (the user's change, then the write-back
    // revert inside alDiopterAbortPresetSelection) but the guard means only
    // ONE of those calls does real work -- the re-entrant one returns
    // AL_TRANSITION_NOOP without even consuming the reason flag. The
    // re-entrant call's push_back happens WHILE the outer call is still
    // executing (the revert runs synchronously inside the outer call), so
    // it lands in the vector BEFORE the outer call's own result -- counted
    // here rather than indexed by position, so that ordering detail can't
    // make this assertion silently check the wrong slot.
    ensure_equals("the transition signal fired twice (change + revert)",
                  (int)conn.mActions.size(), 2);
    S32 noop_entries = 0;
    S32 aborted_entries = 0;
    for (ALPresetTransitionAction action : conn.mActions)
    {
        if (action == AL_TRANSITION_NOOP) { ++noop_entries; }
        else if (action == AL_TRANSITION_ABORTED) { ++aborted_entries; }
    }
    ensure_equals("assertion 6: handler entered exactly once with real work despite "
                  "the write-back revert's own signal firing", aborted_entries, 1);
    ensure_equals("the other firing was the guard-blocked re-entrant revert",
                  noop_entries, 1);
}

// ---- 7: seed a missing field with the LIVE value captured at load time ----
template<> template<>
void aldiopterpresetbank_object::test<7>()
{
    set_test_name("schema versioning: missing field seeded from the live setting at load");
    const std::string dir = scratchDir("t7");
    ALDiopterPresetBankTest::setTestDir(dir);

    const std::vector<std::string>& names = alDiopterOwnedControlNames(AL_BANK_DIOPTER);
    const std::string& missingField = names.front();

    // Hand-write a bank missing `missingField`, holding every OTHER field.
    LLSD fields;
    for (const std::string& name : names)
    {
        if (name != missingField)
        {
            fields[name] = 1.f;
        }
    }
    LLSD root;
    root["version"] = 1;
    root["fields"] = fields;
    writeRawBank(ALDiopterPresetBankTest::bankPath(AL_BANK_DIOPTER), root);

    gSavedSettings.setF32(missingField, 123.f); // the "live value at load time"
    alDiopterLoadBanks();

    gSavedSettings.setF32(missingField, 456.f); // a later, unrelated edit
    ensure("restore succeeds", alDiopterRestoreCustomBank(AL_BANK_DIOPTER));
    ensure_equals("the seeded field restores to the value live AT LOAD TIME, "
                  "not a later edit made before restore",
                  gSavedSettings.getF32(missingField), 123.f);
}

// ---- 8: unknown fields survive write-back; upgrade reaches disk ----------
template<> template<>
void aldiopterpresetbank_object::test<8>()
{
    set_test_name("schema versioning: unknown field preserved on write-back; "
                   "old version upgraded atomically on disk");
    const std::string dir = scratchDir("t8");
    ALDiopterPresetBankTest::setTestDir(dir);

    const std::vector<std::string>& names = alDiopterOwnedControlNames(AL_BANK_DIOPTER);
    LLSD fields;
    for (const std::string& name : names)
    {
        fields[name] = 1.f;
    }
    fields["SomeNewerBuildField"] = "sentinel-value"; // unknown to THIS build's schema
    LLSD root;
    root["version"] = 0; // older than the current schema -- forces an upgrade
    root["fields"] = fields;
    writeRawBank(ALDiopterPresetBankTest::bankPath(AL_BANK_DIOPTER), root);

    alDiopterLoadBanks();

    const LLSD afterLoad = readRawBank(ALDiopterPresetBankTest::bankPath(AL_BANK_DIOPTER));
    ensure_equals("load-time upgrade reaches disk: version bumped to 1",
                  afterLoad["version"].asInteger(), 1);
    ensure_equals("unknown field survives the load-time upgrade",
                  afterLoad["fields"]["SomeNewerBuildField"].asString(),
                  std::string("sentinel-value"));

    setDistinctiveValues(AL_BANK_DIOPTER, 50.f);
    ensure("a normal snapshot after loading succeeds",
           alDiopterSnapshotCustomBank(AL_BANK_DIOPTER));

    const LLSD afterSnapshot = readRawBank(ALDiopterPresetBankTest::bankPath(AL_BANK_DIOPTER));
    ensure_equals("unknown field still survives an ordinary write-back",
                  afterSnapshot["fields"]["SomeNewerBuildField"].asString(),
                  std::string("sentinel-value"));
}

// ---- 9: the two banks are independent --------------------------------------
template<> template<>
void aldiopterpresetbank_object::test<9>()
{
    set_test_name("diopter and kaleido banks are independent: invalidating one "
                   "never affects the other");
    ALDiopterPresetBankTest::setTestDir(scratchDir("t9"));
    alDiopterLoadBanks();

    setDistinctiveValues(AL_BANK_DIOPTER, 20.f);
    setDistinctiveValues(AL_BANK_KALEIDO, 30.f);
    ensure("diopter snapshot succeeds", alDiopterSnapshotCustomBank(AL_BANK_DIOPTER));
    ensure("kaleido snapshot succeeds", alDiopterSnapshotCustomBank(AL_BANK_KALEIDO));

    alDiopterInvalidateBank(AL_BANK_DIOPTER);

    ensure("diopter bank now invalid", !alDiopterHasValidBank(AL_BANK_DIOPTER));
    ensure("kaleido bank is UNAFFECTED by the diopter invalidation",
           alDiopterHasValidBank(AL_BANK_KALEIDO));

    setDistinctiveValues(AL_BANK_KALEIDO, 999.f);
    ensure("kaleido restore still works after the diopter bank was invalidated",
           alDiopterRestoreCustomBank(AL_BANK_KALEIDO) && valuesMatch(AL_BANK_KALEIDO, 30.f));
}

// ---- 10: the abort path's checked rename, exercised against a REAL
//          pre-existing bank (Codex review finding F6's refinement) --------
//
// test<4> proves alDiopterSnapshotCustomBank fails cleanly with NO bank
// present. That is the realistic shape of a disk failure, but it means
// alDiopterInvalidateBank's `if (LLFile::isfile(path))` branch is always
// skipped there -- the checked rename itself never actually runs. There is
// no portable, deterministic way to force a NEW write to fail while an
// existing valid file sits at that same path (the natural techniques all
// rely on OS-specific ACL/read-only semantics that behave differently
// between Windows and POSIX rename()). Instead, this test calls
// alDiopterAbortPresetSelection -- the exact function every real
// snapshot-failure path calls (the interactive listener above, AND the
// startup sync in llviewerfloaterreg.cpp) -- directly, with a real,
// previously-snapshotted bank already on disk, so its rename-to-.stale
// branch runs for real against a real file.
template<> template<>
void aldiopterpresetbank_object::test<10>()
{
    set_test_name("alDiopterAbortPresetSelection renames a PRE-EXISTING bank to "
                   ".stale, not just a no-op over an absent one");
    ALDiopterPresetBankTest::setTestDir(scratchDir("t10"));
    alDiopterLoadBanks();

    setDistinctiveValues(AL_BANK_DIOPTER, 60.f);
    ensure("existing bank is created", alDiopterSnapshotCustomBank(AL_BANK_DIOPTER));
    ensure("existing bank file is really on disk",
           LLFile::isfile(ALDiopterPresetBankTest::bankPath(AL_BANK_DIOPTER)));

    gSavedSettings.setU32(PRESET_DIOPTER, 5); // stand-in for "the failed transition's new id"

    alDiopterAbortPresetSelection(AL_BANK_DIOPTER, PRESET_DIOPTER, 0 /*prior*/);

    ensure_equals("abort reverts the preset control to the prior (Custom) value",
                  gSavedSettings.getU32(PRESET_DIOPTER), 0U);
    ensure("the PRE-EXISTING bank file was actually renamed away (not a no-op)",
           !LLFile::isfile(ALDiopterPresetBankTest::bankPath(AL_BANK_DIOPTER)));
    ensure(".stale now holds what was the real, valid bank content",
           LLFile::isfile(ALDiopterPresetBankTest::stalePath(AL_BANK_DIOPTER)));
    ensure("bank is invalid after the abort", !alDiopterHasValidBank(AL_BANK_DIOPTER));
}

} // namespace tut
