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
- `5c1de7d4503` — `Perfect native Actor FX across avatar and material paths`
- `cb66bd165d6` — `Document stale Release shader assertion`
- `ecc29219038` — `Complete cinematic Actor FX across all shader paths`

The Wire/Dissolve/PBR parity corrections described below are the follow-up to
`ecc29219038`. Use `git log -1 --oneline` after receiving the handoff to record the
final follow-up hash. The earlier points allow progressively larger rollbacks if a
driver-specific regression is found.

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

Actor FX is a native, per-draw material transform attached to the viewer's existing
beauty, alpha, shadow, glow, and velocity passes. This removes the former full Actor
FX overlay clone and gives system bodies, BOM surfaces, rigged mesh, worn
attachments, and standalone Animesh one ownership model. True topology Wireframe is
the deliberate exception: it reuses the actor's exact live VBO and skin palette for
one GL line pass because a fragment shader cannot recover triangle edges.

Render modes:

- `Layer` mixes the selected style with the authored material result using Layer
  alpha. PBR/legacy material response and authored emissive stay intact.
- `Cover` fully owns the visible colour/material response while preserving authored
  coverage, shadows, velocity, depth, and normal pass ordering. Flat/sensor Cover
  looks suppress legacy spec/gloss/environment and unrelated authored emissive;
  Chrome, Gold, and Ice supply their own intentional material response.
- Wire always uses the same exact live topology pass. Layer mixes the authored fill
  toward a neutral dark hidden-line backing by Layer strength; Cover owns that
  backing completely. This prevents separate legacy/PBR material faces from reading
  as solid patches or holes under otherwise continuous topology.
- Rigged/static mesh Wire samples each indexed material slot's authored MASK/BLEND
  alpha. The hidden-line depth prime includes only SOLID/MASK coverage; BLEND lines
  are alpha-preserving, depth-tested, and never write depth. Fully transparent hair-
  card texels are discarded at a 1/255 coverage floor, preventing rectangular card
  edges without turning a faint transparent layer into an opaque self-occluder.
- Standalone/independently styled Animesh now harvests its control avatar's
  `mRootVolp` linkset directly. Worn Animesh continues through wearer attachment
  enumeration. Static legacy no-material auto-mask faces inherit their actual
  alpha-mask pool cutoff instead of being treated as solid.

The feature provides all 28 Ghost Studio-derived looks plus actor/custom hue, Layer
alpha, independent Dissolve progress, pixel size, shimmer speed/amount, glitch,
distortion mode/amount, brightness, and effect FPS controls.

All looks evaluated in shader core (IDs are the persisted scene values):

`0 Ghost`, `1 Clone`, `2 Hologram`, `3 Wireframe`, `4 X-ray`, `5 Thermal`,
`6 Neon Outline`, `7 Silhouette`, `8 Toon/Ink`, `9 Chrome`, `10 Dissolve`,
`11 Negative`, `12 Gold Statue`, `13 Night-vision`, `14 Blueprint`,
`15 Ectoplasm`, `16 Frost/Ice`, `17 Prism`, `18 Thermal Scope`,
`19 Wallhack/ESP`, `20 Night-vision Tube`, `21 Damage Overlay`, `22 Killcam`,
`23 Oil Slick`, `24 Vaporwave`, `25 Halftone/Comic`, `26 Sonar Reveal`, and
`27 Hologram Echo`.

The exhaustive parity audit found inherent time laws in looks
`2/10/13/15/16/17/20/21/22/23/24/26/27`. Looks
`0/1/3/4/5/6/7/8/9/11/12/14/18/19/25` are static in Ghost Studio by design unless
shimmer, glitch, or a clocked distortion is enabled; they are not frozen-shader
regressions. Effect FPS `0` means smooth real-time animation, not paused time.

Independent distortion IDs are `0 None`, `1 Pixelate`, `2 Voxel`,
`3 Lens/Magnify`, `4 Wave/Ripple`, `5 RGB Split`, `6 Block Glitch`,
`7 Vertical Tear`, and `8 VHS`.
Each distortion is evaluated with every look, not only Hologram/Wire/Halftone.

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
- Material-owning Cover looks neutralize inherited PBR tangent-space normal detail
  and AO while retaining the geometric normal. Layer, Clone, and inactive paths
  preserve the authored normal/AO/RM response exactly.
- Actor FX disabled/zero-strength is an exact identity path: no sRGB round trip,
  optional UV work, look math, or extra texture taps.
- Legacy deferred, PBR, and classic/system-avatar forward paths perform the Actor FX
  transform in linear light and return to their existing encoded contract.
- Legacy specular, gloss, and environment response now follows the same Layer/Cover
  ownership contract as PBR roughness/metallic instead of punching through Cover.
- Fullbright geometric normals are reconstructed from derivatives where the glow
  vertex format does not carry a normal.
- Legacy and PBR material families publish styled emissive consistently.
- Zero-authored-glow alpha faces get a lightweight synthetic glow sub-pass only for
  the nine signature bloom looks (`2/6/10/14/15/17/19/26/27`). Other styles and
  disabled Actor FX do not pay that duplicate alpha draw.

## Dissolve, shadows, motion blur, and snapshots

- Dissolve has a dedicated persisted progress control. `0` is exactly whole and `1`
  is exactly gone; Layer alpha no longer doubles as the dissolve threshold.
- At intermediate progress, the rest/object-space breakup field travels at the same
  rate as Ghost Studio. Its incandescent orange/tinted edge is derived from that
  exact shared field and contributes emissive/bloom instead of darkening under
  scene lighting.
- Beauty and shadow passes call the same rest/object-space Dissolve coverage utility.
- Rigid, rigged, alpha-mask, PBR, and classic-avatar velocity shaders use the same
  coverage, so dissolved holes do not write motion vectors and smear background
  during cinematic motion blur.
- High-resolution snapshots use raw framebuffer tile offsets and freeze Actor FX
  time for all tiles, preventing seams and phase changes across a tiled capture.
- The live topology shader also receives tile offsets/frozen F64-epoch time. Its
  display-authored colour is converted to linear only for the pre-tonemap HDR world
  pass; Ghost Studio's post-tonemap overlay path remains unchanged.
- Live mesh/Animesh topology is submitted after Ghost alpha and Prism composites but
  before the main screen target resolves. It is gated out of Prism auxiliary,
  reflection, cube, impostor, and HUD renders and cannot write the optional visible-
  diffuse/coverage sidecars.
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
- System/BOM topology and alpha-card line semantics:
  `indra/newview/lldrawpoolavatar.cpp`
- Mesh/Animesh live topology and its shader:
  `indra/newview/llactormover.cpp`, `indra/newview/llactormover.h`,
  `indra/newview/app_settings/shaders/class1/interface/actorghostF.glsl`
- Final main-world submission/sidecar isolation:
  `indra/newview/pipeline.cpp`
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
cmake --build build-Windows-vs2026-os --config Release --target alchemy-bin --parallel 4
```

Expected executable:

```text
I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\AlchemyTest.exe
```

Confirm the staged runtime shader tree contains every modified/new shader and matches
the source hashes before testing.

### Validation completed in this session

- `git diff --check` passes.
- The complete Release viewer compiled, linked, and completed the manifest copy with
  Visual Studio 18/MSVC; no LTO workaround was required for this final build.
- Every shader modified by this final pass matched the staged Release copy by
  SHA-256, including Actor FX core/fallback/Dissolve, actorghost, legacy diffuse/
  bump/material, PBR glow, alpha-mask, and fullbright-shiny paths.
- The exact staged Release `AlchemyTest.exe` launched responsively on OpenGL 4.6 /
  GLSL 4.60. Shader cache was purged automatically, forcing a real compile.
- Basic, interface, and deferred shader groups loaded. There were no Actor FX or
  Actor Ghost compile/link failures and no `setShaders: ASSERT(loaded)` fatal.
- Indexed PBR opaque/glow used the GPU's full 32-channel limit as designed.
- The existing optional `cineOutlineF.glsl` compile failure remains; it disables only
  Cine Outline and did not prevent deferred shaders from loading. It is unrelated to
  Actor FX.
- The test viewer was stopped at the login screen after shader validation. The
  in-world visual and frame-time matrix below still needs to be run on the reported
  production avatars before declaring the cinematic result visually approved.

### Native transparency boundary

This follow-up fixes every shader-side animation mismatch found by the exhaustive
look/distortion audit, but it does **not** claim that a native opaque GBuffer face can
become a true translucent Ghost Studio overlay. Layer alpha is treatment opacity;
authored OPAQUE/MASK/BLEND coverage remains authoritative. Dissolve is the deliberate
coverage-changing exception.

Exact clone-style procedural translucency for Hologram/X-ray/Neon/Ectoplasm/Prism/
Wallhack/NV Tube/Sonar/Hologram Echo would require a shared live-VBO transparent
activation path. A partial overlay was not enabled here because it cannot cover the
classic system/BOM body with the existing object-skin shader, would leave the native
actor visible underneath, and would reintroduce extra geometry cost and blend-layer
ordering problems. Do not describe that unsolved architectural step as a shader bug
or as completed cinematic transparency.

### Release artifact warning and verification

- Do not mix an executable from one configuration/commit with another build's
  `app_settings/shaders` directory. Treat the executable and staged runtime tree as
  one inseparable artifact.
- Use the freshly staged Release directory from the final all-look commit. The older
  2026-08-24 Release artifact predates this pass and must not be used for acceptance.

## Cinematic runtime test matrix

Use a fixed camera, resolution, graphics preset, environment, and animation. Capture
screenshots or short lossless clips for comparisons.

0. **Complete 28 x 9 contact sheet**
   - Capture every look ID `0..27` in both Layer and Cover on the same reference
     actor, then combine each look with distortion IDs `0..8` at nonzero strength.
   - Use Effect FPS `0` and `12`, a dark set and a bright HDR set, and alpha values
     `.25/.60/1.0` for Layer. Cover must remain independent of stored Layer alpha.
   - Reject any look that is static when its Ghost Studio counterpart animates,
     restarts per material island, changes at snapshot tile seams, or exposes
     unrelated authored PBR/spec/emissive through Cover.

1. **Disabled baseline/performance**
   - Actor FX disabled on all actors.
   - Confirm exact normal appearance and record GPU/CPU frame time.
   - Compare a crowded shot to the checkpoint build; disabled Actor FX must not
     show the old Layer overlay cost.
   - Record Layer and Cover separately for the same non-Wire look. They share one
     native geometry pass and should be close; only the nine signature bloom looks
     may add the documented alpha glow sub-pass. Wire deliberately adds one line
     pass and should be measured separately.

2. **System/BOM avatar**
   - Test classic system body, BOM skin/tattoos/clothing, hair, and eyes.
   - Check deferred and any available forward fallback.
   - Verify face/body hue and brightness match rather than splitting by pass.

3. **Rigged Bento head and layered alpha**
   - Use the reported rigged head/outfit with BOM and multiple alpha faces.
   - Cycle Hologram, Dissolve, Halftone/Comic, Thermal, Night Vision, and Neon.
   - Look for missing head sections, flicker, alpha crawling, doubled surfaces, or
     animation/joint offset.
   - Include BOM-on-mesh heads, eyelashes/hair cards, multiple overlapping blend
     faces, legacy materials, PBR OPAQUE/MASK/BLEND, and standalone mesh surfaces.

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

10. **Wire world-order and render-state isolation**
   - Place half the actor behind an opaque prop, another avatar, a Ghost alpha clone,
     and a Prism display. No topology may show through foreground geometry and the
     line draw must not hide later transparent/display content.
   - Enable global viewer wireframe and toggle Actor Wire repeatedly; subsequent
     pools must remain in GL line mode.
   - Repeat with the visible-diffuse sidecar, reflections, cube snapshots, avatar
     impostors, HUD rendering, and Prism auxiliary capture. The main camera should
     include Actor Wire; auxiliary targets and sidecars must remain uncontaminated.

11. **Multiple actors and lifecycle**
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
