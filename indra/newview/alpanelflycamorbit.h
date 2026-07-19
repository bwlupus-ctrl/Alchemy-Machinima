/**
 * @file alpanelflycamorbit.h
 * @brief Flycam Orbit rig panel: enable, anchor (Cinematic Cam joint +
 *        target-selected), level horizon, min/max radius, smoothing, lens
 *        zoom, and up/left focus offsets.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Extracted from floater_flycam_orbit.xml so the standalone Flycam Orbit
 * floater and the Director Console's Camera tab embed the SAME panel
 * (panel_flycam_orbit.xml) instead of hand-copying the control group (which
 * had drifted -- the console copy was missing the joint combo, target-selected
 * check, and anchor hint). Every control is settings-backed (control_name), so
 * multiple live instances stay in sync through gSavedSettings; the panel holds
 * no state of its own and needs no C++ wiring beyond registration.
 */

#ifndef AL_ALPANELFLYCAMORBIT_H
#define AL_ALPANELFLYCAMORBIT_H

#include "llpanel.h"

class ALPanelFlycamOrbit final : public LLPanel
{
public:
    ALPanelFlycamOrbit();
    ~ALPanelFlycamOrbit() override = default;

    bool postBuild() override;
};

#endif // AL_ALPANELFLYCAMORBIT_H
