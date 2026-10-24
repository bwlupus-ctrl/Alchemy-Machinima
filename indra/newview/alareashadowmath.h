/** Deterministic, allocation-free Phase 2 selection and view policy. */
#ifndef AL_AREA_SHADOW_MATH_H
#define AL_AREA_SHADOW_MATH_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
namespace ALAreaShadow
{
struct View
{
    bool cube = false, prism = false, hud = false, impostor = false;
    bool reflection = false, tiled = false;
};
inline bool allowed(const View& v, bool feeds = false)
{
    return !(v.cube || (v.prism && !feeds) || v.hud || v.impostor || v.reflection || v.tiled);
}
// Importance ranking (10/22). Every light that could receive a soft-shadow channel is
// ranked by how much it matters on screen, whether or not it owns a native SL shadow
// map (the older "unmapped first" rule let eight nearby practicals push out a bright
// mapped projector). Inputs are this frame's only:
//   luminance    : Rec.709 luminance of the linear colour x light scale, BEFORE Local lights EV;
//                 exposure changes radiance only, preserving channels and their quality/strength
//                 even when rig lights are excluded from the exposure adjustment;
//   range        : the light's influence range (radius * 1.5, metres);
//   distance     : camera to light centre (metres);
//   nativeWeight : how much of the light is covered by a native SL map right now, 0..1.
//   importance = luminance * range / max(distance, range / 2) * (1 + kUnmappedBonus * (1 - nativeWeight))
// The camera term saturates at range / 2 instead of testing "inside the range", so a
// lamp 5-10 m away is never worth 0 merely because the camera is outside its range and
// dollying across a range boundary changes nothing. The score is continuous and
// non-increasing in distance; there is no history, hysteresis or timing.
// The bonus is bounded and CONTINUOUS in nativeWeight: the pipeline passes the slot fade
// times the cutoff validity at the light, so acquiring or losing a slot (the fade runs
// 0 -> 1) and a light crossing the native shadow distance move the score smoothly, never a
// 25 % step that could pop a channel. A light with no native map has weight 0; at 0.25 it
// never outranks a light more than 25 % more important.
// The one remaining step: when a slot's map STOPS being sampled (nativeMapSampled flips false:
// Preferences shadow detail lowered, the slot lost to the live limit, or a map not rendered
// that frame) the light genuinely has no native shadow from that frame on, so its weight is 0
// and the bonus returns in one frame. Holding the old fade through that flip would need the
// previous frame (history, which the ranking forbids) and would rank a light by a shadow it no
// longer has; it only happens on a user action or a lost slot, and is documented as such.
constexpr double kUnmappedBonus = 0.25;
inline double importance(double luminance, double range, double distance, double nativeWeight)
{
    if (!(luminance > 0.0) || !(range > 0.0)) return 0.0;
    const double weight = std::isfinite(nativeWeight) ? std::clamp(nativeWeight, 0.0, 1.0) : 0.0;
    const double score = luminance * range / std::max(distance, range * 0.5) *
        (1.0 + kUnmappedBonus * (1.0 - weight));
    return std::isfinite(score) ? score : 0.0;
}
// [World Shadow pick] A PICKED light is scored as world-owned: no native map and no native fade, whether or not its cube has rendered yet,
// so a newly picked native projector is not penalised against picks that already own a cube (the arbitration between picks must not depend
// on which of them happens to be resident). Only the ranking among picks uses it; a channel's own ranking still scores the real state.
inline double pickedImportance(double luminance, double range, double distance)
{
    return importance(luminance, range, distance, 0.0);
}
template<class ID> struct Candidate
{
    ID id{};
    bool shadowMap = false;
    double score = 0.0;
    // P1-lite: this light is RESIDENT in a world cube map (pinned for the session). A resident light always gets a channel
    // while at least one light is admitted: it holds its place by residency, never by score. false for every light when the
    // world maps are off, which is exactly the ordering before P1-lite (alrelightmath_test.cpp pins the permutation identity).
    bool world = false;
};
// Resident (world) lights first, then descending importance, then the stable UUID. shadowMap is reported, never ranked.
template<class ID> inline bool before(const Candidate<ID>& a, const Candidate<ID>& b)
{
    if (a.world != b.world) return a.world;
    if (a.score != b.score) return a.score > b.score;
    // Object UUID in production, never collector order or drawable address.
    // One candidate per emitting object; area channels do not rank faces.
    return a.id < b.id;
}
template<class ID> struct Selection
{
    std::array<Candidate<ID>, 8> lights{};
    unsigned size = 0, candidates = 0;
    void add(const Candidate<ID>& light, unsigned cap)
    {
        add(light, cap, [](const Candidate<ID>& a, const Candidate<ID>& b) { return before(a, b); });
    }
    // The comparator parameter exists so tests can prove their fixtures reject a
    // superseded ordering; production always uses before().
    template<class Before> void add(const Candidate<ID>& light, unsigned cap, Before ranks_first)
    {
        cap = std::clamp(cap, 1u, 8u);
        ++candidates;
        unsigned at = 0;
        while (at < size && !ranks_first(light, lights[at])) ++at;
        if (at >= cap) return;
        const unsigned last = std::min(size, cap - 1);
        for (unsigned i = last; i > at; --i) lights[i] = lights[i-1];
        lights[at] = light;
        size = std::min(size + 1, cap);
    }
};
// [World Shadow pick] The same ordering as Selection, but it keeps EVERY candidate up to N (Selection keeps the best 8): the picks are ranked
// completely (up to the pick cap), so a pick outside the first 8 still has a rank and "not in the list" can only mean "not a candidate".
template<class ID, unsigned N> struct Ranking
{
    std::array<Candidate<ID>, N> lights{};
    unsigned size = 0, candidates = 0;
    void add(const Candidate<ID>& light)
    {
        ++candidates;
        unsigned at = 0;
        while (at < size && !before(light, lights[at])) ++at;
        if (at >= N) return;
        const unsigned last = std::min(size, N - 1);
        for (unsigned i = last; i > at; --i) lights[i] = lights[i-1];
        lights[at] = light;
        size = std::min(size + 1, N);
    }
};
// A slot's native SL map counts as mapped only while it is really sampled: shadows above
// "projector shadows off" (detail > 1), the slot inside the live slot limit, and its map
// rendered this frame and complete. Otherwise (a stale owner left after detail 2 -> 1 -> 0,
// a slot above a lowered limit, a failed/skipped render) the light is unmapped: contact fade
// 0, full soft-shadow trace. Pure so the policy is testable; the pipeline supplies the inputs.
inline bool nativeMapSampled(int shadowDetail, unsigned slot, unsigned slotLimit,
    bool complete, bool renderedThisFrame)
{
    return shadowDetail > 1 && slot < slotLimit && complete && renderedThisFrame;
}
// The native spot sampler (shadowUtil.glsl sampleSpotShadow) as a pure function, for the
// tests' independent oracles: given the map's own PCF result, it returns lit beyond
// shadow_clip.w, and otherwise (pcf*W + add)/W with add = max(2u-1,0),
// u = (depth-clip.z)/(clip.w-clip.z), and W the cascade-blend weight
// 1 - max(clip.z*(1+blend) - depth,0)/(2*blend*clip.z). The caller adds shadow_fade and clamps.
inline double nativeSpotShadow(double pcf, double depth, double clipZ, double clipW, double blend = 0.25)
{
    if (!(depth < clipW)) return 1.0;
    const double u = (depth - clipZ) / (clipW - clipZ);
    const double add = std::max(2.0 * u - 1.0, 0.0);
    const double weight = 1.0 - std::max(clipZ * (1.0 + blend) - depth, 0.0) / (2.0 * blend * clipZ);
    return std::abs(weight) > 1.e-9 ? (pcf * weight + add) / weight : pcf;
}
// Native hand-over (the weight of the NATIVE map in a mapped light's screen visibility).
// A mapped light's screen term is contact-only while its native map is valid; past the
// map it must become the full trace. The native sampler itself ramps TO LIT over the
// last half of [clip.z, clip.w] (u from .5 to 1), so the hand-over must be finished
// before that ramp starts, or the two meet mid-way and their final min() lets light
// through a shadow both systems see (the reported bright band). Here the screen term
// takes over across u in [kHandoverStart, kHandoverEnd] = [.25, .45]: 1 (native valid) at
// the start, 0 (full trace) at the end. Inside it the native value is still its own
// shadow (the ramp has not begun) and beyond it the screen visibility is the full trace,
// so for a blocker both systems see, every term of min(native, screen) is dark all the way.
// Unset or malformed clip values (the GL default 0,0) mean the native map is valid everywhere.
// The window ends at .45, not .5 (the ramp's start): the bilateral filter (radius up to 8 px)
// blends a few percent of the lit side across a depth step, so strength reaching 0 exactly at the ramp
// start leaked a few /255. The shader (areaNativeStrength in areaShadowF.glsl) hard-codes the same
// window: keep kHandoverStart/kHandoverEnd and its (u-0.25)*5.0 in sync (relight_rank_check.py pins both).
constexpr double kHandoverStart = 0.25, kHandoverEnd = 0.45;
inline double nativeShadowStrength(double viewDepth, double clipZ, double clipW)
{
    if (!(clipZ > 0.0) || !(clipW > clipZ) || !std::isfinite(viewDepth)) return 1.0;
    const double u = (viewDepth - clipZ) / (clipW - clipZ);
    return 1.0 - std::clamp((u - kHandoverStart) / (kHandoverEnd - kHandoverStart), 0.0, 1.0);
}
// The depth the native sampler itself compares: sampleSpotShadow first moves the receiver
// along its normal, pos += norm * spot_shadow_offset, and ramps to lit by THAT depth. The
// hand-over must be evaluated at the same shifted depth or a tilted receiver (the offset is
// up to 0.15 m) meets the native ramp mid-way and lets light through a shared blocker.
// viewZ is the view-space z of the receiver (negative in front of the camera), normalZ the z
// of its unit view-space normal; the result is the positive depth the sampler compares.
inline double nativeReceiverDepth(double viewZ, double normalZ, double offset)
{
    const double shifted = viewZ + (std::isfinite(normalZ) && std::isfinite(offset) ? normalZ * offset : 0.0);
    return -shifted;
}
// Housing of the emitting prim is NOT modelled per light. The gather ignores occluders within
// the global light bias (RenderAreaShadowLightBias, "Light bias" on the Soft shadows card) of the
// light centre and stops each ray floor + bias short of its target, exactly as before this
// change (floor = max(source radius, 0.1 m); a rig source radius is counted once, there).
// Per-light housing shapes (bounding sphere, oriented box, capped box) were tried and each one
// erased real blockers or left a visible seam; the cost is a documented limitation: a LARGE
// emitting prim (a lit room shell, a big softbox) can shadow its own light until the user raises
// Light bias. See RELIGHT_SL_INTEGRATION.md.
// Integer avalanche also used by the GLSL disk sampler. Seed by ID, never slot.
inline std::uint32_t hash(std::uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; return x ^ (x >> 16);
}
// nearSteps > 0 marks the opt-in Thin objects march: that many dense steps over the
// first 1.85 m, the rest far. minFar is the authored uniform count. Under budget
// pressure the far segment may be thinned to min(minFar, kThinFarFloor) before a view
// falls back to the uniform march; the floor is COMMON to every preset.
struct Quality { int samples, steps; bool half; int nearSteps = 0; int minFar = 0; float screenReach = 0.f; };
inline float lightBias(float bias)
{
    return std::isfinite(bias) ? std::clamp(bias,0.f,4.f) : .25f;
}
// ---------------------------------------------------------------------------------------------------- soft shadow LOOK
// Five LOOK settings (RenderAreaShadowStrength / Contrast / WorldScreenReach / WorldAmount / ScreenAmount). Every one defaults to
// "no change", and the shaders then run byte-for-byte today's arithmetic (branches, never a multiply by one). The uniforms are
// DEVIATIONS from identity so a program that was never given them (a harness, a reload) is the identity too. KEEP IN SYNC with
// areaShadowF.glsl (the world channel's reach march and extra term) and areaShadowFilterF.glsl (area_look); relight_shader_check.py pins both.
inline float lookStrength(float s) { return std::isfinite(s) ? std::clamp(s,0.f,1.f) : 1.f; }
inline float lookContrast(float c) { return std::isfinite(c) ? std::clamp(c,.25f,4.f) : 1.f; }
inline float lookReach(float r)    { return std::isfinite(r) ? std::clamp(r,0.f,8.f) : 0.f; }
inline float lookAmount(float a)   { return std::isfinite(a) ? std::clamp(a,0.f,1.f) : 1.f; }
// area_look = (1 - strength, contrast - 1, 1 - world amount): all zero = identity.
inline void lookUniform(float strength, float contrast, float worldAmount, float out[3])
{
    out[0] = 1.f-lookStrength(strength);
    out[1] = lookContrast(contrast)-1.f;
    out[2] = 1.f-lookAmount(worldAmount);
}
// GLSL mix(x, y, a) = x * (1 - a) + y * a, in float.
inline float mixf(float x, float y, float a) { return x*(1.f-a)+y*a; }
// The world channel's screen-space trace distance beyond the contact band: 0 = the contact march alone (today). Capped by Trace distance.
inline float reachLimit(float reach, float contact, float traceDistance)
{
    const float r = lookReach(reach);
    return r > contact ? std::min(traceDistance,r) : 0.f;
}
// Steps of the reach march: 12 (or the authored count if lower) at no reach, growing linearly to the authored count at 8 m.
inline int reachSteps(int authored, float limitMetres)
{
    const int base = std::min(authored,12);
    const int more = std::max(authored-12,0);
    return base+static_cast<int>(std::ceil(static_cast<float>(more)*std::clamp(limitMetres*.125f,0.f,1.f)));
}
// Conservative per-ray work for a group that may contain world-owned channels.
// Those channels run contact AND reach; other channels retain their original march.
// Reserve the larger cost for every requested light so changing ownership cannot
// break a frame/draw ceiling. Reach zero retains the original integer cost exactly.
inline int marchWork(const Quality& q)
{
    const float reach = lookReach(q.screenReach);
    return reach > 0.f ? std::max(q.steps,std::min(q.steps,12)+reachSteps(q.steps,reach)) : q.steps;
}
// Final visibility of one channel: contrast first (pow, skipped at exactly 1), then strength (mix(1, v, s), skipped at exactly 1).
inline float lookApply(float v, const float look[3])
{
    if (look[1] != 0.f) v = std::pow(std::max(v,0.f),1.f+look[1]);
    if (look[0] != 0.f) v = mixf(1.f,v,1.f-look[0]);
    return v;
}
// The world term, weakened by its amount: mix(1, world, amount).
inline float worldAmountApply(float world, const float look[3])
{
    return look[2] != 0.f ? mixf(1.f,world,1.f-look[2]) : world;
}
// A world channel's screen term: the contact band (always full) minus the extra darkening of the reach march scaled by Screen amount.
// contactAvg and extraDark are fractions of the rays (a ray is counted in exactly one of: contact hit, extra hit, none).
inline float screenTerm(float contactAvg, float extraDark, float screenAmount)
{
    return std::max(contactAvg-screenAmount*extraDark,0.f);
}
inline Quality quality(int preset, bool thin = false)
{
    constexpr Quality presets[] = {{4,8,true}, {8,12,true}, {12,20,true}, {24,32,false}};
    auto q = presets[std::clamp(preset,0,3)];
    // Opt-in density: 48 near (0.02..min(1.85m,limit)) + 32 far steps.
    if (thin) { q.minFar=q.steps; q.steps=80; q.nearSteps=48; }
    return q;
}
inline Quality segmented(Quality q, int preset)
{
    const auto minimum = quality(preset,true);
    q.nearSteps = minimum.nearSteps;
    const int far_steps = std::clamp(q.steps,1,64);
    // Existing saved preset values get the new far density too. Custom keeps
    // its authored far count. Persisted uniform counts remain unchanged.
    q.steps = (preset>=0 && preset<4 ? std::max(far_steps,minimum.steps-minimum.nearSteps):far_steps)+q.nearSteps;
    q.minFar = far_steps; // The authored uniform count: OFF's step density.
    return q;
}
// Runtime work admission. Integer arithmetic and fixed degradation order; no timing input.
// The setting can lower the safety ceiling, never remove it.
struct Workload
{
    Quality q;
    unsigned channels = 0;
    std::uint64_t pixels = 0, iterations = 0, budget = 0, drawCap = 0;
    // A: lights that keep a soft-shadow channel (see the ladder below). A is planned from
    // the requested cap, never the visible set; iterations count min(visible, A) lights.
    // 0 whenever nothing draws (channels == 0).
    unsigned admitted = 0;
    // Thin objects hybrid. The `lead` highest ranked admitted lights march with q; the other
    // admitted lights (admitted - lead) march with `rest` (the regular, Thin-OFF march) in draws of
    // restChannels lights. Both share one resolution (q.half == rest.half). Every plan that is
    // not a hybrid has lead == admitted and no rest lights.
    unsigned lead = 0;
    Quality rest{};
    unsigned restChannels = 0;
};
// Capture-feed slots sharing the frame budget (LLPrismLens::MAX_CAPTURES; asserted in
// pipeline.cpp).
constexpr unsigned kFeedSlots = 8;
// The capture feeds that can run an area-shadow pass THIS frame (fix rounds 4-5): how many, each one's identity (the Prism
// registry slot) and its render target size for the RESERVATION. Built from the current state only (the pipeline reads it from the
// Prism registry once per frame, frame-stamped, and every pass of that frame reuses the same set), never from history.
// The extent is the feed's FIXED capacity target (LLPrismLens::cameraTargetSize at the slot's output capacity and its aspect), never
// the per-frame target, which PrismAdaptivePerformance scales with the presented FPS (fix round 5): the main plan must not depend on
// timing. The real target of a pass is always <= its capacity, so the reservation stays conservative. A plain slot count means that
// many feeds of the largest target (2048 x 2048), the worst case every earlier round planned for.
struct FeedSet
{
    struct Extent { unsigned w = 0, h = 0; };
    unsigned count = 0;
    std::array<Extent,kFeedSlots> extent{};
    std::array<unsigned,kFeedSlots> id{};
    FeedSet() = default;
    FeedSet(unsigned slots) : count(std::min(slots,kFeedSlots))
    {
        for (unsigned i = 0; i < count; ++i) { extent[i] = {2048u,2048u}; id[i] = i; }
    }
    void add(unsigned width, unsigned height) { add(width,height,count); }
    void add(unsigned width, unsigned height, unsigned identity)
    {
        if (count < kFeedSlots && width && height) { extent[count] = {width,height}; id[count] = identity; ++count; }
    }
};
// Segmented plans thin their far segment to at most this many steps (Low's uniform
// far count, 8): every preset shares one floor, so no preset loses the segmented march
// while a lower one keeps it, and the floor is never coarser than OFF Low.
constexpr int kThinFarFloor = 8;
// A feed target is at most 2048x2048 (largest camera capacity); its half-resolution
// pixel count is what the feed guarantee below is sized for.
constexpr std::uint64_t kFeedGuaranteePixels = 1024ull*1024ull;
// Segmented plans keep full resolution only while this many rays per pixel fit
// (the High preset's request), so Cinematic never ends below High in rays per pixel.
constexpr int kFullResMinSamples = 12;

// Ray floor (user decision 10/22): a soft-shadow penumbra estimated from fewer than 4 rays
// per pixel is grain, so under budget pressure the LIGHT COUNT falls before the rays do. The
// lights that lose their soft shadow are the lowest ranked; the status line reports the coverage.
constexpr int kRayFloor = 4;
// Rung 2 of the ladder keeps this many rays before any steps are given up.
constexpr int kRayRungSamples = 8;

// Planner ladder. A pure, integer function of (preset quality, width, height, requested cap K,
// budget, feed hold-back): no timing, frame or visible-set input. A fixed order of rungs; the
// first plan that fits at A = K lights wins:
//   1 resolution  full resolution only while kFullResMinSamples rays still fit, else half;
//   2 rays        down to min(requested, 8);
//   3 steps       down to max(8, steps / 2)  (Thin objects: the far segment, same rule);
//   4 rays        down to min(requested, kRayFloor);
//   5 steps       down to 8 (Thin objects: the common far floor; near steps never thin);
//   6 lights      MAIN view only (floors = true): A falls one at a time down to 1 (the lowest ranked lose soft shadows).
// Below that (one light at the floors still does not fit, or its single draw exceeds the per-draw
// cap) the MAIN view draws nothing: A = 0, never a sub-floor ray or step count, so
// more budget can never mean fewer lights. A Thin objects request that reaches that point is planned
// by hybridWithin() (below): Thin never admits fewer lights than the regular plan. A capture FEED
// (floors = false, fix round 5) has NO rung 6: when all K lights do not fit at the floors it keeps HEAD's fixed-K fallback
// unchanged, from the requested plan: rays and steps fall to 1 for all K lights, then fail closed. A feed therefore admits all
// K lights or none, never a count in between, and its admission cannot fall when its allowance rises. The feed guarantee
// below depends on that fallback.
inline Workload workloadWithin(Quality q, unsigned width, unsigned height, unsigned lights,
    std::uint64_t budget, unsigned requestedLights, std::uint64_t feedHoldBack = 0, bool floors = true)
{
    Workload w{};
    const int dense = std::clamp(q.nearSteps,0,64);
    w.q = {std::clamp(q.samples,1,32),std::clamp(q.steps,1,dense ? 128:64),q.half,dense,
        dense ? std::clamp(q.minFar,1,64):0,lookReach(q.screenReach)};
    const Quality asked = w.q;
    lights = std::min(lights,8u);
    // Reserve for the requested cap, never the changing visible set. At least
    // the actual count keeps malformed callers within the hard frame ceiling.
    const unsigned basis = std::max(lights,std::clamp(requestedLights,1u,8u));
    unsigned admitted = basis;
    w.budget = std::min(budget,std::uint64_t(1500000000));
    w.drawCap = w.budget/2;
    // Planning limit: the nominal budget minus the feed reservation (segmented main only).
    const std::uint64_t planned = dense ? w.budget-std::min(feedHoldBack,w.budget) : w.budget;
    const auto update = [&]()
    {
        w.pixels = w.q.half ? ((std::uint64_t(width)+1)/2)*((std::uint64_t(height)+1)/2) :
            std::uint64_t(width)*height;
        w.iterations = w.pixels*std::min(lights,admitted)*w.q.samples*marchWork(w.q);
    };
    // Render-target dimensions are GL-sized; bound pathological pure-function inputs too.
    if (width > 65536 || height > 65536) return w;
    update();
    const auto drawn = [&]() { return w.pixels*w.q.samples*marchWork(w.q)<=w.drawCap; };
    const auto over = [&]() { return w.pixels*admitted*w.q.samples*marchWork(w.q)>planned || !drawn(); };
    const auto finish = [&]()
    {
        const auto perChannel = w.pixels*w.q.samples*marchWork(w.q);
        w.channels = lights && perChannel ? unsigned(std::min<std::uint64_t>(4,w.drawCap/perChannel)) : 0;
        // Nothing draws (no light, or a draw above the per-draw cap): no light is admitted either.
        w.admitted = w.channels ? admitted : 0;
        w.lead = w.admitted;
        return w;
    };
    // Segmented plans choose half resolution unless 12 full-res rays (High's request) fit.
    const auto resolution = [&]()
    {
        if (!w.q.half)
        {
            const std::uint64_t rays = std::min(w.q.samples,kFullResMinSamples);
            if (w.pixels*basis*rays*marchWork(w.q)>planned || w.pixels*rays*marchWork(w.q)>w.drawCap)
            { w.q.half=true; update(); }
        }
    };
    // Rung 1. Uniform plans too: Cinematic keeps its full resolution only while 12 rays fit.
    resolution();
    const int farMin = dense ? std::max(1,std::min(w.q.minFar,kThinFarFloor)) :
        std::min(w.q.steps,kThinFarFloor);
    const int thin = w.q.steps-dense; // the part of the march a step rung may thin
    const int halfSteps = std::min(thin,std::max(farMin,thin/2));
    const auto rays = [&](int floor) { while (over() && w.q.samples>floor) { --w.q.samples; update(); } };
    const auto steps = [&](int floor) { while (over() && w.q.steps-dense>floor) { --w.q.steps; update(); } };
    rays(std::min(w.q.samples,kRayRungSamples));
    steps(halfSteps);
    rays(std::min(w.q.samples,kRayFloor));
    steps(std::min(thin,farMin));
    if (!over()) return finish();
    // Rung 6 (main view only; a feed keeps HEAD's fixed-K fallback below). Fewer lights, never fewer rays. Only when the
    // per-draw cap is already met: a light count cannot cure an oversized single draw.
    if (floors && drawn())
    {
        while (over() && admitted>1) { --admitted; update(); }
        if (!over()) return finish();
    }
    // Below the floors. The main view admits no light rather than restore sub-floor rays (the user's
    // "fewer lights, smooth" decision); the coverage readout reports it.
    if (floors) { w.iterations = 0; w.admitted = 0; w.channels = 0; return w; }
    // A capture feed: the previous planner from the requested plan, for all K lights.
    admitted = basis;
    w.q = asked;
    update();
    if (dense) resolution();
    while (over() && w.q.samples>1) { --w.q.samples; update(); }
    // Segmented marches preserve their steps until nothing else is left.
    while (over() && !dense && w.q.steps>1) { --w.q.steps; update(); }
    if (over() && !w.q.half) { w.q.half=true; update(); }
    // Last resort for a segmented plan: thin only the far segment, down to the common
    // floor min(authored count, kThinFarFloor), never coarser than OFF Low; near steps stay.
    const int farFloor = dense ? std::min(w.q.minFar,kThinFarFloor) : 0;
    while (over() && dense && w.q.steps-dense>farFloor) { --w.q.steps; update(); }
    // Even one channel/one step cannot fit: skip this view, never exceed the cap.
    if (over()) { w.iterations=0; return w; }
    return finish();
}

// Feed guarantee. A uniform (OFF) feed plan draws if and only if its allowance is at
// least pixels(half) * max(requested lights, 2): one ray, one step, half resolution.
// That is the most any feed up to 2048x2048 can need to draw at all. A segmented main
// plan holds back the sum over the LIVE feeds of min(OFF feed allowance, that feed's need), so
// with Thin ON every feed gets at least min(its Thin OFF allowance, need) and still draws
// wherever OFF draws. The OFF feed allowance is (budget - OFF main reservation) / live feeds.
// The bound is preset independent: every OFF main plan that draws costs at least the cheapest
// one (1 ray, 1 step, half resolution), so allowance(OFF) <= (budget - that) / live feeds for
// every preset, and segmented admission does not depend on the preset either. (This is the
// floor-level guarantee; the exact one, every feed keeping its full Thin-OFF allowance, is
// feedSpend() below.)
inline std::uint64_t feedReservation(unsigned mainWidth, unsigned mainHeight, std::uint64_t budget,
    unsigned requestedLights, const FeedSet& feeds)
{
    if (!mainWidth || !mainHeight || !feeds.count) return 0;
    budget = std::min(budget,std::uint64_t(1500000000));
    const auto cheapest = workloadWithin({1,1,true},mainWidth,mainHeight,requestedLights,budget,requestedLights);
    const auto reserve = cheapest.channels ? cheapest.iterations : 0;
    const std::uint64_t allowance = (budget-reserve)/feeds.count;
    std::uint64_t sum = 0;
    for (unsigned i = 0; i < feeds.count; ++i)
    {
        // What THIS feed needs to draw at all: one ray, one step, half resolution, for max(K, 2) lights.
        const std::uint64_t half = std::min(((std::uint64_t(feeds.extent[i].w)+1)/2)*((std::uint64_t(feeds.extent[i].h)+1)/2),
            kFeedGuaranteePixels);
        sum += std::min(allowance,half*std::max(std::clamp(requestedLights,1u,8u),2u));
    }
    return sum;
}
// The most a live feed's plan can ever cost (fix round 4): its target at the requested march's resolution, for K lights, at the
// requested rays and steps. Every plan a feed can get (pure Thin, the regular fallback, a hybrid) only ever lowers these, and a
// feed plans for at most the requested cap K, so a feed never spends more than this however large its allowance is.
inline std::uint64_t feedSpendBound(const Quality& request, unsigned width, unsigned height, unsigned requestedLights)
{
    const int dense = std::clamp(request.nearSteps,0,64);
    const std::uint64_t samples = std::clamp(request.samples,1,32), steps = std::clamp(request.steps,1,dense ? 128:64);
    const std::uint64_t pixels = request.half ? ((std::uint64_t(width)+1)/2)*((std::uint64_t(height)+1)/2) :
        std::uint64_t(width)*height;
    return pixels*std::clamp(requestedLights,1u,8u)*samples*marchWork({int(samples),int(steps),request.half,dense,request.minFar,request.screenReach});
}
// What the live feeds can spend in one frame when each gets its Thin-OFF allowance (budget - the Thin-OFF main plan's cost) / count.
// A feed spends at most min(allowance, its bound); when EVERY feed is bound by its allowance the whole pool
// (budget - regularMain) is spent (the integer remainder of the division included). The Thin or hybrid main plan may use
// everything else: with no live feed it is the whole budget, with a few small feeds most of it.
inline std::uint64_t feedSpend(const Quality& request, std::uint64_t budget, std::uint64_t regularMain,
    unsigned requestedLights, const FeedSet& feeds)
{
    if (!feeds.count) return 0;
    budget = std::min(budget,std::uint64_t(1500000000));
    const std::uint64_t pool = budget-std::min(regularMain,budget), allowance = pool/feeds.count;
    std::uint64_t sum = 0;
    bool saturated = true;
    for (unsigned i = 0; i < feeds.count; ++i)
    {
        const std::uint64_t bound = feedSpendBound(request,feeds.extent[i].w,feeds.extent[i].h,requestedLights);
        if (bound < allowance) { saturated = false; sum += bound; } else sum += allowance;
    }
    return saturated ? pool : sum;
}

// The OFF main plan (nearSteps == 0) is the regular plan. It is NOT the pre-P0 plan: P0.1 applies the resolution rung and the
// ray/step floors to uniform plans too, by design (user decision 10/22: "P0 may change the default look"). A segmented
// plan is admitted only inside budget - feedReservation(); else the caller falls back.
inline Workload workload(Quality q, unsigned width, unsigned height, unsigned lights,
    int millions, unsigned requestedLights, const FeedSet& feeds = FeedSet(kFeedSlots), bool floors = true)
{
    const std::uint64_t budget = std::uint64_t(std::clamp(millions,1,1500))*1000000;
    return workloadWithin(q,width,height,lights,budget,requestedLights,
        q.nearSteps > 0 ? feedReservation(width,height,budget,requestedLights,feeds) : 0,floors);
}

// Thin objects hybrid (user decision 10/22; fix rounds 3-4). Thin objects must NEVER remove a shadow that Thin OFF would draw, and the
// lights that cannot get the Thin march still get the REGULAR (Thin-OFF) march, exactly as Thin OFF would march them:
//   A (admitted lights) = max(A_thin, A_off), where A_thin is the pure Thin plan's admission (0 when it does not fit) and
//   A_off the Thin-OFF plan's at the same budget. Both are monotone in the budget, so A is too.
//   * A_thin >= A_off and the Thin plan draws: the Thin plan itself (every admitted light marches Thin).
//   * otherwise A = A_off and the hybrid: the regular group (the A - nT lowest ranked admitted lights) keeps EXACTLY the Thin-OFF
//     plan's per-light quality (rays, steps and resolution: it is the OFF plan, never degraded to make room), and the nT Thin
//     lights (the highest ranked) get only what the budget leaves over: the largest nT that fits is used, nT = 0 (no Thin light
//     at all, the Coverage row says "0 thin-quality") is exactly the Thin-OFF plan. The Thin lights share the OFF plan's
//     resolution AND ITS RAY COUNT (fix round 4: the two groups' disk strata must match so that a Thin light that ties the first
//     regular light marches the regular light's estimator exactly; the Thin lights degrade only in their far steps, down to the
//     common floor). If the OFF ray count leaves no room for a Thin light, fewer Thin lights are used, down to none.
// While VCam feeds are LIVE the Thin or hybrid main plan also holds back what those feeds can spend (fix round 4, feedSpend()):
// every live feed keeps its Thin-OFF allowance (budget - the Thin-OFF main plan's cost) / count, exactly as with Thin OFF, so a feed
// admits at least the lights it admits with Thin OFF (there is no feed exception), and the main plan may use the rest. A feed cannot
// spend more than its own bound (feedSpendBound: target x K x requested rays x steps), so a few small feeds leave most of the
// budget to the main view; with no live feed the main view has the whole budget. Turning a feed on therefore CAN lower the main
// view's Thin quality (the reservation grows with each live feed); it never lowers the number of lights the main view shadows.
// The cost the planner charges a regular (Thin-OFF) plan: admitted lights x its per-light march (planning basis, never the visible
// set). This is the SAME quantity feedWorkload() reserves for the main plan (the OFF main plan planned for the requested K lights).
inline std::uint64_t regularCost(const Workload& w)
{
    return w.channels ? w.pixels*w.admitted*w.q.samples*marchWork(w.q) : 0;
}
// What any plan reserves from the frame budget: every admitted light at its group's march (the lead lights q, the rest lights
// `rest`), whatever the visible set. Equal to regularCost() for a plan that is not a hybrid.
inline std::uint64_t basisCost(const Workload& w)
{
    if (!w.channels) return 0;
    const std::uint64_t lead = std::min(w.lead,w.admitted);
    // A hybrid's Thin lights may also march the regular step count (fix round 5): they are charged the larger of the two.
    return w.pixels*(lead*w.q.samples*std::max(marchWork(w.q),marchWork(w.rest))+
        (w.admitted-lead)*std::uint64_t(w.rest.samples)*marchWork(w.rest));
}
inline bool hybridPlan(Workload& out, Quality thin, const Workload& off, unsigned width, unsigned height,
    unsigned lights, unsigned thinLights, std::uint64_t planned)
{
    const unsigned admitted = off.admitted;
    if (!off.channels || !thinLights || thinLights > admitted || width > 65536 || height > 65536) return false;
    const int dense = std::clamp(thin.nearSteps,0,64);
    if (!dense) return false;
    // The Thin lights march EXACTLY the regular ray count (same disk strata, same resolution); only their steps differ.
    Quality t{off.q.samples,std::clamp(thin.steps,1,128),off.q.half,dense,std::clamp(thin.minFar,1,64),lookReach(thin.screenReach)};
    const bool half = off.q.half;
    const std::uint64_t px = half ? ((std::uint64_t(width)+1)/2)*((std::uint64_t(height)+1)/2) :
        std::uint64_t(width)*height;
    const std::uint64_t nT = thinLights, nR = admitted-thinLights;
    const std::uint64_t perRest = px*std::uint64_t(off.q.samples)*marchWork(off.q);
    if (!perRest) return false;
    // The regular group is the Thin-OFF plan, unchanged; the Thin lights get what is left.
    const std::uint64_t restCost = nR*perRest;
    if (restCost > planned) return false;
    const std::uint64_t room = planned-restCost, drawCap = off.drawCap;
    // Fix round 5 (DeepSeek P1): a Thin light near the boundary also marches regular rays, with the regular (Thin-OFF) step count
    // `restSteps` (the shader no longer clamps it to the Thin draw's steps). Where that count exceeds the Thin march's, the cost of
    // the light is the larger of the two: every Thin light is charged samples x max(Thin steps, regular steps), in perThin, the
    // channel count, iterations and basisCost() alike.
    const int restSteps = std::clamp(off.q.steps,1,64);
    const auto perThin = [&]() { return px*std::uint64_t(t.samples)*std::max(marchWork(t),marchWork(off.q)); };
    const auto over = [&]() { return nT*perThin() > room || perThin() > drawCap; };
    // Far steps only: down to the common floor (near steps never thin).
    const int thinPart = t.steps-dense;
    const int farMin = std::max(1,std::min(t.minFar,kThinFarFloor));
    // (Once the Thin steps are no more than the regular count, thinning them further saves nothing: perThin is the larger.)
    while (over() && t.steps>restSteps && t.steps-dense>std::min(thinPart,farMin)) --t.steps;
    if (over()) return false;
    out = Workload{};
    out.q = t;
    out.rest = off.q;
    out.rest.half = half;
    out.pixels = px;
    out.budget = off.budget;
    out.drawCap = drawCap;
    out.admitted = admitted;
    out.lead = thinLights;
    out.channels = unsigned(std::min<std::uint64_t>(4,drawCap/perThin()));
    out.restChannels = off.channels;
    const std::uint64_t leadLights = std::min<std::uint64_t>(lights,thinLights);
    const std::uint64_t drawnLights = std::min<std::uint64_t>(lights,admitted);
    out.iterations = px*(leadLights*std::uint64_t(t.samples)*std::max(marchWork(t),marchWork(off.q))+
        (drawnLights-leadLights)*std::uint64_t(off.q.samples)*marchWork(off.q));
    return true;
}

// The Thin objects plan: pure Thin, the regular (Thin-OFF) plan, or the hybrid of the two (see above).
// `uniform` is the authored regular march; floors = false is a capture feed (the previous sub-floor behaviour of the
// two component plans is kept, and the hybrid only forms inside the floors). `feeds` are the feeds LIVE this frame: the plan
// holds back what they can spend (feedSpend), on top of feedHoldBack (the floor-level guarantee, feedReservation).
inline Workload hybridWithin(Quality thin, Quality uniform, unsigned width, unsigned height, unsigned lights,
    std::uint64_t budget, unsigned requestedLights, std::uint64_t feedHoldBack, bool floors, const FeedSet& feeds = FeedSet())
{
    const auto off = workloadWithin(uniform,width,height,lights,budget,requestedLights,0,floors);
    const std::uint64_t capped = std::min(budget,std::uint64_t(1500000000));
    const std::uint64_t hold = std::max(std::min(feedHoldBack,capped),
        feedSpend(thin,capped,regularCost(off),requestedLights,feeds));
    const std::uint64_t planned = capped-hold;
    // The pure Thin plan plans against the same limit (workloadWithin holds back `hold` of its budget).
    const auto pure = workloadWithin(thin,width,height,lights,budget,requestedLights,hold,floors);
    if (pure.channels && pure.admitted >= off.admitted) return pure;
    if (!off.channels) return pure.channels ? pure : off;
    Workload hybrid{};
    for (unsigned thinLights = off.admitted; thinLights >= 1; --thinLights)
        if (hybridPlan(hybrid,thin,off,width,height,lights,thinLights,planned)) return hybrid;
    return off;
}

// The OFF main plan (nearSteps == 0) is the regular plan (see workload()). A segmented (Thin objects) request is planned by
// hybridWithin() inside budget - feedReservation() and, while feeds are live, inside what they leave (feedSpend).
inline Workload workloadOrUniform(Quality q, Quality uniform, unsigned width, unsigned height,
    unsigned lights, int millions, unsigned requestedLights, const FeedSet& feeds = FeedSet(kFeedSlots), bool floors = true)
{
    if (!(q.nearSteps > 0)) return workload(q,width,height,lights,millions,requestedLights,feeds,floors);
    const std::uint64_t budget = std::uint64_t(std::clamp(millions,1,1500))*1000000;
    return hybridWithin(q,uniform,width,height,lights,budget,requestedLights,
        feedReservation(width,height,budget,requestedLights,feeds),floors,feeds);
}

// Bilateral blur radius, in TRACE pixels. The user's Denoise radius scales with the target
// height and halves at half resolution (a trace pixel is then two screen pixels). P0.2: whenever
// the blur is on at all it spans at least kMinFilterRadius trace pixels, so the Gaussian
// (sigma = r / sqrt 2) reaches past the correlation length of the R2 noise; zero still skips it.
// This is a deliberate STEP at the lowest setting: a Denoise value that scales to a radius between 0 and
// the floor reads as the floor (a small setting is never "a little blur" of an unfiltered noise pattern),
// and only an exact 0 turns the blur off. The Director help says so.
constexpr float kMinFilterRadius = 2.f;
inline float filterRadius(float setting, float resolutionScale, bool half)
{
    if (!std::isfinite(setting) || !std::isfinite(resolutionScale)) return 2.f;
    const float scaled = std::clamp(setting*resolutionScale*(half ? .5f:1.f),0.f,8.f);
    return scaled > 0.f ? std::clamp(scaled,kMinFilterRadius,8.f) : 0.f;
}

// Smooth group fade (user decision 10/22). While lights are dropped for want of budget, every ADMITTED light i keeps its soft
// shadow at strength smoothstep(1, 1.05, score[i] / score[first dropped]): 1 for a light more than 5 percent more important than the
// first dropped one, falling to 0 as the two become equally important (lead decision 10/22: the window was 25 percent, which faded
// a whole cluster of near-equal lamps almost out; 5 percent only touches the lights right at the threshold), so a soft shadow fades instead of vanishing when the camera
// moves and the ranks change at the coverage boundary. The reference is an order statistic (the first dropped light's score), so
// swapping two lights WITHIN the admitted set changes no strength; and every term is this frame's score (frame-pure). It is 1
// when nothing is dropped, and when the first dropped light carries no importance (next <= 0): nothing to swap with.
constexpr double kAdmissionWindow = 0.05;
inline double admissionStrength(double score, double next)
{
    if (!(next > 0.0)) return 1.0;
    if (!(score > 0.0)) return 0.0;
    const double t = std::clamp((score/next-1.0)/kAdmissionWindow,0.0,1.0);
    return t*t*(3.0-2.0*t);
}
// Tied lamps (lead decision 10/22, on the user's behalf): importance is constant within range / 2 of the camera, so identical
// nearby lamps tie EXACTLY, and the raw fade above then takes every admitted light of the cluster to 0 ("No soft shadows right
// now"). An ADMITTED light is therefore never faded below half strength: strength = max(0.5, smoothstep(1, 1.05, ratio)).
// This accepts at most a half-strength pop at an exact boundary swap and keeps the strength continuous within the admitted set
// (the raw term is an order statistic; the floor is a constant). A light with no importance (score <= 0) stays gone.
constexpr double kAdmissionFloor = 0.5;
inline double admittedStrength(double score, double next)
{
    const double raw = admissionStrength(score,next);
    return (score > 0.0 && next > 0.0) ? std::max(kAdmissionFloor,raw) : raw;
}
// Thin/regular boundary (fix rounds 3-4): in a hybrid plan the Thin light i marches the fraction w of its rays the Thin way,
// w = max(0.5, smoothstep(1, 1.05, s_i / s_firstRegular)) (the floor mirrors the admission floor above: lead decision 10/23, round 4);
// the other rays of that light, its regular share 1 - w <= 0.5, march the regular (Thin-OFF) way, with the regular ray count and
// strata (hybridPlan). Within the Thin set nothing moves when two Thin lights swap (an order statistic again), a clear leader
// is all Thin (w = 1; so is a light when nothing regular follows or the first regular light has no importance). At a rank swap
// between the last Thin light and the first regular light the two lights trade a regular share of 0.5 for 0 (and back): the
// boundary pops by HALF of that light's rays, which the floor accepts, exactly like the admission floor at every admitted/dropped
// crossing (the 0.5 floor pops by 0.5 at ANY coverage-boundary crossing, not only at exact ties: a leaving light goes
// 0.5 -> 0). Without the floor an exact tie made every Thin light 100 percent regular while the Coverage row still said
// "thin-quality".
inline double thinWeight(double score, double firstRegular) { return admittedStrength(score,firstRegular); }
inline double regularShare(double score, double firstRegular) { return 1.0-thinWeight(score,firstRegular); }
// A Thin light counts as thin-quality (Coverage row) only while most of its rays march Thin: its regular share is below one half.
constexpr double kThinQualityShare = 0.5;
inline bool thinQuality(double share) { return share < kThinQualityShare; }
// A strength below one 8-bit step draws nothing visible: such a light is not counted as rendered.
constexpr double kAdmissionVisible = 1.0/255.0;
inline bool admissionVisible(double strength) { return strength >= kAdmissionVisible; }

// Director status line for the light coverage of the main view: wanted = lights the importance
// ranking selected (at most the requested cap), rendered = lights that really got a visible soft-shadow channel this
// frame (the planner ladder, the fade and allocation can lower it), thin = how many of those march Thin objects quality. thinRequested: the Thin objects switch is on, so "(0
// thin-quality)" is said when it fell back entirely (instead of reading like Thin OFF). lowMemory: the pass could not allocate
// its targets (an explicit flag from the pipeline, so an early return reads "low video memory", never "work budget").
// Pure so the wording is testable.
inline std::string coverageText(bool soft, unsigned rendered, unsigned wanted, unsigned thin = 0,
    bool thinRequested = false, bool lowMemory = false)
{
    if (!soft) return "Soft shadows are off";
    if (!wanted) return "No lights are selected for soft shadows";
    // "low video memory" only when the pass really failed to allocate (the pipeline's explicit flag): a plan that admits fewer
    // lights than wanted, or that drops a plane or a draw, is a work-budget effect, never a memory one.
    const std::string why = lowMemory ? "low video memory" : "work budget";
    const std::string noun = wanted == 1 ? " light" : " lights";
    const bool say = thin || (thinRequested && rendered);
    const std::string quality = std::to_string(thin) + " thin-quality";
    if (rendered >= wanted)
    {
        if (!say) return "Soft shadows on " + std::to_string(wanted) + noun;
        return "Soft shadows on " + std::to_string(rendered) + " of " + std::to_string(wanted) + noun + " (" + quality + ")";
    }
    if (!rendered) return "No soft shadows right now (" + why + ")";
    return "Soft shadows on " + std::to_string(rendered) + " of " + std::to_string(wanted) + " lights (" + why +
        (say ? ", " + quality : std::string()) + ")";
}

// Planes the pipeline can actually draw after target allocation. A failed allocation is
// retried on the next frame (nothing remembers it): plane 0 failing leaves no soft shadows
// this frame; plane 1 failing keeps the four most important lights (plane 0) instead of
// dropping everything. Pure so the policy is testable.
inline unsigned drawablePlanes(unsigned planesWanted, bool plane0Ok, bool plane1Ok)
{
    if (!planesWanted || !plane0Ok) return 0;
    return (planesWanted > 1 && !plane1Ok) ? 1 : std::min(planesWanted, 2u);
}
inline unsigned drawableChannels(unsigned selected, unsigned planes)
{
    return std::min(selected, planes * 4u);
}

// Frame identity is used only to reset workload accounting, never sampling. Feed passes are admitted by FEED IDENTITY (fix round 5),
// not by a bare count: a pass runs only for a feed that is in this frame's live set, and at most once per frame, so one source
// rendered twice (a Gate may re-render its source) can never use up the pass of another live feed and starve it, and the passes
// of a frame can never outnumber the live set the budget was divided by.
struct FeedPasses
{
    std::uint32_t frame = 0;
    unsigned served = 0; // bit i: the live feed at position i of the set has had its pass this frame
    bool admit(std::uint32_t currentFrame, unsigned identity, const FeedSet& live)
    {
        if (frame != currentFrame) { frame = currentFrame; served = 0; }
        for (unsigned i = 0; i < live.count; ++i)
        {
            if (live.id[i] != identity) continue;
            if (served & (1u << i)) return false;
            served |= 1u << i;
            return true;
        }
        return false;
    }
};

// Reserve the regular (Thin-OFF) main plan for the requested K, then divide only the
// remainder into fixed allowances, one per LIVE feed (fix round 4: `feeds` is the set of feeds live this frame, a plain count
// meaning that many 2048x2048 feeds). No dependence on cadence, timing, prior frames or the main view's changing selected set.
// A segmented (Thin objects) plan is the pure Thin plan, the authored uniform plan or the hybrid of the two (hybridWithin), so
// a feed never admits fewer lights than the OFF feed.
// The reservation is that of the regular (Thin-OFF) main plan, whatever Thin objects is set to: regularCost(main) is EXACTLY the
// quantity hybridWithin() subtracts from the budget for the feeds' pool (the Thin-OFF main plan planned for the requested K, the
// admitted lights at its per-light march; fix round 4 made the two the same expression), and a Thin or hybrid main plan
// holds back what the live feeds can spend of that pool (feedSpend). With Thin ON every feed therefore gets EXACTLY the
// allowance it gets with Thin OFF (there is no feed exception), the one frame budget still holds (basisCost(main) + the
// feeds' spend <= budget), and the feed admits at least the lights the OFF feed admits (hybridWithin at an equal allowance).
inline Workload feedWorkload(Quality q, Quality uniform, unsigned mainWidth, unsigned mainHeight,
    unsigned width, unsigned height, unsigned lights, int millions,
    unsigned requestedLights, const FeedSet& feeds)
{
    const auto main = workload(uniform,mainWidth,mainHeight,requestedLights,millions,requestedLights,feeds);
    const auto allowance = mainWidth && mainHeight && feeds.count ? (main.budget-regularCost(main))/feeds.count : 0;
    // floors = false: a feed keeps the previous planner below the floors (the guarantee above).
    if (!(q.nearSteps > 0)) return workloadWithin(q,width,height,lights,allowance,requestedLights,0,false);
    return hybridWithin(q,uniform,width,height,lights,allowance,requestedLights,0,false);
}

}
#endif
