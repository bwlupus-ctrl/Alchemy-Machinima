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
 * timer wakes within ~0.5 ms regardless of the system tick. The last ~0.5 ms is
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

#include <chrono>
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

ALFrameLimiter::FramePacer sPacer;
PlatformClock              sClock;
}

namespace ALFrameLimiter
{
double endFrameWait(int fps)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_APP("FrameLimiter wait");
    return sPacer.wait(fps, sClock,
                       PlatformClock::hiRes() ? SPIN_SECONDS_HIRES : SPIN_SECONDS_COARSE);
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
#endif
    sPacer.reset();
}
}
