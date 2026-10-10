# Adversarial review WP1 R3 (HEAD bd3cd75f4d1)

Reviewer: Opus, independent context, 2026-10-10.
Scope:
- the R2 fixes (`git diff 1af3670dc8b bd3cd75f4d1`);
- the harness edge case;
- new defects in the R2 diff.

**VERDICT: FIX-NEEDED.** 1 BLOCKER, 0 MAJOR, 9 MINOR. Every other R2 BLOCKER/MAJOR, plus B10 and M14, is genuinely fixed. M6 remains partial because the tests cannot see the BLOCKER.

**Evidence.**
- I ran a TUT replica: the real `tests/alkitmath_test.cpp` against a tut shim, built with MSVC `/O2 /fp:precise`. All 18 tests pass at HEAD, including test<10>.
- Scratch probes against the repo `alkitmath.h` covered:
  - 14M pitch round trips, with |OffsetZ|/r0 from 0 to 2.5 and the current base offset by up to ±60°;
  - 61k unreachable pointer pitches.
- I also applied the fix below to a scratch copy of the header:
  - orchestrator harness: **PASS, 0 failing**;
  - TUT replica: 18/18;
  - both probes: 0 failures.
- No repo code was edited.

---

## R2 findings: verification

| R2 | Status | Evidence at HEAD |
|---|---|---|
| NB1 pitch inverse | **NOT FIXED. Narrowed, but still a blocker.** | `alkitmath.h:64-97`. Only the two principal roots remain and the ±360 alias is gone. But the filter is `cos(q)>0` (`:86`), which admits roots with 85<\|q\|<90, outside `pDisp`'s domain. See **RB1**. Codex also departed from NB1_FIX_RECIPE (\|q\|≤85), and the recipe itself is wrong too (see RB1). |
| NB2 short pages bottom-aligned | FIXED | `alkitpage.cpp:662-664`: `doc_h=max(natural,viewport)`, then the grid is placed at `doc_h-PAD_T-gridH`. When the content overflows, this reduces to `PAD_B`, so it is correct in both regimes. |
| B10 (R1 partial) | FIXED | Same lines. The order is grid reshape, then doc reshape, then top-anchored origin. |
| NM1 TUT test<10> red | FIXED | `alkitmath_test.cpp:126`: same-depth pairs (−41→−70, 126→148). `:160-173`: the cross-side pointer proves the captured side is kept, with x/y matched. The replica passes. |
| NM2 history baseline | FIXED (one residual, m1) | `alkitrow.cpp:125-136`: the hook connects before `resolveBinding()`, so it precedes the `ALKitWidgetBinding` commit slot. `refreshValue` writes only `mShownValue` (`:358-369`). `pollLiveValue`/`bindingChanged` adopt the baseline only when `!alIsBeingEdited` (`:371-377`, `:389`). Toggles: the flip handler now touches only display state, so the hook sees old≠new. |
| NM3 v1 setVisible probe | FIXED | `alkitrow.cpp:331-337` restores `mControl->getVisible()` (link/vec exempt). The XML-seeded `mCodeHidden` is gone, so a control hidden in XML and later shown reappears. The card probe (`alkitcard.cpp:~748-758`) iterates every row, hidden or not. |
| NM4 bar overlap | FIXED (collapse caveat, m3) | `alkitpage.cpp:255,623-630`. The master box starts at the left-cluster end + COL_GAP, and search is right-aligned. Each one is hidden only if it would collide. Rig: left end 378 → master 384-554; at page width 760 (1160 floater) search is at 574, so 0 overlap. |
| NM5 per-row 10 Hz timers | FIXED | The `doPeriodically` per row is gone. One `ALLiveReadoutClock{10}` (`alkitpage.h:38`) is polled in `ALKitPage::draw` (`alkitpage.cpp:789-793`). It touches only visible cards and effectively visible rows, and does diff-guarded writes (the aluirefresh pattern). A hidden page does nothing. |
| NM6 menu double-delete | FIXED | `alkitcard.cpp:97-100,280,300`, using `LLHandle<LLContextMenu>` (`llmenugl.h:736` `getDerivedHandle`). At shutdown `cleanup_menus` kills the handle, so the destructor skips it. A normal close calls `die()`, which is an LLMortician deferred delete; `~LLMortician` removes the menu from the graveyard if `gMenuHolder` deletes it first, so there is no double free. |
| M6 (R1 partial) | **PARTIAL** | test<4> (`:45-56`) is dense, but it passes `current = pitch`, so root selection is never exercised and RB1 is invisible. See the test requirements under RB1. |
| M14 (R1 partial) | FIXED | As NM6. |
| R2 m1 Who chip | Deferred (OK) | Code still reveals the hidden combo (`alkitpage.cpp:350-355`). The new WP2 acceptance line (spec :1637) owns it, but that line is uncommitted (working tree only). |
| m2 legacy summary | FIXED (partial) | It is in the bar (`alkitpage.cpp:211-222`). §3.5 asks for an "ellipsized custom draw"; it is a plain 160 px LLTextBox, so long summaries clip. MINOR. |
| m3 card images | FIXED | `alkitcard.cpp:77-78,718-721`. |
| m4 option overlays | FIXED | `alkitcard.h:121` calls `setMouseOpaque(false)` after the base ctor, so it overrides the XML default. |
| m5 compass letters | FIXED (one mismatch, m5 below) | `alcompassdial.cpp:149,364`. |
| m6 `_launch` rename | FIXED in the spec working tree only | `DIRECTOR_KIT_P0_SPEC.md` diff is uncommitted. Commit it with WP1. |
| m7 adoptPage prefix | FIXED | `alscrollfit.cpp` uses `kit_page->scroller()` / `legacyHost()`. |
| m8 Side yaw near axis | Changed, not improved | `alkitmath.h:219-226`: the `atan2` uses the captured depth sign, which is correct (keep-side 0 fails). `sqrt(1−c²)` is exactly as ill-conditioned as `acos` near c=±1 (harness max 0.47°, inside the 0.5° axis tolerance). This is inherent in the projection, so it is acceptable. The ledger's "precision" claim is cosmetic. |
| m9 per-tick copies | **R2 finding withdrawn** | Codex is right. `Setup`/`Transforms`/`Globals` (`alcinelightrigmodel.h:140-169`) are POD with no strings. |
| m10 Side tooltip | FIXED | `allightrigmap.cpp:592-597`, `setToolTipIfChanged`. |
| m11 shell scrollbar | FIXED | `alkitmath.h:239-244`. 1160 gives 2 columns and track 132; 850 gives track 149. |

---

## BLOCKER

**RB1. `baseFromDisplayPitch` is wrong for |OffsetZ| ≥ r0. Pitch drags jump by up to 57° when a light's per-light Z is a little above the rig radius, and they snap the wrong way when the pointer leaves the reachable cone.**

**Location.** `alkitmath.h:64-97`. It is reached on every non-Top drag through `finishDrag` (`:169-175`). Note that `current` is the immutable `base_pitch0`, so during a long drag the target is routinely far from `current`.

**Mechanism.**
- (a) **1 < |h|/r ≲ 1.2:** the subject lies just outside the orbit circle. Both roots are genuine, but one has 85<|q|<90. `cos>0` keeps it, nearest-to-current picks it, and the clamp to ±85 then lands far from the pointer.
  - The orchestrator's single failure is the limiting case |h|=r: the second root is the degenerate q=±90 (the light coincides with the subject), and it survives the filter on float noise.
  - Harness case: r0=1, h=−1, orbit=12, base 72.14 / current 75.14 → 73.00 (q=85). That displays −2.50° against a target of −2.93°.
- (b) **|h| > r, pointer outside the cone:** only displayed pitches with |t| ≥ acos(r/|h|), on h's side, are reachable. Outside that cone `rhs` saturates to ±1 and `q=t±90` is chosen. That root is not the tangent, so the light moves *away* from the pointer.

**Measured (HEAD).**

| Sweep | Cases | Failures | Worst |
|---|---|---|---|
| Reachable round trips, \|h\|/r ∈ {±1, ±1.0001, ±1.01, ±1.05, ±1.1} | 13.95M | 431,646 | 57.4° display error |
| Unreachable pointer pitches | 61,182 | 55,716 not at the nearest reachable pitch | 51.1° excess |

- Per ratio at r0=1: 1.0001 → 9.5%; 1.05 → 6.7%; 1.1 → 1.6%.

**Reachability in practice.** Rig radius defaults to 1.5 m (min 0.5, step 0.25), and per-light `CineLightRig<Role>OffsetZ` is a free slider. A key raised about 1.6 m on the default rig is in band (a). A low fill under a small rig (h=−1.5, r=0.6) is in (b) for almost every upward drag. Under the severity gate this breaks the map's core promise (the drag follows the pointer), so it is a BLOCKER.

**The harness is right; this is not a tolerance issue.** At |h|=r the base pitch is *not* multi-valued inside ±85°: the displayed pitch is (q∓90)/2, which is injective, and the second root is the degenerate pole outside the domain. For |h| ≥ r the harness already checks in display space (`alkitmath_harness.cpp:50`), which is the correct test, and the returned base misses the displayed target by 0.43°. Do not loosen it.

**NB1_FIX_RECIPE.md is also wrong. Do not apply "keep |q|≤85".**
- It returns `current` (that is, base0, a snap-back) for every saturating pointer: 2,244/2,244 cases, e.g. dragging past 85°.
- It also fails 264 cases at |h|=0.999r, where asin noise pushes the true root just past ±85.
- Update the recipe to the fix below.

**Exact fix.** Replace the body of `baseFromDisplayPitch`. Verified: 0 failures on all of the sweeps above, plus the full harness and the TUT replica.
```cpp
inline float baseFromDisplayPitch(float target, float orbit, float offset_z,
                                  float radius, float current)
{
    if (radius <= 1e-6f) return clamp(current, -85.f - orbit, 85.f - orbit);
    // |OffsetZ| > r: only a cone of displayed pitches is reachable; clamp the pointer to its edge.
    if (std::fabs(offset_z) > radius)
    {
        const float lim = std::acos(radius / std::fabs(offset_z)) * RAD_TO_DEG;
        target = offset_z < 0.f ? std::min(target, -lim) : std::max(target, lim);
    }
    const float t = target * DEG_TO_RAD;
    const float asin_rhs = std::asin(clamp(-offset_z * std::cos(t) / radius, -1.f, 1.f));
    auto canonical = [](float d) { const float w = wrap180(d); return w <= -180.f ? 180.f : w; };
    const float candidates[] = { canonical((t + asin_rhs) * RAD_TO_DEG),
                                 canonical((t + PI - asin_rhs) * RAD_TO_DEG) };
    const float current_q = canonical(current + orbit);
    float best_q = clamp(current_q, -85.f, 85.f), best_err = 1e9f, best_dist = 1e9f;
    for (float q : candidates)
    {
        if (std::cos(q * DEG_TO_RAD) <= 0.f) continue;              // opposite ray
        const float qc = clamp(q, -85.f, 85.f);                      // pDisp's domain
        const float err = std::fabs(pDisp(qc, 0.f, offset_z, radius) - target);
        const float dist = angularDistance(qc, current_q);
        if (err < best_err - 1e-3f || (std::fabs(err - best_err) <= 1e-3f && dist < best_dist))
        { best_q = qc; best_err = err; best_dist = dist; }
    }
    return clamp(best_q - orbit, -85.f - orbit, 85.f - orbit);
}
```
In short: clamp each root into the domain *before* scoring, score by the displayed error of the clamped root, and break ties by distance to `current`.

**Required tests (closes M6).** Extend test<4>:
- `current = pitch + {−60, −30, −10, 0, 10, 30, 60}`, within ±85 of the orbit;
- |h|/r ∈ {±1, ±1.0001, ±1.01, ±1.05, ±1.1, ±1.3, ±2.5};
- assert in display space when |h| ≥ r.

Add test<19> for saturation:
- for |h|<r, a target beyond the reachable range returns the nearer ±85 bound, not `current`;
- for |h|>r, a target outside the cone returns the cone edge.

**Harness change (orchestrator).** Keep the display-space check. Add:
- the near-ratio heights `h = ±r·{1.0001, 1.01, 1.05, 1.1}`;
- `cur = b ± {10, 30, 60}`;
- an unreachable-target check comparing against the brute-force best reachable display.

With `cur` only ±3 and h ∈ {−1, −0.3, 0, 0.4, 1.5}, the current harness hits RB1 only at the single exact r0=1, h=−1 point, which is why it showed 1 case.

## MAJOR

None.

## MINOR

1. **History baseline freezes on keyboard focus, not just while editing.**
   - `alIsBeingEdited` (`aluirefresh.h:124`) is true whenever the control keeps keyboard focus. Clicked buttons, checkboxes, sliders and combos keep it until focus moves.
   - An external write while the control is focused is therefore never adopted (`alkitrow.cpp:376,389`). An example is WP2 Ctrl+Z replay, which does not move focus. The next commit then records a stale `old`.
   - It is inert in WP1 (history is stubbed). In WP2, either have replay/restore reseed `mLastValue` on the rows it writes, or freeze only on mouse capture or on a focused text-entry descendant.
2. **Widget-bound amber no longer refreshes on a passive `setValue`.** The old per-row poll called `bindingChanged()`, but `pollLiveValue` only refreshes text. This is consistent with spec §3.3 ("binding signals, never per frame"), but `ALKitWidgetBinding` only signals on commit. Note it for WP4: the Easy feather amber is stale after preset loads.
3. **The page bar hides search instead of shrinking it.** On Rig, search disappears at page width < 746: floater 916-923 px with the rail-label hysteresis, or any narrower custom main. §3.5 has no collapse rule. Prefer shrinking to a minimum width (e.g. 120) before hiding, and record the rule in §3.5.
4. **The legacy summary is not ellipsized** (§3.5 "ellipsized custom draw"). `alkitpage.cpp:213-221` uses a plain 160 px textbox.
5. **Compass draw vs helper.**
   - The draw uses `radius + 4.f` unscaled (`alcompassdial.cpp:364`), while `dialGeometry` and test<15> use `+4s`. For cells smaller than 48 px (s<1) the letters overflow the cell (h=28 puts them 0.7 px from the edge), and the test asserts the helper, not the widget.
   - Use `4.f * scale` in the draw, or have the generator emit `show_cardinals=false` for small dials.
6. **The Who chip still runs a `doPeriodically(.5)` per page** (`alkitpage.cpp:365-374`). It runs while hidden: the `isWidgetOnScreen` early-out still costs one timer per Who page. Fold it into the page `mReadoutClock` in draw for consistency with NM5.
7. **The page bar's left cluster starts at x=0, not PAGE_PAD_LR** (`alkitpage.cpp:194`), while search is right-padded by 16. This is cosmetic, but asymmetric.
8. **WP1_STATUS R2 ledger overclaims.** "A dense … sweep requires <0.01° error" is true only with `current = pitch`, so it does not exercise branch selection (RB1). "Side inversion uses `atan2` rather than ill-conditioned `acos`" overstates m8.
9. **The spec edits (`_launch`, the kit compass decision, Side `atan2`, the WP2 Who acceptance) are uncommitted** in the working tree. They are not in bd3cd75f4d1, so commit them with the WP1 fix.

---

## Checklist

- **Shared poll lifetime:** OK. The clock is a page member; rows come from `mCards`, which are children of the page grid. The poll runs only in draw, so it costs nothing when hidden.
- **Menu handle:** OK. The deferred delete is safe against the `gMenuHolder` teardown order.
- **History baseline ordering:** OK. The hook precedes the binding slot, and display is split from baseline (residual: m1).
- **Visibility logic:** OK (§3.3 restored; the XML seed is removed).
- **Bar layout maths:** OK at 850/1160 for Rig, with no overlap (residual: m3).
- **Maths:** the Side keep-side, zero-motion, out-and-back and effYaw checks are clean. **The pitch inverse is still wrong (RB1).**
- **TUT:** green as committed, but blind to RB1.
