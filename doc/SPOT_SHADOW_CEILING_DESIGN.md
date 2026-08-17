# Spot (Projector) Shadow Ceiling: 6 -> 8 Design / Scoping

Read-only design pass. **No source was modified.** Every claim cites `file:line` and is
labelled **PROVES** (the cited text directly establishes it), **IMPLIES** (follows with one
short inference from cited text), or **INFERS** (reasoned conclusion needing test/judgement).

Goal:
1. Raise the compile-time ceiling on simultaneous projector shadows from **6 -> 8**, and
   keep the ceiling a *single* easy-to-change constant.
2. Make the Cinematic Light Rig automatically obtain enough active shadow slots so that
   "more than 2 projectors cast shadows at once" happens without the user hand-editing a
   setting.

---

## 0. Executive summary / verdicts

- **Edit count:** 6 files carry *load-bearing* edits (`pipeline.h`, `llviewercamera.h`,
  `shadowUtil.glsl`, `llshadermgr.cpp`, `llshadermgr.h`, `settings.xml`) plus 2 UI/rig files
  (`floater_lightbox_settings.xml`, `alpanelcinelightrig.cpp`). All 21 `MAX_SPOT_SHADOWS`
  sites and all 10 `bdmergeMaxSpotShadows()` sites in `pipeline.cpp` **auto-adjust** — none
  hard-codes `6`. The only *hard-coded* things that must be hand-extended are the
  **camera-id enum** (`llviewercamera.h`), the **GLSL sampler declarations + index dispatch**
  (`shadowUtil.glsl`), and the **LLShaderMgr uniform-name table + enum** (`llshadermgr.cpp/.h`).
- **Texture-unit feasibility:** **8 fits** on any desktop GL that reports
  `GL_MAX_TEXTURE_IMAGE_UNITS == 32` (all current NVIDIA/AMD/Intel). The worst-case deferred
  spot-light fragment program already binds ~25 texture channels at 6 spots — *already above
  the GL-guaranteed minimum of 16* — so the real floor this fork depends on is 32, not 16.
  Going to 8 adds exactly 2 samplers (~27 channels), leaving ~4-5 headroom. **Safe hard max
  before crossing 32 is ~10-12 spot shadows.** A hardware gate is recommended (details §4).
- **Rig auto-active-count:** recommend **option (a) scoped + opt-in** — an
  auto-raise of `BDMergeMaxSpotShadows` to `max(current, min(sum_of_requested_slots, 8))`,
  driven from the rig, gated behind a new `CineLightRigAutoShadowSlots` bool (default on),
  reusing the already-present `computeRequestedShadowSlots()` logic. It writes a persisted
  global on the user's behalf, so it must be surfaced (§6).
- **Version bump:** **No scene/asset version bump needed.** `BDMergeMaxSpotShadows` keeps its
  default of `2`; a persisted user value of 2..6 stays valid. Only the *comment* and the UI
  slider `max` change. No serialized rig-blob schema changes.

---

## 1. Constants — the ceiling

### 1.1 The single knob

`indra/newview/pipeline.h:1030`
```cpp
static constexpr U32    MAX_SPOT_SHADOWS = 6;                 // -> 8
static constexpr U32    MAX_SHADOW_MATS = 4 + MAX_SPOT_SHADOWS; // auto -> 12
```
**PROVES** the ceiling is one constant and `MAX_SHADOW_MATS` derives from it
(`pipeline.h:1030-1031`). Changing `6 -> 8` makes `MAX_SHADOW_MATS` become `12` with no other
C++ edit in this header.

### 1.2 Arrays that resize automatically (no edit)

All sized by `MAX_SPOT_SHADOWS` or `MAX_SHADOW_MATS`, so they grow with the constant
(**PROVES**, `pipeline.h`):
- `mSpotShadow[MAX_SPOT_SHADOWS]` :1032
- `mPrismSpotShadow[MAX_SPOT_SHADOWS]` :1041
- `mSunShadowMatrix / mShadowModelview / mShadowProjection [MAX_SHADOW_MATS]` :1196-1198
- `mShadowSpotLight / mSpotLightFade / mTargetShadowSpotLight [MAX_SPOT_SHADOWS]` :1201-1203
- `mPrismSavedShadowSpotLight / mPrismSavedTargetShadowSpotLight / mPrismSavedSpotLightFade
  [MAX_SPOT_SHADOWS]` :1294-1296
- `mPrismSavedSunShadowMatrix / mPrismSavedShadowModelview / mPrismSavedShadowProjection
  [MAX_SPOT_SHADOWS]` :1301,1305,1306 (these hold **spot** matrices only, hence
  `MAX_SPOT_SHADOWS` not `MAX_SHADOW_MATS`).

### 1.3 The `pipeline.cpp` usages (the "27" audit)

`grep MAX_SPOT_SHADOWS pipeline.cpp` -> **21 hits**; `grep bdmergeMaxSpotShadows()` -> **10
hits**. Every one is a loop bound, a clamp, a static-array size, or a comment — **all
auto-adjust**; **none hard-codes `6`** (**PROVES**, lines cited):

Runtime clamp (the ceiling enforcement):
- `pipeline.cpp:598-601` `bdmergeMaxSpotShadows()` = `llclamp((U32)max_spots, 2u,
  LLPipeline::MAX_SPOT_SHADOWS)` — clamps to the constant, so raising the constant raises the
  runtime cap automatically. **PROVES**.

Loops / array sizes keyed on `MAX_SPOT_SHADOWS` (all auto): :677, :1603, :2040, :3055, :9373,
:9522, :9565, :16021 (`i < 4 + MAX_SPOT_SHADOWS` bind loop), :16296-16297 (`spot_shadow_softness[MAX_SPOT_SHADOWS]` C-side), :16646, :17575, :17823, :18907
(`llassert(i < MAX_SPOT_SHADOWS)`), :19886 & :20157-20161 (`static LLCullResult
result[MAX_SPOT_SHADOWS]` — the arrays whose being `[2]` was the old slot-3 crash), plus
comments :597, :16190. **PROVES** each is size/bound-derived.

Loops keyed on the runtime getter `bdmergeMaxSpotShadows()` (all auto): :1602, :1644, :13799,
:14614, :17611, :19698, :19791, :20001 (plus comment :14226). **PROVES**.

Matrix upload uses `MAX_SHADOW_MATS` (auto -> 12):
- `pipeline.cpp:16191-16200` builds `F32 mat[16*MAX_SHADOW_MATS]` and uploads
  `MAX_SHADOW_MATS` matrices to `DEFERRED_SHADOW_MATRIX`. **PROVES** the uniform upload count
  tracks the constant.

**Verdict (§1):** In `pipeline.h` + `pipeline.cpp`, the *only* required edit is
`pipeline.h:1030` `6 -> 8`. Everything else in these two files follows.

---

## 2. The hard-coded C++ that does **not** auto-adjust

### 2.1 Camera-id enum — REQUIRED edit (correctness, not cosmetic)

`indra/newview/llviewercamera.h:54-59` declares exactly six spot cameras:
```cpp
CAMERA_SPOT_SHADOW0, CAMERA_SPOT_SHADOW1, CAMERA_SPOT_SHADOW2, // [BDMerge NSpot]
CAMERA_SPOT_SHADOW3, CAMERA_SPOT_SHADOW4, CAMERA_SPOT_SHADOW5,
CAMERA_WATER0, ...  NUM_CAMERAS
```
The shadow render loops set the current camera by **arithmetic offset**:
`pipeline.cpp:19737, 19888, 20297` all do
`sCurCameraID = (eCameraID)(CAMERA_SPOT_SHADOW0 + i)` where `i` runs to `num_spots-1`.
**PROVES**: with `num_spots = 8`, `i = 6,7` would index `CAMERA_WATER0 / CAMERA_WATER1` —
the wrong camera id, silently corrupting the water/other camera state during projector
shadow generation. **Required edit:** insert `CAMERA_SPOT_SHADOW6, CAMERA_SPOT_SHADOW7` before
`CAMERA_WATER0` (`llviewercamera.h:59`). `NUM_CAMERAS` and any `NUM_CAMERAS`-sized arrays
then grow automatically. **PROVES** (enum is contiguous, `NUM_CAMERAS` is the sentinel :64).

### 2.2 Shader sampler declarations + dispatch — REQUIRED (see §2/§3 shader detail below)

### 2.3 LLShaderMgr uniform table + enum — REQUIRED (see §3)

**These three are the entire hand-extended surface.** Everything else scales off
`MAX_SPOT_SHADOWS` / `bdmergeMaxSpotShadows()`.

---

## 3. Shader + uniform-binding changes

### 3.1 `shadowUtil.glsl` (the ONLY shader file that declares the samplers/dispatch)

`indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl`. The two consumers
(`class3/deferred/spotLightF.glsl`, `class3/deferred/projectorVolumetricF.glsl`) only
*call* `sampleSpotShadow()` / `sampleSpotShadowConservative()` — they do **not** declare
samplers. **PROVES**: `spotLightF.glsl:94` and `projectorVolumetricF.glsl:280,284` are bare
forward declarations `float sampleSpotShadow(...)` / `sampleSpotShadowConservative(...)`; the
grep for `uniform sampler` in those files returns only `environmentMap / lightMap /
lightFunc` (no shadow samplers). So the sampler/dispatch edit is centralized in one file.

Edits in `shadowUtil.glsl`:

1. **Sampler declarations** :36-42 — add two under the `#if defined(SPOT_SHADOW)` block:
   ```glsl
   uniform sampler2DShadow shadowMap10;
   uniform sampler2DShadow shadowMap11;
   ```
   **PROVES** current block ends at `shadowMap9` (:42).
2. **Matrix array** :49 `uniform mat4 shadow_matrix[10];` -> `[12]`. **PROVES** (:49).
3. **Softness array** :84 `uniform float spot_shadow_softness[6];` -> `[8]`. **PROVES** (:84).
4. **`sampleSpotShadow()` dispatch** :386-415 — the `if (index==0)...else if(index==4)...else`
   chain maps slot 0..5 to `shadowMap4..9` / `shadow_matrix[4..9]` /
   `spot_shadow_softness[0..5]`. **PROVES** (:388-414). Extend to `index==5 -> shadowMap9`,
   `index==6 -> shadowMap10`, `else(7) -> shadowMap11`, with matching
   `shadow_matrix[10..11]` and `spot_shadow_softness[6..7]`.
5. **`sampleSpotShadowConservative()` dispatch** :522-542 — parallel `if/else` mapping slot
   0..5 to `shadowMap4..9`. **PROVES** (:524-542). Extend identically for slots 6,7.

The sun-shadow paths (`pcfShadow`, `nonpcfShadow`, `sampleSunShadow` using
`shadowMap0..3`, `shadow_matrix[0..3]`, :286-344, :551-595) are **untouched** — off-limits
(§OFF-LIMITS).

### 3.2 LLShaderMgr uniform registration

`indra/llrender/llshadermgr.cpp:1428-1437` pushes `shadowMap0..shadowMap9` reserved-uniform
names. **PROVES**. Add `shadowMap10`, `shadowMap11` after :1437.

`indra/llrender/llshadermgr.h:226-235` declares the parallel enum `DEFERRED_SHADOW0..
DEFERRED_SHADOW9` (contiguous, before `DEFERRED_POSITION` :236). **PROVES**. Add
`DEFERRED_SHADOW10, DEFERRED_SHADOW11` after :235. The bind loops index by
`DEFERRED_SHADOW0 + i` (`pipeline.cpp:16013, 16023`), so contiguity is load-bearing —
**PROVES** the two new enum entries must sit immediately after `DEFERRED_SHADOW9`.

**Stale-assert finding:** `llshadermgr.cpp:1439`
`llassert(mReservedUniforms.size() == LLShaderMgr::DEFERRED_SHADOW5+1);` is **already stale** —
the loop pushes through `shadowMap9` (`DEFERRED_SHADOW9`), so the true size is
`DEFERRED_SHADOW9+1`, four more than the asserted `DEFERRED_SHADOW5+1`. **INFERS**: `llassert`
must be compiled out in this build (the fork runs), otherwise startup would trip. When adding
slots this assert should be corrected to `DEFERRED_SHADOW11+1` (or fixed to
`DEFERRED_SHADOW9+1` now); leaving it merely perpetuates a dead assert. Flag it (§risks).

### 3.3 The bind loop and matrix upload (auto — no edit)

- Shadow-map texture binding: `pipeline.cpp:16006-16033` (`bindShadowMaps`) binds sun 0..3
  then `for (i = 4; i < 4 + MAX_SPOT_SHADOWS; i++)` -> `enableTexture(DEFERRED_SHADOW0 + i)`
  bound to `getSpotShadowTarget(i-4)`. **PROVES** the loop auto-covers slots 6,7 once the
  constant and the enum grow. No edit here.
- Matrix upload: `pipeline.cpp:16200` uploads `MAX_SHADOW_MATS` matrices (auto -> 12). The
  GLSL `shadow_matrix[12]` (§3.1) must match. **PROVES/IMPLIES**.
- Per-slot softness upload: `pipeline.cpp:16296-16308` builds `spot_shadow_softness[MAX_SPOT_SHADOWS]` (auto) and uploads `MAX_SPOT_SHADOWS` floats to `SPOT_SHADOW_SOFTNESS`
  (registered `llshadermgr.cpp:1760` as `"spot_shadow_softness"`). The GLSL array must be
  `[8]` (§3.1 edit 3). **PROVES** the C side auto-sizes; only the GLSL literal needs bumping.

---

## 4. Texture-unit feasibility (the real risk) — VERDICT: 8 FITS

### 4.1 Worst-case fragment sampler budget (deferred spot-light program)

`bindDeferredShader` + `bindShadowMaps` + `bindReflectionProbes` bind, per fragment stage:

| Group | Uniforms | Units | Cite |
|---|---|---|---|
| G-buffer | diffuse, specular, normal, emissive, depth | 5 | `pipeline.cpp:16098-16138` |
| Misc | exposure, noise, lightFunc, brdfLut, lightMap | 5 | :16140,16159; bindLightFunc :15993-16002; :16171 |
| Sun shadows | shadowMap0..3 | 4 | :16008-16019 |
| Spot shadows | shadowMap4..N | **6 -> 8** | :16021-16032 |
| Reflection probes | reflectionProbes, irradianceProbes, heroProbe | 3 | :17949-17966 |
| SSR (optional) | sceneMap, sceneDepth | 0-2 | :17993,18023 |
| Projector cookie | projectionMap | 1 | :17654 / :17839 |

**Today (6 spots, probes on, SSR on):** ~5+5+4+6+3+2+1 = **~26 channels**.
**At 8 spots:** **~28 channels**. **PROVES** the counts from the cited `enableTexture` sites.

`environmentMap` (:16206) is bound **only when reflection probes are disabled**
(`if (!sReflectionProbesEnabled)`) — mutually exclusive with the 3 probe units, so it does
not stack. **PROVES** (:16204-16206).

### 4.2 Against the limits

- GL 4.x **guaranteed minimum** `GL_MAX_TEXTURE_IMAGE_UNITS` (per fragment stage) = **16**.
- This fork **already** binds ~26 fragment channels at 6 spots — *already 10 over the
  guaranteed minimum*. **IMPLIES** the fork already requires a GL that provides **32** per-stage
  texture image units (the near-universal desktop value on NVIDIA/AMD/Intel). The code reads
  it at runtime: `gGLManager.mNumTextureImageUnits = glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS)`
  (`llgl.cpp:1379`). **PROVES** the value is queried.
- There is already a near-budget diagnostic: `llglslshader.cpp:520-530` logs at `>24`
  channels and **warns** `mActiveTextureChannels > gGLManager.mNumTextureImageUnits` with
  "samplers beyond the limit silently read unit 0!" — comment tagged `[BDMerge NSpot diag]`.
  **PROVES** the authors already knew NSpot pushes the program to the texture-unit edge.

**Verdict:** On a 32-unit GPU, 8 spots (~28 channels) fits with ~4 headroom. On a 16-unit
GPU the fork is **already broken at 6 spots** (silent unit-0 reads) — 8 changes nothing about
that pre-existing state. **INFERS** the safe hard ceiling before *any* config crosses 32 is
**~10-12 spot shadows** (each further spot = +1 fragment sampler on the worst-case program;
28 at 8 -> 32 at ~12). Beyond ~12 you must drop something else (e.g. gate SSR) to stay legal.

### 4.3 Recommended hardware gate

Add a gate so 8-spot is only *reachable* where units allow, falling back to 6 on constrained
GPUs. Cleanest location: **`bdmergeMaxSpotShadows()` (`pipeline.cpp:598-601`)** — the single
choke point every consumer routes through. Change the upper clamp from the raw constant to a
hardware-aware ceiling:

```cpp
// design sketch
U32 hw_ceiling = (gGLManager.mNumTextureImageUnits >= 32)
                 ? LLPipeline::MAX_SPOT_SHADOWS   // 8
                 : 6;                             // constrained GPU fallback
return llclamp((U32)max_spots, 2u, hw_ceiling);
```
**INFERS** this is the least-invasive gate: it needs no new plumbing (`gGLManager` is a global,
already used by `llfeaturemanager.cpp:801-807` which masks features at `<=8` / `<=16` units —
**PROVES** the pattern of gating on `mNumTextureImageUnits` is already established). It also
means the *arrays/RTs* still size to 8 but only 6 are ever allocated/rendered on weak GPUs
(the `handleShadowsResized` loop releases slots `>= num_spots`, `pipeline.cpp:1603-1608` —
**PROVES**). Alternative (heavier): a dedicated `maskFeatures` entry in `featuretable` — not
recommended; it cannot express a numeric fallback as cleanly.

---

## 5. RT allocation & per-frame cost

### 5.1 Allocation

`pipeline.cpp:1592-1614` (`handleShadowsResized` path): when `shadow_detail > 1`, for
`i < num_spots` it `mSpotShadow[i].allocate(spot_w, spot_h, 0, /*depth=*/true)`; slots
`>= num_spots` are released. **PROVES**. Resolution = sun-shadow width unless
`BDMergeProjectorShadowResolution >= 256`, clamped `256..8192` (:1597-1601). The Prism aux
maps `mPrismSpotShadow[]` are the same size, allocated lazily and released together
(`pipeline.h:1041`; release `pipeline.cpp:2036-2046`). **PROVES**.

### 5.2 VRAM delta (2 extra slots)

Each spot map is a **depth** target (`allocate(...,0,true)`), ~4 bytes/texel. Two extra slots:

| Spot resolution | +2 main maps | +2 Prism aux (if active) |
|---|---|---|
| 1024² | ~8 MB | ~16 MB |
| 2048² | ~32 MB | ~64 MB |
| 4096² | ~128 MB | ~256 MB |

**INFERS** (res² x 4 x 2). At a typical 2048² projector-shadow res, going 6 -> 8 costs
**~32 MB** (main) or **~64 MB** with the Prism lens active.

### 5.3 Per-frame cost

Two extra shadow slots => up to **2 extra cull passes + 2 extra depth-only shadow renders per
frame**, but only when 8 projectors actually qualify (loops run to `num_spots`, and each slot
early-outs when `mShadowSpotLight[i]==NULL` — `pipeline.cpp:14616-14620, 19698+`). **PROVES**
the cost is proportional to *active* casters, not the ceiling. The projector-volumetric shaft
pass also iterates `bdmergeMaxSpotShadows()` (:14614) — 2 more potential shaft marches.
**INFERS** cost tradeoff: linear in active shadow-casting projectors; the resolution lever
(`BDMergeProjectorShadowResolution`) dominates VRAM/fill far more than slot count.

---

## 6. Rig auto-active-count (the "no manual setting" goal)

### 6.1 What exists today (manual, capped at 6)

`indra/newview/alpanelcinelightrig.cpp`:
- `computeRequestedShadowSlots()` :803-821 — mode 1 (Key only) => `KeyOn ? 1 : 0`; mode 2 (All
  compete) => count of enabled roles `ROLE_ON_SETTINGS[i]` over `LIGHT_COUNT` (=4,
  `alcinelightrigmodel.h:17`). **PROVES**.
- `onClickShadowFixIt()` :823-828 — a **manual button** sets
  `BDMergeMaxSpotShadows = clamp(requested, 2u, 6u)`. **PROVES** the write already exists and
  is clamped to 6.
- `updateDerivedStatus()` :870-888 — shows a hint "N rig projectors request M shadow slots.
  Raise Max Spot Shadows..." and a "Allow N spot shadows" fix-it button when
  `requested > slots`. **PROVES** the current UX is *notify + one click*, not automatic.

### 6.2 Priority is already categorical — the only gate is active count

`alcinelightrigmanager.cpp:466-471`: "each enabled rig casts per its own ShadowMode and the
pipeline caps the total at MAX_SPOT_SHADOWS(6) by priority... a suppressed projector loses its
shadow slot AND its shaft." **PROVES** rig projectors already win slots over world lights;
the sole remaining bottleneck to "more than 2 cast at once" is the **runtime active count**
(`BDMergeMaxSpotShadows`), not slot priority.

### 6.3 Multi-instance reality

Up to `ALCineLightRigSlot::COUNT = 5` rig instances (SELF, A, B, C, D —
`alcinelightrig.h:26-34`), each with up to `LIGHT_COUNT = 4` projectors, i.e. a theoretical
**20** requested slots — far over 8. **PROVES** the requested sum must be **capped at
`MAX_SPOT_SHADOWS`**. `computeRequestedShadowSlots()` today reads the *selected* rig's role
settings only; a multi-instance-correct version must sum enabled shadow-casters across enabled
instances, then `min(sum, 8)`.

### 6.4 Recommendation — option (a), scoped + opt-in

Chosen over (b) "reserve N slots" (needs a new reservation channel through the whole spot
assignment path — heavy, and redundant given §6.2 priority) and (c) "just auto-raise, never
restore" (silently ratchets a global up and leaves it — worst surprise).

Design:
1. New bool setting **`CineLightRigAutoShadowSlots`** (Persist, default **1/on**), sibling of
   the existing `CineLightRig*` keys in `settings.xml`.
2. When on, on rig enable / shadow-mode change / role toggle, compute
   `needed = min(sum_requested_across_enabled_instances, MAX_SPOT_SHADOWS)` and, **only if it
   raises the value**, set `BDMergeMaxSpotShadows = max(current, needed)`. This reuses the
   existing `computeRequestedShadowSlots()` (extended per §6.3) and the existing write at
   `alpanelcinelightrig.cpp:826`. **INFERS** raise-only avoids fighting a user who set a high
   value manually.
3. **Restore-on-disable:** remember the pre-rig baseline (a transient, e.g.
   `mAutoShadowBaseline`, captured the first time the rig raises it) and lower back to it when
   the rig is disabled / auto turned off — *only if* the current value still equals what the
   rig last wrote (don't clobber a manual change made meanwhile). **INFERS** least-surprise.
4. Keep the existing hint/fix-it (:870-888) as the manual fallback for when
   `CineLightRigAutoShadowSlots` is off.

**Surface it (required):** this writes a *persisted global* on the user's behalf. Recommend a
one-line note in the rig panel ("Auto-raises Max Spot Shadows to N") next to the toggle, and
the setting defaults on but is discoverable/undoable. **INFERS**.

Edits for the rig side:
- `alpanelcinelightrig.cpp:827` `clamp(...,2u,6u)` -> `2u,8u`; `:882` `clamp(requested,2,6)`
  -> `2,8`. **PROVES** these literals cap the rig at 6 today.
- Extend `computeRequestedShadowSlots()` to sum across enabled instances and add the
  auto-apply hook + baseline restore.
- Add `CineLightRigAutoShadowSlots` to `settings.xml`.

---

## 7. `settings.xml` and UI

- `settings.xml:6815-6825` `BDMergeMaxSpotShadows`: **comment** :6818 says "stock viewer: 2,
  max 6" -> "max 8". **Value stays 2**, Type `U32`, Persist 1. No clamp lives here (the clamp
  is code-side, §1.3). **PROVES**. No default change => no migration needed.
- `floater_lightbox_settings.xml:2537` slider `max_val="6"` -> `"8"`. **PROVES** (the
  `BDMergeMaxSpotShadows` slider, control bound :2529). `min_val="2"`, `increment="1"` stay.
- Add `CineLightRigAutoShadowSlots` (§6.4) to `settings.xml`.
- Registration side-effects: `BDMergeMaxSpotShadows` already triggers live reallocation via
  `setting_setup_signal_listener(..., handleShadowsResized)` (`llviewercontrol.cpp:1155`) —
  **PROVES** raising the value at runtime reallocates spot RTs with no restart. The rig
  auto-write therefore takes effect the same frame.

---

## 8. Interaction with the per-light shadow-softness registry

The registry resolves softness per *current projector-shadow slot*: `pipeline.cpp:16296-16308`
loops `i < MAX_SPOT_SHADOWS`, looks up `mShadowSpotLight[i]->getVOVolume()->getID()` via
`getProjectorShadowSoftness(...)`, and uploads `MAX_SPOT_SHADOWS` floats. **PROVES** it is
already indexed by `MAX_SPOT_SHADOWS`, so the **C side auto-extends to 8**. The only registry-
adjacent edit is the **GLSL array literal** `spot_shadow_softness[6] -> [8]`
(`shadowUtil.glsl:84`, §3.1). **IMPLIES**: with that one bump the softness registry covers all
8 slots; slots 6,7 read `spot_shadow_softness[6..7]` in the extended dispatch (§3.1 edit 4).

---

## §TEST PLAN

Pure-testable (no GPU) units:

1. **Clamp** — `bdmergeMaxSpotShadows()` behaviour (`pipeline.cpp:598-601`): assert
   `clamp(input, 2, ceiling)` for input {0,1,2,6,7,8,99} at ceilings 6 and 8, and (with the
   §4.3 gate) that `mNumTextureImageUnits < 32` pins ceiling at 6. Extract the clamp into a
   free function for host-testability if not already.
2. **Slot -> sampler/matrix index mapping** — a table test asserting the dispatch in
   `shadowUtil.glsl` (§3.1) maps slot `k -> shadowMap[4+k]`, `shadow_matrix[4+k]`,
   `spot_shadow_softness[k]` for k in 0..7, and that the camera-id offset
   `CAMERA_SPOT_SHADOW0 + k` stays `< CAMERA_WATER0` for k in 0..7 (guards the §2.1 bug).
   This is verifiable by parsing the enum / a small C++ mirror of the mapping.
3. **Rig active-count computation** — `computeRequestedShadowSlots()` (extended): parametrize
   ShadowMode {0,1,2}, `KeyOn` {t,f}, role-on masks, and instance-enabled masks; assert
   `needed == min(sum_enabled_casters, 8)` and the raise-only / restore-baseline logic
   (`current`, `lastWritten`, disable) via a settings mock. There is already a rig test
   harness: `indra/newview/tests/alcinelightrigmodel_test.cpp`. **PROVES** a test target
   exists to extend.

GPU / integration (manual, not unit): shader compiles with 12 samplers on a 32-unit GPU;
`llglslshader.cpp:520-530` logs channel count but no `EXCEEDS` warning; 8 projectors visibly
cast simultaneously; 6-and-below output unchanged (byte-diff a capture at 6).

---

## §RISKS

1. **Texture-unit overflow on low-end** — a 16-unit GPU silently reads unit 0 for
   over-budget samplers (`llglslshader.cpp:527-529`). The fork is *already* over 16 at 6
   spots, but 8 widens the gap. **Mitigation:** §4.3 hardware gate pins low-unit GPUs at 6
   (or lower). Without the gate, low-end sees corrupt shadows, not a crash.
2. **Shader compile failure from a missed case** — if any of {sampler decl, `shadow_matrix[12]`,
   `spot_shadow_softness[8]`, both dispatch chains} is left at the old size, the program
   either fails to link or samples the wrong slot. Both dispatch functions
   (`sampleSpotShadow` :386-415 **and** `sampleSpotShadowConservative` :522-542) must be
   edited together. **Mitigation:** test 2 above; keep both chains adjacent.
3. **Camera-id corruption (§2.1)** — forgetting `CAMERA_SPOT_SHADOW6/7` makes slot 6/7 stomp
   `CAMERA_WATER0/1` state — a subtle, non-crashing water/reflection glitch. **High-priority**
   edit; test 2 guards it.
4. **Stale `llassert` (`llshadermgr.cpp:1439`)** — already wrong (references
   `DEFERRED_SHADOW5+1`); if a build ever enables `llassert`, both today's code and the 8-spot
   change trip it. Fix to the correct `DEFERRED_SHADOW11+1` while here.
5. **VRAM** — +32 MB (2048², main) or +64 MB (with Prism aux). Negligible on modern cards,
   non-trivial at 4096². Resolution lever dominates.
6. **Perf** — up to +2 shadow renders + 2 shaft marches/frame when 8 casters active; scales
   with active casters, capped by the runtime count.
7. **Auto-setting-write surprise (§6)** — silently raising a persisted global. **Mitigation:**
   opt-in default-on bool, raise-only, restore-on-disable, surfaced in the panel.

---

## §OFF-LIMITS (must stay byte-identical)

- **Sun-shadow behaviour** — `shadowMap0..3`, `shadow_matrix[0..3]`, the 4 sun cascades and
  `sampleSunShadow` (`shadowUtil.glsl:286-344, 551-595`; `bindShadowMaps` sun loop
  `pipeline.cpp:16008-16019`). Do not touch.
- **6-and-below output** — with `BDMergeMaxSpotShadows <= 6` and unchanged resolution, the
  rendered result must be identical. The clamp default (2) and the additive `4 + i` dispatch
  guarantee this if only the *ceiling* and the *new slots 6,7* are added; the existing slot
  0..5 branches are not reordered.
- **Categorical rig priority** — the "rig wins slots over world" policy
  (`alcinelightrigmanager.cpp:466-471`) stays; this change only raises the *active count*, not
  the priority.
- **Prism aux pass semantics** — `mPrismSpotShadow[]` lifetime/redirect
  (`getSpotShadowTarget` :18905-18912) unchanged beyond auto-resize.

---

## §Is 8 the right target? What caps it?

- **What caps it:** the deferred **spot-light fragment program's texture-unit budget**, not
  the shadow subsystem. At 8 spots the worst-case program uses ~28 of the 32 units modern
  desktop GL guarantees; each further spot is +1 unit. **INFERS** the numeric ceiling before a
  legal config crosses 32 is **~10-12** spot shadows (then you'd have to trade away SSR /
  hero-probe / an env unit).
- **Is 8 right:** yes as the *default* max — it doubles the useful multi-projector case
  (cinematic 3-4 key/fill/rim setups per subject, or 2 subjects) while staying ~4 units under
  the desktop limit and adding only ~32 MB VRAM. **INFERS**.
- **Configurable higher (10/12):** feasible *only* behind the §4.3 hardware gate and only by
  raising `MAX_SPOT_SHADOWS` further (all arrays/loops already scale). It would demand: two
  more sampler decls + dispatch cases + camera-ids + uniform-table entries **per slot**, and
  would leave near-zero texture-unit headroom on the spot-light program — meaning SSR or a
  probe unit would have to become mutually exclusive with the top slots. **Recommendation:**
  ship **8** as the constant; if a "10/12 extreme" mode is ever wanted, make it a *second*
  gated tier that also disables SSR on the spot pass to stay under 32. Do **not** raise beyond
  what `GL_MAX_TEXTURE_IMAGE_UNITS` can prove at runtime.
