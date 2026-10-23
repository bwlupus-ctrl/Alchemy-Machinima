/**
 * @file alframelimiter.h
 * @brief [FrameLimiter] Frame rate cap: pure pacing maths, an injectable-clock
 *        pacer, the pacing-mode decision and the platform entry points used by
 *        the main loop.
 *
 * Two ways to hold the cap, chosen per frame by the Limiter:
 *
 *  1. CPU DEADLINE PACING (the DEFAULT, and the fallback for everything else: VSync
 *     off, a cap that does not divide the refresh, an inexact or unknown refresh,
 *     an unfocused window, a driver that refused or ignored the interval, the
 *     FrameLimiterVSyncLock setting off). No swap-interval call is made at all.
 *     PRESENT PACING: each present waits (just before SwapBuffers) for
 *     max(previous deadline + interval, now), so the present interval is the cap
 *     interval whatever the work before the swap cost (the cure for the 24-44 ms
 *     judder of an end-of-frame wait whose swap time varies).
 *     NO LATE-WAKE RE-ANCHOR (dropped for good, VSync on or off). An earlier round re-anchored
 *     the deadline chain to a wake later than max(2 ms, interval/16) so that a short interval
 *     never followed a long one. Each re-anchor moves the chain later for good and nothing pulls
 *     it back, so the swap phase creeps onto the display's vblank edge (VSync on) or the DWM
 *     compositor's (VSync off, windowed) until a present misses it: a rare 4 ms oversleep at
 *     60 Hz cap 30 made three 50 ms intervals per 1,500 frames, where HEAD makes none (modelled
 *     and tested with VSync on AND off, test 30). Without it the chain keeps its plan: a late
 *     wake is absorbed by the next interval (one long interval, then one equally short one, the
 *     average stays on the cap), and VSync-off fullscreen intervals more than 2 ms off the cap
 *     are about 124 per 10k presents against HEAD's 3,304. The deadline chain still re-anchors
 *     to "now" after a frame that overshot its whole deadline (planNext: no catch-up burst).
 *     VSYNC ON AND THE CAP AT THE REFRESH = HEAD'S SCHEDULING, EXACTLY: with VSync on (the cached
 *     RenderVSyncEnable setting) and the cap at or above 0.995 x the EXACT refresh of the monitor the
 *     window is on (the window's own monitor, an exact num/den fraction, cached, re-read only when
 *     the monitor or the display configuration changes, no WGL call) the cap uses HEAD's end-of-frame
 *     wait with HEAD's deadline logic and nothing else (headSchedule(): no wait before the present,
 *     no glFlush, HEAD's raw 0.5 ms spin). That case is HEAD's behaviour step for step, so it can never
 *     exceed the cap whatever the driver does (VSync forced off, Fast Sync / Enhanced Sync, a wrong
 *     refresh): there is no detector to fool. Two earlier designs tried to do better there and failed.
 *     The present-wait chain settles at the worst work phase and walks the swap onto the vblank edge
 *     (a deferring driver at 144/144: 24-86 vs 3-10 missed vblanks per 10k, worse than HEAD). The
 *     "skip the wait because VSync holds the cap" bypass, with a detector for a driver that does not
 *     hold it, let the cap be exceeded for ever by up to 1.33x (the cached refresh was the PRIMARY
 *     display's: a window on a 75 Hz secondary with cap 60 ran 75 fps; a driver forcing VSync off at
 *     1.05-1.33x the cap was never caught). If the refresh is unknown or inexact (an integer
 *     EnumDisplaySettings fallback) the present pacing of this section is used instead.
 *     Everything else (VSync off, or VSync on with a cap below the refresh) uses present pacing.
 *     INPUT LATENCY: a present wait samples input about one cap interval before the present
 *     (the work is done, then it waits), where HEAD's end-of-frame wait sampled it about the
 *     wait time before (up to +30 ms at cap 30). That is the price of evenly spaced presents.
 *
 *     Either way, a cap that
 *     does not divide the refresh (60 on 144 Hz, 45 on 60 Hz) CANNOT be
 *     judder-free with VSync on, because the display only shows a new frame on a
 *     refresh: the presents land on refresh edges, so some intervals are N
 *     refreshes and some N+1. Use a cap that divides the refresh, or switch VSync
 *     off (the present interval is then exactly the cap).
 *
 *  2. DRIVER PACING (OPT-IN, setting FrameLimiterVSyncLock, default off). With VSync
 *     on, the exact refresh known and refresh/fps within 0.5% of a whole number
 *     N >= 1, the swap interval is set to N with wglSwapIntervalEXT and the driver
 *     blocks every present to the Nth vblank, so presents are exactly N refreshes
 *     apart with no phase to drift (59.94 Hz, 50 ppm clock drift, 144 Hz cap 48 ...).
 *     The vblank is never inferred from how long SwapBuffers took: that was tried
 *     twice and false-locked (37-74 fps over the cap), missed 59.94 Hz, missed
 *     non-blocking swaps and went stale on a refresh change.
 *
 *     It is only used while the window can be measured (visible, not minimised and
 *     the foreground window). An unfocused or covered window's swap can stop blocking,
 *     so then the cap runs on CPU pacing (strictly), and driver pacing is
 *     reconsidered on refocus.
 *
 *     Two layers keep a driver that does NOT honour N from running the cap fast:
 *
 *     A. CPU PRE-WAIT, ONLY WHILE SUSPICIOUS (the rate bound). The driver is
 *        "suspicious" once the MEAN of the last DRIVER_SUSPECT_WINDOW (8) present-to-present
 *        intervals is under 0.9 x N refreshes (an honoured N averages N refreshes, an ignored
 *        one 1 to N; 0.9 is the pre-wait's own fraction, so an ignored N it does not latch is
 *        already within the 1.11 x the pre-wait would bound it to). The mean is S/8 with S = the time
 *        between the present 8 back and this one, so one late SwapBuffers return (a long
 *        interval, then a short one) moves S by at most that return's lateness: 10 ms of a
 *        267 ms window at 60 Hz cap 30, and the return jitter of a 280-360 Hz display is a
 *        few ms of a window of 8 x N refreshes. A window that CONTAINS, OR IS PRECEDED BY, AN
 *        INTERVAL LONGER THAN N + 0.5 REFRESHES (a late return: 10 ms is 1.2 refreshes at 120 Hz cap
 *        120, 1.4 at 240/120, 3.6 at 360/120) is IGNORED, because a late return must never latch it
 *        (it raised missed vblanks from 62 to 100 per 10k at 120/120, 240/120 and 360/120). The long
 *        interval is usually the one just BEFORE the window: the late return is the window's first
 *        point, and its displacement is what pulls the mean down. An ignored N returns every refresh
 *        (or every ~N refreshes) and never produces such an interval. A render-ahead queue (Q = 1-3)
 *        refills after the long interval with a tail of short returns (2.4, 2.2, 6.3 ms ... at 120 Hz) that
 *        outlives the long interval's place in the ring and used to latch the pre-wait (52 vs 20 uneven
 *        intervals per 120 s at 120/120, Q1, a 10 ms late return every 100): so after ANY interval longer
 *        than N + 0.5 refreshes the latch is not evaluated for the next 2 x DRIVER_SUSPECT_WINDOW (16)
 *        returns (the queue-refill quarantine; an ignored N is then caught 16 presents later). The same
 *        quarantine runs at the start of every measurement (netReset(): a focus, mode, cap or environment
 *        change), because a queue fills then too (Q = 2-3 latched the pre-wait 60-75 ms after every start;
 *        a queue fills over about Q / (1 - work / interval) frames, 5-8 returns at Q = 3). Its price: the
 *        pre-wait of an ignored N starts up to 16 presents later, so it is bounded at about 1.17 x the cap
 *        (measured 1.15-1.17 at caps 10-30), not 1.11 x, before the net trips. A
 *        single short interval, or even a (short, normal) pair after a late return, is NOT
 *        evidence: the one-interval latch made a
 *        10 ms late return every 100 presents activate the pre-wait (89-382 uneven intervals
 *        per 300 s), an honoured driver at 280-360 Hz missed 400-2400 vblanks per 10k presents
 *        with it, and a consecutive-pair sum rule still latched on the (short, normal) pair
 *        after a late return longer than half a refresh (55 uneven intervals in 120 s at 60 Hz
 *        cap 30, test 32). An ignored N returns every refresh and is caught within 8 presents.
 *        A render-ahead refill after a long hitch is quarantined too. The latch is cleared by
 *        netReset() (a mode, focus, cap or environment change, or an unmeasurable
 *        frame) and ages out after one net window without another short interval.
 *        While suspicious, the CPU sleeps (no spin) before every present until the
 *        previous present returned + driverPrewaitFraction x the cap interval (0.90),
 *        which bounds an ignored N at about 1.11 x the cap (about 1.3 x above ~120 fps; about 1.17 x
 *        counting the start quarantine above) even before the net trips. While NOT suspicious there is NO pre-wait: a driver
 *        that blocks at its own flip (no render-ahead queue) would otherwise miss
 *        vblanks whenever a late swap return, a late timer wake and the swap submit
 *        latency together ate the 0.1 x interval margin (modelled: 89-815 missed
 *        vblanks per 10k presents at 60 Hz cap 30 .. 60). The pre-wait is recorded as
 *        sleep (RENDER_SLEEP), like the swap block.
 *
 *     B. COUNT-BASED SAFETY NET (the detector). Over a sliding window of focused driver
 *        wall time, netWindowSeconds() = max(6 s, 180 x the cap interval), the presents
 *        are counted; the net trips when count > window x max(cap, refresh/N) +
 *        DRIVER_NET_SLACK_PRESENTS (12). The window grows at low caps so the fixed
 *        12-present slack cannot hide the whole pre-wait excess (an ignored N under the
 *        pre-wait is 1.11 x: at cap 10 that is about 1.1 presents/s, so it needs >= 11 s).
 *        Why it cannot false-trip an honoured driver (for a render-ahead queue up to
 *        about 10 deep): it presents at most refresh/N flips per second, SwapBuffers
 *        returns run ahead of the flips by at most the driver's render-ahead queue Q
 *        (typically 2-4; 5-6 on some drivers), so a window holds at most
 *        window x refresh/N + Q + 1 returns. The 12-present slack absorbs Q up to about
 *        10 plus timing slop (modelled: Q 0-12, hitches, teleports and late returns never
 *        tripped). KNOWN LIMIT of this opt-in mode: a queue of about 16 or more frames can
 *        false-trip it (the code does not enforce a queue bound and WGL_EXT_swap_control
 *        gives none); the cost is a CPU-paced fallback and, after 3 trips, CPU pacing for
 *        the session, i.e. the default behaviour. It is a COUNT, not a running sum: a
 *        hitch only reduces the count, so no hitch, teleport or queue-refill pattern can
 *        bank credit toward a trip, while an ignored N accumulates excess without bound.
 *        What it catches: an ignored N gives about 1.11 x under the pre-wait, which trips
 *        in about one window to two. Excess below about 12 / (window x cap) (about 3% at 60 fps,
 *        7% at 30 fps and below) is NOT caught: it stays on driver pacing a few percent over
 *        the cap, bounded and cosmetic. The window is RESET (a fresh measurement
 *        starts) whenever the window is not measurable, and on any mode, N, cap, VSync,
 *        GL-context, display or monitor change; only fully focused stretches are
 *        measured. A trip counts toward the session give-up only because of this: the
 *        window it came from was focused throughout. Pending evidence is
 *        generation-tagged: if the environment, focus, cap or N changed between the
 *        present that tripped and the next frame's updatePacing(), it is discarded.
 *
 *     After a trip the cap runs on CPU pacing and retries driver pacing after ~10 s.
 *     After 3 trips in one environment it gives up for the session (log once). Each 60 s
 *     of trip-free driver time forgives one trip, so an occasional topmost cover (which is
 *     not detected as unmeasurable) cannot add up to a give-up over a long session.
 *     KNOWN LIMIT: a periodic undetected cover at an interval over ~70 s (say a 10 s topmost
 *     overlay every 80 s) trips once per cycle and is forgiven again before the next, so it
 *     never reaches the give-up and the mode flips between driver and CPU pacing every cycle
 *     (bounded: about 14 uneven intervals per 10k presents modelled).
 *     RE-ARM SEMANTICS: the failure latch (the N that was refused or ignored) is cleared
 *     by a cap disable, a focus or visibility change, and a GL context / VSync / display /
 *     monitor change. The trip count and the give-up (gaveUp) are cleared ONLY by a GL
 *     context / VSync / display / monitor change; a cap disable or a focus change does not
 *     clear a give-up.
 *
 *     Whether driver pacing holds depends on the driver honouring the swap interval: the
 *     call is verified by reading the interval back (a failure falls back to CPU pacing),
 *     but reading it back does not prove the cadence; that is what the net measures.
 *     LATENCY: driver mode lets the driver queue frames (its render-ahead, typically 2-3),
 *     so input-to-photon latency can grow by up to that many cap intervals (about 100 ms
 *     at cap 30), where CPU pacing keeps the queue empty. The pre-wait (when active) trims
 *     this.
 *     CLONED DISPLAYS: in duplicate mode two display paths can share one GDI source name;
 *     the first active path that matches the window's monitor wins (clones share one
 *     refresh rate in practice).
 *     TIMER MARGIN: only while suspicious does the pre-wait end (1 - fraction) x cap
 *     interval before the expected flip (3.3 ms at 60 Hz cap 30, 1.7 ms at 60 Hz cap 60),
 *     and then a late wake can miss a vblank on a no-queue driver; that is the price of
 *     bounding a driver that is already misbehaving.
 *
 * Everything here except the platform functions at the bottom is header-only
 * and has no viewer or platform dependencies, so the real pacing code
 * (FramePacer, Limiter) is unit tested against a fake clock and a fake swap
 * control.
 *
 * Rules of the CPU pacer (tested in alframelimiter_test.cpp):
 *  - true start-to-start pacing: wake-up lateness and work done after the
 *    wait are absorbed by the next interval, so the long-run average equals
 *    the cap instead of drifting under it;
 *  - no catch-up burst: a frame that ends at or after its deadline waits zero
 *    and re-anchors the chain to "now", so a slow frame is never repaid;
 *  - a late wake is not re-anchored (see the top): the chain keeps its plan, a wake that is
 *    late by d makes the next interval d shorter (one long, one short, the average on the cap),
 *    which keeps the long-run average on the cap and the swap phase from creeping onto a vblank
 *    edge;
 *  - every present is paced against its own deadline (a counter, not a flag),
 *    so two presents in one frame (snapshot, resize) cannot burst;
 *  - the wait is a coarse sleep for the bulk plus a short busy-spin for the
 *    last stretch, because Windows timers wake late by up to a tick;
 *  - the wait is taken just before the present (SwapBuffers), so the present
 *    interval is the cap interval no matter how much the work before the swap
 *    varies. A frame that never presents is paced at the end of the frame
 *    instead. (Except VSync on with the cap at the refresh: HEAD's end-of-frame wait for every
 *    frame, see the top of the file.)
 *
 * Cap OFF is inert: no wait, no swap-interval change, nothing recorded. With the
 * cap on and FrameLimiterVSyncLock off (the default) the swap interval is not touched
 * either (no WGL call at all); the only window call is the cached read of the window
 * monitor's refresh (and only with VSync on), which HEAD's scheduling needs. The swap interval is only changed while the
 * cap is on AND the lock is on (or while a driver interval set by the cap is still
 * applied), and the viewer's own VSync setting is restored (setInterval(0)) as soon
 * as the cap, the lock, the mode or VSync stops needing the driver interval.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_FRAME_LIMITER_H
#define AL_FRAME_LIMITER_H

#include <cmath>
#include <functional>

namespace ALFrameLimiter
{
constexpr int    MIN_FPS            = 10;
constexpr int    MAX_FPS            = 240;
constexpr int    DEFAULT_FPS        = 60;
// Busy-spin window at the end of a wait. With the high-resolution timer,
// wake-up jitter is about 0.5 ms; 1 ms leaves margin for the odd late wake
// (about 3% of a core at 30 fps). The plain-timer fallback wakes up to a
// scheduler tick late, so it needs a wider spin window. The window is capped
// at 1/8 of the interval (see spinWindow) so a high cap does not spin a core.
constexpr double SPIN_SECONDS_HIRES = 0.001;
constexpr double SPIN_SECONDS_COARSE = 0.0025;
// VSync on and the cap at or above this fraction of the window monitor's EXACT refresh: HEAD's
// scheduling, exactly (headSchedule(), see the top of the file).
constexpr double HEAD_SCHEDULE_RATIO = 0.995;
// HEAD's spin window (high-resolution timer): HEAD's scheduling uses it, so that case is HEAD step for step.
constexpr double SPIN_SECONDS_HEAD_HIRES = 0.0005;
// Driver pacing: refresh/fps must be within this relative error of a whole
// number N. The delivered rate is refresh/N, so this is also the worst fps error.
constexpr double DRIVER_RATIO_TOLERANCE = 0.005;

// ---- Driver pacing (opt-in): the CPU pre-wait (rate bound) -------------------
// While the driver is "suspicious" (the mean of the last DRIVER_SUSPECT_WINDOW intervals is under
// 0.9 x N refreshes, see below), every present first waits until the previous present returned +
// driverPrewaitFraction(I) x I (I = the cap interval). Honoured N: free (the swap blocks to
// the Nth vblank anyway). Ignored N: the rate is bounded at about 1 / fraction x the cap.
// Not suspicious: no pre-wait at all (it costs a missed vblank on a no-queue driver whenever
// late wakes eat the margin).
constexpr double DRIVER_PREWAIT_FRACTION   = 0.90;
// While it pre-waits, the wait ends at least this long before the expected flip, so a 0.5 ms
// timer wake cannot make the swap miss the vblank at very high caps (10% of a 6.9 ms interval
// at 144 fps is only 0.7 ms). It lowers the fraction only for intervals below ~15 ms (caps
// above ~66).
constexpr double DRIVER_PREWAIT_MARGIN_MIN = 0.0015;
constexpr double DRIVER_PREWAIT_FRACTION_FLOOR = 0.5;
// The driver is suspicious when the mean of the last DRIVER_SUSPECT_WINDOW present-to-present
// intervals is under DRIVER_SUSPECT_RATIO x N refreshes (an honoured N averages N; an ignored one
// 1 to N). 0.9 is the pre-wait's own fraction: a rate under 1.11 x the cap is what the pre-wait
// would bound it to anyway, so it needs no latch. A late return only moves the window's total by its own
// lateness, so timer / return jitter cannot latch an honoured driver, and a window that contains or is
// preceded by an interval longer than N + 0.5 refreshes (a late return) is ignored outright; an ignored N
// is caught within the window.
constexpr double DRIVER_SUSPECT_RATIO = 0.9;
constexpr int    DRIVER_SUSPECT_WINDOW = 8;
// The ring holds one return more than the window needs (WINDOW + 1 for the mean) so the interval that
// PRECEDES the window is known too: a late return is the long interval just before the window whose
// first point it is, and it is that point's displacement that pulls the window's mean down. The latch
// is only evaluated with the ring full, so no window starts on a point whose lateness is unknown (the
// very first present after a reset can be a late return).
constexpr int    DRIVER_SUSPECT_RING   = DRIVER_SUSPECT_WINDOW + 2;
// The latch is not evaluated for this many returns after an interval longer than N + 0.5 refreshes (a late return)
// and after the start of every measurement (a focus, mode, cap or environment change; netReset()): a render-ahead
// queue (Q = 1-3) refills with a burst of short returns that outlives the long interval's place in the ring and would
// read as an ignored N (a queue fills over about Q / (1 - work / interval) frames: 5-8 returns at Q = 3 with work at
// 40% of the interval, so 16 covers it). An ignored N returns short for ever and is caught right after.
constexpr int    DRIVER_SUSPECT_QUARANTINE = 2 * DRIVER_SUSPECT_WINDOW;

// ---- Driver pacing (opt-in): the count-based safety net ----------------------
// Trip when, within the last netWindowSeconds() of focused driver-mode wall time, the number of
// presents exceeds window x max(cap, refresh / N) + DRIVER_NET_SLACK_PRESENTS. An honoured swap
// interval flips at most refresh / N times a second; SwapBuffers returns run ahead of the flips
// by the render-ahead queue Q, so a window holds at most window x refresh / N + Q + 1 returns.
// 12 slack absorbs a queue about 10 deep plus slop (late returns bunch up by well under one
// frame); a queue of about 16 or more can false-trip it (known limit of the opt-in mode). It
// counts presents, so hitches only reduce it. An ignored interval accumulates excess without
// bound. The window is max(6 s, DRIVER_NET_WINDOW_FRAMES x I): 180 cap intervals keep the fixed
// slack below the pre-wait's 1.11 x excess at every cap (at cap 10 the 6 s window held only
// 6 presents of excess, so an ignored N ran at 11 fps for ever).
constexpr double DRIVER_NET_WINDOW_SECONDS = 6.0;
constexpr double DRIVER_NET_WINDOW_FRAMES  = 180.0;
constexpr int    DRIVER_NET_SLACK_PRESENTS = 12;
constexpr int    DRIVER_NET_CAPACITY       = 4096;  // presents held; above any threshold (max(6 s, 180 I) x cap <= 1440, 6 s x 540 Hz + 12 = 3252)
// Re-arm: after a failure (the driver ignored or refused the interval) the cap runs on CPU
// pacing and retries driver pacing after this long. After DRIVER_MAX_TRIPS failures in one
// environment (GL context, VSync, display configuration, monitor) it gives up for the
// session. Every DRIVER_TRIP_DECAY_SECONDS of failure-free driver time forgives one trip (an
// occasional topmost cover that is not detected must not add up to a give-up). A cap disable,
// a focus or visibility change and an environment change clear the failure latch (the N that
// failed); only an environment change clears the trip count and the give-up at once.
constexpr double DRIVER_RETRY_SECONDS      = 10.0;
constexpr int    DRIVER_MAX_TRIPS          = 3;
constexpr double DRIVER_TRIP_DECAY_SECONDS = 60.0;

inline int clampFps(int fps)
{
    return fps < MIN_FPS ? MIN_FPS : (fps > MAX_FPS ? MAX_FPS : fps);
}

// Target frame-start to frame-start interval, in seconds.
inline double targetInterval(int fps)
{
    return 1.0 / static_cast<double>(clampFps(fps));
}

// Spin window for a given interval: the base window, at most interval/8.
inline double spinWindow(double baseSeconds, double intervalSeconds)
{
    const double cap = intervalSeconds * 0.125;
    return (baseSeconds < cap) ? baseSeconds : cap;
}

// Pre-wait fraction for a cap interval: DRIVER_PREWAIT_FRACTION, but the pre-wait must end at
// least DRIVER_PREWAIT_MARGIN_MIN before the expected flip, so the fraction is
// min(0.90, 1 - margin / interval), never below DRIVER_PREWAIT_FRACTION_FLOOR. 0.90 for caps
// up to ~66 fps; 0.78 at 144 fps.
inline double driverPrewaitFraction(double intervalSeconds)
{
    if (!(intervalSeconds > 0.0))
    {
        return DRIVER_PREWAIT_FRACTION;
    }
    double f = 1.0 - DRIVER_PREWAIT_MARGIN_MIN / intervalSeconds;
    if (f > DRIVER_PREWAIT_FRACTION)
    {
        f = DRIVER_PREWAIT_FRACTION;
    }
    if (f < DRIVER_PREWAIT_FRACTION_FLOOR)
    {
        f = DRIVER_PREWAIT_FRACTION_FLOOR;
    }
    return f;
}

// The safety net's observation window for a cap: max(6 s, 180 cap intervals).
inline double netWindowSeconds(int fps)
{
    const double w = DRIVER_NET_WINDOW_FRAMES * targetInterval(fps);
    return (w > DRIVER_NET_WINDOW_SECONDS) ? w : DRIVER_NET_WINDOW_SECONDS;
}

// ---- Refresh rate and the pacing-mode decision -------------------------------

// The refresh rate of the monitor the window is on. exact == true when it came
// from the display configuration as a fraction (60000/1001); false for an
// integer fallback (EnumDisplaySettings reports 59.94 Hz as 59 or 60).
struct Refresh
{
    double hz    = 0.0;
    bool   exact = false;
    bool known() const { return hz > 0.0 && std::isfinite(hz); }
};

inline Refresh refreshFromFraction(unsigned numerator, unsigned denominator)
{
    Refresh r;
    if (numerator > 0 && denominator > 0)
    {
        r.hz    = static_cast<double>(numerator) / static_cast<double>(denominator);
        r.exact = true;
    }
    return r;
}

inline Refresh refreshFromInteger(int hz)
{
    Refresh r;
    if (hz > 0)
    {
        r.hz    = static_cast<double>(hz);
        r.exact = false;
    }
    return r;
}

struct PacingMode
{
    bool driver       = false;  // true: swap interval N, no CPU wait
    int  swapInterval = 0;      // N when driver, else 0
    bool operator==(const PacingMode& o) const { return driver == o.driver && swapInterval == o.swapInterval; }
    bool operator!=(const PacingMode& o) const { return !(*this == o); }
};

// Pure decision. Driver pacing only with VSync on, an EXACT known refresh and
// refresh/fps within DRIVER_RATIO_TOLERANCE of a whole number N >= 1. An
// inexact (integer) refresh always uses CPU pacing: the integer can be off by
// up to 1 Hz (59 or 60 for 59.94), so N cannot be trusted and a wrong N would
// run the display at the wrong rate; CPU pacing is the safe fallback. A cap
// above the refresh has N < 1 and uses CPU pacing (VSync then limits the rate).
inline PacingMode choosePacing(bool vsyncOn, const Refresh& refresh, int fps)
{
    PacingMode m;
    if (!vsyncOn || !refresh.known() || !refresh.exact)
    {
        return m;
    }
    const double ratio = refresh.hz / static_cast<double>(clampFps(fps));
    const double n     = std::floor(ratio + 0.5);
    if (n >= 1.0 && n < 1000.0 && std::fabs(ratio - n) <= DRIVER_RATIO_TOLERANCE * n)
    {
        m.driver       = true;
        m.swapInterval = static_cast<int>(n);
    }
    return m;
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

// The swap interval and the display, injectable so the real Limiter runs
// under a fake. The window implementation owns the interval: setInterval(0)
// restores whatever the viewer's own VSync setting expects.
struct ISwapControl
{
    virtual ~ISwapControl() = default;
    // The viewer's VSync is on and the swap-interval extension is usable.
    virtual bool    vsyncOn() = 0;
    // Refresh rate of the monitor the window is on (cheap; called every frame
    // while the cap is on; the implementation re-queries only when the monitor
    // or the display configuration changed).
    virtual Refresh refresh() = 0;
    // The driver interval this layer last set and that is still in force (0 =
    // none, the viewer's own setting: a VSync toggle or a context rebuild clears it).
    virtual int     appliedInterval() = 0;
    // n >= 1: request swap interval n (verified; false = refused, the
    // viewer's own interval is back in force). n == 0: restore the viewer's own.
    virtual bool    setInterval(int n) = 0;
    // A counter that changes when the environment a failure was observed in changes: the GL
    // context is rebuilt or the VSync setting toggled, the display configuration changes, or
    // the window moves to another monitor. Read AFTER refresh() (which notices a monitor move).
    virtual unsigned envGeneration() = 0;
    // A counter that changes on a focus, activation or visibility (minimise / restore) change.
    virtual unsigned focusGeneration() = 0;
    // The window can be measured: it is visible, not minimised and is the foreground (focused)
    // window. An unfocused or covered window's swap can stop blocking at the vblank, so
    // driver pacing is not used then (CPU pacing holds the cap) and nothing is measured.
    virtual bool     measurable() = 0;
};

// The CPU pacing state machine used by the viewer. Main thread only.
class FramePacer
{
public:
    // Waits out the rest of the interval for the present (or frame) that just
    // finished its work and returns the seconds waited (0 when late). Call
    // once per present. A late wake is NOT re-anchored (see the top of the file): the chain
    // keeps its plan and the next interval absorbs the lateness.
    // rawSpin: use spinSeconds as given (HEAD's scheduling) instead of capping it at interval/8.
    double wait(int fps, IClock& clock, double spinSeconds, bool rawSpin = false)
    {
        const double now  = clock.now();
        const double interval = targetInterval(fps);
        const NextStep s  = planNext(mHavePrev, mDeadline, now, interval, rawSpin ? spinSeconds : spinWindow(spinSeconds, interval));
        mDeadline = s.deadline;
        mHavePrev = true;
        mLastWakeLate = 0.0;
        if (!(s.plan.total() > 0.0))
        {
            return 0.0;
        }
        if (s.plan.sleepSeconds > 0.0)
        {
            clock.sleepFor(s.plan.sleepSeconds);
        }
        clock.spinUntil(s.deadline);
        const double woke = clock.now();
        mLastWakeLate = (woke > s.deadline) ? (woke - s.deadline) : 0.0;
        return woke - now;
    }

    void reset() { mHavePrev = false; mLastWakeLate = 0.0; }
    // True when a wait() at `now` would sleep or spin (the chain is anchored and not yet due).
    bool wouldWait(int fps, double now) const
    {
        return mHavePrev && std::isfinite(now) && std::isfinite(mDeadline) &&
               (mDeadline + targetInterval(fps) - now) > 0.0;
    }
    bool anchored() const { return mHavePrev; }
    double deadline() const { return mDeadline; }
    // Seconds the last wait woke past its deadline (0 = on time or no wait).
    double lastWakeLate() const { return mLastWakeLate; }

private:
    double mDeadline = 0.0;
    double mLastWakeLate = 0.0;
    bool   mHavePrev = false;
};

// The per-frame gating around a FramePacer and the pacing mode, header-only so
// the glue the viewer relies on (cap-off no-ops, a wait per present, driver
// mode, fallback pacing, the swap-interval restore, the driver pre-wait and safety
// net, and its re-arm) is unit tested on a fake clock. The platform entry points
// below are thin wrappers over one instance.
class Limiter
{
public:
    // driverLock: the FrameLimiterVSyncLock setting. false (the default): CPU pacing only, no
    // swap-interval query at all. true: driver pacing where choosePacing() allows it.
    // vsyncSetting: the CACHED RenderVSyncEnable setting (never a WGL query).
    // refreshHz: the EXACT refresh of the monitor the window is on (ISwapControl::refresh() when it is an exact
    // fraction; 0 = unknown or inexact; never a WGL query). With VSync on and the cap at or above
    // 0.995 x refreshHz the cap uses HEAD's scheduling exactly (headSchedule(), see the top of the file).
    void beginFrame(bool on, int fps, bool driverLock = false, bool vsyncSetting = false, double refreshHz = 0.0)
    {
        if (on && mFps != fps)
        {
            netReset(); // a cap change restarts the rate measurement
            mEv.valid = false;
        }
        {
            const double hz   = (refreshHz > 0.0 && std::isfinite(refreshHz)) ? refreshHz : 0.0;
            const bool   head = mHeadPolicy == HeadSchedulePolicy::Auto && vsyncSetting && hz > 0.0 &&
                                static_cast<double>(clampFps(fps)) >= HEAD_SCHEDULE_RATIO * hz;
            if (head != mHeadEligible)
            {
                mPacer.reset(); // the scheduling changed (present pacing <-> HEAD's end-of-frame wait): a fresh chain
            }
            mHeadEligible = head;
        }
        if (mDriverLock != driverLock)
        {
            netReset(); // the lock toggled: nothing measured before applies
            mEv.valid = false;
            clearFailure();
        }
        mOn = on;
        mFps = fps;
        mDriverLock = driverLock;
        mPresents = 0;
        mPresentPaced = false;
        mPresentSkipped = false;
        if (!on)
        {
            mHavePresent = false; // no stale intervals after the cap was off
            mHaveFallback = false;
            clearFailure();       // a cap disable clears the failure latch; NOT the give-up / trip count
            netReset();
            mEv.valid = false;
        }
    }

    // Call once per frame right after beginFrame(). Decides the pacing mode and
    // keeps the swap interval consistent with it. Cap off, or the driver lock off
    // (the default), with nothing applied: does nothing at all (no call into the swap
    // control). With the lock off and an interval still applied (the setting was just
    // turned off) it restores the viewer's own interval once.
    void updatePacing(ISwapControl& sw)
    {
        mModeChanged = false;
        const bool eligible = mOn && mDriverLock;
        if (!eligible && mRequestedN == 0 && !mRestorePending)
        {
            return; // inert: nothing asked for, nothing left to restore
        }
        PacingMode want;
        if (eligible)
        {
            const Refresh refresh = sw.refresh();
            want = choosePacing(sw.vsyncOn(), refresh, mFps);
            mRefreshHz = refresh.known() ? refresh.hz : 0.0;
            // Generations are read after refresh(), which notices a monitor move.
            const unsigned env = sw.envGeneration();
            const unsigned foc = sw.focusGeneration();
            const bool measurable = sw.measurable();
            // Pending evidence from the last present (the net tripped on it). It is only
            // valid in the exact environment it was measured in: same GL context / display /
            // monitor, same focus, same cap, same N, window still focused. Otherwise it is
            // dropped, so a change between presentDone() and here cannot condemn a new setup.
            if (mEv.valid)
            {
                const bool same = measurable && mGenKnown && mEv.env == env && mEv.foc == foc && mEv.fps == mFps &&
                                  mMode.driver && mMode.swapInterval == mEv.n &&
                                  want.driver && want.swapInterval == mEv.n;
                const int n = mEv.n;
                mEv.valid = false;
                if (same)
                {
                    recordFailure(n); // the driver does not honour N: latch it as failed (CPU pacing)
                }
            }
            if (!mGenKnown)
            {
                mGenKnown = true;
                mSeenEnv = env;
                mSeenFocus = foc;
            }
            // A change of environment (GL context, VSync, display, monitor) clears the failure
            // latch AND the trip budget (and the give-up); a focus / visibility change clears
            // the failure latch only (a cap disable does the same, in beginFrame()).
            if (env != mSeenEnv)
            {
                mSeenEnv = env;
                mTrips = 0;
                mGaveUp = false;
                clearFailure();
                netReset();
            }
            if (foc != mSeenFocus)
            {
                mSeenFocus = foc;
                clearFailure();
                netReset();
            }
            mMeasurable = measurable;
            if (!measurable)
            {
                // Unfocused, minimised or not the foreground window: its swap can stop
                // blocking, so the cap is held strictly by the CPU pacer (and nothing is
                // measured) until the window is focused again.
                netReset();
                want = PacingMode();
            }
        }
        if (mFailedN != 0 && !mGaveUp && mFailTime >= 0.0 && (mLastClock - mFailTime) >= DRIVER_RETRY_SECONDS)
        {
            clearFailure(); // retry driver pacing after a while on the CPU fallback
        }
        if (!want.driver)
        {
            mFailedN = 0; // the failure latch only holds while a driver interval is wanted
        }
        else if (mGaveUp || want.swapInterval == mFailedN)
        {
            want = PacingMode(); // the driver refused / ignored this N: stay on CPU pacing
        }
        // A restore of the viewer's own interval can be REFUSED (the window's GL context is not the
        // current one, so the call would hit another context): keep it pending and retry every frame
        // until it succeeds, however the cap / lock / mode changed meanwhile.
        bool restorePending = false;
        if (want.driver)
        {
            if (sw.appliedInterval() != want.swapInterval && !sw.setInterval(want.swapInterval))
            {
                recordFailure(want.swapInterval);
                want = PacingMode();
                if (!sw.setInterval(0)) // make sure the viewer's own interval is in force
                {
                    restorePending = true;
                }
            }
        }
        else if (mRestorePending || mRequestedN != 0 || sw.appliedInterval() != 0)
        {
            // Verify on leaving driver mode even if toggleVSync() already cleared the window's cache:
            // its unverified WGL call may have left our N applied. Pending restores likewise survive
            // a cleared cache until readback confirms the viewer's own interval.
            if (!sw.setInterval(0)) // back to the viewer's own VSync setting
            {
                restorePending = true;
            }
        }
        mRestorePending = restorePending;
        if (want != mMode)
        {
            mMode = want;
            mModeChanged = true;
            mPacer.reset(); // a new mode starts a fresh chain
            netReset();
        }
        mRequestedN = mMode.driver ? mMode.swapInterval : 0;
    }

    // True while the cap is on (this frame's presents are paced).
    bool capActive() const { return mOn; }
    // True while the cap is on and the end-of-frame wait applies: nothing was presented yet this frame
    // (the fallback wait), or HEAD's scheduling is in force (VSync on, the cap at the refresh: every frame
    // is paced at its end, as in HEAD).
    bool fallbackPending() const { return mOn && (mPresents == 0 || headSchedule()); }
    // HEAD's scheduling is in force: the cap is on, no driver interval is applied, VSync is on (the cached
    // setting) and the cap is at or above 0.995 x the exact refresh of the window's monitor. The wait is
    // HEAD's end-of-frame wait (endFrameWait() with HEAD's raw spin), nothing is waited before the present.
    bool headSchedule() const { return mOn && !mMode.driver && mHeadEligible; }
    // True while the driver blocks each present (the CPU only pre-waits).
    bool driverPacing() const { return mOn && mMode.driver; }

    // True when presentWait(clock) is about to sleep or spin, so a caller can open its
    // sleep record only for a wait that happens (an overshot frame waits zero).
    bool presentWaitPending(IClock& clock) const
    {
        const double now = clock.now();
        if (!mOn || skipSecondPresent(now))
        {
            return false;
        }
        if (mMode.driver)
        {
            double target = 0.0;
            return mHavePresent && prewaitWanted(now) && prewaitTarget(now, target);
        }
        if (headSchedule())
        {
            return false; // HEAD's scheduling waits at the end of the frame, never before the present
        }
        return mPacer.wouldWait(mFps, now);
    }

    // Just before every present (swap(), the resize present, the login screen): waits
    // out the deadline of THIS present (cap on only) and counts it. Two presents in
    // one frame are each paced, except a second one right behind the first (less than a quarter
    // of the interval after it, e.g. a resize frame's two presents), which is not waited for:
    // there is nothing to space. CPU mode: the deadline chain (sleep + spin). Driver mode: NO
    // wait while the driver behaves (the swap blocks to the Nth vblank itself); only while
    // the driver is suspicious (see suspicious()) the pre-wait (sleep only) until the
    // previous present returned + 0.90 x the cap interval, which bounds an ignored N. HEAD's scheduling
    // (headSchedule(): VSync on, the cap at or above 0.995 x the exact refresh): no wait before the present
    // at all, the wait is the end-of-frame wait (endFrameWait()).
    double presentWait(IClock& clock, double spinSeconds)
    {
        if (!mOn)
        {
            return 0.0;
        }
        const double nowT = clock.now();
        const bool skip = skipSecondPresent(nowT);
        ++mPresents;
        mPresentPaced = true;
        mPresentSkipped = skip;
        if (mMode.driver)
        {
            double waited = 0.0;
            mPrewaitLate = 0.0;
            if (mHavePresent && !skip)
            {
                const double now = clock.now();
                double target    = 0.0;
                if (prewaitWanted(now) && prewaitTarget(now, target))
                {
                    clock.sleepFor(target - now); // no spin: the swap that follows blocks anyway
                    const double woke = clock.now();
                    waited       = woke - now;
                    mPrewaitLate = (woke > target) ? (woke - target) : 0.0;
                }
            }
            stamp(clock.now());
            return waited;
        }
        if (skip || headSchedule())
        {
            // A second present right behind the first: nothing to space. HEAD's scheduling: the wait is
            // the end-of-frame wait, the chain (mPacer) is left to endFrameWait().
            stamp(clock.now());
            return 0.0;
        }
        const double waited = mPacer.wait(mFps, clock, spinSeconds);
        stamp(clock.now());
        return waited;
    }

    // Just after the present returned. Only for a present presentWait() counted.
    // Records the present-to-present interval and feeds the driver safety net.
    // Returns true when it handled one. A present that presentWait() SKIPPED (a second present right
    // behind the first, a resize frame's) is only a baseline for the next present: it feeds neither
    // the safety net, the suspicion ring nor the clean-time credit (it returns at once and is not a
    // flip of its own, so it would count twice for one flip).
    bool presentDone(IClock& clock)
    {
        if (!mOn || !mPresentPaced)
        {
            return false;
        }
        mPresentPaced = false;
        const bool skipped = mPresentSkipped;
        mPresentSkipped = false;
        const double t = clock.now();
        stamp(t);
        mPresentInterval = (mHavePresent && !skipped) ? (t - mLastPresent) : -1.0;
        if (!mMode.driver || !mMeasurable)
        {
            netReset(); // not in driver mode, or the window is not measurable (unfocused, minimised, covered)
        }
        else if (!skipped)
        {
            // The queue-refill quarantine: after an interval longer than N + 0.5 refreshes (a late return) the
            // render-ahead queue refills with a tail of short returns that must not read as an ignored N.
            bool quarantined = mSuspectQuarantine > 0;
            if (quarantined)
            {
                --mSuspectQuarantine;
            }
            if (mHavePresent && mNetHavePrev)
            {
                // Both presents were focused driver-mode presents of this setup.
                noteCleanDriverTime(t - mLastPresent);
                if (mRefreshHz > 0.0 &&
                    (t - mLastPresent) > (static_cast<double>(mMode.swapInterval) + 0.5) / mRefreshHz)
                {
                    mSuspectQuarantine = DRIVER_SUSPECT_QUARANTINE;
                    quarantined        = true;
                }
            }
            if (mRefreshHz > 0.0)
            {
                // SUSTAINED evidence only: the mean of the last DRIVER_SUSPECT_WINDOW intervals
                // (a late SwapBuffers return, or return jitter, moves the window's total by its
                // own lateness only; an ignored N returns every refresh). The ring holds the
                // return times of the last WINDOW + 1 focused driver-mode presents.
                mSuspectRing[mSuspectPos] = t;
                mSuspectPos               = (mSuspectPos + 1) % DRIVER_SUSPECT_RING;
                if (mSuspectCount < DRIVER_SUSPECT_RING)
                {
                    ++mSuspectCount;
                }
                if (mSuspectCount == DRIVER_SUSPECT_RING && !quarantined)
                {
                    // The ring is full, so the interval before the window is known (a late return that is the
                    // window's first point is visible as the long interval before it). The present WINDOW back
                    // (the newest is at mSuspectPos - 1).
                    const double oldest = mSuspectRing[(mSuspectPos - 1 - DRIVER_SUSPECT_WINDOW + 2 * DRIVER_SUSPECT_RING) % DRIVER_SUSPECT_RING];
                    const double n      = static_cast<double>(mMode.swapInterval);
                    const double meanRefreshes = (t - oldest) * mRefreshHz / static_cast<double>(DRIVER_SUSPECT_WINDOW);
                    // A window holding an interval longer than N + 0.5 refreshes (a late return) is
                    // ignored: an ignored N never produces one, a late return must never latch.
                    if (meanRefreshes < DRIVER_SUSPECT_RATIO * n && !suspectWindowHasLateReturn(n))
                    {
                        // Pre-wait the next presents while this stays true.
                        mSuspect     = true;
                        mSuspectTime = t;
                    }
                }
            }
            mNetHavePrev = true;
            if (netObserve(t) && !mEv.valid)
            {
                // Evidence the driver ignores N, tagged with the setup it was measured in.
                mEv.valid = true;
                mEv.fps   = mFps;
                mEv.n     = mMode.swapInterval;
                mEv.env   = mSeenEnv;
                mEv.foc   = mSeenFocus;
            }
        }
        mLastPresent = t;
        mHavePresent = true;
        mHaveFallback = false;
        return true;
    }

    // The one sequence every present site uses: pace (doWait must call presentWait()), the
    // present itself (doSwap), then record it. Cap off: just the present, nothing else.
    // The viewer's wrapper is ALFrameLimiter::pacedPresent(); display_startup() (login
    // screen), the resize present and swap() all go through it.
    template <class DoWait, class DoSwap>
    bool pacedPresent(IClock& clock, DoWait&& doWait, DoSwap&& doSwap)
    {
        const bool paced = mOn;
        if (paced)
        {
            doWait();
        }
        doSwap();
        if (paced)
        {
            presentDone(clock);
        }
        return paced;
    }

    // End of the frame: paces a frame that did not present (the fallback), and under HEAD's scheduling every
    // frame, as HEAD does (HEAD's raw spin window: pass SPIN_SECONDS_HEAD_HIRES for the high-resolution timer).
    // 0 when the cap is off or the frame already presented and was paced at its present.
    double endFrameWait(int fps, IClock& clock, double spinSeconds)
    {
        if (!fallbackPending())
        {
            return 0.0;
        }
        const bool presented = mPresents > 0;
        ++mPresents;
        const double waited = mPacer.wait(fps, clock, spinSeconds, headSchedule());
        const double t = clock.now();
        stamp(t);
        if (!presented)
        {
            mFallbackInterval = mHaveFallback ? (t - mLastFallback) : -1.0;
            mLastFallback = t;
            mHaveFallback = true;
            mHavePresent = false; // a frame without a present breaks the present chain (no pre-wait baseline)
            mNetHavePrev = false; // ... and the clean-time credit: no interval spans a frame that did not present
        }
        return waited;
    }

    void reset()
    {
        mPacer.reset();
        mHavePresent = mHaveFallback = mPresentPaced = mOn = false;
        mMode = PacingMode();
        mRequestedN = 0;
        mFailedN = 0;
        mFailTime = 0.0;
        mTrips = 0;
        mGaveUp = false;
        mGenKnown = false;
        mEv = Evidence();
        mPresents = 0;
        mLastClock = 0.0;
        mHaveClock = false;
        mMeasurable = true;
        mRefreshHz = 0.0;
        mDriverLock = false;
        mHeadEligible = false;
        mCleanSeconds = 0.0;
        mPrewaitLate = 0.0;
        mRestorePending = false;
        mPresentSkipped = false;
        netReset();
    }

    const FramePacer& pacer() const { return mPacer; }
    const PacingMode& mode() const { return mMode; }
    bool   modeChanged() const { return mModeChanged; }
    // True while CPU pacing is in use because the driver refused, or ignored, the swap interval.
    bool   driverRefused() const { return mFailedN != 0 || mGaveUp; }
    // Failures (ignored or refused intervals) in the current environment, and whether the
    // limiter gave up on driver pacing for the session (DRIVER_MAX_TRIPS reached). The give-up
    // is cleared only by an environment change (GL context, VSync, display, monitor), not by a
    // cap disable or a focus change.
    int    trips() const { return mTrips; }
    bool   gaveUp() const { return mGaveUp; }
    int    presentsThisFrame() const { return mPresents; }
    // Seconds between the last two real presents (SwapBuffers returns); < 0 = none.
    double presentInterval() const { return mPresentInterval; }
    // Seconds between the last two fallback-paced frames; < 0 = none.
    double fallbackInterval() const { return mFallbackInterval; }
    // The safety net's current window: presents counted, and the count that trips it.
    int    netCount() const { return mNetCount; }
    int    netThreshold() const
    {
        const double perN = (mRefreshHz > 0.0 && mMode.swapInterval > 0) ? mRefreshHz / mMode.swapInterval : 0.0;
        const double fps  = static_cast<double>(clampFps(mFps));
        const double rate = (perN > fps) ? perN : fps;
        int thresh = static_cast<int>(std::floor(netWindowSeconds(mFps) * rate)) + DRIVER_NET_SLACK_PRESENTS;
        if (thresh > DRIVER_NET_CAPACITY - 1)
        {
            thresh = DRIVER_NET_CAPACITY - 1;
        }
        return thresh;
    }
    // True when pending evidence (a trip not yet acted on) is held.
    bool   evidencePending() const { return mEv.valid; }
    // Driver mode only: the driver returned a present sooner than an honoured N allows within the
    // last net window, so the CPU pre-wait is active. Cleared by netReset().
    bool   suspicious(double now) const { return mSuspect && (now - mSuspectTime) <= netWindowSeconds(mFps); }
    // TEST HOOK (the viewer never calls it): WhenSuspicious is the production behaviour; Always
    // is the round-5 unconditional pre-wait; Never is a pure driver-pacing control.
    enum class PrewaitPolicy { WhenSuspicious, Never, Always };
    void setPrewaitPolicy(PrewaitPolicy p) { mPrewaitPolicy = p; }
    // TEST HOOK (the viewer never calls it): Auto is the production behaviour (HEAD's scheduling with VSync on
    // and the cap at the refresh); Never always uses present pacing (the round-5..7 behaviour) so tests can
    // prove they would fail.
    enum class HeadSchedulePolicy { Auto, Never };
    void setHeadSchedulePolicy(HeadSchedulePolicy p) { mHeadPolicy = p; }
    // A restore of the viewer's own swap interval was refused and is retried every frame.
    bool   restorePending() const { return mRestorePending; }
    bool   suspectLatched() const { return mSuspect; }
    // Seconds the last driver-mode pre-wait woke past its target (0 = on time or no pre-wait).
    double prewaitLate() const { return mPrewaitLate; }
    // Failure-free driver seconds counted toward forgiving a trip.
    double cleanDriverSeconds() const { return mCleanSeconds; }

private:
    // Evidence that the driver ignores N, tagged with the setup it was measured in.
    struct Evidence
    {
        bool     valid = false;
        int      fps   = 0;
        int      n     = 0;
        unsigned env   = 0;
        unsigned foc   = 0;
    };

    void clearFailure() { mFailedN = 0; }
    // True when the suspicion window (the last DRIVER_SUSPECT_WINDOW intervals AND the one before them,
    // as far as the ring knows it) holds an interval longer than N + 0.5 refreshes: a late return.
    bool suspectWindowHasLateReturn(double n) const
    {
        const double limit = (n + 0.5) / mRefreshHz;
        const int first    = (mSuspectPos - mSuspectCount + DRIVER_SUSPECT_RING) % DRIVER_SUSPECT_RING;   // the oldest return held
        double prev        = mSuspectRing[first];
        for (int i = 1; i < mSuspectCount; ++i)
        {
            const double cur = mSuspectRing[(first + i) % DRIVER_SUSPECT_RING];
            if (cur - prev > limit)
            {
                return true;
            }
            prev = cur;
        }
        return false;
    }
    void stamp(double t)
    {
        mLastClock = t;
        mHaveClock = true;
        if (mFailTime < 0.0)
        {
            mFailTime = t;
        }
    }
    void recordFailure(int n)
    {
        mFailedN = n;
        mFailTime = mHaveClock ? mLastClock : -1.0; // -1: stamp at the first clock reading
        mCleanSeconds = 0.0;
        if (++mTrips >= DRIVER_MAX_TRIPS)
        {
            mGaveUp = true;
        }
    }
    // Failure-free driver time forgives trips: one per DRIVER_TRIP_DECAY_SECONDS. A single
    // present interval counts for at most one second (a long hitch is not a long clean run).
    void noteCleanDriverTime(double dt)
    {
        if (mTrips <= 0 || mGaveUp)
        {
            mCleanSeconds = 0.0;
            return;
        }
        mCleanSeconds += (dt > 0.0) ? (dt < 1.0 ? dt : 1.0) : 0.0;
        if (mCleanSeconds >= DRIVER_TRIP_DECAY_SECONDS)
        {
            mCleanSeconds = 0.0;
            --mTrips;
        }
    }

    // The driver safety net (see DRIVER_NET_*): the times of the presents of the last
    // netWindowSeconds() in driver mode (a ring buffer). Returns true when their number
    // exceeds netThreshold(). Hitches only drop old entries, so nothing slow can trip it.
    // netReset() also clears the "suspicious" latch (the pre-wait) and the previous-present link.
    void netReset()
    {
        mNetHead = 0;
        mNetCount = 0;
        mSuspect = false;
        mNetHavePrev = false;
        mSuspectPos = 0;
        mSuspectCount = 0;
        mSuspectQuarantine = DRIVER_SUSPECT_QUARANTINE; // a render-ahead queue fills at the start of every measurement
    }
    bool netObserve(double t)
    {
        const double window = netWindowSeconds(mFps);
        while (mNetCount > 0 && (t - mNetTimes[mNetHead]) > window)
        {
            mNetHead = (mNetHead + 1) % DRIVER_NET_CAPACITY;
            --mNetCount;
        }
        if (mNetCount >= DRIVER_NET_CAPACITY)
        {
            mNetHead = (mNetHead + 1) % DRIVER_NET_CAPACITY; // cannot happen below the threshold; stay bounded
            --mNetCount;
        }
        mNetTimes[(mNetHead + mNetCount) % DRIVER_NET_CAPACITY] = t;
        ++mNetCount;
        return mNetCount > netThreshold();
    }

    FramePacer mPacer;
    PacingMode mMode;
    bool   mOn = false;
    bool   mModeChanged = false;
    int    mFps = DEFAULT_FPS;
    int    mPresents = 0;
    int    mRequestedN = 0;    // the driver interval this Limiter asked for (0 = none)
    int    mFailedN = 0;       // an N the driver refused or ignored (0 = none): the failure latch
    double mFailTime = 0.0;    // mLastClock when the failure was recorded (-1: not stamped yet)
    int    mTrips = 0;         // failures in the current environment
    // DRIVER_MAX_TRIPS reached: CPU pacing until the environment changes. NOT cleared by a cap
    // disable or a focus change (those clear mFailedN only); an env change (GL context, VSync,
    // display, monitor) clears it together with mTrips.
    bool   mGaveUp = false;
    bool   mGenKnown = false;
    unsigned mSeenEnv = 0;
    unsigned mSeenFocus = 0;
    Evidence mEv;
    bool   mMeasurable = true; // the window is focused, visible and the foreground window
    double mRefreshHz = 0.0;   // the refresh of the monitor, for the net threshold
    bool   mHaveClock = false;
    double mLastClock = 0.0;   // the latest clock reading seen (the Limiter has no clock of its own)
    double mNetTimes[DRIVER_NET_CAPACITY] = {};
    int    mNetHead = 0;
    int    mNetCount = 0;
    // A second present right behind the first one of the frame (a resize frame presents twice):
    // there is nothing to space, so it is not waited for. Needs the first present to be recorded.
    bool   skipSecondPresent(double now) const
    {
        return mPresents >= 1 && mHavePresent && now >= mLastPresent &&
               (now - mLastPresent) < 0.25 * targetInterval(mFps);
    }
    // The driver-mode pre-wait target (the previous present's return + the fraction of the cap
    // interval, at most one interval ahead). True when there is time to wait.
    bool   prewaitTarget(double now, double& target) const
    {
        const double interval = targetInterval(mFps);
        target                = mLastPresent + driverPrewaitFraction(interval) * interval;
        if (target > now + interval)
        {
            target = now + interval; // never wait more than one cap interval (a clock hiccup)
        }
        return std::isfinite(now) && std::isfinite(target) && target > now;
    }
    bool   prewaitWanted(double now) const
    {
        return mPrewaitPolicy == PrewaitPolicy::Always ||
               (mPrewaitPolicy == PrewaitPolicy::WhenSuspicious && suspicious(now));
    }
    PrewaitPolicy mPrewaitPolicy = PrewaitPolicy::WhenSuspicious;
    bool   mDriverLock = false; // FrameLimiterVSyncLock: driver pacing is allowed
    bool   mHeadEligible = false;      // VSync on (cached setting) and cap >= 0.995 x the window monitor's exact refresh
    HeadSchedulePolicy mHeadPolicy = HeadSchedulePolicy::Auto;
    bool   mPresentSkipped = false;    // this present was a second present right behind the first (no wait, no feed)
    bool   mRestorePending = false;    // a restore of the viewer's own swap interval was refused: retry
    double mSuspectRing[DRIVER_SUSPECT_RING] = {};   // return times of the last 10 focused driver-mode presents
    int    mSuspectPos = 0;
    int    mSuspectCount = 0;
    int    mSuspectQuarantine = 0;  // returns left in which the latch is not evaluated (after a late return)
    bool   mSuspect = false;    // an early return was seen: pre-wait while it stays recent
    double mSuspectTime = 0.0;
    bool   mNetHavePrev = false; // the previous present was a focused driver-mode present
    double mPrewaitLate = 0.0;
    double mCleanSeconds = 0.0;
    bool   mPresentPaced = false;
    bool   mHavePresent = false;
    double mLastPresent = 0.0;
    double mPresentInterval = -1.0;
    bool   mHaveFallback = false;
    double mLastFallback = 0.0;
    double mFallbackInterval = -1.0;
};

// ---- Platform side (alframelimiter.cpp) ------------------------------------

// Call first thing every frame with the cap state. Clears the per-frame present
// count, then (cap on and driverLock on, or a driver interval still applied) picks
// the pacing mode and keeps the swap interval consistent with it. With on == false
// (or driverLock == false) and nothing applied, presentWait() and endFrameWait()
// do nothing special and no window call is made. driverLock is the opt-in
// FrameLimiterVSyncLock setting (default off: CPU pacing only). vsyncSetting is the cached
// RenderVSyncEnable setting (never a WGL query): with it on and the cap at or above 0.995 x the
// EXACT refresh of the monitor the window is on (the window's cached getFrameRefresh(), no WGL
// call; unknown or inexact = not used) the cap uses HEAD's scheduling exactly: the end-of-frame wait.
void beginFrame(bool on, int fps, bool driverLock = false, bool vsyncSetting = false);

// True while the cap is on. Callers use it to skip all work (including stats
// records) when the cap would do nothing, so the cap-off path stays inert.
bool capActive();

// True while the driver paces the present (swap interval N): the swap blocks to
// the Nth vblank by design, so callers record it as sleep, not as a slow swap.
bool driverPacing();

// True while the cap is on and this frame has not presented yet, or HEAD's
// end-of-frame scheduling applies even to presented frames.
bool fallbackPending();

// EVERY present goes through this (swap(), the resize present, the login screen in
// display_startup()): cap on, it paces the present (CPU deadline wait, or the driver-mode
// pre-wait while the driver is suspicious; glFlush first, recorded as sleep only when a wait
// happens), calls `swap` (the SwapBuffers call, plus whatever stats scope the caller wants around
// it), then records the present interval and feeds the driver safety net. Cap off: just calls
// `swap`, nothing else.
void pacedPresent(const std::function<void()>& swap);

// Call at the end of the frame while the cap is enabled. Waits if the frame did
// not present (for example a minimised window), or HEAD's scheduling applies.
// Returns the seconds waited; 0 when the cap is off.
double endFrameWait(int fps);

// Release the timer handle (call from viewer cleanup).
void shutdown();
}

#endif // AL_FRAME_LIMITER_H
