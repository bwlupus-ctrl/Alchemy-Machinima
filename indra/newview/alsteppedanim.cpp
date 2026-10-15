/**
 * @file alsteppedanim.cpp
 * @brief Pure stepped-animation timing, assignment and pose helpers.
 */
#include "llviewerprecompiledheaders.h"

#include "alsteppedanim.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>

namespace
{
template <typename T> T finiteOr(T value, T fallback)
{
    return std::isfinite(value) ? value : fallback;
}

F64 fraction(F64 value)
{
    value -= std::floor(value);
    return value < 0. ? value + 1. : value;
}

bool vectorBitsEqual(const LLVector3& a, const LLVector3& b)
{
    return std::memcmp(a.mV, b.mV, sizeof(a.mV)) == 0;
}

bool quaternionBitsEqual(const LLQuaternion& a, const LLQuaternion& b)
{
    return std::memcmp(a.mQ, b.mQ, sizeof(a.mQ)) == 0;
}

U64 splitmix64(U64 value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

F32 frameVelocity(const ALSteppedAnim::LimbFrame& a,
                  const ALSteppedAnim::LimbFrame& b, U32 effector)
{
    const F64 dt = b.t - a.t;
    return dt > 0. ? (b.p[effector] - a.p[effector]).length() / static_cast<F32>(dt) : 0.f;
}
}

namespace ALSteppedAnim
{
F32 effectiveRate(F32 rate)
{
    rate = finiteOr(rate, 0.f);
    return rate < 2.f ? 0.f : llclamp(rate, 2.f, 30.f);
}

F64 effectiveDwell(F32 min_dwell, F32 current_rate)
{
    const F64 configured = llclamp(static_cast<F64>(finiteOr(min_dwell, .25f)), .05, 2.);
    const F64 rate = effectiveRate(current_rate);
    return rate > 0. ? llmax(configured, 1. / rate) : llmax(configured, .05);
}

Config sanitize(const Config& raw)
{
    Config out = raw;
    out.scope = llclamp(raw.scope, 0, 2);
    out.maxAvatars = llclamp(raw.maxAvatars, 1, 100);
    out.mode = llclamp(raw.mode, 0, 2);
    out.rate = llclamp(finiteOr(raw.rate, 12.f), 2.f, 30.f);
    out.fastRate = effectiveRate(finiteOr(raw.fastRate, 24.f));
    out.adaptThreshold = llclamp(finiteOr(raw.adaptThreshold, 1.5f), .1f, 10.f);
    out.adaptHysteresis = llclamp(finiteOr(raw.adaptHysteresis, .3f), .05f, .9f);
    out.adaptMinDwell = llclamp(finiteOr(raw.adaptMinDwell, .25f), .05f, 2.f);
    out.backgroundRate = effectiveRate(finiteOr(raw.backgroundRate, 8.f));
    out.what = llclamp(raw.what, 0, 1);
    out.clock = llclamp(raw.clock, 0, 1);
    out.offset = llclamp(raw.offset, 0, 2);
    out.phase = llclamp(finiteOr(raw.phase, 0.f), 0.f, 1.f);
    if (out.phase == 1.f) out.phase = 0.f;
    out.baseFps = raw.baseFps < 25 ? 24 : (raw.baseFps < 28 ? 25 : 30);
    out.boilAmount = llclamp(finiteOr(raw.boilAmount, .6f), 0.f, 3.f);
    out.boilTicks = llclamp(raw.boilTicks, 1, 8);
    out.impactTicks = llclamp(raw.impactTicks, 1, 12);
    out.impactThreshold = llclamp(finiteOr(raw.impactThreshold, 25.f), 5.f, 200.f);
    out.smearStrength = llclamp(finiteOr(raw.smearStrength, .5f), 0.f, 1.f);
    out.smearThreshold = llclamp(finiteOr(raw.smearThreshold, 2.5f), .5f, 10.f);
    out.easeAmount = llclamp(finiteOr(raw.easeAmount, .5f), 0.f, 1.f);
    return out;
}

ActorOverride sanitize(const ActorOverride& raw)
{
    ActorOverride out = raw;
    out.mode = llclamp(raw.mode, 0, 2);
    out.rate = llclamp(finiteOr(raw.rate, 12.f), 2.f, 30.f);
    out.what = llclamp(raw.what, -1, 1);
    out.role = llclamp(raw.role, 0, 2);
    return out;
}

F32 unitHash(U64 seed, S64 a, U32 b, U32 c)
{
    U64 value = seed;
    value ^= splitmix64(static_cast<U64>(a));
    value ^= splitmix64((static_cast<U64>(b) << 32) | c);
    const U32 mantissa = static_cast<U32>(splitmix64(value) >> 40);
    return static_cast<F32>(mantissa) * (1.f / 16777216.f);
}

F64 actorPhase(const Config& cfg, U64 seed, F64 roto_phase)
{
    F64 phase = 0.;
    if (cfg.offset == OFFSET_RANDOM)
        phase = unitHash(seed, 0x7068617365LL, 0, 0);
    else if (cfg.offset == OFFSET_MANUAL)
        phase = cfg.phase;
    return fraction(phase + roto_phase);
}

U8 heldClasses(const Config& cfg, S32 what)
{
    U8 mask = JC_BODY;
    if (what == WHAT_POSE_POSITION) mask |= JC_ROOT;
    if (cfg.face) mask |= JC_FACE;
    if (cfg.gaze) mask |= JC_GAZE;
    if (cfg.flexi) mask |= JC_PHYS;
    return mask;
}

Assignment resolveActor(const Config& raw, const ActorFacts& facts, F64 roto_phase)
{
    const Config cfg = sanitize(raw);
    Assignment out;
    if (!cfg.enabled || !facts.drawn || facts.excludedAlways || facts.posing ||
        (cfg.excludeCameraMounted && facts.cameraMounted))
        return out;

    const ActorOverride ov = sanitize(facts.ov);
    if (ov.mode == 1) return out;
    bool in_scope = cfg.scope == SCOPE_SELECTED ? facts.isSelected :
                    cfg.scope == SCOPE_CAST ? facts.inCast : facts.drawn;
    bool target = facts.isSelected;
    if (ov.role == 1) target = true;
    else if (ov.role == 2) target = false;

    F32 rate = cfg.rate;
    bool adaptive = cfg.mode == MODE_ADAPTIVE;
    if (cfg.mode == MODE_MIXED)
    {
        if (cfg.scope != SCOPE_VIEW) in_scope = facts.inCast || facts.isSelected;
        rate = target ? cfg.rate : cfg.backgroundRate;
        adaptive = false;
    }
    if (ov.mode == 2)
    {
        in_scope = true;
        rate = ov.rate;
        adaptive = false;
    }
    rate = effectiveRate(rate);
    if (!in_scope || rate == 0.f) return out;

    out.stepped = true;
    out.adaptive = adaptive;
    out.rate = rate;
    out.fastRate = adaptive ? cfg.fastRate : 0.f;
    out.what = ov.mode == 2 && ov.what >= 0 ? ov.what : cfg.what;
    out.phase = actorPhase(cfg, facts.seed, cfg.matchRoto ? roto_phase : 0.);
    out.heldClasses = heldClasses(cfg, out.what);
    return out;
}

bool ghostMayInherit(const Config& raw, const ActorFacts& facts)
{
    const Config cfg = sanitize(raw);
    const ActorOverride ov = sanitize(facts.ov);
    return cfg.enabled && facts.drawn && !facts.excludedAlways && !facts.posing &&
        (!cfg.excludeCameraMounted || !facts.cameraMounted) && ov.mode != 1;
}

bool inheritAttachedAssignment(const Assignment& source, U64 source_seed,
                               Assignment& attached, U64& attached_seed)
{
    if (!source.stepped) return false;
    attached = source;
    // The control avatar follows its worn volume transform. Holding its own
    // root as well double-applies the wearer's held attachment transform.
    attached.heldClasses &= ~JC_ROOT;
    attached_seed = source_seed;
    return true;
}

bool migrateParticleSetting(bool attach_fx, bool& particles, bool& migrated)
{
    if (migrated) return false;
    if (attach_fx) particles = true;
    migrated = true;
    return true;
}

bool particleSourceEligible(bool is_avatar, bool is_attachment, bool is_hud)
{
    return !is_hud && (is_avatar || is_attachment);
}

bool claimParticleFrame(ParticleStepDebt& debt, U64 frame)
{
    if (debt.frame == frame) return false;
    debt.frame = frame;
    return true;
}

bool particleAgeExpired(F32 age, F32 max_age)
{
    return age > max_age;
}

ParticleSubsteps particleSubsteps(F32 dt, bool bounce)
{
    constexpr F32 max_dt = 1.f / 30.f;
    // Bounce particles get a wider budget so typical impact holds are fully
    // integrated (the bounce plane is tested per substep) instead of tailed.
    const U32 max_steps = bounce ? 64 : 16;
    const F32 integrated = llmin(dt, max_steps * max_dt);
    ParticleSubsteps out;
    out.count = llclamp(static_cast<U32>(std::ceil(integrated / max_dt)), U32(1), max_steps);
    out.dt = integrated / out.count;
    // Bound nonlinear work even across long impact holds. The remaining time
    // still advances ballistic motion and age exactly, without a giant force step.
    out.ballisticDt = dt - integrated;
    return out;
}

F32 particleGroupDt(F32 frame_dt, F32 skipped_time, F32& skip_offset, bool step_only)
{
    const F32 dt = frame_dt + skipped_time - skip_offset;
    skip_offset = step_only ? skipped_time + frame_dt : 0.f;
    return dt;
}

F32 particleStepDt(F32 dt, ParticleStepDebt& debt, U64 epoch, const FrameDecision* decision,
                   U64 actor_epoch)
{
    if (debt.epoch != epoch || debt.actorEpoch != actor_epoch || !decision || !decision->hold)
    {
        debt.seconds = 0.f;
        debt.epoch = epoch;
        debt.actorEpoch = actor_epoch;
    }
    if (decision && decision->hold && !decision->capture)
    {
        debt.seconds += dt;
        return 0.f;
    }
    const F32 result = dt + debt.seconds;
    debt.seconds = 0.f;
    return result;
}

void inheritAttachedFrame(const Assignment& source_assignment, U64 source_seed,
                          const ActorState& source_state, const FrameDecision& source_decision,
                          Assignment& attached_assignment, U64& attached_seed,
                          ActorState& attached_state, FrameDecision& attached_decision)
{
    if (!inheritAttachedAssignment(source_assignment, source_seed,
                                   attached_assignment, attached_seed))
        return;
    attached_state = source_state;
    attached_decision = source_decision;
}

void applyActorCap(const Config& raw, const std::vector<ActorFacts>& facts,
                   std::vector<Assignment>& assignments)
{
    const S32 cap = sanitize(raw).maxAvatars;
    std::vector<U32> active;
    for (U32 i = 0; i < assignments.size() && i < facts.size(); ++i)
        if (assignments[i].stepped) active.push_back(i);
    std::stable_sort(active.begin(), active.end(), [&facts](U32 a, U32 b)
    {
        auto priority = [&facts](U32 i)
        {
            if (facts[i].isSelected) return 0;
            if (facts[i].ov.mode == 2) return 1;
            if (facts[i].inCast || facts[i].isSelf) return 2;
            return 3;
        };
        const S32 pa = priority(a), pb = priority(b);
        if (pa != pb) return pa < pb;
        if (pa == 2 && facts[a].castOrder != facts[b].castOrder)
            return facts[a].castOrder < facts[b].castOrder;
        return pa == 3 && facts[a].cameraDistance != facts[b].cameraDistance
            ? facts[a].cameraDistance < facts[b].cameraDistance : a < b;
    });
    for (U32 i = static_cast<U32>(cap); i < active.size(); ++i)
        assignments[active[i]] = Assignment();
}

S64 tickIndex(F64 t, F64 rate, F64 phase)
{
    if (!(rate > 0.) || !std::isfinite(t) || !std::isfinite(rate) || !std::isfinite(phase)) return 0;
    return static_cast<S64>(std::floor(t * rate + fraction(phase) + kTickEps));
}

F64 nextBoundary(F64 t, F64 rate, F64 phase)
{
    if (!(rate > 0.) || !std::isfinite(t) || !std::isfinite(rate))
        return std::numeric_limits<F64>::infinity();
    return (static_cast<F64>(tickIndex(t, rate, phase) + 1) - fraction(phase)) / rate;
}

void LimbTracker::reset()
{
    mStart = mCount = 0;
    mSmoothedSpeed = 0.f;
    mPreviousVelocity.fill(0.f);
    mCurrentVelocity.fill(0.f);
    mVelocityDt = 0.;
}

void LimbTracker::push(const LimbFrame& frame)
{
    if (!std::isfinite(frame.t)) return;
    const U32 write = (mStart + mCount) % mFrames.size();
    if (mCount == mFrames.size())
    {
        mStart = (mStart + 1) % mFrames.size();
        mFrames[(mStart + mCount - 1) % mFrames.size()] = frame;
    }
    else
    {
        mFrames[write] = frame;
        ++mCount;
    }
    if (mCount < 2) return;
    const LimbFrame& latest = mFrames[(mStart + mCount - 1) % mFrames.size()];
    U32 old_index = mCount - 2;
    while (old_index > 0 && latest.t - mFrames[(mStart + old_index) % mFrames.size()].t < .033)
        --old_index;
    const LimbFrame& old = mFrames[(mStart + old_index) % mFrames.size()];
    mVelocityDt = latest.t - old.t;
    if (!(mVelocityDt > 0.)) return;
    mPreviousVelocity = mCurrentVelocity;
    F32 maximum = 0.f;
    for (U32 e = 0; e < kEffectors; ++e)
    {
        mCurrentVelocity[e] = frameVelocity(old, latest, e);
        maximum = llmax(maximum, mCurrentVelocity[e]);
    }
    const F32 alpha = 1.f - std::exp(-static_cast<F32>(mVelocityDt) / .05f);
    mSmoothedSpeed += alpha * (maximum - mSmoothedSpeed);
}

F32 LimbTracker::decel(U32 e) const
{
    return e < kEffectors && mVelocityDt > 0.
        ? llmax(0.f, (mPreviousVelocity[e] - mCurrentVelocity[e]) / static_cast<F32>(mVelocityDt)) : 0.f;
}

F32 LimbTracker::prevSpeed(U32 e) const
{
    return e < kEffectors ? mPreviousVelocity[e] : 0.f;
}

F32 speedAt(const LimbTracker& tracker, F64 t_sub)
{
    if (tracker.mCount < 2) return 0.f;
    auto segmentSpeed = [&tracker](U32 right)
    {
        const LimbFrame& a = tracker.mFrames[(tracker.mStart + right - 1) % tracker.mFrames.size()];
        const LimbFrame& b = tracker.mFrames[(tracker.mStart + right) % tracker.mFrames.size()];
        F32 maximum = 0.f;
        for (U32 e = 0; e < kEffectors; ++e) maximum = llmax(maximum, frameVelocity(a, b, e));
        return maximum;
    };
    for (U32 i = 1; i < tracker.mCount; ++i)
    {
        const LimbFrame& a = tracker.mFrames[(tracker.mStart + i - 1) % tracker.mFrames.size()];
        const LimbFrame& b = tracker.mFrames[(tracker.mStart + i) % tracker.mFrames.size()];
        if (t_sub <= b.t)
        {
            const F32 right_speed = segmentSpeed(i);
            const F32 left_speed = i > 1 ? segmentSpeed(i - 1) : right_speed;
            const F64 dt = b.t - a.t;
            const F32 alpha = dt > 0. ? static_cast<F32>(llclamp((t_sub - a.t) / dt, 0., 1.)) : 1.f;
            return lerp(left_speed, right_speed, alpha);
        }
    }
    return segmentSpeed(tracker.mCount - 1);
}

bool smearHistoryReady(bool previous_valid, size_t previous_size, size_t current_size)
{
    return previous_valid && previous_size == current_size;
}

U32 overlayRenderFrame(U32 current_frame)
{
    return current_frame + 1;
}

bool paletteNeedsOverlayRebuild(U32 cache_frame, U32 current_frame)
{
    return cache_frame == overlayRenderFrame(current_frame);
}

bool preservePreviousOverlayPalette(U32 last_frame, U32 overlay_frame, U32 render_frame)
{
    return overlay_frame == render_frame && last_frame == render_frame - 1;
}

bool paletteContainsOverlay(U32 cache_frame, U32 overlay_frame, U32 current_frame)
{
    return cache_frame == current_frame && overlay_frame == current_frame;
}

bool paletteReadyForOverlay(U32 cache_frame, U32 overlay_frame, U32 render_frame,
                            U32 cache_pose_epoch, U32 pose_epoch)
{
    return cache_frame != render_frame ||
        (overlay_frame == render_frame && cache_pose_epoch == pose_epoch);
}

bool hasOverlayWrites(const std::vector<U8>& written_mask, U8 root_mask)
{
    return root_mask != 0 || std::any_of(written_mask.begin(), written_mask.end(),
        [](U8 mask) { return mask != 0; });
}

void observe(ActorState& state, const Config& raw, const LimbFrame& frame)
{
    const Config cfg = sanitize(raw);
    state.limbs.push(frame);
    if (cfg.mode == MODE_ADAPTIVE)
    {
        const F32 current_rate = state.fast ? cfg.fastRate : cfg.rate;
        if (frame.t - state.stateSince >= effectiveDwell(cfg.adaptMinDwell, current_rate))
        {
            const bool switch_fast = !state.fast && state.limbs.speed() > cfg.adaptThreshold;
            const bool switch_slow = state.fast && state.limbs.speed() < cfg.adaptThreshold * (1.f - cfg.adaptHysteresis);
            if (switch_fast || switch_slow)
            {
                state.fast = switch_fast;
                state.stateSince = frame.t;
                if (switch_fast) state.forceCapture = true;
            }
        }
    }
    if (cfg.impact && !state.impactPending && state.impactHoldLeft == 0 && !state.impactCooldown)
    {
        for (U32 e = 0; e < kEffectors; ++e)
            if (state.limbs.prevSpeed(e) >= 1.f && state.limbs.decel(e) >= cfg.impactThreshold)
            {
                state.impactPending = true;
                break;
            }
    }
}

FrameDecision planFrame(ActorState& state, const Assignment& assignment,
                        const Config& raw, F64 t)
{
    const Config cfg = sanitize(raw);
    FrameDecision out;
    if (!assignment.stepped) return out;
    out.hold = true;
    out.rate = assignment.adaptive && state.fast ? assignment.fastRate : assignment.rate;
    if (out.rate < 2.f)
    {
        out.capture = true;
        out.hold = false;
        out.tick = state.captureTick + 1;
        return out;
    }
    out.tick = tickIndex(t, out.rate, assignment.phase);
    out.boilIndex = tickIndex(t, assignment.rate, assignment.phase) / cfg.boilTicks;
    const bool rate_changed = !state.hasCapture || state.captureRate != out.rate;
    bool boundary = !rate_changed && out.tick != state.captureTick;
    bool capture = !state.hasCapture;
    if (rate_changed && state.hasCapture)
    {
        state.captureRate = out.rate;
        state.captureTick = out.tick;
        boundary = false;
    }
    if (state.forceCapture || state.impactPending)
    {
        capture = true;
        state.forceCapture = false;
        if (state.impactPending)
        {
            state.impactPending = false;
            state.impactHoldLeft = cfg.impactTicks;
            state.impactCooldown = true;
        }
    }
    else if (boundary)
    {
        if (state.impactHoldLeft > 0)
            --state.impactHoldLeft;
        else
        {
            capture = true;
            if (state.impactCooldown) state.impactCooldown = false;
        }
    }
    if (capture || boundary)
    {
        state.hasCapture = true;
        state.captureRate = out.rate;
        state.captureTick = out.tick;
    }
    out.capture = capture;
    return out;
}

LLQuaternion boilRotation(U64 seed, S64 boil_index, U32 joint, F32 amplitude_deg)
{
    amplitude_deg = llclamp(finiteOr(amplitude_deg, 0.f), 0.f, 3.f);
    LLVector3 axis(unitHash(seed, boil_index, joint, 0) * 2.f - 1.f,
                   unitHash(seed, boil_index, joint, 1) * 2.f - 1.f,
                   unitHash(seed, boil_index, joint, 2) * 2.f - 1.f);
    if (axis.lengthSquared() < 1.e-8f) axis.set(1.f, 0.f, 0.f);
    axis.normalize();
    const F32 angle = (unitHash(seed, boil_index, joint, 3) * 2.f - 1.f) *
        amplitude_deg * DEG_TO_RAD;
    return LLQuaternion(angle, axis);
}

SmearChain smearFor(F32 displacement, F32 tick_dt, F32 chain_len, F32 min_bone_len,
                    U32 n_joints, F32 strength, F32 threshold)
{
    SmearChain out;
    strength = llclamp(finiteOr(strength, 0.f), 0.f, 1.f);
    if (strength == 0.f || n_joints == 0 || !(tick_dt > 0.f) || chain_len < .01f ||
        min_bone_len < .005f || displacement < 1.e-4f ||
        !std::isfinite(displacement) || !std::isfinite(tick_dt) || !std::isfinite(chain_len))
        return out;
    if (displacement / tick_dt <= threshold) return out;
    const F32 total = 1.f + .6f * strength * llclamp(displacement / chain_len, 0.f, 1.f);
    out.active = true;
    out.lagT = 1.f - .5f * strength;
    out.stretch = std::pow(total, 1.f / static_cast<F32>(n_joints));
    return out;
}

LLVector3 smearAxisScale(F32 k, const LLVector3& direction)
{
    if (!std::isfinite(k) || k == 1.f) return LLVector3(1.f, 1.f, 1.f);
    LLVector3 unit = direction;
    if (unit.lengthSquared() <= 0.f) return LLVector3(1.f, 1.f, 1.f);
    unit.normalize();
    return LLVector3(1.f + (k - 1.f) * std::fabs(unit[VX]),
                     1.f + (k - 1.f) * std::fabs(unit[VY]),
                     1.f + (k - 1.f) * std::fabs(unit[VZ]));
}

F32 easeWeight(F32 amount, F32 minimum, F32 capture)
{
    amount = llclamp(finiteOr(amount, 0.f), 0.f, 1.f);
    if (amount == 0.f) return 0.f;
    minimum = llmax(0.f, finiteOr(minimum, 0.f));
    capture = llmax(1.e-6f, finiteOr(capture, 0.f));
    return amount * llclamp(1.f - minimum / capture, 0.f, 1.f);
}

std::vector<U8> classifyJoints(const std::vector<JointDesc>& joints)
{
    std::vector<U8> out(joints.size(), JC_BODY);
    S32 face_root = -1;
    for (U32 i = 0; i < joints.size(); ++i)
        if (joints[i].name == "mFaceRoot") face_root = static_cast<S32>(i);
    auto gaze = [](const std::string& name)
    {
        return name == "mNeck" || name == "mHead" || name == "mEyeLeft" ||
            name == "mEyeRight" || name == "mFaceEyeAltLeft" || name == "mFaceEyeAltRight";
    };
    for (U32 i = 0; i < joints.size(); ++i)
    {
        if (joints[i].collisionVolume) { out[i] = JC_PHYS; continue; }
        if (joints[i].parent < 0 && joints[i].name == "mRoot") { out[i] = JC_ROOT; continue; }
        if (gaze(joints[i].name)) { out[i] = JC_GAZE; continue; }
        S32 parent = static_cast<S32>(i);
        while (parent >= 0 && parent != face_root) parent = joints[parent].parent;
        if (parent == face_root) out[i] = JC_FACE;
    }
    return out;
}

void composeRender(const PoseBuffer& smooth, const PoseBuffer& held,
                   const std::vector<U8>& classes, U8 mask, PoseBuffer& out)
{
    out = smooth;
    const U32 count = llmin(static_cast<U32>(smooth.joints.size()),
                            llmin(static_cast<U32>(held.joints.size()), static_cast<U32>(classes.size())));
    for (U32 i = 0; i < count; ++i)
        if (classes[i] & mask)
        {
            out.joints[i].rot = held.joints[i].rot;
            out.joints[i].pos = held.joints[i].pos;
            if (classes[i] & JC_PHYS) out.joints[i].scale = held.joints[i].scale;
        }
    if (mask & JC_ROOT)
    {
        out.rootPos = held.rootPos;
        out.rootRot = held.rootRot;
    }
}

bool bitEqual(const JointLocal& a, const JointLocal& b)
{
    return quaternionBitsEqual(a.rot, b.rot) && vectorBitsEqual(a.pos, b.pos) && vectorBitsEqual(a.scale, b.scale);
}

JointLocal restoreValue(const JointLocal& current, const JointLocal& written,
                        const JointLocal& smooth, bool epoch_changed)
{
    (void)epoch_changed;
    JointLocal out = current;
    if (quaternionBitsEqual(current.rot, written.rot)) out.rot = smooth.rot;
    if (vectorBitsEqual(current.pos, written.pos)) out.pos = smooth.pos;
    if (vectorBitsEqual(current.scale, written.scale)) out.scale = smooth.scale;
    return out;
}

RotoMatch mapRotoInk(F32 rate)
{
    RotoMatch out;
    out.sketchFps = llclamp(finiteOr(rate, 12.f), 2.f, 24.f);
    const struct { F32 rate; S32 step; } exact[] = {{12.f, 1}, {8.f, 2}, {6.f, 3}};
    for (const auto& item : exact)
        if (std::fabs(rate - item.rate) <= .01f)
        {
            out.motionStep = item.step;
            out.setMotionStep = true;
            break;
        }
    return out;
}

const std::vector<std::string>& trackedSettings()
{
    static const std::vector<std::string> settings = {
        "StepAnimScope", "StepAnimMode", "StepAnimRate", "StepAnimFastRate",
        "StepAnimAdaptThreshold", "StepAnimAdaptHysteresis", "StepAnimAdaptMinDwell",
        "StepAnimBackgroundRate", "StepAnimWhatSteps", "StepAnimFace", "StepAnimGaze",
        "StepAnimFlexi", "StepAnimAttachFX", "StepAnimParticles", "StepAnimTextureAnim",
        "StepAnimClock", "StepAnimOffsetMode",
        "StepAnimPhase", "StepAnimBaseFPS", "StepAnimBoil", "StepAnimBoilAmount",
        "StepAnimBoilTicks", "StepAnimImpactHold", "StepAnimImpactTicks",
        "StepAnimImpactThreshold", "StepAnimSmear", "StepAnimSmearStrength",
        "StepAnimSmearThreshold", "StepAnimEase", "StepAnimEaseAmount", "StepAnimMatchRotoInk"
    };
    return settings;
}

const std::vector<std::string>& presetSettings()
{
    static const std::vector<std::string> settings = []
    {
        std::vector<std::string> out;
        for (const std::string& name : trackedSettings())
            if (name != "StepAnimParticles" && name != "StepAnimTextureAnim") out.push_back(name);
        return out;
    }();
    return settings;
}

const std::vector<Preset>& builtInPresets()
{
    static const std::vector<Preset> presets = []
    {
        std::vector<Preset> out;
        auto add = [&out](const char* id, const char* label, Config values)
        {
            out.push_back({id, label, values});
        };
        Config c;
        c.ease = true; c.easeAmount = .3f; add("anime_twos", "Anime on twos", c);
        c = Config(); c.rate = 8.f; c.ease = true; c.easeAmount = .3f; add("anime_threes", "Anime on threes (limited)", c);
        c = Config(); c.mode = MODE_ADAPTIVE; c.impact = true; c.impactTicks = 2; c.impactThreshold = 30.f;
        c.smear = true; c.smearStrength = .4f; c.ease = true; c.easeAmount = .4f; c.matchRoto = true;
        add("kpdh", "KPop Demon Hunters", c);
        c = Config(); c.scope = SCOPE_VIEW; c.mode = MODE_MIXED; c.smear = true; c.smearStrength = .6f; c.smearThreshold = 2.f;
        add("spiderverse", "Spider-Verse mixed", c);
        c = Config(); c.offset = OFFSET_LOCKED; c.face = true; c.boil = true; add("stopmotion", "Stop-motion (Laika/Aardman)", c);
        c = Config(); c.offset = OFFSET_LOCKED; c.face = true; c.boil = true; c.boilAmount = 1.5f; c.boilTicks = 1;
        add("claymation", "Claymation boil", c);
        c = Config(); c.scope = SCOPE_SELECTED; c.boil = true; c.boilAmount = .3f; add("harryhausen", "Harryhausen creature", c);
        c = Config(); c.face = true; c.matchRoto = true; add("rotoscope", "Rotoscope (A Scanner Darkly style)", c);
        c = Config(); c.rate = 16.f; c.offset = OFFSET_LOCKED; add("undercrank", "Silent film undercrank", c);
        c = Config(); c.rate = 10.f; c.what = WHAT_POSE; c.face = false; c.gaze = false; c.boil = true; c.boilAmount = .3f;
        add("puppet", "Puppet / marionette", c);
        c = Config(); c.rate = 6.f; c.offset = OFFSET_LOCKED; add("retro_sprite", "Retro sprite", c);
        c = Config(); c.rate = 4.f; c.offset = OFFSET_LOCKED; add("strobe", "Choppy dream / strobe", c);
        c = Config(); c.rate = 24.f; c.impact = true; c.impactTicks = 4; c.impactThreshold = 20.f; c.smear = true;
        add("hitstop", "Hit-stop action", c);
        c = Config(); add("custom", "Custom (start from defaults)", c);
        return out;
    }();
    return presets;
}
}
