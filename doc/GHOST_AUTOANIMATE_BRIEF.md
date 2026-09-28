# Ghost AutoAnimate (learned performance) — design brief (for adversarial review)

**Date:** 2026-09-28
**Status:** DESIGN ONLY. Nothing implemented, nothing built. Per `CLAUDE.md`: Codex implements from
this brief, an Opus sub-agent attacks it, loop to 0 must-fix, THEN build once.
**Ship whole:** engine + settings + Ghost Studio UI + Director reachability + chat command +
preset/reset wiring, in one delivery.

---

## 1. The ask (user, verbatim, condensed)

> "If I remove the source, the animations on the clone can continue to play. Not an AO state
> persona, but a hybrid record / learned AO." — "It should be a mode. Toggle. It would also need to
> be versatile to handle bento attachments and animesh ones."

So:
- A per-clone **toggle** ("AutoAnimate"). While it is on and the clone is in Mirror drive, the
  clone passively **learns** the source's performance.
- When the source goes away (or the user presses **Detach**), the clone keeps performing on its own
  from what it learned — the AO keeps cycling stands, face/hand anims keep firing — with **no
  scripts** involved.
- **Explicitly out of scope:** locomotion-state mapping (walk/run/turn overrides). The user does not
  want an AO state machine. No velocity inference, no Actor Mover integration.
- Must cover **both** animation channels a clone has:
  1. the clone avatar's own body/bento skeleton (drives body, bento head/hands/tails, and every
     rigged attachment worn on it), and
  2. **every cloned animesh linkset** (worn animesh attachments, pets, props), each independently.

---

## 2. What the code PROVES today (file:line)

| Fact | Where |
|---|---|
| Drive modes are `DRIVE_MIRROR=0, DRIVE_DIRECTED, DRIVE_FROZEN` | `alghoststudio.h:100` |
| Mirror copies the source's `mSignaledAnimations` (minus ground-sit) every frame into the clone | `llghostavatar.cpp:1987-2013` |
| Body sync is a **pure ledger diff** — only starts/stops what changed; clone-local, no sim traffic | `synchronizeCloneAnimations`, `llghostavatar.cpp:492-526` |
| Animesh is mirrored per prim by copying source entries of `LLObjectSignaledAnimationMap` onto the clone prim ids, then `updateControlAvatar()` | `llghostavatar.cpp:2050-2125` |
| Source linkset UUIDs change on outfit re-rez; hold logic skips a missing source root | `llghostavatar.cpp:2060-2077` |
| "Hold on source change" keeps the LAST set when the source contributes nothing | `llghostavatar.cpp:1985-2012`, setting `GhostMirrorHoldOnSourceChange` |
| **Leaving MIRROR stops every body animation** (`synchronizeCloneAnimations(empty)`) | `setEntityDriveMode`, `llghostavatar.cpp:548-552` |
| Entering DIRECTED also clears cloned object (animesh) animations | `llghostavatar.cpp:557-561` |
| Drive mode is clamped to `[MIRROR, FROZEN]` | `llghostavatar.cpp:531-532` |
| Pause = FROZEN; Resume returns to `mResumeDriveMode` (with a FROZEN→MIRROR fallback) | `alghoststudio.cpp:392`, `:1157-1166` |
| Keyframe data cache is process-global and is **not evicted** except for local anims (`flushKeyframeCache` body is commented out) | `llkeyframemotion.cpp:2355-2358`, `lllocalanim.cpp:72,132` |
| `LLKeyframeMotion` exposes `getLoop()` / `getDuration()` once loaded | `llkeyframemotion.h:89-96` |
| Per-clone anim speed is re-asserted on every animesh control avatar each frame | `llghostavatar.cpp:2128-2150` |

**Consequence of the eviction fact (proven for the cache, INFERENCE for playback):** an animation the
source played stays resolvable after the source logs off, so the clone can start it again later.
Reviewer: confirm that `startMotion()` on a UUID whose motion instance was stopped/deleted re-creates
it from the cache without a network fetch, and that a cache miss simply triggers a normal fetch
(i.e. degrades to a delay, never a crash).

---

## 3. Design

### 3.1 States — one toggle, one new drive mode

- New per-instance bool **`mAutoAnimate`** on the Ghost Studio instance (the toggle). Default from
  new setting `GhostAutoAnimateDefault` (false).
- New drive mode **`DRIVE_AUTO = 3`**, appended after FROZEN (never renumber existing values —
  they are combo `value`s and chat-command arguments).
- Transitions:

| From | Trigger | To |
|---|---|---|
| MIRROR (+AutoAnimate on) | source lost (§3.4) | AUTO (automatic) |
| MIRROR (+AutoAnimate on) | **Detach** button / command | AUTO (manual) |
| AUTO | source present again AND `GhostAutoAnimateRecouple` on | MIRROR (learning resumes, pool kept) |
| AUTO | Pause | FROZEN, `mResumeDriveMode = AUTO` |
| FROZEN | Resume with resume mode AUTO | AUTO (pool and schedule intact) |
| any | AutoAnimate toggled OFF | pool discarded; if in AUTO → FROZEN on the current pose (never "snap to T-pose") |

- **Entity clones only.** Overlay clones have no skeleton or motion controller of their own — they
  re-draw the live source (drive modes are already "ignored for overlay instances",
  `alghoststudio.h:95-99`) — so with the source gone there is nothing to keep playing.
  **Overlay UX:** the toggle stays visible on overlays but reads *AutoAnimate (entity only)*;
  turning it on offers **Convert to entity & AutoAnimate**. Never a silent no-op. When the selection
  contains more than `GhostAutoAnimateConvertWarnCount` (default 10) overlays, the offer shows the count and
  warns that entity clones cost more than overlays before converting. Declining leaves the overlays
  untouched and the toggle off.
- **Retargeting / outfit change:** if an entity's source changes (re-sourced to a different avatar)
  while learning, the window restarts and the pool is discarded — it would otherwise mix two
  animation sets. A sealed pool is kept, and handed off from normally, until the user presses
  **Relearn**.

### 3.2 The recorder (runs only while MIRROR + AutoAnimate on, and only inside the learn window)

**Learning is time-capped, not indefinite.** Toggling AutoAnimate on opens a **learn window** of
`GhostAutoAnimateDuration` seconds (default **120**, range 15-600) of *source-present* time. When it
expires the pool is **sealed**: the recorder stops, costs nothing further, and the clone keeps
mirroring live with a finished pool ready for handoff. Status shows `Learning 1:24 / 2:00`, then
`Learned ✓`.
- **Relearn** button: discards the pool and opens a fresh window.
- **Stop now** button: seals early (e.g. once you've seen the AO cycle through its stands).
- Source absent time does not count toward the window (a de-rez doesn't eat the budget).
- Handoff before the window ends uses whatever was learned so far (and logs `POOL THIN` if small).

Two independent channels, same data model:

- **Body channel:** observe the diff that `synchronizeCloneAnimations()` is about to apply (the
  desired set vs the previous desired set). Record `start(anim, t)` / `stop(anim, t)`.
- **Animesh channel:** one recorder **per cloned linkset index** (index into `mClonedLinksets`, NOT the
  source UUID — source UUIDs change on re-rez, the clone linkset does not). Record per clone prim id.

Timestamps use the clone's motion-controller time, so Studio anim speed and bullet-time scale the
learned timing consistently.

Each event is folded **immediately** into the pool (no event log is kept — see §3.7):
- **Loop entries** — anims whose motion reports `getLoop()==true` (or, until loaded, that stayed
  playing longer than their duration). Stored with observed *dwell* samples (how long the source kept
  it before switching). These are the AO stands / idle loops.
- **One-shot entries** — non-looping anims. Stored with observed *inter-arrival* samples and
  duration. These are expressions, blinks, fidgets.
- **Co-occurrence** — for each one-shot, which loop entries were active when it fired. Lets a face
  anim that only ever played with stand B stay paired with stand B.
- **Mutual exclusion groups** — loop entries never observed simultaneously are treated as
  alternatives (this is how "the AO's stand set" is discovered without knowing it is an AO).

Caps: pool entries capped (`GhostAutoAnimateMaxAnims`, default 64 per channel); timing samples per entry
capped at 16 (reservoir). Excluded: ground-sit (matches Mirror), and any anim already filtered by
Mirror today.

### 3.3 Autonomous playback (DRIVE_AUTO)

Per channel, a small scheduler produces a desired set each frame and hands it to the **existing**
sync paths — `synchronizeCloneAnimations()` for the body, the `LLObjectSignaledAnimationMap` +
`updateControlAvatar()` write for each animesh linkset. No new start/stop code paths.

- **Loops:** keep the current alternative running for a dwell drawn from its observed samples; then
  switch to another alternative in the same exclusion group, weighted by observed frequency, never the
  same one twice in a row if another exists.
- **One-shots:** Poisson-style firing from the observed inter-arrival samples, only while a
  co-occurring loop is active (or unconditionally if no co-occurrence was observed).
- **Randomness is seeded per clone instance id** (same pattern as Chaos, `seeded_unit`), so a take is
  repeatable; a `GhostAutoAnimateReseed` action gives a new variation.
- **Anything unloaded** (motion not yet resident) is skipped for this pick, not blocked on.

### 3.4 The handoff — the part that must be seamless

1. **Do NOT route MIRROR→AUTO through the existing `setEntityDriveMode` body.** It calls
   `synchronizeCloneAnimations(empty)` on leaving MIRROR (`:548-552`), which would stop everything —
   the exact pop this feature exists to prevent. Add an explicit MIRROR→AUTO (and AUTO→MIRROR)
   branch that **leaves `mClonePlayingAnimations` and the clone's object-anim entries untouched**.
2. The scheduler **adopts** the currently playing set as its initial state: currently running loops
   get a fresh dwell starting now; nothing is restarted.
3. "Source lost" = source avatar null/dead for `GhostAutoAnimateLossGrace` seconds (default 1.5) — reuses
   the same grace idea as hold-on-source-change so a brief de-rez does not trigger a handoff. For
   animesh, per linkset: source root missing for the grace period.
4. AUTO→MIRROR (recouple) goes through the normal mirror diff on the next frame, which is already
   hitch-free for anims both sets share.

### 3.5 Channel independence

Body and each animesh linkset hand off **independently**: a worn animesh pet whose source linkset was
detached switches to learned while the body is still mirroring live, and vice versa. The Detach
button detaches all channels at once.

### 3.6 Persistence

- **Save / Load learned performance** per clone: JSON (LLSD XML is fine) in the per-account user dir,
  storing only anim UUIDs, loop flags, timing samples, co-occurrence. No asset data.
- Loading onto a clone of a **different** outfit: body pool always applies; animesh pools apply by
  linkset index only when the count matches, otherwise skipped with a logged reason.

### 3.7 Overhead budget (user priority: low overhead)

Target: **no measurable frame cost** with learn off, and negligible with it on, even for crowds.

- **Event-driven, not per-frame work.** Mirror already computes the desired-vs-previous comparison
  every frame (`llghostavatar.cpp:2009`). The recorder hooks only the branch where that comparison
  found a change — a few times per second at most for a busy AO + face HUD. Zero work on frames
  where nothing changed.
- **Animesh:** same — hook only the `changed == true` branch of the existing per-linkset mirror.
- **Incremental fold, no event log.** Each start/stop updates fixed-size per-anim stats (count,
  running sums, 16-sample reservoir). No growth over time, no end-of-window batch processing.
  Worst case per channel: 64 entries × ~200 bytes ≈ 13 KB.
- **No per-frame allocation** in recorder or scheduler. Pool containers are reserved when the
  window opens.
- **One recorder per ENTITY, never shared (user decision 2026-09-28).** Each entity clone owns its
  own recorder, learn window and pool. Reason: the same source can be on a different avatar/outfit
  with an entirely different AO and animation set by the time the next clone learns (or mid-take),
  so a pool keyed by source id would mix two performances. Per-entity keeps each clone's pool
  exactly what that clone saw during its own window. This is affordable because the recorder is
  event-driven (a few updates/second while learning, zero after sealing) and bounded (~13 KB per
  channel), so N clones cost N × tiny, and nothing runs once their windows close.
- **Scheduler cost in Auto:** per channel per frame, one comparison of `now` against the next
  scheduled event time. It only builds a new desired set and calls the existing sync when an event
  is due.
- **Sealed = zero.** After the learn window closes the recorder is detached from the hook entirely.
- **Measured, not assumed:** the log reports recorder and scheduler time per second
  (`AUTOANIM-COST rec=…µs sched=…µs clones=N`) once a minute while active, so overhead is a number
  the user can read, not a claim.

---

## 4. Surfaces (all in one delivery)

- **Settings** (`settings_alchemy.xml`): `GhostAutoAnimateDefault`, `GhostAutoAnimateDuration` (120 s),
  `GhostAutoAnimateRecouple` (true), `GhostAutoAnimateLossGrace` (1.5), `GhostAutoAnimateMaxAnims` (64),
  `GhostAutoAnimateConvertWarnCount` (10).
- **Ghost Studio, Pose & Animation section** (`panel_ghost_studio.xml:205+`):
  - check box **AutoAnimate** (per selected clone);
  - drive combo gains **Auto** (`value="3"`);
  - button **Detach** (enabled when Mirror + AutoAnimate on);
  - learn-window spinner (seconds) + **Stop now** / **Relearn**;
  - status line: `Learning 1:24 / 2:00` then `Learned ✓ 3 loops · 9 one-shots · 2 animesh` (per
    channel on hover);
  - **Save…** / **Load…** / **Reseed**.
- Existing controls: Pause/Resume must round-trip AUTO; "Follow live" must mean MIRROR (keeps
  pool). Presets/reset: the toggle and new settings register with Studio reset.
- **Chat command:** extend `/ghostanim` (`alchatcommand.cpp:777`) with `auto on|off`, `detach`,
  `autoanim`, keeping `[all|selected]`.
- **Director reachability** (backlog rule: anything controllable in Studio must be reachable from the
  Director): add **AutoAnimate** and **Detach performance** to the Director cast menu
  (`menu_director_cast.xml`).

---

## 5. Instrumentation — the log states a verdict

On each handoff, per channel, one line:
- `AUTOANIM-HANDOFF ok` — N anims carried over unchanged, 0 stopped.
- `AUTOANIM-HANDOFF POP` — any anim in the playing set was stopped during the handoff frame. **This is
  the seamlessness test**; it must never fire.
- `AUTOANIM-POOL THIN` — fewer than 2 loop alternatives or < 30 s observed (warn, still runs).
- `AUTOANIM-SKIP <uuid> unloaded` — scheduler skipped a non-resident motion (rate-limited).

Separate outcome for untrustworthy measurement: if the source was never observed (learn toggled on
after the source was already gone), log `AUTOANIM-NODATA`, not a pool verdict.

---

## 6. Off-path must be inert (CLAUDE.md rule 1)

With the toggle **off** (the default), the code must be provably identical to today:
- the recorder does not run, allocates nothing;
- `setEntityDriveMode` behaviour for MIRROR/DIRECTED/FROZEN is byte-for-byte unchanged apart from the
  clamp widening and the two new explicit branches;
- no change to any render state. This feature touches no GL.

---

## 7. Reviewer: attack these first (least-sure list)

1. **§3.4.1** — is there any OTHER path on MIRROR exit that stops anims (e.g. `clearClonedObjectAnimations`,
   the pause requests, the source-change hold) that the explicit branch would still hit?
2. Does anything else clear the clone's `LLObjectSignaledAnimationMap` entries when the **source**
   linkset dies (region teardown, `releaseClonedAttachments`, a sim ObjectAnimation refresh)? If so,
   animesh learned playback is overwritten.
3. The co-occurrence / exclusion-group inference: find a realistic AO or face-HUD pattern where it
   produces a wrong grouping (e.g. an AO that briefly overlaps two stands during its own transition).
4. Motion priority: learned playback starts anims in a different ORDER than the source did. Can equal-
   priority anims resolve differently and produce a visibly different pose?
5. Clamp/enum widening: every `switch`/comparison on `EDriveMode` in `alpanelghoststudio.cpp`,
   `aldirectoranimswitcher.cpp`, `alchatcommand.cpp` — list the ones that silently mis-handle 3.
6. Per-entity recorders (§3.7): confirm the learning cost for a large crowd all learning at once
   (e.g. 50 entities in one window) stays inside the logged budget, and that pool memory is released
   on despawn / Relearn.
7. Is the approach wrong? In particular: should the recorder observe the **source** signaled maps
   directly rather than the clone's desired diff (the hold-on-source-change filter sits between them)?

---

## 8. In-world test (one pass, outcomes stated in advance)

1. Spawn a clone of yourself, Mirror, **AutoAnimate** on (default 2:00 window). Wear an AO with
   ≥3 stands and a face HUD that uses animations. Wait for the window to close.
   *Expect:* status counts down, then `Learned ✓` with ≥3 loops and several one-shots;
   `AUTOANIM-COST` shows the recorder stopped (rec=0) after sealing.
2. Press **Detach**. *Expect:* `AUTOANIM-HANDOFF ok`, no visible pop; the clone keeps the current stand.
3. Take your AO off. *Expect:* the clone is unaffected and cycles to other stands within their
   observed dwell; face keeps moving.
4. Wear an animesh attachment that animates; clone; learn; then detach the attachment from yourself.
   *Expect:* that linkset alone logs a handoff and keeps animating.
5. Pause → Resume. *Expect:* returns to Auto, same pool.
6. Save, despawn, respawn, Load. *Expect:* same behaviour.

If step 2 logs `POP`, the feature is broken regardless of how it looks.
