# ReShade Step C — re-patch plan at the iMMERSE **V3** level

> ⚠️ **PARTLY SUPERSEDED 2026-07-25 by `doc/RESHADE_BRIDGE_CONTRACT.md` v2.** The contract is now the
> authoritative spec and carries the reconciled facts (camera fallback is BUILT, TAAU live value is
> `.77` not `.66`, the `0.5` motion factor is exact, `Force10BitFormat=1` in the live config,
> COEXIST/OWNED modes, explicit validity). This plan's phase *sequencing* remains useful; where it
> disagrees with the contract on a FACT, the contract wins. The phased route in contract §14
> (C0…C5) supersedes the Phase 0-5 numbering below.

**Written 2026-07-25.** Target is the LIVE stack: iMMERSE **V3** + standalone `SL_GBufferProvider.fx`.
V4 stays abandoned (GL-incompatible — see [[alchemy-v4-bridge-port]]); nothing here revisits it.

Source: `DEEP_RESEARCH_DELIVERABLES.zip` → `RESHADE_STEP_C_GBUFFER_LAUNCHPAD_RETIREMENT.md` (1274 lines),
reconciled against the actual tree on 2026-07-25. **The research analysed a zip snapshot and is stale on
three points** — those corrections are recorded below so we do not re-fix solved problems.

---

## 0. Verified state (checked in source 2026-07-25, not assumed)

| Claim | Verdict | Evidence |
|---|---|---|
| Viewer normals are **octahedral**, not spheremap | **TRUE — research correct, our old note was wrong** | `globalF.glsl:46-74`, Knarkowicz `OctWrap`/`encodeNormal`/`decodeNormal` |
| `SL_DecodeNormal` used the spheremap formula | **WAS TRUE — FIXED TODAY** | `SL_Bridge.fxh:69`; patched to the octahedral port, deployed to repo + both live copies (backups `SL_Bridge.fxh.pre-octafix.bak`) |
| `SL_BridgeDebug.fx` carries its own stale spheremap decoder | **FALSE — research stale** | it calls `SL_NormalView()` from the shared header (`SL_BridgeDebug.fx:31`); the single header fix covers it |
| Albedo blocked by unmapped `GL_SRGB8_ALPHA8` (`0x8C43`) | **FALSE — already fixed** | mapped at `sl_reshade_bridge.cpp:87` → `r8g8b8a8_unorm_srgb`, landed `c3f03ded2fb` 2026-07-15 |
| Native albedo is not valid at every visible pixel | **TRUE and architectural** | alpha/water/fullbright/HUD never write G-buffer albedo |
| Deferred target is cleared to magenta | **UNVERIFIED** | greps found black clears (`pipeline.cpp:11148` etc.); the coverage argument holds regardless of clear colour |

Both consumers (`SL_GBufferProvider.fx:152`, `SL_BridgeDebug.fx:31`) route through the shared header, so
the decode correction propagates without touching either shader.

**Why this was invisible:** octahedral and spheremap agree at `enc=(0.5,0.5)` → `(0,0,1)` and diverge
completely off-axis (`enc=(1,0.5)`: octahedral `(1,0,0)`, spheremap `(0,0,-1)`). Head-on surfaces looked
right; grazing angles were wrong. **"Working in-world" was never evidence of correctness** — the same
lesson as the Codex-PASS rule, applied to our own eyes.

---

## 1. Guardrails (earned the hard way — do not relax)

1. **`ReShade.ini` stays `attrib +r`.** Golden copy `ReShade.ini.golden`. Corruption of a *used* ini was
   the true root cause of the whole soft-crash saga; the read-only lock is the fix. Unlock → change →
   re-lock, never leave it writable.
2. **`RenderGLMultiThreadedTextures = FALSE`.** Still required. Every textures-ON test predates the clean
   locked ini, so it is *unverified*, not *proven bad* — but do not bundle that experiment with this work.
3. **Never patch proprietary iMMERSE source.** Public headers and the public technique-state API only.
   Do not invent internal pass names.
4. **One variable per launch.** The saga's contradictions came from multi-variable runs. Repeat every
   run at least twice before believing it.
5. **Always keep a known-good fallback staged** before swapping any shader or dll.

---

## 2. Phases, each with a falsifiable gate

### Phase 0 — Prove the decode fix (no new code) ⬅ **START HERE**
The patch is deployed but **unproven in-world**. Everything downstream depends on it.

- Run the research's **basis-validation test** (§2.4): three matte planes with known world normals
  (camera-facing, camera-right, up) plus a sphere; freeze; orbit the camera 90°.
- Visualise with `SL_BridgeDebug.fx` `DebugMode 0` (`n*0.5+0.5`), and A/B against Launchpad's own normal
  debug view while both are loaded.
- **Gate:** decoded normals rotate with the camera (proving view-space, not world), the three planes read
  as three distinct stable colours, and no systematic axis swap appears. Grazing-angle surfaces must now
  differ visibly from the `.pre-octafix.bak` behaviour — if they look *identical*, the patch is not being
  picked up and nothing below is trustworthy.
- **Rollback:** restore the two `.pre-octafix.bak` files.

### Phase 1 — Basis + direct-copy decision
- Compute the angular-error heatmap `acos(saturate(dot(n_sl_transformed, n_dest_decoded)))`.
- The current chain is decode → `sl_gl_to_view` (negate Z) → `Math::octahedral_enc(-n_view)`. Note the Z
  negation is **not** expressible as a `.xy` copy even though both ends are octahedral — it folds
  hemispheres, which is exactly the `OctWrap` branch. So keep decode-transform-encode.
- **Gate:** median angular error under a few degrees against Launchpad's normals on opaque geometry.
  Do NOT adopt the research's `.xy` direct-copy optimisation — it requires proving six separate
  properties (§2.5) for a saving we have no measurement demanding.

### Phase 2 — Albedo coverage contract
Blocker 1 is already fixed; this phase is only the real one.
- Implement the **two-mask contract** (§4.2): G-buffer validity + visible-surface coverage.
- Invalid pixels bind a **neutral fallback**, never a retained previous-frame texture (silent staleness
  is worse than a visible neutral).
- **Gate:** sky, water, and an alpha-heavy scene show no magenta and no ghosted albedo; RTGI bounce colour
  on opaque geometry is unchanged from the Launchpad-fed baseline.

### Phase 3 — TAAU
⚠️ **CORRECTED 2026-07-25: the live value is `.77`, not `.66`** — verified at `ReShade.ini:21`
(`_MARTYSMODS_TAAU_SCALE=.77`). The "0.66" in earlier notes came from memory, not the file. Treat
`.77` as the live baseline and test 1.0 / .77 / .66 as separate controlled runs against
runtime-enumerated resource dimensions. Note TAAU is NOT uniform across destinations: normals are
declared at DLSS/TAAU size but motion and albedo at full buffer size, so never scale a motion
vector's magnitude by the TAAU factor.
- Per §2 the provider must decode → depth-aware downsample → normalize → re-encode at the reduced
  resolution. Naive bilinear on *encoded* normals is garbage across edges — the same class of bug as the
  point-sampling fix we already made.
- **Gate:** measure the frame-time delta of 0.66 vs 1.0, and watch avatar edges for ghosting. This is the
  single largest known perf lever in the stack — quantify it before anything else is tuned.

### Phase 4 — Motion units
- Validate native motion sign/units by measurement, not by the current guessed `*0.5` NDC→UV constant.
- **Gate:** panning shows no smear and no reversed hue in `SL_DEBUG_VIEW` Motion mode vs provider-off.

### Phase 5 — Staged Launchpad retirement (last)
Strict order, one step per launch, per §Executive verdict:
1. coexist (`Launchpad → SL provider → consumers`) while validating;
2. retire optical flow only after Phase 4 passes;
3. retire normal generation only after Phase 0/1 pass — and consciously accept losing Launchpad's
   *smoothed/textured* normal detail, which is extra quality we do not reproduce;
4. make the provider own every shared resource/declaration consumers need;
5. disable the **whole** technique via the public technique-state API with an exact allowlist;
6. cold-load with Launchpad absent to prove no declaration/include dependency remains.

**Known regression risk:** removing Launchpad removes its optical flow, which filled motion gaps where
SL velocity is zero. OF **cannot** simply be re-enabled — it soft-crashes this build.

⚠️ **CORRECTED 2026-07-25 — the camera-motion-from-depth replacement IS ALREADY BUILT.** This section
previously called it "designed but unbuilt" and a hard prerequisite. Wrong, and verified wrong in
source: `gVelocityCameraProgram` (`llviewershadermgr.cpp:251,3445-3450`) runs `velocityCameraF.glsl`,
dispatched at `pipeline.cpp:5997-6015` as `[BDMerge A5.4-1c]`. It draws **first**, filling every pixel
with camera-induced motion reprojected from scene depth so sky, excluded blended alpha and impostors
never read "static"; geometry passes then overwrite covered pixels. It needs no ABI matrix export —
prefer keeping this viewer-side rather than shipping matrices to ReShade.

So step 2 is far closer than this plan claimed. What remains is narrower but real:
1. **Two defects in the existing pass** — it uses the CURRENT unjittered projection for BOTH
   endpoints (add `last_projection_matrix_unjittered`; reset on discontinuous projection change), and
   its "correct for sky" comment is wrong: far/clear depth reconstructs a finite world point and so
   receives translation, when sky should be rotation-only or explicitly invalid.
2. **Genuinely moving non-geometry content** — particles, moving water surface, animated alpha — is
   still uncovered. That is a much smaller hole than "sky/water/alpha" as originally written.
3. **Prove the draw order** by GPU capture (camera-first, geometry-second) rather than by reading it.

---

## 3. Explicitly out of scope
- iMMERSE V4 (abandoned, GL-incompatible).
- 10-bit ReShade (shelved; our builds crash, crosire's official binary does not — never reproduced from
  source). Current stack is the shipping 8bpc dll.
- Any edit to proprietary Launchpad source.
- `RenderGLMultiThreadedTextures` re-test — separate experiment, separate session.

## 4. Immediate next action
**Phase 0 only.** The decode patch is deployed and unproven; proving or falsifying it is one in-world
session with no build. Everything else is sequenced behind it.
