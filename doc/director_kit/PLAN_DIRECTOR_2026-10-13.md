# Director v2 plan (written 10/13/2026, 5:45 PM)

## Goal
Every Director v2 page matches the mockup and every control is **proven at runtime** to change the value its label says. "Done" for a page = converted + audited + runtime PASS with every not-run control listed with a reason.

## Where things stand (facts)
- Build ready: **5:08 PM, a7953fd46a2**. Login-screen runtime test on it: **1150 PASS**; 0 wrong-value failures on every page converted today.
- **Converted to mockup kit pages (14):** VCam, Light Rig, Rim, Bloom & Glow, Roto Ink, Tron, Image, Actor FX, Focus, Filters, Diopter, Weather, Atmosphere, Shadows.
- **Still legacy (21):** listed under the batches in step 5.
- **Not runtime-proven yet:**
  - VCam (168 controls): the test can't follow its name-routed handler.
  - Image's 7 three-number rows: the test doesn't handle multi-box rows.
  - Gaze, Props, Move and Animate: these need the world.
  - The non-page features: the bottom bar, the … and right-click menus, the search popup, adding you to the cast at login, and scene Rename/Save.
- **Rain at altitude (user, 10/13 ~8:30 PM):** the rain shader works, but rain is not visible at high altitude. Noted only: **don't fix until the user asks.** Suspects: rain spawn or volume tied to a ground/water height band, an altitude fade, or height-based gating. Start points: `codex_rain_diag*.txt` and the manifest's `rain_gate` expectation.
- **Open product items:**
  - Tab between kit controls: unproven. Codex says the failure is a test artefact.
  - Operator page: FlycamOperatorModeBlendTime and FlycamOperatorSimulationHz have no control on the v2 page.
  - The 5 Image→Exposure shortcuts wait for the Exposure page conversion.

## Steps, in order

### 1. Finish the test tool, then stop touching it (in progress)
- Round 8 (running) is the final policy:
  - **Login screen:** run everything except file dialogs, quit, external programs and the clipboard. Before every batch, prove there is no server connection. Every user data folder is backed up and restored byte for byte.
  - **In-world:** round 7's strict blocking of every server call, unchanged.
- **Pass criteria:**
  - Login screen: ≥ 1100 controls run.
  - In-world: the self-test fixtures prove that every server-call form is blocked.
  - Your data folders come back byte-identical, with the diff shown.
- **Then:** one Opus safety check, then commit.
- **After this:** no more test-tool rounds unless a feature answer is blocked. Any change after that starts from a written spec.

### 2. Cover the gaps in "does it work" (one spec, one round)
- **VCam:** the test follows the name-routed handler branch for each control.
- **Image three-number rows:** test each box on its own. Change one component, then check that only that component of the setting changed. This is the regression test for the "R written as 0" bug.
- **Pass criteria:** VCam at least 120 controls run, Image 64, with the leftover controls listed by name and reason.

### 3. Your hands-on checklist (the test can't cover these)
- **Bottom bar:**
  - ACTION/CUT;
  - the arm ▲ button;
  - take play/stop;
  - the scene "⋯" menu (Save as, Rename, Delete);
  - REC;
  - the cast chip.
- **Card menus:**
  - the card "…" button opens a menu;
  - right-clicking a slider, spinner or dropdown opens the row menu;
  - adding a control to Favorites from that menu works.
- **Search:**
  - results show as a list;
  - the popup closes when you click away or press Esc, and stays closed after a page change.
- **Login:** you're in the cast list, and the scene isn't marked unsaved.
- **Scenes:** Rename only renames, and saving over a different scene asks first.
- **Light Rig:** the aim ◀▶▲▼ buttons turn the selected light only.

### 4. Fix what steps 2 and 3 find
- Each bug goes through: diagnose with file:line → fix → compile → Opus review only if it touches render or safety.

### 5. Remaining conversions, one batch at a time
Each batch follows the same process (step 6). Order:

| Batch | Pages | Mockup rows |
|---|---|---:|
| Light | Exposure, Practicals, Actors, Light Cues (plus the 5 Image→Exposure shortcuts) | 74 |
| Camera + Capture | Shot, Moves, Operator (plus the 2 missing Flycam controls), Frame/Orbit, Takes, Temporal, Scene Options | 231 |
| Performance | Animate, Switchboard, Gaze, Polish, Gaze Cues | 199 |
| Cast A | Move, Path, Props | 114 |
| Cast B | Ghosts, Dance | 207 |

### 6. Process for each conversion batch (no deviations)
1. **Spec:** a brief listing the pages, the known traps, and the value-wiring audit rule. Ranges come from the code's clamp, not the mockup.
2. **Write:** Haiku writes the sources and the audit doc (`WIRING_AUDIT_<batch>.md`).
3. **Review:** Opus checks every row by script and traces ~40 rows by hand into the code. Fixes go to Codex, then Opus re-reviews once.
4. **Pin:** lower the page-count pin by the number of pages converted (you approved: lower only).
5. **Checks:** regenerate and pass all 9 gates.
6. **Build:** commit, then one build, run only when your viewer is closed.
7. **Runtime test:** the login-screen run, plus an in-world run as 5kvideoeye, only after the step 1 safety check passes.
8. **Report per page:** PASS / FAIL / not run, each with a reason. A batch is done when every page shows 0 wrong-value failures.

## Things I won't do
- Start implementing without a written spec and pass criteria.
- Report "works" from code review alone. Code-checked and runtime-proven are reported separately.
- Loosen any check to get a pass. Any change to a check needs your OK.
- Build while your viewer is open, push, or pull anything from upstream.
