# Uniformly Scaling a Client-Only Cloned Avatar and Attachments

## Deep Research Report for the Alchemy-Machinima `develop` Branch

**Target viewer:** Alchemy-Machinima, branch `develop`  
**Upstream architecture:** Alchemy / Second Life viewer lineage  
**Problem:** Uniformly resize a client-only `LLGhostAvatar`—system body, rigged clothing, non-rigged prim attachments, and attached animesh—as one solid entity, pivoted at the feet, without skeleton stretching or flicker.

---

## Executive Verdict

The correct architecture is:

> **Run all native skinning and attachment transforms normally, then apply one client-only, foot-pivoted outer render transform to the completed geometry.**

The transform is:

\[
M_{outer} = T(P_{foot})\,S(s,s,s)\,T(-P_{foot})
\]

where:

- `P_foot` is the clone's render-space foot pivot;
- `s` is the uniform scale factor;
- all skeleton, animation, attachment-point, and animesh control-avatar state remains unchanged.

There is no single existing model matrix that controls both the system avatar body and every attachment in this viewer. The implementation therefore needs **one semantic outer transform consumed through two physical renderer hooks**:

1. **System avatar body and rigid avatar parts:** a scoped model-view override used by `LLVOAvatar::renderSkinned()` and `LLVOAvatar::renderRigid()`, including the matrix returned by `LLDrawPoolAvatar::getModelView()`.
2. **Rigged, static, PBR, and animesh volume batches:** a separate outer-transform owner on `LLDrawInfo`, composed in `LLRenderPass::applyModelMatrix()` as:

```text
View × OuterCloneTransform × ExistingObjectModelTransform
```

This is the deferred-renderer equivalent of the working overlay path's `gGL.scalef()`.

---

## 1. What the Linked Viewer Actually Renders

### 1.1 The system avatar body bypasses `LLDrawInfo`

In `LLDrawPoolAvatar::renderAvatars()`:

- rigid avatar parts such as eyeballs are drawn through `avatarp->renderRigid()`;
- the system body is drawn through `avatarp->renderSkinned()`;
- avatar shadow rendering also directly invokes `renderSkinned()` in several pass variants.

These calls do not depend on `LLDrawInfo::mModelMatrix` and do not pass through the generic `LLRenderPass::pushBatch()` volume path.

**Consequence:** assigning a model matrix to attachment draw calls cannot, by itself, scale the system avatar body.

Source:

- [`lldrawpoolavatar.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/lldrawpoolavatar.cpp)

---

### 1.2 The body skin palette already contains model-view state

`LLViewerJointMesh::uploadJointMatrices()` obtains system-avatar joint world matrices and combines them with the avatar draw pool's model-view matrix before uploading the palette used for hardware skinning.

The body shader then consumes the resulting skinned position as an eye-space result and applies projection.

A critical detail is that `LLDrawPoolAvatar::getModelView()` reconstructs its matrix from the global `gGLModelView` data rather than merely trusting any temporary local matrix-stack multiplication.

Therefore, this is not sufficient by itself:

```cpp
gGL.pushMatrix();
gGL.multMatrix(outer);
avatar->renderSkinned();
gGL.popMatrix();
```

The weighted body palette may still be generated from an unscaled model-view unless the override is made visible to `LLDrawPoolAvatar::getModelView()` as well.

Sources:

- [`llviewerjointmesh.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/llviewerjointmesh.cpp)
- [`avatarSkinV.glsl`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/app_settings/shaders/class1/avatar/avatarSkinV.glsl)
- [`lldrawpoolavatar.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/lldrawpoolavatar.cpp)

---

### 1.3 Volume attachments do use draw-call model matrices

Volume batching distinguishes between rigged and non-rigged geometry:

- rigged meshes normally use a null object model matrix because their final placement comes from the skinning palette;
- active or static non-rigged volumes receive a drawable world, render, or region matrix;
- `LLDrawInfo` stores the model matrix, skinning avatar, and skin information separately.

Typical relevant fields include:

```cpp
const LLMatrix4*          mModelMatrix;
LLPointer<LLVOAvatar>     mAvatar;
LLPointer<LLMeshSkinInfo> mSkinInfo;
```

Batch compatibility also considers properties such as the model matrix, skinning avatar, and skin hash.

Sources:

- [`llvovolume.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/llvovolume.cpp)
- [`llspatialpartition.h`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/llspatialpartition.h)

---

### 1.4 `LLRenderPass::applyModelMatrix()` is the correct volume hook

The generic volume rendering path loads the current pass's base `gGLModelView` and multiplies it by the native object model matrix when one is present.

The current semantic transform is:

\[
M_{modelview} = V M_{object}
\]

The clone implementation should extend it to:

\[
M_{modelview} = V M_{outer} M_{object}
\]

For a rigged attachment, where the ordinary object matrix is normally null, the shader receives the equivalent of:

\[
V M_{outer} M_{skin}
\]

This places the clone scale outside the normal skinning result.

Sources:

- [`lldrawpool.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/lldrawpool.cpp)
- [`materialV.glsl`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/app_settings/shaders/class1/deferred/materialV.glsl)

---

### 1.5 Animesh needs a separate outer-transform owner

An attached animated object is skinned by an `LLControlAvatar`, not directly by the wearer. The control avatar can locate the attached/wearing avatar through `LLControlAvatar::getAttachedAvatar()`.

These are different concepts:

```text
mAvatar / skinning owner = skeleton that supplies the matrix palette
outer transform owner    = clone whose whole rendered entity is being scaled
```

For an attached animesh object:

```text
skinning owner        = LLControlAvatar
outer transform owner = LLGhostAvatar / wearer clone
```

Do not overload `LLDrawInfo::mAvatar` with the new meaning.

Source:

- [`llcontrolavatar.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/llcontrolavatar.cpp)

---

## 2. Why an Outer Transform Does Not Stretch the Skeleton

Let the normally skinned vertex be:

\[
p_{skin} = \sum_i w_i B_i p
\]

where `B_i` is the final native skin transform for joint `i`.

Apply one identical transform after skinning:

\[
p_{final} = M_{outer} p_{skin}
\]

Then:

\[
p_{final} = M_{outer}\sum_i w_i B_i p
\]

Every point of the already-deformed body is enlarged by the same factor around the same pivot. The following remain unchanged:

- bone spacing;
- joint animation;
- shape deformation;
- attachment-point state;
- the separate animesh skeleton.

For a static attachment:

\[
p_{final} = M_{outer} M_{object} p
\]

For rigged clothing:

\[
p_{final} = M_{outer}\sum_i w_i B_i p
\]

For animesh:

\[
p_{final} = M_{outer}\sum_i w_i C_i p
\]

where `C_i` is supplied by its `LLControlAvatar`.

The whole entity shares the same final scale without sharing or altering one skeleton.

Relevant prior-art model:

- [Khronos glTF 2.0 Specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)

---

## 3. Recommended Data Model

Create a small, reference-counted client-only transform object owned by the clone:

```cpp
struct LLClientOuterTransform final : public LLRefCount
{
    LLMatrix4 mCurrent;
    LLMatrix4 mPrevious; // reserved for motion-vector support
    LLMatrix4 mInverse;

    LLVector3 mFootPivot;
    F32       mScale = 1.f;
    U32       mRevision = 1;
    bool      mEnabled = false;
};
```

Add accessors to `LLVOAvatar` or the clone subclass:

```cpp
class LLVOAvatar
{
public:
    bool hasClientOuterTransform() const;
    const LLMatrix4& getClientOuterTransform() const;
    const LLMatrix4& getClientOuterTransformInverse() const;
    U32 getClientOuterTransformRevision() const;
    LLClientOuterTransform* getClientOuterTransformHandle() const;

protected:
    LLPointer<LLClientOuterTransform> mClientOuterTransform;
};
```

Enable it only for the local clone:

```cpp
bool LLVOAvatar::hasClientOuterTransform() const
{
    return mIsLocalOnly &&
           mClientOuterTransform.notNull() &&
           mClientOuterTransform->mEnabled &&
           !is_approximately_one(mClientOuterTransform->mScale);
}
```

Do not place the scale in:

- simulator object updates;
- avatar appearance messages;
- shape parameters;
- server-visible object scale;
- persistent joint transforms.

### Why a shared transform handle is preferable

A shared handle provides:

- stable lifetime;
- one authoritative current matrix;
- one inverse for picking;
- one revision number for renderer cache invalidation;
- no full matrix copy in every draw call;
- immediate scale and pivot updates without rewriting every existing `LLDrawInfo`.

`LLDrawInfo` is intentionally compact, so a pointer-sized owner field is preferable to embedding multiple matrices.

---

## 4. Computing the Foot-Pivoted Transform

Reuse the exact foot-pivot calculation already used by the working overlay path.

Conceptually:

```cpp
LLVector3 foot = getRenderPosition();
foot.mV[VZ] -= getPelvisToFoot();

M_outer =
    translate(foot) *
    uniform_scale(scale) *
    translate(-foot);
```

The pivot remains invariant:

\[
P_{foot} + s(P_{foot} - P_{foot}) = P_{foot}
\]

### Required details

- Build the matrix in the same region/render coordinate space used by avatar joints and drawable matrices.
- Do not feed global-double coordinates directly into a region-space renderer matrix.
- Reuse the overlay helper to avoid two definitions of the foot pivot.
- Do not apply clone yaw twice. Include yaw in the outer matrix only when the clone's native transforms do not already contain that orientation.
- Restrict the scale to a positive, nonzero value unless negative-scale winding and inverse-handling are deliberately implemented.

A useful validation invariant is:

```cpp
transformPoint(M_outer, foot) == foot;
```

A second test point should produce the same result as the immediate-mode overlay transform at the same scale, position, and yaw.

---

## 5. Exact System-Avatar Implementation

### 5.1 Add a scoped model-view override to `LLDrawPoolAvatar`

```cpp
class LLDrawPoolAvatar
{
public:
    static LLMatrix4& getModelView();

    static void pushModelViewOverride(const LLMatrix4& matrix);
    static void popModelViewOverride();

private:
    static bool      sHasModelViewOverride;
    static LLMatrix4 sModelViewOverride;
};
```

Modify `getModelView()`:

```cpp
LLMatrix4& LLDrawPoolAvatar::getModelView()
{
    if (sHasModelViewOverride)
    {
        return sModelViewOverride;
    }

    static LLMatrix4 ret;
    ret.initRows(
        LLVector4(gGLModelView + 0),
        LLVector4(gGLModelView + 4),
        LLVector4(gGLModelView + 8),
        LLVector4(gGLModelView + 12));
    return ret;
}
```

The existing path remains unchanged for all ordinary avatars.

---

### 5.2 Use an RAII scope

```cpp
class LLScopedAvatarOuterModelView
{
public:
    explicit LLScopedAvatarOuterModelView(const LLVOAvatar* avatar)
    {
        if (!avatar || !avatar->hasClientOuterTransform())
        {
            return;
        }

        mActive = true;

        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.pushMatrix();

        // Start with the active camera/light/probe view.
        gGL.loadMatrix(gGLModelView);

        // Compose the clone-wide post transform.
        gGL.multMatrix(
            avatar->getClientOuterTransform().mMatrix);

        mComposedModelView = captureCurrentViewerModelViewMatrix();

        // Ensure avatar palette generation sees the same matrix.
        LLDrawPoolAvatar::pushModelViewOverride(mComposedModelView);

        // Prevent a stale generic volume transform cache.
        LLRenderPass::invalidateModelMatrixCache();
    }

    ~LLScopedAvatarOuterModelView()
    {
        if (!mActive)
        {
            return;
        }

        LLDrawPoolAvatar::popModelViewOverride();

        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.popMatrix();

        LLRenderPass::invalidateModelMatrixCache();
    }

private:
    bool      mActive = false;
    LLMatrix4 mComposedModelView;
};
```

`captureCurrentViewerModelViewMatrix()` is a small adapter around the renderer's current model-view storage.

The renderer already synchronizes model-view and inverse-transpose normal matrices through its matrix-upload path:

- [`llrender.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/llrender/llrender.cpp)

---

### 5.3 Instantiate the scope at avatar render entry points

Recommended location:

```cpp
U32 LLVOAvatar::renderSkinned()
{
    LLScopedAvatarOuterModelView outer_scope(this);

    // Existing rendering follows unchanged.
    ...
}
```

and:

```cpp
U32 LLVOAvatar::renderRigid()
{
    LLScopedAvatarOuterModelView outer_scope(this);

    // Existing rendering follows unchanged.
    ...
}
```

This central placement covers:

- ordinary avatar rendering;
- deferred body rendering;
- eyeballs and rigid avatar pieces;
- avatar shadow variants that call `renderSkinned()`;
- full-geometry impostor capture;
- reflection views that use the standard avatar path.

An alternative is to wrap each direct call in `LLDrawPoolAvatar`, but the two approaches must not both be active or the scale will be applied twice.

### Why this is flicker-free

The renderer changes only transient draw state. It never mutates:

- `LLJoint` local transforms;
- joint world matrices;
- animation state;
- attachment-point transforms;
- `LLControlAvatar` skeletons.

There is no per-frame apply/undo window in which another subsystem can observe a stretched intermediate skeleton.

---

## 6. Exact Volume and Attachment Implementation

### 6.1 Add a separate outer-transform field to `LLDrawInfo`

```cpp
LLPointer<LLClientOuterTransform> mOuterTransform;
```

Keep the meanings distinct:

```text
mAvatar         = skinning palette owner
mModelMatrix    = native object/world model transform
mOuterTransform = clone-wide post-skin/post-object transform
```

---

### 6.2 Stamp the transform owner onto every cloned attachment object

Each client-only cloned attachment root and every child in its linkset should carry a viewer-local transform owner:

```cpp
class LLViewerObject
{
public:
    LLClientOuterTransform* getClientOuterTransform() const;
    void setClientOuterTransform(LLClientOuterTransform* transform);

private:
    LLPointer<LLClientOuterTransform> mClientOuterTransform;
};
```

When cloning an attachment linkset:

```cpp
void LLGhostAvatar::stampRenderOwner(LLViewerObject* object)
{
    if (!object)
    {
        return;
    }

    object->setClientOuterTransform(mClientOuterTransform);

    for (LLViewerObject* child : object->getChildren())
    {
        stampRenderOwner(child);
    }
}
```

Stamp all of the following:

- ordinary prim attachments;
- rigged clothing roots;
- every linkset child;
- attached animesh roots;
- static children of animesh linksets.

Explicit ownership is more reliable than inferring the wearer from parent joints, especially because static faces have no skinning avatar and animesh faces identify an `LLControlAvatar` as their skinning owner.

---

### 6.3 Populate the field when building draw information

In `LLVolumeGeometryManager::registerFace()` or the corresponding local batch-building function:

```cpp
LLClientOuterTransform* outer =
    resolveClientOuterTransform(facep->getViewerObject());

draw_info->mOuterTransform = outer;
```

Add the owner to the batch compatibility check:

```cpp
info->mModelMatrix    == model_mat &&
info->mAvatar         == facep->mAvatar &&
info->mOuterTransform == outer &&
info->getSkinHash()   == facep->getSkinHash()
```

This prevents geometry from two clones with different scales from being merged into one draw batch.

---

### 6.4 Extend `LLRenderPass::applyModelMatrix()`

Use a cache key that includes both the native object matrix and the outer transform owner/revision:

```cpp
struct LLAppliedTransformKey
{
    const LLMatrix4*                mModel = nullptr;
    const LLClientOuterTransform*   mOuter = nullptr;
    U32                             mOuterRevision = 0;

    bool operator==(const LLAppliedTransformKey& rhs) const;
};

static LLAppliedTransformKey sLastAppliedTransform;
```

Then:

```cpp
void LLRenderPass::applyModelMatrix(const LLDrawInfo& params)
{
    LLClientOuterTransform* outer =
        params.mOuterTransform.get();

    LLAppliedTransformKey key;
    key.mModel = params.mModelMatrix;
    key.mOuter = outer;
    key.mOuterRevision = outer ? outer->mRevision : 0;

    if (key == sLastAppliedTransform)
    {
        return;
    }

    sLastAppliedTransform = key;

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.loadMatrix(gGLModelView);

    if (outer && outer->mEnabled)
    {
        gGL.multMatrix(
            reinterpret_cast<const GLfloat*>(
                outer->mCurrent.mMatrix));
    }

    if (params.mModelMatrix)
    {
        gGL.multMatrix(
            reinterpret_cast<const GLfloat*>(
                params.mModelMatrix->mMatrix));
    }

    ++gPipeline.mMatrixOpCount;
}
```

The composition is:

```text
camera/light/probe view
    × clone outer transform
    × ordinary object model transform
    × vertex
```

Add an invalidation helper:

```cpp
void LLRenderPass::invalidateModelMatrixCache()
{
    sLastAppliedTransform = {};
}
```

Call it:

- when entering and leaving the scoped avatar model-view override;
- when the clone scale or pivot revision changes;
- anywhere else that directly replaces the active model-view state outside the usual cache path.

The existing cache cannot remain keyed only by `mModelMatrix` because that would permit stale scale/pivot state.

---

## 7. Geometry-Category Behavior

| Geometry category | Native transform source | Final transform |
|---|---|---|
| System body | Avatar joint palette containing model-view | `View × Outer × NativeSkin` |
| Eyeballs / rigid body parts | Current model-view and joint world matrix | `View × Outer × Joint` |
| Rigged clothing | Rigged palette; object matrix commonly null | `View × Outer × Skin` |
| Non-rigged prim | Drawable/object model matrix | `View × Outer × Object` |
| Rigged animesh | `LLControlAvatar` matrix palette | `View × OuterOfWearer × ControlSkin` |
| Static animesh children | Child object model matrix | `View × OuterOfWearer × Object` |

This does not rely on parent-joint scale propagation. That is important because the viewer's xform path reconstructs child world state from selected parent components and does not provide the desired one-piece inherited actor transform.

Source:

- [`xform.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/llmath/xform.cpp)

---

## 8. Animesh Ownership Resolution

Explicitly stamped ownership should be authoritative. A fallback resolver can use `LLControlAvatar::getAttachedAvatar()`:

```cpp
LLClientOuterTransform*
resolveClientOuterTransform(LLViewerObject* object,
                            LLVOAvatar* skinning_avatar)
{
    if (object)
    {
        if (auto* outer = object->getClientOuterTransform())
        {
            return outer;
        }
    }

    if (auto* control =
            dynamic_cast<LLControlAvatar*>(skinning_avatar))
    {
        LLVOAvatar* wearer = control->getAttachedAvatar();

        if (wearer && wearer->hasClientOuterTransform())
        {
            return wearer->getClientOuterTransformHandle();
        }
    }

    if (skinning_avatar &&
        skinning_avatar->hasClientOuterTransform())
    {
        return skinning_avatar
            ->getClientOuterTransformHandle();
    }

    return nullptr;
}
```

Do not apply a global scale to the `LLControlAvatar` skeleton. Its animation and skinning should remain native; only its completed rendered volume batches receive the wearer clone's outer transform.

---

## 9. Auxiliary Rendering and Interaction Passes

### 9.1 Main deferred, forward, alpha, and PBR paths

The two core hooks divide responsibility:

- avatar body and eyes use the scoped avatar model-view;
- all volume geometry uses the extended `LLRenderPass::applyModelMatrix()`.

Audit every specialized volume path to ensure it calls the common transform helper or reproduces the same composition order.

---

### 9.2 Shadows

Body shadow rendering directly invokes `renderSkinned()` in several material modes. A scope created inside `renderSkinned()` automatically covers those calls.

Volume shadow paths should use:

```text
LightView × Outer × NativeGeometry
```

Any specialized shadow renderer that directly loads `gGLModelView` and `mModelMatrix` must be updated to include the outer transform.

---

### 9.3 Reflection probes and secondary cameras

The outer matrix is composed with the current pass's base view, so it works with:

- the main camera;
- shadow lights;
- reflection probes;
- cube-map faces;
- impostor capture cameras.

This is safer than a shader uniform containing a pivot precomputed only in main-camera eye space.

---

### 9.4 Impostors

Recommended policy:

1. During impostor texture capture, render the full geometry with the outer transform active.
2. Compute capture dimensions and extents from transformed bounds.
3. Invalidate the impostor when the transform revision changes.
4. Do not apply the outer transform again when drawing the completed billboard.

Applying it both during capture and billboard draw would double-scale the avatar.

---

### 9.5 Bounds and culling

Render-only vertex scaling is insufficient. Unscaled bounds can cause:

- premature frustum culling;
- missing attachment geometry at screen edges;
- incorrect impostor capture extents;
- bad occlusion decisions;
- incorrect LOD and texture-priority estimates.

For positive uniform scale around pivot `P`, native AABB center `c`, and half-extents `e`:

\[
c' = P + s(c-P)
\]

\[
e' = s e
\]

Use transformed extents for:

- avatar aggregate bounds;
- cloned attachment drawable bounds;
- animated-object bounds;
- spatial-group or bridge extents;
- impostor sizing.

For factors greater than one, enlarging the culling bounds is mandatory for correctness.

---

### 9.6 LOD and pixel area

At fixed distance:

- projected linear size scales approximately by `s`;
- projected area scales approximately by `s²`.

Prefer deriving LOD from transformed bounds. A minimal approximation is:

```cpp
effective_radius     = native_radius * scale;
effective_pixel_area = native_pixel_area * scale * scale;
```

Feed the result into:

- avatar body LOD;
- volume LOD;
- impostor selection;
- texture streaming priority;
- minimum-size render thresholds.

---

### 9.7 Picking and selection

Visible scaled geometry must be picked through the inverse transform:

```cpp
native_start = inverse_outer * world_start;
native_end   = inverse_outer * world_end;
```

Run the existing native-space intersection, then transform the hit back:

```cpp
world_hit = outer * native_hit;
```

Also update broad-phase bounds. Inverse-transforming only the narrow-phase ray is not enough if the enlarged clone was rejected before intersection testing.

Selection outlines and highlights must use the same outer transform as the visible geometry.

---

### 9.8 Motion vectors and velocity

The inspected public branch does not expose an obvious dedicated avatar velocity shader/path equivalent to the main avatar, shadow, and impostor passes. Even so, retaining both current and previous outer transforms is prudent.

Future velocity calculation should use:

```text
current clip  = CurrentProjection × CurrentView × CurrentOuter × geometry
previous clip = PreviousProjection × PreviousView × PreviousOuter × geometry
```

This prevents scale changes from producing temporal smearing when TAA or motion vectors are present.

---

### 9.9 HUD attachments and ancillary effects

World-foot scaling should normally exclude HUD attachments because they are rendered in a different coordinate system and do not have a meaningful world foot pivot.

Name tags, particles, sounds, and lights are separate systems. They will not automatically inherit this geometry transform. Their behavior should be defined explicitly if they are considered part of the clone-scale feature.

---

## 10. Alternatives Compared

### A. `LLDrawInfo::mModelMatrix` alone

**Verdict: incomplete.**

It can cover volume batches but not the system body or eyes, which are rendered through direct avatar methods.

It should also not be replaced with a pointer to a temporary composite matrix because:

- the pointer must remain valid;
- renderer batching and caching use matrix identity;
- the native object matrix remains independently meaningful;
- rigged batches intentionally use a null native model matrix.

---

### B. Post-skin shader scale uniform

**Verdict: mathematically correct but operationally inferior.**

For a body shader, an eye-space implementation could resemble:

```glsl
pos.xyz = foot_eye + scale * (pos.xyz - foot_eye);
```

For rigged volume shaders, it could be applied after the skin/model-view result.

Disadvantages:

- requires every avatar shader variant;
- requires every rigged and static volume material variant;
- requires PBR, alpha, shadow, and other pass variants;
- needs per-pass conversion of the foot pivot;
- still needs CPU ownership plumbing for attachments;
- still requires independent CPU work for bounds and picking.

The model-view approach is more centralized and naturally composes with each pass's current camera or light view.

---

### C. Left-multiply the completed skinning palette

**Verdict: valid for rigged vertices in isolation, but incomplete for the entity.**

Mathematically:

\[
\sum_i w_i(M_{outer}B_i p)
=
M_{outer}\sum_i w_iB_i p
\]

Therefore, left-multiplying every fully completed palette matrix by the same outer transform can uniformly scale rigged output.

However, it still misses:

- non-rigged prim attachments;
- rigid/unweighted avatar geometry;
- static children in animesh linksets;
- bounds and picking.

It also risks interacting badly with viewer-specific palette construction if applied before all bind/pivot corrections are complete. The volume/object outer-transform path would still be required.

---

### D. Scale root or joint transforms

**Verdict: reject.**

This changes skinning inputs and joint spacing instead of scaling the completed geometry. It is the source of the observed body stretching and flicker and cannot reliably propagate to non-rigged attachments because parent joint scale is not the required whole-actor transform.

---

### E. Scale a spatial group or render node

**Verdict: unsuitable as the primary vertex transform.**

One clone can span:

- the avatar draw pool;
- multiple volume/material pools;
- several spatial groups;
- a separate animesh control avatar;
- static and active drawable branches.

Spatial groups can support aggregate culling, but they are not a unified actor transform for all these paths.

---

### F. Shared outer transform with two renderer consumers

**Verdict: recommended.**

Advantages:

- one authoritative scale and pivot;
- no skeleton mutation;
- correct body, rigged, static, PBR, and animesh behavior;
- automatic composition with the active camera/light/probe view;
- one inverse for picking;
- one previous transform for future velocity support;
- minimal impact on ordinary avatars.

---

## 11. Patch Map

| File | Recommended change |
|---|---|
| `llghostavatar.*` | Retain scale UI state; build/update the outer transform; stamp the handle onto cloned attachment descendants |
| `llvoavatar.h/.cpp` | Store/access the client transform; compute foot pivot; update revision; invalidate bounds and impostor |
| `lldrawpoolavatar.h/.cpp` | Add scoped model-view override support; make `getModelView()` honor it |
| `llviewerjointmesh.cpp` | Ideally no skinning algorithm change; it receives the overridden model-view automatically |
| `llviewerobject.h/.cpp` | Add an optional client-only outer-transform owner for cloned volumes |
| `llspatialpartition.h` | Add a compact outer-transform handle to `LLDrawInfo`; include it in relevant batch identity |
| `llvovolume.cpp` | Resolve transform owner during batch construction; add it to compatibility checks and new draw infos |
| `lldrawpool.h/.cpp` | Compose `View × Outer × Model`; extend and invalidate the model-matrix cache |
| `llcontrolavatar.cpp` | Do not scale its skeleton; optionally use `getAttachedAvatar()` as an owner fallback |
| Avatar/object intersection code | Inverse-transform pick rays and transform hit results back |
| Bounds/LOD/impostor code | Use transformed extents and scale-aware projected size |

---

## 12. Recommended Implementation Sequence

### Phase 1 — Visual correctness

1. Add `LLClientOuterTransform`.
2. Compute the foot-pivoted matrix from the same helper used by the overlay.
3. Add the body model-view override.
4. Add a separate volume outer-transform owner.
5. Extend `LLRenderPass::applyModelMatrix()`.
6. Stamp all cloned attachment and animesh linkset objects.
7. Add the transform owner to batch compatibility.
8. Extend the renderer matrix-cache key.

This should produce the first correct body-plus-attachments visual result.

### Phase 2 — Pass consistency

1. Verify body shadow calls are scoped.
2. Audit specialized volume shadow paths.
3. Test PBR, alpha blend, alpha mask, and glow.
4. Test reflection probes and secondary cameras.
5. Test impostor capture and prevent billboard double scaling.

### Phase 3 — Interaction and optimization correctness

1. Transform aggregate and drawable bounds.
2. Correct LOD and pixel-area calculations.
3. Correct broad-phase and narrow-phase picking.
4. Correct selection outlines.
5. Store the previous transform for future velocity support.

---

## 13. Validation Matrix

The following tests are especially diagnostic:

1. Two clones using the same mesh/material but different scale factors. This detects incorrect batch merging and stale matrix-cache keys.
2. Rapidly change scale while walking and turning. The skeleton must never momentarily stretch.
3. Wear rigged clothing with multi-joint weights. Body and garment proportions must remain identical.
4. Test static head, hand, and foot prim attachments. Their size and offset from the foot must scale together.
5. Test multipart attachment linksets. Every child must use the same outer-transform owner.
6. Test attached animesh with static children. Its control skeleton must remain native while the whole object follows the wearer clone's outer scale.
7. Test scale factors `0.25`, `0.5`, `1.0`, `2.0`, and `4.0`.
8. Move large clones across camera-frustum edges. They must not disappear based on unscaled bounds.
9. Compare shadow, reflection, and main-camera size.
10. Cross the impostor transition. There must be no size jump or double scaling.
11. Click near the visible outer silhouette. Picking must match rendered geometry.
12. Render a normal avatar beside the clone. The normal avatar must remain completely unaffected.

A useful invariant for every geometry category is:

```text
entity_path_render(scale)
≈
overlay_path_render(scale)
```

at the same animation frame, position, orientation, and foot pivot.

---

## Final Recommendation

Implement a **client-only, foot-pivoted outer render transform** owned by the ghost avatar.

Do not place the scale into:

- `LLJoint` state;
- attachment-point scales;
- object simulator scales;
- avatar shape state;
- the `LLControlAvatar` skeleton.

Apply the transform through these two routes:

```text
SYSTEM AVATAR BODY / EYES
LLVOAvatar::renderSkinned() and LLVOAvatar::renderRigid()
    → scoped model-view override
    → LLDrawPoolAvatar::getModelView() returns the composed view
```

```text
RIGGED + STATIC + PBR + ANIMESH VOLUMES
LLDrawInfo::mOuterTransform
    → LLRenderPass::applyModelMatrix()
    → View × Outer × ExistingModel
```

Propagate the same transform owner to every cloned attachment descendant. Keep animesh `LLControlAvatar` objects as their native skinning owners. Extend batching and model-matrix cache keys, and update culling bounds, impostors, LOD, picking, and selection.

This is the real deferred-path equivalent of the working overlay's single `gGL.scalef()` operation: **one logical actor-scale transform applied after native deformation, without altering the skeleton.**

---

## References

### Viewer source

- [Alchemy-Machinima repository, `develop`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/tree/develop)
- [`indra/newview/lldrawpoolavatar.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/lldrawpoolavatar.cpp)
- [`indra/newview/lldrawpool.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/lldrawpool.cpp)
- [`indra/newview/llviewerjointmesh.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/llviewerjointmesh.cpp)
- [`indra/newview/llspatialpartition.h`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/llspatialpartition.h)
- [`indra/newview/llvovolume.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/llvovolume.cpp)
- [`indra/newview/llcontrolavatar.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/llcontrolavatar.cpp)
- [`indra/llmath/xform.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/llmath/xform.cpp)
- [`indra/llrender/llrender.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/llrender/llrender.cpp)
- [`avatarSkinV.glsl`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/app_settings/shaders/class1/avatar/avatarSkinV.glsl)
- [`materialV.glsl`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/app_settings/shaders/class1/deferred/materialV.glsl)

### Related upstreams

- [Alchemy Viewer](https://github.com/AlchemyViewer/Alchemy)
- [Second Life official viewer](https://github.com/secondlife/viewer)
- [Firestorm Viewer](https://github.com/FirestormViewer/phoenix-firestorm)

### Transform and skinning references

- [Khronos glTF 2.0 Specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
- [OpenGL `glScale` reference](https://docs.gl/gl2/glScale)
