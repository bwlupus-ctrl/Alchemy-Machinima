# Mirror light rig implementation report

2026-10-20. Based on `feature/director-kit-p0`, HEAD
`1f9c4fc19326ddfb0221af5735c5a0065b543966`.

## Design and scope

One per-clone, session-only **Mirror light rig** checkbox. It is enabled only for
an entity clone whose runtime drive is Reflect or True Mirror. It affects the
selected clone, with no group fan-out, global mode, extra controls, or saved scene
state. A duplicated clone starts unticked. Leaving a supported drive removes the
copy immediately; the session preference can remain checked for re-entry.

The source is the user's **Self** Cine Light Rig, not the currently selected
editing slot (A/B/C/D can be edited without changing this source). The existing
manager owns five independently evaluated rigs. `setAnchor()` resets smoothing;
`getAnchor()` returns the legacy UUID. Normal Self resolution uses the agent;
A/B/C/D resolve cast subjects. Group/object anchoring and smoothing are already
resolved by the source rig before copying. Re-anchoring a second evaluated rig
would reproduce neither the source's world positions nor live FX reliably.

Instead, the manager owns session-only `ALCineLightRig` instances keyed by clone
runtime UUID. They never tick, read a second setup, or enter persistence. After
all source rigs evaluate each frame, `mirrorFrom()` reuses the rig's existing
emitter creation/destruction and copies its actual evaluated light objects:
four projectors, four bounce omnis if present, and catchlight if present.
Colour (with Kelvin already applied), intensity (with EV already applied), size,
radius, falloff, cutoff, cone, cookie/gobo and animated gobo override, shadow
softness are copied. Copies never cast shadows; rotating gobo speed is reversed.
Cookies/patterns remain horizontally unflipped (see Review fixes).
Per-light rig rim/wrap parameters are copied and
resolved through the existing manager lookup. No second lighting model or
editable second rig was added.

World positions use the pure `ALReflect::planePoint()` helper. Aim and up use
`planeVector()`; a proper quaternion is reconstructed from the reflected axes.
The source's current source-rig FX, transitions, edits and light on/off state
therefore propagate every frame. Rig off removes the derived emitters on the
next rig update, before rendering.

### Mirror planes

- **Reflect:** uses the clone's existing valid `mReflectPlanePoint` and
  `mReflectPlaneNormal`, including Fixed capture, Face pair, Live and recapture.
  The manager runs after the Ghost Studio post-object-list pose/plane pass.
- **True Mirror:** the existing `stampTrueMirrorSkeleton()` copies pose and
  optionally source facing at the clone's own placement; it has no stored or
  geometric reflection plane. For this feature, derive the vertical live
  source/clone foot bisector with the same `ALReflect::updatePlane()` rule as
  Reflect: midpoint as point, horizontal source-to-clone unit vector as normal.
  Near coincident feet use horizontal source forward, then world +X. This is
  explicitly a light-plane convention; it does not change True Mirror's pose.
- Missing source or invalid plane removes the copy rather than retaining stale
  light objects. Vertical planes preserve light height and source/light distance
  when comparing reflected positions.

### Clone-only gate: supported without renderer changes

The existing `rebuildActorOnlyRegistry()` already explicitly allows entity
clones. `LLDirectorCast::resolve()` resolves them through Ghost Studio;
`LLVOAvatar::getActorFxOwnerId()` gives the clone its own runtime ID.
`LLActorMover::actorMaskAvatarEligible()` accepts visible entity clones, and the
existing native body/eyes, attachment owner bytes and forward light selection
consume the same registry. No pipeline or shader changes were needed.

Each derived rig is registered with `mActorOnlyAll=true`, the clone's own
recipient/FX-owner ID, and no world-bounce exception. Thus every copied projector,
omni and catchlight is actor-only regardless of the source's Actor only settings.
The existing mask excludes these lights from surrounding geometry, probe captures
and atmospheric shafts/volumes. If the Actor Mask debug bypass is on, derived
lights are removed rather than becoming world lights. The existing six-recipient
limit remains: overflow-only lights receive the existing orphan bit and affect
nothing, never the surroundings. Styled/impostor/render-path restrictions of the
existing Actor Mask remain applicable.

Untick destroys emitters immediately and refreshes the registry. Runtime drive
changes (including switcher overrides), Studio deletion, avatar `markDead()` and
manager shutdown also destroy them. Each frame prunes missing or unsupported
clones. Emitter teardown clears light membership, shadow/softness/gobo entries
and local objects through the existing idempotent shutdown path. No mirrored
state is serialized in rig, scene or clone persistence.

### Cost

Up to **nine additional local light objects per opted-in clone** (four spots,
four bounce omnis, one catchlight), with per-frame transform/property copies,
registry work, mask drawing and light rendering. Existing forward-light budgets
still apply; several clones can exhaust those budgets or the six-recipient mask
capacity. Copies consume no spot-shadow slots. No new budget mode/control was added.

## Exact click paths and UI reachability

1. Shared panel, standalone host: **Machinima > Clone Studio... > select an
   entity clone > Pose > drive combo > Reflect or True Mirror > Mirror light
   rig** (beside Face pair/distance).
2. Shared panel, legacy Director host: **Machinima > Director Console... > Cast
   > Clones > select an entity clone > Pose > drive combo > Reflect or True
   Mirror > Mirror light rig**. Both hosts embed `panel_ghost_studio.xml`.
3. Director v2: **Machinima > Director Console... (F2) > Cast > Clones > select
   an entity clone > Motion > Pose & drive > Drive > Reflect or True Mirror >
   Reflect card > Mirror light rig**. Face pair is an alternative way to enter
   Reflect. The checkbox is adjacent to Mirror position in that card.

`ALPanelGhostStudio::postBuild()` binds the real widget in both layouts and
`refreshDetail()` gates it using the ghost's actual runtime drive. The v2 row is
in `director_mockup_data.json`, generated as a `kit_switch` (the checkbox class).
Widget undo history is disabled because this state is per-clone/session-only.
The generator row pin is **2140**, with an explanatory comment. An audit of
Reflect widget names found no independent C++-built page/list requiring another
entry: both layouts use this same controller and the kit creates rows from XUI.

## Validation

- `zs_check.ps1 -Files <one file>`: PASS in separate invocations for every
  touched translation unit: `alcinelightrig.cpp`, `alcinelightrigmanager.cpp`,
  `alghoststudio.cpp`, `alpanelghoststudio.cpp`, `llghostavatar.cpp`, and
  `tests/alreflectmath_test.cpp` (with `-IncludeTests`).
- Additional includers: `alpanelcinelightrig.cpp` for `alcinelightrig.h`;
  `alcinelightrig.cpp` for `alcinelightrigmanager.h`; `llghostavatar.cpp` for
  `alghoststudio.h`; `aluiperfselftest.cpp` for `alpanelghoststudio.h`;
  `alreflectanim.cpp` for `alreflectmath.h`. All PASS. The compiler's supported
  `-CachePath` points at the existing workspace flags cache because the user
  cache is outside the writable workspace. Commands run via PowerShell with
  execution-policy bypass. Logs: `.tmp/rig_mirror_zs_*.log`.
- Reused the earlier standalone unit-test compile/link commands (not a viewer
  build): `.tmp/reflect_math_compile.cmd`, `.tmp/reflect_unit_link.cmd`, then
  `.tmp/reflect_unit_bin/INTEGRATION_TEST_alreflectmath.exe`: **27/27 PASS**.
  New tests 25-27 cover offset/full-3D position and direction reflection,
  actor-left becoming reflection-right, height/distance/direction-length
  preservation and involution. Logs: `.tmp/rig_mirror_test_compile.log`,
  `.tmp/rig_mirror_test_link.log`, `.tmp/rig_mirror_tests.log`. Environment setup
  prints a non-fatal missing `vswhere.exe` message; compilation/linking succeed.
- Regenerated with `director_kit_gen.py`, `director_page_sources.py` and
  `director_functional_manifest.py`; all three `--check` commands PASS.
  Functional manifest retains five generator warnings; its registry/currentness
  check passes. Logs: `.tmp/rig_mirror_{kit,sources,manifest}*_check.log`.
- `prebuild_guard.ps1 -ChecksOnly`: **PASS, all 11 gates** (exit 0),
  via the existing `.tmp/reflect_prebuild_checks.ps1` wrapper. It invokes the
  installed guard unchanged, redirects only the compiler's supported CachePath,
  and sets `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, and
  `GIT_CONFIG_VALUE_0=false`. It also uses the existing workspace safe-directory
  Git config because the user-global config is unreadable in this sandbox.
  The initial run rejected the new per-frame by-name settings read; using
  `LLCachedControl` fixed it. Final log: `.tmp/rig_mirror_prebuild.log`.
- `git diff --check`: **PASS**. The new untracked report was also checked for
  trailing whitespace and conflict markers directly.
- `git ls-files --eol`: all three touched tracked JSON files report
  **i/lf w/lf attr/text eol=lf** (`director_mockup_data.json`,
  `page_sources.json`, `functional_manifest.json`).

No viewer build or launch, commit, push, staging, or stash operation was done.
Existing untracked work and the dirty `vcpkg` submodule were left alone.

## In-world acceptance still required

1. In all three UI paths, select a clone, enter Reflect (also Face pair) or True
   Mirror, and tick Mirror light rig. Other drive modes and overlay clones must
   have the checkbox disabled. Select another clone: its setting is independent.
2. Use a strongly asymmetric Self rig (different key/fill colours, Kelvin and EV,
   key on actor's left). Verify mirrored position/aim and equal look parameters:
   the key appears on the reflected clone's right. Test each projector, bounce
   omni and catchlight; edit all look settings, orbit, FX, cue fades and gobos
   while watching the copy update. Editing another rig slot must not change
   which rig is copied. Test rig off/on.
3. Verify clone body, system eyes/hair and rigid/rigged attachments receive the
   copy while source actor, a second clone, nearby avatars, walls/floor, probe
   captures and atmospheric volumes receive none of the copied lights. Test
   source Actor only both on and off. Confirm ordinary source/world lighting
   continues to follow its existing settings.
4. Move/rotate the source and clone; test Fixed, Live, recapture and Mirror
   position. Test True Mirror's live bisector and near-coincident fallback,
   seated source, non-default clone scale and different regions/origins.
5. Untick; leave supported drive (also via switcher/freeze); delete the clone;
   refresh its runtime avatar; lose its source; cross regions; log out. Confirm
   no copied light or stale registry ID survives. Save/load a scene and relog:
   no mirrored rig is restored. Duplicate a clone: it starts unticked.
6. Try several opted-in clones, full shadow/forward budgets and more than six
   distinct Actor Mask recipients. Confirm graceful existing budget limits and
   no world-light leakage, including Actor Mask debug bypass.

## Files

Handwritten implementation, tests, UI source and generator pin:

- `indra/newview/alcinelightrig.cpp`
- `indra/newview/alcinelightrig.h`
- `indra/newview/alcinelightrigmanager.cpp`
- `indra/newview/alcinelightrigmanager.h`
- `indra/newview/alghoststudio.cpp`
- `indra/newview/alghoststudio.h`
- `indra/newview/alpanelghoststudio.cpp`
- `indra/newview/alpanelghoststudio.h`
- `indra/newview/alreflectmath.h`
- `indra/newview/llghostavatar.cpp`
- `indra/newview/tests/alreflectmath_test.cpp`
- `scripts/code_tools/director_kit/director_mockup_data.json`
- `scripts/code_tools/director_kit_gen.py`
- `indra/newview/skins/default/xui/en/panel_ghost_studio.xml`
- `doc/director_kit/RIG_LIGHT_MIRROR_REPORT.md`

Generated outputs (including source-hash-only changes in sibling pages):

- `doc/director_kit/functional/FUNCTIONAL_MANIFEST.md`
- `doc/director_kit/functional/functional_manifest.json`
- `doc/director_kit/page_sources.json`
- `doc/director_kit/page_sources.md`
- `doc/director_kit/pages/ghosts_tab.md`
- `indra/newview/skins/default/xui/en/director_kit_index.xml`
- `indra/newview/skins/default/xui/en/floater_director_v2.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_camera_frame_orbit.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_camera_moves.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_camera_shot.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_camera_takes.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_camera_vcam.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_capture_scene_options.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_dance.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_path.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_cast_props.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_lens_diopter.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_lens_filters.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_lens_focus.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_light_actors.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_light_cues.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_light_practicals.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_look_actor_style.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_look_bloom_glow.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_look_tron.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_animate.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze_cues.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_polish.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_performance_switchboard.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_quick.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml`
- `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml`
- `scripts/code_tools/director_kit/gen_report.txt`

## Review fixes

2026-10-20 fix round against `.tmp/review_codex_rigmirror.md`:

1. **Scene lifecycle:** accepted `applySceneData()` paths (v1 and absent rig
   blocks, including legacy fallback) shut down and clear every `mMirrors`
   entry, clear every Studio instance's `mMirrorLightRig` opt-in, and rebuild
   the Actor Mask registry. Unsupported rig versions retain the existing
   rejected-apply behavior. Clones cannot recreate copies after a scene load.
   Static lifecycle confirmation: region teardown after teleport calls
   `LLViewerObjectList::killObjects()` and clone `markDead()` removes its copy;
   avatar runtime refresh retires the old ghost through the same death path.
   Missing/dead runtimes and missing sources are also pruned by `updateMirrors()`.
   Self rig off skips copy refresh and shuts down all copies in that update.
   These runtime cleanup paths preserve the session checkbox preference; scene
   apply explicitly clears it. In-world transition testing remains outstanding.
2. **Shadow policy chosen: shadows OFF on all copies.** A destination-only
   no-shadow flag and assertion replace copying the source shadow policy.
   This uses the existing suppression gate before the shadow auction, with no
   pipeline change. Original lights retain their own shadow settings and slots.
   `scripts/code_tools/rig_mirror_shadow_test.py` compiles the production mirror
   policy, suppression function and shadow auction extracted from their source
   files, using lightweight object stubs. It asserts identical original slot
   assignments with/without copies over 4,320 cases: all candidate orders,
   2/4/10 slots, source shadows on/off, equal-priority Self+A versus A-only
   copies, a high-priority copy, other rig and world lights, and repeated copy
   updates. **PASS**; log: `.tmp/rig_mirror_fix_shadow_tests.log`.
3. **Projected pattern limitation accepted:** the existing ordinary projector
   path has no general horizontal UV flip. Spotlight params are FOV/focus/
   ambience; gobo zoom is positive, and pattern params are pattern-specific.
   The camera-feed-only UV transform is not an ordinary cookie/gobo control.
   A general horizontal flip needs pipeline/shader support, so cookies and
   procedural patterns remain enabled and horizontally unflipped. Asymmetric
   patterns therefore are not exact mirror images. No pipeline/shader edits.
   Rotating gobos (mode 1) do use the existing signed speed parameter with the
   opposite sign; other animation modes keep their source speed. New helper
   test 28 covers positive/negative/zero/boundary speeds, involution and all
   non-rotation modes. Reflection unit tests: **28/28 PASS**, freshly compiled
   and linked as standalone tests; no viewer build.
4. **First frame:** `setIsLight()` now precedes copied light-property setters,
   ensuring newly enabled emitters have their light parameter block before
   colour, intensity, radius, falloff, cutoff and projector properties are set.

Fix-round validation: separate `zs_check.ps1 -Files <one .cpp> -IncludeTests`
calls PASS for all six feature translation units (including the test), plus
`alreflectanim.cpp` as a helper-header includer. Logs:
`.tmp/rig_mirror_fix_zs_*.log`. Standalone reflection test compile/link/run logs:
`.tmp/rig_mirror_fix_test_compile.log`, `.tmp/rig_mirror_fix_test_link.log`,
`.tmp/rig_mirror_fix_reflect_tests.log`. The VS environment setup still emits
the pre-existing non-fatal `vswhere.exe` message; test compilation succeeds.

`prebuild_guard.ps1 -ChecksOnly`: **PASS, all 11 gates, exit 0**, through the
existing `.tmp/reflect_prebuild_checks.ps1` wrapper with `GIT_CONFIG_COUNT=1`,
`GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false` and the supported
workspace compiler cache. The initial run detected stale page-source evidence;
regenerating `director_page_sources.py` refreshed its generated index, and the
final run passes. Log: `.tmp/rig_mirror_fix_prebuild.log`.
Python shadow-test syntax and untracked report/test whitespace checks PASS.
`git diff --check`: **PASS** (exit 0). No viewer build/launch, commit, push,
staging or stash operation was performed.
