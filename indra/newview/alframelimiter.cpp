/**
 * @file alframelimiter.cpp
 * @brief [FrameLimiter] Platform clock and waits for the frame rate cap. The
 *        pacing logic lives in alframelimiter.h (FramePacer, planNext) and is
 *        unit tested with a fake clock.
 *
 * Timer choice: on Windows the bulk of the wait uses a high-resolution
 * waitable timer (CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, Windows 10 1803+).
 * Plain Sleep()/sleep_for wakes on the scheduler tick (15.6 ms by default,
 * 1 ms only while someone holds timeBeginPeriod), which at 30 fps would make
 * the frame interval land anywhere from 28 to 32 fps. The high-resolution
 * timer wakes within ~0.5 ms regardless of the system tick. The last ~1 ms is
 * a busy-spin on the monotonic clock (_mm_pause, not yield, so a busy core
 * cannot cost a whole timeslice) so the frame start lands on the deadline.
 * If the timer cannot be created (older Windows) it falls back to sleep_for
 * with a wider spin window (the window thread already holds
 * timeBeginPeriod(1)).
 *
 * Main thread only. Does not touch the network/message or worker threads.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#include "llviewerprecompiledheaders.h"

#include "alframelimiter.h"

#include "llglheaders.h"
#include "llperfstats.h"
#include "llviewerwindow.h"
#if LL_WINDOWS
#include "llwindowwin32.h"
#endif

#include <chrono>
#include <optional>
#include <thread>

#if LL_WINDOWS
#include <intrin.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif

namespace
{
using Clock = std::chrono::steady_clock;

inline void cpuRelax()
{
#if LL_WINDOWS
    _mm_pause();
#else
    std::this_thread::yield();
#endif
}

#if LL_WINDOWS
HANDLE sHiResTimer = nullptr;
bool   sTimerTried = false;

HANDLE hiResTimer()
{
    if (!sTimerTried)
    {
        sTimerTried = true;
        sHiResTimer = CreateWaitableTimerExW(nullptr, nullptr,
                                             CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                             TIMER_ALL_ACCESS);
    }
    return sHiResTimer;
}
#endif

class PlatformClock final : public ALFrameLimiter::IClock
{
public:
    double now() override
    {
        return std::chrono::duration<double>(Clock::now() - origin()).count();
    }

    void sleepFor(double seconds) override
    {
        if (!(seconds > 0.0))
        {
            return;
        }
#if LL_WINDOWS
        if (HANDLE timer = hiResTimer())
        {
            LARGE_INTEGER due;
            due.QuadPart = -static_cast<LONGLONG>(seconds * 1.0e7); // relative, 100 ns units
            if (due.QuadPart < 0 && SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
            {
                // Bounded wait: a stuck timer costs one frame, never a hang.
                WaitForSingleObject(timer, 250);
                return;
            }
        }
#endif
        std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    }

    void spinUntil(double deadline) override
    {
        while (now() < deadline)
        {
            cpuRelax();
        }
    }

    static bool hiRes()
    {
#if LL_WINDOWS
        return hiResTimer() != nullptr;
#else
        return false;
#endif
    }

private:
    static Clock::time_point origin()
    {
        static const Clock::time_point t = Clock::now();
        return t;
    }
};

ALFrameLimiter::Limiter sLimiter;
PlatformClock           sClock;

// The window side of the swap interval and the refresh rate. Windows only; on
// other platforms the cap always uses CPU pacing.
class WindowSwap final : public ALFrameLimiter::ISwapControl
{
public:
    bool vsyncOn() override
    {
#if LL_WINDOWS
        if (LLWindowWin32* w = win())
        {
            return w->isFrameVSyncActive();
        }
#endif
        return false;
    }

    ALFrameLimiter::Refresh refresh() override
    {
#if LL_WINDOWS
        if (LLWindowWin32* w = win())
        {
            U32  num = 0, den = 0;
            bool exact = false;
            if (w->getFrameRefresh(num, den, exact))
            {
                if (exact)
                {
                    return ALFrameLimiter::refreshFromFraction(num, den);
                }
                return ALFrameLimiter::refreshFromInteger((S32)num);
            }
        }
#endif
        return ALFrameLimiter::Refresh();
    }

    int appliedInterval() override
    {
#if LL_WINDOWS
        if (LLWindowWin32* w = win())
        {
            return w->getFrameSwapInterval();
        }
#endif
        return 0;
    }

    bool setInterval(int n) override
    {
#if LL_WINDOWS
        if (LLWindowWin32* w = win())
        {
            return w->setFrameSwapInterval(n);
        }
#endif
        return n == 0;
    }

    unsigned envGeneration() override
    {
#if LL_WINDOWS
        if (LLWindowWin32* w = win())
        {
            return w->getFrameEnvGeneration();
        }
#endif
        return 0;
    }

    unsigned focusGeneration() override
    {
#if LL_WINDOWS
        if (LLWindowWin32* w = win())
        {
            return w->getFrameFocusGeneration();
        }
#endif
        return 0;
    }

    bool measurable() override
    {
#if LL_WINDOWS
        if (LLWindowWin32* w = win())
        {
            return w->isFrameWindowMeasurable();
        }
#endif
        return false;
    }

#if LL_WINDOWS
    static void forgetWindow()
    {
        sCachedBase = nullptr;
        sCachedWin  = nullptr;
    }

private:
    // The Win32 window, with the dynamic_cast cached per LLWindow pointer: this runs about
    // five times a frame while the driver lock is on. The cache is also dropped at the top of
    // every frame (beginFrame), so a window destroyed and recreated at the same address can never
    // leave a dangling pointer behind for more than the rest of one frame.
    static LLWindowWin32* win()
    {
        LLWindow* base = gViewerWindow ? gViewerWindow->getWindow() : nullptr;
        if (base != sCachedBase)
        {
            sCachedBase = base;
            sCachedWin  = base ? dynamic_cast<LLWindowWin32*>(base) : nullptr;
        }
        return sCachedWin;
    }
    static inline LLWindow*      sCachedBase = nullptr;
    static inline LLWindowWin32* sCachedWin  = nullptr;
#endif
};
WindowSwap sSwap;

double spinSeconds()
{
    return PlatformClock::hiRes() ? ALFrameLimiter::SPIN_SECONDS_HIRES : ALFrameLimiter::SPIN_SECONDS_COARSE;
}
}

namespace ALFrameLimiter
{
void beginFrame(bool on, int fps, bool driverLock, bool vsyncSetting)
{
#if LL_WINDOWS
    WindowSwap::forgetWindow(); // re-resolve lazily for this frame's refresh or driver-pacing queries
#endif
    // The EXACT refresh of the monitor the window is on (getFrameRefresh(): the window's own monitor, an exact
    // num/den fraction, cached and re-read only on a monitor / display change, no WGL call), read only when
    // it can matter: the cap on with VSync on. An unknown or inexact (integer) refresh reads as 0. With VSync
    // on and the cap at or above 0.995 x it the cap uses HEAD's scheduling exactly (the end-of-frame wait).
    double refreshHz = 0.0;
    if (on && vsyncSetting)
    {
        const ALFrameLimiter::Refresh r = sSwap.refresh();
        if (r.known() && r.exact)
        {
            refreshHz = r.hz;
        }
    }
    sLimiter.beginFrame(on, fps, driverLock, vsyncSetting, refreshHz);
    // Cap off, or FrameLimiterVSyncLock off (the default), with no driver interval applied:
    // updatePacing() returns at once and makes no window call, so those paths stay inert
    // (no swap-interval calls; the cached refresh may already have been read above).
    sLimiter.updatePacing(sSwap);
    if (sLimiter.modeChanged())
    {
        const PacingMode& m = sLimiter.mode();
        if (m.driver)
        {
            LL_INFOS("FrameLimiter") << "Frame cap " << fps << " fps: driver pacing, swap interval "
                                     << m.swapInterval << " (VSync on, the refresh rate divides the cap)"
                                     << LL_ENDL;
        }
        else
        {
            LL_INFOS("FrameLimiter") << "Frame cap " << (on ? "on" : "off") << ": CPU deadline pacing"
                                     << (sLimiter.driverRefused() ? " (the driver refused or ignored the swap interval)" : "")
                                     << LL_ENDL;
        }
    }
    static bool sGaveUpLogged = false;
    if (sLimiter.gaveUp() && !sGaveUpLogged)
    {
        sGaveUpLogged = true;
        LL_WARNS("FrameLimiter") << "Frame cap: the driver ignored or refused the swap interval "
                                 << sLimiter.trips() << " times; CPU deadline pacing for this session "
                                 << "(unless the display or GL context changes)" << LL_ENDL;
    }
    else if (!sLimiter.gaveUp())
    {
        sGaveUpLogged = false; // a display or context change re-armed driver pacing
    }
}

bool capActive()
{
    return sLimiter.capActive();
}

bool driverPacing()
{
    return sLimiter.driverPacing();
}

bool fallbackPending()
{
    return sLimiter.fallbackPending();
}

void pacedPresent(const std::function<void()>& swap)
{
    if (!sLimiter.capActive())
    {
        swap(); // cap off: the present and nothing else
        return;
    }
    sLimiter.pacedPresent(sClock,
        []
        {
            if (sLimiter.headSchedule())
            {
                // HEAD's scheduling (VSync on, the cap at the refresh): nothing before the present, no glFlush;
                // the wait is HEAD's end-of-frame wait. Just count the present.
                sLimiter.presentWait(sClock, 0.0);
                return;
            }
            // CPU pacing: the deadline wait. Driver pacing: no wait unless the driver is
            // suspicious, then the pre-wait (sleep only; it bounds the present rate if the driver
            // ignores the swap interval). Recorded as sleep, so AutoTune reads the deliberate
            // wait as neither a slow frame nor a slow swap.
            // glFlush() first, OUTSIDE the sleep record (its cost is real frame cost): the GPU
            // work is submitted before the wait, so its tail does not run past the vblank / deadline.
            glFlush();
            LL_PROFILE_ZONE_NAMED_CATEGORY_APP("FrameLimiter wait (present)");
            // The sleep record is opened only for a wait that actually happens: an overshot frame
            // (or a second present right behind the first) waits zero and records nothing.
            std::optional<LLPerfStats::RecordSceneTime> S;
            if (sLimiter.presentWaitPending(sClock))
            {
                S.emplace(LLPerfStats::StatType_t::RENDER_SLEEP);
            }
            sLimiter.presentWait(sClock, sLimiter.driverPacing() ? 0.0 : spinSeconds());
            // Driver mode has no pacer wake: plot the pre-wait's own lateness there.
            LL_PROFILE_PLOT("FL wake late us",
                            (sLimiter.driverPacing() ? sLimiter.prewaitLate() : sLimiter.pacer().lastWakeLate()) * 1.0e6);
        },
        swap);
    if (sLimiter.presentInterval() >= 0.0)
    {
        // Real presents only (time between SwapBuffers returns, so it includes any
        // VSync block). Fallback frames have their own plot.
        LL_PROFILE_PLOT("FL present interval us", sLimiter.presentInterval() * 1.0e6);
    }
}

double endFrameWait(int fps)
{
    if (!sLimiter.fallbackPending())
    {
        return 0.0; // cap off, or the frame presented and was paced at its present
    }
    LL_PROFILE_ZONE_NAMED_CATEGORY_APP("FrameLimiter wait");
    // HEAD's scheduling uses HEAD's spin window (0.5 ms with the high-resolution timer) so that case is HEAD step for step.
    const double spin = sLimiter.headSchedule()
        ? (PlatformClock::hiRes() ? ALFrameLimiter::SPIN_SECONDS_HEAD_HIRES : ALFrameLimiter::SPIN_SECONDS_COARSE)
        : spinSeconds();
    const double waited = sLimiter.endFrameWait(fps, sClock, spin);
    if (sLimiter.fallbackInterval() >= 0.0)
    {
        LL_PROFILE_PLOT("FL fallback interval us", sLimiter.fallbackInterval() * 1.0e6);
    }
    LL_PROFILE_PLOT("FL wake late us", sLimiter.pacer().lastWakeLate() * 1.0e6);
    return waited;
}

void shutdown()
{
#if LL_WINDOWS
    if (sHiResTimer)
    {
        CloseHandle(sHiResTimer);
        sHiResTimer = nullptr;
    }
    sTimerTried = true; // never recreate after shutdown
    WindowSwap::forgetWindow();
#endif
    sLimiter.reset();
}
}
