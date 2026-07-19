/**
 * @file alpanelflycamorbit.cpp
 * @brief Flycam Orbit rig panel -- see alpanelflycamorbit.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelflycamorbit.h"

#include "alpanelcinecamparams.h"   // alRegisterMachinimaResetControl()

// both the standalone Flycam Orbit floater and the Director Console Camera tab
// instantiate this class via <panel class="panel_flycam_orbit" .../>. Every
// control is bound to a gSavedSettings control_name, so there is nothing to
// wire up here -- the panel exists purely so the two hosts share one XML.
static LLPanelInjector<ALPanelFlycamOrbit> t_panel_flycam_orbit("panel_flycam_orbit");

ALPanelFlycamOrbit::ALPanelFlycamOrbit()
{
    // this panel's per-control reset buttons use the same global commit callback
    // as the Cinematic Camera panel; make sure it exists before our children build
    alRegisterMachinimaResetControl();
}

bool ALPanelFlycamOrbit::postBuild()
{
    return true;
}
