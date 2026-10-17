# Light Rig targets: design review and implementation plan

Status: proposed design, 2026-10-17. This is a specification only. No code was changed, built, committed, or pushed for this review. Source locations below refer to the current working tree, including its uncommitted edits; they will move during implementation.

## Recommendation

Adopt named rigs and an explicit target question. Keep the existing five rig instances, but treat their slots as storage positions rather than people. A new rig targets a deliberately selected Cast member, or nobody. Selecting another rig changes what is being edited; it never changes whom either rig lights.

Use direct Cast actor identities for new person/group targets. Do not assign or borrow Director Subjects behind the scenes. Preserve old slot-based bindings through a compatibility path until the user explicitly replaces their targets. Keep shared group-centre lighting; omit **Each person** from the first version and use separate rigs for individual lighting.

This addresses the reported case without inventing a general group manager, expanding the renderer, or redesigning cameras and gaze.

## What is confusing today

There are three different concepts behind controls that appear to name a person:

| Concept | Current implementation | Consequence |
|---|---|---|
| Rig being edited | `ALCineLightRigManager::selectedSlot()` | “You” and “Subject A” identify independent stored lighting setups. |
| Default person for that rig | `resolveSlotAvatar()` | A–D resolve Director Subjects; SELF normally resolves the operator. |
| Effective target | Object, otherwise enabled group, otherwise default person | A rig named “Subject A” can legitimately illuminate You while Subject A is unassigned. |

Evidence: `indra/newview/alcinelightrigmanager.cpp:712`, `indra/newview/alcinelightrig.cpp:1062`, `indra/newview/alcinelightrig.cpp:2499`, `indra/newview/alcinelightrig.cpp:2555`.

The supplied screenshots demonstrate this vocabulary failure. `editing_rig_menu.png` offers “Subject A – not set,” while another rig can already be on. `whos_lit_card.webp` shows only You in Cast but a permanent You/A/B/C/D target checklist. Its header switch means “override this rig's default target with a group,” although “Who's lit” reads like a report of scene coverage. The screenshot itself is editing You; the reported Subject A/group-You state comes from the user's live observation, not from that screenshot alone.

The label fixes currently in the tree make the override more visible. They do not remove the two meanings of “You” or make unassigned Cast actors targetable.

## Critique of Claude's mockup

Reviewed both markup and `renderVals()` in `.tmp/teleport_log/LightRigWho_mockup.dc.html`.

Keep these parts:

- A strip of rigs showing **rig name → target**, with separate On/Off state. This makes a rig a thing the operator can select and reuse.
- One explicit question, **Who does this rig light?**, followed by a person list, a group checklist, or an object target.
- A reverse view showing the rigs assigned to each Cast actor. Clicking a rig name there should select it for editing.
- Retaining an unavailable target and showing that the rig is waiting. A missing actor must not cause a different actor to inherit their lighting.

Correct these parts before implementation:

| Issue | Minimal correction |
|---|---|
| `addRig` always uses `solo: 'you'` (line 294), despite its tooltip mentioning Cast selection (line 27). | Use a valid, single, explicit Cast selection. With no selection or multiple selections, create an unassigned rig. Never interpret an empty selection as You. |
| `people()` always includes You and cannot model zero actors or “Roddra without me” (line 163). One-person help assumes the sole person is You (line 219). | Use actual Cast membership; offer the existing Add You action when needed. Exercise zero actors and one non-self actor in the mockup. |
| Changing example Cast to one person rewrites every group rig to solo, using its old solo target (line 288). | Cast changes must not change target mode or membership. Preserve the group and its missing members. |
| The Several people segment is disabled below two Cast members (line 209). | Gate entry into a new group this way, with explanatory text, but keep an existing group inspectable/editable when Cast shrinks. Zero/one selected members are valid intermediate editing states. |
| “Their centre / Each person” looks like a cheap aiming option (line 246). | Only centre lighting exists. Remove this choice initially; say “One shared rig follows their centre.” Separate rigs give individual control. |
| `targets()` equates Cast membership with world availability (line 176). A partly missing group silently loses names; `rigWarn` only catches a completely empty result (line 196). | Keep configured names in chips and show missing counts. Distinguish removed from Cast, unavailable in world, and off camera. |
| Object summary says “Lighting” even when the rig is off (line 254); a nonempty object string implies availability. | Apply the same Off/Waiting/Ready logic to objects. Store an object identity, not its display string. |
| “Not lit” and “N of N lit” imply measured illumination. | Prefer **No rig assigned**, **Rig off**, **Waiting**, and **Targeted by Rig 1**. Add one short explanation: other lights may also affect this actor. |
| Two rigs on one actor is presented as an amber warning (line 267). | Show a neutral **2 rigs assigned** count. Multiple rigs are normal for machinima: accents, opposing keys, practicals, and separate shots. Warn about a real capacity/readiness problem. |
| New rig and Light actions have no limit (lines 274, 294); Remove has no handler despite promising Undo (line 50). | Expose the five-rig limit; implement removal and history before making that promise. Rename “Light” to **Add rig** so its effect is explicit. |

The two-column composition is useful at a generous width. At the viewer's minimum width, stack the reverse view below the target editor or collapse it. Keep the current light map, fixtures, presets, and Easy/Advanced editing; this mockup is the targeting section, not a replacement lighting console. A simple editable rig name is enough; no naming dialog or reusable group library is needed.

## Proposed interaction contract

### Rig selection and creation

- Show existing rigs, including Off and Waiting rigs, with names such as **Rig 1**, **Rig 2**, or an operator-entered name such as **Door key**. Names are independent of actor names and preset names.
- A chip shows configured targets, not just currently resolved targets: `Rig 2 → Roddra, Juno · 1 unavailable`. Use text/tooltip as well as any warning icon. Preserve full names in the editor; identical names must remain distinguishable.
- **+ New rig** allocates one free internal position, initializes a clean Classic 3-Point setup, and selects the new rig. It does not copy the previous rig's group, object, Actor only flags, cues, or transport state.
- With exactly one explicitly selected Cast member, the new rig targets that actor and turns on. Otherwise it is unassigned and off, with **Choose a target**. No fallback to the first Cast row or You. Choosing or changing a target leaves On/Off unchanged; the unassigned creation path therefore requires an explicit Turn on after choosing its target.
- Cast selection after creation does not retarget a rig or automatically switch the edited rig. Remove **Follow cast selection** from this page's rig selector. The current adapter at `alpanelcinelightrig.cpp:1001` only maps actors already assigned to Subjects and is not a solution for arbitrary Cast targets.
- At five allocated rigs, disable both creation entry points with **5 of 5 rigs used. Remove a rig to add another.** Turning a rig off does not free its position. Remove does, and is undoable under the history contract below.

### Choosing targets

| Mode | UI and behavior |
|---|---|
| One person | Radio list of enrolled Cast actors, including ghosts/animesh that resolve as actors. You appears when explicitly enrolled. Unassigned is a real state, distinct from Self. |
| Several people | Checklist of Cast actors, at most five members per shared rig initially. Membership belongs to this rig; it does not edit Cast production tags. Start from the current solo person when entering this mode; do not tick everyone or add You automatically. |
| An object | Named root/linkset target plus **Use selected object** and Clear. Reuse current selection validation initially. A true click-in-world picker can follow separately; do not label a selection-reading button “Pick in world.” |

Switching modes is one explicit, undoable target change, with one active target variant. Entering One person from a multi-person group requires a person choice; do not silently choose its first member. Entering Object with no previous object leaves an empty object target that emits nothing. Canceling/rejecting an object replacement retains the existing object. Clearing an object leaves an unassigned object rig; it does not resurrect a hidden group or self target. Undo restores the prior complete target.

The ordinary list contains actual Cast members, not unused Subject roles. Selected missing actors remain visible as a small **Saved targets** section, with their last known names and Remove/Replace actions. This is an exception for retaining authored intent, not a list of every empty role.

**Not being in the scene is supported.** On a new scene, no light targets the operator merely because the viewer has a self avatar. Use `getCast()`/`getIds()` for these lists (`lldirectorcast.h:304`, `:320`); do not use `getActorChoices()` unchanged, because `aldirectoractorchoice.cpp:32` unconditionally inserts You and subsequently merges assigned Subjects. The Cast rail itself uses actual IDs (`llfloaterdirector.cpp:5239`), and Add You already explicitly enrolls the agent (`llfloaterdirector.cpp:5479`). Do not change the existing empty-Cast-means-self behavior of transport, movement, or other features as a side effect.

### Availability and coverage

- Enrolled but temporarily unavailable: retain identity/name, emit nothing for that actor, resume for the same identity when available. Being outside the camera frame does not make an actor unavailable.
- Removed from Cast: retain the saved target but stop following it for new explicit bindings, even if its avatar is still nearby. Re-enrolling the same identity resumes it; selecting another actor explicitly replaces it. No name-based substitution.
- A partially available shared group follows the available members' centre, preserving all requested members. Show **1 of 2 available**. With none available, destroy/suspend emitters and clear actor linking. Keep group mode and the On intent, so return is predictable.
- Summary examples: **On · targeting Roddra Venus**, **Off · targets Roddra Venus**, **Waiting · Roddra is unavailable**, **Waiting · target removed from Cast**, **Choose an object**. Reserve “lighting” as a friendly description, not proof of pixels receiving light.
- The reverse **Who's lit** panel reports assignments and readiness, including Off rigs in a subdued state. If an actor already has an Off rig, offer that rig/Turn on before creating a duplicate. **Add rig** remains available as an explicit additional-lighting action when capacity allows.

## Mapping onto the existing implementation

### Five named rigs, not five named people

`ALCineLightRigSlot` is SELF=0, A=1, B=2, C=3, D=4 (`alcinelightrig.h:100`). The manager constructs exactly five instances (`alcinelightrigmanager.cpp:376`) and owns fixed instance/blob arrays (`alcinelightrigmanager.h:1380`). Keep this limit initially. Two rigs may target the same actor; any storage position may target any Cast actor or an object. Off rigs retain their parameters and identity.

Add manager-owned metadata per position: allocated/present, stable rig ID, display name, and explicit target. A rig's ID survives rename, Off/On, save/load, and undo. Reusing a removed position creates a new ID; do not derive IDs/names from the current list length as the mockup does. No user-facing slot letters in the rig strip, target editor, coverage panel, or history labels.

The existing settings are a single selected-rig editing buffer, not five independent setting namespaces. `setSelectedSlot()` captures the old buffer and writes the new one (`alcinelightrigmanager.cpp:707–723`); selected/nonselected ticks take different paths (`:1138–1175`). Retain that routing. In particular, `captureSelected()` must not overwrite new metadata/targets with defaults when constructing a ParamBlob from settings. Keep manager-owned metadata separate from that parameter capture, and explicitly serialize/apply it with the slot record.

Allocation and enablement must be distinct. On v1 import, keep every enabled rig, selected rig, rig with target/group/cue intent, assigned subject, or non-default authored parameters. Only demonstrably unused, factory-equivalent disabled positions can become free; retain ambiguous Off rigs. Compare authored fields, not transient FX time, emitter IDs, or the currently selected buffer. Preserve imported parameters. A conservative import can fill all five positions; never overwrite a saved Off rig to make New work. Fresh v2 scenes can have zero allocated rigs, and no-rig UI/settings paths must not implicitly create SELF.

### Target representation and resolution

| Approach | Assessment |
|---|---|
| Auto-assign a free Director Subject on target selection | Reject. “Free” means unassigned now, not owned by lighting. Assigning A can immediately change camera behavior. Removing/reusing roles can invalidate gaze/camera intent; four letters also cannot address an arbitrary Cast roster. |
| Set `mAnchor` for every slot | Insufficient. Only SELF consumes `mAnchor`; A–D ignore it in `resolveSlotAvatar()` (`alcinelightrig.cpp:1062`). Group resolution and Actor Mask still read subject letters. |
| Give new rigs explicit actor references | Recommended. Resolve any eligible Cast identity without writing Subjects; share resolved results across lighting, coverage, and Actor Mask. |

Use a small tagged target, conceptually:

- Person: optional `ActorRef`.
- People: ordered unique `ActorRef` list, initially bounded by `GROUP_MAX_MEMBERS == 5` (`alcinelightrigmodel.h:58`).
- Object: optional root/linkset UUID and cached name.
- Legacy binding: existing slot/group/object fields, only for imported state; not a fourth user-selectable segment.

An `ActorRef` distinguishes explicit Self from a specific canonical actor ID. Empty/unassigned must never be represented by a bare null UUID passed to `cast.resolve()`: null resolves the operator (`lldirectorcast.cpp:1802`), and `isSelf(null)` is true (`:1066`). Canonicalize/deduplicate self aliases, animesh handles, and stable Ghost Studio instance IDs using the existing actor identity APIs (`:1093`, `:1808–1830`). Store cached names for missing-target explanations, never as identity. Do not replace a missing ghost with its appearance source or a recreated object that happens to share its name.

Resolve targets in the model/update path into requested identities, live eligible avatars, status/reason, and effective centre/scale. Consumers must share this result. Existing resolution is duplicated across `gatherGroupMembers()` (`alcinelightrig.cpp:164`), `tickShared()` (`:2552`), `isSlotLit()` (`alcinelightrigmanager.cpp:789`), and Actor Mask (`:1094`). Updating only the UI or solo resolver would leave targeting and light linking disagreeing.

Use separate configuration and availability revisions. UI changes compare revisions and use cached names; no world resolution, UUID string formatting, list rebuilding, or target mutation from `draw()`. Keep numeric geometry updates independent from roster-text updates. Follow `CLAUDE.md`'s refresh/visibility/ownership rules.

### Centre, tracking, and individual lighting

Current groups use a bounding-box centre of valid member tracking points and a scale that includes their spread (`alcinelightrig.cpp:2586–2642`). They create one shared light set. The first valid member supplies facing (`:2646`) and the sole catchlight avatar (`:3049`). A group membership check therefore does not promise equal exposure or an eye light on everyone, especially with widely separated actors or projector reach limits.

`CineLightRigTrackMode` is independent: Skeleton follows the chest; Body-stable uses body position plus a height offset (`alcinelightrig.cpp:216`, `:2588`, `:2835`; `panel_cine_light_rig.xml:227`). Neither mode means “each person.” Preserve these existing controls and geometry. Preserve a stable member order so sorting the Cast list or resolving a temporary absence does not arbitrarily change the chosen lead.

**Each person** would require independent emitter sets/frames per member, lifecycle handling, catchlights, shadow/probe accounting, and per-member linking. It cannot be implemented by renaming a track mode or aiming one four-light set at multiple centres. Initial alternative: create/select separate named rigs, within the shared five-rig limit. No mass-creation button is necessary yet.

### Actor only and Actor Mask

The target question chooses what the rig follows. It does not, by itself, exclude other actors or the set from ordinary light spill. Actor only remains a separate receiver restriction. Preserve per-light flags, the rig-wide override, catchlight behavior, and **Bounce lights the set**; choosing a group must not turn those flags on.

For new targets, supply the exact same resolved actor identities to the Actor Mask registry that were used for aiming. The current registry reconstructs groups from `lastResolvedGroupSlots()` and Subjects (`alcinelightrigmanager.cpp:1094–1106`); this must change along with group targeting. Membership loss must remove stale receiver bits, including attachments and supported animesh/ghost owner mappings.

Capacity matters: the registry supports **six distinct recipient identities across participating rigs**, with bit 6 reserved for orphans; further identities are skipped (`alcinelightrigmanager.h:683`, `:755–806`). Arbitrary UUID groups can exceed this even with five rigs and five members each. Keep the five-member shared-group limit and six-person Actor-only union initially. Validate authored target unions when enabling/changing Actor only or enabling another participating rig, reserving absent configured identities so arrivals do not reshuffle the budget. Reject an over-capacity edit with a clear explanation and retain the previous state. Imported/legacy overflow still needs a visible status and fail-closed behavior; never silently broaden it to world lighting or claim every requested actor is covered.

Object targeting currently bypasses Actor only; controls are disabled but saved flags survive (`alpanelcinelightrig.cpp:2703–2744`; `alcinelightrigmanager.cpp:1054–1063`). Preserve this behavior, with **Object targets use normal lights; Actor only is unavailable** beside the controls. Do not advertise object-only light linking.

### Other consumers and the “no letters” requirement

Director Subjects remain intact. Subject A anchors cinematic cameras and B participates in two-shots/OTS (`lldirectorcast.h:365`; `llcinematiccamera.cpp:681`, `:720`, `:740`); gaze has its own slot mapping (`alpanellensgaze.cpp:769`, `:798`). Lighting actions must not call `setSubjectA/B/C/D()`, including on remove/reset/undo.

Live Probe target values really select rig positions (`alcinelightrigmanager.cpp:447`, `:629`). Populate those choosers with named allocated rigs while retaining internal position values. Remove/reuse must clear or explicitly rebind a probe target; a new rig in the same position must not inherit the old rig's probe association silently.

Night Mask is different: its target is independently resolved from Self/Subjects, not from the selected rig (`pipeline.cpp:14409–14422`, `alpanelcinelightrig.cpp:2982`). Do not merely relabel those values as Rig 1–5. To remove letters from this page, show current actor names and a retained “Saved target unassigned” item for its existing role binding; keep its existing semantics. Extending Night Mask to arbitrary direct Cast targets is separate work, not an excuse to auto-assign a Subject here. State that limit in its help.

Likewise audit rig-related history labels and probe/diagnostic UI rather than changing internal enum values or Subject terminology throughout Director. Camera-focus lighting priority currently consults `resolveSlotAvatar()` (`alcinelightrigmanager.cpp:914`); it must use the effective target centre for new group/object/person bindings. Reuse existing smoothed geometry where appropriate, without introducing per-frame UI work.

## Root cause investigation: Subject A with group ON + You

### What the code proves

1. **The symptom follows the stored state exactly.** Object targets win. Otherwise, with `mGroupEnabled=true` and `mGroupSlots & GROUP_SLOT_SELF`, `gatherGroupMembers()` resolves You, regardless of the rig's A slot or whether Subject A exists (`alcinelightrig.cpp:164–179`, `:2555–2569`). This explains “A controls me.” Independently saved parameters plus selecting another rig can explain the reciprocal impression; the screenshots do not establish an actual slot-routing swap.
2. **The Who's lit card is a writable override, not coverage.** `panel_dpage_light_cine_light_rig.xml:137` gives it `enable_widget="cine_group_enable"`; `alkitcard.cpp:286–296` creates that switch. The panel wires it to `onGroupEnabledCommit()` (`alpanelcinelightrig.cpp:798`), which writes the selected rig's group flag (`:1776`). The You row writes the group mask through `onGroupSlotsCommit()` (`:1783`). Enabling the header preserves the existing mask; it does not seed You. Enabling an empty group alone lights nobody.
3. **Factory defaults do not enable this state.** Runtime fields start false/zero (`alcinelightrig.h:429–430`); ParamBlob also defaults false/zero (`alcinelightrigmanager.h:185`). Non-self startup blobs explicitly clear group and object intent (`alcinelightrigmanager.cpp:382–389`). There is no default “A group includes You.”
4. **Lighting setup presets do not create the group.** `loadSetup()` loads fixture/setup globals and volumetrics (`alcinelightrig.cpp:3689–3735`); it does not call the group setters. It also does not clear an existing group. Loading Classic 3-Point can therefore retain group-You targeting. The page's `preset_mode="widgets"` means hosting the existing setup widgets (`alkitpage.cpp:384`), not a generic snapshot/replay of every targeting switch.
5. **Scenes/session restore can recreate it.** ParamBlob captures and reapplies the exact flag/mask (`alcinelightrigmanager.cpp:321–354`), serializes `group`/`group_slots` (`alcinelightrigmanager.h:473`), and restores them (`:640–644`). Startup restores `CineLightRigInstances` (`alcinelightrigmanager.cpp:396`); scenes carry all five blobs (`:1639–1666`, `:1714`). The legacy single-rig scene also persists `anchor_group` and `anchor_group_slots` (`alcinelightrig.cpp:4233`, `:4336`). This explains persistence, not the first creation of the flags.

### The proposed refresh-commit explanation is not supported

The relevant current lines are `alpanelcinelightrig.cpp:1820` and `:1832` (shifted from the cited 1780–1820 region by the uncommitted helper). `setValueIfChanged(widget, value, true)` takes **force**, not “commit.” It compares then calls the LLSD `setValue()` overload (`aluirefresh.h:291–302`). `LLCheckBoxCtrl::setValue()` only sets its button (`indra/llui/llcheckboxctrl.cpp:238`); `ALKitSwitch::setValue()` adds a flip notification (`alkitswitch.cpp:113`). The row listener only refreshes its displayed value (`alkitrow.cpp:421`, `:733`). None invokes the group commit handlers.

Actual commits use `onCommit()` (`LLCheckBoxCtrl::onCommit`, `llcheckboxctrl.cpp:159`). Widget bindings explicitly pair `setValue()` with `onCommit()` for user/programmatic edits and history replay (`alkitbinding.cpp:137`, `:152`). A displayed value changing during synchronization is not evidence of such a commit. Removing `true` would introduce focus-dependent stale UI and would not fix the model.

### What remains unproven, and the fix plan

The supplied live state establishes group ON + You, but not the event that first authored it. The copied logs in `.tmp/teleport_log/Alchemy_live_lightrig.log` and `Alchemy_20261016.log` contain no matching `#CineRig selected_slot`/`group_mask=` transition evidence. Static inspection cannot distinguish a prior header/You click, scene/session restoration, or a binding/history replay. Do not label one of those as the confirmed historical cause.

The confirmed product cause is a hidden target override presented as a coverage card, combined with person-named storage slots and persistence of that override across setup changes. Fix that model/UI contract. Do not mass-clear saved group-You rigs: it is also a legitimate intentional configuration.

Before implementation claims the origin is solved, make one discriminating reproduction: record complete target state and commit counts before/after opening/revealing the page, switching rigs, loading Classic 3-Point, toggling the group header, toggling You, and undo/redo. Add change-only diagnostic events at actual mutation boundaries, with source (`user`, `scene`, `session`, `undo`, `reset`), rig ID/internal position, and before/after target fields. Preserve the existing selected-target logger (`alcinelightrigmanager.cpp:726`); it reports state, not mutation origin. Never log every refresh/frame.

Expected verdicts: refresh-only operations leave model target state and commit counts unchanged; a header click changes only the selected rig's group flag; You changes the selected rig's membership; setup load leaves both fields unchanged; restore reproduces precisely the saved state. Unexpected mutation is FAIL with the originating path. Missing instrument/source coverage is INCONCLUSIVE. A scoped UI-sync callback guard can enforce future passive refreshes, but it is defensive hardening, not a proven repair for this incident.

## Saved scenes, presets, resets, and history

### Compatibility policy

Read existing v1 session stores, five-instance scenes, and legacy single-rig scenes. Introduce an explicitly versioned new rig/target format for new saves, rather than changing the meaning of old `anchor`, `group_slots`, or null IDs. Update both manager and selected-rig scene parsing: the current manager stamps the instance-store version while `ALCineLightRig::applySceneData()` has a separate v1 gate (`alcinelightrigmanager.cpp:1670–1721`; `alcinelightrig.cpp:4280`). Unknown/malformed new targets must produce an unassigned/waiting rig with a diagnostic, never fall through to SELF or the previous scene's target.

For imported records without an explicit target, keep **Legacy binding** with the old fields and precedence. This preserves intentional Director-role-following, unresolved roles, group masks, and the legacy arbitrary SELF anchor. Do not resolve every role to a UUID once and silently change its future behavior. The editor projects that binding onto the three segments, shows current names, and adds **Follows a saved Director target** where applicable. An unresolved role appears as **Saved target unassigned — choose a Cast member**; do not invent its identity. Compatibility is allowed to retain a target outside Cast because that is what old files did; make this visible as a saved target, not as a normal roster member.

A deliberate new target selection replaces the entire legacy binding with an explicit one. For group editing, show all retained missing entries; an explicit replacement checklist commit uses the displayed chosen identities, so unresolved entries require explicit removal/replacement rather than being silently dropped. Changing a preset, renaming a rig, viewing another tab, or toggling On must not perform this conversion. Re-saving untouched legacy bindings preserves them, including their later Subject-following behavior. New explicit bindings ignore subsequent Subject reassignment.

Scene loading already restores Cast before light rigs (`llfloaterdirector.cpp:4308`, `:4452`). Preserve that ordering, including ghost-instance identity restoration. Persist all allocated Off/Waiting rigs and their missing references, metadata, selected rig ID/position, parameters, cues, and supported transport state. Apply a new structured record atomically; do not combine a previous target with newly loaded parameters. Existing flat settings remain the editing buffer, not the authoritative new target store.

New-format scenes are supported by the new viewer; do not promise lossless use in older binaries that do not understand direct person/group targets. Do not write a misleading old-style fallback that lights You for an unrepresentable group. Old files remain untouched until an explicit save.

### Presets and reset boundaries

- Built-in/user lighting setups and Easy looks retain their current artistic scope. They neither create rigs nor change target identities, rig names, or group membership. A look preset is reusable across actors. Scene save owns assignments.
- Preserve fixture, Actor only, FX/cue and transition behavior according to their existing setup contracts; do not expand preset scope as part of this change.
- Define **Reset lighting** as resetting the selected rig's lighting parameters while retaining its name/target. **Remove rig** removes that rig. The existing confirmed global reset (`alpanelcinelightrig.cpp:2169`, `alcinelightrigmanager.cpp:756`) must be explicitly labeled **Reset all rigs**, clear allocated rigs/targets, and leave an empty v2 rig list. It must not reintroduce a default You target or modify Director Subjects.
- No-block/old-scene and malformed-data paths need explicit clean target handling for all affected records; do not inherit new target metadata from the preceding scene.

### History and undo

Current rig switching suspends setting writes then calls `history().barrier("rig slot changed")` (`alpanelcinelightrig.cpp:1684–1690`). Current context strings only cover some fixtures/lights and use slot labels (`:969–981`); the XML `undo_context` attribute alone does not supply complete model history. It is unsafe to assume the mockup's Remove/Undo already works.

Use `ALKitHistory::recordCustom()` (`alkithistory.cpp:201`) for atomic target and lifecycle commands. Record before/after authored state keyed by stable rig ID and scene generation, not a mutable name or just a reused position. One checklist click, mode switch, object clear/pick, create, remove, or rename is one undo step; passive refreshes, selection, and availability changes are not.

Finish open gestures before changing edited rigs. Replace the blanket rig-selection barrier for these model commands so clicking another rig does not erase Remove Undo. Parameter/widget history must retain correct rig/fixture context and its existing conflict checks (`alkithistory.cpp:406`): replay must route to the recorded rig or refuse with an explanation, never apply its EV/color to whichever rig is selected now. Extend context coverage to all per-rig settings; do not accidentally scope shared probe/volumetric settings as per-rig. Avoid recording both a model command and a duplicate series of widget commits.

Remove stores enough authored data to restore the same rig, name, target, cue list, On state, and owned associations. Undo restores its original position only if free or still owned by that ID; never overwrite a newer rig that reused it. Ordinary chronological undo of the newer creation frees it first. Scene load/global reset retains a scene history boundary. Availability while undoing is recomputed: restoring a missing person restores a Waiting target, not a different actor. History is session state, not a new on-disk scene feature.

## Disposition of the current uncommitted work

Reviewed `git diff`; do not revert, stage, or overwrite this work during this specification task.

| Working-tree change | Keep or supersede during implementation |
|---|---|
| `alpanelcinelightrig.cpp:1508` / `.h:38`: `rigTargetDescription()` | Keep the intent to distinguish rig from effective target. Supersede the implementation with a model-owned summary. It names groups only from resolved slots, losing missing requested actors, uses an object UUID prefix, and labels a solo SELF legacy arbitrary anchor as You. Configured identity and readiness need separate output. |
| `alpanelcinelightrig.cpp:1544–1643`, `.h:261`: selector filtering/cache fields | Retain visibility of selected, active, and saved Off rigs. Supersede the subject-assigned/slot-letter rule with allocated named rigs. The current filter can hide an Off object/group rig with an unassigned owning Subject; then it is inaccessible until something else selects it. Do not treat hidden as free. |
| `alkitpage.cpp:149`: editing chip uses full combo label | Keep this idea temporarily; the named-rig strip becomes the final source of selection text. Do not reuse actor-only label shortening for rig names. |
| `alkitpage.cpp:1637–1677`, `.h` removed hint-cache members | Supersede. The new `has_subjects` check confuses “no Subjects assigned” with “no Cast.” Its “Also on” text still uses letters. Removing the cache causes repeated summary/string construction from the draw path (`:1941–1952`), including possible UUID formatting through the helper. Replace with manager configuration/availability revisions, not per-frame text comparisons. |
| `alpanellensgaze.cpp:890`, `:1022`, `.h:132`: gaze selector identity/selection retention | Keep separate. It addresses gaze's slot edit context and avoiding an automatic fallback to All. This lighting redesign neither replaces nor authorizes reverting it; it still needs its own review/validation. |
| `doc/director_kit/page_sources.json` | Current diff is eleven source line-number shifts. Do not hand-edit it to implement behavior. Regenerate registry/docs after the final UI wiring using the existing generator. |
| Concurrent transport/layout edits | During this review, changes also appeared in `alkitbar.cpp:144`, `director_kit_gen.py:1429–1481`, and generated XUI/report files, including the Rig page's source-bundle comment. These concern ACTION/CUT icon centering and transport text alignment, not rig targets. Preserve them separately; the target redesign does not supersede them. No generator was run by this specification task. |

The present changes do not filter the Who's lit A–D rows into actual Cast people. Also, hiding only a child switch can leave its Kit row label/layout behind. Any interim filter must operate on the complete row, preserve selected missing references, and cover both the Kit page and legacy panel. The final dynamic target editor supersedes this fixed five-row surface.

## Implementation slices and validation

These are development/review units, not permission to ship an engine with no usable UI. The named-rig feature lands as one complete reachable workflow after the slices converge; only a self-contained corrective fix should ship separately. This follows the feature-completeness rule in `CLAUDE.md`. No slice is implemented by this document.

| Slice | Concrete work and starting locations | Main risk | Required evidence |
|---|---|---|---|
| 1. Prove mutation paths; contain current ambiguity | Instrument actual target mutations/restore/replay; add passive-sync guard if justified. Clarify current header as group override pending replacement. `alpanelcinelightrig.cpp:1776`, `:1804`; `alcinelightrigmanager.cpp:321`, `:726`, `:1581`; `panel_dpage_light_cine_light_rig.xml:137`. | Blaming forced refresh or destroying legitimate saved groups. | One discriminating reproduction above; opening/revealing/switching/loading setups must not mutate targets. A header/You edit affects only the chosen rig. |
| 2. Rig identity, target record, compatible codec | Add allocated/ID/name/target metadata outside selected settings capture; versioned read/write, legacy-binding adapter, conservative slot import, explicit empty-state/reset semantics. `alcinelightrigmanager.h:185`, `:401`, `:493`, `:1380`; `.cpp:707`, `:756`, `:1566–1816`; `alcinelightrig.cpp:4224`, `:4280`; `llfloaterdirector.cpp:4043`, `:4308`, `:4452`. | Losing Off rigs/cues, null→self alias, partial restore, runtime capture wiping metadata. | Field-exact round trips of all five positions, empty scene, rename, unavailable identities, legacy arbitrary anchor/groups, v1 import and malformed/unknown formats. Read old files without rewriting them. |
| 3. Shared resolution and renderer consumers | New direct actor/group path, preserving existing centre/scale math and tracking; cached availability; wire Actor Mask, coverage readiness, focus/probe consumers. `alcinelightrig.cpp:164`, `:1062`, `:2499–2646`, `:3049`; `alcinelightrigmanager.cpp:789`, `:914`, `:1037`, `:1138`; `.h:755`. | Correct aiming with wrong receiver bits, group lead churn, stale emitters, Actor Mask overflow. | Arbitrary unassigned-to-Subject actor works from every storage position; two rigs on one actor; five-person group; all/partial/no availability; six-recipient boundary and rejected seventh; object and Actor-only off paths preserved. |
| 4. Reachable named-rig editor and model undo | Strip, target segment/list/checklist, object selection, summaries, reverse panel, limit, remove/rename, stable-ID commands; remove this page's follow-Cast adapter. `alpanelcinelightrig.cpp:661`, `:792`, `:969–1009`, `:1508–1891`; `.h`; `alkitpage.cpp:149`, `:1637`, `:1700`, `:1941`; `alkithistory.cpp:201`, `:406`, `:449`; both Light Rig XUI files. | Selection retargets lights; reused positions corrupt undo; missing actors disappear; minimum-width clipping. | End-to-end create → select targets → edit lighting → switch rigs → remove → undo/redo; no-selection/multi-selection defaults; object cancel/clear; saved missing references; full-capacity states; keyboard focus and narrow/wide layouts. |
| 5. Integration and release gate | Named probe labels; honest independent Night Mask labels; refresh gates; registry/docs regeneration and regression. `panel_dpage_light_cine_light_rig.xml:102`, `:792`, `:879`; `alpanelcinelightrig.cpp:2787`, `:2982`; `pipeline.cpp:14409`; `scripts/code_tools/director_kit_gen.py`; existing model/functional tests. | Letters/old semantics survive elsewhere, misleading readiness, UI hitch, unrelated gaze changes bundled. | Saved-scene reopen, setup/Easy/random/cue behavior, probe removal/reuse, Night Mask unchanged binding, camera/gaze unchanged by all lighting actions; layout/draw lint and in-world visual validation. |

Reuse meaningful model test coverage rather than adding checkbox-value mirror tests: group geometry at `tests/alcinelightrigmodel_test.cpp:1946`, slot/blob/settings routing at `:2163–2200`, migration at `:2383`, cue storage at `:3438`, and Actor Mask identity/overflow/scene tests at `:6924–7195`. Its TUT group declares an explicit test count (`:533`); increase that count if adding cases. Preserve `tests/lldirectorcast_choice_test.cpp:53`'s existing always-offer-You contract for other consumers; new lighting membership tests need their own roster contract.

Essential acceptance scenarios:

1. **Operator absent:** empty Cast, then Roddra-only Cast. Opening the rig page creates nothing. New without an explicit selection stays unassigned/off; New with Roddra selected targets Roddra. No You target is silently introduced. Ordinary world-light spill remains subject to Actor only.
2. **No Subject hijack:** assign A/B to camera/gaze actors, then light a third unassigned Cast member from each internal rig position. Capture Subjects and camera/gaze authored state before/after create, retarget, remove, reset, undo, and applying a rig block against already restored Cast. Lighting commands leave them unchanged. A full scene load still restores that scene's saved camera/gaze state as usual.
3. **Reported saved case:** load A unassigned plus an enabled A group targeting You. Import preserves the look, displays its actual target, and does not call it “Subject A lights Subject A.” Explicitly choose Roddra; that rig now follows Roddra and has no active hidden You/group/object override. Other rigs remain unchanged.
4. **Group integrity:** select two actors, remove one from Cast, take the other out of world, re-enroll/return them, and reorder roster display. Mode and requested identities persist; centres/status/receiver bits update together. Off-camera actors remain valid targets. Test mixed scale, duplicates/self aliases, ghost and animesh identities.
5. **History/capacity:** five allocated rigs including Off rigs; sixth creation rejected; remove/undo preserves all authored data; slot reuse has a new ID; selecting rigs and renaming cannot redirect old history. Scene boundaries block stale replay. Actor-only seventh-recipient rejection is visible and leaves prior assignments intact.
6. **Truthful coverage:** On with no active fixtures, target unavailable, Actor only off/on, objects, overlapping rigs, and possible world spill never become a false “actual illumination measured” claim. Probe and Night Mask states are separate from actor assignment.

Future implementation must run relevant model/functional checks plus `xui_layout_check.py` and `ui_draw_lint.py` for changed UI code, including hidden/reveal and minimum-width cases. Use Python only by its supplied full path: `C:\Users\xianw\AppData\Local\Python\pythoncore-3.14-64\python.exe`. Obtain the mandated adversarial code review before a build; build only after review convergence, and leave in-world verification explicitly pending until the user runs the viewer. This review ran source/diff inspection and viewed the supplied screenshots; it did not run tests or the viewer.

## Open questions for the user

1. Is **five named rigs**, with separate rigs for individual actor lighting, sufficient for the first version, or is “Each person” essential now?
2. For new scenes, should You appear only after **Add You to Cast**, as proposed, or remain an optional explicit choice even when not enrolled? Either way, it would never be the silent default.
3. Is it useful to keep the reverse **Who's lit** panel visible beside the editor, or should it be collapsible by default to leave more room for the light map and fixtures?
4. Should imported scenes keep their existing Director-target-following bindings until explicitly retargeted, as proposed, or should import intentionally pin them to the actors currently assigned?
5. Is **Reset lighting** for the selected rig plus a separately labeled **Reset all rigs** the intended reset behavior?
