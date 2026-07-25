# Alchemy-Machinima — Feature Changelog

A human-readable record of what this fork adds on top of [Alchemy Viewer](https://github.com/AlchemyViewer/Alchemy).
Organized by area rather than by date. Reconstructed from git history on `develop`
(fork work begins 2026-07-05); maintained forward from 2026-07-25.

**Status key:** ✅ confirmed working in-world · ⏳ built but not yet verified in-world ·
⚠️ known limitation · ⛔ tried and reverted

---

## Director Console — the machinima control surface

A single floater that composes every machinima tool, so a shot can be staged, cued and captured
from one place. Anything editable in a standalone machinima floater is also editable here.

- ✅ **Cast list** — nominate any avatar (or clone) as an actor; **Subject A/B** drive camera framing
  and two-shots.
- ✅ **Actor groups** — tag actors into named groups, display and control them as a unit, with
  **per-group staggered starts** so a crowd doesn't move in lockstep.
- ✅ **ACTION / CUT transport** — one playhead arms moves, camera, and recorder together, with an
  optional countdown so a solo operator can get into frame.
- ✅ **Marks** — snapshot each actor's current position and reset the whole cast back to it.
- ✅ **Scene files** — save and reload cast, subjects, marks and groups.
- ✅ **Tabs** — Cast, Camera, Path, Animate, Takes, Ghosts. Tab memory, icons, legend, and a toolbar
  button.
- ✅ **Hotkeys** — gated F2–F8 machinima transport keys.
- ✅ **Right-click Director submenu** consolidating machinima actions on avatars and objects.
- ✅ **Shared-panel architecture** — Flycam Orbit, Flycam Recorder, animation preview, Actor Mover
  and the path editor are single panels reused by both their standalone floaters and the console.

## Cameras

- ✅ **Cinematic Camera — 42 shot modes.** Film-grammar moves (dolly zoom, push-in, low hero,
  God's-eye, over-the-shoulder, crash/slow zoom, whip arc, reveal, pull-back, two-shot, lead-follow,
  ECU, long lens, spiral, pedestal), a dozen acrobatic/dance/impact moves (barrel roll, corkscrew,
  pendulum, contra-orbit, fisheye lunge, floor skimmer, boost rise, boom over, top spin, turntable,
  floating ECU, tilt whip), plus a composable **Dutch angle** and global **Frame Up** offset.
- ⏳ **8 newer modes** — Body Helix Reveal (orbits feet→head, ending on the face), Descent, Parallax
  Slide, Figure-8, Detail Sweep, Step Orbit, Cable Cam, Breathing Hold.
- ✅ **Locked follow subject** via avatar context menu.
- ✅ **Bone Camera** — mount the camera on any character joint (BD donor behavior).
- ✅ **Cinematic head tracking** — camera focus and roll follow the head.
- ✅ **Flycam Orbit** — bone-locked, stabilized orbit for the joystick flycam, with zoom and anchor
  offsets.
- ✅ **Flycam Recorder** — record, play back and scrub camera paths; relative playback with a
  selectable anchor.
- ✅ **Per-node cameras on actor paths** — a walk and its camera move as one TAKE.
- ✅ **Phototools floater** — WYSIWYG depth of field, focus crosshair, framing guides.
- ✅ **Camera presets** — auto-persisting edits, in-place rename, per-control reset buttons.
- ✅ **Mouselook camera offset** (X/Y/Z) and configurable **near-clip** for extreme close-ups.

## Actors, pathing and choreography

- ✅ **Actor Mover** — local ghost locomotion at arbitrary speed for any avatar, with a multi-actor
  roster, sync or individual control, heading preview, animesh and custom animations.
- ✅ **Actor pathing** — 3D Catmull-Rom splines with an in-world waypoint editor, per-actor colors,
  path visualization, onion-skin, undo/redo and structural ops.
- ✅ **Choreography** — sync-to-take, follow-the-leader, and look-at-while-walking (procedural gaze).
- ✅ **Suspend/resume across teleport or derez** — a walk survives instead of being destroyed.
- ✅ **Prop Mover** — client-side objects driven along the same splines.

## Ghost Studio — client-only avatar clones

Styled, independently posed copies of any avatar, visible only to you. Nothing is ever sent to the
simulator.

- ✅ **Entity clones** that render through the **real avatar path** with full scene lighting and
  shadows — correct rigging, animesh, baked (BOM) and PBR/GLTF textures, source-matched LOD.
- ✅ **Independent animation** — Mirror / Directed / Frozen drive modes; a clone can play its own
  animation while its source does something else.
- ✅ **Placement** — numeric transform, one-click ground placement, snap-to-actor/me, in-world edit
  mode with a manipulator gizmo, and a live heading dial.
- ✅ **Uniform scaling** — foot-pivoted, so feet stay planted. The cinematic camera frames scaled
  clones correctly.
- ✅ **Seeded chaos** — deterministic per-clone variation in yaw, scale and (newer) animation tempo,
  so a crowd reads as individuals rather than copies.
- ✅ **Arrays** — line and ring duplication.
- ✅ **Safety guards** — clones are excluded from IM, calls, teleport, pay, friendship, block/mute,
  voice moderation, tracking, autopilot, the Scene Explorer and RLVa name paths, so a synthetic id
  can never leak to the server.
- ⏳ **Formations** — Arc, Grid, V/wedge, Spiral, Staircase, Tunnel, and deterministic Scatter.
- ⏳ **Placement QoL** — drop-to-ground, align-feet, reset-upright, duplicate-in-place, copy/paste
  transform, multi-select.
- ⏳ **Look-at / face target** — point clones at the camera, you, a cast member or another clone;
  one-shot or continuous.
- ⏳ **Animation tools** — per-clone speed (0.05–4×), pause/resume, sync-restart across a selection,
  loop / play-once, and an **inventory animation library** with duration, loop points, priority and
  joint metadata (replacing raw UUID entry).
- ⏳ **Bullet-time freeze-strip** — captures successive poses over real time into a formation, so one
  action becomes a frozen strip.
- ⏳ **Procedural formation motion** — Orbit, Spin, Breathe, Ripple applied to a whole formation.
- ⏳ **Avatar physics on clones** — breast/belly/butt physics driven from the source's own settings.
- ⏳ **Texture animation** — `llSetTextureAnim` effects (scrolling, rotating, flipbook) replicate
  onto clones, which have no scripts.
- ⏳ **Lens gaze** — head, neck and eyes track the camera, overriding animation on those joints.
  Works for clones, your own avatar, and other residents (locally).
- ⚠️ A ground-sitting source leaves its clone in its previous pose rather than seated — reproducing
  the ground-sit pose carried side effects that made the clone vanish.

## Poser and animation

- ✅ **Poser** — works on any avatar.
- ✅ **Pose Stand + Undeform.**
- ✅ **Animation Explorer** — forensics on recently played animations, with UUID tools.
- ✅ **Client-side animation priority override** — per-instance, with no cache bleed.
- ✅ **Pose ghosts** — true-3D translucent model ghosts with render styles and an always-visible
  blocking preview.
- ✅ **Configurable head/eye tracking limits.**

## Rendering and lighting

- ✅ **Volumetric lighting / godrays** (Black Dragon donor) with its own Lightbox tab.
- ✅ **Per-projector volumetric light cones** — visible spotlight shafts with per-projector opt-in,
  gobo-colored shadows, temporal reprojection, half-res march with depth-aware upsample, blue-noise
  dither, height fog, density and bloom feed, plus S-Log beam levers and a directional rim glow.
- ✅ **Up to 6 projector shadows** (was hard-limited to 2) — each spotlight now samples its own
  shadow map.
- ✅ **Per-cascade and per-projector shadow-map resolution**; **Vogel-disk PCF** soft shadows;
  **stable projector shadows** by world-size priority.
- ✅ **Independent light-source class toggles.**
- ✅ **AMD FidelityFX LPM** as a tonemap option, with its own parameter sliders.
- ✅ **BD-parity LOD defaults** and a full LOD stack in the Lightbox; **MachinimaHighLOD** filming
  mode (force LOD 3 + anti-cull).
- ✅ **Motion vectors / velocity buffer** including true rigged and classic avatar limb motion, with
  a **native motion-blur composite**.
- ✅ **Forced client-side alpha masking** with adjustable cutoff, plus **per-object / per-avatar
  alpha-mode override** (Force Mask / Force Blend) reachable from attachment menus.
- ✅ **Resolution-aware autoscale** for SSAO, shadow blur and DoF.
- ✅ **Shader binary cache fix** — the cache now keys on source contents, so shader edits actually
  take effect.
- ✅ **10-bit output** — the viewer requests a standard R10G10B10A2 back buffer.

## Performance and memory

Tuned for a 192 GB / RTX 5090 class machine.

- ✅ **Decoded-texture RAM pool** — system RAM feeds VRAM, with tiered residency.
- ✅ **Decoded-mesh RAM pool** — unpacked LODs served from RAM.
- ✅ **Parallel texture-cache I/O** — cache reads went from ~781 ms mean / 5 s p95 to ~46 ms / 200 ms.
- ✅ **Capture-mode pin** — no mid-scene texture degradation during a take.
- ✅ **Auto-sized heap** (⅞ of installed RAM) and configurable decode thread ceiling.
- ✅ **Fast shutdown** and a bounded in-RAM chat backlog.
- ✅ **Live RAM-pool stats** in the Lightbox Machinima tab.

## ReShade integration

- ✅ **G-buffer bridge** — the viewer publishes real depth, albedo, ORM, normals, emissive and motion
  vectors to an external add-on over a shared ABI, so iMMERSE effects can use real scene data instead
  of reconstructing it. Includes an R32F depth copy (SL's OpenGL depth is a long-standing ReShade
  pain point) and correct resource-view registration.
- ✅ **10-bit detection fix** — contributed to a personal fork of ReShade so it detects
  `R10G10B10A2` on NVIDIA.
- ⚠️ Marty's Launchpad still runs its own optical flow; real compute offload needs the remaining
  provider work. Albedo currently causes visual glitches and is under investigation.

## Quality of life

- ✅ **Borderless fullscreen** at desktop resolution, with Windows Fullscreen Optimizations defeated
  so alt-tab doesn't stutter.
- ✅ **FS Extended Radar** + Show Friends Only.
- ✅ **EEP environment editor rework** — local Windlight presets, water adjust, env settings window,
  toolbar buttons.
- ✅ **Controller/gamepad configuration** — the shipping Alchemy subsystem with named button/axis
  mapping and Xbox defaults.
- ✅ **Durable L$ transaction log** — every transaction is written to disk before any toast, so a
  crash or missed notification can't lose the record.
- ✅ **Chat commands** for machinima actions (case-insensitive).
- ✅ **Snapshot conveniences**, slider precision override, **Nimble** movement, **Freeze World**,
  hide own lookAt broadcast, isolated profile directory, advanced/debug menus on by default.

---

## Reverted or abandoned

Kept honest, so nobody re-attempts them blind.

- ⛔ **Local avatar scale (scaling real avatars).** Reverted 2026-07-24 after three distinct
  in-world defects — the transform never bound to draw batches; bin radius and cached extents ignored
  the scale; and the throttled extent update's staleness was multiplied by the scale, culling
  geometry during movement. It also regressed working clone scaling by replacing it. All three root
  causes are documented in `doc/AVATAR_LOCAL_SCALE_PHASE1_BRIEF.md`,
  `SCALED_AVATAR_CULLING_EXTENTS_BRIEF.md` and `SCALED_AVATAR_STALE_EXTENTS_FIX_BRIEF.md`.
- ⛔ **Screen-space contact shadows** — landed, reworked, then fully reverted after in-world testing.
- ⛔ **SMAA T2x** — ported from Black Dragon, then reverted.
- ⛔ **First ReShade RTGI bridge** — shelved as a dead end; preserved on `archive/reshade-rtgi-bridge`.
  A later, cleaner ABI-based bridge replaced it.
- ⛔ **Ghost capability split** — an `EAvatarKind` redesign, reverted; the architecture may still be
  sound for a future re-land.

## Notes on this changelog

Entries marked ⏳ come from a large late-July push and are **built but not yet verified in-world**.
This project's repeated lesson is that compiling and code review do not catch render or animation
defects — only running the viewer does. Treat ⏳ as "should work" rather than "does work".
