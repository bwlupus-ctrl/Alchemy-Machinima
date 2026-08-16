# Cinematic Light Rig — Enhancements (Master Temp · Per-Light Gobos · Bundled Gobo Library · Gobo Presets · Shadow Fix-It · Radius UX) — Deep Design

**Status:** design pass complete. **No source modified.**
**Date:** 2026-08-16.
**Answers:** `doc/CINE_LIGHT_RIG_ENHANCE_SEED.md` sections A–F, all six in ONE
delivery (ship-whole). Gizmo drag-editing is explicitly NOT in this delivery
(user decision, fixed).
**Base architecture (finals, not re-litigated):**
`doc/CINEMATIC_LIGHT_RIG_DEEP_DESIGN.md`,
`doc/CINE_LIGHT_RIG_MASTER_PRESETS_DESIGN.md`. Current state:
`doc/CINE_LIGHT_RIG_STATUS.md` (base + masters + mirror fix built and
confirmed working in-world 2026-08-16).

Claim labels: **PROVES** = I read the code at the cited line. **IMPLIES** =
comment/doc or unbroken-but-unread chain. **INFERENCE** = reasoning from proven
facts; could be wrong. Guesses say "guess".

---

## 0. Executive summary

Six enhancements, all inside the rig's own files plus one shipped data file,
seven shipped PNGs, and one generation script — **zero pipeline.cpp edits, zero
shader edits, zero llprimitive edits, zero llvovolume/texture-system edits, no
scene or preset version bump, five new settings keys**:

1. **Master Colour Temperature (§A)** — one global RELATIVE warm/cool trim in
   **mireds** (`CineLightRigMasterTempMired`, F32, default 0, range
   [−110, +150]). The model converts the mired shift to a **linear-RGB white-
   balance gain** (Planckian locus → XYZ → linear sRGB, green-normalized) and
   multiplies every light's profile colour in LINEAR space, clamped [0,1],
   then converts back to sRGB so the existing `setLightSRGBColor` transport
   path is untouched. Shift = 0 is a **bitwise no-op fast path** (the gain
   stage is skipped entirely). Relative gels are preserved by construction:
   one gain vector multiplies every light. Pure model function, TUT-pinned
   against independently computed literals (§A.2 table).
2. **Per-light gobos (§B)** — `LightBase` gains a per-light **gobo INDEX**
   (`S32 mGobo`, default 0), exactly like profile/beam: settings-backed
   (4 new keys `CineLightRig{Key,Fill,Rim,Bg}Gobo`), carried by presets and
   scene base blocks with a `gobo_idx`+`gobo_name` double-write, blended
   discretely, sanitized by clamp. The controller maps index → texture UUID
   and drives it through the **existing per-emitter `setLightTextureID`**
   reconcile in `applyFrame` — the pipeline already binds each projector's own
   texture per light draw (`pipeline.cpp:17584`), so **NO shader or pipeline
   change** is needed. Index 0 = today's cookie (`rigCookie()`, still driven
   by `CineLightRigCookieUUID`) — absent/zero = current behaviour. Omnis keep
   a null texture; gobos are projector-only.
3. **Bundled gobo library (§C)** — 7 procedurally generated 512×512 grayscale
   PNGs (blinds, panes, bars, slats, grid, dapple, branches) under
   `skins/default/textures/cine_gobos/`, produced by a checked-in Python
   script (deterministic, reviewable). Loaded via
   `getFetchedTextureFromFile(..., FTT_LOCAL_FILE)` — the file-backed texture
   registers in `gTextureList` under a UUID that the light-texture fetch path
   finds (§1.3 proves the chain). **No `textures.xml` entries are needed**
   (seed contradiction, §1.6-X1) and **no `viewer_manifest.py` change** (the
   existing glob ships the directory, §1.3). Missing file → default cookie;
   unresolvable texture → the pipeline's own white fallback — **never black**.
4. **Gobo master presets (§D)** — 5 new masters (Venetian Noir, Window Light,
   Prison Bars, Dappled Forest, Skylight Grid) appended to
   `cine_light_rig_presets.xml` (26 → 31 file entries; 32 masters with
   compiled Classic), fully specified per light including gobo index.
5. **Shadow fix-it button (§E)** — one button under the existing amber shadow
   hint that computes the same `requested` count the hint computes (shared
   helper, so they agree by construction) and writes
   `BDMergeMaxSpotShadows = clamp(requested, 2, 6)`. Settings write only; the
   existing `handleShadowsResized` listener reallocates live
   (`llviewercontrol.cpp:1155`). The rig can request at most 4 (< 6 cap), so
   the "exceeds 6" case is unreachable from this button; the clamp stays as
   defence.
6. **Radius UX (§F)** — spinner increment 0.1 → 0.25, a rewritten tooltip
   naming both thresholds, and an amber "Radius" label cue whenever the
   nominal radius exceeds the 9.09 m full-brightness ceiling
   (`SCALED_RADIUS_CEIL`). No new control, no clamp change; 0.5..512 stays.

New model surface: `Globals::mMasterTempMired`, `LightBase::mGobo`,
`EmitterState::mGobo`, constants `MASTER_TEMP_MIRED_MIN/MAX`,
`MASTER_TEMP_PIVOT_KELVIN`, `GOBO_COUNT`, functions `masterTempGain`,
`goboName`. Two new TUT tests (`test<13>` master temp, `test<14>` gobo), plus
a one-field extension of the frozen golden renderer. Settings: 5 new keys, all
added to `sceneSettingsList`. **Both the scene `light_rig` block and the
preset file format stay version 1** — every addition is an optional field
whose absence means "today's behaviour" (§1.5 states the full forward/backward
matrix).

---

## 1. Verification — what the code proves

### 1.1 Colour flow, end to end (the §A foundation)

| # | Fact | Evidence | Label |
|---|---|---|---|
| V1 | Colour source is a discrete per-light profile index (0..23) into a compiled table of sRGB triplets, all components in [0,1] | `PROFILES[24]` at `alcinelightrigmodel.cpp:50-75`; lookup `profileSRGB` `:1141-1148` | PROVES |
| V2 | The model copies the profile triplet verbatim into `EmitterState.mSR/mSG/mSB` — no gamma, no clamp, no headroom interaction in the model | `alcinelightrigmodel.cpp:554-555` (lookup), `:564-566` (projector), `:583-585` (omni) | PROVES |
| V3 | Intensity is a **separate channel** (`mIntensity`, EV → `exp2` → clamp [0,1] in `intensityFromEV` `:505-516`); the headroom re-base (`total_ev − mHeadroomStops`, `:547-551`) never touches mSR/mSG/mSB | same lines | PROVES |
| V4 | The controller pushes colour via `setLightSRGBColor(LLColor3(mSR,mSG,mSB))` for both projector and omni | `alcinelightrig.cpp:809-810`, `:839-840` | PROVES |
| V5 | `setLightSRGBColor` = `setLightLinearColor(linearColor3(color))` — one sRGB→linear conversion, then `LLLightParams::setLinearColor` which **clamps to [0,1]** (`mColor.clamp()`); the param stores linear colour, alpha = intensity | `llvovolume.cpp:3208-3225`; `llprimitive.h:135-141`, `:161` | PROVES |
| V6 | The sRGB→linear conversion is the standard piecewise EOTF: `v < 0.04045 ? v/12.92 : pow((v+0.055)/1.055, 2.4)` | `llmath.h:503-511`; applied per channel by `linearColor3p`, `v3color.h:462-470` | PROVES |
| V7 | The inverse (`linearTosRGB`) exists and is used by `srgbColor3` | `v3color.h:452-460` | PROVES (existence; body IMPLIES the standard inverse) |

**Consequence (INFERENCE, load-bearing):** the correct place for a relative
temperature trim is a **multiplicative gain in LINEAR RGB** between V2 and V4.
Because the model outputs sRGB and the controller's transport is
`setLightSRGBColor`, the model applies the gain internally as
sRGB → linear → gain → clamp [0,1] → sRGB, leaving V4/V5 untouched. The
clamp inside the model mirrors V5's clamp, so the model's output remains the
authored colour and the downstream clamp is a no-op. The seed's phrase
"gamma-corrected" for today's colour is slightly off — the model emits the
table's sRGB values raw and the *viewer* linearizes (V5); nothing in the rig
gamma-corrects. Design unaffected; noted for accuracy.

### 1.2 Per-projector texture path (the §B foundation)

| # | Fact | Evidence | Label |
|---|---|---|---|
| V8 | The projector cookie is a **per-object** parameter: `LLLightImageParams::mLightTexture`, `isLightSpotlight() = mLightTexture.notNull()` | `llprimitive.h:341-346` | PROVES |
| V9 | Each emitter is created with its own `setLightTextureID(rigCookie())` (projector) or `LLUUID::null` (omni) | `alcinelightrig.cpp:514-522` (creation; the seed's ":516") | PROVES |
| V10 | `applyFrame` reconciles the cookie **per emitter per frame**: `if (projector->getLightTextureID() != cookie) setLightTextureID(cookie)`; omnis are forced back to null | `alcinelightrig.cpp:777`, `:805-808`, `:835-837` | PROVES |
| V11 | The deferred pipeline binds **each volume's own** light texture at draw time: `LLViewerTexture* img = volume->getLightTexture(); if (img == NULL) img = LLViewerFetchedTexture::sWhiteImagep;` — two sites (main spot path, hero/second path) | `pipeline.cpp:17584-17588`, `:17769-17773` | PROVES |
| V12 | `getLightTexture()` resolves the per-object UUID through `LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT, true, BOOST_NONE)` | `llvovolume.cpp:3411-3420`, also `:947-958` (per-frame stats) | PROVES |
| V13 | An omni can never enter the projector path: null texture ⇒ `isLightSpotlight()` false (V8) — re-confirmed for this delivery | `llprimitive.h:345` + V10 | PROVES |

**Consequence:** distinct cookies per rig light are already fully supported —
world spotlights carry distinct cookies today and V11 binds per volume. A
per-light gobo is purely a per-object parameter VALUE change on rig-owned
objects. **No shader, pipeline, or llvovolume change.** V11 also gives the
last-ditch fallback: an unresolvable texture binds WHITE (an un-gobo'd
projector), never black.

### 1.3 Bundled texture → light-texture chain (the §C foundation)

The make-or-break question: can a file shipped in `skins/default/textures`
serve as a projector cookie through the UUID-keyed fetch in V12? Yes — chain:

1. `getFetchedTextureFromFile(name, FTT_LOCAL_FILE, mips, boost)` resolves the
   file via `gDirUtilp->findSkinnedFilename("textures", name)` — i.e. relative
   to `skins/*/textures/` — and forwards to `getImageFromUrl("file://" +
   full_path, ...)` (`llviewertexturelist.cpp:406-433`). PROVES.
2. `getImageFromUrl` keys the texture by `new_id.generate(url)` (deterministic
   from the absolute path) and registers it via
   `addImage(imagep, get_element_type(boost))` (`llviewertexturelist.cpp:
   450-499`). PROVES.
3. `get_element_type` returns `TEX_LIST_STANDARD` for every boost except
   ICON/THUMBNAIL (`llviewertexturelist.cpp:76-79`); the registry key is
   `(UUID, ETexListType)` — **FTT is not part of the key**
   (`llviewertexturelist.h:62-79`, `:230-232`). PROVES.
4. The light path's later `getFetchedTexture(id, FTT_DEFAULT, true,
   BOOST_NONE)` (V12) → `getImage` → `findImage(id, TEX_LIST_STANDARD)`
   → **finds the existing file-backed entry and returns it** — no grid fetch;
   the FTT-mismatch warn is skipped because the request is `FTT_DEFAULT`
   (`llviewertexturelist.cpp:582-611`, the guard at `:598`). PROVES.
5. Precedent for file-backed textures rendered in-world (not UI): parcel
   boundary textures `world/NoEntryLines.png` (`llviewerparcelmgr.cpp:
   160-161`), avatar cloud `SoftDotNoBack.png` (`llvoavatar.cpp:1252`). PROVES.
6. Packaging: `viewer_manifest.py:175-183` ships `skins/*/textures` with globs
   `*/*.png` (one subdirectory deep) and `*.png` — a new
   `cine_gobos/*.png` directory ships with **no manifest change**. PROVES.
7. Missing file: `getImageFromFile` warns and returns the IMG_DEFAULT texture
   (`llviewertexturelist.cpp:423-428`) — but the design never relies on this;
   the registry checks `findSkinnedFilename` itself and falls back to
   `rigCookie()` (§B.3), and V11's white fallback backstops everything.

Eviction guard: the controller holds a static `LLPointer` per gobo texture for
the session (§B.3), and the light path adds per-frame texture stats
(`llvovolume.cpp:955`), so the decode stays resident like any in-use texture.

### 1.4 Shadow slots (the §E foundation)

| # | Fact | Evidence | Label |
|---|---|---|---|
| V14 | Compile-time cap `MAX_SPOT_SHADOWS = 6` | `pipeline.h:1028-1032` | PROVES |
| V15 | Runtime count = `BDMergeMaxSpotShadows` clamped **[2, 6]**, read via cached control default 2 | `pipeline.cpp:597-601` (`bdmergeMaxSpotShadows()`) | PROVES |
| V16 | Changing the setting reallocates live: listener `handleShadowsResized` | `llviewercontrol.cpp:1155`; consumption at `pipeline.cpp:1598-1648` | PROVES |
| V17 | The setting's declared type is **U32** (`settings.xml:6668-6678`, Type "U32", default 2); `llprismlens.cpp:5792-5795` reads it as U32; **the panel currently reads it as `getS32`** (`alpanelcinelightrig.cpp:633`) — works via LLSD conversion, but new code writes with `setU32` to match the declared type | cited lines | PROVES |
| V18 | The hint: `requested` = 1 if mode==1 && KeyOn, else (mode==2) count of On lights, else 0; `hint_state 1` iff `requested > slots`; text "N rig projectors request M shadow slots..." | `alpanelcinelightrig.cpp:604-648` | PROVES |
| V19 | `requested` ≤ `LIGHT_COUNT` = 4 by construction (V18 counts at most the four rig lights) — the fix-it target can never exceed 6 | V18 + `alcinelightrigmodel.h:17` | PROVES |

### 1.5 Persistence & schema facts (the version-bump decision's inputs)

| # | Fact | Evidence | Label |
|---|---|---|---|
| V20 | Local preset file = `{version:1, name, radius, lights[4]}`; each light = yaw/pitch/profile_idx/profile_name/ev/beam_idx/beam_name/on. **No global (MasterEV, headroom, cookie...) is stored in any preset** | `lightToLLSD` `alcinelightrig.cpp:201-213`, `setupToLLSD` `:266-277`, `saveSetup` `:1519-1521` | PROVES |
| V21 | `setupFromLLSD` reads known keys only; absent LLSD keys read as 0/""/false; unknown keys are ignored; index+name double-write with name-fallback is the profile/beam forward-compat idiom | `alcinelightrig.cpp:215-264` | PROVES |
| V22 | Master library file: same light schema inside `{version:1, presets:[...]}`, parsed by the same `setupFromLLSD`; wrong version ⇒ whole file discarded to compiled Classic | `alcinelightrig.cpp:1249-1416` | PROVES |
| V23 | Scene `light_rig` block v1 = anchor/mirror/orbit/fx/seed/phase/shafts/heroes + denormalized `base` (`setupToLLSD`); unknown version warns-and-preserves | `sceneData` `:1550-1581`, `applySceneData` `:1583-1706` (version gate `:1600-1609`) | PROVES |
| V24 | Every persisted rig control ALSO rides the Director scene's flat settings map via `sceneSettingsList()` (42 rig keys today); on load, **only keys present in the map are applied — absent keys leave the live value untouched** | `llfloaterdirector.cpp:645-768` (list, rig keys `:721-765`), save `:885-892`, load `:1036-1045` (the `has()` guard at `:1038`) | PROVES |
| V25 | An old scene's `base` block, run through the new `setupFromLLSD`, yields `mGobo = 0` for all four lights (absent key → `asInteger()` = 0), and `applySceneData` applies it via `writeSetupToSettings` — so **loading an old scene explicitly resets the gobos to the default cookie**, reproducing its original look | V21 + `alcinelightrig.cpp:1653-1660`, `writeSetupToSettings` `:451-466` | PROVES (chain) + INFERENCE on LLSD `asInteger()` of undefined = 0 (standard LLSD semantics, relied on by `:235` already) |

**DECISION — no version bumps anywhere.** Stated per the seed's item 4:

- **Preset files (local + master library): stay version 1.** Additions are two
  optional per-light keys (`gobo_idx`, `gobo_name`). Old file → new viewer:
  absent → 0 = default cookie = today's look. New file → old viewer: unknown
  keys ignored by the old `setupFromLLSD` (V21) — the preset loads minus
  gobos. That is graceful degradation, not corruption; a bump would instead
  make the old viewer REJECT the file (`version != 1` gate, `alcinelightrig.
  cpp:1494`), which is strictly worse for a purely additive field.
- **Scene `light_rig` block: stays version 1.** The `base` map gains the same
  optional per-light keys with the same matrix (V25 gives old-scene fidelity
  for free). Master temp is deliberately NOT in the block (below).
- **Master temp rides `sceneSettingsList` only** (like MasterEV — V20/V24
  precedent; the block holds session state + base, globals ride the flat
  map). Old-build scenes lack the key → the live trim survives a scene load
  (V24 absent-key semantics). This is the same behaviour class as every
  settings key ever added after a scene was saved; accepted and documented
  (risk §7-R8). Scenes saved by the new build always carry it.
- **Seed lean overturned:** the seed suggested presets "probably" store the
  master temp "as a global alongside MasterEV" — but **MasterEV is not stored
  in presets** (V20). Trusting the codebase: presets are pure `Setup`
  (radius + lights) and stay that way; a global trim baked into per-look
  files would double-apply against the scene settings map on load. Master
  temp is settings + scene-settings-map only.

### 1.6 Seed claims contradicted (trust the codebase)

- **X1 — "Bundled textures ship via a `textures.xml` entry + a PNG" is wrong
  for THIS use.** `textures.xml` is the LLUIImage registry (UI images); the
  light-texture path never consults it. `getFetchedTextureFromFile` resolves
  directly through `findSkinnedFilename("textures", ...)` (§1.3.1), and
  packaging is glob-based, not textures.xml-based (§1.3.6). The
  `Command_Lightbox_Icon` row the seed cites (`textures.xml:153`) is the UI
  idiom, correct for toolbar icons, irrelevant to projector cookies. **No
  textures.xml entries are added** — deliberately, to keep gobo assets out of
  the UI-image namespace. (If a future gobo picker wants thumbnails, entries
  can be added then; deferred §8.)
- **X2 — "profile index → RGB, gamma-corrected, then used as the light
  colour":** the model applies no gamma; linearization happens once in
  `setLightSRGBColor` (V5). Same conclusion (gain must act in linear), wrong
  attribution. §A places the conversion inside the model's gain stage only.
- **X3 — line numbers:** the global-cookie set the seed cites as
  "alcinelightrig.cpp:516" is the *creation-time* site (now `:514-517`); the
  per-frame reconcile the design actually modifies is `:805-808`. The
  PROFILES table is `:50-75` (seed said 50-81). Content correct, lines drifted.

---

## Section A — Master Colour Temperature (relative shift)

### A.1 Scale and range: mireds, [−110, +150], pivot 6500 K

**Unit: mireds** (10⁶/K), positive = warmer. Chosen over a normalized [−1,1]
or a Kelvin target because mired shift is the perceptually-uniform,
industry-standard axis for a white-balance TRIM: equal mired steps read as
equal warmth steps across the whole range, and gel/filter strengths are quoted
in mireds (Full CTO ≈ +167, Full CTB ≈ −131), which makes the numbers
meaningful to the operator this feature serves. A Kelvin target would be an
ABSOLUTE control — the seed fixed RELATIVE.

**Pivot: 6500 K** (`MASTER_TEMP_PIVOT_KELVIN = 6500.f`) — D65, the sRGB
reference white; shift 0 must mean "no trim", and 6500 K is the temperature at
which the gain is exactly {1,1,1} by construction. The pivot is a calibration
constant, not a claim about any profile's temperature.

**Range: [−110, +150] mireds** (`MASTER_TEMP_MIRED_MIN/MAX`), asymmetric and
deliberate: pivot mired M₀ = 10⁶/6500 ≈ 153.85; shift −110 ⇒ 22 807 K (deep
blue sky), +150 ⇒ 3291 K (near-Full-CTO warm). Both endpoints sit inside the
Planckian-locus approximation's validity (1667–25 000 K), so **the slider has
no dead zone** — a symmetric ±150 would go flat below ≈ −114 when the Kelvin
clamp engaged. UI slider min/max mirror the model constants exactly (the
existing UI-range-equals-model-clamp rule).

### A.2 The math, precisely

One pure model function:

```cpp
// Linear-RGB white-balance gain for a relative CT trim of `mired_shift`
// mireds about the 6500 K pivot. gain[1] == 1 exactly; gain(0) == {1,1,1}
// exactly (callers fast-path shift == 0 and never call this with 0).
void masterTempGain(F32 mired_shift, F32 gain[3]);
```

Definition (every constant stated; implement in F32 with F64 intermediates
for the cubics):

1. `M0 = 1e6 / 6500`; `M = clamp(M0 + shift, 40, 500)` (2000–25 000 K
   belt-and-braces — unreachable with the sanitized shift range, kept as
   defence); `K = 1e6 / M`.
2. **Planckian locus** (Kim et al. cubic approximation, the standard one):
   - `x = -0.2661239e9/T³ - 0.2343589e6/T² + 0.8776956e3/T + 0.179910` for
     T ≤ 4000, else
     `x = -3.0258469e9/T³ + 2.1070379e6/T² + 0.2226347e3/T + 0.240390`.
   - `y` from `x` by the matching cubic per band (T ≤ 2222 / ≤ 4000 / > 4000):
     `y = -1.1063814x³ - 1.34811020x² + 2.18555832x - 0.20219683`,
     `y = -0.9549476x³ - 1.37418593x² + 2.09137015x - 0.16748867`,
     `y = 3.0817580x³ - 5.87338670x² + 3.75112997x - 0.37001483`.
3. xy → XYZ with Y = 1: `X = x/y, Z = (1−x−y)/y`; XYZ → **linear sRGB** via
   the standard matrix (`3.2406 −1.5372 −0.4986 / −0.9689 1.8758 0.0415 /
   0.0557 −0.2040 1.0570`); negative components floor to 0.
4. `w = rgb(K)`, `w0 = rgb(6500)`; `gain[i] = w[i] / max(w0[i], 1e-6)`;
   **green-normalize**: `gain[i] /= max(gain[1], 1e-6)` (so gain[1] == 1 —
   green carries most luminance, MasterEV owns brightness, and the invariant
   "G never changes" makes the tests and the gel-order statement clean);
   final clamp `gain[i] ∈ [0.1, 10]` (defence; unreachable in range).

**Application inside `render()`** (the only call site), composing with V1–V7:

```cpp
F32 temp_gain[3];
const bool temp_active = safe_globals.mMasterTempMired != 0.f;   // -0.f == 0.f: fast path taken
if (temp_active) masterTempGain(safe_globals.mMasterTempMired, temp_gain);
...
profileSRGB(light.mProfile, rgb);            // existing, :554-555
if (temp_active)
{
    for (int c = 0; c < 3; ++c)
    {
        F32 lin = srgbChannelToLinear(rgb[c]);       // model-local copy of llmath.h:503-511
        lin = std::clamp(lin * temp_gain[c], 0.f, 1.f);
        rgb[c] = linearChannelToSRGB(lin);           // standard inverse: v<=0.0031308 ? v*12.92 : 1.055*pow(v,1/2.4)-0.055
    }
}
```

Both projector and omni take the shifted `rgb` (they already share it,
V2) — the bounce warms/cools with its light, as a real bounce would.

- **Why sRGB in/out:** keeps `EmitterState` and the controller transport (V4)
  byte-identical in shape and path; the model's [0,1] clamp mirrors V5's so
  the downstream `mColor.clamp()` is a no-op; sRGB↔linear conversion is
  strictly monotone per channel, so every ordering statement made in linear
  survives to the emitted sRGB.
- **The model cannot include `llmath.h`** (`stdtypes.h`-only discipline —
  master design SA-2 precedent): it carries its own copies of the two
  piecewise formulas with a cross-referencing comment naming
  `llmath.h:503` — same maintenance contract as the `GHOST_SCALE` mirror.
  Bit-identity with llmath's versions is NOT required: the conversion
  round-trip happens only inside the gain stage, which the shift==0 fast path
  skips entirely, so today's output is reproduced bitwise at shift 0 and the
  (sub-ULP) round-trip drift exists only when the operator has asked for a
  shifted look.
- **Headroom / re-base / intensity composition: none.** Colour and intensity
  are separate channels end to end (V3, V5 alpha). PROVEN, not asserted.
- **Relative gel preservation, stated exactly:** for lights i, j with linear
  colours cᵢ, cⱼ, the same gain vector multiplies both, so per-channel ratios
  cᵢ/cⱼ are invariant **until the [0,1] clamp**; the clamp only compresses
  channels already ≥ 1/gain (a fully-warm gel cannot get warmer — physically
  correct saturation) and compression is monotone, so a cooler light NEVER
  becomes warmer than a warmer light. Warm/cool CONTRAST in un-saturated
  channels is preserved exactly.

**Reference values** (computed with the formulas above in double precision;
the reference Python implementation is reproduced in §5 for the reviewer and
must be committed beside the test as a comment or doc pointer). C++ F32 must
match within relative 2e-3:

| shift (mired) | K | gain R | gain G | gain B |
|---|---|---|---|---|
| −110 | 22 807 | 0.701629 | 1.0 | 1.823553 |
| −50 | 9 630 | 0.838483 | 1.0 | 1.338201 |
| −25 | 7 761 | 0.913639 | 1.0 | 1.159399 |
| 0 | 6 500 | 1.0 | 1.0 | 1.0 |
| +25 | 5 591 | 1.098071 | 1.0 | 0.859702 |
| +50 | 4 906 | 1.208315 | 1.0 | 0.736917 |
| +100 | 3 939 | 1.466342 | 1.0 | 0.534902 |
| +150 | 3 291 | 1.768046 | 1.0 | 0.380079 |

Worked composition example (test pin material): profile 5 "8000K Moon"
sRGB (0.45, 0.55, 1.00) at shift +100 → emitted sRGB **(0.5373, 0.55,
0.7579)** — warmed but still unmistakably the cool gel; profile 0 "2700K
Incan" (1.00, 0.55, 0.15) at the same shift → (1.00 [R clamped], 0.55,
0.103) — order preserved.

### A.3 Model plumbing

- `Globals` gains `F32 mMasterTempMired = 0.f` (`alcinelightrigmodel.h:57-67`).
- `sanitizeGlobals` (`alcinelightrigmodel.cpp:388-413`) adds:
  `output.mMasterTempMired = std::clamp(finiteOr(globals.mMasterTempMired,
  0.f), MASTER_TEMP_MIRED_MIN, MASTER_TEMP_MIRED_MAX);` — NaN/inf → 0 → the
  bitwise no-op path (the seed's "shift NaN → 0" robustness rule).
- Header constants: `MASTER_TEMP_MIRED_MIN = -110.f`, `MASTER_TEMP_MIRED_MAX
  = 150.f`, `MASTER_TEMP_PIVOT_KELVIN = 6500.f`.
- `masterTempGain` declared in the header (TUT-visible), implemented with the
  file-local locus/matrix helpers.
- `render()` change exactly as A.2 — the gain computed once per call, applied
  per light after `profileSRGB`. Everything else in `render()` untouched;
  `distance_ev`, headroom, scale logic unmodified.

### A.4 Controller and UI

- `readSettings` (`alcinelightrig.cpp:373-449`) adds one
  `LLCachedControl<F32> master_temp(gSavedSettings,
  "CineLightRigMasterTempMired")` → `globals.mMasterTempMired`. The existing
  `sanitizeGlobals` call at `:447` sanitizes it. Nothing else in the
  controller changes for §A — the model owns the math.
- **Settings key** (settings.xml, beside the other rig keys ~line 82 block):
  `CineLightRigMasterTempMired`, Type F32, Persist 1, Value 0.0, Comment
  "Global white-balance trim in mireds (+warm/−cool) applied to all rig
  lights, preserving each light's gel".
- **UI** (`panel_cine_light_rig.xml`, Rig globals section): one new full-width
  row after the OffsetZ/Headroom row — label "Warmth" (left 8), slider
  `control_name="CineLightRigMasterTempMired"` `min_val="-110"
  max_val="150"` `increment="5"` `decimal_digits="0"` `can_edit_text="true"`
  (left 52, width 266, the yaw-slider geometry), reset button at 322
  (`CineLightRig.ResetControl` parameter = the key — the D.1 roster rule:
  value control ⇒ reset button; roster 29 → 34 with §B's four). Tooltip:
  "Relative colour-temperature trim in mireds: + warms, − cools all lights
  together; each light keeps its own gel. 0 = off."
  Subsequent rows shift down 26 px; panel height grows accordingly (§B.5
  gives the consolidated layout delta). Settings binding syncs both hosts
  for free (V7 of the base design).
- **Scene:** `sceneSettingsList()` gains the key (one line,
  `llfloaterdirector.cpp:765` region). Not in the `light_rig` block (§1.5).
- **Presets:** not stored (§1.5 decision). Loading any preset therefore keeps
  the operator's current trim — a preset is a look; the trim is the grade on
  top, exactly like MasterEV behaves today.

---

## Section B — Per-light gobos

### B.1 Decision: a per-light INDEX in the model's `LightBase`

The gobo joins `mProfile`/`mBeam` as the third discrete per-light look field:
`S32 mGobo = 0` in `LightBase` (`alcinelightrigmodel.h:33-41`). Rationale,
against the controller-only alternative:

- Presets and scene base blocks carry it for free through the existing
  `Setup` serialization (V20/V21/V25) — a gobo preset (§D) is then just data.
- The model stays pure: it stores an index and a name table, never a texture
  or UUID (same purity line profiles already draw — names in the model,
  RGB/texture semantics at the edges).
- Transitions and FX behave correctly by construction: `blendLight` swaps the
  discrete field at eased > 0.5 exactly like profile/beam; FX frames carry
  gobo 0 (their `LightBase`s are memset — FX project through the default
  cookie, stated and intended).
- Controller-only per-light settings would need a parallel 4-slot plumbing
  outside `Setup` and a second serialization path — strictly more code for
  strictly less capability.

**Index 0 = "Default"** = today's `rigCookie()` — which itself still honours
`CineLightRigCookieUUID`, so the existing custom-UUID escape hatch survives as
the definition of slot 0 (the seed's B "custom UUID" question: library indices
with slot 0 = the setting-driven cookie; per-light custom UUIDs deferred §8).

### B.2 Model changes (complete field-threading checklist)

Adding a field to `LightBase` touches every place the struct is copied
field-by-field — each is named here because a missed one is a silent
gobo-drop (the memcmp-based tests in §5 pin them):

| Site | Change |
|---|---|
| `alcinelightrigmodel.h` | `LightBase::mGobo` (S32, = 0, appended after `mOn`); `EmitterState::mGobo` (S32, = 0); `constexpr S32 GOBO_COUNT = 8;` `const char* goboName(S32 index);` |
| `cleanLight` (`:119-130`) | `output.mGobo = std::clamp(input.mGobo, 0, GOBO_COUNT - 1);` |
| `copyCleanLights` (`:208-222`) | copy `mGobo` (field-by-field copier — MUST be extended) |
| `computeLive` (`:451-471`) | `out[i].mGobo = base.mGobo;` (field-by-field — MUST be extended; without this no gobo ever reaches a live light) |
| `blendLight` (`:473-503`) | `output.mGobo = eased > 0.5f ? safe_target.mGobo : safe_start.mGobo;` (the profile/beam discrete rule) |
| `render()` (`:518-598`) | `projector.mGobo = light.mGobo;` omni's stays 0 (memset) — gobos are projector-only (V13) |
| `setLight` / `initializeFX` | unchanged — memset leaves gobo 0; FX use the default cookie by design |
| `goboName` | table `{"Default", "Venetian Blinds", "Window Panes", "Prison Bars", "Slats", "Grid", "Soft Dapple", "Branches"}`, clamped lookup like `profileName` (`:1136-1139`) |
| `classicSetup` | unchanged (memset ⇒ gobo 0) |

`Setup`/`Globals`/`Transforms` sanitizers otherwise untouched. Layout note:
`LightBase` grows 24 → 28 bytes; all memset/memcmp idioms in model and tests
remain valid because construction is memset-based throughout.

### B.3 Controller: index → texture registry + per-light reconcile

**Registry** (file-local in `alcinelightrig.cpp`, beside `rigCookie()`):

```cpp
const char* const GOBO_FILES[GOBO_COUNT] = {
    nullptr,                    // 0 = Default -> rigCookie()
    "cine_gobos/gobo_blinds.png",   "cine_gobos/gobo_panes.png",
    "cine_gobos/gobo_bars.png",     "cine_gobos/gobo_slats.png",
    "cine_gobos/gobo_grid.png",     "cine_gobos/gobo_dapple.png",
    "cine_gobos/gobo_branches.png",
};

LLUUID rigGoboTexture(S32 index)    // index already model-sanitized; re-guard anyway
{
    if (index <= 0 || index >= GOBO_COUNT) return rigCookie();
    static LLPointer<LLViewerFetchedTexture> cache[GOBO_COUNT];
    static bool attempted[GOBO_COUNT] = {};
    if (!attempted[index])
    {
        attempted[index] = true;
        if (!gDirUtilp->findSkinnedFilename("textures", GOBO_FILES[index]).empty())
        {
            cache[index] = LLViewerTextureManager::getFetchedTextureFromFile(
                GOBO_FILES[index], FTT_LOCAL_FILE, MIPMAP_YES,
                LLGLTexture::BOOST_NONE);
        }
        if (cache[index].isNull())
        {
            LL_WARNS("CineLightRig") << "Bundled gobo missing: "
                << GOBO_FILES[index] << "; using default cookie" << LL_ENDL;
        }
    }
    return cache[index].notNull() ? cache[index]->getID() : rigCookie();
}
```

- `BOOST_NONE` ⇒ `TEX_LIST_STANDARD` registration (§1.3.3) ⇒ the light path's
  `FTT_DEFAULT/BOOST_NONE` lookup finds it (§1.3.4). The held `LLPointer`
  pins the entry for the session; per-frame stats keep it decoded (§1.3
  eviction note). One-time attempt per index — no per-frame filesystem work.
- Fallback chain, explicit: bad index → `rigCookie()`; missing file →
  `rigCookie()` (existence pre-check, warn once); UUID that later fails to
  resolve → pipeline binds WHITE (V11) — an un-gobo'd projector. **Black is
  unreachable.**
- New includes in `alcinelightrig.cpp`: `llviewertexture.h` (manager +
  `LLViewerFetchedTexture`); `lldir.h` already included.

**`applyFrame` reconcile** (`alcinelightrig.cpp:772-808`): replace the single
hoisted `const LLUUID cookie = rigCookie();` (`:777`) with a per-light lookup
inside the projector branch:

```cpp
const LLUUID cookie = rigGoboTexture(state.mGobo);   // per light i
if (projector->getLightTextureID() != cookie)
{
    projector->setLightTextureID(cookie);
}
```

The existing != guard (V10) plus `setLightTextureID`'s own no-change guard
(`llvovolume.cpp:3144`) keep this a no-op per frame at steady state — same
cost profile as today. A live `CineLightRigCookieUUID` edit still propagates
next frame for every light on index 0, preserving current behaviour exactly.
Omni branch untouched (null forced, `:835-837`). `createEmitter` (`:516`)
keeps seeding `rigCookie()` — the first `applyFrame` reconciles to the
per-light gobo; transient is at most one frame on a brand-new emitter.

### B.4 Settings + schema

- **4 new settings keys**: `CineLightRigKeyGobo`, `CineLightRigFillGobo`,
  `CineLightRigRimGobo`, `CineLightRigBgGobo` — Type S32, Persist 1, Value 0,
  Comment "<ROLE> gobo index into the bundled library (0 = default cookie)".
- `readSettings`: 4 cached controls, `setup.mLights[i].mGobo = gobos[i]`
  (the existing per-role array pattern, `:412-432`).
- `writeSetupToSettings` (`:451-466`): `gSavedSettings.setS32(prefix + "Gobo",
  light.mGobo);`.
- `lightToLLSD` (`:201-213`): `data["gobo_idx"] = light.mGobo;
  data["gobo_name"] = goboName(light.mGobo);`.
- `setupFromLLSD` (`:215-264`): read `gobo_idx`; name-fallback via a new
  file-local `goboIndex(name)` mirroring `profileIndex` (`:177-187`) with the
  identical trust-name-on-disagreement rule (`:236-247`) — the forward-compat
  story if the gobo table is ever reordered. Absent keys → 0 (V21/V25).
- `sceneSettingsList()` += the 4 keys (with §A's, 42 → 47 rig keys).
- Version bumps: none (§1.5).

### B.5 UI

Each light block gains one row after its Beam row (before the divider):
label "Gobo" (left 8), combo `control_name="CineLightRig<Role>Gobo"`
(left 52, width 148 — the profile/beam geometry, e.g. `panel_cine_light_rig.
xml:44-45`), reset button at 204 (the paired-row inline-reset idiom,
`:46`). Populated in `postBuild` exactly like profile/beam
(`alpanelcinelightrig.cpp:224-236`):

```cpp
LLComboBox* gobo = getChild<LLComboBox>(widget_prefix + "_gobo");
for (S32 i = 0; i < ALCineLightRigModel::GOBO_COUNT; ++i)
    gobo->add(ALCineLightRigModel::goboName(i), LLSD(i));
gobo->setValue(gSavedSettings.getS32(setting_prefix + "Gobo"));
```

**Consolidated layout delta for the whole delivery** (worked map; ±6 px
implementer freedom, invariants: reset column stays at 322, no width change,
both hosts' scroll containers absorb the growth — the master-design D.1
precedent): 4 × 26 px (gobo rows) + 26 px (§A Warmth row) + 26 px (§E fix-it
row) = **panel height 1130 → 1286**; every row below an insertion shifts by
the cumulative offset. Reset-button roster 29 → 34 (Warmth + 4 gobos); the
§D.3 mechanical audit (every `commit_callback.parameter` greps to a
settings.xml key; count == 34) carries over as a review item.

### B.6 Confirmations the seed asked for

- **Gobos apply to projectors only**: V9/V10/V13 — omnis carry null and
  cannot enter the projector path. CONFIRMED, enforced in the model
  (`EmitterState.mGobo` stays 0 for omnis) and the controller (null forced
  per frame).
- **Index 0 = today's cookie**: `rigGoboTexture(0)` = `rigCookie()` — the
  same UUID, same setting, same live-edit behaviour. A default-valued rig is
  **bitwise identical** in every emitter parameter to today's build.

---

## Section C — the bundled gobo library

### C.1 The set (7 files + the default slot)

Chosen as the minimal set that spans the classic cinematographic break-up
vocabulary — one per distinct shadow-language, no near-duplicates; every seed
candidate is covered:

| idx | Name | File | Pattern spec (all grayscale, white = light) |
|---|---|---|---|
| 0 | Default | — (rigCookie / `CineLightRigCookieUUID`) | today's soft circle |
| 1 | Venetian Blinds | `gobo_blinds.png` | 9 horizontal bright bands, duty cycle ~55 %, edges softened by ~3 px Gaussian; classic noir slats |
| 2 | Window Panes | `gobo_panes.png` | 2×3 bright panes separated by dark mullions (~7 % of width), outer frame dark; soft 2 px edge |
| 3 | Prison Bars | `gobo_bars.png` | 7 dark vertical bars (~12 % duty) over bright field; hard 1 px edge softening |
| 4 | Slats | `gobo_slats.png` | 45° diagonal bright bands, 6 periods, duty ~50 %, 3 px soften |
| 5 | Grid | `gobo_grid.png` | 6×6 bright squares with dark grid lines (~8 %); skylight / eggcrate |
| 6 | Soft Dapple | `gobo_dapple.png` | thresholded low-frequency value noise (fixed seed), heavy blur (σ ≈ 12 px); sun-through-leaves pools |
| 7 | Branches | `gobo_branches.png` | mid-frequency ridged noise (fixed seed) mapped to dark organic strands over a bright field, light blur (σ ≈ 3 px) |

**Common rules, all files:** 512×512, 8-bit single-channel grayscale PNG;
every pattern is multiplied by a **radial vignette** falling smoothstep from
r = 0.72·(size/2) to r = 1.0·(size/2) so the border is fully black — the
projector frustum edge then fades like the shipped soft-circle cookie instead
of slicing a hard bright square (design rule derived from the default
cookie's shape; the projected texture spans the full frustum, so a bright
border would paint the frustum edge). Mean luminance kept within 0.35–0.65 so
switching gobos does not read as an exposure jump (the operator trims EV, not
fights it). File size ~10–60 KB each; total well under 0.5 MB.

Orientation note: the rig's projectors have **no roll control** (aim via
`shortestArc` from −Z, `alcinelightrig.cpp:799-800`), so orientation is baked
into the texture: blinds horizontal, bars vertical, slats diagonal — the three
axis variants exist as separate entries deliberately.

### C.2 Reproducible generation

**New script `scripts/gen_cine_gobos.py`** (Python 3, Pillow + NumPy, no
other deps), committed to the repo; regenerating must be byte-stable:

- One function per pattern (`blinds()`, `panes()`, `bars()`, `slats()`,
  `grid()`, `dapple(seed=0xC1FE)`, `branches(seed=0xB07A)`); noise from
  `numpy.random.default_rng(seed)` with pinned seeds; all parameters (band
  counts, duty cycles, blur sigmas, vignette radii) as named constants at the
  top mirroring the C.1 table.
- Shared post-pass: `vignette()` then quantize to 8-bit and save grayscale
  PNG into `indra/newview/skins/default/textures/cine_gobos/`.
- Header comment states the regeneration command and that outputs are
  committed artifacts (the build does NOT run the script — no build-system
  change; same model as every other committed skin texture).
- Review check: run the script, `git status` shows no diff.

### C.3 Location + packaging

- Files: `indra/newview/skins/default/textures/cine_gobos/*.png`.
- Packaging: **no change** — `viewer_manifest.py:177-183` already globs
  `skins/*/textures/*/*.png` (§1.3.6). PROVES.
- `textures.xml`: **no entries** (§1.6-X1) — the light path does not read it
  and the gobos are not UI images.

### C.4 Fallback matrix (forward-facing rule from the seed)

| Failure | Behaviour | Mechanism |
|---|---|---|
| index out of range (file corruption, future preset) | clamped by `cleanLight`; belt-and-braces guard in `rigGoboTexture` → default cookie | §B.2, §B.3 |
| bundled PNG missing (broken install) | default cookie + one LL_WARNS | §B.3 existence pre-check |
| PNG present but undecodable | texture resolves but never decodes; projector renders with the fetched-texture placeholder behaviour, worst case the pipeline's white bind — un-gobo'd light, not black; ships-with-viewer file, so this is a build defect caught by the in-world pass | V11; §1.3.7 |
| `CineLightRigCookieUUID` malformed (index 0) | existing `rigCookie()` fallback to the compiled default UUID | `alcinelightrig.cpp:139-157`, unchanged |

---

## Section D — gobo master presets

Five masters appended to `app_settings/cine_light_rig_presets.xml`
(26 → 31 file entries; 32 with compiled Classic #0). Same authoring ground
rules as the master-design §C (2-stop headroom ⇒ EV 0 = 0.25; peaks ≤ +1.5;
all four lights fully specified; yaw world-relative; profile indices from
`alcinelightrigmodel.cpp:50-75`; beams 0 Standard / 1 Softbox / 2 Snoot), plus
one new rule: **patterned gobos ride Standard beams** — the gobo spans the
frustum, so Snoot (fov 0.2) compresses the whole pattern into a slash and
Softbox (fov 2.8) stretches it past legibility; Standard (1.5) reads. Snoots
keep gobo 0. Loader work: none — entries parse through the extended
`setupFromLLSD` (§B.4); the existing per-entry validation and duplicate/
decoration guards (`alcinelightrig.cpp:1302-1331`) apply unchanged.

Format per light: `yaw° / pitch° / profile / EV / beam / on / gobo`.

| # | Name | Intent (tooltip) | Key | Fill | Rim | Bg | radius |
|---|---|---|---|---|---|---|---|
| 27 | **Venetian Noir** | Hard white key through venetian blinds; no fill, snoot hair-light. | 75/30/23/+1.0/0/on/**1** | −75/10/23/−4.0/1/off/0 | −120/35/23/−1.0/2/on/0 | 0/−20/5/−3.0/0/off/0 | 1.8 |
| 28 | **Window Light** | Soft daylight through window panes, one high side; cool ambient fill and wash. | 60/40/3/+0.5/0/on/**2** | −50/10/4/−2.0/1/on/0 | −150/35/4/−1.5/1/off/0 | 120/−10/4/−2.0/1/on/0 | 1.9 |
| 29 | **Prison Bars** | Cold hard key striped by bars; white rim, no fill. | 40/45/4/+1.0/0/on/**3** | −40/10/4/−3.0/1/off/0 | 180/40/23/−1.0/2/on/0 | 0/−20/8/−2.5/0/off/0 | 1.7 |
| 30 | **Dappled Forest** | Warm sun pooled through leaves, green bounce, branch-broken cool rim. | 45/55/17/+0.5/0/on/**6** | −45/15/9/−2.0/1/on/0 | −140/35/3/−1.0/0/on/**7** | 150/−10/8/−2.0/1/on/0 | 1.6 |
| 31 | **Skylight Grid** | Neutral overhead through a paned skylight; soft under-fill. | 0/75/2/+0.5/0/on/**5** | 0/−20/2/−2.0/1/on/0 | 180/45/4/−1.5/0/off/0 | 90/−15/4/−2.0/0/off/0 | 1.5 |

All profile indices ∈ [0,23], beams ∈ [0,2], gobos ∈ [0,7], pitches within
±85 — verify by counting at implementation (CLAUDE.md rule; the slice-3
programmatic-transcription review precedent applies to these 5 entries too).

**Scaled-clone reading (inherits every SA guarantee):** within the
proportional regime the projected pattern's footprint on the subject is
scale-invariant — the projector fov is constant and the throw distance scales
with the orbit radius, so footprint diameter ∝ 2·r_eff·tan(fov/2) ∝ r_eff
while the subject height ∝ s ∝ r_eff: **bars-per-face stays constant** from
0.05× to the radius ceiling (INFERENCE from proven frustum geometry + SA-8).
Above the ceiling the pattern coarsens on giants exactly as coverage narrows
— same degradation story as the base presets. Venetian Noir (27) and Prison
Bars (29) are the sharpest in-world probes (hard stripes make any footprint
drift obvious); Dappled Forest (30) doubles as the probe for two distinct
gobos live at once — the first setup ever to drive two different cookies
simultaneously, which exercises V11's per-volume bind directly.

---

## Section E — shadow-policy fix-it button

**Behaviour.** Factor the `requested`/`slots` computation out of
`updateDerivedStatus` (`alpanelcinelightrig.cpp:604-633`) into a private
`S32 computeRequestedShadowSlots() const` used by BOTH the hint and the
button — agreement by construction, per the seed's requirement. Button
handler:

```cpp
void ALPanelCineLightRig::onClickShadowFixIt()
{
    const S32 requested = computeRequestedShadowSlots();
    gSavedSettings.setU32("BDMergeMaxSpotShadows",
        llclamp(static_cast<U32>(requested), 2u, 6u));
}
```

- Write type is **setU32** to match the declared U32 (V17). The existing
  `getS32` read at `:633` is left as-is (works; flagged as a nit, not
  churned).
- The write fires `handleShadowsResized` → live reallocation (V16). No
  pipeline edit — settings write only, exactly the seed's constraint.
- Next `draw()` → `updateDerivedStatus` re-reads slots ⇒ `hint_state`
  leaves 1 ⇒ hint and button hide themselves. Self-clearing, no extra state.
- **Honesty at the cap:** `requested ≤ 4 < 6` (V19) — the "request exceeds 6"
  case is unreachable from the rig, so the button never has to apologise; the
  clamp remains as pure defence and the hint's existing "or use Key only"
  wording is untouched for the impossible case.

**Placement + label.** New button `cine_shadow_fixit` directly under the hint
text (`cine_shadow_hint`, `panel_cine_light_rig.xml:225`), left 8, width 170,
height 22, initially `visible="false"`. Shown iff `hint_state == 1`
(same condition as the hint's state-1 text), with a live label set beside the
hint text update (`alpanelcinelightrig.cpp:641-648`):
`setLabel(llformat("Allow %d spot shadows", llclamp(requested, 2, 6)))`.
Tooltip: "Raises Max Spot Shadows so every requesting rig projector gets a
shadow slot. Each slot renders an extra shadow map per frame." (perf cost
surfaced — it is a global graphics setting). "Reset all" and the panel bottom
shift down 26 px (§B.5 consolidated delta). The button gets **no** reset
button (it is an action, not a value control — D.1 roster rule).

**Non-goals stated:** the button never lowers the setting, never touches
`CineLightRigShadowMode`, and does not attempt to police non-rig projectors
competing for the slots (the auction remains the pipeline's, untouched).

---

## Section F — radius "further away" UX

No new control, no model/clamp change (0.5..512 stays; the 20 m reach cap is
a design input — `llprimitive` untouched). Three concrete edits, all in
`panel_cine_light_rig.xml` + `alpanelcinelightrig.cpp`:

1. **Stepping:** spinner `increment` 0.1 → **0.25** (`panel_cine_light_rig.
   xml:170`). 5 → 20 m becomes 60 clicks instead of 150, while portrait-range
   trims (1.25 → 1.5) remain one click; the spinner keeps
   text entry for jumps and arrow-key auto-repeat for sweeps. Considered and
   rejected: 0.5 (too coarse under 2 m where every portrait preset lives), a
   second coarse spinner (clutter; two controls fighting one setting), a
   slider (0.5..512 linear is dominated by the useless 20..512 tail; a log
   slider is a new widget behaviour — out of proportion for this item).
2. **Reach-cap legibility — passive cue:** in `updateDerivedStatus`
   (`alpanelcinelightrig.cpp:594-671`), read the nominal radius and colour
   the existing "Radius" label amber (the hint colour `1 0.75 0.25 1`,
   matching `cine_shadow_hint`) when
   `radius > ALCineLightRigModel::SCALED_RADIUS_CEIL` (= 20/2.2 ≈ 9.09 —
   the model constant, `alcinelightrigmodel.h:31`, so the UI can never drift
   from the math), restoring the default colour otherwise. The label gets a
   name (`cine_radius_label`) and a cached pointer like the other derived
   widgets. Nominal radius is the correct trigger: for unscaled subjects
   effective == nominal, and for scaled clones SA-8's ceiling already
   handles the geometry — the cue tells the operator their AUTHORED orbit
   has left the fully-compensated regime.
3. **Tooltip truthfulness:** rewrite the spinner tooltip (currently
   `panel_cine_light_rig.xml:170`) to name all three regimes with numbers:
   "Orbit radius. Full brightness compensation up to 9.1 m; from 9.1–20 m the
   light dims as the 20 m projector reach cap bites; past 20 m the subject
   leaves the light's reach entirely. Scale-aware geometry grows with the
   subject until the same cap." The same amber-label tooltip repeats the
   short form. (Reach facts: falloff radius = 2.2 × orbit clamped to 20 m —
   `alcinelightrigmodel.cpp:568`, `llprimitive.h:127` / master design C9;
   contribution is zero past `dist > 1` — `deferredUtil.glsl:808`, master
   design §1.4. PROVES via those citations.)

---

## 5. Model TUT test additions (`alcinelightrigmodel_test.cpp`)

Existing suite: tests 1–12 (`:149-1116`); the frozen golden renderer
`renderScaleOneGolden` (`:66-146`) gains exactly one line —
`projector.mGobo = light.mGobo;` — mirroring §B.2's render change (omni gobo
stays 0 via memset). The entire existing suite continues to pass untouched:
default `mMasterTempMired` = 0 and `mGobo` = 0 make every existing case the
shift-0/gobo-0 regression pin.

The reference implementation for the §A.2 pinned literals (double-precision
Python, committed reasoning): reproduce steps A.2.1–A.2.4 exactly; the table
in §A.2 IS its output. The C++ test pins those numbers as **literals**, never
the model's own symbols (the slice-1 review rule: a co-edit of model + test
must fail loudly).

**`test<13>` — master temperature.** Each sub-assertion with its
discriminating failure:

1. *Bitwise no-op at shift 0:* for a grid of setups (classic; every beam;
   profiles {0, 2, 5, 11, 23}; radii {0.5, 1.5, 9.1, 512}; MasterEV {−4, 0,
   +2}), `render` with `mMasterTempMired = 0` is `memcmp`-identical to
   `renderScaleOneGolden` (which contains no temperature code at all). Fails
   if the gain stage runs at shift 0 (round-trip drift) or reorders anything.
2. *Sanitize pins:* NaN/±inf shift → output memcmp-equal to shift 0; shifts
   −10 000/+10 000 → equal to shifts −110/+150; bounds asserted against the
   literals −110.f/+150.f (not the constants).
3. *Gain literals:* `masterTempGain(+100)` ≈ {1.466342, 1.0, 0.534902},
   `masterTempGain(−50)` ≈ {0.838483, 1.0, 1.338201} (relative 2e-3);
   `gain[1] == 1.f` exactly at both. Fails on any locus/matrix/normalization
   slip.
4. *Composed output literal:* profile 5 (8000K Moon) at shift +100 renders
   `mSR/mSG/mSB` ≈ (0.5373, 0.55, 0.7579) (±2e-3) — pins the full
   sRGB→linear→gain→clamp→sRGB composition, not just the gain.
5. *Relative gel order preserved:* for profiles 0 (warm) and 5 (cool) at
   shifts {−110, −50, +50, +150}: linearized R/B ratio of the warm light
   stays strictly greater than the cool light's; additionally the cool
   light's B channel never drops below the warm light's B. Fails if the gain
   were applied per-light-differently or in sRGB space (a sRGB-space gain
   breaks the ratio invariant measurably at these shift sizes).
6. *Clamp/finiteness:* profile 12 (Congo Blue) and 23 (Pure White) at ±
   extreme shifts: every mSR/mSG/mSB ∈ [0,1] and finite; a channel at 1.0
   saturates rather than wraps (it does NOT stay bit-exactly 1.0 — the clamp
   is in LINEAR space and the value is then re-encoded to sRGB, so the emitted
   channel is linearChannelToSRGB(1.0f) = 0.99999994, one ULP below 1.0. Assert
   this with a tolerance, NEVER exact equality — an exact-1.0 assertion fails
   at runtime, as the first review round found).
7. *Monotonicity:* over ~27 shift samples spanning [−110, +150],
   `gain[0]` strictly increasing, `gain[2]` strictly decreasing. Fails on a
   band-boundary discontinuity in the locus cubics (the T = 4000 seam sits
   inside our range at shift ≈ +100).
8. *Intensity independence:* `mIntensity` and `mClipped` (proj and omni)
   bitwise identical across all shifts for an EV grid including clip-edge
   cases — the exact "temperature leaked into exposure" regression.

**`test<14>` — per-light gobo.** Discriminating assertions:

1. *Clamp:* `cleanLight` with mGobo = −5 → 0; = 99 → 7 (literal, not
   `GOBO_COUNT − 1`); plus `ensure_equals(GOBO_COUNT, 8)` so a table edit
   must consciously touch the test.
2. *Round-trip:* `sanitizeSetup` preserves in-range gobos bitwise
   (memcmp of `Setup` before/after for gobos {0,3,7} across the four lights).
3. *computeLive carries it:* a setup with distinct gobos {1,2,3,4} — live
   lights carry them through orbit/mirror transforms unchanged. Fails on the
   §B.2 field-copy hazard (the copier that silently drops the field).
4. *render passthrough:* `mProj[i].mGobo == live[i].mGobo` for distinct
   values; `mOmni[i].mGobo == 0` always (projector-only rule).
5. *blendLight discrete swap:* start gobo 2, target gobo 5 → 0.49 gives 2,
   0.51 gives 5; endpoints exact (eased ≤ 0 / ≥ 1 paths).
6. *FX default:* `evalFX` output carries gobo 0 for every FX id at several
   times (memset rule pinned — FX must project through the default cookie).
7. *goboName safety:* out-of-range index returns a valid string (clamped),
   matching `profileName` behaviour; name/index table consistency for all 8.

Not TUT-tested, deliberately: `rigGoboTexture` and the texture registry
(viewer-side — LLDir + texture manager; same boundary as the master-library
loader, reviewed via §7-R1 checklist + in-world), the panel button (UI), and
the settings/scene round-trip (V24/V25-cited code paths, review checklist).

---

## 6. File/function checklist (the Codex-brief skeleton)

| File | Change |
|---|---|
| `indra/newview/alcinelightrigmodel.h` | `Globals::mMasterTempMired`; `LightBase::mGobo`; `EmitterState::mGobo`; constants `MASTER_TEMP_MIRED_MIN/MAX`, `MASTER_TEMP_PIVOT_KELVIN`, `GOBO_COUNT`; declare `masterTempGain`, `goboName` |
| `indra/newview/alcinelightrigmodel.cpp` | sanitize clamp for the shift (§A.3); locus/matrix/gamma helpers + `masterTempGain` (§A.2); gain application in `render()` with the shift==0 fast path; the §B.2 field-threading list (cleanLight, copyCleanLights, computeLive, blendLight, render, goboName table) |
| `indra/newview/alcinelightrig.cpp` | `GOBO_FILES` + `rigGoboTexture` registry (§B.3); per-light cookie in `applyFrame` (§B.3); `readSettings`/`writeSetupToSettings` + `lightToLLSD`/`setupFromLLSD` (+`goboIndex` helper) for gobo and temp (§A.4, §B.4); include `llviewertexture.h`; `sameLight` gains `mGobo` (transition trigger, `:110-118`) |
| `indra/newview/alpanelcinelightrig.cpp/.h` | gobo combo population (§B.5); `computeRequestedShadowSlots` factor-out + `onClickShadowFixIt` + fix-it visibility/label in `updateDerivedStatus` (§E); radius label amber cue (§F.2); cached pointers for the new widgets |
| `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml` | 4 gobo rows, Warmth row, fix-it button, radius increment 0.25 + tooltips, reflow (§B.5 delta; height 1130 → 1286); 5 new reset buttons (roster 34) |
| `indra/newview/app_settings/settings.xml` | 5 new keys: `CineLightRigMasterTempMired` (F32 0), `CineLightRig{Key,Fill,Rim,Bg}Gobo` (S32 0) |
| `indra/newview/llfloaterdirector.cpp` | `sceneSettingsList()` += the 5 keys |
| `indra/newview/app_settings/cine_light_rig_presets.xml` | append the 5 §D entries (26 → 31) |
| `indra/newview/skins/default/textures/cine_gobos/*.png` | NEW — 7 generated gobos (§C) |
| `scripts/gen_cine_gobos.py` | NEW — deterministic generator (§C.2) |
| `indra/newview/tests/alcinelightrigmodel_test.cpp` | golden gains `mGobo` line; `test<13>`, `test<14>` (§5) |

Not touched: CMake (no new C++ sources; PNGs/script are not build inputs),
`viewer_manifest.py`, `textures.xml`, notifications.xml, scene/preset version
numbers, `applySceneData`'s three-way split (only the `base` payload grew),
`masterSetups()` loader (new entries parse through the extended
`setupFromLLSD` with zero loader change).

## 7. Risks, ranked

**Render-state call-out first, per the brief:** this delivery contains **zero
edits to pipeline.cpp, any shader, llprimitive, llvovolume, or the texture
system**. Per-light gobos ride the EXISTING per-object light-image parameter
that every world spotlight already uses, and the pipeline already binds that
parameter per volume per draw (V11) — this is precisely what keeps the
feature low-risk, and it is confirmed, not assumed (§1.2/§1.3). The beauty
pass / hero path binds the same per-object texture at its own site
(`pipeline.cpp:17769`) and therefore inherits per-light gobos with no work.
The items below marked ⚠ are the only ones even ADJACENT to shared render
state, and both are additive value-level effects.

- **R1 ⚠ Texture-registry residency** — 7 file-backed textures registered in
  the shared `gTextureList` (same API and list as parcel/script textures).
  Additive; keyed by path-hash UUIDs that cannot collide with grid assets in
  practice (`generate(url)` namespace). Failure mode if a texture is evicted
  or never decodes: the pipeline's white bind (V11) — visible as an
  un-gobo'd light, self-healing, never black. Review: confirm the held
  `LLPointer` + per-frame stats keep the decode resident across a long
  session and a texture-memory squeeze; in-world: toggle every gobo on every
  light, teleport across regions, relog.
- **R2 ⚠ Per-light cookie churn** — `applyFrame` now computes the target
  UUID per light. Steady state is unchanged (two no-op guards, V10 +
  `llvovolume.cpp:3144`); a gobo EDIT retargets one light's texture, which
  restarts that projector's texture stats — worst case a one-frame blur on
  that light while the new cookie decodes (first use per session). Not a
  regression class: editing `CineLightRigCookieUUID` does this today for all
  four.
- **R3 Colour-math defect** — wrong tint from a locus/matrix slip. Pure
  model, no render state; TUT pins literals, composition, order, and
  monotonicity across the band seam (§5-13.7). Operator kill-switch = slider
  to 0, which is the bitwise no-op path. In-world: A/B shift 0 vs ±50 on
  Classic and on Neon Crossfire (saturated gels — the clamp-compression
  case).
- **R4 Struct-field threading** — `mGobo` must be added to two field-by-field
  copiers (`copyCleanLights`, `computeLive`) or gobos silently never render.
  Called out in §B.2 as a checklist; `test<14>.3/.4` are the tripwires.
- **R5 Preset/scene forward-compat** — additive optional fields; matrix in
  §1.5. Review must load: an old local preset, an old scene (expect gobo
  reset via V25 + live temp untouched via V24 — both by design), a new
  preset in a simulated old `setupFromLLSD` (unknown keys ignored). No
  version gates changed anywhere — verify by grep.
- **R6 Shadow button writes a global graphics setting** — deliberate,
  explicit, clamped [2,6], perf cost stated in the tooltip, reversible in
  prefs; the hint/button pair cannot disagree (shared helper). Residual: a
  user on weak hardware clicks it during a heavy scene — accepted; it is the
  documented purpose of the control the button writes.
- **R7 Gobo art quality** (guess-flagged) — duty cycles, blur radii, and the
  0.72 vignette knee are authored guesses; the generation script makes the
  in-world tuning loop cheap (edit constant, re-run, rebuild nothing). The
  mean-luminance 0.35–0.65 rule bounds the exposure-jump risk.
- **R8 Old-build scenes leak the live master temp** (V24 absent-key
  semantics) — documented, precedent-consistent, self-limiting (new saves
  carry the key). Rejected fix: forcing 0 on absent key would break the
  settings-map contract every other key obeys.
- **R9 Panel reflow** — XUI-only; the mechanical audit (34 reset parameters,
  both hosts, no overlap by rect arithmetic) carries over from the master
  design's D.3 checklist.

## 8. Deferred (decided now, with reasons)

1. **Per-light custom gobo UUIDs** — index 0 + `CineLightRigCookieUUID`
   covers the single-custom case today; a per-light UUID field would need 4
   more settings keys and UI real estate for a niche need. Revisit on artist
   demand; the schema extends the same way gobo_idx did.
2. **Gobo picker thumbnails / textures.xml entries** — pure UI sugar; would
   re-open X1. After the slate survives in-world curation.
3. **Gobo rotation/scale controls** — the projector has no roll channel
   (§C.1 orientation note); adding one touches aim math frozen as a final.
   The axis-variant textures (blinds/bars/slats) cover the practical cases.
4. **User-imported gobos (Local Bitmaps integration)** — real feature, real
   scope; the index architecture accommodates it later as appended entries.
5. **Animated gobos / cookie sequencing** — FX-system territory; the FX
   gobo-0 rule (§B.2) keeps the door open without committing.
6. **Gizmo drag-editing** — explicitly excluded by the user from this
   delivery (seed header). Unchanged: v1 gizmo remains display-only.
7. Carried from earlier deliveries, unchanged: N>4 lights, hotkeys,
   renderer >1.0 intensity headroom, projvol/shadow decoupling
   (`doc/MACHINIMA_FEATURE_BACKLOG.md`).

## 9. OFF-LIMITS for the implementation brief

Everything not named in §6 is off-limits. Explicitly, even where adjacent:
**`pipeline.cpp` in its entirety** (V11 is consumed, not touched; the nine
gate exemptions and the shadow auction are finals), **all GLSL**,
**`indra/llprimitive/*`** (the [0,1] colour clamp and 20 m radius clamp are
design inputs the maths above DERIVE from), **`llvovolume.cpp` /
`llviewertexture.*` / `llviewertexturelist.*`** (read-only consumers — the
§1.3 chain works because these are NOT modified), `llmath.h` / `v3color.h`
(the model copies the two gamma formulas by value, by rule),
`llvoavatar.* / llghostavatar.* / llactormover.cpp / llcinematiccamera.cpp`
(read-only precedents), `lldirectorcast.*`, `llselectmgr.*`,
`bdmerge_should_render_*` bodies, the `applySceneData` three-way split
semantics (`alcinelightrig.cpp:1583-1620` — only the `base` payload contents
grew), the scale-aware and mirror code (finals — touched ONLY where §6 names
a legitimate new-field thread), `viewer_manifest.py`, `textures.xml`, and
**both version numbers** (scene `light_rig` v1, preset/library v1 — §1.5
decides no bump, so any `version` edit in the diff is a defect).
