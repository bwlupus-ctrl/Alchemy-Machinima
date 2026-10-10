# Custom Water Level

## Behavior

`RenderWaterLevelOverride` enables one absolute viewer-side water level for
every loaded region. `RenderWaterLevel` stores that level in meters. Both
settings have `Persist = 0`, so the override is session-only and starts
disabled at every login. With the override disabled, simulator water is passed
through unchanged.

The slider and adjacent numeric field cover 0–10000 m.
Values are made finite and clamped once, in
`LLViewerRegion::sanitizeVisualWaterHeight()`. **Set to camera height** uses
the camera altitude and enables the override. **Reset** disables the override
and restores each region's latest simulator water level.

The effective height remains the value returned by
`LLViewerRegion::getWaterHeight()`, so existing water geometry, fog,
reflection/refraction, alpha sorting, exclusions, probes, and other consumers
stay on the same plane. This intentionally also means swim/AO state and audio
follow the custom level; there is no separate simulator-water camera test or
special-case behavior.

## Update behavior

Setting changes only queue work. `LLWorld::updateRegions()` applies the newest
value to all regions once per frame and rebuilds shared edge/hole water once,
which avoids strobing while dragging. Reflection-probe content is reset only
after the level has remained unchanged for about 250 ms. Region probe origins
are moved when the plane moves.

New regions initialize their water object from the already-selected effective
height instead of briefly using the 20 m default. Later simulator water updates
are retained underneath the override and become visible immediately when it is
disabled.

A camera below a very high water plane is underwater, so dark underwater fog
is expected. For a useful view of a high plane, place the camera near or above
that altitude.

## Checks

- Leave the toggle off and verify ordinary simulator water is unchanged.
- Enable it and drag the slider across region borders; all visible planes and
  fog transitions should move together without flashing.
- Type values between 4096 and 10000, touch the slider without a downward
  clamp, and try Set to camera height and Reset.
- Discover or enter a new region while enabled; it should appear directly at
  the custom level without a 20 m pop.
- After dragging stops, allow at least 250 ms for reflection probes to reset.
- Run `scripts/code_tools/xui_layout_check.py` on both edited XUI files; each
  must report zero findings.
