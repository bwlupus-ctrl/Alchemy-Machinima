# Realtime 360° Equirectangular Video Mode — Design (DRAFT)

Branch: TBD (cine work). Goal: a **realtime, per-frame** equirectangular render path —
the world reprojected into a live 2:1 equirect panorama **every frame**, presented
to the screen (and therefore to any screen recorder / the machinima frame-dump) so
the user records **360 video in realtime**. This is NOT the existing
`LLFloater360Capture` still path (6 blocking `simpleSnapshot` shots → WebGL
`CubemapToEquirectangular` → one JPEG). It is a live viewport mode.

The reprojection math is exactly the "clean 2:1" formula that made the seamless
template: per output pixel, `(u,v) → (lon,lat) → dir`, sample the scene **by
direction**. Direction sampling of a cubemap is inherently seamless — there is no
±180 seam, ever, because there is no stitch line; the cube texture is continuous.

## Contracts (hard)
- **OFF = byte-identical.** When 360 Video Mode is disabled the normal perspective
  render path runs untouched — no cube render, no reprojection pass, no extra
  target allocation. Default = OFF. This is a viewport mode you toggle on to record.
- **Runs at the user's actual render resolution, but the equirect is always a true
  2:1 sub-rect — never stretched.** The user records at whatever size they run
  (e.g. 4096×2160). The client computes the **largest centered 2:1 rectangle** that
  fits that framebuffer, renders the equirect into exactly that rect, and fills the
  leftover margin with **black bars (letterbox/pillarbox)**. So 4096×2160 →
  4096×2048 of correct equirect + 56 px black top and bottom; the 360° content is
  never anamorphically squished — off-ratio just costs black margin, not geometry.
  This auto-fit is the DEFAULT and only mode (see "Auto 2:1 fit") — it happens
  internally, without ever touching the window. The floater still *informs* the user
  what a native 2:1 window size would be (so they could choose one themselves next
  session to spend zero pixels on bars), but the client never resizes for them.
- **Seamless by construction.** Reprojection samples a cubemap by direction with
  seamless cube sampling enabled (`GL_TEXTURE_CUBE_MAP_SEAMLESS`). No per-face blit,
  no manual edge blend, no ±180 marker. The gradient/horizon/geometry cross ±180
  continuously — same as the clean template.
- **NEVER resizes or moves the window.** This is the core requirement. The client
  adapts the video output **internally** to this mode — it composites the 2:1
  equirect into the existing framebuffer (auto 2:1 fit + black bars) and changes
  nothing about the OS window, its size, its position, or its fullscreen state. The
  user's window stays exactly as they set it (e.g. 4096×2161 to skirt Windows FSO);
  the mode works *within* that, dynamically. No SetWindowPos, no swap-chain resize,
  no borderless toggle — the adaptation is purely in how we render into the frame we
  already have.
- **Realtime target.** One cubemap render + one fullscreen reprojection pass per
  displayed frame. Budget is 6 face renders/frame; must hit a usable capture
  framerate (see Perf), NOT seconds-per-frame like the still path.

## Pipeline (per frame, when mode ON)
1. **Cube render.** Render the scene into a 6-face GPU cubemap centered at the
   camera eye. Six 90° FOV, aspect-1.0 views along ±X/±Y/±Z — the same six look
   dirs the still path already uses
   (`llfloater360capture.cpp:475`, `look_dirs`/`look_upvecs`), but into cube-face
   render targets, not `LLImageRaw` reads, and with **no CPU readback / no JPEG /
   no freezeWorld**. The world keeps animating; that is the point.
2. **Equirect reprojection pass.** One fullscreen triangle into the 2:1 equirect
   target. Fragment shader = the clean formula:
   ```glsl
   // uv in [0,1]; equirect target is 2:1 (W = 2*H)
   float lon = uv.x * 2.0*M_PI - M_PI;   // -pi..+pi, front at center
   float lat = (0.5 - uv.y) * M_PI;       // +pi/2 top .. -pi/2 bottom
   vec3 dir = vec3(cos(lat)*sin(lon), sin(lat), -cos(lat)*cos(lon));
   vec3 col = texture(uSceneCube, dir).rgb; // seamless cube sample
   ```
   (Match the axis/handedness convention to the cube render orientation from
   step 1 so FRONT lands at lon=0; the still path's WebGL shader
   `CubemapToEquirectangular.js:43` is the reference for the mapping, just ported
   to native GLSL with our world axes.)
3. **Present (auto 2:1 fit).** The reprojection pass writes into the largest
   centered 2:1 sub-rect of the live framebuffer (see "Auto 2:1 fit"); the leftover
   margin is cleared to black. That composited frame — equirect + black bars — is
   what's on screen and what a recorder captures. The equirect is always
   geometrically correct (never anamorphic); an off-2:1 window just adds black
   margin, which the user crops or ignores in Vegas.

## Reuse — realtime cubemap already exists
The viewer already renders **realtime environment cubemaps** for reflection/hero
probes (`llreflectionmapmanager.*`, `llheroprobemanager.*`, and the cube handling in
`pipeline.cpp`). 360 Video Mode is the same shape of work — render the scene into a
cubemap each frame — pointed at the player camera at capture resolution and reused
for display instead of reflections. Preferred path: **build on that cube-render
infrastructure** rather than standing up a new 6-view renderer, so lighting, alpha,
PBR, sky and water come through the normal deferred pipeline per face.

Open question for the probe path: probe cube renders may run reduced feature sets
(shadows/particles/atmospherics) for reflection budget; Video Mode needs the FULL
beauty pipeline per face. Decide whether to (a) drive a full-quality cube render
through the existing probe render entrypoint with cine-quality flags, or (b) add a
dedicated `renderCubeFace()` that calls the standard deferred display path six times
into cube targets. (b) is cleaner for parity with the flat render; (a) reuses more.

## Auto 2:1 fit (the letterbox rule)
The equirect is dynamically fit to a true 2:1 inside whatever resolution the user
records at, so it is never stretched. **The input `W × H` can be odd or deliberately
off** — e.g. the user runs **4096×2161** (odd height, chosen on purpose to skirt
Windows fullscreen optimization / FSO). The fit must not assume even, clean, or 2:1
input. Given framebuffer `W × H`:
```
// pick the 2:1 orientation, then floor the rect to EVEN dims (encoders require even)
if (W >= 2*H) {                     // window too wide -> pillarbox (black L/R)
    rectH = H & ~1;                 // largest even <= H
    rectW = rectH * 2;              // even by construction
} else {                            // window too tall -> letterbox (black T/B)
    rectW = W & ~1;                 // largest even <= W
    rectH = rectW / 2;              // W even => rectH integer; force even: rectH &= ~1, rectW = rectH*2
}
rectX = (W - rectW) / 2;            // centered; integer div floors
rectY = (H - rectH) / 2;           // odd remainder -> 1 extra px in the BOTTOM/RIGHT bar
```
- Clear the whole framebuffer to black, then render the equirect reprojection into
  the `rectX,rectY,rectW,rectH` viewport. Sphere → that rect only. Never scale the
  sphere to odd dims — always the even 2:1 rect, remainder absorbed by the bars.
- Examples:
  - **4096×2161** (the FSO-skirt case) → equirect **4096×2048**, bars 113 px total:
    **56 px top / 57 px bottom** (odd remainder in the bottom bar). Equirect stays a
    clean even 4096×2048.
  - 4096×2160 → 4096×2048, 56 px top+bottom (even).
  - 1920×1080 → 1920×960, 60 px top+bottom.
  - 4096×2048 → fills exactly, zero bars.
- Asymmetric ≤1 px bar from an odd remainder is invisible and harmless — the
  equirect content is centered to integer pixels and the extra row is pure black.
- Bars are true black (0,0,0) so a 360 player / Vegas crop ignores them cleanly; the
  user sets the Vegas project to the 2:1 sub-rect size (4096×2048) and the bars fall
  outside, or crops them.
- The reported sub-rect (floater status + any metadata) is the EVEN 4096×2048, not
  the raw 4096×2161 window — so 360 metadata tags the true equirect region.

## Resolution guidance (floater)
Because the output follows the user's real render resolution, the floater must make
2:1 easy to hit. It should:
- **Show live status:** current render size + aspect, plus the resulting equirect
  sub-rect and bar size — e.g. "3840×1920 — 2:1 ✓ full frame" green vs
  "4096×2161 — equirect 4096×2048, 56/57 px black bars" amber. Never "stretched"; the
  auto-fit guarantees correct geometry, the message just tells them how much frame
  they're spending on bars.
- **Suggest (inform only) the nearest valid 2:1 sizes** for the user's display, e.g.
  2048×1024, 3840×1920, 4096×2048, 5760×2880, 7680×3840 — as a hint they can apply
  themselves later if they want zero bars. The client does NOT resize to them.
- No resize/relayout controls. Because the user often runs a deliberate odd size
  (4096×2161 for FSO), the mode must respect it and adapt internally — the auto 2:1
  fit already yields a correct equirect from any window without changing it.

## Framerate & cube res
- The reprojection samples a cubemap, so **cube face size**, not the window, sets
  the real sharpness. Face size ≈ output height (e.g. 2048 faces feeding a 4096×2048
  window) is a good match; expose it as a setting independent of window size so the
  user can keep a big cube even in a smaller window.
- Cost ≈ 6× a normal frame (six face renders) + one cheap reprojection pass. At
  2048 faces expect a fraction of realtime on heavy scenes — acceptable for
  machinima because capture can run below 30fps and be conformed in Vegas, and
  Freeze-World / step capture is available for hero shots. Expose a face-res knob so
  the user trades sharpness for live framerate.
- Poles: equirect over-samples the zenith/nadir (many pixels, few texels). That is
  inherent to equirect, not a bug; keep critical action near the horizon band.

## Mono now, stereo later
- **Phase 1 = MONO.** Single cubemap at the eye → one 2:1 equirect. Ships the
  realtime panorama.
- **Phase 2 = STEREO (over/under).** Omnidirectional stereo needs per-eye
  horizontal disparity that is maximal at the horizon and **zero at the poles**,
  with eyes offset horizontally only (never vertical). Cheap approximation: two eye
  cubemaps at ±IPD/2 and two reprojections stacked top(L)/bottom(R) into a 1:1
  square (4096×4096). Correct ODS is per-column eye offset; defer. Phase 1 does not
  block on this.

## Output / Vegas handoff
- The on-screen frame IS a true 2:1 equirect, so any recorder (OBS, the built-in
  frame-dump, external capture) yields equirect frames directly. No stitching step.
- For a finished file, tag it spherical: inject 360 metadata (Google Spatial Media
  Metadata Injector, or ffmpeg `-metadata:s:v spherical=1` / v2 spherical tags) so
  Vegas / YouTube treat it as monoscopic 360. Vegas project frame size must be 2:1
  (e.g. 4096×2048); the globe/look-around preview is a reprojection aid, not the
  render.

## Why there is no seam (the whole point)
The still path and this path both reproject a **cubemap sampled by direction**.
A cubemap has no ±180 edge — sampling `dir` walks continuously around the sphere, so
the equator, horizon, and any geometry cross the back of the panorama with no line.
The only way to reintroduce a seam is to stitch flat rectangles edge-to-edge without
blending; we never do that here — we sample one continuous cube texture. That is
identical to why the clean 2:1 template wrapped seamlessly.

## Build gating / toggle
- `RenderEquirect360Mode` (BOOL, default 0) — master on/off; OFF short-circuits to
  the normal render path before any cube/target work.
- `Equirect360FaceSize` (U32) — cube face resolution → output res.
- `Equirect360Stereo` (BOOL, default 0) — Phase 2 over/under.
- Hotkey / cine-menu toggle so it can be armed for a take without a floater.

## Phasing
1. Mono realtime equirect: cube target alloc + full-beauty cube render + native
   reprojection pass + present at 2:1, gated OFF by default. **This is the deliverable.**
2. Face-res setting + frame-dump writes equirect target at full res.
3. Stereo over/under (approx ODS), then per-column ODS refinement.
