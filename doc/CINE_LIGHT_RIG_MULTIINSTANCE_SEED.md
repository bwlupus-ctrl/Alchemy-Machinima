# Cine Light Rig — Multi-Instance (per-subject independent rigs) — design seed

**Status:** seed for a design pass. No source modified. Captured 2026-08-16.
Feeds `doc/CINE_LIGHT_RIG_MULTIINSTANCE_DESIGN.md` -> Codex -> Opus review -> build.

The Cinematic Light Rig (base + scale + mirror + enhancements + track-mode + master presets +
gobos + **multi-anchor group-as-unit**) is built, Opus-reviewed, and now BUILT (Alchemy Viewer
26.2.0.63033, branch `feature/cine-light-rig`, baseline commit `d12fa7a91e`). This adds **multiple
concurrent rig instances — one independent rig per Director cast slot** — so several avatars can be
lit at once, each with its own controls. Motivated by the multi-avatar / virtual-cam workflow: today
the rig is a **singleton**, so pointing it at a second avatar just moves the one shared rig off the
first, and you cannot light two subjects differently at the same time.

## User decisions (2026-08-16, fixed)
1. **Concurrent per-subject rigs.** Every subject can be lit at the SAME time, each an independent
   rig with its own parameters. Not "one retargetable rig."
2. **Edit-selection via the existing anchor dropdown.** The rigs run concurrently; a single panel
   edits ONE at a time. The `cine_anchor` combo becomes the **editor selector** — choosing a slot
   re-binds the whole (unchanged) panel to that slot's rig. "Using the anchor drop-down would be a
   great way to choose which rig to edit."
3. **One instance per Director cast slot, max 5.** {You, Subject A, B, C, D}. The instance's anchor
   is IMPLICIT (the slot's avatar); count is bounded to 5 by construction.
4. **Camera-focused rig gets shadow priority.** With the hard `MAX_SPOT_SHADOWS = 6` cap, the rig on
   the subject the camera is on casts shadows; other rigs stay lit but shadowless. Must never exceed 6.

## Claude decision (pre-committed, user may veto)
- **The group multi-anchor stays as a per-instance MODE.** An instance can light its own single
  subject OR (group mode) span several slots as one unit. Multi-instance sits on top of it; it does
  not replace it. The group-vs-instance overlap (double-lighting a shared member) is addressed in
  "open questions" below.

## Verified current model (read from code — cite file:line in the design)
- **Singleton.** `static ALCineLightRig& instance();` (`alcinelightrig.h:54`). One controller owns
  ONE emitter set (`mProjectors[LIGHT_COUNT]`, `mOmnis[LIGHT_COUNT]`, `alcinelightrig.h:132-135`),
  one `mAnchor` (`:138`), one group state (`mGroupEnabled`/`mGroupSlots`, `:139-140`), one FX/
  transition/smoothing block (`:142-169`).
- **All parameters are single GLOBAL `settings.xml` keys** — `CineLightRigEnabled`, `...Power`,
  `...Radius`, `...MasterEV`, `...MasterTempMired`, `...OffsetZ`, `...HeadroomStops`, `...Bounce*`,
  `...TransitionSec`, `...Damping`, `...TrackMode`, `...CookieUUID`, `...Seed`, `...Mirror`,
  `...OrbitYaw/Pitch`, `...FX`, `...ShadowMode`, `...Gizmo`, plus per-light `...Key/Fill/Rim/Bg` x
  {Yaw,Pitch,Profile,EV,Beam,Gobo} (`settings.xml`, ~40+ keys). `readSettings()`
  (`alcinelightrig.h:113`) pulls the ONE rig's params from these keys each tick.
- **The panel binds directly to those settings keys** (`control_name="CineLightRig..."` in
  `panel_cine_light_rig.xml`; reset buttons and combos in `alpanelcinelightrig.cpp`). So "the
  lighting is shared" is literal: one rig, one settings-backed parameter set, one panel.
- **The anchor combo** (`cine_anchor`) is populated with `"You"` (null UUID) + each cast member
  (`alpanelcinelightrig.cpp:311-317`); `onAnchorSelected()` calls `setAnchor()` (`:340-347`). It is
  DISABLED while the rig is enabled (`:386` `setEnabled(!enabled)`) — you cannot re-anchor mid-run.
- **tick()** drives the one rig each frame from `llviewerdisplay.cpp`. Emitter lifecycle, damping,
  transitions, FX all consume the single settings-backed state.
- **Cast roster:** `LLDirectorCast` owns {You, A, B, C, D}; `resolve(uuid)` -> `LLVOAvatar*`.
- **Scene round-trip:** `sceneData()` / `applySceneData()` serialize the one rig; the frozen
  three-way split reads `data["anchor"]` plus the additive `anchor_group`/`anchor_group_slots` keys
  (`alcinelightrig.cpp:1671-1697`), version stays 1.
- **Budget reality:** `MAX_SPOT_SHADOWS = 6` (compile-time, `pipeline.h`). Each instance can create
  up to 8 emitters (4 projectors + 4 omnis); 5 instances = up to 40 local lights and 20 potential
  spot-shadow casters against a hard cap of 6.

## What the design pass must decide

### A. The instance manager & state model (the core)
- Introduce a **manager** owning up to 5 `ALCineLightRig` instances keyed by cast slot {You,A,B,C,D}.
  `instance()` singleton is retired or reframed as the manager. Decide the class shape:
  `ALCineLightRigManager` owning `ALCineLightRig mInstances[5]`, each instance holding its OWN
  params blob + emitter set + transition/smoothing state. Each instance's anchor is its slot.
- **State-vs-settings (RECOMMEND, justify):** keep the ~40 `settings.xml` keys as the **editing
  buffer for the SELECTED instance only**. The panel keeps binding to those keys UNCHANGED. On a
  dropdown switch: flush the settings keys into the OUTGOING instance's blob, load the INCOMING
  instance's blob into the settings keys, refresh the panel. tick(): every instance ticks from its
  OWN blob; only the selected instance's blob mirrors the live settings keys. This localizes the
  change (finalized panel + its settings bindings untouched) — the alternative (rebind the whole
  panel to the manager, drop settings keys) is a large UI rewrite; reject it unless there is a reason.
- **Cross-session persistence:** settings.xml stores only the selected instance's buffer. The other
  4 instances' blobs must persist somewhere: (a) a per-instance LLSD store file; (b) one new
  settings key holding an LLSD array of the 5 blobs; (c) scene-only (non-selected instances reset on
  restart). Decide and justify. Prefer no schema churn / forward-compat.
- **Which instances are ACTIVE:** each instance has its own enable/power (default: only "You"
  enabled on first run; others off until turned on). Only ENABLED instances create emitters and tick
  live. Zero enabled = no emitters (today's dark path). Confirm an instance with its slot's avatar
  unresolved goes dark without disturbing the others.

### B. Budget & shadow priority (the hard constraint)
- **Never exceed `MAX_SPOT_SHADOWS = 6.`** Define "camera-focused instance": the instance whose slot
  avatar is the current camera focus / nearest the camera look-at (or the VCam-framed subject).
  Specify the exact selection rule and its fallback (e.g. the selected-for-edit instance). That
  instance's projectors get shadow slots first; remaining slots (if any) fill by a deterministic
  order (slot order You->A->..); all others run **shadowless but lit**. Must be stable (no per-frame
  thrash/pop) — hysteresis or a stable ordering.
- **Total light budget:** 5x8 = up to 40 emitters. Confirm the existing `bdmerge`/`calcNearbyLights`
  rig-light exemptions still hold at this count, and decide whether a soft cap on concurrently-active
  instances (or on emitters per instance) is needed. Quantify the worst case.
- Per-instance FX/transition/damping is fine (already per-controller fields, just x5).

### C. UI — anchor dropdown as editor selector
- `cine_anchor` combo: from "who the one rig lights" to "which slot's rig I'm editing." Selecting a
  slot re-binds the panel (flush/load per A). It must ALWAYS be usable (drop the `setEnabled(!enabled)`
  lock at `:386`; each instance has its own enable now). Show which slots are ACTIVE/lit (e.g. a
  marker in the combo row or a status line). Per-instance enable/power lives in the panel and edits
  the SELECTED instance.
- Everything else in the finalized panel (presets, gobos, master temp, track-mode, reset buttons,
  group multi-anchor controls, FX) now edits the SELECTED instance — no layout change required if the
  buffer model (A) holds. Confirm the Director console tab AND the standalone floater both edit the
  selected instance in lockstep (they host the same panel).
- Decide the group-mode presentation: group controls are per selected instance; a member covered by
  a group instance AND running its own instance is double-lit — RECOMMEND: allow it (user's choice),
  optionally surface a warning; do NOT auto-suppress in v1 (note as a deferred nicety).

### D. Scene serialization (backward-compat)
- `sceneData()`/`applySceneData()` serialize a LIST of up to 5 instance blobs + the selected slot.
  **Migration:** an OLD single-rig scene (`data["anchor"]` + legacy keys) loads into the instance
  for THAT anchor's slot; the other 4 default-off. NEW scene -> OLD binary: keep WRITING the legacy
  single-rig block for the selected/primary instance so old viewers still load something sane; add
  an `instances` array read only by new binaries (additive, after the frozen three-way split). Decide
  whether this stays version 1 (additive) or needs a bump; prefer additive/no-bump. Load-test all
  four cross-version combinations.

### E. Interaction with the FINALS (must not disturb per-rig behaviour)
- This is a MULTIPLICATION of instances, NOT a change to per-rig math. Each instance runs the
  finalized model verbatim: `scaledPoint` foot-pivot, SA-9 exposure invariance, mirror reflection,
  master colour-temp gain, gobos, track-mode, group multi-anchor — ALL unchanged per instance.
- The SINGLE-instance / "You"-only case (feature effectively untouched) MUST be bitwise-identical to
  today: one enabled instance == today's singleton behaviour. This is the delivery's ship-risk —
  demand a structural fast path, not "equivalent."

## Constraints
- Label PROVES/IMPLIES/INFERS; cite file:line. No source modified by the design.
- Ship-whole: manager + per-instance state + selector UI + budget/shadow policy + scene in one
  delivery. Defer nothing silently (group auto-suppress may be explicitly deferred).
- OFF-LIMITS: the finalized per-rig model (scaledPoint body, exposure/EV chain, mirror reflection,
  master-temp gain, gobo path, track-mode, group math); pipeline/shaders/llprimitive (the 6-slot cap
  and 20 m reach are design inputs); `lldirectorcast.*` (consumed read-only). No bump if achievable.
- Forward-facing robustness: 0 enabled instances; a slot's avatar leaving mid-shot; 5 differently-
  scaled subjects (0.05x clone + 1.0 avatar) each with its own rig; an old single-rig scene; the
  camera moving between subjects (shadow-priority hand-off must not pop).
- Concrete enough to become a Codex brief: named classes/fields/functions/settings/UI + OFF-LIMITS.
