# SL-as-Truth for ReShade: the DEPTH semantic (design brief v3)

Status: **design only, v3** (2026-10-02, repo HEAD `4b6d13f92cc`). Nothing below is built.
- **v1** drew a Codex NO-GO (`C:\Users\xianw\cxs\depthbrief_review.txt`). §0 answers it.
- **v2** reversed v1's core choice: Generic Depth stays the DEPTH provider, and the bridge only
  pins its selection.
- **v2 review** (`C:\Users\xianw\cxs\depthbrief_review2.txt`) found the route viable, but
  NO-GO on Phase A details. §0b answers it.

This brief supersedes §2 of `doc/SL_TRUTH_RESHADE_BRIEF.md` (2026-07-14).

Evidence labels:
- **[P]** the repo code at file:line proves it.
- **[E]** ReShade **v6.8.0** source, `crosire/reshade` tag `v6.8.0`,
  `examples/09-depth/generic_depth_addon.cpp`. This is the installed version (ReShade.log:1).
  The file is not in this repo, so it is cited by function name; line numbers are
  approximate.
- **[I]** inference. Every [I] item is repeated in §9.

---

## 0. v2 changes / review response

| Review item | v2 answer |
|---|---|
| **Route decision** (asked explicitly) | **Keep Generic Depth (GD) as the provider. The bridge pins GD's override to the right buffer every frame.** GD keeps the SRV, the backup copy, the `DEPTH` binding, `bufready_depth` and every lifecycle transition. P0-3, P0-4 and most of P0-5 only arise from *replacing* GD, so they disappear. §3 compares all routes. |
| P0-1: the blit samples depth **bilinear**, not point | **Accepted; v1 §2.2 was wrong.** `bindTexture(..., depth=true)` defaults to `TFO_BILINEAR` (llglslshader.h:314) and applies it after binding (llglslshader.cpp:1170-1178); the call site is pipeline.cpp:22358. Phase A uses the window depth itself, so there is no second sampler. Phase B's encoder issues the *same* `bindTexture` call from the *same* shader source (§4.2). Tests now include non-integer render scales and odd window sizes (§8). |
| P0-2: a shared warp is not colour parity | **Accepted as stated.** Phase A *is* the window depth the user validated in-world, so it has exactly today's parity, no more and no less. Phase B is a second draw of the blit source, with one deliberate change: **far-neutral (1.0) wherever fisheye colour is black**. The remaining sub-pixel gap is documented in §6 as an inherent limit; it exists today. |
| P0-3: the DEPTH handoff is broken | **Closed by design.** GD stays the binder. "Release" means writing GD's override (to the window handle, or to 0 for auto). GD's own change path then rebinds DEPTH and resets `bufready_depth` [E: `update_effect_runtime` runs when `selected_shader_resource` changes]. Nothing is ever null-bound. Protocol in §4.3. |
| P0-4: `bufready_depth` lost | **Closed.** GD remains enabled, and GD sets `source="bufready_depth"` in `update_effect_runtime` [E]. MShadersCommon.fxh:47-48 keeps working. |
| P0-5: config must be established, not just diagnosed | **Phase A** binds the same raw window depth as today, so it does not change config requirements: today's ini (ReShade.ini:16) remains the contract. **Phase B** introduces a required-config contract (§5). The bridge validates *every* coordinate and decode macro. On any mismatch it enters an explicit **UNSUPPORTED_CONFIG** state: it does **not** pin the Phase-B buffer, falls back to the window pin, and logs it. |
| P1: copy traffic and the never-polled sentinel | Cost is now a measured budget (§4.4), not an estimate. The gate uses a `NEVER` sentinel (§4.4). |
| Verified OK | `u = z−1` encoding; far = 1024; ABI append with minor/size checked **before** touching extension bytes; R32F/D32F immutable copy. These carry over unchanged in §4–5. |
| Inherent limits | Documented in §6. Not re-litigated. |

## 0b. v3 changes / review-2 response

| Review-2 item | v3 answer |
|---|---|
| 1. Wrong GUID | **Fixed.** The runtime selection struct `generic_depth_data` is `7c6363c7-f94e-437a-9160-141782c44a98`, created on the `effect_runtime`. `e006e162-…` is `generic_depth_device_data`, created on the device [E]. The `override_depth_stencil` offset of 8 is unchanged. See §3. |
| 2. Ordering | **Fixed.** The bridge pins in `init_effect_runtime`, which runs after GD's own init because GD registered first, and on every re-init. It re-pins per frame in the **`present`** event, which fires before the runtime renders effects. `reshade_present` and `begin_effects` are dropped. See §4.1. |
| 3. Compatibility gate | **Fixed.** Exact-build allowlist, all checked read-only before any dereference: the SHA-256 of `Release\opengl32.dll` plus FileVersion 6.8.0.2155 plus `device_api::opengl`. An unknown build gets stock GD behaviour and one log line. See §4.1. |
| 4. Never-blank / readiness | **Fixed.** Phase A requires `[DEPTH] DepthCopyBeforeClears` to be absent or 0; otherwise it does not pin. A missing target or a failed view falls back to stock GD. "Never blank" is withdrawn. Readiness is **only** GD's `bufready_depth`; `sl_depth_pin` reports pin status, not readiness. |
| 5. Handle literal | **Fixed.** The handle is `(0x8218ull<<40)\|0x821A` = **`0x008218000000821A`**. `0x821A` is **GL_DEPTH_STENCIL_ATTACHMENT**; v2 mislabelled it GL_DEPTH_ATTACHMENT, which is `0x8D00`. The bridge re-acquires GD's data and re-pins after every runtime init and resize. |
| 6. Lifetime | **Fixed.** The pointer is re-acquired inside each callback and never cached. On `destroy_effect_runtime` we only drop state. Restoration happens only from render-thread callbacks; DLL detach does nothing. See §4.3. |
| 7. Config closure | **Scoped.** Phase A does not change depth decoding (same window depth as today), so it has **no** config validator and today's ini keeps working. Full effective-config closure, including per-effect overrides, is a Phase B requirement (§5.2). An absent `MULTIPLIER` is accepted, because both headers default it to 1. |
| P0-2 nuance | Accepted: Phase B's different warp/resampling stages are an **implementation limitation**, not inherent. §6 is reworded. |

---

## 1. The problem

GD picks the depth-stencil with the best draw stats: vertices, unless
`drawcalls_indirect >= drawcalls/3` [E: `draw_stats::operator>`]. On SL that is always the
G-buffer depth `0x000de1…49c` (GL_TEXTURE_2D, 482 draws). The correct buffer is the window
depth. The handle is built as `(type<<40)|name` (reshade_api_resource.hpp:385), with type
GL_FRAMEBUFFER_DEFAULT `0x8218` and name GL_DEPTH_STENCIL_ATTACHMENT `0x821A`. That gives
**`0x008218000000821A`**. The user's transcription `0x0082180000821a` dropped zeros. Note that
the attachment is depth **stencil**, which suggests the window is D24S8 rather than D24X8 [I]
(§9).

GD stores a manual pick in `generic_depth_data::override_depth_stencil`. It **never writes it
to config** [E]. Only filter and heuristic keys persist (`DepthCopyBeforeClears`,
`DepthCopyAtClearIndex`, `DrawStatsHeuristic`, `UseAspectRatioHeuristics`, `FilterFormat`,
`FilterResolutionWidth/Height`, `DisableINTZ`); compare ReShade.ini:4-7. That is why picks are
lost on every launch.

The values are also metrically wrong for SL (§5.1): roughly 9× at 10 m.

---

## 2. What writes the window depth, and why it lines up (corrected from v1)

### 2.1 Writer [P]
- The final present blit, `gBlitWithEffectsProgram`, draws a 3-vertex triangle into FBO 0 with
  `LLGLDepthTest(GL_TRUE, GL_TRUE, GL_ALWAYS)` (pipeline.cpp:22349-22475, draw at 22469-22471).
- The fragment shader writes `gl_FragDepth = texture(depthMap, duv).r`
  (blitWithEffectsF.glsl:122).
- `depthMap` is `mRT->deferredScreen`'s depth, bound **bilinear** (pipeline.cpp:22358;
  llglslshader.h:314; llglslshader.cpp:1178).
- The bound framebuffer is FBO 0, because `flush()` with no previous target binds 0 and the
  `gGLViewport` (llrendertarget.cpp:635-651). The viewport is the world-view rect
  (pipeline.cpp:22241-22245), which equals the whole window (llviewerwindow.cpp:4558, 4560-4575).
- FBO-0 depth is cleared earlier in the frame (llviewerdisplay.cpp:876).
- The old explicit depth blit is commented out (pipeline.cpp:22494-22499).

### 2.2 Content [P]
- **Same texture.** One D24 depth texture (llrendertarget.cpp:108) is shared by
  `deferredScreen`, `screen` and `postPingMap` (pipeline.cpp:1342, 1413). It holds G-buffer
  depth plus forward-pass depth (pipeline.cpp:7626).
- **Same projection.** `perspective(fov, aspect, getNear(), MAX_FAR_CLIP*2 = 1024 m)`
  (llviewerwindow.cpp:6391; llcamera.h:44; llviewercamera.cpp:376-378).
  Near is 0.1 m, or `BDMergeNearClipDistance` (llviewerdisplay.cpp:685-687).
- **Resampled.** The G-buffer is render-scaled (pipeline.cpp:1276-1289, 1299). The blit
  **bilinearly** resamples it to window size; the texture's allocation-time POINT setting
  (llrendertarget.cpp:405) is overridden at bind. So at non-integer scales, edge texels are
  *interpolated* depths.
- **Warped.** Fisheye then diopter/kaleidoscope (blitWithEffectsF.glsl:97-121), with
  parameters shared with the colour pass (pipeline.cpp:22425-22466). Fisheye depth **clamps**
  where fisheye colour is black (blitWithEffectsF.glsl:115 vs cineFisheyeF.glsl:48-58).
- **Timing.** `renderFinalize` (llviewerdisplay.cpp:1747), then HUD, ghosts and UI
  (1758-1817), then swap (1157→1839). ReShade runs inside `wglSwapBuffers`
  (reshade_events.hpp:1656, 1704-1707).

### 2.3 Why it lines up and the G-buffer texture does not
- [P] Window depth is warped like the colour and sized like the back buffer. The G-buffer
  texture is neither.
- [I, strong] Orientation. The live `RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN=0` (ReShade.ini:16)
  suits FBO-0 depth, while copied GL textures need a V-flip (SL_Bridge.fxh:23-32;
  ALBEDO_TEST_RESULTS_ROUND1.md:14).

### 2.4 Writers between the blit and present (unchanged from v1, all [P] except noted)
- Probes run **after** present (llappviewer.cpp:1563, 1569).
- Hero probes, Prism, impostors and the Actor Mask all run **before** the blit
  (llviewerdisplay.cpp:747-752, 919, 856-864, 1122).
- Deferred snapshots never reach finalize (llviewerwindow.cpp:5794-5802;
  llviewerdisplay.cpp:1154-1158).
- After the blit: composition guide, focus point and physics debug
  (pipeline.cpp:22486-22492), HUD attachments (llviewerdisplay.cpp:1766→1607), studio ghosts
  (1790; depth writes possible, llactormover.cpp:15148-15150), and the 3D UI (1799). These may
  write FBO-0 depth [I].
- **GD copies FBO-0 depth at `begin_effects`, so Phase A inherits any such HUD/ghost depth,
  exactly as today. Phase B excludes it.**

---

## 3. Route comparison: replace GD vs keep GD

| Route | Mechanism | Delivers correct buffer? | Auto + stable? | Cost / risk |
|---|---|---|---|---|
| **R0 (v1)**: replace GD | Bridge owns `DEPTH`; user disables GD | yes | yes | Must re-own `bufready_depth`, null-bind handoff, config; 3 P0s. **Rejected.** |
| **R1**: config only, via `FilterFormat` | `[DEPTH] FilterFormat=3` (D24X8) or `4` (D24S8) [E: `check_depth_format`] | **only if** the window and G-buffer formats differ. The user saw D24X8 on the window; the G-buffer's GL_DEPTH_COMPONENT24 is very likely D24X8 too [I], so it **probably cannot separate them**. | no: the auto loop **skips** any buffer with `vertices <= 3 && drawcalls_indirect == 0` [E]. With the UI hidden for filming, the window depth may fall to the blit's 3 vertices and vanish [I]. | free; fragile |
| **R2**: aspect/resolution filter | `UseAspectRatioHeuristics=3/4` + `FilterResolution*` [E: `check_aspect_ratio`] | no: at render scale 1 both buffers are exactly 4096×2161 | no | free; ineffective |
| **R3**: make the G-buffer fail the filter | Change the G-buffer depth format (D32F, llrendertarget.cpp:104-106) or its size | yes, with `FilterFormat` | partly: the ≤3-vertex skip still applies | **high**: core depth-format change; depth blits between FBOs need identical formats. Rejected. |
| **R4 (CHOSEN)**: pin GD's override | Bridge writes `generic_depth_data::override_depth_stencil` through ReShade's public private-data API, at runtime init and every frame | **yes**: GD applies the override *after* its heuristics, replacing their result. Tracked resources bypass the low-vertex, age and format filters [E, confirmed by review 2] | **yes**: re-asserted every frame; no persistence needed | add-on-only change. Couples to GD's private layout, so gated on the exact build hash (§4.1). |

R4 works through a public API on purpose. `effect_runtime` is an `api_object` whose
`get_private_data(guid, &u64)` (reshade_api_device.hpp:277-280, 292-296) returns GD's
per-runtime struct. GD creates it with `runtime->create_private_data<generic_depth_data>()`
in `on_init_effect_runtime` and destroys it in `on_destroy_effect_runtime` [E]. The runtime
UUID is **`7c6363c7-f94e-437a-9160-141782c44a98`**. Not `e006e162-…`: that one is
`generic_depth_device_data`, on the device. The v6.8.0 layout is
`{ resource selected_depth_stencil; resource override_depth_stencil; resource_view selected_shader_resource; bool using_backup_texture; }`
[E], so `override_depth_stencil` sits at byte offset 8.

**Recommendation:** R4 Phase A now, add-on only. Phase B later, for metres.

---

## 4. Design

### 4.1 Phase A: pin GD to the window depth (add-on only, zero viewer change)
- **Handle.** `kWindowDepth = (0x8218ull << 40) | 0x821A` = `0x008218000000821A`
  (GL_FRAMEBUFFER_DEFAULT, GL_DEPTH_STENCIL_ATTACHMENT; reshade_api_resource.hpp:385). The
  pseudo-handle is constant across resizes and render-scale changes, but GD's resource state
  behind it is destroyed and recreated with the GL context, so we always re-pin.
- **Compatibility gate.** It is evaluated **once at add-on load**, read-only, before any
  private-data dereference. Every condition must hold; otherwise the bridge never touches GD
  and logs one INFO line ("unknown ReShade build; stock Generic Depth behaviour").
  - The ReShade module is the one exporting `ReShadeRegisterAddon` (reshade.hpp:264-270). Its
    file must be allowlisted by **SHA-256** and **FileVersion**. The reviewed build is
    `Release\opengl32.dll`:
    - SHA-256 `0CEE63F9C9F13F3AC909C5B4903F4DBB4B719A7AB3B4F13B0DEAF83C814B94F7`;
    - FileVersion `6.8.0.2155`;
    - 5592064 bytes;
    - matches ReShade.log:1.
    A new ReShade drop needs a fresh layout review and a new allowlist entry; there is no
    version-range match.
  - `runtime->get_device()->get_api() == device_api::opengl`.
  - The GD runtime data exists: `get_private_data(7c6363c7-…)` is non-null. Built-in GD
    enabled is implied by the allowlisted binary plus this check.
  - `[DEPTH] DepthCopyBeforeClears` is absent or 0, read via `get_config_value`
    (reshade.hpp:170-188). With it on, GD can serve a pre-clear copy rather than the final
    window content [E], which breaks Phase A's "final window depth" contract. In that case:
    don't pin, log it.
  - `[SL_BRIDGE] PinDepth` is not 0 (default 1). This is the escape hatch.
- **When to write** (GD registered first and callbacks run in registration order [E per
  review], so ours always run after GD's in the same event):
  - In **`init_effect_runtime`**, after GD has created its runtime data. This runs on every
    runtime init and re-init, including context recreate and resize-driven resets, so frame 1
    is pinned.
  - In **`present`** (reshade_events.hpp:1650-1666), every frame. That event fires before the
    runtime renders effects, so it is in place for GD's `begin_effects` selection in the
    **same** frame.
- **Write path.**
  - Re-acquire the pointer with `get_private_data` **inside each callback**. Never cache it.
  - Store only when the value differs: one aligned 8-byte store. Both callbacks run
    synchronously on the GL presentation path, so they do not race GD's reads.
- **Missing target.** If the window handle is not in GD's tracked set this frame, GD uses its
  heuristic, and that can select nothing [E]. **No "never blank" claim.** The bridge does
  nothing extra: this is stock behaviour.
- **Readiness.** Taken **only** from GD's `bufready_depth`, which GD sets from its own SRV
  state (MShadersCommon.fxh:47-48). This covers GD's view/alloc failures. The bridge never
  asserts readiness.
- **Diagnostics, read-only.**
  - In `reshade_finish_effects`, re-acquire GD's data and compare `selected_depth_stencil`
    (offset 0) with the pin.
  - After 60 consecutive mismatched frames, log once
    `[SLBridge] DEPTH pin not honoured (GD selected 0x…)`.
  - `sl_depth_pin` (0 off / 1 pinned and honoured / 2 not honoured / 3 gate rejected) is a
    status value, **not** a readiness signal.
- **Delete** the dead direct-bind code: `DepthSlot`, `update_depth_binding`, `SL_PROVIDE_DEPTH`
  (sl_reshade_bridge.cpp:218-267, 810-867, 1167-1169). The bridge **never** calls
  `update_texture_bindings("DEPTH", …)`.
- **Config:** **out of scope for Phase A.** It decodes the same window depth the user already
  validated by hand with today's ini (ReShade.ini:16). Phase A adds **no** config validator,
  so a working ini is never flagged.

### 4.2 Phase B: metric (pre-compensated) depth, still GD-provided
- **Viewer: encoder pass, immediately after the present blit** (after pipeline.cpp:22475,
  before 22477). It is a second draw of the **same shader source**: `blitWithEffectsF.glsl`
  compiled with permutation `RESHADE_DEPTH_ENCODE`. Under that permutation the shader:
  - **keeps** the `duv` block (blitWithEffectsF.glsl:97-121) as the identical text;
  - skips the colour work;
  - mirrors the input uv (`v' = 1 − v`) *before* `duv` (the pre-flip, verified OK by review);
  - writes far-neutral 1.0 where the fisheye source is out of range, i.e. where colour is
    black (cineFisheyeF.glsl:48-58);
  - writes `gl_FragDepth = d'` per §5.1.
- **Encoder state.**
  - Uniforms come from the same upload block, extracted into
    `bindPresentDepthWarp(shader, diopter_warped, fisheye_warped, sourceBuffer)`. The blit
    must be **bit-identical before and after** that extraction (beauty-path rule,
    CLAUDE.md rule 1).
  - Depth is bound with the **same** `bindTexture(DEFERRED_DEPTH, &mRT->deferredScreen, true)`
    call, so it gets the same default BILINEAR (P0-1).
  - The inverse projection is `inverse(gGLLastProjection)`, uploaded to a dedicated uniform:
    it is snapshotted at pipeline.cpp:25135-25142, and the managed `inv_proj` gets clobbered
    (precedent pipeline.cpp:13791-13807).
- **Target.** `mReShadeDepthRT` is **depth-only `DEPTH_FMT_32F`** at window size.
  - Zero colour attachments are supported (llrendertarget.cpp:549-553); D32F is supported
    (:104-106).
  - Draw with `LLGLDepthTest(GL_TRUE, GL_TRUE, GL_ALWAYS)`, like the blit.
  - GD sees a real depth-stencil with one draw. The auto loop would skip it (≤3 vertices),
    but the override path is unaffected by stats [E].
  - D32F spacing near 1 is about 6e-8, roughly 1.6 cm at 512 m.
- **Why a depth texture rather than R32F colour.** GD already provides the viewer's mutable
  GL depth textures today: it shows the G-buffer `…49c`. Our own `create_resource_view` on
  such a texture failed (sl_reshade_bridge.cpp:235-241); GD's path, possibly via its backup
  texture [E: backup when the resource lacks shader access], demonstrably does not. So we
  reuse the provider that works.
- **Add-on.** Pin `override_depth_stencil = (0x0DE1ull << 40) | gl_name` (GL_TEXTURE_2D
  encoding, as in `0x000de1…49c`), using the ABI-published GL name. This happens only in state
  **METRIC_OK**:
  - ext VALID this frame;
  - §5.2 config valid;
  - `RESHADE_DEPTH_LINEARIZATION_FAR_PLANE` equals the published `F`.
  Otherwise pin `kWindowDepth` (Phase A).
- **Gate (viewer).** All of these must hold:
  - `RenderReShadeDepth` (default TRUE);
  - `mRT == &mMainRT` (precedent pipeline.cpp:22059);
  - consumer poll: `mLastPollFrame != NEVER && frame_counter − mLastPollFrame ≤ 300`.
    `mLastPollFrame` initialises to `NEVER = UINT64_MAX`, so startup cannot open the gate
    (P1). It is updated only when `SLReShade_GetFrame` sees a requested minor ≥ 3
    (llreshadebridge.cpp:113-120).
  - Closed gate means no allocation and no GL calls. The CPU cost is a few compares, not zero.
  - When VALID drops, the bridge re-pins `kWindowDepth` **in the same frame**. GD's
    SRV-change path rebinds and updates `bufready_depth`. No stale buffer is served: the
    override path does not check `last_used_in_frame` [E], which is exactly why the bridge
    must move the pin.

### 4.3 Handoff and restoration protocol (P0-3)
| Event | Bridge action | Who rebinds DEPTH / bufready |
|---|---|---|
| Startup: compat gate, `DepthCopyBeforeClears` or `PinDepth` fails | none, ever | GD (auto), as today |
| `init_effect_runtime` (every init/re-init) | re-acquire, pin `kWindowDepth` (or Phase B target) | GD, on SRV change |
| `present`, normal (Phase A) | re-acquire, pin `kWindowDepth` if it differs | GD |
| Phase B valid | override = encoder texture | GD |
| Phase B invalid, gate closed, or UNSUPPORTED_CONFIG | override = `kWindowDepth` | GD |
| `PinDepth` set to 0 (re-read on `reshade_reloaded_effects`) | in the next `present`: override = 0 once, then stop | GD (auto) |
| Our `destroy_effect_runtime` | drop local state only; **no** dereference (GD destroys its data with the runtime) | GD |
| DLL detach | **nothing.** Off the render thread, no synchronisation | n/a |
| Effect reload | nothing: GD re-binds itself | GD |

### 4.4 Cost budget (P1); measure, do not assume
- **Phase A:** zero viewer GPU work. The add-on adds one 8-byte store per frame. GD's FBO-0
  copy already happens today.
- **Phase B:**
  - Viewer pass: 4096×2161, bilinear D24 read plus D32F write, about 71 MB.
  - GD backup copy, if GD takes the backup path for this texture [I]: about another 71 MB
    read plus write.
  - Budget: **≤ 0.25 ms total on the 5090.** Measure with the Tracy GPU zone
    `"reshade depth encode"` plus ReShade's statistics tab. Over budget is a FAIL.
  - VRAM: +35 MB viewer, +35 MB GD backup [I].

### 4.5 ABI 1.3 (Phase B only)
- Append-only, after the 440-byte base struct, with a `static_assert` of the layout. The tail
  is full except words 17-19 (llreshadebridgeabi.h:307-309).
  - Minor 2 → 3 (:77).
  - Bit `SLRESHADE_SEM_VALID_RESHADE_DEPTH (1u<<10)` (bits 0-9 are taken, :217-226).
  - `SLReShadeFrameV13Ext { ext_magic 'SLX3', ext_size,
    SLReShadeTexture reshade_depth (GL_DEPTH_COMPONENT32F 0x8CAC), depth_flags, float F (1000),
    proj_near, proj_far, src_w, src_h, reserved[16] }`.
  - `frame.struct_size = sizeof(base + ext)`.
- Readers check **minor ≥ 3 and `struct_size`, then magic and size, before touching extension
  bytes**. They copy base and ext from the live container in the same seqlock iteration
  (extends sl_reshade_bridge.cpp:484-494).
- Old add-ons only test `struct_size >= sizeof(base)` (sl_reshade_bridge.cpp:477), so they
  are unaffected.
- The seqlock is same-thread belt-and-braces (llreshadebridgeabi.h:24-29), not general C++
  synchronisation, as the review notes.
- Track the new texture in `gatherFrame` (`SL_RESHADE_TRACKED_TEXTURES` 9 → 10,
  llreshadebridge.cpp:317-323) so resizes bump the generation.

---

## 5. Encoding and config (Phase B)

### 5.1 Encoding (verified OK by review)
- Both ReShade.fxh:110-111 and mmx_depth.fxh:78 compute `L = d/(F − d(F−1))` with near
  hard-coded to 1. iMMERSE metres are `z = L·F + 1` (mmx_camera.fxh:34;
  MartysMods_DEPTHOFFIELD.fx:491).
- Encode `u = clamp(z − 1, 0, 0.998F)` and `d' = u·F/(F + u(F−1))`, with F = 1000 and z the
  planar view depth from `inverse(gGLLastProjection)`.
- Sky (`d ≥ 1`, cleared at lldrawpoolwlsky.cpp:98-99) and fisheye out-of-range both become
  1.0.
- The `0.998F` cap keeps geometry under Launchpad's sky test (MartysMods_LAUNCHPAD.fx:887).

| true z (m) | 0.5 | 1 | 2 | 10 | 100 | 512 |
|---|---|---|---|---|---|---|
| today, iMMERSE z (raw window depth n=0.1 f=1024) | ≈5.0 | ≈9.9 | ≈19.7 | ≈92 | ≈527 | ≈912 |
| Phase B d' | 0 | 0 | 0.50025 | 0.90081 | 0.99098 | 0.99904 |
| Phase B iMMERSE z | 1.0 (floor) | 1.0 | 2.0 | 10.0 | 100.0 | 512.0 |

`L·F` consumers (plain ReShade.fxh) read z − 1. The sub-1 m floor comes from `saturate` at
mmx_depth.fxh:79 and is inherent (§6).

### 5.2 Required config, established at deployment (P0-5)
- **Deploy step** (user-run, documented in §7): ReShade.ini `[GENERAL] PreprocessorDefinitions`
  must contain exactly the values below, with no other `RESHADE_DEPTH_*` overrides:
  - `RESHADE_DEPTH_LINEARIZATION_FAR_PLANE=1000.0`
  - `RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN=0`
  - `RESHADE_DEPTH_INPUT_IS_REVERSED=0`
  - `RESHADE_DEPTH_INPUT_IS_LOGARITHMIC=0`
  - `RESHADE_DEPTH_INPUT_IS_MIRRORED=0`
  - `RESHADE_DEPTH_MULTIPLIER=1`
  - `RESHADE_DEPTH_INPUT_X_SCALE=1`, `RESHADE_DEPTH_INPUT_Y_SCALE=1`
  - `RESHADE_DEPTH_INPUT_X_OFFSET=0`, `RESHADE_DEPTH_INPUT_Y_OFFSET=0`
  - `RESHADE_DEPTH_INPUT_X_PIXEL_OFFSET=0`, `RESHADE_DEPTH_INPUT_Y_PIXEL_OFFSET=0`
  The full macro set is ReShade.fxh:11-51 and mmx_depth.fxh:22-54. REVERSED is
  **mandatory**: both headers default it to **1** (ReShade.fxh:15, mmx_depth.fxh:26).
  MULTIPLIER may be absent, because both headers default it to 1 (ReShade.fxh:24-26,
  mmx_depth.fxh:34-36). Today's ini (ReShade.ini:16) therefore passes as-is.
- **Runtime validation (Phase B only):** at init and on every `reshade_reloaded_effects`, which
  includes preset changes:
  - Read the **effective** value of each macro: the global value
    (`get_preprocessor_definition`, reshade_api.hpp:644), overridden by any per-effect value
    (`get_preprocessor_definition_for_effect`, :787).
  - Check this for **every loaded effect that declares a DEPTH texture**. Enumerate effects via
    `enumerate_texture_variables` (:407) and the variable's effect, matching the
    `DepthBufferTex`/`DepthInputTex` names.
  - If the global value mismatches, or REVERSED is absent: **UNSUPPORTED_CONFIG**. No Phase-B
    pin; pin the window depth instead; one ERROR log naming each macro; `sl_depth_pin = 3`.
  - An effect-local mismatch is logged as "unsupported consumer: <effect>" and counted. It
    blocks the Phase-B pin, because one DEPTH binding serves every effect.
  - It never calls `set_preprocessor_definition` (:656).

### 5.3 FX side
- Today the SL shaders do not sample DEPTH. `SLNearFar`/`SLProj` are declared but unused
  (SL_Bridge.fxh:103-112).
- iMMERSE reads DEPTH via `Depth::get_linear_depth` (mmx_depth.fxh:106-116), using the
  `DepthInputTex : DEPTH` declarations (e.g. MartysMods_MXAO.fx:214).
- **Add** an SL_BridgeDebug.fx "Depth (m)" view. It decodes with the iMMERSE formula, draws
  iso-bands at 1/2/5/10/20/50 m, and shows a banner with `sl_depth_pin` and `bufready_depth`.
  This is the §8 instrument.
- Deploy hygiene: the stale `Shaders/iMMERSE/SL_Bridge.fxh` (its hash differs from the repo)
  and the older deployed SL_BridgeDebug.fx must be replaced or removed.

---

## 6. Inherent limits (documented, not open)
- One depth value cannot represent Diopter's blended double image, blur, transparency, or
  optical ghosts. SL depth has no alpha surfaces (old brief §4).
- Warped DEPTH does not repair the **unwarped** SL_NORMALS and motion, nor iMMERSE's
  pinhole reconstruction (mmx_camera.fxh:43-59), under fisheye or diopter.
- Phase B evaluates the nested warp at window resolution from bilinear depth. Colour warps at
  post-chain resolution and is then resampled. The result is the same mapping with different
  filtering: a sub-pixel gap that today's validated window depth already has.
  - This is an **implementation limitation, not inherent** (review 2). Exact parity would carry
    depth through the same discrete warp passes, and that is deferred.
- Phase A keeps today's metric error (§5.1) and any post-blit HUD/ghost depth (§2.4).
- Sub-1 m reads 1 m in iMMERSE (Phase B). Today a face at 0.5 m reads about 5 m.
- The pre-blit Phase B depth excludes HUD, ghost and UI depth, but their **colour** is still in
  ReShade's input (llviewerdisplay.cpp:1747-1817).
- **Corrections to v1 and the old brief, still valid:**
  - The published `f.far_clip` (llreshadebridge.cpp:175) and `f.proj` (`getProjection()`
    rebuilt from the draw distance, llviewercamera.cpp:169-172) do not describe the depth
    buffer's 1024 m projection.
  - The old brief §2.7 REVERSED default claim is wrong (ReShade.fxh:15 says 1).
  - README.md:27-29 ("use generic_depth, copy-before-clear") must be updated to the pin.

## 7. Process
- **Phase A** is add-on only.
  - Build with `cmake --build reshade-addon/build-cmake --config Release --target
    sl_reshade_bridge`, from PowerShell.
  - API pin 18 (CMakeLists.txt:27-30). It needs only `get_private_data`, `get_config_value`
    and the `present` / `init_effect_runtime` / `reshade_finish_effects` events, all
    API-18-era [I].
  - The SHA-256 allowlist is a constant in the add-on. Updating ReShade means a new layout
    review, a new hash and a rebuilt add-on.
  - Size smoke test: about 150 KB. Deploy with the viewer **closed** to
    `build-Windows-vs2026-os/newview/Release/sl_reshade_bridge.addon`.
  - It does not touch pipeline.cpp, so **it can proceed while Codex's shadow work is in
    flight**.
- **Phase B** touches pipeline.cpp, `blitWithEffectsF.glsl`, llviewershadermgr.cpp,
  llreshadebridge*, the ABI header and the add-on.
  - **Blocked** until the shadow feature in pipeline.cpp/lldrawpool.cpp lands. lldrawpool.cpp
    is off-limits.
  - Flow per CLAUDE.md: Codex implements via `--prompt-file`; Opus adversarial review covers
    blit bit-identity, the off-path, ext reads and the pin state machine; loop to 0 must-fix;
    checkpoint commit; build once.
- Shaders deploy to `Release/reshade-shaders/Shaders`. The exe is
  `build-Windows-vs2026-os/newview/Release/AlchemyTest.exe`.

## 8. Test plan (outcomes stated in advance)
- **A0 instrument.**
  - Expected log: `[SLBridge] GD pin active (ReShade 6.8.0.2155, sha 0CEE63F9…) ->
    0x008218000000821A`, and no "not honoured" line.
  - In GD's overlay list, the window entry shows as selected.
  - Anything else is INCONCLUSIVE.
- **A1 the main goal.** Restart the viewer three times without touching the overlay. Each
  launch must auto-select the window depth.
  - FAIL with the window entry present: the layout or version guard is wrong.
  - FAIL with the window entry absent: GD does not track FBO 0 on that frame.
- **A2 filming.** Hide the UI (Ctrl+Alt+F1) and remove HUDs. The window depth must stay
  selected. This proves the ≤3-vertex skip does not affect the override path [E].
- **A3 robustness.** Resize the window; change `RenderResolutionDivisor` (2) and
  `RenderResolutionMultiplier` (0.75 and 1.33, non-integer); test odd window sizes
  (e.g. 1365×767); toggle HDR; reload effects. DisplayDepth.fx must still line up at the
  centre and all four edges.
- **A4 escape hatch.** With `PinDepth=0`, GD reverts to auto (the G-buffer) and the log says
  so.
- **A5 gates.**
  - With `DepthCopyBeforeClears=1`: no pin, a log line, stock GD.
  - With a byte-modified copy of opengl32.dll (hash mismatch): no pin, "unknown build" log,
    stock GD.
  - Neither case may crash.
- **A6 first frame.** On launch, GD's list shows the window entry selected from the first
  frame on which it is tracked. The pin is set in `init_effect_runtime`.
- **B1** = A1–A3 repeated with Phase B active; `sl_depth_pin = 1` with the metric source.
  - Fisheye black corners read far.
- **B2 metres.** Markers at 2/5/10/20/50 m: the iso-bands and the DoF readout agree within
  ±2%.
- **B3 invariance.** Changing draw distance 64↔512 and the near clip must not move the bands.
- **B4 config.** Remove REVERSED from the ini and reload. Expect UNSUPPORTED_CONFIG, an ERROR
  log, and a fallback to the window pin. Depth is never silently inverted.
- **B5 off-path.** With no add-on, or the 1.2 add-on, Tracy shows no
  `"reshade depth encode"` zone.
- **B6 cost.** Pass plus GD copy ≤ 0.25 ms, or FAIL.
- **B7 gate.** Toggle ReShade effects off for more than 300 frames, then back on. The bridge
  re-pins the window depth while invalid. `bufready_depth` tracks GD's SRV state only; the pin
  never implies readiness.

## 9. Least-sure list (check first)
1. **The GD layout (offset 8) and the runtime GUID `7c6363c7-…`** come from v6.8.0 source [E,
   confirmed by review 2]. Whether the tag source equals the deployed `6.8.0.2155` binary
   is assumed. Any build mismatch is caught by the hash gate, not detected per field.
2. Whether GD keeps FBO-0 in `current_depth_stencil_resources` on frames with ≤3 vertices
   (A2 settles it).
3. Whether GD takes the backup-copy path for the Phase-B D32F texture, which affects cost
   (B6).
4. **Formats.** The window attachment is GL_DEPTH_STENCIL_ATTACHMENT, so it is probably
   D24S8, while the G-buffer GL_DEPTH_COMPONENT24 is probably D24X8. If so, R1
   (`FilterFormat=4`) *could* separate them as a config-only fallback, with the ≤3-vertex
   caveat. **Ask the user to read both formats in GD's list.**
5. Thread safety is narrowed to `present`, `init_effect_runtime` and `reshade_finish_effects`
   all running synchronously on the GL presentation path (review 2). DLL detach is excluded
   by design.
6. Per-effect override enumeration (§5.2) relies on texture-variable names. A DEPTH consumer
   with an unusual variable name escapes it.
7. Pre-flip orientation for a GD-provided GL **texture** in Phase B. Verified OK for the R32F
   copy path, but GD's texture path is a different backend route [I]. If DisplayDepth shows it
   inverted, flip the viewer constant (`RenderReShadeDepthFlipY`); never touch the ini.
