# Lens Gaze — Phases 1-4 to feature-complete — design/blueprint seed

**Status:** seed for a design/blueprint pass. No source modified. Captured 2026-08-16.
Feeds `doc/LENS_GAZE_ALLPHASES_DESIGN.md` -> Codex (ALL phases) -> Opus review -> build.

## What the user asked (2026-08-16)
Take the look-at-camera / lens-gaze follow feature to **feature-complete** — implement ALL of Phases
1-4 from the existing research — via Codex, in one delivery.

## The authoritative research (READ IT FIRST)
`doc/LENS_GAZE_RESEARCH_FINDINGS.md` — the deep research pass (2026-07-25). Phase 0 is BUILT (the
post-blend procedural gaze system `applyGaze`/`gazePaint` in llactormover.cpp, GAZE_CAMERA mode aiming
at LLViewerCamera origin, called from LLVOAvatar::updateCharacter, works for clones/self/others).
Phases 1-4 remain. This blueprint turns them into buildable specs. Where the research and the CURRENT
code disagree (the research is ~3 weeks old; code has moved), TRUST THE CODE and say so.

## The four phases (from the research, L139-148) — the design must make each concrete
- **Phase 1 — targeting polish** (the biggest realism win):
  - **Torso/head lag differential** — gazePaint currently uses ONE weight for torso/neck/head, so the
    upper body swings as a rigid block. The built-ins use different half-lives (torso ~0.27s vs head
    ~0.15s, LLHeadRotMotion). Make the head lead and the torso trail. This is the core fix for the
    "robotic/stiff" look. Anchor: gazePaint torso->neck->head distribution (llactormover.cpp ~2855-2879).
  - **Dead zone** — neither the built-in nor gazePaint has one; add ~3-5deg no-op so the head stops
    micro-jittering to hold dead-centre on the lens.
  - **Selectable break-off** — past ~120deg ease the gaze weight to 0 and let the animation reclaim the
    head (no owl-neck at extreme camera angles).
- **Phase 2 — clone life signals:** tracking CLONES have dead, unblinking eyes (mEnableDefaultMotions
  =false, no LLEyeMotion). Start ONLY ANIM_AGENT_EYE on clones (the setEntityPhysicsEnabled pattern,
  llghostavatar.cpp:224-248) purely for blink morphs + saccades — safe because the gaze layer overwrites
  eye JOINTS downstream, so LLEyeMotion contributes only its visual-param morphs. Do NOT start
  ANIM_AGENT_HEAD_ROT on clones (it drives mTorso toward root-forward).
- **Phase 3 — breadth & UI:** promote the gaze controls out of the Path tab into the shared panel as a
  superset (enable, target mode, and the new Phase-1/4 knobs). Decide the exact host panel + layout.
- **Phase 4 — safety interlocks:** the R1 bone-lock feedback guard (a gaze on a subject that is ALSO
  the active camera bone-lock target creates a feedback loop — smoothing only slows divergence; a hard
  interlock must SUPPRESS gaze on the active feedback target — see gazeCameraSafe, llactormover.cpp
  ~3024, and the LLCinematicCamera bone-lock). Plus the optional eyeline angular offset (a small
  head-local angular offset so the subject can look just off-lens, or dead-centre for talk-to-camera).

## What the blueprint must decide (per phase, concretely)
- The EXACT parameters (dead-zone deg, torso/head half-lives, break-off deg + ease width, eyeline
  offset range) and whether each is a fixed constant or a settings-backed control.
- The gazePaint math change for the lag differential (per-joint smoothing half-lives), stated
  precisely, preserving the existing anatomical torso->neck->head distribution and the eyes-carry-
  residual behaviour; and how it composes with the dead zone and break-off (order of operations).
- Phase 2: exactly where/how ANIM_AGENT_EYE is started/stopped per clone, tied to gaze-enabled, and the
  teardown (stop it when gaze disabled / clone destroyed) so no dangling anim.
- Phase 3: the host panel (the machinima/Director shared panel), the control set, and the reflow (mind
  the wrapper-height lesson — a grown panel must bump its host wrappers). Keep the existing Path-tab
  entry or move it — decide.
- Phase 4: the interlock's exact condition (when is a subject "the active feedback target") and the
  suppression behaviour (ease gaze weight to 0, non-destructive); the eyeline offset frame (head-local)
  and range.
- Settings keys (al/CineLightRig-style names? this is gaze, so a gaze-specific prefix), scene/persistence,
  and whether any of this round-trips through a Director scene.
- Verification: what is unit-testable (the pure math — dead zone, lag half-lives, break-off ease — if
  any is factored into a pure helper) vs in-world only (the R1 interlock, mesh-head behaviour R2).

## Risks (from the research L150+)
- **R1 feedback loop** — gaze-on-the-bone-lock-target oscillation. The Phase 4 interlock is the
  mitigation; the blueprint must specify it precisely, because a wrong interlock ships an oscillating
  camera. Highest risk.
- **R2 mesh heads** — fitted-mesh heads may not respond identically to joint rotation; needs in-world
  proof. Note it; do not block on it.
- Feedback with the ReShade motion/gaze-back-into-camera interlocks (gazeCameraSafe) — Phase 4 must not
  break the existing cinematic-camera gaze-feedback guards.

## Constraints
- Label PROVES/IMPLIES/INFERS; cite file:line. No source modified by the design.
- Ship-whole: all 4 phases + UI + settings + interlocks in ONE delivery (that is the user's
  "feature-complete" ask). If any sub-item genuinely must defer, say so up front with the reason.
- Concrete enough to become a Codex brief: named files/functions/settings/params, the panel layout +
  reflow, and an OFF-LIMITS list. This touches the avatar animation path (llactormover.cpp gazePaint,
  llvoavatar updateCharacter, clone anims) — call out the shared-state / every-avatar-path risks
  explicitly (Phase 0 already runs for every avatar class).
