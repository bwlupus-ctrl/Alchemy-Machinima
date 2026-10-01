# Cine Light Rig: "Actor only" lights through a per-pixel Actor Mask (design brief V3)

**Date:** 2026-10-01
**Status:** DESIGN ONLY. Nothing implemented, nothing built, nothing committed.
**Supersedes:** `doc/CINE_RIG_ACTOR_MASK_BRIEF_V2.md` and V1. Resolves every finding in
`doc/CINE_RIG_ACTOR_MASK_REVIEW_OPUS_R1.md` (A1-A13, B1-B7, C1-C6) and
`doc/CINE_RIG_ACTOR_MASK_REVIEW_CODEX_R1.md` (Codex 1-8) and `doc/CINE_RIG_ACTOR_MASK_REVIEW_FABLE_R1.md`
(Fable A1-A4 plus nits; edited in place). This document stands alone.

**Anchored at:** HEAD `057e55885f9`. Only documents changed since `025ed6763c6`, which contains probe
on-demand `def6630db74` and the manual probe rate. Every `file:line` below was re-read at this HEAD.

**Labels:**
- **PROVES:** read in code at HEAD.
- **INFERENCE:** reasoning not yet checked in code or in-world.
- **GUESS:** a value someone has to tune.

---

## 0. Decisions already made (not reopened here)

| # | Decision |
|---|---|
| D1 | Mask = **shared-depth R8 UNORM** target. Each recipient avatar gets a **CPU-computed byte**. The mask is written with **blending OFF, plain overwrite**. |
| D2 | The mask pass runs **once at the top of `renderDeferredLighting()`** for every view that passes through it. |
| D3 | Data model: per light `light_bits` (U8), shader test **any-of** `(byte & light_bits) != 0`. Supports Group mode (`GROUP_SLOT_*`) and slots SELF, A-D. Since Codex R1 #5, bits are allocated per **unique resolved recipient** (§5.1). |
| D4 | **Object-target rigs: Actor-only is not supported in v1.** The checkbox is disabled with a tooltip, the rig lights normally, and one line is logged. |
| D5 | **Who implements:**<br>- **Codex:** heavy engine work: mask target and pass, light programs, forward gating, view classification, probe exclusion.<br>- **Sonnet:** param blob, persistence, UI, settings, docs.<br>- **Opus:** instrumentation.<br>- **No Fable.**<br>Adversarial review is done by **Opus + Codex**. |
| D6 | Add **WP0** (frozen registry/API). After WP0, every package that shares a file runs **sequentially** (§10). View classification is part of WP1. Probe exclusion runs after WP0 and integrates with the probe on-demand code at HEAD. |
| D7 | **Everything off by default.** The off path stays inert behind a cached `anyActorOnly()` flag. Only **new** program objects are added. No existing program or shader file changes. |

---

## 1. The ask

> "Give the cine light rig lights a special property to only light the avatar selected. The outside
> environment lights can impact the avatar, but the lights produced by the rig have an option to only
> impact the actor." / "It would have to impact the entire actor and all their attachments." / "Do it the
> right way."

The industry names for this are light linking (Blender/Maya) and lighting channels (Unreal, Unity HDRP).
The bounding-volume shortcut is rejected: it lights anything inside the actor's box.

---

## 2. Resolution tables

### 2.1 Opus R1 (V1 → V2)

| Finding | Resolution | Where |
|---|---|---|
| A1: data model is stale (Group, Object target) | Per-light `light_bits` with an any-of test. Group rigs resolve their members. Object-target rigs disable Actor-only and the rig lights normally (D4) | §5.1, §6.3 |
| A2: additive "OR" corrupts bits | CPU-computed byte, blend OFF, overwrite | §5.4 |
| A3: forward gating fails test 3 and misses streams | A **world set** that excludes actor-only lights is always the default. **Actor sets** are selected only around identified subject draws. Their content is restored without setters (N2) | §5.7 |
| A4: Live ignore list is too narrow | A global registry predicate `actorOnlyBits(id)`, applied through view classification and at every capture-light builder | §5.2, §5.8 |
| A5: a blanket `gCubeSnapshot` gate kills mirrors | Mirrors are classified by `mHeroProbeManager.isMirrorPass()` | §5.2 |
| A6: shared-depth lifecycle | Release before every depth reallocation, lazy re-share, size and depth-id check, fail closed | §5.3 |
| A7: wrong harvest, body missing | `walkGhostSourceGeometry` with an explicit INCLUDE temp policy and a group callback. Body and eyes use the `renderSystemActorGhost` pattern. Passes come from an **include-list** | §5.4 |
| A8: impostored subjects | Keep-live hook extended. Subjects that are still impostored, muted or jellydolled fail closed (B7) | §5.4 |
| A9: spot-shadow slot competition | Three-tier shadow auction, with explicit null-slot handling (Codex R1 #4) | §5.6 |
| A10: bounce-keep loses the actor-only bounce | The Live bounce scale is applied per omni and skipped for actor-only omnis | §5.8 |
| A11: light programs | **New** program objects only, built from unchanged `.glsl` bodies through **fragment and vertex main-rename wrappers**. `discard` on a miss. Actor-only lights do not consume `count`. **Fullscreen-only routing: 2 new light programs** | §5.5 |
| A12: invariant guard is too weak | Extended snapshot (polygon offset, scissor, logic-op) in an opt-in mode | §9 |
| A13: polygon offset | Units only (factor 0), negative toward the camera. The default is a **GUESS**, tuned through the verdicts | §5.4 |
| B1-B7 | Stated plainly in **Known limits (accepted)** and not engineered around | §8 |
| C1: Fable vs Codex | No Fable (D5) | §10 |
| C2: persistence site | `mActorOnly` on `LightBase`, and appended after `mRimBackBias` in `ParamBlob::Light`. 16 sites, counted | §6.1 |
| C3: catchlight id | `ALCineLightRig::catchlightId()` | §5.1 |
| C4: pixel count | Tagged asynchronous occlusion queries (Codex R1 #7) | §9.1 |
| C5: automated test 3 | Isolation/parity instrument | §9.3 |
| C6: off-path cost | Static cached `anyActorOnly()` | §5.1, §7 |

### 2.2 Codex R1 (V2 → V3)

| # | Finding (Codex R1) | Resolution in V3 | Where |
|---|---|---|---|
| 1 | The suppression check depends on whichever shader is bound (lldrawpool.cpp:861-881) | The mask no longer calls `shouldSuppressSharedActorFx` itself. A per-view **G-buffer eligibility recorder** captures every draw and body suppressed during that view's real G-buffer submission, under the real shaders. The mask replays only what was natively drawn. Forward selection sits behind the in-context per-draw suppression test, so both paths use the eligibility of the real submission | §5.4.1, §5.7 |
| 2 | Renaming `main` also renames the vertex stage's `main`, because `mDefines` reach every listed file (llglslshader.cpp:466-469) | A **matching vertex wrapper**, `actorMaskGateV.glsl`, restores a vertex `main`. Library objects are precompiled without the program's defines (llglslshader.cpp:569-596). The source-edit fallback is **removed**: existing files stay byte-identical, with no exceptions | §5.5 |
| 3 | Both collectors stop scanning early (pipeline.cpp:23427-23430, 10722-10726) | Actor-only candidates are collected by a **separate, uncapped scan** after the unchanged world loop. The world loop only gains a side-effect-free `continue` ahead of every count, fade and unit write, so the baseline world count, fades and units are exact | §5.5, §5.7 |
| 4 | A null shadow slot ranks as a non-rig tier 1 | Null slots rank below every real actor-only candidate. A null displaced candidate ends the bubble (`break`), which matches today's outcome exactly. The world and rig comparisons against null are kept verbatim | §5.6 |
| 5 | Slot-bit aliasing: a legacy SELF anchor and Group SELF share bit 0 | Bits are allocated per **unique resolved recipient avatar** (at most 6 can exist; 8 are available). Each light's recipient set is translated into bits. N12 is resolved | §5.1 |
| 6 | Worn animesh that is also a cast subject loses the wearer's membership | One rule, used by the mask replay and the forward path alike: `drawByte = byteForOwner(mActorFxOwner) \| byteForOwner(mActorFxFallbackOwner)` | §5.4, §5.7 |
| 7 | EMPTY is not a trustworthy verdict | EMPTY is removed (Codex R2: a loose-depth hit does not prove visibility either). A tight count of 0 gives `VISIBILITY INCONCLUSIVE` with the loose count as a diagnostic. Broken-mask detection is the Debug=1 overlay plus NO-GEOMETRY. Each query is tagged with the frame, view, recipient and the visibility state captured at issue time | §9.1 |
| 8 | Work-package parallelism contradicts file ownership | The "disjoint files" claim is dropped. A single sequential order covers every WP | §10 |

### 2.3 Fable R1 (edited into V3 in place)

All four A items are PROVES at HEAD `057e55885f9`. I re-read every anchor below.

| # | Finding (Fable R1) | Resolution | Where |
|---|---|---|---|
| A1 (WP1) | `LLRenderTarget::clear()` is a bare `glClear` with the current clear colour (llrendertarget.cpp:569-596). At the mask site that colour is still the G-buffer magenta (llviewerdisplay.cpp:1030 main, 1373 cube; grey under wireframe at 1026/1369), and the colour mask is still `(true,false)`. Both are reset only later, at pipeline.cpp:23066 and 23096-23098. R8 clears to 255, every pixel passes any-of, and the whole set is lit | Before the colour-only clear: `gGL.setColorMask(true, true); glClearColor(0,0,0,0);`. On exit, restore the previous clear colour and colour mask. Clear colour is added to the §7 checklist. The instrument logs a mask-byte histogram | §5.3, §5.4, §7, §9.3 |
| A2 (WP1) | `mirror_flag` and `clipPlane` (llsettingsvo.cpp:1312, 1315) only reach programs dirtied from `mShaderList` (llenvironment.cpp:1676-1690). Mask programs that are not on that list keep `mirror_flag = 0`, so `mirrorClip` does nothing in mirror and Prism views | Push every mask program onto `mShaderList`, following the actorghost pattern (llviewershadermgr.cpp:614-627). Rigged variants are dirtied with their parent (llenvironment.cpp:1681-1683). The WP1 report verifies `getUniformLocation(MIRROR_FLAG) >= 0` (llshadermgr.h:104-105) for each program. New verdict `ACTORMASK NO-CLIP`: the view drops to `EXCLUDE` | §5.4.2, §9.1 |
| A3 (WP3) | System avatar alpha (hair, skirt, eyelashes) is forward-lit by `gDeferredAvatarAlphaProgram` through `LLDrawPoolAvatar::renderPostDeferred`, outside `LLDrawPoolAlpha`. No selection site covers it | On the post-deferred path of `LLDrawPoolAvatar::renderAvatars`: call `selectForwardLightSet(recipientBit(avatarp))` after the suppression test at lldrawpoolavatar.cpp:1132 and before `renderSkinned()` at :1179, then restore the world set. lldrawpoolavatar.cpp joins WP3 | §5.7, §10 |
| A4 (WP1) | Depth agreement is a rule per program. The shadow avatar template has no `AVATAR_CLOTH` | Each mask program uses the identical `gl_Position` expression and helper file as its G-buffer counterpart. The body copies `avatarV`, including the `AVATAR_CLOTH` block, and uploads the three wind uniforms as llactormover.cpp:15122-15146 already does. The WP1 report lists each expression beside its counterpart | §5.4.2 |

**Nits folded in:**
- The bias default is now -8, and the bisect starts there (Fable C1).
- PARITY records which path each F3 point took, and is INCONCLUSIVE if none took the volume path (C4).
- The new light programs copy the stock feature flags, call `add_common_permutations` and bind through `bindDeferredShader` (C5).
- The texture-channel count of the two new programs is logged (C6).
- New invariant: actor spot draws come after both world spot blocks (C8).
- B5 now notes that mirrors at probe level 0 skip all local lights (C3).
- The dead `isMirrorPass` clause in the probe-centric branch is removed (C9).
- Fable's shader-depth-sampling alternative is recorded as a **fallback only** (§5.4.3).

### 2.4 Findings of my own carried from V2 (PROVES unless marked)

- **N1.** `LLRenderTarget::clear()` also clears a shared depth attachment (llrendertarget.cpp:569-577).
  The mask must clear with `clear(GL_COLOR_BUFFER_BIT)` only and must never write depth.
- **N2.** `LLLightState::setPosition`/`setSpotDirection` use the current modelview (llrender.cpp:680-689,
  736-744). `enableLights()` rewrites diffuse from `mHWLightColors[]` (pipeline.cpp:10751-10780). Actor
  sets are therefore captured inside `setupHWLights()` and applied as raw content.
- **N3.** `multiPointLightF.glsl` loops over the compile-time `LIGHT_COUNT` (multiPointLightF.glsl:109,
  :146), so unused entries are padded with an inert sentinel.
- **N4 (INFERENCE for the G-buffer part).** Shared Actor FX Replace/Cover suppress native draws
  (lldrawpool.cpp:921-957, 960-1017). This is handled by the eligibility recorder (§5.4.1).
- **N5.** G-buffer shaders call `mirrorClip` (diffuseF.glsl:37, :62; plane at llsettingsvo.cpp:1287-1307).
  The mask shaders do the same.
- **N6.** Fullbright faces are drawn after deferred lighting and are excluded from the mask.
- **N7 (INFERENCE).** The mask mirrors G-buffer culling.
- **N8.** The shadow auction is categorical rig-over-world (pipeline.cpp:24391-24402). The Prism feed
  selects spot shadows nearest-first (27156-27219).
- **N9.** `appendCinematicProbeLightSignature` already skips rig emitters (pipeline.cpp:9770-9773).
- **N10.** The on-demand scheduler samples rig emitters (llreflectionmapmanager.cpp:1254-1317, 1755-1786).
- **N11.** Model copies of `LightBase` are field-by-field after a `memset` (cleanLight 243-246,
  blendLight 1106-1107).
- **N12.** SELF aliasing (alcinelightrig.cpp:163-166 vs 1051-1064). **Resolved** by recipient bits
  (Codex R1 #5).

**Corrections to V1:**
- Settings live in `app_settings/settings.xml`.
- V1's "Hide shafts" option is dropped (B4; a per-light Shaft toggle already exists).
- Catchlight Actor-only is **off** by default (D7).

---

## 3. Scope

**In this delivery (ship whole):**
1. WP0 registry: `anyActorOnly()`, `actorOnlyBits(id)`, the recipient bit table, `light_bits`.
2. Actor Mask target, its lifecycle, the G-buffer eligibility recorder, the mask pass, view
   classification.
3. Actor-only routing of deferred local lights (two new fullscreen programs plus the two wrapper files).
4. Three-tier spot-shadow auction.
5. Forward world set and actor sets: the alpha pool, the interleaved stream, Shared Actor FX proxies.
6. Exclusion from probes and the Live probe, Live bounce handling, on-demand scheduler eligibility.
7. Per-light **Actor only**, plus rig-level **Bounce lights the set** and **Catchlight Actor only**. These
   persist through settings, the param blob, setup presets, cues and scene save.
8. UI in the rig panel (also embedded in the Director), Lightbox debug controls, tooltips, the reset list.
9. Verdict log, debug overlay, extended invariant guard, automated isolation/parity test.

**Deferred (separate brief):** an ABI-appended ReShade `SL_ACTOR_MASK` semantic. It is the only remedy
for B1.

**Hard constraints (unchanged):**
- G-buffer layout.
- Depth format (`DEPTH_FMT_24`, pipeline.cpp:1280).
- The ReShade bridge (`0x81A6`, llreshadebridge.cpp:209-214).
- Every existing shader file and program object.
- Any light that is not an enabled rig light flagged Actor-only.

---

## 4. Anchor table at HEAD `057e55885f9`

| Item | HEAD anchor |
|---|---|
| Slot enum SELF..D | alcinelightrig.h:100-108 |
| `GROUP_SLOT_*` bits (SELF=1<<0 .. D=1<<4, mask 0x1F) | alcinelightrig.h:173-181 |
| `resolveSlotAvatar` (legacy SELF anchor at 1060-1064) / `projectorId` / `omniId` | alcinelightrig.cpp:1051-1066 / 1068-1076 / 1079-1087 |
| `gatherGroupMembers` (Group SELF = `resolve(null)` at 165-166; deduplicates the same avatar) | alcinelightrig.cpp:159-209 |
| Live ignored ids / projector ids | alcinelightrig.cpp:1110-1126 / 1128-1140 |
| Rig Live signature / animating | alcinelightrig.cpp:1175-1205 / 1207-1230 |
| `mCatchlight` (no id accessor) | alcinelightrig.h:393 |
| `tickShared`: object branch / group branch / avatar | alcinelightrig.cpp:2362-2407 / 2409-2501 / 2423-2426 |
| Live bounce scale applied | alcinelightrig.cpp:2537 |
| Catchlight destroyed for object targets / applied | alcinelightrig.cpp:2559-2563 / 2826-2839 |
| FX modifier copy loop | alcinelightrig.cpp:2733-2758 |
| `applyFrame` omni write | alcinelightrig.cpp:2012-2039 |
| Manager `tick` (rig loop ends 951, `updateLiveProbe` 958) | alcinelightrigmanager.cpp:914-959 |
| `updateLiveProbe` → `setCinematicLiveProbe` | alcinelightrigmanager.cpp:657-662 |
| `ParamBlob::Light` / positional `mLights[]` rows (11 members) | alcinelightrigmanager.h:39-69 / 145-154 |
| `LightBase` / `Cue` / `CueState` | alcinelightrigmodel.h:80-103 / 153-169 / 178-185 |
| `renderGeomDeferred` (`setupHWLights` 7291, pool loop 7293+) | pipeline.cpp:7260 |
| G-buffer callers | llviewerdisplay.cpp:1080 (main), 1381 (cube face); llprismlens.cpp:7584 (Prism) |
| `renderDeferredLighting` / `preserveReShadeGBuffer` / `sCull` return | pipeline.cpp:23016 / 23033 / 23035-23038 |
| Deferred light loop: null check / Live ignore / count break | pipeline.cpp:23386-23393 / 23399-23404 / **23427-23431** |
| Point vol / spot vol / multi-point / multi-spot | pipeline.cpp:23369-23528 / 23530-23570 / 23572-23634 / 23636-23671 |
| `renderGeomPostDeferred` call / its `calcNearbyLights`+`setupHWLights` | pipeline.cpp:23754 / 7617-7618 |
| Ghost forward stages | pipeline.cpp:23748-23775 |
| `calcNearbyLights`: probe-centric / Prism / main | pipeline.cpp:9844-9941 / 9943-10024 / 10026-10196 |
| `setupHWLights`: loop / Live ignore 10585-10595 / Live count break 10618-10622 / fade 10636-10652 / unit end **10722-10726** / fill 10729-10738 / disable 10744-10748 | pipeline.cpp:10576-10727 |
| `enableLights` | pipeline.cpp:10751-10780 |
| Spot shadow auction (null incumbent 24384-24389, swap 24400-24408) | pipeline.cpp:24372-24410 |
| Prism spot-shadow selection | pipeline.cpp:27156-27219 |
| Light program registration / unload | llviewershadermgr.cpp:2639-2721 / 1620-1626 |
| Shadow alpha-mask program family (fragment-side starting point only; vertex follows the A4 rule) | llviewershadermgr.cpp:3431-3449, 3554-3590 |
| `createShader`: hash / binary cache / per-file compile with `&mDefines` / feature attach | llglslshader.cpp:443 / 457 / **466-479** / 483 |
| Library shader objects (precompiled, shared) | llglslshader.cpp:569-596 |
| Define injection at `[EXTRA_CODE_HERE]` | llshadermgr.cpp:730-733 (permutation defines), 862-895 (marker insertion) |
| Reserved uniforms end | llshadermgr.h:917-919; llshadermgr.cpp:2140-2146 |
| `LLRender` light snapshot API / `LLLightState` / `syncLightState` | llrender.h:527-535, 243-288; llrender.cpp:1654-1668, 903-967 |
| `LLRenderTarget` share / release / clear / `getDepth` | llrendertarget.cpp:421-455 / 457-487 / 569-586; llrendertarget.h:169 |
| Suppression: beauty context (reads bound shader) / zero-cover context | lldrawpool.cpp:861-881 / 883-905 |
| Suppression predicates: by id / by draw / PBR by draw | lldrawpool.cpp:940-957 / 960-990 / 993+ |
| G-buffer suppression call sites | lldrawpool.cpp:1245, 1350, 1477, 1535, 1675, 1722, 1791, 1883, 1939; lldrawpoolbump.cpp:413, 1068; lldrawpoolmaterials.cpp:126, 323; lldrawpoolavatar.cpp:548, 889, 1088, 1132 |
| Alpha pool: proxy turn / `applyModelMatrix` / **suppression test** / palette upload | lldrawpoolalpha.cpp:969-999 / 1096 / **1209-1218** / 1220 |
| Draw-info owners | llspatialpartition.h:113-119 |
| `getActorFxOwnerId` | llvoavatar.h:530; llvoavatar.cpp:12365 |
| ALR keep-live hook | llvoavatar.cpp:4643 |
| Ghost harvest | llactormover.cpp:13794-13822 (passes), 13830-13957 (walk; temp policy 13843-13844; control-avatar branch 13890-13897) |
| System body replay pattern | llactormover.cpp:14861-14899, 14901+ |
| Shared Actor FX proxies | llactormover.cpp:16932-17020 |
| Ghost invariant guard | llghostdeferreddiagnostics.h:25-44, 61-88 |
| Mirror/Prism clip | diffuseF.glsl:37/62; llsettingsvo.cpp:1287-1307; uploads at 1312 (`CLIP_PLANE`) and 1315 (`MIRROR_FLAG`); reserved names llshadermgr.h:104-105 |
| Environment-uniform dirtying over `mShaderList` | llenvironment.cpp:1676-1690 (rigged variant 1681-1683) |
| `mShaderList` push pattern (actorghost) | llviewershadermgr.cpp:614-627 |
| G-buffer clear colour (magenta, grey under wireframe) | llviewerdisplay.cpp:1026, 1030 (main); 1369, 1373 (cube) |
| `renderDeferredLighting` colour mask / clear-colour pair | pipeline.cpp:23066 / 23096-23098 |
| Avatar pool: suppression test / cloth wind uniforms / `renderSkinned` | lldrawpoolavatar.cpp:1132 / 1152-1175 / 1179 |
| Actorghost body cloth upload (template for the mask body) | llactormover.cpp:15122-15146 |
| Rig panel per-light rows / Director embed / reset list | panel_cine_light_rig.xml:321-322, 390-391, 455-456, 520-521 / floater_director.xml:1683-1684 / alpanelcinelightrig.cpp:471+ |

The remaining anchors (the release/alloc sites in pipeline.cpp, llviewerdisplay order, Prism, probes)
are cited inline where they are used.

---

## 5. Design

### 5.1 Registry and data model (WP0, frozen before anything else)

**Per light (model):** `bool LightBase::mActorOnly = false;` is appended after `mFixturePreset`
(alcinelightrigmodel.h:102). Presets, cues and transitions carry it. `blendLight` switches it at the
midpoint (`eased > 0.5f ? target : start`).

**Per rig (not cue-animated):**
- `mBounceLightsSet` (default **false**). When false, omni *i* follows light *i*.
- `mCatchlightActorOnly` (default **false**).

**Recipients (Codex R1 #5).** A recipient is a unique resolved avatar. For each enabled rig that is not
object-targeted:
- **Non-group rig:** its recipient set is `{resolveSlotAvatar()}` when non-null. A SELF rig with a legacy
  `mAnchor` therefore yields the **anchor** avatar (alcinelightrig.cpp:1060-1064).
- **Group rig:** its recipient set is the avatars that the set bits of `lastResolvedGroupSlots()`
  resolve to. The resolution is the one `gatherGroupMembers` uses: SELF → `resolve(null)`, A..D →
  `resolveSubjectA..D` (163-175).

The two SELF paths can now never share a bit unless they really are the same avatar.

**Bit allocation.** Every distinct recipient (keyed by avatar UUID) that appears in any actor-only
light's recipient set gets its own bit, `recipientBit(av) = 1 << k`, with `k` in 0..7.
- Assignment order is deterministic: rigs in slot order SELF..D, then members in resolution order.
- At most 6 distinct recipients can exist: SELF, A-D, plus one legacy anchor. R8 has 8 bits.
- Defensively: a recipient beyond 8 gets no bit. Its lights light nothing on it (fail closed), and the
  registry logs `ACTORMASK RECIPIENT-OVERFLOW` once.

**`light_bits`** = OR of `recipientBit` over that light's rig recipient set:
- Projector *i*: only if `mCurrentLive[i].mActorOnly`. That is the state after transition, cue and FX.
- Omni *i*: the projector's bits, unless `mBounceLightsSet`.
- Catchlight: only if `mCatchlightActorOnly`.
- Object-targeted rig: 0 for every emitter (D4). Log `ACTORMASK OBJECT-TARGET <slot>` once per
  transition.

**Ownership rule for geometry (Codex R1 #6).** Used by the mask replay **and** by forward selection:
- Draw infos: `drawByte(di) = byteForOwner(di.mActorFxOwner) | byteForOwner(di.mActorFxFallbackOwner)`.
  Null owners contribute 0.
- System body and eyes: `recipientBit(av)`.

Worn animesh that is also a cast subject therefore carries both its own bit and the wearer's bit, so it
is lit by both the wearer's lights and its own.

**Frozen API** (alcinelightrigmanager.h). The logic lives in a pure `ALCineLightRigManagerModel`
function so it can be unit-tested:

```cpp
struct ActorOnlyRecipient { LLPointer<LLVOAvatar> mAvatar; LLUUID mAvatarId; LLUUID mFxOwner; U8 mBit; };
static bool anyActorOnly();                         // static cached bool; false when the kill switch is set
U8   actorOnlyBits(const LLUUID& light_id) const;   // 0 = world light
U8   recipientBit(const LLVOAvatar* av) const;      // 0 = not a recipient
U8   byteForOwner(const LLUUID& fx_owner) const;    // matches LLDrawInfo owners
U8   drawByte(const LLDrawInfo& di) const;          // the Codex R1 #6 union
const std::vector<ActorOnlyRecipient>& actorOnlyRecipients() const;
bool isActorOnlyRecipient(const LLVOAvatar* av) const;  // keep-live hook (A8)
```

**Rebuild and caching:**
- The registry is rebuilt in `ALCineLightRigManager::tick()` after the rig loop (after line 951) and
  before `updateLiveProbe()` (958). It holds at most 45 light entries and 6 recipients.
- `mFxOwner = avatar->getActorFxOwnerId()`. Dead avatars are skipped at use.
- `anyActorOnly()` = `!CineLightRigActorMaskDisable && table non-empty`, held in a static bool.

**Also in WP0:**
- `ALCineLightRig::catchlightId()` (C3).
- The two rig-level members with their setters.
- At llvoavatar.cpp:4643: `|| (ALCineLightRigManager::anyActorOnly() && ALCineLightRigManager::instance().isActorOnlyRecipient(this))`.

### 5.2 View classification (WP1)

Classification is computed once at the top of `renderDeferredLighting()`, after
`preserveReShadeGBuffer()` (23033) and the `sCull` return (23035-23038). It is stored in
`LLPipeline::mActorMaskView`, and an RAII guard resets it to `NONE` on every exit.

| Condition (in order) | Class |
|---|---|
| `!anyActorOnly()` | `NONE` |
| `sImpostorRender` or `sRenderingHUDs` | `EXCLUDE` |
| `gCubeSnapshot && mHeroProbeManager.isMirrorPass()` (pack `mHeroProbeRT`) | `MASK` |
| `gCubeSnapshot && !isMirrorPass()` (reflection probes, Live, on-demand faces) | `EXCLUDE` |
| `sPrismLensRender` (active Prism scratch pack, llprismlens.cpp:7586) | `MASK` |
| `mRT == &mMainRT` (main view, including tiled hi-res snapshots) | `MASK` |
| anything else | `EXCLUDE`; log `ACTORMASK VIEW-UNCLASSIFIED` once |

The three callers at HEAD are llviewerdisplay.cpp:1120, llprismlens.cpp:7586 and
llviewerdisplay.cpp:1388. If a `MASK` view cannot produce a valid mask, it becomes `EXCLUDE` for that view
(fail closed) and logs a verdict.

### 5.3 Mask target lifecycle (WP1)

- `RenderTargetPack` gains `LLRenderTarget actorMask` and `U32 actorMaskDepthSrc`. The format is `GL_R8`,
  with no depth of its own.
- **Release before any depth reallocation:**
  - `release_pack` (pipeline.cpp:2299-2311) and `release_prism_scratch_entry` (1574-1586): release the
    mask before `deferredScreen.release()`.
  - Top of `allocateScreenBufferInternal`: release the current pack's mask before line 1280.
- **Lazy ensure** at the start of each `MASK` view:
  - If the mask is missing, its size differs, or `actorMaskDepthSrc != deferredScreen.getDepth()`:
    release, `allocate(w, h, GL_R8, false)`, `deferredScreen.shareDepthBuffer(actorMask)`, record the
    depth id.
  - If it still mismatches, log `ACTORMASK STALE`; the view becomes `EXCLUDE`.
  - Release always comes first because `shareDepthBuffer` raises LL_ERRS on a target that already has
    depth (llrendertarget.cpp:430-438).
- **Freed when unused:** at the main view, if `!anyActorOnly()`, every pack's mask is released.
- **Clear (Fable A1):**
  1. Save the current clear colour and colour mask.
  2. `gGL.setColorMask(true, true); glClearColor(0, 0, 0, 0);`.
  3. `actorMask.clear(GL_COLOR_BUFFER_BIT)` only (N1). Never clear or write depth.
  4. Restore both values on exit.

  `clear()` is a bare `glClear` with the current clear colour (llrendertarget.cpp:569-596). At this
  site that colour is still the G-buffer magenta (llviewerdisplay.cpp:1030, 1373), so a mask cleared
  without step 2 reads 255 everywhere.

### 5.4 The mask pass (WP1)

The mask pass runs once per `MASK` view, right after classification. At that point the view's matrices
are current and its G-buffer depth is final. In the main view that depth includes ghost-proxy depth.

**State (all RAII, restored on exit):**
- Bind `actorMask` (FBO and viewport).
- `LLGLDisable(GL_BLEND)`.
- `LLGLDepthTest(GL_TRUE, GL_FALSE, GL_LEQUAL)`.
- `LLGLEnable(GL_POLYGON_OFFSET_FILL)` with `glPolygonOffset(0, CineLightRigActorMaskBias)`, then
  `glPolygonOffset(0, 0)` on exit.
- G-buffer culling: back-face, except double-sided GLTF (N7).
- Colour mask `(true,true)` and clear colour `(0,0,0,0)` set for the clear (§5.3), then both restored.
- **No** `blendFunc`, **no** scissor, **no** logic-op.
- Wrapped in the extended invariant guard (§9).

#### 5.4.1 G-buffer eligibility recorder (Codex R1 #1)

The native-draw suppression predicates read the **currently bound shader** (lldrawpool.cpp:861-881), so
they cannot be re-evaluated correctly from the mask pass. V3 records what the real G-buffer submission
did instead.

**Recorder:** `LLActorMaskEligibility`, in a new file owned by WP1.
- **Armed:** an RAII scope inside `renderGeomDeferred` (pipeline.cpp:7260) arms it for the duration of
  the pool loop, only when `anyActorOnly()`.
- **Stamp:** each arming clears it and stamps it with the view identity:
  `{gFrameCount, mRT, gCubeSnapshot, isMirrorPass(), sPrismLensRender, active Prism scratch}`.
- **Recording:** while armed, the three suppression predicates record the cases where they return
  **true**. The guard is `if (armed) record(...)`, and it sits on the true branch only, so it is inert
  when disarmed.
  - By draw (lldrawpool.cpp:960) and PBR by draw (993): insert `&params` into
    `std::unordered_set<const LLDrawInfo*> mSuppressedDraws`.
  - By id (940), reached from the body call sites in lldrawpoolavatar.cpp:548/889/1088/1132: insert
    `actor_id` into `mSuppressedBodies`.

  These predicates are called from every G-buffer pool (call sites in §4), always under the shader that
  actually drew, so the record is the true native eligibility for that view.
- **Consumption:** the mask pass checks the recorder **only if** its stamp equals the current view's
  identity. It then:
  - skips every draw info in `mSuppressedDraws`;
  - skips the body and eyes of any recipient whose `getActorFxOwnerId()` is in `mSuppressedBodies`;
  - logs `ACTORMASK STYLED <recipient>` once per change when anything was skipped.

  On a stamp mismatch (for example, an unexpected caller order), every recipient that has an enabled
  Shared Actor FX style is skipped entirely (fail closed), with `STYLED`.

**The mask then covers what was natively drawn into this view's G-buffer** (modulo culling, which is
conservative: a culled group cannot reach the visible depth).

#### 5.4.2 What is drawn per recipient

Each draw writes `byte / 255.0` (exact in UNORM8).

1. **Skip, with a verdict and no draw (fail closed):**
   - Avatar outside the frustum: `OFFSCREEN`.
   - Impostored, visually muted, jellydolled, too complex or `isTooSlow`: `IMPOSTOR` (A8, B7).
2. **Body and eyes** (non-control avatars): the `renderSystemActorGhost` pattern
   (llactormover.cpp:14901+), with `LLDrawPoolAvatar::sMinimumAlpha`.
   - Value: `recipientBit(av)`.
   - Skipped if recorded as suppressed (§5.4.1).
3. **Attachments, rigged and unrigged, and worn or standalone animesh:** `walkGhostSourceGeometry`
   (13830) with two new optional parameters. Their defaults reproduce today's behaviour, so the existing
   callers (llactormover.cpp:14152, llclonefidelityaudit.cpp:801) are untouched.
   - `temp_policy_override`: the mask passes `INCLUDE`.
   - `group_cb`: receives each collected `LLSpatialGroup*`.

   For each group, draw the draw infos of this **include-list** of passes, with each draw info's model
   matrix and palette. **Value: `drawByte(di)`** (§5.1). Skip draw infos with `drawByte == 0` and those
   in `mSuppressedDraws`.
   - Rigged: `PASS_SIMPLE_RIGGED, PASS_SHINY_RIGGED, PASS_BUMP_RIGGED, PASS_MATERIAL_RIGGED,
     PASS_MATERIAL_ALPHA_MASK_RIGGED, PASS_SPECMAP_RIGGED, PASS_SPECMAP_MASK_RIGGED, PASS_NORMMAP_RIGGED,
     PASS_NORMMAP_MASK_RIGGED, PASS_NORMSPEC_RIGGED, PASS_NORMSPEC_MASK_RIGGED, PASS_ALPHA_MASK_RIGGED,
     PASS_GLTF_PBR_RIGGED, PASS_GLTF_PBR_ALPHA_MASK_RIGGED`.
   - Unrigged: the same list without `_RIGGED`.
   - Excluded: blend, emissive, glow and **all fullbright** passes (N6).
   - The implementer checks every enum against `llrender/lldrawpool.h` and lists the final set in the
     WP1 report.

   The same geometry can be reached from two recipients' walks: the wearer's attachment walk and a cast
   animesh's control-avatar branch (13890-13897). Both write the same `drawByte`, so the result is
   idempotent.
4. **Alpha-mask draws:** use the draw's own cutoff and texture transform, mirroring exactly the discard
   of the G-buffer program that drew the draw.
5. **Mirror clip (N5, Fable A2):**
   - Every mask fragment shader calls `mirrorClip(vary_position)`.
   - Every mask program is **pushed onto `mShaderList`** using the actorghost pattern
     (llviewershadermgr.cpp:614-627). That way `LLEnvironment::update` dirties it
     (llenvironment.cpp:1676-1690, rigged variants 1681-1683), and `mirror_flag`/`clipPlane` are uploaded
     at bind (llsettingsvo.cpp:1312, 1315).
   - The WP1 report verifies `getUniformLocation(LLShaderMgr::MIRROR_FLAG) >= 0` for each mask program.
   - At runtime: in a mirror `MASK` view, or a Prism view with an active lens clip, if any bound mask
     program lacks the `MIRROR_FLAG` location, log `ACTORMASK NO-CLIP` and drop the view to `EXCLUDE`.

**Programs:** new program objects only: static, rigged, GLTF alpha-mask (+ rigged), avatar body,
eyeball. That is at most 6. They are added to the unload list, and every bind is guarded by
`isComplete()`. If one is missing, log `ACTORMASK NO-PROGRAM` and the view becomes `EXCLUDE`.

**Depth-agreement rule (Fable A4):** depth agreement is a rule for each program, not a family template.
Each mask vertex shader uses the identical `gl_Position` expression, uniform set and helper file
(`objectSkinV.glsl`, `avatarSkinV.glsl`) as the G-buffer program that drew that geometry:
- **Static:** `modelview_projection_matrix * vec4(position, 1)`, the form used by
  diffuseV/pbropaqueV/materialV/bumpV.
- **Rigged (`HAS_SKIN`):** `mat = modelview_matrix * getObjectSkinnedTransform(); pos = mat * p; gl_Position = projection_matrix * pos`.
- **System body:** the `avatarV` transform, **including the `AVATAR_CLOTH` block**. The permutation is
  mirrored from `gDeferredAvatarProgram` whenever `RenderAvatarCloth` is on. The three wind uniforms
  (`AVATAR_WIND`, `AVATAR_SINWAVE`, `AVATAR_GRAVITY`) are uploaded exactly as the actorghost replay does
  at llactormover.cpp:15122-15146, which mirrors lldrawpoolavatar.cpp:1152-1175. The shadow avatar
  template is **not** enough: it has no cloth path, so displaced skirt and lower-body vertices would leave
  holes.
- **Eyes:** the static form.

The shadow alpha-mask family (llviewershadermgr.cpp:3431-3449, 3554-3590) stays a starting point for the
fragment side only.

**The WP1 report lists each mask program's `gl_Position` expression beside its G-buffer counterpart**, and
the reviewer verifies them by reading, not by compiling. Under `RenderDepthPrePass` the static
pre-pass uses the same static MVP form; note this in the report.

**Ordering and ties:**
- At an exact depth tie inside one avatar, every draw writes the same byte, so the result is harmless.
- At an exact tie between two avatars, the beauty pass z-fights too, so either owner is legitimate.
- Overwrite makes same-byte overlaps idempotent (A2).

**Queries:** tagged per recipient (§9.1).

**Polygon offset:** units only, factor 0. `CineLightRigActorMaskBias` defaults to **-8.0 (GUESS)**, range
-64..0. In-world test 1 bisects toward 0 starting from -8. Only holes are at stake: the offset is tens of
micrometres at 5 m, far below hand-on-shoulder spacing.

#### 5.4.3 Fallback only: depth sampling in the mask shader (Fable R1 §6)

**Use this only if** in-world test 1's bias bisect shows the shared-depth LEQUAL-plus-offset route is
flaky on the user's driver. It is not a plan for v1.

If needed, the mask pass would stop using the shared depth and the polygon offset entirely. Instead:
- The mask fragment shader samples `deferredScreen`'s depth texture. It is not attached to the mask FBO,
  so there is no feedback loop.
- It compares linear depths with a metric tolerance that scales with `z^2`.

The cost is one depth fetch per mask fragment, plus the loss of hardware early-Z on mask draws.

Adopting it is a separate fix round with its own review.

### 5.5 Deferred actor-only lights (WP2)

**World loop: the light-off baseline is preserved exactly (Codex R1 #3).** Right after the
`if (!volume) continue;` check (23390-23393), and before the Live-ignore test, the toggles, `count++`
and the count break (23427-23431), add:

`if (anyActorOnly() && actorOnlyBits(volume->getID())) continue;`

It has no side effects. World lights see the same order, the same `count` and the same break point as
when the actor-only lights are switched off.

**Actor collection: an independent, uncapped scan.** This runs only in a `MASK` view with a valid mask,
**after** the world loop, so the world loop's break can never hide an actor light.
- Scan `mNearbyLights` again. For each entry with `bits = actorOnlyBits(id) != 0`, apply the world
  loop's reject tests:
  - colour, computed with the identical expression
    `light_scale * ALEnvIntensity::localLightEVScale(volume)`;
  - size;
  - `AABBInFrustumNoFarClip`.
- Push the light to `actor_points` or `actor_spots`.
- No count limit applies: the registry bounds actor lights to 45.
- Spots call `updateSpotLightPriority()` outside Prism, as today.

**Decision: route actor-only lights through the two fullscreen paths only (2 new light programs).**
- **Spots (PROVES):** the spot volume and fullscreen spot programs compile the same `spotLightF.glsl`.
  `MULTI_SPOTLIGHT` only swaps how the centre is sourced (spotLightF.glsl:53-57, 119-123). The catchlight
  and every projector are spots.
- **Points (PROVES the differences; INFERENCE that they are invisible):**
  - The legacy branch has a back-face eps term.
  - Multi-point has a `far_z` discard.
  - Stock already swaps lights between these two programs when the camera crosses a light box
    (pipeline.cpp:23464-23521).
  - The only actor-only points are bounce omnis.
- **Stated test:** the instrument's **PARITY** verdict (§9.3), plus in-world test 3.
- **If PARITY fails:** add the two volume variants (4 programs in total). It is never 19, thanks to the
  padding below.

**New programs** (llviewershadermgr.cpp, registered after the existing four, added to the unload list):

1. `gDeferredMultiLightActorMaskProgram`: `LIGHT_COUNT = 16`.
   - Vertex: `multiPointLightV.glsl` + `deferred/actorMaskGateV.glsl`.
   - Fragment: `multiPointLightF.glsl` + `deferred/actorMaskGateF.glsl`.
2. `gDeferredMultiSpotLightActorMaskProgram`: `MULTI_SPOTLIGHT = 1`.
   - Vertex: `multiPointLightV.glsl` + `actorMaskGateV.glsl`.
   - Fragment: `spotLightF.glsl` + `actorMaskGateF.glsl`.

**Program setup (Fable C5, C6):**
- Both programs copy the stock light programs' feature flags verbatim (`isDeferred`, `hasFullGBuffer`,
  `hasShadows`, `hasSrgb`; llviewershadermgr.cpp:2639-2721) and call `add_common_permutations`.
  Otherwise `rigRimMode()`, the shadow samplers and the `GET_GBUFFER_FLAG` preamble would differ from the
  bodies they wrap.
- They bind through `bindDeferredShader` (pipeline.cpp:22665), like the stock programs.
- They are deliberately **not** in `mShaderList`: they take no environment uniforms.
- The WP2 report logs each program's `mActiveTextureChannels` against `gGLManager.mNumTextureImageUnits`.
  `createShader` warns above 24 channels, and the mask sampler adds one.

**Main-rename wrappers (Codex R1 #2).** Each new program calls `addPermutation("main", "actorMaskedMain")`
**after** `clearPermutations()`. `createShader` passes `mDefines` to **every** listed file
(llglslshader.cpp:466-469), so the light's vertex **and** fragment `main` both become `actorMaskedMain`.
Each stage gets its own wrapper that supplies the real `main`.

```glsl
// deferred/actorMaskGateV.glsl
/*[EXTRA_CODE_HERE]*/
#undef main
void actorMaskedMain();
void main() { actorMaskedMain(); }
```

```glsl
// deferred/actorMaskGateF.glsl
/*[EXTRA_CODE_HERE]*/
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

**Why the wrappers compile and link:**
- Permutation defines are injected at the `[EXTRA_CODE_HERE]` marker (llshadermgr.cpp:730-733, 862-895),
  so `#undef main` follows the define in each wrapper.
- Library objects attached by `attachShaderFeatures` are precompiled shared objects, not compiled with
  the program's defines (llglslshader.cpp:483, 569-596). They contain no `main`, so they are unaffected.
- The light files compile with the **same source text** as the stock programs. ALR Tame, Rig Rim, Tron
  tint and EnvIntensity carry over unchanged.

**No source-edit fallback exists:** existing shader files stay byte-identical, with no exceptions. If
the wrappers fail on the user's driver, the program reports `ACTORMASK NO-PROGRAM`, the feature fails
closed, and the next fix round must find another wrapper-only mechanism.

**Reserved uniforms:** `ACTOR_MASK_MAP` ("actorMaskMap") and `ACTOR_MASK_BITS` ("actor_mask_bits").
Append them after `SHADOW_LIFT_GAIN` (llshadermgr.h:917-919; llshadermgr.cpp:2144), in lockstep, and
count them (rule 6).

**Draws** go after the existing multi-spot block (~23671), in the same `BT_ADD` scope:
- **Points:**
  - Group `actor_points` by `bits`, in chunks of 16.
  - Pack `light/col/rim` as at 23587-23600.
  - **Pad** unused entries with `light = (0, 0, 1e9, 1)`, colour 0, rim 0.
  - Compute `far_z` from real lights only.
  - Set `actor_mask_bits`, bind the mask texture, draw.
- **Spots:** for each spot: `setupSpotLight()`, the same uniforms as 23654-23664, `actor_mask_bits`, draw.
- No scissor in v1. Tracy zone: `"actor-only lights"`.
- **Invariant (Fable C8):** the actor spot draws come **after both world spot blocks** (spot volumes
  23530-23570 and multi-spot 23636-23671). The shadow auction runs inside `setupSpotLight`, so this
  order ensures every world auction completes before any tier-0 candidate appears. Moving the actor
  block earlier would silently turn "fills empty slots" into "fills, then gets displaced".

### 5.6 Spot-shadow tiers (WP2, Codex R1 #4)

In the auction (pipeline.cpp:24379-24409), define for a non-null volume:
`tier(v) = isCineRigEmitter(v) ? (actorOnlyBits(v->getID()) ? 0 : 2) : 1`.

Per slot *i*:

1. **If `potential` is null:** `break`. Today a null potential never outranks anything, because it is
   not a rig emitter and `0 > pri` is false for every incumbent. So stopping is byte-identical to
   today's outcome, and it also handles a null displaced candidate explicitly.
2. **If the incumbent slot is null:**
   - An actor-only potential (tier 0) **always fills it**. A null slot ranks below every real
     candidate, so actor-only self-shadows use free slots.
   - Rig (tier 2) and world (tier 1) potentials keep today's comparison against a null slot
     **verbatim** (`potential_is_rig ? true : m_pri > 0`), so the baseline stays byte-identical.
3. **Both non-null:** `outranks = tier_p != tier_i ? tier_p > tier_i : m_pri > pri`.
4. **On a swap:** `potential` becomes the displaced incumbent, which may be null. If it is null, rule 1
   ends the loop on the next slot.

With no actor-only light present, tiers 2 and 1 reproduce today's categorical rule, and rules 1 and 2
reproduce today's null behaviour. Actor-only projectors only take slots that are empty in the light-off
baseline.

In `generatePrismSpotShadows` (27156-27219), sort actor-only candidates after all others and keep the
existing order within each group.

This byte-identity depends on the draw-order invariant in §5.5: actor spot draws come after both world
spot blocks.

### 5.7 Forward lighting: world set and actor sets (WP3)

**World loop (every view, whenever `anyActorOnly()`), Codex R1 #3.** In `setupHWLights()`, the first
statement after the null check (10581-10584) is:

`if (anyActorOnly() && actorOnlyBits(light->getID())) continue;`

It comes before the Live ignore/pinned tests, the Live count (10618-10622), the fade write
(10636-10652), the unit write and the unit-end break (10722-10726). World lights therefore get exactly
the light-off units, fades and `mLightMovingMask`.

**Actor scan: independent, after the world loop closes**, and only when `mActorMaskView == MASK` with a
valid mask.
- Scan `mNearbyLights` again, uncapped, for entries with non-zero bits.
- Apply the colour and radius rejects.
- Advance each one's fade clock **once** per `setupHWLights()` call.
- Keep them nearest first.

**Building actor sets** (same call, so the view's modelview is current):
1. Snapshot the world content with `gGL.getLightStateSnapshot` and save `mHWLightColors[2..7]`.
2. For each distinct `drawByte` value *b* that can occur, build one set:
   - Candidate values are each recipient's bit, plus the union value of any worn-animesh owner pair
     (computed by the registry).
   - The set's units are the actor lights with `bits & b`, then world units `0..(6 - n)`. Write them
     with the factored per-unit helper (today's body at 10632-10721), snapshot the result, and save its
     `mHWLightColors`.
   - More than 6 matching actor lights increments `alpha_slot_overflow` (B2).
3. Restore the world content before the disable loop (10744-10748), which is unchanged.

**Applying a set:**
- New `LLRender::restoreLightUnitContent(snapshot, 2, 7)` copies every field except `mIndex`/`mEnabled`,
  writes black diffuse for units disabled in the current mask, and bumps `mLightHash` once.
- New `LLPipeline::selectForwardLightSet(U8 byte)` calls it and swaps `mHWLightColors[2..7]`. It does
  nothing if the selection is unchanged.

**Where the set is selected** (lldrawpoolalpha.cpp `renderAlpha`):
- **Native draws: after the existing in-context suppression test (1209-1218) and before the palette
  upload (1220).** Only a draw that is actually drawn ever selects a set, and its eligibility is the
  one evaluated under the real forward shader. This is the same principle as §5.4.1: eligibility always
  comes from the real submission, never from a foreign shader context.
  - `byte = drawByte(params)` (the Codex R1 #6 union).
  - It is forced to 0 when the view is not `MASK`, or under `sRenderingHUDs` / `sImpostorRender`.
- **Shared Actor FX proxy turn** (969-999): select `recipientBit(proxy avatar)` through the new
  `LLActorMover::sharedActorStyleProxyAvatar(index)` (the proxy holds `mAvatar`, 16932-16960). Restore
  the world set afterwards.
- **Every `renderAlpha` exit:** an RAII guard restores the world set.
- **System avatar alpha (Fable A3).** Hair, skirt and eyelashes are forward-lit by
  `gDeferredAvatarAlphaProgram` through `LLDrawPoolAvatar::renderPostDeferred`, which is outside
  `LLDrawPoolAlpha`.
  - In `LLDrawPoolAvatar::renderAvatars`, on the post-deferred path only, call
    `selectForwardLightSet(recipientBit(avatarp))` **after** the suppression test at
    lldrawpoolavatar.cpp:1132 (so eligibility stays with the real submission) and **before**
    `renderSkinned()` at :1179.
  - Restore the world set afterwards with RAII.
  - The byte is forced to 0 when the view is not `MASK`, and for impostor pass 0 (B7).
  - Each switch counts in `alpha_switches`.

Ghost proxy and overlay replays (23748-23775) run outside `renderAlpha` and always see the world set.

**To verify:** a hash bump between two draws that share a bound shader must re-upload before the second
draw (precedent: `setRigRimActive`, llrender.cpp:888-900).

### 5.8 Probes, Live probe, bounce (WP4)

| Site | Change |
|---|---|
| Deferred loop and `setupHWLights` in cube faces | Covered by §5.2, §5.5 and §5.7 |
| `calcNearbyLights` probe-centric branch (9875-9939) | Skip actor-only lights at insertion. (No mirror clause is needed: the hero mirror never enters this branch and reuses the main-eye list. Fable C9.) |
| `appendCinematicProbeLightSignature` | **No change** (N9) |
| Rig Live signature/animating (alcinelightrig.cpp:1175-1230) | Actor-only emitters become absent markers (`addExact(false)`) and are ignored by `liveProbeAnimating` |
| `updateLiveProbe` (alcinelightrigmanager.cpp:657-662) | Remove actor-only ids from the pinned list |
| On-demand `probe_gather_light` (llreflectionmapmanager.cpp:1288-1316) | Actor-only lights get `s.mEligible = false`. Verify in `ALProbeSched` that an ineligible light's motion emits no dirty event |
| Bounce-keep (A10), alcinelightrig.cpp:2537 | When the rig has an actor-only omni, scale per omni: actor-only omnis are unscaled, world omnis are scaled, and bounce stays enabled while an actor-only omni needs it. With none, line 2537 runs verbatim |

### 5.9 Other systems (behaviour stated)

| System | Behaviour |
|---|---|
| Rig Rim | Follows the mask (deferred) and the selected set (forward) |
| ALR | The Tame carrier is read by the unchanged light bodies |
| Tron avatar tag | Untouched |
| SSR | Shows the lit actor, never the light on the set (B5) |
| Shafts, froxel lights, depth haze | Still show the beam (B4) |
| Entity clones cast in a slot | Ordinary recipients |
| Ghost proxies, overlay ghosts | Never the actor (B7) |
| Impostor bakes | `EXCLUDE`, world set only |

---

## 6. Settings, persistence, UI

### 6.1 Persistence (rule 6: count every site)

Every non-UI, non-test site that handles `mFixturePreset` gets a matching `mActorOnly` line: **16
sites.** Verify by grepping both names and comparing the counts.

- **WP0 (5 runtime sites):**
  - alcinelightrigmodel.cpp: `cleanLight` 272, `copyCleanLights` 432, `computeLive` 1080, `blendLight`
    1145 (midpoint);
  - alcinelightrig.cpp: FX copy 2756.
- **WP5 (11 IO sites):**
  - alcinelightrig.cpp: `ACTOR_ONLY_SETTINGS[4]` beside 86-89, `lightsEqual` 270, LLSD write 489
    (`"actor_only"`), LLSD read 600 (an absent key means false), `readSettings` 1429,
    `readSettings(blob)` 1480, `writeSetupToSettings` 1532;
  - alcinelightrigmanager.h: 263, 351, 423, 554.
- **Declarations:**
  - `LightBase` after `mFixturePreset` (model.h:102).
  - `ParamBlob::Light` after `mRimBackBias` (manager.h:68). The positional rows at 145-154 are
    unaffected; count them anyway.
- **Rig-level fields:** `mBounceLightsSet` and `mCatchlightActorOnly` are top-level blob fields beside
  `mCatchlight` (manager.h:118). They get settings and LLSD IO plus the rig setter call in
  `tickSelected`/`tickFromBlob`. They are not part of cues.
- **Tests (`alcinelightrigmodel_test.cpp`):**
  - blob round-trip;
  - `blendLight` midpoint;
  - `cleanLight` copy;
  - registry recipients and bits: slot, group with a duplicate avatar, object target, the
    **legacy-SELF-anchor + Group-SELF case (two distinct bits)**, and the worn-animesh owner union.

### 6.2 Settings (`app_settings/settings.xml`; all default off/false)

| Key | Type | Default | Meaning |
|---|---|---|---|
| `CineLightRig{Key,Fill,Rim,Bg}ActorOnly` | Boolean | 0 | Per-light Actor only |
| `CineLightRigBounceLightsSet` | Boolean | 0 | 0 = the bounce follows its light |
| `CineLightRigCatchlightActorOnly` | Boolean | 0 | The catchlight lights only the recipients |
| `CineLightRigActorMaskDisable` | Boolean | 0 | Kill switch: flags are ignored and lights behave normally |
| `CineLightRigActorMaskDebug` | U32 | 0 | 0 off, 1 overlay, 2 mask only; also enables guard capture |
| `CineLightRigActorMaskBias` | F32 | -8.0 | Polygon offset units (GUESS; bisect from -8) |
| `CineLightRigActorMaskTest` | U32 | 0 | 1 arm, 2 cancel (§9.3) |

Add the six rig keys to `ALPanelCineLightRig::settings()` (alpanelcinelightrig.cpp:471+). Do not add them
to llpresetsmanager.cpp.

### 6.3 UI (WP6)

- **Rig panel** (also embedded in the Director):
  - an **Actor only** checkbox on each light row, next to Shaft/Hero;
  - **Bounce lights the set** and **Catchlight: Actor only** in the rig section;
  - wiring follows `mShaftControls` (alpanelcinelightrig.cpp:827, 2311).
- **Object-target rig:** the controls are disabled with the tooltip "Actor only needs an avatar subject.
  With an object target these lights light normally."
- **Per-light tooltip:** "Lights only this rig's subject and everything they wear. Environment lights
  still light them. Cannot cast a visible shadow onto the set. Shafts still show the beam. Reflections
  of the set never show it."
- **Lightbox (Cine section):** kill switch, Debug combo, Bias slider, "Run isolation test".

---

## 7. Off path and ON path correctness (CLAUDE.md rule 1)

### Off (every flag false, or the kill switch on)

When `anyActorOnly()` is false, nothing new runs:
- No mask is allocated, no mask pass runs, and no new program is bound. The mask programs sit on
  `mShaderList` (Fable A2), so they are dirtied each frame. Uniforms are uploaded only at bind, and they
  are never bound, so nothing reaches the GPU.
- The eligibility recorder is never armed. Its hook sits on the predicates' true branch behind one bool.
- The world loops' new `continue` is one cached-bool branch.
- No actor scans run.
- The shadow tiers and null rules reproduce today exactly (§5.6).
- The bounce scale line runs verbatim.

Existing code is unchanged:
- Existing `.glsl` files and program objects are byte-identical (§5.5 wrappers; no fallback).
- `walkGhostSourceGeometry` defaults reproduce today's behaviour.
- The ghost guard keeps its default mode.
- The appended reserved uniforms change no existing index.
- Depth stays `DEPTH_FMT_24`.

### On

**Definition.** Every pixel that is not covered by a recipient must equal the same scene with the
actor-only lights off. B4 and B5 are excluded from this definition.

It holds only with all of the following:
- side-effect-free world-loop skips (§5.5, §5.7);
- independent actor scans;
- shadow tiers with null handling (§5.6);
- pinned-list filtering (§5.8);
- `discard` on a miss;
- a mask that never writes or clears depth;
- a mask restricted to natively drawn geometry (§5.4.1).

**What reviewers check.** Against both the beauty pass and the off path, after the mask pass and after
the actor-only light block:
- colour mask;
- **clear colour** (Fable A1);
- blend enable and blend function;
- draw buffers;
- depth function and depth mask;
- polygon offset;
- bound FBO;
- viewport;
- program.

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
   shows the actor lit, but updates at the hero-probe rate, so it can trail by a few faces. At reflection
   probe level 0, mirrors skip all local lights, as stock does for every local light. The mirror then
   shows the actor without the Actor-only light while the main view shows it lit.
6. **About 1-pixel speckle at the silhouette.** The mask is a second drawing of the actor, so edge
   pixels and contact points (a hand on a shoulder) can be off by about one pixel. The debug overlay
   shows exactly where.
7. **Impostored or muted subjects get nothing.** If the subject is drawn as an impostor, muted,
   jellydolled or too complex, the Actor-only light lights nothing on them (fail closed); the log
   says `IMPOSTOR`. The same applies to a Shared Actor FX Replace/Cover actor (`STYLED`). Ghost proxies
   and overlay ghosts are never "the actor".

---

## 9. Instrumentation: the log states a verdict (WP7, Opus)

### 9.1 Summary, queries and verdicts (Codex R1 #7)

**Summary line** (`LL_INFOS("ActorMask")`), once per 5 s while `anyActorOnly()`:
`views=<n> recipients=<n> mask_draws=<n> actor_only_lights=<n> alpha_switches=<n> alpha_slot_overflow=<n>`

**Queries.** Main view only, per recipient bracket:
- **Tight** query: every frame, during the real mask draws.
- **Loose** and **raster** probe queries: issued only on the 5 s summary frame or when Debug is non-zero.
  - Loose: the same draws, colour writes **off**, depth `GL_LEQUAL`, offset `16 x Bias`.
  - Raster: the same draws, colour off, depth test off, counting fragments inside the viewport.
- **Tags.** Each query is tagged
  `{frame, view id, recipient avatar id, kind, draws_issued, in_frustum}`, and the tag is captured at
  issue time.
- **Reading.** Results are read only when `GL_QUERY_RESULT_AVAILABLE` is set, from a 3-frame ring.
  Verdicts are computed from the tag's captured state, **never** from current-frame state. A result
  whose tag no longer matches a live recipient is discarded.

**Verdicts:**

| Verdict | Condition |
|---|---|
| `ACTORMASK OK <r>` | tight > 0 |
| `ACTORMASK VISIBILITY INCONCLUSIVE <r>` | tight == 0 and raster > 0, whatever the loose result. The subject may be occluded, or the mask may be broken; this is undecidable from queries. (Codex R2 #7: a loose-tolerance hit does not prove visibility, because a fully occluded surface between the tight and loose tolerances also passes. So no query-only state may claim EMPTY.) The loose count is printed as a diagnostic (`loose=<n>`). A run of 10 or more consecutive INCONCLUSIVE summaries for an on-screen recipient while Debug is 0 adds the hint "check Debug=1 overlay". |
| *(no EMPTY verdict)* | The EMPTY verdict is removed. Occlusion-query state cannot prove fragment visibility, and a group passing the occlusion test only proves its bounding box survived. The authoritative "mask silently broken" check is the Debug=1 overlay (in-world test 1) plus NO-GEOMETRY (harvest failure) above. |
| `ACTORMASK NO-GEOMETRY <r>` | in frustum but `draws_issued == 0` (harvest failure) |
| `ACTORMASK OFFSCREEN <r>` | not in frustum, or raster == 0 |
| `ACTORMASK IMPOSTOR <r>` / `STYLED <r>` | Fail closed by design (B7; §5.4.1) |
| `ACTORMASK OBJECT-TARGET <slot>` / `RECIPIENT-OVERFLOW` | §5.1 |
| `ACTORMASK STALE` / `ALLOC-FAIL` / `NO-PROGRAM` | The view fell back to `EXCLUDE` |
| `ACTORMASK NO-CLIP` | Mirror `MASK` view, or Prism view with an active lens clip, where a mask program has no `MIRROR_FLAG` location; the view fell back to `EXCLUDE` (Fable A2) |
| `ACTORMASK VIEW-UNCLASSIFIED <view>` | Fail-closed path hit |
| `ACTORMASK STATE-LEAK <what>` | Extended guard tripped |

### 9.2 Guard and debug view

**Guard (A12).** An extended mode of `LLScopedGhostRenderInvariant` snapshots:
- `GL_POLYGON_OFFSET_FILL` with its factor and units;
- `GL_SCISSOR_TEST` with its box;
- `GL_COLOR_LOGIC_OP`.

Its capture is enabled by `CineLightRigActorMaskDebug != 0`. It wraps the mask pass, the loose and
raster probes, and the actor-only light block. Ghost call sites keep the default mode.

**Debug view.** Mode 1 tints the mask by bit. Mode 2 shows the raw mask. Both are a new program at the
end of the main view's `renderDeferredLighting`.

### 9.3 Isolation / parity instrument (C5)

**Arming and preconditions.** Armed by `CineLightRigActorMaskTest = 1`. Main view, fixed camera, frozen
subject. Each mode is held for at least 1.5 s / 90 frames before it is captured.

**Modes:**
- F0: actor-only lights excluded everywhere.
- F1: masked deferred, forward world set only.
- F2: normal.
- F3: actor-only lights treated as world lights.
- F0': F0 again.

**Captures** (`mRT->screen`, HDR, before post):
- C_deferred: right after the local-light block.
- C_post: after `renderDeferredLighting`.
- At most two full captures plus the mask are held at once.

**Stable pixels** are those bit-identical between F0 and F0'. Below 50% stable, the result is
INCONCLUSIVE.

**Checks:**
- **ISOLATION-DEFERRED:** F1 vs F0 at C_deferred. Stable pixels with mask == 0 must be bit-identical.
  Zero changed mask pixels gives INCONCLUSIVE.
  - The instrument also prints a mask-byte histogram (Fable A1).
  - A histogram with no zero bin means the mask was not cleared to 0, so the test reports FAIL and
    names "mask clear".
- **ISOLATION-FORWARD:** F2 vs F1 at C_post. Stable pixels outside the recipients' bounds, dilated by
  8 px, must be identical. Precondition: shafts, froxel lights and haze are off (otherwise
  INCONCLUSIVE, B4).
- **PARITY:** F1 vs F3 at C_deferred, on mask pixels eroded by 2 px:
  `|a-b| <= 1e-3 + 0.01*max(a,b)`. INCONCLUSIVE if the shadow-slot assignment differs between F1 and
  F3, or the rig is animating.
  - **Path record (Fable C4):** F3 records the path each actor-only point light took, volume or
    fullscreen (pipeline.cpp:23464-23466).
  - If no point light took the volume path, PARITY is INCONCLUSIVE, reason "no volume-path point in F3".
    In that case F3 and F1 ran the same program, so the comparison proves nothing.
  - Spots need no record: both spot programs compile `spotLightF.glsl`.

**INCONCLUSIVE preconditions:**
- SSR is on;
- the camera changed;
- the mask changed across F1-F3;
- no active actor-only light;
- the recipient is off screen.

**Output:** one line per test, `ACTORMASK TEST <name> PASS|FAIL|INCONCLUSIVE <reason>`.

---

## 10. Work packages (Codex R1 #8: strictly sequential)

**Process:**
- Codex is invoked only with `--prompt-file` (a file inside the repo), launched through
  `run_in_background`, and watched by the log-stall watchdog. Codex never builds.
- Each WP gets an OFF-LIMITS list: every file another WP owns.
- Each WP gets an adversarial review from Opus and Codex, looped until there are 0 must-fix. A finding
  that breaks the feature's core promise is a blocker, whatever its label.
- After all WPs: one cross-package Opus + Codex review covering rule 1 and rule 6.
- Then a checkpoint commit and an exe backup, then **one** build by Claude, then the in-world test.

**No two WPs run in parallel.** Several WPs share files: alcinelightrig.cpp, alcinelightrigmanager.h/.cpp,
alcinelightrigmodel_test.cpp, alpanelcinelightrig.cpp, pipeline.cpp, llviewershadermgr.cpp and
llactormover.cpp. The order below is the only order. Each WP starts only after the previous WP's review
has converged.

| Step | WP | Owner | Content | Files |
|---|---|---|---|---|
| 1 | **0** | Codex | Registry, recipient bits, `drawByte`, `LightBase.mActorOnly` + 5 runtime sites, rig-level members, `catchlightId()`, keep-live clause, pure-model registry + tests | alcinelightrigmodel.h/.cpp, alcinelightrig.h/.cpp, alcinelightrigmanager.h/.cpp, llvoavatar.cpp, alcinelightrigmodel_test.cpp |
| 2 | **5** | Sonnet | §6.1 IO sites, rig-level IO, settings.xml keys, reset list, tests | alcinelightrig.cpp, alcinelightrigmanager.h, settings.xml, alpanelcinelightrig.cpp (`settings()` only), alcinelightrigmodel_test.cpp |
| 3 | **1** | Codex | Classification, mask lifecycle (including clear colour), eligibility recorder, mask pass and programs (each on `mShaderList`; report lists every `gl_Position` expression beside its G-buffer counterpart, and the `MIRROR_FLAG` location check), reserved uniforms, walk parameters, proxy-avatar accessor, query plumbing | pipeline.h/.cpp, lldrawpool.cpp (recorder hooks), new llactormaskeligibility.h/.cpp, llviewershadermgr.h/.cpp, llshadermgr.h/.cpp, new mask shaders, llactormover.h/.cpp |
| 4 | **2** | Codex | Light skip + actor scan, 2 programs + 2 wrappers (stock feature flags, `add_common_permutations`, texture-channel log), fullscreen draws placed after both world spot blocks, shadow tiers, Prism order | pipeline.cpp, llviewershadermgr.cpp, new `actorMaskGateV/F.glsl` |
| 5 | **3** | Codex | World/actor sets, `restoreLightUnitContent`, `selectForwardLightSet`, alpha-pool selection, system avatar alpha selection (Fable A3) | pipeline.h/.cpp, llrender.h/.cpp (new method only), lldrawpoolalpha.cpp, **lldrawpoolavatar.cpp** |
| 6 | **4** | Codex | §5.8 | pipeline.cpp, llreflectionmapmanager.cpp, alcinelightrig.cpp, alcinelightrigmanager.cpp |
| 7 | **6** | Sonnet | §6.3 UI, tooltips, Lightbox controls, doc note | panel_cine_light_rig.xml, floater_lightbox_settings.xml, alpanelcinelightrig.cpp/.h |
| 8 | **7** | Opus | §9 verdicts, tagged query readout, extended guard, debug view, test instrument | llghostdeferreddiagnostics.h/.cpp, new alactormaskdiagnostics.h/.cpp, pipeline.cpp, llviewershadermgr.cpp |

---

## 11. In-world test (outcomes stated in advance)

Open the log and keep `CineLightRigActorMaskDebug` handy.

1. **Mask sanity.** Rig on SELF, Key Actor-only, Debug = 1.
   - *Expect tinted:* the avatar and everything worn.
   - *Expect not tinted:* background seen through hair cards, an object in front, fullbright faces.
   - *Expect in the log:* `ACTORMASK OK`.
   - Then, starting at the default -8, step `Bias` toward 0 until holes in the Debug=1 overlay or
     speckle appear, and report the value where it is clean. If no value is clean, see the fallback in
     §5.4.3.
2. **Isolation.** Light floor and wall, Key Actor-only.
   - *Expect:* the avatar lit; no pool and no bounce pool on floor or wall.
   - With **Bounce lights the set** ticked, only the bounce pool appears.
   - With Actor only unticked, the key pool appears.
3. **Instrument.** Freeze the pose; SSR, shafts and haze off; fixed camera; then "Run isolation test".
   - *Expect:* `ISOLATION-DEFERRED PASS`, `ISOLATION-FORWARD PASS`, `PARITY PASS`.
   - INCONCLUSIVE: redo with what the reason names.
   - **Any FAIL means the feature is broken, however good it looks.** Report the raw lines.
4. **Everything worn.** Alpha hair, a held prop, worn animesh, rigged clothing. *Expect:* all lit by the
   Actor-only key; nothing else.
5. **Two subjects / group.**
   - (a) Rig A on Subject A and rig B on Subject B, both Actor-only, hug pose. *Expect:* A's key lights
     only A and B's only B, including where they touch.
   - (b) A Group rig on A+B with an Actor-only key, and Subject C standing between them. *Expect:* A and
     B lit, C not.
6. **RTGI.** *Expect (INFERENCE):* some bounce near the actor (B1). Note how much.
7. **Mirror and feed.**
   - *Expect:* lit like the main view; the mirror may trail (B5).
   - *Expect:* no light on the wall behind the actor inside the mirror.
8. **Off path.**
   - All flags off. *Expect:* no `ACTORMASK` lines; fps unchanged.
   - Kill switch on with flags on. *Expect:* normal lighting.
9. **Object target.** *Expect:* controls disabled with the tooltip, one `OBJECT-TARGET` line, normal
   lighting.
10. **Probes.** Shiny floor, Live Probe on, Actor-only key with an animated FX.
    - *Expect:* the reflection never shows the key's pool.
    - *Expect:* the Live refresh rate does not climb.
11. **Fail closed.**
    - Jellydoll or impostor subject. *Expect:* nothing lit; `IMPOSTOR`.
    - Shared Actor FX Cover/Replace subject. *Expect:* `STYLED`, and no light on the wall behind.
12. **Cue and shadows.**
    - A cue from off to on switches at the midpoint.
    - Two shadowed world projectors keep their shadows when the rig key is toggled to Actor-only.

---

## 12. Least-sure list (for the next reviewer)

1. **Polygon offset.** The -8 default is a GUESS. Look for rigged-redraw z-fighting and for a partner's
   hand passing within the epsilon. Depth invariance across programs is INFERENCE (Fable A4): record the
   clean bias for each geometry class (static, rigged, body, eyes).
2. **Eligibility recorder completeness.** Is every native G-buffer suppression path routed through the
   three predicates? Is every G-buffer submission for a `MASK` view inside the armed scope? The ghost
   deferred submit sits between the main G-buffer and lighting; does it call the predicates while
   armed?
3. **Wrappers.**
   - Does every listed file honour the `[EXTRA_CODE_HERE]` marker, so the define lands before
     `#undef`?
   - Does any vertex-stage `out` or `in` interface depend on `main` being in that file?
   - Program-binary cache keys (llglslshader.cpp:443, 457) differ for the new programs: confirm.
4. **Forward set switching.**
   - The hash bump between draws that share a shader.
   - `enableLights` flips at lldrawpoolalpha.cpp:1354/1448.
   - The proxy's own binds.
5. **Owner keys.** Does `getActorFxOwnerId()` equal the `mActorFxOwner`/`mActorFxFallbackOwner` stamped
   for self, entity clones, worn animesh and cast animesh?
6. **Pass include-list.** Look for holes (a G-buffer pass missing from the list) and leaks (a
   non-G-buffer pass present).
7. **Loose-probe validity.** Is `16 x Bias` loose enough to prove visibility yet tight enough not to
   pass through a contacting partner?
8. **Instrument confounds.** Beyond SSR, volumetrics and animation, what else differs on stable pixels?
9. **On-demand scheduler.** Does an ineligible light emit no further dirty events?
