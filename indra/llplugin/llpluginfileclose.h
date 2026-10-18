/** Cross-thread close acknowledgement. Only the parent touches LLProcess state. */
#ifndef LL_LLPLUGINFILECLOSE_H
#define LL_LLPLUGINFILECLOSE_H
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#if LL_WINDOWS
#include "llwin32headers.h"
#endif

struct LLPluginFileClose
{
    std::atomic<bool> closed{false}, force{false};
    bool timedOut = false; // maintenance worker only
    double deadline = 0.; // maintenance worker only
    // Published by parent, read by maintenance. The callback owns an OS process
    // observation handle, never the parent or an LLProcess with main-thread state.
    std::atomic<std::shared_ptr<const std::function<bool()>>> processExited;
    void observeExitWith(std::function<bool()> observe)
    { processExited.store(std::make_shared<const std::function<bool()>>(std::move(observe))); }
#if LL_WINDOWS
    void observeProcessHandle(HANDLE process)
    {
        if (processExited.load()) return;
        HANDLE duplicate = nullptr;
        if (!DuplicateHandle(GetCurrentProcess(), process, GetCurrentProcess(), &duplicate, SYNCHRONIZE, FALSE, 0)) return;
        auto handle = std::shared_ptr<void>(duplicate, [](void* h) { CloseHandle(h); });
        observeExitWith([handle]() { return WaitForSingleObject(handle.get(), 0) == WAIT_OBJECT_0; });
    }
#endif
    bool poll()
    {
        // LLTimer/LLFrameTimer update main-thread counters even on reads.
        return poll(std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    bool poll(double now)
    {
        if (closed.load()) return true;
        const auto exited = processExited.load();
        if (exited && (*exited)()) { closed = true; return true; }
        if (!deadline) deadline = now + 5.;
        if (now >= deadline && !timedOut) { timedOut = true; force = true; }
        // A timeout requests termination; it never grants permission to unlink.
        return closed.load();
    }
    template<class Running, class Terminate>
    void observeProcess(bool owned, bool launching, Running running, Terminate terminate)
    {
        if (closed.load()) return;
        if (!owned) { if (!launching) closed = true; return; }
        // Only the main-thread parent observes/reaps/signals its owned process.
        // Once reaped, this numeric PID may already belong to somebody else.
        if (!running()) { closed = true; return; }
        if (force.exchange(false)) terminate();
        // Requesting termination is never a close acknowledgement.
    }
};
#endif
