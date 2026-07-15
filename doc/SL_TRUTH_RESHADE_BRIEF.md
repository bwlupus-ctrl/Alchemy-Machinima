# SL-as-Truth for ReShade — design brief

Status: **draft / planning** (2026-07-14). Depth *binding* is in test; everything
below is not yet built.

## 0. Thesis

ReShade was built to post-process games it knows nothing about, so it **infers**
almost everything: which buffer is depth, how to linearise it, what the normals
are, where things moved, what the albedo was, what the HDR looked like before
tonemapping. We own the viewer. **Every inference is a latent bug we can delete
by handing ReShade the ground truth instead.**

Work so far replaced the *buffers* (normals / motion / albedo, and now the depth
binding). This brief covers what's left, in payoff order.

---

## 1. Status quo

| Data | Source today | Truth available? |
|---|---|---|
| Normals | SL G-buffer via bridge ✅ | done |
| Motion | SL velocity buffer via bridge ✅ | done |
| Albedo | SL G-buffer via bridge ✅ | done |
| Depth (*which buffer*) | ❌ **direct bind FAILED — rolled back**, see §2.8 | via §2.4 |
| **Depth (*linearisation*)** | **ReShade guesses — WRONG, see §2** | **P0** |
| Emissive | not bridged (ABI has it) | free win |
| ORM (rough/metal) | bridged, **consumed by nothing** | free-ish |
| HDR colour | iMMERSE *inverse-tonemaps* the SDR backbuffer | high value |
| Sun/moon dir+colour | effects infer from the sky | medium |
| Exposure | effects re-derive luminance | medium |
| Alpha surfaces | **absent from the G-buffer entirely** | structural |
| UI/HUD | post-processed along with the scene | structural |

---

## 2. P0 — Depth linearisation is wrong *right now*

### 2.1 The mismatch

```
ReShade.fxh GetLinearizedDepth():
    const float N = 1.0;                       // near — HARDCODED, not a knob
    depth /= FAR_PLANE - depth * (FAR_PLANE - N);

ReShade.ini:  RESHADE_DEPTH_LINEARIZATION_FAR_PLANE = 1000.0
SL actual:    near = 0.1     (llagentcamera.cpp:235  setNear(0.1f))
              far  = 128     (user RenderFarClip; llviewerdisplay.cpp:229 setFar)
```

Near is off by **10×**, far by **7.8×**.

### 2.2 How bad, concretely

Object at a true **z = 10 m**, SL projection (n=0.1, f=128):

```
d  = f(z−n) / ((f−n)z) = 128(9.9) / (127.9·10)      = 0.990774
L  = d / (F − d(F−1)),  F=1000                       = 0.09697
z_reported = L · FAR_PLANE                           = 96.97 m      ← true is 10 m
```

**~10× error.** Every AO radius, GI ray length, DOF focus distance and fog
falloff is computed against this. It "works" today only because the sliders were
tuned by eye against a distorted curve — the error is *non-linear*, so the
compensation is scene-dependent and breaks whenever the composition changes.

### 2.3 Why config cannot fix it

Setting `FAR_PLANE = 128` (the honest value) still gives:

```
L = 0.990774 / (128 − 0.990774·127) = 0.4562  →  58.4 m      ← true is 10 m
```

Still **5.8× off**, because `N = 1.0` is welded into `ReShade.fxh` and SL's near
is `0.1`. There is *no* preprocessor value that makes this correct. Additionally
SL's far is **dynamic** — halved in Customize-Avatar mode, shrunk under texture
memory pressure, swapped for the probe distance in cube snapshots
(llviewerdisplay.cpp:215-229) — so a static constant could never track it anyway.

### 2.4 The fix: pre-compensated depth

Don't fight the formula — **feed it a depth that makes its fixed math come out
right.** We own the DEPTH binding now, so we choose what `d` is.

Let `F` = the (now permanently fixed) `RESHADE_DEPTH_LINEARIZATION_FAR_PLANE`.
We want shaders to get `L·F = z` (true metres). Solve `L(d') = z/F` for `d'`:

```
        d' / (F − d'(F−1)) = z / F
   =>   d'·F = z(F − d'(F−1))
   =>   d'(F + z(F−1)) = z·F
   =>   d' = z·F / (F + z(F−1))
```

**Check** (F = 1000, z = 10): `d' = 10000/10990 = 0.90992`
→ `L = 0.90992/(1000 − 908.99...) = 0.01` → `×1000 = 10.0 m` ✅ exact.

Precision over SL's range with F = 1000 is healthy (no bunching):

| z (m) | 0.1 | 1 | 10 | 128 | 512 |
|---|---|---|---|---|---|
| d' | 0.0909 | 0.5003 | 0.9099 | 0.9932 | 0.9980 |

### 2.5 Why this is the right shape

- **Fixes every shader at once** — including third-party ones we cannot edit
  (VirtualCinema, GShade, SuperDepth3D…), because they all go through
  `ReShade::GetLinearizedDepth`.
- **Zero config, forever.** `FAR_PLANE = 1000` becomes a fixed constant. Draw
  distance / camera mode / memory pressure change freely; `d'` is recomputed from
  the live `n`/`f` each frame.
- **`depth × FAR_PLANE` becomes true metres**, so effect radii finally mean what
  they say.

### 2.6 Implementation sketch

1. **Client**: fullscreen pass (mirror `velocityCameraF.glsl`, which already does
   the `inv_proj` reconstruction) → `z = −(inv_proj · ndc).z / w` → write
   `d' = z·F/(F + z(F−1))`, `F = 1000.0`, into a dedicated **R32F** target.
   R32F, not R16F — 16F's 10-bit mantissa would band.
2. **ABI**: append a field (e.g. `depth_reshade`) — **append only**, then bump
   `SLRESHADE_ABI_MINOR` (1.0 → 1.1). The addon gates on `MAJOR` only
   (`llreshadebridge.cpp` `SLReShade_GetFrame`), so appending stays
   backward-compatible; `struct_size` covers the rest.
3. **Addon**: bind that texture to `DEPTH` instead of the raw depth (the §1
   binding work is the delivery mechanism; only the *source* changes).
4. **Config**: `FAR_PLANE=1000`, `IS_REVERSED=0`, `IS_LOGARITHMIC=0`, and
   `RESHADE_DEPTH_MULTIPLIER=1`. Never touch again.

### 2.7 Risks / open questions — **both blockers AUDITED CLEAR (2026-07-14)**

- ✅ **`mmx_depth.fxh` uses the IDENTICAL formula.** `Depth::linearize` is
  `x /= RESHADE_DEPTH_LINEARIZATION_FAR_PLANE - x * (FAR_PLANE - 1.0)` — same
  hardcoded `1.0` near, same constant as `ReShade.fxh`. So **one pre-compensation
  corrects iMMERSE and ReShade-native shaders identically**; no per-stack casing.
- ✅ **No semantic raw-depth readers in the active stack.** MXAO / RTGI_SPECULAR /
  RCAO_Blended *do* `tex2DgatherR(DepthInput, …)`, but purely as a 4-texel fetch
  optimisation — the very next line is `Depth::linearize(depth_texels)`
  (`LINEARIZE_OVERLOAD` covers float/2/3/4). Everything funnels through the same
  formula. Third-party genuine raw-readers exist (Bloom, SuperDepth3D,
  DisplayDepth, Temporal_AA, Trails, ThinFilm, RimLight, *_Sharp, pos.fxh) but
  **none are enabled** in `ugh3.ini`. Re-audit if any get turned on.
- ⚠️ **`RESHADE_DEPTH_INPUT_IS_REVERSED` default disagreement (landmine).**
  `ReShade.fxh` defaults it to **0**; `mmx_depth.fxh` defaults it to **1**. Today
  the global `PreprocessorDefinitions=…IS_REVERSED=0` masks this. If that global
  is ever dropped, iMMERSE and non-iMMERSE effects silently invert depth relative
  to each other. Keep it explicitly set.
- Requires the §1 depth binding to be working first (it's the delivery path).
- Validate against a known-distance in-world ruler, not by eye.

### 2.8 Attempt 1 (direct bind) — FAILED, and why it matters

Tried: bind ReShade's `DEPTH` semantic straight at the viewer's depth texture via
`create_resource_view` on the wrapped GL handle. **Rolled back 2026-07-14.**

```
[SLBridge] SL_ORM: bound 4096x2161 fmt 28          <- add-on loaded fine (API-18 pin OK)
[SLBridge] SL_COLOR_HDR: bound 4096x2161 fmt 10
[SLBridge] DEPTH: create_resource_view failed for GL 3044 (fmt 0x81A6)
[SLBridge] 30 sl_* uniform(s) wired
```

Everything else worked; only the view creation failed. Leading hypothesis
(**unconfirmed — verify before acting**): ReShade's GL backend creates views with
**`glTextureView`, which requires IMMUTABLE storage (`glTexStorage*`)**, whereas
`LLRenderTarget` allocates with `glTexImage2D` (mutable). Secondary possibility:
no valid GL view format for `DEPTH_COMPONENT24` → `r24_unorm_x8_uint`.

**This is strong evidence for why the colour slots replicate rather than bind:**
the add-on copies into textures *it* creates (ReShade makes those immutable, hence
viewable). The original author almost certainly hit this same wall.

**Consequence — this inverts the plan's difficulty order.** §2 was filed as the
"harder" follow-up to a working §1. In fact **§2 routes around §1's blocker
entirely**: the client writes pre-compensated depth into an **R32F colour**
texture, which rides the existing, proven copy path — no view of a depth resource
is ever needed. So §2 is not merely the correctness fix, it is the *only* viable
delivery path found so far.

Other routes if §2 stalls:
- Copy depth→depth into an add-on-owned depth resource, then view that (keeps
  format class legal; unknown whether ReShade will make it `shader_resource`).
- Client-side: reallocate the depth target with `glTexStorage2D` (immutable) so
  the source becomes viewable — invasive, touches core `LLRenderTarget`.
- Leave `generic_depth` owning DEPTH and fix only linearisation — impossible, see
  §2.3 (`N` is hardcoded).

---

## 3. Free / cheap wins

### 3.1 Emissive is published but never bridged
`llreshadebridgeabi.h:137` has `f.emissive` (deferredScreen attachment 3); the
addon's slot table has no `SL_EMISSIVE`. Meanwhile `VirtualCinema_EmissiveGlow`
*luminance-thresholds the colour buffer* to guess emissives. Add the slot →
exact emissive masks, and RTGI can treat emissive surfaces as real light sources.
Cost: ~3 lines (`pick_emissive` + slot entry), plumbing already exists.

### 3.2 We pay for ORM + COLOR_HDR and use neither
Both are copied full-res every frame, consumed **only** by `SL_BridgeDebug.fx`.
Either cash them in or drop them:
- **`SL_COLOR_HDR` — highest value.** iMMERSE's `unpack_hdr`/`pack_hdr` is a
  fitted curve *pretending to undo a tonemap it never saw*. We have the real
  pre-tonemap `GL_RGBA16F` scene colour. Feeding it fixes bloom/DOF/GI **energy**.
- **`SL_ORM`** — true roughness/metallic → RTGI_SPECULAR can stop assuming.

---

## 4. Structural (bigger, real ceilings)

- **Alpha surfaces don't exist to ReShade.** SL's deferred G-buffer is
  opaque-only: water, glass and avatar alpha write no depth/normals, so DOF/AO/GI
  treat hair and windows as *background*. Biggest quality ceiling for machinima.
  Needs a client-side alpha depth/normal pre-pass — non-trivial.
- **UI contamination.** ReShade hooks `SwapBuffers`, i.e. *after* UI composite, so
  DOF/grain/CA process chat windows. Fix: client-published UI mask, or invoke
  ReShade before UI compositing. **Verify current severity first** — it may
  already be mitigated.
- **Sun/moon direction + colour** — SL knows exactly; `Godrays_Auto` infers from
  the sky. Publish as uniforms (the `sl_*` uniform channel already exists).
- **Exposure** — SL's auto-exposure value is known; effects re-derive luminance.

---

## 5. Sequencing

1. ⏳ **Validate the depth binding** (in test now) — everything in §2 rides on it.
2. **§2 pre-compensated depth** — active correctness bug; unlocks true metres
   globally. Audit raw-`DepthBuffer` readers + `mmx_depth.fxh` first.
3. **§3.1 emissive** — nearly free, plumbing exists.
4. **§3.2 HDR colour** — biggest *quality* win; needs iMMERSE-side edits, so it
   inherits the re-patch-on-update burden (see `graft_launchpad_v4.py`).
5. **§4** — pick by pain. Alpha depth is the highest ceiling and the most work.

## 6. Ground rules

- **ABI: append-only + bump MINOR.** Never reorder/insert; the addon gates on
  MAJOR and uses `struct_size`.
- **Addon build — three things CMake originally did NOT do** that the old
  hand-rolled `cl.exe` build did. All three are now in `CMakeLists.txt`; all three
  produce silent or fatal failures if lost:
  1. **Static CRT.** `/MD` yields a ~23 KB addon importing MSVCP140/VCRUNTIME140
     that can fail `LoadLibrary` → "the bridge silently does nothing". Correct
     build ≈**146 KB with zero CRT imports** — size is the smoke test.
  2. **Pin `RESHADE_API_VERSION=18.`** `external/reshade` headers describe API
     **20**; the installed ReShade is **18**, and `register_addon()` rejects a
     newer request outright: *"Failed to register add-on, because the requested
     API version (20) is not supported (18)!"* → add-on never loads → the whole
     bridge dies and Generic Depth silently keeps DEPTH. The header's
     `#define RESHADE_API_VERSION 20` was **unconditional**, so `-D` alone could
     not override it; it is now `#ifndef`-guarded (local change, documented in
     the header). This is what the legacy binary name
     `sl_reshade_bridge_api18.dll` was recording. Safe only while we call
     API-18-era methods (ReShade appends vtable entries, so old indices hold).
     **Raise the pin only after updating ReShade itself.**
  3. **Copy the artifact into `reshade-addon/build/`** — that dir is ReShade's
     `AddonPath`, not a CMake build dir (configure into `build-cmake/`).
  Build from **PowerShell** (Git Bash poisons `link.exe`).
  Verify the pin with a compile-time `static_assert(RESHADE_API_VERSION == 18)`
  — and confirm the assert actually *fails* without `-D`, or the test is a
  false positive.
- **Never IPC-gate bridge provision.** The bridge pass is the *sole writer* of its
  buffer, and those buffers are never cleared — so any frame the gate is shut, the
  buffer freezes and temporal effects ghost off stale data (this caused the
  indefinite RTGI ghost). Provision is ~3 cheap fullscreen passes and is always
  needed; gating buys nothing.
  *Correction to an earlier claim: it is NOT true that "nothing requests IPC" —
  the active preset runs stock `MartysMods_RTGI_SPECULAR`, which does
  `IPC_REQUEST_FEATURE(NORMALS | OPTICALFLOW)`. Only the custom
  `iMMERSE_RTGI_RCAO_Blended` (the main diffuse GI) has no request at all. The
  rule stands on the sole-writer argument, not on the request census.*
- **Proprietary iMMERSE edits are scripted, not hand-made** — see
  `reshade-addon/tools/graft_launchpad_v4.py` / `patch_dof_v4.py`; they must be
  re-runnable after every iMMERSE drop.
