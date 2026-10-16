/**
 * @file alsteppedanimruntime.h
 * @brief Viewer bridge for the render-window stepped-animation overlay.
 */
#ifndef AL_STEPPED_ANIM_RUNTIME_H
#define AL_STEPPED_ANIM_RUNTIME_H

#include "alsteppedanim.h"
#include "lluuid.h"
#include "llvoavatar.h"
#include "v3dmath.h"

#include <map>
#include <limits>
#include <memory>
#include <vector>

class ALSteppedFriendCache;
class LLDirectorCast;
class LLVOAvatar;
class LLViewerObject;

class ALSteppedAnimRuntime
{
public:
    static bool sActive;
    static bool sHasOverlay;
    static bool sStepParticles;
    static bool sStepTextureAnim;

    static ALSteppedAnimRuntime& instance();
    static void initialize();
    ~ALSteppedAnimRuntime();
    // The sanitized page config, for the panel's Effective readout (same function the planner reads).
    ALSteppedAnim::Config configSnapshot() const { return readConfig(); }

    void beginFrame();
    void applyHeld();
    void endFrameRestore();
    const ALSteppedAnim::FrameDecision* decisionFor(const LLVOAvatar* avatar, U64* particle_epoch = nullptr) const;
    bool holdsFlexi(const LLVOAvatar* avatar) const;
    // Lip-sync follows the Face part: held between the Face part's own refreshes while the actor's
    // face bones are held (Face part Smooth / StepAnimFace off: never).
    bool holdsFace(const LLVOAvatar* avatar) const;
    bool holdsAttachmentFX(const LLVOAvatar* avatar) const;
    bool holdsTextureAnim(const LLVOAvatar* avatar) const;
    F32 attachmentOmegaDt(LLViewerObject* object, F32 dt);
    F32 selectedLimbSpeed() const;
    S32 activeActorCount() const { return mActiveActors; }
    S32 skippedActorCount() const { return mSkippedActors; }

private:
    ALSteppedAnimRuntime() = default;

    struct Entry
    {
        ALSteppedAnim::ActorState mState;
        ALSteppedAnim::Assignment mAssignment;
        ALSteppedAnim::FrameDecision mDecision;
        ALSteppedAnim::PoseBuffer mSmooth, mHeld, mPreviousHeld, mCandidate, mWritten;
        // Pose-buffer order: skeleton joints, collision volumes, then the non-HUD attachment points
        // (mAttachPointIds, in attachment-id order). mParents indexes the same buffer (-1 = root).
        std::vector<U16> mClasses;
        std::vector<S32> mParents;
        std::vector<S32> mAttachPointIds;
        std::vector<ALSteppedAnim::JointLocal> mAttachHeld; // root-relative placements (Attachments part)
        // Smear tick per +1 / +2 part (set on that part's own capture, like ActorState::smearTick
        // for the torso grid), so a coarser limb smears only on its own ticks.
        std::array<bool, ALSteppedAnim::kPartGroups> mPartSmearTick{};
        std::array<S32, ALSteppedAnim::kEffectors> mEffectorIndices{};
        std::array<std::array<S32, 2>, 4> mSmearIndices{};
        std::array<std::array<S32, 2>, 4> mSmearChildIndices{};
        S32 mPelvisIndex = -1;
        std::vector<U8> mWrittenMask;
        ALSteppedAnim::PoseBuffer mVerifyPose;
        std::vector<U8> mVerifyMask;
        std::array<LLVector3, ALSteppedAnim::kEffectors> mHeldEffectors{};
        std::array<LLVector3, ALSteppedAnim::kEffectors> mPreviousEffectors{};
        bool mHaveHeld = false;
        bool mHavePreviousHeld = false;
        bool mHaveCandidate = false;
        F32 mCandidateSpeed = 0.f;
        S64 mLastEaseTick = std::numeric_limits<S64>::min();
        U32 mSkeletonSerial = 0;
        U32 mJointCount = 0;
        U32 mOverrideEpoch = 0;
        U64 mRegionHandle = 0;
        U64 mSeed = 0;
        U64 mParticleEpoch = 0;
        LLUUID mDecisionSource;
        U64 mEaseCandidateSerial = 0;
        U64 mInheritedEaseCandidateSerial = 0;
        F32 mFrameEaseWeight = 0.f;
        bool mFrameEaseApplied = false;
        U32 mWriteEpoch = 0;
        U8 mRootWrittenMask = 0;
        U8 mVerifyRootMask = 0;
        LLVOAvatar::StepAnimLabelState mLabels;
        LLVector3 mRootDelta;
        bool mDropAfterRestore = false;
        bool mPaletteOverlay = false;
        bool mRenderStateMismatchLogged = false;
    };

    ALSteppedAnim::Config readConfig() const;
    void collectActors();
    void refreshCastCache(const LLDirectorCast& cast);
    bool eligible(const LLVOAvatar* avatar) const;
    void readPose(LLVOAvatar* avatar, Entry& entry, ALSteppedAnim::PoseBuffer& out);
    void writeOverlay(LLVOAvatar* avatar, Entry& entry, const ALSteppedAnim::PoseBuffer& render);
    void restoreOverlay(LLVOAvatar* avatar, Entry& entry);
    void buildClasses(LLVOAvatar* avatar, Entry& entry);
    ALSteppedAnim::LimbFrame limbFrame(LLVOAvatar* avatar, const Entry& entry, F64 time) const;
    void updateEaseCandidate(Entry& entry, const ALSteppedAnim::LimbFrame& limbs, F64 time);
    // `groups` = part group bits (ALSteppedAnim::groupBit) whose held joints are refreshed from the
    // smooth pose; the Torso bit makes it a full capture (previous pose, effectors, ease candidate).
    void capturePose(LLVOAvatar* avatar, Entry& entry,
                     const ALSteppedAnim::LimbFrame& limbs, F64 time,
                     const Entry* decision_source, U16 groups, U16 forced);
    // (`forced` = parts whose offset just changed: their smear history is seeded with the new held
    // pose; all other smear history advances per part, only for the parts in `groups`.)
    static U32 attachPointCount(const LLVOAvatar* avatar);
    static U32 poseJointCount(const LLVOAvatar* avatar);
    static LLJoint* poseJoint(LLVOAvatar* avatar, const Entry& entry, U32 index);
    void applyEffects(LLVOAvatar* avatar, Entry& entry, ALSteppedAnim::PoseBuffer& render);
    void verifyRestored(const char* point);
    void updateRotoMatch();
    void resetEntryHistory(Entry& entry);
    void deactivate();

    // Rebuilt only when the cast / scene revision changes (not every frame): cast order and the
    // cast-group rate offsets by member id.
    struct CastCache
    {
        bool valid = false;
        U64 sceneRevision = 0;
        U64 castRevision = 0;
        std::map<LLUUID, S32> order;
        std::map<LLUUID, S32> groupOffsets;
    };
    CastCache mCastCache;
    std::unique_ptr<ALSteppedFriendCache> mFriends; // only alive while the Friends scope is in use

    ALSteppedAnim::Config mConfig;
    F64 mTime = 0.;
    std::map<LLUUID, Entry> mEntries;
    std::vector<LLUUID> mPlanned;
    S32 mActiveActors = 0;
    S32 mSkippedActors = 0;
    U64 mPaletteRebuilds = 0;
    U64 mNextParticleEpoch = 0;
    std::map<LLUUID, F32> mOmegaDebt;
    U64 mAgentRegionHandle = 0;
    LLVector3d mAgentOriginGlobal;
    bool mHaveAgentOrigin = false;
};

#endif
