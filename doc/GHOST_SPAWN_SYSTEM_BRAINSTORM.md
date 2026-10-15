# Ghost Studio Spawn and Placement System — Brainstorm

Date: 2026-10-14  
Status: design exploration only; no implementation commitment

## Product intent

- Keep every Ghost Studio capability, but make placing one clone or a crowd feel like a polished game editor.
- Preserve creative freedom: clones may overlap, touch, form dense piles, or span an arbitrarily broad area.
- The only placement condition that should refuse a final commit is a foot point below loaded region terrain.
- Everything else—overlap, walls, missing surface support, high density, distance, cost, or region-edge risk—is advisory.
- Give a warning, preview, correction command, or optional auto-fix; never silently rewrite or forbid an authored choice.
- Keep the feature viewer-only. No simulator rez, object update, capability call, or new network protocol.

## Rating key

- Impact: **H** changes the everyday feel, **M** removes recurring friction, **L** is finishing polish.
- Effort: **S** localized, **M** spans several viewer systems, **L** needs new models/render paths or persistence.
- Touch: **R** rendering, **P** picking/raycasting, **N** networking.
- `N: No` is intentional on every item; source appearance may use data the viewer already has, but placement sends nothing.

## Current implementation: what exists today

### Architecture and clone types

- `ALGhostStudio` is the session-scoped, UI-free model; positions are global so instances survive region-frame changes (`indra/newview/alghoststudio.h:15`, `indra/newview/alghoststudio.h:25`).
- It supports overlay ghosts and scene-lit entity clones through `BACKING_OVERLAY` / `BACKING_ENTITY_CLONE` (`indra/newview/alghoststudio.h:85`).
- Overlay Add copies the selected source's feet and world orientation into a new instance (`indra/newview/alghoststudio.cpp:742`).
- Entity Add resolves a source and region, starts a `STATE_SPAWNING` record, offsets it 2.5 m forward, creates a local runtime, then commits or rolls back transactionally (`indra/newview/alghoststudio.cpp:767`).
- Entity refresh builds a replacement runtime and preserves the existing clone if the replacement fails (`indra/newview/alghoststudio.cpp:858`).
- `LLGhostAvatar` is a local-only, non-dummy avatar that uses the real scene avatar path for deferred lighting, shadows, fog, tonemapping, and post effects (`indra/newview/llghostavatar.cpp:968`).
- It creates its own drawable because no simulator `ObjectUpdate` will arrive (`indra/newview/llghostavatar.cpp:1001`).
- Its explicit foot-position API maintains stable foot placement independently of avatar-root details (`indra/newview/llghostavatar.cpp:1062`).
- Scale is centrally bounded to `0.05–150` (`indra/newview/alghoststudio.h:56`).
- Runtime controls already include visibility, looks, drive modes, true mirror, cloned appearance, and per-frame animation (`indra/newview/llghostavatar.cpp:1308`, `:1396`, `:1472`, `:1572`, `:1836`, `:3428`).

### Shared Ghost Studio UI

- The Director Ghosts page embeds `panel_ghost_studio.xml` (`indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:3`).
- The standalone Ghost Studio floater embeds the same panel, so there is one functional UI definition (`indra/newview/skins/default/xui/en/floater_ghost_studio.xml:18`).
- The panel exposes Add, Duplicate, Delete, and Refresh (`indra/newview/skins/default/xui/en/panel_ghost_studio.xml:40`).
- Transform controls include position, yaw, heading dial, scale, Place, To actor, To me, Drop to ground, Align feet, Upright, Duplicate here, and transform copy/paste (`indra/newview/skins/default/xui/en/panel_ghost_studio.xml:70`, `:85`, `:98`, `:104`).
- Scale and yaw are precise spinner paths; scale is `0.05–150` in the XUI too (`indra/newview/skins/default/xui/en/panel_ghost_studio.xml:75`, `:82`).
- Style, pose, true mirror, AutoAnimate, look-at/turn, nameplates, chaos, animation, and entity diagnostics are already part of the shared kit (`indra/newview/skins/default/xui/en/panel_ghost_studio.xml:121`, `:210`, `:225`, `:263`, `:445`).
- `ALPanelGhostStudio` owns UI callbacks and a per-panel placement owner, plus a session transform clipboard (`indra/newview/alpanelghoststudio.h:48`, `:422`, `:447`).

### Single-clone placement and edit mode

- Place arms `ALToolGhostPlace` for the selected clone/group, exits edit mode, and installs a one-shot transient tool (`indra/newview/alpanelghoststudio.cpp:2703`).
- A click runs the shared resolver, then immediately transforms the selected unit or group and returns the camera (`indra/newview/altoolghostplace.cpp:114`).
- The resolver ladder is visible surface → terrain/water → work plane → camera depth (`indra/newview/alghostplacementresolver.h:134`).
- A visible-surface hit can already be a prim/floor/stage; the gap is continuous preview and per-member surface conform, not basic prim picking.
- The resolver records a surface normal, but current single placement does not expose an upright/align-to-normal choice (`indra/newview/alghostplacementresolver.h:55`).
- Place has no hover model beyond the create cursor; the transform happens on mouse-down (`indra/newview/altoolghostplace.cpp:209`).
- Esc and right-click cancel cleanly (`indra/newview/altoolghostplace.cpp:215`, `:223`).
- To actor and To me preserve the selected unit's rotation and scale while replacing its foot position (`indra/newview/alpanelghoststudio.cpp:2722`, `:2759`).
- Drop to ground samples land height only; it does not find a prim floor, stage, stair, roof, or seat (`indra/newview/alpanelghoststudio.cpp:2792`).
- Align feet copies the first selected unit's Z plane, and Upright strips pitch/roll while retaining yaw (`indra/newview/alpanelghoststudio.cpp:2806`, `:2827`).
- Copy/paste stores foot position, full rotation, and scale and applies it to selected units (`indra/newview/alpanelghoststudio.cpp:2844`).
- Edit mode selects an enabled ghost by projecting its foot and a scale-aware mid-body point, using an 18 px radius (`indra/newview/altoolghostedit.cpp:23`, `:62`).
- It then hands control to the stock manipulation proxy/gizmos; Delete removes the unit and Esc exits (`indra/newview/altoolghostedit.cpp:105`, `:168`).
- Groups already have transform APIs and can be rigid or move/face units (`indra/newview/alghoststudio.cpp:2845`, `:2989`, `:3020`).

### Arrays, formations, and crowd preview

- Line and Ring are quick presets that enter the same formation preview path, not separate builders (`indra/newview/alpanelghoststudio.cpp:3128`).
- The crowd UI already offers count, spacing, many shapes, guide shape/size, terrain snap, gap, jitter, seed, facing, copy kind, group mode, Preview, and Confirm (`indra/newview/skins/default/xui/en/panel_ghost_studio.xml:319`, `:329`, `:360`, `:372`, `:383`, `:390`, `:423`).
- The pure deterministic `ALFormationSolver` supports up to 256 members and centralizes layout geometry (`indra/newview/alformationsolver.h:24`, `:28`, `:222`).
- Legacy `makeArray` still exists, while reliable interactive placement uses `FormationSpec`, an owned `CrowdPlacementDraft`, and immutable resolved slots (`indra/newview/alghoststudio.h:200`, `:284`, `:696`).
- `array_build` maps to `onClickBuildArray`, which snapshots a selected ungrouped prototype and starts the draft (`indra/newview/alpanelghoststudio.cpp:3136`).
- The active crowd tool continuously resolves the cursor anchor and reports its basis/validity/warning to the draft (`indra/newview/altoolcrowdplace.cpp:305`).
- Its current interaction is wheel rotate, MMB-drag height, click to pin, second click/Confirm to create, Alt camera, Esc cancel (`indra/newview/skins/default/xui/en/panel_ghost_studio.xml:443`).
- `GhostStudioShowFormationPreview` gates an in-world guide/marker overlay that distinguishes members and prototype (`indra/newview/alghoststudio.cpp:4929`).
- The preview uses exactly the stored transforms later consumed by commit, avoiding preview/commit drift (`indra/newview/alghoststudio.h:793`).
- Commit is transactional: allocate all copies, apply stored transforms, validate the prototype, group if requested, or roll back (`indra/newview/alghoststudio.cpp:4633`).
- Terrain conform is currently terrain-height-only per slot and falls back to the flat formation plane with a warning if terrain data is missing (`indra/newview/alghoststudio.cpp:4395`).
- Seeded variation and chaos already exist; new variation controls should extend those deterministic channels rather than add unrelated RNG (`indra/newview/alpanelghoststudio.cpp:3421`, `indra/newview/alghoststudio.cpp:2412`).

### Chat commands and viewer-only boundary

- Existing `/ghost*` surface includes `/ghosttest`, `/ghostdress`, `/ghostturn`, `/ghostlook`, `/ghostverify`, `/ghostscale`, `/ghostrefresh`, `/ghostchaos`, `/ghostanim`, and `/ghostclear` (`indra/newview/alchatcommand.cpp:408`, `:413`, `:425`, `:527`, `:621`, `:628`, `:694`, `:737`, `:777`, `:895`).
- `/ghostscale` uses the same `0.05–150` contract (`indra/newview/alchatcommand.cpp:628`).
- The placement adapter explicitly treats ghosts as client-only and does not apply simulator land-use/no-build rules (`indra/newview/alghostplacementadapter.h:43`).
- The present adapter does treat “outside any loaded region” as a legality failure, with a clamp into the current region when possible (`indra/newview/alghostplacementadapter.cpp:123`).
- Product decision required: to honor the requested single hard rule literally, unloaded/out-of-region results should become warnings/fallbacks; only below-loaded-terrain should block commit.

## Where the current experience feels rough

- Single Place is click-to-commit with no live clone silhouette; users learn the result after the transform. **Impact H · Effort M · Touch R/P · N: No**
- Crowd preview is strong but abstract: numbered markers/guide strokes do not show body width, look, animation pose, or impending overlap. **Impact H · Effort M · Touch R · N: No**
- An 18 px foot/mid-body screen pick can be hard in dense crowds, extreme scale, or perspective overlap. **Impact H · Effort S · Touch P · N: No**
- Surface normals are captured but not used as an authored placement option. **Impact M · Effort S · Touch P · N: No**
- Drop and crowd terrain conform ignore prim support, so floors, stages, stairs, rooftops, and sitting/elevated surfaces require manual Z work. **Impact H · Effort M · Touch P · N: No**
- Only the anchor uses the rich placement resolver; formation members do not independently raycast against scene geometry. **Impact H · Effort M · Touch P · N: No**
- Numeric edits and gizmos can put clones under a floor or inside objects without a warning; that freedom is good, but feedback is absent. **Impact M · Effort M · Touch R/P · N: No**
- There is no grid, clone, mark, actor-facing, or stage-point snap shared across Place and Edit. **Impact H · Effort M · Touch R/P · N: No**
- Precise yaw/scale exist, but fast wheel/modifier adjustment is split between crowd placement and panel controls. **Impact M · Effort S · Touch P · N: No**
- Add can visibly appear at its default/source-offset position before the user places it; entity appearance work is synchronous enough to risk pop-in or a hitch. **Impact H · Effort L · Touch R · N: No**
- Edits snap to their final transforms; repeated crowd adjustments can feel mechanical rather than directed. **Impact M · Effort M · Touch R · N: No**
- Overlap is allowed, correctly, but there is no overlap heatmap, optional spacing relaxation, or “tighten” operation. **Impact H · Effort M · Touch R/P · N: No**
- Dense crowd authoring is shape-and-spinner driven; painting, paths, polygons, and presets are missing. **Impact H · Effort L · Touch R/P · N: No**
- Existing groups transform well but lack a full scene-outliner experience: named crowd operations, lock/hide/solo, group clipboard, and mirror affordances. **Impact M · Effort M · Touch R/P · N: No**
- There is no session undo/redo transaction layer for Add/Delete/Place/Edit/formation operations. **Impact H · Effort L · Touch none · N: No**
- Ghost layouts are session state; there is no obvious Ghost Studio scene serialization contract in `ALGhostStudio`'s public API. **Impact H · Effort L · Touch none · N: No**
- Errors live across status text, disabled actions, and log output, so a failed click or stale preview may not explain the recovery action near the cursor. **Impact M · Effort S · Touch R · N: No**
- Region edges, teleports, unloaded terrain, and source outfit changes have robust pieces but no single user-facing lifecycle story. **Impact M · Effort M · Touch R/P · N: No**

## Design principles

- **Preview is truth:** commit the exact transform(s) shown; never recompute against a changed camera or surface on click.
- **One interaction language:** single clone, multi-select, groups, and formations share hover, snapping, rotation, scale, cancel, and warning semantics.
- **Freedom with legibility:** highlight questionable results; make “Place anyway” the default for every non-terrain concern.
- **Fast first success:** click a source, see a hologram, click a surface; advanced controls remain discoverable, not mandatory.
- **Deterministic crowds:** every scatter/variation has an explicit seed and can be reproduced after save/load.
- **Viewer budget, not simulator policy:** cost feedback is local rendering guidance, never a land permission check.
- **Progressive fidelity:** markers instantly, silhouettes next, full appearance only when ready; input must never wait on appearance cloning.

## 1. Unified placement tool UX

- Add `ALGhostPlacementSession`, shared by `ALToolGhostPlace`, `ALToolCrowdPlace`, and edit repositioning; it owns source snapshot, preview transforms, snap state, last valid hit, warnings, and commit token. **Impact H · Effort L · Touch R/P · N: No**
- Replace single-clone click-to-commit with a live translucent hologram following the cursor. For overlay clones, reuse the ghost draw path; for entity clones, use a cheap silhouette/proxy until appearance is ready. **Impact H · Effort M · Touch R/P · N: No**
- Keep the current resolver ladder but expose its result continuously: basis icon, support name, height, normal, and warning beside the cursor. Extend `ALGhostPlacementHit`, not a second picker. **Impact H · Effort S · Touch R/P · N: No**
- Add a downward support ray for terrain and prims at each preview foot point. Prefer the highest walkable hit below the candidate within a configurable range. **Impact H · Effort M · Touch P · N: No**
- Add support filters: Terrain + Prims, Terrain only, Selected set only, and Ignore transparent/phantom as an optional policy—not a hard restriction. **Impact M · Effort M · Touch P · N: No**
- Offer surface orientation modes: **Upright** (default), **Align to normal**, and **Blend** with a 0–100% slope amount. Feed the stored normal into the preview rotation. **Impact H · Effort S · Touch R/P · N: No**
- Wheel rotates yaw; Q/E rotates in fixed steps; Shift+wheel uses fine steps; a configurable modifier+wheel changes scale. Preserve the exact panel spinners. **Impact H · Effort S · Touch P · N: No**
- Click commits one; Shift-click commits and keeps the tool armed with the same source/options; Esc/right-click cancels the uncommitted preview. **Impact H · Effort S · Touch P · N: No**
- After each Shift-click, immediately instantiate the next lightweight preview so rapid stamping never waits on the previous appearance. **Impact H · Effort M · Touch R · N: No**
- Add snap toggles and temporary modifier inversion for grid, clone feet/centres, Director marks, Actor Mover waypoints/path, source actor, and camera heading. **Impact H · Effort M · Touch R/P · N: No**
- Visualize snap candidates with small glyphs and a line from raw hit to snapped transform; Tab cycles candidates when several share a pixel. **Impact M · Effort M · Touch R/P · N: No**
- Alignment helpers: face camera, face me, face selected subject, face picked point, face formation centre, copy actor facing, or preserve authored facing. **Impact H · Effort S · Touch R/P · N: No**
- Keep To actor / To me as immediate commands, but allow holding a modifier to preview the move before committing. **Impact M · Effort S · Touch R/P · N: No**
- Make “Lift to ground” the inline recovery when any preview foot is below loaded terrain; show the lift delta and re-enable commit after applying it. **Impact H · Effort S · Touch R/P · N: No**

## 2. Spawn feel and reveal pipeline

- Add a per-Look spawn transition: None, Fade, Ghost dissolve, Digital assemble, or custom effect identifier. Default should respect overlay/entity visual language. **Impact H · Effort M · Touch R · N: No**
- Treat reveal as a state machine: Allocate → Prepare drawable/LOD → Clone appearance → Prewarm pose → Reveal. Extend `Instance::mState` and `LLGhostAvatar` visibility rather than special-case UI sleeps. **Impact H · Effort L · Touch R · N: No**
- Keep entity clones invisible until body, attachments, first valid transform, and an acceptable first LOD are ready; time out to a readable degraded silhouette instead of stalling. **Impact H · Effort M · Touch R · N: No**
- Stream/prewarm appearance in a per-frame queue so a 100-person crowd does not clone every attachment in one frame. **Impact H · Effort L · Touch R · N: No**
- Add staggered crowd reveal with duration in milliseconds, ordering by slot, row, distance, seeded random, or centre-out wave. **Impact H · Effort M · Touch R · N: No**
- Spawn effect timing must be deterministic and saved with the crowd, while “reduced effects” accessibility/performance mode collapses it to Fade/None. **Impact M · Effort S · Touch R · N: No**
- Ease edit moves/turns/scales over a short configurable duration, with instant preview handles and exact final authored transforms. **Impact M · Effort M · Touch R · N: No**
- During an active drag, render the clone at the handle transform immediately; only ease after discrete commands or release, preventing input lag. **Impact H · Effort S · Touch R · N: No**

## 3. Spacing intelligence without rejection

- Add an optional personal-space radius per clone/crowd. It is metadata for warnings and tools, never placement legality. **Impact H · Effort S · Touch R/P · N: No**
- Show overlap as amber/red foot discs or capsule outlines in preview; show the count of intersecting pairs, not a modal error. **Impact H · Effort M · Touch R · N: No**
- Add **Auto-space**: deterministic 2D relaxation from current positions, minimizing movement while resolving the requested personal-space radius. **Impact H · Effort M · Touch R/P · N: No**
- Add **Tighten**: move members toward a centroid/path/row until the first personal-space contact, preserving order and facing. **Impact H · Effort M · Touch R/P · N: No**
- Before either operation, show a ghosted before/after and Apply/Cancel; “Apply even with overlaps” stays valid. **Impact H · Effort S · Touch R · N: No**
- Add Poisson-disk scatter brush with radius and density sliders; permit density beyond the radius recommendation by marking collisions rather than clamping it. **Impact H · Effort L · Touch R/P · N: No**
- Add seeded “blue-noise-ish” fast preview and a settled solve on mouse release, so brush feedback remains responsive. **Impact M · Effort M · Touch R/P · N: No**
- Optional wall/prim avoidance casts short probes from proposed centres and relaxes away from obstacles. Its result is an offered adjustment, never a block. **Impact M · Effort L · Touch P · N: No**
- Define hard validation in one function, e.g. `ALGhostPlacementPolicy::hardFailure`: non-finite data is an internal error; the only authored-world rejection is foot Z below loaded terrain Z. **Impact H · Effort M · Touch P · N: No**
- Reclassify outside-loaded-region, no support, inside-object, overlap, over-budget, and missing terrain data as warnings with explicit consequences. **Impact H · Effort S · Touch R/P · N: No**

## 4. Crowd creation tools

- Add brush modes: Scatter, Line, Arc, Ring, Grid, Path Follow, and Fill Area. All produce the same `FormationSpec`/resolved-slot transaction. **Impact H · Effort L · Touch R/P · N: No**
- Scatter paints members continuously; radius controls footprint, density controls target count, spacing radius controls advisory separation. **Impact H · Effort L · Touch R/P · N: No**
- Line/Arc/Path brushes click control points, show live resampling, and support equal-distance or fixed-count distribution. **Impact H · Effort M · Touch R/P · N: No**
- Path Follow reads an Actor Mover path snapshot and stores path identity plus a baked fallback, so the scene survives a missing/edited path. **Impact H · Effort L · Touch R/P · N: No**
- Fill Area accepts rectangle, circle, or clicked polygon; holes and walls are optional avoidance masks, not exclusions unless the user applies the solve. **Impact H · Effort L · Touch R/P · N: No**
- Ship crowd presets: Audience, Queue, March Column, Dance Floor, Party Clusters, Photo Rows, and Stage Chorus. Presets populate controls; they are not opaque generators. **Impact H · Effort M · Touch none · N: No**
- Add per-row/per-zone facing: stage point, formation centre, path tangent, across aisle, or authored yaw. Extend the existing facing enum/solver mapping. **Impact H · Effort M · Touch R · N: No**
- Variation panel: scale range, yaw jitter, position jitter, animation phase offset, animation-speed range, look mix, source/look variety, and spawn-delay jitter. **Impact H · Effort M · Touch R · N: No**
- Every variation channel derives from crowd seed + stable slot index + channel salt, matching the current deterministic seed approach. **Impact H · Effort S · Touch none · N: No**
- Add Reroll (increments seed), Previous seed, and Lock channel buttons so art direction is reproducible. **Impact M · Effort S · Touch none · N: No**
- Add select-by-brush with Add/Subtract/Toggle and optional filters by crowd, source, Look, visibility, or overlap state. **Impact H · Effort M · Touch R/P · N: No**

## 5. Selection and manipulation

- Replace point-only dense-crowd selection with body bounds/ID picking where available; keep projected probes as the overlay fallback. **Impact H · Effort M · Touch P · N: No**
- Add click cycling and an under-cursor mini-list when multiple clones overlap. **Impact H · Effort S · Touch P · N: No**
- Add marquee and lasso selection plus the brush selector. **Impact H · Effort M · Touch R/P · N: No**
- Give multi-select a centre-pivot gizmo for translate/rotate/uniform scale, with pivot choices: bounds centre, first selected, cursor, or picked mark. **Impact H · Effort M · Touch R/P · N: No**
- Add align left/right/front/back/feet/centres and distribute horizontally, vertically, radially, or along path. **Impact H · Effort M · Touch R · N: No**
- Promote the existing group model to a named Crowd unit in the list: transform, lock, hide, solo, duplicate, copy/paste, true mirror, and dissolve group. **Impact H · Effort M · Touch R/P · N: No**
- Keep “edit members” and pinned-member offsets; expose them in a compact outliner rather than relying on double-click discovery. **Impact M · Effort S · Touch none · N: No**
- Group mirror should mirror positions about a chosen local axis and optionally mirror member yaw/animation, while preserving source/Look identity. **Impact M · Effort M · Touch R · N: No**
- Never overload “lock” to mean placement validity: it means protect from accidental editing only. **Impact M · Effort S · Touch none · N: No**

## 6. Undo/redo and scene persistence

- Add `ALGhostHistory` with commands for Add, Duplicate, Delete, Transform, Style/Look, Group/Ungroup, Auto-space, Brush stroke, Formation commit, and Refresh replacement. **Impact H · Effort L · Touch none · N: No**
- Coalesce continuous drag/wheel/spinner edits into one transaction from mouse-down/focus-start to release/commit. **Impact H · Effort M · Touch none · N: No**
- Store enough clone snapshot data to restore deletes locally; entity runtime recreation can be deferred through the spawn queue. **Impact H · Effort L · Touch R · N: No**
- History is session-only by default, bounded by entry count/memory, and cleared or checkpointed on scene load. **Impact M · Effort S · Touch none · N: No**
- Add versioned Ghost Studio scene data: instance IDs, source references, backing kind, transforms, style/Look, pose/drive/AutoAnimate settings, facing, group hierarchy, seeds, layout generators, and visibility/lock state. **Impact H · Effort L · Touch none · N: No**
- Integrate the blob with Director scene save/load rather than inventing a parallel file workflow; permit Export/Import Layout as a portable subset. **Impact H · Effort L · Touch none · N: No**
- On missing source/animation/path, load placeholders with a repair action and retain authored transforms/data. **Impact H · Effort M · Touch R · N: No**
- Save baked transforms alongside procedural formation/path parameters; the baked result is the recovery truth when dependencies change. **Impact H · Effort M · Touch none · N: No**

## 7. Feedback and ease of use

- Add a cursor HUD: action, support basis, snap target, yaw, scale, count, warning, and the next click/Esc behavior. **Impact H · Effort S · Touch R · N: No**
- Add an optional keyboard cheat-sheet overlay that changes by Place, Brush, Edit, and Crowd Preview mode. **Impact M · Effort S · Touch R · N: No**
- Make the Director Ghosts page the complete “kit” UI and keep the standalone floater on the same shared panel contract. **Impact H · Effort S · Touch none · N: No**
- Reorganize controls into Source, Place, Selection, Crowd, Appearance/Performance, and Animation; keep frequent actions above the fold. **Impact M · Effort M · Touch none · N: No**
- Errors should say what happened and offer an action: “2 feet are below terrain — Lift all 0.38 m”; “surface unloaded — keep plane placement.” **Impact H · Effort S · Touch R · N: No**
- Add a persistent counter: total/enabled/visible/entity/overlay, spawning/ready/error, selected, and estimated draw cost. **Impact H · Effort M · Touch R · N: No**
- Add a local performance gauge based on visible rigged meshes, attachments, animation learners, pixel area, and shadow/physics cost—not only raw clone count. **Impact H · Effort L · Touch R · N: No**
- Warn at configurable soft budgets (for example green/amber/red); never disable Add/Confirm solely for cost. **Impact H · Effort S · Touch R · N: No**
- Provide one-click mitigations: disable physics, reduce effect FPS, lower pose rate, hide distant nameplates, stagger reveal, or switch distant clones to impostors. **Impact H · Effort M · Touch R · N: No**
- Add a minimap-like formation footprint and region-edge overlay only when the layout approaches an edge. **Impact M · Effort M · Touch R · N: No**

## 8. Performance and robustness

- Introduce `ALGhostSpawnQueue` with per-frame budgets for runtime allocation, appearance cloning, attachment cloning, first pose, and reveal. **Impact H · Effort L · Touch R · N: No**
- Keep `commitCrowdPlacement` transactional at the model level, but let runtime realization be asynchronous: all records commit together, visual readiness streams afterward. **Impact H · Effort L · Touch R · N: No**
- Use cheap preview proxies and cancel tokens so moving/cancelling a 256-member draft does not build discarded appearances. **Impact H · Effort M · Touch R · N: No**
- Frustum/distance cull before pose/appearance work where safe; retain authored state even while no runtime is resident. **Impact H · Effort L · Touch R · N: No**
- Add distant impostor/LOD policy specific to clones, with hero-clone opt-out and a visible diagnostic state. **Impact H · Effort L · Touch R · N: No**
- Keep the solver's 256-member technical bound initially, but present it as a per-operation batch limit with “continue painting,” not as a creative density rule. **Impact M · Effort S · Touch none · N: No**
- If a safety cap is ultimately necessary, warn before it, allow override where memory-safe, and explain the limiting resource; do not conflate it with placement validity. **Impact H · Effort M · Touch R · N: No**
- On region crossing, keep global authored transforms; suspend runtimes whose destination region is unavailable and show placeholders/warnings rather than deleting layout state. **Impact H · Effort M · Touch R/P · N: No**
- On teleport, retain the scene layout by default, offer Hide remote / Follow me / Clear, and never silently drag world-authored crowds to the new region. **Impact M · Effort M · Touch R · N: No**
- On source outfit/shape change, mark dependent clones stale, queue a transactional background rebuild, and reveal-swap only when the replacement is ready. **Impact H · Effort M · Touch R · N: No**
- Preserve frozen poses and authored transforms through a rebuild; report attachments that could not be reconstructed. **Impact H · Effort M · Touch R · N: No**
- Instrument queue time, first-preview latency, first-reveal latency, frame budget consumed, and rollback count under the existing Ghost Studio diagnostics. **Impact M · Effort S · Touch none · N: No**

## Proposed code ownership

- `ALGhostStudio`: authoritative instances, crowd/group operations, commit transactions, hard-rule validation, history hooks, and persistence DTOs.
- `ALGhostPlacementResolver` / adapter: surface + terrain support hits, normals, below-terrain test, region-edge warnings, and prim-aware downward probes.
- `ALToolGhostPlace`: unified hover/commit controls for a single clone and repeat stamping.
- `ALToolCrowdPlace`: reuse the same session controls for formations and brushes; retain pin/confirm semantics.
- `ALToolGhostEdit` / `ALGhostManipProxy`: richer hit testing, multi-select gizmo, snapping, eased finalization, and drag transaction boundaries.
- `ALFormationSolver`: deterministic layouts, relaxation, tighten, Poisson scatter, polygon fill, path sampling, and seeded channel salts.
- `LLGhostAvatar`: hidden preparation, readiness gates, reveal effects, clone LOD/impostor policy, and smooth presentation transforms.
- `ALPanelGhostStudio`: tool modes, presets, status/recovery actions, outliner operations, history buttons, and budget display.
- `panel_ghost_studio.xml`: shared kit layout; `panel_dpage_cast_ghosts.xml` and `floater_ghost_studio.xml` continue embedding it.
- Director scene model: versioned Ghost Studio section and dependency repair; no server persistence or network messages.
- `alchatcommand.cpp`: preserve all `/ghost*`; optionally add `/ghostundo`, `/ghostredo`, `/ghostplace`, and `/ghostbudget` as thin calls into the same APIs.

## Phased roadmap

### Phase 1 — biggest feel wins for least effort

- Single-clone live hologram using the existing resolver and overlay rendering primitives.
- Continuous support/basis/warning HUD; Upright vs Align to normal.
- Wheel/Q/E yaw, modifier scale, Shift-click repeat, Esc cancel.
- Prim-aware Drop to surface and per-slot support option for formations.
- Grid/clone/mark snap plus face camera/subject/centre helpers.
- Overlap highlight only; no relaxation yet.
- Fade reveal, per-frame spawn queue shell, and hidden-until-ready entity clones.
- Cursor-readable below-terrain error with one-click Lift to ground.
- Initial transform/Add/Delete/formation undo transactions.
- Counter and soft performance warning.

### Phase 2 — crowd direction and manipulation

- Multi-select body picking, click cycling, marquee/lasso, centre gizmo.
- Align/distribute and named Crowd outliner actions.
- Auto-space/Tighten deterministic relaxation.
- Scatter/Line/Arc/Path brushes and core presets.
- Seeded variation channels and staggered crowd reveal.
- Smooth post-edit motion and configurable spawn effects.
- Versioned Director scene save/load with baked fallback transforms.
- Region crossing, teleport, and stale-source UX.

### Phase 3 — scale and advanced authoring

- Polygon fill, Poisson brush, optional wall avoidance, and path dependency repair.
- Full asynchronous appearance pipeline with cancellation and priority.
- Clone-specific LOD/impostors and richer cost model.
- Group mirror with animation semantics, solo workflows, and portable layout import/export.
- Preset authoring/sharing and per-Look spawn-effect customization.
- Profiling-led increases to practical crowd size while retaining advisory budgets.

## First slice acceptance criteria

- Selecting either an overlay ghost or entity clone and pressing Place shows a correctly scaled, translucent preview before any transform is committed.
- Preview follows visible prims and terrain under the cursor at interactive frame rate and reports the support basis.
- Upright is default; Align to normal visibly follows a ramp and can be toggled without losing the hit.
- Wheel and Q/E rotate; the chosen modifier scales within `0.05–150`; panel values stay synchronized.
- Click commits the exact previewed foot/rotation/scale; Shift-click commits and immediately previews another; Esc changes nothing.
- Grid and other-clone snapping are visible and temporarily invertible with a modifier.
- A foot below loaded terrain cannot commit; the message gives the depth and Lift to ground makes it committable in one action.
- Overlap, extreme density, no prim support, inside-object risk, over-budget state, and broad spread only warn; Confirm remains available.
- Formation preview can show body-sized silhouettes or capsules and flag overlaps while retaining current marker/guide readability.
- Formation support conform can place members on a stage, stairs, and terrain; missing hits retain authored Z with a warning.
- Spawning ten entity clones does not expose unprepared bodies/attachments and does not create a single-frame allocation spike above the agreed budget.
- Undo restores a single Place, repeat-stamp batch, Delete, and formation commit with source/look/transform/group state intact.
- All existing Add/Duplicate/Delete/Refresh, To actor/To me, Drop, Align feet, Upright, copy/paste, Line/Ring/array, chaos, nameplates, true mirror, AutoAnimate, look-at/turn, and `/ghost*` behavior remains available.
- No placement or persistence action sends simulator/network messages.

## Open questions for the user

- Should “below region terrain” be tested at the foot pivot only, or across an approximate footprint/capsule for large scales?
- If terrain data is not loaded, should commit remain allowed with a warning, or wait until the terrain test is knowable?
- Should outside-loaded-region placement remain as off-screen authored data, clamp as an offered fix, or be limited to a preview placeholder until that region loads?
- For prim support, should phantom and transparent prims count by default?
- When several surfaces stack vertically, should the default be nearest visible hit, highest surface below cursor depth, or cycleable candidates?
- Is Upright the universal default, or should each Look remember Upright/Align to normal?
- Should sitting/elevated placement be foot-on-surface only in the first version, or include sit-target/pelvis semantics?
- Which control should own scale during placement: Ctrl+wheel, Alt+wheel, or a dedicated key to avoid camera conflicts?
- Should Shift-click create one history entry per clone or one batch entry for the entire stamping session?
- Which three crowd presets matter most for the first production use: audience, queue, march, dance floor, party clusters, or photo rows?
- Should path-follow remain linked live to Actor Mover paths, or update only when the user presses Rebuild?
- Should variation choose among multiple source actors, multiple Looks from one source, or both in Phase 2?
- What is the expected comfortable target: 25, 100, 256, or more visible entity clones on the reference machine?
- Should hero clones be automatically exempt from impostors/pose throttling, or explicitly pinned as Hero?
- Should spawn effects be saved per clone, per Look, per crowd, or as a scene-wide default with overrides?
- How should remote layouts behave after teleport: retain silently, hide with a banner, or prompt once?
- Should Director scene load replace the current Ghost Studio session, merge it, or preview a diff first?
- Are group names and clone names production identifiers that external scene tooling must keep stable?
- Is a compact game-editor toolbar preferred, or should advanced placement remain inside the current tabbed Ghost Studio panel?

## Recommendation

- Start with the first slice: unified hologram placement, surface-aware support, direct rotation/scale controls, soft overlap feedback, reveal staging, and basic undo.
- It transforms the moment-to-moment feel without discarding the reliable formation solver, transactional crowd commit, shared panel, group model, or viewer-only clone architecture already in place.
- Build every later brush, preset, and persistence feature on the same preview-is-truth placement session so single clones and 256-member crowds never diverge again.
