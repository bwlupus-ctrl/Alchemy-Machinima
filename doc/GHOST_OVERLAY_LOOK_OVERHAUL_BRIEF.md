# Ghost Studio — Overlay Look Overhaul (Codex brief)

**Scope:** the OVERLAY ghost look path only (the cheap harvested-batch path drawn via
`actorghostF.glsl`, style-switched in `drawGeometryGhost`). This is SEPARATE from the
scaffolded entity-clone look combo (`entity_look_combo`, Normal/Apparition/Hologram*/
Chrome*/Toon*/Silhouette* — "advanced shaders deferred"); unifying them is out of scope
here but note the overlap so the two don't diverge needlessly.

Produce an **adversarial design first** (shader-uniform strategy + risks), then implement
as ONE cohesive, `-Wswitch`-clean change. Render correctness is **in-world only**.

## Anchors (read these before designing)
- FX shader: `indra/newview/app_settings/shaders/class1/interface/actorghostF.glsl` (+ `V`).
  Uniforms: `color` (tint+alpha), `diffuseMap`, `ghostParams`(x scan, y rim, z flicker,
  w period px), `ghostAux`(x mask-cutoff, y tex-RGB mix 0..1, z pixelate, w phase),
  `ghostFx`(x shimmer-Hz, y shimmer-int, z glitch, w brightness), `ghostSlot` (indexed
  slot filter). Has: scanlines, fresnel rim, flicker/shimmer, glitch (chroma+tear),
  pixelate, `ghost_hash`, screen-space `gl_FragCoord`.
- Style enum: `llactormover.cpp:3576` (GHOST_STYLE_GHOST/CLONE/HOLOGRAM/WIREFRAME/XRAY).
  Adding enum values is COUPLED with adding their `switch` cases (`-Wswitch` under
  warnings-as-errors) — no safe partial increment.
- Draw: `drawGeometryGhost` (`llactormover.cpp`): prime pass ~4410-4448; per-style shade
  cases 4453-4585; the parameterized `draw_batches` lambda 3969-4130. NOTE `clone_tex`
  currently conflates THREE things: bind texture, set `tex_mix=1`, AND force a near-white
  per-draw color (4119-4126). `alpha_aware` drives mask/blend + per-slot indexed redraw
  (4043-4047, `have_fx && slot_count>1 && (clone_tex || (alpha_aware && is_mask))`).
- UI: `style_combo` `panel_ghost_studio.xml:121` (+ tooltip :122). MIRROR every look into
  the Director Console Look dropdown (`floater_director.xml`) — superset rule. The
  per-instance sliders (hue/alpha/pixelate/shimmer/glitch/brightness, plumbed via
  `ghostFx`/`ghostAux`/`color`) MUST keep working for every look.

## Fix 1 — Hologram + X-ray → TEXTURED + FX
Today they draw flat-tint (`draw_batches(SWEEP_ALL, /*clone_tex=*/false, true)` → tex_mix=0),
so the recognizable face vanishes. Make them bind the real texture with `tex_mix≈1` so the
face reads, with FX modulating on top (holo = tint + scanlines; x-ray = rim). **Decouple
`draw_batches`:** separate "bind texture + tex_mix" from "force clone near-white color", so
a style can be textured while KEEPING its style tint (cyan holo, cool x-ray). Preserve the
per-slot indexed redraw for textured multi-material heads. Shader already supports this via
`base = mix(white, tex.rgb, ghostAux.y) * color.rgb`.

## Fix 2 — Ghost → CLEAN spectral silhouette
Root cause of the "melt": the shade pass draws `SWEEP_ALL` with `GL_BLEND` ON, so coplanar
face layers (skin+makeup+brows) OVER-ACCUMULATE and translucency reveals interior meshes
(eye sockets, inner mouth) as dark voids. Fix: draw ONE clean front layer (e.g. prime
frontmost depth, then a single depth-EQUAL/LEQUAL blended pass; or draw solids blend-off to
a clean base then a single translucency pass) so there's no coplanar double-blend; keep it
faint/translucent; occlude the interior. Result = smooth spectral silhouette, no melt/voids.

## New looks (each = style enum value + shade case + params; reuse the toolkit, add small
## shader math where noted). Prefer branching in-shader on a LOOK-ID uniform + param presets
## uploaded from drawGeometryGhost over many near-duplicate programs.
- **Thermal** — luminance/rim → blue→green→yellow→red heat LUT, opaque. [new fn]
- **Neon Outline** — body ~invisible, only fresnel rim glows in tint. [preset]
- **Silhouette** — solid single-color cutout, opaque. [preset]
- **Toon/Ink** — posterize texture into N bands + dark fresnel ink lines. [small math]
- **Chrome** — matcap-ish reflection from eye-space normal. [small math]
- **Dissolve** — animated noise-threshold alpha + glowing edge (uses ghostTime+hash). [math]
- **Negative** — invert tex.rgb. [trivial]
- **Gold Statue** — texture→luminance→gold gradient, opaque, soft rim. [small math]
- **Night-vision** — green monochrome + grain + scanlines. [small math]
- **Blueprint** — dark body + cyan edges + faint screen-space grid. [small math]
- **Ectoplasm** — flowing animated fbm noise + wispy edge alpha. [new fn]
- **Frost/Ice** — pale blue, refractive-ish rim, sparkle noise. [small math]
- **Prism** — rainbow fresnel (hue by rim angle). [small math]

## Constraints
- Cheap: reuse harvested batches, no second avatar, overlay gate unchanged.
- Every look respects the per-instance sliders already plumbed.
- Director Console superset: each look in BOTH `panel_ghost_studio.xml` and
  `floater_director.xml` Look dropdowns (+ update the tooltip enumerating styles).
- Wireframe stays as-is.

## Acceptance
Holo/X-ray show the recognizable textured face + FX; Ghost is a clean translucent
silhouette (no melt/voids); each new look renders distinctly on the test avatar; per-instance
sliders work on all; Director mirrors the dropdown; Clone unregressed; builds clean.
