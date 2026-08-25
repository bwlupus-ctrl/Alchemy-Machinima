# Alchemy Machinima Session Handoff — Claude Test/Build

Date: 2026-08-25
Repository: `I:\alchemy-machinima`
Branch: `feature/vram-management`

## Safety and rollback

This is a deliberately dirty working tree with unrelated user files. Do not reset,
clean, restore, or broadly reformat it. Build and test the current branch as-is.

Known rollback points:

- `6468f6b6772` — `Checkpoint: cinematic light rig + actor gaze feature stack`
- `f547400e0fb` — `Checkpoint: cinematic controls and native Actor FX groundwork`

The second checkpoint was made before completing full native Actor FX activation so
the renderer can be stepped back safely if a driver-specific regression is found.

## Whole-session feature inventory

This handoff covers the full session, not only Actor FX.

### Joystick configuration

- The Joystick Configuration floater has more usable space and can expand to fit
  the monitor, mappings, defaults, and action buttons without clipping.
- Joystick/flycam support and layout changes are in the existing checkpoint.

### Pose Polish and Actor Gaze

- Pose Polish is opened from inside Actor Gaze, adjacent to `Gaze Cues...`.
- New/default Pose Polish state enables M1, M2, M3, M4, and M6.
- Diagnostics M5 remains off by default.
- Actor Gaze, independent eye targeting, camera-facing behavior, gaze cues,
  asymmetric pitch, movement styles, and related presets remain in the stack.

### Director subjects C and D

- Director Cast supports Subjects A, B, C, and D.
- World context menus, attachment/object menus, and Director cast menus include C/D.
- Subject IDs are canonicalized for animated-object linkset roots.

### Cinematic light rig

- The rig has one master `Power` control; the redundant top-level `Enable` control
  was removed while per-subject You/A/B/C/D checkboxes remain individual soft power.
- `Gizmo` remains available.
- Hotkey actions exist for master rig power on/off/toggle, but no default keys are
  assigned. The machinima keybinding table was updated so users can bind them.
- Rig controls, presets, multi-anchor work, mirror/aim controls, shaft behavior, and
  the related cinematic light stack are included in the checkpoints.

### Projector volumetric shafts

- Lightbox `Proj Shafts` and Director `Shafts` use the same underlying settings.
- Shared fast presets are available.
- Cone controls are exposed in the Lightbox easy controls and in manual light
  settings so the volumetric beam matches the authored projector cone.
- Runtime shader assets are now staged for ordinary developer builds, avoiding a
  linked executable running against stale shader sources.

### Local atmospheric volumes

- Local Froxel atmospheric volumes were investigated but are **not claimed fixed**.
- The in-world volume cage and numeric controls are local render tools; the reported
  no-fog result remains held for later work.
- See `doc/CINEMATIC_SUBJECT_FOG_HOLDOUT_DESIGN.md` for the design/research record.

## Actor FX: final architecture

Director has a final scrollable `Actor FX` tab for You and Director cast members.
Settings persist for away actors and in Director scenes.

Actor FX no longer redraws harvested Ghost Studio overlay geometry. It is a native,
per-draw material transform attached to the viewer's existing beauty, alpha, shadow,
glow, and velocity passes. This removes the overlay clone's duplicate-geometry cost
and gives system bodies, BOM surfaces, rigged mesh, worn attachments, and standalone
Animesh one ownership model.

Render modes:

- `Layer` mixes the selected style with the material result using Actor FX alpha.
- `Replace` uses full style strength while preserving the material's authored
  coverage and its normal pass ordering.

The feature provides all 28 Ghost Studio-derived looks plus actor/custom hue, alpha,
pixel size, shimmer speed/amount, glitch, distortion mode/amount, brightness, and
effect FPS controls.

## Actor ownership and lifecycle

- Residents use the stable avatar UUID.
- You uses Director's self style, with the runtime agent UUID only for stable phase
  and color generation.
- Worn rigged attachments resolve to the wearer unless an explicitly cast animated
  attachment has its own style.
- Standalone Animesh uses the persistent animated linkset-root UUID, not the
  regenerated `LLControlAvatar` UUID.
- Older loaded scenes canonicalize available transient control-avatar IDs to their
  current linkset root, including Subject and gaze references.
- Draw infos cache a canonical primary owner and a wearer fallback.
- The renderer resolves primary/fallback style in one cast-map lookup path and does
  not query the world object list per draw.

Hard user/privacy suppression stays authoritative. Block/mute-list, Never Render,
UI avatars, and a hard-muted wearer cannot be revived by Actor FX.

## Avatar state and performance-jelly behavior

An enabled/effective Actor FX style makes only automatic complexity/too-slow jelly
state behave as normal for that actor:

- skeleton and animation update every frame;
- real simulator animations resume instead of jelly stand/sit substitutes;
- attachment and alternate-bind overrides are restored;
- normal mesh visibility, extents, rigged rendering, and root placement are used;
- impostor and performance-jelly draw shortcuts are bypassed.

Disabling the style, or setting a Layer strength to effectively zero, returns the
actor to the viewer's ordinary performance policy. Explicit hard mute remains
stronger than styling.

This state fix is important for Bento/rigged heads, BOM faces, system avatars, and
Animesh attachment surfaces; a live material shader is not sufficient if its
skeleton remains animation-shelved.

## Material, alpha, PBR, and color contracts

- Authored alpha is sampled at the original UV and remains owned by the original
  opaque/mask/blend shader and draw pool.
- Actor FX UV distortion and RGB split affect RGB/material taps only.
- PBR OPAQUE ignores texture alpha as required; MASK and BLEND retain authored
  cutoff/transparency behavior.
- Actor FX disabled/zero-strength is an exact identity path: no sRGB round trip,
  optional UV work, look math, or extra texture taps.
- Legacy deferred, PBR, and classic/system-avatar forward paths perform the Actor FX
  transform in linear light and return to their existing encoded contract.
- Fullbright geometric normals are reconstructed from derivatives where the glow
  vertex format does not carry a normal.
- Legacy and PBR material families publish styled emissive consistently.
- Zero-authored-glow alpha faces get a lightweight synthetic glow sub-pass only for
  an effective glow-producing style; other styles and disabled Actor FX do not pay
  that duplicate alpha draw.

## Dissolve, shadows, motion blur, and snapshots

- Beauty and shadow passes call the same rest/object-space Dissolve coverage utility.
- Rigid, rigged, alpha-mask, PBR, and classic-avatar velocity shaders use the same
  coverage, so dissolved holes do not write motion vectors and smear background
  during cinematic motion blur.
- High-resolution snapshots use raw framebuffer tile offsets and freeze Actor FX
  time for all tiles, preventing seams and phase changes across a tiled capture.
- Shader sources are staged beside normal test builds by `viewer_manifest.py`.

## Shader compatibility and sampler safety

- Actor FX beauty and dissolve modules use a coupled identity fallback. If either
  optional effect module fails to compile/attach on a driver, both beauty and
  shadow coverage degrade to identity together; core avatar/object shader families
  continue instead of failing viewer shader load or producing unmatched silhouettes.
- Indexed legacy/PBR material sampler families receive deterministic texture units
  during generic uniform mapping; the later explicit setup remains a safety check.
- Ordinary indexed texture shaders and indexed material families are asserted not to
  share incompatible layouts, and the 32-channel canary remains enforced.

## Primary implementation locations

- Actor style model/persistence/owner canonicalization:
  `indra/newview/lldirectorcast.h`, `indra/newview/lldirectorcast.cpp`
- Effective live avatar state:
  `indra/newview/llvoavatar.h`, `indra/newview/llvoavatar.cpp`
- Per-draw uniforms, velocity submission, and owner resolution:
  `indra/newview/lldrawpool.h`, `indra/newview/lldrawpool.cpp`
- Alpha and synthetic glow:
  `indra/newview/lldrawpoolalpha.h`, `indra/newview/lldrawpoolalpha.cpp`
- Shader enrollment and fallback loading:
  `indra/llrender/llshadermgr.cpp`, `indra/newview/llviewershadermgr.h`,
  `indra/newview/llviewershadermgr.cpp`
- Material sampler mapping:
  `indra/llrender/llglslshader.h`, `indra/llrender/llglslshader.cpp`
- Shared effect modules:
  `indra/newview/app_settings/shaders/class1/alchemy/actorFxF.glsl`,
  `actorFxDissolveF.glsl`, and their fallback modules
- Director UI:
  `indra/newview/llfloaterdirector.cpp`,
  `indra/newview/skins/default/xui/en/floater_director.xml`
- Runtime shader staging: `indra/newview/viewer_manifest.py`

## Required build and static checks

Run from `I:\alchemy-machinima`:

```powershell
git diff --check
cmake --build build-Windows-vs2026-os --config RelWithDebInfo --target alchemy-bin -- /m:4
```

Expected executable:

```text
I:\alchemy-machinima\build-Windows-vs2026-os\newview\RelWithDebInfo\AlchemyTest.exe
```

Confirm the staged runtime shader tree contains every modified/new shader and matches
the source hashes before testing.

### Validation completed in this session

- `git diff --check` passes (only Git's existing CRLF conversion notices remain).
- The complete RelWithDebInfo viewer compiled and linked successfully. Visual Studio
  18/MSVC 14.51 reproducibly crashes inside link-time optimization while processing
  unchanged `llimagej2coj.cpp`; the successful validation build therefore used the
  local-only MSBuild override below. This does not change source or Actor FX behavior.

```powershell
cmake --build build-Windows-vs2026-os --config RelWithDebInfo --target alchemy-bin -- /m:4 /p:WholeProgramOptimization=false /p:LinkTimeCodeGeneration=Default
```

- All 76 modified/new shader sources were present in the staged runtime tree with
  identical SHA-256 hashes: 0 missing and 0 mismatched.
- The exact staged `AlchemyTest.exe` launched responsively on an NVIDIA GeForce RTX
  5090 using OpenGL 4.6 / GLSL 4.60 and driver 591.74.
- The GPU loaded deferred shaders. There were no Actor FX compile/link, varying,
  texture-channel, or indexed-sampler failures. Indexed PBR glow used the GPU's full
  32-channel limit as designed.
- The existing optional `cineOutlineF.glsl` compile failure remains; it disables only
  Cine Outline and did not prevent deferred shaders from loading. It is unrelated to
  Actor FX.
- The test viewer was stopped at the login screen after shader validation. The
  in-world visual and frame-time matrix below still needs to be run on the reported
  production avatars before declaring the cinematic result visually approved.

## Cinematic runtime test matrix

Use a fixed camera, resolution, graphics preset, environment, and animation. Capture
screenshots or short lossless clips for comparisons.

1. **Disabled baseline/performance**
   - Actor FX disabled on all actors.
   - Confirm exact normal appearance and record GPU/CPU frame time.
   - Compare a crowded shot to the checkpoint build; disabled Actor FX must not
     show the old Layer overlay cost.

2. **System/BOM avatar**
   - Test classic system body, BOM skin/tattoos/clothing, hair, and eyes.
   - Check deferred and any available forward fallback.
   - Verify face/body hue and brightness match rather than splitting by pass.

3. **Rigged Bento head and layered alpha**
   - Use the reported rigged head/outfit with BOM and multiple alpha faces.
   - Cycle Hologram, Dissolve, Halftone/Comic, Thermal, Night Vision, and Neon.
   - Look for missing head sections, flicker, alpha crawling, doubled surfaces, or
     animation/joint offset.

4. **PBR matrix**
   - PBR OPAQUE, MASK, and BLEND materials.
   - Normal/ORM/emissive textures, fullbright, double-sided faces, and zero-authored
     glow materials.
   - Confirm opaque faces never consume arbitrary texture alpha, mask holes stay
     exact, blend transparency remains sorted, and emissive is stable.

5. **Standalone and worn Animesh**
   - Test a standalone animated linkset and a worn animated attachment.
   - Save/reload a Director scene and force an Animesh/control-avatar rebuild.
   - Confirm the style stays on the correct stable root and all surfaces animate.

6. **Performance-jellied actor**
   - Lower avatar complexity/ART thresholds until the actor jellies without styling.
   - Enable Actor FX and confirm the live skeleton, rigged head, animations, and
     attachment overrides return.
   - Apply Never Render or block/mute and confirm Actor FX does not revive the actor.

7. **Dissolve parity**
   - Inspect beauty and all shadows frame by frame.
   - Enable cinematic motion blur and move/animate the actor rapidly.
   - Confirm dissolved holes do not leave shadow islands or velocity smears.

8. **Glow parity**
   - Compare opaque, alpha, fullbright, legacy material, and PBR faces with authored
     glow zero and nonzero.
   - Confirm edge-dependent styles follow real geometry and bloom without a fake
     camera-facing normal.

9. **High-resolution snapshot**
   - Capture a multi-tile high-resolution image using pixelation, scanlines, shimmer,
     glitch, and Dissolve.
   - Inspect every tile boundary at 100%; there must be no pattern seam or time jump.

10. **Multiple actors and lifecycle**
    - Style You plus A/B/C/D and a standalone Animesh with distinct looks.
    - Overlap silhouettes, move between spatial groups, leave/return, save/reload,
      and rebuild outfits.
    - Confirm no style leaks to another owner and away settings resume correctly.

## Report format

For a visual or performance failure, record:

- executable/configuration and git commit;
- GPU, driver, resolution, and graphics settings;
- avatar surface type (system/BOM/rigged/PBR/Animesh);
- Actor FX mode/look/alpha/distortion/effect FPS;
- whether the face is opaque, mask, blend, fullbright, or emissive;
- baseline and styled CPU/GPU frame time;
- screenshot/clip plus the exact reproduction steps.

Do not report the unresolved local atmospheric-volume no-fog issue as an Actor FX
regression; it remains a separate held task.
