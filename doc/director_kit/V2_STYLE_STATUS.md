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
| Verification probe | The Director-v2 audit writes `<capture>.style.json` once per default page × size, recording key widget role/name, background token+RGBA, IBM font descriptor/size/weight, and floater-space rect. The driver requires 36 probes at each of 9 audit sizes. |

## Deliberate limitations pending runtime comparison

- Viewer widget primitives do not expose CSS box shadows, letter-spacing, or asymmetric split-button corner radii. The implementation uses the nearest native 9-slice/focus-border treatment; no global widget renderer was changed for those cosmetic-only effects.
- `LLScrollListCtrl` has no embedded rounded-tag cell primitive. Cast rows therefore keep their stable pastel group tag as a tinted name suffix while the list background, hover, selection, status, typography, and header/tool chrome use kit tokens.
- The light-map backdrop is a native two-layer radial approximation rather than the browser SVG/CSS gradient. Its interaction geometry and token palette are preserved.
- Exact raster/font metrics and dropdown/list internals require the orchestrator's generator, `/Zs`, lint/parity gates, build, and mockup side-by-side; none were run in this write-only round.
