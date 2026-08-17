# Cinematic Light Rig — FX & Preset Expansion Spec

**Status:** design spec, ready for implementation. No source is modified by this document.
**Scope:** adds **16 new animated FX** (ids 33–48) and **15 new master presets** to the Cinematic Light Rig, expressed entirely in the existing `evalFX` / preset-XML systems. **No schema change, no new model capability is required.**

---

## Part A — Existing-system inventory (the contract)

### A.1 The evalFX contract

Defined in `indra/newview/alcinelightrigmodel.cpp` and `alcinelightrigmodel.h`.

```
void evalFX(S32 fx, U64 seed, F64 t_seconds, LightBase out[LIGHT_COUNT]);
```
(`alcinelightrigmodel.h:143-144`, implementation `alcinelightrigmodel.cpp:914-1432`)

* **Inputs:** effect id, deterministic seed (never `Math.random`; seed comes from `Globals::mSeed`, default `0x13579bdf`, `alcinelightrigmodel.h:26`), and `t_seconds` = presentation time since the FX started (`alcinelightrig.cpp:1799-1800` passes `presentation_time - mFXStart`). Time is clamped to `[0, 1e12]` (`alcinelightrigmodel.cpp:927-928`).
* **Time convention:** each FX has a per-effect tick `interval` in seconds (`FX_INTERVALS`, `alcinelightrigmodel.cpp:131-137`). The body works in **phase units** `fs = t_seconds / interval` (a float, `cpp:929`) and in discrete steps `step = floor(fs)` (`virtualStep`, `cpp:262-267`). Loops are explicit: `positiveFmod(fs, N)` (float loop of N phase units) or `positiveMod(step, N)` (integer loop). Nothing latches — every output is a pure closed-form function of `(fx, seed, t)`, byte-stable and scrub-order independent (enforced by test 5, `indra/newview/tests/alcinelightrigmodel_test.cpp:685-726`).
* **Structure of an FX:** `initializeFX(fx, lights)` (`cpp:318-455`) writes the static base pose via `setLight(lights, index, yaw, pitch, profile, ev, beam, on)` (`cpp:287-298`; `mGobo`/`mGel` can be assigned directly on the struct afterwards); then the `switch (fx)` in `evalFX` (`cpp:933-1429`) overwrites fields over time. Output passes through `copyCleanLights` → `cleanLight` sanitation (`cpp:300-316`, `155-168`).
* **Outputs / what an FX may modulate**, per light (KEY=0, FILL=1, RIM=2, BG=3, role names `alcinelightrig.cpp:66-68`), fields of `LightBase` (`alcinelightrigmodel.h:39-49`):
  * `mYawDeg` — orbit azimuth, wrapped to (−180, 180], 0 = subject front (`wrap180`, `cpp:587-603`)
  * `mPitchDeg` — elevation, clamped to ±85° (`PITCH_LIMIT_DEG`, `.h:24`)
  * `mProfile` — colour index 0–23 (table `cpp:61-86`; this is the FX "hue" channel — there is no continuous hue, colour animates by stepping profiles)
  * `mEV` — exposure in stops, clamped [−20, 20] (`MIN_EV/MAX_EV`, `cpp:25-26`). Practical FX range is **−10 (idiomatic "black")** to about **+3**; EV −10 falls under the 0.001 pre-headroom cutoff in `render()` (`cpp:836-837`) so it reads as off.
  * `mBeam` — 0 Standard (FOV 1.5 rad, falloff 1.0), 1 Softbox (2.8, 1.5), 2 Snoot (0.2, 0.5) (`cpp:88-92`)
  * `mOn` — hard gate
  * `mGobo` — 0–7 (`cpp:114-117`); flows through the FX path (the rig only re-layers the operator's **gel** on top of FX output, `alcinelightrig.cpp:1806-1811`, so FX must not set `mGel` — colour via `mProfile` only)
* **Helpers available (and the only randomness permitted):**
  * `phaseSin(x)` / `phaseCos(x)` — sin/cos of a radian phase, fmod-safe (`cpp:234-242`)
  * `unitHash(seed, fx, counter, light, draw)` → [0,1) stateless hash (`cpp:252-260`) — stepped randomness
  * `valueNoise(seed, fx, coordinate, light, draw)` → smoothstep-interpolated 1-D value noise (`cpp:275-285`) — smooth randomness
  * `positiveFmod`, `positiveMod`, `virtualStep`, `wrap180`, `std::pow` (already used, e.g. `cpp:1351`)
  * **Per-light independent phase already exists** — pass the light index into the hash/noise `light` argument or add a per-light phase offset (see FX 19 Fairy Woods, `cpp:1105-1109`). No extension needed.
* **Seed discipline:** effects that call `unitHash`/`valueNoise` are "random" and must separate seeds; analytic effects must be seed-invariant. Test 6 keeps an explicit list `random_fx[] = {1,2,5,6,9,11,13,14,17,20,25,27,30,31}` (`alcinelightrigmodel_test.cpp:732-734`) that must be extended for new random FX.
* **Registration surface for a new FX:** `FX_COUNT` (`alcinelightrigmodel.h:22`), `FX_NAMES` (`cpp:119-129`), `FX_INTERVALS` (`cpp:131-137`), a case in `initializeFX` (`cpp:318-455`), a case in `evalFX` (`cpp:933-1429`). The UI combo self-populates from `FX_COUNT` (`alpanelcinelightrig.cpp:316-318`) — no XUI change. Golden tests mirror the tables: `FX_COUNT == 33` assertions (`alcinelightrigmodel_test.cpp:896-897`), name table (`:964-974`), interval table (`:980-986`).

### A.2 Existing 33 FX (do not duplicate)

| id | name | one-line behaviour (cite = `alcinelightrigmodel.cpp`) |
|---:|---|---|
| 0 | Police Sirens | red/blue lights at ±45° alternate EV +1 / −10 every 0.15 s step (`:935-946`) |
| 1 | Club Strobe | 4 lights, random pitch 30–60°, 40 % chance per 0.1 s of a random party-profile pop at EV 0.5–1.5 (`:947-965`) |
| 2 | Fire Flicker | two warm low lights, stepped random EV −1.25±0.75 / −1.5±0.5 every 0.2 s (`:966-971`) |
| 3 | Streetlight | one 5600K light walks overhead front→back (pitch 30→85, flip yaw, 85→30) over an 8 s loop; EV tracks pitch (`:972-988`) |
| 4 | Neon Pulse | pink & cyan at ±45°, smooth offset sine EV ±1.5 (`:989-992`) |
| 5 | Paparazzi | each of 4 snoots has 20 %/0.1 s chance of a white EV +2 flash from a random direction (`:993-1010`) |
| 6 | TV Screen | frontal light flickers EV −1..+1, profile flips 6500K/10000K randomly (`:1011-1014`) |
| 7 | Underwater | cyan/red pair, slow sine EV and pitch wobble (`:1015-1020`) |
| 8 | UFO Abduction | overhead Emerald snoot spins 15°/step, EV pulses 0.5–1.5 (`:1021-1025`) |
| 9 | Haunted Flicker | dim warm key −1.5±0.15 with 10 % blackout dropouts (`:1026-1030`) |
| 10 | RGB Gamer | 4 lights co-rotate 5°/step, cycle the 16 party profiles (`:1031-1040`) |
| 11 | Disco Ball | 4 lights spin 20°/step, pitch sine, random colour every 5 steps (`:1041-1055`) |
| 12 | Warning Alert | rotating Rosco-Red beacon, 25°/step (`:1056-1059`) |
| 13 | Matrix Drop | green light descends pitch 85→−85, re-drops at a random yaw (`:1060-1068`) |
| 14 | Thunderstorm | 10 %/0.05 s chance of a 5600K flash EV 2–3 from a random direction, else black (`:1069-1084`) |
| 15 | Searchlight | 5600K snoot sweeps yaw ±90° as a slow sine (`:1085-1087`) |
| 16 | Heartbeat | lub-dub: EV 1.5 at beats 0 and 3 of a 15-step cycle, else −3 (`:1088-1093`) |
| 17 | Movie Projector | frontal flicker EV −1..1.5, profile flips 5600K/4500K (`:1094-1097`) |
| 18 | Warp Tunnel | 4 coloured lights staggered pitch sweeps 85→−85 (`:1098-1104`) |
| 19 | Fairy Woods | 3 coloured lights breathe EV as independent slow sines (`:1105-1109`) |
| 20 | Short Circuit | overhead sputters bright/dim/black on stepped random (`:1110-1123`) |
| 21 | Red Alert Pulse | two Rosco-Red lights, synchronized sine EV ±2 (`:1124-1127`) |
| 22 | Aurora Borealis | 3 cool/coloured high lights, slow independent pitch waves (`:1128-1132`) |
| 23 | Cyber Scanner | cyan snoot scans pitch ±60° sine (`:1133-1135`) |
| 24 | Shooting Star | 1 s streak yaw −115→+115 with decaying EV, dark for rest of 2 s loop (`:1136-1151`) |
| 25 | Elevator Fault | 4-light dim overhead ring, full blackout every 15 steps + rare single-light dropouts (`:1152-1174`) |
| 26 | Car Pass | paired headlights sweep by, later red taillights recede; 3.85 s loop (`:1175-1204`) |
| 27 | Explosion | flash → amber decay → ember flicker → dark with sparks, 65-step cycle (`:1205-1259`) |
| 28 | Supernova | slow charge-up → white flash → collapse → cold afterglow, 90-step cycle (`:1260-1313`) |
| 29 | Stage Debut | sequential build: bg, key, fill fade, rim, then key breathing; all beat-latched (`:1314-1344`) |
| 30 | Swinging Lamp | pendulum yaw with exponential amplitude decay + noise EV (`:1345-1371`) |
| 31 | Parachute Flare | flare descends pitch 85→5 while swinging and dimming, burns out (`:1372-1399`) |
| 32 | Villain Reveal | darkness → under-light rises → hold with fill fade-in, 12 s cycle (`:1400-1426`) |

Concepts from the brainstorm list **already covered** (do not re-propose): police strobe (0), club/rave strobe (1), campfire flicker (2), paparazzi (5), TV noise (6), underwater shimmer (7), haunted/faulty flicker (9, 20, 25), disco colour-cycle (11), lightning storm (14), heartbeat (16), projector gate flicker (17), aurora drift (22), red-alert (21).

### A.3 The preset (master setup) contract

A preset is exactly an `ALCineLightRigModel::Setup` (`alcinelightrigmodel.h:51-57`):

| field | unit / range | cite |
|---|---|---|
| `mRadius` | metres, clamped [0.5, 512] (practical 0.9–2.5) | `alcinelightrigmodel.cpp:462-463`, `.h:25` |
| `mLights[4]` | KEY / FILL / RIM / BG in that order | `alcinelightrig.cpp:66-68` |
| per light `mYawDeg` | degrees, wrapped (−180,180], 0 = subject front, +90 = subject's left | `cleanLight`, `cpp:159` |
| per light `mPitchDeg` | degrees, ±85; negative = below eyeline | `cpp:160` |
| per light `mProfile` | 0–23, table `alcinelightrigmodel.cpp:61-86` | |
| per light `mEV` | stops, [−20,20]; presets use ≈ −6..+1.75 | `cpp:162` |
| per light `mBeam` | 0 Standard / 1 Softbox / 2 Snoot | `cpp:88-92` |
| per light `mOn` | bool | |
| per light `mGobo` | 0–7: Default, Venetian Blinds, Window Panes, Prison Bars, Slats, Grid, Soft Dapple, Branches | `cpp:114-117` |
| per light `mGel` | 0–14: None, CTO, 1/2 CTO, 1/4 CTO, CTB, 1/2 CTB, 1/4 CTB, Plus Green, Minus Green, Bastard Amber, Steel Blue, Congo Blue, Primary Red, Primary Green, Primary Blue | `cpp:96-112` |
| `mRatioLock` / `mRatioStops` | bool / [0,5] stops; when locked, FILL EV := KEY EV − ratio | `cpp:469-470`, `:645-652` |

**Important scope note:** master EV, master colour-temp mired trim, mirror, orbit yaw/pitch, offset-Z, bounce, catchlight are **`Globals`/`Transforms`, not `Setup`** (`alcinelightrigmodel.h:59-80`) and are **not stored in presets** — `setupFromLLSD` / `setupToLLSD` serialize only radius, ratio lock/stops and the 4 lights (`alcinelightrig.cpp:366-456`). New presets below therefore express everything through profiles/gels/EV; where a master-temp trim would help, it is mentioned in the intent text only.

**Data path:** built-ins live in `indra/newview/app_settings/cine_light_rig_presets.xml` (LLSD, `version=1`, `presets` array of maps with `name`, optional `intent`, optional `category` == `"Genre / Mood"`, `radius`, optional `ratio_lock`/`ratio_stops`, `lights` array of exactly 4 maps with `yaw`, `pitch`, `profile_idx`+`profile_name`, `ev`, `beam_idx`+`beam_name`, `on`, optional `gobo_idx`+`gobo_name`, optional `gel_idx`+`gel_name`). The loader is `ALCineLightRig::masterSetups()` (`alcinelightrig.cpp:2016-2185`): it prepends the compiled `Classic 3-Point` (`classicSetup()`, `alcinelightrigmodel.cpp:1479-1489`), rejects duplicates/malformed entries, groups `Genre / Mood` entries into their own combo section (`alcinelightrig.cpp:2097-2099`, `2215-2232`), and auto-renames colliding user presets. Name-strings are authoritative fallbacks over indices (`setupFromLLSD`, `cpp:390-437`), so `*_name` keys must exactly match the tables.

### A.4 Existing presets (do not duplicate)

Compiled: **Classic 3-Point** (`alcinelightrig.cpp:2022-2026`).
From `cine_light_rig_presets.xml` (line cites into that file): Rembrandt (:8), Paramount Butterfly (:19), Broadcast Interview (:30), **Film Noir** (genre, :41), Silhouette (:55), **Golden Hour** (genre, :66), Blue Hour Moon (:80), Neon Crossfire (:91), Firelight (:102), Loop (:113), Split (:124), Clamshell Beauty (:135), High Key (:146), Low Key Drama (:157), Uplight Horror (:168), Top Light (:179), Motivated Window (:190), Overcast Soft (:201), Teal & Orange (:212), Sci-Fi Cool (:223), Candlelight 2700K (:234), Tungsten 3200K (:245), Neutral 4500K (:256), Daylight 5600K (:267), Cool 6500K (:278), Moonlight 8000K (:289), Venetian Noir (:300), Window Light (:311), Prison Bars (:322), Dappled Forest (:333), Skylight Grid (:344), **Horror Underlight** (genre, :355), **Romance Soft** (genre, :369), **Sci-Fi Rim** (genre, :383), **Music-Video Neon** (genre, :397).

Brainstorm concepts **already covered**: Rembrandt, butterfly, Loop, Split, high-key beauty, low-key thriller, interrogation top-light, teal-&-orange, candlelit, moonlit night, overcast soft, tungsten interior, backlit silhouette, firelit warmth, sunset/daylight window (Motivated Window / Window Light), single-source hard noir, horror underlight, magenta/cyan gel cross (Music-Video Neon, Neon Crossfire).

---

## Part B — 16 new animated FX (ids 33–48)

Conventions used below: `fs` = phase units (`t/interval`), `step = floor(fs)`, `counter = (U64)step`, `L[i]` = `lights[i]`, EV −10 = idiomatic black, colour changes only via `mProfile`. All specs are pure functions of `(seed, t)`; every loop is via `positiveFmod`/`positiveMod`. **Seed-dependent** FX must be added to the `random_fx` list in test 6.

New table rows (append in this order):

```
FX_NAMES   += "Neon Buzz", "Welding Arc", "Candle Draft", "Sunrise Sweep",
              "Dying Bulb", "Rave Chase", "Lighthouse", "Night Train",
              "Fireworks Finale", "Passing Clouds", "Will-o'-Wisp",
              "Hologram Glitch", "Signal Lamp", "Breathing Swell",
              "Arcane Orbit", "Rock With You",
FX_INTERVALS += 0.05f, 0.05f, 0.20f, 0.25f, 0.10f, 0.125f, 0.10f, 0.10f,
              0.08f, 0.25f, 0.10f, 0.06f, 0.15f, 0.20f, 0.10f, 0.10f,
```

### FX 33 — Neon Buzz  *(interval 0.05 s, 8 s loop, seed-dependent)*
**Use:** a half-dead neon sign over the subject's shoulder — motel, dive bar, alley. Distinct from Neon Pulse (4), which is a clean smooth sine; this is duty-cycle sputter plus a periodic re-strike.
**Base (`initializeFX`):** `setLight(L,0, 60, 20, 15 /*Cyber Pink*/, 0, 0, true)` (the dying tube = KEY); `setLight(L,2, -120, 25, 16 /*Sci-Fi Cyan*/, -1, 0, true)` (the healthy tube = RIM).
**Temporal spec:**
```
cycle = positiveFmod(fs, 160.0)              // 8 s loop
// healthy tube: constant with a whisper of mains shimmer
L[2].mEV = -1 + 0.05 * phaseSin(fs * 1.2);
if (cycle < 12.0)                            // 0.6 s re-strike attempt at loop start
    L[0].mEV = (step & 1) ? 0.5f : -10.f;    // 10 Hz strobing strike
else {
    r = unitHash(seed, fx, counter, 0, 0);
    if      (r > 0.35) L[0].mEV = 0.3 + 0.15 * unitHash(seed, fx, counter, 0, 1); // lit
    else if (r > 0.20) L[0].mEV = -2.f;      // plasma half-glow
    else               L[0].mEV = -10.f;     // dropout
}
```

### FX 34 — Welding Arc  *(interval 0.05 s, 6 s loop, seed-dependent)*
**Use:** shipyard/garage scenes; harsh actinic blue stutter from below with hot-metal amber that heats up while the arc runs and cools between passes. Distinct from Thunderstorm (14): sustained low-angle stutter bursts, not sparse sky flashes.
**Base:** `setLight(L,0, 30, -25, 6 /*10000K Sky*/, -10, 0, true)` (arc); `setLight(L,1, 10, -35, 13 /*Deep Amber*/, -2.5, 1, true)` (glowing workpiece).
**Temporal spec:**
```
work = positiveFmod(fs, 120.0);              // 6 s cycle
if (work < 70.0) {                           // 3.5 s of welding
    r = unitHash(seed, fx, counter, 0, 0);
    L[0].mEV = (r > 0.25) ? 1.8f + 0.7f * unitHash(seed, fx, counter, 0, 1)
                          : -10.f;           // arc breaks ~25 % of ticks
    L[1].mEV = -2.5f + std::min(1.5, work / 70.0 * 1.5)     // metal heats
             + 0.1f * unitHash(seed, fx, counter, 1, 0);
} else {                                     // 2.5 s rest: arc off, metal cools
    L[0].mEV = -10.f;
    L[1].mEV = -1.f - (F32)((work - 70.0) / 50.0) * 2.f;
}
```

### FX 35 — Candle Draft  *(interval 0.2 s, non-repeating noise, seed-dependent)*
**Use:** a single candle in a drafty room — séance, vigil, period drama close-up. Distinct from Fire Flicker (2): smooth `valueNoise` flame (no stepped jumps), the flame *leans* (yaw/pitch sway), and slow gusts gutter it.
**Base:** `setLight(L,0, 10, -20, 0 /*2700K Incan*/, 0, 0, true)`; `setLight(L,1, -25, -10, 13 /*Deep Amber*/, -2, 1, true)` (warm room bounce).
**Temporal spec:**
```
n = valueNoise(seed, fx, fs * 0.9,  0, 0);                 // fast body flicker
L[0].mEV       = -0.4f + 0.8f * n;
L[0].mPitchDeg = -20.f + 6.f * (valueNoise(seed, fx, fs * 0.6, 0, 1) - 0.5f);
L[0].mYawDeg   =  10.f + 8.f * (valueNoise(seed, fx, fs * 0.6, 0, 2) - 0.5f);
g = valueNoise(seed, fx, fs * 0.15, 0, 3);                 // slow draft
if (g > 0.8f) L[0].mEV -= (g - 0.8f) * 7.5f;               // gust gutters up to −1.5 EV
L[1].mEV = -2.f + 0.32f * n;                               // bounce follows at 40 %
```

### FX 36 — Sunrise Sweep  *(interval 0.25 s, 60 s loop, analytic)*
**Use:** time-lapse dawn: the sun climbs and whitens from ember-red to daylight while the sky fill blues in. For establishing shots and "morning after" transitions; the loop's hard cut back to night is masked by a camera cut.
**Base:** `setLight(L,0, 170, 2, 0, -2.5, 0, true)` (sun, behind-left rising); `setLight(L,1, -10, 10, 8 /*Full CTB*/, -3.5, 1, true)` (pre-dawn sky); `setLight(L,3, 0, -20, 5 /*8000K Moon*/, -3, 0, true)` (night floor wash).
**Temporal spec:**
```
u = positiveFmod(fs, 240.0) / 240.0;         // 0..1 over 60 s
L[0].mPitchDeg = 2.f + (F32)u * 48.f;        // sun climbs 2° → 50°
L[0].mEV       = -2.5f + (F32)u * 3.f;       // −2.5 → +0.5
L[0].mProfile  = u < 0.20 ? 0 : u < 0.45 ? 17 : u < 0.70 ? 1 : 3;
                                             // 2700K → Golden Hour → 3200K → 5600K
L[1].mEV       = -3.5f + (F32)u * 2.5f;      // sky brightens
L[1].mProfile  = u < 0.60 ? 8 : 4;           // CTB night → 6500K morning
L[3].mEV       = -3.f + (F32)u * 2.f;
```

### FX 37 — Dying Bulb  *(interval 0.1 s, 20 s loop, seed-dependent)*
**Use:** the practical that gives out mid-scene: sag → sputter → pop → glowing filament → darkness → (loop) fresh bulb. Horror beats, power-cut gags. Distinct from Short Circuit (20, stationary sputter) and Haunted Flicker (9): this is a full life-cycle with a red filament afterglow.
**Base:** `setLight(L,0, 0, 65, 1 /*3200K Tung*/, 0.3, 0, true)` (overhead practical).
**Temporal spec:**
```
b = positiveFmod(fs, 200.0);
if (b < 120.0)                               // 12 s: slow sag with hum
    L[0].mEV = 0.3f - (F32)(b / 120.0) * 0.9f
             + 0.1f * (valueNoise(seed, fx, fs * 0.5, 0, 0) - 0.5f);
else if (b < 170.0) {                        // 5 s: sputter, worsening
    r = unitHash(seed, fx, counter, 0, 0);
    if      (r > 0.6f) L[0].mEV = -0.6f - (F32)((b - 120.0) / 50.0) * 1.5f;
    else if (r > 0.3f) L[0].mEV = -3.f;
    else               L[0].mEV = -10.f;
}
else if (b < 172.0) { L[0].mEV = 1.5f; L[0].mProfile = 23; }   // 0.2 s pop, white
else if (b < 180.0) {                        // 0.8 s filament afterglow
    L[0].mProfile = 11 /*Rosco Red*/;
    L[0].mEV = -4.f - (F32)(b - 172.0) * 0.75f;
}
else L[0].mOn = false;                       // dead until the loop replaces the bulb
```

### FX 38 — Rave Chase  *(interval 0.125 s, 4-step chase / 4 s colour lap, analytic)*
**Use:** warehouse rave / runway: a single hot beam **chases** around the subject at 8 Hz (one light at a time), colour advancing each step, with a unison crescendo blink every 4 s. Distinct from Club Strobe (1, random pops) and RGB Gamer (10, continuous co-rotation): this is a deterministic sequential chase.
**Base:** for `i` in 0..3: `setLight(L,i, yaws[i], 20, 7, -10, 0, true)` with `yaws = {45,-45,135,-135}`.
**Temporal spec:**
```
active = positiveMod(step, 4);
for i in 0..3:
    L[i].mProfile = 7 + (S32)positiveMod(step, 16);   // colour advances every step
    L[i].mEV      = (i == active) ? 1.0f : -10.f;
if (positiveMod(step, 32) == 31)             // unison blink each 4 s bar
    for i in 0..3: L[i].mEV = 1.2f;
```

### FX 39 — Lighthouse  *(interval 0.1 s, 6 s revolution, analytic)*
**Use:** coastal night: a narrow white beam sweeps a full circle and blasts the subject once per revolution; between passes only cold fog ambience. Distinct from Searchlight (15, smooth ±90° sine, always lit) and Warning Alert (12, constant-EV red beacon): the EV here follows a sharp cosine-power lobe.
**Base:** `setLight(L,0, 0, 8, 23 /*Pure White*/, -8, 2 /*Snoot*/, true)`; `setLight(L,1, -20, 10, 8 /*Full CTB*/, -3.5, 1, true)` (fog).
**Temporal spec:**
```
theta = positiveFmod(6.0 * fs, 360.0);       // 60°/s, one rev per 6 s
L[0].mYawDeg = wrap180((F32)theta);
c = std::max(0.f, phaseCos(theta * DEG_TO_RAD));
L[0].mEV = -8.f + 9.5f * std::pow(c, 24.f);  // ~±15° hot lobe, peak +1.5 EV
```

### FX 40 — Night Train  *(interval 0.1 s, 10 s loop, analytic)*
**Use:** the classic noir insert — a train passes outside; slatted window light strobes across the subject with an approach/recede loudness envelope, then silence. Distinct from Car Pass (26, one sweeping pair of lamps): rhythmic fixed-direction window flashes under a sine envelope, using the Slats gobo.
**Base:** `setLight(L,0, 90, 15, 2 /*4500K*/, -10, 0, true); L[0].mGobo = 4 /*Slats*/;` `setLight(L,1, 90, -15, 4 /*6500K*/, -10, 1, true)` (rolling-stock ambient).
**Temporal spec:**
```
b = positiveFmod(fs, 100.0);
if (b < 70.0) {                              // 7 s of train
    env = phaseSin(PI * b / 70.0);           // approach → recede
    w   = positiveFmod(fs, 3.0);             // one window per 0.3 s
    L[0].mEV = (w < 1.5) ? -1.f + 2.f * env : -6.f;   // 50 % duty strobe
    L[1].mEV = -3.5f + 1.f * env;            // low rumble glow
} else { L[0].mOn = false; L[1].mOn = false; }   // 3 s of night
```

### FX 41 — Fireworks Finale  *(interval 0.08 s, 2 s burst windows, seed-dependent)*
**Use:** festival / New Year scenes: coloured aerial bursts bloom above the subject and decay, several overlapping, over a steady night wash. Distinct from Paparazzi (5, instant white pops) and Explosion (27, one scripted event): each burst has a random colour, offset, and a ~0.8 s decay tail.
**Base:** `setLight(L,0, 40, 60, 7, -10, 0, false); setLight(L,1, -40, 60, 7, -10, 0, false);` `setLight(L,3, 180, 30, 20 /*Deep Space*/, -3, 0, true)` (night sky).
**Temporal spec (per shell light i in {0,1}):**
```
cycle = (U64)(step / 25);                    // 2 s launch window
local = step - (S64)cycle * 25;
if (unitHash(seed, fx, cycle, i, 0) < 0.7) {           // 70 % of windows fire
    o = (S64)(unitHash(seed, fx, cycle, i, 1) * 10.f); // burst offset 0–0.8 s
    if (local >= o) {
        ev = 1.8f - (F32)(local - o) * 0.28f;          // ~0.8 s to black
        if (ev > -6.f) {
            L[i].mOn = true;  L[i].mEV = ev;
            L[i].mProfile = 7 + (S32)(16.f * unitHash(seed, fx, cycle, i, 2));
            L[i].mYawDeg  = wrap180(L[i].mYawDeg +
                             60.f * (unitHash(seed, fx, cycle, i, 3) - 0.5f));
        }
    }
}
```

### FX 42 — Passing Clouds  *(interval 0.25 s, non-repeating noise, seed-dependent)*
**Use:** exterior realism: the sun slowly dips and returns as clouds drift over — the subtle exposure "breathing" of real daylight. Distinct from Underwater (7, fast sine) and every strobe: multi-second smoothed noise, no rhythm.
**Base:** `setLight(L,0, 40, 55, 3 /*5600K*/, 0.5, 0, true)` (sun); `setLight(L,1, -40, 20, 4 /*6500K*/, -2, 1, true)` (sky ambient).
**Temporal spec:**
```
cover = valueNoise(seed, fx, fs * 0.05, 0, 0);   // one noise cell ≈ 5 s
L[0].mEV = 0.5f - cover * 2.2f;                  // sun: full → −1.7 under cloud
if (cover > 0.8f) L[0].mProfile = 4;             // fully occluded: diffuse cool
L[1].mEV = -2.f - cover * 0.5f;                  // sky dims far less
```

### FX 43 — Will-o'-Wisp  *(interval 0.1 s, non-repeating noise, seed-dependent)*
**Use:** swamp/forest fantasy: a green spirit-light *wanders* around the subject in both yaw and pitch while breathing. Distinct from Fairy Woods (19, three static twinkling positions): a single moving source with a following ambient spill.
**Base:** `setLight(L,0, 0, 20, 21 /*Toxic Waste*/, -0.5, 2 /*Snoot*/, true)`; `setLight(L,1, 0, 10, 14 /*Emerald*/, -3, 1, true)` (spill).
**Temporal spec:**
```
L[0].mYawDeg   = 360.f * valueNoise(seed, fx, fs * 0.07, 0, 0) - 180.f;
L[0].mPitchDeg = -10.f + 50.f * valueNoise(seed, fx, fs * 0.09, 0, 1);
L[0].mEV = -0.5f + 0.8f * phaseSin(fs * 0.5)
         + 0.4f * (valueNoise(seed, fx, fs * 0.3, 0, 2) - 0.5f);
L[1].mYawDeg = L[0].mYawDeg * 0.5f;              // spill lags at half angle
L[1].mEV     = -3.f + 0.3f * phaseSin(fs * 0.5);
```

### FX 44 — Hologram Glitch  *(interval 0.06 s, seed-dependent)*
**Use:** sci-fi projection/AI-character shots: stable cyan scan-shimmer, but ~8 % of ticks the image "tears" — the key jumps sideways and a magenta chromatic ghost flashes at the mirrored offset; ~4 % of ticks it drops out entirely. Distinct from Short Circuit (20, EV only): this glitches *position and colour separation*.
**Base:** `setLight(L,0, 15, 10, 16 /*Sci-Fi Cyan*/, 0, 0, true)`; `setLight(L,1, -15, 5, 10 /*Magenta*/, -10, 0, false)` (ghost).
**Temporal spec:**
```
L[0].mEV = 0.15f * phaseSin(fs * 3.0);           // idle scanline shimmer
g = unitHash(seed, fx, counter, 0, 0);
if (g > 0.92f) {                                 // tear
    jump = 30.f * (unitHash(seed, fx, counter, 0, 1) - 0.5f);
    L[0].mYawDeg += jump;   L[0].mEV += 0.6f;
    L[1].mOn = true;  L[1].mYawDeg = -15.f - jump;  L[1].mEV = -0.5f;
} else if (g < 0.04f) L[0].mOn = false;          // dropout
```

### FX 45 — Signal Lamp  *(interval 0.15 s, 5.4 s loop, analytic)*
**Use:** naval/period drama: a distant Aldis lamp blinks S-O-S in real Morse timing across the water; deep-blue night holds between letters. Deterministic pattern — nothing like it in the current set.
**Base:** `setLight(L,0, 165, 5, 2 /*4500K*/, -10, 2 /*Snoot*/, true)` (lamp astern); `setLight(L,3, 0, -20, 20 /*Deep Space*/, -2.5, 0, true)` (night sea).
**Temporal spec:**
```
// 36-step cycle; on-steps: S(· · ·)=0,2,4  O(— — —)=8-10,12-14,16-18  S=22,24,26
static const U64 SOS_MASK =
    (1ull<<0)|(1ull<<2)|(1ull<<4)|
    (7ull<<8)|(7ull<<12)|(7ull<<16)|
    (1ull<<22)|(1ull<<24)|(1ull<<26);
b = positiveMod(step, 36);
L[0].mEV = (SOS_MASK >> b) & 1 ? 1.2f : -10.f;
```

### FX 46 — Breathing Swell  *(interval 0.2 s, 8 s period, analytic)*
**Use:** dream/memory/sleep sequences: the entire rig inhales and exhales in unison — a slow ±0.6 EV synchronized swell with a slight key rise on the inhale. Distinct from Fairy Woods (19, independent phases, colours) and Neon Pulse (4, two coloured lights): all four lights, neutral, one shared phase.
**Base:** `setLight(L,0, 45, 35, 1, 0, 1, true); setLight(L,1, -45, 10, 1, -1.5, 1, true); setLight(L,2, -135, 40, 5, -1.5, 0, true); setLight(L,3, 0, -20, 1, -2.5, 0, true)`.
**Temporal spec:**
```
s = phaseSin(fs * 0.157);                        // 2π / 40 steps = 8 s period
for i in 0..3: L[i].mEV += 0.6f * s;
L[0].mPitchDeg = 35.f + 2.f * s;                 // key lifts as the scene inhales
```

### FX 47 — Arcane Orbit  *(interval 0.1 s, 5 s revolution, analytic)*
**Use:** spell-casting / power-up shots: purple and cyan sources **counter-rotate** around the subject and flash when they cross (front and back), like orbiting energy. Distinct from RGB Gamer/Disco (co-rotation) and Warp Tunnel (pitch sweeps): opposed orbits with crossing events.
**Base:** `setLight(L,0, 0, 15, 22 /*Neon Purple*/, -0.2, 0, true); setLight(L,1, 180, 15, 16 /*Sci-Fi Cyan*/, -0.2, 0, true); setLight(L,3, 0, -20, 20 /*Deep Space*/, -2.5, 0, true)`.
**Temporal spec:**
```
L[0].mYawDeg = wrap180((F32)positiveFmod( 7.2 * fs, 360.0));         // CW
L[1].mYawDeg = wrap180((F32)positiveFmod(-7.2 * fs + 180.0, 360.0)); // CCW
base = -0.2f + 0.3f * phaseSin(fs * 0.3);
L[0].mEV = L[1].mEV = base;
d = positiveFmod(14.4 * fs, 180.0);              // relative angle collapses twice/rev
if (d < 10.0 || d > 170.0) { L[0].mEV = base + 1.f; L[1].mEV = base + 1.f; }
```

### FX 48 — Rock With You  *(interval 0.1 s, component periods 4 s / 3 s / ~0.86 s / 16 s, seed-dependent)*
**Use:** the Michael Jackson *Rock With You* (1979) music-video look — a hard cool spotlight pins the performer from directly overhead while green and blue laser beams sweep and starburst around them through haze, deep black everywhere else. Distinct from Arcane Orbit (47, front-height purple/cyan energy orbit with no key): here the subject is *keyed* by a steady overhead snoot and the counter-rotating beams are low, laser-tight, haze-shimmering accents.
**Light mapping:** KEY = the overhead performer spot; RIM = green sweeping laser; BG = blue counter-sweeping laser; FILL = killed for the void.
**Base (`initializeFX`):**
```
setLight(L,0,   0, 82,  4 /*6500K Cool*/,  0.5, 2 /*Snoot*/, true);   // overhead spot
setLight(L,1, -45, 10, 20 /*Deep Space*/, -10., 0,           false);  // FILL dead: the void
setLight(L,2,  90,  5, 14 /*Emerald*/,    -0.5, 2 /*Snoot*/, true);   // green laser
setLight(L,3, -90,  8, 16 /*Sci-Fi Cyan*/,-0.5, 2 /*Snoot*/, true);   // blue laser
```
**Temporal spec:**
```
// KEY: steady with a slow disco breath (±0.15 EV, 4 s period)
L[0].mEV = 0.5f + 0.15f * phaseSin(fs * 0.157);              // 2π/40 steps

// RIM green laser: continuous CW sweep, one rev per 4 s, pitch fans up/down
L[2].mYawDeg   = wrap180((F32)positiveFmod(9.0 * fs, 360.0));      // 9°/step
L[2].mPitchDeg = 5.f + 18.f * phaseSin(fs * 0.35);

// BG blue laser: counter-rotating, faster (one rev per 3 s), opposite fan phase
L[3].mYawDeg   = wrap180((F32)positiveFmod(-12.0 * fs + 180.0, 360.0)); // −12°/step
L[3].mPitchDeg = 8.f + 14.f * phaseCos(fs * 0.27);

// laser shimmer through haze: smooth per-light noise (seed-dependent)
L[2].mEV = -0.5f + 0.4f * (valueNoise(seed, fx, fs * 0.8, 2, 0) - 0.5f);
L[3].mEV = -0.5f + 0.4f * (valueNoise(seed, fx, fs * 0.8, 3, 0) - 0.5f);

// starburst: relative yaw closes at 21°/step; flare both beams as they cross
d = positiveFmod(21.0 * fs, 180.0);          // a crossing every ~0.86 s
if (d < 6.0 || d > 174.0) { L[2].mEV += 0.8f; L[3].mEV += 0.8f; }

// slow green<->blue hue drift: swap laser profiles every 8 s (16 s full cycle)
if (positiveMod(step / 80, 2) == 1) { L[2].mProfile = 16; L[3].mProfile = 14; }
```
All components are individually periodic and closed-form (no master loop needed); `valueNoise` makes it **seed-dependent** (add 48 to `random_fx`).

> **Operator note (must ship in the FX description / user docs):** the sweeping beams are only *visible as beams* when the per-light **volumetric shafts** are enabled and each projector holds a shadow slot — an FX can only drive `LightBase` fields and cannot toggle `mShaftEnabled` (shafts are per-rig operator state, `ALCineLightRig::setShaftEnabled`, `alcinelightrig.cpp:560-568`; shafts additionally require the projector not be flagged no-shadow, see the shaft/shadow diagnostic at `alcinelightrig.cpp:1003-1027`). For the full look: turn **Shafts ON for KEY, RIM and BG**, and set the rig **Shadow policy to mode 2 "all projectors"** (`updateShadowPolicy`, `alcinelightrig.cpp:956-972` — mode 0 suppresses all, mode 1 keeps key only, mode 2 lets every projector cast) so all three beams get shadow slots. In a hazy scene the KEY reads as the overhead cone and RIM/BG read as swept laser blades.

### B.1 FX implementation note (for Codex)

All 16 fit the existing system with **no schema or capability change**:

1. `indra/newview/alcinelightrigmodel.h:22` — `FX_COUNT` 33 → **49**.
2. `indra/newview/alcinelightrigmodel.cpp:119-129` — append the 16 names to `FX_NAMES` (order above).
3. `alcinelightrigmodel.cpp:131-137` — append the 16 intervals to `FX_INTERVALS` (order above).
4. `alcinelightrigmodel.cpp:318-455` — add `case 33: … case 48:` to `initializeFX` with the base poses above (use `setLight`; assign `mGobo` directly for FX 40).
5. `alcinelightrigmodel.cpp:933-1429` — add the matching cases to the `evalFX` switch, using only `phaseSin/phaseCos/positiveFmod/positiveMod/unitHash/valueNoise/std::pow/wrap180`. Never touch `mGel` (operator gel is re-layered at `alcinelightrig.cpp:1806-1811`).
6. Tests `indra/newview/tests/alcinelightrigmodel_test.cpp`:
   * `:896-897` — both `FX_COUNT` golden asserts 33 → **49**.
   * `:964-974` and `:980-986` — extend the mirrored golden name/interval tables identically.
   * `:732-734` — extend `random_fx[]` with the seed-dependent ids **{33, 34, 35, 37, 41, 42, 43, 44, 48}** (36, 38, 39, 40, 45, 46, 47 are analytic and must remain seed-invariant).
7. No UI work: the FX combo populates from `FX_COUNT` (`alpanelcinelightrig.cpp:316-318`).

Design constraints honoured: EV stays within the existing −10..+3 idiom; loops are closed-form (scrub-safe, replay-safe per tests 5/7); per-light phase independence is achieved via hash `light`/`draw` arguments and phase offsets — the "per-light independent phase" capability already exists, so **no evalFX extension is needed**.

---

## Part C — 15 new cinematic presets

All are complete `Setup`s in the exact XML vocabulary of `cine_light_rig_presets.xml` (§A.3). Lights are listed **KEY / FILL / RIM / BG** in order. Fields omitted in the tables use the schema defaults (`ratio_lock` false, `ratio_stops` 0, `gobo_idx` 0 "Default", `gel_idx` 0 "None"). Presets marked **[Genre / Mood]** carry `<key>category</key><string>Genre / Mood</string>`. Names verified unique against §A.4 (the loader rejects duplicates, `alcinelightrig.cpp:2076-2091`).

### C.1 Short Side Portrait
**Intent:** "Key from beyond the far cheek; the camera side falls into shadow — the slimming, editorial complement to Loop and Rembrandt."
`radius 1.4`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 110 | 42 | 2 "4500K Neut" | 0.5 | 1 "Softbox" | true | — | — |
| FILL | −35 | 8 | 2 "4500K Neut" | −2.4 | 1 "Softbox" | true | — | — |
| RIM | −140 | 40 | 4 "6500K Cool" | −1.4 | 2 "Snoot" | true | — | — |
| BG | 160 | −12 | 2 "4500K Neut" | −2.6 | 0 "Standard" | false | — | — |

### C.2 Caravaggio  **[Genre / Mood]**
**Intent:** "One hard warm shaft from high side, five-stop chiaroscuro plunge, background swallowed by black — Baroque tenebrism."
`radius 1.5`, `ratio_lock true`, `ratio_stops 5.0`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 65 | 55 | 1 "3200K Tung" | 0.75 | 0 "Standard" | true | — | 9 "Bastard Amber" |
| FILL | −45 | 5 | 0 "2700K Incan" | −4.25 | 1 "Softbox" | true | — | — |
| RIM | −140 | 30 | 1 "3200K Tung" | −3.0 | 2 "Snoot" | false | — | — |
| BG | 170 | −25 | 20 "Deep Space" | −3.5 | 0 "Standard" | true | — | — |

(FILL EV is derived by the lock; the stored value documents the intent.)

### C.3 Clinical Morgue
**Intent:** "Fluorescent green-tinged top light, cold floor bounce, steel-blue edge — autopsy rooms, cold opens, bad news."
`radius 1.7`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 0 | 78 | 4 "6500K Cool" | 0.5 | 1 "Softbox" | true | — | 7 "Plus Green" |
| FILL | 0 | −25 | 6 "10000K Sky" | −2.8 | 1 "Softbox" | true | — | — |
| RIM | 180 | 35 | 4 "6500K Cool" | −2.0 | 0 "Standard" | true | — | 10 "Steel Blue" |
| BG | 160 | 0 | 6 "10000K Sky" | −2.2 | 1 "Softbox" | true | — | — |

### C.4 Sodium Vapor Night  **[Genre / Mood]**
**Intent:** "Overhead sodium streetlamp pools amber over the subject; the night beyond is dead blue — the Fincher parking-lot look. Pairs with the Streetlight FX."
`radius 1.6`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 15 | 65 | 13 "Deep Amber" | 0.25 | 0 "Standard" | true | — | — |
| FILL | −40 | 5 | 8 "Full CTB" | −3.25 | 1 "Softbox" | true | — | — |
| RIM | 170 | 30 | 13 "Deep Amber" | −1.75 | 2 "Snoot" | true | — | — |
| BG | −170 | −15 | 20 "Deep Space" | −2.5 | 0 "Standard" | true | — | — |

### C.5 Ethereal Halo  **[Genre / Mood]**
**Intent:** "Blown white halo from behind, near-shadowless cool wrap in front — dream sequences, afterlife, memory. Distinct from High Key: the exposure lives in the backlight."
`radius 1.6`, `ratio_lock true`, `ratio_stops 0.75`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 20 | 30 | 3 "5600K Day" | 0.25 | 1 "Softbox" | true | — | 6 "1/4 CTB" |
| FILL | −20 | 20 | 3 "5600K Day" | −0.5 | 1 "Softbox" | true | — | — |
| RIM | 180 | 50 | 23 "Pure White" | 1.75 | 1 "Softbox" | true | — | — |
| BG | 165 | 20 | 8 "Full CTB" | −0.75 | 1 "Softbox" | true | — | — |

### C.6 Solo Spotlight
**Intent:** "A single tight follow-spot from the house, faint Congo-blue stage behind — stand-up, torch song, curtain call."
`radius 2.2`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 12 | 42 | 23 "Pure White" | 1.25 | 2 "Snoot" | true | — | — |
| FILL | −30 | 10 | 2 "4500K Neut" | −5.0 | 1 "Softbox" | false | — | — |
| RIM | −168 | 55 | 12 "Congo Blue" | −2.5 | 2 "Snoot" | true | — | — |
| BG | 180 | −10 | 12 "Congo Blue" | −3.5 | 0 "Standard" | true | — | — |

### C.7 Ringside Kickers
**Intent:** "Boxing-ring geometry: hard overhead array, canvas bounce from below, twin hard white kickers carving both shoulders — sweat-and-glory sports drama."
`radius 1.9`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 0 | 75 | 2 "4500K Neut" | 0.75 | 0 "Standard" | true | — | — |
| FILL | 0 | −30 | 2 "4500K Neut" | −2.5 | 1 "Softbox" | true | — | — |
| RIM | 135 | 25 | 23 "Pure White" | 0.0 | 2 "Snoot" | true | — | — |
| BG | −135 | 25 | 23 "Pure White" | 0.0 | 2 "Snoot" | true | — | — |

(The BG channel is repurposed as the second kicker — legal: roles are conventions, not constraints.)

### C.8 Ring Light Beauty
**Intent:** "Close frontal ring: on-axis soft key plus lower-ring fill, LED wall behind — the creator-studio look. Enable the rig catchlight for the signature eye ring."
`radius 0.9`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 0 | 5 | 3 "5600K Day" | 0.25 | 1 "Softbox" | true | — | — |
| FILL | 0 | −12 | 3 "5600K Day" | −1.0 | 1 "Softbox" | true | — | — |
| RIM | 180 | 40 | 4 "6500K Cool" | −1.5 | 1 "Softbox" | true | — | — |
| BG | 170 | 0 | 18 "Vaporwave" | −1.75 | 0 "Standard" | true | — | — |

### C.9 Cathedral Shaft  **[Genre / Mood]**
**Intent:** "A warm sunbeam through leaded panes cuts the stone gloom from high; Congo-blue darkness everywhere else — confessions, weddings, vigils."
`radius 2.4`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 55 | 62 | 2 "4500K Neut" | 1.0 | 0 "Standard" | true | 2 "Window Panes" | 2 "1/2 CTO" |
| FILL | −35 | 8 | 12 "Congo Blue" | −3.75 | 1 "Softbox" | true | — | — |
| RIM | −160 | 45 | 2 "4500K Neut" | −2.25 | 2 "Snoot" | true | — | 2 "1/2 CTO" |
| BG | 175 | −20 | 20 "Deep Space" | −3.0 | 0 "Standard" | true | — | — |

### C.10 Amber Fog  **[Genre / Mood]**
**Intent:** "Monochrome irradiated amber from every direction, deliberately low contrast — the Blade Runner 2049 Vegas wasteland."
`radius 1.8`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 30 | 35 | 13 "Deep Amber" | 0.5 | 1 "Softbox" | true | — | — |
| FILL | −35 | 10 | 7 "Full CTO" | −1.0 | 1 "Softbox" | true | — | — |
| RIM | 175 | 25 | 13 "Deep Amber" | 0.25 | 1 "Softbox" | true | — | — |
| BG | 160 | 0 | 7 "Full CTO" | −0.5 | 1 "Softbox" | true | — | — |

### C.11 Underwater Depths  **[Genre / Mood]**
**Intent:** "Cyan surface light dappled by caustics from above, Congo-blue depth fill, cold void behind. Pair with the Underwater FX for motion."
`radius 1.7`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 10 | 70 | 16 "Sci-Fi Cyan" | 0.25 | 0 "Standard" | true | 6 "Soft Dapple" | — |
| FILL | −45 | −10 | 12 "Congo Blue" | −2.5 | 1 "Softbox" | true | — | — |
| RIM | 170 | 40 | 8 "Full CTB" | −1.25 | 0 "Standard" | true | — | — |
| BG | −170 | −20 | 20 "Deep Space" | −1.75 | 1 "Softbox" | true | — | — |

### C.12 High Noon
**Intent:** "Brutal near-vertical sun, hot sand bounce from below, background baked bright — western stand-offs and desert exteriors."
`radius 1.8`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 5 | 72 | 3 "5600K Day" | 1.25 | 0 "Standard" | true | — | — |
| FILL | 0 | −35 | 13 "Deep Amber" | −2.25 | 1 "Softbox" | true | — | — |
| RIM | 180 | 20 | 3 "5600K Day" | −3.0 | 0 "Standard" | false | — | — |
| BG | 170 | 10 | 3 "5600K Day" | −0.75 | 0 "Standard" | true | — | — |

### C.13 Fluorescent Office
**Intent:** "Grid-diffused ceiling tubes with the Plus Green sickness, a window's daylight spilling from the side — cubicle realism. Pairs with the Elevator Fault FX."
`radius 1.6`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 8 | 70 | 2 "4500K Neut" | 0.0 | 1 "Softbox" | true | 5 "Grid" | 7 "Plus Green" |
| FILL | −12 | 55 | 2 "4500K Neut" | −0.75 | 1 "Softbox" | true | — | 7 "Plus Green" |
| RIM | 178 | 60 | 2 "4500K Neut" | −1.25 | 1 "Softbox" | true | — | 7 "Plus Green" |
| BG | 90 | −10 | 4 "6500K Cool" | −2.0 | 0 "Standard" | true | — | — |

### C.14 Rainy Night Window  **[Genre / Mood]**
**Intent:** "Cold street light through window panes, blue night ambience, a neon sign's pink bleeding in from outside — insomnia, stake-outs, city apartments at 3 a.m."
`radius 1.5`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 70 | 30 | 4 "6500K Cool" | 0.25 | 0 "Standard" | true | 2 "Window Panes" | — |
| FILL | −30 | 5 | 6 "10000K Sky" | −3.0 | 1 "Softbox" | true | — | — |
| RIM | −155 | 35 | 15 "Cyber Pink" | −1.5 | 2 "Snoot" | true | — | — |
| BG | 150 | −12 | 20 "Deep Space" | −2.25 | 0 "Standard" | true | — | — |

### C.15 Rock With You  **[Genre / Mood]**
**Intent:** "1979 soundstage disco: a hard cool spot from straight overhead, green and blue laser blades low around the performer, black void beyond. Companion to the Rock With You FX (48) — this static setup holds the look between takes; enable per-light Shafts on Key/Rim/Bg and Shadow policy 'all projectors' so the beams read through haze."
`radius 2.0`
| light | yaw | pitch | profile | ev | beam | on | gobo | gel |
|---|---|---|---|---|---|---|---|---|
| KEY | 0 | 82 | 4 "6500K Cool" | 0.75 | 2 "Snoot" | true | — | — |
| FILL | −45 | 10 | 20 "Deep Space" | −6.0 | 0 "Standard" | false | — | — |
| RIM | 120 | 8 | 14 "Emerald" | −0.25 | 2 "Snoot" | true | — | — |
| BG | −120 | 12 | 16 "Sci-Fi Cyan" | −0.25 | 2 "Snoot" | true | — | — |

(Static beam positions are staggered ±120° so the frozen look already reads as crossing lasers; the FX takes over the sweep when enabled. Colour is pure profile — no gels — so the operator's own gel choices layer cleanly if the FX is running.)

### C.16 Preset implementation note (for Codex)

* **Where:** append 15 `<map>` entries to the `presets` array of `indra/newview/app_settings/cine_light_rig_presets.xml`, following the exact LLSD shape of the existing entries (e.g. the gel-carrying Romance Soft entry at `:369-381` and the gobo-carrying Venetian Noir entry at `:300-309`). Keep `version` at `1`. Emit **both** `*_idx` and `*_name` keys for profile/beam/gobo/gel (names must byte-match the tables in `alcinelightrigmodel.cpp:61-117`, `96-112`; on mismatch the loader trusts the name, `alcinelightrig.cpp:390-437`).
* **No code change:** `masterSetups()` (`alcinelightrig.cpp:2016-2185`) picks up new entries automatically; `category = "Genre / Mood"` routes an entry to the genre section of the combo (`:2097-2099`, `2215-2232`). New names are unique against §A.4, so the duplicate-skip and user-preset-rename paths are not triggered spuriously.
* **Schema fit:** every value above is inside the sanitized ranges (yaw wraps, pitch ≤ 85, EV within [−20,20], radius ≥ 0.5, ratio ≤ 5). Nothing uses `Globals`/`Transforms` fields, which presets cannot carry (§A.3 scope note) — colour temperature is expressed with profiles + temperature gels instead of the master mired trim.
* **Optional test:** no golden test covers the XML contents (only the loader's robustness), so no test change is required for Part C. If desired, a sanity pass loading the file through `setupFromLLSD` in `alcinelightrigmodel_test.cpp` style would be new coverage, not a requirement.

---

## Summary of required edits (all of Part B + C)

| file | change |
|---|---|
| `indra/newview/alcinelightrigmodel.h:22` | `FX_COUNT = 49` |
| `indra/newview/alcinelightrigmodel.cpp` | +16 rows `FX_NAMES` (:119), +16 rows `FX_INTERVALS` (:131), +16 cases `initializeFX` (:318), +16 cases `evalFX` (:933) |
| `indra/newview/tests/alcinelightrigmodel_test.cpp` | goldens 33→49 (:896-897), mirrored name/interval tables (:964, :980), `random_fx` += {33,34,35,37,41,42,43,44,48} (:732) |
| `indra/newview/app_settings/cine_light_rig_presets.xml` | +15 preset maps (6 with `category` "Genre / Mood") |

No new model capability, no schema version bump, no XUI change.
