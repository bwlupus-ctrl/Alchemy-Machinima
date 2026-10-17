# Teleport RAM growth, capture-pin VRAM thrash, and arrival flashes — v2

Status: **PLAN GREEN (10/16 night)**: Opus (v1 NOT GREEN, v2 + latch fixes GREEN), Codex xhigh author + high/medium rechecks GREEN, Gemini Flash GREEN on v1 (low weight). F0 (RAM owner) is diagnose-then-fix; F1 small edits and F2 are ready to implement. Documentation only.
Source snapshot: `dba68386dae`. Read `CLAUDE.md` before later implementation.

## Scope and priorities

Configuration: RTX 5090 32 GB, 4096 x 2160 window, ReShade add-on loaded, Cine Light Rig and Actor Mask active. User-confirmed settings: `RenderVRAMGovernorEnabled=1`, `RenderMaxVRAMBudget=0`, `RenderTextureVRAMDivisor=1`, **`BDMergeCaptureModePin=1`**, `BDMergeCineModeAuto=0`; `TextureLoadFullRes` is at its default, false (`app_settings/settings.xml:24393-24402`). The manual control is “Pin scene textures (no degrade during takes)” (`skins/default/xui/en/floater_lightbox_settings.xml:4620-4622`). Auto capture being off does not disarm the manual pin. Preparing this spec changes no settings.

| Finding | Priority and conclusion |
| --- | --- |
| **F0: viewer system-RAM growth on dense-region arrival** | **BLOCKER / primary performance finding.** Single-digit FPS tracks large growth of this viewer process's working set/private commit, including long after the logged VRAM loop stops. Identify the retained allocations; a VRAM-only fix cannot close this issue. |
| **F1: capture pin defeats quality recovery; repeated eviction rescans** | **P1, code/log-supported fix.** Bias repeatedly snaps to 1 at Normal, then full-resolution loading refills memory. Share the existing quality hold with all three override paths, add a small suspend latch, scan on the first eviction of a pressure episode only, and raise the existing reserve ceiling. |
| **F2: arrival camera fly-through and repeated ReShade resets** | **P1, code/log-supported fix.** Snap the arrival camera instead of interpolating across regions. Suppress only teleport-related pose detection and emit the one teleport reset when the transition settles. Preserve ordinary cut detection. |
| **F3: complexity-toast flapping** | **P1 presentation fix.** Apply a plain approximately two-second settle debounce. The exact 1,946-point cost input remains unproven and does not block this small notification fix. |
| **F4: other render hypotheses** | Keep the established audit boundaries; do not add speculative render-state, shader, or target changes to these fixes. |

**Implementation order is F1 small edits, F2, F3 debounce, then the focused F0 memory evidence/fix.** F0 blocks calling the performance problem solved; it does not block shipping independently useful, proven F1/F2 corrections. No prerequisite broad evidence session or new status UI.

## Evidence conventions and verified baseline

`LOG:L` means line L of the unchanged, read-only file `.tmp/teleport_log/Alchemy_20261016.log` (`I:/alchemy-machinima/.tmp/teleport_log/Alchemy_20261016.log`).
SHA-256: `0d6af8ea51793990b7cd73f7696ee0f74b6fbda0b3d0f4994a837e2621fe7bad`; 4,370,426 bytes, 25,731 lines. Source locations below are repository-relative; new diagnostic markers are proposals.

The filename uses the local date. The log spans **2026-10-17 00:44:51–02:47:14 UTC**, or **2026-10-16 20:44:51–22:47:14 EDT**. All times below are UTC unless stated otherwise.

| Measurement | Verified result |
| --- | --- |
| Critical transitions | 148; **144 report discard_bias=1**, four report 2.5 |
| Driver-eviction immediate rescans | 343, often on consecutive one-second samples |
| FPS during 01:32:00–02:19:59, despite the Critical/recovery loop | 287 samples, **22.6–30.0 FPS**, median 28.9 |
| Last eviction scan / last VRAMGovernor line | `LOG:20539`, 02:25:40 / `LOG:20558`, Normal at 02:25:51; **zero later VRAMGovernor lines** |
| ReShade reset messages | 24 jumps, five teleport completions, no Director cut messages |
| 02:40 teleport | 11 jumps plus one completion reset; ten successive post-arrival jumps sum to **100,523.86 m** |
| Self-complexity notifications | 252 total; 102 at 78,296 and 101 at 76,350; 49 other outfit-loading values |
| Durations of the 101 low-value excursions | 69 return in the same logged second, 30 in one second; **two exceptions last three and five seconds** |
| Readback warnings / immediate-upload-downscale warnings | Seven error-1280 readback warnings; 104 printed downscale warnings, reaching the sampled **10,100th occurrence** |

These are marker counts, not counts of visible flashes, allocations, or evicted objects. FPS is a roughly ten-second aggregate (`llviewerdisplay.cpp:274-284`), not an instantaneous frame-time trace.

## F0 — BLOCKER: identify the viewer's retained system RAM

### Primary evidence and attribution boundary

On Windows, “Current allocated physical memory” is **this process's WorkingSetSize**, and “Current committed private memory” is its **PrivateUsage** (`indra/llcommon/llmemory.cpp:131-144`, `:244-249`). These are not whole-machine RAM usage and not tracked VRAM. The samples show tens of GB added to the viewer during arrival/loading:

| UTC | Working set, logged MB | Private commit, logged MB | Evidence |
| --- | ---: | ---: | --- |
| 02:20:53, before the 02:21 arrival | 24,550.68 | 41,205.27 | `LOG:19212-19213` |
| 02:23:53 | 31,532.51 | 60,130.54 | `LOG:20251-20252` |
| 02:24:52 | 44,391.16 | 73,035.23 | `LOG:20402-20403` |
| 02:26:53 | 49,909.06 | 79,418.85 | `LOG:20716-20717` |
| 02:27:53 | 55,510.06 | 81,132.50 | `LOG:21052-21053` |
| 02:39:53, before the 02:40 arrival | 26,329.56 | 53,930.38 | `LOG:23141-23142` |
| 02:40:55 | 50,897.74 | 78,815.50 | `LOG:23888-23889` |
| 02:41:55 | 58,051.99 | 75,614.45 | `LOG:24244-24245` |

The review's onset time needs a precise correction: `LOG:20215` at **02:23:41 is 12.20 FPS**; the first single-digit sample after the long healthy loop is **9.70 at 02:23:51** (`LOG:20250`), followed by 7.70/7.20 (`LOG:20284`, `:20323`). After the 02:40 arrival, FPS falls to 3.90 at 02:40:25 (`LOG:23715`) and 1.00 at 02:41:57 (`LOG:24256`). Private commit remains around 80,000 logged MB later (`LOG:24646`, `:24975`, `:25436`).

**The primary correlation is viewer system-memory growth, not time spent in the governor's Critical loop.** This does not prove whether allocation/copy work, retained raw buffers, driver backing/paging, or another owner causes each stall. Zero later governor lines proves no later logged transition/eviction response, not healthy physical GPU residency. Conversely, substantial available machine RAM/commit remains (`LOG:24246-24248`); this is not evidence of crossing the viewer's system-memory emergency threshold.

### What the code already identifies

| Candidate owner | Verified allocation/retention path and diagnostic implication |
| --- | --- |
| Decoded texture RAM pool | `lltexturefetch.cpp:1866-1869` writes successful decodes into `BDMergeTexPool`. `bdmergetexpool.cpp:223-274` allocates and copies an independent raw-pixel buffer; a hit allocates/copies another `LLImageRaw` (`:188-219`). Count pool storage separately from live raw images. Region grace/recency affect eviction preference; the three-pass budget backstop still exists (`:73-135`). |
| Live, saved, auxiliary and readback raws | `llviewertexture.cpp:3349-3438` can copy a valid raw into `mSavedRawImage` before dropping the working raw. `:3459-3473` records forced retention; `:3476-3501` allocates a GPU readback raw; release is at `:3504-3527`. Inspect callback/retention lifetime, not just raw counts. `LLImageRaw::sRawImageCount` is a count, not a total byte measure (`indra/llimage/llimage.h:320`). |
| Mesh pool and live geometry | `bdmergemeshpool.cpp:149-208` deep-copies volume faces on fetch/put and exposes estimated retained bytes at `:136-139`; calls are in `llmeshrepository.cpp:2040`, `:2435`. Pool bytes do not account for every live volume/face/object buffer. `LLVertexBuffer::getBytesAllocated()` reports VBO storage, not a CPU-heap breakdown. |
| GL driver system-memory backing / VRAM spill under the pin | Full-resolution GL allocation growth can increase driver-managed system backing. Viewer GL allocation estimates and NVX available VRAM do not measure that backing or establish its owner. Pair process memory with OS per-process dedicated/shared GPU-memory data where available; an unexplained residual is not automatically “driver spill.” |

There is already useful RAM evidence. `BDMergeMemoryBudget::refresh()` (`bdmergememorybudget.cpp:106-206`) shares a live physical/commit safety budget between the texture and mesh pools; it is separate from the VRAM governor. `LOG:139` initializes at Normal with a **43.9 GiB reserve and 68.5 GiB combined pool budget**. `LOG:19790` reports **13.8 GiB in decoded pools**, private commit 60.4 GiB, on entering Soft at 02:21:57. Later transitions show 13.2 GiB at `LOG:20184`, 10.7 GiB at `LOG:20249`, then only **0.3 GiB at `LOG:20322`** while the later process growth continues. These transition-only samples do not explain the later 50–58 GB working set. The final settings dump records `BDMergeTexPoolFraction≈0.6` (`LOG:25424`); do not assume the default 0.5 or infer a pool leak from its permitted budget alone. No RAM-governor Critical transition is logged, so its `isCritical()` capture gate (`llviewertexture.cpp:518`) need not have suspended the pin.

### Minimal diagnosis, then the owner's fix

After F1/F2, add **one bounded process-memory breakdown**, reusing `LLMemory`, pool `getBytes()`/`appendReport()`, and the existing texture pipeline report (`bdmergetexspike.cpp:116-151`). Sample before teleport, about once a second during the first two minutes of arrival, then every five seconds during a quiet dwell. No general profiling framework or prerequisite F3/Actor Mask/target/shader instrumentation.

The proposed `[TeleportMemory]` record needs process working set/private commit, available physical/commit, texture and mesh pool bytes/budgets, **live raw-image bytes as well as count**, saved/aux/readback retention subsets, live mesh/CPU-buffer estimates, and tracked GL/VBO bytes/driver available. Include pin requested/effective/suspended and foreground/background state. Reuse existing decode/upload timings for correlation. Keep estimates and unavailable values labelled.

Use thread-safe 64-bit raw byte accounting at the existing allocation/reallocation/release paths if needed, including buffer-transfer paths; counters must reflect actual lifetime. Obtain a small owner breakdown at baseline/peak/settled points, with callback/forced-save reasons and the largest retained buffers. Deduplicate aliased raw pointers; saved/readback categories are subsets, not extra bytes to add again. The pool's independent byte copies are additional storage. Do not add a per-frame full texture/mesh walk or retain diagnostic references that prolong the allocations being measured.

Compare the original route with the F1/F2 fixes, pin armed as confirmed, and a matched fresh-launch pin-off control. If the residual remains large, make **one measurement-directed control**: decoded pool off using its existing switch, or OS driver-memory/allocation capture if viewer-owned bytes fail to explain the growth. Match focus/background intervals. Avoid changing several features at once.

Apply only the measured correction:

- If obsolete saved/readback raws dominate, release or replace the stale references once their required callbacks/retention have ended; avoid duplicate full-size copies. Preserve sculpt, baked/UI and mandatory raw-callback correctness.
- If optional decoded texture/mesh pools dominate, correct their retention/accounting or reduce their growth through the existing shared RAM-budget path. Do not add a second cache governor or flush all textures on every teleport.
- If GL backing/spill dominates, use F1's pin suspension/headroom correction first; fix a remaining specific allocation lifetime only with ownership evidence.
- If live mesh/object buffers dominate, repair the demonstrated reference/lifetime or duplicate allocation. Do not label an unexplained process residual a leak.

**F0 completion gate:** account for the tens-of-GB delta without double counting, identify the owner responsible for sustained growth/stall work, and demonstrate bounded memory over repeated matched arrivals and a quiet dwell. Compare real frame durations/existing timing and the ten-second FPS samples. Fewer VRAM warnings is insufficient; unresolved process growth or recurring single-digit foreground FPS leaves F0 **INCONCLUSIVE/FAIL**.

## F1 — Minimal capture recovery and eviction fixes

### Proven feedback driver

`llviewertexture.cpp:665-677` gives Auto detected VRAM divided by the divisor, 32,607 MB here. The existing driver-headroom/eviction checks (`:709-799`) already make the response driver-aware even when tracked allocation is below that cap. Render-target texture bytes are already included in texture accounting (`:654-663`).

The strongest evidence is the bias reversal:

```text
LOG:10286 01:32:28 Normal:   tracked=9095 MB,  driver_available=13855 MB, bias=4
LOG:10324 01:32:44 Critical: tracked=21048 MB, driver_available=1898 MB,  bias=1
LOG:10325          Emergency driver-eviction rescan
```

**144/148 Critical entries start at bias 1**, while Normal entries log 2.5–4. The transition logger (`:852-873`) runs before the final capture override (`:1129-1131`). In this governor-enabled, manually pinned path, that override is the explicit snap to 1: gradual recovery at `:950-961` cannot explain a 4→1 reversal in 16 seconds. Pressure recovery returns to Normal, the pin bypasses the three-second quality hold, full-resolution demand refills, and a new eviction returns Critical. Background restoration is another possible bias change and is handled below; it does not explain this sustained foreground pattern.

`isCaptureQualityPinActive()` (`:522-524`) checks only capture and pressure. Both independent `want_fullres` sites (`:2310-2312`, `:3626-3628`) likewise bypass the hold. Fixing the helper alone misses discard-0 loading. `update_capture_mode()` (`:533-550`) returns immediately for the manual pin before reading auto-arm; the confirmed pin is sufficient without automatic cinematic mode.

### Small independent edits

1. **One shared quality permission and one suspend latch.** Route `isCaptureQualityPinActive()`, the bias override at `:1129`, and **both** `want_fullres` sites through the same cheap per-frame permission: when the governor is enabled, no active pressure, no existing quality-recovery hold, and no suspended override. Cover `TextureLoadFullRes` too. Set the suspend latch when a requested full-resolution/capture override encounters pressure; returning to Normal alone must not re-enable the override and refill again. Keep the user's pin setting armed. For the minimal initial policy, release the latch when the underlying quality requests are actually withdrawn and explicitly rearmed, or when the governor is disabled; normal screen-space quality can still recover at the existing gradual rate. Do not mistake a safety-suppressed helper returning false for withdrawal of the underlying request.

   **The suspension is visible and self-releasing (review r2).**
   - **Auto-release only when the refill would actually fit.**
     - When the latch trips, record `peak_tracked`, the tracked MB at the moment of suspension.
     - Resume only when `peak_tracked + driver_reserve ≤ current_tracked + driver_available`, using freshly sampled driver telemetry, and only after that has held continuously for 30 s with no eviction. It is the same budget the existing `enough_driver_headroom` check (`llviewertexture.cpp:947-948`) uses, not the cap percentage, which can't fail on a 32 GB auto cap.
     - **The refill must also fit the cap:** additionally require `peak_tracked ≤ RECOVERY_CAP_PERCENTAGE × effective_cap`. A suspension that cap pressure triggered (as opposed to driver eviction) stays latched until the effective cap changes (the user raises the budget) or the user re-toggles the pin. Downrezzing alone can never satisfy the resume test, because `peak_tracked` is the full-resolution demand (`llviewertexture.cpp:787-799`).
     - **Each re-suspend within 10 minutes doubles the hold** (30 → 60 → 120 s, capped at 10 min). A loop therefore decays instead of cycling.
     - If driver telemetry is unavailable, stay suspended until the user re-toggles the pin.
   - **Status:** while suspended, the existing pin control (`floater_lightbox_settings.xml:4620-4622` and its Director mirror) shows "suspended: VRAM". This uses the existing label or tooltip, not new UI.
   - **Logging:** each latch transition writes one `VRAMGovernor` line (`capture pin suspended` / `capture pin resumed`), so takes are never silently downrezzed.

   **Governor Off is an explicit compatibility requirement.** The three override sites execute outside `if (vram_governor_enabled)`. New hold/latch/background recovery restrictions must be conditional on `RenderVRAMGovernorEnabled`; Off retains the current legacy pin/full-res behavior and existing system/process-memory safety. Reset the new governor-only latch on disable so it cannot leak into Off. Cache permission once per frame, not settings lookups per texture.

2. **Rescan only on the first eviction of a pressure episode.** At `:898-914`, add an eviction-start edge beside `system_low_started`. The first eviction gets the existing immediate safety rescan; later eviction samples while that pressure episode is active do not rescan the full texture list again. Remember that a first driver eviction can occur while cap pressure is already Critical. Clear the eviction latch after the existing hysteretic return to Normal, or governor disable. Preserve immediate escalation/emergency bias and the independent true system-low edge; only repeated scans change. Reuse the existing paced scheduler for ordinary continuing work. No new sweep queue, progress/no-progress machine, or scheduler redesign.

3. **Raise only `MAX_DRIVER_RESERVE_MB` (`:644`) from 1,024 to an initial 4,096 MB.** Retain `MIN_DRIVER_RESERVE_MB=256` and the existing 10% calculation (`:782-790`). This gives this 32,607 MB card about 3,261 MB reserve and earlier use of the existing driver-pressure checks. Do not impose a 1,024 MB minimum on small GPUs. Keep current cap semantics, recovery thresholds and once-per-second telemetry.

These edits are independently reviewable/shippable; the shared permission change itself must include all three override paths. No new target formula, paired-sample budget estimator, sample-age policy, eviction-counter rebasing, GL-context detector, budget-raise rules, fallback budget mode, or status UI. In particular, no `B_target` formula and no `min(C, 0.8D)` fallback. Preserve existing telemetry-query failure behavior.

### Background/refocus belongs in this fix's checks

`LOG:22893` at 02:38:24 says backgrounded **for 60 seconds**, implying focus loss around **02:37:24**, not at the message time; refocus is `LOG:22939` at 02:38:45. The second interval similarly starts around **02:42:27**, logs the 60-second release at 02:43:27 (`LOG:24598`), and refocuses at 02:43:36 (`LOG:24614`). Starts are inferred from the timer, not explicit focus-event timestamps.

`llviewertexture.cpp:1070-1104` saves bias, requests bias 5 after the background delay (finally clamped to 4), then restores the saved bias on refocus. The later pin snap can override the requested background bias when Normal, so the log message alone does not prove actual downrez. Ensure the shared governor-enabled quality permission yields during that existing background release, and refocus neither clears a pressure-suspend latch nor bypasses the existing quality hold. In the enabled branch, use that hold/gradual recovery rather than unconditionally restoring a low saved bias after downrez. Preserve the legacy save/restore path when Off. This uses the existing background state and recovery mechanism, not another timer/state machine.

### Validation for later implementation

- Repeat the pinned scene/route: no Normal→pin snap→Critical refill loop under unchanged conditions; no repeated automatic discard-0 retries while suspended. Explicit withdrawal/rearm works after the existing hold and can suspend again if it still cannot fit.
- Require at most **one driver-triggered full rescan per pressure episode**, including consecutive eviction samples and a first eviction during cap-only Critical. True system-low still gets its own edge response.
- Verify all three quality sites with governor On/Off, pin on/off, `TextureLoadFullRes` on/off, and live governor toggles. Off must be behaviorally unchanged.
- Exercise both logged-length background/refocus sequences, once under pressure and once without pressure; verify actual bias/full-res requests, not merely the background message. Watch process RAM/refill after refocus as part of F0.
- Check small-GPU reserve calculations, custom caps, missing NVX/ATI telemetry, and mandatory UI/baked/sculpt callbacks. Use existing governor state logs plus bounded latch transitions; no new panel is required.
- F1 can pass on corrected gates/scans/headroom behavior even if F0 still owns a separate FPS failure. Do not claim the RAM problem solved from reduced eviction counts.

## F2 — Snap the arrival camera; reset ReShade after settling

### The camera motion itself is a flash candidate

At 02:40, `LOG:23201-23204` reaches ARRIVING, emits a 137.883 m jump, then changes to NONE. **Ten further main-camera samples travel a cumulative 100.52 km** (`LOG:23206-23223`), with individual steps up to 15.74 km and serial 0→0. At 02:21, eight jumps at `LOG:19327-19350` total **61.52 km** (the whole ten-jump burst is 62.17 km). These are sampled final main-view poses, not merely an overactive reset detector. The fly-through frames can visibly flash the scene and churn visible texture demand on their own; suppressing reset flags alone leaves that defect.

The code explains how an arrival snap is undone:

1. Cross-region movement completion tries to snap with `setFocusOnAvatar(true, false)`, `slamLookAt()` and `updateCamera()` (`llviewermessage.cpp:3339-3345`).
2. The subsequent START_ARRIVAL display step calls `resetView(true, true)` (`llviewerdisplay.cpp:389-398`). `resetView()` changes to the default camera and uses `setFocusOnAvatar(true, ANIMATE)` (`llagentcamera.cpp:341-343`, `:381`); default third-person changes also start animation (`:2692-2705`, `:2831-2833`).
3. `startCameraAnimation()` (`:2987-2993`) captures the then-current global camera/focus as its start. `updateCamera()` (`:1498-1501`) lerps those starts toward the destination. Short-distance positional smoothing at `:1548-1560` does not explain kilometer-scale motion by itself.

`detectCameraCut()` (`llreshadebridge.cpp:146-244`) then flags each large pose step. Its `teleport_pending` check (`:187-188`) is only the pending publication bit, cleared on swap at `:618-622`. The existing completion call (`llagent.cpp:4303-4308`) is late: at 02:40 its reset is `LOG:23314`, after the jumps. At 02:27 a jump even follows completion (`LOG:20952`, `:20972`). Neither first-arrival reset nor completion alone defines a settled camera.

### Minimal change

**Snap at the existing teleport-arrival reset sites.** After the arrival-only `resetView(true,true)`, cancel any animation it armed using the existing `stopCameraAnimation()` (`llagentcamera.cpp:2999-3002`) and required camera/UI finalization; calculate/publish the destination pose only after destination position/focus is valid. Cover the local reset at `llviewermessage.cpp:6674-6683` as well as START_ARRIVAL. Keep the existing Keep Look At/flycam exception. Do not globally disable normal camera animations or alter coordinate rebasing.

**Keep the reset change inside the existing bridge path.** Change `noteTeleport()` from immediately emitting a reset to remembering one pending completion request. In `detectCameraCut()`, suppress the **generic pose backstop** while `gAgent.getTeleportState() != LLAgent::TELEPORT_NONE`, and while `gAgentCamera.getCameraAnimating()` is still true immediately after that transition. Use a short monotonic deadline, initially **one second after state returns to NONE**, only to bound this post-arrival animation suppression. A settled camera releases it immediately; there is no compulsory quiet-period wait. Update the pose/serial/owner baseline on every suppressed main sample.

Emit the pending **single `SLRESHADE_RESET_TELEPORT` on the first valid, presentable main frame once state is NONE and the arrival animation has ended**, not on the first arrival frame. A completion callback arriving after settling emits on the next eligible frame; an early callback stays pending. On the short deadline, reseed/emit the pending reset once and end suppression, with a bounded timeout diagnostic; continuing kilometer-scale motion makes the arrival test fail rather than justifying a longer mute. Canceled/failed requests without completion produce no manufactured teleport reset. Preserve pending flags over snapshots/non-swaps and count an event only when newly emitted.

Use only the existing teleport state, camera-animation predicate and completion call, with the minimal pending/deadline state in `LLReShadeBridge`. **Drop v1's extra start/arrival hooks, episode IDs, 3–10 second settling window, two-sample quiet test, and generic-discontinuity latch.** No ABI/add-on change is planned.

### Preserve real cuts, including non-Director sources

A repository-wide call-site check finds **only `llcinematiccamera.cpp:2722` calls `noteCameraCut()`**. Path, Recorder, presets and flycam rely on the pose backstop (`llreshadebridge.cpp:213-220`). Protecting only explicit Director cuts is insufficient.

Keep `mPendingDirectorCut` and its presentation-owner/movement/same-slot/ease checks intact; explicit real Director cuts still get evaluated during teleport suppression. Outside the actual teleport and its short animation tail, retain the current generic position/rotation tests on **every** main frame. No generic latch or global cooldown may absorb successive real cuts from Path/Recorder/presets/flycam. Coincident reasons in one published frame can use the existing reset flag combination; do not introduce another cut-episode mechanism. Resource lifetime, resize, HDR and source-validity resets remain independent.

### Validation for later implementation

- Far/local/forced teleports, back-to-back teleports, cancellations, Keep Look At/flycam and low-FPS arrivals: **no presented inter-region fly-through**, one settled teleport reset per success, zero jump resets from arrival interpolation. Verify the destination image and reset timing together.
- At normal and 1–5 FPS, verify no reset is consumed by a snapshot/auxiliary view or non-swapped frame, and no suppression persists past the short deadline. A timeout is diagnostic evidence, not proof of a correct snap.
- Exercise distinct hard cuts on consecutive frames for **Director, Path, Recorder, presets and flycam**, after teleport and at the suppression boundary. Each real post-transition discontinuity must retain its reset; ordinary eased motion must retain current behavior.
- Preserve existing `BDMergeReShadeCutDetect` rollback, distance/angle settings, and inactive-consumer behavior. One correct arrival reset must not be suppressed to make the flash disappear.
- Viewer reset flags prove requests, not every effect's visible response. If whole-window/UI flashing persists after camera/reset correction, investigate the active ReShade preset with the matching add-on log; F2 does not establish a consumer-side cause by itself.

## F3 — Plain two-second complexity-toast settle debounce

`llavatarrendernotifier.cpp:233-275` notifies on every changed integer once notification is armed; the 10% threshold elsewhere concerns regional over-limit percentages, not this self-cost pair. The outfit churn at `LOG:18680-18702` supplies an arming event. The oscillation begins before the later teleports and continues after the last governor line, so do not attribute it solely to VRAM.

The repeating values differ by **1,946 points (2.55%)**. `LOG:19201/19203` dips/returns in the same second; `LOG:19256/19257` takes one second. A plain approximately **two-second stable-candidate debounce** removes these fast excursions without changing numeric complexity or imposing a minimum change:

1. Keep the computed/latest cost and attachment data current. When the candidate value changes, replace it and restart a two-second settle timer.
2. After it stays unchanged for two seconds, notify once only if it differs from the last delivered value. A quick return to that delivered value cancels the pending change naturally.
3. Evaluate expiry in the existing model update path (`updateNotificationState()` / self update at `llvoavatar.cpp:9933-9936`), with a cheap timer check at a sufficient cadence. A one-off stable small change must notify even without another complexity recomputation. Process the latest available candidate before emitting an expired one. Preserve notification settings, mouselook suppression, outfit eligibility and regional warning behavior.

**No magnitude threshold, ten-second rate limit, outfit re-arm redesign, or cost-formula change.** Notification presentation can ship without solving the exact cost input.

Verification corrects one review overstatement: 203 pair-value messages represent an initial high plus 101 low/high excursions; **99 excursions return within one logged second, but `LOG:24746/24748` lasts three seconds and `LOG:24991/25002` lasts five seconds**, during the late low-FPS period. A timestamp replay with a two-second debounce leaves those two slow excursions eligible. Therefore “all ~203 flaps removed” is not proven by this log. Keep the simple fix; document its residual behavior rather than add threshold/rate-limit/re-arm machinery to force zero.

The exact cost root remains open. The prior audit's conditional per-target Force Mask cycle is a separate candidate: `pipeline.cpp:22642-22644` seeds plain-face Blend from prior pool membership; `:22688-22711` can return before the later texture alpha-capability fallback. Pool assignment (`llvovolume.cpp:6545-6572`) can alternate under those conditions, affecting the alpha cost multiplier (`:4393-4457`). The log does not establish that per-target override or identify an attachment contributing exactly 1,946 points. Global `BDMergeForceAlphaMask=1` (`LOG:25424`) is a different input. Do not change shared alpha classification, clone ownership, Actor FX/Mask or standard complexity accounting on this evidence alone. An optional later self-only component diff can investigate it; it is not part of the required F0 evidence patch.

Validate fast 76,350→78,296 returns with **zero repeat toasts after the initial settled value**; include a single small stable edit, burst outfit loading, identical repeated values, notifications disabled, mouselook and regional visibility changes. A stable small edit gets one notice after about two seconds plus model-update delay. At low FPS test timer ordering and acknowledge that a value stable longer than two seconds can legitimately notify. Count self-notification add/replace operations, not every unrelated “Closing floater toast.”

## F4 — Retained audit boundaries, not another evidence project

- **Readback error 1280:** seven warnings (`LOG:2341`, `:2375`, `:19613`, `:19933`, `:19939`, `:23793`, `:24492`). `indra/llrender/llimagegl.cpp:2083-2152` drains the error before the actual transfer at `:2177`; this is not a demonstrated readback-origin error or OOM. The final warning is beside 21.30 FPS. F0 must count retained readback raws regardless of warning count. Preserve required raw callbacks; investigate a specific GL call only if the memory/timing evidence implicates it.
- **Targets/shaders:** all 16 logged `setShaders` lines are startup, ending at `LOG:2244`. Arrival resets camera/texture stats, not evidence of a main-target/shader-reload storm. Existing bridge resource-generation/HDR/source-loss resets remain necessary. No unconditional target or shader reset.
- **Actor Mask/rig:** interval counters such as `views=256` (`LOG:24419`) are accumulated auxiliary/main view work, not 256 views each frame. Twelve actor-only lights exceed the existing six forward-alpha slots (`pipeline.cpp:11713-11728`); that is a lighting-capacity limitation, not an allocation overrun. CineRig dumps are every 120 calls, not target recreations. Keep mask validity/fail-closed guards and avoid a light architecture or shared GL-state rewrite.

The former F4 “unexplained 1 FPS” item is promoted into **F0's process-memory timeline**, not left as a minor late-stall footnote. Further mask/readback/target instrumentation is optional only if the focused memory breakdown points there.

## Implementation order and completion gates

1. **F1 small edits first:** shared hold/suspend permission at all three override paths, strict governor-Off compatibility, first-eviction scan edge, reserve-ceiling increase, and the small enabled-only background/refocus recovery adjustment. These can be reviewed/shipped independently; no paired-telemetry policy or UI dependency.
2. **F2 camera snap and settled reset:** correct arrival-only animation, then the narrow bridge suppression/deferred completion reset. Preserve every non-teleport cut source's pose backstop. No extra lifecycle hooks or generic latch.
3. **F3 notification debounce:** the plain two-second settle; no prerequisite attachment investigation or classifier fix.
4. **Focused F0 evidence patch and one matched arrival session:** process-memory breakdown, including raw/readback, decoded texture/mesh pools, live CPU geometry and possible driver backing. Reuse existing counters/reports/timings; drop v1's broad evidence wishlist. Then fix the demonstrated owner and verify memory/FPS on the same route.
5. **Only measured follow-ups:** exact complexity-input or GL/render changes remain separate unless their evidence is obtained. Do not bundle unrelated renderer/UI work into these fixes.

For later code work, follow `CLAUDE.md`'s independent adversarial review loop before any build. This planning revision is not an implementation approval or in-world PASS. Keep separate F0/F1/F2/F3 verdicts: **PASS, FAIL, INCONCLUSIVE**. Missing/unexercised telemetry cannot be PASS, and successful F1/F2 does not close unresolved F0. This task edits only this spec; no code, build, commit or push.

Read-only baseline searches:

```powershell
$logPath = '.tmp/teleport_log/Alchemy_20261016.log'
rg -n 'Current allocated physical memory|Current committed private memory|BDMergeMemory|FPS:' $logPath
rg -n 'VRAM pressure is now|Emergency .*pressure|Texture will be downscaled|backgrounded' $logPath
rg -n '\[ReShadeReset\]|TELEPORT_(START|MOVING|ARRIVING)|teleport completed' $logPath
rg -n 'displayNotification : AgentComplexity ' $logPath
# After the focused memory instrumentation exists, use its new reproduction log:
rg -n '\[TeleportMemory\]|BDMergeMemory|decoded RAM pool|decoded mesh' $logPath
```

For later Python analysis use exactly `C:\Users\xianw\AppData\Local\Python\pythoncore-3.14-64\python.exe`. Retain the supplied evidence file unchanged.

## Remaining questions

1. Does the observed flash cover the entire window/UI or mainly the complexity-toast area, and which ReShade add-on build/preset/effects were active? A matching ReShade log for 22:40–22:42 EDT would distinguish remaining consumer behavior.
2. Which same destinations/outfit/rig arrangement should be used for the matched F0 run, and was recording or another GPU-heavy application active? The old `nvidia-smi` used/free observation lacks a matching timestamp.
3. If the exact complexity input needs later investigation, which attachments/local alpha overrides or scripted material changes were active? The 1,946-point component is unlogged.

Capture pin state, auto-capture state, full-res default and the two logged background intervals are already established above; they are no longer open questions.
