/**
 * @file alframelimiter.h
 * @brief [FrameLimiter] Frame rate cap: pure pacing maths, an injectable-clock
 *        pacer, and the platform entry points used by the main loop.
 *
 * The cap holds a steady frame-start to frame-start interval of 1/FPS using a
 * deadline chain: each frame's deadline is max(previous deadline + interval,
 * now). Everything here except the two platform functions at the bottom is
 * header-only and has no viewer or platform dependencies, so the real pacing
 * code (FramePacer) is unit tested against a fake clock.
 *
 * Rules (tested in alframelimiter_test.cpp):
 *  - true start-to-start pacing: wake-up lateness and work done after the
 *    wait are absorbed by the next interval, so the long-run average equals
 *    the cap instead of drifting under it;
 *  - no catch-up burst: a frame that ends at or after its deadline waits zero
 *    and re-anchors the chain to "now", so a slow frame is never repaid;
 *  - time already spent in the VSync swap counts as elapsed, so VSync and the
 *    limiter never double-wait;
 *  - the wait is a coarse sleep for the bulk plus a short busy-spin for the
 *    last stretch, because Windows timers wake late by up to a tick.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_FRAME_LIMITER_H
#define AL_FRAME_LIMITER_H

#include <cmath>

namespace ALFrameLimiter
{
constexpr int    MIN_FPS            = 10;
constexpr int    MAX_FPS            = 240;
constexpr int    DEFAULT_FPS        = 60;
// Busy-spin window at the end of a wait. With the high-resolution timer,
// wake-up jitter is well under 0.5 ms; the plain-timer fallback wakes up to a
// scheduler tick late, so it needs a wider spin window.
constexpr double SPIN_SECONDS_HIRES = 0.0005;
constexpr double SPIN_SECONDS_COARSE = 0.0025;

inline int clampFps(int fps)
{
    return fps < MIN_FPS ? MIN_FPS : (fps > MAX_FPS ? MAX_FPS : fps);
}

// Target frame-start to frame-start interval, in seconds.
inline double targetInterval(int fps)
{
    return 1.0 / static_cast<double>(clampFps(fps));
}

struct WaitPlan
{
    double sleepSeconds = 0.0;  // coarse timer sleep for the bulk
    double spinSeconds  = 0.0;  // then busy-spin for the remainder
    double total() const { return sleepSeconds + spinSeconds; }
};

struct NextStep
{
    double   deadline = 0.0;    // when the next frame is due to start
    WaitPlan plan;              // how to spend the time until then
};

// Pure deadline-chain step. Times are seconds on any monotonic clock.
//   havePrev/prevDeadline: the deadline of the previous frame (if any).
//   now: the time the current frame finished its work.
// deadline = max(prevDeadline + interval, now). If that is "now" (first frame,
// or the frame ran late past a full interval) nothing is waited and the chain
// re-anchors to now: no catch-up. Bad inputs fail open (no wait).
inline NextStep planNext(bool havePrev, double prevDeadline, double now,
                         double intervalSeconds, double spinSeconds)
{
    NextStep step;
    step.deadline = std::isfinite(now) ? now : 0.0;
    if (!havePrev || !(intervalSeconds > 0.0) || !std::isfinite(now) || !std::isfinite(prevDeadline))
    {
        return step;
    }
    const double candidate = prevDeadline + intervalSeconds;
    const double remaining = candidate - now;
    if (!(remaining > 0.0))
    {
        return step; // late (or on time): re-anchored to now, nothing to wait
    }
    const double spin = (spinSeconds > 0.0) ? (spinSeconds < remaining ? spinSeconds : remaining) : 0.0;
    step.deadline          = candidate;
    step.plan.sleepSeconds = remaining - spin;
    step.plan.spinSeconds  = spin;
    return step;
}

// Clock and waits, injectable so the real pacer runs under a fake clock.
struct IClock
{
    virtual ~IClock() = default;
    virtual double now() = 0;                      // seconds, monotonic
    virtual void   sleepFor(double seconds) = 0;   // coarse sleep (may oversleep)
    virtual void   spinUntil(double deadline) = 0; // busy-wait until now() >= deadline
};

// The pacing state machine used by the viewer. Main thread only.
class FramePacer
{
public:
    // Waits out the rest of the interval for the frame that just finished and
    // returns the seconds waited (0 when late). Call once per frame.
    double wait(int fps, IClock& clock, double spinSeconds)
    {
        const double now  = clock.now();
        const NextStep s  = planNext(mHavePrev, mDeadline, now, targetInterval(fps), spinSeconds);
        mDeadline = s.deadline;
        mHavePrev = true;
        if (!(s.plan.total() > 0.0))
        {
            return 0.0;
        }
        if (s.plan.sleepSeconds > 0.0)
        {
            clock.sleepFor(s.plan.sleepSeconds);
        }
        clock.spinUntil(s.deadline);
        return clock.now() - now;
    }

    void reset() { mHavePrev = false; }
    bool anchored() const { return mHavePrev; }
    double deadline() const { return mDeadline; }

private:
    double mDeadline = 0.0;
    bool   mHavePrev = false;
};

// ---- Platform side (alframelimiter.cpp) ------------------------------------

// Call at the end of the frame while the cap is enabled. Waits out the rest of
// the 1/fps deadline-chain interval and returns the seconds waited.
double endFrameWait(int fps);

// Release the timer handle (call from viewer cleanup).
void shutdown();
}

#endif // AL_FRAME_LIMITER_H
