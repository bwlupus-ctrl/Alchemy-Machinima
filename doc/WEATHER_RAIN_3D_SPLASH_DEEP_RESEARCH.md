# Weather Rain → 3D · Splashes · Life — Deep Research & Codegen Brief

**Repo:** `I:\alchemy-machinima` (Alchemy-Machinima fork, Second Life viewer).
**Status of the base feature:** the procedural rain + lightning weather system is **built and in-world-verified** (rain renders as depth-occluded world-anchored streaks; lightning lights the scene; default-off). This document is a brief for an external research/codegen agent (GPT / Codex) to design and **GENERATE CODE** for the *next* stage — making rain read as 3D, splash on surfaces, and feel alive — **without building**. A Fable/Opus adversarial review and a Claude build will follow.

> ⚠️ **Task for the researching agent:** deep-research the techniques below, choose an approach per pillar (with rationale), and **produce generated code + a git-appliable patch + a self-adversarial review**. **DO NOT BUILD.** This is an *enhancement on top of a working baseline*, not a rewrite — the current rain/lightning behavior must be preserved behind the same default-off gate. Every invariant in §3 is mandatory; two of them (reserved-sampler binding, HDR energy bounding) each already cost a full in-world debug cycle, so treat §3 as load-bearing.

---

## 1. Vision & goals

Make rain feel **alive and three-dimensional**, cinematic enough for machinima on a high-end GPU (RTX 5090 target), while staying true-default-off and cheap when disabled.

- **Pillar A — 3D / volumetric rain.** The current pass is already world-anchored and depth-marched (2.5D). Push it to read as genuine 3D: near→far parallax, perspective-correct streak length, wind gusts, near-field emphasis.
- **Pillar B — Splashes on surface hits (the headline "life").** Where rain meets up-facing surfaces, spawn procedural splash ripples + micro-splashes — screen-space, reusing the deferred G-buffer (normal + depth), **no CPU particle system**.
- **Pillar C — Wetness & atmosphere.** Subtle wet-surface sheen, optional ground mist during heavy rain, shared wind gusts, optional camera-"lens" drops for POV shots.

**Non-goals:** not a CPU/particle-engine rewrite unless a pillar genuinely can't be done procedurally; not simulator-side weather; not re-architecting SL's separate water surface (visually perturbing it is fine, rearchitecting it is out).

---

## 2. Current implementation (the baseline to extend)

**Architecture.** Rain is a **screen-space, world-anchored, depth-clamped, procedural** effect composited **additively into the HDR scene buffer `mRT->screen`** inside `LLPipeline::renderWeather()` (`pipeline.cpp`), which runs in `renderFinalize()` **before** `generateLuminance / generateExposure / generateBloomHDR / colorCorrect` — so rain participates in HDR exposure + bloom + tonemap. Lightning is a sibling pass driven by a deterministic controller (`ALWeatherModel`, `alweathermodel.{h,cpp}`); **rain itself is pure shader** (no controller state). An optional **half-res + depth-aware bilateral upsample** path exists for performance.

**Rain shader algorithm (`weatherRainF.glsl`).** Per pixel: reconstruct view position from the G-buffer depth (`getPosition`, from `deferredUtil.glsl`), march from `0.75` to `min(surfaceDistance, maxDistance)` in quadratic steps (more samples near camera), evaluate `weatherDropField()` in **world space** (via `weather_inv_modelview`), accumulate distance-weighted, and additively composite a **bounded, hue-preserving** contribution. Depth-occluded (rain stops at the surface). World-anchored (streaks don't swim with the camera). `weatherDropField` = narrow-transverse cell-hash **filaments** × falling **streak** phase (animated by `weather_time * fall_speed` along a wind-slanted `fall_dir`) × a **population** hash.

**Hard-won correctness (already fixed — do not regress):**
- Matrices come from `gGLLastModelView` / `gGLLastProjection` (the pair that produced the sampled deferred depth), and the pass **overrides `deferredUtil`'s `inv_proj`** (`LLShaderMgr::INVERSE_PROJECTION_MATRIX`) so view↔world reconstruction is coherent with the depth — *not* from incidental post-pass GL matrix state.
- The additive contribution is **bounded** (`energy = min(rain*intensity*1.5, 0.25)` + a hue-preserving peak cap at 0.25) so a degenerate field can't dominate/poison HDR exposure+bloom.
- The half-res upsample samples the rain map via a **reserved** uniform (`LLShaderMgr::WEATHER_RAIN_MAP`) bound by **index** — see §3, this replaced a string-overload bind that silently sampled texture unit 0 (the depth map) and washed the whole frame red/orange.

**Settings (rain subset, all `Persist`, in `settings.xml`, surfaced in the shared weather panel):**
`AlchemyWeatherEnabled` (master, default 0) · `AlchemyWeatherRainEnabled` (default 1) · `AlchemyWeatherRainIntensity` (0.55) · `AlchemyWeatherRainDensity` (0.65) · `AlchemyWeatherRainFallSpeed` (28) · `AlchemyWeatherRainMaxDistance` (80) · `AlchemyWeatherRainSamples` (12, clamp 4–24) · `AlchemyWeatherRainResolutionDivisor` (2, 1–4) · `AlchemyWeatherWindScale` (1) · `AlchemyWeatherRainColor` (Color3 0.55,0.68,0.78) · `AlchemyWeatherEEPCoupling` (0.35).

Full current source for all of the above is in the **Appendix (§8)** — build on it.

---

## 3. HARD CONSTRAINTS & INVARIANTS (mandatory)

1. **True default-off, zero cost.** `renderWeather` early-returns on `!AlchemyWeatherEnabled` before any allocation/bind. Every new feature gets its own enable flag and adds **no** per-frame cost when off. No new render target is allocated, no pass runs, when disabled.
2. **Deferred/HDR only + fail-soft.** Everything runs in the deferred path. **Every** shader bind is gated on `program.isComplete()`. A new shader that fails to compile must drop *only* its own layer — never disable deferred, never break the existing rain/lightning.
3. **RESERVED-UNIFORM RULE for every new sampler (load-bearing — cost 2 debug cycles).** Any new texture sampler a shader reads MUST be (a) added to `EShaderUniforms` in `indra/llrender/llshadermgr.h` (append before `END_RESERVED_UNIFORMS`), (b) `mReservedUniforms.push_back("exact_glsl_name")` in `llshadermgr.cpp` at the **matching ordinal** (there is a size assertion — enum index must equal push-back position), and (c) bound with the **S32/index** overload `program.bindTexture(LLShaderMgr::YOUR_ENUM, rt, …)`. **NEVER** use the string overload `bindTexture("name", …)` — it passes a GL *location* where a reserved *index* is expected and, for a non-reserved sampler, leaves the sampler at GL texture unit 0. `WEATHER_RAIN_MAP` is the worked example. (A `mapUniform` `LL_WARNS` now flags any unreserved sampler at shader-load — watch for it.) Passes that only read the deferred G-buffer (depth/normal) via `bindDeferredShader` need **no** new sampler.
4. **Matrix coherence.** Reconstruct view/world from `gGLLastModelView`/`gGLLastProjection` and override `LLShaderMgr::INVERSE_PROJECTION_MATRIX` with `inverse(gGLLastProjection)` (see the current rain/upsample passes). Do not depend on whatever matrices happen to be current during post-processing.
5. **HDR discipline.** Every additive contribution to `mRT->screen` (rain, splash, wetness, mist) MUST be **bounded** (hard energy ceiling) and **NaN/Inf-safe** (guard every divide, `normalize()` of a possibly-zero vector, `pow/log/exp`, `1/x`). An unbounded or non-finite value poisons `generateLuminance`→exposure→bloom and produces a whole-frame artifact. Prefer a single shared "weather exposure-safety" clamp.
6. **Determinism / reproducibility.** Any new time-based animation MUST derive from the **presentation clock** `LLPresentationTime::currentFrame().presentation_time` (the same source lightning uses), **not** wall-clock — so machinima takes reproduce and respect the world-time-scale slider. No per-frame accumulator that drifts with frame rate; seed spatial randomness from world position + a fixed seed.
7. **Client-only.** No simulator messages, no shared state.
8. **UI = Director-Console-superset.** New controls go in the **shared** `panel_weather_settings.xml` (LLPanelInjector-registered), which is embedded in **both** the Lightbox floater and the Director Console "Weather" tab. Every setting must have a real consumer, round-trip through preset save/load + Reset All, and use auto-hiding groups. Gray/disable controls when the deferred path is unavailable (pattern already present).
9. **Render-state hygiene.** Any new render target (cf. `mWeatherRainHalf`): `bindTarget`/`flush` must nest correctly, restore blend/colorMask/scissor/viewport, and `release()` on disable/resize. Do not leak state into the post chain.
10. **GL target & shader tier.** GL 3.3–4.6 core. New fragment passes are **class1** and pair with the stock `deferred/postDeferredNoTCV.glsl` vertex stage (fullscreen triangle), exactly like the current rain/upsample/lightning shaders. Only go higher-class with justification.

---

## 4. Research pillars — candidate techniques, tradeoffs, recommendations

### Pillar A — 3D / volumetric rain (evolve the existing pass)
Research and choose among:
- **Layered parallax slabs.** Evaluate 2–3 depth layers (near/mid/far) with distinct `spacing`/size/`fall_speed`/opacity; composite. Cheap, strong depth cue.
- **Perspective-correct streak length.** Streak length ∝ `fall_speed × virtual-shutter × projection`, so streaks lengthen with speed and shrink with distance (motion-blur look). Fatter/longer near, thinner/fainter far.
- **Near-field emphasis.** A few large, strongly-parallaxed close streaks layered over the procedural volume for tangible 3D presence.
- **Gusty wind.** Time+space turbulence (bounded) modulating `fall_dir`, shared with splash density (Pillar B/C).
- **(Heavier, evaluate & likely defer) GPU-instanced billboard streaks** for the nearest band — more literal 3D but adds a geometry pass; discuss vs. staying fully procedural.

**Recommended:** stay in the existing procedural ray-march; add **layered parallax + perspective streak-length + gusts + near-field emphasis**. No new geometry, no new sampler. This is the lowest-risk path that materially improves the 3D read.

### Pillar B — Procedural surface splashes (headline "life")
Goal: where rain lands on **up-facing** surfaces, render animated splashes — procedurally, screen-space, from the G-buffer, no particles.
- **Splash events by hash.** Reconstruct world position; sample the deferred **normal**; gate on `normal·up > threshold` AND within range AND rain active. Hash `floor(world.xz / cellSize)` + quantized presentation-time to spawn events; event probability scales with `intensity`. Each event → an **expanding ring** (radius grows with age, opacity fades) + optional tiny vertical **micro-splash** streak. Density ∝ intensity; ring size/lifetime as knobs.
- **Continuous wet ripples.** Subtle moving normal-perturbation ripples on wet up-facing surfaces to catch specular (separate, gentler than discrete rings).
- **Water vs. land.** SL water renders separately; decide whether to detect water (depth/normal heuristics) and tune ring behavior, or treat all up-facing surfaces uniformly (acceptable first cut). Avoid heavy far-water shimmer.
- **Inputs/bindings.** If it only needs deferred normal+depth (both provided by `bindDeferredShader`) → **no new sampler**. If it needs a half-res splash mask RT → **reserve** its sampler (§3.3) and reuse the bilateral-upsample idiom.

**Recommended:** a dedicated **screen-space "surface response" pass** after the rain pass — reads deferred normal+depth, spawns expanding-ring ripples + micro-splashes keyed on up-facing world-XZ×presentation-time hashes, intensity-scaled, **bounded additive**, fail-soft, presentation-time-driven. This is the architecture-consistent, no-particle path and the biggest "alive" payoff.

### Pillar C — Wetness & atmosphere
- **Wet-surface sheen:** on up-facing surfaces during rain, bounded specular lift / slight albedo darkening (screen-space) so the world looks rained-on.
- **Ground mist:** subtle bounded height-fog near up-facing surfaces during heavy rain.
- **Camera-lens drops:** optional screen-space POV drops/streaks (toggle, **default off** — divisive for machinima).
- Share the gust field with A/B.

**Recommended:** wet sheen + optional ground mist + optional lens drops, each its own bounded, default-friendly toggle.

---

## 5. Proposed architecture / integration

- **Where:** extend `renderWeather()`. Keep Pillar-A streak enhancements **inside** `weatherRainF.glsl`. Add Pillar-B/C as a **new dedicated fragment pass** (e.g. `gDeferredWeatherSurfaceProgram` + `weatherSurfaceF.glsl`, class1, screen-triangle, via `bindDeferredShader` for normal+depth), composited **bounded-additive** into `mRT->screen` after the rain pass — gated + fail-soft + presentation-time-driven + matrix-coherent.
- **New reserved uniforms/samplers:** only if a pass introduces a *new texture* (e.g. a splash-mask half-res RT). Reserve per §3.3. The surface pass likely needs only the already-reserved deferred normal/depth + scalar uniforms.
- **New settings:** `AlchemyWeatherRain*` (layers, streak length, gust), `AlchemyWeatherSplash*` (enable, density, ring size, lifetime, up-threshold, range), `AlchemyWeatherWetness*` (enable, strength), `AlchemyWeatherMist*`, `AlchemyWeatherLensDrops` — enable toggles + tunables, **all with real consumers**, all in the shared panel, all preset/reset.
- **Clock:** thread `LLPresentationTime::currentFrame().presentation_time` into the new pass(es).
- **Perf:** reuse the half-res + reserved-sampler bilateral-upsample pattern where it helps; add quality tiers + distance LOD + density caps.

---

## 6. Determinism, performance, quality knobs
- **Determinism:** presentation-time clock; spatial hashes seeded by world position + fixed seed; frame-rate-independent; reproducible takes; world-time-scale aware.
- **Performance (RTX 5090 target, but scalable):** per-pillar quality tiers; distance LOD; optional half-res; sample/density caps. State expected relative cost of each pillar and the default tier.
- **Safety:** every additive term individually bounded; a single global weather HDR-safety clamp as backstop.

---

## 7. Deliverable for the researching agent (GPT / Codex)

Produce, **without building**:
1. **Design section:** the technique chosen per pillar + rationale + rejected alternatives + expected cost.
2. **Generated code:**
   - `weatherRainF.glsl` enhancements (Pillar A).
   - New `weatherSurfaceF.glsl` (Pillars B/C) + any upsample shader.
   - `pipeline.cpp` `renderWeather` integration: new pass(es), each **gated + fail-soft + reserved-sampler-correct + matrix-coherent + presentation-time-fed + bounded-additive + state-clean**.
   - `indra/llrender/llshadermgr.h` + `.cpp` reserved-uniform additions for any new sampler (ordinal-aligned; note the size assert).
   - `settings.xml` new keys (sane defaults, real consumers) + `panel_weather_settings.xml` controls (auto-hide groups; the Director Console tab already hosts this shared panel).
3. **A git-appliable patch** (like prior bundles) + a **self-adversarial review** covering: HDR bounding, NaN/Inf safety, reserved-uniform compliance (no string `bindTexture`), default-off zero cost, fail-soft, determinism (presentation-time, frame-rate independence), UI/preset round-trip, and **no regression to the working baseline rain/lightning**.
4. **Verification checklist** (self-check against every item in §3).

**Explicitly:** no build, no link, no asset deploy — Claude owns those. Deliver code + patch + review only.

---

## 8. Appendix — current source (verified current, {post fixes})

### 8.1 `indra/newview/app_settings/shaders/class1/deferred/weatherRainF.glsl` (full)
```glsl
/**
 * @file weatherRainF.glsl
 * @brief World-anchored, depth-clamped precipitation composite.
 */
/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;
in vec2 vary_fragcoord;

uniform mat4  weather_inv_modelview;
uniform vec3  weather_wind;
uniform vec3  weather_rain_color;
uniform float weather_time;
uniform float weather_intensity;
uniform float weather_density;
uniform float weather_fall_speed;
uniform float weather_max_distance;
uniform float weather_lightning;
uniform float weather_frame;
uniform int   weather_samples;

vec4 getPosition(vec2 pos_screen);   // deferredUtil.glsl — view-space pos from depth

const int WEATHER_MAX_SAMPLES = 24;

float weatherHash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
float weatherGradientNoise(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

float weatherDropField(vec3 world_pos, vec3 fall_dir, vec3 side, vec3 across) {
    const float min_spacing = 0.24;
    const float max_spacing = 1.25;
    float spacing = mix(max_spacing, min_spacing, weather_density);

    vec2 transverse = vec2(dot(world_pos, side), dot(world_pos, across));
    vec2 cell = floor(transverse / spacing);
    float seed = weatherHash21(cell);
    vec2 seed_offset = vec2(seed, fract(seed * 19.371));
    vec2 in_cell = fract(transverse / spacing + seed_offset) - 0.5;

    // Narrow transverse footprint => filaments not dots. radius derivative is
    // discontinuous at cell edges + grows for distant half-res samples; clamp it
    // so a filament never fills a cell (that caused a fullscreen wash).
    float radius = length(in_cell);
    const float filament_radius = 0.035;
    float aa = clamp(fwidth(radius), 0.008, filament_radius);
    float filament = 1.0 - smoothstep(filament_radius - aa, filament_radius + aa, radius);

    float along = dot(world_pos, fall_dir);
    float period = mix(11.0, 4.0, weather_density);
    float phase = abs(fract((along - weather_time * weather_fall_speed) / period + seed) - 0.5);
    float streak_fraction = mix(0.055, 0.19, weather_intensity);
    float streak = 1.0 - smoothstep(streak_fraction, streak_fraction + 0.025, phase);

    float population = step(1.0 - weather_density, weatherHash21(cell + vec2(17.0, 43.0)));
    return filament * streak * population;
}

void main() {
    vec2 tc = vary_fragcoord.xy;
    vec3 surface_view = getPosition(tc).xyz;
    float surface_distance = length(surface_view);
    float max_distance = max(weather_max_distance, 1.0);

    if (!(surface_distance > 0.0) || weather_intensity <= 0.0) { frag_color = vec4(0.0); return; }

    float ray_end = min(surface_distance, max_distance);
    if (ray_end <= 0.75) { frag_color = vec4(0.0); return; }

    vec3 ray_dir  = surface_view / surface_distance;
    vec3 fall_dir = normalize(vec3(weather_wind.xy, -max(weather_fall_speed, 1.0)));
    vec3 reference = abs(fall_dir.z) < 0.95 ? vec3(0.0,0.0,1.0) : vec3(1.0,0.0,0.0);
    vec3 side   = normalize(cross(fall_dir, reference));
    vec3 across = normalize(cross(fall_dir, side));

    int sample_count = clamp(weather_samples, 4, WEATHER_MAX_SAMPLES);
    float jitter = weatherGradientNoise(gl_FragCoord.xy + vec2(weather_frame * 0.071));
    float sum = 0.0, weight_sum = 0.0;

    for (int i = 0; i < WEATHER_MAX_SAMPLES; ++i) {
        if (i >= sample_count) break;
        float u = (float(i) + jitter) / float(sample_count);
        float t = mix(0.75, ray_end, u * u);            // quadratic: denser near camera
        vec3 view_pos  = ray_dir * t;
        vec3 world_pos = (weather_inv_modelview * vec4(view_pos, 1.0)).xyz;
        float distance_weight = 1.0 - t / max_distance; distance_weight *= distance_weight;
        sum += weatherDropField(world_pos, fall_dir, side, across) * distance_weight;
        weight_sum += distance_weight;
    }

    float rain = weight_sum > 1.0e-5 ? sum / weight_sum : 0.0;
    vec3 color = weather_rain_color * mix(1.0, 2.1, clamp(weather_lightning, 0.0, 1.0));
    float energy = min(rain * weather_intensity * 1.5, 0.25);   // bounded for HDR
    vec3 contribution = color * energy;
    float peak = max(contribution.r, max(contribution.g, contribution.b));
    contribution *= min(1.0, 0.25 / max(peak, 1.0e-5));         // hue-preserving ceiling
    frag_color = vec4(contribution, 0.0);
}
```

### 8.2 `indra/newview/app_settings/shaders/class1/deferred/weatherRainUpsampleF.glsl` (full)
```glsl
/**
 * @file weatherRainUpsampleF.glsl
 * @brief Depth-aware bilateral resolve for low-resolution precipitation.
 */
/*[EXTRA_CODE_HERE]*/
out vec4 frag_color;
in vec2 vary_fragcoord;

uniform sampler2D weather_rain_map;      // RESERVED as LLShaderMgr::WEATHER_RAIN_MAP
uniform vec2 weather_rain_map_res;

vec4 getPosition(vec2 pos_screen);

void main() {
    vec2 tc = vary_fragcoord.xy;
    vec2 low_res = max(weather_rain_map_res, vec2(1.0));
    float full_depth = length(getPosition(tc).xyz);

    vec2 low_pos = tc * low_res - 0.5;
    vec2 base = floor(low_pos);
    vec2 fraction = fract(low_pos);
    float sigma = max(full_depth * 0.035, 0.08);

    vec3 sum = vec3(0.0); float weight_sum = 0.0;
    for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) {
        vec2 tap_texel = base + vec2(float(x), float(y)) + 0.5;
        vec2 tap_uv = clamp(tap_texel / low_res, vec2(0.0), vec2(1.0));
        vec3 rain = texture(weather_rain_map, tap_uv).rgb;
        float tap_depth = length(getPosition(tap_uv).xyz);
        float wx = x == 0 ? 1.0 - fraction.x : fraction.x;
        float wy = y == 0 ? 1.0 - fraction.y : fraction.y;
        float depth_weight = exp(-abs(full_depth - tap_depth) / sigma);
        float weight = (wx * wy) * depth_weight;
        sum += rain * weight; weight_sum += weight;
    }
    vec3 rain = weight_sum > 1.0e-4 ? sum / weight_sum : texture(weather_rain_map, tc).rgb;
    frag_color = vec4(rain, 0.0);
}
```

### 8.3 `pipeline.cpp` — `renderWeather` rain + upsample integration (verified current excerpt; the reference pattern to copy)
```cpp
// ... after the default-off gate, cube-snapshot guard, setting reads, controller update,
//     and: draw_rain = rain_intensity>0 && gDeferredWeatherRainProgram.isComplete();
//          draw_lightning = weather.mActive && gDeferredWeatherLightningProgram.isComplete();

// Matrices that PRODUCED the deferred depth (gGLLast* are current-frame here despite the name).
const glm::mat4 modelview          = glm::make_mat4(gGLLastModelView);
const glm::mat4 projection         = glm::make_mat4(gGLLastProjection);
const glm::mat4 inverse_modelview  = glm::inverse(modelview);
const glm::mat4 inverse_projection = glm::inverse(projection);
const glm::mat4 view_projection    = projection * modelview;

if (draw_rain) {
    const U32 divisor = llclamp(rain_divisor_setting(), 1U, 4U);
    bool low_resolution = divisor > 1U && gDeferredWeatherRainUpsampleProgram.isComplete()
                       && target->getWidth() >= divisor*4U && target->getHeight() >= divisor*4U;
    if (low_resolution) { /* allocate/resize mWeatherRainHalf (GL_RGBA16F); on fail -> low_resolution=false */ }

    LLRenderTarget* rain_target = low_resolution ? &mWeatherRainHalf : target;
    rain_target->bindTarget();
    if (low_resolution) { /* LLGLDisable scissor; glClearColor(0,0,0,0); rain_target->clear(GL_COLOR_BUFFER_BIT); */ }
    {
        LLGLDepthTest no_depth(GL_FALSE); LLGLEnable blend(GL_BLEND); LLGLDisable no_scissor(GL_SCISSOR_TEST);
        gGL.setSceneBlendType(LLRender::BT_ADD); gGL.setColorMask(true, false);

        bindDeferredShader(gDeferredWeatherRainProgram);
        // Override deferredUtil's inv_proj with the projection that produced the sampled depth:
        gDeferredWeatherRainProgram.uniformMatrix4fv(LLShaderMgr::INVERSE_PROJECTION_MATRIX, 1, false,
                                                     glm::value_ptr(inverse_projection));
        // ... finite_clamp'd wind/color/intensity/density/fallspeed/maxdist/lightning/frame/samples uniforms,
        //     weather_inv_modelview = inverse_modelview, weather_time = fmod(presentation_now, 3600) ...
        mScreenTriangleVB->setBuffer(); mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
        unbindDeferredShader(gDeferredWeatherRainProgram);
    }
    rain_target->flush();

    if (low_resolution) {                 // depth-aware bilateral upsample -> screen
        target->bindTarget();
        LLGLDepthTest no_depth(GL_FALSE); LLGLEnable blend(GL_BLEND); LLGLDisable no_scissor(GL_SCISSOR_TEST);
        gGL.setSceneBlendType(LLRender::BT_ADD); gGL.setColorMask(true, false);
        bindDeferredShader(gDeferredWeatherRainUpsampleProgram);
        gDeferredWeatherRainUpsampleProgram.uniformMatrix4fv(LLShaderMgr::INVERSE_PROJECTION_MATRIX, 1, false,
                                                             glm::value_ptr(inverse_projection));
        // RESERVED-uniform, INDEX overload — NOT the string overload:
        const S32 rain_channel = gDeferredWeatherRainUpsampleProgram.bindTexture(
            LLShaderMgr::WEATHER_RAIN_MAP, &mWeatherRainHalf, false, LLTexUnit::TFO_BILINEAR);
        gDeferredWeatherRainUpsampleProgram.uniform2f(sRainMapRes,
            (F32)mWeatherRainHalf.getWidth(), (F32)mWeatherRainHalf.getHeight());
        mScreenTriangleVB->setBuffer(); mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
        if (rain_channel >= 0) gDeferredWeatherRainUpsampleProgram.unbindTexture(LLShaderMgr::WEATHER_RAIN_MAP);
        unbindDeferredShader(gDeferredWeatherRainUpsampleProgram);
        target->flush();
    }
}
// draw_lightning: similar, direct into target; uses weather_world_to_view / weather_view_projection.
// End of renderWeather restores: gGL.setColorMask(true,true); gGL.setSceneBlendType(LLRender::BT_ALPHA);
```

### 8.4 Reserved-uniform pattern (the §3.3 rule, worked example)
```cpp
// indra/llrender/llshadermgr.h — EShaderUniforms enum, appended before END_RESERVED_UNIFORMS:
WEATHER_RAIN_MAP,        // "weather_rain_map"   (index 451)
// indra/llrender/llshadermgr.cpp — initAttribsAndUniforms(), matching ordinal push_back:
mReservedUniforms.push_back("weather_rain_map");   // must align 1:1 with the enum (size assert)
// Any NEW weather sampler (e.g. a splash mask) follows this exact pattern; bind by index, never by string.
```

### 8.5 `alweathermodel.h` — controller interface (context; lightning is stateful, rain is pure shader)
```cpp
namespace ALWeatherModel {
  struct Position { F64 mX, mY, mZ; };
  struct Config   { bool mEnabled; F64 mRatePerMinute, mFlashDurationSeconds, mBoltDurationSeconds,
                    mMinDistanceMeters, mMaxDistanceMeters, mBoltHeightMeters; U64 mSeed; };
  struct Frame    { bool mActive, mStrikeStarted; F32 mFlash, mBolt; Position mStrikeBase;
                    F64 mBoltHeightMeters; U64 mStrikeId; };
  class Controller {                       // fed presentation time (frame-rate-independent)
    Frame update(F64 now_seconds, const Config&, const Position& anchor);
    bool  trigger(F64 now_seconds, const Config&, const Position& anchor);
    void  reset(U64 seed = /*fixed*/);
  };
}
// Rain has no controller — if a splash/wetness pillar needs cross-frame state or event scheduling,
// prefer a stateless presentation-time + world-hash formulation (deterministic) over adding CPU state.
```

---

## 9. Summary for the agent
Extend a **working, in-world-verified** procedural rain into a 3D, splashing, alive system by: (A) enriching the existing world-anchored ray-march (layers, perspective streaks, gusts), (B) adding a procedural **surface-response** pass (splash rings + micro-splashes from the deferred normal/depth, world-hash × presentation-time), and (C) wetness/mist/lens toggles — all **default-off, deferred, fail-soft, HDR-bounded, NaN-safe, reserved-sampler-correct, matrix-coherent, presentation-time-deterministic, client-only, and Director-Console-superset**. **Generate code + patch + self-review. Do not build.**
