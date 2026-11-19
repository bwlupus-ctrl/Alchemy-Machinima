# Cine Light Rig — Easy Mode Preset Normalization (4-way presence, rev 2)

## Method

Each preset is re-expressed in easy-native exposure form with **geometry untouched** (yaw/pitch/profile/beam/gobo/gel identical). Because presets today carry no master EV, the old effective Key brightness is simply the old Key `ev`; Easy Mode anchors Key EV = 0, so we set **`master_ev` = old Key EV** (adjusted only where intent demands it, e.g. Silhouette). **Drama** (`ratio_lock=true`, `ratio_stops`) reproduces the old Key−Fill spread: reuse authored `ratio_stops` where a lock existed, else `clamp(oldKey − oldFill, 0, 5)` when Fill was on, else a mood-fitted value (noir/single-source ≈ 4–5, glamour/broadcast ≈ 1.5–2.5, flat ≈ ≤1). **Rim/BG presence** now uses the **4-way buckets** — Rim: Off / Faint (−2.5) / Subtle (−1.0) / Strong (+0.5); BG: Off / Faint (−3.5) / Subtle (−2.0) / Strong (−0.5) — chosen from the light's old EV *relative to the old Key* (rel = oldEV − oldKeyEV) via the READ thresholds (Rim: <−1.75 Faint, <−0.25 Subtle, else Strong; BG: <−2.75 Faint, <−1.25 Subtle, else Strong), which also select the nearest bucket EV; exact-midpoint ties resolve to the brighter bucket per the threshold rule. All values round-trip: presets are written with the exact bucket EVs, so the READ mapping recovers the same presence; masters are within −8..+8 and dramas within 0..5. Residual quantization error per row is in the notes; the appendix lists the exact per-light values to write, so the implementation is mechanical.

## Normalization table

Old columns show EV and on/off for each light: Key / Fill / Rim / Bg. "(off)" means the light was disabled (its EV is the stored fallback). rel = old EV minus old Key EV. Errors quoted as hot/dim vs the authored relative EV.

| Preset | Category | old KeyEV / FillEV / RimEV(on) / BgEV(on) | → master_ev | drama (ratio_stops) | rim_presence | bg_presence | notes / compromises |
|---|---|---|---|---|---|---|---|
| Rembrandt | — | 0.5 / −2.5 / −1.0 / −2.0(off) | **0.5** | **3.0** | Subtle | Off | Rim rel −1.5 → Subtle 0.5 hot. |
| Paramount Butterfly | — | 0.5 / −2.0 / −1.0 / −2.0(off) | **0.5** | **2.5** | Subtle | Off | Rim rel −1.5 → Subtle 0.5 hot. |
| Broadcast Interview | — | 0.0 / −1.0 / −0.5 / −1.5 | **0.0** | **1.0** | Subtle | Subtle | Rim rel −0.5 → 0.5 dim; bg rel −1.5 → 0.5 dim. Wash stays behind the subject. |
| Film Noir | Genre / Mood | 1.0 / −4.0(off) / −1.5 / −3.0(off) | **1.0** | **4.0** (reused authored lock) | **Faint** | Off | Rim rel −2.5 = Faint **exactly** — the authored hair-snoot dimness now survives (was 1.5 hot under 3-way). |
| Silhouette | — | −6.0(OFF) / −6.0(off) / +1.5 / 0.0 | **1.0** | **5.0** | Strong | Strong | **SPECIAL — Key was OFF.** See special cases. master +1.0 anchors the backlight: Strong rim = +1.5 abs (old exactly), Strong bg = +0.5 abs (old 0.0). Easy forces Key on, so the subject front renders at +1.0 — a hot backlit portrait, not a true silhouette. |
| Golden Hour | Genre / Mood | 1.0 / −1.5 / −2.0(off) / −2.0 | **1.0** | **2.5** (reused authored lock) | Off | **Faint** | Bg rel −3.0 → Faint 0.5 dim (was 1.0 hot under 3-way Subtle). CTB counter-wash now slightly gentler than authored. |
| Blue Hour Moon | — | 0.0 / −2.5 / −0.5 / −2.5(off) | **0.0** | **2.5** | Subtle | Off | Rim rel −0.5 (warm practical) → 0.5 dim. |
| Neon Crossfire | — | 0.5 / 0.5 / −0.5 / −2.0(off) | **0.5** | **0.0** | Subtle | Off | Key≈fill duotone → drama 0. Rim rel −1.0 = Subtle exact. |
| Firelight | — | 0.5 / −1.5 / −1.5 / −2.5(off) | **0.5** | **2.0** | **Faint** | Off | Rim rel −2.0 → Faint 0.5 dim (was 1.0 hot); the cool night rim stays a whisper behind the flicker. |
| Loop | — | 0.5 / −1.8 / −1.2 / −2.2(off) | **0.5** | **2.3** | Subtle | Off | Rim rel −1.7 → Subtle 0.7 hot (Faint would be 0.8 dim; threshold keeps Subtle). |
| Split | — | 0.5 / −3.0 / −1.0 / −2.5(off) | **0.5** | **3.5** | Subtle | Off | Rim rel −1.5 → 0.5 hot. |
| Clamshell Beauty | — | 0.5 / −1.0 / −1.0 / −2.0(off) | **0.5** | **1.5** | Subtle | Off | Rim rel −1.5 → 0.5 hot. |
| High Key | — | 0.5 / 0.0 / −0.5 / −0.5 | **0.5** | **0.5** | Subtle | Strong | Rim rel −1.0 exact; bg rel −1.0 → Strong 0.5 hot — right for a lit background. |
| Low Key Drama | — | 1.0 / −2.5(off) / −1.5 / −3.0(off) | **1.0** | **4.5** (fill off; mood-fit) | **Faint** | Off | Rim rel −2.5 = Faint **exactly** — the authored "cool whisper rim" is preserved verbatim. |
| Uplight Horror | — | 0.5 / −2.5(off) / −1.5 / −2.5(off) | **0.5** | **4.0** (fill off; mood-fit) | **Faint** | Off | Rim rel −2.0 → Faint 0.5 dim. |
| Top Light | — | 0.5 / −2.5(off) / −1.5 / −2.5(off) | **0.5** | **4.0** (fill off; mood-fit) | **Faint** | Off | Rim rel −2.0 → Faint 0.5 dim; interrogation edge stays restrained. |
| Motivated Window | — | 0.5 / −2.0 / −1.5(off) / −2.0 | **0.5** | **2.5** | Off | Subtle | Bg rel −2.5 → Subtle 0.5 hot (Faint would be 1.0 dim). |
| Overcast Soft | — | 0.0 / −0.8 / −1.2 / −1.5 | **0.0** | **0.8** | Subtle | Subtle | Rim rel −1.2 → 0.2 hot; bg rel −1.5 → 0.5 dim. Low-contrast look preserved. |
| Teal &amp; Orange | — | 0.5 / −2.0 / −1.0 / −2.0(off) | **0.5** | **2.5** | Subtle | Off | Rim rel −1.5 → 0.5 hot. |
| Sci-Fi Cool | — | 0.5 / −2.5 / −0.5 / −2.0(off) | **0.5** | **3.0** | Subtle | Off | Rim rel −1.0 = Subtle exact. |
| Candlelight 2700K | — | 0.5 / −1.5 / −1.0 / −2.0(off) | **0.5** | **2.0** | Subtle | Off | Rim rel −1.5 → 0.5 hot. |
| Tungsten 3200K | — | 0.5 / −1.5 / −1.0 / −2.0(off) | **0.5** | **2.0** | Subtle | Off | Same family as Candlelight; identical quantization. |
| Neutral 4500K | — | 0.5 / −1.5 / −1.0 / −2.0(off) | **0.5** | **2.0** | Subtle | Off | Same family; identical quantization. |
| Daylight 5600K | — | 0.5 / −1.5 / −1.0 / −2.0(off) | **0.5** | **2.0** | Subtle | Off | Same family; identical quantization. |
| Cool 6500K | — | 0.5 / −1.5 / −1.0 / −2.0(off) | **0.5** | **2.0** | Subtle | Off | Same family; identical quantization. |
| Moonlight 8000K | — | 0.5 / −1.5 / −1.0 / −2.0(off) | **0.5** | **2.0** | Subtle | Off | Same family; identical quantization. |
| Venetian Noir | — | 1.0 / −4.0(off) / −1.0 / −3.0(off) | **1.0** | **4.5** (fill off; noir-fit between Film Noir 4.0 and fallback spread 5.0) | **Faint** | Off | Rim rel −2.0 → Faint 0.5 dim. Blinds gobo untouched. |
| Window Light | — | 0.5 / −2.0 / −1.5(off) / −2.0 | **0.5** | **2.5** | Off | Subtle | Bg rel −2.5 → Subtle 0.5 hot. |
| Prison Bars | — | 1.0 / −3.0(off) / −1.0 / −2.5(off) | **1.0** | **4.0** (fill off; matches fallback spread) | **Faint** | Off | Rim rel −2.0 → Faint 0.5 dim on the white rim. |
| Dappled Forest | — | 0.5 / −2.0 / −1.0 / −2.0 | **0.5** | **2.5** | Subtle | Subtle | Rim rel −1.5 → 0.5 hot; bg rel −2.5 → 0.5 hot. |
| Skylight Grid | — | 0.5 / −2.0 / −1.5(off) / −2.0(off) | **0.5** | **2.5** | Off | Off | Clean two-light preset; exact round-trip. |
| Horror Underlight | Genre / Mood | 1.0 / −4.0(off) / −1.0 / −2.0 | **1.0** | **4.0** (reused authored ratio_stops; lock was false but value matches intent) | **Faint** | **Faint** | Rim rel −2.0 → Faint 0.5 dim — the "restrained blood rim" stays restrained (was 1.0 hot). Bg (Deep Space) rel −3.0 → Faint 0.5 dim. |
| Romance Soft | Genre / Mood | −0.25 / −1.25 / −1.75 / −3.0 | **−0.25** | **1.0** (reused authored lock) | Subtle | Subtle | Rim rel −1.5 → 0.5 hot. Bg rel −2.75 is the exact Faint/Subtle midpoint; threshold resolves to Subtle (0.75 hot; Faint would be 0.75 dim). |
| Sci-Fi Rim | Genre / Mood | −2.5 / −4.0(off) / +1.5 / +0.25 | **1.0** | **3.0** (reused authored ratio_stops) | Strong | Strong | **SPECIAL — see special cases.** master +1.0 makes Strong rim +1.5 abs (old exactly) and Strong bg +0.5 abs (old +0.25), but the Key, forced on at 0, renders 3.5 stops hotter than the authored dark centre. Advanced-only. |
| Music-Video Neon | Genre / Mood | 0.75 / 0.5 / 0.0 / −1.0 | **0.75** | **0.25** | Subtle | Subtle | Rim rel −0.75 → 0.25 dim; bg rel −1.75 → 0.25 dim. Very close. |
| Short Side Portrait | — | 0.5 / −2.4 / −1.4 / −2.6(off) | **0.5** | **2.9** | **Faint** | Off | Rim rel −1.9 → Faint 0.6 dim (Subtle would be 0.9 hot); the editorial edge softens slightly. |
| Caravaggio | Genre / Mood | 0.75 / −4.25 / −3.0(off) / −3.5 | **0.75** | **5.0** (reused authored lock) | Off | **Faint** | Bg (Deep Space) rel −4.25 → Faint 0.75 hot (was 2.25 hot under 3-way). The swallowed-black void now essentially survives. |
| Clinical Morgue | — | 0.5 / −2.8 / −2.0 / −2.2 | **0.5** | **3.3** | **Faint** | Subtle | Rim rel −2.5 = Faint **exactly** — steel-blue edge preserved verbatim. Bg rel −2.7 → Subtle 0.7 hot (just brighter than the Faint midpoint). |
| Sodium Vapor Night | Genre / Mood | 0.25 / −3.25 / −1.75 / −2.5 | **0.25** | **3.5** | **Faint** | Subtle | Rim rel −2.0 → Faint 0.5 dim. Bg (Deep Space) rel −2.75 is the exact midpoint; threshold resolves to Subtle (0.75 hot). |
| Ethereal Halo | Genre / Mood | 0.25 / −0.5 / +1.75 / −0.75 | **0.25** | **0.75** (reused authored lock) | Strong | Strong | **Residual clip.** Halo authored rel +1.5; Strong caps at +0.5, so the blown halo still loses ~1 stop (reads bright, not blown). Bg rel −1.0 → Strong 0.5 hot. Only remaining candidate for a hotter "Blast" rim bucket. |
| Solo Spotlight | — | 1.25 / −5.0(off) / −2.5 / −3.5 | **1.25** | **5.0** (fill off; spot vs black house) | **Faint** | **Faint** | Rim (Congo stage glow) rel −3.75 → Faint 1.25 hot; bg rel −4.75 → Faint 1.25 hot. Much improved from 2.75 hot under 3-way; the house comes up ~1.25 stops but Congo Blue's dim saturation masks most of it. |
| Ringside Kickers | — | 0.75 / −2.5 / 0.0 / 0.0 | **0.75** | **3.25** | Subtle | Strong | Light 3 is the second white kicker, not a wash. Rim rel −0.75 → Subtle 0.25 dim; bg-slot kicker rel −0.75 → Strong 0.25 hot. The authored-equal twin kickers become 0.5 stops unequal (−1.0 vs −0.5) — cosmetic. |
| Ring Light Beauty | — | 0.25 / −1.0 / −1.5 / −1.75 | **0.25** | **1.25** | Subtle | Subtle | Rim rel −1.75 is the exact Faint/Subtle midpoint; threshold resolves to Subtle (0.75 hot). Bg (Vaporwave wall) rel −2.0 = Subtle exact. |
| Cathedral Shaft | Genre / Mood | 1.0 / −3.75 / −2.25 / −3.0 | **1.0** | **4.75** | **Faint** | **Faint** | Rim (warm counter-snoot) rel −3.25 → Faint 0.75 hot; bg (Deep Space) rel −4.0 → Faint 0.5 hot. The sunbeam-vs-gloom ratio now survives (was 2.25/2.0 hot under 3-way). |
| Amber Fog | Genre / Mood | 0.5 / −1.0 / +0.25 / −0.5 | **0.5** | **1.5** | Strong | Strong | Rim rel −0.25 sits on the Subtle/Strong boundary; Strong (0.75 hot) keeps the deliberate every-direction low-contrast wrap. Bg rel −1.0 → Strong 0.5 hot. |
| Underwater Depths | Genre / Mood | 0.25 / −2.5 / −1.25 / −1.75 | **0.25** | **2.75** | Subtle | Subtle | Rim rel −1.5 → 0.5 hot; bg rel −2.0 = Subtle exact. |
| High Noon | — | 1.25 / −2.25 / −3.0(off) / −0.75 | **1.25** | **3.5** | Off | Subtle | Bg rel −2.0 = Subtle exact. (Intent says "baked bright", but the authored EV is the ground truth; Strong would overshoot by 1.5.) |
| Fluorescent Office | — | 0.0 / −0.75 / −1.25 / −2.0 | **0.0** | **0.75** | Subtle | Subtle | Rim rel −1.25 → 0.25 hot; bg rel −2.0 = Subtle exact. |
| Rainy Night Window | Genre / Mood | 0.25 / −3.0 / −1.5 / −2.25 | **0.25** | **3.25** | Subtle | Subtle | Rim (Cyber Pink bleed) rel −1.75 is the exact midpoint; threshold resolves to Subtle (0.75 hot). Bg (Deep Space) rel −2.5 → 0.5 hot. |
| Rock With You | Genre / Mood | 0.75 / −6.0(off) / −0.25 / −0.25 | **0.75** | **5.0** (fill off; black void demands max) | Subtle | Strong | Lights 2/3 are the green/cyan laser blades. Rim rel −1.0 = Subtle exact; bg-slot laser rel −1.0 → Strong 0.5 hot (Subtle would halve it twice). Blades become 0.5 stops unequal — cosmetic. |

## Special cases / Key-off presets

- **Silhouette — Key was OFF (structural conflict, unchanged by 4-way).** The look is a blasted +1.5 backlight and a 0.0 sky wash with *no* front light. Easy forces Key on at EV 0, so any master_ev either front-lights the subject (master high, backlight correct — the row above) or crushes the frame (master low, backlight dead). The best-effort row anchors the backlight exactly and accepts a lit subject front. True silhouette needs Advanced (Key off), or loader tolerance for `key_on=false` on this one preset.
- **Sci-Fi Rim — exposure lives in the rims (structural, unchanged by 4-way).** Old Key −2.5 (dark centre) with rim at rel **+4.0** over Key; Strong caps at +0.5 and Drama only darkens Fill, never Key. The best-effort row anchors the edges (exact) and sacrifices the dark centre by 3.5 stops. Advanced-only.
- **Ethereal Halo — blown backlight still clipped.** Rim authored rel +1.5 vs Strong's +0.5: ~1 stop residual. Reads "bright halo", not "blown". The sole surviving argument for a hotter-than-Strong ("Blast") rim value.
- **Solo Spotlight — deepest accents still 1.25 stops hot.** Rim rel −3.75 and bg rel −4.75 sit below even the Faint floors (−2.5 / −3.5). Faint is a large improvement (was 2.75 hot); the residual 1.25 stops is masked by Congo Blue's dimness and is acceptable.
- **Resolved by the 4-way buckets:** Cathedral Shaft, Caravaggio, Film Noir, Low Key Drama, Clinical Morgue, Venetian Noir, Prison Bars, Horror Underlight, Sodium Vapor Night, Firelight, Uplight Horror, Top Light — all now within ≤0.75 stop of authored (Film Noir, Low Key Drama and Clinical Morgue's rims are exact).
- **Ringside Kickers and Rock With You — bg slot repurposed** as a second kicker / laser blade. Buckets work mechanically; the authored-equal twin lights become 0.5 stops unequal after quantization. Cosmetic.

## Appendix — exact write values per preset (mechanical, no interpretation)

EVs are the fixed bucket values, relative to Key@0 (they track master_ev automatically). `off` means write `on=false` (keep the existing stored EV as the Advanced-mode fallback).

| Preset | rim: on, ev | bg: on, ev |
|---|---|---|
| Rembrandt | on, −1.0 | off |
| Paramount Butterfly | on, −1.0 | off |
| Broadcast Interview | on, −1.0 | on, −2.0 |
| Film Noir | on, −2.5 | off |
| Silhouette | on, +0.5 | on, −0.5 |
| Golden Hour | off | on, −3.5 |
| Blue Hour Moon | on, −1.0 | off |
| Neon Crossfire | on, −1.0 | off |
| Firelight | on, −2.5 | off |
| Loop | on, −1.0 | off |
| Split | on, −1.0 | off |
| Clamshell Beauty | on, −1.0 | off |
| High Key | on, −1.0 | on, −0.5 |
| Low Key Drama | on, −2.5 | off |
| Uplight Horror | on, −2.5 | off |
| Top Light | on, −2.5 | off |
| Motivated Window | off | on, −2.0 |
| Overcast Soft | on, −1.0 | on, −2.0 |
| Teal &amp; Orange | on, −1.0 | off |
| Sci-Fi Cool | on, −1.0 | off |
| Candlelight 2700K | on, −1.0 | off |
| Tungsten 3200K | on, −1.0 | off |
| Neutral 4500K | on, −1.0 | off |
| Daylight 5600K | on, −1.0 | off |
| Cool 6500K | on, −1.0 | off |
| Moonlight 8000K | on, −1.0 | off |
| Venetian Noir | on, −2.5 | off |
| Window Light | off | on, −2.0 |
| Prison Bars | on, −2.5 | off |
| Dappled Forest | on, −1.0 | on, −2.0 |
| Skylight Grid | off | off |
| Horror Underlight | on, −2.5 | on, −3.5 |
| Romance Soft | on, −1.0 | on, −2.0 |
| Sci-Fi Rim | on, +0.5 | on, −0.5 |
| Music-Video Neon | on, −1.0 | on, −2.0 |
| Short Side Portrait | on, −2.5 | off |
| Caravaggio | off | on, −3.5 |
| Clinical Morgue | on, −2.5 | on, −2.0 |
| Sodium Vapor Night | on, −2.5 | on, −2.0 |
| Ethereal Halo | on, +0.5 | on, −0.5 |
| Solo Spotlight | on, −2.5 | on, −3.5 |
| Ringside Kickers | on, −1.0 | on, −0.5 |
| Ring Light Beauty | on, −1.0 | on, −2.0 |
| Cathedral Shaft | on, −2.5 | on, −3.5 |
| Amber Fog | on, +0.5 | on, −0.5 |
| Underwater Depths | on, −1.0 | on, −2.0 |
| High Noon | off | on, −2.0 |
| Fluorescent Office | on, −1.0 | on, −2.0 |
| Rainy Night Window | on, −1.0 | on, −2.0 |
| Rock With You | on, −1.0 | on, −0.5 |
