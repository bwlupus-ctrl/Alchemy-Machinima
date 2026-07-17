# Director Console — design brief

Status: DESIGN 2026-07-17. Owner: machinima campaign. Builds on: Actor Mover v2
(e6ff77dc906), Flycam Orbit (7395fda9a2f), CineCam floater rework (958ed05b8c4).

## Why

Three machinima tools, three targeting systems: CineCam follow-target, Actor Mover
roster, Animation Explorer capture list. The operator alt-tabs between floaters and
starts things by hand in the right order. The console unifies them around ONE shared
object — the **Cast** — and ONE transport — **ACTION / CUT**. Usability is the top
requirement: every state visible, every disabled control explains itself, no modal
dialogs except destructive confirms, no hidden modes.

## Architecture

### LLDirectorCast (new: lldirectorcast.h/.cpp — engine, no UI)
Session-scoped singleton. The single source of truth for "who is in the scene."

```
struct CastMember {
    LLUUID      mId;             // avatar or control-avatar id
    std::string mLastName;       // cached display name (survives region exit)
    LLVector3   mMark;           // region coords; zero = no mark set
    bool        mHasMark;
    LLUUID      mLocoAnim;       // per-actor locomotion override (null = stock walk)
    // transient, resolved per query: alive?, moving?, anim count
};
```

API (all session-only, nothing persisted except via scene files):
- add/remove/toggle(id), ordered list access, `resolve(id) -> LLVOAvatar*`
- Subject A / Subject B (LLUUIDs into the cast; A doubles as the orbit/CineCam anchor)
- `setMarks()` / `resetToMarks()` (uses ActorMover-style local root placement for
  the snap-back; a reset while moving stops the move first)
- ACTION/CUT: `action()` starts, per the arming flags (see Transport), everything at
  once; `cut()` stops all. Arming flags live here so the floater is a pure view.

Integration (thin, no behavioral fork):
- LLActorMover: roster queries delegate to the Cast (the v2 roster becomes the Cast
  list; right-click "Actor Mover Target" becomes an alias for Add to Cast).
- LLCinematicCamera::resolveTarget(): Subject A wins when set, then existing
  follow-target -> selected -> self chain. Two-Shot/OTS read Subject B for the
  second body when set (falls back to current self+target behavior).
- Flycam Orbit anchors off the same resolveAnchor() and therefore Subject A.
- Old floaters stay registered and functional; they read/write the same state.

### LLFloaterDirector (new: llfloaterdirector.h/.cpp + floater_director.xml)

Layout (resizable, min 620x480, save_rect):

```
+--------------------------------------------------------------------------+
| [  ACTION  ]  delay [3s v]  | [Set Marks] [Reset to Marks] | scene: [v] [Save][x] |
+--------------------------------------------------------------------------+
| CAST (190px)        |  [ Move ] [ Animate ] [ Camera ] [ Takes ]         |
| +-----------------+ |                                                    |
| | ● Aria      A ▸ | |  (active tab content)                              |
| | ● Kestrel   B   | |                                                    |
| | ○ crow(mesh)    | |                                                    |
| | ◌ Voss (away)   | |                                                    |
| +-----------------+ |                                                    |
| [+ You] [– Remove]  |                                                    |
| ☑ show heading rays |                                                    |
+--------------------------------------------------------------------------+
| 2 moving · CineCam: Push-In → Aria · Orbit: off · REC ○                  |
+--------------------------------------------------------------------------+
```

**Cast list** (LLScrollListCtrl, multi-select):
- Row = status glyph + name + badges. Glyphs: ● moving, ○ idle, ◌ not in world
  (grayed, kept in cast, auto-revives on return). Badges: A / B subject, ⚑ mark set.
- Right-click context menu: Set Subject A, Set Subject B, Set Mark Here,
  Reset to Mark, Clear Loco Anim, Copy UUID, Remove from Cast.
- "+ You" adds self; hint text explains world right-click -> Add to Cast.
- Multi-select drives batch ops on the Move tab.
- Refresh in draw() with change-diffing (v2 actor-mover idiom): cells re-set only
  when text/state changed; rows rebuilt only on membership change.

**Transport bar**:
- ACTION is the largest control on the floater; it morphs to CUT (red) while
  running. Optional countdown spinner 0-10 s (default 0): ACTION waits, shows
  "3…2…1…" on the button itself, so a solo operator can get into frame.
- Arming checkboxes (small row under the bar, persisted settings): ☑ actor moves,
  ☑ camera pattern, ☐ recorder playback, ☐ recorder capture. ACTION fires exactly
  what is armed; the button tooltip lists what will fire.
- Set Marks snapshots every cast member's current rendered root; Reset to Marks
  snaps back (local-only, same philosophy as the mover).

**Move tab** — Actor Mover v2 controls re-homed:
- Segmented target control at top: (Selected actors | Everyone) replacing the
  sync checkbox — same setting underneath.
- Speed / Distance / Heading / Cadence / End behavior + Walk & Stop.
- Per-actor state is already visible in the cast list; the tab shows the params
  that will be CAPTURED at start (label: "Applies when a walk starts").

**Animate tab** — the new middle ground:
- Header: selected cast member's name.
- Live list of that actor's currently signaled anims (LLVOAvatar::mSignaledAnimations,
  diffed refresh): asset UUID, priority when resolvable, playing state.
- Row actions: Copy UUID · Set as Loco Anim (writes CastMember::mLocoAnim; the
  mover uses per-actor loco anim when set, else ActorMoverCustomAnim/stock) ·
  Play Local / Stop Local (client-side startMotion/stopMotion on that avatar).
- Paste-a-UUID line editor + Play Local button (plays any anim id on the selected
  actor, local only).
- Small link-button: "Open Animation Explorer" for discovery beyond the cast.

**Camera tab**:
- Subject line: "A: Aria [set from selection] · B: Kestrel [set from selection]
  [clear]" — buttons, mirrored by the cast badges.
- Embedded CineCam shared header (Enabled, Mode, LookAtHead, UseOperator,
  Smoothing, DutchAngle, FrameOffsetUp) + the per-mode auto-hiding panel
  (REUSE: extract the mode-panel host from LLFloaterCinematicCamera into a shared
  LLPanel subclass both floaters instantiate — do NOT duplicate 22 panels of XML;
  factor floater_cinematic_camera.xml's panels into a panel_cinecam_params.xml
  included by both).
- Preset row (same presets as the standalone floater — same folder).
- Flycam Orbit group: Enabled, Level, Min/Max radius, Zoom, Offset Up/Left,
  Smoothing (bound to the same FlycamOrbit* settings).

**Takes tab**:
- Flycam Recorder re-homed: record/play/stop, scrub slider, anchor mode combo —
  bound to the same state as the standalone recorder floater (extract a shared
  panel if practical; otherwise mirror the controls against the same
  LLFlycamRecorder API).

**Status strip**: one line, always current: "<n> moving · CineCam: <mode> → <A name>
 · Orbit on/off · REC ●/○". Doubles as the at-a-glance sanity check before ACTION.

## Visual language (bind D2) — match the poser, not our v1 floaters
User feedback 2026-07-17: the v1 machinima floaters (stacked full-width sliders)
read as utilitarian next to the FS poser. The poser's language, all reusable
in-tree (floater_fs_poser.xml is the reference):
1. **Left icon/label tab rail**: `tab_container` with `tab_position="left"` —
   the console's Move/Animate/Camera/Takes tabs use this, NOT top tabs. The
   cast list sits inside each tab's left edge? No — cast stays a persistent
   column between rail and tab content (rail 72px, cast ~180px, content flex).
2. **Direct-manipulation centerpiece**: the poser's `fs_virtual_trackpad`
   (FSVirtualTrackpad, registered custom LLUICtrl) is the pattern. The console
   gets a **heading/orbit pad**: Move tab shows a compass dial (drag to set
   ActorMoverHeading, needle + N/E/S/W ticks, live-linked to the heading ray);
   Camera tab reuses the same widget class for orbit azimuth/elevation.
   Implement as a new custom widget (al_compass_dial or similar) registered
   like fs_virtual_trackpad; design it to be reusable for the planned wind
   az/el dial later (same widget, different bindings).
3. **Row idiom**: label + slider + spinner on ONE row (poser style), consistent
   label column width per panel; group related rows in bordered sub-panels
   with a bold section label; buttons in aligned rows at panel bottom.
4. **Iconography**: use existing skin icons (the poser's floater.string icon
   registry idiom) for cast row glyphs (avatar vs animesh vs missing) and
   transport buttons instead of ASCII glyphs where an icon exists.
5. **Density**: the poser fits a whole rig in 450x330. Budget the console at
   ~640x460 default; no wasted vertical runs of full-width sliders.

## Usability rules (bind the implementation)
1. Disabled controls carry tooltips that say WHY ("Select a cast member first").
2. No notification popups for names: scene save uses an inline line editor +
   Save button. Confirms (Delete scene, Reset All) may use notifications.
3. Everything ACTION will do is legible before pressing it (arming row + tooltip
   + status strip).
4. All units in labels or tooltips (m, m/s, deg, s).
5. The floater must be fully usable at min size; nothing clipped.
6. Existing standalone floaters keep working; no state forks — one source of truth.

## Scene files (Phase 2 of this brief)
LLSD one-file-per-scene in user_settings/director_scenes/ (LLURI::escape names,
same idiom as cinematic_presets): cast (ids + cached names + marks + loco anims +
subjects), arming flags, active CineCam preset name + mode, FlycamOrbit* values,
Move-tab params. Load = restore all of it; missing avatars stay in cast grayed.
Save/load/delete UI in the transport bar (combo + Save + delete ×).

## Build order
- **D1 (engine):** LLDirectorCast + LLActorMover/LLCinematicCamera/orbit
  integration + right-click alias. No UI beyond menu aliases. Old floaters
  unaffected. Testable via existing floaters.
- **D2 (console):** LLFloaterDirector + XML + Move/Camera/Takes tabs + transport
  (ACTION/CUT/marks/countdown/arming) + status strip + cast list UX + the
  CineCam mode-panel extraction. Menu entry "Director Console...".
- **D3 (depth):** Animate tab + scene files + polish pass (keyboard: Enter on
  focused ACTION, Esc = CUT while running).

Each stage: default no-op outside the new floater, build-lock protocol if agents
run in parallel, staging + grep-verify, explicit-path commits.

D1/D2/D3 SHIPPED 2026-07-17 (faf3789b71a / ac42adff161 / 73bdca95273).

===========================================================================
# ACTOR PATHING SUBSYSTEM (2026-07-17 — built BEFORE the deferred polish below,
# per user "pathing first, polish later"). Requirement from user, verbatim intent:
# pathing must be ROBUST and NATURAL above all — smooth natural rotation between
# points, works on slopes/stairs (3D), easy to place/edit/manipulate, DOCUMENTED.
===========================================================================

## Concept
Generalize an actor move from today's single straight segment (LLActorMover::Move
= mOrigin/mHeading/mSpeed/mDistance) to a **3D polyline path** the actor walks,
with natural curved turning, ground-following, and per-node expression. Local-only
(same ghost philosophy — sim never told). The path is owned per cast member and
saved in the scene file.

## P1 — Path engine (LLActorMover, no new floater)
Data model — replace/extend Move with a path:
- `struct Waypoint { LLVector3 mPosGlobal (region/global coords, Z included);
   F32 mDwell (s, 0=none); F32 mSpeedOverride (0=use path speed); LLUUID mAnim
   (null=use loco anim); F32 mGroundOffset (manual Z nudge). }`
- `struct Path { std::vector<Waypoint>; F32 mSpeed; S32 mEndMode (stop/loop/
   pingpong); F32 mTension (0..1 corner smoothing); F32 mEaseIn, mEaseOut (s);
   S32 mArrivalFacingMode (none/direction/castmember); F32/LLUUID facing target;
   bool mGroundFollow; bool mPitchToSlope; }`. Stored per-actor in LLDirectorCast
   (or an LLActorMover map keyed by actor id). A single-waypoint or empty path
   falls back to the legacy straight-segment behavior (byte-identical default).

Motion (the load-bearing math — get this RIGHT):
- **Centripetal Catmull-Rom spline** through the waypoints (centripetal alpha=0.5
   parameterization SPECIFICALLY — it is the variant proven free of cusps and
   self-intersections on tight corners; uniform/chordal will loop and look drunk).
   `mTension` blends corner sharpness (0=tight/near-polyline, 1=loose/flowing);
   expose as a slider. Endpoints: duplicate/reflect end tangents (standard CR
   phantom-point trick).
- **Arc-length parameterization**: precompute a cumulative arc-length table so the
   actor advances at CONSTANT GROUND SPEED along the curve, not constant spline
   parameter. This is what keeps cadence-lock honest — foot plants match true
   ground speed through curves and over 3D climbs. Recompute the table when the
   path edits.
- **Facing** from the path tangent (derivative of the spline), with a **max
   turn-rate clamp** (deg/s, ~natural walk turn) so sharp nodes ease rather than
   snap. Straightaways ignore the clamp.
- **Ease-in/ease-out**: speed ramps from 0 over mEaseIn at the start and down to 0
   over mEaseOut into the final node (skip for loop mode). No pop from standstill.
- **Arrival facing**: at path end (stop mode), rotate to face a compass direction
   or a cast member.
- Cadence-lock unchanged in principle: setAnimTimeFactor from the CURRENT ground
   speed (which now varies with ease + per-node speed overrides), so the walk
   clock tracks instantaneous speed. Speed-driven gait: optional walk/run anim
   pick by a speed threshold.

3D / ground-follow (slopes + stairs):
- Per-waypoint Z stored; between nodes the spline interpolates Z.
- **mGroundFollow ON**: each frame, resolve the ground under the actor and clamp
   feet to it + mGroundOffset. Terrain slopes: cheap land-height query
   (LLWorld::resolveLandHeightGlobal or equiv). **Stairs/prims: downward object
   raycast** (the tree raycasts for build tools — reuse that path; e.g.
   LLWorld::raycast / gViewerWindow pick down-vector). CAUTION for the worker:
   per-frame per-actor object raycasts cost + have surface-pick edge cases — cap
   the ray length, skip the actor's own attachments, fall back to interpolated Z
   on no-hit, and note this as the piece needing in-world A/B (stairs especially).
- **mPitchToSlope** (default off): tilt root pitch to the local path/ground slope.

Integration: the existing Walk button walks the path (falls back to straight when
no path). applyOverride evaluates the spline at the path clock each frame and sets
root world pos/rot — same site that already beats animesh matchVolumeTransform.
Everything default-off / empty-path = byte-identical to current behavior.

## P2 — In-world editor + path visualization (Move tab + debug render)
Placement/manipulation (EASY TO USE — bind this):
- With an actor selected + an "Edit path" mode toggle on: left-click ground = drop
   a numbered waypoint (Z from ground-snap under the click); drag a node to move
   it (screen-drag on the ground plane, Alt-drag for height); click a segment to
   insert a node; right-click a node = delete/set dwell/set speed/set anim; Shift
   changes append-vs-insert. A waypoint list in the Move tab mirrors the nodes
   (select row ↔ highlight node) with a per-node inspector (dwell/speed/anim/Z).
Visualization (UPGRADES the current thin amber heading line):
- Thick, outlined path line (readable over any ground), **numbered node markers**,
   **direction chevrons** along the path, dwell nodes flagged, **time-tick marks**
   (where the actor is at t=1s,2s… — for eyeballing camera sync), selected node
   highlighted/pulsing, a start **facing gizmo** (current facing vs first travel
   dir). No path yet → the thick straight heading arrow (today's line, upgraded).
- Renders in the same beacon-style UI pass (no depth writes) already used by
   renderHeadingPreview; gated to when the Director/Actor Mover floater is open.
- mTension slider, ground-follow + pitch-to-slope toggles, ease-in/out spinners,
   arrival-facing controls all live on the Move tab.

## P3 — Choreography (build on P1; feature-complete per user)
- **Look-at while walking**: per-actor gaze target = point / cast member / camera.
   Client-side head+neck+eye joint override layered on the walk. Head-vs-eyes-only
   blend + intensity weight + NATURAL joint limits (ease toward target, give up
   gracefully when it's behind them). Default "look where I'm going" tracks the
   path tangent. Composes with Lead-Follow cam = walk-and-talk.
- **Sync path to camera Take**: bind a path's duration to a LLFlycamRecorder take;
   the take's playhead becomes the master clock driving the actor's path param
   (with a lead/trail offset). SCRUBBING THE TAKE SCRUBS THE ACTOR (dry-run preview
   of body+lens together). Press ACTION once → deterministic re-takes. This is the
   feature that makes Takes legible (retro-fixes the "what does Takes do" confusion).
- **Follow-the-leader**: actor B rides actor A's path BY REFERENCE (edit A → B
   updates) at a distance offset (N m back on the curve) or time offset (T s later).
   Chainable for processions; composes with look-at + dwell.
- **Staggered start delay** per actor (entrances not all on one frame).
- Named paths saved in the scene file (blocking restored on scene load, not just
   marks). Path-ghost dry-run scrubber (draggable phantom along the path).

## Explicitly OUT (director wants exact control, not surprises)
Inter-actor collision avoidance; navmesh pathfinding (client can't reach it cleanly
— ground-raycast covers real needs).

===========================================================================
# DEFERRED POLISH (after pathing, per user sequencing)
===========================================================================
- **Layout reflow**: Director floater panels should stretch with the window
   (follows/auto-resize), less cramped, workflow order L→R (Cast→Move→Animate→
   Camera→Takes→ACTION). Bump default size; cast column + tab body both grow.
- **Takes-tab explainer**: one-line header "Record and replay the CAMERA's motion
   for this shot" — kill the waypoints-vs-camera confusion at the source.
- **Animate tab preview pane**: embed LLPreviewAnimation (the Animation Explorer
   spinning-dummy viewport). Clicking a UUID row (or pasting a UUID) AUTO-PLAYS it
   in the pane ON LOOP. Plus the **priority slider** on the custom UUID player
   (LLKeyframeMotion::setPriority exists — but motions are asset-cached, so VERIFY
   setting priority doesn't bleed onto other avatars playing the same asset; scope/
   restore if it does).
