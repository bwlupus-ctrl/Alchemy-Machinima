/** Deterministic, allocation-free Phase 2 selection and view policy. */
#ifndef AL_AREA_SHADOW_MATH_H
#define AL_AREA_SHADOW_MATH_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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
//   luminance    : Rec.709 luminance of the linear colour x light scale x EV scale;
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
template<class ID> struct Candidate
{
    ID id{};
    bool shadowMap = false;
    double score = 0.0;
};
// Descending importance, then the stable UUID. shadowMap is reported, never ranked.
template<class ID> inline bool before(const Candidate<ID>& a, const Candidate<ID>& b)
{
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
struct Quality { int samples, steps; bool half; int nearSteps = 0; int minFar = 0; };
inline float lightBias(float bias)
{
    return std::isfinite(bias) ? std::clamp(bias,0.f,4.f) : .25f;
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
};
// Capture-feed slots sharing the frame budget (LLPrismLens::MAX_CAPTURES; asserted in
// pipeline.cpp).
constexpr unsigned kFeedSlots = 8;
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

inline Workload workloadWithin(Quality q, unsigned width, unsigned height, unsigned lights,
    std::uint64_t budget, unsigned requestedLights, std::uint64_t feedHoldBack = 0)
{
    Workload w{};
    const int dense = std::clamp(q.nearSteps,0,64);
    w.q = {std::clamp(q.samples,1,32),std::clamp(q.steps,1,dense ? 128:64),q.half,dense,
        dense ? std::clamp(q.minFar,1,64):0};
    lights = std::min(lights,8u);
    // Reserve for the requested cap, never the changing visible set. At least
    // the actual count keeps malformed callers within the hard frame ceiling.
    const unsigned basis = std::max(lights,std::clamp(requestedLights,1u,8u));
    w.budget = std::min(budget,std::uint64_t(1500000000));
    w.drawCap = w.budget/2;
    // Planning limit: the nominal budget minus the feed reservation (segmented main only).
    const std::uint64_t planned = dense ? w.budget-std::min(feedHoldBack,w.budget) : w.budget;
    const auto update = [&]()
    {
        w.pixels = w.q.half ? ((std::uint64_t(width)+1)/2)*((std::uint64_t(height)+1)/2) :
            std::uint64_t(width)*height;
        w.iterations = w.pixels*lights*w.q.samples*w.q.steps;
    };
    // Render-target dimensions are GL-sized; bound pathological pure-function inputs too.
    if (width > 65536 || height > 65536) return w;
    update();
    const auto over = [&]() { return w.pixels*basis*w.q.samples*w.q.steps>planned ||
        w.pixels*w.q.samples*w.q.steps>w.drawCap; };
    // Segmented plans choose half resolution unless 12 full-res rays (High's
    // request) fit. Choose before reducing samples so the half-res plan keeps
    // all spare rays: a higher preset never ends with fewer rays per pixel.
    if (dense && !w.q.half)
    {
        const std::uint64_t rays = std::min(w.q.samples,kFullResMinSamples);
        if (w.pixels*basis*rays*w.q.steps>planned || w.pixels*rays*w.q.steps>w.drawCap)
        { w.q.half=true; update(); }
    }
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
    const auto perChannel = w.pixels*w.q.samples*w.q.steps;
    w.channels = lights && perChannel ? unsigned(std::min<std::uint64_t>(4,w.drawCap/perChannel)) : 0;
    return w;
}

// Feed guarantee. A uniform (OFF) feed plan draws if and only if its allowance is at
// least pixels(half) * max(requested lights, 2): one ray, one step, half resolution.
// That is the most any feed up to 2048x2048 can need to draw at all. A segmented main
// plan holds back kFeedSlots * min(OFF feed allowance, that need), so with Thin ON every
// feed gets at least min(its Thin OFF allowance, need) and still draws wherever OFF
// draws. The OFF feed allowance is (budget - OFF main reservation) / slots. The bound is
// preset independent: every OFF main plan that draws costs at least the cheapest one
// (1 ray, 1 step, half resolution), so allowance(OFF) <= (budget - that) / slots for every
// preset, and segmented admission does not depend on the preset either.
inline std::uint64_t feedReservation(unsigned mainWidth, unsigned mainHeight, std::uint64_t budget,
    unsigned requestedLights, unsigned feedSlots)
{
    if (!mainWidth || !mainHeight || !feedSlots) return 0;
    budget = std::min(budget,std::uint64_t(1500000000));
    const auto cheapest = workloadWithin({1,1,true},mainWidth,mainHeight,requestedLights,budget,requestedLights);
    const auto reserve = cheapest.channels ? cheapest.iterations : 0;
    const std::uint64_t allowance = (budget-reserve)/feedSlots;
    const std::uint64_t need = kFeedGuaranteePixels*std::max(std::clamp(requestedLights,1u,8u),2u);
    return feedSlots*std::min(allowance,need);
}

// The OFF main plan (nearSteps == 0) is the committed plan, unchanged. A segmented
// plan is admitted only inside budget - feedReservation(); else the caller falls back.
inline Workload workload(Quality q, unsigned width, unsigned height, unsigned lights,
    int millions, unsigned requestedLights, unsigned feedSlots = kFeedSlots)
{
    const std::uint64_t budget = std::uint64_t(std::clamp(millions,1,1500))*1000000;
    return workloadWithin(q,width,height,lights,budget,requestedLights,
        q.nearSteps > 0 ? feedReservation(width,height,budget,requestedLights,feedSlots) : 0);
}

// A segmented (Thin objects) plan that cannot fit at all falls back to the authored
// uniform march and plan, so the switch never removes shadows OFF would draw.
inline Workload workloadOrUniform(Quality q, Quality uniform, unsigned width, unsigned height,
    unsigned lights, int millions, unsigned requestedLights, unsigned feedSlots = kFeedSlots)
{
    auto w = workload(q,width,height,lights,millions,requestedLights,feedSlots);
    if (!w.channels && q.nearSteps) w = workload(uniform,width,height,lights,millions,requestedLights,feedSlots);
    return w;
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

// Frame identity is used only to reset workload accounting, never sampling.
struct FeedPasses
{
    std::uint32_t frame = 0;
    unsigned passes = 0;
    bool admit(std::uint32_t currentFrame, unsigned slots)
    {
        if (frame != currentFrame) { frame = currentFrame; passes = 0; }
        if (passes >= slots) return false;
        ++passes;
        return true;
    }
};

// Reserve the main view's unchanged requested-K plan, then divide only the
// remainder into fixed allowances. No dependence on active feed count, cadence,
// timing, prior frames or the main view's changing selected set. A segmented
// (Thin objects) plan that does not fit a feed's allowance falls back to the
// authored uniform march and plan. A segmented main plan only exists inside
// budget - feedReservation(), and a main plan that cannot fit there is the uniform
// (OFF) plan, so with Thin ON every feed gets at least min(its OFF allowance, the
// minimum a 2048x2048 feed needs) and draws wherever the OFF feed draws. It may get
// fewer rays than OFF when the main view spends the difference.
inline Workload feedWorkload(Quality q, Quality uniform, unsigned mainWidth, unsigned mainHeight,
    unsigned width, unsigned height, unsigned lights, int millions,
    unsigned requestedLights, unsigned feedSlots)
{
    const auto main = workloadOrUniform(q,uniform,mainWidth,mainHeight,requestedLights,millions,requestedLights,feedSlots);
    const auto reserve = main.channels ? main.iterations : 0;
    const auto allowance = mainWidth && mainHeight && feedSlots ? (main.budget-reserve)/feedSlots : 0;
    auto feed = workloadWithin(q,width,height,lights,allowance,requestedLights);
    if (!feed.channels && q.nearSteps)
        feed = workloadWithin(uniform,width,height,lights,allowance,requestedLights);
    return feed;
}

}
#endif
