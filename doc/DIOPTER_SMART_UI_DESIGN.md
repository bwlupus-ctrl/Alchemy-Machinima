# Ultimate Diopter — Smart UI Overhaul

**Design document — revision 6. No code changes, no builds, no git.**
Target: `I:\alchemy-machinima`
Companion artifact: **`diopter_relevance_table.md`** — the complete 181-row relevance table, and the implementation source of truth for `sRelevance`.
Date: 2026-08-27

---

## Change log

### rev 6 — post Codex delta scan of rev 5 (verdict: NOT READY, 7 reasons)

Table spot-checks (8/8) and the RAII/state-machine design passed; the remainder was implementation-grade exactness.

| Codex finding | Change | § |
|---|---|---|
| **1** | Enum lacked `PRED_LENS_POWERED` and `AL_PRED_COUNT`; the sweep driver never called `alAssertCoverage`. Both added to the enum; a TUT entry point now wires `alRunSweeps` → `alAssertCoverage`. | §2.3 |
| **3** | `{}` was both the clause terminator and a valid ALWAYS clause, so coverage loops skipped ALWAYS clauses. **Sentinels replaced with explicit counts** (`mClauseCount` / `mTermCount`); ALWAYS clauses are accumulated and asserted; `diopter_enabled` is the only `mClauseCount == 0` row. Layer-1 gains the count-bounds asserts. | §2.3 |
| **5** | `alDiopterInvalidateBank` ignored `LLFile::rename`'s error return. Now checked (like `alcinelightrig.cpp:3284-3301`): on rename failure — session-scoped refusal flag, log, notification; the two-failures-plus-crash residual risk is stated explicitly. | §6.3 |
| **Fly's Eye** | Changelog "7-row inventory" wording, the min-height rationale's "each ... 7-row band", and the Source-group count (5 → **4**, Offset X/Y is a §3.7 paired row like Center X/Y) reconciled with the authoritative §2.2 layout. | changelog, §2.2, §4.2 |
| **N1** | Table↔schema parity rule added: a column exists iff a schema field exists; Setting is header-marked DERIVED; the layer-1 validator parses the table header. Companion table regenerated with the `mSection` / `mPresetOwned` / `mTier` columns. | §2.3, table |
| **N2** | `diopter_mag_scale` gains `PRED_LENS_POWERED` (Magnify Scale multiplies by `strength_d`, `pipeline.cpp:14249-14252`; additive Magnify Trim correctly does not) — new illustrative row + table row; S8 flips the predicate with no corpus change. | §2.3, table |
| **N3** | Predicate-glossary exactness made a stated rule: every entry quotes the exact renderer expression with file:line — `PRED_WARP_ARMED`'s real `1e-3`/`1e-2`/zoom-delta thresholds (`pipeline.cpp:14347-14352`) replace the paraphrased `>0`/`!=1`. | §2.3, table |

### rev 5 — post Codex delta scan of rev 4 (verdict: NOT READY, 5 reasons)

Rev 4's four nits were resolved and the 8-sweep corpus was verified to flip all predicates. The remaining objections were "prose where code is required" (4) plus two real logic holes.

| Codex finding | Change | § |
|---|---|---|
| **1** | Coverage bitmap was prose. **Actual test code** now given: bitmap declarations, the per-case update inside the sweep loop, and both end-of-test assertions (naming the failing predicate / the failing row and clause index). `PRED_NONE` explicitly excluded from the flip rule — it is a "this term uses a setting mask" sentinel with no truth value. | §2.3 |
| **2** | `// ... 181 rows total` blocked clause-coverage verification. **The complete table is now a companion artifact** — `diopter_relevance_table.md`, 181 rows, 194 clauses, with per-row clauses in OR-of-AND form, reset name or null, and the UNCONDITIONAL flag. §2.3 keeps ~10 illustrative rows and defers to it. | §2.3 |
| **3** | `UNCONDITIONAL` was claimed in the meta-rule but absent from the struct. Added `mUnconditional` to `ALDiopterRelevance`, with the implicit-Enabled-term rule that makes its population exactly 1. | §2.3 |
| **4** | `ScopedPresetMaterializing` was named but never defined. **Full RAII class given** — ctor *saves* the prior flag value, dtor *restores* it, so nesting is safe (a naive set-true/set-false would clear the flag on the inner scope's exit and expose the outer scope's remaining writes). | §6.3 |
| **5** | **Listener re-entry hole.** The selection listener never checked `sDiopterPresetMaterializing`, so the guarded write-back revert would still have re-entered it. Added the guard check as the listener's first statement, matching the shipping owned-setting listeners (`llviewerfloaterreg.cpp:605-608`). The test's "no re-entry" assertion is now satisfiable. | §6.3 |
| **6** | **Stale-bank refusal was memory-only**, so a restart would read the stale file as valid — and rev 4 even let a later load clear the invalid flag. Invalidation now reaches disk: the bank file is atomically renamed to `.stale` via the same temp→rename path, and the loader refuses `.stale`/tombstoned banks **permanently** until a new successful snapshot replaces them. Operation order stated once, canonically. | §6.3 |
| **7** | Three inconsistent Fly's Eye inventories. **One authoritative inventory** (a 3-row pattern band plus standing Source/Cell groups — *not* 7 rows; that number conflated band with groups) now stated in §2.2 and referenced from §3.6, §3.8c and §6.1; Breathing and Merge are separate rows (documented as *not* paired). | §2.2, §3.6, §3.8c, §6.1 |

### rev 4 — post Codex delta scan of rev 3 (verdict: NOT READY, 2 reasons)

Rev 3 closed blockers 3–6 and all extras with tree-verified evidence. Two blockers and four consistency nits remained.

| Codex finding | Change | § |
|---|---|---|
| **B1** | Sweep corpus did not exercise `PRED_EDGE_WOBBLE_ARMED` or `PRED_HANDHELD_ARMED`. Sweeps 2 and 3 now vary Handheld and Edge Wobble; **an independent audit found four more unexercised predicates** (Ghosts, Seam, Aberration, Field Curvature), so two new sweeps were added. Corpus re-totalled **~1200 → 1576** (rev 6 later adds S0's 4 enabled-axis cases → the current total of **1580**). New **meta-rule the oracle test enforces: a predicate no sweep flips in both directions is a test failure**, extended to a clause-coverage rule that also catches dead clauses. | §2.3 |
| **B2** | **Snapshot-failure abort path was broken.** `LLControlVariable` signals fire *post-write* (`llcontrol.cpp:225-263` — `firePropertyChanged` runs after `mValues` is mutated), so rev 3's listener `return` did **not** revert the preset; the combo showed a preset that was never materialized, and a later user-selected Custom would have restored a stale bank over live settings. Redesigned as an explicit four-step failure path: write-back revert under the suppression flag, skip materialization, mark the in-memory bank invalid so a later restore refuses the stale file, and notify. | §6.3 |
| **B2 (consistency)** | Phase 1 table said `Diopter.ResetControl` sets the reason flag directly; §6.3 correctly routes resets through the owned-setting listener. Phase 1 corrected to match. | §7 |
| **N1** | §1.4 still described On-Lens as killing Size "as a mask boundary only" — contradicting the corrected all-three-paths text. Aligned. | §1.4 |
| **N2** | The regroup adds a ninth tab (Halo & Ghosts), but the Invariant W sizing and check still said "all eight tabs". Reconciled, with current-8 vs proposed-9 disambiguated. Ghost Copies has **seven** dependents, not six. | §3.6, §3.8b, §4.2 |
| **N3** | Overlay inventories understated: the Motion band needs **Speed + six Stutter sliders** (7 rows), and the Fly's Eye example omitted **FX Flow**, which §2.2 marks live for mode 20. | §3.6, §3.8c, §6.1 |
| **N4** | Phase 1 claimed "no layout change" while widening sliders and resizing the floater. Claim dropped (the widening is low-risk and stays). | §7 |

### rev 3 — post Codex delta scan of rev 2 (verdict: NOT READY, 6 reasons)

Rev 2 resolved 5 of 12 amendments outright and survived a 9-row spot check of the self-audit with zero stale citations. Six items were still under-specified or wrong. Rev 3 closes all six.

| Codex finding | Change | §
|---|---|---|
| **A1** | Branch validator was prose. Now names the concrete artifacts — `aldiopterrelevance.cpp/.h` as an LLUI-free model unit, `tests/aldiopterrelevance_oracle.py` as the checked-in extractor, `tests/aldiopterrelevance_test.cpp` under `LL_ADD_INTEGRATION_TEST` — and mandates **cross-product** sweeps (shape × placement × profile × warp; motion × pulse-target × content), not one-enum-at-a-time. D2 is the proof single sweeps are insufficient. | §2.3 |
| **A3** | **Custom-bank state machine was destructive.** The existing edit flow materializes then sets `CineDiopterPreset = 0` (`llviewerfloaterreg.cpp:599-615`), so rev 2's listener would have seen `id == 0`, restored the old bank, and **erased the edit the user just made**. Rev 3 adds an explicit transition-reason flag, plus atomic persistence, schema versioning, scope, and load ordering. | §6.3 |
| **A2** | Phase 3 admitted a scrollbar could still appear at minimum size — leaving the wheel hazard live. Rev 3 commits to **the hard invariant** (scrollbar never visible in any supported mode/size), machine-checked in the same test target, with the ancestor-gate named as the only sanctioned fallback. | §4.2, §3.8b |
| **A12** | "10 combos + kaleido equivalents" was not an implementable allowlist. Rev 3 carries the **complete named inventory of all 25 combos** and opts out every one. | §4.5 |
| **A10** | The validator's "every `mResetName` must resolve" could never pass — `CineDiopterEnabled` has no reset button (`floater_ultimate_diopter.xml:48-54`). Nullable-reset exception added to schema and validator. | §2.3 |
| **D1** | **On-Lens Size contradiction.** Rev 2 §2.1 claimed Size still scales refraction and the warp under On-Lens; it does not — the CPU **discards `eff_size`** and recomputes `glass_radius` from centre, frame corners and the animated `size_scale` only (`pipeline.cpp:14279-14296`). Rev 2's own tooltip said the opposite. Every Size clause now carries `PlacementMode == framed`. | §2.1, §2.3 |
| **D2** | FX Flow direct-user mask typo: `0xFFF200u` omits bit 11 (Vortex reads `ft` directly, `glsl:406-418`). Corrected to **`0xFFFA00u`**. | §2.3 |
| **A4** | Stale citation corrected: settings load is `llappviewer.cpp:819-821` (`initConfiguration`) and `:2819-2820` (`loadSettingsFromDirectory("User")`); floater registration stays `:961`. | §6.3 |
| **C1** | Adjudicated **INTENDED, no renderer change.** Wave is *edge* undulation; Full Frame pins coverage to 1 (`glsl:234-237`) and On-Lens has no shape SDF (`glsl:200-205`). Reclassified from "probable renderer bug" to a UI fact, with a relevance rule and a tooltip. | §2.1, §2.4 |
| **C2** | Adjudicated **INTENDED, no cross-tab bug.** `CineDiopterWobbleFreq` is *spatial* edge-wave density (`shape4.y`); the animation phase is a separate uniform (`shape4.z`). Rev 2's "undocumented cross-tab coupling" was a mislabel. Relevance rule: WobbleFreq is relevant when **static Edge Wobble is armed OR Motion = Wave is armed**. | §2.1, §2.4 |
| **A6** | Phase 1 ship-alone status restored — it was contingent on A3, which is now fixed. | §7 |

### rev 2 — post Codex adversarial scan (verdict: SOUND WITH AMENDMENTS 12)

Rev 1's direction survived verification; its *matrix data* and *phasing* did not. Changes:

| Codex finding | Change |
|---|---|
| 1.3 | Headline recount. Factory default is `CineDiopterEnabled = 0` (`settings.xml:90`, session-only), and the whole effect is skipped at `pipeline.cpp:19037-19046` → **180 of 181 inert at literal startup**. The 128/181 figure is re-scoped to "after enabling, Diopter tool, camera-focus resolving". New **master-enable tier** added to §1.4. |
| 1.4 | Size is **OR-consumed** — the mask may ignore it while refraction (`glsl:334-343`) and halo warp (`glsl:398-400`) still use it as the glass radius. Shape matrix restructured; §2.1 now separates mask consumption from glass-radius consumption. |
| 1.5 | Aperture Rotation membership corrected to **{1,2,3}** (Heart subtracts `rot`, `glsl:479-486`); Anamorphic uses its own angle uniform. |
| 1.7 | Mirror Tile (kaleido 2) never consumes `feed` → Source Angle/Spin inert there. "Always live" claim removed. |
| 1.8 | FX Flow reaches modes 3/4/5/8/10 only through `kal_cellScale`'s `ftime`, which is a no-op unless Cell Breathe > 0 (`glsl:110-118`). New Tier-C predicate `PRED_CELL_BREATHE_ARMED`. |
| 1.17 | Polygon Prism (mode 18) **does** consume wave — added to the Wave set. |
| 1.22 | **Rule model rebuilt as OR-of-AND clauses** (§2.3). The ANDed-predicates schema could not express OR-consumption. Validation strategy specified against branch tests. |
| 2.4 | Snapping-bypass mechanism corrected: `precision_override` exists but the wheel path never passes it. Conclusion (coarse ×10 only) unchanged. |
| 2.5 / 6.8 | Combo wheel **commits** (`llcombobox.cpp:958-997`). Escalated from "verify acceptability" to a required scoped opt-out. |
| 2.6 / 6.2 | **Re-phased.** Phase 1 is no longer ship-alone: `wheel_adjust` + retained scroll containers means an ordinary scroll gesture mutates sliders. Plain wheel deferred to the phase that resolves scrolling; modifier-gated interim documented. |
| 3.2 | "44 controls lying" corrected — 44 is the **write-list** size; only the 8 warp fields neutralize for every preset. Lying count is per-preset and varies. |
| 3.3 / 3.7 | Kaleido ownership corrected to **49** fields → 49 of 58 non-authoritative (not 51). Protect Center X/Y are the non-owned pair. |
| 3.5 | **Materialize-on-selection redesigned.** Rev 1 would have overwritten 44/49 `Persist=1` Custom settings, flushed to disk at logout (`llappviewer.cpp:1989-1992`). Now specifies persistent shadow storage for the Custom bank. |
| 3.6 | Added **one-time startup materialize** after listener install — connecting a signal does not invoke it, so a saved non-Custom preset would never sync. |
| 4.6 | Hover-write→Custom risk **scoped to the kaleido picker only**; diopter focus settings are not in the diopter owned array. |
| 4.7 | Fallback warning must report **provenance** (live DoF / alt-zoom / manual). New enum in §5.3. |
| 5.3 | "Active controls only = Off ⇒ everything visible" was incompatible with same-rect overlays. **Off redefined** as "show inactive controls within the selected mode". |
| 6.1 | Muscle-memory toggle moved from Phase 5 to **the first release that hides anything**. |
| 6.3 | Scroll-container removal collides with `min_height=420` + saved rects (`llfloater.cpp:976-1007`). Strategy added. |
| 6.4 | Runtime `setMinValue`/`setMaxValue` leaves an off-track thumb. Clamp-and-commit specified. |
| 6.5 | Uniform Macro/Room/Set/Vista table replaced with **per-control range tables**. |
| 6.6 | **Reset buttons folded into the relevance model** — reset writes immediately and can trigger materialization. |
| 6.7 | **Focus transfer** on hide/swap specified (`setVisible`/`setEnabled` do not release focus). |

**Independent re-audit (not sampled by Codex): 11 further defective rows corrected.** See §2.4.

---

## 0. Executive summary — the five headline moves

1. **The floater is never more than 68% relevant, and once enabled ~71% of it is inert. Gate it.** Add a real `ALFloaterUltimateDiopter` C++ class whose only job is a data-driven relevance pass: an **OR-of-AND clause table** maps every control to the branches that consume it, and one `refreshRelevance()` walks it. **Hide by mode, dim by armed-state** — a three-tier rule, not one strategy.
2. **Fitting the tabs kills the scroll containers — which is what unblocks the mouse wheel.** The eight `scroll_container`s exist only because the flat lists are 560–820px tall. Hide the dead controls and every tab drops under the viewport. **This ordering is now mandatory, not merely elegant:** turning on `wheel_adjust` while the scrollers remain makes an ordinary scroll gesture silently mutate settings.
3. **Mouse wheel is nearly free: `wheel_adjust` already exists in this fork.** `LLSlider::handleScrollWheel` (`llslider.cpp:290`) is already implemented behind an `[AL]` opt-in param, already used in `floater_director.xml` and `panel_cine_light_rig.xml`. The work is sequencing and a coarse modifier, not a feature build.
4. **Presets currently lie — and fixing it naively destroys the user's Custom bank.** While a non-Custom preset is selected, the sliders are not authoritative for the 44 diopter / 49 kaleido preset-owned fields. Materializing on selection fixes the display but overwrites 44/49 *persisted* settings, so it must ship **with shadow storage for the Custom bank and a one-time startup sync**.
5. **Depth is hard because it is dimensionless.** Add an eyedropper (`Pick…` → click in world → camera distance lands in the slider), a live "subject is at 12.4 m" readout **with provenance**, and per-control range presets — not log sliders.

---

## 1. Diagnosis — why the floater is hard

### 1.1 The raw census

`floater_ultimate_diopter.xml` — 1829 lines, 152 KB, floater 520×640 (min 520×420).

| Widget | Count |
|---|---|
| `slider` | 149 |
| `combo_box` | 25 |
| `check_box` | 7 |
| **Value controls (subtotal)** | **181** |
| `button` (per-control mini reset) | 180 |
| `panel` | 16 |
| `scroll_container` | 8 |
| `tab_container` | 1 |
| **Total widgets** | **361** |

`settings.xml` contains exactly **181 `CineDiopter*` keys** — a perfect 1:1 with the value controls. Every control is a direct `control_name` binding; there is no C++ floater class.

### 1.2 The finding that explains the "grouped improperly" gripe

**The floater contains zero `<text>` elements, zero `<view_border>`, zero `<layout_stack>`, zero icons.**

```
grep -c "<text|<view_border|<layout_stack|<icon" floater_ultimate_diopter.xml  →  0
```

The only structure in the entire file is **eight XML comments** (`<!-- ===== Shape ===== -->`) which are invisible at runtime. What the director actually sees is eight flat scrolling columns of 14–30 visually identical slider rows, ordered by the sequence in which the features were implemented, with no headings, no rules, no grouping, and no visual hierarchy of any kind.

"Everything is grouped improperly and too far apart to make sense" is not a perception problem. **There is literally no grouping.**

Worse, the ordering is *implementation order*, so semantically adjacent things are separated. Concrete examples:

- **Shape** and its shape-specific parameters are on the Shape tab, but **Size**, **Angle**, and **Center X/Y** — which are shape parameters — are on **Main**, four tabs away.
- **Spin Mode** and its five spin sliders sit at the bottom of **Motion**, below Handheld, even though Spin is an independent system from the Motion combo above it.
- **Faceted Fold** (`diopter_pattern_mode` + 3 sliders) lives on **Bokeh**, but its output is a *halo warp* that is armed jointly with Ring Fold / Twist / Lobe Amount on **Glass**. The five controls that together arm one shader branch are split across two tabs.
- **Bokeh Highlight** and **Window Surround** are on **Focus**; **Aperture Shape** is on **Bokeh**. They are the same subsystem.
- **Magnify Scale/Trim** are on **Bokeh**; the thing they scale (`strength_d`, from Power / Base / Lens) is on **Focus**.

### 1.3 Layout mechanics that make this rigid

Every control is absolutely positioned with `left=` / `top_pad=` / `top_delta=`. `top_pad` is resolved **at build time**, not at layout time. Consequence: calling `setVisible(false)` on a control today leaves a **hole**, it does not reflow. This is the single most important constraint on the gating design (see §3.2).

Typical row geometry (`diopter_blend`):

```xml
<slider name="diopter_blend" left="12" top_pad="10" height="16" width="360"
        label="Master Blend" label_width="140"
        min_val="0" max_val="1" increment="0.01" decimal_digits="2"
        control_name="CineDiopterBlend" />
<button left="376" width="18" height="18" top_delta="-1" image_overlay="Refresh_Off"
        name="rst_CineDiopterBlend" commit_callback.function="Diopter.ResetControl"
        commit_callback.parameter="CineDiopterBlend" />
```

Row pitch is 20–22px; inner panels are 486 wide inside a 508 scroll viewport inside a 512 tab inside a 520 floater. **34px of the 520 width is pure chrome.** The right-hand 90px of every inner panel (x=394..486) is empty.

### 1.4 Dead controls — the core artifact

Every mode combo in this floater gates a shader branch. When a branch is off, its sliders still render, still accept input, still write settings, and **do nothing**. There is no feedback of any kind.

The gates, verified in the shaders and `pipeline.cpp`:

**Tier 0 — the master enable.** `CineDiopterEnabled` defaults to **0** (`settings.xml:90`, and it is session-only — not persisted), and the entire post pass is skipped at `pipeline.cpp:19037-19046`. **At literal first startup, 180 of 181 controls are inert; the only live control is the enable checkbox itself.** Every count below is therefore stated *after* enabling. The UI must treat this as its outermost gate: with Enable off, the correct display is the whole floater dimmed with a single "Turn on Ultimate Diopter to use these controls" affordance — not 181 controls that appear operable.

| Gate | Location | Kills |
|---|---|---|
| **`CineDiopterEnabled == 0`** | **`pipeline.cpp:19037-19046`** | **all 180 non-enable controls** |
| `CineDiopterToolMode` | `pipeline.cpp:19054/19063` | the other tool's entire 6 or 2 tabs |
| `profile <= 0` → early return | `ultimateDiopterGatherF.glsl:336` | IOR, Thickness, Rim Width, Rim Warp, Rim Caustic, Rim Darken |
| `shape <= 0` → return 1.0 | `ultimateDiopterGatherF.glsl:456` | Sides/Points, Rotation, Blade Curvature, Star Inner, Anam Squeeze, Anam Angle |
| `diopter_halo2.w < 0.5` → return uv | `ultimateDiopterGatherF.glsl:390` | Halo Rings, Ring Phase, Lobe Count, Lobe Phase, Segments, Feed Angle |
| `ghostCount > 0` | `ultimateDiopterF.glsl:107` | Ghost Spacing, Arc Smear, Radial Smear, Highlight Threshold, Highlight Knee, Arc Energy, Dispersion |
| `content == 1` | `ultimateDiopterGatherF.glsl:593` | Window Surround (only lives under Sharp Window) |
| **`content == 1`** (overrides) | **`pipeline.cpp:14261-14266`** | **Pulse Target = Focus, Stutter Focus, Handheld Focus Breath, Magnify Scale/Trim** — `lens_focus_m` and `magnify` are both overwritten |
| **`content != 1` guards** | **`pipeline.cpp:14092`, `:14165`** | Stutter Focus and Focus Breath are additionally guarded *inside* their motion branches |
| `diopter_comp.x > 1e-4` | `ultimateDiopterF.glsl:90` | Seam Double Amount (unless Seam Double px > 0) |
| `motion == N` chain | `pipeline.cpp:14034-14149` | every motion slider not owned by the selected mode |
| `spin_mode == 0` | `pipeline.cpp:14174` | Spin Travel, Duration, Overshoot, Delay |
| `handheld > 0.f` | `pipeline.cpp:14154` | Handheld Speed, Gait, Angle Sway, Focus Breath |
| `placement == 1` (On-Lens) | `pipeline.cpp:14289-14296` + `:13978` | **Size outright** — the CPU discards `eff_size` and recomputes the radius, so all three consumption paths are cut (§2.1); also Wobble, Corner Rounding, Hollow-as-mask, the 10 shape-specific sliders, and forces Content to Diopter |
| `f_focus_mode == 1` **and a focus target resolves** | `pipeline.cpp:14204-14232` | Base Distance (m) — **conditionally**; see §2.1 |
| `f_lens_mode == 1` / `== 0` | `pipeline.cpp:14235` | Power (+D) **or** Lens Distance (m) — always exactly one is dead |
| **`strength_d == 0` or `character == 0`** | **`pipeline.cpp:14253-14256`** | **Lateral CA, Axial CA, Field Curvature, Edge Vignette** |
| `kmode` chain | `ultimateKaleidoF.glsl:273-673` | see §2.2 |
| **Cell Breathe `== 0`** | **`ultimateKaleidoF.glsl:110-118`** | **FX Flow, in modes 3/4/5/8/10 only** (its sole path there is `kal_cellScale`'s `ftime`) |
| `pmode > 0` | `ultimateKaleidoF.glsl:710` | Protect Radius/Feather/Anchor/Center X/Y, Depth Cut, Depth Feather, Depth Invert |
| `kmode == 0` | `ultimateKaleidoF.glsl:694` | Seam Soften (Radial Mirror only) |
| `preset_id != 0` | `pipeline.cpp:13574` / `:14549` | **44 diopter / 49 kaleido preset-owned fields** (§1.5) |

**Not a gate — an OR-consumption.** Several controls feed more than one independent shader path, so a single mode check is not sufficient to call them dead. The three that matter: **Size** (mask boundary ∨ refraction radius ∨ halo-warp annulus outer), **Hollow** (mask onion ∨ halo-warp annulus inner), and **Surround Max Blur** (field-curvature floor ∨ Sharp Window floor). This is what forces the OR-of-AND rule model in §2.3.

### 1.5 The preset lie — the worst offender

Both resolvers implement "preset owns the look":

```cpp
// pipeline.cpp:13574  (diopter)
if (preset_id != 0U) { ring_fold = 0.f; twist_deg = 0.f; lobe_amt = 0.f;
                       pattern_mode = 0; pattern_zoom = 1.f; ... }
switch (preset_id) { case 1: shape = 1; content = 0; character = 0.2f; ... }
```

```cpp
// pipeline.cpp:14549  (kaleido) — comment: "Every non-Custom preset fully owns
// the whole style/motion block: baseline everything to the reference defaults"
if (preset_id != 0U) { mode = 0; segments = 8.f; angle_deg = 0.f; twist_deg = 0.f;
                       edge_wrap = 0; ring_count = 4.f; ... all 49 fields ... }
```

**The two tools behave differently, and the distinction matters for the fix.**

- **Diopter.** `ALDiopterLook` carries **44** preset-overridable fields, and `materializeDiopterPreset` (`pipeline.cpp:13745`) writes all 44. But the `preset_id != 0` block only neutralizes **8** fields unconditionally (the warp block, `:13574-13583`); each `case` then overrides a *varying subset* — 11 fields for Split Diopter, 23 for Halo FX – Full. **So 44 is the write-list size, not the lying count.** The set of currently-lying controls is per-preset: every field the active `case` assigns, plus the 8 warp fields. It ranges from ~19 to ~31.
- **Kaleido.** `ALKaleidoLook` carries **49** preset-owned fields (`pipeline.cpp:14427-14439`), and the baseline block resets **all 49** before the `case` runs (`:14549`). So the kaleido genuinely does lie about all of them at once. The two non-owned controls are **Protect Center X/Y**; adding Blend, Setup View, Freeze, Freeze At and the preset combo, **49 of 58 kaleido controls are non-authoritative under any non-Custom preset.**

`materializeDiopterPreset` runs **only when the user edits a preset-owned control**. Until that first edit, the sliders show whatever was there before, and the render ignores them.

**This is the single biggest comprehension failure in the tool.** The director selects "Vintage Swirl", sees Element Profile reading "Off" and IOR reading 0.50, and is looking at Biconvex at 0.60. Then they drag any preset-owned slider and dozens of *other* sliders visibly jump. That is indistinguishable from a bug.

**And it is worse on a fresh login than in-session.** The preset settings are persisted, but the listeners that would materialize them are registered at `llappviewer.cpp:961` — *after* settings load (`initConfiguration()` at `:819-821`, then `loadSettingsFromDirectory("User")` at `:2819-2820`) — and connecting a `boost::signals2` slot does not invoke it. So a director who quits with "Vintage Swirl" selected comes back to a floater whose sliders have never been synced to it at all. Any fix must include an explicit one-time sync at startup (§6.3).

### 1.6 Dead-control count in a typical configuration

Defaults confirmed from `settings.xml`: ToolMode=0 (Diopter), Preset=0 (Custom), Shape=1 (Split), Content=0, Placement=0, GlassProfile=0 (Off), ApertureShape=0 (Round), PatternMode=0, MotionMode=0 (Static), SpinMode=0, GhostCount=0, RingFold=0, Twist=0, LobeAmt=0, PatternZoom=1 (warp inactive), FocusMode=1 (camera), LensFocusMode=0 (power), SeamGhostPx=0.

**Configuration 0 — literal first launch.** `CineDiopterEnabled = 0`, the pass is skipped entirely (`pipeline.cpp:19037-19046`): **180 of 181 dead — 99.4%.** This is the state every new user meets, and the floater currently presents it identically to a fully operational one.

**Configuration A — enabled, otherwise factory default, Diopter tool:**

| Tab | Controls | Live | Dead | Dead % |
|---|---|---|---|---|
| Main | 14 | 12 | 2 | 14% |
| Shape | 22 | 8 | 14 | 64% |
| Focus | 15 | 12 | 3 | 20% |
| Glass | 27 | 10 | 17 | 63% |
| Bokeh | 16 | 7 | 9 | 56% |
| Motion | 29 | 4 | 25 | 86% |
| Kaleido | 28 | 0 | 28 | 100% |
| Kal Anim | 30 | 0 | 30 | 100% |
| **Total** | **181** | **53** | **128** | **71%** |

**Configuration B — Kaleidoscope tool, Custom preset, Radial Mirror, Static, Protect Off:**
160 of 181 dead — **88%**.

**Configuration C — Kaleidoscope tool, any non-Custom preset (e.g. "Vortex"):**
123 diopter + 49 preset-owned = **172 of 181 non-authoritative — 95%**, and 49 of those are actively displaying values the renderer ignores.

**The floater's structural ceiling is 68%** — the six diopter tabs (123 controls) and two kaleido tabs (58 controls) are mutually exclusive, so at least 32% of the widget count is dead at all times, unconditionally.

### 1.7 Per-tab dead-control detail (Configuration A)

**Motion — 25 of 29 dead.** The worst tab. With Motion=Static, `tm` is computed and never read, so **Speed** itself is dead. Dead: Speed, Sweep Direction, Sweep Range, Ping-Pong, Pulse Target, Pulse Amount, Wave Amplitude, Path Freq X/Y, Path Phase, Path Amplitude, Stutter Rate/Position/Angle/Size/Focus/Smooth (17, motion mode); Handheld Speed, Gait, Angle Sway, Focus Breath (4, `handheld == 0`); Spin Travel, Duration, Overshoot, Delay (4, `spin_mode == 0`). Live: the Motion combo, Handheld, Spin Mode, Spin Rate.

**Glass — 17 of 27 dead.** Profile=Off kills 6. Warp inactive kills Halo Rings, Ring Phase, Lobe Count, Lobe Phase (4) — note Ring Fold, Twist, and Lobe Amount stay live because they are the *arming* controls. Ghost Copies=0 kills 7.

**Shape — 14 of 22 dead.** Shape=Split's *mask* uses only Stretch, Feather, Invert, Wobble Amt/Freq, Split Curvature (+ Shape, Content). Split's mask does not read `S`, and with Profile=Off and the warp disarmed nothing else reads it either, so **Size (on the Main tab) is dead too** in this specific configuration — a cross-tab dead control the user has no way to discover. **But Size is not shape-dead in general**: arm refraction or the warp and it becomes the glass radius for *every* shape including Full Frame and Split (§2.1). The same is true of Hollow, which the halo warp reads as the annulus inner radius regardless of shape. Arc Length and Broken Segments require `shape >= 4`; Corner Rounding requires shape ∈ {6,7,9,10}.

**Bokeh — 9 of 16 dead.** Round aperture kills 6. Faceted Fold=Off kills Segments and Feed Angle (Source Zoom stays live — it arms the warp). Seam px=0 kills Seam Double Amount. Magnify Scale/Trim are live here only because Profile=Off *and* Content=Diopter; either one flipping kills both.

**Focus — 3 of 15 dead, but one of them conditionally.** Lens Distance (power mode wins) and Window Surround (Sharp Window only) are unconditionally dead. **Base Distance is dead only if a focus target actually resolves.** In camera-focus mode the code tries `sLastFocusPoint` when `dof_focus_live`, else falls back to `gAgentCamera.getFocusGlobal()` — and if there is no region, the focus point is zero, or the forward distance is ≤ 0.05, **`base_focus_m` silently stays on the manual slider** (`pipeline.cpp:14213-14231`). So this control is live, dead, or dead-via-fallback depending on runtime state the UI must query rather than infer. This is the tab the user complained about ergonomically rather than structurally — and this three-way ambiguity is a large part of why.

**Main — 2 of 14 dead.** Freeze At (Freeze off), Size (Split shape).

---

## 2. Mode → relevant-controls matrix

### 2.1 Diopter

**Shape (12 modes) × Shape-tab parameters — MASK consumption only.** From `ud_mask` / `ud_edgeProfile` (`ultimateDiopterGatherF.glsl:134-325`). `*` = the control lives on the Main tab. **Read this table together with the OR-consumption table below it — three of these columns are not the whole story.**

| Shape | Size*ᴼ | Angle* | Stretch | Feather | Hollowᴼ | Arc/Broken | Round | Wobble | shape-specific |
|---|---|---|---|---|---|---|---|---|---|
| 0 Full Frame | – | – | – | – | – | – | – | – | – |
| 1 Split | – | ✓ | ✓ | ✓ | – | – | – | ✓ | Split Curvature |
| 2 Ramp | ✓ | ✓ | ✓ | – | – | – | – | ✓ | – |
| 3 Bar | ✓ | ✓ | ✓ | ✓ | ✓ | – | – | ✓ | – |
| 4 Circle | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | – | ✓ | – |
| 5 Squircle | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | – | ✓ | Squircle Exponent |
| 6 Polygon | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | Polygon Sides |
| 7 Star | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | Star Points, Star Inner |
| 8 Heart | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | – | ✓ | – |
| 9 Flower | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | Flower Petals, Petal Depth |
| 10 Blob | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | Blob Seed, Blob Irregularity |
| 11 Crescent | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | – | ✓ | Crescent Bite, Crescent Shift |

**ᴼ OR-consumed — the mask table above is necessary but not sufficient:**

| Control | Path 1 (mask) | Path 2 | Path 3 | Truly inert only when |
|---|---|---|---|---|
| **Size** | shapes 2–11 (`glsl:249,264,277,281,291`) | refraction radius, `glsl:334-343` | halo-warp annulus outer, `glsl:398-400` | **`placement == On-Lens`** (all three paths at once), **or** mask-doesn't-use-it **AND** `profile == 0` **AND** warp disarmed |
| **Hollow** | shapes 3–11 (`glsl:295-299`) | halo-warp annulus inner (`innerR = outerR * hollow`, `glsl:399`) | — | `shape < 3` **AND** warp disarmed |
| **Surround Max Blur** | field-curvature floor (`glsl:592`) | Sharp Window floor (`glsl:593-594`) | — | `field_curve == 0` **AND** `content != 1` |

Angle, Stretch, Feather, Corner Rounding, Wobble and the ten shape-specific sliders are genuinely mask-only — neither `ud_glassRefract` nor `ud_haloWarp` reads them.

**On-Lens kills Size outright — all three paths, not just the mask.** Rev 2 got this wrong. Placement = On-Lens replaces the mask branch entirely (`glsl:200-232`), Content is forced to Diopter (`pipeline.cpp:13978`), and critically the CPU **discards `eff_size` and recomputes the uploaded radius from scratch**:

```cpp
// pipeline.cpp:14289-14296
F32 glass_radius = eff_size;
if (placement == 1)
{
    const F32 aspect = out_w / out_h;
    const F32 dx = llmax(center_x, 1.f - center_x) * aspect;
    const F32 dy = llmax(center_y, 1.f - center_y);
    glass_radius = sqrtf(dx * dx + dy * dy) * llclamp(size_scale, 0.3f, 2.f);
}
```

Since `diopter_shape.z` is the *only* radius every downstream consumer sees — mask, `ud_glassRefract`, `ud_haloWarp` alike — the Size slider reaches none of them under On-Lens. The replacement radius depends only on centre, aspect and the **animated** `size_scale` (Pulse: Size, Stutter Size), which is why size-targeted motion still breathes the falloff reach while the static slider does nothing. **Every Size relevance clause therefore carries `PlacementMode == framed` as a term** (§2.3), and the tooltip at §6.4 is the correct one.

Under On-Lens only Angle, Stretch, Feather, Arc Length and Broken Segments survive as mask controls; Size, Wobble, Corner Rounding, Hollow-as-mask and all ten shape-specific sliders go dark.

**Edge Wobble Frequency is shared spatial density, not a coupling bug** (Codex C2, adjudicated INTENDED). The edge-wave term is:

```glsl
// glsl:256-259
wobY = shape4.x * sin(q.y * shape4.y)                              // static wobble
     + shape4.w * sin(q.y * shape4.y * 1.31 + shape4.z);           // Wave motion
```

`shape4.y` is Wobble Frequency — the *spatial* density of the edge wave, correctly labelled and legitimately shared by both terms. `shape4.x` is the static Edge Wobble amount, `shape4.w` is Wave Amplitude × gain, and the animation phase is a separate uniform `shape4.z` (`t * MotionSpeed * TAU`). Rev 2's "undocumented cross-tab coupling" claim was a mislabel and is withdrawn. **Relevance rule: Wobble Frequency is relevant when Edge Wobble > 0 OR Motion = Wave is armed** — an OR-of-AND row, not a static-wobble-only one.

**Motion = Wave is inert under Full Frame and On-Lens by design** (Codex C1, adjudicated INTENDED). Wave is *edge* undulation: it rides `wobY`/`wobA`, and Full Frame pins coverage to `m = 1.0` with no edge to undulate (`glsl:234-237`) while On-Lens has no shape SDF at all (`glsl:200-205`). No renderer change. The UI must gate the combination and say why — see the tooltip in §6.4.

**Motion (12 modes) × Motion-tab parameters.** From `pipeline.cpp:14034-14149`.

| Motion | Speed | Sweep Dir | Sweep Range | PingPong | Pulse Tgt/Amt | Wave Amp | Path Fx/Fy/Phase | Path Amp | Stutter ×6 |
|---|---|---|---|---|---|---|---|---|---|
| 0 Static | – | – | – | – | – | – | – | – | – |
| 1 Sweep | ✓ | ✓ | ✓ | ✓ | – | – | – | – | – |
| 2 Pulse | ✓ | – | – | – | ✓ | – | – | – | – |
| 3 Wave | ✓ | – | – | – | – | ✓ | – | – | – |
| 4 Path | ✓ | – | – | – | – | – | ✓ | ✓ | – |
| 5 Stutter | ✓ | – | – | – | – | – | – | – | ✓ |
| 6 Orbit | ✓ | ✓ (phase) | – | – | – | – | – | ✓ | – |
| 7 Wander | ✓ | – | – | – | – | – | – | ✓ | – |
| 8 Handheld | ✓ | – | – | – | – | – | – | ✓ | – |
| 9 Pendulum | ✓ | – | ✓ | – | – | – | – | – | – |
| 10 Heartbeat | ✓ | – | – | – | ✓ (Amt) | – | – | – | – |
| 11 Strobe Jump | ✓ | – | ✓ | – | – | – | – | ✓ | – |

**Compound guards inside the motion branches — three entries above are conditional:**

- **Pulse Target = Focus (rack)** is entirely inert under **Contents = Sharp Window**: the rack is applied to `lens_focus_m` at `:14259`, then unconditionally discarded by `if (content == 1) lens_focus_m = base_focus_m;` at `:14261-14266`. Pulse Amount still works under Pulse Target = Size.
- **Stutter Focus** is guarded twice — `if (content != 1 && m_stutter_focus > 0.f)` at `:14092` — so it needs Motion = Stutter **AND** Contents ≠ Sharp Window.
- **Handheld Focus Breath** likewise: `if (content != 1)` at `:14165`, so it needs Handheld > 0 **AND** Contents ≠ Sharp Window.

**Motion = Wave shares a spatial-density control and is dead under two placements — both by design.** The wave rides the *edge-wobble* terms (`glsl:256-259`), so (a) its spatial density comes from the **Edge Wobble Frequency** slider on the Shape tab, which is a legitimate shared parameter rather than a coupling bug, and (b) because Full Frame and On-Lens never evaluate `wobY`/`wobA`, **Motion = Wave does nothing at all under those two placements**. Both adjudicated INTENDED; see the detailed treatment under §2.1's OR-consumption notes.

Independent of the above: **Handheld** (arms Handheld Speed / Gait / Angle Sway / Focus Breath), and **Spin Mode** — 0 Constant → Spin Rate only; 1 Ease / 2 Loop → Travel, Duration, Delay; 3 Flick → Travel, Duration, Overshoot, Delay.

**Aperture Shape (5) × Bokeh:** 0 Round → none. 1 Polygon → Sides, Blade Curvature. 2 Star → Sides, Blade Curvature, Star Inner. 3 Heart → Blade Curvature. 4 Anamorphic → Anam Squeeze, Anam Angle. **Rotation applies to {1, 2, 3}** — Heart subtracts `rot` at `glsl:479-486`; Anamorphic does *not* use it, it has its own Anamorphic Angle uniform (`aperture2.z`, uploaded separately at `pipeline.cpp:14340-14344`). **Cat's-Eye** is unconditional (`glsl:517`).

**Faceted Fold (4) × Bokeh:** 0 Off → none. 1/2/3 → Segments, Feed Angle. **Source Zoom** is self-arming: `pattern_zoom != 1` is itself one of the five warp-arming terms, so moving it always has effect.

**Element Profile (5) × Glass:** 0 Off → none. 1–4 → IOR, Thickness, Rim Width, Rim Warp, Rim Caustic, Rim Darken. **Magnify Scale/Trim die when `profile > 0` OR `content == 1`** — `magnify = 1.f` at both `pipeline.cpp:14252` and `:14264`.

**Character × Glass:** Lateral CA, Axial CA, Field Curvature and Edge Vignette are all products of `strength_d * character * <their own scale>` (`pipeline.cpp:14253-14256`). They therefore go inert when **Character = 0** *or* when **`strength_d == 0`** — which happens whenever the lens and base planes coincide (Power = 0 in physical mode, or Lens Distance = Base Distance in manual mode). Both are Tier-C armed-state conditions, not mode conditions.

**Focus mode pairs:** Lens Focus = Physical power → Power live, Lens Distance dead; = Manual distance → the reverse. **Exactly one of Power / Lens Distance is always dead.** Base Focus is *not* a clean pair: = Manual → Base Distance live; = Camera focus point → Base Distance live **only if neither the live-DoF path nor the alt-zoom fallback resolves a target** (`pipeline.cpp:14204-14232`). The UI must query the resolver, not the combo (§5.3).

### 2.2 Kaleidoscope — 24 pattern modes

Derived by static analysis of the `kmode` branch chain in `ultimateKaleidoF.glsl:273-673`. `Seg` = Segments, `Ring` = Ring Count, `Tw` = Twist, `SS` = Star Sharpness, `SB` = Shape Bias, `Band/Amt/Flow/Freq` = the four Subject FX sliders, `Cells` = Size Scatter + Breathing (via `kal_cellScale`), `Sub` = Subdivide, `Mrg` = Merge, `Tint` = Iridescent Tint (needs a valid `cellKey`).

`Feed` = Source Angle **and** Source Spin (both fold into the one `feed` radian value on the CPU). `Wv` = Wave Amplitude + Wave Frequency (live only under Motion = Wave). **`Flow` marked ⓑ is Tier C**: in those modes FX Flow's only path is `kal_cellScale`'s `ftime`, which is a no-op unless **Cell Breathe > 0** (`glsl:110-118`).

| # | Pattern | Seg | Ring | Tw | Feed | Wv | SS | SB | Band | Amt | Flow | Freq | Cells | Sub | Mrg | Tint |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | Radial Mirror | ✓ | – | ✓ | ✓ | ✓ | – | – | – | – | – | – | – | – | – | – |
| 1 | Radial Rotate | ✓ | – | ✓ | ✓ | ✓ | – | – | – | – | – | – | – | – | – | – |
| 2 | Mirror Tile | ✓ | – | – | **–** | – | – | – | – | – | – | – | – | – | – | ✓ |
| 3 | Honeycomb | ✓ | – | – | ✓ | – | – | – | – | – | ⓑ | – | ✓ | – | – | ✓ |
| 4 | Square Grid | ✓ | – | – | ✓ | – | – | – | – | – | ⓑ | – | ✓ | ✓ | – | ✓ |
| 5 | Triangle | ✓ | – | – | ✓ | – | – | – | – | – | ⓑ | – | ✓ | – | – | ✓ |
| 6 | Concentric Rings | ✓ | ✓ | ✓ | ✓ | ✓ | – | – | – | – | – | – | – | – | – | – |
| 7 | Star | ✓ | – | ✓ | ✓ | ✓ | **✓** | – | – | – | – | – | – | – | – | – |
| 8 | Pinwheel Lattice | ✓ | – | – | **–** | – | – | – | – | – | ⓑ | – | ✓ | ✓ | – | ✓ |
| 9 | Echo Rings | – | ✓ | ✓ | ✓ | ✓ | – | – | ✓ | – | ✓ | – | – | – | – | – |
| 10 | Orbit Clones | ✓ | – | – | ✓ | ✓ | – | – | ✓ | – | ⓑ | – | ✓ | – | – | ✓ |
| 11 | Vortex | – | – | ✓ | ✓ | ✓ | – | – | – | ✓ | ✓ | – | – | – | – | – |
| 12 | Ripple | – | – | – | ✓ | ✓ | – | – | ✓ | ✓ | ✓ | ✓ | – | – | – | – |
| 13 | Petal Mandala | ✓ | – | ✓ | ✓ | ✓ | – | – | ✓ | – | ✓ | – | – | – | – | – |
| 14 | Infinity Tunnel | – | – | ✓ | ✓ | ✓ | – | – | ✓ | ✓ | ✓ | – | – | – | – | – |
| 15 | Shatter | ✓ | – | – | ✓ | ✓ | – | – | ✓ | ✓ | ✓ | – | – | – | – | ✓ |
| 16 | Spiral Arms | ✓ | – | – | ✓ | ✓ | – | **✓** | – | – | ✓ | – | – | – | – | – |
| 17 | Droste Spiral | – | – | ✓ | ✓ | ✓ | – | **✓** | ✓ | ✓ | ✓ | – | – | – | – | – |
| 18 | Polygon Prism | ✓ | – | – | ✓ | **✓** | – | – | ✓ | – | ✓ | – | – | – | – | – |
| 19 | Strip Mirror | ✓ | – | – | ✓ | – | – | **✓** | – | – | ✓ | ✓ | – | – | – | – |
| 20 | Fly's Eye | ✓ | – | – | ✓ | – | – | – | – | ✓ | ✓ | – | ✓ | – | **✓** | ✓ |
| 21 | Inversion | – | – | ✓ | ✓ | ✓ | – | – | ✓ | ✓ | ✓ | – | – | – | – | – |
| 22 | Gem Facets | ✓ | ✓ | – | ✓ | ✓ | – | – | – | ✓ | ✓ | – | ✓ | – | – | ✓ |
| 23 | Sawtooth Sun | ✓ | – | ✓ | ✓ | ✓ | – | **✓** | ✓ | – | ✓ | – | – | – | – | ✓ |

Live in **every** mode: Source Zoom, Source Offset X/Y, Center X/Y, Angle, Edge Wrap, Blend, Setup View. **Source Angle and Source Spin are *not* universal** — modes **2 (Mirror Tile)** and **8 (Pinwheel Lattice)** never read `feed`, so both controls are inert there. **Seam Soften: mode 0 only.**

Bold cells mark the "one mode only" controls: **Star Sharpness → mode 7 alone. Cell Merge → mode 20 alone.** Ring Count is used by 3 of 24 modes; Shape Bias by 4; Subdivide by 2; FX Frequency by 2. Wave is dead in 6 of 24 modes even with Motion = Wave selected.

#### Authoritative Kaleido tab layout — one inventory, used everywhere

Rev 4 stated the Fly's Eye control set three different ways in three sections. This is the single definition; §3.6, §3.8c and §6.1 all defer to it.

The Kaleido tab has **one swapping band and two standing groups**:

| Region | Members | Behaviour |
|---|---|---|
| **Header** | Preset · Pattern · Center X/Y · Angle · Blend · Setup View | always visible |
| **Pattern band** (swaps) | the mode's subset of: Segments · Ring Count · Twist · Star Sharpness · Shape Bias · FX Band · FX Amount · FX Flow · FX Frequency | overlay panel per mode |
| **Source group** (standing) | Source Zoom · Source Offset X/Y (always live) · Source Angle · Source Spin (dark in modes 2 and 8) | always present; members gate individually |
| **Cell group** (standing) | Cell Size Scatter · Cell Breathing · Cell Subdivide · Cell Merge · Cell Tint | always present; members gate individually |

Wave Amplitude and Wave Frequency live on **Kal Anim**, under Motion, not here.

**Worked example — Fly's Eye (mode 20).** Band: **Segments · FX Amount · FX Flow** (3 rows). Source group: Zoom, Offset X/Y, Angle, Spin all live (**4 rows** — Offset X/Y is one of §3.7's natural side-by-side 2-tuples, exactly as Center X/Y is counted as one row in the header). Cell group: Size Scatter, Breathing, Merge, Tint live; **Subdivide dark** (mode 20 is not a quadtree mode). **Breathing and Merge are separate rows — they are not a paired control**, and no paired rows are proposed anywhere in this design; every slider gets its own row except the natural 2-tuples listed in §3.7, which do not include these.

**Tallest band is 5 rows, not 7** — Droste Spiral (Twist · Shape Bias · FX Band · FX Amount · FX Flow) and Sawtooth Sun (Segments · Twist · Shape Bias · FX Band · FX Flow). Fly's Eye is a 3-row band; rev 4's "7 rows" conflated the band with the standing groups. The Kaleido tab's height budget is therefore header (6) + band (5) + Source (**4**, with the Offset pair on one row) + Cell (5) + 3 headings, not a 7-row band.

**Kaleido Motion (12 modes)** — same structure as the diopter's (`alKaleidoResolveMotion`, `pipeline.cpp:14704`), with two differences: mode **5 = Track** (uses `alKaleidoFocusUV`, consumes no sliders), and **Pulse Target has 3 options** (Zoom / Twist / Offset) rather than 2, with Offset additionally consuming Sweep Direction. Wave here also consumes **Wave Frequency**, which the diopter lacks.

**Protect Mode (4):** 0 Off → nothing. 1 Disc → Radius, Feather, Anchor (+ Center X/Y when Anchor=Fixed). 2 Depth → Depth Cut, Depth Feather, Invert. 3 Disc×Depth → all eight.

### 2.3 Proposed C++ table format

One static table, one relevance pass. Deliberately *data*, so adding a shader mode is a table edit rather than a control-flow edit.

**The schema must be OR-of-AND, not a flat ANDed predicate list.** Rev 1 proposed one row per control with predicates ANDed together; that cannot express the three OR-consumed controls in §2.1 (Size is live when *the mask uses it* **OR** *refraction is on* **OR** *the warp is armed*), and it cannot express the compound motion guards (Stutter Focus needs Motion = Stutter **AND** Contents ≠ Sharp Window, which is one clause, but Pulse Amount is live under `PulseTarget == Size` **OR** `PulseTarget == Focus AND content != 1`). A control is relevant when **any** clause passes; a clause passes when **all** its terms pass.

```cpp
// alfloaterultimatediopter.h

enum ALDiopterPred                 // named predicates for threshold-armed state
{
    PRED_NONE = 0,
    PRED_WARP_ARMED,               // pipeline.cpp:14350, shared helper
    PRED_HANDHELD_ARMED,           // CineDiopterHandheld > 0
    PRED_GHOSTS_ARMED,             // CineDiopterGhostCount > 0
    PRED_SEAM_ARMED,               // CineDiopterSeamGhostPx > 0
    PRED_REFRACTION_ARMED,         // CineDiopterGlassProfile > 0
    PRED_ABERRATION_ARMED,         // character > 0 && strength_d > 0
    PRED_FIELD_CURVE_ARMED,        // field_curve > 0
    PRED_CELL_BREATHE_ARMED,       // CineDiopterKalCellBreathe > 0   [Codex 1.8]
    PRED_BASE_FOCUS_MANUAL,        // resolver reports MANUAL provenance (§5.3)
    PRED_EDGE_WOBBLE_ARMED,        // CineDiopterWobbleAmt > 0        [Codex C2]
    PRED_SHAPE_HAS_EDGE,           // shape != FullFrame && placement == framed
                                   //   -- the mask evaluates wobY/wobA  [Codex C1]
    PRED_LENS_POWERED,             // strength_d > 0 -- the exact renderer
                                   //   expression from pipeline.cpp:14246-14249
                                   //   (|1/lens - 1/base| after clamps); distinct
                                   //   from ABERRATION_ARMED, which also needs
                                   //   character > 0. Multiplicative consumers
                                   //   (Magnify Scale, Character) die here even
                                   //   when their own slider is nonzero.
    AL_PRED_COUNT                  // terminator -- bounds the coverage loops;
                                   //   never a term value
};

struct ALDiopterTerm               // one ANDed term
{
    const char*     mSetting;      // driving setting, or nullptr if mPred is used
    U32             mMask;         // bit i set == term passes when value == i
    ALDiopterPred   mPred;         // alternative to (mSetting, mMask)
    bool            mNegate;       // term passes when the above does NOT
};

// Counts are EXPLICIT -- no sentinel termination. Rev 5 used "{} terminates",
// which made {} simultaneously the list terminator and the representation of a
// valid "no condition beyond Enabled + tool" clause; the coverage loops stopped
// at the first empty clause, so ALWAYS clauses were never accumulated or
// asserted [Codex delta-5 #3]. With counts, an ALWAYS clause is {0 terms},
// participates in evaluation (vacuously passes) and in coverage (it fails
// exactly when the implicit Enabled term fails), and nothing is skipped.
struct ALDiopterClause
{
    U8              mTermCount;    // 0 == ALWAYS (passes given Enabled + tool)
    ALDiopterTerm   mTerms[4];     // ANDed; entries beyond mTermCount must be {}
};

struct ALDiopterRelevance
{
    const char*      mCtrlName;      // XUI name, e.g. "diopter_star_points"
    const char*      mResetName;     // sibling reset button, or nullptr -- see below
    U32              mTool;          // AL_TOOL_DIOPTER | AL_TOOL_KALEIDO | AL_TOOL_BOTH
    U32              mSection;       // section id for the panel/heading pass
    bool             mPresetOwned;   // in ALDiopterLook / ALKaleidoLook
    bool             mUnconditional; // exempt from the implicit Enabled term AND from
                                     //   the clause-coverage rule. Exactly one row
                                     //   sets this: diopter_enabled. Such a row has
                                     //   mClauseCount == 0.
    U32              mTier;          // TIER_MODE (hide) | TIER_OVERRIDDEN | TIER_UNARMED
    U8               mClauseCount;   // 0 only when mUnconditional
    ALDiopterClause  mClauses[3];    // ORed; entries beyond mClauseCount must be {}
};
```

Layer-1 validation gains the corresponding structural asserts: every count within bounds, every entry beyond a count zeroed, `mClauseCount == 0` iff `mUnconditional`.

**The implicit Enabled term is what makes `mUnconditional` a population of one.** Every row with `mUnconditional == false` carries an implicit `CineDiopterEnabled == on` term ANDed into all of its clauses; the evaluator applies it once rather than the table repeating it 180 times. This is also how the master-enable tier (§1.4, Tier 0) is implemented — no separate mechanism. The consequence that matters for testing: a row with a single ALWAYS clause (`mClauseCount == 1`, `mTermCount == 0` — "no condition beyond Enabled and the tool") is a real clause the coverage loops visit: it passes whenever Enabled is on and fails whenever it is off, so it is observed in both states and satisfies clause coverage like any other. Only `diopter_enabled` — which sits outside the gate it controls — is genuinely never irrelevant; it is the only row allowed `mUnconditional`, and the only row with `mClauseCount == 0`.

**`mResetName` is nullable, and exactly one row may use that.** The floater ships 180 reset buttons against 181 controls: `diopter_enabled` has none (`floater_ultimate_diopter.xml:48-54` — the checkbox is followed directly by the Tool Mode combo, with no `rst_` sibling). Rev 2's validator rule "every `mResetName` must resolve to a real child" could therefore never pass. The rule becomes:

> `mResetName` may be `nullptr` **only** for `diopter_enabled`; for every other row it must be non-null and must resolve. The validator asserts the null count is exactly 1 and that the null row is the master enable — so if someone later deletes a reset button, the build fails rather than silently widening the exception.

```cpp

#define BIT(n) (1u << (n))
#define S(name, mask) {name, mask, PRED_NONE, false}
#define P(pred)       {nullptr, 0u, pred, false}

static const ALDiopterRelevance sRelevance[] =
{
 // ---- simple one-clause rows -----------------------------------------
 // mPresetOwned FALSE: StarPoints is NOT in the 44-entry owned array
 // (llviewerfloaterreg.cpp:571-588) -- caught by the table regeneration.
 {"diopter_star_points", "rst_CineDiopterStarPoints", AL_TOOL_DIOPTER,
   SEC_SHAPE_SPECIFIC, false, false, TIER_MODE, 1,
   {{2, { S("CineDiopterShape", BIT(7)),
          S("CineDiopterPlacementMode", BIT(0)) }}}},           // not On-Lens

 {"diopter_round", "rst_CineDiopterCornerRound", AL_TOOL_DIOPTER,
   SEC_SHAPE_EDGE, false, false, TIER_MODE, 1,
   {{2, { S("CineDiopterShape", BIT(6)|BIT(7)|BIT(9)|BIT(10)),
          S("CineDiopterPlacementMode", BIT(0)) }}}},

 // ---- master enable: the one row with no reset button  [Codex A10] ----
 // UNCONDITIONAL and the only row with mClauseCount == 0.
 {"diopter_enabled", nullptr, AL_TOOL_BOTH,
   SEC_HEADER, false, /*uncond*/ true, TIER_MODE, 0, {}},

 // ---- OR-consumed: Size is three paths, ALL killed by On-Lens  [1.4/D1]
 // PlacementMode==0 (framed) is a term in EVERY clause: On-Lens discards
 // eff_size and recomputes glass_radius (pipeline.cpp:14289-14296), so the
 // slider reaches neither the mask, nor refraction, nor the warp annulus.
 {"diopter_size", "rst_CineDiopterSize", AL_TOOL_DIOPTER,
   SEC_FRAMING, false, false, TIER_MODE, 3,
   {{2, { S("CineDiopterShape", 0xFFCu /* 2..11 */),            // mask boundary
          S("CineDiopterPlacementMode", BIT(0)) }},
    {2, { P(PRED_REFRACTION_ARMED),
          S("CineDiopterPlacementMode", BIT(0)) }},             // refraction radius
    {2, { P(PRED_WARP_ARMED),
          S("CineDiopterPlacementMode", BIT(0)) }}}},           // warp annulus outer

 // ---- shared spatial density: static wobble OR Wave motion  [Codex C2] -
 {"diopter_wobble_freq", "rst_CineDiopterWobbleFreq", AL_TOOL_DIOPTER,
   SEC_SHAPE_EDGE, false, false, TIER_UNARMED, 2,
   {{2, { P(PRED_EDGE_WOBBLE_ARMED),
          P(PRED_SHAPE_HAS_EDGE) }},                            // not FullFrame/On-Lens
    {2, { S("CineDiopterMotionMode", BIT(3)),                   // Motion = Wave
          P(PRED_SHAPE_HAS_EDGE) }}}},

 // ---- Wave Amplitude: needs an edge to undulate  [Codex C1, INTENDED] --
 {"diopter_wave_amp", "rst_CineDiopterWaveAmp", AL_TOOL_DIOPTER,
   SEC_MOTION_PARAMS, false, false, TIER_MODE, 1,
   {{2, { S("CineDiopterMotionMode", BIT(3)),
          P(PRED_SHAPE_HAS_EDGE) }}}},

 // ---- OR-consumed: Hollow is mask onion OR warp inner radius ---------
 {"diopter_hollow", "rst_CineDiopterHollow", AL_TOOL_DIOPTER,
   SEC_SHAPE_EDGE, true, false, TIER_MODE, 2,
   {{2, { S("CineDiopterShape", 0xFF8u /* 3..11 */),
          S("CineDiopterPlacementMode", BIT(0)) }},
    {1, { P(PRED_WARP_ARMED) }}}},

 // ---- compound guard: Sharp Window kills the focus rack  [Codex 1.22] -
 {"diopter_stutter_focus", "rst_CineDiopterStutterFocus", AL_TOOL_DIOPTER,
   SEC_MOTION_PARAMS, false, false, TIER_MODE, 1,
   {{2, { S("CineDiopterMotionMode", BIT(5)),
          {"CineDiopterContent", BIT(1), PRED_NONE, /*negate*/ true} }}}},

 // ---- Tier C: armed-state, not mode ----------------------------------
 {"diopter_ghost_spacing", "rst_CineDiopterGhostSpacing", AL_TOOL_DIOPTER,
   SEC_GHOSTS, true, false, TIER_UNARMED, 1,
   {{1, { P(PRED_GHOSTS_ARMED) }}}},

 // ---- multiplicative gate: Magnify Scale x strength_d  [Codex delta-5] -
 // Magnify Trim is ADDITIVE (pipeline.cpp:14249-14252) and correctly has no
 // PRED_LENS_POWERED term; Scale multiplies and dies with coincident planes.
 // mPresetOwned FALSE: MagnifyScale is NOT in the owned array either.
 {"diopter_mag_scale", "rst_CineDiopterMagnifyScale", AL_TOOL_DIOPTER,
   SEC_GLASS_OPTICS, false, false, TIER_UNARMED, 1,
   {{3, { S("CineDiopterGlassProfile", BIT(0)),                 // Profile Off
          S("CineDiopterContent", ~BIT(1)),                     // not Sharp Window
          P(PRED_LENS_POWERED) }}}},

 // ---- kaleido: FX Flow is Tier C in the cell modes  [Codex 1.8] ------
 {"kal_fx_flow", "rst_CineDiopterKalFXFlow", AL_TOOL_KALEIDO,
   SEC_KAL_FX, true, false, TIER_MODE, 2,
   {{1, { S("CineDiopterKalMode",                               // direct ft users
           0xFFFA00u /* bit 9 + bits 11..23 */) }},             // [Codex D2]
    {2, { S("CineDiopterKalMode", BIT(3)|BIT(4)|BIT(5)|BIT(8)|BIT(10)),
          P(PRED_CELL_BREATHE_ARMED) }}}},                       // cellScale path only

 // ---- kaleido: feed is NOT universal  [Codex 1.7 + re-audit] ---------
 {"kal_source_angle", "rst_CineDiopterKalSourceAngle", AL_TOOL_KALEIDO,
   SEC_KAL_SOURCE, true, false, TIER_MODE, 1,
   {{1, { {"CineDiopterKalMode", BIT(2)|BIT(8), PRED_NONE, /*negate*/ true} }}}},
};
```

> **The rows above are illustrative. The complete table is the companion artifact `diopter_relevance_table.md`** — all 181 rows carrying EVERY schema field as a column per the parity rule below (control, reset, tool, **section, preset-owned, tier**, `UNCONDITIONAL`, explicit clause count, clauses in OR-of-AND form) plus the DERIVED Setting column and the 12-predicate glossary quoting exact renderer expressions with citations. **That file, not this section, is the implementation source of truth**, and it is what the clause-coverage rule below is verified against. Its asserted invariants: 181 rows, exactly 1 `UNCONDITIONAL`, exactly 1 null reset (both `diopter_enabled`), no duplicate controls or settings, and per-tab counts Main 14 · Shape 22 · Focus 15 · Glass 27 · Bokeh 16 · Motion 29 · Kaleido 28 · Kal Anim 30.
>
> Note it carries **12** predicates, not the 11 audited in rev 4: building the full table surfaced that `diopter_character` is itself gated — Character does nothing when the lens and base planes coincide — which needs `PRED_LENS_POWERED` (`strength_d > 0`) distinct from `PRED_ABERRATION_ARMED` (`character > 0 ∧ strength_d > 0`). Sweep S8 already varies plane separation, so it flips the new predicate without a new axis.

**Predicate exactness is a rule, not a preference: every glossary entry quotes the renderer's exact expression with its file:line.** `PRED_WARP_ARMED` reuses the *exact* thresholds from `pipeline.cpp:14347-14352` (the `1e-3` / `1e-2` epsilons and the zoom-delta test — NOT a paraphrased `> 0` / `!= 1`, which Codex delta 5 caught the table using); `PRED_ABERRATION_ARMED` / `PRED_FIELD_CURVE_ARMED` the exact ones from `:14253-14256`; `PRED_LENS_POWERED` the exact `strength_d` computation from `:14246-14249`. Factor them into shared helpers in pipeline.cpp so the UI can never disagree with the renderer about what is live — the glossary then cites the helper, and the helper is the single source for both consumers.

**Table↔schema parity rule: the companion table carries a column iff `ALDiopterRelevance` has the field** — `mCtrlName`, `mResetName`, `mTool`, `mSection`, `mPresetOwned`, `mUnconditional`, `mTier`, `mClauseCount`, and the clauses. The table's *Setting* column is the one deliberate exception, header-marked **DERIVED**: it is the control→setting binding read from the XUI, present for review convenience, not a schema field. Any future schema field addition must add a table column in the same change, and the layer-1 validator parses the table header to enforce the parity.

**Reset buttons live in the row, not beside it.** `mResetName` is not decoration. `Diopter.ResetControl` writes the setting immediately (`llviewerfloaterreg.cpp:538-556`), which means a reset click on a preset-owned control **triggers materialization and the Custom transition** exactly like a slider drag. A reset button whose slider is hidden must be hidden with it; one whose slider is dimmed must be dimmed with it; and its tooltip must carry the same explanation. Leaving 180 always-live reset buttons in a gated floater would be a hole straight through the gating model.

#### Validation — against branch tests, not just the widget tree

The table encodes shader control flow, so type-checking it is not enough. Rev 2 described this as prose; here are the concrete artifacts.

**Put the table in an LLUI-free model unit** so it can be tested headlessly, following the fork's own precedent (`alcinelightrigmodel`, `aldirectorswitchermodel`, `alsceneexplorerpredicate`, all of which have `tests/*_test.cpp` companions):

| Artifact | Path | Role |
|---|---|---|
| Model unit | `indra/newview/aldiopterrelevance.h` / `.cpp` | `sRelevance`, the predicate enum, and `alDiopterEvaluate(const ALDiopterState&)`. **No LLUI, no `gSavedSettings`** — state arrives as a plain struct, so the evaluator is pure and sweepable. |
| Floater glue | `indra/newview/alfloaterultimatediopter.cpp` | reads `gSavedSettings` into an `ALDiopterState`, calls the evaluator, applies `setVisible`/`setEnabled`/`setToolTip`. |
| Oracle extractor | `indra/newview/tests/aldiopterrelevance_oracle.py` | parses `ultimateDiopterGatherF.glsl`, `ultimateDiopterF.glsl`, `ultimateKaleidoF.glsl` and the resolver blocks in `pipeline.cpp`, emitting per-branch uniform-consumption sets. This is the same analysis that produced §2.1–§2.2; **check it in rather than discarding it.** |
| Generated oracle | `indra/newview/tests/aldiopterrelevance_oracle.llsd` | the extractor's committed output; regenerating it is a reviewable diff. |
| Test target | `indra/newview/tests/aldiopterrelevance_test.cpp`, wired with `LL_ADD_INTEGRATION_TEST(aldiopterrelevance ...)` in `indra/newview/CMakeLists.txt` (idiom at `:2675-2726`) | runs all three layers below. |

**Layer 1 — structural.** Every `mCtrlName` resolves against the XUI (parsed, not instantiated); `mResetName` resolves for all rows except exactly one, which must be `diopter_enabled`; every one of the 181 `CineDiopter*` settings appears exactly once; `mPresetOwned` matches `ALDiopterLook` (44) and `ALKaleidoLook` (49); every `TIER_MODE` row has ≥ 1 clause.

**Layer 2 — cross-product branch coverage.** *This is the layer that matters, and it must be a cross-product, not one enum at a time.* The D2 mask typo — a single missing bit for Vortex — is proof: it sits inside one enum's mask and a per-enum sweep would have passed it, because the row was still "relevant in most modes". Compound and OR-consumed rows only fail on combinations. The required sweeps:

| # | Sweep | Cardinality | Catches | Predicates flipped |
|---|---|---|---|---|
| S1 | shape × placement × profile × warp-armed | 12 × 2 × 5 × 2 = **240** | the Size / Hollow OR-consumption, and D1's On-Lens discard | `WARP_ARMED`, `REFRACTION_ARMED`, `SHAPE_HAS_EDGE` |
| S2 | motion × pulse-target × content × **handheld-armed** | 12 × 2 × 2 × 2 = **96** | the Sharp Window guards on Stutter Focus, Focus Breath, Pulse:Focus | `HANDHELD_ARMED` |
| S3 | shape × placement × motion × **edge-wobble-armed** | 12 × 2 × 12 × 2 = **576** | C1 (Wave inert under Full Frame / On-Lens) and C2 (WobbleFreq's OR rule) | `EDGE_WOBBLE_ARMED`, `SHAPE_HAS_EDGE` |
| S4 | kal-mode × cell-breathe × motion | 24 × 2 × 12 = **576** | D2, and the Tier-C FX Flow path | `CELL_BREATHE_ARMED` |
| S5 | aperture-shape × pattern-mode | 5 × 4 = **20** | Rotation membership {1,2,3} | `WARP_ARMED` (via pattern) |
| S6 | focus-mode × lens-mode × provenance | 2 × 2 × 3 = **12** | the three-way Base Distance ambiguity | `BASE_FOCUS_MANUAL` |
| **S7** | **ghosts-armed × seam-armed × content × profile** | 2 × 2 × 2 × 5 = **40** | the 7 ghost dependents, Seam Double Amount, and Magnify's profile-OR-content rule | `GHOSTS_ARMED`, `SEAM_ARMED` |
| **S8** | **character × plane-separation × field-scale × content** | 2 × 2 × 2 × 2 = **16** | the four aberration controls, Surround Max Blur's OR rule, and Magnify Scale's multiplicative gate | `ABERRATION_ARMED`, `FIELD_CURVE_ARMED`, `LENS_POWERED` |

| **S0** | **enabled × tool-mode** | 2 × 2 = **4** | the master-enable Tier-0 gate, and — critically — the FALSE state of every ALWAYS clause: `alDiopterEvalClause` applies the implicit Enabled term (see below), and S0 is the only sweep that turns Enabled off, so without it an ALWAYS clause could never be observed failing and the clause-coverage rule would be unsatisfiable for those rows. | (the implicit term itself) |

**1580 cases total** (S0's 4 + the 1576 above) — still trivial for a pure evaluator. For each, assert the evaluator's relevant-set equals the oracle's consumed-set. A shader edit that adds a uniform read then fails the build rather than silently creating a lying control.

**`alDiopterEvalClause(row, clause_index, st)` includes the implicit terms**: it evaluates `st.enabled AND (row.mTool matches st.tool) AND all explicit terms of row.mClauses[clause_index]` — the row operand is in the signature precisely because tool-match reads `row.mTool`, which lives on the row, not the clause. That is what makes an ALWAYS clause (`mTermCount == 0`) a real, coverable clause — it passes whenever Enabled+tool pass and fails in S0's disabled cases — rather than a constant the coverage rule would reject as never-failing. S1–S8 run with Enabled on and the row's own tool selected.

S7 and S8 are rev-4 additions found by the audit below; S2 and S3 gained a dimension each for the same reason. Note S8's axes are *derived* quantities — `strength_d` is `|1/lens − 1/base|`, so "plane-separation" is driven by setting Lens Distance equal to or different from Base Distance, and the sweep must set the underlying settings, not the derived value.

#### Meta-rule: coverage of the predicate and clause space is itself asserted

Rev 3's sweeps were hand-picked to catch known defects, which is exactly how `PRED_EDGE_WOBBLE_ARMED` and five others ended up never exercised. Hand-picking does not scale past the first review. So the test enforces its own adequacy:

> **Predicate-flip rule.** Every value in `ALDiopterPred` **except the `PRED_NONE` sentinel** must be observed **in both states** somewhere in the sweep corpus. `PRED_NONE` means "this term uses `(mSetting, mMask)` rather than a predicate" and has no truth value, so it is excluded by construction. A predicate that no sweep flips is a **test failure** — the message names the predicate and tells the author to add an axis.
>
> **Clause-coverage rule.** Every clause in every row must be observed both passing and failing at least once, and every row must be observed both relevant and irrelevant — except rows with `mUnconditional == true` (exactly one: `diopter_enabled`). A clause that never fails is dead weight; a clause that never passes is unreachable. Both are bugs in the table.

Here is the code, not the intent:

```cpp
// tests/aldiopterrelevance_test.cpp

struct ALCoverage
{
    // [pred][state] -- state 0 = observed false, 1 = observed true.
    // PRED_NONE occupies slot 0 and is never written or checked.
    bool mPred[AL_PRED_COUNT][2] = {};
    // [row][clause][state]; AL_MAX_CLAUSES == 3 per the schema.
    bool mClause[AL_RELEVANCE_COUNT][AL_MAX_CLAUSES][2] = {};
    // [row][state] -- the row's overall relevance verdict.
    bool mRow[AL_RELEVANCE_COUNT][2] = {};
};

// Called once per sweep case, after evaluating the table against `st`.
void alAccumulateCoverage(const ALDiopterState& st, ALCoverage& cov)
{
    for (S32 p = PRED_NONE + 1; p < AL_PRED_COUNT; ++p)
    {
        cov.mPred[p][alDiopterEvalPred((ALDiopterPred)p, st) ? 1 : 0] = true;
    }

    for (S32 r = 0; r < AL_RELEVANCE_COUNT; ++r)
    {
        const ALDiopterRelevance& row = sRelevance[r];
        // mClauseCount, not a sentinel scan: ALWAYS clauses (mTermCount == 0)
        // are real clauses and are accumulated here like any other.
        for (S32 c = 0; c < row.mClauseCount; ++c)
        {
            const bool pass = alDiopterEvalClause(row, c, st);
            cov.mClause[r][c][pass ? 1 : 0] = true;
        }
        cov.mRow[r][alDiopterEvaluate(row, st) ? 1 : 0] = true;
    }
}

void alAssertCoverage(const ALCoverage& cov)
{
    for (S32 p = PRED_NONE + 1; p < AL_PRED_COUNT; ++p)
    {
        for (S32 s = 0; s < 2; ++s)
        {
            ensure(llformat(
                "predicate %s never observed %s across the sweep corpus -- "
                "add a sweep axis that varies it (design doc SS2.3)",
                alDiopterPredName((ALDiopterPred)p), s ? "true" : "false"),
                cov.mPred[p][s]);
        }
    }

    for (S32 r = 0; r < AL_RELEVANCE_COUNT; ++r)
    {
        const ALDiopterRelevance& row = sRelevance[r];
        if (row.mUnconditional)
        {
            // diopter_enabled: never irrelevant by design. Assert that the
            // exemption is not being used to hide an untested row.
            ensure("only diopter_enabled may be UNCONDITIONAL",
                   std::string(row.mCtrlName) == "diopter_enabled");
            continue;
        }

        for (S32 s = 0; s < 2; ++s)
        {
            ensure(llformat("row %s never observed %s -- unreachable or "
                            "always-on rule", row.mCtrlName,
                            s ? "relevant" : "irrelevant"),
                   cov.mRow[r][s]);
        }

        for (S32 c = 0; c < row.mClauseCount; ++c)
        {
            ensure(llformat("row %s clause %d never PASSES -- unreachable clause",
                            row.mCtrlName, c), cov.mClause[r][c][1]);
            ensure(llformat("row %s clause %d never FAILS -- dead clause, the "
                            "terms are implied by something else",
                            row.mCtrlName, c), cov.mClause[r][c][0]);
        }
    }
}
```

and the sweep driver that feeds it:

```cpp
void alRunSweeps(ALCoverage& cov)
{
    S32 cases = 0;
    for (const ALSweep& sweep : sSweeps)          // S0..S8, table above
    {
        for (ALDiopterState st : sweep)           // iterates the cross product
        {
            // 1. the correctness assertion: table vs oracle
            ensure_equals(llformat("%s case %d", sweep.mName, cases),
                          alDiopterRelevantSet(st),
                          alOracleConsumedSet(st));   // from the .llsd oracle
            // 2. the adequacy accumulation
            alAccumulateCoverage(st, cov);
            ++cases;
        }
    }
    ensure_equals("sweep corpus size changed unexpectedly", cases, 1580);
}

// The TUT entry point -- the accumulate/assert pair is actually WIRED, not
// implied [Codex delta-5 #1]: sweeps run, then adequacy is asserted.
template<> template<>
void aldiopterrelevance_object::test<1>()
{
    ALCoverage cov;
    alRunSweeps(cov);        // correctness (table vs oracle) + accumulation
    alAssertCoverage(cov);   // adequacy (predicate-flip + clause coverage)
}
```

The final `ensure_equals` on the case count is deliberate: it makes a silent narrowing of a sweep axis (the failure mode that produced rev 4's six coverage holes) a visible, reviewable diff rather than a quiet loss of protection.

This is what makes the corpus self-defending: adding `PRED_FOO` without adding an axis that flips it breaks the build immediately, rather than at the next adversarial review.

#### Predicate-flip audit (rev 4)

Applying the rule retroactively to rev 3's six sweeps. Codex identified two gaps; the full audit found **six**:

| Predicate | Exercised by rev-3 sweeps? | Resolution |
|---|---|---|
| `PRED_WARP_ARMED` | yes (S1 axis, S5 via pattern-mode) | — |
| `PRED_REFRACTION_ARMED` | yes (S1 profile axis) | — |
| `PRED_SHAPE_HAS_EDGE` | yes (S1/S3 shape × placement) | — |
| `PRED_CELL_BREATHE_ARMED` | yes (S4 axis) | — |
| `PRED_BASE_FOCUS_MANUAL` | yes (S6 provenance axis) | — |
| `PRED_HANDHELD_ARMED` | **no** — S2 varied motion but never Handheld, so the Focus Breath coverage claim was false | **Codex B1**; S2 gains a handheld axis |
| `PRED_EDGE_WOBBLE_ARMED` | **no** — S3 varied shape/placement/motion only | **Codex B1**; S3 gains a wobble axis |
| `PRED_GHOSTS_ARMED` | **no** — no sweep varied `CineDiopterGhostCount`; **seven** ghost dependents were entirely uncovered | **audit**; new S7 |
| `PRED_SEAM_ARMED` | **no** — no sweep varied `CineDiopterSeamGhostPx` | **audit**; new S7 |
| `PRED_ABERRATION_ARMED` | **no** — no sweep varied Character or plane separation | **audit**; new S8 |
| `PRED_FIELD_CURVE_ARMED` | **no** — same; also left Surround Max Blur's OR rule untested | **audit**; new S8 |

Six of eleven predicates — and the entire Glass-tab aberration group plus all seven ghost dependents — were outside the corpus. That is the strongest possible argument for the meta-rule: the gaps were not in obscure corners, they were in the two tabs with the highest dead-control counts (§1.6), and a human review found only two of the six.

**Layer 3 — runtime differential (debug builds).** Assert that `PRED_WARP_ARMED`, `PRED_ABERRATION_ARMED` and `alDiopterResolveBaseFocus()` return what the renderer computed that frame. Catches drift if anyone edits a pipeline expression without touching the shared helper.

### 2.4 Independent re-audit — 11 further corrections

Codex sampled roughly 20 matrix rows and found six defects. Re-auditing the remainder against the same three failure classes (OR-consumption, compound gating, wrong mode membership) surfaced eleven more, all now folded into §2.1–§2.2:

| # | Row | Defect | Class | Evidence |
|---|---|---|---|---|
| 1 | Kaleido mode 8 (Pinwheel Lattice) | never reads `feed` → Source Angle **and** Source Spin inert; rev 1 called them universal | membership | `glsl:317-336` |
| 2 | Kaleido mode 2 (Mirror Tile) | also never reads `twist` (Codex caught only `feed`) | membership | `glsl:273-281` |
| 3 | Shape row 0 (Full Frame) | Size marked unconditionally dead — same OR-consumption as Split | OR | `glsl:334-343`, `398-400` |
| 4 | Hollow, shapes 0/1/2 | marked dead; the halo warp reads it as annulus inner radius for **any** shape | OR | `glsl:399` |
| 5 | Stutter Focus | missing the `content != 1` guard inside the branch | compound | `pipeline.cpp:14092` |
| 6 | Pulse Target = Focus | entirely inert under Sharp Window — the rack is discarded downstream | compound | `pipeline.cpp:14259` vs `14261-14266` |
| 7 | Motion = Wave | inert under Full Frame and On-Lens (rides `wobY`/`wobA`, never evaluated there) — **adjudicated INTENDED**, wave is edge undulation and neither placement has an edge | compound | `glsl:256-259`, `234-237`, `200-205` |
| 8 | Wave Amplitude | shares its *spatial density* with Edge Wobble Frequency — **adjudicated INTENDED, not a coupling bug**; the animation phase is a separate uniform (`shape4.z`). Relevance rule: WobbleFreq is live when static Edge Wobble **OR** Motion = Wave is armed | OR | `glsl:256-259` |
| 9 | Surround Max Blur | marked always-live; dead when `field_curve == 0` **AND** `content != 1` | OR | `glsl:592-594` |
| 10 | Lateral/Axial CA, Field Curvature, Edge Vignette | ungated; all die at Character = 0 or `strength_d == 0` | compound | `pipeline.cpp:14253-14256` |
| 11 | Magnify Scale/Trim | gated on Profile only; also dead under Sharp Window | compound | `pipeline.cpp:14252` **and** `14264` |

Rev 2 flagged #7 and #8 as probable renderer bugs. **Both were adjudicated INTENDED and require no renderer change** — they are UI facts to be gated and explained, not defects to be fixed. #7 is correct behaviour for an edge effect on a shape with no edge; #8 was a mislabel on my part, since `shape4.y` is genuinely a spatial-density parameter shared by two terms and the animation phase lives elsewhere. Both now carry relevance rules (§2.3) and tooltips (§6.4) instead of a bug report.

---

## 3. UI architecture proposal

### 3.1 Promote to a real C++ class

One-line change at `llviewerfloaterreg.cpp:679`:

```cpp
// before
LLFloaterReg::add("ultimate_diopter", "floater_ultimate_diopter.xml", &LLFloaterReg::build<LLFloater>);
// after
LLFloaterReg::add("ultimate_diopter", "floater_ultimate_diopter.xml",
                  (LLFloaterBuildFunc)&LLFloaterReg::build<ALFloaterUltimateDiopter>);
```

Every existing `control_name=` binding and all 180 `Diopter.ResetControl` bindings keep working untouched — `LLFloater::postBuild` still runs the XML wiring. This is a genuinely zero-risk conversion.

**Critical constraint the new class must respect:** the app-lifetime preset→Custom listeners at `llviewerfloaterreg.cpp:565-618` (44 diopter settings) and `:620-676` (49 kaleido settings) watch every preset-owned setting. **If the new class writes any of those settings programmatically it will force `CineDiopterPreset = 0`.** Either hold `LLPipeline::sDiopterPresetMaterializing` around such writes, or — better — make the class read-only with respect to settings except for explicit user actions (eyedropper, range presets).

### 3.2 Gating strategy — the recommendation

**Recommended: a three-tier rule, not one strategy.** A single strategy is wrong here because "this control is inert" has three genuinely different meanings, and the director needs to be told which one applies.

| Tier | Meaning | Treatment | Why |
|---|---|---|---|
| **A — Not part of this mode** | Star Points under Circle; Path Freq X under Sweep; every kaleido control under Tool Mode = Diopter | **Hide**, in a same-rect overlay panel | ~85% of the dead controls. Showing them is pure noise; they will never matter until the mode changes, and when it changes they arrive as a set. |
| **B — Overridden right now** | Base Distance under Camera Focus; Power under Manual Lens Distance; Size under On-Lens | **Disable + dim, and rewrite the tooltip to say why** | The value is still *the user's*, and it comes back the moment they flip the mode. Hiding it destroys the mental model ("where did my number go?"). |
| **C — Not yet armed** | Ghost Spacing at Ghost Copies = 0; Halo Rings with the warp disarmed; Handheld Speed at Handheld = 0 | **Disable + dim, and place directly beneath the arming control** | These teach causality. Seeing six greyed sliders snap live the instant you nudge Ghost Copies off zero is how the director learns what Ghost Copies *is*. |

This matches the house convention the fork already states in `llfloaterdirector.cpp:2020-2023`, where `mActorStyleDissolveProgress` gets both `setVisible(dissolve)` and `setEnabled(active && dissolve)` — **hide for "not part of this mode", grey for "not applicable right now."**

**Rejected: pure disable-and-dim for everything.** At 181 controls, greying 128 of them leaves the director scrolling past six screens of grey to find the four live sliders on the Motion tab. Dimming communicates *state*; it does not reduce *search cost*, and search cost is the actual complaint.

**Rejected: pure hide-and-reflow of individual widgets.** Two reasons, one fatal:

1. Every control in this XML is absolutely positioned with `top_pad=`, which is resolved at **build time**. `setVisible(false)` on a slider leaves a hole; nothing moves up. This is exactly the bug already shipping in `alfloaterlightbox.cpp:530-551`, where the six hidden AMD LPM slider groups blank a band and leave it blank.
2. Reflow-on-every-mode-change means the control under the cursor moves. Mid-shoot, that is worse than clutter.

**Rejected: `LLLayoutStack` conversion.** It does work — `LLLayoutPanel::setVisible` marks the parent stack `mNeedsLayout` (`lllayoutstack.cpp:158-169`), and `collapsePanel()` is the reliable driver (`llsidepanelinventory.cpp:370`, `alfloatersceneexplorer.cpp:1996`). But no AL* settings floater in this fork uses it, it requires the stack to be the panel's *direct* parent, and converting 1829 lines of `top_pad` layout to nested stacks is a large, high-regression rewrite for a benefit (variable-height reflow) that overlay panels deliver without motion.

### 3.3 The house pattern to copy: same-rect overlay panels

`ALPanelCineCamParams` already solves exactly this problem for ~30 camera modes, and it is the pattern to lift wholesale.

`panel_cinecam_params.xml:330, 395, 418` — every mode panel at **identical `top` and `height`**, all `visible="false"`:

```xml
<panel name="panel_mode_bone"  left="0" top="135" width="320" height="98" visible="false" ... >
<panel name="panel_mode_orbit" left="0" top="135" width="320" height="98" visible="false" ... >
<panel name="panel_mode_hover" left="0" top="135" width="320" height="98" visible="false" ... >
```

and the entire visibility engine is 13 lines (`alpanelcinecamparams.cpp:511-524`):

```cpp
void ALPanelCineCamParams::updateModePanel()
{
    const S32 mode = gSavedSettings.getS32("CinematicCamMode");
    for (const ModeEntry& entry : modeTable())
    {
        if (entry.mPanel)
        {
            if (LLPanel* panel = findChild<LLPanel>(entry.mPanel))
            {
                panel->setVisible(entry.mMode == mode);
            }
        }
    }
}
```

Nothing reflows because nothing is ever in a hole — the panels are stacked in **Z**, not in Y. The band is sized once to the tallest member.

### 3.4 Reconciling the two table formats

Use **both**, at different granularities:

- **`ModeEntry`-style panel table** (mirroring `alpanelcinecamparams.h:57-63`) for the clean *partitions*: Motion params (12 disjoint sets), Spin params (4), Aperture params (5), Kaleido Pattern params (24), Protect params (4). These are one-of-N — an overlay band is exactly right.
- **`sRelevance` bitmask table** (§2.3) for *overlapping* membership inside a panel: the Shape tab's edge group, where Hollow is shapes 3–11, Arc/Broken are 4–11 plus On-Lens, and Corner Rounding is {6,7,9,10}. A bitmask expresses that; an overlay panel would need duplicate widgets.

The bitmask table is also what drives Tier B and Tier C dimming, which overlay panels cannot express at all.

### 3.5 The refresh loop

Follow `LLFloaterDirector::draw()` (`llfloaterdirector.cpp:552-565`) — unconditional refreshers, guarded by a cheap signature string:

```cpp
void ALFloaterUltimateDiopter::draw()
{
    refreshRelevance();     // signature-guarded; see below
    refreshReadouts();      // live subject distance, per-frame, cheap
    LLFloater::draw();
}

void ALFloaterUltimateDiopter::refreshRelevance()
{
    // Rebuild only when a driving value actually moved. 20 drivers, not 181.
    const std::string sig = llformat("%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%.4f|%.4f|%.4f|%.4f|%d",
        gSavedSettings.getU32("CineDiopterToolMode"),
        gSavedSettings.getU32("CineDiopterPreset"),
        gSavedSettings.getU32("CineDiopterShape"), /* ... */ );
    if (sig == mRelevanceSig) return;
    mRelevanceSig = sig;
    // ... walk sRelevance, setVisible / setEnabled / setToolTip ...
}
```

Plus a settings listener on each of the ~20 driving controls, using the fork idiom from `alpanelcinecamparams.cpp:429-433` — **listen on `LLControlVariable::getSignal()`, not on the combo's commit callback**, so Debug Settings, preset load, and scene restore also refresh the UI. Store the `boost::signals2::connection`s and disconnect in the destructor (`alfloaterlightbox.cpp:77-81`).

Use `findChild<T>` throughout, not `getChild<T>` — tabs are lazily built and `getChild` emits dummy-widget spam (documented rationale at `alfloaterlightbox.cpp:534`).

### 3.6 Regrouping — what moves where

**The organising principle: every tab opens with a "what am I looking at" header — the mode combo and the always-live framing controls — and everything below it is that mode's parameters, nothing else.**

Concrete moves:

| Tab | New structure |
|---|---|
| **Main** | Enable · Tool Mode · **Preset** (promoted to second position) · Quality · Master Blend · Setup View. **Move out:** Center X/Y, Size, Angle, Anchor, Placement → *Shape* (they are shape/placement controls). Freeze + Freeze At → a persistent footer strip visible on every tab (it is a capture control, not a Main control). |
| **Shape** | Header: Placement · Shape · Contents · Center X/Y · Size · Angle · Stretch · Anchor. Band 1 (bitmask-gated): Seam Feather · Invert · Hollow · Arc Length · Broken · Corner Rounding · Edge Wobble ×2. Band 2 (overlay, one per shape): the 10 shape-specific sliders. |
| **Focus** | Header: Base Focus mode + Base Distance + **eyedropper** + live readout. Then Lens Focus mode + Power/Lens Distance (Tier B pair). Then the falloff group. Window Surround appears only under Contents = Sharp Window. |
| **Glass** | Header: Element Profile. Band (overlay per profile): IOR · Thickness · Rim ×4. Then a **Character** group: Character · Lateral CA · Axial CA · Field Curvature · Edge Vignette (they all scale off `character`, so they belong together and dim together at Character = 0). |
| **Halo & Ghosts** *(new — the ninth tab, split from Glass)* | The warp arming row (Ring Fold · Twist · Lobe Amount · Faceted Fold · Source Zoom) with its dependents beneath (Halo Rings · Ring Phase · Lobe Count · Lobe Phase · Segments · Feed Angle). **This fixes the worst split in the floater** — those five arming controls currently live on two different tabs (Glass and Bokeh) despite jointly gating one shader branch (`pipeline.cpp:14350`). Then Ghost Copies with its **seven** dependents (Ghost Spacing · Arc Smear · Radial Smear · Highlight Threshold · Highlight Knee · Arc Energy · Dispersion — the full set gated by `ghostCount > 0` at `ultimateDiopterF.glsl:107`). |
| **Bokeh** | Aperture Shape + overlay band per shape + Cat's-Eye. Magnify Scale/Trim move here and dim when **Element Profile > Off OR Contents = Sharp Window** (`magnify = 1.f` at `pipeline.cpp:14252` *and* `:14264`). Seam Double px + Amount as an armed pair. |
| **Motion** | Header: Motion combo. Band: overlay panel per motion mode (12 panels, sized to the tallest — **Stutter, at 7 rows: Speed plus its six Stutter sliders**, since Speed is consumed by every non-Static mode and belongs in the band, not the header). Then **Handheld** as its own armed group, then **Spin** as its own group with a 4-way overlay band. Three visually separated systems instead of one 29-item list. |
| **Kaleido** | Exactly the layout defined in §2.2: header, a 24-panel pattern band (max 5 rows), then the standing Source group (**4** rows — the Offset X/Y §3.7 pair on one row) and Cell group (5), each member gating individually. |
| **Kal Anim** | Motion header + overlay band (12), Spin group (4-way band), Protect group (4-way band). |

Net effect on the Motion tab in the default state: **29 rows become 4** (Motion combo, Handheld, Spin Mode, Spin Rate) plus two collapsed group headings. That is the "feel the tool get simpler" moment.

### 3.7 Two-column layout — recommendation: **no**

The floater is 520 wide; the inner panel is 486; a slider row is 360 + an 18px reset button = 394, leaving 90px unused on every row. Two columns of ~240 would fit geometrically.

**Do not do it.** A settings list is readable because there is exactly one label column and one value column, so the eye scans a single vertical line. Two columns double the scan paths and force the label column to ~90px, which will truncate "Anamorphic Squeeze", "Highlight Threshold", "Cell Subdivide Chance" and about thirty others.

Do these instead:
- **Reclaim the 90px**: widen sliders from 360 to 440 and move the reset button to x=452. More travel per slider is a direct precision win, and it costs one find-and-replace.
- **Pair only the natural 2-tuples side by side** at half width: Center X/Y, Path Freq X/Y, Source Offset X/Y, Anamorphic Squeeze/Angle, Depth Cut/Feather. These read as one control, not two.
- **Raise the default floater size** to 560×720 (`min_width` stays 520). It is `can_resize="true"` with `save_rect="true"`, so a director on a 4K monitor sets it once. Taller-and-narrow beats wide because the floater lives *beside* the frame during a shoot.

### 3.8 Mechanics the restructure must handle

Three things that look like details and are not.

**a) Keyboard focus is not released by hiding or disabling.** `LLView::setVisible` and `setEnabled` (`llview.cpp:477-485`, `634-648`) do not touch `gFocusMgr`, and key dispatch keeps routing to the stored focus element (`llviewerwindow.cpp:3277-3359`). So a director who tabs into Star Points, then changes Shape to Circle, ends up typing into an invisible widget — and arrow keys will keep editing a control that is not on screen. **Before any `setVisible(false)` or `setEnabled(false)` on a subtree, check whether the focus root is inside it and move focus out** (to the section's mode combo, which is the control that caused the change and is guaranteed still visible). This applies to overlay-panel swaps too, not just individual hides.

**b) Removing the scroll containers collides with saved rects.** The floater declares `min_height="420"` and `save_rect="true"`. Restored rects are clamped against the *current* min/max at `llfloater.cpp:976-1007`, so a director whose saved rect is 520×440 — perfectly usable today because every tab scrolls — will get a clipped, unscrollable floater the moment the scrollers are removed. Three options, in order of preference:

1. **Keep the scroll containers but guarantee they never scroll — Invariant W (§4.2).** Raise `min_height` to the *measured* worst-case gated content height across all **nine** tabs of the proposed layout (the eight today plus Halo & Ghosts, §3.6) and all mode combinations (~600 including chrome), so the scrollbar exists as an inert safety net for pathological resizes but is never visible in a supported configuration. `LLScrollContainer` only reaches its "always eat" branch when the vertical bar is *visible* (`llscrollcontainer.cpp:270-286`), so an invisible bar means the wheel falls through to the child slider exactly as if the container were gone. **This gets the wheel behaviour without the clipping risk, and it is what I recommend** — with the height check machine-verified in the §2.3 test target rather than eyeballed, since a hand-maintained bound decays in one release.
2. Migrate saved rects on first run of the new version (bump a `CineDiopterUIRectVersion` setting; clamp up if below the new minimum).
3. Remove the containers and accept that some users' saved rects need a manual resize. Cheapest to write, worst to receive.

**c) Section headings and overlay bands must be sized to the tallest member.** The band height is fixed at build time (the `ALPanelCineCamParams` pattern), so it is set by the largest parameter set — on Motion that is **Stutter at 7 rows** (Speed + six Stutter sliders), and on Kaleido **5 rows** (Droste Spiral / Sawtooth Sun), per the authoritative inventory in §2.2. Budget for that when checking the tab fits; the win is still large (Motion's worst case is 7 rows against today's 29).

---

## 4. Mouse wheel plan

### 4.1 What already exists — this is 90% done

**`LLSlider::handleScrollWheel` is already implemented in this fork**, behind an `[AL]` opt-in param (`llslider.cpp:290-301`):

```cpp
bool LLSlider::handleScrollWheel(S32 x, S32 y, LLScrollDelta delta)
{
    // [AL] wheel_adjust: hovered vertical wheel steps a horizontal slider by its
    // increment (wheel up = increase), giving precision control without dragging.
    if ( mOrientation == VERTICAL || (mWheelAdjust && mOrientation == HORIZONTAL) )
    {
        F32 new_val = getValueF32() - delta.mClicks * getIncrement();
        setValueAndCommit(new_val);
        return true;
    }
    return LLF32UICtrl::handleScrollWheel(x,y,delta);
}
```

`wheel_adjust` is an `Optional<bool>` on both `LLSlider::Params` (`llslider.h:69`) and `LLSliderCtrl::Params` (`llsliderctrl.h:58`), defaulting `false`, forwarded at `llsliderctrl.cpp:146-151`. It already ships enabled in `floater_director.xml:1448`, `panel_cine_light_rig.xml:162/779/789`, and `floater_lightbox_settings.xml:4343`.

**The diopter floater simply never opted in.** The change itself is a find-and-replace adding `wheel_adjust="true"` to 149 sliders, with no C++ at all — **but it must not ship before the scrolling question is resolved.** See §4.2.

### 4.2 The scroll-container conflict, and the rule

`LLScrollContainer::handleScrollWheel` (`llscrollcontainer.cpp:265-300`) gives children first crack:

```cpp
bool LLScrollContainer::handleScrollWheel( S32 x, S32 y, LLScrollDelta delta )
{
    // Give event to my child views - they may have scroll bars
    if (LLUICtrl::handleScrollWheel(x,y,delta))
        return true;
    LLScrollbar* vertical = mScrollbar[VERTICAL];
    if (vertical->getVisible() && vertical->getEnabled())
    {
        if (vertical->handleScrollWheel( 0, 0, delta ) ) { updateScroll(); }
        return true;                    // Always eat the event
    }
    // ...
}
```

So a `wheel_adjust` slider inside a scroll container **wins** and the panel does not scroll. That is THE conflict, and it is real: the diopter's eight tabs are all scroll containers.

**Recommended disambiguation rule — geometric, and it already exists for free.**

`LLSliderCtrl` does **not** override `handleScrollWheel`, so the wheel reaches the child `LLSlider` only when the cursor is geometrically inside the slider *bar's* rect. For a typical diopter row (`width="360" label_width="140"`, 2 decimals), `llsliderctrl.cpp:95-121` computes `text_width ≈ 29`, so the bar spans x = 140…331 — **191px of the 486px inner panel width.**

That yields a natural two-zone rule with zero code:

> **Wheel over the slider bar → adjusts the value. Wheel over the label, the numeric readout, the reset-button column, or the right-hand gutter → scrolls the panel.**

~60% of each row's width still scrolls, and the label column is a contiguous 140px target running the full height of the tab.

**But rev 1 over-sold this as sufficient. It is not.** Today's tabs are 560–820px of tightly stacked 20px rows; the bar bands very nearly tile the vertical extent of the scrollable area. A director who parks the pointer mid-panel and spins the wheel to reach the bottom of the Motion tab will, with high probability, be sitting on a bar — and will silently mutate a setting instead of scrolling. **A UI that changes render state when the user meant to scroll is worse than a UI with no wheel support at all**, because the damage is invisible until it shows up in the shot. Rev 1's claim that this could ship stand-alone in Phase 1 was wrong.

**Sequencing rule: plain `wheel_adjust` ships only in the release that guarantees no visible scrollbar.** Rev 2 stated this but then let Phase 3 admit that a scrollbar could still appear at minimum floater size — which leaves the hazard live in exactly the configuration a cramped-screen user runs. Rev 3 commits to the stronger form.

> **Invariant W: in every supported configuration — every tab, every mode combination, every floater size from `min_height` upward — the vertical scrollbar is not visible.**

`LLScrollContainer` only reaches its "always eat" branch when the vertical bar is *visible* (`llscrollcontainer.cpp:270-286`), so Invariant W makes the wheel unambiguous by construction, and the two-zone geometric rule above becomes a genuine safety net rather than the primary defence.

**How it is enforced, not merely asserted.** The worst-case content height per tab is a static property of the gated layout: header + tallest overlay band + always-live groups + section headings. Compute it and check it:

1. **Set `min_height` to the measured worst case across all nine tabs of the proposed layout, plus chrome.** (Nine, not eight: §3.6 splits Glass into Glass + Halo & Ghosts.) On current estimates that is ~600px — Motion (7-row band), Kaleido (5-row band plus its two standing groups), and the new Halo & Ghosts are the tall ones; the default rises to 720 anyway, so the practical cost is a floater that cannot be shrunk below roughly its useful size.
2. **Machine-check it in the same test target** as §2.3: walk each tab's XUI, compute the maximum height over every mode combination the panel table can produce, and assert it fits the viewport at `min_height`. **A future control addition that would break Invariant W then fails the build**, which is the whole point — a hand-maintained invariant would decay in one release.
3. Keep the `scroll_container`s in place as an inert safety net (§3.8b), never as a working scroll surface.

**If Invariant W ever cannot be met** — a future tab genuinely needs more room than the smallest supported floater — the sanctioned fallback is the ancestor-scrollbar gate, and it must land **in the same commit that breaks the invariant**, never after:

```cpp
// llslider.cpp — fallback only; one walk up the parent chain
bool claim = mWheelAdjust && mOrientation == HORIZONTAL;
if (claim && ancestorScrollbarVisible(this) && !gKeyboard->getKeyDown(KEY_CONTROL))
    claim = false;                 // let the panel scroll; Ctrl+wheel still adjusts
```

I am recommending Invariant W over shipping this gate pre-emptively: the gate trades a permanent discoverability cost (wheel silently does nothing until you learn about Ctrl) against a problem the layout work removes outright.

**Do NOT add a `handleScrollWheel` override to `LLSliderCtrl`.** It would make the whole row (label included) swallow the wheel and leave the tab genuinely unscrollable. This is the one tempting change that must be refused.

**The gating work and the wheel work are not merely complementary — the wheel work depends on the gating work.** That dependency drives the re-phasing in §7.

### 4.3 Step sizing — and a snapping trap

Wheel-up = increase (`- delta.mClicks * getIncrement()`; wheel-up yields negative `mClicks`). One click = one `increment`, matching the arrow-key step in `handleKeyHere` (`llslider.cpp:269-288`). Keep that.

**A coarse modifier is not a nicety here, it is required.** Click counts end-to-end at the shipping increments:

| Slider | Range | Increment | Clicks |
|---|---|---|---|
| `CineDiopterKalTwist` | −720 – 720 | 1 | **1440** |
| `CineDiopterKalDepthCut` | 0.1 – 256 | 0.1 | **2560** |
| `CineDiopterBaseFocusM` | 0.1 – 128 | 0.1 | **1280** |
| `CineDiopterLensFocusM` | 0.1 – 128 | 0.1 | **1280** |
| `CineDiopterKalDepthFeatherM` | 0.05 – 32 | 0.05 | **640** |
| `CineDiopterAngleDeg` | −180 – 180 | 1 | 360 |

Every one of those is a depth or angle control — i.e. exactly the "adjusting depth is difficult" complaint.

Adopt the modifier convention `LLSpinCtrl` already uses (`llspinctrl.cpp:176-190`) so the two widget families agree — **Ctrl or Alt = ×10 coarse** — but note the trap:

> A snapping bypass **does** exist — `LLSlider::setValue` takes a `precision_override` flag (`llslider.cpp:101-114`) — but the wheel path does not reach it: `handleScrollWheel` calls `setValueAndCommit`, which calls `setValue` without the flag (`llslider.cpp:158-166`). So a *fine* modifier (`shift = increment × 0.1`) would today be silently rounded back to zero change, and enabling it means plumbing `precision_override` through `setValueAndCommit` — which changes behaviour for every existing `wheel_adjust` slider in the viewer, not just the diopter's.

So: **ship the coarse modifier only.** Fine steps are unnecessary anyway — the diopter's increments are already 0.005–0.01 on the unit-range sliders. The minimal change:

```cpp
// llslider.cpp, inside handleScrollWheel
F32 step = getIncrement();
if (gKeyboard->getKeyDown(KEY_CONTROL) || gKeyboard->getKeyDown(KEY_ALT))
{
    step *= 10.f;                  // coarse; stays a multiple of mIncrement,
}                                  // so setValue's snapping is a no-op
F32 new_val = getValueF32() - delta.mClicks * step;
setValueAndCommit(new_val);
```

`step = increment × 10` remains an exact multiple of `increment`, so the snapping in `setValue` cannot fight it. That property is why ×10 works and ×0.1 does not.

### 4.4 Global or scoped? — **scoped, deliberately**

Do **not** flip the `wheel_adjust` default to `true` globally. Every scroll container in the viewer (Preferences, Debug Settings, the appearance editor, `floater_lightbox_settings.xml`) would stop scrolling over its sliders, and users have years of muscle memory there. The `false` default is documented as deliberate at `llslider.h:65-68` for exactly this reason.

The coarse-modifier change in §4.3 *is* global, and that is correct — it adds a capability without changing any existing behaviour (no modifier = identical to today).

If a viewer-wide opt-in is ever wanted, the right lever is a new `LLCachedControl<bool>` read inside `LLSlider::handleScrollWheel` that ORs with `mWheelAdjust` — one line, user-togglable, reversible. Do not build it now.

### 4.5 The combo boxes are a live hazard, not a footnote

Rev 1 filed this as "worth verifying". That was too soft. `LLComboBox::handleScrollWheel` (`llcombobox.cpp:958-997`) does not merely move the highlight — it **changes the selection and commits**, eating the event except at the list boundary, where it returns false. The diopter has **25 combos**, and they are the highest-consequence controls in the floater:

- A stray wheel over **Pattern** or **Motion** commits a mode change — which, under §3.3 overlays, **replaces the controls under the pointer mid-gesture**. The next wheel click lands on a different slider than the one the user was looking at.
- A stray wheel over **Preset** commits a preset change — which, under the §6.3 materialization design, **writes 44 or 49 persisted settings**. One careless scroll can overwrite the director's Custom bank.
- `Tool Mode` swaps six tabs for two.

`LLSpinCtrl` also wheels unconditionally (`llspinctrl.cpp:481`), though the diopter uses no spinners.

**Required, not optional: a scoped wheel opt-out.** Two viable mechanisms:

1. **A `wheel_select` opt-out param on `LLComboBox`** mirroring `wheel_adjust`'s shape — `Optional<bool>`, defaulting **true** so nothing else in the viewer changes, set `false` per-combo in the diopter XML. Cheapest and most surgical.
2. **Require the dropdown to be open or focused** before the wheel changes selection. More principled and arguably the right global default, but it is a viewer-wide behaviour change and belongs in its own discussion.

Take option 1. It must land **with or before** the `wheel_adjust` rollout — together they are what make a wheel gesture safe anywhere in this floater.

**Recommendation: opt out all 25. Do not curate a subset.** Rev 2's "the 10 mode/preset combos and the kaleido equivalents" was not an implementable allowlist, and drawing the line by consequence invites a reviewer to re-litigate each one. Every combo in this floater selects from a discrete enumeration where a wheel-commit is a semantic jump, and **not one of them has a sensible wheel-scrub interaction** — there is no continuum to scrub. A blanket rule is shorter to specify, trivial to verify (grep the XML: 25 combos, 25 opt-outs), and has no failure mode.

The complete inventory — `wheel_select="false"` on all of these:

| Tab | XUI name | Label | Consequence of a stray wheel |
|---|---|---|---|
| Main | `diopter_tool_mode` | Tool Mode | swaps six tabs for two |
| Main | `diopter_preset` | Preset | **writes 44 persisted settings** |
| Main | `diopter_quality` | Quality | changes tap count mid-shoot |
| Main | `diopter_track` | Anchor | detaches the lens from the subject |
| Main | `diopter_placement` | Placement | Framed ↔ On-Lens; kills Size + 10 shape sliders |
| Main | `diopter_debug` | Setup View | overlays a debug view on the frame |
| Shape | `diopter_shape` | Shape | **replaces the overlay band under the pointer** |
| Shape | `diopter_content` | Contents | Diopter ↔ Sharp Window; flips 5 dependent controls |
| Focus | `diopter_focus_mode` | Base Focus | manual ↔ camera focus |
| Focus | `diopter_lens_mode` | Lens Focus | swaps which of Power / Lens Distance is live |
| Glass | `diopter_profile` | Element Profile | arms/disarms refraction + 6 controls |
| Bokeh | `diopter_ap_shape` | Aperture Shape | **replaces the overlay band** |
| Bokeh | `diopter_pattern_mode` | Faceted Fold | arms the halo warp |
| Motion | `diopter_motion_mode` | Motion | **replaces the overlay band** |
| Motion | `diopter_pulse_target` | Pulse Target | Size ↔ Focus rack |
| Motion | `diopter_spin_mode` | Spin Mode | **replaces the spin band** |
| Kaleido | `kal_preset` | Preset | **writes 49 persisted settings** |
| Kaleido | `kal_mode` | Pattern | **replaces one of 24 overlay bands** |
| Kaleido | `kal_edge_wrap` | Edge Wrap | mirror / tile / clamp |
| Kaleido | `kal_debug` | Setup View | overlays a debug view |
| Kal Anim | `kal_motion_mode` | Motion | **replaces the overlay band** |
| Kal Anim | `kal_pulse_target` | Pulse Target | Zoom / Twist / Offset |
| Kal Anim | `kal_spin_mode` | Spin Mode | **replaces the spin band** |
| Kal Anim | `kal_protect_mode` | Protect Mode | arms/disarms 8 controls |
| Kal Anim | `kal_protect_anchor` | Protect Anchor | pattern / fixed / focus |

The eight bolded rows are the acute cases — under §3.3 overlays they replace the controls beneath the pointer mid-gesture, so the *next* wheel click lands on a different slider than the one the user was looking at. The two preset rows are the destructive ones. The remaining fifteen are merely wrong rather than dangerous, and are included because the blanket rule is worth more than the exceptions.

**Validator hook:** extend the §2.3 structural layer to assert that every `<combo_box>` in `floater_ultimate_diopter.xml` carries `wheel_select="false"`, so a combo added later cannot silently reintroduce the hazard.

---

## 5. Depth and focus ergonomics

### 5.1 Why depth is hard here

Four separate reasons, all fixable:

1. **The number has no referent.** "Base Distance 8.0 m" means nothing unless you know how far the subject is. Nothing in the viewer tells you.
2. **The sliders are unusable at their increments** — 1280 to 2560 wheel clicks, or ~0.7 m per pixel of drag on a 191px bar (§4.3).
3. **Exactly one of Power / Lens Distance is always dead** (`pipeline.cpp:14235`), and nothing says which.
4. **"Camera focus point" can silently fall back.** `dof_focus_live` (`pipeline.cpp:14004`) requires `RenderDepthOfField` on, not-in-build-mode, and a non-zero `sLastFocusPoint`. When it fails the code falls back to `gAgentCamera.getFocusGlobal()` (`:14218-14231`) — a different plane, with no UI indication whatsoever.

### 5.2 The eyedropper

Copy `ALToolGhostPlace` (`altoolghostplace.h/.cpp`) — the fork's own one-shot world-point picker, already used by the Ghost Studio's crowd-facing pick. Strip the ghost/resolver branches and you have a ~60-line `ALToolFocusPick`.

```cpp
class ALToolFocusPick final : public LLTool, public LLSingleton<ALToolFocusPick>
{
    LLSINGLETON(ALToolFocusPick);
public:
    using DepthPickCallback =
        std::function<void(bool accepted, F32 depth_m, const LLVector3d& point_global)>;
    bool arm(const LLUUID& owner_id, DepthPickCallback cb);
    bool cancelForOwner(const LLUUID& owner_id);
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;   // cancel
    bool handleKey(KEY key, MASK mask) override;                   // Esc cancels
    void handleSelect() override;                                  // set cursor
    void handleDeselect() override;                                // disarm(true)
private:
    void disarm(bool notify_cancel);   // moves mCallback out, fires (false, 0.f, {})
    LLUUID mOwnerId;
    DepthPickCallback mCallback;
};
```

`handleMouseDown`:

```cpp
    LLPickInfo pick = gViewerWindow->pickImmediate(x, y, /*transparent*/ false, /*rigged*/ true);
    if (pick.mPosGlobal.isExactlyZero() || !pick.mPosGlobal.isFinite())
        return true;                                  // sky miss: stay armed, try again
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    const LLVector3 pt_agent = gAgent.getPosAgentFromGlobal(pick.mPosGlobal);
    const F32 depth_m = (pt_agent - cam->getOrigin()) * cam->getAtAxis();
    if (depth_m <= 0.05f) return true;                // behind the lens
    DepthPickCallback accepted = std::move(mCallback);
    mOwnerId.setNull();
    accepted(true, depth_m, pick.mPosGlobal);
    LLToolMgr::getInstance()->clearTransientTool();
    return true;
```

Four decisions worth stating explicitly:

- **Use `mPosGlobal`, never `mIntersection`.** `mIntersection` is only filled on `PICK_OBJECT`/`PICK_FLORA` hits (`llviewerwindow.cpp:6987-6990`); on a terrain hit it stays `(0,0,0)` (`:7038`). An eyedropper pointed at the ground would silently read zero. Miss-detection is `mPosGlobal.isExactlyZero()`, the fork's own idiom at `altoolpathedit.cpp:55`.
- **Use the dot-product distance, not `dist_vec`.** `(pt_agent - origin) * cam->getAtAxis()` is what `renderDoF` (`pipeline.cpp:18672`) and `renderUltimateDiopter` (`:14207`) both consume. Euclidean distance would be subtly wrong off-axis, so the eyedropper and "Camera focus point" mode would disagree.
- **Use `pickImmediate`, not `pickAsync`.** Synchronous result, no static-callback relay, no `LLHandle` lifetime window — and it dodges the `fetchResults()` early-return at `llviewerwindow.cpp:6960` that swallows the async callback on a far-land miss.
- **`pick_rigged = true`** (unlike the fork's placement tools, which use `false`). A director focusing on an actor's face wants the ray to stop at the body.

**Cursor:** `UI_CURSOR_PIPETTE` (`llcursortypes.h:66`), set in **both** `handleSelect()` and `handleHover()` — the fork's pattern at `altoolghostplace.cpp:209-237`. Cancel via Esc and right-click, and `handleDeselect() → disarm(true)` so tool replacement always resolves the callback exactly once.

**Where the eyedroppers go and what each writes:**

| Button | Writes | Also must set | Clamp |
|---|---|---|---|
| Focus tab, beside Base Distance | `CineDiopterBaseFocusM` | `CineDiopterFocusMode = 0` (Manual) | 0.1 … 128 |
| Focus tab, beside Lens Distance | `CineDiopterLensFocusM` | `CineDiopterLensFocusMode = 1` (Manual distance) | 0.1 … 128 |
| Kal Anim, beside Depth Cut | `CineDiopterKalDepthCut` | `ProtectMode`: 0→2, 1→3 (arm depth protection) | 0.1 … 256 |

The mode write is mandatory. Writing `CineDiopterBaseFocusM` while `CineDiopterFocusMode == 1` does nothing — `pipeline.cpp:14204` overwrites it from the camera focus point every frame. Clamping to the slider's own `min_val`/`max_val` is mandatory too, or the setting and the thumb position disagree.

**Live preview while hovering: no — but the reason is narrower than rev 1 claimed.** The preset→Custom hazard applies to the **kaleido picker only**: `CineDiopterKalDepthCut` and `CineDiopterKalProtectMode` are both in the kaleido owned array (`llviewerfloaterreg.cpp:629-645`), so a hover-write there would flip the preset to Custom and materialize 49 settings. The diopter's `CineDiopterBaseFocusM`, `CineDiopterLensFocusM` and the two focus-mode settings are **not** in the diopter owned array (`:571-588`), so hover-writing them would be preset-safe.

That still leaves two reasons to refuse it, and they are enough: `pickImmediate` performs a GL pick that is not free per-frame at 4K, and a cancelled pick would leave the setting mutated with no undo. **Instead**, throttle a hover pick to ~10 Hz and display the metre value in the floater's readout strip, committing only on click. The director sees the number before they commit, and nothing is written until they do. Apply this uniformly across all three pickers — the diopter's exemption is not worth a behavioural inconsistency between two buttons that look identical.

### 5.3 Quick wins — higher value per line than the eyedropper

**a) The live subject-distance readout.** `LLPipeline::sLastFocusPoint` is a public static (`pipeline.h:1638-1644`), agent-space, updated every frame `renderDoF` runs. The per-frame *distance* is a function-local static inside `renderDoF` and not readable — recompute it. Reuse the `dof_focus_live` predicate verbatim (it is already duplicated three times: `pipeline.cpp:14000-14006`, `:14204-14232`, and `alKaleidoFocusUV` at `:14811`):

```
Subject: 12.4 m          ← next to Base Distance and Depth Cut
```

**The readout must report provenance, not just a number.** "Camera focus point" resolves through a three-step ladder and can land anywhere in it (`pipeline.cpp:14204-14232`): the live DoF target; failing that the alt-zoom `getFocusGlobal()` fallback; and failing *that* — no region, a zero focus point, or a forward distance ≤ 0.05 — it silently leaves `base_focus_m` on the manual slider. Three very different situations that currently look identical. So the shared helper returns provenance, not a bool:

```cpp
enum ALFocusProvenance
{
    AL_FOCUS_LIVE_DOF,      // sLastFocusPoint, DoF running        -> "tracking subject"
    AL_FOCUS_ALT_ZOOM,      // gAgentCamera.getFocusGlobal()       -> "tracking camera target"
    AL_FOCUS_MANUAL,        // ladder failed; slider is live       -> "using the slider"
};
ALFocusProvenance alDiopterResolveBaseFocus(F32& out_m);   // shared by UI and renderer
```

driving three distinct strings:

```
Base focus: 12.4 m — tracking subject
Base focus: 12.4 m — camera focus unavailable, tracking camera target
Base focus: 8.0 m — no focus target; the slider below is in control
```

The third case is the one that matters most: it is the only signal the director will ever get that a control the UI would otherwise grey out is in fact authoritative. It is also why `PRED_BASE_FOCUS_MANUAL` in §2.3 must query this helper rather than reading the focus-mode combo.

**Factor the helper and the distance computation into pipeline.cpp** and call them from all four sites (`:14000-14006`, `:14204-14232`, `alKaleidoFocusUV` at `:14811`, and the UI), so the UI can never disagree with the renderer.

**b) "Set from camera focus" button.** Same helper, writes the metres and forces `FocusMode = 0`. One line beside the eyedropper, and it is the fastest path for the common case ("focus where the camera is already focused, then nudge").

**c) Surface the depth setup view.** `Setup View = Depth` already exists (`CineDiopterDebugView = 3`, `ultimateDiopterF.glsl:198`, rendering metres/50), and the kaleido's `Setup View = 3` renders the subject mask with a live depth preview (`ultimateKaleidoF.glsl:755-772`). Both are buried in a Main-tab combo. **Put a "Show depth" toggle button directly beside the depth sliders.** Calibrating a depth cut while looking at the depth map is a completely different experience from calibrating it blind.

**d) Expose `RenderFocusPointCrosshair`** (`settings_alchemy.xml:2572`, drawn by `LLPipeline::renderFocusPoint`, `pipeline.cpp:7555-7594`) as a checkbox in the Focus tab. It already draws the focus point in-world; the director just has no idea it exists. `RenderFocusPointLocked` is worth surfacing beside it.

**e) Range presets instead of log sliders.** The metre sliders span three decades. A log-scale slider is the textbook answer, but `LLSlider` has no log mode, and adding one collides with the unconditional increment snapping in `setValue` (§4.3) and with the numeric readout. **Not worth touching `LLSlider` for.**

Instead, put a small range combo beside each metre slider and call `setMinValue`/`setMaxValue` (`llf32uictrl.h:65-68`) at runtime.

**Ranges must be per-control, not one shared table.** Rev 1 proposed a single Macro/Room/Set/Vista ladder for every metre slider; that recreates the exact failure this document is trying to remove — a UI that offers values the renderer will not honour. The three controls have different hard ceilings, and one of them is clamped in the renderer rather than the XML:

| Control | XML range | Renderer clamp | Ceiling that governs |
|---|---|---|---|
| `CineDiopterBaseFocusM` | 0.1 – 128 | `0.1 – 4096` (`pipeline.cpp:14203`) | **128** (XML) |
| `CineDiopterLensFocusM` | 0.1 – 128 | `0.1 – 4096` (`:14237`) | **128** (XML) |
| `CineDiopterKalDepthCut` | 0.1 – 256 | — | **256** |
| `CineDiopterKalDepthFeatherM` | 0.05 – 32 | **`0.05 – 32`** (`:14515-14516`) | **32** (both) |

So: three range tables, each capped at that control's own ceiling, each carrying its own increment **and displayed decimal precision** (a Vista range showing three decimals is noise; a Macro range showing one is unusable).

| Base / Lens Distance (cap 128) | Depth Cut (cap 256) | Depth Feather (cap 32) |
|---|---|---|
| Macro 0.1 – 2, inc 0.01, 2dp | Near 0.1 – 4, inc 0.02, 2dp | Tight 0.05 – 1, inc 0.01, 2dp |
| Room 0.5 – 15, inc 0.05, 2dp | Room 1 – 20, inc 0.1, 1dp | Soft 0.5 – 6, inc 0.05, 2dp |
| Set 5 – 60, inc 0.25, 1dp | Set 10 – 80, inc 0.5, 1dp | Wide 2 – 32, inc 0.25, 1dp |
| Vista 20 – 128, inc 0.5, 1dp | Vista 50 – 256, inc 1.0, 0dp | — |

Every entry lands at 150–300 clicks and 2–5px of drag per step on a 191px bar. Log resolution, zero new widget math, and the range names are the language a director already thinks in.

**Changing the range at runtime needs an explicit value policy.** `setMinValue`/`setMaxValue` update the bounds but do not re-clamp the current value, and `updateThumbRect` (`llslider.cpp:125-140`) will then place the thumb off the track — pinned at an end, or past it — until the next edit snaps it back. Silently leaving that state is worse than the problem being solved. The policy:

1. On range change, **clamp the setting into the new bounds and commit it immediately**, so the thumb, the numeric readout, and the renderer agree on the same frame.
2. If the clamp would actually move the value, **say so** rather than silently retargeting the shot: *"12.4 m is outside Macro — clamped to 2.0 m"*, with the range combo reverting on a single undo.
3. **Prefer never to hit case 2:** auto-select the range on open and after every eyedropper pick, choosing the narrowest range that contains the current value. Manual range changes then become rare and deliberate.

---

## 6. What "smart" means, concretely

Four behaviours. Nothing here is AI, prediction, or magic — "smart" means *the floater knows what it is currently capable of, and says so*.

**1. The floater reshapes itself on mode change.** Changing Pattern from Radial Mirror to Fly's Eye swaps the parameter band beneath it from `Segments · Twist` to `Segments · FX Amount · FX Flow`, while in the standing Cell group below, Size Scatter, Breathing, Merge and Tint light up and Subdivide stays dark — exactly the inventory in §2.2. The header stays put; only the band swaps and the standing groups re-gate. Driven by `LLControlVariable::getSignal()`, so it also fires when a preset, a scene load, or Debug Settings changes the mode.

**2. An "Active controls only" toggle** (new setting `CineDiopterUIActiveOnly`, default **on**), as a checkbox in the floater's footer strip.

- **On** — Tier A hidden, Tiers B and C dimmed. The working mode.
- **Off** — **every control that belongs to the currently selected modes is shown, including the inactive ones, all dimmed with explanatory tooltips.** The learning mode.

**Rev 1 defined Off as "everything visible". That is not implementable against same-rect overlays** — the mode panels occupy one rect and only one can be visible at a time (`alpanelcinecamparams.cpp:514-521`); showing all 24 kaleido pattern bands at once would stack them on top of each other. Off therefore means *within the selected mode*, not *across all modes*: the Tier-C armed-state controls come back (Ghost Spacing at zero copies, Halo Rings with the warp disarmed, Handheld Speed at zero), and the Tier-B overridden ones stay visible as they always do — but Star Points under a Circle shape does not reappear, because there is nowhere to put it.

If a genuine "show me everything" learning view is wanted later, it needs its own layout — a separate scrolling flat list, essentially today's floater with headings — rather than a flag on the gated one. I would not build it: the filter box (below) covers "where did control X go" more cheaply, and the mode combo covers "what else could this do".

**Honest limitation to state in the UI:** Off cannot restore rev-1 muscle memory. The §3.6 regroup physically moves controls between tabs, so a director who had memorised "Size is third on Main" will not find it there in any mode. The toggle preserves *visibility*, not *position*. Say so in the tooltip and in the release note; do not let the toggle imply a legacy layout that no longer exists.

**Shipping rule: this toggle ships in the first release that hides anything** — not in a later polish phase. Rev 1 filed it under Phase 5, which would mean shipping Tier-A hiding with no escape hatch at all. It moves to Phase 3 (§7).

**3. Preset-first, and presets that tell the truth.**

The highest-value fix in this document — and, as rev 1 wrote it, a data-loss bug. **Materialize the preset into the sliders at selection time, not at first edit — but only with shadow storage for the Custom bank and a startup sync.** Three pieces, all required together.

**Piece 1 — persistent shadow storage for the Custom bank.**

The 44 diopter / 49 kaleido preset-owned settings are `Persist = 1`; `gSavedSettings` is flushed to `settings.xml` at logout (`llappviewer.cpp:1989-1992`). So materializing on selection **permanently overwrites the director's hand-built Custom look**, and rev 1's proposed session-scratch `LLSD` in Phase 5 was both too late and too weak — it would not survive a restart, and a crash mid-session would lose the bank anyway.

Two workable mechanisms:

| | Parallel `CineDiopterCustom*` settings | `diopter_custom.llsd` in `user_settings/` |
|---|---|---|
| Shape | 93 new `Persist=1` keys shadowing the owned fields | one LLSD map, written on transition |
| Pros | atomic with the rest of settings; no new I/O path; Debug Settings can inspect it | no settings-file bloat; trivially extensible; easy to version |
| Cons | **+93 keys (a 51% increase in this feature's settings footprint)**; every future owned field needs a twin | needs explicit save/load and crash-safety care |

**Recommend the LLSD file.** 93 shadow keys is a maintenance tax paid forever on a feature whose field list is still growing, and the twin-key scheme fails silently the moment someone adds a field to `ALDiopterLook` without adding its shadow. A single versioned map, written whenever the preset leaves Custom and read whenever it returns, keeps the invariant in one place. Write it through the same path the fork already uses for its other user-settings LLSD blobs, and write it *before* the first materialize, not on the transition back.

**Piece 2 — the transition-reason flag, and why rev 2's listener was destructive.**

Rev 2's listener restored the bank whenever it saw `preset == 0`. That is a data-loss bug, because **`preset == 0` has two entirely different meanings** and the existing edit flow produces the dangerous one. The shipping auto-Custom listener (`llviewerfloaterreg.cpp:599-615`) ends with:

```cpp
U32 active = gSavedSettings.getU32("CineDiopterPreset");
if (active != 0)
{
    LLPipeline::materializeDiopterPreset(active, changed ? changed->getName() : std::string());
    gSavedSettings.setU32("CineDiopterPreset", 0);          // <-- fires the new listener
}
```

So an ordinary slider drag under a preset materializes the look, then sets the preset to 0 — and rev 2's listener would have promptly **restored the old bank over the top, erasing the edit the user just made** and the materialized look with it. The promised "re-snapshot after the flip" never runs, because the restore happens first.

The fix is an explicit reason flag, set by the only two callers that can legitimately reach Custom:

```cpp
enum ALPresetExit
{
    AL_EXIT_USER_SELECTED_CUSTOM,   // the user picked "Custom" in the combo -> RESTORE
    AL_EXIT_AUTO_AFTER_EDIT,        // edit/reset of an owned control        -> RE-SNAPSHOT
};
static ALPresetExit sDiopterPresetExit = AL_EXIT_USER_SELECTED_CUSTOM;
```

The auto-Custom listener sets `sDiopterPresetExit = AL_EXIT_AUTO_AFTER_EDIT` immediately before its `setU32(..., 0)`; the combo's own commit path leaves the default. The selection listener then branches on the reason, never on the value alone. **This applies to reset buttons too** — `Diopter.ResetControl` writes an owned setting and therefore travels the identical edit path (§2.3).

The corrected lifecycle:

```
Custom -> non-Custom            : snapshot bank (atomically), verify persisted, THEN materialize
non-Custom -> non-Custom        : materialize only (bank already holds the user's Custom look)
non-Custom -> 0, reason=USER    : restore the bank; do not materialize
non-Custom -> 0, reason=AUTO    : do NOT restore -- the materialized+edited state IS the new
                                  Custom look; re-snapshot the bank from it instead
```

**Piece 2b — the selection listener.**

```cpp
// llviewerfloaterreg.cpp, beside the existing preset-owned listeners
if (LLControlVariable* c = gSavedSettings.getControl("CineDiopterPreset"))
{
    c->getSignal()->connect([](LLControlVariable*, const LLSD& newv, const LLSD& oldv)
    {
        // FIRST statement, matching the shipping owned-setting listeners
        // (llviewerfloaterreg.cpp:605-608). Without this the write-back
        // revert in piece 2f re-enters here and retries the failing
        // snapshot forever.
        if (LLPipeline::sDiopterPresetMaterializing)
        {
            return;
        }

        const U32 id  = (U32)newv.asInteger();
        const U32 was = (U32)oldv.asInteger();
        const ALPresetExit reason = sDiopterPresetExit;
        sDiopterPresetExit = AL_EXIT_USER_SELECTED_CUSTOM;      // consume; default is safe

        if (id != 0)
        {
            if (was == 0 && !alDiopterSnapshotCustomBank())     // atomic; piece 2c
            {
                alDiopterAbortPresetSelection(was);             // piece 2f -- NOT `return`
                return;
            }
            LLPipeline::materializeDiopterPreset(id, std::string());
        }
        else if (reason == AL_EXIT_USER_SELECTED_CUSTOM)
        {
            alDiopterRestoreCustomBank();
        }
        else                                                    // AL_EXIT_AUTO_AFTER_EDIT
        {
            alDiopterSnapshotCustomBank();                      // the edit IS the new Custom
        }
    });
}
```

Verified safe against recursion: the preset settings are themselves absent from the owned arrays, and `sDiopterPresetMaterializing` guards the writes, so the auto-Custom listener does not re-enter and the combo does not flip back.

**Piece 2c — atomic write.** Use the fork's existing crash-safe pattern verbatim (`alcinelightrig.cpp:3284-3301`): serialize to `<path>.<uuid>.tmp`, `flush()`, check `output.good()`, `close()`, check `output.fail()`, then `LLFile::rename(temporary, path)`; on any failure `LLFile::remove(temporary)` and return `false`. A partially written bank is worse than none.

**Piece 2f — the failure path, and why `return` is not enough.**

Rev 3 handled a failed snapshot by logging and returning from the listener. **That is unsound, because the signal fires after the write has already happened.** `LLControlVariable::setValue` mutates `mValues` and only then calls `firePropertyChanged(original_value)` at the very end (`llcontrol.cpp:225-263`):

```cpp
    if(value_changed)
    {
        firePropertyChanged(original_value);
    }
}                                   // <-- new value is already stored
```

There is no veto. A listener that merely returns leaves three bad states at once: the setting holds the new preset id, the combo displays a preset that was **never materialized** (so the sliders lie exactly as before the fix), and the bank on disk is stale or absent — so a later user-selected Custom would restore stale values over the director's live settings.

The only tool available post-signal is a **write-back revert**. **Canonical operation order, stated once and used everywhere in this document:**

> **check guard flag → revert preset under the RAII guard → skip materialization → invalidate the bank in memory *and on disk* → notify.**

```cpp
// pipeline.h -- the guard, defined in full. The ctor SAVES the prior value and
// the dtor RESTORES it, rather than unconditionally clearing: a nested scope
// that set-false on exit would expose the OUTER scope's remaining writes to
// both listeners. Nesting happens for real -- the abort path runs inside the
// selection listener, which may itself already be inside a materialize.
class LLPipeline::ScopedPresetMaterializing
{
public:
    ScopedPresetMaterializing()
      : mPrior(LLPipeline::sDiopterPresetMaterializing)
    {
        LLPipeline::sDiopterPresetMaterializing = true;
    }
    ~ScopedPresetMaterializing()
    {
        LLPipeline::sDiopterPresetMaterializing = mPrior;   // restore, not clear
    }
    ScopedPresetMaterializing(const ScopedPresetMaterializing&) = delete;
    ScopedPresetMaterializing& operator=(const ScopedPresetMaterializing&) = delete;
private:
    bool mPrior;
};
```

```cpp
static void alDiopterAbortPresetSelection(U32 prior_preset)
{
    LL_WARNS() << "diopter: Custom bank not persisted; reverting preset selection" << LL_ENDL;

    // (a) revert under the guard. The listener's own first-statement guard
    //     check (piece 2b) is what this relies on to prevent re-entry.
    {
        LLPipeline::ScopedPresetMaterializing guard;
        sDiopterPresetExit = AL_EXIT_USER_SELECTED_CUSTOM;
        gSavedSettings.setU32("CineDiopterPreset", prior_preset);
    }

    // (b) materialization is skipped by the caller's early return.

    // (c) invalidate in memory AND on disk -- see below.
    alDiopterInvalidateBank();

    // (d) tell the user.
    LLNotificationsUtil::add("DiopterPresetBankWriteFailed");
}
```

**On (a):** the revert must run under the same guard the materializer uses, and the listener must check that guard as its first statement (piece 2b) — otherwise the revert re-enters with `id == prior` and, if `prior != 0`, retries the snapshot: an unbounded loop on a full or read-only disk. Rev 4 specified the guard but not the check, which left the loop open; both halves are required.

**On (c) — invalidation must reach disk, not just memory.** Rev 4 set an in-memory `mValid = false`, which is erased by the next restart: the loader would then read the stale file, believe it, and overwrite 44 live settings on the next return to Custom. Worse, rev 4 said the flag "clears on a successful load" — which is exactly backwards, since a successful load of a *stale* file is the failure being guarded against.

```cpp
void alDiopterInvalidateBank()
{
    sBankValid = false;                                  // in-memory, this session
    const std::string path = alDiopterBankPath();
    if (LLFile::isfile(path))
    {
        // same temp->rename atomicity as the writer; a rename is atomic on
        // both NTFS and POSIX, so there is no window where neither exists.
        // LLFile::rename returns an error code (llfile.cpp:894-899) and the
        // established writer CHECKS it (alcinelightrig.cpp:3284-3301) -- so
        // do we [Codex delta-5 #5]: on the same failing disk the rename
        // itself can fail, and ignoring that would leave the original bank
        // loadable after restart.
        if (LLFile::rename(path, path + ".stale") != 0)
        {
            LL_WARNS() << "diopter: could not mark Custom bank stale on disk; "
                          "refusing restores for this session" << LL_ENDL;
            sBankRefusedThisSession = true;              // belt over the braces
            LLNotificationsUtil::add("DiopterPresetBankInvalidateFailed");
        }
    }
}
```

The loader refuses a bank whenever `<path>.stale` exists, `<path>` is absent, or either in-memory flag (`!sBankValid`, `sBankRefusedThisSession`) is set, and **that refusal is cleared only by a *new successful snapshot***, which writes a fresh `<path>` and removes the `.stale` file as its last step — never by a load. The refusal survives restarts **whenever the `.stale` marker reached disk**; the one exception is the residual window below.

*Residual risk, stated honestly:* if the rename **also** fails (same dying disk) and the session then crashes before any successful snapshot, the next session's loader sees an unmarked stale `<path>` and would trust it. This is the one window the design cannot close from user space — it requires two distinct disk failures plus a crash between them — and it is why the notification in (d) fires on the rename failure too, so the director knows the bank is suspect before any restart.

The behavioural contract this buys: once the bank is untrustworthy, `alDiopterRestoreCustomBank()` **refuses and leaves current settings untouched** — this session unconditionally, and every later one provided the `.stale` marker landed (the residual window above being the sole exception) — until the user does something that produces a known-good bank. Leaving the director's current sliders alone is always recoverable; overwriting 44 settings from a stale bank is not.

**On (d):** a disk-full or permissions failure is exactly the situation where silence costs the most, because the director will keep working and keep assuming their Custom look is protected. The notification should say what happened and what is still safe: *"Couldn't save your Custom diopter settings, so the preset wasn't applied. Your current settings are unchanged."*

**Test obligations** (part of the four-transition unit test in Phase 1). Simulate snapshot failure and assert:

1. `CineDiopterPreset` equals its prior value;
2. no owned setting changed;
3. the bank is invalid **in memory and on disk** — `<path>.stale` exists, `<path>` does not;
4. a restore in the same session is a no-op;
5. **a restore after simulated restart is still a no-op** (this is the assertion rev 4 could not make);
6. the listener was entered exactly once — instrument a counter, since the re-entry guard is the thing under test;
7. a subsequent *successful* snapshot clears `.stale` and re-enables restore.

Assertion 5 is the one that distinguishes this design from rev 4's; assertion 6 is the one rev 4 stated but could not satisfy.

**Piece 2d — schema versioning.** Store `{"version": 1, "fields": {…}}`. On load: **seed any field missing from the bank with the current live setting** before materializing (so a bank written by an older build cannot leave newly added fields un-restored), **preserve unknown fields** on write-back (so a downgrade does not silently drop what a newer build added), and **upgrade atomically** — read, transform in memory, write through the same temp→rename path. Never migrate in place.

**Piece 2e — scope and concurrency.** `LL_PATH_USER_SETTINGS` is **global, not per-account** (`llstartup.cpp:1129-1155` switches to the per-account directory only for `LL_PATH_PER_SL_ACCOUNT`), which correctly matches `gSavedSettings`' own scope — the settings the bank shadows are global, so the bank must be too. Consequence to state plainly: **with two viewer instances running, the last writer wins**, exactly as with `settings.xml` itself. That is acceptable because it matches the behaviour of the data being shadowed; do not build locking for it.

**Piece 3 — the one-time startup sync, and its ordering constraint.**

Connecting a `boost::signals2` slot does not invoke it, and the listeners are installed at `llappviewer.cpp:961` — *after* settings load (`initConfiguration()` at `:819-821`, then `loadSettingsFromDirectory("User")` at `:2819-2820`). So a director who logs out with "Vintage Swirl" selected logs back in to sliders that were never synced to it — the exact lying state the fix exists to remove, now surviving restarts.

**Immediately after installing each listener, read the current preset and, if non-zero, materialize it once.** The ordering is strict:

```
1. settings load                    (llappviewer.cpp:819-821, then :2819-2820)
2. LOAD THE BANK FROM DISK          <-- must precede step 4
3. register listeners               (llviewerfloaterreg.cpp, via llappviewer.cpp:961)
4. startup sync: if preset != 0, snapshot the bank IF ABSENT, then materialize
```

Step 2 before step 4 is the load-bearing part: the startup sync's "snapshot if absent" test must be able to see an existing bank, or the first run after upgrade would snapshot the *already-materialized preset values* as if they were the user's Custom look — silently destroying it on the very upgrade meant to protect it.

Do all of this for **both** tools; the kaleido path is identical with 49 fields and `materializeKaleidoPreset`.

**Net effect.** Rendering is bit-for-bit unchanged — `alDiopterResolveLook` reads those same settings and then overwrites them from the preset switch. What changes is that the sliders display what is actually on screen, on every login, without ever destroying a Custom look.

One more, cheap: **label the escape hatch honestly.** The Custom item currently reads "Custom (use sliders)". Under the new behaviour every preset uses the sliders, so rename it to **"Custom (start from scratch)"** and put a one-line status under the combo: `Vintage Swirl — edit any control to make it yours`.

**4. Explain the greys.** Every Tier B and Tier C control gets a rewritten tooltip when it goes inert, using the fork's `setToolTipIfChanged` helper (currently duplicated at `llfloaterdirector.cpp:584` and `alfloatervirtualcam.cpp:29` — **a third copy is the point to extract it into a shared header**):

- Base Distance, disabled: *"Overridden — Base Focus is tracking the camera focus subject"*
- Ghost Spacing, disabled: *"Raise Ghost Copies above 0 to use this"*
- Halo Rings, disabled: *"Needs a warp: raise Ring Fold, Twist, Lobe Amount, or Source Zoom, or turn on Faceted Fold"*
- Size, disabled under On-Lens: *"On-Lens glass fills the frame — Size is not used. Pulse: Size and Stutter Size still breathe the falloff."* (This is the accurate wording; §2.1 documents why all three consumption paths are cut, not just the mask.)
- Wave Amplitude, disabled under Full Frame / On-Lens: *"Wave undulates the glass edge — Full Frame and On-Lens have no edge to undulate. Pick a framed shape to use it."*
- Wobble Frequency, disabled: *"Sets how tight the edge waves are. Raise Edge Wobble, or set Motion to Wave, to use it."*

The Halo Rings tooltip in particular encodes a five-term shader condition (`pipeline.cpp:14350`) that is currently undiscoverable by any means short of reading the source.

**Also worth building, lower priority: a filter box.** `LLFloaterSettingsDebug` has a working live filter (`llfloatersettingsdebug.cpp:71, 604-647`) — an `LLFilterEditor`, `+`-separated AND tokens, lowercase substring match. At 181 controls a "find the slider called *bokeh*" field is genuinely useful. But with gating in place the need drops sharply, so this is polish, not a fix.

**And a cheap legibility win:** the 180 reset buttons already know each control's default. Swap the button image when the value is non-default (`control->isDefault()`), so the director can see at a glance which of the visible controls they have touched — a free "what have I changed" map with no new widgets.

---

## 7. Phased implementation plan

**Re-phased in rev 2.** Rev 1's Phase 1 bundled the wheel with the preset fix and called the result ship-alone. It was not: `wheel_adjust` without scroll resolution makes ordinary scrolling mutate settings (§4.2), and materialize-on-selection without shadow storage destroys the Custom bank (§6.3). The wheel now moves to the phase that resolves scrolling; the preset work is split so only the safe half goes first.

### Phase 1 — Stop the lying — **S**

Highest value per line in the document, and genuinely ship-alone. No hiding, no wheel, no gating. The two cosmetic layout edits at the bottom of the table (slider widening, default size) are low-risk find-and-replace work with no behavioural coupling to the preset machinery — they ride along here because they touch the same file, and can be dropped to Phase 3 without affecting anything else.

| Change | File |
|---|---|
| Custom-bank shadow storage: versioned LLSD in `LL_PATH_USER_SETTINGS`, temp→flush→close→rename (§6.3 pieces 1, 2c–2e) | `indra/newview/` new `aldiopterpresetbank.h/.cpp` |
| **Transition-reason flag** `ALPresetExit`, set by the owned-setting (auto-Custom) listener — which is also the path every `Diopter.ResetControl` click travels, so resets need no separate wiring (§6.3 piece 2) | `llviewerfloaterreg.cpp`, `pipeline.h` |
| Selection listener branching on reason, with the **write-back revert failure path** — post-signal semantics mean a bare `return` cannot abort (§6.3 pieces 2b, 2f) | `llviewerfloaterreg.cpp` (~70 lines) |
| `ScopedPresetMaterializing` RAII guard (save/restore, nesting-safe) + guard check as the selection listener s first statement (§6.3 pieces 2b, 2f) | `pipeline.h`, `llviewerfloaterreg.cpp` |
| Bank load → listener register → startup sync, in that order (§6.3 piece 3) | `llviewerfloaterreg.cpp`, `llappviewer.cpp` |
| Bank round-trip + state-machine unit test: all four transitions incl. AUTO-after-edit, **plus the 7 snapshot-failure assertions** (§6.3 piece 2f), including restore-still-refused-after-restart and the entered-exactly-once re-entry counter | `tests/aldiopterpresetbank_test.cpp`, `CMakeLists.txt` |
| Widen sliders 360→440, reset button to x=452 | `floater_ultimate_diopter.xml` (find-and-replace) |
| Default floater size 520×640 → 560×720 | `floater_ultimate_diopter.xml` (2 attributes) |

**Delivers: sliders that stop lying, on every login, without destroying a Custom look — plus 22% more slider travel.** No behaviour a director has today is removed. Ship-alone-safe now that the state machine distinguishes user-selected Custom from auto-Custom; the four-transition unit test is what makes that claim checkable rather than asserted.

### Phase 1b — Combo wheel opt-out — **S**

Small, independent, and a prerequisite for Phase 3. Ship whenever.

| Change | File |
|---|---|
| `wheel_select` opt-out param on `LLComboBox` (§4.5, default true) | `indra/llui/llcombobox.h/.cpp` |
| `wheel_select="false"` on **all 25 combos** (named inventory, §4.5) | `floater_ultimate_diopter.xml` |
| Validator: assert every `<combo_box>` in the floater carries the opt-out | `tests/aldiopterrelevance_test.cpp` |
| Coarse wheel modifier (Ctrl/Alt = ×10) | `indra/llui/llslider.cpp` (~6 lines, §4.3) |

The coarse modifier is safe to land early: with no modifier held, behaviour is identical to today.

### Phase 2 — The class and the relevance table — **M**

| Change | File |
|---|---|
| New `ALFloaterUltimateDiopter` (postBuild, draw, refreshRelevance, ~20 settings listeners) | **new** `alfloaterultimatediopter.h` / `.cpp` |
| Registration one-liner | `llviewerfloaterreg.cpp:679` |
| `sRelevance` OR-of-AND table — **transcribe the 181 rows / 194 clauses from the companion artifact `diopter_relevance_table.md`**, each owning its reset button (nullable for `diopter_enabled` only), as an **LLUI-free model unit** (§2.3) | **new** `aldiopterrelevance.h` / `.cpp` |
| Coverage bitmap + `alAssertCoverage` (predicate-flip excluding `PRED_NONE`, clause-coverage, case-count guard) — code in §2.3 | `tests/aldiopterrelevance_test.cpp` |
| Oracle extractor + generated oracle + cross-product test target (**1580 cases across 9 sweeps**, plus the predicate-flip and clause-coverage meta-rules, §2.3) | `tests/aldiopterrelevance_oracle.py`, `tests/aldiopterrelevance_oracle.llsd`, `tests/aldiopterrelevance_test.cpp`, `CMakeLists.txt` |
| Factor out `alDiopterWarpArmed()`, `alDiopterAberrationArmed()`, and `alDiopterResolveBaseFocus()` returning provenance (§5.3) | `pipeline.cpp` / `pipeline.h` |
| Master-enable tier: whole floater dimmed with one affordance when `CineDiopterEnabled == 0` (§1.4 Tier 0) | `alfloaterultimatediopter.cpp` |
| Extract `setToolTipIfChanged` to a shared header | `llfloaterdirector.cpp`, `alfloatervirtualcam.cpp`, new header |
| Structural + branch-coverage validators (§2.3) | `alfloaterultimatediopter.cpp`, test target |
| Build wiring | `indra/newview/CMakeLists.txt` |

**Gate on disable-and-dim only in this phase** — no XML restructure, no hiding, so no focus-transfer or saved-rect exposure yet. Zero layout risk, and it independently delivers the "which controls matter right now" answer plus every explanatory tooltip. **The branch-coverage validator is not optional here** — it is what stops the 181-row table decaying into a new source of lies, and building it in the same phase as the table is far cheaper than retrofitting it.

### Phase 3 — The regroup, and the wheel — **L**

The big one, and the one that produces the felt simplification. **The wheel ships here because this is the phase that removes the visible scrollbar** (§4.2).

| Change | File |
|---|---|
| Restructure into header + same-rect overlay bands per §3.6; add `<text>` section headings; split Glass into Glass + Halo & Ghosts; move Center/Size/Angle/Placement to Shape; add a persistent Freeze footer | `floater_ultimate_diopter.xml` (substantial rewrite) |
| **Invariant W**: keep the scroll containers inert, raise `min_height` to the measured worst case (~600), migrate saved rects (§3.8b, §4.2) | `floater_ultimate_diopter.xml`, `alfloaterultimatediopter.cpp` |
| **Machine-check Invariant W**: max content height over every mode combination ≤ viewport at `min_height`, per tab | `tests/aldiopterrelevance_test.cpp` |
| Panel-visibility table (`ModeEntry` style) + `updateModePanel()` | `alfloaterultimatediopter.cpp` |
| Enable Tier A hiding | `alfloaterultimatediopter.cpp` |
| **Focus transfer before every hide/swap** (§3.8a) | `alfloaterultimatediopter.cpp` |
| **"Active controls only" toggle + `CineDiopterUIActiveOnly`** — ships with the first hiding (§6.2) | `alfloaterultimatediopter.cpp`, `settings.xml`, XML |
| `wheel_adjust="true"` on all 149 sliders | `floater_ultimate_diopter.xml` |

Do this phase in one commit with a full mode-by-mode visual pass — 24 kaleido modes × one band each is the bulk of the QA — plus an explicit wheel pass **at `min_height`**, which under Invariant W must show no scrollbar on any tab in any mode. If it does, the invariant is broken and the ancestor-gate fallback (§4.2) ships in the same commit.

### Phase 4 — Depth ergonomics — **M**

| Change | File |
|---|---|
| `ALToolFocusPick` (one-shot world pick, ~60 lines, modelled on `altoolghostplace.cpp`) | **new** `altoolfocuspick.h` / `.cpp` |
| Three eyedropper buttons + "Set from camera focus" + provenance readouts | `alfloaterultimatediopter.cpp`, `floater_ultimate_diopter.xml` |
| Per-control range combos + clamp-and-commit policy (§5.3e) | `alfloaterultimatediopter.cpp`, XML |
| "Show depth" toggle beside the depth sliders; `RenderFocusPointCrosshair` checkbox | `floater_ultimate_diopter.xml` |
| Build wiring | `CMakeLists.txt` |

Depends on Phase 2's shared `alDiopterResolveBaseFocus()` helper. Independent of Phase 3 — can ship before or after it.

### Phase 5 — Polish — **S**

| Change | File |
|---|---|
| Non-default indicator on the 180 reset buttons | `alfloaterultimatediopter.cpp` |
| `LLFilterEditor` filter box | `alfloaterultimatediopter.cpp`, XML |

(The muscle-memory toggle and the Custom-bank preservation both moved out of this phase — to Phase 3 and Phase 1 respectively — because shipping either later than the thing it protects would be a regression.)

### What stays pure XML vs needs the C++ class

**Pure XML** (no class required): `wheel_adjust`, tooltips, section headings, the overlay-panel skeleton, slider widths, floater size, the reset buttons, range-combo *markup*.

**Needs the C++ class**: relevance gating (all three tiers), dynamic tooltips, the eyedropper, live metre readouts, runtime slider range changes, the non-default indicator, the Active-controls-only toggle, and the filter box.

**Needs neither** (belongs in `llviewerfloaterreg.cpp`): materialize-preset-on-selection — it must be an app-lifetime listener like the two that already live there, so it works even when the floater is closed.

---

## 8. Risks and things not to break

Ordered by how expensive the mistake is to discover.

- **Never ship `wheel_adjust` while a vertical scrollbar can appear in any supported configuration** — including at `min_height`, which is where rev 2 left the hole. An ordinary scroll gesture then mutates render settings invisibly (§4.2, Invariant W). This is the single highest-consequence sequencing constraint in the plan.
- **Never ship materialize-on-selection without the Custom bank.** The 44/49 owned settings are `Persist = 1` and flush to disk at logout (`llappviewer.cpp:1989-1992`); without shadow storage, picking a preset permanently destroys a hand-built look (§6.3).
- **`preset == 0` is ambiguous, and the ambiguity is destructive.** The shipping edit flow materializes then sets the preset to 0 (`llviewerfloaterreg.cpp:599-615`), so a restore-on-zero listener erases the user's edit. Branch on the `ALPresetExit` reason flag, never on the value (§6.3 piece 2). Reset buttons travel this same path.
- **A failed bank write must abort materialization**, not proceed — use the temp→flush→close→rename pattern at `alcinelightrig.cpp:3284-3301` and check every step (§6.3 piece 2c).
- **Settings signals fire *post*-write, so a listener cannot veto.** `firePropertyChanged` runs after `mValues` is mutated (`llcontrol.cpp:225-263`). Aborting means an explicit write-back revert under a scoped suppression guard, plus bank invalidation and a notification — a bare `return` leaves an un-materialized preset displayed and a stale bank armed to overwrite live settings later (§6.3 piece 2f).
- **The revert must run under the suppression guard**, or it re-enters the listener and retries the failing snapshot forever on a full or read-only disk. Use RAII, not raw flag bracketing (§6.3 piece 2f).
- **A predicate no sweep flips is a coverage hole, not a passing test.** Six of eleven predicates were unexercised in rev 3 — including the entire aberration group and all seven ghost dependents. The meta-rule in §2.3 must be enforced by the test, not by review.
- **Load the bank before the startup sync.** Otherwise the first upgrade run snapshots already-materialized preset values as if they were the user's Custom look, destroying it on the very upgrade meant to protect it (§6.3 piece 3).
- **Connecting a signal does not invoke it.** Listeners install at `llappviewer.cpp:961`, after settings load (`:819-821`, then `:2819-2820`), so a saved non-Custom preset never syncs without an explicit one-time startup materialize (§6.3 piece 3).
- **The bank is global, not per-account** (`LL_PATH_USER_SETTINGS`; `llstartup.cpp:1129-1155`), matching `gSavedSettings`' scope. Two concurrent instances mean last-writer-wins, exactly as for `settings.xml`. Do not add locking; do state it.
- **Combos change selection *and commit* on wheel** (`llcombobox.cpp:958-997`). With overlays this replaces the controls under the pointer mid-gesture; with materialize-on-selection it writes 44/49 persisted settings. The scoped opt-out (§4.5) is required, not optional.
- **`setVisible`/`setEnabled` do not release keyboard focus** (`llview.cpp:477-485`, `634-648`), and key dispatch keeps targeting the stored element (`llviewerwindow.cpp:3277-3359`). Transfer focus out before hiding or swapping any subtree that contains it (§3.8a).
- **Reset buttons are part of the gating model, not chrome.** `Diopter.ResetControl` writes immediately (`llviewerfloaterreg.cpp:538-556`) and can trigger materialization and the Custom transition. Every relevance row owns its sibling button's visibility, enablement and tooltip (§2.3).
- **Removing scroll containers collides with `min_height=420` + saved rects** (`llfloater.cpp:976-1007`) — existing users get a clipped, unscrollable floater. Prefer keeping the containers and raising the true minimum (§3.8b).
- **`setMinValue`/`setMaxValue` do not re-clamp the value**, leaving the thumb off-track until the next edit (`llslider.cpp:125-140`). Clamp and commit explicitly on every range change (§5.3e).
- **Range tables must be per-control.** A shared ladder would offer values the renderer clamps away — Depth Feather is capped at 32 in `pipeline.cpp:14515-14516`, not in the XML (§5.3e).
- **Do not add `handleScrollWheel` to `LLSliderCtrl`.** It makes the label band swallow the wheel and renders scroll containers unscrollable (§4.2).
- **Do not flip `wheel_adjust` globally.** Every scroll container in the viewer regresses (§4.4).
- **Any programmatic write to a preset-owned setting trips the app-lifetime auto-Custom listener** (`llviewerfloaterreg.cpp:589`, `:645`). Hold `LLPipeline::sDiopterPresetMaterializing`, or do not write.
- **`sRelevance` must not drift from the shaders.** Share the armed-state helpers rather than duplicating the expressions, and ship the branch-coverage validator (§2.3) — the OR-consumed and compound-guarded rows are exactly the ones a human re-derives incorrectly, as both rev 1 and Codex's sampling demonstrated.
- **`mIntersection` is a trap** — zero on terrain hits. Always use `mPosGlobal` (§5.2).
- **`pickImmediate` silently drops its `pick_unselectable` argument** (`llviewerwindow.cpp:4904` passes a literal `false` into that ctor slot). Do not rely on it.
- **`LLSlider::setValue` snaps to `increment`** unless `precision_override` is passed, which the wheel path never does (§4.3).
- **Size is fully inert under On-Lens** — all three consumption paths, not just the mask, because the CPU discards `eff_size` and recomputes the radius (`pipeline.cpp:14289-14296`). Every Size clause needs `PlacementMode == framed`; rev 2 got this wrong in the matrix while getting it right in the tooltip (§2.1).
- **§2.4 #7 and #8 were adjudicated INTENDED** — no renderer change. Gate and explain them (§6.4 tooltips); do not file them as bugs and do not "fix" the Wobble Frequency label, which is correct for a spatial-density parameter.
- **The Freeze pair is a capture control, not a Main control.** Moving it to a persistent footer keeps it reachable from every tab — check that no existing documentation points at the Main tab for it.
