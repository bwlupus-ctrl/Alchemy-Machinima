# Cinematic Camera — new automated modes (easy-win batch)

Backlog item: *"Add new automated cam patterns (e.g. a SPIRAL that pans UP the body) + other fun
moves."* Non-clone, machinima-facing. **Pure math + settings + UI — no shader, culling, LOD, or
draw-pool work.** Each mode is a small self-contained pattern function, matching the 34 that exist.

## How modes are structured today (follow this exactly)
- Enum `MODE_*` in `indra/newview/llcinematiccamera.h` — currently 1..34, ending `MODE_TILT_WHIP = 34`.
- One pattern function per mode in `llcinematiccamera.cpp`, e.g.
  `LLVector3 LLCinematicCamera::patternOrbit(const LLVector3& center, F32 phase)`. Variants take
  `LLVOAvatar* av`, `LLVector3& focus_io` (to steer framing), and/or out-params for
  `mode_fov_mul` / `mode_roll`.
- Dispatched from the big `switch ((S32)mode)` in `LLCinematicCamera::updateCamera()`.
- Tunables are `static LLCachedControl<F32>` reading `CinematicCam*` settings; add matching entries
  wherever the existing `CinematicCam*` defaults live (`app_settings/settings*.xml`).
- Mode list UI: the `CinematicCamMode` combo in
  `skins/default/xui/en/panel_cinecam_params.xml` — add one `<combo_box.item>` per new mode,
  matching the existing label/name/value convention.
- **Subject scale:** the camera now scales subject-relative geometry via `getUniformScale()` and
  `cc_scaleAboutSubjectBase()` (foot-pivoted) in the post-dispatch block of `updateCamera()`. New
  modes that return a subject-relative position get this automatically through the `default:` case
  — do NOT add a second scaling of your own, and do not add new `case` labels to that scale switch
  unless the mode is genuinely tripod-absolute (like CRASH_ZOOM / SLOW_ZOOM).

## Modes to add (each must be visibly distinct from the existing 34)
1. **Body Spiral / Helix Reveal** ⭐ (the explicitly requested one) — orbit the subject while rising
   from FEET to HEAD over the shot, with the focus point tracking up the body so it ends framed on
   the face. Distinct from: `MODE_SPIRAL` (21, tightening orbit at roughly constant framing),
   `MODE_PEDESTAL` (22, straight vertical rise, no orbit), `MODE_CORKSCREW` (24, fast/aggressive +
   roll). This one is SLOW, body-scale, and its whole point is the focus travelling up the body.
   Tunables: radius, revolutions over the shot, duration/speed, start height (feet) and end height
   (head) — derive head/foot from the subject rather than hardcoding metres where practical, so it
   works on scaled subjects.
2. **Descent / Reverse Pedestal** — the inverse: start above the head, descend the body to the feet,
   gaze staying level. Good for menace/reveal-downward.
3. **Parallax Slide (truck)** — lateral dolly past the subject at fixed height, subject kept
   centred, producing strong foreground/background parallax. Straight-line, not an orbit.
4. **Figure-8 / Lemniscate** — camera traces a horizontal figure-8 around the subject; parallax
   direction reverses each lobe. Excellent over dance loops.
5. **Detail Sweep** — slow, CLOSE lateral drift at a selectable height band (waist/chest/face) for
   showing off outfit/costume detail. Narrow framing, gentle speed.
6. **Step Orbit** — orbits in discrete stepped increments with a hold at each step (stop-motion /
   bullet-time staccato) instead of continuous motion. Tunables: steps per revolution, hold
   fraction.
7. **Cable Cam / Fly-by** — fast straight-line pass on a chord past the subject while the camera
   yaws to keep them framed; drone/sports energy. Distinct from WHIP_ARC (which arcs around them).
8. **Breathing Hold** — a near-static frame with very subtle drift, for dialogue/emotional holds.
   Distinct from `MODE_FLY_HOVER` (3): far smaller amplitude, no wander, meant to read as "locked
   but alive" without the full handheld operator.

Use the existing naming/comment idiom — each enum entry carries a short comment naming the emotion
or use ("euphoria", "introduction", …). Match it.

## Constraints
- Every new tunable gets a sane default and appears in the params panel if that is the existing
  convention for comparable modes; don't leave a mode unusable without editing debug settings.
- Do not renumber or change ANY existing `MODE_*` value — `CinematicCamMode` is a persisted
  setting, so renumbering would silently change a saved user's mode. Append new values from 35 up.
- Do not modify existing pattern functions' behavior.
- Keep the phase/clock conventions of neighbouring modes (`mPhase`, `cc_frac`, `cc_lerp`,
  `cc_fbm`, `cc_avatarYaw`, `cc_lookAt` helpers already exist — reuse them, don't reinvent).
- One-shot vs looping: follow the convention already used by comparable modes (e.g. REVEAL /
  PULL_BACK are one-shot eased moves; ORBIT loops). State each new mode's choice in the report.
- Self-review to 0 must-fix. Report: each mode added with its enum value, its tunables + defaults,
  one-shot vs looping, how it differs from the nearest existing mode, and confirm no existing enum
  value moved and no existing pattern changed.
