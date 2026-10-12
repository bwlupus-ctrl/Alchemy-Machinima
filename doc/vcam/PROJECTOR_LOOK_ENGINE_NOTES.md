# VCam projector look: engine notes

## Data and compatibility

`LLPrismLens::ProjectorSettings` now owns `mLook` (`ProjectorLook`) plus the
orientation fallback fields `mFlipH`, `mFlipV`, and `mRotate` (clockwise quarter
turns, 0-3). The fallback is necessary because `ScreenEffects::mRotate90` is a
boolean rather than a four-state rotation. The fallback orientation is composed
with `mEffects` before the existing face-composite sampler runs, so fit, anchor,
orientation, and every TV effect share the display-face mapping.

The scene settings block persists `look` with the contract field names in
snake_case. Missing `look` and missing fallback-orientation keys retain their
defaults, so pre-look scenes retain the old feed and bind paths. A completely
default `ProjectorLook` selects the original projector-feed shader family and
does not add a pass or a uniform upload.

## Rendering

Static look work remains one RGBA8 conversion pass followed by the existing mip
generation. Keystone is a bounded projective trapezoid warp with opaque black
outside. Hotspot is a radial lamp falloff. Lamp colour uses a cheap black-body
approximation divided by its 6500 K result, then multiplies the gel. Dust,
scratches, edge burn, and grain are analytic hashes seeded from presentation
time. Halation is a gated four-tap highlight blur in the same pass; no texture or
intermediate target is allocated. The existing Tracy CPU/GPU zone remains
`VCam projector feed prep`.

Shutter flicker, gate weave, and CAMERA_FOV throw are bind-time arithmetic only.
They reuse the existing gobo time, tint, pattern, and pattern-parameter uniforms:
shutter multiplies tint; a private negative texture marker carries weave offset
and FOV scale. Thus they animate every main frame without republishing or
re-preparing the camera feed and add no allocation or texture bind. OFF and
LIGHT_FOV preserve the current cone mapping. CAMERA_FOV compares vertical
half-angle tangents and both camera/projector aspects, producing a centred crop
or black exterior where the camera field is narrower than the light cone.

## Verification boundary

Unit coverage exercises ProjectorLook LLSD round-trip, default-path selection,
exact 6500 K identity, and exact zero-keystone identity. Per request, no build or
runtime GPU measurement was performed; the 0.15 ms target still requires the
512-square Tracy acceptance run on representative hardware.
