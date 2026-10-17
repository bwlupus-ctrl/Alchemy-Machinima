# Media slots: Slice 1 implementation and triage

Base: `feature/director-kit-p0`, `de73653d1b3e2cf81c87ea3b1861c68086f66d0e`.
Specification: `SPEC_media_projector.md` v4, including the round-4 enqueue rollback note.
The development setting `VCamMediaSlotsEnabled` is Boolean and defaults to FALSE.
No viewer build, commit, push, source navigation, source resizing, audio changes,
asset upload, object update, or VCam history implementation was performed.

This slice enables an existing, normally admitted LibVLC face to supply completed
images to a media slot and its raw monitor. Display/projector routing stays closed
for media until Slice 2. Runtime visual/lifetime acceptance remains unexecuted.

## Review fix round (2026-10-17)

Both Opus and Codex confirmed the round-1 fixes in their round-2 reviews.
The three remaining round-2 P1s and the accepted general cleanup are recorded
below; final acceptance still requires re-review and runtime validation.
No viewer/plugin/test executable was built or run, and no commit or push occurred.

### Follow-up P1: disable during a tracked upload (2026-10-17)

The pre-import guard in `LLViewerMediaImpl::update()` now checks
`mMediaUploadState.pending` independently of `VCamMediaSlotsEnabled`. Disabling
tracking and replacing VLC with accelerated CEF cannot reconfigure the upload's
`LLImageGL` while its worker is outstanding. Completion/rejection and terminal
cleanup retain the existing pending/ref cleanup. With no tracked job pending,
the guard is false, including for legacy OFF uploads; the legacy upload/enqueue
block and three legacy texture helpers remain byte-identical to HEAD after
line-ending normalization.

Case 26 in the existing `llprismprojector` target covers tracking disable with an
outstanding upload, source replacement, stale completion rejection, terminal
cleanup, and OFF with no tracked job. This is a state/lifetime regression case;
it does not exercise accelerated CEF or a live GL worker race. No executable was
built or run. Follow-up gate results are recorded below.

### Round 2

| Item | Change and evidence |
| --- | --- |
| P1-A: setting transitions | Upload selection uses the source's actual `TrackingState::active`, independently of the global setting. An existing untagged source keeps consuming legacy dirties until the first matching tagged frame activates tracking. The setting signal configures all existing sources once. VLC receives an in-place `enable_media_frames` subscription change: no navigation, reload, stop, resize, seek or audio change. Enable issues a fresh token and republishes completed current-player pixels, including paused/static media. Disable clears token loads, immediately selects legacy uploads, invalidates queued tracked uploads through the layout epoch, and switches decoder notifications back to the original sender. Decoder callbacks read only atomic flags; subscription tokens belong to the plugin message thread. A callback context remains owned by its producing player until stop/release quiesces it, so retired players cannot acquire a later subscription token. Case 25 in the existing llprismprojector target covers existing-untagged OFF -> ON -> OFF, disable before the first tagged reply, active-player disable, repeated ON, re-enable and new-load provenance. |
| P1-B: reopened static previews | Demanded media with retained output re-offer the accepted publication serial and accepted output quality to the preview service. `notePublication()` is idempotent for unchanged serial/quality. A cleared demand entry is repopulated even if hide/reopen occurred between registry updates; the atlas can redraw the retained slot output without another decoded frame or slot copy. No new production serial or cadence sample is fabricated. |
| P1-C: persistent GL errors | All three opt-in `glGetError()` drains (worker allocation/upload, main fence wait, placeholder allocation) stop after at most eight queries. Persistent errors still fail the subsequent operation check and prevent promotion/readiness. |
| Accepted general fix: LibVLC release | Keep the ungated `stopPlayer()` release of the previous player and `mLibVLCMedia`. This fixes their pre-existing leak for both OFF and ON playback and preserves callback quiescence before context/pixel retirement. It is intentionally a general lifetime fix, not feature-off isolation. |

The legacy viewer upload/enqueue/helpers and original VLC `display`/`setDirty`
bodies remain unchanged. The VLC callback wrapper now records whether a frame
exists and reads an atomic tracking flag in both modes so existing players can
switch safely; OFF uses the original immediate sender and performs no deferred
notification or geometry polling. This transition support supersedes round 1's
creation-only opt-in description below. Round-2 gate results are recorded under
Validation.

### Round 1 (confirmed by both reviewers)

| Item | Change and evidence |
| --- | --- |
| B1: worker cleanup / non-atomic refs | The ON worker lambda is mutable and resets its upload capture before returning, before `postTo` can publish the reply. The main reply finishes and releases its capture. The upload's custom deleter only deletes on main, so latch rollback, both `LLRefCount` unrefs, fence cleanup and deferred texture deletion stay on main. A reply/task discarded on a worker during queue shutdown deliberately leaks that job and its retained resources; it cannot destroy the impl or touch the deferred-delete list there. Enqueue rejection and normal/stale/failed completion still clean up on main. OFF retains the original main-thread unref callback. |
| P1-2: static/paused faces and resize | Both branches call texture update, then `setPlaying(true)`, before inspecting the dirty rectangle. The OFF helpers are restored verbatim. The ON helper also updates the used-width/height mapping without a decoder dirty; mapping retention and descriptor validation still precede `resetDirty()`. |
| P1-3: feature OFF isolation / strings | The OFF update/enqueue block and all three legacy upload/texture helpers match HEAD byte-for-byte (after line-ending normalization). Identity, retained mapping, validation, retries and upload allocations are confined to ON. Reconciliation in media update/teardown/load and auxiliary rendering is setting-gated; disabling reconciles once through the setting signal. Layout tracking is opt-in for sources; VLC load tokens, deferred callback notification and geometry polling are opt-in for VLC loads. Untagged VLC uses its original `display` and `setDirty` bodies. Legacy shared-memory lookups avoid mutex/shared_ptr churn. With the setting OFF or no media entries, Director and VCam restore the original captures/cameras text and tooltip. |
| P1-4: GL false success | Only ON establishes GL error boundaries around placeholder allocation, pending-image allocation, pixel upload and fence/wait operations. A GL error fails the job before GL-name promotion, completed-serial advance or readiness publication. Matching failed uploads union the rectangle back into dirties and retry once per normal eligible update; allocation failures leave dirties intact. |
| P1-5: anamorphic Auto aspect | LibVLC queries the selected video track's `i_sar_num/den` through `libvlc_media_tracks_get`, releases the track list, and tags SAR with the same load token as coded dimensions. The viewer carries SAR through completed-image metadata and computes Auto as `(width / height) * (SAR numerator / denominator)`; unavailable/invalid SAR uses square pixels. SAR-only changes trigger metadata publication and accepted slot layout refresh. Manual aspect choices still win. Case 24 now includes PAL 16:9/4:3, NTSC 16:9, invalid SAR and manual override checks. |

The SAR fields and track-list ownership follow the bundled LibVLC 3 headers and
[VideoLAN's LibVLC 3 implementation](https://github.com/videolan/vlc-3.0/blob/master/lib/media.c).
Previously accepted source admission, token provenance, retained mapping bounds,
bounded retry, shader/SDR/orientation, camera/source tagging and S2/S3 routing gates
remain in place. Runtime fault injection, paused-face/resize visual checks,
anamorphic playback and shutdown stress still need execution; syntax checks do
not establish runtime acceptance.

## Implemented contract

- `SourceSlotData` owns a private tagged variant. Empty slots have no payload;
  CAMERA creation is explicit, and MEDIA_FACE has no camera payload. Old public
  `mMode`, `mCamera`, and `mCameraObjectId` accesses no longer exist on runtime
  slots or snapshots. `cameraData()` returns null for media and empty slots.
- `mSlotCount` counts occupied snapshot entries; `mCaptureCount` counts CAMERA
  entries, including real surface lenses. Capacity and handle identity remain
  shared across eight slots. Scheduling filters camera payloads before camera work.
- Successful player creation records the platform-resolved plugin basename.
  Teardown clears it and advances the source epoch. Only a non-null actual
  `media_plugin_libvlc` source is accepted. Faces/ownership/policy/plugin/load
  identity are rechecked before media interest work, on teardown/load, and before
  slot copying. A slot retains a media texture, never an impl or face pointer.
- Each opt-in VLC load/subscription has a non-reused token. Player callback contexts
  belong to their producing player and are retired only after stop/release
  quiesces that player. Setting transitions change only the current player's
  message-thread subscription token; decoder callbacks never read that token.
  Decoder callbacks set an atomic flag; the normal message thread sends the
  tagged frame notification, `libvlc_video_get_size` geometry and selected-track
  sample aspect ratio. Untagged loads keep the original callback notification.
  Time-only
  updates cannot produce image readiness. Metadata never calls player setSize.
- One immutable pending CPU upload retains its mapping before consuming dirties.
  The parent map uses shared mapping ownership: removal responses and parent
  destruction drop only their own references. The pending upload independently
  keeps its mapping mapped. With one pending upload per impl, at most one retired
  mapping per impl can be retained by this handoff, including across replacement.
  Descriptors validate row bounds/extent before exposing pixels to the worker.
- Main/worker upload success is explicit. Worker fence ordering precedes a single
  main-thread current-identity/base-GL-name check, GL-name promotion and completed
  serial publication. Rejected/failed/stale jobs cannot promote old GL names.
  The existing NVIDIA worker synchronization workaround is retained.
- Failure re-dirties the matching rectangle, unioning newer dirties. One attempt
  occurs through the normal policy-gated update, with no additional pixel buffer
  or extra pending job. Main-thread terminal cleanup rolls back the pending latch
  and both explicit refs after enqueue rejection or completion. A discarded queue
  capture cleans up only on main; worker-side shutdown discard deliberately leaks
  the job. Mappings release only after worker reads stop.
- Media copies use the fixed retained slot target and preflight the shader,
  sampler channel, completed image and target before drawing. The shader samples
  the entire used buffer at level zero, excludes padding, uses VLC's existing
  OpenGL orientation, and decodes SDR to linear RGB with zero glow alpha. Raw
  previews encode once using the ungraded, non-tonemapped preview permutation.
- Auto aspect comes from tagged video geometry and sample aspect ratio, with a 16:9 fallback; 16:9, 4:3
  and 2.39 overrides are per slot. An aspect change schedules a new accepted slot
  publication from the current completed image. Held images retain their accepted
  metadata. Copy cadence shares the bounded adaptive entitlement calculation;
  camera render and media copy counters are separate.
- Media preparation/copy/atlas work happens before the zero-camera return and
  after the `sPrismLensRender`, deferred-context and `gCubeSnapshot` exclusions.
  Media never allocates a world-render scratch pack, depth or exposure resources.
- VCam has Add media slot, a transient exact-face picker, Source aspect, source
  status and existing removal. Picker cancellation writes nothing; completion
  checks panel lifetime, selected handle/generation and registry revision before
  one validated descriptor commit. Camera cards/actions are guarded or hidden.
  Add is hidden when the development switch is off. No undo was introduced.
- Basic media descriptor scene carriage is included to preserve isolated test
  fixtures: explicit kind/object/TE/aspect/rate, no runtime state. Unknown kinds,
  malformed descriptors and media gate arms fail before mutation. Full mixed
  routing/persistence acceptance remains Slice 3.

## Complete camera-assumption triage ledger

Line references below are the original v4/base references, so they remain
comparable to the plan after the implementation moves code. “Deferred S2” and
“Deferred S3” are explicit closed-gate dispositions, not claims of completed
media routing or runtime acceptance.

| Site | Decision / resulting invariant |
| --- | --- |
| `llprismlens.h:729,770,926,973`; `llprismlens.cpp:1846` | Private tagged producer payload shared by runtime and snapshots. Empty and media slots have null cameraData; no public forwarding fields/default camera payload/fallback mode. Projector feed carries explicit source-kind space for the later routing work. |
| Registry add/free/remove/clear, `MAX_CAPTURES`, `CaptureHandle` | Common occupied-slot identity/capacity. Explicit camera initialization in all three camera creation paths. Media removal uses the same stable handle/generation and does not compact physical slots or other bindings. |
| `llprismlens.cpp:6187-6197` snapshot publication | Compact occupied entries by mSlotCount; increment actual mCaptureCount only for CAMERA. Copy the tagged payload atomically with common metadata and accepted aspect/serial. |
| Registry default labels and `setLabel` | Common labels; media defaults to Media, camera labels read camera settings only under the camera tag. |
| OTS rollback/capacity, `llprismlens.cpp:8966` | Capacity subtraction/iteration uses occupied slots. OTS eligibility/replaceability remains camera-only. Rollback copies the entire tagged payload; unrelated media slots/routes retain identity. |
| `llfloaterdirector.cpp:7123` and count/status summaries | Capacity and summary iteration use mSlotCount; summary distinguishes occupied slots from actual camera count. Existing camera-only OTS tests stay filtered by camera eligibility. |
| Adaptive controller, `llprismlens.cpp:364` | Overall presence is source-slot presence. Existing controller remains the protection ceiling. Media never submits fabricated world-render timing. |
| `prepare()`, `llprismlens.cpp:3931` | Dispatch guard rejects MEDIA before the surface/camera branches. Media prepares only an image descriptor and logical copy extent. |
| `updateCadenceEntitlements()`, `chooseRenderSlot()`, `llprismlens.cpp:4053` | Common water-fill entitlements include only demanded media with pending frame/layout work. One calculation is reused for mixed slots; the world-render chooser requires a camera payload. Media copies and camera renders have distinct counters. |
| `renderAuxiliaryView()`, `llprismlens.cpp:9727-9760` | Reconcile, prepare, copy and update atlas before actual-zero-camera return; recursive/cube/deferred exclusions precede media work. Existing camera render scheduling remains one attempt per presented frame. |
| Display target validation/rebind, `llprismlens.cpp:2186,2231,2695,2768` | Common handles/counts migrated; active media display routing explicitly remains rejected until S2. Camera self-source conflict checks only run after camera-feed eligibility. Surface apertures stay immutable. |
| Projector target validation/reassignment, `llprismlens.cpp:2364` | Capability helper admits explicit MEDIA but live registry bind/rebind remains camera-only for S1, including direct API calls. Deferred S2: shared media routing and all destination/conflict rules. |
| Feed resolution, `llprismlens.cpp:5782,5817` | Explicit media rejection before FOV/settings reads. This also closes media no-signal/local-projector sampler overrides while routing is unfinished. Deferred S2: accepted media feed/cookie identity and common signal metadata. |
| `getCompositeStates()`, `getAuxCompositeStates()`, `llprismlens.cpp:10462,10730` | Guard camera optics; world/aux display compositing explicitly excludes media for S1. Deferred S2: media surface composition/fit/looks. |
| `buildSignalCompositeState()`, `buildPreviewCompositeState()`, `llprismlens.cpp:10334,10921` | Common accepted image region; media's accepted logical aspect and SDR tag; camera optics only with cameraData. Raw media preview uses that region/aspect with no camera exposure or grading. |
| `buildProjectorCompositeState()`, `llprismlens.cpp:11030`; `pipeline.cpp:2105` | Common slot output handles are retained; no media projector binding can reach preparation in S1. Deferred S2: prepared-cookie preflight, fit/looks/mips, accepted publication and allocate-before-release resize spare. |
| `llprismpreview.cpp/.h` | Common stable slot demand/publication identity and retained target. Media preview uses permutation zero and per-slot invalidation, preserving camera tiles on media-off. Camera scratch warmup stays reachable only from a scheduled camera. |
| `alvcammonitorstrip.cpp/.h` raw/output tiles, labels, paging/activity | Common fixed slot indices/labels/status/atlas handles, including media-only pages. No camera reads needed for tile identity. Media carries a MEDIA label through the common selector. |
| `alvcammonitorstrip.cpp:130,640-648` | Drag checks surface capability directly, without a default camera mode. Media is not mistaken for an immutable aperture; actual output binding remains rejected by the live S1 routing gate. |
| `llprismlens.h:871-876`; `tests/llprismprojector_test.cpp:234-238` | Reassignment helper consumes tagged source capability. Tests cover surface rejection, explicit camera acceptance and explicit media capability, plus absent public fields and no default payload. |
| Scene writer/parser/apply, `llprismlens.cpp:6370,6745` | Branch explicit source_kind. Basic descriptor serialization/restoration includes media without reading camera fields; legacy camera parsing is preserved. Full schema/mixed-output failure injection/round-trip acceptance deferred S3. |
| Scene binding guards `llprismlens.cpp:6574,7323,7359` | Common references remain generation/identity checked; media output binding restore stays rejected under the S1 camera-only routing gate. Media gate arms explicitly fail. Deferred S2/S3: restore media display/projector routes. |
| Director scene carriage `llfloaterdirector.cpp:4045-4047` | Existing carriage of all three arrays remains intact; no parallel registry/container is introduced. |
| Gate arm/list/prewarm/cuts and gate eye source | Camera-feed predicates run before camera object/settings access. Media cannot arm, prewarm, inherit a gate route or become a default virtual camera. |
| Camera guide/map drawing `llprismlens.cpp:9374` | Iterates occupied snapshots but filters CAMERA_FEED before any transform/guide read. No icon/frustum is emitted for a media source prim. |
| Camera transform/optics/bone setters | Existing camera-feed gates now include the explicit tag. Media actions fail before accessing camera payload. |
| Rig feed selector `alpanelcinelightrig.cpp:1237-1255,1305` | Occupied-slot iteration plus explicit camera-feed predicate; no raw mode read. Deferred S2: admit MEDIA, keep combo visible in media-only rigs, and route it. Current closed-gate behavior does not claim media-only rig support. |
| Texture picker `lltexturectrl.cpp:1438-1455` | Iterate occupied slots; common handle/output size labels. Media raw atlas already applies accepted source aspect, so the picker preserves the full 16:9 atlas cell and does not fit it twice. Preview/selection work without camera-count filtering; actual binding remains S2. |
| Build display binding `llpanelface.cpp:3432-3437,3464,3540` | Resolve common stable slot references with mSlotCount. Keep existing target validation/cancel path. Deferred S2: allow media display binding through that validation. |
| `llpanelvolume.cpp:1169` picker cancellation lookup | Common occupied-slot handle resolution; no camera-count-based failure. Existing light settings/cancellation ownership remains intact. Media projector binding remains S2. |
| `alsteppedanimruntime.cpp:280-283` | Guard CAMERA_FEED before the first bone settings read or anchor lookup. MEDIA does none of that work. |
| `alvcambonepov.cpp:315-317,345-380` | Filter kind before payload/anchor reads and before smoother selection/update. Unseen smoother cleanup remains intact. |
| `alvcampagemodel.cpp:172,470,495` | Common selection/labels/rates/removal/monitor handles use slots. Gate/display capability remains explicit camera-feed eligibility. New Add media command uses the same registry/model publication path. |
| `alpaneldirectorvcam.cpp:180,509,627-660` | Guard bone-choice callback; common output lists/counts; camera settings/card refresh requires cameraData. Media has its own descriptor/status/aspect controls. |
| `alpaneldirectorvcam.cpp:1055,1140-1166,1205-1259,1547` | Camera snap/reset/source-type/generic setters and paste require cameraData. Media aspect/rate branches happen before the generic camera setter. Picker uses current handle/generation/revision and writes only on validated completion. |
| `llfloaterprismmanager.cpp:560-734` | Bone callbacks check camera payload before reads; camera edit commit remains guarded. |
| `llfloaterprismmanager.cpp:915-1046,1154-1290` | Common list/detail labels and counts use slots; media label is explicit; camera optics detail only inside camera-feed branch. Rates remain common. |
| `llfloaterprismmanager.cpp:1398-1566,1846-2289` | Gate/camera/transform/optics actions use kind-aware predicates. Common labels/removal stay available; camera-only display/projector actions remain closed for media until S2. |
| `pipeline.cpp:10662-10665` live probe signature | No payload read to migrate; currently reached only for camera feeds because S1 media projector feed resolution returns false. Deferred S2: add slot kind/generation/source epoch and accepted cookie revision within the existing refresh budget. |
| `llreflectionmapmanager.cpp:1293-1303` structural key | Same closed S1 routing gate; existing camera structural key preserved. Deferred S2: add media structural identity/epoch without pending-frame/video-serial churn. |
| Round-4 `alcinelightrig.cpp:1213` live rig signature | Deferred S2 with media cookie routing: add accepted-cookie revision as a change trigger without changing structural identity or refresh budget. |
| Round-4 `llreflectionmapmanager.cpp:650,1638` on-demand signature | Deferred S2 with media cookies: add accepted-cookie revision to the on-demand live signature; retain its ordinary structural/published-light tokens and refresh budget. |
| `aluiperfselftest.cpp:139-141` | Selected-source lookup iterates occupied slots. Camera probes additionally require a camera payload. |
| `aluiperfselftest.cpp:6141,7495,7532` | Snap/test-pattern oracle reads require cameraData before first read; existing camera assertions remain. |
| `aluiperfselftest.cpp:6482-6486` | Kind-aware fixture selection; a media-only fixture clears cameras, creates no player, and exercises Add/aspect through independent kind/count/revision oracles. No silent zero-camera skip for media controls. |
| Generated VCam rows, search inventory and functional manifests | Five additive rows with stable IDs; Add/aspect use media-only model oracles; Pick is explicitly DEFER_INWORLD/requires_world. The custom projector-list tag is registered in the active CONTROL_TYPE map. No functional threshold/assertion/legacy pin was reduced; the exact source-row pin increases by the five added rows. Full search aliases/freeze/resume fixtures remain S3/S2. |
| `syncProjectorVolumetrics()` / `releaseRenderResources()` | Master/kind eligibility checked before acquisition's same-target early return. Master-off releases owned overrides; media-off cannot reacquire from a snapshot/status query. No media bindings are admitted during S1; per-light declined state is preserved. |
| `llviewermedia.cpp:3136-3139` enqueue rollback | ON rejected postTo executes failure/re-dirty, then main-thread terminal cleanup clears the pending latch and releases both refs. The worker resets its capture before reply publication. A discarded reply/task on a worker deliberately leaks; it cannot unref viewer objects or enqueue deferred GL deletion there. OFF keeps the original queue and unref code. |
| `llpluginclassmedia.cpp:332-339`; parent remove response/destructor | Upload obtains shared mapping ownership before its raw pointer/dirty consumption. Map retirement/destruction cannot unmap the pending job's region. No parent/process is retained. |

## Remaining slices

Slice 2: owner-scoped demand/bootstrap and admission/priority integration; all
display/projector/rig/Build/texture-picker binding paths; media output fit/looks;
cookie preflight/mips/accepted identity and allocate-before-release resizing with
one temporary spare; all four probe change-trigger sites; output-driven cadence;
freeze/resume and owned volumetric lifetime under all enable combinations.

Slice 3: complete mixed-scene and malformed-input acceptance, full source/output
UI/search and functional coverage (including freeze/resume), picker in-world
commit/cancel proof, diagnostics/failure injection, mixed-camera regressions,
SDR/aspect/orientation comparisons, paused-frame recovery, queued-mapping lifetime
stress, GL reset/shutdown, beauty/feature-off tests and measured performance.
Basic descriptor scene carriage and picker-local cancellation are already present;
their comprehensive acceptance is not claimed by syntax checks.

## Validation

### Follow-up P1: disable during upload

- PASS: `zs_check -Files` for the uncommitted changed C++ files: 19 mapped TUs,
  including `llviewermedia.cpp` and `llprismprojector_test.cpp`. The unmapped
  nested LibVLC TU and case 26's own integration-test target also passed `/Zs`
  using the existing runner and unchanged project flags.
- PASS: `C:\Users\xianw\cxs\prebuild_guard.ps1 -ChecksOnly`, all gates, exit 0,
  before object purge. Existing workspace cache redirect and full-path Python
  routing were reused; no gates, flags, thresholds or assertions were loosened.
- PASS: HEAD comparison of legacy upload/enqueue/helpers, original VLC callbacks,
  and the accelerated OFF block (line endings normalized); the tracked-only
  pending guard precedes accelerated import. `git diff --check` also passed.
- Case 26 was syntax checked only. Live disable/VLC-to-CEF worker-race validation
  remains unexecuted. No viewer/plugin/test build, commit or push occurred.

Logs: `.tmp/media_slice1/p1_disable_zs.log`, `p1_disable_extra.log`, and
`p1_disable_prebuild.log`.

### Round 2

All required gates passed without changing gate scripts, flags, thresholds or
assertions. Python calls used
`C:\Users\xianw\AppData\Local\Python\pythoncore-3.14-64\python.exe`.

| Check | Result |
| --- | --- |
| `zs_check -Files` for every changed C++ file | PASS: 19 mapped TUs, including the expanded test TU. The nested LibVLC TU also passed using the unchanged gate flag parser and its real plugin project flags. |
| Exact llprismprojector test target flags | PASS (`cl /Zs`), including new case 25's existing-untagged OFF -> ON -> OFF and tagged-player transition checks. No test executable was built or run. |
| Legacy source comparison against HEAD | PASS: viewer legacy upload/enqueue/helpers and original VLC `display`/`setDirty` bodies remain byte-identical after line-ending normalization. Only the temporary comparison helper's selector anchor was updated from the global setting to actual source tracking. |
| `C:\Users\xianw\cxs\prebuild_guard.ps1 -ChecksOnly` | PASS: UI, draw, compile, light-map maths, kit generation/lint/parity, XUI selftest, page sources and both theme checks. Exited before object purge. |
| `git diff --check` | PASS. |
| Reviewer re-review and viewer/plugin/GL runtime acceptance | NOT RUN. The new transition assertions were syntax checked only; in-world setting toggles, paused/static preview reopening, persistent-context-error injection and shutdown stress still require execution. |

Round-2 logs: `.tmp/media_slice1/zs_round2.log`, `extra_round2.log`,
`legacy_round2.log` and `prebuild_round2.log`. The supported workspace cache
redirect and process-local Git ignore/Python handling described below were
reused. No viewer/plugin/test build, commit or push occurred.

### Round 1

Review-round checks use the same unchanged gates and the requested full Python
path. `zs_check -Files` selected all 20 changed C++ files: 19 mapped TUs passed;
the nested LibVLC TU passed with the gate's unchanged flag parser and its real
project flags. The projector test TU also passed with its own integration-test
project flags. The final reconciliation edit received a further explicit
`zs_check -Files indra/newview/llprismlens.cpp` PASS.

`check_legacy_review.py` passed byte-for-byte comparisons against HEAD for the
OFF update/enqueue/unref block, both legacy upload helpers, `updateMediaImage`,
and the untagged LibVLC `display`/`setDirty` functions (normalizing line endings
only). No identity/mapping/validation/retry/GL-error code occurs in that OFF block.

The first checks-only guard passed UI, draw, compile and maths, then correctly
failed kit generation/lint on stale generated source fingerprints after the
Director string fix. Running the existing generator refreshed those headers;
no gate, threshold or assertion was edited to address the failure. The final
checks-only rerun **PASSED every gate** and exited before object purge:

| Review-round gate | Result |
| --- | --- |
| `zs_check -Files` for all changed C++ files | PASS: 19 mapped TUs; nested LibVLC PASS with the real plugin project flags; projector tests also PASS with their exact target flags. Final lens edit rechecked explicitly: PASS. |
| OFF source comparison against HEAD | PASS: original upload/update/texture helpers and untagged VLC callback/sender bodies. |
| `C:\Users\xianw\cxs\prebuild_guard.ps1 -ChecksOnly` | PASS: UI, UI draw, compile, maths, kit generation, kit lint, parity, XUI selftest, page sources and both theme checks. |
| `git diff --check` | PASS. |
| Opus/Codex re-review; viewer/GL/runtime acceptance | NOT RUN. The incoming reviews remain NOT GREEN pending re-review. |

Current-round logs: `.tmp/media_slice1/zs_review.log`, `extra_review.log`,
`zs_review_final_lens.log`, `legacy_review.log`, `regen_review.log`,
`prebuild_review.log` and `prebuild_review_final.log`.

### Original implementation checks (prior to this fix round)

The required-site ledger above is supplemented by
`MEDIA_SLICE1_ACCESS_AUDIT.txt`: 325 individual baseline access/declaration lines.
Unit cases 22-24 and the expanded case 14 are added to the
existing llprismprojector target. They are syntax checked with that target's real
Release flags; no llprismprojector or viewer executable was built or run. The
required guard's existing standalone light-map maths harness did run.

| Check | Final result |
| --- | --- |
| `C:\Users\xianw\cxs\zs_check.ps1 -Files <changed cpp files>` | PASS: 19 mapped TUs, including tests. Two mechanical-edit failures in the first run were corrected and rechecked. |
| Nested LibVLC TU | The script's top-level mapper skips this nested project. Its unchanged flag parser and VS environment were used with the actual `media_plugins/libvlc/media_plugin_libvlc.vcxproj`; `cl /Zs` PASS. No reduced flags or gate edits. |
| Exact test target | Additional `cl /Zs` using `INTEGRATION_TEST_llprismprojector.vcxproj` flags: PASS. This covers the unit TU with its own project rather than only the gate's generic newview test fallback. |
| `C:\Users\xianw\cxs\prebuild_guard.ps1 -ChecksOnly` | PASS on final sources: UI, draw lint, compile since `7050a38cc4e`, light-map maths, kit generation, kit lint, light-group parity, XUI layout selftest, page sources, theme check and theme selftest. Exited before object purge. |
| Regeneration | `director_kit_gen.py`, `director_page_sources.py`, `director_functional_manifest.py` regenerated their outputs. All checks use the requested full Python executable path. |
| `director_functional_manifest.py --check` | PASS: current inventory; zero known-legacy pin retained. Pick remains explicitly in-world deferred. |
| `director_kit_gen.py --selftest` | PASS. |
| `functional_report.py --self-test` | PASS. |
| Focused new-control manifest enrichment, retaining all 47 required consumer rows | PASS: Add/aspect remain RUN with vcam_media model probes and the media-switch prerequisite; Pick remains DEFER_INWORLD. Existing in-world reachability denials remain fail-closed. The optional whole-inventory enrichment was stopped in favor of this focused check; the full inventory gate above still checks all 3996 entries. |
| `git diff --check` | PASS. |
| Viewer/GL/in-world acceptance and execution of the TUT binary | NOT RUN, consistent with the no-build instruction. Syntax and manifest checks do not establish runtime image/lifetime correctness. |

Raw successful checks-only output:

```text
UI gate: OK (XUI changed since 7050a38cc4e pass the layout checker)
UI draw gate: OK (no new per-frame UI cost since 7050a38cc4e)
Compile gate: OK (zs_check -Base 7050a38cc4e -IncludeTests)
Light-map maths gate: OK (alkitmath_harness -Repo I:\alchemy-machinima)
Director kit gen gate: OK (--check)
Kit lint gate: OK (--all)
Director parity gate: OK (--groups light_group)
XUI layout selftest gate: OK (--selftest)
Director page sources gate: OK (--check)
Director theme gate: OK (--check)
Director theme selftest: OK (--selftest)
ChecksOnly: all gates ran; exiting before the obj purge (no files touched).
```

Environment handling: the original guard was executed unchanged. Its nested
syntax check received the supported `-CachePath` option pointing into the
workspace because the default CXS cache is outside writable roots. A
process-local Git `core.excludesFile` override avoids the unreadable user ignore
file while preserving the inherited safe-directory entries; it excludes no
additional files. The existing Python calls are routed to
`C:\Users\xianw\AppData\Local\Python\pythoncore-3.14-64\python.exe`.
Logs and reproducible runners are in `.tmp/media_slice1/` (`prebuild_final.log`,
`zs_final.log`, `extra_final.log`, `functional_check.log`, `kit_selftest.log`,
`functional_report_selftest.log`). Gate scripts, thresholds and existing
assertions were not changed. The exact generated-source row pin rose from 2108
to 2113 solely for the five added rows; no shrink-only pin was reduced.

## Changed files

83 source, generated and report files. The review round refreshed all generated Director source fingerprints after restoring the camera-only text; additional generated-page diffs contain only header changes. The pre-existing modified vcpkg submodule and all unrelated untracked files were left alone. Gate runners, temporary edit scripts and logs are under `.tmp/media_slice1/` and are excluded from this deliverable inventory.

```text
doc/director_kit/MEDIA_SLICE1_ACCESS_AUDIT.txt
doc/director_kit/MEDIA_SLICE1_TRIAGE.md
doc/director_kit/functional/FUNCTIONAL_MANIFEST.md
doc/director_kit/functional/functional_manifest.json
doc/director_kit/page_sources.json
doc/director_kit/page_sources.md
doc/director_kit/pages/cine_light_rig_tab.md
indra/llplugin/llmediaframestate.h
indra/llplugin/llpluginclassmedia.cpp
indra/llplugin/llpluginclassmedia.h
indra/llplugin/llpluginprocessparent.cpp
indra/llplugin/llpluginprocessparent.h
indra/media_plugins/libvlc/media_plugin_libvlc.cpp
indra/newview/alpanelcinelightrig.cpp
indra/newview/alpaneldirectorvcam.cpp
indra/newview/alsteppedanimruntime.cpp
indra/newview/aluiperfselftest.cpp
indra/newview/alvcambonepov.cpp
indra/newview/alvcammonitorstrip.cpp
indra/newview/alvcampagemodel.cpp
indra/newview/alvcampagemodel.h
indra/newview/app_settings/settings.xml
indra/newview/app_settings/shaders/class1/deferred/prismMediaCopyF.glsl
indra/newview/app_settings/shaders/class1/deferred/prismMediaCopyV.glsl
indra/newview/llfloaterdirector.cpp
indra/newview/llfloaterprismmanager.cpp
indra/newview/llpanelface.cpp
indra/newview/llpanelvolume.cpp
indra/newview/llprismlens.cpp
indra/newview/llprismlens.h
indra/newview/llprismpreview.cpp
indra/newview/llprismpreview.h
indra/newview/lltexturectrl.cpp
indra/newview/llviewermedia.cpp
indra/newview/llviewermedia.h
indra/newview/llviewershadermgr.cpp
indra/newview/llviewershadermgr.h
indra/newview/skins/default/xui/en/director_kit_index.xml
indra/newview/skins/default/xui/en/floater_director_v2.xml
indra/newview/skins/default/xui/en/panel_dpage_camera_frame_orbit.xml
indra/newview/skins/default/xui/en/panel_dpage_camera_moves.xml
indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml
indra/newview/skins/default/xui/en/panel_dpage_camera_shot.xml
indra/newview/skins/default/xui/en/panel_dpage_camera_takes.xml
indra/newview/skins/default/xui/en/panel_dpage_camera_vcam.xml
indra/newview/skins/default/xui/en/panel_dpage_capture_scene_options.xml
indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml
indra/newview/skins/default/xui/en/panel_dpage_cast_dance.xml
indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml
indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml
indra/newview/skins/default/xui/en/panel_dpage_cast_path.xml
indra/newview/skins/default/xui/en/panel_dpage_cast_props.xml
indra/newview/skins/default/xui/en/panel_dpage_lens_diopter.xml
indra/newview/skins/default/xui/en/panel_dpage_lens_filters.xml
indra/newview/skins/default/xui/en/panel_dpage_lens_focus.xml
indra/newview/skins/default/xui/en/panel_dpage_light_actors.xml
indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml
indra/newview/skins/default/xui/en/panel_dpage_light_cues.xml
indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml
indra/newview/skins/default/xui/en/panel_dpage_light_practicals.xml
indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml
indra/newview/skins/default/xui/en/panel_dpage_look_actor_style.xml
indra/newview/skins/default/xui/en/panel_dpage_look_bloom_glow.xml
indra/newview/skins/default/xui/en/panel_dpage_look_image.xml
indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml
indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml
indra/newview/skins/default/xui/en/panel_dpage_look_tron.xml
indra/newview/skins/default/xui/en/panel_dpage_performance_animate.xml
indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml
indra/newview/skins/default/xui/en/panel_dpage_performance_gaze_cues.xml
indra/newview/skins/default/xui/en/panel_dpage_performance_polish.xml
indra/newview/skins/default/xui/en/panel_dpage_performance_switchboard.xml
indra/newview/skins/default/xui/en/panel_dpage_quick.xml
indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml
indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml
indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml
indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml
indra/newview/tests/llprismprojector_test.cpp
scripts/code_tools/director_functional_manifest.py
scripts/code_tools/director_kit/gen_report.txt
scripts/code_tools/director_kit/vcam_page.json
scripts/code_tools/director_kit_gen.py
scripts/perf/functional_report.py
```
