# Research Brief — Uniformly Scaling a Client-Only Cloned Avatar (+ Attachments) in an OpenGL SL Viewer

> **Purpose of this document:** A *research brief* to hand to an AI research assistant (OpenAI
> ChatGPT / deep research). It defines the problem, the render architecture, what has already been
> tried and exactly why each attempt failed, and the questions to answer. Produce a concrete,
> citation-backed recommendation for the CORRECT technique and where to implement it. Do the research;
> do not just restate this brief.

---

## 1. Objective

We have a working **client-only cloned avatar** ("entity clone" / `LLGhostAvatar`) in a Second Life
viewer (Alchemy/Firestorm-derived, OpenGL deferred renderer). It is a real `LLVOAvatar` that renders
through the normal avatar path with correct rigging, textures (baked + PBR), and animesh. We need to
**uniformly resize the entire clone — system body + rigged mesh clothing + NON-rigged prim attachments
+ animesh (animated-object) attachments — as ONE solid piece, pivoted at the feet (feet stay planted),
with no skeleton stretching and no flicker.** It is **client-only** (viewer-local; nothing may be sent
to the simulator). Real avatars must be unaffected.

The desired result is exactly what the viewer's *overlay* ghost already achieves (see §4): a single
uniform scale of the whole rendered figure about the foot.

## 2. Render architecture (context the researcher needs)

- **Viewer:** OpenGL, deferred/G-buffer renderer with PBR/GLTF materials. Large legacy C++ (LGPL
  Second Life viewer lineage). Windows / NVIDIA.
- **Avatar rendering:** an avatar is an `LLVOAvatar`. Its rigged/skinned mesh (system body and
  avatar-rigged mesh attachments) is drawn by **`LLDrawPoolAvatar`** using a **joint matrix palette**
  (skinning: each vertex is transformed by a weighted blend of bone/joint world matrices). Skinning
  effectively **interpolates vertices between joint positions.**
- **Skeleton:** `LLJoint` hierarchy; `mRoot` is the pelvis. Each joint has a local
  position/rotation/scale and a computed **world matrix** (`LLXform`/`LLXformMatrix`). `getPelvisToFoot()`
  gives the pelvis→foot offset used to plant feet.
- **Non-rigged attachments:** each is a separate `LLViewerObject`/`LLVOVolume` parented to an
  **attachment-point joint**. Its render (model) matrix is reconstructed by `LLXform` from the parent
  joint's **position and rotation only — the parent's SCALE is dropped** (this is the crux of one
  failure below). Static geometry then renders from that drawable render matrix.
- **Animesh / animated objects:** rigged to a *separate* `LLControlAvatar` skeleton, not the wearer's.
- **The clone specifics:** `LLGhostAvatar` is client-only (`mIsLocalOnly`); it clones attachments as
  local objects and tracks them in linksets. A per-instance `mScale` and a `/ghostscale` command + a
  Director spinner already exist and are wired to a `setInstanceScale()` — only the underlying render
  mechanism is wrong.

## 2A. Source repositories & where the code lives (READ THESE)

The viewer is open source (LGPL). Study the actual avatar-render code in these repositories:

- **Alchemy (the upstream this fork is based on) — PRIMARY reference for the render pipeline:**
  <https://github.com/AlchemyViewer/Alchemy>
- **This fork (where the client-only clone work lives):**
  <https://github.com/bwlupus-ctrl/Alchemy-Machinima> — branch `develop`. Its standard viewer
  rendering code matches the Alchemy base (commit `14063eefd25a5873887e90c4ea5e38012bc38fbe`); the
  `LLGhostAvatar` clone and the failed scaling attempts are a local layer on top of that.
- **Related upstreams with the SAME avatar-render architecture (useful cross-reference):**
  - Second Life official viewer (Linden Lab): <https://github.com/secondlife/viewer>
  - Firestorm: <https://github.com/FirestormViewer/phoenix-firestorm>

Key files/paths (standard `indra/` tree; the same layout in all of the above):
- `indra/newview/lldrawpoolavatar.cpp` / `.h` — the **avatar draw pool** (skinning, plus the deferred,
  shadow, and velocity passes). **The most likely place to inject a per-avatar uniform render/model
  scale.**
- `indra/newview/llvoavatar.cpp` / `.h` — `LLVOAvatar` (updateCharacter, mesh/skin, the avatar's
  render matrix, `getPelvisToFoot()`).
- `indra/llcharacter/lljoint.cpp` / `.h` — `LLJoint` skeleton and world matrices.
- `indra/llmath/xform.cpp` / `.h` — `LLXform`/`LLXformMatrix`. **Note:** an attachment's world
  transform is rebuilt from its parent joint's position+rotation, **dropping the parent's scale**
  (this defeated one of the attempts below).
- `indra/newview/llvovolume.cpp` — `LLVOVolume` static/attachment geometry render (the skinned-vs-static
  render-matrix branch that non-rigged attachments use).
- `indra/newview/llviewerjointattachment.cpp` / `.h` — attachment-point parenting.
- `indra/newview/llcontrolavatar.cpp` / `.h` — `LLControlAvatar` (the separate animesh skeleton).
- Avatar/rigged **skinning shaders** under `indra/newview/app_settings/shaders/` (deferred `avatar`/
  `rigged` skinning shaders) — relevant if the uniform scale is applied post-skin in-shader.

The clone-specific code (`LLGhostAvatar`, `indra/newview/llghostavatar.*`, `indra/newview/alghoststudio.*`)
is a local fork addition and may not exist in the public upstreams; the SOLUTION must fit the **standard
avatar render pipeline** shown fully by the Alchemy/Firestorm/Linden repositories above.

## 3. What has been tried, and EXACTLY why each failed (do not repeat these)

**Attempt A — scale the avatar ROOT JOINT** (`mRoot->setScale(s,s,s)` + pelvis-height compensation,
reapplied each frame).
- FAILED: scaling the root joint changes joint *spacing*; the skinned mesh reads as **taller/stretched**
  rather than uniformly larger (mesh cross-section/width comes from the shape, not joint scale). Non-
  rigged attachments did not follow correctly and **sank through the floor**.

**Attempt B — scale every JOINT WORLD MATRIX about the foot** (a new
`LLJoint::scaleWorldMatrixAboutPivot(scale, footPivot)` applying `T(foot)·S(scale)·T(-foot)` to each
joint's world matrix recursively — scaling the 3×3 basis and the pivot-relative translation — applied
*after* the base character update and undone *before* the next; same applied to animesh control
avatars; plus a follow-up that re-applied the same transform to each non-rigged attachment's
reconstructed drawable matrix).
- FAILED in-world: the body **flickers and exposes a STRETCHED skeleton — stretched neck, arms, torso.**
  Scaling the *joint* world matrices moves the bone positions apart, so the skinned mesh **stretches
  between the displaced bones** (skinning interpolates verts between joints; this is inherent to
  scaling bone transforms rather than the final geometry). The per-frame **apply-after / undo-before**
  sequence produces **visible flicker** (intermediate stretched state is exposed). Non-rigged prims
  still did not reliably resize because `LLXform` rebuilds their world transform from the parent
  joint's **position+rotation only, dropping scale** (`xform.cpp` reconstruct-from-parent path).
- CONCLUSION: **scaling the skeleton/joint matrices is the wrong lever.** Skinning must run at normal
  scale; the *output* must be uniformly scaled.

## 4. The reference that WORKS (and why it's not directly reusable)

The viewer's **overlay** ghost (a separate immediate-mode geometry re-draw, `drawGeometryGhost()`)
scales correctly: it wraps the entire draw in a single uniform GL transform about the foot —
`T(foot) · Rz(yaw) · S(scale) · T(-foot)` with `gGL.scalef(scale, scale, scale)` — so **the whole
already-skinned geometry (body + every attachment batch) is scaled uniformly as one piece, feet
planted.** This proves the correct *concept* (scale the final geometry uniformly, not the bones).
But it is a bespoke immediate-mode path; the **entity clone renders through the real
`LLVOAvatar`/`LLDrawPoolAvatar` deferred path**, which has no equivalent single "scale the whole
avatar's output" knob today. **The research goal is the real-path equivalent of that `gGL.scalef`.**

## 5. Core question

**What is the correct way to apply a uniform, foot-pivoted scale to the FINAL rendered output of a
skinned `LLVOAvatar` (and all its rigged + non-rigged + animesh attachments) in a deferred OpenGL
avatar renderer — scaling the geometry uniformly WITHOUT stretching the skinned mesh and WITHOUT
flicker — and where in the code should it be injected?**

Leading hypothesis to validate or refute: insert a **uniform model/world render-matrix scale** for the
whole avatar (an object-space transform applied to the avatar's model matrix / the matrix that
positions its skinned output and all its attachment drawables in world space), so skinning runs at
native scale and the *result* is scaled uniformly about the foot — the deferred-path analogue of the
overlay's `gGL.scalef`. Determine whether that is feasible per-avatar in `LLDrawPoolAvatar` / the
deferred avatar shaders (e.g. a per-avatar model-matrix or an extra uniform), and how it composes with
the skinning matrix palette, shadow pass, velocity/motion-vector pass, reflection probes, and picking.

## 6. Sub-questions to answer

1. In `LLDrawPoolAvatar` / the deferred avatar render, where is the avatar's **model/world matrix**
   established, and can a **uniform scale about a world pivot (the foot)** be inserted there per-avatar
   so it multiplies the final skinned vertex positions (and all attachment drawables) uniformly?
2. How do **rigged (skinned) attachments** get their transform (matrix palette) vs **non-rigged
   attachments** (object model matrix from the attachment joint)? What single injection point scales
   **both** consistently? (Note the `LLXform` "drops parent scale" behavior — the fix must not rely on
   parent-joint scale propagation.)
3. **Animesh**: its `LLControlAvatar` skeleton is separate — how does a whole-clone uniform scale reach
   animesh attachments so they scale by the same factor and stay attached?
4. **Feet planting**: how to choose/apply the pivot (root position − pelvisToFoot) so the scale keeps
   feet on the ground at any factor, in the model-matrix approach.
5. **Flicker-free**: an approach that does not require per-frame apply/undo of skeleton state. Is a
   render-time model-matrix (or shader uniform) scale inherently flicker-free because it never mutates
   persistent skeleton/animation state?
6. **Shadow / velocity / reflection / picking / LOD**: does the scale need to be replicated into the
   shadow pass, motion-vector pass, reflection-probe capture, selection/pick, and bounding-box/LOD
   (`calcLOD`) so the scaled clone shadows, occludes, and selects correctly?
7. Is there prior art for **uniformly scaling an avatar** in SL viewers or other skinned-character
   engines at render time (a per-instance model scale / GLTF node scale / "actor scale"), as opposed
   to editing the skeleton? How do they keep skinning un-stretched?
8. Alternative approaches to compare: (a) per-avatar model-matrix scale in the draw pool; (b) a scale
   uniform in the avatar/skinning shaders applied post-skin; (c) baking a uniform scale into the joint
   matrix palette in a way that scales geometry uniformly rather than repositioning bones (is that even
   possible — scaling each bind/output pair uniformly?); (d) scaling the whole avatar spatial group /
   render node. Give pros/cons and pick one.

## 7. Constraints

- **Client-only:** viewer-local only; never send scale/position/appearance to the simulator. Real
  (non-clone) avatars must be completely unaffected.
- Must cover **system body + rigged clothing + non-rigged prims + animesh**, uniformly, feet planted.
- **No stretch, no flicker.** Must visually match the overlay ghost at the same factor.
- Fits the existing deferred OpenGL avatar pipeline without a renderer rewrite.

## 8. Required output

1. A clear **recommended mechanism** and the **exact injection point(s)** in the `LLVOAvatar` /
   `LLDrawPoolAvatar` / deferred-shader / attachment-drawable path, with rationale.
2. How it scales rigged + non-rigged + animesh uniformly and keeps feet planted, without stretching
   skinned mesh or flickering.
3. Which auxiliary passes (shadow/velocity/reflection/pick/LOD) must also receive the scale.
4. A comparison of the alternatives in §6.8 and why the recommendation wins.
5. Pseudocode / concrete API sketch against the SL viewer classes named here.
6. A **REFERENCES** section (SL viewer render source, skinning/model-matrix docs, GLTF node scaling,
   skinned-character uniform-scale prior art) suitable for seeding further ChatGPT research.

## 9. Success criteria

A strong answer identifies the render-time uniform-scale approach (not skeleton scaling), pinpoints the
code injection point that covers body + rigged + non-rigged + animesh, plants the feet, is inherently
flicker-free, enumerates the auxiliary passes that need the scale, and cites primary sources — so a
follow-up implementation pass can execute it directly.
