# BD → Alchemy Merge Patch Log

Tracking log for the Black Dragon → Alchemy feature merge, per the make-ready
work specification (`Make-Ready_BD_to_Alchemy_Feature_Merge.md`). One entry per
work item. Newest entries at the top of each table.

## Conventions (established by A0.1)

- **One item = one revertable commit** on `develop`, message prefixed
  `[BDMerge <item-id>]`. Revert with `git revert <sha>`; no item may depend on
  another item's commit unless its `Depends on` gate says so.
- **Runtime gate:** every item that changes behavior is gated by a debug
  setting in `indra/newview/app_settings/settings.xml`, default **off** unless
  the item spec says otherwise. Setting comment carries the `[BDMerge <id>]`
  tag. Per-frame reads use `LLCachedControl`.
- **Compile-time gate (only when needed):** `LL_BDMERGE_<ITEM>` define in
  `indra/newview/CMakeLists.txt`, defaulting to the feature compiled **in**
  (runtime setting still gates behavior). Use only where a runtime gate is
  impossible (e.g. shader-tree restructuring).
- **Origin notes:** files lifted from a donor keep their license header intact
  (BD headers, or the Firestorm header per
  `FIRESTORM-SOURCE_LICENSE_HEADER.txt` in the donor repo). Blended files get
  a short header note: donor, BD commit/release (or fork commit), and the
  `[BDMerge <id>]` tag. Contributor credits (Tofu Buzzard, Geenz, …) are
  retained in headers and mirrored here.
- **Follow-up fixes** from the donor's changelog land in the same commit as
  the feature (Ground Rule 4).
- **Log entry format:** item id, commit sha, donor + donor ref, gate name(s),
  credits, notes.

## Donors

| Key | Donor | License | Ref |
|-----|-------|---------|-----|
| BD | Black Dragon viewer (NiranV Dean) | LGPL-2.1 (LL-derived) | per-item commit/release |
| FSX | bwlupus-ctrl/phoenix-reshade-XL (Firestorm fork) | LGPL-2.1 | https://github.com/bwlupus-ctrl/phoenix-reshade-XL |

## Items landed

| Item | Commit | Donor | Gate(s) | Credits | Notes |
|------|--------|-------|---------|---------|-------|
| C8 — slider precision override | `085a03ab32` | BD | `BDMergeSliderPrecision` | NiranV Dean (Black Dragon) | Donor: BD 94c47d42de (exact isolated commit; the later BD 949792d79d "overdrive beyond min/max" is a different feature, deliberately NOT ported). Typed text-field commits bypass increment snapping when gated on; thumb-drag/keys/wheel always snap. Gated globally via settings (LLCachedControl on LLUI config group, llbutton.cpp precedent) instead of BD's per-widget XUI param (would be dead complexity — no XML sets it). LLSpinCtrl verified not to snap typed input (no change needed); LLMultiSliderCtrl has the same snap pattern but BD's donor commit doesn't touch it — left for donor parity, legit follow-up if ever needed. |
| B4 — head/eye tracking degree limits | `c8f9fdd86e` | BD | `BDMergeHeadEyeLimits` (+ `BDMergeHeadRotationLimit`/`BDMergeEyeRotationLimit`, degrees) | NiranV Dean (Black Dragon) | Donor: BD 525e398af5 (2019-04-02) + follow-ups 637c2c6f7d/9b824a0b50/29d4267a1e. Replicated BD's behavior, not its wiring: BD reads gSavedSettings inside llcharacter (layering violation); here newview pushes per-frame via LLCachedControl into LLHeadRotMotion/LLEyeMotion statics (sGlobalTimeFactor pattern). Gate-off resets statics to stock constants each frame — bit-identical stock behavior. Gate-on defaults (72°/27°) equal Alchemy's stock constants, not BD's reduced 60°/40°. |
| B2 — camera presets (Z focus/offset, per-preset custom) | `b37c655a69` | BD | `BDMergeCameraPresets` | NiranV Dean (Black Dragon) | Alchemy already ships LL's full XYZ offset/focus/smoothing preset system (save/load/delete/per-control reset) — diff-first found only two genuine deltas: BD-style auto-persist-of-live-edits into the active preset, and true in-place rename (neither stock LL nor BD's own UI had rename; added to meet the spec's rename acceptance). BD's hyphen-delete bug verified not to reproduce (Alchemy uses LLURI::escape consistently). Donor ref: BD 152762d400 (2018-12-03) + 4ff6498a7e. |
| A0.2 — depth-buffer baseline characterization | `3a89dbb9cf` | — | n/a (docs + test tool only) | — | `doc/BD_MERGE_DEPTH_BASELINE.md` + `scripts/perf/bdmerge_depth_compare.py` (self-tested against synthetic captures). In-world baseline reference captures owed at next login; procedure in the doc §6. |
| A0.1 — patch loop + build-flag scaffolding | `d1e60e092a` | — | `BDMergeSamplePatch` (no-op sample) | — | Establishes conventions above; sample gated no-op in `LLAppViewer::idle()`. |
| B3a — animation speed / slow-motion (all avatars) | `333e1e57b4` | FSX | `AnimationTimeFactor` via Advanced ▸ Animation Speed (`LLMotionController::sGlobalTimeFactor`) | Firestorm/Phoenix contributors; custom implementation by bwlupus-ctrl | Landed before this log existed, as part of the phoenix-reshade-XL port. Composition with B3 (Freeze World) still owed when B3 lands: freeze fully stops, slow-mo scales when not frozen, Poser (PR-2) overrides both. |

## Partial / related pre-existing work (not spec items, affects scoping)

Commit `333e1e57b4` (phoenix-reshade-XL port) also landed:

- **Cinematic Camera** (`llcinematiccamera.cpp/h`): bone-lock (GoPro) mode +
  Orbit / Fly Hover / Sweep / Crane patterns. Overlaps **B6 (Bone Camera)**
  and **B8 (Cinematic camera mode)** — when executing B6/B8, diff against
  this first and extend rather than duplicate; BD behavior wins on conflicts
  per PR-0.
- **Flycam Camera Operator** (`llcameraoperator.cpp/h`): procedural handheld
  camera. Adjacent to **B9 (gamepad-native flycam)** — B9 is about input
  mapping/deadzone/smoothing (PR-3, BD authoritative) and must compose with,
  not replace, the operator.
- **ReShade bridge scaffold** (`llreshadebridge.cpp/h`, behind
  `LL_RESHADE_ADDON=0`): outside the merge spec; render items (A-series)
  should avoid breaking its post-`renderFinalize()` capture point.

## A1.1 recon finding (2026-07-09) — donor code no longer exists

Ground Rule 6 verification against BD master (v5.6.3, `I:\black-dragon`)
found that **BD's separable shader loading was dismantled during BD's PBR
refactor.** Current BD's `llviewershadermgr.cpp` and its `setShaders()` path
are structurally identical to Alchemy's monolithic LL baseline; SSAO/shadow
toggles in current BD trigger the same full recompile Alchemy does. What
survives in BD:

- Dead, commented-out granular handlers (`handleSSAOChanged`,
  `handleSSRChanged`, …) at BD `llviewercontrol.cpp:933-976`, calling
  per-feature loaders (`loadShadersSSAO` etc.) that were deleted.
- Two live, small recompile-avoidance wins that DO port directly:
  `handleShadowMapsChanged` (BD `llviewercontrol.cpp:919` — shadow-resolution
  changes do `allocateShadowBuffer()` realloc instead of full recompile) and
  `handleRenderDeferredLightsChanged` (`:750`, flag-only).
- Historical design reference: anchor commit `16bcf38b9c` ("reload shaders
  only where necessary"), follow-up fixes `a9a8d6b3ad`, `7c0b4ce307`,
  `369109abd2`, `d7dd2f38cc` et al.; dismantled in `8b9bc67a72` + PBR merges.

**Ruling (user, 2026-07-09): descope — closed with no code change.**
Follow-up diff showed even the two "portable" handlers don't port standalone:
`handleShadowMapsChanged` reads BD's Vector4 per-cascade
`RenderShadowResolution` (a constituent of **A4.1**, setting absent in
Alchemy), and `handleRenderDeferredLightsChanged` services BD's
light-toggle flags (constituent of **A5.6**, `sRenderDeferredLights` absent
in Alchemy). Both port *with their owning items* (Ground Rule 3 — no
double-application). Alchemy already avoids recompile on shadow-resolution
scaling via `RenderShadowResolutionScale` → `requestResizeShadowTexture()`
(`llviewercontrol.cpp:271,998`). Consequence: items formerly gated on A1.1
(A3.1, A5.x) are ungated; toggle-testing pays a full-recompile stall, same
as current BD.

## Item status board

| Item | Status | Blocked by |
|------|--------|-----------|
| A0.1 scaffolding | **done** | — |
| A0.2 depth baseline | **done** (reference captures owed at next in-world session) | — |
| A1.1 separable shaders | **closed — no port** (2026-07-09 ruling: descope; see "A1.1 recon finding") | — |
| A1.2 resolution autoscale | open | — |
| A1.3 shadow softening kernel | open | — |
| A2.1 near-clip reduction | open | A0.2 |
| A2.2 alpha-out-of-depth | open | A0.2 |
| A2.3 forced alpha masking | open | A2.2 |
| A2.4 rigged alpha-swap flicker | open | A2.2/A2.3 |
| A3.1 decouple SSAO/shadows/SSR | open | — |
| A3.2 volumetric lighting | open | A2.1, A2.2, A3.1 |
| A4.1 per-cascade shadow res | open | A1.3 (soft) |
| A4.2 quadratic shadow dims | open | A4.1 |
| A5.1 tonemappers | open | — |
| A5.2 HDR/auto-exposure | open | A5.1 |
| A5.3 chromatic aberration | open | — |
| A5.4 motion blur | open | — |
| A5.5 sepia/greyscale/posterize/CAS | open | — |
| A5.6 light-source toggles | open | — |
| A5.7 high-altitude shadows | open | A1.3 (soft) |
| B1 machinima sidebar | open | A-series settings it surfaces |
| B2 camera presets | **done** | — |
| B3 freeze world | open | — (coordinate with C6) |
| B3a animation speed | **done** (`333e1e57b4`) | — |
| B4 head/eye tracking limits | **done** | — |
| B5 max cam/pelvis diff | **closed — already present via LL upstream** (2026-07-09: `AvatarRotateThresholdSlow`/`Fast` debug settings wired at `llvoavatar.cpp:4613` — same change BD carries as LL commit 9fe788e031; Ground Rule 3, no double-apply. Slider UI deferred to B1 sidebar, which should surface these two settings) | — |
| B6 bone camera | open | — (diff vs `llcinematiccamera` first) |
| B7 poser | open | — (phased) |
| B8 cinematic camera mode | open | B6 |
| B9 gamepad flycam | open | — (compose with `llcameraoperator`) |
| B10 pose sync | open | B7 |
| B11 lookAt broadcast off | open | — |
| B12 mouselook head scaling | open | — |
| B13 EEP editor rework | open | — |
| C1 client AO | open | — |
| C2 movement conveniences | open | — |
| C3 camera QoL | open | — |
| C4 click-to-walk hardening | open | — |
| C5 rebindable keys | **closed — no port** (PR-1; verified 2026-07-09: `llkeyconflict.cpp/h` + `key_bindings.xml` present, wired into preferences) | — |
| C6 snapshot conveniences | open | B3 coordination |
| C7 texture channel lock | **closed — already present via shared upstream** (2026-07-09: "Synchronize materials" checkbox, `SyncMaterialSettings` + `syncOffsetX/Y`/`syncRepeatX/Y`/`syncMaterialRot` in `llpanelface.cpp`, upstream MAINT-3223 commit `69db7cd947` carried identically by BD and Alchemy; PBR/GLTF transforms untouched by sync in both trees, matching BD scope) | — |
| C8 slider precision | **done** | — |
