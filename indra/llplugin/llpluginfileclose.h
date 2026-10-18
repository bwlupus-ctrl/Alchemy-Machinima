/** Cross-thread close acknowledgement. Only the parent touches the process. */
#ifndef LL_LLPLUGINFILECLOSE_H
#define LL_LLPLUGINFILECLOSE_H
#include <atomic>
#include <chrono>

struct LLPluginFileClose
{
    std::atomic<bool> closed{false}, force{false};
    bool timedOut = false; // maintenance worker only
    double deadline = 0.; // maintenance worker only
    bool poll()
    {
        // LLTimer/LLFrameTimer update main-thread counters even on reads.
        return poll(std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    bool poll(double now)
    {
        if (closed.load()) return true;
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
