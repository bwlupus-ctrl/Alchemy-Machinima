# [AvatarLightResponse] adversarial code review (Opus, independent of Codex)

Scope: uncommitted diff vs HEAD 1ecdbbdc35c plus the new files. Read-only review. Nothing was built.

## Verdict: NO P0. One P1 (fix before build). The rest are P2.

The approach holds up. Using the Actor FX draw ownership, a separate uniform pair and the normal.b carrier is sound, and I could not find a place where the OFF path stops being identical to HEAD.

## Compile-safety pass (no errors found)
- **C++.** Every symbol in the new code resolves:
  - `findInstanceByRuntime`, `getInstance` const/non-const, `resolveEntityClone`, `getInstances`, `BACKING_ENTITY_CLONE` are public.
  - `mNeedsImpostorUpdate` is public and mutable. `getAttachedAvatar() const` exists on LLControlAvatar.
  - `llsd::inMap`, `F64SecondsImplicit` to F64, `std::hash<LLUUID>`, `LLSliderCtrl::setValue(F32)`, `ensure_approximately_equals_range` all exist.
  - `upload_alr` sits in lldrawpool.cpp's anonymous namespace, where `pipeline.h`, `llviewercontrol.h` and `llagentdata.h` are included.
  - `policy_info` and `info` are `LLDrawInfo*`. `face->mDrawInfo` converts from LLPointer.
  - Only static functions call private members (`keepsLive` calls `ensureTable`/`findSlot`), and that is legal. I found no unreferenced uninitialised locals (C4101) and no Windows-macro identifiers.
  - The CMake source, header and test entries are all present.
- **GLSL.**
  - The module is attached only inside `if (features->hasActorFx)`, never for shadow-only programs.
  - Every `HAS_ACTOR_FX` permutation has `hasActorFx=true`. The alpha HUD variant drops both.
  - All `alr*` prototypes and calls sit inside `#ifdef HAS_ACTOR_FX`. The Keep prototypes are unconditional, and deferredUtil is linked wherever they are used.
  - There is no duplicate definition: the old names forward with 1.0, and the Rim wrapper is defined after RimKeep but before the rimless caller.
  - The actorFx helper comes after `actorFxCoverMode` (:478), `actorFxFlatSensorLook` (:488) and `actorFxPbrStrength` (:505). The fallback returns 0.
  - `env_intensity` in fullbrightShinyF is a local, not a uniform. `gb` is in scope in all four light shaders. `ghostLook` is an int. `color` is vec3 or vec4 correctly at each debug override.

## P1
**P1-1: the "self" entry is dropped on load and then erased from disk.**
- `llstartup.cpp:1206-1207` calls `loadFromFile()` in `STATE_LOGIN_CLEANUP`. `gAgentID` is only set later, in `process_login_success_response` (`llstartup.cpp:3881`).
- So `keyFromString("self")` returns null (`alavatarlightresponse.cpp:95-100`), and `importMerge` `continue`s past it (:619-622). The "You" entry never loads.
- The first `saveIfPersist()` then rewrites the file without it. That save happens on any slider mouse-up, on preset, and on simply closing the floater (`alfloateravatarlightresponse.cpp:150`). The loss is permanent, and T11 fails for You.
- (The spec anchor itself was wrong here.)
- **Fix:**
  - Move the load to after `process_login_success_response()` succeeds (after `llstartup.cpp:1472`), or into `STATE_WORLD_INIT`.
  - Also make `saveToFile()` refuse to write while `gAgentID.isNull()`.
  - Optionally keep an unresolved "self" LLSD blob and merge it once the agent id is known.

## P2
1. **A worn animesh with its own entry loses its trim at distance.**
   - `LLControlAvatar::isImpostor()` follows the wearer (`llcontrolavatar.cpp:741-748`).
   - `keepsLive(wearer)` checks only the wearer's own slot (`alavatarlightresponse.cpp:149-168`).
   - So when the wearer is not adjusted but is impostored, the pet is drawn inside the wearer's bake, which is identity. T8 and T10 then fail for pets.
   - Fix: have `keepsLive` return true for a wearer when any worn control avatar resolves to a non-identity slot, for example via a "live wearers" set built in `ensureTable`.
2. **The attachment context menus target the wearer, not the worn animesh.**
   - `handle_avatar_light_response` (`alviewermenu.cpp:1005-1017`) calls `find_avatar_from_object`, which walks attachments up to the avatar (`llviewermenu.cpp:9384-9390`).
   - Fix: prefer `primary->getControlAvatar()` when it is non-null.
3. **Untrusted input and scene I/O.**
   - `importMerge` has no cap on the number of entries or the label length. A hostile scene or file gets an O(N) table rebuild every frame, O(N·avatars) `dirtyImpostors` calls, and is persisted forever.
   - `applySceneData` writes the file on every scene apply, even when nothing was merged (`lldirectorcast.cpp` ~2417-2422).
   - Fix: cap entries (for example 512) and labels (for example 64 characters), and only save when `any` is true.
4. **Floater usability.**
   - The 1 Hz `refreshRows` rebuild calls `refreshEditors()` (:373-374). If avatars enter or leave during a drag, it resets the slider mid-drag. Fix: skip that call while a slider has mouse capture.
   - `onApplyPreset` reads `getValue().asInteger()`. An empty combo gives 0, which means Reset. Fix: select item 0 in `postBuild`.
   - Brightness does not show the specified "+0.00 EV" format.
5. **Glow coverage.** The legacy emissive mask (`frag_data[0].a = getEmissive(...)` in materialF.glsl and materialIndexedF.glsl :362-368) is not scaled by Glow. Either document it or scale it. Separately, the legacy glow alpha `a *= g` above 1 may clamp in a unorm glow target, so Glow above 100 % may do nothing on legacy glow.
6. **Known gaps.** These are acceptable but should be written down in the doc:
   - `lighting/lightAlphaMask*F.glsl` is reached only through `gAvatarProgram` and `gObjectAlphaMaskNoColorProgram` in the non-deferred `beginRigid`/`beginSkinned` path (`lldrawpoolavatar.cpp:695/777`, commented "preview only"). Confirm that with T1.
   - Actor FX Cover replays of system bodies go through `gAvatarActorGhostProgram` and get identity, by design.
7. **Instrument.** Overlay clones report `mVisible=-1`, so the log verdict "INCONCLUSIVE-NOT-VISIBLE" when there are 0 uploads is misleading. `countAdjustedVisible` counts animesh against the avatar impostor limit.

## Spec claims checked and found TRUE
- **OFF path.** When `!sEverActive` there are no added GL calls and `keepsLive`/`tick` return immediately.
  - The `uploadActorFx(LLDrawInfo)` restructure behaves exactly as before.
  - The Keep helpers multiply by an exact 1.0. Even if the compiler fuses the add, `fma(x,1,y)` equals x+y.
  - Every PBR-flag writer writes .b as 0: pbropaque without actor FX, HUD, pbrterrain, occlusionF.
  - The only readers of .b are the four light shaders. cineOutline, tronWorld and SSR read only .w or .xy.
- **Tame** scales only `iblSpec` and `specPunc` after the BRDF. Diffuse is untouched.
- **Brightness** is applied once:
  - `actor_fx_authored_source` is captured at sharedActorFxPbrF:419, before the trim.
  - `actorFxPbrPostLight` has only one non-early return, `mix(layer_source, fx, strength)` at :1957, and `strength` is never modified.
- **Glow** touches authored emission only. SWEEP_GLOW goes through the GLOW slot.
- **Bound-program uploads.**
  - `batch_shader` and `static_shader` receive the upload after `apply_program`, for every style.
  - The per-slot redraws use the same program. The highlight program has no location, so the upload is a no-op.
  - `gActorGhostProgram` is bound nowhere outside `drawGeometryGhost`.
  - Shared replay uses each command's own owners. `sGhostAlrInstance` is saved and restored by RAII and is set only for Studio proxies (`mInstanceId`, llactormover.cpp:14406).
- **Spec deviation, correct in the code.** §5.6 said to set `mLightResponseKey` at ":17792/:17811". Those sites are SharedActorStyleProxy Actor FX replays and have no instance id. The implementer correctly left the key null there.
- **UI.** Every XUI control name and notification exists. The setting types are right.
