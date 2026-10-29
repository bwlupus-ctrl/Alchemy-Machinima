# Rig-Fed Cinematic Lens Flare + Starburst — Design (DRAFT)

Branch: feature/cine-light-rig. Goal: a polished, refined, robust, cinematic lens
flare + diffraction "star" drawn at each ACTIVE CINEMATIC LIGHT RIG PROJECTOR
(rig-fed, deterministic — not screen-space blind detection), pairing with the
volumetric projectors. Visual bar: the ReShade `VirtualCinema_LensFlare.fx`
reference (full Sapphire-style stack + 19 lens presets). Reimplemented as
ORIGINAL GLSL in the native pipeline (standard optical-flare techniques; not a
copy of the .fx).

## Contracts (hard)
- OFF = byte-identical: master strength 0 (or feature disabled) early-outs before
  any new texture fetch / transcendental, exactly like the current sun flare's
  `if (density<=0) return` idiom. Default settings = feature OFF.
- Existing SUN flare path preserved: the sun/moon source keeps working; the new
  multi-source path is additive and gated.
- Rig-fed sourcing: N projector sources (uv, linear color, intensity, visibility)
  computed CPU-side each frame and uploaded; shader loops over them. N cap set by
  uniform budget (LIGHT_COUNT=4 rig lights + sun => cap ~8 is plenty).
- Per-source depth occlusion + on-screen edge fade (reuse the sun logic per src).
- Sub-effect gating: each element's intensity 0 => its math is skipped.

## Element stack (per source) — the "full polished stack"
Screen-space, drawn at source UV `s`, toward frame center `c=(0.5,0.5)`, axis
`a = c - s`. Aspect-corrected distances. All additive, tinted by source color
(SourceColorAmount) blended with element tints.
1. Core glow — hot multi-scale radial glow at `s` (sum of 2-3 gaussians).
2. Diffraction spikes (STAR) — `pow(|cos(theta*N + rot)|, sharpness)` angular
   burst × radial falloff; N = spike count (even lobes), optional 2nd rotated set
   for cross/anamorphic; chromatic tip shift.
3. Anamorphic streak — long horizontal gaussian streak (length/width/falloff),
   two-tone core->tip tint, chromatic R/B vertical split.
4. Chromatic ghost chain — GhostCount soft discs stepped along axis `a`
   (dispersal spacing), R/B radial split (ghostChroma), brightness falloff.
5. N-gon iris ghosts — polygonal aperture discs (IrisSides blades) marching along
   the axis, bright-rimmed; per-element hash variation.
6. Halo — chromatic ring at HaloRadius opposite the source, R/B split.
7. Chromatic dispersion rings — RingCount concentric rainbow rings (cosine
   spectrum, RingDispersion sweep) at RingRadius.
8. Spectral lens circles — CircleCount scaled tinted copies of the source ringing
   outward through the optical axis.
9. Spectral arc — partial rainbow arc on the far side of center.
10. Lens warp (barrel/pincushion) applied to ghost/circle/ring/halo layer;
    streak/spikes stay straight.

Helpers: gamma-2.0 lin/gam, luma, cosine spectrum palette, n-gon SDF (frac angle
fold), radial warp, per-element hash, TPDF dither on the composite (dissolve
low-amplitude ring/halo banding at 8-bit write).

## 19-preset parameter tables (adapted from the VirtualCinema reference)
Each preset sets: threshold/knee (only relevant if a highlight fallback is added;
for rig-fed these gate intensity), streakInt, streakTint(rgb), ghostCount,
ghostDisp, ghostInt, ghostChroma, haloInt, haloWidth, haloChroma, ringInt,
ringRadius, ringWidth, ringDisp, ringCount, circleInt, circleScale,
circleSpacing, circleCount, core, spikeInt, spikeCount, spikeLen, irisInt,
irisCount, irisSides, irisSize, arcInt.

Presets (id: name):
0 Manual (sliders)
1 Anamorphic Blue    streak 1.2 tint(.35,.55,1) ghosts4/.35/.22/.012 halo .28/.35/.015 ring .15/.30/.10/1.0/1 circ .12/.15/.14/5 proc core.35 spk.10/6/.40 iris.15/4/6/.055 arc.08
2 Anamorphic Gold    streak 1.1 tint(1,.78,.42) ghosts4/.33/.24/.014 halo .30/.33/.016 ring .16/.29/.10/1.0/1 circ .13/.15/.13/5 proc core.35 spk.12/6/.40 iris.16/4/6/.055 arc.08
3 Spherical Prime    streak .35 tint(.80,.85,1) ghosts5/.30/.28/.010 halo .22/.30/.012 ring .12/.32/.09/1.0/1 circ .15/.14/.12/6 proc core.25 spk.32/8/.30 iris.26/6/9/.050 arc.06
4 Vintage Uncoated   streak .55 tint(1,.72,.50) ghosts8/.28/.40/.022 halo .55/.40/.024 ring .35/.30/.12/1.3/2 circ .30/.16/.16/7 proc core.45 spk.15/12/.25 iris.40/8/5/.070 arc.22
5 Master Anamorphic  streak 1.4 tint(.40,.60,1) ghosts2/.40/.14/.008 halo .20/.30/.010 ring .08/.30/.08/1.0/1 circ .06/.14/.12/3 proc core.28 spk.06/4/.45 iris.10/2/8/.045 arc.04
6 Sci-Fi / Neon      streak 1.6 tint(.30,.90,1) ghosts7/.36/.45/.028 halo .50/.42/.030 ring .40/.35/.13/1.6/3 circ .35/.18/.17/7 proc core.50 spk.45/10/.50 iris.35/7/6/.065 arc.30
7 Minimal Glint      streak .30 tint(.85,.90,1) ghosts1/.30/.08/.008 halo .12/.28/.010 ring .05/.28/.08/1.0/1 circ .04/.14/.12/2 proc core.15 spk.12/6/.25 iris.06/1/7/.040 arc.03
8 Rainbow Prism      streak .60 tint(.70,.80,1) ghosts6/.34/.30/.020 halo .35/.34/.025 ring .45/.30/.13/1.8/3 circ .40/.16/.16/7 proc core.35 spk.25/14/.35 iris.30/7/6/.065 arc.45
9 Dreamy Halo        streak .25 tint(.90,.92,1) ghosts3/.30/.15/.012 halo .70/.42/.020 ring .15/.32/.12/1.0/1 circ .18/.16/.14/5 proc core.55 spk.08/4/.50 iris.12/3/9/.060 arc.12
10 JJ Blue Blast     streak 2.0 tint(.30,.55,1) ghosts6/.40/.35/.015 halo .35/.35/.015 ring .20/.30/.10/1.2/2 circ .15/.15/.14/4 proc core.65 spk.18/4/.60 iris.22/6/8/.055 arc.08
11 Retro 70s Warm    streak .50 tint(1,.72,.45) ghosts8/.28/.40/.024 halo .50/.40/.024 ring .35/.30/.13/1.3/2 circ .35/.17/.16/7 proc core.40 spk.18/10/.30 iris.45/8/5/.075 arc.20
12 Cyberpunk Neon    streak 1.5 tint(1,.30,.90) ghosts7/.36/.45/.028 halo .50/.42/.030 ring .40/.35/.13/1.6/3 circ .40/.18/.17/7 proc core.50 spk.42/12/.45 iris.35/7/3/.060 arc.35
13 Ethereal Angelic  streak .30 tint(1,.98,.92) ghosts4/.30/.18/.014 halo .65/.44/.018 ring .18/.33/.12/1.0/1 circ .22/.16/.14/6 proc core.60 spk.15/6/.55 iris.10/3/9/.060 arc.10
14 Golden Hour       streak .80 tint(1,.70,.45) ghosts3/.32/.18/.012 halo .40/.38/.018 ring .12/.32/.11/1.0/1 circ .15/.16/.14/4 proc core.55 spk.10/6/.45 iris.12/3/7/.060 arc.06
15 Noir Practical    streak .35 tint(.75,.85,1) ghosts2/.30/.12/.008 halo .18/.30/.010 ring .05/.30/.09/1.0/1 circ .05/.14/.12/2 proc core.30 spk.06/4/.30 iris.06/2/8/.045 arc.02
16 Documentary Real  streak .20 tint(.90,.95,1) ghosts2/.28/.10/.006 halo .10/.28/.008 ring .03/.30/.08/1.0/1 circ .03/.14/.12/2 proc core.18 spk.05/6/.20 iris.05/2/7/.040 arc.02
17 Blockbuster T-O   streak 1.3 tint(.25,.75,.90) ghosts5/.36/.28/.014 halo .32/.34/.016 ring .15/.32/.11/1.2/2 circ .18/.16/.15/5 proc core.50 spk.15/6/.45 iris.20/5/8/.055 arc.10
18 Sodium Night      streak .55 tint(1,.62,.25) ghosts6/.30/.35/.020 halo .45/.40/.022 ring .18/.33/.12/1.2/2 circ .22/.17/.15/6 proc core.45 spk.10/8/.30 iris.30/6/5/.065 arc.12
19 Music Video Glam  streak .90 tint(1,.85,.95) ghosts5/.34/.30/.018 halo .60/.42/.022 ring .22/.34/.12/1.1/2 circ .30/.17/.15/6 proc core.55 spk.30/12/.40 iris.25/6/9/.060 arc.20

MasterIntensity: one-knob scale over the whole flare, independent of preset.

## Integration (mapped)
**Projector world positions (rig-fed, Path A — authoritative):** rig lights are real
`LLVOVolume` light prims tagged `LOCAL_OBJECT_CINE_RIG_EMITTER`. Enumerate via
`ALCineLightRigManager::instance()` (enabledMask()/isSlotLit(slot)/at(slot)); per
rig, per light i in 0..LIGHT_COUNT(=4): `LLUUID id = rig.projectorId(i)` (null =
dead/absent => skip), `const RigFrame& f = rig.lastFrame()` gives `f.mProj[i]`
(EmitterState: mOn, mIntensity, mSR/mSG/mSB, mClipped); resolve prim via
`gObjectList.findObject(id)` -> world pos `o->getPositionAgent()` (agent space,
matches the deferred modelview basis), linear color `((LLVOVolume*)o)->
getLightLinearColor()`. DON'T use the object-list scan (Path B) — it also returns
omni fills + catchlight (same tag). Only the 4 spot projectors should flare.
Realistic ceiling: 2 subjects x 4 = 8 sources.

**Project world->UV:** in `LLPipeline::colorCorrect()` (pipeline.cpp ~11299-11405,
right after the sun block). Mirror the sun path but w=1:
`get_current_projection()*get_current_modelview()*vec4(pos_agent,1.0)`, reject
clip.w<=0, NDC divide, *0.5+0.5, reuse the sun edge-fade (margin 0.2, ~11344-49).
Depth (DEFERRED_DEPTH) + diffuse already bound (~11246-47). Per-source visibility
temporal smoothing => small std::array<F32,N> on LLPipeline (sun uses one scalar
mLensFlareSunVisibility).

**Uniform arrays:** LLGLSLShader::uniform4fv(index,count,ptr). Precedent =
gDeferredMultiLightProgram (pipeline.cpp ~17228; MULTI_LIGHT/MULTI_LIGHT_COL,
cap 16). Register enums in llshadermgr.h (LENS_FLARE block ~394-418) + matching
names in llshadermgr.cpp (~1596-1620) IN THE SAME ORDER. Pack 2 vec4 arrays +
count: `uCineFlareA[N]=(uv.x,uv.y,vis,intensity)`, `uCineFlareColor[N]=(r,g,b,scale)`,
`int uCineFlareCount`. **Compile-time cap AL_CINE_FLARE_MAX=8.**

**Pass architecture: Option (a) — extend the inline computeLensFlare loop
(RECOMMENDED).** Flare is called inline in colorCorrectF.glsl:112 in LINEAR,
full-res, PRE-tonemap (correct — tonemap rolls off + bloom picks it up; the ND/
polarizer "pre-bloom" comment refers to onLensFiltersF, a different pass). Refactor
computeLensFlare into a body parameterized by (uv, visibility, color) and loop:
source 0 = existing sun (math UNTOUCHED), then append the rig sources. Shape/preset
uniforms stay global (one look across sources). No new RT/program/dispatch. Files:
postEffectUtilsF.glsl (loop refactor), pipeline.cpp (enumerate+project+pack+upload
after sun block), llshadermgr.h/.cpp (array enums), LLPipeline per-source vis state.
Perf: hoist/gate the per-source depth-occlusion Poisson loop (up to 32 taps) —
early-continue when CPU visibility<=0; reduce taps for secondary sources. Defer the
dedicated half-res pass (Option b) unless N=8@4K profiles as the bottleneck (and
half-res would soften the sharp starburst anyway).

**Off-path byte-parity:** master `RenderCineLensFlareStrength` default 0. When 0
(or count 0) the CPU skips enumeration and uploads count=0; shader guards the new
loop with `if(uCineFlareCount>0)`. Contribution is purely additive so a zero-count
loop leaves the existing sun-only output BIT-IDENTICAL. Keep sun as source 0 with
untouched math + unchanged final scale.

**Settings:** `indra/newview/app_settings/settings_alchemy.xml` (NOT settings.xml);
RenderLensFlare* block ~2661-2896. Add RenderCineLensFlare* keys + preset index in
the same style.

**UI:** no existing flare panel (flare is debug-settings-only today). Home the
19-preset dropdown + controls in the **Lights panel** (`alpanelcinelightrig.{h,cpp}`
+ its paired XUI under skins/.../xui/en/), **as a collapsible sub-section placed
directly UNDER the Volumetrics controls** (user request) so it reads as part of the
projector/volumetric group and does not clutter the main rig controls. Preset table
= CPU table stamping the individual RenderLensFlare*/RenderCineLensFlare* controls
(like MasterSetup presets), mirroring the gaze Range/Lean preset boxes (pick ->
stamp all controls -> combo snaps back to "Choose preset...").

**Gaze-panel follow-up (unrelated to flare, fold into the flare UI pass):** on the
Movement Style row, the user wants each dropdown's reset adjacent to its own combo.
Current: reset_gaze_priority at left=470 (inner, resets Movement Style), reset_gaze_
ik_lean at left=486 (outer, resets Lean). Revisit if the user still wants them split
next to each combo rather than bunched at the right.

**Risks:** per-source occlusion tap cost (CPU pre-gate); enumeration correctness
(use projectorId(i), not tag scan); uniform enum ordering must match across .h/.cpp;
keep the sun accumulation additive/untouched; freshly-created projector null for a
few retry ticks => treat null as "no source".

## Delegation
- Fable (math/GLSL): the multi-source flare shader — all 10 elements + helpers +
  the 19-preset tables + MasterIntensity + off-path early-out. Perf-conscious
  (skip zero-intensity elements; cap N).
- Sonnet (pipeline C++ + UI): enumerate active rig projectors -> project to
  screen UV + color + intensity + visibility -> upload uniform arrays; register
  the new pass/program if dedicated; settings.xml keys; UI panel with the
  19-preset dropdown + controls (like the gaze preset boxes).
- Codex/Opus: adversarial review — off-path byte-parity, uniform bounds, perf,
  correctness of the source projection + occlusion.
