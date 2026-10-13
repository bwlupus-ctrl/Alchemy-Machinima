# Director v2 QoL ideas (Codex, 10/13)

Already picked and in progress: changed-from-default markers, Quick (favorites) page, copy/paste between items, right-click row menu.

## Navigation

1. **Fuzzy search with a full, grouped results view** — tolerate typos and let directors browse every match instead of only the best 12.
   Evidence: search is prefix-only (`alkitsearch.cpp:183-208`); elsewhere results are capped at 12 (`alkitshell.cpp:1028-1059`). Value **4/5**; effort **M** — `alkitsearch.*`, `alkitshell.*`; risk: fuzzy matches may feel noisy.

2. **Back/Forward navigation history** — traverse recently visited pages and search hits without reopening the rail hierarchy.
   Evidence: eight groups contain 34 leaf pages (`floater_director_v2.xml:34-101`), while search can jump between them (`alkitshell.cpp:1069-1080`). Value **4/5**; effort **S** — `alkitshell.*`, `llfloaterdirector.cpp`; risk: distinguish history from planned last-page restoration.

3. **Cast roster filter and group folding** — search a large cast by name/group and collapse unrelated groups.
   Evidence: the roster is an unfiltered five-column scroll list (`floater_director_v2.xml:15-21`); its authored group selector is always hidden by layout code (`alkitshell.cpp:546-549`). Value **4/5**; effort **M** — `floater_director_v2` source data, `alkitshell.*`, `llfloaterdirector.cpp`; risk: preserve multi-selection while filtering.

4. **Docked sidecar for “full editor” pages** — open Diopter, Practicals, Actor Mask, Light Cues, and Gaze Cues inside Director instead of spawning workflow-breaking floaters.
   Evidence: Light/Gaze Cues are launcher-only cards (`panel_dpage_light_cues.xml:6-9`, `panel_dpage_performance_gaze_cues.xml:6-9`); Practicals is also a launcher (`panel_dpage_light_practicals.xml:6-9`); Diopter’s summary overstates its inline controls (`WIRING_AUDIT_look_lens.md:15`). Value **4/5**; effort **L** — kit page sources, `llfloaterdirector.cpp`, editor panels; risk: duplicate ownership and cramped layouts.

## Editing speed

5. **Fine/coarse modifiers and relative arithmetic for numeric controls** — Shift/Alt nudges plus entries such as `+0.5`, `-10%`, or `*2`.
   Evidence: the keyboard legend only says “Arrows: adjust” (`floater_director_v2.xml:166-176`), while shot building repeats tightly ranged spinners (`panel_dpage_camera_moves.xml:11-28`). Value **4/5**; effort **M** — `alkitrow.*`, spinner/slider helpers; risk: locale parsing and accidental large changes.

6. **Named Undo/Redo tooltips** — show “Undo Light › Key EV” instead of generic “Undo.”
   Evidence: history already exposes `undoLabel()`/`redoLabel()` (`alkithistory.h:58-61`), but buttons use fixed tooltips (`alkitpage.cpp:444-449`). Value **4/5**; effort **S** — `alkitpage.cpp`; risk: long labels need ellipsis.

7. **Bulk Subject A–D assignment** — assign ordered multi-selected cast members to A–D, or drag roster rows onto subject slots.
   Evidence: Shot repeats four separate “Set from selection” and Clear pairs (`panel_dpage_camera_shot.xml:8-27`). Value **4/5**; effort **S/M** — `llfloaterdirector.cpp`, Shot page source, cast drag payload; risk: ordering must be explicit.

8. **Gang/relative editing for rig lights** — multi-select Key/Fill/Rim/BG and adjust EV, warmth, yaw, or pitch together while preserving offsets.
   Evidence: Lights is a single-role selector (`panel_dpage_light_cine_light_rig.xml:233-236`) over four repeated role-specific control blocks (`:286-551`). Value **5/5**; effort **L** — `alkitcard.*`, `alpanelcinelightrig.*`, rig model; risk: clarify absolute versus relative edits.

9. **Preset provenance readout** — retain “Started from: Softbox,” then mark it “edited,” rather than snapping action presets back to an anonymous sentinel.
   Evidence: flare/night presets deliberately return to “Choose preset…” after applying (`alpanelcinelightrig.cpp:1116-1158,1884-1934`). Value **3/5**; effort **M** — `alpanelcinelightrig.*`, kit card/page sources; risk: provenance becomes stale after external setting changes.

## Feedback/visibility

10. **Persistent Director event log** — keep recent successes, refusals, warnings, cue fires, scene operations, and their timestamps.
    Evidence: VCam’s action row is normally hidden (`panel_dpage_camera_vcam.xml:17`) and every message disappears after six seconds (`alpaneldirectorvcam.cpp:264-268`). Value **4/5**; effort **M** — `alkitshell.*`, `alpaneldirectorvcam.cpp`, `llfloaterdirector.cpp`; risk: avoid notification spam.

11. **Per-control “why no effect?” diagnostics with Fix buttons** — explain gates such as HDR, shadow capacity, masking, missing targets, or disabled masters at the affected row.
    Evidence: gate banners cover only a few hard-coded pages (`alkitpage.cpp:567-597`), while Light Rig separately diagnoses shadow-slot and policy conflicts (`alpanelcinelightrig.cpp:2657-2708`). Value **5/5**; effort **L** — `alkitrow/card/page.*`, feature panels; risk: dependency rules can drift.

12. **Universal target pin** — let Animate, Gaze, Actor FX, Path, and similar pages lock their actor instead of silently following roster selection.
    Evidence: Light Rig alone has a “follow selection or lock” subject chip (`alkitpage.cpp:600-669`); Path is overwritten from current cast selection every draw (`llfloaterdirector.cpp:3274-3280`). Value **5/5**; effort **M** — `alkitpage.*`, `alkitshell.*`, actor-page adapters; risk: stale pins when actors leave.

13. **Visible scene-operation receipts** — show “Scene X saved/loaded at 14:32” near the scene controls.
    Evidence: successful save/load only write logs (`llfloaterdirector.cpp:2296,2567`), whereas failures reach notifications (`:2256-2262,2315-2321`). Value **3/5**; effort **S** — `llfloaterdirector.cpp`, transport-bar source; risk: keep receipts unobtrusive.

## Performance

14. **Director-wide performance governor** — one target-FPS policy coordinating VCam, probe refresh, shadows, weather, and expensive look effects.
    Evidence: adaptive protection currently belongs only to VCam (`panel_dpage_camera_vcam.xml:838-863`), while Live Probe exposes an independent five-mode refresh system (`panel_dpage_light_cine_light_rig.xml:813-842`). Value **5/5**; effort **L** — feature managers, `llfloaterdirector.cpp`, performance page; risk: automatic quality shifts during a take.

15. **Selected-camera Solo Preview** — update only the edited VCam monitor, with a momentary “refresh all” command.
    Evidence: when previews are enabled, demand is registered for every capture (`alpaneldirectorvcam.cpp:332-358`); the UI already reports preview cost (`panel_dpage_camera_vcam.xml:34`). Value **4/5**; effort **M** — `alpaneldirectorvcam.*`, monitor widgets, preview service; risk: stale thumbnails must be visibly marked.

16. **Actionable UI-health meter** — turn “UI -- ms” into green/amber/red health plus a tooltip naming the slowest visible page/card.
    Evidence: the raw metric is cryptic and first in the narrow-width collapse order (`floater_director_v2.xml:103,127`). Value **3/5**; effort **M** — `alkitshell.*`, profiling hooks, transport-bar source; risk: profiling itself must stay cheap.

## Safety/recovery

17. **Make scene selection non-destructive until Load is pressed** — combo selection should preview metadata; the existing Load button should be the sole apply action.
    Evidence: choosing a combo entry immediately calls `loadScene()` (`llfloaterdirector.cpp:2140-2146`) despite a separate Load button in the UI (`floater_director_v2.xml:110-116`). Value **5/5**; effort **S** — `llfloaterdirector.cpp`; risk: behavior change for users accustomed to instant loading.

18. **Scene revisions and overwrite confirmation** — retain recent versions and offer Save As/Duplicate before replacing a named scene.
    Evidence: saving atomically renames directly over the destination (`llfloaterdirector.cpp:2250-2293`); deletion confirms (`:2190-2209`) but overwrite does not. Value **5/5**; effort **M** — scene persistence/UI in `llfloaterdirector.*`; risk: storage growth and revision cleanup.

19. **Scene load preview/report** — summarize what will change, what is unsupported, missing presets, offline faces, and unsaved categories before committing.
    Evidence: unsupported versions and missing camera presets mostly log warnings (`llfloaterdirector.cpp:2326-2376,2541-2549`); the Scene page lists major categories not saved (`panel_dpage_capture_scene_options.xml:7-8`). Value **5/5**; effort **M/L** — scene parser, diff model, Scene page; risk: report must match tolerant legacy-load semantics.

20. **Crash-recovery autosave with a scene-level dirty indicator** — periodically save a rolling recovery snapshot and offer restoration next launch.
    Evidence: scenes are exclusively manual Save operations (`floater_director_v2.xml:110-117`), despite spanning cast, settings, Prism, and rig state (`llfloaterdirector.cpp:2212-2248`). Value **5/5**; effort **M/L** — `llfloaterdirector.*`, settings, startup recovery; risk: avoid capturing transient half-edits or sensitive paths.

21. **Production lock during ACTION/recording** — optionally freeze scene loads, subject reassignment, destructive resets, and camera removal until CUT.
    Evidence: transport changes ACTION into CUT while running (`llfloaterdirector.cpp:1831-1868`) but does not lock the rest of Director; scene load also cuts and disarms state (`:2379-2393`). Value **4/5**; effort **M** — `llfloaterdirector.cpp`, `alkitshell/page.*`, VCam panel; risk: emergency corrections need an override.

## Shooting workflow

22. **ACTION preflight** — before firing, flag away cast, missing subjects/marks, unavailable camera sources, recording conflicts, and FPS below target.
    Evidence: ACTION validates only whether something is armed (`llfloaterdirector.cpp:1847-1868`); away-state and marks already exist in the roster/legend (`floater_director_v2.xml:149-159`). Value **5/5**; effort **M** — `llfloaterdirector.cpp`, cast/VCam/recorder queries; risk: provide “Fire anyway.”

23. **Slate and take log** — auto-number takes per scene/shot, add notes and ratings, and retain failed/aborted takes instead of treating the recorder as one anonymous buffer.
    Evidence: Takes only embeds the live recorder (`panel_dpage_camera_takes.xml:3-4`); transport exposes timecode but no take identity (`floater_director_v2.xml:107-110`). Value **5/5**; effort **M/L** — recorder model/panel, Director transport, persistence; risk: migration and disk retention policy.

24. **Unified timecoded cue lane** — schedule light, gaze, animation, camera-switcher, and recorder events against the take clock and arm them with ACTION.
    Evidence: ACTION arms only moves, camera, recorder play/capture, and delay (`floater_director_v2.xml:130-145`); light and gaze cues live in separate launcher pages (`panel_dpage_light_cues.xml:3-9`, `panel_dpage_performance_gaze_cues.xml:3-9`). Value **5/5**; effort **L** — transport, cue models, switchers, recorder timeline; risk: clock synchronization and partial cue failure.

25. **Named mark banks** — store blocking sets such as “Start,” “Door,” and “Finale,” preview differences, then recall one bank.
    Evidence: transport provides only one global Set Marks/Reset Marks pair (`floater_director_v2.xml:118-119`), calling a single cast snapshot/reset path (`llfloaterdirector.cpp:1810-1817`). Value **5/5**; effort **M/L** — `lldirectorcast.*`, scene serialization, transport UI; risk: world movement remains client-local and may collide with live edits.

26. **Visible/audible countdown overlay** — flash or sound each beat before ACTION so a solo operator or performer knows exactly when the take starts.
    Evidence: delay is configurable to ten seconds (`floater_director_v2.xml:143-144`), but countdown feedback is confined to the small CUT button label (`llfloaterdirector.cpp:1831-1840`). Value **4/5**; effort **M** — transport, overlay/audio hooks, settings; risk: accessibility and streamer-safe audio defaults.

## Top 6 by value/effort

1. **Explicit Load-only scene application** — **5/S**
2. **Named Undo/Redo tooltips** — **4/S**
3. **ACTION preflight** — **5/M**
4. **Universal target pin** — **5/M**
5. **Scene revisions/overwrite confirmation** — **5/M**
6. **Bulk Subject A–D assignment** — **4/S–M**
