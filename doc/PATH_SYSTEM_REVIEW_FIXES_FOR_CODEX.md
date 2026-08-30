# Path System Upgrade — Adversarial Review Findings & Fix Brief (for Codex)

**Reviewed:** the uncommitted Path system upgrade in `I:\alchemy-machinima` (working tree vs `6fbc0fc0efa`): collapsible editor, Diamond/Figure-8/Sine presets, Half-Arc/Sine endpoint fitters, per-path foot cadence, avatar/ghost path-guide occlusion, schema-3 persistence.
**Method:** 3 read-only passes (geometry math; state/persistence + render; UI). No build/run.
**Verdict:** **Solid — no P0/P1 correctness bugs.** The tilted half-arc fix is confirmed mathematically correct and robust (verified: `getEulerAngles` exactly inverts the `Rx·Ry·Rz` convention `planeRotation` applies; `shortestArc` maps the canonical arc onto the endpoints; flat/tilted/vertical/gimbal-lock all close to ~1e-4). Persistence round-trips with no dropped field across save/load/copy/undo/redo/follower. The render change is single-draw, depth-correct, and correctly gated.

Items below are fix-next; none blocks the final link. Each has a file:line and a concrete change. **Two items are design decisions flagged for the user — do not implement those without a decision.**

---

## A. Fix directly

### A1 (P1, UI) — Suspend banner overlaps the accordion header
- **Where:** `indra/newview/skins/default/xui/en/panel_path_editor.xml` — `suspend_banner` at `top="18" height="48"` (~:253-261) → y[18,66]; `accordion` at `top="38"` (~:55). Overlap band y[38,66]. `refreshSuspendBanner()` (`indra/newview/alpanelpatheditor.cpp:670-726`) only toggles banner/hint visibility, never the accordion.
- **Symptom:** whenever a walk is suspended (e.g. actor TP'd away), the banner covers the top ~28 px of the "Nodes & Playback" tab header and its collapse arrow. Always reproducible.
- **Fix (recommended, dynamic):** in `refreshSuspendBanner()`, when the banner is shown, push the accordion down by the banner's occupied height and shrink its height by the same amount (`mAccordion->setRect(...)` / `reshape`); restore to `top=38`/full height when hidden. This keeps a gap-free layout in the common (not-suspended) case.
- **Fix (alternative, static/simpler):** move the accordion `top` to clear the banner (e.g. 38→68) and reduce its `height` by 30 in the XML. Simpler but leaves a ~30 px gap when the banner is hidden.

### A2 (P2, geometry) — Sine endpoint fitter misses the endpoints for short/vertical chords
- **Where:** `indra/newview/llactormover.cpp:1996` — `mPrimitiveRadiusX = llmax(0.5, horizontal * 0.5);`
- **Symptom:** endpoints are hit only when `2·radiusX == horizontal chord`, i.e. only when `horizontal ≥ 1 m`. The 0.5 floor breaks this: chord `(0,0,0)→(0.4,0,5)` misses by 0.3 m each end; a purely vertical chord misses by 0.5 m in an arbitrary (world-X) direction. `syncPrimitiveNodes` (`:1080`) then bakes the wrong positions into `mNodes.front()/back()`. (The half-arc fitter is immune — it uses the full 3-D `length` + `shortestArc`.)
- **Fix:** drop the floor for the endpoint half-span: `mPrimitiveRadiusX = horizontal * 0.5;`. Guard degeneracy the way the half-arc fitter does (it rejects `length < 0.02` at `:1964`): if the horizontal span is below a small epsilon (a sine needs a horizontal baseline — a near-vertical chord has none), reject the fit with the same user-facing warning path the half-arc uses, rather than silently flooring. Keep the amplitude default (`radiusY = horizontal*0.25`) as-is.
- **Add a test** (see C1) — the current tests never exercise the sine fitter, which is why this went undetected.

---

## B. Design decisions — DO NOT implement without a user answer

### B1 (P2, gait) — Live cadence edit pops the foot phase; the doc's "no step-discontinuity" claim is FALSE
- **Where:** sampling `odometer/nominal` at `indra/newview/llactormover.cpp:334` / `:854-856`; `mv.mNominal = llclamp(path.mCadence, 0.5f, 12.f)` re-read every frame at `:4787`; `mPhaseOrigin` set once when `<0` at `:842-846`, never re-based. Cadence slider is live (not `!walking`-gated): `onPathCadenceCommit` (`indra/newview/alpanelpatheditor.cpp:1540-1546`), enabled `have_actor` only (`:442`) — unlike the geometry sliders which carry `&& !walking`.
- **Symptom:** changing cadence N1→N2 at odometer D jumps the foot sample by `D·(1/N2 − 1/N1)` mod the walk cycle (≈ half a stride at D≈50 m, N 3→4) — a visible foot pop. Followers pop the same way off the leader's cadence (`:5127`). The `:949` comment ("cadence never step-discontinuities") is about per-node *speed* continuity, not live cadence-field edits, and does not cover this.
- **Decision needed (pick one):**
  - **(A) Preserve phase continuity live:** on a cadence change, re-base `mPhaseOrigin` so the current foot phase is held and only the rate changes. Cost: breaks the deterministic odometer→phase model that sync-to-take scrubbing and follower lockstep rely on (`:4868-4874`, `:5165-5167`) — would need the re-base scoped to live edits only, not scrub.
  - **(B) Gate the slider `!walking`** like the geometry sliders (`alpanelpatheditor.cpp:442`), so cadence can't change mid-walk. Simplest; preserves determinism; loses live tuning. Also correct the doc.
  - **(C) Accept the pop; correct the doc's claim** to "cadence changes re-phase the gait."
- Recommend **(B)** unless live cadence tuning during a take is a required workflow, in which case **(A)**.

### B2 (design, render) — Edit-tool path nodes are now occluded behind terrain while marking
- **Where:** the edit overlay now draws with depth test on (`renderActorPathOverlay`, depth `LLGLDepthTest(true,false,GL_LEQUAL)` at `indra/newview/llactormover.cpp:13105`), composited after deferred lighting (`indra/newview/llviewerdisplay.cpp:1135-1140`).
- **Symptom:** nodes/ribbon occluded by hills/props are now invisible while you author — the old overlay was deliberately always-on-top "so you always see what you're marking." (Picking is a separate raycast, so a hidden node may still be clickable but not visible.)
- **Decision needed:** occlusion is correct for the *filmed/playback* guide but arguably wrong for the *editing* view. Option: keep depth-tested occlusion for the played guide, but render the **edit-mode nodes always-on-top** (depth test off) when the path editor is open / in marking mode. If wanted, split the two draws by depth state on an "is-editing" flag. Confirm before implementing — it's a UX preference.

---

## C. Minor / hardening (P3 — safe to batch)

- **C1 — Test the production fitter + the untested shapes.** `indra/newview/tests/alpathgeometry_test.cpp:225-248` re-implements the half-arc fitter inline instead of calling `LLActorMover::fitPathPrimitiveToEndpoints` (`llactormover.cpp:1940`). Change the regression test to invoke the production fitter, and add: a **sine-fitter** endpoint test (incl. a horizontal span <1 m and a near-vertical chord — would catch A2), and Diamond/Figure-8 **reverse** + distance-parameterized-travel tests (currently only positions/seam are smoke-tested).
- **C2 — `finite_f32` treats a missing key as 0.0.** `llactormover.cpp:1607-1611` uses `value.asReal()`, which returns 0.0 for an absent LLSD key (finite), so the fallback fires only on NaN/inf. A schema-3 file missing `cadence` loads 0.0→clamped 0.5 (not 3.0); missing `primitive_radius_x`→0.01. Harmless for viewer-written files; affects hand-edited/truncated scenes. Fix: gate on `data.has(key)` and use the intended default when absent.
- **C3 — `reversePath` drops Roll/Pitch.** `llactormover.cpp:3207-3211` copies back Center/Start/Sweep/Yaw/Rise but not Roll/Pitch, relying on the latent identity that `reverse` for SINE is exactly `yaw += 180` with roll/pitch preserved (`alpathgeometry.h:188-189`). Correct today, but silently breaks if `half_turn` (`alpathgeometry.h:184`) ever becomes a non-Z axis. Fix: also copy back `mPrimitiveRollDeg`/`mPrimitivePitchDeg` from the reversed primitive, or add an assert + comment documenting the invariant.
- **C4 — Director Path tab over-widened.** `indra/newview/skins/default/xui/en/floater_director.xml:546` embeds `panel_path_editor` at `width="700"` vs its native `width="500"` (`panel_path_editor.xml:10`) → ~200 px dead strip on the right of the left-anchored button/spinner rows (the Actor Mover host embeds it at the exact 500 and looks right). Fix: either add `follows="...right"` stretch to the button rows / two-column grids, or host it at 500 left-aligned in the Director tab.
- **C5 — Harden the render block's matrix state.** The guide draw (`llviewerdisplay.cpp:~1130-1144`) relies on the ambient `gGL` world projection/modelview left by the prior world pass rather than loading it explicitly; likely fine (in-world testing will confirm the guides land on the path), but an explicit world proj/modelview load in the block would harden it against future insertions between lighting and this draw. Also `gUIProgram` is left bound after both fns (`:12714`, `:13106`) — conventional, harmless.
- **C6 — Cosmetic label.** "Plane tilt" (`panel_path_editor.xml:231`) vs the variable `mPrimitivePitch`/`primitive_pitch_deg` — synonym; optionally align the wording.

---

## D. Render tradeoffs to accept or note (consequences of the occlusion feature, not bugs)

- **Underwater guides vanish** behind the water surface: water writes depth with `GL_ALWAYS`+write-on (`lldrawpoolwater.cpp:116`) before the guide draw, so segments below a water plane fail `GL_LEQUAL` even though the actor stays visible through the water. Special-case only if it bites.
- **Per-actor colors desaturate under tonemapping:** guides now composite into the HDR scene target pre-`renderFinalize`, so they pass through SSR/bloom/DoF/tonemap. The saturated per-actor hues (`llactormover.cpp:13120-13125`) flatten under Khronos-Neutral/ACES (weakening the color-coding that distinguishes overlapping paths); bright lines can bloom; DoF may smear a crisp line using the background depth. Inherent to the design; accept or boost guide saturation to compensate.

---

## E. Verification after fixes

1. Close the running viewer (PID 15980) and complete the Release link (blocked only by the exe lock).
2. Build `INTEGRATION_TEST_alpathgeometry` and run — all existing 10 tests **plus** the new C1 tests must pass.
3. In-world QA per `doc/PATH_SYSTEM_UPGRADE_AND_CREATIVE_ROADMAP.md` "Runtime QA checklist", with emphasis on: half-arc/sine fit at same vs different heights and <1 m horizontal spans (A2); cadence change mid-walk (B1 — confirm the chosen behavior); a path behind a real avatar / deferred ghost / studio ghost (occlusion ordering); and editing nodes behind terrain (B2).
