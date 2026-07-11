# BDMerge — Full Inventory RAM Residency Brief

Investigation into keeping the **entire inventory resident in RAM** for the Alchemy-Machinima
fork on the 192GB / 9950X / RTX 5090 target, so inventory browsing/search is instant with no
on-demand disk or server fetching during a session.

**Status: READ-ONLY investigation.** No source edited, nothing built or committed. This file is
the only artifact.

---

## TL;DR

- The viewer **already holds all loaded inventory in RAM** (`LLInventoryModel::mItemMap` /
  `mCategoryMap` / parent-child trees) for the whole session. Nothing is evicted or paged out.
- The viewer **already kicks off a recursive whole-inventory background fetch at login**
  (`LLInventoryModelBackgroundFetch::instance().start()` with the null root → root folder +
  library, recursive). So "load everything into RAM up front" is essentially the *default*
  behavior — it is just **throttled** so it doesn't hammer AIS.
- The **real latency** a user feels is opening a folder *before the background sweep has reached
  it* (or a folder whose cached version was invalidated). That folder is `VERSION_UNKNOWN`, so
  opening it schedules a fresh AIS fetch and the UI waits on the network round-trip.
- **Disk is touched only twice per session in the normal path:** once on login (gunzip + parse
  the per-account `.inv.gz` cache into RAM) and once on logout/periodic (serialize RAM → `.inv.gz`).
  There is **no lazy disk re-read** during a session — once in RAM, reads are pure memory.
- Therefore the useful lever is **not** "keep it in RAM" (already true) but **"finish the
  up-front fetch faster / guarantee it completes before the user browses, without getting
  AIS-throttled or disconnected."**
- **Honest limit:** the *first-ever* login for an account (empty cache) still has to pull the
  whole tree from the AIS server, which is inherently network-bound and rate-limited server-side.
  Subsequent logins load from the local `.inv.gz` cache and are fast; only version-mismatched
  folders re-fetch.

---

## 1. What is already RAM-resident

`LLInventoryModel` (the global `gInventory`) holds the complete loaded inventory in memory for
the lifetime of the session. Key structures (declared in `indra/newview/llinventorymodel.h`,
constructed at `llinventorymodel.cpp:443-446`):

- `mItemMap` (`cat_map_t`/`item_map_t` = `std::map<LLUUID, LLPointer<LLViewerInventoryItem>>`) —
  every item, keyed by UUID. `getItem()` at `llinventorymodel.cpp:640`.
- `mCategoryMap` — every folder/category, keyed by UUID. `getCategory()` at `:654`.
- `mParentChildCategoryTree` / `mParentChildItemTree` — per-parent arrays giving O(1) "children
  of folder X" lookups (`getDirectDescendentsOf()` at `:681`). Rebuilt wholesale by
  `buildParentChildMap()` at `:3102`.
- `mBacklinkMMap` — link/back-link index.

These are plain in-process heap containers held by `LLPointer` (ref-counted). **Once an item or
category is added it stays resident** until explicitly removed (`removeItem`/`removeCategory` at
`:2147-2195`) or the maps are cleared at shutdown (`:2665-2676`). There is no LRU, no cache
eviction, no memory-pressure trimming. Browsing/searching an already-loaded folder is a pure
in-RAM map/tree walk — **zero disk, zero network.**

**Footprint characteristics:** each item/category is a `LLViewerInventoryItem`/`Category` object
(name string, several UUIDs, permissions, sale info, timestamps, asset/type enums). Order of a
few hundred bytes to ~1KB each including map overhead. Even a very large inventory (say 200k
items) is on the order of tens–low hundreds of MB — **utterly trivial against 192GB.** RAM is
not, and never will be, the binding constraint here.

**Conclusion:** the "keep it in RAM" half of the goal is already satisfied by design. The only
question is getting *everything* into those maps promptly.

---

## 2. The disk / cache path (login parse + logout write)

Inventory is cached on disk as a gzipped LLSD file, **per account, per grid**:

- Path built by `getInvCacheAddres()` at `llinventorymodel.cpp:2448` →
  `<cache>/<owner_uuid>.inv` (+ grid suffix off production), and the on-disk file is that name
  plus `.gz`.
- **Write (RAM → disk):** `LLInventoryModel::cache()` at `:2470` collects cacheable descendents
  (`collectDescendentsIf` with `LLCanCache`), `saveToFile()` to a temp file (`:2493`), then
  `gzip_file()` to `<uuid>.inv.gz` (`:2496`). This runs at logout / cache points — **not during
  normal browsing.**
- **Read (disk → RAM), login only:** `LLInventoryModel::loadSkeleton()` at `:2799`. The server
  sends a lightweight *skeleton* (folder ids + names + parents + **version numbers**, no
  contents). loadSkeleton then:
  1. `gunzip_file()` the `.inv.gz` into a temp file (`:2861-2889`).
  2. `loadFromFile()` parses categories+items into RAM (`:2891`).
  3. **Version reconciliation** (`:2900-2937`): for each cached category, if the cached version
     **matches** the skeleton version, the folder is trusted and its contents are kept in RAM
     (`cached_ids.insert`, `:2930`). If the version **differs**, the folder is set to
     `VERSION_UNKNOWN` (`:2926`) → it will be re-fetched from the server on demand / by the
     background sweep.
  4. Items whose parent cached correctly are added to RAM (`:2962-2994`); broken links handled
     (`:2995-3025`); descendent counts restored (`:3055-3075`).
  5. Temp gunzip file removed (`:3080`); obsolete cache removed (`:3086`).

Then `buildParentChildMap()` (`:3102`, called from `llstartup.cpp:2163`) wires up the trees and
marks inventory usable.

**Key point for the goal:** after login, **disk is not read again on a per-folder basis.** A
folder is either (a) in RAM with a valid version (instant), or (b) `VERSION_UNKNOWN` and pulled
from the *server* (not disk). There is no "lazy disk fetch" to eliminate — the disk hit is a
one-time login parse.

---

## 3. The lazy-fetch path — the real latency source

The latency the user actually experiences is the **server** fetch of a not-yet-fetched folder,
driven by `LLInventoryModelBackgroundFetch` (`llinventorymodelbackgroundfetch.cpp`) over AIS
(AISv3, `llaisapi.cpp`).

### How a folder becomes "slow"
A folder is "incomplete" when its version is `VERSION_UNKNOWN` (`llviewerinventory.cpp`:
`VERSION_UNKNOWN` sentinel; `getVersion()` at `:689`). This happens when:
- the folder is new in the skeleton but absent/stale in cache (loadSkeleton set it UNKNOWN), or
- the cached version didn't match the server skeleton version, or
- it was never cached (first login).

Opening / referencing such a folder calls `LLViewerInventoryCategory::fetch()`
(`llviewerinventory.cpp:699`): if `VERSION_UNKNOWN` and the descendents-request timer has
expired, it calls `LLInventoryModelBackgroundFetch::instance().start(mUUID, false)` (`:719`) and
returns — **the contents are not there yet**; the UI shows a loading/fetching state until the AIS
response arrives. That network round-trip (tens to hundreds of ms, more under load) is the felt
latency.

Entry points that trigger on-demand fetches: `fetchDescendentsOf()`
(`llinventorymodel.cpp:2419` → `cat->fetch()`), and dozens of `...BackgroundFetch::start(id,...)`
call sites (filter/search `llinventoryfilter.cpp:210`, gallery `llinventorygallery.cpp:2956`,
bridge `llinventorybridge.cpp:1306/3954`, texture picker `lltexturectrl.cpp`, etc.).

### The fetch engine and its throttles
`backgroundFetch()` (`:537`) runs from an idle callback; with AIS available it calls
`bulkFetchViaAis()` (`:737`). Throttles that pace it (and thus delay full residency):

- **`PoolSizeAIS`** (`settings.xml:17194`, default **20**) → `max_concurrent_fetches =
  clamp(PoolSizeAIS-1, 1, 50)` (`:746-749`). Caps in-flight AIS requests; the loop returns early
  once `mFetchCount >= max` (`:751`).
- **Per-idle time budget:** after `STATE_WEARABLES_WAIT`, only **6 ms** per idle tick is spent
  issuing fetches (`:757-761`); 1s before that. Keeps the fetch from stalling the frame but
  spreads a large inventory over many ticks.
- **`BatchSizeAIS3`** (`settings.xml:17183`, default **20**, clamped 1..40) → how many child
  folders per `FetchCategorySubset` request (`:883-884`, `:909`).
- **AIS server depth cap:** `MAX_FOLDER_DEPTH_REQUEST = 50` (`llaisapi.cpp:58`); requests send
  `depth` up to 50 and the server caps further, so deep trees need multiple round-trips
  (`FetchCategoryChildren` at `llaisapi.cpp:447`, depth handling `:465-490`).
- **Failure/back-off:** a 403 "too big" splits the request or lowers depth
  (`llaisapi.cpp:920-945`; folder handler split in `bulkFetch`/`BGFolderHttpHandler::processFailure`
  at `:1495-1562`); `FETCH_FAILED` backs a folder off for 60s
  (`llviewerinventory.cpp:739-743`).
- (Legacy UDP/cap path `bulkFetch()` at `:1053` uses `max_batch_size=10`,
  `max_concurrent_fetches=12`; only used when AIS is unavailable.)

### The up-front recursive sweep already exists
`start(LLUUID::null, true)` (`:283-319`) enqueues the **root folder recursively**
(`FT_FOLDER_AND_CONTENT`, `:305`) **and** the **library root** (`:316`), i.e. a whole-inventory
recursive fetch. It is invoked at login from `llstartup.cpp:2175` and `:2203` (and `:410`), and
again whenever the inventory floater opens (`llpanelmaininventory.cpp:828/924/1132`).
`isEverythingFetched()` (`:247`) latches true once `setAllFoldersFetched()` (`:505`) runs after
the folder queue drains — the sweep is intended to run **once per session**.

So the mechanism to pull everything into RAM is present and automatic. The gap vs. the user's
goal is purely **timing + throttle**: a user can open a folder in the seconds/minutes before the
throttled sweep reaches it, and pay the on-demand round-trip.

---

## 4. Existing "fetch everything" levers

- **Automatic recursive sweep at login** — `start()` (null root, recursive), described above.
  This *is* the "fetch all inventory" feature. No separate menu item is required for it to run;
  `llpanelmaininventory.cpp` also re-issues it and surfaces a Fetching/Complete state
  (`:1035-1043`).
- **`findLostItems()`** (`:497`) — recursive orphans fetch (Lost & Found), not whole-inventory.
- **Marketplace** — fetched after main inventory completes (`:818-838`).
- Tunables already exposed: `PoolSizeAIS`, `BatchSizeAIS3` (both in `settings.xml`). Raising
  these is the lightest-touch way to speed the sweep — but AIS **rate-limits server-side**, so
  cranking them risks 403s/throttling/disconnect (the code even reserves one request slot for
  non-fetch actions, `:748`).

No existing setting *forces the sweep to complete before the UI is interactive*, and none removes
the per-idle time budget. Those are the two things a new lever would add.

---

## 5. Server-side reality (accuracy caveat)

"Always in RAM from disk" is bounded by the fact that inventory ultimately lives on the sim/AIS
server. The chain is **server → local `.inv.gz` cache → RAM**:

- **First login on an account (cold cache):** everything must come from the AIS server. This is
  network-bound and **server-rate-limited** — no client change makes it instant. 192GB of RAM
  does not help fetch speed.
- **Subsequent logins (warm cache):** loadSkeleton loads the whole tree from `.inv.gz` into RAM;
  only folders whose server version changed since last session are re-fetched. This is the fast
  path and is already close to the goal.
- Therefore the honest promise is: *"after the first full sweep completes (once per session), all
  browsing/search is pure-RAM and instant; the first sweep on a cold cache is network-bound and
  must respect AIS limits."*

---

## 6. Proposed gated implementation

Add a debug setting **`BDMergeInventoryFullPreload`** (BOOL, default **false**) plus two optional
companion knobs. All changes are additive and gated; default-off preserves stock behavior.

### 6a. Force the sweep early and let it run to completion
- **Site:** `llstartup.cpp:2175` (right after `buildParentChildMap()` and the existing
  `start()`), or hook `LLInventoryModelBackgroundFetch::setFetchCompletionCallback()`
  (`:527`) to log/notify when done.
- **Action when enabled:** the sweep already starts here; the addition is to (a) ensure it starts
  with the widest safe throttle, and (b) optionally show progress until
  `isEverythingFetched()`. Minimal code — the recursive fetch is already wired.

### 6b. Loosen throttles under a "preload" profile (respect AIS limits)
When `BDMergeInventoryFullPreload` is on, raise the pacing *within safe bounds* rather than
removing limits:
- **`PoolSizeAIS`**: default 20 → a preload value of ~**30-40** (hard-clamped to 50 at
  `llinventorymodelbackgroundfetch.cpp:749`). Do **not** go past the existing clamp; AIS throttles
  and 403s escalate beyond that.
- **`BatchSizeAIS3`**: default 20 → up to **40** (already clamped 1..40 at `:884`).
- **Per-idle time budget** (`:757-761`): raise the post-login `0.006f` (6 ms) to e.g.
  **0.020f-0.030f** *only while the preload sweep is active and the frame can afford it*. This is
  the single most effective change for "finish sooner" without adding network pressure — it
  issues more of the *already-permitted* concurrent requests per tick rather than spreading them
  over more frames. Gate it so it reverts once `isEverythingFetched()` is true.
- Expose these as `BDMergePreloadPoolSizeAIS` / `BDMergePreloadBatchSizeAIS3` /
  `BDMergePreloadIdleMs` so the user can tune on the 9950X/network without recompiling.

### 6c. Keep the AIS back-off/split logic intact (do NOT bypass)
The 403-split and depth-lowering recovery (`llaisapi.cpp:920-945`,
`BGFolderHttpHandler::processFailure` `:1495-1562`) and the 60s `FETCH_FAILED` back-off
(`llviewerinventory.cpp:739-743`) are what keep a big inventory from getting the session
disconnected. **Leave them on.** The preload profile should push *concurrency/pacing*, never
disable rate-limit handling. This is the guard against "hammering AIS."

### What NOT to do
- Do **not** try to pin folders resident or add a second cache layer — RAM residency is already
  permanent (§1).
- Do **not** re-read from disk on demand — there is no such path to add (§2).
- Do **not** raise `PoolSizeAIS` past 50 or remove the concurrency clamp — that invites
  server-side throttling / 403 storms / disconnects.

### Effort estimate
- **Small.** 1 BOOL + ~3 tuning settings in `settings.xml`; ~20-40 lines total across
  `llinventorymodelbackgroundfetch.cpp` (wrap the 3 `LLCachedControl` reads and the idle budget
  in the preload profile) and an optional completion notification in `llstartup.cpp`. No new
  classes, no protocol changes. ~half a day including a login smoke test.

---

## 7. Does Black Dragon have anything relevant?

**No.** `I:\black-dragon\indra\newview\llinventorymodelbackgroundfetch.cpp` is the same upstream
LL implementation (identical `isEverythingFetched` / `scheduleFolderFetch` / `FT_RECURSIVE` /
`BatchSizeAIS3` surface — 18 matching occurrences, same structure). BD offers no inventory-preload
feature to port; the Alchemy fork already carries the same AISv3 background-fetch engine.

---

## 8. Risks & honest limits

- **Server rate-limiting (primary risk).** AIS throttles aggressively; pushing concurrency/batch
  too hard causes 403s, request splits, back-offs, and in the worst case a disconnect during
  login. The gated profile must stay within the existing clamps (Pool ≤ 50, Batch ≤ 40) and keep
  the back-off logic. This is the one thing that can actually make the experience *worse*.
- **Cold-cache first login is unavoidably network-bound.** No amount of RAM changes this; the
  brief/UX copy should say "first login fetches from server; later logins are cache-fast."
- **Very large inventories** still take real wall-clock time to sweep even at max safe pacing
  (thousands of folders × depth-limited round-trips). "Instant" applies *after* the sweep
  completes, not during it. A progress indicator (§6a) sets expectations.
- **Cache invalidation.** Any folder whose server version changed since last session re-fetches
  regardless of preload (loadSkeleton sets it `VERSION_UNKNOWN`, `:2926`). Preload just means the
  sweep will get to it sooner rather than on first open.
- **RAM is a non-issue.** Even pathological inventories are tens-to-hundreds of MB against 192GB;
  no memory-pressure concern.

---

## Appendix — primary code anchors

| Concern | File:line |
|---|---|
| In-RAM item/category maps + trees | `llinventorymodel.cpp:443-446`, `:640`, `:654`, `:681` |
| Rebuild parent-child trees | `llinventorymodel.cpp:3102` |
| Login cache parse (disk→RAM) | `llinventorymodel.cpp:2799` (`loadSkeleton`) |
| Cache file path (per account/grid) | `llinventorymodel.cpp:2448` (`getInvCacheAddres`) |
| Cache write (RAM→disk, `.inv.gz`) | `llinventorymodel.cpp:2470` (`cache`), `saveToFile`/`gzip_file` `:2493-2496` |
| Version reconciliation on load | `llinventorymodel.cpp:2900-2937`, `VERSION_UNKNOWN` `:2926` |
| On-demand folder fetch (latency source) | `llviewerinventory.cpp:699` (`fetch`) → `start(id,false)` `:719` |
| `fetchDescendentsOf` | `llinventorymodel.cpp:2419` |
| Whole-inventory recursive sweep | `llinventorymodelbackgroundfetch.cpp:283-319` (`start` null root) |
| Sweep invoked at login | `llstartup.cpp:2175`, `:2203`, `:410` |
| AIS bulk fetch + throttles | `llinventorymodelbackgroundfetch.cpp:737` (`bulkFetchViaAis`) |
| `PoolSizeAIS` clamp | `llinventorymodelbackgroundfetch.cpp:746-749`; default `settings.xml:17194` |
| `BatchSizeAIS3` clamp | `llinventorymodelbackgroundfetch.cpp:883-884`; default `settings.xml:17183` |
| Per-idle 6ms budget | `llinventorymodelbackgroundfetch.cpp:757-761` |
| `isEverythingFetched` / completion | `llinventorymodelbackgroundfetch.cpp:247`, `:505` (`setAllFoldersFetched`) |
| AIS depth cap / recovery | `llaisapi.cpp:58` (`MAX_FOLDER_DEPTH_REQUEST=50`), `:447`, `:920-945` |
| Fetch back-off (FETCH_FAILED 60s) | `llviewerinventory.cpp:739-743` |
