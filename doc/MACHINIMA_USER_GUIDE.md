# Absolute Cinema SL — Machinima Feature Guide

> ### ⚠️ Independent & unaffiliated
>
> **Absolute Cinema SL** is an independent, **AI "vibe-coded"** project (built with heavy AI assistance). It is **not a TPV-compliant viewer** and is **not
> affiliated with, endorsed by, or supported by** Alchemy, Firestorm, Black Dragon,
> or Linden Lab. All product names and trademarks belong to their respective owners.

> ### ⚠️ No warranty — use at your own risk
>
> **Absolute Cinema SL is provided "AS IS", without warranty of any kind**, express
> or implied, including without limitation any warranty of merchantability, fitness
> for a particular purpose, title, or non-infringement. The creator makes **no
> guarantee** that it is stable, secure, error-free, or suitable for any purpose.
>
> **To the fullest extent permitted by law, the creator accepts no liability** for
> any damage or loss arising from downloading, installing, or using this software —
> including without limitation damage to your computer, hardware, software, or data;
> loss of Second Life inventory, currency, or account access; or **suspension or
> termination of your Second Life account.** This is a third-party viewer that is
> **not TPV-compliant and not approved by Linden Lab**; using it may violate the
> Second Life Terms of Service, and any consequences are **your sole responsibility.**
> By downloading, installing, or using it, you accept these terms.

A creator's guide to the machinima toolset in **Absolute Cinema SL**. It covers the
camera system, the Director Console, actor and prop motion, clones, weather, and the
capture helpers — what each feature does and how to reach it.

> **Open beta build.** Features are usable but still settling. If something
> misbehaves, see [Reporting bugs](#reporting-bugs) at the end — there is a
> logging switch that makes bug reports far more useful.
>
> *This beta may still show "Alchemy Test" in the window title and login screen
> while the in-client branding is being finalized — everything in this guide still
> applies.*

## About this viewer

**What it is.** **Absolute Cinema SL** is a viewer built for **machinima production
in Second Life** — it hands the creator far more control over cameras, timing,
staging, actors, and look than a standard viewer.

**Lineage.** Based on the **Alchemy** viewer, with features borrowed from
**Firestorm** and **Black Dragon**.

**Your data is separate.** This viewer keeps its own settings, logs, cache, and
presets in an **`AlchemyMachinima`** folder in your user data (on Windows,
`%APPDATA%\AlchemyMachinima`) — completely separate from Alchemy or any other
viewer. Installing or testing it will **not** touch your existing viewer's settings.

**ReShade.** Supports **ReShade** integration — explicitly the **iMMERSE V3**
shader suite — by publishing the viewer's frame data to an external ReShade
add-on (see [Render extras](#10-render-extras), `RenderVisibleDiffuseSidecar`).

## Feature summary — what Absolute Cinema SL adds

Everything below is **added by Absolute Cinema SL** on top of stock Alchemy. Each
item links to its full how-to.

**Cameras & framing**
- **[Cinematic Camera](#31-cinematic-camera)** — a bank of premade cinematic camera
  moves (orbits, dollies, cranes, sweeps) you can trigger and tune.
- **[Motion Start](#32-motion-start-how-moves-begin)** — choose *where* and *which
  direction* a moving shot begins (seed / start angle / CW–CCW), instead of every
  move starting the same way.
- **[Director Camera Switcher](#33-director-camera-switcher)** — a 12-slot camera bank
  with 1–9 hotkeys, A/B/C/D subject marks, auto-director sequencing, and hard-cut /
  ease / bullet-time transitions.
- **[Camera Operator / Camera Shake](#34-camera-operator--camera-shake)** — handheld
  operator simulation with locomotion styles and shake presets.
- **[Frame & Lens](#35-frame--lens)** — aspect-ratio-as-lens FOV plus non-destructive
  letterbox framing guides.
- **[Auto-Reframe](#36-auto-reframe)** — skeleton-aware automatic subject framing.
- **[Flycam Recorder & Orbit](#37-flycam-recorder--orbit)** — record, play back, and
  scrub camera paths.

**Direction & timing**
- **[Director Console](#2-the-director-console)** — one hub for every machinima
  control, with a reflowing layout, plus the
  **[Anim](#anim-tab--animation-inspector--player)** (animation inspector/player) and
  **[Takes](#takes-tab)** tabs.
- **[Temporal Capture](#5-temporal-capture-world-time-scale)** — slow, speed up, or
  freeze world time for bullet-time and precise timing.
- **[Look-At / Gaze](#4-look-at--gaze)** — aim an avatar's gaze and head at a target,
  now with a smooth turn-through instead of a 180° snap.

**Staging & actors**
- **[Actor Mover](#actors--actor-mover)** — drive avatars along 3D waypoint paths,
  with dance modes.
- **[Prop Mover](#props--prop-mover)** — the same motion-path control for objects.
- **[Ghost Studio](#7-ghost-studio-clones)** — spawn scene-lit avatar clones and crowds.

**Look & atmosphere**
- **[Weather](#8-weather-rain-lightning)** — 3D rain, splashes, surface wetness, and
  fractal lightning.
- **[Volumetric Light](#9-volumetric-light)** — sun god-rays and projector light shafts,
  with expanded quality controls.
- **[Lightbox — expanded](#10-render-extras)** — 8 tonemappers (**AMD LPM** is the new
  default), colour-grade LUTs, and CAS/DLS sharpening.
- **Projector shadows raised 2 → 6** — more set lights can cast shadows at once.
- **[ReShade + iMMERSE V3](#10-render-extras)** — publishes frame data to an external
  ReShade add-on for advanced post.

**Under the hood**
- **RAM caches** — decoded-texture and decoded-mesh pools cut stutter (see
  [Performance](#performance--resource-usage)).
- **10-bit SDR output + dither** for smoother gradients on capable displays.
- **[Debug-logging master switch](#reporting-bugs)** — quiet logs for beta, one toggle
  to turn diagnostics back on.
- **Separate `AlchemyMachinima` profile** — never touches your other viewers.

*In development (not yet usable): **Prism Lens** — a designated-prim magnifier /
telescope lens. The render foundation is in place, but there is nothing to point at a
prim with yet.*

## Performance & resource usage

This viewer is tuned for **quality and smoothness over a light footprint**, so it
deliberately uses **more RAM — and somewhat more GPU — than a stock viewer.** Aim
it at a **mid-range or better machine**: a modern discrete GPU (RTX-class is ideal)
and plenty of RAM (**16 GB minimum, 32 GB+ recommended**). It is *not* built for
low-end hardware.

**Higher RAM is by design.** Two "keep it warm in RAM" caches trade memory for a
big reduction in stutter and re-decoding:
- **Decoded-texture pool** (`BDMergeTexPoolEnable`, on) — keeps decoded textures in
  system RAM so re-viewing is a fast upload instead of a disk read + JPEG2000
  re-decode. Auto-sizes up to **50% of physical RAM** (`BDMergeTexPoolFraction`).
- **Decoded-mesh pool** (`BDMergeMeshPoolEnable`, on) — keeps unpacked mesh geometry
  in RAM to skip re-parsing. Auto-sizes up to **10% of RAM** (`BDMergeMeshPoolFraction`).
- Combined, these can hold **up to ~60% of your system RAM** as a warm cache. They
  auto-size to your machine (with floors so small PCs aren't starved), and you can
  lower the fractions or turn the pools off for stock streaming.

**GPU / VRAM.** **HDR** is on by default (`RenderHDREnabled`) with bloom, and
**reflection probes** are the single biggest per-frame cost — roughly *half* the
frame at default quality (VRAM is bounded by `RenderMaxVRAMBudget`). The heavy
**machinima** effects (weather, volumetric light, clones, realtime probes/mirrors)
are **default-off** and only cost you when you enable them.

**To lighten the load:** lower `RenderReflectionProbeLevel` (0 = minimal) and
`RenderReflectionProbeResolution`; reduce `BDMergeTexPoolFraction` /
`BDMergeMeshPoolFraction` (or disable the pools); turn off HDR.

---

## Contents

- **[Feature summary — what Absolute Cinema SL adds](#feature-summary--what-absolute-cinema-sl-adds)**

1. [Getting around](#1-getting-around)
2. [The Director Console](#2-the-director-console)
3. [Cameras](#3-cameras)
   - [Cinematic Camera](#31-cinematic-camera)
   - [Motion Start (how moves begin)](#32-motion-start-how-moves-begin)
   - [Director Camera Switcher](#33-director-camera-switcher)
   - [Camera Operator / Camera Shake](#34-camera-operator--camera-shake)
   - [Frame & Lens](#35-frame--lens)
   - [Auto-Reframe](#36-auto-reframe)
   - [Flycam Recorder & Orbit](#37-flycam-recorder--orbit)
4. [Look-At / Gaze](#4-look-at--gaze)
5. [Temporal Capture (world time scale)](#5-temporal-capture-world-time-scale)
6. [Actors & Props (motion paths)](#6-actors--props-motion-paths)
7. [Ghost Studio (clones)](#7-ghost-studio-clones)
8. [Weather (rain, lightning)](#8-weather-rain-lightning)
9. [Volumetric light](#9-volumetric-light)
10. [Render extras](#10-render-extras)
11. [Quick reference](#11-quick-reference)
12. [Reporting bugs](#reporting-bugs)

---

## 1. Getting around

Almost everything lives in one of two places: a **floater** you open from the
**Me** menu (the same menu as *Camera Controls…*, `Ctrl+K`), or the **Director
Console**, which embeds most of the floaters as tabs in a single window.

Floaters you can open directly from the **Me** menu:

| Floater | What it is |
|---|---|
| Cinematic Camera… | The premade cinematic camera modes |
| Flycam Recorder… | Record / play back / scrub a camera take |
| Flycam Orbit… | Simple orbit-around-a-subject flycam |
| Actor Mover… | Walk avatars along waypoint paths |
| Director Console… | The hub — everything in one window |
| Temporal Capture… | Slow down / speed up world time |
| Ghost Studio… | Clone avatars for crowds and doubles |
| Prop Mover… | Drive objects along paths |

**Chat commands.** Several features are also driven by `/`-commands typed into
local chat (for example `/ghostscale`, `/pathwalk`). These require
**`AlchemyChatCommandEnable`** (on by default) and are case-insensitive.

**Hotkeys.** The Director hotkeys (F2–F8, and 1–9 for the camera switcher)
require **`DirectorHotkeysEnabled`** (on by default) and only fire when a text
field does **not** have keyboard focus, so they never interfere with typing.

> **Tip — settings.** Anything named in `code font` below (e.g.
> `CinematicCamSmoothing`) is a setting you can find in **Preferences → search**,
> or in *Advanced → Show Debug Settings*. Most features also expose these as
> sliders/checkboxes in their panel; the setting name is given so you can find it
> quickly.

---

## 2. The Director Console

**Open:** Me menu → *Director Console…*, or press **F2**.

The Director Console is the hub. It has a transport bar at the top, an arming
row, a cast list, and a set of tabs that embed the individual tools.

**Transport & scenes**
- **ACTION** starts the armed elements (moves, camera, recorder). **CUT** stops
  them. A **Delay** spinner (`DirectorActionDelay`) counts down before ACTION
  fires, so you can get out of frame.
- **Set marks** stores every actor's current position/facing as the take's start
  pose; **Reset to marks** snaps them all back so you can retake.
- **Scenes** save the whole console setup (cast, arming, tab settings) to a named
  file you can reload later.

**Arming** — tick what ACTION should drive: `DirectorArmMoves`,
`DirectorArmCamera`, `DirectorArmRecorderPlay`, `DirectorArmRecorderCapture`.

**Cast list** — add avatars (yourself with **+You**), assign each an actor slot
and an optional group, and mark **A/B/C/D** subject roles that the cameras and
look-at can target. **Focus** frames the selected cast member.

**Tabs:** Move, Ghosts, Props, Anim, **Camera** (with sub-tabs Switcher,
Cinematic, Orbit, Frame, Operator), Recorder, Takes, Shafts (projector
volumetrics), Weather, Time. Each is the same panel you'd get from its standalone
floater.

**Run a take (the whole sequence):**
1. **Build the cast** — right-click avatars in world → Director → Add to Cast (or **+You**); assign **A/B/C/D** subject roles in the Camera tab.
2. **Set the scene** — place actors, set paths/anims, configure the Camera / Weather / Time tabs.
3. **Set marks** (F8) so you can rehearse and **Reset to marks** (F5) between takes.
4. **Arm** what ACTION should fire: `DirectorArmMoves`, `DirectorArmCamera`, `DirectorArmRecorderPlay`, `DirectorArmRecorderCapture`.
5. Set a **Delay** if you need to get into frame, then hit **ACTION** (F3). It flips to **CUT** (F4), which stops everything and restores what it changed.
6. **Save** the setup as a **Scene** to reload later.

> ACTION does nothing visible unless at least one **arm** box is ticked. If both Recorder-capture and Recorder-play are armed, **capture wins**.

### Director hotkeys

Require `DirectorHotkeysEnabled` and no text field focused. F3–F8 need the
Director Console **or** the Actor Mover open; F2 works even when closed.

| Key | Action |
|---|---|
| **F2** | Toggle the Director Console |
| **F3** | ACTION (or CUT if a take is already running) |
| **F4** | CUT |
| **F5** | Reset actors to their marks |
| **F6** | Toggle pose "onion-skin" ghosts (`PathShowOnionSkin`) |
| **F7** | Flycam take play / pause |
| **F8** | Set marks |
| **1–9** | Punch camera-switcher slots 1–9 (see [Switcher](#33-director-camera-switcher)) |

### Anim tab — animation inspector & player

The **Anim** tab inspects and plays animations on a cast member — handy for finding a walk/pose UUID or previewing an animation before you use it.

- The list shows every animation currently signaled on the selected cast member — **UUID**, **priority**, and an **On** marker for what's actually playing. Right-click a row for actions.
- **Copy UUID** grabs the selected animation's asset ID; **Set as loco** uses it as the actor's custom walk (pairs with the Actor Mover custom-anim settings).
- **Play local** / **Stop local** play an animation on the selected member **client-side only** — nobody else sees it. Paste any animation asset UUID into the field to preview it on loop; `DirectorAnimPlayLocalPriority` sets the playback priority.
- **Animation Explorer** (button) opens a browsable list of recently-seen animations to pull UUIDs from.

### Takes tab

The **Takes** tab is the [Flycam Recorder](#37-flycam-recorder--orbit) embedded in the Console — record, play back, and scrub your camera takes without leaving the Director Console.

---

## 3. Cameras

### 3.1 Cinematic Camera

**Open:** Me menu → *Cinematic Camera…*, or Director Console → Camera → Cinematic.
**Enable:** `CinematicCamEnabled`. **Pick a move:** `CinematicCamMode`.

**How to use it:**
1. To frame someone other than you, **select that avatar in world** first.
2. Tick **Enable Cinematic Camera** (`CinematicCamEnabled`) — it takes over the camera (runs on *you* by default).
3. Pick a shot in **Mode** (`CinematicCamMode`).
4. Tick **Target selected avatar** (`CinematicCamUseSelected`) to follow the selected avatar (falls back to you if the selection isn't a valid avatar).
5. Soften with **Smoothing**; optionally tick **Handheld operator** (see [Camera Operator](#34-camera-operator--camera-shake)).

> Nothing happening? The master is off, or an armed Switcher / running flycam is holding the camera. *Static* modes just frame with no motion — that's expected.

A large library of ready-made camera moves that frame a subject for you. Choose a
target with `CinematicCamUseSelected` (use the selected avatar) or let it use
your focus; frame the head with `CinematicCamLookAtHead`; soften motion with
`CinematicCamSmoothing`; add a tilt with `CinematicCamDutchAngle`; and route the
result through the handheld [Camera Operator](#34-camera-operator--camera-shake)
with `CinematicCamUseOperator`.

**The modes.** Moving moves: Orbit, Fly Hover, Sweep, Crane, Dolly Zoom
(Vertigo), Push-In, Low Hero, Overhead, Over-the-Shoulder, Crash Zoom, Slow Zoom,
Whip Arc, Arc Move, Reveal Rise, Pull-Back, Two-Shot, Lead Follow, ECU Eyes, Long
Lens, Spiral, Pedestal Rise, Barrel Roll, Corkscrew, Pendulum, Contra-Orbit,
Fisheye Lunge, Floor Skimmer, Boost Rise, Boom Over, Top Spin, Turntable Crane,
Floating Close-Up, Tilt Whip, Body Helix Reveal, Descent Pedestal, Parallax
Slide, Figure-8, Detail Sweep, Cable Cam Fly-by, Breathing Hold, plus **Bone
Lock** (rides a named joint). Locked framings: Static Wide / Medium / Close /
Profile L / Profile R / Low Hero / High Angle / Full Body.

Each move has its own parameters (radius, height, speed, duration, etc.) exposed
in the panel and as `CinematicCam<Mode>…` settings.

### 3.2 Motion Start (how moves begin)

The moving modes above no longer always begin from the same fixed spot circling
the same way. **Motion Start** controls where a move begins and which way it
travels:

| Setting | What it does |
|---|---|
| `CinematicCamMotionStartMode` | Where the move starts: **SubjectFacing** (default — relative to the avatar's facing), **Classic** (the old fixed world-angle), **CameraRelative** (eases out of your current view), **Explicit** (an exact angle), **Random** (varied per take) |
| `CinematicCamMotionStartOffsetDeg` | The angle offset for SubjectFacing/Explicit — e.g. `0` = in front, `90` = to the side, `180` = behind |
| `CinematicCamMotionDirection` | Travel direction: **Auto**, **CW**, **CCW** (or, for lateral tracks, which way it slides). Also **Random**. |
| `CinematicCamMotionSeed` | Seed for the Random options. Same seed + same shot = the **same** result every replay, so a locked take is reproducible. |

Default is **SubjectFacing / Auto**, so a move starts sensibly relative to your
subject rather than a fixed compass point. Choose **Classic** to restore the old
behavior exactly.

### 3.3 Director Camera Switcher

**Open:** Director Console → Camera → **Switcher**.

A 12-slot camera "bank" for multi-angle coverage — like a vision mixer. Each slot
holds a camera mode plus a subject and an optional custom framing
(yaw/pitch/distance/height/FOV).

**How to set up a multi-cam shoot:**
1. Set your subjects: **Set from selection** for **Subject A** (and B/C/D as needed — B is the second body for Two-Shot / OTS).
2. For each slot 1–12 (click it — while disarmed this only *edits* the slot): choose its **mode**, a **Subject**, and an optional **Label**.
3. To store a hand-framed angle: line up the view, tick **Custom angle**, click **Capture current view**, then fine-tune yaw/pitch/distance/height/FOV.
4. Tick **Armed** (`DirectorSwitcherArmed`) — now the slots go live.
5. Cut manually by clicking a slot or pressing **1–9**, or tick **Auto** for hands-free cutting (mark slots **Use in auto**, set **Every** + **Jitter**, Random/Sequence).

> Slots only *edit* until **Armed** is on, and the number keys need the console **open and armed**. A running flycam keeps the camera unless **Override flycam** is on.

- **Punch a slot live:** number keys **1–9** (slots 1–9). This requires the
  **Director Console open**, `DirectorSwitcherArmed` on, and no text field
  focused.
- **Auto-director:** `DirectorSwitcherAuto` cuts between slots on its own — either
  in sequence (`DirectorSwitcherSequence`) or shuffled — every
  `DirectorSwitcherIntervalSec` (± `DirectorSwitcherJitterSec`), seeded by
  `DirectorSwitcherSeed` so it's repeatable.
- **Cuts vs. eases:** hard cut by default; enable `DirectorSwitcherEaseCuts` for a
  timed glide (`DirectorSwitcherEaseSec`, with a selectable curve /
  custom Bézier).
- **Bullet-time on a cut:** `DirectorSwitcherEaseFreeze` slows or freezes world
  time across the ease for a "bullet-time" transition (works with
  [Temporal Capture](#5-temporal-capture-world-time-scale)).
- **Subjects:** each slot can target the cast's **A/B/C/D** marks.
- `DirectorSwitcherOverridesFlycam` decides whether the switcher takes priority
  over a running flycam take.

### 3.4 Camera Operator / Camera Shake

**Open:** Director Console → Camera → **Operator** (shown as "Camera Shake").

Adds believable **handheld** movement on top of whatever camera is running —
breathing, sway, footstep bob, and reactive lag, instead of a robotic glide.

**How to use it:**
1. To add it to a **cinematic** shot, tick **Handheld operator** (`CinematicCamUseOperator`) on the Cinematic panel — *or* tick **Apply to active camera** (`CameraShakeApplyActive`) to add shake to whatever camera is live.
2. Pick a one-click **Shake preset** (Locked Tripod, Subtle Handheld, Documentary, Shoulder Rig, Run-and-Gun, Vehicle, Drone Float, Vérité, Heartbeat) — it fills in the controls below.
3. Adjust **Master** / **Reactivity** and the per-axis authority sliders to taste.

> Heads-up: **Use on live flycam** (`FlycamOperatorEnabled`) only shakes the *joystick flycam* — it does **not** add shake to the cinematic camera. For that, use **Handheld operator** or **Apply to active camera**.

- **Locomotion mode** (`FlycamOperatorLocomotionMode`): Legacy, Creep, Walk, Run,
  Drive, Float, Unsteady, or Auto (picks by speed).
- **Style / Profile** (`FlycamOperatorStyle`, `FlycamOperatorProfile`) pick the
  overall feel; `FlycamOperatorReactivity` sets how quickly it chases motion.
- **Per-axis strength**: `FlycamOperatorGainSurge / Sway / Heave / Roll / Pitch /
  Yaw / FOV`, plus breathing (`Breath…`) and step bob (`StepBob`, `StepRoll`).
- Built-in **shake presets** are available in the panel for one-click looks.

### 3.5 Frame & Lens

**Open:** Director Console → Camera → **Frame**.

**How to use it:**
1. Pick a **Target format** (`CinematicFrameAspectRatio`) — 16:9, 2.35, 2.39, 4:3, 9:16, Custom, etc.
2. For a real lens-change look, tick **Reframe as a lens** (`CinematicFrameLensEnabled`) — this unlocks **Base lens (mm)** (`CinematicFrameFocalLengthMM`); wider formats reveal a wider field like an anamorphic gate.
3. Tick **Show format guide** (`CinematicFrameGuideEnabled`) to compose to the frame — this unlocks **Guide style** and **Bar opacity**.

> Lens mode only affects the **cinematic/director** camera, not the normal mouse cam. The guide is screen-only — it never crops or bakes into a snapshot, so you crop to the format in post.

- **Lens/aspect** (`CinematicFrameLensEnabled`): pick an aspect
  (`CinematicFrameAspectRatio` / `CinematicFrameCustomRatio`) and a focal length
  (`CinematicFrameFocalLengthMM`); the framing changes like swapping a real lens.
- **Framing guide** (`CinematicFrameGuideEnabled`): a non-destructive letterbox /
  guide overlay to compose with — adjust `CinematicFrameGuideOpacity` and
  `CinematicFrameGuideStyle`. It's a viewport guide only; it doesn't crop your
  actual snapshot.

### 3.6 Auto-Reframe

`CinematicAutoFrameEnabled` lets the cinematic camera fit and place the subject
from its skeleton for the chosen shot type (adjust with `Fill`, `ComposeLine`,
`DistanceTrim`, and the `…Settle…` easing keys). Off by default; turn it on when
you want the camera to size the shot for you.

### 3.7 Flycam Recorder & Orbit

**Flycam Recorder** — Me menu → *Flycam Recorder…*, or Director Console →
Recorder. Record a free camera take, then play it back or scrub it.

**How to record & play:**
1. Drive the camera (flycam, mouse, or cinematic), click **Record**, perform the move, click **Stop**.
2. Click **Play** (or **F7**) to play it back; drag the **Time** slider while stopped to scrub to any moment.
3. **Save… / Load…** write hand-editable XML takes; **Clear** discards.
4. For a moving subject, set an **Anchor** (My avatar / selected object / camera-at-start) and tick **Follow anchor** so the move rides the anchor instead of fixed world space. Record a clean pass, then add texture with **operator on playback** (`FlycamRecUseOperator`).

- **F7** plays / pauses the take (also drivable from the console).
- `FlycamRecSampleRate` (samples/sec), `FlycamRecSpeed` (playback rate),
  `FlycamRecSmooth` (Bézier smoothing), `FlycamRecLoopMode`.
- **Relative playback:** `FlycamRecAnchorMode` / `FlycamRecFollowAnchor` let a
  recorded move follow a moving anchor (e.g. a walking subject) instead of playing
  back in fixed world space.

**Flycam Orbit** — Me menu → *Flycam Orbit…*. A simple, always-available orbit:
`FlycamOrbitEnabled`, with `MinRadius` / `MaxRadius` / `Zoom` / `Level` /
`OffsetLeft` / `OffsetUp` / `Smoothing`.

---

## 4. Look-At / Gaze

**Where:** the Director Console (the Look-at controls, by the cast).

Makes an avatar look at the camera (or a target).

**How to use it:**
1. Add the avatar(s) to the **cast** (right-click → Director → Add to Cast).
2. Select the cast member(s) and click **Set selected** — an **L** appears next to them in the cast list.
3. Tick **Look at camera** (`DirectorLookAtCameraEnabled`) — the master gate.
4. Choose a **Mode** (`DirectorLookAtCameraMode`): **Gaze (head/eyes)** or **Turn body**.

> Nothing turns unless members are **flagged (L)** *and* the master is on — the flag alone does nothing. Ghosts and animesh are never affected (real cast avatars only).

The two modes (`DirectorLookAtCameraMode`):

- **Gaze (head/eyes)** — turns the head and eyes only. Tune with
  `DirectorLookAtCameraStrength`, `DirectorLookAtCameraHeadEye` (head-vs-eyes
  balance), `DirectorLookAtCameraTorso` / `…TorsoAmount` (let the chest follow),
  `DirectorLookAtCameraSmoothing`, and `DirectorLookAtCameraEaseTime`
  (responsiveness).
- **Turn body** — rotates the whole avatar to face the target (plays the AO's turn
  animation when appropriate).

The head no longer snaps when a target passes directly behind the avatar — it
sweeps around smoothly. The maximum turn speed of that sweep is
`BDMergeGazeHeadSlewRate` (degrees/second).

---

## 5. Temporal Capture (world time scale)

**Open:** Me menu → *Temporal Capture…*, or Director Console → Time.

Slows down or speeds up **world time** — animations, physics, particles — so you
can shoot slow-motion or time-lapse, or hold a "bullet-time" freeze.

**How to do slow-mo:**
1. Click a preset — **0.25x**, **0.5x**, or **Freeze** (these auto-switch to Manual) — or set **Mode = Manual** and drag **World speed** (`TemporalWorldScale`).
2. Confirm the **Drive** boxes for what should slow together: animation, object motion, texture animation, particles.
3. **1x** returns to Live (in Live mode the scale does nothing).

> The **camera stays real-time by design** so you can move over a slowed world. The panel's **Freeze** pauses the cinematic clocks — it's *not* the snapshot floater's "Freeze World" (which halts object/drawable motion).

- `TemporalWorldScale` — the time multiplier (below 1 = slow motion, above 1 =
  fast).
- `TemporalMode` / `TemporalOutputFPS` — capture behavior.
- Pick exactly what the time scale drives: `TemporalDriveAnimation`,
  `TemporalDriveObjects`, `TemporalDriveTextureAnim`, `TemporalDriveParticles`,
  `TemporalDriveCamera`.

This is what the switcher's [bullet-time cut](#33-director-camera-switcher) uses
under the hood.

---

## 6. Actors & Props (motion paths)

### Actors — Actor Mover

**Open:** Me menu → *Actor Mover…*, or Director Console → Move. Walk avatars along
a smooth **Catmull-Rom** waypoint path.

| Command | Does |
|---|---|
| `/pathadd` | Add the current position as a waypoint |
| `/pathwalk` | Walk the actor along the path (needs ≥ 2 waypoints) |
| `/pathloop` | Toggle looping the path |
| `/pathclear` | Clear the path |

Tune with `ActorMoverSpeed`, `ActorMoverDistance`, `ActorMoverHeading`,
`ActorMoverEndMode`, `ActorMoverWalkNominal`, `ActorMoverSync`,
`ActorMoverShowHeading`, and an optional custom walk animation
(`ActorMoverUseCustomAnim` / `…CustomAnim` / `…CustomAnimPriority`). The in-panel
path editor lets you edit nodes visually.

### Props — Prop Mover

**Open:** Me menu → *Prop Mover…*, or Director Console → Props. Drive **objects**
along a path the same way. Results echo to local chat with a `[Prop Mover]` prefix.

| Command | Does |
|---|---|
| `/objpathadd` | Add a waypoint |
| `/objpathdrive` | Drive the object along the path |
| `/objpathstop` | Stop |
| `/objpathloop` | Toggle looping |
| `/objpathclear` | Clear the path |
| `/objpathspeed` | Set speed |
| `/objpathskid <deg>` | Add a drift/skid angle |

---

## 7. Ghost Studio (clones)

**Open:** Me menu → *Ghost Studio…*, or Director Console → Ghosts. Make copies of
avatars — for crowds, doubles, and stylized effects.

**How to clone an avatar:**
1. Add the target to the Director **cast** so it appears in the **Source** dropdown (or use **You**). Quick self-clone: type **/ghostdress**.
2. In **Instances**, pick the **Source** and a **Type** (Overlay ghost or Entity clone), then click **Add**.
3. **Place tab:** set position / Yaw / Scale, or **Place (click ground)** / **To me** / **Drop to ground**; tick **Edit ghosts** to drag them in world.
4. **Style tab:** pick a **Look** and tune it (**overlay ghosts only** — see below).
5. **Pose tab:** **Grab pose now** / **Follow live**, drive mode, anim speed, per-clone look-at.
6. Clear everything with **/ghostclear all**.

**Clone types:** **Overlay ghost** (a lightweight client-side copy) or **Entity
clone** (a fuller clone that can take scene lighting). Add / Duplicate / Delete /
Refresh from the roster.

**Place tab:** position (X/Y/Z), Yaw, Scale (each with a reset button),
place-on-ground by click, snap **To actor** / **To me**, **Face now** / **Keep
facing**, **Drop to ground**, **Align feet**, **Upright**, and **Copy/Paste
transform**.

**Style tab:** Ghost, Clone, Hologram, Wireframe, X-ray, Thermal, Neon Outline,
Silhouette, Toon-Ink, and more. Each clone also has an **animation-speed** spinner
(0.05×–4×).

**Chat commands:**

| Command | Does |
|---|---|
| `/ghostdress` | Clone the targeted avatar |
| `/ghosttest` | Spawn a test clone |
| `/ghostscale <0.05..150> [all\|selected]` | Resize clone(s) |
| `/ghostturn [track\|all\|off\|camera\|me\|here]` | Turn clone(s) to face a target |
| `/ghostlook [off\|camera\|me] [once\|keep] [all]` | Head/eye look-at for clone(s) |
| `/ghostanim <uuid\|mirror\|freeze> [all\|selected]` | Drive clone animation |
| `/ghostchaos <0..1> [all\|selected]` | Add per-clone variation so a crowd isn't identical |
| `/ghostrefresh [all\|selected]` | Rebuild clone(s) from the source |
| `/ghostclear [all\|selected\|test]` | Remove clone(s) |
| `/ghostverify [test]`, `/clonefidelity` | Diagnostics: check how faithfully a clone matches its source |

Useful settings: `GhostStudioTurnRate`, `GhostStudioTurnEase`,
`GhostStudioNameplateMode`, `GhostStudioShowFormationPreview`, and
`GhostDeferredEnable` (route entity clones through scene lighting).

---

## 8. Weather (rain, lightning)

**Open:** Director Console → Weather. **Master:** `AlchemyWeatherEnabled`.

A local weather layer with rain, splashes, surface wetness, mist, lens drops, and
lightning. Save/load your own presets from the panel.

**How to turn it on (order matters):**
1. Tick **Cinematic weather** (`AlchemyWeatherEnabled`) — the master; it un-greys everything.
2. Use the **Controls:** dropdown to switch group (Rain volume / Surface response / Cover-occlusion / Lightning — only one shows at a time).
3. **Rain:** tick **Depth-aware volumetric rain** (`AlchemyWeatherRainEnabled`) and set **Intensity**.
4. **Lightning:** tick **Automatic and manual lightning** (`AlchemyWeatherLightningEnabled`), set **Strikes/minute** (0 = manual only), or click **Trigger** for one strike.
5. Save the look as a named **preset**.

> **Requires Advanced Lighting Model (deferred rendering).** Also: the rain sliders un-grey with the *master*, but you won't see rain until **Depth-aware volumetric rain** itself is ticked. HDR lightning quality wants HDR on to look right.

**Highlights:**

- **Rain:** `AlchemyWeatherRainEnabled`, `…Intensity`, `…Density`, `…FallSpeed`,
  `…MaxDistance`, `…GustStrength`, `…Color`. Ground **splashes**
  (`AlchemyWeatherSplashEnabled` …) and surface **wetness**
  (`AlchemyWeatherWetnessEnabled` / `…Strength`).
- **Atmosphere:** `AlchemyWeatherMist…`, `AlchemyWeatherLensDrops…`, and
  `AlchemyWeatherEEPCoupling` to tie it to your environment preset.
- **Lightning:** `AlchemyWeatherLightningEnabled`, `…Rate`, `…FlashDuration`,
  `…BoltHeight`, `…Brightness`, `…Color`, `…Seed`, plus a manual `…Trigger`. A
  higher-quality path (fractal bolts, cloud sheet flashes, wet glint) is available
  via `AlchemyWeatherLightningQualityEnabled` / `…Tier`.

> Lightning quality effects look best with HDR rendering enabled.

---

## 9. Volumetric Light

Beams of light in the air — sun shafts and glowing projector cones. Two independent
systems, and **both need shadows on** (they're built from the shadow buffers). The
full controls live in **Lightbox** (Me → *Lightbox…*); the Director Console
**Shafts** tab mirrors the projector temporal controls.

### How to turn on sun god-rays

1. Set shadows to **Sun/Moon + Projectors** (`RenderShadowDetail`).
2. In Lightbox, tick **Volumetric Lighting (Godrays)** (`RenderVolumetricLighting`).
3. Tune **Multiplier** / **Resolution** / **Falloff**. Shafts show where shadowed air
   meets sunlit gaps — shoot from inside a structure or under canopy, camera looking
   across the light, sun low.

### How to turn on projector shafts (from set lights)

Order matters — the cones only appear once the light can actually cast a shadow:

1. **Shadows on** — set shadows to **Sun/Moon + Projectors**.
2. **Let projectors cast shadows** — raise **Shadow-casting projectors**
   (`BDMergeMaxSpotShadows`, stock 2 → up to 6); optionally tick **Stable projector
   shadows** (`BDMergeStableSpotShadows`) for deterministic slots.
3. **Have a projector (set) light** in the scene — a light set to *project* a texture
   — within draw distance.
4. **Enable the cones** — Lightbox → **Volumetric Projector Cones**
   (`BDMergeProjectorVolumetrics`), or Director Console → **Shafts** tab →
   **Enable volumetric projector cones**.
5. **Tune** — **Multiplier** is the primary brightness lever; then the controls below.
6. **Stabilise (optional)** — in the Director Console **Shafts** tab, tick **Half-res
   march**, then **Temporal accumulation** (each un-greys the next); nudge Temporal
   blend/reject if moving shafts shimmer.

> **No shafts showing?** Almost always one of: shadows aren't set to include
> Projectors, the light isn't a shadow-casting projector, or there are more
> projectors than shadow slots — raise `BDMergeMaxSpotShadows`.

**The controls (two systems):**

- **Sun / directional god-rays** — `RenderVolumetricLighting` (with
  `RenderVolumetricLightingDirectional`, `…Resolution`, `…Multiplier`,
  `…FalloffMultiplier`). Shafts of sunlight streaming through the atmosphere.
- **Projector / spot light shafts** — `BDMergeProjectorVolumetrics` — visible
  volumetric cones from projector and spot lights. This is the Director Console
  **Shafts** tab, with a deep control set:
  - **Look:** `BDMergeProjectorVolumetricsMultiplier` (brightness), `…Anisotropy`
    (forward/back scatter), `…Density`, `…Tint` / `…TintStrength`, edge `…Rim…`,
    and animated `…Noise…` for drifting dust-in-light.
  - **Ground fog:** `…FogStrength`, `…FogGroundDensity`, `…FogFalloff`, `…FogBase`.
  - **Quality / performance:** `…HalfRes`, `…Resolution`, `…Adaptive`,
    `…ShadowSamples`, `…MaxLuminance`.
  - **Temporal denoise:** `…Temporal` + `…TemporalBlend` / `…TemporalReject` /
    `…TemporalBeamDepth` — accumulates frames so moving shafts stay clean.
- **Froxel volumetrics** — `BDMergeFroxelVolumetrics`, an alternative
  froxel-based volumetric fog path.

**Open:** Director Console → **Shafts** tab (projector volumetrics). The sun
god-rays live in the render settings (`RenderVolumetricLighting…`).

> **Projector shadows (2 → 6):** stock viewers cast at most **2** projector (spot)
> shadows at once; this viewer raises the cap to **6** via `BDMergeMaxSpotShadows`
> (each extra slot renders one more shadow map per frame). Raise it when several
> projector lights must cast shafts/shadows together — otherwise nearby projectors
> can make each other's shafts/shadows drop out. See also `BDMergeStableSpotShadows`.

---

## 10. Render extras

- **Lightbox — expanded post-processing & look** (Me menu → *Lightbox…*). This
  viewer's Lightbox goes well beyond Alchemy's. *Image* tab: the full **tonemapper**
  list — Khronos Neutral, ACES, ACES Boosted, Reinhard, Filmic, GT, AgX, and
  **AMD LPM** (`AlchemyRenderTonemapType`, now the **default**) — with dedicated
  AMD LPM controls (HDR max, exposure, contrast, per-channel saturation),
  color-grade LUT presets (`RenderColorGradeLUT`), exposure, and sharpening
  (CAS / DLS via `RenderSharpenMethod`). *Rendering* tab: reflection-probe
  detail/level, screen-space reflections, mirrors, and hero-probe controls in one place.
- **ReShade bridge (iMMERSE V3)** — `RenderVisibleDiffuseSidecar` (off by default)
  publishes the visible linear diffuse colour + an exactness sidecar so an external
  ReShade add-on can consume the viewer's frame state. It's built to drive the
  **iMMERSE V3** shader suite. Turn it on only if you're running the companion
  ReShade bridge.
- **10-bit output** — `RenderGLContext10bitSDR` (on by default) uses a 10-bit SDR
  framebuffer where supported; `RenderDitherEnabled` dithers the final output to
  hide banding on 8/10-bit displays.
- **Prism Lens** — `PrismLensEnabled` (experimental) renders geometry behind a
  designated prim at a different zoom/FOV. Off by default.
- **Animation Override (AO)** — a built-in AO engine. Command `/ao` (see
  `AlchemyChatCommandAnimationOverride`), with its own panel; overrides stands,
  walks, sits, etc.

---

## 11. Quick reference

### Hotkeys

| Key | Action | Needs |
|---|---|---|
| F2 | Toggle Director Console | `DirectorHotkeysEnabled` |
| F3 | ACTION / CUT | Console or Actor Mover open |
| F4 | CUT | " |
| F5 | Reset to marks | " |
| F6 | Toggle pose ghosts | " |
| F7 | Flycam take play/pause | " |
| F8 | Set marks | " |
| 1–9 | Camera switcher slots 1–9 | Console open + `DirectorSwitcherArmed` |

### Chat commands

`AlchemyChatCommandEnable` must be on.

- **Actors:** `/pathadd` `/pathwalk` `/pathloop` `/pathclear`
- **Props:** `/objpathadd` `/objpathdrive` `/objpathstop` `/objpathloop` `/objpathclear` `/objpathspeed` `/objpathskid`
- **Clones:** `/ghostdress` `/ghosttest` `/ghostscale` `/ghostturn` `/ghostlook` `/ghostanim` `/ghostchaos` `/ghostrefresh` `/ghostclear` `/ghostverify` `/clonefidelity`
- **AO:** `/ao`

### Key enable switches

| Setting | Turns on |
|---|---|
| `CinematicCamEnabled` | Cinematic Camera |
| `DirectorSwitcherArmed` | Camera switcher hotkeys |
| `FlycamOperatorEnabled` | Handheld camera operator |
| `DirectorLookAtCameraEnabled` | Look-at / gaze |
| `AlchemyWeatherEnabled` | Weather |
| `DirectorHotkeysEnabled` | Director hotkeys |
| `AlchemyChatCommandEnable` | Chat commands |

---

## Reporting bugs

This is an open-beta build. When something goes wrong, a log helps a lot.

- **`MachinimaDebugLogging`** (off by default) turns on verbose diagnostic logging
  for the machinima subsystems (cameras, Director, clones, weather, etc.). Leave
  it **off** for normal use so your log stays clean; turn it **on**, reproduce the
  problem, then attach your log when reporting. Warnings and errors are always
  logged regardless of this switch.
- Note what you did (which floater/command/hotkey), what you expected, and what
  happened. A short clip or screenshot of a visual issue is ideal.

Thanks for helping test.
