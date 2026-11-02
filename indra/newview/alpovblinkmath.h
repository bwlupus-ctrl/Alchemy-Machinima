/** POV blink: pure timing/scheduling math. No avatar or gaze dependencies. */
#ifndef AL_POV_BLINK_MATH_H
#define AL_POV_BLINK_MATH_H
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ALPOVBlinkMath
{
inline double unit(double x) { return std::isfinite(x) ? std::clamp(x, 0.0, 1.0) : 0.0; }
// Same C2 quintic envelope as ALGazeBlink, without its viewer math dependency.
inline double ease(double x) { x = unit(x); return x*x*x*(x*(6*x-15)+10); }
struct Params
{
    double close = .09, hold = .04, open = .18, depth = 1.;
};
inline double seconds(double x) { return std::isfinite(x) ? std::clamp(x, 0.0, 5.0) : 0.0; }
inline Params sanitize(Params p)
{
    return {seconds(p.close), seconds(p.hold), seconds(p.open), unit(p.depth)};
}
inline bool shouldDraw(double amount, bool live) { return live && amount > 0.; }

// The production admission/copy/draw wrapper. Backends own GL state, never the
// destination. Bands conservatively bound the curved feather, plus blur taps.
template<class Backend>
void present(double amount, bool live, int width, int height, double curve,
             double softness, double blur, bool sampling, Backend& backend)
{
    if (!shouldDraw(amount, live) || width <= 0 || height <= 0) return;
    if (sampling)
    {
        if (!backend.acquire(width, height)) return;
        const double c = unit(amount), f = std::max(unit(softness)*.15, 1./height);
        const double x = .5*width/height;
        const double arch = unit(curve)*x*x/(1.+x*x)*c*(1.-c);
        const int pad = static_cast<int>(std::ceil(1.+10.*unit(blur)*c));
        int bottom = std::clamp(static_cast<int>(std::ceil((-f+(.42+2.*f)*c+arch+f)*height))+pad, 0, height);
        int top = std::clamp(static_cast<int>(std::floor((1.+f-(.58+2.*f)*c-arch-f)*height))-pad, 0, height);
        if (bottom >= top) backend.copy(0, height);
        else
        {
            if (bottom > 0) backend.copy(0, bottom);
            if (top < height) backend.copy(top, height-top);
        }
    }
    backend.bind();
    backend.drawSceneImage();
}

// Shared mapping: short L3 click blinks on release; >=180 ms holds closed.
// Distinct mappings: tap fires on press, hold follows the button immediately.
struct PadGesture
{
    bool held = false;
    bool sample(double now, std::uint16_t raw, std::uint16_t tap,
                std::uint16_t hold, bool prime, bool focused)
    {
        if (prime || !focused || tap != lastTap || hold != lastHold)
        {
            previous = raw; pending = false; armed = false; held = false;
            lastTap = tap; lastHold = hold;
            return false;
        }
        bool fire = false;
        if (!(raw & hold)) armed = true;
        const bool pressed = (raw & tap) && !(previous & tap);
        if (tap && tap == hold)
        {
            if (pressed && armed) { pending = true; start = now; }
            held = pending && (raw & hold) && now-start >= .18;
            if (pending && !(raw & hold))
            {
                fire = now-start < .18;
                pending = false;
            }
        }
        else
        {
            held = armed && (raw & hold);
            fire = pressed;
        }
        previous = raw;
        return fire;
    }
private:
    std::uint16_t previous = 0, lastTap = 0, lastHold = 0;
    bool pending = false, armed = false;
    double start = 0.;
};

struct Scheduler
{
    static constexpr double refractory = .65;
    std::uint32_t seed = 0x71e1d123;
    double next = 0.;
    bool armed = false;
    double delay(double bpm)
    {
        bpm = std::isfinite(bpm) ? std::clamp(bpm, 1., 60.) : 15.;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        return std::max(refractory, (60./bpm)*(.55 + .9*(seed / 4294967295.)));
    }
    void defer(double now, double bpm) { next = now + delay(bpm); armed = true; }
};

class State
{
public:
    bool held = false;
    Scheduler scheduler;
    double sample(double now) const
    {
        const double t = std::max(0., now-start);
        if (phase == Closing)
        {
            if (held || t < p.close)
                return from + (p.depth-from)*ease(p.close > 0. ? t/p.close : 1.);
            if (t < p.close+p.hold) return p.depth;
            return p.depth*(1.-ease(p.open > 0. ? (t-p.close-p.hold)/p.open : 1.));
        }
        if (phase == Opening)
            return from*(1.-ease(p.open > 0. ? t/p.open : 1.));
        return 0.;
    }
    void blink(double now, Params params)
    {
        if (held) return;
        from = sample(now); p = sanitize(params); start = now; phase = Closing;
    }
    void hold(double now, bool value, Params params)
    {
        if (held == value) return;
        from = sample(now); p = sanitize(params); start = now;
        held = value; phase = value ? Closing : Opening;
    }
    double update(double now, Params params, bool automatic, double bpm)
    {
        const double end = start + (phase == Closing ? p.close+p.hold+p.open : p.open);
        if (!held && phase != Idle && now >= end)
        {
            phase = Idle;
            // Schedule from the observed end: no burst after a stall/capture.
            scheduler.defer(now, bpm);
        }
        if (!automatic) scheduler.armed = false;
        else if (!scheduler.armed) scheduler.defer(now, bpm);
        else if (!held && phase == Idle && now >= scheduler.next)
            blink(now, params);
        return sample(now);
    }
private:
    enum Phase { Idle, Closing, Opening } phase = Idle;
    Params p;
    double start = 0., from = 0.;
};
}
#endif
