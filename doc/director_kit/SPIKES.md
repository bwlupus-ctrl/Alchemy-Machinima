# Director kit WP1 spikes

Code-read results, 2026-10-10. Runtime/build checks were prohibited for this work loop, so anything that requires construction, timing, rendering, or signal-count measurement uses the specified fallback.

## S1 — LLSliderCtrl value column

`LLSliderCtrl` takes an explicitly provided `text_width` verbatim, puts `text_left` at `width - text_width`, and right-anchors the value editor/text box there (`llsliderctrl.cpp:71,96-113,176-210,394-405`). A 52 px value column is therefore supported. Its editor uses `validateFloat`, so a rendered unit suffix would break editing. Decision: use the 52 px bare numeric value; keep the unit in row/help metadata (fallback).

## S2 — ALKitSwitch parse and value flow

`ALKitSwitch::Params` derives from `LLCheckBoxCtrl::Params`, so the existing attributes parse. The widget defaults copy the inner 13x13 toggle structure and the subclass expands its hit rect. `LLCheckBoxCtrl::setControlName` binds `mButton` (`llcheckboxctrl.cpp:280-283`), external changes call the bound control's `setValue` (`lluictrl.cpp:639-647`), and direct checkbox `setValue` writes `mButton` (`llcheckboxctrl.cpp:238-241`). The subclass observes all three paths with edge suppression and replaces its scoped setting connection on rebind. Exact click/undo edge counts and alpha inheritance require runtime proof; decision: retain the explicit hooks and draw the switch directly, with no overlay fallback unless later runtime evidence requires it.

## S3 — lazy live-page construction

`LLUICtrlFactory::create()` calls `createWidgetImpl(params,parent)` before `postBuild()` (`lluictrlfactory.h:132-143`), and factory parenting is `parent->addChild` (`lluictrlfactory.cpp:234-239`), so the parent chain exists during `postBuild`. `ALScrollFit::fitPage` recursively follows ordinary panels, nested current tabs, scroll documents, and deeper scrollers, so it reaches a legacy host's scrollers. Re-entrancy and Rig build time cannot be proven without running. Decision: use the fallback and prebuild the Rig on idle when WP2 supplies the shell scheduler; WP1's generic lazy page remains first-reveal construction because no Rig page exists yet.

## S6 — IBM Plex Mono

`skins/default/xui/en/fonts.xml` declares family `IBMPlexMono` with regular/bold/italic faces and global size names `Small` (9) and `Medium` (10) at lines 325-337 and 651-655. Decision: request `LLFontDescriptor("IBMPlexMono", "Small", 0)` and fall back to `getFontMonospace()`. UI-scale geometry requires runtime proof; fixed 28 px logical metrics remain as specified.

## S9 — amber storm and rebind

`LLUICtrl::setControlName` owns a replaceable control connection and immediately seeds from the control (`lluictrl.cpp:510-535`); `LLCheckBoxCtrl` delegates binding to its inner button. `ALKitSwitch` separately disconnects its scoped observer before rebinding, so an old setting cannot drive its edge signal. The ~150-signal timing cannot be proven without running. Decision: take the fallback for amber work—setting changes fan out through `ALKitSettingWatch`, and layout consumers must coalesce visual recomputation into an idle pass when WP2 installs the scheduler.

R5 clarification: every declared shortcut setting must be included in the shortcut page's `watch_settings`; home-canonical applies only to search/help/step labeling. The Rig Easy Edge (`cine_easy_cone_feather`) has no `control_name`, so its undo source is the row's code-bound `recordWidget` hook, not the setting watch.
