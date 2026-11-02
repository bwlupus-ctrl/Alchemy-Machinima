/** Session-only Clone Studio assignment and fade math. No viewer dependencies. */
#ifndef AL_CLONEQOL_H
#define AL_CLONEQOL_H
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace ALCloneQol
{
inline double randomUnit(uint64_t seed, uint64_t id)
{
    uint64_t x = id ^ (seed + 0x9e3779b97f4a7c15ULL);
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return double((x ^ (x >> 31)) >> 11) / 9007199254740992.;
}
inline double ripple(double x, double y, int direction)
{ return direction == 2 ? std::hypot(x, y) : direction == 1 ? -y : -x; }
inline double normalise(double value, double low, double high)
{ return high > low ? std::clamp((value-low)/(high-low), 0., 1.) : 0.; }
inline unsigned lookIndex(uint64_t seed, uint64_t id, unsigned row, bool by_row, unsigned count)
{ return count ? (by_row ? row % count : std::min(count-1, unsigned(randomUnit(seed, id)*count))) : 0; }
struct Fade
{
    double start = 0., duration = 0.;
    float from = 1.f;
    bool removing = false;
    float alpha(double now) const
    {
        const double t = duration > 0. ? std::clamp((now-start)/duration, 0., 1.) : 1.;
        const float smooth = float(t*t*(3.-2.*t));
        return removing ? from*(1.f-smooth) : from+(1.f-from)*smooth;
    }
    bool done(double now) const { return now >= start+duration; }
};
inline bool visible(bool master, bool enabled, bool solo, bool member)
{ return master && enabled && (!solo || member); }
}
#endif
