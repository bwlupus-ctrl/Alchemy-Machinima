# Director Hotkeys — F1–F12 audit & reservation

Audit of every F-key binding in **this fork** (Alchemy-Machinima, branch
`develop`), and the Director transport keys reserved from the genuinely free
ones. Implementation: `indra/newview/aldirectorhotkeys.{h,cpp}`, dispatched
from `LLViewerWindow::handleKey`; master switch `DirectorHotkeysEnabled`
(default **ON** — see verdicts below).

## Audit sources checked

| Source | Where | Result |
|---|---|---|
| Viewer keybinding defaults | `indra/newview/app_settings/key_bindings.xml` (LLViewerInput; user copy in `user_settings` overrides) | **No F-keys bound** in any mode (first_person / third_person / edit_avatar / sitting). The "F" bindings there are the letter F (fly), not F-keys. |
| Menu accelerators | `grep shortcut=` over `skins/default/xui/en/*.xml` | Hits in `menu_viewer.xml`, `menu_login.xml`, `panel_script_ed.xml` — itemized below. |
| Hardcoded key handling | `grep KEY_F1..KEY_F12` over `indra/newview` + `indra/llwindow` | `llfloaterhowto.cpp` (F1 closes the guidebook), `llinventorygallery.cpp` / `lloutfitgallery.cpp` (F2 = rename while that gallery has keyboard focus), `llpreviewgesture.cpp` (blocks the Ctrl+F10 combo in the gesture editor UI), `llviewerinput.cpp` (F-key repeat suppression + user F2–F12 remap support). `llwindow` hits are only key-name/translation tables, not bindings. |
| Gestures | `LLGestureMgr::triggerGesture` (llviewerwindow.cpp:3386) | Users may bind **any** F2–F12 (± modifiers) to gestures at runtime. Gestures are checked **before** our hook, so a user gesture always pre-empts a Director key — safe by construction, but a gesture on F3 will shadow ACTION. |

## F1–F12 verdicts (unmodified unless noted)

| Key | Currently bound to | Where | Verdict |
|---|---|---|---|
| F1 | Help / Guidebook (`Help.ToggleHowTo`); guidebook floater also closes on F1 | `menu_viewer.xml:2189`, `menu_login.xml:62`, `llfloaterhowto.cpp:85` | **CONFLICT** — respected, not used |
| F2 | Free. (Rename in inventory/outfit gallery **only while that gallery has keyboard focus** — focused-UI handling runs before our hook, so it still wins) | `llinventorygallery.cpp:1114`, `lloutfitgallery.cpp:185` | **SAFE** → reserved |
| F3 | Free | — | **SAFE** → reserved |
| F4 | Free unmodified. **Alt+F4 = OS window close** — we bind unmodified only; modified combos fall through our hook untouched | OS | **SAFE (unmodified)** → reserved |
| F5 | Free | — | **SAFE** → reserved |
| F6 | Free | — | **SAFE** → reserved |
| F7 | Free | — | **SAFE** → reserved |
| F8 | Free | — | **SAFE** → reserved |
| F9 | Free | — | SAFE — left unreserved (headroom) |
| F10 | Free in-viewer. Gesture editor refuses the Ctrl+F10 combo (`llpreviewgesture.cpp`), and F10 is the Windows menu-bar convention in native apps (not intercepted by this GL window) | `llpreviewgesture.cpp:949` | SAFE — left unreserved |
| F11 | Free (no fullscreen binding in this fork) | — | SAFE — left unreserved |
| F12 | Free | — | SAFE — left unreserved |

Modified combos that must never be shadowed (and cannot be — the hook rejects
any nonzero mask before doing anything):

| Combo | Bound to | Where |
|---|---|---|
| Ctrl+Alt+F1 | **Hide/show UI** (Advanced ▸ Rendering Features ▸ UI) — the fork's HideUI machinery | `menu_viewer.xml:2955` |
| Ctrl+Alt+F2…F6, F8, F9 | Rendering feature toggles (Selected, Highlighted, Dynamic Textures, Foot Shadows, Fog, Test FRInfo, Flexible) | `menu_viewer.xml:2966–3032` |
| Shift+F1 | Script editor help | `panel_script_ed.xml:169` (floater-local) |
| Alt+F4 | OS window close | OS |

## Reserved Director keys

Active only when `DirectorHotkeysEnabled` (default **ON**, because unmodified
F2–F12 carry no viewer bindings) **and** — except F2 — the Director Console or
Actor Mover floater is open:

| Key | Action | Implementation |
|---|---|---|
| F2 | Toggle Director Console | `LLFloaterReg::toggleInstance("director")` |
| F3 | ACTION — morphs to CUT while counting down / running (exactly the console's big button) | `LLDirectorCast::action()` / `cut()` |
| F4 | CUT (also cancels a pending countdown; no-op when idle) | `LLDirectorCast::cut()` |
| F5 | Reset to marks | `LLDirectorCast::resetToMarks()` |
| F6 | Pose ghosts on/off | toggles `PathShowOnionSkin` |
| F7 | Flycam take play/pause (no-op without a loaded take) | `LLFlycamRecorder::togglePlayback()` |
| F8 | Set marks | `LLDirectorCast::setMarks()` |

**Documented deviation:** F2 (console toggle) works even with no operator
floater open — a toggle that could only ever *close* the console would be
pointless. It still honors `DirectorHotkeysEnabled`, and gestures / focused-UI
handling still pre-empt it.

## Mechanism choice (why not the keybinding registry or menu accelerators)

The fork's real binding registry is `LLKeyboardActionRegistry` +
`key_bindings.xml` (`LLViewerInput`). It was **not** used because:

1. `key_bindings.xml` is copied to `user_settings` and user-edited; existing
   installs would never receive new default bindings without a migration.
2. Its bindings are per-agent-mode world bindings that only fire when the UI
   did not consume the key; the required gate here ("only while a Director
   floater is open, regardless of focus") doesn't map onto that model without
   gating inside the handlers anyway.

Menu accelerators (the F1/Help mechanism) were also rejected: an accelerator
always eats its key once registered, so a gated-off Director key would still
swallow F3 for the whole session.

Instead there is one surgical hook in `LLViewerWindow::handleKey`
(`llviewerwindow.cpp`, directly above the menu-accelerator dispatch):
**after** focused-UI / tool / gesture handling — so all of those pre-empt us —
and **before** menu accelerators — so we could never shadow F1 anyway (we
don't bind it). When the set is gated off the hook returns false without side
effects and every key keeps its normal meaning. F-key auto-repeat is already
suppressed upstream (`llviewerinput.cpp:1206`), so holding F3 cannot re-fire
ACTION.
