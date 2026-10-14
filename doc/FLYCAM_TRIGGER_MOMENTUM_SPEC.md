# Flycam: triggers drive momentum (spec, 10/13/2026)

## User request (10/13 ~9 PM)
"The brake and gas should influence the momentum, and an option for it."
User decisions (picked from options):
- **Brake (LT): active brake.** LT pushes against the camera's current momentum, proportional to how far it is pulled. Full LT stops a coasting camera in about 1 second, even with the sticks released.
- **Gas (RT): faster build-up.** RT raises top speed (today's throttle scaling, kept) AND makes momentum build faster, so the camera surges. Releasing RT returns to normal response; the camera still coasts with its mass.
- **Scope:** Weighted Classic (FlycamFlightModel 2) and Drone (1). **On by default.** Classic (0) stays bit-exact.

## Current state (verify before changing)
- Trigger mode: JoystickXInputTriggerMode 0 = ROLL (default), 1 = THROTTLE. Toggled with D-pad Down (llviewerjoystick.cpp ~3616). In ROLL mode none of this applies.
- **Weighted Classic** (alflycamweighted.cpp/.h) receives only `classic_input.triggerThrottle` (llviewerjoystick.cpp ~2486), a speed multiplier from ALXInput::throttle(LT, RT, min, max) (alxinputmath). It has no brake.
- **Drone** (llflycamdrone.cpp/.h) already has `input.throttle`, `input.brake`, `droneThrottle`, `brakeDeceleration` and ThrottleMode ASSIST/PURE_THROTTLE (llviewerjoystick.cpp ~2600). Check what LT/RT already do there against the decisions above, and only add what's missing.

## Behaviour to implement
New setting **FlycamTriggerMomentum** (Boolean, default TRUE, persisted). Shown in the Joystick floater's Flight model tab as "Triggers affect momentum (gas builds faster, brake slows the camera)", next to the Weighted section. Applies only when trigger mode = THROTTLE, flycam is active and the flight model is Weighted or Drone. When it's off, behaviour is exactly today's.

**Impact slider (user, 10/13):** add **FlycamTriggerMomentumImpact** (F32, persisted, default 1.0, range 0.0–2.0, shown as 0–200%). It's a VISIBLE slider directly under the checkbox, labelled "Trigger impact", with the tooltip "How strongly gas and brake change the camera's momentum". It scales BOTH effects:
- gas: `GasResponse_eff = lerp(1, GasResponse, clamp(impact, 0, 2) / 1)`, clamped so it never goes below 0.05 (at 200% the build-up is stronger than default);
- brake: `BrakeDecel_eff = BrakeDecel * impact`.

Impact 0% behaves exactly like the option OFF, byte-identical. The two Advanced values (GasResponse, BrakeDecel, BrakeRotation) stay as fine-tuning underneath.

Inputs: `gas` = shaped RT in 0..1 and `brake` = shaped LT in 0..1, using the existing dead zone and curve (JoystickXInputTriggerDeadZone, TriggerCurve). Don't reuse the combined Min/Max throttle scalar for these.

### Weighted Classic
The model is a critically damped 2-lag per axis with time constants tau_t (translation) and tau_r = 0.6·tau_t (alflycamweighted).
- **Gas:** keep the existing top-speed scaling. Additionally shorten the effective time constants while RT is held: `tau_eff = tau * lerp(1, GasResponse, gas)` with **FlycamTriggerGasResponse** (F32, default 0.35, range 0.1–1; lower = faster build-up). This applies only while the stick target magnitude exceeds the current velocity's magnitude (building up). When the stick is released or reversed, the normal tau is used, so coasting stays natural.
- **Brake:** latch the current translational and angular speeds when brake rises from zero. Apply `|Δv| = brake * Impact * v_ref / FlycamTriggerBrakeStopTime * dt`, raising `v_ref` if stick input makes speed rise, and clamp exactly to zero without reversing. **FlycamTriggerBrakeStopTime** is F32 seconds (default 1.0, range 0.2–5). Rotation uses the same law multiplied by **FlycamTriggerBrakeRotation** (default 1.0, range 0–2; 0 = translation only). Releasing brake clears both latches.
- Fixed-step integration: brake and gas changes go into the existing fixed-step/interpolation loop. They must be frame-rate independent: the same result at 30, 60 and 144 fps within tolerance.
- Weight 0% stays bit-exact Classic when the option is OFF. With the option ON at Weight 0%, gas only changes top speed as today, and brake applies its decel to the classic velocity. That's acceptable; document it.

### Drone
- Map onto the existing throttle/brake. Brake = the existing brakeDeceleration path, scaled by LT, which must actively decelerate even with no stick input. Gas = the existing throttle path. If ASSIST mode doesn't already make RT build momentum faster, apply the same GasResponse idea to the Drone's acceleration limit. Reuse the shared FlycamTriggerGasResponse / BrakeDecel settings unless the Drone has better-tuned equivalents; document the choice.
- **FlycamTriggerMomentum OFF** = Drone behaves exactly as today.

### Feedback
- The D-pad Down hint keeps working. When the option is on and the model is Weighted, the THROTTLE hint reads "Triggers: GAS + BRAKE".
- Optional: a light rumble proportional to brake, using the existing rumble channel and JoystickXInputRumble. Default off unless trivial.

## Pass criteria
1. Unit tests in indra/newview/tests:
   - **alflycamweighted_test.cpp:**
     - (a) brake from a coasting velocity reaches 0 in about 1.0 s ±10% at Weight 50%, and never reverses sign;
     - (b) full RT reaches 90% of target velocity measurably faster than with no RT (ratio about GasResponse);
     - (c) releasing everything coasts exactly as today;
     - (d) option OFF is byte-identical to the current model output over a scripted input sequence;
     - (e) frame-rate independence at 30/60/144 fps;
     - (f) Impact: at 0% the output is byte-identical to option OFF; at 50% the brake takes about 2× as long to stop as at 100%; at 200% about half as long; gas build-up gets monotonically faster with impact.
   - **llflycamdrone_test.cpp:** brake with no stick decelerates to 0; option OFF is identical to today.
   - **alflycamclassic_test.cpp:** unchanged and passing (Classic untouched).
   - All existing tests still pass: weighted 50/50, classic 4/4, drone 49/49, xinput 12/12 (or the current counts).
2. Compile gate (zs_check) plus all prebuild_guard gates green.
3. The joystick floater shows the new checkbox and the two tuning values (Advanced). xui_layout_check reports zero findings.
4. No per-frame setting lookups by name: use LLCachedControl.

## Out of scope
Classic model behaviour; ROLL mode; D-pad mappings; recorder playback (it replays camera poses, not inputs).
