# Settings — Director Kit page contract

Page key: `director_settings_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (19/26 setting-bound rows; 7 C++-referenced rows).

## Real owners

- `director_settings_tab_logic` / `panel` at `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:3` (25 matching rows).
- Referencing classes: `LLFloaterDirector` (6), `LLNotifications` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALDirectorSettingsKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `scene_recovery` / `settings_recovery_mode` (Unsaved scene) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:8` | kit `combo` ↔ `DirectorRecoveryMode` `S32` | none |
| `scene_recovery` / `settings_crash_backups` (Crash backups) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:17` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `scene_recovery` / `settings_restore_backup` (Restore backup) | `button` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:23` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `scene_recovery` / `settings_delete_backup` (Delete backup) | `button` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:26` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `scene_recovery` / `settings_recovery_help` (About recovery) | `text` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:29` | kit readout ← approved static copy; no runtime writer | none |
| `settings_cast` / `settings_add_self_link` (Add me at login) | none | kit-native action `link_to:capture_group/scene_options_tab`; no duplicate widget is required | none |
| `settings_prompts` / `settings_reset_prompts` (Reset prompts) | `button` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:37` | `ALDirectorSettingsKitAdapter` command/model binding → wired in `LLFloaterDirector::bindDirectorSettingsPage()`; behavior `LLNotifications::setIgnored()` (`indra/newview/llfloaterdirector.cpp:2345`) | none |
| `settings_appearance` / `settings_theme` (Theme) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:42` | kit `combo` ↔ `DirectorTheme` `String` | none |
| `settings_appearance` / `settings_theme_invert` (Invert theme) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:62` | kit `toggle` ↔ `DirectorThemeInvert` `Boolean` | none |
| `settings_appearance` / `settings_background_opacity` (Background opacity) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:65` | kit `slider` ↔ `DirectorBackgroundOpacity` `F32` | none |
| `settings_appearance` / `settings_tag_highlight` (Highlight edited name tags) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:70` | kit `toggle` ↔ `DirectorTagHighlight` `Boolean` | none |
| `settings_appearance` / `hide_cursor_with_ui` (Hide cursor with interface) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:73` | kit `toggle` ↔ `HideCursorWithUI` `Boolean` | none |
| `settings_appearance` / `settings_turn_to_face_selected` (Turn to face selected objects) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:76` | kit `toggle` ↔ `TurnToFaceSelectedObjects` `Boolean` | none |
| `settings_appearance` / `settings_action_all_off` (Replace ACTION with All Off) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:79` | kit `toggle` ↔ `DirectorActionAllOffMode` `Boolean` | none |
| `settings_appearance` / `settings_restore_last_reset` (Restore last reset) | `button` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:82` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `settings_performance` / `settings_rigged_lod_hysteresis` (LOD hysteresis) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:87` | kit `slider` ↔ `RenderRiggedLODHysteresis` `F32` | none |
| `settings_performance` / `settings_shadow_direct_transitions_check` (Shadow passes) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:92` | kit `toggle` ↔ `RenderShadowDirectTransitions` `Boolean` | none |
| `settings_performance` / `settings_vbo_pool_clean_check` (VBO pool cleanup) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:95` | kit `toggle` ↔ `RenderVBOPoolCleanAtFrameEnd` `Boolean` | none |
| `settings_performance` / `settings_emergency_purge_sliced` (Spread texture purges) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:98` | kit `toggle` ↔ `AlchemyEmergencyPurgeSliced` `Boolean` | none |
| `settings_performance` / `settings_capture_quality_pin` (Pin scene textures (no degrade during takes)) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:101` | kit `toggle` ↔ `BDMergeCaptureModePin` `Boolean` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4704` `capture_quality_pin` |
| `settings_performance` / `settings_perf_geombudgetcap` (Cap geometry budget) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:104` | kit `toggle` ↔ `AlchemyPerfGeomBudgetCap` `Boolean` | none |
| `settings_performance` / `settings_perf_batchedrowuploads` (Batch texture rows) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:107` | kit `toggle` ↔ `AlchemyPerfBatchedRowUploads` `Boolean` | none |
| `settings_performance` / `settings_perf_cachedmatrixinverse` (Cache matrix inverses) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:110` | kit `toggle` ↔ `AlchemyPerfCachedMatrixInverse` `Boolean` | none |
| `settings_performance` / `settings_perf_coalescedbodyvisibility` (Coalesce body visibility) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:113` | kit `toggle` ↔ `AlchemyPerfCoalescedBodyVisibility` `Boolean` | none |
| `settings_group_chat` / `settings_group_chat_snooze_minutes` (Snooze minutes) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:118` | kit `number` ↔ `GroupChatSnoozeMinutes` `S32` | none |
| `settings_group_chat` / `settings_group_chat_snooze_ask` (Ask snooze duration) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml:121` | kit `toggle` ↔ `GroupChatSnoozeAsk` `Boolean` | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `LLNotifications::setIgnored()` — `indra/newview/llfloaterdirector.cpp:2345` | Move model-facing work into `ALDirectorSettingsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **32 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-scene_recovery-tallest-850` | Only `scene_recovery` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-scene_recovery-tallest-1160` | Only `scene_recovery` is put in its tallest local state at 1160 px; all other cards stay default. |
| `settings_recovery_mode-option-0` | Only `settings_recovery_mode` selects `Ask me`; other selectors remain default. |
| `settings_recovery_mode-option-1` | Only `settings_recovery_mode` selects `Restore automatically`; other selectors remain default. |
| `settings_recovery_mode-option-2` | Only `settings_recovery_mode` selects `Don't restore (keep the backup)`; other selectors remain default. |
| `card-settings_cast-tallest-850` | Only `settings_cast` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-settings_cast-tallest-1160` | Only `settings_cast` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-settings_prompts-tallest-850` | Only `settings_prompts` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-settings_prompts-tallest-1160` | Only `settings_prompts` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-settings_appearance-tallest-850` | Only `settings_appearance` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-settings_appearance-tallest-1160` | Only `settings_appearance` is put in its tallest local state at 1160 px; all other cards stay default. |
| `settings_theme_invert-on` | Only `settings_theme_invert` is on; dependent visibility is measured. |
| `settings_tag_highlight-on` | Only `settings_tag_highlight` is on; dependent visibility is measured. |
| `hide_cursor_with_ui-on` | Only `hide_cursor_with_ui` is on; dependent visibility is measured. |
| `settings_turn_to_face_selected-on` | Only `settings_turn_to_face_selected` is on; dependent visibility is measured. |
| `settings_action_all_off-on` | Only `settings_action_all_off` is on; dependent visibility is measured. |
| `card-settings_performance-tallest-850` | Only `settings_performance` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-settings_performance-tallest-1160` | Only `settings_performance` is put in its tallest local state at 1160 px; all other cards stay default. |
| `settings_shadow_direct_transitions_check-on` | Only `settings_shadow_direct_transitions_check` is on; dependent visibility is measured. |
| `settings_vbo_pool_clean_check-on` | Only `settings_vbo_pool_clean_check` is on; dependent visibility is measured. |
| `settings_emergency_purge_sliced-on` | Only `settings_emergency_purge_sliced` is on; dependent visibility is measured. |
| `settings_capture_quality_pin-on` | Only `settings_capture_quality_pin` is on; dependent visibility is measured. |
| `settings_perf_geombudgetcap-on` | Only `settings_perf_geombudgetcap` is on; dependent visibility is measured. |
| `settings_perf_batchedrowuploads-on` | Only `settings_perf_batchedrowuploads` is on; dependent visibility is measured. |
| `settings_perf_cachedmatrixinverse-on` | Only `settings_perf_cachedmatrixinverse` is on; dependent visibility is measured. |
| `settings_perf_coalescedbodyvisibility-on` | Only `settings_perf_coalescedbodyvisibility` is on; dependent visibility is measured. |
| `card-settings_group_chat-tallest-850` | Only `settings_group_chat` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-settings_group_chat-tallest-1160` | Only `settings_group_chat` is put in its tallest local state at 1160 px; all other cards stay default. |
| `settings_group_chat_snooze_ask-on` | Only `settings_group_chat_snooze_ask` is on; dependent visibility is measured. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

None.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. An unresolved binding is not permission to invent a visual class.

## Unresolved and pending-decision items

- `settings_crash_backups` (Crash backups): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `settings_restore_backup` (Restore backup): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `settings_delete_backup` (Delete backup): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `settings_restore_last_reset` (Restore last reset): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
