# Cinematic Light Rig — Master Preset Library + Scale-Aware Geometry — Deep Design

**Status:** design pass complete. **No source modified.**
**Date:** 2026-08-16.
**Answers:** `doc/CINE_LIGHT_RIG_MASTER_PRESETS_SEED.md` (sections A, B, C, and the
appended D). **Feeds:** a Codex `--prompt-file` implementation brief.
**Base architecture:** `doc/CINEMATIC_LIGHT_RIG_DEEP_DESIGN.md` (normative for the
existing feature). **Current state:** `doc/CINE_LIGHT_RIG_STATUS.md` — the base
feature is built (AlchemyTest.exe 2026-08-16), not yet tested in-world.

Claim labels per CLAUDE.md: **PROVES** = I read the code at the cited line.
**IMPLIES** = documentation/comment or an unbroken but unread chain.
**INFERENCE** = reasoning; could be wrong. Guesses say "guess".

---

## 0. Executive summary

Four additions in one delivery, all inside the rig's own files plus one shipped
data file — **zero pipeline.cpp edits, zero shader edits, zero scene-schema
changes, one new settings key**:

1. **Scale-aware geometry (§A).** The rig reads the anchor's uniform scale each
   tick via the polymorphic `avatar->getUniformScale()` (confirmed: base 1.0 at
   `llvoavatar.h:266`, ghost override at `llghostavatar.h:116`) and:
   - moves the rig centre by the **foot-pivot mapping** the fork's camera and
     actor mover already use — because, overturning the seed, **joint world
     positions do NOT include the clone's outer render scale**
     (`llactormover.cpp:2998-3001`); the seed's "centre is right for free" is
     wrong;
   - multiplies the orbit radius (and therefore both light-falloff radii and
     the emitter box) by the smoothed scale, clamped to the regime
     `[0.1 m, 20/2.2 ≈ 9.09 m]` where the engine's `dist = d/lightSize`
     attenuation (`deferredUtil.glsl:807`) keeps the subject's illumination
     **exactly invariant**;
   - feeds **NOMINAL radius, never scaled radius, into `EV_dist`** — scaling
     the EV input would add `log2(s)` stops of drift (+7.2 stops at 150×),
     which in this renderer's attenuation model is a bug, not physics (§A.4);
   - guards 0.05..150, ≤0, NaN, subject-lost, and mid-shot rescale (rides the
     existing centre damping; snap by default). Presets stay NOMINAL at 1.0.
   All scale math lives in the pure model (new `Globals::mSubjectScale`) and is
   TUT-tested (§5). A kill-switch `CineLightRigScaleAware` (BOOL, default TRUE)
   restores today's behaviour bit-exactly.
2. **Master preset library (§B).** A bundled read-only LLSD file
   `app_settings/cine_light_rig_presets.xml` (the fork's shipped-data idiom:
   `llvograss.cpp:114`, `llvotree.cpp:114`, `fsfloaterposestand.cpp:120`;
   packaged for free by the `app_settings` prefix at `viewer_manifest.py:80`),
   with **compiled `classicSetup()` always injected as master #0** so a missing
   or corrupt file can never blank the rig. Grouped combo via the in-tree
   `add_labeled_separator` idiom (`bdmergeenvlibrary.cpp:59-67`). Save/Delete
   refuse every master name; a one-time startup migration renames any local
   preset that collides with a master. **Scene round-trip needs no work**: the
   seed's premise that scenes reference presets by name is wrong — scenes carry
   the denormalized base (`alcinelightrig.cpp:1269`), so master renames can
   never dangle.
3. **Curated slate (§C).** Ten masters (Classic + nine reimagined
   cinematographer looks), fully specified per light against the existing 24
   profiles / 3 beams, each with a scaled-clone reading note.
4. **Panel reset buttons + Director tab icon (§D).** 29 per-control mini reset
   buttons mirroring the weather panel exactly (`panel_weather_settings.xml`
   rows + `registerWeatherResetControl`, `alpanelweathersettings.cpp:35-53`),
   laid out together with the grouped combo in one panel revision; and the
   Lights tab entry in the Director icon map (`llfloaterdirector.cpp:231-242`)
   using the existing `Command_Lightbox_Icon` (`textures.xml:153`).

The two questions the pass was ordered to check first are answered in §1.3 and
§1.4: **(1)** `getUniformScale()` is the complete scale signal **today** —
no real avatar can currently be scaled at all (the outer transform hard-requires
`mIsLocalOnly`, `llvoavatar.cpp:887`), and the planned real-avatar scale feature
(Phase 1 brief) explicitly keeps the same accessor — but the rig must ALSO do
its own foot-pivot mapping of joint positions, which the seed missed.
**(2)** Radius, both falloff radii, height/OffsetZ and the emitter box scale;
EV/intensity must not — and `EV_dist` must keep reading the nominal radius or
exposure drifts by `log2(s)` stops (§A.4 derives this from the shader).

---

## 1. Verification — what the code proves, and where it contradicts the seed

### 1.1 Seed claims CONFIRMED

| # | Claim | Evidence | Label |
|---|---|---|---|
| C1 | `LLVOAvatar::getUniformScale()` is virtual, base returns `1.f` | `llvoavatar.h:266` | PROVES |
| C2 | `LLGhostAvatar` overrides it to return `mEntityScale` | `llghostavatar.h:116`; field default `1.f` at `llghostavatar.h:188` | PROVES |
| C3 | Ghost scale range 0.05..150 | `alghoststudio.h:62-63` (`GHOST_SCALE_MIN/MAX`); clamped at the apply chokepoint `LLGhostAvatar::setEntityScale`, `llghostavatar.cpp:385-389` | PROVES |
| C4 | The rig resolves the anchor once per tick, giving one polymorphic read | `alcinelightrig.cpp:835` (`LLDirectorCast::instance().resolve(mAnchor)`) | PROVES |
| C5 | Rig centre = `mChest` joint world position, fallback render position + 1.2 m | `alcinelightrig.cpp:923-931` | PROVES |
| C6 | `CineLightRigOffsetZ` added to centre, clamped −10..10, absolute metres | `alcinelightrig.cpp:932-937` | PROVES |
| C7 | Projector falloff radius = `radius × 2.2`, omni = `radius × 1.5` | `alcinelightrigmodel.cpp:547`, `:571` | PROVES |
| C8 | `EV_dist = log2(radius / 1.5)` enters every light's EV | `alcinelightrigmodel.cpp:511`, `:523` | PROVES |
| C9 | Client clamps light radius to 20 m, intensity to [0,1] | `llprimitive.cpp:74-76` (`LIGHT_MIN_RADIUS 0`, `LIGHT_MAX_RADIUS 20`); base design O1 | PROVES |
| C10 | Only one built-in today; local presets are LLSD XML under `<user_settings>/cine_light_rig/` | `alcinelightrig.cpp:46-47`, `:1131-1146` | PROVES |
| C11 | `setupNames()` flat: Classic + sorted local files | `alcinelightrig.cpp:1148-1165` | PROVES |
| C12 | `loadSetup` special-cases Classic; `saveSetup`/`deleteSetup` refuse only Classic; version gate `== 1` | `alcinelightrig.cpp:1170-1173`, `:1186`, `:1202`, `:1236` | PROVES |
| C13 | Panel combo lists whatever `setupNames()` returns; Delete disables only on Classic | `alpanelcinelightrig.cpp:316-320`, `:604-607` | PROVES |
| C14 | Emitter box fixed 0.25 m, re-asserted every applyFrame | `alcinelightrig.cpp:49`, `:741-742`, `:772-773` | PROVES |

### 1.2 Seed claims OVERTURNED (trust the codebase)

**O-A — "A scaled clone's chest sits at the correctly-scaled height, so the
CENTRE is right for free" is FALSE.** The ghost's scale is a client-only
**outer render transform** — a foot-pivoted matrix applied at draw time
(`llghostavatar.cpp:292-338`: `rendered = foot + (native − foot) × s`, built
into `LLClientOuterTransform::mCurrent` and consumed by the render paths in
`lldrawable.cpp:1405`, `llface.cpp:2334-2340`, `llvovolume.cpp:1542/5525-5530`,
`lldrawpool.cpp`). **The skeleton itself is never scaled**; joint world
positions stay native. The fork's own comment says it outright:

> "Joint world positions do not include a ghost avatar's client-only outer
> render scale. Match `LLGhostAvatar::updateEntityOuterTransform()` (and the
> cinematic-camera equivalent) exactly: uniform scale about the foot"
> — `llactormover.cpp:2998-3001` (PROVES)

Both existing consumers therefore map joints manually:
`gazeRenderedJointPosition` (`llactormover.cpp:3002-3019`) and
`cc_scaleAboutSubjectBase` (`llcinematiccamera.cpp:238-247`), each with a
literal `scale == 1.f` no-op fast path. On a 0.5× clone the rig's current
`chest->getWorldPosition()` (`alcinelightrig.cpp:925`) returns the FULL-SIZE
chest height — the centre is wrong today for every scaled clone, not right for
free. §A.2 adopts the established idiom.

**O-B — "a saved scene references a preset by NAME; a master name must resolve
on load" is FALSE.** `sceneData()` stores the **denormalized base**
(`data["base"] = setupToLLSD(setup)`, `alcinelightrig.cpp:1269`) and no setup
name anywhere; `applySceneData` applies that base directly
(`alcinelightrig.cpp:1343-1350`). Scenes are self-contained by design (base
design §6.6.3). Consequence: scene round-trip requires **no master-library
work at all**, and future master renames/removals can never dangle a scene
reference. The 42-key settings block also rides `sceneSettingsList()`
unchanged.

**O-C — seed line-number nits (content correct, lines off-by-one):**
`GHOST_SCALE_MIN/MAX` are at `alghoststudio.h:62-63` (seed said 62-63 —
confirmed exact). `presetsDir()` begins at `alcinelightrig.cpp:1131`,
`presetPath()` at `:1143`, `setupNames()` at `:1148` — all as seeded. PROVES.

### 1.3 The scale signal — VERIFIED, with the real-avatar answer

**Is `getUniformScale()` overridden as claimed?** Yes — the only two
definitions in the tree are the base (`llvoavatar.h:266`, returns `1.f`) and
the ghost override (`llghostavatar.h:116`, returns `mEntityScale`). PROVES
(project-wide grep; consumers are `llactormover.cpp:3005` and seven sites in
`llcinematiccamera.cpp`).

**Does a uniform-scaled REAL avatar report through it?** The precise answer:

1. **Today a real avatar CANNOT be uniform-scaled at all.** The only scale
   entry point is `LLGhostAvatar::setEntityScale` (`llghostavatar.cpp:385`),
   called exclusively from `alghoststudio.cpp` (nine sites, all clone
   instances). No UI, chat command, or code path scales a non-ghost. PROVES.
2. **The outer-transform machinery is structurally closed to real avatars:**
   `LLVOAvatar::hasClientOuterTransform()` requires `mIsLocalOnly`
   (`llvoavatar.cpp:884-889`: `return mIsLocalOnly && outer && outer->mEnabled
   && !is_approx_equal(outer->mScale, 1.f);`). A server-backed avatar can
   never activate it. PROVES.
3. Therefore **`getUniformScale()` is today the complete signal**: real
   avatars are always 1.0 (correct — they cannot be scaled), ghosts report
   their entity scale. The rig needs to read nothing else.
4. **Forward compatibility:** the fork's real-avatar scale feature is a
   **plan, not code** — `doc/AVATAR_LOCAL_SCALE_PHASE1_BRIEF.md` Part B
   explicitly generalizes by making `getUniformScale()` "backed by a real
   base-class field" on `LLVOAvatar`, with `LLGhostAvatar::mEntityScale`
   unified into it (IMPLIES — brief read in full; none of it implemented:
   no `setLocalScale` exists, no scale UI in xui, the `mIsLocalOnly` gate is
   still present). When Phase 1 lands per its own brief, the rig's polymorphic
   read keeps working unchanged. **Requirement exported to Phase 1** (add to
   that brief when it runs): `getUniformScale()` must remain the single
   accessor, and joint positions will presumably remain native — the rig's
   foot-pivot mapping (§A.2) is already written for that world.

**What the rig must ALSO read (the part the seed missed):** because joints are
native (O-A), the scale-aware rig reads three additional public avatar APIs,
all already consumed the same way by `llactormover.cpp:3011-3018`:
`getRootJoint()->getWorldPosition()`, `getPelvisToFoot()`, and (fallback path)
`getRenderPosition()`. No private state, no outer-transform handle needed —
the foot-pivot formula reconstructs the rendered point from public data.

### 1.4 The attenuation math that decides the exposure question

The deferred lighting attenuation in this fork (PBR path, and the classic
helper both spot and point paths use):

- `float dist = lightDist / lightSize;` — **distance is normalized by the
  light's falloff radius** — `deferredUtil.glsl:807`. PROVES.
- `if (dist <= 1.0)` — **hard cutoff**: a light contributes nothing beyond its
  falloff radius — `deferredUtil.glsl:808`. PROVES.
- `calcLegacyDistanceAttenuation(dist, falloff)` is a pure function of the
  normalized `dist` and the falloff parameter — `deferredUtil.glsl:360-369`.
  PROVES.

Consequence chain (INFERENCE from the proven lines, load-bearing for §A.4):
the subject sits at `dist = orbitRadius / (orbitRadius × 2.2) = 1/2.2 ≈ 0.4545`
— **independent of orbit radius** so long as the falloff radius is the
unclamped `2.2 ×` product. Scaling orbit radius and falloff radius together by
`s` leaves the subject's attenuation **bit-for-bit invariant** (same `dist`,
same `falloff`). This invariance holds until `radius × 2.2` hits the 20 m
`llprimitive` clamp, i.e. up to orbit ≈ 9.09 m; past it `dist` rises and the
light dies entirely at orbit ≥ 20 m (`dist > 1`).

---

## Section A — scale-aware geometry (the complete design)

All rules are numbered and testable. "s" is the subject scale. The invariant
sits first because everything else derives from it:

**SA-0 (nominal invariant).** Setups — settings, local preset files, master
presets, scene `base` blocks — are authored NOMINAL, for a scale-1.0 subject.
No file format carries a scale. The rig adapts at apply time, per frame. A
setup saved while anchored to a scaled clone is identical to one saved on an
unscaled subject (settings hold nominal values; there is nothing to un-bake —
scale never writes into settings). Scene schema stays version 1 untouched:
scale is re-read live from the anchor after `applySceneData`, so a scene made
with a 3× clone plays correctly on a later 0.5× clone by construction.

### A.1 Signal, sanitation, kill-switch

**SA-1.** Per tick, after anchor resolution (`alcinelightrig.cpp:835`):
`F32 s_raw = avatar->getUniformScale();`. This is the ONLY scale source. Never
read `mEntityScale`, the outer-transform handle, or ghost-studio state.

**SA-2 (guards).** In the model's `sanitizeGlobals` (new field
`Globals::mSubjectScale`, default `1.f`):
`if (!std::isfinite(s) || s <= 0.f) s = 1.f;` then
`s = std::clamp(s, SUBJECT_SCALE_MIN, SUBJECT_SCALE_MAX)` with new model
constants `SUBJECT_SCALE_MIN = 0.05f`, `SUBJECT_SCALE_MAX = 150.f`.
(Implementation note, post-review: the collapse condition ALSO includes
`s <= FLT_MIN` — a subnormal positive scale collapses to nominal 1.0 rather
than clamping to 0.05. This is an intentional strengthening beyond the line
above, to avoid denormal geometry; it is unreachable in practice because
`GHOST_SCALE` clamps to [0.05,150] at the source. The code carries a comment
saying so. The test cluster's degenerate case pins this behaviour.)
These are
numerically paired with `GHOST_SCALE_MIN/MAX` (`alghoststudio.h:62-63`); the
pure model cannot include a viewer header (the `stdtypes.h`-only discipline),
so both sides get a cross-referencing comment, exactly the maintenance
contract `alghoststudio.h:55-61` already documents for its other mirrors.

**SA-3 (kill-switch).** One new settings key: `CineLightRigScaleAware`
(BOOL, default TRUE, `settings.xml`). FALSE forces `s_raw = 1.f` before
sanitize — bit-identical to the just-built binary (the s = 1 fast paths below
make this provable, not aspirational). This is the in-world-test insurance for
a behaviour change to an untested-in-world feature; it is the only new key.

### A.2 The rig centre (fixes the latent mis-framing, O-A)

**SA-4.** Replace the centre computation (`alcinelightrig.cpp:921-931`) with
the established foot-pivot idiom. NOTE (corrected post-review): the pivot
ALGEBRA matches `gazeRenderedJointPosition` (`llactormover.cpp:3002-3019`), but
the degenerate GUARDS must be copied from the GHOST, not the actor mover —
`gazeRenderedJointPosition` uses raw `getPelvisToFoot()` with no finite check.
The three guards below (non-finite p2f → 0 and continue; non-finite foot →
bail) come verbatim from `llghostavatar.cpp:307-313`, which is the render's own
transform and therefore the authority:

```
scaledPoint(av, p, s):                    # file-local helper in alcinelightrig.cpp
    if (s == 1.f) return p;               # bit-identical fast path (llactormover.cpp:3006)
    root = av->getRootJoint(); if (!root) return p;
    foot = root->getWorldPosition();
    p2f = av->getPelvisToFoot(); if (!llfinite(p2f)) p2f = 0;
    foot.z -= max(0.f, p2f);              # llghostavatar.cpp:307-309 guards, verbatim
    if (!foot.isFinite()) return p;
    return foot + (p - foot) * s;
```

- Chest path: `centre_agent = scaledPoint(avatar, chest->getWorldPosition(), s_raw)`.
- Fallback path (no chest joint): `centre_agent = scaledPoint(avatar,
  avatar->getRenderPosition() + LLVector3(0,0,1.2f), s_raw)` — the +1.2 m
  guess must sit INSIDE the scaled mapping (a 150× giant's chest guess is
  180 m up, not 1.2 m).
- This is the fork's third copy of the 8-line idiom (camera, actor mover, now
  rig). The in-tree precedent explicitly chose duplication with a
  "match exactly" comment over a shared helper (`llactormover.cpp:2999-3001`);
  keep that convention, note a future consolidation in the backlog.

**SA-5 (OffsetZ).** `centre_agent.z += clamp(offset_z, −10, 10) × s_raw` —
the trim is authored as "metres on a normal-sized subject" and must stay
proportional (an eye-level nudge on a doll is millimetres, on a giant metres).
The −10..10 clamp stays on the NOMINAL value (spinner range,
`panel_cine_light_rig.xml:154`), so the world-space trim can legitimately reach
±1500 m on a 150× giant whose chest is itself ~270 m up — guarded by the
existing finiteness checks, not by a tighter clamp.

### A.3 Where the factor applies, and mid-shot rescale behaviour

**SA-6 (per-frame, never at load).** The scale is read and applied every tick,
because clones rescale mid-shot (the Ghost Studio scale slider drags
continuously, and typed values jump). Baking at preset load is rejected: it
goes stale on rescale, breaks SA-0, and double-applies on re-save.

**SA-7 (smoothing: ride the existing damping, one knob).** New controller
state `F32 mSmoothedScale` beside `mSmoothedCentre`:
- Sampled per tick from the sanitized `s_raw`.
- Smoothed with the **same exponential formula and the same
  `CineLightRigDamping` constant** as the centre
  (`alcinelightrig.cpp:940-960`): default damping 0 ⇒ **snap** (a rigid,
  hard-mounted-rig look, matching the existing default); damping > 0 ⇒ the
  centre and the orbit shrink/grow together with one time constant. No new
  setting, no separate ease curve.
- Reset (snap to current) exactly when `mHaveSmoothedCentre` resets: first
  frame, damping 0, non-monotonic presentation time, anchor switch
  (`setAnchor`, `alcinelightrig.cpp:251-255`), subject reacquired after loss.
- **Interaction with the 0.9 s setup transition — orthogonal by
  construction:** the transition system eases NOMINAL radius
  (`mCurrentRadius`, `alcinelightrig.cpp:674-675`, and the FX-path snap at
  `:974-975`); the scale multiplies the model's geometry downstream (SA-8).
  A rescale during a running transition, or during an FX, needs no special
  case: `effective = eased_nominal × smoothed_scale` at every frame. The
  centre-smoothing interaction is likewise composition, not coupling: a
  rescale moves the true centre (chest height changes) through the existing
  centre damper while the radius follows `mSmoothedScale` with the same τ —
  transient divergence is bounded by τ and zero at the default (both snap).

**SA-8 (geometry, in the model).** `render()` reads
`s = safe_globals.mSubjectScale` and derives one number:

```
if (s == 1.f)   effective_radius = safe_radius;              # bit-identical path
else            effective_radius = clamp(safe_radius * s,
                                         min(safe_radius, SCALED_RADIUS_FLOOR),
                                         max(safe_radius, SCALED_RADIUS_CEIL));
SCALED_RADIUS_FLOOR = 0.1f
SCALED_RADIUS_CEIL  = 20.f / 2.2f          # ≈ 9.0909, derived not tuned:
                                           # the largest orbit whose 2.2× falloff
                                           # radius still fits llprimitive's 20 m
```

Everything spatial derives from `effective_radius` instead of `safe_radius`:
projector offsets (`alcinelightrigmodel.cpp:520-522`), aim normalization,
projector falloff radius `× 2.2` (`:547`), omni offsets (`:558-560`), omni
falloff radius `× 1.5` (`:571`). The `min/max`-bounded clamp form keeps three
properties provable: (i) `s = 1` is exact identity for ANY nominal radius,
including a user-typed 12 m that exceeds the ceiling (the bound is
`max(nominal, CEIL)`, so scaling up never shrinks a large nominal, and
`min(nominal, FLOOR)` never grows a tiny one); (ii) `effective_radius` is
monotone non-decreasing in `s`; (iii) inside `[FLOOR, CEIL]` the orbit is
exactly proportional, which by §1.4 keeps the subject's attenuation
bit-invariant — **the preset's look is preserved across the scale range, and
beyond the ceiling it degrades by design** (a 150× giant gets its head and
torso lit by lights orbiting 9 m out with 20 m reach — framing the face is
the correct cinematic degradation; nothing in this engine can light a 135 m
body with 20 m lights, see C9).

Floor rationale: below ~0.1 m orbit the emitters sit inside the subject's
chest volume and shadow-map near planes clip; 0.1 m on a 0.05× clone (9 cm
doll) still reads as the nominal composition slightly widened (attenuation
ratio unchanged; framing ratio 0.1/0.075 = 1.33× wide). Guess on the exact
0.1 value — flagged for the in-world pass; it is a named constant.

**SA-9 (exposure — scale must NOT enter EV).** `distance_ev` keeps reading the
NOMINAL radius: `log2(safe_radius / 1.5)` (`alcinelightrigmodel.cpp:511`
**unchanged**). Reasoned from the shader (§1.4): within the invariance regime,
co-scaling orbit and falloff radius already holds subject illumination
constant; feeding `effective_radius` into `EV_dist` would ADD
`log2(s)` stops — +7.2 stops at 150× (hard-clipped white), −4.3 at 0.05×
(near-black) — pure drift with no physical meaning in a
normalized-distance attenuation model. The seed's intuition "a closer light
on a small subject IS brighter" is inverse-square reasoning that this
renderer does not implement for local lights; the correct in-engine statement
is "a proportionally closer light with a proportionally smaller falloff
radius is EQUALLY bright." Corollaries, all testable:
- Intensity, `mClipped`, the headroom re-base, the omni bounce ratio, and the
  −10 EV FX-off threshold are all **scale-invariant** — zero interaction with
  the just-shipped exposure machinery. The headroom question the seed raised
  answers itself: no radius-derived EV term changes under scale, so no drift
  and no re-base interaction exists to compensate.
- Above the ceiling (giant clones) `EV_dist` still derives from nominal, so
  exposure stays put while coverage narrows — the operator sees a stable,
  correctly-exposed head-and-torso key, not a dimming one.
- `AlchemyGlobalLightScale`, profiles, beam falloff PARAMETERS (0.5/1.0/1.5 —
  dimensionless curve shapes, `deferredUtil.glsl:362`), and FOVs (angles are
  scale-free) are untouched.

**SA-10 (emitter box — the one subtle YES).** The 0.25 m emitter box
participates in projector optics (base design O3: `setupSpotLight` builds the
frustum from `getScale()`; near-plane distance `(scale.y×0.5)/tan(fov×0.5)`).
For the Snoot (fov 0.2) that near offset is `0.125/tan(0.1) ≈ 1.246 m`; at a
0.1 m orbit a constant box turns the Snoot's beam ~13× wider than authored
(beam-width ratio `(d_near + r)/r`), destroying the narrow-beam look on small
subjects. Rule: in `applyFrame`, box edge =
`clamp(EMITTER_SCALE × effective_radius / nominal_radius, 0.01f, 1.0f)` —
exactly `0.25` when `s = 1` (ratio 1), restoring the authored
`(d_near + r)/r` ratio across the proportional regime. Both projector and
omni boxes use it (omnis ignore optics; one code path). This is a per-object
param VALUE change on rig-owned invisible objects — no pipeline code.

**SA-11 (what explicitly does NOT scale — the closed list).** EV / intensity /
headroom / bounce ratio (SA-9); falloff parameters; FOVs; yaw/pitch angles;
profiles; transition and damping DURATIONS (time is not space); the gizmo's
cone-length display clamp (cosmetic, deferred §8); `RigFrame` non-spatial
fields.

**SA-12 (degenerate & lost-subject matrix).**

| Condition | Behaviour | Mechanism |
|---|---|---|
| scale NaN / inf / ≤ 0 | treated as 1.0 | SA-2 sanitize (tested) |
| scale outside 0.05..150 | clamped | SA-2 (tested) |
| subject lost mid-shot | emitters destroyed, state cleared — unchanged | existing `alcinelightrig.cpp:836-845` |
| subject reacquired | centre AND scale snap (no ease from stale values) | SA-7 reset coupling |
| anchor switched mid-shot | snap to new subject's scale | `setAnchor` resets `mHaveSmoothedCentre` (`:254`) |
| root joint missing, OR foot non-finite after the pelvis subtraction | unscaled point used (never NaN into the frame) | SA-4 guards, mirroring `llghostavatar.cpp:301-313` |
| pelvisToFoot alone non-finite | **p2f treated as 0, foot-pivot CONTINUES** (NOT the unscaled point) — this lands the centre exactly where `updateEntityOuterTransform` scales the clone; the unscaled fallback would mis-frame by ~0.65 m on a 0.5× clone (Opus review, 2026-08-16, verified against `llghostavatar.cpp:307-309`) | SA-4, `alcinelightrig.cpp` scaledPoint |
| `CineLightRigScaleAware` FALSE | today's behaviour, bit-exact | SA-3 |
| mid-shot rescale during FX / transition | composes, no special case | SA-7 orthogonality |

**Per-preset "don't scale" flag: NO for v1** (seed A5). The global
kill-switch covers the escape-hatch need; a per-preset flag would leak scale
policy into the nominal file format against SA-0. Revisit only on artist
demand.

---

## Section B — the master preset library

### B.1 Storage: bundled LLSD file, compiled Classic as bedrock

**Decision: `indra/newview/app_settings/cine_light_rig_presets.xml`** (LLSD
XML), loaded lazily once per session, **plus** the compiled `classicSetup()`
(`alcinelightrigmodel.cpp:1139`, `alcinelightrigmodel.h:109`) always injected
as master index 0 regardless of file state.

Reasons for the file over a C++ table:
- **Fork idiom for shipped data**: `llvograss.cpp:114`, `llvotree.cpp:114`,
  `llviewerfoldertype.cpp:161`, `fsfloaterposestand.cpp:120` all load
  domain data from `LL_PATH_APP_SETTINGS` (PROVES); packaging is free — the
  whole directory ships via the `app_settings` prefix
  (`viewer_manifest.py:80`, PROVES).
- Curation iterates without a rebuild (the in-world tuning pass for §C WILL
  adjust values), and a future viewer ships more presets by editing data.
- Read-only by location: `LL_PATH_APP_SETTINGS` is the install dir; the rig
  never writes there (saves go to the user dir, `alcinelightrig.cpp:1131`),
  and a viewer update replaces the file cleanly.

The corruption objection to data files is answered by the **fallback chain**,
not by hardcoding: file unreadable / unparseable / wrong version → masters =
{compiled Classic} + LL_WARNS, locals untouched, rig fully functional. A
malformed entry → that entry skipped + warn, rest load. **The Classic-injection
rule doubles as single-source-of-truth**: the shipped file must NOT contain a
"Classic 3-Point" entry (loader skips any case-insensitive match with a warn),
so Classic's values live exactly once, in `classicSetup()`, which is also what
the settings defaults and Reset All produce.

### B.2 On-disk schema (version 1)

```xml
<llsd><map>
  <key>version</key><integer>1</integer>
  <key>presets</key><array>
    <map>
      <key>name</key><string>Rembrandt</string>
      <key>intent</key><string>One-sentence tooltip text</string>
      <key>radius</key><real>1.4</real>
      <key>lights</key><array>  <!-- exactly 4, Key/Fill/Rim/Bg order -->
        <map> yaw, pitch, profile_idx, profile_name, ev, beam_idx, beam_name, on </map>
        ...
      </array>
    </map>
  </array>
</map></llsd>
```

The `radius`+`lights` block is **byte-compatible with the existing local
preset body**, so the loader reuses `setupFromLLSD`
(`alcinelightrig.cpp:169-218`) verbatim — inheriting its sanitize pass and
its index+name double-write with name-fallback (`:190-201`), which is the
forward-compat story for a future profile-table edit. `intent` is optional
(combo tooltip). Entry validation: `name` non-empty after trim, unique
case-insensitively among masters, `setupFromLLSD` returns true. File
validation: map, `version` integer == 1, `presets` array. Unknown version →
whole file discarded (fallback chain), `LL_WARNS("CineLightRig")` naming the
version — matching the scene loader's warn-and-preserve precedent
(`alcinelightrig.cpp:1290-1299`). **Adding entries is NOT a version bump**;
bump only for structural changes to the map shape.

### B.3 Controller changes (`alcinelightrig.{h,cpp}`)

```cpp
struct MasterSetup { std::string mName, mIntent; ALCineLightRigModel::Setup mSetup; };
struct SetupEntry  { std::string mName; bool mMaster; };

static const std::vector<MasterSetup>& masterSetups();       // lazy; index 0 = compiled Classic; never empty
static const MasterSetup* findMasterSetup(const std::string& name);  // case-insensitive
std::vector<SetupEntry> setupNamesGrouped() const;           // replaces setupNames()
static bool isMasterSetup(const std::string& name);          // convenience over findMasterSetup
```

- **`setupNamesGrouped()`**: masters in FILE order (curated order, Classic
  first — deliberately NOT sorted; the slate is sequenced §C), then locals
  sorted as today (`:1163`). Replaces `setupNames()` — single caller at
  `alpanelcinelightrig.cpp:317` (PROVES); delete the old method rather than
  deprecate (no other consumers; scene code never touches names, O-B).
- **`loadSetup(name)`** (`:1167`): resolve `findMasterSetup` FIRST (exact,
  then case-insensitive) → `stopFX(); writeSetupToSettings(master->mSetup)` —
  the generalization of the Classic special case at `:1170-1173`, which is
  deleted. Else the local-file path unchanged. Master-over-local precedence is
  belt-and-braces: B.4's migration makes collisions unrepresentable.
- **`saveSetup(name)`** (`:1198`): replace the `clean_name ==
  CLASSIC_SETUP_NAME` refusal (`:1202`) with
  `isMasterSetup(clean_name)` (case-insensitive — required because the preset
  filesystem is case-insensitive on Windows and because "film noir" shadowing
  "Film Noir" in the combo is operator-hostile). Also refuse the two combo
  caption strings (§B.5).
- **`deleteSetup(name)`** (`:1234`): same generalization of the refusal.
- **`CLASSIC_SETUP_NAME`** (`:47`) survives as the constant naming master #0;
  everywhere else the literal-comparison special cases die.

### B.4 Local-collision migration (forward-compat with future masters)

At `masterSetups()` first load: scan the local presets dir; for every local
file whose display name case-insensitively equals a master name, rename the
file to `"<name> (local)"` (uniquified `(local 2)`... on repeat), one
`LL_WARNS` per file plus one aggregate `GenericAlert` ("N of your saved rig
setups were renamed because this viewer ships built-in presets with the same
names"). Rename failure → the local is EXCLUDED from `setupNamesGrouped()`
and warned, never silently listed-but-shadowed and never deleted. This is the
policy for "a future viewer ships more master presets while the user already
has a local of that name" (seed B5): user data survives with a visible new
name; the invariant *no listed local bears a master name* holds from the first
frame of every session.

### B.5 Setup-combo UI: grouped, via the proven in-tree idiom

**What LLComboBox supports in THIS codebase (verified):**
`add(name, LLSD value, EAddPosition, bool enabled)` — a `false`-enabled item
renders as a non-selectable greyed row — and `addSeparator(EAddPosition)`
(`llcombobox.h:138-142`, PROVES). There are NO labeled group headers natively;
the fork's established approximation is
**`add_labeled_separator` = `addSeparator()` + disabled caption row**
(`bdmergeenvlibrary.cpp:59-67`, PROVES — built exactly because "Alchemy's
`LLComboBox::addSeparator()` takes no label"; further separator precedent at
`alfloaterphototools.cpp:142,158,174`). Adopt it:

```
[— Built-in —]      disabled caption, value LLSD()
Classic 3-Point     value = name
Rembrandt ...       (tool_tip = intent)
――――――――――――        addSeparator()
[— My setups —]     disabled caption, value LLSD()   (omitted when no locals)
Alley test          value = name
```

in `refreshSetupList()` (`alpanelcinelightrig.cpp:299-333`). Safety of the
decoration rows against every existing flow, checked path by path:
- **Select**: disabled rows are unselectable by construction (the env-library
  combo ships this in production).
- **Save** reads typed text `getSimple()` (`:374`); a user could type a
  caption string — the caption literals join the save-refusal set (B.3).
  Empty-value caption rows already fail the `name.empty()` guards on the
  delete (`:399`) and load (`:365-368`) paths.
- **Selection restore** is by value (`setSelectedByValue`, `:322,:332`) —
  unaffected by decoration rows.
- The pending-refresh / focus-guard machinery (`:306-313`, `:614-617`) is
  orthogonal (row-set contents only).

**Delete-button gating** (`:604-607`): from `selected != CLASSIC_SETUP_NAME`
to `!selected.empty() && !ALCineLightRig::isMasterSetup(selected)`. The
Save-refusal notification text (`:378-383`) updates to name master presets
generally. Both reuse the existing `GenericAlert` idiom — no new notification
templates.

### B.6 Scene round-trip, reset, and the fate of Classic

- **Scene round-trip: no changes** (O-B). Scenes carry the denormalized base;
  the 42 settings keys ride `sceneSettingsList()`; the three-way
  `applySceneData` split (`:1275-1324`) is a final and is untouched.
- **Reset-to-default**: Reset All keeps resetting every key to its
  settings.xml default — which IS Classic — and re-selects
  `masterSetups()[0].mName` instead of the literal at
  `alpanelcinelightrig.cpp:477`. Behaviourally identical today; correct by
  construction if master #0 ever changes.
- **Classic 3-Point becomes master #0** (recommended option taken): compiled
  values, injected, undeletable and un-overwritable like every master, listed
  under the Built-in caption. Its four special-case literal comparisons in
  controller+panel are replaced by the general master checks — a net deletion
  of special cases.

---

## Section C — the curated slate (ten masters)

Authoring ground rules, stated once: values use the existing 24 profiles /
3 beams (`alcinelightrigmodel.cpp:50-81`); yaw is world-relative (the orbit
has no facing sense — the operator aims the whole rig with Orbit Yaw /
Mirror, which compose on top of every setup by the v20 architecture); pitch
positive = above the subject; EV values assume the shipped 2-stop headroom
(EV 0 ⇒ intensity 0.25) and keep peaks ≤ +1.5 so FX headroom survives; all
four lights are fully specified even when off (schema requires it; parked
values chosen to look right if the operator flips the light on). Beams:
0 = Standard (fov 1.5, falloff 1.0), 1 = Softbox (2.8, 1.5),
2 = Snoot (0.2, 0.5). Profiles by index: 0 2700K Incan · 1 3200K Tung ·
2 4500K Neut · 3 5600K Day · 4 6500K Cool · 5 8000K Moon · 6 10000K Sky ·
7 Full CTO · 8 Full CTB · 13 Deep Amber · 15 Cyber Pink · 16 Sci-Fi Cyan ·
17 Golden Hour · 22 Neon Purple · 23 Pure White.

**Why ten:** one flagship per genuinely distinct look-family — portrait
craft (2-4), drama (5, 9), environmental/time-of-day (6, 7, 10), stylized
(8), plus the broadcast workhorse and the inherited Classic. Every candidate
beyond ten (Loop, Broad/Short, Beauty Dish, High-Key product...) is a
sub-variant of one of these reachable with two slider moves, and a combo
over ~10 stays scannable without scrolling. A tight set the operator learns
beats a catalogue they search.

Format per light: `yaw° / pitch° / profile / EV / beam / on`.

| # | Name | Intent | Key | Fill | Rim | Bg | radius |
|---|---|---|---|---|---|---|---|
| 0 | **Classic 3-Point** | The LSL boot rig; neutral daylight coverage. | 45/35/3/0.0/1/on | −45/5/3/−2.0/1/on | −135/45/4/−0.5/0/on | 0/−20/3/0.0/0/off | 1.5 |
| 1 | **Rembrandt** | Warm single-source portrait; deep short-side shadow, cheek triangle. | 40/50/1/+0.5/1/on | −50/10/1/−2.5/1/on | −140/40/4/−1.0/2/on | 0/−20/2/−2.0/0/off | 1.4 |
| 2 | **Paramount Butterfly** | Glamour front-key high above lens axis, under-nose shadow; soft under-fill. | 0/55/2/+0.5/1/on | 0/−25/2/−2.0/1/on | 180/50/3/−1.0/0/on | 90/−15/2/−2.0/0/off | 1.5 |
| 3 | **Broadcast Interview** | Flattering 2:1 news/doc coverage with a background wash. | 35/30/3/0.0/1/on | −35/15/3/−1.0/1/on | −165/40/2/−0.5/0/on | 150/−15/4/−1.5/0/on | 1.7 |
| 4 | **Film Noir** | Hard white side-key, no fill, blade-edge shadows. | 90/25/23/+1.0/2/on | −90/10/23/−4.0/1/off | −120/35/23/−1.5/2/on | 0/−20/5/−3.0/0/off | 1.8 |
| 5 | **Silhouette** | Subject as a black cutout against blasted backlight and cold sky wash. | 45/35/23/−6.0/1/off | −45/5/23/−6.0/1/off | 180/15/23/+1.5/1/on | 165/45/8/0.0/0/on | 1.8 |
| 6 | **Golden Hour** | Low warm sun from behind, amber bounce fill, cool sky counter-wash. | 170/20/17/+1.0/0/on | −10/10/13/−2.5/1/on | −60/30/17/−2.0/2/off | 60/40/8/−2.0/1/on | 2.0 |
| 7 | **Blue Hour Moon** | Cold top-heavy moonlight with one warm practical accent. | 30/60/5/0.0/1/on | −45/5/6/−2.5/1/on | −150/30/1/−0.5/2/on | 0/−20/6/−2.5/0/off | 1.6 |
| 8 | **Neon Crossfire** | Duotone club split — pink and cyan hard cross, purple hair snoot. | 90/10/15/+0.5/0/on | −90/10/16/+0.5/0/on | 180/45/22/−0.5/2/on | 0/−25/18/−2.0/0/off | 1.5 |
| 9 | **Firelight** | Low warm flicker source below the eyeline, cool night rim. Pair with the Fire Flicker FX. | 15/−30/0/+0.5/0/on | −30/−20/7/−1.5/1/on | 170/25/5/−1.5/0/on | 0/−20/13/−2.5/0/off | 1.2 |

Intent strings above are the `intent` schema field (combo tooltips).

**How each reads on a scaled clone** (all inherit SA-8's guarantee —
attenuation-invariant from 0.05× up to the radius ceiling, ≈6× at these
nominal radii; beyond that, coverage narrows to head/torso at constant
exposure):
- 0-3 (soft portrait family): scale-transparent; Softbox near-plane offset is
  ~2 cm so even doll scale is clean.
- 4 (Noir) and the snoot rims (1, 7, 8): the look **depends on SA-10** —
  without the proportional emitter box a Snoot floods a 0.05× doll into a
  wash; with it, the blade edge survives. These are the presets to eyeball at
  0.05× / 0.5× in the in-world pass.
- 5 (Silhouette): needs full-body backlight coverage, so it is the FIRST to
  degrade on giants (torso halo instead of full-figure rim above ~6×) — the
  intent tooltip carries a note; acceptable, physically forced (C9).
- 6 (Golden Hour, radius 2.0): ceiling hits at ~4.5× instead of ~6× — the
  long-throw look compresses earliest of the family; by design.
- 9 (Firelight): below-horizon pitches rely on the foot-pivot centre (SA-4)
  keeping the source proportionally below the eyeline — on the current
  unscaled-centre code a scaled clone would break this preset worst; it
  doubles as the in-world regression probe for O-A.

No preset needs a 4th mandatory light: Bg is on only in 3, 5, 6 and is an
accent, not structure (seed C's "flag any that need 4" — none do).

### Section C (expanded) — proper-cinema slate (user, 2026-08-16)

User asked to expand the slate "for proper cinema lighting." Eleven more
masters (10–20), total **21**. Authoring constraint that shapes the set:
**yaw is world-relative** (§Section C ground rules) — the operator rotates the
whole rig with Orbit Yaw / Mirror — so Short vs Broad, and left- vs right-hand
Rembrandt, are the SAME preset rotated, NOT distinct entries. The expansion
therefore spans distinct *patterns* (key elevation, source count, key:fill
contrast, beam hardness, colour temperature, rim role), never handedness.
Same format: `yaw° / pitch° / profile / EV / beam / on`. Same authoring rules
(2-stop headroom ⇒ EV 0 = 0.25 intensity; peaks ≤ +1.5; all four lights fully
specified; parked values plausible if flipped on).

| # | Name | Intent | Key | Fill | Rim | Bg | radius |
|---|---|---|---|---|---|---|---|
| 10 | **Loop** | Portrait workhorse; small nose-loop shadow, one notch fuller than Rembrandt. | 30/45/2/+0.5/1/on | −35/12/2/−1.8/1/on | −140/40/4/−1.2/2/on | 0/−20/2/−2.2/0/off | 1.4 |
| 11 | **Split** | Hard side key at 90°, level; face divided light/dark, drama. | 90/12/2/+0.5/0/on | −90/8/2/−3.0/1/on | 180/30/4/−1.0/2/on | 0/−20/5/−2.5/0/off | 1.6 |
| 12 | **Clamshell Beauty** | Front key high + front under-fill; glamour, near-even, catchlight top&bottom. | 0/50/2/+0.5/1/on | 0/−30/2/−1.0/1/on | 180/45/3/−1.0/1/on | 90/−15/2/−2.0/0/off | 1.5 |
| 13 | **High Key** | Bright, near-shadowless, key≈fill, background lit; fashion / comedy. | 35/30/3/+0.5/1/on | −35/20/3/0.0/1/on | −150/40/3/−0.5/1/on | 150/−10/3/−0.5/1/on | 1.8 |
| 14 | **Low Key Drama** | Single hard warm source, deep shadow, no fill, cool whisper rim. | 40/25/1/+1.0/2/on | −40/10/1/−2.5/1/off | −130/35/4/−1.5/2/on | 0/−20/5/−3.0/0/off | 1.6 |
| 15 | **Uplight Horror** | Cold source below the eyeline; ominous, unnatural. | 15/−35/6/+0.5/2/on | −20/−15/6/−2.5/1/off | 175/25/6/−1.5/1/on | 0/20/8/−2.5/0/off | 1.3 |
| 16 | **Top Light** | Hard overhead; eye-socket shadow, interrogation / menace. | 0/80/2/+0.5/2/on | 0/−20/2/−2.5/1/off | 180/40/4/−1.5/2/on | 0/−25/5/−2.5/0/off | 1.5 |
| 17 | **Motivated Window** | Soft directional daylight one high side, cool ambient fill + wash. | 60/40/3/+0.5/1/on | −50/10/4/−2.0/1/on | −150/35/4/−1.5/1/off | 120/−10/4/−2.0/1/on | 1.9 |
| 18 | **Overcast Soft** | Very soft even wrap, low contrast, neutral-cool, near-shadowless. | 30/35/4/0.0/1/on | −30/25/4/−0.8/1/on | 180/40/4/−1.2/1/on | 150/0/4/−1.5/1/on | 2.0 |
| 19 | **Teal & Orange** | Warm key, cool cyan fill + rim; the blockbuster grade. | 40/30/17/+0.5/1/on | −45/10/16/−2.0/1/on | −135/40/16/−1.0/2/on | 0/−20/8/−2.0/0/off | 1.6 |
| 20 | **Sci-Fi Cool** | Cold hard key, cyan rims; clinical / tech. | 45/25/4/+0.5/0/on | −45/10/8/−2.5/1/on | 180/45/16/−0.5/2/on | 0/30/8/−2.0/0/off | 1.6 |

**Count is now 21** (deliberate override of the earlier "ten is scannable"
rationale — the user wants a proper library). The set spans the textbook space
without redundancy: portrait patterns (Rembrandt 1, Loop 10, Butterfly 2,
Split 11, Clamshell 12), contrast/drama (Noir 4, Low Key 14, High Key 13,
Top 16, Uplight 15), three-point/broadcast (Classic 0, Broadcast 3,
Overcast 18), motivated / time-of-day (Golden 6, Blue Hour 7, Window 17,
Firelight 9), stylized (Silhouette 5, Neon 8, Teal&Orange 19, Sci-Fi 20).
Because the master group is scrollable and separated (B.5), 21 is fine; if the
combo feels long in-world, sub-grouping the master block is a later UI tweak,
not a content change.

**Scaled-clone behaviour, expanded set:** all inherit the SA guarantees.
Hard-beam looks that DEPEND on SA-10 emitter-box scaling to survive at doll
scale: Split (11), Low Key (14), Uplight (15), Top (16), and Sci-Fi key (20) —
add these to the in-world 0.05×/0.5× eyeball list beside Film Noir. Wrap-soft
looks that are scale-transparent: Clamshell (12), High Key (13), Motivated
Window (17), Overcast (18). Uplight (15) also stresses the foot-pivot centre
(SA-4) via its below-eyeline pitch — a second probe for O-A alongside
Firelight.

All profile indices used are in-range [0,23] and all beam indices in [0,2]
(verify by counting at implementation — CLAUDE.md rule 6). These values are a
STARTING slate for in-world curation; the operator tunes and the adversarial
review sanity-checks the lighting logic, index ranges, and on/off parity.

---

## Section D — panel reset controls + Director tab icon

### D.1 Per-control mini reset buttons

**Precedent verified:** the weather panel ships exactly this — an 18×18
`<button image_overlay="Refresh_Off" ... commit_callback.function="Weather.ResetControl"
commit_callback.parameter="<SettingsKey>">` per row in a right-edge column at
`left="322" width="18"` inside a 350-wide panel
(`panel_weather_settings.xml:46-61` and every subsequent group, PROVES), all
served by ONE registered callback whose body is
`gSavedSettings.getControl(param)->resetToDefault(true)`
(`registerWeatherResetControl`, `alpanelweathersettings.cpp:35-53`, registered
from the panel constructor before XUI child construction resolves callbacks,
`:56-61`, PROVES).

**Mirror for the rig:**
- Register `CineLightRig.ResetControl` in the `ALPanelCineLightRig`
  constructor via a file-local `registerCineLightRigResetControl()` — copy the
  weather function including its static-`registered` once-guard; identical
  body. `resetToDefault(true)` fires the control's commit, so settings-bound
  widgets in BOTH hosts (Director tab + floater) sync through
  `gSavedSettings` for free, and a Radius reset glides through the existing
  0.9 s transition like any radius commit.
- The rig panel is also 350 wide (`panel_cine_light_rig.xml:8`), so the
  weather column geometry transfers verbatim: **reset column at left=322,
  rightmost control edge moves 340 → 318**.

**Which controls get one — the rule and the roster.** Rule: every
`control_name`-bound VALUE control (slider / spinner / combo / line editor)
gets a reset; booleans and transport/session controls do not. Roster —
**29 buttons**:
- Per light × 4 (Key/Fill/Rim/Bg): Yaw, Pitch, Profile, EV, Beam → 20.
- Globals: `CineLightRigRadius`, `CineLightRigMasterEV`,
  `CineLightRigOffsetZ`, `CineLightRigHeadroomStops`,
  `CineLightRigBounceRatio`, `CineLightRigTransitionSec`,
  `CineLightRigDamping`, `CineLightRigShadowMode`, `CineLightRigCookieUUID`
  → 9. (The cookie line-edit is the poster case — recovering the shipped
  UUID otherwise requires reading settings.xml.)

Excluded, with reasons (the judgement calls the seed asked to be stated):
- **All checkboxes** (Enable, Power, Gizmo, Mirror, BounceEnabled, 4× On):
  one click already restores either state; 9 more buttons is pure noise.
- **FX combo**: "None" IS its reset, and FX is transport, not a tweak.
- **Seed editor**: the seed is a take's identity (determinism story), not a
  tunable — a stray reset silently changes what a re-render reproduces; the
  existing Randomize button covers "give me a fresh one". No reset.
- **Orbit aim ±15° buttons / transform state**: already covered by the
  existing **Reset aim** which zeroes all three transform keys atomically
  (`alpanelcinelightrig.cpp:295` area) — a per-key reset would be a worse
  duplicate.
- **Shaft/Hero checkboxes, Anchor combo**: session state, not settings-backed
  (`control_name`-less), nothing to reset to.

**Layout (one coherent revision with §B.5's grouped combo).** The panel
gains no width; rows re-flow under two patterns:
- *Full-width rows* (Yaw/Pitch sliders, currently `left=52 width=288` →
  `width=266`; cookie editor; damping slider): reset in the column at 322.
- *Paired rows* keep two controls and give the LEFT control an inline reset
  (weather has no paired rows, so this is the one local extension of the
  idiom; the column still catches the rightmost control). Worked map, ±6 px
  implementer freedom, invariants being "every roster control has an adjacent
  reset" and "the right column aligns at 322":
  - Colour/EV row: label 8 → profile combo 52..200 (w148) → inline reset
    204 → "EV" 226 → EV spinner 246..318 (w72) → column reset 322.
  - Beam row: beam combo 52..200 (w148) → inline reset 204 → Shaft 232 →
    Hero 292 (checkboxes, no reset).
  - Globals paired spinner rows (Radius|MasterEV, OffsetZ|Headroom,
    Bounce ratio|Ease, ShadowMode row): left spinner narrows to w74 + inline
    reset; right spinner ends 318 + column reset.
  - Setups row is unchanged (combo + Save/Delete; §B.5 changes its CONTENTS
    only). Panel height stays 1130; the scroll containers in both hosts
    absorb any small overflow.
- Every button: `height="18" width="18" image_overlay="Refresh_Off"
  image_top_pad="0" tool_tip="Reset to default"
  commit_callback.function="CineLightRig.ResetControl"
  commit_callback.parameter="<exact settings key>"` — the weather attribute
  set verbatim.

**Reset All stays** (`alpanelcinelightrig.cpp:455-479`) — it additionally
clears anchor/shaft/hero session state, which per-control resets never touch.

### D.2 Director Console icon for the Lights tab

**How the other tabs get icons (verified):** a file-local constant per tab
(`llfloaterdirector.cpp:70-80`, all from the existing `Command_*_Icon`
toolbar-icon family) feeds a `{ panel-name, icon }` table
(`llfloaterdirector.cpp:231-242`) applied via
`mTabContainer->setTabImage(panel, icon)` (`:247`), which composes the icon
as an image overlay to the LEFT of the tab label (comment `:230`). The rig's
tab already exists — `name="cine_light_rig_tab"`, `label="Lights"`
(`floater_director.xml:1506-1514`) — and is **absent from the table**, so it
currently renders label-only. PROVES.

**Specification:**
- Constant: `constexpr char TAB_ICON_LIGHTS[] = "Command_Lightbox_Icon";`
  beside the others at `llfloaterdirector.cpp:80`.
- Table row: `{ "cine_light_rig_tab", TAB_ICON_LIGHTS },` — name-keyed like
  every row, so tab reordering can't break it.
- **Asset: `Command_Lightbox_Icon`** — exists and preloads
  (`textures.xml:153`, `toolbar_icons/lightbox.png`); a lightbox is
  photographic-lighting iconography, on-theme for a light rig. Reusing a
  toolbar command's icon for a Director tab is the established pattern —
  Shafts already borrows `Command_PersonalLighting_Icon` (`:78`, asset
  `textures.xml:169`), Weather borrows Water, Time borrows Environments
  (`:79-80` with the same "no dedicated asset ships" style of comment).
  Within the rail it is unique (no tab uses it; its only other consumer is
  the Lightbox floater toolbar command, `commands.xml:282` — different UI
  surface, no confusion). Rejected: `Command_PersonalLighting_Icon` (taken by
  Shafts, adjacent in the rail), `Command_Windlight_Icon` (environment/sky
  semantics collide with Weather/Time), a new bundled PNG (asset churn with
  an on-theme asset already shipping).
- Label composition: nothing further — `setTabImage` + existing `label="Lights"`.

### D.3 Section-D verification / tests

- XUI integrity: viewer boots with zero XUI warnings for
  `panel_cine_light_rig.xml` (parse errors log loudly); both hosts render the
  re-flowed panel inside their scroll containers.
- **Parameter audit (review checklist, mechanical):** every
  `CineLightRig.ResetControl` `commit_callback.parameter` in the XML greps to
  an existing `settings.xml` key, and the roster count is exactly 29 — this
  catches the classic typo'd-key silent no-op.
- Behavioural: reset on Radius mid-transition eases (not snaps) — rides the
  normal commit path; reset on a per-light EV clears a CLIP indicator when it
  should; a reset in the floater updates the Director tab instance same-frame
  (settings binding, V7 of the base design).
- Icon: Lights tab shows icon + label; the other ten tabs unchanged (the
  table is additive); no icon regression when the tab is the persisted
  startup tab.

---

## 5. Model TUT test additions (`alcinelightrigmodel_test.cpp`)

The file (688 lines) already pins the base maths; scale adds one test cluster.
Every test states its discriminating failure. `Globals::mSubjectScale`
defaults to 1.f, so **the entire existing suite doubles as the s = 1
regression pin and must pass UNTOUCHED** — that is itself the first assertion.

1. **s = 1 bitwise no-op**: for a grid of setups (classic, each beam, radii
   {0.5, 1.5, 9.1, 12, 512}), `render` output with `mSubjectScale = 1` is
   `memcmp`-identical to the pre-change golden (regression: any accidental
   reordering of the radius math).
2. **Proportionality in-band**: at s ∈ {0.5, 2, 4} with nominal 1.5:
   `mOffX/Y/Z`, `mLightRadius` (proj ×2.2, omni ×1.5) all exactly ×s
   (powers of two make F32 equality exact; fails if any spatial term still
   reads nominal).
3. **Exposure invariance**: `mIntensity` and `mClipped` (proj and omni)
   bitwise identical across s ∈ {0.05, 0.5, 1, 2, 6, 150} for EV grids
   including clip-edge cases (fails on the `log2(s)` drift bug — the exact
   regression SA-9 forbids).
4. **Attenuation-ratio pin**: `|offset| / mLightRadius` equals the s = 1
   value (cross-multiplied F32 compare) at every s INCLUDING the clamped
   s = 150 — proves the ceiling preserves the look, not just bounds it.
5. **Ceiling**: nominal 1.5, s = 150 → effective radius ==
   `SCALED_RADIUS_CEIL` exactly; `mLightRadius == SCALED_RADIUS_CEIL * 2.2f`
   ≤ 20.
6. **Floor**: nominal 1.5, s = 0.05 → effective == `SCALED_RADIUS_FLOOR`.
7. **Out-of-band nominal continuity**: nominal 12 → s = 1 gives 12; s = 2
   gives 12 (bound is `max(nominal, CEIL)`); s = 0.5 gives 6. Nominal 0.5
   (== MIN_RADIUS) → s = 0.05 gives `min(0.5, 0.1) = 0.1`... assert the
   formula's stated `min/max` bounds verbatim.
8. **Degenerate scale**: s ∈ {0, −1, NaN, +inf, FLT_MIN} → output bitwise
   equal to s = 1 (sanitize collapses all of them; fails if any degenerate
   value leaks into a clamp).
9. **Monotonicity**: effective radius non-decreasing over an s grid
   (0.05 → 150, ~40 points) for nominals {0.5, 1.5, 12}.
10. **sanitizeGlobals pins**: `mSubjectScale` clamp bounds are exactly 0.05
    and 150 (the `GHOST_SCALE_*` numeric pairing — fails if one side is
    edited without the other), default 1.
11. **Emitter-box ratio** (if the ratio is exported via the model — see brief
    note): `s = 1 → 1.0` exactly; clamps at 0.01 / 1.0.

The master-library loader is deliberately NOT TUT-tested: it is viewer-side
(LLSD + LLDirUtil), the pure model keeps its `stdtypes.h`-only discipline,
and its parse core (`setupFromLLSD`) is already exercised through the model's
sanitize tests. Library robustness is covered by the fallback-chain review
items (§7 R4) and the in-world checklist.

---

## 6. File/function checklist (the Codex-brief skeleton)

| File | Change |
|---|---|
| `indra/newview/alcinelightrigmodel.h` | `Globals::mSubjectScale` (F32, =1.f); constants `SUBJECT_SCALE_MIN/MAX`, `SCALED_RADIUS_FLOOR/CEIL`; `render()` doc-comment gains the nominal-EV rule |
| `indra/newview/alcinelightrigmodel.cpp` | `sanitizeGlobals` scale guards (SA-2); `render()` effective-radius derivation (SA-8) with `distance_ev` untouched (SA-9) |
| `indra/newview/alcinelightrig.h` | `mSmoothedScale`; `MasterSetup`/`SetupEntry`; `masterSetups()/findMasterSetup()/isMasterSetup()/setupNamesGrouped()` (drop `setupNames()`) |
| `indra/newview/alcinelightrig.cpp` | `scaledPoint` helper + centre/fallback/OffsetZ (SA-4/5); scale sample+smooth in `tick` (SA-7); emitter box in `applyFrame` (SA-10); master loader + Classic injection + migration (B.1-B.4); generalized load/save/delete refusals (B.3) |
| `indra/newview/alpanelcinelightrig.{h,cpp}` | grouped combo via labeled separators (B.5); Delete gating + Save refusal on masters; `registerCineLightRigResetControl()` (D.1); Reset All selects `masterSetups()[0]` |
| `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml` | reset column + inline resets, 29 buttons (D.1 map); no width change |
| `indra/newview/llfloaterdirector.cpp` | `TAB_ICON_LIGHTS` + `{ "cine_light_rig_tab", ... }` row (D.2) |
| `indra/newview/app_settings/cine_light_rig_presets.xml` | NEW — §B.2 schema, §C values (9 entries; Classic is compiled) |
| `indra/newview/app_settings/settings.xml` | ONE new key: `CineLightRigScaleAware` (BOOL, TRUE) |
| `indra/newview/tests/alcinelightrigmodel_test.cpp` | §5 cluster |

Not touched: scene schema (v1), `sceneSettingsList` (gains only the one new
key), notifications.xml (GenericAlert reuse), CMake (no new sources).

## 7. Risks, ranked

**Render-state call-out first, per the brief:** this delivery contains **zero
edits to pipeline.cpp, shaders, or any shared render state**. The nine gate
exemptions, the beauty pass, `setupSpotLight`, and the shadow auction are
untouched; every render-facing effect is a per-object parameter VALUE
(position / scale / radius) on rig-owned invisible objects — values any prim
in the world may legally carry. The two items below marked ⚠ are the only
ones even ADJACENT to render behaviour, and both are value-level.

- **R1 ⚠ Emitter-box scaling (SA-10) changes projector near-plane optics** —
  through existing `setupSpotLight` inputs only, but it alters the shipped
  Snoot/Softbox beam geometry when s ≠ 1 and the box ratio when the user's
  nominal radius changes... it does NOT: the ratio is `effective/nominal`,
  which is 1 whenever s = 1 regardless of nominal. Residual risk is confined
  to scaled subjects. Verify in-world at s ∈ {0.05, 0.5, 1, 6, 150} with
  preset #4 (Film Noir). Rollback: the kill-switch restores 0.25 everywhere.
- **R2 ⚠ Centre foot-pivot (SA-4) changes framing on scaled clones** vs the
  just-built (untested-in-world) binary. It fixes a proven latent bug (O-A),
  but it is a behaviour change layered on unvalidated code — test the
  UNSCALED path first in-world (must be pixel-identical; s = 1 fast path),
  then scaled. Preset #9 (Firelight) is the sharpest probe.
- **R3 Giant-scale coverage collapse** — physics of the 20 m clamp (C9), not
  a defect; mitigated by the ceiling clamp preserving exposure/look for the
  covered region and by extending the existing radius tooltip
  (`panel_cine_light_rig.xml:150`) to mention subject scale.
- **R4 Master-file failure modes** — fallback chain (B.1) makes the worst
  case "Classic only + warning". Review must include: file absent, truncated
  XML, wrong version, non-map entry, duplicate names, a "Classic 3-Point"
  entry in the file.
- **R5 Migration renames user files** (B.4) — one-time, logged, notified;
  failure hides rather than deletes. Still the only place this feature
  touches user data — review the rename loop for the empty-dir and
  read-only-file cases.
- **R6 Combo decoration rows vs text entry** (B.5) — caption strings join the
  save-refusal set; adversarial review should type both captions and a
  master name with different case into the combo and hit Save.
- **R7 Mid-shot rescale during FX / transition** — composition argument
  (SA-7); in-world: run Club Strobe on a clone while dragging the scale
  slider 0.05→6; expect stable exposure, geometry tracking, no transition
  restarts.
- **R8 Smoothed-scale vs smoothed-centre transient** at damping > 0 —
  bounded-by-τ cosmetic divergence, zero at default. Documented, untested.
- **R9 Panel re-flow regressions** — XUI-only; the D.3 checklist (29-button
  audit, both hosts) contains it.

## 8. Deferred (decided now, with reasons)

1. **Per-preset "don't scale" flag** — violates SA-0's clean nominal format;
   kill-switch covers the need (seed A5 agreed "probably not").
2. **Gizmo cone-length scale adaptation** — display clamp 0.35..1.5 m
   (`alcinelightrig.cpp:1067-1068`) reads oversized on dolls; cosmetic,
   display-only path, zero shot impact.
3. **Localized master names/intents** — the LLSD carries English; the
   strings.xml route exists when localization of the fork's machinima UI
   happens at all (none of the Director surface is localized today).
4. **Preset thumbnails / visual browser** — pure UI sugar over a working
   combo; revisit after the slate survives in-world curation.
5. **Real-avatar uniform scale itself** — Phase 1 is its own brief
   (`AVATAR_LOCAL_SCALE_PHASE1_BRIEF.md`); the rig is forward-compatible
   through the `getUniformScale()` contract (§1.3.4). One line should be
   added to that brief when it runs: "keep `getUniformScale()` as the single
   accessor; the light rig consumes it."
6. **Shared `scaledJointPoint` avatar helper** — three call sites now
   duplicate the foot-pivot idiom by explicit in-tree convention
   (`llactormover.cpp:2999`); consolidation is a refactor for the backlog,
   not this delivery.
7. Carried from the base feature, unchanged: per-light gobos, N>4 lights,
   hotkeys, renderer >1.0 headroom, gizmo drag-editing, projvol/shadow
   decoupling.

## 9. OFF-LIMITS for the implementation brief

Everything not named in §6 is off-limits. Explicitly, even where adjacent:
**`pipeline.cpp` in its entirety** (this delivery needs no gate work; the
nine exemptions are a final), all GLSL, `indra/llprimitive/*` (the 20 m /
[0,1] clamps are design inputs — `SCALED_RADIUS_CEIL` DERIVES from them,
never "fixes" them), `llvoavatar.*` / `llghostavatar.*` /
`llclientoutertransform.h` / `llactormover.cpp` / `llcinematiccamera.cpp`
(read-only precedents), `lldirectorcast.*`, `llcombobox.*` (existing API
only; no BD-style labeled-separator port), `alweathermodel.*` (+test),
`alghoststudio.*` (the scale-range constants are mirrored by comment, not
included), `bdmerge_should_render_*` bodies, `llselectmgr.*`, the
`applySceneData` three-way split (`alcinelightrig.cpp:1275-1324` semantics
frozen), and the scene/preset version numbers (both stay 1 — nothing in this
delivery changes either format's shape; masters are a new, separate file).
