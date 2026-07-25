# Clone avatar physics — static audit findings

Independent static audit, 2026-07-25, against `develop`. **Nothing was run or observed in-world.**
Where it matters the audit distinguishes *"the code path is correct"* from *"it will look right"*.

**Verdict: no BROKEN findings. The implementation is structurally sound.** Three RISKY items are
tracked separately in `doc/GHOST_PHYSICS_AUDIT_FIXES_BRIEF.md`; this document records the
**verification** work, which is the part worth keeping as reference.

---

## FINE — verified sound

### 1. No agent / simulator / COF dependency
`llphysicsmotion.cpp` contains **exactly one** self/agent reference in the whole file:
`const bool is_self = (dynamic_cast<LLVOAvatarSelf*>(mCharacter) != NULL)` (`:703`), used only as an
LOD bypass (`:704`). No `gAgent`, `isSelf`, `getRegion`, `mIsLocalOnly`, `sendAnimation`,
`gMessageSystem`, `LLAppearanceMgr` or COF usage anywhere else; the `#include "llagent.h"` (`:37`) is
vestigial. Everything the motion needs comes off `LLCharacter`: `getJoint("mChest"/"mPelvis")`
(`:222`), `getVisualParam()` (`:157`, `:226`), `getPixelArea()` (`:701`), `setVisualParamWeight()`
(`:675`, `:775`), `updateVisualParams()` (`:474`).

A ghost is never `isSelf()`, so it takes the non-self branch everywhere — the already-exercised
upstream path for every other resident you see jiggle.

**Skeleton-readiness:** `onInitialize` hard-crashes via `llassert_always(false)` (`:285, 306, 329,
351, 373, 397`) if `getJoint()` returns null. Safe as written because `initInstance()` calls
`buildCharacter()` before `createObjectViewer` returns, and every `setEntityPhysicsEnabled` call site
runs after that. **A landmine only if physics is ever started earlier.**

**Skeleton reset:** ghosts hit `setOverallAppearanceNormal()` → `resetSkeleton(false)` once. This does
**not** dangle the physics joint pointers — `allocateCharacterJoints()` only reallocates when the
bone count changes (`llavatarappearance.cpp:677-687`).

### 2. Driver confirmed: joint world-position delta
`llphysicsmotion.cpp:430-439` takes `joint->getWorldPosition()`, diffs against `mPosition_world`
(refreshed only at `:724`), and converts to local velocity. Joints are `mChest` (breast ×3) and
`mPelvis` (butt ×2, belly ×1) (`:279, 303, 326, 347, 370, 393`). Avatar travel matters only insofar
as it moves those joints.

- **Stationary clone playing an animation → jiggles** (keyframe motions move `mPelvis`/`mChest`).
- **Clone moved by `LLActorMover` → jiggles** (`applyOverride` writes the root world position,
  `llactormover.cpp:1894-1895`; descendants shift with it). Same for formation motion.
- **Frozen/paused clone → completely still**, and keeps its mid-jiggle pose: the paused branch runs
  `updateIdleActiveMotions()` which never calls `onUpdate()` (`llmotioncontroller.cpp:911-914`).

**Two timing facts:** physics is `ADDITIVE_BLEND` (`llphysicsmotion.h:86`) so it updates *before*
regular motions and always reads **last frame's** pose (upstream behaviour, uniform across avatars).
And `mAnimTime` is scaled by the clone's `setAnimTimeFactor`, so **per-clone anim speed automatically
slows/speeds the jiggle in lockstep** — slow-mo machinima should look right.

### 3. Uniform scale composes correctly (no fight, no double-apply)
The outer transform is **render-only**: built at `llghostavatar.cpp:168-203`, consumed as a post-skin
modelview multiply at `llvoavatar.cpp:5544-5550` and `lldrawpool.cpp:729`. Joint world positions are
entirely pre-transform.

Physics output lands *inside* that space: the driven params (1200-1207) are `<param_morph>` with
`<volume_morph>` payloads on collision volumes `LEFT_PEC`/`RIGHT_PEC`/`BELLY`/`BUTT`
(`avatar_lad.xml:7526-7605`, `8111-8155`); `LLPolyMorphTarget::apply` translates those joints
(`llpolymorph.cpp:643-651`), which feed the rigged matrix palette, which is then multiplied by the
outer matrix.

**Net result:** a clone at scale 2.0 gets the same normalized response rendered through a 2× matrix —
**jiggle relative to the body looks identical to an unscaled clone.** Also confirmed: physics cannot
perturb the foot pivot (`computeBodySize()` only reruns on `mSkeletonSerialNum` change, which
`param_morph` params do not bump), so no per-frame `invalidateModelMatrixCache()` churn.

*Aesthetic caveat (not a bug):* the forcing function reads **unscaled** world travel, so a 2× giant
walked 1m jiggles as if a normal avatar walked 1m, then rendered 2×. Physically a giant should jiggle
less per metre. Correct treatment if ever wanted: divide the measured delta by `getUniformScale()`
before `toLocal()`. **Recommended NOT to change** — current behaviour is the intuitive "same avatar,
bigger" result.

### 4. Physics params genuinely reach the clone — verified end to end
1. **Transmitted:** all physics behaviour params are `group="0"` (`VISUAL_PARAM_GROUP_TWEAKABLE`),
   IDs 10000-10029 (`avatar_lad.xml:17041`+): `Breast_Physics_Mass/_Gravity/_Drag/_UpDown_Max_Effect/
   _Spring/_Gain/_Damping` plus InOut/LeftRight variants, `Belly_Physics_*` from 10011,
   `Butt_Physics_*` from 10018. `parseAppearanceMessage` collects exactly those groups
   (`llvoavatar.cpp:10167-10186`).
2. **Copied by value and re-resolved:** `copyAppearanceFrom` rebuilds `mParams` by ID against the
   clone's own list with `llassert(my_param != src_param)` (`llvoavatar.cpp:10396-10412`), then
   `applyParsedAppearanceMessage(..., slam_params=true)` (`:10424`, applied `:10508-10517`).
3. **Controllers correctly NOT copied:** `*_Physics_*_Controller` (1100-1105) and `_Driven`
   (1200-1207) are `group="1"` (ANIMATABLE) — untransmitted, driven locally by the motion.
4. **Read per frame from the clone:** `getParamValue` caches `mCharacter->getVisualParam(name)`
   (`:149-168`).

**Expected non-bug:** `behavior_maxeffect` gates everything (`:766-773`). If the source wears **no
physics layer**, max-effect is 0, the driven param pins to its midpoint, and **the motion runs but
nothing moves.** Do not chase this as a fault. Also: breast params are `sex="female"` and
`LLPolyMorphTarget::apply` forces non-matching-sex params to default (`llpolymorph.cpp:565`), so male
clones get belly/butt only.

### 5. No shared state — clones cannot interfere
Complete static inventory: `sDefaultController` (`:202, 218`, read-only in practice — but accessed
via `std::map::operator[]` which *would* default-insert a missing key; all 7 used keys are
pre-populated, the only absent one `"Smoothing"` is dead code — **latent only**);
`controller_key[]` (`:137-147`, immutable); `av_physics` `LLCachedControl` (`:458`, the global
`AvatarPhysics` kill switch — **turning it off kills clone physics too**); `smoothing` (`:444`,
const); `sPhysicsLODFactor` (read-only here).

Everything mutable is per-avatar: one controller per `LLMotionController` (a member of
`LLCharacter`); each of the 6 `LLPhysicsMotion`s owns its own `mPosition_local/_world`,
`mVelocity_local`, `mLastTime`, `mParamCache`; visual params are per-avatar objects; collision
volumes are per-avatar; morph vertex buffers are per-avatar (only the read-only
`LLPolyMeshSharedData` base coords are shared).

**Verdict: N clones of the same source cannot interfere with each other or with the source.**

### 8. No simulator traffic; source cannot be mutated
`setEntityPhysicsEnabled` calls **`LLCharacter::startMotion`/`stopMotion` explicitly qualified**
(`llghostavatar.cpp:232, 242, 246`), bypassing `LLVOAvatar`'s overrides and landing straight in
`LLMotionController`. **Preserve this deliberately** — it is what makes the `gAgent` branches at
`llvoavatar.cpp:6693`/`:6737` structurally unreachable.

On `llvoavatar.cpp:6583` (`gAgent.sendAnimationRequest(ANIM_AGENT_DO_NOT_DISTURB, …)`, genuinely
*not* `isSelf()`-gated): it is reachable only from `processAnimationStateChanges()` iterating
`mPlayingAnimations`. **A ghost's `mPlayingAnimations` and `mSignaledAnimations` are both permanently
empty**, and the physics path never calls `processAnimationStateChanges()`. Not reachable here.

The one indirect call into `LLVOAvatar` is `updateVisualParams()` (`:474`), which can call virtual
`stop/startMotion(ANIM_AGENT_SIT)` (`llvoavatar.cpp:7682-7684`) — safe twice over: it requires
`getSex() != avatar_sex && mIsSitting` and physics params never touch the `male` param; and the
virtual start/stop gate all `gAgent` work on `isSelf()`. `requestStopMotion()` is likewise
unreachable (physics has `getDuration()==0` and `getLoop()==true`, so `mSendStopTimestamp = F32_MAX`).

Every write is `mCharacter->setVisualParamWeight(...)` where `mCharacter` is the clone, resolving
through the clone's own `mVisualParamIndexMap`. **The source's motion controller is never touched.**

---

## Lifecycle — FINE
**Destruction:** no leak, no dangle. `LLMotionController` is a by-value member of `LLCharacter`;
`~LLMotionController` → `deleteAllMotions()` → deletes the controller → deletes its 6 motions
(`llphysicsmotion.cpp:243-251`). `mCharacter` is a raw back-pointer of the same lifetime.

**Replacement:** correct. Every path (`spawnEntityClone`, `refreshEntityClone`, recovery,
`duplicateInstanceInPlace`) routes through `onEntityRuntimeReplaced` → `applyEntityRuntimeState` →
`setEntityPhysicsEnabled(inst.mPhysicsEnabled)`. The `enabled == mEntityPhysicsEnabled` early-return
branch **still starts the motion** when the member already defaults true but no motion exists on the
fresh clone — that guard is doing real work and is why refresh does not silently lose physics.

**Pause:** freezes and resumes cleanly, no leak.

---

## Suggested in-world test order
1. One clone, source with **known-strong physics**, playing a dance — confirm jiggle exists at all.
2. Same clone at scale **0.5 / 1.0 / 2.0** — confirm the proportional-scaling prediction (§3).
3. Toggle Physics **off** mid-jiggle — look for frozen morph residue (FIX 1 in the fixes brief).
4. Freeze, drag with the gizmo, unfreeze — look for the deferred velocity pop.
5. 20+ clones with Tracy running (`AVATAR` zone) for real cost numbers.

## Not determinable from static reading
- **Whether it looks right** — amplitude, phase relative to the animation, and how it reads on a
  *mesh body* (which skins to collision volumes rather than the system mesh) are render outcomes.
- **Real per-frame cost** — morph vertex counts live in binary `.llm` assets; needs a Tracy capture.
- **Whether typical sources carry non-zero max-effect** — runtime wearable state.
