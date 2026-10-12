# VC5 monitor drag-and-drop trace

## Source and handoff

1. `ALVCamMonitorStrip::handleMouseDown()` (`indra/newview/alvcammonitorstrip.cpp:322`) accepts the whole tile except the small ellipsis menu hit box. This includes both the preview image and the drawn `::::` handle. It records screen coordinates with `LLToolDragAndDrop::setDragStart()` at line 349 and captures the mouse on the strip.
2. `handleHover()` (`alvcammonitorstrip.cpp:354`) uses the shared `isOverThreshold()` slop calculation (`lltooldraganddrop.cpp:264`). At line 365 it calls `beginDragFromTile()` after the threshold is crossed.
3. `beginDragFromTile()` (`alvcammonitorstrip.cpp:408`) refreshes the capture handle, rejects Surface Lens captures with an explicit reason at line 421, saves the face-fit mode, and calls `beginDrag(DAD_VCAM_FEED, capture UUID, SOURCE_VCAM)` at lines 432-433.
4. `LLToolDragAndDrop::beginDrag()` (`lltooldraganddrop.cpp:272`) stores the UUID cargo/source, takes mouse capture at line 310, installs itself as the transient tool, and starts with the no-drop cursor. The first tool hover maps acceptance to the drag/copy/no cursor (`lltooldraganddrop.cpp:459-466`).

Root cause: after step 4, the strip cleared global mouse capture. That invoked `LLToolDragAndDrop::onMouseCaptureLost()` (`lltooldraganddrop.cpp:360`), which cleared `mCargoIDs`, reset `mSource`, and removed the transient tool before any world hover or drop. The post-`beginDrag()` capture clear was removed; `beginDrag()` now owns the handoff. This matches avatar-list drag startup (`llavatarlist.cpp:521-529`) and inventory folder startup (`llfolderviewitem.cpp:737-742` -> `llfolderviewmodelinventory.cpp:58-59`), neither of which clears capture after the drag tool takes it.

## World hover and validation

5. Unhandled UI drag traversal enters `dragOrDrop3D()` (`lltooldraganddrop.cpp:722-740`). Hover queues an asynchronous pick; drop uses an immediate pick.
6. `pick()` classifies the target and passes `pick_info.mObjectFace` as the TE/face index for `DT_OBJECT` (`lltooldraganddrop.cpp`). For VCam cargo it preserves a picked non-HUD attachment as an object instead of remapping it to its avatar, allowing static, non-rigged attachment faces through the normal geometry validator. VCAM's dictionary entry sends objects to `dad3dVCamFeed`; none/avatar/land targets return custom face-only explanations.
7. `dad3dVCamFeed()` (`lltooldraganddrop.cpp:2308`) calls `ALVCamPageModel::faceDropStatus()` at line 2322. Rejection text is copied to `mCustomMsg`, becomes the hover tooltip through `acceptanceToCursor(ACCEPT_NO_CUSTOM)`, and selects the no-drop cursor.
8. `faceDropStatus()` (`alvcampagemodel.cpp:425`) rejects disabled Virtual Cam at lines 429-431, resolves the capture, detects an existing binding on the hit object/TE for reassignment, then calls `reassignDisplayStatus()` or `LLPrismLens::displayTargetStatus()` (`alvcampagemodel.cpp:467-476`).
9. `displayTargetStatus()` rejects Surface Lens sources, invalid/non-flat/loading faces, self-display, duplicates, and the 16-face cap. `validateDisplaySurfaceTarget()` deliberately performs no ownership or `permModify()` check: the display is drawn only by this viewer and does not change the target object's prim, TE, or material. Static, non-rigged world volume geometry is still required for a stable compositor surface frame; avatars, rigged/animated faces, and HUD faces are therefore rejected for technical rather than permission reasons (`indra/newview/llprismlens.cpp`).

## Drop mutation

10. Mouse-up runs a fresh acceptance pass, then the `drop=true` callback. `dad3dVCamFeed()` calls `dropOnFace()` (`lltooldraganddrop.cpp:2339`). `dropOnFace()` revalidates, uses `LLPrismLens::reassignDisplay()` for an existing display, or `bindDisplayTarget()` for a new face (`alvcampagemodel.cpp:480-500`). Both registry paths validate again before mutating.
