# ReShade Bridge — Root Cause of the Flat-Depth / Grey-Motion Failure

**Status:** Root cause identified from source + logs, fix applied. 2026-07-12.
**Verdict in one line:** the binding always worked — the *textures* were mipmap-incomplete under ReShade's sampler objects, so every sample returned constant black. One `glTexParameteri(GL_TEXTURE_MAX_LEVEL, 0)` per bound texture fixes it.

---

## 1. The symptom recap

- Viewer addon (`llreshadebridge.cpp`) binds the R32F depth copy to ReShade's `DEPTH` semantic and the A5.4 velocity buffer to a custom `SL_MOTION_NDC` semantic.
- `create_resource_view` **succeeds** (Alchemy.log: `[RTGI] bound GL 3110 (41) -> ReShade 'DEPTH'`, `bound GL 3120 (34) -> 'SL_MOTION_NDC'`, rebind `4211` after resize — resize handling works too).
- Yet in-world: depth override reads FLAT, motion debug tint reads GREY (zero motion).
- Meanwhile ReShade's built-in generic_depth addon delivers correct SL depth. Our `create_resource_view` call matches generic_depth's byte-for-byte. Paradox.

## 2. The evidence chain (each link verified in source)

1. **ReShade's GL bind path is a pass-through, not a tracking table.**
   `push_descriptors` (`reshade-SL/source/opengl/opengl_impl_command_list.cpp:1109-1127`) binds a
   `sampler_with_resource_view` descriptor as literally:
   `glBindTextureUnit(unit, view.handle & 0xFFFFFFFF)` + `glBindSampler(unit, sampler)`.
   Our raw GL texture name IS bound to the effect's sampler unit. The old
   "foreign-texture tracking" hypothesis is **wrong at the bind level** — no lookup table is consulted.

2. **`create_resource_view` took the fast path and returned our raw name.**
   (`opengl_impl_device.cpp:1140-1148`) — when the requested format equals the live GL object's
   internal format (R32F==r32_float, RG16F==r16g16_float), the "view" is just the original
   texture name re-wrapped. So the effect sampler was sampling *our actual texture*. Content
   should have flowed. It didn't — because of link 3.

3. **Every ReShade GL sampler object uses a MIPMAPPED min filter.**
   `create_sampler` (`opengl_impl_device.cpp:435-511`): every `api::filter_mode` case maps to a
   `GL_*_MIPMAP_*` min filter — the FX default (`MinFilter=MagFilter=MipFilter=LINEAR`) becomes
   `GL_LINEAR_MIPMAP_LINEAR`. There is **no non-mip case** in the switch.

4. **GL completeness rule (GL 4.6 §8.17 + §8.23.1).**
   When a sampler object is bound to a unit, **its** parameters — not the texture object's —
   determine texture completeness. A mipmapped min filter requires the texture to be
   *mipmap complete*. A texture that is not complete samples as constant **(0, 0, 0, 1)**.

5. **Viewer RT textures are exactly the incomplete case.**
   `LLRenderTarget` allocates attachments with `glTexImage2D(level 0)` only
   (`llrendertarget.cpp:325,396` via `LLImageGL::setManualImage`), never touches
   `GL_TEXTURE_MAX_LEVEL` (default **1000**), and sets non-mip filters only as *texture*
   parameters — which the bound sampler object overrides. Level 0 defined + levels 1‥1000
   undefined + mipmapped sampler filter ⇒ **mipmap-incomplete ⇒ every sample = black**.
   Black depth = "flat". Black velocity = zero motion = grey debug tint. One mechanism,
   both symptoms.

6. **Why generic_depth works: it binds ReShade-OWNED textures, which are immutable.**
   - ReShade creates all its resources with `glTexStorage2D` (`opengl_impl_device.cpp:770`).
     Immutable-format textures **cannot be mipmap-incomplete** — completeness is defined by
     their allocated levels.
   - The user's `ReShade.ini` has `[DEPTH] DepthCopyBeforeClears=1`, so generic_depth runs in
     backup-copy mode (`generic_depth_addon.cpp:1016-1044`): it copies SL's raw depth into a
     **backup texture ReShade created**, then `create_resource_view`s the *backup*. The texture
     the effects sample is immutable ⇒ complete ⇒ works.
   - So generic_depth's "identical" `create_resource_view` call was operating on a texture with
     different *object state*. The difference was never the call — it was mutable-vs-immutable
     storage and `MAX_LEVEL`.

7. **Everything else checked out** (i.e. ruled out): `SL_GBufferProvider.fx` compiled clean
   (ReShade.log line 324), it is enabled in the active preset `PRESETS/DUBS/ugh3.ini` in the
   correct order (Launchpad → provider → RTGI/MXAO/SPECGI), and its
   `Deferred::MotionVectorsTex` declaration (`BUFFER_WIDTH × BUFFER_HEIGHT, RG16F`) matches
   `mmx_deferred.fxh:78` exactly, so cross-effect texture sharing aliases correctly.

## 3. The fix (applied)

`llreshadebridge.cpp bind_gl_texture()`: when a (new) GL name is about to be bound, clamp its
mip range to the one level that exists:

```cpp
GLint prev_tex2d = 0;
glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex2d);
glBindTexture(GL_TEXTURE_2D, gl_name);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_tex2d));
```

- Runs once per texture (guarded by the existing `gl_name != state.gl_name` check), on the
  render thread with the GL context current (ReShade fires `reshade_begin_effects` inside
  SwapBuffers), binding saved/restored.
- Side-effect free for the viewer: these RTs have no mips and are never sampled with a
  mip filter viewer-side. `MAX_LEVEL` survives `LLRenderTarget::resize()` (texture state, not
  image state); a full release/re-allocate produces a new GL name, which re-triggers the clamp.
- Generic: any future buffer (normals, ORM, albedo) routed through `bind_gl_texture` inherits
  the fix.

## 4. What this un-blocks / validation plan (in-world)

1. **Motion (the machinima win):** enable `BDMergeVelocityBuffer`, enable
   `SL_GBufferProvider` + its debug tint. Expected: tint now shows warm/cool motion colors on
   camera orbit instead of uniform grey. This also finally *visually validates A5.4 Phase 1a*
   (never confirmed): if the tint stays grey **while the camera moves**, the velocity pass
   content itself is wrong — that becomes a viewer-side A5.4 bug, not a bridge bug.
   Then check RTGI ghosting on camera orbits (expect visible reduction; skinned avatars still
   zero-motion until A5.4 Phase 1b).
2. **Depth override (optional, low value):** `BDMergeReShadeOverrideDepth=1` + DisplayDepth.
   Expected: real depth gradient. Note the ini has `RESHADE_DEPTH_INPUT_IS_REVERSED=1`; SL is
   non-reversed, but this preprocessor applies identically to generic_depth's copy of the same
   raw values, so whatever calibration made generic_depth look right applies to ours too.
   Recommendation stands: leave the override OFF by default — generic_depth (backup mode)
   already handles depth; our value-add is motion + normals.
3. **Possible simplification (test later):** with the completeness clamp, binding the RAW
   depth texture (`deferredScreen.getDepth()`) directly may now work, making the R32F copy
   pass unnecessary. The copy shader's comment blaming "depth-format sampling quirks" was
   this same completeness bug in disguise. Keep the copy until the direct bind is proven.

## 5. Meta-lesson

"Matches the reference call exactly yet behaves differently" ⇒ the difference lives in
*object state or environment*, not the call. Here: mutable vs immutable storage and one
default-valued texture parameter. The log datapoint that split the problem was
`create_resource_view` SUCCESS — that immediately moved suspicion from the bind to
sampling state/content.
