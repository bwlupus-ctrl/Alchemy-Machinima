# ReShade ↔ Core Deferred Renderer Integration — Feasibility & Architecture

**Status:** Research only. No code changes. Author: rendering-integration research pass, 2026-07-12.
**Goal:** Let ReShade ray-traced GI/AO/specular (iMMERSE / MartysMods RTGI) consume the viewer's **real** engine buffers (G-buffer normals, ORM, depth, motion vectors, light data) instead of screen-space **reconstruction/guessing**, to kill the depth artifacts, light leaking, and disocclusion/ghosting that ruin machinima shots.

> **IP / licensing boundary (read first).** MartysMods / iMMERSE RTGI shaders are proprietary (Pascal Gilcher; *"Unauthorized copying of this file, via any medium is strictly prohibited"* — verbatim header in every `MartysMods\*.fxh`). This report proposes **only** feeding better DATA through supported interfaces: ReShade's addon texture-binding API (`update_texture_bindings` + FX `SEMANTIC`) and ReShade's documented same-named-texture sharing. **No path here copies, modifies, reverse-engineers, or redistributes his shader source.** Every place a tempting shortcut would cross that line is flagged with a compliant alternative. `llreshadebridge.*` and the `reshade-SL` fork are the user's own code and fair game.

---

## 1. Current state — what exists vs. what's missing

### 1.1 Viewer side — `LLReShadeBridge`

`indra/newview/llreshadebridge.h` / `.cpp` is a **data-gathering scaffold, wired to nothing.**

**Present and working (compiles today, `LL_RESHADE_ADDON = 0`):**
- `struct LLReShadeFrameData` (`llreshadebridge.h:27-58`) — a per-frame snapshot: camera scalars (near/far/fovY/aspect), world-space camera frame (origin + at/left/up axes), row-major `mView`/`mProj` (16 floats each, copied straight from `LLMatrix4::mMatrix`), RT dimensions, and **raw GL texture names** for HDR color, depth, albedo, ORM, encoded normals, emissive.
- `gatherFrame()` (`llreshadebridge.cpp:36-106`) — pure CPU read, no GPU work. Pulls camera state from `LLViewerCamera`, then reads GL handles off `gPipeline.mRT->deferredScreen` (attachments 0=albedo, 1=ORM, 2=normals, 3=emissive; `getDepth()` for depth) and `gPipeline.mRT->screen` (attachment 0 = HDR color). Sets `mValid` only if normals+depth handles are non-zero.
- It is **called** once per frame: `llviewerdisplay.cpp:1529`, right after `gPipeline.renderFinalize()` (`:1525`) and **before** HUD/UI compositing (`:1536`). Comment claims the G-buffer is "still valid" at that point — see Risk §5.

**Missing / stubbed (gated behind `LL_RESHADE_ADDON`, currently `0` = no-op sink):**
- **No ReShade SDK is vendored.** `#include "reshade.hpp"` (`.cpp:22`) is inside the dead `#if`. The `reshade-SL` headers exist at `I:\reshade-SL\include\` but are not on the viewer's include path and no CMake wires them in.
- **No addon registration.** `init()` (`.cpp:111-138`) is a comment block describing the intended contract; `mEnabled` is hard-set `false` (`.cpp:137`). `shutdown()` is empty.
- **No push to ReShade.** `gatherFrame()`'s `#if LL_RESHADE_ADDON` block (`.cpp:100-105`) contains only a commented `// pushToReShade(f)`. Nothing ever calls `register_addon`, subscribes to events, or calls `update_texture_bindings`.
- **The GL-name → ReShade-handle encoding is explicitly left "to be confirmed, do not guess"** (`.cpp:130-133`). **This report resolves it — see §1.3.**
- **Motion vectors are NOT captured.** `LLReShadeFrameData` has no velocity field and `gatherFrame()` never touches `gPipeline.mVelocityMap`. The A5.4 velocity buffer exists in the pipeline but is invisible to the bridge. This is a real gap Tier 1 must close.

**Exact gap to a working addon:** vendor `reshade.hpp` + set `LL_RESHADE_ADDON=1` in CMake; in `init()` call `reshade::register_addon(hSelfModule)` and subscribe to `init_effect_runtime` (cache the `effect_runtime*`) and `reshade_begin_effects`; add a velocity handle to the struct and capture it; and implement the handle-wrap + `update_texture_bindings` calls. None of the hard design questions were answered in the scaffold — they are answered below.

### 1.2 ReShade addon API in `reshade-SL` (the fork that is actually injected)

Confirmed against the fork source (`RESHADE_API_VERSION 20`, `reshade.hpp:13` — i.e. ReShade 6.x):

- **Registration:** `reshade::register_addon(void* addon_module)` (`reshade.hpp:250`). Because ReShade is injected as `opengl32.dll` **into the viewer process**, the "addon module" is the viewer's own module handle — the addon is *in-process*, same GL context. No separate `.addon` DLL is required.
- **Events** (`reshade_events.hpp`): `addon_event::init_effect_runtime` → `void(effect_runtime*)` (`:1846`) — cache the runtime pointer here. `addon_event::reshade_begin_effects` → `void(effect_runtime*, command_list*, resource_view rtv, resource_view rtv_srgb)` (`:1946`) — fires each frame just before effects run; safe point to (re)assert bindings/uniforms.
- **Texture injection:** `effect_runtime::update_texture_bindings(const char* semantic, resource_view srv, resource_view srv_srgb)` (`reshade_api.hpp:514-523`). Binds an SRV to **every** FX texture declared `texture Name : SEMANTIC;`. Confirmed by upstream ReShade docs: calling `update_texture_bindings("BLUB", …)` lets any effect reference it via `texture Whatever : BLUB;`. **It is expensive (CPU/GPU sync)** and must NOT be called every frame — call it once on init/resize. Because a GL texture *name* is stable across frames, binding once is enough: the SRV keeps pointing at the same GL object and its **contents flow every frame automatically**.
- **Uniforms:** `enumerate_uniform_variables` + `set_uniform_value_float` to push matrices / near-far / fov by matching a custom `source` annotation (as the scaffold notes). Lower priority than textures.
- **Semantic mechanics confirmed in `runtime.cpp`:** the `_texture_semantic_bindings` map is consulted when an effect texture has a non-empty semantic (`runtime.cpp:2760-2771`); `update_texture_bindings` writes that map and is re-applied on reload (`:971-978`). Constraint: **a texture that has a semantic cannot be a render target** (`runtime.cpp:2028-2031`) — so semantic-bound inputs are read-only; anything we need to *write* must be a semantic-less texture.
- **Cross-effect texture sharing by name:** two effects that declare a texture with the same `unique_name` share the **same underlying resource**, provided the descriptions match (`runtime.cpp:2045-2060`). This is the mechanism that lets an addon-side provider effect write into a texture another effect samples (see §4).

**Upstream (crosire/reshade) cross-reference & divergence.** The addon SDK is source-compatible between upstream 6.x and `reshade-SL`: `register_addon`, the event enums, and `update_texture_bindings(semantic, srv, srv_srgb)` are identical in shape, and the DEPTH-semantic pattern below is upstream-standard. **The integration must be compiled against whichever `reshade.hpp` matches the injected runtime.** The user's injected runtime IS `reshade-SL` (their 10-bit fork), so vendor `I:\reshade-SL\include\` — its `RESHADE_API_VERSION 20` is the contract. Only divergence that matters here: `reshade-SL` carries the 10-bit `R10G10B10A2` back-buffer work (`source/opengl/opengl_impl_type_convert.cpp`), which does not change the addon API surface. If the user ever swaps to a stock ReShade build, re-vendor that build's header and rebuild the addon; do not mix headers across API versions.

### 1.3 The crux — wrapping a raw GL texture name as a ReShade resource

**Resolved.** `reshade-SL/source/opengl/opengl_impl_type_convert.hpp:100-108`:

```cpp
constexpr auto make_resource_handle(GLenum target, GLuint object) -> api::resource
    { return { (uint64_t(target) << 40) | object }; }
constexpr auto make_resource_view_handle(GLenum target, GLuint object, bool standalone = false) -> api::resource_view
    { return { (uint64_t(target) << 40) | (uint64_t(standalone ? 1 : 0) << 32) | object }; }
```

A ReShade OpenGL `resource_view` is literally the GL target packed into bits 40+ OR'd with the GL object name. So the viewer's `mTexNormals`, `mTexDepth`, etc. become bindable via `make_resource_view_handle(GL_TEXTURE_2D, glName)`.

**Caveat / flag:** this helper lives in an **internal** backend header (`source/opengl/`), *not* in the public addon SDK (`include/`). An in-process addon can (a) replicate the one-line bit-packing, or (b) `#include` the fork's internal header since we build against the fork anyway. Option (b) is cleaner but couples the addon to an unstable internal encoding — pin it with a static-assert/round-trip test and re-verify on any ReShade upgrade. Either way, **the "do not guess" note in the scaffold is now answered:** the encoding is `(target<<40)|object`, target `GL_TEXTURE_2D`, `standalone=false` for a texture ReShade did not itself create. (For sRGB samplers, pass a second SRV of the sRGB format variant; for linear buffers like normals/depth/velocity, pass the same handle as both args or leave `srv_srgb` zero.)

---

## 2. Data inventory — real buffers vs. what RTGI guesses

### 2.1 What the viewer actually has

| Buffer | Source (file:line) | GL format | Space / encoding | Valid when | Directly injectable? |
|---|---|---|---|---|---|
| **Normals** | `deferredScreen` attach 2; `addDeferredAttachments` `pipeline.cpp:406,421`; captured `llreshadebridge.cpp:88` | `GL_RGBA16` (HDR) / `GL_RGB10_A2` (non-HDR) | **VIEW/EYE space** octahedral in `.xy`; `.z`=env-intensity, `.w`=gbuffer flag. Encode/decode `globalF.glsl:55-75` (Cigolle/Stubbe oct + `OctWrap`). Written eye-space: `pbropaqueV.glsl:94 vec3 n = normal_matrix * normal` → `pbropaqueF.glsl:117 encodeNormal(tnorm,…)`. | after opaque deferred fill; persists through `renderFinalize` | Yes (GL name). Needs **transcode** to Marty's oct, not a bit-copy (§4). |
| **Depth** | `deferredScreen.getDepth()`; captured `llreshadebridge.cpp:81` | GL depth texture | Standard GL hardware depth `[0,1]`, **non-reversed**. Linearize: `deferredUtil.glsl:206 linearDepth(d,zn,zf)= zn*2*zf/(zf+zn-(2d-1)(zf-zn))` | shared w/ deferred + velocity | **Yes — via the standard `DEPTH` semantic. Zero new FX.** |
| **ORM** | `deferredScreen` attach 1; `llreshadebridge.cpp:87` | `GL_RGBA` | occlusion / roughness / metallic (+spec for legacy) | after deferred fill | Yes (GL name); no standard consumer semantic — needs a provider texture. |
| **Albedo** | `deferredScreen` attach 0; `.cpp:86` | `GL_SRGB8_ALPHA8` | sRGB diffuse/base color | after deferred fill | Yes; feeds Marty `Deferred::AlbedoTex` (he currently *de-lights the backbuffer to guess this*). |
| **Emissive** | `deferredScreen` attach 3 (optional, `RenderEnableEmissiveBuffer`); `.cpp:89` | `GL_RGB16F`/`GL_RGB` | linear emissive | after deferred fill; 0 if disabled | Yes, when enabled. |
| **HDR color** | `mRT->screen` attach 0; `.cpp:93` | scene HDR (pre-tonemap) | linear HDR | after `renderFinalize` scene composite | Yes; not usually needed (ReShade already owns the back buffer as `COLOR`). |
| **Motion vectors** | `mVelocityMap` `GL_RG16F`, `pipeline.cpp:1115`; written `renderGeomVelocity` `pipeline.cpp:4600-4638` | `GL_RG16F` | **screen-space velocity = current NDC − previous NDC**, un-jittered. Rigid + camera in Phase 1a; **skinned avatars emit zero (Phase 1b seam)**. Gated `BDMergeVelocityBuffer`. | after opaque velocity pass, before UI | Yes (GL name) — **but not yet captured by the bridge (gap).** Needs NDC-delta → UV-delta convert + Y-flip for Marty (§4). |
| **Per-light data** | `LLPipeline` light lists / `mHWLightColors`; projector/shadow slots (`BDMergeMaxSpotShadows`) | CPU/UBO | world-space light list already feeding deferred + volumetrics | during deferred lighting | **Not** injectable as a texture; relevant only to Tier 2 (native pass), not to RTGI. |

### 2.2 What RTGI currently guesses (and where)

RTGI reads **all** of its geometry data through three accessors in the proprietary `Deferred` namespace — `mmx_deferred.fxh:65-98`:
- `Deferred::get_normals(uv)` ← samples `Deferred::NormalsTexV3` (`RGBA16`, oct `.xy`=surface, `.zw`=geometry; `-Math::octahedral_dec`, **view space, negated-Z** convention).
- `Deferred::get_motion(uv)` ← samples `Deferred::MotionVectorsTex` (`RG16F`, `.xy` = **delta UV**).
- `Deferred::get_albedo(uv)` ← samples `Deferred::AlbedoTex` (`RGBA16F`).

Those three textures are **filled by the Launchpad prepass**, not by the engine: `MartysMods_LAUNCHPAD.fx` writes `Deferred::MotionVectorsTex` (`:1546`), `Deferred::AlbedoTex` (`:1589`), `Deferred::NormalsTexV3` (`:1592,:1596`). The Launchpad is the *guessing engine*:
- **Normals** are **reconstructed from depth** (Launchpad `NormalsPS`) → the depth-derived normals that produce your light-leaking / faceted-GI artifacts.
- **Motion** is **estimated by iterative optical flow** ("gradient descent, similar to AI training", `LAUNCHPAD.fx:62`; multi-scale pyramid `MotionTexNewA/B/Upscale*` `:196-202`) → the source of temporal ghosting and disocclusion smearing on fast camera moves.
- **Albedo** is de-lit from the back buffer → wrong under strong lighting.
- **Depth** is taken from ReShade's `DEPTH` semantic (`ReShade.fxh:75 texture DepthBufferTex : DEPTH`), then linearized by Marty against a *user-configured* far plane (`mmx_depth.fxh:106-116`). With an OpenGL-injected runtime, generic_depth frequently mis-detects or grabs a recycled depth buffer — another major artifact source.

RTGI (`MartysMods_RTGI_SPECULAR.fx`, `iMMERSE_RTGI_RCAO_Blended.fx`) only ever calls those `Deferred::get_*` accessors (e.g. SPECULAR `:821,:1043,:1046,:1174`; it never reads the engine directly). **So the entire integration reduces to: put real data into what those accessors read, before RTGI runs.**

---

## 3. Design principle — a general-purpose G-buffer provider, RTGI is just the first consumer

Every buffer we expose benefits **any** ReShade effect that today guesses from depth — MXAO/qUINT SSAO, ReShade/CinematicDOF depth-of-field, motion blur, TAA/upscalers, fog. Design the bridge as a reusable **"SL real G-buffer + motion provider,"** not an RTGI hack, and publish the buffers under the names/semantics the ecosystem already looks for so shaders opt in with little or no change:

- **Depth → the standard `DEPTH` semantic.** Instantly upgrades *every* depth-consuming effect (MXAO, DOF, RTGI's `mmx_depth`) and eliminates ReShade's depth-buffer-detection guesswork. Highest leverage per line of code. **No provider FX at all** — just the addon.
- **Motion vectors → the community convention.** Two consumers differ: iMMERSE reads the semantic-less `Deferred::MotionVectorsTex`; the broader ecosystem (qUINT/vort motion consumers, motion-blur, TAA) reads a shared texture literally **named `MotionVectors`** (RG16F, delta-UV). A general provider should publish **both** — write `Deferred::MotionVectorsTex` *and* expose/`update_texture_bindings` a `MotionVectors`-named/semantic texture.
- **Normals → iMMERSE's `Deferred::NormalsTexV3` (view-space oct).** No single ecosystem-wide normal semantic exists; most SSAO variants reconstruct internally. So normals primarily benefit iMMERSE-family shaders (RTGI, MXAO, PTVL) via the `Deferred` texture. Offer **view-space** (Marty's convention) as primary; world-space is derivable but no consumer needs it — don't ship it until asked.
- **Space/format contract to honor:** Marty wants **view-space** normals (matches SL's eye-space G-buffer — see §5), **negated** with his octahedral encoding; motion as **delta-UV** with ReShade's Y-down convention. Provide those; do not assume SL's oct == Marty's oct.

Because a semantic-bound texture cannot be a render target (`runtime.cpp:2028`), feeding the semantic-less `Deferred::*` textures requires the provider pattern in §4 (bind SL buffers to *our* semantics; a *ours* FX transcodes into the shared `Deferred::*` names). Depth and a `MotionVectors`-semantic texture are the exceptions that need no provider FX.

---

## 4. Tiered options

### Tier 1 (recommended, 80/20) — wire `LL_RESHADE_ADDON`, inject real normals + linear depth + A5.4 motion vectors

**What it is.** Turn the scaffold on and hand RTGI real data through supported interfaces only.

**Integration points.**
1. **CMake / vendoring:** add `I:\reshade-SL\include` to the newview include dirs; define `LL_RESHADE_ADDON=1`.
2. **`llreshadebridge` (viewer):**
   - `init()`: `reshade::register_addon(hSelf)`; subscribe `init_effect_runtime` (cache `effect_runtime*`), `reshade_begin_effects` (re-assert bindings after a reload). `shutdown()`: `unregister_addon`.
   - **Add a velocity handle** to `LLReShadeFrameData` and capture `gPipeline.mVelocityMap.getTexture(0)` in `gatherFrame()` (currently absent — the one required struct change).
   - On init/resize (**not per frame**): wrap each GL name via `make_resource_view_handle(GL_TEXTURE_2D, name)` and call:
     - `update_texture_bindings("DEPTH", depthSrv, 0)` — standard, no FX.
     - `update_texture_bindings("SL_NORMALS", normalSrv, 0)`, `("SL_MOTION", velSrv, 0)`, optionally `("SL_ORM",…)`, `("SL_ALBEDO", albedoSrv, albedoSrvSrgb)`.
   - Push camera uniforms (view/proj/near/far) via `set_uniform_value_float` if a provider FX declares them.
3. **Compliant provider effect (OUR code — `SL_GBufferProvider.fx`, authored from scratch):**
   - Declares read-only inputs: `texture SLNormals : SL_NORMALS; texture SLMotion : SL_MOTION; …`.
   - Re-declares the shared `namespace Deferred { texture NormalsTexV3 {…matching desc…}; texture MotionVectorsTex {…}; texture AlbedoTex {…}; }` — semantic-less, render-target-able, and **shared by name** with RTGI/Launchpad (`runtime.cpp:2045-2060`; descriptions must match exactly).
   - Passes: sample `SLNormals` → decode SL oct (`globalF.glsl` convention) → view-space normal → **re-encode into Marty's oct** (`Math::octahedral_enc`, negated) → write `Deferred::NormalsTexV3`. Sample `SLMotion` → convert **NDC-delta → UV-delta** (×0.5, Y-flip) → write `Deferred::MotionVectorsTex`. Optionally write `Deferred::AlbedoTex` from real albedo.
   - Technique ordered **after** Launchpad and **before** RTGI (or simply **replace** Launchpad: disable its technique and let our provider fill all three `Deferred::*` textures — the textures still exist because RTGI's own include declares them, and this avoids Launchpad's wasted optical-flow/normal work and ordering fragility).

**IP compliance.** We only *write named textures via the documented sharing/semantic interface* and author *our own* FX. We never edit, copy, or reverse-engineer Marty's `.fx`/`.fxh`. Matching a texture's `Format/Width/Height` to satisfy the sharing contract is interface conformance, not copying shader logic. **A non-compliant shortcut to avoid:** do NOT paste Marty's `NormalsPS`/optical-flow code into our provider or edit `mmx_deferred.fxh`/`LAUNCHPAD.fx` — that crosses the line. The compliant equivalent is the transcode-into-shared-texture pass above.

**Effort:** moderate. ~1 struct field + ~150 LOC of addon glue in `llreshadebridge.cpp`; one ~200-line provider `.fx`; CMake vendoring. No pipeline surgery.
**Risk:** medium — coordinate/encoding correctness (§5), the internal handle encoding, texture-description matching, and buffer-content validity at the capture point. All are verifiable incrementally.
**Expected impact:** **large.** Real depth removes depth-detection failures and depth artifacts for *all* effects. Real view-space normals remove the depth-reconstructed faceting and light-leaking in GI/AO. Real A5.4 motion vectors remove temporal ghosting/disocclusion smear on camera moves — the machinima-critical win — for rigid + camera motion (skinned avatars still zero until A5.4 Phase 1b lands). This is the recommended path.

### Tier 2 — native GLSL GI/AO pass inside the deferred pipeline (no ReShade)

**What it is.** A fullscreen GI/AO pass in `pipeline.cpp` that reads the full G-buffer **and the real light list** and composites additively into the HDR scene buffer before color-correct — exactly like the fork's existing `renderProjectorVolumetric` (`pipeline.cpp:9579`, runs in `renderFinalize`, additive-in-place) and the volumetric/godray passes (`:9533`). Slot it in `renderFinalize` **before** `colorCorrect` (`pipeline.cpp:8311`) and before bloom sampling, matching the established pattern.

**Why it's attractive:** no IP entanglement (all our code), no reconstruction whatsoever (native access to eye-space normals, linear depth, ORM, *and* per-light data RTGI can never see), no addon boundary, deterministic for frame-accurate machinima. **Cost:** we implement the GI/AO algorithm ourselves (SSGI/GTAO-class, or a horizon/visibility-bitmask AO). It won't match RTGI's ray-marched quality out of the gate, but it's fully controllable and integrates with the fork's existing temporal/velocity machinery (`mVelocityMap`) for stable denoising. Best long-term home for AO; a serious GI here is a larger project.
**Effort:** high (new shader + pipeline plumbing). **Risk:** medium (self-contained, follows precedent). **Impact:** high-quality AO achievable; GI depends on effort invested.

### Tier 3 — true hardware ray tracing (scene BVH via GL↔D3D12/Vulkan interop)

**What it is.** Build a BVH of scene geometry and trace real rays via a D3D12/Vulkan RT backend, sharing results back to GL through external-memory interop.

**Reality check:** the only interop in-tree is **CEF/dullahan accelerated-paint** (`llcefaccelinterop.h`, `WGL_NV_DX_interop` in `llglheaders.h`) — a browser-texture path, **not** a general compute/RT interop. There is no BVH, no acceleration-structure management, no GL↔RT-queue synchronization, no geometry-stream extraction from the draw pools. All of that is greenfield in a GL-first viewer whose geometry lives in GL buffers. **Honest assessment: out of scope near-term.** It's a multi-month subsystem (BVH build/refit over dynamic SL scenes, descriptor/interop plumbing, denoiser) with high risk. Revisit only after Tier 1 ships and if Tier 2's native GI proves insufficient. The interop *precedent* exists, but nothing reusable for RT does.

---

## 5. Recommended first milestone — smallest change that feeds ONE real buffer and shows a visible win

Do it in two small steps; each is independently verifiable.

**Step A — real DEPTH (plumbing proof, zero Marty-side, zero transcoding).**
- **Files/functions:** `llreshadebridge.cpp init()` — vendor `reshade.hpp`, `register_addon`, subscribe `init_effect_runtime`; on runtime-ready, `update_texture_bindings("DEPTH", make_resource_view_handle(GL_TEXTURE_2D, mFrame.mTexDepth), 0)`. CMake: `LL_RESHADE_ADDON=1` + include dir. `reshade-SL`: no change (its `make_resource_view_handle` already exists). Ship **without** the generic_depth addon so nothing fights our binding.
- **Verify in-world:** ReShade → depth-preview shader shows a clean, correctly-oriented depth that tracks the camera with no detection dropouts; MXAO/DOF immediately stop swimming. If inverted/black, flip `RESHADE_DEPTH_INPUT_IS_REVERSED` / set `RESHADE_DEPTH_LINEARIZATION_FAR_PLANE = RenderFarClip`. Proves registration + handle-wrap + binding end-to-end with no FX authoring.

**Step B — real motion vectors (the machinima win).**
- **Files/functions:** viewer — add `mTexVelocity` to `LLReShadeFrameData`, capture `mVelocityMap.getTexture(0)` in `gatherFrame()`, ensure `BDMergeVelocityBuffer` is on; `update_texture_bindings("SL_MOTION", velSrv, 0)`. Author `SL_GBufferProvider.fx` (ours) with one pass: sample `SLMotion`, convert NDC-delta→UV-delta (×0.5, Y-flip), write `Deferred::MotionVectorsTex`; order it after Launchpad / disable Launchpad's MV pass reliance. `reshade-SL`: none.
- **Verify in-world:** enable RTGI + Launchpad debug MV view (`LAUNCHPAD_DEBUG_OUTPUT`) — our injected field should be crisp and match true screen motion (vs. the noisy optical-flow guess); then in RTGI, orbit the camera past a foreground object and confirm the trailing ghost/disocclusion smear collapses. Expect a clear reduction in temporal ghosting on rigid/camera motion (skinned avatars unchanged until A5.4 1b).

Motion vectors are chosen as the first *quality* win because the hard part — generating a real, un-jittered velocity buffer — is **already done** (A5.4), and ghosting is what most visibly ruins shots. Normals is the natural next increment on identical plumbing.

---

## 6. Open questions / risks

- **Normal coordinate space (highest priority to verify).** SL writes **eye/view-space** normals (`pbropaqueV.glsl:94 normal_matrix * normal`), which *matches* Marty's view-space expectation — but the bridge header comment "encoded world normals" (`llreshadebridge.h:54`) is **inaccurate/misleading** and must not be trusted. Confirm empirically (inject, view Marty's normal debug). If a residual rotation shows, apply the view-basis fix in the provider (camera axes are already in `LLReShadeFrameData`).
- **Octahedral encoding mismatch.** SL uses Cigolle/Stubbe oct with `OctWrap` (`globalF.glsl:50-75`); Marty uses `Math::octahedral_enc/dec` and **negates** (`mmx_deferred.fxh:68 -octahedral_dec`). These are *not* bit-compatible — the provider must **decode-then-re-encode**, never copy raw `.xy`.
- **Motion-vector encoding.** SL stores **NDC delta** (`current NDC − previous NDC`, `pipeline.cpp:4590`); Marty wants **UV delta** (`mmx_deferred.fxh:77`). Convert ×0.5 and **flip Y** (GL bottom-up vs. ReShade top-down). Sign/scale must be validated against the debug view or GI will smear the wrong direction.
- **Matrix marshaling.** `mView/mProj` are copied row-major from `LLMatrix4` (`llreshadebridge.cpp:70-71`); ReShade/HLSL FX is column-major — transpose when pushing as uniforms.
- **GL vs. ReShade Y-flip / half-res.** ReShade's GL backend flips Y; injected normals/motion may need a `.y` flip. RTGI/Launchpad size textures to `BUFFER_*_DLSS` (`LAUNCHPAD.fx:196-202,812`) — if any DLSS/upscale res-scale is active, the viewer buffers (full-res) won't line up 1:1; keep ReShade at native for machinima or handle the scale.
- **Reversed-Z / near-far.** SL depth is standard non-reversed (`deferredUtil.glsl:206-210`); set ReShade's depth config to match (`RESHADE_DEPTH_INPUT_IS_REVERSED=0`, far = `RenderFarClip`). Re-check if the fork ever enables reversed-Z.
- **Buffer-content validity at the capture point.** `gatherFrame()` runs *after* `renderFinalize()` (`llviewerdisplay.cpp:1525-1529`); ReShade's effects run later still (at the `opengl32` SwapBuffers hook, after UI). GL texture *names* are stable, but confirm `deferredScreen` attachment **contents** aren't recycled/overwritten by post-processing before ReShade samples them. `mVelocityMap` is a separate RT and is safe. Verify per buffer; if a G-buffer attachment is clobbered, snapshot it (blit to a dedicated RT) at capture time.
- **GL handle lifetime across resize.** RT reallocation (`pipeline.cpp:995,1115`) changes GL names; re-issue `update_texture_bindings` on resize (hook the same path that reallocs). Never cache stale handles.
- **`update_texture_bindings` cost.** Expensive (CPU/GPU sync) — bind on init/resize only, never per frame; use `reshade_begin_effects` only to *re-assert* after an effect reload.
- **Addon-vs-addon contention.** If generic_depth (or another MV addon) is also loaded it will also call `update_texture_bindings("DEPTH"/…)`; ship without them or guarantee ours runs last.
- **Internal handle encoding coupling.** `make_resource_view_handle` is an internal `reshade-SL` header (`opengl_impl_type_convert.hpp`); pin with a round-trip assert and re-verify on every ReShade upgrade.
- **IP boundary (standing).** Everything above feeds data through named-texture sharing + semantics and our own provider FX. The moment a change would edit, copy, or re-implement Marty's shader logic, stop — the compliant equivalent is always "write the real data into the texture his shader already samples."
