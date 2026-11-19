/**
 * @file aldiopterpresetbank.cpp
 * @brief [Ultimate Diopter] Persistent shadow storage for the Custom preset bank.
 *
 * See aldiopterpresetbank.h and doc/DIOPTER_SMART_UI_DESIGN.md §6.3 for the
 * full design rationale. This file owns:
 *   - the canonical owned-control name lists (44 diopter / 49 kaleido),
 *   - the crash-safe atomic writer (mirrors alcinelightrig.cpp:3284-3301),
 *   - load with seed-missing / preserve-unknown / atomic-upgrade semantics,
 *   - snapshot / restore / invalidate, with the CHECKED stale-rename that
 *     Codex delta-5 #5 required.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "aldiopterpresetbank.h"

#include "lldir.h"
#include "llfile.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "lluuid.h"
#include "llviewercontrol.h"

namespace
{
    const S32 BANK_SCHEMA_VERSION = 1;

    const char* const DIOPTER_BANK_FILENAME = "diopter_custom.llsd";
    const char* const KALEIDO_BANK_FILENAME = "diopter_kal_custom.llsd";

    // ---- canonical owned-control name lists ---------------------------
    // Diopter: the 44 fields written by LLPipeline::materializeDiopterPreset
    // (pipeline.cpp:13745-13810). Order is not significant; this must stay
    // the exact set the auto-Custom listener array in llviewerfloaterreg.cpp
    // installs against -- llviewerfloaterreg.cpp reads this same list rather
    // than keeping its own, so the two can never drift apart.
    const std::vector<std::string>& diopterOwnedNames()
    {
        static const std::vector<std::string> names = {
            "CineDiopterShape", "CineDiopterContent", "CineDiopterHollow",
            "CineDiopterArcLengthDeg", "CineDiopterBrokenCount", "CineDiopterCharacter",
            "CineDiopterGlassProfile", "CineDiopterIOR", "CineDiopterThickness",
            "CineDiopterRimWidth", "CineDiopterRimWarp", "CineDiopterRimCaustic",
            "CineDiopterRimDarken", "CineDiopterApertureShape", "CineDiopterBlades",
            "CineDiopterBladeCurve", "CineDiopterAnamorph", "CineDiopterCatEye",
            "CineDiopterSpotBlur", "CineDiopterBokehHighlight", "CineDiopterRingCount",
            "CineDiopterRingFold", "CineDiopterRingPhase", "CineDiopterTwistDeg",
            "CineDiopterLobeAmt", "CineDiopterLobeCount", "CineDiopterLobePhaseDeg",
            "CineDiopterGhostCount", "CineDiopterGhostSpacing", "CineDiopterTangentSmear",
            "CineDiopterRadialSmear", "CineDiopterGhostGain", "CineDiopterDispersion",
            "CineDiopterPatternMode", "CineDiopterPatternSegments", "CineDiopterPatternFeedDeg",
            "CineDiopterPatternZoom", "CineDiopterMotionMode", "CineDiopterHandheld",
            "CineDiopterHandheldSpeed", "CineDiopterSpinMode", "CineDiopterSpinSpeed",
            "CineDiopterSeamGhostPx", "CineDiopterPlacementMode",
        };
        return names;
    }

    // Kaleido: the 49 fields written by LLPipeline::materializeKaleidoPreset
    // (pipeline.cpp:14858-14932).
    const std::vector<std::string>& kaleidoOwnedNames()
    {
        static const std::vector<std::string> names = {
            "CineDiopterKalMode", "CineDiopterKalEdgeWrap", "CineDiopterKalProtectMode",
            "CineDiopterKalProtectAnchor", "CineDiopterKalMotionMode", "CineDiopterKalPulseTarget",
            "CineDiopterKalSpinMode", "CineDiopterKalSegments", "CineDiopterKalAngle",
            "CineDiopterKalTwist", "CineDiopterKalRingCount", "CineDiopterKalStarSharp",
            "CineDiopterKalShapeBias", "CineDiopterKalSourceAngle", "CineDiopterKalSourceZoom",
            "CineDiopterKalSourceOffsetX", "CineDiopterKalSourceOffsetY", "CineDiopterKalSourceSpin",
            "CineDiopterKalFXBand", "CineDiopterKalFXAmount", "CineDiopterKalFXFlow",
            "CineDiopterKalFXFreq", "CineDiopterKalProtectRadius", "CineDiopterKalProtectFeather",
            "CineDiopterKalDepthCut", "CineDiopterKalDepthFeatherM", "CineDiopterKalDepthInvert",
            "CineDiopterKalPingPong", "CineDiopterKalSpeed", "CineDiopterKalMotionAngle",
            "CineDiopterKalSweepRange", "CineDiopterKalPulseAmt", "CineDiopterKalWaveAmp",
            "CineDiopterKalWaveFreq", "CineDiopterKalPathFreqX", "CineDiopterKalPathFreqY",
            "CineDiopterKalPathPhase", "CineDiopterKalPathAmp", "CineDiopterKalSpinSpeed",
            "CineDiopterKalSpinTravel", "CineDiopterKalSpinDuration", "CineDiopterKalSpinBounce",
            "CineDiopterKalSpinDelay", "CineDiopterKalSeamSoften", "CineDiopterKalCellSizeVar",
            "CineDiopterKalCellBreathe", "CineDiopterKalCellSubdiv", "CineDiopterKalCellMerge",
            "CineDiopterKalCellTint",
        };
        return names;
    }

    const std::vector<std::string>& ownedNamesFor(ALDiopterBankKind kind)
    {
        return (kind == AL_BANK_KALEIDO) ? kaleidoOwnedNames() : diopterOwnedNames();
    }

    const char* filenameFor(ALDiopterBankKind kind)
    {
        return (kind == AL_BANK_KALEIDO) ? KALEIDO_BANK_FILENAME : DIOPTER_BANK_FILENAME;
    }

    struct BankState
    {
        bool mValid = false;              // holds a value trusted for restore
        bool mRefusedThisSession = false; // stale-rename itself failed; belt over the braces
        LLSD mFields;                     // raw "fields" map (may retain unknown keys)
    };

    BankState sBank[AL_BANK_KIND_COUNT];

    // Test-only redirect; empty means "use gDirUtilp / LL_PATH_USER_SETTINGS".
    std::string sTestDir;

    // §6.3 piece 2 -- the re-entry guard and transition-reason flag. Used to
    // live as LLPipeline statics; moved here (Codex review finding F6) so
    // alDiopterHandlePresetTransition below has no rendering-pipeline
    // dependency and can be linked into a lightweight unit test.
    bool sPresetMaterializing = false;
    ALPresetExit sPresetExit = AL_EXIT_USER_SELECTED_CUSTOM;

    std::string resolveBankPath(ALDiopterBankKind kind)
    {
        const std::string filename = filenameFor(kind);
        if (!sTestDir.empty())
        {
            return sTestDir + "/" + filename;
        }
        return gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, filename);
    }

    std::string resolveStalePath(ALDiopterBankKind kind)
    {
        return resolveBankPath(kind) + ".stale";
    }

    // Crash-safe write: temp -> flush -> close -> rename, all checked --
    // verbatim mirror of the fork's established pattern
    // (alcinelightrig.cpp:3284-3301). A partially written bank is worse
    // than none, so any failure removes the temp file and leaves the
    // existing on-disk bank (if any) untouched.
    bool writeAtomic(const std::string& path, const LLSD& content)
    {
        const std::string temporary = path + "." +
            LLUUID::generateNewID().asString() + ".tmp";
        llofstream output(temporary.c_str());
        if (!output.is_open())
        {
            return false;
        }
        const S32 serialized = LLSDSerialize::toPrettyXML(content, output);
        output.flush();
        const bool succeeded = serialized > 0 && output.good();
        output.close();
        if (!succeeded || output.fail() || LLFile::rename(temporary, path) != 0)
        {
            LLFile::remove(temporary);
            return false;
        }
        return true;
    }

    // §6.3 piece 2d -- seed any owned field missing from a loaded bank
    // (written by an older build that predates the field) with the CURRENT
    // live setting, so it is not silently left un-restored. Keys already in
    // `fields` that are NOT in `names` (written by a NEWER build) are left
    // exactly as-is here -- that is what makes write-back preservation
    // (below, in snapshot) possible.
    void seedMissing(LLSD& fields, const std::vector<std::string>& names)
    {
        for (const std::string& name : names)
        {
            if (!fields.has(name))
            {
                if (LLControlVariable* control = gSavedSettings.getControl(name))
                {
                    fields[name] = control->getValue();
                }
            }
        }
    }

    // Load one tool's bank from disk into sBank[kind]. The loader refuses --
    // leaves mValid false -- whenever "<path>.stale" exists or "<path>" is
    // absent or unparsable; that refusal is what makes a stale bank
    // permanently unloadable until a fresh snapshot replaces it.
    bool loadOne(ALDiopterBankKind kind)
    {
        BankState& state = sBank[kind];
        state.mValid = false;
        state.mFields = LLSD::emptyMap();

        const std::string path = resolveBankPath(kind);
        if (LLFile::isfile(resolveStalePath(kind)) || !LLFile::isfile(path))
        {
            return false;
        }

        llifstream input(path.c_str());
        if (!input.is_open())
        {
            return false;
        }

        LLSD root;
        if (LLSDSerialize::fromXML(root, input) == LLSDParser::PARSE_FAILURE || !root.isMap())
        {
            return false;
        }

        LLSD fields = root["fields"];
        if (!fields.isMap())
        {
            fields = LLSD::emptyMap();
        }

        const S32 version = root.has("version") ? root["version"].asInteger() : 0;
        const std::vector<std::string>& names = ownedNamesFor(kind);

        seedMissing(fields, names);

        if (version < BANK_SCHEMA_VERSION)
        {
            // §6.3 piece 2d -- upgrade ATOMICALLY: transform in memory, then
            // write through the same temp->rename path. Never migrate in
            // place. If the re-write fails, the (still-valid, old-version)
            // on-disk bank is left untouched and the seeded/upgraded map is
            // still used in memory for this session.
            LLSD upgraded;
            upgraded["version"] = BANK_SCHEMA_VERSION;
            upgraded["fields"] = fields;
            writeAtomic(path, upgraded);
        }

        state.mFields = fields;
        state.mValid = true;
        return true;
    }
}

const std::vector<std::string>& alDiopterOwnedControlNames(ALDiopterBankKind kind)
{
    return ownedNamesFor(kind);
}

void alDiopterLoadBanks()
{
    loadOne(AL_BANK_DIOPTER);
    loadOne(AL_BANK_KALEIDO);
}

bool alDiopterHasValidBank(ALDiopterBankKind kind)
{
    const BankState& state = sBank[kind];
    return state.mValid && !state.mRefusedThisSession;
}

bool alDiopterSnapshotCustomBank(ALDiopterBankKind kind)
{
    BankState& state = sBank[kind];
    const std::vector<std::string>& names = ownedNamesFor(kind);

    // Preserve any unknown keys already held in memory (e.g. seeded from a
    // newer-build bank on load) -- only the owned fields are overwritten.
    LLSD fields = state.mFields.isMap() ? state.mFields : LLSD::emptyMap();
    for (const std::string& name : names)
    {
        if (LLControlVariable* control = gSavedSettings.getControl(name))
        {
            fields[name] = control->getValue();
        }
    }

    LLSD root;
    root["version"] = BANK_SCHEMA_VERSION;
    root["fields"] = fields;

    if (!writeAtomic(resolveBankPath(kind), root))
    {
        return false;
    }

    // A fresh, successful snapshot is the only thing allowed to clear a
    // prior stale marker or session refusal (§6.3: cleared only by a new
    // successful snapshot, which writes a fresh bank and removes the
    // ".stale" file as its LAST step). That removal must be CHECKED
    // (Codex review finding F5): the content write above already succeeded,
    // but the loader refuses on ".stale" existing at all, regardless of
    // what the new (perfectly good) content says -- so if the removal
    // itself fails, this session would look valid while a restart would
    // silently refuse the very bank we just wrote. Treat that as an overall
    // snapshot failure: keep the invalid/refused state, return false, and
    // notify, exactly as the checked rename in alDiopterInvalidateBank does.
    const std::string stale_path = resolveStalePath(kind);
    if (LLFile::isfile(stale_path) && LLFile::remove(stale_path) != 0)
    {
        LL_WARNS() << "diopter: wrote a fresh Custom bank but could not clear "
                      "its stale marker on disk (kind=" << (int)kind << "); "
                      "refusing restores for this session" << LL_ENDL;
        state.mRefusedThisSession = true; // belt over the braces
        LLNotificationsUtil::add("DiopterPresetBankInvalidateFailed");
        return false;
    }

    state.mFields = fields;
    state.mValid = true;
    state.mRefusedThisSession = false;
    return true;
}

bool alDiopterRestoreCustomBank(ALDiopterBankKind kind)
{
    if (!alDiopterHasValidBank(kind))
    {
        return false;
    }

    const BankState& state = sBank[kind];
    const std::vector<std::string>& names = ownedNamesFor(kind);
    for (const std::string& name : names)
    {
        if (state.mFields.has(name))
        {
            gSavedSettings.setUntypedValue(name, state.mFields[name]);
        }
    }
    return true;
}

void alDiopterInvalidateBank(ALDiopterBankKind kind)
{
    BankState& state = sBank[kind];
    state.mValid = false; // in-memory, this session

    const std::string path = resolveBankPath(kind);
    if (LLFile::isfile(path))
    {
        // Same temp->rename atomicity as the writer; a rename is atomic on
        // both NTFS and POSIX, so there is no window where neither exists.
        // LLFile::rename returns an error code (llfile.cpp:894-899) and the
        // established writer CHECKS it (alcinelightrig.cpp:3284-3301) -- so
        // do we [Codex delta-5 #5]: on the same failing disk the rename
        // itself can fail, and ignoring that would leave the original bank
        // loadable after restart.
        if (LLFile::rename(path, resolveStalePath(kind)) != 0)
        {
            LL_WARNS() << "diopter: could not mark Custom bank stale on disk "
                          "(kind=" << (int)kind << "); refusing restores for "
                          "this session" << LL_ENDL;
            state.mRefusedThisSession = true; // belt over the braces
            LLNotificationsUtil::add("DiopterPresetBankInvalidateFailed");
        }
    }
}

bool alDiopterIsPresetMaterializing()
{
    return sPresetMaterializing;
}

ALPresetExit alDiopterConsumePresetExitReason()
{
    const ALPresetExit reason = sPresetExit;
    sPresetExit = AL_EXIT_USER_SELECTED_CUSTOM; // consume; default is safe
    return reason;
}

void alDiopterSetPresetExitReason(ALPresetExit reason)
{
    sPresetExit = reason;
}

ALScopedPresetMaterializing::ALScopedPresetMaterializing()
  : mPrior(sPresetMaterializing)
{
    sPresetMaterializing = true;
}

ALScopedPresetMaterializing::~ALScopedPresetMaterializing()
{
    sPresetMaterializing = mPrior; // restore, not clear
}

void alDiopterAbortPresetSelection(ALDiopterBankKind kind,
                                    const char* preset_control_name,
                                    U32 prior_preset)
{
    LL_WARNS() << "diopter: Custom bank not persisted; reverting preset selection"
               << LL_ENDL;

    // (a) revert under the guard. The caller's own first-statement guard
    //     check (alDiopterIsPresetMaterializing) is what stops this
    //     write-back from re-entering the transition handler and retrying
    //     the failing snapshot forever.
    {
        ALScopedPresetMaterializing guard;
        alDiopterSetPresetExitReason(AL_EXIT_USER_SELECTED_CUSTOM);
        gSavedSettings.setU32(preset_control_name, prior_preset);
    }

    // (b) materialization is skipped by the caller's early return.

    // (c) invalidate in memory AND on disk.
    alDiopterInvalidateBank(kind);

    // (d) tell the user what happened and what is still safe.
    LLNotificationsUtil::add("DiopterPresetBankWriteFailed");
}

ALPresetTransitionAction alDiopterHandlePresetTransition(
    ALDiopterBankKind kind, const char* preset_control_name, U32 newv, U32 oldv)
{
    // FIRST statement, matching the owned-setting listeners: without this,
    // the write-back revert inside alDiopterAbortPresetSelection would
    // re-enter here and retry the failing snapshot forever on a full or
    // read-only disk.
    if (sPresetMaterializing)
    {
        return AL_TRANSITION_NOOP;
    }

    const ALPresetExit reason = alDiopterConsumePresetExitReason();

    if (newv != 0)
    {
        // Custom -> non-Custom: snapshot BEFORE materializing so a stray
        // wheel/click into a preset can never destroy a hand-built Custom
        // look. non-Custom -> non-Custom (oldv != 0) skips the snapshot --
        // the bank already holds the Custom look from when Custom was last
        // left.
        if (oldv == 0 && !alDiopterSnapshotCustomBank(kind))
        {
            alDiopterAbortPresetSelection(kind, preset_control_name, oldv);
            return AL_TRANSITION_ABORTED;
        }
        return AL_TRANSITION_MATERIALIZE;
    }
    else if (reason == AL_EXIT_USER_SELECTED_CUSTOM)
    {
        alDiopterRestoreCustomBank(kind);
        return AL_TRANSITION_RESTORED;
    }
    else // AL_EXIT_AUTO_AFTER_EDIT
    {
        alDiopterSnapshotCustomBank(kind); // the edit IS the new Custom look
        return AL_TRANSITION_RESNAPSHOTTED;
    }
}

namespace ALDiopterPresetBankTest
{
    void setTestDir(const std::string& dir)
    {
        sTestDir = dir;
        for (BankState& state : sBank)
        {
            state = BankState();
        }
    }

    std::string bankPath(ALDiopterBankKind kind)
    {
        return resolveBankPath(kind);
    }

    std::string stalePath(ALDiopterBankKind kind)
    {
        return resolveStalePath(kind);
    }

    void resetTransitionState()
    {
        sPresetMaterializing = false;
        sPresetExit = AL_EXIT_USER_SELECTED_CUSTOM;
    }
}
