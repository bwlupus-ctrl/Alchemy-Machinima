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
