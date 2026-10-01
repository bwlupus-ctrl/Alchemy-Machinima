# Adversarial review R2 (Opus): PROBE_UPDATE_ON_DEMAND_BRIEF_V3

Read-only review at HEAD 303065bda35. Every anchor cited below was re-read. Under the user ruling, anything that breaks the promise is a P0.

**Verdict: GO-after-fixes (not a redesign).** V3 closes most of R1. Five promise-breakers remain. All of them are local fixes, and one generic mechanism (a streak detector, fix for N1) covers two of them. The main-eye light-list residue needs an explicit user decision (§C).

## A. R1 findings against V3

| R1 | Status | Evidence |
|---|---|---|
| P0-1 terrain / tree / grass / tex-anim churn | **Fixed for the listed sites.** A sibling path is still open: N2. | LT llsurfacepatch.cpp:1054-1070, LTr llvotree.cpp:376-381, LG llvograss.cpp:368-392, TX llvovolume.cpp:665-681 and :876-894 all verified. |
| P1-1 LOD tag scope | **Fixed.** | Whole `updateLOD` plus `updateGeometry` under `lod_only`, which covers `genBBoxes`→`movePartition` at :1967. No real edit hides under the tag: every real edit sets a content flag or goes through H3 first, and the Q1 rebuild completes before the manager runs (llappviewer.cpp:1559-1566). |
| P1-2 main-eye lights | **Partly fixed.** | Child-prim attachment lights and toggles are now covered. Still wrong: the fade bucket (N3), the cap rank (N3), gobo arrival (N4), and worn and flicker lights that never settle (N1). |
| P1-3 `extern` recording flag | **Fixed.** | |
| P1-4 texture churn | **Partly fixed.** | First-image and coarse rules are bounded and settle. Two holes remain: re-sharpen after a downgrade (N5) and gobo arrival (N4). |
| P1-5 flush cost | **Fixed as a cost.** The overflow semantics are a P0 under continuous load (N1). | |
| P2 probe fields / shift / txn / H4 union / D-PBR / anchors / button | **Fixed.** Exceptions: shift is incomplete (P1-a), and Rig Rim has a type bug (P1-c). | Lightbox :1537 `</button>` and :1538 `<!--` confirmed; alfloaterlightbox.cpp:67 registrar confirmed. |

## B. New P0 (promise-breakers)

**N1. Continuous movers and continuous lights mean probes never settle, and Live never idles.**
- *Movers:* `LLViewerObject::applyAngularVelocity` (llviewerobject.cpp:7337-7361) runs every frame for every llTargetOmega prim, and so does `updateDrawable` (:2538-2550). V3's new rotation test in H1a catches every one of those frames. Prop Mover spin/oscillation, physical movers and vehicles do the same.
- *Consequences:*
  - Every static probe within about 131 m is dirty forever.
  - In a spinner-dense sim, `E×P > 16384` overflows every frame, so every ORDINARY record is resynced and `clearTxn`'d. That nacks the probe in flight, so **no probe ever completes**, and `++mLiveSceneSerial` fires every frame.
- *Lights:* rig flicker runs `applyFlickerModulation` → `setLightIntensity` every frame (alcinelightrig.cpp:303), and idle animation moves worn facelights more than 5 cm. Both produce continuous light-diff events.
- *Fix:* add a per-key streak detector in the recorder and in the light diff (key = drawable or light pointer).
  - A key noted in K (≈8) consecutive flushes is demoted: it is dropped and counted as `drop_cont`.
  - When it has been quiet for Q (≈0.5 s), emit one union "settle" event.
  - List these sources in §4.9.
  - The flush must not `clearTxn` the in-flight probe on overflow. Resync hits are enough.

**N2. Mesh LOD arrival is camera-driven and untagged.**
- Dollying the camera across LOD thresholds re-requests mesh LODs. `notifyMeshLoaded` (llvovolume.cpp:1284-1287) sets `mSculptChanged`, which makes H4 `content` true, plus `markRebuild(REBUILD_GEOMETRY)` for H3. The result is R_GEOM, and the Live footprint gets bumped.
- The sculpt-texture refinement at llviewertexture.cpp:2135-2143 is the same case.
- T7 and T8b would fail.
- *Fix:* keep an `mProbeBuilt` flag on the volume, set after the first successful build with faces. After that, a mesh or sculpt LOD arrival sets a `mLodArrival` flag. H3 is tagged in `notifyMeshLoaded`, and `updateGeometry` treats `mSculptChanged && mLodArrival` as `lod_only`. The first-ever arrival stays an event.

**N3. The fade bucket is not real content.**
- The deferred local-light loop that probes use (pipeline.cpp:23263-23345) never reads `fade`. Only forward `setupHWLights` does (:10517-10532).
- So fade-bucket events are spurious, camera-driven dirt.
- The loop *does* stop at `count > effective_local_light_count` (:23309-23312), and it counts in distance order after the attachment and world-light filters. With more than 256 lights, the rendered set changes as the camera moves, and the membership diff cannot see that. That is a missed change.
- *Fix:* remove the fade bucket. Mirror the loop instead: walk the list in order, apply the same filters, and cap at `RenderLocalLightCount`. Diff the in-cap set.
- Also size each light event by its real `getLightRadius()·1.5`, not `LIGHT_MAX_RADIUS`.

**N4. Projector gobo arrival is missed.**
- When the texture arrives, its id does not change, so the §4.5 claim "tracks through texture id plus isLight" does not detect anything.
- *Fix:* H6 also fans out over `mVolumeList[LLRender::LIGHT_TEX]` and emits a light-sized C_LIGHT event per volume (the R1 fix).

**N5. Re-sharpen after a downgrade is missed.**
- Scenario: the camera turns away, and textures behind it drop discard level (VRAM governor or bias). Some probe then recaptures for an unrelated reason and bakes the blurry texture. When the camera turns back and the texture re-sharpens to ≤ `mProbeNotedDiscard`, nothing is notified.
- *Fix:* record a downgrade below the probe level (`d > Dp`) together with the global serial of the last ordinary txn start. On re-sharpen, notify only if an ordinary capture started after that downgrade. This bounds the churn and never misses the case.
- Downgrades take the GL scale-down path, so hook there as well (llviewertexture.cpp: the `scaleDown` / discard-drop site; implementer to locate).

## C. Residue

- **Continuous sources (§4.9, plus the N1 list): acceptable only with explicit user sign-off.** Tracking them means the probe never settles. Refresh-all plus MaxAge is an honest backstop, provided the tooltip and log list them.
- **Main-eye light membership: this is a user decision, not a silent residue.**
  - It is real: probe content depends on the camera, through RenderFarClip and the cap. So "camera motion dirties probes" literally holds here, although it is bounded once N3 is fixed.
  - The only way to remove it is a probe-centric light list for ON-path ordinary captures: reuse the transient cube-eye branch at pipeline.cpp:9739-9820 with save/restore, as Live does.
  - That changes capture content compared with OFF, and it touches `calcNearbyLights`, which is off-limits.
  - Present both options to the user.

## D. P1

- **a.** The `shift()` light snapshot: the per-light StickyHash accumulators hold agent-space positions. Offsetting `mPos` is not enough; every light fires on every region crossing, which contradicts T14. Either use global positions in the light signature, or re-baseline the snapshot silently on a shift.
- **b.** The ordinary capture uses `mLightScale`, not `AlchemyGlobalLightScale` (pipeline.cpp:22924-22927). The global scale over-dirties, which is harmless. `RenderReflectionProbeMaxLocalLightAmbiance` is missing from the env tail; that is a missed real change.
- **c. Compile/UB:** `CineRigRimTint` is `F32` (alcinerigrim.cpp:218), so `addAbs3` on it reads past the value. `isSlotEnabled`/`at` take `ALCineLightRigManager::Slot` (alcinelightrigmanager.h:1089-1091), so the index loop needs `static_cast<Slot>`.
- **d.** A texture shared by more than 4096 faces becomes an "all ORDINARY" event that never bumps Live. That is a missed Live change. Bump Live too.

## E. P2

- An ownership flip back to ORDINARY resyncs the previous closest-dynamic probe on camera moves. It is harmless (dynamic probes are always eligible), but it inflates `r` in T14. Exclude it from the T14 check.
- `mProbeNotedDiscard` must be written only while recording. Compute `Dp` with integer shifts.
- H2 octree rebalancing emits insert/remove pairs. That is over-dirty only; count it.

## F. Tests

T0-T22 are close but not sufficient. Add these:
- **T23** An llTargetOmega spinner and a Prop Mover prop in a static set. Expect `drop_cont`, probes settle, and Live idles.
- **T24** A rig with flicker, and a crowd wearing facelights. Expect settling.
- **T25** A mesh-heavy dolly. Expect `g≈0`.
- **T26** A VRAM-pressure turn-away / turn-back. Expect the re-sharpened reflection within the latency model.
- **T27** A projector gobo first load.
- **T28** More than 256 lights with an orbit.
- **T14** also needs a count of `nl`.
- Add a log verdict **UNSETTLED**: the same ORDINARY probe starting in 3 or more consecutive windows with no user edit. It turns "never settles" into a stated FAIL.

## G. Other checks

- **Budget:** unchanged, ≤7 faces per `update()`. OVER BUDGET is correctly described as a bug detector.
- **OFF path:** inert. Live H is bit-identical when OFF, as long as the extraction is verbatim with the three edits. The :756 control is used only at :810 and :892.
- **Transaction contract:** sound. An ack needs `mInTxn`, irradiance done, a matching epoch and a matching cube. The cube cannot change mid-txn because the free-loop exempts `mUpdatingProbe`. A refusal cannot loop except through toggles.
- **D:** keeping PBR is correct.
