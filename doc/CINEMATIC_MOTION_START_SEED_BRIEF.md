# Cinematic Camera — configurable motion start (start-azimuth / direction / seed)

## Problem

The Cinematic Camera premade movement modes are not intelligent about where and
which way their motion begins. Every orbital and lateral mode derives its
angle/position purely from `mPhase`, and `mPhase` is reset to `0` at the start of
every shot (`llcinematiccamera.cpp:2417`). Consequences:

- Orbital modes always start at **region-absolute azimuth 0** (world +X). That is
  "the front" only by coincidence of where the region axes point — it is unrelated
  to the subject facing or to where the camera currently is.
- Orbital modes always rotate in a **single fixed direction** (CCW, because
  `a = phase * speed` only increases).
- Lateral/truck modes always start at the **same fixed end** and travel the **same
  way**.
- Repeated takes of the same mode are byte-identical — no variety, no way to say
  "this time start from the side / behind" or "go right-to-left instead".

The user wants these to be configurable and smarter: a start-azimuth choice, a
direction choice (left->right or right->left), and a seed so takes can vary while
staying reproducible.

## Evidence — exact anchors (all in `indra/newview/llcinematiccamera.cpp`)

Driver:
- Shot (re)start block, `mPhase = 0.f`, `mTripodPos = camera origin`: ~2413-2423.
- Phase advance (switcher = presentation-time deterministic; standalone = dt accumulate): 2430-2447.
- `center` / `focus` computed from the avatar: 2450-2467.
- Mode dispatch switch: 2475+.

Orbital family (angle = `phase * speed * DEG_TO_RAD`, `cos/sin`, azimuth 0, one direction):
- `patternOrbit` 790-801  (`a = phase*speed*DEG; center + (cos a*r, sin a*r, h)`)
- `patternCrane` 852-863
- `patternSpiral` 1228-1241
- `patternTurntable` 1499
- `patternFigureEight` 1631
- also rotational: Corkscrew, ContraOrbit, TopSpin, BoomOver, BarrelRoll, Descent, BodyHelix (verify each in the header list `llcinematiccamera.h:165-212`).

Lateral / truck family (start one fixed end, travel one fixed way):
- `patternSweep` 824-850 — already has `CinematicCamSweepHeading` (default 0) and ping-pong that starts at `t=0` (one end).
- `patternParallaxSlide` 1613
- `patternCableCam` 1679
- `patternDetailSweep` 1651
- `patternFloorSkimmer` 1423

Precedents to REUSE (the codebase already knows how to anchor to the subject and pick a side):
- `cc_avatarYaw(LLVOAvatar*)` ~871 — region-frame yaw of the avatar facing.
- `patternPedestal` uses `cc_avatarYaw(av) + heading*DEG` (1256).
- `patternBodyHelix` uses `cc_avatarYaw(av) + revolutions*TWO_PI*u` (~1575).
- Per-mode direction knobs that already exist: `CinematicCamOTSSide` (1265, 1=right/0=left), `CinematicCamSweepHeading` (829), `CinematicCamPedestalHeading` (1252).

## Feature to build

A single shared "Motion Start" configuration that all move-around modes honor.

1. **Start azimuth mode** (`CinematicCamMotionStartMode`, enum):
   - `0 Classic/Absolute` — today's behavior (region azimuth 0). Kept so nothing is lost.
   - `1 SubjectFacing` — start relative to the avatar facing, plus an offset in degrees
     (`CinematicCamMotionStartOffsetDeg`, default 0 = in front; 90 = side; 180 = behind).
     Reuse `cc_avatarYaw`.
   - `2 CameraRelative` — start from where the camera already is at shot start
     (continuity: the move eases out of the current view instead of teleporting to azimuth 0).
   - `3 Explicit` — an absolute region azimuth from `CinematicCamMotionStartOffsetDeg`.
   - `4 Random` — seeded (see seed rule below).
2. **Direction** (`CinematicCamMotionDirection`, enum): `0 Auto`, `1 CW`, `2 CCW`
   (for lateral modes CW/CCW read as one travel sign vs the other), `3 Random` (seeded).
   `Auto` = pick a sensible sign (e.g. the shorter turn, or turn away from the current
   camera so the subject is not immediately crossed) — your call, document it.
3. **Seed** (`CinematicCamMotionSeed`, S32): `0` = auto-derive deterministically from the
   shot identity; nonzero = fixed seed. `Random` start/direction MUST be a **pure function
   of (seed, mode, and the switcher slot/shot index)** — NO `rand()`, NO wall-clock, NO
   `ll_frand` seeded by time. This preserves the Director Switcher / Director playback
   determinism (the presentation-time path at 2436-2438). A locked take must replay identically.

## Mechanism guidance

- Add a small helper that returns `(F32 a0 /*start-angle offset, radians*/, F32 dir /* +1 or -1 */)`
  from the settings above plus a per-shot start snapshot.
- Capture the shot-start state ONCE when the shot (re)starts. In the `mPhase == 0` reset
  block (~2417) you already have the camera origin (`mTripodPos`, 2420). Compute the
  camera azimuth relative to `center` on the first frame of the shot (lazily, once `center`
  is known at 2458, gated by a new `mMotionStartCaptured` flag). Store new members
  `mMotionStartAzimuth` and `mMotionDir`. Reset the flag whenever the shot changes.
- Orbital modes: replace the bare `a = phase*speed*DEG_TO_RAD` with
  `a = a0 + dir * phase*speed*DEG_TO_RAD`. Thread `a0`/`dir` in (either as new params, or
  read the cached members inside the pattern helpers — pick the cleaner refactor).
- Lateral modes: derive the start end and travel sign from `a0`/`dir`; for `patternSweep`
  COMPOSE with the existing `CinematicCamSweepHeading` (do not fight it — global offset adds).
- Reconcile with existing per-mode heading/side settings so a user who set `SweepHeading`
  or `OTSSide` still gets what they set.

## Defaults (the user wants smarter, not fixed)

Default to the intelligent behavior, keep the old behavior selectable:
- `CinematicCamMotionStartMode` default = `2 CameraRelative` (begin from the current view).
- `CinematicCamMotionDirection` default = `0 Auto`.
- `CinematicCamMotionSeed` default = `0`.
- `CinematicCamMotionStartOffsetDeg` default = `0`.

If `CameraRelative` is unstable for a specific mode, fall that mode back to `SubjectFacing`
and note it. State clearly in your summary that the default look of existing shots changes,
and that `Classic/Absolute` restores it exactly.

## Constraints

- All new settings go in `indra/newview/app_settings/settings.xml` with the defaults above.
- Deterministic under the switcher/director (presentation-time). No wall-clock randomness.
- Do not touch the scheduler, the ease/cut logic, or Flycam. Motion modes only.
- Build must stay clean (this fork builds `newview` via MSBuild).
- Add a compact "Motion Start" UI group (start-mode combo, offset spinner, direction combo,
  seed spinner) to the Cinematic params panel
  `indra/newview/skins/default/xui/en/panel_cinecam_params.xml`. Per the project superset
  rule this panel is already surfaced inside the Director Console Camera tab, so that covers
  the console too — keep it minimal.

## Deliverable

Implement it, then output a concise summary: every file and function changed, each new
setting and its default, how `a0`/`dir` is now threaded through EVERY rotational and lateral
mode (completeness check — list them), and exactly how determinism is preserved for the
`Random` path. Do a self-review pass for: (a) any move-around mode you missed, (b) the
switcher determinism, (c) NaN/degenerate cases when the camera sits on top of `center`
(azimuth undefined) — fall back to Classic there.

---

# PART B — Look-at / gaze head "snap at 180 degrees" fix (bug)

Implement this in the SAME task as Part A (it is a disjoint file, so no conflict).

## Problem

When an avatar (typically the self avatar) is set to look at the camera / a target
(Director look-at, or Actor-Mover gaze with `GAZE_CAMERA`) and the target passes DIRECTLY
BEHIND the avatar (crosses the ~180 degree azimuth), the head violently SNAPS to the
opposite side instead of turning smoothly. The user wants the head to turn gradually but
fast, sweeping across to re-align as the target comes back around.

## Root cause (verified, `indra/newview/llactormover.cpp`, in `gazePaint`)

- Desired world direction is smoothed as a VECTOR: `g.mSmoothDir += (dir - g.mSmoothDir)*a` (3300-3318).
- Head-local euler extracted: `raw_yaw` via `getEulerAngles` (3365-3366).
- Body-aim chase in the yaw domain with `llsimple_angle` unwrap: `g.mBodyAimYaw` chases
  `raw_yaw` the short way (3393-3410). `mBodyAimYaw` is stored WRAPPED and is NOT clamped.
- Cone clamp applied to a COPY only: `yaw = llclamp(mBodyAimYaw, -head_yaw_max, +head_yaw_max)`,
  `head_yaw_max = BDMergeGazeHeadYawMax` default 72 deg (3417-3426), then written to head/neck/torso.

The snap: as the target sweeps behind from one side to the other, `mBodyAimYaw` moves
continuously through +180 and WRAPS to -180. The clamp maps +179 -> +72 (held on the left)
and, the instant it wraps, -179 -> -72 (held on the right). So the APPLIED head yaw jumps
+72 -> -72 in a SINGLE frame = ~144 deg instantaneous snap. The "eases to max and HOLDS"
comment (3413-3415) is only true while the target stays on one side; crossing the
exact-behind seam produces the teleport. Eyes (3461-3501) recompute relative to head world,
so they snap with it.

## Fix

Rate-limit the APPLIED head rotation so a large clamp jump is traversed over time at a
bounded angular speed ("turn gradually fast"), never in one frame:

- Add persistent applied state to the `Gaze` struct (e.g. `F32 mAppliedYaw, mAppliedPitch;
  bool mAppliedValid`). Reseed it (`mAppliedValid = false`) at the SAME points
  `mDirValid`/`mBodyAimValid` are reset (in `applyGaze` ~2816-2817 and the inactive/reseed
  paths) so it never slews in from a stale pose on (re)activation.
- After computing the cone-clamped target yaw/pitch (the block at 3416-3427), do NOT write
  them directly. Slew `mAppliedYaw`/`mAppliedPitch` toward the clamped target using `dt` and
  a max angular rate: shortest-path delta via `llsimple_angle`, per-frame step clamped to
  `rate*dt`. On the first frame (`mAppliedValid` false) set applied = target with no slew.
  Write the SLEWED yaw/pitch to `head_rot_local` (`setEulerAngles`) instead of the raw
  clamped values.
- New setting `BDMergeGazeHeadSlewRate` (deg/s) in `settings.xml`, alongside the other
  `BDMergeGaze*` keys. Default high enough that ordinary in-cone tracking is never visibly
  rate-limited, but the behind-seam teleport is smoothed — suggest ~480 deg/s (tune). You may
  instead engage the limiter only when the per-frame jump exceeds a threshold, so normal
  tracking stays byte-identical; document whichever you choose.
- Because the target is behind, the natural traversal is across the FRONT (through 0), which a
  shortest-path slew from +72 to -72 gives automatically (144 deg through 0, not 216 the back way).
- Reuse the SAME `dt` already computed (presentation-clock aware, 2799-2806) so Temporal
  Capture slow/fast still scales the head turn.

## Constraints (Part B)

- Default-safe: with the default slew rate, normal tracking must look the same as today; only
  the pathological behind-seam snap changes.
- Deterministic under the switcher/presentation clock (reuse existing `dt`, no wall-clock).
- This clamp block is shared by BOTH the Actor-Mover gaze and the Director look-at
  (`applyDirectorLookAt` -> `gazePaint`), so the fix covers the self look-at-camera reported.
- No new joints, do not change the cone limits themselves, roll stays zeroed.

## Deliverable (Part B)

List the struct fields added, the reseed points touched, the exact slew code, the new setting
and its default, and confirm normal tracking is unchanged while the 180-degree snap is gone.
