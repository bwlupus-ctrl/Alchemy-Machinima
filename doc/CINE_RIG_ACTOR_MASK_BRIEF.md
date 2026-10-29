# Cine Light Rig — "Actor only" lights via a per-pixel Actor Mask — design brief

**Date:** 2026-10-01
**Status:** DESIGN ONLY. Nothing implemented, nothing built.
**Process (user directive for this feature):** heavy engine work is delegated to **Fable and Codex**
sub-agents; **Opus / Sonnet** take minor work (UI, settings, docs). **Every work package gets an
ADVERSARIAL review (independent Opus sub-agent) and loops to 0 must-fix BEFORE anything is built.**
Claude does the single build after convergence. Codex/Fable never build. See §10.

> **BASE BRANCH.** Line numbers are from `origin/fix/animesh-clone-pose-polish` (`f9ad494`), the newest
> pushed branch containing the Cine Light Rig and Live Probe. The user has unpushed local work; diff it
> against this brief before implementation and re-anchor the references.

---

## 1. The ask

> "Give the cine light rig lights a special property to only light the avatar selected. The outside
> environment lights can impact the avatar, but the lights produced by the rig have an option to only
> impact the actor." — "It would have to impact the entire actor and all their attachments." — "Do it
> the right way."

Industry name: **light linking** (Blender/Maya) / **lighting channels / light layers** (Unreal, Unity
HDRP). The cheap bounding-volume shortcut was considered and **rejected**: it lights anything inside
the actor's volume (partner's hand, the chair, the floor at their feet, background behind hair cards).

---

## 2. Scope (stated up front, per "ship whole")

**In this delivery:**
1. Actor Mask render target + mask pass (per view that runs deferred lighting).
2. "Actor only" variants of all four deferred local-light paths, used only for flagged rig lights.
3. Forward/alpha gating so actor-only rig lights light the actor's alpha (hair, lashes, sheer cloth)
   and nothing else's.
4. Consistency with every other system that sees rig lights: Live Probe, rig bounce, catchlight,
   reflection probes, hero mirrors, VCam/Prism feeds, ghost clones.
5. Per-light "Actor only" flag in the rig, persisted through presets, cues and scene save.
6. UI in the rig panel / Lightbox / Director, settings, reset/preset registration.
7. Mask debug view + log verdicts.

**Explicitly deferred (separate brief if wanted):** publishing the Actor Mask through the ReShade
bridge as a new semantic (ABI-append, additive). Mentioned in §8 because it is the remedy for the RTGI
interaction.

**Not changing (hard constraints):** the G-buffer layout; the depth format; the ReShade bridge;
any light that is not a rig light flagged Actor-only.

---

## 3. What the code PROVES today (base-branch file:line)

| Fact | Where |
|---|---|
| Rig emitters are client-only `LLVOVolume`s made with `createObjectViewer`: projectors (spot), omnis (point), one catchlight | `alcinelightrig.cpp:1412`; id accessors `:1064`, `:1082-1104` |
| Rig targets are slots SELF, A, B, C, D | `alcinelightrig.h:98-106` |
| Subject resolution: `LLDirectorCast::resolve` / `resolveSubjectA..D` | `lldirectorcast.cpp:1659`, `:1711-1726` |
| Main frame order: `renderGeomDeferred` → ghost deferred submit → `deferredScreen.flush()` → `renderDeferredLighting()` | `llviewerdisplay.cpp:1074-1123` |
| There is already a post-G-buffer avatar submission wrapped in a **render-state invariant guard** (`LLScopedGhostRenderInvariant`) with a log verdict on leaked GL state | `llviewerdisplay.cpp:1084-1088` |
| The fork already re-draws a whole avatar — body, rigged mesh, attachments — with a custom shader (Ghost overlay harvest/draw) | `llactormover.cpp:13683` (`collectGhostBatches`), sweep machinery `~12444-12807` |
| Deferred local lights take **four** paths: point volumes (`gDeferredLightProgram`), spot volumes (`gDeferredSpotLightProgram`), full-screen point batches (`gDeferredMultiLightProgram[i]`, `LIGHT_COUNT` permutation), full-screen spots (`gDeferredMultiSpotLightProgram`) | `pipeline.cpp:20119-20400` |
| Light shaders are registered with `clearPermutations()` then `addPermutation()` | `llviewershadermgr.cpp:2583-2630` (e.g. `:2612` then `:2617`) |
| G-buffer `deferredScreen` is allocated **depth without stencil** | `pipeline.cpp` `allocateScreenBufferInternal` (`deferredScreen.allocate(..., GL_SRGB8_ALPHA8, true)`), `llrendertarget.h:92` (stencil defaults false) |
| `LLRenderTarget::shareDepthBuffer` exists | `llrendertarget.cpp:~444-482` |
| ReShade bridge publishes depth with a **hard-coded** `GL_DEPTH_COMPONENT24` (`0x81A6`); unmapped formats die silently in the add-on | `llreshadebridge.cpp:208-215`; contract §6.1 |
| Forward lighting uses 8 global light units (0 = sun/moon, 2-7 = nearby lights), synced to shaders via `gGL.mLightHash` | `llrender.h:64`, `:588`; `pipeline.cpp:9905`; `llrender.cpp:611-703` |
| Alpha pool toggles lighting state per draw already (`light_enabled`, `enableLightsDynamic`) | `lldrawpoolalpha.cpp:861-1127`, `:1325-1419` |
| Live Probe capture builds its own nearby-light list and pins rig projectors first | `pipeline.cpp:9199-9290`, `:9905-9965` |

---

## 4. Design

### 4.1 The Actor Mask target

- New target `mActorMask` per `RenderTargetPack` (main, and any pack whose view runs deferred
  lighting with actor-only lights active — see §4.6). Format **`GL_R8`**, same size as
  `deferredScreen` (respects `RenderResolutionDivisor` / multiplier).
- Bit layout: bit 0 = SELF, bits 1-4 = Subject A-D. A light targets one bit; a pixel can carry several
  (two subjects touching).
- **Shares `deferredScreen`'s depth** via `shareDepthBuffer`. The mask pass depth-tests against the
  finished scene with **depth writes OFF**. Nothing else about depth changes.
- **Allocated lazily**: only while at least one rig light is Actor-only AND enabled. Released when
  none are. Off-path = no allocation (§7).

### 4.2 The mask pass

Runs **after** `renderGeomDeferred` + ghost-deferred submission and **before**
`renderDeferredLighting`, on the same camera.

- **What is drawn** for each subject slot with an active Actor-only light: the resolved avatar's
  body; its rigged mesh; every non-HUD attachment (rigged and unrigged, including child prims of
  attachment linksets); worn animesh (its control avatar's drawables); and — when the subject is an
  **entity clone** (`LLGhostAvatar`) — the clone's geometry. Overlay clones are not avatars in the
  G-buffer and are out of scope (log once).
- **How:** reuse the Ghost harvest (`collectGhostBatches`-style enumeration) so skinning palettes,
  LOD and draw lists are the ones used this frame. A dedicated `gActorMaskProgram` (+ rigged
  variant) writes `bit` with blending `GL_ONE, GL_ONE` (bit-OR semantics via `max`/additive on
  distinct bits — reviewer to pick the exact blend that guarantees OR for R8; see §9.3).
- **Depth test:** `GL_LEQUAL` against shared depth with a small polygon offset toward the camera.
  Same vertex math as the G-buffer pass so the actor's own pixels pass and occluders block them.
- **Alpha-mask parts:** the mask shader samples base-colour alpha and applies the **same cutoff** the
  G-buffer used for that face (alpha-mask mode + cutoff). Without this, transparent texels of hair
  cards would mark the background behind them and the rig light would leak onto the wall through the
  hair.
- **Alpha-BLEND parts are not drawn into the mask** — they aren't in the G-buffer; they are handled by
  §4.4.
- Wrapped in an `LLScopedGhostRenderInvariant`-style guard: any leaked GL state (colour mask, blend,
  depth func/mask, bound FBO, polygon offset) is a logged verdict, not a silent bug.

### 4.3 Deferred local lights: actor-only variants

In `renderDeferredLighting` (`pipeline.cpp:20119+`), when building the four light lists, a light
whose drawable is a rig emitter with **Actor-only** set is routed into a parallel list for its path.
Every other light keeps today's list, today's shader, today's uniforms.

- Four new programs (or one `ACTOR_MASK` permutation of each, registered **after
  `clearPermutations()`** — the exact hazard recorded in `CLAUDE.md`): point volume, spot volume,
  multi-point (all `LIGHT_COUNT` variants), multi-spot. Each samples `mActorMask` and multiplies its
  contribution by `(mask & light_bit) != 0`.
- Uniform `actor_mask_bit` per light (point/spot volume: per draw; multi-point: per batch — batch
  actor-only lights by target bit so one batch has one bit).
- Shadows: unchanged. An actor-only projector still uses its shadow map, so the actor self-shadows
  (arm across face). **By definition it cannot throw a visible shadow onto the set** — the set isn't
  lit by it. UI tooltip must say so.

### 4.4 Forward / alpha gating

Alpha-blended actor parts are lit per draw from the 8 global light units.

- In the alpha pool's draw loop, for each draw determine **is-actor(bit)**: rigged draws via the
  draw's avatar; unrigged attachment draws via the drawable's root avatar; animesh via its attachment
  owner. Cache per draw-info, not per frame lookup chains.
- Before a **non-actor** draw: actor-only rig lights present in units 2-7 are zeroed. Before an
  **actor** draw: restored. Only switch on change (the existing `gGL.mLightHash` sync then re-uploads
  once per switch, not per draw).
- **Pre-existing limitation to surface, not fix here:** forward alpha only sees the 6 globally
  nearest local lights. If a rig light isn't among them, the actor's hair doesn't get it today either.
  Reviewer: confirm whether rig emitters are guaranteed a forward slot for the main view; if not,
  propose pinning actor-only rig lights into units 2-7 while active (as the Live Probe pins projectors
  for its own capture).
- Also covers: interleaved alpha path, rigged alpha, alpha-mask-in-forward (fullbright alpha mask),
  glow pass (glow should not change — it's emissive).

### 4.5 Rig semantics that must agree

| System | Behaviour with an Actor-only light |
|---|---|
| **Live Probe capture** | Actor-only lights are **excluded** (they light no environment, so the environment reflection must not show them). Add to the ignored-light list (`setCinematicLiveProbe`). |
| **Ordinary reflection probes / sky probe** | Excluded (cube snapshots don't run the mask; a light that lights only the actor must not light probe captures). |
| **Rig bounce** | Follows its key light: if the key is Actor-only, bounce is Actor-only by default; per-rig override checkbox. |
| **Catchlight** | Actor-only by default (it's for the eyes); override available. |
| **Projector volumetric shafts** | Unchanged — they're airborne, not surface light. Optional "hide shafts for Actor-only" checkbox. |
| **SSR** | Sees the lit actor in the beauty image — correct (reflections of the actor show the actor lit). |
| **ReShade RTGI** | Will gather the brighter actor from the backbuffer and may bounce some onto the set. INFERENCE (iMMERSE is proprietary). See §8. |

### 4.6 Other views

The mask is per view. Every caller of `renderDeferredLighting` must be classified:

| View | Mask? | Actor-only lights |
|---|---|---|
| Main camera | Yes | Masked |
| Hero probe / mirrors | Yes (own mask pass for the mirror camera) | Masked — a mirror must show the actor lit the same way |
| VCam / Prism camera feeds | Yes | Masked |
| Reflection-probe cube snapshots (`gCubeSnapshot`) | No | Excluded |
| Shadow passes | n/a | n/a |
| Impostor renders | No | Excluded (impostors are distant; log once) |

Reviewer/implementer must enumerate **all** `renderDeferredLighting` callers on the base branch; any
view not in this table fails closed (Actor-only lights excluded) and logs once.

### 4.7 Data model and persistence

- Per light: `mActorOnly` (bool). Target = the rig's slot (SELF/A-D) — one rig, one subject, so no
  per-light target is needed in v1.
- Per rig: `mBounceFollowsKey` (bool, default true), `mCatchlightActorOnly` (bool, default true),
  `mHideShaftsWhenActorOnly` (bool, default false).
- Persist through `ALCineLightRigParamBlob` (settings, LLSD read/write, presets, cues, scene save).
  **CLAUDE.md rule 6 applies:** if any per-light data is positional/aggregate-initialised, verify by
  **counting** every row after the edit and cross-check values — "it compiled" proves nothing.
- Cue crossfades: Actor-only is a bool — it switches at the cue's midpoint, never interpolated.

---

## 5. Surfaces

- **Rig panel / Lightbox:** per-light **Actor only** checkbox next to each projector/omni; rig-level
  Bounce-follows-key, Catchlight Actor-only, Hide shafts. Tooltip: "Lights only the rig's subject and
  everything they wear. Environment lights still light them. Cannot cast a visible shadow onto the
  set."
- **Director Console:** the same per-light toggle in its rig section (reachability rule).
- **Settings** (`settings_alchemy.xml`): `CineLightRigActorMaskEnable` (master, default true),
  `CineLightRigActorMaskDebug` (0 off / 1 overlay / 2 mask-only), `CineLightRigActorMaskBias`
  (polygon offset), and the per-rig/per-light keys carried by the param blob.
- Reset/preset registration for all of the above.

---

## 6. Instrumentation — the log states a verdict

Once per 5 s while any Actor-only light is active (`LL_INFOS("ActorMask")`):
`views=<n> subjects=<bits> mask_draws=<n> mask_px≈<n> actor_only_lights=<n> alpha_switches=<n>`

- `ACTORMASK OK` — mask non-empty for every subject whose avatar is on screen.
- `ACTORMASK EMPTY <slot>` — an Actor-only light is active, its subject is on screen, but the mask has
  zero pixels for that bit. **This is the "feature silently broken" outcome** and must never look like
  "light is just dim".
- `ACTORMASK OFFSCREEN <slot>` — subject not visible; mask empty is expected (separate verdict, so the
  instrument can't confuse the two).
- `ACTORMASK STATE-LEAK <what>` — invariant guard tripped.
- `ACTORMASK VIEW-UNCLASSIFIED <view>` — §4.6 fail-closed path hit.
- Pixel count via a cheap downsampled readback or occlusion query on the mask draw (reviewer picks;
  must not stall the frame).

Debug view: overlay tints masked pixels per subject bit; mask-only mode shows the raw R8.

---

## 7. Off-path must be inert (CLAUDE.md rule 1)

With no Actor-only light enabled (the default):
- no mask target allocated, no mask pass, no new programs bound;
- the four light lists, their shaders and uniforms are exactly today's;
- the alpha pool performs zero extra light-state switches;
- `deferredScreen` depth format unchanged → the ReShade bridge's hard-coded `0x81A6` stays true.

With Actor-only lights enabled, **the beauty pass for every non-actor pixel must be bit-identical** to
the same scene with those lights simply turned off. That is the correctness definition — test 3 in §9.

---

## 8. ReShade

- **No bridge rewrite.** Layout and depth format are untouched; every published semantic is unchanged.
- **RTGI interaction (INFERENCE, untested):** RTGI gathers from the final image, so a brighter actor
  can bounce light onto nearby set surfaces, partially undoing Actor-only in the composited frame.
  One in-world check after build (test 6). If it matters, the deferred follow-up is to publish
  `SL_ACTOR_MASK` as an ABI-appended semantic so an FX can exclude actor pixels from RTGI gathering.

---

## 9. Reviewer: attack these first (least-sure list)

1. **Depth agreement.** Will the mask pass's LEQUAL + offset reliably pass on the actor's own pixels
   for rigged mesh (re-skinned) and fail on pixels where something else is in front? Find the case
   where it flickers (z-fighting at silhouettes, LOD switch between G-buffer and mask pass, skinning
   palette updated between the two).
2. **Coverage of "everything they wear."** Enumerate what the Ghost harvest misses: unrigged attachment
   child prims, attached animesh, alpha-mask faces, flexi, sculpts, media faces, glow-only faces,
   materials with emissive-only. Each miss is a hole in the mask where the rig light vanishes.
3. **R8 OR semantics.** Which blend state actually yields bitwise-OR for two subjects on one pixel in
   UNORM8? (Additive works only if bits never repeat on a pixel; same subject drawn twice — overlapping
   attachments — would carry.) Consider `glLogicOp(GL_OR)` on an integer target vs `R8UI` vs one R8
   per subject.
4. **Shader registration order.** Any permutation added before `clearPermutations()` is erased —
   check all four programs and the `LIGHT_COUNT` loop.
5. **Alpha pool gating** (§4.4): find draws whose avatar ownership is ambiguous (attachments
   mid-detach, clones, animesh on a ghost) and the interleaved-alpha path.
6. **Every `renderDeferredLighting` caller** (§4.6) — list them; any unlisted view must fail closed.
7. **Rule 1:** prove the beauty pass and the feature-off path are untouched — specifically colour mask,
   blend enable/func, draw buffers and depth mask after the mask pass returns.
8. **Is the approach wrong?** Argue stencil-on-a-separate-depth-copy vs this shared-depth R8 mask vs an
   R8UI target; argue whether the mask pass should be folded into the ghost-deferred submission point.

---

## 10. Work packages and delegation

| WP | Content | Owner | Review |
|---|---|---|---|
| 1 | Actor Mask target, lazy alloc, shared depth, mask program(s), mask pass, invariant guard | **Codex** (implements, `--write`) | Opus adversarial |
| 2 | Four Actor-only light variants, light-list partition, batching by bit | **Fable** | Opus adversarial |
| 3 | Alpha/forward gating (§4.4), forward-slot pinning if required | **Fable** | Opus adversarial |
| 4 | §4.5/§4.6 consistency: Live Probe ignore list, probe/impostor exclusion, hero/VCam/Prism mask passes | **Codex** | Opus adversarial |
| 5 | Param blob fields, presets/cues/scene save (**rule 6 counting check**) | **Sonnet** | Opus adversarial |
| 6 | UI (rig panel, Lightbox, Director), settings, reset registration, tooltips | **Sonnet** | Opus adversarial |
| 7 | Instrumentation verdicts + debug views (§6) | **Opus** | independent Opus adversarial |

Rules: each implementer gets an explicit OFF-LIMITS file list (other WPs' files). Reviews are
adversarial per `CLAUDE.md` (attack the claim, name the least-sure items). Fixes are batched and
re-deferred until 0 must-fix across ALL packages; **then** one build by Claude; then the in-world test.
Codex is invoked only via `--prompt-file` (never a command-line prompt).

---

## 11. In-world test (outcomes stated in advance)

Run with `CineLightRigActorMaskDebug=1` available and the log open.

1. **Mask sanity.** Rig on SELF, one projector Actor-only. Debug overlay: *expect* exactly the avatar
   and everything worn tinted; background through hair cards NOT tinted; an object in front of the
   avatar NOT tinted. Log `ACTORMASK OK`.
2. **Isolation.** Stand on a light floor beside a wall, key light Actor-only. *Expect:* avatar lit; no
   pool of light on floor or wall. Toggle Actor-only off → the pool appears.
3. **Bit-identical background.** Screenshot with the Actor-only light ON vs the same light turned OFF.
   *Expect:* every non-actor pixel identical. Any difference = leak (report where).
4. **Attachments + hair.** Wear alpha-blended hair, a held prop, worn animesh. *Expect:* all lit by the
   Actor-only key; nothing else.
5. **Two subjects.** Rig A on Subject A, rig B on Subject B, both Actor-only. Hug pose. *Expect:* A's
   key lights only A, B's only B, including where they touch.
6. **RTGI.** Enable iMMERSE RTGI. *Expect (inference):* some bounce near the actor. Note magnitude.
7. **Mirror + VCam.** Actor in a mirror and a VCam feed. *Expect:* lit identically to main view.
8. **Off-path.** Disable every Actor-only flag. *Expect:* no `ACTORMASK` log lines, fps unchanged vs
   before the build.

If test 3 shows any non-actor difference, the feature is broken regardless of how good it looks.
