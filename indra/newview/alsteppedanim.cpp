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
    out.scope = llclamp(raw.scope, 0, 4);
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
    out.nearbyRadius = llclamp(finiteOr(raw.nearbyRadius, 20.f), 1.f, 256.f);
    out.phaseSpread = llclamp(finiteOr(raw.phaseSpread, 1.f), 0.f, 1.f);
    out.rateVariation = llclamp(finiteOr(raw.rateVariation, 0.f), 0.f, 1.f);
    out.timingJitter = llclamp(finiteOr(raw.timingJitter, 0.f), 0.f, 1.f);
    for (U32 part = 0; part < kPartGroups; ++part)
        out.parts[part] = part == PART_TORSO ? RATE_SAME : llclamp(raw.parts[part], 0, 3);
    return out;
}

const char* partSetting(U32 part)
{
    static const char* const names[kPartGroups] = {
        "", "StepAnimPartHead", "StepAnimPartFace", "StepAnimPartArms", "StepAnimPartHands",
        "StepAnimPartLegs", "StepAnimPartExtras", "StepAnimPartAttach"
    };
    return part < kPartGroups ? names[part] : "";
}

const char* partKey(U32 part)
{
    static const char* const keys[kPartGroups] = {
        "torso", "head", "face", "arms", "hands", "legs", "extras", "attach"
    };
    return part < kPartGroups ? keys[part] : "";
}

ActorOverride sanitize(const ActorOverride& raw)
{
    ActorOverride out = raw;
    out.mode = llclamp(raw.mode, 0, 2);
    out.rate = llclamp(finiteOr(raw.rate, 12.f), 2.f, 30.f);
    out.what = llclamp(raw.what, -1, 1);
    out.role = llclamp(raw.role, 0, 2);
    out.rateOffset = llclamp(raw.rateOffset, -1, 2);
    for (U32 part = 0; part < kPartGroups; ++part)
        out.parts[part] = part == PART_TORSO ? -1 : llclamp(raw.parts[part], -1, 3);
    return out;
}

LLSD writeOverride(const ActorOverride& input)
{
    const ActorOverride ov = sanitize(input);
    if (ov.mode == 0 && ov.rate == 12.f && ov.what == -1 && ov.role == 0 && ov.rateOffset == -1 &&
        ov.parts == kPartsInherit)
        return LLSD();
    LLSD value = LLSD::emptyMap();
    value["mode"] = ov.mode;
    value["rate"] = ov.rate;
    value["what"] = ov.what;
    value["role"] = ov.role;
    if (ov.rateOffset != -1) value["rate_offset"] = ov.rateOffset;
    if (ov.parts != kPartsInherit)
    {
        // Only the parts that override are written, keyed by name (partKey), so a scene stays
        // readable if groups are ever added; an absent key reads back as inherit.
        LLSD parts = LLSD::emptyMap();
        for (U32 part = PART_HEAD; part < kPartGroups; ++part)
            if (ov.parts[part] != -1) parts[partKey(part)] = ov.parts[part];
        value["parts"] = parts;
    }
    return value;
}

ActorOverride readOverride(const LLSD& data)
{
    ActorOverride ov;
    if (!data.isMap()) return ov;
    if (data.has("mode")) ov.mode = data["mode"].asInteger();
    if (data.has("rate")) ov.rate = static_cast<F32>(data["rate"].asReal());
    if (data.has("what")) ov.what = data["what"].asInteger();
    if (data.has("role")) ov.role = data["role"].asInteger();
    if (data.has("rate_offset")) ov.rateOffset = data["rate_offset"].asInteger();
    if (data.has("parts") && data["parts"].isMap())
        for (U32 part = PART_HEAD; part < kPartGroups; ++part)
            if (data["parts"].has(partKey(part))) ov.parts[part] = data["parts"][partKey(part)].asInteger();
    return sanitize(ov);
}

PartOffsets resolveParts(const Config& raw, const ActorOverride& raw_ov, S32 group_offset)
{
    const Config cfg = sanitize(raw);
    const ActorOverride ov = sanitize(raw_ov);
    // A cast group with its own rate steps its members on one grid: the page's part rates are not
    // applied to them (actor > group > global), only an actor's own part overrides are.
    const bool grouped = groupRateInEffect(cfg, ov, group_offset); // the rate the actor really steps on
    PartOffsets out = kPartsSame;
    for (U32 part = PART_HEAD; part < kPartGroups; ++part)
    {
        S32 offset = grouped ? RATE_SAME : cfg.parts[part];
        // StepAnimFace off is the page's Face & lip-sync switch (presets, scenes): Smooth for all.
        if (part == PART_FACE && !cfg.face) offset = RATE_SMOOTH;
        if (ov.parts[part] >= 0) offset = ov.parts[part];
        out[part] = offset;
    }
    return out;
}

bool groupRateApplies(const ActorOverride& raw_ov, S32 group_offset)
{
    const ActorOverride ov = sanitize(raw_ov);
    return llclamp(group_offset, 0, 3) != RATE_SAME && ov.rateOffset < 0 && ov.mode != 2;
}

bool groupRateInEffect(const Config& raw, const ActorOverride& ov, S32 group_offset)
{
    return groupRateApplies(ov, group_offset) && !sanitize(raw).matchRoto;
}

S32 faceControlShown(bool face_on, S32 part_face)
{
    const S32 part = llclamp(part_face, 0, 3);
    return !face_on || part == RATE_SMOOTH ? RATE_SMOOTH : part;
}

void applyFaceControl(S32 value, bool& face_on, S32& part_face)
{
    const S32 v = llclamp(value, 0, 3);
    if (v == RATE_SMOOTH) { face_on = false; return; }
    face_on = true;
    part_face = v;
}

S32 groupRate(const GroupRates& rates, const std::string& group)
{
    const auto it = rates.find(group);
    return it != rates.end() ? llclamp(it->second, 0, 3) : 0;
}

void setGroupRate(GroupRates& rates, const std::string& group, S32 offset)
{
    if (group.empty()) return;
    offset = llclamp(offset, 0, 3);
    if (offset == 0) rates.erase(group); // 0 = no entry, so the map never bloats
    else rates[group] = offset;
}

void renameGroupRate(GroupRates& rates, const std::string& old_name, const std::string& new_name)
{
    if (old_name.empty() || new_name.empty() || old_name == new_name) return;
    const auto it = rates.find(old_name);
    if (it == rates.end()) return;
    // Renaming onto an existing group merges the two: the target's own rate wins, else the source's rides along.
    if (rates.find(new_name) == rates.end()) rates[new_name] = it->second;
    rates.erase(old_name);
}

void pruneGroupRate(GroupRates& rates, const std::string& group, bool group_has_members)
{
    if (!group.empty() && !group_has_members) rates.erase(group);
}

LLSD writeGroupRates(const GroupRates& rates, const std::vector<std::string>& live_groups)
{
    LLSD out = LLSD::emptyMap();
    for (const std::string& name : live_groups)
    {
        const S32 offset = groupRate(rates, name);
        if (offset > 0) out[name] = offset;
    }
    return out;
}

GroupRates readGroupRates(const LLSD& data, const std::vector<std::string>& live_groups)
{
    GroupRates out;
    if (!data.isMap()) return out;
    for (const std::string& name : live_groups)
        if (data.has(name)) setGroupRate(out, name, data[name].asInteger());
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

U64 mixSeed(U64 digest, U32 user_seed)
{
    if (user_seed == 0) return digest;
    return splitmix64(digest ^ splitmix64(0x5eed5eed00000000ULL + user_seed));
}

F32 rateFactor(U64 seed, F32 variation)
{
    variation = llclamp(finiteOr(variation, 0.f), 0.f, 1.f);
    return 1.f + variation * unitHash(seed, 0x72617465LL, 1, 0);
}

F32 offsetRate(F32 rate, S32 offset, S32 base_fps)
{
    if (offset < RATE_PLUS1 || offset > RATE_PLUS2 || !(rate >= 2.f) || base_fps < 1) return rate;
    const F32 stepped = 1.f / (1.f / rate + static_cast<F32>(offset) / static_cast<F32>(base_fps));
    return llmax(stepped, 2.f);
}

F32 varyRateAtFactor(F32 rate, F32 factor, S32 base_fps)
{
    if (!(rate >= 2.f) || base_fps < 1) return rate;
    const F32 base = static_cast<F32>(base_fps);
    const F32 length = base / rate; // the actor's own step, in base frames
    const F32 whole = std::floor(length + .5f);
    if (length >= 1.f && std::fabs(length - whole) <= 1.e-3f)
    {
        // The actor steps in whole base frames: lengthen by whole base frames, never shorter
        // than its own step and never past the 2 fps floor.
        const S32 own = static_cast<S32>(whole);
        const S32 longest = llmax(own, static_cast<S32>(std::floor(base * .5f)));
        const S32 frames = llclamp(static_cast<S32>(std::floor(length * factor + .5f)), own, longest);
        return frames == own ? rate : base / static_cast<F32>(frames);
    }
    // Any other rate (10 or 30 fps on a 24 base): its own step is the grid, so the step becomes
    // a whole number of its own steps (x1 or x2 for a spread of at most 1).
    const S32 steps = llmax(1, static_cast<S32>(std::floor(factor + .5f)));
    const F32 varied = rate / static_cast<F32>(steps);
    return varied >= 2.f ? varied : rate;
}

F32 varyRate(F32 rate, U64 seed, F32 variation, S32 base_fps)
{
    if (!(variation > 0.f) || !(rate >= 2.f) || base_fps < 1) return rate;
    return varyRateAtFactor(rate, rateFactor(seed, variation), base_fps);
}

bool extraHold(U64 seed, S64 tick, F32 jitter, bool previous_was_extra)
{
    jitter = llclamp(finiteOr(jitter, 0.f), 0.f, 1.f);
    if (!(jitter > 0.f) || previous_was_extra) return false;
    return unitHash(seed, tick, 0x6a697474, 2) < jitter * .25f;
}

F64 actorPhase(const Config& cfg, U64 seed, F64 roto_phase)
{
    F64 phase = 0.;
    if (cfg.offset == OFFSET_RANDOM)
        phase = static_cast<F64>(unitHash(seed, 0x7068617365LL, 0, 0)) *
            static_cast<F64>(llclamp(finiteOr(cfg.phaseSpread, 1.f), 0.f, 1.f));
    else if (cfg.offset == OFFSET_MANUAL)
        phase = cfg.phase;
    return fraction(phase + roto_phase);
}

U16 heldClasses(const Config& cfg, S32 what, const PartOffsets& parts)
{
    U16 mask = JC_BODY;
    if (what == WHAT_POSE_POSITION) mask |= JC_ROOT;
    // Face bones follow the Face part (Smooth while StepAnimFace is off, see resolveParts).
    if (parts[PART_FACE] != RATE_SMOOTH) mask |= JC_FACE;
    if (cfg.gaze) mask |= JC_GAZE;
    if (cfg.flexi) mask |= JC_PHYS;
    for (U32 part = PART_TORSO; part < PART_ATTACH; ++part)
        if (part == PART_TORSO || parts[part] != RATE_SMOOTH) mask |= groupBit(part);
    // Attachment points are left alone (they ride their bone) unless the Attachments part differs
    // from the body: coarser holds their placement, Smooth re-places them from the smooth pose.
    if (parts[PART_ATTACH] != RATE_SAME) mask |= JC_ATTACH | groupBit(PART_ATTACH);
    return mask;
}

U16 heldClasses(const Config& cfg, S32 what)
{
    return heldClasses(cfg, what, resolveParts(cfg, ActorOverride()));
}

U32 partOf(U16 joint_class)
{
    for (U32 part = 0; part < kPartGroups; ++part)
        if (joint_class & groupBit(part)) return part;
    return PART_TORSO;
}

namespace
{
// Membership on a sanitized config; includes the mixed-rate "cast plus target" rule.
bool scopeMember(const Config& cfg, const ActorFacts& facts)
{
    bool member = false;
    switch (cfg.scope)
    {
        case SCOPE_SELECTED: member = facts.isSelected; break;
        case SCOPE_CAST: member = facts.inCast; break;
        case SCOPE_VIEW: member = facts.drawn; break;
        case SCOPE_FRIENDS: member = facts.drawn && (facts.isFriend || facts.isSelf); break;
        case SCOPE_NEARBY: member = facts.drawn && facts.cameraDistance <= cfg.nearbyRadius; break;
        default: break;
    }
    // Everyone / Friends / Nearby include you only when "Include me" is on.
    if (cfg.scope >= SCOPE_VIEW && facts.isSelf && !cfg.includeSelf) member = false;
    // Mixed mode treats Selected actor and Whole cast as "cast plus the target".
    if (cfg.mode == MODE_MIXED && (cfg.scope == SCOPE_SELECTED || cfg.scope == SCOPE_CAST))
        member = facts.inCast || facts.isSelected;
    return member;
}

bool withinLimit(const Config& cfg, const ActorFacts& facts)
{
    return !cfg.limitDistance || facts.cameraDistance <= cfg.nearbyRadius;
}
}

bool scopeIncludes(const Config& raw, const ActorFacts& facts)
{
    const Config cfg = sanitize(raw);
    return scopeMember(cfg, facts) && withinLimit(cfg, facts);
}

RateChoice resolveRate(const Config& raw, const ActorOverride& raw_ov, bool selected, S32 group_offset)
{
    const Config cfg = sanitize(raw);
    const ActorOverride ov = sanitize(raw_ov);
    RateChoice out;
    if (ov.mode == 1) return out;
    bool target = selected;
    if (ov.role == 1) target = true;
    else if (ov.role == 2) target = false;

    F32 rate = cfg.rate;
    bool adaptive = cfg.mode == MODE_ADAPTIVE;
    if (cfg.mode == MODE_MIXED)
    {
        rate = target ? cfg.rate : cfg.backgroundRate;
        adaptive = false;
    }
    // Rate offset precedence: per-actor > cast group > page (no offset).
    // groupRateApplies is the predicate resolveParts shares, so the parts rule follows the rate the
    // actor really steps on (a Roto Ink lock is applied below and reported as rotoSuppressed).
    S32 offset = groupRateApplies(ov, group_offset) ? llclamp(group_offset, 0, 3) : RATE_SAME;
    if (ov.rateOffset >= 0) offset = ov.rateOffset;
    if (ov.mode == 2)
    {
        rate = ov.rate;
        adaptive = false;
        if (ov.rateOffset < 0) offset = RATE_SAME;
    }
    rate = effectiveRate(rate);
    if (rate == 0.f || offset == RATE_SMOOTH) return out;

    // Match Roto Ink redraws on the page rate's cadence. While it is on, group and actor rate
    // offsets and the seed rate variation are disabled, so the steps stay on Roto's redraws.
    // Own rates, the Mixed background rate and the Adaptive fast rate are not re-timed to that
    // cadence; they keep their own values. Smooth, Never and the imperfect-timing extra hold
    // are unaffected.
    F32 variation = cfg.rateVariation;
    out.rotoLocked = cfg.matchRoto;
    if (cfg.matchRoto)
    {
        out.rotoSuppressed = offset != RATE_SAME || variation > 0.f;
        offset = RATE_SAME;
        variation = 0.f;
    }
    out.stepped = true;
    out.adaptive = adaptive;
    out.offset = offset;
    out.variation = variation;
    out.rate = offsetRate(rate, offset, cfg.baseFps);
    out.fastRate = adaptive && cfg.fastRate > 0.f ? offsetRate(cfg.fastRate, offset, cfg.baseFps) : 0.f;
    return out;
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
    bool in_scope = scopeMember(cfg, facts) && withinLimit(cfg, facts);
    // Own settings step the actor even outside the scope, but Limit distance caps every scope.
    if (ov.mode == 2) in_scope = withinLimit(cfg, facts);
    if (cfg.includeGhosts && facts.ghostOwnAnimation && withinLimit(cfg, facts)) in_scope = true;
    const RateChoice choice = resolveRate(cfg, ov, facts.isSelected, facts.groupOffset);
    if (!in_scope || !choice.stepped) return out;

    out.stepped = true;
    out.adaptive = choice.adaptive;
    out.rate = varyRate(choice.rate, facts.seed, choice.variation, cfg.baseFps);
    out.fastRate = choice.fastRate > 0.f
        ? varyRate(choice.fastRate, facts.seed, choice.variation, cfg.baseFps) : 0.f;
    out.seed = facts.seed;
    out.what = ov.mode == 2 && ov.what >= 0 ? ov.what : cfg.what;
    out.phase = actorPhase(cfg, facts.seed, cfg.matchRoto ? roto_phase : 0.);
    out.parts = resolveParts(cfg, ov, facts.groupOffset);
    out.heldClasses = heldClasses(cfg, out.what, out.parts);
    return out;
}

bool ghostMayInherit(const Config& raw, const ActorFacts& facts)
{
    const Config cfg = sanitize(raw);
    const ActorOverride ov = sanitize(facts.ov);
    return cfg.enabled && facts.drawn && !facts.excludedAlways && !facts.posing &&
        (!cfg.excludeCameraMounted || !facts.cameraMounted) && ov.mode != 1 &&
        !(cfg.includeGhosts && facts.ghostOwnAnimation);
}

bool inheritAttachedAssignment(const Assignment& source, U64 source_seed,
                               Assignment& attached, U64& attached_seed,
                               S32 attach_offset, S32 base_fps)
{
    if (!source.stepped || attach_offset == RATE_SMOOTH)
    {
        // Attachments Smooth: the worn animesh keeps its own smooth animation.
        if (source.stepped) attached = Assignment();
        return false;
    }
    attached = source;
    // The control avatar follows its worn volume transform. Holding its own
    // root as well double-applies the wearer's held attachment transform.
    attached.heldClasses &= ~JC_ROOT;
    // One grid for the whole animesh: its own parts are Same, its attachment points untouched.
    attached.parts = kPartsSame;
    attached.heldClasses = static_cast<U16>((attached.heldClasses & kClassBits & ~JC_ATTACH) |
                                            (kGroupBits & ~groupBit(PART_ATTACH)));
    if (attach_offset == RATE_PLUS1 || attach_offset == RATE_PLUS2)
    {
        attached.rate = offsetRate(source.rate, attach_offset, base_fps);
        if (source.fastRate > 0.f) attached.fastRate = offsetRate(source.fastRate, attach_offset, base_fps);
        // The wearer's Attachments grid: same time offset as the wearer's rigid attachment points.
        attached.phase = partPhase(source.phase, source.rate, attached.rate);
    }
    attached_seed = source_seed;
    return true;
}

FrameDecision attachedDecision(const Assignment& attached, const ActorState& wearer_state,
                               const FrameDecision& wearer_decision, S32 attach_offset, S32 base_fps)
{
    if (!attached.stepped) return wearer_decision;
    FrameDecision out = wearer_decision;
    const bool coarser = attach_offset == RATE_PLUS1 || attach_offset == RATE_PLUS2;
    if (coarser && wearer_decision.rate >= 2.f) // (a smooth, below-2-fps wearer captures every frame)
    {
        out.rate = offsetRate(wearer_decision.rate, attach_offset, base_fps);
        out.capture = (wearer_decision.captureGroups & groupBit(PART_ATTACH)) != 0;
        out.tick = wearer_state.partTick[PART_ATTACH];
        out.boilIndex = wearer_decision.partBoil[PART_ATTACH];
    }
    // The animesh is one grid: a forced capture of the wearer's Attachments part (its offset just
    // changed, Same included) is a forced capture of every animesh group.
    const bool forced = (wearer_decision.forceGroups & groupBit(PART_ATTACH)) != 0;
    if (forced) out.capture = true;
    out.captureGroups = out.capture ? kGroupBits : 0;
    out.forceGroups = forced ? kGroupBits : 0;
    out.partBoil.fill(out.boilIndex);
    return out;
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
                          ActorState& attached_state, FrameDecision& attached_decision,
                          S32 attach_offset, S32 base_fps)
{
    if (!inheritAttachedAssignment(source_assignment, source_seed,
                                   attached_assignment, attached_seed, attach_offset, base_fps))
        return;
    attached_state = source_state;
    attached_decision = attachedDecision(attached_assignment, source_state, source_decision,
                                         attach_offset, base_fps);
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

F64 partPhase(F64 phase, F64 rate, F64 part_rate)
{
    // Same time offset (phase / rate seconds) on the part's grid, so a +1 / +2 part's boundaries
    // land on the actor's base-frame lattice and coincide with torso ticks whenever the rates do.
    if (!(rate > 0.) || !std::isfinite(rate) || !std::isfinite(part_rate) || !std::isfinite(phase)) return fraction(phase);
    return fraction(phase * part_rate / rate);
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
        out.captureGroups = kGroupBits;
        return out;
    }
    out.tick = tickIndex(t, out.rate, assignment.phase);
    out.boilIndex = tickIndex(t, assignment.rate, assignment.phase) / cfg.boilTicks;
    const bool rate_changed = !state.hasCapture || state.captureRate != out.rate;
    bool boundary = !rate_changed && out.tick != state.captureTick;
    bool capture = !state.hasCapture;
    // A first or forced capture (impact, adaptive switch) refreshes every part; a plain torso
    // boundary refreshes only the parts that ride the torso grid.
    const bool capture_all = !state.hasCapture || state.forceCapture || state.impactPending;
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
        else if (extraHold(assignment.seed, out.tick, cfg.timingJitter, state.jitterHeld))
            state.jitterHeld = true; // imperfect timing: hold one extra step, never two in a row
        else
        {
            state.jitterHeld = false;
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
    // Part grids. Same parts refresh with the torso; +1 / +2 parts hold on their own, coarser grid
    // (offsetRate of the current rate, partPhase) and refresh only on their own boundaries, exact
    // (no imperfect-timing extra hold) except during an impact hold (impactHoldLeft, then the
    // cooldown until the torso's next capture), when nothing moves; Smooth body parts are never
    // held (not in the mask); Smooth attachments are re-placed from the smooth pose every frame. A
    // part whose effective offset changed is captured at once (forceGroups), so switching a part
    // to another grid never shows a stale hold or an empty attachment placement.
    const bool impact_frozen = !capture && (state.impactHoldLeft > 0 || state.impactCooldown);
    for (U32 part = 0; part < kPartGroups; ++part)
    {
        const S32 offset = part == PART_TORSO ? RATE_SAME : assignment.parts[part];
        const bool offset_changed = state.partOffset[part] != offset;
        state.partOffset[part] = offset;
        // (a Smooth body part is not held, so there is nothing to capture for it)
        if (offset_changed && (offset != RATE_SMOOTH || part == PART_ATTACH)) out.forceGroups |= groupBit(part);
        if (offset == RATE_PLUS1 || offset == RATE_PLUS2)
        {
            const F64 part_rate = offsetRate(out.rate, offset, cfg.baseFps);
            const S64 part_tick = tickIndex(t, part_rate, partPhase(assignment.phase, out.rate, part_rate));
            const bool regrid = state.partRate[part] != part_rate;
            const bool part_capture = capture_all || offset_changed ||
                (!regrid && !impact_frozen && part_tick != state.partTick[part]);
            state.partRate[part] = part_rate;
            state.partTick[part] = part_tick;
            if (part_capture) out.captureGroups |= groupBit(part);
            const F64 boil_rate = offsetRate(assignment.rate, offset, cfg.baseFps);
            out.partBoil[part] = tickIndex(t, boil_rate, partPhase(assignment.phase, assignment.rate, boil_rate)) /
                cfg.boilTicks;
            continue;
        }
        if (offset == RATE_SMOOTH ? part == PART_ATTACH : (capture || offset_changed))
            out.captureGroups |= groupBit(part);
        out.partBoil[part] = out.boilIndex;
    }
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

namespace
{
bool startsWith(const std::string& name, const char* prefix)
{
    return name.compare(0, std::strlen(prefix), prefix) == 0;
}

// Part group a bone names explicitly (avatar_skeleton.xml names); kPartGroups when the bone has
// no name of its own in the table and follows its parent (mSkull, collision volumes, ...).
U32 namedPart(const std::string& n)
{
    if (startsWith(n, "mPelvis") || startsWith(n, "mSpine") || startsWith(n, "mTorso") ||
        startsWith(n, "mChest") || startsWith(n, "mCollar")) return PART_TORSO;
    if (n == "mNeck" || n == "mHead" || startsWith(n, "mEye")) return PART_HEAD;
    if (n == "mFaceRoot") return PART_FACE;
    if (startsWith(n, "mShoulder") || startsWith(n, "mElbow") || startsWith(n, "mWrist")) return PART_ARMS;
    if (startsWith(n, "mHand")) return PART_HANDS;
    if (startsWith(n, "mHip") || startsWith(n, "mKnee") || startsWith(n, "mAnkle") ||
        startsWith(n, "mFoot") || startsWith(n, "mToe")) return PART_LEGS;
    if (startsWith(n, "mTail") || startsWith(n, "mGroin") || startsWith(n, "mHindLimb") ||
        startsWith(n, "mWing")) return PART_EXTRAS;
    return kPartGroups;
}

// Root-relative rotation and position of joint `index` from the local transforms, composed top-down
// exactly as LLXformMatrix::update does for joints: a child's offset is scaled by its parent's scale,
// rotated by the parent's world rotation and added to the parent's world position. `parents[i] < 0`
// is the avatar root, which is left out (it is common to the smooth and the rendered pose).
void rootRelative(const PoseBuffer& pose, const std::vector<S32>& parents, S32 index,
                  LLQuaternion& rot, LLVector3& pos)
{
    constexpr U32 kMaxDepth = 64;
    S32 chain[kMaxDepth];
    U32 depth = 0;
    for (S32 j = index; j >= 0 && static_cast<size_t>(j) < pose.joints.size() && depth < kMaxDepth;
         j = static_cast<size_t>(j) < parents.size() ? parents[j] : -1)
        chain[depth++] = j;
    rot.loadIdentity();
    pos.clear();
    LLVector3 parent_scale(1.f, 1.f, 1.f);
    bool has_parent = false;
    for (S32 k = static_cast<S32>(depth) - 1; k >= 0; --k)
    {
        const JointLocal& local = pose.joints[chain[k]];
        LLVector3 offset = local.pos;
        if (has_parent) offset.scaleVec(parent_scale);
        pos = offset * rot + pos;
        rot = local.rot * rot;
        parent_scale = local.scale;
        has_parent = true;
    }
}
}

std::vector<U16> classifyJoints(const std::vector<JointDesc>& joints)
{
    std::vector<U16> out(joints.size(), JC_BODY);
    S32 face_root = -1;
    for (U32 i = 0; i < joints.size(); ++i)
        if (joints[i].name == "mFaceRoot") face_root = static_cast<S32>(i);
    auto gaze = [](const std::string& name)
    {
        return name == "mNeck" || name == "mHead" || name == "mEyeLeft" ||
            name == "mEyeRight" || name == "mFaceEyeAltLeft" || name == "mFaceEyeAltRight";
    };
    auto parentOf = [&joints](S32 i)
    {
        const S32 parent = joints[i].parent;
        return parent >= 0 && static_cast<size_t>(parent) < joints.size() && parent != i ? parent : -1;
    };
    for (U32 i = 0; i < joints.size(); ++i)
    {
        // Class: what a toggle holds.
        U16 cls = JC_BODY;
        if (joints[i].attachment) cls = JC_ATTACH;
        else if (joints[i].collisionVolume) cls = JC_PHYS;
        else if (joints[i].parent < 0 && joints[i].name == "mRoot") cls = JC_ROOT;
        else if (gaze(joints[i].name)) cls = JC_GAZE;
        else
        {
            S32 parent = static_cast<S32>(i);
            U32 guard = 0;
            while (parent >= 0 && parent != face_root && guard++ < joints.size()) parent = parentOf(parent);
            if (parent == face_root) cls = JC_FACE;
        }
        // Group: which grid holds it. Bones name their own group; everything else (collision
        // volumes, mSkull, unknown bones) follows the nearest named ancestor; parentless is Torso.
        // Every Gaze joint (neck, head, eyes, the Bento alt eyes) is Head: the Gaze toggle holds
        // the eyes exactly as before whatever the Face part says.
        U32 part = PART_TORSO;
        if (joints[i].attachment) part = PART_ATTACH;
        else if (cls == JC_GAZE) part = PART_HEAD;
        else
        {
            S32 j = static_cast<S32>(i);
            U32 guard = 0;
            while (j >= 0 && guard++ < joints.size())
            {
                if (!joints[j].collisionVolume && !joints[j].attachment)
                {
                    const U32 named = namedPart(joints[j].name);
                    if (named < kPartGroups) { part = named; break; }
                }
                j = parentOf(j);
            }
        }
        out[i] = static_cast<U16>(cls | groupBit(part));
    }
    return out;
}

void composeRender(const PoseBuffer& smooth, const PoseBuffer& held,
                   const std::vector<U16>& classes, U16 mask, PoseBuffer& out)
{
    out = smooth;
    const U32 count = llmin(static_cast<U32>(smooth.joints.size()),
                            llmin(static_cast<U32>(held.joints.size()), static_cast<U32>(classes.size())));
    for (U32 i = 0; i < count; ++i)
        // Attachment points are placed by applyAttachmentHolds, never copied from the held locals.
        if (!(classes[i] & JC_ATTACH) && jointHeld(classes[i], mask))
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

U16 refreshGroups(const Assignment& assignment, const FrameDecision& decision)
{
    U16 groups = 0;
    for (U32 part = 0; part < kPartGroups; ++part)
    {
        const S32 offset = part == PART_TORSO ? RATE_SAME : assignment.parts[part];
        if (offset == RATE_PLUS1 || offset == RATE_PLUS2) groups |= decision.captureGroups & groupBit(part);
        else if (offset == RATE_SMOOTH) { if (part == PART_ATTACH) groups |= groupBit(part); }
        else if (decision.capture || (decision.forceGroups & groupBit(part))) groups |= groupBit(part);
    }
    return groups;
}

F32 smearChainDt(const Assignment& assignment, U16 chain_class, bool torso_smear_tick,
                 bool part_smear_tick, F32 decision_rate, S32 base_fps)
{
    if (!assignment.stepped || !jointHeld(chain_class, assignment.heldClasses)) return 0.f;
    const U32 part = partOf(chain_class);
    const S32 offset = part == PART_TORSO ? RATE_SAME : assignment.parts[part];
    if (offset == RATE_SMOOTH) return 0.f;
    if (offset == RATE_PLUS1 || offset == RATE_PLUS2)
        return part_smear_tick ? 1.f / llmax(offsetRate(decision_rate, offset, base_fps), 2.f) : 0.f;
    return torso_smear_tick ? 1.f / llmax(decision_rate, 2.f) : 0.f;
}

JointLocal relativePlacement(const PoseBuffer& pose, const std::vector<S32>& parents, S32 index)
{
    JointLocal out;
    if (index < 0 || static_cast<size_t>(index) >= pose.joints.size()) return out;
    out.scale = pose.joints[index].scale;
    rootRelative(pose, parents, index, out.rot, out.pos);
    return out;
}

JointLocal localForPlacement(const PoseBuffer& pose, const std::vector<S32>& parents, S32 index,
                             const JointLocal& relative)
{
    JointLocal out;
    if (index < 0 || static_cast<size_t>(index) >= pose.joints.size()) return out;
    out = pose.joints[index]; // the joint's own scale is never held
    const S32 parent = static_cast<size_t>(index) < parents.size() ? parents[index] : -1;
    if (parent < 0 || static_cast<size_t>(parent) >= pose.joints.size())
    {
        out.rot = relative.rot;
        out.pos = relative.pos;
        return out;
    }
    LLQuaternion parent_rot;
    LLVector3 parent_pos;
    rootRelative(pose, parents, parent, parent_rot, parent_pos);
    out.rot = relative.rot * ~parent_rot;
    LLVector3 offset = (relative.pos - parent_pos) * ~parent_rot;
    const LLVector3& scale = pose.joints[parent].scale;
    for (U32 axis = 0; axis < 3; ++axis)
        offset.mV[axis] = std::fabs(scale.mV[axis]) > 1.e-6f ? offset.mV[axis] / scale.mV[axis]
                                                             : pose.joints[index].pos.mV[axis];
    out.pos = offset;
    return out;
}

void captureAttachmentHolds(const PoseBuffer& smooth, const std::vector<U16>& classes,
                            const std::vector<S32>& parents, U16 mask, std::vector<JointLocal>& held)
{
    if (held.size() != smooth.joints.size()) held.resize(smooth.joints.size()); // skeleton change only
    const U32 count = llmin(static_cast<U32>(smooth.joints.size()), static_cast<U32>(classes.size()));
    for (U32 i = 0; i < count; ++i)
        if ((classes[i] & JC_ATTACH) && jointHeld(classes[i], mask))
            held[i] = relativePlacement(smooth, parents, static_cast<S32>(i));
}

void advancePartHistory(const PoseBuffer& held, const std::vector<U16>& classes, U16 groups, bool root,
                        PoseBuffer& previous)
{
    if (previous.joints.size() != held.joints.size())
    {
        previous = held; // skeleton change / first history: everything starts from the held pose
        return;
    }
    const U32 count = llmin(static_cast<U32>(held.joints.size()), static_cast<U32>(classes.size()));
    for (U32 i = 0; i < count; ++i)
        if (classes[i] & groups & kGroupBits) previous.joints[i] = held.joints[i];
    if (root)
    {
        previous.rootPos = held.rootPos;
        previous.rootRot = held.rootRot;
    }
}

void applyAttachmentHolds(const std::vector<U16>& classes, const std::vector<S32>& parents, U16 mask,
                          const std::vector<JointLocal>& held, PoseBuffer& render)
{
    const U32 count = llmin(static_cast<U32>(render.joints.size()),
                            llmin(static_cast<U32>(held.size()), static_cast<U32>(classes.size())));
    for (U32 i = 0; i < count; ++i)
        if ((classes[i] & JC_ATTACH) && jointHeld(classes[i], mask))
            render.joints[i] = localForPlacement(render, parents, static_cast<S32>(i), held[i]);
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
        "StepAnimScope", "StepAnimIncludeGhosts", "StepAnimMode", "StepAnimRate", "StepAnimFastRate",
        "StepAnimAdaptThreshold", "StepAnimAdaptHysteresis", "StepAnimAdaptMinDwell",
        "StepAnimBackgroundRate", "StepAnimWhatSteps", "StepAnimFace", "StepAnimGaze",
        "StepAnimFlexi", "StepAnimAttachFX", "StepAnimParticles", "StepAnimTextureAnim",
        "StepAnimClock", "StepAnimOffsetMode",
        "StepAnimPhase", "StepAnimBaseFPS", "StepAnimBoil", "StepAnimBoilAmount",
        "StepAnimBoilTicks", "StepAnimImpactHold", "StepAnimImpactTicks",
        "StepAnimImpactThreshold", "StepAnimSmear", "StepAnimSmearStrength",
        "StepAnimSmearThreshold", "StepAnimEase", "StepAnimEaseAmount", "StepAnimMatchRotoInk",
        "StepAnimNearbyRadius", "StepAnimLimitDistance", "StepAnimIncludeSelf", "StepAnimSeed",
        "StepAnimPhaseSpread", "StepAnimRateVariation", "StepAnimTimingJitter",
        // Part rates (preset-owned: a look, like Face / Gaze). Order = EPartGroup from PART_HEAD.
        "StepAnimPartHead", "StepAnimPartFace", "StepAnimPartArms", "StepAnimPartHands",
        "StepAnimPartLegs", "StepAnimPartExtras", "StepAnimPartAttach"
    };
    return settings;
}

namespace
{
// Targeting and variation belong to the scene / take, never to a style preset. (Group rates are
// scene data in the cast; the legacy StepAnimGroupRates setting is no longer read.)
bool isTargetingOrVariation(const std::string& name)
{
    static const char* const names[] = {
        "StepAnimNearbyRadius", "StepAnimLimitDistance", "StepAnimIncludeSelf", "StepAnimSeed",
        "StepAnimPhaseSpread", "StepAnimRateVariation", "StepAnimTimingJitter"
    };
    for (const char* item : names) if (name == item) return true;
    return false;
}
}

const std::vector<std::string>& presetSettings()
{
    static const std::vector<std::string> settings = []
    {
        std::vector<std::string> out;
        for (const std::string& name : trackedSettings())
            if (name != "StepAnimParticles" && name != "StepAnimTextureAnim" &&
                name != "StepAnimIncludeGhosts" && !isTargetingOrVariation(name)) out.push_back(name);
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
