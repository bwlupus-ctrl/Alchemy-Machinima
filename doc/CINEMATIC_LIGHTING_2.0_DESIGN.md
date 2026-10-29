# Cinematic Lighting 2.0 — Fixtures, Not RGB
**Design doc for Alchemy-Machinima Cinematic Light Rig 2.0**
*Grounded in `indra/newview/alcinelightrigmodel.{h,cpp}` and `alcinelightrig.{h,cpp}`*

---

## 0. Design stance

The 1.x rig is a color-and-exposure rig: pick a profile swatch (24 sRGB profiles), an EV, a beam, a gel index — a *colorist's* mental model. Real set lighting doesn't work that way. A gaffer orders a **fixture** — "M18 through half CTO and a 4×4 frame of 250" — and the look falls out of three physical facts:

1. **What's burning** — emitter color temperature (tungsten 3200K, HMI 5600K, bi-color LED between).
2. **What's in front of it** — gels (CT correction in mired, green/magenta correction) and diffusion.
3. **How big it is relative to the subject** — the single biggest determinant of hard vs soft, because **apparent source size sets penumbra width at the shadow edge**. A 20cm fresnel at 3m is a point; a 5ft octabox at 1.5m is half the sky. Same lux, radically different shadow edges.

2.0 models exactly this, and **almost every hook already exists**: `mGel`/`gelMiredShift`/`masterTempGain` already do mired math on a Planckian locus (`planckianLinearRGB`, pivot 6500K, mired clamped 40–500); `mShadowSoft` already reaches `LLPipeline::setProjectorShadowSoftness(id, 0..8)`; `mGobo` routes a cookie UUID through `rigGoboTexture()`; `updateTransition/evaluateTransition/ease()/blendLight` is already an eased crossfade on `presentation_time`. 2.0 is a *recomposition and extension*, not a parallel system.

**What SL cannot do — stated up front, faked honestly:**

| Real phenomenon | SL renderer reality | Our fake |
|---|---|---|
| Area-light penumbra (edge softness ∝ angular source size) | Punctual spotlights only; one projector-shadow blur scalar per light (0–8) | Compute angular size from modeled source diameter ÷ live orbit distance; map to the blur scalar every frame |
| Contact hardening (penumbra grows with occluder→receiver distance) | Blur uniform in shadow-map space | Not fakeable with one scalar. Accept; document. (Verify-later: per-pixel blur scale in projector shadow shader.) |
| Soft-source wrap (light bending around form) | Punctual = no wrap | Couple source size → lower falloff exponent + slightly wider FOV + more omni-bounce feed (`mBounceRatio` exists) |
| Area specular (window reflections in eyes) | Point specular only | Existing catchlight emitter; scale catchlight size with source size |
| Spectral gel transmission / CRI | RGB triplets | Mired-correct CT gels via Planckian locus (in place); colorimetric multipliers for the rest |

---

## 1. What exists today (confirmed in code)

- **`ALCineLightRigModel`** (pure, deterministic): `LIGHT_COUNT=4` (Key/Fill/Rim/Bg), `LightBase { mYawDeg, mPitchDeg, mProfile, mEV, mBeam, mOn, mGobo, mGel, mFlickerProgram, mFlickerAmount }` → `render()` → `EmitterState { mSR/mSG/mSB, mIntensity, mLightRadius, mFalloff, mFovRad, mOn, mGobo }` per light, plus omni bounce array and catchlight.
- **Gel table (15)**: `CTO +135 / ½ +70 / ¼ +35`, `CTB −110 / ½ −70 / ¼ −35` (mired, via `masterTempGain`), `Plus Green {0.78,1,0.76}`, `Minus Green {1,0.72,1}`, plus five party colors. `applyGel()` multiplies linear RGB; gel 0 = exact identity.
- **Kelvin math**: `masterTempGain(mired_shift)` uses `planckianLinearRGB` at a 6500K pivot, green-normalized (G=1.0 so temperature never changes exposure), mired clamped [40, 500] ≈ 25,000K–2000K. `Globals.mMasterTempMired` ∈ [−110, +150]; `MasterSetup` optionally carries it.
- **Shadow softness**: per-light `mShadowSoft` / `mShadowSoftness[4]`, clamped **0–8**, pushed as `LLPipeline::setProjectorShadowSoftness(projectorUUID, softness)`. Today a raw slider — nothing physical drives it.
- **Projector path**: `applyFrame()` pushes `setLightSRGBColor`, `setLightRadius`, `setLightFalloff`, `setSpotLightParams(fov)` + cookie via `rigGoboTexture(index, cookie_setting)`. 8 gobos: Default, Venetian Blinds, Window Panes, Prison Bars, Slats, Grid, Soft Dapple, Branches.
- **Fades**: `updateTransition(target[], radius, duration, presentation_time)` + `evaluateTransition` + `ease()` + `blendLight` (continuous fields lerp; discrete `mGel/mGobo/mProfile/mBeam` snap at eased t>0.5). `Globals.mTransitionSec` default 0.9s.
- **Layering established**: presets (`MasterSetup` w/ optional master EV / master mired) set base; `evalFX` (63) and `evalFlicker` (per-light, seeded, scrub-safe) multiply on top per frame from `presentation_time`.

---

## 2. Fixture model

### 2.1 `FixtureBase` extends `LightBase`
```cpp
// -------- 2.0 fixture fields (defaults = legacy behavior) --------
bool mFixtureMode   = false;  // false => 1.x profile/gel path, bit-exact
F32  mKelvin        = 5600.f; // emitter CCT, clamped [2000, 20000]
S32  mGelSlot[3]    = {0,0,0};// stacked gels, indices into GELS_V2
F32  mSourceSizeM   = 0.10f;  // effective emitting diameter, meters [0.01, 4.0]
S32  mFixturePreset = 0;      // provenance only (which named fixture seeded this)
```
`mEV`, `mBeam`, `mGobo`, `mYaw/mPitch`, flicker fields unchanged and shared by both modes. `mProfile`/single `mGel` remain the legacy path; ignored in fixture mode. **Intensity stays EV** (SL intensity is a 0–1 clamp; faking lumens would be dishonest) — label the UI in stops.

### 2.2 Kelvin + gel stack → linear RGB (per light, per frame in `render()`, replacing `profileSRGB→applyGel` when `mFixtureMode`)
```
1. mired_fixture = 1e6 / clamp(mKelvin, 2000, 20000)
2. mired_total   = mired_fixture + Σ gelMiredShift(mGelSlot[i])   // CT gels stack additively in mired (real-world rule)
3. mired_total  += globals.mMasterTempMired                        // master trim rides on top (existing field)
4. K_eff = 1e6 / clamp(mired_total, 40, 500)                       // existing clamp: 2000K–25000K
5. white[3] = planckianLinearRGB(K_eff), green-normalized (G=1)    // masterTempGain's method, generalized from 6500K pivot to arbitrary K
6. for each non-CT gel: white *= gel.mMultiplier                   // plus/minus green, party — colorimetric, order-independent
7. white *= flicker color mul; intensity from EV as today
```
Gels shift *reciprocal* CT by a constant regardless of source (full CTO on 6500K→~3200K; on 4300K→~2400K) — the codebase already commits to this, so 2.0 just moves the pivot from fixed 6500K to per-light `mKelvin`. **One new pure fn:**
```cpp
void fixtureWhite(F32 kelvin, const S32 gel_slots[3], F32 master_mired, F32 out_linear_rgb[3]); // steps 1–6
```
Green-normalization retained so a 3200K→5600K cue fade doesn't pump brightness (white-balance before grade).

### 2.3 Gel table 2.0 (`GELS_V2`) — real Lee/Rosco values (keep old `GELS` byte-identical for legacy)

| # | Gel (Lee) | Type | Mired | RGB mult | Use |
|---|---|---|---|---|---|
| 0 | None | — | 0 | 1,1,1 | identity (preserved) |
| 1 | Full CTO (204) | CT | **+159** | — | 6500K→3200K |
| 2 | ½ CTO (205) | CT | **+109** | — | daylight→~3800K workhorse |
| 3 | ¼ CTO (206) | CT | **+64** | — | subtle warm |
| 4 | ⅛ CTO (223) | CT | **+30** | — | skin kiss |
| 5 | Full CTB (201) | CT | **−137** | — | 3200K→~5700K |
| 6 | ½ CTB (202) | CT | **−78** | — | tungsten→daylight |
| 7 | ¼ CTB (203) | CT | **−49** | — | cool trim |
| 8 | ⅛ CTB (218) | CT | **−26** | — | moonlight trim |
| 9 | Plus Green (244) | tint | 0 | 0.78,1.00,0.76 | match to fluoro/discharge |
| 10 | ½ Plus Green (245) | tint | 0 | 0.89,1.00,0.88 | partial |
| 11 | Minus Green (247) | tint | 0 | 1.00,0.72,1.00 | kill fluoro green (magenta) |
| 12 | ½ Minus Green (248) | tint | 0 | 1.00,0.86,1.00 | partial |
| 13 | Bastard Amber (R02) | tint | 0 | 1.00,0.55,0.18 | theatrical warm (kept) |
| 14 | Steel Blue (R64) | tint | 0 | 0.22,0.48,1.00 | stage moonlight (kept) |
| 15 | Congo Blue (L181) | tint | 0 | 0.015,0.02,0.55 | deep club/neon (kept) |
| 16–18 | Primary R/G/B | tint | 0 | (kept) | party/FX |

CT gels sum in mired; tint gels multiply. 3 slots; no transmission-loss modeling (see intensity rationale).

### 2.4 Fixture presets `FIXTURES[]` `{name, kelvin, defaultSourceSizeM, defaultBeam, suggestedGels}` (seed the per-light fields, not a live reference)

| Fixture | Kelvin | Source | Beam | Notes |
|---|---|---|---|---|
| Tungsten Fresnel (650W) | 3200 | 0.12 | spot | hard warm; studio-era key |
| Big Fresnel (5K) | 3200 | 0.30 | spot | hard, fatter edge close |
| Open-face (Redhead) | 3200 | 0.18 | flood | punchy worklight |
| HMI (M18) | 5600 | 0.20 | spot | daylight punch, "sun through window" |
| LED bi-color panel | 2700–6500 | 0.45 | flood | modern do-everything |
| Kino/fluoro bank | 4300 | 1.20 | flood | ships ½ Plus Green — Fincher office |
| 2' China ball | 3000 | 0.60 | flood | soft omni-ish; higher bounce |
| 3' Softbox | 5600 | 0.90 | flood | interview key |
| 5' Octabox | 5600 | 1.50 | flood | beauty soft |
| Book light | 5600 | 2.40 | flood | softest; Deakins wrap |
| Practical bulb | 2700 | 0.05 | flood | motivated lamp; Candle/Firelight flicker |
| Sodium vapor street | 2200 | 0.25 | flood | +Plus Green; night grime |
| Moonlight (cheated) | 6500 +½CTB | 1.00 | flood | soft cool toplight |

This table is also Easy Mode 2.0 (§7).

---

## 3. Source size → penumbra (highest-value fake)

**Physics:** penumbra ≈ D×(r/d); hardness set by source **angular size** θ from subject (sun 0.5°=hard; overcast 180°=shadowless). Soften by making the source bigger/closer, never dimmer — intensity and softness are orthogonal. 1.x `mShadowSoft` is a raw slider; 2.0 derives it.

**Mapping (pure, unit-testable):**
```cpp
F32 penumbraSoftness(F32 source_size_m, F32 distance_m) {
    const F32 d = std::max(distance_m, 0.25f);
    const F32 theta = 2.f*std::atan(std::clamp(source_size_m,0.01f,4.f)/(2.f*d)); // rad
    const F32 theta_deg = theta*RAD_TO_DEG;
    const F32 t = std::clamp((theta_deg-0.5f)/(60.f-0.5f), 0.f, 1.f); // 0.5°→0, 60°+→max
    return 8.f*std::sqrt(t);   // sqrt: perceptual — early softness reads fast
}
```
`distance_m` = light's **live orbit distance** (`effective_radius` already in `render()`). Output feeds existing `mShadowSoftness[i]` → `setProjectorShadowSoftness()` — **no renderer change for Phase 1**. Dollying in makes shadows softer, as on set.

Worked (subject @2m): Practical 0.05m→1.4°→0.5 (razor); Tungsten 0.12→3.4°→1.8 (hard); HMI 0.20→5.7°→2.4; China ball 0.60→17°→4.2; 3' softbox 0.90→25°→5.1; 5' octabox 1.50→41°→6.6; Book light 2.40→62°→8.0 (edgeless). Octabox pulled 2m→4m: 6.6→5.0 (softness decays with distance).

**Secondary couplings (in `render()`):** (1) falloff `lerp(beamFalloff, 0.35, soft8/8)`; (2) FOV feather `fov*(1+0.18*soft8/8)` (clamp to projector range); (3) bounce feed `×(1+0.5*soft8/8)`; (4) catchlight size scales with source size; (5) **intensity untouched** — same EV = same exposure, hard or soft.

**Honest limits:** no contact hardening (one blur scalar). Verify-later: whether the projector-shadow shader can scale kernel radius by receiver-occluder depth (PCSS-lite) → Phase 4. Calibrate the 60°/sqrt constants once against `setProjectorShadowSoftness` kernel semantics. 4 shadowed projectors = existing budget.

---

## 4. Cookies & breakup library

Flat light is the amateur tell; real sets break every wash with a cucoloris/branches/blinds. Mechanism exists (`mGobo`→`rigGoboTexture`). Grow 8→24 achromatic luma masks (color comes from Kelvin+gels only):
- **Architectural**: Arched Window, French Door, Curtain Edge, Stairwell Rail, Door Crack (noir slice).
- **Organic**: Dense Foliage, Sparse Branches, Palm Dapple, Water Caustics.
- **Grip**: Classic Cucoloris, Fine Celo, Scrim Wave, Smoke Drift.
- **Graphic/hard**: Chain Link, Industrial Grate, Rotating Fan (static; animate later), Neon Sign Mask.

**Cookie × source size:** the projector texture stays pin-sharp regardless of shadow blur — the telltale mismatch. Ship each cookie at **3 pre-blur levels** (sharp/medium/heavy, authored once); `rigGoboTexture()` selects by softness bucket (`soft8<2.5→sharp; <5.5→medium; else heavy`). One S32 in the gobo lookup, zero shader work — this is what makes "5ft softbox through branches" read as dappled wrap. UI: categorized picker w/ thumbnails; no "breakup amount" slider (it's emergent from cookie + source size).

---

## 5. Cue stack (lighting console)

**Cue = complete rig state + timing** (theatrical console semantics; snapshots, not diffs):
```cpp
struct Cue {
  std::string mLabel; F32 mFadeSec=3, mDelaySec=0; S32 mProfile=0 /*ease/linear/snap*/, mFollow=-1;
  ALCineLightRigModel::Setup mSetup; ALCineLightRigModel::Globals mGlobals; ALCineLightRigModel::Transforms mTransforms;
  F32 mShadowSoftOverride[LIGHT_COUNT]; /*-1=derived*/ S32 mFX=-1;
};
struct CueList { std::string mName; std::vector<Cue> mCues; };
```
Serialized LLSD in `presetsDir()` (same plumbing as `loadSetup/saveSetup`; `sceneData/applySceneData` already round-trips rig state — capture = snapshot live).

**Transport:** GO / BACK (fade to previous using *this* cue's fade) / GOTO n / SNAP GOTO / RELEASE (fade to off via `mTransitionSec`). `Space`=GO.

**Engine — build on `updateTransition`, fix one thing:** (1) widen `blendLight`: for fixture lights lerp `mKelvin` **in mired space**, lerp `mSourceSizeM`, snap gel slots/gobo at 0.5 as today — so a 3200K→5600K fade is a true color fade, not a swatch pop; (2) transition `Globals` too (master EV/mired/bounce; mired lerp for temp); (3) cue clock on `presentation_time`, pure function of `(cue_list, active_cue, go_time, presentation_time)` — add optional **timecode mode** (each cue an absolute `mAtSec`) for full scrub-back safety; live GO-driven mode is monotonic-forward.

**Layer stack (contract):** `Cue (base, faded) → evalFX → evalFlicker → emitters`. Cues set base and may arm an FX (`mFX`); FX/flicker ride on top. Presets remain the palette; "Capture Cue" snapshots the live rig however reached. Cues embed state (provenance string only) so editing a preset never rewrites a programmed show.

---

## 6. Cinematography grounding
- **3200K vs 5600K** is the axis of the gel system; CTO/CTB tables are the actual Lee conversions.
- **Minus green** because discharge/fluoro spike green on camera (office = minus-green fixtures or plus-green your HMIs) — hence pre-greened Kino + sodium-vapor presets (*Se7en*/*Zodiac* institutional grime = leave the green in).
- **Soft = big, not dim** — book-light lesson (bounce into 4×4 then through diffusion; 60°+ source erases edges).
- **Motivated**: practical-bulb + Candle/Firelight flicker.
- **Negative fill** deliberately NOT modeled (SL has no subtraction); honest substitute in UI copy: kill Fill + drop `mBounceRatio`; contrast comes from ratio (`mRatioLock/mRatioStops` exist).
- **Named cue-list demos**: "Venetian Noir" (hard fresnel + Venetian + steep pitch), "Fincher Office" (Kino + ½ plus green, soft toplight, low ratio), "Golden Hour" (HMI + ½ CTO low pitch + Branches heavy-blur), "Interrogation" (bare practical, source 0.05, Door Crack).

---

## 7. Migration & Easy Mode
**Zero-break:** `mFixtureMode=false` default; every preset/scene/Easy-Mode path renders **bit-identically** (gel 0 identity, old `GELS` untouched, `mShadowSoft` slider authoritative when not fixture mode).
- **Upgrade (opt-in "Convert to Fixture")**: `mGel`→`mGelSlot[0]` via old→new index map; `mProfile`→nearest `(Kelvin, gel)` by min RGB distance (party colors → 6500K+party gel); user-set `mShadowSoft`→solve §3.2 backwards for equivalent `mSourceSizeM` at current radius (look doesn't jump).
- **Master fields**: `mMasterTempMired` keeps its role (global WB trim); `mHasMasterEV` unchanged.
- **Easy Mode 2.0**: keep brightness buckets; add a **fixture picker** (plain names: "Warm lamp glow", "Window sun", "Big soft light", "Office fluorescents") writing Kelvin/gels/source-size in one tap, never showing a mired. Reads back to nearest fixture bucket like `rimPresenceFromEV` does today.

---

## 8. Phased implementation plan (for Codex)

**Phase 1 — Fixture core: Kelvin/gel-stack + source-size→penumbra** *(highest quality-per-line; model-layer, unit-testable, NO renderer changes)*
1. `GELS_V2` + `fixtureWhite()` (generalize `masterTempGain` pivot; reuse `planckianLinearRGB`). Golden-value unit tests (3200K+FullCTB≈5700K white; mired stacking; identity).
2. `FixtureBase` fields on `LightBase` + sanitize + LLSD round-trip (regression test: old blobs render identical frames).
3. `penumbraSoftness()` → existing `mShadowSoftness` path; secondary couplings (falloff, FOV feather, bounce, catchlight).
4. `FIXTURES[]` + minimal UI (Kelvin slider, 3 gel slots, source-size slider w/ fixture detents).
- Risks/verify: blur-scalar semantics of `setProjectorShadowSoftness` (calibrate 60°/sqrt); FOV clamp on `setSpotLightParams`; falloff curve deferred vs forward.

**Phase 2 — Cookie library**: 16 new masks × 3 pre-blur variants (batch script); extend `rigGoboTexture` with blur-bucket selection; categorized picker. Risk: viewer-side texture/UUID strategy (follow existing 8 gobos); VRAM trivial.

**Phase 3 — Cue stack**: `Cue/CueList` LLSD + capture (reuse `sceneData`); widen `blendLight`/`updateTransition` (mired Kelvin lerp, source-size lerp, Globals pair); transport + follow timer + floater UI + timecode mode. Risk: gobo swap at t=0.5 mid-fade (matches console "block", document); cue-arms-FX during running FX (cue's `mFX` wins, restart phase at go_time).

**Phase 4 (stretch, verify-first)**: PCSS-lite contact hardening in projector shadow shader; animated cookies (rotating fan via gobo UV scroll FX).

**Key files:** `indra/newview/alcinelightrigmodel.{h,cpp}` (fixture model, gel table, penumbra fn, `render()`), `indra/newview/alcinelightrig.{h,cpp}` (`tickShared` softness feed, `rigGoboTexture` bucket, `updateTransition` widening, cue engine), `LLPipeline::setProjectorShadowSoftness` (existing 0–8 blur API).
