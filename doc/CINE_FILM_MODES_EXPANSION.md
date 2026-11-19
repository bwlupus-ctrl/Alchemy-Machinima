# Cinematic Light Rig — Film Modes Expansion (Pack A presets + Pack B FX)

**Status:** design spec, ready for implementation. No source is modified by this document.
**Scope:** adds **15 new film-look presets** (Pack A) and **14 new animated FX** (Pack B, ids 49–62) to the Cinematic Light Rig. Everything fits the existing `evalFX` / preset-XML systems — **no schema change, no new model capability.** This doc is the successor to `doc/CINE_FX_PRESETS_EXPANSION.md` (the 33→49 expansion) and follows its structure and normative conventions; read that doc's Part A for the full contract. Only the deltas that matter for this pack are restated below.

---

## Part A — Contract deltas since the last expansion

1. **FX ids now start at 49.** `FX_COUNT` is currently **49** (`indra/newview/alcinelightrigmodel.h:22`); FX 0–48 are implemented (`alcinelightrigmodel.cpp:119-144` names/intervals, `initializeFX` cases through 48 at `cpp:459-536`, `evalFX` cases through 48 at `cpp:1078-1868`). This pack takes `FX_COUNT` to **63**.
2. **Pure helper vocabulary (unchanged, exclusive):** `phaseSin`, `phaseCos` (radian phase, fmod-safe), `unitHash(seed, fx, counter, light, draw)`, `valueNoise(seed, fx, coordinate, light, draw)`, `positiveFmod`, `positiveMod`, `virtualStep`, `wrap180`, `std::pow`, plain arithmetic and `static const` lookup tables (the FX-38 `yaws[]` / FX-45 `SOS_MASK` idiom). No state, no RNG, no time source except the passed `t`. Same `t` ⇒ same output (scrub-safe; enforced by test 5).
3. **Seed discipline:** the golden test's `random_fx[]` list is currently `{1,2,5,6,9,11,13,14,17,20,25,27,30,31,33,34,35,37,41,42,43,44,48}` (`indra/newview/tests/alcinelightrigmodel_test.cpp:732-735`). Every new FX below that calls `unitHash`/`valueNoise` must be appended; every analytic FX must stay seed-invariant.
4. **FX may set `mGobo`, must NEVER set `mGel`** (operator gel is re-layered on top of FX output, `alcinelightrig.cpp:1806-1811`). Colour animates only by stepping `mProfile` (0–23). EV −10 is the idiomatic "black/off"; practical FX range −10..+3.
5. **Presets are now EASY-NATIVE** (per `doc/CINE_EASY_LOOK_DESIGN.md` Revision 2 and `doc/PRESET_EASY_NORMALIZATION.md`, both already shipped — the bundled XML already carries `master_ev`, Key EV 0, ratio locks). Every Pack A preset below obeys:
   * **Key (light 0):** `ev = 0.0`, `on = true`. All overall exposure lives in the preset-level `master_ev` field (serialized via `optionalSetupGlobalsFromLLSD`, applied on load).
   * **`ratio_lock = true`, `ratio_stops = Drama`** — the key-to-fill spread in stops (0..5). Fill's rendered EV is **derived** (`0 − Drama`); the Fill row's stored `ev` below is exactly `−Drama`, kept only as the Advanced-mode display fallback. A few void looks ship Fill `on=false` (the Film Noir idiom); the lock + `ratio_stops` still record the intended Drama for when the user enters Easy Mode.
   * **Rim (light 2)** on-EV is one of the 4-way presence buckets: **Faint −2.5 / Subtle −1.0 / Strong +0.5**, or `on=false` (stored fallback EV noted).
   * **BG (light 3)** on-EV: **Faint −3.5 / Subtle −2.0 / Strong −0.5**, or `on=false`.
   * Geometry/colour uses only the real tables: profiles 0–23, beams 0–2 (Standard/Softbox/Snoot), gobos 0–7 (`Default, Venetian Blinds, Window Panes, Prison Bars, Slats, Grid, Soft Dapple, Branches`), gels 0–14. Presets MAY use gels.
6. **Category:** the loader special-cases exactly one category string, `"Genre / Mood"` (`alcinelightrig.cpp:2145-2146` — it is a bool; any other string silently falls into the Built-in section). All Pack A entries therefore ship `category = "Genre / Mood"` so they group on the mood/theatrical shelf with zero code change. *(Optional, explicitly out of scope: promoting the bool to a string and adding a third "— Cinema —" combo caption would give these their own section; nothing in this doc depends on it.)*

---

## Part B — PACK A: 15 film-look presets

All entries append to the `presets` array of `indra/newview/app_settings/cine_light_rig_presets.xml` in the exact LLSD shape of the existing normalized entries (e.g. Rembrandt at `:7-20`: `name`, `category`, `intent`, `radius`, `master_ev`, `ratio_lock`, `ratio_stops`, `lights[4]` each with `yaw`, `pitch`, `profile_idx`+`profile_name`, `ev`, `beam_idx`+`beam_name`, `on`, optional `gobo_idx`+`gobo_name`, `gel_idx`+`gel_name`). Lights are listed **KEY / FILL / RIM / BG**. Omitted gobo/gel = index 0. Names verified unique against every shipped preset (§A.4 of the prior doc plus its 15 additions). All 15 carry `category = "Genre / Mood"`.

Bucket legend used below — Rim: Faint −2.5 / Subtle −1.0 / Strong +0.5; BG: Faint −3.5 / Subtle −2.0 / Strong −0.5.

### A.1 Don's Study
**Film:** *The Godfather* (1972) — DP **Gordon Willis** ("the Prince of Darkness"). **Technique:** overhead top-key from a practical so the brow shadows the eye sockets; warm low-wattage tungsten; deliberate underexposure; the office swallowed in amber dark.
**Intent:** "Top-key drops the eyes into shadow; warm amber gloom — the offer you can't refuse."
`radius 1.4`, `master_ev -0.5`, `ratio_lock true`, `ratio_stops 4.5`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 5 | 68 | 1 "3200K Tung" | 0.0 | 0 "Standard" | true | — | 2 "1/2 CTO" |
| FILL | −30 | 0 | 0 "2700K Incan" | −4.5 | 1 "Softbox" | true | — | — |
| RIM | −150 | 35 | 1 "3200K Tung" | −2.5 | 2 "Snoot" | false | — | — |
| BG | 165 | −10 | 13 "Deep Amber" | −3.5 | 0 "Standard" | true | — | — |
Rim **Off** (Willis used none — fallback −2.5); BG **Faint**.

### A.2 Duel by Candlelight
**Film:** *Barry Lyndon* (1975) — DP **John Alcott** (with Kubrick's NASA f/0.7 Zeiss lenses). **Technique:** scenes lit by actual candle clusters; very soft, wrapping, low-contrast warm light falling off into brown-black rooms.
**Intent:** "A table of candles just below the face; soft warm wrap, period gloom beyond."
`radius 1.1`, `master_ev -0.25`, `ratio_lock true`, `ratio_stops 1.25`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 15 | −12 | 0 "2700K Incan" | 0.0 | 1 "Softbox" | true | — | — |
| FILL | −40 | −8 | 0 "2700K Incan" | −1.25 | 1 "Softbox" | true | — | — |
| RIM | −155 | 30 | 1 "3200K Tung" | −2.5 | 2 "Snoot" | false | — | — |
| BG | 170 | −5 | 13 "Deep Amber" | −2.0 | 1 "Softbox" | true | — | — |
Rim **Off**; BG **Subtle**. Pairs with FX 35 Candle Draft.

### A.3 Velvet Coven
**Film:** *Suspiria* (1977) — DP **Luciano Tovoli**. **Technique:** Technicolor three-strip homage — saturated primary red, blue and green thrown through velvet drapes and stained glass, each colour owning its own direction, contrast kept low so every primary reads.
**Intent:** "Red key, blue fill, green edge, magenta wall — the ballet school as a nightmare."
`radius 1.6`, `master_ev 0.25`, `ratio_lock true`, `ratio_stops 1.0`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 40 | 35 | 11 "Rosco Red" | 0.0 | 0 "Standard" | true | — | — |
| FILL | −50 | 10 | 12 "Congo Blue" | −1.0 | 1 "Softbox" | true | — | — |
| RIM | −140 | 40 | 14 "Emerald" | +0.5 | 2 "Snoot" | true | — | — |
| BG | 160 | 0 | 10 "Magenta" | −2.0 | 0 "Standard" | true | — | — |
Rim **Strong**; BG **Subtle**.

### A.4 Graphic Noir
**Film:** *Sin City* (2005) — dir./DP **Robert Rodriguez**. **Technique:** digital-backlot hard black-and-white: pure white hard sources, razor shadow edges, black voids with a single blown highlight rim doing the drawing.
**Intent:** "Ink-hard B&W — one hot white slash, one blown edge, nothing else survives."
`radius 1.5`, `master_ev 0.5`, `ratio_lock true`, `ratio_stops 5.0`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 75 | 50 | 23 "Pure White" | 0.0 | 0 "Standard" | true | — | — |
| FILL | −35 | 5 | 23 "Pure White" | −5.0 | 1 "Softbox" | false | — | — |
| RIM | −130 | 35 | 23 "Pure White" | +0.5 | 2 "Snoot" | true | — | — |
| BG | 175 | −15 | 5 "8000K Moon" | −3.5 | 0 "Standard" | false | — | — |
Fill **off** (void; fallback −5.0); Rim **Strong**; BG **Off**.

### A.5 Pod Bay
**Film:** *2001: A Space Odyssey* (1968) — DP **Geoffrey Unsworth**. **Technique:** shadowless clinical white from bounced/practical panels (the set itself is the source), with HAL's red lens as the sole colour accent behind the subject.
**Intent:** "Sterile shadowless white; a single red eye watching from behind."
`radius 1.7`, `master_ev 0.25`, `ratio_lock true`, `ratio_stops 0.5`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 0 | 45 | 23 "Pure White" | 0.0 | 1 "Softbox" | true | — | — |
| FILL | 0 | −20 | 23 "Pure White" | −0.5 | 1 "Softbox" | true | — | — |
| RIM | −160 | 30 | 4 "6500K Cool" | −2.5 | 2 "Snoot" | false | — | — |
| BG | 175 | 5 | 11 "Rosco Red" | −0.5 | 2 "Snoot" | true | — | — |
Rim **Off**; BG **Strong** (the snoot keeps HAL's eye a tight hot dot, not a wash).

### A.6 Mood for Love
**Film:** *In the Mood for Love* (2000) — DP **Christopher Doyle** (with Mark Lee Ping-bing). **Technique:** saturated warm tungsten through smoke in cramped corridors, contrasted with sickly green fluorescent spill; frames within frames, colour carrying the longing.
**Intent:** "Warm tungsten noir brushed with green fluorescence — Hong Kong corridors at midnight."
`radius 1.3`, `master_ev 0.0`, `ratio_lock true`, `ratio_stops 3.0`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 55 | 30 | 1 "3200K Tung" | 0.0 | 0 "Standard" | true | — | 2 "1/2 CTO" |
| FILL | −35 | 5 | 9 "Plus Green" | −3.0 | 1 "Softbox" | true | — | — |
| RIM | −150 | 35 | 13 "Deep Amber" | −1.0 | 2 "Snoot" | true | — | — |
| BG | 165 | −8 | 9 "Plus Green" | −2.0 | 0 "Standard" | true | — | — |
Rim **Subtle**; BG **Subtle**.

### A.7 Sick Green Room
**Film:** *Joker* (2019) — DP **Lawrence Sher**. **Technique:** decaying institutional Gotham — green-contaminated fluorescents overhead against sodium-vapor amber contamination; the grid of a failing troffer written on the subject.
**Intent:** "Failing fluorescent grid overhead, sodium amber seeping in — the city that gave up."
`radius 1.5`, `master_ev -0.25`, `ratio_lock true`, `ratio_stops 2.5`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 0 | 72 | 9 "Plus Green" | 0.0 | 1 "Softbox" | true | 5 "Grid" | — |
| FILL | −25 | −5 | 13 "Deep Amber" | −2.5 | 1 "Softbox" | true | — | — |
| RIM | 170 | 30 | 9 "Plus Green" | −2.5 | 0 "Standard" | true | — | — |
| BG | 150 | −10 | 13 "Deep Amber" | −2.0 | 0 "Standard" | true | — | — |
Rim **Faint**; BG **Subtle**. **Gobo:** Grid on the key = the fluorescent troffer. Pairs with FX 25 Elevator Fault.

### A.8 2019 Blinds
**Film:** *Blade Runner* (1982) — DP **Jordan Cronenweth**. **Technique:** hard shafts through venetian blinds into smoke, cool ambient key, roving exterior searchlights, neon bleeding in from the street; eyes picked out by moving slatted light.
**Intent:** "Slatted light through smoke, pink neon bleed, deep blue city night — Deckard's apartment."
`radius 1.5`, `master_ev 0.0`, `ratio_lock true`, `ratio_stops 3.5`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 65 | 42 | 4 "6500K Cool" | 0.0 | 0 "Standard" | true | 1 "Venetian Blinds" | — |
| FILL | −30 | 5 | 6 "10000K Sky" | −3.5 | 1 "Softbox" | true | — | — |
| RIM | −145 | 30 | 15 "Cyber Pink" | −1.0 | 2 "Snoot" | true | — | — |
| BG | 175 | −12 | 20 "Deep Space" | −3.5 | 0 "Standard" | true | — | — |
Rim **Subtle**; BG **Faint**. **Gobo:** Venetian Blinds on the key. Distinct from the shipped *Venetian Noir* (warm hard noir): this is the cool/neon smoke variant. Pairs with FX 15 Searchlight for the roving spinner beams.

### A.9 The Grid
**Film:** *TRON: Legacy* (2010) — DP **Claudio Miranda**. **Technique:** the costumes and set ARE the lights — electroluminescent cyan edge glow on black, with Clu's warm orange as the opposing accent; almost no conventional key.
**Intent:** "EL cyan glow out of blackness, one hostile orange edge — you're on the Grid."
`radius 1.8`, `master_ev -0.25`, `ratio_lock true`, `ratio_stops 3.0`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 10 | 15 | 16 "Sci-Fi Cyan" | 0.0 | 1 "Softbox" | true | — | — |
| FILL | −45 | 0 | 12 "Congo Blue" | −3.0 | 1 "Softbox" | true | — | — |
| RIM | 160 | 25 | 7 "Full CTO" | +0.5 | 2 "Snoot" | true | — | — |
| BG | −170 | −10 | 16 "Sci-Fi Cyan" | −3.5 | 0 "Standard" | true | — | — |
Rim **Strong** (Clu's orange); BG **Faint**.

### A.10 Kurtz Compound
**Film:** *Apocalypse Now* (1979) — DP **Vittorio Storaro**. **Technique:** the face carved out of absolute black by low raking firelight; darkness treated as an active compositional force — half the head simply does not exist.
**Intent:** "A low fire rakes the face out of total darkness; the horror stays unlit."
`radius 1.6`, `master_ev 0.0`, `ratio_lock true`, `ratio_stops 5.0`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 85 | −8 | 17 "Golden Hour" | 0.0 | 0 "Standard" | true | — | — |
| FILL | −40 | 0 | 0 "2700K Incan" | −5.0 | 1 "Softbox" | false | — | — |
| RIM | −140 | 30 | 13 "Deep Amber" | −2.5 | 2 "Snoot" | false | — | — |
| BG | 170 | −15 | 19 "Blood Red" | −3.5 | 0 "Standard" | true | — | — |
Fill **off** (the void; fallback −5.0); Rim **Off**; BG **Faint**. Pairs with FX 2 Fire Flicker.

### A.11 Neon Purgatory
**Film:** *Only God Forgives* (2013) — DP **Larry Smith** (dir. Nicolas Winding Refn). **Technique:** monochrome immersion in deep red — whole rooms gelled red so the colour is the location; a faint magenta edge is the only relief.
**Intent:** "Everything drowned in red; a whisper of magenta is the only way out."
`radius 1.4`, `master_ev -0.25`, `ratio_lock true`, `ratio_stops 1.5`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 25 | 25 | 11 "Rosco Red" | 0.0 | 1 "Softbox" | true | — | — |
| FILL | −35 | 5 | 19 "Blood Red" | −1.5 | 1 "Softbox" | true | — | — |
| RIM | −160 | 35 | 10 "Magenta" | −2.5 | 2 "Snoot" | true | — | — |
| BG | 175 | −5 | 19 "Blood Red" | −2.0 | 0 "Standard" | true | — | — |
Rim **Faint**; BG **Subtle**. Distinct from Velvet Coven (A.3): monochrome red immersion vs. multi-primary.

### A.12 Green Code
**Film:** *The Matrix* (1999) — DP **Bill Pope**. **Technique:** every scene inside the Matrix carries a green fluorescent cast (famously pushed further in the DI/timing) — cool green-tinted key, green ambience, sickly-digital edge.
**Intent:** "The green cast that tells you none of this is real."
`radius 1.5`, `master_ev 0.0`, `ratio_lock true`, `ratio_stops 2.0`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 30 | 55 | 2 "4500K Neut" | 0.0 | 1 "Softbox" | true | — | 7 "Plus Green" |
| FILL | −40 | 10 | 9 "Plus Green" | −2.0 | 1 "Softbox" | true | — | — |
| RIM | −155 | 40 | 14 "Emerald" | −1.0 | 0 "Standard" | true | — | — |
| BG | 165 | −10 | 21 "Toxic Waste" | −2.0 | 0 "Standard" | true | — | — |
Rim **Subtle**; BG **Subtle**. Distinct from Sick Green Room (A.7): clean cool-green cast vs. green-plus-sodium decay. Pairs with FX 13 Matrix Drop.

### A.13 Expressionist Shadow
**Film:** *Nosferatu* (1922) — DP **Fritz Arno Wagner** (German Expressionism). **Technique:** a single hard raking key from an extreme side angle throwing enormous graphic shadows onto walls; the shadow, not the actor, plays the scene (the staircase claw).
**Intent:** "One hard raking key; the shadow on the wall is the monster."
`radius 1.7`, `master_ev 0.25`, `ratio_lock true`, `ratio_stops 5.0`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 100 | 20 | 2 "4500K Neut" | 0.0 | 0 "Standard" | true | 7 "Branches" | — |
| FILL | −30 | 0 | 5 "8000K Moon" | −5.0 | 1 "Softbox" | false | — | — |
| RIM | −140 | 35 | 5 "8000K Moon" | −2.5 | 2 "Snoot" | false | — | — |
| BG | −170 | −18 | 5 "8000K Moon" | −3.5 | 0 "Standard" | true | 3 "Prison Bars" | — |
Fill **off** (fallback −5.0); Rim **Off**; BG **Faint**. **Gobos:** Branches on the key claws the subject; Prison Bars on the BG paints stair-rail shadows up the wall. *(A true Nosferatu silhouette — key fully off — is not expressible easy-native since Easy forces Key on at EV 0; the raking-key reading was chosen deliberately. The shipped Advanced-native `Silhouette` preset covers the cutout case.)*

### A.14 Green-Gold Café
**Film:** *Amélie* (2001) — DP **Bruno Delbonnel**. **Technique:** storybook green-and-gold grade — warm golden key on faces with green-washed walls and saturated, lit backgrounds; whimsical, high-presence world.
**Intent:** "Golden faces in a green world — Montmartre as a picture book."
`radius 1.3`, `master_ev 0.25`, `ratio_lock true`, `ratio_stops 1.5`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 20 | 30 | 17 "Golden Hour" | 0.0 | 1 "Softbox" | true | — | — |
| FILL | −40 | 5 | 9 "Plus Green" | −1.5 | 1 "Softbox" | true | — | — |
| RIM | −150 | 35 | 7 "Full CTO" | −1.0 | 0 "Standard" | true | — | — |
| BG | 170 | 0 | 9 "Plus Green" | −0.5 | 1 "Softbox" | true | — | — |
Rim **Subtle**; BG **Strong** (the world stays lit — the anti-noir).

### A.15 Wallace Caustics
**Film:** *Blade Runner 2049* (2017) — DP **Roger Deakins**. **Technique:** Wallace Corp interiors lit by golden water-reflection caustics — a single hard warm source bounced off rippling water, shadows in amber-black. (The film's other signature, the monochrome orange Vegas dust, already ships as the `Amber Fog` preset.)
**Intent:** "Golden water ripples crawl over the subject; everything else is amber darkness."
`radius 2.0`, `master_ev 0.0`, `ratio_lock true`, `ratio_stops 4.0`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 45 | 60 | 17 "Golden Hour" | 0.0 | 0 "Standard" | true | 6 "Soft Dapple" | — |
| FILL | −35 | 0 | 13 "Deep Amber" | −4.0 | 1 "Softbox" | true | — | — |
| RIM | −150 | 40 | 17 "Golden Hour" | −2.5 | 2 "Snoot" | false | — | — |
| BG | 170 | −10 | 13 "Deep Amber" | −3.5 | 0 "Standard" | true | 6 "Soft Dapple" | — |
Rim **Off**; BG **Faint**. **Gobos:** Soft Dapple on key and BG = the caustic ripple. Pairs with FX 42 Passing Clouds for a slow exposure breathe that reads as water movement.

### A.16 Preset implementation note (for Codex)

* Append 15 `<map>` entries to `indra/newview/app_settings/cine_light_rig_presets.xml` (`version` stays 1), copying the normalized shape of the existing entries (Rembrandt `:7-20` shows the easy-native field set; gel-carrying and gobo-carrying rows exist throughout the file as syntax references). Emit **both** `*_idx` and `*_name` for profile/beam/gobo/gel — names must byte-match the tables at `alcinelightrigmodel.cpp:61-117` (the loader trusts names over indices on mismatch).
* Every entry: `category = "Genre / Mood"`, `master_ev` as specified, `ratio_lock true`, `ratio_stops` = Drama, Key `ev 0.0` + `on true`, Fill `ev` = −Drama (on/off per table), Rim/BG at their exact bucket EVs (or `on false` with the noted fallback EV).
* **No code change:** `masterSetups()` picks new entries up automatically; `master_ev` is applied via the existing optional-globals path. Sanity ranges all hold (yaw wraps, |pitch| ≤ 85, EV within [−20,20], radius ≥ 0.5, ratio ≤ 5, master within ±16).
* Easy round-trip: all Rim/BG EVs are exact bucket values, so `rimPresenceFromEV`/`bgPresenceFromEV` (`alcinelightrigmodel.cpp:575-591`) recover the intended presence; Dramas are within [0,5]; masters within ±16.

---

## Part C — PACK B: 14 new animated FX (ids 49–62)

Conventions (identical to the prior expansion): `fs` = phase units (`t/interval`), `step = floor(fs)` (`virtualStep`), `counter = (U64)step`, `L[i] = lights[i]`, EV −10 = idiomatic black. All loops via `positiveFmod`/`positiveMod`; all specs are pure closed-form functions of `(fx, seed, t)`. FX never touch `mGel`. Seed-dependent FX must be appended to `random_fx[]`.

New table rows (append in this order):

```
FX_NAMES     += "Boogie Floor", "Shootout", "Chopper Hunt", "Plasma Globe",
                "Dimensional Rift", "Kawoosh", "Time Circuits", "Jacob's Ladder",
                "Build & Drop", "Biolume Tide", "Mount Doom", "Vaporwave Sunset",
                "Carousel Waltz", "Five Tones",
FX_INTERVALS += 0.125f, 0.05f, 0.10f, 0.05f, 0.10f, 0.10f, 0.10f, 0.05f,
                0.10f, 0.20f, 0.20f, 0.25f, 0.10f, 0.15f,
```

### FX 49 — Boogie Floor  *(interval 0.125 s, beat 0.5 s = 120 BPM, bar 2 s, analytic)*
**Inspiration:** *Saturday Night Fever* (1977, DP Ralf D. Bode) — the under-lit dance floor pulsing on the beat. Distinct from Heartbeat (16, fixed lub-dub) and Club Strobe (1, random pops): a metronomic four-on-the-floor kick envelope from **below**, colours rotating per bar.
**Base (`initializeFX`):**
```
setLight(L,0,    0, -25, 15 /*Cyber Pink*/,  -10, 0, true);   // floor cell A
setLight(L,1, -120, -20, 16 /*Sci-Fi Cyan*/, -10, 0, true);   // floor cell B
setLight(L,2,  120, -20, 22 /*Neon Purple*/, -10, 0, true);   // floor cell C
setLight(L,3,  180,  10, 18 /*Vaporwave*/,  -3.5, 1, true);   // room glow
```
**Temporal spec:**
```
beatpos = positiveFmod(fs, 4.0) / 4.0;                // 0..1 inside each beat
env     = (1.0 - beatpos) * (1.0 - beatpos);          // hard attack, square decay
push    = (positiveMod(step / 4, 4) == 0) ? 0.6f : 0.f;   // downbeat accent
static const S32 FLOOR[4] = { 15, 16, 22, 14 };       // pink,cyan,purple,emerald
for i in 0..2:
    L[i].mProfile = FLOOR[positiveMod(step / 16 + i, 4)]; // colours rotate per bar
    L[i].mEV      = -2.5f + (2.8f + push) * (F32)env;     // peak +0.9 on the one
L[3].mEV = -3.5f + 0.4f * (F32)env;                   // room breathes with the kick
```
Seed-dependence: **none** (analytic).

### FX 50 — Shootout  *(interval 0.05 s, 2 s engagement windows, seed-dependent)*
**Inspiration:** *Heat* (1995, DP Dante Spinotti) — the downtown-LA daylight gun battle: clustered muzzle-flash strobes from one direction, answered from the opposite side, then silence. Distinct from Thunderstorm (14, sparse single sky flashes) and Paparazzi (5, uniform random pops): flashes arrive in **bursts** with a call-and-answer structure.
**Base:** for `i` in 0..3: `setLight(L,i, yaws[i], pitches[i], 23 /*Pure White*/, -10, 2 /*Snoot*/, true)` with `yaws = {30,-60,150,-120}`, `pitches = {5,0,10,0}`.
**Temporal spec:**
```
window = (U64)(step / 40);                            // one engagement beat per 2 s
local  = step - (S64)window * 40;
if (unitHash(seed, fx, window, 0, 0) < 0.75f) {       // 75% of windows have fire
    shooter = (S32)(4.f * unitHash(seed, fx, window, 0, 1)) & 3;
    rounds  = 4 + (S32)(6.f * unitHash(seed, fx, window, 0, 2));   // 4..9 rounds
    start   = (S64)(10.f * unitHash(seed, fx, window, 0, 3));      // 0..0.45 s in
    k = local - start;
    if (k >= 0 && k < rounds * 2 && (k & 1) == 0) {   // 10 Hz muzzle strobe
        L[shooter].mEV = 2.0f + 0.4f * unitHash(seed, fx, counter, shooter, 4);
        L[shooter].mYawDeg = wrap180(L[shooter].mYawDeg +
            6.f * (unitHash(seed, fx, counter, shooter, 5) - 0.5f));  // recoil jitter
    }
    if (unitHash(seed, fx, window, 1, 0) < 0.5f) {    // 50%: return fire
        replier = (shooter + 2) & 3;                  // from the opposite side
        j = local - 20 - (S64)(8.f * unitHash(seed, fx, window, 1, 1));
        if (j >= 0 && j < 8 && (j & 1) == 0) L[replier].mEV = 1.8f;
    }
}
// all other lights remain at base EV -10 (black between volleys)
```
Seed-dependence: **yes** (add 50 to `random_fx`).

### FX 51 — Chopper Hunt  *(interval 0.1 s, ~9 s orbit, seed-dependent)*
**Inspiration:** *Terminator 2* / *The Fugitive* (1993, DP Michael Chapman) — a police helicopter circling overhead, its searchlight hunting: an unstable orbiting beam pointed steeply down, with rotor chop and a hot lock-on as it passes the subject. Distinct from Searchlight (15, ground-level ±90° sine) and Lighthouse (39, level rotation with a cosine lobe): high-pitch orbital hunt with noise wobble and rotor flicker.
**Base:** `setLight(L,0, 0, 68, 3 /*5600K Day*/, -0.5, 2 /*Snoot*/, true)` (the beam); `setLight(L,2, 0, 75, 11 /*Rosco Red*/, -10, 2, true)` (nav strobe); `setLight(L,3, 180, -15, 20 /*Deep Space*/, -3, 0, true)` (night wash).
**Temporal spec:**
```
a = positiveFmod(4.0 * fs, 360.0);                    // orbit: 40°/s, one lap ~9 s
L[0].mYawDeg   = wrap180((F32)(a +
    25.f * (valueNoise(seed, fx, fs * 0.3, 0, 0) - 0.5f)));   // pilot hunting
L[0].mPitchDeg = 68.f + 12.f * (valueNoise(seed, fx, fs * 0.35, 0, 1) - 0.5f);
c = std::max(0.f, phaseCos(a * DEGREES_TO_RADIANS));  // lock-on lobe near yaw 0
L[0].mEV = -0.5f + 2.0f * std::pow(c, 6.f);           // pass-over hits +1.5
if (step & 1) L[0].mEV -= 0.3f;                       // 5 Hz rotor-blade chop
L[2].mYawDeg = L[0].mYawDeg;                          // nav strobe rides the airframe
L[2].mEV = (positiveMod(step, 10) == 0) ? 0.2f : -10.f;   // 1 Hz red blink
```
Seed-dependence: **yes** (add 51). Operator note: enable the KEY shaft for the visible hunting beam (same shaft/shadow caveats as the FX-48 note in the prior doc).

### FX 52 — Plasma Globe  *(interval 0.05 s, non-repeating noise, seed-dependent)*
**Inspiration:** *The Prestige* (2006, DP Wally Pfister) — Tesla's laboratory: violet filaments writhing inside a discharge globe, snapping into white arcs. Distinct from Will-o'-Wisp (43, one slow green wanderer): **two** fast counter-wandering violet filaments plus stochastic white discharge cracks.
**Base:** `setLight(L,0, 20, 10, 22 /*Neon Purple*/, -1, 2 /*Snoot*/, true)`; `setLight(L,1, -30, -5, 18 /*Vaporwave*/, -1.2, 2, true)`; `setLight(L,3, 180, 0, 12 /*Congo Blue*/, -3, 1, true)` (lab ambience).
**Temporal spec:**
```
for i in {0, 1}:                                      // two filaments
    L[i].mYawDeg   = 360.f * valueNoise(seed, fx, fs * 0.6, i, 0) - 180.f;
    L[i].mPitchDeg = 60.f * valueNoise(seed, fx, fs * 0.7, i, 1) - 20.f;
    L[i].mEV       = -1.2f + 0.8f * valueNoise(seed, fx, fs * 1.5, i, 2);
    L[i].mProfile  = (i == 0) ? 22 : 18;              // re-assert base hues
r = unitHash(seed, fx, counter, 0, 3);
if (r > 0.94f) {                                      // ~6%/tick: discharge snap
    j = (r > 0.97f) ? 1 : 0;
    L[j].mEV = 1.6f;  L[j].mProfile = 23;             // white arc
}
L[3].mEV = -3.f + 0.3f * phaseSin(fs * 0.25);         // globe hum
```
Seed-dependence: **yes** (add 52).

### FX 53 — Dimensional Rift  *(interval 0.1 s, non-repeating, seed-dependent)*
**Inspiration:** *Stranger Things* (2016–, pilot DP Tim Ives) — the Upside Down gate: a membranous red-violet mass behind the subject that breathes and crawls, occasionally tearing open with a violet flash and a cold blast on the subject's front. Distinct from Villain Reveal (32, scripted rise) and Supernova (28, one charge/flash cycle): an unresting organic breathe with stochastic tear events.
**Base:** `setLight(L,0, 180, 10, 19 /*Blood Red*/, -1, 1 /*Softbox*/, true)` (membrane); `setLight(L,1, 180, 0, 22 /*Neon Purple*/, -10, 0, false)` (tear flash); `setLight(L,3, 0, -15, 8 /*Full CTB*/, -3.5, 1, true)` (cold front wash).
**Temporal spec:**
```
L[0].mEV = -1.f + 0.6f * phaseSin(fs * 0.5) + 0.4f * phaseSin(fs * 0.13)
         + 0.5f * (valueNoise(seed, fx, fs * 0.4, 0, 0) - 0.5f);   // organic breathe
L[0].mYawDeg = wrap180(180.f +
    20.f * (valueNoise(seed, fx, fs * 0.15, 0, 1) - 0.5f));        // veins crawl
if (unitHash(seed, fx, counter, 0, 2) > 0.96f) {      // ~4%/step: the gate tears
    L[1].mOn = true;  L[1].mEV = 1.3f;                // violet flash
    L[3].mEV = -2.f;                                  // cold blast lifts the front
}
```
Seed-dependence: **yes** (add 53).

### FX 54 — Kawoosh  *(interval 0.1 s, 12 s loop, seed-dependent)*
**Inspiration:** *Stargate* (1994, DP Karl Walter Lindenlaub) — the gate sequence: seven chevrons lock around the ring, the event horizon erupts, then settles into a rippling blue pool. A scripted multi-phase sequence unlike anything in 0–48 (Supernova is one radial event; Stage Debut is a lighting build, not a device).
**Base:** `setLight(L,0, 0, 10, 13 /*Deep Amber*/, -10, 2 /*Snoot*/, true)` (chevron lamp); `setLight(L,1, 0, 0, 16 /*Sci-Fi Cyan*/, -10, 0, true)` (event horizon); `setLight(L,3, 180, -10, 20 /*Deep Space*/, -3, 0, true)` (gate room).
**Temporal spec:**
```
b = positiveFmod(fs, 120.0);                          // 12 s cycle
if (b < 56.0) {                                       // 7 chevrons, 0.8 s each
    chev = (S64)(b / 8.0);                            // 0..6
    lk   = positiveFmod(b, 8.0);
    L[0].mYawDeg = wrap180(-135.f + 45.f * (F32)chev);   // steps around the ring
    L[0].mEV = (lk < 3.0) ? 0.8f : -2.5f;             // blink on lock, dim hold
} else if (b < 60.0) {                                // KAWOOSH: 0.4 s eruption
    L[0].mEV = -10.f;
    L[1].mEV = 2.2f;  L[1].mProfile = 23;             // blinding white-blue burst
} else if (b < 105.0) {                               // open pool: water shimmer
    L[1].mProfile = 16;
    L[1].mEV = -0.6f + 0.5f * (valueNoise(seed, fx, fs * 0.9, 1, 0) - 0.5f)
             + 0.2f * phaseSin(fs * 1.7);
    L[3].mEV = -2.5f;                                 // room lifts in the glow
} else {                                              // shutdown
    L[0].mEV = -10.f;
    L[1].mEV = -0.6f - (F32)((b - 105.0) / 15.0) * 9.4f;   // fade to black
}
```
Seed-dependence: **yes** (add 54; the shimmer uses `valueNoise`).

### FX 55 — Time Circuits  *(interval 0.1 s, 10 s loop, analytic)*
**Inspiration:** *Back to the Future* (1985, DP Dean Cundey) — the flux capacitor: a three-arm blink accelerating as the DeLorean builds to 88 mph, ending in a white flash and burning tyre trails. Distinct from Rave Chase (38, constant-rate chase): the defining feature is the **chirp** — blink rate accelerates quadratically inside each run.
**Base:** `setLight(L,0, 40, 30, 4 /*6500K Cool*/, -10, 0, true); setLight(L,1, -40, 30, 4, -10, 0, true); setLight(L,2, 180, 45, 4, -10, 0, true);` (the three capacitor arms); `setLight(L,3, 0, -30, 13 /*Deep Amber*/, -10, 0, false)` (fire trails).
**Temporal spec:**
```
b = positiveFmod(fs, 100.0);                          // 10 s cycle
if (b < 80.0) {                                       // acceleration run
    p = 0.004 * b * b;                                // quadratic phase: 0.8→6.3 Hz
    gate   = positiveFmod(p, 1.0) < 0.5;              // 50% duty blink
    active = positiveMod((S64)(p * 3.0), 3);          // chase advances with the chirp
    for i in 0..2: L[i].mEV = (gate && i == (S32)active) ? 0.9f : -10.f;
} else if (b < 84.0) {                                // 88 MPH: white flash
    for i in 0..2: { L[i].mEV = 2.5f; L[i].mProfile = 23; }
} else if (b < 95.0) {                                // burning tyre trails
    for i in 0..2: L[i].mEV = -10.f;
    L[3].mOn = true;
    L[3].mEV = 0.5f - (F32)(b - 84.0) * 0.55f;        // amber trails die out
}                                                     // else: dark until the loop
```
Seed-dependence: **none** (analytic).

### FX 56 — Jacob's Ladder  *(interval 0.05 s, 2 s loop, seed-dependent)*
**Inspiration:** *Frankenstein* (1931, DP Arthur Edeson) — the laboratory's climbing spark gap: a sputtering violet-white arc rises the ladder, extinguishes at the top, restrikes at the bottom. Distinct from Matrix Drop (13, smooth green descent): a rising, sputtering arc with white snaps.
**Base:** `setLight(L,0, 25, -30, 22 /*Neon Purple*/, -10, 0, true)` (the arc); `setLight(L,3, 180, 20, 6 /*10000K Sky*/, -4, 1, true)` (lab glow).
**Temporal spec:**
```
b = positiveFmod(fs, 40.0);                           // 2 s cycle
if (b < 30.0) {                                       // 1.5 s ascent
    u = b / 30.0;
    L[0].mPitchDeg = -30.f + 100.f * (F32)u;          // climbs -30° → +70°
    r = unitHash(seed, fx, counter, 0, 0);
    L[0].mEV = (r > 0.3f) ? 0.6f + 0.5f * unitHash(seed, fx, counter, 0, 1)
                          : -3.f;                     // 70% lit / 30% sputter
    L[0].mProfile = (r > 0.8f) ? 23 : 22;             // occasional white snap
    L[3].mEV = -4.f + 1.2f * (F32)u;                  // lab brightens as arc rises
} else {                                              // 0.5 s gap before restrike
    L[0].mEV = -10.f;
}
```
Seed-dependence: **yes** (add 56).

### FX 57 — Build & Drop  *(interval 0.1 s, 16 s loop, analytic)*
**Inspiration:** EDM festival / *Spring Breakers* (2012, DP Benoît Debie) neon rave climax — the strobe tempo ramps through the build, a one-second blackout, then the drop hits with a full-rig colour strobe. Distinct from Club Strobe (1, stationary random pops) and Rave Chase (38, constant chase): the structure IS the tempo ramp and the drop.
**Base:** `setLight(L,0, 60, 25, 15 /*Cyber Pink*/, -10, 0, true); setLight(L,1, -60, 25, 16 /*Sci-Fi Cyan*/, -10, 0, true); setLight(L,2, 150, 30, 22 /*Neon Purple*/, -10, 0, true); setLight(L,3, -150, 30, 14 /*Emerald*/, -10, 0, true)`.
**Temporal spec:**
```
b = positiveFmod(fs, 160.0);                          // 16 s bar
if (b < 80.0) {                                       // THE BUILD (8 s)
    p    = 0.003 * b * b;                             // chirp: ~0 → 4.8 strobes/s
    gate = positiveFmod(p, 1.0) < 0.5;
    ev   = -2.f + (F32)(b / 80.0) * 3.f;              // climbs -2 → +1
    L[0].mEV = gate ? ev : -10.f;                     // pink and cyan
    L[1].mEV = gate ? -10.f : ev;                     //   alternate anti-phase
    L[2].mEV = L[3].mEV = -10.f;
} else if (b < 90.0) {                                // 1 s blackout: the inhale
    for i in 0..3: L[i].mEV = -10.f;
} else {                                              // THE DROP (7 s)
    on   = positiveMod(step, 2) == 0;                 // 5 Hz full-rig strobe
    prof = 7 + (S32)positiveMod(step / 2, 16);        // party palette walk
    for i in 0..3: { L[i].mProfile = prof; L[i].mEV = on ? 1.5f : -10.f; }
}
```
Seed-dependence: **none** (analytic).

### FX 58 — Biolume Tide  *(interval 0.2 s, ~3.6 s wave lap, seed-dependent)*
**Inspiration:** *Life of Pi* (2012, DP Claudio Miranda) — the bioluminescent ocean: a glowing crest travels around the subject as plankton sparkle. Distinct from Underwater (7, two-light sine wobble): a **travelling wave** across four quadrant lights from below, with stochastic glints.
**Base:** for `i` in 0..3: `setLight(L,i, yaws[i], -10, profiles[i], -3, 1 /*Softbox*/, true)` with `yaws = {45,-45,135,-135}`, `profiles = {16,14,16,14}` (cyan/emerald alternating).
**Temporal spec:**
```
w = fs * 0.35;                                        // crest laps once per ~3.6 s
for i in 0..3:
    crest = phaseCos(w - (F64)i * (TWO_PI / 4.0));    // phase-offset per quadrant
    L[i].mEV = -3.f + 1.6f * std::max(0.f, crest)     // wave lifts each side in turn
             + 0.5f * (valueNoise(seed, fx, fs * 0.8, i, 0) - 0.5f);  // shimmer
    if (unitHash(seed, fx, counter, i, 1) > 0.985f)   // rare plankton glint
        L[i].mEV = 0.8f;
```
Seed-dependence: **yes** (add 58).

### FX 59 — Mount Doom  *(interval 0.2 s, non-repeating noise, seed-dependent)*
**Inspiration:** *The Lord of the Rings: The Return of the King* (2003, DP Andrew Lesnie) — inside Sammath Naur: molten under-light surging in slow swells, amber spatter, a fire-red sky behind. Distinct from Fire Flicker (2, fast stepped campfire) and Explosion (27, one scripted event): multi-second molten **swells** from below with rare ember spikes.
**Base:** `setLight(L,0, 10, -35, 19 /*Blood Red*/, -1, 1 /*Softbox*/, true)` (lava under-key); `setLight(L,1, -30, -25, 13 /*Deep Amber*/, -1.5, 1, true)` (molten bounce); `setLight(L,3, 180, 20, 19 /*Blood Red*/, -3.5, 0, true)` (fire sky).
**Temporal spec:**
```
surge = valueNoise(seed, fx, fs * 0.06, 0, 0);        // one swell ≈ 3.3 s
L[0].mEV = -1.5f + 2.0f * surge;                      // -1.5 → +0.5 at full surge
L[1].mEV = -2.f + 1.5f * valueNoise(seed, fx, fs * 0.3, 1, 0);   // faster churn
L[3].mEV = -3.5f + surge;                             // sky glows with the swell
if (unitHash(seed, fx, counter, 0, 1) > 0.95f)        // 5%/step: ember spatter
    L[1].mEV = 0.9f;
```
Seed-dependence: **yes** (add 59).

### FX 60 — Vaporwave Sunset  *(interval 0.25 s, 40 s loop, analytic)*
**Inspiration:** synthwave / *Kung Fury* (2015, dir. David Sandberg) — the neon-grid sunset: a striped sun sinks from gold through pink to violet behind the subject while the grid pulses and a scanline blanks periodically. Distinct from Sunrise Sweep (36, naturalistic dawn whitening): reversed direction, synthetic neon palette, rhythmic grid pulse and scanline dips.
**Base:** `setLight(L,0, 180, 25, 17 /*Golden Hour*/, 0.5, 0, true)` (the sun, behind); `setLight(L,2, -100, 12, 15 /*Cyber Pink*/, -1.5, 2 /*Snoot*/, true)` (neon rim); `setLight(L,3, 0, -25, 22 /*Neon Purple*/, -2, 1, true)` (the grid, from below-front).
**Temporal spec:**
```
u = positiveFmod(fs, 160.0) / 160.0;                  // 0..1 over 40 s
L[0].mPitchDeg = 25.f - 30.f * (F32)u;                // sun sinks 25° → -5°
L[0].mEV       = 0.5f - 0.8f * (F32)u;
L[0].mProfile  = u < 0.33 ? 17 : u < 0.66 ? 15 : 18;  // gold → pink → vaporwave
L[2].mEV = -1.5f + 0.4f * phaseSin(fs * 0.8);         // neon rim throb
L[3].mEV = -2.f + 0.6f * phaseSin(fs * 0.4);          // grid pulse
if (positiveFmod(fs, 16.0) < 0.5) L[0].mEV -= 1.f;    // scanline blank every 4 s
```
Seed-dependence: **none** (analytic).

### FX 61 — Carousel Waltz  *(interval 0.1 s, 6 s revolution, waltz bar 1.5 s, analytic)*
**Inspiration:** *Strangers on a Train* (1951, DP Robert Burks) — the runaway carousel finale: warm incandescent bulb clusters orbiting the subject in waltz time, bobbing with the horses. Distinct from Disco Ball (11, fast random-colour spin) and RGB Gamer (10): slow warm constant-colour orbit with a 3/4-time downbeat accent and a vertical bob.
**Base:** `setLight(L,0, 0, 18, 0 /*2700K Incan*/, -0.8, 0, true); setLight(L,1, 180, 18, 0, -0.8, 0, true);` (opposed bulb clusters); `setLight(L,3, 0, -20, 12 /*Congo Blue*/, -3, 1, true)` (fairground night).
**Temporal spec:**
```
a = positiveFmod(6.0 * fs, 360.0);                    // one revolution per 6 s
L[0].mYawDeg = wrap180((F32)a);
L[1].mYawDeg = wrap180((F32)(a + 180.0));
bob = phaseSin(fs * 0.9);                             // the horses rise and fall
L[0].mPitchDeg = 18.f + 6.f * bob;
L[1].mPitchDeg = 18.f - 6.f * bob;                    // opposite horse phase
accent = (positiveMod(step, 15) < 5) ? 0.5f : 0.f;    // OOM-pah-pah (0.5 s beats)
shimmer = 0.15f * phaseSin(fs * 3.1);                 // filament shimmer
L[0].mEV = L[1].mEV = -0.8f + accent + shimmer;
```
Seed-dependence: **none** (analytic).

### FX 62 — Five Tones  *(interval 0.15 s, 12 s loop, analytic)*
**Inspiration:** *Close Encounters of the Third Kind* (1977, DP Vilmos Zsigmond) — the mothership light-organ conversation: a five-note colour phrase played quietly, answered louder, then the full light-organ, then the blinding white reply. Nothing in 0–48 sequences colour as melody.
**Base:** `setLight(L,0, 0, 35, 10 /*Magenta*/, -10, 2 /*Snoot*/, true); setLight(L,1, 40, 50, 10, -10, 0, true); setLight(L,2, -40, 55, 10, -10, 0, true); setLight(L,3, 180, 60, 10, -10, 0, true)` (all overhead — the ship above).
**Temporal spec:**
```
b    = positiveMod(step, 80);                         // 12 s conversation
note = (S32)positiveMod(step / 4, 5);                 // one note per 0.6 s
static const S32 NOTES[5] = { 10, 13, 22, 7, 12 };    // magenta,amber,purple,CTO-orange,congo
gate = positiveMod(step, 4) < 3;                      // 3-on 1-off articulation
if (b < 20) {                                         // the call (quiet, one lamp)
    L[0].mProfile = NOTES[note];
    L[0].mEV = gate ? -0.5f : -10.f;
} else if (b < 40) {                                  // the answer (louder, twin)
    L[0].mProfile = L[1].mProfile = NOTES[note];
    L[0].mEV = L[1].mEV = gate ? 1.f : -10.f;
} else if (b < 48) {                                  // pause: faint hull glow
    L[3].mProfile = 20;  L[3].mEV = -3.f;
} else if (b < 68) {                                  // full light-organ, staggered
    for i in 0..3:
        L[i].mProfile = NOTES[positiveMod(note + i, 5)];
        L[i].mEV = gate ? 1.4f : -1.5f;
} else {                                              // the blinding white reply
    for i in 0..3: { L[i].mProfile = 23; L[i].mEV = 2.4f - (F32)(b - 68) * 0.25f; }
}
```
Seed-dependence: **none** (analytic; `NOTES` is a `static const` table like FX 45's `SOS_MASK`).

### C.1 FX implementation note (for Codex)

1. `indra/newview/alcinelightrigmodel.h:22` — `FX_COUNT` 49 → **63**.
2. `alcinelightrigmodel.cpp:119-134` — append the 14 names to `FX_NAMES` (order above).
3. `alcinelightrigmodel.cpp:136-144` — append the 14 intervals to `FX_INTERVALS` (order above).
4. `initializeFX` (`cpp:325-540`) — add `case 49: … case 62:` with the base poses above (`setLight`; no new FX uses a base-pose gobo, so no direct `mGobo` assignments are needed in `initializeFX` for this pack).
5. `evalFX` switch (`cpp:1078-1868`) — add the matching cases using only `phaseSin/phaseCos/positiveFmod/positiveMod/unitHash/valueNoise/std::pow/wrap180/std::max/std::min` and plain arithmetic (`static const` tables permitted: FLOOR, NOTES, yaws/pitches). Never touch `mGel`.
6. Tests `indra/newview/tests/alcinelightrigmodel_test.cpp`:
   * FX_COUNT golden asserts 49 → **63**.
   * Extend the mirrored golden name/interval tables identically.
   * `random_fx[]` (`:732-735`) += **{50, 51, 52, 53, 54, 56, 58, 59}** (49, 55, 57, 60, 61, 62 are analytic and must remain seed-invariant).
7. No UI work: the FX combo populates from `FX_COUNT`.
8. Operator note to carry into user docs: Chopper Hunt (51) and Kawoosh (54) read best with the relevant per-light **volumetric shafts** enabled and shadow policy "all projectors" — same caveat block as the FX 48 note in `doc/CINE_FX_PRESETS_EXPANSION.md` (an FX cannot toggle `mShaftEnabled` itself).

Design constraints honoured: EV stays within the −10..+3 idiom; every loop is closed-form via `positiveFmod`/`positiveMod` (scrub-safe, replay-safe); the two chirp effects (55, 57) compute their quadratic phase from the **loop-local** `b`, so they too are exactly periodic and scrub-order independent; per-light phase independence uses the hash/noise `light`/`draw` arguments — no evalFX extension needed.

---

## Summary tables

### Pack A — presets

| preset | film (DP) | master_ev | drama | rim | bg |
|---|---|---:|---:|---|---|
| Don's Study | The Godfather (Gordon Willis) | −0.5 | 4.5 | Off | Faint |
| Duel by Candlelight | Barry Lyndon (John Alcott) | −0.25 | 1.25 | Off | Subtle |
| Velvet Coven | Suspiria (Luciano Tovoli) | 0.25 | 1.0 | Strong | Subtle |
| Graphic Noir | Sin City (Robert Rodriguez) | 0.5 | 5.0 | Strong | Off |
| Pod Bay | 2001: A Space Odyssey (Geoffrey Unsworth) | 0.25 | 0.5 | Off | Strong |
| Mood for Love | In the Mood for Love (Christopher Doyle) | 0.0 | 3.0 | Subtle | Subtle |
| Sick Green Room | Joker (Lawrence Sher) | −0.25 | 2.5 | Faint | Subtle |
| 2019 Blinds | Blade Runner (Jordan Cronenweth) | 0.0 | 3.5 | Subtle | Faint |
| The Grid | TRON: Legacy (Claudio Miranda) | −0.25 | 3.0 | Strong | Faint |
| Kurtz Compound | Apocalypse Now (Vittorio Storaro) | 0.0 | 5.0 | Off | Faint |
| Neon Purgatory | Only God Forgives (Larry Smith) | −0.25 | 1.5 | Faint | Subtle |
| Green Code | The Matrix (Bill Pope) | 0.0 | 2.0 | Subtle | Subtle |
| Expressionist Shadow | Nosferatu (Fritz Arno Wagner) | 0.25 | 5.0 | Off | Faint |
| Green-Gold Café | Amélie (Bruno Delbonnel) | 0.25 | 1.5 | Subtle | Strong |
| Wallace Caustics | Blade Runner 2049 (Roger Deakins) | 0.0 | 4.0 | Off | Faint |

### Pack B — FX

| id | name | inspiration | interval (s) | seed-dep |
|---:|---|---|---:|---|
| 49 | Boogie Floor | Saturday Night Fever (dance-floor beat pulse) | 0.125 | no |
| 50 | Shootout | Heat (muzzle-flash volleys) | 0.05 | yes |
| 51 | Chopper Hunt | Terminator 2 / The Fugitive (circling searchlight) | 0.10 | yes |
| 52 | Plasma Globe | The Prestige (Tesla filaments) | 0.05 | yes |
| 53 | Dimensional Rift | Stranger Things (the Upside Down gate) | 0.10 | yes |
| 54 | Kawoosh | Stargate (chevron lock + eruption + pool) | 0.10 | yes |
| 55 | Time Circuits | Back to the Future (flux-capacitor chirp) | 0.10 | no |
| 56 | Jacob's Ladder | Frankenstein 1931 (climbing spark gap) | 0.05 | yes |
| 57 | Build & Drop | EDM festival / Spring Breakers (tempo-ramp strobe) | 0.10 | no |
| 58 | Biolume Tide | Life of Pi (bioluminescent wave) | 0.20 | yes |
| 59 | Mount Doom | LOTR: The Return of the King (molten swells) | 0.20 | yes |
| 60 | Vaporwave Sunset | synthwave / Kung Fury (neon-grid sunset) | 0.25 | no |
| 61 | Carousel Waltz | Strangers on a Train (carousel finale) | 0.10 | no |
| 62 | Five Tones | Close Encounters of the Third Kind (light-organ) | 0.15 | no |

### Required edits recap

| file | change |
|---|---|
| `indra/newview/alcinelightrigmodel.h:22` | `FX_COUNT = 63` |
| `indra/newview/alcinelightrigmodel.cpp` | +14 rows `FX_NAMES`, +14 rows `FX_INTERVALS`, +14 cases `initializeFX`, +14 cases `evalFX` |
| `indra/newview/tests/alcinelightrigmodel_test.cpp` | goldens 49→63, mirrored name/interval tables, `random_fx` += {50,51,52,53,54,56,58,59} |
| `indra/newview/app_settings/cine_light_rig_presets.xml` | +15 preset maps, all `category` "Genre / Mood", all easy-native (`master_ev`, Key EV 0, ratio lock, bucket Rim/BG) |

No new model capability, no schema version bump, no XUI change.

---

## Ideas dropped and why

* **Already covered by FX 0–48** (the brainstorm list overlapped the shipped set heavily): Police Lights (0), Rave Strobe (1), Lightning Storm (14), Disco Ball (11), Heartbeat Monitor (16), Broken Neon Sign (33 Neon Buzz), Paparazzi Flashbulbs (5), Aurora Borealis (22), Film Projector Flicker (17), Welding Arc (34), TV Static Glow (6), Camera Flash Cascade (≈5), Fireball/Explosion (27), Emergency Siren sweep (≈0+12 Warning Alert). Time Portal was merged into Kawoosh/Dimensional Rift to avoid three portal effects.
* **Music VU Pulse (true beat-reactive)** cannot exist under the pure-FX contract — evalFX has no audio input; Boogie Floor ships the closest legal thing, a fixed 120 BPM closed-form kick envelope.
* **Animated gel effects** (e.g. a CTO-ramp sunset via gels) are contractually impossible — FX must never set `mGel`; all colour animation in Pack B steps `mProfile` instead.
* **Preset duplicates avoided:** John Wick / Continental and Drive (magenta+cyan LA night) were dropped because Neon Crossfire, Music-Video Neon and Sodium Vapor Night already own that territory; Mad Max Fury Road teal-night overlapped Blue Hour Moon / Moonlight 8000K; Oppenheimer's stark looks overlapped Graphic Noir (B&W) and Kurtz Compound (fire out of black); BR2049's orange Vegas dust already ships as Amber Fog (hence the Wallace golden-caustics look instead).
* **Easy-native casualties:** a true Nosferatu **silhouette** (Key off) is inexpressible easy-native (Easy forces Key on at EV 0) — the preset was authored as the raking-key expressionist look instead, with the shipped Advanced-native Silhouette covering the cutout case. No other Pack A concept had to bend: all Rim/BG accents landed on exact bucket values.
