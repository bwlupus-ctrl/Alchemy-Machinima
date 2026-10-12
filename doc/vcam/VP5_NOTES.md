# VCam Projector VP5 notes

## Texture-picker UX

The standard `PICK: TEXTURE` floater now has a `VCam` source. It presents `None / Remove VCam feed` plus the current Cam 1-8 registry entries. Each camera row samples its cached VC3 preview-atlas cell when one is published and otherwise keeps the camera name with an empty thumbnail. With no captures, the source shows `No VCam cameras - add one in Director > Camera > VCam` rather than a blank list.

The existing texture swatch is the entry point, including on no-modify objects. `LLTextureCtrl` keeps only its VCam source interactive when normal texture editing is disabled; Inventory, Local, and Bake remain disabled. This avoids adding a second, slot-specific button and makes the same source available anywhere the diffuse texture picker is used. A bound slot overlays `VCam: Cam n` and exposes a viewer-local tooltip.

VCam selection has a separate callback from the texture UUID commit path. Apply-now invokes the local callback immediately; OK invokes it when Apply-now is off. It never changes `mImageAssetID`, calls the ordinary texture/light commit callback, changes a TE or LightImage, or sends an object update. Cancel does not roll back an already applied local choice, consistent with the no-undo rule.

Face application preflights every selected face, treats already-bound faces as no-ops, and refuses the whole request before mutation if any target is invalid or if the new bindings would exceed the global 16-face cap. Projector application uses the VP1 `LLPrismLens`/`ALVCamPageModel` binding API and is likewise viewer-local. `None` removes matching selected-face bindings or the selected light's projector binding.

## Drag and output rules

A monitor drop on an object that is already a spotlight/projector always binds the projector, independent of which face was hit. This is the clearest rule because a projector light has one cookie target, while the hit face is incidental. A drop on every other object continues to bind the hit face. Re-dropping the same camera is an idempotent success.

The selected camera's Output section now includes a `Projectors` card with one row per bound projector and a `Remove` action. The entire card is hidden when that camera has no projector bindings.

## Integration

No new `LLPrismLens` API was required. VP5 consumes the existing public projector status/bind/reassign/remove registry API and the existing `LLPrismPreviewService` atlas publication API. No undo/history entry is created.
