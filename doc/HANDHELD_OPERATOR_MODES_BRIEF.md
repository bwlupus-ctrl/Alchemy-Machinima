# Handheld Camera Operator — LOCOMOTION MODES + full DOF control

**Design + implement.** Repo `I:\alchemy-machinima`, branch `develop`, HEAD `3c7b1b7768c`.

## What the user asked for

A handheld operator with **named locomotion modes** — *driving, walking, creeping, run,* and
**creative** modes — each carrying **setting tweaks that match**, and with **all degrees of freedom
over the camera** available.

This is for machinima. The user is a director shooting in-world; the operator is the difference
between "game camera" and "someone is holding a camera in this world".

## What already exists — READ IT FIRST, do not redesign it

`indra/newview/llcameraoperator.h` (88 lines), `indra/newview/llcameraoperator.cpp` (434 lines).
A VCHH port, driven per-frame from the flycam path.

**Output is already 6DOF + FOV** (`LLCameraOperatorOutput`):
`mPosOffset` (3 axes), `mRoll` / `mPitch` / `mYaw`, `mFovMul`.

**Input** (`LLCameraOperatorInput`): `mDeltaTime`, `mLinearVel`, `mAngularVel`.

**Two existing axes, both multiplicative:**
- **Persona / "Operator Style"** (`FlycamOperatorStyle`, 0-8) — a RIG/OPERATOR archetype:
  0 Custom, 1 Tripod/Sticks, 2 Subtle Handheld, 3 Documentary, 4 Run & Gun, 5 Shoulder Rig,
  6 Gimbal Float, 7 Verite (Chaotic), 8 Breathing Only. Fields: `ampMul freqMul hiMul transMul
  rollMul breathMul energyGain onsetGain settleGain drag smoothing walkCouple walkRate motionCalm`.
- **Profile** (`FlycamOperatorProfile`, 0-7), blended in by `FlycamOperatorProfileInfluence` (0.7).

**~40 live settings**, already including a gait block that is exactly what locomotion needs:
`FlycamOperatorForceWalk`, `WalkCadence`, `CadenceDrive`, `ForwardWalkBias`, `LateralWalkBias`,
`StepBob` (m), `LateralStep` (m), `StepRoll` (deg), `GaitCoupling`, plus
`RefLinearSpeed` (3 m/s) and `RefAngularSpeed` (60 deg/s) as the speed-metric normalisers.

**⚠️ CORRECTION — there IS a UI, and the pattern you need already exists. Use it.**

`indra/newview/skins/default/xui/en/panel_cinecam_params.xml` + `alpanelcinecamparams.cpp`
(`ALPanelCineCamParams`, class `panel_cinecam_params`, height 262). Its own header says:

> *"shared header row, preset row, resets, and the 42 per-mode auto-hiding parameter panels.
> Embedded by BOTH the standalone Cinematic Camera floater and the Director Console's Camera tab;
> every control is settings-backed so instances stay in sync."*

So the project's superset rule is ALREADY satisfied by construction: ONE shared `LLPanel`, injected
via `LLPanelInjector<ALPanelCineCamParams>`, embedded by `floater_cinematic_camera.xml` (a thin
shell) and by `floater_director.xml:883` (the console's Camera tab). **Do not invent a second
pattern. Extend this one.**

**Critically: "42 per-mode auto-hiding parameter panels" means the exact mechanism locomotion modes
need is already built and proven in this panel** — select a mode, its parameter panel appears, the
others hide. Reuse it rather than designing a new disclosure scheme.

What actually exists for the operator today is a SINGLE checkbox:
`CinematicCamUseOperator`, labelled "Handheld operator", at `panel_cinecam_params.xml:111` --
*"Route the cinematic camera through the procedural handheld operator for organic texture."*

**That is the entire editing surface. On/off.** None of the ~40 `FlycamOperator*` settings, neither
Persona nor Profile, are reachable from any UI. The user's words: *"it will need a new UI to
accommodate editing features."* They are right, and they have seen the console — design for the
space that panel actually has (height 262, embedded in a tab alongside Bone Lock and Flycam Orbit
sections), not for a blank canvas.

## The design questions — answer these before writing code

1. **Is LOCOMOTION a third axis, or does it subsume one of the existing two?**
   Persona = *what rig the operator is using*. Locomotion = *what the operator is physically doing*.
   Those are genuinely orthogonal (a shoulder rig can be walking or driving). But **three
   multiplicative axes is a combinatorial trap** — Persona x Profile x Locomotion, each scaling the
   same ~40 settings, will produce nonsense corners that nobody can debug and the user cannot
   predict. Decide and justify: third axis, replacement for Profile, or a preset layer that WRITES
   the settings rather than multiplying them at runtime. Say which you'd want to live with.
2. **What exactly distinguishes each mode physically?** Not vibes — parameters. For each of
   *driving, walking, creeping, running/run-and-gun* and at least two *creative* modes, specify the
   full parameter set with values, and say WHY each differs. Anchor them in real camera physics:
   - **Driving** — vehicle-borne. High-frequency road buzz, near-zero gait, suspension-like low-freq
     heave, the operator is seated and braced. Lateral lean on turns coupled to angular velocity.
   - **Walking** — cadence-locked step bob, lateral sway, heel-strike roll, moderate settle.
   - **Creeping** — slow, deliberate, braced. Very low amplitude, low frequency, long smoothing,
     high motion-calm; tension rather than shake. Breath becomes proportionally MORE visible
     because everything else is damped.
   - **Running** — high amplitude and frequency, strong onset/settle, aggressive gait coupling.
   - **Creative** — the user asked for these explicitly. Propose a set worth having (e.g. drone/
     float, crash-zoom punch-in, snap-whip, drunk/dutch, breathing-only lock-off) and say what each
     is FOR on a shoot.
3. **"All degrees of freedom over the camera" — interpret this and propose the concrete control
   surface.** The output is already 6DOF+FOV. The most likely reading is that the user wants
   **per-DOF authority**: independent gain/enable for surge/sway/heave and pitch/yaw/roll and FOV,
   per mode, so a mode can be "all tilt, no roll" or "translation only". Confirm or propose better.
   State how per-DOF gains compose with the existing Persona/Profile multipliers WITHOUT the
   combinatorial trap in (1).
4. **Speed-driven auto-blend.** The operator already computes a normalised speed EMA (`mSpeed`) from
   `mLinearVel`/`mAngularVel` against `RefLinearSpeed`/`RefAngularSpeed`. Should locomotion mode
   **auto-select or auto-blend** from actual speed (creep -> walk -> run as the camera accelerates),
   with manual override? That is a big usability win for a solo director and reuses machinery that
   already exists. Design the hysteresis so it cannot flutter at a threshold mid-shot.
5. **Determinism and repeatability.** `FlycamOperatorSeed` exists. Machinima needs the SAME shot
   twice. Confirm a given seed + mode + path reproduces identical motion, and if not, what to fix.
   This matters more than it sounds — the user re-shoots takes.
6. **Interaction with existing systems.** The Flycam Recorder records/plays back camera paths, and
   there is a Cinematic Camera with bone-lock and automated motion patterns. Does the operator run
   ON TOP of recorded playback (adding handheld feel to a clean path — probably desirable) or is it
   baked at record time? State the intended composition order and any conflict with bone-lock.

## UI — required, not optional

⚠️ **Project rule: the Director Console is a SUPERSET of all machinima floaters.** Anything editable
in a standalone floater MUST also be editable in the Director Console; prefer extracting a shared
`LLPanel` over duplicating. Mirror any new floater/control into the console in the same change.

The rule is already satisfied structurally (one shared panel, two hosts) -- so the work is to EXTEND
`panel_cinecam_params.xml` / `ALPanelCineCamParams`, following its existing auto-hiding per-mode
panel mechanism, NOT to build a parallel UI.

Propose: the locomotion mode selector, per-DOF controls, and which tunables surface vs stay
debug-only. Keep the full ~40 settings reachable but do not put 40 sliders in front of a director
mid-shoot. Respect the existing preset row (Preset / Save / Del) and the Reset Mode / Reset All
buttons -- new modes must participate in preset save/load and in both resets, or the panel becomes
inconsistent with everything else in it.

Note the Camera tab already carries Cinematic Camera + Bone Lock + Flycam Orbit sections. Say
plainly if the operator's editing surface needs its own collapsible section, its own sub-tab, or a
separate floater that the console also embeds -- and justify it against the space that tab has.

## Constraints

- Follow the existing code's idiom: `LLCachedControl` statics, `vc_*` helpers, VCHH naming.
- New settings go in `indra/newview/app_settings/settings.xml` with real Comment strings.
- **Default behaviour must not change** for anyone who does not touch the new controls. State how
  you guarantee that.
- The user cannot be handed anything that needs a rebuild to iterate on feel — prefer runtime
  settings over compile-time constants for anything tuning-related.

## What to produce

1. Answers to the six design questions, decided — not a menu.
2. The full per-mode parameter tables with actual values.
3. Complete, mechanically applicable code: `llcameraoperator.h/.cpp`, `settings.xml`, and the XUI +
   panel wiring including the Director Console mirror.
4. Implementation order, and what can ship independently.
5. What the user should look at in-world to judge each mode, phrased as a shot they'd actually take.

The workspace may be READ-ONLY for you — put everything in your FINAL MESSAGE as text, do not try to
write files. Be opinionated about feel: the user is a director, and "technically correct but reads
like a game camera" is a failure.
