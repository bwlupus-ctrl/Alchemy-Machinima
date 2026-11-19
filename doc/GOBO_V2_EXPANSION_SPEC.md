# GOBO v2 EXPANSION — IMPLEMENTATION SPEC

**On disk:** `I:\alchemy-machinima\doc\GOBO_V2_EXPANSION_SPEC.md`
**Run mode:** Codex **`--write`**, ONE batch, `--cwd I:\alchemy-machinima`. PowerShell 5.1 (`Select-String`, `;`, `if ($?)`; no `grep`/`&&`).
**Build:** Claude builds Release; never Codex. **Do NOT build.**
**Grounding:** builds directly on the shipped v1 gobo system (procedural patterns 0–6, `sampleGobo`/`proceduralGobo`/`animatedGoboUV`, `gobo_time`/`gobo_pattern`/`gobo_anim_params`, `LLPipeline::GoboOverride` + `sGoboOverrides`, Lightbox Selected-Light UI). **Re-verify every symbol with `git grep` before use** — line numbers are hints. No `.md` authored by you. No TODO/placeholder on added lines.

## 0. OBJECTIVE
Extend the working procedural gobo engine with: (A) **colored gels/tint**, (B) **per-pattern variation params**, (C) **~6 new patterns**, (D) **more animation modes**. Client-render-only, per-projector, session-only overrides — exactly like v1. **Zero regression:** with default values every existing projector (and every gobo already assigned) renders byte-identical to today.

## 1. HARD RULES (acceptance criteria)
1. **Off-path parity is #1.** `GoboOverride` defaults must produce today's output: new `gobo_tint` default = `(1,1,1)` (no color change), new `gobo_pattern_params` default = `(0,0,0,0)` and **each existing pattern 0–6 must reproduce its exact current constants when `gobo_pattern_params == vec4(0)`**. Non-overridden projectors (`getGoboOverride` returns a default `GoboOverride`) and `gobo_pattern=-1` (texture path) stay unchanged except an intentional white-tint multiply that is a no-op. New anim modes (3–6) only affect output when selected; modes 0–2 unchanged.
2. **Shared path:** all new behavior lives in `deferredUtil.glsl`'s `sampleGobo`/`proceduralGobo`/`animatedGoboUV` (+ a new intensity helper) so surface (`spotLightF`), volumetric beam (`projectorVolumetricF` `projGoboTexture`), and froxel inject all inherit identically. Do not fork per-consumer.
3. **Uniform lockstep:** the 2 new uniforms get enum in `indra/llrender/llshadermgr.h` + matching `mReservedUniforms.push_back` at the SAME relative index in `llshadermgr.cpp` `initAttribsAndUniforms()`, GLSL decl, and upload in BOTH `setupSpotLight` and `setupSpotLightVolumetric`. Model on the existing `gobo_time`/`gobo_pattern`/`gobo_anim_params` block.
4. **Clock:** animation continues to use `gobo_time` (already `LLPresentationTime::currentFrame().presentation_time` wrapped). No new clock.
5. **Session-only, local:** overrides never touch `LLVOVolume`/sim. Extend `clearGoboOverride`/`clearVolumetricShafts` coverage as already wired (no change needed if using the existing map).
6. **Cost:** intensity modes and tint are scalar/vec3 multiplies — cheap, fine in all passes. New patterns stay analytic with small fixed loop bounds; keep any noise ≤ 2 octaves. No multi-tap texture work added outside the existing surface CA path.

## 2. NEW UNIFORMS (register per §1.3)
- `gobo_tint` (vec3) — per-projector cookie color multiply. Default `(1,1,1)`.
- `gobo_pattern_params` (vec4) — per-pattern variation. Convention: **each component 0 = built-in default**. Suggested mapping (per pattern, document in comments): `x` = detail/count, `y` = thickness/tilt, `z` = softness/edge, `w` = invert(≥0.5)+contrast. Default `(0,0,0,0)`.

Enum symbols e.g. `GOBO_TINT`, `GOBO_PATTERN_PARAMS`, placed in the `GOBO_*` block after `GOBO_ANIM_PARAMS`.

## 3. C++ — GoboOverride + upload + clamps (pipeline.h / pipeline.cpp)
- Extend `struct GoboOverride` (pipeline.h ~1716): add `LLColor3 mTint = LLColor3(1.f,1.f,1.f);` and `LLVector4 mPatternParams;` (default zero).
- `setGoboOverride` clamps: widen `mPattern` clamp to `[-1, 12]`; widen `mAnimMode` clamp to `[0, 6]`; clamp `mPatternParams` components to sane ranges (e.g. `[-1,1]` or `[0,1]` per component — pick and document; ensure 0 stays representable); `mTint` clamp each channel `[0,1]` (or allow >1 for overdrive — clamp `[0,4]`, default 1).
- Upload in **both** `setupSpotLight` and `setupSpotLightVolumetric`, next to the existing gobo uploads: `shader.uniform3fv(LLShaderMgr::GOBO_TINT, 1, ...)` and `shader.uniform4fv(LLShaderMgr::GOBO_PATTERN_PARAMS, 1, ...)`. Non-overridden projectors upload the defaults (white / zero) → parity.

## 4. SHADER (deferredUtil.glsl)
### 4.1 Restructure `sampleGobo` for tint + intensity
Currently `sampleGobo(uv,lod)` early-returns the procedural or texture sample. Restructure so BOTH branches produce a `vec4 col`, then apply, once, before return:
```
col.rgb *= gobo_tint;              // default (1,1,1) = no-op
col.rgb *= goboAnimIntensity();    // default 1.0 = no-op
return col;
```
Keep the `#ifndef FXAA_GLSL_120` texture fallback intact.

### 4.2 `goboAnimIntensity()` — new helper (intensity-space anim modes)
```
float goboAnimIntensity();  // reads gobo_anim_params + gobo_time
```
- mode (gobo_anim_params.x): `!=5 && !=6` → return `1.0` (parity for none/rotate/wind/pan/pulse).
- mode `5` FLICKER: candle/fire — e.g. `mix(0.6, 1.0, goboValueNoise(vec2(gobo_time*max(speed,0.001)*3.0, 0.0)))` (1-D noise flicker). Never <0.
- mode `6` STROBE: `step(fract(gobo_time*max(speed,0.001)), 0.5)` (50% duty). Speed = gobo_anim_params.y.

### 4.3 Extend `animatedGoboUV` — UV-space anim modes
Keep the existing off-path early-return (mode<0.5 && zoom==1). Extend the mode branches (gobo_anim_params.x):
- `1` rotate — unchanged.
- `2` wind-sway — unchanged.
- `3` PAN/scroll: `uv += normalize(vec2(1.0, 0.35)) * gobo_anim_params.y * gobo_time * 0.1;` (fixed diagonal; small factor so speed 1 is gentle).
- `4` PULSE/breathe: oscillate the zoom — effective zoom = `gobo_anim_params.z * (1.0 + 0.25*sin(gobo_anim_params.y * gobo_time))`; apply the existing zoom-about-center using this effective value.
- Flicker/strobe (5/6) do nothing to UV (handled in 4.2).

### 4.4 `proceduralGobo(int id, vec2 uv, vec4 pp)` — add pp + new patterns
- **Change signature** to take `vec4 pp` (update the `sampleGobo` call to pass `gobo_pattern_params`).
- **Re-parameterize existing ids 0–6** so `pp == vec4(0)` reproduces the CURRENT constants EXACTLY. Example convention: blinds slat frequency `= 12.0 * exp2(pp.x)` (pp.x=0 → 12.0); edge softness offset by `pp.z`; etc. After computing `pattern`, apply global invert/contrast from `pp.w`: `if (pp.w >= 0.5) pattern = 1.0 - pattern;` (or a smooth contrast remap where pp.w=0 is identity — pick one, keep pp=0 identity). **Verify each pattern's pp=0 output is unchanged from v1.**
- **Add new patterns (ids 7–12), pure analytic functions of `uv` (+pp), grayscale `.rgb`, `.a=1`:**
  - `7` Prison/heavy vertical bars — thick vertical `step` bars, count from pp.x.
  - `8` Cathedral arched window — rectangular multi-pane with a semicircular arch mask at the top (combine pane bars with a circle SDF for the arch).
  - `9` Iris / vignette framing — soft circular (or oval via pp.y) aperture: `pattern = 1.0 - smoothstep(r0, r1, length(uv-0.5))`; radius/softness from pp. Doubles as a framing/barn-door-style crop.
  - `10` Dot matrix / disco grid — array of dots: cell via `fract(uv*count)`, `pattern = 1.0 - smoothstep(rad, rad+soft, length(cell-0.5))`; count from pp.x.
  - `11` Concentric rings / starburst — radial: rings `= abs(fract(length(uv-0.5)*count)-0.5)`; optional angular spokes from `atan` for starburst (pp.y blends rings↔spokes).
  - `12` Cloud / god-ray soft breakup — 2-octave `goboValueNoise` soft threshold (softer than foliage id 5), for dappled god-ray texture.
  - (Optional `13` Rain-on-glass streaks — vertical streaks scrolled by `gobo_time`; include only if trivial.)

## 5. UI (floater_lightbox_settings.xml + alfloaterlightbox.cpp)
- **Pattern combo `sl_gobo`:** add entries for ids 7–12 (values 7..12) with readable labels (Prison bars, Arched window, Iris/vignette, Dot grid, Rings/starburst, Clouds).
- **Anim-mode combo `sl_gobo_animmode`:** add Pan (3), Pulse (4), Flicker (5), Strobe (6).
- **New controls** (place in the Selected-Light tab, matching the existing `sl_gobo*` layout idiom):
  - `sl_gobo_tint` — `color_swatch` (LLColorSwatchCtrl), `can_apply_immediately="true"`, default white. Handler `LightBox.SelGoboTint`.
  - `sl_gobo_var1`, `sl_gobo_var2` — sliders (variation → `pp.x`, `pp.y`), sensible ranges, default at the value that maps to pp component 0.
  - `sl_gobo_soft` — slider (`pp.z`), default → 0.
  - `sl_gobo_invert` — `check_box` (`pp.w` ≥0.5), default off.
  Handlers all route to the existing `onSelGoboChanged` (add them to the registrar block ~63–67 as `LightBox.SelGobo*`).
- **`updateSelectedLightPanel` (~140):** fetch the new widgets with `findChild<>` (null-safe), populate from `getGoboOverride(id)` (`mTint`, `mPatternParams`), gate-enable on `is_projector` like the others.
- **`onSelGoboChanged` (~239):** read the new widgets, fill `GoboOverride.mTint`/`mPatternParams`, call `setGoboOverride`. Keep `clearGoboOverride` clearing all (tint/params reset to defaults via a fresh `GoboOverride`).
- The color swatch needs its ctrl type included/handled; verify `LLColorSwatchCtrl` getValue→LLColor4→LLColor3 conversion.

## 6. VERIFY BEFORE RETURN (reviewer re-runs)
- **Parity:** `gobo_tint=(1,1,1)` + `gobo_pattern_params=(0,0,0,0)` + anim mode 0 ⇒ identical to v1 for texture (-1) AND every pattern 0–6. Confirm each id 0–6 with pp=0 matches its current constants (read the diff).
- **Lockstep:** both new enums have push_back at the matching index; `git grep` resolves `gobo_tint`/`gobo_pattern_params`.
- **Shared:** surface + beam + froxel inject all reach the new tint/intensity/pp via `sampleGobo` (read the call chains).
- **Clamps:** pattern `[-1,12]`, animMode `[0,6]`, tint/params clamped; no out-of-range combo value can index a missing pattern branch (proceduralGobo default returns white/`vec4(1)` for unknown id).
- New UI widget names match XML exactly (getChild/findChild). XML well-formed. No new files needed (no CMake change). No `.md`. No TODO.
- Preserve ALL existing uncommitted working-tree changes (switchboard, 08-12, v1 gobo/fog); edit additively; never git checkout/stash/reset/clean; commit nothing.
- Build-dependent items (compile/link, XUI instantiation, GPU visuals) noted for Claude's Release build.

## 7. ORDER
1. Uniforms + registration (§2,§3 lockstep). 2. `proceduralGobo` pp + parity re-param + new ids (§4.4). 3. `animatedGoboUV` + `goboAnimIntensity` + `sampleGobo` tint/intensity (§4.1–4.3). 4. UI (§5).

## APPENDIX — verified anchors (confirm before use)
- `deferredUtil.glsl`: `goboHash`@104, `goboValueNoise`@109, `proceduralGobo(int id, vec2 uv)`@123, `animatedGoboUV(vec2 uv)`@179, `sampleGobo(vec2 uv,float lod)`@207. Uniforms `gobo_time`/`gobo_pattern`/`gobo_anim_params`@~74.
- `pipeline.h`: `struct GoboOverride`@~1716 `{mPattern=-1,mAnimMode=0,mSpeed=1,mZoom=1,mDispersion=0}`.
- `pipeline.cpp`: `setGoboOverride` clamps (widen); uploads in `setupSpotLight` + `setupSpotLightVolumetric` next to existing `GOBO_TIME`/`GOBO_PATTERN`/`GOBO_ANIM_PARAMS`.
- `llshadermgr.h`/`.cpp`: `GOBO_*` enum block + `mReservedUniforms` `"gobo_*"` block (lockstep).
- `floater_lightbox_settings.xml`: `sl_gobo`@4323, `sl_gobo_animmode`@4335, `sl_gobo_speed`@4342, `sl_gobo_zoom`@4346, `sl_gobo_dispersion`@4350.
- `alfloaterlightbox.cpp`: registrar `LightBox.SelGobo*`@63–67; reads@140–144/166–167; `onSelGoboChanged`@239–264; `clearGoboOverride`@235.
