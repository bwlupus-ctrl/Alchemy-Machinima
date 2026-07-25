# The SL → ReShade Bridge Contract — **v2**

**Authoritative specification of the viewer → ABI → add-on → FX → iMMERSE chain.**

| | |
|---|---|
| **Version** | 2 (supersedes v1, 2026-07-25 morning) |
| **Date** | 2026-07-25 |
| **Tree** | `I:\alchemy-machinima` `develop` @ `90b9c8e216d` (local; **not pushed** — see §11.9) |
| **Inputs** | v1 + `SL_RESHADE_MOTION_RETIREMENT_DELIVERABLES` reconciliation ledger (20 items) |

Every fact carries a `file:line` citation, and everything checkable was re-read from source on
2026-07-25. **Where this document and a code comment disagree, this document and the cited source
win** — four wrong code comments and four wrong v1 statements were found on the day it was written.

**Status legend used throughout:** ✅ verified in source · ⚠️ corrected from v1 · ❓ runtime gate
(source cannot settle it) · ⛔ defect, unfixed.

---

## 0. What v1 got wrong

v1 was written the same morning and was wrong in four material places. All four came from **memory
rather than source**. This section exists so the errors are not silently re-absorbed.

| # | v1 claimed | Truth | Evidence |
|---|---|---|---|
| 1 | camera-motion-from-depth is "designed but unbuilt" and a **hard prerequisite** for retiring optical flow | **It is built, registered and drawn** | `llviewershadermgr.cpp:251,3445-3450`; `pipeline.cpp:5997-6015` `[BDMerge A5.4-1c]` |
| 2 | the `0.5` NDC→UV motion factor is "an assumed constant, not measured" | `0.5` is **exact** algebra | §3.3 |
| 3 | `_MARTYSMODS_TAAU_SCALE = 0.66` is the live value | live value is **`.77`** | `ReShade.ini:21` |
| 4 | "current stack is 8bpc, 10-bit shelved" | live config sets **`Force10BitFormat=1`** | `ReShade.ini:7` |

And one omission of my own making: v1 defined the §13 five-artifact change protocol, then **left a
stale `// spheremap .xy` comment** at `SL_Bridge.fxh:33` — in one of the five files. Fixed.

### 0.1 A fifth error — this one in v2, found by research 2026-07-25

| # | v1 **and v2** claimed | Truth | Evidence |
|---|---|---|---|
| 5 | the magenta-clear claim is *"unverified — greps found black clears"* | **The claim is TRUE.** I grepped `pipeline.cpp`; the clear lives elsewhere | `llviewerdisplay.cpp:1020` — `glClearColor(1, 0, 1, 1)` |

**This one matters beyond colour.** The clear's **alpha is 1.0**. `bindTarget()` sets `glDrawBuffers`
across *all* colour attachments (`llrendertarget.cpp:533-545`) and `clear()` is a plain
`glClear(GL_COLOR_BUFFER_BIT|…)` (`:558-585`), so alpha 1.0 lands in the **normals attachment's `.w`**
— colliding exactly with `GBUFFER_FLAG_HAS_HDRI` (1.0).

> ⛔ **Therefore any coverage gate of the form `.w > 0` is UNSOUND** — it admits both unwritten
> (cleared) pixels and HDRI sky. Only a **windowed two-bucket** test is correct:
> ```hlsl
> bool covered = abs(w - 0.34) < 0.1 || abs(w - 0.67) < 0.1;  // HAS_ATMOS or HAS_PBR
> ```
> Windowed compare, never equality — equality breaks in both the HDR and non-HDR formats.

I had proposed the `.w > 0` form in conversation before this was checked. It would have been wrong,
and would have looked like it worked, because sky is usually behind other geometry.

**Related premise of mine that was also wrong:** I assumed `GL_RGB10_A2` (non-HDR) made `.w` unusable
because alpha has only 2 bits. It does not. 2-bit UNORM quantizes to exactly `{0, ⅓, ⅔, 1}`; the flag
values `0.34`/`0.67` land 0.0067 and 0.0033 away from `⅓`/`⅔`, far inside the ±0.1 decode window, and
the level spacing exceeds twice the window so buckets cannot collide. **The flag values were evidently
chosen to be 2-bit-safe.** Proof by dependence: the viewer's own non-HDR deferred lighting branches on
these flags every frame (`softenLightF.glsl:167-206`) and non-HDR mode works.

**The governing lesson, unchanged and now doubly earned:** *"confirmed working in-world" is evidence
of plausibility, not correctness*, and **a claim recalled is not a claim verified**. Two shipped bugs
and four wrong contract statements in one day, every one of them visible in source to anyone who
looked.

---

## 1. The chain

```text
LLRenderTarget (GL, viewer-owned, MUTABLE, level-0 only)
  │  gatherFrame()  — llreshadebridge.cpp, post-renderFinalize / pre-UI   ❓ seam unproven, §11.8
  ▼
SLReShadeFrame  (C struct, seqlock, exported as SLReShade_GetFrame)
  │                                          ← ABI boundary: the ONLY coupling
  ▼
sl_reshade_bridge.addon   copy_texture_region → add-on-owned IMMUTABLE textures
  │                       update_texture_bindings(semantic, srv, srv)
  ▼
FX semantics: SL_NORMALS · SL_MOTION_NDC · SL_ALBEDO · SL_ORM · SL_COLOR_HDR · DEPTH
  │  SL_Bridge.fxh — shared declarations, decode, orientation
  ▼
SL_GBufferProvider.fx → Deferred::NormalsTexV3 / MotionVectorsTex / AlbedoTex
  ▼
iMMERSE consumers (RTGI, MXAO, RCAO …), Launchpad upstream during COEXIST
```

Load order is fixed: **`Launchpad → SL_GBufferProvider → consumers`**.

---

## 2. Layer 1 — what the viewer produces

### 2.1 Render targets ✅
HDR = `RenderHDREnabled` **and** `mGLVersion > 4.05`.

| Slot | Source | HDR | non-HDR | GL enum | Cite |
|---|---|---|---|---|---|
| `albedo` | `deferredScreen` 0 | `GL_SRGB8_ALPHA8` | same | `0x8C43` | `pipeline.cpp:1066` |
| `orm` | `deferredScreen` 1 | `GL_RGBA` (→RGBA8) | same | `0x1908` | `pipeline.cpp:442,457` |
| `normals` | `deferredScreen` 2 | `GL_RGBA16` | `GL_RGB10_A2` | `0x805B`/`0x8059` | `pipeline.cpp:443,452,458` |
| `emissive` | `deferredScreen` 3 | `GL_RGB16F` | `GL_RGB` | `0x881B`/`0x1907` | `pipeline.cpp:444,453,461` |
| `color_hdr` | `screen` 0 | `GL_RGBA16F` | same | `0x881A` | `pipeline.cpp:1071` |
| `motion` | `mVelocityMap` 0 | `GL_RG16F` | same | `0x822F` | `pipeline.cpp:1169` |
| `depth` | `deferredScreen` depth | `GL_DEPTH_COMPONENT24` | same | `0x81A6` | `llreshadebridge.cpp:144` |

Emissive exists only with `RenderEnableEmissiveBuffer` (**default false**); `mVelocityMap` only with
`BDMergeVelocityBuffer` or `BDMergeMotionBlur` (**default off**).

### 2.2 Channel semantics ✅ (`pbropaqueF.glsl:115-120`)
| Att | Contents |
|---|---|
| 0 | diffuse / base colour, **sRGB-encoded storage** |
| 1 | PBR: linear packed **Occlusion, Roughness, Metal**; legacy materials: specular |
| 2 | `encodeNormal(tnorm, env_intensity, gbuffer_flag)` → `.xy` normal, `.z` env, `.w` flags |
| 3 | PBR **sRGB emissive**, or legacy material env intensity |

Attachments 1 and 3 are **not** a universal contract — meaning depends on the PBR vs legacy path.

### 2.3 Normal encoding — OCTAHEDRAL ✅ (`globalF.glsl:46-74`)

```glsl
// viewer, authoritative
vec2 OctWrap(vec2 v) { return (1.0-abs(v.yx)) * vec2(v.x>=0.0?1.0:-1.0, v.y>=0.0?1.0:-1.0); }
vec4 encodeNormal(vec3 n, float env, float flag) {
    n /= (abs(n.x)+abs(n.y)+abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
    return vec4(n.xy*0.5+0.5, env, flag);
}
vec4 decodeNormal(vec4 norm) {
    vec2 f = norm.xy*2.0-1.0;
    vec4 n; n.xyz = vec3(f.x, f.y, 1.0-abs(f.x)-abs(f.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.xy += vec2(n.x>=0.0?-t:t, n.y>=0.0?-t:t);
    n.xyz = normalize(n.xyz); return n;
}
```
```hlsl
// SL_Bridge.fxh — must stay expression-for-expression identical
float3 SL_DecodeNormal(float2 enc) {
    float2 f = enc*2.0-1.0;
    float3 n = float3(f.x, f.y, 1.0-abs(f.x)-abs(f.y));
    float  t = saturate(-n.z);
    n.xy += float2(n.x>=0.0?-t:t, n.y>=0.0?-t:t);
    return normalize(n);
}
```

**Why the old spheremap bug hid:** both agree at `enc=(0.5,0.5)` → `(0,0,1)`; at `enc=(1,0.5)`
octahedral gives `(1,0,0)`, spheremap `(0,0,-1)`. Head-on correct, grazing angles wrong.

**Space:** view/camera space ✅. **Destination convention: ❓ runtime gate** — do not assume iMMERSE
shares axes merely because both are "view space" (§9 N2).
**Precision:** `GL_RGBA16` is 16-bit **UNORM**, not half-float. Non-HDR `GL_RGB10_A2` is materially worse.

### 2.4 Coverage — G-buffer albedo is not valid everywhere ✅
Written by the **deferred opaque/masked** path only. Sky/background, alpha-blended, water, fullbright
and HUD are not covered; at those pixels attachment 0 holds the clear value or **the opaque surface
behind** the transparent one. Architectural, not a bug — and the reason albedo cannot simply replace
Launchpad's estimate without an explicit coverage signal (§4).

---

## 3. Motion architecture ⚠️ **rewritten in v2**

### 3.1 The layered model ✅
Velocity is produced in **two layers, camera first**, into `mVelocityMap`:

1. **Camera-depth fallback** — `gVelocityCameraProgram` (`llviewershadermgr.cpp:251`, built from
   `postDeferredNoTCV.glsl` + `velocityCameraF.glsl` at `:3445-3450`), dispatched at
   `pipeline.cpp:5997-6015` as `[BDMerge A5.4-1c]`. A fullscreen triangle, depth test **off**,
   sampling the shared depth attachment. Fills **every pixel** with camera-induced motion reprojected
   from scene depth, *"so pixels no draw pool stamps (sky, excluded blended alpha, impostors) don't
   read 'static' and ghost in temporal consumers."*
2. **Per-object geometry passes** then overwrite covered pixels with true per-object motion
   (rigid, rigged, classic avatar).

This is the OF replacement. **It needs no ABI matrix export — keep it viewer-side.**

### 3.2 Two defects in the existing pass ⛔
- **Previous projection is wrong.** `velocityV`, `avatarVelocityV` and `velocityCameraF` all use the
  **current** unjittered projection for *both* endpoints. Add `last_projection_matrix_unjittered`
  and reset on discontinuous projection change (FOV animation, aspect change, cut).
- **Sky is mishandled.** The `[A5.4-1c]` comment claims correctness for sky, but far/clear depth
  reconstructs a **finite world point** and therefore receives translation. Sky must be
  **rotation-only**, renderer-provided, or explicitly invalid.

### 3.3 Units ⚠️ corrected
Source (`velocityF.glsl`) is `cur_ndc - prev_ndc`, NDC, forward. iMMERSE reprojects as `uv + motion`,
wanting `(prev_uv - cur_uv)` in top-left UV. Therefore the exact conversion is
**`(-0.5*d.x, +0.5*d.y)`** — the `0.5` is exact NDC→normalized-UV algebra, **not a guess**. Current
code computes exactly this. ❓ Still to prove at runtime: **consumer direction and resource
orientation**, not the factor.

### 3.4 What remains uncovered
**Independently moving non-geometry content** — particles, a moving water surface, animated alpha.
Materially narrower than v1's "sky/water/alpha". Optional dense optical flow, if ever added, must be
**confidence-gated and restricted to native-invalid regions** (occlusion/disocclusion produce
confident-looking garbage otherwise).

⛔ **Launchpad's own OF cannot be re-enabled — it soft-crashes this build.**

---

## 4. Validity is explicit, never inferred ⚠️ **new in v2**

> **Zero motion, black albedo, and `(0,0)` octahedral are all LEGAL DATA** — a stationary surface, a
> black material, an extreme normal direction. **None of them means "unfed".**

Any code using a payload value as an availability sentinel is wrong. Required instead:
- **frame-level semantic availability** (ABI 1.1 tail, §5.4);
- **per-pixel coverage/validity** for albedo (G-buffer coverage ≠ visible-surface coverage);
- **temporal reset signalling** on discontinuities.

⛔ **Current defect:** the add-on **retains the last destination when a source disappears**
(`sl_reshade_bridge.cpp` `update_slot()`), and returns early without rebinding on an invalid global
frame. That silently serves stale pixels. Required: bind neutral or deterministically clear, mark the
semantic invalid, bump the target generation, and request history reset.

---

## 5. Layer 2 — the ABI

`extern "C" const SLReShadeFrame* SLReShade_GetFrame(uint32_t requested_abi);`
Magic `0x534C5242`, major 1, minor 0. NULL on major mismatch. Pointer stable for process lifetime.

### 5.1 Seqlock read protocol — mandatory
```c
do { uint32_t end = frame->write_end;   /* END first */
     /* copy struct */
     uint32_t begin = frame->write_begin;
} while (begin != end);
```

### 5.2 Flags ✅
`VALID 0x1` (`normals && depth` present, `llreshadebridge.cpp:183`) · `HDR 0x2`
(`normals == 0x805B`, `:173`) · `SNAPSHOT 0x4`.
At the login screen `VALID` is unset and the add-on is inert — **a login-screen run proves nothing**.

### 5.3 GL name lifetime — the rule that killed generation 1
Names are viewer-owned and may be **deleted/reallocated** on resize or settings change; guaranteed
alive only for the publishing frame.

> Published textures are **level-0-only MUTABLE** textures. Copying from them is fine.
> **Binding them directly to a ReShade sampler is not** — ReShade GL samplers always mip-filter, an
> incomplete texture samples **black**.

Re-read every frame; compare `{gl_name, width, height, gl_internal_format}`; always copy into
add-on-owned **immutable** resources.

### 5.4 ABI 1.1 tail — proposed, uses `reserved[24]` ⚠️ new
Layout of `SLReShadeFrame` is **unchanged**; the tail is consumed additively under a MINOR bump, so
old add-ons keep working. It carries:
- `semantic_valid` bitmask — per-semantic availability (COLOR_HDR, DEPTH, ALBEDO, ORM, NORMALS,
  EMISSIVE, MOTION, MOTION_META, SURFACE_COVERAGE);
- `reset_flags` — FIRST_VALID_FRAME, FRAME_DISCONTINUITY, CAMERA_CUT, TELEPORT, PROJECTION_CHANGE,
  RESIZE, TARGET_RECREATED, HDR_MODE_CHANGE, SOURCE_LOSS, CONTEXT_CHANGE, SNAPSHOT_TRANSITION,
  PAUSE_RESUME;
- `motion_encoding` — declares the exact convention rather than leaving consumers to infer it;
- `orientation_flags` — source UV origin, required V-flip, motion Y-up, destination Y-down;
- `motion_coverage` — diagnostic bitmask of categories attempted this frame.

Design sketch: `llreshadebridgeabi_v1_1_tail_proposal.h` in the motion-research package.

### 5.5 Conventions ✅
Matrices 16 floats **row-major**, `LLMatrix4`, **row-vector** `v' = v*M`. Depth **standard GL, not
reversed-Z**. Camera axes SL world (X fwd, Y left, Z up). `motion` is published by this tree from
`mVelocityMap` when the velocity buffer is on; the enve tree always publishes 0.

---

## 6. Layer 3 — the add-on

Built against **v6.7.3 headers → API 18**. ⛔ A dev-header build reports API 20 and stable ReShade
rejects it: *"requested API version (20) is not supported (18)"*, error 1114, add-on silently absent.
Build with `/I I:\reshade-SL\include`.

**Per frame:** seqlock-read → per slot create an add-on-**owned immutable** resource matching the
published format → `copy_texture_region` → `update_texture_bindings`. *Copy needs no
mip-completeness — that is what sidesteps §5.3.* GL handle encoding `(0x0DE1 << 40) | name`.

### 6.1 Format map ✅ (`sl_reshade_bridge.cpp:73`)
`0x1908`/`0x8058` → `r8g8b8a8_unorm` · **`0x8C43` → `r8g8b8a8_unorm_srgb`** · `0x805B` →
`r16g16b16a16_unorm` · `0x8059` → `r10g10b10a2_unorm` · `0x881A` → `r16g16b16a16_float` · `0x822F` →
`r16g16_float` · `0x8230` → `r32g32_float` · anything else → `unknown` → **slot silently disables**.

**Not mapped: `GL_RGB16F` (0x881B) and `GL_RGB` (0x1907)** — the emissive formats. Emissive is not
published as a semantic today; if it ever is, both must be added or the slot dies silently.

**The sRGB entry is load-bearing.** Missing until `c3f03ded2fb` (2026-07-15), it disabled albedo
entirely and iMMERSE fell back to Launchpad's *estimated* albedo for whole sessions with only a log
line. It maps to the `_srgb` view so hardware decodes sRGB→linear on sample —
**therefore `SL_ALBEDO_TO_LINEAR` must stay `0`; setting it to `1` decodes twice.**

### 6.2 Cost ⚠️ new
Five copied slots at 4K ≈ **28 B/pixel ≈ 232 MB/frame** of logical payload. Not incidental.
Demand-copy only active semantics, instrument per-slot GPU time and copied bytes, and **fail closed**
on a missing slot rather than quietly retaining it (§4).

### 6.3 Robustness ✅
`on_destroy_effect_runtime` **drops** handles rather than destroying resources (it fires during
`wglDeleteContext` with no current context; a GL delete there faults). SEH guard with stage
breadcrumbs logs `[SLBridge] FATAL: exception 0x… at stage '…'` and self-disables for the session.
Device-keyed so a runtime device change rebuilds slots.

---

## 7. Layer 4 — the FX layer

### 7.1 `SL_Bridge.fxh` — single source of truth
Declares the `SL_*` textures/samplers, `sl_*` uniform annotations, `SL_DecodeNormal` (§2.3) and the
orientation helper. **Every consumer must route through it**; no private decoders. Both current
consumers comply ✅.

Uniforms: `sl_view_matrix`, `sl_proj_matrix`, `sl_camera_pos/at/left/up`, `sl_near_far`, `sl_fov_y`,
`sl_aspect`, `sl_frame_counter`.

**Orientation** ❓: GL is bottom-left origin, ReShade top-left. `SL_INPUT_IS_UPSIDE_DOWN` (default 1)
drives `SL_UV()`. The add-on deliberately does **not** flip — `copy_texture_region` cannot mirror.
Note the live config sets `RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN=0`; **do not force these equal by
comment** — backend copy paths may differ. Prove alignment (§9 O1).

**Sample point, never linear.** Linear filtering blends *encoded* normals across edges — meaningless,
visible as edge shimmer. Fixed once; do not regress.

### 7.2 `SL_GBufferProvider.fx`
| Pass | Destination | Note |
|---|---|---|
| `ProvideNormals` | `Deferred::NormalsTexV3` | `RenderTargetWriteMask = 3` — `.xy` only |
| `ProvideMotion` | `Deferred::MotionVectorsTex` | discards where SL velocity is 0 |
| `ProvideAlbedo` | `Deferred::AlbedoTex` | discards where SL albedo unfed |
| `ShowReceived` | backbuffer | debug, behind `SL_ENABLE_DEBUG` |

`RenderTargetWriteMask = 3` is a **correctness fix, not an optimisation**: Launchpad writes `.xy` =
detailed normal, `.zw` = smooth **geometry** normal; clobbering `.zw` produced sparkle.

Chain: `SL_DecodeNormal` → `sl_gl_to_view` (negate Z) → `Math::octahedral_enc(-n_view)`.
> A direct `.xy` copy is **not** available even though both ends are octahedral: the Z negation folds
> hemispheres, which is exactly the `OctWrap` branch. Decode-transform-encode is the only auditable path.

Switches: `SL_PROVIDE_MOTION` 1 · `SL_PROVIDE_ALBEDO` 1 · **`SL_ALBEDO_TO_LINEAR` 0** · `SL_NORMAL_FLIP_Z` 1 ·
`SL_ENABLE_DEBUG` 1. Live uniforms: `SL_DEBUG_VIEW`, `SL_MOTION_SCALE`, `SL_MOTION_FLIP_X` true, `SL_MOTION_FLIP_Y` false.

### 7.3 COEXIST vs OWNED ⚠️ **new in v2**
These are **different contracts** and must be an explicit mode, not an accident:

```text
COEXIST : Launchpad owns initial shared-resource contents. Provider may discard ONLY where
          explicit validity is false. Yields quality validation, NO Launchpad compute saving.
OWNED   : Provider owns every destination and writes or clears EVERY pixel EVERY frame.
          No discard may rely on Launchpad. NormalsTexV3 .xy AND .zw must both be defined.
```
⛔ **Retirement blocker:** the provider masks to `.xy`, so in cold absence **nothing writes `.zw`** —
the geometry-normal half consumers rely on. Owned mode must generate it.

### 7.4 The `ReShade.fxh` incompatibility
Any `.fx` including Marty's `mmx_*.fxh` must **not** also include `ReShade.fxh`: the latter `#define`s
`BUFFER_PIXEL_SIZE`/`BUFFER_SCREEN_SIZE`/`BUFFER_ASPECT_RATIO` as macros while `mmx_global.fxh`
declares them `static const`, rewriting to `static const float2 float2(...)` → X3000 at
`mmx_global.fxh:47-49`. Provide your own fullscreen VS.

---

## 8. Layer 5 — iMMERSE consumers

**Public documentation establishes:** Launchpad supplies optical flow from consecutive frames,
smoothed normals, and **textured** normals reintroducing high-frequency image detail; top of load order.

**Public documentation does NOT establish:** `NormalsTexV3` exact format/axis packing, internal pass
names, or any albedo de-lighting formula. ❓ **Runtime-discovery questions, not research questions.**

⚠️ **TAAU is not uniform:** public `mmx_deferred.fxh` declares normals at DLSS/TAAU size but **motion
and albedo at full buffer size**. Inspect each destination independently; **never scale a motion
vector's magnitude by the TAAU factor.**

❓ **Reset/reactivity is an external-interface gate.** The public shared header exposes normal, motion
and albedo but **no demonstrated temporal-reset or reactive-mask input**. If no hook exists, the
residual quality boundary must be documented, not assumed away.

### IP boundary — non-negotiable
Include only Marty's **public** headers (`mmx_deferred/math/global.fxh`). **Never** vendor, patch or
edit proprietary source. **Never** invent internal pass names. Retirement disables the **whole
technique** via the public technique-state API against an exact allowlist of observed display names.

---

## 9. Validation gates

No gate passes on "it looks fine". Each names the observation that **fails** it. The motion-research
package ships a 38-test matrix (`RUNTIME_VALIDATION_MATRIX.csv`) covering contract, motion,
projection, lifecycle, sky, transparency, particles, water, normals, albedo, retirement and
performance; the gates below are the spine.

**N1 decode** — `SL_BridgeDebug` DebugMode 0 before/after the octahedral fix. *Fails if* grazing-angle
surfaces look identical to `.pre-octafix.bak` → shader not being picked up, everything downstream void.

**N2 basis** — three matte planes with known world normals + a sphere; freeze; orbit 90°. *Passes if*
normals rotate with the camera (view-space) and planes read as three stable distinct colours.
*Fails on* a systematic axis swap.

**N3 angular error** — heatmap `acos(saturate(dot(n_sl_transformed, n_dest_decoded)))` vs Launchpad
during COEXIST. *Passes if* median error on opaque geometry is a few degrees.

**O1 orientation** — corner marker + depth/normal edge overlay + vertical motion test. *Fails if*
source flip and generic-depth flip disagree in practice, regardless of what the config says.

**M1 motion direction** — `SL_DEBUG_VIEW` Motion mode, pan, vs provider-off. *Fails on* opposite hue
(axis sign) or smear (sign). **Not** a test of the `0.5` factor, which is exact algebra.

**M2 projection history** — animate FOV, change aspect, hard cut. *Fails if* motion is wrong during
the change → confirms the §3.2 single-projection defect.

**M3 sky** — translation-only camera move over sky pixels. *Fails if* sky receives translation
(it should be rotation-only or invalid).

**A1 albedo colour space** — *fails if* enabling `SL_ALBEDO_TO_LINEAR` improves anything, which would
mean the `_srgb` view is not hardware-decoding and §6.1 is wrong.
✅ **RUN 2026-07-25 — PASSED.** Setting it to 1 did **not** improve the image, so the `_srgb` view
*is* hardware-decoding and §6.1 is correct. Keep the define at **0**. Codex's independent source
analysis reached the same conclusion. **This is the first gate in this document actually executed.**

> ### ⚠️ ALBEDO IS FAITHFUL BUT DOWNSTREAM-INCOMPATIBLE — do not "fix" this again
> Symptom (reproduced in-world 2026-07-25, and reported as recurring for weeks): with
> `SL_PROVIDE_ALBEDO 1` the image becomes over-saturated and "burnt", with rainbow/prismatic
> iridescence on fine geometry. Disabling **only** `SL_PROVIDE_ALBEDO` restores a correct image while
> normals continue to be provided.
>
> **It is not a bridge bug.** Evidence: (a) gate A1 passed, eliminating the colour-space hypothesis;
> (b) in the albedo debug view skin reads plausible tan and gold armour reads plausible gold — only
> one *material* is prismatic, whereas a decode/format/swizzle fault would corrupt everything;
> (c) Codex's independent source trace found the chain colorimetrically faithful.
>
> **Cause:** Launchpad fills `Deferred::AlbedoTex` with a **de-lit estimate derived from the lit
> backbuffer**; we write the **raw G-buffer base colour**. Those are different quantities. RTGI
> computes roughly `rtgi * albedo`, and the user's RTGI/RCAO settings were tuned against the
> estimate, so true base colour over-drives the bounce. Highly chromatic base textures (iridescent
> feathers, holographic fabrics — common in SL) are where the mismatch becomes obvious.
>
> **Working configuration: normals ON, albedo OFF, motion OFF.** That is the configuration that
> delivers the bridge's actual value and it should be treated as the default, not a degraded mode.
>
> **Outstanding question (H2 vs H3), not settled and not blocking:** is the wing texture genuinely
> iridescent, or is our albedo merely a different magnitude convention? Decisive test — point the
> albedo debug view at an object with a known flat neutral grey or cream base texture. A faithful
> bridge must show flat neutral albedo *regardless of lighting*. If it does, the rainbow belongs to
> the wing material and H3 is confirmed.
>
> Re-enabling albedo usefully requires either viewer-published `surface_coverage` (§4, currently
> unpublished) or re-tuning RTGI bounce for true base colour. Neither is a code defect.

**A2 albedo coverage** — sky + water + alpha-heavy scene. *Fails on* magenta, ghosted/stale albedo, or
bounce colour on opaque geometry differing from the Launchpad-fed baseline.

**L1 lifecycle** — resize, minimize, toggle a source buffer off. *Fails if* any semantic keeps serving
stale pixels instead of going invalid (§4).

**T1 TAAU** — live baseline is **`.77`**. Test 1.0 / .77 / .66 as **separate controlled runs** against
runtime-enumerated resource dimensions. Watch avatar edges for ghosting; measure frame time.

**R1 cold absence** — load with Launchpad absent. *Fails if* any consumer errors on a missing
declaration, or `NormalsTexV3.zw` is undefined (§7.3).

**P1 cost** — per-slot GPU timers and copied-byte counters (§6.2).

---

## 10. Failure signatures

| Symptom | Cause |
|---|---|
| effect samples pure **black** | bound a viewer texture directly instead of copying (§5.3) |
| slot absent, log says unmapped format | GL enum missing from §6.1 |
| albedo unfed, Launchpad estimate used | sRGB mapping missing, or every pixel discarded |
| add-on absent, **error 1114**, "API (20) not supported (18)" | built against dev headers |
| normals fine head-on, wrong at grazing angles | spheremap/octahedral mismatch (§2.3) |
| sparkle on detailed surfaces | clobbered `.zw`, or linear-sampled encoded normals |
| GI smears on fast pans | motion coverage gap (§3.4) |
| stale image persists after resize/minimize | §4 stale-destination defect |
| ghosting through a hard cut | no temporal reset signalling (§5.4) |
| ReShade overlay dies, no dump, no log trace | **corrupt `ReShade.ini`** — rename, test fresh, re-add functional keys only |

---

## 11. Open unknowns

1. **Moving non-geometry content** (particles, water surface, animated alpha) — §3.4.
2. **Smoothed/textured normals** — image-derived detail native G-buffer normals structurally cannot
   contain. Accept the loss or design an original enhancement.
3. **Albedo for forward/transparent categories** — the high-quality answer is a viewer-side unlit
   albedo prepass; net-new renderer work, unscoped.
4. **`NormalsTexV3` format/axis packing, Launchpad technique names** — runtime discovery (§8).
5. **Reset/reactivity hook existence** — external-interface gate (§8).
6. **10-bit** — historically shelved (our builds soft-crashed, crosire's official binary did not,
   never reproduced from source) ⚠️ **but the live config sets `Force10BitFormat=1`**. Log the actual
   backbuffer mode before any bridge test; never mix it into a bridge experiment.
7. **`gatherFrame()` seam** — the call site and texture-content lifetime need a full-tree trace and a
   capture at `reshade_begin_effects`.
8. **Draw order** — camera-first/geometry-second is *read* from `pipeline.cpp`, not yet *proven* by
   GPU capture.
9. **Provenance** — `90b9c8e216d` is local and unpushed. Do not claim remote reproducibility until it
   is pushed or bundled.

---

## 12. Environment invariants

- **`ReShade.ini` stays `attrib +r`.** Corruption of a *used* ini was the true root cause of the
  soft-crash saga. Golden copy `ReShade.ini.golden`. Unlock → change → re-lock.
- **`RenderGLMultiThreadedTextures = FALSE`.** Every textures-ON test predates the clean locked ini,
  so it is *unverified*, not *disproven* — but do not bundle that experiment with bridge work.
- Add-on loads from `AddonPath`, currently the dev **build output** — a mid-rebuild binary can load
  half-written. Move to a fixed path once stable.
- **One variable per launch**; repeat every run at least twice before believing it.

---

## 13. Change protocol

These must change **together** or the chain silently breaks:

1. `globalF.glsl` encode/decode
2. `llreshadebridgeabi.h` normals note
3. `SL_Bridge.fxh` — **both** `SL_DecodeNormal` **and** the `SLNormalsTex` declaration comment
4. this document
5. the §9 gate that proves it

v1 defined this protocol and then violated it by missing item 3's comment. Item 3 now names both
sites explicitly for that reason.

---

## 14. Implementation route

Phased, from the motion-research checklist. **Do C0 first and alone.**

| Phase | Content | Exit |
|---|---|---|
| **C0** | runtime contract telemetry, baseline freeze, full-tree trace of the camera pass, GPU capture proving draw order, live resource dimensions/formats at **`.77`**, orientation calibration | no code changes; **no TAAU/10-bit/renderer experiments in the same run** |
| **C1** | prove and fix camera/geometry velocity: `last_projection_matrix_unjittered`, projection-change reset, sky policy | M2, M3 |
| **C2** | ABI 1.1 tail — semantic validity, reset flags, motion encoding, orientation; fail-closed lifecycle in the add-on | L1, binary-compat test |
| **C3** | explicit COEXIST and OWNED provider modes; `.zw` ownership in owned mode | R1 |
| **C4** | category coverage — opaque/masked, then transparent/secondary | coverage heatmap |
| **C5** | public consumer reset/reactivity integration, or documented residual boundary | §8 gate |

**Next action: C0 only.** Much of it needs no build — the decode fix is deployed and still unproven
(N1), and the draw-order capture and resource enumeration are pure observation.
