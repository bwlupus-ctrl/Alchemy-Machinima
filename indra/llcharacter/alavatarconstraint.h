/**
 * @file alavatarconstraint.h
 * @brief Pure spatial relaxation shared with the keyframe constraint solver.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef AL_AVATARCONSTRAINT_H
#define AL_AVATARCONSTRAINT_H

#include "v3math.h"
#include <cmath>

namespace ALAvatarConstraint
{
inline F32 seedWeight(F32 sampled, F32 target, bool seed_spatial)
{
    return seed_spatial ? target : sampled;
}

inline bool fullStep(bool unit_speed, bool seed_spatial)
{
    return unit_speed || seed_spatial;
}

// A frozen activation/rebuild gets the same spatial iterations as a normal
// frame once. This does not change any animation clock or damping interval.
inline void relax(LLVector3* positions, LLVector3* velocities, const F32* joint_lengths,
                  S32 chain_length, S32 max_iteration_count, bool unit_speed, F32 step)
{
    constexpr F32 JOINT_LENGTH_K = 0.7f;
    constexpr S32 MIN_ITERATION_COUNT = 2;
    constexpr F32 MIN_ACCELERATION_SQUARED = 0.0005f * 0.0005f;
    S32 iteration_count;
    for (iteration_count = 0; iteration_count < max_iteration_count; iteration_count++)
    {
        S32 num_joints_finished = 0;
        for (S32 joint_num = 1; joint_num < chain_length; joint_num++)
        {
            // constraint to child
            LLVector3 acceleration = (positions[joint_num - 1] - positions[joint_num]) *
                (dist_vec(positions[joint_num], positions[joint_num - 1]) - joint_lengths[joint_num - 1]) * JOINT_LENGTH_K;
            // constraint to parent
            acceleration  += (positions[joint_num + 1] - positions[joint_num]) *
                (dist_vec(positions[joint_num + 1], positions[joint_num]) - joint_lengths[joint_num]) * JOINT_LENGTH_K;

            if (acceleration.magVecSquared() < MIN_ACCELERATION_SQUARED)
            {
                num_joints_finished++;
            }

            if (unit_speed)
            {
                velocities[joint_num - 1] = velocities[joint_num - 1] * 0.7f;
                positions[joint_num] += velocities[joint_num - 1] + (acceleration * 0.5f);
                velocities[joint_num - 1] += acceleration;
            }
            else
            {
                velocities[joint_num - 1] *= powf(0.7f, step);
                positions[joint_num] += (velocities[joint_num - 1] + acceleration * (0.5f * step)) * step;
                velocities[joint_num - 1] += acceleration * step;
            }
        }

        if ((iteration_count >= MIN_ITERATION_COUNT) &&
            (num_joints_finished == chain_length - 1))
        {
            break;
        }
    }

}
}
#endif
