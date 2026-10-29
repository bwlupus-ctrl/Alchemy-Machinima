# Cine Light Rig — Easy Mode ("Lighting for Dummies")

## Problem
Exposure today is fiddly: the user hand-sets four per-light **EV** sliders
(Key/Fill/Rim/Bg, -20..+20), a **Ratio lock** (Fill = Key - ratioStops, 0..5),
plus a **Master EV** (-16..+16) and **Headroom**. Getting a balanced look means
juggling all of them, and the Ratio knob only moves Fill *relative to Key*, so
adding "drama" also darkens the whole frame and you chase it back with Master EV.

## Goal
A beginner surface of a few global dials that "just work", with the existing
per-light controls preserved but folded away (matches the earlier panel reorder:
"the avg user is not going to touch the light stuff"). Presets re-tuned so a
preset + two dials reproduces the intended look.

## Chosen control surface (user decision)
**Brightness + Drama + Rim + BG** (+ Warmth). Advanced per-light EV sliders stay,
folded below.

## Key idea — Easy Mode is a UI MACRO over existing settings (no new render math)
The four easy dials both **read** (derive their handle position from the current
`CineLightRig*` settings) and **write** (a small deterministic macro back into
those same settings). The render pipeline and the pure exposure model are
UNCHANGED. This means: no new scene-key/blob layout, no version bump, no
migration, no new render path — only UI + a couple of pure helper functions for
the mapping (so it is unit-testable) + one bool setting to remember panel choice.

### The dials → settings mapping (normative)

Let key be light 0, fill 1, rim 2, bg 3.

- **Brightness** (EV, range -8..+8, default 0)
  - WRITE: `CineLightRigMasterEV = Brightness`, and force `CineLightRigKeyEV = 0`
    (Key is the anchor; all "how bright is my subject" lives in Master).
    Also force `CineLightRigKeyOn = true`.
  - READ (derive handle): `Brightness = clamp(MasterEV, -8, +8)`.

- **Drama** (stops, range 0..5, default 2.0)
  - WRITE: `CineLightRigRatioLock = true`, `CineLightRigRatio = Drama`.
    (Fill EV is then derived by the existing model as Key - Drama = -Drama, so the
    subject/Key exposure is INDEPENDENT of Drama — turning up Drama only deepens
    the shadow side. This is the whole point.)
    Also force `CineLightRigFillOn = true`.
  - READ: `Drama = RatioLock ? clamp(Ratio,0,5) : clamp(KeyEV - FillEV, 0, 5)`.
  - Semantics: 0 = flat / high-key (fill == key); 2 ~ natural 4:1; 5 = noir.

- **Rim presence** (enum 0 Off / 1 Subtle / 2 Strong, default 1)
  - WRITE: Off -> `RimOn=false`. Subtle -> `RimOn=true, RimEV = -1.0`.
    Strong -> `RimOn=true, RimEV = +0.5`. (EV is relative to Key@0, so it tracks
    Brightness automatically.)
  - READ (bucket): `!RimOn -> Off; RimEV < -0.25 -> Subtle; else Strong`.

- **BG presence** (enum 0 Off / 1 Subtle / 2 Strong, default 1)
  - WRITE: Off -> `BgOn=false`. Subtle -> `BgOn=true, BgEV = -2.0`.
    Strong -> `BgOn=true, BgEV = -0.5`.
  - READ (bucket): `!BgOn -> Off; BgEV < -1.25 -> Subtle; else Strong`.

- **Warmth**: reuse existing `CineLightRigMasterTempMired` unchanged; just surface
  it in the Easy block.

### Pure helpers (unit-testable, live in ALCineLightRigModel)
Add small pure functions so the mapping is tested and not buried in UI:
- `F32 easyRimEV(S32 presence)`, `F32 easyBgEV(S32 presence)`,
  `bool easyRimOn(S32 presence)`, `bool easyBgOn(S32 presence)`
- `S32 rimPresenceFromEV(bool on, F32 ev)`, `S32 bgPresenceFromEV(bool on, F32 ev)`
These are the single source of truth for both the WRITE and READ directions and
must round-trip: `rimPresenceFromEV(easyRimOn(p), easyRimEV(p)) == p` for p in
{0,1,2}; same for bg. Bucket thresholds above are chosen so this holds.

## Persistence
- ONE new setting: `CineLightRigEasyMode` (bool, default **true**) — remembers
  which panel the user prefers (Easy vs Advanced). It does NOT gate render; it
  only chooses which block is expanded and whether the Advanced EV/Ratio sliders
  are greyed. Because it is only a UI preference, it may stay a plain global
  setting (does not need to be per-instance in the blob). If trivial, mirror into
  the blob too for consistency, but not required.
- No other new keys. No blob layout change. No version bump. No migration.

## Panel behaviour (skins + alpanelcinelightrig)
- Add an **Easy** block at the very TOP of `panel_cine_light_rig.xml`:
  Brightness slider, Drama slider, Rim presence (combo/radio 3-way), BG presence
  (3-way), Warmth slider (bound to MasterTempMired), and an **Easy / Advanced**
  toggle (bound to `CineLightRigEasyMode`).
- Moving any Easy dial runs the WRITE macro above (writes into the existing
  `CineLightRig*` controls, which already drive the live buffer + blob + render).
- On panel refresh, derive each Easy dial's displayed value via the READ mapping
  from the current settings (so switching selected rig / loading a preset updates
  the Easy handles correctly).
- While Easy Mode is ON: grey/disable the Advanced **EV** sliders (Key/Fill/Rim/Bg
  EV), the **Ratio** controls, and **Master EV** — they are driven. Everything
  else (angles, profiles, beams, gobos, gels, headroom, bounce, seed, FX) stays
  fully live. Toggling to Advanced re-enables them starting from current values.
- Keep the panel within its existing rect; reflow rows, do not grow the floater
  beyond bounds. Watch the XML `--` comment gotcha (no double-hyphen in comments).

## Presets — re-evaluate ALL (Fable authors)
Today presets bake exposure into per-light EV (e.g. keyEV=1.0, fillEV=-2.5) AND a
separate masterEV. For the Easy model to read them cleanly they must be
**normalized to easy-native form** WITHOUT changing the visible look:
1. Anchor **Key EV = 0** and move the preset's overall exposure into `master_ev`
   (add a `master_ev` field to each preset map; the loader already reads globals,
   confirm it applies master_ev on preset load — if not, wire it).
2. Set `ratio_lock = true` and `ratio_stops = Drama`, where Drama reproduces the
   old Key-minus-Fill spread. Fill EV then derives; keep an explicit `ev` on Fill
   only as a fallback for Advanced display.
3. Map Rim/Bg to the **presence buckets** (choose Off/Subtle/Strong so RimEV/BgEV
   land in the bucket ranges above), preserving whether they were on and roughly
   how strong.
4. **Geometry is identity — do NOT touch** yaw/pitch/profile/profile_name/beam/
   gobo/gel. Only exposure (key/master/ratio/rim-bg presence) is normalized.
5. Each preset should therefore round-trip: loading it, the Easy dials read back
   sensible Brightness/Drama/Rim/BG, and nudging them behaves intuitively.
Deliverable: the full rewritten `cine_light_rig_presets.xml` (every preset,
including the 16 FX-companion / Genre-Mood ones and Rock With You), byte-complete,
same schema + the added `master_ev` field, geometry byte-identical to today.

## Tests
- Pure round-trip tests for the presence helpers (both directions, p in 0..2).
- Brightness/Drama read-write identity (Brightness->MasterEV->Brightness;
  Drama->Ratio->Drama within clamp).
- Confirm no change to existing model/FX golden tests (Easy Mode adds helpers
  only; it does not alter evalFX, render, or intensityFromEV).

## Explicitly OUT of scope for v1
- Auto-exposure metering, per-light softness changes, crossfade, LUTs.
- Any change to headroom/bounce/FX behaviour.

---

# Revision 2 — resolutions from adversarial review + preset-fit pass

This section SUPERSEDES the earlier specifics where they conflict. The core idea
(Easy Mode = macro over existing settings, no new render math) stands, but the
review found the globals are not serialized with setups today, so real (additive,
back-compat) plumbing is required.

## R2.1 Presence is 4-way: Off / Faint / Subtle / Strong
The 3-way buckets were too coarse (noir accents below the Subtle floor came up
too bright; blown-halo rims capped too dim). Adopt 4 buckets. WRITE values and
READ thresholds (thresholds are exact midpoints, so p in {0,1,2,3} round-trips):

- **Rim** WRITE: Off -> RimOn=false; Faint -> on, RimEV = -2.5; Subtle -> on,
  RimEV = -1.0; Strong -> on, RimEV = +0.5.
  READ: `!RimOn -> Off; RimEV < -1.75 -> Faint; RimEV < -0.25 -> Subtle;
  else Strong`.
- **BG** WRITE: Off -> BgOn=false; Faint -> on, BgEV = -3.5; Subtle -> on,
  BgEV = -2.0; Strong -> on, BgEV = -0.5.
  READ: `!BgOn -> Off; BgEV < -2.75 -> Faint; BgEV < -1.25 -> Subtle;
  else Strong`.

Round-trip invariant to unit-test: for p in {0,1,2,3},
`rimPresenceFromEV(easyRimOn(p), easyRimEV(p)) == p` and the bg equivalent.
Two presets (Silhouette=Key-off, Sci-Fi Rim=dark-centre) remain Advanced-only by
design; do not try to force them into Easy.

## R2.2 Brightness range = +/-16 (match MasterEV), not +/-8
Prevents handle clamp + write-back drift for any setup with |MasterEV| > 8.
READ: `Brightness = clamp(MasterEV, -16, +16)`. Default 0.

## R2.3 P1 — serialize globals with setups (the real work)
Today `setupFromLLSD`/`setupToLLSD`/save/scene handle `Setup` only; MasterEV and
MasterTempMired are `Globals` and are never persisted with a setup/preset/scene.
Add them, additively and back-compat:
- Extend the setup LLSD schema with optional `master_ev` and `master_temp_mired`.
- WRITE side (`setupToLLSD`, saveSetup, director scene `data["base"]`): include
  both from current globals.
- READ side (`setupFromLLSD` and every load path: bundled preset ~2084, file
  preset ~2274, scene base ~2440): **only apply if the key is present** —
  `if (data.has("master_ev")) gSavedSettings.setF32("CineLightRigMasterEV", ...)`
  — otherwise LEAVE `CineLightRigMasterEV` untouched. Same for master_temp_mired.
  Acceptance: an OLD preset/setup/scene lacking these keys loads byte-identically
  to today (no exposure/temperature change); a NEW normalized preset applies its
  master. This is the single most important correctness rule of the feature.
- `MasterSetup` (in-memory) must gain the two global fields so the round-trip
  (save -> reload) preserves Brightness/Warmth; otherwise user "Save Setup" drops
  them.

## R2.4 P2 — normalize-on-enter (Key EV = 0 invariant)
Anchoring is currently asserted only on a Brightness nudge. When Easy Mode is
entered (toggle on, and on first show if already on), run a one-shot normalize on
the SELECTED instance's live settings:
`MasterEV += KeyEV; KeyEV = 0; KeyOn = true;` (preserves subject exposure exactly,
since render uses KeyEV+MasterEV). Then the Brightness handle reads true and the
Rim/Bg "relative to Key@0" framing holds. Do NOT auto-touch Rim/Bg EV here (only
bucket them through the presence combos when the user sets them, or when loading a
normalized preset). Re-run the READ derivation on every instance selection change.

## R2.5 Default + migration (don't destroy power-user looks)
`CineLightRigEasyMode` default **true**, BUT on panel construction detect whether
the selected instance is "easy-native": KeyEV within ~0.01 of 0 AND (RimOff or
RimEV at a bucket value) AND (BgOff or BgEV at a bucket value). If NOT easy-native
(i.e. a hand-tuned Advanced look, or an un-normalized old preset that has not been
loaded through the new loader), open in **Advanced** for that instance so nothing
is silently overwritten. Fresh profiles (all defaults, KeyEV=0) open in Easy.
Never run the R2.4 normalize implicitly on a non-easy-native instance without the
user choosing Easy.

## R2.6 Per-instance correctness (must verify in impl)
Easy dials read/write the SELECTED instance. The WRITE macro sets `gSavedSettings`
`CineLightRig*` keys; the existing live-edit path already commits those to the
selected instance's blob (MasterEV/MasterTempMired exist as blob fields too).
Codex MUST route the Easy writes through that same commit path (they are existing
keys, so they already do) and MUST re-derive the Easy READ on instance switch.
Add a targeted check that a Brightness/Warmth change on instance B does not bleed
to instance A.

## R2.7 Dead-control notes (accept for v1; document in UI)
- Fill-fully-off and arbitrary Rim/Bg EVs are not expressible in Easy; that is
  what Advanced is for. The Easy block should carry a one-line "for fine control,
  switch to Advanced" hint.
- Drama sets `RatioLock=true`. When the user switches to Advanced, leave the lock
  as-is but ensure the Advanced Ratio-lock checkbox reflects it (it already does),
  so the greyed-then-inert FillEV is explained by the visible lock state.
- Drama READ clamps negative spreads (Fill brighter than Key) to 0; such backlit
  presets are Advanced-native — acceptable.

## R2.8 Preset re-authoring — final
Use `doc/PRESET_EASY_NORMALIZATION.md` (Fable) as the source of master_ev + drama
per preset, remapped to the 4-way presence buckets in R2.1 (the "below Subtle
floor" accents flagged there become **Faint**; blown rims stay **Strong**). Add
`master_ev` (and, where a preset intends warmth, `master_temp_mired`) to each
preset map; anchor Key EV=0; set `ratio_lock=true` + `ratio_stops=drama`; set
Rim/Bg `on`+`ev` to the chosen bucket's exact WRITE value. Geometry stays
byte-identical. Silhouette and Sci-Fi Rim keep their authored (Advanced) EVs.
