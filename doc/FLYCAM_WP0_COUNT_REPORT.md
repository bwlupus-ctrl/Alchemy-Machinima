# Flycam WP0 counting report

Source of truth: `HEAD:indra/newview/llviewerjoystick.cpp`, re-anchored at lines 1769-2027 before the WP0 edit. Destination: `indra/newview/alflycamclassic.cpp`. Comments and whitespace are ignored only for the token-identity check. No build result is used as evidence.

## Line counts

| Block | Source physical / non-blank | Destination phases physical / non-blank | Counted moved non-blank source / destination |
|---|---:|---:|---:|
| BD (`ljc:1769-1913` -> delta + finish phases) | 145 / 123 | 150 / 127 | 115 / 115 |
| non-BD (`ljc:1914-2021` -> delta + finish phases) | 108 / 86 | 113 / 90 | 78 / 78 |
| Level-horizon cleanup (`ljc:2023-2027` -> finish tail) | 5 / 5 | 5 / 5 | 5 / 5 |
| **Total moved body** |  |  | **198 / 198** |

For each branch, the source counted body excludes three branch-wrapper lines and the five AFK lines. The destination counted body concatenates the delta-phase and finish-phase bodies while excluding signatures, braces, the `Intermediate` return, and the two local aliases that carry `time` and effective feather across the caller-owned AFK boundary. The cleanup has no exclusions.

## Statements and branches

“Statements” below counts non-comment source lines ending in `;` (multi-line statements count once); the plumbing-only return statements are excluded.

| Block | Statements | `if` | `else` | `for` | ternary |
|---|---:|---:|---:|---:|---:|
| BD source / destination | 48 / 48 | 16 / 16 | 2 / 2 | 1 / 1 | 1 / 1 |
| non-BD source / destination | 35 / 35 | 11 / 11 | 2 / 2 | 1 / 1 | 1 / 1 |
| cleanup source / destination | 2 / 2 | 1 / 1 | 0 / 0 | 0 / 0 | 0 / 0 |
| **Total source / destination** | **85 / 85** | **28 / 28** | **4 / 4** | **2 / 2** | **2 / 2** |

## Required token counts

Counts are taken after mechanically applying the plumbing substitutions listed below to the source, as required by §16.2.1. Every pair is source / destination.

| Block | `+` | `-` | `*` | `/` | `+=` | `-=` | `*=` | `llclamp` | `llmin` | `llmax` | `llabs` | subscripts |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| BD | 6/6 | 9/9 | 6/6 | 1/1 | 4/4 | 3/3 | 4/4 | 5/5 | 1/1 | 0/0 | 2/2 | 50/50 |
| non-BD | 5/5 | 6/6 | 6/6 | 0/0 | 1/1 | 0/0 | 4/4 | 2/2 | 2/2 | 1/1 | 0/0 | 40/40 |
| cleanup | 0/0 | 1/1 | 0/0 | 0/0 | 0/0 | 0/0 | 0/0 | 0/0 | 0/0 | 0/0 | 0/0 | 3/3 |
| **Total** | **11/11** | **16/16** | **12/12** | **1/1** | **5/5** | **3/3** | **8/8** | **7/7** | **3/3** | **1/1** | **2/2** | **93/93** |

## Non-verbatim plumbing tokens

Each occurrence pair is original / extracted. These are all identifier/expression substitutions in the counted body.

| Original | Extracted | Occurrences |
|---|---|---:|
| `getFlycamAxis(mJoystickAxis[i])`; cleanup form with `CAM_X_AXIS` | `in.axis[i]`; `in.axis[CAM_X_AXIS]` | 3 / 3 |
| `getJoystickButton(mMappedButtons[ZOOM_DEFAULT])` | `in.zoomDefault` | 1 / 1 |
| `getJoystickButton(mMappedButtons[ZOOM_OUT])` | `in.zoomOut` | 1 / 1 |
| `getJoystickButton(mMappedButtons[ZOOM_IN])` | `in.zoomIn` | 1 / 1 |
| `getJoystickButton(mMappedButtons[JUMP])` | `in.jump` | 1 / 1 |
| `getJoystickButton(mMappedButtons[CROUCH])` | `in.crouch` | 1 / 1 |
| `getJoystickButton(mMappedButtons[ROLL_LEFT])` | `in.rollLeft` | 1 / 1 |
| `getJoystickButton(mMappedButtons[ROLL_RIGHT])` | `in.rollRight` | 1 / 1 |
| `getJoystickButton(mMappedButtons[ROLL_DEFAULT])` | `in.rollDefault` | 2 / 2 |
| `gFrameIntervalSeconds.value()` | `in.frameIntervalSeconds` | 2 / 2 |
| `mFlycamFeathering` | `in.flycamFeathering` | 2 / 2 |
| `LLToolMgr::getInstance()->inBuildMode()`; `in_build_mode` | `in.buildMode` | 2 / 2 |
| `build_mode_scale` | `in.buildModeScale` | 2 / 2 |
| `getMinView()` / `getMaxView()` | `in.minView` / `in.maxView` | 3 / 3 each |
| `gSavedSettings.getF32("CameraAngle")` | `in.cameraAngle` | 1 / 1 |
| `trigger_roll` / `trigger_throttle` | `in.triggerRoll` / `in.triggerThrottle` | 2 / 2; 4 / 4 |
| `mXITakeover` / `mXIMode` | `in.xiTakeover` / `in.xiMode` | 4 / 4; 2 / 2 |
| `m3DCursor` / `mInvertPitch` | `in.cursor3D` / `in.invertPitch` | 3 / 3; 2 / 2 |
| `mAutoLeveling` / `mZoomDirect` | `in.autoLeveling` / `in.zoomDirect` | 2 / 2 each |
| `level_horizon` | `in.levelHorizon` | 5 / 5 |
| `mFlycamAxisDeadZone` / `mFlycamAxisScale` | `in.axisDeadZone` / `in.axisScale` | 4 / 4; 5 / 5 |
| `sLastDelta` / `sDelta` | `state.lastDelta` / `state.delta` | 7 / 7; 18 / 18 |
| `sFlycamPosition` / `sFlycamRotation` / `sFlycamZoom` | `state.flycamPosition` / `state.flycamRotation` / `state.flycamZoom` | 2 / 2; 12 / 12; 9 / 9 |

Additional plumbing outside the counted bodies: `blackdragon` becomes `in.blackDragon` (1 / 1); branch dispatch, phase signatures, `Intermediate { blackDragon, isZero, time, feather }`, its aggregate returns, the finish-phase `time`/`feather` aliases, input population, and the reference-state aggregate are new scaffolding. The two identical branch-local AFK blocks become one caller-side block using `classic_intermediate.isZero`; it remains between delta processing (including the BD Zoom Default reset) and position/rotation/level/final-zoom processing, exactly matching the original side-effect boundary. The shared cleanup remains after finish integration.

Input acquisition intentionally snapshots the pure reads required by §16.2's plumbing interface. `getJoystickAxis`, `getFlycamAxis`, and `getJoystickButton` are const lookup-only getters; the camera view-limit getters, `CameraAngle`, and build-mode query are stable reads with no intervening mutation. Some formerly conditional reads are therefore unconditional snapshots, but no observed value or state transition changes. The cleanup reuses `in.axis[CAM_X_AXIS]`, the same frame's raw value, as §16.2.1 explicitly requires.

## Identity result

After removing comments and whitespace, removing the two original AFK blocks, concatenating each destination's delta and finish bodies, excluding phase scaffolding, and applying exactly the substitutions above, the BD source and destination strings are byte-identical (2,477 characters each), as are the non-BD strings (1,687 characters each). The cleanup is likewise identical after its one input substitution. Expression order, floating-point operation order, AFK side-effect order, and the pre-existing dead BD pitch inversion are unchanged.
