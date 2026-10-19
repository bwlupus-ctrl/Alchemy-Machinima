# Simple VCam Gate: implementation spec (v3.1)

2026-10-19. Plan only; this is the last spec round before implementation. Supersedes `SPEC_gate_media_cycle.md` v3.3 and the Gate parts of `GATE_MEDIA_IMPL_REPORT.md`.

**Anchors (pinned; m6, Codex #9):**
- `H:` = `cb1ed737385`, read with `git show cb1ed737385:<path>`. All H: line numbers refer to it.
- `W:` = `98fd1f60054`, the committed 2048 change and current HEAD. W: anchors are given only for 2048 code. Between H: and W:, llprismlens.cpp lines after the 2048 hunks shift by +5 to +28, so locate them by function name.
- `P:` = `6977243fd32^`, the last commit before the media Gate.
- No other commit touched the Gate files, except `aluiperfselftest.cpp` (clone Pitch/Roll).

Build on W:. Its old-Gate hooks are deleted (§e).
**v2/v3** answer two rounds of adversarial review (§i). There is no new shader, no fixed frame, no media weight, one media rule, one deadline and one failure transition.

## a. User workflow (approved 10/19; build exactly this)

1. **Add.** Settings for → **+ Add** → **Gate**. The slot appears in the slot list and the monitor strip like any camera, labelled "Gate". One Gate per scene, an added simplicity limit (a second add fails: "Only one Gate"). It uses one of the 8 slots, so adding fails when all 8 are used.
2. **Gate card** (shown when the Gate slot is selected):
   - **Sources:** a tick box and a name for each existing Camera Feed and video slot (URL or prim face), **up to 7** (the Gate uses the 8th slot). Never the Gate itself. Two restrictions are inherited from the old Gate (P:llprismlens.cpp:5952): Surface Lens cameras and cameras on avatars or attachments are not listed.
   - **Mode:** Manual, or Auto + **Interval** (s, 0.5–120, default 8).
   - **Transition:** Cut, or Crossfade + **Length** (s, 0.1–3, default 0.5).
   - **Off-air videos:** Keep playing (default) or Pause.
   - **Audio (user decision 10/19):** only the on-air video is heard, at its own slot Volume. Off-air videos are muted. A Crossfade fades the sound across too. A camera on air means no Gate video sound. There is no new control. See §d "Audio".
   - **Status:** "On air: <name>", or an error and why.
3. **Manual.** Clicking a ticked source's name puts it on air at once with the chosen transition. There is no Preview/Take. Clicking the on-air name does nothing. A click during a fade applies when the fade ends (the latest click wins). Names do nothing in Auto.
4. **Auto.** Steps through the ticked sources in slot-list order, every Interval.
5. **Output.** The Gate slot has the same Output cards as any camera slot: Faces this camera feeds, Projectors / Projector / More effects (TV), Face mapping, TV effects, Raw output. They show the program. Faces, screens and projectors (what gets recorded) show every cut and the full Crossfade blend. The Gate's own monitor tile and Raw output show the Gate texture (the program, cut only) at their normal preview refresh (coordinator decision A).
6. **Prim surfaces** (same as the old face "Feed from: Gate program"):
   - Add selected face, Add virtual screen, and dragging a monitor tile onto a face bind faces to the Gate slot exactly as for a camera.
   - Any number of faces on any number of prims, up to the shared 16-face cap (`MAX_DISPLAY_BINDINGS`). Each face keeps its own Fit/Fill/Stretch, anchors, bars and TV effects.
   - Projectors (object and rig lights, including 2048) bind the same way.
   - Every consumer shows the program and follows cuts and crossfades.
7. **Removed:** everything listed in §e.

## b. Data model, signal contract, scenes

**New types** (`llprismlens.h`). They replace H:312-361. Keep `EGateMode` (H:100) and `EGateTransition`.
```cpp
enum class ESourceSlotKind : U8 { EMPTY, CAMERA, MEDIA_FACE, MEDIA_URL, GATE };  // H:61
enum class EGateOffAir : U8 { KEEP_PLAYING, PAUSE };
struct GateSource                        // 4th SourceSlotData variant (H:797): gateData(), isGate()
{   std::vector<LLUUID> mSources;        // capture ids, <= 7, never self; order = slot-list order
    EGateMode mMode = EGateMode::MANUAL;               F64 mIntervalSeconds = 8.0;
    EGateTransition mTransition = EGateTransition::CUT; F64 mCrossfadeSeconds = 0.5;
    EGateOffAir mOffAir = EGateOffAir::KEEP_PLAYING;
    LLUUID mProgramId; };                // restored on load only if eligible (§d rule 2)
```
**Runtime state** (not saved):
- `GateState` (pure, `llprismgate.h`), held by the Gate `PrismInstance` (H:1878). It records program, pending and from; `held` (the source whose picture is in the Gate texture); cursor; pendingStart; fadeStart; nextAuto; cutSerial; and the copy key `{held id, generation, publication serial}`.
- Each source keeps the existing `mGateDecodeIdentity`/`mGateDecodeSerial` (H:1884-1885) as its pending baseline (§d rule 3).
- Registry: `std::set<LLUUID> mGateGrants` (session-only; cleared on scene load), plus `mGainedImpls` (last frame's audio-affected media texture ids; §d Audio).
- `LLViewerMediaImpl::mPrismGateGain` (default 1), §d "Audio".
- The snapshot exposes `GateSource` and `GateStatus {program, pending, from, alpha, cutSerial, text}`.

**API** (replaces H:llprismlens.h:1355-1372):
- New: `addGateSlot`, `setGateSource`, `gateTake(gate, sourceId)`.
- Keep `gateOnAirCameraEye()` (llactormover.cpp:7072, 7501, 7704, 7738), reimplemented from the Gate's program camera and cutSerial.

**Gate signal contract (B1, m7).** The Gate has no camera or media payload. It publishes the signal of its `held` source:
- `mSignalAspect`, the video-or-camera kind and optics `[4]`;
- output region and orientation, copied 1:1;
- for a held camera, its effective vertical FOV and output aspect. `getProjectorFeed` uses them for `mCameraVerticalFovRad`/`mCameraAspect` (Throw = Camera FOV, H:pipeline.cpp:28338). They are 0 for a video, exactly as for a video slot.
- The throw follows the held source, so it changes at a cut or a fade end.
- Add three helpers: `signalAspect(c)`, `signalIsVideo(c)` (true for `isMedia()`, or a Gate holding a video) and `signalOptics(c)` (camera optics, or the Gate's copy).
- **Rule:** media-only work keeps testing `isMedia()`. Every `cameraData()->` dereference must be guarded by `cameraData()`. Every `isMedia() ? … : cameraData()->…` becomes a helper call.
- Known sites (H:llprismlens.cpp):
  - prepare dispatch 4178 (`isGate()` → `prepareGateSlot`)
  - label 5761
  - `getProjectorFeed` aspect 7011, 7080
  - `buildSignalOne`: SDR 11871, optics 11874, raw aspect 11895, mapping aspect/predicate 11934-11938 (add `isGate()` to the Fit/Fill predicate)
  - getCompositeStates SDR/optics 12122-12128; aux SDR/optics 12382-12388
  - llfloaterprismmanager.cpp: 951, 1021, 1082, 1520, 1535 and 1918 (labels); 2278 (aspect); 1200-1224 (camera fields; the Gate is read-only there) (m8)
- Audit: `git grep -n "cameraData()->"` in lens, manager, panel and page model, plus a per-consumer test (§g).

**GATE is accepted like CAMERA_FEED** by: `canRouteSource` (H:10598), `addDisplaySelectionStatus` (H:10240), `displayTargetStatus`, `bindDisplayTarget`, `addVirtualDisplay`, `bindProjector`, `bindRigProjector`, `reassignDisplay`, `reassignProjector`, `validateProjectorReassignment` (H:984) and the Light Rig projector-feed combos.
**GATE is skipped** by `updateCadenceEntitlements`, `chooseRenderSlot`, `copyMedia` and `reconcileMedia`.

**Scenes.**
- **Item:** a `prism_captures` entry with `source_kind:"gate"`, `capture_id`, `label`, `gate_sources:[uuid]`, `gate_mode`, `gate_interval`, `gate_transition`, `gate_crossfade_seconds`, `gate_off_air:"keep_playing"|"pause"`, `gate_program_id`.
- **Loader (C13):** `applySceneData` gets a `gate` branch before the camera-field checks (H:8058-8092). It accepts the kind and does not require mode or rate. Gate source ids resolve **after** all capture items are parsed, because `sceneData` writes in slot order (H:7622). Unknown, self, duplicate or ineligible ids are dropped and reported. A second Gate item is skipped and reported. New scenes never write `prism_gates`, `gate_source`, `home_capture_id` or `source_on_cut`.
- **Migration.** A pure `gateMigrateScene(LLSD&, notes)` runs first in `applySceneData` (H:7943). It runs when there is no gate item and some display or projector has `gate_source=true`.
  - **Free slot (fewer than 8 captures):** append a Gate item built from `prism_gates[0]`:
    - sources = armed `capture_id`s with `enabled=true` that resolve to camera_feed or media items;
    - mode, interval, transition and crossfade copied; off-air = keep_playing; program = the program arm.
    - A malformed record still creates the Gate, with no sources.
    - Every `gate_source` display and projector gets `capture_id` = the Gate id. The faces become Gate faces and keep their TE, mapping and TV effects.
  - **Fallback (8 captures):** a projector goes to `home_capture_id`, else its hint. A face goes to its hint if it resolves. Everything else is dropped and reported as a `SkippedBinding`, never left to fail at H:8471.
  - Always strip the old keys. Old scenes load without errors.
- **Settings:** delete `VCamGateMediaCycleEnabled`. It is defined at settings.xml H:29741 and used at lens 2711, 6453 and 6479; panel 367 and 1252; selftest 7353 and 7466-7469. No new settings.

## c. Render path (`renderAuxiliaryView`, H:llprismlens.cpp:11219)

1. Delete the `updateGate` calls at H:11239 and 11270, and `GatePublicationScope` at H:11272-11276.
2. **Prepare the Gate slot first** in the loop at H:11292, with `prepareGateSlot()`.
   - Demand comes from the Gate's visible faces (`prepareDisplayFrame`, as in H:4736-4750), its consumed projectors (`addProjectorDemand`, H:6884) and its tile. These combine by **max** into the watch size W×H.
   - Set `mFrame.mPrepared` only when there is demand.
3. **`stepGate(now)`:** run `gateStep` (§d). Set `mGateWatchFrames = fed ? 1 : 0` and `mGateWatchW/H = W×H` on every slot before it is prepared. The watch controls rendering only, never capacity.
   - Issue transport requests and set audio gains.
   - Delete `ageGateWatches`, `cancelGateWatches` and `watchGateArm` (H:9145-9188).
4. **Prepare the other slots.**
   - Fed cameras become visible producers through the P: watch block (P:4685-4690), maxed into their own size even when they have a visible face (pitfall b; drop `!mAnyDisplayVisible`).
   - Delete `mGateWarmOnly`. Restore P: `updateCadenceEntitlements` (P:4885-4975) and P: `chooseRenderSlot` (P:5007).
   - Fed media keep the H: watch block (H:3375-3381, without `mGateWarmOnly`) and the H:3309 demand. Delete the H:3397-3398 copy skip; freshness is judged in §d instead.
   - **Off-air sources get no watch.** A camera with no face, projector or open tile of its own is not prepared (H:4182) and **does not render**.
5. `copyMedia`, then one `render_one`. Delete `chooseReservedRenderSlot` (H:5294, H:11801).
6. **`composeGate()`** is called in two places: before the H:11313-11317 video-only return, and after H:11800 (before H:11802-11806). The copy key makes the second call a no-op. Monitor tiles refresh at their normal preview cadence (decision A).

**`composeGate()`.** Only when the Gate is prepared (markProduced needs `mPrepared`, H:5313); otherwise the Gate holds.
- If `held`'s output is complete, full-quality and its copy key changed, **copy it 1:1** into `mPrismLensOutput[gate]` (a blit of the logical region).
  - Copy through W:`produceProjectorOutput(output, spare, capacity, allocate, produce)`. The Gate texture's capacity = the held source's output capacity, so a capacity change produces into the spare and swaps; the picture is never blanked.
  - Then copy the held source's signal metadata.
  - The Gate's W:`requestProjectorOutputCapacity` callback clears the copy key, so a capacity change re-copies even a frozen source.
- **Commit contract (C4):**
  - `gateStep` only proposes. `gateApply(state, proposal, ok)` commits program, cutSerial, status and audio only after its pixels exist: a successful copy for a Cut or a fade end; a complete incoming output for a fade start.
  - A failed copy changes nothing and is retried next frame, within the single §d6 deadline.
- **Copy key (C5):** `{held id, generation, publication serial}`. Slot serials are not globally unique; the id and generation make them so.
- **Publication rule (Codex #2, N2):** every Gate publication calls `markProduced(gate)` (H:5306) and then `gPipeline.preparePrismProjectorFeeds(gate)` (camera path, inside a `ScopedPrismRenderState`).
  - A publication is any successful copy (steady state or a cut), every fade step (during a fade the Gate publishes each frame because α changed), and **every rollback or cancellation frame** (§d6, Codex R3 #1).
    - On a rollback frame the restored presentation is published even when no copy is needed, and `dirtyPrismProjectorFeed` is called for each Gate projector first, so W:pipeline.cpp:2182's unchanged-generation skip cannot keep a stale blended cookie.
  - A Gate publication is never preview-quality. `markProduced`'s `mOutputPreviewQuality = mPreviewOnly && !isMedia()` gains `&& !isGate()`, so a face that comes into view uses the Gate at once.
  - A projector-only Gate therefore updates every time.
  - Cookies use W:'s committed cookie behaviour unchanged, with no spare-and-swap (decision B; §h). A cookie that fails to prepare keeps its last accepted cookie and retries.

**Program contract (C11; no exposure/colour change, no double letterbox).**
- **Steady and Cut:** the Gate texture is a 1:1 copy of the held source with that source's own aspect, kind and optics. Every consumer then applies its own Fit/Fill/Stretch and transfer exactly as it would to the source directly.
  - A 4:3 source fills a 4:3 face.
  - A video keeps the video transfer; a camera keeps the camera transfer.
- **Crossfade (coordinator decision 10/19: blend per consumer):** blending happens in each consumer's **existing HEAD two-source pass**. That pass draws the primary from the Gate texture (which holds `from`) and the incoming from the program source's own output, at alpha α. The Gate texture itself is never a blend.
  - The consumers that blend are: world faces and screens (`getCompositeStates` H:12035), the aux mirror (H:12301) and projector cookies (H:12649). All go through `buildSignalCompositeState`.
  - **Previews** (H:12496 → H:12578, the Gate tile and Raw output) always get α = 0. They show the Gate texture only: `from` during a fade, then the program (decision A).
  - Keep llprismpreview.cpp's H:1205 `bindGateComposite` call and its H:1276 permutation choice. They are needed even with α = 0, because HEAD's prismPreviewF.glsl:43 chooses the camera or video transfer from `prismPrimaryMediaSDR`, and only `bindGateComposite` sets that uniform. Everything else in that file returns to P:.
  - Retarget the branch at H:11985-12013. Keep the second `buildSignalOne` and the incoming-field copy. Delete the subscribed and frozen-settings code at 11985-11993, and replace `gateIncomingSlot`/`gateAlpha`/`gateTransition` with the Gate slot's own fade state.
  - **Defensive fallback:** if the incoming `buildSignalOne` fails, return the primary-only state (α = 0) instead of failing the whole composite (H:12000). This covers a loss the step has not seen yet.
  - Each source keeps its own framing and transfer: prismProjectorFeedF.glsl:67 for cookies; `bindGateComposite` (pipeline H:2247, 2514, 27999) for world, aux and cookies.
  - The round-4 GPU check showed this is exact at both endpoints. At fade end the Gate copies the program's current frame, so the next frame is identical.
  - No new shader, and no existing shader is edited.

**2048** (the cut-back job gives the old Gate path none; the new slot provides it):
- **Gate projector capacity:** `gateOwn` = W:`projectorOutputCapacity` over projectors bound to the Gate slot: 2048 if one is set to 2048, else 1024.
- **Sources:** a **camera** in the Gate's durable program/pending/from set gets `outputCapacity = max(own, gateOwn)`. Pass this as a new `inherited` parameter of W:llprismlens.h:644 `projectorOutputCapacity`, replacing its `!mGateSubscribed` test, so one helper still decides. Videos keep HEAD's 1024 copy cap and are copied 1:1. The Gate texture's capacity follows its held source (§c `composeGate`).
- **Resizing (M1, C3):** use only the W: mechanism for capture and Gate textures. W:`releaseUnusedProjectorCapacity` (llprismlens.cpp:6929) calls `requestProjectorOutputCapacity` (llprismlens.h:683) to ask for a render. `produceProjectorOutput` (llprismlens.h:690) renders into a spare and swaps; the old picture stays on screen until then.
  - Add one branch: a slot whose capacity dropped and that has no visible face, consumed projector, open tile or Gate feed is released at once, since nobody sees it. W: also releases a lowered cookie on its own.
- **Pitfall a (thrash):** capacity is keyed on durable membership and bound projectors, never on the watch or demand. It changes only at a commit, a fade end, a prune, a projector bind/edit/unbind, or Gate deletion.
- **Pitfall b (shrink):** every size combines by max (steps 2 and 4).

## d. Program rules and media (pure `gateStep` / `gateApply` in `llprismgate.cpp`)

**Inputs, per ticked source in slot order:**
- id and kind (camera, URL or face)
- `hasOutput` and full quality (`!mOutputPreviewQuality`)
- publication serial (H:5331)
- copied decoder serial (`mMediaCompletedSerial`) and the decoder's current `completedMediaImage()` (P:llviewermedia.h:295)
- player paused (`isMediaPaused()`, P:325), frozen (`!mMediaRunning`), still downloading, `owner->desired`, eligible, and the slot's runtime reason
- **eligible** = a camera, a prim-face video (exempt from grants; it has no Play button, m2), or a granted URL

Also: now, settings, the take id, and `demanded` (the Gate is prepared).

**Demand debounce (v3.2, coordinator, from Opus r4 MINOR).** For rules 5 and 6, `demanded` is debounced. The Gate counts as **unseen** only after it has been unprepared for 0.25 s continuously. A shorter lapse counts as still demanded: for example, the 2-frame projector consumption window, or a face briefly leaving view. A selected Crossfade therefore never degrades to a Cut, with its audio jumping, because of a one-frame lapse. The deadline clock (rule 6) also pauses only after that debounce. Copies and cookie preparation still happen only on frames where the Gate is actually prepared. Test it in §g4b: demand off on alternate frames during a Crossfade hold must still give a Crossfade, with no failure.

**Fade while demand lapses (v3.2, from Opus r4 MINOR).** A fade in progress keeps running on time: α advances with `now`, whether or not the Gate is demanded. If the fade reaches α ≥ 1 while unseen, the fade-end copy waits for demand and is then copied on the first demanded frame. Its deadline counts demanded time only.

1. **Prune.** Drop any program, pending or from that is no longer ticked or present. If nothing is ticked: draw black, "No sources ticked".
2. **Next (M2, C7, C9).** Candidates are eligible sources only; an ungranted URL never starts from the Gate.
   - Choose with a **cursor**: the next candidate after the last *attempted* source, wrapping.
   - Used when there is no program (including a restored ineligible program) or the program was pruned. In Auto, also when there is no pending, no fade and now ≥ nextAuto.
   - nextAuto = commit (or failure) time + Interval. There is no catch-up.
   - A Manual take grants the clicked video and becomes pending (latest wins; queued during a fade).
3. **Pending.** Record its baseline: publication serial and, for a video, the decoder's current serial and identity, stored in `mGateDecode*` (C2). Feed = {program, pending}, so the outgoing source stays live.
4. **Fresh** = full-quality output, and one of:
   - a camera with publication > baseline publication;
   - a **playing** video with publication > baseline publication **and** copied serial ≠ baseline decoder serial, with identity current. This is N1's strict check: both conditions are needed, so an old copy never passes.
   - **Exempt from the publication-advance test** (neither is ever re-copied; H:3385 and the `pending` test in `prepareMedia`):
     - a **paused prim-face video** whose validated copied image equals the decoder's current image, at current dimensions (C8);
     - a **frozen** source with output (m3). Both count as fresh at once.
5. **Commit (B2, Codex #8).** One rule: **a Crossfade starts only when the Gate texture holds a valid picture and the Gate is demanded; otherwise the commit is a Cut.** This covers the first program, recovery after a prune or failure, and an unseen commit.
   - A demanded, fresh commit goes through `gateApply`. A Crossfade sets from = program and starts the fade. At α ≥ 1 it copies the program and clears from.
   - An unseen commit (Gate not demanded, coordinator decision 10/19) happens at once. Audio and status follow, and the Gate texture keeps its last frame. When demand returns, the program is copied only from a frame fresh against its commit baseline.
6. **One deadline, one failure transition (Codex #1, #6, m1).** Each transition has a 5 s deadline: a pending source, an unseen commit awaiting its first copy, and a fade-end copy.
   - **The clock counts only on frames where the Gate is demanded (Opus R3-1).** It starts, or resumes, when demand returns.
   - Unseen time never counts, so an Auto Gate running unseen never fails back.
   - A pending source whose demand lapses mid-hold becomes an unseen Cut (rule 5) with its clock paused.

   The transition **fails** when:
   - its demanded time reaches 5 s without a successful publication, fresh or not;
   - Play is refused; or
   - during a fade, the incoming source loses its output (end of file, budget loss, source replaced).

   **On failure:** the state returns to the Gate texture's `held` source (program = held if still present, else none), and pending and from are cleared. Audio follows the state. The status names the failed source:
   - "<name> not ready: downloading" (the download continues), or
   - "<name> has no picture: <reason>".

   The cursor is already past the failed source. The failure frame **publishes** the restored `held` presentation, including the cookies (§c publication rule), so faces and projectors both return to `held` on that frame.
   - If the **program's** own player stops (end of file P:2871, media caps P:llviewermedia.cpp:4671), the Gate keeps its copy and shows "<name> stopped: <reason>" (C14). The next take restarts the player through "Pending ⇒ Play".

**Media: one rule (M3, C8, C14).** The Gate uses only the slot's ordinary transport (P:`fileTransport`, P:2653). It never stops a player or owns one. Transport is keyed on `owner->desired` (m5): the Gate calls `fileTransport` only when the desired state must change, never every frame.
- **Pending ⇒ Play,** in both modes, for a granted URL whose `owner->desired != PLAYING`. This resumes a paused URL, restarts an ended one, and starts a stopped one through the ordinary path. Rule 4 then waits for a newly decoded frame.
- **Keep playing (coordinator decision 10/19):** the Gate never pauses in this mode.
  - **Tick in a live session:** the tick grants the URL. If its `owner->desired` is not PLAYING and the user has not paused it, it gets Play once.
  - The video then plays muted off air (gain 0). The first cut to it commits on its next decoded frame, which is effectively instant.
  - This is edge-triggered on the tick only:
    - **Scene load:** ticks are restored but nothing gets Play. The video waits for Play on its slot or for its first take/click (rule 2).
    - **User Pause:** the user's Pause on the slot is never overridden. The Gate does not Play that video again until it is taken.
    - **Switching from Pause to Keep playing** resumes nothing; each video resumes on its next take.
- **Pause:** each step, every ticked URL outside {program, pending, from} whose `owner->desired == PLAYING` and that is not still downloading gets Pause. This is level-triggered, so mode switches, ticks and cancellations reconcile with no edge logic.
- **Prim-face videos** get no transport.
- **Grants (URLs only):**
  - Set by a tick, a name click, or Play on the slot (`playMediaFile`, H:10705).
  - Cleared on scene load ("scenes never auto-play", vcam_page.json:39).
  - An ungranted ticked URL is skipped, with status "<name>: press Play once".
- **On-air protection (N3):** `mediaDemand` (P:2970) gives every program, pending or from player demand = 2 × 1024². That is above the 1024² floor every private player gets (P:alvcammediafilelifecycle.h:335). The existing interest sort and caps (P:llviewermedia.cpp:772-776, 4671) therefore demote and retire off-air players first.

**Audio (user decision; C6, Codex #7).** Gains are reconciled **every frame** from the committed state:
- program 1; during a fade, from 1-α and program α; every other ticked video 0. With a camera on air, all are 0.
- **Per-frame reconciliation:**
  - Resolve each ticked video slot to its impl (keyed by media texture id) and aggregate shared impls by max.
  - Set those gains.
  - Any impl in last frame's `mGainedImpls` that is not in this frame's set is restored to gain 1. That happens on untick, deletion, face rebind, a new prim-media impl, Gate deletion or VCam off.
  - Store this frame's set. Missing impls are skipped.
  - The reconcile also runs in `renderAuxiliaryView`'s `!enabled` branch (H:11237-11250) with an empty set, so turning VCam off restores gain 1.
- The gain lives on `LLViewerMediaImpl` and is multiplied in `updateVolume` (P:llviewermedia.cpp:2314). `setPrismGateGain` calls `updateVolume` only when the gain changes.
- **Private URL players:** a slot Volume edit (`setVolume`, P:2643) or a player created with its cue volume (`cuePrivateFile`, P:2864) cannot unmute an off-air video. Every path that sends volume to the plugin, including media-source creation, must go through `updateVolume`.
- **Prim-face videos:** the gain is the viewer's local per-instance volume. The prim's server media settings are never touched, and no `ObjectMedia*` message is sent.

## e. Delete

- **`llprismgate.h/.cpp`:** delete everything: `GatePosition`…`GateReducer`, the cadence/latency/fade-budget helpers, `GateSource*` events, `gateInterceptsPlay`, the arm/cut/warm helpers, `gateFeedbackResult`, `gateDisplayCaptureReferenceAccepted`, `gateRouteDisplayBinding`, `gateOnCut*`, `gateOutputSourceFromLLSD` and the old LLSD. Keep `prismFullTargetScissor`. New contents: `GateState`, `gateStep`, `gateApply`, the GateSource LLSD and `gateMigrateScene`.
- **`llprismlens.h`:** H:312-361 (keep `isOtsPairArmLabel`); `MediaUrlSource::mOnCut` (790); `mGateSubscribed` and `mHomeCapture` (868-896); `ProjectorFeed` Gate fields (1042-1045); `mGateRevision` (1127); `gateMediaPriority`, `gatePlayVideos`, `setMediaFileOnCut`, `setProjectorGateSubscribed`; 1355-1369; the Gate parameter of `removeProjectorWithCapture` (1019). Keep `CompositeState`'s incoming fields and `bindGateComposite` (used by the fade).
- **`llprismlens.cpp`:**
  - Data: `PrismGateRuntime` (2057-2077); `mGateAwaitingDecodedImage` (1886, and the skip at 3397); `mGateOpportunityFrame`; `mGateWarmOnly`; the display/projector `mGateSubscribed`/`mHomeCapture` and `repairProjectorHomes` (9219); registry members 9761-9765.
  - Functions: the `setCamera` feedback guard (5465-5490); 5925-6704 (rewrite `gateOnAirCameraEye` instead of deleting it); 9116-9194; `routeGateDisplays` (9231); `buildSignalCompositeForGate` (2079, 12812).
  - OTS arming at 10296-10470 and `restoreOtsGateRollback` (7386). llfloaterdirector.cpp:7110 counts OTS cameras by label.
  - Scene Gate code at 7638, 7761-7768, 7854-7868, 7928-7940, 8015-8022, 8435-8470, 8677 and 9044.
  - Restore P: `PrivateMediaRuntime`, `stopPrivate`, `retirePrivateOwners`, `fileTransport`, `tickPrivate` and the private branch at H:3266. Delete `updateGateRepeatModes` and `mGateCloseReceipts`. `clearSlot` (9574) also removes the slot from the Gate's sources.
- **Restore whole files to P:** (no later commits; W: does not touch them): llpluginclassmedia.cpp/.h; llplugingateclock.h (delete); llpluginprocesschild.cpp; llpluginprocessparent.cpp; media_plugin_libvlc.cpp; alvcammediafilelifecycle.h; alvcammediafilepolicy.h with its test additions; llviewermedia.cpp/.h, then add `mPrismGateGain`.
- **llprismpreview.cpp/.h:** restore P:, **except** keep H:1205 (`bindGateComposite`) and H:1276 (permutation), plus GATE handling through `signalIsVideo` (§c). Deleted: the Gate-serial invalidation, H:979, 1084, 1230, 1270, 1284, 1372.
- Keep `alkitcard` `full_width`, `PRISM_GATE_MAP` and all shaders.
- **Committed 2048 Gate hooks (W:, m4):**
  - `projectorRenderResolution`: llprismlens.h:637, and its call sites in llprismlens.cpp at 6534, 6893, 7035, 7075, 9192 and 12712. Callers use `mResolution`.
  - The `!mGateSubscribed` test in `projectorOutputCapacity` (llprismlens.h:644), replaced by `inherited` (§c).
  - `updateProjectorGateSubscription` (llprismlens.h:705; its call at llprismlens.cpp:5930).
  - The Gate parameter of `releaseOversizedProjectorCookie` (llprismlens.h:712; its caller at pipeline.cpp:2058-2072).
  - In llprismprojector_test.cpp: the Gate lines of #45 (~1168-1176), #46 (1191) and #50 (1260).
- **`pipeline.cpp/.h`** (keep the W: hunks): `gateProjectorPrepared` and `finishGateProjectors` (H:2324-2360); the staged-cookie fields; the `mGateSubscribed`/`mGatePublication` checks at 2142-2170, 2376-2384, 2414-2438, 2583 and 28205-28226.
- **`alpaneldirectorvcam.cpp/.h`:**
  - Rewrite `refreshGate` (1236-1297) and `commitGate` (1299-1335).
  - Delete `vc_proj_feed` (725-728, 1373-1385); `vc_face_feed` (922, 1683-1689); `vc_gate_note` (336, 935, 974); `vc_proj_gate_note` (337, 725); the Gate face entries (142-147, 650-667, 785-790, 2185-2194); `mGateList` (266); the setting watch (367); `vc_media_on_cut` (299, 1002, 1033, 1288, 1446).
- **Other UI:**
  - `alvcampagemodel.cpp/.h`: 156, 175, 516, 934.
  - `llpanelface.cpp/.h`: 3423, 3490, 293.
  - `llpanelvolume.cpp/.h`: 1079, 1144, and .h:161.
  - `llfloaterprismmanager.cpp/.h` and `floater_prism_manager.xml`: the Gate tab (xml 831 to the end of `gate_settings_document`), `display_gate_source` (xml 624), the Gate projector list and Play videos. The Gate is listed read-only.
- **Tests:**
  - `llprismgate_test.cpp`: rewrite (#1-#64 removed; drop the raised group limit).
  - `llprismprojector_test.cpp`: delete #42 and #43, any leftover Gate fade-budget test, and the `mGateSubscribed` assertions in #14 (H:268-276).
  - `aluiperfselftest.cpp`: delete `functionalGatePresentation` (7251) and the `vc_media_on_cut` check (8506). **Rewrite (keep the dispatch)** `functionalGateLifecycle` (458-496, dispatch 7547-7551) for the new seams, and `functionalNativeGate` (7345-7489, dispatch 7838-7845) for the new widgets.
  - `director_functional_manifest.py`: **rewrite, not delete,** the Gate rules at 1681, 1699-1706 (the loop that reads `gate_functional_probes`), 1725, 1792-1795 and 1860-1861 for the new widget names (C15).

## f. Kit page (`scripts/code_tools/director_kit/vcam_page.json`)

**Gate card (H:24):** 14 rows become 7, with `"show_if":"gate_slot"`, badge "GATE", and `full_width` kept. Remove card index 3 from the lens/look/output/rate sections (H:255-261). Every row gets `kw` "Gate switcher program cut crossfade sources camera video auto interval".

| # | Label | Kind | Widget | Notes |
| --- | --- | --- | --- | --- |
| 1 | Sources | custom | `vc_gate_sources` | Tick, name, on-air marker; row_height 150. A tick calls `setGateSource`; a name click calls `gateTake` (Manual). |
| 2 | Mode | segmented | `vc_gate_mode` | Manual, Auto. |
| 3 | Interval | slider | `vc_gate_interval` | 0.5–120 s, step 0.5; `show_if` auto. |
| 4 | Transition | segmented | `vc_gate_transition` | Cut, Crossfade. |
| 5 | Length | slider | `vc_gate_crossfade_seconds` | 0.1–3 s; `show_if` crossfade. |
| 6 | Off-air videos | segmented | `vc_gate_off_air` | Keep playing, Pause. |
| 7 | Status | readout | `vc_gate_status` | On air, or the error; wraps. |

**Other changes:**
- **Removed rows (5):** `vc_media_on_cut` (H:32), `vc_proj_feed` (H:131), `vc_proj_gate_note` (H:132), `vc_face_feed` (H:184), `vc_gate_note` (H:194).
- **Add action:** in `camera_selector` (H:22), add `{"value":"gate","label":"Gate (switch cameras and videos)","index":6}`, handled at alpaneldirectorvcam.cpp:1588.
- **Card visibility for the Gate** (alpaneldirectorvcam.cpp:978-1000):
  - Show: gate, faces, projectors, projector, projector_tv, face_mapping, tv_effects, raw_output, performance.
  - Hide: all source, lens, look, output_frame and picture_rate cards.
- **Probes:** `gate_functional_probes` (H:247-253) becomes schema → `vc_gate_mode`, step → `vc_gate_sources`, migrate → `vc_gate_status`. These are consumed by the rewritten manifest loop.
- **Pin:** **2143 − 14 − 5 + 7 = 2131**. Set it at director_kit_gen.py:282 with `# 2131: Simple Gate 10/19: Gate card 14->7; On cut, 2x Feed from, 2x Gate notes removed.`
- Regenerate the three generators, and update both mockup copies (`doc/director_kit/mockup_src/VCamPage.dc.html`).

## g. Acceptance tests

**`llprismgate_test.cpp`** (pure):
1. **LLSD and scenes:** GateSource defaults, round trip, strict ranges, cap 7, self/duplicate/unknown ids dropped. A Gate item written **before** its sources still resolves them (Codex #9). Grants are cleared on load.
2. **Migration:**
   - With a free slot: `enabled` arms only; the face and projector are rebound with TE, mapping and TV effects intact; old keys stripped; the result passes the loader.
   - Malformed record: the Gate is created with no sources.
   - 8 captures: projector → home, face → hint, the rest reported, never failed.
3. **Hold and baseline:**
   - Click B → feed {A,B}, A still held.
   - A frame decoded **before** the click and copied after it is **not** fresh (C2).
   - An old copy (copied 10, decoder at 50 at pending) is **not** fresh until publication passes the baseline (N1).
   - A frozen source with output is fresh at once (m3).
   - A fresh B commits only when `gateApply` gets ok=true. With ok=false, program, status, gains and texture are unchanged (C4).
4. **Fade:** α follows time; feed is {A,B} only for Length; at the end, B is copied and from is cleared.
   - With Crossfade selected, the first program, recovery after the last source is removed, and an unseen commit are all **Cuts** (Codex #8).
4a. **Single deadline (Codex #6, m1):** each of these fails after 5 s of **demanded** time back to `held`, with state, audio and status matching:
   - a fresh B whose copy keeps failing;
   - a broken B committed unseen, once demand returns: A stays on screen and the status no longer says "On air: B";
   - a fade-end copy that keeps failing.
4b. **Unseen time (Opus R3-1):**
   - Auto runs unseen for 20 s (Interval 8). There are no failures, and audio and status cycle normally. When demand returns, the program commits cleanly on its first fresh frame.
   - A pending source whose demand lapses mid-hold becomes an unseen Cut with its clock paused. When demand returns, it publishes cleanly, with no failure.
4c. **Incoming lost mid-fade (Codex #1, R3-1):**
   - EOF, budget loss and source replacement during a fade each fail at once to `from`. Program and audio return to `from`, and the Gate texture still holds `from`.
   - **Frozen A → short URL B ending mid-fade:** on the failure frame, **both** the face and the projector cookie show A. This holds although A's copy key did not change.
   - A fade-end timeout rolls the cookie back the same way.
4d. **Fallback and quality:**
   - When the incoming `buildSignalOne` fails, the face shows the primary only.
   - A Gate with only its tile demanded publishes full quality, so a face entering view shows it at once.
5. **Livelock (M2):**
   - With sources [ungranted video, missing camera, good camera], a load reaches the good camera.
   - A failed B in Auto is not retried before C. A restored ineligible program is not used.
6. **Grants:** initial pick, prune recovery, Auto and restore never Play an ungranted URL. A click grants.
7. **Not demanded:** the commit happens at once; the texture holds A until B is fresh after demand returns (B2).
8. **Transport:**
   - Pending ⇒ Play in both modes. Pause mode pauses only off-feed playing URLs, level-triggered, including on mode entry and after cancellation; still-downloading URLs are left alone.
   - **Keep playing:**
     - A live tick sends exactly one Play, with gain 0. A take of that playing video commits on its next decoded frame.
     - A scene load sends no Play. Ticked videos wait for Play on the slot or the first take/click.
     - A user-paused ticked video gets no Play until it is taken.
     - A Pause→Keep playing switch sends no Play.
   - Prim faces get no transport and need no grant (m2). A paused prim face with a validated current image is fresh **without** a new publication. A frozen source is fresh the same way. A playing video still needs both of N1's conditions.
   - Refused Play cancels at once. The program's player stopping keeps the texture and sets the status.
   - Transport calls happen only when `owner->desired` must change: zero calls in a steady state (m5).
   - **On-air protection (N3):** with 7 Keep-playing URLs and caps of 2 normal + 4 low, the program, pending and from players keep at least LOW, and an off-air player is the one retired.
9. **Audio:**
   - Gains: program 1, others 0, camera on air → all 0, fades URL↔URL, URL↔face and face↔face.
   - A shared impl takes the max.
   - A Volume edit stays muted off air.
   - Untick, delete, or VCam off restores 1. VCam off goes through the `!enabled` branch.
   - **Rebind (Codex #7):** re-pick an off-air prim-face slot to a new face. The old impl returns to gain 1 and the new one is muted.
   - No `ObjectMedia*` message is sent.
10. **Copy key:** a cut between A and B with equal serials still copies (C5).

**`llprismprojector_test.cpp`:**
- **2048:**
  - A Gate with a 2048 projector: program, pending and from **cameras** get 2048. Off-air cameras and videos get 1024, unless a camera has its own 2048 projector, which keeps it at 2048 (Codex #9).
  - The Gate texture capacity equals the held source's.
- **Thrash (a):** 120 frames with the Gate demanded on alternate frames, no cut. Zero reallocations, and the output identity is stable.
- **Live change (C3):**
  - Raising or lowering the Gate projector while on air: every frame, the program camera and the Gate keep `mHasOutput` with a complete texture until the swap.
  - An unfed camera with no consumer is released at once.
- **Max (b):** a 320×180 own face plus a 2048 Gate gives 2048. An own 2048 projector plus a 1024 Gate stays 2048.
- **Routing:** GATE passes `canRouteSource`/`validateProjectorReassignment`. The retargeted two-source branch sets primary = Gate and incoming = program, with α, for world, aux and cookies. Previews get α = 0.
- **Projector-only Gate (Codex #2, N2):** a Gate with only a projector calls `preparePrismProjectorFeeds(gate)` once per publication: a steady copy, a cut and each fade step. Its cookie source generation advances every time.
- **Throw (m7):** a Gate projector with Throw = Camera FOV gets the held camera's FOV and aspect.
- **Consumers (B1):** a Gate snapshot goes through raw preview, face mapping (square face, Fill + anchor, rotate, bars), `getProjectorFeed`, aux composite and the manager list, with no camera payload and no crash.

**`aluiperfselftest` `vcam_gate_simple`** (native; in-world as 5kvideoeye).
- Fixture: cameras A and B, a URL slot, and the Gate with a virtual screen, plus a prim face, a projector and a prim-face video when the fixture prim exists.
- Every Gate-card widget is clicked through its router, with registry read-back.
- **Cut to B:** the face's composite slot is the Gate; the first B frame is newer than the click; A stops rendering within 2 frames. The Gate monitor tile shows B by its next preview refresh (decision A).
- **Crossfade 0.5 s:** both sources render during it. Camera↔video endpoint pixels on the face and the cookie match the sources' direct output (C11). Mid-fade, the face and the projector cookie show the blend at the same α.
- **Mid-fade loss:** a URL ending mid-fade, for example a short clip, returns at once to `from`, with no black frame (Codex #1).
- **Projector-only Gate:** its cookie updates on every cut and fade step (N2).
- **4:3:** a 4:3 source on a 4:3 face fills it, with no double letterbox.
- **2048:** Gate and program are 2048 even while A has its own small face. The allocation count is flat for 300 frames. During a live 1024→2048→1024 change, the retained capture and Gate textures are never blank. **Exception (decision B):** a projector cookie may show one brief No-signal frame.
- **Failures:** an injected copy failure keeps the program. Delete B → A goes on air. Delete A → black plus status. Pan the Gate face away, let Auto cut, pan back: the held picture shows until B is fresh.
- **Audio:** gains read back for URL↔face fades.
- **Scenes:** save/clear/load, and an old `gate_source` fixture, load clean with the face on the Gate.
- Also run: `prebuild_guard`, `zs_check`, `xui_layout_check` with zero findings, and a screenshot compared with the mockup.

**In-world recording:** a prim TV face and a 2048 projector on the Gate. Run 20 cuts, 20 camera↔video crossfades and an Auto cycle. Check: no black, stale, out-of-order frame or exposure jump on faces and screens, no stale or blended cookie after a rollback, and only the on-air audio is heard.

The resolution of the 2048 projector is not changed during the recorded run. A resolution change may show decision B's brief cookie No-signal frame.

## h. Risks

1. **Cadence.** One camera renders per frame (H:11800). A cut holds for at least one frame, and a fade halves each camera's rate (judder, not staleness). Measure it in the recording.
2. **2048.** Capacity keyed on a watch or demand thrashes (pitfall a), and a non-max size shrinks the program (pitfall b). Both are tested.
   - **VRAM (m9):** about +24 MiB per on-air 2048 camera (+48 MiB during a fade), plus +32 MiB for the Gate texture at 2048.
   - **Cookies (decision B, accepted):** Gate cookies use W:'s committed cookie path, with no spare-and-swap. A user resolution change, or a VRAM allocation failure, can show one brief No-signal frame on that projector.
3. **Previews (decision A, scope).** The Gate's monitor tile and Raw output are cut-only and refresh at the preview cadence (0.5–5 Hz). They are not frame-accurate monitors.
4. **Media caps.** At most 2 normal plus 4 low players are allowed, under a CPU limit. Beyond that, off-air players stop first, because on-air demand outranks them (N3). The status says so, and the next take restarts them. Muted off-air players still cost CPU. The first take of a URL that is still downloading can fail at 5 s ("not ready: downloading").
5. **Audio IPC.** A fade sends one volume update per changed gain per frame to each of two players. This is small, but not free.
6. **Merge.** W: and this change both edit lens, pipeline, tests and vcam_page.json. Revert function by function, never whole files.
7. **Scenes.** Migration uses one of the 8 slots. Older builds cannot read the new `source_kind`.

## i. Review resolution

| Finding | Resolution | Section |
| --- | --- | --- |
| Opus B1, C1, C10 | Gate signal contract, `cameraData()` rule, site list, Fit/Fill predicate, per-consumer test | §b, §g |
| Opus B2 | Unfresh commit holds the last Gate frame; compose only when prepared | §d5, §c |
| Opus M1, C3 | W: spare/swap resizing; release only when unseen | §c 2048, §g |
| Opus M2, C9 | Cursor past the last attempt; eligibility; restore only if eligible; retry deadline | §d2, §d6 |
| Opus M3, C8, C14 | One media rule (pending ⇒ Play); paused prim face fresh; level-triggered Pause; cap status | §d |
| C2 | Baseline = decoder image at pending (`mGateDecode*` kept) | §d3-4 |
| C4 | `gateStep` proposes, `gateApply` commits on success | §c, §d |
| C5 | Copy key includes id and generation | §c |
| C6 | Local per-impl gain for URL and prim face; shared max | §d Audio |
| C7 | Grants guard every Gate-originated Play | §d2 |
| C11 | 1:1 copy, source aspect; fades via HEAD two-source pass; no fixed 16:9, no media weight | §c |
| C12 | `composeGate` before the atlas; cookies prepared after every publication (see R2 #2); a failed cookie keeps the last one | §c |
| C13 | Loader gate branch; late source resolution; malformed-record cleanup | §b |
| C15 | Stale refs (#14, panelvolume.h:161, On cut sites); probes rewritten, not deleted | §e, §f |
| C16 | One Gate (added limit), up to 7 sources, inherited exclusions | §a |
| Coordinator 10/19 | (1) per-consumer fade blend for faces, screens and projectors (previews: see decision A); (2) Keep playing: a live tick plays the video muted; no autoplay after a scene load; a user Pause is kept; (3) an unseen commit is a Cut | §a5, §c, §d, §g |
| **R2** Decision A; Codex #3, #4 | Previews show only the Gate texture (cut-only, preview cadence). Keep preview H:1205/H:1276 (the HEAD shader needs `prismPrimaryMediaSDR`); everything else returns to P: | §a5, §c, §e, §h3 |
| R2 Decision B; Codex #5 | Cookies use W:'s committed path, with no spare-and-swap; a brief No-signal frame is an accepted risk | §c, §h2 |
| R2 Codex #1 | Incoming output lost mid-fade → the one failure transition, back to `from` | §d6, §g5b |
| R2 Codex #2, Opus N2 | `preparePrismProjectorFeeds(gate)` after every publication | §c, §g |
| R2 Codex #6, Opus m1 | One 5 s deadline for pending, unseen-commit and fade-end publication; one failure transition to `held` | §d6, §g5a |
| R2 Codex #7 | Per-frame gain reconciliation; departed impls restored; shared impls take the max | §d Audio, §g9 |
| R2 Codex #8 | A Crossfade only from a valid Gate picture; otherwise a Cut | §d5, §g4 |
| R2 Codex #9, Opus m6 | H: pinned to cb1ed737385; W: = 98fd1f60054; Gate-before-sources test; own-2048 exception | header, §g |
| R2 Opus N1 | Video freshness also needs publication > baseline | §d4, §g3 |
| R2 Opus N3 | On-air `mediaDemand` = 2 × 1024², above the floor | §d Media, §g8 |
| R2 Opus m2, m3, m5 | Prim faces need no grant; frozen-with-output counts as fresh; transport keyed on `owner->desired` | §d |
| R2 Opus m4, m7, m8, m9 | W: Gate hooks deleted; held-camera FOV/aspect in the signal; manager sites; Gate texture VRAM | §e, §b, §h2 |
| **R3** Opus R3-1 | The deadline counts only demanded frames; an unseen Auto Gate never fails back; a lapsed pending source becomes an unseen Cut with its clock paused | §d6, §g4b |
| R3 Codex #1 | Rollback and cancellation frames publish the restored presentation; cookies are dirtied and then prepared | §c, §d6, §g4c |
| R3 minors (both) | Paused-face and frozen sources exempt from the publication-advance test (N1 kept for playing video); incoming fallback to primary-only; Gate never preview-quality; gain reconcile in `!enabled`; "never blank" limited to retained textures, with decision B's cookie exception | §d4, §c, §d Audio, §g |
| Opus minors | Gains in `updateVolume`; late resolution; `enabled` arms; preview-quality excluded; recopy on realloc; anchors 6453/6479, 1002, 8506 | §b-§e |
