# Master Preset Library + Scale-Aware Geometry — design seed

**Status:** seed for a deep design pass. No source modified.
**Captured:** 2026-08-16. Feeds `doc/CINE_LIGHT_RIG_MASTER_PRESETS_DESIGN.md`, which then feeds a
Codex implementation brief.

The Cinematic Light Rig feature is BUILT and compiles (see `doc/CINE_LIGHT_RIG_STATUS.md`). This is
a follow-on addition to it. Read `doc/CINEMATIC_LIGHT_RIG_DEEP_DESIGN.md` for the base architecture
and the two reference LSL files under `doc/reference/` for the original behaviour.

## What the user asked for (2026-08-16)

1. A **master preset set**: a curated, read-only, built-in library of named cinematic lighting
   setups, shipped with the viewer, **in addition to** the existing user-local presets.
2. Content: **reimagined**, borrowing ideas from the existing LSL setups but authored fresh for
   this system — NOT a verbatim port. (Claude authors the set; the design pass proposes it.)
3. Behaviour (user-chosen): **read-only built-ins, grouped separately** in the setup combo, above
   local presets. Saving always creates a local preset; a local must not shadow a master name.
4. **CRITICAL — scale awareness:** "it's very important that the system can account for clone
   scaling too." The rig can anchor to a ghost clone (or a uniform-scaled avatar), and a preset
   authored in absolute meters is wrong on a scaled subject.
5. "Do take great care as this is a forward-facing system." Ship-quality, user-facing.

## Verified facts (read from code — cite file:line in the design; label PROVES/INFERS)

### Current preset system (only ONE built-in today)
- `indra/newview/alcinelightrig.cpp`: `CLASSIC_SETUP_NAME = "Classic 3-Point"` (:47), the sole
  built-in, produced by `classicSetup()`. Local presets are LLSD `.xml` files under
  `<user_settings>/cine_light_rig/` (`presetsDir()` :1131, `presetPath()` :1143).
- `setupNames()` (:1148) returns Classic + the local files, flat (no grouping).
- `loadSetup()` (:1167) special-cases Classic, else reads the file; `saveSetup()` (:1198) refuses
  the Classic name; `deleteSetup()` (:1234) refuses Classic. Version gate: `version == 1`.
- The panel setup combo (`alpanelcinelightrig.cpp`) lists whatever `setupNames()` returns.

### Rig geometry and where scale must enter
- The rig centre already tracks scale: it reads the `mChest` joint world position
  (`alcinelightrig.cpp:923`), falling back to `getRenderPosition() + 1.2m` (:930). A scaled
  clone's chest sits at the correctly-scaled height, so the CENTRE is right for free.
- But the ORBIT RADIUS and HEIGHT are absolute meters from the setup/settings. The orbit sphere is
  `radius` around the centre; the projector light falloff radius is `gRadius * 2.2`, the omni's is
  `gRadius * 1.5`. On a 0.5x clone the chest is low (good) but lights still orbit at e.g. 1.5 m and
  the beams cover a full-size volume — framing and falloff are wrong. **This is the whole problem.**
- `CineLightRigOffsetZ` (:801, clamped -10..10) is added to the centre AFTER the joint — an
  absolute vertical nudge that also does not currently scale.

### The scale accessor (the unlock) — PROVES
- `LLVOAvatar::getUniformScale()` is **virtual**, base returns `1.f`
  (`indra/newview/llvoavatar.h:266`).
- `LLGhostAvatar` **overrides** it to return `mEntityScale`
  (`indra/newview/llghostavatar.h:116`), the clone's outer render scale.
- Ghost clone scale range is `GHOST_SCALE_MIN = 0.05f` .. `GHOST_SCALE_MAX = 150.f`
  (`indra/newview/alghoststudio.h:62-63`) — a 3000x span, so scale-awareness is essential, not
  cosmetic.
- The rig already holds the resolved `avatar` (`LLDirectorCast::instance().resolve(mAnchor)`,
  `alcinelightrig.cpp:835`), so `avatar->getUniformScale()` is a one-call, polymorphic read that is
  correct for a real avatar (1.0 unless the fork's avatar-uniform-scale sets otherwise — VERIFY
  whether real avatars ever return != 1.0) and for a clone (its entity scale).

## What the design pass must decide

### A. Scale-aware geometry (the hard part — do this first)
1. Confirm `getUniformScale()` is the right and complete signal. Does a uniform-scaled REAL avatar
   (the fork's avatar-scale feature) report through it, or only clones? If real avatars scale by a
   different mechanism, the rig must read both. VERIFY in code; do not assume.
2. Define exactly which quantities scale by the factor and which do not:
   - orbit radius — YES (framing distance)
   - projector/omni light falloff radius (`gRadius * 2.2`, `* 1.5`) — almost certainly YES
   - height / OffsetZ vertical framing — probably YES (so "eye-level key" stays eye-level)
   - EV / intensity — NO (exposure is not a distance) — but note inverse-square: the engine already
     has `EV_dist = log2(radius/1.5)`; if radius scales, does exposure drift, and is that correct
     (a closer light on a small subject IS brighter)? Work this through — it interacts with the
     headroom re-base.
   - emitter mesh scale (fixed 0.25 m, `EMITTER_SCALE`) — NO (invisible anyway) — but confirm the
     projector near-plane optics don't break at extreme subject scale.
3. Where is the factor applied — at preset load (bake into the live setup) or at render/tick (apply
   every frame)? Live-apply is required if the subject can be RESCALED while the rig is running
   (clones can). State how a mid-shot rescale behaves — snap or ease? The rig already has smoothing
   for the centre; scale should likely ride the same or a parallel smoothing.
4. Clamp/guard: scale can be 0.05..150. At 0.05 the orbit radius is 7.5 cm and the light falloff
   tiny; at 150 it is region-sized. Define sane clamps so the rig never produces a degenerate or
   region-filling light. Guard scale <= 0, NaN, and subject-lost (falls back to scale 1.0?).
5. Presets store geometry as NOMINAL (authored-at-scale-1.0); the rig scales at apply time. State
   this as the invariant and make it explicit in the on-disk format (a preset is defined for a
   1.0 subject; the rig adapts). Decide whether a user can override per-preset (a "don't scale"
   flag) — probably not for v1, but say so.

### B. Master preset library
1. Storage: a bundled read-only LLSD file in `app_settings/` (e.g.
   `app_settings/cine_light_rig_presets.xml`) shipped with the viewer, vs a hardcoded C++ table.
   Recommend one with reasons (a data file is editable without a rebuild and is the fork's idiom
   for shipped data; a C++ table can't be corrupted by the user). Consider: master presets must
   survive a viewer update cleanly and never be written to the user's local dir.
2. Model/controller changes: `setupNames()` must return two groups (master, local) with a stable
   ordering; `loadSetup()` must resolve a name to master-or-local (define precedence and forbid a
   local shadowing a master name at SAVE time, per the user's choice); `saveSetup()`/`deleteSetup()`
   must refuse every master name, not just Classic. Decide the fate of the existing "Classic 3-Point"
   — does it become the first master preset (recommended) or stay a special case?
3. UI: the setup combo shows two labelled groups (master above local). Specify how — a non-selectable
   header row, a separator, or a prefix. Cite whether LLComboBox supports group headers in this
   codebase or whether a prefix/separator is the pragmatic route. The Delete button must disable on
   any master selection (it currently disables only on Classic).
4. Scene round-trip and reset: a saved scene references a preset by NAME; a master name must resolve
   on load. Reset-to-default should select the first master preset. Confirm the 42-key scene
   round-trip still holds.
5. Versioning/forward-compat: the bundled file needs a version and a policy for a future viewer
   shipping more/renamed master presets while an old scene references an old name.

### C. The curated content (Claude authors; the design proposes the slate)
Propose a slate of master presets — reimagined cinematographer looks, not a verbatim LSL copy —
each fully specified in the setup format (per light: yaw, pitch, profile index, EV, beam index, on;
plus radius; all NOMINAL at scale 1.0). Draw on the classic vocabulary: e.g. Rembrandt, Loop,
Butterfly/Paramount, Split, Broad, Short, Film Noir / Low Key, High Key, Golden Hour, Blue Hour,
Silhouette/Backlit, Product/Clean, Interview 3-Point, Beauty Dish, Hard Cross. Use the existing 24
profiles and 3 beams. For EACH: name, one-line intent, and the exact per-light values, with a note
on how it should read on a scaled clone. Keep the count sensible for v1 (a tight, excellent set beats
a sprawling one). Flag any that need a 4th light vs 3.

## D. Two UI additions folded into this SAME delivery (user, 2026-08-16)

These ship with the master-preset/scale work because they reshape the SAME panel; they are not a
separate cycle.

1. **Per-control mini reset buttons** next to every adjustable value in the light panel (~35
   slider/spinner/combo controls). Mirror the WEATHER PANEL precedent exactly — it already does this:
   - XUI: an 18x18 `<button image_overlay="Refresh_Off" ...>` in a right-edge column
     (`panel_weather_settings.xml:46-61`, at `left="322" width="18"`), with
     `commit_callback.function="CineLightRig.ResetControl"` and
     `commit_callback.parameter="<TheSettingKey>"`.
   - C++: register one `CineLightRig.ResetControl` callback (like `registerWeatherResetControl()` at
     `alpanelweathersettings.cpp:35-51`) whose body is `control->resetToDefault(true);` on the named
     setting. One callback serves all buttons via the per-button parameter.
   - The design must lay out the panel with room for this reset column (the master-preset grouped
     combo ALSO changes the panel — design the whole layout once, coherently). Decide the exact set
     of controls that get a reset (all setting-backed sliders/spinners/combos; the seed editor and
     the per-light checkboxes/aim buttons are judgement calls — state which get one and why). The
     existing "Reset All" stays.
2. **Director Console icon for the Lights tab.** The Director tabs run through an icon map (referenced
   at `floater_director.xml:1636` — "the icon map + tab persistence stay wired"). The Lights tab needs
   an entry in that map plus an actual icon asset. The design must locate the icon map (likely in
   `llfloaterdirector.cpp`) and specify: the icon name/asset (an existing skin texture that reads as
   "lighting" if one fits, else a new bundled skin image), the map entry, and how it composes with the
   existing tab label. Verify how the OTHER tabs get their icons before specifying.

## Constraints
- Label PROVES / IMPLIES / INFERS; cite file:line for load-bearing claims.
- No source modified by the design pass.
- Ship-whole: master library + scale-awareness + curated content + UI + scene/reset wiring in one
  delivery. If any part must be deferred, say so up front with the reason.
- Concrete enough to become a Codex `--prompt-file` brief: named files, functions, settings keys,
  the on-disk preset-file schema, and an OFF-LIMITS list. Respect the existing feature's finals: the
  nine pipeline gate exemptions, the llselectmgr rejection, the three-way applySceneData split.
- This is forward-facing: prioritise robustness (degenerate scales, subject lost mid-shot, a master
  file that fails to load must fall back gracefully, never crash or blank the rig).
