# Frame & Orbit — Director Kit page contract

Page key: `frame_orbit_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (29/31 setting-bound rows; 14 C++-referenced rows).

## Real owners

- `panel_cinecam_frame` / `panel` at `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:8` (21 matching rows).
- `panel_flycam_orbit` / `panel` at `indra/newview/skins/default/xui/en/panel_flycam_orbit.xml:13` (10 matching rows).
- `panel_cinecam_params` / `panel` at `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:10` (2 matching rows).
- Referencing classes: `LLViewerJoystick` (7), `LLCinematicCamera` (2), `LLFloaterDirector` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALFrameOrbitKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `frame_format` / `frame_aspect_ratio` (Format) | `combo_box` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:23` | kit `combo` ↔ `CinematicFrameAspectRatio` `F32` | none |
| `frame_format` / `frame_custom_ratio` (Custom W/H) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:46` | kit `number` ↔ `CinematicFrameCustomRatio` `F32` | none |
| `frame_format` / `frame_guide_enabled` (Show guide) | `check_box` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:291` | kit `toggle` ↔ `CinematicFrameGuideEnabled` `Boolean` | none |
| `frame_format` / `frame_guide_style` (Guide style) | `combo_box` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:303` | kit `combo` ↔ `CinematicFrameGuideStyle` `S32` | none |
| `frame_format` / `frame_guide_opacity` (Bar opacity) | `slider` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:318` | kit `slider` ↔ `CinematicFrameGuideOpacity` `F32` | none |
| `frame_format` / `frame_lens_enabled` (Lens reframe) | `check_box` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:58` | kit `toggle` ↔ `CinematicFrameLensEnabled` `Boolean` | none |
| `frame_format` / `frame_focal_length` (Base lens) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:69` | kit `number` ↔ `CinematicFrameFocalLengthMM` `F32` | none |
| `auto_reframe` / `auto_frame_fill` (Skeleton fill) | `slider` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:99` | kit `slider` ↔ `CinematicAutoFrameFill` `F32` | none |
| `auto_reframe` / `auto_frame_compose` (Top line) | `slider` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:112` | kit `slider` ↔ `CinematicAutoFrameComposeLine` `F32` | none |
| `auto_reframe` / `auto_frame_distance_trim` (Dolly trim) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:124` | kit `number` ↔ `CinematicAutoFrameDistanceTrim` `F32` | none |
| `auto_reframe` / `auto_frame_vertical_trim` (Vertical trim) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:136` | kit `number` ↔ `CinematicCamFrameOffsetUp` `F32` | page `operator_tab`, `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:201` `CinematicCamFrameOffsetUp` |
| `auto_reframe` / `auto_frame_resolve` (Re-solve now) | `button` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:147` | **USER DECISION:** register an explicit `ALFrameOrbitKitAdapter` command; no matching setting or C++ literal was found | none |
| `reframe_settle` / `auto_frame_settle_mode` (Settle mode) | `combo_box` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:155` | kit `combo` ↔ `CinematicAutoFrameSettleMode` `S32` | none |
| `reframe_settle` / `auto_frame_settle_seconds` (Settle time) | `slider` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:171` | kit `slider` ↔ `CinematicAutoFrameSettleSeconds` `F32`; `ALFrameOrbitKitAdapter` preserves/calls `?::?()` (`indra/newview/llcinematiccamera.cpp:92`) | none |
| `reframe_settle` / `auto_frame_settle_curve` (Curve) | `combo_box` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:197` | kit `combo` ↔ `CinematicAutoFrameSettleCurve` `S32` | none |
| `reframe_settle` / `auto_frame_settle_max_ratio` (Max ratio) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:183` | kit `number` ↔ `CinematicAutoFrameSettleMaxRatio` `F32` | none |
| `reframe_settle` / `SettleFeather` (Feather) | `slider` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:277` | kit `slider` ↔ `CinematicAutoFrameSettleFeather` `F32` | none |
| `reframe_settle` / `auto_frame_settle_bezier_x1` (X1) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:244` | kit `number` ↔ `CinematicAutoFrameSettleBezierX1` `F32`; `ALFrameOrbitKitAdapter` preserves/calls `?::?()` (`indra/newview/llcinematiccamera.cpp:100`) | none |
| `reframe_settle` / `auto_frame_settle_bezier_y1` (Y1) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:252` | kit `number` ↔ `CinematicAutoFrameSettleBezierY1` `F32`; `ALFrameOrbitKitAdapter` preserves/calls `?::?()` (`indra/newview/llcinematiccamera.cpp:101`) | none |
| `reframe_settle` / `auto_frame_settle_bezier_x2` (X2) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:260` | kit `number` ↔ `CinematicAutoFrameSettleBezierX2` `F32`; `ALFrameOrbitKitAdapter` preserves/calls `?::?()` (`indra/newview/llcinematiccamera.cpp:102`) | none |
| `reframe_settle` / `auto_frame_settle_bezier_y2` (Y2) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:268` | kit `number` ↔ `CinematicAutoFrameSettleBezierY2` `F32`; `ALFrameOrbitKitAdapter` preserves/calls `?::?()` (`indra/newview/llcinematiccamera.cpp:103`) | none |
| `flycam_orbit` / `CinematicCamJoint` (Anchor joint) | `combo_box` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:343`, `combo_box` `indra/newview/skins/default/xui/en/panel_flycam_orbit.xml:48` | kit `combo` ↔ `CinematicCamJoint` `String`; `ALFrameOrbitKitAdapter` preserves/calls `?::?()` (`indra/newview/alpanelcinecamparams.cpp:132`), `LLCinematicCamera::resolveAnchor()` (`indra/newview/llcinematiccamera.cpp:756`), `LLCinematicCamera::patternBoneLock()` (`indra/newview/llcinematiccamera.cpp:786`) | none |
| `flycam_orbit` / `CinematicCamUseSelected` (Use selected) | `check_box` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:99`, `check_box` `indra/newview/skins/default/xui/en/panel_flycam_orbit.xml:75` | kit `toggle` ↔ `CinematicCamUseSelected` `Boolean`; `ALFrameOrbitKitAdapter` preserves/calls `?::?()` (`indra/newview/alpanelcinecamparams.cpp:283`), `?::?()` (`indra/newview/llcinematiccamera.cpp:698`), `LLFloaterDirector::refreshStatusStrip()` (`indra/newview/llfloaterdirector.cpp:3567`) | page `shot_tab` |
| `flycam_orbit` / `FlycamOrbitLevel` (Level horizon) | `check_box` `indra/newview/skins/default/xui/en/panel_flycam_orbit.xml:87` | kit `toggle` ↔ `FlycamOrbitLevel` `Boolean`; `ALFrameOrbitKitAdapter` preserves/calls `?::?()` (`indra/newview/llfloaterdirector.cpp:1102`), `LLViewerJoystick::moveFlycam()` (`indra/newview/llviewerjoystick.cpp:2276`) | none |
| `flycam_orbit` / `FlycamOrbitZoom` (Orbit zoom) | `slider` `indra/newview/skins/default/xui/en/panel_flycam_orbit.xml:157` | kit `slider` ↔ `FlycamOrbitZoom` `F32`; `ALFrameOrbitKitAdapter` preserves/calls `?::?()` (`indra/newview/llfloaterdirector.cpp:1107`), `LLViewerJoystick::moveFlycam()` (`indra/newview/llviewerjoystick.cpp:2280`) | none |
| `flycam_orbit` / `FlycamOrbitMaxRadius` (Max radius) | `spinner` `indra/newview/skins/default/xui/en/panel_flycam_orbit.xml:121` | kit `number` ↔ `FlycamOrbitMaxRadius` `F32`; `ALFrameOrbitKitAdapter` preserves/calls `?::?()` (`indra/newview/llfloaterdirector.cpp:1104`), `LLViewerJoystick::moveFlycam()` (`indra/newview/llviewerjoystick.cpp:2278`) | none |
| `flycam_orbit` / `FlycamOrbitSmoothing` (Smoothing) | `slider` `indra/newview/skins/default/xui/en/panel_flycam_orbit.xml:139` | kit `slider` ↔ `FlycamOrbitSmoothing` `F32`; `ALFrameOrbitKitAdapter` preserves/calls `?::?()` (`indra/newview/llfloaterdirector.cpp:1108`), `LLViewerJoystick::moveFlycam()` (`indra/newview/llviewerjoystick.cpp:2279`) | none |
| `flycam_orbit` / `FlycamOrbitOffsetUp` (Focus up) | `spinner` `indra/newview/skins/default/xui/en/panel_flycam_orbit.xml:174` | kit `number` ↔ `FlycamOrbitOffsetUp` `F32`; `ALFrameOrbitKitAdapter` preserves/calls `?::?()` (`indra/newview/llfloaterdirector.cpp:1105`), `LLViewerJoystick::moveFlycam()` (`indra/newview/llviewerjoystick.cpp:2281`) | none |
| `flycam_orbit` / `FlycamOrbitMinRadius` (Min radius) | `spinner` `indra/newview/skins/default/xui/en/panel_flycam_orbit.xml:104` | kit `number` ↔ `FlycamOrbitMinRadius` `F32`; `ALFrameOrbitKitAdapter` preserves/calls `?::?()` (`indra/newview/llfloaterdirector.cpp:1103`), `LLViewerJoystick::moveFlycam()` (`indra/newview/llviewerjoystick.cpp:2277`) | none |
| `flycam_orbit` / `FlycamOrbitOffsetLeft` (Focus left) | `spinner` `indra/newview/skins/default/xui/en/panel_flycam_orbit.xml:191` | kit `number` ↔ `FlycamOrbitOffsetLeft` `F32`; `ALFrameOrbitKitAdapter` preserves/calls `?::?()` (`indra/newview/llfloaterdirector.cpp:1106`), `LLViewerJoystick::moveFlycam()` (`indra/newview/llviewerjoystick.cpp:2282`) | none |
| `flycam_orbit` / `anchor_hint` (Anchor) | `text` `indra/newview/skins/default/xui/en/panel_flycam_orbit.xml:37` | kit readout ← approved static copy; no runtime writer | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `?::?()` — `indra/newview/alpanelcinecamparams.cpp:132, 283` | Move model-facing work into `ALFrameOrbitKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `?::?()` — `indra/newview/llcinematiccamera.cpp:92, 100, 101, 102, 103, 698` | Move model-facing work into `ALFrameOrbitKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLCinematicCamera::patternBoneLock()` — `indra/newview/llcinematiccamera.cpp:786` | Move model-facing work into `ALFrameOrbitKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLCinematicCamera::resolveAnchor()` — `indra/newview/llcinematiccamera.cpp:756` | Move model-facing work into `ALFrameOrbitKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `?::?()` — `indra/newview/llfloaterdirector.cpp:1102, 1103, 1104, 1105, 1106, 1107, 1108` | Move model-facing work into `ALFrameOrbitKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::refreshStatusStrip()` — `indra/newview/llfloaterdirector.cpp:3567` | Move model-facing work into `ALFrameOrbitKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLViewerJoystick::moveFlycam()` — `indra/newview/llviewerjoystick.cpp:2276, 2277, 2278, 2279, 2280, 2281, 2282` | Move model-facing work into `ALFrameOrbitKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **79 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-frame_format-tallest-850` | Only `frame_format` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-frame_format-tallest-1160` | Only `frame_format` is put in its tallest local state at 1160 px; all other cards stay default. |
| `frame_aspect_ratio-option-0` | Only `frame_aspect_ratio` selects `Off / Native`; other selectors remain default. |
| `frame_aspect_ratio-option-1` | Only `frame_aspect_ratio` selects `16:9 (1.78)`; other selectors remain default. |
| `frame_aspect_ratio-option-2` | Only `frame_aspect_ratio` selects `Flat (1.85:1)`; other selectors remain default. |
| `frame_aspect_ratio-option-3` | Only `frame_aspect_ratio` selects `Univisium (2.00:1)`; other selectors remain default. |
| `frame_aspect_ratio-option-4` | Only `frame_aspect_ratio` selects `CinemaScope (2.35:1)`; other selectors remain default. |
| `frame_aspect_ratio-option-5` | Only `frame_aspect_ratio` selects `Scope (2.39:1)`; other selectors remain default. |
| `frame_aspect_ratio-option-6` | Only `frame_aspect_ratio` selects `4:3 (1.33)`; other selectors remain default. |
| `frame_aspect_ratio-option-7` | Only `frame_aspect_ratio` selects `Square (1:1)`; other selectors remain default. |
| `frame_aspect_ratio-option-8` | Only `frame_aspect_ratio` selects `Vertical (9:16)`; other selectors remain default. |
| `frame_aspect_ratio-option-9` | Only `frame_aspect_ratio` selects `Custom`; other selectors remain default. |
| `frame_guide_enabled-on` | Only `frame_guide_enabled` is on; dependent visibility is measured. |
| `frame_guide_style-option-0` | Only `frame_guide_style` selects `Dimmed bars`; other selectors remain default. |
| `frame_guide_style-option-1` | Only `frame_guide_style` selects `Solid black bars`; other selectors remain default. |
| `frame_guide_style-option-2` | Only `frame_guide_style` selects `Thin guide lines`; other selectors remain default. |
| `frame_lens_enabled-on` | Only `frame_lens_enabled` is on; dependent visibility is measured. |
| `card-auto_reframe-tallest-850` | Only `auto_reframe` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-auto_reframe-tallest-1160` | Only `auto_reframe` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-reframe_settle-tallest-850` | Only `reframe_settle` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-reframe_settle-tallest-1160` | Only `reframe_settle` is put in its tallest local state at 1160 px; all other cards stay default. |
| `auto_frame_settle_mode-option-0` | Only `auto_frame_settle_mode` selects `Off`; other selectors remain default. |
| `auto_frame_settle_mode-option-1` | Only `auto_frame_settle_mode` selects `Finish with cut`; other selectors remain default. |
| `auto_frame_settle_mode-option-2` | Only `auto_frame_settle_mode` selects `Post-cut settle`; other selectors remain default. |
| `auto_frame_settle_curve-option-0` | Only `auto_frame_settle_curve` selects `Smootherstep`; other selectors remain default. |
| `auto_frame_settle_curve-option-1` | Only `auto_frame_settle_curve` selects `Linear`; other selectors remain default. |
| `auto_frame_settle_curve-option-2` | Only `auto_frame_settle_curve` selects `Ease (CSS)`; other selectors remain default. |
| `auto_frame_settle_curve-option-3` | Only `auto_frame_settle_curve` selects `Ease In (CSS)`; other selectors remain default. |
| `auto_frame_settle_curve-option-4` | Only `auto_frame_settle_curve` selects `Ease Out (CSS)`; other selectors remain default. |
| `auto_frame_settle_curve-option-5` | Only `auto_frame_settle_curve` selects `Ease In-Out (CSS)`; other selectors remain default. |
| `auto_frame_settle_curve-option-6` | Only `auto_frame_settle_curve` selects `Quad In`; other selectors remain default. |
| `auto_frame_settle_curve-option-7` | Only `auto_frame_settle_curve` selects `Quad Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-8` | Only `auto_frame_settle_curve` selects `Quad In-Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-9` | Only `auto_frame_settle_curve` selects `Cubic In`; other selectors remain default. |
| `auto_frame_settle_curve-option-10` | Only `auto_frame_settle_curve` selects `Cubic Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-11` | Only `auto_frame_settle_curve` selects `Cubic In-Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-12` | Only `auto_frame_settle_curve` selects `Quart In`; other selectors remain default. |
| `auto_frame_settle_curve-option-13` | Only `auto_frame_settle_curve` selects `Quart Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-14` | Only `auto_frame_settle_curve` selects `Quart In-Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-15` | Only `auto_frame_settle_curve` selects `Quint In`; other selectors remain default. |
| `auto_frame_settle_curve-option-16` | Only `auto_frame_settle_curve` selects `Quint Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-17` | Only `auto_frame_settle_curve` selects `Quint In-Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-18` | Only `auto_frame_settle_curve` selects `Expo In`; other selectors remain default. |
| `auto_frame_settle_curve-option-19` | Only `auto_frame_settle_curve` selects `Expo Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-20` | Only `auto_frame_settle_curve` selects `Expo In-Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-21` | Only `auto_frame_settle_curve` selects `Circ In`; other selectors remain default. |
| `auto_frame_settle_curve-option-22` | Only `auto_frame_settle_curve` selects `Circ Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-23` | Only `auto_frame_settle_curve` selects `Circ In-Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-24` | Only `auto_frame_settle_curve` selects `Back In`; other selectors remain default. |
| `auto_frame_settle_curve-option-25` | Only `auto_frame_settle_curve` selects `Back Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-26` | Only `auto_frame_settle_curve` selects `Back In-Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-27` | Only `auto_frame_settle_curve` selects `Elastic In`; other selectors remain default. |
| `auto_frame_settle_curve-option-28` | Only `auto_frame_settle_curve` selects `Elastic Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-29` | Only `auto_frame_settle_curve` selects `Elastic In-Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-30` | Only `auto_frame_settle_curve` selects `Bounce In`; other selectors remain default. |
| `auto_frame_settle_curve-option-31` | Only `auto_frame_settle_curve` selects `Bounce Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-32` | Only `auto_frame_settle_curve` selects `Bounce In-Out`; other selectors remain default. |
| `auto_frame_settle_curve-option-33` | Only `auto_frame_settle_curve` selects `Custom (bezier)`; other selectors remain default. |
| `card-flycam_orbit-tallest-850` | Only `flycam_orbit` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-flycam_orbit-tallest-1160` | Only `flycam_orbit` is put in its tallest local state at 1160 px; all other cards stay default. |
| `CinematicCamJoint-option-0` | Only `CinematicCamJoint` selects `mHead`; other selectors remain default. |
| `CinematicCamJoint-option-1` | Only `CinematicCamJoint` selects `mNeck`; other selectors remain default. |
| `CinematicCamJoint-option-2` | Only `CinematicCamJoint` selects `mChest`; other selectors remain default. |
| `CinematicCamJoint-option-3` | Only `CinematicCamJoint` selects `mTorso`; other selectors remain default. |
| `CinematicCamJoint-option-4` | Only `CinematicCamJoint` selects `mPelvis`; other selectors remain default. |
| `CinematicCamJoint-option-5` | Only `CinematicCamJoint` selects `mShoulderRight`; other selectors remain default. |
| `CinematicCamJoint-option-6` | Only `CinematicCamJoint` selects `mElbowRight`; other selectors remain default. |
| `CinematicCamJoint-option-7` | Only `CinematicCamJoint` selects `mWristRight`; other selectors remain default. |
| `CinematicCamJoint-option-8` | Only `CinematicCamJoint` selects `mShoulderLeft`; other selectors remain default. |
| `CinematicCamJoint-option-9` | Only `CinematicCamJoint` selects `mWristLeft`; other selectors remain default. |
| `CinematicCamJoint-option-10` | Only `CinematicCamJoint` selects `mHipRight`; other selectors remain default. |
| `CinematicCamJoint-option-11` | Only `CinematicCamJoint` selects `mKneeRight`; other selectors remain default. |
| `CinematicCamJoint-option-12` | Only `CinematicCamJoint` selects `mFootRight`; other selectors remain default. |
| `CinematicCamJoint-option-13` | Only `CinematicCamJoint` selects `mFootLeft`; other selectors remain default. |
| `CinematicCamUseSelected-on` | Only `CinematicCamUseSelected` is on; dependent visibility is measured. |
| `FlycamOrbitLevel-on` | Only `FlycamOrbitLevel` is on; dependent visibility is measured. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. Missing action/model bindings below are USER DECISIONS, not permission to invent a visual class.

## Unresolved and USER DECISION items

- `auto_frame_resolve` (Re-solve now): no C++ reference or setting binding. **USER DECISION required.**
- `anchor_hint` (Anchor): no C++ reference or setting binding.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
