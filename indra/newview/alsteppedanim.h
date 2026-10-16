/**
 * @file alsteppedanim.h
 * @brief Pure stepped-animation timing, assignment and pose helpers.
 */
#ifndef AL_STEPPED_ANIM_H
#define AL_STEPPED_ANIM_H

#include "llquaternion.h"
#include "llsd.h"
#include "stdtypes.h"
#include "v3math.h"

#include <array>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace ALSteppedAnim
{
// Values 0-2 are saved in user settings and presets; never renumber them.
enum EScope : S32 { SCOPE_SELECTED = 0, SCOPE_CAST = 1, SCOPE_VIEW = 2, SCOPE_FRIENDS = 3, SCOPE_NEARBY = 4 };
// Coarser-step offsets for group / per-actor rates: how many extra base-grid frames each hold lasts.
enum ERateOffset : S32 { RATE_SAME = 0, RATE_PLUS1 = 1, RATE_PLUS2 = 2, RATE_SMOOTH = 3 };
enum EMode : S32 { MODE_FIXED = 0, MODE_ADAPTIVE = 1, MODE_MIXED = 2 };
enum EWhat : S32 { WHAT_POSE = 0, WHAT_POSE_POSITION = 1 };
enum EClock : S32 { CLOCK_CAPTURE = 0, CLOCK_REAL = 1 };
enum EOffset : S32 { OFFSET_RANDOM = 0, OFFSET_LOCKED = 1, OFFSET_MANUAL = 2 };
// Joint classes (what a toggle holds) share one U16 with the anatomical part groups (which grid
// holds it): bits 0-5 are the class, bits 6-13 the group. A joint is held only when BOTH its class
// bit and its group bit are in the actor's held mask (jointHeld), so a Smooth part is never held.
enum EJointClass : U16 { JC_ROOT = 1, JC_BODY = 2, JC_FACE = 4, JC_GAZE = 8, JC_PHYS = 16, JC_ATTACH = 32 };
// Anatomical part groups (part rates). Saved in StepAnimPart* and in scenes: never renumber.
enum EPartGroup : S32 { PART_TORSO = 0, PART_HEAD = 1, PART_FACE = 2, PART_ARMS = 3, PART_HANDS = 4,
                        PART_LEGS = 5, PART_EXTRAS = 6, PART_ATTACH = 7 };
constexpr U32 kPartGroups = 8;
constexpr U32 kGroupShift = 6;
constexpr U16 kClassBits = 0x3F;
constexpr U16 kGroupBits = 0x3FC0;
constexpr U16 groupBit(U32 part) { return static_cast<U16>(1u << (kGroupShift + part)); }
// One ERateOffset per part group; PART_TORSO is the base grid and always RATE_SAME.
typedef std::array<S32, kPartGroups> PartOffsets;
constexpr PartOffsets kPartsSame = {{0, 0, 0, 0, 0, 0, 0, 0}};
constexpr PartOffsets kPartsInherit = {{-1, -1, -1, -1, -1, -1, -1, -1}};

constexpr F64 kTickEps = 1.e-6;
constexpr U32 kEffectors = 5;

struct Config
{
    bool enabled = false;
    S32 scope = SCOPE_CAST;
    bool includeGhosts = false;
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
    // Targeting (T2, T3) and variation (V1-V4). Defaults reproduce the pre-existing look.
    F32 nearbyRadius = 20.f;
    bool limitDistance = false;
    bool includeSelf = true;
    U32 seed = 0;
    F32 phaseSpread = 1.f;
    F32 rateVariation = 0.f;
    F32 timingJitter = 0.f;
    // Part rates (StepAnimPartHead..Attach, ERateOffset per EPartGroup; torso unused). All Same by
    // default. The Face part reads Smooth while StepAnimFace is off (one control, not two).
    PartOffsets parts = kPartsSame;
};

Config sanitize(const Config& raw);
// Setting name and scene key of a part group (PART_TORSO has no setting: it is the base rate).
const char* partSetting(U32 part);
const char* partKey(U32 part);
F32 effectiveRate(F32 rate_or_smooth);
F64 effectiveDwell(F32 min_dwell, F32 current_rate);

struct ActorOverride
{
    S32 mode = 0;
    F32 rate = 12.f;
    S32 what = -1;
    S32 role = 0;
    S32 rateOffset = -1; // -1 inherit the group/page, 0 same, 1 +1 step, 2 +2 steps
    // Per-part overrides (-1 inherit the page's part rate, else ERateOffset); torso unused.
    PartOffsets parts = kPartsInherit;
};
ActorOverride sanitize(const ActorOverride& raw);
// Scene (de)serialisation of the per-actor override, shared by the cast and the tests.
// The write returns an undefined LLSD for an all-default override (nothing is saved);
// an older scene without "rate_offset" / "parts" reads back as -1 (inherit).
LLSD writeOverride(const ActorOverride& ov);
ActorOverride readOverride(const LLSD& data);
// Effective part offsets, precedence actor > cast group > global: a per-actor part override wins;
// else a cast group with its own rate (group_offset != Same) steps the whole actor on one grid, so
// the page's part rates read Same for it (cast groups carry one rate, not per-part values); else
// the page's part settings. Face reads Smooth while StepAnimFace is off (the page's Face & lip-sync
// switch, kept for presets and scenes) unless an actor override says otherwise; torso is always Same.
PartOffsets resolveParts(const Config&, const ActorOverride&, S32 group_offset = RATE_SAME);
// The one predicate resolveRate and resolveParts share: the cast group's rate is what the actor
// steps on only while the actor has no rate offset of its own and no Own settings
// (groupRateApplies), and Match Roto Ink is off (groupRateInEffect).
bool groupRateApplies(const ActorOverride&, S32 group_offset);
bool groupRateInEffect(const Config&, const ActorOverride&, S32 group_offset);
// The ONE Face control (Part rates > Face): what it shows for the two settings it drives, and what
// it writes. Smooth is shown whenever the engine treats the face as Smooth (StepAnimFace off OR
// StepAnimPartFace == Smooth, exactly resolveParts' rule); Same / +1 / +2 show the part value.
// Writing Smooth turns StepAnimFace off (the part value is left alone); any other value turns it
// on and sets the part.
S32 faceControlShown(bool face_on, S32 part_face);
void applyFaceControl(S32 value, bool& face_on, S32& part_face);

// Cast-group rate offsets (ERateOffset, group tag -> offset). They live in the cast's group
// data and travel with the scene exactly like the group start delays: a rename carries the
// rate (renaming onto an existing group keeps the target's), a group without members sheds it.
typedef std::map<std::string, S32> GroupRates;
S32 groupRate(const GroupRates& rates, const std::string& group);   // 0 (same) when none
void setGroupRate(GroupRates& rates, const std::string& group, S32 offset); // 0 erases
void renameGroupRate(GroupRates& rates, const std::string& old_name, const std::string& new_name);
void pruneGroupRate(GroupRates& rates, const std::string& group, bool group_has_members);
// Only groups in `live_groups` are written; an empty map writes an empty LLSD map.
LLSD writeGroupRates(const GroupRates& rates, const std::vector<std::string>& live_groups);
// Only groups that still have members are kept (no stale registry). Values clamp to 0-3.
GroupRates readGroupRates(const LLSD& data, const std::vector<std::string>& live_groups);

struct ActorFacts
{
    U64 seed = 0;
    bool inCast = false;
    bool isSelf = false;
    bool isSelected = false;
    bool drawn = false;
    bool ghostOwnAnimation = false;
    bool excludedAlways = false;
    bool cameraMounted = false;
    bool posing = false;
    F32 cameraDistance = 0.f;
    bool isFriend = false;
    S32 groupOffset = 0; // cast-group rate offset (ERateOffset); the runtime resolves it from the group tag
    S32 castOrder = std::numeric_limits<S32>::max();
    ActorOverride ov;
};

struct Assignment
{
    U64 seed = 0; // mixed actor seed (digest + StepAnimSeed); drives per-actor timing jitter
    bool stepped = false;
    bool adaptive = false;
    F32 rate = 0.f;
    F32 fastRate = 0.f;
    S32 what = WHAT_POSE_POSITION;
    F64 phase = 0.;
    U16 heldClasses = 0;      // class bits + group bits (jointHeld needs one of each)
    PartOffsets parts = kPartsSame; // effective part offsets (resolveParts)
};

// One pure rate decision for the planner and the panel's Effective readout: the page rate
// (Fixed / Adaptive fast / Mixed background-or-target), the actor's own settings, and the
// group / per-actor offset. The per-actor seed variation is applied afterwards by varyRate().
struct RateChoice
{
    bool stepped = false;      // false: kept smooth (Never, Smooth group, rate below 2 fps)
    bool adaptive = false;
    F32 rate = 0.f;            // after the offset, before the seed variation
    F32 fastRate = 0.f;        // adaptive fast rate after the offset, 0 otherwise
    S32 offset = RATE_SAME;    // offset actually applied (RATE_SAME while Match Roto Ink locks it)
    F32 variation = 0.f;       // largest step-length spread; 0 when none or locked
    bool rotoLocked = false;   // Match Roto Ink on: offsets and variation would break the Roto cadence
    bool rotoSuppressed = false; // ... and something was actually switched off by it
};
// `selected` is whether the actor is the stepped target (Mixed uses it, an own Mixed role beats it).
RateChoice resolveRate(const Config&, const ActorOverride&, bool selected, S32 group_offset);
Assignment resolveActor(const Config&, const ActorFacts&, F64 roto_phase);
// Scope + include-self + distance rules (T1-T3). Does not apply the per-actor overrides,
// exclusions or the actor cap, which resolveActor / applyActorCap apply afterwards (T4).
bool scopeIncludes(const Config&, const ActorFacts&);
// actor_seed = hash(actor UUID digest, StepAnimSeed); seed 0 returns the digest unchanged.
U64 mixSeed(U64 digest, U32 user_seed);
// Deterministic per-actor step-length factor in [1, 1 + variation] (unquantised).
F32 rateFactor(U64 seed, F32 variation);
// Multiplies the actor's own step length by the factor, quantised on the actor's own grid:
// an actor that steps in whole base frames (12 fps on a 24 base = 2 frames) gets a whole number
// of base frames (2-4, so threes and fours); any other rate (10 or 30 fps on 24) lengthens by
// a whole number of its own steps (x1 or x2). Never faster than `rate`, never below 2 fps and
// never snapped to the base rate.
F32 varyRate(F32 rate, U64 seed, F32 variation, S32 base_fps);
// varyRate with the factor already known (varyRate = this at rateFactor(seed, variation)). The
// factor is in [1, 1 + variation) and the quantised result is monotone in it, so the reachable
// step rates lie between the values at factor 1 and at the largest factor a seed can give.
F32 varyRateAtFactor(F32 rate, F32 factor, S32 base_fps);
// Group / per-actor coarser step: each hold lasts `offset` more base-grid frames (RATE_PLUS1/2 only).
F32 offsetRate(F32 rate, S32 offset, S32 base_fps);
// Imperfect timing: true when this tick should be held one extra step. Never twice in a row.
bool extraHold(U64 seed, S64 tick, F32 jitter, bool previous_was_extra);
bool ghostMayInherit(const Config&, const ActorFacts&);
// A worn animesh follows its wearer on the Attachments part rate: Same copies the wearer's grid
// (identical to the pre-part-rate behaviour), +1 / +2 put it on the wearer's Attachments grid
// (offset rate, partPhase, the wearer's seed; its frames come from the wearer's decision via
// attachedDecision, so it ticks exactly with the wearer's rigid attachments and takes the wearer's
// Adaptive decision), Smooth leaves it unstepped (returns false, `attached` cleared). The animesh
// is one grid: its own parts are all Same and its attachment points are never written.
bool inheritAttachedAssignment(const Assignment& source, U64 source_seed,
                               Assignment& attached, U64& attached_seed,
                               S32 attach_offset = RATE_SAME, S32 base_fps = 24);
void applyActorCap(const Config&, const std::vector<ActorFacts>&, std::vector<Assignment>&);
// Held mask: class bits from the toggles / What steps plus the group bit of every part that is not
// Smooth. Attachment points (JC_ATTACH / PART_ATTACH) are written only when the Attachments part
// is not Same: coarser holds them on their own grid, Smooth re-places them from the smooth pose.
U16 heldClasses(const Config&, S32 what, const PartOffsets& parts);
U16 heldClasses(const Config&, S32 what); // with the page's part rates and no actor override
constexpr bool jointHeld(U16 joint_class, U16 mask)
{
    return (joint_class & mask & kClassBits) != 0 && (joint_class & mask & kGroupBits) != 0;
}
U32 partOf(U16 joint_class); // the joint's EPartGroup (PART_TORSO when no group bit is set)
F64 actorPhase(const Config&, U64 seed, F64 roto_phase);

S64 tickIndex(F64 t, F64 rate, F64 phase);
F64 nextBoundary(F64 t, F64 rate, F64 phase);
// Phase of a coarser part grid: the actor's time offset (phase / rate) expressed on the part's
// rate, so the part's boundaries lie on the actor's base-frame lattice (and on torso ticks when
// the rates coincide). planFrame uses it for the +1 / +2 part grids and their boil indices.
F64 partPhase(F64 phase, F64 rate, F64 part_rate);

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
    bool jitterHeld = false; // the previous boundary was an extra (imperfect-timing) hold
    // Per-part grids (parts on +1 / +2): the last tick and rate seen, so a part captures only on
    // its own boundaries and re-grids without a capture when its rate changes (like the torso).
    std::array<S64, kPartGroups> partTick{};
    std::array<F64, kPartGroups> partRate{};
    // Last effective offset per part (-1 = none yet): a part whose offset changes (page, cast group
    // or actor) is captured at once, so it never shows a stale or empty hold.
    std::array<S32, kPartGroups> partOffset{{-1, -1, -1, -1, -1, -1, -1, -1}};
};

struct FrameDecision
{
    bool capture = false;   // the torso (base) grid captured this frame
    bool hold = false;
    S64 tick = 0;
    S64 boilIndex = 0;
    F32 rate = 0.f;
    // Group bits (groupBit) of the parts whose held transforms are refreshed from the smooth pose
    // this frame: Same parts on `capture`, coarser parts on their own ticks, Smooth attachments
    // every frame, any part whose offset just changed. With all parts Same this is every group bit
    // when `capture`, else 0.
    U16 captureGroups = 0;
    U16 forceGroups = 0; // subset of captureGroups: parts captured because their offset changed
    std::array<S64, kPartGroups> partBoil{}; // boil index per part (its own grid / boilTicks)
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

// The worn animesh's frame from its wearer's: Attachments Same copies the wearer's decision; +1 / +2
// take the wearer's Attachments part grid (capture on the wearer's `captureGroups` Attachments bit,
// that part's tick and boil index, the offset rate), so the animesh never plans its own frame and
// shares the wearer's Adaptive fast / slow choice and impact holds. A forced capture of the
// wearer's Attachments part (its offset changed, in either direction, Same included) forces a
// capture of EVERY held group of the animesh (it is one grid): no stale pose until a torso tick.
FrameDecision attachedDecision(const Assignment& attached, const ActorState& wearer_state,
                               const FrameDecision& wearer_decision, S32 attach_offset, S32 base_fps);
// Copies the wearer's assignment (inheritAttachedAssignment), state and attachedDecision.
void inheritAttachedFrame(const Assignment& source_assignment, U64 source_seed,
                          const ActorState& source_state, const FrameDecision& source_decision,
                          Assignment& attached_assignment, U64& attached_seed,
                          ActorState& attached_state, FrameDecision& attached_decision,
                          S32 attach_offset = RATE_SAME, S32 base_fps = 24);

FrameDecision planFrame(ActorState&, const Assignment&, const Config&, F64 t);
// Group bits whose held transforms an actor refreshes from the smooth pose this frame, from ITS
// assignment's parts: Same parts on `capture` (or when forced by an offset change), coarser parts
// from `captureGroups`, Smooth attachments every frame (Smooth body parts never, they are not
// held). Equals planFrame's captureGroups for an actor that planned its own frame.
U16 refreshGroups(const Assignment&, const FrameDecision&);
// Smear for one limb chain whose root joint has `chain_class`: 0 when the chain does not smear
// this frame (not held: Smooth part or class off; no smear tick for its grid), else the chain's
// tick length. A Same part smears on the torso's smear tick at the torso rate; a +1 / +2 part on
// its own capture flag at its own rate, so a coarser limb never moves on a torso tick.
F32 smearChainDt(const Assignment&, U16 chain_class, bool torso_smear_tick, bool part_smear_tick,
                 F32 decision_rate, S32 base_fps);
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
    bool attachment = false; // a (non-HUD) attachment point joint: class JC_ATTACH, group PART_ATTACH
};

// Class (JC_*) and part group (groupBit) per joint. Groups by bone name (Torso: pelvis, spine,
// torso, chest, collars; Head: neck, head and the eyes, i.e. every Gaze joint, so the Gaze toggle
// keeps holding the eyes whatever the Face part says; Face: the rest of the mFaceRoot subtree;
// Arms: shoulders, elbows, wrists; Hands: mHand*; Legs: hips, knees, ankles, feet, toes; Extras:
// Bento tail, wings, hind limbs, groin); any other bone and every collision volume follow their
// parent; the root and unknown parentless joints are Torso; attachment points are Attachments.
std::vector<U16> classifyJoints(const std::vector<JointDesc>&);
void composeRender(const PoseBuffer&, const PoseBuffer&, const std::vector<U16>&, U16, PoseBuffer&);
JointLocal restoreValue(const JointLocal&, const JointLocal&, const JointLocal&, bool epoch_changed);
bool bitEqual(const JointLocal&, const JointLocal&);

// Attachment holds. A rigid attachment is drawn from its attachment point's world matrix, so to
// step accessories coarser than the bone they hang on, the point's ROOT-RELATIVE placement is
// held and the local transform that reproduces it under the parent's render pose is written each
// frame (children offsets scale by the parent's scale, as LLJoint does). `parents[i] < 0` is the
// avatar root. Only joints with JC_ATTACH that pass jointHeld(mask) are touched.
JointLocal relativePlacement(const PoseBuffer& pose, const std::vector<S32>& parents, S32 index);
JointLocal localForPlacement(const PoseBuffer& pose, const std::vector<S32>& parents, S32 index,
                             const JointLocal& relative);
void captureAttachmentHolds(const PoseBuffer& smooth, const std::vector<U16>& classes,
                            const std::vector<S32>& parents, U16 mask, std::vector<JointLocal>& held);
// Smear history per part: the previous held pose of the joints in `groups` (and the root when
// `root`) becomes the current held pose. Called BEFORE a part's refresh with the parts captured
// now, so a +1 / +2 part's history advances only on its own ticks; called again AFTER the refresh
// with the forced parts (offset just changed) so their previous pose equals the new held pose and
// nothing smears toward a stale hold. Resizes `previous` only when the skeleton changed.
void advancePartHistory(const PoseBuffer& held, const std::vector<U16>& classes, U16 groups, bool root,
                        PoseBuffer& previous);
void applyAttachmentHolds(const std::vector<U16>& classes, const std::vector<S32>& parents, U16 mask,
                          const std::vector<JointLocal>& held, PoseBuffer& render);

struct RotoMatch { F32 sketchFps = 12.f; S32 motionStep = 0; bool setMotionStep = false; };
RotoMatch mapRotoInk(F32 rate);

struct Preset { const char* id; const char* label; Config values; };
const std::vector<Preset>& builtInPresets();
const std::vector<std::string>& trackedSettings();
const std::vector<std::string>& presetSettings();
}

#endif
