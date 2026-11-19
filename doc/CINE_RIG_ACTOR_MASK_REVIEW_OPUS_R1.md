# Cine Rig "Actor only" / Actor Mask: adversarial design review + re-anchor (Opus R1)

**Date:** 2026-10-01
**Reviewed:** `doc/CINE_RIG_ACTOR_MASK_BRIEF.md` (written against `origin/fix/animesh-clone-pose-polish` f9ad494)
**Re-anchored against:** local HEAD `def6630db74`. The working tree also has uncommitted probe edits in
`llreflectionmapmanager.cpp` and `alcineliveproberefresh.h`, which another agent owns.
**Mode:** read-only. No code edits, no build. Labels: **PROVES** = read in code at HEAD; **INFERENCE** =
reasoning that has not been checked in code or in-world.

## Verdict: GO-after-fixes

The approach family is right: a per-view screen mask drawn after the G-buffer, depth-tested against
shared depth, and sampled by separate actor-only light programs. Inside the stated constraints (no
change to the G-buffer layout or the depth format), nothing better is available. The brief is not
implementable as written, though. Since f9ad494 the rig gained **Group** and **Object-target** modes,
so "one rig, one subject" is false. The forward-gating design fails the brief's own test 3. Probe
exclusion leans on a predicate that is gated to Live captures and to the target rig only. The
proposed R8 additive "OR" corrupts subject bits. Fix the A-items below in the brief, have the brief
re-reviewed, and then implement.

---

## 1. Least-sure items (read these first)

1. **Depth agreement of the redraw (INFERENCE).** The G-buffer rigged and legacy/PBR vertex shaders compute
   `gl_Position` with different expression orders. A single mask vertex shader will differ from them by
   about 1 ULP, so a zero offset will z-fight on roughly half the pixels. Some offset is mandatory. Its
   size can only be fixed empirically. The ACTORMASK EMPTY verdict and the debug overlay are the instrument
   for that; do not trust any guessed value.
2. **"One front surface per pixel" (my basis for dropping OR, §4).** This holds except at exact depth ties. A tie
   between two layers of the same avatar is harmless because both write the same byte. A tie between two
   avatars also z-fights in the beauty pass, so either owner is a legitimate answer. I am fairly sure of
   this, but it should be attacked.
3. **Fullscreen-only routing for actor-only lights (A11 recommendation).** This assumes `pointLightF` and
   `multiPointLightF` shade identically. Stock already swaps between them as the camera crosses a light's
   box, so they are meant to be interchangeable. Rig Rim uses `RIG_RIM_LIGHT` in one and `RIG_RIM_LIGHTS[]`
   in the other, so parity must be checked. This is INFERENCE.
4. **"Light-off baseline equals actor-only excluded" (test 3).** This assumes fade clocks (setupHWLights writes `fade`
   in place, pipeline.cpp:10636-10652) and the persistent `updateSpotLightPriority()` state (23477, 23504)
   do not differ between the two cases. In both cases the light stays in `mNearbyLights`, so the fade
   advances; with the light off, it leaves. This could produce a one-off divergence for about
   `LIGHT_FADE_TIME` after a toggle. INFERENCE. Take the screenshots more than 1 s after toggling.
5. **Fullbright opaque attachment faces** are drawn after deferred lighting. The mask marks the background
   pixel underneath them, which is then overwritten. I believe this is invisible, apart from screen-space
   GI or SSR history reads. INFERENCE.

---

## 2. Re-anchor table (brief §3 and design refs → HEAD def6630db74)

| Brief claim | Brief ref | HEAD ref | Status |
|---|---|---|---|
| Rig emitters created via `createObjectViewer` | alcinelightrig.cpp:1412 | **alcinelightrig.cpp:1545** (`createEmitter`); Live Probe object: alcinelightrigmanager.cpp:538 | moved |
| Id accessors | :1064, :1082-1104 | `projectorId` **:1068**, `omniId` **:1079**, `liveProbeIgnoredLightIds` **:1110-1126** (omnis + catchlight), `liveProbeProjectorIds` **:1128-1140**. The catchlight has **no** id accessor (`mCatchlight` is private, alcinelightrig.h:393) | moved; one accessor missing |
| Slots SELF, A-D | alcinelightrig.h:98-106 | **alcinelightrig.h:100-108**. **NEW:** `GROUP_SLOT_SELF..D` bitmask **:173-181**; `setObjectTarget` / `setGroupEnabled` / `setGroupSlots` **:245-256**; resolution with object and group branches **alcinelightrig.cpp:2362-2438** | **semantics changed (A1)** |
| `resolve` / `resolveSubjectA..D` | lldirectorcast.cpp:1659, 1711-1726 | **:1665**, **:1717-1736**; rig wrapper `ALCineLightRig::resolveSlotAvatar` alcinelightrig.cpp:1051-1066 | moved |
| Main frame order | llviewerdisplay.cpp:1074-1123 | `buildGhostDeferredQueue` 1077, `renderGeomDeferred` **1080**, ghost submit + contamination tick **1082-1094**, flush **1115-1116**, `renderDeferredLighting` **1120** | moved |
| Ghost invariant guard | llviewerdisplay.cpp:1084-1088 | **1083-1087**; class at llghostdeferreddiagnostics.h:25-44. It does not snapshot polygon offset, scissor or logic-op, and it captures only while `GhostDeferredDebugLog` is on (header :9-16) | moved; **weaker than the brief assumes (A12)** |
| Ghost harvest | llactormover.cpp:13683 | `collectGhostBatches` **13959-14328**; the side-effect-free walk `walkGhostSourceGeometry` **13830-13957**; pass list `kRiggedPasses` **13794-13822**; system-body replay `canRenderSystemActorGhost` / `renderSystemActorGhost` **14861 / 14901-15240** | moved; **reuse target changes (A7)** |
| Sweep machinery | ~12444-12807 | `updateGhostImpostors` 13704, `renderStudioGhosts` 14618 | moved |
| Four deferred local-light paths | pipeline.cpp:20119-20400 | `renderDeferredLighting` **23016**; light block **23342-23679**: point volumes 23369-23528, spot volumes 23530-23570, multi-point batches 23572-23634, multi-spot 23636-23671 | moved |
| Light shader registration (clear → add) | llviewershadermgr.cpp:2583-2630 | point **2639-2654** (clear 2650); multi `LIGHT_COUNT` loop **2660-2680** (clear 2668, add 2673); spot **2684-2698** (clear 2691); multi-spot **2704-2711+** (clear 2710, add 2711); unload list **1620-1625**. Order is correct today | moved |
| deferredScreen without stencil | allocateScreenBufferInternal | **pipeline.cpp:1280** (`allocate(..., GL_SRGB8_ALPHA8, true)`, so default `DEPTH_FMT_24`); shared at 1323, 1392, 1442; Prism scratch **1660-1670**; llrendertarget.h:92 (stencil=false). **NEW:** `DEPTH_FMT_32F` enum exists (llrendertarget.h:75-76) but deferredScreen does not use it | holds |
| `shareDepthBuffer` | llrendertarget.cpp:~444-482 | **llrendertarget.cpp:421-455** (LL_ERRS if the target already has depth, :430-438); detach in `release()` **:473-487** | moved |
| ReShade depth hard-coded 0x81A6 | llreshadebridge.cpp:208-215 | **:209-214** | holds |
| 8 light units, light hash | llrender.h:64, :588; llrender.cpp:611-703 | llrender.h **:64**, `mLightHash` **:580**, `mRigRimActive` :582; setters llrender.cpp **611-739**; `syncLightState` **903-964**, including the Rig Rim array 923/962 | moved |
| Forward light assignment | pipeline.cpp:9905 | `setupHWLights` **10466**; units 2-7 loop **10556-10725**; Live ignore/pinned **10585-10595**; rig rim per unit **10680** | moved |
| Alpha pool light toggles | lldrawpoolalpha.cpp:861-1127, 1325-1419 | `renderAlpha` **829+**; Shared Actor FX depth prepass **866**; proxy turn **969-999** (`renderSharedActorStyleProxy` 982); `light_enabled` toggles **1125-1141**, **1354-1398**, `enableLightsDynamic` 314 / 1354 / 1448 | moved; **new forward-lit actor stream (A3)** |
| Live Probe list and pinning | pipeline.cpp:9199-9290, 9905-9965 | Probe-centric/Live branch of `calcNearbyLights` **9844-9941** (also used by RenderProbeOnDemand); Prism branch **9943-10024**; main **10026+**; refresh signature `appendCinematicProbeLightSignature` **9660-9826**; per-face rebuild llviewerdisplay.cpp **1305-1309**; predicates llreflectionmapmanager.cpp **2607-2623** (Live-capture-gated) | moved; **predicate scope too narrow (A4)** |
| `setCinematicLiveProbe` | (§4.5) | llreflectionmapmanager.cpp **2582-2605**; called from alcinelightrigmanager.cpp **657-662** with the **target rig only** | **A4** |

### 2.1 Every `renderDeferredLighting` caller at HEAD (PROVES: grep of `renderDeferredLighting(`)

| # | Site | View | mRT | How to classify it |
|---|---|---|---|---|
| 1 | llviewerdisplay.cpp:1120 | Main camera, including tiled hi-res snapshots | `mMainRT` | `!gCubeSnapshot && !sPrismLensRender` → **mask** |
| 2 | llprismlens.cpp:7586 | Prism / VCam feeds (one per slot, scratch pack; runs **before** the main view; no ghost-deferred submit) | scratch `RenderTargetPack` from `acquirePrismLensScratch` (pipeline.cpp:1588) | `sPrismLensRender` → **mask** (per scratch pack) |
| 3 | llviewerdisplay.cpp:1388 `display_cube_face` | **Both** reflection-probe faces (llreflectionmapmanager.cpp:2761, `mAuxillaryRT`) **and** hero mirrors (llheroprobemanager.cpp:301, `mHeroProbeRT`, `mRenderingMirror=true` :245) via `LLViewerWindow::cubeSnapshot` (llviewerwindow.cpp:6162) | aux / hero | `gCubeSnapshot && isMirrorPass()` → **mask**; `gCubeSnapshot && !isMirrorPass()` → **exclude** |

There are no other callers. `generateImpostor` (pipeline.cpp:27604) does not call it, but it does run forward
alpha, so it needs forward-path fail-closed handling (A3). Shadow passes do not call it. There is no
separate "VCam" path: VCam feeds are Prism slots.

---

## 3. Existing per-avatar mechanisms: reuse or conflict

- **Avatar Light Response (c221cda3732): per-draw ownership, no per-pixel id. Reuse it; no conflict.**
  `LLDrawInfo::mActorFxOwner` / `mActorFxFallbackOwner` (llspatialpartition.h:113-119) are set for every
  avatar-owned face in `registerFace`, including unrigged attachments and worn animesh. Owner-aware batching
  stops cross-avatar merging, so a draw's owner is unambiguous. This is exactly the §4.4 "is-actor per draw"
  signal; no new caching is needed. Also reuse `ALAvatarLightResponse::keyForAvatar` (alavatarlightresponse.h:80)
  and the **keep-live hook** at llvoavatar.cpp:4643 (A8). ALR's only per-pixel data is the PBR Tame carrier in
  normal `.b`, which avatar PBR writers set and which is 0 elsewhere. That is a value, not an identity, so it
  cannot serve as the mask. However, **the four deferred light shaders read it** (`s = 1 − .b`, per the ALR
  design §2.2), so actor-only light programs must keep that read (A11). ALR's clone rule is "clones inherit
  their source". The mask must NOT inherit: a Ghost proxy that replays Subject A's draw-infos is not Subject A
  (A3).
- **Rig Rim: does not identify subjects.** `ALCineRigRim::paramsForVolume` (alcinerigrim.cpp:294-312) is
  per light (`rigRimParamsFor(light id)`) and applies to every surface the light reaches. In deferred
  lighting the rim term is part of the same light output, so it follows the mask automatically. In forward
  lighting it is uploaded per light unit (llrender.cpp:923/962), so it follows whichever unit set is active.
  No conflict.
- **Tron A0 G-buffer avatar tag: not reusable as the mask.** It encodes odd 1/6 codes in normal `.w`
  (llshadermgr.cpp:701-726; globalF.glsl:71-78). It is 1 bit (any avatar), off by default
  (`RenderGBufferAvatarTag`, llviewershadermgr.cpp:809-815), HDR-only, and tags only body, eyes and rigged
  mesh. Unrigged attachments are untagged (diffuseF.glsl:102-110). It is useful as a cross-check: every
  masked rigged or body pixel should also be tagged.
- **Roto Ink / Tron / Outline subject selection** uses screen-space ellipses from bounds
  (`collectRotoInkCandidates`, pipeline.cpp:16207). That is the rejected volume shortcut, so there is no
  precedent there. The Actor Mask could serve those features later (out of scope).
- **System-body replay precedent**: `renderSystemActorGhost` (llactormover.cpp:14901-15240) redraws the BOM
  body and eyes through `renderSkinned()` / `renderRigid()` with `LLDrawPoolAvatar::sMinimumAlpha`. This is
  the pattern the mask needs for the body; the Ghost harvest covers attachments only.

---

## 4. §9.8 decision: is the approach right?

**Recommend:** a **shared-depth R8 UNORM** mask holding a **per-avatar byte computed on the CPU**, written with **blend
OFF, plain overwrite**, generated in **one pass at the top of `renderDeferredLighting()`** (after
`preserveReShadeGBuffer()` and the `sCull` early-out). Classify the view there; fail closed for anything
not classified.

- **Not additive "OR" (A2).** Overlapping draws of the same subject are the norm (body plus layered
  mesh clothing at near-equal depth, both passing LEQUAL+offset). Additive blending carries bit 0 into bit 1,
  so SELF becomes Subject A. Every pixel's front surface belongs to exactly one avatar, so a pixel needs that
  avatar's byte, not a running OR. Precompute `byte(av) = OR of every slot or group bit resolving to av`.
  Overwrite is then idempotent for same-avatar overlaps.
- **Not R8UI or logic-op.** They are unnecessary once OR is gone. (`glLogicOp(GL_OR)` would work on UNORM8 as fixed-point,
  but it adds untracked GL state; see A12.)
- **Not one R8 per subject.** That means 5-8 targets and 5-8 passes for no gain.
- **Not stencil.** deferredScreen has no stencil. Adding one changes the depth format to D24S8 and breaks the
  ReShade `0x81A6` constraint. A stencil-on-a-copied-depth design needs a per-frame depth blit plus a second
  depth attachment swapped onto `screen` around the actor-only light draws. Its one real advantage (fixed-function
  rejection, no new light programs) is matched by `discard` in the new programs.
- **Not at the ghost-deferred submission point.** That block is the instrument scope for
  `LLScopedGhostRenderInvariant` and the G-buffer contamination test (llviewerdisplay.cpp:1082-1094). Putting
  mask draws there pollutes that instrument (CLAUDE.md rule 4). It also exists only in the main view: Prism and
  `display_cube_face` have no ghost submit. The top of `renderDeferredLighting` is the single point that every
  view passes through exactly once, with the view's matrices current and its G-buffer depth final, including
  ghost proxy depth on the main view. `preserveReShadeGBuffer()` is the precedent for self-gated work at that
  point (pipeline.h:679).

---

## 5. Findings

### (A) Fixable design bugs, with fixes

**A1. Data model is stale: Group and Object-target rigs (PROVES).** §4.7 says "one rig, one subject". At HEAD a rig can
light a group of slots (`mGroupSlots`, resolved per tick into `mLastResolvedGroupSlots`,
alcinelightrig.cpp:2409-2421) or an object linkset (`mObjectTarget`, :2362-2407).
*Fix:* give each light a `U8 light_bits` value: the slot bit, or `mLastResolvedGroupSlots` in group mode. The
shader test is any-of: `(byte & light_bits) != 0`. The bit layout already matches `GROUP_SLOT_*`. For an
object-targeted rig, decide before implementation, either way:
- (a) draw the target linkset's static faces (the same walk as `gather_root_geometry`) into a spare bit (5-7); or
- (b) disable "Actor only" in the UI for object targets, with a tooltip and an `ACTORMASK OBJECT-TARGET` log line.

The ship-whole rule requires one of these to be stated up front. My pick is (a).

**A2. R8 additive OR corrupts subject identity (PROVES this follows from the blend math; that the overlaps occur is near-certain).** See §4. *Fix:*
use the CPU per-avatar byte with blend disabled and overwrite.

**A3. Forward gating as specified fails test 3 and misses forward-lit actor and non-actor streams (PROVES).**
- (a) Zeroing actor-only units before non-actor alpha draws is not the same as the light being off. When there
  are more than 6 nearby lights, the actor-only light occupies a unit (setupHWLights, pipeline.cpp:10556-10725)
  that the light-off baseline would give to the next light X. Non-actor alpha then loses X.
- (b) Forward-lit draws outside the per-draw loop are not gated: Ghost forward passes
  (`renderGhostRiggedBlend` 6627, `renderGhostPostDeferred` 6956), the impostor bake (`generateImpostor` →
  forward alpha), and cube-face alpha.
- (c) **Shared Actor FX proxies** (styled actors' Cover/Layer, lldrawpoolalpha.cpp:866/982;
  `sharedActorFxPbrF` declares `light_position[8]` even for OPAQUE/MASK, alcinerigrim.cpp:37-46) are
  forward-lit **actor** surfaces that the brief does not list.

*Fix:* **invert the default.**
- `setupHWLights` builds the **world set** with actor-only lights excluded, so that slot fill is identical
  to the light-off baseline.
- An **actor set** is built per subject byte: actor-only lights for those bits pinned first, then the
  nearest world lights.
- Switch to the actor set only around identified actor draws, and restore it afterwards. Actor draws are:
  native alpha draws whose `mAvatar`, `mActorFxOwner` or `mActorFxFallbackOwner` resolves to a subject
  avatar, and the Shared Actor FX proxy of a subject.
- Ghost proxy and overlay replays are never actor draws: classify by render context, not by draw-info owner.
- Never use the actor set during `sImpostorRender`, HUDs, or non-mirror cube faces.
- Switch only on change: the interleaved walk already drains each avatar contiguously (lldrawpoolalpha.cpp:956-965).

**A4. Probe exclusion through the Live ignored list is too narrow (PROVES).** `isCinematicLiveProbeIgnoredLight` returns
false unless a Live capture is running (llreflectionmapmanager.cpp:2607-2614), and the list holds only the
**target rig's** omnis and catchlight (alcinelightrigmanager.cpp:657-662; alcinelightrig.cpp:1110-1126).
Ordinary probe captures with `probe_level > 0` reuse the **main-eye** `mNearbyLights` (pipeline.cpp:9846, 23342-23343).
On-demand probe-centric captures take the 9844 branch, where the Live predicates are empty.

*Fix:* add a global predicate, e.g. `ALCineLightRigManager::actorOnlyBits(light id)` behind a cached `anyActorOnly()`.
Apply it, in cube faces that are not mirror passes, at:
- `renderDeferredLighting`'s light loop
- `setupHWLights`
- the 9844 branch
- `appendCinematicProbeLightSignature` (9660-9826)
- `ALCineLightRig::appendLiveProbeSignature` / `liveProbeAnimating`

The last two are perf only: they stop actor-only animation from forcing probe refreshes.

**A5. A blanket `gCubeSnapshot` gate would wrongly exclude mirrors (PROVES).** Hero mirrors are cube faces through the same
`display_cube_face`. *Fix:* classify with `gPipeline.mHeroProbeManager.isMirrorPass()`, which has precedent at
alenvintensity.cpp:219 and lldrawpool.cpp:879/904. The mask target lives in `mHeroProbeRT`.

**A6. Shared-depth lifecycle (PROVES the mechanics).** deferredScreen's depth texture is recreated on every
`allocateScreenBufferInternal`: resize, `RenderResolutionDivisor`, hi-res snapshot, HDR toggle, and Prism
scratch (1660-1670). `shareDepthBuffer` LL_ERRS if the target already holds depth. A lazily allocated mask
that keeps an old attachment would depth-test against a stale texture.

*Fix:* (re)allocate and re-share the mask inside `allocateScreenBufferInternal` / `acquirePrismLensScratch`
while it is wanted, and release it in `releaseScreenBuffers` before the depth texture is deleted. Each frame,
check that the mask's size and depth id match deferredScreen. On a mismatch, log `ACTORMASK STALE` and fail
closed (exclude the actor-only lights).

**A7. Wrong harvest entry point and missing body (PROVES).** `collectGhostBatches` runs only for path ghosts, Studio or
Actor Styles (13968-14011). It also has side effects: Clone Fidelity capture and `buildSharedActorStyleQueue`.
It covers attachments only; BOM body and eyes are not in it.

*Fix:* call `walkGhostSourceGeometry` (13830) directly, with an explicit INCLUDE temp-attachment policy. It
currently reads `GhostUnifiedExcludeTemporaryAttachments` (13843); a user who set that for ghosts would
otherwise punch holes in the mask. Draw the body and eyes through the `renderSystemActorGhost` pattern with
`sMinimumAlpha`. Skip these: blend passes (`PASS_ALPHA_RIGGED`, `*_BLEND_RIGGED`, `MATERIAL_ALPHA_RIGGED`),
`PASS_GLTF_GLOW_RIGGED`, and static faces with `mAlphaKind == 2`. Apply each draw-info's own alpha-mask
cutoff. The draw maps are cull-independent (13122-13138), so this also works for Prism and mirror views.

**A8. Impostored or jellydolled subjects (PROVES the harvest is cull/impostor-independent; INFERENCE about the visible result).** The
harvest yields full geometry while the G-buffer holds the impostor billboard, so the subject would be half
masked.

*Fix:* extend the keep-live predicate at llvoavatar.cpp:4643 to cover "subject of an enabled actor-only rig".
If the subject is still impostored, muted or jellydolled, log `ACTORMASK IMPOSTOR <slot>` and exclude that bit.

**A9. Spot-shadow slot competition breaks test 3 (PROVES the mechanism).** Actor-only projectors enter the
`setupSpotLight` priority reshuffle (pipeline.cpp:24363-24413) and can take a slot (default 2 slots) away from
a world projector, so the world projector's shadow disappears.

*Fix:* rank actor-only projectors strictly after all world projectors, so they take only slots that would be
empty in the light-off baseline. Document that an actor-only projector may lose self-shadowing when the slots
are full.

**A10. Live Probe bounce-keep removes the actor-only key's bounce with nothing in its place (PROVES).** `globals.mBounceRatio
*= mLiveProbeBounceScale` (alcinelightrig.cpp:2537) assumes the probe re-supplies the bounce. With the key
excluded from the probe (A4), the bounce simply vanishes. *Fix:* when the bounce follows an actor-only key,
leave it unscaled (scale 1).

**A11. Light programs (PROVES the hazards).**
- The brief's option of adding an "ACTOR_MASK permutation to each existing program" edits existing programs.
  Their source and hash change, so the off path is no longer inert.
- Separately written actor-only shaders would lose ALR Tame (`.b` carrier), Rig Rim, Tron tint and EnvIntensity
  behaviour.

*Fix:*
- Create **new program objects compiled from the same .glsl files** with an extra `ACTOR_MASK` define. Add the
  define after `clearPermutations()`.
- Add them to the unload list (1620-1625).
- Guard every bind with `isComplete()` (precedent 23214-23224).
- Append the new reserved uniform and sampler at the end of the reserved list, and count it (rule 6).
- Use **`discard`** on a mask miss, not multiply-by-zero, so NaN or Inf from light math cannot leak and
  non-actor pixels stay bit-identical.
- In the light-list partition, actor-only lights must not consume `count`/`effective_local_light_count`
  (23427-23431). That keeps the list identical to the light-off baseline.
- *Recommended simplification:* route actor-only lights through the two fullscreen paths only. That means one
  multi-point program at `LIGHT_COUNT = LL_DEFERRED_MULTI_LIGHT_COUNT` (16) with the existing runtime count,
  plus one multi-spot program. Scissor both to the union of the subjects' screen rectangles. That is 2 new
  programs instead of 19. It depends on least-sure item 3 holding.

**A12. The invariant guard is weaker than the brief assumes (PROVES).** `LLScopedGhostRenderInvariant` does not snapshot
`GL_POLYGON_OFFSET_FILL` or its factor/units, scissor, or logic-op, and it captures only while
`GhostDeferredDebugLog` is on. *Fix:* extend the Snapshot, enable capture when `CineLightRigActorMaskDebug`
is set, use RAII for polygon offset and scissor, and explicitly reset `glPolygonOffset(0,0)`.

**A13. Polygon offset (INFERENCE, see least-sure 1).** Use **units only (factor 0)**. A slope-scaled factor grows at
grazing silhouettes and at contact points, which is exactly where a partner's hand lies.

### (B) Inherent limits: document them, do not block on them

| # | Limit | Why it cannot be removed inside the constraints | Document it as |
|---|---|---|---|
| B1 | ReShade RTGI or any screen-space GI bounces the brighter actor onto the set | It gathers from the final image; the bridge is frozen. INFERENCE (iMMERSE is proprietary) | Tooltip plus in-world test 6; remedy is a future `SL_ACTOR_MASK` semantic |
| B2 | Forward alpha has 6 local units. The actor's alpha parts and Shared Actor FX Cover/Layer can get at most 6 local lights; with A3 pinning, actor-only lights win and world lights may drop on hair. A group of several rigs can exceed 6 actor-only lights | Fixed light-unit count | Log `alpha_slot_overflow=<n>` and a tooltip note |
| B3 | An actor-only light casts no visible shadow on the set | By definition | Tooltip (already in §5) |
| B4 | Airborne effects (projector shafts, froxel lights, depth haze) still show the beam | They are volumes, not surfaces | Existing optional "hide shafts" |
| B5 | Probes and SSR of the environment never show the light on the set; mirrors lag up to 6/rate faces | By design / hero update cadence | Note |
| B6 | Redraw-mask precision: about 1-px silhouette speckle, and contact surfaces within the depth epsilon | The mask cannot share the G-buffer write without a layout change | Note; the debug overlay shows it |
| B7 | Overlay ghosts and Ghost proxies are never "the actor"; muted, impostored or jellydolled subjects fail closed | They are not in the G-buffer, or are replays | Log verdicts |

### (C) P2 / nits

- C1. `CLAUDE.md` line 43 retires Fable from the implement loop (Codex implements, Opus reviews). The brief gives
  WP2/WP3 to Fable on a feature-specific user directive. Confirm with the user which rule wins.
- C2. Persistence: append `mActorOnly` **after** `mRimBackBias` in `ParamBlob::Light` (alcinelightrigmanager.h:39-69).
  `mLights[]` is positional and lists 11 members (:145-154). The blob lives in **alcinelightrigmanager.h**, not
  alcinelightrig.h. Cue snapshots live in `ALCineLightRigModel::Cue`/`CueState`, so the midpoint switch needs the
  field there as well. Apply the rule-6 count.
- C3. Add a `catchlightId()` accessor.
- C4. The pixel-count verdict should use one occlusion query per subject bracket, read the following frame.
  No readback stall.
- C5. **Build test 3 as an instrument**, not as manual screenshots. Follow the `LLGhostDeferredContaminationTest`
  pattern (llghostdeferreddiagnostics.h:61-80): render with actor-only lights forced excluded and then included
  on a fixed camera, compare all non-mask pixels, and emit PASS / FAIL / INCONCLUSIVE.
- C6. Off-path cost: guard every per-light lookup behind a cached `anyActorOnly()` flag. Do not walk rig ids per light per frame.

---

## 6. §7 off-path claim at HEAD

The claim **holds only if** all of the following are true:
- A11 uses new program objects rather than permutations on existing ones.
- `collectGhostBatches` is untouched (A7).
- The light-list partition, `setupHWLights` changes and alpha switching all sit behind a cached `anyActorOnly()`.
- No mask target is allocated while nothing is actor-only (A6 handles reallocation).

deferredScreen depth stays `DEPTH_FMT_24` (pipeline.cpp:1280), so ReShade `0x81A6` stays true. The Tron
avatar tag and the ALR carrier are untouched. The "bit-identical non-actor pixels" ON-state claim is
**false as written**, because of A3(a), A9 and A11's count limit, and becomes true once those fixes are in.

## 7. §10 work-package split at HEAD

- **WP1 and WP4 overlap.** With the mask pass at the top of `renderDeferredLighting` (§4), the per-view mask passes
  and their classification belong in WP1. WP4 keeps only the probe and Live consistency work (A4, A10).
- **WP2, WP3 and WP4 all edit `pipeline.cpp`** (`renderDeferredLighting`, `setupHWLights`, `calcNearbyLights`).
  Run them sequentially with one owner per function, not in parallel.
- **Add a WP0 first:** the actor-only registry API, frozen before WP1-4 start. Contents: `anyActorOnly()`,
  `actorOnlyBits(light id)`, the per-avatar byte table, and owner-key resolution reused from ALR.
- **WP4 collides with the in-flight probe work.** `llreflectionmapmanager.cpp` and `alcineliveproberefresh.h` are
  modified in the working tree right now, and WP4 needs `calcNearbyLights` and `appendCinematicProbeLightSignature`.
  Start WP4 only after that work is committed, and re-anchor it then.
- **WP7 must take A12** (guard extension) and C5 (automated test 3).
- **WP5 must include the model `Cue` field** (C2).
