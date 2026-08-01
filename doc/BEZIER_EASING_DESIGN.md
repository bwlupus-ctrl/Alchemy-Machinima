# Bézier / Easing Curve System — implementation design (Fable, 2026-07-31)

User-selectable transition curves for the machinima camera. Default = today's `cc_smootherstep` feel (enum 0), byte-identical until a user picks a curve. Applies to: camera cut ease (pos+rot+FOV, one weight), the auto-reframe settle, and the global-slowdown/bullet-time pulse (separate selector). Raise max transition 2→10 s.

## Plug-in points (verified)
- Cut-ease weight: `llcinematiccamera.cpp:2522` `ease_weight = cc_smootherstep(u)` (u = `switcher.cutEaseElapsedSeconds()/mEaseDuration`, unscaled wall clock); one weight blends pos (2523), rot nlerp (2525), FOV (2597).
- Ease arm/latch: `llcinematiccamera.cpp:2285-2311` (latches mEaseDuration, mEaseFromPos/Rot/Fov per cut serial).
- Settle mode 1 (finish-with-cut): `llcinematiccamera.cpp:2141-2148` `w = cc_smootherstep(u)` (same cut timer).
- Settle mode 2 (post-cut): `:2153-2158` `w = cc_smootherstep(elapsed/duration)` (elapsed = presentation mPhase delta; duration latched 2120-2121, clamp 0.05..5).
- Settle termination: `:2160-2163` `if (w>=1) mAutoFrameSettleActive=false` — **BUG under overshoot curves (fix below)**.
- Global slowdown pulse: `aldirectorswitcher.cpp:544-555` symmetric `switcher_smootherstep(u*2)/((u-0.5)*2)` halves on mCutEaseTimer.
- Ease-seconds clamp (single authoritative site): `aldirectorswitcher.cpp:665-669` `llclamp(getF32("DirectorSwitcherEaseSec"),0,2.0)`; llcinematiccamera consumes via `switcher.cutEaseSeconds()`.
- UI maxes: `panel_director_switcher.xml:435` max_val 2.0; `panel_cinecam_frame.xml:171` max_val 5.
- Scheduler: `aldirectorswitchermodel.h:21-23` interval clamp 0.1..3600 default 8.0; boundary → applySlot(force) → restoreEaseWorldTime + reset mCutEaseTimer + bump serial.
- `cc_smootherstep` has ~28 OTHER internal uses in shot patterns (397-1527) — OUT OF SCOPE, touch ONLY the sites above.

## 1. Evaluator — new header-only `indra/newview/alcameracurve.h`, namespace `ALCameraCurve` (shared by llcinematiccamera, aldirectorswitcher, and the UI so preview == deployed math)
CSS/WebKit UnitBezier: fixed P0=(0,0) P3=(1,1); solve x(t)=u, return y(t). Horner form:
```
bezierComponent(t,p1,p2): c=3p1; b=3(p2-p1)-c; a=1-c-b; return ((a*t+b)*t+c)*t
bezierComponentDeriv(t,p1,p2): c=3p1; b=3(p2-p1)-c; a=1-c-b; return (3a*t+2b)*t+c
evalBezier(u,x1,y1,x2,y2):
  if !finite(u) return 1; u=clamp(u,0,1)
  x1=finite?clamp(x1,0,1):0.42; x2=finite?clamp(x2,0,1):0.58   // monotone-x contract
  y1=finite?clamp(y1,-2,3):0;  y2=finite?clamp(y2,-2,3):1
  if u<=0 return 0; if u>=1 return 1                            // exact endpoints, no end-pop
  if (x1==y1 && x2==y2) return u                                // linear/identity fast path
  t=u; for i in 0..7: err=bezierComponent(t,x1,x2)-u; if |err|<1e-6 return clamp(bezierComponent(t,y1,y2),-1,2)
        d=bezierComponentDeriv(t,x1,x2); if |d|<1e-6 break; t=clamp(t-err/d,0,1)   // Newton x8
  lo=0,hi=1,t=u; for i in 0..19: x=bezierComponent(t,x1,x2); if |x-u|<1e-6 break; if x<u lo=t else hi=t; t=0.5(lo+hi)  // bisection x20
  return clamp(bezierComponent(t,y1,y2),-1,2)
```
Pure/stateless/fixed-work → deterministic. y allowed to overshoot [-1,2] (for "back"). Identity path exact.

## 2. Unified model — enum: 0..32 named CLOSED-FORM easings + 100 = Custom bezier (4 stored floats). Bezier eval used ONLY for CSS ids 2-5 and Custom; everything else = exact Penner closed-form (expo/circ/back/elastic/bounce are NOT single cubics).
Stable ids (persisted, append-only): 0 Smootherstep(default=today, calls cc_smootherstep VERBATIM), 1 Linear, 2 Ease, 3 EaseIn, 4 EaseOut, 5 EaseInOut (CSS via evalBezier), 6/7/8 Quad In/Out/InOut, 9/10/11 Cubic, 12/13/14 Quart, 15/16/17 Quint, 18/19/20 Expo, 21/22/23 Circ, 24/25/26 Back, 27/28/29 Elastic, 30/31/32 Bounce, 100 Custom. Unknown→0.
`eval(id,u,bx1,by1,bx2,by2)`: clamp u; every branch guarantees f(0)=0,f(1)=1; final clamp(result,-1,2). Penner forms: power u^n / 1-(1-u)^n / piecewise inOut; expo 2^(10u-10) w/ endpoint guards; circ 1-sqrt(1-u^2)/sqrt(1-(u-1)^2); back c1=1.70158,c3=c1+1; elastic c4=2π/3 (c5=2π/4.5 inOut); bounce n1=7.5625,d1=2.75. CSS points: ease(.25,.10,.25,1) in(.42,0,1,1) out(0,0,.58,1) inout(.42,0,.58,1). Also ship `presetBezier(id)` seed table (Ceaser/easings.net values) for the editor's "copy preset→custom" (not used to evaluate 6-32).

## 3. Settings — 3 targets × (1 enum S32 + 4 F32), Persist=1, default enum 0 (=today, bit-identical). Custom pts default CSS ease-in-out (.42,0,.58,1).
- `DirectorSwitcherEaseCurve` + `DirectorSwitcherEaseBezierX1/Y1/X2/Y2` (cut pose/FOV ease)
- `DirectorSwitcherFreezeCurve` + `DirectorSwitcherFreezeBezier{X1,Y1,X2,Y2}` (bullet-time pulse, per half-ramp)
- `CinematicAutoFrameSettleCurve` + `CinematicAutoFrameSettleBezier{X1,Y1,X2,Y2}` (post-cut settle)
Sanitize on read (x∈[0,1], y∈[-2,3], non-finite→default). **Latch per take** (like mCutEaseSeconds/mAutoFrameSettleDurationLatched): applySlot latches mCutEaseCurveId + mCutEaseBezier[4] (expose cutEaseCurve()/cutEaseBezier() by cutEaseSeconds() aldirectorswitcher.h:81); llcinematiccamera copies into mEaseCurveId/mEaseBezier[4] in arm block (2300-2307); startEaseWorldTime latches Freeze pair; settle latch block (2117-2123) latches Settle pair.

## 4. Apply — replace cc_smootherstep at the 3 sites (same curve pos+rot+FOV: YES, one weight). TWO REQUIRED FIXES:
- Cut ease 2522: `ease_weight = ALCameraCurve::eval(mEaseCurveId,u,mEaseBezier[0..3])`. Termination stays u-based (2515) — already correct.
- Settle mode 1 (2147): use the LATCHED CUT curve (rides the cut ease, must match the pose weight).
- Settle mode 2 (2157): `w = eval(mAutoFrameSettleCurveLatched, min(elapsed/duration,1), ...)`.
- **FIX 4.4 settle termination (2160-2163):** w>=1 ends at first crossing → back/elastic cross 1 mid-flight → snaps early. Replace with PROGRESS-based: mode2 deactivate when `elapsed>=duration`; mode1 when `!mEaseActive || u>=1`. (Identical for smootherstep.)
- **FIX 4.5 global slowdown (546-549):** replace both switcher_smootherstep with `llclamp(ALCameraCurve::eval(mFreezeCurveId, half_u, ...),0,1)` (half_u = today's u*2 / (u-0.5)*2). **Clamp to [0,1]** — overshoot would push speed negative/over-normal. Keep the symmetric two-half structure. Separate selector (different artistic object).
- Overshoot safety downstream: pos lerp bounded; rot nlerp extrapolates then normalizes (unit quat); FOV clamped getMin/MaxView (2604/2610); settle log-space distance clamped AUTO_FRAME_MIN/MAX. All safe with y∈[-1,2].
- Leave all other cc_smootherstep/cc_lerp alone; keep cc_smootherstep as the id-0 target; delete nothing.

## 5. Duration 2→10s
- `aldirectorswitcher.cpp:668` clamp 0..10 (const MAX_EASE_SECONDS=10). Single authoritative clamp.
- `panel_director_switcher.xml:435` max_val 10.0 (keep increment .05; tooltip "up to 10s"). `settings.xml` EaseSec comment 0..10.
- Settle: `llcinematiccamera.cpp:2121` clamp 0.05..10; `panel_cinecam_frame.xml:171` max_val 10; settings comment.
- Scheduler: a long ease outliving the shot is ALREADY possible (2s vs 0.1s interval); boundaries are pure presentation-time fns of (seed,interval,jitter), never consult the ease → NO determinism interaction; a long ease is just pre-empted (chains from cam->getOrigin/Quaternion/View at re-arm, C0 continuity). Ease-freeze + long ease → world-speed hard-restores at the boundary (pop) then re-ramps — ACCEPT for v1 (today's always-restore invariant; do NOT blend pulses across takes, do NOT auto-clamp ease to interval). UX: tooltip "ease longer than the auto-cut interval chains into the next cut; bullet-time restarts per cut — for continuous slow-mo use Temporal Capture."

## 6. UI (SL XUI) — v1 = dropdown + spinners + draw-only preview (drag editor = v2)
- v1 pragmatic: a `combo_box` (33 named + "Custom (bezier)", integer values, S32-combo binding like CinematicAutoFrameSettleMode panel_cinecam_frame.xml:157-161) + 4 spinners (X1/X2 0..1 step .01; Y1/Y2 -2..3 step .01) + reset button. Enable spinners only when id==100 (commit-signal pattern like ALCineCamFramePanel::updateSettleTimeEnabled). Flip combo→Custom when a spinner commits. "Copy preset→custom" button (seeds from presetBezier(id); disabled for elastic/bounce).
- Draw-only preview `ALCurvePreview` (new custom LLUICtrl, registered `curve_preview`): renders unit box + linear ref + 64-sample polyline of eval; calls the shared header. **[SCOPING: DEFER the preview widget + the v2 drag canvas to a fast-follow; ship the combo+spinners first.]**
- Panel `ALCurvePickerPanel` (injected like ALCineCamFramePanel llcinematiccamera.cpp:43-86), one XML `panel_camera_curve_picker.xml`, parent calls init("DirectorSwitcherEase")/("DirectorSwitcherFreeze")/("CinematicAutoFrameSettle").
- Placement (superset rule): cut-ease + freeze pickers in the Switcher panel by the ease row (panel_director_switcher.xml:415-452); settle picker in the Frame panel by the settle rows (panel_cinecam_frame.xml:150-184). Both are shared units (console + standalone floater) → superset satisfied; verify both hosts.

## 7. Determinism — evaluator pure fn (fixed iterations, no state/time); NO clock changes (cut ease + freeze = unscaled wall clock mCutEaseTimer; settle = presentation mPhase; scheduler = presentation-time); per-take latching. Golden-value test: eval(id,u) at u∈{0,.25,.5,.75,1} for each preset + eval(0,u)==cc_smootherstep(u) + linear/identity fast paths.

## Files
NEW: alcameracurve.h (header-only). [DEFERRED: alcurvepreview.h/.cpp, panel_camera_curve_picker.xml drag/preview.] MODIFIED: llcinematiccamera.cpp (2147,2157,2160-2163 termination fix,2300-2307 latch,2522; settle clamp 2121), aldirectorswitcher.cpp/.h (latch+accessors 665-670, pulse 546-549, clamp 668), settings.xml (15 keys+3 comments), panel_director_switcher.xml (max 10 + pickers), panel_cinecam_frame.xml (max 10 + settle picker). Default = bit-identical to today.
