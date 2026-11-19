# Planned Shots ("Setups") — Shot Builder Rework

**Target:** Alchemy-Machinima viewer fork · Director Console
**Status:** Design proposal (Brainstorm item 6.1 — keystone "Setups")
**Grounded against:** `indra/newview/aldirectorswitcher.{h,cpp}`, `aldirectorswitchermodel.h`, `alpanelcinecamparams.{h,cpp}`, `llcinematiccamera.h`, `llfloaterdirector.cpp`

> Confirmed from code: `ALDirectorSwitcher::Slot = {bool mEnabled; S32 mMode; std::string mLabel}` — no params (the DRY limitation). Bank serialization `DirectorSwitcherBank` → `data["slots"][i] = {enabled, mode, label}`, read with `item.has()` guards (already additive-friendly). `ALPanelCineCamParams` is *entirely* settings-backed (every control → a `CinematicCam*` global), holds no per-instance state, and maintains `modeTable()` (per-mode manifest of which settings each mode reads) + `sharedSettings()`, plus a named-preset system (`presetsDir`/`writePreset`/`applyPreset`). 50 cine modes (append-only, `static_assert` guarded). `ALDirectorSwitcherModel`: 12 slots, deterministic seeded `Controller`.
> Caveat: the Vcam Gate (`llprismlens`) and the 3 Shot Builder buttons live on the working branch (`feature/cine-light-rig`), not the staged worktrees inspected — §4 Gate integration and §3.3 template mapping are designed against their described contracts (`gateArm`/`gateTake`/`addVirtualCamera`, `MAX_CAPTURES=8`), while everything about the switcher/model/params panel/serialization is confirmed.

---

## 1. Problem & thesis

Today's Shot Builder = one-shot buttons (OTS/Zolly/Hero-Arc) that flip `LLCinematicCamera` into a mode with a few inline params. It exposes *far less* than the full `ALPanelCineCamParams`. The switcher can only cut between **modes**, and its own tooltip admits the DRY hole: two "Orbit" slots share the *global* orbit params, because a `Slot` stores only `{mEnabled, mMode, mLabel}` and every param lives in `gSavedSettings`.

**Thesis:** A "Shot" (**Setup**) should be a first-class, fully-configured, named, recallable object capturing the *entire* cine configuration. The Shot Builder becomes its editor (reusing `ALPanelCineCamParams` verbatim); the switcher becomes the *player* that cuts between Shots. No parallel param system, no dumbed-down subset.

**Load-bearing insight:** `ALPanelCineCamParams` is 100% settings-backed and *already maintains the manifest* of which `CinematicCam*` settings each mode reads (`modeTable()[mode].mSettings` + `sharedSettings()`). So "a Shot's params" is precisely *"snapshot the value of every setting named in that manifest for this mode."* Reuse that manifest as the serialization schema — don't hand-enumerate params.

---

## 2. Data model

### 2.1 The `Shot` (Setup)
```
Shot {
  id:        LLUUID            // stable identity for reorder/dup/reference
  label:     string (<=40)     // reuses switcher's 40-char truncation
  mode:      S32               // LLCinematicCamera::EMode (0..50), sanitized
  params:    LLSD map          // { "CinematicCamOrbitRadius": 3.5, ... }
                               //   = snapshot of modeTable()[mode].mSettings + sharedSettings()
  subjects:  { A,B,C,D: LLUUID }   // subject bindings (A/B today; C/D reserved)
  lens:      { fov / preset }      // if lens lives outside CinematicCam*, else in params
  operator:  string (preset name)  // shake/handheld operator preset ref
  // --- optional bundles (Phase 3), independently present/absent ---
  lightRig:  string            // ALCineLightRig setup name (by-reference)
  gaze:      LLSD              // gaze/look-at refs
  schema:    S32 = 1
}
```
Rules: **`params` is a manifest snapshot**, built by iterating `modeTable()` for `mode` (+ `sharedSettings()`) and reading each named control — stores exactly what the mode reads, auto-tracks future params. **Bundles by-reference** where a mature preset system exists (light rigs = `ALCineLightRig` setups by name; operator = preset name) — keeps Shots small and lets "re-light setup 4" propagate. **Subjects are subject-relative**, surviving cast movement.

### 2.2 Where Shots live: the Setup Library
One owner: **`ALShotLibrary`** (viewer-scope singleton, mirrors `ALCineLightRig`'s master-setup pattern). Owns the ordered Shot list + persistence — the single source of truth. Slots and Gate arms **reference** library Shots by `id`; they never own config.

### 2.3 Serialization (additive, back-compat)
- **Shot Library** — `shots/*.xml` file-per-shot (mirrors `presetsDir()`; diff/share-friendly).
- **Switcher bank** — extend `DirectorSwitcherBank` additively with one optional field `data["slots"][i]["shot"] = "<uuid>"`. Old viewers ignore it (read `mode`+globals → legacy behavior). New viewers: if `shot` resolves, it wins; else fall back to `mode`. **`mode` stays denormalized** so downgrade never loses cut structure. No enum moves; `static_assert`s stay valid.

---

## 3. Authoring UX — Shot Builder becomes a real editor

```
┌ Shot Builder ───────────────────────────────┐
│ [Shot list]            [ + Capture current ] │
│  ▸ 01 Wide master       [ Templates ▾ ]      │
│  ▸ 02 OTS on B          [ Dup ] [ Del ] [↑↓] │
│  ▸ 03 Hero arc  ● ON-AIR                      │
│ ───────────────────────────────────────────  │
│  Editing: "02 OTS on B"                       │
│  ┌ ALPanelCineCamParams (THE SAME PANEL) ──┐  │
│  │  mode combo + auto-hiding mode panel +   │  │
│  │  shared header + per-control resets      │  │
│  └──────────────────────────────────────────┘ │
│  Subjects: A[▾] B[▾] C[▾] D[▾]                 │
│  Light rig: [none ▾]   Operator: [handheld ▾] │
│  [ Save ]  [ Arm → slot ▾ ]  [ Preview ]      │
└──────────────────────────────────────────────┘
```

**Editing model — reuse `ALPanelCineCamParams` unchanged** via a live scratch binding:
1. **Select a Shot** → *hydrate*: push `Shot.params` into the `CinematicCam*` globals, set mode combo. Panel shows the Shot's real values.
2. **Edit** → user tweaks the panel as today; globals change live, render camera previews live.
3. **Save** → *harvest*: re-snapshot the manifest for the current mode back into `Shot.params`.

Zero new param widgets — the full config is available because it *is* the same panel. **Interaction guard:** checkpoint the operator's own global values on editor entry and restore on exit (sandboxed), so opening the editor doesn't silently overwrite the live Cine Camera controls.

**Core actions:** Capture-current-as-Shot (fastest path); **Templates ▾** (OTS/Zolly/Hero-Arc survive as *seeds* that create a new editable Shot + drop into the editor — old click-behavior remains as Preview, so no lost muscle memory); Name/Save/Duplicate/Delete/Reorder (Duplicate = the coverage primitive); Arm→slot; Preview (solo on render camera without arming).

---

## 4. Switcher integration — recommended architecture

**Keep the two existing layers; add the library as a shared *catalog* both reference. Do NOT build a third control layer.**
```
        ┌─────────────────────────────────────────┐
        │   ALShotLibrary  (Setups: the WHAT)      │  ← config lives here, once
        └───────────────┬───────────────┬──────────┘
        references by id │               │ references by id
     ┌──────────────────▼──┐        ┌────▼───────────────────┐
     │ ALDirectorSwitcher   │        │ Vcam Gate (llprismlens)│
     │ 12 Slots (the WHEN,  │        │ 8 arms (the WHERE-TO — │
     │ drives RENDER camera)│        │ routes virtual cams)   │
     └──────────────────────┘        └────────────────────────┘
```
- **`ALShotLibrary` = the WHAT.** Full config, owned once.
- **`ALDirectorSwitcher` = the WHEN + drives the render camera.** A Slot is a scheduling cell ("at this beat, be Shot X"); it already owns timing/determinism (`presentation_time`, `cutSerial`, seeded `Controller`) and the CUT lease. Gains one optional `shot` id.
- **Vcam Gate = the WHERE-TO** (routing/record of virtual cameras). A Gate arm that wants a planned framing references a Shot id; it does not learn cine params, it cites a Shot.

A Shot can be referenced from a switcher slot **and/or** a gate arm, but its config exists in exactly one place. Neither existing layer owns config; the thin catalog does.

**Activation:** on cut into slot *i*, if `bank[i].shot` resolves → **hydrate** that Shot's `params`/subjects into globals *before* `LLCinematicCamera::updateCamera()` reads them (this is the exact fix for the shared-param DRY hole — two Orbit slots hydrate different radii). Denormalize `Shot.mode → mActiveMode` so status/serial/ease logic is unchanged. Phase-3 bundles (rig/operator/gaze) apply on cut by name. Hydration is a pure function of (Shot, cut serial), inside the existing frozen-`presentation_time` tick — determinism preserved.

**Manual TAKE / auto-cycle / on-air-next** already exist in the model — relabel around Shots: TAKE = `punch(slot)`; auto-cycle = existing `Config.mAuto`/`mSequence`/`mIntervalSeconds`/seed over enabled slots; on-air = `activeSlot()`, next = `selectSequenceSlot(eventIndex+1)` (display-only).

---

## 5. Migration — no broken scenes
1. Existing `DirectorSwitcherBank` loads unchanged; slots with no `shot` behave as today. Library starts empty.
2. The 3 one-shot buttons → template seeds; their old fire-now behavior remains as **Preview**.
3. First Capture/Arm upgrades in place additively (write `shot`, keep `mode` denormalized) — downgraded viewer still cuts correctly.
4. Optional inline-on-export (`slots[i]["params"]`) so recipients without the library file still get per-slot params; by-reference stays the default.
5. No enum churn; `static_assert`s protect legacy values.

---

## 6. Phased plan
**Phase 1 — "Less dumb":** add `ALShotLibrary` (capture/save/load/reorder/dup/delete, `shots/*.xml`); Shot editor = embed existing `ALPanelCineCamParams` with hydrate/harvest bracketing (no new widgets); `Shot.params` = manifest snapshot; extend `Slot` with optional `shot` + hydrate-on-cut in `applySlot()`; templates reskin OTS/Zolly/Hero-Arc as seeds. **Outcome: two same-mode slots hold different params (headline DRY fix); any angle is a named, recallable, armable Shot.**
- Risks/decisions: hydrate/restore must checkpoint+restore live globals (recommend sandboxed editor); add a debug assert that every `CinematicCam*` global a mode reads appears in its manifest (so future params can't escape capture).

**Phase 2 — Switcher/Gate as Shot players:** on-air/next in the list; Arm-to-slot / Arm-to-Gate-arm; auto-cycle relabeled around Shots; Gate arm gains optional `shot`. Decision: switcher stays authoritative for the render camera; Gate references Shots only for routing/record metadata (avoid two things fighting for the render lease).

**Phase 3 — Bundling & coverage:** light-rig by-name bundle applied on cut, operator/shake preset bundle, gaze refs; **coverage generator** (from one master Shot auto-derive master/OTS-A/OTS-B/CU via dup + subject swap); import/export/share + inline-on-export.

**Cross-cutting risks:** determinism (every apply-step inside the frozen-`presentation_time` tick, no new clocks); reference integrity (a deleted Shot still referenced → fall back to denormalized `mode`, warn, never hard-fail a live cut); editor scope creep (resist a second param surface — the win is one panel).

---

## 7. DRY checklist (what to build)
| Need | Reuse (don't reinvent) |
|---|---|
| Full param editing | `ALPanelCineCamParams` embedded verbatim |
| Which params a mode has | `modeTable()` + `sharedSettings()` manifest |
| Named recallable objects | `ALCineLightRig` master-setup / preset UX pattern |
| Per-shot storage & back-compat | additive `DirectorSwitcherBank` LLSD (`item.has()` guards present) |
| Timing / determinism / cut lease | `ALDirectorSwitcherModel::Controller` + `presentation_time` unchanged |
| Cutting between shots | `ALDirectorSwitcher` Slots (+ one `shot` field) |
| Multi-cam routing/record | Vcam Gate arms (+ one `shot` reference) |
| Light bundle | `ALCineLightRig` by-name |

**One new component:** `ALShotLibrary` (the catalog). Everything else is a reference field or a UI shell around existing systems.
