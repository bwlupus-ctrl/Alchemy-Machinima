# Ghost Studio — Consolidated Feature Backlog & Roadmap

Single living backlog for Ghost Studio clone features. Consolidates the 2026-07-22 fun-ideas
brainstorm, the 2026-07-23 Director/Actors design pass, and the two new threads (new looks +
independent animation). Rule: anything controllable here MUST also be reachable from the Director
Console (superset rule). Client-only invariant always holds — nothing a clone does touches the sim.

Legend: ✅ done · 🔧 in progress · ⏭️ queued next · 📋 backlog

## ⭐ ROADMAP UPDATE 2026-07-24 (user-directed)
- **Scaling = DONE (Phase 1).** Uniform outer-render-transform scaling works in-world (confirmed via the
  Director scale spinner). Scale is applied AFTER skinning (client-only, foot-pivoted), per
  `doc/AVATAR_UNIFORM_SCALE_DEEP_RESEARCH.md`. Phase 2/3 polish (shadow/reflection/impostor audit,
  scale-aware LOD, picking/selection, motion vectors) is DEFERRED — not a current priority.
- **User priority order:** Track A (actors) + Track D (fun) + Track C (looks), plus the Director-editing
  items below.
- **NEW — Ghost Director "Add" = CHOICE of ghost type.** Do NOT retire the overlay ghost; instead the
  "Add" button offers spawning EITHER an overlay ghost OR an entity clone (director picks). Keep both
  types available.
- **NEW (pain point) — move entity `/ghost*`-command clones from the Ghost Studio panel.** Once placed,
  an entity clone can't be moved (the registry work disabled entity from the manip proxy / edit tools).
  Re-enable MOVE + rotate for entity kind via the manip box + setGhostPosition — now safe since scaling
  works and move/rotate are just object reposition (no skinning stretch). This is the top concrete item.
- **NEW design area — Cinematic Camera modes.** Add new automated cam patterns (e.g. a SPIRAL that pans
  UP the body) + other fun moves. Extends the existing Cinematic Camera operator / automated motion
  patterns (llactormover / cinematic camera; see phoenix-reshade-xl memory). Needs a design pass.
- **NEW long-term — Director Console KEYFRAME / automation system.** Let the Director Console KEYFRAME
  actions (transform/anim/style/cam/FX) so a directed sequence is REPEATABLE. Connects to the Flycam
  Recorder transport, ACTION-sync (Track A #5), and the clone cue-track idea (Track D). Big; design first.
- **Proposed next implementation batch:** "Ghost Director editing parity" = (1) move/rotate entity clones
  from the panel + (2) "Add" ghost-type choice. Then design pass on cinematic cams + keyframe automation.

## ✅ DONE — Entity clone core (in-world confirmed 2026-07-23)
Client-only `LLGhostAvatar` clone rigs, plays body + **animesh** animation (mirrors source),
shows baked + **PBR/GLTF** textures, and matches source **LOD**. Root cause across all of it: the
clone must replicate the sim's ObjectUpdate extra-params (`PARAMS_SCULPT` / `EXTENDED_MESH` /
`RENDER_MATERIAL`) that a client-only object never receives. See ENTITY_CLONE_RIGGING_FIX_BRIEF.md.

## 🔧 IN PROGRESS
- **#1 One Ghost Registry** — route `/ghostdress` into `ALGhostStudio` as entity-backed instances so
  clones appear/select/toggle/clear in the Director. (Codex implementing; Claude builds.)

## Track A — Director / Actors  (design: GHOST_DIRECTOR_ACTORS_BRAINSTORM.md, 16 ideas)
Ranked near-term slate from that doc:
1. ✅/🔧 One Ghost Registry (M) — see above
2. ⏭️ Ghost Actor Adapter (M) — stable Director id → runtime clone; add clones to the cast
3. ⏭️ Walk / Place / Path a clone (M) — reuse `LLActorMover` + Path Editor (highest payoff)
4. ⏭️ **Directed pose/clip (M)** — MIRROR_SOURCE | DIRECTED_CLIP | RECORDED | FROZEN drive modes  ← "independent animation"
5. 📋 ACTION sync (M) — one playhead for paths + clone anim + flycam takes
6. 📋 Ensemble (S–M) — follow-leader, look-at, staggered group paths
7. 📋 Scene recipe save/load (L) — persistence tier A
8. 📋 Cast Factory + stand-ins (M) — deterministic crowd variation
Big bets: fully-owned locked clone (XL), Performance Time Volume (record once → many scrubbed actors).

## Track B — Independent Animation  ⏭️ (user priority; = Track A #4 + prior "pose-clip chorus L")
Today a clone runs `MIRROR_SOURCE` (copies the avatar's `mSignaledAnimations`). Goal: a per-instance
**drive mode** so a clone plays animation INDEPENDENT of the avatar.
- **DIRECTED_CLIP** — play a chosen animation/pose asset on the clone's own motion controller only
  (never mutate the shared cached motion asset globally). Gate behind the instance's drive mode in
  `LLGhostAvatar::idleUpdate()` (that's where `MIRROR_SOURCE` copying lives now).
- **Per-clone offset / reverse / speed / canon stagger** — one clip across many clones, phase-shifted
  (the "pose-clip chorus"): dancers, patrols, rounds.
- **FROZEN_FRAME** — hold an evaluated pose (mannequin / bullet-time still).
- **RECORDED_STATE** — replay a captured performance (needs the anim recorder, Track A #8/#8-recorder).
- Animated-object attachments: drive the cloned `LLControlAvatar` / `ObjectAnimation` map under the
  same drive mode.
Depends on: #1 registry (per-instance drive-mode field). Researched previously as FSPosingMotion pose-clip.

## Track C — New Looks / Render Styles  ⏭️ (user request; expands "Style shuffle / palette presets")
NOTE: the old `mStyle`/FX params (hue/alpha/shimmer/pixelation/glitch/wireframe) were for the OVERLAY
ghosts. Entity clones render through the real-avatar/deferred path, so a "look" = a per-clone material
override and/or a clone-scoped post pass. Design a `GhostLook` enum on the instance + a small stylization
hook in the clone's draw. Candidate looks (creative wishlist):
- **Apparition / true ghost** — translucent + fresnel rim + faint inner glow; depth-aware spectral.
- **Hologram / X-ray** — additive scanlines, fresnel edge, flicker, chromatic offset; sci-fi projection.
- **Chrome / liquid metal** — metallic=1 roughness≈0 env-reflective override (T-1000).
- **Clay / matcap** — neutral untextured matcap; doubles as the lighting/blocking stand-in (Track A #14).
- **Toon / ink outline** — cel bands + Sobel/normal-depth silhouette edges; anime/comic.
- **Silhouette / shadow-puppet** — solid single-color fill + optional backlit rim; noir/title cards.
- **Dissolve / disintegration** — noise-threshold dissolve with emissive burn edge; materialize/vanish
  (pairs with staggered reveal wave).
- **Statue / bronze** — patina metal freeze ("turned to stone"); museum tableau.
- **Thermal / IR / night-vision** — false-color by depth or velocity; surveillance aesthetic.
- **Iridescent / thin-film** — oil-slick prism sheen over the base material.
- **Blueprint / onion-skin** — single-hue translucent line-art; ideal for echo/onion-skin features.
- **Datamosh glitch** — coherent RGB-split + block-displace + scanline tear "corrupted" look.
Cross-links: per-instance seeded "chaos" variation (below) can pick/tint looks per clone; array
gradients can ramp a look across a formation.

## Track D — Prior FUN backlog (2026-07-22, ranked fun-per-effort; mostly reuse per-instance params + makeArray)
⭐ Top picks:
- **Seeded chaos variation (S)** — UUID-seeded per-clone yaw/scale/hue/alpha/shimmer/glitch so a crowd
  reads as individuals, not clones. Single most transformative crowd feature.
- **Bullet-time freeze-strip (M)** — capture successive source poses into a line/arc/ring (Track A #11).
Cheap wins (S): formation presets (arc/grid/V/spiral/tunnel/scatter); array gradients; mirror clone;
look-at/face-target; style-shuffle presets (neon squad / spectral fade / glitch mob / wireframe chorus);
placement QOL (drop-to-ground, align-feet, reset-upright, duplicate-in-place, copy/paste transform).
Medium (M): echo trail; procedural formation motion (orbit/spin/breathe/ripple without touching the
source pose); staggered reveal wave ("army materializes"); group transform gizmo; formation pivot modes;
clone cue track (timeline cues for visibility/transform/style/freeze/FX).

## Adjacent research (not a clone feature)
- **Native real-time ray tracing** for the viewer — brief for OpenAI: RAYTRACING_RESEARCH_BRIEF_FOR_OPENAI.md.

## Suggested build order after #1
#1 Registry → Track B DIRECTED_CLIP (independent anim, user priority) → Track C GhostLook scaffold +
first 2–3 looks → Track A #2/#3 (actor adapter + paths) → chaos variation / bullet-time. Each: Codex
codes → Claude builds → in-world verify.
