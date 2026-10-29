# Cinematic Lighting 2.0 — Consolidated Adversarial Review Findings

Date: 2026-08-19. Four parallel Opus reviewers audited Phases 1–4 against the actual code in
`I:\alchemy-machinima` (branch feature/cine-light-rig, uncommitted), per
`CINEMATIC_LIGHTING_2_0_ADVERSARIAL_REVIEW_HANDOFF.md`.

## Overall verdict

**Unusually clean. No P1 blockers anywhere.** The C++ compiles and links (verified: the full viewer relinked
at 19:46 with all of this in-tree, after fixing 2 pre-existing cue-floater compile errors). GLSL shows no
static driver-rejection defects. The only issues are **one P2 correctness bug** (cue stack duplicate-timestamp)
and **one P2 visual** (shader penumbra banding); everything else is P3 polish or documentation.

Outstanding (not defects, but not yet done): runtime GPU shader compile on real hardware, the 49 authored
unit tests (never executed), and in-viewer visual calibration.

---

## P2 — fix before ship

### P2-1 — Cue stack: duplicate-timestamp "previous cue" tie-break is inconsistent
`indra/newview/alcinelightrigmodel.cpp:1270-1281` (`evaluateTimecodeCueList`). Category: correctness/determinism.
The `current` cue resolves duplicate absolute times to the LATER row, but the `previous`-state search uses
strictly-greater only, so with 2+ cues sharing an earlier `mAtSec` it picks the EARLIEST row → the fade into
the current cue starts from the wrong full state, plus a visible pop at the timestamp boundary.
Repro: `A{at=10,EV=0}`, `B{at=10,EV=6}`, `C{at=20,EV=10,fade=4}`; at now=22 (C weight 0.5) previous resolves to
A (EV=0) not B (EV=6) → EV 5 instead of 8, and a B→A pop at t=20. test<47> misses it (its dup rows sit at the
current time, not the previous group).
Fix: mirror the tie-break — `if (i != current && at < current.at && (at > previous_at || (at == previous_at && i > previous)))`.

### P2-2 — Shader: penumbra-width banding from quantized blocker-gap
`app_settings/shaders/class1/deferred/shadowUtil.glsl:133-140,238`. Category: GPU-visual correctness.
The blocker-gap estimate takes only 5 discrete values → 5 discrete kernel radii with big back-loaded jumps
(0.156→0.5→1.0), so adjacent receiver texels straddling a threshold snap between radii → visible penumbra-width
contour bands on slanted/curved receivers. (Compiles fine; purely visual.)
Fix: accumulate a fractional count of passed depth tests and `smoothstep`/lerp the gap between thresholds
instead of hard-assigning bands.

---

## P3 — polish / document

Shader (Phase 4):
- 4-diagonal blocker search at one radius misses thin/axis-aligned blockers; `max()` over directions
  over-softens one-sided shadow edges. `shadowUtil.glsl:128-142`. Fix: add ±axis taps (8-tap) and/or average.
- Fixed normalized-depth thresholds (0.02/0.08/0.20) are frustum-scale-dependent → same geometry softens
  differently per projector. Fix: scale thresholds by projector depth range, or document as calibrated.
- Slope shimmer on steep receivers (fixed bias false-positive occluder → kernel widening, not darkening).
- Rotating fan UV rotation about (0.5,0.5) pulls corners outside [0,1] → streak (REPEAT) or smear (CLAMP);
  verify each fan PNG has dead margin so blades never reach corners. `deferredUtil.glsl:279`.
- Hour-wrap: `fmod(presentation_time,3600)` × 0.65 rad/s isn't a 2π multiple → fan jumps once/3600s
  (pre-existing, shared by all animated gobos).
- Perf: soft path adds 4–16 blocker samples on top of 12/Vogel, ×≤4 projectors (~33–50% more shadow samples);
  bounded/opt-in, zero cost when disabled. When `soft_shadow_enable!=0` but effective softness==0 the 16-sample
  search still runs (pre-existing soft-path behavior).

Cue stack (Phase 3):
- Overlapping timecode fades don't compound: a cue firing before the prior fade finishes snaps from mid-fade
  to prior-complete, then fades → a pop. `alcinelightrigmodel.cpp:1255-1287`. Document or guard in UI.
- Auto-follow timing uses the cue's AUTHORED delay/fade even after SNAP GOTO (zeroed) or BACK (borrowed prior
  timing) → follow fires at a time not matching the transition shown. `alcinelightrig.cpp:3426-3443`. Minor.
- Space-to-GO may bubble from a focused spinner/combo that declines Space (only mListName/mLabel are guarded).
  `alfloatercinelightcues.cpp:382-391`. Verify with in-viewer keyboard test.

Cookies/UI (Phase 2):
- `neon_sign()` uses `ImageFont.truetype("DejaVuSans-Bold.ttf")` with a default-bitmap fallback →
  `gobo_neon_sign_*` changes shape/scale across machines on re-generation. `scripts/generate_cine_gobos.py:214`.
  Fix: bundle the .ttf or draw the glyphs as vector strokes.
- Generator script is UNTRACKED and at repo-root `./scripts/` (not `indra/newview/scripts/`) → a commit could
  omit the sole authoring source for the 69 assets; `main()` also depends on the 7 legacy sharp PNGs existing.
  Fix: track the script, document the dependency.
- Softness LABEL recomputes from global `CineLightRigRadius`, but the projected variant uses per-light
  `mShadowSoftness[i]` → label can name a different bucket than what's shown. `alpanelcinelightrig.cpp:678`.
  Cosmetic.
- `rigGoboTexture` one-shot `attempted[index][bucket]` guard: a transient texture-fetch failure is never
  retried and silently falls back to the (possibly blank) custom cookie. `alcinelightrig.cpp:335-354`. Minor.
- Observation: gobo is selectable twice (four per-role dropdowns + the role-scoped thumbnail picker), both
  editing the same 4 settings. Intentional but potentially confusing.

Fixture core (Phase 1):
- P3/doc only: the design doc's worked penumbra value for the Practical (0.05m@2m → "0.5") is an arithmetic
  slip; the formula the code implements yields ≈1.0. Code is correct; fix the doc if desired.

---

## Verified correct (attacks that held up)

- **Zero-break legacy:** `mFixtureMode=false` is byte-identical to pre-2.0 (old profile/temp/gel/falloff/fov/
  bounce lines moved unchanged into the else-branch; old GELS table + masterTempGain untouched; softness only
  overrides when fixtureMode). Old blobs/scenes lacking new fields load with safe defaults; all persistence
  mirrors (blob LLSD, settings store, manager ParamBlob, sceneData) are complete and symmetric.
- **Fixture math:** Kelvin/gel/mired additive stacking + green-normalized Planckian (3200K+FullCTB≈5698K),
  exposure-invariant on CCT change, no master double-application, no div-by-zero (clamps). Penumbra formula
  exact + monotonic; secondary couplings bounded (FOV clamp 3.0 verified safe vs pipeline consumption).
  Transitions interpolate Kelvin in mired space, scrub-safe (pure fn of presentation_time).
- **Cookies:** 69 PNGs + textures.xml thumbnails DO ship in an installed build (`viewer_manifest.py:179`
  packages `cine_gobos/*.png`; variants load by path). Gobo table integrity, variant thresholds (2.5/5.5),
  custom-cookie (index 0) preservation, per-role writes, non-selectable category rows — all correct.
- **Shader:** no GLSL driver-rejection construct (all mirror already-shipping code in the same file); correct
  sampler2DShadow comparison usage; PCSS math directionally correct (contact→sharp, separation→wider,
  monotonic) and an improvement over the old formula; gating/zero-break fallback to classic 5-tap correct.
- **Rotating fan:** presentation-time deterministic (0.65 rad/s), rotates about center, clears on gobo-change
  and emitter-destroy, keyed by unique UUID → no leak to recycled projectors.
- **Cue stack:** transport (GO/GOTO/BACK/RELEASE during fades capture live state; BACK@1 clamps; SNAP GOTO);
  NaN/timing safety; follow catch-up bounded (guard ≤ size, zero-duration chains terminate); layer order
  cue→FX→flicker→emitters; FX phase restart at delayed epoch; midpoint discrete switch at >0.5 consistent;
  serialization rejects malformed/oversized/wrong-version cue lists WHOLE (no partial state); persistence
  restores mid-fade/BACK/GOTO/RELEASE without snapping; multi-instance + timecode purity + presentation-time
  clock all correct.

## Test-run results (added after review — the 49 tests were finally compiled + run)

The suite did NOT compile as delivered: `tests/alcinelightrigmodel_test.cpp:3349` (test<49>) called an
undefined `makePatternBlob()`; fixed to the existing `distinctiveBlob()` helper. After that, **45/49 pass, 4
fail** — all pre-existing in the Lighting 2.0 code (NOT caused by the two P2 fixes above):

- **test<45>** "cue fade profiles honor delay/presentation time" — `ease()` (alcinelightrigmodel.cpp:954)
  implements a CUBIC ease-in-out (`4t³` → 0.0625 at t=0.25), but the test AND the transition blend
  (`:383`, Hermite smoothstep) expect `3t²−2t³` → 0.15625. Curve inconsistency: either `ease()` should be
  smoothstep (matching :383) or the test/expectation updated. Author's design call.
- **test<2>** "endpoint is exact target", **test<10>** "default scale preserves the pre-change golden",
  **test<13>** "zero master temp is a bitwise no-op" — exact/bitwise float comparisons. Either ULP brittleness
  (like the earlier tests 8/10 that were given tolerances) or a real zero-break perturbation from the fixture
  work. test<13> in particular is a zero-break assertion (master temp 0 must be a bitwise no-op) and should be
  investigated to confirm it's brittleness, not a regression.

These 4 are the parallel author's Lighting 2.0 logic/tests — not fixed here (they need a design call on the
ease curve and a real-vs-brittle determination on the exact-compare trio).

## Still outstanding (not defects)
1. Runtime GPU shader compile (NVIDIA/AMD/Intel) + visual calibration of the PCSS constants.
2. Resolve the 4 failing unit tests above (ease-curve consistency + the exact-compare trio).
3. In-viewer visual pass on cookies, cue transitions, and contact-hardening.
