# Research Brief: Anchoring Rig‑Fed Lens Flares to the Actual On‑Screen Light

**Audience:** a research/implementation agent (Codex) with full read access to the
repo `I:\alchemy-machinima` (Alchemy‑Machinima Second Life viewer fork, working
branch has drifted to `feature/vram-management`; the flare work is uncommitted on
top of commit `6468f6b6772`). You may read anything and run experiments; the human
will build and test. Companion design doc: `docs/projector_lensflare_design.md`.

---

## 1. Objective (what "correct" looks like)

A **rig‑fed cinematic lens flare + starburst ("star")**. For each ACTIVE cinematic
light‑rig projector, draw a lens flare / diffraction star that:

- **a.** appears **on the actual bright light as seen in the frame** (where the eye
  perceives the light source / its glow), not on an arbitrary point;
- **b.** **scales with that light's real rendered brightness** (a bright key light
  flares hard; a dim one barely);
- **c.** does **NOT** appear on merely **lit surfaces** — a white floor or wall lit
  by the rig is *not* a light and must never flare;
- **d.** does **NOT** appear for **dark / dimmed / occluded / off / off‑screen**
  lights;
- **e.** is **per‑light** and **reads the actual rendered scene**, rather than
  assuming purely from the rig's configured intensity.

The existing **sun/moon flare must keep working**, and with the feature disabled
(`RenderCineLensFlareStrength == 0 && RenderLensFlareStrength == 0`) per‑frame
output must be **byte‑identical** to before the feature existed.

---

## 2. Current architecture (as built)

**CPU driver** — `LLPipeline::colorCorrect()` in `indra/newview/pipeline.cpp`
(~lines 11300–11640):
- Enumerates active rig projectors: `ALCineLightRigManager::instance()` slots
  (`enabledMask()` / `isSlotLit(slot)` / `at(slot)`), then per slot per light
  `i in 0..LIGHT_COUNT(=4)`: `LLUUID id = rig.projectorId(i)` (null ⇒ skip),
  `const RigFrame& f = rig.lastFrame()` → `f.mProj[i]` (`EmitterState`: `mOn`,
  `mIntensity`, `mSR/mSG/mSB`), resolve prim via `gObjectList.findObject(id)` →
  `LLVOVolume`, world position `getPositionAgent()`, base gel color
  `getLightLinearBaseColor()`.
- Projects each world pos → screen UV: `get_current_projection() *
  get_current_modelview() * vec4(pos_agent, 1.0)`, reject `w<=0`, `/w`, `*0.5+0.5`
  (same matrix path the sun uses). Edge‑fades near frame borders. Temporally
  smooths visibility per **stable (slot,light)** index in
  `mCineFlareVisibility[SLOT_COUNT*LIGHT_COUNT]` (pipeline.h).
- Packs (cap `AL_CINE_FLARE_MAX = 8`): `uCineFlareA[k] = (uv.x, uv.y, visibility,
  1.0)`, `uCineFlareColor[k] = (baseGelColor.rgb, RenderCineLensFlareIntensity)`,
  and `int uCineFlareCount`. Uploads via `uniform4fv` (uniform enums
  `CINE_FLARE_COUNT/A/COLOR/THRESHOLD` registered in `indra/llrender/llshadermgr.h`
  + `.cpp`, in matching order). Also uploads `RenderCineLensFlareThreshold` →
  `uCineFlareThreshold`. All of this is inside `if (cine_strength > 0.f)`.

**Shader** — `indra/newview/app_settings/shaders/class1/alchemy/postEffectUtilsF.glsl`:
- `computeLensFlare(diffuse, depth, uv)` (called from `colorCorrectF.glsl:112`, in
  LINEAR HDR, full‑res, pre‑tonemap) loops over sources: source 0 = sun, then the
  `uCineFlareCount` rig sources, each via
  `flareForSource(diffuse, depth, uv, srcUV, vis, srcColor, occTaps)`.
- Sun uses `occTaps >= 0` (its own overbright pixel gate, `max(luma - 2.0, 0)` on
  the `diffuse` buffer — this is the calibrated diffuse‑white floor for this
  buffer). Rig uses `occTaps < 0`.
- The full optical stack (glow, anamorphic streak, ghosts, halo, starburst, n‑gon
  iris, dispersion rings, spectral circles, arc, lens warp) is drawn at `srcUV`
  and multiplied by `vis` and the final `srcColor * 0.15`. Each element is gated by
  its own intensity uniform (default 0 ⇒ byte‑no‑op).

---

## 3. Current status

**"Now it doesn't work" — nothing flares on the rig projectors.** (The sun flare
still works.)

---

## 4. History of attempts (do NOT re‑propose these verbatim — each failed)

1. **Sun‑style overbright pixel gate at the fixture UV.** Rig sampled `diffuse` at
   the fixture's screen UV, `max(luma - 2.0, 0)`. → Projectors whose fixture pixel
   wasn't HDR‑bright didn't flare (most of them).
2. **Unit‑white energy × CPU intensity (bypass the screen).** Rig energy = white,
   brightness from `mIntensity`. → **All** projectors flared equally regardless of
   what's actually in frame. User rejected: *"it's not reading the screen, it's
   making assumptions."*
3. **5‑tap max‑luma probe at fixture UV, `smoothstep(threshold, +knee, raw_luma)`.**
   → Flared on a **brightly‑lit white floor** — the probe can't distinguish a lit
   surface from a light. User rejected: *"the ground is NOT a light."*
4. **Overbright probe at fixture UV, `max(luma - 2.0, 0)`, threshold 0.25.**
   → **Nothing flares.** (Current state.)

---

## 5. Root‑cause diagnosis (the crux to solve)

The flare is **anchored at the projector's fixture world position** (`getPositionAgent()`
→ screen UV). But **an SL light prim does not render a bright pixel at its own
location** — a point/spot light emits *illumination onto other surfaces* and (for
the cine rig) a *volumetric shaft*; the HDR‑bright pixels are on those lit surfaces
and in the shaft, which are **spatially offset from the fixture's screen point**.

Therefore:
- Requiring genuine brightness **at the fixture UV** (attempts 1, 4) ⇒ reads ~0 ⇒
  no flare.
- Relaxing the brightness test (attempt 3) ⇒ flares on whatever lit surface the
  fixture happens to project onto (the floor).
- Ignoring the screen entirely (attempt 2) ⇒ flares everything, "assumptions."

**The anchor (fixture position) and the brightness signal (screen pixels) do not
line up.** This is structural, not a tunable threshold.

---

## 6. The core research question

> Where, in screen space, is a rig projector *actually visibly bright*, and how do
> we anchor a per‑light flare there and scale it by that real brightness — while
> rejecting merely‑lit surfaces and off/occluded lights?

Sub‑questions to investigate against the actual code/rendering:

1. **Do the cine rig projectors produce an HDR‑bright, detectable feature on
   screen at all?** Investigate the volumetric bloom feed
   `indra/newview/app_settings/shaders/class1/deferred/projectorVolumetricBloomFeedF.glsl`
   and how the rig emitters are created/updated in `indra/newview/alcinelightrig.cpp`
   (`createEmitter` ~1354, `applyFrame` ~1755, light texture/cookie/bloom/beam
   setup). Is there a bright bloom, and **where** is it on screen relative to
   `getPositionAgent()`? Is the shaft bright and where does its base project?
2. **Is there a better world anchor than the fixture prim?** e.g. the beam origin,
   a point sampled along the beam direction (the projector has yaw/pitch), the
   catchlight, or the brightest point of the volumetric shaft. Which projects onto
   the visible glow?
3. **Neighborhood search:** would scanning a disc around the fixture UV for the
   peak overbright reliably find the light's glow without re‑introducing the
   lit‑surface false‑positive? What radius? Does the glow ever fall outside a
   reasonable radius?
4. **Buffer/UV sanity:** confirm the rig samples the *same* bound HDR buffer and UV
   space as the sun (the sun's `max(luma-2.0,0)` works, so 2.0 is a valid floor
   there). Is there any UV/scale mismatch specific to the rig path?
5. **Make the source visibly bright:** could the rig give each emitter a small,
   controlled HDR emissive/bloom sprite **at the fixture** (without disturbing
   normal scene lighting) so a screen probe reliably detects it and the flare
   scales with that? Scope/impact of that change?
6. **Depth occlusion:** the `diffuse` + `depthMap`/`DEFERRED_DEPTH` are bound at the
   flare call site. Can depth confirm the fixture point is un‑occluded (so blocked
   lights don't flare)?
7. **Hybrid legitimacy:** is a hybrid acceptable — rig data decides *which* lights
   are eligible (on + on‑screen + un‑occluded via depth), while the *screen* drives
   *strength* (overbright sampled where the light is actually bright)? Does that
   satisfy objective (e) better than either extreme?

---

## 7. Candidate directions (starting points, not a menu — combine/improve)

- **A — Make the fixture bloom, then screen‑read it.** Give each projector a small
  HDR emissive point at its position so it's genuinely bright on screen; the
  existing overbright probe then detects it and scales with real brightness.
  Cinematically correct (flare emanates from a visible bright source). Touches
  emitter setup (`alcinelightrig.cpp`) + shader.
- **B — Beam/shaft anchor.** Anchor at (or sample along) the projector's beam so the
  probe hits the bright volumetric shaft rather than the invisible fixture point.
- **C — Rig‑authored strength + screen occlusion.** Existence gated by
  on‑screen + depth‑un‑occluded + `mIntensity > 0`; strength from the fixture's own
  intensity/EV. Reliable, always positioned right, but strength is rig‑authored
  (weakest on objective e).
- **D — Neighborhood peak‑overbright search** around the fixture UV.

Weigh each against ALL of objectives a–e and report a concrete recommendation with
reasoning grounded in what the code actually renders.

---

## 8. Hard constraints

- Sun/moon flare path (`occTaps >= 0`) must stay **byte‑identical**.
- Feature‑off (`RenderCineLensFlareStrength == 0 && RenderLensFlareStrength == 0`)
  must be **byte‑identical** to pre‑feature (the shader early‑outs on
  `uCineFlareCount == 0`; the CPU uploads count 0 and touches nothing else).
- Uniform enums in `llshadermgr.h` and `llshadermgr.cpp` must be added in the
  **same order** in both files.
- **Avoid editing** `indra/newview/alpanelcinelightrig.cpp` and
  `skins/default/xui/en/panel_cine_light_rig.xml` if possible — a concurrent effort
  edits those (an unrelated gobo‑dropdown fix). Prefer `settings_alchemy.xml` +
  the pipeline upload for any new tunable.
- Don't break the existing multi‑source packing (`<= 8` cap) or the stable‑index
  visibility smoothing.

---

## 9. Deliverable expected

1. A **root‑cause confirmation** grounded in the actual rendering (what *is* bright
   at/near a rig projector on screen, verified — not assumed).
2. A **recommended approach** meeting objectives a–e, with the tradeoffs of the
   alternatives.
3. An **implementation** in the shader and/or pipeline (+ any uniform/setting
   wiring), respecting the constraints above.
4. How the user should **tune** it (settings + sensible defaults) and how to verify
   in‑world.

Do not build or commit; the human will build, deploy, and test.
