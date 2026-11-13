/**
 * @file alghoststayinplace.h
 * @brief Entity clone "Stay in place": root placement compensation math.
 *
 * Pure and type-generic (the viewer passes LLVector3 / LLQuaternion, the
 * native fixture its own doubles). Nothing here reads or writes a skeleton.
 */
#ifndef AL_GHOST_STAY_IN_PLACE_H
#define AL_GHOST_STAY_IN_PLACE_H

#include <cmath>

namespace ALGhostStayInPlace
{
// A presented root within this squared distance (0.1 mm) of its target is left
// alone, so a held pose does not re-publish the clone every frame.
constexpr float PUBLISH_EPSILON_SQUARED = 1.e-8f;

// The offset the PRESENTED root takes on top of its authored placement so the
// hips stay over the clone's spot.
//
//  pelvis_local    the evaluated pelvis position in its parent (clone root) frame
//  rest_x          the rest reference forward offset: the active pelvis rest
//                  override's X, else 0. The sideways reference is always 0.
//  root_scale      the root joint's scale (joints scale their children's offsets)
//  root_rotation   the ACTUAL root world rotation, so a turned, tilted, flipped
//                  or floor-reflected root gives the right world direction
//  rendered_scale  how far one metre of skeleton drift is drawn: the clone's
//                  outer scale while its scale pivot rides the root, else 1
//
// Root-frame Z (height) is never compensated: sits and crouches still drop.
template <class Vector, class Quaternion>
Vector rootOffset(const Vector& pelvis_local, float rest_x, const Vector& root_scale,
                  const Quaternion& root_rotation, float rendered_scale)
{
    Vector drift(pelvis_local.mV[0] - rest_x, pelvis_local.mV[1], 0.f);
    drift.scaleVec(root_scale);
    Vector offset = drift * root_rotation;
    for (int axis = 0; axis < 3; ++axis)
    {
        offset.mV[axis] *= -rendered_scale;
        if (!std::isfinite(offset.mV[axis])) return Vector(0.f, 0.f, 0.f);
    }
    return offset;
}
}

#endif // AL_GHOST_STAY_IN_PLACE_H
