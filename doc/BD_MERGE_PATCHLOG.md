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
| G5.0 — memory-detection prerequisites & fast wins | (this commit) | novel (spec-native tuning per Alchemy Memory Analysis 2026-07-07) | `MaxHeapSize64` (0 = auto, 16 = stock), `CacheSize` | Geenz (VRAM budget/divisor design, upstream) | Verified all Analysis findings against this tree first (Ground Rule 6). **Finding 1 (VRAM divisor) already resolved upstream** — Alchemy set `RenderTextureVRAMDivisor` default to 1 in `695247539b` ("mitigate frame stuttering"); Analysis read the in-code fallback (2), settings.xml overrides it. No port (Ground Rule 3); existing `target = min(budget−512, budget×0.8)` (`llviewertexture.cpp:505`) already leaves spec-recommended headroom (~25.6GB target on 32GB). **Finding 6 (heap cap):** `MaxHeapSize64` 0-sentinel auto-sizes to ⅞ of installed RAM, floored at the legacy 16GB default (→168GB on the 192GB target; no hardcoded GB figures per spec banner); default flipped 16.0→0.0. **Finding 3 (detection):** verified 64-bit clean end-to-end — `ullAvailPhys` → `U64Bytes` (`llsys.cpp:800`), `LLMemoryAdjustKBResult` only adds 1MB, 192GB fits `U32Kilobytes`. The one real distortion WAS the heap cap: `llmemory.cpp:157` clamps reported-available to `cap − RSS`, so a viewer using ~15.5GB reads <512MB "available" → false `isSystemMemoryLow()` → bias slam ×`getSystemMemoryBudgetFactor()` + emergency purge, with ~176GB actually free. Plausibly the direct cause of the observed mid-scene degrade; fixed by the auto-size. **Finding 4 (disk cache):** `CacheSize` default 6144→32768 (= `MAX_CACHE_SIZE` ceiling, `llappviewer.cpp:4484`, not raised — returns beyond it shift to G5.1's decoded pool); prefs spinner already allows 32768. Revert = set `MaxHeapSize64` 16 / `CacheSize` 6144 / `git revert`. **Validation owed in-world (spec G5.0):** memory readout >16GB, `gGLManager.mVRAM` detects full 32GB, capture-session degrade check with GPU memory monitor. |
| C8 — slider precision override | `bd71dde48b` | BD | `BDMergeSliderPrecision` | NiranV Dean (Black Dragon) | Donor: BD 94c47d42de (exact isolated commit; the later BD 949792d79d "overdrive beyond min/max" is a different feature, deliberately NOT ported). Typed text-field commits bypass increment snapping when gated on; thumb-drag/keys/wheel always snap. Gated globally via settings (LLCachedControl on LLUI config group, llbutton.cpp precedent) instead of BD's per-widget XUI param (would be dead complexity — no XML sets it). LLSpinCtrl verified not to snap typed input (no change needed); LLMultiSliderCtrl has the same snap pattern but BD's donor commit doesn't touch it — left for donor parity, legit follow-up if ever needed. |
| B3 — Freeze World | (this commit) | BD | `BDMergeFreezeWorld` (+ runtime-only `UseFreezeWorld`, Persist 0) | NiranV Dean (Black Dragon) | Donor: BD b4cfc2cb83 (+ fab3226ac5/4f4f37a4c3/e8cbb4eebd). Avatar pause handles + stock FreezeTime; particles additionally halted in idle() (BD leaves them running; spec requires full scene). Snapshot-floater checkbox occupies the freeze-frame slot when gated on (zero layout slack in advanced panel; C6 owns future floater rework). Guards on every stock FreezeTime-clear path (snapshot updateLayout/dtor/onClose, 360capture) so no half-frozen state. Composes with B3a: pause independent of sGlobalTimeFactor, slow-mo factor survives freeze untouched. Closing the floater always thaws — BD's reopen-refreeze memory deliberately dropped (their bug a1ddcd69a9). Known minor: llscenemonitor also writes FreezeTime (obscure dev tool, unguarded, recoverable by retoggle). |
| B11 — hide lookAt/focus broadcast | `09fef181f3` | BD | `BDMergeHideLookAt` | Kyler Eastridge (Black Dragon) | Donor: BD 82477c484d (feature) + 5785bfdb70 (stuck-head fix: one-time IDLE clear-effect send on enable). Ported only the hide-broadcast half — the limit-distance half (`LimitLookAtTarget`/`LimitLookAtTargetDistance`) is already in Alchemy and MORE elaborate than BD's (per-object exceptions). Deliberately not named `EnableLookAtTarget`: Alchemy already uses that key for an unrelated local-indicator-hide feature. Single choke point: all lookAt setters funnel through LLHUDEffectLookAt::setLookAt(). |
| A1.2 — resolution-aware autoscale (SSAO/shadow-blur/DoF) | (this commit) | BD | `BDMergeSnapshotAutoscale` (+ `BDMergeSnapshotAutoscaleMultiplier`, F32, Persist 0) | NiranV Dean (Black Dragon) | Donor: BD 8eed1a6768 (+ derivation-site fix 33556578a1, non-persist multiplier fa96df28ee, close-reset intent of 0b37fb26fd applied as multiplier reset in onClose). Multiplier = output/window height, derived in llfloatersnapshot updateResolution while gated on; pipeline.cpp reads through a single helper that forces 1.0 when gated off (stricter than BD — no stale scaling possible). Scales: DoF FOV input (renderFinalize), SSAO radius/max-radius (bindDeferredShader), shared shadow+SSAO soften blur size (renderDeferredLighting — Alchemy merged those into one pass post-BD). NOT covered: SSR and volumetric/godrays (donor never touched them; manual reconfig still needed). No floater checkbox — debug-setting gate only; trivially wireable later via control_name. |
| B12 — mouselook head-bone scaling (experimental) | `27fb878af7` | BD | `BDMergeMouselookHeadScale` | NiranV Dean (Black Dragon) | Donor: BD 605f6a7416 (+ fix tail 5f3cd7dcca/1bdedd1137/6bef60d304/5ee5e20dfc/f3196e7aa7/771c862a8a/518b3bbca0). Clean-room reimplementation in llvoavatar (BD drives it from its Poser subsystem, absent here): per-frame apply/restore in idleUpdate (self only), depth-first walk from mHead incl. bento tree. Fixes BD's own getJoint("HEAD") bug (resolves to the collision volume — BD never actually scales mHead). Restore = per-joint default-scale snap-back, not resetSkeleton (sidesteps BD's drift-bug tail); computeBodySize guarded at 3 call sites while scaled + recomputed once on restore (fix-forward of BD 518b3bbca0 and of the lost-recompute defect caught in coordinator review). Known edge: CinematicCamJoint pointed at a descendant of mHead collapses toward mHead's origin while active in mouselook. |
| C3 — camera QoL (wheel modifiers, mouselook offset, numeric fields) | `8e110ced0f` | novel (no BD donor exists for any sub-feature) | `BDMergeCameraQoL` (+ `BDMergeMouselookOffsetX/Y/Z`, F32 m) | — | Diff-first: Shift/Ctrl+wheel height+pitch already in stock Alchemy (handleScrollWheel, upstream commit dd72a05cf14 — mapping is mirrored vs the spec's wording; stock kept, no remap) and numeric fields beside view-angle/distance sliders already present (can_edit_text sliders + advanced-view spinners on the same control_name; synergizes with C8's precision gate). Net-new: X/Y/Z mouselook offset, additive to stock head_offset in calcCameraPositionTargetGlobal's mouselook branch, rotates with the avatar; 3 F32 keys so prefs spinners bind via control_name with zero new C++. UI on Move ▸ Mouse Input tab, enabled_control-bound to the gate. |
| C2 — movement convenience flags | `de36febb9f` | Catznip via BD | `BDMergeMovementFlags` | Catznip (CATZ-400) | Partial-port by design: tap-tap-hold-run (`AllowTapTapHoldRun`) and always-run parity verified already present in Alchemy — closed, no port. Nimble ported both halves: jump-input side (llagent.cpp moveUp adds AGENT_CONTROL_FINISH_ANIM, no AgentUpdate semantics change per PR-1) + landing side (llviewermessage.cpp process_avatar_animation skips PRE_JUMP/LAND/MEDIUM_LAND/STANDUP for own avatar only, sends finish-anim). Donor's two keys collapsed into one gate (coordinator ruling). EXT-2781 fly-hack replicated inside the skip path so gating on can't regress the stuck-fly bug. Other avatars' animations untouched in both gate states. |
| A5.6 — independent light-source toggles | `f84860fa50` | BD | `BDMergeLightToggles` (+ 4 per-class keys, default ON) | NiranV Dean (Black Dragon) | Donor: BD pipeline.h/cpp sRenderOwnAttachedLights/sRenderOtherAttachedLights/sRenderDeferredLights (the last is BD's world-light gate despite the misleading name/comment — its UI label is "Render World Lights"). Filters slot in AFTER stock RenderAttachedLights at all 4 gathering/render sites (calcNearbyLights ×2, setupHWLights, renderDeferredLighting). Projector class is novel — BD's RenderSpotLightImages/Reflections keys are dead (wiring commented out); implemented as fallback-to-omni via existing isLightSpotlight() branch points. LLCachedControl locals instead of BD's statics+listeners (hot-path convention). |
| C6 — snapshot floater conveniences | `92335aa294` | BD | `BDMergeSnapshotExtras` (+ `BDMergeSnapshotRememberMode`/`BDMergeSnapshotLastPanel`/`BDMergeSnapshotResolutionUnlock`/`BDMergeSnapshotAddDateTime`) | NiranV Dean (Black Dragon); LLFloaterBigPreview by Merov (stock LL, resurrected from Alchemy history `44c6630838^`) | Donor: BD fab3226ac5 (detachable big preview) + f0db8914d0 (double-toggle crash fix folded in) + d6c8a45325 (UI-snapshot exclusion, reimplemented as draw() early-return) + 16143bb98b (filename date toggle, default flipped to keep-date so gate-on preserves stock filenames) + 4d0faf18c4 + BD's 12228 res-cap raise (12288 here — BD's value looks like a typo — runtime-gated, save-to-disk panel only, warning alertmodal). Remember-mode extended to cross-session (BD was within-session only; spec asks sessions). BD's leftover FreezeTime-thaw in onClickBigPreview deliberately NOT ported (would clobber B3). BD HEAD itself is broken for this feature (CMakeLists references a deleted file). Gate-off layout bit-identical. |
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
| A1.2 resolution autoscale | **done** | — |
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
| A5.6 light-source toggles | **done** | — |
| A5.7 high-altitude shadows | open | A1.3 (soft) |
| B1 machinima sidebar | open | A-series settings it surfaces |
| B2 camera presets | **done** | — |
| B3 freeze world | **done** | — |
| B3a animation speed | **done** (`333e1e57b4`) | — |
| B4 head/eye tracking limits | **done** | — |
| B5 max cam/pelvis diff | **closed — already present via LL upstream** (2026-07-09: `AvatarRotateThresholdSlow`/`Fast` debug settings wired at `llvoavatar.cpp:4613` — same change BD carries as LL commit 9fe788e031; Ground Rule 3, no double-apply. Slider UI deferred to B1 sidebar, which should surface these two settings) | — |
| B6 bone camera | open | — (diff vs `llcinematiccamera` first) |
| B7 poser | open | — (phased) |
| B8 cinematic camera mode | open | B6 |
| B9 gamepad flycam | open | — (compose with `llcameraoperator`) |
| B10 pose sync | open | B7 |
| B11 lookAt broadcast off | **done** (limit-distance half was already present) | — |
| B12 mouselook head scaling | **done** (experimental) | — |
| B13 EEP editor rework | open | — |
| C1 client AO | open | — |
| C2 movement conveniences | **done** (partial port — 2 of 3 sub-features already present) | — |
| C3 camera QoL | **done** (2 of 3 sub-features already present) | — |
| C4 click-to-walk hardening | **closed — already present via shared upstream** (2026-07-09: fly-click-walk early-return `lltoolpie.cpp:632` + `stopClickToWalk()` from `toggleFlying()`; HUD double-click guards `lltoolpie.cpp:660,754` + `mDoubleClickTimer`; alpha/particle immune via `pick_transparent=false`/`pick_particle=false` at `lltoolpie.cpp:597,607,742`. BD's fixes were 2011–2021 upstream LL commits both trees inherited; verified in current source, not just history) | — |
| C5 rebindable keys | **closed — no port** (PR-1; verified 2026-07-09: `llkeyconflict.cpp/h` + `key_bindings.xml` present, wired into preferences) | — |
| C6 snapshot conveniences | **done** | — |
| C7 texture channel lock | **closed — already present via shared upstream** (2026-07-09: "Synchronize materials" checkbox, `SyncMaterialSettings` + `syncOffsetX/Y`/`syncRepeatX/Y`/`syncMaterialRot` in `llpanelface.cpp`, upstream MAINT-3223 commit `69db7cd947` carried identically by BD and Alchemy; PBR/GLTF transforms untouched by sync in both trees, matching BD scope) | — |
| C8 slider precision | **done** | — |
| G5.0 memory prerequisites & fast wins | **done** (in-world validation owed: memory readout >16GB, VRAM detect, capture degrade check) | — |
| G5.1 decoded-texture RAM pool | open — Stage 1 measurement spike first | G5.0 |
| G5.2 texture pinning / capture mode | open | G5.1 (soft) |
| G5.3 scene warm-up / preload | open | G5.2 |

*(G5.x ids are from the 2026-07 top-tier spec sheet — "Alchemy Fork Spec Sheet
BD Graphics + FS QoL", memory/streaming phase for the RTX 5090 / 192GB target;
no A/B/C equivalent existed. G5.0 findings map to the companion "Alchemy Memory
Analysis" doc, findings 1/3/4/6.)*
