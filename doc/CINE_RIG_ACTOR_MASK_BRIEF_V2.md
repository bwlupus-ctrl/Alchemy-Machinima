# Cine Light Rig: "Actor only" lights through a per-pixel Actor Mask (design brief V2)

**Date:** 2026-10-01
**Status:** DESIGN ONLY. Nothing implemented, nothing built, nothing committed.
**Supersedes:** `doc/CINE_RIG_ACTOR_MASK_BRIEF.md` (V1). Resolves every finding in
`doc/CINE_RIG_ACTOR_MASK_REVIEW_OPUS_R1.md` (A1-A13, B1-B7, C1-C6). This document stands alone; you do
not need V1 or the review to implement it.
**Anchored at:** HEAD `025ed6763c6` (contains probe on-demand `def6630db74` and the manual probe rate). The
working tree is clean apart from untracked notes. Every `file:line` below was re-read at this HEAD.
Labels: **PROVES** means read in code at HEAD. **INFERENCE** means reasoning not yet checked in code or
in-world. **GUESS** means a value someone has to tune.

---

## 0. Decisions already made (not reopened here)

| # | Decision |
|---|---|
| D1 | Mask = **shared-depth R8 UNORM** target. Each subject avatar gets a **CPU-computed byte** (OR of every slot/group bit that resolves to it). The mask is written with **blending OFF, plain overwrite**. |
| D2 | The mask pass runs **once at the top of `renderDeferredLighting()`** for every view that passes through it. |
| D3 | Data model: per light `light_bits` (U8), shader test **any-of** `(byte & light_bits) != 0`. Supports Group mode (`GROUP_SLOT_*`) and slots SELF, A-D. |
| D4 | **Object-target rigs: Actor-only is not supported in v1.** The checkbox is disabled with a tooltip, the rig lights normally, one log line. |
| D5 | Implementers: **Codex** = heavy engine (mask target and pass, light programs, forward gating, view classification, probe exclusion). **Sonnet** = param blob, persistence, UI, settings, docs. **Opus** = instrumentation. **No Fable.** Adversarial review = **Opus + Codex**. |
| D6 | Add **WP0** (frozen registry/API). Then the `pipeline.cpp` packages run **sequentially**. View classification is part of WP1. Probe exclusion runs after WP0 and integrates with the probe on-demand code at HEAD. |
| D7 | **Everything off by default.** The off path stays inert behind a cached `anyActorOnly()` flag. Only **new** program objects are added; no existing program changes. |

---

## 1. The ask

> "Give the cine light rig lights a special property to only light the avatar selected. The outside
> environment lights can impact the avatar, but the lights produced by the rig have an option to only
> impact the actor." / "It would have to impact the entire actor and all their attachments." / "Do it the
> right way."

Industry name: light linking (Blender/Maya), lighting channels (Unreal, Unity HDRP). We already rejected the
bounding-volume shortcut, because it lights anything inside the actor's box.

---

## 2. What changed from V1, finding by finding

| Finding | Resolution in V2 | Where |
|---|---|---|
| A1 data model stale (Group, Object target) | Per-light `light_bits` + any-of test. Group rigs use `lastResolvedGroupSlots()`. Object-target rigs: Actor-only disabled, rig lights normally (D4) | §5.1, §6.3 |
| A2 additive "OR" corrupts bits | CPU per-avatar byte, blend OFF, overwrite | §5.4 |
| A3 forward gating fails test 3, misses streams | Inverted: a **world set** that excludes actor-only lights is always the default. **Actor sets** are built in the same `setupHWLights()` pass and selected only around identified subject draws. Content is restored without setters (new finding N2) | §5.7 |
| A4 Live ignore list too narrow | Global registry predicate `actorOnlyBits(id)`, applied by view classification and at every capture-light builder | §5.2, §5.8 |
| A5 blanket `gCubeSnapshot` gate kills mirrors | Classified by `mHeroProbeManager.isMirrorPass()` | §5.2 |
| A6 shared-depth lifecycle | Released before every depth reallocation. Lazily re-shared. Checked for size and depth id each view. Fails closed on mismatch | §5.3 |
| A7 wrong harvest, missing body | `walkGhostSourceGeometry` with explicit INCLUDE temp policy and a new group callback. Body and eyes through the `renderSystemActorGhost` pattern. Pass **include-list** | §5.4 |
| A8 impostored subjects | Keep-live hook extended. Subjects still impostored, muted or jellydolled are not drawn into the mask, so they fail closed (B7) | §5.4 |
| A9 spot-shadow slot competition | Three-tier shadow auction (rig world-lit > world > actor-only), plus actor-only last in Prism selection | §5.6 |
| A10 bounce-keep loses actor-only bounce | Live-probe bounce scale is applied per omni and skipped for actor-only omnis | §5.8 |
| A11 light programs | **New** program objects only. Same `.glsl` bodies, **byte-identical existing files** (main-rename wrapper). `discard` on miss. Actor-only lights do not consume `count`. **Fullscreen-only routing chosen: 2 new light programs** | §5.5 |
| A12 invariant guard too weak | Extended snapshot (polygon offset, scissor, logic-op) in an opt-in mode. Capture enabled by `CineLightRigActorMaskDebug` | §9 |
| A13 polygon offset | Units only (factor 0), negative toward camera, **GUESS** default, tuned by the verdicts | §5.4 |
| B1-B7 | Stated plainly in **Known limits (accepted)**. Not engineered around | §8 |
| C1 Fable vs Codex | Resolved by D5: no Fable | §10 |
| C2 persistence site | `mActorOnly` on `LightBase` (so cues and presets carry it) **and** appended after `mRimBackBias` in `ParamBlob::Light`. All 16 copy/IO sites listed and counted | §6.1 |
| C3 catchlight id | `ALCineLightRig::catchlightId()` added in WP0 | §5.1 |
| C4 pixel count | One `GL_SAMPLES_PASSED` query per subject bracket, read the next frame, never stalls | §9 |
| C5 automated test 3 | `ActorMask` test instrument with ISOLATION and PARITY verdicts | §9.3 |
| C6 off-path cost | Static cached `anyActorOnly()`; no per-light rig walks when false | §5.1, §7 |

**New findings at HEAD (not in the review):**

- **N1 (PROVES).** `LLRenderTarget::clear()` also clears a **shared** depth attachment by default
  (llrendertarget.cpp:569-577: `if (mUseDepth) mask |= GL_DEPTH_BUFFER_BIT`). A plain `actorMask.clear()`
  would erase the G-buffer depth. Fix: the mask must be cleared with `clear(GL_COLOR_BUFFER_BIT)` and
  drawn with depth writes OFF.
- **N2 (PROVES).** `LLLightState::setPosition` and `setSpotDirection` transform by the **current**
  modelview at call time (llrender.cpp:680-689, 736-744). `enableLights()` rewrites unit diffuse from
  `mHWLightColors[]` whenever the light mask changes (pipeline.cpp:10751-10780). Fix: an actor set
  cannot be applied mid-alpha through the setters. It is captured inside `setupHWLights()` and applied
  as raw unit content, with `mHWLightColors[2..7]` swapped in step.
- **N3 (PROVES).** `multiPointLightF.glsl` loops over the compile-time `LIGHT_COUNT`, not the
  `light_count` uniform (multiPointLightF.glsl:109, :146). The review's "one program at
  `LIGHT_COUNT=16` with the existing runtime count" would read stale array entries. Fix: pad unused
  entries with an inert sentinel (§5.5).
- **N4 (INFERENCE).** Shared Actor FX Replace/Cover (and Layer + Dissolve) suppress the actor's
  **native** beauty draws (lldrawpool.cpp:921-957, `shouldSuppressSharedActorFx` 940-1000). If that
  includes its G-buffer draws, the actor is not in the depth buffer. A redraw would then mark the wall
  behind them. Fix: a subject for which `LLRenderPass::shouldSuppressSharedActorFx(owner)` is true in
  this view is **not drawn into the mask** (it fails closed), and logs `ACTORMASK STYLED`.
- **N5 (PROVES).** G-buffer shaders call `mirrorClip(vary_position)` (diffuseF.glsl:37, :62). The plane
  is set for mirrors and for the Prism behind-lens clip at llsettingsvo.cpp:1287-1307. Fix: the mask
  shaders must call the same `mirrorClip`. Otherwise geometry clipped out of the G-buffer marks the
  wall behind it.
- **N6 (PROVES).** Fullbright faces are drawn after deferred lighting, so they are not in the G-buffer
  depth. Fix: they are excluded from the mask (pass include-list, §5.4). This resolves the review's
  least-sure item 5: they are unlit anyway, so the rig light has nothing to do on them.
- **N7 (INFERENCE).** If the mask ignores face culling, a single-sided face seen from behind (culled in
  the G-buffer) marks the background. Fix: the mask mirrors G-buffer culling: back-face cull, except
  double-sided GLTF.
- **N8 (PROVES).** The spot-shadow auction is **categorical rig-over-world** (pipeline.cpp:24391-24402).
  Prism feeds select spot shadows nearest-first (pipeline.cpp:27156-27219).
- **N9 (PROVES).** `appendCinematicProbeLightSignature` already skips every rig emitter
  (pipeline.cpp:9770-9773, "hashed by the rig signature"). The review's A4 bullet for it needs **no
  change**; the rig-side signature does.
- **N10 (PROVES).** The on-demand probe scheduler samples rig emitters as ordinary lights
  (`probe_gather_light`, llreflectionmapmanager.cpp:1254-1317; loop 1755-1786).
- **N11 (PROVES).** Every model copy of `LightBase` starts with `memset(0)` and copies field by field
  (`cleanLight` 243-246, `blendLight` 1106-1107). A new field that is missed at one site silently
  becomes `false`. The count list is in §6.1.
- **N12 (PROVES).** In Group mode, SELF resolves through `cast.resolve(LLUUID::null)`
  (alcinelightrig.cpp:165-166). The SELF rig in non-group mode honours a legacy `mAnchor`
  (alcinelightrig.cpp:1060-1064). These can be two different avatars that both carry bit 0. Accepted
  edge; see the least-sure list.

**Corrections to V1:** settings live in `app_settings/settings.xml`, not `settings_alchemy.xml`. V1's
rig-level "Hide shafts when Actor-only" is **dropped**: each light already has its own Shaft toggle, and
B4 is accepted. V1's "Catchlight Actor-only **by default**" is changed to **default off** (D7).

---

## 3. Scope

**In this delivery (ship whole):**
1. WP0 registry: `anyActorOnly()`, `actorOnlyBits(id)`, per-avatar byte table, `light_bits`.
2. Actor Mask target, its lifecycle, the mask pass, and view classification.
3. Actor-only routing of deferred local lights (two new fullscreen programs).
4. Three-tier spot-shadow auction.
5. Forward world set and actor sets: alpha pool, interleaved stream, Shared Actor FX proxies.
6. Exclusion from probes and Live probe, Live bounce handling, on-demand scheduler eligibility.
7. Per-light **Actor only**, rig-level **Bounce lights the set** and **Catchlight Actor only**. Persisted
   through settings, param blob, setup presets, cues and scene save.
8. UI in the rig panel (also embedded in the Director), Lightbox debug controls, tooltips, reset list.
9. Verdict log, debug overlay, extended invariant guard, and an automated isolation/parity test.

**Deferred (separate brief):** publishing the mask to ReShade as an ABI-appended `SL_ACTOR_MASK`
semantic. That is the only remedy for B1.

**Hard constraints (unchanged):** G-buffer layout; depth format (`DEPTH_FMT_24`, pipeline.cpp:1280); the
ReShade bridge (`0x81A6`, llreshadebridge.cpp:209-214); every existing shader file and program object;
any light that is not an enabled rig light flagged Actor-only.

---

## 4. Anchor table at HEAD `025ed6763c6`

`pipeline.cpp`, `llviewerdisplay.cpp`, `llactormover.cpp` and `alcinelightrig*` did not change between
`def6630db74` and `025ed6763c6`, and their review anchors re-check correct. `llreflectionmapmanager.cpp`
did change (+79 lines); its anchors below are new.

| Item | HEAD anchor |
|---|---|
| Slot enum SELF..D | alcinelightrig.h:100-108 |
| `GROUP_SLOT_*` bits (SELF=1<<0 .. D=1<<4, mask 0x1F) | alcinelightrig.h:173-181 |
| `resolveSlotAvatar` / `projectorId` / `omniId` | alcinelightrig.cpp:1051-1066 / 1068-1076 / 1079-1087 |
| Live ignored / projector ids | alcinelightrig.cpp:1110-1126 / 1128-1140 |
| Rig Live signature / animating | alcinelightrig.cpp:1175-1205 / 1207-1230 |
| `mCatchlight` (no id accessor) | alcinelightrig.h:393 |
| `gatherGroupMembers` (dedupes same avatar) | alcinelightrig.cpp:159-209 |
| `tickShared`: object branch / group branch / avatar | alcinelightrig.cpp:2362-2407 / 2409-2501 / 2423-2426 |
| Live bounce scale applied | alcinelightrig.cpp:2537 |
| Catchlight destroyed for object targets | alcinelightrig.cpp:2559-2563; applied 2826-2839 |
| FX modifier copy loop | alcinelightrig.cpp:2733-2758 |
| `applyFrame` omni write | alcinelightrig.cpp:2012-2039 |
| `createEmitter` | alcinelightrig.cpp:1536 |
| Manager `tick` (rig loop ends 951, `updateLiveProbe` 958) | alcinelightrigmanager.cpp:914-959 |
| `updateLiveProbe` → `setCinematicLiveProbe` | alcinelightrigmanager.cpp:657-662 |
| `liveProbeBounceScaleFor` | alcinelightrigmanager.cpp:494-503 |
| `rigRimParamsFor` (per-light lookup precedent) | alcinelightrigmanager.cpp:738-764 |
| `ParamBlob::Light` / positional `mLights[]` rows (11 members) | alcinelightrigmanager.h:39-69 / 145-154 |
| `LightBase` / `Cue` / `CueState` | alcinelightrigmodel.h:80-103 / 153-169 / 178-185 |
| `renderDeferredLighting` / `preserveReShadeGBuffer` / `sCull` return | pipeline.cpp:23016 / 23033 / 23035-23038 |
| `setupHWLights` call inside deferred lighting | pipeline.cpp:23076 |
| Local-light block (partition point 23386-23431) | pipeline.cpp:23333-23679 |
| Point vol / spot vol / multi-point / multi-spot | 23369-23528 / 23530-23570 / 23572-23634 / 23636-23671 |
| `renderGeomPostDeferred` call (tail) | pipeline.cpp:23754; its `calcNearbyLights`+`setupHWLights` 7617-7618 |
| Ghost forward stages | pipeline.cpp:23748-23775; `renderGhostRiggedBlend` 6627; `renderGhostPostDeferred` 6956 |
| `calcNearbyLights`: probe-centric / Prism / main | pipeline.cpp:9827; 9844-9941 / 9943-10024 / 10026-10196 |
| `appendCinematicProbeLightSignature` (rig skip) | pipeline.cpp:9660-9825 (9770-9773) |
| `setupHWLights`: units loop / fade write / fill / disable | pipeline.cpp:10466; 10576-10727 / 10636-10652 / 10729-10738 / 10744-10748 |
| `enableLights` (diffuse from `mHWLightColors`) | pipeline.cpp:10751-10780 |
| Prism light-state snapshot precedent | pipeline.cpp:10215, 10416 |
| Spot shadow auction | pipeline.cpp:24372-24410 (`setupSpotLight` 24272) |
| Prism spot-shadow selection | pipeline.cpp:27119, 27156-27219 |
| `generateImpostor` (runs `renderGeomPostDeferred`) | pipeline.cpp:27604; 27853-27872 |
| deferredScreen alloc / share to screen | pipeline.cpp:1280 / 1323 |
| Pack recursion (aux, hero) | pipeline.cpp:1231-1255 |
| Prism scratch release / acquire / alloc | pipeline.cpp:1574-1586 / 1588 / 1660-1670 |
| `releaseScreenBuffers` | pipeline.cpp:2297-2318 |
| `bindDeferredShader` (Rig Rim globals 22936) | pipeline.cpp:22665 |
| Light program registration | llviewershadermgr.cpp: point 2639-2656, multi 2658-2680, spot 2682-2700, multi-spot 2702-2721; unload 1620-1626 |
| Shadow alpha-mask program family (template for mask programs) | llviewershadermgr.cpp:3431-3449, 3554-3590 |
| Reserved uniforms end | llshadermgr.h:919; llshadermgr.cpp:2140-2146 |
| `LL_DEFERRED_MULTI_LIGHT_COUNT` = 16 | llviewershadermgr.h:36 |
| Light shaders | class3/deferred/pointLightF.glsl, multiPointLightF.glsl, spotLightF.glsl (MULTI_SPOTLIGHT 53-57, 119-123) |
| `LLRender` light snapshot API | llrender.h:527-535; llrender.cpp:1654-1668 |
| `LLLightState` fields / setters / `syncLightState` | llrender.h:243-288; llrender.cpp:607-739 / 903-967 |
| `LLRenderTarget::shareDepthBuffer` / `release` / `clear` / `getDepth` | llrendertarget.cpp:421-455 / 457-487 / 569-586; llrendertarget.h:169 |
| Main frame order | llviewerdisplay.cpp: ghost queue 1077, G-buffer 1080, ghost submit+invariant 1082-1094, flush 1115-1116, lighting 1120 |
| Cube face (probes and hero mirrors) | llviewerdisplay.cpp:1256; probe-centric rebuild 1305-1309; lighting 1388; via llviewerwindow.cpp:6047/6162 and llreflectionmap.cpp:73 |
| Pack switches | aux llreflectionmapmanager.cpp:2791/2830; hero llheroprobemanager.cpp:301/305 |
| `isMirrorPass` | llheroprobemanager.h:88 |
| Prism feed lighting | llprismlens.cpp:7576 (`calcNearbyLights`), 7586 (`renderDeferredLighting`) |
| Live predicates (Live-gated) | llreflectionmapmanager.cpp:2612-2635, 2637-2653 |
| Live H sample (rig + light signature) | llreflectionmapmanager.cpp:1062-1105 |
| On-demand `probe_gather_light` / scheduler loop | llreflectionmapmanager.cpp:1254-1317 / 1755-1786 |
| Alpha pool | lldrawpoolalpha.cpp: `renderAlpha` 829; FX depth prepass 866; proxy turn 969-999; draw loop 1071+; `applyModelMatrix` 1096; `enableLightsDynamic` 314/1354/1448 |
| Draw-info owner | llspatialpartition.h:113-119 (`mActorFxOwner`, `mActorFxFallbackOwner`) |
| `getActorFxOwnerId` | llvoavatar.h:530; llvoavatar.cpp:12365 |
| ALR keep-live hook | llvoavatar.cpp:4643 |
| Ghost harvest | llactormover.cpp: `kRiggedPasses` 13794-13822; `walkGhostSourceGeometry` 13830-13957 (temp policy 13843-13844); `collectGhostBatches` 13959 |
| System body replay pattern | llactormover.cpp:14861-14899, 14901+ |
| Shared Actor FX proxies | llactormover.cpp:16932-17020 (proxy holds `mAvatar`) |
| Shared FX native suppression | lldrawpool.cpp:865-881, 921-957, 940-1000 |
| Ghost invariant guard | llghostdeferreddiagnostics.h:25-44, 61-88; .cpp Snapshot 148, ctor 243-253 |
| Mirror/Prism clip | diffuseF.glsl:37/62; llsettingsvo.cpp:1287-1307 |
| ReShade depth | llreshadebridge.cpp:209-214 |
| Rig panel per-light rows (Shaft/Hero) | panel_cine_light_rig.xml:321-322, 390-391, 455-456, 520-521; embedded in Director at floater_director.xml:1683-1684 |
| Rig reset list | alpanelcinelightrig.cpp:471+ (`ALPanelCineLightRig::settings()`) |
| Rig settings keys | app_settings/settings.xml (e.g. CineLightRigCatchlight 2085, CineLightRigKeyOn 2436) |

---

## 5. Design

### 5.1 Registry and data model (WP0, frozen before anything else)

**Per light, model level:** `bool LightBase::mActorOnly = false;` appended after `mFixturePreset`
(alcinelightrigmodel.h:102). Because it lives on `LightBase`, setup presets, cues and transitions carry
it. `blendLight` switches it at the midpoint (`eased > 0.5f ? target : start`), never interpolated.

**Per rig (not cue-animated):**
- `mBounceLightsSet` (default **false**). False means bounce omni *i* follows light *i*: if light *i* is
  Actor-only, so is its bounce. True means the bounce stays a world light.
- `mCatchlightActorOnly` (default **false**).

**Effective `light_bits` for rig *r*** (computed by the registry each frame):
- Object-targeted rig (`getObjectTarget().notNull()`): **0** for every emitter. The rig lights normally
  (D4). Log `ACTORMASK OBJECT-TARGET <slot>` once per transition.
- Group rig: `bits(r) = lastResolvedGroupSlots()` (alcinelightrig.h:253-256).
- Otherwise: `bits(r) = 1 << slot(r)` when `resolveSlotAvatar()` is non-null, else 0.
- Projector *i*: `bits(r)` if `mCurrentLive[i].mActorOnly` (post-transition, post-cue, post-FX state),
  else 0.
- Omni *i*: the projector's bits, unless `mBounceLightsSet`.
- Catchlight: `bits(r)` if `mCatchlightActorOnly` and the catchlight exists.

**Per-avatar byte:** `byte(av) = OR of every slot bit that resolves to av` across all enabled rigs with
non-zero bits. Non-group rigs contribute `{resolveSlotAvatar(), 1<<slot}`. Group rigs contribute one
entry per set bit of `lastResolvedGroupSlots()`, resolved the way `gatherGroupMembers` does
(alcinelightrig.cpp:163-175). Duplicates OR together, so any-of stays correct when one avatar fills
two slots.

**Frozen API** (alcinelightrigmanager.h; logic in a pure `ALCineLightRigManagerModel` function so the
unit-test target can cover it):

```cpp
struct ActorOnlySubject { LLPointer<LLVOAvatar> mAvatar; LLUUID mFxOwner; U8 mByte; U8 mSlotBits; };
static bool anyActorOnly();                         // static cached bool; false when kill switch set
U8   actorOnlyBits(const LLUUID& light_id) const;   // 0 = world light
U8   actorByteForAvatar(const LLVOAvatar* av) const;
U8   actorByteForOwner(const LLUUID& fx_owner) const; // matches LLDrawInfo::mActorFxOwner / fallback
const std::vector<ActorOnlySubject>& actorOnlySubjects() const;
bool isActorOnlySubject(const LLVOAvatar* av) const;  // keep-live hook (A8)
```

- Rebuilt in `ALCineLightRigManager::tick()` **after** the rig loop (after line 951) and **before**
  `updateLiveProbe()` (958). At most 45 light entries (5 rigs x (4 + 4 + 1)) and 5 subjects.
- `mFxOwner = avatar->getActorFxOwnerId()`. Dead avatars are skipped at use time (`isDead()`).
- `anyActorOnly()` = `!CineLightRigActorMaskDisable && table non-empty`. It is a static bool, so hot
  paths pay one branch and never call `instance()`.
- WP0 also adds `ALCineLightRig::catchlightId()` (C3), the two rig-level members with setters, and the
  keep-live clause at llvoavatar.cpp:4643: `|| ALCineLightRigManager::instance().isActorOnlySubject(this)`,
  guarded by `anyActorOnly()`.

### 5.2 View classification (WP1)

Computed once at the top of `renderDeferredLighting()`, after `preserveReShadeGBuffer()` (23033) and the
`sCull` return (23035-23038). Stored in `LLPipeline::mActorMaskView` and reset to `NONE` on every exit by
an RAII guard, so impostor bakes and later callers never inherit it.

| Condition (in order) | Class |
|---|---|
| `!anyActorOnly()` | `NONE` (no work at all) |
| `sImpostorRender` or `sRenderingHUDs` | `EXCLUDE` |
| `gCubeSnapshot && mHeroProbeManager.isMirrorPass()` (pack `mHeroProbeRT`) | `MASK` |
| `gCubeSnapshot && !isMirrorPass()` (reflection probes, Live probe, on-demand faces; pack `mAuxillaryRT`) | `EXCLUDE` |
| `sPrismLensRender` (pack = the active Prism scratch, llprismlens.cpp:7586) | `MASK` |
| `mRT == &mMainRT` (main view, including tiled hi-res snapshots) | `MASK` |
| anything else | `EXCLUDE`, log `ACTORMASK VIEW-UNCLASSIFIED` once |

These are all three callers of `renderDeferredLighting` at HEAD: llviewerdisplay.cpp:1120,
llprismlens.cpp:7586 and llviewerdisplay.cpp:1388. In `MASK`, actor-only lights are masked. In
`EXCLUDE`, they are skipped everywhere in that view. A `MASK` view whose mask could not be produced
(allocation, stale depth, program incomplete) becomes `EXCLUDE` for that view (fail closed), with a
verdict.

### 5.3 Mask target lifecycle (WP1, resolves A6 and N1)

- New member `LLRenderTarget actorMask` and `U32 actorMaskDepthSrc` in `RenderTargetPack`. Format
  `GL_R8`, no own depth.
- **Release before any depth reallocation:** add `actorMask.release()` before `deferredScreen.release()`
  in `release_pack` (pipeline.cpp:2299-2311) and `release_prism_scratch_entry` (1574-1586). Also add it
  at the top of `allocateScreenBufferInternal` for the current pack, before line 1280, so resize,
  `RenderResolutionDivisor`, the HDR toggle and hi-res snapshots never leave the mask on a dead depth
  texture.
- **Lazy ensure** at the start of each `MASK` view. If the mask is unallocated, or its size differs
  from `deferredScreen`, or `actorMaskDepthSrc != deferredScreen.getDepth()`: release it,
  `allocate(w, h, GL_R8, false)`, `deferredScreen.shareDepthBuffer(actorMask)`, and record
  `actorMaskDepthSrc`. If it still mismatches, log `ACTORMASK STALE` and the view becomes `EXCLUDE`.
  `shareDepthBuffer` LL_ERRS when the target already holds depth (llrendertarget.cpp:430-438), which is
  why release always comes first.
- **Free when unused:** at the main view, if `!anyActorOnly()` and any pack still holds a mask, release
  all of them.
- **Clear:** `actorMask.clear(GL_COLOR_BUFFER_BIT)` only (N1). Never clear depth. Never write depth.

### 5.4 The mask pass (WP1, resolves A2, A7, A8, A13, N4-N7)

Runs once per `MASK` view, right after classification. It is the first draw work in
`renderDeferredLighting`. At that point the view's matrices are current, the G-buffer depth is final, and
in the main view it includes ghost-proxy depth.

**State, all RAII and restored on exit:** bind `actorMask` (FBO + viewport); `LLGLDisable(GL_BLEND)`;
`LLGLDepthTest(GL_TRUE, GL_FALSE, GL_LEQUAL)` so depth writes are off; `LLGLEnable(GL_POLYGON_OFFSET_FILL)`
with `glPolygonOffset(0, CineLightRigActorMaskBias)` and an explicit `glPolygonOffset(0, 0)` on exit;
back-face culling as in the G-buffer, disabled for double-sided GLTF (N7); colour mask set and
restored. **Do not** call `blendFunc` and **do not** enable scissor or logic-op. Wrapped in the extended
invariant guard (§9).

**What is drawn, per subject in `actorOnlySubjects()` (value written = `byte / 255.0`, exact in UNORM8):**

1. **Skip, with a verdict and no draw (fail closed):**
   - Avatar outside the view frustum: `OFFSCREEN`.
   - Impostored, visually muted, jellydolled, too complex or `isTooSlow`: `IMPOSTOR` (A8, B7).
   - `LLRenderPass::shouldSuppressSharedActorFx(subject owner)` is true in this view: `STYLED` (N4).
2. **Body and eyes** (non-control avatars): the `renderSystemActorGhost` pattern
   (llactormover.cpp:14901+). Use `renderSkinned`/`renderRigid` with `LLDrawPoolAvatar::sMinimumAlpha`
   as the cutoff, through a mask avatar program and an eyeball mask program.
3. **Attachments, rigged and unrigged, worn animesh, standalone animesh subject:**
   `walkGhostSourceGeometry` (13830) with two **new optional parameters**. Defaults reproduce today's
   behaviour exactly, so the existing callers (collectGhostBatches 14152, llclonefidelityaudit.cpp:801)
   are untouched.
   - `temp_policy_override`: the mask passes `INCLUDE`, ignoring `GhostUnifiedExcludeTemporaryAttachments`.
   - `group_cb`: receives each `LLSpatialGroup*` the walk collected.
   For each group, draw the draw infos of this **include-list** of passes, with each draw info's model
   matrix and skin palette:
   - Rigged: `PASS_SIMPLE_RIGGED, PASS_SHINY_RIGGED, PASS_BUMP_RIGGED, PASS_MATERIAL_RIGGED,
     PASS_MATERIAL_ALPHA_MASK_RIGGED, PASS_SPECMAP_RIGGED, PASS_SPECMAP_MASK_RIGGED, PASS_NORMMAP_RIGGED,
     PASS_NORMMAP_MASK_RIGGED, PASS_NORMSPEC_RIGGED, PASS_NORMSPEC_MASK_RIGGED, PASS_ALPHA_MASK_RIGGED,
     PASS_GLTF_PBR_RIGGED, PASS_GLTF_PBR_ALPHA_MASK_RIGGED`.
   - Unrigged: the same list without `_RIGGED`.
   - Everything else is excluded on purpose: blend passes (`*_BLEND*`, `PASS_ALPHA*`,
     `PASS_MATERIAL_ALPHA*` non-mask), emissive passes, glow passes and **all fullbright passes** (N6).
   - Implementer: check every listed enum against `llrender/lldrawpool.h` and the non-rigged pass enum,
     and list the final set in the WP1 report.
4. **Alpha-mask draws** use the draw's own cutoff and texture transform: legacy `mAlphaMaskCutoff` and
   texture matrix; GLTF base-colour alpha times factor and vertex alpha, with KHR texture transform.
   Mirror exactly the discard expression of the G-buffer program that drew it. Opaque draws need no
   texture.
5. **All mask fragment shaders call `mirrorClip(vary_position)`** (N5).

**Programs:** new program objects only, modelled on the shadow alpha-mask family
(llviewershadermgr.cpp:3431-3449, 3554-3590): static, rigged (`mRiggedVariant`), GLTF alpha-mask
(+ rigged), avatar, eyeball. At most 6; WP1 reports the exact set. They are added to the unload list
(1620-1626). Each bind is guarded by `isComplete()`. If any is missing: `ACTORMASK NO-PROGRAM`, and the
view becomes `EXCLUDE`.

**Ordering.** "One front surface per pixel" holds except at exact depth ties. A tie inside one avatar
writes the same byte, so it is harmless. A tie between two avatars also z-fights in the beauty pass, so
either owner is legitimate. Overwrite makes overlapping same-avatar layers idempotent (A2).

**Occlusion query** (C4): one `GL_SAMPLES_PASSED` query per subject bracket, main view only. It is read
the next frame when `GL_QUERY_RESULT_AVAILABLE` is set, and is never waited on. Results go to a
counters struct that WP7 logs.

**Polygon offset (A13):** units only, factor 0. `CineLightRigActorMaskBias` default **-4.0 (GUESS)**,
range -64..0. Its correct value can only be found in-world, with the `EMPTY` verdict and the debug
overlay (§9).

### 5.5 Deferred actor-only lights (WP2, resolves A11 and N3)

**Partition** (light loop 23386-23431). Before `count++`, read
`bits = anyActorOnly() ? actorOnlyBits(volume->getID()) : 0`. If `bits != 0`:
- `EXCLUDE` view: `continue`. The light is skipped entirely.
- `MASK` view with a valid mask: apply the same reject tests as world lights (colour, size, frustum, and
  the attachment/world toggles do not apply to rig emitters). Push the light to `actor_points` or
  `actor_spots` with its bits, then `continue`.

The light never increments `count`, so world lights stop at exactly the same light as in the light-off
baseline (A11). Colour is computed with the identical expression
(`light_scale * ALEnvIntensity::localLightEVScale(volume)`). Spots still call `updateSpotLightPriority()`
outside Prism, as today.

**Decision: route actor-only lights through the two fullscreen paths only. 2 new light programs, not 19.**

*Why the shading-equivalence risk is small and checkable:*
- **Spots (PROVES).** The spot volume program and the fullscreen spot program compile the **same file**,
  `spotLightF.glsl`. `MULTI_SPOTLIGHT` only swaps the light centre between a uniform and a constant
  interpolated from the vertex stage (spotLightF.glsl:53-57, 119-123). The shading is identical by
  construction. The catchlight and all projectors are spots.
- **Points (PROVES the differences; INFERENCE that they are invisible).** `pointLightF.glsl` and
  `multiPointLightF.glsl` share the PBR maths. Two differences:
  - In the legacy branch, the volume shader adds an eps-clamped `nl` term on back-facing pixels; the
    fullscreen shader skips `nl <= 0`.
  - The fullscreen shader has a `far_z` early discard.

  Stock already swaps a light between these two programs whenever the camera crosses its box
  (pipeline.cpp:23464-23521), so a visible mismatch would already pop in the stock viewer. The only
  actor-only point lights are bounce omnis.
- **The stated test** is the instrument's **PARITY** verdict (§9.3). It compares the subject's pixels
  lit by the light as a world light with the same pixels lit through the actor-only path, on a frozen
  frame. The in-world list repeats it visually (test 3).
- **If PARITY fails:** add the two volume-path variants (point volume, spot volume) as new programs, 4
  in total. The full set is never 19, because of the padding below.

**New programs** (llviewershadermgr.cpp, registered after the existing four, added to the unload list):
1. `gDeferredMultiLightActorMaskProgram`: `multiPointLightV/F.glsl`, `LIGHT_COUNT = 16`.
2. `gDeferredMultiSpotLightActorMaskProgram`: `multiPointLightV.glsl` + `spotLightF.glsl`,
   `MULTI_SPOTLIGHT = 1`.

**Existing `.glsl` files stay byte-identical (main-rename wrapper).** Each new program adds
`addPermutation("main", "actorMaskedMain")` **after** `clearPermutations()` (the CLAUDE.md hazard). It
also adds one extra fragment file, `deferred/actorMaskGateF.glsl`:

```glsl
#undef main
uniform sampler2D actorMaskMap;
uniform int actor_mask_bits;
void actorMaskedMain();
void main()
{
    uint b = uint(texelFetch(actorMaskMap, ivec2(gl_FragCoord.xy), 0).r * 255.0 + 0.5);
    if ((b & uint(actor_mask_bits)) == 0u) discard;   // miss: no write, no NaN, bit-identical
    actorMaskedMain();
}
```

The light file's `main` compiles as `actorMaskedMain`, so its body is the **same source text** as the
stock program. That carries ALR Tame, Rig Rim, Tron tint and EnvIntensity unchanged.

- Implementer must verify that LL inserts permutation defines *above* the wrapper's `#undef`.
- If the driver rejects the macro (`ACTORMASK NO-PROGRAM` on first run), the fix-round fallback is
  `#ifdef ACTOR_MASK` blocks inside the two light files. That still leaves existing programs
  functionally identical, but changes their source hash.

**Reserved uniforms:** `ACTOR_MASK_MAP` ("actorMaskMap", sampler) and `ACTOR_MASK_BITS`
("actor_mask_bits"). Append both after `SHADOW_LIFT_GAIN` (llshadermgr.h:917-919) and after
llshadermgr.cpp:2144, in lockstep. Count the push_back list against the enum (rule 6). Appending
changes no existing index.

**Draws** (after the existing multi-spot block, ~23671, inside the same `BT_ADD` scope):
- **Points:** group `actor_points` by `bits`. Per group, in chunks of 16:
  - Pack `light/col/rim` exactly as 23587-23600 does.
  - **Pad unused entries** with an inert sentinel: `light = (0, 0, 1e9, 1)`, colour 0, rim 0. The shader
    then computes `dist = 1e9 > 1`, so the entry is skipped with no inf/NaN.
  - `far_z` is computed from real lights only.
  - Set `actor_mask_bits`, bind the mask texture, draw the screen triangle.
- **Spots:** bind the actor multi-spot program once. Per light: `setupSpotLight()` (same function,
  so the shadow tier applies), the same uniforms as 23654-23664, `actor_mask_bits`, draw.
- **No scissor in v1.** The gate is the first statement, so a miss costs one `texelFetch`. Tracy zone:
  `"actor-only lights"`. If it costs more than 0.3 ms at 4K, add scissor in a later round with
  RAII + guard coverage.

### 5.6 Spot-shadow tiers (WP2, resolves A9 and N8)

In the auction (pipeline.cpp:24391-24402), replace the two-way "rig outranks world" with a tier:
`tier = isCineRigEmitter && actorOnlyBits(id) == 0 ? 2 : (!isCineRigEmitter ? 1 : 0)`.
`outranks = tier_p != tier_i ? tier_p > tier_i : m_pri > pri`. With no actor-only light, the tiers
collapse to today's comparison, byte for byte. Actor-only projectors only take slots that are empty in
the light-off baseline.

In `generatePrismSpotShadows` (27156-27219), sort actor-only candidates after all others; keep the
existing distance order inside each group.

The tooltip states that an Actor-only projector can lose its self-shadow when all slots are taken.

### 5.7 Forward lighting: world set and actor sets (WP3, resolves A3 and N2)

**World set (always, every view, whenever `anyActorOnly()`).** In `setupHWLights()`
(10576-10727), add `if (anyActorOnly() && actorOnlyBits(light->getID())) { collect; continue; }` as the
**first** statement after the null check. It runs before any fade write, any `cur_light` increment and
any Live-capture count. World lights therefore get exactly the units, fade advances and
`mLightMovingMask` bits they get when the actor-only light is off. Every forward consumer (alpha, water,
impostor bakes, probe faces, ghost forward passes) then matches the light-off baseline.

**Actor sets (only when `mActorMaskView == MASK` and the mask is valid).** Built at the end of the same
`setupHWLights()` call, so the modelview is the view's own.
1. Save the world content: `gGL.getLightStateSnapshot(world, amb)` and `mHWLightColors[2..7]`.
2. For each distinct subject byte *b*: the units are the collected actor-only lights with
   `bits & b`, nearest first, then world units `0..(6 - n)` in their existing order.
   - Write them with the **same per-unit code**: factor today's body (10632-10721) into a helper.
   - Advance the actor-only light's fade clock **once** per `setupHWLights()` call, during collection.
   - Snapshot as set *b*. Save its `mHWLightColors`.
   - More than 6 matching actor-only lights is counted as `alpha_slot_overflow` (B2).
3. Restore the world content (step 1) before the disable loop (10744-10748), which is left unchanged.

**Applying a set.** New `LLRender::restoreLightUnitContent(const light_state_snapshot_t&, U32 first, U32 last)`:
- Copies every `LLLightState` field except `mIndex` and `mEnabled` for units 2..7.
- Writes black diffuse for units whose bit is clear in the current light mask, mirroring `enableLights`.
- Bumps `mLightHash` once.

New `LLPipeline::selectForwardLightSet(U8 byte)` calls it and swaps `mHWLightColors[2..7]`, so a later
`enableLights()` mask change re-applies the same set's colours (N2). It does nothing if `byte` equals
the current selection.

**Where it is called** (lldrawpoolalpha.cpp `renderAlpha`):
- **Native draws.** After `applyModelMatrix(params)` (1096) and before the draw:
  - `byte = actorByteForOwner(params.mActorFxOwner)`.
  - If 0 and `mActorFxFallbackOwner` is not null: `actorByteForOwner(mActorFxFallbackOwner)`.
  - Force 0 when the view is not `MASK`, or `sRenderingHUDs`, or `sImpostorRender`.
- **Shared Actor FX proxy turn** (969-999): select the set for `actorByteForAvatar(proxy avatar)`.
  This needs a new accessor `LLActorMover::sharedActorStyleProxyAvatar(index)`; the proxy already holds
  `mAvatar` (16932-16960). Restore the world set after `renderSharedActorStyleProxy`.
- **On every exit of `renderAlpha`:** an RAII guard restores the world set.

The interleaved walk drains each avatar contiguously (956-965), so switches happen about twice per
subject. Ghost proxy and overlay replays (`renderGhost*`, 23748-23775) run outside `renderAlpha` and
always see the world set. This is classification by render context, as the review asks.

**Verify:** a hash bump between two draws that share a bound shader must re-upload before the second
draw. That is the `setRigRimActive` precedent, llrender.cpp:888-900; the reviewer checks the per-draw
`syncMatrices`/`syncLightState` path.

### 5.8 Probes, Live probe, bounce (WP4, resolves A4, A10, N9, N10)

| Site | Change |
|---|---|
| Deferred light loop in cube faces | Covered by §5.2/§5.5: non-mirror cube faces are `EXCLUDE` |
| `setupHWLights` in cube faces | Covered by §5.7: world set excludes; no actor sets outside `MASK` |
| `calcNearbyLights` probe-centric branch (9875-9939) | Skip actor-only lights at insertion **unless** `isMirrorPass()`. This covers Live captures and on-demand ordinary/sliced faces |
| `appendCinematicProbeLightSignature` | **No change.** Rig emitters are already skipped (9770-9773, N9) |
| Rig Live signature / animating (alcinelightrig.cpp:1175-1230) | Actor-only projectors, omnis and catchlight are written as absent markers (`addExact(false)`, the existing `exclude_ignored` pattern) and ignored by `liveProbeAnimating`. Actor-only FX and flicker then stop forcing Live refreshes |
| `updateLiveProbe` (alcinelightrigmanager.cpp:657-662) | Remove actor-only ids from the **pinned** list, so the pinned count matches the light-off baseline (in that baseline the projector fails `getIsLight()` at 1134-1135) |
| On-demand `probe_gather_light` (llreflectionmapmanager.cpp:1288-1316) | Actor-only lights get `s.mEligible = false`. Toggling Actor-only is then one eligibility change, which correctly re-captures once. WP4 verifies in `ALProbeSched` that an ineligible light's later motion emits no dirty event |
| Bounce-keep (A10), alcinelightrig.cpp:2537 | When the rig has any actor-only omni, apply `mLiveProbeBounceScale` **per omni**: actor-only omnis are unscaled, world omnis are scaled. Recompute each omni's on/off as the model does. Bounce-enable must not be cleared while an actor-only omni needs it. With no actor-only omni, today's line 2537 runs verbatim |

### 5.9 Other systems (behaviour stated, no work unless marked)

| System | Behaviour |
|---|---|
| Rig Rim | Part of the same light output, so it follows the mask (deferred) and the selected set (forward). No change |
| ALR (Avatar Light Response) | The `.b` Tame carrier is read by the unchanged light bodies. No change |
| Tron avatar tag | Untouched. It is a useful cross-check on the debug overlay |
| SSR | Shows the lit actor in reflections (correct). Never shows the light on the set (B5) |
| Projector shafts, froxel lights, depth haze | Still show the beam (B4). Users can untick the per-light Shaft |
| Entity clones (`LLGhostAvatar`) cast in a slot | Ordinary avatars in the G-buffer, so masked like any subject |
| Ghost proxies and overlay ghosts | Never the actor (B7) |
| Impostor bakes | `EXCLUDE`; world set only |

---

## 6. Settings, persistence, UI

### 6.1 Persistence (WP5). Rule 6: count every site

`mFixturePreset` is currently the last per-light field. **Every non-UI, non-test site that handles
`mFixturePreset` gets a matching `mActorOnly` line: 16 sites.** Verify by grepping both names
afterwards and comparing counts.
- WP0 (runtime flow, 5 sites):
  - alcinelightrigmodel.cpp: `cleanLight` 272, `copyCleanLights` 432, `computeLive` 1080, `blendLight`
    1145 (midpoint).
  - alcinelightrig.cpp: FX modifier copy 2756 (routing is a modifier, not FX-owned).
- WP5 (persistence, 11 sites):
  - alcinelightrig.cpp: settings-name table (new `ACTOR_ONLY_SETTINGS[4]` beside 86-89), `lightsEqual`
    270, light→LLSD 489 (`"actor_only"`), LLSD→light 600 (absent means false), `readSettings` 1429,
    `readSettings(blob)` 1480, `writeSetupToSettings` 1532.
  - alcinelightrigmanager.h: `fromSettings` 263, `toSettings` 351, `toLLSD` 423, `fromLLSD` 554.
- Declarations:
  - `LightBase` after `mFixturePreset` (model.h:102).
  - `ParamBlob::Light` **after `mRimBackBias`** (manager.h:68). The positional `mLights[]` rows
    (145-154) list only the first 11 members, so they are unaffected. Count them anyway.
- Rig-level: `mBounceLightsSet` and `mCatchlightActorOnly` as top-level blob fields beside
  `mCatchlight` (manager.h:118), with settings read/write, LLSD and the rig setter call in `tickSelected`
  and `tickFromBlob`.
- Cues carry the per-light flag through `Setup`. Rig-level options are not in cues.
- Tests (`alcinelightrigmodel_test.cpp`): blob round-trip (around 95-192, 242-352), `blendLight`
  midpoint, `cleanLight` copy, registry bits/bytes for slot, group (with a duplicate avatar) and object
  target.

### 6.2 Settings (`app_settings/settings.xml`; every default is off or false)

| Key | Type | Default | Meaning |
|---|---|---|---|
| `CineLightRig{Key,Fill,Rim,Bg}ActorOnly` | Boolean | 0 | Per-light Actor only |
| `CineLightRigBounceLightsSet` | Boolean | 0 | 0: bounce follows its light |
| `CineLightRigCatchlightActorOnly` | Boolean | 0 | Catchlight lights only the subject |
| `CineLightRigActorMaskDisable` | Boolean | 0 | Kill switch: flags are ignored and lights act normally |
| `CineLightRigActorMaskDebug` | U32 | 0 | 0 off, 1 overlay tint, 2 mask only; also enables guard capture |
| `CineLightRigActorMaskBias` | F32 | -4.0 | Polygon offset units (GUESS) |
| `CineLightRigActorMaskTest` | U32 | 0 | 1 arm, 2 cancel (§9.3) |

Add the six rig keys to `ALPanelCineLightRig::settings()` (alpanelcinelightrig.cpp:471+) so the rig reset
covers them. They are not graphics-preset keys; do not add them to llpresetsmanager.cpp.

### 6.3 UI (WP6)

- **Rig panel** (also inside the Director, floater_director.xml:1683-1684): an **Actor only** checkbox on
  each light row next to Shaft/Hero (panel_cine_light_rig.xml:321-322, 390-391, 455-456, 520-521). In the
  rig section, add **Bounce lights the set** and **Catchlight: Actor only**. Wiring goes in
  alpanelcinelightrig.cpp/.h, following `mShaftControls` (827, 2311).
- **Object-target rig:** all Actor-only controls disabled. Tooltip: "Actor only needs an avatar subject.
  With an object target these lights light normally."
- **Tooltip (per light):** "Lights only this rig's subject and everything they wear. Environment
  lights still light them. Cannot cast a visible shadow onto the set. Shafts still show the beam.
  Reflections of the set never show it."
- **Lightbox** (existing Cine section of floater_lightbox_settings.xml): kill switch, Debug combo, Bias
  slider, "Run isolation test" button (sets `CineLightRigActorMaskTest = 1`).

---

## 7. Off path and ON path correctness (CLAUDE.md rule 1)

**Off** (every flag false, the default; or the kill switch is set):
- `anyActorOnly()` is false. The mask is never allocated, the mask pass never runs, new programs are
  compiled at load but never bound.
- The light loop, `setupHWLights`, the probe-centric branch and the alpha loop each pay one cached-bool
  branch.
- The shadow auction tier collapses to today's comparison.
- The bounce scale line runs verbatim.
- Existing `.glsl` files and program objects are byte-identical (wrapper, §5.5).
- `walkGhostSourceGeometry` defaults reproduce today's behaviour.
- `LLScopedGhostRenderInvariant` behaves as today unless the extended mode is requested.
- Appended reserved uniforms change no existing index.
- deferredScreen depth stays `DEPTH_FMT_24`, so ReShade `0x81A6` stays true.

**On.** Correctness definition: for every pixel not covered by a subject, the beauty image equals the
same scene with the actor-only lights turned off. Shafts and haze (B4) and SSR (B5) are not part of
that definition. This holds only with all of these:
- World set excludes actor-only lights before any side effect (§5.7, A3a).
- Actor-only lights do not consume `count` (§5.5).
- Shadow tiers (§5.6).
- Pinned-list filtering (§5.8).
- `discard` on miss (§5.5).
- The mask never writes depth and never clears depth (N1).

Reviewers check each against the beauty pass **and** the off path: colour mask, blend enable and func,
draw buffers, depth func and mask, polygon offset, bound FBO, viewport and program after the mask pass
and after the actor-only light block.

---

## 8. Known limits (accepted)

These are true of the finished feature. They are **not** bugs to fix in v1, and no work package may try
to engineer around them.

1. **RTGI bounce.** ReShade RTGI, or any screen-space GI, sees the brighter actor in the final image and
   can bounce some of that light onto nearby walls and floor. That partly undoes "Actor only" in the
   finished frame. (INFERENCE: iMMERSE is closed source.) The only real fix is a future
   `SL_ACTOR_MASK` ReShade semantic.
2. **Six forward lights.** Hair, lashes, sheer cloth and styled Cover/Layer surfaces are lit through 6
   local light slots. Actor-only lights take those slots first, so on a crowded set a world light can
   drop off the actor's hair. Several rigs on one group can have more actor-only lights than slots.
   The log reports `alpha_slot_overflow`.
3. **No shadow on the set.** An Actor-only light cannot throw a visible shadow onto the floor or walls,
   because it does not light them. The actor can still shadow themselves (arm across face). If all
   shadow slots are busy, even that self-shadow can be lost.
4. **Shafts and haze still show the beam.** Projector shafts, froxel lights and depth haze are in the
   air, not on surfaces, so the cone stays visible. Untick that light's Shaft if you do not want it.
5. **Reflections never show it on the set; mirrors lag.** Reflection probes and SSR show the set
   without the Actor-only light. That is by design, because the light does not touch the set. A mirror
   shows the actor lit, but updates at the hero-probe rate, so it can trail by a few faces.
6. **About 1-pixel speckle at the silhouette.** The mask is a second drawing of the actor, so edge
   pixels and contact points (a hand on a shoulder) can be off by about one pixel. The debug overlay
   shows exactly where.
7. **Impostored or muted subjects get nothing.** If the subject is drawn as an impostor, muted,
   jellydolled or too complex, the Actor-only light lights nothing on them (fail closed); the log
   says `IMPOSTOR`. The same applies to a Shared Actor FX Replace/Cover actor (`STYLED`). Ghost proxies
   and overlay ghosts are never "the actor".

---

## 9. Instrumentation: the log states a verdict (WP7, Opus)

### 9.1 Summary and verdicts (`LL_INFOS("ActorMask")`, once per 5 s while `anyActorOnly()`)

`views=<n> subjects=<bits> mask_draws=<n> mask_samples[slot]=<n> actor_only_lights=<n> alpha_switches=<n> alpha_slot_overflow=<n>`

| Verdict | Meaning |
|---|---|
| `ACTORMASK OK` | Every on-screen, non-occluded subject has mask samples > 0 |
| `ACTORMASK EMPTY <slot>` | Subject on screen and not occluded per the main occlusion state, but zero samples. **The feature is silently broken**; this must never look like "the light is dim" |
| `ACTORMASK OCCLUDED <slot>` | Zero samples and the subject's group is occluded: expected |
| `ACTORMASK OFFSCREEN <slot>` | Outside the frustum: expected |
| `ACTORMASK IMPOSTOR <slot>` / `STYLED <slot>` | Fail closed by design (B7, N4) |
| `ACTORMASK OBJECT-TARGET <slot>` | Rig is object-targeted; flags ignored (D4) |
| `ACTORMASK STALE` / `ALLOC-FAIL` / `NO-PROGRAM` | Mask unusable; the view fell back to `EXCLUDE` |
| `ACTORMASK VIEW-UNCLASSIFIED <view>` | Fail-closed path hit |
| `ACTORMASK STATE-LEAK <what>` | Extended guard tripped |

### 9.2 Guard and debug view
- **Guard (A12):** add an extended mode to `LLScopedGhostRenderInvariant`:
  - It snapshots `GL_POLYGON_OFFSET_FILL` with factor/units, `GL_SCISSOR_TEST` with box, and
    `GL_COLOR_LOGIC_OP`.
  - Capture is enabled by a caller-supplied predicate: `CineLightRigActorMaskDebug != 0`.
  - Ghost call sites keep the default mode, so they behave as today.
  - It wraps the mask pass and the actor-only light block.
- **Debug view:** mode 1 tints masked pixels per bit; mode 2 shows the raw mask. It is a fullscreen pass
  at the end of the main view's `renderDeferredLighting` and uses a new program.

### 9.3 Isolation / parity instrument (C5)

Armed by `CineLightRigActorMaskTest = 1`. Main view, fixed camera, subject frozen (pose stand or Actor
Mover freeze). Each mode is held for at least 1.5 s or 90 frames, whichever is longer, before capture.
That covers light fade and shadow-slot fade. Modes:
- F0: actor-only excluded everywhere.
- F1: masked deferred lighting, forward world set only.
- F2: normal.
- F3: actor-only lights treated as world lights everywhere.
- F0': F0 again.

Two capture points from `mRT->screen` (HDR, before post):
- **C_deferred:** right after the local-light block, before atmospherics, alpha and volumetrics.
- **C_post:** after `renderDeferredLighting` returns.

Memory is capped at two full captures plus the mask, about 140 MB at 4K; log the actual figure.

- **Stable pixels** = bit-identical between F0 and F0'. If fewer than 50% are stable, the result is
  INCONCLUSIVE ("scene animating").
- **ISOLATION-DEFERRED:** F1 vs F0 at C_deferred, stable pixels with mask == 0: must be bit-identical.
  Also report changed mask pixels; 0 is INCONCLUSIVE ("light does not reach the subject").
- **ISOLATION-FORWARD:** F2 vs F1 at C_post, stable pixels outside the subjects' screen bounds dilated by
  8 px: must be identical. The precondition is that projector shafts, froxel lights and depth haze are
  off; otherwise INCONCLUSIVE, citing B4.
- **PARITY:** F1 vs F3 at C_deferred, mask pixels eroded by 2 px:
  `|a-b| <= 1e-3 + 0.01*max(a,b)` per channel. INCONCLUSIVE if the spot-shadow slot assignment
  differed between F1 and F3, or the rig is animating (FX, flicker, transition).
- **Preconditions** (each failure gives INCONCLUSIVE with its reason): SSR off; camera matrices
  unchanged across all frames; mask unchanged across F1/F2/F3 (the subject moved otherwise); at least
  one actor-only light active and its subject on screen.
- Each result prints exactly one `ACTORMASK TEST <name> PASS|FAIL|INCONCLUSIVE <reason>` line. "The
  measurement was untrustworthy" is INCONCLUSIVE, never PASS or FAIL.

---

## 10. Work packages

Process (CLAUDE.md, user memory):
- Codex is invoked only with `--prompt-file` (a file inside the repo), launched through
  `run_in_background`, with the log-stall watchdog. It writes; it never builds.
- Each WP gets an explicit OFF-LIMITS list: every file another WP owns.
- Each WP is reviewed adversarially by **Opus and Codex** independently. Attack the claim, name the
  least-sure items, check the beauty pass and the off path. Fixes are batched and re-deferred until
  there are 0 must-fix (a finding that breaks the core promise is a blocker whatever its label).
- After all WPs converge: one cross-package Opus + Codex review of the whole diff for rule 1 and rule 6.
- Then a checkpoint commit and an exe backup, then **one** build by Claude, then the in-world test.

| WP | Owner | Content | Files owned | Order |
|---|---|---|---|---|
| **0** | Codex | Registry + API (§5.1), `LightBase.mActorOnly` + 5 runtime copy sites, rig-level members/setters, `catchlightId()`, keep-live clause, pure-model registry function + unit tests | alcinelightrigmodel.h/.cpp, alcinelightrig.h/.cpp (FX copy, accessors, members only), alcinelightrigmanager.h/.cpp (registry only), llvoavatar.cpp:4643, tests | First; API frozen at its review |
| **1** | Codex | View classification, mask target lifecycle, mask pass, mask programs, reserved uniforms, walk optional params, proxy-avatar accessor, counters struct | pipeline.h/.cpp (§5.2-5.4 sites only), llviewershadermgr.h/.cpp, llshadermgr.h/.cpp, new mask shader files, llactormover.h/.cpp | After WP0 |
| **2** | Codex | Light partition, 2 actor-only light programs + `actorMaskGateF.glsl`, fullscreen draws, shadow tiers, Prism shadow order | pipeline.cpp (23333-23679, 24372-24410, 27156-27219), llviewershadermgr.cpp, new gate file | After WP1 |
| **3** | Codex | World/actor sets, `restoreLightUnitContent`, `selectForwardLightSet`, alpha-pool selection | pipeline.h/.cpp (`setupHWLights` 10466-10749, new helper), llrender.h/.cpp (new method only), lldrawpoolalpha.cpp | After WP2 |
| **4** | Codex | §5.8 table | pipeline.cpp (9875-9939), llreflectionmapmanager.cpp, alcinelightrig.cpp (1175-1230, 2537, applyFrame omni), alcinelightrigmanager.cpp (657-662) | After WP3 **and** WP5 |
| **5** | Sonnet | §6.1 persistence (11 sites + rig-level), settings.xml keys, reset list, tests | alcinelightrig.cpp (IO functions only), alcinelightrigmanager.h (blob IO only), settings.xml, alpanelcinelightrig.cpp (`settings()` list only), tests | After WP0; parallel with WP1-WP3 (disjoint files); must finish before WP4 |
| **6** | Sonnet | §6.3 UI + tooltips + Lightbox controls; doc note in `doc/` | panel_cine_light_rig.xml, floater_lightbox_settings.xml, alpanelcinelightrig.cpp/.h (excluding `settings()`) | After WP5; parallel with engine chain |
| **7** | Opus | §9: verdicts, occlusion-query readout, extended guard, debug view program, test instrument | llghostdeferreddiagnostics.h/.cpp, new alactormaskdiagnostics.h/.cpp, pipeline.cpp hook points, llviewershadermgr.cpp (debug program) | Last in the `pipeline.cpp` chain |

Never let two agents edit the same file at the same time. The `pipeline.cpp` chain is
WP1 → WP2 → WP3 → WP4 → WP7, one owner per function.

---

## 11. In-world test (outcomes stated in advance)

Open the log. Keep `CineLightRigActorMaskDebug` handy.

1. **Mask sanity.** Rig on SELF, Key Actor-only, Debug = 1.
   - *Expect:* the avatar and everything worn tinted.
   - *Expect not tinted:* background seen through hair cards, an object in front of the avatar,
     fullbright attachment faces.
   - *Expect log:* `ACTORMASK OK`. Then step `Bias` toward 0 until `EMPTY` or speckle appears, and report
     the value where it is clean.
2. **Isolation.** Light floor and wall, Key Actor-only. *Expect:* avatar lit, no pool on floor or wall,
   no bounce pool. Tick **Bounce lights the set**: only the bounce pool appears. Untick Actor only: the
   key pool appears.
3. **Instrument.** Freeze the pose, SSR off, shafts/haze off, fixed camera. Press "Run isolation test".
   - *Expect:* `ISOLATION-DEFERRED PASS`, `ISOLATION-FORWARD PASS`, `PARITY PASS`.
   - INCONCLUSIVE means redo with what the reason names. **Any FAIL means the feature is broken,
     however good it looks.** Report the raw lines.
4. **Everything worn.** Alpha-blended hair, a held prop, worn animesh, rigged mesh clothing. *Expect:* all
   lit by the Actor-only key; nothing else.
5. **Two subjects / group.**
   - (a) Rig A on Subject A, rig B on Subject B, both Actor-only, hug pose. *Expect:* A's key lights only
     A and B's only B, including where they touch.
   - (b) One Group rig on A+B with Actor-only key, Subject C standing between them. *Expect:* A and B lit,
     C not.
6. **RTGI.** Enable iMMERSE RTGI. *Expect (INFERENCE):* some bounce near the actor (B1). Note how much.
7. **Mirror and feed.** Actor in a hero mirror and in a Prism/VCam feed. *Expect:* lit like the main
   view; the mirror may trail (B5). No light on the wall behind the actor inside the mirror (mirror
   clip, N5).
8. **Off path.**
   - All flags off. *Expect:* no `ACTORMASK` lines, fps unchanged vs `AlchemyTest` before this build.
   - Then flags on with the kill switch on. *Expect:* the lights act normally.
9. **Object target.** Rig on an object. *Expect:* Actor-only controls disabled with the tooltip, one
   `OBJECT-TARGET` line, normal lighting.
10. **Probes.** Shiny floor, Live Probe on, Key Actor-only with an animated FX. *Expect:* the floor
    reflection never shows the key's pool, and the Live probe refresh rate does not climb (compare its
    stats with Actor-only off).
11. **Fail closed.** Jellydoll or impostor the subject. *Expect:* the Actor-only light lights nothing on
    them; `IMPOSTOR`. A Shared Actor FX Cover/Replace subject: `STYLED`, and no light on the wall behind.
12. **Cue and shadows.**
    - A cue from Actor-only off to on switches at the fade midpoint.
    - With two shadowed world projectors in view, toggling Actor-only on the rig key leaves both world
      shadows unchanged.

---

## 12. Least-sure list (for the next reviewer; attack these first)

1. **Polygon offset.** The -4 default is a GUESS. Find the case where the rigged redraw z-fights, and
   check that units-only offset does not let a partner's hand within the epsilon pass.
2. **N4 STYLED.** Are G-buffer draws of Replace/Cover actors actually suppressed? Check
   `shouldSuppressSharedActorFx` against the G-buffer programs' `hasActorFx` flag. If they are not
   suppressed, the skip is unnecessary but harmless.
3. **Main-rename wrapper.** Where does LL insert permutation defines relative to the wrapper's
   `#undef`? Does any library file linked into the light programs contain the token `main`?
4. **Forward set switching.** Hash bump between same-shader draws; `enableLights` mask flips at
   lldrawpoolalpha.cpp:1354/1448; emissive sub-passes; the proxy's own shader binds.
5. **Owner keys.** Does `getActorFxOwnerId()` equal the `mActorFxOwner` that `registerFace` stamps for
   self, entity clones, worn animesh and a cast animesh?
6. **Pass include-list.** Is any G-buffer-drawn avatar pass missing (a hole) or any non-G-buffer pass
   present (a leak)?
7. **N12 bit-0 ambiguity.** A legacy SELF anchor plus a Group rig containing SELF can tag two avatars
   with bit 0.
8. **Instrument confounds.** Does anything else besides SSR, volumetrics and animation make F0/F1 differ
   on stable pixels (temporal AA history, probe blending)?
9. **On-demand scheduler.** Does an `mEligible=false` light really emit no further dirty events?
