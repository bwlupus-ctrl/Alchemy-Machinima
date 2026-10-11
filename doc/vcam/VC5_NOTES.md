# VC5 implementation notes

## Implemented

- Added reserved `DAD_VCAM_FEED = 22` cargo and `SOURCE_VCAM`. VCam drags skip inventory lookup and inventory background fetch, carry only the capture UUID, and are explicitly forwarded through top/root LLUI traversal; the top-control drop pass now receives `drop=true`.
- Registered object-face-only 3D dispatch. Hover and drop resolve the UUID to the current generation-bearing capture and call the VC1 target validator; rejected hover text is the validator/reassignment reason.
- Added one page-model face-drop adapter. An unbound face binds with the tile's new-face fit default; an existing world face reassigns its stable display handle, preserving mapping/effects and bypassing allocation at the 16-display cap. No-op and failed validation return before mutation.
- Added the drag-source half of `ALVCamMonitorStrip`: `beginDragFromTile(capture[, fit])`, a face-row drop adapter, and gesture-local fit state. Surface Lens drag is refused because its sole aperture display is immutable.
- Send-to-surface uses `ALVCamPageModel::bindSelectedFace` plus the new `bindSelectedFaceStatus` for exact disabled reason text; Move existing face uses `reassignDisplay` plus `reassignDisplayStatus`. VC6 can wire its anchored menu directly to these APIs without another validator.
- No undo integration was added, per the revised product decision that VCam has no undo.

## Verification

- Per request, no configure, build, executable test, commit, fetch, or upstream operation was run.
- Static verification covered cargo/source call sites, source/type gating, CMake registration, LF/trailing whitespace, and `git diff --check`.
