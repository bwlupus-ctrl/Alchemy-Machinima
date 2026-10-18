# VCam media cache fix round

2026-10-18. Builds on the uncommitted cache-lifetime feature described in
`MEDIA_CACHE_LIFETIME_REPORT.md`, for the regressions reported against
`2d2358243a1`. No commit, push, viewer build, or viewer launch.

## Cache initialization and recovery

`ownCache()` now obtains a metadata snapshot from `readCacheIndex()` in the new
`alvcammediafilecache.h`. Its local `ifstream` is destroyed before the caller
publishes entries or starts a sweep/index replacement. Metadata is parsed into
temporary records. A malformed, partial, duplicate-key, invalid-extension/MIME,
invalid-number, or zero-size record discards the entire snapshot, including any
valid prefix. Missing metadata rebuilds; a zero-byte index is valid. The normal
owned sweep handles unindexed files using the existing orphan accounting and
deletion bounds.

Stale `index.tmp` is explicitly removed under ownership during initialization
and before every rewrite. Removal affects only that direct directory entry and
does not follow a temporary symlink. The index writer checks close failure as
well as flush failure. The shared replacement helper retains Windows
`MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH` and captures `GetLastError()`
immediately on failure, before cleanup can overwrite it.

Failed lock acquisition now releases its local handles and parent pins. Once
acquired and validated, the lock and locality pins remain process-lifetime
service resources, including across metadata-recovery failures. A retry does
not contend against its own lock. Windows share-none locking, reparse refusals,
parent/root pins, POSIX no-follow/locality rules, leased files, reserved bytes,
active partials, promotion rules, Clear/degraded handling, cache lifetime and
bounded quit draining remain in place. Local source files never enter cleanup.

Every ownership attempt logs an INFO `VCamMedia` outcome with its stage, captured
error code, and a safe short reason on failure. Cache failure is an atomic typed
value; UI snapshots no longer race a mutable worker error string. Public status
is `Video cache unavailable: <short reason>`, with the reason list added to
`SPEC_media_projector.md`. Transfer failures retain their existing statuses.

After ownership fails, each explicit HTTP Play may request one new attempt on
the existing detached maintenance worker. An atomic failed-to-checking transition
coalesces pending requests. The attempt uses the existing five lock attempts over
two seconds and I/O deadlines. There is no automatic ownership retry loop and no
session-wide failure latch: another explicit Play can recover from a later
failure. Main performs no filesystem work or waiting.

## Status and text-row clipboard

Both Source Status update paths use the shared `mediaSourceStatus()` rule.
Current runtime failures win over cached Play/editor errors and empty-draft
prompts. The monitor tile checks the same runtime failure rule before editor
refusals and displays the identical failure text, including HTTP suffixes,
typed cache failures, unsupported players and copy failures. Nonfailure runtime
states retain the existing editor-status behavior.

The row menu is implemented in `ALKitRow::showContextMenu()`. All text-kind or
direct line-editor rows now enable Paste value when OS text is available and
the row's existing edit/type/binding guards permit it. Clicking re-reads the OS
clipboard through `LLClipboard::pasteFromClipboard()` and sends the whole string
through `setBoundValue()` and the ordinary binding/control commit. If clipboard
text disappears, it does not fall back to stale value-clipboard data. Text-row
Copy value also writes OS text, preserving copy/paste round trips. Numeric and
combo rows keep the existing typed value clipboard and coercion/range checks.

The Video URL commit's shared `commitVideoDraft()` seam validates before calling
source replacement. Invalid pasted text stays in the editor with its refusal;
it cannot reach the source-replacement/possible playback-stop boundary. Existing
availability, generation, owner, history and source admission checks still run.

## Validation

Only the three authorized integration targets and their library dependencies
were built. Final results:

| Check | Result |
| --- | --- |
| `INTEGRATION_TEST_alvcammediafilepolicy` | 52/52 passed |
| `INTEGRATION_TEST_llprismprojector` | 41/41 passed |
| `INTEGRATION_TEST_alposepolish` | 23/23 passed |
| Explicit `zs_check` | 7 TUs passed, 0 failures |
| `prebuild_guard.ps1 -ChecksOnly` | All gates passed; exited before object purge |
| Changed XUI layout check | 40 files, 0 findings, 0 failed files |
| `git diff --check` | Passed |

New media tests 48-52 cover held-reader Windows replacement, closed-reader
sweep/replacement, corrupt/partial/empty/missing metadata, stale temporary
cleanup, failed-replacement recovery on Play, pending retry coalescence, later
Play recovery without a session latch, runtime/editor failure precedence and
text versus numeric/combo clipboard routing. The invalid-text test uses the same
draft-validation seam as the panel and asserts that source replacement is not
called. The TUT group explicitly admits all 52 cases rather than silently
stopping at its default limit of 50.

The Windows negative control produced:

```text
Held MSVC index reader replacement: Win32 error=5
Total Tests: 52
Passed Tests: 52
```

Error 5 is `ERROR_ACCESS_DENIED`; Windows refused replacement while the MSVC
reader was open. Replacement succeeded after the production reader helper
returned. The fixture accepts both access denied and sharing violation as the
native held-reader refusal. Its miniature orphan sweep uses disposable test
files; it shares production metadata parsing, temporary cleanup and native index
replacement, without opening the viewer cache.

Explicit syntax checks cover every C++ file changed in this round:
`alvcammediafileio.cpp`, `alpaneldirectorvcam.cpp`, `alkitrow.cpp`, and
`tests/alvcammediafilepolicy_test.cpp`. Additional header consumers are
`alvcammediafilepolicy.cpp`, `aluiperfselftest.cpp`, and `alkitcard.cpp`.
Header coverage: cache header through I/O plus policy tests; policy header
additionally through policy.cpp; lifecycle header additionally through selftest;
QOL header additionally through card.cpp.

Logs are `.tmp/cache_fix_test_build*.log`,
`.tmp/cache_fix_test_<suite>.log`, `.tmp/cache_fix_zs_final.log`,
`.tmp/cache_fix_prebuild_final.log`, and `.tmp/cache_fix_layout.json`.
The checks-only wrapper uses workspace-local temporary files, syntax flag cache,
and process-local Git configuration; it does not change any guard or persistent
Git configuration. Initial test/syntax failures were corrected before the final
passing runs.

These are offline Windows integration/policy and syntax checks. Real HTTP
downloads, live OS clipboard/menu interaction, renderer playback, two running
viewers and shutdown close fences were not exercised in-world. The optional
external media-file probe was skipped because `VCAM_MEDIA_PROBE_FILE` was unset.

## Fix round 2

2026-10-18. Addresses all findings in the second Opus/Codex fix request.
No commit, push, viewer build, or viewer launch. Only the three permitted
integration targets and their library dependencies were built.

### Retirement, process exit, and bounded quit work

Executed retirement closures are now destroyed by `executeRetirements()` before
any sweep. This closes their captured validation handles before unlink; the
validation open still uses **FILE_SHARE_READ only**, retaining its deny-delete
pin. Recursive mailbox/lease retirement keeps maintenance alive for another
pass. A release counter also prevents final-pass exit before newly released
cache work has received a deletion attempt.

Each Windows private-file close fence receives an independently duplicated
process handle with SYNCHRONIZE access. Maintenance polls that handle with
`WaitForSingleObject(..., 0)`, so actual child death closes the fence while main
is inside `drainShutdown()`. It neither reads main-thread LLProcess state nor
signals a numeric PID. Pending process creation gets another attachment attempt
from parent idle. A kill request or fence timeout alone still cannot release a
lease. The normal-session 15-second plugin lock-up rule is unchanged.

Quit initiates one sweep, then only a newly released lease/entry forces another
immediate inventory/deletion attempt. Other failures retain the normal backoff;
`shutdownClear` no longer resets the retry deadline every 20 ms. Large sweeps
continue in batches of at most 128 previously unattempted names. Failed names
are not retried by those continuations. Thus a cache with more than 128 files
can finish draining without repeatedly hammering its locked files.

Keep ON cancels maintenance immediately. Keep OFF finishes after ordinary-worker
and mailbox handoff, once useful deletion work is exhausted. Undeletable files
do not prolong the drain; cache leases still owned by a live child remain
protected within the existing two-second bound. Local-only player fences have
no cache deletion work and can remain safely retained by the process-lifetime
service without delaying quit. Remaining files fall to the next owner's
startup sweep. Main still does no filesystem I/O or joins.

### Ownership wait, status ordering, and purge persistence

Every download has a separate ten-second ownership-wait deadline, initialized
before its worker launches. This deadline works before transfer start and
outside filesystem operations. Main can terminalize a stalled wait with
`Video cache unavailable: <reason>`; the timeout verdict remains sticky.

Runtime media events and editor validation/refusal events now share a monotonic
sequence. Both Source Status paths and monitor tiles select the newer event.
A current invalid edit/paste or newer successful validation beats an older 404;
a later runtime failure beats a cached editor refusal. Cached draft events
retain their sequence across selection changes and are checked against the
capture generation and committed cue. Invalid drafts still cannot reach source
replacement or stop playback. Runtime transport events publish their revision,
and later playback status replaces the successful-validation prompt.
Monitor refusals and runtime failures restore the mockup's `Error: ` prefix.

Ownership diagnostics preserve the error category supplied by a system_error,
and map error conditions through that category. Generic errno EIO remains cache
I/O failure even though its value equals Win32 ERROR_ACCESS_DENIED. Both success
and failure ownership logs include the `os_error` category.

`VCamMediaCachePurgePending` is a separate persisted Boolean, default false.
`initCache()` sets it before consuming the viewer's general one-shot purge flag.
A media non-owner, failed inventory, degraded Clear, or remaining file cannot
acknowledge it. Only a successful owner sweep of the entire media cache posts
the one-shot acknowledgement, applied on main through a setting control
resolved during initialization. The request therefore survives a process that
does not win the media lock. Source-reference artifacts were regenerated with
`director_page_sources.py`; check thresholds and baselines were not changed.

### Validation

| Check | Result |
| --- | --- |
| `INTEGRATION_TEST_alvcammediafilepolicy` | 58/58 passed, exit 0 |
| `INTEGRATION_TEST_llprismprojector` | 41/41 passed, exit 0 |
| `INTEGRATION_TEST_alposepolish` | 23/23 passed, exit 0 |
| Explicit `zs_check` | 10 TUs passed, 0 failures |
| `prebuild_guard.ps1 -ChecksOnly` | All gates passed; no object purge |
| Changed XUI layout check | 40 files, 0 findings, 0 failed files |
| `git diff --check` | Passed |

New cases 53-58 exercise the Windows deny-delete negative control and actual
post-retirement unlink, recursive retirement/final-pass ordering, an actual
disposable child and duplicated handle observed from a worker without main
event pumping, Keep ON/Keep OFF drain decisions, local-only fences, backoff,
300 deletable files plus a file attempted only once, the independent ownership
deadline, 404 followed by invalid typing without source replacement, subsequent
runtime precedence, errno/Win32 category separation (including both disk-full
codes), and owner-only successful-purge acknowledgement.

The explicit syntax run covers `alvcammediafileio.cpp`,
`alpaneldirectorvcam.cpp`, `llprismlens.cpp`, `llappviewer.cpp`,
`llpluginprocessparent.cpp`, and `tests/alvcammediafilepolicy_test.cpp`.
Additional header consumers are `alvcammediafilepolicy.cpp` (policy),
`aluiperfselftest.cpp` (lifecycle and panel), `llviewerdisplay.cpp` (lens), and
`llpluginclassmedia.cpp` (parent and close-fence headers).

Final logs: `.tmp/cache_fix_r2_test_build.log`,
`.tmp/cache_fix_r2_test_<suite>.log`, `.tmp/cache_fix_r2_zs.log`,
`.tmp/cache_fix_r2_prebuild.log`, `.tmp/cache_fix_r2_layout.json`, and
`.tmp/cache_fix_r2_sources.log`. Initial warning-as-error and stale generated
source-reference findings were corrected before the final passing checks.
These are offline Windows checks; renderer playback and two running viewers
were not launched. The optional external media fixture remains unset/skipped.

## Fix round 3

2026-10-18. Addresses N1-N5 and the Codex Keep ON drain finding in the
uncommitted tree. No commit, push, viewer build, or viewer launch.

- **N1:** `cacheFailureFromError()` preserves a semantic `IN_USE` refusal before
  considering native error mappings, including MSVC's permission mapping for
  sharing violation 32. `cacheFailureStatus(IN_USE)` returns the spec's exact
  **Video downloads are in use by another open viewer**. Actual access and
  disk-full failures retain their separate categories.
- **N2:** failed user/startup Clear sweeps schedule the next inventory/deletion
  attempt after 250 ms. Three immediately failing attempts therefore reach the
  existing degraded fallback in roughly 500 ms. Once resolved/degraded, ordinary
  60-second orphan backoff resumes. Quit retains its bounded batch/attempt-set
  behavior. HTTP's Clear wait has a separate ten-second deadline across mutex
  races, checked both by `awaitCacheClear()` and main's subscription polling.
  A blocked worker terminalizes with **Video cache clear timed out**; that
  verdict remains sticky and late completion cannot publish playback.
- **N3:** the production validation guard, `CacheValidationPin`, increments the
  release counter when cancellation/failure unwinds without lease adoption.
  Maintenance therefore observes the newly unleased entry and performs its
  final deletion pass. Adoption still leaves the lease pinned until retirement.
- **N4:** a completed degraded owner purge acknowledges the persisted media
  purge marker. Known undeletable children are retained in orphan accounting and
  logged; retained leases and unknown inventory remain conservative. Non-owners
  and unresolved Clear cannot acknowledge. With Keep ON, the next launch keeps
  valid cache entries rather than repeating the entire purge for one leftover.
- **N5:** keystroke validation updates the Source editor's draft/error state and
  sequence without publishing a monitor refusal. Enter, focus loss, Paste and
  Browse commit paths publish refusal/success records. Monitor tiles retain
  committed editor versus runtime event ordering, generation/cue checks and the
  `Error: ` prefix. Invalid text still cannot replace a source or stop playback.
- **Codex:** the production `beginShutdown()`/`drainShutdown()` path captures
  Keep in `ShutdownDrain`. Keep ON returns without polling workers, sleeping or
  waiting for the two-second deadline. Keep OFF retains the single bounded drain.
  `jobsRemain()`, Windows native-thread/TLS reaping, retained transfer leases,
  and the existing post-persistence `_exit` rule remain intact.

No delete sharing was added. Share-none ownership, deny-delete validation pins,
locality/reparse checks, owner-only mutation, lease protection, byte reservations,
partial promotion and cancellation rules remain in place. The spec's former
lock-status override was corrected and generated page-source evidence refreshed;
no checker thresholds or baselines changed.

### Validation

| Check | Result |
| --- | --- |
| `INTEGRATION_TEST_alvcammediafilepolicy` | 62/62 passed, exit 0 |
| `INTEGRATION_TEST_llprismprojector` | 41/41 passed, exit 0 |
| `INTEGRATION_TEST_alposepolish` | 23/23 passed, exit 0 |
| Explicit `zs_check` | 5 TUs passed, 0 failures |
| `prebuild_guard.ps1 -ChecksOnly` | All gates passed; no object purge |
| Changed XUI layout check | 40 files, 0 findings, 0 failed files |
| `git diff --check` | Passed |

Cases 59-62 add LOCKED plus native code 32/exact-status coverage, simulated-clock
Clear retries after one transient failure and three persistent failures, a hung
Clear deadline, cancelled/adopted production validation guards, degraded purge
acknowledgement and Keep ON next-start behavior. The shutdown regression executes
the **same begin/drain implementation called by production**, with active counts
representing maintenance, preflight/download and blocked filesystem workers. It
asserts zero ticks, sleeps and clock/deadline polls for Keep ON, preserves active
jobs for the forced-exit safeguard, and verifies Keep OFF's two-second bound.
It is not limited to the earlier `finishShutdownMaintenance()` Boolean helper.
Existing case 57 covers committed invalid input and subsequent runtime ordering;
the panel's keystroke path was inspected to verify it writes no monitor record.

Explicit syntax checks cover every C++ file touched in this round:
`alvcammediafileio.cpp`, `alpaneldirectorvcam.cpp`, and
`tests/alvcammediafilepolicy_test.cpp`. Additional consumers are
`alvcammediafilepolicy.cpp` for the policy header and `aluiperfselftest.cpp` for
the lifecycle header. The checks-only guard also checks the wider uncommitted
tree. Only the three permitted integration targets and dependencies were built.

Logs: `.tmp/cache_fix_r3_test_build.log`, `.tmp/cache_fix_r3_test_<suite>.log`,
`.tmp/cache_fix_r3_zs.log`, `.tmp/cache_fix_r3_prebuild.log`,
`.tmp/cache_fix_r3_layout.json`, and `.tmp/cache_fix_r3_sources.log`.
These are offline Windows tests and syntax/layout checks; live HTTP, two running
viewers, actual quit and interactive typing/Browse were not exercised. The
optional external media fixture remains unset/skipped.

## Fix round 4

2026-10-18. Addresses the Codex P1 incomplete sweep acknowledgement and the
Opus degraded-log finding in the uncommitted round 3 tree. No commit, push,
viewer build, or viewer launch.

- **Complete coverage before acknowledgement:** `clearPending` now represents
  Clear completion independently of `clearDegraded`, which allows Play to
  continue. Neither Clear completion nor the persisted purge marker can be
  acknowledged until inventory is complete, every remaining child has had a
  real deletion attempt, and retained leases have released. Failed children
  require three actual failed unlink attempts each before degraded completion;
  an unattempted child earns no strikes. Complete inventory publication preserves
  only strikes for children still present. A new Clear resets that evidence.
- **Incomplete and failed inventory:** partial/failed enumeration keeps inventory
  unknown and cannot acknowledge Clear or purge, including after degradation.
  Outstanding coverage continues in bounded batches of at most 128 children at
  the 250 ms cadence. A batch with only successful deletions and untouched
  children does not count as an unlink failure. Degraded continuation preserves
  new cache hits created by later Play requests. The independent ten-second
  Play wait deadline remains, and Play can bypass pending degraded Clear.
  After completion, ordinary orphan retries return to the 60-second backoff.
  Quit keeps its existing attempt-set batches and bounded drain; its sweep still
  clears cache hits even if an earlier user Clear had already degraded.
- **Bounded private diagnostics:** the degraded warning is one summary line with
  separate failed/unattempted/leased counts and inventory-unknown state, followed
  on that same line by at most three indexed examples (state and byte count).
  The formatter never emits paths, filenames, or SHA-256 cache keys.

Owner-only mutation, share-none ownership, deny-delete validation pins, lease
protection, conservative unknown-size accounting, locality/reparse checks,
reservation/promotion rules, and shutdown bounds remain in place. Existing
abstract Clear fixtures now explicitly supply their coverage assumption;
production and new regressions use the actual orphan coverage predicate.
Generated page-source references were refreshed for moved C++ lines.

### Validation

| Check | Result |
| --- | --- |
| `INTEGRATION_TEST_alvcammediafilepolicy` | 65/65 passed, exit 0 |
| `INTEGRATION_TEST_llprismprojector` | 41/41 passed, exit 0 |
| `INTEGRATION_TEST_alposepolish` | 23/23 passed, exit 0 |
| Explicit `zs_check` | 4 TUs passed, 0 failures |
| `prebuild_guard.ps1 -ChecksOnly` | All gates passed; no object purge |
| `git diff --check` | Passed |

Cases 63-65 exercise 600 deletable children with and without an additional
undeletable child, three-pass non-acknowledgement, per-child strike coverage,
continued fast retries, degraded Play before complete attempt coverage, five
inventory failures without acknowledgement, recovery with a retained lease,
preservation of later hits, and the capped formatter with 1,000 keyed children
plus empty/single-child inventories. The registered test count was raised to 65.
The inventory/removal callbacks in these cases are deterministic in-memory
fixtures using the production maintenance, orphan retry, coverage, Play-wait,
acknowledgement and log helpers. Existing real-file and Windows lease tests also
pass; no live HTTP or interactive viewer workflow was exercised. The optional
external media fixture remains unset/skipped.

Explicit syntax checks cover all C++ files touched in this round:
`alvcammediafileio.cpp`, `tests/alvcammediafilepolicy_test.cpp`, and
`aluiperfselftest.cpp`. The additional lifecycle-header consumer is
`alpaneldirectorvcam.cpp`. The checks-only guard checks the wider uncommitted
tree. Its initial run hit an unreadable user-global Git ignore file; the final
run used a temporary empty ignore file inside the workspace and disabled Git's
line-ending warnings for that process. No repository/global Git configuration,
checker threshold, or baseline was changed.

Logs: `.tmp/cache_fix_r4_test_build.log`, `.tmp/cache_fix_r4_test_<suite>.log`,
`.tmp/cache_fix_r4_zs.log`, `.tmp/cache_fix_r4_prebuild.log`, and
`.tmp/cache_fix_r4_sources.log`. Only the three permitted integration targets
and their dependencies were built.

## Known P2 carried forward (Opus round-4 note)
`clearFailureRetry` keeps the 250 ms cadence while a Clear is pending, even after degradation. With a persistently failing inventory, or an index that can never be replaced, maintenance repeats an inventory, up to 128 unlink attempts and an index write about 4×/s for the session. This is off-main and light, and Play is already unblocked by degraded mode. Next batch: fall back to the ordinary 60 s backoff once degraded with no coverage progress (test 64 asserts the current cadence and must be updated deliberately).
