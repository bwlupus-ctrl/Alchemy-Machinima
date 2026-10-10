# Actor FX — Director Kit page contract

Page key: `actor_style_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (0/18 setting-bound rows; 18 C++-referenced rows).

## Real owners

- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (18 matching rows).
- Referencing classes: `LLFloaterDirector` (56).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALActorStyleKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `actor` / `actor_style_target` (Actor) | `combo_box` `indra/newview/skins/default/xui/en/floater_director.xml:1529` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleTargetChanged()` (`indra/newview/llfloaterdirector.cpp:506`), wired in `LLFloaterDirector::bindActorStylePage()`; behavior `LLFloaterDirector::onActorStyleTargetChanged()` (`indra/newview/llfloaterdirector.cpp:1061`), wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `actor` / `actor_style_status` (Status) | `text` `indra/newview/skins/default/xui/en/floater_director.xml:1645` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `actor` / `actor_style_reset` (Reset style) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:1644` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleReset()` (`indra/newview/llfloaterdirector.cpp:528`), wired in `LLFloaterDirector::bindActorStylePage()`; behavior `LLFloaterDirector::onActorStyleReset()` (`indra/newview/llfloaterdirector.cpp:1083`), wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `actor` / `actor_style_hint` (About) | `text` `indra/newview/skins/default/xui/en/floater_director.xml:1514` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `look` / `actor_style_look` (Look) | `combo_box` `indra/newview/skins/default/xui/en/floater_director.xml:1566` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `look` / `actor_style_mode` (Render mode) | `combo_box` `indra/newview/skins/default/xui/en/floater_director.xml:1551` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `look` / `actor_style_alpha` (Opacity) | `slider` `indra/newview/skins/default/xui/en/floater_director.xml:1621` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `look` / `actor_style_dissolve_progress` (Dissolve) | `slider` `indra/newview/skins/default/xui/en/floater_director.xml:1622` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `color` / `actor_style_use_actor_hue` (Use actor hue) | `check_box` `indra/newview/skins/default/xui/en/floater_director.xml:1616` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `color` / `actor_style_hue` (Custom hue) | `slider` `indra/newview/skins/default/xui/en/floater_director.xml:1620` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `color` / `actor_style_brightness` (Brightness) | `slider` `indra/newview/skins/default/xui/en/floater_director.xml:1642` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `motion` / `actor_style_shimmer_amount` (Shimmer amt) | `slider` `indra/newview/skins/default/xui/en/floater_director.xml:1625` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `motion` / `actor_style_shimmer_speed` (Shimmer Hz) | `slider` `indra/newview/skins/default/xui/en/floater_director.xml:1624` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `motion` / `actor_style_glitch` (Glitch) | `slider` `indra/newview/skins/default/xui/en/floater_director.xml:1626` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `motion` / `actor_style_effect_fps` (Effect fps) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:1643` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `distortion` / `actor_style_distortion` (Distortion) | `combo_box` `indra/newview/skins/default/xui/en/floater_director.xml:1630` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `distortion` / `actor_style_distortion_amount` (Amount) | `slider` `indra/newview/skins/default/xui/en/floater_director.xml:1641` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |
| `distortion` / `actor_style_pixel` (Pixelate) | `slider` `indra/newview/skins/default/xui/en/floater_director.xml:1623` | `ALActorStyleKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onActorStyleControlChanged()` (`indra/newview/llfloaterdirector.cpp:2404`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `LLFloaterDirector::onActorStyleControlChanged()` — `indra/newview/llfloaterdirector.cpp:2404` | Move model-facing work into `ALActorStyleKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onActorStyleReset()` — `indra/newview/llfloaterdirector.cpp:528, 1083` | Move model-facing work into `ALActorStyleKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onActorStyleTargetChanged()` — `indra/newview/llfloaterdirector.cpp:506, 1061` | Move model-facing work into `ALActorStyleKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **71 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-actor-tallest-850` | Only `actor` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-actor-tallest-1160` | Only `actor` is put in its tallest local state at 1160 px; all other cards stay default. |
| `actor_style_target-option-0` | Only `actor_style_target` selects `Selected cast`; other selectors remain default. |
| `actor_style_target-option-1` | Only `actor_style_target` selects `Mara`; other selectors remain default. |
| `actor_style_target-option-2` | Only `actor_style_target` selects `Jun`; other selectors remain default. |
| `actor_style_target-option-3` | Only `actor_style_target` selects `Me`; other selectors remain default. |
| `card-look-tallest-850` | Only `look` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-look-tallest-1160` | Only `look` is put in its tallest local state at 1160 px; all other cards stay default. |
| `actor_style_look-option-0` | Only `actor_style_look` selects `Ghost`; other selectors remain default. |
| `actor_style_look-option-1` | Only `actor_style_look` selects `Clone`; other selectors remain default. |
| `actor_style_look-option-2` | Only `actor_style_look` selects `Hologram`; other selectors remain default. |
| `actor_style_look-option-3` | Only `actor_style_look` selects `Wireframe`; other selectors remain default. |
| `actor_style_look-option-4` | Only `actor_style_look` selects `X-ray`; other selectors remain default. |
| `actor_style_look-option-5` | Only `actor_style_look` selects `Thermal`; other selectors remain default. |
| `actor_style_look-option-6` | Only `actor_style_look` selects `Neon Outline`; other selectors remain default. |
| `actor_style_look-option-7` | Only `actor_style_look` selects `Silhouette`; other selectors remain default. |
| `actor_style_look-option-8` | Only `actor_style_look` selects `Toon/Ink`; other selectors remain default. |
| `actor_style_look-option-9` | Only `actor_style_look` selects `Chrome`; other selectors remain default. |
| `actor_style_look-option-10` | Only `actor_style_look` selects `Dissolve`; other selectors remain default. |
| `actor_style_look-option-11` | Only `actor_style_look` selects `Negative`; other selectors remain default. |
| `actor_style_look-option-12` | Only `actor_style_look` selects `Gold Statue`; other selectors remain default. |
| `actor_style_look-option-13` | Only `actor_style_look` selects `Night-vision`; other selectors remain default. |
| `actor_style_look-option-14` | Only `actor_style_look` selects `Blueprint`; other selectors remain default. |
| `actor_style_look-option-15` | Only `actor_style_look` selects `Ectoplasm`; other selectors remain default. |
| `actor_style_look-option-16` | Only `actor_style_look` selects `Frost/Ice`; other selectors remain default. |
| `actor_style_look-option-17` | Only `actor_style_look` selects `Prism`; other selectors remain default. |
| `actor_style_look-option-18` | Only `actor_style_look` selects `Thermal Scope`; other selectors remain default. |
| `actor_style_look-option-19` | Only `actor_style_look` selects `Wallhack/ESP`; other selectors remain default. |
| `actor_style_look-option-20` | Only `actor_style_look` selects `Night-vision Tube`; other selectors remain default. |
| `actor_style_look-option-21` | Only `actor_style_look` selects `Damage Overlay`; other selectors remain default. |
| `actor_style_look-option-22` | Only `actor_style_look` selects `Killcam`; other selectors remain default. |
| `actor_style_look-option-23` | Only `actor_style_look` selects `Oil Slick`; other selectors remain default. |
| `actor_style_look-option-24` | Only `actor_style_look` selects `Vaporwave`; other selectors remain default. |
| `actor_style_look-option-25` | Only `actor_style_look` selects `Halftone/Comic`; other selectors remain default. |
| `actor_style_look-option-26` | Only `actor_style_look` selects `Sonar Reveal`; other selectors remain default. |
| `actor_style_look-option-27` | Only `actor_style_look` selects `Hologram Echo`; other selectors remain default. |
| `actor_style_look-option-28` | Only `actor_style_look` selects `-- Live Actor Exclusives --`; other selectors remain default. |
| `actor_style_look-option-29` | Only `actor_style_look` selects `Rim Noir`; other selectors remain default. |
| `actor_style_look-option-30` | Only `actor_style_look` selects `Gel Split`; other selectors remain default. |
| `actor_style_look-option-31` | Only `actor_style_look` selects `Bass Sweep`; other selectors remain default. |
| `actor_style_look-option-32` | Only `actor_style_look` selects `Moonlit`; other selectors remain default. |
| `actor_style_look-option-33` | Only `actor_style_look` selects `Possessed`; other selectors remain default. |
| `actor_style_look-option-34` | Only `actor_style_look` selects `Seraph`; other selectors remain default. |
| `actor_style_look-option-35` | Only `actor_style_look` selects `Interrogation`; other selectors remain default. |
| `actor_style_look-option-36` | Only `actor_style_look` selects `Wraith`; other selectors remain default. |
| `actor_style_look-option-37` | Only `actor_style_look` selects `Tron Suit`; other selectors remain default. |
| `actor_style_mode-option-0` | Only `actor_style_mode` selects `Layer`; other selectors remain default. |
| `actor_style_mode-option-1` | Only `actor_style_mode` selects `Cover`; other selectors remain default. |
| `card-color-tallest-850` | Only `color` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-color-tallest-1160` | Only `color` is put in its tallest local state at 1160 px; all other cards stay default. |
| `actor_style_use_actor_hue-on` | Only `actor_style_use_actor_hue` is on; dependent visibility is measured. |
| `card-motion-tallest-850` | Only `motion` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-motion-tallest-1160` | Only `motion` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-distortion-tallest-850` | Only `distortion` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-distortion-tallest-1160` | Only `distortion` is put in its tallest local state at 1160 px; all other cards stay default. |
| `actor_style_distortion-option-0` | Only `actor_style_distortion` selects `None`; other selectors remain default. |
| `actor_style_distortion-option-1` | Only `actor_style_distortion` selects `Pixelate`; other selectors remain default. |
| `actor_style_distortion-option-2` | Only `actor_style_distortion` selects `Voxel`; other selectors remain default. |
| `actor_style_distortion-option-3` | Only `actor_style_distortion` selects `Lens/Magnify`; other selectors remain default. |
| `actor_style_distortion-option-4` | Only `actor_style_distortion` selects `Wave/Ripple`; other selectors remain default. |
| `actor_style_distortion-option-5` | Only `actor_style_distortion` selects `RGB Split`; other selectors remain default. |
| `actor_style_distortion-option-6` | Only `actor_style_distortion` selects `Block Glitch`; other selectors remain default. |
| `actor_style_distortion-option-7` | Only `actor_style_distortion` selects `Vertical Tear`; other selectors remain default. |
| `actor_style_distortion-option-8` | Only `actor_style_distortion` selects `VHS`; other selectors remain default. |
| `actor-none` | Rail has no actor; target-dependent rows are disabled with an explicit empty state. |
| `actor-selected` | One rail actor is selected and unpinned. |
| `actor-pinned-rail-changes` | Page target is pinned while the rail changes to another actor. |
| `actor-removed` | The active actor disappears while the page is visible. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

18 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. An unresolved binding is not permission to invent a visual class.

## Unresolved and pending-decision items

None in the current evidence index.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
