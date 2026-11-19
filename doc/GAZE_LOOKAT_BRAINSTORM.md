# Gaze Direction System — Brainstorm: "Where They Look Is What They Feel"

Framing note: almost everything below decomposes into **modulation of primitives you already have** (hold length, aversion vector, blink schedule, eye-vs-head lead, acquire/release ease, micro-life amplitude) plus **a first-class timeline cue system**. Determinism is preserved everywhere by two techniques: (a) *authored cues* → envelopes evaluated as `f(t − cue_time)`, and (b) *seeded event lattices* — the timeline is divided into intervals and each event (blink, glance, saccade) is derived from `hash(seed, interval_index)`, so any time `t` can be evaluated statelessly.

---

## A. Expressive Intent — the emotion of a look

**1. Gaze Persona Vector ("Attitude Dial")**
Three director-facing axes — Dominance, Affection, Anxiety — that remap all existing primitives at once: dominance lengthens holds and squares the head, anxiety shortens contact and biases aversion downward, affection softens acquire ease and adds lid narrowing. This is the single highest-leverage abstraction: directors think in attitude, not in "glance-break frequency."
`Medium` — pure parameter remapping; nothing new is animated.

**2. Contact Ration ("Shy Budget")**
A hard cap on continuous mutual-gaze duration before a *mandatory* aversion, with aversion direction as a personality tell: downward = submission/shame, lateral = cognitive load, upward = exasperation. Real gaze-aversion research maps direction to meaning — this makes "shy" readable in one shot.
`Easy` — a seeded schedule of contact windows; aversion direction from persona.

**3. Alpha Stare ("Dominant Lock-On")**
Suppress blinks below baseline, zero glance-breaks, chin drops 2–3°, head fully square to target, micro-saccade amplitude reduced. Unbroken mutual gaze is the universal dominance display; the stillness itself is the performance.
`Easy` — it's mostly turning existing things *off*, plus a chin-pitch bias.

**4. Glance-and-Away ("The Flirt Cycle")**
The canonical flirtation loop: acquire target → hold 0.8–1.5s → break with a lid narrow (Duchenne-adjacent squint, not a blink) and slight head turn → re-check after 2–4s with eyes-only. The *re-check* is what reads as flirtation, not the look.
`Medium` — a seeded state machine expressible as a periodic schedule with jitter.

**5. Suspicion Vector ("Side-Eye")**
Clamp head yaw toward the target hard (e.g. max 15°) while letting eyes carry the full deviation, so the eyes strain in their sockets. Side-eye = surveillance without commitment; instantly reads as distrust.
`Easy` — per-persona override of the head-yaw clamp you already have.

**6. Thousand-Yard Stare ("Defocus")**
Vergence relaxes *past* the target (eyes go parallel), micro-saccades damp to near zero, blink rate halves, lids sag slightly. The character is looking *through* the camera — dissociation, trauma, grief.
`Easy` — needs a vergence term (see #29); everything else is amplitude reduction.

**7. Smooth Pursuit Mode ("Predator")**
Replace saccadic re-acquisition with continuous low-latency smooth pursuit — no catch-up jumps, head leads the eyes slightly, body commits early. Humans only smooth-pursuit things they're *hunting or coveting*; it reads as intent.
`Medium` — swap the acquire model for a critically-damped tracker evaluated in closed form from target trajectory.

**8. The Once-Over ("Body Scan")**
A scripted eyeline path over a cast member: face → chest → down → feet → back to face, with authorable dwell times and a brow micro-raise on the return. "Checking someone out" is pure blocking gold for romance and menace alike.
`Medium` — needs face/body sub-anchors (#17); the path itself is a keyed spline in target-local space.

**9. Penny Drop ("Dawning Realization")**
A staged compound: unfocused stare (defocus) → micro-saccade burst (searching memory) → blink → snap-to-target with lid widen and slight head recoil, spread over a director-set duration (0.5s = shock, 4s = horror). The single most-requested acting beat in dialogue scenes.
`Medium` — one cue, one fixed envelope sequence, fully `f(t − cue_time)`.

**10. Deference ("Submissive Down-Glance")**
Baseline gaze pitched 10–20° below target's eyes, punctuated by brief *checking glances* up to the face (200–400ms) then back down. The checking glance is the documented submissive pattern — it plays servants, prisoners, shy lovers.
`Easy` — pitch bias + seeded up-glance lattice.

---

## B. Eye-line & Blocking Craft

**11. Off-Lens Ring ("Barrel Dialing")**
Replace freeform yaw/pitch nudge with *calibrated* off-lens presets in degrees: 0° down-the-barrel, 1–2° "tight single" (subject seems to see the audience's soul), 5° "close single," 10–15° "dirty over." On real sets this is the DP's whole conversation; naming the numbers teaches the craft.
`Easy` — it's your existing nudge, quantized and labeled, computed from actual lens FOV.

**12. Button Look ("Look to Lens on the Last Beat")**
A one-click timeline event: maintain scene eyeline, then at the marked frame, snap or drift to lens and *hold through the cut*. The Fleabag/Office button. Add a "hold-frames-past-cut" parameter so the edit lands.
`Easy` — a cue with an acquire envelope; trivially deterministic.

**13. Reverse-Safe ("Eyeline Match Lock")**
Record the gaze vector (in screen space: left-of-frame, 8° below lens) from shot A, and auto-mirror it when shooting the reverse on another Vcam, so the two singles cut. Broken eyelines are the #1 reason machinima dialogue feels amateur — this is invisible craft that fixes it.
`Medium` — per-Vcam stored eyeline offsets; deterministic because Vcams are on the timeline.

**14. Triangle Scan ("The Listener")**
A listening subject scans the speaker's face in the documented eyes→eyes→mouth triangle, weighting toward the mouth during speech-marked regions and the eyes during pauses. Listening is 80% of screen acting; this makes reaction shots alive.
`Hard` — needs face sub-anchors + a seeded saccade lattice over three points; speech regions come from authored dialogue markers, keeping it scrub-safe.

**15. Pass the Look ("Mutual Gaze Handshake")**
Choreograph a look *exchange*: A looks at B; after an authored latency B "feels it" and looks back; on mutual-gaze onset a shared beat fires (both hold, or A breaks). Gaze-cueing between two characters is how audiences read relationships without dialogue.
`Medium` — two coupled cue tracks with authored latencies; no runtime feedback loop, just offset envelopes.

**16. Room Attention ("Group Focus Director")**
One master "attention target" track that N subjects follow with per-subject seeded delay (100–600ms), acquire style, and compliance weight — the whole tavern turns to the door, but never in lockstep. Crowd gaze is impossible to hand-key per subject; this makes ensemble scenes one-click.
`Medium` — a shared cue track fanned out through per-subject `hash(seed, cue_id)` delays.

**17. Anatomy Anchors ("Face Sub-Targets")**
Target not "cast member B" but *B's mouth, left eye, hands, or held prop*. Watching hands = wariness; watching mouth = desire or lip-reading; watching the gun = the scene. Prerequisite for #8, #14.
`Medium` — offsets in the target's head/hand bone space, resolved per frame.

**18. Kuleshov Anchor ("The Implied Off-Screen Thing")**
An eyeline bookmark: name a fixed point ("the body," "the door"), and every shot in the sequence that references it auto-matches the eyeline direction in screen space — the audience assembles the geography from consistent looks alone. Kuleshov's whole discovery, operationalized.
`Medium` — named world-space points + per-shot screen-space consistency check; tooling more than animation.

---

## C. Timed Performance — gaze on the timeline

**19. Look Cues ("The Gaze Track")** — *the backbone*
Promote gaze to a first-class timeline track per subject: clips of `LookAt(target, persona, acquire-style, release-style)` with crossfades between clips. Everything in this section composes from it. Without this, every idea above is a live knob; with it, gaze becomes *editable performance*.
`Medium` — clip evaluation is inherently `f(t)`; crossfade two pure functions.

**20. Two-Look ("The Double-Take")**
Canned macro: glance at target → casual release away → 200–500ms gap → violent snap-back with overshoot, lid widen, and head recoil. The gap length is the comedy dial. One button, endless mileage.
`Easy` — one cue, fixed envelope chain.

**21. Busted ("Caught Looking")**
A stares at B; at the cue, A executes an escape saccade *away* (never through the lens), a guilty blink, a micro head-duck, and a too-casual fake target acquisition. Optionally chained: B's look-toward *is* the cue for A's break. Instant comedy or instant menace depending on tempo.
`Medium` — two synced cues on two subjects; the "fake target" is a seeded pick from a hemisphere excluding B and lens.

**22. Creep Turn ("The Slow Burn")**
An acquire stretched over 2–8 seconds with authored stagger: eyes land first, then head trails, then neck, then the body-turn threshold trips *last* with a weight shift — dread is the anatomy chain played in slow motion. Your chain already exists; this exposes its *timing* as a performance parameter.
`Easy` — per-cue override of chain stagger times.

**23. Check the Watch ("Object Glance Insert")**
Glance to object → authored dwell → return to previous target, with a "thought residue" parameter (slower, heavier return = the object *meant* something). Motivates every insert shot you'll ever cut to.
`Easy` — a temporary cue that restores the underlying track on release.

**24. Tracking Loss ("Follow and Lose")**
Smooth-pursue a moving target; at cue (or occlusion angle), pursuit gain fades, gaze overshoots and drifts, a searching saccade burst fires, then release to idle or hero target. "Watching the car drive away" — longing in one gesture.
`Medium` — pursuit gain is an authored envelope; the search burst is a seeded lattice.

**25. Look on the Line ("Beat-Synced Cues")**
Snap gaze cues to audio/dialogue markers so a look lands on a *word* — look up on "you," break on the line's end. Gaze that syncs to dialogue is the difference between animation and acting.
`Easy` — markers are authored timeline data; pure quality-of-life with outsized craft payoff.

---

## D. Micro-Behavior Realism

**26. Lid Follow ("Lid-Gaze Coupling")**
Eyelids track vertical gaze: looking down narrows the aperture, looking up widens it, and lids *lead* the eye by ~30ms on downward saccades. This is the single cheapest realism win in eye animation — its absence is why CG eyes look dead.
`Easy` — lids as a direct function of current gaze pitch; zero new state.

**27. Arousal Lids ("Interest Widen / Suspicion Squint")**
On target acquisition, a brief lid widen + brow micro-raise (orienting arousal); persona-gated squint for suspicion/scrutiny. Ties the *lids* to the *meaning* of the look, not just its direction.
`Easy` — envelope keyed to acquire events, which are already deterministic.

**28. Punctuation Blinks ("Blink Semantics")**
Replace uniform blink cadence with the real pattern: blinks cluster *on* gaze shifts (blink-during-saccade masks the jump), at cognitive beats, and at held-gaze release — and are suppressed mid-lock. Blinks become grammar instead of noise.
`Medium` — blink lattice re-derived from the gaze-event schedule; still `hash(seed, event_id)`.

**29. Vergence ("Close-Up Cross")**
Eyes converge as a pure function of target distance — at a 0.6m lens they visibly cross a few degrees. Close-ups are where machinima gaze currently dies, because parallel eyes at close range read as staring *past* the camera. This fixes the most-watched shot in any film.
`Easy` — trigonometry from target distance; stateless by construction.

**30. Orient Reflex ("Startle Glance")**
Authored "stimulus markers" (a bang, a passing figure) trigger the involuntary express saccade: eyes jump toward the stimulus direction in <100ms, head follows only if it persists, then recovery to the prior target. Involuntary looks make the *world* feel real, not just the character.
`Easy` — marker + fixed reflex envelope.

**31. Attention Decay ("Fatigue Drift")**
With no active cue for N seconds, gaze slowly de-converges, saccade tempo drops, blinks lengthen, head pitch sinks a degree — the idle state itself becomes "mind wandering" instead of robotic hold. Background cast stops looking embalmed.
`Easy` — envelope on time-since-last-cue, computable from the cue track.

**32. Held Breath ("Stillness Is Intensity")**
During max-intensity locks, *duck* micro-life amplitude toward zero on a slow oscillation, as if breath is held — then release it in a burst when the gaze breaks. Actors know: stillness reads as the loudest emotion. Counterintuitive and cheap.
`Easy` — micro-life gain as a function of lock intensity.

**33. VOR Counter-Rotation**
During slow head drift, eyes counter-rotate (vestibulo-ocular reflex) so the *eyeline stays welded* to target while the head lives — currently head drift likely drags the eyeline subtly. The eyes-locked/head-moving decoupling is what "held eye contact" actually looks like.
`Medium` — eyes computed as target-relative *after* head drift is applied; ordering fix more than new math.

---

## E. Directability & Presets

**34. Gaze LUTs ("The Performance Library")**
Ship ~12 named presets bundling persona + micro params: *Intense Lock, Nervous Avoider, Runway Confidence, Interrogation Stare, Lovers' Exchange, Cold Read, Grieving Distance, Fan Meeting Idol, Guilty Party, The Bodyguard (scans room, checks principal), Doting Parent, Thousand-Yard.* Presets are how the lighting side got adopted; gaze needs the same on-ramp.
`Easy` — parameter bundles over everything above.

**35. Same But Different ("Style Re-Roll")**
Re-roll only the seed *within a preset's constraint box* — same attitude, different take. For groups: auto-distribute seeds so five "Nervous Avoiders" never sync. One-click take variety.
`Easy` — seed offset; the constraint box is the preset.

**36. Copy Look / Paste Look**
Copy a subject's full gaze configuration (preset + tweaks + cue track) to another subject with automatic seed decorrelation and target remapping ("their B is my A"). Directing five characters shouldn't cost 5× the knob-turning.
`Easy` — config serialization.

**37. Gaze Takes ("A/B While Looping")**
Store up to 4 gaze configurations per subject as "takes," hot-swap them while the shot loops. Directors judge gaze by *feel at speed*, never by parameter values — this is the review workflow the feature deserves.
`Medium` — config snapshots + instant swap (safe, since evaluation is stateless).

**38. The More/Less Knob ("Director's Macro")**
One slider that scales the entire active persona toward/away from neutral — the note every director gives is "do that, but 60%." Maps to a lerp between neutral params and current params.
`Easy` — single-parameter lerp.

---

## F. Camera-Aware Tricks

**39. Confidant Camera ("Fleabag Mode")**
Mid-scene, the subject throws brief conspiratorial glances *to the live lens* — a 300–800ms flick with a lid narrow (the shared-joke squint) — then returns to the scene eyeline as if nothing happened. Frequency/duration seeded or cue-placed. This is a signature look no other machinima tool has.
`Medium` — layered temporary cue over the base track; lens position comes from the deterministic Vcam Gate.

**40. Never Look ("Camera Repulsion Cone")**
A forbidden solid angle around the live barrel: any gaze path that would cross it deflects around it, and idle aversion picks directions *away* from the lens. A character who pointedly refuses the camera is as charged as one who addresses it — and it also just keeps extras from accidentally spiking the lens.
`Medium` — deflection is a pure geometric remap of the target direction.

**41. Dolly Draw ("Push-In Magnetism")**
As lens-to-subject distance shrinks (a push-in), gaze weight drifts fractionally toward the lens — the audience *feels* seen without a hard look. On pull-out, the character releases back to the scene. Because Vcam moves are on the timeline, this is fully deterministic.
`Medium` — blend weight as a function of camera distance, itself `f(t)`.

**42. Hold the Audience ("Zoom Lock")**
Glance-break probability and micro-saccade amplitude scale down with focal length / shot tightness — tight shots demand steadier eyes (a real acting discipline: "less is more in the close-up"). Auto-applies the discipline your cast doesn't have.
`Easy` — gains driven by the live Vcam's FOV.

**43. Cut to Me ("On-Cut Re-Aim Styles")**
Per Vcam-Gate cut, an authored re-aim style: *Snap* (was already looking — continuity), *Discover* (notices the new camera 300ms after the cut — fourth-wall comedy), or *Refuse* (avoids the new lens). Turns the Gate's re-aim from plumbing into a performance choice.
`Easy` — cut events are authored; style = which envelope plays at `t − cut_time`.

**44. No Blink on the Cut ("Money-Window Guard")**
Deterministically exclude blinks (and glance-breaks) from ±6 frames around every cut and from director-painted "money windows" — the frames that must land. Editors reshoot real actors over a badly-timed blink; you can just legislate it.
`Easy` — the blink lattice already knows all cut times; filter events, still stateless.

**45. Barrel Check ("Eyeline HUD")**
Viewport overlay: each subject's gaze ray, degrees-off-barrel readout, a ring showing #11's calibrated offsets, and a mutual-gaze indicator when two rays intersect. You can't direct what you can't see; this is the light-meter of gaze.
`Medium` — pure debug rendering; no animation risk at all.

---

## Top 6: Highest Impact for Effort

| # | Idea | Why |
|---|------|-----|
| 26 | **Lid Follow** | Biggest realism-per-line-of-code in the entire list; fixes "dead CG eyes" everywhere at once |
| 29 | **Vergence** | Rescues the close-up — the most-watched shot type — with pure trigonometry |
| 19 | **Look Cues (Gaze Track)** | The backbone: converts gaze from live knobs into editable, cut-able *performance*; unlocks 15 other ideas |
| 1+34 | **Persona Vector + Gaze LUTs** | Lets directors speak in attitude ("Nervous Avoider, 60%") instead of eleven sliders; the adoption on-ramp |
| 11 | **Off-Lens Ring** | Nearly free (relabel + quantize existing nudge) and teaches real DP eyeline craft |
| 12+44 | **Button Look + Money-Window Guard** | Two tiny cue-based features that directly manufacture the two most iconic gaze moments in editing: the look-to-lens button, and a clean face on the cut |

**The Sleeper: The Attention Score (Epic).**
Treat every subject's attention as a *conducted score*: one master timeline lane where the director drags named attention events ("door opens," "she enters," "gunshot," "he says the line"), each with a saliency weight — and every cast member's gaze track is *derived* from the score through their persona (the Nervous Avoider checks the door twice; the Alpha never turns; the crowd ripples with seeded delays). Layer #14's listener scanning and #15's mutual-gaze handshakes on top and you get whole ensemble scenes — a dinner party, a courtroom, a standoff — where attention flows like music from a single editable lane. It stays deterministic because the score *is* authored timeline data and every derived look is `f(seed, persona, event_times, t)`. It would make this the first machinima tool where you don't animate characters looking — you conduct where the room's mind is.
