# Director v2 style implementation status

Implemented from `style_tokens.json` and the approved `Director.dc.html` rendering. Kit styling is scoped to generated Director-v2 XUI and `ALKit*` instances; global widget defaults and Director v1 remain unchanged.

| Token area | Implementation |
|---|---|
| Colours | `colors.xml` contains named `Kit*` entries for every solid palette colour, light-map scene colour, and RGBA overlay/shadow token. `ALKitStyle` caches the runtime subset once. |
| Typography | `fonts.xml` adds Kit10–Kit15 sizes. Kit-created labels/buttons use IBM Plex Sans; numeric values, dials, REC, and timecode use IBM Plex Mono. `styleTree()` applies this only beneath the v2 shell/page. |
| Radii / borders | Cards use cached `Kit_RoundRect6` outer/inner 9-slices; controls use `Kit_RoundRect4`; pills/switches use `Kit_Pill9`/`Kit_Circle`; popovers use radius 6. The PNG generator adds radius 8 plus slider track/thumb masks. |
| Rail / tabs / modes / chips | 176×40 rail rows use the supplied white tintable icons, left labels, active fill and a painted 6 px amber trailing dot. Page tabs use 13 px text and a 2 px amber underline. Mode/chip selection uses token fills; hidden chips do not reserve space. |
| Page chrome | Presets, reset/undo/redo icons, who pill, dark search field, amber master switch, warning band, and rounded buttons are laid out in their token-height bands. Native title becomes `Director › Group › Page`. |
| Cards / rows / controls | 36 px card header, 13 px semibold title, token reset icons, 12 px labels, mono values, counted More-options link, dark rounded button/combo/editor styling, amber/grey slider tracks with white thumb, 34×18 switches, swatch border, and token hover/flash colours. |
| Help / roster / transport | Help pane uses token hierarchy and sizes. Roster tools share the CAST heading. Transport uses the supplied action/play/stop/marks/legend icons, amber/red actions, mono timecode/REC, token buttons and compact responsive collapse order. |
| Dials / light map | Compass/arc use the token face, bezel, tick, range, amber needle and IBM Plex labels. Light map uses the approved dark-blue centre/edge/frame and Key/Fill/Back/BG palette. |
| Assets | Every supplied `<name>_16.png` and `_32.png` is registered in `textures.xml`; images remain white RGBA and are tinted by widgets/draw code. Runtime textures are cached outside draw paths. |
| Verification probe | The Director-v2 audit writes `<capture>.style.json` after each default framebuffer capture, recording measured RGB8 pixels, expected token RGB8, delta/pass, rendered text/caret/state checks, font descriptors, and floater-space rects. The driver requires and parses 36 probes at each of 9 audit sizes. |

## Round 3 corrections

- Every kit combo now paints the registered right-aligned chevron and reserves label padding; generated XUI carries the same contract.
- `styleTree()` stops at stock compound widgets. It skins a kit combo/spinner/slider/list as a whole, but never overwrites checkbox/radio, spinner, scrollbar, list-header, combo, or legacy-tab child buttons. Legacy page interiors are deliberately not traversed.
- Buttons support a separate selected tint with backward-compatible single-tint defaults. Mode segments, option segments, chips, tabs and rail controls now have visibly distinct active treatment; hover glow, pressed tint and disabled colours remain token-driven. Late-created Easy/Advanced buttons are styled in `refreshModeProxy()`.
- Kit search/spinner/slider editors use the dark input colour; numeric editors use Plex Mono. Search paints the magnifier and reserves left padding. Kit scrollers/lists use token track/thumb colours without replacing their arrow glyphs.
- Section chips use the pill 9-slice and live visible-card counts. Row/card reset buttons have transparent backgrounds and retain faint/amber default-state glyphs.
- Programmatic reset/who/search/cast popover controls receive the same scoped kit styling.
- The style probe now samples the captured framebuffer, records measured and expected RGB8 values/deltas, scans rendered text and caret pixels, compares selected/unselected pixels, and makes every mismatch fatal.

## Legacy-page boundary

Rig, Rim, Bloom & Glow, and Tron are the currently rolled-out kit-native pages. The other 31 legacy page interiors intentionally retain their v1 controls until WP6+; only their Director-v2 frame/chrome is kit-styled. This is why compound descendants and legacy bodies are excluded from restyling, while the PNG variance gate still proves that their controls actually rendered.

## Deliberate limitations pending runtime comparison

- Viewer widget primitives do not expose CSS box shadows, letter-spacing, or asymmetric split-button corner radii. The implementation uses the nearest native 9-slice/focus-border treatment; no global widget renderer was changed for those cosmetic-only effects.
- `LLScrollListCtrl` has no embedded rounded-tag cell primitive. Cast rows therefore keep their stable pastel group tag as a tinted name suffix while the list background, hover, selection, status, typography, and header/tool chrome use kit tokens.
- The light-map backdrop is native GL vertex-colour art rather than browser SVG. It now carries the token radial sphere, lighter front hemisphere, floor/orbit lines, avatar and light glow without a per-frame texture allocation.
- Exact raster/font metrics still require a post-build capture and mockup side-by-side. This round intentionally did not build; token, kit-page, XML, draw-lint, and scoped whitespace gates passed (the whole-worktree whitespace gate still sees unrelated pre-existing XML whitespace).

## Visual fix round 4 ledger (2026-10-10)

| # | Result |
|---|---|
| 1 | Number rows now keep their dark numeric field and spinner arrows visible at rest; the Rig EV setting remains bound through its generated `control_name`, so disabled Easy-mode state no longer looks like an empty row. |
| 2 | The Acting-on pill budgets the measured name, person icon, rail tag and caret independently. Its right text padding protects the tag even if the available band becomes narrower. |
| 3 | The blue preset highlight came from editable `LLComboBox::onFocusLost()` selecting its editor after programmatic selection. The kit installs a deferred editor focus-loss deselect, disables select-all-on-commit, and audits every resting line editor. |
| 4 | Compass and arc tokens are now 60 px in 94 px bordered mini-cells, with the existing mono degree readout beside each dial. Card preset width is 160 px, removing the Fixture preset truncation. |
| 5 | The 51 Rig overlaps were the three Light-map view buttons intentionally occupying the map's reserved footer in 17 states. That composite relationship is now excluded; arbitrary sibling overlap remains fatal. |
| 6 | Transport fit read the flex item's stale hidden state from the prior narrow pass, then restored it after measuring; that undercount pushed Cast and `?` outside every page. Measurement now uses natural/collapsed state consistently, with the same padding as layout. |
| 7 | The v2 Path host restores the runtime-shrunk Nodes & Playback panel to its authored 304 px, containing Play/Stop without changing `panel_path_editor.xml`. |
| 8 | Text audit records only non-empty, visible, on-screen, positive-alpha runs; glyph width uses the font's drawn text width, and rectangles are clipped to the widget and enclosing scroller viewport. Findings require more than 2 px in both axes and different text widgets. |
| 9 | A control's own label/descendant text is excluded from `TEXT_ON_CONTROL`; unrelated interactive areas remain fatal. The calibration fixture's expected Rig positive is the round-1 light-map key/drag-hint intersection; a 2 px edge touch is expected clean. |
| 10 | Scope is v2 kit code and self-test only. Legacy v1 XUI is unchanged; no build or commit was performed. |

## Visual fix round ledger (2026-10-10)

| # | Result |
|---|---|
| 1 | Fixed: widget-host order is now XUI order; Rig reads preset combo, Save as..., trash, reset, undo, redo, divider, master and search. Preset focus is explicitly cleared after reparenting. |
| 2 | Fixed: the Who band now has a separate caps label, Acting on pill with rail marker, and the full rail-follow note. Pages without a Who picker no longer paint an orphan band. |
| 3 | Fixed: Rig uses filtering chips for the approved Lights-only state; pills carry live counts and explicit amber selected text/fill. |
| 4 | Fixed: mode/selector segments now set both selected and unselected colours explicitly. Audit mode changes use the user path, so Advanced is applied instead of passively preserving Easy. |
| 5 | Fixed: card traversal restores authored Key, Fill, Back, BG and EV, Colour, paired Yaw/Pitch, Gel, Gobo, Shaft order. This also restores the 48 px paired compass/arc cells and readouts. |
| 6 | Fixed: map has radial sphere shading, orbit cues, avatar/camera/light art, four-column legend with amber selection, non-overprinting symbolic key/hint, and Top/Side/2.5D below the map. |
| 7 | Existing row geometry retained: values occupy the right-hand 52 px column on the same 28 px row; no additional below-row value widget is created. Runtime screenshot recheck remains required. |
| 8 | Fixed: reversing row discovery restores section/card flow, and a combined card no longer repeats a first subheading identical to its title. |
| 9 | Existing native title integration already emits `Director › Group › Page`; native window buttons remain platform-owned. |
| 10 | Fixed causes covered here: reversed descendants, indistinct state colours, orphan bands, audit mode no-op, map overprint, and minimum-width transport spill. Runtime-only findings require the requested post-build capture. |
| 11 | Cheap reductions: removed two redundant full-page mode/chip/layout passes during native page construction. The remaining likely cost is eager construction/binding of every Rig card/row; deeper lazy-card work is not safe for tonight without a build/profile cycle. |

## Visual fix round 2 ledger (2026-10-10)

| # | Result |
|---|---|
| 1 | The map now reserves independent 40 px legend, 28 px View/key, and 18 px hint bands. View segments overlay only their reserved band; the key is right-aligned, the hint has its own baseline, and CAM remains inside the canvas. |
| 2 | Reparented editable preset combos explicitly clear child-editor focus and selection, so refresh/build leaves plain text. |
| 3 | Slider child editors are forced into the row's 52 px value column and 20 px vertical slot; numeric spinners have a compact 52 px layout with no duplicate unit label. Both are mono/right-aligned and transparent at rest; spinner arrows and the subtle field appear only on hover/focus. |
| 4 | Card reveal now reapplies mode/chip visibility. The Lights chip therefore reveals `light_map` then `lights` in row-major order; the 340 px grid threshold still controls the two-to-one-column break. |
| 5 | The Acting-on control measures its live cast-member label plus person icon, paints `rail` as a distinct compact inset tag with its own caret, and never stretches to the remaining band width. |
| 6 | Easy/Advanced uses a dark unselected container and a light selected segment with dark text; card-owned segment colours are no longer overwritten by the generic style pass. |
| 7 | The flat concentric-disc approximation was replaced by a vertex-colour radial sphere and front hemisphere using the documented light-map scene palette. |
| 8 | Compact transport collapse now removes the optional hotkey/scene actions before the required Cast access, eliminating the 900/minimum right spill. The segment style overwrite was the cause of Rim's identical-state probe. |
| 9 | The live audit now keeps a self-test-only registry of drawn kit text and interactive rectangles and emits `TEXT_OVERLAP` / `TEXT_ON_CONTROL`; it also emits `VALUE_OUTSIDE_ROW` when a slider/spinner value leaves its owner row. |
| 10 | Round-1 proof: its map key and drag hint were registered at the identical `MAP_LEGEND_H + 7` baseline, so their measured glyph rectangles intersect and produce `TEXT_OVERLAP`; the old CAM baseline also intersected that footer region. The round-2 three-band geometry separates all three before drawing. |
| 11 | Scope remains Director v2 only. No build or commit was performed; real screenshot confirmation is pending the caller's build/capture pass. |
