/**
 * @file aldirectorhotkeys.cpp
 * @brief Director hotkeys -- see aldirectorhotkeys.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "aldirectorhotkeys.h"

#include "indra_constants.h"        // KEY_F2..KEY_F8, MASK_NONE
#include "lldirectorcast.h"         // ACTION/CUT transport, marks
#include "llfloater.h"
#include "llfloaterreg.h"
#include "llflycamrecorder.h"       // take play/pause
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl

namespace
{
// an operator floater is up: instantiated AND on screen (same double check the
// heading-preview / ghost gates in LLActorMover use)
bool floater_open(const char* name)
{
    LLFloater* floaterp = LLFloaterReg::findInstance(name);
    return floaterp && floaterp->getVisible();
}
} // anonymous namespace

bool ALDirectorHotkeys::handleKey(KEY key, MASK mask)
{
    // cheap rejects first -- this runs on every keydown. Unmodified F2..F8
    // only (F1 stays Help; the KEY_F* constants are contiguous). Modified
    // combos (Ctrl/Alt/Shift) all fall through untouched, so Ctrl+Alt+F1-F9
    // (render feature toggles) and Alt+F4 (OS close) are never shadowed.
    if (mask != MASK_NONE || key < KEY_F2 || key > KEY_F8)
    {
        return false;
    }
    static LLCachedControl<bool> enabled(gSavedSettings, "DirectorHotkeysEnabled", true);
    if (!enabled)
    {
        return false;
    }

    // F2 (console toggle) is the ONE key that also works with no operator
    // floater open -- a toggle that could only ever close would be pointless.
    // Documented deviation from the all-keys gate; user gestures and focused
    // UI still pre-empt it upstream, and DirectorHotkeysEnabled kills it.
    if (key == KEY_F2)
    {
        LLFloaterReg::toggleInstance("director");
        return true;
    }

    // everything else fires only while the Director Console or Actor Mover
    // is open, so the transport keys can never hijack normal use
    if (!floater_open("director") && !floater_open("actor_mover"))
    {
        return false;
    }

    LLDirectorCast& cast = LLDirectorCast::instance();
    switch (key)
    {
    case KEY_F3:
        // ACTION, morphing to CUT while counting down / running -- exactly
        // the console's big button (LLFloaterDirector::onClickAction)
        if (cast.isRunning() || cast.isCountingDown())
        {
            cast.cut();
        }
        else
        {
            cast.action();
        }
        return true;

    case KEY_F4:
        // explicit CUT: also cancels a pending countdown; safe no-op when idle
        cast.cut();
        return true;

    case KEY_F5:
        // snap every marked cast member back to its mark (local placement)
        cast.resetToMarks();
        return true;

    case KEY_F6:
        // pose ghosts on/off (the shared panel's "Pose ghosts" checkbox)
        gSavedSettings.setBOOL("PathShowOnionSkin",
                               !gSavedSettings.getBOOL("PathShowOnionSkin"));
        return true;

    case KEY_F7:
        // flycam take play/pause (recorder no-ops without a loaded take)
        LLFlycamRecorder::instance().togglePlayback();
        return true;

    case KEY_F8:
        // snapshot every cast member's current position as its mark
        cast.setMarks();
        return true;

    default:
        return false;
    }
}
