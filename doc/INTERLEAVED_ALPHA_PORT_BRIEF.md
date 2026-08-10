# Interleaved Alpha Port Brief — secondlife/viewer PR #5927 → Alchemy-Machinima

**Author of design:** Claude (Opus). **Implementer:** Fable. **Builder/reviewer:** Claude.
**Date:** 2026-08-09. **Repo:** `I:\alchemy-machinima` (branch `develop`, backup branch
`backup-pre-interleaved-alpha` @ dbc1dbf846d).

## What this is
LL PR #5927 "Depth-splice rigged attachment alpha into world alpha" replaces the stock
two-pass alpha (all rigged first, then world back-to-front) with a **single back-to-front
walk that depth-interleaves each avatar's whole ensemble into the world alpha**, keyed by a
per-avatar `mAvatarDepth`. It fixes avatar hair/attachment alpha compositing wrong against
world objects (both in front of and behind avatars). Gated by a new `RenderInterleavedAlpha`.

The **verbatim upstream diff** is at:
`C:\Users\xianw\AppData\Local\Temp\claude\I--enve\fa04e792-1193-4b82-847b-b4e93c5fbcb2\scratchpad\pr5927.diff`
Read it first — it is your source of truth for the new comparators, the enum, the merged
walk, and `calcRiggedAlphaDepth`. This brief tells you how to ADAPT it to the fork.

## Product decision (do exactly this)
- **COEXIST.** Keep the fork's existing `BDMergeAlphaAttachmentSort` 3-pass feature intact as
  a fallback. Add interleaved as a new, separate path.
- **Interleaved is the DEFAULT** — `RenderInterleavedAlpha` default value **1 (true)**, matching
  the PR.
- **Precedence at runtime:** interleaved (when its eligibility gate passes) wins → else if
  `BDMergeAlphaAttachmentSort` on → the existing 3-pass → else stock two-pass.
- **Byte-identical** to today when `RenderInterleavedAlpha=false` AND
  `BDMergeAlphaAttachmentSort=false`.

## Hard constraints
1. **Do NOT `git apply` / `git cherry-pick`.** The fork has diverged; apply every change by
   hand with the Edit tool after Reading the exact current lines.
2. **The fork's `forwardRender`/`renderAlpha` have NO GLTFSceneManager depth block.** The PR's
   hunk that adds `LLGLDepthTest gltf_depth(...)` + `GLTFSceneManager::instance().render(...)`
   inside `forwardRender` **does not exist in the fork — SKIP it entirely.** Do not introduce
   GLTFSceneManager.
3. **Preserve all fork-specific code**, especially:
   - The **visible-diffuse sidecar** guard at the top of `renderAlpha`
     (`publish_visible_diffuse`, `setIndexedDrawBufferGuardMask`, `SL_SIDECAR_ATTACHMENT`).
   - The **`AttachmentFilter` param + 3-pass BDMerge logic** (`ATTACHMENT_NONE/ONLY/ALL`,
     `BDMergeAlphaAttachmentSort`).
   - The **above/below-water reject** inside the group loop.
4. Match surrounding brace/indent style (4-space, Allman braces). Add code comments in the
   same voice as the existing `// [BDMerge]` notes.
5. Do not build. Do not touch any other feature. Report a concise list of files+functions
   changed when done.

---

## Signature reconciliation (the core adaptation)
The PR changes `bool rigged` → `EAlphaStream stream`. The fork **also** has a trailing
`AttachmentFilter filter` param on the same two functions. **Keep BOTH** — put `stream` where
`bool rigged` was, keep `filter` after it.

New signatures (header `lldrawpoolalpha.h`):
```cpp
// which alpha stream(s) a pass draws
enum class EAlphaStream
{
    WORLD,      // the distance-sorted alpha groups
    RIGGED,     // the rigged alpha groups only (depth-writing prepass)
    INTERLEAVED // both streams in one back-to-front walk; depth writes decided per group
};

void forwardRender(EAlphaStream stream = EAlphaStream::WORLD, AttachmentFilter filter = ATTACHMENT_ALL);
void renderAlpha(U32 mask, bool depth_only = false, EAlphaStream stream = EAlphaStream::WORLD, AttachmentFilter filter = ATTACHMENT_ALL);
```
Keep the existing `AttachmentFilter` enum exactly as-is (it stays above these).

Call-site mapping (old → new):
- `forwardRender(true)`                    → `forwardRender(EAlphaStream::RIGGED)`
- `forwardRender()`                        → `forwardRender(EAlphaStream::WORLD)`
- `forwardRender(false, ATTACHMENT_NONE)`  → `forwardRender(EAlphaStream::WORLD, ATTACHMENT_NONE)`
- `forwardRender(false, ATTACHMENT_ONLY)`  → `forwardRender(EAlphaStream::WORLD, ATTACHMENT_ONLY)`

---

## Edit sites

### 1. `app_settings/settings.xml`
Add the `RenderInterleavedAlpha` key exactly as in the PR diff (Boolean, Persist 1,
**Value 1**). Any reasonable alphabetical spot near other `Render*` keys is fine.

### 2. `lldrawpoolalpha.h`
- Add the `EAlphaStream` enum (above `forwardRender`, below the existing `AttachmentFilter`).
- Change `forwardRender` and `renderAlpha` signatures to the reconciled ones above.

### 3. `lldrawpoolalpha.cpp` — top of file
Add `#include <optional>` near the other includes (the PR adds it too).

### 4. `lldrawpoolalpha.cpp` — `renderPostDeferred` (currently ~line 162–251)
**a)** Near the top of the function (right after `llassert(LLPipeline::sRenderDeferred);`), add
the eligibility check + early re-sort:
```cpp
const bool interleave = LLPipeline::canUseInterleavedAlpha() &&
                        getType() == LLDrawPool::POOL_ALPHA_POST_WATER;
if (interleave)
{
    // postSort left the shared lists in legacy order; switch them to the
    // interleaved order only for the consumer that performs the merge.
    gPipeline.sortAlphaGroupsForInterleaving();
}
```
**b)** Replace the current dispatch block (the `static LLCachedControl<bool> attach_sort(...)`
+ the `if (!LLPipeline::sRenderingHUDs) { if (attach_sort && POST_WATER) {3-pass} else {2-pass} } else {HUD}`)
with this precedence structure — **keep the existing `attach_sort` cached control and its
3-pass calls, just nest them under `else`:**
```cpp
static LLCachedControl<bool> attach_sort(gSavedSettings, "BDMergeAlphaAttachmentSort", false);
if (interleave)
{
    // single pass: depth-interleave whole avatars with the distance-sorted world
    // alpha. canUseInterleavedAlpha() already limited this to the non-HUD world
    // camera; HUD, cube, reflection, shadow keep the legacy paths below.
    forwardRender(EAlphaStream::INTERLEAVED);
}
else if (!LLPipeline::sRenderingHUDs)
{
    if (attach_sort && getType() == LLDrawPool::POOL_ALPHA_POST_WATER)
    {
        forwardRender(EAlphaStream::WORLD, ATTACHMENT_NONE);  // pass 1: SIM non-rigged
        forwardRender(EAlphaStream::RIGGED);                  // pass 2: rigged
        forwardRender(EAlphaStream::WORLD, ATTACHMENT_ONLY);  // pass 3: worn attachment non-rigged
    }
    else
    {
        forwardRender(EAlphaStream::RIGGED);   // first pass: rigged only, to depth
        forwardRender(EAlphaStream::WORLD);    // second pass: regular non-rigged
    }
}
else
{
    forwardRender(EAlphaStream::WORLD);        // HUD: single non-rigged pass
}
```
Keep the existing `// [BDMerge]` comment block above this, adjusting its wording only if needed.

### 5. `lldrawpoolalpha.cpp` — `forwardRender(EAlphaStream stream, AttachmentFilter filter)` (currently ~253)
- Change signature.
- At the top of the body add: `const bool rigged = (stream == EAlphaStream::RIGGED);`
  Then the rest of the body can keep using the local `rigged`.
- Keep the existing `write_depth` computation as-is (it now reads the local `rigged`).
- Change the depth-test line so interleaved turns pass-wide depth writes OFF (per-group control
  happens inside `renderAlpha`):
  ```cpp
  // in interleaved mode depth writes are decided per group inside renderAlpha
  LLGLDepthTest depth(GL_TRUE, (write_depth && stream != EAlphaStream::INTERLEAVED) ? GL_TRUE : GL_FALSE);
  ```
- Change the `renderAlpha(...)` call to pass `stream` instead of `rigged`:
  `renderAlpha(getVertexDataMask() | ..., false, stream, filter);`
- The `renderDebugAlpha` guard `if (!rigged && ... (filter == ATTACHMENT_ALL || ATTACHMENT_ONLY))`
  stays as written (local `rigged` is false for INTERLEAVED, so the highlight fires once — correct).
- **Do NOT add any GLTFSceneManager block.**

### 6. `lldrawpoolalpha.cpp` — `renderAlpha(U32 mask, bool depth_only, EAlphaStream stream, AttachmentFilter filter)` (currently ~582)
This is the delicate one. Preserve the sidecar guard block at the very top unchanged.
- Change signature. Immediately after `LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;` (i.e. before
  the sidecar guard is fine, or right after it) add:
  ```cpp
  const bool merged = (stream == EAlphaStream::INTERLEAVED);
  bool rigged = (stream == EAlphaStream::RIGGED); // flips per group in the merged walk
  ```
- Replace the single-stream iterator setup
  ```cpp
  LLCullResult::sg_iterator begin;
  LLCullResult::sg_iterator end;
  if (rigged) { begin = ...beginRiggedAlphaGroups(); end = ...endRiggedAlphaGroups(); }
  else        { begin = ...beginAlphaGroups();       end = ...endAlphaGroups(); }
  ```
  with the dual-iterator setup from the PR:
  ```cpp
  LLCullResult::sg_iterator iter = nullptr;
  LLCullResult::sg_iterator iter_end = nullptr;
  LLCullResult::sg_iterator rigged_iter = nullptr;
  LLCullResult::sg_iterator rigged_end = nullptr;
  if (merged || rigged)   { rigged_iter = gPipeline.beginRiggedAlphaGroups(); rigged_end = gPipeline.endRiggedAlphaGroups(); }
  if (merged || !rigged)  { iter = gPipeline.beginAlphaGroups();              iter_end = gPipeline.endAlphaGroups(); }
  ```
- Keep the fork's `water_height` / `above_water` setup that follows.
- Just before the loop, add the PR's pass-wide depth-write precondition + the per-group depth
  state holder:
  ```cpp
  const bool write_depth_always = LLDrawPoolWater::sSkipScreenCopy ||
                                  LLPipeline::sImpostorRenderAlphaDepthPass ||
                                  getType() == LLDrawPoolAlpha::POOL_ALPHA_PRE_WATER;
  std::optional<LLGLDepthTest> depth_state;   // merged mode: re-emplaced only on write-state change
  bool depth_state_writes = false;
  ```
- Replace the loop header `for (LLCullResult::sg_iterator i = begin; i != end; ++i) { ... LLSpatialGroup* group = *i;`
  with the PR's merged walk:
  ```cpp
  while (iter != iter_end || rigged_iter != rigged_end)
  {
      LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("renderAlpha - group");

      if (merged)
      { // take the farther of the two stream heads; an ensemble's groups share
        // one avatar depth, so each avatar drains contiguously (rigged run first,
        // then its unrigged attachment groups, which composite over it).
          if (rigged_iter == rigged_end)      { rigged = false; }
          else if (iter == iter_end)          { rigged = true; }
          else
          {
              F32 rigged_depth = (*rigged_iter)->mAvatarDepth;
              F32 world_depth  = (*iter)->worldAlphaDepth();
              if (rigged_depth != world_depth)
              {
                  rigged = rigged_depth > world_depth;
              }
              else
              {
                  const LLVOAvatar* world_av  = (*iter)->mAvatarp;
                  const LLVOAvatar* rigged_av = (*rigged_iter)->mAvatarp;
                  rigged = !rigged_av || !world_av || world_av == rigged_av ||
                           std::less<const LLVOAvatar*>()(rigged_av, world_av);
              }
          }
      }

      LLSpatialGroup* group = rigged ? *rigged_iter++ : *iter++;
      llassert(group);
      llassert(group->getSpatialPartition());
      ...
  ```
  (The remainder of the loop body — `mRenderByGroup`/`isDead` check, `bridge`/`ext`, the
  above/below-water reject, the emissive vectors, the `draw_info = rigged ? PASS_ALPHA_RIGGED
  : PASS_ALPHA` selection, the `(bool)params.mAvatar != rigged` skip, and the `AttachmentFilter`
  skips — all stay exactly as they are; they already key off the local `rigged`, which is now
  correct per-group.)
- Right after the above/below-water reject block and **before** `static std::vector<LLDrawInfo*> emissives;`,
  insert the PR's per-group depth guard:
  ```cpp
  // merged mode: stamped rigged groups write depth so attachment-order layering
  // holds; an unstamped group has no defined position in the rigged order and must
  // not depth-reject geometry behind it (it still blends). Non-merged passes keep
  // the caller's depth state (set in forwardRender).
  if (merged)
  {
      bool write_depth = write_depth_always || (rigged && group->mAvatarp != nullptr);
      if (!depth_state || write_depth != depth_state_writes)
      {
          depth_state.emplace(GL_TRUE, write_depth ? GL_TRUE : GL_FALSE);
          depth_state_writes = write_depth;
      }
  }
  ```
- **Watch the AttachmentFilter skip:** the fork skips on `filter == ATTACHMENT_NONE/ONLY`.
  In INTERLEAVED mode the caller always passes `ATTACHMENT_ALL`, so those skips are no-ops —
  leave them as-is. Do not gate them on `merged`.

### 7. `llspatialpartition.h`
Apply the PR's changes verbatim (they land cleanly — the fork already has `mAvatarp`,
`mRenderOrder`, `CompareRenderOrder`, `CompareDepthGreater`):
- Add `#include <functional>`.
- On `LLSpatialGroup`: add `F32 mAvatarDepth = 0.f;` (next to `mAvatarp`/`mRenderOrder`),
  add `worldAlphaDepth()`, add `CompareWorldAlphaDepth`, add `CompareDepthRenderOrder`, and
  update `CompareRenderOrder` to use `std::less<const LLVOAvatar*>()` for the pointer tie-break.
- On `LLSpatialBridge`: add `LLVOAvatar* mAvatarp = nullptr; U32 mRenderOrder = 0; F32 mAvatarDepth = 0.f;`
  (the bridge does not currently have these — add all three).

### 8. `llvoavatar.h` / `llvoavatar.cpp`
- `.h`: declare `F32 calcRiggedAlphaDepth() const;` (next to `idleUpdateMisc`, as the PR does).
- `.cpp`: add the `calcRiggedAlphaDepth()` body verbatim from the PR (uses `mLastAnimExtents`,
  `LLViewerCamera`).
- `.cpp` `idleUpdateMisc`: **refactor the attachment stamp.** Currently (rigged-only branch):
  ```cpp
  LLSpatialGroup* group = attached_object->mDrawable->getSpatialGroup();
  if (group)
  { //set draw order of group
      group->mAvatarp = this;
      group->mRenderOrder = draw_order++;
  }
  ```
  Change to stamp the **bridge** (available from the `bridge = attached_object->mDrawable->getSpatialBridge();`
  re-fetch just above), gated so non-HUD unrigged attachments are included too, and carry depth.
  - Add, once, before the attachment loop (near `U32 draw_order = 0;`):
    `const F32 rigged_depth = calcRiggedAlphaDepth();`
  - Move the stamp out of the `else { rigged }` sub-branch so it also runs for non-rigged
    worn attachments, and replace it with:
    ```cpp
    // stamp the attachment's draw order onto its bridge; LLPipeline::postSort fans it
    // out to the bridge's alpha groups, sorting the wearer's whole ensemble at one
    // avatar depth. HUD alpha sorts in HUD space, so unrigged HUDs stay unstamped.
    if (rigged || !attached_object->isHUDAttachment())
    {
        bridge->mAvatarp = this;
        bridge->mRenderOrder = draw_order++;
        bridge->mAvatarDepth = rigged_depth;
    }
    ```
    Place this inside the existing `if (bridge) { ... }` block (the re-fetched bridge at
    ~line 3149), after the rigged/non-rigged `updateMove` calls. The old direct-group stamp is
    removed. NOTE: `bridge` here is `attached_object->mDrawable->getSpatialBridge()` — confirm
    that is the same handle whose groups postSort walks (it is: postSort reads
    `group->getSpatialPartition()->asBridge()`).

### 9. `llcontrolavatar.cpp` — `idleUpdate` (~line 370)
Apply the PR's animesh-bridge stamp verbatim into the `else { LLVOAvatar::idleUpdate(agent,time); ... }`
branch:
```cpp
// stamp the animesh bridge like LLVOAvatar::idleUpdateMisc stamps attachments.
// Worn animesh is skipped (the wearer stamps this bridge). Bridge fetched from the
// drawable -- mControlAVBridge can go stale.
if (mRootVolp && mRootVolp->mDrawable && !getAttachedAvatar())
{
    LLSpatialBridge* bridge = mRootVolp->mDrawable->getSpatialBridge();
    if (bridge)
    {
        bridge->mAvatarp = this;
        bridge->mRenderOrder = 0;
        bridge->mAvatarDepth = calcRiggedAlphaDepth();
    }
}
```

### 10. `pipeline.cpp` / `pipeline.h`
- `pipeline.h`: declare `static bool canUseInterleavedAlpha();` (next to `isWaterClip()`) and
  `void sortAlphaGroupsForInterleaving();` (next to `endRiggedAlphaGroups()`).
- `pipeline.cpp`: add both function bodies verbatim from the PR (`canUseInterleavedAlpha` reads
  the `RenderInterleavedAlpha` cached control and excludes HUD/shadow/cube/non-world-camera;
  `sortAlphaGroupsForInterleaving` runs the two interleaved sorts).
- `pipeline.cpp` `postSort`: add the bridge→group fan-out. In the fork, the alpha-store block
  fetches `bridge` **inside** the `if (alpha != group->mDrawMap.end())` block (~line 4807).
  Hoist a bridge fetch to the **top** of the `if (hasRenderType(RENDER_TYPE_PASS_ALPHA))` block
  and add the fan-out **before** the `mDrawMap.find(PASS_ALPHA)` line:
  ```cpp
  LLSpatialBridge *bridge = group->getSpatialPartition()->asBridge();

  // fan the attachment's stamp (LLVOAvatar::idleUpdateMisc) out from the bridge to
  // every visible alpha group of the linkset; the shared mAvatarDepth keys the
  // wearer's ensemble as one block in the interleaved walk.
  if (bridge && bridge->mAvatarp)
  {
      group->mAvatarp = bridge->mAvatarp;
      group->mRenderOrder = bridge->mRenderOrder;
      group->mAvatarDepth = bridge->mAvatarDepth;
  }
  ```
  Then remove the now-duplicate inner `LLSpatialBridge *bridge = ...;` declaration inside the
  `if (alpha != end())` block and just reuse the hoisted `bridge`. **Do not change** the final
  `std::sort(... CompareDepthGreater())` / `std::sort(... CompareRenderOrder())` at the bottom
  of postSort — those remain the legacy baseline; interleaved re-sorts later via
  `sortAlphaGroupsForInterleaving()`. (You may update the comment to note that.)

---

## Self-check before reporting done
- [ ] `RenderInterleavedAlpha` default is **1**.
- [ ] With both settings false, the dispatch reduces to `forwardRender(RIGGED); forwardRender(WORLD);`
      (stock) — byte-identical.
- [ ] `BDMergeAlphaAttachmentSort` 3-pass still reachable when interleaved is off.
- [ ] No GLTFSceneManager code added anywhere.
- [ ] Sidecar guard, AttachmentFilter skips, and water reject all still present in `renderAlpha`.
- [ ] `LLSpatialBridge` gained `mAvatarp` / `mRenderOrder` / `mAvatarDepth`.
- [ ] `idleUpdateMisc` stamps the **bridge** (not the group) and includes non-HUD unrigged
      attachments; `calcRiggedAlphaDepth()` computed once per detailed update.
- [ ] `postSort` fans bridge→group before the PASS_ALPHA find; no duplicate `bridge` decl.
- [ ] Every `forwardRender(...)` / `renderAlpha(...)` call site compiles against the new signatures.
- [ ] `#include <optional>` (lldrawpoolalpha.cpp) and `#include <functional>` (llspatialpartition.h) added.

Report: list each file touched with the functions changed, and flag anything in the fork that
didn't match this brief so Claude can review before building.
