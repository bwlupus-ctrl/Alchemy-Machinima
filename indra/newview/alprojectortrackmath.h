/** Client-only projector transforms; viewer-free math shared with the TUT tests. */
#ifndef AL_PROJECTOR_TRACK_MATH_H
#define AL_PROJECTOR_TRACK_MATH_H
#include "v3math.h"
#include "llquaternion.h"
#include "m4math.h"
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
// Roots can change region origin; child locals require the packet's parent.
inline bool restorableFrame(bool packet_child, bool resolved_child, bool same_parent)
{
    return packet_child == resolved_child && (!packet_child || same_parent);
}
inline LLVector3 renderedJoint(const LLVector3& native, const LLMatrix4& outer)
{
    return native * outer;
}
inline LLQuaternion bodyYaw(const LLQuaternion& body)
{
    const LLVector3 forward = LLVector3::x_axis * body;
    return LLQuaternion(std::atan2(forward.mV[1], forward.mV[0]), LLVector3::z_axis);
}
// Local -Z is the beam, +Y the gobo up. Transport the previous up, then
// attract toward the nearest hemisphere of world-up outside the pole cone.
// Correction is bounded by beam travel: no threshold can half-turn the gobo.
inline LLQuaternion aim(LLVector3 direction, const LLQuaternion& previous)
{
    if (!direction.isFinite() || direction.normalize() < 1.e-6f) return previous;
    const LLVector3 z = -direction;
    const LLVector3 old_z = LLVector3::z_axis * previous;
    LLQuaternion transport;
    transport.shortestArc(old_z, z);
    LLVector3 y = (LLVector3::y_axis * previous) * transport;
    y -= z * (y * z);
    if (y.normalize() < 1.e-6f) return previous;
    LLVector3 world_up = LLVector3::z_axis - z * z.mV[2];
    const F32 projection = world_up.normalize();
    if (projection > 1.e-6f)
    {
        if (world_up * y < 0.f) world_up = -world_up;
        const F32 angle = std::atan2(z * (y % world_up), y * world_up);
        const F32 travel = std::atan2((old_z % z).length(), old_z * z);
        const F32 blend = llclamp((projection - .1f) / .3f, 0.f, 1.f);
        y = y * LLQuaternion(llclamp(angle, -travel * 2.f, travel * 2.f) * blend, z);
    }
    LLVector3 x = y % z;
    x.normalize();
    y = z % x;
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
