/** Pure pose-flip ownership and pose publication rules. */
#ifndef AL_ALGHOSTANIMMIRRORMATH_H
#define AL_ALGHOSTANIMMIRRORMATH_H

#include "alreflectmath.h"
#include <string_view>
#include <array>
#include <algorithm>
#include <cstring>

namespace ALGhostAnimMirrorMath
{
// Kept index-parallel with ALGhostStudio::EDriveMode by static assertions there.
enum Drive : S32 { DRIVE_MIRROR, DRIVE_DIRECTED, DRIVE_FROZEN, DRIVE_TRUE_MIRROR, DRIVE_REFLECT };
inline constexpr bool poseFlipDrive(S32 drive)
{
    return drive == DRIVE_MIRROR || drive == DRIVE_DIRECTED || drive == DRIVE_TRUE_MIRROR;
}
inline constexpr bool directedMirrorActive(S32 drive, S32 resume_drive, bool mirror)
{
    return mirror && (poseFlipDrive(drive) ||
        (drive == DRIVE_FROZEN && poseFlipDrive(resume_drive)));
}
inline constexpr bool eligible(S32 authored, S32 resume_drive, bool mirror, S32 runtime)
{
    // Do not flip a temporary Director performance borrowed by another drive.
    // Retain the existing Directed -> Director Freeze presentation contract.
    return directedMirrorActive(authored, resume_drive, mirror) &&
        (runtime == authored || (authored == DRIVE_DIRECTED && runtime == DRIVE_FROZEN));
}
enum GazeJoint : U32
{
    GAZE_PELVIS = 1, GAZE_TORSO = 2, GAZE_CHEST = 4,
    GAZE_NECK = 8, GAZE_HEAD = 16, GAZE_EYES = 32
};
inline U32 gazeJointBit(std::string_view name)
{
    if (name == "mPelvis") return GAZE_PELVIS;
    if (name == "mTorso") return GAZE_TORSO;
    if (name == "mChest") return GAZE_CHEST;
    if (name == "mNeck") return GAZE_NECK;
    if (name == "mHead") return GAZE_HEAD;
    if (name == "mEyeLeft" || name == "mEyeRight" ||
        name == "mFaceEyeAltLeft" || name == "mFaceEyeAltRight") return GAZE_EYES;
    return 0;
}
inline bool excluded(U32 joint, U32 partner, U32 gaze)
{ return ((joint | partner) & gaze) != 0; }

inline bool rotationFinite(const LLQuaternion& rotation)
{
    for (F32 component : rotation.mQ) if (!ALReflect::finiteBits(component)) return false;
    return true;
}
// Six ownership groups, but distinct fixed slots for all four eye rotations.
inline size_t gazeJointSlot(std::string_view name)
{
    if (name == "mPelvis") return 0;
    if (name == "mTorso") return 1;
    if (name == "mChest") return 2;
    if (name == "mNeck") return 3;
    if (name == "mHead") return 4;
    if (name == "mEyeLeft") return 5;
    if (name == "mEyeRight") return 6;
    if (name == "mFaceEyeAltLeft") return 7;
    if (name == "mFaceEyeAltRight") return 8;
    return 9;
}
struct GazeSample
{
    bool written = false;
    F32 alpha = 0.f;
    LLQuaternion preLocal;
    LLQuaternion targetWorld;
};
// Last evaluation only; no evaluation means retain the weights AND endpoints.
struct GazeWeights
{
    std::array<F32, 6> alpha{};
    std::array<GazeSample, 9> samples{};
    void record(U32 bits, F32 weight)
    {
        if (!ALReflect::finiteBits(weight)) return;
        weight = std::clamp(weight, 0.f, 1.f);
        for (size_t i = 0; i < alpha.size(); ++i)
            if (bits & (1u << i)) alpha[i] = std::max(alpha[i], weight);
    }
    void observe(std::string_view name, F32 weight, const LLQuaternion& pre_local,
                 const LLQuaternion& target_world)
    {
        const size_t slot = gazeJointSlot(name);
        if (slot >= samples.size() || !ALReflect::finiteBits(weight) || weight <= 0.f ||
            !rotationFinite(pre_local) || !rotationFinite(target_world)) return;
        record(gazeJointBit(name), weight);
        auto& sample = samples[slot];
        // Repeated writes: first animation local, maximum applied alpha, final
        // unblended aimed world result. Never feed the eased local back as a target.
        if (!sample.written) sample.preLocal = pre_local;
        sample.targetWorld = target_world;
        sample.alpha = std::max(sample.alpha, std::clamp(weight, 0.f, 1.f));
        sample.written = true;
    }
    F32 weight(U32 bits) const
    {
        F32 result = 0.f;
        for (size_t i = 0; i < alpha.size(); ++i)
            if (bits & (1u << i)) result = std::max(result, alpha[i]);
        return result;
    }
    void merge(const GazeWeights& other)
    {
        for (size_t i = 0; i < alpha.size(); ++i) alpha[i] = std::max(alpha[i], other.alpha[i]);
        for (size_t i = 0; i < samples.size(); ++i)
            if (other.samples[i].written && (!samples[i].written || other.samples[i].alpha >= samples[i].alpha))
                samples[i] = other.samples[i];
    }
};
inline bool poseFinite(const ALReflect::LocalPose& pose)
{
    if (!rotationFinite(pose.rotation)) return false;
    for (F32 component : pose.position.mV) if (!ALReflect::finiteBits(component)) return false;
    return true;
}

inline bool sameRotationBits(const LLQuaternion& a, const LLQuaternion& b)
{ return std::memcmp(a.mQ, b.mQ, sizeof(a.mQ)) == 0; }
inline bool samePositionBits(const LLVector3& a, const LLVector3& b)
{ return std::memcmp(a.mV, b.mV, sizeof(a.mV)) == 0; }
inline ALReflect::LocalPose guardedRestore(const ALReflect::LocalPose& current,
                                          const ALReflect::LocalPose& input,
                                          const ALReflect::LocalPose& written)
{
    return {sameRotationBits(current.rotation, written.rotation) ? input.rotation : current.rotation,
            samePositionBits(current.position, written.position) ? input.position : current.position};
}
// Validate symmetry once when the cache is built. The frame kernel retains
// bounds safety. A bad channel isolates its pair, never poisons other joints.
inline bool validatePartners(const size_t* partners, size_t count, size_t& bad)
{
    for (size_t i = 0; i < count; ++i)
        if (partners[i] >= count || partners[partners[i]] != i) { bad = i; return false; }
    return true;
}
inline bool reflectPose(const ALReflect::LocalPose* input, ALReflect::LocalPose* output,
                        const size_t* partners, const U8* exclusions, size_t count, size_t& bad)
{
    for (size_t i = 0; i < count; ++i)
        if (partners[i] >= count) { bad = i; return false; }
    bad = count;
    for (size_t i = 0; i < count; ++i)
    {
        const bool valid = poseFinite(input[i]) && poseFinite(input[partners[i]]);
        if (!poseFinite(input[i]) && bad == count) bad = i;
        if (!valid || (exclusions && (exclusions[i] || exclusions[partners[i]]))) output[i] = input[i];
        else ALReflect::reflectLocalPose(input[partners[i]], output[i]);
    }
    return true;
}

// Parent-before-child skeleton order; count denotes the unchanged external root.
// Scratch storage belongs to the clone. No allocations or scene graph writes.
inline bool preserveGaze(const ALReflect::LocalPose* input, ALReflect::LocalPose* output,
                         const size_t* partners, const size_t* parents, const F32* alpha, size_t count,
                         const LLQuaternion& root, LLQuaternion* before, LLQuaternion* after,
                         const GazeSample* samples = nullptr)
{
    for (size_t i = 0; i < count; ++i)
        if (partners[i] >= count || (parents[i] != count && parents[i] >= i)) return false;
    for (size_t i = 0; i < count; ++i)
    {
        const size_t parent = parents[i];
        const LLQuaternion& pre_parent = parent < count ? before[parent] : root;
        const LLQuaternion& post_parent = parent < count ? after[parent] : root;
        // Propagate invalidity without doing quaternion arithmetic on NaNs.
        before[i] = !rotationFinite(pre_parent) ? pre_parent :
            (!rotationFinite(input[i].rotation) ? input[i].rotation : input[i].rotation * pre_parent);
        if (alpha[i] > 0.f && poseFinite(input[i]) && poseFinite(input[partners[i]]) && poseFinite(output[i]))
        {
            LLQuaternion mirrored_local = output[i].rotation;
            const size_t partner = partners[i];
            if (samples && samples[partner].written && rotationFinite(samples[partner].preLocal))
                mirrored_local = ALReflect::rotation(samples[partner].preLocal);
            // A shared ownership group does not imply an actual joint write.
            // Unobserved eyes follow the mirrored parent using partner animation.
            const bool observed = !samples || samples[i].written;
            if (observed)
            {
                const LLQuaternion& target_world = samples ? samples[i].targetWorld : before[i];
                LLQuaternion aimed = input[i].rotation;
                if (rotationFinite(target_world) && rotationFinite(post_parent))
                    aimed = target_world * ~post_parent;
                output[i].rotation = nlerp(alpha[i], mirrored_local, aimed);
            }
            else output[i].rotation = mirrored_local;
            // Gaze writes rotations only; retain reflectPose's animated position.
        }
        after[i] = !rotationFinite(post_parent) ? post_parent :
            (!rotationFinite(output[i].rotation) ? output[i].rotation : output[i].rotation * post_parent);
    }
    return true;
}
}
#endif
