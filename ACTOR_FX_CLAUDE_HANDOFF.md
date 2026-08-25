# Actor FX — Claude Test/Build Handoff

Date: 2026-08-24  
Repository: `I:\alchemy-machinima`  
Branch observed during implementation: `feature/vram-management`

## Important workspace warning

This repository already has a large dirty worktree containing unrelated user work. Do **not** reset, discard, restore, or broadly reformat existing changes. Limit any fixes to the Actor FX files and preserve overlapping edits.

## What was added

Director now has a final, scrollable **Actor FX** tab that applies Ghost Studio-style rendering to a live actor at their current position. It does not create or manage a separate clone.

The tab can target:

- You
- Any Director cast member
- Cast members currently away from the region; their settings remain stored for when they return

Available controls:

- Per-actor enable switch
- Render mode: `Layer` or `Opaque cover (preview)`
- All 28 existing Ghost Studio looks
- Actor color or custom hue
- Alpha
- Pixelation
- Shimmer speed and amount
- Glitch
- Distortion mode and amount
- Brightness
- Effect FPS
- Reset to defaults

Actor FX is appended after the pre-existing Director tabs so older numeric `DirectorLastTab` values continue to open the same tabs.

## Rendering semantics

`Layer` draws the selected effect over the normally rendered actor.

`Opaque cover (preview)` forces the styled layer to full opacity, but it does **not** suppress the actor's normal render underneath. The UI deliberately calls this a preview rather than claiming true replacement.

The feature reuses Actor Mover/Ghost Studio's harvested worn geometry and `actorghostF.glsl` style pipeline. Rendering occurs in the always-visible scene-dressing portion of `render_ui()`, before normal UI chrome. It remains present when the viewer UI debug feature is hidden and is included in both deferred and non-deferred snapshot paths.

## Data and persistence

`LLDirectorCast::ActorStyle` stores the per-actor configuration. The new data is additive and backward-compatible:

- Each cast scene entry can contain an `actor_style` map.
- The local user is stored separately as `self_actor_style`.
- Old scenes without these keys load with styling disabled.
- Partial or malformed numeric values are defaulted, clamped, and checked for non-finite values.
- Null UUID continues to mean You.
- A non-null `gAgentID` also routes to the authoritative self-style record, even while the avatar object is temporarily unavailable during a rebuild.

## Main source locations

- Model, defaults, and API: `indra/newview/lldirectorcast.h`
- Sanitization and scene serialization: `indra/newview/lldirectorcast.cpp`
- Geometry harvesting and actor-style renderer: `indra/newview/llactormover.h`, `indra/newview/llactormover.cpp`
- Frame integration: `indra/newview/llviewerdisplay.cpp`
- Director control logic: `indra/newview/llfloaterdirector.h`, `indra/newview/llfloaterdirector.cpp`
- Actor FX tab layout: `indra/newview/skins/default/xui/en/floater_director.xml`

Useful symbols/search terms:

- `LLDirectorCast::ActorStyle`
- `LLDirectorCast::getActorStyle`
- `LLDirectorCast::setActorStyle`
- `actor_style_wants_overlay`
- `LLActorMover::renderStyledActors`
- `LLFloaterDirector::refreshActorStyleTab`
- `actor_style_tab`

## Adversarial-review fixes already applied

Three independent review passes were performed. The following issues were found and fixed:

1. The original `Replace` wording falsely promised suppression of the normal avatar. It is now honestly labeled `Opaque cover (preview)` in the combo, tooltip, status, and renderer comments.
2. An enabled Layer with alpha zero still ran the ghost renderer's depth-prime pass, creating an invisible silhouette that could occlude later Actor FX or Ghost Studio overlays. `actor_style_wants_overlay()` now excludes Layer alpha values at or below `0.001` from detection, harvesting, and rendering.
3. Inserting Actor FX near Ghosts shifted every later saved `DirectorLastTab` index. Actor FX is now the final XML tab, preserving all previous indices.
4. Self targeting previously depended on `isAgentAvatarValid()`, which could split state during avatar reconstruction. Model routing now uses stable `gAgentID`, and the UI uses stable `gAgent.getID()` when suppressing a duplicate self cast row.
5. The snapshot call chain was explicitly audited. Deferred snapshots still run `render_ui()` because `display()` clears its local `for_snapshot` gate while retaining `gSnapshot`; non-deferred snapshots explicitly call `render_ui()` from `LLViewerWindow::rawSnapshot()`. Actor FX should be included exactly once in both paths.

The final post-fix review reported no release-blocking model, UI, rendering, persistence, or integration issue.

## Post-runtime PBR/alpha fix — 2026-08-24

The first in-world test on a complex PBR outfit exposed unstable alpha coverage/flicker and a larger-than-expected Layer FPS cost. Two concrete renderer defects were found and fixed:

1. **Cross-wearer spatial-group leakage.** `walkGhostSourceGeometry()` gathers the spatial groups containing the target's attachments, but a spatial group is a shared world bucket and can contain rigged draw batches from nearby avatars. The old callback accepted every rigged draw in those groups. Actor FX could therefore harvest and redraw unrelated nearby wearers, with the set changing as spatial grouping changed. This produced unstable PBR coverage and potentially enormous overdraw in crowded regions. Rigged batches are now accepted only when their drawing avatar is the requested wearer, or when the drawing avatar is an attached `LLControlAvatar` whose wearer is the requested avatar.
2. **Incorrect PBR alpha semantics.** The actor-ghost shader previously multiplied every PBR base-colour texture's alpha into output coverage, even for GLTF `OPAQUE` materials. Opaque materials are required to ignore completed base-colour alpha; that channel may contain arbitrary or packed data and its mip transitions were punching/flickering holes in Actor FX. The draw now uploads explicit authored-alpha semantics: Opaque uses neither texture nor factor alpha, while Mask/Blend use texture alpha, appropriate vertex alpha, and the PBR base-colour factor alpha. The shader centralizes this as `ghostAlpha`/`authoredAlpha` before cutoff and look evaluation.

The same fix also corrects the Clone/Actor FX alpha slider: clone-color draws now retain the requested instance alpha instead of replacing it with the material factor alpha.

Verification after this patch:

- Integrated RelWithDebInfo compile, link, dependency deployment, and manifest copy: passed with exit code 0.
- Release/x64 `ClCompile` for `llactormover.cpp`: passed with 0 warnings and 0 errors.
- `git diff --check` for the renderer and shader: passed.
- A complete runtime asset tree was staged beside the RelWithDebInfo executable, then verified to contain the updated `actorghostF.glsl` (`ghostAlpha`/`authoredAlpha`) and current Actor FX XUI. This matters because the normal development manifest's `copy` action does not copy the full shader tree; shaders are normally included only by its packaging action.

The new build still needs an in-world comparison on the reported outfit. Use the same camera, look, and graphics settings and toggle only Layer/Opaque cover when measuring FPS.

Ready-to-run post-fix test build:

```text
I:\alchemy-machinima\build-Windows-vs2026-os\newview\RelWithDebInfo\AlchemyTest.exe
```

Do not test the currently running Release executable for this fix unless it has been fully relinked and its staged shader tree refreshed; an older Release asset tree was observed to contain a stale `actorghostF.glsl`.

## Build verification already completed

The integrated RelWithDebInfo build, link, dependency deployment, and viewer manifest copy completed successfully with exit code 0:

```powershell
cmake --build build-Windows-vs2026-os --config RelWithDebInfo --target alchemy-bin -- /m
```

Built executable:

```text
I:\alchemy-machinima\build-Windows-vs2026-os\newview\RelWithDebInfo\AlchemyTest.exe
```

Additional checks already passed:

- `floater_director.xml` parses as XML.
- All 19 Actor FX named controls are unique and match their C++ control types.
- `git diff --check` reports no Actor FX whitespace errors.
- Selected-file compile checks passed for the model, renderer, and Director UI.

The feature has not yet received a full interactive in-world visual test; that is the purpose of this handoff.

## Recommended runtime test matrix

### 1. Basic You styling

1. Launch the RelWithDebInfo executable and log in.
2. Open Director and select the final **Actor FX** tab.
3. Select **You** and enable actor styling.
4. Cycle several distinctive looks: Ghost, Clone, Wireframe, X-ray, Toon/Ink, Dissolve, Gold Statue, and Holo Echo.
5. Verify sliders update the live actor without creating a separate cast/clone entry.
6. Disable styling and confirm the effect disappears cleanly.

### 2. Layer and alpha behavior

1. Use `Layer` mode.
2. Test alpha at `1.0`, approximately `0.5`, and `0.0`.
3. At alpha zero, confirm the style vanishes and does not hide another Actor FX actor or a Ghost Studio overlay positioned behind it.
4. Re-enable a visible alpha and verify rendering resumes.
5. On PBR `OPAQUE` surfaces, confirm texture alpha no longer creates holes, crawling transparency, or mip-dependent flicker.
6. On PBR `MASK` surfaces such as cutout feathers/hair, confirm authored holes and cutoff edges remain intact.
7. On true PBR `BLEND` surfaces, confirm transparency follows the material without turning opaque surfaces translucent.

### 3. Opaque cover preview

1. Select `Opaque cover (preview)`.
2. Confirm the effect is forced opaque regardless of the stored alpha slider.
3. Confirm the normal actor may still be visible underneath for open effects such as Wireframe, X-ray, and Dissolve. This is expected for the preview implementation.

### 4. Multiple cast actors

1. Add at least two avatars/animesh actors to Director Cast.
2. Give each actor a different look and hue.
3. Move the camera so their silhouettes overlap.
4. Confirm effects follow the correct live actors and are not duplicated.
5. Confirm selecting or editing one actor does not alter the other actor's settings.

### 5. Away/return lifecycle

1. Configure a cast member's effect.
2. Have the actor leave the region or otherwise become unresolved.
3. Confirm the selector retains an `(away)` entry and its controls remain editable.
4. When the actor returns and resolves again, confirm the stored style resumes on that actor.

### 6. Self rebuild stability

Trigger an outfit/avatar rebuild if practical. Confirm the You entry does not duplicate and its style settings do not reset or jump to a cast-member record.

### 7. Scene persistence

1. Configure different effects for You and multiple cast members.
2. Save a Director scene.
3. Change or reset the effects.
4. Reload the scene.
5. Confirm all per-actor fields restore correctly.
6. Load an older scene without Actor FX data and confirm styling defaults to off.

### 8. UI-hidden filming

Enable an Actor FX look, hide viewer UI/debug UI using the normal filming shortcut, and verify the styled actor remains visible as scene dressing.

### 9. Snapshot coverage

Capture:

- A normal deferred snapshot
- A high-resolution deferred snapshot
- A non-deferred snapshot if that path is available in the current configuration

Confirm the Actor FX result appears once in each image and is not missing or double-rendered.

### 10. Ghost Studio coexistence

Enable both Actor FX and one or more existing Ghost Studio overlays. Confirm both render and that a zero-alpha Actor FX Layer cannot invisibly occlude a Studio ghost.

### 11. Crowded-scene ownership and performance regression

1. Stand in a region with several nearby avatars, preferably including PBR outfits.
2. Apply Actor FX only to You.
3. Confirm no geometry from another wearer appears, flashes, or inherits the effect.
4. Record baseline FPS with Actor FX disabled.
5. Use one fixed look and camera; record Layer FPS at alpha `1.0`, then Opaque-cover FPS.
6. Repeat at Layer alpha `0.5`.
7. Move nearby avatars across spatial-group boundaries and confirm the styled geometry and frame time remain stable.
8. Report the exact look, alpha, resolution, nearby-avatar count, and before/after FPS if a substantial Layer-only gap remains.

## Known limitations — do not report these as new regressions

1. **No true replacement yet.** Opaque cover leaves the normal actor rendering underneath.
2. **Overlay-style depth behavior.** Actor FX uses the same post-finalize overlay approach as overlay clones, so ordinary looks may show through props or walls. World-depth occlusion is not implemented for this pass.
3. **Legacy/system body coverage.** The shared geometry harvester focuses on worn mesh and attachments and does not reproduce the complete legacy system-avatar body path. A pure legacy avatar can be partially styled or show no useful effect.
4. **Alpha-blended materials.** Some advanced looks shade solid/masked sweeps but do not fully restyle true alpha-blended hair, clothing, or attachments. Clone and Wireframe have broader coverage. This is separate from the fixed bug where Opaque PBR surfaces incorrectly consumed their texture alpha.
5. **Shader fallback.** If an advanced actor-ghost shader fails to compile, the shared renderer can fall back to the basic Ghost look while the combo still displays the requested look.

## What should be treated as a bug

- Crash, assertion, or persistent GL-state corruption when enabling/disabling Actor FX
- A style targeting the wrong cast member
- You settings disappearing across an avatar rebuild
- Saved scene fields failing to restore
- Actor FX missing from ordinary or high-resolution snapshots
- Actor FX disappearing merely because viewer UI is hidden
- Alpha-zero Layer hiding another Actor FX or Ghost Studio overlay
- Duplicate rendering of the same wearer because they are reachable as both You and a cast member
- Significant cost while no Actor FX style, path ghost, or Ghost Studio instance is active

## If a fix is needed

Keep the patch tightly scoped. Re-run at minimum:

```powershell
[xml](Get-Content -Raw indra/newview/skins/default/xui/en/floater_director.xml) | Out-Null
git diff --check
cmake --build build-Windows-vs2026-os --config RelWithDebInfo --target alchemy-bin -- /m
```

Do not solve the known true-replacement or world-depth limitations by globally suppressing avatar render passes. A correct replacement implementation must account for system body geometry, rigged and non-rigged attachments, alpha/material passes, impostors, and shadow casting without punching holes in normal rendering.
