# VCam media live-test fix round 3

2026-10-18. Built on the uncommitted rounds 1+2 tree, base `88c254041fc`.
Read its diff, `CLAUDE.md`, both live reports, the loop diagnostic and Gate v2.
The concurrently updated media spec v8.2 / Gate v3 were retained.
No commit, push, viewer/plugin build, viewer launch or hardware-capture test.
Only the three authorized integration targets and their library dependencies were built.

**Offline checks pass. Live seamless-loop acceptance is still UNVERIFIED.**
The previous claim that stop/play kept the input was false. Native repeat now
keeps the input for recorded-length files. Source research also found a real
VLC duration-less Matroska exception; that recovery is explicitly allowed to
hitch and is labelled in the UI. No unit test certifies a VLC or audio seam.

## 1. Native repeat, source proof, and its actual limit

`indra/media_plugins/libvlc/media_plugin_libvlc.cpp:680` configures private,
validated local/cache media with `:file-caching=50` and the production
`ALMediaFile::inputRepeatOption()` (`indra/newview/alvcammediafilepolicy.h:344`):
`:input-repeat=65535` ON, `:input-repeat=0` OFF, before player/input creation.
The recorded-length EOF path no longer calls stop/play, `playMedia()`, or
rebuilds admission. Player/load tokens, accepted output and volumetric ownership
survive normal native wraps. Private media alone gets the smaller file cache;
ordinary/borrowed playback keeps its prior options.

VLC references below are **3.0.23 tag, raw source line numbers** (the browser's
plain-text view can collapse blank lines):

- [`lib/media_player.c:162`](https://github.com/videolan/vlc/blob/3.0.23/lib/media_player.c#L162)
  stops/closes the input; `:1063` calls it from stop. `:986` creates an input
  in play. Retaining the media-player object never proved input continuity.
- [`src/input/input.c:560`](https://github.com/videolan/vlc/blob/3.0.23/src/input/input.c#L560)
  handles demux EOF. `:610` decrements input-repeat and queues a start-position
  seek on the existing input. The main loop invokes it at `:755`, after draining
  at `:733`, instead of leaving the running input. Neither length nor
  `DEMUX_GET_LENGTH` conditions this repeat controller. Seek handling at `:2018`
  calls the existing demux and clears EOF on success. Teardown is elsewhere,
  in `End()` at `:1499`.
- [`src/input/es_out.c:622`](https://github.com/videolan/vlc/blob/3.0.23/src/input/es_out.c#L622)
  flushes existing decoder instances and resets their clocks during seek;
  it does not delete them. [`src/input/decoder.c:1476`](https://github.com/videolan/vlc/blob/3.0.23/src/input/decoder.c#L1476)
  invokes decoder flush and flushes the existing vout/aout. `:2066` checks
  decoder FIFO/drain and vout emptiness. This establishes retained input,
  demux and normal decoder/vout lifetimes, **not zero measured presentation or
  audio latency**. Same-segment Matroska seek uses the current segment at
  [`virtual_segment.cpp:554`](https://github.com/videolan/vlc/blob/3.0.23/modules/demux/mkv/virtual_segment.cpp#L554).

The repeat controller accepting unknown duration is insufficient proof for
the demux. [`mkv.cpp:348`](https://github.com/videolan/vlc/blob/3.0.23/modules/demux/mkv/mkv.cpp#L348)
rejects SET_POSITION without positive duration; `Seek()` at `:470` rejects
negative duration even for time seeks. Thus **duration-less WebM/MKV cannot be
claimed to repeat seamlessly through this VLC API**. The plugin observes the
native repeat marker even when failed seeks swallow EndReached. Only admitted,
Loop-ON, Playing, unknown-length inputs may recover by `playMedia(true)`
(`media_plugin_libvlc.cpp:271`; policy helper `:345`). They get fresh, bounded,
muted admission with only old accepted output/ownership retained. Their visible
status is **This file has no recorded length - loops may hitch (remux to fix)**
(`indra/newview/llprismlens.cpp:2876`, `:3274`). Known-length EOF never takes this
reopen exception. Native repeat remains the preferred recorded-length path;
source proves no mandatory input teardown there, so no speculative near-EOF
seek that discards encoded tail frames was substituted.

**Admission:** Loop-after-admission is enforced by the admission latch, not
by delaying the media option. `media_plugin_libvlc.cpp:252` rejects a pending
EOF/native-repeat marker before it can be hidden by a later video verdict;
otherwise the existing fixed 10 s deadline still rejects never-admitted inputs.
Publication remains verdict-gated in idle, and both audio gain and accepted-frame
measurement remain zero/inactive until admission. Real rebuilds re-admit.

**Loop edits:** `media_plugin_libvlc.cpp:178` detects a changed live repeat mode,
samples the current time, recreates media once and seeks the new input to that
position. `:242` withholds admission while that resume seek is pending. This is
an explicit user-edit interruption; it never runs at a normal unattended wrap.
Both private transport and set_loop use it. Loop OFF starts with repeat zero,
so Gate Play-to-end continues to receive terminal EOF, including unknown length.
This does not implement the separate Gate cycle feature.

**Measurement:** `media_plugin_libvlc.cpp:168` registers a private-only VLC log
observer for the exact native repeat marker. VLC emits that marker after drain,
before seek. `:359` timestamps accepted display callbacks with steady_clock;
the first after the marker subtracts the last before it. The message thread
emits exactly `loop seam gap <ms>` at INFO/VCamMedia (`:229`), once per presented
wrap, never per frame. Duration-less recovery carries the last accepted timestamp
into its replacement and includes the admission gap. No PTS polling, LibVLC call,
message send or log formatting enters the vout callback. The only queued values
are seam gaps; a short mutex protects their handoff, and logging occurs outside
it. Logger teardown is a callback barrier before context destruction (`:424`),
as established by [`lib/log.c:80`](https://github.com/videolan/vlc/blob/3.0.23/lib/log.c#L80).
This measures plugin presentation, not viewer/GPU/recorder delivery or audio.
Those require the next permitted live run.

Tests 37/38 no longer substitute a lambda and assert a VLC lifetime result.
`alvcammediafilepolicy_test.cpp:956` tests options, admission/deadline and the
unknown-only recovery decision. `llprismprojector_test.cpp:864` exercises actual
plugin protocol admission/epoch/dirty-pixel handling for an explicit edit.
Neither claims to execute LibVLC.

## 2. POSIX descriptor rewind

`media_plugin_libvlc.cpp:655` rechecks unchangedFile on the exact validated
open descriptor, then requires `lseek(fd, 0, SEEK_SET) == 0` before media_new_fd.
Failure refuses the player. Explicit replay of an ended private input recreates
through this path. Old identity validation still runs before closing/reopening;
new opens validate against the pinned identity. No file-policy predicate changed.

VLC [`modules/access/file.c:158`](https://github.com/videolan/vlc/blob/3.0.23/modules/access/file.c#L158)
duplicates an fd, sharing its offset; regular files install FileSeek at `:231`,
which performs lseek at `:309`. Native demux seeks therefore rewind through the
fd access without reopening the pathname. The Matroska duration restriction
above is a demux restriction, not failure of fd seek. Windows checks cannot
certify Linux/macOS execution; no POSIX runtime PASS is claimed.

## 3–4. Flare controls and NaN recovery

`indra/newview/pipeline.cpp:14039` normalizes finite dt once, then computes the
existing 100 ms attack and authored RenderCineLensFlareRelease response (0–2 s).
The same pair feeds CPU visibility (`:14170`) and uEnergyResponse (`:13650`,
call at `:14225`). Smoothing ON no longer substitutes 60/250 ms. Release zero
returns alpha 1 immediately. `indra/newview/alflareenergy.h:10` owns the shared
production finite-delta/response/state helpers; nonfinite CPU history resets
to current finite target. The GPU history shader guards both invalid current
energy and NaN/Inf history (`cineFlareEnergyF.glsl:53`, `:57`), falling back to
the current value for poisoned history.

`llprismprojector_test.cpp:908` tests authored release changes, zero release,
100 ms attack, frame-rate independence, NaN dt/history/target recovery, a finite
update after NaN, and the existing soft-knee behavior. These are CPU helper
checks; shader runtime and beauty/off-path GL presentation remain unexecuted.

## 5–6, 8–9. Director status, real counters and bounded work

- `alpaneldirectorvcam.cpp:511` gates numeric readouts to at most 2 Hz. The real
  FPS, p95, attempts/s and Adaptive state return to Performance; Admitted shows
  X of Y attempts/s; Req/Ent/Obs/Age shows target request, actual entitlement,
  observed publication and output age. Decimal precision quantizes each string;
  setValueIfChanged only writes a changed result (`:518`, `:522`, `:525`). Only
  monitor tiles retain Preview / Error: reason / Disabled captions.
- `llprismlens.cpp:3127` replaces the exact disabled reason with current empty
  or cued status when both switches permit media, before ownerless tickPrivate
  can continue. `alpaneldirectorvcam.cpp:531` clears mMediaPlayStatus whenever
  either switch changes, including transitions where their combined AND stays
  false. The helpers at `alvcammediafilepolicy.h:349` and `:355` are exercised by
  policy test 38 (`:983`), preserving unrelated refusals.
- `llprismlens.cpp:3041` coalesces latest route states into **one line per slot
  per 2 s**, including all current binding reasons. Logging-only demand uses
  temporal hysteresis: 250 ms continuously visible to enter, 750 ms continuously
  invisible to leave (`:3054`). Threshold chatter cannot generate per-frame logs;
  output demand/render admission itself is unchanged.
- `alvcampagemodel.cpp:105` uses one repeating idle callback, returning without
  snapshots/formatting until the 250 ms runtime deadline (`:126`). Configuration
  and selection remain responsive. Frame production may still bump its runtime
  revision, but runtime/performance/preview-only Director refreshes are capped
  at 4 Hz. No callback is repeatedly appended during one idle pass.

## 7. Actual runtime rectangles replace synthetic geometry claims

`xui_layout_check.py:1578` now calls the textual checks **KitSourcePins**;
`kit_source_pin_findings()` at `:1610` retains action ownership, atomic resize,
measured-height stacking and alignment source pins. The invented stacking,
wrapping and hard-coded value_top geometry was removed. Existing generic XUI
checks, thresholds, baseline pins and assertions were not relaxed.

`aluiperfselftest.cpp:11361` walks real Source-card widgets after layout. It
converts their actual local rectangles to card coordinates and records rows,
controls, nested buttons, labels and first-line font baselines. It never
recomputes wrapping, padding or row heights. The viewer writes
`logs/ui_vcam_source_rects.csv` (`:13477`). Two bounded audit states use the real
Director Add > media menu and Source selector, with an empty cue/no player, to
exercise URL and prim-face modes. The temporary capture is removed on teardown
and the media-slots setting is restored.

`scripts/code_tools/vcam_runtime_rect_check.py:11` consumes the dump, checking
sibling-row overlap, widgets escaping/crossing owner rows and baseline mismatch
**over 3 px**. Missing dumps/owner rows/required media modes fail. Both source
modes are required for each dumped size. `scripts/perf/ui_selftest.ps1:952`
archives the fresh dump and runs the checker in the Director post-build self-test;
its result participates in the existing budget exit gate (`:1236`).

`--selftest` proves a good rect fixture passes, action and value rectangles moved
100 px fail, sibling-row overlap fails, a 4 px baseline mismatch fails, and 3 px
is allowed. The CLI also rejected `.tmp/media_live3/rect_mutated.csv` (3 findings,
exit 1), and passed the good CSV (zero, exit 0). See `rect_selftest.log`,
`rect_mutated.json`, `rect_good.json`. These are fixture checks, **not a claim
that the new viewer audit ran**; it requires the prohibited viewer rebuild.

## 10. Producer provenance: what is and is not established

The prior report's suggestion that failure could expose a former camera merely
because a slot was reused was not proven, and that path is contradicted by the
identity reset:

1. `llprismpreview.cpp:719` compares capture UUID **and generation**. On either
   change, `:747` resets selection, publication and the whole TilePublication,
   including the atlas handle. Last-owner removal also clears it (`:800`).
   `tileState()` only returns a tile for the matching demanded id/generation
   (`:882`). A different camera cannot inherit that cell through ordinary slot
   reuse.
2. `llprismlens.cpp:2522` constructs a new MEDIA_URL capture with a fresh UUID
   and generation. `CaptureDefinition::switchMediaKind()`
   (`llprismlens.h:801`) only switches MEDIA_URL/MEDIA_FACE payloads. Camera
   rendering excludes media; accepted media serial is set only after successful
   media copy (`llprismlens.cpp:3293`, `:3369`). The signal builder additionally
   requires that serial for media (`:11637`).
3. Successful atlas drawing samples the resolved capture's output and publishes
   its cell only after the composite (`llprismpreview.cpp:1252`, `:1320`, `:1366`).
   A failed build previously left the **same-capture** atlas handle. It could
   expose that capture's earlier picture again; it does not establish a camera
   producer for a media capture. The handle clear (`:1262`), accepted-serial
   defense and monitor no-signal guard remain.

Thus stale same-capture pixels are a proven retention possibility; the user's
camera-looking media pixels remain an **unproven root cause**. A prim face can
itself contain camera-looking video. No report calls that a confirmed diagnosis.

## Verification

All artifacts are under `.tmp/media_live3/`:

| Check | Result |
| --- | --- |
| Authorized Release target build | PASS, `test_build_final.log`; only the three named integration targets/dependencies |
| Executables, run from sharedlibs/Release | **38/38**, **39/39**, **23/23 PASS**; `tests_alvcammediafilepolicy.log`, `tests_llprismprojector.log`, `tests_alposepolish.log` |
| Explicit zs_check -Files | **18/18 TUs PASS**, `zs_final.log`; plus **1/1 LibVLC TU PASS**, `zs_plugin.log` |
| prebuild_guard -ChecksOnly | All gates PASS, `prebuild_final.log`; exits before purge |
| xui_layout_check --changed HEAD --json | **40 files, zero findings**, `layout.json` |
| XUI source-pin/layout selftests | PASS through the full guard |
| Runtime rectangle checker | good PASS; mutated CLI FAIL as required; selftests PASS |
| git diff --check | PASS |

The explicit syntax list includes every changed .cpp plus alkitcard.cpp,
alvcammediafilepolicy.cpp and llviewerdisplay.cpp. Header consumers include
alkitcard.cpp for alkitrow.h; aluiperfselftest.cpp for panel/model headers;
alvcammediafileio.cpp and policy tests for container/policy/lifecycle headers;
alpaneldirectorvcam.cpp for monitor/io/model headers; llviewermedia.cpp and
projector tests for plugin/prism/media headers; pipeline.cpp and
llviewerdisplay.cpp for renderer/shader headers; projector tests for
alflareenergy.h. All changed headers therefore have another checked consumer.

The separate plugin syntax invocation uses an exact SHA256-matching copy of
its own real `media_plugin_libvlc.vcxproj` for the external script's project-map
limitation (same procedure as rounds 1+2), with its actual Release flags. No
plugin link/build occurred. TEMP/TMP and syntax caches are workspace-local;
process-local Git/Python wrappers only resolve environment access restrictions.
No gate was skipped, weakened or given a growing baseline. Page-source metadata
was regenerated after its guard reported drift. Initial syntax/audit-lint
findings were corrected; only bounded self-test stimuli have inline lint reasons.

The real-file probe remains explicitly SKIPPED without VCAM_MEDIA_PROBE_FILE.
Actual user-file decoding, source-specific audio continuity, recorder frame
gaps, GPU/shader behavior, native UI rectangles and the new in-viewer seam log
remain for the next maintainer viewer/plugin build and live hardware run.

## Fix round 4

2026-10-18. Addressed the six round-4 findings on the uncommitted rounds 1–3
tree. No commit, push, viewer build, plugin build, launch or hardware capture.
Only the three authorized integration targets and their dependencies were built.
The LibVLC translation unit was syntax-checked with its own project flags.

**1, 6 — Marker-time admission and independent clock fallback.**
`alvcammediafilepolicy.h` now owns the pure `WrapPolicy` transitions. The VLC
marker callback performs one atomic `fetch_or(Marker)` on the same state word
used by admission's CAS. State 0 is pending, 2 is a pending-wrap refusal, 1 is
admitted, and 3 is a normal admitted wrap. Admission cannot change 2 to 3.
The marker therefore wins even after the tick exchanged its event flags or
computed a local video verdict, provided atomic admission has not happened.
If admission wins first, a later marker is a permitted native wrap. Failed
CAS attempts recompute from the new state; they never overwrite the marker.
The final atomic admission check precedes audio/publication permission and
follows the existing POSIX identity check. Rejection clears seekability and
the explicit-edit output hold as well as stopping/muting the player.

The same helper tracks pending-input time samples. After an actual forward
step, a drop greater than 1 s **or** greater than half a positive duration
latches the same refusal. Initial backward/unavailable times and small jitter
do not establish a wrap; unavailable time resets the stable-step requirement.
This fallback does not depend on VLC's repeat log format and does not revoke
already-admitted native loops. Policy tests 39/40 cover marker-before-admit in
one tick, marker after local observation, marker after atomic admission, an
actual failed/stale atomic CAS and retry, startup non-monotonic times, unknown
duration, strict thresholds, short clips, jitter and an unavailable clock.

**2 — Explicit Loop edits on duration-less/non-seekable files.**
`setLoop()` samples a resume position only when both the previous admission's
seekability and VLC's current seekability permit it. Otherwise the replacement
starts without a resume seek. Pure `ResumeWait` releases the video predicate
immediately on a reported seek failure, on reaching the target, or at an
absolute 500 ms deadline. While waiting, every plugin idle can evaluate it;
the ordinary 100 ms admission-poll gate cannot extend that wait.

VLC 3's `libvlc_media_player_set_time()` returns void. The existing log observer
also recognizes the fixed `INPUT_CONTROL_SET_TIME ... failed or not possible`
format and hands only a failure bit to the message thread. A non-seekable
capability releases immediately too; silent refusal/changed diagnostics still
hit the 500 ms bound. No log arguments, paths or arbitrary VLC text are sent.
Source: [`lib/media_player.c`](https://github.com/videolan/vlc/blob/3.0.23/lib/media_player.c#L1367),
[`src/input/input.c`](https://github.com/videolan/vlc/blob/3.0.23/src/input/input.c#L2050).
Policy test 41 covers skipped seek, explicit refusal, silent refusal at 500 ms,
successful seek and absent Playing notification. Real video-track admission is
still required; the original 10 s never-video deadline is unchanged.

**3 — Evaluated always-repeat plus pause/stop; retained explicit-edit reopen.**
The required last-frame proof is absent, so this alternative was not adopted:

- In VLC 3.0.23, `MainLoop()` waits for empty decoder/vout queues before
  `MainLoopTryRepeat()`. Its marker is synchronous on the **input thread**, and
  title/position seek controls are queued **after the callback returns**.
  [`input.c`](https://github.com/videolan/vlc/blob/3.0.23/src/input/input.c#L610),
  [`messages.c`](https://github.com/videolan/vlc/blob/3.0.23/src/misc/messages.c#L56)
  and [`lib/log.c`](https://github.com/videolan/vlc/blob/3.0.23/lib/log.c#L62)
  show that the log callback is called inline, under the logger's read lock.
- Calling stop there is unsafe: `libvlc_media_player_stop()` holds the player
  input lock and releases/closes the input; `input_Close()` joins that very
  input thread. That is a self-join/deadlock hazard, not a frame-retention
  mechanism. Changing/unsetting the logger inside its callback would also
  require its write lock while the callback holds its read lock.
  [`media_player.c`](https://github.com/videolan/vlc/blob/3.0.23/lib/media_player.c#L162),
  [`input_Close`](https://github.com/videolan/vlc/blob/3.0.23/src/input/input.c#L222),
  [`vlc_LogSet`](https://github.com/videolan/vlc/blob/3.0.23/src/misc/messages.c#L390).
- Pause queues an input state change (or stops an input that cannot pause).
  Deferring pause/stop until after the callback, on the plugin message thread,
  avoids the direct self-join but does not order it ahead of the repeat seek:
  that seek can already be queued or executed before the next plugin idle.
  Position handling resets decoder state/clocks before asking the demux to
  seek. Thus neither API call establishes last-frame retention at the marker.
  [`set_pause`](https://github.com/videolan/vlc/blob/3.0.23/lib/media_player.c#L1022),
  [`seek handling`](https://github.com/videolan/vlc/blob/3.0.23/src/input/input.c#L2018).

Repeat ON/OFF therefore stays configured before input creation. Reopen remains
an explicit user-edit interruption; ordinary recorded-length Loop-ON wraps
continue on the existing input. Duration-less recovery retains its prior
documented hitch warning. No VLC/audio/GPU seam PASS is inferred from this.

**4 — Viewer logging and aggregation.**
The plugin sends `media/loop_seam` with load token, `gap_ms` and `frame_ms` from
its message thread. Frame duration comes from the enumerated source frame rate,
or the preceding accepted presentation interval when that rate is unavailable
(0 if neither exists). Callback work still only measures/queues numbers.
`LLPluginClassMedia` checks the current private load token and writes INFO under
`VCamMedia` in the viewer process, independent of SLPlugin file logging.

`llpluginloopseam.h` emits the first three seams individually per load, then
aggregates later seams in constant space. One summary per 10 s reports count,
minimum/maximum/mean gap and mean `max(0, gap_ms - frame_ms)` excess. Viewer idle
flushes nonempty windows even if playback pauses before another seam; empty
windows emit nothing. A new load resets the budget and rejects stale messages.
Projector test 40 checks statistics, deadline/idle flush, empty windows and rate
limits. Test 41 sends actual plugin-protocol messages into the viewer handler
and captures its INFO output: exactly three individual lines, frame/excess
values, stale-load rejection and reset for a replacement load.

**5 — Verbatim Performance and master-off text.**
Restored HEAD's complete `Presented … FPS · render p95 … · scale … · admitted
… attempts/s · Adaptive: …, protecting … FPS` format, its original arguments
and `performanceText()` mapping, plus `Virtual Cam is off. Nothing renders;
cameras and faces stay editable.` The restored block remains inside the 2 Hz
gate. `.tmp/media_live4/check_head_strings.py` compares the actual UTF-8 literal
bytes and complete state-label function against `git show
HEAD:indra/newview/alpaneldirectorvcam.cpp`; `head_strings.log` records PASS.

Round-4 verification artifacts are in `.tmp/media_live4/`:

| Check | Result |
| --- | --- |
| Authorized Release target build | PASS, `test_build_final.log`; no viewer/plugin target |
| Tests, launched from `build-Windows-vs2026-os/sharedlibs/Release` | **41/41 policy, 41/41 projector, 23/23 pose-polish PASS**, `tests_*.log` |
| Explicit `zs_check.ps1 -Files` | **18/18 TUs PASS**, `zs_final.log`, including all changed viewer/library/test .cpp files and additional header consumers |
| LibVLC explicit `-Files`, own project flags | **1/1 PASS**, `zs_plugin_final.log`; SHA256 of the mapped project equals the real `media_plugin_libvlc.vcxproj` |
| `prebuild_guard.ps1 -ChecksOnly` | All gates PASS, `prebuild_final.log`; no purge |
| `xui_layout_check.py --changed HEAD --json` | **40 files, zero findings**, `layout.json` |
| HEAD byte comparison and 2 Hz placement | PASS, `head_strings.log` |
| `git diff --check` | PASS |

The explicit syntax list checks `alvcammediafilepolicy.cpp` plus policy tests
for `alvcammediafilepolicy.h`; `llviewermedia.cpp` plus projector tests for
`llpluginclassmedia.h`; and projector tests directly include/test the new
`llpluginloopseam.h` (also included through the class header by the viewer
consumers). Earlier changed headers retain the round-3 consumer coverage above.
The gate scripts and project flags were reused unchanged; no threshold, test,
baseline, policy or layout check was weakened. The real-file probe still reports
SKIPPED without `VCAM_MEDIA_PROBE_FILE`. Live VLC decoding, actual file-specific
audio continuity, final-frame presentation and viewer/GPU/recorder seam timing
remain unexecuted under this round's no-viewer-build restriction.


## Fix round 5

2026-10-18. Fixed the paused/slow-open Loop-edit resume loss on the
uncommitted round-4 tree. No commit, push, viewer build, plugin build or launch.

`ResumeWait::open()` now saves the eligible target with an unarmed deadline.
The first observed Playing state arms the absolute 500 ms seek window in
`step()`, which requests the seek in that same step. Time spent paused or
opening cannot exhaust that window. Once armed, the deadline still expires
even if Playing disappears; repeated ticks cannot extend it. Play on a
not-yet-started replacement resets the pending seek/deadline, so a pause during
a pending seek also gets a fresh attempt after Play.

`setLoop()` uses `ResumeWait::loopTarget()` to carry an existing pending target
through another replacement, including when the replacement's admission has
reset seekability and its current time is zero. After the target completes or
fails, normal sampling again requires both admitted seekability and VLC's
seekable capability. Duration-less/non-seekable inputs without a pending target
still skip seeking. Reported seek refusal, actual non-seekability, reaching the
target and the armed 500 ms timeout retain their previous release behavior.
Video proof, publication/audio gates, wrap refusal and the ordinary 10 s
never-video bound are unchanged.

Policy test 41 retains the prior skip/refusal/success/timeout assertions and
adds slow open beyond 500 ms before Playing, extended pause before the first
Playing followed by a successful target resume, Play after pausing an already
armed pending seek, and re-toggles both before and after sending the seek.
It checks that re-toggles retain the original target, completed resumes use
the current sample, and absent Playing after arming cannot extend the bound.

Round-5 verification artifacts are in `.tmp/media_live5/`:

| Check | Result |
| --- | --- |
| Explicit `zs_check.ps1 -Files` | **3/3 TUs PASS**: `zs_policy.log`, `zs_plugin.log`; policy test plus additional header consumer `alvcammediafilepolicy.cpp`, and the LibVLC TU |
| LibVLC project flags | Mapped project SHA256 matches the real project: `plugin_project_hash.log` |
| `prebuild_guard.ps1 -ChecksOnly` | All gates PASS, exit 0; `prebuild.log`; no purge |
| Authorized Release target builds | PASS, exit 0; `test_build.log`; only policy/projector/pose-polish targets and dependencies |
| Tests from `build-Windows-vs2026-os/sharedlibs/Release` | **41/41 policy, 41/41 projector, 23/23 pose-polish PASS**, exit 0; `tests_*.log` |
| `git diff --check` | PASS; `diff_check.log` |

The existing gate scripts, thresholds and baselines were reused unchanged.
Only the three authorized integration targets and their dependencies were
built. The real-file probe reports SKIPPED without `VCAM_MEDIA_PROBE_FILE`.
Live VLC decoding, audio/video continuity and viewer/GPU capture remain
unexecuted under the no-viewer-build restriction.
