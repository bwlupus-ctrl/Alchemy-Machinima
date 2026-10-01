# Adversarial review R4 (Opus): PROBE_UPDATE_ON_DEMAND_BRIEF_V5

Read-only review at HEAD 84545d8b841. `[LiveProbeFaceLights]` is committed and verified: llviewerdisplay.cpp:1292-1305 is gated on `isCinematicLiveProbeCapture()` and sits right after `display_update_camera()`. User rule: anything that breaks the promise is a P0.

**Verdict: GO-after-fixes.** V5 closes most of R3. Three new promise-breakers remain, each with a local fix, plus one R3 item that is only half-closed.
1. Scripted appearance cycling (slideshows, colour cycling) takes the discrete path, so it never freezes. T40 as written cannot pass.
2. Attachment-light positions of other avatars are visibility-gated, which makes light settles camera-driven.
3. Recolouring a flickering rig light is hidden from Live and from ordinary probes indefinitely.

## 1. R3 items against V5 (checked in code)

| R3 | Status |
|---|---|
| P0-1 empty list | **Fixed.** The Live part is committed (84545d8b841). §4.5.2 extends the same gate. OFF: both terms are false. |
| P0-2 use-after-free | **Fixed.** Loops use `getNumFaces(ch)` (llviewertexture.h:175) and `getNumVolumes`. |
| P0-3 barrier | **Fixed for hang and false-done.** The timeout is not load-aware (P1-1). |
| P0-4 slow sources | **Half-fixed.** Motion is fixed. Discrete appearance cycling is not (N-1). |
| P0-5 edit hidden | **Fixed for prims.** Animated-light recolour is still hidden (N-3). |
| P0-6 overflow ordering | **Fixed.** Motion takes no slots; E×P is computed after filtering. The bulk state can hide a one-off move (P1-2). |
| P0-7 asset swap | **Fixed by identity.** The readiness reset is too broad (P1-3). |
| P0-8 re-sharpen | **Fixed** via `mMinFaceSerial`. An off-by-one remains (P2). |
| P1-1 cap / cost | **Partly fixed.** Spots count at any distance (N-4), and the prefilter is ineffective (P1-4). |
| P1-2 isTooSlow | **Fixed** (transient branch only; the deferred loop never tests it). |
| P1-3 S13 | **Fixed:** a reviewer-produced golden literal plus a colour-moved diff. |

## 2. New P0 (promise-breakers)

**N-1. Scripted appearance cycling bypasses the debounce and never settles (§4.10 "discrete notes never enter the debounce").**
- A texture slideshow (`llSetTexture` every 1 s), a colour cycle or a dance floor calls `setTE*`, which reaches `markTextured` (H3, pipeline.cpp:4517). That makes a discrete `R_TEX`.
- Scripted gobo or FOV cycling works the same way: it is a light STRUCT change, which is also discrete.
- So nearby probes restart every MinInterval, and UNSETTLED fires.
- T40 ("1 s texture slideshow → frozen") contradicts the mechanism.
- *Fix:* per-(key, channel) **rate demotion**. A key/channel with 3 or more discrete events within 10 s routes further events on that channel through the debounce (new `Motion::APPEAR` and `LIGHT_STRUCT`). A one-off edit, or an edit on a *different* channel of a moving object, still bypasses. This keeps R3 P0-5 intact and freezes cycles.

**N-2. Attachment-light XFORM changes are camera-driven for other avatars.**
- Off-screen or far avatars get `HIDDEN_UPDATE`: `updateCharacter` early-outs at llvoavatar.cpp:5692-5699 and :5756-5758.
- Attachment positions update only on `detailed_update` / `visible` (:3136-3139, :3157).
- An idle avatar's facelight therefore moves continuously (frozen) while in view and stops when the camera turns away. That fires a settle about 0.5 s later, and probes refresh. When the avatar comes back into view it moves again, and the cycle repeats.
- T7-strict fails in any set with a lit avatar other than self.
- *Fix:* for attachment lights of non-self avatars, quantise the XFORM signature coarsely: position tolerance about 1 m, and drop the orientation fields. Idle sway then never fires, but walking still counts as motion. T55 checks this.

**N-3. Recolouring a flickering rig light is hidden indefinitely (§0.3(b), §5.2).**
- Flicker modulates the emitter's live colour and intensity every frame (alcinelightrig.cpp:303, applied at :1976-1994), so the PHOTO class is permanently in motion.
- The ON-path Live H drops the rig signature (section 2, :768) and uses only *published* PHOTO hashes. A rig colour or intensity edit, which is the Live Probe's main use, never refreshes Live or ordinary probes while a flicker look runs.
- The OFF path handled this by forcing FULL (:524-528).
- *Fix:* add an **authored-state token** per enabled rig slot to the ON-path Live H, and a discrete STRUCT-class event for ordinary probes. The token covers the unmodulated colour/intensity/FX program from `mCurrentLive` (alcinelightrig.h:415), not the emitter values. This needs one read-only accessor in alcinelightrig.h (currently off-limits: ask the user). Non-rig scripted flicker stays as declared residue.

**N-4. The cap rule counts every eligible spot at any distance (§4.5.6).**
- Spots are indeed list candidates at any distance (:9660, :9815), but the deferred loop caps in distance order (`calc_light_dist`, :9426-9446). Far spots sort **after** every near light, so they can never displace a light that is relevant to a probe.
- Counting them makes every probe "over cap" in projector-heavy sims (more than 256 spots across 4-9 regions). Every facelight or rig settle anywhere then becomes global, and probes in a crowd never settle.
- *Fix:* count only lights whose `calc_light_dist` from the probe is below `Rcap + 1.5·LIGHT_MAX_RADIUS`, spots included. Only those can change the in-cap rank of a light the probe can see.

## 3. P1

- **P1-1. Refresh-all timeout (§4.11).** 30 s is too short for P = 64 probes at 20 fps (768 frames = 38 s), so the button reports "Incomplete" in exactly the heavy scenes it exists for. Re-arming restarts everything. *Fix:* use a stall timeout (no member progress in 10 s unpaused), or `max(30 s, 3·12·P / fps)`.
- **P1-2. The bulk state can hide a one-off move (§4.10).** Once the map is full, a real move shares one timer with perpetual sources, so it never settles. *Fix:* evict non-pending (settled) states first, since they carry only gap history. Fold into bulk only when more than 16384 states are *pending*, and use a coarse spatial bucket (32 m cells), not one global bulk.
- **P1-3. MS readiness reset (§4.4 H4 "if `mVolumeChanged` → `mProbeAssetReady=false`").** `mVolumeChanged` is set by every `markForUpdate` (llvovolume.cpp:4758-4766), for example the PBR completion lambda (llviewerobject.cpp:5447-5454) and object updates. The next camera-driven LOD arrival then becomes a real event. *Fix:* reset only when the stored (sculpt id, type) differs from the current one. Identity already catches swaps. Also, the sculpt readiness predicate is left to the implementer; specify it.
- **P1-4. The light-diff prefilter is ineffective.** The union AABB of all probes' 131 m spheres covers essentially the whole loaded area. At 2000 lights with 3 signatures each per frame, T52 (≤ 0.5 ms) will likely fail. *Fix:* first compare a raw cache of about 20 floats (position, rotation, colour, radius, falloff, spot, texture id, gobo pointer) with memcmp, and build signatures only on a raw change.
- **P1-5. The cadence registry must apply `classify` (§4.12).** Flexi attachments (hair, skirts) register through H4's flexi branch and are avatar-attached. They must be dropped as DYN, or every probe near an avatar cadences.

## 4. P2

- **DS stamp off-by-one.** Use `currentSerial()+1`: a face rendered in the same serial as the downscale was rendered before it.
- **Published XFORM/PHOTO hashes and `shift()`.** Either store the published accumulators and re-digest them, or leave published hashes untouched (they are opaque tokens). Do not recompute published hashes from *current* accumulators.
- **`mCineStickyOn` origin field on shift.** Offset it like the lights, or accept one Live pair per crossing (today's behaviour).
- **A texture whose GL image is destroyed while faces still reference it.** It bypasses DS; verify whether `destroyGLTexture` can happen for in-use textures, and stamp there if it can.
- **AutoTune.** Show a Lightbox note when AutoTune is active with on-demand ON.
- **Compile.**
  - The debounce map key `(pointer, Motion)` needs a custom hash.
  - `probeShouldRenderLight` must be defined after the file-static `bdmerge_should_render_light` (:9470).
- The second `calcNearbyLights` in `renderGeomPostDeferred` rebuilds the same list, doubling the per-face light cost. Measure it in T52.

## 5. Residue

| Residue | Judgement |
|---|---|
| Recolouring a flickering *scripted* (non-rig) light | Acceptable: the colour change *is* the animation. |
| Rig flicker recolour | Blocker (N-3). |
| AutoTune required off for strict T7 | Acceptable with the UI note. Its changes are real setting changes. |
| Sources with a period over 30 s | Acceptable: one refresh per real change, below UNSETTLED. |
| Flexi/media frozen, and the UD2 cadence | Acceptable as designed. First sighting is camera-gated once, then cadence is camera-independent. Cost is 12 faces per affected probe per period, through the 1-face queue, so the budget is unchanged. |

## 6. Budget

Still ≤ 7 faces per `update()` in every combination:
- ordinary 1 (Refresh-all, cadence and settles are dirt reasons only);
- realtime ≤ 6: Live FULL/budget or sliced N, and these are mutually exclusive.

ON Live never FULLs from animation (`cine_animating` is forced false), so it is at or below OFF. OVER BUDGET remains a bug detector.

## 7. Tests

T0-T53 and S1-S17 are insufficient. Add:
- **T54** an `llSetTexture` slideshow and an `llSetColor` cycle (frozen via rate demotion);
- **T55** T7-strict with an idle non-self avatar wearing a facelight (no `z`);
- **T56** a Live On change flicker look with the key colour edited (prompt pair);
- **T57** Refresh-all with 64 or more probes at 20 fps or below (completes);
- **T58** flexi hair near a probe with cadence on (no registration);
- **T59** a PBR material load on a mesh, then a dolly (`g = 0`);
- **S14** plus rate demotion;
- **S16** plus published-hash stability on shift.

Keep T40, but it can only pass after the N-1 fix.
