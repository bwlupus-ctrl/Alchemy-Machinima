# Actor Chroma Key / Matte — design (v13)

Status: DESIGN ONLY (2026-10-05, v13 after Codex round 11: NOT APPROVED → revised (checklist precision only); D26 and D30 decided by the
user: v1 = no ReShade DLL change). No source edits, no build, no commit. Written by Fable from the code
at HEAD; every file:line cited was read, not recalled.
Labels: **PROVES** = read in code, **IMPLIES** = from a design doc, **INFERENCE** = reasoning not yet
checked in code or in-world.

## v13 changes (Codex R11 resolution)

Accepted by R11 and not reopened: the v12 refusal latch and both v12 corrections (item 9 ownerless
draws, item 5 `@setoverlay`); no other open item. Four checklist precision fixes:

| Codex R11 item | v13 resolution | Where |
|---|---|---|
| §14 item 3: I-REBIND checked only `mActorKeyMatte` | Per-target debug write serial (bind/clear/copy-or-blit destination/swap/allocate/release); asserted unchanged for `mMainRT.actorMask` from mask completion to `buildMatte`, and for `mActorKeyMatte` from `buildMatte` to the blit; destination-use grep listed in the WP1 review | §14 item 3 |
| Item 14 had no test for "Exact never misses a beauty actor shadow" | **W-9m**: `ActorKeyShadowSelfTest` compares the beauty actor-shadow mask with Exact's term (`SHADOW-EXACT miss` required 0, one-texel edge tolerance) for system-body, rigid-attachment, rigged-only and GLTF casters, plus a mode-2 vs Exact snapshot cross-check | §14 item 14, §11 9m, §7 |
| Item 17's presenting-thread evidence was false for the public `render_technique` (6.7 runtime_api.cpp:1245-1313) | Reclassified IMPLEMENTATION-TIME; off-present callbacks explicitly **supported** (counter handlers take only the SRWLOCK and call nothing else; an off-present event refuses the next present); acceptance **U-CONC** concurrency test | §14 item 17, §11 |
| R5-D said `reshade_render_technique` fires before a technique | Corrected: begin before the loop (3909), technique event **after** each technique (4213), finish after the loop (3949); W-9i(d) now requires a **real** `render_effects` call (`/DSL_PROBE=15`) and a real public `render_technique` call (`/DSL_PROBE=16`); the synthetic-counter fallback is removed | §14 R5-D, §11 9i(d) |
## v12 changes (Codex R10 resolution)

Confirmed by R10 and not reopened: the packed module-state word closes the settle race; unsigned
32-bit subtraction handles `GetTickCount` wrap.

| Codex R10 item | v12 resolution | Where |
|---|---|---|
| Refusal latch: a non-clearing input change (suspension reason A → B, a disconnect, window movement while `gWindowResized` stays true) made the tuple unequal and bypassed suppression | Automatic requests return immediately **whenever latched**, unless one of the enumerated clearing events occurred (explicit user action, key toggle/`revision()` change, requested-size change, window-size change with `gWindowResized == false`, reconnect, suspension reason → none). Non-clearing changes update the latch **silently** (no render, log or notification); no tuple-equality test exists any more | §4.4 item 2b, §11 U-SNAP |
| §14 still carried open safety/invariant checks (R5-C..E, items 1-19) | §14 rewritten as a **closed checklist**: each item RESOLVED with file:line evidence and section, or IMPLEMENTATION-TIME with a named acceptance test (U-…, W-…, or a new I-… check defined there). Corrections found while closing: item 9's "never set-map pollution" was wrong (an ownerless draw lands in the set map) and is withdrawn with a required `caster_no_owner == 0` check; item 5 resolved (`@setoverlay` draws after the blit — new §4 table row); item 6 resolved with a `H_target == 0` rule; item 8 added to U-SPLIT | §14, §4 table, §11 |

## v11 changes (Codex R9 resolution)

Confirmed by R9 and not reopened: the present path — Win32 has exactly one `SwapBuffers` call
(llwindowwin32.cpp:3925-3929) and no `wglSwapLayerBuffers`; blocking it during a synchronous snapshot
causes no GL-state or vsync issue. Everything else from R1-R8 stays closed.

| Codex R9 item | v11 resolution | Where |
|---|---|---|
| P0 settle race: the callback bumped the generation and the timestamp in two separate stores, so a paused callback let a scan pair cache CLEAN with a new generation and an old time; the 500 ms fast path compared the generation only | **One packed 64-bit word** `g_mod_state` / `sModState` = generation (high 32) \| `GetTickCount()` of that bump (low 32), written only by the notification callback through an `InterlockedCompareExchange64` CAS loop (loader-lock-safe: kernel32 tick + Interlocked only); every reader takes one atomic 64-bit read; the quiet period, the across-scans check, the cache key and the 500 ms fast path all use the **whole word**; wrap-safe 32-bit age arithmetic | §4.2 whitelist, reader states, §11 |
| P1 refusal UX: the live preview retries every idle cycle, repeating render, log and notification | **Refusal latch**: first refusal latches {reason, key revision, size, window size, connected, suspension}; automatic (idle) requests with unchanged inputs return immediately with no render/log/notification and a placeholder; the latch clears with one retry on explicit user action, key toggle/revision change, size change, post-resize window change, reconnect, or the suspension reason clearing; trigger passed via `SnapshotRequestScope(explicit)` | §4.4 item 2b, §8, §11 |

Also updated: §8 files, §14 rewritten for Codex round 10.

## v10 changes (Codex R8 resolution)

Confirmed by R8 and not reopened: every other R7 closure; R8-B/E/F; save/thumbnail/postcard/inventory
snapshots all converge on `rawSnapshot`; 360 uses `simpleSnapshot`; Prism and cube auxiliary renders are
intentionally unkeyed; ReShade 6.8.0 uses the base path when `AddonPath` is absent and iterates the
folder non-recursively over `.addon`/`.addon64`.

| Codex R8 item | v10 resolution | Where |
|---|---|---|
| P0 snapshot "never presented" premise is false (resize early-return present, llviewerdisplay.cpp:437-443; start-up presents 201-212, 573; disconnected first `swap()` re-arms `gDisplaySwapBuffers`, 696-700/1841, so the later swap presents) | **Snapshot completion contract**: a `SnapshotScope` around `rawSnapshot` blocks **every** present at the single `SwapBuffers` site (`LLWindowWin32::swapBuffers`, llwindowwin32.cpp:3925-3929), and each tile's COLOR readback requires a completed `renderFinalize` with the key composite applied (serial + keyed flag + size); otherwise the snapshot is **refused** with a reason and a notification — never silently un-keyed | §4.4, §8, §11 |
| P0 generation stability ≠ stable enumeration | **Settle rule** in add-on and viewer: CLEAN may be cached only after a ≥ 200 ms quiet period since the last notification **and** two consecutive identical full scans with no notification across them; otherwise verdict 5 UNSETTLED (bracket still opens, viewer stays ARMING, amber "waiting for DLL loading to settle"); nothing incomplete is ever cached; without the notification mechanism, two scans every KEY/ARM present and no caching | §4.2 whitelist, reader states, ARMING, §4.1 1b |
| P1 snapshot exactness scope | Exact codes only for **COLOR**, **unfiltered**, **lossless** (PNG/BMP/lossless AVIF) saves at rendered size; DEPTH24 reads depth (llviewerwindow.cpp:5831), filters mutate pixels (llsnapshotlivepreview.cpp:968-974), JPEG/lossy/J2C/postcard and rescales change codes; the snapshot floater shows the scope note | §0, §1, §3.7a, §4.4, §9, §11 |

Also updated: cost text (two scans per re-validation, ≤ 1.2 ms/s at the unverified estimate's upper
end with no churn), tests (unit snapshot contract + settle rule; in-world 8c, 9k(d)), §13 WP2/WP4, §14
rewritten for Codex round 9.

## v9 changes (Codex R7 resolution)

Confirmed by R7 and not reopened: the begin/finish proof, the public `render_technique` event sequence,
the three-counter verdict, the 60/61 ARMING boundary, bracket-on-FOREIGN, failure refusal, provenance.

| Codex R7 item | v9 resolution | Where |
|---|---|---|
| P0 snapshot path bypasses the guard and the key composite | Premise checked: `sRenderDeferred` is always true (llappviewer.cpp:624) and `display()` resets its **local** `for_snapshot` to false under deferred (llviewerdisplay.cpp:457-462), so snapshot renders **do** run `render_ui()` → `renderFinalize()` → `gatherFrame()` (1154-1158, 1747-1751); `gDisplaySwapBuffers = false` (llviewerwindow.cpp:5782, 6020) means they are never presented, so ReShade never touches them. Specified: snapshots are **keyed exactly**; `keyGuardAllows()` is true on `gSnapshot` renders; they publish no KEY/ARM flag and leave all guard bookkeeping untouched; 360 (`simpleSnapshot`) is suppressed (it was wrongly described as `gCubeSnapshot`); tiled high-res seams documented | new §4.4, §1/§0 line, §3.7, §9 items 8-9, §11 |
| P0 generation scans can lose a concurrent notification | Capture-before-scan in add-on and viewer: read gen → scan → recheck; store only the pre-scan value; up to 3 attempts, then refuse "module set unstable". Viewer captures `mGenAtPublish` in `gatherFrame()` (before the present and the add-on's scan) | §4.2 whitelist, reader states |
| P1 late-ReShade wording | A ReShade loading after `renderFinalize`'s check with an enabled effect can draw on that keyed present — stated as covered by the D30 phrase; the next frame is not keyed | §4.2 reader states, 9k(b) |
| P1 loader-lock increment | Both callbacks use `InterlockedIncrement` on a `volatile LONG` | §4.2 |
| P1 missing AddonPath is normal | No key → base path; missing directory → empty set; only truncation, unformable paths or other `FindFirstFileW` errors refuse | §4.2 item 2, §11 |
| P1 remaining unconditional exactness claims | §3.7a retitled "viewer-framebuffer exactness" with the D30 scope; tests labelled **[viewer]** / **[no ReShade]** / **[ReShade]**; OBS test split | §0, §3.7a, §11 |
| P2 counters / wording | `effect_calls` = saturating `begins + finishes`; "techniques exist but none is enabled" | §4.1, §4.2, §11 |
| P2 cost | 50-300 µs labelled unverified; ≤ 0.6 ms/s limited to no module churn | §4.2 |

Also updated: §8 files, §13 WP2/WP4, §14 rewritten for Codex round 8.

## v8 changes (Codex R6 resolution + user decision D30)

Closed by R6 and not reopened: stale-bracket restore/dead/SKIPPED outside the lock; conservative-refusal
disclosure; the unconditional ReShade-UI warning; reuse of the depth pin's hash check; the two 6.8.0
built-ins (Generic Depth, Effect Runtime Sync).

**User decision (2026-10-05, D30): accept the soft path's one-frame limit and state it plainly.** The
single guarantee statement, used verbatim in the UI, §1, §4.2 and the tests:
*"Key colour is exact except where ReShade's own UI draws, and an add-on effect switched on mid-key may
tint ONE frame before keying stops."* (Without ReShade: "Key colour is exact.")

| Codex R6 item | v8 resolution | Where |
|---|---|---|
| P0 a CLEAN report for N-1 cannot prove N clean | Accepted limit (D30); guarantee statement everywhere; contradictory "exact" wording removed (§0 note, §4.2 "why" paragraph retitled, §4.2b, §6, D29, tests 9i/9j) | §1, §4.1, §4.2, §6, §11, §12 |
| P0 late ReShade load fails open (NO_RESHADE scanned only at key-on) | Viewer `LdrRegisterDllNotification` generation counter; before **every** keyed `renderFinalize`: gen unchanged → NO_RESHADE still valid; changed → ReShade re-test first; found or test failure → that frame not keyed (fail closed). No notification → re-test every keyed frame | §4.2 reader states |
| P0 whitelist not maintained during KEYING | Add-on rescans on any module load/unload (own notification counter) and at 2 Hz, on every KEY/ARM present; enumeration FALSE, truncation, path-query failure, unexpected FindFirstFile error → FOREIGN "check failed" → refuse; bracket still opens on FOREIGN. Viewer also un-keys a frame when the module generation moved since the last CLEAN report. Cost stated: steady state = one atomic load + clock read; full scan est. 50-300 µs on load events and ≤ 2 Hz (Tracy zones) | §4.2 whitelist |
| P1 begin/finish proof | Corrected: automatic `render_effects` is gated by the Effects state; the public `effect_runtime::render_technique` (6.8.0) raises begin/render/finish regardless — still counted; the v7 "no add-on begin/finish callback ran" claim withdrawn | §4.2 counters |
| P1 counters conflate events | Three separate counters (passes, begins, finishes); report `passes` + `effect_calls`; status "effect activity: <p> pass(es), <c> begin/finish call(s)" | §4.1 1b/3, §4.2 |
| P1 timeout boundary | `mArmPresents = 0` on ARMING entry; per evaluation: (1) qualifying report wins, (2) `mArmPresents ≥ 60` → REFUSED, (3) else publish ARM and `++`; exactly 60 ARM presents, the next evaluation is the first REFUSED frame; tests aligned | §4.2, §11 |
| Provenance wording | Local `I:\reshade-SL` = `v6.7.3-2-gd13926f9`; no local 6.8.0 checkout; 6.8 semantics cited to upstream GitHub tag v6.8.0 sources; local line numbers labelled "6.7" | §4.2, §4.2a |

Also updated: §8, §12 (D27 wording, D30), §13 (WP2, WP4), §14 rewritten for Codex round 7.

## v7 changes (Codex R5 resolution)

Confirmed by R5 and not reopened: upstream 6.8.0 marks `.addonfx` as `effect.addon`, suppresses
ordinary effects with Effects off and raises `reshade_render_technique` after every ReShade-rendered
technique; one ARMING frame; the `SLReShadeReaderInfo`/`SLReShadeKeyGuard` sizes; the bracket lock and
SEH recovery; v1 does not touch `draw_gui` (input intact); all earlier viewer-side specs; nothing in v1
depends on §4.2a.

| Codex R5 finding | v7 resolution | Where |
|---|---|---|
| P0-1 unobserved render paths: add-on GPU work in `reshade_begin_effects`/`finish_effects`/`reshade_overlay` (and other events), plus ReShade's own preview/OSD/splash in `draw_gui()`, emit no technique event (upstream runtime.cpp:3938, runtime_gui.cpp:785-787, 1337-1353) | **Choice: strict whitelist + split guarantee (D28, D29).** (1) No add-on enumeration API exists in 6.8.0 (verified), so the add-on proves the loaded set itself: exact host binary (the depth pin's 6.8.0.2155 hash ⇒ built-ins = Generic Depth + Effect Runtime Sync), add-on folder contains only our `.addon`, no other process module with `.addon*` extension or add-on exports (`AddonInit`/`AddonUninit`/`NAME`/`DESCRIPTION`); any failure → FOREIGN_ADDON → refuse. (2) begin/finish-effects events are counted in the bracket: with Effects off and no `.addonfx` they cannot fire (3701 precedes 3909/3949), so a CLEAN bracket proves no add-on's begin/finish callback ran. (3) ReShade's own GUI cannot be suppressed in v1: an **unconditional** amber warning while keying, named causes where observable (overlay, ini OSD, tutorial/reload splash), and the UI guarantee is worded "exact except where ReShade's own UI draws". Justification: invisible GPU work is global and silent → must refuse; ReShade GUI is visible, local and user-controlled → warn honestly, like the viewer's own UI | §4.1, §4.2 |
| P0-2 missing reports fail open in KEYING | **Heartbeat**: frame N keys only with a CLEAN report for the previous bracketed frame (delivered in that frame's `SwapBuffers`, same thread, before N's `renderFinalize`); otherwise N is un-keyed and the state drops to ARMING (→ REFUSED after 60) | §4.2 |
| P0-3 old/non-guard reader keys unprotected | Four reader states: NO_RESHADE (no ReShade module in process → key freely), RESHADE_NO_READER (refuse), READER_NO_GUARD (refuse), READER_GUARD (ARMING/KEYING) | §4.2, §4.1 |
| P1-4 stale open bracket at raw present | Invariant failure: restore, close, `g.dead = true`, report SKIPPED outside the lock → viewer REFUSED | §4.2 bracket |
| P1-5 false refusals (techniques may target off-screen resources, runtime_api.cpp:1149-1205) | No RTV/dependency filtering; documented conservative refusal; status text now says "effect pass(es) with Effects off", not "tinting" | §4.2, §4.1, D27 |
| P1-6 overlay warning incomplete | Warning unconditional while ReShade is loaded and keying; `gui_flags` names overlay-open, ini OSD, tutorial splash, reload splash; preview covered by the unconditional line; other add-ons' `reshade_overlay` callbacks excluded by the whitelist | §4.2, §4.1 |

`SLReShadeKeyGuard` keeps its 32-byte layout: `overlay_open` → `gui_flags`, `reserved[0]` →
`foreign_addons`; verdict 4 FOREIGN_ADDON added. Also updated: §6 status, §8, §11 (unit + 9i/9j),
§12 (D27 reworded, D28, D29), §13 (WP2, WP4), §14 rewritten for Codex round 6.

## v6 changes (Codex R4 resolution)

Confirmed by R4 and not reopened: R3 rain ownership (BOUND/OWN draw modes), the RAII
`LLShadowCascadeScope` lifetime, full GL teardown release, unique-match fail-closed, rigged AlphaCap
claim, and the OpenGL ordering (raw `present` and `present_effect_runtime` are consecutive,
opengl_hooks_wgl.cpp:272-276; a per-runtime one-shot request is correct **if** it is consumed at the
absolute entry of `on_present` and written only on the presenting thread).

| Codex R4 finding | v6 resolution | Where |
|---|---|---|
| P0-1 skipping `draw_gui()` also skips the `_ignore_shortcuts` reset and the input-block updates (reshade-SL runtime_gui.cpp:819, 829-835, 1482-1497) → an overlay-open frame can leave viewer input blocked for the whole keyed period | The fork path never skips `draw_gui()`. ImGui `NewFrame`/`Render`, the shortcut reset, `_block_input_next_frame` consumption (runtime.cpp:662) and the input-block code all run every present; **only the draw-data submission** (runtime_gui.cpp:1499-1517) is suppressed, ImGui receives a neutral input frame, the active item is cleared on the first hidden present, and input capture is **explicitly released** (`block_mouse_input(false)`, `block_keyboard_input(false)`, `block_mouse_cursor_warping(false)`) while hidden | §4.2a |
| P0-2 `runtime_manager::find` does not exist (runtime_manager.hpp:10-18, runtime_manager.cpp:15-77); `runtime` is declared in runtime.hpp:29, not runtime_internal.hpp | **Swapchain-based, validated API.** The add-on passes the raw `present` event's own `swapchain*`; the fork accepts it only if it equals a `thread_local` "swapchain currently presenting on this thread" that the wgl hook sets around the raw `present` + `present_effect_runtime` pair, then resolves the runtime exactly as ReShade does (`swapchain->get_private_data<reshade::runtime>()`, runtime_manager.cpp:74-78). No registry, no lock, no stale pointer can pass. The request is stored through a new public `runtime` method (runtime.hpp) | §4.2a |
| P0-3 exports belong on the API-export path (`RESHADE_API_LIBRARY_DECL`, include/reshade.hpp:15-48; implementations in source/addon.cpp:6-15), not dll_main.cpp:16 | Declared beside `ReShadeGetBasePath` in include/reshade.hpp, implemented in source/addon.cpp inside its existing `#if defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON` guard; the add-on resolves them with `GetProcAddress(get_reshade_module_handle(), …)` like `ReShadeLogMessage` (reshade.hpp:137-139) | §4.2a |
| P0-4 atomics do not make a concurrent `std::map` traversal safe; the lock-free SEH walk is UB | Lock-free walk **removed**. The bracket registry is a fixed-capacity POD slot array (8 slots, never reallocated, no allocator in recovery) and **every** access, including recovery, holds `g_bracket_lock`. Recovery takes the lock with a blocking `AcquireSRWLockExclusive`, which cannot self-deadlock because `__finally` has already released it on the faulting thread | §4.2 |
| P1-5 release provenance: deployed ReShade is 6.8.0.2155 (the depth pin's gate, sl_depth_pin.hpp:39-73), the fork tree is 6.7.3.3 (res/version.h:3-10) | Options (a) port the bypass onto exact 6.8.0, (b) migrate the fork to 6.8, (c) no fork DLL in v1: soft bracket + a runtime **key guard** that refuses to key while any technique renders with Effects off. **Decision D26 — DECIDED by the user 2026-10-05: (c) for v1; (a) is the v1.1 follow-up.** The fork design (§4.2a, with the P0-1..3 fixes) is kept as a v1.1 spec only | §4.2b, §12 |

**v1 scope (D26 decided):** no ReShade DLL change. v1 = soft bracket + runtime key guard (refuse with a red
status) on the stock 6.8.0.2155; the ReShade overlay is not hidden (amber note). Rows P0-1..P0-3 above
correct the **deferred v1.1** fork spec (§4.2a) and are not built or reviewed in v1; row P0-4 is v1.

Consequences carried through: §3.1 data flow, §4.1 statuses, §4.2 (soft bracket is now the v1 mechanism,
plus the key guard), §4.3 (no DLL deploy in v1), §6 status text, §7 (`ActorKeyHideReShadeOverlay`
deferred), §8 files, §11 tests (9, 9b, 9e, 9g, new 9i), §12 (D24 revised, D25 deferred, D26, D27),
§13 (v1 work packages only; the fork package listed as deferred v1.1), §14 rewritten for Codex round 5 (v1 scope only).

## v5 changes (Codex R3 resolution)

Closed by R3 and not reopened: per-owner eligibility, rigged/GLTF caster coverage, independent cap/caster
filters, reload inside the bracket, RLVa sphere, zero radius, the 128-byte reader ABI, the hash
manifest, the Home test; per-owner cost accepted.

| Codex finding | v5 resolution | Where |
|---|---|---|
| P0-1 `set_effects_state(false)` does not suppress effects flagged `effect.addon` (reshade-SL runtime.cpp:3701, 3716, 3923) | **Option (a): fork hard bypass.** New per-runtime present-bypass mask consumed in `runtime::on_present`, exported as `ReShadeSL_SetPresentBypass`; skips ALL effect rendering (addon-flagged included) and, by option, the overlay, for one present; nothing to restore. Soft bracket kept only as a red-flagged fallback for a DLL without the export. Fact check: nothing in the fork tree ever sets `effect.addon = true` (declared runtime_internal.hpp:214, read at 3701/3716/3923, no assignment in `source/**/*.cpp`/`*.hpp`), and the user's setup registers no add-on effects (our bridge registers none; `SL_GBufferProvider.fx` and iMMERSE live in the ordinary shader folder) | §4.2a, §4.3 |
| P0-2 rain caller binds/clears before `renderShadow`, reads pixels, flushes after (pipeline.cpp:26223, 26246, 26292, 26321) | `draw()` has two explicit modes: **bound-target** (caller owns bind/clear/flush; used by the `renderShadow` wrapper → rain, spot, aux, and by the beauty cascade) and **target-owning** (actor/set draws only). No existing caller changes | §5.2 |
| P0-3 cull / depth-clamp / depth-test guards must outlive every draw; each draw needs unwind-safe flush and matrix-pop | The three `LLGLState` guards are **members** of `LLShadowCascadeScope`; `draw()` wraps matrix push/pop and (owning mode) bind/flush in local RAII helpers so any early return unwinds them | §5.2 |
| P0-4 SEH recovery can deadlock on a held bracket mutex; `open` set after the mutation | `SRWLOCK` with C-API acquire/release inside `__try/__finally`; `open = true` written **before** the disabling call; `__except` recovery uses `TryAcquire` + lock-free per-runtime atomics. With the hard bypass there is nothing to restore at all | §4.2 |
| P1-5 full GL teardown (`releaseGLBuffers` → `releaseShadowBuffers`, 2344-2346, 2367-2385) never calls `releaseSunShadowTargets` | `releaseActorShadowTargets()` called from `releaseShadowBuffers`, `releaseSunShadowTargets`, `resizeShadowTexture`, the OFF transition and the ensure rollback | §5.2 |
| P1-6 device+HWND is not an enforced unique key; `g_brackets[rt]` inserts | Raw present requires **exactly one** match (0 or >1 → log once, no mutation); `reshade_present`/destroy use `find` | §4.2 |
| P2-7 `pushMaskBatchesIndexed(..., rigged=true)` already evaluates `skip_shadow_draw` (lldrawpool.cpp:1255-1289) | Claim corrected: that rigged sweep keeps today's cap test; the caster predicate is added beside it. The cap is added to no loop that lacks it today | §5.2 |

## v4 changes (Codex R2 resolution)

Closed by R2 and not reopened: exact colour (§3.7/3.7a), the light/mask API split (§3.3), forward
masking, the 7371 gate, material-coverage spec (§3.5), `clampHDRRange` wording, raw-present ordering.

| Codex finding | v4 resolution | Where |
|---|---|---|
| P0-1 shared KEY bit breaks eligibility: `coverageChanged()` accumulates one global byte (llactormaskeligibility.cpp:33-40) and the mask pass tests it per recipient bit (pipeline.cpp:23778-23801) → one styled keyed avatar marks every keyed avatar styled | Suppression/coverage tracked **per owner UUID** (`mCoveredOwners`), the mask pass derives `native_styled` and the per-draw byte from owner sets, never from `coverage_bits & KEY_BIT` | §3.3 |
| P0-2 shadow split has no state-lifetime contract (today's restore of `sUseOcclusion`/`sShadowRender`/AlphaCap is at function exit, 26379-26380, 26682-26685) | RAII `LLShadowCascadeScope`: ctor = prepare, `draw()` member, dtor = today's exit restores; the three draws live inside one scope; every exit restores | §5.2 |
| P0-3 filters miss the rigged render-map paths (`pushRiggedBatches`, `pushUntexturedRiggedBatches`, `pushRiggedMaskBatches`, rigged GLTF scalar/indexed loops, lldrawpool.cpp:1154-1200, 1225-1252, 2473-2510, 2533-2579 — none call `skip_shadow_draw`) | Caster-ownership predicate evaluated in every rigged loop before `uploadMatrixPalette`; the AlphaCap predicate is deliberately NOT added there (today's behaviour for 0-3 preserved; latent gap flagged) | §5.2 |
| P0-4 one global runtime vs one runtime per swapchain (reshade-SL runtime_manager.cpp:15-47, 74-77; bridge.cpp:239-242, 843-850) | Bracket state **per runtime**; raw present maps the presenting swapchain to its runtime by device + hwnd exactly as `depth_pin::present` already does (sl_depth_pin.hpp:224-235); only that runtime is mutated | §4.2 |
| P1-5 reload is not "between frames": raw present precedes `update_effects()` which raises `reshade_reloaded_effects` (opengl_hooks_wgl.cpp:268-276; runtime.cpp:624, 3354, 3369, 3688) | Asserts dropped; reload leaves the open bracket untouched; restore still happens at `reshade_present`/destroy | §4.2 |
| P1-6 SET_ONLY vs AlphaCap nesting (26535-26537, 26612-26614) | `sShadowDrawFilter` (cap) and new `sShadowCasterFilter` (ownership) are **independent statics**; every draw loop evaluates both; the cap scopes are untouched | §5.2 |
| P1-7 RLVa sphere is a post-matte spatial filter (22334-22340; rlvF.glsl:129-145) | `key_now` also requires the sphere pass not to have run this frame | §3.7, §4 |
| P1-8 shadow targets need an ON-transition allocation path (`allocateShadowBuffer` runs only on resize, 1181-1192) | Lazy `ensureActorShadowTargets()` before first use, shared allocator with the beauty loop, rollback to All-shadows on partial failure; released on OFF and on `resizeShadowTexture` | §5.2 |
| P1-9 feather/choke zero divides by r | Explicit zero-radius branch (`r < 0.5 px` → `m = coverage`), r ≥ 0.5 otherwise | §3.6 |
| P1-10 reader-report ABI inconsistent | One fixed-layout `SLReShadeReaderInfo` (128 bytes, static_assert), `void SLReShade_ReportReader(const SLReShadeReaderInfo*)`, string ownership and null-export behaviour defined | §4.1 |
| P2-11 hash manifest unspecified | Manifest: sorted ordinal UTF-8 forward-slash relative paths, extensions, exclusions, BOM/CRLF normalisation, one shared CMake module; the header hashed is `indra/newview/llreshadebridgeabi.h`, which the add-on includes directly (bridge.cpp:71) | §4.3 |
| P2-12 Home-key test: overlay draws before shortcuts are processed (runtime.cpp:652-677) | Test expectation rewritten: the restore is observable only as state, the checkbox stays unchecked on screen | §11 |

## v3 changes (Codex R1 resolution)

| Codex finding | v3 resolution | Where |
|---|---|---|
| P0-1 `reshade_present` runs after effects and overlay (reshade-SL runtime.cpp:628-659, 830) → bypass one frame late | Bypass moves to the raw `addon_event::present`, which precedes `present_effect_runtime`; `reshade_present` is now the *restore* point | §4.2 |
| P0-2 never leave effects globally disabled; `set_effects_state` only flips `_effects_enabled` (runtime_api.cpp:1316-1323) | Per-present **bracket**: save+disable in raw present, restore in `reshade_present`; restore also in `destroy_effect_runtime` (runtime still alive, runtime.cpp:580-582) and in the SEH `__except` path. Persistent ACTIVE state and drop-on-destroy removed | §4.2 |
| P0-3 `renderShadow` re-entry duplicates maps/alpha groups (`stateSort` appends via `pushDrawInfo`, pipeline.cpp:5336; `LLCullResult` not cleared) | `renderShadow` split into `prepareShadowCascade` (cull+sort, once) and `drawShadowCascade` (draw-only, re-runnable); beauty, actor and set maps drawn from ONE immutable sorted result. Re-entry implementation dropped | §5.2 |
| P0-4 exact colour unproven: no explicit `GL_FRAMEBUFFER_SRGB` disable at the blit (22436-22559) | `LLGLDisable srgb(GL_FRAMEBUFFER_SRGB)` in the blit block, invariant-checked; 10-bit acceptance = exact 10-bit code AND deterministic 8-bit readback; "byte-exact 10-bit" wording dropped | §3.7, §3.7a |
| P0-5 `recipientBit`/`byteForOwner` gated by `sAnyActorOnly` (949-976); key-only rendering cannot reuse them | Two byte spaces: **light bits** (existing API, unchanged, never contain bit 7) and **mask bytes** (new `mask*` accessors gated on `sAnyMaskRecipient`). Bit 7 never enters `actorOnlyBits`, `liveRecipientBits`, light-set keys, diagnostics or overflow accounting | §3.3 |
| P0-6 unknown bytes fall back to world (11010-11016) → `rigBit\|KEY_BIT` would lose rig lighting | Forward lookups receive light bits only (by construction) and additionally mask `& ~KEY_BIT` + assert at entry | §3.3 |
| P1-7 actor/set targets need identical per-cascade size, compare mode/func, address mode, dynamic filtering (1903, 1954-1976) | Allocated beside `mRT->shadow[j]` with the same overrides; the sampler-state block and `applyShadowUnitsFiltering` iterate the extra targets | §5.2 |
| P1-8 matte binder must upload the complete sun-shadow state (probe mode 22784, meta, res, sun/moon dir, soft controls, SUN_UP_FACTOR 23188-23221) | `bindSunShadowState()` extracted from `bindDeferredShader` and used by BOTH the deferred shader and the matte program; parity is structural | §5.3 |
| P1-9 filters 4/5 in every path; `renderAlphaObjects` treats ≥2 as attachment-only (11926-11975) | Every shadow draw loop names all values 0-5 explicitly; consumer list enumerated | §5.2 |
| P1-10 coverage shaders must mirror scalar/rigged GLTF base-colour factor, texture alpha, vertex alpha, indexed materials; PASS_ALPHA identity confirmed (5324-5338, lldrawpoolalpha.cpp:1094-1099, llvovolume.cpp:7577-7586) | Spelled out per material family | §3.5 |
| P1-11 suspension must come from the actual `fisheye_warped`/`diopter_warped` predicates (22350-22391) | Key decision taken inside `renderFinalize` after those locals exist; settings alone never decide | §4 table, §3.7 |
| P1-12 teardown/fault tests | Added: reload, runtime re-init, add-on unload, missing frame, SEH probe | §11 |
| P2-13 `clampHDRRange` is an inf/NaN scrub (deferredUtil.glsl:349-358), not a clamp | Wording fixed; key injected after it as the last statement | §3.7 |
| P2-14 build stamps cannot detect a stale same-1.3 binary | Add-on reports the SHA-256 of `reshade-addon/src` + ABI header it was built from; the viewer embeds the expected hashes at configure; mismatch → red. Limit stated | §4.3 |
| P2-15 the key gate must replace the exact gate at 7370-7371; keep-live hook (4631-4651) is real | Gate text fixed; hook PROVES, extended to mask recipients | §3.2, §3.3 |
| DROP | re-entry implementation, unmeasured "<0.1 ms" claims, "byte-exact 10-bit" | throughout |
| KEEP deferred | projector shadows, SSAO/contact separation, keyed 360 capture, `SL_ACTOR_MATTE` | §5.1, §9 |

## v2 decisions (user, 2026-10-05) — closed; not reopened below

| # | Decision | Outcome |
|---|---|---|
| D1 | Default subject set | **All avatars** |
| D2 | Alpha-coverage pass in v1 | **Ship** (checkbox stays) |
| D3 | Edge fill | **Blend to key**; hard edge as an option; no despill |
| D4 | `ActorKeyEnabled` persistence | **Session-only** |
| D5 | Actor shadows in mode 1 | **Build shadow-keeping in v1.** §5 is now the full specification (source, isolation, strength, keyability, cost, fallback). The two-pass workflow stays documented as the exact alternative |
| D6 | ReShade | **Auto-bypass in v1, add-on side included.** §4.1 viewer flag, §4.2 add-on design, §4.3 handshake + build/deploy |
| D7 | Fisheye/diopter active | **Suspend the key with a UI reason** |
| D8 | Director link | **KEY toggle on the console** |
| D9 | Hotkey | **F9**, gated like F3-F8 |
| D10 | Kill switch coupling | **Independent** |
| D11 | Feather unit | **px @1080p** |
| D12 | Impostor policy | **Rely on the keep-live hook if llvoavatar.cpp:4643 is it; otherwise report only** (WP0 confirms) |
| D13 | `SL_ACTOR_MATTE` ReShade semantic | **Later** |
| D14 | Default preset | **Chroma Green #00B140** |
| D15 | Mid Grey preset | **Keep** |

What changed v1 → v2: §5 rewritten as a full specification and work package (WP3); §4.1 split into
viewer flag (§4.1), add-on design (§4.2) and handshake/deploy (§4.3); new `ActorKeyShadow*` settings;
§3.1 data flow, §7, §8, §10, §11 and §13 updated; §12 now lists only the items that stay open; §14 is
the Codex review list, extended with the shadow and add-on items.

Related: `doc/CINE_RIG_ACTOR_MASK_BRIEF_V3.md` (the mask this feature builds on),
`doc/CINE_RIG_ACTOR_MASK_UI_NOTE.md`, `doc/RESHADE_BRIDGE_CONTRACT.md`, `doc/DIRECTOR_HOTKEYS.md`.

---

## 0. Fixed by the user (not reopened)

- Three modes: **Actors on key** (avatars stay, set becomes the key colour), **Set only** (avatars become
  the key colour, set stays), **Alpha matte** (white actors on black, invertible).
- Key colour: presets for the common chroma colours (Chroma Green `#00B140`, Pure Green `#00FF00`,
  Chroma Blue `#0047BB`, Pure Blue `#0000FF`, Magenta `#FF00FF`, Pure Black, Pure White, Mid Grey) plus
  a custom picker; the preset value must reach the viewer's final framebuffer **exactly** (§3.7a); what
  ReShade may draw afterwards is bounded by the v1 guarantee statement (§1, §4.2).
- Soft edge: **feather** and **choke**.
- Output is the viewport itself, toggled on/off. OBS captures what the user sees (with ReShade loaded: bounded by the v1 guarantee statement, §1). Viewer snapshots are keyed by the viewer with every present blocked until the keyed readback completes, so ReShade is never involved; exact codes for unfiltered COLOR snapshots saved lossless at rendered size (§4.4, v10).

## 1. Summary

The Cine Light Rig Actor Mask is the right **substrate**: a full-resolution, depth-exact, per-avatar
silhouette already drawn with GL-state hygiene, probes and a debug view. It is **not** a drop-in matte:

| Finding (PROVES) | Where | Consequence for the key |
|---|---|---|
| Exists only while an Actor-only light is live (`anyActorOnly()`), released on the off transition | pipeline.cpp:23369-23371, 23936-23944 | Needs a second demand source: `actorMaskWanted() = anyActorOnly() \|\| key active` |
| Recipients come only from actor-only rigs (SELF/A-D/legacy anchor), max 8 bits | alcinelightrigmanager.cpp:990-1088; brief §5.1 | Key needs its own recipient list; crowds share one reserved **KEY bit** |
| Binary: each pixel is a bit byte, written with `discard`/overwrite, no coverage | actorMaskF.glsl:126, actorMaskAvatarF.glsl:14 | Softness must be synthesised: blur + remap, plus a real alpha-coverage pass |
| Only opaque + alpha-**masked** passes; **no `PASS_ALPHA`/`PASS_ALPHA_RIGGED`**; system body drawn with `sSkipTransparent = true` (no system hair/skirt/lashes) | pipeline.cpp:23527-23565, llactormover.cpp:15045 | Alpha-blended hair, sheer clothing, flexi/alpha attachments are **absent** from the mask: they would be cut out. A coverage pass is required |
| Valid only inside the lighting view (`ResetActorMaskView` zeroes `mActorMaskValid` on scope exit) | pipeline.cpp:23917-23926 | The matte must be finalised while the view is valid (the `endMainView` site), then frame-stamped for `renderFinalize()` |
| Impostored / muted / jellydolled / too-complex avatars and Actor-FX Replace/Cover ("STYLED") actors draw nothing | llactormover.cpp:15011-15030; brief §8.7 | Those avatars vanish into the key (mode 1) or stay un-keyed (mode 2). Must be reported in the UI |
| Target is `GL_R8`, same size as `deferredScreen`, shared depth, one per `RenderTargetPack` | pipeline.cpp:23372-23387, pipeline.h:1224 | Follows `RenderResolutionDivisor` and the high-res snapshot reallocation for free |

Recommended architecture in one paragraph: reuse the mask pass unchanged for opaque geometry; add a
**key recipient list** (reserved bit 7) to the registry so the pass runs with no actor-only light; add an
**alpha-coverage replay** of the actors' blended draws into a second R8; build a **soft matte** at the
end of the main lighting view (coverage → separable blur at half res → choke/feather remap) into a
frame-stamped `mActorKeyMatte` (RG8: R matte, G actor shadow); composite the key **inside
`blitWithEffectsF.glsl` after dither**, so the key colour is written as exact display code values and
nothing downstream in the viewer touches it. **Actor sun/moon shadows survive** as a hue-preserving
darkening of the key, from actor-only cascade depth maps rendered with the existing shadow-draw filter
(§5). **ReShade is bypassed** while keying: the viewer publishes `SLRESHADE_FLAG_ACTOR_KEY`, the bridge
add-on brackets each keyed present with `set_effects_state(false)` and restores it at `reshade_present`,
a **key guard** refuses to key while any effect pass still runs with Effects off, while any non-whitelisted
ReShade add-on is loaded, or when the bridge stops confirming frames (v7), and a version/source-hash handshake flags a stale add-on (§4.1-4.3; v6: no ReShade DLL
rebuild in v1, the fork hard bypass is the v1.1 follow-up, §4.2a-4.2b). UI and HUD draw after the blit and are never
keyed; HUD attachments are suppressed while keying. Off path: one static bool; no allocation, no draw,
no log line.

**v1 guarantee (the single statement; UI, tests and user guide use exactly this wording, §4.2):**
*"Key colour is exact except where ReShade's own UI draws, and an add-on effect switched on mid-key may
tint ONE frame before keying stops."* Without ReShade loaded the key colour is exact. Snapshots: exact codes for unfiltered COLOR captures saved lossless at rendered size (§4.4). The one-frame limit
is accepted by the user for v1 (D30); the v1.1 fork bypass (§4.2a) removes it.

---

## 2. The Actor Mask as read in the code

### 2.1 Target, format, resolution, lifetime
- `RenderTargetPack::actorMask` is `GL_R8`, allocated at `deferredScreen` size and sharing its depth
  (`ensureActorMask`, pipeline.cpp:23369-23406). One per pack: main, auxiliary, hero probe, probe
  capture, every Prism scratch (`releaseActorMasks`, 23351-23367).
- Allocated lazily inside `renderActorMask()`; released when `anyActorOnly()` turns false
  (23936-23944). `anyActorOnly()` is a static bool = kill switch off and the registry has lights
  (alcinelightrigmanager.cpp:1088).
- `bindActorMask()` binds it point-sampled under `LLShaderMgr::ACTOR_MASK_MAP` (23408-23413).

### 2.2 When it is written
`renderDeferredLighting(source)` classifies the view (23935-23969): MAIN with `mRT == &mMainRT`,
PRISM scratch, or CUBE only for the hero **mirror** pass. Not for impostors, HUDs, ordinary cube faces.
It then calls `renderActorMask()` **before** any lighting, with the G-buffer depth complete. The pass
depth-tests `GL_LEQUAL` with `glPolygonOffset(0, CineLightRigActorMaskBias)` (default -8) against the
opaque scene depth (23708-23715), colour-clears to 0, and overwrites bytes (no blending).

### 2.3 What counts as an actor
- Recipients: `ALCineLightRigManager::actorOnlyRecipients()` — avatars resolved from each actor-only
  rig's subject slots (brief §5.1; code 990-1088). Ghost clones qualify when they are cast subjects
  (`actorMaskAvatarEligible`, llactormover.cpp:15011-15030, admits `isGhostAvatar()` clones whose
  entity clone is visible). Ghost **proxies/overlays** never (pipeline.cpp:7089).
- Geometry per recipient: system body + eyes via `renderSystemActorMask` (opaque meshes only), then
  attachments / rigged mesh / worn animesh via `walkGhostSourceGeometry` over the include-list
  `kActorMaskPasses` (pipeline.cpp:23527-23565): simple, shiny, bump, material (+mask, +emissive),
  specmap/normmap/normspec (+mask, +emissive), alpha-mask, GLTF PBR (+alpha-mask), each with a rigged
  twin. **Excluded: `PASS_ALPHA`, `PASS_ALPHA_RIGGED`, fullbright, glow.**
- Alpha-masked draws replay the native cutoff and texture alpha (actorMaskF.glsl:117-127), so
  alpha-masked hair is in the mask; alpha-**blended** hair is not.
- Fail-closed verdicts per recipient: `OFFSCREEN`, `IMPOSTOR`, `STYLED` (23777-23779, 23868-23875).
- Eligibility recorder: the pass refuses to run without same-view native-submission evidence
  (`recorder.matches(actorMaskViewStamp())`, 23694-23700). The Scope is created in the G-buffer pass
  and is gated by exactly `if (ALCineLightRigManager::anyActorOnly()) actor_mask_scope.emplace(actorMaskViewStamp());`
  (pipeline.cpp:7370-7371, PROVES).

### 2.4 Soft or binary
Binary. Each fragment writes `bits/255` or discards. The debug view (`actorMaskDebugF.glsl`) decodes
bits per recipient; there is no coverage anywhere.

### 2.5 Verdict on suitability
Suitable as the opaque silhouette source. Three additions are unavoidable for a chroma key:
(1) a demand source independent of actor-only lights, (2) a recipient list independent of rigs,
(3) alpha-blended coverage for hair/cloth. Softness is a post-process on the mask (cheap). Nothing in
the existing pass needs to change its output; all additions are new code behind a new gate.

---

## 3. Architecture

### 3.1 Data flow (main view, key ON)

```
generateSunShadow() (main view), per cascade j:
  [key+shadow] ensureActorShadowTargets()  (lazy, rollback → All-shadows)                       [§5.2]
  LLShadowCascadeScope cascade(result[j])  ONCE (cull + stateSort; RAII restores on every exit)   [§5.2]
  cascade.draw(beauty)                                   ──► mRT->shadow[j]        (unchanged output)
  [key+shadow] { ShadowCasterFilterScope KEY_ONLY; cascade.draw() } ──► mActorShadow[j]   (same immutable sorted result)
  [exact]      { ShadowCasterFilterScope SET_ONLY; cascade.draw() } ──► mSetShadow[j]
G-buffer pass ─────────────┐ (eligibility Scope armed when actorMaskWanted(), replacing the gate at 7371)
renderDeferredLighting(MAIN):
  renderActorMask()  ──► mMainRT.actorMask  (R8 mask bytes; same pass, now reading the mask* accessors, §3.3)
  [NEW] renderActorKeyCoverage() ──► mActorKeyCoverage (R8, 0..1)
        = convert(actorMask & selection) MAX alpha-replay of PASS_ALPHA(+RIGGED) + system hair/skirt
  ... lighting, forward alpha, etc. (unchanged)
  [NEW, at the endMainView site, mask still valid] ALActorChromaKey::buildMatte():
        coverage ──► half-res blur H ──► half-res blur V ──► remap(choke, feather) ──► mActorKeyMatte.R (full res)
        [shadow, §5] sun maps (actor-only + full) ──► actor shadow term ──► mActorKeyMatte.G
        mActorKeyMatteFrame = gFrameCount
renderFinalize():
  ... tonemap, grade, AA, CAS, DoF, lens FX (unchanged); diopter_warped / fisheye_warped computed (22350-22391)
  key_now = matte valid this frame && !diopter_warped && !fisheye_warped && !rlv_sphere_ran
            && keyGuardAllows()                                                 [§3.7; P1-11; v6 key guard §4.2]
  (guard ARMING: frame renders un-keyed, gatherFrame publishes SLRESHADE_FLAG_ACTOR_KEY_ARM instead)
  final blit (LLGLDisable GL_FRAMEBUFFER_SRGB, explicit): key_now ? gBlitWithEffectsKeyProgram : gBlitWithEffectsProgram
        after vignette/CVD/grain/dither/preview/clampHDRRange: out = composite(mode, key colour, matte)   (last statement)
gatherFrame() (ReShade bridge): publishes SLRESHADE_FLAG_ACTOR_KEY iff key_now
render_hud_elements / render_ui_*: unchanged, never keyed; render_hud_attachments skipped when ActorKeyHideHUD
ReShade add-on, same present (v1, stock 6.8.0.2155 DLL):
  raw present → [KEY or ARM flag, unique runtime, whitelist ok, bridge alive] save+disable Effects →
  on_present: technique / begin / finish-effects events that still fire are counted → reshade_present:
  restore, report the verdict (SLReShade_ReportKeyGuard) → anything counted, foreign add-on, or no report
  for the previous bracketed frame ⇒ viewer does not key, red status   [§4.2]
  (v1.1, fork DLL only: raw present → ReShadeSLSetPresentBypass(swapchain, EFFECTS|OVERLAY) → consumed at
   on_present entry: no effects, overlay submission suppressed, nothing to restore   [§4.2a, deferred])
```

Key is applied only for `mRT == &mMainRT && !gCubeSnapshot && !sPrismLensRender`. Prism feeds, probes,
mirrors, impostors and 360 faces never see the key (see §9 for consequences).

### 3.2 Gate and demand
- New static `ALActorChromaKey::sActive` (cached from `ActorKeyEnabled` via a settings listener, the
  `sAnyActorOnly` pattern) and `LLPipeline::actorMaskWanted()` = `anyActorOnly() || sActive`.
- `actorMaskWanted()` replaces `anyActorOnly()` **only** at: `ensureActorMask` (23371), the
  classification/release gate (23930-23944), the eligibility Scope arm — the exact statement
  `if (ALCineLightRigManager::anyActorOnly()) actor_mask_scope.emplace(actorMaskViewStamp());` at
  pipeline.cpp:7371 becomes `if (actorMaskWanted()) ...` — and the registry rebuild trigger. Every
  light-side consumer (`actorOnlyBits`, forward actor sets, deferred actor light block, probe skipping,
  shadow tiers, diagnostics `onLightingView`) keeps `anyActorOnly()`. That is what keeps "key on, no
  actor-only lights" from entering any lighting branch.
- WP7 diagnostics stay gated on `anyActorOnly()`; the key gets its own 5 s summary line (`ACTORKEY`).

### 3.3 Key recipients: light bits vs mask bytes (Codex R1 P0-5, P0-6)
PROVES: `recipientBit()` and `byteForOwner()` return 0 whenever `!sAnyActorOnly`
(alcinelightrigmanager.cpp:951, 963), and `drawByte()` is their union (973-977). Key-only rendering
therefore cannot reuse them, and bit 7 must never leak into the light side. Two byte spaces:

**Light bits (existing API, frozen, unchanged).** `actorOnlyBits(light)`, `recipientBit(av)`,
`byteForOwner()`, `drawByte(di)`, `liveRecipientBits()`, `actorOnlyDrawBytes()`,
`actorOnlyRecipients()`, the RECIPIENT-OVERFLOW accounting and every WP7 diagnostic keep their
`sAnyActorOnly` gate and their rig-only content. Bit 7 can never appear in any of them because the rig
table never allocates it: rig recipients use bits 0..6 (the brief caps distinct rig recipients at 6, so
no real headroom is lost; a 7th rig recipient now overflows instead of an 8th — stated in the brief as
fail-closed behaviour and logged).

**Mask bytes (new, superset).** In `ALCineLightRigManager`:
```
static bool anyMaskRecipient();                 // sAnyMaskRecipient = sAnyActorOnly || ALActorChromaKey::sActive && !keyList.empty()
const std::vector<MaskRecipient>& maskRecipients() const;   // rig recipients (own bit, + KEY_BIT if also keyed) ∪ key-only avatars (KEY_BIT)
U8 maskRecipientBit(const LLVOAvatar*) const;   // rigBit | (keyed ? KEY_BIT : 0); gated on anyMaskRecipient()
U8 maskByteForOwner(const LLUUID&) const;       // same union over mFxOwner
U8 maskDrawByte(const LLDrawInfo&) const;       // maskByteForOwner(owner) | maskByteForOwner(fallback)
U8 keyMaskBits() const;                         // KEY_BIT | own bits of rig recipients that are also keyed
```
`KEY_BIT = 0x80` lives in `ALCineLightRigManagerModel` beside `ACTOR_ONLY_ORPHAN_BIT`. The rebuild
builds the light table exactly as today (`sAnyActorOnly = !mLights.empty()`, 1088), then the key list
from `ActorKeySubjects` (§3.4), then the mask table as the union. Unit-tested in the pure model.

**Who reads which.**
- Mask pass (`renderActorMask`, 23764-23821: `registry.actorOnlyRecipients()`, `registry.drawByte(*di)`,
  `registry.recipientBit(av)`), `renderSystemActorMask`'s byte and the coverage pass (§3.5) switch to
  the `mask*` accessors. Output of the mask pass with the key off and lights on is identical to today
  (the mask table equals the rig table).
- **Eligibility per owner, not per bit (Codex R2 P0-1).** PROVES: `coverageChanged()` ORs one global
  `mCoverageBits` (llactormaskeligibility.cpp:33-40) and the pass tests `coverage_bits & recipient.mBit`
  for `native_styled` and `owner_byte & ~coverage_bits` per draw (pipeline.cpp:23778-23801). With a shared
  KEY_BIT, one Actor-FX-styled keyed avatar would set bit 7 and mark **every** keyed avatar STYLED (no
  mask draws → all vanish into the key). v4: the recorder additionally keeps
  `std::set<LLUUID> mCoveredOwners` — `coverageChanged(owner)` inserts `owner`; `coverageChanged(draw)`
  inserts `draw.mActorFxOwner` and `draw.mActorFxFallbackOwner` when non-null (the `mBodies`/`mDraws`
  sets already work this way, llactormaskeligibility.h:30-34). The mask pass then computes, for every
  recipient (rig or key): `native_styled = recorder.covered(recipient.mFxOwner)`, and per draw
  `byte = (covered(di.mActorFxOwner) ? 0 : maskByteForOwner(di.mActorFxOwner)) |
          (covered(di.mActorFxFallbackOwner) ? 0 : maskByteForOwner(di.mActorFxFallbackOwner))`
  (`maskDrawByteUncovered(di, recorder)`). For rig recipients this is provably equivalent to today's bit
  arithmetic because rig bits are unique per recipient (reviewer proves; §11 tests it); for key
  recipients it is the only correct form. `coverageBits()` stays for the WP7 summary line only and is
  never consulted for a draw decision. `LLActorMaskQueryTag.styled` is set from the per-owner result.
- Deferred actor-light gate shaders test `b & actor_mask_bits` with `actor_mask_bits = actorOnlyBits(light)`
  (light bits) — bit 7 in the mask byte is invisible to them.
- Forward sets: `setupHWLights`' set builder and the `selectForwardLightSet` callers in
  lldrawpoolalpha.cpp / lldrawpoolavatar.cpp pass light-side `drawByte`/`recipientBit` → never contain
  KEY_BIT by construction. Defence in depth, because an unknown byte silently falls back to world
  (pipeline.cpp:11010-11016) and `rigBit|KEY_BIT` would therefore strip legitimate rig lighting:
  `selectForwardLightSet(U8 byte)` begins `llassert(!(byte & KEY_BIT)); byte &= ~KEY_BIT;`, and the set
  builder masks its candidate values the same way. Reviewer greps every call site.
- `LLActorMaskQueries` tags stay (recipient UUID, bit); recipients sharing bit 7 are distinguished by UUID.
  Key-only recipients are tagged but excluded from the WP7 verdict tables (they are not light recipients);
  they get their own `ACTORKEY` summary.
- Keep-live hook, PROVES: llvoavatar.cpp:4645-4647 keeps `isActorOnlyRecipient()` avatars at
  `mUpdatePeriod = 1` (never impostored) unless visually muted. v3 adds `|| (anyMaskRecipient() &&
  isMaskRecipient(this))` with the same `!isVisuallyMuted()` guard. Consequence: "All avatars" keying
  keeps every visible avatar live (no impostors) — intended for a keyed shot; the UI says so (§6, D12).

### 3.4 Which actors are keyed — `ActorKeySubjects`
| Value | Set | Source |
|---|---|---|
| 0 | **All avatars** (default) | `LLCharacter::sInstances` → `LLVOAvatar`, dead/control excluded; same walk as `collectRotoInkCandidates` case 2 (pipeline.cpp:16565-16576) |
| 1 | Director cast | `LLDirectorCast::getIds()` → `resolve()` (case 1, 16556-16563) |
| 2 | Actor-only lit avatars | existing `actorOnlyRecipients()`; no key recipients added |
| 3 | Me only | `gAgentAvatarp` |

Modifiers: `ActorKeyIncludeClones` (bool, default **true**; Ghost clones are `LLVOAvatar`s and pass
`actorMaskAvatarEligible`), `ActorKeyIncludeAnimesh` (bool, default **false**; adds visible
`LLControlAvatar`s as recipients — their body is skipped by `renderSystemActorMask`, but their mesh is
reached through the attachment walk's control-avatar branch, brief §5.4.2 item 3). Recommendation:
ship 0 as default (the mental model is "people vs set"); Director cast is the one for crowd scenes.

### 3.5 Alpha coverage pass (`renderActorKeyCoverage`)
Runs right after `renderActorMask()` in the MAIN MASK view, same depth, same saved-state wrappers
(`ScopedActorMaskState`, `LLScopedActorMaskInvariant("key coverage")`).
1. Bind `mActorKeyCoverage` (R8, full res, shares `deferredScreen` depth like the mask). Clear 0.
2. **Convert**: full-screen pass reading `actorMask` (point): `cov = (bits & keyMaskBits()) != 0 ? 1 : 0`
   (§3.3). No depth.
3. **Alpha replay** with `glBlendEquation(GL_MAX)`, depth test `GL_LEQUAL`, depth write off, same
   polygon offset: for each mask recipient with a key bit, `walkGhostSourceGeometry` over `PASS_ALPHA`
   and `PASS_ALPHA_RIGGED` draw infos with `maskDrawByte(di) & keyMaskBits()`. PASS_ALPHA draw-info
   identity between the spatial groups and the alpha pool is confirmed viable (Codex R1: pipeline.cpp:
   5324-5338 pushes the group's own `LLDrawInfo*`; lldrawpoolalpha.cpp:1094-1099 and llvovolume.cpp:
   7577-7586 consume the same objects). Coverage programs (static, rigged, GLTF, GLTF rigged) mirror
   the alpha pool's **alpha inputs exactly, per material family**, not its lighting:
   - legacy/Blinn-Phong (`PASS_ALPHA`, non-GLTF): `texture(diffuseMap).a × vertex_color.a`, with the
     material's diffuse colour alpha where `LLDrawPoolAlpha` applies it, and the indexed-material slot
     selection of `actorMaskF.glsl` (`mask_textures[slot]`, `mask_material_slots`) for multi-slot draws;
   - GLTF PBR alpha-blend (scalar and rigged): `baseColorFactor.a × baseColorTexture.a × vertex_color.a`,
     using the same `pbr_slots`/`pushRiggedGLTFBatch` palette path the mask pass already handles
     (`drawActorMaskBatch`, pipeline.cpp:23576-23661);
   - system avatar transparent meshes (hair, skirt, lashes): `renderSystemActorMask`-style replay with
     `sSkipOpaque = true, sSkipTransparent = false` and `texture(diffuseMap).a` (the avatar alpha
     program's input).
   The reviewer diffs each coverage shader's alpha expression against the corresponding alpha-pool
   shader (`alphaF.glsl`, `pbralphaF.glsl`, `avatarAlphaF`) line by line.
   Blend MAX means hair over body stays 1; hair over the set contributes its alpha.
4. Verdict: `ACTORKEY COVERAGE <draws>/<skipped>` in the 5 s summary; a `MASK_FAILED` analogue drops the
   frame's key to **opaque-only** (not off) and logs `ACTORKEY COVERAGE-FAIL` once.

`ActorKeyAlphaCoverage` (bool, default true) skips step 3 entirely for users who want the cheapest path.

Why not change the mask format to RG8 and write coverage in G: the gate shaders `texelFetch(...).r`
would still work, but the mask pass is frozen, reviewed code shared with lighting; a separate R8 keeps
rule 1 (off path and beauty pass provably unchanged) trivially true.

### 3.6 Matte build (`ALActorChromaKey::buildMatte`)
Called from the `endMainView` site (pipeline.cpp:25206-25209; screen still bound, mask valid), under
the same guards as the debug view, **before** the debug view draw so the debug overlay can also show the
matte (debug mode 3 "Key matte").
0. **Radius and the zero branch (Codex R2 P1-9).** `scale = H_target / 1080`;
   `r_px = max(feather_px, |choke_px|) × scale` (output-pixel radius). **If `r_px < 0.5`: no blur, no
   remap — `m = coverage_fullres` (the raw coverage, 0/1 for opaque, alpha for hair) and steps 1-3 are
   skipped.** Otherwise `r = max(r_px, 0.5)` so every division below has `r ≥ 0.5`. `feather = 0` with
   `choke ≠ 0` therefore erodes/dilates through the blur with a hard threshold (s = 1/255);
   `feather = choke = 0` is the exact raw coverage.
1. Downsample coverage to half res `mActorKeyBlurA` (R8) with a 2×2 box (bilinear fetch at texel
   centres). Skipped when `r_px < 1.5`: the blur and remap run at full res directly.
2. Separable Gaussian, radius `r` in the working resolution's pixels, clamp 1..16 taps per side
   (`mActorKeyBlurA` → `B` → `A`). One program, direction/radius uniforms.
3. Remap to `mActorKeyMatte.R` (RG8 full res — G is the actor shadow term of §5.3; bilinear upsample of the blur):
   ```
   c  = (choke_px × scale) / (2 r)                 // >0 shrinks, <0 grows, in blur-radius units; |c| ≤ 0.5
   s  = max((feather_px × scale) / (2 r), 1.0/255.0)  // half-width of the ramp; 1/255 = hard step
   lo = clamp(0.5 + c - s, 0, 1), hi = clamp(0.5 + c + s, 0, 1)
   m  = smoothstep(lo, hi, blur)
   m  = max(m, step(0.999, coverage_fullres) * (choke <= 0 ? 1 : 0))   // interior stays exactly 1 unless choking
   ```
   With a binary source this yields an anti-aliased edge of width ≈ feather; with the coverage pass,
   hair alpha passes through as true partial values. Exactly 0 and exactly 1 are reachable (needed for
   exact key colour in the interior/exterior).
4. `mActorKeyMatteFrame = gFrameCount; mActorKeyMatteRT = &mMainRT`. `renderFinalize` uses the matte
   only when both match the current frame and target; otherwise the frame renders un-keyed and
   `ACTORKEY MATTE-STALE` is counted (the UI status shows "not keyed this frame" if it persists).

Cost: two half-res separable passes plus one full-res remap; measured by the Tracy zone
`ActorKey::buildMatte`, not budgeted here.

### 3.7 Composite in the final blit
`blitWithEffectsF.glsl` (class1/alchemy) currently ends `vignette → CVD → grain → dither → preview →
clampHDRRange → frag_color` (lines 80-91). `clampHDRRange` is **not a clamp**: it replaces infinities
with 1 and NaNs with 0 (deferredUtil.glsl:349-358) and passes finite values through untouched. Add,
under `#ifdef ACTOR_KEY`, **after** `clampHDRRange` as the last statements before `frag_color = diff;`,
so nothing at all follows the injection:
```
float m = texture(actorKeyMatteMap, vary_fragcoord.xy).r;          // 0 set, 1 actor
vec3  key = actor_key_color;                                      // exact display code values
int   mode = int(actor_key_params.x);  bool invert = actor_key_params.y > 0.5; int edge = int(actor_key_params.z);
if (mode == 2)       { float v = invert ? 1.0 - m : m; diff.rgb = vec3(v); }
else {
  if (mode == 1) m = 1.0 - m;                                      // set-only: actors become key
  if (edge == 1) m = step(0.5, m);                                 // hard edge option
  diff.rgb = (m >= 1.0) ? diff.rgb : (m <= 0.0) ? key : mix(key, diff.rgb, m);
}
```
Key pixels are written as the literal uniform values after every other stage in the shader.

**Framebuffer state made explicit (P0-4).** PROVES: the final blit block (pipeline.cpp:22436-22559)
has no `GL_FRAMEBUFFER_SRGB` statement; it relies on the state it inherits. v3 adds, inside that block
before the draw, `LLGLDisable srgb(GL_FRAMEBUFFER_SRGB);` (the enum is already tracked by `LLGLState`:
`LLGLEnable framebuffer_srgb(GL_FRAMEBUFFER_SRGB)` at 6566), so the display-encoded key is never
re-encoded by the hardware. It is invariant-checked two ways: a debug-only
`llassert(!glIsEnabled(GL_FRAMEBUFFER_SRGB))` immediately before `drawArrays`, and the self-test
(§3.7a) reads the framebuffer after the blit and fails if the codes are off. The `LLGLDisable` is added
unconditionally (also with the key off) — it is a state the blit already assumed; the reviewer confirms
no caller depends on sRGB being enabled across the blit (none found: the three other uses are at 6571,
7704, 24223, all scoped).

**Program selection (P1-11).** The decision is made in `renderFinalize` *after* `diopter_warped` and
`fisheye_warped` are computed (22350-22391), using those exact locals — which already fold in
`gSnapshotNoPost`, zero blend, incomplete programs and the kaleidoscope tool mode:
`const bool key_now = ALActorChromaKey::matteValid(gFrameCount, mRT) && !diopter_warped && !fisheye_warped && !rlv_sphere_ran && ALActorChromaKey::keyGuardAllows();`
(v7: `keyGuardAllows()` is the ReShade key guard of §4.2 — true when no ReShade module is loaded
(NO_RESHADE), or in KEYING with a CLEAN report for the previous bracketed frame (heartbeat); false while
ARMING or REFUSED and for RESHADE_NO_READER / READER_NO_GUARD; v9/v10: always true on a `gSnapshot` render, during which every present is blocked, §4.4), where `rlv_sphere_ran` is a new local set to true inside the existing
`if (RlvActions::hasBehaviour(RLV_BHVR_SETSPHERE)) { ... runEffect(EVisualEffect::RlvSphere ...) }`
block (pipeline.cpp:22335-22340; Codex R2 P1-7: rlvF.glsl:129-145 blurs, chromatically shifts and
pixelates — a spatial filter after the matte was built). DoF and motion blur remain non-suspending
(documented misregistration, user-controlled, §4 table); the sphere is an externally forced effect, so
it suspends. `key_now` selects `gBlitWithEffectsKeyProgram` (a **second program object** built from the same files
with `ACTOR_KEY 1` in its own permutation list, never touching the existing program's permutations —
CLAUDE.md's `clearPermutations()` hazard), publishes `keyedThisFrame()` for the bridge flag and the UI,
and nothing else reads the fisheye/diopter settings. Fail-closed: incomplete key program → `key_now`
false for the session, `ACTORKEY NO-PROGRAM` once, UI status "unavailable (shader)".

Mode 3 output is written to all three channels; alpha untouched.

### 3.7a Key colour presets and the exactness guarantee

Presets (`ActorKeyColorPreset`); values are 8-bit sRGB display codes written by the blit as `code/255`:

| # | Preset | Hex | R,G,B | Typical use |
|---|---|---|---|---|
| 0 | **Chroma Green** | `#00B140` | 0,177,64 | The conventional "digital/broadcast green" (paint and fabric are matched to it; it sits far from skin tones and inside the Rec.709 gamut, so it survives YUV subsampling in OBS/NLE codecs). Default |
| 1 | **Pure Green** | `#00FF00` | 0,255,0 | Compositors that expect a saturated primary; OBS Chroma Key filter default ("Green") |
| 2 | **Chroma Blue** | `#0047BB` | 0,71,187 | Conventional "digital blue"; for scenes with green costumes/foliage; lower luma, less spill on hair |
| 3 | **Pure Blue** | `#0000FF` | 0,0,255 | Saturated primary for keyers that want it; OBS "Blue" |
| 4 | **Magenta** | `#FF00FF` | 255,0,255 | Scenes with both green and blue costumes; OBS "Magenta"; keep away from skin reds in the keyer |
| 5 | **Pure Black** | `#000000` | 0,0,0 | Luma key / matte; also mode 3's background |
| 6 | **Pure White** | `#FFFFFF` | 255,255,255 | Luma key (inverted) / matte |
| 7 | **Mid Grey** | `#808080` | 128,128,128 | Neutral fill for exposure/spill checks and for a shadow-pass composite where a chroma colour would bias the grade. Cheap to include; low priority |
| 8 | **Custom** | picker | any | `ActorKeyColor` edited directly; picking flips the preset to Custom |

Verification of the cited values: `#00B140` and `#0047BB` are the values the chroma-key literature
quotes for "chroma key green/blue" (they are the Wikipedia "Chroma key" infobox swatches and match
the Rosco/Datacolor digital green/blue conventions). They are **conventions, not a standard**: no SMPTE
or ITU document defines a chroma colour. For this feature exactness means "the chosen code reaches the
framebuffer unchanged", which the presets guarantee; any downstream keyer samples the actual colour
anyway. The pure primaries, black and white are exact by definition.

**Viewer-framebuffer exactness (design rule, enforced by WP2 and the self-test; v9 scope: this is the viewer's half — what a capture of the presented window shows with ReShade loaded is bounded by the D30 statement, §1, §4.2; the self-test and viewer snapshots read back with presents blocked and are ReShade-independent; snapshot exactness is further limited to unfiltered COLOR captures saved lossless at rendered size, §4.4):** the key colour is injected
in the final blit **after** every stage that could alter a value: tonemap, colour grade, exposure,
bloom composite, legacy gamma, FXAA/SMAA, CAS, DoF, motion blur, lens FX, vignette, CVD, film grain
and the blit's own dither (`blitWithEffectsF.glsl:82-87`), `applyPreview` and the `clampHDRRange`
inf/NaN scrub. Nothing follows the injection in the shader, and `GL_FRAMEBUFFER_SRGB` is explicitly
disabled for the draw (§3.7). The uniform is uploaded as `code / 255.0` in float; a UNORM8 backbuffer
stores exactly `code` (the float is within 2^-24 of the representable value).

Dither sources checked (PROVES from the blit header, lines 10-15): the 8-bit path dithers inside
`colorCorrectF` (before the ping/pong chain), the HDR/10-bit path dithers in the blit; both are
**upstream of the injection** and so cannot touch key pixels. Bloom and projector-shaft dithers are HDR
scene-space operations before `colorCorrect`; irrelevant for the same reason. The HDR→LDR conversion is
`colorCorrect` itself, also upstream: the key never passes through it.

Acceptance criteria (the self-test states PASS/FAIL per criterion):
- **8-bit backbuffer:** `glReadPixels(GL_UNSIGNED_BYTE)` of a key pixel equals the requested codes.
- **10-bit backbuffer** (`LLRender::s10bitBackBuffer`): two criteria, both required. (a) The stored
  10-bit code equals `round(code × 1023 / 255)` exactly (read via `GL_UNSIGNED_INT_2_10_10_10_REV`;
  e.g. 177 → 710, 64 → 257, 0 → 0, 255 → 1023). (b) The 8-bit readback `glReadPixels(GL_UNSIGNED_BYTE)`
  equals the requested code — deterministic, because the forward rounding error is ≤ 0.5/1023 and the
  back conversion error ≤ 0.125 of an 8-bit step, so `round(round(c·1023/255)·255/1023) == c` for every
  c in 0..255. This replaces the v2 "byte-exact" wording: a 10-bit buffer does not store the 8-bit code,
  it stores the nearest 10-bit code, and the capture path's 8-bit conversion is what the user receives.
- **`GL_FRAMEBUFFER_SRGB`** off during the blit (explicit `LLGLDisable`, §3.7). If it were on, the
  display-encoded key would be encoded twice (0,177,64 → about 0,217,137); the self-test's readback is
  the regression catch, the `llassert` the debug-build catch.

Anything **outside the viewer** is excluded from this viewer-side rule (and ReShade is covered by the D30 statement instead): ReShade techniques (§4.1), OBS colour
space conversion (set OBS to sRGB/full range), and video codecs' chroma subsampling (which is why
Chroma Green, not Pure Green, is the default: 4:2:0 subsampling smears a saturated primary's edge
more than a mid-saturation one; INFERENCE from compositing practice, not measured here).

### 3.8 Why not elsewhere in the chain
- Before `colorCorrect` (HDR, linear): the key colour would be tonemapped, graded, bloomed and dithered;
  pure green is impossible and bloom would halo the key edge. Rejected.
- Between `colorCorrect` and the blit (display-referred ping/pong): correct colour space, but
  `RGB10_A2`/fp16 targets then pass through FXAA/SMAA/CAS/DoF/fisheye/diopter (which would blur or warp
  the key edge and the colour) and finally vignette/grain/dither in the blit (which would perturb the
  exact value). Rejected.
- Inside the blit after dither: the last viewer-controlled write before UI. **Chosen.**
- After UI (a pass over the backbuffer): would key the UI/HUD. Rejected.

---

## 4. Interactions with the rest of the post chain

| Stage | Order vs key | Effect in mode 1 (actors on key) | Effect in mode 2 (set only) | Note |
|---|---|---|---|---|
| HDR bloom (`generateBloomHDR`, folded into tonemap) | before | Actor glow halo is cut at the silhouette; set glow that spills **onto** the actor stays on the actor | Set keeps all bloom; the actor hole is hard-edged against the glow | Cannot remove light already on the actor. Advise lowering bloom while keying |
| DoF / motion blur | before | A defocused actor's blur extends beyond the sharp matte → visible cut; use feather ≈ CoC or turn DoF off | same | Matte is not DoF-aware; out of scope (§9) |
| Tonemap, grade, exposure | before | Actor keeps the look; key colour is **not** tonemapped | same | Required for exact values |
| Roto Ink (any layer), Tron, on-lens filters, night mask | before | Ink on the set is keyed out, ink on actors stays | Ink on actors replaced by key | Expected; ink is scene content |
| Froxel/projector volumetrics, depth haze | before | Shafts in front of the actor stay on the actor; shafts over the set vanish | Shafts stay | Same as bloom caveat |
| FXAA/SMAA/CAS | before | AA'd scene edge vs feathered matte edge; CAS cannot sharpen the key | same | Feather ≥ 1 px recommended |
| Fisheye / diopter / kaleidoscope | before | They warp the **scene**, not the matte → misregistration | same | Key suspended for the frame when the blit's own `diopter_warped` or `fisheye_warped` local is true (pipeline.cpp:22350-22391: covers `gSnapshotNoPost`, zero blend, incomplete programs, kaleidoscope tool mode); never decided from the settings. UI reads `keyedThisFrame()` and shows the reason. D7 |
| RLVa `@setsphere` (`LLVfxManager` RlvSphere, pipeline.cpp:22335-22340) | before, post-matte | Blur / chromatic shift / pixelate would move pixels relative to the matte | same | Key suspended for the frame when the pass ran (`rlv_sphere_ran`, §3.7); UI shows the reason |
| Vignette, CVD, grain, dither (blit) | before (same shader) | Key pixels exempt; actor pixels get them | Set gets them | Intended |
| Composition guides, focus point, physics debug, UI 2D/3D, HUD elements | after | Never keyed | — | UI hidden with Ctrl+Alt+F1 for a clean capture, as today |
| HUD attachments (`render_hud_attachments`, llviewerdisplay.cpp:1515-1537) | after | Would float over the key | — | Suppressed while keying when `ActorKeyHideHUD` (default on) |
| RLVa `@setoverlay` (llviewerdisplay.cpp:1760-1764) | after (in `render_ui`, after `renderFinalize` 1747 and `gatherFrame` 1751) | Drawn over the key like UI; never moves pixels relative to the matte | same | An enforced restriction; not suppressed (v12, §14 item 5) |
| Prism TV feeds | scene content | Keyed as set | kept | Prism aux renders never run `renderFinalize` (llprismlens.cpp:7597) → no key inside feeds. Correct |
| ReShade add-on (present time) | **after** | Any enabled technique recolours/blurs the key | same | §4.1 |

### 4.1 ReShade: the problem and the viewer side (D6)
The add-on runs on the swapchain at present, after the UI (contract §1; llreshadebridge.cpp:221-225
confirms `gatherFrame()` is post-blit, pre-UI). Anything enabled in ReShade therefore contaminates the
key: RTGI/MXAO shade the key edge, sharpeners ring it, LUTs shift the colour, grain dithers it. FX code
cannot disable other techniques, so the fix lives in **our add-on** (`reshade-addon/src/
sl_reshade_bridge.cpp`), which already runs every frame on the presenting runtime.

Viewer side (ABI MINOR 1.2 → **1.3**, additive; `llreshadebridgeabi.h`, `llreshadebridge.cpp`):
1. `#define SLRESHADE_FLAG_ACTOR_KEY 0x10u` next to the four existing flags (abi.h:90-93). Set in
   `gatherFrame()` beside `SLRESHADE_FLAG_SNAPSHOT` (llreshadebridge.cpp:299-302) when
   `ALActorChromaKey::keyedThisFrame()` — true only if this frame's final blit used the key variant
   (matte frame-stamp matched). Never set from the setting alone, so a frame where the key was suspended
   (fisheye, stale matte, program missing) does not bypass ReShade for nothing.
   v6: `#define SLRESHADE_FLAG_ACTOR_KEY_ARM 0x40u` — set instead of `ACTOR_KEY` on a frame where every
   other `key_now` term is true but the key guard (§4.2) is ARMING: the frame is rendered **un-keyed**, the
   add-on brackets it exactly like a keyed present and reports whether anything still rendered. The two
   bits are never set together. (`0x20u` stays reserved for `ACTOR_KEY_HIDE_OVERLAY`, v1.1 fork path.)
1b. Key-guard report (v6, new optional viewer export, same header):
   ```
   typedef struct SLReShadeKeyGuard {
       uint32_t struct_size;        /* = sizeof of the WRITER; first 32 bytes frozen                  */
       uint32_t verdict;            /* 1 CLEAN, 2 CONTAMINATED (techniques/effect events in bracket),
                                       3 SKIPPED (no unique runtime / dead / invariant failure),
                                       4 FOREIGN_ADDON (whitelist failed, v7),
                                       5 UNSETTLED (DLL loading not yet settled, v10; viewer stays ARMING) */
       uint64_t viewer_frame;       /* f.frame of the SLReShadeFrame the bracket was opened for       */
       uint32_t passes;             /* v8: reshade_render_technique events (saturating), §4.2         */
       uint32_t gui_flags;          /* v7 (was overlay_open): SLRESHADE_GUI_* bits, §4.2 GUI note:
                                       0x1 overlay open, 0x2 OSD always on (ini), 0x4 splash (tutorial),
                                       0x8 splash (< 5 s after a reload), 0x10 add-on reloaded          */
       uint32_t foreign_addons;     /* v7: number of non-whitelisted add-on modules/files found         */
       uint32_t effect_calls;       /* v8 (was reserved0): begins + finishes, saturating add (v9)       */
   } SLReShadeKeyGuard;             /* 32 bytes, static_assert (layout unchanged from v6) */
   extern "C" __declspec(dllexport) void SLReShade_ReportKeyGuard(const SLReShadeKeyGuard* g);
   ```
   Called by the add-on at `reshade_present` of every bracketed present (ARM or KEY; v8: FOREIGN_ADDON
   presents are bracketed too), and for SKIPPED also from the raw `present` that declined to bracket — always on the presenting
   thread (for GL the viewer's main thread inside `SwapBuffers`) and **never while `g_bracket_lock` is
   held**. The viewer copies the struct into atomics during the call, never retains the pointer, and
   ignores reports whose `viewer_frame` is older than the current arming epoch. v7: the viewer treats
   the **absence** of a report as a failure (heartbeat, §4.2), so a missing export on an old viewer is
   irrelevant (such a viewer has no guard at all).
2. Reader handshake (§4.3): `SLReShade_GetFrame(requested_abi)` records `mReaderAbi = requested_abi`
   and `mReaderSeenFrame = gFrameCount`. New optional export, **one signature** (Codex R2 P1-10), in
   `llreshadebridgeabi.h`:
   ```
   typedef struct SLReShadeReaderInfo {
       uint32_t struct_size;        /* = sizeof(SLReShadeReaderInfo) of the WRITER (the add-on)      */
       uint32_t abi_version;        /* SLRESHADE_ABI_VERSION the add-on was compiled against          */
       uint32_t caps;               /* SLRESHADE_READER_CAP_*: KEY_BYPASS 0x1, KEY_GUARD 0x2, HARD 0x4 */
       uint32_t reserved0;          /* 0                                                               */
       char     build_stamp[64];    /* NUL-terminated UTF-8, e.g. "2026-10-05 14:02:11"                 */
       char     commit[16];         /* NUL-terminated git short SHA, or "" if unknown                  */
       uint8_t  source_sha256[32];  /* manifest hash, section 4.3                                      */
   } SLReShadeReaderInfo;           /* 16 + 64 + 16 + 32 = 128 bytes */
   static_assert(sizeof(SLReShadeReaderInfo) == 128, "reader info layout");
   extern "C" __declspec(dllexport) void SLReShade_ReportReader(const SLReShadeReaderInfo* info);
   ```
   Contract: the add-on calls it once per `init_effect_runtime`, on the presenting thread, with a
   pointer to its own static storage; the viewer **copies** the struct during the call (reads
   `min(info->struct_size, sizeof)` bytes, zero-fills the rest) and never retains the pointer. Strings
   are owned by the add-on and must be NUL-terminated within their arrays (the viewer forces the last
   byte to NUL after copying). A null export (older viewer: `GetProcAddress` returns null) → the add-on
   logs once "viewer has no ReportReader; stale check unavailable" and continues. A null `info` or
   `struct_size < 16` → the viewer ignores the call and logs once. Future growth appends fields and
   bumps `struct_size`; the first 128 bytes never change. Old add-ons never call it (they resolve only
   `SLReShade_GetFrame`), which is itself the stale-add-on signal.
3. UI/log (v8 wording; every red line means **the key is refused**):
   - **Guarantee line (v8, always visible in the Lightbox key section while ReShade is loaded):**
     "Key colour is exact except where ReShade's own UI draws, and an add-on effect switched on mid-key
     may tint ONE frame before keying stops." Without ReShade: "Key colour is exact." No other UI text
     may claim exactness.
   - "ReShade: not loaded" (no ReShade module in the process; re-checked before every keyed frame,
     §4.2 reader states) — key runs;
   - "ReShade add-on 1.3, key bypass active" — KEYING with a guard reader;
   - amber "ReShade: checking…" — ARMING (normally one frame);
   - **red** "ReShade is loaded but the SL bridge add-on is not running — key refused (deploy
     sl_reshade_bridge.addon)";
   - **red** "ReShade add-on is OLDER than this viewer (no key guard) — key refused (run deploy_addon.ps1)";
   - **red** "ReShade effect activity with Effects off: <p> pass(es), <c> begin/finish call(s)
     (.addonfx / add-on effects) — key refused. Disable them in the ReShade overlay, then Retry"
     (CONTAMINATED; conservative: activity is counted whatever it writes, §4.2);
   - **red** "Other ReShade add-ons are loaded (<n>, listed in ReShade.log) — key refused. Remove them
     from the add-on folder for keying" (FOREIGN_ADDON) / "ReShade add-on check failed: <step> — key
     refused" (enumeration failure or truncation, v8) / "ReShade check failed — key refused" (viewer-side
     module test failure, v8);
   - **red** "Unverified ReShade build — key refused (expected 6.8.0.2155)" (host binary gate, §4.2);
   - **red** "ReShade bridge stopped confirming frames — key refused" (heartbeat miss / SKIPPED / dead);
   - **amber, named causes under the guarantee line while keying:** "overlay open", "FPS/clock OSD
     always on (ReShade.ini)", "splash shown until the ReShade tutorial is dismissed", "splash after
     reload (≤ 5 s)". Texture preview has no observable state and is covered by the guarantee line.
   Summary line carries `reader_abi=<hex> reader_caps=<hex> guard=<state> guard_passes=<n>
   guard_calls=<n> guard_gui=<hex> foreign=<n> heartbeat_miss=<n> arm_presents=<n> module_scans=<n>`.
4. `SL_GBufferProvider.fx` needs nothing; the matte is not published as a semantic (D13, later).

### 4.2 ReShade add-on: key bypass (D6)
Files: `reshade-addon/src/sl_reshade_bridge.cpp` (all logic), `reshade-addon/src/sl_depth_pin.hpp`
(untouched: the GD private-ABI pin is unrelated and its exact-binary check stays), the vendored
`llreshadebridgeabi.h` copy the add-on compiles against (bumped to 1.3 in lockstep).

**Event order (Codex R1, verified against I:\reshade-SL\source\runtime.cpp:628-659, 830).** Within one
present: the raw `addon_event::present` (device/swapchain level) fires first, then
`present_effect_runtime` renders the effects and the overlay, then `reshade_present` fires. So:
- a handler on `reshade_present` acts **after** the effects of the frame it inspects — one frame late,
  and the keyed frame would already carry RTGI. **Rejected** (v2's choice);
- the raw `present` event is the only hook that precedes effect rendering. The bridge already registers
  it for `depth_pin::present` (bridge.cpp:1152); v3 adds a second handler there. **v6 (D26): in v1 that
  handler runs the soft bracket below plus the key guard, against the stock deployed DLL.** The fork
  hard bypass (§4.2a) is the v1.1 follow-up; when a DLL exporting it is present the handler uses it
  instead and the guard still runs (it then should never see a technique).

**v1 mechanism.** `effect_runtime::set_effects_state(false)` (reshade_api.hpp:684) — the same
global switch as the Home-key "Effects" checkbox. PROVES (Codex R1, runtime_api.cpp:1316-1323): it
changes only `_effects_enabled`; it writes neither the preset nor `ReShade.ini`. PROVES (Codex R3,
runtime.cpp:3701/3716/3923): it does **not** suppress `addon`-flagged effects, so on its own it is not
exact; v6 closes that hole with the **key guard** (detect and refuse) instead of a DLL change. It was
chosen over per-technique `set_technique_state` (api.hpp:635; edits the live preset, which ReShade
autosaves) and over "skipping technique rendering" (no veto exists: `reshade_begin_effects`/
`reshade_render_technique` are void notifications, events.hpp:1946, 1758-1761).

**Key guard (v6 option (c); v7 = Codex R5; v8 = Codex R6 + user decision).**

> **The v1 guarantee (the single statement used in the UI, §1, the tests and the user guide):**
> *"Key colour is exact except where ReShade's own UI draws, and an add-on effect switched on mid-key
> may tint ONE frame before keying stops."*
> Without ReShade loaded the key is exact, full stop. "Add-on effect switched on mid-key" includes a
> ReShade add-on loaded mid-key. The one-frame limit is inherent to the soft path (a CLEAN report for
> frame N-1 cannot prove frame N clean) and was **accepted by the user (2026-10-05, D30)**; the v1.1 fork
> bypass (§4.2a) removes it.

*Provenance of citations (Codex R6 item 7).* Plain `file:line` numbers in this section refer to the
**local** tree `I:\reshade-SL` at `v6.7.3-2-gd13926f9` (6.7.3.3). There is **no local 6.8.0 checkout**.
Statements about 6.8.0 semantics cite the upstream sources on GitHub, tag v6.8.0:
`https://github.com/crosire/reshade/blob/v6.8.0/source/runtime.cpp`, `.../source/runtime_api.cpp`,
`.../source/runtime_gui.cpp`, `.../source/addon_manager.cpp`, `.../include/reshade.hpp`, as read by
Codex R5/R6 and (include/reshade.hpp, source/runtime_internal.hpp) by WebFetch on 2026-10-05.

The guarantee is split by *who* can draw into the back buffer during a keyed present:

| Who draws during present | Observable? | Treatment |
|---|---|---|
| ReShade effects (ordinary `.fx`) | suppressed by the bracket | bracket (6.8.0: ordinary effects are suppressed with Effects off — Codex R5, upstream runtime.cpp) |
| `.addonfx` effects (`effect.addon`; 6.8.0 marks `.addonfx` — Codex R5, upstream runtime.cpp) | yes: `reshade_render_technique` after every ReShade-rendered technique (Codex R5) | **counted → CONTAMINATED → refuse** (one-frame limit) |
| Add-on calls to the public `effect_runtime::render_technique` / `render_effects` | yes: in 6.8.0 the public `render_technique` raises begin/render/finish **regardless of the Effects state** (Codex R6, upstream runtime_api.cpp); `render_effects` is gated | counted; only whitelisted add-ons can make such calls, and neither ours nor the two built-ins does |
| Arbitrary GPU work by add-ons in any event (`reshade_begin_effects`/`finish_effects`, `reshade_overlay`, `present`, `reshade_present`, …) | **no** event, no API | **strict whitelist, maintained continuously → FOREIGN_ADDON → refuse** (one-frame limit for an add-on loaded mid-key) |
| ReShade's own GUI: overlay, OSD, splash/message windows, texture preview (`draw_gui()`; 6.7: runtime_gui.cpp:785-836, 1337-1353, 1440-1476) | partly; preview **no** | **always announced** — part of the guarantee statement (D29) |

*Counters (per bracket slot, under the lock; v8 = R6 item 5).* Three **separate** saturating counters,
each kept twice (`*_since_present` since the runtime's last `reshade_present`, and `*_in_bracket` while
the bracket is open):
- `passes` — `reshade_render_technique` events (one per rendered technique);
- `begins` — `reshade_begin_effects` events (bridge.cpp:1157 already registers it);
- `finishes` — `reshade_finish_effects` events (bridge.cpp:1153 already registers it).
One technique rendered through the public API raises one of each; a `render_effects` pass where
techniques exist but none is enabled raises begin + finish with `passes == 0`. The report carries
`passes` and `effect_calls = sat_add(begins, finishes)` (saturating at `UINT32_MAX`, v9) separately
(§4.1 item 1b), and the status says **"ReShade effect
activity with Effects off: <p> pass(es), <c> begin/finish call(s)"**, never "tint". Verdict CONTAMINATED
iff any of the three is non-zero.
Corrected proof (R6 item 4): with Effects off and no `.addonfx`, ReShade's **automatic** `render_effects`
returns before raising begin/finish (6.7: 3701 → 3909/3949; 6.8.0 per Codex R5/R6), so in a CLEAN bracket
no automatic effect pass ran. The **public** `effect_runtime::render_technique` is *not* gated by the
Effects state in 6.8.0 and raises begin/render/finish itself — those calls are still counted (and so
refused), and only whitelisted code (ours, GD, Effect Runtime Sync — none of which call it) could issue
them. The v7 claim "a CLEAN bracket proves no begin/finish callback of any add-on ran" is **withdrawn**;
what holds is "no begin/finish event fired on that present", which is what the counters measure.
False refusals (R5 P1-5, unchanged): no RTV/dependency filtering; documented conservative refusal (D27).

*Strict add-on whitelist (R5 P0-1; v8 = R6 item 3: maintained during KEYING).* No add-on enumeration
API exists in 6.8.0 (upstream include/reshade.hpp at v6.8.0, GitHub; 6.7: include/reshade.hpp:25-48;
`addon_loaded_info` is internal, 6.7 addon_manager.cpp:140). The add-on proves the set itself:
1. **Host binary**: depth pin `binary_allowed` (exact 6.8.0.2155 SHA-256, sl_depth_pin.hpp:30-78),
   computed once per process (the DLL cannot change while loaded) ⇒ built-ins = Generic Depth +
   Effect Runtime Sync (6.7 addon_manager.cpp:161-189; same two in 6.8.0 — closed by R6). Unknown host →
   FOREIGN_ADDON "unverified ReShade build".
2. **Add-on folder**: listing of the search path must be exactly `{ our file }`. Path resolution
   mirrors ReShade (6.7 addon_manager.cpp:213-216; same rule upstream v6.8.0): **no `[ADDON] AddonPath`
   key is the normal case and selects the base path** (`get_reshade_base_path`); a present key selects
   `base / AddonPath` (an absolute value replaces the base, as `std::filesystem` `operator/` does). A
   directory that does not exist (`ERROR_PATH_NOT_FOUND`/`ERROR_FILE_NOT_FOUND`) is an **empty set** —
   ReShade's own iteration with an error code loads nothing from it (6.7: 227). Only a truncated value
   (`get_config_value`/`get_reshade_base_path` size > buffer), a path that cannot be formed (exceeds
   `MAX_PATH`-extended limits, invalid characters) or any other `FindFirstFileW` error refuses (v9, R7
   item 5). Run at `init_effect_runtime` and every ARMING (ReShade loads folder add-ons only at
   start-up, 6.7 addon_manager.cpp:144-148, so the loaded consequences are covered continuously by
   item 3).
3. **Loaded modules**: `K32EnumProcessModules` into a fixed 1024-entry array; for each module other than
   the ReShade DLL and ours: `.addon*` extension, or any of the exports `AddonInit`, `AddonUninit`,
   `NAME`, `DESCRIPTION` → foreign.
**Continuous maintenance.** The add-on registers `LdrRegisterDllNotification` (ntdll, documented;
resolved with `GetProcAddress`) at init.
**One packed module-state word (v11, Codex R9 P0-1).** v10 kept the generation and the notification
time in two variables written by two separate interlocked stores, so a callback paused between them let
a scan pair cache CLEAN with the new generation and the old timestamp, and the 500 ms fast path compared
the generation only. v11 replaces both with a single 64-bit word updated in one atomic step:
```
volatile LONG64 g_mod_state;      // bits 63..32 = generation (uint32, wraps), bits 31..0 = GetTickCount() at that bump (uint32 ms)
// LdrRegisterDllNotification callback — the ONLY writer; lock-free, loader-lock-safe (kernel32 GetTickCount + Interlocked only):
LONG64 old, nw;
do {
    old = InterlockedCompareExchange64(&g_mod_state, 0, 0);                       // atomic 64-bit read
    nw  = (LONG64)((((ULONG64)(ULONG)(old >> 32)) + 1) << 32) | (ULONG64)GetTickCount();
} while (InterlockedCompareExchange64(&g_mod_state, nw, old) != old);           // generation and time change together
// readers: s = InterlockedCompareExchange64(&g_mod_state, 0, 0); gen(s) = (ULONG)(s >> 32); t(s) = (ULONG)s
```
Every reader therefore sees a generation and the timestamp **of that same bump**, never a mix. Age is
computed in wrapping 32-bit arithmetic, `(ULONG)(GetTickCount() - t(s))`, valid for intervals < 49.7
days (the only intervals compared are 200 ms and 500 ms). A notification that arrives while another is
mid-CAS simply retries; both bumps are counted.
**Settle rule (v10, Codex R8 P0-2).** A generation that did not move during one scan does not prove the
enumeration was complete: a notification can precede the loader's list insertion, and
`K32EnumProcessModules` can return a plausible but stale list while the list changes. A CLEAN module
verdict may therefore be **cached** only when module loading is observably settled:
(a) **quiet period** — no notification for ≥ `SETTLE_QUIET_MS` = 200 ms before the first scan starts;
(b) **two consecutive full scans** with identical results (same sorted `HMODULE` list, same per-module
verdicts) and (c) **no notification** between the start of the first scan and the end of the second
(the packed word `g_mod_state` bit-for-bit unchanged across both). Until all three hold, the verdict is
**UNSETTLED** (new verdict 5): the bracket still opens, and the viewer does not key (it stays ARMING with
the amber status "waiting for DLL loading to settle", §4.2 state machine). Nothing incomplete is ever
cached.
```
module_guard():                                       // raw present, KEY or ARM (presenting thread only)
    s = InterlockedCompareExchange64(&g_mod_state, 0, 0);                          // ONE atomic read: gen + time together
    if (g_have_notify && g_cache_valid && s == g_settled_state                      // v11: whole 64-bit word, not the generation alone
        && (ULONG)(GetTickCount() - g_last_scan_ms) < 500) return g_cached;          // cached only if settled
    g_cache_valid = false;                                                          // nothing stale survives a miss
    if (g_have_notify && (ULONG)(GetTickCount() - t(s)) < SETTLE_QUIET_MS) return UNSETTLED;   // (a) quiet period from the SAME word
    r1 = full_module_scan();  if (r1.failed) return FOREIGN(r1.step);               // failure refuses (below)
    r2 = full_module_scan();  if (r2.failed) return FOREIGN(r2.step);               // (b)
    if (r1 != r2 || InterlockedCompareExchange64(&g_mod_state, 0, 0) != s) return UNSETTLED;   // (b)/(c): word unchanged bit-for-bit
    g_settled_state = s;  g_last_scan_ms = GetTickCount();  g_cached = r2.verdict;  g_cache_valid = true;
    return g_cached;                                                                // caches the PRE-scan word s only
```
If the notification cannot be registered (`g_have_notify == false`), no quiet period is observable: the
two-scan rule (b) runs on **every** KEY/ARM present and nothing is cached (each present either has two
identical scans or reports UNSETTLED).
Residual (stated): a module whose `DllMain` is still running after the quiet period is listed and judged
by its exports (exports exist from mapping), so an add-on mid-`DllMain` is still classified; only a
module inserted into the loader list more than 200 ms after its notification could escape both scans,
which the loader does not do in normal operation (INFERENCE; §14 R9-B).
**Fail closed on any enumeration problem** (R6 item 3): `K32EnumProcessModules` returns FALSE; the
needed size exceeds the array (truncation); `GetModuleFileNameW` fails or fills its buffer
(truncation); a truncated or unformable add-on path, or a `FindFirstFileW` error other than not-found
(item 2) → verdict FOREIGN_ADDON, reason "add-on check failed: <step>" → refuse.
**On FOREIGN_ADDON the bracket still opens** (ordinary effects are suppressed on that present too);
only the verdict differs.
Cost (Tracy zones `SLBridge::moduleScan`, `ActorKey::moduleGate`): the steady-state check is one
interlocked read + one clock read per keyed present. A full scan is one `K32EnumProcessModules` (~250
modules in this viewer) plus `GetModuleFileNameW` and up to 4 `GetProcAddress` per module — **50-300 µs
is an unverified estimate**, to be replaced by the first Tracy measurement (§11 9k(e)). Amortised:
**with no module churn** the 2 Hz re-validations cost ≤ 4 × the per-scan time per second (two scans
each under the v10 settle rule; ≤ 1.2 ms/s at the unverified estimate's upper end); each settle after a module load/unload adds two scans (more if loads keep
landing). Without the notification (fallback), every keyed present pays two full scans; the log says
so once.
Residual (D28, unchanged): a convention-free self-registering DLL loaded outside the folder.

*Reader states (R5 P0-3; v8 = R6 item 2: re-checked before every keyed frame).* Viewer side, main
thread, evaluated in `renderFinalize` **before every keyed blit** (inside `keyGuardAllows()`):
- the viewer registers its own `LdrRegisterDllNotification` callback with the **same packed-word
  protocol** (v11): `volatile LONG64 sModState` (generation | GetTickCount), CAS loop as above, the only
  writer; every comparison below uses the whole 64-bit word read with one `InterlockedCompareExchange64`;
- `NO_RESHADE` is valid for a frame only if `sModState == mStateAtReShadeScan` (whole word); otherwise the viewer
  re-runs the ReShade-module test (a module exporting both `ReShadeRegisterAddon` and
  `ReShadeUnregisterAddon`, the `get_reshade_module_handle` rule, 6.7 reshade.hpp:62-85) **before**
  deciding this frame, with the **same settle rule** as the add-on (v10): quiet ≥ 200 ms since the
  viewer's last notification (timestamp taken from the same word as the generation), two identical
  consecutive scans, the word unchanged across both, then `mStateAtReShadeScan = s` (the pre-scan
  word). Until settled, frames are **not keyed** (status amber
  "waiting for DLL loading to settle"). ReShade found → this frame is **not keyed**, state re-evaluated (→
  RESHADE_NO_READER until the bridge reports). Scan failure or truncation → not keyed, REFUSED "ReShade
  check failed". Notification unavailable → the test runs before every keyed frame (cost as above).
  Scope (v9, R7 item 3): the check runs in `renderFinalize` **before** the blit; a ReShade that loads
  after that check, with an effect already enabled, can draw on that already-keyed present. That is
  the one frame the D30 statement allows ("an add-on effect switched on mid-key" includes ReShade
  itself loading with an enabled effect); the next frame is not keyed.
- `RESHADE_NO_READER` (ReShade present, no `SLReShade_GetFrame` in the last 120 frames) and
  `READER_NO_GUARD` (no `ReportReader` or no `CAP_KEY_GUARD`) → REFUSED.
- (v11 naming note: wherever this section and §11 say `mModuleGen` / `mGenAtPublish` /
  `g_module_gen` / `g_settled_gen`, read the packed 64-bit word `sModState` / `g_mod_state` and its
  whole-word captures; generation-only comparisons no longer exist.)
- `READER_GUARD` → ARMING/KEYING below. The viewer captures `mModuleGen` in `gatherFrame()` of every
  bracketed frame (`mGenAtPublish`, taken **before** that frame's present and therefore before the
  add-on's scan of it — capture-before, v9). Frame N keys only if `mModuleGen` still equals the value
  captured for the previous bracketed frame; otherwise → ARMING (frame N un-keyed), and ARMING only
  returns to KEYING on a CLEAN report, which the add-on can issue only after its settle rule holds
  (UNSETTLED reports keep ARMING). Cost (v10): any DLL load mid-key (e.g. a lazily loaded viewer DLL)
  stops keying for at least the 200 ms quiet period plus two scans — about 12-20 frames at 60 fps,
  shown as amber "waiting for DLL loading to settle".

*Heartbeat (R5 P0-2).* In KEYING the viewer keys frame N only with a CLEAN report for the **previous
bracketed frame** (the last frame published with `ACTOR_KEY` or `ACTOR_KEY_ARM`), delivered in that
frame's `SwapBuffers` on the same thread before frame N's `renderFinalize`. Stale or missing → frame N
is not keyed (`heartbeat_miss++`) → ARMING.

*ARMING timeout (v8 = R6 item 6, exact boundary).* On every entry to ARMING: `epoch = gFrameCount`,
`mArmPresents = 0`. Order inside each frame's guard evaluation (in `renderFinalize`, before the blit):
(1) consume a qualifying report (`viewer_frame ≥ epoch`) → act on its verdict; (2) else, if
`mArmPresents >= 60` → **REFUSED** for this frame (no flag published, not keyed); (3) else publish
`ACTOR_KEY_ARM` for this frame and `++mArmPresents` (in `gatherFrame()`, when the flag is written). So
exactly **60** ARM presents are attempted; the frame evaluated after the 60th ARM present without a
qualifying report is the first REFUSED frame (it publishes no flag). A report delivered in the 60th ARM
frame's `SwapBuffers` is consumed in step (1) of that next evaluation and wins over the timeout.
UNSETTLED (v10): a qualifying UNSETTLED report keeps ARMING, publishes ARM for this frame and **resets**
`mArmPresents = 0` (DLL loading in progress is not a bridge failure; ARMING stays un-keyed, so waiting is
fail-closed). The 60-present timeout therefore measures only presents without any report.

Viewer state machine (`ALActorChromaKey`, main thread):
```
OFF ──key on──► NO_RESHADE ──► KEYING_UNGUARDED (keys; before EVERY keyed frame: module-gen check, ReShade re-test on change)
KEYING_UNGUARDED ── ReShade module appears ──► this frame un-keyed; → RESHADE_NO_READER | READER_GUARD path
OFF ──key on──► RESHADE_NO_READER | READER_NO_GUARD ──► REFUSED (red, reason)
OFF ──key on──► READER_GUARD ──► ARMING (epoch = gFrameCount, mArmPresents = 0; frames UN-keyed, ACTOR_KEY_ARM)
ARMING ── qualifying CLEAN ──► KEYING (next frame keys)
ARMING ── qualifying UNSETTLED ──► ARMING (mArmPresents = 0; amber "waiting for DLL loading to settle")   [v10]
KEYING ── UNSETTLED ──► ARMING (this frame un-keyed)                                                      [v10]
ARMING ── CONTAMINATED | FOREIGN_ADDON | SKIPPED ──► REFUSED;  ARMING ── evaluation with mArmPresents ≥ 60 ──► REFUSED
KEYING ── report for the previous bracketed frame missing/stale, or mModuleGen changed ──► ARMING (this frame un-keyed)
KEYING ── CONTAMINATED | FOREIGN_ADDON | SKIPPED ──► REFUSED (this frame un-keyed)
REFUSED ── Retry | key off→on | reload flag (gui_flags 0x10) ──► re-evaluate reader state → ARMING or REFUSED
any ── key off ──► OFF
```
Why the guarantee statement holds (v8): with a guard reader, on every keyed present (i) ordinary effects
are off (bracket, opened even on FOREIGN); (ii) any effect activity is counted and the previous frame's
CLEAN report is required, so activity that first appears on frame N can affect only frame N (the
accepted one-frame limit); (iii) the module set is re-validated by the viewer before each keyed frame
(generation) and by the add-on at each KEY/ARM present (generation + 2 Hz), and a CLEAN module verdict
is cached only after loading has settled (v10: quiet period + two identical scans), so an add-on loaded
mid-key is likewise limited to the frame in which it loads, minus the documented residual; (iv) ReShade's own
UI is always announced. Without ReShade, nothing draws after the blit and the key is exact; a ReShade
that appears later is caught before the next keyed frame.

**ReShade GUI in v1 (R5 P1-6).** v1 cannot suppress `draw_gui()`. The warning is therefore
**unconditional** while ReShade is loaded and the key is on (amber line of §4.1 item 3), and the
add-on adds named causes in `gui_flags` where state is observable:
- 0x1 overlay open — `reshade_open_overlay` observed (returns `false` = never vetoes,
  runtime_gui.cpp:5102-5106);
- 0x2 OSD always on — `get_config_value(rt, "OVERLAY", "ShowFPS" | "ShowClock" | "ShowFrameTime" |
  "ShowPresetName")` == 1 (runtime_gui.cpp:298-302; the live hotkey toggle at 772-775 is not
  observable, hence the unconditional line);
- 0x4 splash until the tutorial is dismissed — `get_config_value(rt, "OVERLAY", "TutorialProgress")` == 0
  (runtime_gui.cpp:305, 793);
- 0x8 splash after reload — within 5 s of `reshade_reloaded_effects` (runtime_gui.cpp:793);
- texture preview (`_preview_texture`, drawn even with the overlay closed, runtime_gui.cpp:1440-1476)
  has no observable state → covered only by the unconditional line.
Add-on `reshade_overlay` callbacks from non-whitelisted add-ons cannot exist (whitelist). Hiding is the
v1.1 fork path's job (§4.2a, D25 deferred).

**One bracket per runtime (Codex R2 P0-4) — the v1 path.** PROVES: ReShade creates one `effect_runtime` per
swapchain (reshade-SL runtime_manager.cpp:15-47, 74-77) while the bridge keeps a single global
`g.runtime` (bridge.cpp:239-242, 843-850). The raw `present` event carries the **swapchain**
(`present(command_queue*, swapchain*, const rect*, const rect*, uint32_t, const rect*)`,
events.hpp:1941) and API 18 keeps `effect_runtime` and `swapchain` as separate interfaces that must
never be cast into each other (sl_depth_pin.hpp:230). The depth pin matches them in `present` by
`runtime->get_device() == swap->get_device() && runtime->get_hwnd() == swap->get_hwnd()`
(sl_depth_pin.hpp:224-235); the bracket uses the same match. **v6 (Codex R4 P0-4): the registry is a
lifetime-stable fixed array, not a `std::map`**, so nothing in it allocates, frees or relinks nodes:
```
struct KeyBracket {                 // plain data; every field read and written ONLY under g_bracket_lock
    effect_runtime *rt;             // nullptr = free slot
    bool open, saved;
    uint32_t reload_during, gui_flags;
    uint32_t passes_since, begins_since, finishes_since, passes_in, begins_in, finishes_in;   // v8: separate, saturating
    bool foreign;                   // v8: whitelist verdict for this bracket
    uint64_t viewer_frame;          // f.frame the bracket was opened for (guard report)
};
static KeyBracket g_brackets[8];    // never reallocated; zero-initialised at load
static SRWLOCK    g_bracket_lock = SRWLOCK_INIT;
init_effect_runtime(rt):    slot = find(rt) ?: free slot; if found && open → restore first; reset fields; slot.rt = rt
                            (no free slot → log once "actor-key: >8 runtimes, bracket unavailable (runtime %p)";
                             that runtime is never bracketed and its presents report SKIPPED)
destroy_effect_runtime(rt): slot = find(rt); if (slot) { if (slot.open) restore(rt, slot); slot = {} }   // runtime still alive (runtime.cpp:580-582)
```
Only the runtime that **owns the presenting swapchain** is ever mutated, and only when the match is
**unique** (Codex R3 P1-6, confirmed R4): zero or several matches → `[SLBridge] actor-key: %u runtimes
match the presenting swapchain, skipping` once, nothing is touched, and the guard reports SKIPPED so the
viewer refuses instead of keying unprotected.

**The bracket (v1).** Effects are never left disabled across a present boundary. Every ARM or KEY present
is bracketed on its own runtime:
```
raw present(queue, swap, ...)  (before effects; opengl_hooks_wgl.cpp:272 → 276 → runtime.cpp:586):
    ok  = read_viewer_frame(f)                                   // seqlock memcpy, bridge.cpp:393-447; no GL
    key = ok && (f.flags & (SLRESHADE_FLAG_ACTOR_KEY | SLRESHADE_FLAG_ACTOR_KEY_ARM))   // torn/absent == neither
    if (!key) return;
    rt = unique_runtime_for(swap)                                // exactly one device+hwnd match, else log once, report SKIPPED, return
    if (!rt) return;
    if (g.dead)            { report(SKIPPED, f.frame); return; }                      // v7: outside any lock
    whitelist_ok = module_guard();          // v8: rescan if g_module_gen moved or >= 500 ms (or every present w/o notification); any enumeration failure == false
    if (g_hard_bypass && ReShadeSLSetPresentBypass(swap, mask)) { mark slot hard; return; }   // v1.1 only (§4.2a); false → bracket below
    stale = false;
    LOCK(g_bracket_lock) {                                       // SEH-safe protocol below
        b = find(rt); if (!b) break;
        if (b->open) {                                           // v7 (R5 P1-4): INVARIANT FAILURE — a previous present never closed
            if (!rt->get_effects_state()) rt->set_effects_state(b->saved);   // restore exactly as reshade_present would
            b->open = false;  g.dead = true;  stale = true;      // close, stop bracketing for the session
            break;
        }
        b->viewer_frame = f.frame;  b->passes_in = b->begins_in = b->finishes_in = 0;  b->foreign = !whitelist_ok;   // v8: bracket opens even when FOREIGN
        b->saved = rt->get_effects_state();
        b->open  = true;                                         // written BEFORE the mutating call (Codex R3 P0-4)
        if (b->saved) rt->set_effects_state(false);
    }
    if (stale) { log_once("actor-key: stale open bracket (runtime %p) — restored, bridge dead"); report(SKIPPED, f.frame); return; }
update_effects() / effects + overlay (ReShade): may raise reshade_reloaded_effects(rt) WHILE b.open (handler leaves the
    bracket alone); reshade_render_technique / reshade_begin_effects / reshade_finish_effects on rt →
    LOCK { passes|begins|finishes _since_present++; if open: the matching _in_bracket++ }   (v8: three separate counters, §4.2)
reshade_present(rt)  (after effects + overlay):
    LOCK(g_bracket_lock) {
        b = find(rt); if (!b) break;
        if (b->open) {
            if (!rt->get_effects_state()) rt->set_effects_state(b->saved);   // restore only if still off: a user toggle inside the bracket wins
            b->open = false;
            verdict = b->foreign ? FOREIGN_ADDON : (any of the six counters != 0) ? CONTAMINATED : CLEAN;   copy passes, effect_calls, gui_flags
        }
        b->passes_since = b->begins_since = b->finishes_since = 0;   // every present, bracketed or not
    }
    if (verdict) SLReShade_ReportKeyGuard(&report)               // OUTSIDE the lock: never call into the viewer while holding it
```
**SEH-safe lock protocol (Codex R3 P0-4, revised for R4 P0-4).** C++ lock guards do not unwind through a
structured exception, so:
- the lock is a Win32 `SRWLOCK` taken with `AcquireSRWLockExclusive`/`ReleaseSRWLockExclusive` (C API,
  no destructor), and every locked region is `__try { ... } __finally { ReleaseSRWLockExclusive(&g_bracket_lock); }`
  — on MSVC x64 the `__finally` runs during the unwind phase, before the outer `__except` body
  executes, so the faulting thread no longer holds the lock when recovery runs. Functions containing
  `__try` hold no C++ objects with destructors (C2712), exactly as `on_begin_effects` does today
  (bridge.cpp:1115-1138);
- **no lock-free path exists** (the v5 lock-free map walk is deleted). `restore_all_open_brackets()`
  acquires the lock with a **blocking** `AcquireSRWLockExclusive`. It cannot self-deadlock (the
  `__finally` above released it) and cannot wait forever on another thread, because every other holder
  is also inside a `__try/__finally` region with no blocking call except `get/set_effects_state`. It
  walks the fixed array (POD, no allocator, no node pointers), and for each `open` slot calls
  `set_effects_state(saved)` inside its own inner `__try/__except` (a second fault on one runtime skips
  that runtime and is logged; the walk continues), clears `open`, releases, then sets `g.dead = true`;
- because `open = true` is written before `set_effects_state(false)`, a fault inside the mutating call
  is recoverable (the bracket is already visible to recovery);
- the guard counters are mutated only under the lock; `SLReShade_ReportKeyGuard` is called after the
  release, so a fault inside the viewer callback can never be taken with the lock held;
- the v1.1 hard-bypass call takes none of this lock and leaves no state to recover.
Invariant: for each runtime, `open` is true only between its raw present and its `reshade_present`.
Every other path also closes it with restore:
- `destroy_effect_runtime(rt)` — runtime still exists (Codex R1: runtime.cpp:580-582): restore if open,
  then clear the slot. No "drop on destroy".
- `reshade_reloaded_effects(rt)` — **can fire inside the bracket** (Codex R2 P1-5: raw present precedes
  `update_effects()`, which raises it, runtime.cpp:3354, 3369, 3688). The handler does not touch
  `open`/`saved`; it counts `reload_during++` for the log and tells the viewer (one-shot flag in the next
  guard report) so a REFUSED guard can re-arm. A reload does not change `_effects_enabled`, so the
  pending restore at `reshade_present` is still the right value. No assert anywhere in the bracket code.
- `init_effect_runtime(rt)` — claims a slot; if the same pointer already has one (re-init) it is reset
  **after** restoring if it was open.
- SEH: every handler that takes the lock is wrapped like `on_begin_effects`. The `__except` path calls
  `restore_all_open_brackets()` (blocking acquire, above), then `g.dead = true`. Once dead, no bracket
  opens again and raw presents with KEY/ARM report SKIPPED (if the report call itself is still safe —
  it touches no bridge state) so the viewer refuses; the handshake shows "bridge faulted".
- `DLL_PROCESS_DETACH` mid-bracket means the process is exiting; the swapchains are gone with it.
- Missing/torn viewer frame: no bracket opens for that present; `g.key_frames_missed` counts it; the
  viewer, receiving no report for an ARM frame, stays ARMING until the 60-present timeout. No
  hysteresis: nothing persists past the present.
Edge-logged per runtime (`[SLBridge] actor-key bracket first open / last close (runtime %p)`,
`[SLBridge] actor-key guard CONTAMINATED tech=<n> (runtime %p)`), never per frame.

User toggles during keying (D18, Codex R2 P2-12): the overlay is drawn **before** shortcuts are processed
(runtime.cpp:652-677), so a Home press during a keyed present is applied after the overlay and before
`reshade_present`. The restore-only-if-still-off rule then sees `true`, skips the restore, and the
user's toggle survives as the new setting; the next raw present saves `true` and disables again, so
effects stay suppressed while keyed and return the instant the key clears. On screen the checkbox
therefore stays **unchecked** for the whole keyed period — the toggle is observable only as state (the
add-on logs "user toggle observed inside bracket" once) and after the key is off.

Nothing else in the bridge changes: semantic slots update whenever `begin_effects` runs (it does not
run while disabled — the same gap as the user having effects off today), the depth pin is untouched.

**Why the bracket suppresses ordinary effects (one part of the v1 guarantee, §4.2).** With `_effects_enabled == false` and no `addon`-flagged effect,
`render_effects` returns at 3701 before any technique (Codex R1 against runtime.cpp); the swapchain
presents the viewer's framebuffer. If any technique does render, the guard sees it and the viewer stops
keying (§4.2 key guard). The in-world self-test with ReShade loaded and RTGI in the preset is the
acceptance: PASS means the exact codes reached the readback.

### 4.2a [DEFERRED — v1.1, NOT IN v1] Hard bypass in the ReShade fork (Codex R3 P0-1, R4 P0-1..P0-3)
**Status (D26 decided 2026-10-05): v1.1 spec only — nothing in this section is implemented, built,
deployed or reviewed in v1.** v1 ships the soft bracket + key guard (§4.2) on the stock
deployed DLL. This section is the exact design for the follow-up, rebased onto 6.8.0 per §4.2b
option (a); line numbers below are from the 6.7.3 tree at `I:\reshade-SL` and are re-derived on the
6.8.0 port.

Why it is still wanted: PROVES (I:\reshade-SL\source\runtime.cpp) `render_effects` returns early only
if Effects are off **and** no effect is `addon`-flagged (3701); uniform updates (3716) and the technique
loop (3923) keep rendering `addon` effects while `_effects_enabled == false`. The v1 guard *refuses* in
that case; the fork bypass would *key anyway* and also hide the overlay/OSD (D25), with nothing to
restore. Fact check (Codex R3/R4): no statement in the 6.7 tree assigns `effect.addon = true`;
upstream v6.8.0 still declares `bool addon = false` (https://github.com/crosire/reshade/blob/v6.8.0/source/runtime_internal.hpp, read by WebFetch 2026-10-05; there is no local 6.8.0 checkout); whether
6.8.0 assigns it is **unverified** (§14 R5-A).

Fork change (all additive; each file named with its real declaration site):
```
// source/runtime.hpp (class runtime, declared at :29) — public:
void sl_request_present_bypass(uint32_t mask) { _sl_present_bypass = mask & SL_BYPASS_VALID; }
// source/runtime.hpp — private:
uint32_t _sl_present_bypass = 0;      // SL_BYPASS_EFFECTS 0x1, SL_BYPASS_OVERLAY 0x2
bool     _sl_overlay_hidden = false;  // this present only; read by draw_gui()
bool     _sl_overlay_was_hidden = false; // previous present; edge detection in draw_gui()

// source/runtime.cpp, on_present() — ABSOLUTE ENTRY, before `if (!_is_initialized) return;` (586-589):
const uint32_t sl_bypass = _sl_present_bypass; _sl_present_bypass = 0;   // consumed even if not initialised
_sl_overlay_hidden = (sl_bypass & SL_BYPASS_OVERLAY) != 0 && !_is_vr;  // VR path never hides
...
if (!(sl_bypass & SL_BYPASS_EFFECTS) && !is_loading() && !_techniques.empty()) { ...render_effects... }  // condition at 631
...
_is_vr ? draw_gui_vr() : draw_gui();     // 654-657: ALWAYS called (R4 P0-1) — never skipped
_block_input_next_frame = false;          // 662: unchanged, therefore still consumed every present

// source/runtime_gui.cpp, draw_gui() (755):
//  - 819-821 (`_ignore_shortcuts = false; ...`) runs unchanged every present.
//  - early-out branch 823-836: if _sl_overlay_hidden, the three block_* calls at 831-833 pass false.
//  - input feed 848-…: if _sl_overlay_hidden, feed a NEUTRAL frame — mouse position
//    (-FLT_MAX,-FLT_MAX), every mapped key and mouse button reported UP, no wheel, no characters — so
//    invisible widgets cannot be clicked or typed into; on the first hidden present
//    (_sl_overlay_hidden && !_sl_overlay_was_hidden) call ImGui::ClearActiveID() so
//    `_ignore_shortcuts |= IsAnyItemActive()` (1483) cannot stay latched by a text box that was active
//    when keying began.
//  - ImGui::Render() (1486) runs unchanged.
//  - input-block block 1488-1497: if _sl_overlay_hidden, call block_mouse_input(false),
//    block_keyboard_input(false), block_mouse_cursor_warping(false) — capture explicitly RELEASED
//    while hidden, whatever _show_overlay / _block_input_next_frame / WantCapture* say.
//  - draw-data submission 1499-1517: skipped iff _sl_overlay_hidden. This is the ONLY thing suppressed.
//  - end: _sl_overlay_was_hidden = _sl_overlay_hidden.
//  The overlay hotkey at 767-768 still works while hidden (the overlay opens invisibly and appears when
//  the key goes off); `_show_overlay` and every persisted GUI setting are untouched.

// source/opengl/opengl_hooks_wgl.cpp, around 272-276 (the GL raw present + present_effect_runtime pair):
reshade::sl_presenting_scope sl_scope(this);    // sets thread_local t_sl_presenting_swapchain = this;
reshade::invoke_addon_event<reshade::addon_event::present>(g_opengl_context, this, nullptr, nullptr, 0, nullptr);
reshade::present_effect_runtime(this);           // scope dtor restores the previous value (nullptr)

// include/reshade.hpp, inside the RESHADE_API_LIBRARY block beside ReShadeGetBasePath (25-48):
RESHADE_API_LIBRARY_DECL uint32_t ReShadeSLGetBypassABI();
RESHADE_API_LIBRARY_DECL bool     ReShadeSLSetPresentBypass(reshade::api::swapchain *swapchain, uint32_t mask);

// source/addon.cpp, inside the existing `#if defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON` (6):
uint32_t ReShadeSLGetBypassABI() { return 2; }   // 2 = swapchain-validated API (v6); 1 was never shipped
bool ReShadeSLSetPresentBypass(reshade::api::swapchain *swapchain, uint32_t mask)
{
    if (swapchain == nullptr || swapchain != reshade::t_sl_presenting_swapchain) return false;  // wrong thread,
                                                       // outside raw present, stale or foreign pointer → refused
    const auto rt = swapchain->get_private_data<reshade::runtime>();   // ReShade's own lookup, runtime_manager.cpp:76
    if (rt == nullptr) return false;
    rt->sl_request_present_bypass(mask);
    return true;
}
```
Validation argument (R4 P0-2): the only accepted pointer is the swapchain that is presenting **on the
calling thread right now**; it cannot be destroyed during its own present on that thread, and the
runtime lookup is the one `present_effect_runtime` performs two statements later
(runtime_manager.cpp:74-78), so the request lands on exactly the runtime that consumes it. No registry,
no lock. Other APIs (DXGI, Vulkan, D3D9, OpenXR/VR) never set the scope → the export returns `false`
there (fail closed; the add-on falls back to the guard). The 6.7 `dll_main.cpp` placement and
`runtime_manager::find` of v5 are withdrawn (R4 P0-2/3).

Semantics: one present only; consumed at absolute entry even for an uninitialised runtime; written only
on the presenting thread inside the scope; `_effects_enabled`, preset, ini and `_show_overlay` untouched;
a crash or unload of our add-on leaves at most one bypassed present. `SL_BYPASS_OVERLAY` comes from
`ActorKeyHideReShadeOverlay` via `SLRESHADE_FLAG_ACTOR_KEY_HIDE_OVERLAY 0x20u` (v1.1, D25).

Add-on side (v1.1): resolve `ReShadeSLSetPresentBypass`/`ReShadeSLGetBypassABI` once with
`GetProcAddress(reshade::internal::get_reshade_module_handle(), …)` (the pattern of reshade.hpp:137-139);
require `GetBypassABI() == 2`; in the raw `present` handler call it with the event's **own** `swapchain`
argument — the device+hwnd runtime match is not needed on this path. `true` → no bracket opened, the
guard still counts techniques (expected 0) and reports; `false` → the v1 bracket runs for that present.
`caps |= SLRESHADE_READER_CAP_HARD_BYPASS 0x4` (0x2 is `CAP_KEY_GUARD`, §4.3).

DLL build/deploy and depth pin: §4.2b option (a).

### 4.2b ReShade release provenance (Codex R4 P1-5) — decision D26: DECIDED (c) for v1
Facts (2026-10-05):
- PROVES deployed: `I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\opengl32.dll` — file
  version 6.8.0.2155, product version "6.8.0" (no "UNOFFICIAL"), company "crosire", not Authenticode
  signed, SHA-256 `0CEE63F9…814B94F7` = the depth pin's expected hash (sl_depth_pin.hpp:47, 70-73).
  INFERENCE: an upstream release build of 6.8.0, not built from our fork. Generic Depth is compiled into
  that DLL (the pin hashes the ReShade module itself, sl_depth_pin.hpp:32-34, and cites upstream
  v6.8.0 `examples/09-depth`).
- PROVES fork: `I:\reshade-SL` is upstream tag `v6.7.3` + 2 commits (`4fbedce9` GL R10G10B10A2 detection,
  `d13926f9` `<intrin.h>` build fix), branch `fix-opengl-10bit-stable`, `res/version.h` 6.7.3.3
  "UNOFFICIAL"; no v6.8 tag fetched locally (remote `upstream` = crosire/reshade is configured).
  (Unrelated install: `C:\Program Files\AlchemyBeta\opengl32.dll` is a 6.7.3.2 fork build.)

| Option | Work | Effort | Risk |
|---|---|---|---|
| **(a)** Port the bypass onto exact 6.8.0 | `git fetch upstream tag v6.8.0`; branch `sl/6.8.0-keybypass` from the tag; record the tag commit SHA in `reshade-addon/README.md`; cherry-pick `4fbedce9`/`d13926f9` only if still needed; apply §4.2a; regenerate `res/version.h` with a distinct SL identity (file version 6.8.0.<SL build>, never 2155; product "6.8.0 SL") so the pin can tell the binaries apart, and turn `check_binary` into a two-entry table: the stock 2155 entry stays (reverting the DLL keeps the pin working) plus the SL build's version + SHA-256; verify GD `Prefix {selected, override_depth, srv}` (sl_depth_pin.hpp:25-26) against the **shipped** GD: (1) the v6.8.0 GD source at the recorded commit, (2) the shipped 2155 DLL by a read-only probe build of the add-on that logs the three fields on the current DLL, (3) the same probe on the rebuilt DLL — all three must agree; `deploy_reshade.ps1` | ~1.5-2 days incl. build-env bring-up for the 6.8 tree and a full in-world depth/RTGI regression | Medium: our compiler/flags differ from crosire's release build (behaviour drift is possible and everything ReShade does — RTGI, GD, overlay — becomes ours to regress); depth-pin hash rotates every rebuild; must be re-ported at every ReShade upgrade |
| **(b)** Migrate the fork to 6.8 wholesale | Merge/rebase the fork's `main` onto upstream 6.8.x (latest), adopt it as the SL ReShade line, then the same depth-pin and bypass work as (a) | ~2-3 days (the fork is only 2 commits ahead of v6.7.3, so the merge itself is small; the cost is a moving target — "6.8.x latest" ≠ what is tested today) | Medium-high: changes the ReShade version the user runs today (not just a rebuild of it); upgrades GD and the overlay at the same time; no exact-provenance anchor unless pinned to a tag, at which point it *is* (a) |
| **(c)** No fork DLL in v1 | Soft bracket + key guard (§4.2) on the stock 6.8.0.2155; `effect.addon` assignments in 6.8.0 checked by grep on the fetched tag as a *diagnostic* only, because the guard detects the behaviour at runtime and refuses with the red "add-on effects present" status | ~0.5 day on top of WP4 (one event handler, one viewer export, the state machine, tests) | Low: no binary change, depth pin untouched; residual: the ReShade overlay/OSD/splash cannot be hidden (amber warning), one un-keyed effects-off frame at key-on, one tinted frame if an `addon` effect appears mid-keying, and a user with such an effect cannot key until they disable it |

**Decision (user, 2026-10-05): (c) for v1; (a) is the v1.1 follow-up; (b) rejected. D26 closed.**
Reasoning recorded: the user's machinima install has no
add-on effects today (our bridge registers none; iMMERSE and `SL_GBufferProvider.fx` are ordinary
shader files), so (c) meets the v1 guarantee statement in the user's real setup and refuses in any other — the guard turns
the unknown (`effect.addon` in 6.8.0) into a visible refusal instead of a tinted key. (a) is the better
end state (keys even with `addon` effects, hides the overlay, nothing to restore) but it is a different
binary to own and regress, and it should not gate the key feature. (b) is not recommended: it bundles a
ReShade upgrade with a feature. Trigger for doing (a): the user wants the overlay hidden while keyed,
or the guard ever refuses in practice.

### 4.3 Add-on build, deploy and the stale-binary handshake (D6)
Memory `reshade-gbuffer-stale-addon`: a stale deployed `.addon` once produced black albedo and broken
specular that looked like a viewer bug. The handshake makes a stale add-on **visible**, the deploy step
makes it unlikely:
1. Build: `cmake -S reshade-addon -B reshade-addon/build -G "Visual Studio 18 2026" -A x64` then
   `cmake --build reshade-addon/build --config Release` (README.md:99-106). Codex never builds; Claude
   does, as for the viewer. CMake embeds in the add-on: `SLRESHADE_ABI_VERSION`, the git short SHA, a
   build stamp and **`SL_ADDON_SOURCE_SHA256`** (manifest hash below), logs them at
   `init_effect_runtime` (`[SLBridge] build <stamp> commit <sha> src <hash8> abi 1.3 caps 0x3`) and
   reports them through `SLReShade_ReportReader(const SLReShadeReaderInfo*)` (§4.1).
   **Hash manifest (Codex R2 P2-11), one implementation in `cmake/SLAddonSourceHash.cmake` included by
   BOTH `reshade-addon/CMakeLists.txt` and the viewer's `indra/newview/CMakeLists.txt`:**
   - File set: every regular file under `reshade-addon/src/` with extension `.cpp`, `.hpp`, `.h`, `.inl`
     (case-insensitive), plus `reshade-addon/CMakeLists.txt`, plus **`indra/newview/llreshadebridgeabi.h`**
     — the header the add-on actually compiles (bridge.cpp:71 includes `../../indra/newview/llreshadebridgeabi.h`
     directly; there is no vendored copy). Excluded: anything under `reshade-addon/build*`, `external/`,
     and generated files (`sl_bridge_build_info.h`, written by configure into the build tree, never the
     source tree).
   - Ordering: relative paths from the repo root, forward slashes, encoded as UTF-8, sorted by byte-wise
     ordinal comparison (`list(SORT ... COMPARE STRING CASE SENSITIVE)` on the normalised strings).
   - Per file: strip a leading UTF-8 BOM, normalise CRLF and lone CR to LF, no trailing-whitespace
     changes; the hashed stream is `path "\n" content "\0"` for each file in order; the result is
     SHA-256 (`string(SHA256)` over the concatenation read with `file(READ ... HEX)` → decoded, or
     `file(SHA256)` over a generated normalised temp file — the module picks one and both callers use it).
   - Both configure steps print the hash and the manifest file list, so a mismatch can be diagnosed by
     diffing the two printed lists.
2. Deploy: copy `sl_reshade_bridge.addon` next to the ReShade DLL / `AddonPath` (README.md:121-122;
   note README still says `firestorm-bin.exe` — update to `AlchemyTest.exe`). A `scripts/deploy_addon.ps1`
   compares SHA-256 of built vs deployed before and after the copy and prints both; a mismatch after copy
   (file locked by a running viewer) is an error, not a warning.
3. Runtime check, both directions, three tiers (P2-14):
   - **ABI**: the viewer compares `mReaderAbi` (from `GetFrame`) with its own `SLRESHADE_ABI_VERSION`;
     older minor → **red** (§4.1 item 3) + one-shot notification on key toggle-on; newer MAJOR → the
     add-on already refuses (bridge.cpp:415-426). The add-on compares the viewer's `f.abi_version`
     minor: < 1.3 → logs once "viewer predates actor-key flag; bypass unavailable" (a 1.2 viewer leaves
     the bit zero anyway).
   - **Source hash (the real stale check)**: the viewer's CMake runs the same manifest module at
     *viewer* configure and embeds the result as `SL_EXPECTED_ADDON_SOURCE_SHA256`. `ReportReader`
     delivers the add-on's hash (`source_sha256[32]`); mismatch → **red**
     "ReShade add-on binary is stale (built from different bridge sources): run deploy_addon.ps1". This
     catches a same-1.3 binary built from older or locally edited sources, which stamps alone cannot.
   - **Commit**: differing git SHA with equal source hash → informational only (the add-on sources did
     not change between those commits).
   Stated limit: the expected hash is captured at viewer *configure* time; a source edit to the add-on
   after the last viewer configure makes a correct new add-on read as stale until the viewer is
   reconfigured (the deploy script prints both hashes so this is obvious). Hashing the deployed *binary*
   is not attempted: the viewer cannot know the add-on's install path.
   Caps (v6): `SLRESHADE_READER_CAP_KEY_BYPASS 0x1` (soft bracket), `SLRESHADE_READER_CAP_KEY_GUARD
   0x2` (render-technique guard + `ReportKeyGuard`), `SLRESHADE_READER_CAP_HARD_BYPASS 0x4` (v1.1 only,
   set iff `ReShadeSLGetBypassABI() == 2`). A v1 add-on logs `caps 0x3`. The viewer runs the ARMING
   state only when `CAP_KEY_GUARD` is reported; a reader without it is "older than this viewer" (red).
   The add-on also logs the host DLL identity once at init (`ReShadeVersion` export + module file
   version + whether the depth pin's binary gate passed), so ReShade.log records which ReShade the key
   was verified on.
4. In-world verification (checklist §11): ReShade.log shows the build/commit/source-hash line with
   `caps 0x3`, the host-DLL line (`6.8.0.2155`, depth pin eligible), and `bracket first open / last
   close` edge lines around each key toggle; the Lightbox status shows "key bypass active"; self-test
   PASS with RTGI enabled in the preset; the guard logs CLEAN at arming.
5. **Fork DLL — v1.1 only (§4.2a, §4.2b option (a)); nothing in this item is built or deployed in v1.**
   Build the 6.8.0-based SL branch (its own solution, Release x64); deploy `opengl32.dll` next to
   `AlchemyTest.exe` with `deploy_reshade.ps1` (same SHA-256 before/after discipline as the add-on
   script, and it keeps `opengl32.stock-6.8.0.2155.dll` for one-step revert). In the same deliverable
   extend `depth_pin::check_binary` (sl_depth_pin.hpp:30-78) to a two-entry table (stock 2155 + the SL
   build's version and SHA-256) and verify the GD `Prefix` three ways (§4.2b); otherwise the depth pin
   silently disables itself. Status line: "ReShade DLL <ver> (SL bypass ABI 2)".

### 4.4 Snapshots while keying (v9, Codex R7 P0-1; v10 = Codex R8 P0-1 / P1-3)
What holds (PROVES; R8 confirmed the routing): save, thumbnail, postcard and inventory snapshots all
converge on `rawSnapshot`; 360 uses `simpleSnapshot`; Prism and cube auxiliary renders are intentionally
unkeyed. `LLPipeline::sRenderDeferred` is always true (llappviewer.cpp:624) and `display()` resets its
**local** `for_snapshot` to false (llviewerdisplay.cpp:457-462), so a normal snapshot render runs
`render_ui()` → `renderFinalize()` (1747) → `gatherFrame()` (1751).
What v9 got wrong (R8 P0-1): "never presented" is **not** guaranteed by `gDisplaySwapBuffers = false`.
`display()` presents directly and returns early on a resized window (437-443); start-up presents
directly (201-212, 573); and on the disconnected path the first `swap()` (696-700) resets
`gDisplaySwapBuffers = true` (1841), so the later `swap()` at 1154-1158 of the same snapshot render
presents. v10 therefore replaces the premise with an enforced **snapshot completion contract**.

**Contract (v10).** `rawSnapshot` opens an `ALActorChromaKey::SnapshotScope` whenever `ActorKeyEnabled`
is on (RAII; closes on every return path):
1. **No present, anywhere, while the scope is open.** `LLWindowWin32::swapBuffers()`
   (llwindowwin32.cpp:3925-3929) — the single place this viewer calls `SwapBuffers`, reached by all
   three call sites in llviewerdisplay.cpp (212, 443, 1839) and by llwindowwin32.cpp:1924 — checks a
   process-wide `LLWindow::sPresentBlocked` (`std::atomic<bool>`, set and cleared by the scope). While
   set it does **not** call `SwapBuffers`, increments `LLWindow::sBlockedPresents` and returns. No
   present means no ReShade `present` / `present_effect_runtime` for that render, by construction rather
   than by reasoning about `gDisplaySwapBuffers`.
2. **Readback only after a completed keyed finalize.** Before each `display()` of a tile/pass the scope
   records `ALActorChromaKey::sFinalizeSerial`. `renderFinalize()` on a `gSnapshot` render, after the key
   blit, increments `sFinalizeSerial` and records `sLastFinalizeKeyed = key_now` and the render's target
   size. `rawSnapshot` performs the COLOR `glReadPixels` (llviewerwindow.cpp:5824) for that tile **only
   if** the serial advanced, `sLastFinalizeKeyed` is true and the size matches the tile. Otherwise —
   resize early-return (no finalize), disconnected path, key suspended (fisheye / diopter / RLVa sphere /
   stale matte / missing program), or any other incomplete render — the snapshot is **refused**:
   `rawSnapshot` returns false through its existing failure path, the log prints
   `ACTORKEY SNAPSHOT-REFUSED <reason>` and the user gets a notification "Snapshot not taken: the keyed
   frame could not be completed (<reason>). Try again." A keyed-mode snapshot is never silently unkeyed.
2b. **Refusal latch (v11, Codex R9 P1-2).** The live preview re-requests a snapshot from idle while it is
   stale (llsnapshotlivepreview.cpp:733-784), so a bare refusal would re-render, re-log and re-notify
   every idle cycle. `ALActorChromaKey` therefore keeps one `SnapshotRefusalLatch` { reason, key revision
   (`revision()` of §6: enabled/mode/colour/subjects/…), requested width × height, window size, connected
   flag, suspension reason } set on the first refusal. While latched:
   - **v12 rule (Codex R10 item 1): every automatic request returns false immediately while latched**
     (live-preview idle refresh, auto-snapshot) — no render, no log line, no notification — **unless a
     clearing event has occurred since the latch was set**. The recorded tuple is *not* compared for
     equality; it exists only to detect the clearing events below. The preview keeps a placeholder
     "Keyed snapshot unavailable: <reason> — press Refresh" instead of the stale image;
   - the **only** clearing events (each clears the latch and allows exactly one new attempt): an
     **explicit user action** (Refresh, Save/Upload/Send button, snapshot floater re-opened); the key
     toggled or `revision()` changed; a change of the **requested** snapshot size; a window-size change
     **observed with `gWindowResized == false`** (the resize has completed); `gDisconnected` going
     true → false (reconnect); the recorded suspension reason becoming **none** (e.g. fisheye off with no
     other suspension);
   - **non-clearing changes** — the suspension reason changing from A to B (both non-none), a
     disconnect (`gDisconnected` false → true), window-size movement while `gWindowResized` stays true,
     any other input change — **update the latch silently**: the stored reason/tuple is overwritten (so
     the placeholder text shows the current reason and later clearing events are detected against the
     current state), with no render, no log line and no notification;
   - the trigger is passed explicitly: the snapshot floater/live preview opens an
     `ALActorChromaKey::SnapshotRequestScope(explicit = true)` around the button handlers and
     `(explicit = false)` around its idle refresh; `rawSnapshot` reads it (default = explicit, so other
     callers keep today's behaviour);
   - log and notification fire **once per latch** (a new latch after a cleared one may notify again).
3. **Guard isolation (unchanged from v9).** `keyGuardAllows()` is true on `gSnapshot` renders (with
   presents blocked, ReShade cannot touch them); `gatherFrame()` publishes no KEY/ARM flag, does not
   capture `mGenAtPublish`, does not become the previous bracketed frame and does not change
   `mArmPresents` or the heartbeat.
4. **360** (`simpleSnapshot`, llfloater360capture.cpp:578): not keyed (`SuppressScope`, §9 item 9); no
   contract needed because nothing keyed is read back.
5. **Tiled high-res**: each tile must satisfy 2 independently; feather seams at tile joins remain a
   documented limit (tiling occurs only when the buffers cannot be reallocated, llviewerwindow.cpp:5679).

**Scope of snapshot exactness (v10, R8 P1-3).** The exact-codes claim for snapshots applies only to:
- `SNAPSHOT_TYPE_COLOR` captures — DEPTH24 snapshots read depth (llviewerwindow.cpp:5831-5858), not the
  keyed colour;
- with **no snapshot filter** — a selected filter is applied to the preview image afterwards
  (llsnapshotlivepreview.cpp:968-974) and changes the codes;
- saved in a **lossless** format at the rendered size: PNG, BMP and lossless AVIF keep the codes; JPEG,
  lossy WebP/AVIF, J2C texture/inventory uploads and postcards (JPEG) do not, nor does any rescale (edge
  pixels mix; interior key pixels survive only lossless, unscaled saves).
Outside that scope the snapshot is still keyed by the viewer, but the exact-codes promise does not
apply; the snapshot floater shows "Keyed snapshot: exact colour only for unfiltered COLOR saves in
PNG/BMP/lossless AVIF" when the key is on.

---

## 5. Keeping actor shadows in mode 1 (D5, v1)

In mode 1 every set pixel becomes the key, so the actor's shadow on the floor and its contact shading
disappear with the set. v2 keeps the **sun/moon shadow cast by keyed actors** as a darkening of the key
colour along the key's own hue, so a keyer reads it as a soft shadow layer.

### 5.1 Feasibility verdict (honest)
- **Sun/moon cascades: feasible, with one known ambiguity.** The viewer has four directional cascade
  depth maps (`mRT->shadow[0..3]`, pipeline.cpp:1885-1908) generated by `renderShadow()` per cascade
  (28073-28107), and a **per-draw caster filter already exists**: `LLPipeline::sShadowDrawFilter` with
  `skip_shadow_draw()` (lldrawpool.cpp:429-442; value 2 = attachments only, 1 = non-attachments) plus the
  `attach_only` branch in `renderAlphaObjects` (pipeline.cpp:11926). Avatar bodies enter through
  `LLDrawPoolAvatar::renderShadow` (lldrawpoolavatar.cpp:480-510), a pool-level entry with an avatar
  pointer in hand. So "render a cascade with **only keyed actors as casters**" is a new filter value plus
  owner tests in every shadow draw loop, reusing the existing shadow *draw* code. PROVES for the hook
  points. What cannot be reused as-is is `renderShadow()` itself: every call runs `stateSort`
  (26412-26417), which appends draw infos through `sCull->pushDrawInfo` (5324-5336) without clearing the
  `LLCullResult` (Codex R1: llspatialpartition.cpp:3978-4012), so a second call on the same result
  duplicates every render map and alpha group. v3 therefore **splits** `renderShadow` into a
  prepare/sort phase run once and a draw-only phase run up to three times from one immutable sorted
  result (§5.2). The v2 re-entry implementation is dropped.
- **Separating actor-cast from set-cast shadow is exact only with a set-only map.** A pixel is in
  shadow in the beauty frame iff *something* occludes it. From an actor-only map we know whether a keyed
  actor occludes it; from the full map we know whether anything does. We do **not** know, from those
  two, whether the set *also* occludes it (actor on a balcony, pixel under the balcony floor). Two
  honest modes:
  - **Fast (default):** `s_actor = (1 − vis_actorMap)`; darken wherever a keyed actor occludes.
    Wrong only where the set independently shadows the same pixel and the actor is also in the sun's
    line: the key darkens there although the beauty frame shows no extra shadow. Rare in a lit set,
    harmless for a keyer (it is darker key either way), listed as a known limit.
  - **Exact (opt-in, costs a second full scene shadow pass per cascade):** also render a **set-only**
    cascade (filter = everything except keyed actors) and use `s_actor = (1 − vis_actorMap) × vis_setMap`
    — shadow only where the set would have been lit. This is the correct decomposition; its cost is the
    whole shadow pass again (§5.6).
  - **All-shadows fallback:** `s = (1 − vis_fullMap)`: every sun shadow, actor and set, darkens the key.
    Zero extra passes (reads the maps already generated). Not isolation, but useful when the set is
    flat and open, and the guaranteed fallback if the filtered pass misbehaves on the user's driver.
- **Projector / Cine-rig spot shadows: not in v1.** Isolating actor casters there needs an actor-only
  depth pass per shadowed spot light (`generatePrismSpotShadows` and the spot tiers at 28172-28250 and
  28346-28376), multiplying cost by the number of shadowed spots; the actor-only light feature already
  restricts those slots. v1 handles sun/moon only; the UI says so.
- **Contact/ambient occlusion of the actor onto the set: out of scope** (SSAO/bounce terms live in the
  lit scene and are not separable).
- **Exact alternative with zero engineering** remains the two-pass workflow: take A = mode 1, take B =
  mode 2 (set with key holes, every shadow intact), composite in post (Flycam take / Ghost AutoAnimate
  for repeatability). Documented in the user guide beside the new option.

### 5.2 Data source and passes
Settings: `ActorKeyShadow` (bool, default **true**), `ActorKeyShadowIsolation` (0 fast, 1 exact,
2 all-shadows; default 0), `ActorKeyShadowStrength` (0..0.85, default 0.5), `ActorKeyShadowCascades`
(1..4, default 2), `ActorKeyShadowSoftness` (0..1, default 0.5: PCF kernel scale).

Requires `RenderShadowDetail > 0` and the main view's sun shadows generated this frame
(`mRT->shadowMeta` serial advanced); otherwise the UI says "Sun shadows off — actor shadow unavailable"
and the matte's G channel is 0.

**The shadow split with a state-lifetime contract (Codex R1 P0-3, R2 P0-2).** PROVES: today
`renderShadow()` sets `sShadowRender = true` and zeroes `sUseOcclusion` at entry (26376-26380) and
restores `sUseOcclusion`, clears `sShadowRender` and `sAlphaCapCounts.active` at exit (26682-26685).
v4 makes that lifetime an RAII object that encloses all draws of one cascade:
```
class LLPipeline::LLShadowCascadeScope   // non-copyable; one per cascade; lives on the stack in generateSunShadow / renderShadow
{
public:
    LLShadowCascadeScope(LLPipeline& p, LLCamera& shadow_cam, LLCullResult& result,
                         bool do_cull, bool alpha_cap_active, bool depth_clamp, bool cull_faces);
        // = today's prefix 26363-26417, ONCE: cap_debug/sAlphaCapCounts setup, sShadowRender = true,
        //   mSavedOcclusion = sUseOcclusion; sUseOcclusion = 0;
        //   mCull.emplace(GL_CULL_FACE, cull_faces ? ENABLED : DISABLED); mClamp.emplace(depth_clamp ? GL_DEPTH_CLAMP : 0);
        //   mDepth.emplace(GL_TRUE, GL_TRUE, GL_LESS);                      // the three guards of 26400-26407 as MEMBERS (Codex R3 P0-3)
        //   [updateCull]; stateSort(shadow_cam, result)
    enum class Target { BOUND /*caller owns bind/clear/flush*/, OWN /*draw binds, sets viewport, clears, flushes*/ };
    bool draw(const glm::mat4& view, const glm::mat4& proj, Target mode, LLRenderTarget* target, const ShadowDrawOptions& o);
        // = today's body 26461-26680. Per-draw RAII inside draw():
        //   struct MatrixScope  { ctor: 26462-26467 push proj/modelview + load;  dtor: 26676-26680 pop both, gGLLastMatrix = NULL; }
        //   struct TargetScope  { only for Target::OWN: ctor: target->bindTarget(); target->getViewport(gGLViewport); target->clear();
        //                          dtor: target->flush(); }                     // flush on every exit incl. early return
        //   then: simple/rigged loops, [doOcclusion if o.occlusion_queries], renderGeomShadow, alpha passes. Returns false on any bail-out.
        //   o = { include_rigged, include_alpha_blend, include_alpha_cutout, bool occlusion_queries, bool alpha_cap_accounting }
        //   (depth_clamp / cull_faces are scope-level because the guards must outlive every draw)
    ~LLShadowCascadeScope();
        // = today's exit 26682-26685 on EVERY exit path (normal, early return, exception):
        //   member guards destroyed (cull/clamp/depth restored); sUseOcclusion = mSavedOcclusion; sShadowRender = false;
        //   sAlphaCapCounts.active = false; llassert(sShadowCasterFilter == CASTER_ALL)
private:
    std::optional<LLGLState>     mCull;     // GL_CULL_FACE enabled/disabled per cull_faces
    std::optional<LLGLEnable>    mClamp;    // GL_DEPTH_CLAMP or 0
    std::optional<LLGLDepthTest> mDepth;    // GL_TRUE, GL_TRUE, GL_LESS
    S32 mSavedOcclusion; ...
};
void LLPipeline::renderShadow(view, proj, cam, result, depth_clamp, do_cull, include_rigged, include_alpha_blend, cull_faces, include_alpha_cutout)
{   // thin wrapper, BOUND mode: every existing caller keeps owning its target exactly as today
    LLShadowCascadeScope s(*this, cam, result, do_cull, !include_alpha_cutout, depth_clamp, cull_faces);
    s.draw(view, proj, LLShadowCascadeScope::Target::BOUND, nullptr, {include_rigged, include_alpha_blend, include_alpha_cutout, /*occlusion*/true, /*accounting*/true});
}
```
**Target modes (Codex R3 P0-2).** PROVES: the rain occlusion caller binds and clears
`mWeatherRainOcclusion` **before** `renderShadow` (pipeline.cpp:26223-26228), reads the depth back with
`glReadPixels` while the target is still bound (26292-26296) and flushes **after** (26321); the spot
callers (28375, 28796) and the beauty cascade (28073-28107) likewise own their bind/clear/flush. A
`draw()` that bound, cleared and flushed its own target would break the rain readback (flushed before
the read) and double-clear every caller. So: `Target::BOUND` = the caller owns the target, `draw()`
touches neither FBO nor viewport (today's behaviour; used by the `renderShadow` wrapper and by the
**beauty** cascade draw, which keeps the existing bind/viewport/clear/flush at 28073-28107 untouched);
`Target::OWN` = `draw()` binds, sets the viewport from the target, clears depth, draws and flushes via
`TargetScope` — used **only** by the actor/set draws, whose targets no one else touches. No existing
caller changes mode.

Contract: nothing may call `updateCull` or re-sort between `draw()` calls (the §11 size check and a
debug `llassert(sCull == &result)` in `draw()` enforce it); `doOcclusion` and AlphaCap accounting run
only when the options say so (beauty draw); a failure inside any `draw()` returns `false` after
`MatrixScope`/`TargetScope` unwind, and the enclosing function's early return runs the scope destructor.
`AlphaCapSunLeakCheck` (624-634) keeps asserting at every `generateSunShadow` exit.

Per cascade in `generateSunShadow()` (today's bind/clear at 28073-28075, call at 28092-28093 and flush at
28107 become):
```
LLShadowCascadeScope cascade(*this, shadow_cam, result[j], !have_union_cull, !alpha_casters, /*depth_clamp*/true, /*cull_faces*/true);
mRT->shadow[j].bindTarget(); mRT->shadow[j].getViewport(gGLViewport); mRT->shadow[j].clear();            // unchanged lines
cascade.draw(view[j], proj[j], Target::BOUND, nullptr, beauty_opts);                                       // byte-identical output to today
mRT->shadow[j].flush();                                                                                   // unchanged line
if (key_shadows_ready && j < ActorKeyShadowCascades) {                                                    // targets ensured this frame, see below
    { ShadowCasterFilterScope casters(CASTER_KEY_ONLY); cascade.draw(view[j], proj[j], Target::OWN, &mActorShadow[j], aux_opts); }
    if (isolation == EXACT) { ShadowCasterFilterScope casters(CASTER_SET_ONLY); cascade.draw(view[j], proj[j], Target::OWN, &mSetShadow[j], aux_opts); }
    mActorShadowFrame = gFrameCount; mActorShadowCascades = j + 1;
}
// scope destructor restores here, on every exit
```
`aux_opts` = beauty options with `occlusion_queries = false`, `alpha_cap_accounting = false`. Same
`view[j]/proj[j]` → the cascade's `mSunShadowMatrix[j]` and `mSunClipPlanes` apply unchanged to the
actor and set maps. Alpha-blended hair casts through the same `renderAlphaObjects` path with
`ALPHA_BLEND_CUTOFF`, exactly as in the beauty map.

**Two independent predicates (Codex R2 P1-6, P1-9).** PROVES: with `include_alpha_cutout == false`
the alpha passes nest a `ShadowDrawFilterScope(cap_filter)` (26535-26537, 26612-26614) that forces
attachment-only alpha casters; a single filter byte cannot hold "set only" and "attachments only" at
once — overwriting it with SET_ONLY would bring capped world-alpha casters back, overwriting it with the
cap would draw attachments into the set map. So v4 keeps **two statics**:
- `U8 LLPipeline::sShadowDrawFilter` — the AlphaCap/rain filter, values 0-3, semantics, scopes and
  consumers exactly as today (lldrawpool.cpp:429-442, pipeline.cpp:11926, 26535-26537, 26612-26614).
  **Not added to any new path.**
- `U8 LLPipeline::sShadowCasterFilter` — new: `CASTER_ALL` (0), `CASTER_KEY_ONLY`, `CASTER_SET_ONLY`,
  set only by the RAII `ShadowCasterFilterScope` inside the key draws; always `CASTER_ALL` outside them
  (asserted by the cascade scope destructor and by `AlphaCapSunLeakCheck`'s sibling
  `CasterFilterLeakCheck`).
Predicates: `skip_by_cap(params)` = today's `skip_shadow_draw` body; `skip_by_caster(params)` =
`CASTER_KEY_ONLY → !(maskDrawByte(*p) & keyMaskBits())`, `CASTER_SET_ONLY → (maskDrawByte(*p) & keyMaskBits()) != 0`,
`CASTER_ALL → false`. A draw is skipped if **either** says so. Under SET_ONLY with the cap active, a
capped world-alpha caster stays skipped (cap) and a keyed attachment is skipped (caster) — both
predicates independent, exactly as Codex asked.

**Where each predicate is evaluated (Codex R2 P0-3).** PROVES: the rigged render-map loops —
`pushRiggedBatches` (lldrawpool.cpp:1154-1180), `pushUntexturedRiggedBatches` (1182-1200),
`pushRiggedMaskBatches` (1225-1252), `pushRiggedGLTFBatches`/`pushUntexturedRiggedGLTFBatches`
(2473-2510) and `pushRiggedGLTFBatchesScalar`/`Indexed` (2533-2579) — never call `skip_shadow_draw`;
only the non-rigged loops do (1083, 1123, 1146, 1216, 1276, 2150, 2169, 2197, 2223). The caster
predicate is evaluated **before `uploadMatrixPalette` and the draw** in every one of those rigged
loops, and beside the existing cap test in every loop that has one. The cap predicate is added to **no**
loop that lacks it today: doing so would change behaviour for filters 1-3 (rain / attachment-only), which
must stay bit-identical. Correction (Codex R3 P2-7): `pushMaskBatchesIndexed(type, rigged)` already
evaluates `skip_shadow_draw` for **both** its rigged and non-rigged sweeps (lldrawpool.cpp:1255-1289,
test at 1276 before the `rigged` palette upload at 1285-1290), so that rigged sweep keeps its cap test as
today and gets the caster predicate beside it. The other rigged loops (1154-1200, 1225-1252, 2473-2579)
have no cap test today and get only the caster predicate; that gap is pre-existing, flagged as D23 for a
separate decision.

| Consumer | `skip_by_cap` (today, unchanged) | `skip_by_caster` (new) |
|---|---|---|
| non-rigged loops: `pushBatches`/`pushUntexturedBatches`/`pushMaskBatches`/GLTF scalar+indexed (lldrawpool.cpp 1083-2223 sites) | as today | added beside it |
| `pushMaskBatchesIndexed(type, rigged)` — both sweeps (1255-1289) | **as today, incl. rigged** (1276) | added beside it, before the palette upload (1285) |
| rigged loops without a cap test today: `pushRiggedBatches`, `pushUntexturedRiggedBatches`, `pushRiggedMaskBatches`, rigged GLTF scalar/indexed (1154-1200, 1225-1252, 2473-2579) | **not evaluated (as today; D23)** | added before `uploadMatrixPalette` |
| `renderAlphaObjects` (pipeline.cpp:11926-11975; rigged and non-rigged branches) | `attach_only` as today | added per draw before the program bind/palette |
| `LLDrawPoolAvatar::renderShadow` (body; lldrawpoolavatar.cpp:480-510) | none (as today) | `KEY_ONLY → return unless maskRecipientBit(av) & keyMaskBits(); SET_ONLY → return if it is` |
| `renderGeomShadow` pool loop (terrain, tree, grass, water, materials pools; 7969-8010) | as today | `KEY_ONLY → skip every non-avatar pool; SET_ONLY → draw all` |
| grass / tree passes inside the draw body (26568-26584) | as today | `KEY_ONLY → skip` |
The reviewer greps `skip_shadow_draw`, `sShadowDrawFilter`, `uploadMatrixPalette` and every
`beginRenderMap` loop reachable from the shadow draw body and confirms the table is complete.

**Targets (Codex R1 P1-7, R2 P1-8).** PROVES: `allocateShadowBuffer` runs at initial allocation and
on `resizeShadowTexture()` (pipeline.cpp:1181-1192, driven by `gResizeShadowTexture`), not when a
setting flips. So the key cannot rely on it for the ON transition. v4: a lazy
`bool LLPipeline::ensureActorShadowTargets(bool exact)` runs at the top of `generateSunShadow()` when
`ActorKeyShadow && sActive`, once per frame:
- for `i < ActorKeyShadowCascades`: if `mActorShadow[i]` (and `mSetShadow[i]` when exact) is complete
  with the same dimensions as `mRT->shadow[i]`, keep; else (re)allocate through the **same helper the
  beauty loop now uses**, `allocateSunCascadeTarget(i, target)` — identical per-cascade
  `cascade_width/height` incl. the `BDMergeShadowResolution0-3` overrides (1894-1902), identical
  `allocate(w, h, 0, true)`, then `applySunShadowSamplerState(target, sun_filter)` (the block at
  1954-1976 factored: dynamic `sun_filter` from `shadowUnitsFilterWanted()`, `TAM_CLAMP`,
  `GL_COMPARE_REF_TO_TEXTURE`, `GL_LEQUAL`);
- **rollback**: if any allocation or completeness check fails, release every actor/set target, log
  `ACTORKEY SHADOW-FAIL alloc` once, set the session fallback to All-shadows (§5.7) and return false
  (`key_shadows_ready = false` → no aux draws this frame);
- `applyShadowUnitsFiltering` (22699) iterates the extra targets when they exist.
- **Release on every teardown path (Codex R3 P1-5).** PROVES: full GL teardown goes
  `releaseGLBuffers` → `releaseShadowBuffers` (2344-2346), which releases each pack's `shadow[i]`
  directly (2367-2385) and never calls `releaseSunShadowTargets`. So one `releaseActorShadowTargets()`
  (releases `mActorShadow[0..3]`, `mSetShadow[0..3]`, clears `mActorShadowFrame/Cascades`) is called
  from: `releaseShadowBuffers` (full teardown), `releaseSunShadowTargets` (resize path),
  `resizeShadowTexture`, the OFF transition (`ActorKeyShadow` false or key off, in the setting
  listener's next-frame hook) and the `ensureActorShadowTargets` rollback. They are re-ensured lazily on
  the next keyed frame. A debug counter asserts `LLRenderTarget::sBytesAllocated` returns to its pre-key
  value after OFF (§11).
Main pack only (never hero/probe/Prism packs).

Owner availability (least-sure, §14): `maskDrawByte` reads `mActorFxOwner`/`mActorFxFallbackOwner` on
the draw info; the shadow path reaches draw infos through `LLCullResult` render maps, not
`walkGhostSourceGeometry`. A caster whose owner fields are unset drops out of the actor map (a hole)
and, under filter 5, is drawn as set — never the reverse.

### 5.3 Shadow term in the matte pass
`buildMatte()` (§3.6) gains a second output channel: `mActorKeyMatte` becomes **RG8** (R = coverage
matte, G = actor shadow density). The matte program is compiled with `SUN_SHADOW` and links
`shadowUtil.glsl`, so it calls the stock `sampleDirectionalShadow(pos, norm, pos_screen)`
(shadowUtil.glsl:671) — cascade selection, split blending, bias, offset and the PCF kernel are the
beauty pass's own, so the darkening lands exactly where the beauty shadow would have. Inputs:
`deferredScreen` depth → `pos` via the existing `getPositionWithDepth` path; `norm` from the G-buffer
normals (same decode as `sunLightF`).

**Complete binder (P1-8).** The sun-shadow upload is extracted from `bindDeferredShader` into one
helper used by **both** the deferred light shaders and the matte program, so parity is structural:
```
void LLPipeline::bindSunShadowState(LLGLSLShader& shader, const LLRenderTarget* const maps[4]);
```
It uploads everything shadowUtil.glsl's sun path reads, in today's order: the four cascade samplers
(`DEFERRED_SHADOW0-3`, from `maps[]` instead of `getSunShadowTarget`), `DEFERRED_SHADOW_MATRIX` (all
`MAX_SHADOW_MATS`, 23093-23103), `DEFERRED_SHADOW_CLIP` (23137), `uploadShadowUniforms` (ShadowDist
meta: units/subject/fitted/coef/reuse matrices and `SHADOW_PROBE_MODE`, 22784), `shadow_split_blend`,
`shadow_res_cascade`/`_h`, `DEFERRED_SHADOW_OFFSET`/`BIAS` and the spot bias/offset (23186-23188), the
soft-shadow block (`SOFT_SHADOW_ENABLE/SCALE/MAX/FILL/SUN/VOGEL/TAPS`, 23190-23201),
`DEFERRED_SUN_DIR`/`MOON_DIR` (23216-23217), `DEFERRED_SHADOW_RES` from `maps[0]` (23218-23221),
`SUN_UP_FACTOR`, and `DEFERRED_SUN_WASH`/`SHADOW_NOISE`/`BLUR_SIZE` (23142-23144) if the linked
shadowUtil reads them. `bindDeferredShader` calls it with the beauty maps; the reviewer diffs the full
uniform upload list of the deferred path before and after the refactor (must be identical, same order).
The matte program is compiled with the same defines as the deferred sun path (`SUN_SHADOW`,
`SOFT_SHADOW_VOGEL_MAX`, …) and calls the helper with `mActorShadow[0..n-1]`; cascades beyond `n` get a
cleared 1×1 depth target (samples 1.0 = lit) rather than the beauty map, so they can never add set shadow.
Two draws of one program, swapping the map set:
```
pass A (actor maps):  vis_actor = sampleDirectionalShadow(...)            → G_a
pass B (exact: set maps / fast: skipped / all: full maps mRT->shadow): vis_other
G = isolation==0 ? 1 − vis_actor
  : isolation==1 ? (1 − vis_actor) × vis_set
  :                1 − vis_full
G *= step(0.0, NdotL)                 // back-facing receivers are already dark in the beauty
G *= (1 − R)                           // actor pixels carry no key, so no shadow term
G  = pow(G, 1.0 / max(ActorKeyShadowSoftness, 0.05))  // soft/firm density shaping, default identity-ish
```
Blur: G rides the same half-res blur/upsample as R only when `ActorKeyShadowSoftness > 0.5` (penumbra
softening); otherwise the PCF softness is kept as is.

### 5.4 Keyable darkening in the blit
In `blitWithEffectsF.glsl` (§3.7), mode 0 (actors on key) only:
```
float g = texture(actorKeyMatteMap, uv).g;            // actor shadow density, 0..1
vec3  key_px = key * (1.0 − actor_key_shadow_strength * g);   // uniform scale toward black
diff.rgb = (m >= 1.0) ? diff.rgb : (m <= 0.0) ? key_px : mix(key_px, diff.rgb, m);
```
Why this stays keyable: a uniform scale of the three sRGB code values keeps the **hue angle** and the
HSV saturation unchanged (chromaticity is a ratio of the channels), so hue-based keyers (Keylight,
Ultra Key, Resolve 3D keyer, OBS "Chroma Key" with its similarity/smoothness) see the same colour at
lower luma. What drops is chroma magnitude in YCbCr (Cb/Cr scale with the luma), so a keyer tuned too
tight will release the darkest shadow pixels. Hence the cap: strength ≤ 0.85 (Chroma Green at 0.85 →
0,27,10, still green but near the floor); default 0.5 (0,88,32). Shadow pixels keep exactness in the
same sense as the key (literal arithmetic on the uniform; no later stage) and the self-test reports the
darkest shadow code it finds. Unshadowed key pixels are byte-identical to the no-shadow build (g = 0).
Mode 1 (set only) and mode 2 (matte) ignore G in the colour path; **mode 2 gains `ActorKeyMatteShadow`
(bool, default off)**: when on, the matte writes `max(R, 0.5 × g)` — white actors, grey actor shadow,
black set — the compositor-grade route when the live keyer cannot hold a darkened key.

### 5.5 Risks
1. Darkened key falls outside the keyer's tolerance → dark green remnants in the composite. Mitigation:
   the strength cap, the OBS note (raise Similarity; or use Luma/Matte+shadow route), and the self-test's
   darkest-code report so the user can set the keyer from a number.
2. Fast isolation darkens under set shadow where the actor also occludes (§5.1). Visible as a slightly
   darker key patch inside a set shadow that the beauty frame shows uniformly; the keyer treats it as
   shadow anyway. Exact mode removes it at the cost in §5.6.
3. Cascade count: beyond `ActorKeyShadowCascades` there is no actor map, so distant actor shadows are
   not kept; the matte samples "lit" there. Default 2 covers the subject range of the shadow-distance
   feature (`doc/SPOT_SHADOW_CEILING_DESIGN.md` / ShadowDist); UI shows the chosen cascades.
4. The split refactor touches the beauty shadow path. Its acceptance is byte-identical beauty maps
   (the reviewer reads the moved code as a pure move; the §11 render-map-size check proves the draw
   phase does not mutate the result).
5. Occlusion queries (`doOcclusion`, 26516-26519) run only in the beauty draw (`o.occlusion_queries`);
   `sUseOcclusion` save/zero (26379-26380) is in the `LLShadowCascadeScope` constructor and the restore
   in its destructor, so the actor/set draws see it already zeroed and every exit path restores it.
6. `mShadowMeta`/ShadowDist "units/subject" modes change the cascade layout and sampling path
   (`shadow_units_mode`, `shadow_subject`, shadowUtil.glsl:677-683). Because the matte pass reuses
   `sampleDirectionalShadow` with the same uniforms, it follows whatever mode is active, but the actor
   maps must be generated with the same `view[j]/proj[j]` the meta describes — guaranteed by rendering
   them inside the same cascade iteration.
7. Moon: `sun_up_factor` selects `moon_dir` in the shader (shadowUtil.glsl:686); the maps are the same
   set, so moon shadows work with no extra code.

### 5.6 Cost (measured, not budgeted: Tracy zones `ActorKey::shadowCascade[j]`, `ActorKey::shadowTerm`)
- Fast: one extra draw-only cascade per kept cascade (no second cull or sort). GPU cost is the keyed
  actors' share of the beauty shadow draw, depth-only. Matte: one extra full-screen pass running the
  sun-shadow sampling once. Memory: `n` depth maps at cascade size (2 × 2048² × 32-bit ≈ 32 MB at
  default scale for n = 2).
- Exact: adds one draw-only set cascade per kept cascade — the beauty draw cost again minus the
  actors — and another ≈ 32 MB. Opt-in for that reason.
- All-shadows: no extra draws; the matte pass only.
- Off (`ActorKeyShadow` false or key off): no targets, no passes, the matte's G is never written and the
  blit multiplies by 1 (uniform strength 0).

### 5.7 Fallback ladder (in the UI as one combo, in this order)
Exact → Fast → All sun shadows → Off. If the filtered pass logs `ACTORKEY SHADOW-FAIL` (program
missing, `ensureActorShadowTargets` rollback, caster-filter leak detected by `CasterFilterLeakCheck`),
the renderer drops to All-shadows for the session and the status line says so; the user keeps a usable
shadow layer and the two-pass workflow remains the exact answer.

---

## 6. UI

Home: the Lightbox **"Actor Mask" tab** (`floater_lightbox_settings.xml`, panel `actor_mask_settings`,
host `alfloaterlightbox.cpp`), renamed **"Actor Mask & Key"**. New section above the existing
Actor-only controls, all `control_name`-bound; dynamic rows driven by **settings listeners** (the
Roto Ink pattern at alfloaterlightbox.cpp:222-236 and `updateRotoInkAnimVisibility`), never from
`draw()`.

```
[■] Key actors (chroma key / matte)            ← ActorKeyEnabled      (hotkey F9)
Status: KEYING ON — Actors on chroma green · 3 keyed, 1 impostor not keyed · shadows: fast, 2 cascades
        ReShade: add-on 1.3, key bypass active          (v7: exact wording in §4.1 item 3 — red = key refused: bridge not running, older than this viewer, stale binary, effect pass(es) with Effects off, other add-ons loaded, unverified ReShade build, bridge stopped confirming / faulted / ambiguous runtime; amber: "checking…")
        Key colour is exact except where ReShade's own UI draws, and an add-on effect switched on mid-key may tint ONE frame before keying stops.   (v8 guarantee line, always shown while ReShade is loaded; "Key colour is exact." without ReShade; amber named causes beneath: overlay open / OSD always on / tutorial splash / reload splash)
        [Retry]                                         ← visible only while the key guard is REFUSED (§4.2); re-arms the guard
Mode:        [Actors on key ▼]                 ← ActorKeyMode
Key colour:  [Chroma Green #00B140 ▼] [swatch] ← ActorKeyColorPreset (9 entries, §3.7a), ActorKeyColor (swatch enabled on Custom)
             hex readout "0,177,64"           ← diff-guarded text, so the user can type the same value into the keyer
Matte:       [ ] Invert (white set, black actors)   ← ActorKeyMatteInvert   (row visible in mode 2 only)
Who:         [All avatars ▼]  [■] Clones  [ ] Animesh   ← ActorKeySubjects, ActorKeyIncludeClones, ActorKeyIncludeAnimesh
Edge
  Feather    [====o-----] 1.5 px                ← ActorKeyFeather   (px at 1080p; scales with output height)
  Choke      [-----o----] 0.0 px                ← ActorKeyChoke     (-8..8; + shrinks)
  Edge fill  [Blend to key ▼]                   ← ActorKeyEdgeMode  (0 blend, 1 hard)
  [■] Hair & alpha coverage                      ← ActorKeyAlphaCoverage
Output
  [■] Hide HUD attachments while keying          ← ActorKeyHideHUD
Actor shadows (mode 0; rows hidden in other modes)
  [■] Keep actor shadows on the key              ← ActorKeyShadow      (disabled + reason when RenderShadowDetail == 0)
  Isolation  [Fast (actor map) ▼ | Exact (actor × set) | All sun shadows]   ← ActorKeyShadowIsolation
  Strength   [=====o----] 0.50  → "shadow code 0,88,32"   ← ActorKeyShadowStrength (0..0.85; readout diff-guarded)
  Cascades   [1 | 2 | 3 | 4]  Softness [====o-----] 0.5   ← ActorKeyShadowCascades, ActorKeyShadowSoftness
  [ ] Grey shadow in the matte (mode 2)          ← ActorKeyMatteShadow (row visible in mode 2 only)
  Debug view: [Off ▼ | Overlay | Mask only | Key matte | Actor shadow]   ← CineLightRigActorMaskDebug gains values 3 and 4
  [Self-test]  → logs ACTORKEY SELFTEST PASS/FAIL (reads one key pixel back + the darkest shadow code)
Known limits (text): the guarantee line above (ReShade UI not keyed; one-frame limit for an add-on effect switched on mid-key); impostors/muted/jellydoll/Actor-FX-styled avatars are not keyed; DoF/motion blur
  are not matte-aware; projector/spot shadows are not kept (sun/moon only); Fast isolation can darken
  inside a set shadow the actor also crosses (use Exact); very dark shadow needs a looser keyer
  (raise OBS Similarity) or the Matte + grey shadow route; for exact shadows shoot a second pass in
  Set only mode.
```

Rules applied (CLAUDE.md "UI panels must not do work in draw()"):
- Status line: `ALActorChromaKey` exposes `U64 revision()` bumped when any of {active, mode, preset,
  keyed count, impostor count, reader-seen, matte-stale, program-missing} changes; the panel's `draw()`
  compares the revision at ≤ 2 Hz and calls `setTextIfChanged`. Tracy zone
  `LL_PROFILE_ZONE_SCOPED_CATEGORY_UI` named `ALPanelActorMaskKey::draw`.
- Preset combo → colour: a commit callback (`LightBox.ActorKeyPreset`) writes `ActorKeyColor`; the
  renderer reads only `ActorKeyColor`. Choosing a custom colour in the swatch flips the preset to Custom
  through the colour control's listener (no draw-time work).
- Row visibility: settings listeners on `ActorKeyMode`, `ActorKeyColorPreset`, `ActorKeyEnabled`.
- Layout: the tab is a scroll host touched after 2026-10-05 → rule 6 applies: no hand-typed scroll
  document height; `ALScrollFit::Controller::installForTabs` on the Lightbox tab container (the
  implementer checks whether the Lightbox already installs it; floater_lightbox_settings.xml is **not**
  on the named-exception list).

Making ON obvious:
1. The tab's checkbox label and bold status line.
2. A one-shot notification on toggle-on ("Actor key is ON: the set is now <colour>. F9 or the Lightbox
   Actor Mask & Key tab turns it off." with "Don't show again"), `notifications.xml`.
3. A small badge in `render_ui_2d` (top-right, "KEY ON · Actors on green"), drawn only while the UI
   is visible (`RENDER_DEBUG_FEATURE_UI` and `getUIVisibility()`, the same gate as the composition
   guide, pipeline.cpp:22049-22051), so Ctrl+Alt+F1 removes it for capture. Diff-guarded text.
4. Director console: a "KEY" toggle button bound to `ActorKeyEnabled` next to the existing look toggles
   (decision D8), and `ActorKeyEnabled` is **not persisted** across sessions (default false on start).

Hotkey: **F9** "Toggle actor key" via the existing hook `ALDirectorHotkeys::handleKey`
(aldirectorhotkeys.cpp:34-151; `DIRECTOR_HOTKEYS.md` lists F9 as free headroom). Gate: unmodified,
`DirectorHotkeysEnabled`, and one of Director Console / Actor Mover / Lightbox open (same gate shape as
F3-F8). Update `DIRECTOR_HOTKEYS.md` (F9 reserved).

---

## 7. Settings (`app_settings/settings.xml`)

| Setting | Type | Default | Persist | Range / values |
|---|---|---|---|---|
| ActorKeyEnabled | Boolean | false | **no** | master toggle |
| ActorKeyMode | U32 | 0 | yes | 0 actors on key, 1 set only, 2 alpha matte |
| ActorKeyColorPreset | U32 | 0 | yes | 0 Chroma Green #00B140, 1 Pure Green #00FF00, 2 Chroma Blue #0047BB, 3 Pure Blue #0000FF, 4 Magenta #FF00FF, 5 Black, 6 White, 7 Mid Grey #808080, 8 Custom (§3.7a) |
| ActorKeyColor | Color3 | 0/255, 177/255, 64/255 | yes | display sRGB codes as `code/255`; uploaded unchanged; the UI shows the 0-255 readout |
| ActorKeyMatteInvert | Boolean | false | yes | mode 2 only |
| ActorKeySubjects | U32 | 0 | yes | 0 all, 1 Director cast, 2 actor-only lit, 3 me |
| ActorKeyIncludeClones | Boolean | true | yes | |
| ActorKeyIncludeAnimesh | Boolean | false | yes | |
| ActorKeyFeather | F32 | 1.5 | yes | 0..32 px (at 1080p) |
| ActorKeyChoke | F32 | 0 | yes | -8..8 px |
| ActorKeyEdgeMode | U32 | 0 | yes | 0 blend to key, 1 hard |
| ActorKeyAlphaCoverage | Boolean | true | yes | |
| ActorKeyHideHUD | Boolean | true | yes | |
| ActorKeyShadow | Boolean | true | yes | keep actor sun/moon shadows on the key (mode 0) |
| ActorKeyShadowIsolation | U32 | 0 | yes | 0 fast (actor map), 1 exact (actor × set maps), 2 all sun shadows |
| ActorKeyShadowStrength | F32 | 0.5 | yes | 0..0.85 darkening toward black along the key hue |
| ActorKeyShadowCascades | U32 | 2 | yes | 1..4 cascades given an actor-only map |
| ActorKeyShadowSoftness | F32 | 0.5 | yes | 0..1 density shaping / penumbra blur |
| ActorKeyMatteShadow | Boolean | false | yes | mode 2: grey actor shadow in the matte |
| ~~ActorKeyHideReShadeOverlay~~ | — | — | — | **Not added in v1** (v6, D25 deferred): v1.1 only, with the fork DLL; it then publishes `SLRESHADE_FLAG_ACTOR_KEY_HIDE_OVERLAY` so the fork suppresses only the overlay's draw-data submission on keyed presents (§4.2a) |
| ActorKeyShadowSelfTest | U32 | 0 | no | debug (v13): 1 = run the W-9m Exact-vs-beauty comparison once and log `ACTORKEY SHADOW-EXACT`; renderer resets to 0 |
| ActorKeySelfTest | U32 | 0 | no | 1 = run once; renderer resets to 0 (the `CineLightRigActorMaskTest` idiom) |

Existing: `CineLightRigActorMaskDebug` gains value 3 (Key matte). `CineLightRigActorMaskDisable`
(kill switch) does **not** disable the key: it kills actor-only lighting only. Rationale: the kill
switch exists to make lights behave normally; keying is a different feature with its own master.
`CineLightRigActorMaskBias` is shared (same pass).

Presets/reset: all `ActorKey*` join the Lightbox "reset to default" set; none join Cine Light Rig
presets (they are output settings, not a look).

---

## 8. Files and shaders

New
- `indra/newview/alactorchromakey.h/.cpp` — model + render helper: statics (`sActive`), setting
  listeners, subject gathering (`gatherKeyRecipients()` → list of `LLVOAvatar*`), target ownership
  (`mActorKeyCoverage`, `mActorKeyBlurA/B`, `mActorKeyMatte`, frame stamp), `renderCoverage(pipe)`,
  `buildMatte(pipe)`, `bindForBlit(shader)`, self-test readback, revision, summary counters.
- Shaders (class1): `deferred/actorKeyCoverageF.glsl` (material/PBR alpha → coverage; vertex shaders
  reuse `actorMaskV.glsl` with `HAS_SKIN` twin), `deferred/actorKeyCoverageAvatarF.glsl` (uses
  `actorMaskAvatarV.glsl`), `alchemy/actorKeyConvertF.glsl` (bits → coverage), `alchemy/actorKeyBlurF.glsl`,
  `alchemy/actorKeyMatteF.glsl` (remap/upsample). All full-screen passes use `postDeferredNoTCV.glsl`
  like the debug view (llviewershadermgr.cpp:5273).
- `indra/newview/tests/alactorchromakey_test.cpp` — pure remap math and KEY_BIT allocation.

Modified
- `pipeline.h/.cpp`: `actorMaskWanted()`; the four gate sites (§3.2); call `renderCoverage` after
  `renderActorMask()` in MAIN MASK view; call `buildMatte` at the `endMainView` site; the blit program
  selection + bindings (`ACTOR_KEY_MATTE_MAP`, `ACTOR_KEY_COLOR`, `ACTOR_KEY_PARAMS`); allocate/release
  key targets with the main pack (`allocateScreenBuffer` path near 1465, `releaseScreenBuffers`,
  `releaseActorMasks`).
- `alcinelightrigmanager.h/.cpp`, `alcinelightrigmodel.h/.cpp`: `KEY_BIT`, the key list in the
  rebuild, the mask-byte accessors (`anyMaskRecipient`, `maskRecipients`, `maskRecipientBit`,
  `maskByteForOwner`, `maskDrawByte`, `keyMaskBits`, §3.3); light-side API and `sAnyActorOnly`
  unchanged. `pipeline.cpp` `selectForwardLightSet` + set builder: `& ~KEY_BIT` and assert.
- `llshadermgr.h/.cpp`: three uniform names. `llviewershadermgr.h/.cpp`: ~7 program objects
  (4 coverage, convert, blur, matte) + `gBlitWithEffectsKeyProgram`; unload list entries.
- `app_settings/shaders/class1/alchemy/blitWithEffectsF.glsl`: `#ifdef ACTOR_KEY` block (§3.7);
  bytes of the non-key variant unchanged.
- `llviewerdisplay.cpp`: `render_hud_attachments` gate; badge call in `render_ui_2d`.
- `llviewerwindow.cpp` (v9/v10): `ALActorChromaKey::SuppressScope` around the `display()` loop in `simpleSnapshot` (360 not keyed); v10: `ALActorChromaKey::SnapshotScope` (RAII) around `rawSnapshot`'s tile loop when the key is enabled, the per-tile completed-keyed-finalize check before the COLOR `glReadPixels`, refusal through the existing failure path + `ACTORKEY SNAPSHOT-REFUSED` + notification (§4.4).
- `indra/llwindow/llwindow.h/.cpp`, `llwindowwin32.cpp` (v10): `static std::atomic<bool> LLWindow::sPresentBlocked`, `sBlockedPresents`; `LLWindowWin32::swapBuffers()` (3925-3929) returns without `SwapBuffers` while blocked. `pipeline.cpp` `renderFinalize()`: `sFinalizeSerial` / `sLastFinalizeKeyed` / size stamp on `gSnapshot` renders. `llfloatersnapshot` / `llsnapshotlivepreview`: the exactness-scope note when the key is on, `SnapshotRequestScope(explicit)` around button handlers (true) and the idle refresh (false), the refusal placeholder; `alactorchromakey` `SnapshotRefusalLatch` (v11, §4.4 item 2b). Module-state word (v11): `g_mod_state` in the add-on and `sModState` in the viewer, packed generation|tick, CAS-loop callback (§4.2). `llreshadebridge.cpp` `gatherFrame()`: no KEY/ARM flag and no guard bookkeeping on `gSnapshot` renders.
- `llreshadebridge.cpp`, `llreshadebridgeabi.h`: ABI 1.3 — `SLRESHADE_FLAG_ACTOR_KEY`,
  `SLRESHADE_FLAG_ACTOR_KEY_ARM` (v6), reader abi/caps stamp, `SLReShade_ReportReader` and
  `SLReShade_ReportKeyGuard` (v6, `SLReShadeKeyGuard` 32 bytes) exports (§4.1, §4.3).
- `alactorchromakey.h/.cpp` (v8 additions: own `LdrRegisterDllNotification` generation counter, ReShade re-test before every keyed frame, `mArmPresents` boundary, guarantee-line text; v6/v7 additions): key-guard state machine (OFF/ARMING/KEYING/REFUSED, arming
  epoch, 60-present timeout, Retry), v7 reader states (NO_RESHADE via a cached module scan for the
  `ReShadeRegisterAddon`+`ReShadeUnregisterAddon` exports, RESHADE_NO_READER, READER_NO_GUARD,
  READER_GUARD), v7 heartbeat (last bracketed frame vs last report), `keyGuardAllows()` in `key_now`,
  guard fields (`gui_flags`, `foreign_addons`, `heartbeat_miss`) in the revision and the `ACTORKEY
  SUMMARY` line.
- Shadow (§5): `pipeline.cpp`/`.h` `LLShadowCascadeScope` (renderShadow split), `sShadowCasterFilter`
  + `ShadowCasterFilterScope` + `CasterFilterLeakCheck`, `ensureActorShadowTargets` with
  `allocateSunCascadeTarget`/`applySunShadowSamplerState` shared with the beauty loop, targets
  `mActorShadow[4]`/`mSetShadow[4]`; `lldrawpool.cpp` `skip_by_caster` beside `skip_by_cap` in every
  non-rigged loop and new in every rigged loop (1154-1200, 1225-1252, 2473-2579); `renderAlphaObjects`
  (11926-11975) per-draw caster test; `LLDrawPoolAvatar::renderShadow` owner test; `renderGeomShadow`
  pool gate; a factored `bindSunShadowState(shader, targets[4])` helper; matte program linked with `shadowUtil.glsl`
  (`SUN_SHADOW`), `mActorKeyMatte` → RG8; blit uniform `ACTOR_KEY_SHADOW` (strength).
- Add-on (§4.2-4.3, **v1**): `reshade-addon/src/sl_reshade_bridge.cpp` (raw-present handler: KEY/ARM
  flags, unique runtime match, bracket over the fixed 8-slot `g_brackets[]` with the `SRWLOCK`
  `__try/__finally` protocol and `open`-before-mutation; v7 stale-open invariant failure → restore,
  close, dead, SKIPPED; new `reshade_render_technique` and `reshade_open_overlay` (observe-only)
  handlers, counters added to the existing `reshade_begin_effects`/`reshade_finish_effects` handlers;
  v8 `LdrRegisterDllNotification` generation + 2 Hz rescan, fail-closed enumeration; v7 whitelist (`binary_allowed` read from the depth pin, add-on-folder listing via
  `get_reshade_base_path` + `[ADDON] AddonPath`, `K32EnumProcessModules` export scan) run at
  `init_effect_runtime` and at every ARMING; v7 `gui_flags` (overlay event, `[OVERLAY]` ini keys via
  `get_config_value(rt, …)`, reload time); `reshade_present` close + guard verdict +
  `SLReShade_ReportKeyGuard` call outside the lock; `restore_all_open_brackets` with blocking acquire;
  host-DLL identity log; build stamp/commit/source hash + `SLReShade_ReportReader` call, caps 0x3),
  `reshade-addon/CMakeLists.txt` (stamp + manifest hash via the shared `cmake/SLAddonSourceHash.cmake`),
  `reshade-addon/README.md` (deploy, exe name, the recorded ReShade host 6.8.0.2155), new
  `reshade-addon/scripts/deploy_addon.ps1`. `reshade-addon/src/sl_depth_pin.hpp` is **untouched in v1**.
  The ABI header is `indra/newview/llreshadebridgeabi.h` (included directly, bridge.cpp:71): 1.3 flags,
  `SLReShadeReaderInfo`, `SLReShadeKeyGuard`.
- **v1.1 only, not in v1 (§4.2a, §4.2b option (a))** — ReShade fork on a new branch from upstream tag
  v6.8.0: `include/reshade.hpp` (two `RESHADE_API_LIBRARY_DECL` declarations beside
  `ReShadeGetBasePath`), `source/addon.cpp` (their implementations in the existing export block),
  `source/runtime.hpp` (`sl_request_present_bypass`, `_sl_present_bypass`, `_sl_overlay_hidden`,
  `_sl_overlay_was_hidden`), `source/runtime.cpp` (consume at `on_present` absolute entry, effects gate),
  `source/runtime_gui.cpp` (neutral input, active-ID clear, input release, draw-data-only suppression),
  `source/opengl/opengl_hooks_wgl.cpp` (`sl_presenting_scope` around the raw present +
  `present_effect_runtime`), `res/version.h` (SL identity); add-on: hard-bypass resolution/call, cap 0x4;
  `sl_depth_pin.hpp` two-entry `check_binary` table; `deploy_reshade.ps1`. Withdrawn from v5:
  `runtime_internal.hpp` field, `dll_main.cpp` exports, `runtime_manager::find`.
- `aldirectorhotkeys.cpp`: F9. `alfloaterlightbox.cpp` + `floater_lightbox_settings.xml`: tab.
  `floater_director.xml` (+ cpp): KEY toggle. `settings.xml`, `notifications.xml`.
- Docs: `DIRECTOR_HOTKEYS.md`, `MACHINIMA_USER_GUIDE.md`, `CINE_RIG_ACTOR_MASK_UI_NOTE.md`.

Shader permutation count: no new permutations on existing programs. New program objects: 8. Each is
`isComplete()`-guarded; any missing one disables only the key (not the Actor-only lights), logs
`ACTORKEY NO-PROGRAM <name>` once and sets the UI status to "unavailable (shader)".

---

## 9. Edge cases and known limits

1. Impostored, visually muted, jellydolled, too-complex, `isTooSlow` avatars: not in the mask → mode 1
   keys them **out** (they vanish), mode 2 leaves them un-keyed. UI shows "N not keyed (impostor)";
   `ACTORKEY IMPOSTOR <id>` once per transition. If the A8 keep-live hook covers key recipients
   (§3.3), this only affects muted/complex cases.
2. Shared Actor FX Replace/Cover ("STYLED") actors: their replacement geometry is not a recipient
   surface → same as 1. Documented; extending STYLED to the mask is out of scope.
3. Alpha-blended **set** geometry in front of an actor (glass, smoke prims, particles): the actor's
   coverage is depth-tested against **opaque** depth only, so the actor is keyed as fully present
   behind the glass; the beauty shows the glass tint on the actor. Particles are never in the mask.
4. Water: actors below the water surface are keyed as present; water above them stays on the actor.
5. Reflections/mirrors/probes/Prism feeds show the un-keyed scene (correct: they are set content).
6. The eligibility recorder refuses a stale stamp → that frame has no mask → no matte → the frame
   renders un-keyed; counted as `MATTE-STALE`. A persistently stale state is a bug and the UI says so.
7. `RenderResolutionDivisor > 1`: mask/matte at reduced res, bilinearly upsampled by the blit; feather
   is specified in output pixels and converted, so the look is stable. Expect a softer edge.
8. Snapshots (`rawSnapshot`): keyed by the viewer under the v10 completion contract (presents blocked,
   readback only after a completed keyed finalize, otherwise refused), ReShade not involved; exact codes
   only for unfiltered COLOR saves in a lossless format at rendered size — §4.4. High-res snapshots reallocate the screen buffers → `ensureActorMask` re-shares depth at the new
   size; key targets follow via `allocateScreenBuffer`. With `no_post` the key **still applies** (it is
   the shot's purpose). With `show_ui` the UI is captured over the key, as on screen.
9. 360 capture: the Lightbox/floater 360 capture uses `simpleSnapshot` (llfloater360capture.cpp:578),
   which renders each face as an ordinary **main view** (not `gCubeSnapshot`; v9 correction), so without
   a rule its faces would be keyed with per-face seams. v9: `simpleSnapshot` opens an
   `ALActorChromaKey::SuppressScope` → `key_now` false for those renders, `ACTORKEY 360 not keyed` logged
   once. The `gCubeSnapshot` cube path (llviewerwindow.cpp:6184) is gated as before. Keyed 360 is out of
   scope. No stereo path exists in this fork (Equirect video shelved).
10. Fisheye/diopter/kaleidoscope warp the scene after the matte is built → the key is **suspended**
    while they are active (UI explains). Alternative (not v1): warp the matte with the same map.
11. Multiple rigs + key: rig recipients keep their bits; forward/deferred lighting unchanged; only the
    `& ~KEY_BIT` masking is new. Verified by the existing Actor-only isolation test running with the
    key ON (expected: PARITY unchanged).
12. `CineLightRigActorMaskDisable` on + key on: `anyActorOnly()` false, `actorMaskWanted()` true →
    mask pass runs for key recipients only; lights normal. Intended.
13. Non-HDR path (`RenderHDREnabled` off): the blit is identical; the matte is identical. Supported.
14. Login/teleport/region crossing: recipients are re-gathered every tick; dead avatars skipped.

---

## 10. Performance

OFF (default): `sActive == false` → `actorMaskWanted()` collapses to `anyActorOnly()` (one static
read); no targets allocated; no listener work beyond the setting signal; `render_hud_attachments` and
the blit read one static each. Zero draws, zero logs. Reviewer checks the off path explicitly against
the beauty pass (CLAUDE.md rule 1).

ON: no numbers are claimed before measurement. Cost classes, each with its Tracy zone
(`ActorKey::coverage`, `ActorKey::buildMatte`, `ActorKey::shadowCascade[j]`, `ActorKey::shadowTerm`,
`renderFinalize - final blit`):
- Mask pass: a depth-only second draw of the keyed actors (today's cost class when lights are on).
- Coverage: one full-screen R8 convert + alpha replay of the actors' blended draws.
- Matte: two half-res separable passes + one full-res remap (+ the shadow sampling pass when on).
- Blit: one extra bilinear fetch and a few ALU.
- Memory: RG8 matte + R8 coverage + 2 × R8 half ≈ 4 MB at 1080p, 16 MB at 4K; shadow maps per §5.6.
- ReShade bridge: one seqlock memcpy per raw present in the add-on, no GL; v6 guard: one uncontended
  SRWLOCK acquire + counter increment per rendered technique, one 32-byte report per keyed present.
The in-world checklist records the measured zones; acceptance thresholds are set from the first
measurement, not guessed here.
- "All avatars" with a crowd of 20+: linear in recipients (walk + draws); plus impostoring off for
  recipients if the keep-live hook applies → the real cost is the beauty pass rendering full avatars.
  The UI's "N keyed" count makes this visible; Director cast mode is the escape hatch.

---

## 11. Test plan

Unit (TUT, `alactorchromakey_test.cpp`, `alcinelightrigmodel_test.cpp`):
- Two byte spaces (§3.3): with 0 rig lights and N key subjects, every light-side accessor
  (`actorOnlyBits`, `recipientBit`, `byteForOwner`, `drawByte`, `liveRecipientBits`,
  `actorOnlyDrawBytes`) returns 0 and `anyActorOnly()` is false, while `maskRecipientBit` returns
  `KEY_BIT` and `anyMaskRecipient()` is true. With 6 rig recipients + overlap, the overlapping avatar's
  light bit is unchanged and its mask byte is `rigBit|KEY_BIT`; bit 7 appears in no light-side value for
  any input (exhaustive over the model's recipient permutations); a 7th rig recipient overflows as
  fail-closed, and the overflow counter never counts key-only recipients.
- `keyMaskBits()` = 0x80 ∪ own bits of keyed rig recipients.
- Forward lookup defence: `selectForwardLightSet` asserts and masks `& ~KEY_BIT`; for every byte in
  0..255 the masked value selects the same set as the unmasked light-side byte would (no world fallback
  for a `rigBit|KEY_BIT` input).
- Shadow split (§5.2): a test harness constructs one `LLShadowCascadeScope` and calls `draw()` three
  times on a populated `LLCullResult`, asserting `getRenderMapSize(type)` for every type and every
  pool's `mDrawFace.size()` are unchanged after each draw (the Codex R1 duplication would fail this on
  the second draw); after the scope dies, `sUseOcclusion`, `sShadowRender`, `sAlphaCapCounts.active`
  and `sShadowCasterFilter` equal their pre-scope values on the normal path AND on an early-return path
  (a draw option that forces a mid-draw return). The beauty draw through the wrapper issues the same
  draw sequence as the old `renderShadow` (recorded call list compared). v12 [U-SPLIT]: also asserts
  `sCull == &result[j]` before and after each of the three draws and that `result[j]`'s address and
  render-map sizes are unchanged by `generateSunShadow`'s loop between them.
- Per-owner eligibility (§3.3): with recipients {rig A bit0, rig B bit1, key C, key D} and owner C
  recorded as covered: A, B, D are drawn, C is STYLED; with A covered: only A is STYLED (today's bit
  arithmetic gives the same answer for A and B — asserted). A draw owned by (D, fallback C) gets byte
  `KEY_BIT` from D only.
- Caster predicate: for a synthetic draw-info set (world, keyed attachment, un-keyed attachment, rigged
  keyed, rigged un-keyed) × cap filter {0,1,2,3} × caster {ALL, KEY_ONLY, SET_ONLY}, the skip decision
  equals `skip_by_cap || skip_by_caster` with the cap column identical to today's truth table.
- Zero radius (§3.6) [U-ZERO]: `feather = choke = 0` → matte bytes equal coverage bytes; `feather = 0, choke = 4`
  → no NaN/inf in the remap (s = 1/255, r = 4·scale); v12: `H_target == 0` (first frame after a resize)
  → `buildMatte` returns without drawing and the matte is invalid for the frame (key not applied, frame
  stamp not set), never a division by zero.
- `SLReShadeReaderInfo`: `static_assert(sizeof == 128)`; the viewer copy accepts `struct_size` 16, 128
  and 256 (future) and rejects 8; strings without NUL inside their arrays are truncated, never read past.
- Key guard (v7, pure state machine, no GL): reader states — NO_RESHADE → keys from the first frame,
  no ARMING; RESHADE_NO_READER and READER_NO_GUARD → REFUSED and **never** `keyGuardAllows()` (R5 P0-3);
  READER_GUARD → ARMING; ARMING never yields `keyGuardAllows()`; CLEAN with `viewer_frame < epoch` is
  ignored; CLEAN ≥ epoch → KEYING; CONTAMINATED/FOREIGN_ADDON/SKIPPED in ARMING or KEYING → REFUSED;
  ARMING timeout boundary (v8): after entry `mArmPresents == 0`; each un-reported evaluation publishes ARM and increments; the evaluation with `mArmPresents == 60` and no qualifying report is the first REFUSED frame (no flag) — exactly 60 ARM presents; a report delivered in the 60th ARM frame wins; **heartbeat (R5 P0-2)**: in KEYING, frame N with no
  CLEAN report for the last bracketed frame → not keyed, state ARMING — exhaustive over sequences of
  {report present/absent/stale/CONTAMINATED} × 1..5 frames, asserting no keyed frame ever follows a
  frame without a CLEAN report; Retry / key off-on / reload flag → re-evaluate reader state; key off
  from any state → OFF; `SLReShadeKeyGuard` `static_assert(sizeof == 32)`, copy accepts
  `struct_size` ≥ 32, rejects < 32.
- Bracket registry (add-on unit build, `SL_ADDON_TESTS`): 9 `init_effect_runtime` calls → 8 slots
  claimed, the 9th logs once and its presents report SKIPPED; destroy of an open slot restores then
  frees; re-init of an open slot restores before reset; v7: a raw present that finds its slot already
  open restores, closes, sets dead and reports SKIPPED exactly once, with the lock released before the
  report (a test double for the report asserts the lock is free when called).
- Concurrency [U-CONC] (v13, add-on unit build, fake runtime): 4 threads fire technique/begin/finish
  events concurrently with a presenting thread running raw-present/`reshade_present` brackets for 10⁶
  iterations; per-counter totals equal the events fired, no deadlock (watchdog timeout fails the test),
  I-LOCKRULE assertions silent, every present overlapping an off-present event reports CONTAMINATED.
- Whitelist (add-on unit build, v7): a fake module list and folder listing — {ReShade, ours} → ok;
  + `foo.addon64` file → FOREIGN (also when listed in `DisabledAddons`); + module exporting `AddonInit`
  or `NAME`+`DESCRIPTION` → FOREIGN; + module named `x.addon` → FOREIGN; unknown host hash → FOREIGN
  ("unverified build"); an ordinary viewer DLL with none of those exports → ok. v8: enumeration FALSE,
  needed > array (truncation), `GetModuleFileNameW` buffer-full, truncated base-path/AddonPath value,
  unformable path, `FindFirstFileW` error other than not-found → each FOREIGN ("check failed: <step>");
  v9: **no `AddonPath` key → base path, ok**; AddonPath naming a missing directory → empty set, ok;
  module-gen unchanged and < 500 ms → no rescan; gen changed → rescan before the verdict; no
  notification → rescan on every KEY/ARM present; a FOREIGN present still opens the bracket.
  v10 settle rule (fake clock + fake enumerator): notification < 200 ms ago → UNSETTLED, nothing
  cached; quiet but the two scans differ (enumerator returns a list that changes between calls) →
  UNSETTLED; notification injected between scan 1 start and scan 2 end → UNSETTLED; quiet + identical
  scans + no notification → verdict cached with `g_settled_state` = the **pre-scan** packed word; v11
  packed word: the callback's CAS loop is the only writer and updates generation and time in one step —
  a test double that interleaves two callbacks at every CAS retry point never yields a word whose time
  belongs to a different bump; a "paused callback" schedule (callback reads, scans run, callback
  commits) → the post-scan comparison sees a changed word → UNSETTLED, nothing cached; the fast path
  with an equal generation but a different time half → cache miss, rescan; 32-bit tick wrap across the
  200 ms / 500 ms comparisons → correct ages; at no
  point is a CLEAN verdict cached from a single scan or from a scan overlapped by a notification; with
  no notification mechanism nothing is ever cached and every KEY/ARM present runs two scans.
- Viewer module gate (v8/v9/v10, pure): NO_RESHADE with `mModuleGen` unchanged → keys; gen changed →
  frames not keyed until the viewer's own settle rule holds, then the re-test decides; re-test finds
  ReShade → not keyed and the state leaves NO_RESHADE; re-test failure → not keyed, REFUSED; in KEYING,
  `mModuleGen` differs from the `mGenAtPublish` of the previous bracketed frame → not keyed, ARMING;
  UNSETTLED reports keep ARMING and reset `mArmPresents`, never reach KEYING.
- Snapshot (v9/v10, §4.4, pure + debug hooks): with the guard in each of ARMING, REFUSED, KEYING and
  every reader state, a `gSnapshot` render → `keyGuardAllows()` true; `gatherFrame` publishes no KEY/ARM
  flag; previous bracketed frame, `mGenAtPublish`, `mArmPresents` and heartbeat bookkeeping unchanged;
  `SuppressScope` active → `key_now` false. v10 contract: with `SnapshotScope` open, a
  `LLWindow::swapBuffers()` call does not reach `SwapBuffers` and increments `sBlockedPresents`; a tile
  whose `display()` produced no `renderFinalize` (simulated resize early-return), a finalize with
  `key_now == false`, or a size mismatch → `rawSnapshot` returns false with `ACTORKEY SNAPSHOT-REFUSED`;
  the scope clears `sPresentBlocked` on every return path (early return and failure included).
  v11/v12 refusal latch: first refusal latches and logs/notifies once; 100 automatic (idle) requests →
  0 renders, 0 log lines, 0 notifications, placeholder shown — including when interleaved with each
  non-clearing change (suspension reason A → B, disconnect, window-size movement with
  `gWindowResized == true`), which only update the stored reason/tuple silently; each clearing event
  (explicit request, key toggle, `revision()` change, size change, post-resize window size change,
  reconnect, suspension reason cleared) → exactly one new attempt; an explicit request while latched
  always attempts; `rawSnapshot` with no `SnapshotRequestScope` behaves as explicit.
- Counters (v8/v9): one public `render_technique` → passes 1, begins 1, finishes 1 → `passes=1,
  effect_calls=2`; `render_effects` where techniques exist but none is enabled → `passes=0,
  effect_calls=2`; each counter and the `begins + finishes` sum saturate at `UINT32_MAX` (sum of two
  near-max values stays `UINT32_MAX`); any non-zero → CONTAMINATED.
- Remap math: binary coverage edge → monotonic 0..1 over `feather`; `choke = +r` shrinks by ≈ r px,
  `-r` grows; interior exactly 1.0, exterior exactly 0.0; feather 0 → step.

Log verdicts (every 5 s while ON; the log states PASS/FAIL/INCONCLUSIVE):
- `ACTORKEY SUMMARY mode=<m> recipients=<n> drawn=<d> impostor=<i> styled=<s> coverage_draws=<c>
  matte_frames=<f>/<total> stale=<st> reader_seen=<0|1>`.
- `ACTORKEY SELFTEST`: on demand, after the blit, `glReadPixels` of a set pixel chosen from the matte
  (cheapest reliable choice, e.g. the top-left pixel if its matte value is 0 and its shadow term is 0).
  Reports, per §3.7a: 8-bit path → PASS iff the readback equals the requested codes; 10-bit path →
  PASS iff the 10-bit code equals `round(c·1023/255)` AND the 8-bit readback equals `c`; FAIL prints
  both values; INCONCLUSIVE if no qualifying pixel or if `glIsEnabled(GL_FRAMEBUFFER_SRGB)` was true at
  the blit (that is a separate `SRGB-LEAK` FAIL line). Also prints the darkest shadow code found.

Existing instrument: the Actor-only isolation test (`CineLightRigActorMaskTest`) must still report
PARITY with the key ON in both modes (the key is applied after its C_post capture, so the captures are
clean; the implementer confirms `captureDeferred`/`capturePost` run before the blit — they do, both are
inside `renderDeferredLighting`).

In-world checklist (user; outcomes stated in advance):
1. Key OFF (fresh start): frame identical to current build (debug view Off; `RenderBufferVisualization`
   untouched). Expected: no `ACTORKEY` lines in the log at all.
Scope labels (v9, R7 item 6; v10): **[viewer]** = self-test, or a viewer snapshot that is COLOR, unfiltered
and saved as PNG/BMP/lossless AVIF at rendered size, read back with presents blocked — ReShade not
involved, exact codes expected whether or not ReShade is loaded; **[no ReShade]**
= capture of the presented window with ReShade not loaded — exact; **[ReShade]** = capture of the
presented window with ReShade loaded — judged against the D30 statement ("exact except where ReShade's
own UI draws, and an add-on effect switched on mid-key may tint ONE frame before keying stops").
2. **[viewer]** Key ON, mode 1, Pure Green, feather 1.5, me alone on a lit set: set is flat 0,255,0 in a
   snapshot (verify with the self-test and an external pixel picker on the PNG); UI/HUD not keyed; hair
   edge soft, no halo cut-out of alpha hair; eyelashes present.
3. Mode 2: I am a green silhouette with soft edges; set and my floor shadow intact.
4. Mode 3 and Invert: pure 0/255 matte, soft edge; invert swaps.
5. **[viewer]** Each of the 8 presets + a custom colour: self-test PASS with the exact 0-255 codes (Chroma Green
   must read 0,177,64, Chroma Blue 0,71,187; a pixel picker on a PNG snapshot agrees); custom swatch
   flips the preset to Custom. Repeat Chroma Green with the 10-bit backbuffer if the user's monitor
   path uses it; the self-test reports which rounding matched.
6. Choke ±4: silhouette visibly shrinks/grows; feather 0 = hard; feather 16 = wide ramp.
7. Subjects: All vs Director cast vs Me; clones on/off; animesh on/off: expected membership, UI count
   matches.
8. OBS capture of the window with UI hidden (Ctrl+Alt+F1), no badge: **[no ReShade]** key colour
   exact; **[ReShade]** (overlay closed, no add-on effects) key colour exact on every frame stepped,
   the guarantee line shown in the Lightbox.
8b. **[viewer]** Snapshot while keying (v9, §4.4): with ReShade loaded and RTGI in the preset, and with
   the guard deliberately REFUSED (`/DSL_PROBE=6`), take a normal snapshot, a high-res 2× snapshot and a
   `no_post` snapshot → all three keyed with the exact codes (pixel picker), no RTGI on the key; the log
   shows no `ACTOR_KEY`/`ARM` publish and no `mArmPresents`/heartbeat change for the snapshot renders;
   the next presented frame's guard state is what it was before the snapshot. 360 capture while keying
   → faces not keyed, `ACTORKEY 360 not keyed` once.
8c. Snapshot contract (v10, R8 P0-1 / P1-3): (a) take a snapshot while dragging the window edge
   (resize early-return) → either the snapshot succeeds keyed or it is refused with "keyed frame could
   not be completed (resize)"; never an un-keyed or black image; ReShade.log shows no `present` during
   the snapshot (`sBlockedPresents` ≥ 1 logged); (b) fisheye on, key on, take a snapshot → refused with
   reason "fisheye" (not silently un-keyed); (c) DEPTH24 snapshot → taken, and the floater note states
   exactness applies to COLOR only; (d) a filter selected → saved, note shown, pixel picker shows changed
   codes (expected); (e) JPEG save → codes not exact (expected), PNG save of the same frame → exact;
   (f) disconnected-state snapshot is not testable in-world without a disconnect; covered by the unit
   test's blocked-present assertion; (g) v11 latch: with fisheye on and the snapshot floater open
   (live preview), the refusal notification appears **once**, the log shows one `SNAPSHOT-REFUSED`, the
   preview shows the placeholder and the frame rate does not drop from repeated snapshot renders;
   turning fisheye off refreshes the preview once, keyed; pressing Refresh while still latched retries
   once.
9. ReShade (§4.2-4.3): deploy the rebuilt add-on; ReShade.log shows `[SLBridge] build <stamp> commit
   <sha> src <hash8> abi 1.3 caps 0x3` and the host-DLL line `6.8.0.2155` (depth pin eligible). With
   RTGI + a LUT enabled in the preset, toggle key ON: one ARMING frame (un-keyed, effects off), then the
   very first keyed frame is clean (record with OBS at 60 fps and step frames: no frame shows RTGI over
   the key), log shows `bracket first open (effects were on)` and `guard CLEAN`, status shows "key
   bypass active", self-test PASS. Press Home while keyed (Codex R2 P2-12): the checkbox on screen does NOT flip (the overlay is
   drawn before shortcuts are processed, runtime.cpp:652-677); effects stay suppressed; the add-on log
   shows "user toggle observed inside bracket". Toggle key OFF: effects come back on the first un-keyed
   frame in the TOGGLED state (if effects were on before and Home was pressed once while keyed, they
   are now off and the checkbox is unchecked — the toggle was honoured as state), log shows
   `bracket last close`.
   Stale binary: deploy an add-on built from an older `reshade-addon/src` → status red "stale (different
   bridge sources)"; deploy a 1.2 add-on → red "older than this viewer"; in both cases the key edge
   shades under RTGI and the self-test FAILS (the warning is the deliverable).
9b. Teardown/fault (P1-12), each ending with "Effects checkbox is ON and effects visibly render":
   (a) key ON → "Reload" in the ReShade overlay while keyed: `reshade_reloaded_effects` fires INSIDE
   the bracket (raw present precedes `update_effects()`); log shows `reload_during=1` on the next
   bracket line, effects stay suppressed, and after key OFF effects render; (b) key ON → teleport across
   a region that recreates the GL context (`destroy_effect_runtime` + `init_effect_runtime`) → key OFF;
   (c) key ON → exit the viewer normally → relaunch → ReShade.ini `EffectsEnabled`/preset untouched and
   effects on at login; (d) key ON at the login screen is impossible (no main view), but force
   `SLRESHADE_FLAG_ACTOR_KEY` with the debug setting `ActorKeyForceBridgeFlag` while the frame is
   INVALID → no bracket opens (log `key_frames_missed`); (e) build the add-on with `/DSL_PROBE=3` (new:
   throws inside the raw-present handler after opening the bracket) → ReShade.log shows the SEH line and
   `restored on`, effects render, bridge dead; (f) unload/replace the add-on while keyed is a process
   restart — covered by (c); (g) two swapchains: open a second ReShade-hooked GL window in the same
   process (e.g. the viewer's media/plugin window if ReShade attaches to it; otherwise a debug build
   option that creates a second runtime) → keying the main window never flips the second runtime's
   Effects checkbox, and the log's `runtime %p` matches the main window's.
9e. **v1.1 only (fork DLL, §4.2a)** — not run for v1. Hard bypass: ReShade.log shows `SL bypass ABI 2`
   and `caps 0x7`; open the ReShade overlay, enable RTGI, key ON: the overlay disappears from the screen
   and the capture while keyed (D25), effects do not render, the Effects checkbox is UNTOUCHED when the
   key goes off, self-test PASS. Overlay lifecycle (Codex R4 P0-1): (1) overlay open with the mouse over
   it, key ON → the viewer immediately receives mouse and keyboard (camera orbit and typing in chat
   work), no invisible ReShade widget reacts to clicks; (2) focus a ReShade text box, key ON → the
   ReShade Home/screenshot shortcuts still work while keyed (`_ignore_shortcuts` not latched); (3) key
   OFF → the overlay reappears in the state it had, input capture behaves as before keying; (4) press
   the overlay key while keyed → nothing visible; key OFF → the overlay is open. Export validation: a
   debug add-on build calls `ReShadeSLSetPresentBypass` outside the raw present and with a foreign
   pointer → `false` both times, no present bypassed. Depth pin: `[DepthPin]` eligible on the SL DLL
   (second table entry) AND on the stock 2155 DLL after `deploy_reshade.ps1 -Revert`; the three-way
   `Prefix` probe log agrees.
9i. Key guard (v6, v1): (a) stock 6.8.0.2155, iMMERSE RTGI + LUT on → guard CLEAN, key works (9);
   (b) contamination probe: add-on built with `/DSL_PROBE=6` (counts one synthetic technique in the
   bracket) → status red "add-on effects present — key refused", the frame after the report is NOT
   keyed, log `guard CONTAMINATED tech=1`; Retry with the probe still on → refused again after exactly
   one ARMING frame; (c) `/DSL_PROBE=7` (suppresses the report) → ARMING publishes exactly 60 ARM presents, then the next frame is REFUSED
   REFUSED "no confirmation"; (d) a **real** mid-frame `effect_runtime::render_effects` call (v13: the synthetic `/DSL_PROBE=8`
   counter fallback is removed): add-on debug build `/DSL_PROBE=15` calls `rt->render_effects(cmd_list,
   back_buffer_rtv, rtv_srgb)` with Effects **on** and RTGI enabled from the `reshade_begin_effects`-free
   point just before our raw-present bracket opens (inside the raw `present` handler, before
   `module_guard`) → status red "effect activity", log shows non-zero `passes`/`effect_calls`, the next
   frame is not keyed; a second variant `/DSL_PROBE=16` calls the public `render_technique` for one RTGI
   technique on the back buffer → same result; (e) Reload in the overlay while REFUSED → the guard re-arms by
   itself (reload flag); (f) overlay open while keyed → amber "ReShade overlay is open" and the key
   meets the guarantee outside the overlay rectangle (self-test PASS on a pixel outside it); (g) no ReShade
   installed → no ARMING, the key starts on the first frame.
9j. Guard v7 (Codex R5): (a) whitelist — copy `obs_capture.addon64` into the add-on folder, restart:
   red "Other ReShade add-ons are loaded (1)", ReShade.log names the file, no frame is keyed; rename it
   to `.off` and restart → key works; (b) put the same file name in `[ADDON] DisabledAddons` → still
   refused (documented conservative rule); (c) `.addonfx` effect enabled (any shader renamed to
   `.addonfx`) → red "effect pass(es) with Effects off", no keyed frame after the first report;
   (d) heartbeat — `/DSL_PROBE=7` toggled ON mid-keying via a debug key (reports stop) → the very next
   frame is un-keyed (OBS frame-step), status ARMING, exactly 60 ARM presents, then REFUSED on the next frame; (e) reader states —
   delete `sl_reshade_bridge.addon`, keep ReShade → red "bridge add-on is not running", no keyed frame;
   deploy a v6-caps add-on (built with `/DSL_CAPS=0x1`) → red "older than this viewer", no keyed frame;
   remove ReShade's `opengl32.dll` → status "ReShade: not loaded", key works; (f) stale bracket —
   `/DSL_PROBE=11` (skips one `reshade_present` close) → next raw present logs `stale open bracket`,
   Effects checkbox restored ON, status red "bridge stopped confirming", key refused; (g) GUI notes —
   set `ShowFPS=1` in ReShade.ini → amber "OSD always on" while keying; reset `TutorialProgress=0` →
   "splash until tutorial dismissed"; Reload → "splash after reload" for ≤ 5 s; the v8 guarantee line
   is present, verbatim, in every keyed state with ReShade loaded, and "Key colour is exact." without it.
9k. v8 (Codex R6). Every check is stated against the guarantee *"Key colour is exact except where
   ReShade's own UI draws, and an add-on effect switched on mid-key may tint ONE frame before keying
   stops."*: (a) **one-frame limit, measured**: while KEYING with RTGI in the preset, enable an
   `.addonfx` effect from the overlay → OBS frame-step shows **at most one** tinted frame, then un-keyed
   frames and red "effect activity"; the same with a debug DLL exporting `NAME`/`DESCRIPTION` loaded
   mid-key through the debug setting `ActorKeyDebugLoadDll` → at most one affected frame, then red
   "Other ReShade add-ons are loaded"; (b) **late ReShade** (R6 item 2): start without ReShade, key ON
   ("ReShade: not loaded"), then load a test DLL exporting `ReShadeRegisterAddon` +
   `ReShadeUnregisterAddon` via `ActorKeyDebugLoadDll` → the **very next** frame is not keyed and the
   status leaves NO_RESHADE (red "bridge add-on is not running"); the frame already keyed when the DLL
   loaded is the one D30 frame (v9); (c) **enumeration failure** (R6 item 3):
   `/DSL_PROBE=12` (module enumeration returns FALSE), `/DSL_PROBE=13` (truncation: array size forced to
   8), `/DSL_PROBE=14` (AddonPath value truncated) → each red "ReShade add-on check failed: <step>"; v9:
   removing the `AddonPath` key from ReShade.ini and pointing it at a missing folder are both **not**
   failures (key works); no keyed
   frame; viewer debug setting `ActorKeyDebugFailModuleScan` → red "ReShade check failed", no keyed
   frame; (d) **benign DLL load**: load an ordinary DLL (no add-on exports) mid-key → un-keyed ARM frames with
   amber "waiting for DLL loading to settle" for ≥ 200 ms plus two scans (v10 settle rule), then KEYING
   resumes (log `module_scans` +2, `foreign=0`, one `UNSETTLED` run); (e) cost: Tracy
   `SLBridge::moduleScan` and `ActorKey::moduleGate` recorded with ~250 modules; steady-state gate
   ≤ a few µs per frame; full scans only on load events and at 2 Hz (record the numbers, no threshold on
   the first run); (f) counters: one `.addonfx` technique → status shows "1 pass(es), 2 begin/finish
   call(s)"; an add-on test hook that calls `render_effects` while techniques exist but none is enabled → "0 pass(es),
   2 begin/finish call(s)".
9m. Exact shadow completeness (v13, §14 item 14): debug setting `ActorKeyShadowSelfTest` renders, for
   the current frame, the beauty actor-shadow mask (sun visibility with all casters < visibility with the
   set map only) and Exact's actor term at full resolution, and logs `ACTORKEY SHADOW-EXACT miss=<n>
   extra=<m> total=<t>` — `miss` = pixels in the beauty actor-shadow mask where Exact's term is 0,
   excluding pixels within one shadow-map texel of a mask edge (filtering tolerance). Run it with, one at
   a time, standing on flat ground in sun: (a) a system-body avatar (no attachments); (b) a rigid prim
   attachment held out from the body (e.g. a staff); (c) a rigged-mesh-only avatar (system body hidden);
   (d) a GLTF-material/GLTF-mesh attachment. Required: `miss == 0` in every case. Cross-check per case:
   a mode-2 (Set only) **[viewer]** PNG and a mode-0 Exact PNG from the same frozen camera — the darkened
   key region in mode 0 covers the floor-shadow region visible in mode 2 (pixel picker on 5 points
   inside the mode-2 shadow).
9f. Split regressions (Codex R3 P0-2/P0-3): rain occlusion log line (`Rain occlusion: caster_batches=…
   depth_min=… gl_error=0`, pipeline.cpp:26308-26315) identical before/after the refactor; spot and
   aux shadows unchanged; a forced early return inside an OWN-mode draw (debug setting
   `ActorKeyShadowFailDraw`) leaves the GL matrix stack depth, the bound FBO, `sShadowRender`,
   `sUseOcclusion`, cull/clamp/depth enables and `sShadowCasterFilter` at their pre-cascade values
   (`LLGLState::checkStates` passes; the §11 unit test asserts the same).
9g. SEH lock (Codex R3 P0-4, R4 P0-4): add-on built with `/DSL_PROBE=4` (faults while `g_bracket_lock`
   is held after `open = true` and before `set_effects_state`) → ReShade.log shows the SEH line,
   `restored`, no hang (recovery's blocking acquire succeeds because `__finally` released), effects
   render, the viewer shows "bridge faulted — key refused"; `/DSL_PROBE=5` (faults inside
   `set_effects_state`) → same; `/DSL_PROBE=9` (faults inside the `reshade_render_technique` handler
   while it holds the lock) → same; `/DSL_PROBE=10` (second fault inside recovery's per-slot restore) →
   that slot logged `restore faulted`, the walk completes, no hang.
9h. Teardown release (Codex R3 P1-5): key + shadows ON, then toggle `RenderDeferred`/change the graphics
   preset (full `releaseGLBuffers`) → `SHADOW release (teardown)` logged, no `LLRenderTarget` leak
   (`sBytesAllocated` returns to baseline after key OFF), targets re-ensured when keying resumes.
9c. Shadow split / filters (Codex R2): (h) `RenderShadowAlphaCapDebug` on, alpha cap active
   (`include_alpha_cutout=false` cascades), isolation Exact: the set map excludes keyed attachments AND
   excludes capped world alpha casters (debug view 4 "Actor shadow" vs a new 5 "Set shadow" shows no
   grass/world-alpha silhouettes in the capped cascades); (i) a rigged-mesh-only avatar (no system body
   visible) casts into the actor map and not into the set map — proves the rigged loops evaluate the
   caster predicate; (j) `ActorKeyShadow` toggled ON mid-session: the first frame after the toggle has
   the actor maps (log `SHADOW ensure ok`), no shadow-map resize hitch, no `shadowMeta` reset; toggled
   OFF: targets released (`SHADOW release`); window resize while ON: maps re-ensured at the new size;
   (k) forced allocation failure (debug setting `ActorKeyShadowFailAlloc`) → `SHADOW-FAIL alloc`,
   fallback to All-shadows, beauty shadows intact.
9d. Edge (Codex R2 P1-9): feather 0 + choke 0 → matte equals raw coverage (hard, no blur, debug view 3
   shows 0/1 plus hair alpha only); feather 0 + choke 4 → hard-edged shrink; feather 4 + choke 0 → ramp.
9a. Shadows (§5): mode 0, strength 0.5, fast: my floor shadow appears as darker green with a soft edge
   that tracks the beauty shadow (compare against mode 2's real shadow by toggling); set shadows (a
   wall's shadow) do NOT darken the key. Strength 0 → flat key; 0.85 → near-black shadow, OBS Chroma
   Key with Similarity ~400+ still keys it. Isolation exact: stand on a balcony, look at the floor
   below — fast shows a darker patch inside the balcony shadow, exact does not. All-shadows: every sun
   shadow darkens the key. Cascades 1 vs 2: a distant actor's shadow disappears at 1. Moon night: same
   with moonlight. `RenderShadowDetail 0`: status says unavailable, key colour unaffected (guarantee unchanged). Mode 2 + matte
   shadow: grey shadow in the matte. Self-test reports the darkest shadow code.
10. Actor-only light + key ON: light still only lights the actor; isolation test PARITY.
11. **[viewer]** High-res snapshot 2× window: keyed with exact codes, feather proportionally identical
    (§4.4; with ReShade loaded too).
12. `RenderResolutionDivisor 2`: keyed; softer edge, no misregistration.
13. Toggle fisheye: key suspends with the UI reason; toggle off: resumes. Same with an RLVa
    `@setsphere` restriction active (test viewer with RLVa on and a scripted sphere): suspended with
    reason "RLVa vision sphere".
14. F9 with Lightbox open toggles; with no operator floater open does nothing; `DirectorHotkeysEnabled`
    off disables it.
15. Perf: record every `ActorKey::*` Tracy zone at 1080p with 3 actors for shadow off / fast / exact
    and report the numbers; no threshold is asserted on the first run (§10).
16. Shadow log checks: `ACTORKEY SHADOW cascades=<n> isolation=<i> casters=<draws> fail=<0|1>`
    every 5 s; `AlphaCapSunLeakCheck` never fires with the key on; with `RenderShadowAlphaCapDebug` on,
    the AlphaCap expected/actual counts are identical with the key on and off (the 4/5 draws are not
    counted).
17. Beauty shadow parity after the split: with the key OFF, a before/after screenshot pair of a
    shadowed scene is pixel-identical (`RenderBufferVisualization` of the shadow maps agrees too).

---

## 12. Decisions still open (v2)

D1-D15 are closed (see the v2 table at the top). New items raised by the v2 work, each with a
recommendation; silence = the recommendation stands:

| # | Decision | Options | Recommendation |
|---|---|---|---|
| D16 | Default shadow isolation | fast / exact / all | **Fast** (cheap, right in open sets); exact one click away |
| D17 | Default shadow strength | 0.35 / 0.5 / 0.7 | **0.5** (0,88,32 for Chroma Green: clearly a shadow, well inside keyer tolerance) |
| D18 | User presses Home while keyed | effects suppressed again next present / honour the user | **Suppressed again next present**; the user's setting is preserved and returns the instant the key clears (bracket semantics, §4.2) |
| D19 | ~~Add-on flag poll event~~ | — | **Closed by Codex R1**: raw `addon_event::present` opens the bracket, `reshade_present` closes it |
| D20 | Add-on deploy script | ship `deploy_addon.ps1` / manual copy | **Ship the script** (prints built/deployed/expected source hashes; the stale-binary memory is the reason) |
| D21 | Expected add-on source hash captured at viewer configure | accept the reconfigure caveat / hash at build time via a custom command | **Accept** for v1 (the deploy script makes a mismatch obvious); build-time hashing if it bites |
| D22 | Shadow target ON-transition path | lazy `ensureActorShadowTargets` with rollback / `requestResizeShadowTexture()` (full sun+spot reallocation, `shadowMeta` reset) | **Lazy ensure** (no hitch, no meta reset, same allocator as the beauty loop) |
| D23 | Today's AlphaCap/rain filter does not reach most rigged shadow loops (lldrawpool.cpp:1154-1200, 1225-1252, 2473-2579; `pushMaskBatchesIndexed` is the exception) | leave as is (this package) / fix in a separate change | **Leave; separate change** — fixing it here would alter filters 1-3 behaviour the beauty-parity test freezes |
| D24 | ~~ReShade fork DLL as part of the deliverable~~ | — | **Superseded by D26 (v6)**: not in v1; the v5 recommendation rested on a fork tree (6.7.3.3) that is not the deployed binary (6.8.0.2155) |
| D25 | Hide the ReShade overlay during keyed presents (`SL_BYPASS_OVERLAY`) | yes / no | **Deferred to v1.1** (needs the fork DLL). Then **yes**, suppressing only the draw-data submission and releasing input capture (§4.2a). In v1 an open overlay is an amber warning |
| D26 | ReShade release provenance / key-bypass mechanism (Codex R4 P1-5, §4.2b) | (a) port the bypass onto exact upstream v6.8.0, SL version identity, two-entry depth-pin table, three-way GD `Prefix` check (~1.5-2 d, medium risk) / (b) migrate the fork to 6.8.x wholesale (~2-3 d, medium-high) / (c) no DLL change in v1: soft bracket + runtime key guard that refuses to key while any technique renders with Effects off (~0.5 d, low) | **DECIDED (user, 2026-10-05): (c) for v1** — soft bracket + runtime key guard (refuse with red status), overlay not hidden (amber note), no ReShade DLL or depth-pin change. (a) is the v1.1 follow-up, outside this package; (b) rejected |
| D27 | Key-guard policy when contaminated | refuse to key (red) / key anyway with a red warning | **Refuse** (exactness is the feature's promise, review-severity gate); Retry button + automatic re-arm on reload. v7: refusal is **conservative** — any `.addonfx` pass or begin/finish-effects event during a keyed present refuses, whatever it writes (no RTV/dependency tracing); status text says "effect activity: <p> pass(es), <c> begin/finish call(s)" (v8), not "tint" |
| D28 | Unobservable add-on GPU work (Codex R5 P0-1) | strict whitelist, refuse otherwise / weaken the guarantee in the UI / allow-list configurable by the user | **Strict whitelist** (host 6.8.0.2155 built-ins + our bridge only; folder listing + module export scan; `DisabledAddons` not honoured). Invisible, global contamination must refuse, not warn. Residual: a convention-free self-registering DLL injected outside the add-on folder. A reviewed allow-list (e.g. `obs_capture`) is a later, separate change |
| D29 | ReShade's own GUI (overlay, OSD, splash, preview) over the key in v1 (Codex R5 P0-1/P1-6) | refuse while any may draw / warn honestly | **Warn, unconditionally, with named causes**; guarantee worded as the single v8 statement (§4.2). Justification: visible, local, user-controlled, the same class as the viewer UI (which is also never keyed); preview state is unobservable, so a refusal rule could not be complete anyway. Hiding = v1.1 (D25) |
| D30 | Soft-path one-frame limit (Codex R6 P0: a CLEAN report for N-1 cannot prove N) | build the fork bypass now / accept and disclose | **USER DECIDED 2026-10-05: accept and disclose.** The single guarantee statement (§1, §4.2) is used verbatim in the UI, docs and tests; no other text claims unconditional exactness while ReShade is loaded. Removed by the v1.1 fork bypass |

---

## 13. Work packages — v1 only (sequential; each Codex-implemented, Opus/Codex adversarially reviewed, one build at the end)

v1 scope per D26 (decided): WP0, WP1, WP2, WP3, WP4 (add-on), WP5. No ReShade DLL work; the fork
bypass (§4.2a) is a separate v1.1 package listed after the build line for reference only.

- **WP0 registry** — `KEY_BIT`, the mask-byte accessors and `anyMaskRecipient()` (§3.3), the mask
  pass / `renderSystemActorMask` switched to them, the recorder's per-owner `mCoveredOwners` and the
  mask pass's `native_styled`/`maskDrawByteUncovered` derived from it (never from `coverage_bits &
  KEY_BIT`), `& ~KEY_BIT` + assert in `selectForwardLightSet` and the set builder, `actorMaskWanted()`
  at its four gate sites including the exact 7371 statement, keep-live hook extension, tests. Reviewer:
  prove every light-side accessor is unchanged and bit-7-free; prove the per-owner result equals
  today's bit arithmetic for rig recipients; prove the mask pass output with key off equals today's;
  prove the off path is byte-identical.
- **WP1 coverage + matte** — targets, programs, `renderCoverage`, `buildMatte`, debug view 3, summary
  line, Tracy zones. Reviewer: GL state restore proven by `LLScopedActorMaskInvariant`; blend equation
  restored (`GL_MAX` is not in the invariant's capture list today — add `GL_BLEND_EQUATION_RGB` is
  already there at line 158; confirm MAX is restored to FUNC_ADD).
- **WP2 composite** — blit key program (incl. shadow strength) with the injection after
  `clampHDRRange`, explicit `LLGLDisable(GL_FRAMEBUFFER_SRGB)` + debug assert in the blit block,
  `key_now` from the `diopter_warped`/`fisheye_warped` locals, HUD suppression, self-test with the
  §3.7a acceptance (8-bit / 10-bit two-criterion / SRGB-LEAK / darkest shadow), ABI 1.3 flag set iff
  `key_now`, reader handshake + `SLReShade_ReportReader` + expected source hash at configure; v6: the
  key-guard state machine, `keyGuardAllows()` in `key_now`, `ACTOR_KEY_ARM` on ARMING frames,
  `SLReShade_ReportKeyGuard` export; v9: snapshot rule (§4.4: `keyGuardAllows()` true on `gSnapshot`,
  no KEY/ARM publish, no guard bookkeeping; 360 `SuppressScope`), capture-before-scan generation
  protocol, `InterlockedIncrement` notification callback; v10: snapshot completion contract (present block,
  completed-keyed-finalize readback check, refusal + notification, exactness-scope note), viewer-side
  settle rule; v7: reader states (ReShade-module scan), heartbeat, FOREIGN_ADDON
  and `gui_flags` statuses; v8: the verbatim guarantee line, the viewer module gate (`LdrRegisterDllNotification` generation + ReShade re-test before every keyed frame, fail closed), the exact ARMING boundary (`mArmPresents`), separate passes/effect_calls in the status. Reviewer: the SRGB disable is
  reviewed against the beauty pass and the off path (CLAUDE.md rule 1 — shared GL state); the 10-bit
  criteria; the flag never set on a suspended frame; `ACTOR_KEY` and `ACTOR_KEY_ARM` never both set; no
  keyed frame without a CLEAN report for the previous bracketed frame whenever ReShade is loaded; no
  keyed frame at all for RESHADE_NO_READER / READER_NO_GUARD.
- **WP3 actor shadows (§5)** — `LLShadowCascadeScope` (RAII prepare/draw/restore; cull/clamp/depth
  guards as members; `MatrixScope`/`TargetScope` per draw; BOUND vs OWN target modes) with
  `renderShadow` as a BOUND-mode thin wrapper so rain/spot/aux callers are untouched;
  `releaseActorShadowTargets` on every teardown path; the independent `sShadowCasterFilter` + `ShadowCasterFilterScope` +
  `CasterFilterLeakCheck`; the caster predicate in every non-rigged AND rigged loop (table in §5.2), the
  cap predicate untouched; `ensureActorShadowTargets` with the shared `allocateSunCascadeTarget` /
  `applySunShadowSamplerState` helpers and rollback; `bindSunShadowState` extracted and used by both the
  deferred shaders and the matte program; matte RG8 + shadow term; fallback ladder; `ACTORKEY SHADOW`
  summary. Reviewer: the split is a pure move (beauty maps byte-identical, draw sequence identical);
  the destructor restores on every exit incl. early returns; no draw loop mutates the result;
  `sShadowDrawFilter` consumers and the AlphaShadowCap counters unchanged; `sShadowCasterFilter` is
  `CASTER_ALL` at every `generateSunShadow` exit; the aux draws cannot write `mRT->shadow[j]`; the
  deferred shader's uniform upload list is identical before/after the binder extraction.
- **WP4 add-on (§4.2-4.3)** — raw `present` handler for KEY/ARM (unique device+hwnd match,
  fail closed → SKIPPED report), bracket over the fixed 8-slot `g_brackets[]` with the SEH-safe
  `SRWLOCK` `__try/__finally` protocol and `open`-before-mutation, `restore_all_open_brackets` with a
  **blocking** acquire and per-slot inner `__try/__except` (no lock-free path), reload-inside-bracket
  tolerated + reload flag, key guard (`reshade_render_technique` + begin/finish-effects counters,
  `reshade_open_overlay` observe-only, verdict + `SLReShade_ReportKeyGuard` after the lock), v7 strict
  whitelist (host hash, add-on folder, module export scan) with per-item logging, v7 `gui_flags`, v7
  stale-open-bracket invariant failure, host-DLL identity log; v8: continuous whitelist (module-gen notification + 2 Hz rescan, fail closed on any enumeration/truncation/path failure, bracket opened on FOREIGN), three separate counters; v9: capture-before-scan `module_guard()` with `InterlockedIncrement` callback; v10: settle rule (200 ms quiet via `InterlockedExchange64` timestamp, two identical scans, no notification across them, UNSETTLED verdict 5, nothing incomplete cached), AddonPath default/missing-folder rules, saturating `effect_calls`,
  `SL_PROBE=3..11` fault probes and `SL_CAPS` override, `SLReShadeReaderInfo` + `ReportReader` (caps 0x3),
  manifest-hash CMake module shared with the viewer, README (host 6.8.0.2155 recorded),
  `deploy_addon.ps1`. Before coding: `git fetch upstream tag v6.8.0` into `I:\reshade-SL` (read-only use)
  and confirm on the 6.8.0 source that `reshade_render_technique` fires for every rendered technique,
  that `render_effects`' early return precedes it, and list any `effect.addon = true` sites (diagnostic
  for the README; the guard does not depend on it). Reviewer (adversarial): construct any sequence of
  present(A/B) / reshade_present(A/B) / render_technique / reload / re-init / destroy / SEH fault (inside
  and outside the lock, inside recovery) / torn-frame / >8-runtime events that ends with any runtime's
  `_effects_enabled == false` while its bracket is closed, with the lock held, with a keyed frame on
  which a technique or begin/finish-effects event fired without a CONTAMINATED report, with a report
  issued while the lock is held, or with the viewer keying after a missing report — the design claims
  none exists; and find a loaded add-on configuration the whitelist accepts that is not {host built-ins,
  our bridge}; confirm `__finally` releases before the `__except` body on
  MSVC x64. Claude builds the add-on and deploys with the script; the stamp/hash/host lines in
  ReShade.log, `guard CLEAN` and a green Lightbox status are the acceptance.
- **WP5 UI** — Lightbox tab (incl. shadow rows and ReShade status), Director toggle, F9,
  notification, badge, settings, docs (user guide: shadow option, two-pass alternative, OBS keyer
  notes, Home-key recovery). Reviewer: the eight draw() rules; no `getChild` or
  `gSavedSettings.get*` in any draw path.
- Then **one viewer build + one add-on build**, deploy both (the ReShade DLL is NOT rebuilt or
  redeployed in v1), then the in-world checklist (9e is v1.1-only and skipped).

**Deferred — v1.1, NOT part of this package (reference only):** *WP-v1.1 ReShade fork* (§4.2a, §4.2b
option (a)) — branch from upstream tag v6.8.0 (commit recorded); `ReShadeSLGetBypassABI`/
`ReShadeSLSetPresentBypass` declared in include/reshade.hpp, implemented in source/addon.cpp;
`sl_request_present_bypass` on `runtime` (runtime.hpp); consume at `on_present` absolute entry;
`draw_gui()` always runs with only the draw-data submission suppressed, neutral ImGui input, active-ID
clear and explicit input release; `sl_presenting_scope` in opengl_hooks_wgl.cpp; SL version identity;
two-entry depth-pin table + three-way `Prefix` check; `deploy_reshade.ps1` with revert; add-on cap 0x4;
`ActorKeyHideReShadeOverlay` (D25); test 9e. Gets its own design review when scheduled.

## 14. Closed checklist — Codex round 12, v1 scope only
Every item is either **RESOLVED** (design-level, with file:line evidence and the section where it is
addressed) or an **IMPLEMENTATION-TIME** check (verifiable only in code or in-world) with a named
acceptance test. Nothing is left open. Closed in the v2-v12 headers and not repeated: everything Codex
confirmed in R1-R10 (incl. R10: the packed module-state word and wrap-safe tick arithmetic). Accepted by
the user, not a finding: the one-frame limit (D30). Out of scope (D26): §4.2a, §4.2b option (a), test 9e.

Test ids. **Unit (§11 "Unit")**: U-BITS = "Two byte spaces"; U-OWNER = "Per-owner eligibility";
U-SPLIT = "Shadow split" (+ v12 `sCull` assertions); U-CASTER = "Caster predicate"; U-ZERO = "Zero
radius" (+ v12 `H_target == 0`); U-ABI = the `SLReShadeReaderInfo` bullet + the `SLReShadeKeyGuard`
`static_assert` in the key-guard bullet; U-GUARD = "Key guard"; U-BRACKET = "Bracket registry";
U-WL = "Whitelist"; U-GATE = "Viewer module gate"; U-SNAP = "Snapshot" (incl. the refusal latch);
U-COUNT = "Counters"; U-CONC = "Concurrency" (v13). **In-world (§11 checklist)**: W-<n> = item n (e.g.
W-9b, W-9c, W-9g, W-9i, W-9j, W-9k, W-9m, W-8c, W-10, W-16, W-17, W-9a). **Implementation checks defined here** (I-…): each is a debug-build
assertion or scripted check the implementer adds and the reviewer runs once.

| # | Item | Status | Evidence / acceptance |
|---|---|---|---|
| R5-C | Report timing: no viewer lock needed across `SwapBuffers` by the report path | **RESOLVED** | The report export copies the struct into atomics and takes no lock (§4.1 item 1b contract); the add-on calls it only after releasing `g_bracket_lock` (§4.2 bracket, `reshade_present`). Nothing in the report path can block on a viewer lock. Enforced additionally by I-LOCKRULE |
| R5-D | `*_since` counters catch a mid-frame `render_effects`; `_effects_rendered_this_frame` cannot hide one | **RESOLVED** | 6.7 runtime.cpp:3691-3702: the flag only stops a *second* call in the same present; the first call that draws passes 3701, raises `reshade_begin_effects` before the technique loop (3909), `reshade_render_technique` **after** each rendered technique (4213; v13 correction — it is not raised before), and `reshade_finish_effects` after the loop (3949); a call stopped at 3694/3701 draws nothing and raises nothing. Detection relies on begin (before) and technique/finish (after) all being counted for the present in which they occur. 6.8.0 semantics per Codex R5/R6 (upstream runtime.cpp, v6.8.0). Counted by the `*_since` counters (§4.2). Acceptance: U-COUNT and **W-9i(d) with a real API call** (v13: no synthetic-counter fallback) |
| R5-E | Lock discipline: recovery's `set_effects_state` cannot re-enter our handlers; counter handlers cannot nest in a locked region | **RESOLVED** | 6.7 runtime_api.cpp:1320-1323: `set_effects_state` only assigns `_effects_enabled` (no event, no rendering); the API doc (reshade_api.hpp:681) states any `reshade_set_effects_state` event goes to *other* add-ons. No technique renders inside it, so `reshade_render_technique`/`begin`/`finish` handlers cannot run while a bracket region holds the lock. Enforced by I-LOCKRULE |
| 1 | `mFxOwner` = the UUID the recorder receives in `coverageChanged(owner)`; fallback-owner coverage for worn animesh | **IMPLEMENTATION-TIME** | Both come from `getActorFxOwnerId()` (brief §5.1); verify in code. Acceptance: U-OWNER (incl. a worn-animesh fallback-owner case added to it) + W-7 (animesh on/off membership) |
| 2 | `maskDrawByteUncovered` = today's `owner_byte & ~coverage_bits` for rig-only configs | **IMPLEMENTATION-TIME** | Acceptance: U-OWNER "today's bit arithmetic gives the same answer" assertion, exhaustive over rig-only recipient permutations; U-BITS |
| 3 | Nothing between `renderDeferredLighting(MAIN)` and `renderFinalize` rebinds `mMainRT.actorMask`/`mActorKeyMatte` as a draw target (impostor generation was INFERENCE) | **IMPLEMENTATION-TIME** | I-REBIND (v13: both targets): every `LLRenderTarget` gets a debug-only write serial, advanced by `bindTarget`, `clear`, `copyContents`/blit **as destination**, `swap`, `allocate`/`release`. (a) `mMainRT.actorMask`: the serial is recorded when the mask (and the coverage replay) completes in the MAIN mask view, and `buildMatte` asserts it is unchanged when it samples the mask — no rebind or overwrite between mask completion and `buildMatte`; (b) `mActorKeyMatte`: recorded at the end of `buildMatte`, asserted unchanged by the key blit; (c) the matte frame stamp equals `gFrameCount` at the blit. The implementer also greps every use of `actorMask`/`mActorKeyMatte` as a destination and lists them in the WP1 review. Acceptance: I-REBIND silent + `ACTORKEY SUMMARY stale=0` over 5 min in a crowd of 20+ with impostors cycling (W-7 extended) |
| 4 | Explicit `LLGLDisable(GL_FRAMEBUFFER_SRGB)` in the blit changes nothing for beauty/off path; `LLGLState` tracks the enum consistently | **IMPLEMENTATION-TIME** | Evidence: every existing use is a scoped `LLGLEnable` (pipeline.cpp:6566, 6571, 24223) or a parameter (7704), all through `LLGLState`. Acceptance: W-17 (key OFF before/after pixel-identical), W-1, the self-test's `SRGB-LEAK` line never firing, and `LLGLState::checkStates()` clean after the blit (I-SRGB) |
| 5 | `diopter_warped`/`fisheye_warped`/`rlv_sphere_ran` are the only forced post-matte spatial filters; `@setoverlay` after the blit | **RESOLVED** | The only RLVa effect in pipeline.cpp is `RlvSphere` (22335-22338, the single `RlvActions::hasBehaviour` site); `@setoverlay` runs in `render_ui()` after `renderFinalize()` (llviewerdisplay.cpp:1747) and `gatherFrame()` (1751), at 1760-1764 — drawn over the key like UI, never moving pixels relative to the matte (§4 table row, v12) |
| 6 | Zero radius: no NaN when `H_target` is 0 | **RESOLVED** (design) + U-ZERO | Spec rule (v12, §11 U-ZERO): `H_target == 0` → `buildMatte` returns, matte invalid for the frame, key not applied; `r < 0.5 px` branch (§3.6) covers zero feather/choke. Acceptance: U-ZERO |
| 7 | `LLShadowCascadeScope::draw()` is a pure move of 26461-26680; nothing mutates `LLCullResult`/`mDrawFace` | **IMPLEMENTATION-TIME** | Acceptance: U-SPLIT (render-map and `mDrawFace` sizes unchanged after each of three draws; recorded beauty call list identical to the old `renderShadow`) + W-17 + W-9f |
| 8 | `sCull` remains `&result[j]` across the draws; `LLCullResult` not moved/swapped between draws | **IMPLEMENTATION-TIME** | Acceptance: U-SPLIT v12 assertions (`sCull == &result[j]` before/after each draw; address and sizes unchanged across `generateSunShadow`'s loop) |
| 9 | Owner availability for every draw info reaching shadow maps and rigged loops | **IMPLEMENTATION-TIME** | Corrected claim (v12): by the predicate (§5.2: `KEY_ONLY → !(maskDrawByte & keyMaskBits())`, `SET_ONLY → … != 0`), a draw with no owner has byte 0, so it is **absent from the actor map (a hole) and present in the set map**; for a keyed avatar's draw that would attribute its shadow to the set (Exact under-darkens, Fast misses it). The v5 wording "never set-map pollution" is withdrawn. Acceptance: I-OWNER — a debug counter `caster_no_owner` (draw infos whose avatar/attachment drawable belongs to a key recipient but whose owner is empty) in the `ACTORKEY SHADOW` line, required 0 over W-9c(i) (rigged-only avatar), W-7 (clones, animesh) and W-9a; + U-CASTER |
| 10 | Caster predicate in every `beginRenderMap` loop and `uploadMatrixPalette` site reachable from the shadow draw | **IMPLEMENTATION-TIME** | I-CASTERSITES: the WP3 reviewer greps every `beginRenderMap`/`uploadMatrixPalette` call reachable from `LLShadowCascadeScope::draw` and checks each against the §5.2 table; `CasterFilterLeakCheck` (debug) counts draws evaluated without the predicate (expected 0). Acceptance: I-CASTERSITES clean + W-9c(h)(i) |
| 11 | `sShadowCasterFilter == CASTER_ALL` during the beauty draw | **RESOLVED** (by construction) + check | Only `ShadowCasterFilterScope` sets it, and the beauty draw is outside any such scope (§3.1, §5.2). Acceptance: U-CASTER + `CasterFilterLeakCheck` + W-16 (AlphaCap counts identical key on/off) |
| 12 | `ensureActorShadowTargets` cannot disturb the caller's FBO/viewport; release on resize in `releaseSunShadowTargets` | **IMPLEMENTATION-TIME** | I-ALLOC: debug-build comparison of `LLRenderTarget::sCurFBO` and `glGetIntegerv(GL_VIEWPORT)` before/after `ensureActorShadowTargets`. Acceptance: I-ALLOC silent + W-9c(j) (toggle ON mid-session, resize while ON) + W-9h (teardown, no leak) |
| 13 | `bindSunShadowState` is the complete set the sun path of shadowUtil.glsl reads | **IMPLEMENTATION-TIME** | I-BINDER: after linking, enumerate the matte program's active uniforms (`glGetActiveUniform`) and assert every sun-shadow uniform is set by `bindSunShadowState`; the WP3 reviewer diffs the deferred shader's upload list before/after the extraction (must be identical). Acceptance: I-BINDER + W-9a |
| 14 | Fast-mode semantics (no darkening without an occluding keyed actor; Exact never misses a beauty actor shadow) | **IMPLEMENTATION-TIME** | Fast: documented limit (§5, §6 known-limits text), acceptance W-9a balcony case. Exact: **W-9m** (v13) — Exact-vs-beauty comparison for system-body, rigid-attachment, rigged-only and GLTF casters with the per-pixel `SHADOW-EXACT miss` count required 0 and a mode-2 snapshot cross-check |
| 15 | Bracket invariant across two runtimes and the 8-slot registry | **IMPLEMENTATION-TIME** | Acceptance: U-BRACKET (exhaustive event sequences over two runtimes, incl. stale-open, >8 runtimes, destroy/re-init while open) + W-9b(g) two swapchains + W-9g + W-9j(f) |
| 16 | `reshade_reloaded_effects` in the bracket cannot change `_effects_enabled` (incl. a failed compile) | **RESOLVED** (6.7) + check (6.8.0) | 6.7: `_effects_enabled` is written only at runtime.cpp:676 (Home toggle), runtime_api.cpp:1322 (`set_effects_state`) and its initialiser runtime.hpp:270 — no reload path writes it. I-68GREP (WP4 pre-coding step): the same grep on the fetched v6.8.0 tag must show no reload-path write. Acceptance: I-68GREP + W-9b(a) |
| 17 | Threads: which events run where; `g_bracket_lock` and the depth pin's `state_mutex` never nested | **IMPLEMENTATION-TIME** (v13 reclassified) + design rule | Corrected (v13, Codex R11): raw `present` and `reshade_present`, and the begin/technique/finish events raised by `on_present`'s own `render_effects`, run on the presenting thread (6.7 opengl_hooks_wgl.cpp:259-276 → runtime.cpp:586-830). The **public** `effect_runtime::render_technique` (6.7 runtime_api.cpp:1245-1313; begin 1303, finish 1309, technique event inside the private `render_technique`) and `render_effects` may be called by an add-on **outside** `on_present`, on whatever thread that add-on uses. v13 explicitly **supports** such off-present callbacks: the counter handlers take only `g_bracket_lock` (an SRWLOCK, any thread), increment and release; they call nothing else, so they cannot nest with `state_mutex` or the report; an off-present event between two presents lands in the `*_since` counters of the next present and refuses it (conservative). Only whitelisted add-ons can make such calls and none does (§4.2). Acceptance: **U-CONC** (v13) — add-on unit build with a fake runtime: 4 threads fire technique/begin/finish events concurrently with a presenting thread running raw-present/`reshade_present` brackets for 10⁶ iterations; counts equal the number fired, no deadlock (watchdog), I-LOCKRULE assertions silent, every present overlapping an off-present event reports CONTAMINATED. The depth pin takes `state_mutex` only inside its own functions (sl_depth_pin.hpp:89, 198-274); the design rule (§4.2) forbids calling any depth-pin function or the viewer report inside a `g_bracket_lock` region. I-LOCKRULE: a debug `thread_local bool t_in_bracket_lock` set inside every locked region and asserted false at every depth-pin entry point and in `report()` |
| 18 | ABI 1.3 additive; 128 B / 32 B layouts without padding; old add-on never calls the exports; new add-on tolerates their absence | **IMPLEMENTATION-TIME** (compile-time) | Acceptance: U-ABI (`static_assert` sizes and `offsetof` of every field; copy tests for 16/128/256 and ≥ 32); `GetProcAddress` null-path tests in the add-on unit build; W-9 stale-binary cases |
| 19 | Manifest determinism (sort, CRLF/BOM normalisation, generated header excluded, ABI header included) | **IMPLEMENTATION-TIME** | I-HASH: configure the add-on and the viewer from an LF checkout and from a `core.autocrlf=true` checkout; all four printed hashes and manifest lists must be identical; `sl_bridge_build_info.h` absent from the list; `indra/newview/llreshadebridgeabi.h` present |
| R10-A | Packed module-state word | **RESOLVED** (Codex R10) | §4.2; U-WL/U-GATE packed-word cases |
| R10-B / v12 | Refusal latch: automatic requests suppressed whenever latched except after an enumerated clearing event; non-clearing changes silent | **IMPLEMENTATION-TIME** | §4.4 item 2b (v12 rule). Acceptance: U-SNAP latch cases (incl. non-clearing changes interleaved) + W-8c(g) |

