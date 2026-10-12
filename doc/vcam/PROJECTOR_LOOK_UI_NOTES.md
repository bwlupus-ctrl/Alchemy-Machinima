# Projector look UI notes

The Director **Camera > VCam > Output** page now treats each projector row as a selectable item. Clicking the row body selects it; clicking **Remove** removes only that viewer-local feed. The selected handle lives in `ALVCamPageModel`, is cleared when the camera changes, and is invalidated from registry snapshots when the projector disappears.

The **Projector** and **More effects (TV)** cards are code-hidden until a projector is selected. Their badge is the projector's one-based position within the selected camera's projector list. All controls are collected once in `postBuild()` and refreshed only from the existing model/registry signals.

Every edit copies the selected `ProjectorSettings` snapshot and writes through `ALVCamPageModel::setProjectorSettings()` with no undo entry. `setProjectorLook()` replaces only `ProjectorSettings::mLook`; framing, signal settings, feed exposure, and TV effects are preserved. The five look presets and their values are defined in `alvcampagemodel.cpp` from `PROJECTOR_LOOK_CONTRACT.md`; exact preset matching drives the combo, otherwise it displays **Custom**.

Projector orientation uses `ProjectorSettings::mFlipH`, `mFlipV`, and quarter-turn `mRotate`. The creative controls use `ProjectorSettings::mLook`, while the reused TV rows use `ProjectorSettings::mEffects`. Gel tint is presented as a standard kit colour swatch and committed as `LLColor3`.

`scripts/code_tools/director_kit/vcam_page.json` is the XUI and functional-row authority. Regenerate the VCam page and functional manifest in the normal integrated job; the new code-bound controls intentionally defer at login when no projector is available.
