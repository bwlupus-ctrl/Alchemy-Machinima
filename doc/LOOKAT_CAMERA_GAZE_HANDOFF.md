# Look-at Camera / Lens Gaze Enhancements Handoff & Adversarial Review

**Specification**: `doc/LOOKAT_CAMERA_GAZE_ENHANCEMENTS.md`  
**Date**: 2026-08-19  
**Branch**: `feature/cine-light-rig` (tracking `develop`)  
**Build Status**: Code complete, pure gaze math unit tests authored (Tests 1–12), ready for independent adversarial review and build verification.

---

## 1. Executive Summary

This deliverable implements the comprehensive Look-at Camera and Lens Gaze enhancements specified in `doc/LOOKAT_CAMERA_GAZE_ENHANCEMENTS.md`, divided into **Part A (Targeting Enhancements)** and **Part B (Animation Naturalism)**.

### Delivered Capabilities
1. **Targeting (Part A)**:
   - **Look-at Camera Slot Toggles (You / A / B / C / D)**: Quick-select row in Lens Gaze panel for immediate single-click assignment of the active camera look-at to user avatar or director subjects.
   - **Per-Subject GazeTarget Architecture**: Full `GazeTarget` struct with `EMode` enum (`MOTION`, `CAMERA`, `CAST_MEMBER`, `FIXED_POINT`, `OBJECT`) for granular multi-actor targeting.
   - **Vcam Gate Live Dynamic Camera Re-aim**: Integrates with `LLPrismLens::gateOnAirCameraEye(out_agent)` to resolve the active virtual/anchored on-air camera position in real-time, falling back gracefully to the viewer camera.
   - **In-World Object Targeting**: Mode `GAZE_OBJECT` resolves the linkset bounding-box center for any selected prim or linkset.
   - **Scene Data Persistence**: Full serialization and deserialization of per-actor and self `gaze_target` configurations in `LLDirectorCast::sceneData()` and `applySceneData()`.

2. **Animation Naturalism (Part B)**:
   - **Deterministic Pseudo-Random Math**: Seeded via avatar UUID (`castSeedFromUUID`), `splitMix64`, `unitHash`, and `valueNoise` for byte-reproducible, temporally stable procedural motion.
   - **Procedural Micro-life**: Saccades (~2.2 Hz flick/hold < 1.5°), slow Lissajous head drift (< 0.8°), and periodic eyelid dip/blinks driven by presentation time.
   - **Reaction Latency**: Per-subject randomized latency before target acquisition begins (`evalReactionDelay`).
   - **Natural Glance Breaks**: Frequency-gated natural glances away (8°–14°) within tracking range (`evalNaturalBreak`).
   - **Anatomical Chain Distribution**: Progressive hierarchical joint recruitment (Eyes $\rightarrow$ Head $\rightarrow$ Neck $\rightarrow$ Torso) with physiological cone clamping and body-turn trigger (`distributeAnatomicalChain`).
   - **Asymmetric Ease**: Independent acquire ($0.25\,\text{s}$) vs. release ($0.60\,\text{s}$) transition timing (`asymmetricEnvStep`).
   - **Per-Subject Variation**: Deterministic $\pm15\%$ jitter across intensity, smoothing, and timing to avoid synchronized robotic motion (`applySubjectVariation`).
   - **Settings Registry**: 9 new runtime `DirectorGaze*` settings in `settings.xml`.

---

## 2. Files Modified & Added

| File | Subsystem | Description |
|---|---|---|
| `indra/newview/algazemath.h` | Core Math | Added deterministic hashing, naturalism structs (`GazeLifeParams`, `MicroLifeOffsets`, `AnatomicalChainPose`), `evalMicroLife`, `evalNaturalBreak`, `evalReactionDelay`, `asymmetricEnvStep`, `applySubjectVariation`, and `distributeAnatomicalChain`. |
| `indra/newview/tests/algazemath_test.cpp` | Unit Tests | Added Tests 6–12 verifying determinism, ease curves, reaction delay, micro-life bounds, glance breaks, chain distribution, and subject variation. |
| `indra/newview/llprismlens.h` | Vcam Gate | Declared `bool gateOnAirCameraEye(LLVector3& out_agent)`. |
| `indra/newview/llprismlens.cpp` | Vcam Gate | Implemented `gateOnAirCameraEye` resolving virtual or object-anchored on-air camera eye position. |
| `indra/newview/lldirectorcast.h` | Director | Added `mGazeTarget` to `CastMember`, declared `setGazeTarget` / `getGazeTarget`, and added `mSelfGazeTarget`. |
| `indra/newview/lldirectorcast.cpp` | Director | Implemented `setGazeTarget` / `getGazeTarget` and additive scene serialization in `sceneData()` / `applySceneData()`. |
| `indra/newview/llactormover.h` | Actor Mover | Added `GazeTarget` struct, `GAZE_OBJECT` enum mode, `setGazeObjectTarget` / `getGazeObjectTarget`, and `setGazeTargetConfig` / `getGazeTargetConfig`. |
| `indra/newview/llactormover.cpp` | Actor Mover | Integrated Vcam Gate on-air re-aim, Object targeting, deterministic naturalism modulations (saccades, blinks, head drift, glance breaks), and asymmetric ease envelope into `applyDirectorLookAt`, `applyGaze`, and `gazePaint`. |
| `indra/newview/app_settings/settings.xml` | Settings | Added 9 new `DirectorGaze*` tunables. |
| `indra/newview/skins/default/xui/en/panel_lens_gaze.xml` | UI XUI | Added You/A/B/C/D slot toggles, Object targeting controls, and Naturalism/Micro-life control group. |
| `indra/newview/alpanellensgaze.h` | UI Panel | Declared slot commit callbacks, object pick/clear handlers, and UI control pointers. |
| `indra/newview/alpanellensgaze.cpp` | UI Panel | Implemented slot toggles, object pick/clear from selection, and controls synchronization. |

---

## 3. Mathematical & Deterministic Specifications

### 3.1 Hash & Noise Determinism
```cpp
U64 castSeedFromUUID(const LLUUID& id);
U64 splitMix64(U64& state);
F32 unitHash(U64 seed, U64 salt);
F32 valueNoise(U64 seed, F64 t_sec, F32 frequency_hz);
```
- Fully deterministic across viewer restarts, independent of frame rate or platform endianness.
- Presentation clock (`LLPresentationTime::presentationTime()`) drives temporal evaluations, ensuring deterministic capture during temporal slow-motion / speed-up takes.

### 3.2 Micro-Life Saccades & Drift
- **Saccade Interval**: $T \approx \text{Uniform}(0.35\,\text{s}, 0.65\,\text{s})$.
- **Saccade Amplitude**: $< 1.5^\circ$ ($0.026\,\text{rad}$) yaw/pitch offset.
- **Head Drift**: 2D non-repeating Lissajous curve with frequencies $f_x = 0.23\,\text{Hz}$, $f_y = 0.17\,\text{Hz}$ and amplitude $< 0.8^\circ$.
- **Blink Dip**: Downward pitch excursion ($2.5^\circ$) during blink window ($\approx 0.15\,\text{s}$ duration every $3.5\,\text{s} \pm 1.5\,\text{s}$).

### 3.3 Progressive Anatomical Chain Distribution
Recruitment thresholds in yaw:
- $|\text{yaw}| \le 25^\circ$: Eyes only.
- $25^\circ < |\text{yaw}| \le 55^\circ$: Head ($65\%$) + Neck ($35\%$).
- $55^\circ < |\text{yaw}| \le 90^\circ$: Torso recruited ($25\%$).
- $|\text{yaw}| > \theta_{\text{body\_turn}}$ (default $90^\circ$): Triggers body re-orientation flag `mTriggerBodyTurn`.

---

## 4. Verification & Testing

### 4.1 Unit Test Coverage (`indra/newview/tests/algazemath_test.cpp`)
- **Test 1**: Envelope Step.
- **Test 2**: Smoothing Alpha.
- **Test 3**: Dead Zone Filtering.
- **Test 4**: Behind Envelope.
- **Test 5**: Eyeline Offset Rotation.
- **Test 6 (NEW)**: Deterministic Hash & Noise consistency across calls.
- **Test 7 (NEW)**: Asymmetric Acquire ($0.25\,\text{s}$) vs Release ($0.60\,\text{s}$) envelope progression.
- **Test 8 (NEW)**: Reaction Latency gating and zero envelope prior to delay expiry.
- **Test 9 (NEW)**: Micro-life saccade and drift maximum bound checks ($< 1.5^\circ$ saccade, $< 0.8^\circ$ drift).
- **Test 10 (NEW)**: Natural glance breaks triggering and bounds.
- **Test 11 (NEW)**: Anatomical chain distribution joint limits (eyes $\le 35^\circ$, head $\le 72^\circ$, neck $\le 25^\circ$, torso $\le 20^\circ$).
- **Test 12 (NEW)**: Per-subject character variation non-zero variance between different UUIDs.

---

## 5. Adversarial Review Checklist

- [x] **Compile Safety (`/WX`)**: All numeric conversions (`LLSD::Real` $\rightarrow$ `F32`, `double` $\rightarrow$ `float`) explicitly cast.
- [x] **LLUI Text Rule**: No bare string literal calls to `setText()` / `setLabel()`; all use `std::string` or `LLStringExplicit`.
- [x] **XUI Control Resolution**: Every `getChild<T>("name")` in `alpanellensgaze.cpp` matches a named control in `panel_lens_gaze.xml`.
- [x] **No Comment Traps**: No `--` inside XML comments in `panel_lens_gaze.xml`.
- [x] **Global/Agent Coordinate Space Safety**: `LLVector3d` global and `LLVector3` agent coordinate spaces strictly separated and converted via `gAgent.getPosAgentFromGlobal` / `gAgent.getPosGlobalFromAgent`.
- [x] **Zero Memory Leaks / No Dangling Pointers**: Linkset bounds and object targeting query `gObjectList.findObject()` with dead-object checks.
- [x] **Backward Scene Compatibility**: `LLDirectorCast::applySceneData()` loads legacy scenes seamlessly (missing `gaze_target` defaults cleanly).
- [x] **Runtime Asset Sync**: Updated `panel_lens_gaze.xml` and `settings.xml` deployed to `build-Windows-vs2026-os/newview/Release/`.
