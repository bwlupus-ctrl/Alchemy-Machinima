/**
 * @file   threadpool.cpp
 * @author Nat Goodspeed
 * @date   2021-10-21
 * @brief  Implementation for threadpool.
 *
 * $LicenseInfo:firstyear=2021&license=viewerlgpl$
 * Copyright (c) 2021, Linden Research, Inc.
 * $/LicenseInfo$
 */

// Precompiled header
#include "linden_common.h"
// associated header
#include "threadpool.h"
// STL headers
// std headers
// external library headers
// other Linden headers
#include "commoncontrol.h"
#include "llerror.h"
#include "llevents.h"
#include "llsd.h"
#include "stringize.h"

#include <boost/fiber/algo/round_robin.hpp>
#include <algorithm>    // std::min (idle backoff clamp)

/*****************************************************************************
*   Custom fiber scheduler for worker threads
*****************************************************************************/
// As of 2022-12-06, each of our worker threads only runs a single (default)
// fiber: we don't launch explicit fibers within worker threads, nor do we
// anticipate doing so. So a worker thread that's simply waiting for incoming
// tasks should really sleep a little. Override the default fiber scheduler to
// implement that.
// [BDMerge] Was: suspend_until() slept a flat 1ms and notify() did nothing, so
// every idle worker woke 1000x/second forever (~33k context switches/sec across
// the viewer's pool threads, with timeBeginPeriod(1) making 1ms timers real),
// AND freshly posted work waited up to 1ms before any worker noticed it.
//
// This version waits on an auto-reset event instead, with a bounded backoff.
//
// Why an EVENT and not a condition_variable: notify() can fire between a fiber
// becoming ready and the worker entering its wait. A condvar would drop that
// wakeup (the classic lost-wakeup race) and the work would sit until the next
// timeout -- an intermittent latency spike that is miserable to diagnose. A
// Windows auto-reset event REMEMBERS a SetEvent() that arrives before the wait,
// so the race closes itself with no lock and no predicate. notify() is also
// noexcept and may be called from arbitrary threads, which rules out anything
// that can throw or block.
//
// LIVENESS: the timeout is always bounded (<= BACKOFF_MAX_MS), so even a
// completely missed notification costs latency, never a hang -- including at
// shutdown, where ThreadPoolBase::close() closes the queue and then joins.
struct sleepy_robin: public boost::fibers::algo::round_robin
{
    static constexpr DWORD BACKOFF_MIN_MS = 1;
    static constexpr DWORD BACKOFF_MAX_MS = 16;

#if LL_WINDOWS
    sleepy_robin():
        // auto-reset, initially unsignaled
        mWake(CreateEvent(NULL, FALSE, FALSE, NULL)),
        mBackoffMs(BACKOFF_MIN_MS)
    {}

    ~sleepy_robin()
    {
        if (mWake)
        {
            CloseHandle(mWake);
        }
    }
#endif

    virtual void suspend_until( std::chrono::steady_clock::time_point const& deadline) noexcept
    {
#if LL_WINDOWS
        if (! mWake)
        {
            // Event creation failed: fall back to the historical behavior
            // rather than spinning.
            Sleep(BACKOFF_MIN_MS);
            return;
        }

        // Respect the caller's deadline when it has one. boost passes
        // time_point::max() to mean "no deadline", in which case we simply use
        // the backoff -- we are never permitted to wait unboundedly, because
        // that would make a missed notification a hang instead of a delay.
        DWORD timeout_ms = mBackoffMs;
        if (deadline != std::chrono::steady_clock::time_point::max())
        {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            const DWORD clamped = (remaining <= 0)
                ? BACKOFF_MIN_MS
                : (DWORD)std::min<long long>(remaining, (long long)mBackoffMs);
            timeout_ms = clamped;
        }

        if (WaitForSingleObject(mWake, timeout_ms) == WAIT_OBJECT_0)
        {
            // Real work arrived: go back to being maximally responsive.
            mBackoffMs = BACKOFF_MIN_MS;
        }
        else
        {
            // Idle timeout: back off geometrically so a quiet pool costs
            // almost nothing, but never past the cap (see LIVENESS above).
            mBackoffMs = std::min<DWORD>(mBackoffMs * 2, BACKOFF_MAX_MS);
        }
#else
        // currently unused other than windows, but might as well have something here
        // different units than Sleep(), but we actually just want to sleep for any de-minimis duration
        (void)deadline;
        usleep(1);
#endif
    }

    virtual void notify() noexcept
    {
#if LL_WINDOWS
        if (mWake)
        {
            // Safe if no one is waiting yet: an auto-reset event stays
            // signaled until a wait consumes it, which is precisely what
            // closes the lost-wakeup race described above.
            SetEvent(mWake);
        }
#endif
    }

#if LL_WINDOWS
private:
    HANDLE mWake;
    DWORD  mBackoffMs;
#endif
};

/*****************************************************************************
*   ThreadPoolBase
*****************************************************************************/
LL::ThreadPoolBase::ThreadPoolBase(const std::string& name,
                                   size_t threads,
                                   WorkQueueBase* queue,
                                   bool auto_shutdown):
    super(name),
    mName("ThreadPool:" + name),
    mThreadCount(getConfiguredWidth(name, threads)),
    mQueue(queue),
    mAutomaticShutdown(auto_shutdown)
{}

void LL::ThreadPoolBase::start()
{
    for (size_t i = 0; i < mThreadCount; ++i)
    {
        std::string tname{ stringize(mName, ':', (i+1), '/', mThreadCount) };
        mThreads.emplace_back(tname, [this, tname]()
            {
                set_thread_name(tname.c_str());
                LL_PROFILER_SET_THREAD_NAME(tname.c_str());
                run(tname);
            });
    }

    if (!mAutomaticShutdown)
    {
        // Some threads, like main window's might need to run a bit longer
        // to wait for a proper shutdown message
        return;
    }

    // Listen on "LLApp", and when the app is shutting down, close the queue
    // and join the workers.
    LLEventPumps::instance().obtain("LLApp").listen(
        mName,
        [this](const LLSD& stat)
        {
            std::string status(stat["status"]);
            if (status != "running")
            {
                // viewer is starting shutdown -- proclaim the end is nigh!
                LL_DEBUGS("ThreadPool") << mName << " saw " << status << LL_ENDL;
                close();
            }
            return false;
        });
}

LL::ThreadPoolBase::~ThreadPoolBase()
{
    close();
    if (!LLEventPumps::wasDeleted())
    {
        LLEventPumps::instance().obtain("LLApp").stopListening(mName);
    }
}

void LL::ThreadPoolBase::close()
{
    if (! mQueue->isClosed())
    {
        LL_DEBUGS("ThreadPool") << mName << " closing queue and joining threads" << LL_ENDL;
        mQueue->close();
        for (auto& pair: mThreads)
        {
            if (pair.second.joinable())
            {
                LL_DEBUGS("ThreadPool") << mName << " waiting on thread " << pair.first << LL_ENDL;
                pair.second.join();
            }
        }
        LL_DEBUGS("ThreadPool") << mName << " shutdown complete" << LL_ENDL;
    }
}

void LL::ThreadPoolBase::run(const std::string& name)
{
#if LL_WINDOWS
    // Try using sleepy_robin fiber scheduler.
    boost::fibers::use_scheduling_algorithm<sleepy_robin>();
#endif // LL_WINDOWS

    LL_DEBUGS("ThreadPool") << name << " starting" << LL_ENDL;
    run();
    LL_DEBUGS("ThreadPool") << name << " stopping" << LL_ENDL;
}

void LL::ThreadPoolBase::run()
{
    mQueue->runUntilClose();
}

//static
size_t LL::ThreadPoolBase::getConfiguredWidth(const std::string& name, size_t dft)
{
    LLSD poolSizes;
    try
    {
        poolSizes = LL::CommonControl::get("Global", "ThreadPoolSizes");
        // "ThreadPoolSizes" is actually a map containing the sizes of
        // interest -- or should be, if this process has an
        // LLViewerControlListener instance and its settings include
        // "ThreadPoolSizes". If we failed to retrieve it, perhaps we're in a
        // program that doesn't define that, or perhaps there's no such
        // setting, or perhaps we're asking too early, before the LLEventAPI
        // itself has been instantiated. In any of those cases, it seems worth
        // warning.
        if (! poolSizes.isDefined())
        {
            // Note: we don't warn about absence of an override key for a
            // particular ThreadPool name, that's fine. This warning is about
            // complete absence of a ThreadPoolSizes setting, which we expect
            // in a normal viewer session.
            LL_WARNS("ThreadPool") << "No 'ThreadPoolSizes' setting for ThreadPool '"
                                   << name << "'" << LL_ENDL;
        }
    }
    catch (const LL::CommonControl::Error& exc)
    {
        // We don't want ThreadPool to *require* LLViewerControlListener.
        // Just log it and carry on.
        LL_WARNS("ThreadPool") << "Can't check 'ThreadPoolSizes': " << exc.what() << LL_ENDL;
    }

    LL_DEBUGS("ThreadPool") << "ThreadPoolSizes = " << poolSizes << LL_ENDL;
    // LLSD treats an undefined value as an empty map when asked to retrieve a
    // key, so we don't need this to be conditional.
    LLSD sizeSpec{ poolSizes[name] };
    // We retrieve sizeSpec as LLSD, rather than immediately as LLSD::Integer,
    // so we can distinguish the case when it's undefined.
    return sizeSpec.isInteger() ? sizeSpec.asInteger() : dft;
}

//static
size_t LL::ThreadPoolBase::getWidth(const std::string& name, size_t dft)
{
    auto instance{ getInstance(name) };
    if (instance)
    {
        return instance->getWidth();
    }
    else
    {
        return getConfiguredWidth(name, dft);
    }
}
