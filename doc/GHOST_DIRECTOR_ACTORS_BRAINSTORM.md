# Ghost Director Actors — Design Brainstorm

Design-only exploration for the working client-only `LLGhostAvatar` entity clone.

## Non-negotiable boundary

Every proposal below preserves the clone as a viewer-local entity:

- `mIsLocalOnly` is established before parenting, selection, or pipeline exposure.
- No clone transform, attachment, animation, material, extra-param, path, cast, or persistence operation may emit simulator traffic.
- Extra-param copies retain `local_origin=false`; any generic parameter-change send remains guarded by `!isLocalOnly()`.
- Director/cast/path IDs for ghosts are synthetic viewer IDs, never simulator identities.
- Teardown happens on the viewer thread outside protected pipeline traversals, using the existing deferred `markForDeath()` discipline.

This is an architectural boundary, not a setting. Saved scenes describe local reconstruction recipes; they do not turn ghosts into simulator objects.

## What the repo already gives us

There are currently two different meanings of “ghost”:

1. `LLGhostAvatar` is a real client-only scene entity. `/ghostdress` creates one, clones attachments, mirrors avatar and object-animation state, and renders through the normal avatar scene path. Its UUID is held only in file-static `sTestGhostIds` in `llghostavatar.cpp`.
2. `ALGhostStudio::Instance` is the production Studio model for styled geometry re-draws. It stores source, placement, style, pose, and FX; `LLActorMover::renderStudioGhosts()` consumes it. It does not own an `LLGhostAvatar`.

The correct unification is therefore one Director-facing registry with explicit backing kinds, not pretending the two render mechanisms are identical.

A useful model extension is conceptually:

```text
Instance
  stable instance id
  kind = OVERLAY | ENTITY_CLONE
  source id + cached source label
  enabled/style/pose/transform
  entity object id (ENTITY_CLONE only; weak UUID, never strong static lifetime)
  owned reconstruction/snapshot state (future)
  lifecycle state = SPAWNING | READY | SOURCE_MISSING | LOCKED | TEARING_DOWN | ERROR
```

The stable `mId` remains the Director/scene identity. `mEntityId` is the current runtime `LLGhostAvatar` identity and may change after load/reconstruction. This distinction prevents saved paths, cast references, and UI selection from breaking when an entity is respawned.

## Ranked ideas

### 1. One Ghost Registry: `/ghost*` enters Ghost Studio

**What/use case:** Every `/ghostdress` clone immediately appears in the Director’s Ghosts list, can be selected, enabled, inspected, and cleared from the same console used for the rest of a shoot.

**Design:** Make `ALGhostStudio` the owner of production entity-clone records. Add an entity-backed creation path which first allocates an `Instance`, then creates/clones the `LLGhostAvatar`, and commits its weak object UUID only after successful setup. `/ghostdress` calls that service instead of pushing directly into `sTestGhostIds`. Keep a deliberately named harness registry only for `/ghosttest` control/test pairs. `/ghostverify` should enumerate entity-backed Studio instances plus explicit harness ghosts when asked for test scope. `/ghostclear` should mean “clear entity clones created by chat” or, preferably, expose `/ghostclear [all|selected|test]` so it cannot unexpectedly remove overlay set dressing.

The Director list adds a `Kind/State` signal: Entity, Overlay; live, frozen, locked, source missing, loading, error. Existing columns already cover enabled, source, style, and pose. Double-click enabled toggling and delete/clear route through Studio lifecycle APIs. Style for an entity clone initially means normal scene-lit clone versus explicitly supported per-instance render modifiers; unsupported overlay-only styles are disabled or labelled, not silently approximated.

**Reuse map:** `llghostavatar.{h,cpp}` creation, clone, verification, deferred death; `alghoststudio.{h,cpp}` instance CRUD/selection; `alpanelghoststudio.{h,cpp}` list and controls; `llfloaterdirector.{h,cpp}` and `alfloaterdirectory.{h,cpp}` shared panel hosting; `altoolghostplace.h` and the existing ghost manipulation proxy design; `llactormover.{h,cpp}` render/update phase.

**Effort/risk:** **M.** Main risk is double ownership or destroying an avatar during pipeline traversal. The Studio stores UUIDs, never a process-lifetime `LLPointer`; removal is two-phase and idempotent. A failed spawn must roll back the record. “Enabled off” should hide/suspend locally, not kill and rebuild every toggle. Absolutely no generic object update or selection path may send to the sim.

**Dependencies/order:** First. It establishes the identity and lifetime seam required by actor wiring, persistence, and multi-clone tools.

### 2. Ghost Actor Adapter: a stable Director actor handle

**What/use case:** Add an entity clone to the cast and operate it everywhere a normal actor is accepted, without teaching each floater a second ghost-specific movement engine.

**Design:** Introduce a small actor-resolution layer that maps a stable Director actor handle to the runtime `LLGhostAvatar`. Today `LLDirectorCast::resolve()` already resolves any live avatar-shaped object through `gObjectList`, and `LLActorMover` already keys state by resolved UUID. For session-only v1, the clone’s runtime UUID can be admitted directly to the cast. For persistence-ready v2, cast/path/follow/gaze records should key on a stable handle (the Studio `Instance::mId`) and resolve it through Studio to `mEntityId`.

The adapter exposes capabilities—avatar skeleton, local-only transformable, animation-controllable, source-backed/owned—rather than branching on “real avatar versus ghost” throughout UI code. Cast rows label entity clones as `Ghost: <source> #N`.

**Reuse map:** `lldirectorcast.{h,cpp}` resolution and cast model; `llactormover.{h,cpp}` `path_key`, `mMoves`, `mPaths`, `mFollows`, `mGazes`; `alpanelactormover.{h,cpp}`; `llfloateractormover.h`; `alfloaterdirectory.{h,cpp}` and `llfloaterdirector.{h,cpp}`.

**Effort/risk:** **S** for direct runtime UUID admission, **M** for the stable-handle resolver worth adopting. Risk is stale runtime UUIDs after teardown/load and accidentally feeding synthetic IDs into name cache, mute, telemetry, or server-facing cast actions. Capability gates and ghost labels must keep those paths local.

**Dependencies/order:** Requires idea 1. Stable handles should precede durable scene persistence.

### 3. Walk, Place, and Path a Clone

**What/use case:** Pose a clone at a mark, draw a Catmull–Rom path, and have it walk the blocking exactly for repeatable machinima.

**Design:** Reuse `LLActorMover` directly. `LLGhostAvatar` is already an `LLVOAvatar` with a real root joint, skeleton, and drawable; `LLDirectorCast::resolve()` can already return it by object UUID. The minimum wiring is:

1. make it resolvable through the stable actor handle;
2. admit it to the cast/path panels;
3. ensure the ghost’s avatar update reaches `LLActorMover::applyOverride()` and `applyGaze()` in the same ordering as a normal avatar;
4. on Studio removal, call a single `forgetActor(handle)` teardown which stops moves and erases path edit selection, follow edges, gaze state, marks, and cast membership before deferred entity death.

Do not move every cloned attachment individually. Drive the avatar root through the existing rendered-root override; attachments and private `LLControlAvatar` animated-object skeletons inherit the clone hierarchy. Path position is authoritative while walking; the Studio transform mirrors it for UI/persistence rather than competing with it.

**Reuse map:** `llactormover.{h,cpp}` placement, path traversal, cadence, ground follow, gaze; `alpanelpatheditor.{h,cpp}`; `altoolpathedit.{h,cpp}`; `altoolghostplace.{h,cpp}`; `/pathadd`, `/pathwalk`; `alpanelactormover.{h,cpp}`.

**Effort/risk:** **M.** The technical risk is two transform writers (Studio gizmo and mover) causing jitter, and update ordering between mirrored source animation and locomotion/root/gaze overrides. Define explicit authority: gizmo/mark while idle, path mover while walking, recorder when scrubbing. Per-frame cost is already proportional to actors; clones add normal avatar and attachment cost.

**Dependencies/order:** Ideas 1–2. This unlocks 4–7 and most creative choreography.

### 4. Clone Pose and Animation Control

**What/use case:** Turn a clone into a mannequin or performer: mirror live motion, play a chosen pose/animation, blend back to live, or freeze at the current frame.

**Design:** Give entity instances an animation drive mode: `MIRROR_SOURCE`, `DIRECTED_CLIP`, `RECORDED_STATE`, `FROZEN_FRAME`. `LLGhostAvatar::idleUpdate()` currently copies the source’s `mSignaledAnimations`; move that behind the drive-mode decision. Directed clips operate only on the clone’s motion controller. Keep root locomotion owned by Actor Mover and layer gaze after motion evaluation. For animated attachments, mirror or replay the per-cloned-prim `ObjectAnimation` map under the same timeline authority.

**Reuse map:** `llghostavatar.{h,cpp}` mirrored `mSignaledAnimations`, `processAnimationStateChanges()`, cloned-linkset source mapping, `LLObjectSignaledAnimationMap`, cloned `LLControlAvatar`; Director Animate tab in `llfloaterdirector.{h,cpp}`; existing pose/frozen fields in `ALGhostStudio::Instance`; `LLActorMover::applyGaze()`.

**Effort/risk:** **M.** Motion priority/shared asset state and deterministic local time are the main risks. Never mutate a shared cached motion asset globally. Teardown must stop clone-local motions and erase synthetic object-animation map entries. Scrub requires evaluation at an explicit time, not merely start/stop packets.

**Dependencies/order:** Ideas 1–3. A small directed-pose slice can ship before full timeline recording.

### 5. ACTION-Synchronized Ghost Cast

**What/use case:** One ACTION command launches clone paths, directed animations, follow chains, and the camera take from the same clock for repeatable takes.

**Design:** Treat Director ACTION as a transport coordinator. At countdown completion, latch one monotonic take epoch and hand it to camera, actor paths, and ghost animation timelines. CUT stops/holds all armed local channels. Avoid multiple subsystems independently sampling wall-clock start times. Existing `Path::mSyncToTake` already derives actor arc from `LLFlycamRecorder::getPlayhead()`; generalize that playhead into a read-only Director transport clock or let other tracks consume it through a small interface.

**Reuse map:** `lldirectorcast.cpp` `fireAction()` and CUT/reset marks; `aldirectorhotkeys.{h,cpp}` F3/F4/F5/F7/F8; `LLFlycamRecorder`; `LLActorMover::Path::mSyncToTake`; Director Takes/Move/Animate panels.

**Effort/risk:** **M.** Risk is clock ownership and behavior on pause, seek, loop, ping-pong, and CUT. Define a state table before code. All effects remain viewer-local.

**Dependencies/order:** Ideas 3–4. It should precede elaborate multi-track recording.

### 6. Follow-Leader, Look-At, and Directed Ensembles

**What/use case:** Stage walk-and-talks, processions, dancers, and eyelines using multiple clones as a coherent cast.

**Design:** Expose existing follow and gaze controls for ghost handles. Clone B can follow clone A’s path by reference and offset; gaze targets accept camera, fixed point, real cast member, or ghost handle. Add cast groups and a “clone this blocking” action that duplicates path plus optional time/spacing offsets while keeping a shared master path reference.

**Reuse map:** `LLActorMover::Follow`, `setFollow()`, `advanceFollower()`, `Gaze`, `applyGaze()`; `alpanelpatheditor.{h,cpp}` follow/gaze UI; `LLDirectorCast`; `ALGhostStudio::makeArray()`.

**Effort/risk:** **S–M** after actor support. Risk is dangling follow/gaze edges and update order in chains. Existing cycle prevention should operate on stable handles. Large casts multiply avatar, attachment, animation, and look-at costs.

**Dependencies/order:** Ideas 2–3; ACTION sync optional but highly complementary.

### 7. Scene Recipe Save/Load

**What/use case:** Reopen a project and reconstruct ghost blocking—source, appearance mode, style, pose, transform, path, gaze/follow links, and take bindings.

**Design:** Save a versioned LLSD scene recipe keyed by stable Studio instance IDs. Store:

- source UUID and cached display label;
- backing kind and reconstruction mode;
- enabled/style/FX/pose/scale/foot/rotation;
- path authored data (`Waypoint` fields and path parameters), follow/gaze, marks, groups;
- animation drive mode and clip/timeline reference;
- worn snapshot manifest, initially asset IDs and material values, not live object pointers;
- take binding and offsets.

Load is transactional: parse/validate, allocate stable records, resolve all cross-references, then spawn entities. Unresolved live sources remain visible as `SOURCE_MISSING`; the user may relink, retry, or use an owned snapshot tier. Never deserialize raw runtime object IDs as authority.

**Reuse map:** `ALGhostStudio`; `LLDirectorCast` scene serialization; `LLActorMover::Path` accessors; `LLFlycamRecorder::saveToFile/loadFromFile()` LLSD XML/versioning pattern; Director Directory/scene UI.

**Effort/risk:** **L.** Risks are format migration, asset availability/permissions, partial loads, and stable cross-reference remapping. Saving a source UUID is not persistence of its appearance. Scene load must only construct local objects and never rez or update simulator objects.

**Dependencies/order:** Stable handles from idea 2; basic directed animation schema from idea 4. Can ship recipe-only before locked snapshots.

### 8. Animation-State Recorder and Scrubber

**What/use case:** Record what a source and its animated attachments did, then scrub, loop, offset, freeze, and replay that performance on one or many clones.

**Design:** Follow the Flycam Recorder’s transport/UI pattern but record semantic animation state, not per-frame joint matrices by default. At change boundaries or a capped sample rate, capture:

- avatar signaled animation set with animation IDs, sequence/order metadata available locally, and normalized/local elapsed time;
- optional root-motion/Studio transform channel;
- each cloned source object’s `ObjectAnimation` map, mapped to a stable cloned-linkset/prim key;
- explicit discontinuity markers when assets or source objects are unavailable.

Playback evaluates the state at the Director playhead. Freeze holds an evaluated state without advancing. Loop/ping-pong and per-clone offsets are timeline transforms. For faithful scrubbing, add clone-local explicit-time motion evaluation; simply restarting animations on every slider move will pop and will not be deterministic. A dense “baked joint palette” recording is a later fallback for motions that cannot seek cleanly.

**Reuse map:** `alpanelflycamrecorder.{h,cpp}` shared transport-panel pattern; `llflycamrecorder.{h,cpp}` state machine, playhead, seek, LLSD I/O; `llghostavatar.cpp` mirrored avatar/object animation state; `LLControlAvatar`; Director Takes/Animate tabs.

**Effort/risk:** **L** semantic recorder; **XL** for baked palettes and guaranteed arbitrary-time evaluation. Risks are memory growth, motion cache/shared-state contamination, non-seekable procedural motions, and object-animation identity changes. Sampling every joint of every clone every frame is not the near-term design.

**Dependencies/order:** Ideas 4–5; scene persistence for durable recordings.

### 9. Persistence Tiers

**What/use case:** Let directors choose the right fidelity/cost point for a shot instead of treating “persistent” as one misleading checkbox.

**Tier A — Live mirror:** Save the recipe and reconnect to a present source; appearance and animation remain live. **Effort S/M**, cheap memory and implementation. Risk: source leaving or changing breaks fidelity.

**Tier B — Frozen pose:** Store placement plus copied palette values and frozen attachment matrices, expanding the existing `POSE_FROZEN` concept to the entity clone and retaining enough asset/material descriptors to reconstruct when the source remains asset-resolvable. **Effort M/L.** Risk: current frozen Studio materials and geometry can still reference live mutable data; frozen pose is not a locked appearance.

**Tier C — Fully owned locked clone:** Deep-copy geometry/VB content, material values, texture bindings/content where required, joint palettes, attachment hierarchy, and mutable BOM/baked texture pixels into clone-owned resources. It survives source departure and later mutation. **Effort XL.** Risk: GPU/CPU memory, texture readback/copy timing, PBR/GLTF material graph completeness, flexi/animesh/procedural state, resource eviction, context loss, and clean destruction. References are explicitly insufficient: vertex-buffer or texture identity does not imply immutability.

**Reuse map:** `ALGhostStudio::POSE_FROZEN`, palette/static attachment captures; `LLGhostAvatar` attachment/entity ownership; clone fidelity audit briefs for distinguishing value equality from live identity; renderer VB/material/texture abstractions; scene LLSD recipe.

**Dependencies/order:** A follows ideas 1/7; B follows pose control; C is a big bet after profiling and a narrowly scoped locked-avatar prototype.

### 10. Time Echo / Onion-Skin Performer

**What/use case:** Show past and future poses around the current actor for dance analysis, supernatural trails, or blocking continuity.

**Design:** Sample a recorded animation/path timeline at offsets around the current playhead and display a bounded number of ghost actors. Use low-cost overlay Studio ghosts for editing onion skins and entity clones only for final scene-lit frames that need true PBR/shadows. Provide count, spacing in seconds, past/future/both, falloff, and “bake selected echoes to cast.”

**Reuse map:** `LLActorMover` path onion-skin/render ghost pipeline; `ALGhostStudio::makeArray()` concepts; animation recorder; path editor visualization; entity clone scene path.

**Effort/risk:** **M** overlay preview, **L** scene-lit entity echoes. Risk is explosive per-frame avatar/attachment cost. Hard budgets, LOD policy, and shared immutable resources are mandatory.

**Dependencies/order:** Animation recorder for temporal pose accuracy; actor paths for positional echoes. Overlay preview can start earlier.

### 11. Bullet-Time Freeze Strip 2.0

**What/use case:** Build a row or arc of scene-lit frozen performances along a camera move—the classic bullet-time tableau, now with actual worn mesh, PBR, animesh, and matched LOD.

**Design:** Sample one performance timeline at authored times, create frozen instances at corresponding path/camera-relative transforms, and expose a strip editor: sample count, time range, spatial curve, facing rule, and fade/style ramp. Seeded per-instance variation may alter pose time, hue, glitch, scale, or prop visibility reproducibly. Preview as cheap overlays; promote chosen frames to entity or locked tiers.

**Reuse map:** Flycam Recorder playhead/path; `ALGhostStudio::makeArray()`; frozen palette capture; seeded chaos backlog; path editor Catmull–Rom evaluation; clone entity renderer.

**Effort/risk:** **L**, **XL** if every frame is fully owned. Risk is memory/render cost and capturing a coherent source frame. Creation must be queued outside pipeline traversal.

**Dependencies/order:** Ideas 8–10; benefits strongly from persistence tiers.

### 12. Crowd and Cast Factory

**What/use case:** Turn one performer into extras, formations, audience rows, patrols, or a chorus with controlled variation instead of random chaos.

**Design:** A Director “Cast Factory” duplicates a prototype into line/ring/grid/spline/scatter layouts. Variation is deterministic from `(scene seed, instance stable id)` and can affect animation phase, speed, gaze target, scale within safe limits, color/style, start delay, and path lateral offset. Offer linked versus independent blocking and a performance budget estimate before creation.

**Reuse map:** `ALGhostStudio::makeArray()`; `LLActorMover` copy/follow/path APIs; Director cast/groups; seeded chaos backlog; path templates; Ghost panel arrays.

**Effort/risk:** **M.** Risk is per-frame cost and visually obvious identical asset reuse. Never vary shared source state; all parameters are per instance. Teardown/group delete must be transactional.

**Dependencies/order:** Ideas 1–6. High creative payoff after basic actor support.

### 13. Pose Library and Digital Stand-Ins

**What/use case:** Save named blocking poses—eyeline, handoff, doorway, lighting mark—and drop a clone mannequin into a shot before the performer arrives.

**Design:** Save pose assets as directed animation/time plus optional joint override values and prop/attachment manifest. A stand-in can use a lightweight overlay while blocking and promote to an entity clone once a source or owned appearance is available. Include height/bounds and camera-framing guides.

**Reuse map:** Ghost frozen pose; Director Animate tab; actor marks/path editor; `altoolghostplace`; scene recipe; animation scrubber.

**Effort/risk:** **M** for clip/time poses, **L** for robust joint-pose snapshots. Risk is skeleton compatibility and joint overrides leaking/shared motion mutation.

**Dependencies/order:** Ideas 4 and 7. Can precede full animation recording if limited to a clip UUID plus time.

### 14. Lighting / Focus / Occlusion Stand-In

**What/use case:** Use a clone as a physically representative lighting, shadow, depth-of-field, and framing stand-in while the live actor is elsewhere.

**Design:** Add a “stand-in” presentation mode with optional neutral clay/material override, silhouette, bounds, and focus target—but retain the same geometry/LOD when judging shadows and occlusion. Provide an A/B toggle between faithful PBR and clay diagnostic views. The clone never becomes a light; it is a scene-lit subject proxy.

**Reuse map:** Entity clone normal scene render path; PBR/GLTF and matched LOD foundation; Director camera tools; Ghost style/brightness controls.

**Effort/risk:** **S/M.** Risk is misleading results if diagnostic material replacement changes alpha/double-sided behavior or if clone LOD diverges. Keep “lighting-faithful” and “diagnostic clay” explicitly distinct.

**Dependencies/order:** Idea 1 only; a good small creative follow-up.

### 15. Wardrobe Continuity Vault

**What/use case:** Capture “hero enters in Look A” and compare or restore visual continuity across pickups without relying on the performer still wearing that outfit.

**Design:** Near-term, save a worn manifest and thumbnails/metadata; reconstruct from locally available assets when source is present. Long-term, promote a vetted snapshot to fully owned locked resources for frame-stable continuity. Diff two snapshots by attachment hierarchy, mesh/material IDs and material values.

**Reuse map:** `LLGhostAvatar::cloneAttachmentsFrom()` and source mappings; clone fidelity audit value snapshots; scene persistence; Directory/Director asset UI.

**Effort/risk:** **L**, reaching **XL** for genuinely locked BOM and geometry. Permissions, missing assets, baked texture mutability, and memory are the central risks.

**Dependencies/order:** Scene recipe first; owned snapshot research before promising restore fidelity.

### 16. Clone-to-Camera Blocking Tools

**What/use case:** Automatically place cast marks for over-the-shoulders, eyeline matches, turntables, lineup slates, and repeatable coverage.

**Design:** Add Director commands such as “face camera,” “look at Subject A,” “place on thirds,” “orbit formation,” and “match previous silhouette.” These author ordinary Studio transforms, actor gaze, or paths; they are not a new runtime. A camera-relative formation can optionally bake to global positions at ACTION for deterministic retakes.

**Reuse map:** Flycam/current render camera; `ALToolGhostPlace`; Actor Mover gaze; Studio transform setters/manip proxy; Director Subject A/B and camera panels.

**Effort/risk:** **S/M.** Risk is confusing live camera-relative constraints with baked marks. Make constraint state visible and avoid two transform authorities.

**Dependencies/order:** Actor adapter and transform authority rules.

## Recommended architecture decisions

### Registry ownership

Use Studio-owned records and weak runtime UUIDs. Do not make the Director merely scan all `LLGhostAvatar` objects every refresh: enumeration is a useful recovery/debug fallback, but it loses source/style/pose intent, cannot represent spawning/error state cleanly, and makes ownership ambiguous.

### Stable IDs

Separate stable instance/actor ID from runtime viewer-object ID now. Direct UUID admission is tempting and cheap, but durable paths and cross-references otherwise break on every reconstruction.

### One actor engine

Do not create `LLGhostActorMover`. Adapt resolution and lifecycle, then reuse `LLActorMover`, `ALPanelPathEditor`, `ALToolPathEdit`, follow, gaze, sync-to-take, marks, and ACTION. The clone is already the avatar-shaped runtime object these systems want.

### Transform authority

At most one writer owns root placement per frame:

```text
idle          -> Studio transform/gizmo or mark hold
path playing  -> Actor Mover
take scrub    -> Director transport evaluation
locked/frozen -> stored evaluated transform
```

Other models mirror the winning value for UI and save, never fight it.

### Lifecycle teardown

Centralize `removeInstance()`:

1. mark record `TEARING_DOWN` and disable selection/render/update;
2. stop Director transport participation;
3. remove follow/gaze edges, paths/moves/marks/cast references;
4. erase cloned object-animation map entries and stop clone-local motions;
5. release cloned attachments;
6. request deferred `LLGhostAvatar::markForDeath()`;
7. clear runtime UUID and finally remove/tombstone the record.

All steps are idempotent, viewer-thread-only, and safe when source/runtime objects have already vanished.

### Performance policy

Expose quality budgets per group: maximum active entity clones, attachment/triangle estimates, forced/matched LOD choice, animation update rate for distant extras, shadow participation, and overlay-preview promotion. Never silently violate source-matched LOD for a hero clone; instead show the cost and let the director choose preview versus final.

## Ranked near-term slate

1. **One Ghost Registry (M):** route `/ghostdress` into an entity-backed `ALGhostStudio::Instance`; list/select/toggle/delete/clear it in the Director. This fixes the immediate product inconsistency.
2. **Stable Ghost Actor Adapter (M):** resolve stable Studio IDs to runtime clone UUIDs and add entity clones to cast without server-facing behavior.
3. **Walk/Place/Path Clone (M):** reuse Actor Mover and Path Editor; define transform authority and centralized teardown. This is the highest creative payoff.
4. **Directed Pose/Clip (M):** mirror/live versus selected local clip versus frozen frame, including safe cleanup.
5. **ACTION Sync (M):** one playhead/epoch for paths, clone animation, and flycam takes.
6. **Ensemble Controls (S–M):** follow-leader, look-at, stagger, group path copy.
7. **Scene Recipe v1 (L):** stable IDs, transforms, styles, paths, gaze/follow, clip references, source manifest; honest `SOURCE_MISSING` handling.
8. **Cast Factory + Stand-In Tools (M):** deterministic crowd variation and lighting/blocking stand-ins with cost preview.

This ordering gets a single `/ghostdress` clone into the Director quickly, then turns it into a useful actor before taking on resource ownership and full recording.

## Big bets

### Fully Owned Locked Clone

A genuinely immutable, source-independent actor with owned geometry/VBs, material values, palettes, attachment state, and copied mutable baked texture content. This is the continuity and archival breakthrough, but it is **XL** and should begin as a measured one-avatar/one-frame prototype with explicit GPU/CPU accounting and context-loss tests.

### Performance Time Volume

Record one performance once, then instantiate any number of independently scrubbed temporal actors: echoes, bullet-time strips, alternate timing, reversible motion, and camera-linked retiming. Start semantic and sparse; offer baked palettes only as an expensive “finalize performance” operation. This is **XL**, but it turns the clone foundation into a genuinely new machinima medium rather than just duplication.

## Bottom line

The immediate win is not a new renderer. It is admitting the working `LLGhostAvatar` into the production Studio/Director identity and lifecycle model. Once that happens, the existing actor mover already supplies most of the desired direction vocabulary—paths, marks, gaze, follow, camera synchronization, undo, and ACTION. The design work is chiefly stable identity, transform/time authority, and teardown discipline. Full persistence is a ladder: live recipe first, frozen values next, and only then the expensive owned locked clone.
