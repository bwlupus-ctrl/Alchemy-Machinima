# Cinematic Light Rig — Design Seed / Brief

**Status:** seed brief for a deep design pass. **No source modified.**
**Captured:** 2026-08-15.
**Deliverable this feeds:** `doc/CINEMATIC_LIGHT_RIG_DEEP_DESIGN.md`, which then feeds a Codex
`--prompt-file` implementation brief per CLAUDE.md.

---

## 1. What the user asked for

Port the in-world **"Cinematic Studio"** LSL light rig into the viewer as a **client-side feature**:
local cinematic lights that orbit an avatar, with no prims, no script, no rez rights, working on
no-rez parcels and in other people's sets.

The two LSL scripts that define the behaviour to match are checked in verbatim as reference:

- `doc/reference/cinematic_studio_render_engine_v20.lsl` — the rig engine (truth model, transforms,
  transitions, render).
- `doc/reference/cinematic_studio_fx_pack1.lsl` — the motion FX coprocessor (33 animated effects).

### User decisions already made (2026-08-15)

| Decision | Answer |
| --- | --- |
| Scope | **Full implementation, with a GUI floater.** Needs a deep think about how to *re-imagine* the design for C++ first — do not transliterate the LSL. |
| Anchor | **Your own avatar + any Director cast member.** |
| UI home | **Both** a Director Console tab **and** a standalone floater, **in lockstep**. |

"In lockstep" = the two hosts are two views of one model, not two copies of state. The fork already
has this pattern; see §3.4.

---

## 2. What the LSL rig actually does (behaviour to preserve)

Read the reference scripts for detail. The load-bearing ideas:

1. **Derived-state architecture (the whole point of v20).** `gBase` is the pristine loaded setup.
   `gTMirror / gTYaw / gTPitch` are three plain transform values. The live light state is
   *recomputed* from `gBase × transforms` on every change (`computeLive()`) — never accumulated in
   place. Consequences that must survive the port:
   - mirror twice = exact identity
   - orbit +15° then Reset = exactly the loaded setup, bit for bit
   - switching setups keeps your current aim (transforms re-apply to the new base)
   - Reset is a local recompute, never a reload round-trip
2. **4 logical lights** — KEY / FILL / RIM / BG. Each carries `yaw, pitch, profile, EV, beam, on`.
3. Each logical light drives **two emitters**: a **projector** (spot, aimed at the subject) and an
   **omni** (soft bounce fill at 0.45× intensity, pitch −45° from the projector).
4. **24 colour profiles** (gel/CT names → RGB) and **3 beam types** (Standard / Softbox / Snoot →
   fov + falloff pairs).
5. **Exposure model:** `intensity = 2^(EV_light + EV_master + EV_dist)` where
   `EV_dist = log2(radius / 1.5)` — i.e. moving the rig out keeps subject exposure constant
   (inverse-square compensation). This is a real cinematographic idea and must survive.
6. **Rig geometry:** spherical — `radius`, per-light `yaw`/`pitch`, plus a global `height` offset.
   Projector position = orbit vector + height; projector aim = look at rig centre.
7. **0.9 s eased transitions** (cubic in/out) on every change, with **shortest-path yaw blending**,
   discrete params (profile/beam) swapping at `e > 0.5`, and `on = onStart || onTarget` so a light
   stays lit through a cross-fade.
8. **FX layer** — 33 animated effects that take over the rig, split into *discrete/beat* effects
   (advance one step per tick) and *smooth* effects (wall-clock continuous, so timer jitter never
   lurches). On STOP FX the engine re-asserts truth and the FX layer goes idle.
9. Global toggles: power, mesh visibility, gamma, auto-height (aim rig centre at the subject's
   upper body).

---

## 3. Verified viewer anchors (facts, established by reading the code — not inference)

### 3.1 Client-side light objects are viable

`gObjectList.createObjectViewer(LL_PCODE_VOLUME, region)` + `mIsLocalOnly = true` +
`mLocalObjectKind` is an **established in-tree recipe**:

- `indra/newview/alghostmanipproxy.cpp:187-245` (`createProxy`) — the cleanest template: create,
  mark local-only + typed, `gPipeline.createObject()`, `setLOD`, `setVolume(box params)`,
  `setScale`, then `mDrawable->setState(LLDrawable::FORCE_INVISIBLE)` to suppress render submission
  while keeping valid extents.
- `indra/newview/lllocalmesh.cpp:1543-1557` — the original recipe this cites.
- `LLViewerObject::LOCAL_OBJECT_*` is the existing enum; a new kind will be needed so
  `llselectmgr` does not route these objects into Local Mesh handling.

### 3.2 No sim traffic — already guarded

`LLViewerObject::parameterChanged` at `indra/newview/llviewerobject.cpp:6831-6834` opens with:

```cpp
// Client-only objects have no sim counterpart -- never send param changes up.
if (local_origin && !isLocalOnly())
```

So setting light params on a local-only object cannot generate `ObjectExtraParams` traffic.

### 3.3 Local lights reach the deferred renderer

- `LLVOVolume::setIsLight(true)` → `setParameterEntryInUse(PARAMS_LIGHT, true, true)` →
  `gPipeline.setLight(mDrawable, true)` (`indra/newview/llvovolume.cpp:3186-3204`), which is what
  populates `LLPipeline::mLights`.
- `LLPipeline::calcNearbyLights` (`indra/newview/pipeline.cpp:9081`) scans `mLights` and requires
  `drawable->isState(LLDrawable::LIGHT)`. It does **not** require visibility, so `FORCE_INVISIBLE`
  is compatible with being a light.
- Full parameter API on `LLVOVolume` (`indra/newview/llvovolume.h:264-305`):
  `setIsLight`, `setLightSRGBColor`, `setLightLinearColor`, `setLightIntensity`, `setLightRadius`,
  `setLightFalloff`, `setLightCutoff`, `setLightTextureID` (projector), `setSpotLightParams`
  (`<fov, focus, ambiance>`).
- This maps 1:1 onto the LSL `PRIM_POINT_LIGHT` + `PRIM_PROJECTOR` rules the rig writes.
- `RenderLocalLightCount` defaults to **256** in this fork (`pipeline.cpp:9099`), so an 8-emitter
  rig is not near the cap.

### 3.4 Lockstep UI precedent

One registered XUI panel class instantiated in two hosts:

- `panel_cinecam_params` → used in `floater_cinematic_camera.xml:53` **and**
  `floater_director.xml:1257`.
- `panel_weather_settings` → used in `floater_director.xml:1535` **and**
  `floater_lightbox_settings.xml:4281`.

C++ side: `alpanelcinecamparams.{h,cpp}`, `alpanelweathersettings.{h,cpp}`.

### 3.5 Pure-model precedent (this is the shape the rig math should take)

`indra/newview/alweathermodel.h` — a renderer-independent namespace with `Config` / `Frame` /
`Controller`, a deterministic seeded PRNG, an absolute presentation-time clock, a fixed sim tick
with interpolation, and explicit sanitisation of settings that a hand-edited `settings.xml` could
corrupt. It is unit-tested under TUT at `indra/newview/tests/alweathermodel_test.cpp`.

### 3.6 Director console structure

`floater_director.xml` tab container `director_tabs` currently holds: Move, Path, Ghosts, Props,
Animate, Camera, Takes, Projector volumetrics, Weather, Atmospheric Volumes, Temporal. A **Lights**
tab belongs beside Projector volumetrics.

Cast/subject resolution already exists in `lldirectorcast.{h,cpp}`.

---

## 4. What the deep design pass must decide

These are the questions that make this a *re-imagining* rather than a transliteration. The design
doc must answer each with a stated rationale, and must flag anything it could not verify in-tree.

### 4.1 Emitter model
- 4 logical lights × (projector + omni) = 8 local objects, as in LSL? Or is the omni bounce-fill
  layer a **workaround for SL's lighting** that the viewer's own probes/GI make redundant — and if
  so, should it stay as an opt-in "bounce fill" control rather than always-on?
- Should the count stay fixed at 4, or become N (the LSL was capped by prim/script budget, which
  does not apply client-side)? If N, what breaks — the KEY/FILL/RIM/BG role naming, the setup
  format, the FX effects that address `y0..y3` positionally?
- Invisible emitters, or an optional **visible gizmo** (the frustum-gizmo precedent in
  `llprismlens` / `render_ui_3d` draws client-side wireframes with the UI shader, no depth) so the
  user can see where their lights are while aiming?

### 4.2 Units and clamps — where LSL lied and C++ does not have to
The LSL values were shaped by the *simulator's* clamps. Establish the viewer's real ranges and say
explicitly which LSL constants are now free:
- light intensity (LSL/sim clamps to 0..1; `2^EV` above 0 EV was being crushed — what does
  `LLVOVolume::setLightIntensity` / the deferred path actually accept?)
- light radius (sim caps at 20 m; `radius * 2.2` was written against that cap)
- falloff range, cutoff, and spot `<fov, focus, ambiance>` ranges
- **gamma**: the LSL does `pow(colour, 2.2)` by hand. The viewer has both `setLightSRGBColor` and
  `setLightLinearColor` — so the "gamma" toggle should become a correct colour-space choice, not a
  reimplementation. Say which is right and why.
- Decide whether the exposure model stays `2^EV` in the same units, or is re-based now that the
  ceiling is gone — and what that does to the 24 profiles and every FX effect's `ev` constants.

### 4.3 Determinism (this is the one the LSL cannot do and the fork requires)
The FX effects use `llFrand` and tick-counting. This fork records takes and has a **Temporal
Capture** system that renders at non-realtime rates. Therefore:
- every FX must be a **pure function of (effect, seed, presentation time)**, not of frame count or
  wall clock, so a re-render of the same take produces identical light;
- follow the `ALWeatherModel::Controller` pattern — absolute time in, seeded PRNG, fixed tick with
  interpolation, at most one discrete event per call even after a hitch;
- state explicitly how each of the 33 effects converts: the *smooth* ones are already wall-clock
  (`fs = elapsed / gInterval`) and port directly; the *discrete/beat* ones count ticks (`gFXStep`)
  and need re-expressing against the virtual step index; the `llFrand` ones need a seeded stream
  keyed so scrubbing backwards reproduces.
- Say what "scrub / seek" means for an FX — the LSL has no answer; the viewer needs one.

### 4.4 Anchoring and per-frame update
- Where does the tick live (viewer idle / `LLViewerDisplay` / director tick), and what is the
  ordering constraint against `calcNearbyLights` so the rig is never a frame stale?
- Subject resolution through `lldirectorcast` — own avatar, another avatar, a ghost clone. What
  happens when the subject dies, unloads, goes out of draw distance, or the region changes?
- Agent-space vs region-space placement, and region-crossing/`shift` handling.
- Auto-height: derive from a skeleton joint rather than the LSL's crude `+0.4` guess?
- What happens when the subject moves fast — does the rig follow rigidly, or is there damping?

### 4.5 Interaction with the fork's existing render systems
Each of these already exists here and must be reasoned about explicitly:
- **Projector volumetrics / hero beams** — a rig of 4 projectors is exactly the input that system
  wants; what is the policy?
- **Shadow slot auction** — there are only 2 spot-shadow slots (`MAX_SPOT_SHADOWS`), and
  `doc/MACHINIMA_FEATURE_BACKLOG.md` records a known pre-existing bug in the projvol/shadow
  interaction. Which rig lights get shadows, and who decides?
- **`bdmerge_should_render_light(is_attachment, is_own_avatar)`** gate in `calcNearbyLights` — if
  the user has world local lights off, does the rig vanish? Almost certainly the rig must be exempt.
  Verify the gate's semantics before asserting an exemption.
- **Froxel injection / gobo** (`doc/CINEMATIC_GOBO_AND_LOCAL_FOG_SPEC.md`) — the projector texture
  is currently one hardcoded UUID in LSL; client-side we can ship a gobo library. Is that in scope
  or a stated follow-on?
- **Prism auxiliary renders** — `calcNearbyLights` has a whole `sPrismLensRender` branch that builds
  an independent light list. Does the rig appear in Prism/VCam camera feeds? (It should, or a
  virtual camera would see an unlit subject.)
- Reflection probes / cube snapshots — `calcNearbyLights` early-outs on `gCubeSnapshot`.

### 4.6 Persistence
- Setups (the LSL "library") — where do they live? `settings.xml` controls, a preset directory like
  the existing `PRESETS`, or Director scene save/load?
- Must a rig round-trip through a **Director scene** save/load, like the VCam virtual camera and
  virtual screen do?
- What is the versioned on-disk format for a user-authored setup?

### 4.7 UI
- The shared panel class (name it), its two hosts, and how lockstep is achieved — control-backed
  widgets bound to the same settings, a singleton model with an observer, or both. Cite which of
  `alpanelcinecamparams` / `alpanelweathersettings` is the better template and why.
- Layout for 4 lights × (yaw, pitch, profile, EV, beam, on) plus globals plus setups plus FX,
  in a form that fits a Director tab **and** a standalone floater.
- Hotkeys through `aldirectorhotkeys` (the LSL had none; the fork's other features do).
- Reset-to-default wiring, per the fork's `resetToDefault` idiom.

### 4.8 Failure modes / off-path
Per CLAUDE.md rule 1 (shared render state): prove the **feature-off path is inert**, and that
teardown removes every local object and every light-set entry. State what happens on logout,
teleport, region restart, and viewer shutdown (`LLApp::isExiting`), and on `CHANGED_LINK`-style
object-list churn.

---

## 5. Constraints on the design doc itself

- **Label claims.** Separate what the code PROVES, what docs IMPLY, and what is INFERENCE
  (CLAUDE.md rule 3). Cite `file:line` for anything load-bearing.
- **No source modified** by the design pass.
- **Ship whole** (CLAUDE.md): engine + model + settings + UI + registration in one delivery. If any
  part genuinely must be deferred, say so **in the design doc**, up front, with the reason.
- The design must be concrete enough to become a Codex `--prompt-file` implementation brief:
  named files, named classes, named settings keys, named XUI files, and an explicit
  OFF-LIMITS file list.
- Flag every place the LSL behaviour should **not** be copied, and say what replaces it.
