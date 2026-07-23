# Scene-Lit Clone — Entity Route: Deep-Dive Brief for an External Design Pass

**Audience:** an external reasoning model (e.g. OpenAI GPT-5 / o-series) asked to produce a
concrete, sequenced execution plan. Self-contained — you do **not** need repo access to reason
about it, but file:line pointers are included for a repo-aware tool.

**Repo:** `I:\alchemy-machinima` (fork of Alchemy Viewer, a Second Life client; branch `develop`).
C++ / OpenGL deferred renderer. Windows, CMake + vcpkg, VS2026.

---

## 0. What we want you to produce

A grounded, prioritized **execution plan** that takes the *existing, committed-but-untested*
`LLGhostAvatar` code from where it is now to "shipping scene-lit entity clones." Specifically:

1. **In-world validation sequence** for what already exists (`/ghosttest`, `/ghostdress`,
   `/ghostverify`) — exact run order, what each PASS/FAIL actually proves, and what it does NOT.
2. **A prioritized `isGhostAvatar()` exclusion audit** — enumerate the concrete sites that must
   test `isGhostAvatar()` before a ghost is ever spawned in a real session, the correct guard at
   each, and the safety-critical ordering (things that can hit the *server*, teleport the *real
   agent*, or write *persistent* state must come first).
3. **The path to Ghost Studio integration** — how to swap the current batch-harvest clone for the
   entity clone behind a setting, coexisting with (not breaking) the existing overlay/deferred
   paths, plus the `LLActorMover` driving hook.
4. **A risk register + per-milestone go/no-go acceptance gates.**
5. **Independent critique** — tell us where the reasoning below is wrong, overconfident, or
   missing something. We would rather hear it now.

Also: **verify implementation completeness** — the interface (below) exists and the file is ~57 KB,
but we have not confirmed every method is fully implemented vs. stubbed. Flag anything that reads
as a stub.

---

## 1. Goal & why this architecture

**Goal:** a Ghost Studio "clone" — a local, non-networked copy of an avatar placed in the scene —
that is visually **indistinguishable from a second real avatar**: lit by the same sun/lights,
casting and receiving shadows, fogged, tonemapped, color-graded, ReShade-visible, and rendered with
**all** material types (legacy Blinn-Phong + GLTF PBR), rigged **and** non-rigged, including the
system body. It must also be drivable as an "actor" (walk paths, hit marks) from one source avatar =
a whole cast for a solo creator.

**Why the entity route (this is the crux).** The *previous* approach harvested the source avatar's
draw batches and re-drew them ourselves in an overlay/deferred pass. That means re-implementing the
viewer's render pipeline piece by piece — rigged-solid, rigged-blend, rigged-glow, static-solid,
static-blend, static-glow, system-body — each its own code path that must independently get
materials/shaders/palettes/lighting right. Parity there is ~a dozen sub-parities; fixing one
regresses another. A dedicated read-only audit tool (`/clonefidelity`) recently PROVED the harvested
**data** is 100% correct (170/170 rigged surfaces exact) while the deferred **render** is still
broken — confirming the gap is in our re-drawing, not the data.

**The entity route reaches parity by construction:** spawn a real client-only `LLVOAvatar` subclass
that wears the source's appearance and attachments, and let it render through the **real** avatar
pipeline. Rigged + non-rigged + system body + glow + lighting + shadows all come for free because it
IS a real avatar. No per-category ghost code, ever. Three prior in-tree deep dives + two independent
adversarial code reviews concluded this is the only path to an actor-grade clone.

---

## 2. Current state — what already exists in the tree (grounded, committed)

`indra/newview/llghostavatar.{h,cpp}` (~57 KB .cpp) is **committed** (in `e5199b215e9`), tree clean,
**UNTESTED in-world**. The class interface (verbatim intent):

```cpp
class LLGhostAvatar : public LLVOAvatar   // NOT LLControlAvatar — see landmines
{
    LLGhostAvatar(const LLUUID& id, const LLPCode pcode, LLViewerRegion* regionp);
    virtual void initInstance();

    void setGhostPosition(const LLVector3& pos_agent);   // placed by fiat, needs live region
    void ghostSlamPosition(const LLVector3& pos_agent);  // slamPosition MINUS the agent-teleport line
    bool cloneAppearanceFrom(LLVOAvatar* source);        // shape + baked textures (BY VALUE); mandatory or invisible
    S32  cloneAttachmentsFrom(LLVOAvatar* source);       // duplicate worn attachments as client-only objects
    void releaseClonedAttachments();
    void markForDeath();                                 // deferred death (never markDead mid-pipeline)
    virtual void idleUpdate(LLAgent&, const F64&);

    // Test harness (chat commands):
    static bool runPaletteIsolationTest();   // /ghosttest   — MILESTONE 1 gate
    static bool spawnDressedGhost();         // /ghostdress  — MILESTONE 2 (appearance + attachments)
    static void verifyClonedAttachments();   // /ghostverify — deferred face-level acceptance
    static void clearTestGhosts();           // /ghostclear
    // + transactional clone state: mCloneExpectedRoots / mCloneFailures so a partial
    //   clone can NEVER report PASS (a dropped attachment suppresses PASS outright).
};
```

- `isGhostAvatar()` is a base `LLVOAvatar` accessor returning `mIsGhostAvatar` (set in the ghost
  ctor). `mIsDummy` is deliberately left **false** (taking the full avatar path is the whole point).
- Wired chat commands: `/ghosttest`, `/ghostclear` (and `/ghostdress`, `/ghostverify`) in
  `alchatcommand.cpp`.

**What is NOT done (the real remaining work):**
- **`isGhostAvatar()` exclusion audit — essentially unstarted.** The flag + accessor exist, but
  `isGhostAvatar()` is currently tested at **zero** real call sites. Every `mIsDummy==false`
  consequence below is therefore currently UNGUARDED.
- **No `LLActorMover` driving** (the "actor" milestone).
- **No Ghost Studio integration** — real placed clones still use the old batch path; the entity
  code is reachable only via the `/ghost*` test harness.
- **In-world validation of everything above = zero.** It compiles; it has never run in a session.
- The earlier, more elegant **capability-split** (`EAvatarKind` splitting `mIsDummy` into
  `usesSceneRendering` / `hasServerAgentIdentity` / `participatesInWorldQueries` / `ownsMotionState`)
  was **reverted**; current code uses the simpler single `mIsGhostAvatar` flag. The split is
  considered re-landable later but is NOT present now.

---

## 3. Decided architecture + already-answered research (please don't re-derive)

All verified against this codebase in prior dives:

- **Independent posing of a shared mesh already works.** `mMatrixPaletteCache` is a **per-avatar**
  member keyed by skin hash (`llvoavatar.h`); joint numbering (`mJointNums`) is **avatar-independent**
  (deterministic DFS of `avatar_skeleton.xml`, `llavatarappearance.cpp:649`). So two avatars can pose
  the SAME shared `LLMeshSkinInfo` differently in the same frame — this is Milestone 1's premise, and
  `/ghosttest` exists to prove it in practice (spawn 2 ghosts, pose differently, assert distinct
  palette bytes for one shared skin).
- **Rigged geometry ignores its model matrix** (`if (rigged) model_mat = nullptr;`
  `llvovolume.cpp:~5504`). So a rigged clone's placement **is** its skeleton root position —
  `mRoot->setWorldPosition()` and the stock renderer needs zero changes. (This is also why the old
  overlay had to fight the pipeline with a custom placement shader.)
- **Appearance copy is cheap but must be BY VALUE.** `mLastProcessedAppearance` +
  `applyParsedAppearanceMessage(contents, slam=true)` copies shape + baked textures. But it holds
  **source-owned `LLVisualParam*` pointers** — handing them to another avatar would deform the real
  one. `copyAppearanceFrom` re-resolves each param by id (value copy). `cloneAppearanceFrom` wraps it.
  Without a first appearance message the avatar renders **invisible** (`llvoavatar.cpp:~5340`).
- **Attachment duplication is object duplication, not appearance reconstruction.** Precedent:
  `LLLocalMeshMgr` already spawns client-only `LLVOVolume`s and attaches them to an avatar joint
  (`spawnLinkset`, `attachPreviewToAvatar`). Mesh assets / `LLVolume`s / `LLMeshSkinInfo`s / textures
  come from shared caches; only `LLVOVolume`/`LLDrawable`/`LLFace`/`LLVertexBuffer` duplicate. Cost ≈
  one extra avatar per clone (scales badly past ~5 — fine for a small cast).
- **Geometry canNOT be shared:** `LLFace::mAvatar` is a single raw pointer written at rebuild
  (`llvovolume.cpp:~6140`); aliasing corrupts the source. Duplication is mandatory.

---

## 4. Known landmines (respect these — they are load-bearing)

1. **`slamPosition()` teleports the REAL agent.** `LLVOAvatar::slamPosition()` opens with
   `gAgent.setPositionAgent(...)` (`llvoavatar.cpp:~4038`). `LLUIAvatar` gets away with it (masked).
   A ghost must use `ghostSlamPosition()` (that function minus that line). **Never** call base
   `slamPosition()` on a ghost.
2. **`setPositionAgent()` dereferences `getRegion()` unguarded** → null-region ghost crash. Guard
   every placement call.
3. **`mIsDummy == false` blast radius (currently UNGUARDED — this is the main risk).** With a
   non-dummy synthetic avatar, these fire against a fake UUID unless `isGhostAvatar()` gates them:
   - world enumeration: `LLWorld::getAvatars()`/`getAvatar()` → radar / minimap / tracking / RLVa
   - name cache + name-tag paths assume an account identity
   - **persistent** mute / render-policy state written against the synthetic UUID (survives session)
   - autotune + nearby-avatar counts include it (perf accounting, LOD scheduling)
   - footstep + typing sounds fire
   - impostor / jellydoll culling can hide/replace it
   - **missing appearance can trigger a SERVER avatar-texture request** (network side effect!)
   - the **default motion controller runs and will OVERWRITE a recorded/posed skeleton** unless
     disabled (LLControlAvatar disables default motions; a ghost must too)
   - `sInstances` registration happens in the BASE ctor before subclass init; create/destroy must
     stay on the viewer thread and outside any avatar-list traversal
4. **Do NOT derive from `LLControlAvatar`.** `mIsDummy && isControlAvatar()` →
   `releaseMeshData()` HIDES ALL ATTACHMENTS once ≥10 avatars are present; `mIsDummy` also early-
   returns `updateTextures()` (body textures never boosted). `LLGhostAvatar : LLVOAvatar` with
   `mIsDummy=false` avoids both — but then owns the whole blast radius in item 3.
5. **Deferred death only.** Never `markDead()` from inside a graphics-pipeline traversal; mirror
   LLControlAvatar's `markForDeath → idleUpdate → markDead`. Region-cross + logout cleanup hooks
   required (mirror `LLLocalMeshMgr::despawnObjectsInRegion`), or clones outlive their session.
6. **Independent scaling (a later subtest, not a milestone gate):** joint scale has **no single
   owner** — `LLPolySkeletalDistortion::apply` adds shape deltas additively, attachment overrides
   REPLACE scale, animations can write scale. So scale must be a **multiplicative layer applied
   AFTER pose/appearance resolution, composed into the final palette** — never by mutating authored
   joint scales. Also `mBodySize` width/depth are hardcoded constants → recompute body size +
   extents AFTER scaling. Ship one **uniform** slider only (non-uniform needs inverse-transpose
   normals the skinning shader doesn't build).
7. **`applyOverride` currently paints the render root only, not the drawable position**
   (`llvoavatar.cpp:~4914`) → culling/picking/impostor/LOD evaluate at the wrong place. For an
   entity the REAL position must be set. A latent bug the entity forces us to fix.

---

## 5. Milestones & where we are

| # | Milestone | Command | Status |
|---|-----------|---------|--------|
| M1 | Palette isolation — 2 ghosts pose one shared skin differently, distinct palettes | `/ghosttest` | **coded, untested** |
| M2 | Dressed ghost — 1 ghost with appearance **and** duplicated attachments, lit + shadowed | `/ghostdress` | **coded, untested** |
| M2v | Face-level attachment acceptance (deferred; faces schedule async so verify is a later pass) | `/ghostverify` | **coded, untested** |
| M3 | Attachment cloning (folded into `cloneAttachmentsFrom`, transactional) | — | **coded, untested** |
| M4 | `LLActorMover` drives the ghost along paths (the "actor") | — | **not started** |
| INT | Replace Ghost Studio's batch clone with the entity clone behind a setting | — | **not started** |
| SAFE | `isGhostAvatar()` exclusion audit at the §4.3 sites | — | **not started (critical)** |

**The decisive gate (do this first):** `/ghosttest` PASS proves pose→palette computation is
entity-local for a shared skin. `/ghostdress` PASS = a correctly-shaped body **lit with a real
shadow**, wearing cloned mesh, standing in the scene. That single visual result validates deferred
lighting / shadows / tonemap / ReShade for the entire architecture. It does NOT prove: async mesh
completion, LOD/impostor transitions, teleport/region-cross cleanup, friends-only mode, or absence
of the §4.3 side effects — those are separate gates.

---

## 6. Specific deliverables (restating §0 with detail)

1. **Validation runbook:** the exact `/ghosttest → /ghostdress → /ghostverify` sequence, the PASS
   criteria for each, the negative controls, and a table of "what a PASS proves / does NOT prove."
2. **`isGhostAvatar()` exclusion audit, prioritized:** for each §4.3 behavior, the specific guard
   and why, ordered by blast radius (SERVER-touching + REAL-AGENT-touching + PERSISTENT-state first;
   cosmetic last). Call out any site where a guard is insufficient and a deeper change is needed.
3. **Ghost Studio integration plan:** how `ALGhostStudio` instances switch from the batch clone to
   an `LLGhostAvatar` behind a setting; coexistence so the existing overlay/deferred paths are
   byte-unaffected when off; lifecycle (spawn on enable, `markForDeath` on disable / region-cross /
   logout); and the `LLActorMover` driving hook (M4) — noting `LLActorMover::Path` is already keyed
   by arbitrary UUID (the prop mover proves non-avatars drive on it).
4. **Risk register + go/no-go gates** per milestone.
5. **Independent critique** of §§1–5.
6. **Stub check:** flag any method in `llghostavatar.cpp` that looks unimplemented.

---

## 7. Key files & entry points

- `indra/newview/llghostavatar.{h,cpp}` — the entity clone (this brief's subject)
- `indra/newview/llvoavatar.{h,cpp}` — base avatar; `isGhostAvatar()`/`mIsGhostAvatar`, appearance,
  palette cache, `slamPosition`, `applyParsedAppearanceMessage`, the §4.3 sites live here + in
  `llworld.cpp`, `llviewerobjectlist.cpp`, radar/name-cache/autotune modules
- `indra/newview/alchatcommand.cpp` — `/ghosttest /ghostdress /ghostverify /ghostclear` harness
- `indra/newview/lllocalmesh.cpp` — the client-only-object attach recipe (`spawnLinkset`,
  `attachPreviewToAvatar`)
- `indra/newview/llactormover.{h,cpp}` — path/actor driving (Ghost Studio + prop mover live here)
- `indra/newview/alghoststudio.cpp` — Ghost Studio instances (placement, freeze, styles)
- `indra/newview/llclonefidelityaudit.cpp` — the read-only parity audit that proved the batch
  clone's data is correct (context for why we're pivoting)

---

## 8. Open questions we'd like an opinion on

1. **Sequencing:** validate the existing M1/M2 harness in-world FIRST, or do the `isGhostAvatar()`
   safety audit first (so testing in a live region can't leak side effects)? We lean audit-first for
   anything that touches server/persistent state, harness-only region otherwise.
2. **Re-land the capability-split** (`EAvatarKind`) now for clean central control, or keep the single
   `mIsGhostAvatar` flag + scattered guards for M-series and split later?
3. **System body vs. attachments:** the entity gives us the system body free — but is the appearance
   + attachment clone faithful enough (baked textures, bakes-on-mesh / universal wearables, alpha
   masks, BOM) that we don't reintroduce a per-feature grind? Where are the likely fidelity gaps?
4. **Cost governance:** at what visible-clone count do we force impostors, and does forcing an
   impostor on a ghost re-introduce any of the §4.3 hazards?

---

## 9. Known boundary — LSL scripts & dynamic behavior

A clone is a **local, client-only** copy: its attachments have **no simulator presence**, so **no LSL
scripts run on it**. LSL executes server-side; the viewer only ever receives script *results*. You
cannot run the scripts without a real server-side object (asset + permissions + a rez), so this is
inherent to ANY local clone (batch or entity), not specific to this route.

The clone is a **snapshot at clone time.** LOST: anything a script changes over time — scripted
movement/rotation/resize, color/glow/texture/alpha cycling, event-driven particles, sounds,
hovertext, mesh/anim swaps, and touch/collision/timer/sensor reactivity.

PRESERVED **iff the clone copies the prim params** — these are viewer-side effects, not scripts, so a
real `LLVOVolume` clone replays them off the viewer clock: **flexi**, **texture animation**
(`llSetTextureAnim`), **local lights / steady glow**, and **particles** (the particle source data must
be replicated). The entity route is strictly better here than the old batch harvest, which preserved
none of these.

**Action for the plan:** ensure `cloneAttachmentsFrom` copies flexi params, the full texture entry
(incl. texture-anim), light params, and particle source data; and treat scripted *dynamic* behavior
(e.g. a scripted animated costume, spinning parts) as an explicit **non-goal**, re-created only via
recorded clips if ever needed. Flag this boundary in any user-facing description of the feature.

---

*Provenance: distilled from three in-tree deep dives + two independent adversarial code reviews
(all source-grounded) and a fresh audit of the current committed state on `develop`.*
