/**
 * @file aldiopterpresetbank.h
 * @brief [Ultimate Diopter] Persistent shadow storage for the Custom preset bank.
 *
 * See doc/DIOPTER_SMART_UI_DESIGN.md §6.3 ("Preset-first, and presets that
 * tell the truth"). While a non-Custom preset is selected, the 44 diopter /
 * 49 kaleido preset-owned settings (gSavedSettings) are not authoritative
 * for the user's hand-built "Custom" look -- LLPipeline::materializeDiopter/
 * KaleidoPreset overwrite them every time the preset is applied. This bank
 * snapshots the Custom look to disk BEFORE the first materialize, so
 * selecting a preset can never destroy it, and restores it when the user
 * explicitly returns to Custom.
 *
 * Two independent banks -- one per tool -- so a snapshot failure or
 * invalidation in one tool can never affect the other's Custom look. Each
 * bank is one file in LL_PATH_USER_SETTINGS (global, not per-account -- see
 * §6.3 piece 2e), a versioned LLSD map:
 *     { "version": 1, "fields": { "<SettingName>": <value>, ... } }
 * written via the fork's established crash-safe temp->flush->close->rename
 * pattern (alcinelightrig.cpp:3284-3301) and never migrated in place.
 *
 * Canonical operation order (§6.3, stated once and used everywhere):
 *   check guard flag -> revert preset under the RAII guard -> skip
 *   materialization -> invalidate the bank in memory AND on disk -> notify.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * $/LicenseInfo$
 */

#ifndef LL_ALDIOPTERPRESETBANK_H
#define LL_ALDIOPTERPRESETBANK_H

#include "stdtypes.h" // U32

#include <string>
#include <vector>

enum ALDiopterBankKind
{
    AL_BANK_DIOPTER = 0,
    AL_BANK_KALEIDO = 1,
    AL_BANK_KIND_COUNT = 2,
};

// The canonical, single-source-of-truth list of preset-owned control names
// for the given tool -- the exact 44 (diopter) / 49 (kaleido) settings that
// LLPipeline::materializeDiopterPreset / materializeKaleidoPreset
// (pipeline.cpp) write, and that a non-Custom preset makes non-authoritative
// (design doc §1.5/§6.3). llviewerfloaterreg.cpp installs its auto-Custom
// listeners from this same list, so the write-list and the snapshot-list
// can never drift apart.
const std::vector<std::string>& alDiopterOwnedControlNames(ALDiopterBankKind kind);

// Load both banks' on-disk validity state into memory. Call exactly once,
// after settings load and strictly BEFORE the one-time startup sync (§6.3
// piece 3): the startup sync's "snapshot the bank IF ABSENT" test must be
// able to see an existing bank, or the first run after an upgrade would
// snapshot already-materialized preset values as if they were the user's
// Custom look, destroying it on the very upgrade meant to protect it.
void alDiopterLoadBanks();

// True if the given tool's bank currently holds a value trusted for
// restore -- either snapshotted this session, or loaded validly from disk.
// Used by the startup sync's "snapshot the bank IF ABSENT" test.
bool alDiopterHasValidBank(ALDiopterBankKind kind);

// Snapshot every owned control's CURRENT value into the given tool's bank
// and persist it atomically. On any I/O failure the on-disk bank is left
// exactly as it was (the temp file is removed) and this returns false --
// callers must treat that as an abort signal (§6.3 piece 2b/2f), never
// proceed to materialize.
bool alDiopterSnapshotCustomBank(ALDiopterBankKind kind);

// Restore the given tool's owned controls from its bank onto
// gSavedSettings. Refuses -- returns false, settings left untouched --
// whenever the bank is invalid, stale-marked, or absent. Callers are
// expected to call this only under an ALScopedPresetMaterializing guard.
bool alDiopterRestoreCustomBank(ALDiopterBankKind kind);

// Mark the given tool's bank untrustworthy: in memory for this session, and
// on disk by an atomic, CHECKED rename to "<path>.stale" (§6.3 piece 2c,
// Codex delta-5 #5 -- LLFile::rename's error return must be checked the
// same way the writer checks it). On a rename failure this also sets a
// session-scoped refusal flag, so restores keep refusing even though the
// on-disk marker did not land, and fires the DiopterPresetBankInvalidateFailed
// notification. Cleared only by a subsequent *successful* snapshot.
void alDiopterInvalidateBank(ALDiopterBankKind kind);

// ---- §6.3 piece 2: the transition-reason flag and the re-entry guard ------
//
// These used to be static members of LLPipeline (sDiopterPresetMaterializing /
// sDiopterPresetExit / ScopedPresetMaterializing), living there only because
// LLPipeline::materializeDiopterPreset/KaleidoPreset are the only other
// things that touch the guard. They live here now because the guard, the
// reason flag, and the transition decision below them are all pure "preset
// bank state machine" concepts with no rendering-pipeline dependency -- and
// keeping them LLPipeline-free is what lets
// tests/aldiopterpresetbank_test.cpp call the REAL production transition
// function directly instead of maintaining a hand-copied mirror of it
// (Codex review finding F6). LLPipeline's materializers now use
// ALScopedPresetMaterializing from here instead of a nested class of their
// own.

enum ALPresetExit
{
    AL_EXIT_USER_SELECTED_CUSTOM = 0, // combo commit to Custom -> RESTORE the bank
    AL_EXIT_AUTO_AFTER_EDIT,           // owned-setting edit/reset -> RE-SNAPSHOT instead
};

// True while a materializer (or the abort path's write-back revert) is
// writing settings, so the owned-setting/selection listeners know to ignore
// their own signal re-entry.
bool alDiopterIsPresetMaterializing();

// Reads the current reason flag and resets it to the safe default
// (AL_EXIT_USER_SELECTED_CUSTOM) in one step -- "consume" it, matching the
// spec's listener pseudocode. Call this exactly once per transition.
ALPresetExit alDiopterConsumePresetExitReason();

// Set by the owned-setting (auto-Custom) listener immediately before it
// writes the preset control back to 0, so the transition function below can
// tell "the user picked Custom in the combo" apart from "an edit/reset just
// materialized-then-flipped" -- see the canonical lifecycle in the .h/.cpp
// file header.
void alDiopterSetPresetExitReason(ALPresetExit reason);

// RAII guard around alDiopterIsPresetMaterializing(). The ctor SAVES the
// prior value and the dtor RESTORES it (never unconditionally clears): a
// naive set-true/set-false would clear the flag on an inner scope's exit and
// expose the OUTER scope's remaining writes to the listeners. Nesting
// happens for real -- the write-back revert inside alDiopterAbortPresetSelection
// runs inside the transition function, which may itself already be inside a
// materialize.
class ALScopedPresetMaterializing
{
public:
    ALScopedPresetMaterializing();
    ~ALScopedPresetMaterializing();
    ALScopedPresetMaterializing(const ALScopedPresetMaterializing&) = delete;
    ALScopedPresetMaterializing& operator=(const ALScopedPresetMaterializing&) = delete;
private:
    bool mPrior;
};

// §6.3 piece 2f -- the write-back revert failure path. LLControlVariable's
// commit signal fires POST-write (llcontrol.cpp:225-263), so a caller that
// just saw a failed snapshot cannot veto it with a bare `return`; it must
// revert the preset explicitly. Canonical operation order:
//   check guard flag (caller's first statement) -> revert under the RAII
//   guard -> skip materialization (caller's early return) -> invalidate the
//   bank in memory AND on disk -> notify.
// `preset_control_name` is "CineDiopterPreset" or "CineDiopterKalPreset".
// Shared by the interactive selection-transition path
// (alDiopterHandlePresetTransition, below) and the one-time startup sync
// (llviewerfloaterreg.cpp), which can hit the identical failure mode.
void alDiopterAbortPresetSelection(ALDiopterBankKind kind,
                                    const char* preset_control_name,
                                    U32 prior_preset);

// What alDiopterHandlePresetTransition did, so the caller knows whether it
// must materialize the new preset itself. The function never calls
// LLPipeline::materializeDiopterPreset/KaleidoPreset directly -- that would
// pull the whole rendering pipeline into this otherwise-lightweight,
// unit-testable unit.
enum ALPresetTransitionAction
{
    AL_TRANSITION_NOOP,          // re-entrant call (guard was set); do nothing
    AL_TRANSITION_MATERIALIZE,   // caller must materialize `newv` now
    AL_TRANSITION_RESTORED,      // bank was restored onto gSavedSettings
    AL_TRANSITION_RESNAPSHOTTED, // bank was re-snapshotted from the edited look
    AL_TRANSITION_ABORTED,       // snapshot failed; preset reverted, bank invalidated, notified
};

// The exact §6.3 piece 2b/2f preset-selection state machine, factored out so
// it is called by BOTH the real selection listeners (llviewerfloaterreg.cpp)
// and tests/aldiopterpresetbank_test.cpp -- the same function in both places,
// not a copy (Codex review finding F6). `newv`/`oldv` are the raw signal
// values for "CineDiopterPreset"/"CineDiopterKalPreset".
ALPresetTransitionAction alDiopterHandlePresetTransition(
    ALDiopterBankKind kind, const char* preset_control_name, U32 newv, U32 oldv);

// Test-only hooks for tests/aldiopterpresetbank_test.cpp. Never called from
// production code.
namespace ALDiopterPresetBankTest
{
    // Redirects both banks' files under `dir` instead of
    // LL_PATH_USER_SETTINGS, and clears all in-memory bank state (as if the
    // process had just started with nothing loaded). Passing an empty
    // string reverts to the real LL_PATH_USER_SETTINGS location.
    void setTestDir(const std::string& dir);

    // The resolved path for the given tool's bank file / its ".stale"
    // marker, honoring any setTestDir() override -- so the test can inspect
    // disk state directly.
    std::string bankPath(ALDiopterBankKind kind);
    std::string stalePath(ALDiopterBankKind kind);

    // Resets the re-entry guard and the transition-reason flag to their
    // startup defaults (not materializing, AL_EXIT_USER_SELECTED_CUSTOM) --
    // separate from setTestDir()'s bank-state reset since a test may need
    // one without the other.
    void resetTransitionState();
}

#endif // LL_ALDIOPTERPRESETBANK_H
