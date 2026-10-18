# VCam media download cache lifetime

2026-10-18. Implements the approved workflow in SPEC_media_projector.md v8.3.
Read CLAUDE.md, v8.2's cache/lock/LRU/Clear/startup/disposition/shutdown rules,
the detached cache service, initializeMediaFiles, and the viewer purge paths.
No commit, push, viewer build or viewer launch. Only the three authorized
integration targets and their library dependencies were built.

## Behavior

- `VCamMediaCacheKeepBetweenSessions`: Boolean, Persist 1, default FALSE.
  Off leaves downloads usable throughout the session, then schedules cache
  clearing on the detached maintenance worker at quit. Quit samples the current
  setting, so turning it off while a file is playing does not interrupt/delete it.
  On retains the existing cross-session LRU under `VCamMediaCacheMaxMB`.
- Startup Off clears indexed complete files as well as partial/orphan leftovers.
  Initialization runs even with either VCam enable off. Clear gates cache hits
  and downloads, retaining existing bounded retries and degraded handling.
- `initCache()` captures `PurgeCacheOnNextStartup` / `PurgeCacheOnStartup` before
  the one-shot flag is consumed. It passes that request after selecting the cache
  location. The next startup clears media regardless of Keep. This decision is
  independent of the viewer's read-only/second-instance status: the dedicated
  media lock alone determines permission to touch the cache.
- All deletion stays in the existing non-recursive sweep of direct children of
  `<cache>/vcam_media/v1`. The share-none process-lifetime lock, parent/root
  locality pins, leased-file protection, active transfer filenames, reversible
  FileDispositionInfoEx partial promotion and no-replace rename remain intact.
  Non-owners never read/write the index or sweep. Local files are never copied
  into this cache or deleted.

## Quit ordering and limit

`beginShutdown(currentKeep)` starts the same existing two-second monotonic
budget, stops admission, closes mailboxes and cancels all ordinary jobs,
including previously cancelled jobs. Maintenance remains detached and available
for file retirement and session cleanup. `shutdownMediaFiles()` queues player
close fences before `drainShutdown()` arms main-thread draining.

During draining, `tick()` uses try-locks to move closed ordinary mailbox leases
into worker retirement captures. Only after reaping and erasing completed jobs,
with all mailbox retirements queued and only maintenance remaining, main publishes
the final-pass handoff. This avoids treating a thread-count decrement as proof
that its result lease has been released. Maintenance processes retirement work,
forces a final session-only sweep, and checks queue emptiness after its local
captures have destructed, including recursive lease retirement. A newly closed
lease gets an immediate quit unlink attempt rather than the normal 60-second
orphan retry delay.

The drain loop/budget is unchanged; there is no join or filesystem I/O on main.
The existing `jobsRemain()` forced-process-exit override, including
`BDMergeFastExit=0`, is unchanged. Blocked I/O, remaining leases, locked files,
bounded sweep batches or a drain timeout leave work for the next owner's Off
startup sweep. The cache lock/service survive until process death.

## UI, generation and coverage

Both mockups were edited first and remain byte-identical:

- `.tmp/gate_mockup/project/VCamPage.dc.html`
- `.tmp/VCamPage_media_mockup.dc.html`

The Source card's Video URL mode now shows **Keep downloads between sessions**,
default Off, beside Media cache / Clear video cache. Its exact help is:

> Off: downloaded videos are deleted when you quit. On: kept up to the cache limit for faster replays. Local files are never copied.

The mockup preference is global. The kit JSON emits a native setting-bound
switch; the VCam router leaves bound controls to their setting sink. Both URL
visibility and search-reveal guards include the row. Search keywords cover cache,
downloads, sessions, lifetime, quit and purge.

The new row uses the existing `fill_width` attribute to allocate otherwise empty
toggle track space to its label. Only full-width toggles use a 24-pixel On/Off
track; ordinary row geometry is unchanged. At the minimum 340-pixel card width,
the label gets 192 pixels, enough for the measured 189-pixel label. The layout
checker measures and pins that formula; its existing 100-pixel normal-label gate
remains exact. New passing and oversized failing toggle fixtures prevent a
label-fit exemption.

Regenerated all three generators: `director_kit_gen.py`,
`director_page_sources.py`, `director_functional_manifest.py`. The exact source
row pin is **2122 -> 2123**, with a +1 comment; exact equality and existing
thresholds remain. Other page XML changes are generated source-bundle hashes;
page-source line references and subsequent manifest IDs shift mechanically.
The manifest includes a RUN setting toggle/restore oracle and a pure
`cache_lifetime` model probe. The C++ selftest implements that probe; real
filesystem/cache mutations retain their runtime acceptance classification.

## Validation

Independent adversarial review converged to **0 must-fix**, including the
shutdown mailbox handoff and the minimum-card label followup.

| Check | Result |
| --- | --- |
| INTEGRATION_TEST_alvcammediafilepolicy | 47/47 passed; new pure lifecycle cases 42-47 |
| INTEGRATION_TEST_llprismprojector | 41/41 passed |
| INTEGRATION_TEST_alposepolish | 23/23 passed |
| zs_check, every touched C++ TU plus header consumers | 8/8 passed, 0 failures |
| prebuild_guard.ps1 -ChecksOnly | All gates passed; exited before object purge |
| XUI changed-file check | 0 total findings, 0 failed files |
| XUI selftest | Passed, including new valid/invalid wide-toggle fixtures |
| Kit generator selftest / kit lint | Passed; lint 0 errors, 14 existing warnings |
| Functional manifest rule/generation selftests | Passed, 12 rule fixtures |
| All three generator --check runs | Passed |
| Mockup/binding/default/search/manifest artifact assertions | Passed |
| git diff --check | Passed |

The lifecycle tests cover session-only quit scheduling without inline deletion,
Off startup sweep, On preservation, purge override, non-owner refusal, and
leased-file deferral. Header coverage: `alvcammediafileio.h` through appviewer and
prismlens; `alvcammediafilelifecycle.h` through tests and selftest;
`llprismlens.h` additionally through unchanged `llviewerdisplay.cpp`.

Final raw summaries:

```text
alvcammediafilepolicy exit=0  Total Tests: 47  Passed Tests: 47
llprismprojector exit=0      Total Tests: 41  Passed Tests: 41
alposepolish exit=0          Total Tests: 23  Passed Tests: 23
8 TU(s), 8 PASS, 0 FAIL, 24.1s
ChecksOnly: all gates ran; exiting before the obj purge (no files touched).
```

Logs/runners are `.tmp/cache_lifetime_*`, including `test_build.log`, individual
`test_<suite>.log`, `zs.log`, `prebuild.log`, `layout.json` and
`layout_selftest.log` suffixes. The checks-only wrapper redirects the supported
syntax-flags cache to the workspace and uses a process-local readable empty Git
ignore file; it does not change the guards or persistent Git configuration.

These are offline checks. Real playback/child-process close fences, a locked
disk file, two viewers sharing a cache, crash recovery and held-I/O forced exit
were not exercised in a running viewer. The optional external media fixture
probe reported `VCAM_MEDIA_PROBE_FILE: SKIPPED (not set)`; all 47 policy tests ran.
