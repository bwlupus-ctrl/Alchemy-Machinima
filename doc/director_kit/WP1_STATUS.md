# Director kit WP1 status

Audit basis: `DIRECTOR_KIT_P0_SPEC.md` sections 3.1-3.14, 6.6 and WP1, plus the WP1 minors in `REVIEW_SPECS_OPUS_R5.md`. `done` means implemented in WP1 scope; `done (interface)` means the linkable WP1 contract explicitly assigned to WP2; `partial` is an implementation gap, not a deferred claim.

## 3.1 ALKitStyle — done

- **done** All section 3.1 logical-pixel metrics are declared, including row/dial/switch/card/grid/page/sub/more/selector/shell/help/bar/map/breakpoint/flash metrics.
- **done** Every `Kit*` colour is declared once in `colors.xml` and cached as `LLUIColor` by the enum accessor; labels/titles use viewer fonts and numeric text requests IBM Plex Mono Small with the mandated fallback.
- **done** The deterministic, idempotent no-PIL PNG generator and all four output masks exist. `textures.xml` references exactly `kit/Kit_RoundRect4.png`, `kit/Kit_RoundRect6.png`, `kit/Kit_Pill9.png`, and `kit/Kit_Circle.png`; the files exist with those names.

## 3.2 ALKitSwitch — done

- **done** `LLCheckBoxCtrl::Params` inheritance, `kit_switch` registration, copied 13x13 inner-button XUI defaults, full-pill hit rect, stock-image suppression, pill/knob/disabled/focus rendering, and hidden stock label.
- **done** `isOn()` reads the inner button; `setControlName()` disconnects the old observer and consumes the signal value; commit, direct `setValue`, and passive bound writes all use edge-only reconciliation.

## 3.3 ALKitRow — done

- **done** All declared params and metadata fields parse; labels, reset controls, unit/state/dial value text, fixed column layout, kind heights, visibility/filter/code-hide handling, hover/focus help, flash, binding reset order, amber signal hook, and code-row widget history hook exist.
- **done** Link rows create their own control and navigate through the shell contract; shortcut settings are added to the page-local watch set, and code-bound Easy Edge records through `recordWidget`.
- **done** Vec2/vec3 rows create equal component spinners and synchronize through `ALKitVecBinding`; row amber flips propagate without polling.
- **done** Per-kind control geometry covers number/color/gobo/button/list/viewport/custom spans; card aim groups use the 80 px two-up dial cell; mirror combos follow their S32 setting; added rows paint their accent dot.

## 3.4 ALKitCard / ALKitOption — done

- **done** Params/tags, option LLSD values, row/option discovery, header title/badge/divider/reset/enable placement, selector value-based switching and <=6 segments, per-option More state, card sizing, row collapsing, all-row allocation-free visibility bits, coalesced handle-guarded relayout, card background/border, and scoped reset exclusions exist.
- **done** Card amber is maintained as a signal-driven counter and updates the card-reset glyph outside draw.
- **done** Selector cards use value-bearing segments through six options and a value-bearing combo above six; every option enable exists for C++ lookup, the active option's switch occupies the header, and an off switch dims body children only.
- **done** Combined sub-headings, 2-up aim groups, first-sub header presets, later-sub heading presets, link signal-driven summaries/navigation, launcher dispatch, rig-map header view segments, advanced-card Show/Hide, and selector reset-all context menu are implemented.

## 3.5 ALKitPage — done

- **done** All params parse; `kit_page`, `kit_scroller`, `kit_doc`, and `kit_preset_host` contracts exist; cards are reparented into a two-column row grid; lookup/search/reveal/reset APIs exist; layouts coalesce; row settings (including shortcut rows) augment the page watch set; the self-sized document permanently reserves scrollbar width.
- **done** Filter/anchor chips, spanning anchor headings, Who readout and cast-adapter follow, widget-preset reparenting, reset/undo/redo/master/search bar, off banner/document dimming, legacy host, hidden mode switch plus page-instance mode-adapter contract, scroll-to-reveal, and draw-only history/master revision gates are present.
- **done (WP1 boundary)** The provider preset strip, confirmation/Who popovers, persisted page state, real search, and final mode/Who adapters deliberately call or expose inert interfaces here; section 7 assigns their implementations and settings to WP2. No WP2 shell/popover/settings file was pulled into WP1.

## 3.6 ALKitLazyPage — done

- **done** Tag/params, one-shot factory construction, local-rect reshape/follows, `adoptPage`, profiling/logging, and build-before-base visibility ordering.
- **done** The WP1 shell contract owns the page-built signal and LazyPage emits it after adoption.

## 3.7 ALKitShell — done (interface)

- **done (interface)** WP1 retains the minimal registered shell/accessor/navigation/history/help/scroll-fit contract required by Row/Page/LazyPage. Rail, roster, tab strip, responsive shell, popovers, signals, and real navigation are WP2-owned by the work-package table and are intentionally inert here.
- **done** `computeShellLayout` supplies the pure constraint layout and hysteresis contract for WP1 tests.

## 3.8 ALKitSearch — done (interface)

- **done (interface)** `ALKitSearch`, `Hit`, and `Hits` compile-facing contracts needed by `ALKitPage` exist. Index loading/tokenization/query UI/debounce are the explicitly WP2-owned implementation.
- **done** R5 canonical-home wording is preserved: shortcut pages watch edits locally; home canonicalization applies to later search/help/labels only.

## 3.9 Presets — done (interface)

- **done (interface)** Provider, registry, snapshot-store, entry, revision and tracked-setting signatures exist as linkable WP1 stubs. Persistence/providers/strip UI are WP2-owned.

## 3.10 Bindings and amber — done

- **done** Setting/list/widget/vector binding types, page-root/widget registry, precision-aware scalar default comparison, single-source setting-watch fanout, and Who adapter registry exist.
- **done** Array-backed vector/colour defaults compare per component at three decimals; vector setting changes fan out to component spinners.
- **done** Setting/list/vector bindings share `ALKitSettingWatch`'s one source connection per setting; `debugRecomputeAll()` performs and times a complete same-value signal recomputation without polling in draw.
- **done** Binding factories are keyed to the containing page (not the global UI root), carry a page handle, and prune dead owners before add/make.

## 3.11 ALKitHistory — done (interface)

- **done (interface)** Full public types/signatures and linkable widget/custom record, replay, stack, revision, suspend and barrier stubs exist so WP1 widgets link. Gesture timing, setting capture/context/refusal/toasts/global live-Director resolution are WP2-owned.
- **done** Easy Edge's no-`control_name` path reaches the widget hook; declared shortcut settings enter the gesture page's watch list.

## 3.12 Keyboard — done (interface)

- **done (interface)** `ALKitScroller` passes PgUp/PgDn upward and map/dial controls implement their local keys. Floater accelerators, Unicode `/`, text-entry exclusion, chip/page/group navigation, hotkey popover and Director dispatch modify WP2-only files and are not implemented in WP1.

## 3.13 ALCompassDial arc mode — done

- **done** Backward-compatible Params/defaults, range-aware kit compass wrap, arc clamp/horizon mapping, left-weighted arc geometry, control-column-centred kit compass geometry, unchanged legacy wrap/dead-zone/tab/key behavior, half-face/track/baseline/needle/range labels, captured live commit, global wheel guard/opt-out, kit-only keys/Home, rounded bound commit, cached draw strings, and Tracy zone.

## 3.14 ALLightRigMap — done

- **done** Params/tag/settings observers, live eased display, uneased map target accessor/revision, object-front facing publication, unmirrored fallback, camera-relative Top/Side/2.5D projection, gel/role/off colours, floor/meridian, depth sort, back/subject/front layering, glow/solid/dashed/off/under/selected styles, camera labels, profile cues, Side From-cam-L/R chip, four selectable legend chips, topmost disc hit tests, immutable mouse-down snapshot, exact absolute view inverses, no-write click, keyboard, and wheel pass-through.
- **done** Camera-relative legend strings and the distance/fallback footer rebuild outside draw on a handle-guarded visible-only 10 Hz callback; the Side chip includes its mini globe, subject, camera, eye and sight line.
- **done** The mirrored Side profile includes head/nose/eye/pupil/hair/near arm/feet/gaze; under-light rings are dashed; external role/orbit/radius/mirror/view/side writes cancel capture while gesture-owned writes are guarded; selection is synchronized through `DirectorV2SelectedLight`; drag opens the page history gesture.

## alkitmath.h — done

- **done** Header-only/no-UI implementations exist for wrap/arc, displayed-pitch inverse, projection/front factor/yaw text/effective yaw, immutable absolute Top/2.5D/Side drag, shell layout and row packing. Zero-pointer paths return captured settings bit-identically.

## TUT list (section 6.6) — done

- **done** Tests cover wrap, all arc ranges/endpoints/horizon, projection and both Side directions, Side layers/yaw text axes, scaled/unscaled pitch inverse, zero-motion across all views/sides/mirror signs/radii, fixed-pointer no-drift, out/back, Top mirror/wrap, front bounds, effective yaw, all shell widths/hysteresis/track, and hidden/spanning row packing.
- **done** Pure helpers/tests now cover legacy-vs-kit dial geometry and signed/unsigned compass `[min,max)` wrapping.
- **done** Additional tests exercise 1,000-event fixed and out/back paths across views, side directions, scaled radii and both 2.5D depth branches; fake easing cannot perturb immutable drag results; Side pole and 2.5D silhouette continuity, rotated-object facing, and a full-turn Top path are asserted.

## Modified helpers and assets

- **done** `ALFlowGrid pack="rows"` implements row-major/top-aligned/spanning/hidden behavior behind an early branch; the pre-existing masonry body is unchanged. `ALScrollFit` adds only the self-sized-document early return on the v1 fit path plus `adoptPage` for lazy pages.
- **done** `ALCineLightRig` publishes the uneased target frame/revision after resolved facing/subject scale and publishes finite object-front azimuth from `getRenderRotation()`.
- **done** CMake lists every WP1 source/header and the `alkitmath` integration test. Kit colours/textures/XUI are in the allowed files only. The orchestrator-reported draw lint and XUI layout check are both 0.

## Counts and compile risks

Top-level implementation count (3.1-3.14 + maths + TUT): **16 code-complete, runtime proof pending, 0 intentionally deferred WP1 items**.

## R1 fix ledger (2026-10-10)

- **BLOCKER B1-B4:** added a distinct `ALKitScroller::Params`; cleared every cited `/W3 /WX` site; removed stock switch drawing/image mutation; initialized the full programmatic hit rect and reseeded bound state after binding. All registered kit widgets were audited for distinct Params types.
- **BLOCKER B5-B8:** selector values now convert to the setting's declared control type; option ancestors receive real geometry; card changes coalesce into page relayout; legacy hosts start at the page's real local rect before reparenting.
- **BLOCKER B9-B10:** drag snapshots separate metres from projected pixels; row-grid sizing precedes placement; document sizing precedes grid origin; only the document is viewport-floored, removing dead scroll.
- **MAJOR M1-M6:** centred the kit compass in its control column and documented the decision; copied the rig engine's mirror/orbit ordering; made fallback yaw camera-relative; used target-frame grab offsets and role-keyed colours; rewrote inverse tests around moved projected pointers for every view/side/mirror and both 2.5D branches.
- **MAJOR M7-M11:** vec2 preserves z and compares only edited components; absent ranges stay absent; toggle/dial/readout text follows controls; mode always derives from effective state; late factories rebind live rows; reset gestures pass page keys.
- **MAJOR M12-M15:** auto chips count effective visible cards and hide empty sections; deferred reveal scrolling converts into document coordinates; `adoptPage` discovers the real kit page and never double-wraps; menus live in `gMenuHolder`; all new dial input behavior is kit-only.
- **MINOR 1-6:** map targets compute on demand; page structure/banner/master/who/legacy names exist; card title/badge/round mask were corrected; 9-slice scales fixed; Top ring colour, scoped Side tooltip, Unicode cycling, slash length and near shell fixed.
- **MINOR 7-12:** Top front factor restored; card lookup cached; page draw uses cached shell/diff writes and cached scrollbar; adapter entries carry/prune handles; bind defaults empty; shell track includes card padding and scrollbar reservation.
- **MINOR 13-18:** exact Top pitch and Side depth tie-break; widget value/amber refresh and reveal baseline; precision-aware settings maps; launcher naming collision removed; `ALFlowGrid final` restored; lazy build failures log once. Status no longer claims runtime proof.
- **Verification:** no build or commit was run. `git diff --check` is clean; cited API signatures and behavior were checked against the real `llui/newview` implementations. The orchestrator's syntax-only `/Zs /W3 /WX` gate and runtime spikes remain the independent proof steps.

## R2 fix ledger (2026-10-10)

- **BLOCKER NB1-NB2 / partial B10:** the R2 pitch sweep used `current=pitch` and did not exercise branch selection; its inverse claim is superseded by the R3 ledger below. Short-page grids are top-anchored after the viewport floor.
- **MAJOR NM1-NM3 / partial M6:** Side tests use same-depth targets and explicitly prove cross-side pointers retain the captured side; row history has separate displayed and undo-baseline values, installs before binding commit observers, and adopts passive writes only outside edits; direct v1 control visibility again collapses/reveals rows.
- **MAJOR NM4-NM6 / partial M14:** the master box follows the complete left cluster while search alone is right-aligned and collision-collapsed; one visible-page 10 Hz clock replaces all per-row timers; reset menus use a guarded `LLHandle<LLContextMenu>` and `die()`.
- **MINOR 1-4:** WP2 now explicitly owns the Who popover instead of revealing its hidden combo; legacy summary moved into the bar; card masks are constructor-cached; option overlays are non-mouse-opaque.
- **MINOR 5-8:** kit compass geometry keeps cardinals inside 48 px without changing legacy geometry; launcher `_launch` is recorded in the spec; `adoptPage` uses the page's typed scroller/legacy-host accessors; Side inversion uses the captured depth sign in its `atan2` branch (the projected axis remains inherently ill-conditioned and retains its tolerance).
- **MINOR 9-11:** map-target snapshot types were audited as fixed-size arithmetic structs (the review's claimed `std::string` allocation is not present) and remain lazy to avoid off-screen frame computation; the full Side tooltip is restored; shell layout reserves the scrollbar once across the grid rather than once per card.
- **Verification:** no build, test executable, or commit was run, per instruction. `git diff --check` is clean; the orchestrator's `/Zs` gate and standalone `alkitmath` harness remain the independent proof steps.

## R3 fix ledger (2026-10-10)

- **BLOCKER RB1 / partial M6:** `baseFromDisplayPitch` now clamps unreachable pointers to the reachable cone, clamps each principal root to ±85° before displayed-error scoring, and uses distance to the captured pitch only as the tie-break. The incorrect `NB1_FIX_RECIPE.md` was removed.
- **Tests:** test 4 varies the captured/current pitch by ±10/30/60° across injective, near-radius, and outside-radius ratios, checking base space below the radius and display space at/above it; test 19 covers both ±85° saturation inside the circle and reachable-cone saturation outside it.
- **MINOR 1-2:** history baselines freeze only during mouse capture or focused text entry, so passive writes while buttons/toggles/combos merely retain focus are adopted. WP4 acceptance records the passive widget-write amber refresh required for Easy feather.
- **MINOR 3-4, 7:** the page bar starts at `PAGE_PAD_LR`; search shrinks to 120 px before hiding; legacy summaries use single-line ellipses. Section 3.5 records both layout rules.
- **MINOR 5-6:** kit compass cardinal offsets use `4*scale` in the real draw; Who-chip polling shares the visible page's 10 Hz readout clock instead of owning a page timer.
- **MINOR 8-9:** the R2 sweep and Side precision overclaims above are corrected. The `_launch`, kit-compass, Side-branch, and WP2 Who spec edits remain in the commit-ready spec together with the R3 clarifications.
- **Verification:** no build, test executable, or commit was run, per instruction. Source review and `git diff --check` are the local checks; the orchestrator owns `/Zs`, the standalone harness, and TUT execution.
