# Ghost Studio panel UI redesign — de-crowd, use vertical space, reflow on resize

## Goal (user, machinima operator)
The Ghost Studio panel is **VERY crowded**. Redesign the layout so it:
1. Stops feeling cramped — real breathing room between controls and sections.
2. **Uses vertical space** — the operator has screen room below; a taller panel is welcome.
3. **Benefits from a resized window** — right now resizing the floater does nothing
   useful; controls should reflow/widen and more should become visible as the window grows.

This is a **layout redesign**, not a feature change. Every existing control, label,
tooltip, and behavior stays; only the arrangement/sizing/anchoring changes.

## Current state (why it's crowded) — `skins/default/xui/en/panel_ghost_studio.xml`
- Fixed `panel` 360x480, **every widget absolutely positioned** (`layout="topleft"` with
  hardcoded `left`/`top`/`width`/`height`) and `follows="left|top"`. No `layout_stack`, no
  scroll container. So nothing reflows; a wider/taller window just adds empty space.
- Labels truncate at the default size because widths are too tight, e.g.:
  `btn_ghost_dup` "Duplicate" width=54 -> "Duplicat"; `btn_ghost_del` "Delete" width=38 ->
  "Delet"; `ghost_type` width=102 -> "Type: Entity clo..."; `source_combo`/`btn_ghost_add`
  "Add Overlay Ghost" clipped; `studio_hint`, `pose_status` ellipsize.
- Logical groups already exist and should become explicit sections:
  1. **Instances** — `studio_header`, `show_all_check`, `studio_hint`, `edit_ghosts_check`,
     `ghost_list` (scroll_list), `source_combo`, `btn_ghost_add` (flyout), `btn_ghost_dup`,
     `btn_ghost_del`, `btn_ghost_refresh`.
  2. **Placement** — `ghost_type`, `ghost_name`, `place_lbl`, `pos_x/y/z_spinner`,
     `yaw_spinner`, `heading_dial` (the compass), `scale_spinner`, `btn_place`,
     `btn_to_actor`, `btn_to_me`.
  3. **Look** — `look_lbl`, `style_combo`, `actor_tint_check`, `hue_slider`, `alpha_slider`,
     `pixel_slider`, `shimmer_speed_slider`, `shimmer_amount_slider`, `glitch_slider`,
     `brightness_slider`.
  4. **Pose & Animation** — `pose_lbl`, `btn_freeze`, `btn_live`, `pose_status`,
     `drive_mode_combo`, `chaos_check`, `chaos_slider`, `entity_look_combo`,
     `directed_anim_editor`.
  5. **Array** — `array_count_spinner`, `array_spacing_spinner`, `btn_array_line`,
     `btn_array_ring`.
  6. **Status** — `studio_status`.

## HARD CONSTRAINTS (do not violate)
1. **Dual-container.** The SAME panel is embedded by two hosts — it must look right and
   function in BOTH:
   - Standalone: `floater_ghost_studio.xml` — `can_resize="true"`, panel `follows="all"`.
   - Director Console: `floater_director.xml` "Ghosts" tab (`ghosts_tab`, ~374x550), panel
     `follows="left|top|right"`, width 354. This tab has a **fixed height**, so tall content
     MUST live in a scroll container (or accordion) so it scrolls instead of clipping.
2. **Preserve every control `name=` and type.** `ALPanelGhostStudio` (alpanelghoststudio.cpp)
   binds each control via `getChild<T>("<name>")` (recursive, so nesting inside new
   containers is fine). Renaming, removing, or retyping ANY control breaks the C++ binding.
   Before finishing, grep `alpanelghoststudio.cpp`/`.h` for every `getChild`/`findChild`
   string literal and verify each name still exists in the new XML. The set of `name=`
   attributes must be a **superset** of today's (new container panels may add names; none may
   disappear).
3. **XML-layout-first.** Prefer a pure-XML restructure. Do not change control callbacks or
   `ALPanelGhostStudio` logic. If you believe a code change is genuinely required (it should
   not be), STOP and flag it in the report instead of guessing.
4. Keep all tooltips and initial values.

## Direction (recommended shape — adapt as the widgets require)
- Replace absolute positioning with a **vertical `layout_stack`** (`orientation="vertical"`)
  of section `panel`s in the order above, wrapped in an **`LLScrollContainer`** (or use an
  **accordion**, `LLAccordionCtrl`, if it fits the codebase idiom cleanly and you can
  preserve names) so the column scrolls in the fixed-height console tab and reveals more as
  the standalone floater grows taller.
- Inside each section, controls that should **widen with the window** (`ghost_list`, all
  sliders, `directed_anim_editor`, the hint/status texts) get `follows="left|top|right"` (or
  live in an auto-width `layout_panel`) with sane minimums. Fixed-size controls (the compass
  `heading_dial`, spinners) keep their size but sit in a row that reflows.
- **Fix every truncation** at the default width: widen Duplicate/Delete/Refresh, the
  `Type:` label, the Add-Overlay flyout + source combo, and the hint/status texts so nothing
  ellipsizes at the default size.
- Add clear section headers (reuse the existing `*_lbl` bold texts) and generous vertical
  padding between sections — the operator explicitly has vertical room.
- **Standalone floater** (`floater_ghost_studio.xml`): bump the default height (and maybe
  width) to a comfortable size that shows the whole stack without scrolling; set sensible
  `min_height`/`min_width`; keep panel `follows="all"`.
- **Console tab** (`floater_director.xml`): keep the panel `follows="left|top|right"`; rely
  on the panel's own scroll container for overflow. Do not enlarge the console.
- Optional (nice-to-have, only if clean): make sections collapsible so operators hide groups
  they aren't using.

## Validation & expectations
- **You cannot see the rendered UI.** Layout work is blind for you, so this WILL need
  in-world visual iteration by the user (Claude builds; the user eyeballs it). Optimize for a
  strong, safe first pass, not a guaranteed-perfect one.
- Self-review before returning and converge to **0 must-fix**, explicitly checking:
  - XML is well-formed and every element has valid attributes for its widget type.
  - **Name preservation:** produce the before/after list of control `name=` and confirm none
    were dropped/renamed; confirm each `getChild` literal in the C++ resolves.
  - `follows`/anchoring is correct for both containers; nothing relies on absolute `top`
    that a stack will override.
  - Content **scrolls** (not clips) at the console tab's fixed height, and **reflows/widens**
    when the standalone floater is enlarged and stays usable at `min_*`.
- Report: the new structure, the container widget chosen (layout_stack+scroll vs accordion)
  and why, the truncations fixed, any control that had to move between sections, and confirm
  no C++ changes (or flag the one you think is needed).
