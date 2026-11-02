# Director Suitcase

This is a generated integration kit for the Director and its creative features,
with source provenance, hook patches and dependency evidence. It is a porting
ledger and guide, not an apply-ready patch or a claim of target buildability.

**All hooks are mapped against local Alchemy base `7c11d3f38bd` (July 2026).
Current upstream may have drifted.** “Upstream Alchemy” here means that local
base; “LL viewer” means the LL-origin `ll*` code present at that base. No current
remote state, stash or external source was consulted. Coordinates describe
committed HEAD. `inputs.json` identifies its `indra` tree and fingerprints the
working source; dirty source is disclosed separately and needs a commit before
it can enter this committed-history hook map.

## Regenerate and check

Run from the repository root with Python 3 (stdlib only):

```powershell
python -I scripts/code_tools/director_suitcase.py
python -I scripts/code_tools/director_suitcase.py --check
python -I scripts/code_tools/director_suitcase.py --selftest
```

Default mode writes only `doc/director_suitcase/`. Check computes the same bytes,
returns 1 for stale/missing/unexpected outputs, and never repairs them. Self-test
uses temporary planted fixtures under this worktree's `.tmp/` and creates no commits.
This gate is not added to prebuild_guard.
After source/kit evidence changes, regenerate upstream kit evidence with its
existing generators as appropriate, then regenerate this suitcase. `--check`
does not run builds, GL harnesses or other generators. Missing/malformed required
inputs fail generation. No timestamp or changing HEAD commit label is embedded;
committing only these outputs should not itself make them stale.

## Read the ledger before copying

`inventory.csv` / `inventory.json` contain 2546 classified paths.
OWNED means absent at the local base: copy source units with their dependencies.
It does not mean standalone or certified creative-only. HOOK means a pre-existing
LL/Alchemy file: adapt individual hunks from `hooks.json`, never copy that file
wholesale. GENERATED covers documents/evidence/renders and generated runtime XUI:
**regenerate and ship required runtime XUI**. TOOLING carries generators, checks
and fixtures separately. EXCLUDED-PERF is pure governor/pool/threading/tuning work;
EXCLUDED-OTHER records artifacts, vendors and unrelated features with reasons.
Mixed shared files remain HOOK; each hunk marked `mixed/perf-review` requires
creative/perf severance. Unrecognised paths/functions/guards remain explicitly
unknown; `unknowns.md` is the explicit review queue. Feature associations use path, changed code/comments, recent file-level
commit subjects and one-hop includes/qualified calls. Commit-only associations are
kept as separate `candidate_features_from_subjects`, with ownership still unknown.
They are candidate ownership,
not a compiler-derived dependency graph. Generic names can overmatch; inspect the
patch and evidence before landing. Layer 99 means unresolved.

## Features and dependency layers

Start common helpers and kit shell, then page/controller registration, then complete
feature vertical slices. A page often depends on a feature controller; shell-first
does not authorize linking all pages against missing engines. Add each page with
its engine, settings, assets, hooks and shader closure. Each linked feature sheet
lists OWNED sources, exact HOOK hunk IDs, setting keys, shaders, XUI, generated
runtime files, CMake entry lines, kit pages and candidate checks. `features.json`
adds include/call edges; `compatibility.json` lists Alchemy/fork helper dependencies.

| Feature sheet | Layer | OWNED | HOOK files | Setting changes |
|---|---:|---:|---:|---:|
| [common-ui](features/common-ui.md) | 1 | 84 | 23 | 5 |
| [director-shell](features/director-shell.md) | 2 | 308 | 53 | 197 |
| [actor-bar](features/actor-bar.md) | 3 | 63 | 13 | 0 |
| [actor-path](features/actor-path.md) | 3 | 100 | 33 | 32 |
| [camera](features/camera.md) | 3 | 76 | 31 | 489 |
| [composition](features/composition.md) | 3 | 40 | 23 | 12 |
| [gaze](features/gaze.md) | 3 | 53 | 18 | 87 |
| [stepped-animation](features/stepped-animation.md) | 3 | 26 | 20 | 53 |
| [clones](features/clones.md) | 4 | 171 | 77 | 68 |
| [crowd](features/crowd.md) | 4 | 49 | 10 | 10 |
| [roto-tron](features/roto-tron.md) | 5 | 120 | 55 | 226 |
| [capture-sidecar](features/capture-sidecar.md) | 6 | 66 | 58 | 17 |
| [rig](features/rig.md) | 7 | 168 | 56 | 250 |
| [shafts-volumetrics](features/shafts-volumetrics.md) | 7 | 62 | 24 | 104 |
| [tracking](features/tracking.md) | 7 | 9 | 17 | 0 |
| [environment](features/environment.md) | 8 | 98 | 29 | 4 |
| [weather](features/weather.md) | 8 | 58 | 40 | 104 |
| [vcam](features/vcam.md) | 9 | 87 | 43 | 30 |
| [looks](features/looks.md) | 10 | 136 | 128 | 99 |
| [sensor](features/sensor.md) | 10 | 100 | 56 | 60 |

These layers are a review order. A shared file/hunk can serve several layers and
must be split by contract. There is no claim that mechanically applying a layer
compiles; establish source/include/link, settings/UI and shader closure on the
target before its checkpoint.

## Porting order and checkpoint tests

Reuse the methodology and Steps 0–12 in `../MERGE_PLAYBOOK.md` and the creative IN /
pure-perf OUT ruling in `../MERGE_FORK_DIRECTOR_CONSOLE_PLAN.md`, subject to their
new local-suitcase override. On a separately prepared target, take a checkpoint
before each slice; preserve failed experiments before rollback. No branch mutation,
build, commit or push is part of generating this kit.

1. **Baseline and ledger (Step 0).** Resolve unknown ownership and all perf entanglements.
   Checkpoint: XML parses; ledger accounts for every net changed path, including deletions.
2. **Required primitives, common UI and shell (Steps 1–2).** Land helpers, custom widgets,
   history/undo, scroll fit, registrations, kit generator inputs and CMake closure.
   Checkpoint: target compiles/starts with only closed pages registered; resize/scroll/undo work.
3. **Application pages (Step 2).** Land cast/actor bar, actor/path/dance, camera/operator,
   gaze, stepped animation and composition with their controllers and reachable UI.
   Checkpoint: open pages, select/rekey actor, walk/stop, camera release, gaze off and scene round trip.
4. **Clones and crowd (Step 4).** Land spawn/material/attachment isolation and ghost shader.
   Checkpoint: independent clones/crowd survive edits/save/load; user's avatar stays untouched.
5. **Masks and alpha (Step 5).** Land actor masks, Roto/Tron and shared draw-pool adapters.
   Checkpoint: independent enables; feature-off opaque beauty pass remains correct.
6. **Capture/sidecar (Step 6).** Land bridge/ABI/provider, velocity/temporal/motion blur,
   10-bit/color-space dependencies together. Checkpoint: provider provenance and motion validity,
   camera cuts, resize and feature-off; remove governor coupling without losing capture quality.
7. **Rig/tracking/shafts (Steps 7, 9).** Land complete rig UI/model/cues/probes,
   projector tracking, targets, froxel/projvol passes and GLSL. Checkpoint: multiple lights,
   moving subjects, live probes, shadows, resize/login/off path and scene reload.
8. **Weather (Step 8).** Land model/renderer/UI/assets and occlusion convention together.
   Checkpoint: rain under cover/outdoors, splash/lightning, region transition and disable.
9. **Full VCam prism (deferred Step 3).** Adapt/rebuild against target renderer after
   clones/alpha/capture contracts settle. Checkpoint: full matrix in its feature sheet.
10. **Lightbox/looks/sensor and closure (Steps 9–11).** Adapt/rebuild post-processing;
    include exposure, LUT/grade, actor FX, lens presets and save compatibility.
    Checkpoint: chart/exposure comparisons, custom presets, grouped undo and complete looks;
    then all static gates and all feature-sheet in-world tests. Build/in-world acceptance
    on the target requires its own authorized environment; neither was performed here.

## Targets: local Alchemy and LL-derived viewers

For Alchemy at the pinned base, base-owned helpers exist but renderer and app hooks
still need the full fork delta. Prior plans' UPSTREAM-WINS decisions assumed a newer
renderer: **do not discard reverse-Z, UBO, sampler/target or probe changes solely on
that label** at this older local base. Audit which primitive each creative pass needs;
unknown shared renderer hunks stay review candidates rather than perf exclusions.

For an LL-derived client, compare its local implementation to the LL-origin code
at this base. `compatibility.json` distinguishes fork-added dependencies from
`al*`/`fs*`/`bdmerge*` helpers already present at the Alchemy base. A base-owned
Alchemy helper is not assumed present in LL. Port its implementation with its
dependencies, or supply a target-native adapter preserving the observed contract.
Use exact include and qualified-call evidence in `features.json`; it cannot prove
absence in an unseen client. Examples in this tree include the fork kit widgets,
`alscrollfit`, `aluirefresh`, `alpresetlook` and the Director actor-choice helpers;
base-owned dependencies include `indra/newview/alfloaterlightbox.h`,
`indra/newview/alviewermenu.h`, `indra/newview/fsposeranimator.h` and
`indra/llui/fsvirtualtrackpad.h`. For an LL target, port or replace their observed
floater/menu, animation-pose and widget contracts at the recorded consumers;
inspect the dependency table before choosing adapters. Their presence at the Alchemy
base does not establish their presence in another LL-derived client.
Reconcile floater/panel factories, menus/key bindings, build/package manifests,
settings groups, PBR/deferred targets, shadow/depth encodings, shader uniforms and
avatar attachment/lifecycle semantics. Never invoke simulator detach/teleport paths
for local clones. Filename prefixes are not provenance: new `ll*` files can be OWNED.

## Settings and save-data compatibility

`settings.csv` / `.json` enumerate every added/changed/removed declaration in changed
`app_settings/settings*.xml`: type, declared default, Persist metadata, previous default,
feature and candidate scene references. Preserve keys (Alchemy prefixes are identifiers),
types and default semantics. Persist does not prove scene ownership. Scene coverage marks
explicit scene-list members and appended Roto/Tron literal vectors as saved. Other keys
remain unknown: dynamic lists, actor state and structured blocks need semantic review;
a literal reference alone is evidence only. Duplicate declarations remain visible in
settings JSON; the displayed default is the last text occurrence, not a proven runtime policy.

The local scene controller is `indra/newview/llfloaterdirector.cpp`: `SCENE_VERSION`
is 6, `director_scenes` lives under `LL_PATH_USER_SETTINGS`, and scenes use LLSD XML.
Its loader handles legacy versions explicitly (including the unshipped v2 case);
preserve that policy and stable actor IDs. `llactormover.cpp` writes path schema 4
and accepts schemas 1–4; `alpathgeometry.h` supplies path geometry. `alkitpresets.cpp`
uses `director_presets/<family>.xml`; `alcinelightrig.cpp` has version-1 rig/cue blocks
and user directories `cine_light_rig` / `cine_light_rig_cues`. Keep bundled preset assets, escaping rules, recovery
and corrupt-backup policy with the serializer/loader. These are observations of this
local snapshot; when versions change, review this guide's narrative as well as regenerate.

Use each preset family's owned-setting definition. Replace complete looks by defaults
then authored values; preserve targeting, Shot key metadata, calibration, master enables
and compile-time shader settings. Validate before mutation; grouped undo captures the
old values. Old custom presets default missing owned values. User data paths depend on
the target's `gDirUtilp`/channel configuration: copy backups explicitly when migrating
between clients; do not assume two channels share a user-settings directory.

## Shader co-landing and hard spots

Co-land program declaration/registration/loading, all class variants and included GLSL,
reserved uniforms/defines, render-target attachments, feature flags, settings and reachable
UI. A default-off setting is insufficient if startup still calls `createShader()`.
If closure is incomplete, omit registration and source entry until the complete slice
lands, or guard startup creation as well as use. Preserve the target's depth convention,
normal encoding, sampler bindings, color space and target lifecycle explicitly.

Prism/VCam and Lightbox retain the prior plan's REBUILD/adapt warning; unseen targets
need design review, not blind hunk replay. Stage prism after clone/alpha/target support;
stage Lightbox after post/color contracts settle. Rig probes, froxel/projvol, weather,
sidecar and actor masks are renderer-heavy. Check beauty and feature-off paths, GL blend,
color masks, draw buffers and resource release on every experiment. Quality/cost controls
belong to creative features; a name containing “performance” does not make pose polish OUT.
Committed VCam media frame provenance, file admission/close fences and loop-seam helpers
in `indra/llplugin/` are IN dependencies of prism/media playback. The old playbook's
stash-only exclusions do not exclude code that is now committed at this fork HEAD.

Perf severance is mandatory: `llviewertexture.cpp` capture mode calls
`BDMergeMemoryBudget::isCritical()` and capture quality also uses `sQualityOverrideAllowed`.
The prior plan retained the RAM-pressure dependency; this suitcase excludes the pure
budget/pool implementation as a portable feature and marks its callers for adaptation.
Provide the target's equivalent capture-quality/pressure policy or isolate the minimal
required safety contract; never drop the include and leave a missing symbol, and never
silently reintroduce the VRAM governor. This is an explicit IN/OUT difference from the plan.

## Verification and evidence limits

Existing evidence: 2206 wiring rows; page-source summary
`{"ambiguous_rows": 0, "independent_json_rows": 2206, "multi_page_settings": 21, "owner_roots": 35, "pages": 38, "rows": 2206, "unresolved_rows": 59}`.
Functional manifest's recorded wiring CSV SHA matches current bytes:
**True**. Existing manifests can still be stale
against code. `inputs.json` preserves their metadata and hashes; unresolved/ambiguous
rows remain unresolved. They do not establish in-world behavior.

Use the existing generator/check entry points (read `--help` for harness prerequisites):

```powershell
python -I scripts/code_tools/director_kit_gen.py --check
python -I scripts/code_tools/director_page_sources.py --check
python -I scripts/code_tools/director_functional_manifest.py --check
python -I scripts/code_tools/director_parity.py --all
```

`director_wiring_table.py` is a writing generator with its own historical ORIG baseline,
not a `--check` gate; inspect that baseline before rerunning it. The suitcase reads its
existing CSV as evidence. Kit lint, curation/theme/header, actor-bar and light-actors checks complement wiring
and functional metadata. UI layout/draw lint cover only their stated invariants.
Per-feature sheets enumerate actual local scripts, including clone looks, alpha blend,
rig shadow, relight volumetric, sensor and weather GL harnesses. These may require a
GL context, dependencies or compiled fixture hosts; inspect each script before running.
A static/source gate proves its invariant only. GL fixtures prove their test scenes,
not viewer integration. Use every feature sheet's in-world checkpoint and distinguish
PASS / FAIL / INCONCLUSIVE. This kit does not claim a port has been built or run.

The generated hook map covers 498 shared files / 3014 canonical
three-context-line hunks. Function extraction is a conservative lexical scan; XML,
declarations, constructors or complex macros may remain unknown. Local conditional
evidence does not prove a dominating setting guard or an inert off path. Unknown and
mixed/perf-review entries are the manual adjudication queue in `hooks.json` and inventory.
The initial 15+15 deterministic random code spot-checks and exact validation results
are recorded separately in `.tmp/suitcase_20261102/REPORT.md`; that report is not regenerated.

## Licensing and attribution

The repository `LICENSE` is LGPL 2.1. Preserve Linden Lab, Alchemy and other author
notices in each source/shader/asset and carry applicable license texts. Imported FS/BD,
ReShade and vendor files may have their own terms; inspect each notice and redistribute
only what those terms allow. Do not treat the suitcase's class as license clearance or
replace code identifiers during branding. No external license guidance was consulted.
