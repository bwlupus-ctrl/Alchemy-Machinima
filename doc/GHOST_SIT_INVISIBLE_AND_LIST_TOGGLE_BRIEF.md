# Two clone bugs: invisible-when-source-sits, and the list dot not toggling

Both are entity-clone bugs, unrelated to the (now reverted) local avatar scale work.

---

## BUG 1 — Clone turns INVISIBLE when the source avatar SITS  ⭐ (the real one)

### In-world report
> "when sitting, regardless of scale, the clone will turn invisible, mimicking the true avatar.
> but if the avatar stands, the clone appears."

Fully reproducible and reversible: source sits -> clone vanishes; source stands -> clone returns.
Independent of scale (happens at scale 1.0 too).

### Why this matters
An entity clone is supposed to be an INDEPENDENT client-only body. A seated source is an extremely
common machinima setup (posing on furniture, vehicles, pose stands), so a clone that disappears
whenever its source sits is a serious limitation.

### Where to look / hypotheses (investigate, confirm ONE, then fix)
- **H1 — `FORCE_INVISIBLE` spuriously set.** `set_entity_clone_visible()` (alghoststudio.cpp ~60-80)
  drives visibility with `LLDrawable::FORCE_INVISIBLE` plus the persistent
  `LLGhostAvatar::mEntityCloneVisible` gate. Its own comment warns that FORCE_INVISIBLE *"can be
  cleared by ordinary viewer-object update bookkeeping"* — so the inverse is plausible: sitting
  triggers bookkeeping that SETS it (or resets drawable state) on the clone. Check every path that
  sets FORCE_INVISIBLE or rebuilds drawable state when an avatar sits/stands, and whether it can
  reach the clone's drawable or its cloned attachment children.
- **H2 — Avatar-pool render gate.** `LLDrawPoolAvatar` (~lines 80-90) skips a ghost whose
  `isEntityCloneVisible()` is false. Confirm the gate is not being tripped, and check any
  visibility/`updateVisibility()`/`mVisible`/pixel-area path that a seated state changes.
- **H3 — Sit reparenting leaks to the clone.** When the source sits it is reparented onto the sit
  target and its root transform changes. If the clone mirrors root/parent state (or its
  `LLDrawable`/`mXform` parent is derived from the source), the clone could end up transformed to an
  invalid/enclosed position and be culled rather than actually hidden. Distinguish **hidden**
  (visibility flag) from **culled/mispositioned** — they need different fixes.
- **H4 — Mirrored state.** In `MIRROR` drive mode `LLGhostAvatar::idleUpdate()` copies the source's
  signaled animations. Check nothing else about seated state (attachment visibility, HUD/skirt
  handling, `mIsSitting`-conditioned rendering) rides along.

Note `LLGhostAvatar` already overrides `isImpostor()` to return false, so ordinary impostoring is
probably not the cause — but verify rather than assume.

### Requirements
- A clone must stay visible and independently posed while its source sits AND while it stands.
- Standing/sitting the source must not change the clone's visibility at all.
- Client-only invariant: nothing to the simulator.
- If the clone is being *culled* rather than *hidden*, fix the transform/extent cause, not by
  forcing visibility on top of a broken position.
- **Add a short diagnostic** (single tag, e.g. `LL_INFOS("GhostSit")`, throttled) reporting on the
  clone when the source's seated state changes: `mEntityCloneVisible`, whether FORCE_INVISIBLE is
  set on the avatar drawable and on a sample attachment child, the clone's root world position, and
  visible/culled state. This is render behavior you cannot see; one in-world run should then confirm
  or refute the chosen hypothesis. (This instrumentation-first approach is what solved the camera
  bug in one round earlier today.)

---

## BUG 2 — The list "on" dot does not toggle visibility

### In-world report
> "the dot next to instances in ghost studio, I believe it's supposed to toggle invisibility and
> doesn't work"

### Confirmed cause (already diagnosed — just fix it)
The dot is a **display-only** cell: `alpanelghoststudio.cpp` builds
`row["columns"][0]["column"] = "on"` with a filled `●` / hollow `○` glyph reflecting
`inst.mEnabled` (~line 431). The actual toggle is bound to **double-clicking the row**
(`mList->setDoubleClickCallback(... onListDoubleClick ...)` ~line 156 -> `setInstanceEnabled`
~line 673). So clicking the dot does nothing — it only ever *shows* state. The underlying
`ALGhostStudio::setInstanceEnabled()` path looks correct (sets `mEnabled`, and for
`BACKING_ENTITY_CLONE` calls `set_entity_clone_visible(ghost, mShowAll && enabled)`).

### Fix
Make the dot behave the way an operator expects: a **single click on the "on" column toggles that
instance's visibility**, without disturbing row selection semantics elsewhere in the list (clicking
any other column must still just select the row). Keep double-click-to-toggle working so existing
muscle memory is not broken.

Also verify while you are there:
- The toggle genuinely hides/shows an ENTITY clone in-world (the path exists, but the same
  FORCE_INVISIBLE fragility flagged in Bug 1 may affect it — if Bug 1's cause also explains a flaky
  toggle, say so).
- The dot glyph refreshes immediately after toggling.
- The master "Show ghosts" checkbox still composes correctly (`mShowAll && enabled`).

---

## Constraints (both)
- Client-only; nothing to the simulator.
- Do NOT rename or remove any existing control `name=` — `ALPanelGhostStudio` binds via
  `getChild<T>(name)`.
- Do not reintroduce any local-avatar-scale code; that work was reverted deliberately (commit
  `ec82e1a49f8`). Clone scaling uses `LLGhostAvatar`'s own foot-pivoted implementation — leave it
  alone.
- Self-review to 0 must-fix. Report: the confirmed cause of Bug 1 (and which hypothesis it was),
  hidden-vs-culled, the diagnostic tag added, the Bug 2 fix, and confirm no control renames.
