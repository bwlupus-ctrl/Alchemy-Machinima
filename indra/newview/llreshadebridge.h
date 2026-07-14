/**
 * @file llreshadebridge.h
 * @brief Viewer side of the ReShade bridge: gathers per-frame camera +
 *        G-buffer state and publishes it through a C ABI (see
 *        llreshadebridgeabi.h) for the external sl_reshade_bridge add-on DLL.
 *
 * The viewer deliberately does NOT link or include the ReShade SDK. All
 * ReShade-facing logic (resource creation, copies, semantic bindings) lives
 * in the separate add-on, which resolves SLReShade_GetFrame from this
 * executable at runtime. That keeps the viewer buildable/shippable with no
 * ReShade dependency and lets the add-on iterate without viewer relinks.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix / Firestorm Viewer Source Code
 * $/LicenseInfo$
 */

#ifndef LL_LLRESHADEBRIDGE_H
#define LL_LLRESHADEBRIDGE_H

#include "llreshadebridgeabi.h"

// Singleton bridge. gatherFrame() is called once per frame from the display
// loop, right after the scene is finalized and before the UI is composited.
class LLReShadeBridge
{
public:
    static LLReShadeBridge& instance();

    // Capture the current frame's camera + G-buffer state into the published
    // ABI struct. Cheap: reads existing GL handles and camera state, no GPU
    // work. Safe to call every frame.
    void gatherFrame();

    // The struct handed out to the add-on via SLReShade_GetFrame().
    const SLReShadeFrame& getFrameData() const { return mFrame; }

private:
    LLReShadeBridge();

    SLReShadeFrame mFrame;
};

#endif // LL_LLRESHADEBRIDGE_H
