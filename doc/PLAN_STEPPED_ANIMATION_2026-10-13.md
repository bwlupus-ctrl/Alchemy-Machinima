# Plan of attack: Stepped Animation page ("on twos / threes"), rev 2, 10/14/2026

## Goal
A full Director v2 page, designed from the ground up as a kit page, where character motion updates on a stepped clock (e.g. 12 fps on twos, 8 fps on threes) and holds in between, while the camera and world stay smooth. This is the drawn look of KPop Demon Hunters and Spider-Verse, plus the other stepped styles in the presets below. It's viewer-local only (other residents see normal motion).

"Done" = the page matches its mockup, every control is runtime-proven to change its value, and it looks right in your hands-on test.

## USER DECISIONS 10/14 ~5:45 AM (override anything below)
"Build it to completion: all scopes, minus motion blur."
- **Phase 2 is in this build:** smears and ease spacing ship now.
- **All scopes ship:** Selected / Cast / Everyone / Mixed.
- **No motion-blur work:** Card 6 is dropped, along with the step-frame blur modes. Correctness only: held poses must not ghost under TAA.
- **Defaults:** Look › Stepped, pose + position, whole cast.
- **Testing:** build it; no in-world test (monitor off); run the verifications: unit tests, gates, Opus review, login-screen runtime test.

## Decisions for you (step 0, now taken; see above)
1. **Page location.** Recommended: **Look › Stepped** (named "Motion Style" or "Stepped"), next to Roto Ink and Tron, since they combine. Alternative: Performance, next to Animate and Polish.
2. **Default "what steps":** pose + position (authentic) or pose only. Both stay selectable on the page.
3. **Default scope:** selected actor, or the whole cast. Both stay selectable on the page.
4. **Ship phase 2 effects (smears, impact holds) in the first build, or later?**

## The page (kit layout, top to bottom)
The preset strip runs across the top: a kit preset provider with built-ins plus user Save/Delete; "modified" shows when anything differs.

**Card 1: Stepped animation** (master switch in the card header)
- Applies to: Selected actor / Whole cast / Everyone in view (combo).
- Actor: the target selector, the same component as Actor FX, shown when "Selected actor".
- Per-actor override list: shows which cast members have their own setting (click to edit, × to clear).

**Card 2: Rate**
- Mode: Fixed / Adaptive / Mixed per actor (segmented).
- Rate: quick chips **1s · 2s · 3s · 4s**, plus an fps slider from 2 to 30 (shown as "12 fps = on twos at 24").
- Adaptive (shown when Adaptive): Fast-motion rate (default on ones), Threshold (limb speed), Hysteresis. Fast action goes smooth and slow holds go stepped, the KPDH/Spider-Verse technique.
- Background rate (Mixed): the rate for non-target actors.

**Card 3: What steps**
- Pose / Pose + position (segmented).
- Face & lip-sync stepped (toggle).
- Gaze & look-at stepped (toggle).
- Hair, cloth & flexi with avatar (toggle, default on).
- Attachments & particles stepped (toggle).

**Card 4: Timing**
- Clock: Capture time (recommended, exact twos in a 24 fps render) / Real time.
- Actor offset: Random / Locked together / Manual phase (slider, 0 to 1 tick).
- Base fps reference (24 / 25 / 30), which the chips step against.

**Card 5: Character** (advanced; phase 2 rows are marked)
- Pose boil: tiny random jitter on held poses (stop-motion / claymation), with amount and change rate.
- Hold on impact: a hit-stop, holding the pose for N extra ticks when a limb's velocity changes sharply. Frames and Threshold.
- *(phase 2)* Smear frames: on fast step frames, stretch fast limbs along their motion (anime smear). Strength.
- *(phase 2)* Ease spacing: bias held poses toward the extremes ("snappy" key poses), with an amount.

**Card 6: Motion blur & TAA on steps**
- Step frames: Hard cut (no blur, crisp) / Real motion (blur the snap) / Partial (slider).
- Held frames: always zero motion (info row).

**Card 7: Pairs well with**
- Match Roto Ink redraw to this rate (toggle; drives CineOutlineSketchFPS and the Roto Ink motion step).
- Link rows: Roto Ink › open, Tron › open, Actor FX › open.

## Presets
| Preset | Rate | Steps | Extras |
|---|---|---|---|
| Anime on twos | 12 (2s) | pose + position | random offset |
| Anime on threes (limited) | 8 (3s) | pose + position | random offset |
| KPop Demon Hunters | Adaptive: twos at rest, ones on fast action | pose + position | hold on impact on, Roto Ink matched |
| Spider-Verse mixed | Mixed: targets on twos, background on threes | pose + position | hard-cut blur, offsets random |
| Stop-motion (Laika/Aardman) | 12 | pose + position | pose boil medium, no motion blur |
| Claymation boil | 12 | pose + position | pose boil high, fast boil rate |
| Harryhausen creature | selected actor on 12; others smooth | pose + position | pose boil low |
| Rotoscope (A Scanner Darkly style) | 12 | pose + position | Roto Ink matched, face stepped |
| Silent film undercrank | 16 | pose + position | hard cut |
| Puppet / marionette | 10 | pose only | gaze smooth, slight boil |
| Retro sprite | 6 | pose + position | hard cut, no boil |
| Choppy dream / strobe | 4 | pose + position | hard cut |
| Hit-stop action | Fixed ones | — | hold on impact strong (fight scenes) |
| Custom | — | — | user's own |
User presets: Save as / Delete, as on other kit pages.

## Where it goes in the code (verified anchors)
- `LLCharacter::updateMotions` calls `LLMotionController::updateMotions` (llcharacter.cpp:186/197; llmotioncontroller.cpp:865). Animation time keeps advancing every frame; only the pose APPLIED to joints is held.
- `LLVOAvatar::updateCharacter` (llvoavatar.cpp:5670): the per-avatar decision to tick or hold.
- Velocity: `BDMergeVelocityBuffer` / `BDMergeVelocityRiggedBias` / `BDMergeVelocityAlpha` (settings.xml ~9851).
- Flexi: `LLVolumeImplFlexible::doFlexibleUpdate` (llflexibleobject.cpp:429).
- Per-actor storage: `LLDirectorCast::ActorStyle` (lldirectorcast.h:169), the same home as the Actor FX per-actor settings, so scenes save it.
- Kit page pipeline: director_mockup_data.json, then kit_pages.json, then director_kit_gen.py, then panel_dpage_*.xml. Presets via registerLookPresetProviders (as for Roto Ink and Tron).

## Steps
### 1. Mockup first (the mockup wins)
- Add the page to the Director mockup (artifact 23378a00 and doc/director_kit/mockup/Director.dc.html) using the kit look: the cards above, the preset strip and the chips.
- Add the page's cards and rows to `director_mockup_data.json`.
- **Your OK on the mockup before any C++.**

### 2. Engine design (Opus)
`doc/STEPPED_ANIMATION_DESIGN.md`:
- the stepped clock driven by capture/simulation time, with per-actor phase;
- the joint-transform cache and re-apply after the motion controller;
- adaptive-rate detection from limb velocity;
- pose boil (deterministic, seeded per actor and tick);
- hold-on-impact detection;
- velocity/TAA rules for held vs step frames;
- flexi stepping;
- exclusions: camera, flycam, mouselook self, attached cameras;
- interaction with Ghosts, AutoAnimate, Dance Ball, poser, Actor Mover and takes (takes store smooth data; stepping is playback-only);
- cost.

Codex reviews the design until there are no major issues. Smears and ease spacing are designed as phase 2.

### 3. Implementation (Codex), one round
- Pure core `alsteppedanim.*` (clock, adaptive, boil, impact hold) with unit tests.
- Hook in `LLVOAvatar::updateCharacter`. Option off = byte-identical.
- Velocity/TAA handling, flexi stepping.
- The kit page generated from the sources, with its bind handlers and the preset provider.
- **Value-wiring audit** `doc/director_kit/WIRING_AUDIT_stepped.md`: every row checked for setting, type, range (from the consumer clamp), values and read-back.

### 4. Review
- Opus adversarial review: render path, skinning, timing.
- Opus script check of every page row against its setting.
- Compile gate plus the 9 prebuild gates.

### 5. Build and check
- One fresh build, run only when your viewer is closed.
- A login-screen runtime test of the new page: every control changes its value.
- **Your eye test:** each preset; an orbiting camera while the characters step; motion blur on and off; Roto Ink matched; a 24 fps recording.

### 6. Tune, then phase 2
- Tune the presets and adaptive thresholds from your feedback.
- Then smears and ease spacing (Card 5 phase 2 rows) as their own small round.

## Pass criteria
- **Option off:** byte-identical pose, velocity and skinning to the current build.
- **Exact rates:** twos at 24 fps capture = the pose changes exactly every 2nd frame; at 60/144 fps render it's still 12 changes per second.
- **No drift:** the stepped phase matches the smooth phase at tick frames after N seconds.
- **Adaptive:** switches to ones above the threshold and back with hysteresis (unit test, no flicker).
- **Pose boil:** deterministic per seed, with an amplitude bounded by the setting.
- **Motion vectors:** held frames give zero rigged-mesh motion vectors; no TAA ghost trail on held poses (visual check).
- **Page:** matches the mockup (screenshot side by side); every control runtime-proven; presets apply exactly their listed values and show "modified" correctly.

## Risks
- **TAA/velocity** is the main risk: render code, so it gets the Opus review.
- **Gaze, IK and lip-sync:** they may look odd stepped or smooth, which is why each is a toggle.
- **Hit-stop and adaptive detection** need tuning on real animations.
- **Mouselook self and attached cameras** are excluded by default to avoid judder.

## Order
This comes after the trigger-momentum build. Then step 1 (the mockup) for your OK. Does it go before or after the next Director conversion batch? Your call.
