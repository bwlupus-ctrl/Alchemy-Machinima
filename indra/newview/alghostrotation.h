/** Pure clone orientation math. Radians; +X forward, +Y side, +Z up.
 * Yaw world Z, then local side pitch (positive dives), then local forward roll.
 * Quaternion storage is xyzw, matching LLQuaternion. No viewer dependencies.
 */
#ifndef AL_GHOST_ROTATION_H
#define AL_GHOST_ROTATION_H
#include <array>
#include <algorithm>
#include <cmath>

namespace ALGhostRotation
{
using Angles = std::array<double, 3>; // yaw, pitch, roll
using Quaternion = std::array<double, 4>;
constexpr double PI = 3.14159265358979323846;
inline double wrap(double value)
{
    return std::remainder(value, 2.0 * PI);
}
inline Quaternion compose(const Angles& a)
{
    const double cy = std::cos(a[0] * .5), sy = std::sin(a[0] * .5);
    const double cp = std::cos(a[1] * .5), sp = std::sin(a[1] * .5);
    const double cr = std::cos(a[2] * .5), sr = std::sin(a[2] * .5);
    // Hamilton Z * Y * X = LLQuaternion rollX * pitchY * yawZ.
    return {cy*cp*sr - sy*sp*cr, cy*sp*cr + sy*cp*sr,
            sy*cp*cr - cy*sp*sr, cy*cp*cr + sy*sp*sr};
}
inline Angles decompose(Quaternion q, const Angles& reference, bool use_reference = true)
{
    double length = 0.0;
    for (double v : q) length += v*v;
    length = std::sqrt(length);
    if (!(length > 0.0) || !std::isfinite(length)) return reference;
    for (double& v : q) v /= length;
    // A committed edit is stored as F32 by LLQuaternion. Retain its authored
    // angles when only that quantization separates it from our reference;
    // atan2 becomes ill-conditioned close to vertical even before exact gimbal.
    const auto authored = compose(reference);
    double direct = 0.0, negated = 0.0;
    for (int i=0; i<4; ++i)
    {
        direct += (q[i]-authored[i])*(q[i]-authored[i]);
        negated += (q[i]+authored[i])*(q[i]+authored[i]);
    }
    if (use_reference && std::min(direct, negated) < 1.e-14) return reference;
    const double x=q[0], y=q[1], z=q[2], w=q[3];
    const double forward_x=1-2*(y*y+z*z), forward_y=2*(x*y+w*z);
    const double sin_pitch=std::clamp(2*(w*y-z*x), -1.0, 1.0);
    Angles a {std::atan2(forward_y, forward_x), std::asin(sin_pitch),
              std::atan2(2*(y*z+w*x), 1-2*(x*x+y*y))};
    // Only collapse the singularity inside float quaternion precision. Keep yaw
    // from the previous read/edit and solve the coupled roll, never zero it.
    if (std::hypot(forward_x, forward_y) < 1.e-6)
    {
        a[0] = use_reference ? reference[0] : 0.0;
        a[1] = std::copysign(PI*.5, sin_pitch);
        const double coupled = std::atan2(2*(x*y-w*z), 1-2*(x*x+z*z));
        a[2] = std::copysign(1.0, sin_pitch)*(coupled + a[0]);
    }
    else
    {
        const Angles alternate {wrap(a[0]+PI), wrap(PI-a[1]), wrap(a[2]+PI)};
        auto distance = [&](const Angles& candidate)
        {
            double result=0.0;
            for (int i=0; i<3; ++i) { double d=wrap(candidate[i]-reference[i]); result+=d*d; }
            return result;
        };
        if (use_reference && distance(alternate) < distance(a)) a=alternate;
    }
    for (double& v : a) v=wrap(v);
    return a;
}
inline Angles decompose(Quaternion q) { return decompose(q, {}, false); }
}
#endif
