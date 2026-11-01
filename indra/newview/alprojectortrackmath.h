/** Client-only projector transforms; viewer-free math shared with the TUT tests. */
#ifndef AL_PROJECTOR_TRACK_MATH_H
#define AL_PROJECTOR_TRACK_MATH_H
#include "v3math.h"
#include "llquaternion.h"
#include <cmath>
namespace ALProjectorTrackMath
{
struct Pose { LLVector3 position; LLQuaternion rotation; };
inline Pose relative(const Pose& world, const Pose& frame)
{
    return { (world.position - frame.position) * ~frame.rotation,
             world.rotation * ~frame.rotation };
}
inline Pose apply(const Pose& offset, const Pose& frame)
{
    return { frame.position + offset.position * frame.rotation,
             offset.rotation * frame.rotation };
}
// Local -Z is the beam; local +Y is the gobo's up. Preserve the last up
// close to a pole, where projecting world Z would amplify tiny joint noise.
inline LLQuaternion aim(LLVector3 direction, const LLQuaternion& previous)
{
    if (!direction.isFinite() || direction.normalize() < 1.e-6f) return previous;
    const LLVector3 z = -direction;
    LLVector3 up = std::fabs(z.mV[2]) < .98f ? LLVector3::z_axis
                                            : LLVector3::y_axis * previous;
    LLVector3 x = up % z;
    if (x.normalize() < 1.e-4f)
    {
        up = std::fabs(z.mV[1]) < .9f ? LLVector3::y_axis : LLVector3::x_axis;
        x = up % z;
        x.normalize();
    }
    LLVector3 y = z % x;
    y.normalize();
    return LLQuaternion(x, y, z);
}
inline F32 weight(F32 dt, F32 seconds)
{
    if (seconds <= 0.f) return 1.f;
    return -std::expm1(-llmax(0.f, dt) / seconds);
}
inline Pose smooth(const Pose& from, const Pose& to, F32 dt, F32 seconds)
{
    const F32 a = weight(dt, seconds);
    return { from.position + (to.position - from.position) * a,
             slerp(a, from.rotation, to.rotation) };
}
}
#endif
