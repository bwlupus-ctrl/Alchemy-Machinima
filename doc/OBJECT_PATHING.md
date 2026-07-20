# Object Pathing — client-side props on splines (evaluation + prototype)

Status: **prototype shipped** (`ALObjectPathMover`, 2026-07-20). One linked
set drives itself along an `LLActorMover::Path` client-side, with per-node
speed overrides (accel/decel) and a per-node yaw offset (skid). The sim is
never told — same local-only philosophy as the Actor Mover.

## The question

Can the viewer override a non-avatar object's *rendered* transform per frame,
reliably enough to drive a car along a spline — and does moving the linkset
ROOT carry the children?

## Evaluation — what the engine actually does

### 1. Interpolation does NOT fight a parked prop

`LLViewerObject::idleUpdate()` only moves objects when **all** of: the object
is non-static, `sVelocityInterpolate` is on, the object is not selected, and
`interpolateLinearMotion()` only integrates when the update-supplied
velocity/acceleration are **nonzero**. A parked, non-physical prop has zero
velocity: the viewer never touches its position between server updates. So a
client-side write is *not* re-fought every frame by interpolation — the only
thing that resets it is a fresh `ObjectUpdate`/terse update from the sim
(someone edits the object, physics wakes it, region restart). Re-asserting
per frame therefore wins for all practical purposes: a stray server update
can land for at most one frame before the next idle write paints over it.

This is *stronger* than the avatar case: `LLActorMover::applyOverride` must
re-assert against the avatar's own per-frame character update and
`LLControlAvatar::matchVolumeTransform()`; a static prop has no equivalent
per-frame writer at all.

### 2. The local-edit idiom is the placement mechanism

`LLManipTranslate` (build-tools drag before send) is the proven local move:
`object->setPositionAgent()` / `setRotation()` on the ROOT, then mark
drawables moved. Its `LLManip::rebuild()` also forces `REBUILD_VOLUME` and
recurses children — needed there because an edit can change geometry. A pure
move does **not** need the volume rebuild; the prototype does the cheap form:

```
root->setPositionAgent(pos);  root->setRotation(rot);
gPipeline.markMoved(root->mDrawable, /*damped*/false);
for each child: gPipeline.markMoved(child->mDrawable, false);
```

`markMoved(..., false)` forces `MOVE_UNDAMPED` (exact placement, no critical-
damp chase — `LLPipeline::markMoved` also guarantees parents process first).
The first move makes the drawables ACTIVE (spatial bridge), after which they
render through their transform; per-frame cost is move-list processing only,
no geometry rebuild.

### 3. Linked sets: yes, moving the root carries children — with one caveat

Child prim positions/rotations are stored parent-relative; their world
transforms derive from the root in `LLDrawable::updateXform`. But children
are **not automatically marked moved** when the root is (that is exactly why
`LLManip::rebuild` recurses). The prototype marks every child drawable moved
alongside the root — cheap and byte-faithful to the local-edit path.

### 4. What fights, and how much

| Threat | Verdict |
| --- | --- |
| Viewer interpolation | **No fight** for non-physical, zero-velocity props (see 1). |
| Server updates | Reset position/rotation when they arrive; repainted next idle (≤ 1 frame flash). A *physical* or scripted-moving object streams updates continuously — **do not path those**, the flashing would be constant. |
| Selection | A selected object skips interpolation anyway; but build-tools edits write positions — don't edit while driving. |
| Region crossing (object) | Non-physical objects don't cross regions; not a case. |
| Region crossing (viewer/agent) | Path nodes are region-GLOBAL (`Waypoint::mPosGlobal`), placement converts per frame — survives the agent's own crossings. |
| Derez / interest-list churn | The drive HOLDS (skips frames) while the object is unresolvable and resumes when it re-resolves; enrollment toggle is the deliberate exit. |
| KillObject | Drawable dies; resolve-per-frame guards it (drive holds forever until untargeted). |

**Feasibility verdict: solid for the intended case** — non-physical set
dressing (cars, carts, props) on flat or authored-height paths. Not suitable
for physical/scripted movers.

## Prototype (shipped)

- **Enroll**: right-click an object → *Director* → **Path Object** (any
  object; the clicked prim resolves to its linkset ROOT; avatars excluded —
  they have the Actor Mover). Toggling off stops the drive and drops the path.
- **Author + drive** (chat harness; the path-editor panel stays
  avatar-focused for now):
  - `/objpathadd` — append a node at each enrolled object's current root
    position (root-centre height, *not* ground-snapped — a car's path is
    authored at axle height; move the object with build tools between adds).
  - `/objpathdrive` / `/objpathstop` — start/release every enrolled drive.
  - `/objpathclear` — stop + drop all enrolled nodes.
  - `/objpathloop` — toggle loop end-mode (stop ⇄ loop; ping-pong is honored
    if set via the shared `Path::mEndMode`).
  - `/objpathspeed <m/s>` — nominal path speed.
  - `/objpathskid <deg>` — skid yaw offset on the LAST added node.
- **Motion model** — deliberately shared with the walk, no parallel math:
  - Paths live in `LLActorMover::mPaths` keyed by the **object's UUID**
    (`editPath`/`getPath`; centripetal Catmull-Rom + arc-length table).
  - Speed = `LLActorMover::evalPathSpeed` — the *same* per-node
    override blend + ease-in/out the avatar walk uses (accel/decel comes free
    from `mSpeedOverride` + `mEaseIn/Out`).
  - Facing = path tangent yaw + interpolated per-node
    `Waypoint::mYawOffset` (**new field**, the skid) −, all applied
    *relative* to the start: the drive captures the object's rotation and the
    start tangent, then applies `Rz(Δyaw) · R0`. Whatever forward-axis
    convention the model uses (X-forward, Y-forward, askew) and whatever tilt
    it had, it keeps them — no axis-convention guessing.
  - Per-frame drive from the main idle loop (`llappviewer`, next to the
    Actor Mover state machine), so placement always lands before the render.

## Known gaps / full-design recommendations

1. **No suspend/resume UX** — an unresolvable object just holds. Fine for a
   prototype; the full design should mirror the walk's suspend banner.
2. **No ground-follow / pitch-to-slope** — nodes carry their authored height.
   For a car on terrain, the full design should add the walk's ground raycast
   with a per-path enable, plus pitch/roll from the surface normal.
3. **Stop leaves the prop where it stopped** (client-side there is no
   server-true pose to snap back to). Full design: capture the pose at
   enrollment and offer "reset to true position" (mirror of Reset-to-Marks).
4. **UI**: promote the chat harness into the Path tab — the editor panel's
   list/inspector generalize cleanly (the skid field slots in next to the
   per-node speed spinner); needs a "prop mode" that hides the anim/gaze
   rows. Director-superset rule applies when that lands.
5. **ACTION/CUT integration**: enrolled drives should arm like actor moves
   (`DirectorArmMoves` sibling), with the staggered-start machinery reused.
6. **Sync-to-take** (`mSyncToTake`) would work unchanged — the arc clock is
   the same; wire it when the UI lands.
