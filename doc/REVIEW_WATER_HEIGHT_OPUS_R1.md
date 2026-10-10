# Review R1: Local Water Height Offset (`RenderWaterHeightFudge`)

Reviewer: Opus (adversarial). Scope: the uncommitted water-height diff on
`feature/director-kit-p0` (settings.xml, llfloaterwateradjust.cpp,
llpaneleditwater.cpp, llviewercontrol.cpp, llviewerregion.cpp/.h,
floater_adjust_water.xml, panel_settings_water.xml, doc/WATER_HEIGHT_LOCAL.md).
Director-kit files were not reviewed.

---

# R1b: user clarification (supersedes the original brief)

> "the water height is just visual only... just if I wanted to bring the water
> line up to the skybox."

This adds two requirements:

1. **Strictly visual.** Water height must not change any animation, agent
   state, sound, physics, server message or LSL-visible effect. Any non-visual
   consumer that sees the new height is a BLOCKER.
2. **Skybox reach.** The water line must reach skybox height, which is
   typically 300–4000 m and sometimes higher.

## R1b verdict: FIX-NEEDED (redesign)

There are 3 BLOCKERS. The R1 render plumbing (one `LLSurface` source, no
double apply, no server write-back) is still correct and is reused below.

### BB1. A ±100 m offset cannot reach a skybox

**Evidence.** The offset is limited to ±100 m in three places:

- `llviewercontrol.cpp:1003`: `llclamp(..., -100.f, 100.f)`
- `llviewerregion.cpp:856`: the same clamp
- the slider `min_val="-100"` / `max_val="100"` in
  `floater_adjust_water.xml:318-319` and `panel_settings_water.xml:166-167`

With a default sim water level of 20 m, the highest reachable plane is 120 m.
That fails the core promise.

**Recommended design: replace the offset with an absolute custom level.**

Use two settings:

- `RenderWaterLevelOverride` (BOOL, default off). Off means simulator heights,
  which is today's code path and identical in behaviour.
- `RenderWaterLevel` (F32, metres, absolute).

When the override is on, **every** loaded region's `LLSurface` water is set to
that one absolute value. `mOriginalWaterHeight` keeps the simulator value for
the non-visual consumers listed under BB2.

UI, in Edit Water and later the Director Water card:

- a checkbox, "Custom water level (local only)"
- a spinner/slider in metres with **0–4096 on the slider** and a typed value
  up to **10000**
- **Set to camera height** (camera Z), the main button for skyboxes: fly the
  camera to the skybox floor and click
- **Set to avatar feet** (agent Z minus half the avatar height)
- **Reset** (turns the override off)

Why absolute rather than offset (or both):

- **Reaching skybox heights.** An offset has to be computed by the user
  (skybox Z minus sim water), and it differs per region.
- **Continuous horizon.** One absolute level gives every neighbour region and
  the void/edge water the same plane. From 3000 m, neighbours with different
  sim levels (20 / 25 / 0 m) would otherwise show as steps or seams in the
  water. The edge and void water already follow only the agent region's height
  (`llworld.cpp:954, 968, 1024`), so offsetting neighbours separately
  guarantees a seam where they meet.
- **Idempotent.** Setting an absolute value has no double-apply class of bug.
- **Flood use case still covered.** "Set to avatar feet" plus a nudge covers
  flooding a ground set. A separate offset mode adds UI without a use case;
  drop it.

What the renderer can support: there is no viewer-side clamp anywhere on the
water path. `LLSurface::setWaterHeight` (`llsurface.cpp:1143-1155`), the
`LLVOWater` placement (`llvowater.cpp:60, 190`) and `getVisibleExtents`
(`pipeline.cpp:4079`, ±65536 bounds) are all unclamped. At 10 km, F32
agent-space precision is about 1 mm. So 0–10000 is safe. The sanitiser should
only reject non-finite values; see R1 m5, since NaN passes `llclamp`.

Persistence: set `Persist 0`, or keep it persisted with a login warning (see
R1 M2). A forgotten 3000 m level puts every ground location underwater: black
fog and no sky.

### BB2. Non-visual consumers follow the visual plane

At skybox heights the whole ground is under the plane, so every one of these
fires all the time. All are BLOCKERS under rule 1.

| Consumer | Effect at skybox level | Evidence |
|---|---|---|
| AO swim/float swap | Flying anywhere on the ground plays **server-broadcast** swim animations | `llvoavatar.cpp:4151-4163` → `aoengine.cpp:284-333` (`sendAnimationRequest`) |
| Stand-up brush animation | Server animation suppressed on the ground | `llagent.cpp:2794` (`!mBelowWater`) |
| Sound rolloff | Camera under the plane gives **5× rolloff**: all world sound muffled | `llvieweraudio.cpp:421-430` (`cameraUnderWater()`) |
| Wind audio | Altitude shape uses camera height minus the visual plane: muted/underwater wind on the ground, sea-level wind in the skybox (altitude boost lost) | `llvieweraudio.cpp:596` → `llaudioengine_fmodstudio.cpp:668` |
| Avatar `mBelowWater` (cloth hover/frequency, Pose Polish contact gate) | Every ground avatar is "underwater": cloth floats, Pose Polish grounding is skipped | `llvoavatar.cpp:3485, 3509, 5308` |
| Minimap dot colours (informational) | Every object shows as "below water" | `llviewerobjectlist.cpp:1904-1957` |
| Region diagnostic | Prints the visual value as if it were the region's | `llviewermenu.cpp:4376` |

**Fix: one rule, applied once.** Avatar, agent and audio state uses the
simulator height. Rendering uses the visual height.

1. In `LLVOAvatar::idleUpdateBelowWater` (`llvoavatar.cpp:4156`), use
   `getRegion()->getOriginalWaterHeight()`. This one line covers AO, brush,
   cloth and Pose Polish, because they all read `mBelowWater`. No second flag
   is needed. This supersedes the R1 B1 two-flag proposal and is simpler.
2. Add `bool LLViewerCamera::cameraUnderSimWater() const`, the same as
   `cameraUnderWater()` (`llviewercamera.cpp:718-739`) but comparing against
   `getOriginalWaterHeight()`. Use it at `llvieweraudio.cpp:421`.
3. In `llvieweraudio.cpp:596`, subtract
   `gAgent.getRegion()->getOriginalWaterHeight()`.
4. In `llviewerobjectlist.cpp:1904`, use `getOriginalWaterHeight()`.
5. In `llviewermenu.cpp:4376`, print both values.
6. Rename `mOriginalWaterHeight` to `mSimWaterHeight` /
   `getSimWaterHeight()`, so that future code chooses deliberately.

**Coverage check (R1b item 3).** I searched every water-height reader
(`getWaterHeight(`, `getRenderWaterHeight(`, `cameraUnderWater(`,
`sUnderWaterRender`, `mBelowWater`, `DEFAULT_WATER_HEIGHT`) across
`indra/newview` and `indra/llaudio`. After fixes 1–5, no non-visual consumer
reads the visual value.

- **Particles:** client-simulated, with no water logic (no hits in
  `llviewerpartsim`).
- **Footstep and collision sounds:** no water dependency.
- **Server and LSL:** nothing sends a water height. The region terrain tab uses
  `LLRegionInfoModel::mWaterHeight` (raw message,
  `llfloaterregioninfo.cpp:614/2004`), and `llWater()` is server-side.
- **Physics:** the viewer runs none.

Deliberately left on the **visual** height, since they are render or local
presentation:

- `sUnderWaterRender` and the fog, clip and alpha splits
- the camera ±0.2 m surface clamp (`llviewercamera.cpp:109-126`)
- name-tag lift (`llvoavatar.cpp:4083`), which only triggers within about 0.5 m
  of the surface
- the parcel overlay
- probe placement
- ghost placement (`alghostplacementresolver.cpp:244-270`). Clicks on terrain
  under the plane re-anchor to the visible water surface unless
  `mAllowSeabedPlacement` is set. This is consistent with what the user sees,
  but call it out in the doc.

### BB3. A slider drag rebuilds hole water and wipes probes every tick

This is R1 M1, raised to BLOCKER. With a 0–4096 m slider, one drag sweeps
thousands of metres. Each tick:

- rebuilds hole water N times (`llsurface.cpp:1151-1153` →
  `llworld.cpp:951-974`)
- wipes every reflection probe (`llviewercontrol.cpp:1017` →
  `llreflectionmapmanager.cpp:3621-3643`)

The fix is unchanged from R1 M1:

1. Batch the region updates and call `updateWaterObjects()` once.
2. Debounce the probe reset (about 250 ms after the last change).
3. Also commit the spinner on Enter or release, not on each keystroke.

## R1b: hidden-limit hunt at high altitude (300–10000 m)

| # | Area | Finding | Severity / fix |
|---|---|---|---|
| H1 | Clamps | The only clamps are the new ±100 ones (BB1). `LLSurface::setWaterHeight`, `LLViewerRegion`, `LLVOWater`, `getRenderWaterHeight` and the shaders have none. The parcel overlay's `llclamp(camera_height, 0, 100)` (`llviewerparceloverlay.cpp:692`) only scales line pull and has no effect on water. | BB1 |
| H2 | Region water object | `LLVOWater` is a 256×256 quad with scale Z 0 (`llvowater.cpp:60`). Vertices are at `pos - scale/2`, so the Z equals the position Z (`llvowater.cpp:190`). No height limit. | OK |
| H3 | Hole/edge (void) water | Placed at `256 + water_height`, scale Z 512, so the vertex Z equals `water_height` (`llworld.cpp:966-969, 1024-1025`). It uses **only the agent region's** height (`llworld.cpp:954`), which is why BB1 recommends one absolute level for all regions. With absolute mode it is seam-free at any height. | OK with BB1 |
| H4 | New regions before the handshake | `LLSurface::initTextures` places water at raw `DEFAULT_WATER_HEIGHT` (`llsurface.cpp:249-251`). From a 3000 m skybox, every newly connected neighbour shows its plane **2980 m down** until its handshake arrives, then jumps. Any probe captured in that window keeps it. Raised from R1 m1 because the jump is now kilometres. | MAJOR. In `initTextures`, if the override is on, place the object at `RenderWaterLevel` directly (no `updateWaterObjects` from the constructor path). |
| H5 | Clip plane / near-far | The water clip plane is built from `getRenderWaterHeight()` with no distance limit (`pipeline.cpp:4141-4165`). PBR has no planar reflection target. Water reflections come from probes and SSR, so there is no reflection render-target near/far issue. `LLWaterPartition` has infinite far clip (stock), but the GPU far plane still applies. Viewed from the ground, a plane at 3000 m is beyond `RenderFarClip` and is **not drawn**, while the camera is still flagged underwater. This is inherent; document "view from at or above the water level". | OK / doc |
| H6 | Underwater fog, whole ground submerged | `getWaterFogViewNoClip` attenuates by eye depth: `t1 = -kd*pow(0.98, ks*e0)` with `e0 = -waterPlane.w` (`waterFogF.glsl:53-77`). At 3000 m depth that goes to about 0, so a camera on the ground sees **black fog and no sky**. From the skybox (camera above the plane), `e0 = 0` and refraction to terrain kilometres below gives an opaque fog colour, which is the desired "ocean" look. The ground behaviour is correct physically but surprising. | MINOR. Document it, and show a one-line notice in the panel when the camera is more than 50 m under the custom plane. Do not alter the shader. |
| H7 | Reflection probes "above water" | Automatic region probes sit at `max(water, terrain) + 2 m` (`llviewerregion.cpp:1346-1347`). With a 3000 m plane, all automatic probes in every region move to 3002 m. That suits the skybox (water reflections and interior ambient get probes at the floor). Ground scenes then have no automatic probes, only the default probe. Probe capture uses the probe origin for underwater state (`llviewerdisplay.cpp:1483`), so captures are correct per probe. | OK by design; document. Note that any level change re-places 64 probes per region and forces recapture, which BB3's debounce covers. |
| H8 | Alpha water split | Groups are rejected by world-space extent Z against `getRenderWaterHeight()` (`lldrawpoolalpha.cpp:424-481, 1299`). Absolute agent-space Z, no limit. | OK |
| H9 | Frustum/draw distance at altitude | Water partitions use infinite far clip. Edge water extends about 2.5 km past the ±512 m box (`llworld.cpp:930, 1028-1032`). From a skybox camera this is the same horizon as at sea level, with no altitude-specific cull. | OK |
| H10 | Shadows | Shadow extents come from visible partitions with ±65536 bounds (`pipeline.cpp:4077-4112`). Nothing water-height specific. | OK |
| H11 | Precision | At 4096 m the F32 ulp is about 0.5 mm, and about 1 mm at 10 km. The shader's `waterHeight` is the camera-to-plane delta (`lldrawpoolwater.cpp:230-232`), so it is small near the plane. Water UVs are world XY. | OK |
| H12 | Parcel property lines are cached | Edge Z is baked at `max(ground, water)` in `addPropertyLine` (`llviewerparceloverlay.cpp:530-533`), and the listener never dirties it. After a level change, property lines stay at the old height until something else dirties the overlay. | MINOR. Call `dirtyHeights()` (`llviewerregion.cpp:1139-1146`) in the apply path. |
| H13 | `RenderWater=FALSE` | `LLSurface::getWaterHeight` returns raw `DEFAULT_WATER_HEIGHT` (`llsurface.cpp:1171`), so the override is ignored for fog and underwater state. There is no plane to see, so this is moot. | OK |

## R1b fix order

1. BB2: the five sim-height edits; one line covers the avatar.
2. BB1: absolute override, with "Set to camera height" and "Set to avatar feet".
3. BB3: batch and debounce.
4. H4: pre-handshake placement.
5. Persistence decision (R1 M2).
6. H12, H6 notice, and the R1 minor items.

---

# R1 (original brief: ±100 m offset), kept for reference

## Verdict: FIX-NEEDED

1 BLOCKER, 2 MAJOR, 7 MINOR. The render plumbing is sound: every visual
consumer reads the same adjusted `LLSurface` height, and nothing is applied
twice. The problems are elsewhere:

- The visual offset leaks into network-visible avatar behaviour (AO swim
  switching, the stand-up "brush" animation). That breaks the core promise that
  only the creator's viewer sees the change.
- The settings listener is not built for a slider that fires on every drag
  tick.
- The persistence default turns a set tweak into a silent permanent change to
  the world.

---

## What was verified as correct (no action)

- **Single source of truth.** `LLViewerRegion::setWaterHeight` stores the
  simulator value in `mOriginalWaterHeight` and installs `original + offset`
  into `LLSurface` (`llviewerregion.cpp:853-867`). `getWaterHeight()` returns the
  `LLSurface` water-object Z (`llviewerregion.cpp:869-871`,
  `llsurface.cpp:1162-1168`). Every render consumer goes through that value:
  - `LLEnvironment::getWaterHeight` (`llenvironment.cpp:1067-1070`)
  - `LLPipeline::getRenderWaterHeight` and the Prism auxiliary path
    (`pipeline.cpp:10836-10854`)
  - the reflection clip plane (`pipeline.cpp:4148`)
  - the water shader `waterHeight` (`lldrawpoolwater.cpp:230-232`)
  - the fog/clip plane (`llsettingsvo.cpp:1252-1261`)
  - underwater detection (`llviewercamera.cpp:738`, published at
    `llviewerdisplay.cpp:837/1036/1483/1537`)
  - the alpha splits (`lldrawpoolalpha.cpp:424, 1299`)
  - the probe signature (`llreflectionmapmanager.cpp:1423`,
    `alprobeschedule.h:2154`)
  - region probe placement (`llviewerregion.cpp:1327`)
  - actor FX (`llactormover.cpp:16218`)
  - the sky (`llvosky.cpp:1077`) and legacy atmospherics
    (`lllegacyatmospherics.cpp:391`)
  - parcel overlay (`llviewerparceloverlay.cpp:533/681`)
  - minimap dot colours (`llviewerobjectlist.cpp:1904`)
  - hole and edge water (`llworld.cpp:954`)
  - ghost placement (`alghostplacementadapter.cpp:119`)

  I found no render consumer that reads the raw simulator height.
- **No double application.** The only callers of `setWaterHeight` pass message
  data:
  - the handshake (`llviewerregion.cpp:3148/3170`)
  - god tools (`llfloatergodtools.cpp:278/295`)
  - `LLWorld::waterHeightRegionInfo` (`llworld.cpp:876-882`)

  Nothing reads `getWaterHeight()` and writes it back.
- **No server write-back.** The Region/Estate terrain tab reads and sends
  `LLRegionInfoModel::mWaterHeight`, which is the raw message value
  (`llregioninfomodel.cpp:150`, `llfloaterregioninfo.cpp:614/2004`). It never
  reads the region's adjusted height, so an estate manager pressing Apply
  cannot commit the offset to the sim.
- **Persistence across events.** The offset survives handshakes, RegionInfo
  re-sends, new neighbours, teleports and restarts, because each one rebuilds
  from the stored original plus the current offset.
- **Offset 0.** `x + 0.f == x`, so behaviour is unchanged. Nothing new runs per
  frame. The only added cost is one string-keyed `getF32` per handshake.
- **Compile.**
  - `llworld.h`, `pipeline.h` and `llviewerregion.h` are included in
    llviewercontrol.cpp.
  - `llviewercontrol.h` is included in llviewerregion.cpp (line 75).
  - `updateReflectionProbes` is public (`llviewerregion.h:440`).
  - The `void(const LLSD&)` overload of `setting_setup_signal_listener`
    resolves unambiguously, the same way as `handleRenderFarClipChanged`.
  - Member init order matches the header (`mPaused` 588 →
    `mOriginalWaterHeight` 592 → `mRegionCacheHitCount` 617).
  - The settings.xml entry is well formed and in alphabetical order.
- **UI.**
  - Both sliders are bound to `RenderWaterHeightFudge` with range −100..100,
    matching the clamp.
  - Both Reset buttons set the value to exactly 0.
  - `xui_layout_check.py` reports 0 findings on both XML files (run by this
    reviewer).
  - The controls fit in the day-cycle editor (705 px) and the fixed-environment
    editor (750 px).

---

## BLOCKER

### B1. The visual offset changes avatar animations that other people see

**Evidence.**

`LLVOAvatar::idleUpdateBelowWater` (`llvoavatar.cpp:4151-4163`) now compares
against the offset height:

```cpp
water_height = getRegion()->getWaterHeight();
mBelowWater = avatar_height < water_height;
if (isSelf() && wasBelowWater != mBelowWater)
    AOEngine::instance().checkBelowWater(mBelowWater);
```

`AOEngine::checkBelowWater` (`aoengine.cpp:284-333`) then calls
`gAgent.sendAnimationRequest(id, ANIM_REQUEST_STOP/START)` to swap
fly/hover/hover-up/hover-down for the AO's swim states
(`aoengine.cpp:254-278`). Those requests go to the simulator and every observer
sees the result:

- **Offset raised:** the creator's avatar flying anywhere below the fake
  surface plays swim animations in mid-air for everyone else.
- **Offset lowered:** the creator stops swimming in real water.

`llagent.cpp:2794` also suppresses the server-side `ANIM_AGENT_BRUSH` stand-up
animation based on the same flag. The user intent says only their viewer sees
the change. Per the review-severity gate, that makes this a blocker.

**Fix.** Keep two flags.

1. In `idleUpdateBelowWater`, also compute
   `mBelowSimWater = avatar_height < getRegion()->getOriginalWaterHeight();`
   This gives the unused `getOriginalWaterHeight()` a consumer.
2. Pass `mBelowSimWater` to `AOEngine::checkBelowWater`.
3. Use `mBelowSimWater` in `llagent.cpp:2794`.
4. Keep `mBelowWater`, the visual flag, for cloth ripple
   (`llvoavatar.cpp:3485/3509`) and name-tag lift (`llvoavatar.cpp:4083`).
5. See m4 for Pose Polish.

If the user wants "swim in my flood" later, add it as an explicit opt-in that
is off by default, with a tooltip saying other people will see it.

---

## MAJOR

### M1. A slider drag triggers a cascade on every tick

**Evidence.** The slider is bound with `control_name`, so `LLSliderCtrl`
writes the setting on every mouse move. Each write runs
`handleRenderWaterHeightFudgeChanged` (`llviewercontrol.cpp:1001-1018`), and
for each region in the list:

1. `applyLocalWaterHeightOffset` → `LLSurface::setWaterHeight`. The height has
   changed, so this calls `LLWorld::updateWaterObjects()`
   (`llsurface.cpp:1151-1153`).
2. `updateWaterObjects()` runs `clearHoleWaterObjects()`, killing every hole
   water object. It then creates new ones (up to 25 at 512 m range) with
   `createObjectViewer` and `gPipeline.createObject`, and re-places the edge
   water (`llworld.cpp:951-1034`).

That is N regions × a full hole-water rebuild per tick. After the loop comes
`gPipeline.mReflectionMapManager.reset()`, which sets `mReset`. The next
`initReflectionMaps` marks every probe `mComplete=false` with
`mCubeIndex=-1` and `mFadeIn=0`, and resets the schedule
(`llreflectionmapmanager.cpp:3621-3664`).

While dragging, all reflection and ambient probes are wiped every frame. The
result is black or flat ambient strobing on every PBR surface, hitching from
object churn, and possible flicker in void water.

The reset is also largely redundant:

- Stock probes recapture round-robin anyway.
- With `RenderProbeOnDemand` on, the water signature already includes the
  height (`alprobeschedule.h:2154`).

**Fix.**

1. **Batch the water rebuild.** Apply the offset to all regions without the
   per-region `updateWaterObjects()` call, then call it once. For example:
   - give `LLSurface::setWaterHeight` a `bool rebuild_world_water = true`
     parameter and pass `false` from the listener loop, then
   - call `LLWorld::getInstance()->updateWaterObjects()` once after the loop.
2. **Debounce the probe reset.** Schedule a single reset about 250 ms after the
   last change, for example a one-shot `LLEventTimer`/`doAfterInterval` that is
   re-armed on each change. Alternatively, drop the reset and rely on normal
   recapture plus the on-demand signature.

### M2. `Persist=1` makes a set tweak a permanent, invisible world change

**Evidence.** `settings.xml` sets `Persist 1`, and the design survives
handshakes and relogs on purpose. In the donor, the next handshake overwrote
the local height, so in practice the effect was per region and per session.

Here a +5 m flood follows the user to every region and every future login. The
only place the value is visible is the two water editors:

- Underwater fog appears at the wrong height everywhere.
- The camera is clamped off a surface that is not there
  (`llviewercamera.cpp:109-126`).
- Name tags are lifted (`llvoavatar.cpp:4083-4097`).
- Probes and alpha splits are wrong.

Before B1 is fixed, AO swimming also follows the fake surface.

**Fix (user decision).** Choose one:

- Make it `Persist 0`, session-only (recommended for a set tool).
- Keep `Persist 1`, but warn at login when the value is non-zero, for example
  with a notification or a status-bar badge with a one-click reset.

---

## MINOR

**m1. Unadjusted water before the handshake.** `LLSurface::initTextures` places
each new region's water object at raw `DEFAULT_WATER_HEIGHT`
(`llsurface.cpp:249-251`). On teleport or when a neighbour is discovered, the
plane, fog and underwater state sit at 20 m with no offset until the handshake
arrives, then jump. Any probe captured in that window holds the wrong water.

Similarly, with `RenderWater=FALSE` the fallback `return DEFAULT_WATER_HEIGHT`
(`llsurface.cpp:1171`) ignores the offset, so fog and underwater state use the
raw 20 m. That gap already exists in stock and is cosmetic.

*Fix:* in `initTextures`, place the object at `DEFAULT_WATER_HEIGHT + offset`
using the shared helper from m5. Do not call `updateWaterObjects` from the
constructor path.

**m2. Redundant probe repositioning.** The `updateReflectionProbes(false)`
calls are unnecessary. Whenever a region has probes,
`LLSurface::idleUpdate` already calls `mRegionp->updateReflectionProbes(...)`
every idle pass, and that recomputes origins from `getWaterHeight()`
(`llsurface.cpp:641`, `llviewerregion.cpp:1312-1350`). The calls at
`llviewerregion.cpp:858-861` and `llviewercontrol.cpp:1012` are harmless but
dead. Remove them, or keep one with a comment explaining why.

**m3. A global viewer setting inside an asset editor.**
`panel_settings_water.xml` is the water-asset editor used by the Fixed
Environment and Day Cycle floaters. Two problems:

- `LLPanelSettingsWaterMainTab::refresh` calls
  `setAllChildrenEnabled(getCanChangeSettings())` (`llpaneleditwater.cpp:143-144`)
  and disables everything when no settings are loaded (`:138`). The local
  offset is therefore greyed out whenever the user views a no-mod water asset.
- Its placement implies the value is saved into the asset.

`LLFloaterWaterAdjust::refresh` has the same problem when `!mLiveWater`
(`llfloaterwateradjust.cpp:225-228`).

*Fix:* re-enable the two offset controls after `setAllChildrenEnabled`. Add a
tooltip: "Local only. Not saved in the water setting and not seen by others."
Consider keeping the control only in Edit Water plus the future Director Water
card.

**m4. Pose Polish skips the contact stage under fake water.** Pose Polish skips
its contact stage when `av->mBelowWater` (`llvoavatar.cpp:5308`). Actors
walking on the floor of a flooded set lose foot-contact stabilisation, and
their feet slide, in exactly this feature's use case. The skip exists because
of swimming, which is a real-water concept. *Fix:* use `mBelowSimWater` from
B1. Pose Polish ships off by default, which is why this is MINOR.

**m5. Duplicated, string-keyed, partly unvalidated clamp.**
`llclamp(gSavedSettings.getF32("RenderWaterHeightFudge"), -100, 100)` appears in
two files (`llviewerregion.cpp:856`, `llviewercontrol.cpp:1003`):

- A Debug Settings value outside the range is stored as-is but applied clamped,
  so the slider then shows a different value from the stored one.
- `llclamp` passes NaN through, which would put NaN into every water Z.

*Fix:* add one helper, for example
`static F32 LLViewerRegion::getLocalWaterOffset()`, that uses an
`LLCachedControl<F32>`, applies `llfinite`, and clamps. Have the listener
write the clamped value back if it differs.

**m6. Log spam with water off.** With `RenderWater=FALSE`, every listener tick
logs `LL_WARNS "setWaterHeight with no water object!"` once per region
(`llsurface.cpp:1156-1159`). Skip `applyLocalWaterHeightOffset` when
`getLand().getWaterObj()` is null.

**m7. Out-of-scope layout churn in `floater_adjust_water.xml`.** These changes
are not part of the water row:

- `save` `left_pad -7 → 1`
- `panel_settings_water_image` width `300 → 698` plus an added height
- a label's `follows` changed from `left|top|right` to `left|top`

They pass the checker, but confirm they are intentional and record them in the
commit message.

**m8. Doc corrections (`doc/WATER_HEIGHT_LOCAL.md`).**

- "Avatar water state … keep below-water avatar state aligned" presents the
  AO/brush coupling as a feature. Update it after B1.
- The claim that the listener "moves existing region-owned reflection probes"
  overstates the explicit call; idle already does this (m2).
- Add the slider-drag behaviour and the persistence decision (M1, M2) to the
  test checklist.
- Add a checklist item for B1: fly below a raised plane with the AO on and
  watch from a second account. No swim animation should appear.

---

## Fix order

1. B1: two flags, AO and brush on the simulator height.
2. M1: batched `updateWaterObjects` and a debounced probe reset.
3. M2: decide on persistence.
4. Then the minor items: m3 (enable/tooltip), m5 (helper), m6, m1, m2, m4,
   m7, m8.

After the fixes, re-review the listener and `idleUpdateBelowWater` diffs only.
No re-review of the render consumers is needed, since they are already
consistent.

---

# R2: absolute session-only level (redesign)

Scope: the redesigned uncommitted diff in llsurface.*, llworld.*,
llviewerregion.*, llviewercontrol.cpp, settings.xml, llfloaterwateradjust.cpp,
llpaneleditwater.cpp, both XML files, and the design doc.

By user choice, the AO, brush, audio and wind following the custom level are
accepted and not reported. Persistence is session-only by user choice.

## Verdict: CLEAN

There are no blockers and no majors. The design works for bringing the water
line up to a skybox. Six minor items follow; two of them are short code fixes
worth folding in before the build (r1, r2).

## Verified

- **Override off is identical in behaviour.**
  - The region constructor caches
    `mRenderWaterHeight = mSimWaterHeight = DEFAULT_WATER_HEIGHT`
    (`llviewerregion.cpp:653-660`), so `initTextures` places water at 20 m,
    the same as stock (`llsurface.cpp:250-252`).
  - The handshake path is `setWaterHeight` → `applyVisualWaterHeight()` with
    rebuild on → `LLSurface::setWaterHeight(sim, true)`. That is the stock call
    with the same arguments, including the `updateWaterObjects()` rebuild when
    the value changes.
  - The added cost is one bool check per frame in `updateRegions`, plus
    `getBOOL` calls in the constructor and handshake. This is negligible.
  - The one exception is r1.
- **No double application.** The level is absolute. `mSimWaterHeight` is
  written only from simulator messages (`llviewerregion.cpp:859`), and the
  visual value is always recomputed from settings.
  - The `RenderWaterHeightFudge`, `applyLocalWaterHeightOffset` and
    `mOriginalWaterHeight` symbols are fully removed.
  - The sanitiser maps non-finite values to 20 and clamps to 0..10000
    (`llviewerregion.cpp:863-866`).
- **Session-only.** `RenderWaterLevel` and `RenderWaterLevelOverride` are both
  `Persist 0` in settings.xml.
- **Coalescing and debounce are correct.**
  - The listener only sets a flag (`llviewercontrol.cpp:1000-1006`).
  - `applyPendingVisualWaterHeight()` runs at the top of `updateRegions`
    (`llworld.cpp:687`) once per idle frame. It applies every region without a
    rebuild, then calls `updateWaterObjects()` once, and only if something
    changed (`llworld.cpp:760-780`).
  - Each change re-arms the 250 ms probe-reset timer, so the reset fires 250 ms
    after the last tick. That covers slider release, spinner entry and button
    clicks.
  - There are no stuck states:
    - During a teleport, `updateRegions` only runs on ARRIVING
      (`llappviewer.cpp:5459-5474`). The flags persist and apply then.
    - With no world, the request is dropped, but regions created later read the
      settings in their constructor.
  - Lifetime is safe: no idle callback is used. The flags and the
    `LLFrameTimer` are members of the `LLWorld` singleton.
  - `mActiveRegionList` is the full live list (`llworld.cpp:212-213, 294-295`).
  - "Set to my height" writes two settings. They coalesce into one apply.
- **New regions, crossings and teleports.**
  - The constructor sets `mRenderWaterHeight` before `mLandp->create()`
    (`llviewerregion.cpp:658-676`), so new neighbours and teleport targets are
    born at the custom level. The R1b H4 pop of about 3 km is gone.
  - On the later handshake, the visual value is unchanged, so there is no
    rebuild.
  - Crossing regions and teleporting run the stock `updateWaterObjects()`
    (`llagent.cpp:1174`). The agent region height, and with it the edge and
    void water, is the same custom level, so the horizon is seam-free.
  - Region restarts create a new region object, so the same path applies.
- **The camera button uses the camera Z correctly.**
  `gAgentCamera.getCameraPositionGlobal()` is
  `getPosGlobalFromAgent(LLViewerCamera::getOrigin())`
  (`llagentcamera.cpp`). It is therefore the real render eye, flycam and
  drone included, not the agent's orbit camera. Global Z equals region Z.
- **Values above 4096 work.**
  - The spinner allows up to 10000 and is bound to the same control.
  - The sanitiser clamps at 10000, and nothing else on the water path clamps
    (R1b H1).
  - The slider only displays a pinned 4096 (see r4).

## Minor

**r1. `RenderWater=FALSE` is no longer stock with the override off.** Stock
`getWaterHeight()` returned `LLSurface::getWaterHeight()`, which is 20 m when
there is no water object (`llsurface.cpp:1171`). The new code returns the
cached simulator value (`llviewerregion.cpp` `getWaterHeight`).

So with water rendering off, underwater fog, alpha splits, AO and audio now
switch at the simulator level instead of 20 m. This is arguably more correct,
but it is not identical. *Fix:*

```cpp
return mImpl->mLandp->getWaterObj() ? mRenderWaterHeight : DEFAULT_WATER_HEIGHT;
```

This is safe in `initTextures`, because the object is assigned before the
call.

**r2. Parcel property-line split points go stale.** The render path is live.
`renderPropertyLines` re-reads `water_z` every frame and lifts vertices below
it (`llviewerparceloverlay.cpp:681, 745-753`). This downgrades R1b H12.

However, the shoreline split vertices are baked at the old level in
`addPropertyLine`/`checkForSplit` (`llviewerparceloverlay.cpp:533, 553-563`).
*Fix:* in `applyVisualWaterHeight`, call `dirtyHeights()` when the value
changed (`llviewerregion.cpp:1139-1146`).

**r3. "Set to my height" uses the camera, not the avatar.** The behaviour is
correct, but the label suggests the avatar's height. The camera then sits
exactly on the plane and is pushed 0.2 m above it (`llviewercamera.cpp:116-121`).
*Fix:* either

- rename the button to "Set to camera height", or
- add a tooltip: "Sets the level to the current camera height; lower the
  camera to the skybox floor first".

A "feet" variant is optional later.

**r4. A slider touch drops a typed value above 4096.** With a typed value over
4096, the slider displays 4096. Any click, drag or wheel on the slider commits
a value of 4096 or less, which silently drops the typed 6000. This is accepted
behaviour. Mention it in the doc or the tooltip.

**r5. Asset-editor disabling (carried over from R1 m3).**
`LLPanelSettingsWaterMainTab::refresh` still greys the row out when the user
cannot change settings (`llpaneleditwater.cpp:143-144`).
`LLFloaterWaterAdjust::refresh` does the same with no live water
(`llfloaterwateradjust.cpp:225-228`). After those calls, re-enable the five
water-level controls.

**r6. Redundant probe update (carried over from R1 m2).**
`region->updateReflectionProbes(false)` in the apply loop is redundant, because
idle repositions the probes every frame (`llsurface.cpp:641`). It is harmless.
