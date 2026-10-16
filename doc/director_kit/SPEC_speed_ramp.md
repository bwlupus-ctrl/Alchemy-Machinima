# Speed Ramp — Feature Spec (v2, 10/16)

The v1 draft was written by Gemini 4 Argon (agy, low effort) from a verified facts brief, then proof-reviewed and corrected against the code by Opus. v2 applies the user's decisions of 10/16. Status: **spec only, not scheduled**. It goes after the current build/test and the accessibility themes.

**User decisions (10/16):**
- "BPM would be a number only; we don't care about environmental audio." So: no tap tempo, no audio sync, no beat detection.
- "More curves would be nice."
- "We only want to scale animation speed for the stepper mode, not world."
- "Depending on the setting, it could entail the avatar particles and texture animations."

## 1. Goal
A viewer-local, **continuous** speed curve (0×–8×) for the **avatars the stepper applies to**, for fluid cinematic slow–fast–slow ramps and beat pulses. It scales **animation speed only**, plus (optionally) those avatars' worn particles and texture animations. The world is never scaled: no World Time Scale, no objects, no other avatars.

## 2. Architecture
### 2.1 Who it applies to
- **The stepper's targets, exactly.** The ramp reuses the Stepped page's "Applies to" scope and its filters: Selected / Cast / Everyone in view / Friends / Nearby + radius, Limit distance, Include me, Ghost clones, Max actors, Skip camera rigs, cast groups. There is no second target picker.
- **Per-actor control.** The stepper's per-actor override gains **Ramp: Inherit / Off** (and optionally a per-actor speed offset; see §11).

### 2.2 How the speed is applied: one composer
- `LLMotionController::setTimeFactor` is a bare assignment (`llmotioncontroller.cpp:290`), and two features already write it directly:
  - self slow-motion: `llagent.cpp:290-300`, values 0.2 / 1.0;
  - Ghost Studio speed: `llghostavatar.cpp:1772/1788/3705-3720/3815`, re-asserted on every update.
- Add a tiny **`ALAnimSpeed`** helper that holds named per-avatar layers (`self_slowmo`, `ghost_studio`, `ramp`). The effective factor is the **product** of the layers, written once by the helper.
- Move the two existing writers onto their own layer. After that, **nothing else calls `setAnimTimeFactor`**; a grep check enforces it.
- Worn animesh control avatars get the same product as their wearer, using Ghost Studio's existing re-assert pattern.
- `sGlobalTimeFactor` and World Time Scale (`LLPresentationTime`) are **not touched**.

### 2.3 Optional: worn particles and texture animation
These reuse the stepper's existing per-attachment seams: particles at `llviewerpartsim.cpp:380/1078`, texture animation at `llvovolume.cpp:623`, for worn non-HUD attachments. Where those seams *hold* frames for stepping, the ramp *scales the time delta* by the wearer's ramp factor.
- **Ramp particles** (setting): the particle sources on the wearer's attachments age and emit on the ramped delta.
- **Ramp texture animation** (setting): animated textures on the wearer's attachments advance on the ramped delta.
- Both are off by default. HUDs are never affected.

### 2.4 Foot-lock caveat
- **Remote avatars:** walking ones slide their feet when ramped, because their server position isn't scaled. Dances, poses and idles are fine.
- **Local movers:** for actor-mover and Ghost actors that are walking, the same ramp factor also scales their **traversal clock**, so they stay foot-locked. The seam is `llactormover.cpp:4688`, where traversal already follows a time delta.

### 2.5 With stepping
The ramp and pose-stepping are **independent toggles on the same page**:
- **Ramp on, stepping off:** fluid speed ramp.
- **Ramp on, stepping on:** the stepper holds poses on its grid while the underlying animation runs ramped. That's a valid stylized look; no extra rules.

## 3. Curves (more presets)
Every curve outputs a speed in [0, 8], evaluated on a normalized period u ∈ [0, 1).
| # | Curve | Shape |
|---|---|---|
| 0 | Ease in | Slow start, then up to the peak (smoothstep) |
| 1 | Ease out | Peak, then easing down to low |
| 2 | Ease in-out | low → peak → low, smooth |
| 3 | Slow-fast-slow | Dwell slow, whip fast in the middle, dwell slow |
| 4 | Fast-slow-fast | The inverse: a slow-motion "hang" in the middle |
| 5 | Pulse | Sharp peak on the beat, decay to a valley |
| 6 | Double pulse | Two peaks per period (heartbeat feel) |
| 7 | Sine | Smooth wave between low and peak |
| 8 | Saw up | Speeds up through the period, snaps back |
| 9 | Saw down | Fast at the start, slows through the period |
| 10 | Square (stutter) | Alternates low and peak, with a short smoothing edge so it stays fluid |
| 11 | Bounce | Decaying bounces of speed after the beat |
| 12 | Custom | Up to 8 keyframes (u, s), smooth (Catmull-Rom) |

- **Period:** set by **BPM** (a number, 20–300, default 120) × **Beats per cycle** (1, 2, 4 or 8), OR by **Duration** in seconds when Beat sync is off.
- **Phase:** 0–1, to shift where in the period the peak lands. The cycle starts when the ramp is turned on or **on ACTION** (§5).
- **Stay on beat** (default ON for beat-synced curves): the curve's average over each period is exactly 1.0×, so animation time after every cycle equals real time and dances stay on the music and in sync with other viewers.
  - The user sets the **Peak** (and Width, where the curve has one); the **Low** speed is derived, and shown read-only as "auto".
  - If the derived Low would drop below 0, the Peak is reduced and the UI says so.
  - With Stay on beat off, free ramps may drift ahead or behind; they are labelled "for shots".
- **Loop:** on (repeats every period) or off (plays once, then holds the final speed).
- **Phase source:** wall time, so a stall or a minimized window never drifts the beat.

## 4. What follows, and what never does
- **Follows:**
  - skeletal animation of the stepper's targets, including their worn animesh;
  - local movers' traversal, foot-locked;
  - optionally, their worn particles and texture animation.
- **Never follows:**
  - the world: objects, other avatars, sky, World Time Scale;
  - HUDs;
  - wall, network and UI time;
  - remote avatars' server positions;
  - the server: nothing is sent, so it stays viewer-local.

## 5. Interactions
| System | Rule |
|---|---|
| Temporal Capture / World Time Scale | Independent. If both are active they multiply (delta × per-avatar factor × global factor). |
| Freeze World | Untouched. |
| Stepper pose-stepping | Independent toggle; it composes with the ramp (§2.5). |
| Ghost Studio speed / self slow-mo | Composed through `ALAnimSpeed` layers; nothing overwrites anything. |
| Director takes (ACTION) | **Start on ACTION** (setting): the cycle restarts when a take starts, so the timing repeats per take. |
| One-shot animations | They finish early above 1× and late below 1× versus the server's start and stop. This is in the help text; looping animations are fine. |

## 6. Settings (new; viewer-local; owned by the Stepped page)
| Setting | Type | Default | Range |
|---|---|---|---|
| StepAnimRamp | Boolean | 0 | master on/off |
| StepAnimRampCurve | S32 | 3 | 0–12 (§3) |
| StepAnimRampPeak | F32 | 2.0 | 0–8 |
| StepAnimRampLow | F32 | 0.25 | 0–8 (derived and read-only while Stay on beat is on) |
| StepAnimRampWidth | F32 | 0.5 | 0.05–0.95 (pulse/square curves) |
| StepAnimRampBeatSync | Boolean | 1 | period from BPM (on) or Duration (off) |
| StepAnimRampBPM | F32 | 120 | 20–300 |
| StepAnimRampBeats | S32 | 1 | 1 / 2 / 4 / 8 beats per cycle |
| StepAnimRampDuration | F32 | 2.0 | 0.1–30 s |
| StepAnimRampPhase | F32 | 0 | 0–1 |
| StepAnimRampKeepAverage | Boolean | 1 | Stay on beat |
| StepAnimRampLoop | Boolean | 1 | |
| StepAnimRampOnAction | Boolean | 0 | restart the cycle when a take starts |
| StepAnimRampParticles | Boolean | 0 | ramp worn particles |
| StepAnimRampTextureAnim | Boolean | 0 | ramp worn texture animation |
| StepAnimRampKeyframes | LLSD | [] | ≤ 8 {u, s} (Custom curve) |

- Presets and scenes: the stepper's presets don't own the ramp settings unless a style clearly wants them.
- Scenes save the ramp settings with the stepper's scene data, and save per-actor Ramp Off overrides.

## 7. Director UI — Look › Stepped, new card **"Speed ramp"** (kit page; mockup first; labels must fit)
1. **Speed ramp** — master switch (StepAnimRamp). Help: "Speeds the stepper's actors up and down along a curve. Fluid; separate from stepping."
2. **Curve** — combo with the 13 curves, plus a small live curve preview.
3. **Peak** — 0–8× slider.
4. **Low** — 0–8× slider. Shows "auto" and is read-only while Stay on beat is on.
5. **Width** — shown for pulse, square and double pulse.
6. **Timing** — chips: Beats / Seconds.
7. **BPM** — number box only (20–300).
8. **Beats** — combo: 1 / 2 / 4 / 8.
9. **Duration** — seconds; shown when Seconds is chosen.
10. **Phase** — slider.
11. **Stay on beat** — switch. Help: "Balances slow and fast so the average is 1×; dances stay on the music."
12. **Loop** — switch.
13. **Start on ACTION** — switch.
14. **Also ramp** — two switches: Particles, Texture animation (worn by these actors).
15. **Live readout** — "Now 1.00× · avg 1.00×".

Per-actor override (the existing actor override area): **Ramp: Inherit / Off**.

Search keywords: speed ramp, time remap, slow motion, fast forward, beat, bpm, pulse, curve, ease, slow fast slow, heartbeat, stutter, music.

## 8. Edge cases
- **Stall** (wall frame > 0.25 s): the curve phase follows wall time. Non-looping shot curves clamp each step to 0.25 s, so one hitch can't skip the whole ramp.
- **An actor leaves scope, or the ramp is turned off:** `ALAnimSpeed` removes that actor's ramp layer, leaving the other layers intact. Its attachments' particle and texture deltas return to normal.
- **Teleport / region change / scene load:** the cycle continues, since it runs on wall time. Layers are re-applied as avatars and animesh rebuild.
- **Logout:** all ramp layers are cleared.

## 9. Performance
- Curve evaluation is O(1) per frame. Applying it is O(targets), reusing the stepper's cached target pass.
- No per-frame allocation. Zero cost when off.
- The particle and texture paths add one multiply per affected source or face, and only when their switch is on.

## 10. Tests
- **Unit:**
  - bounds and NaN guards for all 13 curves;
  - Stay on beat: the period mean equals 1 within 1e-4 across Peak × Width, with Low derived;
  - cumulative animation time after 100 cycles equals real time ±5 ms at 20 / 60 / 144 fps;
  - the `ALAnimSpeed` product and layer removal;
  - the Custom keyframe curve stays inside [0, 8].
- **Integration:**
  - a grep check: only `ALAnimSpeed` calls `setAnimTimeFactor`, and `LLPresentationTime` is still the only writer of `sGlobalTimeFactor`;
  - a ramped dancer's worn animesh matches the dancer;
  - a non-target avatar is unchanged;
  - a walking Ghost stays foot-locked;
  - with Particles and Texture animation on, only the targets' worn sources change speed;
  - self slow-mo and Ghost speed still work;
  - the value-wiring audit and the label-fit gate pass.

## 11. Open questions
1. Do you want a **per-actor speed offset** (e.g. one dancer at +10%), or just Inherit/Off per actor?
2. During a recorded take, should playback reproduce the ramp, or play at 1×?

## 12. Proof-review corrections to the Argon draft (kept for the record)
- Added the composer mechanism (`ALAnimSpeed`).
- Added the foot-lock gap for local movers.
- Freeze World is independent.
- No per-frame writes of persisted settings.
- Low is derived under the average rule.
- The stepper composes with the ramp instead of excluding it.
- Fixed the `sGlobalTimeFactor` citation.
- Added Start on ACTION.

v2 then removed World mode, tap tempo and audio sync, and added the curve set and the worn particle/texture options, per the user's decisions.
