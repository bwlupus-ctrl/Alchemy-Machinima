# Probe on-demand research at HEAD

Read-only investigation, 2026-09-30. No code edits, build, or commit.
HEAD: `c31f37cc43a47fad052455771811e00cd860f404`, containing Live Probe commit
`615750a40f9`; the HEAD commit adds unrelated environment-intensity presets.
Input: `doc/PROBE_UPDATE_ON_DEMAND_BRIEF.md`, written against `f9ad494`.
All source anchors below were re-derived from HEAD, not copied from the brief.
Paths without a directory prefix are relative to `indra/newview/`.
Code facts are distinguished from proposed design and performance estimates.
The reported approximately 16 ms CPU per cube face is user-supplied Tracy evidence;
this investigation did not run the viewer or independently measure timings.

## 1. Reconciliation with 615750a40f9

**Preserve the reviewed Live Probe implementation; section B is partly implemented,
but its original assumptions no longer describe HEAD.**

| Brief B requirement | HEAD evidence and reconciliation |
|---|---|
| Slice Live Probe faces | Implemented in `llreflectionmapmanager.cpp:513-574,686-743`. Balanced uses 2 faces/frame, Economy 1, On change configurable 1-6; `alcineliveproberefresh.h:477-486,554-556`. |
| Stop when unchanged | On change converges only after clean irradiance and radiance passes agree with current H; `alcineliveproberefresh.h:409-411,561-594`. Balanced/Economy intentionally never idle. |
| Flip passes at face six | `advanceFace` completes at six and records the last pass kind; `alcineliveproberefresh.h:570-594`. Budget capture stops at that boundary, never starts another pass within the same frame; manager `:705-713`. |
| Maintain publication/readiness | Full warm-up uses six faces per frame, setting separate readiness flags after whole passes; manager `:650-668`. Budget radiance completion sets both flags and `mComplete`; `:722-729`. Budget starts only after readiness, unlike the brief's sliced warm-up. |
| Preserve old cube during refresh | Budget capture does not clear completeness/fade on entry; manager `:686-743`. It freezes origin for each six-face pass and restores the following influence origin; `:695-701,738-742`. |
| Handle animated lighting | On change deliberately takes FULL during animation and settling; header `:489-503`. Dispatch includes rig animation and animated non-rig gobos; manager `:525-528`. This can render six faces/frame regardless of ChangeFaces. |
| Correct FULL/budget handoff | Manager resynchronizes `mRealtimeRadiancePass` from the last completed scheduler pass; `:548-557`. Keep this reviewed correction. |
| Origin/radius/environment/light dirty sources | Signature covers probe origin/radius/ambiance/slot, rigs, eligible non-rig lights, sky, water, and capture settings; manager `:750-895`. |
| Arbitrary prim/geometry dirty source | **Missing.** No spatial/object appearance generation is appended by `sampleCinematicH`; `:761-895`. An ordinary prim can change while H remains stable. The watchdog eventually refreshes it. |
| Closest-dynamic slicing without Live Probe | **Missing.** Selection is Live Probe first, otherwise realtime closest dynamic; `:480-501`. Only designated Live enters the scheduler; `:513-514`. Fallback goes to all-six capture; `:583-592,650-652`. |
| Per-probe minimum interval | **Missing.** Params contain mode, face count, watchdog, settle only; header `:361-367`. Settling is an animation tail, not a minimum refresh interval. |
| Zero work forever in a static scene | Not the default: watchdog is 5 seconds; manager `:509,521`, header `:521-526`. Zero means disabled; a watchdog refresh requires a clean pass pair, normally 12 faces. |
| Dynamic-column sources for Live Probe | The brief is wrong: Live Probe creation sets dynamic **false**, `alcinelightrigmanager.cpp:573-576`. Avatar/animesh/particle appearance is not a required dirty source for this static capture; see section 7. |

**Lists and light selection need precise wording.**
- `setCinematicLiveProbe` resets on probe identity change, but merely assigns ignored/pinned
  lists on same-probe calls; `llreflectionmapmanager.cpp:1172-1190`.
- Rig signature samples emitter objects; target-rig omnis/catchlight are represented as
  absent, matching intended exclusion; `alcinelightrig.cpp:1175-1204`.
- Non-rig signature scans all eligible lights, without nearest-N or per-face truncation;
  `pipeline.cpp:9533-9544,9611-9674`. Appearance of a light is broader than geometry dirtying.
- Ignored/pinned predicates are capture-flag gated (`llreflectionmapmanager.cpp:1197-1212`);
  sampling occurs before capture. Do not claim exact list hashing: the signature is a
  conservative selection approximation, and arbitrary same-probe list edits deserve an
  explicit selection generation in the rewrite.

**Scheduler reuse verdict.** Keep `ALCineLiveProbeRefresh::decide` Live-Probe-specific.
- Its FULL warm-up, animation override, singleton identity, watchdog/convergence policy,
  and lack of queue arbitration do not implement ordinary-probe scheduling; header
  `:369-393,452-556`. One state per ordinary probe would not solve scratch ownership.
- Reuse/extract pure signature tolerance helpers and six-face cursor/convergence concepts;
  `alcineliveproberefresh.h:89-219,297-355,561-615`.
- Ordinary probes already have a serialized 12-face transaction in `doProbeUpdate`,
  `llreflectionmapmanager.cpp:1256-1282`; add eligibility and generations around it.
- Live's secondary scratch remains distinct from ordinary primary scratch, `:1335-1339`.
  Identity reset `:1176-1186`, allocation reset `:1969-2019`, cleanup `:2078-2086`,
  and same-probe blocking `:565-573` are invariants to retain.
- Define the disabled path against **HEAD**, including existing Live modes, rather than
  restoring `f9ad494` behavior. ChangeFaces=6 alone does not disable On-change gating.

## 2. Section A dirty hooks and recommended data model

**Recommend batched event push into per-probe generations, not polling GEOM_DIRTY.**
The influence sphere is not the capture footprint: `LLReflectionMap::update` passes
origin/near clip/dynamic state, not radius/far clip, to `cubeSnapshot` (`llreflectionmap.cpp:70-73`).
`cubeSnapshot` changes near/FOV/origin but retains camera far (`llviewerwindow.cpp:6067-6076`).
Ordinary spatial culling uses no-far frustum plus a sphere of `mFrustumCornerDist`,
not simply camera far or probe radius (`llspatialpartition.cpp:1062-1078`;
`llvieweroctree.cpp:1379-1399`). Infinite-far partitions differ (`llspatialpartition.cpp:1442-1455`).
Thus radius-only invalidation misses visible distant geometry. Shadow casters and
light influence also extend beyond the receiver volume; conservative invalidation is required.

### Ranked hook set: coverage versus cost

| Rank / source | Exact HEAD hook candidates | Coverage, cost, and residual misses |
|---|---|---|
| 1: actual spatial movement | `lldrawable.cpp:780-864` updateMove/undamped/damped; `llspatialpartition.cpp:972-1004` move and `:225-240` updateInGroup | Record old/new world bounds after transform/extents changes; coalesce once per drawable/frame. In-node motion does not set GEOM_DIRTY (`:239` is commented out), so insert/remove alone misses it. Bounds-only comparison misses rotation/pose inside unchanged bounds: retain a transform/shape change reason. |
| 1: spatial membership | `llspatialpartition.cpp:774-784` insertion/removal; `:929-964` put/remove | Covers drawable arrival/removal and tree migration, including cached/local objects. Capture removed bounds before release. Misses within-node movement and texture-only changes. |
| 1: geometry/appearance requests | `pipeline.cpp:4517-4522` markTextured; `:4580-4594` drawable markRebuild; `llspatialpartition.h:428` dirtyGeom; `.cpp:503-525` setState | Broad coverage for material/color/shape edits; inexpensive enqueue, potentially noisy due to batching/LOD churn. Do not rely only on a clean-to-dirty flag edge; flags may already be set. |
| 1: geometry becomes drawable | `pipeline.cpp:4178-4180,4334-4337` updateDrawableGeom; `llvovolume.cpp:2157-2255` updateGeometry | Also invalidate when a requested rebuild actually lands, otherwise a probe can capture old geometry and clear its request too early. Deduplicate request/completion where possible, without losing a later publication. |
| 2: fetched texture publication | `llviewertexture.cpp:2107-2118,2121-2151` createTexture/postCreateTexture | Hook main-thread postCreateTexture, traverse referencing faces/channels, enqueue their bounds. Same-UUID sharper mips can change appearance without geometry changes. Fan-out may be large; batch by texture and group. |
| 2: lights | `llvovolume.cpp:4564-4592` parameterChanged; setters `:3131-3178,3182-3276`; `pipeline.cpp:9554-9718` existing Live light sampler | Cover local and received PARAMS_LIGHT/PARAMS_LIGHT_IMAGE changes, on/off, radius/color/falloff/projector. Pair with movement events and light removal. Use old/new light influence or conservative global LIGHT invalidation, not emitter bounds. |
| 2: environment | `llenvironment.cpp:1635-1668` update; manager `sampleCinematicH`, `:777-895` | One sampled environment generation after environment update, shared by all probes. Covers interpolated settings, not only asset replacement; details in section 3. |
| 3: media frame publication | `llviewermedia.cpp:3202-3237` doMediaTexUpdate; `:3077-3094,3253` accelerated texture path | Hook successful main-thread-visible frame publication in both upload and accelerated paths. GPU shared-texture media bypasses setSubImage. Rate-limit scheduling, not loss of dirty events. |
| 3: continuous texture animation | `llvovolume.cpp:616-631,665-691` animateTextures | markTextured occurs on animation start; later texture matrices can change without rebuild. Register active animated surfaces and periodically dirty affected captures. |
| 3: flexi | `llflexibleobject.cpp:351,403,806-815` rebuild requests/state writes | Rebuild hooks cover much flexi, but direct setState writes bypass markRebuild. Actual geometry-publication hook closes that gap. Active flexi still needs bounded recurring updates. |
| 3: particles / pose | `llvopartgroup.cpp:216-240` updateGeometry; `llvoavatar.cpp:1401-1443,2817-2850` extents | Dirty only captures whose render mask includes them. Pose can change with identical bounds; use animation activity/generation, not merely displacement. Cost grows with animated content. |
| 4: object-list fallback | `llviewerobjectlist.cpp:1428-1449,2023-2086` killObject and three creation paths | Useful diagnostic/fallback, but creation precedes final drawable bounds and kill only calls markDead. Not the authoritative spatial hook. Avoid double counting membership events. |

Texture caveat: `addToCreateTexture` calls face dirtyTexture on **component-count changes**,
not on every texture refinement (`llviewertexture.cpp:1910-1929`). `postCreateTexture`
currently rebuilds sculpt users (`:2134-2142`), not all appearance users.
Uploads can run on a worker; the main-thread callback is explicit at `:2204-2219`.
Do not mutate probe/spatial state from `createTexture`'s worker execution.

Alpha/material coverage: `setTETexture` marks textured (`llvovolume.cpp:2355-2366`);
alpha color changes mark textured and volume rebuild (`:2376-2392`);
material/override paths also mark textured/rebuild (`llviewerobject.cpp:5892-5954,7709-7712`).
These cover edits, not every asynchronous material/texture publication; audit each publication
path, retaining safety coverage for uninstrumented PBR material completion and procedural changes.

### Proposed structures and event lifetime

- Manager-owned `ProbeChangeEvent`: monotonic serial, reason mask, capture-class mask,
  old/new world AABBs, region/coordinate epoch, and optional light influence bounds.
  Own the values; never retain an unowned drawable/group pointer after deletion.
- Frame-local coalescer keyed by stable drawable identity or group identity plus lifetime
  generation. Preserve the union of all swept bounds, not just the final position.
- Per-probe record: pending reason mask, dirty generation, requested environment generation,
  transaction-start generations, last completed generations, first-dirty time,
  last-real-complete time, next-eligible time, and allocation/capture-policy generation.
- Maintain separate manager epochs for environment, capture configuration, spatial membership/
  bounds, and render-data replacement. The latter epochs invalidate C's cache exactly;
  they must not inherit tolerance/min-interval filtering from A's visual dirty policy.
- Flush events before ordinary selection and Live H sampling. Intersect against a conservative
  **capture** footprint and actual render classes. For caster dependencies not spatially bounded
  safely, initially invalidate all allocated probes when a shadow-casting object changes.
- Append the selected Live Probe's relevant geometry/appearance generation to its H; retain
  existing lighting/environment fields and animation policy. Static Live excludes avatar geo,
  but attachment **light** changes still matter (`pipeline.cpp:9626-9640`).
- Enforce minimum interval between starts of complete 12-face refresh transactions; do not
  pause individual faces or the irradiance-to-radiance continuation. Changes remain pending.
- On completion acknowledge only generations consumed by that transaction. Changes during
  capture leave a follow-up dirty request; never clear a newer event with a plain bool assignment.
- Budget is manager-wide for ordinary faces, with explicitly separate realtime allowance.
  Keep one ordinary scratch owner until both passes finish; preserve fairness/incomplete priority.
- New allocation, relevance return, probe motion/resizing, capture-mask/settings changes,
  coordinate shift, and reset force freshness evaluation; slot loss invalidates transaction state.
- Bound the event queue. On overflow or lost coordinate history, dirty all affected probes
  conservatively instead of silently dropping events. Deduplicate repeated events for cost control.

### Counters versus pushed dirty generations

| Choice | Benefit | Correctness / cost problem |
|---|---|---|
| Poll existing GEOM_DIRTY | Almost no added storage | Ephemeral rebuild flag, not change history; misses in-node motion and texture/media/light-only changes. |
| Per-node monotonic counters | Cheap writer increments; subtree maxima can prune unchanged regions | Still needs all appearance/light hooks. Removal, shrinking bounds, node destruction, bridge transforms, and tree reparenting require historical coverage. |
| Batched event push, recommended | Old/new bounds survive removal; reason/class information remains available; simple per-probe state | Approximately O(coalesced events x allocated probes) without spatial indexing. Profile this; index probe capture footprints if necessary. |

If counters are later introduced, put render-specific `U64 self/subtree` change serials by
reason class in **LLSpatialGroup**, not indiscriminately in base `LLViewerOctreeGroup`.
The base serves generic octree state (`llvieweroctree.h:178-242`); spatial groups know rendering.
Counters must be independent of `mState` and propagate even while DIRTY is already set.
`unbound` exits early on an already-dirty node (`llvieweroctree.cpp:517-539`).
Removal can destroy the group during `setGroup(NULL)` (`:619-624`), so bump ancestors and
preserve a tombstone/swept-dirty bound before release; a moved-out node otherwise disappears
from the next probe query. A root topology epoch is the conservative fallback.
Bridge-local counters require world-space transformed queries and parent-transform epochs.
This complexity buys little before event fan-out has been measured; do not make it A's first design.

**Safety coverage remains explicit:** unhooked shader-time effects, cloud/water animation,
unobserved texture/material publication, transient pose excursions, distant casters beyond an
incorrect bound, and changes in indirect probe inputs. Safety eventually refreshes current
appearance; it cannot reconstruct a transient already gone. Zero MaxAge accepts these gaps.
Radiance captures consume irradiance (`llreflectionmapmanager.cpp:1291-1297`), so probe
publication is itself a lighting dependency. Track a bounded/coalesced dependency generation;
do not create an endless all-probes-dirty feedback loop on every publication.

## 3. Running day cycle: threshold, demand, and reusable signature

Default day length is **14,400 seconds / four hours**, defined in
`indra/llinventory/llsettingsdaycycle.cpp:107-108`; environment instances adopt it at
`llenvironment.cpp:2713,2820`. For a uniform 360-degree rotation per day:

| Quantity | Calculation / implication |
|---|---|
| Mean angular speed | 360 / 14,400 = 0.025 degrees/second. |
| 0.5-degree threshold | 20 seconds between threshold crossings, approximately. |
| One six-face pass | 6 / 20 = 0.3 faces/second per probe. |
| Complete irradiance + radiance refresh | 12 / 20 = **0.6 faces/second per probe**. |
| P ordinary probes | Approximately 0.6P faces/second from this source alone. At P=100 this consumes a 60-faces/second budget. |
| Existing Live 0.1-degree threshold | Nominally 4 seconds, or 3 faces/second for a 12-face pair, before other signature fields/watchdog. |
| Pure safety refresh at age 60 s | Nominally 12P/60 = 0.2P faces/second; the brief's P/60 estimate counts updates, not faces. |

These are demand estimates, not measured rates or guarantees. EEP interpolates keyframe
rotations with slerp (`indra/llinventory/llsettingssky.cpp:609`); arbitrary tracks need not
rotate uniformly or complete a 360-degree path. Track spans scale with day length
(`llenvironment.cpp:262-289`). A default day length alone does not prove an exact sun speed.
The angle comparison uses strictly greater-than (`alcineliveproberefresh.h:160-182`),
so actual firing occurs just after the threshold at the next sample.

**0.5 degrees does not inherently mean every-frame dirty.** However many probes can saturate
the queue on a running cycle; other color/density/transition changes may fire much sooner.
MinInterval=0.25 seconds caps individual restart frequency, not aggregate demand.
The default probe also has a two-second period (`llreflectionmapmanager.cpp:596-606`),
roughly six faces/second for full pairs if period dominates, subject to scheduling.
Its current rule is a maximum-frequency gate with coverage disabled, and a priority override
with coverage enabled; it is not a universal exact cadence.

**Recommended metric:** compare effective capture inputs with a sticky accepted baseline,
using angular direction deltas, HDR per-channel relative-plus-absolute color tolerance,
field-unit scalar tolerances, and exact resource/configuration generations.
Do not compare only adjacent frames: gradual drift would never cross a small per-frame delta.
Do not equate a settings pointer/asset UUID change with every meaningful environment change.

- Reuse `Signature` / `StickyHash` (`alcineliveproberefresh.h:221-355`) and the sky/water/
  capture-setting schema from `sampleCinematicH` (`llreflectionmapmanager.cpp:777-895`).
- Sample a separate environment signature once per manager step, then publish an environment
  generation. Keep the Live-specific origin/rig/light signature out of the global signature.
- HEAD uses sun/moon 0.1 degrees (`:786-787`), color tolerance 0.5% plus floors (`:788-796`),
  sun/moon disc scales (`:813-816`), water height 0.01 m (`:855`), and transition texture IDs.
- Ordinary probes may use the proposed looser 0.5 degrees without changing reviewed Live
  tolerances. Maintain independent accepted baselines if policy differs.
- Raw blend factor at 0.001 (`:817,846`) can dominate cadence even with nearly identical
  keyframes. Retain it where it drives texture mixing; otherwise compare effective uniforms.
- Cloud scroll advances separately (`llenvironment.cpp:1693-1717`) and is absent from H;
  continuous water/cloud animation needs a declared periodic-refresh policy or safety refresh.
- Track pending generations during a capture; report queue lag separately from source rate.
  A hard MaxAge+MinInterval guarantee is impossible when demanded faces exceed capacity.

## 4. oldestOccluded fake update and freshness accounting

Complete occluded probes enter a separate oldest-occluded selection at
`llreflectionmapmanager.cpp:458-468`, rather than the ordinary oldest-probe branch
at `:469-477`. The exact final block at HEAD (`:622-627`) is:

```cpp
if (oldestOccluded)
{
    // as far as this occluded probe is concerned, an origin/radius update is as good as a full update
    oldestOccluded->autoAdjustOrigin();
    oldestOccluded->mLastUpdateTime = gFrameTimeSeconds;
}
```

There is no face render or completion publication here. Separately, an actual
`LLReflectionMap::update` writes that same timestamp **per face**, before capture
(`llreflectionmap.cpp:52-73`). Neither usage proves a completed fresh cubemap pair.

| Naive extension | Failure | Required behavior |
|---|---|---|
| MaxAge uses mLastUpdateTime | Repeated fake updates can postpone safety indefinitely. | Use a new last-real-complete timestamp, advanced only after radiance face six. |
| Clear dirty after the fake update | Hidden edits are lost; unoccluding can expose stale data. | Preserve pending generations/reasons while hidden. |
| Mark safety dirty but still always exclude occluded probes | Age exceeds MaxAge without service. | Specify whether safety is deferred while occluded or eventually forces a budgeted capture. |
| Clear dirty at transaction end unconditionally | A change received mid-capture disappears. | Acknowledge transaction generations; retain newer pending changes. |

Recommended policy: defer complete occluded probes, preserve dirtiness and real age,
and queue promptly when they become relevant/unoccluded. Keep incomplete warm-up behavior
explicit; the current exclusion only applies when `mComplete` is true.
If strict age refresh while occluded is desired instead, it must compete for the face budget;
it cannot be achieved by updating metadata. State the chosen policy in logs and UI.
Allow `autoAdjustOrigin` maintenance, but motion/resizing must dirty the capture independently.
Keep legacy `mLastUpdateTime` semantics on the disabled path; do not globally repurpose it.
Add last-real-complete at the ordinary radiance completion site (`manager :1273-1277`),
and corresponding Live pass-publication bookkeeping where applicable (`:722-729`).
For diagnostics distinguish real age, pending age, occlusion deferral, and budget starvation.
The brief's single unconditional `PROBE-ONDEMAND OK` age verdict otherwise overclaims.

## 5. Section C: shared cull, exactness, isolation, and likely saving

### What runs for each face now

| Stage | HEAD evidence |
|---|---|
| Install square 90-degree face camera; disable occlusion | `llviewerwindow.cpp:6060-6076,6126-6163`. |
| Update camera/environment, set CAMERA_WORLD, cull scene | `llviewerdisplay.cpp:1290-1307`. Each call uses a static LLCullResult which updateCull clears. |
| Walk all enabled region partitions and VO cache | `pipeline.cpp:3998-4024`; sky drawables separately added at `:4027-4043`. |
| Generate face-specific sun shadows | `llviewerdisplay.cpp:1312`; `pipeline.cpp:25635-25645`. |
| Two probe cascades, subject to shadow enable/sun state | `pipeline.cpp:25975-25990,26081`. Each normally calls renderShadow with its own cull at `:26710-26714`; cull/sort at `:25140-25148`. |
| Sort scene and render deferred geometry/lighting | `llviewerdisplay.cpp:1316-1318,1353-1357`. |
| Clear render references and restore camera/view state | `llviewerdisplay.cpp:1364-1365`; `llviewerwindow.cpp:6176-6196`. |

The brief's old `pipeline.cpp:22713` anchor is now **:25990**: main-view union shadow
culling explicitly requires `!gCubeSnapshot`. Its helper is `bucketShadowCull` at
`:25592-25632`, and its union walk at `:26059-26075`. Do not remove that exclusion.
With two active cascades, six faces ordinarily mean six scene culls plus twelve shadow
culls. Sharing only the scene cull changes 18 walks to 13, not 18 to 1.

### Recommended first implementation: probe-owned candidate cache

- Add a capture context owned by LLReflectionMapManager's active transaction, not by
  `display_cube_face`'s static result or the pipeline-global `sCull`.
- Cache a conservative, ordered hierarchy of candidate groups and bridge roots, with
  lifetime-validated handles. Do not cache final LLDrawInfo pointers/render maps across frames.
  Rebuilds replace shared draw data (`pipeline.cpp:5080-5135`); removal may destroy groups.
- Cache key: probe identity, cube allocation epoch, pass identity, capture origin,
  near/far/frustum-corner extent, projection, enabled render classes, clipping/water state,
  coordinate epoch, and spatial/topology/render-data epochs.
- Use actual union coverage of six face culling predicates. A sphere of **camera far**
  alone is not proven sufficient: ordinary culling uses `mFrustumCornerDist` and some
  partitions have infinite-far behavior (`llspatialpartition.cpp:1442-1455`).
- Keep sky and VO-cache behavior explicit. Broad candidate collection should not mark
  every candidate visible or issue cache requests as if visible in the current face.
  Preserve the per-face effects of `updateCull`, or retain its relevant subpaths unchanged.
- For every face, filter candidates with the original partition predicates, bridge
  traversal/transform rules, render mask, clipping, and ordering; build a fresh face result.
  Run existing stateSort, shadow generation, geometry and lighting for that face.
- `LLSpatialBridge::setVisible` has its own frustum/sphere/pixel-area tests and then marks
  its child tree, `lldrawable.cpp:1591-1635`. Blindly filtering every bridge child against
  a face would change existing behavior; mirror current traversal semantics.
- Invalidate before reuse when scene membership/bounds or render data changes. Appearance-only
  changes may eventually spare membership-cache rebuilds, but the first version can be conservative.
- Invalidate/recollect rather than restart an entire pass on every change: restarting can
  starve continuously changing probes. Each face must use current scene data as today.
- Ordinary passes span six frames at budget one; a cached snapshot is not pixel-identical
  if an object enters/moves/leaves between faces. For a changing scene the cache may provide
  little reuse. Existing Live freezes origin, not the world (`manager :695-701`).

### Pixel-identity argument and its limits

Identity is plausible for **candidate traversal reuse**, not for reusing completed render lists.
It requires the candidate set to contain everything the original traversal could accept;
per-face acceptance, bridge special cases, ordering, clipping and side effects must remain equal.
If that precondition cannot be proved for a partition, keep its original face cull.
Preserve insertion/traversal order where sorting has equal keys; alpha output can depend on it.
Current alpha distance updates deliberately skip cube captures (`pipeline.cpp:5162-5172`);
"fixing" that behavior inside C would violate the output-identical scope.
The shadow helper passes individual drawables/bridges through unfiltered (`:25622-25631`);
its conservative shadow behavior is not a proof that copying it yields identical color faces.

**Do not reuse a color-sphere result as the shadow-caster set.** A caster outside the color
volume can shadow a receiver inside it. Shadow traversal uses a different frustum predicate
(`llspatialpartition.cpp:1112-1126,1442-1445`). A later shadow-candidate cache needs its own
union of actual caster domains and per-cascade filtering; keep per-face shadow maps unchanged.
One color cull cannot honestly be advertised as one cull for the whole 6-face render.

### State that must remain isolated

| State / consumer | Required boundary and HEAD evidence |
|---|---|
| Main scene cull | `sCull` is a global pointer set/cleared by grabReferences/clearReferences (`pipeline.cpp:608,3689-3697`). Never leave it pointing to cached probe storage; restore the face result after nested shadow work. |
| Main-view visibility, LOD and occlusion | Group visibility is mutated by markNotCulled (`:4047-4058`), while distance/LOD updates are guarded by !gCubeSnapshot (`:4646,4807`). Candidate discovery must not widen those side effects. Preserve zero probe occlusion. |
| Main-view shadow union | Leave `sUnionShadowResult` and !gCubeSnapshot gate at `:25988-25990` untouched. Separate any future probe-shadow cache and its camera IDs/matrices. |
| Hero probes | Hero update calls the same LLReflectionMap::update/cubeSnapshot chain (`llheroprobemanager.cpp:295-305`). gCubeSnapshot alone includes hero renders. Require an explicit Ordinary/Live manager-owned context, absent for Hero. |
| VCam / Prism feeds | Auxiliary render uses sPrismLensRender and private RT scopes (`llprismlens.cpp:6284-6327,7249-7258`). Require the probe context and exclude auxiliary scope; never share result lists or cache ownership. |
| Camera/RT/uniform state | Preserve camera, GL matrices, viewport, user clip, render mask, sUnderWaterRender, sCurCameraID, shadow flags/matrices/targets, mRT and active radiance pass. Existing scopes are at window :6060-6196 and manager :1298-1333. |
| Light selection | Preserve the Live nearby-light save/restore (`pipeline.cpp:10321-10344`) and per-face selection; no cached light list borrowed from main/auxiliary views. |

### CPU estimate from the supplied 16 ms/face

Let c be baseline CPU time for the **reusable scene traversal alone**, u the union-build
cost per six-face pass, and b the extra per-face filtering cost. Saving per rendered face is
approximately `c - u/6 - b`; only if u is approximately c is it `5c/6 - b`.
An all-directions union may cost more than one face cull, so that approximation is optimistic.
If reusable traversal is 10%, 25%, or 50% of the reported 16 ms, idealized savings before
filtering are respectively **1.33, 3.33, or 6.67 ms per rendered face**.
These are scenarios, not measured estimates. Saving 13.33 ms assumes all 16 ms is shareable,
which the call chain disproves. GPU/shadow rendering, sorting, environment and lighting remain.
At one face/frame these are per-frame savings while active; on-demand idle frames save no
additional cull time. Measure scene cull, shadow culls, sorting, union and filtering separately.

## 6. Section D: safe detail subset and why LOD bias is separate work

**Recommend optional conservative tiny-batch culling first; defer true probe-only LOD bias.**
Both controls default to identity. Gate on an explicit Ordinary/Live capture context plus
gCubeSnapshot, not gCubeSnapshot alone, because hero captures use the same flag.
Main-view shadows, hero, VCam/Prism and other auxiliary renders must see no detail override.

| Candidate location | Safe use / constraint |
|---|---|
| Probe candidate-to-face filtering | Reject an entirely tiny group/bridge conservatively using probe-local bounds and final probe resolution. Keep near-plane intersections/uncertain bounds. This saves submission work without shared geometry mutation. |
| `pipeline.cpp:5123-5135` postSort draw-info insertion | A scoped filter may omit an entire LLDrawInfo from the face's result. It cannot omit arbitrary member drawables from an already batched draw call. |
| Existing pixel-area helpers | Do not overwrite shared group/face pixel area or reuse main-camera estimates. Group mPixelArea is stored by `llspatialpartition.cpp:632-635`; face caches are exposed in `llface.h:116-118,241-247`. |
| `LLPipeline::calcPixelArea` | LLVector3 overload asserts !gCubeSnapshot (`pipeline.cpp:3643-3645`); both overloads depend on LLDrawable::sCurPixelAngle and distance heuristics (`:3658-3686`). Implement a pure probe-resolution estimate instead. |
| `LLVOVolume::computeLODDetail` | Pure selection math at `llvovolume.cpp:1429` can inform a separate draw representation, but calling normal updateLOD is not scoped. |
| `LLVOVolume::calcLOD/updateLOD` | calcLOD changes persistent mLOD (`:1688-1693`), updateLOD schedules shared volume rebuilding (`:1699-1722`). A temporary global factor or save/restore of mLOD does not undo geometry/queue mutations. |

**Tiny-object qualification:** ordinary geometry is often rendered by spatial-group draw maps,
not one draw call per drawable (`pipeline.cpp:5101-5135`; `llspatialpartition.h:68-95`).
A drawable-level early return in stateSort therefore does not reliably remove that object
from cached batches. Safe first scope is whole-group/whole-batch rejection with conservative
aggregate bounds; exact per-object rejection requires separate batch ranges or probe draw maps.
Describe the first UI accurately; do not promise arbitrary tiny-object omission if only batches
can be omitted. Keep alpha batches intact unless their whole bound passes the same reject test.

Calculate projected footprint against the **final** probe resolution; capture rendering is
supersampled and downsampled (`llreflectionmapmanager.cpp:1358-1384`). Use an upper bound
near cube seams/near clip so uncertainty retains an object. With threshold zero, bypass the
new filter entirely. Keep terrain/sky/water and uncertain/emissive cases initially exempt.
For the safe subset, leave shadow-caster selection unchanged: a tiny object can cast a large
shadow. Reducing probe shadow detail is a distinct optional approximation, not part of C.

True lower LOD requires capture-owned geometry/vertex/index buffers, face/draw-info layout,
material bindings and lifetime management, keyed by source geometry revision and chosen LOD.
Current probe paths explicitly avoid geometry/LOD work: `pipeline.cpp:4309-4312,4807-4820,
5080-5099`. Reusing the main object's mutable volume defeats that separation and can churn
main-view LOD, streaming and rebuild queues. Defer `RenderProbeLODBias` unless that separate
representation is designed and reviewed; a cosmetic setting that cannot work safely should not ship.

**Lights must not follow emitter geometry culling.** A tiny or invisible emitter can illuminate
large visible surfaces. Live's signature intentionally ignores influence radius (`pipeline.cpp:9559`)
and retains eligible lights without per-face/nearest-N truncation (`:9539-9544,9647-9668`).
Keep that superset when emitter geometry is culled; otherwise On change can miss lighting edits.
If an explicit future light-detail policy changes selection, hash its configuration and candidate
membership using the same selection contract. Ordinary-probe LIGHT events likewise survive
geometry culling. Conservative extra dirtying is preferable to missing an illuminating light.

## 7. Avatar extents, dynamic captures, and the Live Probe

| Probe / capture | Avatar, control-avatar/animesh, particle policy |
|---|---|
| Ordinary automatic/static probe | Normally excluded: getIsDynamic returns false without an eligible flagged viewer object (`llreflectionmap.cpp:241-252`). |
| Object probe flagged dynamic | Included only when RenderReflectionProbeDetail > STATIC_ONLY and object is alive with volume; same function. Inclusion still obeys render masks and ordinary visibility filters. |
| Closest-dynamic realtime fallback | Selected through getIsDynamic (`llreflectionmapmanager.cpp:484-489`); therefore subject to dynamic-column dirty sources. |
| Designated Live Probe | **Static.** Manager creates it with setReflectionProbeIsDynamic(false), `alcinelightrigmanager.cpp:575`; full/budget capture does not pass force_dynamic=true (`llreflectionmapmanager.cpp:652,709`). |
| Default sky probe | Additionally restricted to sky/water/clouds/terrain by manager `:1313-1325`; avatar motion should not dirty it. |
| Hero probe | Its separate policy can force dynamic based on hero object flag and detail (`llheroprobemanager.cpp:266-272,303`). Preserve it outside A/C/D. |

`LLReflectionMap::update` uses `force_dynamic || getIsDynamic()` (`llreflectionmap.cpp:71`).
`cubeSnapshot` disables AVATAR, CONTROL_AV and PARTICLES when false
(`llviewerwindow.cpp:6090-6108`). Attachment/animesh bridges carry those render classes
(`lldrawable.cpp:1242-1252,1835-1842`) and test the render mask at `:1541-1544`.
This is a render-class distinction, not a rule that every moving prim is "dynamic":
ordinary moving/flexi geometry can still affect static probes.
Attachment lights are selected independently and can affect Live even when wearer geometry
is excluded (`pipeline.cpp:9626-9640`). Classify light and geometry events separately.

**The lag question is real, but the brief overstates the frozen-bounds interval.**
- Full extents are computed from pelvis, mesh joint world translations, padding and attachment
  geometry; `llvoavatar.cpp:1447-1517`. Control avatars skip the polymesh joint loop because
  unused joints can produce incorrect bounds (`:1455-1458,1485-1489`).
- Full recomputation is staggered by `upd_freq = ((avatarCount-1)/maxPerBatch+1)*period`,
  integer arithmetic, `llvoavatar.cpp:2817-2850`.
- Defaults are period=4 frames, maxPerBatch=5 (`app_settings/settings.xml:11518-11538`):
  1-5 avatars => 4 frames, 10 => 8, 25 => 20; at 60 FPS approximately 67/133/333 ms.
- Between full refreshes, `updateSpatialExtents` translates cached bounds by **pelvis world
  displacement** (`llvoavatar.cpp:1416-1422`). It does not simply wait for the next batch.
- Therefore animation-driven pelvis travel can move the box despite stationary agent/root
  position. A hook only on full calculateSpatialExtents would miss these intermediate shifts.
- Limbs can enter/leave a capture region and return before a full extent recomputation;
  pose/shading can change without changing any box. A 0.1*probe-radius displacement threshold
  alone cannot cover dynamic appearance. Lower frame rates widen the sampling gap.

Recommended dynamic policy: record swept old/new extents on every actual extent update,
plus a rate-limited active-pose/particle dirty generation for captures that draw those classes.
Use a conservative motion margin or explicitly faster extent sampling only near relevant
dynamic captures if measured misses warrant the cost. Do not globally force full joint bounds
every frame for this feature. Safety refresh repairs a persistent stale appearance, not an
already vanished excursion. The static Live Probe needs target-origin movement and geometry/
light changes, not AV dirtying merely because its followed subject animates.

## 8. Recommended architecture, implementation order, and risks

**A:** one manager event coalescer, per-probe dirty generations and completed-capture clocks,
one environment generation per tolerance policy, and the existing serialized ordinary capture.
Extend Live H with relevant appearance/geometry generations while retaining its reviewed policy.
Keep closest-dynamic slicing an explicit B extension with its own ownership/reset decisions.
**C:** a separate probe-owned candidate hierarchy, exact face filtering and epoch invalidation;
retain original per-face shadow culls first. **D:** default-off conservative tiny-batch rejection;
defer mutable-volume LOD bias until capture-owned geometry exists.

### Ordered implementation steps for the design rewrite

1. Define policy contracts before implementation: capture footprint versus influence volume,
   dynamic-class filtering, occlusion deferral, minimum-interval meaning, global/realtime budgets,
   and whether age measures last completed pair or last converged pair. Preserve HEAD off paths.
2. Add real-completion timestamps, dirty/requested/consumed generations, reason masks and
   transaction identity. Retain `check_priority` fairness (`llreflectionmapmanager.cpp:200-206`)
   within eligible probes, with dirty-age/starvation observability rather than impossible deadlines.
3. Add cheap batched movement/membership/geometry-publication and appearance hooks from section 2;
   use deletion-safe world bounds and conservative overflow/caster fallbacks. Initially log events
   and eligibility without changing rendering, so missed and excessive invalidations are visible.
4. Extract reusable environment sampling/tolerance helpers, preserving Live schema and behavior.
   Add texture/media publication and local-light hooks; enumerate safety-only sources explicitly.
5. Gate ordinary selection by dirty state, due safety, interval and relevance; finish in-flight
   pairs under budget. Acknowledge only consumed generations and retain follow-up changes.
   Integrate default-probe period policy without an unconditional priority bypass of the new budget.
6. Append relevant scene generations to Live H. Add closest-dynamic slicing separately, without
   copying Live's static render mask, rig animation override or readiness shortcuts into it.
   Resolve primary/secondary scratch conflicts, realtime identity changes and warm-up accounting.
7. Introduce explicit capture context/ownership for Ordinary, Live, Hero and auxiliary views.
   Add C's candidate cache behind an independent switch; preserve current culling for unproven
   partition cases. Add counters for cache hits, invalidations, union cost and rebucket cost.
8. Validate static pixel identity for C before enabling it by default. Compare all six faces,
   both pass types, clipping/water, alpha, bridges and off-axis shadow casters. In changing scenes
   compare per-face accepted sets/order against the uncached path at the same scene generation.
9. Add D's conservative batch filter behind zero/identity defaults; separately validate main view,
   main shadows, Hero, VCam/Prism and Live light selection. Do not hide a global LOD mutation in D.
10. Add settings/UI/preset/reset integration and honest telemetry: rendered faces, completed pairs,
    dirty reasons, real age, pending age, deferred-occluded count, budget pressure, and cache timing.
    Tests/build/in-world A/B belong to implementation; none were run for this research.

### Biggest risks and acceptance boundaries

- **Spatial undercoverage:** influence-radius tests miss captured geometry/lights/casters.
  Conservative over-invalidation is acceptable initially; undisclosed missed dependencies are not.
- **False convergence:** bool dirty clearing loses mid-pass edits; fake timestamps defeat safety;
  publication and source generations must remain distinct from scheduling/visibility metadata.
- **Repeated work:** main-camera LOD churn, animated media, environment transitions and indirect
  probe dependencies can keep probes dirty. Report source demand and queue service separately.
- **Cache lifetime/identity:** raw group/draw-info retention across sliced frames risks stale
  state or use-after-free. Exactness needs topology/lifetime invalidation, not only a dirty H.
- **Budget promises:** 12 faces per ordinary refresh, six-face Live warm-up/animation exceptions,
  the default probe and too many eligible probes invalidate simplistic faces/s or max-age verdicts.
- **Hidden state leakage:** gCubeSnapshot includes Hero; cached culls, visibility stamps, light
  lists, shared LOD/pixel-area fields and shadow state are all broader than one capture.
- **Unproven performance:** 16 ms cubeSnapshot is an inclusive cost, not cull time. A should
  deliver the dominant static-set saving; C must earn its complexity with measured traversal savings.

Research verification: all source references were inspected against the current tracked code;
`git diff --name-only HEAD -- indra doc/LIVE_PROBE_REFRESH_DESIGN.md` was empty during research.
The existing working-tree extras were left alone. This document is the only authorized write.
