# Clone Placement — Phase 1 Spec (v3, 10/16)

**Status: PLAN GREEN (10/16): Opus (round 10), Codex (xhigh + rechecks), Gemini Flash (low weight; Argon out of quota). Build slice by slice.**

- v2 round 3: Codex xhigh PLAN GREEN; Opus and Argon NOT GREEN.
- The **v3 changes** section below overrides any earlier text it contradicts.
- **UI reference = mockup artboard "Cast › Clones"** in the Director Redesign Mockup (`project/ClonesPage.dc.html`): collapsible group boxes, member rows, OVL/ENT badges, Place card and selection card.

## v3 changes (Opus and Argon round 3, user 10/16)
- **V3.1 Shared selection set.** Replace the single `ALGhostStudio::mSelected` with a model-owned selection set:
  - an ordered set of unit ids, plus an anchor;
  - one revision counter;
  - `setSelection`, `addToSelection`, `toggleSelection` and `clearSelection`.

  The Director Clones list, the floater list and the world all read and write the set:
  - The panels stop `deselectAllItems` + `selectByID` on revision (`alpanelghoststudio.cpp:1085-1116`) and instead mirror the whole set.
  - `onListSelect` pushes all selected rows (`:2047`), not just the first.
  - `selectedUnitIds()` reads the model.
  - Single-id consumers use the anchor.
  - The removal consumer that clears `mSelected` (`alghoststudio.cpp:~313`) removes that id from the set; it also moves the anchor if the removed id was the anchor.
- **V3.2 Extend `ALToolGhostEdit`; no new tool.**
  - Its pick handler (`handleStockPick`, `altoolghostedit.cpp:118-159`) honours Shift (add) and Ctrl (toggle).
  - **Click on empty ground deselects** (user decision); it no longer re-places the selection or exits edit mode.
  - Drag starting on empty ground runs the **clone box-select**, not `LLToolSelectRect`.
  - Moving a selection is done with Place or the gizmo.
  - **Gizmo:**
    - `ALGhostManipProxy` follows the selection set;
    - with N units, the proxy sits at the selection centre and applies its delta to every unit (locked groups as one unit);
    - it is bound to the anchor for single-unit numeric fields.

  The proxy already touches `LLSelectMgr`; it stays limited to the proxy object, so prim selection outside the Clones tool is unchanged.
  - **Handoff while something is selected.** With a selection, the active tool is `LLToolCompTranslate`/`Rotate`/`Scale`, not `ALToolGhostEdit`. When `handleStockPick` returns true, `pickCallback` returns early (`lltoolcomp.cpp:308-312`), so a drag never reaches the edit tool. The comp tools therefore route the clone hook's result explicitly:
    - **Empty-ground mouse-down** hands mouse capture to `ALToolGhostEdit`, so a click deselects and a drag box-selects.
    - **Clone hit** selects as above; the gizmo stays.
  - **Deselect keeps edit mode.** `ALGhostManipProxy::tick()` currently calls `stopEditMode(true)` when the selection goes empty (`alghostmanipproxy.cpp:696-716`). **Both** empty-selection exits (`:696` and `:714`) change to `enterPickerState()` (`:284`). Edit mode ends only through Esc, the tool button or a page change.
- **V3.3 Group box UI (from the mockup).**
  - **Group header row:** a collapse chevron (`aria-expanded`), a lock icon, name, count and kind mix. Clicking the header selects the whole group.
  - **Expanded:** members are listed indented under the header.
  - **Selecting a member edits that member individually while it stays in the group.** Its transform shows and edits as an **offset from its group slot**, with amber reset dots when changed. Member actions: **Back to group spot**, **Take out of group**.
  - **Group actions:** Edit members (expands), Lock/Unlock, Ungroup.
  - **Multi-selection:** "N selected · changes apply to all"; differing values show *mixed*.
  - Rows carry **OVL/ENT badges** with the overlay/entity explanation in the tooltip.

  Model support uses the existing offset API, and **`mAuthoredLocal` is never written by member edits**:
  - `mAuthoredLocal` is the slot itself.
  - A member's offset is `mLocal` plus `mPinned`.
  - Member edits go through `setMemberWorld(..., pin)` (`alghostgroupmodel.cpp:431-459`).
  - "Back to group spot" and the amber reset dots use `resetMemberOffset` (`:554-582`).
  - The dots show when `mLocal != mAuthoredLocal`.
  - `applyGroupTransforms` already keeps the edit, because `resolveMember` reads `mLocal`.

  **"Take out of group"** needs a new `ALGhostGroupModel::removeMember(instance_id)`. It:
  - drops the member and its `mMemberToGroup` entry;
  - keeps the member's current world transform;
  - dissolves the group when fewer than 2 members remain (same path as `ungroup`, `alghoststudio.cpp:2712-2745`);
  - is one undo step.

  **Selection never changes the model.** `mEditMembers` also drives the list's chevron, header double-click and row mapping (`alpanelghoststudio.cpp:1081`, `:1416-1433`, `:2099`), so it is not used for selection:
  - **Expand/collapse is a UI-only state:** a panel-local set of expanded group ids, toggled only by the chevron or **Edit members**. It never bumps the group revision. The list draws member rows from it instead of from `mEditMembers`.
  - **`mEditMembers` is retired as a behaviour switch.** The selection itself says what is being edited:
    - Each selection-set entry is typed: a **group entry** (the header row, a world click on a locked member, or a box-select) or a **member entry** (a member row in the list).
    - One predicate, `isMemberScope(id)`, is true only for member entries. It replaces every `mEditMembers` read:
      - `setSelected`'s fold-to-representative (`alghoststudio.cpp:2638-2644`);
      - `getUnitTransform` (`:2960-2966`), so the readout shows the member's own offset;
      - `transformUnit` (`:2996`, `:3051`);
      - `setInstanceEnabled` (`:1225`);
      - the gizmo's `whole_group` checks (`alghostmanipproxy.cpp:348/400/531/569/610`);
      - the gates in `setMemberWorld` and `resetMemberOffset` (`alghostgroupmodel.cpp:436`, `:557`).
    - The predicate also replaces these readers:
      - `setMemberPinned` (`alghostgroupmodel.cpp:589`);
      - `member_edit` (`alpanelghoststudio.cpp:1467`);
      - the member-action gate (`:4245`);
      - the in-world group labels (`alghoststudio.cpp:1930`);
      - the three Delete gates (`altoolghostedit.cpp:180`, `llselectmgr.cpp:4390`, `:4570`).
    - The **Edit members** checkbox (`:1622`) becomes the expand toggle, labelled "Edit members", with the same mockup button.
    - **Delete on a member entry** deletes only that member. It goes through the `removeMember` path, so the group dissolves below 2 members. It is one undo step, and undo restores the member, its slot offset and its group membership; that includes re-forming the group under its original UUID via `insertGroupWithId` if it dissolved. Delete on a group entry still acts on the whole unit (B7).
      - **How the undo works:** before the delete, snapshot the whole group. Undo checks the group's **authored revision** for conflicts. That is a new counter, `Group::mAuthoredRevision`, bumped only by user commands, never by driven per-frame updates such as Track subject or Keep facing (`alghoststudio.cpp:2067` → `alghostgroupmodel.cpp:545`). After the check it runs `eraseGroup` + `insertGroupWithId` on the snapshot. This covers both the group-survived and the dissolved case; `restoreGroupSnapshot` needs the same member count, so it can't be used.
      - After a dissolve, the conflict check also refuses if the leftover member has since moved or joined another group.
    - The legacy floater's header double-click (`alpanelghoststudio.cpp:2099`) toggles the panel-local expand set instead.
    - **Saved files:** `mEditMembers` is still read for compatibility, then ignored, and written as false. Implementation greps for every remaining reader; any left over fails review.
  - B7's "never treat a grouped member as a single instance" still holds. A member is edited only through these member commands, never as a free instance.
- **V3.4 Delete-undo restores only Clones-page state.**
  - `restoreInstanceWithId` **clears `mWasDirectorSubjectA-D` and `mWasCinematicFollow`** (as duplicate does, `alghoststudio.cpp:1018-1022`), so Director subjects and camera follow are NOT restored.
  - **Cast membership does come back** (the runtime consumer calls `cast.add` for every new runtime, `:~275`); the spec states this explicitly.
  - Entity runtimes are newly spawned through `spawnEntityForInstance` and are never the dead `saved.mEntityId`.
  - The original `mId` is installed before runtime callbacks bind.
- **V3.5 Locked-group delete-undo** snapshots and restores the `ALGhostGroupModel::Group` record under its **original group id**, keeping:
  - name;
  - world frame (rotation and scale);
  - `mAuthoredLocal`, `mPinned`, `mEditMembers`.

  It does NOT call `lockGroup`, which would mint a new id and reset the frame.

  `restoreGroupSnapshot` cannot be used, because it is rollback-only and refuses a group that isn't live (`:652-655`). Add **`ALGhostGroupModel::insertGroupWithId(const Group&)`**. It:
  - refuses an id that is in use;
  - validates through `normalizeGroupCandidate`;
  - rebuilds `mMemberToGroup`;
  - keeps the original UUID; there is no id counter, so nothing is bumped.

  It runs after all member instances are restored.
- **V3.6 All-or-nothing replay.**
  - `ALKitHistory` replay first runs a **`canReplay` check for every custom entry** in the step (each clone command supplies one), plus the existing conflict checks. Only if all pass does it apply.
  - A refused step is dropped with its notice **from undo and from redo** (`alkithistory.cpp:589-590`, `:608-609`).
  - This is safe for existing custom users: Gaze Cues and Light Cues only refuse when their target is gone permanently, and Light Cues already no-ops on conflict.
  - Accepted limit: a step refused because the source avatar is temporarily away is dropped. The notice says "source not nearby".
  - **Failures while applying** (for example entity spawn, appearance or attachment cloning failing, `alghoststudio.cpp:377`) are not refusals:
    - Each clone command applies in stages and **rolls back what it already did in that step**: it despawns restored instances, erases an inserted group and restores the selection.
    - On the **first** failure the step **stays on its stack** (not dropped), with the notice "Couldn't restore — try again".
    - On the **second consecutive** failure of the same step it is **dropped**, with the notice "Couldn't restore — skipped". The step keeps a per-step failure count, which resets on success. The count is stored in the `Step` record, so it survives moves between the undo and redo stacks. The rollback runs before the count is incremented. Older steps stay reachable, so the stack never wedges.
    - Only `canReplay` refusals drop a step.
- **V3.7 Preview geometry = the clone's own displayed draw, moved.**
  - The preview **calls the existing clone/overlay draw path** with an extra world-space post-transform, `M = draftDisplayed × inverse(currentDisplayed)`, applied **after** everything that path already composes:
    - the source→clone frame (`llactormover.cpp:14905`);
    - pose selection: live, Frozen or stepped-cadence palettes, and captured attachment matrices (`:15253`);
    - entity scale (`lldrawpool.cpp:1533`).
  - So the preview shows exactly the pose and scale on screen, moved to the draft. Harvested data is never transformed raw.
  - `currentDisplayed` and `draftDisplayed` are the unit's root world transform **as drawn this frame**, including motion (see V3.8).
  - System/BoM bodies cannot be replayed without an Actor FX style (`canRenderSystemActorGhost`, `:15388`), so they use the **capsule** (sized from the avatar's height × draft scale).
  - Draw point: the depth-bound hook **before the screen-target flush** (`pipeline.cpp:~27599`), next to the actor path guides (`renderHeadingPreview(true)`, `:23231-23262`). It does not go in `render_ui`.
- **V3.8 Motion, Chaos and facing modes.**
  - `applyGroupTransforms` refuses group moves while any member has Motion or Chaos (`alghoststudio.cpp:2890`). Place on such a group is **refused with "Turn off Motion/Chaos to move this group"**; Phase 1 does not rebase them.
  - For single clones, committing updates the motion bases too: `mMotionBaseFoot`, `mMotionBaseRotation`, **`mMotionCentre`** (Orbit/Breathe, `:2327-2355`) and **`mCrowdBaseYaw`** (Wave, `:2080-2092`). The mode then continues from the new base instead of snapping back.
    - Commit goes through **`setTransform` / `setScale`** (which already rebase the foot, rotation and scale and clear the chaos base, `alghoststudio.h:477-501`), never raw `mRotation` writes. Only `mMotionCentre` and `mCrowdBaseYaw` are shifted on top.
    - **`mMotionCentre` is shifted by the same delta** (`centre += newFoot − oldBaseFoot`), never set to the new foot; otherwise the Orbit/Breathe radius `mMotionBaseFoot − mMotionCentre` collapses to zero.
    - **Commit preserves what the preview showed.** Spin, Wave and the facing modes add a phase on top of the base (`alghoststudio.cpp:2342`), and `setTransform` installs the draft as the base (`alghoststudio.h:484`). So commit **solves the base**, `base = draftDisplayed ∘ inverse(currentPhaseOffset)`, for rotation and likewise for the position offsets. On the next frame the clone appears exactly where the preview was; for example, at Spin phase 90° a 180° preview stays 180°.
    - **Target-driven facing (Track subject, Keep facing, Turn) is not a phase.** Those modes overwrite rotation from `target − foot` (`alghoststudio.cpp:2061`, `:1543`). While one is active:
      - the draft is **position-only**: the wheel yaw, Upright/Align and gizmo rotate are disabled, and the readout shows "Facing: Track subject";
      - the preview **evaluates the mode's facing at each draft member's foot**, through the same function the mode uses per frame, so the preview shows the rotation the clone will have after commit;
      - commit writes position only, and the mode keeps owning rotation.
      - **Exception: Turn on a group.** Turn rotates the whole group frame around its turning member (`alghoststudio.cpp:2198` → `transformUnit` `:1708` → `transformFromMember` `:3102` → `alghostgroupmodel.cpp:386-408`), which moves the other members' feet. Place on a Turn-active group is **refused with "Turn off Turn to move this group"**, the same as Motion/Chaos. Turn on a single clone stays position-only as above.
    - **Path motion:** its points are world-space, so Place on a Path-moving clone is refused with "Turn off Path motion to move this clone", the same as Motion/Chaos on groups.
  - Correct B4's wording: Track subject, Keep facing and Turn own **rotation/facing** (`:2047`, `:2119`, `:2195`), not position.
- **V3.9 Place availability and tool lifecycle.**
  - The Place button is enabled for overlay clones, entity clones, locked groups and multi-selections; today it's `overlay && !crowd_active`, `alpanelghoststudio.cpp:1577`. It stays disabled while a crowd draft is active.
  - The promoted base tool uses the **save/restore pattern of `ALToolCrowdPlace`** (`mSavedBaseTool`, `stopPlacement`, `altoolcrowdplace.cpp:150-173`). `cancelForOwner`, `handleDeselect` and `exitPlaceMode` restore the saved base tool, not `clearTransientTool`.
- **V3.10 Citation fix.** `getWantedSources` is at `alghoststudio.cpp:~5101`, not 712-748.

- v1 round-2 reviews: Opus, Codex xhigh and Argon low all NOT GREEN, with converging corrections. v2 applies all of them.
- Round 1 reviewed doc/GHOST_SPAWN_SYSTEM_BRAINSTORM.md.

## User decisions (10/16)
- Ctrl+wheel scales; wheel and Q/E rotate.
- One undo entry per stamped clone.
- The below-terrain test uses the foot pivot.
- Controls live in a Place card on the Clones page and in Clone Studio.
- **Shift-click stamps a copy.** The selected clone stays put; Esc keeps the stamped copies.
- **Unknown terrain blocks ground placement until it loads.**
- **Out-of-region: block entity clones only.** Overlay clones are allowed there with a warning.
- **NEW: drag-select and multi-select clones in the world, like prims.**

Phase 1 = **four slices**, built and reviewed one at a time. Out of scope until there is Tracy data: spawn queue, fade/reveal, hidden-until-ready, snapping, facing helpers, cost gauge.

## Foundation
- **Extend what exists:** ALGhostStudio, ALGhostPlacementResolver/Adapter, `ALToolGhostPlace`, `ALToolCrowdPlace`, `ALGhostInteractionState`. No new session class.
- The 7/28 `ALGhostSpawnEngine` front end is left aside, not deleted. Keep `ALGhostMaterialResolver` and the source guard (clone-of-clone rejection).
- UI changes go in both the generated Director kit page (mockup data) and `panel_ghost_studio.xml` (floater).
- **Viewer-local:** no simulator mutation and no new placement traffic. Ordinary asset loading (textures, animations, AutoAnimate) is allowed. Preserve `mIsLocalOnly` and the server-update suppression.

## Shared building blocks (used by all slices)
- **B1 Filtered surface probe.** `lineSegmentIntersectInWorld` returns the single nearest hit with no filter. It picks avatars (`pipeline.cpp:12479`) and nametags (`:12529`), and doesn't skip phantoms. So add a **filtered world probe**: either iterate the ray past rejected hits, or a filtered octree query.
  - Skip avatars, clones (including the clone being placed), nametags, HUDs, phantom prims, and faces with alpha < ~0.1 (the rain-shelter invisible-prim class), respecting the transparency exceptions at `llvovolume.cpp:5036`.
  - Terrain comes from received patches only.
  - Nearest accepted hit along the ray wins.
  - Water keeps the resolver's current water-surface behaviour.
  - The probe is bounded (max distance and max iterations) and does no per-call heap allocation.
- **B2 Draft transform with tilt.** The draft stores foot, **full rotation** (yaw composed around the surface normal when Align-to-normal is on, otherwise around world Z), scale, and mode.
  - Extend `ALGhostInteractionState::State` to carry a normal or tilt quaternion. Commit writes `Instance::mRotation` directly; it doesn't go through `setYaw`, which would strip the tilt.
  - The Yaw spinner edits the yaw component only and keeps the tilt.
  - Terrain fallback normals use the real slope, not world-up (`resolver:371`).
- **B3 Input.** Promote single-clone Place to the base-tool architecture of `ALToolCrowdPlace` (`gBasicToolset` base tool, so Alt-camera works).
  - Remove the hover-only early-out in `ALGhostInteractionState::reduce` for `EVENT_WHEEL` (`alghostinteractionstate.cpp:263`). Wheel, Q/E and Ctrl+wheel (add `MASK_CONTROL` scale to `EventContext`) apply whenever a Place session is active.
- **B4 Transform ownership.** Before writing an instance's transform, the Place session asks who owns it:
  - True Mirror owns facing (`alghoststudio.cpp:2135`);
  - tracking, wave and motion own position and/or rotation (`:2047`, `:2080`, `:2318`).
  - Rule: **Place sets the authored base transform.** Modes that animate a transform on top keep running from that new base. A mode that fully *owns* a channel (True Mirror facing, tracking position) shows that channel as "owned by <mode>" in the readout and doesn't apply it.
  - The readout says so, so nothing silently diverges.
- **B5 Below-terrain rule** (hard block; all enforcement goes through one function, `checkFootLegality`).
  - The foot pivot Z, after support, orientation and scale, must be ≥ terrain height − 0.02 m.
  - **Unknown terrain** (patch not received): block **only when the support basis is terrain or none**. With prim or water support, commit is allowed and the terrain check is skipped, so skybox and indoor floors work.
  - **Out-of-region:** entity clones are blocked (no region means no runtime). Overlay clones are allowed, with a warning.
  - **Hover and commit use the same `checkFootLegality`**, so hover never shows green and then refuses on commit.
  - Enforced on Place commit, Shift-click stamp and crowd Confirm. Spinners, gizmo, paste, To actor, chat and paths warn only.
  - Remove the silent clamp (`adapter:123`). The message shows the depth, and **Lift to ground** fixes it in one click.
- **B6 Hologram (preview).**
  - **Data.** The draft registers its own preview source with the harvest:
    - overlay clones: the source avatar;
    - entity clones: the entity runtime (`mEntityId`).

    The harvest currently wants overlay sources only, and only while Show clones is on (`getWantedSources`, `alghoststudio.cpp:712-748`), and entity runtimes only under an Actor FX style (`llactormover.cpp:14548`). Add a preview-only want, independent of Show clones.
  - **System and BoM bodies** are not in `mGhostBatches` (`llactormover.cpp:15103`). Draw them through `renderSystemActorGhost` (`:15529`), or fall back to the capsule for those parts.
  - **Draw.** Use a **separate preview draw call** with its own GL state. Don't extend `renderStudioGhosts`, which returns early with no overlay items and uses `LLGLSUIDefault`.
    - It must run **while scene depth is still bound**, i.e. before `renderFinalize()` (`llviewerdisplay.cpp:1818`) or against the scene depth target. That way the depth test occludes it behind walls.
    - Apply the draft scale exactly once. The entity outer transform is not in the harvested data (`lldrawpool.cpp:1521`).
  - **Fallback:** a scaled capsule whenever the harvest for the preview source is empty or partial.
  - **Never** an `Instance`, never an actor, and no frame-lifetime pointers kept past the frame.
  - Reproduces the clone's current displayed pose (Frozen, Directed, stepped, AutoAnimate). Check under Actor FX Cover.
- **B7 Locked groups.** A selection that resolves to a locked group is a **group unit**:
  - Place, stamp and delete act on the whole unit (`transformGroup`, group-aware removal);
  - the preview draws every member;
  - undo records the whole unit as one entry.

  No slice may treat a grouped member as a single instance.

## Slice 1: single-unit Place
1. **Place.** Arm the Place tool for the selected unit (one clone, or one locked group).
   - Draft per B2, input per B3, preview per B6, probe per B1.
   - Click commits **exactly** what was previewed; Esc cancels and changes nothing.
   - The draft dies on Esc, on page/tool change, or on loss of the selected unit or its runtime (checked every frame, with a notice).
2. **Readout by the cursor:** support basis (prim / terrain / water / none), height above support, scale, yaw and ownership (B4). Advisory warnings: no support, extreme scale.
3. **Legality per B5.** Lift to ground.
4. **Drop to surface (prim-aware).** The existing Drop button uses the B1 probe straight down from the clone. This replaces `resolveLandHeightGlobal`, which has no received-patch guard and snaps to Z=0 on unloaded patches (`alpanelghoststudio.cpp:~2911`).

## Slice 2: select clones in the world (user request 10/16)
1. **Click pick.** With the Clones tool active, clicking a clone's **body** selects it. Hit test against each instance's screen-projected bounds, using the foot plus a scaled capsule or the harvested bounds, which works for overlay and entity alike. The nearest clone to the camera wins. Clicking empty space clears the selection.
2. **Modifiers:** Shift-click adds, Ctrl-click toggles. Clicking a locked-group member selects the whole group (B7).
3. **Drag-box.** Dragging from empty space draws a rectangle; on release, every clone whose screen-projected bounds intersect it is selected. Shift and Ctrl add or toggle, as with prims. No GL picking is needed (pure screen projection), so it works for overlay clones, which are not objects.
4. **Sync.** The world selection and the Clones page list (and the floater list) are the **same selection** (`selectedUnitIds`). Changing either updates the other.
5. **Move together.** With several units selected, Place moves them as one: the draft is the selection's centre, and relative offsets, rotation and scale are kept about that centre. B5 applies to every member foot.
6. **Isolation.** All of this runs only while the Clones tool is armed. Normal prim selection, Build and Edit are untouched.
7. **Highlight.** Selected clones get a subtle outline or foot ring, using the same depth rules as B6.

## Slice 3: Shift-click stamping and Clones-page undo
1. **Shift-click stamps a copy** of the selected unit at the draft transform. The original doesn't move, the tool keeps previewing for the next stamp, and Esc keeps the completed stamps.
   - Overlay stamps copy the instance.
   - **Entity stamps require the source avatar to be present.** `duplicateInstanceSnapshotInPlace` → `spawnEntityClone(proto.mSource)` → `createEntityRuntime` resolves the live source (`alghoststudio.cpp:771/990/371`), and no appearance snapshot exists. If the source has left, the stamp is refused with "Source has left; entity copies need the source nearby."
   - Each stamp is synchronous (one hitch each, accepted and measured).
2. **Undo** (Director rules):
   - **The Clones page is the only owner,** through `ALKitHistory::recordCustom` (`alkithistory.cpp:~201`), using typed commands: Place, Stamp, Add, Delete, Transform, Select is NOT recorded.
   - **One entry per stamp.**
   - **Conflict refusal lives in each command's closure.** It compares the clone's current state with the state the command recorded; the kit conflict rule covers settings only (`:455-480`).
   - **A refused replay must not wedge the stack.** Today `undo()` returns without dropping the step when a custom replay returns false (`:589-590`). Change it so a refused step shows its notice and is **dropped** (from both stacks, as with skipped setting conflicts). That applies to all custom history, which fixes the latent wedge generally.
   - The floater, in-world tools and `/ghost` chat record **no** Director history.
   - History resets on scene load (`Step::scene_generation`).
   - **Delete-undo.** Add `restoreInstanceWithId(const Instance& saved, const LLUUID& id)`, which re-inserts the saved instance (transform, look, group, name, settings) under its **original stable id**. Today `addInstance` and `spawnEntityClone` always `mId.generate()` (`:749`, `:779`).
     - For entity clones, the runtime is rebuilt from the live source. If the source has left, the undo is refused with a reason.
     - Locked groups are restored as a whole unit (B7).
     - State owned by other pages is **not** restored: Actor Mover paths, cast membership, camera follow. A notice says so.

## Slice 4: formations
1. **Formation surface support.** Per-slot downward B1 probes run **on pin/confirm, cached**, never every hover. A slot with no hit keeps its authored Z and gets a warning.
2. **Overlap feedback.** Foot discs or capsules are flagged when they overlap; advisory only. No full-body silhouettes.
   - The solver gets an explicit **allow-overlap** option (`alformationsolver.h:119`, `alghoststudio.cpp:4303`). The default stays as today.
3. Crowd Confirm enforces B5 per member.

## Cost and budgets (measure with Tracy)
- Preview harvest + draw ≤ 0.5 ms at 60 fps.
- Probe ≤ 0.1 ms per hover, including filtered iterations.
- Box-select ≤ 0.2 ms for 256 clones (screen projection only).
- Formation per-slot rays only on pin.
- No per-frame heap allocation in the preview, probe or select paths.

## Tests and gates
- **Unit tests (pure parts):**
  - draft reducer: wheel, Q/E, Ctrl+wheel, click, Esc, no hover-gating;
  - rotation composition with tilt, and the Yaw spinner keeping the tilt;
  - `checkFootLegality`: tolerance, unknown terrain by support basis, entity vs overlay out-of-region, hover equal to commit;
  - probe filter policy;
  - box-select projection and modifiers;
  - undo closures: replay, refusal and drop;
  - restore-with-id;
  - one entry per stamp;
  - group units.
- Self-test rows for the Place card and selection on the kit page.
- Gates: label fit, wiring audit, xui check, zs_check.
- Reviews: Opus + Codex xhigh + Argon low.
- One build per slice, then an in-world check as 5kvideoeye.

## Out of Phase 1
Spawn queue, hidden-until-ready, fade/reveal, snapping (grid/clone/mark), face-camera/subject helpers, cost gauge, crowd brushes and presets, variation, appearance snapshots (so entity copies could work without the source), scene persistence changes, region-crossing UX.
