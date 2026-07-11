# Projector Volumetric Cones — Cinema Roadmap

Post-ship enhancement plan for **G3.3** (per-projector volumetric light cones,
commit `1d2be84c50`; design brief in `BDMERGE_PROJECTOR_VOLUMETRICS_BRIEF.md`).
Goal: take the shipped effect from "works" to **cinema-grade** — more robust,
more filmic, and cheaper per frame. Scoped 2026-07-11 with user direction.

## Current implementation (baseline)
- `renderProjectorVolumetric()` in `pipeline.cpp` (after the sun volumetric block
  in `renderFinalize`), `setupSpotLightVolumetric()` (side-effect-free).
- `class3/deferred/projectorVolumetricF.glsl` (+ class1 stub): bounded ray↔sphere
  march (16 samples default), HG phase (g=0.72), per-slot self-shadowing via
  `sampleSpotShadow`, cookie/gobo modulation, per-slot additive blend.
- Runs **post-`colorCorrect`** (display stage) — the main thing Phase 1 changes.
- Gate `BDMergeProjectorVolumetrics` (default off) + Resolution/Multiplier/Anisotropy.
- Scope limit: all shadow-casting projectors (slots 0..N-1) currently emit shafts.

## Locked design decisions (user, 2026-07-11)
- **Per-projector flag is SESSION-BASED** (cleared on relog), toggled via the
  **right-click context menu** on a selected projector, and **OFF by default** —
  a projector emits a shaft only when explicitly flagged this session.
- **Shafts must be influenced by the tonemapper** (composite in linear HDR before
  tonemapping) and carry the **light's color**.
- Animated medium **noise must stay tasteful** — the naive/heavy noise look is a
  non-goal. Ship it subtle (or off) by default; quality over gimmick.
- Confirmed wanted: **feathered cone edge**, **crisp volumetric occluder shadows**,
  plus the perf/quality and atmosphere items below.

---

## Phase 1 — Quality + performance core (do first)
Mostly self-contained shader/pipeline work; makes it both better *and* faster.

**STATUS 2026-07-11 (Opus): items 1, 2, 4, 5, 6, 7 landed; item 3 (half-res +
bilateral upsample) NOW LANDED (see below); full across-frame temporal
reprojection still deferred.** All gated under the existing
`BDMergeProjectorVolumetrics` master (default off); each lever has a sub-control
with a sensible default. In-world verification still owed (no SL session this pass).

1. **HDR-space composite before tonemapping. — DONE.** `renderProjectorVolumetric`
   now composites additively onto `mRT->screen` (linear HDR) *after*
   luminance/exposure/bloom generation but *before* `colorCorrect`, so the active
   tonemapper rolls off the bright cores instead of clipping (old post-`colorCorrect`
   call removed). Placed after exposure/bloom so shafts don't skew auto-exposure or
   leak into bloom (feeding bloom stays a Phase 3 item). Shader clamp relaxed from a
   tight display-space `PROJVOL_MAX=4` to a generous linear-HDR headroom uniform
   `projvol_max` (`BDMergeProjectorVolumetricsMaxLuminance`, default 8.0; guards
   NaN/inf + fireflies only). Sun godray block (G3.2) left at its display stage,
   untouched. Highest-value item.
2. **Blue-noise dithered march offset (+ temporal). — DONE (spatial); temporal
   reprojection DEFERRED.** Replaced the `sin`-hash jitter with interleaved-gradient
   (blue-noise-like) dither keyed on `gl_FragCoord`. Because it is **static per
   screen-pixel it never crawls/shimmers under camera motion** — the mandatory
   recording requirement is met by construction, without a history buffer.
   Sub-control `BDMergeProjectorVolumetricsDither` (0=off, 1=static blue-noise
   [default], 2=animated: adds a subtle golden-ratio per-frame rotation that
   averages residual banding on still shots). **Deferred:** true across-frame
   temporal accumulation with reprojection/neighborhood-clamp — it needs a
   dedicated half-res history target (tied to item 3) and can't be runtime-verified
   this pass. Static blue-noise + the downstream FXAA/SMAA already give clean,
   shimmer-free shafts; the animated mode is the conservative temporal blend.
3. **Half-res march + bilateral (depth-aware) upsample. — DONE (2026-07-11 Opus).**
   New sub-control `BDMergeProjectorVolumetricsHalfRes` (default **on** for the FPS
   win; off = original fullscreen additive-in-place path, kept intact). When on:
   a half-res RGBA16F scratch target `mProjVolHalf` (allocated on demand in
   `renderProjectorVolumetric`, released in `releaseGLBuffers`) is cleared to black
   and the cones are marched into it at a half viewport (quarter the marched
   fragments). A new program `gDeferredProjectorVolumetricUpsampleProgram`
   (`class1/deferred/projectorVolumetricUpsampleF.glsl`, `isDeferred` for
   `getPosition`) then does a **depth-aware 4-tap bilateral upscale** — each
   full-res pixel gathers the 4 surrounding half-res texels weighted by bilinear
   footprint × view-space depth similarity (`exp(-|Δdepth|/sigma)`), so the shaft
   resolves cleanly without haloing across occluder silhouettes — compositing
   additively onto `mRT->screen` at the exact Phase-1-item-1 placement (after
   exposure/bloom, before `colorCorrect`). Scissor + adaptive still work in the
   half-res march; the upsample is additionally scissored to the 2×-scaled union of
   the marched cone rects. The half-res shaft is bound to the reserved `projectionMap`
   sampler (unused by `bindDeferredShader` in this pass) so it gets a real texture
   channel. Full across-frame temporal EMA/reprojection remains deferred (needs a
   history target + reprojection; not runtime-verifiable this pass) — static
   blue-noise (item 2) keeps the half-res result shimmer-free under motion.
   **Expected: ~3-4× fewer marched fragments; the dominant FPS lever with 2+
   flagged projectors on top of items 4+5.** In-world FPS check owed.
4. **Screen-space scissor to each cone's projected bounds. — DONE.** Each cone's
   sphere-of-influence is projected (8 view-space AABB corners → screen) to a
   conservative pixel rect; `glScissor` restricts the fullscreen march to it. Corners
   at/behind the camera plane → safe full-screen fallback; fully off-screen cones are
   skipped entirely. Sub-control `BDMergeProjectorVolumetricsScissor` (default on).
5. **Adaptive sample count. — DONE.** Per cone, sample count scales with on-screen
   coverage (`sqrt` of projected-box screen-height fraction) between a floor
   (`BDMergeProjectorVolumetricsMinResolution`, default 6) and the full
   `...Resolution`. Uploaded per cone as `godray_res`. Sub-control
   `BDMergeProjectorVolumetricsAdaptive` (default on).
6. **Feathered cone edge. — DONE.** Shader smoothsteps the shaft to zero over the
   last `projvol_feather` fraction of the cookie half-width so the beam boundary is
   soft. Sub-control `BDMergeProjectorVolumetricsFeather` (0–0.5, default 0.15;
   0 = hard edge).
7. **Crisp volumetric occluder shadows. — DONE.** Optional `projvol_shadow_samples`
   (1–4) sub-taps of the spot shadow map *between* march steps, resolving thin
   occluders (bars/foliage) into sharp god-ray bands independent of march density.
   Sub-control `BDMergeProjectorVolumetricsShadowSamples` (default 1 = cheap single
   tap).

## Phase 2 — Art direction (the flag + per-light control)

**STATUS 2026-07-11 (Opus): item 1 landed (core ask); item 2 landed as global
overrides; item 3 DEFERRED.** All still gated under the `BDMergeProjectorVolumetrics`
master; the session flag is an additional filter *within* the enabled effect.
In-world verification still owed (no SL session this pass).

**FIXES 2026-07-11 (Opus):**
- **Menu placement.** The "Volumetric Shaft" `menu_item_check` was moved out of the
  nested *Manage* submenu (where it sat beside Derender) to the **first page of the
  object pie / context menu** (`menu_object.xml`, top level, in its own
  separator-delimited section right after *Build*). The enable callback
  (`enable_object_volumetric_shaft`) was already permission-free — it gates on
  `isLightSpotlight()` **only**, no `canModify`/owner check — so the entry now shows
  and is enabled for **any** spotlight projector regardless of ownership or mod
  permission (the effect is purely client-side: a session UUID set, no server round
  trip). The prior "doesn't apply to no-mod projectors" was the item being buried in
  a submenu, not a permission gate in the callback.
- **Shaft color now follows the projector's Light Color.** Root cause: the shaft's
  chroma was sourced *only* through `getProjectedLightDiffuseColor()`, which bakes
  the light `color` **into** the gobo sample — entangling the light color with the
  cookie texture so it read as not tracking the Light Color. Fix
  (`projectorVolumetricF.glsl`): sample the gobo as **texture-only**
  (`projGoboTexture()`, the same LOD math minus the `color.rgb *`) and multiply the
  light's linear diffuse `color` in **once, explicitly, over the whole shaft**. The
  C++ already uploaded the correct per-cone color (`getLightLinearColor()` via
  `DIFFUSE_COLOR`, parity with stock `setupSpotLight`), and the Phase 2 tint still
  lerps on top of the true light color (`TintStrength>0`).

1. **Session per-projector opt-in. — DONE.** Right-click object context-menu entry
   **"Volumetric Shaft"** (`menu_object.xml`, a `menu_item_check` next to the
   Derender items). It is a **session-only** opt-in: a `std::set<LLUUID>`
   (`LLPipeline::sVolumetricShaftObjects`) with static accessors
   `toggleVolumetricShaft` / `isVolumetricShaftEnabled` / `clearVolumetricShafts`
   on `LLPipeline`. **Not persisted** — cleared on logout/relog via
   `clearVolumetricShafts()` called from `LLAppViewer::disconnectViewer()`, and
   empty at startup so every projector is dark until flagged (OFF by default even
   with the master gate on). Menu plumbing (`Object.VolumetricShaft` commit +
   `Object.EnableVolumetricShaft` / `Object.CheckVolumetricShaft` enable/check
   callbacks in `alviewermenu.cpp`) mirrors `ALDerenderList`'s selection wiring but
   is purely in-memory; the entry enables only when the primary selection is a
   spotlight projector (`isLightSpotlight()`), shows a checkmark when flagged, and
   toggles every root object in the selection. **Render filter:**
   `renderProjectorVolumetric()` now skips any spot-shadow slot whose light object
   isn't flagged. Slot→object mapping: `mShadowSpotLight[i]` (an `LLDrawable*`) →
   `getVOVolume()` → `getID()`, and also its `getRootEdit()->getID()` so a flag set
   on the selected root still matches when the light feature lives on a child prim.
2. **Per-light overrides. — DONE as GLOBAL overrides (per-UUID deferred).** The
   two levers that already existed globally (intensity = `...Multiplier`, cone
   softness = `...Feather`) cover most of the ask; the genuinely new lever is a
   **shaft color tint** decoupled from the light color, added as global settings
   `BDMergeProjectorVolumetricsTint` (Color3) + `BDMergeProjectorVolumetricsTintStrength`
   (0–1, default 0 = pure light color → no-op vs. shipped look). Applied in
   `renderProjectorVolumetric()` by lerping the per-cone `col` toward the tint
   before upload. True **per-UUID** tint/intensity/softness maps deferred as a
   follow-up to avoid ballooning scope (would need the flag set to become a
   `map<LLUUID, params>` + a per-object editor UI); the global versions ship now.
3. **Slot pinning. — DEFERRED.** Biasing `setupSpotLight`'s shadow-slot priority so
   a flagged hero projector keeps its slot touches the shadow-assignment path and
   risks destabilizing it / violating the brief-R1 side-effect-free rule; deferred
   rather than shipped this pass. Sketch for next time: in the slot-priority
   comparison, add a bonus to the effective priority of lights in
   `sVolumetricShaftObjects` so they sort ahead of equidistant non-flagged
   projectors, kept read-only w.r.t. the volumetric setup itself.

## Phase 3 — Atmosphere (tasteful)

**STATUS 2026-07-11 (Opus): ALL FOUR ITEMS LANDED.** Every lever ships gated under
the `BDMergeProjectorVolumetrics` master AND at a **no-op default** (density 1.0,
every strength/feed 0.0) — so Phase 3 is invisible until a lever is deliberately
dialed up. The atmosphere modulations work in BOTH the half-res and fullscreen
march paths (same `projectorVolumetricF.glsl`, uniforms uploaded once per frame).
`setupSpotLightVolumetric` stays side-effect-free; the Phase 2 session-flag
semantics and the Phase 1 tonemapper-shaped HDR composite / light-color-follows
behavior are preserved. In-world verification still owed (no SL session this pass).

1. **Animated 3D noise medium. — DONE (TASTEFUL, default OFF).** The per-sample
   in-scatter is modulated by a **world-anchored** 3-octave value-noise fbm (quintic
   interpolation → soft cloud lobes, not the forbidden blocky/grainy hash). The
   sample's view position is transformed back to agent (world, Z-up) space via a new
   `projvol_inv_modelview` uniform so the motes **sit in the air and drift, never
   crawl with the camera**; a continuous `projvol_time` (seconds) scrolls them
   slowly. The density factor is centred on 1.0 (`1 + strength*(2*fbm-1)`) so it
   varies density symmetrically instead of only dimming. Sub-controls
   `...NoiseStrength` (0–1, **default 0 = OFF**; subtle preset ~0.2–0.35),
   `...NoiseScale` (cycles/m, default 0.15 = large soft clouds), `...NoiseSpeed`
   (m/s, default 0.15 = gentle drift). The whole noise block is skipped when
   strength (and fog) are 0.
2. **Height-based fog density. — DONE (default OFF).** Density falls off with world
   altitude (`wpos.z` in region Z-up): `mix(1, ground_density*exp(-max(h,0)/falloff),
   fog_strength)`, denser at/below the ground reference and thinning up high so
   shafts have atmospheric depth. Sub-controls `...FogStrength` (0–1, **default 0 =
   OFF**), `...FogGroundDensity` (default 1.0), `...FogFalloff` (e-fold metres,
   default 24), `...FogBase` (region-Z ground reference, default 0).
3. **Global density / "haziness" master. — DONE.** `...Density` (0–4, **default 1.0
   = no-op**) — one multiplier scaling the whole participating-medium density for
   per-shot atmosphere. Folded into the same per-sample `density` scalar as items
   1–2.
4. **Feed shafts into bloom (soft glow halo). — DONE (controlled, default OFF).** A
   deliberate, gated feed that does NOT reintroduce uncontrolled bloom leakage: the
   scene bloom pyramid is still generated from the **clean pre-shaft scene** (Phase 1
   placement unchanged). After `renderProjectorVolumetric` leaves the shaft in the
   half-res target `mProjVolHalf`, a new pass (`feedProjectorVolumetricBloom` +
   `class1/deferred/projectorVolumetricBloomFeedF.glsl`, program
   `gDeferredProjectorVolumetricBloomFeedProgram`) **tent-blurs** that half-res shaft
   into a soft halo, scales it by `...BloomFeed`, and composites it **additively into
   the HDR bloom pyramid base `bloomMip[0]`** (RGB only — alpha/halation untouched via
   `colorMask(true,false)`) — the buffer `colorCorrect` samples as the final bloom.
   So only `BloomFeed` worth of shaft becomes a halo. Sub-control `...BloomFeed`
   (0–2, **default 0 = OFF**; modest ~0.15–0.4). Requires HalfRes on (the reusable
   shaft texture) and the HDR path; guarded by `mProjVolHalfValid` (reset each frame,
   set only when the half-res march drew a real shaft) so it can never sample a stale
   texture.

**New uniforms** (registered in `llshadermgr.{h,cpp}`): `projvol_density`,
`projvol_noise_strength/scale/speed`, `projvol_time`, `projvol_fog_strength`,
`projvol_fog_ground_density`, `projvol_fog_falloff`, `projvol_fog_base`,
`projvol_inv_modelview`, `projvol_bloom_feed`. **New settings** (all listed above,
all no-op defaults). **New shader/program** for the bloom feed (reconfigure was run
so the staging manifest picked it up; verify it lands under
`Release\app_settings\shaders\class1\deferred\`).

## Sequencing
Phase 1 → Phase 2 → Phase 3. Phase 1 items 1–2 (HDR composite + dither) are the
biggest quality jump and should land together. Each phase is one or more
revertable commits; keep the existing `BDMergeProjectorVolumetrics` gate as the
master and add sub-controls per item.
