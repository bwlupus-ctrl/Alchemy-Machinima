# Cinematic Light Rig — Deep Design

**Status:** design pass complete. **No source modified.**
**Date:** 2026-08-15.
**Feeds:** a Codex `--prompt-file` implementation brief per CLAUDE.md.
**Inputs:** `doc/CINE_LIGHT_RIG_DESIGN_SEED.md`,
`doc/reference/cinematic_studio_render_engine_v20.lsl`,
`doc/reference/cinematic_studio_fx_pack1.lsl`, and the code reads cited below.

Claim labels used throughout, per CLAUDE.md rule 3:
**PROVES** = I read the code at the cited line. **IMPLIES** = documentation or a
comment says so, or an unbroken but unread call chain. **INFERENCE** = my
reasoning; could be wrong. Where I am guessing I use the word **guess**.

---

## 0. Executive summary

Port the LSL "Cinematic Studio" rig as a client-side feature built from four
pieces, shipped together (ship-whole):

1. **`alcinelightrigmodel.{h,cpp}`** — a pure, renderer-independent namespace
   (the `ALWeatherModel` shape) holding all rig math: base setup × transforms →
   live state (`computeLive`), eased transitions, exposure, geometry, and a
   **pure-function FX evaluator** for all 33 effects:
   `evalFX(fx, seed, presentation_time)` — stateless, so seek/scrub and
   non-realtime Temporal Capture re-render bit-identically. Unit-tested under
   TUT in **`indra/newview/tests/alcinelightrigmodel_test.cpp`**.
2. **`alcinelightrig.{h,cpp}`** — class `ALCineLightRig`, the viewer
   controller. Owns up to 8 invisible client-only `LLVOVolume` emitters
   (4 projectors + 4 optional bounce omnis) created with the proven
   `alghostmanipproxy` recipe (`mIsLocalOnly` + a new
   `LOCAL_OBJECT_CINE_RIG_EMITTER` kind). Ticks once per frame from
   `LLAppViewer::idle()` beside `ALLocalFogManager::tick()` on
   `LLPresentationTime::currentFrame().presentation_time`. Anchors to own
   avatar or any Director cast member via `LLDirectorCast::resolve()`.
   Provides `sceneData()/applySceneData()` for Director scene round-trip
   (the `LLPrismLens` precedent).
3. **`alpanelcinelightrig.{h,cpp}` + `panel_cine_light_rig.xml`** — ONE
   `LLPanelInjector`-registered panel class, settings-backed (`control_name`),
   instantiated in BOTH a new Director Console **Lights** tab (beside Shafts)
   and a new standalone floater **`floater_cine_light_rig.xml`**. Lockstep is
   free: every control binds the same `gSavedSettings` key (the
   `ALPanelCineCamParams` mechanism, proven in its header comment).
4. **Settings** — ~36 new `CineLightRig*` keys in `settings.xml` (listed in
   §3.4). Per-light base values ARE settings, so scene save, presets,
   reset-to-default, and two-host lockstep all ride existing machinery.

Key re-imaginings versus the LSL (full table in §2): the link-message
plumbing, 16 fps timer, delta suppression, omni reposition cadence, hover
text, hand-rolled `pow 2.2` gamma, and the "height" parameter (a workaround
for a ground-rezzed rig) are all dropped; `llFrand` becomes a counter-based
hash so every FX is a pure function of presentation time; the gamma toggle
becomes `setLightSRGBColor()` (correct sRGB→linear); exposure keeps
`2^EV` but is re-based by a **2-stop headroom** because — overturning the
seed brief — the client clamps light intensity to [0,1] and radius to 20 m
exactly like the sim (`llprimitive.h:161-164`, PROVES).

The two most consequential render-system facts found (§6.5):
the fork has **up to 6 spot-shadow slots** (2 by default,
`BDMergeMaxSpotShadows`, `pipeline.h:1030`), and **projector volumetrics /
froxel injection only run for projectors currently holding a shadow slot**
(`pipeline.cpp:13788-13800`, PROVES) — so the rig's shadow policy (§6.5.2)
is what decides whether rig lights can have hero beams. The `bdmerge` world-
light gate WOULD hide the rig when the user hides world lights; the design
exempts rig emitters at the six non-attachment gate call sites (§6.5.3) —
the only pipeline.cpp edits in the whole feature, each provably inert when
the feature is off because no rig-kind object can exist then.

Declared deferrals (§8, decided now, not discovered later): per-light gobo
patterns, N>4 lights, new hotkeys, renderer-side >1.0 intensity headroom,
and gizmo drag-editing. Everything else — engine, model, tests, settings,
both UI hosts, scene round-trip, gizmo display, registration — ships in one
delivery.

---

## 1. Verification: what the code PROVES, what the seed got wrong

### 1.1 Confirmed seed claims

| # | Claim | Evidence | Label |
|---|---|---|---|
| V1 | Client-only object recipe (create, `mIsLocalOnly`, typed kind, `gPipeline.createObject`, `setLOD`, box `setVolume`, `setScale`, `FORCE_INVISIBLE`) | `alghostmanipproxy.cpp:187-245` read in full; position/rotation/scale pushed per tick via `setScale(v,false)` / `setRotation(q,false)` / `setPositionGlobal(p,false)` at `alghostmanipproxy.cpp:370-373` | PROVES |
| V2 | No sim traffic from local-only param changes | `llviewerobject.cpp:6831-6834`: `parameterChanged` sends `ObjectExtraParams` only `if (local_origin && !isLocalOnly())` | PROVES |
| V3 | `setIsLight(true)` → `setParameterEntryInUse(PARAMS_LIGHT,…)` → `gPipeline.setLight(mDrawable,true)` populates `mLights` and sets `LLDrawable::LIGHT` | `llvovolume.cpp:3181-3206`, `pipeline.cpp:10119-10131` | PROVES |
| V4 | `calcNearbyLights` requires only `LLDrawable::LIGHT` + gates + distance; **no visibility requirement**, so `FORCE_INVISIBLE` is compatible | `pipeline.cpp:9081-9337` read in full; insertion test at 9281-9327 | PROVES |
| V5 | Full per-light API on `LLVOVolume` incl. `setLightSRGBColor` / `setLightLinearColor` / `setSpotLightParams` | `llvovolume.h:264-305`, setters `llvovolume.cpp:3208-3278` | PROVES |
| V6 | `RenderLocalLightCount` default 256; deferred loop caps at that count of **closest** lights | `pipeline.cpp:16572`, cap applied 16611-16618 | PROVES |
| V7 | Lockstep shared-panel precedent; mechanism is settings-backed controls (“Every control is settings-backed (control_name), so multiple live instances stay in sync through gSavedSettings”) | `alpanelcinecamparams.h:11-16` (header comment), `ALPanelWeatherSettings` + `LLPanelInjector` at `alpanelweathersettings.cpp:28` | PROVES |
| V8 | Pure-model + TUT precedent | `alweathermodel.h` read in full; `tests/alweathermodel_test.cpp` asserts sanitize bounds, two-controller determinism, NaN rejection, ≤1 strike after a hitch | PROVES |
| V9 | Director tab container `director_tabs` with tabs Move/Path/Ghosts/Props/Animate/Camera/Takes/Shafts/Weather/Volumes/Temporal; panels included by `filename=` | `floater_director.xml:317,329,513,559,585,613,857,1376,1400,1512,1553,1607` | PROVES |
| V10 | Cast/subject resolution: `LLDirectorCast::resolve(id)` — null id = own avatar, stale id = `nullptr` (never silently self) | `lldirectorcast.h:80-103` | PROVES |
| V11 | Prism auxiliary renders build an independent light list from `mLights`; spotlights are exempt from near-frustum culling there | `pipeline.cpp:9091-9168` | PROVES |
| V12 | `calcNearbyLights` early-outs on `sReflectionRender || gCubeSnapshot || sRenderingHUDs || LLApp::isExiting()` | `pipeline.cpp:9086-9089` | PROVES |
| V13 | Local objects fully cleaned from light machinery on drawable unlink: `mLights`, `mNearbyLights`, `mShadowSpotLight[]`, `mTargetShadowSpotLight[]` | `LLPipeline::unlinkDrawable`, `pipeline.cpp:3012-3066` | PROVES |
| V14 | `markDead()` → object-list cleanup → `unlinkDrawable` | standard viewer chain; ghost proxy relies on it (`alghostmanipproxy.cpp:247-256`) | IMPLIES |

### 1.2 Seed claims OVERTURNED or corrected

**O1 — "the ceiling is gone" is FALSE at the parameter level.** The seed
hoped the sim's clamps do not apply client-side. They do, in the shared
`llprimitive` library, on the very param block `LLVOVolume` writes:

```cpp
// llprimitive.h:161-164
void setLinearColor(const LLColor4& color)  { mColor = color; mColor.clamp(); }   // [0,1] incl. alpha=intensity
void setRadius(F32 radius)   { mRadius = llclamp(radius, LIGHT_MIN_RADIUS, LIGHT_MAX_RADIUS); }  // 0..20 m
void setFalloff(F32 falloff) { mFalloff = llclamp(falloff, LIGHT_MIN_FALLOFF, LIGHT_MAX_FALLOFF); } // 0..2
```
with `LIGHT_MAX_RADIUS = 20.0f` at `llprimitive.cpp:76` (PROVES). The shader
receives `getLightLinearColor()` = color × intensity ≤ 1 per channel
(`llvovolume.cpp:3311-3322`, PROVES), times the **global**
`AlchemyGlobalLightScale` (`pipeline.cpp:16328-16329`, PROVES). So per-light
intensity above 1.0 is impossible without renderer changes. Consequence for
the exposure model in §6.2.

**O2 — "only 2 spot-shadow slots" is stale for this fork.**
`MAX_SPOT_SHADOWS = 6` (`pipeline.h:1030`, PROVES); the runtime count is
`BDMergeMaxSpotShadows`, default 2, clamped 2..6
(`pipeline.cpp:597-601`, PROVES). Additionally the fork already has a
per-projector **shadow opt-out** (`sNoShadowProjectors`,
`pipeline.cpp:15082-15101`) and a **stable priority mode**
(`BDMergeStableSpotShadows`: priority = radius³ instead of screen pixel area,
`llvovolume.cpp:3379-3389`), both PROVES. The rig does not need to invent a
shadow policy mechanism — only defaults (§6.5.2).

**O3 — spotlight aim/beam geometry comes from the emitter's ROTATION and
SCALE, not only from the spot params.** `setupSpotLight` builds the projector
frustum from `getRenderRotation()` and `getScale()`: near-plane distance
`(scale.y*0.5)/tan(fov*0.5)`, aspect `scale.x/scale.y`, far
`radius*1.5 + dist - scale.z` (`pipeline.cpp:17446-17497`, PROVES). The LSL
rig relied on the same convention (`llRotBetween(<0,0,-1>, -aim)`), but the
design must fix an explicit emitter scale (§3.2) because it participates in
the optics. Also, "is a spotlight" is exactly "has a light texture"
(`llprimitive.h:345`, PROVES) — the projector emitters must always carry a
cookie UUID, and the omnis must have none.

**O4 — projector volumetrics / froxel light injection are coupled to the
shadow auction.** The froxel inject loop iterates `mShadowSpotLight[i]` over
`bdmergeMaxSpotShadows()` slots (`pipeline.cpp:13788-13800`, PROVES); the
comment says the per-cone path uses "the same slot iteration"
(`pipeline.cpp:13785-13787`, IMPLIES for the per-cone loop). The backlog
records this as a known pre-existing bug
(`doc/MACHINIMA_FEATURE_BACKLOG.md:43`). Consequence: a rig light can only
have a volumetric shaft or hero beam while it holds a shadow slot (§6.5.1).

**O5 — spot params are unclamped client-side.** `LLLightImageParams::setParams`
stores raw (`llprimitive.h:346`, PROVES). The LSL Softbox fov 2.8 rad is
accepted; sanitize must impose its own bound (§6.2).

**O6 — the deferred light path does NOT apply the fade-in.** The
`LIGHT_FADE_TIME` fade multiplies light color only in `setupHWLights`
(forward/HW lights, `pipeline.cpp:9707-9723`, PROVES); the deferred local
light loop uses raw color (`pipeline.cpp:16612-16717`, no fade term, PROVES).
So rig lights pop on instantly in deferred — good for machinima; the rig's
own 0.9 s eased transition provides the artistic ramp.

### 1.3 New facts the seed did not have

| # | Fact | Evidence | Label |
|---|---|---|---|
| N1 | `bdmerge_should_render_light` semantics: master `BDMergeLightToggles` default **false** → returns true always (inert). With master on and `BDMergeRenderWorldLights` off, all **non-attachment** lights are skipped | `pipeline.cpp:9018-9039` | PROVES |
| N2 | The world-light gate is applied at exactly six non-attachment call sites: `pipeline.cpp:9136` (Prism list), `9224`, `9316` (main list), `9693` (HW lights), `16641` (deferred loop), `19951` (Prism aux shadow gen) | grep + reads | PROVES |
| N3 | The fork has a presentation clock: `LLPresentationTime::tick()` at the top of `idle()` (`llappviewer.cpp:5070`); fork systems tick on `currentFrame().presentation_time` at `llappviewer.cpp:5514-5523`; **gobo animation already runs on it** (`GOBO_TIME`, `pipeline.cpp:17615-17616`) | reads | PROVES |
| N4 | Session-only per-projector UUID sets exist for shaft opt-in, hero beams, shadow opt-out, gobo overrides — all cleared on disconnect via `clearVolumetricShafts()` | `pipeline.cpp:15051-15120` | PROVES |
| N5 | Reflection probes: local lights render into probe faces only when `RenderReflectionProbeLevel > 0`; probe passes reuse the main `mNearbyLights` (calc early-outs during `gCubeSnapshot`); `updateSpotLightPriority` no-ops during snapshots | `pipeline.cpp:16575`, `9086`, `llvovolume.cpp:3361-3364` | PROVES |
| N6 | Director scenes: LLSD file with engine data + `sceneSettingsList()` settings block + structured sub-blocks from `LLPrismLens::sceneData()` — the exact template for rig round-trip | `llfloaterdirector.cpp:642-905` | PROVES |
| N7 | `calcNearbyLights` is called once per frame from `renderGeom` (`pipeline.cpp:7151`) and once per Prism aux render (`llprismlens.cpp:5835`) — an idle-time rig tick is consumed the same frame, never a frame stale | reads | PROVES |
| N8 | Wireframe gizmo precedent: Prism draws a client-side frustum gizmo with `gUIProgram` from the `render_ui_3d` overlay path | `llprismlens.cpp:5165-5280` | PROVES |
| N9 | Presentation-time exponential damping precedent (frame-rate-independent ease) | `WeatherShelterExposure`, `pipeline.cpp:183-228` | PROVES |
| N10 | An in-code latent LSL bug: FX beam index `m=3` (Elevator Fault, Stage Debut) reads past the 3-entry BEAMS list; LSL out-of-range list reads return 0.0 → fov 0.0 pinhole projector. Must NOT be copied (§7 notes) | `cinematic_studio_fx_pack1.lsl:245-246,376-378` vs `BEAMS` at `:63` | PROVES (LSL read) |

---

## 2. Re-imagining: which LSL decisions were workarounds, and what replaces them

This is the per-decision account the task requires. "Keep" means the idea
survives; "drop" means the mechanism dies.

| LSL decision | Why the LSL did it | Verdict | Replacement |
|---|---|---|---|
| Derived-state architecture (`gBase` × transforms → `computeLive()`, never accumulate) | Correctness (the whole point of v20) | **KEEP — this is the model's spine** | `ALCineLightRigModel::computeLive(setup, transforms)`; the invariants become TUT tests (§4.3) |
| Two scripts (engine + FX coprocessor) with 8000/8001/8011 link messages, state mirroring, `SYS\|SYNC` | LSL script memory + event queues | **DROP** | One controller, one model. FX evaluation is a function call, not a message. Zero sync protocol |
| 16 fps timer (`TICK 0.06`), `gInterval` design cadence, wall-clock `fs = elapsed/gInterval` for smooth FX | Sim timer jitter, script scheduling | **DROP the timer, KEEP the cadence constant** | Per-frame tick on presentation time; each FX keeps its `interval` so all original speed constants carry over: `fs = (t - t0)/interval` |
| `gFXStep++` per tick for discrete/beat effects | No reliable clock for beats | **DROP** | Virtual step index `step = floor((t - t0)/interval)` — pure function of time, seekable (§5) |
| `llFrand` per tick | Only RNG available | **DROP** | Counter-based hash (SplitMix64-style) of `(seed, fx, step, light, draw)` — stateless, scrub-stable (§5.1) |
| Delta suppression (`gOY/gOP/...` caches, skip unchanged prims) | `llSetLinkPrimitiveParamsFast` cost, moving-prim budget | **DROP as a system** | The C++ setters already early-out on equality (`llvovolume.cpp:3218,3233,3246,...`, PROVES). The controller still compares the assembled frame to the last one to skip whole-frame writes — one memcmp, not a protocol |
| Omnis repositioned only ~1×/s (`gFrameN & 15`) | Moving-prim budget | **DROP** | Omnis reposition every frame like projectors |
| Omni bounce layer at 0.45×, pitch −45° | SL has no GI for local lights; fake bounce | **KEEP as an opt-in layer, default ON** (§6.1) | `CineLightRigBounceEnabled` (default TRUE = LSL-parity look) + `CineLightRigBounceRatio` (default 0.45). The viewer's probes do ambient, not per-light dynamic bounce; the omni layer still earns its place. INFERENCE on the aesthetic; the toggle makes it cheap to be wrong |
| Hand-rolled gamma: `pow(col, 2.2)` + GAMMA toggle | LSL has no color-space API | **DROP** | Profiles are artistic sRGB values → feed through `setLightSRGBColor()` (exact sRGB EOTF via `linearColor3`, `llvovolume.cpp:3208-3211`, PROVES). The toggle dies; there is one correct answer now |
| `intensity = 2^(EV + masterEV + EV_dist)`, `EV_dist = log2(radius/1.5)` | Real cinematographic idea (constant subject exposure under radius change) crushed by the sim's [0,1] clamp | **KEEP the model, RE-BASE the mapping** (§6.2) | `intensity = 2^(EV_total − CineLightRigHeadroomStops)`, headroom default 2.0 — FX at ev +2 actually flash instead of clipping. The client has the same [0,1] clamp (O1), so headroom must come from re-basing, not from the renderer |
| Light radius written as `orbitRadius × 2.2`, omni `× 1.5` | Tuned against the sim's 20 m cap and falloff | **KEEP formula, KEEP the clamp awareness** | Client clamps at 20 m too (O1). Sanitize notes the clamp; UI hints when orbit radius > ~9 m stops growing light reach (EV_dist keeps compensating exposure) |
| `gHeight` global Z offset + AUTO_HEIGHT (owner pos + 0.4, clamp 0..3) | The rig was a prim object rezzed on the GROUND; height lifted the orbit centre to body height | **DROP the concept** | The rig centre IS the anchor avatar. Default centre = chest joint world position (`avatar->getJoint("mChest")->getWorldPosition()`, INFERENCE: standard joint API, verify name in implementation); `CineLightRigOffsetZ` remains as a manual trim (default 0). AUTO_HEIGHT as a command disappears — it is the default behavior |
| Hover text status (`llSetText`, role colors) | Only display surface available | **DROP** | The floater/tab is the status surface. The role colors (KEY red / FILL green / RIM blue / BG yellow) survive as gizmo wire colors |
| Mesh visibility + ghost-glow fix (`PRIM_GLOW`, `PRIM_FULLBRIGHT` gated on `gIsVisible`) | Prims were visible set dressing; glow leaked at alpha 0 | **DROP entirely** | Emitters are permanently `FORCE_INVISIBLE`, never selectable. The "see your lights" need is met by the optional wireframe gizmo (§6.1.3), which has no render-state footprint beyond the existing UI overlay pass |
| `PROJ_TEX` single hardcoded cookie UUID | No texture UI in a script | **KEEP a default, make it a setting** | `CineLightRigCookieUUID` (defaults to a shipped soft-spot texture). Per-light gobo patterns: deferred (§8) — the fork's gobo override store exists (`pipeline.h:1728`) but its UI addresses selected in-world objects, which rig emitters deliberately are not |
| `LIGHT_TOGGLE` mutates `gBase` | Correct (on/off is part of the setup) | **KEEP semantics** | Per-light `On` is part of the base setup settings |
| Transition: 0.9 s cubic ease, shortest-path yaw, discrete swap at e>0.5, `on = onS \|\| onT` | Good film craft | **KEEP verbatim in the model** | `blendLight()` + `ease()`; duration becomes `CineLightRigTransitionSec` |
| Pitch clamp ±85°, `wrap180`, radius min 0.5 | Numerical safety | **KEEP in sanitize** | `sanitizeSetup` / `sanitizeTransforms` |
| FX beam index `m=3` out-of-range → fov 0.0 (N10) | LSL bug | **DO NOT COPY** | Sanitize clamps beam to [0,2]; Elevator Fault / Stage Debut map `m=3` → Snoot (2), the closest intent (narrow ceiling spots). Flagged as a deliberate visual deviation |
| Setup library as a separate script answering on 8011 (25-value CSV) | Script memory partitioning | **DROP** | Factory setups compiled into the model; user setups as versioned LLSD preset files (§6.6) |

---

## 3. Proposed architecture

### 3.1 Files

New files (fork conventions: `al*` files, `AL*` classes):

| File | Contents |
|---|---|
| `indra/newview/alcinelightrigmodel.h/.cpp` | pure model (§4) |
| `indra/newview/alcinelightrig.h/.cpp` | `ALCineLightRig` controller (§3.3) |
| `indra/newview/alpanelcinelightrig.h/.cpp` | `ALPanelCineLightRig` shared panel |
| `indra/newview/tests/alcinelightrigmodel_test.cpp` | TUT tests (§4.3) |
| `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml` | the shared panel |
| `indra/newview/skins/default/xui/en/floater_cine_light_rig.xml` | thin standalone host (the `floater_cinematic_camera.xml` pattern of embedding the panel by `filename=`) |

Modified files (each a small, named touch):

| File | Change |
|---|---|
| `indra/newview/llviewerobject.h` | add `LOCAL_OBJECT_CINE_RIG_EMITTER` to the enum at `llviewerobject.h:857-863`; add `isCineRigEmitter()` beside `isGhostManipProxy()` |
| `indra/newview/pipeline.cpp` | rig exemption at the six world-light gate sites (N2, §6.5.3). **The only render-path edits in the feature** |
| `indra/newview/llappviewer.cpp` | `ALCineLightRig::instance().tick(LLPresentationTime::currentFrame().presentation_time);` immediately after the `ALLocalFogManager` tick (`llappviewer.cpp:5522-5523`); teardown call in `disconnectViewer` beside `LLPrismLens::clearDesignations()` (`llappviewer.cpp:6059-6062`) |
| `indra/newview/llviewerfloaterreg.cpp` | register `"cine_light_rig"` |
| `indra/newview/skins/default/xui/en/floater_director.xml` | new tab `label="Lights"` in `director_tabs`, inserted between Shafts (`:1400`) and Weather (`:1512`), hosting `filename="panel_cine_light_rig.xml"` |
| `indra/newview/skins/default/xui/en/menu_viewer.xml` | menu entry beside the other machinima floaters (same branch that opens the Director Console — locate at implementation time; INFERENCE that one exists) |
| `indra/newview/llfloaterdirector.cpp` | scene save/load: `scene["light_rig"] = ALCineLightRig::instance().sceneData()` + apply on load (the Prism pattern, `llfloaterdirector.cpp:830-832`); append the `CineLightRig*` keys to `sceneSettingsList()` |
| `indra/newview/app_settings/settings.xml` | new keys (§3.4) |
| `indra/newview/CMakeLists.txt` | new sources + `LL_ADD_PROJECT_UNIT_TESTS` entry for the model test (mirroring how `alweathermodel_test.cpp` is registered — IMPLIES, copy that registration) |

### 3.2 Emitter objects

Per the ghost-proxy recipe (`alghostmanipproxy.cpp:187-245`), with these
deliberate differences:

- `mbCanSelect = **false**` (the proxy wants selection; rig emitters must be
  untouchable by build tools).
- `mLocalObjectKind = LOCAL_OBJECT_CINE_RIG_EMITTER`.
- Scale fixed at **0.25 × 0.25 × 0.25 m** box. Scale participates in projector
  optics (O3): with fov 1.5 the near plane sits `0.125/tan(0.75)` ≈ 0.13 m
  behind the emitter — sane. Constant, never user-visible.
- Projector emitters: `setLightTextureID(CineLightRigCookieUUID)` (non-null =
  spotlight, O5), `setSpotLightParams(<fov, 0, 0>)`, rotation
  `llRotBetween(<0,0,-1>, aim)` equivalent via `LLQuaternion`.
- Omni emitters: light texture null (plain point light), identity rotation.
- Position pushed per tick via `setPositionGlobal(p, false)` (V1) — global
  coordinates make region-local mapping the object system's problem.
- `FORCE_INVISIBLE` asserted at creation and re-asserted per tick (the proxy
  does the same; drawable rebuilds can clear state — IMPLIES from the proxy's
  "Reasserted every tick" comment, `alghostmanipproxy.cpp:237-243`).

### 3.3 `ALCineLightRig` controller

```cpp
class ALCineLightRig
{
public:
    static ALCineLightRig& instance();

    // idle-time tick, presentation clock in (never wall clock)
    void tick(F64 presentation_time);

    // anchor: null = own avatar; any cast member UUID otherwise (session state)
    void setAnchor(const LLUUID& id);
    const LLUUID& getAnchor() const;

    // FX transport
    void startFX(S32 fx_id, F64 presentation_time);   // records t0 + reseeds stream key
    void stopFX();                                    // engine re-asserts truth (computeLive)
    S32  activeFX() const;                            // -1 = none

    // setups library (user LLSD presets + compiled-in factory setups)
    std::vector<std::string> setupNames() const;
    bool loadSetup(const std::string& name);          // replaces BASE, transforms re-apply
    bool saveSetup(const std::string& name);
    bool deleteSetup(const std::string& name);

    // Director scene round-trip (LLPrismLens pattern)
    LLSD sceneData() const;
    void applySceneData(const LLSD& data);

    // teardown: markDead all emitters, clear session flags; idempotent
    void shutdown();

private:
    bool ensureEmitters();       // create/recreate; false when no region / exiting
    void destroyEmitters();
    void readSettings(ALCineLightRigModel::Setup&, ALCineLightRigModel::Globals&,
                      ALCineLightRigModel::Transforms&) const;
    void applyFrame(const ALCineLightRigModel::RigFrame&, const LLVector3d& centre);

    LLPointer<LLVOVolume> mProj[4];
    LLPointer<LLVOVolume> mOmni[4];
    LLUUID mAnchor;                       // session-only; scene-serialized
    S32 mActiveFX = -1;
    F64 mFXStart = 0.0;                   // presentation timestamp
    // transition state: start snapshot + target + t0 (presentation time)
    ...
};
```

Tick order and contract:

1. Early-out if `!CineLightRigEnabled` (one cached-control read — the OFF
   path is a boolean test; §6.8).
2. Early-out and `shutdown()` if `LLApp::isExiting()`.
3. Resolve anchor via `LLDirectorCast::instance().resolve(mAnchor)` (V10).
   Unresolved (left region / muted / not yet loaded): all emitter lights are
   switched off (params stay, `setLightIntensity(0)` — cheap and reversible);
   objects are kept; reacquire next tick.
4. `ensureEmitters()`: recreate any emitter that `isDead()` (region restart,
   teleport killed it) on `gAgent.getRegion()`.
5. Read settings → sanitize → `computeLive` (or `evalFX` when an FX is
   active) → transition blend → `render()` → `RigFrame`.
6. Anchor point: chest joint world position (fallback: avatar render
   position + 1.2 m if the joint lookup fails), plus `CineLightRigOffsetZ`.
   Optional damping: exponential ease of the centre with time constant
   `CineLightRigDamping` seconds, presentation-time based, exact
   integration (the `WeatherShelterExposure` formula, N9). Default 0 = rigid.
7. `applyFrame`: per emitter — position (global), rotation (projectors),
   `setIsLight`, `setLightSRGBColor`, `setLightIntensity`, `setLightRadius`,
   `setLightFalloff`, `setSpotLightParams`. All setters early-out on equal
   values (PROVES), so a static rig costs near zero.
8. Maintain shadow policy + shaft flags (§6.5.2) via the existing session
   sets.

This runs at `llappviewer.cpp:~5523`, i.e. after `gObjectList.update`
(avatar joints current for this frame) and before render;
`calcNearbyLights` consumes the result later the same frame at
`pipeline.cpp:7151` (N7). **Never a frame stale.**

### 3.4 Settings keys (all in `settings.xml`, prefix `CineLightRig`)

Global (12): `CineLightRigEnabled` (BOOL, F), `CineLightRigPower` (BOOL, T),
`CineLightRigRadius` (F32, 1.5), `CineLightRigMasterEV` (F32, 0),
`CineLightRigOffsetZ` (F32, 0), `CineLightRigHeadroomStops` (F32, 2.0),
`CineLightRigBounceEnabled` (BOOL, T), `CineLightRigBounceRatio` (F32, 0.45),
`CineLightRigTransitionSec` (F32, 0.9), `CineLightRigDamping` (F32, 0),
`CineLightRigCookieUUID` (STRING, shipped default), `CineLightRigSeed`
(U32, nonzero default — the weather "never a zero PRNG state" rule).

Transforms (3): `CineLightRigMirror` (BOOL, F), `CineLightRigOrbitYaw`
(F32, 0), `CineLightRigOrbitPitch` (F32, 0).

FX (1): `CineLightRigFX` (S32, −1 = none). Settings-backed so both hosts
show the same active FX and scenes round-trip it.

Policy (2): `CineLightRigShadowMode` (S32: 0 = no rig shadows, 1 = KEY only
[default], 2 = all rig projectors compete), `CineLightRigGizmo` (BOOL, F).

Per-light base (24): for Role ∈ {Key, Fill, Rim, Bg}:
`CineLightRig<Role>Yaw` (F32), `CineLightRig<Role>Pitch` (F32),
`CineLightRig<Role>Profile` (S32 0..23), `CineLightRig<Role>EV` (F32),
`CineLightRig<Role>Beam` (S32 0..2), `CineLightRig<Role>On` (BOOL).
Defaults = the LSL boot setup (`gBase`, engine `:57-62`):
Key 45/35/3(5600K Day)/0/Softbox/on, Fill −45/5/3/−2/Softbox/on,
Rim −135/45/4(6500K Cool)/−0.5/Standard/on, Bg 0/−20/3/0/Standard/off.

Total: 42 keys. Not persisted as settings: the anchor UUID (session +
scene data only — matches the cast being session-only, `lldirectorcast.h:28`).

### 3.5 UI

**One panel class, two hosts** — `ALPanelCineLightRig`
(`LLPanelInjector`-registered like `ALPanelWeatherSettings`,
`alpanelweathersettings.cpp:28`). **Template choice: `ALPanelWeatherSettings`**
— it is the closer match because it already combines settings-bound controls
with a named-preset combo (save/apply/delete LLSD files), a confirm-guarded
reset-all, an action button, and availability status; `ALPanelCineCamParams`
adds only the per-mode auto-hiding table, which the rig does not need. Cite
both as precedent; borrow the preset/reset member layout from weather
verbatim.

Layout (fits a Director tab and a standalone floater; the Weather tab hosts a
comparable panel today):

- Header row: Enable (master), Power, Anchor combo (You + cast members,
  rebuilt from `LLDirectorCast::getCast()` in `draw()`/`onVisibilityChange` —
  the non-settings-backed exception, synced by both hosts reading the
  controller), Gizmo checkbox.
- 4 light rows (Key/Fill/Rim/Bg, tinted labels): On | Yaw slider | Pitch
  slider | Profile combo (24) | EV spinner | Beam combo (3). All
  `control_name`-bound.
- Globals: Radius, Master EV, Offset Z, Bounce (check + ratio), Transition s,
  Damping.
- Aim row: Mirror | Orbit − / + (write `CineLightRigOrbitYaw`) | Pitch − / + |
  **Reset aim** (zeroes the three transform settings — a local recompute by
  construction, no reload; the LSL `RIG_MATH|RESET` semantics for free).
- Setups row: combo + Save / Delete (weather preset idiom; §6.6).
- FX row: combo (33 effects + None) + Stop + Seed spinner + a "shadow slots"
  hint text when more rig projectors request shadows than
  `BDMergeMaxSpotShadows` allows.
- Reset-all button with confirm, per-control `resetToDefault(true)` (the
  weather `onClickResetAll` idiom — IMPLIES, read its cpp at implementation).
- An EV-clip indicator per light: shows when `2^(EV_total − headroom)` hit
  the 1.0 ceiling this frame (§6.2) — model exports a `mClipped` flag.

Lockstep: settings-backed controls sync through `gSavedSettings` (V7);
controller-derived display (anchor, FX status, clip flags) is polled in
`draw()`, which both instances do independently — two views, one model.

Hotkeys: **none in v1** (§8) — the F-row audit (`aldirectorhotkeys.h:17-21`,
`doc/DIRECTOR_HOTKEYS.md`) has F2–F8 and 1–9 fully allocated; adding keys
reopens that audit. Deliberate deferral, stated up front.

---

## 4. The pure model layer

### 4.1 Shape

`namespace ALCineLightRigModel` — renderer-independent (`stdtypes.h` only,
the `ALWeatherModel` discipline; positions as plain floats, no LLVector):

```cpp
constexpr S32 LIGHT_COUNT = 4;            // v1 fixed; nothing below hardwires 4 except this
constexpr S32 PROFILE_COUNT = 24;
constexpr S32 BEAM_COUNT = 3;
constexpr S32 FX_COUNT = 33;
constexpr F32 PITCH_LIMIT_DEG = 85.f;
constexpr F32 MIN_RADIUS = 0.5f;

struct LightBase   { F32 mYawDeg, mPitchDeg; S32 mProfile; F32 mEV; S32 mBeam; bool mOn; };
struct Setup       { F32 mRadius; LightBase mLights[LIGHT_COUNT]; };
struct Transforms  { bool mMirror; F32 mYawDeg, mPitchDeg; };
struct Globals     { F32 mMasterEV, mHeadroomStops, mBounceRatio, mTransitionSec;
                     bool mBounceEnabled, mPower; U64 mSeed; };

struct EmitterState { F32 mOffX, mOffY, mOffZ;      // metres from rig centre
                      F32 mAimX, mAimY, mAimZ;      // unit vector (projector aim)
                      F32 mSR, mSG, mSB;            // sRGB base color (pre intensity)
                      F32 mIntensity;               // post-clamp [0,1]
                      F32 mLightRadius, mFalloff, mFovRad;
                      bool mOn, mClipped; };
struct RigFrame     { EmitterState mProj[LIGHT_COUNT], mOmni[LIGHT_COUNT]; };

Setup      sanitizeSetup(const Setup&);              // clamps, finiteness, index ranges
Transforms sanitizeTransforms(const Transforms&);
Globals    sanitizeGlobals(const Globals&);          // zero seed replaced, ranges

F32  wrap180(F32 deg);
F32  ease(F32 t);                                    // the LSL cubic in/out, verbatim
void computeLive(const Setup&, const Transforms&, LightBase out[LIGHT_COUNT]);
LightBase blendLight(const LightBase& s, const LightBase& t, F32 eased);
F32  intensityFromEV(F32 ev_total, F32 headroom_stops, bool* clipped);
void render(F32 radius, const LightBase live[LIGHT_COUNT], const Globals&,
            RigFrame& out);                          // geometry + exposure + bounce layer

// FX: pure function of (fx, seed, seconds since FX start). Overwrites the
// pre-transform tuples, exactly the LSL layering (the pack applied
// mirror/yaw/pitch AFTER its own pose math, fx_pack1 doLight:141-143).
void evalFX(S32 fx, U64 seed, F64 t_seconds, LightBase out[LIGHT_COUNT]);

const char* fxName(S32 fx);                          // MY_FX order, stable ids 0..32
const char* profileName(S32 idx);  void profileSRGB(S32 idx, F32 rgb[3]);
F32 beamFov(S32 idx);  F32 beamFalloff(S32 idx);
```

Flow per tick (in the controller): settings → sanitize →
(`evalFX` if FX active else base) → `computeLive`-style transform application
→ transition blend if one is running → `render` → `RigFrame` → emitters.
FX poses feed through the SAME transform + exposure pipeline as static
setups, matching `doLight()` in the pack.

Transitions are controller state (start snapshot, target, `t0` in
presentation seconds), evaluated with model functions — like the LSL, an FX
start cancels an active transition and STOP FX re-asserts truth via a fresh
`computeLive`.

### 4.2 Exposure and geometry math (normative)

- `EV_dist = log2(radius / 1.5)` — kept.
- `intensity = 2^(evLight + evMaster + EV_dist − headroom)`, clamped [0,1],
  `mClipped` set when clamped at the top.
- Projector offset: `(r·cosP·cosY, r·cosP·sinY, r·sinP)`; aim = −normalize(offset).
- Omni offset: same with `pitch − 45°`, floored at −85°; intensity ×
  `mBounceRatio`; falloff 0.75; light radius `r × 1.5`.
- Projector light radius `r × 2.2`, falloff from beam table
  (1.0 / 1.5 / 0.5) — all within the client's [0,2] clamp (O1).
- `on = base.on && power && intensity > 0.001` — the 0.001 threshold is
  load-bearing: FX use `ev = −10` to mean "off" (2⁻¹⁰ ≈ 0.00098 < 0.001).
  With headroom 2, −10 EV maps to 2⁻¹² — still under threshold. Keep the
  threshold on the PRE-headroom value so FX semantics are headroom-invariant.
  (This subtlety goes in the implementation brief verbatim.)
- Sanitize bounds: fov clamp [0.05, 2.9] rad (O5 — the client accepts raw),
  beam/profile index clamps, radius ≥ 0.5, pitch ±85 post-transform.

### 4.3 TUT test file: `alcinelightrigmodel_test.cpp` — what it asserts

Derived-state invariants (the LSL properties, stated as tests):

1. **Mirror twice = identity**: `computeLive(S, {mirror twice applied})`
   — apply mirror, apply mirror again, output bitwise-equal (`==` on F32)
   to `computeLive(S, identity)` for a table of setups incl. yaws at
   ±180 boundary.
2. **Orbit then reset = exact original**: transforms {yaw +15} then zeroed →
   output bitwise-equal to the pristine live state. No accumulation
   possible by construction; the test pins it stays that way.
3. **Setup switch preserves aim**: `computeLive(S2, T)` uses T unchanged —
   assert output equals hand-computed S2×T, and that T is not mutated.
4. Pitch clamp: base pitch 80 + transform 20 → 85 exactly; −80 − 20 → −85.
5. `wrap180` domain (−180, 180]; ±360 wraps; the boot setup's −135 survives.

Transition semantics:

6. `ease(0)=0`, `ease(1)=1`, `ease(0.5)=0.5`, monotone on a grid.
7. Shortest-path yaw: 170 → −170 blends through ±180 (20° arc), never 340°.
8. Discrete swap at e>0.5: profile/beam are start-valued at e=0.49,
   target-valued at e=0.51.
9. `on = onS || onT` throughout the blend.
10. Endpoint exactness: blend at e=1 is bitwise the target (no residue).

Exposure:

11. `intensityFromEV` monotone; +1 EV doubles pre-clamp; radius doubling
    adds exactly +1 stop via `EV_dist`; clamp + `mClipped` at the ceiling;
    the −10 EV "off" threshold holds for headroom ∈ {0, 2, 4}.

Sanitize (the weather pattern):

12. NaN/inf in every field → finite output; zero seed replaced; out-of-range
    profile/beam/fov/radius clamped; a hand-corrupted settings.xml cannot
    reach the renderer.

FX determinism (the core new guarantees):

13. **Bitwise replay**: for every fx in 0..32, `evalFX(fx, seed, t)` called
    twice → identical output structs (memcmp), across a t grid including
    0, sub-step values, step boundaries ±1 ulp-ish offsets, and huge t (1e7 s
    — finiteness under the internal fmod).
14. **Order independence (scrub)**: evaluating a shuffled t sequence equals
    evaluating it sorted — no hidden state (evalFX is a free function of its
    arguments; the test proves the implementation kept it that way).
15. **Seed separation**: different seeds differ somewhere for every
    randomness-using fx (list in §5.2); zero-randomness fx are seed-invariant.
16. **Step alignment**: for a discrete fx, outputs are constant on
    `[k·interval, (k+1)·interval)` and change exactly at the boundary.
17. **Latch closed-forms** (regression pins for the piecewise rewrites, §5.3):
    Stage Debut at beats {0, 6, 13, 18, 23, 40, 75} matches the documented
    pose table; Explosion phase edges at beats {0, 3, 12, 45}; Supernova at
    {0, 50, 53, 65, 75}.
18. Every fx id returns finite, in-range output (profile 0..23 after clamp,
    beam 0..2, |pitch| ≤ 90 pre-transform) for 10k random (seed, t) samples.

CMake: register exactly as `alweathermodel_test.cpp` is registered
(IMPLIES — copy the `LL_ADD_PROJECT_UNIT_TESTS` entry).

---

## 5. Determinism design (seed §4.3) and the 33-FX conversion table

### 5.1 The rules

- **Clock**: FX phase is `t = presentation_time − fx_start_presentation_time`.
  `presentation_time` is the frozen per-frame value from
  `LLTemporalFrameContext` (`lltemporalframecontext.h:50-73`), the same value
  the gobo animator (`pipeline.cpp:17615`) and the local fog manager
  (`llappviewer.cpp:5522`) already consume. Under Temporal Capture's
  `MANUAL_SCALE`/`PAUSED` the rig scales/freezes with the scene automatically;
  under the reserved `FIXED_FRAME_CAPTURE` mode a re-render evaluates the
  identical t sequence → identical light, which is the requirement. No new
  `LLTemporalFeature` drive bit is needed for v1: the rig is purely
  client-evaluated, so it follows the presentation clock unconditionally
  (the local-fog precedent), rather than being one of the stock-behavior
  drives the mask gates. INFERENCE on intent; flag for the implementation
  review.
- **Steps**: the LSL's `gFXStep` (tick counter) becomes
  `step = (S64)floor(t / interval)` — the virtual step index. Because output
  is a pure function of `step`, the weather model's "at most one discrete
  event per call even after a hitch" constraint dissolves: there is no event
  queue to flood; a hitch simply evaluates a later step. This is deliberately
  STRONGER than `ALWeatherModel::Controller` (which keeps a stateful PRNG
  stream and therefore needs monotonic time); FX have no cross-step state
  once the latch effects are rewritten in closed form (§5.3), so the rig can
  be stateless where weather cannot.
- **Randomness**: every `llFrand(x)` becomes
  `x * unitHash(seed, fx, streamKey)` where `unitHash` is a SplitMix64-style
  avalanche of the packed key `(seed ^ fx_id, step_or_cycle_index, light_index,
  draw_index)` mapped to [0,1). Stateless ⇒ scrubbing backwards reproduces;
  re-render reproduces; two viewers with the same seed reproduce (scene files
  carry the seed).
- **What "scrub / seek" means for an FX** (the question the LSL cannot
  answer): the FX pose IS `evalFX(fx, seed, t)`. Seeking sets t; there is
  nothing to rebuild, no fast-forward, no replay of intermediate ticks. A
  take that stores `(fx, seed, fx_start)` re-renders the identical light at
  any frame rate, in any order. Transitions are the one caveat: they are
  user-gesture events, keyed to the presentation timestamp at which the
  gesture happened; within one capture they replay deterministically, but
  they are not part of the FX function and a scrub across "when the operator
  clicked" cannot re-create the click. Stated as a documented limit.

### 5.2 Per-effect conversion table (all 33)

Legend — Class: **D** = discrete/beat (was `gFXStep`), **S** = smooth
(was wall-clock `fs`), **S+D** = smooth motion with discrete sub-events.
Rand: what used `llFrand`. Conversion: how it becomes
`f(seed, floor/frac of t/interval)`.

| # | Effect | Class | Interval s | Rand | Deterministic conversion |
|---|---|---|---|---|---|
| 0 | Police Sirens | D | 0.15 | none | ev by `step % 2` parity |
| 1 | Club Strobe | D | 0.1 | pitch, gate, color, ev ×4 lights | 4 hash draws per (step, light): pitch, gate>0.6, color 7+⌊16u⌋, ev |
| 2 | Fire Flicker | D | 0.2 | ev ×2 | 2 hash draws per step |
| 3 | Streetlight | S | 0.2 | none | closed-form over `fmod(fs,40)` |
| 4 | Neon Pulse | S* | 0.1 | none | `sin(t·0.4)` / phase-offset sin — already continuous (sits in the LSL discrete chain but uses t; port as smooth) |
| 5 | Paparazzi | D | 0.1 | gate, yaw, pitch ×4 | 3 hash draws per (step, light); flash ev fixed +2.0 — the poster child for headroom (§6.2) |
| 6 | TV Screen | D | 0.2 | profile pick, ev | 2 hash draws per step |
| 7 | Underwater | S | 0.2 | none | sin/cos of t, two frequency bands |
| 8 | UFO Abduction | S | 0.1 | none | linear yaw `15·fs` wrapped + sin ev |
| 9 | Haunted Flicker | D | 0.1 | blackout gate, ev | 2 hash draws per step |
| 10 | RGB Gamer | S+D | 0.2 | none | yaw linear in fs; profile `7 + ((⌊fs⌋+k·4) % 16)` |
| 11 | Disco Ball | S+D | 0.1 | color ×4 every 5 steps | motion closed-form; colors = hash(⌊fs/5⌋, light) — replaces the `gLastIS` edge detector |
| 12 | Warning Alert | S | 0.1 | none | linear yaw sweep |
| 13 | Matrix Drop | S+D | 0.15 | yaw per drop | pitch closed-form over `fmod(fs, dropLen)`; yaw = hash(⌊fs/dropLen⌋) — replaces `gDropIdx` |
| 14 | Thunderstorm | D | 0.05 | strike gate, ev, yaw, pitch | 4 hash draws per step (p≈0.1 strike). Note: LSL latches `on0=1` after the first strike and uses ev −10 as "off" between strikes — port keeps that via the intensity threshold, no latch needed |
| 15 | Searchlight | S | 0.1 | none | `sin(t·0.1)·90` |
| 16 | Heartbeat | D | 0.1 | none | lub-dub by `step % 15 ∈ {0,3}` |
| 17 | Movie Projector | D | 0.1 | ev, profile flip | 2 hash draws per step |
| 18 | Warp Tunnel | S | 0.05 | none | 4 phase-shifted `fmod(fs,20)` sweeps |
| 19 | Fairy Woods | S* | 0.2 | none | 3 out-of-phase sines of t (LSL discrete chain, t-based; port as smooth) |
| 20 | Short Circuit | D | 0.05 | two chained gates + ev | 3 hash draws per step (draw order fixed = LSL's sequential `llFrand` calls) |
| 21 | Red Alert Pulse | S* | 0.1 | none | `sin(t·0.2)·2` on both lights (same chain note as #4) |
| 22 | Aurora Borealis | S | 0.2 | none | 3 out-of-phase pitch swells |
| 23 | Cyber Scanner | S | 0.1 | none | `sin(t·0.15)·60` pitch scan |
| 24 | Shooting Star | S | 0.05 | none | piecewise over `fmod(fs,40)`: 10-step streak, dead tail |
| 25 | Elevator Fault | D | 0.1 | 4 ev jitters + rare glitch pick | 5 hash draws per step + `step % 15` blackout. **Beam m=3 → clamp to Snoot (N10)** |
| 26 | Car Pass | S | 0.07 | none | piecewise over `fmod(fs,55)`: headlights sweep / gap / tail-lights (Rosco Red) |
| 27 | Explosion | D | 0.08 | ember-phase ev jitters | `beat = step % 65`; phases {0-3 flash, 3-12 decay, 12-45 embers (2 hash draws/step), 45+ afterglow (1 gated draw)} rewritten closed-form (§5.3) |
| 28 | Supernova | D | 0.08 | none | `beat = step % 90`; pure closed-form of beat (build 0-50 with warm-shift, flash 50-53, collapse 53-65, remnant 65-75, dark) |
| 29 | Stage Debut | D | 0.15 | none | `beat = step % 80`; **latch-heavy** — full piecewise closed-form rewrite (§5.3); beam m=3 → Snoot (N10) |
| 30 | Swinging Lamp | S | 0.07 | per-FRAME ev jitter `llFrand(0.04)` | damped pendulum closed-form over `fmod(fs,230)` (`amp = 60·0.978^beat`); jitter → seeded value-noise of t (band-limited), making it frame-rate-INDEPENDENT — the LSL was frame-rate-dependent here; deliberate improvement |
| 31 | Parachute Flare | S | 0.12 | per-frame ev jitters ×2 | descent closed-form over `fmod(fs,110)`; jitters → seeded value-noise of t |
| 32 | Villain Reveal | S | 0.12 | none | piecewise over `fmod(fs,100)`: dark / key rise / hold + rim fade-in |

(*) #4, #19, #21 sit in the LSL's discrete branch chain but compute from
continuous t; they are smooth in every way that matters and port as smooth.

Counts: 14 discrete, 16 smooth, 3 smooth-with-discrete-sub-events.
Randomness users: 13 effects (#1,2,5,6,9,11,13,14,17,20,25,27,30,31 — 14
including Disco Ball's colors). Every one converts to the counter-based hash;
none needs a stateful stream.

### 5.3 The latch problem (must be in the implementation brief)

Discrete LSL effects mutate `y0..on3` and rely on values PERSISTING across
steps (set at beat k, still in force at beat k+n). A per-step pure function
must instead answer "what is the value at beat b" in closed form. Affected:
**Stage Debut** (poses latched at beats 5/12/16/22, all-off at 0 and ≥70),
**Explosion** (flash pose latched at beat 0, lights dropped in later phases),
**Supernova** (initial latch at 0, then per-beat overwrite), **Thunderstorm**
(`on0` latch — dissolved by the intensity threshold), and mildly **Car Pass /
Shooting Star / Villain Reveal** (each phase fully specifies its lights —
already closed-form). The rewrite is mechanical (piecewise over beat) but is
exactly where a careless transliteration would silently diverge — hence the
regression pins in test 17.

---

## 6. Answers to seed §4.1–§4.8

### 6.1 (§4.1) Emitter model

**6.1.1 — 8 emitters, omnis opt-in-but-default-on.** The omni layer was an
SL-lighting workaround (no per-light GI), but the viewer's probes give
ambient bounce, not dynamic per-light bounce; removing the omnis would
change every setup's look versus the LSL. Decision: keep the 4+4 model;
`CineLightRigBounceEnabled` default TRUE (parity), ratio 0.45 default.
Cost is negligible: 8 lights against a 256 default budget (V6), and the
omnis are plain point lights (no cookie, no shadow eligibility).

**6.1.2 — count stays 4 in v1; the model does not hardwire it.** What breaks
at N: the KEY/FILL/RIM/BG role naming (UI rows, role tints, shadow-policy
default "KEY only"), the settings key scheme (per-role names), the setup
file format, and all 33 FX (they address lights positionally and their
choreography is composed FOR four — e.g. Club Strobe's quad spread,
Warp Tunnel's 4-phase). `LIGHT_COUNT` is a single model constant and the
frame structs are arrays, so a later N-light variant is a settings/UI/FX
re-mapping problem, not a math rewrite. Deferred (§8).

**6.1.3 — invisible emitters + optional wireframe gizmo.** Emitters are
permanently `FORCE_INVISIBLE` and unselectable. `CineLightRigGizmo`
(default off) draws role-colored wire cones (projector frustum: origin,
aim, fov) plus a line to the rig centre, from the same `render_ui_3d` /
`gUIProgram` overlay path Prism uses (N8) — client-side, no depth, no
shared render state beyond the established overlay discipline. Drawing
hook: `ALCineLightRig::renderGizmo()` called where Prism renders its
guides (locate the exact call site in `llviewerdisplay.cpp`/`llprismlens`
at implementation; INFERENCE that the same hook serves).

### 6.2 (§4.2) Units and clamps — where LSL lied and where C++ ALSO lies

The honest table:

| Quantity | Sim clamp (LSL lived with) | Client reality | Design consequence |
|---|---|---|---|
| Intensity | [0,1] | **[0,1] too** — `LLColor4::clamp()` on the param block (`llprimitive.h:161`, O1). Shader gets `color × intensity × AlchemyGlobalLightScale` | Headroom must come from re-basing EV, not the renderer |
| Radius | 20 m | **20 m too** (`llprimitive.cpp:76`) | `orbitRadius × 2.2` saturates at orbit ≈ 9.1 m; exposure compensation (EV_dist) still works past that, reach does not. UI hint |
| Falloff | 0..2-ish | [0,2] (`llprimitive.cpp:79`) | LSL beam falloffs 0.5–1.5 all legal |
| Cutoff | n/a in LSL | [0,180] client | untouched (deferred path keys off falloff; leave default) |
| Spot fov/focus/ambiance | fov ≲ 3 | **unclamped** (`llprimitive.h:346`, O5) | model sanitize clamps fov [0.05, 2.9]; Softbox 2.8 stays legal |
| Gamma | none (hand `pow 2.2`) | proper dual API | profiles are sRGB art → `setLightSRGBColor()`; toggle deleted |

**Exposure re-base.** Keep `intensity = 2^EV_total` in the same stop units —
every profile EV and every FX ev constant carries over UNCHANGED — but
subtract `CineLightRigHeadroomStops` (default 2.0) before the ceiling:
nominal EV 0 at radius 1.5 → intensity 0.25, so FX that push to +2 EV
(Paparazzi flashes, Thunderstorm strikes, Explosion peaks — all clipped
flat in SL) genuinely read as flashes. Cost: the rig is 2 stops dimmer than
the LSL at identical numbers; the operator compensates once with Master EV
(or sets headroom 0 for exact LSL luminance). The clip indicator (§3.5)
makes the remaining ceiling visible instead of silent. Rejected
alternatives: per-light >1 intensity needs edits to the shared deferred
loops (§8); `AlchemyGlobalLightScale` is global and would brighten the whole
world's lights.

### 6.3 (§4.3) Determinism

Answered in full in §5. Summary of the deltas from the seed's assumptions:
the fork's presentation clock and frozen frame context already exist and are
already consumed by comparable systems (N3), so the rig takes
`presentation_time` as its ONLY clock; FX are pure stateless functions
(stronger than the weather Controller's stateful stream, and the "one
discrete event per call" rule becomes unnecessary rather than implemented);
seek/scrub is defined as direct evaluation; the seed lives in settings and
scene files.

### 6.4 (§4.4) Anchoring and per-frame update

- **Tick site**: `LLAppViewer::idle()`, immediately after
  `ALLocalFogManager::instance().tick(...)` (`llappviewer.cpp:5522-5523`) —
  after `gObjectList.update` (avatar joints current) and
  `LLActorMover::updateSuspendState`, before the camera dispatch and render.
  Ordering vs `calcNearbyLights`: consumed the same frame at
  `pipeline.cpp:7151` (N7). The ghost proxy ticks in the same window and its
  same-frame visibility is an in-tree proven property (comment at
  `llappviewer.cpp:5500-5503`). **Never a frame stale.**
- **Subject resolution**: `LLDirectorCast::resolve(mAnchor)` — null = self,
  stale = nullptr (V10). Ghost clones: `resolve` covers control avatars per
  its CastMember doc ("avatar or control-avatar id", `lldirectorcast.h:51`) —
  IMPLIES; verify a ghost's id resolves at implementation. On unresolved
  (death, unload, out of draw distance, region exit): lights off
  (intensity 0), objects retained, silent reacquire. No auto-retarget to
  self — the cast rule ("never double up on the agent") applies to lights
  too.
- **Spaces**: emitters live on `gAgent.getRegion()`; positions written as
  GLOBAL coordinates each tick (V1 pattern), so agent region crossings and
  region-origin shifts are absorbed by the existing object machinery. If the
  emitter's region dies (agent TP, region restart), the emitters die with
  it; `ensureEmitters()` recreates on the current region next tick. The
  subject being in a DIFFERENT region than the agent is fine — positions are
  global — until draw-distance unload makes it unresolvable.
- **Auto-height**: dissolved into the anchor definition — rig centre =
  subject chest joint + `CineLightRigOffsetZ` (§2). No more `+0.4` guess,
  no 0..3 clamp.
- **Fast subjects**: default rigid follow (a hard-mounted rig look). Optional
  `CineLightRigDamping` (seconds, 0 = off) applies the exact-exponential
  presentation-time ease (N9) to the rig centre only — aim still points at
  the true centre so the subject stays lit during catch-up.

### 6.5 (§4.5) Interaction with existing render systems

**6.5.1 Projector volumetrics / hero beams.** Rig projectors are ordinary
spot lights, so the existing per-UUID opt-ins apply directly:
per-light "Shaft" and "Hero beam" checkboxes call
`LLPipeline::toggleVolumetricShaft(id)` / `toggleHeroProjector(id)` (N4).
HARD CONSTRAINT (O4): shafts/froxel injection render only for projectors
holding a spot-shadow slot — so a shafted rig light must also be allowed to
win the auction (mode 2 below), and the UI says so in the hint text. The
backlog's decoupling fix (`MACHINIMA_FEATURE_BACKLOG.md:43`) is NOT in this
feature's scope; the rig must be correct in today's coupled world and will
simply get better when that lands.

**6.5.2 Shadow slot auction.** Facts: up to 6 slots, default 2 (O2);
priority = screen pixel area, or radius³ under `BDMergeStableSpotShadows`
(`llvovolume.cpp:3379-3389`); per-projector opt-out set exists
(`pipeline.cpp:15082-15101`); the auction swap runs in `setupSpotLight`
(`pipeline.cpp:17537-17561`). Danger: rig lights orbit the SUBJECT, i.e.
usually near the camera → high pixel-area priority → four rig projectors can
strip-mine both default slots and evict the set's projectors mid-shot.
Policy (`CineLightRigShadowMode`):
`0` — all four rig projectors enter the no-shadow set;
`1` (default) — KEY may compete, Fill/Rim/Bg are suppressed;
`2` — all compete (for users who raised `BDMergeMaxSpotShadows`).
The controller maintains the suppression set membership every time the mode
or emitter identity changes, and removes its UUIDs on shutdown (the sets are
also globally cleared on disconnect, `pipeline.cpp:15071-15080`). Who
decides is therefore: the existing auction, shaped by an explicit,
user-visible rig policy — no new auction code.

**6.5.3 The bdmerge light gate.** Verified semantics (N1): inert by default;
with `BDMergeLightToggles` + `BDMergeRenderWorldLights=false`, all
non-attachment lights vanish — including the rig, since rig emitters are
world objects. That combination (hide the world's messy lights, light with
the rig) is a core machinima use case, so the rig MUST be exempt. Mechanism:
at the six non-attachment call sites (N2 — `pipeline.cpp:9136, 9224, 9316,
9693, 16641, 19951`) the volume/vobj pointer is already in hand; change the
condition to
`else if (!vobj->isCineRigEmitter() && !bdmerge_should_render_light(false, false))`.
Inertness proof for the OFF path: `isCineRigEmitter()` is a member-enum
compare; with the feature off, no object of that kind exists, so the
expression reduces to the stock condition for every light in the world —
reviewable line by line, and the exact thing to hand the adversarial
review per CLAUDE.md rule 1.

**6.5.4 Froxel / gobo.** Froxel light injection: rig lights participate
exactly as far as they hold shadow slots (O4) and their UUID is in the shaft
set (`pipeline.cpp:13806-13808` matches prim + root ids — IMPLIES from
comment). Gobo: the override store, uniforms, and presentation-clock
animation all exist (N3, N4); a per-light gobo pattern UI is deferred (§8)
because rig emitters are unselectable and the current gobo UI addresses
selected objects. The default cookie ships as `CineLightRigCookieUUID`.

**6.5.5 Prism auxiliary renders.** PROVED yes: the Prism branch of
`calcNearbyLights` rebuilds its light list from `mLights` (V11) and rig
emitters are in `mLights` (V3); spotlights are exempted from the near-cull
(`pipeline.cpp:9154-9166`). The Prism aux SHADOW generation also applies the
world gate at `pipeline.cpp:19951` — covered by the same exemption. So a
VCam/monitor feed sees the lit subject; nothing further needed.

**6.5.6 Reflection probes / cube snapshots.** `calcNearbyLights` early-outs
during `gCubeSnapshot` (V12) so probe passes reuse the frozen main list;
local lights render into probe faces only at `RenderReflectionProbeLevel > 0`
(N5). So: at probe level 0 the rig (like every local light) is absent from
probes — stock behavior; at level ≥ 1 the rig is baked in, including
whatever FX pose held at bake time — a probe-lag cosmetic risk (§7 R6), not
a correctness problem, and not rig-specific. No code.

### 6.6 (§4.6) Persistence

Three layers, all using existing machinery:

1. **Live rig state = the 42 settings keys** — survives restart like every
   fork feature; reset-to-default works; hand-editing is guarded by model
   sanitize (test 12).
2. **Setups library** (the LSL "library" reborn): LLSD preset files in the
   per-user presets directory, subfolder `cine_light_rig`
   (`ALPanelWeatherSettings::presetsDir()` idiom — IMPLIES, copy its
   implementation). Versioned format:
   `{ version: 1, name: str, radius: f, lights: [ { yaw, pitch, profile_idx,
   profile_name, ev, beam_idx, on } × 4 ] }` — profile/beam stored as index
   AND name so a future table edit can fall back by name instead of shifting
   silently. Loading a setup replaces BASE ONLY; transforms re-apply (the
   v20 property). Factory setups (at minimum the LSL boot setup as
   "Classic 3-Point") compiled into the model.
3. **Director scene round-trip — required, like VCam** (N6):
   `scene["light_rig"] = ALCineLightRig::sceneData()` carrying: anchor UUID,
   the transforms, active FX + seed + FX-start phase, and the current base
   (denormalized, so a scene is self-contained even if the named setup was
   deleted); plus all `CineLightRig*` keys appended to `sceneSettingsList()`
   (`llfloaterdirector.cpp:642`). `applySceneData` follows the Prism
   pattern: data application only, never seizes anything live.

### 6.7 (§4.7) UI

Answered in §3.5. Named: class `ALPanelCineLightRig`, XUI
`panel_cine_light_rig.xml`, hosts `floater_director.xml` (new **Lights** tab
between Shafts and Weather) and `floater_cine_light_rig.xml`
(registration key `"cine_light_rig"`). Template: `ALPanelWeatherSettings`
(presets + reset + status), lockstep by settings binding (V7) plus
draw()-time polling for the controller-owned bits (anchor, FX status, clip
flags). Hotkeys: deliberately none in v1 (§8). Reset: per-control
`resetToDefault` behind the confirm dialog, weather idiom.

### 6.8 (§4.8) Failure modes / OFF path

- **OFF is inert, provably.** With `CineLightRigEnabled=false` (default):
  the tick is one cached-control read + return; zero objects exist; zero
  `mLights` entries; zero session-set entries; the six gate exemptions
  reduce to stock conditions (§6.5.3); the gizmo hook checks the same gate.
  The feature touches NO global GL state anywhere — every render-side effect
  flows through per-object light parameters that already exist for every
  prim in the world. This is the statement to hand the reviewer per
  CLAUDE.md rule 1, alongside the six gate lines.
- **Teardown removes everything.** `shutdown()`: for each emitter —
  `setIsLight(false)` (removes from `mLights` via `pipeline.cpp:10131`),
  remove UUID from the shadow/shaft/hero session sets, `markDead()`.
  Even without the explicit `setIsLight(false)`, `unlinkDrawable` scrubs
  `mLights`, `mNearbyLights` and both shadow-slot arrays (V13); doing both
  is belt-and-braces. The `LLPointer` members hold the objects through
  `markDead` (ghost precedent, `alghostmanipproxy.cpp:249`).
- **Logout/disconnect**: `shutdown()` from `disconnectViewer` beside
  `LLPrismLens::clearDesignations()` (`llappviewer.cpp:6059-6062`);
  the session sets are additionally cleared globally there
  (`pipeline.cpp:15071-15080`).
- **Viewer shutdown**: `LLApp::isExiting()` early-out in tick (never create
  during exit); `calcNearbyLights` already refuses to run then (V12).
- **Teleport / region restart**: emitters die with their region (object-list
  kill); per-tick `isDead()` checks trigger recreate on the new region —
  never dereference a dead emitter (test the pointers exactly as the proxy
  does, `alghostmanipproxy.cpp:379-382`).
- **Object-list churn** (the `CHANGED_LINK` analogue): rig objects are
  unlinked/unparented singletons; the only churn that can touch them is
  kill, handled above. No `llselectmgr` route exists because
  `mbCanSelect=false` AND the typed kind keeps any future selection path
  from misrouting into Local Mesh handling (the reason the kind enum exists,
  `alghostmanipproxy.cpp:202-204`).
- **Subject death/unload**: §6.4 — lights off, retain, reacquire.

---

## 7. Risks, ranked

### Class A — could take the client down

- **R1. Dead-emitter dereference across region teardown/TP.** The classic
  crash shape. Mitigation: `LLPointer` + `isDead()` per tick before every
  touch (proven proxy pattern); no caching of raw `LLDrawable*`.
- **R2. Object creation during shutdown/disconnect.** Creating viewer
  objects while the object list is being torn down. Mitigation:
  `LLApp::isExiting()` gate first in tick; `shutdown()` ordering in
  `disconnectViewer` before region teardown.
- **R3. The six pipeline.cpp gate edits.** They sit in the hot light loops
  of the beauty pass. A wrong condition there mis-gates EVERY world light.
  Mitigation: mechanical one-shape edit at all six sites; adversarial review
  explicitly against (a) the beauty pass with the feature off, (b)
  `BDMergeLightToggles` in all four on/off combinations. This is the only
  render-path code in the feature — concentrate the review budget here.

### Class B — could regress the beauty pass (no crash)

- **R4. Shadow-slot strip-mining.** Four subject-adjacent projectors win the
  2 default slots by pixel-area priority and evict set lights mid-shot.
  Mitigation: default `CineLightRigShadowMode=1` (KEY only), suppression via
  the existing opt-out set; UI hint; recommend `BDMergeStableSpotShadows`
  in the hint text. Residual risk: even KEY evicting one set light is a
  visible change the operator must own — it is at least deterministic and
  user-chosen.
- **R5. Crowding `RenderLocalLightCount`.** If a user lowered the budget to
  single digits, the rig's up-to-8 closest-to-subject lights displace world
  lights (deferred loop takes the closest N, V6). No code; documented.
  Default 256 makes this a non-issue in practice.
- **R6. Probe/FX bake lag.** Strobe-class FX baked into reflection probes at
  whatever instant the face rendered (N5) → stale flashes in glossy
  reflections between probe updates. Cosmetic, shared with every dynamic
  light in SL; documented, no code.

### Class C — correctness / UX

- **R7. Silent EV clipping** — mitigated by the per-light clip indicator and
  the headroom re-base (§6.2).
- **R8. Latch-effect divergence** (§5.3) — mitigated by the closed-form
  regression pins (test 17); this is the highest-probability silent-wrong
  area of the port.
- **R9. FX determinism eroded by future edits** — the bitwise/ordering tests
  (13–16) exist precisely to fail loudly when someone adds a stateful
  shortcut.
- **R10. Scene load before the anchor exists in-world** — benign by design
  (resolve-per-tick), verify in test plan.
- **R11. Softbox fov 2.8 optics** — near-hemispheric projector frustum is
  legal but extreme (`tan(1.4)≈5.8`); verify visually in the in-world test
  pass; sanitize already caps at 2.9.

---

## 8. Deferred out of the first delivery (decided now, with reasons)

Per the ship-whole rule these are declared up front; everything NOT listed
here ships in delivery 1 (model + controller + emitters + settings + both UI
hosts + scene round-trip + gizmo + tests + registration).

1. **Per-light gobo patterns.** The store/uniform/animation plumbing exists
   (N3, N4), but the current gobo UI addresses SELECTED in-world objects and
   rig emitters are deliberately unselectable — shipping this means
   duplicating the pattern-editor UI into the Lights panel, a meaningful UI
   subproject with zero engine novelty. The single default cookie
   (`CineLightRigCookieUUID`) covers LSL parity exactly (the LSL had one
   hardcoded texture). Follow-on: "Rig gobo controls" reusing the existing
   override API.
2. **N>4 lights.** Breaks role naming, settings scheme, setup format, and
   all 33 FX addressings (§6.1.2). The model is already N-clean
   (`LIGHT_COUNT`), so this is a later re-mapping, not a rewrite.
3. **New hotkeys.** The F-row and number-punch space is fully allocated and
   audited (`aldirectorhotkeys.h:17-21`); adding rig keys reopens
   `doc/DIRECTOR_HOTKEYS.md`. The rig is a set-and-tweak tool, not a
   transport — low cost to defer.
4. **Renderer-side intensity headroom (>1.0 per light).** Requires touching
   the shared deferred light loops that every light in the world flows
   through — exactly the shared-render-state risk class CLAUDE.md rule 1
   exists for, and the EV re-base removes the need for v1.
5. **Gizmo drag-editing** (grab a wire cone to re-aim a light). Needs
   ghost-manip-proxy-grade selection/tool plumbing per light. The v1 gizmo
   is display-only; sliders aim the lights.
6. **The projvol/shadow decoupling fix** (`MACHINIMA_FEATURE_BACKLOG.md:43`)
   — pre-existing, tracked separately; the rig is designed to be correct in
   the coupled world (§6.5.1).

---

## 9. OFF-LIMITS list for the implementation brief (Codex)

Everything not named in §3.1 is off-limits. Explicitly forbidden even where
adjacent: `indra/llprimitive/*` (no clamp "fixes" — O1 is a design input,
not a bug); all GLSL shaders; `llpresentationtime.*` /
`lltemporalframecontext.*`; `alweathermodel.*` and its test;
`bdmerge_should_render_light` / `bdmerge_should_render_projector` bodies
(`pipeline.cpp:9018-9051` — the exemption goes at the CALL sites, never
inside the gate); `setupSpotLight` / `setupSpotLightVolumetric` /
`renderDeferredLighting` beyond the one named line at 16641;
`llselectmgr.*`; `lldirectorcast.*` (consumed, not modified);
`alghostmanipproxy.*` / `lllocalmesh.*` (templates, not hosts).

---

## Appendix A — LSL constant tables carried into the model

24 profiles (name, sRGB) and 3 beams (fov rad, falloff) verbatim from
`cinematic_studio_render_engine_v20.lsl:75-76`; the FX pack's `PCOLORS`
(`fx_pack1.lsl:56-61`) is the same table minus names — the model keeps ONE
table, ending the LSL's duplicated-table drift risk. Factory setup
"Classic 3-Point" = engine `gBase` (`:57-62`), radius 1.5.

## Appendix B — glossary of proven API surface the controller uses

`gObjectList.createObjectViewer(LL_PCODE_VOLUME, region)`;
`LLVOVolume::{setVolume, setScale, setRotation, setPositionGlobal, setLOD,
markDead, isDead}`;
`LLVOVolume::{setIsLight, setLightSRGBColor, setLightIntensity,
setLightRadius, setLightFalloff, setLightTextureID, setSpotLightParams}`
(`llvovolume.h:264-305`);
`LLPipeline::{toggleVolumetricShaft, toggleProjectorCastShadows,
toggleHeroProjector}` (`pipeline.cpp:15055-15117`);
`LLDirectorCast::{instance, resolve, getCast}` (`lldirectorcast.h`);
`LLPresentationTime::currentFrame().presentation_time`
(`llpresentationtime.h:52`);
`LLDrawable::FORCE_INVISIBLE` (`alghostmanipproxy.cpp:242`).
