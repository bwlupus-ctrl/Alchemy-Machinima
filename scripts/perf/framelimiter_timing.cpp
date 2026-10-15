// [FrameLimiter] Standalone timing check for the frame rate cap. Drives the real
// ALFrameLimiter::endFrameWait (indra/newview/alframelimiter.cpp) with a busy
// "render" of a fixed length and reports the frame-start to frame-start
// interval. Build and run with framelimiter_timing.ps1.
//   framelimiter_timing.exe <fps> [work_us=8000] [frames=300] [post_us=0]
// post_us is extra work done after the wait (lateness the pacer must absorb).
#include "alframelimiter.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv)
{
    using C = std::chrono::steady_clock;
    const int fps     = argc > 1 ? std::atoi(argv[1]) : 30;
    const int work_us = argc > 2 ? std::atoi(argv[2]) : 8000;
    const int frames  = argc > 3 ? std::atoi(argv[3]) : 300;
    const int post_us = argc > 4 ? std::atoi(argv[4]) : 0;
    auto busy = [](int us)
    {
        const auto s = C::now();
        while (std::chrono::duration<double, std::micro>(C::now() - s).count() < us) {}
    };
    std::vector<double> d;
    C::time_point prev{};
    for (int i = 0; i < frames; ++i)
    {
        const auto start = C::now();
        // The first interval has no wait (the chain anchors on frame 0): skip it.
        if (i > 1) d.push_back(std::chrono::duration<double, std::milli>(start - prev).count());
        prev = start;
        busy(work_us);
        ALFrameLimiter::endFrameWait(fps);
        busy(post_us);
    }
    ALFrameLimiter::shutdown();
    std::sort(d.begin(), d.end());
    double sum = 0;
    for (double x : d) sum += x;
    const double mean = sum / d.size();
    std::printf("fps=%-3d work=%dus post=%dus mean=%.3fms (%.2f fps) min=%.3f p50=%.3f max=%.3f\n",
                fps, work_us, post_us, mean, 1000.0 / mean, d.front(), d[d.size() / 2], d.back());
    return 0;
}
