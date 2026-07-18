# Machinima UI shared-panel refactor (top-to-bottom)

Status: PLAN 2026-07-18. Driver: the Director Console hand-replicated several floater
control groups instead of embedding the same panel, so they DRIFTED (Flycam Orbit's
console group was missing the joint/target/anchor-hint the standalone floater has).
User directive: refactor top to bottom — "if there is one fuck up, there are others."

## Target architecture (the invariant)
EVERY machinima feature is ONE shared LLPanel (`panel_*.xml` + an `ALPanel*` class
registered via LLPanelInjector, the ALPanelCineCamParams / ALPanelPathEditor pattern).
- The STANDALONE floater = that shared panel + a window frame (0 own widgets).
- The DIRECTOR CONSOLE tab = the SAME shared panel, embedded via `<panel class=... filename=...>`.
- Result: the console and the floater render the identical UI from ONE source. Drift is
  impossible by construction. Enforces [[director-console-superset-rule]].
- Any NEW control goes in the shared panel → appears in both places automatically.

## Already shared (done, verify only)
- Cinematic Camera → `panel_cinecam_params.xml` / ALPanelCineCamParams (console Camera tab +
  floater_cinematic_camera.xml, 0 own widgets). ✓
- Path editor → `panel_path_editor.xml` / ALPanelPathEditor (console Path tab +
  floater_actor_mover.xml). ✓

## To extract (4 workers, SEQUENTIAL — all touch floater_director.xml + llfloaterdirector.cpp,
## so they cannot parallelize; one clean extraction per worker, build + parity-verify each)

### R1 — Flycam Orbit  → `panel_flycam_orbit.xml` / ALPanelFlycamOrbit  (the reported bug)
Move ALL of floater_flycam_orbit.xml's widgets into the panel: Enable, the ANCHOR HINT text,
the JOINT combo (CinematicCamJoint), TARGET SELECTED (CinematicCamUseSelected), Level horizon,
Min/Max radius, Smoothing, Lens (zoom×), Up, Left offsets. Embed in floater_flycam_orbit.xml
AND the console Camera tab (delete the console's hand-copied orbit group). The joint/target
being in the orbit panel is correct even though they're shared settings — same setting shown
where the user expects it.

### R2 — Flycam Recorder → `panel_flycam_recorder.xml` / ALPanelFlycamRecorder  (Takes tab)
Move floater_flycam_recorder.xml's transport + controls into the panel: Record/Play/Pause/
Stop/Clear, scrub slider, time/status readouts, Save/Load, Speed, Loop, Smooth, Anchor,
Follow, Operator, SampleRate. Embed in the floater AND the console Takes tab (delete the
console's hand-copied recorder controls + their llfloaterdirector wiring).

### R3 — Animation Explorer → `panel_anim_explorer.xml` / ALPanelAnimExplorer  (Animate tab)
Move the explorer UI into the panel: the signaled/recent anim LIST, the LLPreviewAnimation
PREVIEW PANE (auto-loop on select/paste), Copy UUID, Stop, Stop-and-Revoke, Blacklist,
Capture-all, paste-UUID row, set-loco / play-local. TRICKY: the preview's mouse-drag-to-rotate
uses floater-level mouse capture (handleMouseDown/Hover/ScrollWheel/onMouseCaptureLost) — move
that to the PANEL (LLPanel handles mouse events); the panel captures within the preview rect.
Embed in floater_animation_explorer.xml AND the console Animate tab (delete both hand-copies +
the floater-level mouse handlers in llfloaterdirector). Keep the console's cast-scoped
"Stop local / set-loco" (that's cast-specific, not an Explorer control) — or fold cleanly.

### R4 — Actor Mover transport → `panel_actor_mover.xml` / ALPanelActorMover  (Move tab)
Move floater_actor_mover.xml's 14 transport widgets into the panel: roster list, Selected|
Everyone scope (ActorMoverSync), the compass dial (ActorMoverHeading), Speed/Distance/Cadence,
End behavior, custom-anim UUID, Walk/Stop. (floater_actor_mover already embeds panel_path_editor
— it keeps that; this panel is the TRANSPORT half.) Embed in the floater AND the console Move
tab (delete the console's hand-copied transport + wiring). NOTE: the console Move tab also has
the Straight|Path mode + cast column context; keep those console-only bits, embed the transport
panel for the shared controls.

## Rules for every R#
- Behavior/settings unchanged — pure structural move; every control keeps its control_name /
  the same singleton callbacks. No state forks. Standalone floaters keep working (now thinner).
- New panel files → CMakeLists.txt + LLPanelInjector registration.
- VALIDATE every touched XML (python xml.dom.minidom.parse, no `--` in comments) source+staged
  BEFORE build. Build-lock. Stage manually. Explicit-path commits. Mindset: filmmaker+veteran,
  quality==feature.
- After each: PARITY CHECK — the console tab and the standalone floater now render the SAME
  panel; confirm zero remaining hand-copied widgets for that feature.
