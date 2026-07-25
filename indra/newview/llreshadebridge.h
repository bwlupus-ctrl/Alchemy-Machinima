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

    // Renderer-side facts accumulated while producing this frame. These stay
    // explicit: neither coverage nor a reset may be inferred from texture data.
    void noteMotionCoverage(U32 bits);
    void noteProjectionChange();
    // Visible-diffuse sidecar readiness. TWO stages, both required, because
    // either one alone is satisfiable while the buffer is garbage:
    //
    //   Seeded   - the classifying seed pass actually ran this frame. Without
    //              it every deferred-opaque pixel is unclassified, so the K
    //              channel is meaningless even if forward surfaces wrote fine.
    //              This is NOT implied by the setting or by the attachment
    //              existing: the seed program is optional and degrades to
    //              feature-off if it fails to compile on this driver.
    //   Resolved - the forward pool loop then completed over that same
    //              attachment on the main view.
    //
    // Validity must be asserted by the producer, never inferred from the
    // existence of a texture (contract v2 §4).
    void noteVisibleDiffuseSeeded();
    void noteVisibleDiffuseResolved();

    // The struct handed out to the add-on via SLReShade_GetFrame().
    const SLReShadeFrame& getFrameData() const { return mFrame; }

private:
    LLReShadeBridge();
    void noteResetEvent(U32 flags);

    SLReShadeFrame mFrame;
    SLReShadeTexture mLastTextures[8];
    U64 mRenderTargetGeneration;
    U32 mPendingMotionCoverage;
    bool mPendingVisibleDiffuseSeeded;
    bool mPendingVisibleDiffuseResolved;
    U32 mPendingResetFlags;
    U32 mResetEventCounter;
    bool mEverHadValidFrame;
    bool mLastFrameValid;
    bool mLastHDR;
    bool mLastSnapshot;
};

#endif // LL_LLRESHADEBRIDGE_H
