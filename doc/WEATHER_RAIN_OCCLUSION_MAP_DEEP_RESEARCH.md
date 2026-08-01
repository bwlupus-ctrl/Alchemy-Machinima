# Weather Rain — Sky-Visibility / Rain-Occlusion Map — Deep Research & Codegen Brief

**Repo:** `I:\alchemy-machinima` (Alchemy-Machinima, a Second Life viewer fork).
**Goal:** make rain **and** the surface splashes/wetness respect **cover** — a point under a roof, awning, bridge, or overhang should not receive rain or splashes. Today the effect has **no sky-visibility input**, so sheltered up-facing floors still get rain streaks + splash rings ("rain indoors"). This brief is for an external research/codegen agent (GPT/Codex) to design and **GENERATE CODE (no build)** for a per-pixel **rain-occlusion map**. A Codex/Opus adversarial review + a Claude build follow.

> ⚠️ **Task:** deep-research the technique, choose an approach, and **produce code + a git-appliable patch + a self-adversarial review. DO NOT BUILD.** This is an enhancement on top of the working, in-world-verified weather system; preserve all existing behavior behind the same default-off gate. Every invariant in §3 is mandatory — especially the RESERVED-SAMPLER rule, because this feature *does* add a new sampler and that is exactly the class of bug that recently cost multiple debug cycles.

---

## 1. Context — what exists (build on it, don't rewrite)

The weather system is committed at baseline `fe63cb18bf4` (develop) plus an applied 3D-rain enhancement. Full design + embedded code is in **`doc/WEATHER_RAIN_3D_SPLASH_DEEP_RESEARCH.md`** (the prior brief) and the actual sources:
- `indra/newview/app_settings/shaders/class1/deferred/weatherRainF.glsl` — world-anchored, depth-clamped procedural rain ray-march (multi-layer parallax). Reconstructs world pos via `weather_inv_modelview`; composites bounded additive into `mRT->screen`.
- `indra/newview/app_settings/shaders/class1/deferred/weatherSurfaceF.glsl` — procedural splash rings/crowns + wet Fresnel sheen + mist + lens, gated on the **deferred G-buffer normal** (`getNorm`) being up-facing + a distance gate. Also world-pos-reconstructed.
- `indra/newview/pipeline.cpp` `LLPipeline::renderWeather()` — runs in `renderFinalize` before exposure/bloom; matrices from `gGLLastModelView/gGLLastProjection` + explicit `INVERSE_PROJECTION_MATRIX` override; presentation-time clock (`LLPresentationTime::currentFrame().presentation_time`); default-off master `AlchemyWeatherEnabled`.
- Shared UI: `panel_weather_settings.xml` + `alpanelweathersettings.cpp` (Lightbox + Director Console).

**Also landing separately (coordinate, don't collide):** a *coarse* camera-shelter gate — a single CPU raycast straight up from the camera each frame that fades the WHOLE effect when the camera is under cover. The occlusion map is the **per-pixel upgrade**: it lets *individual world points* be sheltered or exposed (partial cover, eaves, standing in a doorway), which the coarse camera gate cannot. Design the occlusion map to **supersede or compose with** the coarse gate (e.g., coarse gate as a cheap early-out / fallback when the map is disabled).

---

## 2. The technique to research — top-down rain-occlusion (sky-visibility) map

Standard approach (Tatarchuk's SIGGRAPH rain course uses a "rain occlusion"/height map): render the **height of the nearest occluder above each XY** into a texture from a camera **looking straight down** over the area around the player, then in the rain + surface shaders test whether each shaded world point is **below** that occluder height (→ sheltered) or has clear sky (→ exposed).

Design decisions to make (with rationale):
- **The map render:** an **orthographic top-down** pass over nearby geometry, centered on (and moving with) the camera/agent, covering an XY extent ≈ the rain max distance (e.g., 128 m), at a modest resolution (e.g., 512² or 1024²). Store either the max occluder **world-Z** (a height map, R32F) or a depth you can convert to world-Z. Research the cheapest *correct* occluder source:
  - (a) a **dedicated minimal ortho depth pass** over nearby static + rigged geometry (bound the draw set to a radius); or
  - (b) **reuse existing shadow infrastructure** — the viewer already renders sun shadow maps; evaluate whether a straight-down "sky shadow" cascade can be added cheaply or an existing buffer repurposed. Prefer reuse if it's clean; otherwise a dedicated small pass.
- **The test (in weatherRainF + weatherSurfaceF):** project the shaded world point's XY into the occlusion map, sample the occluder height `H`, and compute an **exposure factor** = smoothstep so that `point.z >= H - bias` → exposed (1), well below → sheltered (0), with a soft margin to avoid hard edges. Multiply rain contribution + splash/wetness by this factor. For the RAIN ray-march, apply the test at each sample's world point (so rain fades out under the roofline correctly in 3D), or at least at the surface hit — research which reads best without tanking perf.
- **Edge cases:** points outside the map extent → treat as exposed (or clamp/fade at the border); no occluder above → exposed; the agent's own avatar should not self-shelter (consider excluding the avatar or a small radius). Moving the ortho center with the camera means the map must be re-rendered as the camera moves — keep it stable/quantized to avoid shimmer.

---

## 3. HARD CONSTRAINTS & INVARIANTS (mandatory — same as the weather system, plus the new-sampler rule)

1. **Default-off, zero cost.** New feature behind its own enable (e.g. `AlchemyWeatherRainOcclusion`, default on *inside* the default-off weather master, or default off — propose). When the weather master is off, or this feature is off, **nothing** allocates or renders (no occlusion RT, no pass). The `AlchemyWeatherEnabled` early-return stays first.
2. **RESERVED-SAMPLER RULE (load-bearing — this is the feature that adds a sampler).** The occlusion map's sampler MUST be added to `EShaderUniforms` in `indra/llrender/llshadermgr.h` (append before `END_RESERVED_UNIFORMS`) and `mReservedUniforms.push_back("exact_name")` in `llshadermgr.cpp` at the matching ordinal (size assert), and bound with the **S32/index** `bindTexture(LLShaderMgr::YOUR_ENUM, …)` overload. **NEVER** the string `bindTexture("name", …)` overload — it silently binds texture unit 0 (a non-reserved sampler was exactly the recent "whole-frame wash" bug; `mapUniform` now emits a warning for unreserved samplers, and the string overloads were deleted). Also pass the top-down **view-projection matrix** as a uniform to project world→map UV.
3. **Deferred/HDR only + fail-soft.** If any new shader/program fails to compile, gate on `isComplete()`; the feature disables itself and never breaks the existing rain/lightning/surface passes or deferred rendering.
4. **HDR + NaN discipline unchanged.** The occlusion factor only *attenuates* existing bounded terms (it multiplies in [0,1]), so it cannot increase the additive envelope — but guard the projection/sample math (divide-by-zero, out-of-range UV, NaN) so it can't produce a bad factor.
5. **Matrix coherence + determinism.** Reconstruct with the same `gGLLast*` + `INVERSE_PROJECTION_MATRIX` discipline. The occlusion render is a geometry pass — deterministic given camera + scene. Any animation/quantization must derive from presentation time; do not introduce frame-rate-dependent update cadence that would desync a machinima take (if you update the map every-N-frames for perf, quantize by presentation time, and document the reproducibility tradeoff).
6. **State hygiene.** The new ortho pass must fully save/restore GL state (viewport, FBO, matrices, blend, colorMask, depth) and not leak into the deferred/post chain. Nest `bindTarget`/`flush`. Release the RT on disable/resize.
7. **Client-only. GL 3.3–4.6 core. class1 shaders + stock `postDeferredNoTCV.glsl` vertex stage** for the sampling side (the ortho render itself uses whatever minimal depth program is appropriate).
8. **UI = Director-Console-superset.** Enable + resolution/extent/bias/softness controls go in the shared `panel_weather_settings.xml` (both floaters), preset/reset-backed, real consumers, auto-hide group — and include a per-slider reset button matching the panel's (newly added) reset-button pattern.

---

## 4. Performance & quality
- One extra top-down geometry pass over *nearby* geometry at modest resolution. Bound the draw set (radius) and expose resolution + extent knobs + an enable. Estimate cost; provide an LOD/quality tier. Consider whether the map can be lower-res than the screen (it can — cover changes are low-frequency spatially).
- The per-pixel test in the rain/surface shaders is a few texture reads + a smoothstep — cheap.

## 5. Deliverable
Design writeup (occluder-source choice + why; the exposure-test math; edge-case handling) + **generated code**: the ortho occlusion pass + RT + reserved sampler (llshadermgr) + matrix uniform in `pipeline.cpp`; the occlusion test folded into `weatherRainF.glsl` + `weatherSurfaceF.glsl`; new settings + shared-panel controls (with reset buttons). Plus a **git-appliable patch** and a **self-adversarial review** against §3 (esp. the reserved-sampler compliance, default-off, fail-soft, state hygiene, determinism). **No build/link/deploy.**

## 6. Summary
Add a top-down rain-occlusion (sky-visibility) map so rain and splashes stop under roofs/cover, per-pixel. New RT + **properly reserved** sampler + a bounded top-down geometry pass; multiply an exposure factor into the existing bounded rain/surface contributions. Default-off, fail-soft, matrix-coherent, presentation-time-deterministic, client-only, Director-Console-superset. **Generate code + patch + self-review; do not build.**
