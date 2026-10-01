# Reflection probes: update on demand — design brief (for adversarial review)

**Date:** 2026-09-30
**Status:** DESIGN ONLY. Nothing implemented, nothing built. Per `CLAUDE.md`: Codex implements from
this brief, an Opus sub-agent attacks it, loop to 0 must-fix, THEN build once.
**Goal:** cut the per-frame cost of reflection-probe updates without changing what probes look like
when the scene is actually changing. Machinima first: static sets, a Live Probe on a subject,
long takes.

> **BASE BRANCH — READ FIRST.** Line numbers below are from `origin/fix/animesh-clone-pose-polish`
> (`f9ad494`), the newest pushed branch containing the Cine Light Rig **Live Probe** (`277f660`).
> The user has local, unpushed probe work from 2026-09-29 that this brief has NOT seen. Before
> implementing, diff that work against this brief and reconcile §2 — any scheduler change there
> supersedes the line references here.

---

## 1. Scope (stated up front, per "ship whole")

**In this delivery (one batch):**
- **A. On-demand scheduling** — ordinary probes re-render only when something they draw changed.
- **B. Realtime / Live Probe face slicing + dirty gating** — the per-frame 6-face re-render becomes
  N faces per frame, and only while dirty.
- **C. One cull per probe update** instead of one per face (output-identical CPU saving).
- **D. Probe-render detail budget** (GTA V lesson) — optional tiny-object cull / LOD bias inside probe
  renders. **Default OFF** (byte-identical); user-tunable.
- Settings, a Graphics/Lightbox UI group, the probe debug text, and log verdicts for all of the above.

**Explicitly deferred (NOT this delivery, each needs its own review):** one shadow map per probe
instead of 2 cascades per face; configurable supersample factor; relightable cached G-buffer probes
(Ubisoft lesson); fast GGX pre-filtering (Manson & Sloan). Listed so nobody expects them here.

---

## 2. What the code PROVES today (file:line on the base branch)

| Fact | Where |
|---|---|
| Every frame, if no update is in flight, the stalest probe is picked and one face is rendered. There is no "nothing changed" exit — this runs forever on a static scene | `llreflectionmapmanager.cpp:466-471` (pick), `:557-566` (start) |
| A full probe update = 12 face renders: 6 irradiance-pass faces then 6 radiance-pass faces | `doProbeUpdate`, `:819+`; comment block above `updateProbeFace` |
| Each face is a full scene render: `display_cube_face` → `updateCull` + `generateSunShadow` + G-buffer + deferred lighting | `llviewerdisplay.cpp:1288` |
| Probe faces render **2** sun-shadow cascades | `pipeline.cpp:22803` (`j < (gCubeSnapshot ? 2 : 4)`) |
| The main view's union shadow-cull optimisation is explicitly disabled for probe renders | `pipeline.cpp:22713` |
| Probe faces render at 4× probe resolution (512² for a 128 probe) | `llreflectionmapmanager.cpp` `mProbeResolution * 4` (render target + aux RT allocation) |
| **Non-dynamic probes do not draw avatars, animesh or particles** (`dynamic_render_types` toggled off) | `llviewerwindow.cpp:6090` |
| A probe is dynamic only if its object is flagged dynamic AND detail > Static | `llreflectionmap.cpp:241` |
| **The designated Live Probe takes the realtime slot and re-renders all 6 faces EVERY frame, even when `RenderReflectionProbeDetail` is not Realtime** (`cinematicLive ? cinematicLive : (realtime ? closestDynamic : nullptr)`) | `llreflectionmapmanager.cpp:495-514` |
| Live Probe readiness is two flags set by alternate realtime frames; `mComplete` only when both are set | `:509-530` |
| Avatar bounding boxes are built from the pelvis + every joint's world position — so an animation that walks the pelvis across the room moves the box even though the avatar position does not | `llvoavatar.cpp:1446` (`calculateSpatialExtents`) |
| Avatar extents refresh in staggered batches, not every frame | `llvoavatar.cpp:2819+` (`upd_freq`) |
| An occluded, complete probe is "refreshed" by bumping `mLastUpdateTime` without rendering | end of `update()`, `oldestOccluded` |

**Consequence:** today the cost is constant regardless of scene activity — ~1 face/frame for the
round-robin, plus 6 faces/frame whenever a Live Probe is designated (or the closest dynamic probe in
Realtime). That constant is what this brief removes.

---

## 3. Design

### A. On-demand scheduling (ordinary probes)

Each probe gets a **dirty bit** and a **dirty reason**. The round-robin picks only among **dirty**
probes (stalest first, same `check_priority` tie-break). A clean probe is never re-rendered, except
by the safety refresh.

**What marks a probe dirty — filtered by what that probe actually draws:**

| Source | Static probe | Dynamic probe |
|---|---|---|
| Static geometry added / removed / moved / changed LOD inside the probe's influence volume (octree group within radius goes GEOM_DIRTY, drawable moved, object rezzed/killed) | ✔ | ✔ |
| Local light inside the radius changed (on/off, colour, radius, moved) | ✔ | ✔ |
| Environment changed past a threshold: sun/moon direction (angle > `RenderProbeDirtySunDeg`, default 0.5°), sun/ambient colour (ΔE / luminance ratio > `RenderProbeDirtyEnvTol`), sky/water settings asset swapped | ✔ | ✔ |
| **Avatar / animesh / entity-clone bounding box** moved more than `RenderProbeDirtyMoveFrac` × probe radius (default 0.1), or entered/left the radius. Uses the **extents box (pelvis + joints)**, never agent position, so animation-driven movement counts | ✘ (never drawn) | ✔ |
| Particles inside the radius | ✘ | ✔ (rate-limited, see below) |
| Probe itself moved / resized / became relevant / got a new cube slot | ✔ | ✔ |

**Limits (all three always apply):**
1. **Per-probe minimum interval** `RenderProbeMinInterval` (default 0.25 s): a probe dirtied again
   while inside its interval stays dirty and waits. Constant motion ⇒ a steady few updates/sec.
2. **Global face budget per frame** `RenderProbeFaceBudget` (default 1 = today's round-robin rate).
   **Hard guarantee: the on-demand path never renders more faces per frame than today.**
3. **Safety refresh** `RenderProbeMaxAge` (default 60 s): a clean probe older than this is treated as
   dirty with reason `SAFETY`. Catches anything the dirty sources miss. 0 = off (pure on-demand).

The default (sky) probe keeps its existing `RenderDefaultProbeUpdatePeriod` behaviour but is also
gated: it re-renders on environment dirty or at that period, not unconditionally.

**Master switch:** `RenderProbeOnDemand` (default **true** once verified; ship as false for the first
in-world A/B). When false the scheduler is byte-for-byte today's code path.

### B. Realtime probe + Live Probe: face slicing and dirty gating

- **Face slicing:** `RenderProbeRealtimeFacesPerFrame` (1–6, default 2). The realtime/live probe
  renders that many faces per frame, cycling. Irradiance and radiance passes each complete after
  `6 / N` frames; the pass flips only when a pass's 6th face is done (not every frame as now).
- **Dirty gating for the Live Probe:** it re-renders only while dirty. Dirty sources = §A table
  (dynamic column) **plus** its own origin moving (it follows a rig target — Self/Subject A–D — so a
  moving subject is a moving probe), plus any change to the rig's active projectors / ignored /
  pinned light lists (`setCinematicLiveProbe`). A still subject in a still set ⇒ **zero** face renders
  after warm-up.
- **Readiness must stay correct under slicing:** `mCinematicIrradianceReady` / `…RadianceReady` are set
  when a pass's **sixth** face completes, not per frame. `mComplete` and `updateNeighbors` only when
  both are set, as today.
- The realtime closest-dynamic probe (no Live Probe designated, detail = Realtime) gets the same
  slicing and the dynamic-column dirty gating.
- **Scratch slot:** the realtime path uses `sourceIdx + 1` as its scratch cube. With slicing, the
  scratch slot now holds a partially-written cube across frames — nothing else may write it in
  between (see §7.2).

### C. One cull per probe update

For the six faces of one pass, do **one** cull against the probe's bounding sphere (radius = probe
draw distance), then per face re-bucket against that face's frustum — the same shape as the main
view's union shadow cull (`pipeline.cpp:22713`). Output must be pixel-identical to per-face culling;
this is a CPU saving only. Gated by `RenderProbeSharedCull` (default true after A/B).

### D. Probe-render detail budget (default OFF)

Inside `gCubeSnapshot` only: skip drawables whose projected size at the probe's resolution is below
`RenderProbeMinPixelArea` (default 0 = off), and add `RenderProbeLODBias` (default 0 = off) to volume
LOD selection. Never applies to the main view, shadows of the main view, hero probes, or VCam/Prism
feeds. User-facing as "Probe detail" (Full / Balanced / Fast).

---

## 4. Surfaces (all in one delivery)

- **Settings** (`settings_alchemy.xml`): `RenderProbeOnDemand`, `RenderProbeFaceBudget`,
  `RenderProbeMinInterval`, `RenderProbeMaxAge`, `RenderProbeDirtySunDeg`, `RenderProbeDirtyEnvTol`,
  `RenderProbeDirtyMoveFrac`, `RenderProbeRealtimeFacesPerFrame`, `RenderProbeSharedCull`,
  `RenderProbeMinPixelArea`, `RenderProbeLODBias`.
- **UI:** a "Probe updates" group where the existing reflection/probe controls already live
  (Graphics → the fork's rendering panel that holds detail/level/SSR/mirrors/hero), with
  On-demand toggle, Realtime faces/frame, Safety refresh, Probe detail. Same controls reachable from
  the Lightbox next to the Live Probe block.
- **Presets/reset:** register all new settings with the rendering preset save/restore and reset.
- **Debug:** extend the existing `RENDER_DEBUG_PROBE_UPDATES` debug text (it already labels probes
  with their last update time) to show the dirty reason: `OBJ / LIGHT / ENV / AV / PART / MOVE /
  SAFETY / LIVE`.

---

## 5. Instrumentation — the log states a verdict

Once per 5 s while probes exist (`LL_INFOS("ProbeSched")`, rate-limited):
`faces=<n>/s  obj=<n> light=<n> env=<n> av=<n> part=<n> move=<n> safety=<n> live=<n>
skipped_clean=<n> budget_hits=<n> maxlag=<s>`

- **`PROBE-ONDEMAND OK`** — faces/s ≤ today's baseline and no probe exceeded `MaxAge + MinInterval`.
- **`PROBE-ONDEMAND STARVED <probe>`** — a dirty probe waited > 5 s because of the budget (budget too
  low for the scene; not a bug, but visible).
- **`PROBE-ONDEMAND OVER BUDGET`** — more faces in a frame than `FaceBudget` (+ realtime slice). This
  is a bug; must never fire.
- Existing Tracy zones (`"probe update"`, `"display cube face"`, `"rmmu - realtime"`) remain the
  timing truth.

---

## 6. Off-path must be inert (CLAUDE.md rule 1)

- `RenderProbeOnDemand = false` ⇒ today's scheduler, unchanged. `RenderProbeRealtimeFacesPerFrame = 6`
  ⇒ today's realtime/live behaviour. `RenderProbeSharedCull = false` ⇒ per-face cull as today.
  `MinPixelArea = 0`, `LODBias = 0` ⇒ no detail change.
- **Shared state:** C touches `updateCull` / cull-result reuse. Reviewer must prove the main-view cull,
  main-view shadow cull, hero probe, and VCam/Prism feed culls are untouched (they must not see a
  reused probe cull result). D must be provably scoped to `gCubeSnapshot` and not leak LOD state into
  the next main-view frame.
- No GL blend/colour-mask/draw-buffer changes anywhere in this brief.

---

## 7. Reviewer: attack these first (least-sure list)

1. **Dirty hooks.** Where exactly does the viewer learn that geometry inside a radius changed?
   Candidates: `LLDrawable::updateMove`, octree insert/remove, `LLSpatialGroup` GEOM_DIRTY, object
   create/kill in `LLViewerObjectList`. Find what the proposal would MISS (texture finishing a fetch
   and changing a probe's look? media faces? alpha changes? flexi?). Anything missed is covered only by
   the safety refresh — say which.
2. **Scratch slot under slicing** (§B): the realtime path writes `sourceIdx + 1`; the round-robin
   writes `sourceIdx`. With slicing, is there any path (Live Probe re-designation, probe count change,
   `initReflectionMaps` realloc, teleport `reset()`) that reuses or clears the scratch cube mid-cycle?
3. **Pass alternation.** Today `mRealtimeRadiancePass` flips every frame. With slicing it must flip per
   completed 6-face pass. Check `isRadiancePass()` consumers in the pipeline (`mLightScale`, ambiance)
   don't assume per-frame alternation.
4. **`oldestOccluded`** fakes an update by bumping `mLastUpdateTime`. Does that defeat the safety
   refresh, or mark an occluded dirty probe clean without rendering it?
5. **Env-change threshold** on a running day cycle: at default day length, how many faces/s does
   0.5° of sun produce? If "always dirty during a day cycle", say so and propose the right metric.
6. **Avatar extents lag** (`upd_freq`): can a fast animation move an avatar through and out of a
   probe radius between two extent refreshes, so the probe never goes dirty?
7. **Is the approach wrong?** Specifically: should on-demand be per-probe-dirty (this brief) or
   per-octree-node change counters sampled against probe bounds? Argue it.

---

## 8. In-world test (outcomes stated in advance)

Run with the log verdict line visible. Toggle `RenderProbeOnDemand` for each A/B.

1. **Static set, no Live Probe, detail = Dynamic.** *Off:* faces ≈ fps (1/frame). *On:* faces drop to
   ≈ safety rate (probe count / 60 s). Image identical.
2. **Rez / move a prim near a probe.** *On:* that probe logs `obj` within `MinInterval`, reflection
   updates; others stay clean.
3. **Drag the sun slider.** *On:* `env` updates, capped at `FaceBudget`/frame; no `OVER BUDGET`.
4. **Live Probe on a still subject.** *Off:* 6 faces/frame forever. *On:* warm-up (12 faces over
   `12/N` frames), then `live=0` while nothing moves.
5. **Play a walk animation (no control input) on the Live Probe subject.** *On:* `move`/`av` fire as
   the pelvis carries the box; reflection follows at ≤ `MinInterval` cadence.
6. **Static probe + animated avatar walking through it.** *On:* the static probe does **not** go dirty
   from the avatar (it never draws avatars). If it does, the filter is wrong.
7. **`RealtimeFacesPerFrame` 6 vs 2** with detail = Realtime: fps up, radiance lag ≈ 3 frames per
   pass; Live Probe `mComplete` still reached.

If step 1 shows `faces` unchanged with On, the feature is not engaged regardless of fps.
