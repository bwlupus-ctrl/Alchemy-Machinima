# XInput gamepad: D-pad actions and trigger ROLL/THROTTLE (design brief v2)

Status: DESIGN ONLY. No source edits, no build, no commit. Line numbers are against the tree at 2026-10-04.
"PROVEN" means read in code. "INFERENCE" means not provable from this repo. v1 got a Codex NO-GO; section 8
maps every finding to its fix. The USER DECISIONS at the end are binding.

## 1. Current-state map
**DirectInput path (Windows).**
- libndofdev reads only `DIJOYSTATE` axes `lX..lRz` and `rgbButtons`; it never reads `rgdwPOV`, so the
  D-pad is invisible today (`vcpkg/buildtrees/libndofdev/src/v1.0.1-5346caa259.clean/src/ndofdev_win.cpp:311-316,362-363`).
- `updateStatus()` normalises into `mAxes[6]`/`mBtn[16]` (`llviewerjoystick.cpp:836-857`).
- `scanJoystick()` runs once per main-loop pass (`llappviewer.cpp:1501-1510`). It is skipped when focus is
  locked, during progress, or when the window is inactive.
- `moveFlycam()` runs later in the same pass, in idle (`llappviewer.cpp:5602-5604`). `gFrameCount` is
  incremented only in display (`llviewerdisplay.cpp:604`), so scan and flycam see the same frame number. PROVEN.
- In flycam, `updateStatus` is driven by a 30 fps `WM_TIMER` (`llviewerjoystick.cpp:2283-2288`,
  `llviewerwindow.cpp:2140-2148`).

**What the viewer knows about the DI device. PROVEN.**
- `NDOF_Device` exposes only `product`/`manufacturer` strings plus opaque `private_data`
  (`ndofdev_external.h:50-61`).
- The viewer keeps the active device's `guidInstance` in `mLastDeviceUUID` (`llviewerjoystick.cpp:698,755`).
- It holds a real `LPDIRECTINPUTDEVICE8` only inside its own enumeration callback (`:221-253`).
- It can enumerate `DI8DEVCLASS_GAMECTRL` through `getInputDevices()` with a callback receiving `DIDEVICEINSTANCE`
  (`guidInstance`, `guidProduct`, `tszProductName`) (`:2164-2168,142-165`).
- The DI8 interface is reachable via `getWindow()->getDirectInput8()` (`:221`).
- If ndof falls back to its own pick, `mLastDeviceUUID` is cleared (`:738-740`).
- The device interface path is not stored anywhere, but can be queried per device with
  `IDirectInputDevice8::GetProperty(DIPROP_GUIDANDPATH)`.

**Triggers today. PROVEN; this corrects the original premise.**
- Both triggers arrive as merged `lZ` = device axis 2 (`ndofdev_win.cpp:313`).
- Xbox Defaults map roll (`JoystickAxis3`, `CAM_X_AXIS`, `llviewerjoystick.h:71`) to axis 2
  (`llviewerjoystick.cpp:2571`). The user's live settings match: `JoystickAxis3=2`, `BlackDragonControls=1`,
  `Cursor3D=0`.
- So the triggers already give analog roll.

**Who flies with the pad.** Only `moveFlycam`. Free fly runs through the BlackDragon branch (`:1626-1764`) or
the stock branch (`:1765-1866`); Orbit is at `:1868-2001`. Director, CineCam, PathCamera, VCam and recorder
playback never read axes (`llappviewer.cpp:5580-5601`). Throttle therefore covers flycam and Orbit, and recorded
takes inherit it (`:5617`).

**Lighting cue API (the only prev/next cue stack).**
- Call `ALCineLightRigManager::instance().selected()` (`alcinelightrigmanager.h:1285,1315`), then
  `.cueGo(t)` / `.cueBack(t)` (`alcinelightrig.h:308-309`).
- Both no-op on an empty list or in timecode mode (`alcinelightrig.cpp:4019-4049`). State is in
  `cueList()`/`activeCue()` (`alcinelightrig.h:301,312`).
- `t` is `LLPresentationTime::currentFrame().presentation_time` clamped to 0 or more, as the floater does
  (`alfloatercinelightcues.cpp:33-37,348-358`). The floater refreshes in `draw()` (`:373-377`).

## 2. Design
### 2.1 Three gates (the core of v2)
| Gate | Predicate | Enables |
|---|---|---|
| **Associated** | master on, XInput loaded, `mDriverState==JDS_INITIALIZED`, `mJoystickEnabled`, and the latest association scan found **exactly one** IG DI device, **that device's `guidInstance` == `mLastDeviceUUID`**, and **exactly one** connected XInput slot | D-pad, status |
| **Compatible** | `!m3DCursor` and `mJoystickAxis[CAM_X_AXIS]==2` and, for every `i != CAM_X_AXIS` in 0..6, `mJoystickAxis[i] != 2` | lets takeover engage |
| **Takeover** | Associated, Compatible, flycam active, and (`mode==THROTTLE` or `JoystickXInputRollShaped`) | axis-2 suppression and injection |

- **Legacy behaviour holds in every other case.** If not Associated, nothing new runs at all. If Associated but
  not Takeover, DI axis 2 behaves exactly as today and only the D-pad is live.
- **No deliberate opt-in for custom mappings.** It is left out (simplest and safest). To become Compatible, press
  **Xbox Defaults**: it sets `Axis3=2`, no other axis to 2, and `Cursor3D=false` (`:2568-2574,2592`).
- **ROLL by default is a pass-through.** It is bit-identical to today: the DI merged Z keeps driving roll, with
  no suppression and no injection.
- **Shaped roll** (curve and deadzone on the triggers) is opt-in via `JoystickXInputRollShaped`. This is the
  "legacy feel" answer: the default ROLL *is* the legacy feel, so no curve/deadzone approximation is needed.
- **With master ON, nothing changes until the user presses D-pad Down**, because L/R/Up default to None.

### 2.2 P0-2: Pad association (unambiguous or nothing)
- **XInput-capable test (standard Microsoft rule).** An XInput device's HID device path contains `IG_`
  (Microsoft, "XInput and DirectInput", `IsXInputDevice`). The code applies it to `DIPROP_GUIDANDPATH.wszPath`.
  - INFERENCE: SDL's DirectInput backend uses the same path check.
  - VID/PID via `guidProduct.Data1` is logged for diagnostics only. Public XInput exposes no VID/PID to compare against.
- **Association scan** (new `scanXInputAssociation()`).
  - Run it from `getInputDevices(DI8DEVCLASS_GAMECTRL, …)` exactly as `pollForXboxController` does (`:2164-2168`).
  - The callback does `CreateDevice(guidInstance)`, then `GetProperty(DIPROP_GUIDANDPATH)`, then a
    case-insensitive `IG_` search, then `Release()`.
  - Record `igCount` and `igGuid` (the GUID, when there is exactly one).
  - Then probe XInput slots 0..3 with `XInputGetState` and record `slotCount` and `slot`.
  - Both results carry the same `mXIScanGeneration`, and Associated uses only a matched pair.
- **Ambiguity disables everything.** Any of these applies:
  - `igCount != 1`, `slotCount != 1`, or `igGuid != mLastDeviceUUID` (`sameWindowsDeviceId`, `:129-133`);
  - an empty `mLastDeviceUUID` (the ndof fallback pick);
  - a failed `CreateDevice`/`GetProperty` or a failed enumeration.

  Each is treated as "not Associated": no takeover, no D-pad. One `LL_INFOS("XInput")` line is logged per scan
  generation with the counts and the reason. This covers two pads, a virtual Steam/DS4Windows pad plus a
  physical pad, and a non-XInput device selected in DI.
- **Per-frame identity check.** If `mLastDeviceUUID` stops equalling the associated GUID (the floater switched
  device), drop the association and arm a rescan. This is a cheap binary compare.

### 2.3 Scan scheduling (hot-plug, zero cost when settled)
- **Triggers:** startup, master off-to-on, `setDeviceListDirty()` (`llviewerjoystick.h:165`, which now also arms
  `mXIRescan`), an XInput `ERROR_DEVICE_NOT_CONNECTED` on the associated slot, or a device-identity change.
- **Bounded, debounced retries.** A trigger resets the budget to 4 attempts at +0.5 s, +1 s, +2 s and +4 s
  (`LLFrameTimer`). The +0.5 s delay absorbs `WM_DEVICECHANGE` bursts and driver readiness. Stop when Associated,
  or when the budget is spent.
- **Settled cost.** With no pad or with the master off, there are no XInput or DI calls per frame. When
  Associated, there is one `XInputGetState(slot)` per scan pass.
- **Disconnect.** Clear the association, zero LT/RT, set takeover off (history seeded per 2.5), and re-arm the
  retry budget, so a remaining pad B can still be found.

### 2.4 Loader
`XInputGetState` is loaded from the **system directory only**. The application directory is never searched, which
avoids wrapper DLLs and DLL planting. Try in order:
1. `LoadLibraryExW(L"xinput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)`.
2. If that fails with `ERROR_INVALID_PARAMETER` (Win7 without KB2533623), `LoadLibraryW(GetSystemDirectoryW()+L"\\xinput1_4.dll")`.
3. Steps 1 and 2 again for `xinput9_1_0.dll`.

Each candidate must export `XInputGetState`; otherwise `FreeLibrary` and try the next. If all fail, log once and
the feature stays inert. Free the library in `terminate()` (`:822-833`). `<Xinput.h>` is used for types only,
included after `<dinput.h>` (`:50-55`).

### 2.5 Takeover mechanics in `moveFlycam`
**Sampling and staleness.**
- `scanJoystick` (after the `:2278` gate) calls `pollXInputState()`. On success it stores raw `LT`, `RT` and
  `buttons`, plus `mXISampleFrame = gFrameCount`.
- `computeTriggerOutputs()` (after `:1601-1622`) treats the sample as **stale** unless
  `mXISampleFrame == gFrameCount` and the app has focus. A stale sample gives neutral outputs (roll 0,
  throttle 1). Takeover and suppression stay as they are, so the DI axis does not flip on and off.

**Suppression.** Under Takeover, `getFlycamAxis(dev)` returns 0 for `dev == 2`. It replaces `getJoystickAxis` at
`:1657` and `:1782`. Compatible guarantees that only roll is affected.

**History seeding**, via `seedTriggerHistory()`:
- Set `sLastDelta[i]` (for each `i` with `mJoystickAxis[i]==2`) to the value the effective input will report next
  frame: 0 under Takeover, `-mAxes[2]` otherwise.
- Set `sDelta[CAM_X_AXIS] = 0`, which kills roll momentum.
- Run it:
  - in the reset branch right after `resetDeltas` (`:1614`; `resetDeltas` seeds raw `mAxes`, `:1019`);
  - whenever the key {Takeover, mode, RollShaped} differs from last frame. That covers ownership, mode and
    mapping changes, because `refreshFromSettings` (`:611-693`) feeds Compatible.

**Shaper.** `s(t) = t<=d ? 0 : ((t-d)/(1-d))^g`, with `t = trigger/255` and `d`, `g` from settings (clamped).

**ROLL shaped.**
- `r = clamp(s(RT)-s(LT), -1, 1)` is added to `cur_delta[CAM_X_AXIS]` after the deadzone block (BD after `:1697`,
  stock after `:1801`), then clamped to [-1,1].
- It then flows through `FlycamAxisScale3`, dt and feathering like stick roll. Sign mirrors `ROLL_RIGHT`/`ROLL_LEFT`
  (`:1671-1672`).
- Cursor3D is excluded by Compatible, so the absolute path without dt (`:1712-1715`, `:1816-1819`) is never reached
  (Codex P1).

**THROTTLE.**
- `m = Mmax^s(RT) · Mmin^s(LT)`, the log-space net. Multiply `cur_delta[i]` for `i <= Z_AXIS` at the build-scale site
  (BD `:1702-1708`, stock `:1806-1812`), which is before axis scale, dt and feathering. Skip the multiply when both
  `s` are 0.
- Why log space: equal travel gives equal ratio steps, the triggers are independent, and both held gives
  `Mmin^sL·Mmax^sR` (both full = 0.4x with defaults). Chained lerp was rejected because RT dominates and LT
  resolution is wasted.
- With defaults d=0.06, g=2, Mmin=0.1, Mmax=4:

  | Trigger travel | 25% | 50% | 75% | 100% |
  |---|---|---|---|---|
  | LT | 0.91x | 0.60x | 0.29x | 0.10x |
  | RT | 1.06x | 1.36x | 2.11x | 4.0x |

### 2.6 D-pad (edge, priming, dispatch)
- **Raw history** `mXIPrevRaw` is updated from every fresh sample, focused or not. Focus never overwrites it.
- **Priming:** set `mXIPrevRaw = cur` and skip dispatch for that sample when any of these happen:
  - the sample is the first after Associated turns true (this covers slot change, master re-enable and replug);
  - focus is regained;
  - `gFrameCount != mXILastScanFrame+1` (scan skipped).
- **Dispatch** `cur & ~prev` happens only while Associated, focused and not priming. Each press fires once, with
  no repeat. Edges are computed before the focus return at `:2292-2295` and dispatched after it.
- **DOWN** toggles `JoystickXInputTriggerMode` with `setS32`, which persists. A hint is shown.
- **L/R/Up** take an action enum: 0 None, 1 Lighting cue GO, 2 Lighting cue BACK, 3 Level horizon (all
  default 0). The cue actions call 1.x's API, and the hint shows "Light cue 3/8", "No light cues" or "Timecode
  mode".
- **Level horizon:**
  - It is accepted only while `getOverrideCamera()`; otherwise the hint says "Flycam not active".
  - It sets `mXILevelFrame = gFrameCount`. `moveFlycam` consumes it only if `gFrameCount - mXILevelFrame <= 1`;
    otherwise it is dropped, and `toggleFlycam` (`:2076-2112`) clears it.
  - On consumption: snap as `ROLL_DEFAULT` does (BD `:1735,1751`; add the same snap to stock `:1838-1854`),
    set `sDelta[CAM_X_AXIS]=0`, and seed `sLastDelta[CAM_X_AXIS]`.
  - In Orbit, `sOrbitRoll = 0` is applied after its integration at `:1982-1984`.
- **Hint:** a `notifytip` `XInputPadHint [MESSAGE]` next to `notifications.xml:9271-9281`.
  `LLNotificationsUtil::cancel` (`llnotificationsutil.h:62`) the previous hint so only one is ever live.

### 2.7 Settings (`app_settings/settings.xml`, after `JoystickAutoFlycamEnabled` `:13608`; all persist)
| Name | Type | Default | Range / notes |
|---|---|---|---|
| `JoystickXInputEnabled` | Boolean | 1 | master (user decision) |
| `JoystickXInputTriggerMode` | S32 | 0 | 0 ROLL, 1 THROTTLE; set only by D-pad Down |
| `JoystickXInputRollShaped` | Boolean | 0 | 0 = legacy DI roll pass-through |
| `JoystickXInputThrottleMin` / `Max` | F32 | 0.1 / 4.0 | [0.01,1] / [1,20]; NaN clamps to default |
| `JoystickXInputTriggerDeadZone` | F32 | 0.06 | [0,0.5] |
| `JoystickXInputTriggerCurve` | F32 | 2.0 | [1,4] |
| `JoystickXInputDpadLeft` / `Right` / `Up` | S32 | 0 / 0 / 0 | action enum, 2.6 |

All are read with `LLCachedControl` (precedent `:1623-1625`).

## 3. Integration points
| File:line | Change |
|---|---|
| `llviewerjoystick.h:154-185` | methods `scanXInputAssociation`, `pollXInputState`, `handleXInputDpad`, `computeTriggerOutputs`, `getFlycamAxis`, `seedTriggerHistory`; `mXI*` state (slot, generation, igGuid, retry budget/timer, raw/prev buttons, LT/RT, sample frame, last scan frame, level frame, last config key, hint ptr); `setDeviceListDirty()` also arms `mXIRescan` |
| `llviewerjoystick.cpp:50-55,118-166` | `<Xinput.h>`; loader; IG enumeration callback (beside `di8_xbox_probe_callback`) |
| `llviewerjoystick.cpp:2274-2295` | association scheduling beside `pollForXboxController()` (`:2276`); `pollXInputState()` after `:2278`; D-pad edges and dispatch around `:2292` |
| `llviewerjoystick.cpp:1608-1622` | seed after `resetDeltas`; `computeTriggerOutputs()` and config-key transition check |
| `:1657`, `:1782` | `getFlycamAxis` |
| after `:1697`, `:1801` | shaped-roll injection |
| `:1702-1708`, `:1806-1812` | throttle multiply |
| `:1735-1751`, `:1838-1854`, `:1982-1995` | level horizon |
| `:2076-2112` | clear pending level |
| `app_settings/settings.xml:~13608`; `notifications.xml:~9282` | settings; hint |
| `floater_joystick.xml`, `llfloaterjoystick.{h,cpp}` | section 4 |
| new `indra/newview/alxinputmath.h`; `CMakeLists.txt:~1218` and `:2712-2718` | header-only shaper, throttle and edge/priming helpers; TUT registered like `algazemath` |

## 4. UI: Joystick floater, right column (x=359, w=200)
- Grow the floater `height` 700 to 724, and move `ok_btn`/`cancel_btn` `top` from 650 to 674 (`floater_joystick.xml:2569-2590`).
- Rows (`top`, `height`):

  | Top / height | Control |
  |---|---|
  | 470 / 18 | checkbox "Xbox pad: triggers + D-pad" (`JoystickXInputEnabled`) |
  | 490 / 26 | status text, 2 lines, SansSerifSmall: e.g. "Pad: ready / none / ambiguous (2 XInput)" and "Triggers: ROLL legacy / ROLL shaped / THROTTLE / legacy: mapping / legacy: 3D cursor · LT 0.00 RT 0.00" |
  | 518 / 18 | spinners "Slow x" (left 359, w98, label_width 40) and "Fast x" (left 461, w98) |
  | 540 / 18 | spinners "Deadzone" (359) and "Curve" (461) |
  | 562 / 18 | checkbox "Shaped roll (Deadzone/Curve)" (`JoystickXInputRollShaped`) |
  | 586, 608, 630 / 20 | "D-pad L/R/Up" text (w46) + `combo_box` (`left_pad` 4, w150); items None / Light cue GO / Light cue BACK / Level horizon |

  The last row ends at 650, clear of the moved buttons.
- **Cancel:** v1 was wrong. `cancel()` restores `JoystickAutoFlycamEnabled` (`llfloaterjoystick.cpp:350-353`).
  - Add snapshot members (`llfloaterjoystick.h:74-100`) for every new setting **except TriggerMode**.
    TriggerMode is pad-owned and not on the floater, so Cancel must not undo a D-pad toggle.
  - Capture the snapshot in `initFromSettings()` (`:197-200`) and restore it in `cancel()` (`:350+`).
- The status text is updated in `draw()` (`:107-147`).

## 5. Test plan (P2 cases)
**TUT (`alxinputmath`):**
- shaper bounds and monotonicity;
- `m == 1` exactly at rest, endpoints `Mmin`/`Mmax`, both full = `Mmin*Mmax`;
- clamps, including NaN and Mmin > Mmax;
- `r` antisymmetry;
- edge detector: single fire, no repeat, priming on each event in 2.6, and stale samples never produce edges;
- Compatible predicate truth table: axis 2 on roll only / on roll and another axis / on another axis only / unmapped / Cursor3D.

**In-world.** The log states a verdict: `LL_INFOS("XInput")` on each association scan with the counts and the
outcome, and `LL_DEBUGS("XInput")` at 1 Hz with `assoc/compat/takeover/mode/LT/RT/m/r`.
1. **Off path.** Master OFF, and also master ON in ROLL without shaping, on the A/B backup exe: identical roll and
   speed at several single-trigger depths (25/50/75%).
2. **Suppression.** THROTTLE, RT alone at 25/50/75/100%, then LT alone the same way.
   - PASS: the speed changes and roll is exactly 0.
   - FAIL: any roll, which means DI Z leaked. Repeat in ROLL shaped with one trigger at 50%, and compare the roll
     rate with `r`.
3. **Custom mapping.** Map axis 2 to zoom or translation and switch to THROTTLE. The status reads
   "legacy: mapping" and the original control still works.
4. **Cursor3D on.** The status reads "legacy: 3D cursor" and the triggers behave as today.
5. **Two pads** (two Xbox pads, or an Xbox pad plus a Steam/DS4Windows virtual pad).
   - The status reads "ambiguous", the D-pad is dead, and behaviour is legacy.
   - Unplug one: after one retry it becomes "ready".
6. **Hot-plug.**
   - Unplug with RT held mid-flight: `m` goes to 1 without a jolt and a retry burst is logged.
   - Replug: it re-associates, and a held D-pad does not fire.
   - Tracy: no XInput or DI calls per frame once settled.
7. **Staleness.** Hold RT in flycam and open a modal (focus lock). Speed returns to preset.
8. **Transitions.** Toggle mode with a trigger held, press flycam reset, and press Xbox Defaults while flying.
   None of these causes a one-frame roll kick.
9. **Level horizon.** Roll hard, then press: the roll is level and does not resume. A press outside flycam shows
   the hint and has no later effect.
10. **Cues.** GO/BACK with the Light Cues floater open, on an empty list, and in timecode mode.
11. **Floater.** Change every new control, then press Cancel: all are restored, and the mode is untouched.
12. **SpaceMouse active plus pad.** "ambiguous"/not Associated, and behaviour is identical.

## 6. Inherent limits (documented, not looped on)
- **Identity is proven by counting, not by pairing.** It relies on 1 IG device = 1 XInput slot. An XInput device
  with no DI IG twin would break that; none are known for standard drivers (INFERENCE). Public XInput exposes no
  path or VID/PID to pair with.
- **Ambiguous setups get no new features.** Any multi-pad or virtual-pad setup falls into this case by design.
- **D-pad taps can be missed.** A tap shorter than one main-loop frame is missed when the frame rate is low.
- **Frame-rate dependence is inherited.** BD dt is clamped (`:1631`) and stock smoothing is unstable at
  `dt=.2`/high feather (`:1821`). Throttle smoothness is only as good as feathering.
- **Shaped roll is not legacy.** It has new feel by design; legacy is the default.
- **The toast is visible in captures when the UI is shown.**

## 7. Open questions
None blocking. (Pre-existing, out of scope: `resetDeltas` indexes `mAxes[axis[i]]` with `axis == -1` (`:1019`),
which is out-of-bounds UB for unmapped axes. It deserves a separate fix.)

## 8. v2 / review response (Codex NO-GO, `C:\Users\xianw\cxs\xinput_brief_review.txt`)
| Finding | v2 resolution |
|---|---|
| P0 custom mappings | Takeover gated on **Compatible** (2.1); no opt-in path; ROLL default is legacy pass-through, so master-ON is a no-op until D-pad Down |
| P0 pad identity | **Associated** = IG path via `DIPROP_GUIDANDPATH`, exactly 1 IG DI device that *is* the active `guidInstance`, and exactly 1 XInput slot, from the same scan generation; anything else disables everything and logs once (2.2) |
| P1 history | `seedTriggerHistory` on reset, config-key change (ownership/mode/mapping) and level (2.5, 2.6) |
| P1 Cursor3D | excluded by Compatible |
| P1 disconnect | re-arms 4-attempt debounced retry burst, then settles (2.3) |
| P1 D-pad priming | raw history independent of focus; priming on every eligibility/focus/frame-gap event (2.6) |
| P1 stale samples | `mXISampleFrame == gFrameCount` required, else neutral (2.5) |
| P1 level horizon | clears `sDelta`/`sLastDelta` roll, orbit reset after integration, 1-frame expiry (2.6) |
| P1 loader | 1_4 then 9_1_0, system dir only, Win7 absolute-path fallback, export validated (2.4) |
| P2 Cancel | snapshot/restore every new setting except pad-owned mode; concrete geometry (4) |
| P2 tests | single-trigger intermediate leakage, custom mapping, Cursor3D, two-pad, virtual pad (5) |
| v1 actions/shots | removed per USER DECISIONS; D-pad defaults None |

## USER DECISIONS (2026-10-04) -- override anything above that differs
1. D-pad LEFT / RIGHT / UP: default action **None** for all three. Keep them assignable; action list = None, Lighting cue Back, Lighting cue Go (ALCineLightRigManager selected().cueBack/cueGo), Level horizon. **No camera-shot (Director switcher) action** in this version -- do not implement it.
2. Master switch **ON** by default; trigger mode starts in **ROLL** (matches today's trigger roll).
3. Throttle min/max multipliers are user settings (defaults per design: min 0.1, max 4.0, deadzone 0.06, curve 2).
