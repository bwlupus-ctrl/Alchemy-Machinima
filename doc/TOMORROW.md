# Next session — pick up here

Written 2026-07-25, end of a long session. HEAD `6e94ae00c48`.
Exe built 23:08: `build-Windows-vs2026-os/newview/Release/AlchemyTest.exe`

---

## 1. ⚠️ DO THIS FIRST — verify, do not add

Per `CLAUDE.md` rule 5. A large amount of last session's work is committed, built, and **has never
been seen working**. That is a liability, not progress.

**Built but NEVER tested in-world:**
- **Forward-only scoping** (`6ca9b2f065b`) — the coverage attachment, the FX gate, the new
  `SL_BridgeDebug` → **Surface coverage** view. *Expected: forward surfaces (alpha/water/fullbright)
  YELLOW, ordinary opaque geometry BLUE, sky BLACK. **If it is ALL BLUE the gate is failing closed
  and the sidecar is inert.*** Magenta/black stripes = semantic invalid.
- **S3 water** — water should now publish zero-diffuse (specular-only BRDF) instead of seabed albedo.
- **S4 / S2 fullbright K** — the S2 shader fix REPLACED a change that broke the render (opaque
  surfaces went transparent). It is unverified. Check opaque fullbright with alpha in its texture
  stays opaque.
- **Particle suppression** (`7f092fcc84f`) — hide the UI with touch spirals / selection beams /
  object-chat particles present; only those should vanish, scripted world particles must remain.
- **The whole camera operator** (`854eda0c14d`, `d275d9f4509`, `997ab30c11d`) — every locomotion
  mode, Auto, the vehicle layers, the authority sliders. None of it has been run.

Codex wrote a 12-step one-login regression pass covering the sidecar work — it is in the round-1
albedo doc. Step 1 (opaque fullbright with a non-1 alpha texture) is the exact case that broke.

---

## 2. NEW REQUEST — a more REACTIVE cinematic camera to the world

User's ask, verbatim: *"i wanted more of a reactive cinematic camera to the world."*

**Nothing in the camera code is world-aware today.** Verified: no raycasts, no geometry queries, no
object-list lookups anywhere in `alpanelcinecamparams.cpp`, `llcameraoperator.cpp` or
`llagentcamera.cpp`. Every existing mode is procedural — it plays a pattern regardless of what is
actually in front of the lens.

**Design it with Codex FIRST** (per `CLAUDE.md`), and ship it WHOLE (engine + settings + UI + preset
wiring), because "reactive" could mean several materially different features:

- **Geometry-aware framing** — raycast so the camera does not clip through walls, push in when the
  subject is occluded, hold the shot when a pillar crosses frame. The most obviously "cinematic".
- **Subject-aware reframing** — notice the subject moved and RE-COMPOSE like an operator would,
  rather than rigidly tracking. The operator already has a `recompose` layer; this would drive it
  from the world instead of a timer.
- **Event-driven** — react to avatars entering/leaving frame, to animation starts, to chat/sound.
  Most powerful for unattended filming, most work.
- **Environment-driven** — respond to light level, time of day, indoor/outdoor.

Ranked instinct: geometry-aware framing first (it fixes shots that are actively broken by walls),
subject-aware reframing second (it is the one that reads as "an operator is watching"), event-driven
third.

Existing pieces to build on: the operator's recompose layer and speed EMA; the Cinematic Camera's
target/bone-lock resolution; `LLViewerObjectList` for what is nearby.

---

## 3. Still open from last session

- **Motion ghosting** — unresolved. **NOT the flip settings** (X=true, Y=false is algebraically
  correct). Suspect coverage/history/disocclusion.
- **Shutter Blur quality drop** — never chased. The whole velocity path is new relative to the exe
  the user ran before that session (four velocity shaders plus the projection-discontinuity logic
  were uncommitted work swept into `a2bfceed82a`). Prime suspect is motion-vector MAGNITUDE, not
  correctness — try `Motion scale`, currently 1.00, before touching code.
- **Camera operator: fixed simulation quantum** (`FlycamOperatorSimulationHz`). Phase reset already
  makes takes repeatable at a steady frame rate; the quantum is what makes them repeatable across
  different ones.
- **Albedo/RTGI: CLOSED.** Correct GI, too strong for taste — lower `Bounce Lighting`, no code.
  One untested side effect to watch: emitters now contribute light but reflect nothing, so look for
  **missing bounce / dark cut-outs around LARGE emissive surfaces** (the neon signs), not more red.
- **FX3** — `RenderTargetWriteMask` under `#if !SL_MODE_OWNED` is a normals-ownership change that
  rode in on an albedo feature. Deliberately untouched, pending a scope decision from the user.
