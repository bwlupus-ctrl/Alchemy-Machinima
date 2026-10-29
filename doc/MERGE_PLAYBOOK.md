# Upstream rendering re-port playbook

## Grounding and scope

Authoritative committed surfaces:

- Fork: `develop` at `47af3df7b1a535e01bf44e9151311fe56562cc6b`
- Upstream: `alchemy-upstream/develop` at `af0f3bd1beb228ddd5065477211e23f227bb62f0`
- Merge base: `7c11d3f38bd2f28736f8ede94487c9bb9ffb1e8a`
- Divergence: 327 fork commits / 255 upstream commits
- Rendering intersection: 80 files changed on both sides
- Fork additions since the base: 470 files repository-wide; 326 below `indra/newview`, including
  73 `.cpp`, 73 `.h`, 32 shaders, and 64 skin/XUI files

The required scope is the full committed fork, not only VCam, clones, and six renderer features. A
successful re-port must account for every fork-added, modified, renamed, and deleted path.

### Quarantined WIP

`stash@{0}` at `80769c1956479fa93f5938c5d6e45cc07bde8753` is **not committed fork surface**. It contains
`alprismcamdriver.*`, virtual-output refactors, live-feed projector-cookie remapping, rain work, and
diagnostics. Its stash message records a known regression: moving the main camera moves feed shadows.

Do not include this stash in the required re-port, do not use `git stash apply`, and do not describe
its APIs as committed. It may be recovered path-by-path only after the committed re-port passes all
acceptance tests. See the optional WIP stage at the end.

## Strategy

Choose Option C: create an integration branch at the pinned upstream SHA and re-port the entire
committed fork as curated, dependency-ordered, buildable vertical slices.

Reject:

- Giant merge: Git exposes textual conflicts but not old sampler, target, UBO, reverse-Z, probe,
  shader, or impostor semantics.
- Rebase of 327 commits: repeats conflicts hundreds of times and obscures the final renderer
  contract.
- Hybrid renderer merge: acceptable only for clearly isolated application/assets after the complete
  inventory classifies them; never for the renderer intersection.

## Critical endgame: replace the `develop` ref, never merge histories

The integration branch begins at upstream. Its merge base with the old fork `develop` remains
`7c11d3f38bd`; merging it into old `develop` at the end would recreate the giant merge this playbook
rejects.

After the integration head has passed all authorized builds, in-world tests, review, and user
approval, move `develop` to the integration head while preserving the old ref:

```powershell
$oldDevelop = git rev-parse develop
$integration = git rev-parse integration/upstream-af0f3bd
$backupRef = 'backup/develop-before-upstream-ref-swap-20260810'

git branch $backupRef $oldDevelop
git show-ref --verify "refs/heads/$backupRef"

git switch integration/upstream-af0f3bd
git branch -f develop $integration
git switch develop

git rev-parse develop
git rev-parse $backupRef
git merge-base develop $backupRef
```

The expected final state is `develop == $integration`; the backup remains at `$oldDevelop`. There is
no final `git merge`.

Updating remote `origin/develop` rewrites its history and requires separate explicit user approval
and coordination with every consumer. Use a lease pinned to the previously verified remote SHA:

```powershell
git fetch origin develop
$oldRemoteDevelop = git rev-parse origin/develop
# Confirm $oldRemoteDevelop is the expected protected old fork head before continuing.
git push --force-with-lease="refs/heads/develop:$oldRemoteDevelop" origin develop
```

Never use bare `--force`. If the lease fails, stop; someone else advanced the remote. Do not delete
the backup ref until the replacement has been accepted and separately archived.

Risk: destructive branch-history replacement. Confidence: high in mechanics; execution requires
explicit approval.

## Full application/file inventory ledger

Before editing, generate a repository-wide ledger from committed fork state:

```powershell
$mergeBase = '7c11d3f38bd2f28736f8ede94487c9bb9ffb1e8a'
$forkHead = '47af3df7b1a535e01bf44e9151311fe56562cc6b'

git diff --name-status --find-renames "${mergeBase}..${forkHead}"
git diff --name-only --diff-filter=A "${mergeBase}..${forkHead}"
git diff --name-only --diff-filter=M "${mergeBase}..${forkHead}"
git diff --name-only --diff-filter=D "${mergeBase}..${forkHead}"
```

The implementation ledger must have one row per changed path with these columns:

| Column | Required content |
|---|---|
| Path | Exact repository path |
| Git status | A, M, D, or R from merge base to committed fork |
| Subsystem | Renderer core, VCam, clones, director, temporal, path tools, weather, UI/assets, etc. |
| Landing slice | Exact numbered step below |
| Action | KEEP-AS-IS, KEEP-ADAPT, UPSTREAM-WINS, DROP-OBSOLETE, or DELETE |
| Source | Committed fork SHA/path; stash sources are forbidden in the required ledger |
| Dependencies | Headers, hooks, shaders, settings, CMake/manifests needed for compile/runtime closure |
| Validation | Static, startup, in-world, or asset/UI acceptance case |

At minimum, assign these application groups explicitly:

- Ghost Studio suite: `alghost*`, panels/tools, clone/spawn/material/interaction/nameplate code.
- Director suite: switcher/model/hotkeys/animation switcher, camera panels, director UI and hooks.
- Cinematic camera committed surface: `updateCamera`, `applyFrameLens`, `resolveAnchor`,
  `captureCurrentSwitcherView`, and committed `pattern*()` methods.
- Temporal capture, motion velocity/blur, 10-bit/ReShade and sidecar.
- Flow grid, formations, path editor/object path mover, prop/actor movers.
- Weather model/panel/renderer and all weather assets/settings.
- Radar/animation explorer/phototools and other imported application features.
- Existing-file hook edits in `llappviewer`, `llagent`, `llviewerwindow`, menus, controls, pipeline,
  avatar/volume/draw pools, CMake, manifests, settings, strings, textures, and feature tables.
- All 144 additions outside `indra/newview`, including ReShade/addon/vendor/build/document assets that
  are actually product requirements.

Gate: zero unassigned rows. A summary naming only marquee features fails this gate. The stash and
`.tmp.driveupload/` are explicitly excluded, not silently omitted.

## Checkpoints and rollback model

Create a checkpoint branch before every vertical slice:

```powershell
git branch checkpoint/NN-pre-<slice-name> HEAD
```

Early checkpoints may be static-only. A defect in an early shared file can surface only after later
shader/pipeline consumers land. Therefore:

- Fix forward when the faulty contract is understood.
- For an unsafe or unclear regression, reset the integration branch to the pre-slice checkpoint after
  preserving the failed head in a diagnostic branch.
- Use `git revert <commit>` only when the commit is isolated and no later commit depends on its code
  or edits the same function. Do not promise clean single-step reverts across stacked `pipeline.cpp`,
  shader-manager, draw-pool, or avatar work.

Documented reset mechanics for the integration branch only, with exact refs verified first:

```powershell
git branch diagnostic/failed-NN HEAD
git show-ref --verify refs/heads/checkpoint/NN-pre-slice
git switch integration/upstream-af0f3bd
git reset --hard checkpoint/NN-pre-slice
```

This is destructive and must be performed only on the dedicated integration branch after preserving
the failed head. It is a recipe, not authorization for this research task.

## Buildability rule for shader programs

Any commit that registers or unconditionally creates a shader program must include its adapted GLSL
sources, feature flags, reserved uniforms, and required engine-block injections in the same commit.
Specifically, do not land `gPrismLensProgram`, `gActorGhostProgram`, velocity, froxel/projvol, weather,
or sidecar registrations before their shader files.

If a vertical slice cannot include the complete program, leave its C++ registration and CMake source
entry out or gate both behind a default-off compile/runtime feature that does not call
`createShader()`. A default-off rendering setting is not enough if startup still creates the program.

## Ordered landing sequence

### Step 0 - Baseline, backups, and complete ledger

Actions:

- Verify all three pinned SHAs and the 327/255 divergence.
- Preserve old fork `develop` and recent feature commits in backup refs.
- Generate and adjudicate the full repository ledger above.
- Mark every stash-only item WIP/EXCLUDED.

Gate: pristine upstream integration base, all backup refs resolve, zero ledger omissions.

Checkpoint: `checkpoint/00-upstream-pristine-af0f3bd`.

Validation: static only.

### Step 1 - Upstream renderer core and required fork primitives

Actions:

- Accept upstream `ALTextureSlot`, sampler objects, immutable `LLRenderTarget`/`LLImageGL`,
  `ALUniformBuffer`, engine UBOs, reverse-Z, SH probes, modern shadows, PBR corrections, and impostor
  bake as authority.
- Re-port only fork primitives required by later slices: indexed draw-buffer guard, sidecar attachment
  constants, 10-bit formats, outer-transform data holders, and other ledger-owned `llrender`/shared
  types.
- Keep upstream `finalizeShaderList()` assertion shell; do not restore fork `mShaderList` population.

Gate: old API rejection searches; immutable resource descriptors recorded; upstream architectural
hunks remain.

Checkpoint: `checkpoint/01-renderer-primitives`.

Validation: static. An optional upstream-baseline build is useful when separately authorized, but no
later feature is yet accepted.

### Step 2 - Render-independent committed application foundation

Actions:

- Land complete, renderer-independent application/model/controller classes from the ledger in
  dependency groups.
- Include headers and implementation together when they form a closed source unit.
- Do not add a source to CMake until its includes and link dependencies are present.
- Land committed director/cinematic APIs only. `alprismcamdriver.*` and virtual-output refactor APIs
  remain excluded WIP.

Gate: every landed source is compile-closed or deliberately not yet in CMake; no renderer hooks call
unlanded methods.

Checkpoint: `checkpoint/02-application-foundation`.

Validation: static unless all landed CMake units are closed.

### Step 3 - Committed VCam vertical slice

Land atomically:

- `llprismlens.h/.cpp` committed surface, including `LLPrismLens::MAX_CAPTURES` and methods consumed by
  `pipeline.cpp` (`onRenderTargetsReleased`, clip plane/composite state accessors).
- Committed `llcinematiccamera` and director-switcher integration using real committed APIs:
  `updateCamera`, `applyFrameLens`, `resolveAnchor`, `captureCurrentSwitcherView`, and `pattern*()`.
- Pipeline retained outputs, scratch packs, existing `ScopedPrismRenderState` adaptation, auxiliary
  probe/shadow state, `getSpotShadowTarget` adaptation, and composites.
- `gPrismLensProgram` registration **with** adapted `prismLensF/V.glsl`; replace loose matrix uniforms
  in `prismLensV.glsl` with `//[ENGINE_BLOCK Matrices]`.
- SH `reflectionProbeF.glsl` Prism guards and staged SSR fields described in
  `MERGE_VCAM_ADAPTATION.md`.
- Required settings/CMake/manifests for this committed slice.

Explicitly excluded: live-feed cookie remap, `alprismcamdriver`, and stash virtual-output refactors.

Gate: no unresolved include/link dependency; auxiliary state restores; SH/SSR containment is present;
program registration and GLSL co-land.

Checkpoint: `checkpoint/03-vcam-committed`.

Validation: first VCam-capable build/startup checkpoint when authorized, followed by committed VCam
acceptance. Until then the gate is static only.

### Step 4 - Clone/ghost vertical slice

Land atomically:

- Outer-transform types, ownership, batch key, matrix-cache composition, transformed bounds/LOD.
- Upstream-led avatar/volume hooks, committed `LLGhostAvatar`, control-avatar/animesh behavior,
  animation mirror/hold logic, Ghost Studio application files assigned to this slice.
- Avatar-local current and previous palettes including `last_skin_origin`.
- `gActorGhostProgram` registration **with** adapted actor-ghost GLSL; remove the cpp-local
  `gSkinnedActorGhostProgram` definition/references and use `mRiggedVariant`.
- Minimal clone hooks around upstream impostor bake; never restore the removed third depth pass.
- Required CMake/settings/UI assets for a closed clone slice.

Gate: no absolute object-skin helper; no missing Ghost Studio ledger row; program/source closure.

Checkpoint: `checkpoint/04-clones`.

Validation: clone startup/in-world suite when authorized; otherwise static.

### Step 5 - Alpha interleaving, forced mask, and sidecar draw semantics

Land in one ordered slice:

- `mAvatarDepth` on both `LLSpatialGroup` and `LLSpatialBridge`.
- Deterministic comparators, bridge-to-group fan-out, avatar/control-avatar stamp writers, and stamp
  lifecycle/clearing.
- `EAlphaStream`, merged iterator, dispatch precedence, water/attachment filters, and sidecar guard.
- Forced legacy mask cutoff with `!gltf_mat`.
- Upstream alpha/PBR/impostor behavior retained; `sImpostorRenderAlphaDepthPass` remains removed.

Interleaving is deliberately inert until **both** data fields and stamp writers land in this same
slice. Adding `!sImpostorRender` to the eligibility gate changes current fork bake behavior; treat it
as a safety redesign whose detailed/impostor transition test is load-bearing.

Checkpoint: `checkpoint/05-alpha-mask-sidecar`.

Validation: alpha fallback matrix and impostor transition when authorized.

### Step 6 - Velocity, temporal capture, motion blur, 10-bit/ReShade

Land registrations and adapted sources together:

- Velocity/alpha/avatar-velocity programs and rebased previous-palette uploads.
- Motion blur and temporal capture application/controller/UI files.
- 10-bit, LPM, visible-diffuse seed, sidecar publication, ReShade integration and required assets.
- Existing-file hook edits assigned to this slice.

Gate: no registered program without source; current/previous coordinate contracts match; sidecar MRT
layout and color space documented.

Checkpoint: `checkpoint/06-temporal-output`.

Validation: startup plus velocity/sidecar debug views when authorized.

### Step 7 - Projector volumetrics and froxel slice

Land targets, C++ passes, shader registrations, all class1/class3 GLSL, shared upstream shadow/deferred
adapters, settings, UI, and CMake entries together. Use upstream `bindShadowMaps` and reverse-Z depth
helpers.

Do not include stash-only live-feed cookie remapping.

Checkpoint: `checkpoint/07-projvol-froxel`.

Validation: startup, all shadow tiers, temporal/debug views when authorized.

### Step 8 - Weather slice

Land weather model/panel, pipeline targets/passes, rain/surface/lightning/upsample programs and GLSL,
settings, UI, and assets as one closed feature. State the rain-occlusion depth convention explicitly.

Checkpoint: `checkpoint/08-weather`.

Validation: startup and rain occlusion under forward/reverse-Z test configurations when authorized.

### Step 9 - Remaining committed application, UI, assets, and hook closure

Consume every still-unassigned ledger row: flow grid, formations, path/prop/actor tools, phototools,
radar/animation explorer, remaining director panels, settings, XUI, translations, textures,
feature tables, manifests, CMake, ReShade/addon/vendor files, and all existing-file hooks.

For any remaining shader program, co-land registration and GLSL. Preserve upstream
`AlchemyRenderUBOUpdateMode`; keep removed renderer settings removed.

Gate: zero ledger rows remaining and no CMake/manifest reference to a missing path.

Checkpoint: `checkpoint/09-application-closure`.

Validation: full application startup/UI smoke suite when authorized.

### Step 10 - Static adversarial closure

Required:

- Zero `UNKNOWN` pipeline/shared-file hunks.
- Zero unassigned repository ledger rows.
- No committed requirement sourced only from `stash@{0}`.
- All program registrations paired with sources/engine blocks.
- Obsolete API rejection searches clean or individually justified.
- XML/settings/feature tables parse and contain no duplicate/dead keys.

Checkpoint: `checkpoint/10-static-ready`.

Validation: static only.

### Step 11 - Authorized build and in-world acceptance

This research task does not build. In the implementation phase, the designated builder should build
only dependency-complete checkpoints. Steps 0-2 are explicitly static-only; later slices are buildable
only when their CMake/program/source closure gate passes.

Run VCam, clone, alpha/mask, temporal/sidecar, projvol/froxel, weather, water, mirrors, cube captures,
HUD, settings/UI, and application-tool acceptance. A compiler-clean binary is not final acceptance.

Checkpoint: `checkpoint/11-accepted`.

### Step 12 - Ref replacement with explicit approval

Perform the local `develop` ref replacement described above. Remote force-with-lease is a separate,
explicitly approved action. Preserve old develop backup refs.

## Optional post-acceptance WIP recovery

Only after Step 12 acceptance, create a separate branch from the accepted new `develop` and recover
individual WIP paths without applying the stash wholesale:

```powershell
git switch -c wip/reland-vcam-projector develop
git show 'stash@{0}:indra/newview/app_settings/shaders/class1/deferred/deferredUtil.glsl'
git show 'stash@{0}:indra/newview/pipeline.cpp'
git show 'stash@{0}^3:indra/newview/alprismcamdriver.cpp'
```

The stashed cookie contract is two vec4 affine transforms, `xy=scale`, `zw=offset`, applied in this
order:

```glsl
vec2 cookie_tc = tc * proj_cookie_orient.xy + proj_cookie_orient.zw;
cookie_tc = cookie_tc * proj_cookie_region.xy + proj_cookie_region.zw;
```

The identity for both is `(1,1,0,0)`. This is documentation of the WIP source, not approval to
re-land it. First reproduce and solve the recorded main-camera-moves-feed-shadow regression; then
repeat the full VCam/projector/shadow acceptance matrix.

## Definition of done

The committed-fork re-port is done only when:

1. The full repository ledger has zero omissions.
2. Every vertical slice has a protected checkpoint and code/shader/settings closure.
3. Static/adversarial gates have no unresolved blocker or major.
4. Authorized Release/startup and in-world acceptance pass.
5. Old `develop` is preserved in a backup ref.
6. With explicit approval, `develop` is moved to the accepted integration head without a merge.
7. Any stash-derived WIP remains separate unless independently fixed and accepted.
