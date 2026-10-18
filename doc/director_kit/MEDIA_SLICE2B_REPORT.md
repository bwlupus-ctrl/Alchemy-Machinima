# Media slots: Slice 2b — private video files

Implemented against `SPEC_media_projector.md` v8.1 (`717ada6b218`), on top of
slice 1 `a42a4695519` and slice 2 `ffa4842f0ce`. Read `CLAUDE.md`, the entire
specification, both slice reports, and the mockup Source card.

`VCamMediaSlotsEnabled` remains **FALSE**. No build, commit, push, Director page,
scene save/load UI, new history, simulator message or asset upload was added.
Source controls are an internal API for slice 3. Existing face/aspect controls
only received null-safe handling for the additional payload.

**OFF exception:** `OPENSSL_init_crypto(OPENSSL_INIT_NO_ATEXIT, nullptr)` is
unconditional, checked, and precedes profiling/error handling/configuration/TLS
at `indra/newview/llappviewer.cpp:870`, as the specification requires. With the
flag initially off, private I/O does not initialize, acquire a lock, scan a cache,
or create a player. Retirement/shutdown calls return without creating the I/O
service. Existing camera/face routing retains its prior predicates' results.

## Implementation map

Paths below are relative to `indra/`, except settings and generated documentation
where explicitly named. References identify the implementation entry point;
they are source evidence, not a claim of runtime acceptance.

| Slice 2b item | Implementation |
| --- | --- |
| Append MEDIA_URL; HTTP/local descriptor, Loop ON, Volume zero; no camera payload | `newview/llprismlens.h:60`, `:767`, `:779` |
| Runtime-only owner, separate from descriptors/snapshots/rollback copies | `newview/llprismlens.cpp:1848`, `:9232` |
| Atomic syntax-only commit; invalid text does not replace the cue | `newview/llprismlens.cpp:2583`; pure classification in `newview/alvcammediafilepolicy.h:166` |
| Async Browse, caller-lifetime/selection predicate, generation/token checks, detached preflight, cue without playback | `newview/llprismlens.cpp:2643`; uses `LLFilePickerReplyThread::startPicker` |
| Append FFLOAD_VIDEO without renumbering; SDL/NFD, native Windows and native macOS extension tables | `newview/llfilepicker.h:84`; `newview/llfilepicker.cpp:237`, `:650`, `:1142` |
| Lexical UNC/device/authority refusal before filesystem calls; decode file URI once; native filename characters preserved | `newview/alvcammediafilepolicy.h:68`, `:166` |
| Windows drive locality, no-follow reparse refusal, pinned parent directories and read-only file handle denying write/delete | `newview/alvcammediafileio.cpp:175`, `:206` |
| POSIX mount-table locality and no-follow opens; exact descriptor revalidation in the VLC child | `newview/alvcammediafileposix.h:21`, `:59`, `:83`; plugin uses `libvlc_media_new_fd`, never a network/callback reader |
| Detached self-contained jobs, immutable per-job policy, main-owned subscriptions, bounded mailboxes, eight outstanding jobs including cancelled workers and maintenance | `newview/alvcammediafileio.cpp:35`, `:692`, `:798`; no pool, coroutine, future or joined worker |
| Five-second operation deadlines; CancelSynchronousIo; overlapped read/write and CancelIoEx; terminal main result without waiting | `newview/alvcammediafileio.cpp:102`, `:226`, `:774`, `:798` |
| Raw curl handle for every hop; NOSIGNAL=1, FOLLOWLOCATION=0, HTTP/HTTPS protocol mask, strict TLS, copied CA/proxy, no cookies/credentials/netrc; private player skips generic OpenID injection | `newview/alvcammediafileio.cpp:517`, `:576` |
| Initial/hop URL approval, relative redirects, at most five redirects, final extension/MIME/status/encoding gate before accepting body | `newview/alvcammediafilepolicy.h:112`, `:250`; `newview/alvcammediafileio.cpp:440` |
| Whole-file GET, clean nonempty 200, declared-length equality, no HEAD/Range/retry; 64 KiB disk writes, checked cap | `newview/alvcammediafileio.cpp:440`, `:490`, `:517` |
| Connect/no-progress/total deadlines: 30 s / 30 s / 30 min; progress and exact error strings | `newview/alvcammediafileio.cpp:576`, `:798`; constants in `newview/alvcammediafilepolicy.h:12` |
| SHA-256 of complete authored URL excluding fragment, preserving query; hashed unique entry/partial names | `newview/alvcammediafilepolicy.cpp:6`; `newview/alvcammediafileio.cpp:517` |
| Off-main cache-root locality then process-lifetime exclusive cache.lock; five sharing retries over about two seconds; owner-only startup sweep/index/HTTP | `newview/alvcammediafileio.cpp:336`; retry policy in `newview/alvcammediafilepolicy.h:304`; init hook `newview/llappviewer.cpp:4905` |
| Exact second-viewer status; local files remain independent | `newview/alvcammediafilepolicy.h:16`; `newview/alvcammediafileio.cpp:827`, `:884` |
| Atomic index, persistent LRU order independent of reboot clocks, immutable leases, complete-plus-reserved accounting, declared-length or cap reservation | `newview/alvcammediafileio.cpp:291`, `:395`, `:417`, `:440` |
| Same-URL in-flight sharing with independent subscriptions; last unsubscribe cancels | `newview/alvcammediafileio.cpp:770`, `:827` |
| Clear advances cache epoch, cancels HTTP subscriptions, stops cache-backed slots, preserves local sources/cues, defers removal until leases release | `newview/llprismlens.cpp:2668`; `newview/alvcammediafileio.cpp:850`; owner-only usage snapshot `:884` |
| Reversible FileDispositionInfoEx DELETE\|ON_CLOSE, DO_NOT_DELETE\|ON_CLOSE, same-handle FileRenameInfo with ReplaceIfExists FALSE; unsupported-Ex cleanup/sweep fallback | `newview/alvcammediafileio.cpp:560`, `:629`, `:637`, `:680`; no FILE_FLAG_DELETE_ON_CLOSE |
| Main approves the current subscribed job/cache epoch before index publication; failed/unindexed promotion cleanup | `newview/alvcammediafileio.cpp:656`, `:680`, `:798` |
| Exact-file 64-byte sniff, BMFF/old QuickTime/EBML checks; structured reference gate skips compressed frames, rejects dref/rmra/cmov and MKV linked-segment metadata; validates seek/cue targets | `newview/alvcammediafileio.cpp:269`; `newview/alvcammediafilepolicy.h:273`; `newview/alvcammediafilecontainer.h:34`, `:114`, `:140` |
| Fresh private UUID only after explicit Play/preflight; objectless policy; per-impl forced VLC before spare/platform routing; no MIME discovery; once-encoded file URI | `newview/llprismlens.cpp:2676`; `newview/llviewermedia.cpp:1856`, `:2052`, `:2094`, `:2764`, `:3111`; `newview/alvcammediafilepolicy.h:239` |
| Demux pin before player creation; private input never receives creation-time input-repeat | `media_plugins/libvlc/media_plugin_libvlc.cpp:550`, `:623` |
| PENDING, MUTED; tokened event flags; normal-message-thread video-track queries and 100 ms rechecks; fixed ten-second opening deadline; arrays released; unknown duration admitted/non-seekable | `media_plugins/libvlc/media_plugin_libvlc.cpp:148`, `:168`; `llplugin/llpluginclassmedia.cpp:1180`; pure state `newview/alvcammediafilepolicy.h:285` |
| No pending frame publication; viewer and plugin zero-gain latches, private non-positional requested/global volume | `llplugin/llpluginclassmedia.cpp:1213`; `newview/llviewermedia.cpp:2319`, `:3096`; `media_plugins/libvlc/media_plugin_libvlc.cpp:666` |
| Desired Playing/Paused/Stopped carried through startup, load_uri and size_change; admitted EndReached Loop resets admission/gain on the replacement player; non-loop EOF stops and the next Play repeats all gates | `newview/llprismlens.cpp:2613`; `media_plugins/libvlc/media_plugin_libvlc.cpp:200`, `:499`, `:823`, `:901`, `:921` |
| Owned playback/bootstrap demand independent of output/preview/freeze; ordinary CPU/instance admission retained; budget loss revokes Play/recreation | `newview/llprismlens.cpp:2796`; `newview/llviewermedia.cpp:4674`; no HIGH/UI/focus or cap override |
| Guarded, idempotent teardown; deferred last owner release after raw traversal; player quiesces before cache lease release; Stop/scene/removal/policy/master/feature/logout paths | `newview/llprismlens.cpp:2542`, `:2562`, `:2836`, `:3505`, `:9067`; safe boundary `newview/llviewermedia.cpp:687` |
| Stop releases volumetric ownership; stopped/pending private slots cannot reacquire it | `newview/llprismlens.cpp:2542`, `:6338` |
| Both media kinds reuse retained output/copy/projector/probe paths; camera behavior unchanged | `newview/llprismlens.h:61`; five predicate substitutions in `newview/pipeline.cpp:2138`, `:2343`, `:10918`, `:28050`, `:28064`; two in `newview/llreflectionmapmanager.cpp:1305` |
| Syntax-only scene carriage/restoration; explicit location, strict fields, Loop/Volume defaults, no runtime/cache/player state | `newview/llprismlens.cpp:7102`, `:7559`, `:8437` |
| DownloadMaxMB=2048, CacheMaxMB=8192; checked conversion and immutable subsequent-job snapshots | `newview/app_settings/settings.xml:29709`, `:29716`; `newview/alvcammediafilepolicy.h:306`; `newview/llprismlens.cpp:10119` |
| Quit cancels all jobs/mailboxes at cleanup entry, one two-second drain, no joins; curl cleanup guarded; persistence-complete forced exit regardless of BDMergeFastExit | `newview/llappviewer.cpp:1903`, `:2403`; `newview/alvcammediafileio.cpp:858`, `:866`, `:873`; `llcorehttp/httpcommon.cpp:349` |

The generated `page_sources.json` and `page_sources.md` were refreshed with the
existing generator because source references moved. No generated UI, search
inventory, threshold, checker, assertion, baseline or coverage requirement changed.

## Tests and verification

The existing `LL_ADD_INTEGRATION_TEST` pattern registers
`alvcammediafilepolicy` at `indra/newview/CMakeLists.txt:2958`. Its nine TUT cases
are in `indra/newview/tests/alvcammediafilepolicy_test.cpp`:

1. URL/native/file-URI classification, IPv6, native punctuation, single escaping,
   default Loop/Volume and rejected syntax.
2. Canonical UNC/device/authority refusal and lexical path normalization.
3. Extension/container-header/MIME admission, including truncated/mismatched data.
4. Redirect resolution, host/scheme/port/userinfo/control/backslash policy and hop cap.
5. SHA-256 known vector, fragment exclusion, query distinction and hashed spelling.
6. Audio-first pending, stale token, length-independent video admission, seek
   capability, pause mute, rebuild reset, fixed deadline and terminal failure.
7. Lock retry/backoff bound and checked MiB conversion.
8. Structured MP4 self-contained versus local/UNC/HTTP references, reference and
   compressed movies, truncation, and reference-like bytes inside mdat.
9. Structured MKV/WebM references, opaque encoded marker bytes, unknown-length
   Segment/Cluster, real cues/seek targets and seeks into opaque payload.

Existing MEDIA_FACE case-14 assertions remain intact. Additive `llprismprojector` case 14 checks cover the private tagged payload,
defaults, camera exclusion, shared routing capability and gate-subscription refusal.

These test translation units were syntax checked. **No TUT executable was built
or executed.** The requested no-build restriction remains in force.

Final checks passed on the delivered sources. The final hash check covered 25 source/configuration files. No source changed after its final syntax check.

| Check | Final result / evidence |
| --- | --- |
| Required per-file `powershell -NoProfile -File C:/Users/xianw/cxs/zs_check.ps1 -Files <file>` | PASS: 13/13 mapped TUs, `.tmp/media_slice2b/zs_complete.log`; final follow-ups in `zs_test_complete.log`, `zs_privacy_final.log` and `zs_io_final_errors.log` |
| Nested LibVLC source using unchanged gate parser and its real Release project flags | PASS: nested VLC TU, real project flags, exit 0; `.tmp/media_slice2b/zs_plugin_complete.log` |
| `powershell -NoProfile -File C:/Users/xianw/cxs/prebuild_guard.ps1 -ChecksOnly` | PASS: every gate, exit 0; `.tmp/media_slice2b/prebuild_final_complete.log` |
| OFF source comparison and default flag | PASS, `.tmp/media_slice2b/off_path.log` |
| `git diff --check` | PASS; final rerun recorded with final gate output |

Gate environment handling follows the prior slices: the supported CachePath
argument redirects the flags cache into the workspace; a process-local empty Git
ignore override preserves inherited safe-directory configuration; Python is the
existing full-path Python 3.14 executable. No gate code or flags were edited.
The stock mapper skips the nested VLC project, so its real project flags are
also checked explicitly. A parallel early attempt collided on the checker's
shared response file; the final checks run sequentially. Early unsupported curl
option names were replaced with the bundled version's protocol-mask options.

Final raw guard output:

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

The OFF proof compares the entire pipeline/reflection sources against HEAD after
only the seven explicit both-media predicate substitutions. It also compares the
legacy navigation/load/volume bodies after folding private-only branches false,
and checks the saved default remains zero. This is source evidence, not a viewer
beauty/audio/GL runtime comparison.

## Known limits and outstanding acceptance

- The flag stays off. Viewer/GL/audio/network/filesystem fault injection, two
  viewers sharing a cache, supported/unsupported disposition classes, crash
  recovery, blocked TLS/disk quit with BDMergeFastExit=0, delayed-track admission,
  paused resize, stale callbacks/uploads, sentinel side-file denial and recorder
  timing have **not** been exercised. No independent Opus implementation review
  or full viewer/plugin/test build was performed.
- Windows syntax checks exclude POSIX branches. macOS/Linux mount checks,
  descriptor playback and native picker behavior still require their platform
  validation. All reparse points/symlinks and unknown Linux filesystem types fail
  closed. Compressed QuickTime movie metadata and overly deep/large metadata
  structures also fail closed.
- Authenticated SOCKS download configuration is refused with Download failed;
  no secret is read/copied/persisted for private jobs. Unauthenticated configured
  proxies and the copied viewer CA path are supported. Authored cues are retained
  by the scene descriptor; the cache index stores no raw URL, host or query.
- Windows observes actual detached-thread termination before releasing the curl
  lease. POSIX conservatively retains curl leases and forces process exit after
  using the service even when work drained, because there is no portable signalled
  handle proving a detached thread completed its CRT/TLS epilogue.
- Cache commands/transfers serialize at the owner. Same-URL requests share one
  transfer; different URLs may wait. No throughput, performance or runtime
  acceptance claim is made.
- Source-card/search/scene UI and expanded UI functional oracles are deliberately
  absent under this task's explicit slice-3 exclusion. The internal source,
  transport, Browse, cache Clear and owner-only cache-status APIs are available.

The pre-existing dirty vcpkg submodule and unrelated untracked files were left
alone. Logs, temporary edit scripts and gate runners are under `.tmp/media_slice2b/`.

## Fix round 1

Addresses all thirteen review items against v8.1. This section supersedes the
original implementation's maintenance lifetime, cache serialization, retirement,
opening-deadline and POSIX-identity descriptions above. Slice 3 Director controls
remain excluded. The working tree remains uncommitted; no build or push ran, and
no secret was read, copied into a job, logged or persisted.

References below are relative to `indra/`. Regression cases are additive: the
original policy cases 1–9 and projector cases 1–29 retain their assertions.
`F10` means `newview/tests/alvcammediafilepolicy_test.cpp` case 10; `P30` means
`newview/tests/llprismprojector_test.cpp` case 30.

| Review item | Fix and source | Regression / verification |
| --- | --- | --- |
| 1. Maintenance worker death | `newview/alvcammediafileio.cpp:51`, `newview/alvcammediafileio.cpp:118`, `newview/alvcammediafileio.cpp:866`; permanent maintenance has no total deadline. Per-operation expiry fails only its item; every item and iteration catches exceptions. Clear failure releases the pending gate and reports through cache status. | F10 (`newview/tests/alvcammediafilepolicy_test.cpp:195`): simulated time past 30 minutes, a 5-second operation failure, then successful retirement and Clear; also nonstandard exceptions and preserved transfer cancellation. The original shared total/sticky-cancel policy fails these expectations. |
| 2. Deadline double load | `newview/alvcammediafilelifecycle.h:14`, `newview/alvcammediafileio.cpp:931`, `newview/alvcammediafileio.cpp:963`; each check consumes one atomic snapshot. | F11 (`newview/tests/alvcammediafilepolicy_test.cpp:220`): a deadline returns 100 on its first load and 0 on a second; time 90 must not expire and the check must load once. The original double load spuriously expires. |
| 3. Scene/kind replacement orphans | `newview/llprismlens.cpp:2538`, `newview/llprismlens.cpp:2551`, `newview/llprismlens.cpp:2827`, `newview/llprismlens.cpp:2847`, `newview/llprismlens.cpp:2864`, `newview/llprismlens.cpp:8450`; retire before slot/scene replacement, sweep before filtering media slots, compare generation and URL/location identity, and exclude mismatches from resolution and demand. | P30 (`newview/tests/llprismprojector_test.cpp:594`): a playing URL fixture becomes a camera-only scene; orphan demand becomes zero even before reconciliation, and the sweep releases the owner. Also rejects reused generations. The original kind-filtered sweep leaves this owner alive. |
| 4. Protection through child close | `newview/llviewermedia.cpp:1845`, `newview/alvcammediafileio.cpp:155`, `llplugin/llpluginfileclose.h:6`, `llplugin/llpluginprocessparent.cpp:335`, `llplugin/llpluginprocessparent.cpp:1182`, `media_plugins/libvlc/media_plugin_libvlc.cpp:934`; pass the lease to maintenance before child shutdown. A tokened acknowledgement follows VLC stop/release; otherwise actual child exit is required. Maintenance owns the five-second deadline and termination request; parent termination is non-waiting. Timeout alone never authorizes unlink/replacement. | F12 (`newview/tests/alvcammediafilepolicy_test.cpp:231`): Stop, timeout and termination request all keep the protection alive; only acknowledged close/exit releases it. The original immediate lease reset fails before acknowledgement. |
| 5. QuickTime MOV | `newview/alvcammediafilecontainer.h:34`, `newview/alvcammediafilecontainer.h:65`, `newview/alvcammediafilecontainer.h:76`; distinguish QT versus ISO meta by a plausible first hdlr child, accept self-contained url/alis without external payload, and accept the final 32-bit zero in udta. | F13 (`newview/tests/alvcammediafilepolicy_test.cpp:248`): synthetic iPhone MOV with QT meta + self-contained alis + udta terminator admits; ISO meta admits; alis without flag 1, rmra, external URL, flag-1 URL with payload, nonzero udta tail, and references inside QT meta refuse. The original gate rejects the positive QT fixture. Existing reference assertions remain intact. |
| 6. Admission starts on opening | `llplugin/llpluginclassmedia.cpp:838`, `llplugin/llpluginclassmedia.cpp:1192`, `llplugin/llpluginfileadmission.h:4`, `media_plugins/libvlc/media_plugin_libvlc.cpp:587`; queueing starts the existing launch allowance; the first opened-player report starts the fixed ten-second verdict window. Player tokens distinguish rebuilds; notifications cannot extend a player deadline. | F14 (`newview/tests/alvcammediafilepolicy_test.cpp:268`): 25 seconds of launch delay leaves the full verdict window; repeated opening reports cannot extend it; a hung launch still expires at its launch allowance. The original queue-time verdict rejects at 110 instead of allowing opening at 125. |
| 7. POSIX validated-byte identity | `newview/alvcammediafileposix.h:84`, `newview/alvcammediafileposix.h:94`, `media_plugins/libvlc/media_plugin_libvlc.cpp:195`, `media_plugins/libvlc/media_plugin_libvlc.cpp:522`, `media_plugins/libvlc/media_plugin_libvlc.cpp:549`; retain the descriptor, record device/inode/size/nanosecond mtime before validation, then recheck after validation/admission and compare retained plus reopened descriptors on every Loop/rebuild. A mismatch stops with the video refusal. | F15 (`newview/tests/alvcammediafilepolicy_test.cpp:281`): unchanged identity accepts; changing each of the five recorded fields rejects. The original descriptor-only policy has no change check. POSIX runtime acceptance remains outstanding; Windows syntax checks cannot execute those branches. |
| 8. Cache transfer lock and queued clock | `newview/alvcammediafileio.cpp:479`, `newview/alvcammediafileio.cpp:614`, `newview/alvcammediafileio.cpp:616`; separate transfer/cache locks, pin bookkeeping then validate outside the cache mutex, protect active filenames from sweeps, preserve reservations during unrelated hits, and start the transfer clock after acquiring the transfer slot. | F10 verifies queued time does not consume the transfer limit. Source inspection verifies cache hits return before acquiring the transfer mutex and the GET runs without the cache mutex. Concurrent download/cache-hit runtime acceptance remains outstanding. |
| 9. Corrupt cache hit | `newview/alvcammediafileio.cpp:479`, `newview/alvcammediafileio.cpp:513`, `newview/alvcammediafileio.cpp:168`; failed validation closes its handle on the current isolated worker before eviction/redownload. Missing/size/container failure evicts the record and takes the ordinary miss path once. Old leased files stay protected and count toward capacity. No network-failure retry was added. | F17 (`newview/tests/alvcammediafilepolicy_test.cpp:310`) checks the exact retryable cache-integrity failures and terminal transfer/storage/locality failures; existing F3/F8 retain negative header/container fixtures. |
| 10. Minimize and Loop restart | `newview/llviewermedia.cpp:863`, `newview/llprismlens.cpp:2730`; minimization cannot lower private playback to HIDDEN; genuine CPU/instance/policy budget decisions remain. DONE cannot retire an admitted Loop restart. | F18 (`newview/tests/alvcammediafilepolicy_test.cpp:318`): minimized private playback keeps priority, ordinary minimized media still hides, admitted looping DONE stays alive, and non-loop EOF still stops. |
| 11. MEDIA_URL functional aspect | `newview/aluiperfselftest.cpp:7463`; null-check the tagged face/URL payload and use mediaAspect(). | P30 includes a URL-only CINEMA_239 fixture with null mediaData() and verifies mediaAspect(). |
| 12. Exact refusal text | `newview/alvcammediafileio.cpp:250`, `newview/alvcammediafilepolicy.h:211`; final reparse points and DOS device aliases use exactly `Network-share paths are not allowed. Copy the file to a local drive.` | F16 (`newview/tests/alvcammediafilepolicy_test.cpp:300`) checks NUL/CON/AUX/COM1/LPT9 and the exact text; F2 retains all lexical network/device refusal assertions. Final reparse classification was source-checked; native injection remains outstanding. |
| 13. Authenticated SOCKS | `newview/llprismlens.cpp:10170`, `llmessage/llproxy.h:256`, `llmessage/llproxy.h:289`, `llmessage/llproxy.cpp:435`; the public API applies live singleton state to CURL. Credential getters are private; no immutable authentication snapshot is exposed for workers surviving teardown. Downloads stay disabled in that configuration, with `Download failed (Authenticated SOCKS video downloads are unavailable; use a local video file)`. | Inspected the configured-proxy APIs and the fail-closed branch; no live proxy or credentials used. Local-file playback is available. No authenticated-proxy runtime success is claimed. |

Accepted POSIX platform limitation: a local writer can perform an in-place rewrite
that preserves the inode, size and original nanosecond mtime. The identity checks
cannot detect that same-size, same-mtime rewrite. This is an accepted platform
limitation, not a promise of immutable POSIX bytes. Windows continues to deny
write/delete with retained protection handles, now through actual child close.

The new behavioral fixtures exercise production lifecycle predicates and the
container walker. No TUT executable was built or executed under the no-build
instruction; this report does not claim an executed red/green TUT run. The
pre-fix failure descriptions above identify the original behavior each fixture
regresses. New fixtures and all touched translation units receive syntax and
semantic validation through the unchanged `/Zs` gate.

Final verification: **PASS**. No existing checker, gate, threshold or test
assertion was weakened or skipped. The standard TU mapper does not map the nested
VLC project; its unchanged flag parser and the real VLC project flags were used
for the additional `/Zs` check, as in the original slice report.

| Check | Result / evidence |
| --- | --- |
| `zs_check.ps1 -Files` for every touched C++ file, plus the policy TU | PASS: nine mapped TUs in `.tmp/media_slice2b_fix1/zs_complete.log`; the final worker follow-up in `zs_io_final.log` also passes. |
| Nested LibVLC using the unchanged gate parser and real project flags | PASS, exit 0: `.tmp/media_slice2b_fix1/zs_vlc_final.log`. |
| `prebuild_guard.ps1 -ChecksOnly` | PASS, exit 0: `.tmp/media_slice2b_fix1/prebuild_final.log`; all gates ran. |
| `git diff --check` | PASS. |

The first complete guard run identified stale generated source-line references.
Regenerated `doc/director_kit/page_sources.json` and `page_sources.md` with the
existing `director_page_sources.py`; no generator/checker or UI control changed.
The full unchanged guard then passed. Its final output:

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


## Fix round 2

Addresses the eight second-round findings against specification v8.1. This
section supersedes the fix-round-1 claims about Clear failure, fence timing,
fixture coverage, and admission on same-file size rebuilds. The tree remains
uncommitted. No build, commit or push ran; no secret was read or stored. Director
source/transport/cache controls remain deferred to slice 3.

References are relative to `indra/`. F19-F26 name cases in
`newview/tests/alvcammediafilepolicy_test.cpp`; P31-P32 name cases in
`newview/tests/llprismprojector_test.cpp`. All earlier test assertions remain
unchanged. These are additive production-path fixtures, syntax checked under
`/Zs`; no TUT executable was built or executed.

| Item | Fix / evidence | Regression test |
| --- | --- | --- |
| 1. Worker clock data race (P1) | `newview/llviewermedia.cpp:1846` passes the fence itself, without a caller-supplied clock callback. `newview/alvcammediafileio.cpp:155` constructs the production `DeferredClose` (`newview/alvcammediafilelifecycle.h:102`); its poll calls `LLPluginFileClose::poll()` (`llplugin/llpluginfileclose.h:12`), which reads only `std::chrono::steady_clock`. | F19 (`newview/tests/alvcammediafilepolicy_test.cpp:329`) uses that exact retention constructor and no-argument poll with future/past steady deadlines, verifies timeout retains the lease, then releases it through the parent's observed-exit transition. The former epoch-based LLTimer read would expire the future steady deadline incorrectly. |
| 2. Reaped/reused POSIX PID (P1) | `llplugin/llpluginprocessparent.cpp:339` drives `LLPluginFileClose::observeProcess` (`llplugin/llpluginfileclose.h:27`), which checks the owned process's running state before consuming the force request or invoking SIGKILL. A reaped child acknowledges close without signalling. A termination request alone never acknowledges close. | F20 (`newview/tests/alvcammediafilepolicy_test.cpp:349`) drives the production parent state machine with injected process observations: already reaped, running, timeout/force, still running, actual exit, pending launch and cancelled launch. It asserts state observation precedes signalling, reaped PIDs receive zero signals, and the lease survives until observed exit. |
| 3. QuickTime free/skip/wide and cmov (P1) | `newview/alvcammediafilecontainer.h:35`, `:85`: bounded padding probing refuses valid nonzero box chains conservatively, at root/moov levels and nested padding; this includes movie, reference and compressed-movie boxes and wrappers around them. Zero padding and payloads that do not parse as box chains remain allowed. The ordinary structural walk continues rejecting cmov at every visited box. Encoded frame payload stays opaque. | F21 (`newview/tests/alvcammediafilepolicy_test.cpp:382`) refuses ftyp + QT meta + free(mvhd, rmra), moov(cmov), and all seven named metadata types in root/moov/nested free/skip/wide. It admits zero/non-box padding plus a valid movie and the iPhone-style QT meta/self-contained-alias/udta fixture. Existing F8/F13 remain intact. |
| 4. Hollow fixtures (P2) | Extracted production entry points rather than adding comparison-only assertions: `validateFile` (`newview/alvcammediafilecontainer.h:247`) is now called by `verifiedLocalFile` (`newview/alvcammediafileposix.h:99`) and takes the prior identity on Loop/rebuild (`media_plugins/libvlc/media_plugin_libvlc.cpp:556`). `validatedCacheHit` (`newview/alvcammediafilelifecycle.h:82`) is called by `cached` (`newview/alvcammediafileio.cpp:491`). `replaceSceneSources` (`newview/alvcammediafilelifecycle.h:54`) is the retire-before-reset core called by the validated scene-apply commit (`newview/llprismlens.cpp:8459`). | F22 (`newview/tests/alvcammediafilepolicy_test.cpp:407`) runs real sniff/container validation and mutates each identity field during its reader, then checks reopening against the earlier validated identity. F23 (`:450`) feeds real good/corrupt container bytes through the production cache-hit entry, verifying publish versus evict/miss and terminal failure propagation. P31 (`newview/tests/llprismprojector_test.cpp:624`) invokes the actual scene replacement core, asserting every owner (including a camera-slot orphan) retires before any source resets. F19/F20 replace the hand-set close confirmation with real fence transitions. Limitations below narrow these claims. |
| 5. Authenticated SOCKS (P2) | **ACCEPTED limitation: not supported in v1.** Downloads remain disabled at `newview/llprismlens.cpp:10183`. The exact status is `Download failed (Authenticated SOCKS video downloads are unavailable; use a local video file)`. `newview/alvcammediafileio.cpp:606` fails before cache/network work; `failureStatus` preserves this text (`:140`), the job publishes it (`:824`), `tickPrivate` forwards it into stopped runtime state (`newview/llprismlens.cpp:2712`, `:2724`), snapshot copies it (`:6948`), and the existing status control displays `capture->mRuntime.mReason` (`newview/alpaneldirectorvcam.cpp:881`). No authentication code or credentials were accessed. | Source trace confirms the unchanged visible status path. No live authenticated-proxy runtime test or success claim. No code change for this limitation. |
| 6. Paused rebuild / late resume (P2) | `llplugin/llpluginclassmedia.cpp:861` restarts the pending viewer verdict window on transition to Playing, through `LLPluginFileAdmission::resumed` (`llplugin/llpluginfileadmission.h:11`); duplicate Playing notifications do not extend it. The production verdict calculation (`llplugin/llpluginclassmedia.cpp:872`) preserves positive verdicts. Same-file size rebuilds retain positive video admission via `Admission::rebuild` (`newview/alvcammediafilepolicy.h:292`, `media_plugins/libvlc/media_plugin_libvlc.cpp:594`). A fresh URI explicitly resets admission (`:969`), and EOF Loop explicitly resets admission (`:211`). POSIX identity revalidation still precedes retained admission. | F24 (`newview/tests/alvcammediafilepolicy_test.cpp:484`) opens while paused, resumes after 25 seconds, and calls the production viewer verdict calculation just before/at its new deadline; duplicate Playing cannot extend it. F26 (`:522`) drives the production admission rebuild through admitted/paused/rebuilt/late-resume states, checks retained admission and mute/seek behavior, and verifies a new file cannot inherit it. |
| 7. Maintenance status / failed Clear (P2) | Actual maintenance items use `maintenanceWork` (`newview/alvcammediafilelifecycle.h:32`; callers at `newview/alvcammediafileio.cpp:872`, `:876`, `:888`, `:900`), clearing `maintenanceFailed` after success. The outer idle/no-op iteration cannot erase an inner failure. `maintainCache` (`newview/alvcammediafilelifecycle.h:36`; production caller `newview/alvcammediafileio.cpp:890`) leaves Clear pending on a throwing sweep/index publication and schedules cleanup retry. The existing hit gate remains in `cached` (`:485`), and leased entries keep Clear pending until they drain. | F25 (`newview/tests/alvcammediafilepolicy_test.cpp:500`) drives those production maintenance entries through throwing Clear, successful sweep with an outstanding lease, and successful completion after lease release. It verifies failure visibility, recovery, retry gating and Clear completion. |
| 8. Browse from a media-face slot (P2) | `newview/llprismlens.cpp:2663` starts a standalone `FilePickerTicket`, creating no private playback owner. Reconcile therefore cannot retire the pending picker for a face slot. Only the current picked-path callback converts the slot to an empty URL cue, using `SourceSlotData::fileBrowseCue` (`newview/llprismlens.h:798`) to preserve aspect, then obtains a matching owner for preflight. `stopPrivate` also invalidates picker tickets on stop/replacement/scene/disable. | P32 (`newview/tests/llprismprojector_test.cpp:646`) drives the production picker ticket through pending reconciliation, current pick, conversion cue/default/aspect preservation, replacement cancellation and caller-selection loss. Native dialog and asynchronous viewer preflight integration remain runtime acceptance, as described below. |

Worker audit: reviewed every new `alvcammediafile*` header/TU, both new
`llpluginfile*` headers, the retirement callbacks, raw-curl callbacks, the
LLCore transfer lease counter, and the added VLC decoder/event callbacks.
`millis()`/job/deadline/lock-retry/shutdown reads in the I/O service already use
steady_clock. The POSIX validator deadline and VLC `privateNow()` also use
steady_clock. No worker path reads LLTimer, LLFrameTimer, gFrameTime,
gSavedSettings, gDirUtilp or another viewer-main-only global after this fix.
Settings/CA/proxy snapshots and child process observation stay on main; worker
service accesses use immutable policy, atomics, or their documented mutexes.
VLC event/frame callbacks use immutable context and atomic notifications for
private files. The remaining LLTimer reads in LLPluginClassMedia are on the
parent's normal main message/transport path, outside worker callbacks.

Coverage limits (these supersede the broader fix-round-1 descriptions): F15
alone checks identity equality, F12 alone injects close confirmation, F17 alone
checks failure classification, and P30 alone simulates replacement then calls a
sweep. None of those original cases is claimed as end-to-end regression
protection. The new fixtures exercise the production orchestration cores with
injected readers, identity snapshots, process observations and storage effects.
They do not execute native POSIX open/fstat/pread or actual PID reuse, real
SIGKILL/child IPC, HTTP redownload/disk eviction, the complete viewer LLSD scene
parser plus GL teardown/reconciliation, the native picker callback, or VLC audio
and size_change message delivery. Those integrations need a viewer/platform
fixture and remain unverified; no regression protection is claimed for them.
Under the no-build instruction, all new TUT cases are syntax/semantics checked,
not executed. No runtime red/green, race-detector, or playback acceptance claim
is made.

Final round-2 verification: **PASS**. The unchanged `zs_check.ps1 -Files`
checks passed for all 15 mapped slice translation units, including both test
TUs. The nested LibVLC TU also passed with the authoritative gate's unchanged
flag parser and real Release project flags, covering the mapper's unsupported
nested-project layout. No checked C++ source changed after its final check.
The unchanged full prebuild guard passed every gate with exit 0.

| Required check | Result / evidence |
| --- | --- |
| `zs_check.ps1 -Files` on every touched `.cpp` (including the full existing slice tree) | PASS: 15/15 mapped TUs, `.tmp/media_slice2b_fix2/zs_final.log`. The raw mapper reports its inherited VLC mapping limitation; the next row supplies its mandatory check. |
| Nested `media_plugin_libvlc.cpp` `/Zs`, real project flags | PASS, exit 0: `.tmp/media_slice2b_fix2/zs_vlc_final.log`. Total: 16/16 slice TUs checked. |
| `prebuild_guard.ps1 -ChecksOnly` | PASS, every gate, exit 0: `.tmp/media_slice2b_fix2/prebuild_final.log`. |
| `git diff --check` | PASS. |

The supported flags-cache argument and process-local Git/Python environment
handling match fix round 1. No assertion, gate, checker, baseline, threshold or
coverage requirement was weakened, edited or skipped. The existing
`director_page_sources.py` generator refreshed `page_sources.json` and
`page_sources.md` after line numbers moved; its unchanged `--check` gate passed.
No Director controls were added.

Final guard output:

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


## Fix round 3

Addresses the round-3 Opus/Codex findings against specification v8.1. This
section supersedes round 2's admission-retention behavior and indefinite failed
Clear gate. The tree remains uncommitted. No build, commit or push ran; no secret
was stored. The Director UI remains deferred to slice 3.

| Finding | Production change | Regression coverage |
| --- | --- | --- |
| P1: malformed padding tails and opaque hidden movies | `alvcammediafilecontainer.h::ContainerGate::scanMetadata` scans every byte of every non-`mdat` root box, including unknown/uuid/meta/free payloads and all nested moov bytes, for cmov/rmra/rmda/rdrf/foov/dcom/cmvd. A rolling FourCC spans 64 KiB reads. Non-mdat metadata over 64 MiB refuses; only mdat can skip its payload. Root boxes outside moov/mdat/ftyp/meta also refuse hidden mvhd/moov/trak bytes. Existing conservative nested-padding/dref refusal remains. Structural sizes, real moov drefs, QT/ISO meta and udta termination still fail closed; only a harmless short fragment at physical EOF is tolerated. `alis` is deliberately absent from the scan list. | F27 drives the production `checkContainer` function: free(cmov)+four stray bytes, foov(cmov(moov(rmra))), free(mvhd,rmra)+one stray byte, uuid/rmra, every forbidden marker in each root type, unaligned/chunk-spanning markers, unknown-root hidden movies, malformed headers/nested tails, metadata bound, mdat opacity, empty free, wide, zeros and random text. F13/F21 retain the iPhone MOV/alias/QT meta/terminator checks. The previous production gate admits the malformed-padding and foov/uuid fixtures that F27 requires to refuse. |
| P1: replacement inherits admission | `Admission::rebuild` always resets pending and seekability; LibVLC resets it before replacement creation and again at the actual open, so its existing `setVolumeVLC` latch applies zero gain. Every replacement sends pending/opened with a fresh player token, starting the viewer's fresh window. A paused pending player does not tick its verdict; actual resume starts a fresh ten-second decoder window and the viewer's opened-player window. Only a confirmed current-player video track re-admits it. | **F26 was replaced**, as explicitly authorized: the former assertion that same-file rebuilds retain admission contradicted spec line 362. Replacement F26 calls production `rebuild`, `resumed`, `observe`, `audible` and `LLPluginFileAdmission` methods. It asserts muted/pending/unknown-seek after rebuilding an admitted file, no stop on resume 98 seconds after the rebuild, rejection of the old token, silence until fresh video, and a bounded fresh deadline. The former `rebuild` fails the immediate pending/zero-gain requirements. |
| P2: resume consumes launch allowance | `LLPluginFileAdmission` records whether a player actually opened. Resume may restart ten seconds only after open; queued launches retain their launch allowance. | F28 drives the production clock in both notification orders, checks launch timeout, late opened-player resume and duplicate Playing. Existing F14/F24 are unchanged. |
| P2: failed Browse loses face | `llprismlens.cpp` keeps pending Browse/preflight separately from private playback ownership. Neither picking nor failing preflight replaces the descriptor. Only a successful current result calls `setMediaUrlSource` through `commitFileBrowse`; reconciliation continues using the committed face and overlays checking/failure status. Stop/replacement/disable cancel the pending picker/preflight. | P33 drives the production `commitFileBrowse` helper with a real `SourceSlotData`: failure preserves kind/object/TE and exposes the failure status, cancellation preserves the face, success converts and preserves aspect. Existing P32 assertions are unchanged. |
| P2: failed Clear poisons all HTTP Play | Cache lookup waits cancellably outside `cacheMutex` while Clear is pending. Three consecutive failed sweeps run the production degraded fallback: clear every live index/evicted record, best-effort publish an empty disk index (or remove it if publication fails), release the Clear gate, and retain the degraded status. The fallback does no fallible directory enumeration. Ordinary maintenance after degradation leaves orphan deletion to the next startup sweep, so the same undeletable file cannot block new downloads again. Real capacity/ENOSPC paths retain their existing error mapping. | F29 drives production `awaitCacheClear`, retrying `maintainCache` and `maintenanceWork`: HTTP waits through three failures, records drop exactly once, no stale hit remains, degraded completion releases the wait, and Stop cancels a waiter. Exact pending/degraded/disk-full strings are asserted. Existing F25 remains unchanged and continues to exercise individual sweep failures and lease draining. |

The specification's section-7 string list now includes **"Clearing video
cache..."** and **"Some cached videos couldn't be removed"**. Pending HTTP
subscriptions and owner cache status use the first; degraded owner status uses
the second. **"Not enough disk space"** remains the capacity/disk-full error.

False-refusal residual: a stale moov inside a free box left by in-place
QuickTime editing is refused, by design. Marker-like bytes in non-mdat metadata
can also conservatively refuse a file; encoded mdat bytes remain opaque. The
64 MiB metadata bound is a refusal, never a permission to skip unscanned metadata.

All existing assertions outside F26 were retained. F27, F28, F29 and P33 are
additive. Tests call the production functions used by the viewer/plugin; they do
not substitute a separate parser or admission implementation. **No TUT binary
was built or executed**, consistent with the no-build instruction. Passing
syntax checks are not a claim of in-world playback, native-picker, audio or
filesystem fault-injection acceptance.

Verification evidence for this round:

- Explicit `zs_check.ps1 -Files` on all five changed C++ translation units:
  `.tmp/media_slice2b/zs_round3.log`; final policy-test follow-up:
  `.tmp/media_slice2b/zs_test_round3_final.log`.
- The stock mapper skips the nested LibVLC project. Its actual generated Release
  project flags, using the unchanged checker parser with `cl /Zs`, pass in
  `.tmp/media_slice2b/zs_plugin_round3.log` (exit 0).
- Generated Director source references were refreshed with the existing
  `director_page_sources.py` generator because C++ line numbers moved.
- Final `prebuild_guard.ps1 -ChecksOnly` evidence:
  `.tmp/media_slice2b/prebuild_round3_final.log`. The first attempt encountered
  Git line-ending warnings under PowerShell's error policy; the two edited text
  files were normalized to repository LF and the unchanged guard was rerun.
  No checker, flag, threshold, assertion baseline or gate was weakened.


Final result: **PASS**. All five requested per-file invocations completed
successfully (four mapped TUs plus the mapper-skipped nested plugin); the
additional actual-project LibVLC syntax check passed. The final full guard
exited **0**, with every gate OK and its ChecksOnly/no-purge completion line.
`git diff --check` also passed. No C++ source changed after its final check.

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


## Fix round 4

Addresses the round-4 Codex P1/compatibility and Opus P2 findings against
specification v8.1. This section supersedes round 3's bare-FourCC matching,
permanent Browse failure overlay, and startup-only degraded-Clear cleanup.
The tree remains uncommitted; no build, commit or push ran, no secret was
stored, and Director UI work remains deferred.

| Finding | Production change | Production-path regression |
| --- | --- | --- |
| Fragment index entry points | `alvcammediafilecontainer.h::checkOffsets` validates the physical EOF mfro probe after the full root walk. Its destination must exactly match a scanned root mfra. `fragmentIndex` checks every sidx segment start and tfra moof offset against scanned root moof boundaries, with bounded fields/counts and checked arithmetic. Hierarchical sidx and plausible index headers in opaque structures that were not structurally validated refuse. Fragmented zero-size/largesize mdat refuses because VLC's post-mdat parser position uses only its 32-bit size. Encoded mdat stays opaque except for the fixed 16-byte EOF probe. | F30 calls the real `checkContainer`: actual FFmpeg frag_keyframe MP4 with correct mfra/tfra/mfro; the same file without mfra; valid sidx; valid real moov/mvex plus sidx and mfro redirecting into mdat at hidden moov/cmov; wrong type/interior/underflow/overflow destinations; tfra redirects; hierarchical sidx; ambiguous zero-size mdat. The offset fixtures replace the title with equally sized ordinary text so the old text false-refusal cannot mask their missing offset checks. |
| Text compatibility | The rolling scan retains eight bytes, including across 64 KiB reads. A FourCC counts only if its preceding big-endian size is 0, 1, or 8 through the bytes remaining to the enclosing ROOT end. The same rule covers forbidden references/compressed movies, hidden mvhd/moov/trak and padding dref. Root metadata over 64 MiB still refuses. | F31 calls `checkContainer` with udta title, ISO/QT meta, XMP, uuid, free and unknown-root text containing formdata/trakt/moovie/cmovie. Real rmra size 8 and sizes 0/1 still refuse. The shared complete FFmpeg fixture has title=formdata; F31 checks admission with that title. F27 retains every refusal assertion with plausible size headers in the unaligned/crossing-marker fixtures; its short-tail reference now has a preceding zero-size field from the udta terminator. The oversized-payload fixture now has a separate 16-byte free suffix for the EOF probe, retaining the assertion of zero reads inside the oversized payload. No assertion was removed or relaxed. |
| Browse failure sticks | `BrowseOverlay` restores the committed slot's underlying reason before reconciliation, then shows a failure for at most 5 seconds. A change in its own reason, health, output or activity dismisses it earlier. Pending checking remains visible. Starting another Browse or stopping restores the underlying reason before discarding overlay state. No source descriptor is changed on failure. | F32 drives this exact production lifecycle helper through pending, failure, 4999/5000 ms, reason change, enum-state-only change and subsequent frames; it verifies the failure cannot become the stored underlying status or reappear. P33's face-preservation assertions remain unchanged. |
| Degraded Clear loses capacity accounting | Clear drops lookup hits into retained eviction records, preserving leased and undeletable sizes. `cacheUsage` charges live records, evicted records and known orphan files; failed/partial enumeration makes capacity unknown and refuses reservations. Orphan symlink/nonregular targets are not followed for sizing; unknown sizes are charged as full capacity until removal. Transfer leftovers retain the full reservation before it is released, covering partial writes that fail before progress advances; successful cleanup drops that charge. Later maintenance inventories disk and retries at most 128 orphan unlinks per 60-second pass, rotating the cursor so persistent failures cannot starve later files. Only successful unlink or a complete inventory proving absence removes the charge. | F33 drives production `maintainCache`, `dropCacheHits`, `cacheUsage`, `cacheFits` and `CacheOrphans::retry`: three failures release the hit gate while retaining 90 bytes, an 11-byte reservation exceeds a 100-byte limit while 10 fits, failed unlink retains bytes, retry is suppressed before 60 seconds and succeeds at the boundary, unknown inventory refuses reservations, and three bounded passes visit all 300 persistent orphans, and a partial write with a lagging zero-byte counter retains its 90-byte reservation. F25/F29 remain unchanged. |

VLC audit uses the exact upstream **3.0.23** source (physical file lines, not
web renderer line numbers). Every `vlc_stream_Seek` in mp4.c and `MP4_Seek`
call in libmp4.c was reviewed, together with root/chunk loading and UUID dispatch.
The source copies used for this review are under `.tmp/media_slice2b_fix4/`.

| Entry point | VLC 3.0.23 source line | Disposition / proof |
| --- | --- | --- |
| EOF mfro -> arbitrary root parsing until mfra | [mp4.c:4119](https://github.com/videolan/vlc/blob/3.0.23/modules/demux/mp4/mp4.c#L4119), seek at 4146, parse at 4150 | EOF size/type match requires `filesize - mfro_size` to equal a previously scanned root mfra start; no mdat interior, wrong root type, underflow or approximate boundary. |
| sidx first_offset + end-of-sidx + cumulative referenced_size | [mp4.c:4869](https://github.com/videolan/vlc/blob/3.0.23/modules/demux/mp4/mp4.c#L4869), calculation at 4880; seek at 1844, loader at 1852 | Every direct-reference segment start must equal a scanned root moof; hierarchical references refuse. Checked additions prevent wraparound. |
| mfra/tfra moof offsets, then parsing the selected moof | [mp4.c:4899](https://github.com/videolan/vlc/blob/3.0.23/modules/demux/mp4/mp4.c#L4899), offset read at 4919; [FragSeekLoadFragment:1672](https://github.com/videolan/vlc/blob/3.0.23/modules/demux/mp4/mp4.c#L1672), chunk parse at 1690 | The walker descends mfra and validates every tfra entry (versions 0 and 1, all variable field widths). Every offset must be a scanned root moof; the EOF entry into mfra is separately checked. |
| Probed fragment index and initial moov reuse | [mp4.c:4256](https://github.com/videolan/vlc/blob/3.0.23/modules/demux/mp4/mp4.c#L4256), parse at 4270, index positions at 4325; [ProbeFragmentsChecked:4364](https://github.com/videolan/vlc/blob/3.0.23/modules/demux/mp4/mp4.c#L4364), seek at 4392; initial seek at 1105 | These positions come from parsed root positions / root end, not independent numeric index payload. Root boundaries and sizes are walked and metadata-scanned. The moov case of FragSeekLoadFragment reuses the already parsed movie. |
| Sequential fragment parsing after mdat | [mp4.c:5018](https://github.com/videolan/vlc/blob/3.0.23/modules/demux/mp4/mp4.c#L5018), parse at 5022, post-mdat seek at 5118 | Ordinary 32-bit mdat end equals the next walked root boundary (or EOF). Fragmented size-zero and largesize mdat refuse because this VLC path calculates the end using GetDWBE alone. |
| stco/co64 chunk offsets and tfhd/trun data offsets | [libmp4.c:3168](https://github.com/videolan/vlc/blob/3.0.23/modules/demux/mp4/libmp4.c#L3168); [mp4.c:2451](https://github.com/videolan/vlc/blob/3.0.23/modules/demux/mp4/mp4.c#L2451), sample seek/read at 1359/1372; chapter read at 2332; trun sample seek at 4438 | These seek to sample bytes, but LoadChapterApple (mp4.c:2332-2336) leaves the stream at chapter sample data. DemuxFrag can parse boxes there at 5022 before the post-mdat restore at 5118. The gate therefore structurally detects moov/trak/tref/chap and refuses it with the existing not-a-video verdict when moov/mvex or a root moof indicates fragmentation. Non-fragmented chapter files remain allowed; no root-box requirement is imposed on ordinary sample offsets. |
| UUID PIFF tfrf/tfxd (and XML360/PS3 dispatch) | [libmp4.c:4442](https://github.com/videolan/vlc/blob/3.0.23/modules/demux/mp4/libmp4.c#L4442); tfrf fields at 793-819, tfxd at 757 | UUID dispatch reads timing/duration or bounded payload bytes. tfrf contains absolute TIME, not absolute file offsets, and neither handler seeks or parses boxes at those numbers. No extra offset entry exists here. |
| Other libmp4 seeks / sample-description substreams | [libmp4.c:396](https://github.com/videolan/vlc/blob/3.0.23/modules/demux/mp4/libmp4.c#L396), 490/553/1552/2862/2891/4941/5109; memory substreams at 521, 2749, 2837 | These use the current box's bounded start/header/end or reset to file start. Sample-description extensions parse inside copied box bytes, not at attacker-selected absolute file positions. They are covered by the enclosing root byte scan; a plausible sidx/tfra there refuses unless the gate actually walked and validated it. Free/foov/cmov redirection remains covered by plausible-header refusals. |

The old implementation has no EOF/index destination check and admits the
ordinary-title hidden-mdat offset fixture; it rejects title=formdata before
parsing. Its Browse status has no expiry, and its degraded fallback clears all
size-bearing records and skips subsequent orphan sweeps. The new tests assert
the opposite behavior through the production entry points. These are regression
expectations based on the old production paths, **not an executed red/green
claim**: under the no-build constraint, no TUT executable was built or run.
Native picker/VLC playback and real filesystem fault injection remain runtime
acceptance work. The FFmpeg fixture was generated from a local synthetic color
source, with no network or private input.

Final round-4 verification: **PASS**. The final per-file syntax checks and full
ChecksOnly guard exited 0, with no C++ source change afterward. All six checked
source/fixture hashes match `.tmp/media_slice2b_fix4/final_source_hashes.json`.
The existing page-source generator refreshed its line references; no checker,
flag, assertion threshold or baseline was weakened.

| Required check | Result / evidence |
| --- | --- |
| `zs_check.ps1 -Files` for `alvcammediafileio.cpp`, `llprismlens.cpp`, and `tests/alvcammediafilepolicy_test.cpp` | PASS, 3/3 mapped TUs; `.tmp/media_slice2b_fix4/zs_final.log`. |
| `prebuild_guard.ps1 -ChecksOnly` | PASS, every gate, exit 0; `.tmp/media_slice2b_fix4/prebuild_final.log`. |
| `git diff --check` and frozen-source hashes | PASS; `.tmp/media_slice2b_fix4/diff_check.log` and `final_source_hashes.json`. |
| Synthetic FFmpeg fixture inspection | ffprobe confirms MPEG-4 video, 16x16, title=formdata; `.tmp/media_slice2b_fix4/fixture_probe.json`. This is fixture inspection, not VLC playback acceptance. |

Final guard output:

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

Fix round 5: The structural BMFF walk now recognizes moov/trak/tref/chap and
refuses chapter references in fragmented files with the existing not-a-video
verdict. This covers LoadChapterApple leaving the stream at sample bytes before
DemuxFrag's first box parse. F34 drives checkContainer for mvex, earlier/later
root moof, admitted non-fragmented chapters, the admitted existing frag_keyframe
fixture, opaque chapter-like bytes and other track references. Existing assertions
remain unchanged. Verification logs are in `.tmp/media_slice2b_fix5/`; no build,
commit or push was performed. The TUT cases were syntax/semantic checked, not run.

Round-5 verification: **PASS**. `zs_check.ps1 -Files` passed for the test TU
(`zs_final.log`), and `prebuild_guard.ps1 -ChecksOnly` passed every gate, exit 0
(`prebuild_final_pass.log`). Both used process-only `-ExecutionPolicy Bypass`;
the guard also used a process-only readable empty `core.excludesFile` because the
sandbox cannot read the user-level Git ignore file. No checker or threshold was
changed. `git diff --check` passed.
