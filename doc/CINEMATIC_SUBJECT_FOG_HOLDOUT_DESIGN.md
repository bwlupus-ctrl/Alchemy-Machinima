# Cinematic Volumetrics — Subject-Aware Fog/Cloud Holdout (+ ReShade avatar mask)

Status: **DESIGN** (nothing built). Suggested branch: `feature/cine-subject-fog-holdout` (separate from
the gaze work). Touches the shared G-buffer + the ReShade ABI → **Codex-reviewed build**.

> **SCOPE CONFIRMED (author, in-world):** the veil is the **projector-cone volumetrics**. The **froxel
> air fog** and the **local volumetric fog** are **not used** — both are OUT OF SCOPE (no holdout
> needed there). §5 below is retained only as a note; the build targets the projector volumetric only
> (§4) + the avatar flag (§3) + the ReShade mask (§6).

## 1. Problem

Volumetric **fog/cloud medium drifts in front of the subject and veils it**, contaminating the
silhouette so it can't be cleanly graded. Confirmed by an ungraded frame: a backlight sits *behind*
the left avatar (rim on hair/shoulders is correct), but the participating medium (height fog + noise
clouds, and/or the froxel air fog) extends forward and washes a low-contrast veil over her edge and
the mid-ground. A second front light stacks it additively.

This is **not** the rays, the rim, or the base-cone depth clamp — those are correct:
- The march already terminates at the scene surface (`t1 = min(b+sq, t_surface)`,
  [projectorVolumetricF.glsl:335](indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricF.glsl:335)).
- Occluders self-shadow the beam ([:533](indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricF.glsl:533)).

**Depth alone cannot fix it** because the veiling medium is *genuinely closer to camera than the
avatar* — there is real in-scatter between camera and subject. The only realtime cure is to make the
medium **subject-aware**: suppress the fog/cloud in-scatter *in front of avatar pixels*, keeping the
beam everywhere else (the gate, arch, background) and keeping the backlight rim on the avatar.

## 2. Goal

- Suppress the volumetric **in-scatter/medium** (projector fbm-noise + dust + height-fog, and the
  froxel air fog) on pixels whose **frontmost surface is an avatar**, by a tunable strength
  (0 = today's look, 1 = fully clean silhouette). Keep the base cone/rays/rim elsewhere and the
  backlight rim on the subject.
- **Automatic** (no manual per-shot tagging) — driven by an avatar G-buffer signal.
- Works for **any light configuration** and any number of lights (each volumetric pass tests the same
  avatar mask), and for **clone/ghost avatars** (they render through the avatar deferred path).
- **Bonus:** the same avatar signal already rides to ReShade (your `llreshadebridge` publishes the
  normals buffer), so ReShade presets get a **clean per-pixel avatar mask** to grade the subject.
- Default **off / strength 0 ⇒ byte-identical** to today.

## 3. The avatar signal — G-buffer encoding (the low-blast-radius trick)

Current encoding ([globalF.glsl:55-61](indra/newview/app_settings/shaders/class1/deferred/globalF.glsl:55),
[llshadermgr.cpp:646-649](indra/llrender/llshadermgr.cpp:646)):
- `encodeNormal(n, env, flag) → vec4(octNormal.xy, env, flag)` — flag in **`normal.w`**.
- Flags are discrete floats: `HAS_ATMOS = 0.34`, `HAS_PBR = 0.67` (SKIP_ATMOS / HAS_HDRI are other
  buckets — **confirm the full set at build time**), matched by `GET_GBUFFER_FLAG(d,f) = abs(d-f) < 0.1`.
- Avatars currently write `GBUFFER_FLAG_HAS_ATMOS` (0.34) at
  [avatarF.glsl:55](indra/newview/app_settings/shaders/class1/deferred/avatarF.glsl:55).

**Approach — an in-band sub-value (no consumer changes):** define
`GBUFFER_FLAG_AVATAR ≈ 0.30`, and have the avatar deferred write it instead of 0.34. Because
`abs(0.30 − 0.34) = 0.04 < 0.1`, avatars **still pass the coarse `HAS_ATMOS` test**, so **every existing
atmospherics/lighting consumer is byte-unchanged** (avatars keep fog/atmospherics; ~20 call sites like
[softenLightF.glsl:167](indra/newview/app_settings/shaders/class3/deferred/softenLightF.glsl:167),
`luminanceF`, the light shaders — all untouched). The volumetric + ReShade distinguish avatars with a
**tighter** test: `abs(normal.w − 0.30) < 0.02`.

Why this is the right call:
- **Zero blast radius** — only `avatarF.glsl` changes the value it writes; no atmospherics consumer
  edits, no new render target, no new flag bucket to thread through 20 shaders.
- **ReShade-safe** — the normal *direction* (`.xy`) is untouched (what ReShade effects consume); only
  `.w` moves 0.04 within a band ReShade doesn't interpret. And it *gives* ReShade the mask (§6).

Build-time verifications (in the Codex pass):
1. `0.30` sits clear of every *other* flag's ±0.1 band (esp. SKIP_ATMOS / HAS_HDRI) — pick the free
   sub-band once the full set is confirmed; adjust the constant if 0.30 collides.
2. Precision: normals are `GL_RGBA16` on the HDR path (ample) and a lower-precision format on non-HDR
   ([llreshadebridge.cpp:250-252](indra/newview/llreshadebridge.cpp:250)). In RGBA8, `.w` steps ≈ 1/256
   ≈ 0.004, so the 0.04 gap is ~10 steps — distinguishable, but confirm the fine test tolerance
   (`0.02`) is robust on the non-HDR format, else widen the separation.
3. Which writers get the avatar value: `avatarF.glsl` (main), and decide on `impostorF.glsl`
   ([:57](indra/newview/app_settings/shaders/class1/deferred/impostorF.glsl:57)) for impostor avatars.
   Clones/ghosts render via the avatar path → covered automatically.

## 4. Holdout — projector volumetrics (`projectorVolumetricF.glsl`)

The veil over an avatar pixel is the in-scatter accumulated along the whole in-front segment
(`accum`, [:429/:646](indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricF.glsl:646)).
Insertion: fetch the capping surface's flag once (`getNorm(tc).w`, already available — the rim block
already calls `getNorm(tc)` at [:709](indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricF.glsl:709)),
compute `avatar = fineTest(norm.w)`, and after the march scale the accumulated **shaft in-scatter** by
`(1 − ProjVolSubjectClear * avatar)` **before** the rim is added:

```glsl
float subj = ProjVolSubjectClear * avatarMask(getNorm(tc).w);   // 0..1
shaft = accum * march_scale * color * (1.0 - subj);             // veil suppressed over avatars
// ... rim block unchanged: rim is the real backlight ON the avatar, we KEEP it ...
shaft += rim;
```

- Suppresses the full front veil (base cone + fbm noise + dust + height fog) over the subject, while
  **keeping the rim/backlight** (added after) so the subject still reads as lit by the scene.
- Everywhere else (gate, arch, background surfaces, sky) `avatar = 0` → **unchanged beam/atmosphere**.
- `ProjVolSubjectClear = 0` ⇒ identity (byte-identical default).
- **Feather** the mask (`avatarMask` returns a soft 0..1 via `smoothstep` on the fine-test distance, or
  a small depth-aware edge feather) so the subject isn't a hard cutout hole in the smoke.

Alternative (if a per-sample decision is wanted rather than a whole-pixel scale): fold `(1 − subj)`
into the per-sample `density` at [:560](indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricF.glsl:560)
— but the whole-pixel scale on `accum` is cheaper and sufficient since the entire in-front segment
belongs to the same capping surface.

## 5. Holdout — froxel air fog (`froxelInjectF.glsl` + its resolve) — OUT OF SCOPE

**Author confirmed the froxel air fog and the local volumetric fog are not used**, so this holdout is
NOT built. Retained only as a note in case froxel fog is adopted later; if so, the same pattern (scale
the froxel fog contribution by `(1 − FroxelSubjectClear * avatarMask(norm.w))` at the froxel resolve)
applies. For this build, skip §5 entirely.

~~The drifting haze in the reference frame is plausibly the **froxel air fog**~~ (`BDMergeFroxelDensity`,
[froxelInjectF.glsl:40-41](indra/newview/app_settings/shaders/class1/deferred/froxelInjectF.glsl:40)),
not only the projector cones. The froxel grid is integrated front-to-back and composited per-pixel at
its **resolve** stage (locate the froxel resolve/composite shader in the build — it reads scene depth
and applies the integrated fog onto the scene color). Insertion there mirrors §4: at the per-pixel
resolve, scale the froxel fog contribution by `(1 − FroxelSubjectClear * avatarMask(norm.w))` so the
air fog stops veiling avatars too. Same setting family, same default-off guarantee.

(If the froxel fog is the dominant source of the veil, this is the more important of the two
insertions — worth confirming in-world which system the drifting haze comes from.)

## 6. ReShade avatar mask (free, via the existing bridge)

`llreshadebridge` already publishes the **normals** buffer to ReShade
([llreshadebridge.cpp:222](indra/newview/llreshadebridge.cpp:222)), so once avatars carry the sub-value
the mask is *already there* — no new channel:
- Document in the ReShade ABI header (`llreshadebridgeabi.h`) a constant + helper, e.g.
  `SL_GBUFFER_FLAG_AVATAR (0.30)` and `bool SL_IsAvatar(float nw) { return abs(nw-0.30) < 0.02; }`, so
  presets can pull a clean avatar matte: `float m = SL_IsAvatar(tex2D(SLNormals, uv).w);`
- Lets the user **grade the subject vs the haze in ReShade directly** — arguably the strongest lever
  for the ungraded-veil problem, on top of the in-viewer holdout.
- Optional hardening: if RGBA8 `.w` precision proves marginal for a crisp ReShade matte, expose a
  dedicated 1-channel avatar-mask texture through the bridge instead of relying on `normal.w`. Prefer
  the free `.w` route first; only add a channel if precision demands it.

## 7. Settings / UI

- `BDMergeProjVolSubjectClear` (F32, 0..1, default 0) — projector-volumetric subject holdout strength.
- `BDMergeFroxelSubjectClear` (F32, 0..1, default 0) — froxel-fog subject holdout strength.
- `BDMergeSubjectClearFeather` (F32, default small) — silhouette softness so it isn't a hard hole.
- Surface in the Cinematic Light Rig floater (near the volumetric/fog controls). Consider one master
  "Subject Clear" slider driving both, plus advanced per-system overrides.

## 8. Compatibility / defaults

- **Strength 0 ⇒ byte-identical** rendered image (the holdout multiplies by 1).
- **Avatar flag shift is byte-identical to all atmospherics/lighting consumers** (0.30 still passes the
  coarse HAS_ATMOS test) — this is the acceptance gate for the G-buffer change; verify with a golden
  frame that no non-volumetric pixel changes.
- **ReShade**: normal direction (`.xy`) untouched → existing presets unaffected; `.w` gains a
  documented avatar sub-value they can opt into.

## 9. Risks

- **Flag sub-band collision / precision** (§3 verifications) — the main risk; pick the free bucket and
  confirm the fine tolerance on the non-HDR normals format.
- **Which system is veiling** — projector cones vs froxel air fog vs EEP atmospheric fog. The holdout
  covers the first two; **EEP/environment fog is a separate system** the G-buffer holdout does *not*
  touch. Confirm in-world (toggle the froxel density / projector volumetrics) which source dominates
  the drift; if it's EEP haze, that's a different (larger) conversation.
- **Hard cutout** — without feathering, a fully-cleared avatar reads as a smoke hole; ship the feather.
- **Impostors** — decide whether impostor-LOD avatars should carry the flag (avoid a pop when an actor
  crosses the impostor threshold mid-shot).
- **Shared G-buffer + ReShade ABI** — core surface; Codex pass mandatory.

## 10. Build plan (Codex-reviewed, own branch)

1. **Avatar G-buffer flag** — confirm the free sub-band, add `GBUFFER_FLAG_AVATAR`, write it in
   `avatarF.glsl` (+ impostor decision), add the fine-test helper to `gbufferUtil.glsl`. Golden-frame
   verify atmospherics byte-identical. *(critical → Codex)*
2. **Projector-volumetric holdout** (§4) + `ProjVolSubjectClear` + feather settings. Default 0 = identical.
3. ~~Froxel-fog holdout~~ — **dropped** (froxel + local volumetric fog unused).
4. **ReShade ABI mask** (§6) — constant + helper in `llreshadebridgeabi.h`; document for presets.
5. **UI** — Cinematic Light Rig floater controls + feather.

Each stage is default-off and independently revertible.
