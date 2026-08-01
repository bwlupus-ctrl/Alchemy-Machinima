# Director Camera Switcher — Deep-Research + Codegen Brief

**Fork:** Alchemy-Machinima (`I:\alchemy-machinima`, branch `develop`).
**Type:** research + no-build codegen brief. Produce (a) a research writeup and (b) a git-applicable patch. **Do NOT build.** After delivery it goes through the usual Codex + Opus + Fable adversarial review, then Claude builds.
**Doubles as:** the fork's first consolidated *camera-system* paper (Part 1 documents the whole camera stack; Part 2 designs the feature on top).

---

## 0. What the user asked for

A **broadcast-style camera switcher** ("vision mixer") for the Director:

> "choose a programmed camera angle, or switch to a random angle (head / shoulder / wide / tight / etc.) like an actual broadcaster switcher."

Confirmed design decisions (user):
1. **Shot-bank slots hold BOTH** — each slot can be a **static framing** (Wide / Medium-shoulder / Close-head / Tight / OTS / Two-shot / Profile / Low-hero / High-angle / …) **OR** one of the existing 42 cinematic **motion modes**.
2. **Full auto-director** — manual punch (buttons + hotkeys) **and** an automatic switcher that cuts on a timed interval, with a choice of **random** selection or a **programmed sequence**, cutting only among *enabled* slots, and **presentation-time deterministic** so a machinima take replays identically.
3. **Cut style** — **hard CUT by default**, with an **optional quick ease** (~0.15–0.4 s pose glide) per-cut toggle.

Everything must be **default-off / disarmed** (zero behavior change until the user arms the switcher), reuse the existing camera infrastructure rather than reinventing it, and live in the **Director Console** (per the fork's Director-Console-superset rule), with a mirrored standalone entry point acceptable but optional.

---

## PART 1 — The existing camera stack (read this before designing)

All anchors verified in-tree. Trace them and expand as needed.

### 1.1 `LLCinematicCamera` — the automated cinematic camera (the spine)
`indra/newview/llcinematiccamera.{h,cpp}`. Singleton (`instance()`). Header fully documents it.

- **`enum EMode`** (`llcinematiccamera.h:38-85`) — **42 modes**, `MODE_OFF=0` … `MODE_BREATHING_HOLD=42`. Includes framing-relevant modes already: `MODE_OTS=10` (over-the-shoulder on the selected target), `MODE_TWO_SHOT=17` (perpendicular to the self↔target line), `MODE_ECU_EYES=19` (locked face micro-frame, narrow lens), `MODE_LONG_LENS=20`, `MODE_PEDESTAL=22`, plus a full vocabulary of motion patterns (orbit, sweep, crane, dolly-zoom, push-in, arc, reveal, pull-back, lead-follow, spiral, and the acrobatic/dance set 23-42).
- **Activation / takeover:** `isActive()` returns true when the system should own the render camera this frame; `updateCamera()` computes and writes the frame's camera and is called from the idle camera dispatch **instead of** `gAgentCamera.updateCamera()`. Gating cached controls: `CinematicCamEnabled` (bool, default false) + `CinematicCamMode` (S32, default 1) — see `llcinematiccamera.cpp:157-169,1169`.
- **Targeting:** `resolveTarget()` → own avatar, or the **selected** avatar when `CinematicCamUseSelected` (`:200`), or a session **locked follow** subject via `toggleFollowTarget(id)` / `isFollowTarget(id)` (right-click avatar → "Cinematic Cam Follow"), which beats selection while set. `CinematicCamJoint` (default `mHead`) is the anchor joint. `resolveAnchor(pos,rot,level_horizon)` exposes the resolved target pose for external riders (Flycam Orbit already uses it).
- **Operator routing:** when `CinematicCamUseOperator` (`:1173`), the pattern output is routed through `LLCameraOperator` for a handheld texture on top (`:1358-1384`); reset on mode change (`:1196,1356`).
- **Smoothing / state:** `mSmPos`/`mSmRot` (smoothed pose), `mPhase` (wrapped pattern clock), `mLastMode`, `mTripodPos` (captured at activation for zoom modes), fresh-activation phase reset via `mLastUpdateFrame`. **The 0.25 s ease should reuse/extend this smoothed-pose path.**
- **Speed / clock:** pattern phase advances by a speed control (grep `CinematicCamSpeed`). For determinism see §1.6.

### 1.2 The idle camera dispatch (where control is handed over)
`indra/newview/llagentcamera.cpp`. `mCinematicCamera = gSavedSettings.getBOOL("UseCinematicCamera")` (`:226`); the dispatch branch that yields the frame to the cinematic system is at **`:1599` and `:1639`** (`else if (mCinematicCamera) …`).
> ⚠️ **Naming nuance to handle:** the *dispatch gate* is `UseCinematicCamera` (llagentcamera) while `LLCinematicCamera::isActive()` keys off `CinematicCamEnabled` (llcinematiccamera). Confirm the exact relationship (one may mirror the other). The switcher, when armed, must ensure BOTH the dispatch gate and the cinematic enable are satisfied so its chosen mode actually renders — do not add a competing high-priority branch unless necessary.

### 1.3 `LLCameraOperator` — handheld texture layer
`indra/newview/llcameraoperator.{h,cpp}` + `alpanelcinecamparams.{h,cpp}`. Fixed-timestep (F64 120 Hz) locomotion/handheld operator (6 modes, per-DOF gains). Already integrates with the cinematic camera via `CinematicCamUseOperator`. The switcher does not need to change it — just preserve the routing.

### 1.4 Flycam recorder + orbit
`FLYCAM_RECORDER_*` docs; `Flycam*` settings. Records/plays/scrubs camera paths; Flycam Orbit rides `resolveAnchor()`. Out of scope for the switcher, but the switcher must **coexist** (don't break recorder playback; ideally a recorded take can capture switcher cuts — note as a stretch goal, not v1).

### 1.5 Director Console — the UI host
`indra/newview/llfloaterdirector.cpp` + `skins/default/xui/en/floater_director.xml`; embedded shared panel `ALPanelCineCamParams` (`alpanelcinecamparams.*`).
- **Camera tab exists:** `TAB_ICON_CAMERA="Command_View_Icon"` (`:67`), `{ "camera_tab", TAB_ICON_CAMERA }` (`:173`), the "---- Camera tab ----" block (`:305`), `mCineCamPanel = findChild<ALPanelCineCamParams>("cinecam_params_embedded")` (`:318`), `refreshCameraTab()` (`:385`).
- **Mode→label map:** `cinecam_mode_name(S32 mode)` (`:101`) — matches the mode-combo labels; extend it if you add static-framing modes.
- **Scene-save system:** the Director saves a whole setup (cast, marks, loco anims, subjects, **arming**, camera and move parameters) as a named scene. The saved-control set is a list (`llfloaterdirector.cpp:~549-587`, includes `DirectorArmCamera`, `TemporalDriveCamera`, `TemporalDriveMove`, …). **Note (`:549-555`): `CinematicCamMode` is deliberately NOT saved with the scene — it is stored separately.** Decide how the switcher's bank fits scene-save (recommend: the bank config + auto settings save WITH the scene so a scene restores its multi-cam setup, but the *live active slot* is transient like `CinematicCamMode`).
- **Arming:** `DirectorArmCamera` (default true, `:445`) gates whether "Go" fires the camera. The switcher should participate in arming.
- **Superset rule:** anything editable in a standalone switcher floater MUST also be in the Director Console; prefer a shared `LLPanel` mirrored into both (see `director-console-superset-rule`).

### 1.6 Determinism substrate (reuse it — do not roll your own clock/RNG)
The fork's machinima determinism is **presentation-time driven**: `LLPresentationTime::currentFrame().presentation_time` (the same clock the Temporal-Capture world-time-scale and weather use). `ALWeatherModel` (`indra/newview/alweathermodel.{h,cpp}`) is the canonical example of a **deterministic, presentation-time, seeded** scheduler with a stateless hash "visual lane" (SplitMix64) — **model the auto-director's switch schedule + random picks on it** so a take replays bit-identically across runs/machines. Do NOT use wall-clock or `gFrameCount`.

### 1.7 Hotkeys
`doc/DIRECTOR_HOTKEYS.md` + the hotkey dispatch in the director. The switcher wants number-key punches (1-9) for cameras; integrate with the existing director hotkey path rather than a parallel handler.

---

## PART 2 — The feature: Director Camera Switcher

### 2.1 Concept
A **shot bank** of N slots (recommend **12**; hotkeys 1-9 cover the first nine, with UI buttons for the rest). Each slot is a **camera** the director can **punch** (cut to) manually, or that the **auto-director** cuts to on a schedule. A slot is either:
- a **STATIC framing** — a subject-relative fixed camera pose + lens (snap-and-hold, with optional subtle "breathing" life), or
- a **MOTION mode** — one of the existing 42 `LLCinematicCamera::EMode` patterns.

Arming the switcher makes it OWN mode selection: on each cut it sets the active framing/mode (and target), and the existing cinematic-camera pipeline renders it. Cuts are **hard by default**, with an optional short **ease** (pose glide via the existing smoothed-pose path).

### 2.2 Static framing catalog (the "angles")
Define these as **subject-relative** placements resolved from the target's `resolveAnchor()` (or head/pelvis joints), each = { azimuth offset, elevation/pitch, distance, height offset, FOV/lens, aim point }. Reuse the math already in `patternOTS`/`patternTwoShot`/`patternECU`/`patternPedestal` where possible. Suggested defaults (GPT to refine, expose as tunables):

| Shot | Frames | Distance | Aim | Lens/FOV | Notes |
|---|---|---|---|---|---|
| **Wide** | full body + environment | far | mid-torso | wide | establishing |
| **Medium** (shoulder) | waist/chest up | mid | upper chest | normal | the "interview" shot |
| **Close** (head) | head + shoulders | near | head | normal-long | the workhorse |
| **Tight** (ECU) | face | very near | eyes | long/narrow | reuse `MODE_ECU_EYES` geometry |
| **OTS** | over target's shoulder onto subject | mid | subject face | normal | reuse `patternOTS`; needs a second party |
| **Two-shot** | both subject + target | mid-far | midpoint | normal-wide | reuse `patternTwoShot` |
| **Profile L/R** | side silhouette | mid | head | normal | 90° azimuth |
| **Low-hero** | up at subject | near-mid | face/chest | slightly wide | power angle |
| **High-angle** | down at subject | mid | head | normal | diminish angle |
| **Full/boots-to-head** | whole figure vertical | mid | center mass | normal | fashion/reveal |

Static shots hold position (optionally a tiny `MODE_BREATHING_HOLD`-style life so they're not sterile). Handle the **no-second-party** case for OTS/Two-shot (fall back to Close, or disable the slot) — fail-soft.

### 2.3 Switcher engine
New module, recommend `indra/newview/aldirectorswitcher.{h,cpp}` (renderer-independent, reads settings, no direct GL), mirroring `ALWeatherModel`'s shape:
- **State:** the bank (N slots), the live active slot, previous slot (for ease), the auto clock, the seeded RNG lane.
- **`punch(slot)`** — manual cut: set the active slot; if MOTION, set `CinematicCamMode` to that mode; if STATIC, set a new static-framing mode + framing id (see §2.5); capture the pre-cut resolved pose for the ease; reset the pattern phase appropriately.
- **Auto-director** — when auto is on: compute the next switch time from `presentation_time` + interval (± deterministic jitter from the seeded lane); at each boundary pick the next *enabled* slot by **random** (seeded, no immediate-repeat) or **sequence** (wrap through enabled slots); call the same cut path. Fully deterministic given (seed, interval, enabled-set).
- **Ease** — when enabled, blend the resolved camera pose from the previous slot to the new one over the ease duration using the existing `mSmPos/mSmRot` smoothing (extend it with a one-shot ease envelope); hard cut when disabled or duration 0.
- **Target** — reuse `resolveTarget()`/follow/`UseSelected`; the whole bank shares the current subject (a switcher is one subject filmed from many angles). Optional per-slot target override = stretch goal.

### 2.4 Integration (drive the cinematic camera, don't fight it)
When armed, the switcher ensures the cinematic camera owns the render frame (`UseCinematicCamera`/`CinematicCamEnabled` satisfied) and simply sets the active mode/framing + target each cut. `LLCinematicCamera::updateCamera()` then renders it exactly as today, including operator routing. Add a hook in `LLCinematicCamera` so a STATIC framing is a first-class mode (see §2.5). Confirm the switcher's ownership is transient and cleanly releases the camera when disarmed (restore stock `gAgentCamera`).

### 2.5 Static-framing modes in `LLCinematicCamera`
Add a small static-framing family to `EMode` (e.g. `MODE_SHOT_WIDE`, `MODE_SHOT_MEDIUM`, `MODE_SHOT_CLOSE`, `MODE_SHOT_TIGHT`, `MODE_SHOT_PROFILE_L/R`, `MODE_SHOT_LOW`, `MODE_SHOT_HIGH`, `MODE_SHOT_FULL`) OR a single `MODE_STATIC_SHOT` parameterized by a framing id. Reuse `MODE_OTS`/`MODE_TWO_SHOT`/`MODE_ECU_EYES` for those three shots. Each new mode = a `pattern*` that returns a fixed subject-relative pose (+ optional breathing). Extend `cinecam_mode_name()` and the mode combo. **Appending to the enum keeps existing `CinematicCamMode` integer values stable** (append at the end; don't renumber).

### 2.6 UI — Director Console "Switcher" section
Shared `LLPanel` (mirror into the Console Camera tab and, if desired, a standalone floater):
- **Shot-bank grid** — N labeled buttons (e.g. "1 Wide", "2 Medium", "3 Close", …); the **live** camera is highlighted; clicking punches a cut. Each button shows its assigned kind (static shot name or motion-mode name).
- **Per-slot config** — assign a slot to a STATIC framing (dropdown of the §2.2 catalog) OR a MOTION mode (dropdown reusing `cinecam_mode_name`), an enable checkbox (auto-director only cuts among enabled), and an editable label.
- **Auto-director controls** — Auto on/off; interval (seconds) + jitter; **Random | Sequence** radio; a seed field (or reuse the director seed).
- **Cut style** — "Ease cuts" toggle + duration slider (0–0.4 s).
- **Per-slider reset buttons** consistent with the weather panel convention (`Refresh_Off` glyph, a `*.ResetControl` callback).
- **Hotkeys** 1-9 punch cameras via the existing director hotkey path (`DIRECTOR_HOTKEYS`).

### 2.7 Settings (all `gSavedSettings`, default = disarmed / no behavior change)
- `DirectorSwitcherArmed` (bool, default **false**) — master; when false the switcher is fully inert.
- Per-slot (N slots): kind (static/motion), framing id or `CinematicCamMode`, enabled, label. Store compactly (e.g. an LLSD/string blob per bank, or indexed keys) — GPT to choose a clean scheme consistent with how the fork stores structured settings.
- `DirectorSwitcherAuto` (bool, default false), `DirectorSwitcherIntervalSec` (F32), `DirectorSwitcherJitterSec` (F32), `DirectorSwitcherSequence` (bool: false=random / true=sequence), `DirectorSwitcherSeed` (U32).
- `DirectorSwitcherEaseCuts` (bool, default false), `DirectorSwitcherEaseSec` (F32, default 0.25, clamp 0–0.4).
- Scene-save: add the bank + auto settings to the Director scene-save list (`llfloaterdirector.cpp:~549-587`); keep the **live active slot** transient (like `CinematicCamMode`).

### 2.8 Determinism (hard requirement)
The auto-director's switch times and random picks derive ONLY from `presentation_time`, the interval/jitter, the enabled set, and the seed — model on `ALWeatherModel` (SplitMix64 seeded lane, no wall-clock, no `gFrameCount`, no uninitialized/address-based state). A recorded/replayed take with the same scene must produce the **same cuts at the same times**. Provide a determinism unit test (two switcher instances, same seed/inputs → identical cut schedule) mirroring `alweathermodel_test.cpp`.

---

## 3. Invariants (do not violate — same discipline as the weather patches)
1. **Default-off / disarmed** — with `DirectorSwitcherArmed=false`, zero behavior change and near-zero cost (a couple of `LLCachedControl` reads at most). The stock camera and the existing cinematic camera behave exactly as before.
2. **Reuse, don't reinvent** — targeting (`resolveTarget`/follow/`UseSelected`), takeover (the llagentcamera dispatch), operator routing, and pose smoothing already exist. Drive the cinematic camera; do not add a second competing camera owner unless unavoidable, and if you do, document why.
3. **Enum stability** — append new `EMode` values at the end; never renumber existing ones (`CinematicCamMode` is a persisted integer and is referenced by the Director scene system).
4. **Presentation-time deterministic** — see §2.8.
5. **Fail-soft** — invalid slot, missing target, OTS/two-shot with no second party, disarm mid-cut → safe no-op / graceful fallback, never a crash or a stuck camera. Disarming cleanly restores stock `gAgentCamera`.
6. **Director-Console superset** — shared panel mirrored into the Console; any standalone floater duplicates nothing.
7. **Client-only**, no network, no new assets, **no shaders / no reserved samplers** (this is camera math only).
8. **No regression** to the cinematic camera, camera operator, flycam recorder, agent camera, or the Director scene-save/arming flow.

## 4. Deliverables
1. **Research writeup** — confirm/expand Part 1 (the camera-stack map, with any anchors I missed: exact dispatch relationship of `UseCinematicCamera` vs `CinematicCamEnabled`, the scene-save list contents, the hotkey path), and finalize the static-framing geometry.
2. **Git-applicable patch** (no build) — new `aldirectorswitcher.{h,cpp}`, `EMode`/pattern additions in `llcinematiccamera.*`, `cinecam_mode_name` + mode-combo updates, the shared switcher panel + Console mounting + XML, settings.xml additions (all default-disarmed), scene-save wiring, hotkey wiring, and a determinism unit test.
3. Cite **file:line** for every integration point touched. State clearly what is new vs modified. Confirm the default-disarmed byte-identical-behavior claim.

## Appendix A — `LLCinematicCamera::EMode` (current, from `llcinematiccamera.h:38-85`)
`MODE_OFF=0, MODE_BONE_LOCK=1, MODE_ORBIT=2, MODE_FLY_HOVER=3, MODE_SWEEP=4, MODE_CRANE=5, MODE_DOLLY_ZOOM=6, MODE_PUSH_IN=7, MODE_LOW_HERO=8, MODE_OVERHEAD=9, MODE_OTS=10, MODE_CRASH_ZOOM=11, MODE_SLOW_ZOOM=12, MODE_WHIP_ARC=13, MODE_ARC=14, MODE_REVEAL=15, MODE_PULL_BACK=16, MODE_TWO_SHOT=17, MODE_LEAD_FOLLOW=18, MODE_ECU_EYES=19, MODE_LONG_LENS=20, MODE_SPIRAL=21, MODE_PEDESTAL=22, MODE_BARREL_ROLL=23 … MODE_BREATHING_HOLD=42.` (Append new static-shot modes at 43+.)

## Appendix B — key anchors
- Dispatch takeover: `llagentcamera.cpp:226` (`UseCinematicCamera`), `:1599`, `:1639`.
- Cinematic settings: `CinematicCamEnabled`, `CinematicCamMode` (default 1), `CinematicCamUseSelected`, `CinematicCamJoint` (`mHead`), `CinematicCamUseOperator`, `CinematicCamSpeed` — `llcinematiccamera.cpp:157-257,1169,1173`.
- Director Console camera tab: `llfloaterdirector.cpp:67,101,173,305,318,385,445`; scene-save list `:~549-587`; `ALPanelCineCamParams` = `alpanelcinecamparams.*`, embedded as `cinecam_params_embedded`.
- Determinism reference: `alweathermodel.{h,cpp}` + `LLPresentationTime`.
- Hotkeys: `doc/DIRECTOR_HOTKEYS.md`.
