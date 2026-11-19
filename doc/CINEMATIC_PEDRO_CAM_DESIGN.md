# Pedro Cam — Fisheye Peephole + Beat-Bob Camera Mode — Design & Implementation Spec

**Feature:** Pedro Cam (`CineFisheye*` post pass + `CinematicCamPedro*` camera pattern).
**Look:** the "Pedro the raccoon" meme — an extreme fisheye/barrel-bulged circular image inside a dark
circular peephole, camera parked close and low on the subject's face, swaying side-to-side and bobbing
vertically on a fixed BPM with a little sympathetic roll. One button sets the whole meme.

**Architecture:** three cooperating pieces, each using an idiom this codebase already ships:
1. **Fisheye + peephole post pass** — a LATE fullscreen UV-remap in `renderFinalize`'s LDR ping-pong chain
   (a sibling of CAS/FXAA, NOT a G-buffer pass like the Outline). One shader, one draw, zero cost disabled.
2. **Wide render FOV** — via the existing `fov_mul` plumbing in `LLCinematicCamera::updateCamera`.
3. **`MODE_PEDRO_BOB` pattern** — a new appended `EMode` producing the sway/bob/roll, presentation-clock
   addressed (deterministic / scrub-safe), subject-locked at close/low framing.

Reuse `doc/CINEMATIC_OUTLINE_MODE_DESIGN.md` for the registration idioms — this is a sibling post pass;
only the chain position differs (a lens warp must distort the FINISHED image, so it goes last, not pre-bloom).

## 1. Confirmed pipeline ground truth (file:line)

### The late post chain — `LLPipeline::renderFinalize` (`pipeline.cpp:15670`)
- `colorCorrect(&mRT->screen, &mRT->postPingMap, hdr, true)` :15745 — tonemap/grade lands in `postPingMap`.
- **Ping-pong init** :15756-15757: `sourceBuffer = &mRT->postPingMap; targetBuffer = &mRT->postPongMap;`.
  Every late pass is `pass(sourceBuffer, targetBuffer); std::swap(sourceBuffer, targetBuffer);`:
  volumetric godrays :15762-15766, FXAA :15772-15776, SMAA :15777-15782, CAS :15784-15789,
  legacy `combineGlow` :15791-15795, motion blur :15800-15805, DoF :15813-15818, RLV `@setsphere`
  :15827-15832. Then debug overlays (:15835, :15872) and the final blit :15877.
- **Insertion point: `pipeline.cpp:15833`** — immediately AFTER the RLV sphere block (:15832), BEFORE
  `RenderBufferVisualization` (:15835). Rationale: (a) after DoF (:15816) and motion blur (:15803), which
  read the un-warped depth/velocity buffers — warping first would misregister them and DoF/blur streaks
  would bend wrongly; (b) after AA/CAS so their neighborhood filters see undistorted texels; (c) after the
  RLV privacy sphere, which is depth-registered; (d) `sourceBuffer` then feeds the blit directly (:15885).
- **Final blit `gBlitWithEffectsProgram`** :15877-15957 already applies a settings-driven vignette
  (`RenderVignette*`, :15893-15920, default amount 0), film grain :15928-15940, dither :15943. These run
  AFTER our warp in un-warped screen space — correct: grain must not warp; the stock vignette stays an
  independent, defaulted-off knob. Our peephole is built into the fisheye shader (below) because it must
  share the warp's circle geometry exactly.
- `renderFinalize` never runs for cube snapshots (`llassert(!gCubeSnapshot)` :15675).

### The model pass to clone — `LLPipeline::applyCAS` (`pipeline.cpp:11979-12021`)
Canonical src→dst fullscreen pass: `dst->bindTarget()` :11990 → `gCASProgram.bind()` :11992 → uniforms →
`bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, src, false, LLTexUnit::TFO_POINT)` :12012 →
`mScreenTriangleVB->setBuffer(); drawArrays(TRIANGLES,0,3)` :12015-12016 → `unbind(); dst->flush()`.
Difference for us: bind the source with **`TFO_BILINEAR`** (a UV remap needs filtered fetches; CAS wants
point). Fullscreen-tri + shader idiom also per `renderVolumetric` :12329 (see Outline spec §1).

### Camera — `indra/newview/llcinematiccamera.{h,cpp}`
- `EMode` enum `llcinematiccamera.h:40-96`; **append-only** — `static_assert`s at h:97-100 pin
  `MODE_BREATHING_HOLD==42` and `MODE_STATIC_WIDE..MODE_STATIC_FULL==43..50`. New mode is
  **`MODE_PEDRO_BOB = 51`**, appended after `MODE_STATIC_FULL` (h:95), with its own static_assert.
- **Dispatch:** `updateCamera` switch `llcinematiccamera.cpp:2735-2790`. Locals `mode_fov_mul` :2732 and
  `mode_roll` :2733 are the FOV/roll out-params; roll plumbing is proven by
  `case MODE_BARREL_ROLL: patternBarrelRoll(..., mode_roll)` :2759.
- **Determinism:** `LLPresentationTime::currentFrame().presentation_time` :2582-2586;
  `mPhase = fmod(presentation_time - mSwitcherPhaseAnchor, PHASE_WRAP)` :2699-2700 with the explicit
  comment (:2696-2698) that pattern motion is presentation-clock addressed and scrub-stable. Our sines of
  `mPhase` inherit this for free.
- **Subject/head lock:** focus resolves to the `mHead` joint when look-at-head is in effect :2703-2710;
  `triggerMode(EMode, bool force_look_at_head)` h:155 is the one-shot UI seam.
- **FOV write path:** `final_fov = cam->getDefaultFOV() * mode_fov_mul * fov_mul` :2962 →
  `cc_applyFrameLens` :2970 → `setViewNoBroadcast(llclamp(final_fov, cam->getMinView(), cam->getMaxView()))`
  :2979-2980 during eases, else `cam->setView(final_fov)` :2986. Camera limits: `DEFAULT_FIELD_OF_VIEW =
  60°` `llmath/llcamera.h:36`, `MAX_FIELD_OF_VIEW = 175°` :55. Patterns clamp `fov_mul` to [0.05, 4]
  (e.g. :1627), so 60° × 2.3 ≈ 138° is reachable. **Verify at runtime** what `getMaxView()` actually
  returns (risk §7) — even if it bites at ~120°, the post warp carries the look.
- **Pattern idioms to copy:** `patternPendulum` :1579-1594 (in-function `LLCachedControl` settings
  `CinematicCam*`, `sinf(phase * w)`, `motionStartAzimuth`); `patternFisheyeLunge` :1617-1633 (front-of-face
  placement via `cc_avatarYaw(av)`, `fov_mul` from a setting); `patternFloorSkimmer` :1637-1658 (low height).
- `modeName()` h:119 needs a "Pedro Cam" entry; `migrateLegacyMode` untouched (values append-only).

### Registration idioms (identical to the Outline spec §3)
Program template `gVolumetricLightProgram` `llviewershadermgr.cpp:3116-3136` (VS
`deferred/postDeferredNoTCV.glsl` fullscreen tri, `mShaderLevel[SHADER_DEFERRED]`, soft-disable on
`createShader()` failure); `mShaderList.push_back` near :450; extern in `llviewershadermgr.h` (:224 area,
near `gCASProgram`, declared at `llviewershadermgr.cpp:222`). Reserved uniforms: enum in `llshadermgr.h` +
matching `mReservedUniforms.push_back` in `llshadermgr.cpp` (pattern `"gobo_anim_params"` :1770 —
**ENUM ORDER = PUSH ORDER**). Settings: in-function `static LLCachedControl<>` (pattern :15784). UI:
Director floater / `panel_cine_light_rig.xml` precedent.

## 2. Fisheye + peephole shader — new `class1/deferred/cineFisheyeF.glsl` (VS: `postDeferredNoTCV.glsl`)

Uniforms: `diffuseMap` (DEFERRED_DIFFUSE = src), `screen_res` (DEFERRED_SCREEN_RES),
`fisheye_params` = (x `k1`, y `k2`, z `vign_radius`, w `vign_soft`),
`fisheye_params2` = (x `center_x`, y `center_y`, z `zoom`, w reserved P2 beat-phase). Two new reserved
uniforms `fisheye_params` / `fisheye_params2`.

**Destination→source radial remap (edge-anchored barrel), aspect-true circle:**
```glsl
vec2 uv = vary_fragcoord.xy;                       // [0,1]
float aspect = screen_res.x / screen_res.y;
vec2 c = uv * 2.0 - 1.0 - fisheye_params2.xy * 2.0; // centered [-1,1]
c.x *= aspect;                                     // circular space: r=1 at top/bottom edge
float r  = length(c);
float rn = r / max(fisheye_params.z, 0.05);        // normalize to the peephole radius
float k1 = fisheye_params.x, k2 = fisheye_params.y;
float g  = (1.0 + k1*rn*rn + k2*rn*rn*rn*rn) / (1.0 + k1 + k2);   // edge-anchored: g(1)=1
vec2 src = c * g / max(fisheye_params2.z, 0.25);   // zoom>1 crops in (hides any edge pull)
vec2 suv = vec2(src.x / aspect, src.y) * 0.5 + 0.5 + fisheye_params2.xy;
vec3 col = texture(diffuseMap, suv).rgb;
// hard black outside the source frame (defensive; edge-anchoring keeps suv in range inside the circle)
if (any(lessThan(suv, vec2(0.0))) || any(greaterThan(suv, vec2(1.0)))) col = vec3(0.0);
// circular peephole: bright bulge fading to black surround, sharing the warp's circle
float v = 1.0 - smoothstep(fisheye_params.z - fisheye_params.w,
                           fisheye_params.z + fisheye_params.w, r);
frag_color = vec4(col * v, 1.0);
```
Why this math: `g < 1` near center (center magnified = bulge), rising monotonically to `g = 1` at the
peephole rim, so the un-warped rim meets the vignette exactly and no out-of-range sampling occurs inside
the visible circle. `k1` is the strength knob (0 = identity + vignette only; 1.2 = meme); `k2` sharpens the
edge compression. Alpha forced to 1 (final blit reads only rgb; don't leak scene alpha through the warp).
Sample the source **bilinear** (one fetch, no dependent chain). Phase 2 variants: equidistant true-fisheye
`g = tan(rn*theta)/(rn*tan(theta))`; per-channel `k1` offsets for chromatic aberration; `k1` pulse on the
beat via `fisheye_params2.w` (CPU-precomputed from presentation time, per the Outline pulse idiom).

## 3. Pipeline integration

**New `LLPipeline::renderCineFisheye(LLRenderTarget* src, LLRenderTarget* dst)`** — clone `applyCAS`
:11979-12021 exactly, except `TFO_BILINEAR` on the source bind and the uniforms above (upload from the
`CineFisheye*` settings, clamped as in §4). Declare in `pipeline.h` next to `applyCAS`.

**Insertion (`pipeline.cpp:15833`, after the RLV block, before buffer visualization):**
```cpp
static LLCachedControl<bool> fisheye_enabled(gSavedSettings, "CineFisheyeEnabled", false);
if (fisheye_enabled && !gSnapshotNoPost && gCineFisheyeProgram.isComplete())
{
    renderCineFisheye(sourceBuffer, targetBuffer);
    std::swap(sourceBuffer, targetBuffer);
}
```
Gate at the call site (like CAS :15785) so disabled = no draw, no copy, zero cost. Keep the internal
`copyRenderTarget` early-out (like :11983-11987) as belt-and-braces if the program failed to compile.

**Registration:** clone the `gVolumetricLightProgram` block minus atmos/shadows: VS `postDeferredNoTCV.glsl`,
FS `cineFisheyeF.glsl`, class1, `mShaderLevel[SHADER_DEFERRED]`, soft-disable on failure; extern next to
`gCASProgram`; `mShaderList.push_back` (:450); reserved uniforms `fisheye_params`/`fisheye_params2` in
`llshadermgr.{h,cpp}` (enum order = push order).

## 4. The Pedro-bob camera pattern — `MODE_PEDRO_BOB = 51`

**New generator** (declare in h next to `patternBreathingHold` :223):
```cpp
LLVector3 patternPedroBob(LLVOAvatar* av, const LLVector3& focus, F32 phase,
                          F32& roll_out, F32& fov_mul);
```
**Dispatch:** add to the switch after `MODE_BREATHING_HOLD` (:2777):
`case MODE_PEDRO_BOB: pos = patternPedroBob(av, focus, mPhase, mode_roll, mode_fov_mul); break;`
Enum appended at h:95 area; extend the static_assert (`MODE_PEDRO_BOB == 51`); add `modeName` entry.

**Motion math** (all sines of the presentation-clock `phase` — deterministic and scrub-safe per
:2696-2700; settings per the `patternPendulum` idiom :1581-1585):
```cpp
const F32 beats = phase * (bpm / 60.f);                 // CinematicCamPedroBPM
const F32 sway  = sway_amt * sinf(F_PI * beats);        // full L-R cycle = 2 beats
const F32 bob   = -bob_amt * 0.5f * (1.f - cosf(F_TWO_PI * beats)); // one smooth dip per beat
roll_out        = roll_deg * DEG_TO_RAD * sinf(F_PI * beats);       // lean into the sway
fov_mul         = llclamp(fov_deg * DEG_TO_RAD / cam->getDefaultFOV(), 0.05f, 4.f);
// placement: in the subject's face, low, like patternFisheyeLunge :1631
const F32 yaw   = cc_avatarYaw(av);
const LLVector3 dir(cosf(yaw), sinf(yaw), 0.f), perp(-sinf(yaw), cosf(yaw), 0.f);
return focus + dir * distance + perp * sway
             + LLVector3(0.f, 0.f, height + bob);       // height default negative => low angle, aiming up
```
Trigger with `triggerMode(MODE_PEDRO_BOB, /*force_look_at_head=*/true)` (h:155) so `focus` is the head
joint (:2706-2709) — camera below the head looking up = the low peephole angle. Live `cc_avatarYaw` (not
`motionStartAzimuth`) keeps the lens glued to the face if the subject turns, matching FisheyeLunge. FOV
rides the existing :2962 multiply and :2979-2986 clamp; no new FOV plumbing.

## 5. Controls (settings.xml) and the preset button

| Setting | Type | Default | Meaning |
|---|---|---|---|
| CineFisheyeEnabled | Bool | 0 | master gate for the post pass (off = zero cost) |
| CineFisheyeStrength | F32 | 1.2 | `k1`; clamp [0,3] |
| CineFisheyeStrength2 | F32 | 0.3 | `k2`; clamp [0,3] |
| CineFisheyeZoom | F32 | 1.0 | crop-in; clamp [0.5,2] |
| CineFisheyeVignetteRadius | F32 | 0.92 | peephole radius, short-half-dim units; clamp [0.2,1.5] |
| CineFisheyeVignetteSoft | F32 | 0.18 | rim feather; clamp [0.01,1] |
| CineFisheyeCenterX/Y | F32 | 0 / 0 | lens center offset; clamp [-0.5,0.5] |
| CinematicCamPedroBPM | F32 | 148 | beat rate (the meme track sits ~148-150) |
| CinematicCamPedroSway | F32 | 0.22 | lateral half-amplitude, m |
| CinematicCamPedroBob | F32 | 0.10 | vertical dip depth, m |
| CinematicCamPedroRoll | F32 | 6.0 | roll half-amplitude, deg |
| CinematicCamPedroDistance | F32 | 0.9 | camera-to-face distance, m |
| CinematicCamPedroHeight | F32 | -0.35 | camera height relative to head focus, m (low = look up) |
| CinematicCamPedroFov | F32 | 120 | render FOV, deg (clamped by `getMaxView()`) |

**"Pedro Cam" preset button** (Director floater, `llfloaterdirector.cpp`, next to the CineCam mode
controls; XML per `panel_cine_light_rig.xml` idiom): one click sets `CineFisheyeEnabled=TRUE`, writes the
meme defaults above (only if the user hasn't customized — or always; pick "always" for Phase 1, it IS the
preset), then `LLCinematicCamera::instance().triggerMode(LLCinematicCamera::MODE_PEDRO_BOB, true)`.
Toggling off: mode → `MODE_OFF` and `CineFisheyeEnabled=FALSE`. The fisheye pass and the bob pattern remain
independently usable (fisheye over any mode; Pedro bob without the warp).

## 6. Portability / perf / determinism / composition
- **Cost:** one fullscreen pass, 1 bilinear fetch, ~20 ALU, no extra RTs, no dependent reads; strictly
  cheaper than CAS. Disabled = branch not taken.
- **GLSL:** `texture`/`smoothstep`/`tan` only; no derivatives, no texelFetch; GL 3.x class1 floor.
- **Determinism:** shader is a pure function of the source image + settings; camera motion is
  presentation-clock addressed (:2696-2700) — same presentation sample after a scrub = same frame.
- **Ordering:** warp AFTER motion blur (:15803) and DoF (:15816) so their depth/velocity-registered kernels
  run on the un-warped image; grain/dither/stock-vignette in the blit (:15928, :15943, :15904) stay
  screen-space-uniform on top. UI/HUD render after `renderFinalize` — never warped (feature, not bug).
- **Camera-operator overlay:** `LLCameraOperator` handheld (:2943-2952) composes on top of the pattern as
  with every mode — off by default for Pedro (the bob IS the motion), but harmless if on.

## 7. Phased plan (Codex) + risks
**Phase 1 — the full meme in one sitting:** `cineFisheyeF.glsl`; reserved uniforms; program registration;
`renderCineFisheye` + insertion at :15833; `CineFisheye*` settings; `MODE_PEDRO_BOB=51` + static_assert +
`modeName`; `patternPedroBob` + dispatch case; `CinematicCamPedro*` settings; Director "Pedro Cam" preset
button. Acceptance: one click → circular bulged peephole image, subject's face close/low/wide, swaying and
dipping on 148 BPM, roll leaning into the sway; scrub-repeatable; toggle off leaves the chain byte-identical.
**Phase 2:** tap-tempo / beat-phase offset + audio-BPM hook; beat-pulsed `k1` (`fisheye_params2.w`);
chromatic aberration; equidistant true-fisheye mapping tied to the render FOV; preset variants (Goldfish
Bowl = mild k1/no bob; Peephole = vignette only); Director UI sliders panel.
**Risks / P1-exit unknowns:** (a) `cam->getMaxView()` runtime value — if < 120° the FOV push clips (post
warp still sells it; consider raising max under CineCam ownership); (b) interplay with the aspect-aware
Frame lens `cc_applyFrameLens` :2970 at extreme FOV (verify no double-wide surprise); (c) bilinear
minification aliasing in the compressed rim (no mips on postPing/PongMap) — vignette hides most; P2 optional
2-tap supersample; (d) `postPing/PongMap` filterability (expected RGBA filterable; verify no integer
format); (e) snapshot paths: honor `gSnapshotNoPost`, and decide whether hi-res tiled snapshots should warp
per-tile (they shouldn't — gate off when tiling, like other screen-space effects).
