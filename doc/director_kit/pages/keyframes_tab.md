# Keyframes — Director Kit page contract

Page key: `keyframes_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (1/57 setting-bound rows; 57 C++-referenced rows).

## Real owners

- `keyframes_tab_logic` / `panel` at `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:3` (57 matching rows).
- Referencing classes: `ALPanelKeyframes` (279), `std` (1), `ALKeyframeTimeline` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALPanelKeyframes` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `kf_track` / `kf_track` (Track) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:15` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::refreshLibrary()` (`indra/newview/alpanelkeyframes.cpp:295`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:384`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:386`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:417`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:530`) | none |
| `kf_track` / `kf_name` (Name) | `line_editor` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:22` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:400`), wired in `ALPanelKeyframes::refresh()`; behavior `std::string()` (`indra/newview/alpanelkeyframes.cpp:528`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:565`) | none |
| `kf_track` / `kf_new` (New) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:25` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:382`) | none |
| `kf_track` / `kf_save` (Save) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:28` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:396`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:405`) | none |
| `kf_track` / `kf_save_as` (Save as...) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:31` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:396`) | none |
| `kf_track` / `kf_load` (Load) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:34` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:384`) | none |
| `kf_track` / `kf_rename` (Rename) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:37` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:396`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:408`) | none |
| `kf_track` / `kf_delete_track` (Delete track) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:40` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:415`) | none |
| `kf_track` / `kf_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:43` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:543`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:561`) | none |
| `kf_timeline` / `kf_timeline` (Timeline) | `keyframe_timeline` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:48` | `ALPanelKeyframes` command/model binding → wired in `ALPanelKeyframes::postBuild()`; behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:99`), wired in `ALKeyframeTimeline::postBuild()`; behavior `ALKeyframeTimeline::handleMouseDown()` (`indra/newview/alkeyframetimeline.cpp:98`) | none |
| `kf_timeline` / `kf_add` (Add keyframe) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:51` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:347`) | none |
| `kf_timeline` / `kf_insert` (Insert at playhead) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:54` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:347`) | none |
| `kf_timeline` / `kf_delete` (Delete keyframe) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:57` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:346`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:545`) | none |
| `kf_timeline` / `kf_visit` (Visit) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:60` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:342`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:540`) | none |
| `kf_timeline` / `kf_release` (Release) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:63` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:341`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:539`) | none |
| `kf_timeline` / `kf_playhead` (Playhead (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:66` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:343`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:531`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:540`) | none |
| `kf_timeline` / `kf_undo` (Undo) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:69` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:344`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:542`) | none |
| `kf_timeline` / `kf_redo` (Redo) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:72` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:345`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:542`) | none |
| `kf_play` / `kf_playing` (Playing) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:77` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:340`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:529`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:538`) | none |
| `kf_play` / `kf_loop` (Loop) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:80` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:440`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:448`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:565`) | none |
| `kf_play` / `kf_delay` (Start delay (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:83` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:440`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:449`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:565`) | none |
| `kf_play` / `kf_arm` (ACTION plays keyframes) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:86` | kit `toggle` ↔ `DirectorArmKeyframePlay` `Boolean`; `ALPanelKeyframes` preserves/calls behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:439`) | none |
| `kf_pace` / `kf_path` (Path) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:91` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:440`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:450`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:568`) | none |
| `kf_pace` / `kf_even` (Even speed) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:99` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:440`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:451`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:548`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:568`) | none |
| `kf_pace` / `kf_total` (Total length (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:102` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:440`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:461`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:548`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:569`) | none |
| `kf_pace` / `kf_ease_ends` (Ease at start & end) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:105` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:441`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:462`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:569`) | none |
| `kf_pace` / `kf_ease_length` (Ease length (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:108` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:441`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:463`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:569`) | none |
| `kf_key` / `kf_time` (Time (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:113` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:348`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:544`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:580`) | none |
| `kf_key` / `kf_pace_mode` (Move to next) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:116` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:441`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:476`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:544`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:547`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:580`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:586`) | none |
| `kf_key` / `kf_pace` (Duration / cruise) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:124` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:441`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:477`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:544`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:547`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:580`) | none |
| `kf_key` / `kf_segment` (Segment) | `text` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:127` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:585`) | none |
| `kf_key` / `kf_hold` (Hold here (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:130` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:441`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:478`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:544`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:581`) | none |
| `kf_key` / `kf_ease` (Ease here) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:133` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:442`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:479`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:545`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:581`) | none |
| `kf_key` / `kf_zoom` (Zoom (mm)) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:136` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:442`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:480`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:545`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:581`) | none |
| `kf_key` / `kf_roll` (Roll (degrees)) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:139` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:442`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:481`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:545`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:581`) | none |
| `kf_key` / `kf_handles` (Handles) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:142` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:442`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:487`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:545`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:582`) | none |
| `kf_key` / `kf_update` (Update from current view) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:151` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:442`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:504`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:545`) | none |
| `kf_key` / `kf_look` (Look at) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:154` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:443`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:445`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:503`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:541`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:582`) | none |
| `kf_key` / `kf_own` (Own target) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:163` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::refreshActors()` (`indra/newview/alpanelkeyframes.cpp:318`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:376`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:378`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:379`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:541`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:583`) | none |
| `kf_key` / `kf_own_pick` (Pick own target in world) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:170` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:365`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:371`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:541`) | none |
| `kf_key` / `kf_own_joint` (Own target joint) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:173` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:443`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:445`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:464`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:541`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:583`) | none |
| `kf_lock` / `kf_lock` (Lock on) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:187` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::refreshActors()` (`indra/newview/alpanelkeyframes.cpp:318`), behavior `ALPanelKeyframes::refreshActors()` (`indra/newview/alpanelkeyframes.cpp:325`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:376`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:541`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:570`) | none |
| `kf_lock` / `kf_lock_pick` (Pick Lock in world) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:194` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:365`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:541`) | none |
| `kf_lock` / `kf_joint` (Joint) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:197` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:443`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:445`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:464`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:466`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:541`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:577`) | none |
| `kf_lock` / `kf_smoothing` (Smoothing) | `text` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:209` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:562`) | none |
| `kf_lock` / `kf_lock_status` (Lock status) | `text` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:212` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:543`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:561`) | none |
| `kf_world` / `kf_show` (Show path) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:217` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:353`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:355`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:531`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:543`) | none |
| `kf_world` / `kf_dots` (Second dots) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:220` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:353`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:356`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:531`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:543`) | none |
| `kf_world` / `kf_icons` (Camera icons) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:223` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:353`), behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:357`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:531`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:543`) | none |
| `kf_world` / `kf_edit` (Edit in world) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:226` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:360`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:532`) | none |
| `kf_fit` / `kf_source` (Source) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:231` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:423`) | none |
| `kf_fit` / `kf_tolerance` (Tolerance) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:238` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:425`) | none |
| `kf_fit` / `kf_fit_cm` (Position (cm)) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:248` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:428`) | none |
| `kf_fit` / `kf_fit_rotation` (Rotation (degrees)) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:251` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:429`) | none |
| `kf_fit` / `kf_fit_fov` (FOV (degrees)) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:254` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:429`) | none |
| `kf_fit` / `kf_make` (Make keyframes from take) | `button` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:257` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::action()` (`indra/newview/alpanelkeyframes.cpp:421`) | none |
| `kf_fit` / `kf_result` (Result) | `text` `indra/newview/skins/default/xui/en/panel_dpage_camera_keyframes.xml:260` | `ALPanelKeyframes` command/model binding → behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:543`), behavior `ALPanelKeyframes::refresh()` (`indra/newview/alpanelkeyframes.cpp:562`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALKeyframeTimeline::handleMouseDown()` — `indra/newview/alkeyframetimeline.cpp:98` | Move model-facing work into `ALPanelKeyframes` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelKeyframes::action()` — `indra/newview/alpanelkeyframes.cpp:340, 341, 342, 343, 344, 345, 346, 347, 348, 353, 355, 356, 357, 360, 365, 371, 376, 378, 379, 382, 384, 386, 396, 400, 405, 408, 415, 417, 421, 423, 425, 428, 429, 439, 440, 441, 442, 443, 445, 448, 449, 450, 451, 461, 462, 463, 464, 466, 476, 477, 478, 479, 480, 481, 487, 503, 504` | Move model-facing work into `ALPanelKeyframes` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelKeyframes::refresh()` — `indra/newview/alpanelkeyframes.cpp:99, 529, 530, 531, 532, 538, 539, 540, 541, 542, 543, 544, 545, 547, 548, 561, 562, 565, 568, 569, 570, 577, 580, 581, 582, 583, 585, 586` | Move model-facing work into `ALPanelKeyframes` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelKeyframes::refreshActors()` — `indra/newview/alpanelkeyframes.cpp:318, 325` | Move model-facing work into `ALPanelKeyframes` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelKeyframes::refreshLibrary()` — `indra/newview/alpanelkeyframes.cpp:295` | Move model-facing work into `ALPanelKeyframes` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `std::string()` — `indra/newview/alpanelkeyframes.cpp:528` | Move model-facing work into `ALPanelKeyframes` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **59 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-kf_track-tallest-850` | Only `kf_track` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-kf_track-tallest-1160` | Only `kf_track` is put in its tallest local state at 1160 px; all other cards stay default. |
| `kf_track-option-0` | Only `kf_track` selects `Session`; other selectors remain default. |
| `card-kf_timeline-tallest-850` | Only `kf_timeline` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-kf_timeline-tallest-1160` | Only `kf_timeline` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-kf_play-tallest-850` | Only `kf_play` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-kf_play-tallest-1160` | Only `kf_play` is put in its tallest local state at 1160 px; all other cards stay default. |
| `kf_playing-on` | Only `kf_playing` is on; dependent visibility is measured. |
| `kf_loop-on` | Only `kf_loop` is on; dependent visibility is measured. |
| `kf_arm-on` | Only `kf_arm` is on; dependent visibility is measured. |
| `card-kf_pace-tallest-850` | Only `kf_pace` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-kf_pace-tallest-1160` | Only `kf_pace` is put in its tallest local state at 1160 px; all other cards stay default. |
| `kf_path-option-0` | Only `kf_path` selects `Smooth`; other selectors remain default. |
| `kf_path-option-1` | Only `kf_path` selects `Straight`; other selectors remain default. |
| `kf_even-on` | Only `kf_even` is on; dependent visibility is measured. |
| `kf_ease_ends-on` | Only `kf_ease_ends` is on; dependent visibility is measured. |
| `card-kf_key-tallest-850` | Only `kf_key` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-kf_key-tallest-1160` | Only `kf_key` is put in its tallest local state at 1160 px; all other cards stay default. |
| `kf_pace_mode-option-0` | Only `kf_pace_mode` selects `Duration (s)`; other selectors remain default. |
| `kf_pace_mode-option-1` | Only `kf_pace_mode` selects `Cruise speed (m/s)`; other selectors remain default. |
| `kf_ease-on` | Only `kf_ease` is on; dependent visibility is measured. |
| `kf_handles-option-0` | Only `kf_handles` selects `Auto`; other selectors remain default. |
| `kf_handles-option-1` | Only `kf_handles` selects `Aligned`; other selectors remain default. |
| `kf_handles-option-2` | Only `kf_handles` selects `Free`; other selectors remain default. |
| `kf_look-option-0` | Only `kf_look` selects `Follow Lock`; other selectors remain default. |
| `kf_look-option-1` | Only `kf_look` selects `Own target`; other selectors remain default. |
| `kf_look-option-2` | Only `kf_look` selects `None`; other selectors remain default. |
| `kf_own-option-0` | Only `kf_own` selects `None`; other selectors remain default. |
| `kf_own_joint-option-0` | Only `kf_own_joint` selects `Root`; other selectors remain default. |
| `kf_own_joint-option-1` | Only `kf_own_joint` selects `Head`; other selectors remain default. |
| `kf_own_joint-option-2` | Only `kf_own_joint` selects `Chest`; other selectors remain default. |
| `kf_own_joint-option-3` | Only `kf_own_joint` selects `Pelvis`; other selectors remain default. |
| `kf_own_joint-option-4` | Only `kf_own_joint` selects `Left hand`; other selectors remain default. |
| `kf_own_joint-option-5` | Only `kf_own_joint` selects `Right hand`; other selectors remain default. |
| `card-kf_lock-tallest-850` | Only `kf_lock` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-kf_lock-tallest-1160` | Only `kf_lock` is put in its tallest local state at 1160 px; all other cards stay default. |
| `kf_lock-option-0` | Only `kf_lock` selects `None`; other selectors remain default. |
| `kf_joint-option-0` | Only `kf_joint` selects `Root`; other selectors remain default. |
| `kf_joint-option-1` | Only `kf_joint` selects `Head`; other selectors remain default. |
| `kf_joint-option-2` | Only `kf_joint` selects `Chest`; other selectors remain default. |
| `kf_joint-option-3` | Only `kf_joint` selects `Pelvis`; other selectors remain default. |
| `kf_joint-option-4` | Only `kf_joint` selects `Left hand`; other selectors remain default. |
| `kf_joint-option-5` | Only `kf_joint` selects `Right hand`; other selectors remain default. |
| `card-kf_world-tallest-850` | Only `kf_world` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-kf_world-tallest-1160` | Only `kf_world` is put in its tallest local state at 1160 px; all other cards stay default. |
| `kf_show-on` | Only `kf_show` is on; dependent visibility is measured. |
| `kf_dots-on` | Only `kf_dots` is on; dependent visibility is measured. |
| `kf_icons-on` | Only `kf_icons` is on; dependent visibility is measured. |
| `kf_edit-on` | Only `kf_edit` is on; dependent visibility is measured. |
| `card-kf_fit-tallest-850` | Only `kf_fit` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-kf_fit-tallest-1160` | Only `kf_fit` is put in its tallest local state at 1160 px; all other cards stay default. |
| `kf_source-option-0` | Only `kf_source` selects `Current recorded take`; other selectors remain default. |
| `kf_tolerance-option-0` | Only `kf_tolerance` selects `Fine`; other selectors remain default. |
| `kf_tolerance-option-1` | Only `kf_tolerance` selects `Medium`; other selectors remain default. |
| `kf_tolerance-option-2` | Only `kf_tolerance` selects `Coarse`; other selectors remain default. |
| `kf_tolerance-option-3` | Only `kf_tolerance` selects `Advanced`; other selectors remain default. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

5 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
