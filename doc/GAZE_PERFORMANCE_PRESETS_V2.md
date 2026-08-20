# Gaze Performance Presets — Expansion Pack (v2)

16 new named "Performance" presets for the Actor Gaze dropdown, expanding the emotional/genre range beyond the
shipped 12 (Intense Lock, Nervous Avoider, Runway Confidence, Interrogation Stare, Lovers' Exchange, Cold Read,
Grieving Distance, Fan Meeting Idol, Guilty Party, The Bodyguard, Doting Parent, Thousand-Yard).

Each row = persona vector (Dominance/Affection/Anxiety, −1..+1) + base-parameter overrides. Parameter ranges:
H/E (head_eyes_blend 0..1), Torso 0..1, Inten (intensity 0..1), Smooth 0..1, EyeYaw −15..15°, EyePitch −10..10°
(+ = eyeline up / looking-up-under-lids; − = eyeline down), MicroLife 0..1, Blinks on/off, Var 0..1,
Break (glance_break_freq 0..1), Acq 0.05..2.0 s, Rel 0.05..3.0 s, DeadZone 0..15°.

Grouped Power / Romance / Fear-Stress / Detached / Fourth-wall.

| Name | Intent (what the audience reads) | Dom | Aff | Anx | H/E | Torso | Inten | Smooth | EyeYaw | EyePitch | MicroLife | Blinks | Var | Break | Acq | Rel | DeadZone |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **Predator** | Hunting, locked-on, near-unblinking smooth pursuit — coveting or stalking | 0.85 | −0.20 | −0.30 | 0.70 | 0.40 | 1.00 | 0.70 | 0 | 0 | 0.15 | off | 0.10 | 0.00 | 0.60 | 1.20 | 2 |
| **Menacing Villain** | Cold, still threat; chin down, eyes up through the brow | 0.95 | −0.50 | −0.20 | 0.85 | 0.50 | 1.00 | 0.50 | 0 | +1 | 0.10 | off | 0.05 | 0.00 | 0.50 | 1.00 | 3 |
| **Commanding Orator** | Authority addressing a room; steady with controlled sweeps | 0.70 | 0.10 | −0.10 | 0.90 | 0.60 | 0.90 | 0.50 | 0 | 0 | 0.25 | on | 0.20 | 0.30 | 0.40 | 0.80 | 4 |
| **Defiant Challenge** | Chin up, won't look away, a charged edge of nerve | 0.75 | −0.15 | 0.25 | 0.70 | 0.40 | 1.00 | 0.40 | 0 | −4 | 0.20 | on | 0.15 | 0.00 | 0.30 | 1.00 | 2 |
| **Seductive** | Half-lidded come-hither; slow, glance-away-and-return | 0.30 | 0.70 | 0.10 | 0.50 | 0.30 | 0.90 | 0.70 | +3 | +2 | 0.30 | on | 0.25 | 0.35 | 0.90 | 1.40 | 3 |
| **Smitten** | Soft, can't look away, occasional shy break; adoring up-gaze | 0.00 | 0.80 | 0.35 | 0.60 | 0.30 | 0.90 | 0.60 | 0 | +2 | 0.35 | on | 0.30 | 0.20 | 0.70 | 1.20 | 3 |
| **Flirty-Shy** | The flirt cycle — look, hold, break with a smile, re-check | 0.20 | 0.60 | 0.40 | 0.45 | 0.20 | 0.85 | 0.55 | +4 | +1 | 0.40 | on | 0.35 | 0.50 | 0.50 | 0.90 | 3 |
| **Panic** | Fight-or-flight; darting eyes, rapid blink, jittery | −0.30 | −0.10 | 0.95 | 0.40 | 0.20 | 0.85 | 0.20 | 0 | 0 | 0.90 | on | 0.50 | 0.85 | 0.15 | 0.40 | 1 |
| **Vulnerable / Pleading** | Searching the other's face, up-glances, soft and open | −0.40 | 0.50 | 0.70 | 0.55 | 0.30 | 0.90 | 0.50 | 0 | +5 | 0.45 | on | 0.30 | 0.30 | 0.50 | 1.00 | 3 |
| **Hostile Witness** | Evasive under pressure; forced brief contact then away/down | −0.10 | −0.40 | 0.75 | 0.40 | 0.20 | 0.80 | 0.35 | 0 | −2 | 0.50 | on | 0.40 | 0.80 | 0.30 | 0.60 | 2 |
| **Bored / Checked-out** | Minimal engagement, drifting, slow lazy blinks | −0.20 | −0.20 | −0.10 | 0.25 | 0.10 | 0.60 | 0.70 | 0 | −2 | 0.50 | on | 0.30 | 0.55 | 0.80 | 1.50 | 6 |
| **Dissociative-Cold** | Present but affectless; steady, flat, almost never blinks (focused, unlike Thousand-Yard's defocus) | 0.20 | −0.30 | −0.20 | 0.50 | 0.30 | 0.85 | 0.60 | 0 | 0 | 0.08 | off | 0.05 | 0.05 | 0.70 | 1.40 | 4 |
| **Distracted** | Attention split; frequent darts away to "other things," returns | 0.00 | 0.00 | 0.40 | 0.35 | 0.15 | 0.75 | 0.40 | 0 | 0 | 0.50 | on | 0.40 | 0.70 | 0.35 | 0.60 | 3 |
| **Zen / Serene** | Calm, unhurried, soft and steady; nothing to prove | 0.15 | 0.35 | −0.50 | 0.60 | 0.35 | 0.90 | 0.80 | 0 | 0 | 0.20 | on | 0.15 | 0.15 | 1.00 | 1.60 | 4 |
| **Villain Monologue** | Fourth-wall: addresses the lens theatrically, holds the audience | 0.80 | 0.00 | −0.15 | 0.85 | 0.55 | 1.00 | 0.50 | 0 | 0 | 0.20 | on | 0.15 | 0.10 | 0.60 | 1.20 | 3 |
| **Conspirator** | Fourth-wall Fleabag: quick knowing flicks to camera, back to scene | 0.35 | 0.30 | 0.15 | 0.40 | 0.15 | 0.85 | 0.45 | +2 | +1 | 0.35 | on | 0.30 | 0.55 | 0.25 | 0.70 | 3 |

**Notes for implementation**
- Same table shape as the existing 12 presets; append to that data table (keep existing indices stable, add these after).
- "Blinks off" = disable the periodic blink for that performance (Predator/Villain/Dissociative read as unblinking).
- EyePitch sign convention matches the panel's Eyeline pitch slider (+ up / − down); Defiant's −4 is the chin-up/eyes-down look; Vulnerable's +5 is the pleading up-gaze.
- These modulate on top of the persona mapping — verify the extreme personas (Menacing 0.95 dom, Panic 0.95 anx) don't collide with the Alpha-Stare / contact-ration extremes in unintended ways; tune Break/Blinks per row if they do.
