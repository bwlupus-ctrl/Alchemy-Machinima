# Ghost Studio — Clone Transform, Manipulation, Grouping & Rendering Architecture

**A top-down deep-research map for improving the system, with supporting code.**

Status: written 2026-07-26 against the *uncommitted* working tree (HEAD `57b811abfc4`). Line
numbers are approximate (`~`) because the tree is in flux; treat them as anchors, not addresses.
Everything below was verified while debugging the four regressions of 2026-07-26 night
(entity-scale TDR, overlay-scale TDR, crowd-preview blank, overlay-manip loss).

---

## 0. Why this document exists

Tonight four separate regressions all landed in the **same subsystem**: the code that decides
*where a clone is, how big it is, which way it faces, and how you edit that in-world*. They kept
slipping through because that logic is **spread across ~8 files, forked by clone kind, and recently
unified into one `transformUnit` chokepoint** without an invariant to hold it together. This paper
maps the system top-down so it can be improved deliberately instead of patched reactively.

The single most important takeaway is in **§4** and **§13**: there are **two clone kinds with two
different transform authorities**, a **unified `transformUnit`** that was retrofitted over both, and
**no enforced invariant** on the one value (`mRotation`) that turned out to be able to explode. Fix
the *shape* of the transform authority and most of this class of bug becomes unrepresentable.

---

## 1. Mental model

Ghost Studio makes **client-only copies ("clones"/"ghosts") of avatars** for machinima: place them,
pose them, style them, and multiply them into crowds — all local, none of it touches the grid.

Every clone is one **`Instance`** record (§3). An instance has exactly one of two **backings**:

| Backing | What it *is* | Transform authority | Cost | Fidelity |
|---|---|---|---|---|
| **Overlay** (`BACKING_OVERLAY`) | Not an object. A per-frame **re-draw of the source avatar's harvested rigged batches** at a new transform. | A GL modelview `T·R·S·T(-pivot)` composed at draw time (§6). | Cheap | Diffuse-only / stylizable |
| **Entity** (`BACKING_ENTITY_CLONE`) | A **real `LLVOAvatar`** (`LLGhostAvatar`) with a cloned skeleton + attachments. | A **post-skin "outer transform"** matrix (`LLClientOuterTransform`) applied during render (§7). | Heavy | Full deferred/PBR |

These two paths **share the Instance model and the UI**, but **diverge completely at render and at
transform application**. That divergence is the root theme of every section below.

---

## 2. Component map

| File | Role |
|---|---|
| `alghoststudio.{h,cpp}` | **The brain.** `ALGhostStudio` singleton, the `Instance` model, spawn/lifecycle, **`transformUnit`** (the unified move/rotate/scale), groups/lock modes, formations, entity-runtime bridge. |
| `llghostavatar.{h,cpp}` | **The Entity backing.** A real `LLVOAvatar` subclass: cloned attachments, drive modes, the **outer transform** (`updateEntityOuterTransform`), `setEntityScale`, `setGhostPosition/Rotation`. |
| `llactormover.{h,cpp}` | **The Overlay backing + previews.** `drawGeometryGhost` (the diffuse re-draw), the **deferred proxy** builder + `computeProxyBounds`, `renderHeadingPreview`, and the harvested-batch collectors. Also the actor-pathing/gait system that shares `setAnimTimeFactor`. |
| `alpanelghoststudio.{h,cpp}` | **The UI.** Instances list, Place/Pose/Style/Crowd tabs, edit-mode toggle, scale spinner, and the **formation-preview publisher** (`refreshFormationPreview` → `setFormationPreview`). |
| `alghostmanipproxy.{h,cpp}` | **The in-world gizmo.** A throwaway proxy object the stock LL manipulators drag; `pullProxyToInstance` (drag→instance) + `endDrag` (commit). |
| `altoolghostplace.{h,cpp}` (`ALToolGhostEdit`) | The **edit tool** that owns selection + hosts the manip proxy. |
| `llclientoutertransform.h` | The tiny **outer-transform struct** (`mCurrent`, `mInverse`, `mFootPivot`, `mScale`, `mRevision`). Shared by an entity clone and its attachments. |
| `lldirectorcast.{h,cpp}` | The **cast**: resolves a `mSource` id (null = "you"/agent) to a live `LLVOAvatar`. |
| Render integration | `llviewerdisplay.cpp` (~1795/1798: preview hooks), `pipeline.cpp` (deferred ghost passes + `ScopedGhostTransform`), `lldrawpool.cpp` (`applyModelMatrix` for the outer transform), `llvoavatar.cpp` (outer-transform-aware extents). |

---

## 3. The data model

### 3.1 `Instance` (`alghoststudio.h` ~151)

The single source of truth per clone. Transform-relevant fields:

```
LLUUID        mId;            // identity (minted at Add)
LLUUID        mSource;        // cast id; null = agent avatar
EBackingKind  mKind;          // BACKING_OVERLAY | BACKING_ENTITY_CLONE
LLUUID        mEntityId;      // weak runtime id of the LLGhostAvatar (entity only)
LLUUID        mGroupId;       // non-null = member of an indivisible locked unit
ELockMode     mLockMode;      // OFF | RIGID_UNIT | MOVE_FACE

LLVector3d    mFootGlobal;    // FOOT position, GLOBAL coords   ← authoritative placement
LLQuaternion  mRotation;      // body yaw/orientation           ← the value that exploded
F32           mScale;         // uniform, pivoted at the foot   ← 0.05..10

U32           mTransformRevision;  // bumped by the setters; drives manip-proxy re-sync
```

Setters (`alghoststudio.h` ~255-280) are the *only* sanctioned mutators and each does
`++mTransformRevision`:

- `setFootGlobal(foot)` — position only.
- `setTransform(foot, rot)` — position + rotation.
- `setScale(scale)` — scale only.

**Key property:** `mFootGlobal` is a **foot** anchor in **global** coords; `mScale` is documented as
"uniform, pivoted at the foot (feet stay planted)". Both backings must honor *foot-pivoted uniform
scale* — but they implement it in totally different places (§6, §7). That is the fragility.

### 3.2 The singleton (`ALGhostStudio`)

Holds `std::vector<Instance> mInstances`, the entity-runtime registry, the formation-preview state
(`mPreview*`), and the group bookkeeping. Public operations of interest:

- Lifecycle: `spawnEntityClone` (~375), `addInstance` (overlay), `removeInstance`, `duplicate`.
- Transform: **`transformUnit`** (~1535) — the unified move/rotate/scale (§5).
- Entity bridge: `applyEntityTransform` (~804), `applyEntityRuntimeState` (~218), `setInstanceAnimSpeed` (~673), `setEntityScale` (on the avatar).
- Groups: `groupMembers` (~1447), `lockGroup` (~1462).
- Formations: `formationSlotAt` (~1840), `formationPreviewSlots`, `setFormationPreview`/`clearFormationPreview` (~2300), `renderFormationPreview` (~2315).

### 3.3 The enums that shape behavior (`alghoststudio.h` ~57-142)

`EBackingKind`, `ELockMode {OFF, RIGID_UNIT, MOVE_FACE}`, `EDriveMode {MIRROR, DIRECTED, FROZEN}`,
`EPose {LIVE, FROZEN}`, `EFormation` (17 shapes), `EFormationFacing` (10 modes), `EMotion`,
`ETurnMode`, `EGhostLook`. The transform core cares mostly about `EBackingKind` and `ELockMode`.

---

## 4. Coordinate spaces & the transform model (the fragile core)

Three spaces appear, and **mixing them is the recurring bug**:

1. **Global** (`LLVector3d`) — `mFootGlobal`. The authoritative placement.
2. **Agent** (`LLVector3`) — what render + the outer-transform pivot use; converted via
   `gAgent.getPosAgentFromGlobal`.
3. **The source avatar's live frame** — the overlay's harvested batches are already skinned into the
   *source's* world position; the overlay transform must carry them from there to the clone spot.

### 4.1 The pivot: foot = root − pelvisToFoot

Both backings scale/rotate about the clone's **feet**, computed as
`root_world_position.z − getPelvisToFoot()`:

- Overlay: `drawGeometryGhost` ~3902-3916.
- Entity: `updateEntityOuterTransform` ~193-206.

**Hazard (fixed tonight):** `getPelvisToFoot()` can be **negative or non-finite** on a non-standard
/ still-loading skeleton. Used raw it puts the pivot *above* the mesh → scaling drives geometry down
through the ground → NaN vertices → **GPU TDR**. The rest of `llactormover.cpp` already clamped this
(`capture_root_above` ~500-503, `llmax(0.f, getPelvisToFoot())`); three pivot sites did not. Fix =
finite-check + `llmax(0.f, p2f)` (no upper cap) + a finite backstop before the GPU sees the matrix.

### 4.2 Pivoted uniform scale

The scale math is `p' = scale·p + (1 − scale)·pivot` (scale about `pivot`). Correct **iff** `pivot`
and `p` are in the same space and `pivot` sits at the feet. Every "clone flew off-screen when I
scaled down" symptom is this equation with a **wrong or drifting pivot** — scaling toward 0 collapses
everything onto `pivot`, so a bad pivot = the collapse point is somewhere bad.

### 4.3 Rotation is the sharp edge

`mRotation` is a quaternion with **no enforced unit-length invariant**. The unified `transformUnit`
composed it with `~old_rot * rotation` where `~` is **conjugate, not inverse** (equal only for a unit
quaternion) and LL quaternion multiply **does not renormalize** (`llquaternion.cpp` ~540). On a
scale-only edit `rotation == old_rot == q`, so the delta was `conj(q)·q = |q|²` and the stored value
was `q·|q|²` → **‖q‖ cubes every spinner tick** → the quaternion→matrix conversion produces a
`|q|²`-scaled *finite* transform that explodes across ~32 attachments → TDR. Fix = **normalize** the
inputs, the delta, and every stored composition (§5). This is the single highest-value invariant to
make permanent (§15).

---

## 5. `transformUnit` — the unified transform authority

`ALGhostStudio::transformUnit(id, foot, rotation, scale)` (`alghoststudio.cpp` ~1535) is the
**one entry point** all move/rotate/scale flows now route through (panel spinner `onScaleCommit`
~1299, the manip-proxy commit `endDrag` ~349, chat commands, formations). It was introduced by the
"group stuff" rework and **replaced a direct `setInstanceScale` that only touched `mScale`.**

Current shape (post-fix):

1. Guard: `anchor` exists, `foot`/`rotation`/`scale` finite (~1538). Capture `old_foot`/`old_rot`.
2. **Normalize** `old_rot` and `requested_rot` (~1551-1553).
3. Compute `clamped_scale` (0.05..10) and `mode = mGroupId.isNull() ? LOCK_OFF : mLockMode`.
4. **`LOCK_OFF` short-circuit (~1560):** a single/ungrouped clone does **direct absolute edits** —
   `setTransform(foot, requested_rot)` + `setScale(clamped_scale)` + (entity: `setEntityScale` +
   `applyEntityTransform`) + `return`. This is the *restored pre-group behavior*: a scale-only edit
   touches only scale and can never move/rotate the clone.
5. **Group path (`LOCK_RIGID_UNIT` / `LOCK_MOVE_FACE`):** normalized `delta_rot = ~old_rot *
   requested_rot`; per-member offset scaled by a member-bounded `eff_ratio` (RIGID only), rotated by
   the delta, and each member's rotation **normalized before and after** composing. RIGID also
   couples every member's scale to `eff_ratio` (the closest-pair anti-vanish/anti-explode clamp,
   ~1592-1620).

**Design read:** step 4 is the important architectural admission — *single clones never needed the
group math.* The unified path is correct for groups and, post-normalize, safe for singles, but it is
**more machinery than the common case warrants** and it is where the fragility concentrated. See §13.

---

## 6. Overlay render path (`drawGeometryGhost`)

`llactormover.cpp` ~3862. Re-renders the source avatar's harvested rigged `LLDrawInfo` batches
(collected earlier in-frame) at the clone transform:

```
modelview = view · T(ghost_foot) · R(mRotation) · S(scale) · T(-pivot)   (~3958-3975)
```

- `pivot = live_root − p2f` (or a frozen/cadence anchor if `gp.mHavePivot`).
- `scale = clamp(gp.mScale, 0.05, 10)`.
- Shaded by the diffuse-only `gActorGhostProgram` (flat color, alpha-cutoff, per-slot filter, FX
  looks) — **no PBR/normal/emissive/scene lighting**.
- A partial **deferred G-buffer submission** exists behind `GhostDeferredEnable` (default **off**) —
  the `computeProxyBounds` + `ScopedGhostTransform` path (`llactormover.cpp` ~5690-5800,
  `pipeline.cpp` ~4930/5210). This path **also** builds a pivot from raw `getPelvisToFoot` (fixed
  tonight) and is the reason the pivot hazard had *three* sites, not one.

Overlay = cheap, faithful-textured-but-fullbright. Good enough for filming; the deferred parity work
is **parked** (turning `GhostDeferredEnable` off is the correct config).

---

## 7. Entity render path & the outer transform

An entity clone is a genuine `LLVOAvatar` (`LLGhostAvatar`), so it renders through the **full
deferred + GLTF-PBR + alpha pipeline for free**. Its *size* is not baked into the skeleton; it is a
**post-skin outer transform** (`LLClientOuterTransform`, `llclientoutertransform.h`):

`updateEntityOuterTransform` (`llghostavatar.cpp` ~185) builds, about the foot pivot:

```
mCurrent[axis][axis] = mEntityScale
mCurrent[VW][axis]   = (1 - mEntityScale) * foot[axis]     // scale about foot
mInverse             = the reciprocal (1/mEntityScale)      // mEntityScale floored at 0.05, safe
```

Consumed **only at draw** — `lldrawpool.cpp` ~726 (`applyModelMatrix`) and a scoped avatar
model-view in `llvoavatar.cpp` ~5542; extents scale about `mFootPivot` (`llvoavatar.cpp` ~1617).
**It never writes back into `mRoot`**, so there is no pivot-feedback loop (a hypothesis explicitly
refuted tonight). `stampEntityOuterTransform` propagates the same struct to all attachments so the
whole outfit scales as one.

Order note: `transformUnit`'s entity branch calls `setEntityScale` (→ `updateEntityOuterTransform`,
which *reads* `mRoot`) **before** `applyEntityTransform` (which *sets* `mRoot` position). Harmless for
scale-only (offset 0), but reordering would be tidier for combined move+scale (§13, low priority).

---

## 8. In-world manipulation — the drag→commit contract

`ALToolGhostEdit` (edit mode) owns selection and hosts an `ALGhostManipProxy`
(`alghostmanipproxy.cpp`), a throwaway object the **stock LL manipulators** (translate/rotate/scale)
actually drag. The proxy mediates between the gizmo and the `Instance`:

- **`beginDrag`** (~320): snapshot `mDragStart{Foot,Rotation,Scale}` for cancel.
- **`pullProxyToInstance`** (~226): every drag tick, translate the proxy's transform onto the
  instance. **Grouped** clones route through `transformUnit` *live* (~258/278) so the unit moves as
  one; **ungrouped** clones are written **directly** (`mFootGlobal`/`mRotation`/`mScale`, ~262/282)
  with **no revision bump** — deliberately, so a re-sync push can't fight the stock manipulator
  mid-drag. Entity stretch returns early (~253: entity scale is outer-transform-only).
- **`endDrag(commit)`** (~332): on commit, `pullProxyToInstance` captures the final values, then
  (**the fix**, ~349) commits once through `transformUnit` and syncs `mSeenRevision`, then
  `pushInstanceToProxy` re-normalizes the gizmo box. On cancel, `transformUnit(dragStart…)` restores.

**The bug (fixed tonight):** ungrouped overlay drags wrote directly during the drag but **never did
the authoritative commit** on release → no revision, so the edit didn't "stick"/re-sync. The
`endDrag` commit closes that. **This contract — "direct during drag, `transformUnit` once on
release" — is subtle and unenforced; it is a prime candidate for the invariant work in §15.**

---

## 9. Groups & lock modes

- `mGroupId` non-null ⟺ the clone belongs to an indivisible unit; `mLockMode` is its behavior.
- `groupMembers(id)` (~1447): returns `[id]` when ungrouped, else all instances sharing `mGroupId`.
  (`mode == LOCK_OFF ⟺ mGroupId null`, because unlock always clears both together.)
- `lockGroup(ids, mode)` (~1462): dissolves overlapping old units, mints a group id, sets the mode.
- **RIGID_UNIT**: move/rotate/uniform-scale as one. `transformUnit`'s `eff_ratio` block (~1592-1620)
  bounds the single scale ratio so no member leaves 0.05..10 *and* the closest non-coincident pair
  never shrinks below 0.1 m (anti-vanish) — an all-pairs, member-bounded clamp.
- **MOVE_FACE**: members move/face together but keep independent scale.
- UI: Crowd tab `lock_mode_combo`, `btn_group_unit` ("Group"), `btn_ungroup_unit`, plus
  `To actor`/`To me`/`Drop`/`Align feet` group-dedup helpers.

---

## 10. Crowd & formations

- `formationSlotAt` (~1840) is the **single geometry source** for all 17 formations; returns a slot
  foot + facing given `(index, count, spacing, formation, parameter, options)`.
- `formationPreviewSlots` fans it out to a slot vector; `makeArray`/spawn uses the *same* function so
  preview and result cannot disagree.
- **Preview publish (the regression surface):** the panel's `refreshFormationPreview`
  (`alpanelghoststudio.cpp` ~1864) reads the **selected instance** + crowd controls and calls
  `ALGhostStudio::setFormationPreview(...)` (or `clearFormationPreview` when nothing is selected).
  `renderFormationPreview` (~2315) draws stalks/discs/arrows each frame, **gated** on: setting on, an
  operator floater visible, `mPreviewProto` non-null, `mPreviewCount ≥ 2`, and ≥2 slots. Hook:
  `llviewerdisplay.cpp` ~1798.
- **The bug (fixed tonight):** `refreshFormationPreview` was wired only to the crowd-control
  callbacks, **not to selection change**, so selecting a clone never republished `mPreviewProto` →
  the `isNull` gate killed the draw. Fix = call it on selection change too (~493, ~1040).

---

## 11. Animation & drive modes

- Entity clones animate on their **own** motion controller (`llghostavatar.cpp` `idleUpdate` ~1308).
  `DRIVE_MIRROR` mirrors the source's signaled animations; `DRIVE_DIRECTED` plays one asset;
  `DRIVE_FROZEN` holds. Overlays have no controller — they re-skin the source's live palette.
- **Speed:** `update_time += delta_time · mTimeFactor · sGlobalTimeFactor` (`llmotioncontroller.cpp`
  ~871). Clone `mTimeFactor` = `mAnimSpeed · chaos_factor` (default 1.0); a normal avatar's default
  is also 1.0; `sGlobalTimeFactor` scales **both** equally. So a clone has **no built-in speed
  offset** — any perceived difference is loop **phase** (the clone starts its mirrored loop locally)
  or a source that is itself off 1.0 (your own agent avatar hits 0.2 in some states; mover-driven
  actors set factor from gait). The temporal system (`llpresentationtime.cpp`) drives
  `sGlobalTimeFactor` and must capture/restore the prior value, not blindly reset to 1.0.

---

## 12. In-world data-representation layer — path lines, crowd markers, and unifying them

This is the substrate for the **unified "represent data in-world" UI**. Today it is two separate
renderers with a *convergent idiom but duplicated primitives* — the ideal thing to unify.

### 12.1 What draws in the 3D-UI pass

Both editing overlays are drawn back-to-back in `render_ui_3d` (`llviewerdisplay.cpp` ~1795/1798),
each gated on its own setting + an operator floater, each a client-only overlay:

- **Path lines — `LLActorMover::renderHeadingPreview()`** (`llactormover.cpp` ~4747). Draws the
  heading/distance indicator, the **Catmull-Rom actor path sampled into a ground ribbon**, direction
  **chevrons** along it, numbered **node markers**, start/end role colors, a frame-clock **pulse** on
  the selected node, optional **onion-skin pose ghosts**, and enrolled **prop object-paths**. Per-path
  hue via `actorPathColor` so multiple paths stay distinct. Gated on `ActorMoverShowHeading` + a
  floater (`actor_mover`/`director`/`prop_mover`).
- **Crowd markers — `ALGhostStudio::renderFormationPreview()`** (`alghoststudio.cpp` ~2315). Draws a
  dashed **enclosing perimeter** (~2410), per-slot translucent **filled disc** + **dual-tone rings**
  (dark outer ring for contrast, ~2427-2438), a vertical **stalk** (~2442), a **facing arrow**
  (~2446-2458), and a seven-segment **1..N world label** (~2461) — all from the *same*
  `formationSlotAt` slots the spawn will use, so preview and result cannot disagree.

### 12.2 The curve math (shared by draw and traversal)

The path is a **centripetal Catmull-Rom** (Barry-Goldman form, α=0.5), `llactormover.cpp` ~118 — the
*same* evaluator that path *traversal* walks (~1901/2206). So the drawn ribbon and the walked path are
guaranteed identical; the ribbon is just the curve sampled to a polyline and extruded to a ground
quad-strip.

### 12.3 The already-convergent idiom

Both renderers independently share: `LLGLSUIDefault` + `gUIProgram` bind, texture unbound, **no depth
write** (reads over any ground), **immediate-mode `gGL`** (`color4fv` + `begin/end`
LINES/TRIANGLES + `vertex3f`), a small **ground lift** (`PATH_RIBBON_LIFT` 0.08 / formation `LIFT`
0.06), **dark-outlined** fills for contrast over sand/snow, **camera-billboarded** numeric labels, and
frame-clock pulses (never `Math::random`).

### 12.4 The problem: N primitive sets, no shared layer

`renderHeadingPreview` draws through a **helper family** (ribbon / disc / stick / digit / chevron /
arrow) at `llactormover.cpp` ~3134-3560. `renderFormationPreview` **re-implements** ring / disc /
stalk / arrow / label **inline**. The gizmo/selection (`alghostmanipproxy`) and `renderObjectBeacons`
are further one-off overlay drawers. So the same disc, the same dark-outlined ring, the same
seven-seg digit, the same billboard label live in 3+ copies, each with its own color/lift/contrast
constants and — critically — its **own gating + publish path** (which is exactly how the crowd
preview could silently stop drawing, §10).

### 12.5 Proposed unified representation UI (`GhostViz`)

One in-world representation layer, landing independently of the transform refactor:

1. **A primitive library** — one home for `polyline`/`curve` (the Catmull sampler), `ribbon` (ground
   quad-strip along a polyline), `disc`, `ring` (dark-outline option), `stalk`, `arrow`/`chevron`,
   `billboardLabel`, `sevenSegDigit`. Immediate-mode is fine; the win is *one* implementation + *one*
   style. Extract it from `llactormover.cpp` ~3134-3560 (it is already the richest set).
2. **Style tokens** — a small `VizStyle { hue, groundLift, outlineColor, alpha, pulsePhase,
   distanceFade }`, resolved per producer (per-actor / per-instance / per-role). Theme once (e.g. a
   color-blind-safe palette, fade-with-distance) and every overlay follows.
3. **A representation registry** — each frame, producers submit lightweight **descriptors**
   ("ribbon along these points in hue H", "disc+stalk+label at P facing Y") to one `GhostVizPass`.
   The pass owns the `LLGLSUIDefault`/`gUIProgram`/no-depth setup **once** and the visibility gate
   **once**. `renderHeadingPreview` and `renderFormationPreview` become *descriptor producers*, not
   renderers.
4. **One hook** — replace the two `render_ui_3d` calls with a single `GhostViz::instance().render()`.
   "Is the overlay even on?" becomes one testable question — closing the class of §10's blank-preview
   bug by construction.

Staging: (a) extract the primitive library + `VizStyle`; (b) port `renderFormationPreview` onto it
(delete the inline copies); (c) fold in the gizmo + beacons; (d) collapse to one hook + one gate.
This is the concrete substrate for the unified UI — path lines, crowd staging, gizmos, and future
overlays (paths-through-terrain, look-at cones, camera frusta) all speaking one visual language.

---

## 13. Fragility analysis — why regressions keep landing

Five structural patterns produced tonight's four bugs. Each is a *shape* problem, not a typo:

1. **Two transform authorities, one Instance.** Overlay (draw-time modelview) and Entity (outer
   matrix) implement "foot-pivoted uniform scale" in different files. Any placement rule (pivot,
   clamp, finite-guard) must be re-derived in *both*, and one always gets forgotten — the pivot
   hazard lived in **three** sites, the last one (deferred) unguarded until Codex found it.
2. **A retrofitted chokepoint with no invariant.** `transformUnit` unified move/rotate/scale but
   assumed `mRotation` was unit-length and `~q` was an inverse. Nothing *enforced* unit-length, so a
   scale edit could mutate rotation and cube it. The chokepoint concentrated power *and* fragility.
3. **Unenforced multi-step contracts.** The manip proxy's "direct during drag, commit once on
   release" and the preview's "republish on every state change" are conventions living in comments,
   not types. Both broke by omission (a missing commit, a missing republish).
4. **Coordinate-space mixing.** Global feet, agent-space pivots, source-frame batches — correct only
   by discipline, checked nowhere.
5. **Render correctness is in-world-only.** A Codex "PASS" cannot see a TDR. The feedback loop is
   *build → relaunch → drag a slider → maybe crash*, so bugs are found by the user, late, one at a
   time. Tonight's whole session was that loop.

---

## 14. Top-down improvement proposal

Target: make the *shape* of the system such that tonight's bug classes are **unrepresentable**.

**A. One transform authority, kind-polymorphic at the leaf.**
Make `transformUnit` (or a renamed `applyTransform`) the *only* writer, and give each backing a thin
`applyResolvedTransform(foot, rot, scale)` that it cannot bypass. The pivot/clamp/finite rules live
**once**, above the fork; the backing only knows how to *paint* the resolved transform (overlay =
compose modelview; entity = write outer matrix). No placement math below the fork.

**B. A `UnitQuat` (or a normalize-on-write `mRotation` setter).**
Never store a non-unit quaternion. Either wrap rotation in a type whose only constructor normalizes,
or make `setTransform`/`setRotation` normalize on write. Then §4.3 is impossible regardless of who
composes what. (Tonight's fix normalizes at the call site; this makes it structural.)

**C. Split single-clone editing from group editing at the type level.**
`LOCK_OFF` is 95% of editing and needs *only* direct field edits. Keep the group delta-math in a
separate `transformGroup` path invoked *only* for `mGroupId != null`. The `LOCK_OFF` short-circuit
already does this dynamically; promoting it to a structural split removes the "single clone runs group
math" foot-gun permanently.

**D. Make the multi-step contracts explicit.**
- Manip: a small RAII/`DragSession` whose destructor *is* the `transformUnit` commit + revision sync,
  so a drag cannot end without an authoritative commit.
- Preview: publish preview state from a single `refreshFormationPreview` that is invoked by **one**
  observer of "selection or crowd-config changed", not N scattered callbacks.

**E. A finite/degeneracy backstop at every GPU boundary.**
Already added at the three pivots; generalize: no clone transform reaches `syncMatrices`/the deferred
queue without a `isFinite` gate that drops the frame instead of the driver. Cheap insurance against
the entire TDR class.

**F. Coordinate-space types.**
Even lightweight tag types (`GlobalPos`, `AgentPos`) on the hot signatures would have made the
space-mixing bugs compile errors.

Suggested staging (each independently shippable, Codex-reviewed, in-world-verified):
1. Normalize-on-write rotation (B) + generalize the finite backstop (E) — pure hardening, no behavior
   change. **Do first.**
2. Structural `LOCK_OFF` split (C).
3. Kind-polymorphic leaf (A) — the big refactor; do behind the now-hardened core.
4. Explicit drag/preview contracts (D).
5. Space types (F) — opportunistic.

---

## 15. Invariants & a test harness (stop the bleeding)

Even before the refactor, add **assert-level invariants** that turn silent explosions into loud,
early failures:

- `llassert(is_approx_equal(mRotation.magnitude(), 1.f))` on every rotation write.
- `llassert(llfinite(scale) && scale >= 0.05f && scale <= 10.f)` in `setScale`.
- `llassert(mFootGlobal.isFinite())` in `setFootGlobal`/`setTransform`.
- A `debug`-only `ghostTransformInvariant()` walked once/frame over all instances, logging (not
  crashing in release) any non-unit rotation, non-finite foot, or member whose closest-pair gap
  collapsed.

And a **headless-ish repro harness** for the class of "drag a slider N times" bugs: a debug command
that programmatically calls `transformUnit` with scale 1.0→0.05→10 for M iterations on a selected
clone (and a locked group) and asserts the rotation stays unit and the foot stays finite. That single
loop reproduces tonight's #1 and #2 in milliseconds, with no GPU and no in-world session.

---

## 16. Appendix — file:line index (anchors, uncommitted tree)

| Symbol | Location |
|---|---|
| `Instance` struct / setters | `alghoststudio.h` ~151 / ~255-280 |
| `transformUnit` (+ LOCK_OFF, normalize) | `alghoststudio.cpp` ~1535 / ~1560 / ~1551 |
| `spawnEntityClone` / `applyEntityTransform` | `alghoststudio.cpp` ~375 / ~804 |
| `groupMembers` / `lockGroup` | `alghoststudio.cpp` ~1447 / ~1462 |
| `formationSlotAt` / `renderFormationPreview` / `setFormationPreview` | `alghoststudio.cpp` ~1840 / ~2315 / ~2300 |
| `updateEntityOuterTransform` / `setEntityScale` | `llghostavatar.cpp` ~185 / ~235 |
| `LLGhostAvatar::idleUpdate` (drive) | `llghostavatar.cpp` ~1308 |
| `drawGeometryGhost` (overlay draw) | `llactormover.cpp` ~3862 (pivot ~3902, modelview ~3958) |
| deferred proxy builder / `computeProxyBounds` | `llactormover.cpp` ~5690 / ~5753 |
| `capture_root_above` (the clamp the pivots forgot) | `llactormover.cpp` ~500 |
| `refreshFormationPreview` (preview publisher) | `alpanelghoststudio.cpp` ~1864 (calls ~493/1040) |
| `onScaleCommit` / edit mode | `alpanelghoststudio.cpp` ~1299 / ~1643 |
| `pullProxyToInstance` / `endDrag` | `alghostmanipproxy.cpp` ~226 / ~332 |
| `LLClientOuterTransform` | `llclientoutertransform.h` |
| outer transform consumed | `lldrawpool.cpp` ~726, `llvoavatar.cpp` ~5542/1617 |
| anim time advance | `llmotioncontroller.cpp` ~871 |
| preview render hooks (path, crowd) | `llviewerdisplay.cpp` ~1795 / ~1798 |
| **path lines** — `renderHeadingPreview` | `llactormover.cpp` ~4747 |
| overlay **primitive helper family** (ribbon/disc/stick/digit/chevron) | `llactormover.cpp` ~3134-3560 |
| Catmull-Rom curve evaluator (draw + traversal) | `llactormover.cpp` ~118 (used ~1901/2206) |
| **crowd markers** drawing (perimeter/disc/ring/stalk/arrow/label) | `alghoststudio.cpp` ~2369-2470 |

---

*End. The fastest single win is §14-B + §15's normalize-on-write invariant: it makes the highest-cost
bug of the night (the quaternion TDR) structurally impossible, with no behavior change.*
