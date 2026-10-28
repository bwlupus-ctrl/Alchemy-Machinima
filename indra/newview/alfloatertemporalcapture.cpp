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
#include "alscrollfit.h"

ALFloaterTemporalCapture::ALFloaterTemporalCapture(const LLSD& key)
    : LLFloater(key)
{
}

bool ALFloaterTemporalCapture::postBuild()
{
    // The World Time Scale controls live entirely in the embedded shared panel
    // (ALPanelTemporalCapture, class "panel_temporal_capture"), which self-wires
    // via its LLPanelInjector -- the same panel the Director Console Temporal tab
    // hosts. Fit this thin host to the shared panel's measured content.
    fitToContent(getChild<LLPanel>("temporal_capture_panel"));
    return true;
}

void ALFloaterTemporalCapture::fitToContent(LLPanel* panel)
{
    ALScrollFit::fitPanelToChildren(panel, 10);
    const S32 height = panel->getRect().getHeight();
    const S32 top = getLocalRect().getHeight() - panel->getRect().mTop;
    const S32 target = top + height + 4;
    setResizeLimits(getMinWidth(), target);
    LLRect rect = getRect();
    rect.mBottom = rect.mTop - target;
    setShape(rect);
    // The follows-all child stretched with the host; retain its measured
    // content height instead of applying the host's height delta twice.
    const S32 panel_top = panel->getRect().mTop;
    panel->reshape(panel->getRect().getWidth(), height, true);
    panel->translate(0, panel_top - panel->getRect().mTop);
}
