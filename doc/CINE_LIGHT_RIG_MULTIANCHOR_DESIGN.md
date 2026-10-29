# Cine Light Rig — Multi-Anchor (group-as-unit) — Deep Design

**Status:** design pass complete. **No source modified.**
**Date:** 2026-08-16.
**Answers:** `doc/CINE_LIGHT_RIG_MULTIANCHOR_SEED.md` (sections A–F).
**Feeds:** a Codex `--prompt-file` implementation brief → Opus adversarial review → build.
**Finals it builds on (inputs-only, never edited):**
`doc/CINE_LIGHT_RIG_MASTER_PRESETS_DESIGN.md` (scale-aware geometry SA-0..SA-12,
the SA-9 nominal-EV invariant, `scaledPoint`), the mirror facing fix
(status doc FOLLOW-ON 2: reflect the oriented yaw across the facing azimuth),
and the track-mode toggle (FOLLOW-ON 4: `CineLightRigTrackMode`).

Claim labels per CLAUDE.md: **PROVES** = I read the code at the cited line
(all citations re-verified against the working tree on 2026-08-16 — note the
line numbers in the older design docs have drifted; e.g. the anchor resolve
that was `alcinelightrig.cpp:835` is now `:983` after gobos + track-mode
landed. THIS document's numbers are current and authoritative for the brief).
**IMPLIES** = doc/comment or unbroken-but-unread chain. **INFERENCE** =
reasoning; could be wrong. Guesses say "guess".

**User decision (2026-08-16, fixed):** light the group as ONE unit. The rig
anchors to a SET of Director cast subjects, centres on the group, and derives
scale and facing from the group. NOT per-light anchoring. NOT
anchor-to-the-VCam. Both are §8 deferrals by decision, not by omission.

---

## 0. Executive summary

One delivery, controller + pure-model additions + panel/XUI, **zero new
settings keys, zero scene version bump, zero pipeline/shader/llprimitive
edits, lldirectorcast.\* consumed read-only**:

1. **Anchor-set model (§A): the group is a subset of the five subject slots**
   — {You, Subject A, Subject B, Subject C, Subject D} — stored as a session
   bool `mGroupEnabled` + a 5-bit slot mask `mGroupSlots` beside the existing
   `mAnchor`. Member cap is therefore **5 by construction**. Slots (not
   UUIDs) are stored, so recasting Subject B re-lights the group with no rig
   action, and scene round-trip rides the Director cast's own subject
   persistence (`lldirectorcast.cpp:533-536`, `:599-617`). The rig's scene
   block gains two OPTIONAL keys (`anchor_group`, `anchor_group_slots`);
   `data["anchor"]` keeps being written, so an old binary loading a new scene
   degrades gracefully to today's single anchor, and an old scene (keys
   absent) loads as group-off. **No version bump.**
2. **Group centre (§B): axis-aligned bounds midpoint** of the per-member
   points, where each member's point is computed EXACTLY as today's single
   subject: the global `CineLightRigTrackMode` picks chest-joint (0) or
   render-pos+1.2 (1) **per member**, foot-pivot mapped through the finalized
   `scaledPoint` with that member's OWN sanitized scale. Track-mode stays
   **GLOBAL** for v1. Lost members drop out of the aggregate; zero resolved
   members = today's subject-lost dark path; the existing centre damper
   consumes the aggregate unchanged.
3. **Group scale (§C, the hard one): `s_grp = max_i(s_i) + R_spread / r_nom`**
   — the largest member's scale plus the group's bounding radius expressed in
   units of the nominal orbit radius. This one dimensionless number feeds the
   EXISTING `Globals::mSubjectScale` path untouched, so the finalized clamp
   (`[min(r_nom, 0.1), max(r_nom, 9.09)]`), the 20 m reach cap, and — the
   critical part — **the SA-9 invariant (EV reads NOMINAL radius) all hold by
   construction**: group spatial extent provably cannot drift exposure
   because `mSubjectScale` never enters the intensity chain
   (`alcinelightrigmodel.cpp:630`, `:656-678` — grep-verifiable). Coverage is
   proved: every member sits at normalized light distance ≤ 2/2.2 ≈ 0.909
   < 1 in the unclamped regime (§C.3).
4. **Group facing (§D): the PRIMARY resolved member's facing** (first
   surviving slot in the fixed order You → A → B → C → D), fed into the
   finalized mirror formula unchanged. Averaging is rejected because the
   circular mean degenerates exactly in the CENTRAL two-shot case (actors
   facing each other) and every averaging scheme has a discontinuity that
   makes Mirror flip emergently as actors turn; the primary rule is
   deterministic, continuous while the primary lives, and bitwise-today for
   a single member.
5. **UI (§E):** a "Light group (Subjects)" checkbox + five member checkboxes
   (You/A/B/C/D) + a resolved-count status line, inserted under the existing
   anchor combo (which greys out in group mode). Panel reflow **+52 px**:
   panel 1312 → 1364 and — the M1 lesson — **all four wrapper heights** in
   both hosts (§E.3 names each line).
6. **Single-member == today, bitwise (§F).** Group OFF is structurally
   today's code path (default). Group ON with one resolved member takes an
   explicit fast path that executes the single-subject expressions verbatim
   — the same provable-identity discipline as the scale feature's `s == 1`
   path. No new kill-switch: `mGroupEnabled = false` IS the kill state.

Everything the finals own — `scaledPoint`, the exposure math, the mirror
reflection formula, damping, transitions, FX, gobos, master temp, presets,
scene split semantics — is consumed, not edited. The group changes only the
VALUES of three inputs: the centre handed to `applyFrame`, the
`Globals::mSubjectScale` scalar, and the `Transforms::mFacingAzimuthDeg`
scalar.

---

## 1. Verification — current code, cited and re-checked

### 1.1 The current single-anchor model (all PROVES)

| # | Fact | Evidence |
|---|---|---|
| V1 | `mAnchor` is one `LLUUID` session field; `setAnchor` stores it and resets centre/scale smoothing (`mHaveSmoothedCentre = false; mSmoothedScale = 1.f`) | `alcinelightrig.h:120`, `alcinelightrig.cpp:379-384` |
| V2 | Per tick: `LLDirectorCast::instance().resolve(mAnchor)` → one `LLVOAvatar*`; null → destroy emitters, clear state (the dark path) | `alcinelightrig.cpp:983-994` |
| V3 | Facing: `atan2(fwd.Y, fwd.X)` of `<1,0,0> * root->getWorldRotation()`, finite-guarded, written into `transforms.mFacingAzimuthDeg` | `alcinelightrig.cpp:999-1008` |
| V4 | Scale: `globals.mSubjectScale = scale_aware ? avatar->getUniformScale() : 1.f`, then `sanitizeGlobals` | `alcinelightrig.cpp:1011-1014`; accessor `llvoavatar.h:266` (base 1.f), `llghostavatar.h:116` (ghost override) |
| V5 | Centre: track-mode 0 → `mChest` world pos through `scaledPoint(avatar, p, s)`; missing chest or mode 1 → `getRenderPosition() + (0,0,1.2)` through `scaledPoint`; non-finite → frame skip (`return`) | `alcinelightrig.cpp:1090-1112`; mode read `:1092` (`track_mode_setting == 1 ? 1 : 0`) |
| V6 | `scaledPoint` (FINAL): foot-pivot mapping with `s == 1` fast path and the ghost's verbatim guards | `alcinelightrig.cpp:84-108` |
| V7 | OffsetZ: `centre.z += clamp(offset_z, −10, 10) * subject_scale` (raw scale, pre-smoothing), then agent→global | `alcinelightrig.cpp:1113-1120` |
| V8 | Damping: exponential smoother over the GLOBAL-frame centre + the scale, one `CineLightRigDamping` constant; snap on first frame / damping ≤ 0 / non-monotonic time | `alcinelightrig.cpp:1122-1145` |
| V9 | `render(mCurrentRadius, mCurrentLive, globals, mLastFrame)` with `globals.mSubjectScale = mSmoothedScale`; `applyFrame(frame, mSmoothedCentre, true_centre, mCurrentRadius, scale)` — projectors AIM at the undamped `true_centre` | `alcinelightrig.cpp:1171-1174`, `:854-879` |
| V10 | Scene: `data["anchor"] = mAnchor` on save; `setAnchor(data["anchor"].asUUID())` on load; loader reads NAMED keys only, so unknown keys are ignored | `alcinelightrig.cpp:1638-1669` (`:1647`), `:1671-1793` (`:1723-1726`) |
| V11 | Model (FINAL): `distance_ev = log2(safe_radius / 1.5)` from the NOMINAL radius; `effective_radius = clamp(safe_radius * mSubjectScale, min(safe_radius, 0.1), max(safe_radius, 9.09))` only when scale ≠ 1; everything spatial derives from `effective_radius`, everything photometric from nominal | `alcinelightrigmodel.cpp:630`, `:631-638`, `:653-655`, `:683-685`, `:690`, `:702-704`, `:715` |
| V12 | Model mirror (FINAL): `oriented = base_yaw + orbit_yaw; out = mirror ? wrap180(2*facing − oriented) : wrap180(oriented)` | `alcinelightrigmodel.cpp:521-527` |
| V13 | Model scale sanitize: non-finite / ≤ 0 / subnormal → 1.f; clamp `[SUBJECT_SCALE_MIN 0.05, SUBJECT_SCALE_MAX 150]` | `alcinelightrigmodel.cpp:463-472`; constants `alcinelightrigmodel.h:27-32` |
| V14 | Emitter box: `emitterBoxEdge(nominal, scale)` re-derives `effective/nominal` and calls the model's `emitterBoxEdgeFromRatio` | `alcinelightrig.cpp:110-121` |
| V15 | Panel anchor UI: `cine_anchor` combo ("You" + cast rows, value = UUID), `onAnchorSelected → setAnchor`, focus-guarded rebuild + sync; Reset All calls `rig.setAnchor(LLUUID::null)` | `alpanelcinelightrig.cpp:151`, `:266-306`, `:308-320`, `:322-331`, `:551`; XML `panel_cine_light_rig.xml:17-20` |
| V16 | Panel geometry: panel 350×1312; wrappers: floater 1322 content / 1312 embedded panel, Director tab 1322 scroll-content / 1312 embedded | `panel_cine_light_rig.xml:5`, `floater_cine_light_rig.xml:24,34`, `floater_director.xml:1536,1546` |

### 1.2 The Director cast roster API (consumed, READ-only — all PROVES)

| # | Fact | Evidence |
|---|---|---|
| C1 | `LLDirectorCast` is the session singleton for cast + Subjects A/B/C/D; subjects are ids INTO the cast | `lldirectorcast.h:46-103` |
| C2 | `resolve(id)`: null id → agent avatar; ghost clones resolved intentionally; stale/dead → `nullptr` (never silent self-fallback) | `lldirectorcast.cpp:145-184` |
| C3 | `resolveSubjectA()..D()`: unset or dead → `nullptr`; consumers treat that as "fall back to stock" | `lldirectorcast.cpp:186-204`; contract comment `lldirectorcast.h:87-91` |
| C4 | A member removed from the cast stops being a subject (slots auto-null) | `lldirectorcast.cpp:66-82` |
| C5 | All four subjects persist in the Director scene and are restored only when they point into the loaded cast | `lldirectorcast.cpp:533-536`, `:563-566`, `:598-617` |
| C6 | Roster reads: `getCast()` / `getIds()` (ordered), `membersInGroup(name)` for production-group tags | `lldirectorcast.h:66-69`, `:117` |
| C7 | Multi-select precedent in the fork: the Camera tab applies a per-row flag to `selectedCastIds()` (`setLookAtCamera`) | `llfloaterdirector.cpp:2107-2114` |

### 1.3 VCam (motivation only — no dependency)

The Virtual Cam is a free camera at a stored agent-space transform
(`mVirtual` / `mVirtualPos` / `mVirtualRot`, `llprismlens.h:152-163`,
PROVES). It carries **no subject list and no facing tied to any avatar** —
which is exactly why "light what the VCam frames" must be expressed as
"light a set of cast subjects" (this feature) and why anchor-to-VCam is a
non-feature: there is nothing on the VCam to anchor subject geometry to.
The rig never reads `llprismlens.*`.

### 1.4 Attenuation inputs (carried from the presets design, re-confirmed)

`deferredUtil.glsl:807-808`: `dist = lightDist / lightSize`, hard cutoff at
`dist > 1` (PROVES, per the master-presets design §1.4; unchanged since).
Two consequences load-bearing here: (i) co-scaling orbit and falloff radius
preserves subject illumination bit-for-bit (the SA-8/SA-9 foundation);
(ii) a light contributes NOTHING beyond its falloff radius — so §C's
coverage proof must bound every member's normalized distance strictly
below 1, not merely "close".

---

## Section A — the anchor-set model, selection, cap, and scene round-trip

### A.1 Decision: the set is a subset of the five subject slots

**MA-1.** The group is selected from exactly five slots: **You (self),
Subject A, Subject B, Subject C, Subject D** — stored as a 5-bit mask, NOT
as UUIDs. New controller session state beside `mAnchor`:

```cpp
// alcinelightrig.h — public
enum : U32
{
    GROUP_SLOT_SELF = 1 << 0,
    GROUP_SLOT_A    = 1 << 1,
    GROUP_SLOT_B    = 1 << 2,
    GROUP_SLOT_C    = 1 << 3,
    GROUP_SLOT_D    = 1 << 4,
    GROUP_SLOT_MASK = 0x1F,
};
void setGroupEnabled(bool enabled);
bool isGroupEnabled() const { return mGroupEnabled; }
void setGroupSlots(U32 mask);           // sanitized: mask & GROUP_SLOT_MASK
U32  getGroupSlots() const { return mGroupSlots; }
U32  lastResolvedGroupSlots() const;    // what last tick actually resolved (UI status)

// private
bool mGroupEnabled = false;
U32  mGroupSlots = 0;
U32  mLastResolvedGroupSlots = 0;
```

Both setters mirror `setAnchor`'s reset (`alcinelightrig.cpp:379-384`):
`mHaveSmoothedCentre = false; mSmoothedScale = 1.f;` — changing WHO is lit
snaps the smoothing to the new truth, exactly like an anchor switch today.

**Why slots and not UUIDs (the load-bearing choices):**

- **This is what the seed's own universe is** ("Subjects A/B/C/D + self")
  and what the VCam workflow addresses: the subjects are the fork's
  "who is in the shot" registers (CineCam anchor, Two-Shot second body,
  switcher slots — `lldirectorcast.h:16-19,87-91`). A VCam two-shot is
  framed on subjects; the rig lights the same registers.
- **Recast-aware for free.** The mask survives recasting: point Subject B at
  a different actor and the group re-lights on the next tick, because
  resolution happens per tick through `resolveSubjectB()` (C3). A UUID list
  would go stale exactly when the operator is iterating fastest.
- **Scene round-trip is one integer.** The subject UUIDs themselves already
  persist and revive in the Director cast's scene block (C5); the rig
  serializes only the mask (§A.3). No UUID array, no dangling-id policy.
- **Member cap = 5 by construction** — no separate cap constant, no
  "what about a crowd of 30" geometry question. The extent formula (§C)
  stays sane because the operator physically cannot select an unbounded set.

**Rejected alternatives** (seed A's other options):
- *"Group mode = all current cast members"*: the cast is a ROSTER, not a
  shot; a 30-actor crowd would drive `R_spread` into the radius ceiling
  permanently and light nobody well. Also unaddressable: you could not keep
  one actor out of the lit group without evicting them from the cast.
  Deferred as a possible later mode via `membersInGroup()` production tags
  (C6) — see §8.5.
- *N explicit anchor slots on the rig*: duplicates the subject registry the
  Director already owns, with a second UI and a second persistence path.
  The fork's architecture rule is that `LLDirectorCast` is the single
  source of "who" (`lldirectorcast.h:9-11`); the rig should consume it.

**MA-2 (mode semantics).** `mGroupEnabled == false` (DEFAULT) ⇒ today's
single-anchor path, structurally untouched — the group branch is not
entered. `mGroupEnabled == true` ⇒ `mAnchor` is ignored by the tick (but
retained, see A.3) and the mask drives resolution. An enabled group with
mask 0 resolves zero members ⇒ the dark path (V2 behaviour), which the UI
makes visible (§E). This is deliberate: "group of nobody" behaves like
"anchor lost", not like a silent fallback to self.

### A.2 Per-tick resolution, dedupe, and the primary member

**MA-3.** File-local helper in `alcinelightrig.cpp`:

```cpp
// Resolves the mask in fixed slot order Self, A, B, C, D via
// LLDirectorCast::resolve(LLUUID::null) / resolveSubjectA()..D().
// Dedupes by avatar->getID() keeping the FIRST occurrence (self may also
// be assigned to a subject slot). Returns resolved count; out_slots gets
// the bits that resolved (pre-dedupe bits whose avatar survived dedupe).
S32 gatherGroupMembers(U32 slots,
                       LLVOAvatar* out_members[GROUP_MAX_MEMBERS],
                       U32& out_slots);
```

- **Order is normative** (Self → A → B → C → D): it defines the PRIMARY
  member (facing, §D) and makes every aggregate deterministic under review.
- **Dedupe is required**: `resolve(null)` returns the agent avatar (C2), and
  the agent can also be Subject A. The bounds-centre and `max` aggregations
  of §B/§C are duplicate-immune by construction (min/max ignore repeats) —
  a deliberate robustness property of the chosen aggregators — but the
  resolved COUNT feeds the UI and the single-member fast path, so it must
  be the deduped count.
- Zero resolved ⇒ the existing dark path (V2), verbatim.

**MA-4 (single-member fast path — the bitwise guarantee).** If the deduped
resolved count is exactly 1, the tick proceeds down **today's single-subject
code** with `avatar = that member` — the same expressions in the same order
for facing (V3), scale (V4), centre (V5), OffsetZ (V7). No aggregation
arithmetic runs (not even a `(p+p)*0.5` round-trip). This is the same
provable-identity discipline as `scaledPoint`'s `s == 1` fast path and
SA-8's `s == 1` branch: the claim "single-member == today, bitwise" is
enforced by code structure, not by algebra about float round-trips.
Consequence required by the seed: a group whose other members are lost
mid-shot degrades to EXACTLY today's behaviour for the survivor.

### A.3 Scene round-trip — additive keys, no version bump

**MA-5 (save).** `sceneData()` (V10) gains two keys, always written:

```
data["anchor"]             = mAnchor;          // UNCHANGED (:1647)
data["anchor_group"]       = mGroupEnabled;    // NEW, bool
data["anchor_group_slots"] = (S32)mGroupSlots; // NEW, int, bits per MA-1
```

**MA-6 (load).** `applySceneData()` after the existing `setAnchor` call
(`:1723-1726`):

```cpp
setGroupEnabled(data["anchor_group"].asBoolean());        // absent -> false
setGroupSlots((U32)data["anchor_group_slots"].asInteger() // absent -> 0
              & GROUP_SLOT_MASK);
```

- **Old scene → new binary:** keys absent; LLSD undefined coerces to
  `false`/`0` ⇒ group off, single anchor applied — the seed's "an old
  scene's single anchor becomes a one-member set" is satisfied trivially
  (single mode IS the one-member behaviour, bitwise, per MA-4/MA-2).
- **New scene → old binary:** the v1 loader reads named keys only (V10
  PROVES `:1671-1793`), so the two new keys are ignored and the old binary
  uses `data["anchor"]` — graceful degradation to the last single anchor.
  This is why MA-5 keeps writing `"anchor"` unconditionally, holding the
  last single-mode choice (never synthesized from the group).
- **No version bump**: the block shape is unchanged, keys are additive, and
  both cross-version directions degrade sanely — exactly the condition the
  seed set for avoiding a bump. The version stays `1`; the three-way
  no-block / v1 / unknown-version split (`:1671-1697`) is untouched.
- The subject UUIDs the mask points at ride the Director cast scene block
  (C5). Ordering between the two blocks is a non-issue **because slots are
  re-resolved every tick** — the rig never caches subject UUIDs, so it does
  not matter which block applies first.

**MA-7 (Reset All).** The panel's Reset All (`alpanelcinelightrig.cpp:551`)
additionally calls `setGroupEnabled(false); setGroupSlots(0);` — group
state is session intent like anchor/shaft/hero, and Reset All is documented
as the thing that clears session intent (master-presets design §D.1).

**Zero new settings keys.** `mAnchor` is session state (no `control_name`
on `cine_anchor`, V15); the group is the same kind of state and persists
the same single way (the scene). This also means no settings-backed UI
divergence between the two panel hosts to reason about.

---

## Section B — the group centre, track-mode composition, lost members

### B.1 Per-member point: today's finalized computation, per member

**MA-8.** For each resolved member `i`, compute the point EXACTLY as V5/V6
compute it for the single subject today, factored into a file-local helper
(a pure code MOVE of `alcinelightrig.cpp:1090-1112`, no expression changes):

```cpp
// Track-mode point for one member: mode 0 = mChest world position (falls
// back to render-pos + 1.2 when the joint is missing or non-finite), mode
// 1 = render-pos + 1.2. Both foot-pivot mapped via scaledPoint with THIS
// member's sanitized scale. Returns false when even the fallback is
// non-finite (the member is skipped).
bool memberTrackPoint(LLVOAvatar* avatar, S32 track_mode, F32 member_scale,
                      LLVector3& out_point);
```

with `member_scale = sanitizeSubjectScale(scale_aware ?
avatar->getUniformScale() : 1.f)` — `sanitizeSubjectScale` being the V13
rules extracted verbatim into a callable (§F.2). Each member is scaled
about ITS OWN foot pivot with ITS OWN scale: the group centre is the
aggregate of already-scale-correct points, as the seed requires.

**MA-9 (TRACK-MODE INTEGRATION — global, composing per member).**
`CineLightRigTrackMode` stays **GLOBAL** (one S32 setting, one combo,
unchanged — `panel_cine_light_rig.xml:206-211`), and is applied to EVERY
member's point:

- **Skeleton (0):** the group centre is the bounds midpoint of each
  member's CHEST JOINT — the rig breathes with each member's pose, exactly
  the single-subject semantic, aggregated.
- **Body-stable (1):** the bounds midpoint of each member's
  render-position + 1.2 m — every member's animated bob is ignored and the
  group centre is stable, which is the whole point of the toggle, now
  holding for N members at once.

So "relative vs approximate" is evaluated PER MEMBER and then aggregated —
the composition the seed emphasizes. **Per-member track-mode is rejected
for v1**: it would need per-slot UI, per-slot scene data, and a mixed-mode
centre whose motion is half-breathing — complexity with no requested use
case (deferred, §8.3). **Single-member bitwise under either mode:** MA-4's
fast path runs today's code, in which the mode branch is byte-identical to
the shipped `:1092-1112` — bitwise under mode 0 AND mode 1 by structure.

### B.2 Aggregation: axis-aligned bounds midpoint (not centroid)

**MA-10.** The group centre is the **axis-aligned bounding-box midpoint** of
the finite member points: `c = (min(p_i) + max(p_i)) * 0.5` per axis,
computed in agent space (same space as today's `centre_agent`), factored
into the pure model for testing (§F.2):

```cpp
// alcinelightrigmodel.h
constexpr S32 GROUP_MAX_MEMBERS = 5;
void groupBoundsCentre(const F32 points[][3], S32 count, F32 out_centre[3]);
```

**Why bounds midpoint over centroid** (the seed's core geometry question):

1. **It pairs with the extent radius.** §C grows the orbit by
   `R_spread = max_i |p_i − c|`. The bounds midpoint approximately
   minimizes that max distance (exactly minimizes it per axis); the
   centroid can be pulled toward a cluster and inflate the far member's
   distance — worked case: two members co-located and one at distance `D`:
   centroid sits `D/3` from the cluster so `R_spread = 2D/3`; bounds
   midpoint sits at `D/2` so `R_spread = D/2`. Smaller `R_spread` = smaller
   orbit = later collision with the 9.09 m ceiling = wider usable groups.
2. **The two-shot is identical either way** (midpoint of two points), so
   the flagship case is untouched by the choice.
3. **Symmetric coverage.** The bounds midpoint lights the SPREAD
   symmetrically (equal worst-case reach to both extremes per axis), which
   is the correct default for "light the group as one unit"; the centroid
   optimizes average distance, which favours crowds over composition.
4. **Duplicate-slot immunity.** min/max are unaffected by a member resolved
   through two slots; a centroid would double-weight it (MA-3 dedupes
   anyway — this is defense in depth).

Motion behaviour: the bounds midpoint moves at most at half the speed of an
extreme member (INFERENCE, elementary), and the existing damper (V8)
consumes the aggregate **unchanged** — `true_centre` is simply computed
from `c` instead of the single point, then smoothed exactly as today. The
seed's "confirm damping still applies to the group centre": confirmed by
construction; the damper sits downstream of the centre computation
(`:1120-1145`) and is not touched.

**MA-11 (OffsetZ).** Applied ONCE to the aggregated centre:
`c.z += clamp(offset_z, −10, 10) * s_grp_raw` — the group analogue of V7,
using §C's raw (pre-smoothing) group scale so the trim stays proportional
to the rig geometry, exactly as SA-5 argued for the single subject. Single
member: `s_grp_raw == s_member` (§C.2 fast path) ⇒ identical.

### B.3 Lost / unloaded members, and entering/leaving mid-shot

**MA-12.**

| Condition | Behaviour | Mechanism |
|---|---|---|
| A selected slot is unset or its subject left/died | Slot resolves null (C3), member simply absent from the aggregate this tick | MA-3 |
| A member's points non-finite (both chest and fallback) | Member skipped this tick | MA-8 returns false |
| ≥ 1 member resolves | Centre/scale/facing from the RESOLVED members only | MA-3/10; count==1 → MA-4 fast path |
| 0 members resolve (incl. mask 0) | Today's subject-lost dark path, verbatim: emitters destroyed, smoothing cleared | V2 (`:984-994`) |
| Member joins/leaves mid-shot | Centre and `s_grp` STEP; at damping > 0 both are eased by the existing smoother (V8 — it smooths centre AND scale with one τ); at damping 0 they snap — the same class of cut as an anchor switch today | V8; no new mechanism |
| Group toggled / slots edited by the operator | Smoothing reset (snap to new truth) — identical to `setAnchor` semantics | MA-1 setters |
| A recast slot (Subject B → new actor) | Next tick resolves the new actor; behaves as join+leave above | C3 + MA-3 |

The step-on-membership-change is deliberately NOT specially eased beyond
the existing damper: inventing a second easing path for membership would
diverge from how anchor switches behave today and complicate the
determinism story. The UI recommendation (§E) is to use Follow damping for
group shots; a dedicated membership cross-fade is deferred (§8.8).

---

## Section C — group scale / radius (the hard problem)

### C.1 The formula

**MA-13.** One dimensionless group scale, computed in the pure model:

```cpp
// alcinelightrigmodel.h
// Group rig-geometry scale: the largest member's subject scale plus the
// group's spatial bounding radius measured in units of the nominal orbit
// radius. count >= 1; points finite (controller-filtered); member scales
// are sanitized internally. count == 1 returns the sanitized member scale
// EXACTLY (no spread arithmetic).
F32 groupSubjectScale(const F32 points[][3], const F32 member_scales[],
                      S32 count, const F32 centre[3], F32 nominal_radius);
```

```
r_nom     = clamp(finiteOr(setup.mRadius, 1.5), MIN_RADIUS, MAX_RADIUS)   // the model's own sanitize, V11
s_max     = max_i sanitizeSubjectScale(s_i)                               // V13 rules per member
R_spread  = max_i | p_i − c |                                             // Euclidean, agent metres
s_grp     = clamp(s_max + R_spread / r_nom,
                  SUBJECT_SCALE_MIN, SUBJECT_SCALE_MAX)                   // 0.05 .. 150
```

`s_grp` is assigned to `Globals::mSubjectScale` and flows through the
FINALIZED model path **untouched** (V11):

```
effective_radius = clamp(r_nom * s_grp, min(r_nom, 0.1), max(r_nom, 9.0909))
                 = clamp(r_nom * s_max + R_spread, ...)        // inside the clamp band
```

— i.e. **the spread adds metres to the orbit directly** (the division by
`r_nom` exists purely to express extent in the model's dimensionless scale
channel), and every existing clamp, the 20 m reach derivation
(`SCALED_RADIUS_CEIL = 20/2.2`), the emitter-box ratio (V14), and the
smoothing (V8, `mSmoothedScale`) apply to the group with zero new plumbing.

**Why `max` of member scales, not average:** coverage is a max-norm
problem. A 0.05× doll + 1.0 avatar group must carry AT LEAST the geometry
the 1.0 avatar needs; the average (0.525) would orbit the lights half as
far out as the large member requires and clip its rim/backlight framing.
The doll is then lit by a rig sized for the avatar — slightly wide framing
for the doll, correct exposure for both (C.4) — the right degradation, and
the same one SA-8 already accepts for out-of-band single subjects.

**Why extent enters at all (rejecting seed option 1):** two 1.0 avatars
6 m apart at `r_nom = 1.5`: member-scale-only gives orbit 1.5 m about the
midpoint, projector falloff 3.3 m — each member sits 3 m from centre, so a
far-side light is 4.5 m away, PAST its own 3.3 m hard cutoff (§1.4.ii):
the key light literally does not reach the off-side actor. Option 1 fails
the feature's one job. Extent must be in the formula.

**Why not extent-only:** a giant clone and a doll standing chest-to-chest
have near-zero spread but need the giant's scaled geometry — `s_max`
carries the member-size requirement, `R_spread` the separation
requirement; the sum covers both simultaneously (proof next).

### C.2 Single-member identity (bitwise)

`count == 1` ⇒ `groupSubjectScale` returns
`clamp(sanitizeSubjectScale(s_0), 0.05, 150)` — by MA-4 the controller
never even calls it for one member (the fast path reads
`avatar->getUniformScale()` exactly as V4), and the pure function's own
`count == 1` early-return (specified, TUT-pinned §5) is defense in depth.
The double-sanitize is idempotent (V13's clamp is a projection). No
`+ 0 / r_nom` arithmetic executes in either path, so "single-member ==
today" needs no floating-point argument at all.

### C.3 Coverage proof (the formula actually lights the group)

Inside the unclamped regime (`r_eff = r_nom·s_max + R_spread ≤ 9.09 m`),
for every resolved member `i` at distance `d_i ≤ R_spread` from centre `c`:

- Worst-case light-to-member distance: a light sits ON the orbit sphere of
  radius `r_eff` about `c`, so distance ≤ `r_eff + d_i ≤ r_eff + R_spread`.
- Every projector's falloff radius is `2.2·r_eff` (V11 `:690`).
- Normalized distance (the shader's `dist`, §1.4):
  `(r_eff + R_spread) / (2.2·r_eff)`. Since
  `r_eff ≥ r_nom·s_max + R_spread > R_spread` (as `r_nom ≥ 0.5`,
  `s_max ≥ 0.05` ⇒ `r_nom·s_max > 0`):
  `dist ≤ 2·r_eff / (2.2·r_eff) = 1/1.1 ≈ 0.909 < 1.`
  **Every member is strictly inside every light's hard cutoff, with ≥ 9%
  margin.** (Omnis: falloff `1.5·r_eff`, V11 `:715`; worst member
  `dist ≤ 2/1.5 = 1.33` — an off-side member can leave a far omni's
  radius. Acceptable and physical: bounce is a local fill by design, the
  near-side omnis cover each member, and omnis are an accent channel
  (bounce ratio ≤ 1, off by default per preset). Stated so the reviewer
  does not rediscover it as a defect. INFERENCE from V11 arithmetic.)
- Nearest-light bound: distance ≥ `r_eff − R_spread ≥ r_nom·s_max ≥
  r_nom·s_i` — no member ever sits closer to the orbit shell than a
  single subject of the largest member's scale does today ⇒ no blow-out
  hotspot from the group construction.
- Member body extent: the member's own chest/body point is `p_i`; its head
  sits ≈ `0.6·s_i` above. Coverage of the surface needs
  `1.2·r_eff ≥ d_i + 0.6·s_i` (from `2.2·r_eff ≥ r_eff + d_i + 0.6·s_i`);
  since `r_eff ≥ r_nom·s_max + d_i` it suffices that
  `1.2·r_nom·s_max ≥ 0.6·s_i`, true whenever `r_nom ≥ 0.5 = MIN_RADIUS`.
  Per-member "reach" therefore does NOT need its own term in `R_spread` —
  the seed's option-2 "+ their individual reach" is satisfied implicitly by
  `s_max` at every legal nominal radius. (INFERENCE with stated
  anthropometric guess 0.6 m; the margin is 2× at the default radius.)

Beyond the ceiling (`r_nom·s_grp > max(r_nom, 9.09)`): the existing clamp
(V11) freezes the orbit at the ceiling and **coverage narrows at constant
exposure** — the documented giant-clone degradation (SA-8/SA-9), now also
reached by very wide groups. At `r_nom = 1.5`, `s_max = 1`, the ceiling
bites at `R_spread ≈ 7.6 m` — a ~15 m-wide two-shot, comfortably past any
composition a 20 m-reach lighting engine can serve anyway (C9 physics).

### C.4 SA-9 preservation (the trap, closed)

**MA-14.** `distance_ev` continues to read the NOMINAL radius
(`alcinelightrigmodel.cpp:630`, **unchanged**), and the group enters the
model through exactly one channel: `Globals::mSubjectScale`. The consumers
of `mSubjectScale` inside `render()` are the `effective_radius` derivation
(`:631-638`) and nothing else; the intensity chain
(`total_ev`/`pre_headroom`/`raw_intensity`/`mClipped`, `:656-678`, and the
omni chain `:709-714`) contains no `mSubjectScale` term — grep-verifiable,
and pinned by the existing exposure-invariance TUT test, which §5 extends
with group-representative scale values. Therefore:

> **A group's spatial extent CANNOT drift exposure.** `R_spread` changes
> `s_grp`; `s_grp` changes geometry only. The EV a preset authors is the EV
> a two-shot gets — the same invariant, same mechanism, same test as the
> clone-scale trap SA-9 closed.

What DOES vary across a spread group is per-member attenuation: members sit
at normalized distances in `[(r_eff−R_spread)/2.2r_eff,
(r_eff+R_spread)/2.2r_eff]` around the single-subject `1/2.2 ≈ 0.4545`.
That is physically-correct falloff across a physical spread — an actor
walking off-centre dims slightly, as under real fixtures — NOT exposure
drift (the rig's output is constant; the subject moved). Explicitly
accepted; compensating it per-member would require per-light intensity
edits that violate both SA-9's spirit and the one-rig-as-unit decision.
(Numerically minor: attenuation between dist 0.23 and 0.68 in the §C.5
worked case spans well under a stop through
`calcLegacyDistanceAttenuation`; not computed here — in-world item. Guess.)

**MA-15 (semantic note, doc-comment only).** With groups,
`Globals::mSubjectScale` is no longer literally "the subject's scale" — it
is "the rig geometry scale (≥ the largest member's subject scale)". The
field is NOT renamed (churn across model/controller/tests for zero
behaviour); its header comment (`alcinelightrigmodel.h:71` vicinity) gains
one sentence saying so, so no future consumer misreads it as a body size.

### C.5 Worked examples (implementer sanity anchors)

| Group | r_nom | s_max | R_spread | s_grp | r_eff | proj falloff | worst member dist |
|---|---|---|---|---|---|---|---|
| Solo 1.0 avatar | 1.5 | 1.0 | — (fast path) | 1.0 | 1.5 | 3.3 | 0.4545 (today) |
| Two-shot, 1.0 + 1.0, 3 m apart | 1.5 | 1.0 | 1.5 | 2.0 | 3.0 | 6.6 | (3.0+1.5)/6.6 = 0.68 |
| 0.05 doll + 1.0 avatar, 2 m apart | 1.5 | 1.0 | 1.0 | 1.667 | 2.5 | 5.5 | 0.636 |
| 4-shot in a 4 m square (diag pts) | 1.5 | 1.0 | 2.83 | 2.886 | 4.33 | 9.52 | 0.752 |
| 6× giant + doll, adjacent | 1.5 | 6.0 | ~0.5 | 6.33 | 9.09 (ceil) | 20 (cap) | ceiling regime |

All exposure columns are omitted because they are all IDENTICAL to solo —
that is the point (C.4).

### C.6 Smoothing and mid-shot dynamics

`s_grp` (raw) is fed to the existing per-tick sample → `mSmoothedScale`
smoother (V8) exactly as the single scale is today: default damping 0 =
snap; damping > 0 eases centre and orbit with one τ. Members walking
apart/together therefore swell/shrink the orbit continuously (R_spread is
continuous in member positions); joins/leaves step it through the same
damper (MA-12). FX and the 0.9 s transition compose orthogonally exactly
as SA-7 proved — `effective = eased_nominal × smoothed_scale` per frame,
no new coupling.

---

## Section D — group facing (for Mirror)

### D.1 Decision: the primary resolved member's facing

**MA-16.** `transforms.mFacingAzimuthDeg` is computed from the **primary
member** — the first entry of MA-3's fixed slot order (You → A → B → C →
D) that resolved — using today's exact code (V3: root joint world rotation,
forward azimuth, finite-guarded). Single-member group and single-anchor
mode: the primary IS the only member ⇒ **bitwise today's per-avatar
facing** (MA-4 fast path executes V3 verbatim). The finalized mirror
formula (V12) is untouched — it just receives this value.

### D.2 Why not the obvious candidates

- **Average (circular mean) of member facings** — the seed's obvious
  candidate — **degenerates in the CENTRAL case, not an edge case**: a
  conversational two-shot has the actors FACING EACH OTHER, so their
  forward vectors sum to ≈ zero and the mean azimuth is numerically
  undefined/unstable. Back-to-back (the seed's example) is the same sum.
  The mean is also discontinuous: as one actor turns, the mean can sweep
  the mirrored lights across the set faster than either actor moves.
  A note for completeness: the mirror reflection is AXIAL —
  `wrap180(2(θ+180) − oriented) ≡ wrap180(2θ − oriented)` (V12 algebra),
  so the doubled-angle **axial mean** (mean of 2θ) would fix the
  facing-each-other case specifically… and degenerate instead for
  near-perpendicular pairs, with the same emergent-sweep behaviour.
  Every averaging scheme has a degeneracy somewhere on the circle; an
  on-set Mirror toggle must be PREDICTABLE, not emergent. Axial mean is
  the recorded v2 candidate (§8.4) if a real shot demands a
  group-geometric axis.
- **The VCam facing** — excluded by the fixed user decision, and
  structurally unavailable: the rig has no dependency on `llprismlens`,
  the VCam is optional state (§1.3), and a facing that jumps when the
  operator repositions a camera couples lighting to camera work.
- **Fixed/world facing (0) for groups** — silently turns Mirror back into
  the pre-fix world-X reflection (the exact bug FOLLOW-ON 2 fixed) for any
  group, including a group of one-that-happens-to-be-selected-via-mask.
  Violates the single-member-bitwise requirement unless special-cased,
  and the special case IS the primary rule.

**Why primary is right:** (a) deterministic and stable — the operator can
predict which body the mirror axis tracks, and it is the same subject
(You, else A) that the fork's other tools already treat as the reference
(Subject A doubles as the CineCam anchor, `lldirectorcast.h:88-89`);
(b) continuous while the primary stays resolved — no sweep;
(c) cinematically sensible for the two-shot: the primary's facing axis
runs approximately THROUGH the facing partner, so Mirror swaps the rig
across the line of the two-shot — cheek-to-cheek for the pair, which is
the mirror's contract; (d) degrades by the same slot order everything else
uses: primary lost ⇒ facing steps to the next member and the non-FX path's
0.9 s transition (V9 upstream, `updateTransition` consuming `computeLive`
output) eases the resulting yaw swing — no snap. (Facing-step easing is
INFERENCE from the transition path; the FX path recomputes live and would
snap — same as any facing change mid-FX today, e.g. the anchor turning.)

**MA-17.** The facing is read from the primary EVERY tick (no caching,
matching V3), so an Actor-Mover-driven primary keeps steering the mirror
axis exactly as a single anchor does today ("lights swing on turn" is the
shipped, intended behaviour — FOLLOW-ON 2 nit).

---

## Section E — UI

### E.1 Controls

All group widgets are session-state (no `control_name`), synced like the
anchor combo (V15 pattern: guarded rebuild + displayed-state cache).

Row A (below the anchor row): 
- `check_box` **`cine_group_enable`**, label "Light group (Subjects)",
  left 8, width 170. Commit → `setGroupEnabled(checked)`.
- `text` **`cine_group_status`**, left 182, width 158, right-aligned
  informal status: group off → empty; on → `"2 of 3 lit"` from
  `lastResolvedGroupSlots()` vs `getGroupSlots()` (popcount), or
  `"none resolved"` (amber, reusing the radius-cue colour idiom,
  `alpanelcinelightrig.cpp:157-159`) when the mask resolves nobody.

Row B: five `check_box` members, commit → `setGroupSlots(rebuilt mask)`:
- **`cine_group_self`** "You" left 8 w 52 · **`cine_group_a`** "A" left 64
  w 40 · **`cine_group_b`** "B" left 108 w 40 · **`cine_group_c`** "C"
  left 152 w 40 · **`cine_group_d`** "D" left 196 w 40. Tooltip on each:
  "Include Director Subject X in the lit group." (±6 px implementer
  freedom, invariant: rows fit 8..340.)

Enable logic (in the sync path): group ON ⇒ `cine_anchor` combo
`setEnabled(false)` (its selection is retained, greyed — it is still the
old-viewer scene fallback, MA-5); member checks enabled. Group OFF ⇒
combo enabled, member checks greyed but visible (discoverability).

Sync plumbing: new `void syncGroupControls();` called from the same two
sites that already refresh the anchor UI per V15 (`draw()` and
`onVisibilityChange`, `alpanelcinelightrig.cpp:726`, `:743`), guarded by
cached `bool mDisplayedGroupEnabled; U32 mDisplayedGroupSlots; U32
mDisplayedResolvedSlots;` so it writes widgets only on change (the anchor
combo's no-churn discipline). No focus guard needed — checkboxes don't
have the combo's text-entry hazard.

Panel header additions (`alpanelcinelightrig.h`): the three cached fields,
`syncGroupControls()`, and six child pointers (`LLCheckBoxCtrl*
mGroupEnable; mGroupSlotChecks[5]; LLTextBox* mGroupStatus;`).

### E.2 Behavioural notes

- Checking members with group OFF does nothing until the group is enabled
  (mask is stored either way) — one obvious master switch, no spooky
  implicit mode change on a member click.
- The status line is the "group indicator" the seed asks for, and doubles
  as the zero-resolved warning (MA-2's deliberate dark state is visible,
  not mysterious).
- Reset All: MA-7 (clears group; combo returns to "You" via the existing
  `setAnchor(null)` path).

### E.3 Reflow — the M1 lesson, applied by name

Insertion: Row A top 79 (h 18), Row B top 103 (h 18); the section border
currently at top 84 moves to 136. **Every widget with top ≥ 84 shifts
+52.** Heights that MUST all change together (the M1 wrapper-height
lesson — the base panel AND both hosts' two wrapper layers):

| File:line (today) | Value | New |
|---|---|---|
| `panel_cine_light_rig.xml:5` (panel) | 1312 | **1364** |
| `floater_cine_light_rig.xml:24` (content wrapper) | 1322 | **1374** |
| `floater_cine_light_rig.xml:34` (embedded panel) | 1312 | **1364** |
| `floater_director.xml:1536` (`cine_light_rig_scroll_content`) | 1322 | **1374** |
| `floater_director.xml:1546` (`cine_light_rig_embedded`) | 1312 | **1364** |

Review checklist item: rect arithmetic proves the last control
(`cine_reset_all`, ends 1279+24=1303 today → 1355 after shift) sits inside
1364 with 9 px slack, matching today's margin; both hosts scroll to it.

---

## Section F — interaction with the FINALS (inputs only)

### F.1 The contract

**MA-18.** The multi-anchor feature changes the VALUES of exactly three
inputs and nothing else:

| Input | Single (today / group-off / one resolved) | Group (≥ 2 resolved) |
|---|---|---|
| centre → `applyFrame` / damper | member point (V5) | bounds midpoint of member points (MA-10) |
| `Globals::mSubjectScale` | `getUniformScale()` sanitized (V4) | `s_grp` (MA-13) |
| `Transforms::mFacingAzimuthDeg` | anchor facing (V3) | primary member facing (MA-16) |

Consumed UNCHANGED downstream: `scaledPoint` (per member now, body
byte-identical), the effective-radius clamp, `distance_ev` and the entire
intensity/headroom/bounce/clip chain, the mirror reflection line, the
emitter-box ratio, damping, transitions, FX evaluation, gobos, master
temp, shadow policy, projector flags, presets, and the scene split
semantics. Emitter lifetime paths gain zero new states — the group's
"none resolved" reuses V2's dark path verbatim.

### F.2 Refactors that touch final-adjacent code (each bit-identical, each a review item)

1. **`memberTrackPoint`** (MA-8): pure MOVE of `:1090-1112` into a helper.
   Review: diff shows expression-identical; single path calls it with the
   same arguments in the same order.
2. **`sanitizeSubjectScale`** extraction from `sanitizeGlobals`
   (`alcinelightrigmodel.cpp:463-472`): `sanitizeGlobals` becomes a caller;
   TUT pins bit-identity across the degenerate grid (§5.1). The subnormal
   comment travels with the code.
3. **Tick reordering**: today scale (V4, `:1011`) is computed before the
   centre block (`:1090`); the group needs points before the group scale.
   The sampling (members → per-member scales/points → centre → `s_grp` →
   facing) is gathered into one block where V3/V4 sit today, and the
   `:1090-1119` block consumes the precomputed centre. Constraint (review
   item): in single mode the emitted sequence of reads and arithmetic is
   today's — the reorder may not change single-anchor behaviour, and the
   reviewer should demand the single path be the SAME statements, not
   equivalent ones.

### F.3 No new kill-switch — argued

`CineLightRigScaleAware` earned a kill-switch because it changed DEFAULT
behaviour on every subject. Multi-anchor changes nothing until the
operator enables the group: `mGroupEnabled = false` (default, and the
loaded state of every existing scene) IS the off state, structurally
today's code. A settings kill-switch would duplicate that bool. The
`scale_aware` setting continues to force every member's `s_i = 1`
(MA-8's read is gated on it exactly like V4), so its FALSE state composes:
group with scale-aware off = bounds centre of unscaled points, `s_grp = 1 +
R_spread/r_nom` (extent still covers the spread — correct, since extent is
real geometry, not subject scale).

---

## 5. Testability — pure-model TUT additions (`alcinelightrigmodel_test.cpp`)

The aggregation math is IN the pure model (MA-10, MA-13, F.2.2)
specifically so this cluster exists. Existing suite passes untouched (the
group functions are additive; `mSubjectScale` semantics for `render()` are
unchanged).

1. **`sanitizeSubjectScale` extraction pin:** for s ∈ {NaN, ±inf, 0, −1,
   FLT_MIN, 0.04, 0.05, 1, 2, 150, 151}: result bitwise equals
   `sanitizeGlobals({.mSubjectScale = s}).mSubjectScale` — fails if the
   extraction drifts from the shipped rules (V13).
2. **Centre, count 1:** `groupBoundsCentre` of one point returns that
   point bitwise (assert per component) — the defense-in-depth identity.
3. **Centre, two-shot:** two points → exact midpoint (power-of-two
   coordinates for exact F32); **three-point clustered:** {0,0,0},
   {0,0,0}, {4,0,0} → centre (2,0,0) — the case where a centroid (1.33)
   would differ; fails if anyone "optimizes" bounds into a mean.
4. **Duplicate immunity:** appending a copy of an existing point changes
   neither centre nor `groupSubjectScale`.
5. **Scale, count 1:** returns `clamp(sanitize(s_0))` bitwise; verify with
   s_0 = 0.7 and a garbage centre/points argument the early-return never
   reads (pins the specified `count == 1` short-circuit).
6. **Scale formula exact:** points {(−1,0,0),(1,0,0)}, scales {1, 0.05},
   centre (0,0,0), r_nom 2 → `s_grp = 1 + 1/2 = 1.5` exact in F32; and
   the §C.5 two-shot row (r_nom 1.5, spread 1.5 → 2.0 exact).
7. **Monotonicity:** `s_grp` non-decreasing as one point moves outward
   over a grid (fixed scales, ~20 steps); non-decreasing in each member
   scale.
8. **Clamps:** enormous spread (R_spread/r_nom > 200) → exactly
   `SUBJECT_SCALE_MAX`; degenerate member scales fall back per test 1's
   rules inside the group.
9. **SA-9 invariance under a group (the headline):** extend the existing
   exposure-invariance test's scale grid with group-representative values
   {1.5, 1.667, 2.0, 2.886, 3.7}: `mIntensity` and `mClipped` (proj and
   omni) bitwise identical to s = 1 across the EV grid — the direct pin of
   "group extent cannot drift exposure" (fails on any future
   `mSubjectScale` leak into the intensity chain).
10. **Coverage property:** for a grid of (r_nom, s_max, R_spread) with
    `r_nom·s_max + R_spread ≤ SCALED_RADIUS_CEIL`: assert
    `(r_eff + R_spread) ≤ 2.2·r_eff·(1/1.1 + ε)` computed through the REAL
    `render()` output (`|off| + R_spread` vs `mLightRadius`) — pins §C.3's
    guarantee against clamp-shape regressions, using outputs, not the
    formula's own algebra (the lesson of the tautological SA test 4).

**Not TUT-testable (controller-side; adversarial-review items instead):**
slot resolution order & dedupe (needs `LLDirectorCast`/avatars), the
single-member fast path's statement-identity (git-diff review, the F.2.3
constraint), facing-primary selection, scene key round-trip, UI sync.
Review should regress each the way the mirror review did: worked cases at
facing 0/90/37 with a two-member group, primary-loss handoff, old-scene /
new-scene / old-binary matrix.

---

## 6. File/function checklist (the Codex-brief skeleton)

| File | Change |
|---|---|
| `indra/newview/alcinelightrigmodel.h` | `GROUP_MAX_MEMBERS`; `groupBoundsCentre`; `groupSubjectScale`; `sanitizeSubjectScale`; one-sentence `mSubjectScale` doc note (MA-15) |
| `indra/newview/alcinelightrigmodel.cpp` | the three functions; `sanitizeGlobals` re-expressed over `sanitizeSubjectScale` (bit-identical, F.2.2) |
| `indra/newview/alcinelightrig.h` | `GROUP_SLOT_*` enum; `set/get GroupEnabled/GroupSlots`, `lastResolvedGroupSlots`; fields `mGroupEnabled`, `mGroupSlots`, `mLastResolvedGroupSlots` |
| `indra/newview/alcinelightrig.cpp` | `gatherGroupMembers` (MA-3); `memberTrackPoint` (MA-8 move); tick group branch + single fast path (MA-4, F.2.3); setter resets (MA-1); scene keys (MA-5/6) |
| `indra/newview/alpanelcinelightrig.{h,cpp}` | group widgets wiring, `syncGroupControls`, status text, enable logic, Reset All addition (MA-7); cached display state |
| `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml` | Rows A/B (§E.1); +52 px shift of everything below; height 1364 |
| `indra/newview/skins/default/xui/en/floater_cine_light_rig.xml` | wrapper heights 1374/1364 (§E.3) |
| `indra/newview/skins/default/xui/en/floater_director.xml` | wrapper heights 1374/1364 (§E.3) |
| `indra/newview/tests/alcinelightrigmodel_test.cpp` | §5 cluster |

**Not touched:** settings.xml (zero new keys), scene/preset versions (both
stay 1), notifications, CMake (no new sources), `lldirectorcast.*`,
`llfloaterdirector.cpp`, all render/pipeline/shader files.

## 7. Risks, ranked

**Called out separately per the brief — items touching the finalized
inputs or the every-shot path:**

- **R1 ⚠ (highest): the tick refactor (F.2.3) sits on the EVERY-SHOT
  centre/facing/scale path**, including plain single-anchor shots with the
  feature never enabled. Mitigation is structural (group branch additive;
  single path = today's statements) and procedural (review demands
  statement-identity, not equivalence; in-world first test is a
  single-anchor A/B against the current binary). This is the delivery's
  only risk to shipped behaviour.
- **R2 ⚠ `mSubjectScale` semantic widening (MA-15):** every current
  consumer is spatial (verified V11/V14); the risk is a FUTURE consumer
  reading it as body size. Contained by the doc comment; no code hazard
  today.
- **R3 Facing handoff on primary loss (MA-16):** a mirrored setup swings
  through the 0.9 s ease when the primary drops mid-shot. Accepted;
  operator-visible; the deterministic slot order makes it predictable.
  In-world probe: two-member group, Mirror on, kill the primary.
- **R4 Ceiling collision for wide groups (§C.3):** coverage narrows at
  constant exposure past `R_spread ≈ 7.6 m` (defaults). Physics of the
  20 m cap, not a defect; the radius tooltip
  (`panel_cine_light_rig.xml:182`) gains "…or grows with a spread anchor
  group" to keep it legible.
- **R5 Membership churn at damping 0 snaps the rig (MA-12):** same class
  as today's anchor switch; documented; Follow damping is the tool.
  In-world: toggle a member in/out during a take at damping 0 and 0.5.
- **R6 UI reflow (M1 class):** five heights must move together (§E.3
  table); review by rect arithmetic in both hosts.
- **R7 Scene cross-version matrix (MA-5/6):** old→new (keys absent ⇒ off),
  new→old (keys ignored ⇒ single anchor). Both directions argued from V10;
  review should load-test all four combinations including a group-enabled
  scene with mask 0.
- **R8 Dedupe / self-in-slot (MA-3):** self selected via "You" AND as
  Subject A must count once (status text honesty; aggregates already
  immune). Review with that exact configuration.
- **R9 Omni reach on wide groups (§C.3 note):** off-side members can exit
  a far omni's 1.5× radius — accepted bounce behaviour, flagged so the
  in-world pass doesn't file it as a bug.

## 8. Deferred (decided now, with reasons)

1. **Per-light anchoring** (key on A, rim on B): excluded by the fixed
   user decision; also breaks the one-rig/one-exposure model — it is a
   different feature (N rigs), not a parameter of this one.
2. **Anchor-to-VCam**: excluded by decision; structurally empty (§1.3 —
   the VCam has no subjects and no body facing).
3. **Per-member track-mode** (MA-9): no use case; per-slot UI + scene
   surface for a mixed-breathing centre nobody asked for.
4. **Axial-mean (doubled-angle) group facing** (§D.2): the recorded v2
   candidate if a shot needs a group-geometric mirror axis; requires a
   degeneracy guard and an operator-predictability story.
5. **Arbitrary member sets beyond the 5 subject slots** (e.g. lighting a
   production group tag via `membersInGroup`, C6): real future value for
   crowds; needs the crowd-geometry answer §A.1 rejected for v1.
6. **Minimal-enclosing-sphere / weighted centres**: overkill at N ≤ 5;
   bounds midpoint's worst-case inefficiency vs the true 1-centre is small
   at these counts and buys duplicate-immunity + testable simplicity.
7. **Auto-frame from the VCam frustum** ("light exactly what the lens
   sees"): the motivating idea taken literally; needs lens FOV → member
   selection logic and a UX for disagreement between lens and mask.
8. **Membership cross-fade** (ease a joining member's influence in over
   ~0.5 s): would smooth MA-12's step at damping 0; new mechanism, new
   determinism surface — revisit only if in-world shots stutter.
9. **Resolved-member gizmo cue** (tint unresolved slots in the gizmo):
   cosmetic; the status text covers it.

## 9. OFF-LIMITS for the implementation brief

Everything not named in §6 is off-limits. Explicitly, even where adjacent:
**`pipeline.cpp`, all GLSL, `indra/llprimitive/*`** (the 20 m / [0,1]
clamps remain design inputs); **`scaledPoint`'s body**
(`alcinelightrig.cpp:84-108` — call it per member, change nothing);
**the intensity/EV chain and `distance_ev`**
(`alcinelightrigmodel.cpp:630`, `:656-678`, `:709-714` — byte-frozen; §5.9
is the tripwire); **the mirror line** (`alcinelightrigmodel.cpp:521-527`);
**`sanitizeGlobals` semantics** (F.2.2 extraction must be bit-identical —
§5.1 is the tripwire); **the `applySceneData` three-way split**
(`alcinelightrig.cpp:1671-1697` semantics frozen; MA-6 keys are additive
reads after it); **`lldirectorcast.{h,cpp}`** (consumed read-only — the
roster/subject API is complete for this feature; do not add rig-specific
state to it); **`llfloaterdirector.cpp`** (the Director's own subject UI is
not this delivery); **`llvoavatar.*` / `llghostavatar.*` /
`llactormover.cpp` / `llcinematiccamera.cpp` / `llprismlens.*`** (read-only
precedents); **scene version and preset versions** (stay 1); **settings.xml**
(zero new keys — the group is session state); **the FX / gobo / colour-temp
/ preset / shadow-policy code** (unchanged consumers); **`llcombobox.*`**.
