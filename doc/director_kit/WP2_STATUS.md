# Director kit WP2 status

Audit basis: `DIRECTOR_KIT_P0_SPEC.md` sections 3.7-3.12 and 3.16-3.18, WP2 acceptance, the R1-R5 ledgers, the WP1 R3 focus-baseline note, and the rollout's P0 exposure rule. No build, Python gate, or commit was run, as requested.

| Spec item | Status | Notes |
|---|---|---|
| 3.7 shell/layout/rail/tab strip/roster | done | Responsive shell, rail navigation/icons/active state, roster reparenting, lazy page construction, handle-guarded page callbacks, horizontal tab overflow arrows/wheel guard, master-off pips, distinct mode proxy, and persisted chip/card/who-pin state are implemented. Mode is deliberately sourced only from its bound setting and is never persisted in page state. |
| 3.7 popovers/ALKitBar/help | done | Anchored/clamped/exclusive owner-toggled popovers, outside-click/Esc close, arm/legend/hotkey/Who wiring, diffed bar flex/collapse, and event-driven labeled/ellipsized help are implemented without draw-time lookups. |
| 3.8 search | done | Version/15-field validation, skinned text load, 400-line handle-guarded idle batches, camel/non-alphanumeric tokenization, interned tokens, prefix scoring/stable ordering, 120 ms handle-safe debounce, current-page filtering, global-setting behavior, 12-row other-page popover, header/footer, keyboard highlight, Enter navigation, and Esc clearing are implemented. |
| 3.9 presets/SnapshotStore | done | Idempotent registry, cached reads, corrupt-store refusal, temp/rename writes with `.bak`, tracked-key-only loads, corrected Rim settings, provider strip names, modified observation/revert, inline save, and confirmed delete are implemented. |
| 3.11 history | done in WP2 scope | Page/chrome watches, pointer/keyboard admission, idle coalescing, row labels, reverse custom undo, bounded replay/context/generation barriers, scene/reset suspension, and global live-floater lookup are implemented. External Lightbox/cue call sites remain outside the allowed WP2 files. |
| 3.12 keyboard | done | v2-only accelerators, popover/search/CUT Esc order, Unicode `/`, text-entry exclusion, Ctrl+Z/Y, PgUp/PgDn chip navigation, Shift page navigation, Alt group navigation, search Up/Down/Enter, and focused-row Home reset are implemented; v1 still returns false. |
| 3.16 settings/registration/exposure | done | All ten settings and setting-gated floater registration are present. `menu_viewer.xml` intentionally has no item: section 3.16 and rollout WP12 expressly forbid P0 normal-user exposure while `DirectorUIV2` remains default-off. |
| 3.17 floater integration | done | v1 retains its original binder statement order. v2 now binds chrome immediately and uses a table-driven, page-built binder with handle lifetime, lazy construction, unwrapped scroll-fit adoption, cast signal, input history, size bump, index loading and 2 Hz UI-ms reporting. Named page refreshers have independent unbound guards. |
| 3.18 rim extraction/panel | done | `RigRimPresetRow`, the 20-row table and apply function now live in `ALRigRimController`; the public/static `ALPanelCineLightRig::applyRigRimPresetByName` remains as a forwarding compatibility API. Panel cadence, handle-safe confirmation, provider registration and standalone panel remain intact. |
| v1 invariants | done by code inspection | The v1 branch continues through the pre-existing `postBuild` body without reordered statements; v2 takes the new early branch. `hasAccelerators()` is false in v1, page refreshers are null-safe, and the Cine Light Rig controller call sites/cadence are unchanged. |
| Compile/static risk | awaiting orchestrator gate | Fixed the two reported `/Zs` failures (`LLTextBox::Params::wrap`, `LLFile::mkdir`) and swept WP2 sources for the same invented APIs. Scoped `git diff --check` passes. Per instruction, no build, `/Zs`, Python, or commit was run; the real Release `/Zs` gate remains the independent compile verdict. |

## Opus R1 fix ledger

| Finding | Status | Resolution |
|---|---|---|
| B1 | done | Removed mode save/restore; a page build/restore guard prevents setting writes. |
| M1, M8 | done | Popovers only resolve anchors; their owners install the single toggle callback. |
| M2 | done | Lazy adoption now uses the external floater ScrollFit controller when installed. |
| M3 | done | Dance/Exposure bind their real v1 panel names and fill the actual legacy host parent; no `*_tab_logic` lookup remains. |
| M4 | done | Wheel and key gestures extend one open step until 500 ms idle. |
| M5 | done | Viewer pointer timestamps admit combo commits at selection time, independent of dropdown-open duration. |
| M6 | done | Scene load and Rig reset-all are suspended and end in history barriers. |
| M7 | done | Who uses a non-reparented source combo, Follow rail plus item buttons, one chip handler, and persisted `who_pin`; pinned pages ignore rail changes. |
| m1 | done | The gesture timer releases itself when the gesture closes and is reset by undo/redo/barrier. |
| m2 | done | Setting steps use page/row labels when available; custom undo entries replay in reverse. |
| m3, m4 | done | Key activity extends idle close; pointer page attribution uses `pageKeyAt`, with chrome watches registered. |
| m5 | done | Lower-case labels are precomputed, self-test queries log timing/hits, and Enter honors a moved highlight. |
| m6 | done | The current kit page is handle-cached on layout/page build instead of searched per pointer event. |
| m7 | done | Snapshot names/cache, bad-parse refusal, atomic temp/rename backup, and tracked-key load filtering are implemented. |
| m8, m9 | done | Rim tracking adds ActorsOnlyAlpha/removes WrapClamp; preset names derive from the table with a count assertion; dead callback removed. |
| m10 | done | Provider controls use page-prefixed `_preset`, `_preset_save`, and `_preset_delete` names. |
| m11, m12 | done | Help uses Group › Page labels and a custom ellipsized strip; legacy navigation scrolls the nearest container before highlight. |
| m13 | done | One-time rect bump preserves larger dimensions and clamps both dimensions to the floater view. |
| m14, m15 | done | Actor/Animate binders clear partial state and validate required text controls; late-bound Gaze gets the current selection immediately. |
| m16 | done | The parity-sensitive v1/v2 duplicate is documented; the v2 unwrapped list is derived from the shell tab tree. |
| m17 | done | Reshape no longer rereads pip settings/tokens; pip/mode labels are diffed and pips update on their signals or breakpoint change. |
| m18 | done | Cast-roster open is read at shell build; UI-ms visibility follows its setting signal. |
| m19, m20 | done | Esc clears then blurs search; bar collapse computes before diffed visibility, and text-entry detection is shared. |

## Opus R1 verification / fix round 2

| Finding | Status | Resolution |
|---|---|---|
| N1 | done | `dance_legacy_host` and `exposure_legacy_host` are explicit ScrollFit exceptions, so their embedded panels remain direct host children and `fillPage` is no longer circular. The other legacy hosts remain intentionally wrapped; no other WP2 `fillPage` consumer requires a direct host. |
| N2 | done | Tab-strip gestures resolve to the current page, while page clicks, keyboard admission and search reveals all record mode-setting changes against that same current page as one coalesced step. |
| N3 | done | Every shell selection and every floater tab commit now synchronizes the cached page and chrome state, covering key cycling, search jumps, restored tabs and direct/programmatic tab selection. |
| N4 | done | Built kit pages are cached by page key. Coalesced setting updates return before row-label resolution, so only a newly inserted undo entry performs the O(1) page lookup and row lookup. |
| n1-n2 | done | Who labels refresh from the live combo whenever the popover opens, restore sets only the pin flag, and the help strip uses the kit text colour token. |
| n3-n5 | done | ALKitBar accepts external collapsible visibility changes with immediate relayout; corrupt preset saves/deletes notify without touching the file; pointer admission timestamps only hits inside a kit shell. |
| n6-n7 | done | Selection synchronization prevents cross-page state saves. A WP2 runtime launcher is added to the Scene page bar and wired through the existing Snapshot launcher path, preserving access without touching WP3-generated XUI. |
| Verification | static only | Scoped `git diff --check` passes. Per instruction, no build, `/Zs`, Python, generated-XUI edit, or commit was run. |
- **R2-M1 — done:** Passive mode changes and all `navigateTo` work are history-suspended; reveal no longer opens a Mode gesture, while explicit clicks retain their page-attributed undo step.
- **R2-M2 — done:** Pointer admission checks the on-screen top control first, preserving combo ownership through its ancestry when a dropped list overhangs the Director, then falls back to root hit-testing.
- **R2-M3 — done:** Removed the C++ Scene-page Snapshot launcher API and injection; WP3-generated page content remains the sole owner.
