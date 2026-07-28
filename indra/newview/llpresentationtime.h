/**
 * @file llpresentationtime.h
 * @brief Dual-time presentation clock for Temporal Capture (World Time Scale).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Landing 1 of Temporal Capture. See doc/TEMPORAL_CAPTURE_WORLD_TIME_SCALE_BRIEF.md.
 *
 * Owns the presentation clock that scales locally-evaluated cinematic dynamics
 * (skeletal animation + actor/ghost motion, object spin + object paths, texture
 * animation, particles, camera) together, from 0x (freeze) to 8x, while
 * wall/network/UI/watchdog time stays real.
 *
 * INVARIANTS (asserted by design, see brief section 2):
 *  - Wall/network time is NEVER multiplied by the presentation scale.
 *  - Every drive gate OFF => byte-identical to stock behavior.
 *  - tick() runs ONCE at the top of LLAppViewer::idle(), before any consumer;
 *    it freezes exactly one context that the whole idle->display pass reads.
 *  - This class is the SOLE writer of LLMotionController::sGlobalTimeFactor and
 *    restores it to 1.0 on Live / gate-off / shutdown.
 *  - It never touches FreezeTime / UseFreezeWorld (Freeze World is independent).
 */

#ifndef LL_LLPRESENTATIONTIME_H
#define LL_LLPRESENTATIONTIME_H

#include "llsingleton.h"
#include "lltimer.h"
#include "lltemporalframecontext.h"

class LLPresentationTime : public LLSingleton<LLPresentationTime>
{
    LLSINGLETON(LLPresentationTime);
    ~LLPresentationTime();

public:
    // Called once per viewer iteration at the top of idle(), off an independent
    // monotonic timer (NOT gFrameIntervalSeconds), BEFORE any presentation
    // consumer. Reads the Temporal* settings, advances the presentation clock,
    // freezes the frame context, and applies the animation global for the frame.
    void tick();

    // The immutable context for the current iteration (owner accessor).
    const LLTemporalFrameContext& frame() const { return mFrame; }

    // Hot-path accessor for consumers: the current frozen frame, or a static
    // LIVE default if the service has not ticked yet. A pointer deref, no
    // singleton lookup cost, safe because all consumers run on the main thread
    // after tick() in the same iteration.
    static const LLTemporalFrameContext& currentFrame();

    // Convenience predicates over currentFrame() for consumer call sites.
    static bool drives(LLTemporalFeature f)
    {
        const LLTemporalFrameContext& f_ = currentFrame();
        return f_.active() && f_.drives(f);
    }
    static F32 presentationDelta() { return (F32)currentFrame().presentation_delta; }

    // Restore all stock clocks (sGlobalTimeFactor = 1). Idempotent; called on
    // teardown and whenever the animation drive is not active.
    void restoreStockClocks();

private:
    U32 readDriveMask() const;

    LLTemporalFrameContext mFrame;

    LLTimer mMonotonic;                 // independent of gFrameIntervalSeconds
    F64  mPrevWall = 0.0;
    F64  mPresentationTime = 0.0;
    F64  mPrevPresentationTime = 0.0;
    U64  mGeneration = 0;
    bool mInitialized = false;
    bool mAnimGlobalApplied = false;    // are we currently driving sGlobalTimeFactor?
    F32  mSavedGlobalTimeFactor = 1.f;  // value to restore on release (default, or
                                        // an Advanced-menu slow-motion value)
};

#endif // LL_LLPRESENTATIONTIME_H
