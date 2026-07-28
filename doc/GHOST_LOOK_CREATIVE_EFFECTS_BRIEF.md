# Ghost Studio — Creative Effects Expansion (Codex brief, CREATIVE LATITUDE)

Extend the overlay ghost look system with (1) MORE creative looks and (2) a NEW **independent
DISTORTION layer** that stacks on ANY look. You have creative latitude — invent good effects
beyond the seed lists; the human wants a rich, playful set for machinima. Overlay path only
(`actorghostF.glsl` + `drawGeometryGhost`). Adversarial design first, then implement as one
`-Wswitch`-clean change. Render correctness is IN-WORLD only.

## Anchors (read before designing)
- Shader: `indra/newview/app_settings/shaders/class1/interface/actorghostF.glsl`. Current:
  `uniform int ghostLook` branches 13 looks (IDs 5-17) after a base path (styles 0-4). FX toolkit:
  screen-space scanlines, fresnel rim (`edge`), flicker/shimmer, glitch (chroma + HORIZONTAL tear,
  `ghostFx.z`), pixelate (`ghostAux.z`); helpers `ghost_hash/ghost_noise/ghost_fbm/heat_lut/
  ghost_rainbow`. Uniforms: `color`, `ghostParams`, `ghostAux`, `ghostFx`, `ghostLook`, `ghostSlot`.
  Inputs: `vary_texcoord0`, `vary_position` (eye-space), `vary_normal`, `vary_vertex_color`.
- Style enum: `llactormover.cpp:3576+` (`GHOST_STYLE_*`, 0..17). Draw: `drawGeometryGhost` style
  switch ~4453-4632; `ghostLook` uploaded via `uniform1i(sGhostLook, style)` ~3919 inside
  `apply_program` (gated by `have_fx`). Adding enum values is COUPLED with switch cases.
- UI: `style_combo` `panel_ghost_studio.xml` (mirror EVERY addition to `floater_director.xml` AND
  `panel_path_editor.xml` — Director superset rule). Per-instance sliders (hue/alpha/pixelate/
  shimmer/glitch/brightness) must keep feeding every look.
- ⚠️ BUILD GOTCHA: the incremental `alchemy-bin` build copies NEITHER shaders NOR skins to
  `build-Windows-vs2026-os/newview/Release/`. State clearly in your result that the human must copy
  `app_settings/shaders/**/*.glsl` + changed skin XML to the output after building.

## Part 1 — new creative LOOKS (extend the `ghostLook` branch + enum + switch + UI)
Seed ideas (ADD YOUR OWN — aim for distinct, game/machinima-flavored):
- FPS/game: **Thermal scope** (heat LUT + vignette + faint reticle), **Wallhack/ESP** (faint x-ray
  body + strong through-wall fresnel outline in the tint), **Night-vision tube** (green mono + IR
  bloom + grain + circular tube vignette), **Damage overlay** (red edge-vignette pulse over the
  texture), **Killcam** (desaturate + film grain + letterbox tint bars).
- Artistic: **Oil-slick / Iridescent** (thin-film color by view angle), **Vaporwave** (pink/cyan +
  perspective grid), **Halftone / Comic** (Ben-Day dots sized by luminance), **Sonar reveal**
  (a sweeping scan line that lights the body), **Hologram interference** (offset double-image).

## Part 2 — INDEPENDENT DISTORTION LAYER (the key architectural ask)
Add a SECOND control, ORTHOGONAL to the color look, so ANY look + ANY distortion combine
(e.g. "Thermal + Magnify", "Chrome + Wave").
- Add `uniform int ghostDistort` + a distortion-params vec4 (amount etc.). Apply the distortion as
  a UV / screen-space warp **BEFORE** the color-look branch samples, so it distorts whatever the
  look renders. Keep the existing pixelate + horizontal glitch working (fold pixelate in as one
  distortion type; leave the legacy per-instance glitch slider intact).
- Distortion types (enum; the human explicitly wants BEYOND the horizontal tear — add your own):
  **None**, **Pixelate** (fold in existing), **Voxel** (quantize `vary_position` into 3D blocks +
  facet-shade for a blocky look), **Lens / Magnify** (radial barrel/fisheye bulge — a magnifying-
  glass zone; center + radius params), **Wave / Ripple** (animated sine UV warp), **RGB split**
  (chromatic aberration offsets), **Block glitch** (rectangular block displacement, time-quantized
  so blocks hold), **Vertical tear** (vertical slice offsets — the vertical cousin of the current
  glitch), **VHS / tracking** (rolling bands + noise + slight desat).
- UI: a **"Distort"** combo + a **distortion amount** slider in the Look panel; per-instance,
  persisted like other look params; mirrored to Director + Path panels. Drive `ghostDistort` + params
  from `drawGeometryGhost` per instance (new `GhostDrawParams` fields + settings).

## Constraints
- Do NOT break the existing 13 looks or base styles 0-4; extend, don't rewrite. Prefer in-shader
  branching + param presets over duplicate programs.
- Overlay CLONE-family gating unchanged; per-instance params preserved; Director superset for every
  new control/look/distortion.

## Acceptance
Every existing + new look renders distinctly; the distortion layer combines with any look; both UI
sources carry the new controls; per-instance sliders work; enum/switch exhaustive; shader compiles;
result NOTES the shader+skins output-copy step.
