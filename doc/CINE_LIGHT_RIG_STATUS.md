# Cinematic Light Rig — status / resume point

**Updated:** 2026-08-16.
**BUILT (compiles + links clean). NOT YET TESTED IN-WORLD.**

Latest milestones (2026-08-16):
- Base rig + scale + mirror + enhancements + track-mode + master presets + gobos +
  lens-gaze + **multi-anchor (group-as-unit)** all implemented and Opus-adversarial-reviewed
  (multi-anchor: 0 must-fix; fix round F1/F2/F3 + N3/N4 applied and verified).
- **Git baseline** committed: branch `feature/cine-light-rig`, commit `d12fa7a91e` —
  full working-tree snapshot (111 files; excludes `.tmp.driveupload/` + build logs).
- **Release build** `cmake --build build-Windows-vs2026-os --config Release` → exit 0,
  no `LNK1104`/errors; produces valid `AlchemyTest.exe` (Alchemy Viewer 26.2.0.63033).
- REMAINING: in-world smoke test; ReShade volumetric-fog decision (awaiting user).

---

## Where this stands

Feature: the in-world LSL "Cinematic Studio" light rig re-implemented as a client-side,
prim-free viewer feature. 4 logical lights (KEY/FILL/RIM/BG) as local-only `LLVOVolume`
emitters orbiting an anchor (own avatar or any Director cast member), 24 colour profiles,
3 beam types, mirror/orbit/pitch transforms over a pristine base, 0.9 s eased transitions,
33 deterministic FX, shared UI panel in both the Director Console "Lights" tab and a
standalone floater.

Process followed (per CLAUDE.md): design pass → Codex implements → adversarial review →
batch fixes → re-review → **build once** → user tests in-world.

| Stage | State |
| --- | --- |
| Deep design | DONE — `doc/CINEMATIC_LIGHT_RIG_DEEP_DESIGN.md` |
| Codex implementation | DONE |
| Gizmo completion | DONE |
| Review round 1 (4 slices) | DONE — 4 must-fix, 8 should-fix, 12 nits |
| Fix round 1 | DONE — all 24 addressed, 2 reasoned push-backs |
| Review round 2 | DONE — all 4 slices, 0 must-fix. |
| Fix round 2 | DONE — all 8 should-fix + 8 nits applied, 0 rejected. |
| Review round 3 | DONE — both slices, 0 must-fix. 3 should-fix + 2 nits (2 were U1 regressions). |
| Fix round 3 | DONE — all 7 items. Seed cast, full 24+33 goldens, T3 cycle-cross verified. |
| Review round 3-fix | DONE — refresh path, 0 must-fix. 2 cosmetic should-fix carried (below). |
| BUILD attempt 1 | FAILED — 2 errors, both `setText` bare-literal→LLStringExplicit in alpanelcinelightrig.cpp (589, 596). Model + controller compiled clean. |
| Build fix 1 | DONE — both literals wrapped in std::string(); no other bare-literal setText in rig files. |
| BUILD attempt 2 | **DONE — 0 errors. AlchemyTest.exe built 2026-08-16 07:30, 80.4 MB.** |

**BASE FEATURE READY FOR IN-WORLD TEST.** Binary:
`build-Windows-vs2026-os/newview/Release/AlchemyTest.exe`. User runs the viewer. Still NOT run:
the TUT test target (needs a `-DBUILD_TESTING=ON` configure).

---

## FOLLOW-ON (in progress): Master preset library + scale-aware geometry

User asked 2026-08-16 for a curated read-only built-in MASTER preset set (grouped separately, above
local presets), and — critically — the rig must **account for clone scaling** (anchor can be a ghost
clone scaled 0.05x..150x; presets in absolute meters frame a scaled subject wrong). Content is
reimagined, not a verbatim LSL port. Forward-facing, ship-quality.

Key code fact found: `LLVOAvatar::getUniformScale()` is virtual (base 1.0, llvoavatar.h:266);
`LLGhostAvatar` overrides it to `mEntityScale` (llghostavatar.h:116) — one polymorphic read on the
resolved anchor. The rig centre already tracks scale (reads mChest joint world pos); only the orbit
RADIUS, light FALLOFF, and HEIGHT/OffsetZ are absolute meters and must scale.

- Seed: `doc/CINE_LIGHT_RIG_MASTER_PRESETS_SEED.md`
- Design: DONE -> `doc/CINE_LIGHT_RIG_MASTER_PRESETS_DESIGN.md` (806+ lines). Overturned my "centre
  tracks scale for free" claim: joint world pos excludes outer render scale (llactormover.cpp:2998),
  so the rig mis-frames scaled clones TODAY — fixed via foot-pivot idiom. Exposure reads NOMINAL
  radius (geometry scales, EV does not — avoids +7.2-stop drift at 150x). Zero pipeline/shader edits.
- Scope grew (user 2026-08-16): also per-control reset buttons (weather-panel idiom) + Director tab
  icon; and the preset slate EXPANDED to 21 "proper cinema" masters (Classic compiled #0 + 20 in a
  bundled file). All folded into ONE delivery.
- Codex build: DONE — whole delivery (scale math + master library + reset buttons + tab icon + TUT
  cluster). 20 file presets + compiled Classic = 21; tab icon Command_Lightbox_Icon confirmed real
  (textures.xml:153). Codex flagged an internal design contradiction (SA-4 vs SA-12).
- Review: **OPUS adversarial, 4 slices** (user directive; matches CLAUDE.md line 43).
  - Slice 2 (controller geometry) DONE: **0 code must-fix.** Adjudicated SA-4 vs SA-12 IN THE CODE'S
    FAVOR — scaledPoint (alcinelightrig.cpp:95-104) mirrors updateEntityOuterTransform
    (llghostavatar.cpp:308-313) branch-for-branch; non-finite p2f → continue-with-0 lands the centre
    where the clone renders; SA-12's unscaled fallback would mis-frame ~0.65m on a 0.5x clone.
    Verified independently. Also proved default-on build is byte-identical for real avatars (scale
    only diverges on clones). Fix-round items: correct the SA-12 doc row; 3 nits.
  - Slice 3 (master library) DONE: **0 must-fix, 0 should-fix.** Transcription verified
    PROGRAMMATICALLY (counted, not eyeballed): 20 presets/intents/radii, 80 light rows, all indices
    in range, all name/index pairs consistent — 0 mismatches. Corrupt-file fallback provably
    Classic-only+warning in every case; migration rename-only with a correct uniquify guard, never
    overwrites/shadows/deletes a user file; scene round-trip stores base not name (no dangling). Only
    NITs (dir re-scan on refresh = pre-existing, not per-frame).
  - Slice 4 (UI) DONE: **0 must-fix, 0 should-fix.** All 29 reset buttons live (keys verified against
    settings.xml); reflow proven no-overlap by rect arithmetic (panel stays 350x1130); caption rows
    unselectable AND unsaveable; tab icon wired end-to-end (row consumed, name key matches
    floater_director.xml:1512). NITs: stray leading separator above first caption (cosmetic);
    caption literals duplicated across 2 TUs (maintenance hazard); kill-switch has no UI (by design).
  - Slice 1 (model + TUT cluster) DONE: **0 must-fix correctness defect** — exposure invariance and
    the s=1 no-op are genuinely enforced (reviewer regressed distance_ev and confirmed test 3 catches
    it; ceiling arithmetic CEIL*2.2==20.0 exact in F32). 2 SHOULD-FIX are TEST-QUALITY: sub-test 10
    pins against the model's own symbols (a co-edit is invisible → use literals 0.05/150); sub-test 4
    attenuation-ratio is algebraically tautological (passes with the ceiling removed). Plus nits.

  **ALL 4 OPUS SLICES DONE — ZERO must-fix across the whole delivery on the first review round.**
  Findings are test-strengthening + nits + doc. Loop exit condition met.

- Fix round: **RUNNING (Codex)** — strengthen the 2 weak tests (literals, real ceiling pin), fix
  vacuous/circular assertions, add fallback-centre finiteness gate, share the duplicated caption
  literals, suppress the stray leading separator, comment the subnormal guard.
- Design-doc corrections: DONE by Claude (SA-12 non-finite-p2f row; SA-4 guard attribution to the
  ghost; SA-2 subnormal note).
- Fix round DONE — all 8 items applied. Verified by Claude: panel now consumes shared caption
  constants (no duplicate); F2 ceiling pins independent (mOffX<=9.1f, mLightRadius>=19.9f at s=150);
  N3 finiteness gate added. Proportionate check for a 0-must-fix test/nit round; no full re-review.
- BUILD: **DONE, 0 errors.** AlchemyTest.exe built 2026-08-16 09:05, 80.4 MB. Preset file staged to
  build-Windows-vs2026-os/newview/Release/app_settings/cine_light_rig_presets.xml. Green first try.

**USER CONFIRMED IN-WORLD (2026-08-16): "the lights work."** First real in-world verification of the
feature. TUT test target still not run.

**MASTER PRESETS + SCALE: READY FOR IN-WORLD TEST.** Base rig + this delivery are both in the binary.
Still NOT run: the TUT test target (needs -DBUILD_TESTING=ON) — now carries the scale cluster too.

### FOLLOW-ON 2 (in progress): Mirror side-to-side fix + per-Kelvin colour-temp presets
User 2026-08-16, after confirming lights work in-world:
- **Mirror bug**: it flips FRONT-TO-BACK (light goes behind avatar), should flip SIDE-TO-SIDE (cheek
  to cheek). Root cause (proven): mirror negates yaw = reflect across the WORLD X-axis
  (alcinelightrigmodel.cpp:458); the orbit is world-framed (off_x=r*cos(yaw) into global X,
  alcinelightrigmodel.cpp:538), so mirror ignores avatar facing. Fix: reflect across the avatar's
  FACING azimuth — mirrored_yaw = wrap180(2*facing - base_yaw); facing = atan2(fwd.Y,fwd.X) from
  <1,0,0>*avatar->getRotation() in the region frame. facing=0 => identical to old -y (existing tests
  unchanged); only Mirror becomes face-aware, base yaw + Orbit stay world-relative.
- **Colour-temp masters**: 6 per-Kelvin neutral soft 3-points (2700/3200/4500/5600/6500/8000K),
  indices 21-26. Library -> 27 masters. File-only, loader generalises.
- Codex DONE. Mirror line = wrap180(2*facing - base_yaw) (alcinelightrigmodel.cpp:461); facing from
  ROOT joint world rotation (Codex's reasoned choice over getRotation() — Actor Mover/sitting write
  the root). Verified the geometry myself: reflecting across the forward line = left<->right swap
  (front/back preserved), the correct fix. 6 CT presets verified directly (26 file entries, template
  exact). Codex correctly noted facing=0/mirror-twice/NaN tests can't distinguish new from -yaw;
  discriminating power is in the facing=90/facing=37 assertions.
- Review: OPUS 1 slice DONE — **0 must-fix, 0 should-fix; fix is coordinate-frame correct.** Opus
  traced the frame end-to-end: root->getWorldRotation() forward = body facing in region axes (built
  by updateOrientation, llvoavatar.cpp:4799); root source strictly correct vs getRotation() under
  sit (object rot goes seat-relative) and Actor Mover (writes root, leaves object rot stale). North-
  facer front-left light: old->back-left (the bug), new->front-right (fixed). 2 microscopic nits
  (sub-ULP back-compat at base==180+null-root+big-orbit; intended lights-swing-on-turn) — not worth
  a round. No fix round.
- BUILD: **DONE, 0 errors.** AlchemyTest.exe built 2026-08-16 09:44. 26-entry preset file staged.
  **DEFECT FOUND before in-world test (Codex verify pass, 2026-08-16) — DO NOT TEST the built exe.**
  Mirror reflects the BASE yaw across facing then adds Orbit Yaw AFTER, so with nonzero Orbit Yaw the
  reflection plane is off by 2*orbit — a front cheek light can go front-to-back at OrbitYaw=90. Root
  cause = a SPEC ERROR in Claude's brief ("orbit applies after the reflect"), not a Codex mistake.
  Opus review missed it because every worked case + test used OrbitYaw=0.
  FIX: apply orbit FIRST, then reflect the oriented yaw: out.mYaw = mirror ? wrap180(2*facing -
  (base+orbit)) : wrap180(base+orbit). Reduces to current at orbit=0; mirror-twice still identity.
  - Codex fix DONE: computeLive reorders to reflect the ORIENTED (base+orbit) yaw across facing
    (alcinelightrigmodel.cpp:460). Added test<12> (discriminating nonzero-orbit cases). ALSO found an
    EXISTING test (test<1>, base yaw 12, orbit 30) whose expected value 18 encoded the BUG; corrected
    to -42 — verified independently correct by Claude (wrap180(2*0-(12+30))=-42). So a prior test had
    been locking in the bug through earlier rounds.
  - Codex read-only re-verify: **SAFE TO TEST IN-WORLD.** Facing-north + OrbitYaw 90: flipped light
    lands front-right (old gave 225 = behind). Sitting+orbit, extreme orbit, dead-front all clean.
    Only caveat = intended lights-follow-on-turn transition lag.
  - BUILD DONE, 0 errors. AlchemyTest.exe built 2026-08-16 09:59. **VERIFIED SAFE — READY TO TEST.**
    Mirror now swaps cheeks correctly at ANY facing AND any Orbit Yaw.

### FOLLOW-ON 6 (in progress): Multi-anchor group lighting (user 2026-08-16, for the VCam workflow)
Rig lights a GROUP as one unit (not per-light, not anchor-to-VCam). Anchor = a subset of 5 slots
{You,A,B,C,D} (mGroupEnabled + 5-bit mGroupSlots, session state, no settings keys). Centre = bounds
midpoint of per-member track-mode points (track-mode stays GLOBAL, composes per member). Scale =
max(s_i) + R_spread/r_nom into the UNCHANGED mSubjectScale path (SA-9 preserved: mSubjectScale never
touches exposure — VERIFIED distance_ev reads nominal radius, model:630/656). Facing = primary resolved
member. Single-member = STRUCTURAL fast path bitwise-today. Scene: 2 additive keys, no version bump.
- Seed: doc/CINE_LIGHT_RIG_MULTIANCHOR_SEED.md ; Design: doc/CINE_LIGHT_RIG_MULTIANCHOR_DESIGN.md (803L)
- Codex DONE. Verified by Claude: 3 model fns (sanitizeSubjectScale/groupBoundsCentre/
  groupSubjectScale); mGroupEnabled branch (single-anchor = else fast path); scene version stays 1,
  anchor_group additive; reflow panel 1364 + wrappers 1374/1364. Codex confirms single-anchor +
  one-survivor are structural fast paths, SA-9 chain byte-frozen, dedupe by UUID, 4 scene cases.
- Review: OPUS 2 slices. UI+scene slice DONE: **0 must-fix.** Reflow no-clip both hosts (rect
  arithmetic); all 4 scene cross-version cases clean, no version bump; controls live; dedupe honest;
  Reset All clears group; lockstep holds. SHOULD-FIX: group status text LIES when rig globally
  disabled/powered-off (mLastResolvedGroupSlots not cleared on shutdown/power-off/disable paths ->
  reads "2 of 2 lit" while dark; cosmetic, self-corrects next enabled tick). NIT: amber "none resolved"
  shown even with empty mask (neutral state truer). Model+tick slice (R1/SA-9/formula) still running.

### FOLLOW-ON 5 (DONE pending relink): Lens Gaze — ALL phases to feature-complete (user 2026-08-16)
Take the look-at-camera gaze follow feature (Phase 0 built) to feature-complete: implement Phases 1-4.
P1 targeting polish (torso/head lag differential = fix the rigid-block swing; ~3-5deg dead zone;
break-off past ~120deg). P2 clone life signals (ANIM_AGENT_EYE on clones so they blink). P3 UI
promotion (gaze controls into the shared machinima panel). P4 safety interlocks (R1 bone-lock feedback
guard — HIGHEST RISK, a wrong interlock ships an oscillating camera; + eyeline angular offset).
Anchors: gazePaint/applyGaze (llactormover.cpp ~2639-2916), research doc LENS_GAZE_RESEARCH_FINDINGS.md.
- Seed: doc/LENS_GAZE_ALLPHASES_SEED.md
- Design DONE -> doc/LENS_GAZE_ALLPHASES_DESIGN.md (639 lines). MAJOR finding (verified by Claude):
  the 3-week-old research is STALE — dead zone / break-off / slew / the R1 bone-lock interlock
  (gazeCameraSafe :3024 + hard-cut :3103) ALL already shipped. So the risky part (feedback
  oscillation) is DONE and fenced OFF-LIMITS. Remaining work only: P1 torso lag (new setting
  BDMergeGazeTorsoLagRatio default 1.8, torso-only chase), P2 clone blink (ANIM_AGENT_EYE +
  blink-param neutralize on teardown), P3 UI (new shared ALPanelLensGaze in both Move tabs, remove
  Path-tab block, full reflow table), P4 eyeline offset (±15/±10 pre-smoothing). + pure algazemath.h
  + TUT. Much lower risk than "all 4 phases" implied.
- Codex DONE (all remaining phases). Verified by Claude: gazeCameraSafe/hard-cut UNTOUCHED (R1 intact);
  5 new files present; BDMergeGazeTorsoLagRatio in; ratio==1 uses mApplied* (Codex fixed a real
  blueprint defect — pseudocode targeted unslewed target_*, but legacy torso uses mApplied* during seam
  slew, so exact-legacy would've been FALSE as written). Reflow: 6 wrapper heights + 10 widgets shifted.
- Review: OPUS 2 slices. UI slice DONE: **0 must-fix, 0 should-fix.** Reflow verified no-clip by rect
  arithmetic both hosts (17px/8px slack); all 14 controls wired; no dangling path-tab refs; per-actor
  store consumed by gazePaint; panel injected both hosts. 2 optional nits (per-frame findChild; a
  comment check). Animation-core slice DONE: **0 must-fix, 0 should-fix** (PROVED by git diff):
  ratio==1/eyeline==0 bitwise-legacy incl. seam-slew (Codex mApplied fix correct); R1/head-chain/Director
  byte-unchanged; trailing torso doesn't desync eyes; blink neutralized on all 5 teardown paths, never
  HEAD_ROT; ALGazeMath tests real+pure+registered. 2 in-world tuning watch items only (torso lead at
  1.8 across extreme behind-seam; pre-existing distant-clone LOD half-blink). BOTH SLICES 0 MUST-FIX.
  BUILD: COMPILED CLEAN, link-locked (LNK1104 — viewer running). ALL lens-gaze + track-mode objects
  compiled; only the final link to AlchemyTest.exe is blocked by the open viewer. RELINK when the viewer
  is closed (fast — just the link step): `cmake --build build-Windows-vs2026-os --config Release`.

  ===== PENDING RELINK (both reviewed 0-must-fix, compiled): track-mode toggle + lens-gaze all-phases.
        Close viewer -> relink -> both land in AlchemyTest.exe. =====

### FOLLOW-ON 4 (DONE pending relink): tracking-mode toggle (user 2026-08-16)
Track-mode toggle reviewed 0 must-fix, COMPILED clean; build only blocked by LNK1104 (viewer running).
Relink when the viewer is closed. Skeleton (default) / Body-stable combo in the Rig section.
Chest-joint tracking follows animated bones, so in-place animations swing the lights. Toggle:
Skeleton (mode 0, DEFAULT, current chest-joint path) vs Body-stable (mode 1, render-position + 1.2m,
ignores the animated bob). Body-stable REUSES the existing no-chest fallback expression
(alcinelightrig.cpp:1098), so scaledPoint/OffsetZ/damping apply identically; the only difference is
whether the chest joint is consulted. Controller-only, NO model/shader/pipeline, no TUT, no version
bump. New setting CineLightRigTrackMode (S32 default 0); combo in the Rig section by Follow damping;
reflow +1 row -> MUST bump panel + BOTH wrapper heights (the M1 lesson).
- Codex: **RUNNING** -> Opus review (branch correctness + default bitwise-unchanged + reflow reachable)
  -> build.

### FOLLOW-ON 3 (DONE, in binary): enhancement batch (user 2026-08-16)
Decisions fixed: Master Colour Temp = RELATIVE shift (preserves per-light gel); gobo textures =
BUNDLE a curated set; scope = everything EXCEPT gizmo drag (that's phased later).
Batch: (A) global Master Colour Temp relative shift; (B) per-light gobos; (C) bundled grayscale gobo
texture set; (D) gobo cinematic master presets; (E) shadow-policy fix-it button (raises
BDMergeMaxSpotShadows, cap 6 — user accepts 6 is the hard ceiling, no shader change); (F) radius UX
(already reaches 512m; fix stepping + make 20m reach cap legible, NOT a new control).
Key facts: colour = discrete profile index -> RGB, so relative temp shift = RGB warm/cool GAIN on top
of each light's base (not an index change); per-projector setLightTextureID already supported so
per-light gobos need NO shader change; bundled textures ship via textures.xml + viewer_manifest.
- Seed: doc/CINE_LIGHT_RIG_ENHANCE_SEED.md
- Design DONE -> doc/CINE_LIGHT_RIG_ENHANCE_DESIGN.md (849 lines). Overturned 3 seed assumptions w/
  proof (verified by Claude): NO textures.xml + NO manifest change (local PNGs load via
  getFetchedTextureFromFile, manifest already globs skins/*/textures/*/*.png); model does NOT
  gamma-correct (setLightSRGBColor linearizes in the viewer); MasterEV not in presets so master temp
  isn't either -> NO version bumps anywhere. Per-light gobos ride the EXISTING per-object light-image
  param -> ZERO pipeline/shader edits. Top hazard flagged: mGobo field must thread through 2 copiers.
- Codex build DONE (all 6 features). Claude spot-verified: 31 file presets (32 w/ Classic); mGobo
  threaded through cleanLight/copyCleanLights/computeLive/blendLight/render; version stays 1 (no
  bump); 7 gobo PNGs present + gen script re-run byte-stable (0 SHA changes); new settings keys in;
  temp shift 0 + gobo 0 are bitwise no-ops. NO pipeline/shader/manifest/textures.xml edit.
  Codex-flagged coverage nuance for the reviewer: test<14> catches a dropped computeLive gobo copy but
  NOT an independent drop of the private copyCleanLights assignment (every FX starts at gobo 0); the
  assignment is present & static-verified.

  ========================================================================
  >>> RESUMED. Opus review (3 slices). Gobo slice DONE: 0 must-fix, 0 should-fix. mGobo threaded
      through every copier (verified line-by-line); gobo 0 = bitwise current cookie; fallbacks ->
      default cookie never black. NIT (act at commit): the 7 gobo PNGs + gen_cine_gobos.py are
      UNTRACKED in git -> `git add` the cine_gobos/ dir + script or a fresh clone builds with no
      gobos.
      UI slice DONE: **1 MUST-FIX (M1)** + 1 cosmetic nit. M1 (verified): panel grew to height 1286
      but BOTH host wrappers keep stale heights -> bottom ~150px clipped in both hosts, the new shadow
      fix-it button AND "Reset all" unreachable. Fix: floater_cine_light_rig.xml embedded 1130->1286 +
      content 1140->1296; floater_director.xml cine_light_rig_embedded ->1286 + scroll_content ->1296.
      Presets transcribed exact (all 20 rows counted), no dead controls, fix-it correct, radius UX ok.
      N1 nit: fix-it label "Allow 2" when 1 requested (cosmetic, hand-lowered setting only).
      Colour-math slice DONE: **1 MUST-FIX (M2), code is CORRECT.** Reviewer independently validated the
      colour science against blackbody physics (Planck + CIE CMFs, ~0.3% agreement) — gains proven
      physically correct; shift==0 bitwise no-op; gel order preserved; correct colour space; no
      intensity interaction. M2 = test<13> asserts exact 1.0 on a saturated channel that is 1 ULP low
      (linear clamp re-encoded to sRGB = 0.99999994); ensure_equals is exact -> test fails at runtime.
      Fix = tolerance on the 2 assertions (test-only; colour code untouched). Design doc §5.6 corrected.

      ALL 3 SLICES IN. 2 MUST-FIX total (M1 wrapper heights, M2 test tolerance) — both tiny, neither
      touches shipping logic. Fix round RUNNING (Codex): M1 4 heights + M2 2 assertions + N1 cosmetic.
      Still to do at commit: `git add` the untracked cine_gobos/ dir + scripts/gen_cine_gobos.py.
      Fix round DONE + verified: both hosts now 1286/1296 (fix-it ends 1249, Reset all 1277, inside
      range); test assertions use fabs<=2e-3f; colour .cpp untouched; N1 done (hint+button share the
      clamped count). Still to do at commit: git add cine_gobos/ + gen_cine_gobos.py.
      BUILD 1 FAILED — 3 never-compiled API mismatches: MIPMAP_YES undeclared (add #include
      llviewertexturelist.h to alcinelightrig.cpp) + cascade operator= ambiguous; LLTextBox has no
      getColor() (alpanelcinelightrig.cpp:156 radius amber cue -> capture default a supported way).
      Build fix done (llviewertexturelist.h include + LLUIColorTable default colour). BUILD 2 CLEAN:
      AlchemyTest.exe built 2026-08-16 12:01, 0 errors; 31 presets + 7 gobos staged in build tree.
      **ENHANCEMENT DELIVERY READY FOR IN-WORLD TEST.** Reminder: git add cine_gobos/ + gen_cine_gobos.py
      before committing (untracked; local build has them, a fresh clone would not). <<<
  RESUME: run the OPUS adversarial review of this enhancement delivery, focused on:
    - the colour-gain math (§A.2 mired->linear-RGB trim: relative-gel-order preserved, shift==0
      bitwise no-op, no intensity/headroom interaction, gain literals correct)
    - mGobo field threading (the copyCleanLights coverage gap Codex flagged)
    - the 5 gobo presets transcription (programmatic count vs §D, like the slice-3 precedent)
    - texture-registry residency / fallback (R1/R2: gobo decode stays resident; missing->default cookie)
    - panel reflow (height 1130->1286, 34 reset buttons, no overlap, both hosts)
  Then batch fixes -> re-review to 0 must-fix -> BUILD. Brief: CINE_LIGHT_RIG_ENHANCE_CODEX_BRIEF.txt.
  Design (normative): doc/CINE_LIGHT_RIG_ENHANCE_DESIGN.md. Also still open: the faint-dark-outline
  emitter bug (independent; investigate FORCE_INVISIBLE box shadow/occlusion leak).
  ========================================================================

### In-world watch list (master-presets delivery)
- **Scale is the headline.** Anchor the rig to a ghost clone and drag Ghost Studio's scale slider
  0.05x -> 6x: framing/falloff must track, EXPOSURE must stay constant (the anti-drift rule). Firelight
  and Uplight are the sharpest probes for the foot-pivot centre fix (they were mis-framed before).
- **Unscaled path must be pixel-identical** to the earlier binary at scale 1.0 (real avatars are
  always 1.0). If it isn't, the s==1 fast path regressed — kill-switch CineLightRigScaleAware=false
  restores old behaviour.
- Hard-beam presets (Film Noir, Split, Top Light, Sci-Fi) at 0.05x/0.5x: the Snoot must not flood a
  doll into a wash (emitter-box scaling SA-10).
- Master combo: Built-in group above My-setups; 21 masters; Delete greyed on any master; saving over
  a master name refused; 29 per-control reset buttons; Lights tab now shows the lightbox icon.

**Loop closed:** 4 review rounds, every one 0 must-fix. Building now; the compiler is the next
independent signal (this feature has never been compiled — the TUT test target especially).

### Carried into a post-build fix pass (cosmetic, self-healing, never touch live settings/render)
- Stale setup dropdown can be reopened while a cross-host refresh is pending; picking a
  just-deleted setup is a silent no-op load and the combo shows the dead name until focus leaves.
  Fix: apply the pending refresh in the combo's prearrange_callback.
- Delete/Reset confirm-dialog can force-wipe a name typed into the setup combo while the dialog
  was open. Fix: force-refresh only on Save (text already consumed), defer the dialog callbacks.
- Plus nits: Reset-All cross-panel label inconsistency; dead `if(panel)` guard; pending
  selection/allow-empty strings not cleared with the bit; alt-tab-away keeps refresh queued.
| Build | **NOT STARTED — blocked on round 2 reaching 0 must-fix** |

---

## Resume here

1. Collect the three outstanding round-2 review results (controller lifetime, model math,
   UI). They were running when the session paused; if their output was lost, re-run them —
   the prompts' substance is preserved in §"Round 2 scope" below.
2. If any MUST-FIX: write a fix brief, send to Codex (`--prompt-file`, `--effort high`,
   `--resume-last`), re-review, loop. **No build between rounds.**
3. At 0 must-fix, build ONCE:
   ```
   cmake --build build-Windows-vs2026-os --config Release
   ```
   Output: `build-Windows-vs2026-os/newview/Release/AlchemyTest.exe`
   (`LNK1104` on that exe means the viewer is open holding the link lock — poll and retry.)
4. Then in-world testing by the user. Claude cannot run the viewer.

**Expect the build to find things.** Nothing in this feature has ever been through a
compiler. The unit-test target in particular has never been compiled — round 1 found that
`alcinelightrigmodel.cpp` used `DEG_TO_RAD` with no route to `llmath.h` in the no-PCH test
target, which a viewer-only build would have reported as green.

---

## Round 2 scope (what the three outstanding reviews were asked to attack)

A fix is code too. Two round-1 fixes rewrote code that round 1 had explicitly PROVEN correct;
that is the hazard this round exists to catch.

- **Controller lifetime** (`alcinelightrig.*`, `llappviewer.cpp`) — M3 added new emitter
  destruction paths into code round 1 proved free of dangling references. Does
  `setIsLight(false)` still precede `markDead()` on the NEW paths? Can power-cycling
  oscillate create/destroy per frame? Does Shaft/Hero survive destroy/recreate the way it
  survives a region change? Plus S1 (undamped movement), M4 (scene semantics: a pre-feature
  scene with no `light_rig` block must NOT clear Shaft/Hero, while a block that omits the
  arrays MUST), N3 (`LLCachedControl` conversion), N5–N7.
- **Model** (`alcinelightrigmodel.*`, its TUT test) — is `DEGREES_TO_RADIANS` numerically
  identical to the `llmath.h` value it replaced (a truncated literal shifts every light
  position)? Is Codex's S2 interpretation right (post-headroom/pre-clamp, ON gate
  pre-headroom) and does it reproduce the LSL reference ratio? And critically: are the NEW
  tests real — would each fail if the thing it tests were broken? The test they replaced
  looked rigorous and proved nothing, because the output sanitiser independently enforced
  every assertion.
- **UI** (`alpanelcinelightrig.*`, XUI, settings, scene round-trip) — does every U32 seed
  value survive widget → settings → scene file unchanged? LLSD has no unsigned integer type,
  which is exactly where a U32 quietly becomes something else. Plus S6 (did gating the combo
  sync introduce a stale-forever case?), S7 (cross-instance refresh: destructor cleanup,
  use-after-free, recursion), N4 (cached child pointers), N10 (UI ranges must match model
  clamps exactly, in both directions).

---

## Round 2, slice 1 — RETURNED CLEAN

Gizmo GL guard + the nine pipeline gate exemptions + `llselectmgr`: **0 must-fix,
0 should-fix**, 2 no-action nits.

- Gizmo flush ordering verified independently: explicit `gGL.flush()` at
  `alcinelightrig.cpp:1051` precedes all restoration (`LLGLSUIDefault` destructs at scope
  exit, 1053); all four early returns precede guard construction at 977. The rewrite is
  strictly stronger than the original, which was correct only by accident (it relied on
  `LLGLState`'s destructor flushing before mutating, innermost-first).
- Nine exemptions confirmed by truth table in both shapes: six world-light sites read
  `!isCineRigEmitter() && !gate()`, three projector sites read
  `isCineRigEmitter() || gate()` with explicit parentheses. Off-path reduces byte-for-byte
  to the pre-change condition at all nine. No missed tenth gate site.
- Omni emitters provably cannot enter the projector path: `setLightTextureID(null)` disables
  the light-image param, so `isLightSpotlight()` is false.

---

## Round 2, slice 2 — MODEL: 0 must-fix. Code sound; 3 test-coverage gaps.

The model code itself was verified against both LSL references and the design doc and holds.
The three SHOULD-FIX items are all **tests that would not fail if the thing they test broke** —
carry these into the next fix round:

1. **Omni intensity is never asserted away from saturation.** Both existing omni-intensity
   assertions sit at the clamp ceiling (EV 4, where every candidate implementation gives 1.0).
   The exact bug class S2 fixed — which value the bounce is computed from — is untested.
   Discriminator: EV 0, headroom 2, ratio 0.45 → correct 0.1125; the regression gives 0.45.
2. **The omni ON gate's headroom placement is untested on the ON side.** Regressing the gate
   from pre-headroom to post-headroom passes the entire suite. Discriminator: EV −8.5,
   headroom 2, ratio 0.45 → pre = 0.00124 (must be ON), post = 0.00031 (would be off).
3. **Explosion's beat-44 ember replay is unpinned.** The code is correct (verified bit-exact),
   but corrupting `cycle_start + 44` to `counter` — which would re-roll the ember on every
   afterglow step and break scrub reproducibility — passes every test, because the latch
   assertion checks light 2's decay latch rather than light 1's hash-keyed ember.

Nits: null-name UB backstop in the table tests; interior FX intervals and beams 0/1 unpinned
(all 33 interval values were hand-verified against the LSL, so this is a regression-guard gap,
not a present bug); the omni `mClipped` flag test does not separate it from the projector's.

Confirmed good: `DEGREES_TO_RADIANS` is bit-identical to `llmath.h`'s `DEG_TO_RAD` (both round
to F32 `0x3c8efa35`). Codex's S2 interpretation is correct — and at headroom 0 the model
reproduces the LSL exactly, which the design doc names as the intentional escape hatch. The
LSL's 1:1 key:bounce ratio under clipping turns out to be a double-saturation artifact rather
than an authored value. N1 and N2 both verified, including that the real N2 regression test is
the 170→−160 case, not the 170→−170 one. Mirror-twice identity holds at every ±180 boundary
including the −0.0 hazard; all 33 FX re-checked against the LSL pack with no divergence beyond
the deliberate deterministic substitutions.

---

## DECISION WAITING FOR THE USER

Round 2 reached **0 must-fix**, which is the documented condition for building. But 8 should-fix
items remain, and 4 of them are real behaviour bugs rather than polish. Two options:

- **A — fix round 2, then build.** Costs one more Codex pass + one more review round.
- **B — build now**, test in-world, fix afterwards. Faster to something runnable; ships four
  known bugs into the test session, one of which (seed 0) quietly breaks the determinism story
  the feature is sold on.

Recommendation: **A for items 1-4 below, defer 5-8.** Items 1-4 are cheap and two of them are
data-loss/behaviour bugs the in-world test would not reveal.

### Fix round 2 backlog (all confirmed, none blocking a build)

**Behaviour bugs**

1. **Seed 0 is accepted by the UI but aliased to the default by the model.** Round 1 made 0
   survive storage, but `alcinelightrigmodel.cpp:400` and `:163` still coalesce
   `seed ? seed : DEFAULT_SEED`, so seed 0 renders bit-identical to 324508639 while the alert
   text promises "0 through 4294967295". Either reject 0 in `commitSeed` or honour it.
2. **A pre-feature scene load wipes live Shaft/Hero while everything else leaks through.**
   `applySceneData` clears both arrays *before* the map/version check, but on that early-return
   path the anchor, running FX, and entire base setup survive untouched — producing exactly the
   frankenstate the clear was meant to prevent. The clear is only coherent for a *present* v1
   block that omits the arrays. This one came from my round-1 brief conflating "no light_rig
   block" with "block omitting the arrays".
3. **A future `light_rig` version half-applies silently.** Same early-return: a v2 block clears
   shaft/hero and resets pending FX, then ignores everything else and logs nothing. The outer
   scene loader's policy for unknown versions is warn-and-preserve (`llfloaterdirector.cpp:1005`,
   the Prism precedent). Match it.
4. **The N5 retry backoff is keyed to presentation time**, so a single creation failure followed
   by a paused/held transport clock strands the rig dark indefinitely — pre-fix it retried every
   frame. Related: an omni-only failure blacks out the healthy projectors for the whole backoff.

**Test-coverage gaps — each would let a real regression through undetected**

5. Omni intensity is never asserted away from saturation, so the exact bug class S2 fixed is
   untested. Discriminator: EV 0, headroom 2, ratio 0.45 → correct 0.1125, regression 0.45.
6. The omni ON gate's pre-headroom placement is untested on the ON side — regressing it to
   post-headroom passes the whole suite. Discriminator: EV −8.5, headroom 2, ratio 0.45.
7. Explosion's beat-44 ember replay is unpinned: corrupting `cycle_start + 44` to `counter`
   breaks scrub reproducibility and still passes every test.
8. Deleting or resetting a setup now resets *every* live panel's combo to "Classic 3-Point"
   without loading it — label says Classic, live settings are still the old setup. Round-1's
   cross-instance refresh spread a pre-existing single-panel wart to both hosts.

Nits recorded in the review transcripts (not reproduced here): per-frame `rigCookie()` string
re-parse with warn-spam on a malformed UUID; leading zeros persisting in the seed display;
silent scene-seed parse failure; `shutdown()`'s reset gated on `mRegion`; per-frame cast-signature
rebuild in `draw()`; null-name UB backstop in the table tests; unpinned interior FX intervals and
beams 0/1; omni `mClipped` not separated from the projector's.

### Known behavioural cost of round 1, accepted deliberately

M3 destroys emitters on power-off instead of darkening them, so power-on now mints **new UUIDs**
and the spot-shadow slot auction restarts from zero — a shadow pop of a frame or more on every
power-on and every anchor blip. Pre-fix this was seamless. This is the price of not strip-mining
the deferred light budget; worth confirming in-world that the pop is acceptable.

---

## Things a future session must not re-litigate

- `indra/llprimitive/*` clamps ([0,1] light intensity, 20 m radius) are a **design input,
  not a bug**. The EV headroom re-base exists because of them. Do not "fix" them.
- Gate BODIES (`bdmerge_should_render_light` / `bdmerge_should_render_projector`) stay
  untouched; exemptions live at call sites only.
- The pipeline's count-before-filter ordering in `renderDeferredLighting` is stock shared
  code and off-limits; the rig works around it by destroying emitters rather than darkening
  them.
- Codex must never build. Claude builds.

## Deferred out of delivery 1 (decided up front, design doc §8)

Per-light gobo patterns; N>4 lights; new hotkeys; renderer-side >1.0 intensity headroom;
gizmo drag-editing (v1 gizmo is display-only). Plus the pre-existing projvol/shadow
decoupling fix, tracked separately in `doc/MACHINIMA_FEATURE_BACKLOG.md`.

## Artifacts

| File | What |
| --- | --- |
| `doc/CINEMATIC_LIGHT_RIG_DEEP_DESIGN.md` | the spec (normative) |
| `doc/CINE_LIGHT_RIG_DESIGN_SEED.md` | the brief it answers |
| `doc/reference/cinematic_studio_render_engine_v20.lsl` | LSL rig engine (behavioural truth) |
| `doc/reference/cinematic_studio_fx_pack1.lsl` | LSL FX, 33 effects |
| `CINE_LIGHT_RIG_CODEX_BRIEF.txt` | implementation brief |
| `CINE_LIGHT_RIG_GIZMO_BRIEF.txt` | gizmo follow-up brief |
| `CINE_LIGHT_RIG_FIX_ROUND_1.txt` | all 24 round-1 findings |

The three `*.txt` briefs at repo root are scratch and can be deleted once the feature lands.

## Open question for the user

CLAUDE.md line 43 records a 2026-08-03 directive that Fable is retired for this loop (Codex
implements, Opus reviews). This session used **Fable for both the deep design and all
adversarial review**, at the user's explicit direction. If that is the standing arrangement,
CLAUDE.md should be amended so the next session does not re-litigate it.
