/**
 * @file aldirectorhotkeys.h
 * @brief Director hotkeys: F-key transport for the machinima rig.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Global F-key hotkeys for the Director transport, dispatched from
 * LLViewerWindow::handleKey AFTER gestures (a user gesture bound to the same
 * key always wins) and BEFORE the menu accelerators. Hard-gated so the keys
 * can never hijack normal use: every key requires DirectorHotkeysEnabled, and
 * all but the console toggle additionally require the Director Console or
 * Actor Mover floater to be open. Keys and the F1-F12 audit they were chosen
 * from live in doc/DIRECTOR_HOTKEYS.md:
 *
 *   F2 toggle Director Console  F3 ACTION/CUT   F4 CUT
 *   F5 Reset to marks           F6 Pose ghosts  F7 take play/pause
 *   F8 Set marks
 */

#ifndef AL_ALDIRECTORHOTKEYS_H
#define AL_ALDIRECTORHOTKEYS_H

#include "stdtypes.h"       // KEY / MASK

namespace ALDirectorHotkeys
{
    // true = the key was consumed as a Director hotkey. Called on every
    // keydown, so it must reject non-hotkeys (and the gated-off state)
    // cheaply and return false, leaving the key its normal meaning.
    bool handleKey(KEY key, MASK mask);
}

#endif // AL_ALDIRECTORHOTKEYS_H
