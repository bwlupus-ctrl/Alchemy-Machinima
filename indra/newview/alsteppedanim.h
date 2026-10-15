/**
 * @file alsteppedanim.h
 * @brief Pure stepped-animation timing, assignment and pose helpers.
 */
#ifndef AL_STEPPED_ANIM_H
#define AL_STEPPED_ANIM_H

#include "llquaternion.h"
#include "stdtypes.h"
#include "v3math.h"

#include <array>
#include <limits>
#include <string>
#include <vector>

namespace ALSteppedAnim
{
enum EScope : S32 { SCOPE_SELECTED = 0, SCOPE_CAST = 1, SCOPE_VIEW = 2 };
enum EMode : S32 { MODE_FIXED = 0, MODE_ADAPTIVE = 1, MODE_MIXED = 2 };
enum EWhat : S32 { WHAT_POSE = 0, WHAT_POSE_POSITION = 1 };
enum EClock : S32 { CLOCK_CAPTURE = 0, CLOCK_REAL = 1 };
enum EOffset : S32 { OFFSET_RANDOM = 0, OFFSET_LOCKED = 1, OFFSET_MANUAL = 2 };
enum EJointClass : U8 { JC_ROOT = 1, JC_BODY = 2, JC_FACE = 4, JC_GAZE = 8, JC_PHYS = 16 };

constexpr F64 kTickEps = 1.e-6;
constexpr U32 kEffectors = 5;

struct Config
{
    bool enabled = false;
    S32 scope = SCOPE_CAST;
    bool excludeCameraMounted = true;
    S32 maxAvatars = 24;
    S32 mode = MODE_FIXED;
    F32 rate = 12.f;
    F32 fastRate = 24.f;
    F32 adaptThreshold = 1.5f;
    F32 adaptHysteresis = .3f;
    F32 adaptMinDwell = .25f;
    F32 backgroundRate = 8.f;
    S32 what = WHAT_POSE_POSITION;
    bool face = true;
    bool gaze = true;
    bool flexi = true;
    bool attachFX = false;
    bool particles = false;
    bool textureAnim = false;
    S32 clock = CLOCK_CAPTURE;
    S32 offset = OFFSET_RANDOM;
    F32 phase = 0.f;
    S32 baseFps = 24;
    bool boil = false;
    F32 boilAmount = .6f;
    S32 boilTicks = 2;
    bool impact = false;
    S32 impactTicks = 3;
    F32 impactThreshold = 25.f;
    bool smear = false;
    F32 smearStrength = .5f;
    F32 smearThreshold = 2.5f;
    bool ease = false;
    F32 easeAmount = .5f;
    bool matchRoto = false;
};

Config sanitize(const Config& raw);
F32 effectiveRate(F32 rate_or_smooth);
F64 effectiveDwell(F32 min_dwell, F32 current_rate);

struct ActorOverride
{
    S32 mode = 0;
    F32 rate = 12.f;
    S32 what = -1;
    S32 role = 0;
};
ActorOverride sanitize(const ActorOverride& raw);

struct ActorFacts
{
    U64 seed = 0;
    bool inCast = false;
    bool isSelf = false;
    bool isSelected = false;
    bool drawn = false;
    bool excludedAlways = false;
    bool cameraMounted = false;
    bool posing = false;
    F32 cameraDistance = 0.f;
    S32 castOrder = std::numeric_limits<S32>::max();
    ActorOverride ov;
};

struct Assignment
{
    bool stepped = false;
    bool adaptive = false;
    F32 rate = 0.f;
    F32 fastRate = 0.f;
    S32 what = WHAT_POSE_POSITION;
    F64 phase = 0.;
    U8 heldClasses = 0;
};

Assignment resolveActor(const Config&, const ActorFacts&, F64 roto_phase);
bool ghostMayInherit(const Config&, const ActorFacts&);
bool inheritAttachedAssignment(const Assignment& source, U64 source_seed,
                               Assignment& attached, U64& attached_seed);
void applyActorCap(const Config&, const std::vector<ActorFacts>&, std::vector<Assignment>&);
U8 heldClasses(const Config&, S32 what);
F64 actorPhase(const Config&, U64 seed, F64 roto_phase);

S64 tickIndex(F64 t, F64 rate, F64 phase);
F64 nextBoundary(F64 t, F64 rate, F64 phase);

struct LimbFrame
{
    F64 t = 0.;
    LLVector3 p[kEffectors];
};

struct LimbTracker
{
    void push(const LimbFrame&);
    F32 speed() const { return mSmoothedSpeed; }
    F32 decel(U32 e) const;
    F32 prevSpeed(U32 e) const;
    void reset();

    std::array<LimbFrame, 16> mFrames{};
    U32 mStart = 0;
    U32 mCount = 0;
    F32 mSmoothedSpeed = 0.f;
    std::array<F32, kEffectors> mPreviousVelocity{};
    std::array<F32, kEffectors> mCurrentVelocity{};
    F64 mVelocityDt = 0.;
};

struct ActorState
{
    static constexpr U32 kEaseSubsteps = 4;
    LimbTracker limbs;
    bool hasCapture = false;
    F64 captureRate = 0.;
    S64 captureTick = 0;
    bool fast = false;
    F64 stateSince = 0.;
    bool forceCapture = false;
    S32 impactHoldLeft = 0;
    bool impactCooldown = false;
    bool impactPending = false;
    F32 subSpeed[kEaseSubsteps] = {};
    bool subSeen[kEaseSubsteps] = {};
    S32 candidateSub = -1;
    bool candidatePending = false;
    bool smearTick = false;
};

struct FrameDecision
{
    bool capture = false;
    bool hold = false;
    S64 tick = 0;
    S64 boilIndex = 0;
    F32 rate = 0.f;
};

// Epoch changes discard deferred time on disable/release without walking particles.
struct ParticleStepDebt
{
    F32 seconds = 0.f;
    U64 epoch = 0;
    U64 actorEpoch = 0;
    U64 frame = 0;
};
bool migrateParticleSetting(bool attach_fx, bool& particles, bool& migrated);
bool particleSourceEligible(bool is_avatar, bool is_attachment, bool is_hud);
bool claimParticleFrame(ParticleStepDebt& debt, U64 frame);
bool particleAgeExpired(F32 age, F32 max_age);
struct ParticleSubsteps
{
    U32 count = 1;
    F32 dt = 0.f;
    F32 ballisticDt = 0.f;
};
ParticleSubsteps particleSubsteps(F32 dt, bool bounce = false);
F32 particleGroupDt(F32 frame_dt, F32 skipped_time, F32& skip_offset, bool step_only);
F32 particleStepDt(F32 dt, ParticleStepDebt& debt, U64 epoch, const FrameDecision* decision,
                   U64 actor_epoch = 0);

void inheritAttachedFrame(const Assignment& source_assignment, U64 source_seed,
                          const ActorState& source_state, const FrameDecision& source_decision,
                          Assignment& attached_assignment, U64& attached_seed,
                          ActorState& attached_state, FrameDecision& attached_decision);

FrameDecision planFrame(ActorState&, const Assignment&, const Config&, F64 t);
void observe(ActorState&, const Config&, const LimbFrame&);

LLQuaternion boilRotation(U64 seed, S64 boil_index, U32 joint, F32 amplitude_deg);
struct SmearChain { F32 lagT = 1.f; F32 stretch = 1.f; bool active = false; };
SmearChain smearFor(F32 displacement, F32 tick_dt, F32 chain_len, F32 min_bone_len,
                    U32 n_joints, F32 strength, F32 threshold);
LLVector3 smearAxisScale(F32 k, const LLVector3& dir_local);
F32 easeWeight(F32 amount, F32 s_min, F32 s_cap);
F32 speedAt(const LimbTracker&, F64 t_sub);
bool smearHistoryReady(bool previous_valid, size_t previous_size, size_t current_size);
F32 unitHash(U64 seed, S64 a, U32 b, U32 c);

U32 overlayRenderFrame(U32 current_frame);
bool paletteNeedsOverlayRebuild(U32 cache_frame, U32 current_frame);
bool preservePreviousOverlayPalette(U32 last_frame, U32 overlay_frame, U32 render_frame);
bool paletteContainsOverlay(U32 cache_frame, U32 overlay_frame, U32 current_frame);
bool paletteReadyForOverlay(U32 cache_frame, U32 overlay_frame, U32 render_frame,
                            U32 cache_pose_epoch, U32 pose_epoch);
bool hasOverlayWrites(const std::vector<U8>& written_mask, U8 root_mask);

struct JointLocal
{
    LLQuaternion rot;
    LLVector3 pos;
    LLVector3 scale = LLVector3(1.f, 1.f, 1.f);
};
struct PoseBuffer
{
    std::vector<JointLocal> joints;
    LLVector3 rootPos;
    LLQuaternion rootRot;
};
struct JointDesc
{
    std::string name;
    S32 parent = -1;
    bool collisionVolume = false;
};

std::vector<U8> classifyJoints(const std::vector<JointDesc>&);
void composeRender(const PoseBuffer&, const PoseBuffer&, const std::vector<U8>&, U8, PoseBuffer&);
JointLocal restoreValue(const JointLocal&, const JointLocal&, const JointLocal&, bool epoch_changed);
bool bitEqual(const JointLocal&, const JointLocal&);

struct RotoMatch { F32 sketchFps = 12.f; S32 motionStep = 0; bool setMotionStep = false; };
RotoMatch mapRotoInk(F32 rate);

struct Preset { const char* id; const char* label; Config values; };
const std::vector<Preset>& builtInPresets();
const std::vector<std::string>& trackedSettings();
const std::vector<std::string>& presetSettings();
}

#endif
