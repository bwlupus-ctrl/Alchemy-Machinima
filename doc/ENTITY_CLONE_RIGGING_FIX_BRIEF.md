# Entity-clone rigging fix — Codex brief (2026-07-23)

**You (Codex) lead this fix.** Repo `I:\alchemy-machinima`, branch `develop` (clean at
HEAD `14063eefd25`). Claude prepared this brief, will build, and reports test steps to the
user. Make the code edits directly here on `develop`. Re-defer to yourself after every fix
until 0 must-fix.

---

## THE BUG
`/ghostdress` builds an entity-route clone (`LLGhostAvatar`,
`indra/newview/llghostavatar.{h,cpp}`). In-world it renders as **exploded static parts,
non-rigged, untextured**. `/ghostverify` verdict (log 2026-07-23T23:07:30Z):

```
GHOSTVERIFY roots expected=32 recorded=32 ... structure_degraded=2 clone_failures=0
 | prims=701 pending=0 faces=1856 rigged=0 skinned_prims=0 rigged_wrong_avatar=0
```

Geometry BUILT (701 prims / 1856 faces, not pending) but **`rigged=0` and
`skinned_prims=0`** → every cloned `LLVOVolume` has `getSkinInfo()==null`. No skin/rig →
each rigged-mesh piece draws as a STATIC prim at its attachment joint (= exploded).
**Rigging is the only priority** — textures are expected to follow once the mesh/skin loads.

## ROOT CAUSE (Claude's analysis — CONFIRM, then fix)
The clone never acquires its skin because the **sculpt "extra-param" block is never copied
onto the clone**, so `isSculpted()` is false and the entire skin-load path is skipped.

Chain of evidence (current line numbers):

1. `copy_prim_state()` — `indra/newview/llghostavatar.cpp:183`. It copies scale, calls
   `dst->setVolume(src_vol->getParams(), NUM_LODS-1)` (**:199**), then copies TEs + GLTF
   overrides. It does **NOT** copy the source's `PARAMS_SCULPT` extra-param block. Note the
   ordering hazard: `setVolume` runs at :199 **before** the TE loop even runs.

2. `LLVOVolume::setVolume()` — `indra/newview/llvovolume.cpp:1156`. The mesh-skin request is
   the whole block gated on **`if (isSculpted())` (:1167)** → then
   `(volume_params.getSculptType() & MASK) == LL_SCULPT_TYPE_MESH` (:1171) →
   `gMeshRepo.loadMesh(...)` (:1182) and
   `gMeshRepo.getSkinInfo(mesh_id, this)` → `notifySkinInfoLoaded(skin_info)` (:1201-1204,
   which sets `mSkinInfo` at :1315). If `isSculpted()` is false, NONE of this runs.

3. `LLVOVolume::isSculpted()` — `indra/newview/llvovolume.cpp:3625`:
   `return getSculptParams() != nullptr;`

4. `LLViewerObject::getSculptParams()` — `indra/newview/llviewerobject.h:656`:
   `return mSculptParamsInUse ? mSculptParams.get() : nullptr;`
   → non-null **only if the `PARAMS_SCULPT` extra-param block was set AND marked in-use**.

5. The clone is created blank by `gObjectList.createObjectViewer(LL_PCODE_VOLUME, region)`
   in `alloc_local_copy` (`llghostavatar.cpp:240`) and is never sent a sim ObjectUpdate, so
   `mSculptParamsInUse` is false and `mSculptParams` is null. Therefore `getSculptParams()`
   is null → `isSculpted()` is false → skin block skipped → `mSkinInfo` null →
   `rigged=0 / skinned_prims=0`. **This exactly matches the verdict.**

Real objects get this block from the sim's ObjectUpdate ExtraParams (`PARAMS_SCULPT = 0x30`,
`LLSculptParams : public LLNetworkData`, `llprimitive.h:105,306`). The clone bypasses that
path entirely.

## FIX DIRECTION (your call on exact form)
Make the clone carry the sculpt extra-param so `isSculpted()` is true when the skin path
runs. Prime candidate: in `copy_prim_state`, **before** `dst->setVolume(...)`, copy the
source's sculpt params onto `dst`, e.g.

```cpp
if (LLSculptParams* src_sp = src->getSculptParams())
{
    dst->setParameterEntryInUse(LLNetworkData::PARAMS_SCULPT, true, false); // local-only, no sim
    dst->setParameterEntry(LLNetworkData::PARAMS_SCULPT, *src_sp, false);
}
```

Confirm the exact `setParameterEntry` / `setParameterEntryInUse` signatures + argument order
in `llviewerobject.{h,cpp}` / `llprimitive.h` (the `false`/local-only flag must NOT trigger
any sim send). Verify with the constraints below.

Things to check while confirming the fix:
- After the sculpt param is in place, does `setVolume`'s own path at :1167-1207 now populate
  `mSkinInfo` synchronously (cache-resident, same mesh UUID as the source's worn mesh) — or
  does it go async via `gMeshRepo.getSkinInfo(mesh_id, this)` and rely on
  `notifySkinInfoLoaded` firing for a client-only object? If async, ensure the callback path
  works for a local-only obj, or explicitly re-request after setVolume.
- Does `updateSculptTexture()` / `isMesh()` now behave for the clone (they also read
  `getSculptParams()`)?
- Is the ORDER right? Set the sculpt param before `setVolume` at :199 so the gate is true on
  first call. (TEs copied after is fine for skin; textures are secondary.)
- Existing verify hook: `llghostavatar.cpp:1245` already reads `vol->getSkinInfo()` — good
  signal to log against.

## HARD CONSTRAINTS (do not break)
- **Client-only ONLY.** `mIsLocalOnly` must stay set the instant the object exists, before
  parenting. `LLViewerJointAttachment::addObject` sends **ObjectDetach TO THE SIM** for
  non-local objects (`llviewerjointattachment.cpp:183-199`). Never send anything to the sim
  for the clone — any extra-param setter you use must be the local/no-send variant.
- Do NOT re-litigate what already works: `/ghosttest` PASSES (per-entity pose isolation of a
  shared skin is proven); attachment duplication + parenting are correct (ATTACH-VERIFY all
  32 roots OK); the `mIsGhostAvatar` exclusion audit is done + verified.

## SECONDARY (only after rigging works — do not let these block the rig fix)
- Skip HUD attachments in `cloneAttachmentsFrom`: `if (obj->isHUDAttachment()) continue;`
  (user confirmed clone needs no HUDs; currently HUD points 31-38 fall back onto the chest).
- `structure_degraded=2` may resolve once prims rig; don't over-engineer.

## BUILD (Claude runs this)
`cmake --build I:\alchemy-machinima\build-Windows-vs2026-os --config Release`
→ `build-Windows-vs2026-os\newview\Release\AlchemyTest.exe` (AlchemyMachinima profile).

## TEST LOOP (user runs, in a private region)
`/ghostclear` → `/ghostdress` → wait ~10s → `/ghostverify`, then read
`C:\Users\xianw\AppData\Roaming\AlchemyMachinima\logs\Alchemy.log` for the `GHOSTVERIFY`
line (tag `GhostStudio`; `/ghost*` log to file, not chat). **SUCCESS = `rigged` and
`skinned_prims` both > 0** and the clone looks like a real dressed avatar, not exploded.
