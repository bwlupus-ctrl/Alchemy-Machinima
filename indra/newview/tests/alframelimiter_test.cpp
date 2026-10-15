// [FrameLimiter] Pacing maths and the real FramePacer driven by a fake clock;
// header-only, registered directly.
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alframelimiter.h"
#include <cmath>
#include <limits>
#include <vector>

namespace tut
{
struct framelimiter_data {};
typedef test_group<framelimiter_data> framelimiter_group;
typedef framelimiter_group::object framelimiter_object;
framelimiter_group gFrameLimiterGroup("alframelimiter");

namespace
{
bool near_s(double a, double b, double tol = 1e-9) { return std::fabs(a - b) <= tol; }

// Deterministic clock: sleepFor oversleeps by a pseudo-random amount (like a
// coarse Windows timer), spinUntil lands exactly on the deadline.
class FakeClock final : public ALFrameLimiter::IClock
{
public:
    double t = 0.0;
    double maxOversleep = 0.0;   // seconds; 0 = perfect timer
    int    sleeps = 0;
    int    spins = 0;
    double lastSleep = 0.0;
    unsigned seed = 12345u;

    double now() override { return t; }
    void sleepFor(double s) override
    {
        ++sleeps;
        lastSleep = s;
        seed = seed * 1664525u + 1013904223u;
        const double r = static_cast<double>((seed >> 8) & 0xFFFF) / 65535.0;
        t += s + r * maxOversleep;
    }
    void spinUntil(double d) override
    {
        ++spins;
        if (t < d) t = d;
    }
};

// Simulate frames: do `work`, pace, then `overhead` of post-wait work before
// the next frame "starts". Returns the frame start times.
std::vector<double> runFrames(FakeClock& clk, ALFrameLimiter::FramePacer& pacer, int fps,
                              const std::vector<double>& work, double overhead, double spin)
{
    std::vector<double> starts;
    for (double w : work)
    {
        clk.t += w;
        pacer.wait(fps, clk, spin);
        clk.t += overhead;
        starts.push_back(clk.t);
    }
    return starts;
}

double meanInterval(const std::vector<double>& s, size_t from, size_t to)
{
    return (s[to] - s[from]) / static_cast<double>(to - from);
}
}

// Interval for each FPS, clamping.
template<> template<> void framelimiter_object::test<1>()
{
    ensure("30 fps interval", near_s(ALFrameLimiter::targetInterval(30), 1.0 / 30.0));
    ensure("60 fps interval", near_s(ALFrameLimiter::targetInterval(60), 1.0 / 60.0));
    ensure("clamped low", near_s(ALFrameLimiter::targetInterval(0), 0.1));
    ensure("clamped negative", near_s(ALFrameLimiter::targetInterval(-5), 0.1));
    ensure("clamped high", near_s(ALFrameLimiter::targetInterval(100000), 1.0 / 240.0));
    ensure_equals("clampFps low", ALFrameLimiter::clampFps(3), 10);
    ensure_equals("clampFps high", ALFrameLimiter::clampFps(999), 240);
    ensure_equals("clampFps mid", ALFrameLimiter::clampFps(75), 75);
}

// The pure deadline step, including the 30 fps acceptance numbers.
template<> template<> void framelimiter_object::test<2>()
{
    const double I = 1.0 / 30.0;
    const double spin = ALFrameLimiter::SPIN_SECONDS_HIRES;
    // First frame: nothing to wait for, chain anchors on now.
    ALFrameLimiter::NextStep s = ALFrameLimiter::planNext(false, 0.0, 5.0, I, spin);
    ensure_equals("first frame no wait", s.plan.total(), 0.0);
    ensure("first frame anchors to now", near_s(s.deadline, 5.0));
    // Previous deadline 0, frame done at 10 ms -> wait 23.33 ms, the last 0.5 ms of it spin.
    s = ALFrameLimiter::planNext(true, 0.0, 0.010, I, spin);
    ensure("10 ms elapsed waits 23.33 ms", near_s(s.plan.total(), I - 0.010));
    ensure("spin is the last 0.5 ms", near_s(s.plan.spinSeconds, 0.0005));
    ensure("sleep is the bulk", near_s(s.plan.sleepSeconds, I - 0.010 - 0.0005));
    ensure("deadline is prev + interval", near_s(s.deadline, I));
    // 40 ms elapsed: late, no wait, chain re-anchors to now (no catch-up).
    s = ALFrameLimiter::planNext(true, 0.0, 0.040, I, spin);
    ensure_equals("40 ms elapsed no wait", s.plan.total(), 0.0);
    ensure("40 ms re-anchors to now", near_s(s.deadline, 0.040));
    // Exactly on the deadline: no wait.
    s = ALFrameLimiter::planNext(true, 0.0, I, I, spin);
    ensure_equals("exact deadline no wait", s.plan.total(), 0.0);
    // Less than the spin window left: all spin, no sleep.
    s = ALFrameLimiter::planNext(true, 0.0, I - 0.0004, I, 0.001);
    ensure("short remainder is all spin", near_s(s.plan.spinSeconds, 0.0004));
    ensure_equals("short remainder no sleep", s.plan.sleepSeconds, 0.0);
    // Wider fallback spin window; no spin requested.
    s = ALFrameLimiter::planNext(true, 0.0, 0.010, I, ALFrameLimiter::SPIN_SECONDS_COARSE);
    ensure("coarse spin window", near_s(s.plan.spinSeconds, ALFrameLimiter::SPIN_SECONDS_COARSE));
    s = ALFrameLimiter::planNext(true, 0.0, 0.010, I, 0.0);
    ensure_equals("no spin", s.plan.spinSeconds, 0.0);
}

// Bad inputs fail open (no wait, never a stall).
template<> template<> void framelimiter_object::test<3>()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    ensure_equals("NaN now", ALFrameLimiter::planNext(true, 0.0, nan, 0.033, 0.001).plan.total(), 0.0);
    ensure_equals("inf now", ALFrameLimiter::planNext(true, 0.0, inf, 0.033, 0.001).plan.total(), 0.0);
    ensure_equals("NaN prev", ALFrameLimiter::planNext(true, nan, 1.0, 0.033, 0.001).plan.total(), 0.0);
    ensure_equals("zero interval", ALFrameLimiter::planNext(true, 0.0, 0.0, 0.0, 0.001).plan.total(), 0.0);
    ensure_equals("NaN interval", ALFrameLimiter::planNext(true, 0.0, 0.0, nan, 0.001).plan.total(), 0.0);
}

// The real FramePacer on a fake clock with a perfect timer: every frame start
// is exactly one interval after the last, across the fps range.
template<> template<> void framelimiter_object::test<4>()
{
    for (int fps = ALFrameLimiter::MIN_FPS; fps <= ALFrameLimiter::MAX_FPS; fps += 7)
    {
        const double I = ALFrameLimiter::targetInterval(fps);
        FakeClock clk;
        ALFrameLimiter::FramePacer pacer;
        const std::vector<double> work(120, I * 0.25);
        const std::vector<double> starts = runFrames(clk, pacer, fps, work, 0.0, ALFrameLimiter::SPIN_SECONDS_HIRES);
        for (size_t i = 2; i < starts.size(); ++i)
        {
            ensure("fixed interval with a perfect timer", near_s(starts[i] - starts[i - 1], I, 1e-9));
        }
    }
}

// Long-run average equals the cap with timer oversleep AND post-wait overhead:
// lateness is absorbed by the next interval instead of pulling the average
// under the cap. Checked at 30, 60 and 144.
template<> template<> void framelimiter_object::test<5>()
{
    const int rates[] = { 30, 60, 144 };
    for (int fps : rates)
    {
        const double I = ALFrameLimiter::targetInterval(fps);
        FakeClock clk;
        clk.maxOversleep = 0.0004;           // wakes up to 0.4 ms late (inside the 0.5 ms spin window)
        ALFrameLimiter::FramePacer pacer;
        std::vector<double> work;
        for (int i = 0; i < 600; ++i)
        {
            work.push_back(I * (0.15 + 0.4 * ((i * 37) % 11) / 10.0)); // 15%-55% of the interval
        }
        const std::vector<double> starts =
            runFrames(clk, pacer, fps, work, 0.0004, ALFrameLimiter::SPIN_SECONDS_HIRES);
        const double avg = meanInterval(starts, 10, starts.size() - 1);
        ensure("average interval equals the cap (oversleep + overhead)", near_s(avg, I, 1e-6));
        ensure("effective fps equals the cap", near_s(1.0 / avg, static_cast<double>(fps), 0.01));
    }
    // A timer that overshoots by MORE than the spin window (up to 2.5 ms past the
    // deadline) still averages the cap: the next wait is shortened to compensate.
    {
        const double I = ALFrameLimiter::targetInterval(30);
        FakeClock clk;
        clk.maxOversleep = 0.003;
        ALFrameLimiter::FramePacer pacer;
        const std::vector<double> starts = runFrames(clk, pacer, 30, std::vector<double>(400, 0.010), 0.001,
                                                     ALFrameLimiter::SPIN_SECONDS_HIRES);
        ensure("average holds despite 3 ms oversleep",
               near_s(meanInterval(starts, 10, starts.size() - 1), I, 1e-5));
    }
}

// A slow frame is not repaid: the frame after it starts a full interval after
// the slow frame ended, and later frames keep the normal interval (no burst of
// short frames).
template<> template<> void framelimiter_object::test<6>()
{
    const int fps = 30;
    const double I = ALFrameLimiter::targetInterval(fps);
    FakeClock clk;
    ALFrameLimiter::FramePacer pacer;
    std::vector<double> work(40, 0.010);
    work[15] = 0.200;  // one 200 ms hitch
    work[28] = 0.050;  // and a 50 ms frame
    const std::vector<double> starts = runFrames(clk, pacer, fps, work, 0.0, ALFrameLimiter::SPIN_SECONDS_HIRES);
    ensure("hitch interval is the hitch itself", near_s(starts[15] - starts[14], 0.200, 1e-9));
    ensure("50 ms frame interval is the frame itself", near_s(starts[28] - starts[27], 0.050, 1e-9));
    for (size_t i = 2; i < starts.size(); ++i)
    {
        const double dt = starts[i] - starts[i - 1];
        ensure("no interval shorter than the cap (no catch-up burst)", dt >= I - 1e-9);
        if (i != 15 && i != 28)
        {
            ensure("all other intervals are exactly the cap", near_s(dt, I, 1e-9));
        }
    }
}

// First call (or after reset) never waits; cap changes apply on the next frame.
template<> template<> void framelimiter_object::test<7>()
{
    FakeClock clk;
    ALFrameLimiter::FramePacer pacer;
    clk.t = 123.0;
    ensure_equals("first call waits zero", pacer.wait(30, clk, 0.001), 0.0);
    ensure("anchored", pacer.anchored());
    ensure_equals("no sleep on first call", clk.sleeps, 0);
    clk.t += 0.005;
    const double waited = pacer.wait(30, clk, 0.001);
    ensure("second call waits the remainder", near_s(waited, 1.0 / 30.0 - 0.005, 1e-9));
    ensure_equals("sleep used for the bulk", clk.sleeps, 1);
    ensure("sleep excludes the spin window", near_s(clk.lastSleep, 1.0 / 30.0 - 0.005 - 0.001, 1e-9));
    ensure_equals("spin used for the tail", clk.spins, 1);
    pacer.reset();
    clk.t += 0.001;
    ensure_equals("after reset waits zero", pacer.wait(30, clk, 0.001), 0.0);
    clk.t += 0.002;
    const double w60 = pacer.wait(60, clk, 0.001);
    ensure("cap change to 60 applies", near_s(w60, 1.0 / 60.0 - 0.002, 1e-9));
}
}
