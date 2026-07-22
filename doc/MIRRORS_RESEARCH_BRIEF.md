# Better Mirrors (Hero Probes) — Deep-Research Brief

**Audience:** an external deep-research reviewer (OpenAI o-series / GPT-5) with web access.
**Codebase is open-source** — read the real source: Alchemy (https://github.com/AlchemyViewer/Alchemy)
or the LL viewer (https://github.com/secondlife/viewer). Relevant files:
`indra/newview/llheroprobemanager.{h,cpp}`, `llreflectionmapmanager.{h,cpp}`, `llreflectionmap.{h,cpp}`,
`pipeline.cpp` (mirror/probe render + `sReflectionRender`), `llviewerdisplay.cpp`, and the
`RenderMirrors` / `RenderHeroProbeUpdateRate` / `RenderHeroProbeConservativeUpdateMultiplier` settings.
Give a concrete, implementable architecture, not a survey.

## What this is
A Second Life viewer fork (Alchemy, OpenGL, deferred renderer) used for **machinima**. "Mirrors" =
the **Hero Probe** system (`LLHeroProbeManager`): a mirror-flagged surface (`LLVOVolume`) registers a
hero probe; each update the manager renders the scene from the mirror's viewpoint into a **cubemap**
(`mProbeResolution = 1024`, `mMaxProbeLOD = 6`), planar-clipped at the mirror plane
(`mCurrentClipPlane`, `mMirrorPosition`/`mMirrorNormal`), and reflections are sampled from that probe.
Constraints observed in source: **max 2 hero probes** (`LL_MAX_HERO_PROBE_COUNT = 2`); updates are
**throttled** (`RenderHeroProbeUpdateRate`, conservative multiplier); `isMirrorPass()`/`mRenderingMirror`
gate the mirror render; there's also a general reflection-probe system (`LLReflectionMapManager`) and
(if present) screen-space reflections.

## Goals (machinima; quality > perf on high-end HW — RTX 5090)
1. **Sharper, more complete, less-laggy mirrors** for close-up shots.
2. **Get content that's currently missing into mirrors** — reliably reflect **avatars**, and
   specifically the fork's **Ghost Studio "clones"** (client-only duplicate actors). Clones today draw
   as a **post-tonemap overlay in `render_ui`**, so they are NOT part of any probe/world render and
   never appear in reflections. (A parallel research track, `doc/SCENE_LIT_CLONE_RESEARCH_FINDINGS.md`,
   concluded clones should be injected into the deferred pass as a **view-aware frame-local render
   proxy**; mirror coverage is called out there as an explicit requirement.)

## Research questions (answer each; cite public source file/function)
**Q1 — Planar reflection vs cubemap hero-probe.** A flat mirror is planar; the current system renders a
**cubemap** from the mirror point. For a flat mirror, is a single **planar reflection** (reflect the
camera across the mirror plane, render once with an oblique/near clip plane at the mirror surface, at
high resolution) sharper and/or cheaper than a 6-face cubemap? Cover parallax correctness, resolution
vs a cubemap face, oblique-frustum clipping pitfalls, and what mature engines do for flat mirrors.
Recommend planar-vs-cubemap (or a hybrid: planar for the nearest flat hero, cubemap otherwise).

**Q2 — Coverage: what the probe render includes/excludes, and how to add avatars + clones.** Determine
(from `pipeline.cpp` under `sReflectionRender`/`mRenderingMirror`) which render-type masks the hero-probe
pass uses — are **avatars** drawn into mirrors, or masked out (a long-standing SL weakness)? Which of
dynamic objects / particles / water / sky / alpha are included? Then: (a) how to reliably include
**avatars** in the mirror render; (b) the minimal path to include the **Ghost Studio clones** — does it
strictly require the deferred-injection render-proxy (submitting the clone batches during the mirror
pass with the mirror's view·G), and if so what is the smallest correct hook (a `renderGhostProxies()`
call inside the hero-probe face render, view-tagged for the mirror camera)? Enumerate the state/clip-plane
caveats of submitting extra geometry inside the probe pass.

**Q3 — Temporal / update rate.** Throttling (`RenderHeroProbeUpdateRate`, conservative multiplier) makes
reflections LAG when the subject moves — bad for machinima. Design a **full-rate high-quality mirror
mode** (per-frame update for the nearest hero) and address in-probe temporal artifacts: is TAA/jitter
applied inside the probe, and does it ghost? Should the probe use unjittered projection? Cost tradeoff.

**Q4 — More mirrors + recursion.** Feasibility/cost of raising the 2-probe cap, and of **mirror-facing-
mirror** (infinity-mirror) via bounded recursion depth. What breaks (VRAM, per-face cost, clip-plane
recursion) and what's a safe machinima-only ceiling on the 5090.

**Q5 — Quality knobs / "cinematic mirror" preset.** `mProbeResolution`, `mMaxProbeLOD`, mip filtering,
radiance generation — which knobs most improve sharpness, and a recommended high-quality preset with a
VRAM/perf budget. Any hard limits (cubemap array size, format).

**Q6 — SSR as a complement.** Would screen-space reflections (if available in the fork) fill the
near-field / contact reflections the probe misses, and how do SSR + hero-probe compose without
double-reflecting?

**Q7 — Clip-plane & self-reflection accuracy.** Near-mirror geometry, back-face leakage, the mirror's
own surface, and edge artifacts from `mCurrentClipPlane` — the correct oblique near-plane setup and bias.

## Deliverable
A recommended mirror architecture (planar vs cubemap + when), the **minimal correct path to get
avatars and clones into mirrors** (with the exact pass/hook and state caveats), a full-rate cinematic
quality mode, and a risk list (most→least severe) each with a mitigation or a decisive in-viewer test.
Confirm or refute that clones-in-mirrors is gated on the deferred-injection render proxy from
`doc/SCENE_LIT_CLONE_RESEARCH_FINDINGS.md`.
