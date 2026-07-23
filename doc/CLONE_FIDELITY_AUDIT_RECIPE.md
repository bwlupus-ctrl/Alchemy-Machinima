# Clone Fidelity Audit — Implementation Recipe (Codex fresh thread, 2026-07-23)

Compile-oriented HOW for doc/CLONE_FIDELITY_AUDIT_BLUEPRINT.md. Codex session 019f8fba. Implement -> Codex review + adversarial workflow -> 0 must-fix.

Below is the compile-oriented implementation recipe for `develop@0570fc82270`. It follows `doc/CLONE_FIDELITY_AUDIT_BLUEPRINT.md` and preserves the collector’s existing ordering and VB/range dedup semantics.

## 1. Share the attachment/source walk

Recommendation: extract the read-only walk. This is safe if the collector callback retains the existing dedup loop verbatim. It avoids two independently drifting attachment/pass enumerators and lets the late audit inspect exactly the collector’s source domain.

### `llactormover.h`

Add forward declarations:

```cpp
class LLFace;
class LLSpatialGroup;
class LLViewerObject;
```

Add public callback types and walker, immediately before `collectGhostBatches()`:

```cpp
using ghost_rigged_source_cb_t =
    std::function<void(LLVOAvatar* wearer,
                       LLSpatialGroup* group,
                       U32 pass,
                       const LLPointer<LLDrawInfo>& draw_info)>;

using ghost_static_source_cb_t =
    std::function<void(LLVOAvatar* wearer,
                       LLViewerObject* object,
                       LLFace* face)>;

void walkGhostSourceGeometry(
    LLVOAvatar* avatar,
    const ghost_rigged_source_cb_t& rigged_cb,
    const ghost_static_source_cb_t& static_cb);
```

Add `<functional>` if the header does not already include it.

Passing `const LLPointer<LLDrawInfo>&` is important: the late audit copies it into `LiveBatchRecord::mPin`, while the collector still obtains the same raw pointer with `draw_info.get()`.

### `llactormover.cpp`

Move the existing pass array out of `collectGhostBatches()` into file scope, adjacent to the ghost batch helpers:

```cpp
namespace
{
constexpr U32 kRiggedPasses[] =
{
    LLRenderPass::PASS_SIMPLE_RIGGED,
    LLRenderPass::PASS_FULLBRIGHT_RIGGED,
    LLRenderPass::PASS_FULLBRIGHT_SHINY_RIGGED,
    LLRenderPass::PASS_SHINY_RIGGED,
    LLRenderPass::PASS_BUMP_RIGGED,
    LLRenderPass::PASS_MATERIAL_RIGGED,
    LLRenderPass::PASS_MATERIAL_ALPHA_RIGGED,
    LLRenderPass::PASS_MATERIAL_ALPHA_MASK_RIGGED,
    LLRenderPass::PASS_SPECMAP_RIGGED,
    LLRenderPass::PASS_SPECMAP_BLEND_RIGGED,
    LLRenderPass::PASS_SPECMAP_MASK_RIGGED,
    LLRenderPass::PASS_NORMMAP_RIGGED,
    LLRenderPass::PASS_NORMMAP_BLEND_RIGGED,
    LLRenderPass::PASS_NORMMAP_MASK_RIGGED,
    LLRenderPass::PASS_NORMSPEC_RIGGED,
    LLRenderPass::PASS_NORMSPEC_BLEND_RIGGED,
    LLRenderPass::PASS_NORMSPEC_MASK_RIGGED,
    LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE_RIGGED,
    LLRenderPass::PASS_SPECMAP_EMISSIVE_RIGGED,
    LLRenderPass::PASS_NORMMAP_EMISSIVE_RIGGED,
    LLRenderPass::PASS_NORMSPEC_EMISSIVE_RIGGED,
    LLRenderPass::PASS_ALPHA_RIGGED,
    LLRenderPass::PASS_ALPHA_MASK_RIGGED,
    LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK_RIGGED,
    LLRenderPass::PASS_GLTF_PBR_RIGGED,
    LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK_RIGGED,
    LLRenderPass::PASS_GLTF_GLOW_RIGGED,
};
}
```

Do not expose or duplicate this list in the audit. Both early and late enumeration go through `walkGhostSourceGeometry()`.

Implement the walker with the existing code in the same order:

```cpp
void LLActorMover::walkGhostSourceGeometry(
    LLVOAvatar* av,
    const ghost_rigged_source_cb_t& rigged_cb,
    const ghost_static_source_cb_t& static_cb)
{
    if (!av || av->isDead())
    {
        return;
    }

    std::set<LLSpatialGroup*> groups;

    for (const auto& ap_pair : av->mAttachmentPoints)
    {
        LLViewerJointAttachment* ap = ap_pair.second;
        if (!ap || ap->getIsHUDAttachment())
        {
            continue;
        }

        for (const LLPointer<LLViewerObject>& attached : ap->mAttachedObjects)
        {
            std::vector<LLViewerObject*> objs;
            if (attached.notNull())
            {
                objs.push_back(attached.get());
                for (LLViewerObject* child : attached->getChildren())
                {
                    objs.push_back(child);
                }
            }

            for (LLViewerObject* obj : objs)
            {
                if (!obj || obj->isDead() || obj->mDrawable.isNull()
                    || obj->mDrawable->isDead())
                {
                    continue;
                }

                LLDrawable* drawable = obj->mDrawable.get();
                if (LLSpatialGroup* group = drawable->getSpatialGroup())
                {
                    groups.insert(group);
                }

                const S32 count = drawable->getNumFaces();
                for (S32 face_index = 0; face_index < count; ++face_index)
                {
                    LLFace* face = drawable->getFace(face_index);
                    if (!face || face->isState(LLFace::RIGGED)
                        || !face->getVertexBuffer()
                        || face->getIndicesCount() == 0)
                    {
                        continue;
                    }

                    static_cb(av, obj, face);
                }
            }
        }
    }

    for (LLSpatialGroup* group : groups)
    {
        for (U32 pass : kRiggedPasses)
        {
            auto found = group->mDrawMap.find(pass);
            if (found == group->mDrawMap.end())
            {
                continue;
            }

            for (const LLPointer<LLDrawInfo>& draw_info : found->second)
            {
                LLDrawInfo* di = draw_info.get();
                if (!di || di->mAvatar.isNull()
                    || di->mSkinInfo.isNull()
                    || di->mVertexBuffer.isNull())
                {
                    continue;
                }

                rigged_cb(av, group, pass, draw_info);
            }
        }
    }
}
```

The callbacks are assumed valid; pass no-op lambdas when one class is not needed.

### Replace only the per-avatar body in `collectGhostBatches()`

Keep wanted-source construction and map erasure unchanged. For each `av`:

```cpp
std::vector<GhostBatch>& bucket = mGhostBatches[av->getID()];
std::vector<GhostStaticFace>& faces = mGhostStaticFaces[av->getID()];

walkGhostSourceGeometry(
    av,
    [&](LLVOAvatar*,
        LLSpatialGroup*,
        U32 pass,
        const LLPointer<LLDrawInfo>& draw_info)
    {
        LLDrawInfo* di = draw_info.get();

        bool dup = false;
        for (const GhostBatch& batch : bucket)
        {
            if (batch.mInfo->mVertexBuffer.get() == di->mVertexBuffer.get()
                && batch.mInfo->mStart == di->mStart
                && batch.mInfo->mEnd == di->mEnd
                && batch.mInfo->mOffset == di->mOffset)
            {
                dup = true;
                break;
            }
        }

        if (!dup)
        {
            bucket.push_back({ di, pass });
        }
    },
    [&](LLVOAvatar*, LLViewerObject* obj, LLFace* face)
    {
        GhostStaticFace gf;
        gf.mFace = face;
        gf.mObjectId = obj->getID();

        const LLTextureEntry* te = face->getTextureEntry();
        LLGLTFMaterial* gmat = te ? te->getGLTFRenderMaterial() : nullptr;
        if (gmat)
        {
            gf.mDoubleSided = gmat->mDoubleSided;
            if (gmat->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_MASK)
            {
                gf.mAlphaKind = 1;
                gf.mCutoff = gmat->mAlphaCutoff;
            }
            else if (gmat->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_BLEND)
            {
                gf.mAlphaKind = 2;
            }
        }
        else if (te)
        {
            const LLMaterial* mat = te->getMaterialParams().get();
            if (mat &&
                mat->getDiffuseAlphaMode() ==
                    LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
            {
                gf.mAlphaKind = 1;
                gf.mCutoff =
                    mat->getAlphaMaskCutoff() * (1.f / 255.f);
            }
            else if (face->getPoolType() == LLDrawPool::POOL_ALPHA
                     || te->getColor().mV[VW] < 0.999f)
            {
                gf.mAlphaKind = 2;
            }
        }

        faces.push_back(gf);
    });
```

Do not “improve” the dedup key by adding pass, `mCount`, or VB type. The existing collector deliberately dedups on:

```cpp
mVertexBuffer.get(), mStart, mEnd, mOffset
```

The audit separately reports cross-pass collisions as `DROP_DEDUP`.

Before landing, compare collector output from an instrumented frame before/after extraction: sequence of wearer, `mInfo`, `mPass`, VB, start/end/offset and static face/object IDs must match exactly.

## 2. Singleton and two-timepoint snapshots

Create `llclonefidelityaudit.h/.cpp`.

### Public API

```cpp
class LLCloneFidelityAudit
{
public:
    static LLCloneFidelityAudit& instance();

    bool arm(const std::vector<LLUUID>& instance_ids);
    bool isArmed() const;

    void beginEarlyCapture(U32 frame);
    void captureEarlyRigged(
        LLVOAvatar* wearer,
        LLSpatialGroup* group,
        U32 pass,
        const LLPointer<LLDrawInfo>& draw_info,
        bool retained_by_collector);

    void captureEarlyStatic(
        LLVOAvatar* wearer,
        LLViewerObject* object,
        LLFace* face,
        const LLActorMover::GhostStaticFace* harvested);

    void endEarlyCapture();
    void runLateAuditIfPending();

private:
    LLCloneFidelityAudit() = default;
};
```

`arm()` stores instance UUIDs, clears previous report state, and sets a pending state. It does not walk geometry.

### Snapshot ownership rule

Snapshots contain values and numeric addresses only:

```cpp
struct TextureRef
{
    enum class Kind : U8
    {
        REAL,
        WHITE,
        FLAT_NORMAL,
        NULL_GAP,
        MEDIA_OVERRIDE,
        INHERITED
    };

    LLUUID mUUID;
    const LLViewerTexture* mPtr = nullptr; // logged/compared only
    Kind mKind = Kind::REAL;
};

struct BatchSnapshot
{
    const LLDrawInfo* mAddress = nullptr;
    const LLSpatialGroup* mGroupAddress = nullptr;
    const LLVertexBuffer* mVertexBufferAddress = nullptr;

    LLUUID mWearerId;
    LLUUID mRootId;
    LLUUID mObjectId;
    LLUUID mDrawingAvatarId;
    LLUUID mMaterialId;

    U32 mPass = 0;
    U16 mStart = 0;
    U16 mEnd = 0;
    U32 mCount = 0;
    U32 mOffset = 0;
    U32 mVertexTypeMask = 0;
    U64 mSkinHash = 0;

    U64 mTextureMatrixHash = 0;
    U64 mModelMatrixHash = 0;
    bool mHasTextureMatrix = false;
    bool mHasModelMatrix = false;
    bool mHasTextureIndex = false;

    U32 mShaderMask = 0;
    F32 mAlphaMaskCutoff = 0.f;
    LLRender::eBlendFactor mBlendSrc;
    LLRender::eBlendFactor mBlendDst;
    U8 mDiffuseAlphaMode = 0;
    U8 mBump = 0;
    U8 mShiny = 0;
    bool mFullbright = false;
    bool mHasGlow = false;

    MaterialSnapshot mMaterial;
};
```

No `LLPointer` belongs in `BatchSnapshot`, `MaterialSnapshot`, `StaticSnapshot`, or a structural key.

Capture current fields using the real members:

```cpp
snapshot.mAddress = di;
snapshot.mGroupAddress = group;
snapshot.mVertexBufferAddress = di->mVertexBuffer.get();
snapshot.mStart = di->mStart;
snapshot.mEnd = di->mEnd;
snapshot.mCount = di->mCount;
snapshot.mOffset = di->mOffset;
snapshot.mVertexTypeMask = di->mVertexBuffer->getTypeMask();
snapshot.mHasTextureIndex =
    di->mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXTURE_INDEX);
snapshot.mDrawingAvatarId =
    di->mAvatar.notNull() ? di->mAvatar->getID() : LLUUID::null;
snapshot.mSkinHash =
    di->mSkinInfo.notNull() ? di->mSkinInfo->mHash : 0;
snapshot.mMaterialId = di->mMaterialID;
snapshot.mShaderMask = di->mShaderMask;
snapshot.mFullbright = di->mFullbright;
snapshot.mHasGlow = di->mHasGlow;
snapshot.mBump = di->mBump;
snapshot.mShiny = di->mShiny;
snapshot.mDiffuseAlphaMode = di->mDiffuseAlphaMode;
snapshot.mAlphaMaskCutoff = di->mAlphaMaskCutoff;
snapshot.mBlendSrc = di->mBlendFuncSrc;
snapshot.mBlendDst = di->mBlendFuncDst;
```

For normal/specular:

```cpp
textureRef(di->mNormalMap.get(), TextureRef::Kind::REAL);
textureRef(di->mSpecularMap.get(), TextureRef::Kind::REAL);
```

### Hook the early capture

After `bucket` and `faces` are obtained, notify the audit through the same walker callbacks. The cleanest arrangement is for the collector callback to determine `dup`, append if appropriate, then call:

```cpp
LLCloneFidelityAudit::instance().captureEarlyRigged(
    av, group, pass, draw_info, !dup);
```

This records both:

- raw source truth: every eligible draw-map entry/pass;
- collector truth: whether the current VB/range dedup retained it.

For static faces, construct `GhostStaticFace gf`, push it, then call:

```cpp
LLCloneFidelityAudit::instance().captureEarlyStatic(
    av, obj, face, &faces.back());
```

Bracket the wanted-avatar loop:

```cpp
LLCloneFidelityAudit::instance().beginEarlyCapture(
    LLFrameTimer::getFrameCount());

// existing wanted loop

LLCloneFidelityAudit::instance().endEarlyCapture();
```

These methods must return immediately when not armed.

### Late records and pinning

Use:

```cpp
struct LiveBatchRecord
{
    LLPointer<LLDrawInfo> mPin;
    LLVOAvatar* mWearer = nullptr;
    LLSpatialGroup* mGroup = nullptr;
    U32 mPass = 0;
    BatchSnapshot mValues;
};

using live_by_address_t =
    std::unordered_map<const LLDrawInfo*, std::vector<LiveBatchRecord>>;

using structural_map_t =
    std::unordered_multimap<StructuralKey, const LiveBatchRecord*,
                            StructuralKeyHash>;
```

A vector is safer than one record per address because the same `LLDrawInfo` can appear in multiple passes, notably base plus GLTF glow.

Build both maps in `runLateAuditIfPending()` with `walkGhostSourceGeometry()`. Copy `draw_info` into `mPin` before taking any other late references:

```cpp
LiveBatchRecord live;
live.mPin = draw_info;
live.mWearer = wearer;
live.mGroup = group;
live.mPass = pass;
live.mValues = snapshotBatch(...);
live_by_address[draw_info.get()].push_back(std::move(live));
```

Build the structural multimap only after `live_by_address` is complete so its pointers are stable, or store indices instead of pointers.

### Mandatory staleness rule

```cpp
auto found = live_by_address.find(early.mAddress);
if (found == live_by_address.end())
{
    // CLONE_ONLY_STALE.
    // Log and compare only early snapshot values.
    // Do not access early.mAddress->anything.
}
else
{
    // Safe: found records own LLPointer<LLDrawInfo>.
    // Select same-pass candidate, then compare snapshots/resolvers.
}
```

Never write:

```cpp
if (early.mAddress && early.mAddress->mVertexBuffer ...)
```

Address equality is only a membership lookup. If the address exists but early structural fields differ, report `ADDRESS_REUSED_OR_MUTATED`/`EARLY_TO_LATE_MUTATION`. Exact-address, exact-field ABA remains unprovable without an `LLDrawInfo` generation counter.

### Late call site

In `llviewerdisplay.cpp`, include `llclonefidelityaudit.h` and place:

```cpp
LLCloneFidelityAudit::instance().runLateAuditIfPending();
```

immediately before:

```cpp
if (!for_snapshot)
{
    render_ui();
```

At that location `LLSpatialGroup::sNoDelete` remains true and `gPipeline.clearReferences()` has not run.

If `for_snapshot` is true, do not consume the pending audit; wait for the next complete non-snapshot frame.

## 3. Pure binding resolvers

Use value inputs only:

```cpp
enum class GhostSweep : U8
{
    BASE,
    GLOW
};

ResolvedSurfaceBinding resolveGhostBinding(
    const BatchSnapshot& batch,
    GhostSweep sweep,
    S32 slot);

ResolvedSurfaceBinding resolveStockBinding(
    const BatchSnapshot& live,
    U32 pass,
    S32 slot);
```

For static surfaces, either overload both functions or use a tagged `SurfaceSnapshot`:

```cpp
ResolvedSurfaceBinding resolveGhostBinding(
    const StaticSnapshot& face);

ResolvedSurfaceBinding resolveStockBinding(
    const StaticSnapshot& face,
    const BatchSnapshot* live_draw_info);
```

No resolver may call `bind()`, `bindFast()`, `bindBumpMap()`, shader functions, texture-unit functions, or create bump images.

Suggested result:

```cpp
struct ResolvedSurfaceBinding
{
    TextureRef mColor;
    TextureRef mNormal;
    TextureRef mSpecularOrORM;
    TextureRef mEmissive;

    LLColor4 mBaseColor = LLColor4::white;
    LLColor3 mEmissiveColor;
    LLVector4 mSpecColor;
    F32 mMetallic = 1.f;
    F32 mRoughness = 1.f;
    F32 mEnvironment = 0.f;
    F32 mAlphaCutoff = 0.f;
    F32 mFullbright = 0.f;

    std::array<LLGLTFMaterial::TextureTransform,
               LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT> mTransforms;

    bool mDoubleSided = false;
    U8 mBump = 0;
    std::string mReason;
};
```

### Texture references and UUIDs

For an actual viewer texture:

```cpp
TextureRef textureRef(LLViewerTexture* texture, TextureRef::Kind kind)
{
    TextureRef result;
    result.mPtr = texture;
    result.mKind = kind;
    result.mUUID = texture ? texture->getID() : LLUUID::null;
    return result;
}
```

Also snapshot `texture->getTextureListType()` when the pointer is non-null.

For authored GLTF slot UUIDs use:

```cpp
material->mTextureId[
    LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR];

material->mTextureId[
    LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL];

material->mTextureId[
    LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS];

material->mTextureId[
    LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE];
```

For the fetched/effective texture use:

```cpp
material->mBaseColorTexture.get()->getID();
material->mNormalTexture.get()->getID();
material->mMetallicRoughnessTexture.get()->getID();
material->mEmissiveTexture.get()->getID();
```

Capture both authored UUID and effective pointer/UUID. Local-texture substitution can make them differ legitimately.

Default references should use the actual singleton addresses/UUIDs:

```cpp
LLViewerFetchedTexture::sWhiteImagep.get()
LLViewerFetchedTexture::sFlatNormalImagep.get()
```

### Scalar GLTF

Material access:

```cpp
LLFetchedGLTFMaterial* mat = di->mGLTFMaterial.get();

mat->getHash();
mat->isFetching();
mat->isLoaded();
mat->mBaseColor;
mat->mEmissiveColor;
mat->mMetallicFactor;
mat->mRoughnessFactor;
mat->mAlphaMode;
mat->mAlphaCutoff;
mat->mDoubleSided;
mat->mTextureTransform[...];
mat->mBaseColorTexture;
mat->mNormalTexture;
mat->mMetallicRoughnessTexture;
mat->mEmissiveTexture;
```

Stock base and emissive both honor the scalar media override because `pushGLTFBatch()` calls:

```cpp
mat->bind(params.mTexture);
```

Therefore:

```cpp
stock.color =
    di->mTexture.notNull()
        ? mediaOverride(di->mTexture.get())
        : realOrWhite(mat->mBaseColorTexture.get());

stock.emissive =
    di->mTexture.notNull()
        ? mediaOverride(di->mTexture.get())
        : realOrWhite(mat->mEmissiveTexture.get());
```

Ghost BASE matches `ghost_batch_texture()`:

```cpp
ghost.color =
    di->mTexture.notNull()
        ? mediaOverride(di->mTexture.get())
        : realOrWhite(mat->mBaseColorTexture.get());
```

Ghost GLOW mirrors `ghost_batch_slot_texture(..., emissive=true)` semantics and does not use `di->mTexture`:

```cpp
ghost.emissive = realOrWhite(mat->mEmissiveTexture.get());
```

Thus scalar GLTF media plus glow is a concrete `BIND_DIFF map=emissive`.

Normal fallback must mirror stock’s discard check:

```cpp
mat->mNormalTexture.notNull()
    && mat->mNormalTexture->getDiscardLevel() <= 4
```

Otherwise return `FLAT_NORMAL`. ORM null becomes `WHITE`.

### Indexed GLTF

Use `di->mGLTFMaterialList[s].get()`. Preserve every position, including null gaps.

Cap comparison at:

```cpp
LLGLSLShader::sIndexedGLTFChannels
```

Report list length above the cap separately. Do not silently truncate the source-data verdict.

A null material slot is `NULL_GAP`, not white. Stock sets `min_alpha[s] = -1.f` and binds nothing because that vertex slot is expected never to be sampled.

For non-null slots:

```cpp
base     = mBaseColorTexture        or WHITE;
normal   = mNormalTexture usable    or FLAT_NORMAL;
orm      = mMetallicRoughnessTexture or WHITE;
emissive = mEmissiveTexture         or WHITE;
```

No scalar `di->mTexture` media override applies to the indexed path.

Snapshot and compare all four:

```cpp
mat->mTextureTransform[0..GLTF_TEXTURE_INFO_COUNT - 1]
```

plus `mBaseColor`, `mEmissiveColor`, metallic, roughness, alpha mode/cutoff, double-sided and `getHash()`.

### Scalar legacy/simple/fullbright/alpha

Effective color is:

```cpp
di->mTexture.get()
```

Do not normalize null to white unless the exact stock path binds white. For ordinary scalar paths, null is an actual null/unverifiable binding state.

Compare:

```cpp
di->mTextureMatrix
di->mAlphaMaskCutoff
di->mDiffuseAlphaMode
di->mFullbright
di->mBlendFuncSrc
di->mBlendFuncDst
```

Normal/specular inputs are:

```cpp
di->mNormalMap.get()
di->mSpecularMap.get()
di->mNormalMapMatrix
di->mSpecularMapMatrix
di->mSpecColor
di->mEnvIntensity
```

### Bump

Record:

```cpp
di->mTexture.get()
di->mBump
```

The diffuse texture is also the source from which the stock bump image may be derived. Report it as a bump-generation input. Do not invoke the bump-image generator and do not pretend the generated image UUID is knowable from this resolver.

### Indexed legacy material

Use:

```cpp
const LLDrawInfo::MaterialSlot& material =
    di->mMaterialSlotList[s];

material.mDiffuse
material.mNormalMap
material.mSpecularMap
material.mSpecColor
material.mEnvIntensity
material.mAlphaMaskCutoff
material.mFullbright
```

Stock defaults are:

```cpp
mDiffuse     -> sWhiteImagep
mNormalMap   -> sFlatNormalImagep
mSpecularMap -> sWhiteImagep
```

The stock channel cap is currently:

```cpp
LLGLSLShader::sIndexedGLTFChannels
```

for `mMaterialSlotList`, not `sIndexedTextureChannels`, as shown by the indexed `POOL_MATERIALS` implementation.

Ghost color must mirror:

```cpp
ghost_batch_slot_texture(di, s, emissive, cutoff, factor)
```

For legacy material slots, that resolves `mDiffuse`, with no helper-side white substitution; the draw code later binds white when the returned pointer is null. Model that final fallback as `WHITE`.

Even though the overlay does not reproduce material lighting, compare normal/specular/scalars. Equal inputs with different rendering becomes `MATCH_BINDING_APPROXIMATION_ONLY`, not missing data.

### Generic `mTextureList`

This non-material indexed path is bounded by:

```cpp
LLGLSLShader::sIndexedTextureChannels
```

Stock loops over `mTextureList` and only binds non-null entries:

```cpp
if (draw->mTextureList[i].notNull())
{
    gGL.getTexUnit(i)->bindFast(draw->mTextureList[i]);
}
```

Therefore null means `INHERITED`: the unit is left unchanged. It is not white and not a GLTF-style null gap.

Ghost uses:

```cpp
di->mTextureList[s].get()
```

through `ghost_batch_slot_texture()`. If the ghost later skips that slot, preserve that distinction rather than manufacturing white.

### Static

Capture actual face members/accessors:

```cpp
face
face->getViewerObject()
face->getTEOffset()
face->getVertexBuffer()
face->getGeomStart()
face->getGeomCount()
face->getIndicesStart()
face->getIndicesCount()
face->getTexture()
face->getTextureEntry()
face->getTextureIndex()
face->getPoolType()
face->getRenderMatrix()
face->mTextureMatrix
face->mDrawInfo
```

Ghost color exactly mirrors the existing static code:

```cpp
const LLTextureEntry* te = face->getTextureEntry();
LLGLTFMaterial* gmat = te ? te->getGLTFRenderMaterial() : nullptr;

const LLFetchedGLTFMaterial* fetched =
    dynamic_cast<const LLFetchedGLTFMaterial*>(gmat);

LLViewerTexture* ghost_color =
    fetched && fetched->mBaseColorTexture.notNull()
        ? fetched->mBaseColorTexture.get()
        : face->getTexture();
```

If both are null, the draw binds `LLViewerFetchedTexture::sWhiteImagep`.

Stock must begin with the live:

```cpp
LLDrawInfo* di = face->mDrawInfo;
U8 slot = face->getTextureIndex();
```

Then use the same stock resolver as rigged data:

- `di->mGLTFMaterialList.size() > 1`: indexed GLTF slot.
- `di->mMaterialSlotList.size() > 1`: indexed legacy slot.
- `di->mTextureList.size() > 1`: generic indexed slot.
- otherwise scalar GLTF/legacy using `di->mGLTFMaterial` and `di->mTexture`.

If `face->mDrawInfo` is absent at late time, report `UNVERIFIABLE`; do not fall back to `face->getTexture()` and call that the stock result.

For a face slot beyond the applicable list or shader channel cap, report `SOURCE_ONLY_DROPPED`/over-cap rather than indexing it.

## 4. Matching and verdict folding

Structural key:

```cpp
struct StructuralKey
{
    LLUUID mWearerId;
    LLUUID mRootId;
    const LLSpatialGroup* mGroup = nullptr;
    U32 mPass = 0;
    const LLVertexBuffer* mVB = nullptr;
    U16 mStart = 0;
    U16 mEnd = 0;
    U32 mCount = 0;
    U32 mOffset = 0;

    bool operator==(const StructuralKey&) const;
};
```

Matching order:

1. Same address and pass, with matching structural snapshot: `IDENTITY_MATCH`.
2. Same address but different pass: `PASS_RECLASSIFIED`.
3. Same address but changed structure: `EARLY_TO_LATE_MUTATION` plus address-reuse note.
4. Unique structural candidate: `REBUILT_EQUIVALENT`.
5. Multiple structural candidates: `AMBIGUOUS_MATCH`.
6. Early retained record absent late: `CLONE_ONLY_STALE`.
7. Unmatched late source record: `SOURCE_ONLY_DROPPED`.

For every early raw record rejected by collector dedup, locate the retained early record with the same VB/start/end/offset. If passes differ, emit:

```text
DROP_DEDUP conflicting_passes=<retained>,<dropped>
```

### Matrix hash

Use a plain bytewise FNV-1a over the 16 float values:

```cpp
U64 hashMatrix(const LLMatrix4* matrix)
{
    if (!matrix)
    {
        return 0;
    }

    constexpr U64 FNV_OFFSET = 14695981039346656037ULL;
    constexpr U64 FNV_PRIME  = 1099511628211ULL;

    U64 hash = FNV_OFFSET;
    for (S32 row = 0; row < 4; ++row)
    {
        for (S32 column = 0; column < 4; ++column)
        {
            U32 bits = 0;
            static_assert(sizeof(bits) == sizeof(F32),
                          "F32 must be 32 bits");
            std::memcpy(&bits,
                        &matrix->mMatrix[row][column],
                        sizeof(bits));

            for (S32 byte = 0; byte < 4; ++byte)
            {
                hash ^= (bits >> (byte * 8)) & 0xffU;
                hash *= FNV_PRIME;
            }
        }
    }
    return hash;
}
```

Hash values, never the matrix pointer alone. Keep a separate presence boolean because null and an unlikely all-zero/hash-zero condition are semantically different.

### Fold order

Accumulate flags; do not force every surface into one exclusive enum. For the chat headline use this precedence:

1. `CLONE_ONLY_STALE`, `SOURCE_ONLY_DROPPED`, `DROP_DEDUP`,
   `PASS_RECLASSIFIED`, `AMBIGUOUS`
2. `EARLY_TO_LATE_MUTATION`
3. `MATCH_DATA_BINDING_DIFF`
4. `MATCH_BINDING_APPROXIMATION_ONLY`
5. `MATCH_DATA_AND_BINDING`
6. `UNVERIFIABLE`

Final interpretation:

- Membership/material/binding divergence: fixable collection or resolution bug.
- Inputs equal but factors/transforms/path approximated: fixable ghost draw-path approximation.
- All harvested values and effective bindings equal: data identical; remaining difference is render state/order/lighting. Do not claim pixel identity or automatically prescribe locking.

## 5. `/clonefidelity`

### Command handler

Add:

```cpp
#include "alghoststudio.h"
#include "llclonefidelityaudit.h"
```

Use the existing literal-command pattern around `/ghosttest`:

```cpp
else if (cmd == "/clonefidelity")
{
    std::string argument;
    input >> argument;

    std::vector<LLUUID> targets;
    if (!LLCloneFidelityAudit::instance().selectTargets(argument, targets))
    {
        clonefidelity_report(
            "No enabled Clone instance matched the request.");
        return true;
    }

    LLCloneFidelityAudit::instance().arm(targets);
    return true;
}
```

Because `parseCommand()` lowercases the entire input before tokenization, UUID parsing is unaffected.

Add a local chat helper analogous to `objpath_report()`:

```cpp
static void clonefidelity_report(const std::string& message)
{
    if (LLFloaterIMNearbyChat* nearby_chat =
            LLFloaterReg::getTypedInstance<LLFloaterIMNearbyChat>(
                "nearby_chat"))
    {
        LLChat chat;
        chat.mText = "[Clone Fidelity] " + message;
        chat.mSourceType = CHAT_SOURCE_SYSTEM;
        chat.mChatType = CHAT_TYPE_NORMAL;
        nearby_chat->addMessage(chat);
    }
}
```

Prefer exposing a public static report helper from the audit if both the command and late report use it.

### Selection

Implement:

```cpp
bool LLCloneFidelityAudit::selectTargets(
    const std::string& argument,
    std::vector<LLUUID>& out) const;
```

Eligibility is always:

```cpp
instance.mEnabled
&& instance.mStyle == GHOST_STYLE_CLONE
&& ALGhostStudio::instance().getShowAll()
```

Selection order:

1. `"all"`: every eligible clone.
2. Non-empty argument: parse `LLUUID requested(argument)` and match `Instance::mId`.
3. `ALGhostStudio::instance().getSelected()`, if it names an eligible clone.
4. Nearest eligible clone to camera.
5. None.

The instance list is:

```cpp
ALGhostStudio::instance().getInstances()
```

Selected ID:

```cpp
ALGhostStudio::instance().getSelected()
```

Explicit lookup can use:

```cpp
ALGhostStudio::instance().getInstance(id)
```

For nearest, compare squared agent-space distance between the camera origin and instance foot:

```cpp
LLVector3 foot =
    gAgent.getPosAgentFromGlobal(instance.mFootGlobal);

LLVector3 delta =
    foot - LLViewerCamera::getInstance()->getOrigin();

F32 distance_squared = delta.lengthSquared();
```

Do not use source-avatar distance; selection concerns the visible clone placement.

Resolve source through the same studio path:

```cpp
LLVOAvatar* source =
    LLDirectorCast::instance().resolve(instance.mSource);
```

A null source UUID means the local avatar under existing studio semantics.

Arm chat:

```text
[Clone Fidelity] Audit armed for next complete render frame: <short-id> source=<name-or-uuid>.
```

For `all`:

```text
[Clone Fidelity] Audit armed for next complete render frame: <N> Clone instances.
```

Multiple selected instances sharing a source should produce one source walk/report dataset, then attach per-instance metadata.

### CMake

In `indra/newview/CMakeLists.txt`, add alphabetically to the source list:

```cmake
llclonefidelityaudit.cpp
```

and to the header list:

```cmake
llclonefidelityaudit.h
```

No `settings.xml` entry is required. `/clonefidelity` is a fixed diagnostic command gated by the existing `AlchemyChatCommandEnable`. Add a setting only if product requirements demand a configurable command alias; the blueprint’s “opt cmd name” is not necessary for compilation or operation.

### Output formats

Concise success chat:

```text
[Clone Fidelity] DATA/BINDINGS PASS: 147 surfaces matched. This does not assert pixel-identical rendering.
```

Diff chat:

```text
[Clone Fidelity] Clone 8f2 source Alice, frame 12345 / Attachments 6 rigged 139 static 8 / Exact 141 binding-diffs 2 mutated 1 dropped 3 / stale 0 approx-only 4 / DIFF Headband: bind=2 dropped=1
```

Stable per-surface log:

```cpp
LL_INFOS("CloneFidelity")
    << "instance=" << shortUUID(instance_id)
    << " source=" << source_id
    << " attachment=" << attachment_name
    << " root=" << root_id
    << " object=" << object_id
    << " kind=" << kind
    << " status=" << statusString(flags)
    << " pass=" << passName(pass)
    << " early_di=" << pointerString(early_address)
    << " late_di=" << pointerString(late_address)
    << " late_membership=" << boolString(late_membership)
    << " match=" << match_kind
    << " vb=" << pointerString(vb_address)
    << " range=" << start << ":" << end
    << ":" << count << ":" << offset
    << " slot=" << slot
    << " material_id=" << material_id
    << " material_hash=" << material_hash
    << " source_color=" << textureRefString(source_color)
    << " ghost_color=" << textureRefString(ghost_color)
    << " source_xform=" << source_transform_hash
    << " ghost_xform=" << ghost_transform_hash
    << " texture_index=" << texture_index
    << LL_ENDL;
```

For stale records, `late_di=0`, `late_membership=false`, and every other field comes from the early value snapshot.

Log binding differences separately or append:

```text
diff_map=color|normal|specular_or_orm|emissive|factor|cutoff|transform|double_sided
reason=<stable-token>
```

Avoid prose-only reasons; stable tokens make logs scriptable.

## 6. Ranked pitfalls

1. Dereferencing an early raw `LLDrawInfo*` after late membership fails. This is the primary correctness and crash hazard.
2. Changing collector dedup or ordering during extraction. Preserve VB/start/end/offset and first-pass-wins exactly.
3. Using one late record per address. A draw info can occupy both base and glow passes; store address → vector.
4. Treating scalar GLTF `mTexture` as ordinary diffuse. It is the media override and stock `bind()` applies it to base and emissive.
5. Treating indexed GLTF null gaps as white. They are deliberately unsampled gaps with `min_alpha=-1`.
6. Treating generic `mTextureList` nulls as white. Stock leaves the corresponding texture unit unchanged; classify as `INHERITED`.
7. Using `sIndexedTextureChannels` for indexed GLTF or indexed legacy materials. Both current material paths use `sIndexedGLTFChannels`; generic `mTextureList` uses `sIndexedTextureChannels`.
8. Reading only `LLGLTFMaterial::mTextureId`. Also record fetched texture pointer/`getID()` because local substitution and loading state can differ.
9. Comparing matrix pointers rather than their 16 float values. Texture animation can mutate a matrix in place.
10. Resolving static stock color from `face->getTexture()` alone. The decisive stock path is `face->mDrawInfo` plus `face->getTextureIndex()`.
11. Calling material/texture bind helpers from the audit. They mutate GL state and invalidate the diagnostic’s read-only guarantee.
12. Consuming the audit during a snapshot frame or after `render_ui()`/`clearReferences()`. Run immediately before non-snapshot `render_ui()`.
13. Claiming a frozen clone freezes materials. Current `POSE_FROZEN` freezes palettes and attachment matrices; material pointers remain live.
14. Reporting UUID equality as pixel fidelity. Factors, KHR transforms, cutoff, blending, shader behavior, lighting, ordering, and render phase remain separate.
15. Letting `all` redo identical source walks. Group selected instances by resolved source avatar and audit each source once.

Codex session ID: 019f8fba-6404-78b1-9745-4cf0ddfa6ff2
Resume in Codex: codex resume 019f8fba-6404-78b1-9745-4cf0ddfa6ff2
