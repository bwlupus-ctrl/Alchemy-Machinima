# Weather Lightning / Thunderstorm Lighting — Quality Deep-Research & Codegen Brief

**Repo:** `I:\alchemy-machinima` (Alchemy-Machinima, a Second Life viewer fork).
**Goal:** raise the **visual quality** of the thunderstorm lighting — the flash *and* the bolt — to cinematic/machinima grade. The lightning already **works in-world** ("works decently"): a branching procedural bolt, depth-occluded core+halo, directional G-buffer surface flash, far-clip-relative sky flash, and a deterministic multi-stroke flash/bolt pulse. This is a **quality enhancement on top of a working feature**, not a rewrite or a bug fix. This brief is for an external research/codegen agent (GPT/Codex) to design + **GENERATE CODE (no build)**. A Codex/Opus adversarial review + a Claude build follow.

> ⚠️ **Task:** deep-research the techniques, pick an approach per pillar (with rationale + tradeoffs), and **produce generated code + a git-appliable patch + a self-adversarial review. DO NOT BUILD.** Preserve the existing lightning behavior + its determinism behind the same default-off gate. Every invariant in §3 is mandatory — two of them (the reserved-sampler rule and HDR-bounding/NaN-safety) each already cost multiple in-world debug cycles on the sibling rain features.

---

## 1. Vision & goals
Make a Second Life thunderstorm *read* as a thunderstorm and hold up in close cinematic shots:
- **Sheet / cloud lightning** — the defining look: flashes that light the **cloud layer** (intra-cloud glow, the sky pulsing behind/within clouds), not just a flat sky brighten.
- **Richer bolts** — more natural jagged path, hierarchical forking, a bright hot core with a colored corona, per-return-stroke flicker, and strong bloom.
- **Grounded flashes** — the strike lights the world directionally (already partial) *and* pops off **wet surfaces / water** (tie to the new rain/wetness pass) for that rain-storm glint.
- **Distance-graded strikes** — distant strikes = dim diffuse *sheet* flash (little/no bolt); near strikes = bright bolt + strong directional flash.
- **Tasteful HDR drama** — the flash is a deliberate exposure spike + bloom; the scene should punch then settle via auto-exposure, without NaN/runaway.
Machinima-first (RTX 5090 target) with quality/perf knobs; **default-off and cheap when off** stays sacred.

**Non-goals:** not audio/thunder; not a physical lightning simulation; not rearchitecting the EEP sky/atmospherics (may *sample/couple* to them, not replace them).

---

## 2. Current implementation (build on it)

**Controller — `ALWeatherModel` (`alweathermodel.{h,cpp}`), deterministic, presentation-time-driven:**
- `Config { mEnabled, mRatePerMinute, mFlashDurationSeconds, mBoltDurationSeconds, mMinDistanceMeters, mMaxDistanceMeters, mBoltHeightMeters, mSeed }`.
- `Frame { bool mActive, mStrikeStarted; F32 mFlash, mBolt; Position mStrikeBase; F64 mBoltHeightMeters; U64 mStrikeId }`.
- `flashPulse(age,dur)` = `max(primary, echo_a[age≥0.055s], echo_b[age≥0.120s])` clamped [0,1] — a **multi-stroke** (primary + 2 return-stroke echoes) envelope. `boltPulse` similar.
- `beginStrike`: strike placed at `anchor + (cos θ, sin θ)·radius`, `radius ∈ [min,max]`, `mStrikeBase.z = anchor ground height`, height from config. Fully deterministic given seed + presentation time (survives machinima replay). **Keep this determinism.**
- Fed wall-independent presentation time; the scheduler is fixed-timestep + carry-remainder (do not regress).

**Shader — `weatherLightningF.glsl`** (full source in §8): screen-space pass. Per pixel: build the **primary bolt** (24 segments, `weatherBoltOffset` = 3-frequency coherent sine displacement, `sin(pi·u)` envelope anchoring both endpoints) + **2 branches** (8 segments), project each segment to screen via `weather_world_to_view`/`weather_view_projection`, take nearest screen distance → **core** (gaussian, width `weather_lightning_width`) + **halo** (4.5× width, 0.28). Depth-occlude the bolt against `getPosition` scene depth. **Surface flash:** directional light from the strike midpoint using the deferred **`getNorm`** normal (`diffuse·attenuation`), plus a **sky flash** (`scene_depth > 0.95·weather_far_clip`) and an ambient floor. Final: `energy = color · brightness · (flash + bolt·2.5)`, additive into `mRT->screen` before exposure/bloom.

**Pipeline:** `renderWeather` (`pipeline.cpp`) uploads `weather_world_to_view`/`weather_view_projection` (from `gGLLast*` + explicit inv-proj coherence), `weather_strike_base/top`, color, `weather_screen_res`, `weather_lightning_flash`(=`Frame.mFlash`), `..._bolt`(=`mBolt`), width, brightness, ambient, seed, `weather_far_clip`(=live far). Bound + drawn only when `weather.mActive && gDeferredWeatherLightningProgram.isComplete()`.

**Settings (lightning subset):** `AlchemyWeatherLightningEnabled` · `...Rate` · `...FlashDuration` · `...BoltDuration` · `...MinDistance` · `...MaxDistance` · `...BoltHeight` · `...BoltWidth` · `...Brightness` · `...Ambient` · `...Color` · `...Seed` · `...Trigger` (manual one-shot).

---

## 3. HARD CONSTRAINTS & INVARIANTS (mandatory)
1. **Default-off, zero cost.** Lightning already gates on the weather master + `AlchemyWeatherLightningEnabled` + `mActive`; any new feature adds its own toggle and **no work when off** (no new sampler bound, no pass, no per-frame cost). A brand-new heavy sub-effect defaults off inside the default-off master.
2. **Deferred/HDR only + fail-soft.** Every shader bind stays gated on `isComplete()`; a failed new program/permutation must drop only its layer, never disable the existing lightning/rain/deferred. If you add an optional permutation, make it a *separate* program (the rain-occlusion feature's fail-soft-permutation pattern).
3. **RESERVED-SAMPLER RULE (load-bearing).** If any new effect needs a texture (e.g. an EEP **cloud/sky** sample for sheet lightning, or the scene-color/bloom buffer), it MUST be a reserved uniform — add to `EShaderUniforms` (`indra/llrender/llshadermgr.h`, append before `END_RESERVED_UNIFORMS`) + `mReservedUniforms.push_back("exact_glsl_name")` (`llshadermgr.cpp`, matching ordinal, size assert) and bound with the **S32/index** `bindTexture(LLShaderMgr::YOUR_ENUM, …)` overload. **NEVER** the string `bindTexture("name", …)` overload (it silently binds texture unit 0 — this was a whole-frame-wash bug; `mapUniform` now warns on unreserved samplers and the string overloads were deleted). Prefer reusing already-reserved deferred inputs (`getPosition`/`getNorm`) and EEP **uniforms** over a new sampler where possible.
4. **HDR + NaN discipline.** Lightning is an intentional additive **spike** into `mRT->screen` before `generateLuminance`/`generateExposure`/`generateBloomHDR`. It MUST stay finite and bounded to a sane ceiling — guard every `normalize()`/divide/`pow`/`exp`; the flash may be bright but must not NaN or runaway (a NaN here poisons whole-frame exposure). Keep the existing "branch, don't `mix`, to avoid NaN propagation from the unused surface term" idiom. If you make the flash brighter/HDR-punchier, verify auto-exposure adaptation reads as drama (punch → settle), not a broken pump.
5. **Determinism / reproducibility.** All new animation derives from the controller's **presentation time** + seed (same source the flash/bolt/echo pulses use), never wall-clock/frame-count. A recorded machinima take of a storm MUST replay identically. Per-stroke flicker / re-path must be a deterministic function of `mStrikeId`/seed/presentation-time.
6. **Matrix coherence.** Reuse `weather_world_to_view` / `weather_view_projection` (the `gGLLast*`-derived coherent pair) for any new world→screen projection; reconstruct scene position/normal via the existing deferred path.
7. **Client-only.** No simulator sends. Lightning is viewer-local.
8. **UI = Director-Console-superset.** New controls go in the shared `panel_weather_settings.xml` (Lightning group; both the Lightbox floater and the Director Console Weather tab), preset/reset-backed, real consumers, and with the per-slider reset button pattern (`Weather.ResetControl`). No dead settings.
9. **GL target & tier.** GL 3.3–4.6 core; class1 fragment shader paired with the stock `deferred/postDeferredNoTCV.glsl` screen-triangle vertex stage (as now).

---

## 4. Research pillars — techniques, tradeoffs, recommendations

### A. Bolt realism (evolve `weatherLightningF.glsl`)
- **Jaggedness:** replace/augment the 3-sine `weatherBoltOffset` with **recursive midpoint displacement** (fractal subdivision) or a value-noise ridged path for a more natural crooked bolt; keep the `sin(pi·u)` endpoint anchoring.
- **Hierarchical forks:** more than the current 2 fixed branches — a small recursive fork set (branches that spawn sub-branches), each dimmer/thinner, deterministically seeded. Watch the per-pixel segment loop cost (it is O(segments) nearest-distance per pixel) — cap it + gate by a quality knob.
- **Core + corona:** a very bright thin **hot core** + a wider softer **colored corona** (blue-white core → violet/blue outer), instead of a single gaussian + flat halo.
- **Per-stroke flicker & re-path:** drive bolt intensity from the multi-stroke `mBolt` echoes; optionally re-seed the path slightly per return stroke (a "re-strike along a similar channel" shimmer), deterministically from `mStrikeId` + stroke index.
- **Bloom:** ensure the bolt reads as a bloom source (it feeds `generateBloomHDR`) — tune its HDR level so it blooms without clipping the frame.
- **Recommended:** fractal-displaced path + a modest recursive fork tree + hot-core/colored-corona + echo-driven flicker, all bounded by a `LightningQuality`/segment knob. No new sampler.

### B. Flash + CLOUD illumination (the headline quality win)
- **Sheet / cloud lightning:** make the flash light the **cloud layer**, not a flat sky brighten. Research: (i) modulate the sky-flash by EEP **cloud coverage/density** (couple to the current EEP data the weather system already reads, or sample the cloud/sky — if a texture is needed, reserve it per §3.3); (ii) a procedural cloud-shaped glow on sky pixels driven by the flash + a coherent noise, so the sky *pulses in cloud shapes*. This is what makes it read as a thunderstorm.
- **Directional surface flash:** the current `diffuse·attenuation` from the strike midpoint is good; improve falloff/energy, and consider a brief soft ambient "fill" flash distinct from the directional term.
- **Distance-graded strikes:** use `mMinDistance/mMaxDistance` (and the strike distance from camera) to grade **distant = dim diffuse sheet flash, little/no bolt** vs **near = bright bolt + strong flash**. Drives variety + realism.
- **Color temperature variation:** per-strike deterministic variation (some bluer, some whiter/violet) from `mStrikeId`.
- **Recommended:** cloud-modulated sheet-lightning sky flash (EEP-coupled, procedural if no clean cloud sampler) + distance-graded strike character + slight per-strike color variation.

### C. Wet-surface & water reflection (tie lightning to the rain)
- During a flash, **pop specular on wet/up-facing surfaces** (couple to the surface/wetness pass's up-facing + wet state) so the ground *glints* under lightning — hugely cinematic in rain.
- Consider the **water plane**: a flash reflecting off water. Evaluate feasibility without a new pass (the deferred normal + the flash may suffice for a screen-space glint).
- **Recommended:** a wet-surface flash-glint term reusing the deferred normal + the wetness gate; water reflection only if cheap.

### D. Temporal envelope
- Refine the multi-stroke flicker (rapid stutter + decay) in the controller's `flashPulse`/`boltPulse` (more return strokes, jittered timing, deterministic) for a more electric flicker; optional brief **afterglow**. Keep it presentation-time-deterministic.

### E. HDR / exposure drama
- Treat the flash as a controlled exposure spike; verify the auto-exposure "punch then settle" reads well and never NaNs/pumps. Provide a brightness/ceiling knob. The flash energy must stay bounded/finite (see §3.4).

---

## 5. Integration points
Extend `weatherLightningF.glsl` (bolt A, flash B/C/E) + `alweathermodel.{cpp,h}` (D, distance grading, per-strike variation) + `pipeline.cpp` `renderWeather` lightning uploads (any new uniforms; EEP cloud coupling; the wetness/surface state hand-off) + `settings.xml` + shared `panel_weather_settings.xml` (Lightning group controls + reset buttons). New sampler (cloud/sky) only if unavoidable — **reserve it** (§3.3). Keep the pass class1 + screen-triangle. Presentation-time clock throughout.

## 6. Determinism, performance, quality knobs
- Determinism: presentation-time + `mStrikeId`/seed for all new randomness; recorded storms replay identically.
- Perf: the per-pixel bolt loop is the main cost — cap segments/forks; gate sheet-lightning/cloud work; expose a `LightningQuality` tier + individual toggles. State expected cost.
- Every additive term finite + bounded; a global flash-ceiling knob as backstop.

## 7. Deliverable
Design writeup (technique per pillar + rationale + rejected alternatives) + **generated code**: `weatherLightningF.glsl` enhancements, `alweathermodel` changes (flicker/distance/variation), `pipeline.cpp` uploads (+ reserved sampler in `llshadermgr` if any), `settings.xml` + shared-panel controls (with reset buttons). Plus a **git-appliable patch** and a **self-adversarial review** against §3 (HDR bounding, NaN, reserved-sampler compliance, default-off zero-cost, fail-soft, determinism, no regression to the working lightning). **No build/link/deploy.**

## 8. Appendix — current `weatherLightningF.glsl` (verified current)
```glsl
out vec4 frag_color;
in vec2 vary_fragcoord;

uniform mat4  weather_world_to_view;
uniform mat4  weather_view_projection;
uniform vec3  weather_strike_base;
uniform vec3  weather_strike_top;
uniform vec3  weather_lightning_color;
uniform vec2  weather_screen_res;
uniform float weather_lightning_flash;    // controller Frame.mFlash (multi-stroke pulse)
uniform float weather_lightning_bolt;     // controller Frame.mBolt
uniform float weather_lightning_width;
uniform float weather_lightning_brightness;
uniform float weather_lightning_ambient;
uniform float weather_lightning_seed;      // per-strike deterministic seed
uniform float weather_far_clip;            // live LLViewerCamera far

vec4 getPosition(vec2 pos_screen);         // deferred view-space pos
vec4 getNorm(vec2 screenpos);              // deferred view-space normal

const int PRIMARY_SEGMENTS = 24;
const int BRANCH_SEGMENTS  = 8;

// --- coherent multi-frequency lateral displacement, anchored at both ends ---
vec2 weatherBoltOffset(float u, float seed, float amplitude){
    float envelope = sin(3.14159265 * clamp(u,0.0,1.0));
    float x = sin(u*17.0+seed*5.1) + 0.55*sin(u*41.0+seed*11.7) + 0.25*sin(u*93.0+seed*2.3);
    float y = sin(u*19.0+seed*8.4) + 0.50*sin(u*47.0+seed*3.9) + 0.22*sin(u*87.0+seed*13.1);
    return vec2(x,y) * amplitude * envelope;
}
vec3 weatherPrimaryPoint(float u){
    vec3 p = mix(weather_strike_base, weather_strike_top, u);
    float h = max(weather_strike_top.z - weather_strike_base.z, 1.0);
    p.xy += weatherBoltOffset(u, weather_lightning_seed, h*0.022);
    return p;
}
vec3 weatherBranchPoint(float v, int branch){ /* 2 fixed forks off u=0.38 / 0.61, seeded */ ... }
bool weatherProject(vec3 world, out vec2 uv, out float view_depth){
    vec4 view = weather_world_to_view * vec4(world,1.0);
    vec4 clip = weather_view_projection * vec4(world,1.0);
    view_depth = length(view.xyz);
    if (clip.w <= 1.0e-4){ uv = vec2(-10.0); return false; }
    uv = (clip.xy/clip.w)*0.5+0.5;
    return all(lessThan(abs(clip.xy/clip.w), vec2(1.3)));
}
// nearest screen-space distance from `pixel` to each projected segment -> nearest_pixels + depth
void weatherSegmentDistance(vec2 pixel, vec3 first, vec3 second, inout float nearest_pixels, inout float nearest_depth){ ... }

void main(){
    vec2 tc = vary_fragcoord.xy;
    vec2 pixel = tc * weather_screen_res;
    float nearest_pixels = 1e20, nearest_bolt_depth = 1e20;
    // primary bolt (24 segs) + 2 branches (8 segs) -> nearest_pixels / nearest_bolt_depth
    ...
    vec3 surface_view = getPosition(tc).xyz;
    float scene_depth = length(surface_view);
    float width = max(weather_lightning_width, 0.25);
    float core = exp(-0.5*nearest_pixels*nearest_pixels/(width*width));
    float halo = exp(-0.5*nearest_pixels*nearest_pixels/((width*4.5)*(width*4.5)))*0.28;
    float visible = nearest_bolt_depth <= scene_depth + 0.75 ? 1.0 : 0.0;   // depth-occlude
    float bolt = (core+halo)*visible*weather_lightning_bolt;

    // directional surface flash from the strike midpoint via the deferred normal
    vec3 strike_mid = mix(weather_strike_base, weather_strike_top, 0.55);
    vec3 light_view = (weather_world_to_view*vec4(strike_mid,1.0)).xyz;
    vec3 to_light = light_view - surface_view;
    float d = max(length(to_light),0.01);
    vec3 L = to_light/d;
    vec3 n_raw = getNorm(tc).xyz; float nl = length(n_raw);
    vec3 N = nl>1e-5 ? n_raw/nl : vec3(0,0,1);
    float diffuse = max(dot(N,L),0.0);
    float range = max(length(weather_strike_top-weather_strike_base)*1.5, 64.0);
    float atten = 1.0/(1.0 + d*d/(range*range));
    float sky = scene_depth > 0.95*weather_far_clip ? 1.0 : 0.0;            // far-clip-relative sky
    float surface_flash = sky>0.5 ? 1.0 : diffuse*atten;                    // branch (avoid NaN mix)
    float flash = weather_lightning_flash * (weather_lightning_ambient + surface_flash);

    vec3 energy = weather_lightning_color * weather_lightning_brightness * (flash + bolt*2.5);
    frag_color = vec4(energy, 0.0);   // additive into mRT->screen, pre-exposure/bloom
}
```
*(Full verbatim source is at `indra/newview/app_settings/shaders/class1/deferred/weatherLightningF.glsl`; the branch-point + segment-distance bodies are elided above for brevity — read the file for exact code.)*

## 9. Summary
Elevate the thunderstorm lighting: **cloud/sheet-lightning flashes** (the headline), richer **fractal/forked bolts with hot core + corona + per-stroke flicker**, **wet-surface/water flash glints** tied to the rain, and **distance-graded** strikes — all **default-off, deferred, fail-soft, HDR-bounded, NaN-safe, reserved-sampler-correct (if any new texture), presentation-time-deterministic, client-only, Director-Console-superset**, building on the working lightning + its deterministic controller. **Generate code + patch + self-review; do not build.**
