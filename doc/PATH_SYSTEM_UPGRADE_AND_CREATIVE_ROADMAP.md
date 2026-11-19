# Actor Path System Upgrade and Creative Roadmap

## Scope

This pass turns the Path editor from a dense, fixed sheet into a staged
authoring tool and adds exact generated paths, per-path gait cadence, endpoint
generators, exact waypoint corner fillets, and depth-correct guide rendering.

## Implemented in this pass

### Readable, collapsible editor

The shared editor is now wider and organized into independently collapsible sections:

1. **Nodes & Playback** — waypoint list, pose-ghost look, edit operations, copy, and transport.
2. **Selected Node & Camera** — node timing/action plus the complete authored-camera workflow.
3. **Movement & Timing** — travel speed, foot cadence, smoothing, easing, terrain behavior, and end behavior.
4. **Shape Designer** — exact shape selection, endpoint generators, dimensions, phase/sweep, orientation, and rise.
5. **Choreography** — camera-take sync and follow-the-leader controls.

The standalone Actor Mover is wider, and the Director Path tab now gives the shared editor a wide content surface rather than forcing it into the old narrow column.

### Exact shape presets

The path engine now supports these exact whole-path shapes:

- Waypoints (centripetal Catmull-Rom)
- Circle / Arc
- Ellipse / Arc
- Helix
- Diamond
- Figure 8 (Gerono-style crossing curve)
- Sine Wave

Diamond, Figure 8, and Sine Wave use the same bounded adaptive arc-length compiler as ellipse and waypoint curves. Actors therefore travel by distance along the shape instead of by raw parameter, avoiding visible speed changes through tight portions of the curve.

### Two-point / endpoint generators

After placing two or more waypoints, **Fit Half Arc to Endpoints** and **Fit Sine Wave to Endpoints** use the first and last authored nodes as endpoints.

- Half Arc computes an exact semicircle through the endpoints, including a tilted plane when their heights differ.
- Sine Wave aligns its local axis to the endpoints, selects a useful default amplitude, and preserves their vertical difference through the Rise value.
- Existing node metadata is retained and redistributed along the generated path.
- Half arcs default to Stop; sine waves default to Ping-pong because a sine path is open and cannot loop without a teleport.

### Exact rounded waypoint corners

Select an interior waypoint and set **Corner radius (m)** above zero to replace
the sharp incoming/outgoing junction with a tangent circular arc. A city-block
turn is authored as three points: approach, corner pivot, and exit.

- Straight legs and circular fillets are evaluated as exact compiled pieces.
- A 90-degree turn with a 2 m radius begins and ends 2 m from its corner pivot.
- Oversized radii are reduced automatically to 49% of the shorter adjacent leg,
  preventing neighboring corner fillets from overlapping.
- Degenerate straight/U-turn requests are rejected with a visible message.
- Open paths may round interior nodes; loop paths may round every node, including
  the seam.
- The authored pivot remains draggable in edit mode. A tether and secondary
  marker show the traveled arc midpoint.
- Node dwell, animation, camera, and speed metadata trigger at the arc midpoint.
- Pose ghosts and camera leaders also use that traveled midpoint and its tangent.
- Turn speed is capped by `v = sqrt(2.5 * effective_radius)`. A continuous
  `v^2 = v_corner^2 + 2 * 2.5 * distance` envelope brakes on the approaching
  straight and accelerates away after the tangent point. The minimum envelope
  across all arcs handles consecutive corners and loop seams deterministically.
- Any active corner radius switches waypoint routing from tension-blended
  Catmull-Rom to exact straight/arc pieces. The smoothing slider is disabled
  while this mode is active.

### Per-path cadence

**Foot cadence ref** is stored per path rather than being only a global Actor Mover setting.

- Travel speed still controls metres per second along the route.
- Cadence controls the animation's design-speed reference used for foot-cycle timing.
- It updates a playing path live.
- Followers inherit the leader path's cadence.
- Scene save/load, copy-to-actor, undo, and redo preserve it.
- Scene schema 4 saves cadence, exact-shape identifiers, and per-node corner
  radii. Schema 1-3 paths remain compatible; older paths load with zero corner
  radius and preserve their previous geometry.

### Avatar and actor-ghost render priority

Path guides no longer render in the late UI-3D pass after the world depth buffer has been cleared. They are composited into the main scene target immediately after deferred lighting with depth testing enabled and depth writes disabled.

Consequences:

- Real avatars occlude path lines.
- Deferred actor ghosts occlude path lines.
- Props, terrain, and other scene geometry occlude path lines naturally.
- Later fallback/studio ghost overlays still paint over guides.
- The old cleared-depth duplicate draw has been removed.

## Verification completed

- Release compilation succeeded for all changed C++ translation units.
- The dedicated `INTEGRATION_TEST_alpathgeometry` Release target built successfully.
- All 15 geometry tests passed, including exact/clamped planar fillets, a
  tilted arbitrary-3D-plane fillet with endpoint, tangent, radius, and planarity
  assertions, and a 60-degree case that independently guards the
  `radius * tan(turn/2)` relationship.
- The adversarial tilted-endpoint test exposed an Euler-order error in the first
  half-arc fitter implementation. The fitter now derives its persisted Euler
  fields from an exact shortest-arc quaternion, and the 3D endpoint regression
  passes.
- All three edited XUI files parse as XML.
- All 64 `getChild()` bindings in `ALPanelPathEditor` resolve to unique control
  names in the rewritten shared panel.
- `git diff --check` reports no whitespace errors.

The complete Release target linked successfully and the viewer manifest copied
the updated `AlchemyTest.exe` plus runtime assets into
`build-Windows-vs2026-os/newview/Release`. Runtime QA below remains to be
performed in-world.

## Runtime QA checklist

### UI

- Open Actor Mover and Director > Path; confirm the accordion fills the wider content area.
- Expand every section independently and verify its content scrolls when multiple sections are open.
- Resize each host to its minimum and larger widths; check labels, editable slider fields, combo boxes, and the suspend banner.
- Confirm Edit in world, path camera preview, copy-to-actor, sync-to-take, and follow controls still respond.

### Shapes and generators

- Create each exact shape and verify numbered event anchors follow it.
- Change Extent X/Y, phase, sweep/cycles, yaw, tilt, roll, and rise; verify the guide and duration update immediately.
- Place two points at the same height and generate a half arc and sine.
- Repeat with endpoints at different heights.
- Reverse each new shape twice and verify geometry and node metadata return to the original ordering.
- Save and reload a scene containing each new shape and a non-default cadence.
- Confirm Sine Wave refuses Loop/Close Loop and defaults to Ping-pong.

### Rounded corners

- Place three waypoints in a 90-degree L and set the middle node to 1 m, 2 m,
  and an intentionally oversized radius.
- Verify tangent entry/exit, correct left/right turn direction, and automatic
  clamping without overlapping an adjacent rounded corner.
- Test a sloped 3D corner, two consecutive corners, and a loop with a rounded
  seam.
- Confirm the edit pivot remains draggable while the route midpoint, actor,
  pose ghost, node camera timing, and dwell event remain on the traveled arc.
- Reverse and mirror rounded paths; save/reload a schema-4 scene; undo/redo a
  radius change.
- Compare a tight and broad corner at high authored speed and verify the tighter
  radius receives the lower speed cap, braking begins before the entry tangent,
  and speed rises continuously after the exit tangent.

### Cadence

- Play two actors at the same travel speed with different cadence values.
- Adjust cadence while playing and verify foot-cycle rate changes without changing route speed.
- Copy a path and confirm cadence copies.
- Start a leader and follower and confirm the follower uses the leader's cadence.

### Render priority

- Place a path directly behind and through a real avatar; the body must occlude the guide.
- Repeat with a deferred entity-clone ghost and a studio/fallback ghost.
- Check HDR on/off, transparent water, terrain, alpha attachments, and a Prism display.
- Confirm Hide UI and Show Path retain their existing gating behavior.
- Check that path colors remain readable after HDR tonemapping and lens filters.
- Enable pose ghosts and confirm their own depth priming does not hide unrelated foreground geometry.

## Creative feature roadmap

### 1. Modifier stack — recommended next architecture

Treat the route as a base path plus non-destructive modifiers:

`Base: line/arc/waypoints -> Wave -> Bank -> Vertical lift -> Noise -> Ease profile`

This would let two points become much more than a special-case generator. A director could draw a line, then add Half Arc, Sine, S-curve, or handheld drift as adjustable layers. Modifiers could be reordered, disabled, copied, and keyframed without destroying the original points.

High-value modifiers:

- Arc bow: signed curvature between endpoints.
- Wave: amplitude, cycles, phase, falloff at ends.
- Vertical profile: hop, crest, dip, staircase, or custom height curve.
- Bank/lean intent: feed path curvature into body banking.
- Organic drift: seeded low-frequency noise for less mechanical blocking.
- Endpoint falloff: smoothly suppress a modifier near start/end.

### 2. More generated presets

- S-curve / double arc
- Spiral in/out
- Clover / three-lobed orbit
- Heart
- Rounded rectangle / racetrack
- Polygon with adjustable side count and corner rounding
- Camera dolly arc around Subject A
- Orbit target while translating
- Staircase / corkscrew with landings

### 3. Tangent handles and segment modes

Give each waypoint incoming/outgoing handles with Auto, Smooth, Corner, and Broken modes. This would solve exact doorway turns, pauses at marks, and hand-shaped camera-blocking curves without requiring more points.

### 4. Speed and cadence curves

Add a small curve strip along the path for:

- Travel speed
- Foot cadence
- Stride scale
- Body lean
- Look-at weight

Separating speed from cadence enables deliberate performance choices: hurried short steps, relaxed long strides, slow-motion travel with natural gait, or a stylized march.

### 5. Beat and take synchronization

- BPM grid and beat snapping for nodes and dwell events.
- "Arrive on beat/bar" solver that adjusts speed/ease without moving geometry.
- Named cue markers shared by actor paths, camera takes, lights, and animation switches.
- Time-stretch preview that reports whether the requested performance exceeds turn-rate or acceleration limits.

### 6. Record a performance into a path

Record a live avatar/ghost rehearsal, simplify it into editable waypoints, and preserve timing as a speed curve. A tolerance slider could trade fidelity for cleaner blocking.

### 7. Formation lanes

Generate parallel or radial lanes from one master path. Each actor gets an offset lane with corner compensation so a formation does not collapse or cause inner actors to outrun outer actors.

### 8. Clearance and staging diagnostics

- Capsule clearance preview along the path.
- Red/yellow heatmap for walls, low ceilings, steep slopes, or excessive curvature.
- Camera-frustum visibility heatmap: show when Subject A is blocked or leaves frame.
- Estimated turn-rate, acceleration, and foot-sliding warnings.

### 9. Path-relative events

Allow events at distance or normalized progress, not only at nodes:

- Animation/action cue
- Look target change
- Camera cut/ease marker
- Light cue
- Dialogue/emote trigger
- Formation change

Distance-relative events would remain stable when the shape is resized; time-relative events would remain stable when the take duration changes.

### 10. Reusable path assets

Save a path, its modifier stack, timing curves, cameras, and events as a named asset. Support applying it at an actor, between two marks, around a target, or aligned to the current camera.

## Recommended next slice

Build the **non-destructive modifier stack** with Arc Bow and Wave as the first two modifiers. It generalizes the two endpoint buttons, unlocks half arcs, S-curves, sine paths, and organic variation from the same system, and provides the right foundation for later speed curves and beat synchronization.
