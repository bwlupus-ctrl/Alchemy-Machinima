/**
 * @file alfloatertemporalcapture.cpp
 * @brief Standalone Temporal Capture floater -- see alfloatertemporalcapture.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alfloatertemporalcapture.h"

ALFloaterTemporalCapture::ALFloaterTemporalCapture(const LLSD& key)
    : LLFloater(key)
{
}

bool ALFloaterTemporalCapture::postBuild()
{
    // The World Time Scale controls live entirely in the embedded shared panel
    // (ALPanelTemporalCapture, class "panel_temporal_capture"), which self-wires
    // via its LLPanelInjector -- the same panel the Director Console Temporal tab
    // hosts. Nothing host-specific to wire here.
    return true;
}
