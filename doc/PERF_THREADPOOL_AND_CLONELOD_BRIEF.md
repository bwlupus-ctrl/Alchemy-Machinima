# Two performance fixes: thread-pool idle wait, and clone LOD lookup

Both come from a static research pass (`doc/HARDWARE_OPTIMIZATION_FINDINGS.md`,
`doc/CLONE_WORKLOAD_AND_PROFILING_FINDINGS.md`). They are independent and touch different files.

**Item 1 is high-risk and needs maximum care** — it changes thread synchronization shared by every
worker pool in the viewer. **Item 2 is low-risk** — a pure data-structure swap.

---

# ITEM 1 — `sleepy_robin`: replace the 1 ms idle poll with a real wait

## Current code (verified)
`indra/llcommon/threadpool.cpp:36-58`, installed on **every** pool thread by
`LL::ThreadPoolBase::run()` via `boost::fibers::use_scheduling_algorithm<sleepy_robin>()`:

```cpp
struct sleepy_robin: public boost::fibers::algo::round_robin
{
    virtual void suspend_until(std::chrono::steady_clock::time_point const&) noexcept
    {
#if LL_WINDOWS
        Sleep(1);            // <-- polls, ignores the deadline entirely
#else
        usleep(1);
#endif
    }
    virtual void notify() noexcept
    {
        // "Since our Sleep() call above will wake up on its own, we need not
        //  take any special action to wake it."   <-- the design flaw, stated
    }
};
```

## Two problems
1. **Idle cost.** Every idle worker wakes 1000×/second forever. With ~33 pool threads and
   `timeBeginPeriod(1)` (`llwindowwin32.cpp:4988`) making 1 ms timers actually fire at 1 ms, that is
   **~33,000 context switches per second while the viewer is idle** — each evicting cache lines the
   main thread wants.
2. **Latency.** Because `notify()` does nothing, **newly posted work waits up to 1 ms before any
   worker notices**. This is a silent tax on the already-optimized texture-cache I/O path.

## THE CRITICAL DESIGN POINT — lost wakeups
The naive rewrite (a `std::mutex` + `std::condition_variable`, `notify()` calls `notify_one()`) has a
**lost-wakeup race**: if `notify()` fires between a fiber becoming ready and the worker entering its
wait, the wakeup is dropped and the work sits until the timeout expires. That produces exactly the
kind of intermittent "this task occasionally took 200 ms longer" behaviour that is miserable to debug.

**Use a Windows auto-reset event.** `SetEvent()` called before any `WaitForSingleObject()` is
*remembered* — the event remains signaled until a wait consumes it — so the race resolves itself with
no lock and no predicate. This is the specific reason to prefer an event over a condition variable
here. (`notify()` is declared `noexcept` and may be called from arbitrary threads, which also argues
against anything that can throw or block.)

If you implement the POSIX branch too, a condvar there **must** carry an explicit
`bool mSignaled` predicate guarded by the mutex to close the same race — an event's memory semantics
are not free on that path.

## Requirements
- One event **per scheduler instance**. `use_scheduling_algorithm` installs a scheduler per thread, so
  the event is naturally per-thread — do NOT make it static/shared.
- `suspend_until(deadline)` must **respect the deadline**: wait for the event OR until the deadline,
  whichever comes first. The current code ignores it entirely. Convert the `steady_clock::time_point`
  to a millisecond timeout, clamped to >= 1 ms, and treat
  `std::chrono::steady_clock::time_point::max()` (the "no deadline" case boost passes) as
  `INFINITE` — or as the backoff ceiling below, which is safer.
- **Backoff:** on a wake with no work, grow the timeout 1 → 2 → 4 → 8 → 16 ms; reset to 1 ms whenever
  the event actually fires. This keeps latency low under load and idle cost near zero when quiet.
  Do not exceed ~16 ms or a pool can feel sluggish to start.
- Clean up the handle in the destructor.
- Must behave correctly for **all** pools: ImageDecode (24 threads), CacheIO (4), MeshLodProcessing,
  General, LLImageGL, plus any created later.

## Gating — required
Put it behind a setting, e.g. `BDMergeThreadPoolWait`:
- `0` = **stock `Sleep(1)` behaviour, byte-identical** (the default until proven)
- `1` = event-based wait with backoff

The scheduler is selected once per thread at `run()`, so read the setting there. This gives a clean
A/B and an instant escape hatch if a pool misbehaves in the field.

## Verification the implementation must satisfy
- **No lost work.** A soak test: post N jobs across all pools from a burst, assert all N complete.
  Repeat with jobs posted while workers are deep in backoff (i.e. after seconds of idle) — that is
  precisely when a lost wakeup would bite.
- **No deadlock on shutdown.** Pools are joined during teardown; a worker parked on an event that is
  never signaled would hang exit. Confirm the existing close/join path (`ThreadPoolBase::close()`)
  still terminates promptly, and signal the event on shutdown if needed.
- **Idle cost actually dropped.** Context switches/sec for the process with the viewer idle, before
  vs after (Process Explorer or `xperf`).
- **Latency improved, not regressed.** The existing `BDMergeTexSpikeLog` cache-read histogram is the
  natural instrument — mean and p95 should not worsen, and may improve.

---

# ITEM 2 — `getClonedSourceLOD`: invert the search into a map

## Current code (verified)
`indra/newview/llghostavatar.cpp:2108`, called from `LLVOVolume::calcLOD()`
(`indra/newview/llvovolume.cpp:1622`) for **every** local-only prim whose LOD is re-evaluated:

```cpp
const LLUUID clone_id = volume->getID();
std::vector<LLUUID> ghost_ids = sPaletteTestHarnessGhostIds;      // heap alloc + copy, EVERY call
for (inst : ALGhostStudio::instance().getInstances())
    if (entity clone) ghost_ids.push_back(inst.mEntityId);
for (ghost_id : ghost_ids) {
    dynamic_cast<LLGhostAvatar*>(gObjectList.findObject(ghost_id));  // per ghost
    for (linkset : ghost->mClonedLinksets) {                          // linear scan
        if (linkset.mRoot == clone_id) ...
        for (i : linkset.mChildren)                                   // linear scan
            if (linkset.mChildren[i] == clone_id) ...
    }
}
```

So: a linear search through **every clone's entire attachment graph** to answer a question about one
prim, plus a vector allocation per call. At 50 clones × ~25 prims ≈ 1250 local prims, a single LOD
sweep costs on the order of **60k hash lookups, 60k dynamic_casts, >1M UUID compares and 1250
allocations**. It re-runs whenever `LLSpatialBridge::updateDistance` fires, gated by
`LLSpatialGroup::changeLOD()`, which returns true immediately on `OBJECT_DIRTY` — near-constant for
animating clones. The bridge's impostor early-out can never help: ghosts are never impostors.

**This is the only super-linear term in the clone path.**

## The fix
The mapping never changes between attach and detach, so build it once instead of searching for it.

- Add a static `boost::unordered_map<LLUUID /*clone prim*/, LLUUID /*source prim*/>` (or the
  codebase's preferred hash map — match local convention).
- Populate it in `cloneAttachmentsFrom` where `ClonedLinkset::mRoot` / `mChildren` /
  `mSourceRoot` / `mSourceChildren` are already being built — every pair is known there.
- Erase entries in `releaseClonedAttachments` (and anywhere else a linkset is torn down, including
  the test-harness path that populates `sPaletteTestHarnessGhostIds`).
- `getClonedSourceLOD` becomes: one hash lookup for the source prim id, then the existing
  `findObject` + `getLOD()` on the source. No vector, no scan.

**Semantics must be identical** — same return value, same `source_lod` out-param, same `false` when
the prim is not a clone or the source is gone. This is purely a lookup-structure change.

## Correctness requirements
- The map must not outlive its entries: a stale entry pointing at a dead source prim must be handled
  exactly as today (resolve fails → return false), and torn-down clones must be erased so the map does
  not grow across a session.
- Must cover **both** producers of cloned prims: the Ghost Studio instances AND the palette-test
  harness (`sPaletteTestHarnessGhostIds`).
- Clone replacement (`refreshEntityClone`) destroys and recreates linksets — verify the map is
  rebuilt, not left pointing at the old runtime's prims.

## Verification
- Behavioural: a cloned linkset with mixed-LOD parts (the original bug was wings rendering low-poly)
  must still match its source's LOD at various distances.
- Performance: with a Tracy zone on `getClonedSourceLOD`, the 1/10/25/50 clone sweep should go from
  **convex to flat**. That shape change is the proof.

---

## Constraints (both items)
- Client-only; nothing to the simulator; never mutate the source avatar.
- Item 1's default must be stock behaviour until proven.
- Item 2 must not change what any prim renders — only how fast the answer is found.
- Do not rename or remove any XUI control name.
- Do not reintroduce local-avatar-scale code (reverted, commit `ec82e1a49f8`).
- Self-review to 0 must-fix. Report: the exact wait/notify handshake used and why it cannot lose a
  wakeup, the shutdown path, the setting name and default, the map's lifetime/ownership story, and
  which call sites populate and erase it.
