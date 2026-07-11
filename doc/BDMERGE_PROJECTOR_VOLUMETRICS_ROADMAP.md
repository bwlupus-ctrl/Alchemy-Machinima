# Projector Volumetric Cones — Cinema Roadmap

Post-ship enhancement plan for **G3.3** (per-projector volumetric light cones,
commit `1d2be84c50`; design brief in `BDMERGE_PROJECTOR_VOLUMETRICS_BRIEF.md`).
Goal: take the shipped effect from "works" to **cinema-grade** — more robust,
more filmic, and cheaper per frame. Scoped 2026-07-11 with user direction.

## Current implementation (baseline)
- `renderProjectorVolumetric()` in `pipeline.cpp` (after the sun volumetric block
  in `renderFinalize`), `setupSpotLightVolumetric()` (side-effect-free).
- `class3/deferred/projectorVolumetricF.glsl` (+ class1 stub): bounded ray↔sphere
  march (16 samples default), HG phase (g=0.72), per-slot self-shadowing via
  `sampleSpotShadow`, cookie/gobo modulation, per-slot additive blend.
- Runs **post-`colorCorrect`** (display stage) — the main thing Phase 1 changes.
- Gate `BDMergeProjectorVolumetrics` (default off) + Resolution/Multiplier/Anisotropy.
- Scope limit: all shadow-casting projectors (slots 0..N-1) currently emit shafts.

## Locked design decisions (user, 2026-07-11)
- **Per-projector flag is SESSION-BASED** (cleared on relog), toggled via the
  **right-click context menu** on a selected projector, and **OFF by default** —
  a projector emits a shaft only when explicitly flagged this session.
- **Shafts must be influenced by the tonemapper** (composite in linear HDR before
  tonemapping) and carry the **light's color**.
- Animated medium **noise must stay tasteful** — the naive/heavy noise look is a
  non-goal. Ship it subtle (or off) by default; quality over gimmick.
- Confirmed wanted: **feathered cone edge**, **crisp volumetric occluder shadows**,
  plus the perf/quality and atmosphere items below.

---

## Phase 1 — Quality + performance core (do first)
Mostly self-contained shader/pipeline work; makes it both better *and* faster.

1. **HDR-space composite before tonemapping.** Move `renderProjectorVolumetric`
   ahead of `colorCorrect` so the shafts are shaped by the active tonemapper
   (AMD LPM / ACES roll off the bright cores filmically instead of clipping).
   Eliminates the HDR-blowout risk noted in the brief (R2). Highest-value item.
2. **Blue-noise dithered march offset + temporal accumulation.** Per-pixel jittered
   start + across-frame accumulate → smooth at ~8 samples, and **no shimmer during
   camera moves** (mandatory for recording). TAA/jitter-aware.
3. **Half-res march + bilateral (depth-aware) upsample.** March at ¼ pixels, edge-
   preserving upscale. ~3–4× cheaper, near-identical look. Add a quality control.
4. **Screen-space scissor to each cone's projected bounds.** Only march the rect the
   frustum/sphere covers on screen — skips the majority of fullscreen work.
5. **Adaptive sample count** by on-screen cone size / distance.
6. **Feathered cone edge** — soften the angular falloff so the beam boundary isn't a
   hard line.
7. **Crisp volumetric occluder shadows** — higher-quality shadow taps inside the
   shaft so bars/foliage throw sharp god-ray patterns through the beam.

## Phase 2 — Art direction (the flag + per-light control)
1. **Session per-projector opt-in**, right-click context menu ("Volumetric Shaft"),
   OFF by default. Session-only UUID set (not persisted). Raymarch runs only for
   flagged, shadow-casting projectors. Mirror `ALDerenderList`'s selection plumbing
   but session-scoped.
2. **Per-light overrides** on flagged lights: intensity/density multiplier, cone
   softness, and a **shaft color tint** that can differ from the light color.
3. **Slot pinning** so a hero shaft keeps its shadow slot when other projectors are
   nearer.

## Phase 3 — Atmosphere (tasteful)
1. **Animated 3D noise medium** (scrolling curl/worley) → drifting dust motes /
   turbulence in the beam. Subtle default; quality-gated per the ruling.
2. **Height-based fog density** — denser near ground, thinning with altitude.
3. **Global density / "haziness" master** for per-shot atmosphere.
4. **Feed shafts into bloom** for a soft glow halo around the source.

## Sequencing
Phase 1 → Phase 2 → Phase 3. Phase 1 items 1–2 (HDR composite + dither) are the
biggest quality jump and should land together. Each phase is one or more
revertable commits; keep the existing `BDMergeProjectorVolumetrics` gate as the
master and add sub-controls per item.
