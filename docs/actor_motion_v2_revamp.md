# Actor Motion V2 — Natural Authored Pathing and Grounded Locomotion

**Status:** deep-research architecture and delivery specification  
**Repository:** `I:\alchemy-machinima`  
**Target subsystem:** `LLActorMover`, the shared Move/Path panels, path preview, Director scene persistence, trajectory presentation, and grounded root motion  
**Research date:** 2026-08-23  
**Implementation rule:** the human builds and performs in-world tests. Codex may implement and add automated tests, but must not build or commit unless explicitly asked.

This document supersedes the earlier proposal to merge Move and Path into one panel. That would solve a model problem by damaging the cleanest motion UI in the viewer. The correct boundary is the opposite:

> **Freeze the two existing user experiences; unify and replace the motion engine beneath them.**

The Move panel remains the fast, low-friction way to send an actor in a direction. The Path panel remains the precise authoring surface for routes, timing, pose ghosts, cameras, and choreography. They may share a model and runtime, but they must not be visually merged.

The end state is an actor that faithfully follows the curved path drawn by the director, anticipates turns, changes speed naturally with curvature, remains grounded on terrain and stairs, places its feet convincingly, and reproduces the same take. Preview ghosts must be inert reference images: enabling or hovering over them must never move the actor, mutate the path, or make the ghosts walk away.

---

## 1. Executive decisions

These are design constraints, not suggestions.

1. **Preserve the UI.** Keep `panel_actor_mover.xml` and `panel_path_editor.xml` as separate panels with their current hierarchy, density, and primary controls. Both the Actor Mover floater and Director Console must continue embedding those shared panel XML files. No combined panel, duplicated variant, or large control migration.
2. **One engine, two workflows.** Move creates a transient, usually straight motion command. Path edits a persistent motion asset. Both compile to the same trajectory/runtime types and receive the same grounding, turning, speed, and animation presentation.
3. **Preview is read-only.** Hover can alter highlight state only. Preview rendering consumes an immutable snapshot and may never read a moving actor's live root as the snapshot pivot.
4. **The authored path is authoritative.** The user places the route. V2 compiles that exact artistic intent into a smooth, time-parameterized trajectory; it never searches for another route, inserts avoidance detours, or moves control points behind the director's back.
5. **Natural turning is trajectory presentation, not IK.** Path interpolation determines the spatial curve. Look-ahead facing, angular acceleration limits, curvature-aware speed, animation blending, cadence, and bounded stride/orientation warping make the avatar appear to understand the curve.
6. **No new IK system.** Actor Motion V2 does not require true, full-body, or bespoke locomotion IK. The viewer's existing optional Pose Polish contact pass may remain as a small stair/grounding enhancement, but it must not become a dependency or expand the scope of the project.
7. **No navmesh or automatic pathfinding.** Actor Motion V2 must not depend on Havok, simulator navmesh access, Recast, obstacle routing, crowd steering, or automatic collision avoidance. If the user draws a path through an object, V2 does not rewrite it.
8. **Persistence is required.** Authored paths, path settings, trajectory settings, and locomotion profiles belong in Director scenes. The current session-only `mPaths` map is not acceptable for a production motion system.

### Definition of done

Actor Motion V2 is done when all of the following are true:

- the existing Move and Path menus look and behave familiar at first glance;
- a user can draw a curved route and see the exact compiled motion before playback;
- the actor slows into tight turns, rotates without snapping or skating, and honors arrival facing;
- the actor traverses supported ramps and stairs without root bobbing, floating, toe burial, or feet sliding across planted contacts;
- pose ghosts remain fixed at their authored nodes while the source actor moves;
- pointer hover without a verified drag cannot mutate a node, start locomotion, or change actor transforms;
- saved scenes restore the same path and produce the same motion within defined tolerances;
- unsupported ground or stair transitions warn visibly and use an explicit stop/fallback policy without altering the horizontal route.

The minimum convincing result is deliberately simpler than physical simulation: the avatar looks ahead into the bend, begins rotating before the apex, slows enough to avoid skating, keeps animation cadence/stride near the travel speed, and eases cleanly into the next direction.

---

## 2. Product model: authored path, trajectory, and animation presentation

Several terms were previously collapsed into “pathing.” Separating them prevents architectural mistakes.

| Layer | Question it answers | Example output |
|---|---|---|
| **Intent** | What does the director want? | “Walk through these marks and arrive facing camera.” |
| **Authored path** | Where must the actor travel? | User-placed anchors plus interpolation/tangent settings. |
| **Geometric curve** | What exact continuous line connects those anchors? | Evaluated spline or explicit linear segment. |
| **Trajectory** | Where, how fast, and which way over time? | Samples of position, velocity, acceleration, facing, curvature, and surface. |
| **Locomotion** | Which animation and phase realize that motion? | Walk, jog, turn, step-up; stride/cadence scaling. |
| **Animation presentation** | How does the performance appear to understand the curve? | Gait choice, cadence, stride, turn anticipation, bounded body twist, lean, and transition blends. |
| **Preview** | What will the take do without changing scene state? | Frozen route, diagnostics, and immutable pose ghosts. |

The current Catmull–Rom implementation already provides a geometric path and traversal clock. V2 keeps the route under the user's control and adds the missing natural-trajectory, grounding, gait, and animation-presentation layers without throwing away useful work.

---

## 3. Verified current state

This section records the implementation facts on which the proposal is based. Symbol names are preferred over brittle line numbers.

### 3.1 The UI already has the right separation

The standalone Actor Mover and Director Console both reuse the same panel definitions:

- `skins/default/xui/en/panel_actor_mover.xml` — scope, heading, speed, distance, cadence, end behavior, custom animation, Walk, and Stop;
- `skins/default/xui/en/panel_path_editor.xml` — waypoint editing, pose ghosts, per-node inspector, path settings, cameras, sync/follow, and suspend status;
- `skins/default/xui/en/floater_actor_mover.xml` — embeds Move and Path as separate tabs;
- `skins/default/xui/en/floater_director.xml` — embeds the same two panels in the Director rail.

This reuse is a strength. UI changes in one host cannot drift from the other as long as both continue to embed the same shared panel. V2 must preserve that property.

### 3.2 The two panels currently feed different models

`ALPanelActorMover` is effectively a settings-backed command surface. It reads global values such as `ActorMoverSpeed`, `ActorMoverDistance`, `ActorMoverHeading`, and `ActorMoverEndMode`, then calls `LLActorMover::start()` or `startAll()`.

`ALPanelPathEditor` edits a per-actor `LLActorMover::Path` stored in `LLActorMover::mPaths`. A path includes waypoints, per-node dwell/speed/animation/ground offset, interpolation and easing, end behavior, arrival facing, cameras, follow behavior, and other advanced state.

Inside `LLActorMover::start()`, a path with at least two nodes wins a hidden branch. That branch uses `Path::mSpeed`, `Path::mEndMode`, and the path length. The immediate Move branch uses the Move settings. Therefore a visible Move speed or end behavior can be ignored merely because an authored path exists. This is a data-contract bug, not a reason to combine the panels.

### 3.3 Current trajectory and grounding

`LLActorMover::advancePath()` already provides useful foundations:

- Catmull–Rom evaluation with an arc-length table;
- per-node speed overrides;
- tangent-derived facing with a configurable yaw turn-rate cap;
- optional pitch-to-slope;
- ease-in/ease-out and cadence scaling;
- Stop, Loop, and Ping-pong traversal.

`LLActorMover::resolveGroundZ()` performs a single vertical intersection at the actor root XY, starting about 1.5 m above and ending about 3 m below. It returns a height, not a durable surface sample. It does not provide a normal, support area, step classification, toe/heel support, or temporal surface identity. This is adequate for a first terrain-follow pass but structurally unable to produce stable stair motion.

### 3.4 A prior flat-ground regression was already fixed

Commit `0e08560acbe` fixed an older “flat-ground hover” problem by persisting each waypoint's root-above-ground relationship in `Waypoint::mRootAbove` and reconstructing the root from surface height plus that offset. That fix should remain. It is distinct from the current report that preview ghosts walk or drift away when pathing begins.

### 3.5 The current ghost path has a live-frame pivot hazard

`renderHeadingPreview()` gathers path-node ghosts and calls `drawGeometryGhost()`. For ordinary path ghosts it constructs `GhostDrawParams path_gp` but does not set `mHavePivot`. `drawGeometryGhost()` then computes its pivot from the source avatar's **live** render position and current pelvis-to-foot distance.

This is unsafe once the source actor starts moving. The ghost's desired destination is a fixed node, but the placement frame is recomputed from a changing live source. Geometry/palette data and pivot can also represent different frames. The visible result can be translation drift, node ghosts “walking” with or away from the actor, or a pose that appears to flee when pathing is enabled.

The codebase already contains the correct precedent: Ghost Studio frozen and cadence instances set `GhostDrawParams::mHavePivot`, provide `mPivotFootAgent`, and pair that pivot with captured palettes. Path preview should use the same frame-matched contract.

### 3.6 Hover is intended to be pure, but needs a hard invariant

`ALToolPathEdit::handleHover()` changes a waypoint only when both of these are true:

- `mDragNode >= 0`; and
- the tool owns mouse capture.

Otherwise it updates `mHoverNode` and the cursor. A normal hover should therefore be read-only today. However, stale capture, a missed mouse-up, actor replacement, tool deactivation, or a lost window focus can leave transient drag state alive. V2 must make hover purity explicit and testable rather than relying on paired events always arriving.

### 3.7 Paths are not saved with Director scenes

`LLFloaterDirector::saveScene()` writes scene version 4, cast state, a list of global settings, prism state, and lighting data. `LLActorMover::mPaths` is explicitly marked session-only and is not serialized. A serious authoring tool cannot lose route work across viewer sessions or scene changes.

### 3.8 Existing contact polish is optional

`LLVOAvatar::updateCharacter()` already calls `mPosePolish.run()` after motion updates, and `ALPosePolish::runContact()` can stabilize planted contacts. `indra/newview/tests/alposepolish_test.cpp` tests its pure contact inference layer.

This is useful existing polish, but it is not the solution to curved walking. Natural curve traversal must already look good from trajectory timing, facing, cadence, and animation blending with Pose Polish disabled. If later stair testing shows value, the existing pass may consume better surface/contact hints behind its current feature gate. V2 must not introduce or require a new solver.

### 3.9 Navmesh and automatic route search are deliberately excluded

The viewer contains legacy pathfinding UI and physics-extension hooks, but Actor Motion V2 will not integrate them. The required Havok functionality is unavailable, and—more importantly—the director is already programming the route by placing nodes. Automatic route search would add a dependency while weakening artistic control.

The V2 problem is therefore not “find a way from A to B.” It is: **make the avatar traverse the exact user-authored A-to-B curve as a convincing living performer.** World intersection queries remain useful for ground height, surface normal, stairs, and foot contacts; they must never become permission to reroute the horizontal path.

---

## 4. Non-negotiable UI preservation contract

“Preserve the UI” must be enforceable during review.

### 4.1 Frozen surfaces

The following are frozen unless a concrete bug requires a change:

- panel separation and tab/rail placement;
- primary control order;
- labels and familiar terms;
- Walk/Stop placement;
- waypoint list plus node inspector workflow;
- default collapsed/expanded sections;
- selection/scope semantics;
- both hosts using the same panel XML resources.

Engine fixes, persistence, smarter defaults, route compilation, grounding, and preview isolation require **no** visible layout change.

### 4.2 Allowed cleanup

Cleanup is allowed only when it reduces ambiguity without rearranging the workflow:

- correct a misleading tooltip;
- disable a control with a precise explanation when it cannot apply;
- show a compact route-state message in the existing status/readout area;
- remove a redundant or provably dead field;
- fix spacing, clipping, focus order, or accessibility labels;
- expose genuinely advanced controls through one unobtrusive `Advanced…` action or context menu, leaving the panel's default geometry unchanged.

### 4.3 Advanced feature exposure

The first delivery phases should add no new primary controls. Use good defaults:

- existing hand-authored paths compile as `Direct/Smooth`;
- Move remains a direct transient route;
- grounded locomotion can be enabled behind an Advanced profile and later become the default after validation;
- optional motion tuning such as turn anticipation, gait limits, and foot-contact diagnostics may live in an `Advanced…` dialog;
- diagnostics may use overlays toggled from that dialog, not permanent panel clutter.

If later user testing proves a mode selector deserves first-class status, the maximum addition is one compact row in the existing Path settings area. That is a separate product decision, not permission to redesign the panel.

### 4.4 UI regression gate

Before and after screenshots must be captured for:

- Actor Mover / Move;
- Actor Mover / Path;
- Director / Move;
- Director / Path;
- 100%, 125%, and 150% UI scale;
- shortest supported window height;
- empty selection, single actor, multi-actor, and active playback.

Automated checks should assert that both floaters still reference the shared `panel_actor_mover.xml` and `panel_path_editor.xml`, and that the frozen primary control names remain present. Any positional change requires an annotated screenshot and explicit review.

---

## 5. P0: stop the runaway preview before extending pathing

This defect must be fixed and regression-tested before natural locomotion work. Advanced features built on a moving preview cannot be trusted.

### 5.1 Preview ownership rule

A preview is a rendering artifact, not an actor state.

It may read:

- an immutable path snapshot;
- an immutable preview-pose snapshot;
- camera and display preferences;
- explicit hover/selection highlight state.

It may not write:

- actor root position or rotation;
- playback state;
- path nodes or settings;
- animation state;
- scene selection;
- a live source pivot into a retained preview object.

### 5.2 Frame-matched pose snapshot

Add a preview snapshot keyed by actor identity and a monotonically increasing generation:

```cpp
struct PathPreviewPoseSnapshot
{
    LLUUID actor_id;
    U64 generation = 0;
    LLVector3 pivot_foot_agent;
    LLQuaternion source_rotation;
    frozen_palette_map_t palettes;
    frozen_attachment_matrix_map_t attachment_matrices;
    std::vector<GhostBatch> batches;
    std::vector<GhostStaticFace> static_faces;
    bool complete = false;
};
```

The exact aliases can reuse Ghost Studio types. The invariant matters more than the spelling: batches, palettes, attachment matrices, pivot, and source rotation all describe the same capture frame.

Capture or refresh the snapshot only on explicit invalidation:

- actor/appearance/skeleton generation changes;
- user requests a preview refresh;
- path preview is enabled and no valid snapshot exists;
- an intentional cadence preview advances to a newly captured frame.

Do **not** recapture merely because the actor root moved during playback. At draw time, set:

```cpp
path_gp.mHavePivot = true;
path_gp.mPivotFootAgent = snapshot.pivot_foot_agent;
path_gp.mFrozenPalettes = &snapshot.palettes;
path_gp.mFrozenAttachMats = &snapshot.attachment_matrices;
```

Then place each preview at the node's fixed foot destination. A missing or incomplete snapshot should use the existing card/stick fallback, never fall back to a live moving pivot during active path playback.

### 5.3 Hover/drag state machine

Replace the implicit `mDragNode + mouse capture` contract with an explicit transient state:

```text
Idle -> HoverNode -> PressedNode -> DraggingNode -> Idle
                     |               |
                     +-> Cancelled <-+
```

Rules:

- `HoverNode` can change only highlight/cursor state.
- A node mutation requires a `PressedNode` record created by a verified primary-button down in the current tool generation.
- Enter `DraggingNode` only after a small pixel threshold or explicit drag policy.
- Each write validates actor id, path revision, node identity/index, button state, and current capture ownership.
- Capture loss, window deactivation, Escape, actor switch, path replacement, tool switch, deletion, or viewer focus loss cancels the transaction and clears all drag fields.
- Mouse-up commits exactly one undoable edit and releases capture.
- Walk-to arming and node dragging are mutually exclusive states.

Use a stable node UUID for long-lived editor references. An index alone can point at another node after insertion/deletion.

### 5.4 Transform write guard

Add debug-only instrumentation around actor transform writes during editor interaction. Every `LLActorMover` root write should carry a reason such as:

```text
PlaybackAdvance
ImmediateMoveAdvance
FollowAdvance
WalkToAdvance
RestoreOnStop
EditorPreview  // forbidden
```

The renderer and hover handlers should have no transform-write reason at all. A debug assertion or scoped write token will turn future “hover moved my actor” regressions into an immediate, attributable failure.

### 5.5 P0 acceptance tests

For a stationary and then moving actor:

1. Enable path and pose-ghost display; record every ghost's world transform.
2. Start playback and let the actor travel beyond the first two nodes.
3. Confirm each node ghost remains at the same destination within 1 mm and does not inherit actor root translation.
4. Hover every node, line segment, and empty ground area for at least five seconds. Confirm path revision, node coordinates, playback state, and actor root are unchanged.
5. Begin a drag, move outside the viewer, release, return, and hover. Confirm no stale drag continues.
6. Repeat with actor selection change, tool switch, Escape, floater close, focus loss, actor runtime replacement, region crossing, and Stop/Restore.
7. Repeat with frozen, cadence, live/fallback, rigged mesh, system avatar, non-rigged attachment, and missing-batch paths.
8. Toggle path display during playback. No preview item may jump toward or away from the live actor.

Log snapshot generation, pivot, path revision, tool state, and transform-write reason when `ActorMotionDebugPreview` is enabled.

---

## 6. V2 architecture

```text
  Move panel (unchanged)             Path panel (unchanged)
       | transient command                 | persistent asset edits
       +------------------+----------------+
                          v
                Motion command / asset model
                          |
                    compile / validate
                          v
       +-------------------------------------------+
       | authored curve -> arc length/curvature    |
       | -> timed trajectory + surface samples     |
       +-------------------------------------------+
                          |
                immutable compiled motion
                          |
                    traversal instance
                          v
       root position + travel/facing intent + phase hints
                          |
             animation selection / cadence / warp
                          |
                 LLVOAvatar motion blending
                          |
             optional existing contact-polish pass
                          |
                       final pose

  Separate branch: compiled motion + frozen pose snapshot -> preview renderer
                   (no actor/model write path)
```

### 6.1 Core types

Names are proposals; contracts are normative.

#### `MotionDefaults`

Shared semantic values used by both workflows:

```cpp
struct MotionDefaults
{
    F32 speed_mps;
    F32 nominal_anim_speed_mps;
    EndMode end_mode;
    LLUUID animation_id;
    S32 animation_priority;
    LocomotionProfileId locomotion_profile;
};
```

The Move panel edits the user defaults. A new Path may copy those defaults once at creation, after which its values are explicit per-asset properties. Clicking Walk in Move always means “execute this Move command”; clicking Play/Walk for a Path means “execute this Path asset.” The runtime must never silently choose one merely because the other exists.

This resolves the current hidden precedence while preserving both panels.

#### `MotionAsset`

```cpp
struct MotionAsset
{
    S32 schema_version;
    LLUUID asset_id;
    std::string name;
    LLUUID actor_binding;          // optional; empty means reusable
    std::vector<RouteNode> nodes;
    MotionDefaults defaults;
    InterpolationSettings curve;
    TrajectorySettings trajectory;
    LocomotionProfile locomotion;
    CameraTrack camera;
    Revision revision;
};
```

Keep authored data separate from derived caches. Arc tables, curve derivatives, trajectory samples, and surface queries must be rebuildable and should not pollute undo history.

#### `CompiledMotion`

An immutable, revision-stamped result:

```cpp
struct CompiledMotion
{
    LLUUID asset_id;
    Revision source_revision;
    CompileStatus status;          // Ready, Warning, Invalid
    std::vector<PathSegment> path;
    std::vector<TrajectorySample> samples;
    Diagnostics diagnostics;
    U64 deterministic_hash;
};
```

Playback owns a `MotionInstance` referencing a `shared_ptr<const CompiledMotion>`. Editing creates a new revision and compile result; it does not mutate a path under a running take.

#### `TrajectorySample`

At minimum:

```cpp
struct TrajectorySample
{
    F64 time_s;
    F64 distance_m;
    LLVector3d root_global;
    LLVector3 velocity_agent;
    LLVector3 acceleration_agent;
    LLQuaternion facing;
    F32 curvature;
    SurfaceSample surface;
    GaitHint gait;
};
```

The runtime may evaluate analytically between sparse samples. The compiled representation must be deterministic and independent of frame rate.

### 6.2 Command semantics

Use explicit entry points:

```cpp
startImmediate(actor_id, MoveCommand)
startAsset(actor_id, asset_id, revision)
startWalkTo(actor_id, destination, policy)
```

Deprecate ambiguous `start(actor_id)` behavior that inspects unrelated state to decide whether the user meant Move or Path.

### 6.3 Coordinate and identity rules

- Author route nodes in global coordinates (`LLVector3d`) as today.
- Build local float work data relative to a compile origin to preserve precision.
- Store foot-relative and root-relative heights explicitly; never infer one from the other after an actor scale/skeleton change without invalidating the compile.
- Key authored assets by stable asset UUID, not the transient avatar runtime pointer.
- When actor runtime identity changes, rebind the motion instance and preview snapshot deliberately; never migrate raw pointers.

### 6.4 Threading and lifetime

- UI/model edits occur on the main thread.
- Curve/trajectory compilation operates on an immutable asset revision and should remain cheap enough for interactive editing; long surface-preflight work may use a worker over immutable samples.
- World-object pointers may not cross a worker boundary. Copy only the minimal surface results and stable object identifiers needed for diagnostics.
- Publish compile results atomically only if asset id and revision still match.
- Playback performs no route search, curve rebuilding, or heap allocation in the hot per-frame path.

---

## 7. True pathing: faithfully compile the authored curve

### 7.1 Authorial authority contract

The node sequence is the route. The motion engine may evaluate, time, and visually realize it, but must not:

- insert an avoidance waypoint;
- delete, reorder, or laterally move a user waypoint;
- substitute a shortest path;
- steer around avatars or objects;
- project the horizontal path onto a hidden routing representation;
- change the curve because playback has started.

The only automatic vertical adjustment is grounding against terrain, ramps, and stair treads. Even there, the engine changes height/support interpretation, not the authored horizontal course.

### 7.2 Curve compilation pipeline

For each immutable path revision:

1. validate finite coordinates and at least two distinct nodes;
2. transform global coordinates into a stable local compile frame;
3. build the selected interpolation through the authored nodes;
4. create an adaptive arc-length table with a bounded distance error;
5. compute tangent, curvature, grade, and segment boundaries;
6. construct a forward/backward speed envelope from requested speed, curvature, acceleration, braking, dwell, and arrival constraints;
7. generate facing/look-ahead intent without altering position;
8. sample expected ground support and step events when world data is available;
9. emit an immutable `CompiledMotion` stamped with the source revision and deterministic hash.

Editing publishes a new revision. A running take continues using its captured revision until stopped or explicitly restarted.

### 7.3 Spline policy

Current Catmull–Rom interpolation remains useful for art-directed paths. Prefer centripetal parameterization for new path segments because it avoids cusps and self-intersections within individual segments under the conditions analyzed by Yuksel, Schaefer, and Keyser ([“Parameterization and applications of Catmull–Rom curves,” 2011](https://www.sciencedirect.com/science/article/pii/S0010448510001533)).

The interpolation must pass through every authored node. It may not “repair” a difficult bend by moving a point. Instead, expose the consequence in preview: tight curvature, forced slowdown, abrupt tangent, insufficient braking distance, or a degenerate segment.

Recommended segment modes:

- `AuthoredSpline` — smooth interpolation through user nodes;
- `AuthoredLinear` — exact straight connection when a hard corner is intentional;
- `Hold` — remain at the node for its dwell duration;
- `Teleport/Action` — explicit non-walk transition, never inferred silently.

Optional tangent handles are an authoring tool, not an autonomous route generator. Automatic tangents may provide a reversible initial value, but the stored result must remain inspectable and deterministic.

### 7.4 Natural-turn feasibility

The compiler should diagnose motion that cannot look natural at the requested speed:

- curvature exceeds the gait/profile limit;
- a segment is too short to accelerate or brake;
- the requested arrival facing demands an extreme last-frame turn;
- adjacent duplicates or reversals create an undefined tangent;
- a loop or ping-pong seam has discontinuous position, tangent, speed, or phase;
- the vertical course contains an unsupported step or grade.

Default behavior is to lower speed within the user's permitted limits and preserve the curve. If that is still insufficient, mark the path `Warning` and show the affected segment. Do not reshape it.

### 7.5 Compile status

- `Ready` — the authored curve and timing are internally valid.
- `Warning` — playback is possible, but a turn, surface, animation, or stair condition may not meet the selected naturalness profile.
- `Invalid` — malformed/degenerate input prevents deterministic evaluation.
- `StaleSurface` — the curve is valid but cached ground samples no longer match the world; refresh grounding without changing XY.

Diagnostics should name actionable causes and their path distance: tight turn, speed reduction, abrupt facing, insufficient braking distance, unsupported surface, excessive grade/step, low contact confidence, or animation range exceeded.

### 7.6 Determinism and direct control

- the same authored asset and compiler version produce the same horizontal curve and timing;
- no dynamic obstacle or actor can steer the performer away from the curve;
- no random steering term exists in principal-actor playback;
- user-authored node speed, dwell, facing, animation, and camera overrides remain hard constraints;
- ground reacquisition may change only vertical support and optional animation/contact polish under an explicit policy;
- playback never recompiles because another object entered the scene.

---

## 8. Natural curved locomotion and body orientation

A spline-following root with tangent yaw is necessary but visually insufficient. Natural motion needs a time-parameterized trajectory and layered orientation.

### 8.1 Curvature-aware speed

Let the horizontal path be parameterized by arc length `s`. Estimate curvature:

```text
kappa(s) = |dT/ds|
```

Apply limits from both animation/art direction and physical comfort:

```text
v_curve <= sqrt(a_lateral_max / max(kappa, epsilon))
omega = v * kappa
```

Then run a forward/backward speed pass over the path to enforce acceleration and braking limits, node speed caps, arrival speed, and dwell. Humans also reduce speed as path curvature rises; experimental walking research supports treating curvature as a speed-planning input rather than maintaining a constant speed through every bend ([Vieilledent et al., “Relationship between velocity and curvature of a human locomotor trajectory,” 2001](https://www.sciencedirect.com/science/article/pii/S0304394001017980)).

Use the equation as a stable trajectory-compiler constraint, not a claim of biomechanical simulation. Tune defaults by gait and avatar scale.

### 8.2 Look-ahead facing

Facing should derive from a filtered look-ahead direction, not only the instantaneous spline derivative:

- look-ahead distance grows with speed and is clamped by remaining segment length;
- orientation anticipates a corner before the root reaches it;
- limit angular velocity and angular acceleration;
- unwrap yaw across ±180°;
- blend into an explicit arrival facing target;
- use turn-in-place when translation is near zero but required facing change is large.

Separate:

- **travel direction** — horizontal velocity;
- **root/hips facing** — locomotion frame;
- **chest/head attention** — look-at or camera target;
- **foot direction** — contact and animation outcome.

This permits a character to glance at camera while continuing through a curve without rotating the whole root unnaturally.

### 8.3 Orientation and stride warping

The engine should supply trajectory intent; the animation presentation layer can distribute a small residual mismatch. Epic's documented pose-warping model is a useful reference for separating orientation, stride, and slope adjustments ([Pose Warping documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/pose-warping-in-unreal-engine)). V2 needs only a conservative equivalent—not a generalized pose solver.

For this viewer, implement a conservative equivalent rather than copying an engine-specific node graph:

- keep root yaw driven by the trajectory;
- distribute a bounded travel-versus-facing delta across pelvis, spine, and legs;
- adjust cadence first and stride second within authored limits;
- clamp every adjustment to a conservative range so the source animation remains recognizable;
- disable or heavily limit warping for custom animations unless their metadata opts in.

### 8.4 Lean and banking

Additive lean can communicate acceleration and cornering:

- forward/back lean from tangential acceleration;
- lateral lean from `v² * curvature` with sign;
- low-pass filtering and strict angle caps;
- distributed over pelvis/spine, not encoded as a waypoint yaw offset;
- disabled for stairs until foot and pelvis stability are proven.

This is polish, not a substitute for safe route geometry or foot contacts.

### 8.5 Start, stop, reverse, and ping-pong

The locomotion state machine must explicitly handle:

```text
Idle -> Start -> Walk/Jog/Run -> Brake -> Stop
                      |
               Turn / StepUp / StepDown
                      |
              ReverseTransition (Ping-pong)
```

Ping-pong must not instantaneously negate velocity and yaw. Brake to a configurable turnaround, execute a turn or direction-aware transition, then accelerate back. Loop seams must validate position, tangent, speed, facing, animation phase, and surface continuity.

---

## 9. Terrain, ramps, and stairs

### 9.1 Replace height lookup with a surface service

Introduce a shared `ALSurfaceSampler` used by trajectory validation and root grounding, with optional read-only hints for existing contact polish:

```cpp
struct SurfaceSample
{
    LLVector3 position_agent;
    LLVector3 normal_agent;
    LLUUID object_id;
    U32 face_id;
    SurfaceKind kind;       // terrain, prim, mesh, unknown
    F32 confidence;
    F32 support_area;
    bool valid_support;
};
```

API examples:

```cpp
sampleSupport(point, probe, filter)
sweepSupportSphere(from, to, probe_radius, filter)
sampleFoot(ankle, foot_geometry, previous_surface, filter)
```

All queries must exclude the actor, worn attachments, path ghosts, Ghost Studio clones, edit handles, and other non-world preview geometry.

### 9.2 Probe strategy

A single ray can miss a tread near the riser or snap between adjacent surfaces. Use a layered strategy:

- center sphere sweep for robust root support;
- left/right foot support probes;
- heel, toe, and optionally lateral foot samples for orientation and edge confidence;
- short forward probe when approaching a suspected step;
- previous object/face as a temporal coherence hint;
- terrain fallback only if no valid object support is found within the profile range.

Sphere sweeps are commonly used for robust foot placement because they tolerate small lateral offsets better than a zero-radius ray; Unreal's foot-placement trace settings explicitly expose a sweep radius, start/end offsets, and penetration allowance ([FootPlacementTraceSettings](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/FootPlacementTraceSettings?application_version=5.6)).

### 9.3 Support selection and hysteresis

Score candidate surfaces using:

- vertical distance from predicted support;
- upward-facing normal and slope limit;
- continuity with the previous object/face;
- area under the foot;
- support area and edge distance;
- direction of travel and expected step event;
- exclusion/material flags.

Switch support only when the new candidate wins by a margin or the old support becomes invalid. This prevents root chatter between a stair tread and terrain below it.

### 9.4 Root motion on stairs

Root vertical motion should follow a filtered support envelope, not each raw trace:

- preserve `mRootAbove` semantics as the actor-specific nominal foot-to-root relation;
- permit controlled upward movement when the next tread is reachable;
- constrain downward rate so a momentary miss does not drop the actor through a step;
- distinguish step-up/down from a continuous slope;
- keep horizontal progression tied to arc distance, not frame count;
- pause/fail if the next step exceeds profile height or lacks landing support;
- treat moving platforms as a later explicit surface-relative feature.

For authored stair routes, compile step events with expected takeoff/landing support. Runtime sampling verifies them rather than discovering the whole staircase one frame at a time.

### 9.5 Optional contact polish—not a V2 requirement

Stair traversal must first look credible from the compiled root-height envelope, step-aware speed, appropriate animation phase/cadence, and smooth facing. That is the required V2 solution.

The existing `ALPosePolish::runContact()` may optionally receive predicted support/contact hints after the core system is stable. Any such integration must:

- remain behind the existing feature gate and default safely off until validated;
- use the existing implementation rather than create a new solver in `LLActorMover`;
- affect limb presentation only, never route position, root facing, or trajectory timing;
- release immediately on teleport, discontinuity, support loss, animation change, reverse, or loop seam;
- be removable without changing how the actor follows the curve.

True or full-body IK is explicitly outside this project. The success criterion is the appearance of natural walking, not biomechanical reconstruction.

### 9.6 Stair acceptance matrix

Test at minimum:

- terrain slopes below, at, and above the profile limit;
- prim ramps and mesh ramps;
- straight stairs with shallow, nominal, and near-limit risers;
- open stairs with gaps;
- narrow treads near foot length;
- landings and 90°/180° turns;
- mixed terrain-to-prim and prim-to-mesh transitions;
- actor scales from the supported minimum to maximum;
- walking up and down, stopping mid-flight, ping-pong, loop, and resume;
- custom animation enabled and disabled;
- low frame rate, hitch, and fixed-timestep playback;
- missing/late geometry and disappearing support objects.

Acceptance should measure root height discontinuity, obvious foot penetration/sliding, surface flapping, heading continuity, animation-speed mismatch, and time-to-route completion—not rely only on visual impression.

---

## 10. Animation and gait contract

### 10.1 Locomotion profile

```cpp
struct LocomotionProfile
{
    BodyProfile body;
    GaitThresholds gait;
    F32 max_accel;
    F32 max_decel;
    F32 max_lateral_accel;
    F32 max_yaw_rate;
    F32 max_yaw_accel;
    F32 stride_warp_min;
    F32 stride_warp_max;
    F32 contact_polish_weight;
    bool grounded;
};
```

Profiles should be reusable and named, with a built-in conservative default. Per-path overrides must be sparse so profile improvements can propagate.

### 10.2 Gait selection

Begin with deterministic thresholds plus hysteresis:

- idle;
- start/stop;
- walk;
- jog/run if appropriate animations exist;
- turn-in-place;
- step-up/step-down hints;
- reverse transition.

Do not switch gait every time speed crosses a single threshold. Use entry/exit bands, minimum dwell, and phase-compatible transitions.

### 10.3 Cadence and stride

The existing nominal animation speed is valuable. Formalize it:

```text
cadence_scale = desired_speed / nominal_animation_speed
```

Clamp cadence to a natural range. Use bounded stride warping for the remaining difference. If the requested speed lies outside both safe ranges, choose another gait or report that a custom animation cannot faithfully match the trajectory.

### 10.4 Custom animation metadata

Custom animation should support optional metadata:

- nominal speed and stride;
- loop period and contact phases;
- left/right foot plant windows;
- facing/travel compatibility;
- allowable cadence/stride warp;
- optional grounded contact polish allowed;
- root-motion present/ignored;
- transition tags.

Without metadata, preserve current animation behavior and disable aggressive stride/orientation changes. Never guess contact phases or large body warps from animation UUID alone.

### 10.5 Future motion matching

Trajectory-conditioned motion matching can eventually choose clips using future travel samples, pose similarity, and contact state ([Epic Game Animation Sample](https://dev.epicgames.com/documentation/en-us/unreal-engine/game-animation-sample-project-in-unreal-engine)). This is a future research track, not a dependency for V2. Deterministic authored animation plus trajectory/orientation/stride presentation can deliver most near-term value with much lower data and integration risk.

---

## 11. Persistence, undo, and reproducible takes

### 11.1 Scene schema

Bump the Director scene schema and add a motion section:

```json
{
  "version": 5,
  "motions": {
    "assets": [
      {
        "asset_id": "...",
        "name": "Hero entrance",
        "actor_binding": "...",
        "nodes": [],
        "defaults": {},
        "curve": {},
        "trajectory": {},
        "locomotion": {},
        "camera": {}
      }
    ],
    "bindings": {}
  }
}
```

Version 5 is a proposal based on the current `SCENE_VERSION == 4`; coordinate with any concurrent scene-schema work before implementation.

Store authored nodes, curve/tangent settings, timing, locomotion profile, and the compiler version needed for reproducibility. Do not store GPU preview batches or per-frame runtime caches.

### 11.2 Migration

- Version 4 scenes have no saved paths; load with empty motion assets and existing Move global defaults.
- Current session paths converted during the same run should receive stable asset/node UUIDs and schema defaults.
- Preserve unknown future fields where practical or reject with a precise version message.
- Validate all numeric values for finiteness, bounds, node count, and sane coordinates before allocation or compile.

### 11.3 Undo model

Commands should include:

- add/delete/move/reorder node;
- edit node properties;
- edit path/profile properties;
- change selected segment interpolation or tangent handles;
- accept/reject an automatic tangent initialization;
- bind/unbind actor;
- paste/import motion asset.

Drag produces one undo record from press snapshot to release result, not one record per hover frame. Any automatic tangent/smoothing initialization is staged and becomes undoable only when accepted.

### 11.4 Deterministic playback

- Traverse by accumulated simulation time and arc length, not frames.
- Clamp/substep long deltas for contact and surface safety without changing final time.
- Use stable node/segment ordering; principal-actor traversal contains no stochastic steering.
- Hash the authored revision, compile inputs, and derived trajectory.
- Record curve-compiler and locomotion-profile versions with the asset.
- A fixed input scene and timestep sequence must produce the same trajectory hash and final transform.

---

## 12. Feature roadmap and dream-feature backlog

### Tier A — essential to the dream

1. **Author-faithful curve compiler.** Preserve every node while producing stable arc length, tangents, curvature, and deterministic timing.
2. **Curvature-aware motion.** Anticipatory turning, angular acceleration limits, and automatic slowdown in bends.
3. **Grounded stairs and slopes.** Multi-probe support, step validation, stable root height, step-aware speed/animation timing, and optional existing contact polish.
4. **Motion asset persistence.** Named, reusable routes saved with scenes and bound to actors.
5. **Immutable preview and scrub.** Stable pose ghosts plus a non-destructive time scrubber showing the evaluated trajectory/pose.
6. **Path diagnostics overlay.** Color by curvature speed cap, facing error, slope, surface confidence, step events, and contact phase.

### Tier B — high-value directing tools

7. **Arrival composition.** Arrive on a mark with exact facing, speed, foot preference, and optional look-at target.
8. **Behavior anchors.** Node actions such as pause, look, gesture, animation cue, prop handoff, or camera trigger with explicit blocking/non-blocking timing.
9. **Record-to-route.** Drive an actor manually, simplify the recording into editable anchors, then compile it into a clean deterministic trajectory.
10. **Tangent and curve handles.** Let the director shape entry/exit tangents, hard corners, and turn radius without moving the primary path nodes.
11. **Footstep timing editor.** Display predicted left/right plants and let advanced users align a chosen foot with a stair, mark, stop, or dramatic beat.
12. **Shared actor/camera timeline.** Show camera transitions and actor events on the same distance/time ruler without replacing the existing node editor.
13. **Retarget profiles.** Recompile the same motion asset for different avatar scale, stride, step height, and animation library.
14. **Per-segment locomotion style.** Walk, run, sneak, sidestep, backpedal, or custom animation with explicit transition metadata.

### Tier C — carefully bounded extensions

15. **Explicit traversal actions.** Doors, ladders, jumps, seats, elevators, and teleports, each with a named transition animation and entry/exit contract; never infer these from geometry.
16. **Moving-platform support.** Bind contacts and a route segment to a moving object's local frame with loss/reacquire policy.
17. **Authored formations.** Apply user-defined local offsets and timing relationships to a leader path while retaining editable paths for every performer.
18. **Independent gaze and torso tracks.** Aim head/chest attention separately from root travel and foot direction.
19. **Path transformation tools.** Mirror, offset, scale, reverse, and time-retime a route as explicit undoable edits.
20. **Trajectory-conditioned motion matching.** Future, data-heavy option once core deterministic locomotion and contact metrics are mature.

---

## 13. Delivery plan

Do not implement all layers in one branch. Each phase must leave the old workflow usable.

### Phase 0 — preview containment and editor purity

Deliver:

- frame-matched frozen path-preview snapshots;
- no live-pivot fallback during active playback;
- explicit hover/drag state and cancellation;
- transform-write reason instrumentation;
- preview/editor regression tests and in-world checklist.

Exit: reported runaway behavior cannot be reproduced under the P0 matrix, and hover performs zero model/transform writes.

### Phase 1 — one model/runtime beneath two panels

Deliver:

- explicit `startImmediate`, `startAsset`, and `startWalkTo` semantics;
- authored `MotionAsset` plus immutable `CompiledMotion`/`MotionInstance` split;
- stable asset/node IDs and revisioning;
- scene schema/persistence and migration;
- shared defaults with no silent Move/Path precedence;
- unchanged panel layouts.

Exit: saved/reloaded path playback matches the authored path, and each panel launches exactly the workflow it presents.

### Phase 2 — deterministic curved trajectory

Deliver:

- centripetal/validated spline option;
- forward/backward speed planning;
- curvature, acceleration, braking, yaw-rate, and yaw-acceleration constraints;
- look-ahead facing and correct arrival behavior;
- safe loop/ping-pong seams;
- trajectory diagnostics and deterministic hashes.

Exit: curves no longer produce constant-speed corner skating or yaw snaps, and fixed-timestep tests reproduce results.

### Phase 3 — authored-path control and motion diagnostics

Deliver:

- optional explicit tangent/hard-corner controls without moving primary nodes;
- curvature, facing, braking-distance, and gait-range diagnostics;
- reversible automatic tangent initialization for newly created paths;
- per-segment interpolation and locomotion-style metadata;
- non-destructive time scrub and footstep/contact markers;
- advanced disclosure without panel redesign.

Exit: edits preserve every authored node, preview matches playback, tight turns produce understandable speed/facing diagnostics, and no world object can cause the actor to leave the curve.

### Phase 4 — surface sampling and stairs

Deliver:

- `ALSurfaceSampler` with normals, identity, confidence, root/foot probes, and exclusions;
- root support hysteresis and step classification;
- compile-time stair/grade/support validation;
- stable root vertical trajectory across terrain, mesh, prims, landings, and stairs.

Exit: the stair acceptance matrix passes at the root/support and animation-presentation level with optional contact polish disabled.

### Phase 5 — natural animation presentation

Deliver:

- locomotion profile and gait hysteresis;
- bounded cadence/orientation/stride adjustments;
- start, stop, turn, reverse, and step transition blending;
- curve-look-ahead signals for hips/chest/head presentation;
- custom-animation metadata and safe fallback;
- optional surface/contact hints to the existing Pose Polish pass only if stair tests prove they materially improve the result.

Exit: the avatar appears to understand and naturally follow shallow curves, S-curves, tight turns, stops, reversals, slopes, and stairs with contact polish disabled; enabling existing polish may improve feet but cannot be required for acceptance.

### Phase 6 — choreography tools

Deliver in independent slices:

- time scrub and route heatmap;
- behavior anchors;
- record-to-route;
- authored formations and timing relationships;
- advanced arrival composition;
- curve transformation tools and explicit traversal-action framework.

Exit: each tool preserves deterministic asset revisions, undo, persistence, and the UI freeze contract.

---

## 14. Test strategy

### 14.1 Pure unit tests

- centripetal spline evaluation, derivative, arc length, and degenerate nodes;
- curvature estimation and speed-envelope forward/backward passes;
- yaw unwrap, rate/acceleration limiting, and arrival-facing blend;
- authored-node interpolation, tangent continuity, and degenerate-segment handling;
- surface-candidate scoring and hysteresis;
- step classification and profile limits;
- root-support transitions, step classification, and optional existing contact-polish regressions;
- schema round-trip, malformed input, and version migration;
- editor state machine including capture loss and stale node revision;
- deterministic trajectory hash.

Extend `alposepolish_test.cpp` only if the optional existing contact pass is given new hints. Core curve and stair acceptance must not depend on it.

### 14.2 Synthetic curve and surface fixtures

Construct deterministic authored paths and support geometry for:

- two-node straight path;
- shallow S curve and tight S curve;
- 90°, hairpin, and near-180° turns;
- repeated, coincident, and extremely short nodes;
- deliberate linear hard corner;
- closed loop with continuous and discontinuous seams;
- ping-pong turnaround;
- flat plane, slope boundary, and stair flight/landing;
- abrupt height change beyond the step profile;
- path edited while an older revision is playing.

Assert exact node passage, bounded arc-length error, tangent/curvature results, speed envelope, facing continuity, slope/step warnings, endpoints, and deterministic output. Confirm that support geometry changes Z/pose only and never changes authored XY.

### 14.3 Runtime integration tests

- Move with and without an existing Path asset;
- Path with Move defaults changed immediately before playback;
- simultaneous actors and staggered starts;
- Stop/release, suspend/resume, loop, ping-pong, follow, Walk-to, and runtime actor replacement;
- edit asset while another revision plays;
- scene save/load/rebind and missing actor;
- region-coordinate boundary and teleport cancellation;
- custom animation success, missing asset, and malformed metadata;
- low FPS, hitches, time dilation, and repeated take.

### 14.4 Preview isolation tests

Keep a snapshot of:

- actor root transform;
- animation/playback state;
- motion asset revision and node array;
- compiled trajectory hash;
- preview node transforms.

Run only preview enable/disable, hover, selection, camera, and UI focus actions. Assert the first four are unchanged and preview destinations remain fixed. These tests specifically guard the user-reported “hover over the path icon / ghost walks away” failure.

### 14.5 Performance budgets

Measure before fixing numeric budgets. Initial engineering targets:

- no route search, obstacle steering, or curve rebuild per frame;
- no allocations in per-actor traversal after warm-up;
- surface query count bounded per active actor and visible in diagnostics;
- preview pose captured on invalidation, not rebuilt for every ghost/node/frame;
- compiled trajectory memory proportional to path length with a documented sampling ceiling;
- worker results cancel promptly when path revision changes;
- no render-thread wait on surface preflight.

Performance telemetry should report curve/trajectory compile time, sample count, surface-query count, cache hit rate, snapshot size, and per-actor traversal/pose time.

---

## 15. Diagnostics and observability

Add one `ActorMotion` log category with actor id, asset id, revision, instance id, and phase. Avoid unstructured per-frame spam.

Useful events:

- compile requested/cancelled/published;
- curve/profile version chosen;
- compile status and first warning;
- playback start/stop/suspend/rebind;
- surface-support switch and confidence loss;
- step-up/down classification;
- gait/animation transition and optional contact-polish transition;
- preview capture/invalidation/fallback;
- editor press/drag/commit/cancel;
- forbidden or unexplained transform write.

Developer overlay layers:

- geometric curve and trajectory samples;
- authored nodes and tangent handles;
- speed/curvature heatmap;
- surface normal/confidence;
- root support and optional left/right support hints;
- optional existing contact locks when that feature is enabled;
- facing look-ahead and arrival target;
- compiled revision versus playing revision.

Every overlay is read-only and disabled by default.

---

## 16. Risks and mitigations

| Risk | Consequence | Mitigation |
|---|---|---|
| Curve smoothing changes the director's intent | Actor misses a mark or changes blocking | Every interpolation passes through authored nodes; automatic tangents are visible, reversible, and stored. |
| Requested speed is too high for a tight curve | Skating, snapping, or impossible body twist | Curvature-aware speed envelope, angular limits, and a visible warning when minimum speed is still insufficient. |
| Grounding changes horizontal travel | Actor drifts away from the drawn line | Surface correction owns Z/support only; compiled XY is immutable during playback. |
| Single-ray grounding survives too long | Stair jitter and buried feet | Surface service and root-height acceptance must precede optional limb polish. |
| Scope expands into a new pose solver | Long implementation, fragile avatars, little benefit to curve following | No new IK; naturalness is accepted with existing contact polish disabled. |
| Aggressive warping damages custom animation | Distorted poses | Metadata opt-in, conservative limits, visible diagnostics. |
| Path compile blocks render thread | UI hitch on complex scenes | Immutable snapshots, worker build/query, revision cancellation. |
| UI feature creep | Clean Move/Path workflows are lost | Freeze contract, shared panel XML, screenshot gate, advanced disclosure. |
| Preview captures live moving frame | Ghosts drift again | Frame-matched immutable snapshot and no live fallback during playback. |
| Dynamic SL geometry is incomplete/stale | Ground support changes or disappears | Mark surface samples stale, resample Z/support, and use an explicit pause/fallback without altering XY. |
| Actor scale/skeleton changes invalidate assumptions | Floating root, poor stride, or unreachable stair contact | Profile/skeleton revision in compile key; invalidate and recompile. |

---

## 17. Explicit non-goals for the first V2 release

- redesigning or combining the Move and Path panels;
- replacing the viewer's general avatar animation system;
- physics-authoritative movement or server-side character control;
- automatic jumps, ladders, doors, sitting, or elevators without explicit traversal actions;
- any Havok, navmesh, shortest-path, obstacle-avoidance, or automatic rerouting feature;
- true, full-body, or newly implemented locomotion IK;
- machine-learned locomotion as a prerequisite;
- changing a user-authored horizontal path to keep it away from geometry;
- making path ghosts into live autonomous clones.

---

## 18. Code map and likely change boundaries

| Area | Current files/symbols | V2 responsibility |
|---|---|---|
| Move adapter | `alpanelactormover.{h,cpp}`, `panel_actor_mover.xml` | Build explicit `MoveCommand`; layout unchanged. |
| Path adapter | `alpanelpatheditor.{h,cpp}`, `altoolpathedit.{h,cpp}`, `panel_path_editor.xml` | Edit `MotionAsset`; explicit drag transaction; advanced disclosure. |
| Runtime | `llactormover.{h,cpp}` | Compile coordinator, `MotionInstance`, traversal, explicit start modes. |
| Preview | ghost batching/drawing inside `llactormover.cpp` | Immutable frame-matched path preview snapshots. |
| Scene persistence | `llfloaterdirector.{h,cpp}` | Versioned motion assets/bindings and migration. |
| Surface service | new `alsurfacesampler.{h,cpp}` or equivalent | Support position/normal, step classification, and root/foot probes. |
| Curve compiler | extracted from `llactormover.{h,cpp}` as appropriate | Arc length, tangents, curvature, speed envelope, facing intent, and deterministic samples. |
| Optional contact polish | existing `alposepolish.h`, `alcontactstab.h`, `LLVOAvatar` implementation | Optional surface/contact hints only; no new solver and no core dependency. |
| Tests | `indra/newview/tests/` | Pure geometry, state, schema, contacts, preview invariants. |
| Settings/defaults | `app_settings/settings.xml` | Profiles, debug flags, conservative feature gates. |

Avoid letting `LLActorMover` become the permanent home of curve math, surface sampling, gait, preview, and serialization concerns. Phase 1 should begin extracting contracts even if current code temporarily delegates to legacy functions.

---

## 19. Research notes and source conclusions

### Curves and trajectory following

- Centripetal Catmull–Rom is a strong default for hand-authored curves because of its segment-level cusp/self-intersection guarantees. It still passes through each control point, keeping the director's marks authoritative ([Yuksel et al.](https://www.sciencedirect.com/science/article/pii/S0010448510001533)).
- Turn anticipation should come from look-ahead samples on the same authored curve. It changes facing and speed, never route position.
- Curvature-aware speed is supported by human locomotion observations and is also a practical way to cap lateral acceleration ([Vieilledent et al.](https://www.sciencedirect.com/science/article/pii/S0304394001017980)).

### Grounding and animation presentation

- Robust root support benefits from sphere/area sampling rather than a single ray, especially near stair edges ([Epic foot trace settings](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/FootPlacementTraceSettings?application_version=5.6)).
- Orientation, stride, and slope adaptation are distinct bounded animation-presentation operations ([Epic Pose Warping](https://dev.epicgames.com/documentation/en-us/unreal-engine/pose-warping-in-unreal-engine)). V2 uses that separation as inspiration without requiring a generalized solver.

---

## 20. Open validation questions

These should be answered by small instrumented prototypes and in-world tests, not by redesigning the UI first.

1. What avatar scale/leg-length range is officially supported for stair profiles?
2. Which animation set is guaranteed for start/stop/turn/step events, and which transitions must initially use the standard walk?
3. Can the current raycast/object intersection APIs provide stable face normals and sphere sweeps, or should the first pass combine several ordinary rays?
4. How should transparent, phantom, temporary, flexi, and animated mesh objects participate in **ground support only**?
5. What error tolerances look acceptable at normal machinima camera distance for root vertical motion, foot penetration, and planted drift?
6. What curvature, angular velocity, and angular acceleration limits look natural for walk, jog, and run on the default avatar scale?
7. Should an explicit tangent-handle mode be exposed immediately, or should V2 first ship improved automatic tangents plus diagnostics?
8. Which custom animations can safely accept orientation/stride warping without metadata?
9. How should a stair-support failure behave during a take: preserve last support briefly, pause, or follow raw authored Z?
10. Which existing status line can show `Ready/Warning/Invalid/StaleSurface` without changing panel geometry?

---

## 21. Final recommendation

The fastest credible route to the dream is to keep the path exactly where the director drew it and make the performance along that path progressively better. Begin by making preview and input state trustworthy, then give both preserved panels explicit access to one persistent motion model. Upgrade the current spline into a deterministic curvature-aware trajectory, add anticipatory facing and natural animation-speed matching, and replace the root ray with a stable surface service for slopes and stairs. The existing contact pass remains optional finishing polish.

That sequence produces value at every phase and keeps the artistic workflow stable:

```text
fixed ghosts -> explicit motion semantics -> saved paths -> natural curves
             -> natural turning/speed -> stable stairs -> convincing feet/body
```

The UI stays recognizable. The engine becomes substantially more intelligent. The take remains under the director's control.
