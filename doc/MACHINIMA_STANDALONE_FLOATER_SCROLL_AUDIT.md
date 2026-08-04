# Machinima standalone floater scrolling audit

Status: audited fixes implemented in XUI/source; static verification only and
no build/client launch performed

Repository snapshot reviewed: 2026-08-04

## 1. Scope and method

This audit covers the custom machinima/production surfaces registered together
in `llviewerfloaterreg.cpp`, plus Animation Explorer, Lightbox, Phototools, Quick
Settings, and the Director Console that embeds several of the same panels. It is
the Camera-menu machinima surface audit, not a claim that every viewer floater
has been audited. Stock upstream floaters and unrelated fork-specific tools such
as Animation Overrider and Particle Editor (`llviewerfloaterreg.cpp:613` and
`:633`) are explicitly outside this change set.

For each surface, inspect:

- default and declared minimum dimensions;
- the lowest reachable child at the minimum height;
- `follows` behavior when a saved rectangle is restored smaller than default;
- ownership of vertical scrolling;
- nested lists, previews, and controls that must retain their own wheel input;
- whether the content body reserves the vertical-scrollbar gutter.

This is static evidence. The final fix still needs interactive regression testing
at default/minimum/saved rectangles and 100/125/150/200-percent UI scale.

## 2. Findings

| Surface | Result | Static evidence |
|---|---|---|
| Cinematic Camera | Partial, P1 | Both pages have vertical scrollers (`floater_cinematic_camera.xml:35-63` and `:75-103`). The Camera Shake body is 320 px wide but embeds a 340 px operator at `:84-101`; operator reset controls reach x=332 in `panel_cinecam_operator.xml:62-66`, under or beyond the scrollbar gutter. |
| Flycam Recorder | Pass | Fixed 330x340 shell contains a 320x314 panel at y=20-334 (`floater_flycam_recorder.xml:4-23`). |
| Flycam Orbit | Pass | Fixed 320x240 shell contains a 320x214 panel at y=20-234 (`floater_flycam_orbit.xml:4-23`). |
| Actor Mover | Fail, P0 | Resizes from 1310 to 440 with no outer scroller (`floater_actor_mover.xml:2-11`). Its Actor Mover panel and Path editor extend to about y=1282 (`:71-103`); at minimum height even the shared Walk/Stop row (`panel_actor_mover.xml:300-310`) is clipped and the Path editor is unreachable. The default is also taller than common displays. |
| Animation Explorer | Pass | Its minimum equals the complete 676x374 layout; list, preview, and bottom UUID actions remain inside it (`floater_animation_explorer.xml:13-27`, `:39-88`, `:103-134`). |
| Ghost Studio, standalone | Partial, P1 (vertical pass) | The host follows all and its four settings pages own proper scrollers (`floater_ghost_studio.xml:5-24`; `panel_ghost_studio.xml:50-64`, `:115-123`, `:202-207`, `:291-296`). However, the floater permits width 370 while the shared panel was authored at 500. At minimum, its two right-followed header controls move from x=252-364 to roughly x=102-214 and overlap the fixed x=8-168 title on the y=5 row (`panel_ghost_studio.xml:14-22`). |
| Prop Mover | Fail, P0 | The host allows height 330 without a scroller (`floater_prop_mover.xml:5-24`). Its followed body shrinks to about 302 px, but path-wide actions start at y=310, transport at y=340, and status ends at y=422 (`panel_prop_mover.xml:165-269`). |
| Temporal Capture | Partial, P2 | The host allows height 320 without scrolling (`floater_temporal_capture.xml:6-27`). Interactive controls fit, but the 40 px explanatory text at body y=286-326 clips when the body shrinks to about 294 px (`panel_temporal_capture.xml:253-276`). |
| Director Console | Fail, P0 | The root may shrink from 650 to 560 (`floater_director.xml:8-19`), reducing the tab viewport to about 458 px. Move reaches y=538 without a page scroller (`:314-470`); Animate reaches y=504 without one (`:571-798`); and the 534 px Ghost Studio embed follows only left/top/right (`:518-543`), so the host itself clips even though its child pages scroll. Path, Camera flow, and Weather already demonstrate the correct local-scroller pattern (`:472-516`, `:1153-1337`, `:1478-1519`). |
| Lightbox Settings | Fail, P0 | The 860 px floater permits a 500 px minimum and contains an 830 px followed tab container with zero `scroll_container` children (`floater_lightbox_settings.xml:2-23`). Proj Shafts reaches roughly y=1115 and clips even at default size; Rendering reaches roughly y=546; Froxel Air roughly y=654; and Weather embeds a fixed 710 px panel (`:4241-4251`). |
| Phototools | Partial, P1 | At its 280 px minimum (`floater_phototools.xml:19-40`), the 250 px tab container yields about a 227 px Focus page after the tab strip. Guide Opacity remains around y=219-239, so its lower 12 px clip; the bottom-followed Lightbox button moves to roughly y=194-217 and does not overlap it (`:321-357`). |
| Quick Settings | Pass | The fixed-height shell is 159 px (`floater_quick_settings.xml:2-19`); actual shared controls end near y=126 despite the panel's stale larger declared height (`panel_quick_settings.xml:189-223`). |

## 3. Recommended fixes

### 3.1 Lightbox Settings

Give every tab whose fixed content can exceed the tab viewport its own
`scroll_container follows="all"`. Put the controls inside a definite-height body
that follows left/top/right but not bottom. Weather, Proj Shafts, Froxel Air, and
Rendering are mandatory; audit every remaining tab while converting the file.

Do not wrap the entire `tab_container` in one outer scroller. Page heights differ,
and one global wheel owner makes nested controls harder to use.

### 3.2 Actor Mover

Do not preserve the 1310 px monolith. Keep the roster/status area fixed, then
give the Move transport and Path editor separate scroll ownership, preferably as
tabs or a vertical layout with one active body. The path editor's own list keeps
its native scroll behavior. Use a practical default near 700 px and retain a
minimum near 440 px only after every action remains reachable.

### 3.3 Director Console

- wrap Move's fixed-height content in a page-local scroller;
- wrap Animate's fixed-height content in a page-local scroller;
- make the embedded Ghost Studio host `follows="all"` so its internal scrollers
  receive the actual viewport;
- leave the already-scrolling Path, Camera flow, and Weather pages structurally
  unchanged.

The Props panel's declared body reaches y=478, but its last real control ends at
y=430 after the Director inset, inside the minimum viewport; it needs runtime
regression coverage, not a scroll repair based on the unused panel tail.

### 3.4 Prop Mover and Temporal Capture

Prop Mover should either use a fixed-height scrolled body or restore its minimum
to the full 452 px. Raising the minimum is the lower-risk first repair because
the panel contains two lists that should continue to own wheel events.

Temporal Capture can safely restore `min_height="358"`; use scrolling only if
the product requires a shorter surface. Do not silently clip the explanatory
text at the declared minimum.

### 3.5 Cinematic Camera, Phototools, and Ghost Studio

Set the Camera Shake inner body and embedded operator to the same usable width,
leaving the scrollbar gutter clear. For Phototools, either give Focus a page-local
scroller or restore the 330 px minimum; the latter is the smaller, safer fix.

Ghost Studio's vertical tab scrolling is already sound. Reflow its fixed header
controls for the declared 370 px minimum or raise the minimum width to the first
verified non-clipping value; do not add another outer vertical scroller.

## 4. Floater layout contract

New and revised production floaters should follow these rules:

1. A resizable root declares a minimum at which every action is reachable.
2. A variable-height settings region owns a `scroll_container follows="all"`.
3. The scroll body has a definite content height and follows left/top/right,
   never bottom or vertical `all`.
4. Sticky selection, action, or status regions sit outside the settings scroller.
5. A `scroll_list`, preview, or other wheel-consuming widget is not unnecessarily
   nested inside a giant outer wheel scroller.
6. Inner content reserves the scrollbar width; right-edge buttons never extend
   into the gutter.
7. Selection changes, programmatic reveals, and validation errors call
   `LLScrollContainer::scrollToShowRect()` for the newly relevant control.
8. Every focusable descendant in a newly scrolled document registers a
   focus-received callback. Convert its local rectangle into the direct document's
   coordinates before `scrollToShowRect()`; the container does not automatically
   reveal keyboard Tab focus.
9. Every direct child of a resizable `tab_container` declares an explicit design
   width and height. Without that construction-time rectangle, the 10 px fallback
   height corrupts `follows="bottom"` and vertical `follows="all"` offsets before
   the tab container performs its final reshape.

## 5. Required regression matrix

For every changed floater:

- open at default, declared minimum, and a previously saved minimum rectangle;
- test 1280x720 and 1920x1080 at 100, 125, 150, and 200 percent UI scale;
- reach every control by mouse wheel, scrollbar thumb, and keyboard Tab;
- verify focus is scrolled into view after selecting a row or exposing an error;
- place the pointer over embedded lists and previews and confirm their intended
  wheel behavior wins;
- switch every tab at minimum size and confirm no overlap, dead area, or clipped
  sticky action/status region;
- repeat after localization with longer labels.

## 6. Implementation status

The audited XUI/C++ repairs are now implemented for Actor Mover, Director,
Lightbox, Cinematic Camera, Ghost Studio, Prop Mover, Temporal Capture,
Phototools, and Prism Manager. Shared keyboard-focus reveal lives in
`alscrollfocus.h`. XML parsing, control-name preservation, content-height checks,
and `git diff --check` pass.

No build or client launch was run. The interactive matrix above remains required
for nested mouse-wheel arbitration, localization, saved rectangles, and high UI
scale behavior.
