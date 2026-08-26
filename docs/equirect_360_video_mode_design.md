# Realtime 360° Equirectangular Video Mode — Focused Design

## Requirement

When 360 Video Mode is enabled, render the world as one flat, monoscopic,
equirectangular 360° panorama every video frame.

The panorama must:

- cover the full sphere around the current camera;
- be an exact 2:1 image;
- contain every direction from the same simulation frame;
- join continuously at its left and right edges; and
- retain the viewer's existing world-rendering behavior and visual quality.

This is a projection mode, not a new recorder. The existing video recorder records
the resulting flat panorama exactly as it records the normal viewport.

## Non-goals

This mode does not add or change:

- video encoding or recording behavior;
- spherical-video metadata;
- stereo or omnidirectional stereo;
- the existing 360° still-capture workflow;
- reflection-probe behavior;
- lighting, shadows, materials, atmosphere, water, avatars, actors, or effects;
- window size, position, or fullscreen state; or
- the normal perspective renderer while the mode is disabled.

## Per-frame render path

For each output video frame:

1. Update the world once.
2. Freeze the render-time camera, animation, lighting, environment, particles, and
   other time-dependent state for this output frame.
3. From the same camera position, render six 90° square views covering
   `+X`, `-X`, `+Y`, `-Y`, `+Z`, and `-Z` into a cubemap.
4. Do not advance the world between cube faces.
5. Reproject the completed cubemap into one exact 2:1 equirectangular render target.
6. Present that flat panorama as the frame that the video recorder captures.

"Rendered at once" means all 360° directions belong to one output frame and one
world state. The GPU may perform six internal face passes, but no face is displayed
or recorded separately.

## Equirectangular reprojection

For each output pixel, convert its panorama coordinate to a direction and sample
the cubemap by that direction:

```glsl
vec2 uv = (gl_FragCoord.xy + vec2(0.5)) / outputSize;

float longitude = (uv.x - 0.5) * (2.0 * PI);
float latitude  = (0.5 - uv.y) * PI;
float ring      = cos(latitude);

vec3 direction =
    cameraForward * (ring * cos(longitude)) +
    cameraRight   * (ring * sin(longitude)) +
    cameraUp      * sin(latitude);

vec3 color = texture(sceneCube, direction).rgb;
```

The center of the panorama is the current camera-forward direction. The top and
bottom are the camera's up and down directions. The implementation must match the
viewer's existing cubemap face orientation and OpenGL framebuffer origin.

## Seamless left/right edge

The panorama is periodic horizontally. Longitudes `-π` and `+π` are the same world
direction, so the left and right sides meet without a stitched boundary.

The implementation requirements are:

- sample one completed cubemap by direction rather than assembling flat face
  rectangles;
- enable seamless cubemap filtering;
- render every cube face from the same world state and with identical render
  settings;
- perform exposure, tone mapping, and other full-frame color operations only after
  the cubemap has been reprojected, when practical;
- make any panorama-space filtering wrap horizontally; and
- never clamp the panorama's horizontal sampling at its left or right edge.

Seamless cubemap filtering fixes sampling across cube-face boundaries. Synchronized
world state and identical face rendering prevent moving objects, animated water,
particles, shadows, or exposure from disagreeing across those boundaries.

## Preserve the existing renderer

The six directions must use the normal full-quality world renderer. The existing
`gCubeSnapshot`/probe paths are useful references for cubemap orientation and GPU
resources, but they must not define this mode's visual behavior because those paths
intentionally omit or alter some normal-view effects.

Use a dedicated 360 render context that:

- changes only the camera projection and render destination;
- saves and restores all camera, viewport, framebuffer, culling, and render state;
- uses the normal scene's lighting and effect decisions for all six directions;
- prevents reflection-probe updates from recursively starting during the six
  panorama passes; and
- releases its dedicated targets when the mode is disabled.

When `RenderEquirect360Mode` is off, execution follows the existing perspective
render path without allocating or rendering the panorama resources.

## Output dimensions

The panorama render target is always exactly 2:1, for example `4096×2048` or
`2048×1024`. Cube-face resolution is an internal quality setting; it does not
change the panorama's projection.

If the live window is not 2:1, display the target without stretching it. This is
only presentation of the already-rendered panorama and must not resize the window
or change the world renderer. A recorder intended to produce a standard 360° file
must capture the exact 2:1 panorama region.

## Minimal settings

- `RenderEquirect360Mode` — enables the mode; default is off.
- `Equirect360Width` — width of the 2:1 output target; height is always width/2.
- `Equirect360FaceSize` — square cubemap-face resolution used for quality/performance.

No stereo setting or alternate capture mode is part of this feature.

## Acceptance criteria

The mode is complete when:

1. Every recorded frame is one flat, exact 2:1, full-sphere panorama.
2. The left and right edges join continuously when viewed with horizontal wrapping.
3. A moving object crossing any cube boundary does not split across time.
4. Camera yaw, pitch, and roll orient the panorama correctly.
5. The world retains the same supported lighting, materials, atmosphere, water,
   avatars, actors, and visual behavior as the ordinary renderer.
6. Enabling the mode does not resize or move the application window.
7. Disabling the mode restores the untouched normal perspective path.

## Implementation summary

One world update, six synchronized camera directions, one cubemap, one
cube-to-equirectangular shader pass, and one flat 2:1 video frame. No stitching
stage and no changes to the world's rendering behavior.
