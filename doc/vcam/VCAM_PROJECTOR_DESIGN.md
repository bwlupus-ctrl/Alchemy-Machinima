# VCam Projector — camera feed through a projector light

> **SCOPE (user, 2026-10-12):** the goal is that a VCam output can be used **as a texture** wherever a texture is chosen, in particular the **Light (projector) texture slot** in the build floater's Features tab and the face **Texture** slot. **Primary UX = the standard texture picker** (PICK: TEXTURE): add a **"VCam" source** (alongside Inventory/Local) listing Cam 1-8 with live thumbnails; choosing one applies a **viewer-local override** (like Local textures: no object update is sent and the prim's real texture is untouched), so it works on any object regardless of permissions. Light slot -> projector binding (this doc); face slot -> the existing VCam display binding. Drag-and-drop / Send-to / the Output card remain shortcuts. No undo (user rule).

Status: FEASIBILITY + DESIGN (read-only study, 2026-10-11, branch `feature/director-kit-p0` @ 5f624299d41).
Request (10/11): "it would also be nice if we can project it, as a projector surface" — a VCam's live
picture projected by a projector light, viewer-local, like a movie projector.

## 0. Verdict

**YES** for existing projector prims (any light with a projector texture) and for Cine Light Rig
projectors. **PARTIAL** for plain point lights (needs a render-only "local projector" override, VP7) and for
drag-and-drop onto *invisible* projector prims (the DnD pick ignores transparent faces; covered by menus/lists).

Why it is cheap: the projector path already samples ONE 2D cookie through one sampler
(`projectionMap`) with one C++ bind site per program family. Swapping that bind for a VCam-derived texture
needs **no change to the lighting shaders**, adds no scene render, and shadows keep coming from the light's
existing spot shadow map. The only new GPU work is a small "feed prep" pass + mip generation per camera update.

## 1. How projectors work here (code facts)

| Fact | Where |
|---|---|
| A light is a projector iff its LightImage param has a non-null texture UUID | `llprimitive.h:345` (`isLightSpotlight()`), `llvovolume.cpp:3523-3531` |
| Cookie lookup: UUID -> fetched `LLViewerTexture`, cached in `mLightTexture` | `llvovolume.cpp:3534-3551`, `3443-3451` |
| Spot params = (FOV rad, focus, ambiance) | `llvovolume.cpp:3455-3464` |
| Shadow-slot priority (screen area; rig emitters r^3 tier; StableSpotShadows) | `llvovolume.cpp:3471-3520` |
| Lighting loop routes projectors to spot programs (rig emitters bypass the projector toggle) | `pipeline.cpp:26336-26347`, `26368-26377`; toggle `pipeline.cpp:10135-10145` |
| `setupSpotLight`: frustum/proj_mat/near/p/n/origin/range/ambiance uniforms | `pipeline.cpp:27357-27425` |
| Shadow-slot auction (NSpot, rig tiers, no-shadow opt-out) | `pipeline.cpp:27426-27513`; `MAX_SPOT_SHADOWS=10` `pipeline.h:1442`, default 2 `pipeline.cpp:763-768` |
| **The cookie bind** (white fallback, aniso, `proj_lod = log2(width)`, focus/ambient LOD) | `pipeline.cpp:27515-27548` |
| Procedural gobo override per light UUID (pattern/anim/tint/dispersion), session-only | `pipeline.cpp:27555-27569`, `pipeline.h:2449-2464`, cleared `pipeline.cpp:21943-21953` |
| Volumetric twin (froxel inject + projector shafts) duplicates the bind | `setupSpotLightVolumetric` `pipeline.cpp:27586`, bind `27705-27726`; callers `20665`, `21517` |
| Shader: `projectionMap`, `proj_lod`, `proj_focus`, `proj_ambiance`, `gobo_*` | `deferredUtil.glsl:53-83` |
| `sampleGobo`: procedural if `gobo_pattern>=0` else `textureLod(projectionMap, uv, goboLod())` | `deferredUtil.glsl:314-333`, `goboLod` `94-108` |
| Sample is **sRGB-decoded and multiplied by alpha**; edge fade on tc in [0,1] | `deferredUtil.glsl:512-536`, `558-575` |
| Ambiance uses `lod = proj_lod` (the smallest mip = average colour) | `deferredUtil.glsl:541-552` |
| Probe dirty signature + probe light state include texture id + gobo | `pipeline.cpp:10336-10358`, `llreflectionmapmanager.cpp:1289-1302` |
| Cine rig projectors are viewer-created FORCE_INVISIBLE `LLVOVolume`s; cookie reset each tick | `alcinelightrig.cpp:2083-2125`, ids `1105-1113`; 5 rigs x 4 lights `alcinelightrigmanager.h:1263-1274`, `alcinelightrigmodel.h:20` |
| Only the deferred spot/multispot + volumetric shaders read the cookie (alpha-blended forward lighting does not) | `spotLightF.glsl`, `deferredUtil.glsl`, `projectorVolumetricF.glsl` |

## 2. The VCam side (code facts)

- Retained output: per capture slot, lifetime-fixed 1024x1024 `GL_RGBA16F`, **no mips**
  (`pipeline.cpp:1886-1915`). It is a copy of the aux `rt.screen` right after `renderDeferredLighting` +
  projector shafts, i.e. **pre-tonemap, pre-exposure linear HDR** (`llprismlens.cpp:8600-8613`).
- Logical region inside the pack: `scale = (w-1)/1024`, `offset = 0.5/1024`; plus a frozen orientation
  transform (`llprismlens.cpp:8691-8719`).
- **Alpha is the pre-tonemap glow channel, not opacity** (`prismSignalF.glsl:85`). Face composites draw this
  HDR into the main screen before tonemap (`pipeline.cpp:26795-26813`, bind `27032`).
- Publication: `markProduced` after the aux render (`llprismlens.cpp:8634-8636`, `3708-3729`); a GL-scoped
  post-step precedent exists (`ScopedPrismRenderState` + `updateAtlas`, `8638-8643`).
- Demand: a capture renders full quality only if a display is visible (`3196-3214`) or the Gate watch asks
  (`3282-3288`); otherwise preview-only or nothing (`3289-3294`). Preview-quality output is skipped by the
  composite (`8829`).
- Release: `releasePrismLensOutputs()` + `LLPrismLens::onRenderTargetsReleased()` (`pipeline.cpp:2547-2548`).
- Bindings: `DisplayDefinition` with quarantine (`llprismlens.h:496-507`); `mVirtual` precedent for a
  faceless kind (`llprismlens.h:459-480`); scene LLSD `sceneData/applySceneData` (`llprismlens.h:716-717`),
  offline -> quarantined on apply (`llprismlens.cpp:6133-6169`). No permission check (none in `llprismlens.cpp`).
- DnD: `DAD_VCAM_FEED` -> `dad3dVCamFeed` (`lltooldraganddrop.cpp:238`, `2312-2353`) ->
  `ALVCamPageModel::faceDropStatus/dropOnFace` (`alvcampagemodel.cpp:418-493`). Pick is
  `pickImmediate/pickAsync(x, y, ..., pick_transparent=false, ...)` (`lltooldraganddrop.cpp:734`, `740`).
- UI: tile menu "Send to surface..." (`menu_vcam_monitor.xml:6-13`), Output section card "Faces this camera
  feeds" (`panel_dpage_camera_vcam.xml:8`, `314`). Lightbox already has a per-projector cookie section
  (`alfloaterlightbox.cpp:454-612`); object menu has projector items (`alviewermenu.cpp:635-642`).
- `FOLLOW_PROJECTOR` FOV mode already exists for a camera mounted on a projector (`llprismlens.cpp:3258-3273`).

## 3. Answers to the feasibility questions

**Q2a — bind a render target instead of an `LLViewerTexture`?** Yes. `LLRenderTarget::bindTexture(index,
channel, filter)` binds the GL name on any unit (`llrendertarget.cpp:615-618`); the prism composite already does
this for the retained output. `lod_range` must come from the bound texture's width instead of `img->getWidth()`.

**Q2b — mips.** Required. Focus blur selects `lod = diff * proj_lod`, ambiance samples the last mip, aniso raises
LOD (`deferredUtil.glsl:94-108`, `541-575`). Binding the mip-less retained RT with a trilinear filter would sample
an incomplete texture (black) or alias. Solution: a per-binding **prep target allocated with `TMG_AUTO`**;
`flush()` runs `glGenerateMipmap` (`llrendertarget.cpp:628-633`; precedent `mLuminanceMap`, `pipeline.cpp:2831`).

**Q2c — format / exposure.** Do **not** project raw HDR: (1) the shader sRGB-decodes the sample, (2) multiplies by
alpha (= glow, often 0), (3) HDR magnitude depends on the *filmed* scene's light, while a real projector's
brightness is lamp x film transmittance (0..1). The prep pass therefore produces the **display-mapped signal**
the monitor shows: `linear * mainExposure(exposureMap) * 2^(capture ExposureBias + feed EV)` -> tonemap
(Khronos PBR Neutral, `tonemapUtilF.glsl:122`; or the viewer's current `applyToneMap` if its uniforms are bound,
`tonemapUtilF.glsl:421-430`) -> clamp -> `linear_to_srgb` -> **RGBA8, alpha = 1**. The existing shader decode then
yields display-linear 0..1, multiplied by light colour x intensity. Net: zero lighting-shader edits.
(Alternative: RGBA16F + a `proj_feed` flag to skip the decode; only needed for an "HDR passthrough" mode later.)

**Q2d — UV region / aspect / orientation.** Handled in the prep pass, not the lighting shader: sample
`retained_uv = (logical_uv * orientScale + orientOffset) * regionScale + regionOffset` (same math as
`prismSignalF.glsl:62-91`), with FIT/FILL/STRETCH + anchor against the projector aspect
(`scale.x/scale.y`, `pipeline.cpp:27392-27397`). FIT bars are black = no light. Flip H/V + optional TV effects
come free by reusing `prismSignalSample()` in the prep shader.

**Q3 — lifetime/sync.** Prep runs right after `markProduced` for that slot, in the same scoped GL state as
`updateAtlas`; the main view's lighting runs later in the frame -> 0 extra frames for the main view, 1 update for a
camera filming its own projection (video feedback; allowed, no read/write hazard because the aux writes its
scratch + retained RT, never the prep target). Between updates the prep target simply holds the last frame
(HELD). On `onRenderTargetsReleased` (context loss, ALM/res change) prep targets are released with the outputs;
the binding becomes "no signal" until the next produced frame, and the light falls back per policy
(own cookie [default] / black / white).

**Q4 — override mechanism.** Registry-owned bindings + an O(1) lookup consulted only at the cookie bind:
`const LLPrismLens::ProjectorFeed* LLPrismLens::projectorFeedFor(const LLVOVolume*)` (<= 8 entries, UUID compare;
rig entries resolve `(rig slot, light) -> ALCineLightRig::projectorId()` once per frame). No prim/TE/LightImage
change, no simulator message, no permission check, offline objects quarantined like displays, saved in Director
scenes inside the Prism payload.

**Q5 — non-projector lights.** `isLightSpotlight()` is network truth used by build UI (`llpanelvolume.cpp:1012`,
`1234`, `1628`) — must not be faked. A plain point light can become a *local* projector only through a new
render-only predicate (`isProjectorForRender()`: spotlight OR local-projector override with local FOV/focus/
ambiance) at the ~10 render sites (`pipeline.cpp:10299`, `10469`, `10553`, `11165`, `26336`, `26368`, `26589`,
`llvovolume.cpp:3471`, `llreflectionmapmanager.cpp:1289`, `1807`) plus `getSpotLightParams` fallback (FOV 0 would
divide by zero at `pipeline.cpp:27378`). Optional VP7. A cheaper alternative is **"Add virtual projector here"**:
a viewer-created emitter exactly like rig projectors (`alcinelightrig.cpp:1607` `createObjectViewer`).

**Q6 — shadows/perf.** Shadows unchanged: the light's spot shadow map (if it wins a slot) occludes the picture.
Per camera update: one <=512^2 (opt. 1024^2) quad + mip chain (~0.03-0.1 ms GPU, est.); per frame: one texture
bind per feed projector (zero shader cost). Memory 1.4 MB per binding at 512^2 RGBA8+mips (5.6 MB at 1024^2).
The real cost is the **capture itself**: a projector-only camera must now render at full quality at its rate
(same cost as one visible monitor).

## 4. Recommended design

### 4.1 Data model (separate table, display paths untouched)
New in `llprismlens.h`, beside displays (do NOT reuse the display table: every face/virtual composite path would
need a third kind gate; a separate table keeps the display paths byte-identical):

```cpp
constexpr U32 MAX_PROJECTOR_BINDINGS = 8;
enum class EProjectorTarget : U8 { OBJECT, RIG_LIGHT, VIRTUAL /*VP7*/ };
enum class ENoSignal : U8 { OWN_COOKIE, BLACK, WHITE };
struct ProjectorHandle { LLUUID mId; U64 mGeneration = 0; };
struct ProjectorSettings {
    EFitMode mFitMode = EFitMode::FIT; F32 mAnchor[2] = {0.5f, 0.5f};
    F32 mFeedEV = 0.f;      // -6..+6, applied while preparing the LDR feed
    F32 mLampPowerEV = 0.f; // -2..+8, viewer-only HDR light multiplier
    bool mFollowMainExposure = true; U16 mResolution = 512; // 256/512/1024
    ENoSignal mNoSignal = ENoSignal::OWN_COOKIE; bool mKeepGobo = false;         // keep tint/flicker
    ScreenEffects mEffects;                                                       // reuse, default zero
};
struct ProjectorDefinition {
    ProjectorHandle mHandle; CaptureHandle mCapture; EProjectorTarget mTarget;
    LLUUID mObjectId; U8 mRigSlot = 0, mRigLight = 0; ProjectorSettings mSettings;
    bool mGateSubscribed = false; bool mQuarantined = false; std::string mQuarantineReason;
    EDisplayHealth mHealth; std::string mReason;  // READY / OFFLINE / INVALID ("not a projector")
};
```
APIs mirror displays: `projectorTargetStatus(capture, object)`, `bindProjector`, `reassignProjector`,
`setProjectorSettings`, `removeProjector`, `setProjectorGateSubscribed`, snapshot/revision signals,
`projectorFeedFor(volume)`. Removing a capture removes its projector bindings (same as displays).
Validation: live `LLVOVolume`, `getIsLight()`, `isLightSpotlight()` (or rig light), not HUD; linkset drop resolves
the hit prim if it is a projector, else the single projector in the linkset, else rejects with
"N projectors in this linkset; use Send to projector". One binding per light (rebinding = reassign).

### 4.2 Demand
`setupSpotLight` stamps `lastConsumedFrame` on the binding when called for the main view
(`!sPrismLensRender && !gCubeSnapshot`). The capture prepare step (`llprismlens.cpp:3282-3294`) treats
"any projector binding consumed within the last 2 frames, or bound within the last 30 frames (bootstrap)" like the
Gate watch: `mAnyDisplayVisible = true`, `required = mResolution x aspect`. Budget/rate controller unchanged.

### 4.3 Feed prep pass
`LLPipeline::mPrismProjectorFeed[MAX_PROJECTOR_BINDINGS]` (`LLRenderTarget`, `GL_RGBA8`, `TMG_AUTO`), lazily
allocated at the binding's resolution, released in `releasePrismLensOutputs` path. New program
`gPrismProjectorFeedProgram` = `prismLensV`-style fullscreen + `prismSignalF.glsl` (reused `prismSignalSample`)
+ new `prismProjectorFeedF.glsl` (exposure, tonemap, `linear_to_srgb`, alpha 1). After `markProduced(slot)`
(`llprismlens.cpp:8634`), inside a `ScopedPrismRenderState`, render every non-quarantined binding owned by that
slot, `flush()` (mips), set `mValid=true`, `mProducedFrame`. Main exposure from `exposureMap` (previous frame).

### 4.4 Pipeline binding
Extract one helper used by both `setupSpotLight` (`27515-27548`) and `setupSpotLightVolumetric` (`27705-27726`):
`bindProjectorCookie(shader, volume, focus, proj_range, bool surface)`. If `projectorFeedFor(volume)` is valid:
`feed.bindTexture(0, channel, aniso ? TFO_ANISOTROPIC : TFO_TRILINEAR)`, `lod_range = log2(feed width)`, and
upload `gobo_pattern = -1` (feed wins over procedural gobos; keep tint/flicker/dispersion only if `mKeepGobo`).
Else: existing code verbatim (byte-identical when no binding). Probe signature (`pipeline.cpp:10339`) and probe
light state (`llreflectionmapmanager.cpp:1291`) add the binding identity (capture id+generation), not content,
so probes re-dirty on bind/unbind but do not churn per video frame. Optional setting
`VCamProjectorShadowPriority` lifts feed projectors one tier in the shadow auction (`pipeline.cpp:27490-27501`).

### 4.5 UX
- **Drag** a monitor tile onto a projector prim: `dad3dVCamFeed` asks `ALVCamPageModel::projectorDropStatus`
  first when the hit object/linkset carries a projector light; tooltip "Project CAM 2 through 'Spot 3'".
  Holding Shift forces "bind face instead". Non-projector light -> "This light has no projector texture. Use a
  Light Rig light or Add virtual projector."
- **Tile menu** `menu_vcam_monitor.xml`: new "Send to projector..." submenu: *Selected projector*, *Nearby
  projectors* (up to 12 lit projectors in draw distance, nearest first, name + distance), *Light Rig* (slots with
  live projectors: "Rig A Key"...), *Cancel*.
- **Output section**: new card "Projectors this camera feeds" after `faces` (rows: name, health, Fit, Feed EV,
  Lamp power EV, Resolution, No-signal, Remove). Projector rows accept tile drops (reassign) like face rows.
- **Lightbox** projector section: "Camera feed" combo (None / CAM 1..8) beside the gobo controls.
- **Light Rig** card per light: "Project camera" combo (RIG_LIGHT target, keyed by slot+light, survives emitter
  re-creation).
- Undo via `ALKitHistory` typed entries (bind/unbind/reassign/settings), as VC design 5.4.

### 4.6 Persistence
Prism scene payload gains `"projectors": [ {id, gen, capture, target:"object"|"rig", object_id | rig_slot+rig_light,
settings{...}, gate} ]`. Absent key = none (old scenes load unchanged). Offline object -> quarantined record,
revalidated when the object rezzes (same mechanism as displays). Old builds ignore the unknown array (verify in
VP1 that the old parser does not reject unknown top-level keys; if it does, bump the Prism scene schema).

## 5. Work packages

| VP | Scope | Files | Acceptance |
|---|---|---|---|
| VP1 Registry + schema | ProjectorDefinition table, validate/bind/reassign/remove/settings, revisions, quarantine, LLSD, undo entries | `llprismlens.h/.cpp`, `alvcampagemodel.h/.cpp`, `alkithistory*` | New `llprismprojector_test.cpp`: bind/reassign/remove; capacity 8; stale handle; remove capture drops bindings; LLSD round-trip; offline -> quarantined -> rez -> READY; old scene w/o key loads identical; non-projector rejected with reason |
| VP2 Demand | consumption stamp + capture demand + bootstrap; health readouts | `llprismlens.cpp` (3282-3294 area), `pipeline.cpp` | Projector-only camera renders full quality at its rate; unbinding stops renders within 2 frames; no change when no bindings (Tracy A/B) |
| VP3 Feed prep | targets, program, shader, run after `markProduced`, release on target loss | `pipeline.h/.cpp`, `llprismlens.cpp`, `llviewershadermgr.cpp`, `prismProjectorFeedF.glsl` | Readback: prep mip0 matches tonemapped feed within 2/255; last mip = average; alpha 255; FIT bars 0; release+realloc after ALM toggle/resize without GL errors |
| VP4 Pipeline bind | `bindProjectorCookie` helper (surface + volumetric + froxel), gobo precedence, probe signature, optional shadow tier | `pipeline.cpp/.h`, `llreflectionmapmanager.cpp` | No-binding frames byte-identical (shader-uniform + image diff); bound projector shows the camera image on a wall, focus blur by distance, volumetric shaft coloured by feed |
| VP5 UX | DnD resolution, Send to projector menu, Output "Projectors" card, Lightbox combo | `lltooldraganddrop.cpp`, `alvcammonitorstrip.cpp`, `menu_vcam_monitor.xml`, `panel_dpage_camera_vcam.xml`, VCam page code, `alfloaterlightbox.cpp` + XUI | `xui_layout_check.py` zero findings; `ui_selftest.ps1` clicks every new control and asserts registry effect; drop on projector/linkset/non-projector gives the right accept/reason |
| VP6 Rig lights | RIG_LIGHT target resolution + Light Rig card combo + scene save | `alcinelightrig*.cpp/.h`, `alcinelightrigmanager.*`, rig XUI | Rig Key projects CAM 1; survives rig rebuild/relog-in-session and Director scene reload |
| VP7 (optional) Local/virtual projector | "Add virtual projector here" emitter and/or render-only local-projector override for point lights | `alcinelightrig.cpp` pattern, `pipeline.cpp` render sites, `llvovolume.*` | Point light becomes a projector locally; build floater still shows it as a point light; no object update sent |
| VP8 Gates | functional self-test + visual match + perf | `scripts/perf/ui_selftest.ps1`, test scene | Test-pattern camera projected: correct orientation viewed from behind the projector; colours/brightness match the monitor face; Tracy: prep <= 0.1 ms/update at 512^2; FUNCTIONAL + VISUAL MATCH gates per repo rules |

Ordering: VP1 -> VP3 -> VP4 (first visible result) -> VP2 -> VP5 -> VP6 -> VP8; VP7 only on request.
One build at the end, per repo process.

## 6. Cost estimate (rough)

| VP | C++ / GLSL / XUI (LOC) |
|---|---|
| VP1 | ~550 + ~250 test |
| VP2 | ~120 |
| VP3 | ~280 C++ + ~120 GLSL |
| VP4 | ~180 |
| VP5 | ~450 C++ + ~200 XUI |
| VP6 | ~220 |
| VP7 | ~300 (optional) |
| VP8 | ~200 script/test |
| **Total** | **~2.3k core (VP1-VP6, VP8), ~2.6k with VP7**; 2-3 Codex rounds + Opus review |

## 7. Risks

1. **Alpha = glow channel** in the retained output; projecting it raw would multiply the picture by ~0. Prep
   must write alpha 1 (`prismSignalF.glsl:85`, `deferredUtil.glsl:574`).
2. **Double sRGB / HDR range**: raw HDR into an sRGB-decoding sampler blows out or darkens; prep encodes
   display-referred sRGB8.
3. **Missing mips** -> black/aliasing with `textureLod`; prep target must be `TMG_AUTO` and only marked valid
   after its first `flush()`.
4. **Starved capture**: without VP2 a camera with no visible monitor never renders, so the projection freezes.
5. **Picking**: invisible projector prims and FORCE_INVISIBLE rig emitters cannot be DnD targets
   (`lltooldraganddrop.cpp:734/740`); the menu/list paths are mandatory, not optional.
6. **Shadow slots** (default 2): a feed projector without a slot paints through walls; offer the priority tier and
   show "no shadow slot" in the row.
7. **Probes**: content is not in the probe signature by design (stale-but-stable probe lighting).
8. **GL state**: the prep pass runs between the aux render and the main view; it must use the existing scoped
   restore (`ScopedPrismRenderState`) or it can leak FBO/viewport state into the main frame.
9. **Rig emitter ids change** on rebuild; rig bindings must be keyed by (slot, light), never by object UUID.
10. **Scene compatibility**: confirm older builds tolerate the new `projectors` key.
11. **Feedback loops** (camera films its own projection) are allowed and lag one update; can look like
    video feedback.

## 8. Not possible / out of scope

- Other residents never see it (viewer-local; the prim keeps its own cookie for everyone else).
- Omni/point lights cannot project a picture (would need a cube cookie); only spot projectors (or VP7).
- Alpha-blended surfaces (hair, glass, particles) do not receive projected cookies in this renderer.
- More shadowed projectors than shadow slots; true lens bokeh (focus blur is mip blur); physically exact HDR
  projection (it is display-mapped by design); zero-latency self-recursion.
- Projectors render only in the deferred renderer with the projector toggle on (`pipeline.cpp:10135-10145`);
  volumetric/froxel colouring additionally needs shadows on (`pipeline.cpp:20463`).
