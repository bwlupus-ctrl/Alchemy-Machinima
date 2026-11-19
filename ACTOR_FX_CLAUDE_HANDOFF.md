# Alchemy Machinima — Complete Session Handoff for Claude Test/Build

Date: 2026-08-25

Repository: `I:\alchemy-machinima`

Branch: `feature/vram-management`

This document covers the entire session: Joystick Configuration layout, Pose
Polish placement/defaults, Director Subjects C/D, Cinematic Light Rig controls and
keybinding support, shared Projector Shafts controls, cone controls, the held local
atmospheric-volume investigation, and the current Actor FX shared-activation work.

## Implementation and machine-validation state

- Shared-activation base commit: `ab48c7cdd5407c842a7ab60b5804984582d7451b`
  (`Complete full shared Actor FX activation`)
- Deferred-native cinematic redesign: the commit containing this handoff; use
  `git log -1 --oneline` after checkout to identify it exactly
- `git diff --check`: **PASS** for the redesign implementation worktree
- Changed XUI XML parse checks from the full session: **PASS**
- Release build command: **PASS, exit 0**
- Release artifact:
  `I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\AlchemyTest.exe`
- Artifact size: `81826816` bytes
- Artifact SHA-256:
  `DDCD660DC6C64B70A991164DEA68D9DF5466E0CE537AD189523CFEB225F58E66`
- Shader staging audit: **all 8 edited shader sources equal their Release runtime
  copies byte-for-byte; missing 0; mismatch 0**
- Exact-artifact GPU smoke: **PASS** on NVIDIA GeForce RTX 5090, OpenGL 4.6,
  NVIDIA driver `591.74`, reported VRAM `32607 MB`
- Smoke log Actor FX critical counter: `ActorFxGpuErrors=0`; deferred shaders loaded
  and both scalar and indexed shared-PBR families compiled/linked
- The first GPU smoke found an invalid two-argument `vec4` constructor in three glow
  paths. It was corrected, restaged, and the complete GPU smoke was rerun cleanly.
- Consolidated static/adversarial recheck: `P0=0`, `P1=0`
- Temporary smoke-test PIDs `72864` and `51204`: stopped after validation; the user's
  pre-existing viewer PID `66568` was not stopped or modified
- Known smoke warning: the unrelated optional Cine Outline shader still reports its
  existing compile warning; it does not invalidate Actor FX/shared-PBR loading
- In-world cinematic visual matrix: **REQUIRED; not yet completed**
- GPU/frame-time comparison: **REQUIRED; not yet completed**

The shared-activation base supplies same-frame actor-wide replay, readiness, the
third alpha stream, component sorting, Cover depth behavior, and full scalar/indexed
PBR material replay. The current redesign changes how those captured materials are
styled: live PBR avatars now use a scene-linear, deferred-native cinematic treatment,
while classic/BOM/system/Animesh world paths receive compatible linear-light timing,
distortion, brightness, bloom, and atmosphere handling. Clone rendering deliberately
retains its historical late unlit/post-tonemap appearance. The Release build,
staging, XML, static-audit, and shader-load smoke gates passed. Those machine gates do
not prove in-world visual perfection, alpha ordering on production avatars, animation
parity, or acceptable cinematic frame time; the runtime matrix remains mandatory.

## Safety and workspace rules

The worktree is deliberately dirty and contains unrelated user files and historical
logs. Preserve them.

- Do not run `git reset --hard`, `git clean`, broad `git checkout --`, or a blanket
  restore/reformat.
- Do not stage with `git add -A` or `git add .`. Add only the reviewed implementation
  and handoff paths explicitly.
- Do not delete the untracked review briefs/logs.
- Do not mix a viewer executable from one commit/configuration with another build's
  `app_settings\shaders` tree. The executable and staged runtime assets are one
  inseparable artifact.
- Build and launch the exact Release artifact named below, not an installed copy
  under `C:\Program Files`.
- If a test process must be stopped, stop only the PID returned by the launch command.
- Preserve a failing log and screenshot before rebuilding; shader-cache and staging
  failures are otherwise difficult to distinguish from rendering defects.

Useful rollback/checkpoint commits:

- `6468f6b6772` — `Checkpoint: cinematic light rig + actor gaze feature stack`
- `f547400e0fb` — `Checkpoint: cinematic controls and native Actor FX groundwork`
  (includes the larger/expandable Joystick Configuration layout)
- `5c1de7d4503` — `Perfect native Actor FX across avatar and material paths`
- `cb66bd165d6` — `Document stale Release shader assertion`
- `ecc29219038` — `Complete cinematic Actor FX across all shader paths`
- `21b8a7701d7` — `Fix Actor FX animation and material parity`
- `76dd5913879` — `Use one Cinematic Light Rig enable master`
- `ab48c7cdd54` — `Complete full shared Actor FX activation`

These are reference points, not permission to reset the user's worktree.

## Whole-session feature inventory

### Joystick Configuration

- The Joystick Configuration panel was widened and given more usable real estate and
  an expandable layout so mappings, monitor, defaults, button assignments, and
  OK/Cancel fit without the clipping shown in the original report.
- The checkpoint containing this layout is `f547400e0fb`.
- Test at both the user's normal window size and a smaller supported size; verify
  that expanding/resizing does not strand controls outside the floater.

### Pose Polish inside Actor Gaze

- `Pose Polish...` is launched from inside Actor Gaze and sits beside `Gaze Cues...`.
- New/default state enables M1, M2, M3, M4, and M6.
- M5, the diagnostic layer overlay, is off by default.
- Existing Actor Gaze behavior remains in scope: actor selection, per-actor versus
  broadcast editing, gaze cues, camera-facing behavior, independent eye/head/torso
  allocation, asymmetric limits, and movement-style presets.
- Existing user settings must not be silently overwritten merely by opening the
  floater; the defaults above describe a fresh/default profile.

### Director Subjects A/B/C/D

- Director Cast supports Subjects A, B, C, and D.
- The avatar context menu, attachment/object context routes, and Director cast UI
  expose C and D as well as A and B.
- Animated-object identities are canonicalized to the persistent linkset root so a
  regenerated control-avatar UUID does not lose the assignment.
- Test context assignment, replacement, removal, save/reload, and leave/return for
  all four subject slots.

### Cinematic Light Rig and keybinding support

- The panel now has one visible master checkbox: `Enable`.
- The redundant visible `Power` checkbox was removed.
- `Gizmo` remains available.
- `You`, `A`, `B`, `C`, and `D` remain individual per-subject soft-power controls;
  they do not replace the one master `Enable` gate.
- The historical action ID `cine_light_rig_power` is intentionally retained for
  keybinding compatibility, but it toggles `CineLightRigEnabled`.
- The action appears in the machinima keybinding table and ships with no default key.
  A user may bind it without a source change.
- `CineLightRigPower` is a deprecated compatibility/persistence value only. Runtime
  master power is `CineLightRigEnabled`; the legacy value is kept forced true so an
  old saved false value cannot become a hidden second gate.
- Verify there is no `Power` checkbox in either the standalone or embedded rig panel,
  that the bound action updates the visible `Enable` checkbox, and that `Gizmo`
  remains independent.

### Shared Projector Shafts and cone controls

- Lightbox `Proj Shafts` and Director `Shafts` expose the same underlying projector-
  volumetric settings rather than separate partial control surfaces.
- Fast configuration presets are:
  - `Fast Preview`
  - `Balanced`
  - `Cinematic`
  - `Hazy Stage`
- Lightbox easy mode includes controls that adjust the cones of all rig beams plus a
  global cone-edge feather control.
- Manual/advanced light controls include separate cone controls for Key, Fill, Rim,
  and BG so an individual light can be authored precisely.
- Test that changes made in either Lightbox or Director are immediately reflected in
  the other, and that every preset updates both views consistently.

### Local atmospheric volumes — investigated, held

- Local atmospheric/Froxel volumes were investigated after the in-world cage was
  visible but no local fog appeared.
- This issue is **not claimed fixed in this session** and is intentionally held for
  later work.
- Do not fold this known no-fog result into Actor FX acceptance or report it as a
  regression caused by shared activation.
- The research/design record is `doc\CINEMATIC_SUBJECT_FOG_HOLDOUT_DESIGN.md`.

## Actor FX objective and current architecture

The goal is cinematic parity with Ghost Studio's stylized clones while styling the
actual live actor. The final direction is **exact same-frame full shared activation**,
not a cloned avatar/entity and not a late screen-space approximation.

The shared path captures the live renderer's already-resolved draw inputs during the
same frame and replays them under Actor FX ownership:

1. The original actor supplies the exact VBO, index range, material assignment,
   transforms, and current skin palette.
2. A frame-local actor proxy records components without copying or reanimating an
   avatar.
3. Readiness is evaluated for the entire visible actor.
4. If every required visible component is replayable, the shared path activates as
   one actor-wide transaction.
5. If any required component, material, palette, texture-slot layout, or optional
   shader family is unavailable, the entire actor fails open to native rendering for
   that frame. A half-native/half-styled actor is not accepted.
6. Solid/masked replacement and transparent components are submitted in the correct
   main-world stages. The alpha work is a third interleaved stream, not a late blob
   draw after all world transparency.

There is no second avatar, no cloned skeleton, no copied animation clock, and no
separate attachment hierarchy. This removes clone pose drift and makes the visual
effect use the actor's exact current deformation.

### Layer and Cover contracts

`Layer` and `Cover` are deliberately different render contracts:

- **Layer** preserves the authored actor and mixes/composites the selected style by
  the Alpha value. Alpha `0` is visually native; `.25` and `.60` are partial
  treatments; `1` is the full treatment while authored coverage remains valid.
- Legacy/BOM Layer normally keeps native beauty/depth and adds the sorted shared
  treatment. This is why Layer can cost more than Cover.
- PBR Layer uses authoritative full-material shared replay: native
  PBR beauty/glow is suppressed only after the complete PBR replay is ready, the
  authored material is reconstructed, and Actor FX strength is Alpha. This avoids
  double PBR shading while preserving the PBR result.
- **Cover** owns visible color/material response. Native actor color is suppressed
  only when the complete shared replacement is ready. Alpha is replacement opacity:
  `0` emits no shared color or depth, `.25/.60` are translucent replacement, and `1`
  is fully covered.
- Cover preserves authored coverage. OPAQUE stays opaque before replacement opacity;
  MASK uses its authored cutoff; BLEND retains texture/material alpha.
- A zero-opacity Cover must not leave an invisible depth/DoF silhouette. A nonzero
  Cover must retain useful depth and DoF behavior.

Never suppress native rendering first and hope replay succeeds later. Suppression and
activation must be controlled by the same actor-wide readiness decision.

### Third alpha stream and component sorting

The shared alpha renderer participates inside the normal interleaved alpha stage and
sorts replay pieces at component granularity. The current component classes are:

- actor solid/mask work;
- one transparent component per rigged `LLDrawInfo`;
- one transparent component per static alpha face;
- one aggregate classic/system/BOM transparent component, because the classic body
  does not expose a finer independently sortable unit through this path.

Transparent components are stably sorted far-to-near with deterministic ties. Cover
solid/mask depth is primed before native/shared alpha color begins. Shared transparent
color depth-tests but does not write depth. The path must never turn hair cards or a
BLEND face into an opaque rectangle merely to stabilize sorting.

Honest sorting limitations remain:

- classic/system/BOM transparency is one aggregate sort item;
- rigged alpha depth uses the available `LLDrawInfo`/spatial-group bounds rather than
  a perfect per-triangle or per-material-island centroid.

These limitations must be evaluated on overlapping hair, eyelashes, layered faces,
and intersecting actors; do not describe them as solved by compilation.

### Ownership, Animesh, lifecycle, and fail-open rules

- Residents use their stable avatar UUID.
- `You` uses the Director self-style, with the agent UUID used for stable phase/color
  generation.
- Worn rigged attachments normally inherit the wearer.
- An explicitly cast animated attachment/linkset with its own style owns that style
  and must not also be replayed through the wearer.
- Unstyled worn Animesh inherits the wearer style.
- Standalone Animesh uses its persistent animated linkset-root UUID, not the
  regenerated `LLControlAvatar` UUID.
- Scene load canonicalizes available transient control-avatar IDs to their current
  persistent root for cast and gaze references.
- Streaming/rebuild transitions are actor-wide: remain completely native until all
  visible descriptors are ready, switch completely to shared, and fail completely
  back to native if readiness is lost.
- Block/mute, Never Render, UI-avatar exclusion, and a hard-muted wearer remain
  authoritative. Actor FX must never revive content hidden by those policies.

Actor FX still bypasses automatic complexity/too-slow jelly shortcuts only for an
enabled/effective style so that the live skeleton, Bento head, real animations,
attachments, alternate binds, and extents are available. Disabling the style or
returning Layer to effective zero returns the actor to normal performance policy.
Hard user/privacy suppression remains stronger than styling.

## Full shared PBR replay contract — implemented, visual validation pending

PBR cannot be accepted as an Actor Ghost approximation. Each PBR component must be
replayed as its full authored material and then receive the Actor FX transformation.

The integration covers both vertex layouts:

- **Scalar PBR** VBOs do not provide `MAP_TEXTURE_INDEX`; their shader permutation
  must not declare/require the `texture_index` attribute.
- **Indexed PBR** VBOs carry material slot indices. Replay must use the indexed
  permutation, validate a contiguous/usable slot set, bind the correct material per
  slot/filter, and never access a scalar VBO with the indexed attribute mask.

It also covers static and rigged forms and all three glTF alpha modes:

- OPAQUE ignores arbitrary base-color texture alpha;
- MASK evaluates sampled base alpha times factor alpha against the material cutoff;
- BLEND evaluates authored sampled/factor alpha and then the shared replacement
  opacity where applicable.

The beauty replay must preserve, as applicable:

- base-color texture/factor and correct sRGB-to-linear handling;
- UV transforms and the correct texture coordinate set;
- tangent-space normal mapping/TBN and double-sided normal orientation;
- metallic, roughness, occlusion/AO, and their factors/channels;
- emissive texture/factor and unlit/fullbright intent;
- static object transforms and exact rigged skin palettes;
- sun/punctual/probe/atmosphere/water inputs supported by the shared forward shader.

Glow is derived exactly once from each PBR base component:

- if authored emissive data is present, use the authored-glow permutation and add
  the style's intentional synthetic contribution without replaying a harvested glow
  batch a second time;
- if no authored emissive vertex stream is present, use the synthetic-glow
  permutation only when the chosen look needs it;
- glow must retain authored OPAQUE/MASK/BLEND coverage and shared opacity;
- destination-alpha glow accumulation must not alter beauty RGB or leak bloom from
  fully transparent texels.

The optional PBR shader family is all-or-none. A missing/link-failed permutation must
disable shared PBR activation and leave the actor native; it must not trip
`LLViewerShaderMgr::setShaders: ASSERT(loaded)` or leave only some materials styled.

## Deferred-native cinematic look redesign — implemented, visual validation pending

The live actor and a Ghost Studio clone enter the renderer at different stages, so
literal clone-shader reuse was rejected as the quality target. Clones keep their late
unlit/post-tonemap treatment. Live deferred actors now use a scene-linear contract
that preserves authored PBR response first and applies the selected cinematic design
at the correct material, lighting, coverage, emission, and atmosphere stages.

- Every persisted look ID `0..27` and distortion ID `0..8` remains supported; no ID
  or saved-setting migration was introduced.
- Chrome (`9`), Gold Statue (`12`), Frost/Ice (`16`), Prism (`17`), and Oil Slick
  (`23`) are physical-material looks. They modify PBR material inputs before the one
  BRDF evaluation instead of repainting an already-lit avatar.
- Graphic and sensor looks classify a stable authored-colour source in Cover mode;
  Layer treats the normally lit HDR result. Alpha interpolates the appropriate PBR
  endpoints and does not trigger a second BRDF evaluation.
- Authored emissive beauty is separated from style-generated emission, filtered once,
  and recombined once before atmospheric attenuation. Synthetic bloom is deliberately
  limited to IDs `2`, `6`, `10`, `14`, `15`, `17`, `19`, `26`, and `27`; the other
  looks do not receive an accidental whole-body glow.
- VHS transforms the lit source, authored Cover source, style-emission source, and
  authored emissive beauty/glow consistently. Shimmer, glitch, brightness, scalar
  distortion, and the shared effect clock are applied exactly once.
- Sky and underwater extinction affect PBR and legacy-world synthetic glow without
  adding fog colour into the bloom buffer. Classic/BOM/system/Animesh world styling
  performs modulation after sRGB decode. The clone path is intentionally unchanged.
- Geometry normals drive Actor FX rims and bloom; authored shading/material normals
  remain responsible for BRDF detail. Degenerate normals, view vectors, and shadow
  divides have finite-value guards.
- Authored OPAQUE/MASK/BLEND coverage remains authoritative. Dissolve is the only look
  that intentionally changes coverage; no generic look is allowed to flatten hair,
  lashes, BOM layers, PBR blend faces, or multi-material heads into opaque cards.
- Shader-family activation remains coupled and fail-open. Unsupported or link-failed
  exact replay leaves the complete actor native for that frame rather than producing
  a partially styled actor or retrying a known-bad shader.
- Expensive perceptual/HDR/tint work is lazy for source-only Clone, Chrome, Dissolve,
  and Gold paths unless lens tint is needed. Full PBR replay, transparent overdraw,
  legacy Layer composition, and Wireframe topology still carry real GPU cost and must
  be profiled rather than assumed inexpensive.

This redesign is intended to make the live version look purpose-built for deferred
rendering, not merely identical in pixels to the clone. Side-by-side clone comparison
is still useful for creative intent, timing, and control response, but acceptance is
the strongest cinematic result each render architecture can produce while preserving
the live actor's materials, animation, alpha, lighting, and scene integration.

## Actor FX looks, controls, and animation audit

Persisted look IDs must remain stable:

| ID | Look | Inherent time law in the existing Ghost Studio design |
|---:|---|---|
| 0 | Ghost | No |
| 1 | Clone | No |
| 2 | Hologram | Yes |
| 3 | Wireframe | No |
| 4 | X-ray | No |
| 5 | Thermal | No |
| 6 | Neon Outline | No |
| 7 | Silhouette | No |
| 8 | Toon/Ink | No |
| 9 | Chrome | No |
| 10 | Dissolve | Yes |
| 11 | Negative | No |
| 12 | Gold Statue | No |
| 13 | Night-vision | Yes |
| 14 | Blueprint | No |
| 15 | Ectoplasm | Yes |
| 16 | Frost/Ice | Yes |
| 17 | Prism | Yes |
| 18 | Thermal Scope | No |
| 19 | Wallhack/ESP | No |
| 20 | Night-vision Tube | Yes |
| 21 | Damage Overlay | Yes |
| 22 | Killcam | Yes |
| 23 | Oil Slick | Yes |
| 24 | Vaporwave | Yes |
| 25 | Halftone/Comic | No |
| 26 | Sonar Reveal | Yes |
| 27 | Hologram Echo | Yes |

“No” means the base look is static by design, not that its optional shimmer, glitch,
or time-driven distortion controls may freeze. Effect FPS `0` means smooth real-time
animation. Nonzero Effect FPS quantizes the effect clock; it does not disable it.

Independent distortion IDs are:

`0 None`, `1 Pixelate`, `2 Voxel`, `3 Lens/Magnify`, `4 Wave/Ripple`,
`5 RGB Split`, `6 Block Glitch`, `7 Vertical Tear`, and `8 VHS`.

All looks and all distortions need evaluation. Do not validate only Hologram,
Wireframe, and Dissolve because they were the initially reported examples.

## Honest render scope and known limits

Shared Actor FX is intentionally limited to the main deferred world beauty view.

- Reflection, cube/prism auxiliary capture, HUD, impostor, shadow, velocity, and
  other auxiliary views remain native/fail-open unless a specific existing path says
  otherwise. Shared color must not contaminate those targets.
- Water/opposite-side and water-straddling actors fail open to native rendering when
  the shared POST_WATER placement cannot preserve correct clip/fog ordering.
- The shared PBR implementation is a forward material replay inside the deferred
  world. It does **not** claim a styled deferred G-buffer, styled SSAO, styled SSR, or
  styled velocity output. Native auxiliary/depth behavior remains responsible for
  those features. These limitations must be visible in the acceptance report.
- The classic/BOM aggregate transparent sort and `LLDrawInfo`/group-level rigged
  alpha depth approximations remain as described above.
- Layer is generally more expensive than Cover because legacy/BOM Layer retains
  native beauty and adds shared treatment. Authoritative PBR Layer can avoid a
  duplicate native PBR beauty pass once ready, but that does not make Layer free or
  guarantee parity with Cover on a mixed actor.
- Wire adds a topology line pass and can be substantially more expensive on dense
  avatars.
- Compilation, shader-link smoke, and screenshots from one avatar cannot prove
  cinematic correctness. In-world validation on the reported production avatars is
  still required.

## Primary implementation locations

Shared activation, proxy collection, readiness, sorting, and replay:

- `indra\newview\llactormover.h`
- `indra\newview\llactormover.cpp`
- `indra\newview\lldrawpoolalpha.cpp`
- `indra\newview\pipeline.cpp`

Avatar/system/BOM/mesh descriptor ownership and readiness:

- `indra\newview\llvoavatar.h`
- `indra\newview\llvoavatar.cpp`
- `indra\newview\llviewerjointmesh.h`
- `indra\newview\llviewerjointmesh.cpp`
- `indra\newview\lldrawpoolavatar.cpp`

Native suppression and material-path integration:

- `indra\newview\lldrawpool.h`
- `indra\newview\lldrawpool.cpp`
- `indra\newview\lldrawpoolbump.cpp`
- `indra\newview\lldrawpoolmaterials.cpp`

Shared Actor Ghost/classic shaders:

- `indra\newview\app_settings\shaders\class1\interface\actorghostV.glsl`
- `indra\newview\app_settings\shaders\class1\interface\actorghostF.glsl`
- `indra\newview\app_settings\shaders\class1\avatar\avatarActorGhostV.glsl`
- `indra\newview\app_settings\shaders\class1\avatar\eyeballActorGhostV.glsl`

Shared PBR beauty/glow shaders:

- `indra\newview\app_settings\shaders\class1\alchemy\actorFxF.glsl`
- `indra\newview\app_settings\shaders\class1\alchemy\actorFxFallbackF.glsl`
- `indra\newview\app_settings\shaders\class1\deferred\sharedActorFxPbrV.glsl`
- `indra\newview\app_settings\shaders\class2\deferred\sharedActorFxPbrF.glsl`
- `indra\newview\app_settings\shaders\class1\deferred\sharedActorFxPbrGlowV.glsl`
- `indra\newview\app_settings\shaders\class1\deferred\sharedActorFxPbrGlowF.glsl`
- `indra\newview\app_settings\shaders\class1\deferred\sharedActorFxPbrSyntheticGlowV.glsl`
- `indra\newview\app_settings\shaders\class1\deferred\sharedActorFxPbrSyntheticGlowF.glsl`
- `indra\newview\llviewershadermgr.h`
- `indra\newview\llviewershadermgr.cpp`

Actor FX state/UI:

- `indra\newview\lldirectorcast.h`
- `indra\newview\lldirectorcast.cpp`
- `indra\newview\llfloaterdirector.cpp`
- `indra\newview\skins\default\xui\en\floater_director.xml`

Light Rig master/keybinding:

- `indra\newview\alcinelightrig.cpp`
- `indra\newview\alpanelcinelightrig.cpp`
- `indra\newview\llviewerinput.cpp`
- `indra\newview\app_settings\settings.xml`
- `indra\newview\skins\default\xui\en\panel_cine_light_rig.xml`
- `indra\newview\skins\default\xui\en\control_table_contents_machinima.xml`

## Required static checks and Release build

Run from `I:\alchemy-machinima`:

```powershell
git status --short
git diff --check

[xml](Get-Content -LiteralPath `
  'indra\newview\skins\default\xui\en\floater_director.xml' -Raw) | Out-Null
[xml](Get-Content -LiteralPath `
  'indra\newview\skins\default\xui\en\panel_cine_light_rig.xml' -Raw) | Out-Null
[xml](Get-Content -LiteralPath `
  'indra\newview\skins\default\xui\en\control_table_contents_machinima.xml' -Raw) | Out-Null

cmake --build build-Windows-vs2026-os --config Release --target alchemy-bin --parallel 4
```

Expected executable:

```text
I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\AlchemyTest.exe
```

Expected staged shader root:

```text
I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\app_settings\shaders
```

After the build, compare every modified/new source shader with its path-relative
staged Release copy. At minimum, hash all `actorghost*`, `avatarActorGhost*`,
`eyeballActorGhost*`, `sharedActorFxPbr*`, and shared Actor FX module files. A linked
executable beside stale GLSL is a failed build artifact even if MSVC succeeded.

Example for one file:

```powershell
Get-FileHash `
  'indra\newview\app_settings\shaders\class2\deferred\sharedActorFxPbrF.glsl', `
  'build-Windows-vs2026-os\newview\Release\app_settings\shaders\class2\deferred\sharedActorFxPbrF.glsl' `
  -Algorithm SHA256
```

The two hashes must match. Repeat for every modified/new shader.

## Exact executable smoke test and log checks

Launch the exact Release artifact and retain its PID:

```powershell
$actorFxExe = 'I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\AlchemyTest.exe'
$actorFxProc = Start-Process -FilePath $actorFxExe `
  -WorkingDirectory (Split-Path -Parent $actorFxExe) -PassThru
$actorFxProc | Select-Object Id,Path,StartTime
```

Watch the exact viewer log:

```powershell
$actorFxLog = 'C:\Users\xianw\AppData\Roaming\AlchemyMachinima\logs\Alchemy.log'
Get-Content -LiteralPath $actorFxLog -Tail 250
rg -n -i -S `
  'sharedActorFx|actorghost|compile|link|error|fatal|assert\(loaded\)|setShaders' `
  $actorFxLog
```

Required smoke result:

- the exact process remains responsive through shader loading;
- no Actor FX/Actor Ghost/shared-PBR shader compile or link failure;
- no `LLViewerShaderMgr::setShaders: ASSERT(loaded)` fatal;
- optional shared PBR failure, if deliberately injected for testing, fails open to
  native without aborting the primary shader load;
- the login/world view uses the just-staged shader tree.

When finished, stop only the captured test PID:

```powershell
Stop-Process -Id $actorFxProc.Id
```

## Cinematic in-world runtime matrix

Use a fixed camera, resolution, graphics preset, environment, animation, and capture
codec for comparisons. Disable unrelated HUDs where possible. Record both a bright
HDR environment and a dark set with practical emissive lights. Save screenshots and
short lossless clips; a still cannot validate animation, flicker, or temporal sorting.

### 1. Every look, both modes, every key Alpha value

For each look ID `0..27`, capture all of these states on the same reference actor:

- disabled/native baseline;
- Layer Alpha `0`, `.25`, `.60`, and `1.0`;
- Cover Alpha `0`, `.25`, `.60`, and `1.0`.

That is 224 enabled primary look/mode/alpha states, plus native baselines. Reject:

- a material island or head/body part staying native while the rest is styled;
- Cover leaking the native actor underneath;
- Layer losing authored coverage or replacing instead of mixing;
- Cover Alpha `0` leaving color, depth occlusion, DoF silhouette, or glow;
- per-face phase resets, flicker, PBR shimmer, or a style discontinuity at material
  boundaries.

Repeat representative states at Effect FPS `0`, `12`, and `24`. Effect FPS `0` must
be smooth, not frozen.

### 2. Every look with every distortion

At Alpha `.60`, run all distortion IDs `0..8` against all look IDs `0..27` in Layer
and Cover. Capture at least a short clip per row/look so animated distortion is
observable. Reject UV/color treatment being applied outside authored alpha coverage,
RGB split exposing transparent card rectangles, or one material using a different
time origin.

### 3. Animation parity with Ghost Studio

Place a Ghost Studio clone and shared-styled live actor side by side using the same
look, values, and time controls.

- Explicitly time-check looks `2`, `10`, `13`, `15`, `16`, `17`, `20`, `21`, `22`,
  `23`, `24`, `26`, and `27`.
- For the other looks, enable shimmer, glitch, and each clocked distortion in turn.
- Run for at least 30 seconds and cross an effect-time wrap/quantization boundary.
- Verify the live actor animation deforms normally while the effect clock advances.
- Reject a look that is visually frozen, visibly restarts per face/material, crawls
  at a different rate than the clone, or snaps when Effect FPS changes.

### 4. Avatar and surface coverage matrix

Run the primary look/mode cases across:

- classic/system body, eyes, skirt, and hair;
- BOM skin, tattoos, alpha layers, and clothing layers;
- Bento rigged head with BOM-on-mesh, eyelashes, and multiple material faces;
- rigged body/clothing/attachments with legacy and PBR materials;
- static attachments, including alpha mask and alpha blend faces;
- worn Animesh, both inherited wearer style and explicitly styled root;
- standalone Animesh with a regenerated control avatar;
- You and Subjects A, B, C, and D simultaneously.

Use the originally failing complex heads/outfits as mandatory cases. Verify skeleton
animation, facial animation, alternate binds, attachment placement, and root motion.

### 5. Scalar/indexed PBR material matrix

Test scalar and indexed VBOs separately; do not infer one from the other. For each,
cover static and rigged geometry and OPAQUE, MASK, and BLEND alpha modes.

Required material features:

- base-color texture plus nonwhite/nonunit factor;
- normal map with obvious tangent-space detail;
- combined ORM/occlusion, roughness, and metallic extremes;
- emissive texture/factor and a material with no authored emissive stream;
- UV offset/scale/rotation and alternate texture coordinate where available;
- double-sided and single-sided material;
- unlit/fullbright material where supported;
- indexed mesh with several materially different slots in one VBO.

Reject:

- debug assertion or missing geometry caused by requiring `texture_index` on a
  scalar VBO;
- indexed slots sampling another slot's texture/factor/cutoff;
- arbitrary texture alpha punching holes in OPAQUE;
- MASK cutoff drift or BLEND becoming opaque;
- flipped/dropped normal detail, ignored ORM, wrong metallic/roughness, broken UV
  transform, wrong back-face normal, or rigged palette drift;
- double native/shared PBR beauty or double emissive bloom;
- a material becoming native while its actor remains partly styled.

### 6. Alpha sorting, depth, and DoF

Build shots with overlapping eyelashes, hair cards, transparent clothing, glass,
multiple rigged BLEND faces, static BLEND attachments, BOM alpha, two styled actors,
and an unrelated world alpha surface.

- Orbit the camera slowly and animate both actors.
- Test both modes at Alpha `0/.25/.60/1`.
- Put the camera and actor on both sides of the water surface.
- Enable and disable DoF, then use an exaggerated focus distance/aperture.
- Verify stable far-to-near component order without frame-to-frame flicker.
- Cover Alpha `0` must not write invisible depth or blur.
- Nonzero Cover must not lose all depth/DoF.
- Layer must retain the expected native depth while its shared alpha treatment sorts
  with world transparency.
- Record known residual artifacts that correlate with aggregate BOM or
  `LLDrawInfo`/group bounds rather than claiming per-triangle sorting.

### 7. Wireframe multi-face/material and GL-state test

Use dense avatars containing system/BOM surfaces, scalar and indexed PBR,
legacy-material faces, rigged/static mesh, MASK hair cards, BLEND layers, and several
materials in one VBO.

- Compare shared Wireframe with the Ghost Studio clone at the same hue/brightness.
- Verify exact live deformation and no missing/misaligned head or attachment pieces.
- Verify transparent card texels do not reveal rectangular topology blocks.
- Put half the actor behind an opaque prop and behind transparent world geometry.
- Toggle Actor Wire repeatedly with global viewer wireframe both off and on.
- After each draw, subsequent pools must see their expected polygon mode, culling,
  depth mask/function, blend function, color mask, texture bindings, and shader.
- A global viewer-wireframe state must be preserved when enabled; Actor Wire must not
  leak line mode when global wireframe is disabled.
- Repeat with two actors and an auxiliary reflection/cube/prism/HUD capture to prove
  the shared pass does not contaminate excluded views.

### 8. Dissolve coverage and animation

- Sweep Dissolve progress slowly from `0` (whole) through `.25/.60` to `1` (gone),
  then reverse it.
- Repeat at Effect FPS `0`, `12`, and `24` while the actor animates.
- Compare live actor and Ghost Studio clone breakup motion and edge treatment.
- Check OPAQUE, MASK, BLEND, BOM, scalar/indexed PBR, and Animesh surfaces.
- At `0`, no islands may already be missing; at `1`, no beauty/glow/depth island may
  remain.
- Inspect native shadow and cinematic motion-blur/velocity behavior separately;
  remember that shared forward beauty does not itself claim a styled velocity
  buffer.
- Capture a high-resolution tiled snapshot and inspect every tile boundary for time
  or pattern seams.

### 9. Authored and synthetic glow

Use HDR/bloom settings that make destination-alpha glow obvious without clipping.

- Test authored emissive zero and nonzero on scalar/indexed, static/rigged PBR.
- Test legacy/fullbright/alpha surfaces and all looks that intentionally synthesize
  glow, especially `2`, `6`, `10`, `14`, `15`, `17`, `19`, `26`, and `27`.
- Verify authored glow is preserved in Layer as intended and Cover shows only the
  selected look/material contract.
- Verify zero-alpha texels produce no glow, MASK holes produce no glow, and BLEND
  glow tracks authored coverage.
- Reject double bloom from replaying both harvested native glow and derived shared
  glow, or glow whose phase differs from beauty/Dissolve.

### 10. Readiness, streaming, rebuild, and ownership

- Enter a scene with cold texture/material caches and enable Actor FX before the
  actor is fully loaded.
- Force outfit rebuilds, spatial-group moves, LOD changes, attachment add/remove,
  Animesh control-avatar regeneration, teleport/leave/return, and scene save/reload.
- The actor must remain wholly native while incomplete, switch wholly styled only
  when ready, and fail wholly native if readiness is lost.
- Verify a separately styled Animesh is not also included in the wearer's replay.
- Verify an unstyled worn Animesh inherits its wearer.
- Verify style identity does not leak between You/A/B/C/D or across regenerated
  control-avatar UUIDs.
- Apply block/mute and Never Render; the shared path must not revive the actor.

### 11. Water and excluded render views

- Move an actor from above water to below water, straddle the surface, and place the
  camera on both sides.
- Expected safe behavior for unsupported placement is a complete native fail-open,
  not half shared and not incorrect clip/fog.
- Capture reflection, cube, prism/auxiliary, HUD, impostor, shadow, and velocity
  views/passes where observable.
- Shared color must appear only in the supported main deferred world beauty view.
- Explicitly report the absence of styled deferred G-buffer/SSAO/SSR/velocity parity;
  do not mark it passed merely because main beauty looks correct.

### 12. Performance and frame pacing

Use the same camera/scene for each measurement and record both CPU and GPU frame
time, not only FPS. Warm shaders/textures first, then capture a stable interval.

Measure:

- Actor FX disabled/native baseline;
- Layer and Cover for a representative non-Wire look at `.25/.60/1`;
- Wireframe;
- an animated look with synthetic glow;
- scalar PBR and multi-slot indexed PBR;
- one actor, You+A/B/C/D, and a crowded mixed-avatar shot;
- the earlier opaque-Cover baseline versus Layer, because Layer was reported to have
  the larger FPS drop.

Record draw calls and GPU timing if available. Some Layer overhead is architectural
for legacy/BOM, and Wire adds a line pass, but unexplained growth over time, duplicate
PBR beauty/glow, per-frame allocation spikes, or a disproportionate multi-actor
scaling curve is a defect. Do not invent a pass threshold after the fact; include raw
numbers so the owner can set the cinematic budget.

### 13. Non-Actor-FX regression checks

- Resize Joystick Configuration at normal and small window sizes; verify all controls
  and OK/Cancel remain reachable.
- Open Actor Gaze and verify `Pose Polish...` is beside `Gaze Cues...`; on a clean
  default profile verify M1/M2/M3/M4/M6 on and M5 off.
- Assign A/B/C/D from avatar, attachment/object, and Director context routes; reload.
- Verify the Light Rig has `Enable` and `Gizmo` but no visible `Power`; verify per-
  subject You/A/B/C/D soft power.
- Confirm `cine_light_rig_power` has no default binding, bind it manually, and verify
  it toggles the visible Enable master.
- Change Projector Shafts in Lightbox and Director, exercise all four presets, and
  verify two-way setting parity.
- Test the easy all-beam cone/global feather controls and manual Key/Fill/Rim/BG cone
  controls.
- Record local atmospheric volume no-fog only as the already-held separate issue.

## Failure report format

For every visual, shader, lifecycle, or performance failure, record:

- exact git commit and whether the worktree had additional changes;
- exact executable path and build configuration;
- source/staged shader hashes for the affected shader;
- GPU, driver, OS, resolution, graphics preset, AA, DoF, HDR/bloom, and environment;
- avatar/surface type: system, BOM, rigged, static, worn Animesh, standalone Animesh,
  legacy, scalar PBR, or indexed PBR;
- actor identity/assignment: You, A, B, C, D, wearer, or persistent Animesh root;
- Actor FX mode, look ID/name, Alpha, Dissolve progress, distortion ID/amount,
  shimmer/glitch values, brightness, and Effect FPS;
- material alpha mode, cutoff, double-sided/fullbright status, authored emissive
  status, and relevant texture maps/transforms;
- whether the failure appears in main beauty, alpha, depth/DoF, glow, shadow,
  velocity/motion blur, reflection/auxiliary capture, or snapshot;
- native, Layer, and Cover CPU/GPU frame times from the same shot;
- screenshot or lossless clip and exact reproduction steps;
- relevant `Alchemy.log` lines, without truncating the first shader error.

## Acceptance boundary

The implementation, Release build, staged-shader comparison, XML checks, static
audits, and exact-artifact shader-load smoke have passed with the evidence recorded
above. The work is ready for a cinematic sign-off only after the in-world matrix has
evidence on the reported complex avatars. A successful Release build proves
C++/link integration; a responsive login screen proves shader compilation/loading;
neither proves alpha ordering, PBR material parity, animation parity, visual
perfection, or acceptable frame time.

The held local atmospheric-volume issue is not part of this Actor FX acceptance. The
main deferred-only and shared-forward-PBR limitations are part of the result and must
remain documented even if every supported-path test passes.
