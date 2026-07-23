/**
 * @file llclonefidelityaudit.cpp
 * @brief [CloneFidelity] see llclonefidelityaudit.h. Read-only, two-timepoint
 *        source-vs-clone data/binding audit.
 */

#include "llviewerprecompiledheaders.h"

#include "llclonefidelityaudit.h"

#include "llactormover.h"           // walkGhostSourceGeometry + GhostStaticFace
#include "alghoststudio.h"
#include "lldirectorcast.h"
#include "llvoavatar.h"
#include "llviewerobject.h"
#include "llface.h"
#include "llspatialpartition.h"     // LLDrawInfo
#include "llfetchedgltfmaterial.h"
#include "llgltfmaterial.h"
#include "llglslshader.h"           // sIndexedGLTFChannels / sIndexedTextureChannels
#include "llviewertexture.h"
#include "llviewercamera.h"
#include "llagent.h"
#include "lltextureentry.h"
#include "llrender.h"
#include "llmatrix4a.h"
#include "m4math.h"
#include "llframetimer.h"

#include "llfloaterimnearbychat.h"
#include "llfloaterreg.h"
#include "llchat.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_map>

// ---------------------------------------------------------------------------
// file-local helpers
// ---------------------------------------------------------------------------
namespace
{

// EGhostStyle::GHOST_STYLE_CLONE is file-local to llactormover.cpp; mirror its
// value here (Instance::mStyle stores the same ids).
constexpr S32 kGhostStyleClone = 1;

std::string shortId(const LLUUID& id)
{
    const std::string s = id.asString();
    return s.substr(0, 8);
}

U64 fnvBytes(const void* data, size_t len, U64 seed = 14695981039346656037ULL)
{
    constexpr U64 FNV_PRIME = 1099511628211ULL;
    const U8* p = static_cast<const U8*>(data);
    U64 hash = seed;
    for (size_t i = 0; i < len; ++i)
    {
        hash ^= p[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

U64 hashMatrix(const LLMatrix4* m)
{
    if (!m)
    {
        return 0;
    }
    // FNV-1a over the 16 float values (value hash: a matrix can mutate in place).
    return fnvBytes(m->mMatrix, sizeof(F32) * 16);
}

U64 hashMatrixVal(const LLMatrix4& m)
{
    return fnvBytes(m.mMatrix, sizeof(F32) * 16);
}

U64 hashTransform(const LLGLTFMaterial::TextureTransform& xform)
{
    LLGLTFMaterial::TextureTransform::Pack packed;
    xform.getPacked(packed);
    return fnvBytes(&packed, sizeof(packed));
}

GhostAuditTextureRef refReal(LLViewerTexture* t)
{
    GhostAuditTextureRef r;
    if (t)
    {
        r.mPtr = t;
        r.mUUID = t->getID();
        r.mKind = GhostAuditTextureRef::Kind::REAL;
    }
    else
    {
        r.mKind = GhostAuditTextureRef::Kind::NONE;
    }
    return r;
}

GhostAuditTextureRef refMedia(LLViewerTexture* t)
{
    GhostAuditTextureRef r = refReal(t);
    r.mKind = GhostAuditTextureRef::Kind::MEDIA_OVERRIDE;
    return r;
}

GhostAuditTextureRef refWhite()
{
    GhostAuditTextureRef r;
    r.mKind = GhostAuditTextureRef::Kind::WHITE;
    LLViewerTexture* w = LLViewerFetchedTexture::sWhiteImagep.get();
    if (w) { r.mPtr = w; r.mUUID = w->getID(); }
    return r;
}

GhostAuditTextureRef refFlatNormal()
{
    GhostAuditTextureRef r;
    r.mKind = GhostAuditTextureRef::Kind::FLAT_NORMAL;
    LLViewerTexture* n = LLViewerFetchedTexture::sFlatNormalImagep.get();
    if (n) { r.mPtr = n; r.mUUID = n->getID(); }
    return r;
}

GhostAuditTextureRef refGap()
{
    GhostAuditTextureRef r;
    r.mKind = GhostAuditTextureRef::Kind::NULL_GAP;
    return r;
}

GhostAuditTextureRef refInherited()
{
    GhostAuditTextureRef r;
    r.mKind = GhostAuditTextureRef::Kind::INHERITED;
    return r;
}

std::string refString(const GhostAuditTextureRef& r)
{
    const char* k = "none";
    switch (r.mKind)
    {
    case GhostAuditTextureRef::Kind::REAL:           k = "real"; break;
    case GhostAuditTextureRef::Kind::WHITE:          k = "white"; break;
    case GhostAuditTextureRef::Kind::FLAT_NORMAL:    k = "flatnorm"; break;
    case GhostAuditTextureRef::Kind::NULL_GAP:       k = "gap"; break;
    case GhostAuditTextureRef::Kind::MEDIA_OVERRIDE: k = "media"; break;
    case GhostAuditTextureRef::Kind::INHERITED:      k = "inherited"; break;
    case GhostAuditTextureRef::Kind::NONE:           k = "none"; break;
    }
    return std::string(k) + ":" + shortId(r.mUUID);
}

// LLGLSLShader indexed channel caps -- accessed via the shader class.
S32 indexedGLTFCap()   { return LLGLSLShader::sIndexedGLTFChannels; }
S32 indexedTexCap()    { return LLGLSLShader::sIndexedTextureChannels; }

// ---- classify one live LLDrawInfo into per-slot ghost+stock bindings --------
// di MUST be live (early: harvested this frame; late: pinned). Never called on a
// stale pointer.
void resolveBatchBindings(LLDrawInfo* di, GhostAuditBatch& out)
{
    out.mSlots.clear();

    // ---- indexed GLTF (multi-material PBR) ----
    if (di->mGLTFMaterialList.size() > 1)
    {
        out.mMaterialKind = "indexed-gltf";
        const S32 cap = indexedGLTFCap();
        const S32 n = (S32)di->mGLTFMaterialList.size();
        for (S32 s = 0; s < n; ++s)
        {
            GhostAuditSlot slot;
            slot.mSlot = s;
            slot.mOverCap = (s >= cap);
            LLFetchedGLTFMaterial* mat = di->mGLTFMaterialList[s].get();
            if (!mat)
            {
                slot.mIsGap = true;
                slot.mGhost.mColor = refGap();
                slot.mStock.mColor = refGap();
                out.mSlots.push_back(slot);
                continue;
            }
            // no scalar media override in the indexed path
            slot.mGhost.mColor = mat->mBaseColorTexture.notNull()
                ? refReal(mat->mBaseColorTexture.get()) : refWhite();
            slot.mStock.mColor = slot.mGhost.mColor;
            slot.mGhost.mEmissive = slot.mStock.mEmissive =
                mat->mEmissiveTexture.notNull() ? refReal(mat->mEmissiveTexture.get()) : refWhite();
            const bool nrm_ok = mat->mNormalTexture.notNull()
                && mat->mNormalTexture->getDiscardLevel() <= 4;
            slot.mGhost.mNormal = slot.mStock.mNormal =
                nrm_ok ? refReal(mat->mNormalTexture.get()) : refFlatNormal();
            slot.mGhost.mSpecularOrORM = slot.mStock.mSpecularOrORM =
                mat->mMetallicRoughnessTexture.notNull()
                    ? refReal(mat->mMetallicRoughnessTexture.get()) : refWhite();
            slot.mGhost.mColorTransformHash = slot.mStock.mColorTransformHash =
                hashTransform(mat->mTextureTransform[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR]);
            slot.mGhost.mAlphaCutoff = slot.mStock.mAlphaCutoff = mat->mAlphaCutoff;
            slot.mGhost.mDoubleSided = slot.mStock.mDoubleSided = mat->mDoubleSided;
            out.mSlots.push_back(slot);
        }
        return;
    }

    // ---- scalar GLTF (single material PBR) ----
    if (di->mGLTFMaterial.notNull())
    {
        out.mMaterialKind = "scalar-gltf";
        LLFetchedGLTFMaterial* mat = di->mGLTFMaterial.get();
        GhostAuditSlot slot;
        slot.mSlot = 0;
        LLViewerTexture* media = di->mTexture.get();
        // GHOST base sweep == stock base: media else base else white
        slot.mGhost.mColor = media
            ? refMedia(media)
            : (mat->mBaseColorTexture.notNull() ? refReal(mat->mBaseColorTexture.get()) : refWhite());
        slot.mStock.mColor = slot.mGhost.mColor;
        // GHOST emissive: material emissive only (NO media override)
        slot.mGhost.mEmissive = mat->mEmissiveTexture.notNull()
            ? refReal(mat->mEmissiveTexture.get()) : refWhite();
        // STOCK emissive: media else material emissive else white (bind() applies
        // the media override to emissive too) -> the decisive scalar-GLTF diff.
        slot.mStock.mEmissive = media
            ? refMedia(media)
            : (mat->mEmissiveTexture.notNull() ? refReal(mat->mEmissiveTexture.get()) : refWhite());
        const bool nrm_ok = mat->mNormalTexture.notNull()
            && mat->mNormalTexture->getDiscardLevel() <= 4;
        slot.mGhost.mNormal = slot.mStock.mNormal =
            nrm_ok ? refReal(mat->mNormalTexture.get()) : refFlatNormal();
        slot.mGhost.mSpecularOrORM = slot.mStock.mSpecularOrORM =
            mat->mMetallicRoughnessTexture.notNull()
                ? refReal(mat->mMetallicRoughnessTexture.get()) : refWhite();
        slot.mGhost.mColorTransformHash = slot.mStock.mColorTransformHash =
            hashTransform(mat->mTextureTransform[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR]);
        slot.mGhost.mAlphaCutoff = slot.mStock.mAlphaCutoff = mat->mAlphaCutoff;
        slot.mGhost.mDoubleSided = slot.mStock.mDoubleSided = mat->mDoubleSided;
        out.mSlots.push_back(slot);
        return;
    }

    // ---- indexed legacy material ----
    if (di->mMaterialSlotList.size() > 1)
    {
        out.mMaterialKind = "indexed-legacy";
        const S32 cap = indexedGLTFCap();   // legacy indexed uses the GLTF cap
        const S32 n = (S32)di->mMaterialSlotList.size();
        for (S32 s = 0; s < n; ++s)
        {
            GhostAuditSlot slot;
            slot.mSlot = s;
            slot.mOverCap = (s >= cap);
            const LLDrawInfo::MaterialSlot& ms = di->mMaterialSlotList[s];
            // ghost_batch_slot_texture resolves mDiffuse; draw binds white if null
            slot.mGhost.mColor = ms.mDiffuse.notNull() ? refReal(ms.mDiffuse.get()) : refWhite();
            slot.mStock.mColor = slot.mGhost.mColor;
            slot.mGhost.mNormal = slot.mStock.mNormal =
                ms.mNormalMap.notNull() ? refReal(ms.mNormalMap.get()) : refFlatNormal();
            slot.mGhost.mSpecularOrORM = slot.mStock.mSpecularOrORM =
                ms.mSpecularMap.notNull() ? refReal(ms.mSpecularMap.get()) : refWhite();
            slot.mGhost.mAlphaCutoff = slot.mStock.mAlphaCutoff = ms.mAlphaMaskCutoff;
            out.mSlots.push_back(slot);
        }
        return;
    }

    // ---- generic indexed texture list (non-material) ----
    if (di->mTextureList.size() > 1)
    {
        out.mMaterialKind = "indexed-texlist";
        const S32 cap = indexedTexCap();
        const S32 n = (S32)di->mTextureList.size();
        for (S32 s = 0; s < n; ++s)
        {
            GhostAuditSlot slot;
            slot.mSlot = s;
            slot.mOverCap = (s >= cap);
            LLViewerTexture* t = di->mTextureList[s].get();
            // stock leaves the unit UNCHANGED for a null slot -> INHERITED
            slot.mGhost.mColor = t ? refReal(t) : refInherited();
            slot.mStock.mColor = slot.mGhost.mColor;
            out.mSlots.push_back(slot);
        }
        return;
    }

    // ---- scalar legacy / simple / fullbright / alpha / bump ----
    out.mMaterialKind = "scalar-legacy";
    GhostAuditSlot slot;
    slot.mSlot = 0;
    LLViewerTexture* diff = di->mTexture.get();
    slot.mGhost.mColor = diff ? refReal(diff) : GhostAuditTextureRef();  // NONE if null
    slot.mStock.mColor = slot.mGhost.mColor;
    slot.mGhost.mNormal = slot.mStock.mNormal =
        di->mNormalMap.notNull() ? refReal(di->mNormalMap.get()) : refFlatNormal();
    slot.mGhost.mSpecularOrORM = slot.mStock.mSpecularOrORM =
        di->mSpecularMap.notNull() ? refReal(di->mSpecularMap.get()) : refWhite();
    slot.mGhost.mAlphaCutoff = slot.mStock.mAlphaCutoff = di->mAlphaMaskCutoff;
    slot.mGhost.mBump = slot.mStock.mBump = di->mBump;
    out.mSlots.push_back(slot);
}

// ---- fill a batch snapshot's structural/flag fields from a live di ----------
void snapshotBatchFields(LLDrawInfo* di, LLVOAvatar* wearer, LLSpatialGroup* group,
                         U32 pass, GhostAuditBatch& out)
{
    out.mAddress = di;
    out.mVertexBuffer = di->mVertexBuffer.get();
    out.mGroup = group;
    out.mWearerId = wearer ? wearer->getID() : LLUUID::null;
    out.mObjectRootId = di->mAvatar.notNull() ? di->mAvatar->getID() : LLUUID::null;
    out.mMaterialId = di->mMaterialID;
    out.mPass = pass;
    out.mStart = di->mStart;
    out.mEnd = di->mEnd;
    out.mCount = di->mCount;
    out.mOffset = di->mOffset;
    out.mVertexTypeMask = di->mVertexBuffer.notNull() ? di->mVertexBuffer->getTypeMask() : 0;
    out.mHasTextureIndex = di->mVertexBuffer.notNull()
        && di->mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXTURE_INDEX);
    out.mSkinHash = di->getSkinHash();
    out.mHasTextureMatrix = (di->mTextureMatrix != nullptr);
    out.mTextureMatrixHash = hashMatrix(di->mTextureMatrix);
    out.mHasModelMatrix = (di->mModelMatrix != nullptr);
    out.mModelMatrixHash = hashMatrix(di->mModelMatrix);
    out.mShaderMask = di->mShaderMask;
    out.mAlphaMaskCutoff = di->mAlphaMaskCutoff;
    out.mDiffuseAlphaMode = di->mDiffuseAlphaMode;
    out.mBump = di->mBump;
    out.mShiny = di->mShiny;
    out.mFullbright = di->mFullbright;
    out.mHasGlow = di->mHasGlow;
    resolveBatchBindings(di, out);
}

// ---- static-face resolution -------------------------------------------------
void snapshotStaticFields(LLFace* face, LLViewerObject* obj,
                          U8 alpha_kind, F32 cutoff, bool double_sided,
                          GhostAuditStaticFace& out)
{
    out.mFaceAddress = face;
    out.mObjectId = obj ? obj->getID() : LLUUID::null;
    out.mTEOffset = face->getTEOffset();
    out.mTextureIndex = (S32)face->getTextureIndex();
    out.mHasDrawInfo = (face->mDrawInfo != nullptr);
    out.mRenderMatrixHash = hashMatrixVal(face->getRenderMatrix());
    out.mTextureMatrixHash = hashMatrix(face->mTextureMatrix);
    out.mAlphaKind = alpha_kind;
    out.mCutoff = cutoff;
    out.mDoubleSided = double_sided;

    // GHOST color mirrors the existing static overlay code: fetched GLTF base
    // else face->getTexture(); draw binds white if both null.
    const LLTextureEntry* te = face->getTextureEntry();
    LLGLTFMaterial* gmat = te ? te->getGLTFRenderMaterial() : nullptr;
    const LLFetchedGLTFMaterial* fetched = dynamic_cast<const LLFetchedGLTFMaterial*>(gmat);
    LLViewerTexture* ghost_color =
        (fetched && fetched->mBaseColorTexture.notNull())
            ? fetched->mBaseColorTexture.get() : face->getTexture();
    out.mGhost.mColor = ghost_color ? refReal(ghost_color) : refWhite();
    out.mGhost.mAlphaCutoff = cutoff;   // harvested cutoff (else colour+cutoff compare is inert)

    // STOCK color: the DECISIVE path is face->mDrawInfo + getTextureIndex.
    LLDrawInfo* di = face->mDrawInfo;
    if (!di)
    {
        out.mSourceUnverifiable = "no-drawinfo";
        return;
    }
    GhostAuditBatch stock_batch;
    resolveBatchBindings(di, stock_batch);
    if (stock_batch.mSlots.empty())
    {
        out.mStock.mColor = refWhite();
        return;
    }
    // pick the vertex-selected slot for indexed batches, else slot 0. An
    // out-of-range texture index on an indexed batch is UNVERIFIABLE (silently
    // reading slot 0 would fabricate a wrong stock result).
    S32 slot = 0;
    if (stock_batch.mSlots.size() > 1)
    {
        const S32 ti = (S32)face->getTextureIndex();
        if (ti >= 0 && ti < (S32)stock_batch.mSlots.size())
        {
            slot = ti;
        }
        else
        {
            out.mSourceUnverifiable = "static-texindex-out-of-range";
            return;
        }
    }
    out.mStock = stock_batch.mSlots[slot].mStock;
}

// ---- binding comparison -> flags --------------------------------------------
// FULL comparison across every field GhostAuditBinding carries -- used both for
// ghost-vs-stock (resolution divergence) and early-vs-late (material mutation).
bool bindingDiffers(const GhostAuditBinding& a, const GhostAuditBinding& b, std::string& why)
{
    bool diff = false;
    if (!a.mColor.sameBinding(b.mColor))          { why += "color "; diff = true; }
    if (!a.mNormal.sameBinding(b.mNormal))        { why += "normal "; diff = true; }
    if (!a.mSpecularOrORM.sameBinding(b.mSpecularOrORM)) { why += "spec_orm "; diff = true; }
    if (!a.mEmissive.sameBinding(b.mEmissive))    { why += "emissive "; diff = true; }
    if (a.mColorTransformHash != b.mColorTransformHash) { why += "uv "; diff = true; }
    if (a.mAlphaCutoff != b.mAlphaCutoff)         { why += "cutoff "; diff = true; }
    if (a.mDoubleSided != b.mDoubleSided)         { why += "doublesided "; diff = true; }
    if (a.mBump != b.mBump)                       { why += "bump "; diff = true; }
    return diff;
}

// COLOR-ONLY comparison. The static (non-rigged) overlay resolves ONLY the
// colour map; normal/spec/emissive are the source's data that the unlit overlay
// deliberately ignores (the approximation gap), NOT a data divergence -- so
// comparing them would false-flag every static face. The decisive question for
// a static clone is: does it show the right COLOUR texture?
bool colorBindingDiffers(const GhostAuditBinding& a, const GhostAuditBinding& b, std::string& why)
{
    bool diff = false;
    if (!a.mColor.sameBinding(b.mColor)) { why += "color "; diff = true; }
    // COLOUR ONLY. The static (non-rigged) overlay resolves only the colour map;
    // normal/spec/emissive are the source's data the unlit overlay ignores (the
    // approximation gap, not a data divergence). UV + cutoff are also excluded:
    // the ghost overlay's effective UV (face->mTextureMatrix * KHR) and cutoff
    // (only meaningful on masked faces) do not share the stock batch's
    // representation, so comparing them false-flags every GLTF/masked static
    // face even when the bound colour texture is identical. The decisive
    // question for a static clone -- "does it show the right colour texture?" --
    // is fully answered by mColor.
    return diff;
}

// Structural + material-identity fields that a rebuilt/reused LLDrawInfo could
// change between early harvest and late draw (the ABA / mutation guard).
bool batchStructureDiffers(const GhostAuditBatch& a, const GhostAuditBatch& b, std::string& why)
{
    bool diff = false;
    if (a.mVertexBuffer != b.mVertexBuffer)   { why += "vb "; diff = true; }
    if (a.mStart != b.mStart || a.mEnd != b.mEnd || a.mCount != b.mCount
        || a.mOffset != b.mOffset)            { why += "range "; diff = true; }
    if (a.mGroup != b.mGroup)                 { why += "group "; diff = true; }
    if (a.mVertexTypeMask != b.mVertexTypeMask) { why += "vtmask "; diff = true; }
    if (a.mHasTextureIndex != b.mHasTextureIndex) { why += "texidx "; diff = true; }
    if (a.mTextureMatrixHash != b.mTextureMatrixHash
        || a.mHasTextureMatrix != b.mHasTextureMatrix) { why += "texmat "; diff = true; }
    if (a.mModelMatrixHash != b.mModelMatrixHash
        || a.mHasModelMatrix != b.mHasModelMatrix) { why += "modelmat "; diff = true; }
    if (a.mSkinHash != b.mSkinHash)           { why += "skin "; diff = true; }
    if (a.mMaterialId != b.mMaterialId)       { why += "matid "; diff = true; }
    if (a.mMaterialKind != b.mMaterialKind)   { why += "matkind "; diff = true; }
    if (a.mShaderMask != b.mShaderMask)       { why += "shadermask "; diff = true; }
    if (a.mDiffuseAlphaMode != b.mDiffuseAlphaMode) { why += "alphamode "; diff = true; }
    if (a.mAlphaMaskCutoff != b.mAlphaMaskCutoff) { why += "alphacut "; diff = true; }
    if (a.mBump != b.mBump || a.mShiny != b.mShiny
        || a.mFullbright != b.mFullbright || a.mHasGlow != b.mHasGlow) { why += "flags "; diff = true; }
    if (a.mSlots.size() != b.mSlots.size())   { why += "slotcount "; diff = true; }
    return diff;
}

// early-vs-late per-slot binding drift (material mutated between harvest + draw)
bool batchBindingsDrifted(const GhostAuditBatch& early, const GhostAuditBatch& late, std::string& why)
{
    const size_t n = std::min(early.mSlots.size(), late.mSlots.size());
    for (size_t s = 0; s < n; ++s)
    {
        std::string w;
        if (bindingDiffers(early.mSlots[s].mGhost, late.mSlots[s].mGhost, w))
        {
            why += llformat("[s%d ghost %s]", (S32)s, w.c_str());
        }
        w.clear();
        if (bindingDiffers(early.mSlots[s].mStock, late.mSlots[s].mStock, w))
        {
            why += llformat("[s%d stock %s]", (S32)s, w.c_str());
        }
    }
    return !why.empty();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
LLCloneFidelityAudit& LLCloneFidelityAudit::instance()
{
    static LLCloneFidelityAudit sInstance;
    return sInstance;
}

void LLCloneFidelityAudit::report(const std::string& message)
{
    if (LLFloaterIMNearbyChat* nearby =
            LLFloaterReg::getTypedInstance<LLFloaterIMNearbyChat>("nearby_chat"))
    {
        LLChat chat;
        chat.mText = "[Clone Fidelity] " + message;
        chat.mSourceType = CHAT_SOURCE_SYSTEM;
        chat.mChatType = CHAT_TYPE_NORMAL;
        nearby->addMessage(chat);
    }
    LL_INFOS("CloneFidelity") << message << LL_ENDL;
}

bool LLCloneFidelityAudit::isArmed() const
{
    return mState == STATE_ARMED || mState == STATE_CAPTURING || mState == STATE_CAPTURED;
}

void LLCloneFidelityAudit::resetCaptureState()
{
    mEarlyBatches.clear();
    mEarlyStatics.clear();
}

// ---- selection --------------------------------------------------------------
bool LLCloneFidelityAudit::selectTargets(const std::string& argument,
                                         std::vector<LLUUID>& out) const
{
    out.clear();
    ALGhostStudio& studio = ALGhostStudio::instance();
    const std::vector<ALGhostStudio::Instance>& instances = studio.getInstances();

    auto eligible = [&](const ALGhostStudio::Instance& inst) -> bool
    {
        return inst.mEnabled && inst.mStyle == kGhostStyleClone;
    };

    if (argument == "all")
    {
        for (const auto& inst : instances)
        {
            if (eligible(inst)) { out.push_back(inst.mId); }
        }
        return !out.empty();
    }

    if (!argument.empty())
    {
        LLUUID requested(argument);
        if (requested.notNull())
        {
            for (const auto& inst : instances)
            {
                if (inst.mId == requested && eligible(inst))
                {
                    out.push_back(inst.mId);
                    return true;
                }
            }
        }
        // fall through to default selection on a non-matching argument
    }

    // selected instance, if it names an eligible clone
    const LLUUID& sel = studio.getSelected();
    if (sel.notNull())
    {
        for (const auto& inst : instances)
        {
            if (inst.mId == sel && eligible(inst))
            {
                out.push_back(sel);
                return true;
            }
        }
    }

    // nearest eligible clone to the camera
    const LLVector3 cam = LLViewerCamera::getInstance()->getOrigin();
    const ALGhostStudio::Instance* best = nullptr;
    F32 best_d2 = 0.f;
    for (const auto& inst : instances)
    {
        if (!eligible(inst)) { continue; }
        const LLVector3 foot = gAgent.getPosAgentFromGlobal(inst.mFootGlobal);
        const F32 d2 = (foot - cam).lengthSquared();
        if (!best || d2 < best_d2) { best = &inst; best_d2 = d2; }
    }
    if (best)
    {
        out.push_back(best->mId);
        return true;
    }
    return false;
}

bool LLCloneFidelityAudit::arm(const std::vector<LLUUID>& instance_ids)
{
    if (instance_ids.empty())
    {
        return false;
    }
    resetCaptureState();
    mInstanceIds = instance_ids;
    mInstanceMeta.clear();
    mSourceIds.clear();

    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& iid : instance_ids)
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(iid);
        if (!inst) { continue; }

        // RESOLVE the source to the real avatar the harvest actually sees.
        // Critical for the self-clone case: inst->mSource is null ("my avatar")
        // but collectGhostBatches harvests under the AGENT's real UUID, so the
        // early-capture membership filter must compare against the resolved id.
        // An UNRESOLVED source (out of world / left region) is skipped entirely
        // -- arming on its raw/null id would insert a phantom that matches
        // nothing and falsely report total data loss.
        LLVOAvatar* src = LLDirectorCast::instance().resolve(inst->mSource);
        if (!src || src->isDead())
        {
            continue;
        }

        InstanceMeta meta;
        meta.mInstanceId = iid;
        meta.mSourceId = inst->mSource;
        meta.mStyle = inst->mStyle;
        meta.mFrozen = (inst->mPose == ALGhostStudio::POSE_FROZEN);
        meta.mResolvedSourceId = src->getID();
        mInstanceMeta.push_back(meta);

        if (std::find(mSourceIds.begin(), mSourceIds.end(), meta.mResolvedSourceId)
            == mSourceIds.end())
        {
            mSourceIds.push_back(meta.mResolvedSourceId);
        }
    }

    if (mSourceIds.empty())
    {
        report("Selected instance(s) have no resolvable source avatar.");
        mState = STATE_IDLE;
        return false;
    }

    mState = STATE_ARMED;
    mArmedFrame = LLFrameTimer::getFrameCount();

    if (instance_ids.size() == 1)
    {
        report(llformat("Audit armed for next complete render frame: %s.",
                        shortId(instance_ids[0]).c_str()));
    }
    else
    {
        report(llformat("Audit armed for next complete render frame: %d Clone instances.",
                        (S32)instance_ids.size()));
    }
    return true;
}

// ---- early capture ----------------------------------------------------------
void LLCloneFidelityAudit::beginEarlyCapture(U32 frame)
{
    if (mState != STATE_ARMED)
    {
        return;     // capture only on the frame AFTER arming, once
    }
    // don't capture on the same frame we armed (arming happens mid-frame in the
    // command; wait for the next full frame's harvest)
    if (frame == mArmedFrame)
    {
        return;
    }
    resetCaptureState();
    mState = STATE_CAPTURING;
    mCaptureFrame = frame;
}

void LLCloneFidelityAudit::captureEarlyRigged(LLVOAvatar* wearer, LLSpatialGroup* group, U32 pass,
                                              const LLPointer<LLDrawInfo>& draw_info,
                                              bool retained_by_collector)
{
    if (mState != STATE_CAPTURING) { return; }
    LLDrawInfo* di = draw_info.get();
    if (!di) { return; }
    // only audit sources we armed for
    const LLUUID wid = wearer ? wearer->getID() : LLUUID::null;
    if (std::find(mSourceIds.begin(), mSourceIds.end(), wid) == mSourceIds.end())
    {
        return;
    }
    GhostAuditBatch batch;
    snapshotBatchFields(di, wearer, group, pass, batch);
    batch.mRetainedByCollector = retained_by_collector;
    mEarlyBatches.push_back(std::move(batch));
}

void LLCloneFidelityAudit::captureEarlyStatic(LLVOAvatar* wearer, LLViewerObject* object, LLFace* face,
                                              U8 alpha_kind, F32 cutoff, bool double_sided)
{
    if (mState != STATE_CAPTURING) { return; }
    if (!face) { return; }
    const LLUUID wid = wearer ? wearer->getID() : LLUUID::null;
    if (std::find(mSourceIds.begin(), mSourceIds.end(), wid) == mSourceIds.end())
    {
        return;
    }
    GhostAuditStaticFace sf;
    snapshotStaticFields(face, object, alpha_kind, cutoff, double_sided, sf);
    mEarlyStatics.push_back(std::move(sf));
}

void LLCloneFidelityAudit::endEarlyCapture()
{
    if (mState != STATE_CAPTURING) { return; }
    mState = STATE_CAPTURED;
}

// ---- late audit -------------------------------------------------------------
void LLCloneFidelityAudit::runLateAuditIfPending()
{
    const U32 frame = LLFrameTimer::getFrameCount();

    // ---- expiry: an armed audit whose source never harvested (collectGhostBatches
    // returned early -- source unresolved / out of world / studio inactive) would
    // otherwise stay armed forever. Give it a bounded window, then fail cleanly.
    if (mState == STATE_ARMED)
    {
        constexpr U32 kArmExpiryFrames = 30;
        if (frame > mArmedFrame + kArmExpiryFrames)
        {
            report("Audit expired: the source avatar produced no ghost harvest "
                   "(out of world, not built nearby, or studio inactive).");
            mState = STATE_IDLE;
            resetCaptureState();
        }
        return;
    }

    if (mState != STATE_CAPTURED) { return; }
    // only consume on the SAME frame the early capture ran (the late walk must
    // see the same-frame live draw maps)
    if (mCaptureFrame != frame)
    {
        // stale (a snapshot frame or timing skip intervened) -> disarm cleanly
        mState = STATE_IDLE;
        resetCaptureState();
        return;
    }
    doLateAudit();
    mState = STATE_IDLE;
    resetCaptureState();
}

namespace
{
// stable structural identity for the REBUILT_EQUIVALENT fallback (a draw-info
// rebuilt in place gets a NEW address but the same wearer/group/pass/VB/range).
std::string structuralKey(const GhostAuditBatch& b)
{
    return llformat("%s|%p|%u|%p|%u:%u:%u:%u", b.mWearerId.asString().c_str(),
                    b.mGroup, b.mPass, b.mVertexBuffer,
                    (U32)b.mStart, (U32)b.mEnd, b.mCount, b.mOffset);
}
} // anonymous namespace

void LLCloneFidelityAudit::doLateAudit()
{
    // ---- re-walk each source's LIVE draw maps, pinning each di --------------
    struct LiveRecord
    {
        LLPointer<LLDrawInfo> mPin;   // pins the di for the audit's lifetime
        GhostAuditBatch mValues;
        bool mMatched = false;
    };
    std::vector<LiveRecord> live_records;
    std::unordered_map<const void*, std::vector<size_t> > live_by_address;
    std::unordered_map<std::string, std::vector<size_t> > live_by_structure;

    struct LiveStatic
    {
        GhostAuditStaticFace mValues;
        bool mMatched = false;
    };
    std::vector<LiveStatic> live_statics;
    std::unordered_map<const void*, std::vector<size_t> > live_static_by_address;

    LLActorMover& mover = LLActorMover::instance();
    for (const LLUUID& sid : mSourceIds)
    {
        LLVOAvatar* src = LLDirectorCast::instance().resolve(sid);
        if (!src || src->isDead()) { continue; }

        mover.walkGhostSourceGeometry(src,
            [&](LLVOAvatar* wearer, LLSpatialGroup* group, U32 pass,
                const LLPointer<LLDrawInfo>& draw_info)
            {
                LiveRecord rec;
                rec.mPin = draw_info;   // PIN before any other late reference
                snapshotBatchFields(draw_info.get(), wearer, group, pass, rec.mValues);
                const void* addr = rec.mValues.mAddress;
                const std::string skey = structuralKey(rec.mValues);
                live_records.push_back(std::move(rec));
                const size_t idx = live_records.size() - 1;
                live_by_address[addr].push_back(idx);
                live_by_structure[skey].push_back(idx);
            },
            [&](LLVOAvatar* /*wearer*/, LLViewerObject* obj, LLFace* face)
            {
                LiveStatic ls;
                snapshotStaticFields(face, obj, 0, 0.f, false, ls.mValues);
                const void* addr = ls.mValues.mFaceAddress;
                live_statics.push_back(std::move(ls));
                live_static_by_address[addr].push_back(live_statics.size() - 1);
            });
    }

    // consume a live candidate ONE-TO-ONE (skip already-matched); returns the
    // index or npos.
    auto claimCandidate = [&](const std::vector<size_t>& idxs, U32 want_pass,
                              bool require_pass) -> size_t
    {
        for (size_t idx : idxs)
        {
            if (live_records[idx].mMatched) { continue; }
            if (require_pass && live_records[idx].mValues.mPass != want_pass) { continue; }
            live_records[idx].mMatched = true;
            return idx;
        }
        return (size_t)-1;
    };

    // ---- counters + collected diffs ----------------------------------------
    S32 exact = 0, binding_diff = 0, mutated = 0, dropped = 0, stale = 0,
        reclassified = 0, rebuilt = 0, ambiguous = 0, dedup_conflicts = 0;
    std::vector<std::string> diff_lines;

    // retained early records that survived the collector dedup (the clone's real
    // membership). Non-retained early records feed the DROP_DEDUP check only.
    std::vector<const GhostAuditBatch*> retained_early;
    for (const GhostAuditBatch& e : mEarlyBatches)
    {
        if (e.mRetainedByCollector) { retained_early.push_back(&e); }
    }

    // ---- DROP_DEDUP: a dropped early entry whose VB/range twin was retained
    // under a DIFFERENT pass -- the collector's pass-blind dedup discarded a
    // semantically distinct pass the clone may actually need (e.g. base+glow).
    for (const GhostAuditBatch& e : mEarlyBatches)
    {
        if (e.mRetainedByCollector) { continue; }
        for (const GhostAuditBatch* r : retained_early)
        {
            // same SOURCE (collector dedup happens within each wearer's bucket)
            // + same VB/range but a DIFFERENT pass.
            if (r->mWearerId == e.mWearerId
                && r->mVertexBuffer == e.mVertexBuffer && r->mStart == e.mStart
                && r->mEnd == e.mEnd && r->mOffset == e.mOffset && r->mPass != e.mPass)
            {
                ++dedup_conflicts;
                diff_lines.push_back(llformat("DROP_DEDUP kept_pass=%u dropped_pass=%u",
                                              r->mPass, e.mPass));
                LL_INFOS("CloneFidelity") << "kind=rigged status=DROP_DEDUP"
                    << " kept_pass=" << r->mPass << " dropped_pass=" << e.mPass
                    << " vb=" << e.mVertexBuffer << LL_ENDL;
                break;
            }
        }
    }

    // ---- match each retained early record against the live truth -----------
    for (const GhostAuditBatch* ep : retained_early)
    {
        const GhostAuditBatch& early = *ep;
        std::string status;
        const GhostAuditBatch* late = nullptr;

        auto ait = live_by_address.find(early.mAddress);
        if (ait != live_by_address.end())
        {
            // same address, same pass (one-to-one)
            size_t idx = claimCandidate(ait->second, early.mPass, /*require_pass=*/true);
            if (idx != (size_t)-1)
            {
                late = &live_records[idx].mValues;
            }
            else
            {
                // same address, different pass -> reclassified
                idx = claimCandidate(ait->second, 0, /*require_pass=*/false);
                if (idx != (size_t)-1)
                {
                    late = &live_records[idx].mValues;
                    status = "PASS_RECLASSIFIED";
                    ++reclassified;
                }
            }
        }

        if (!late)
        {
            // structural fallback: a rebuilt-in-place di (new address, same
            // wearer/group/pass/VB/range).
            auto sit = live_by_structure.find(structuralKey(early));
            if (sit != live_by_structure.end())
            {
                // count remaining unmatched candidates for AMBIGUOUS
                S32 remaining = 0;
                for (size_t idx : sit->second) { if (!live_records[idx].mMatched) { ++remaining; } }
                size_t idx = claimCandidate(sit->second, 0, /*require_pass=*/false);
                if (idx != (size_t)-1)
                {
                    late = &live_records[idx].mValues;
                    if (remaining > 1) { status = "AMBIGUOUS_MATCH"; ++ambiguous; }
                    else               { status = "REBUILT_EQUIVALENT"; ++rebuilt; }
                }
            }
        }

        if (!late)
        {
            // CLONE_ONLY_STALE -- never dereference early.mAddress
            ++stale;
            LL_INFOS("CloneFidelity") << "instance-src=" << shortId(early.mWearerId)
                << " kind=rigged status=CLONE_ONLY_STALE pass=" << early.mPass
                << " early_di=" << early.mAddress << " late_membership=false"
                << " vb=" << early.mVertexBuffer
                << " range=" << early.mStart << ":" << early.mEnd
                << ":" << early.mCount << ":" << early.mOffset << LL_ENDL;
            diff_lines.push_back(llformat("CLONE_ONLY_STALE pass=%u", early.mPass));
            continue;
        }

        // ---- early-vs-late structural/material mutation (ABA / deferred-phase drift)
        std::string mut_why;
        const bool structural_mut = batchStructureDiffers(early, *late, mut_why);
        std::string drift_why;
        const bool binding_drift = batchBindingsDrifted(early, *late, drift_why);

        // ---- late ghost-vs-stock resolution divergence (the decisive check) ----
        std::string bind_why;
        bool any_bind_diff = false;
        for (size_t s = 0; s < late->mSlots.size(); ++s)
        {
            std::string sw;
            if (bindingDiffers(late->mSlots[s].mGhost, late->mSlots[s].mStock, sw))
            {
                any_bind_diff = true;
                bind_why += llformat("[s%d %s]", (S32)s, sw.c_str());
            }
        }

        // Mutation is an INDEPENDENT dimension: count it whenever early->late
        // structural or binding drift is present, even if the record already got
        // a match-kind status (PASS_RECLASSIFIED / REBUILT_EQUIVALENT / AMBIGUOUS).
        const bool mut = structural_mut || binding_drift;
        if (mut) { ++mutated; }

        if (status.empty())
        {
            if (mut)                { status = "EARLY_TO_LATE_MUTATION"; }
            else if (any_bind_diff) { status = "MATCH_DATA_BINDING_DIFF"; ++binding_diff; }
            else                    { status = "MATCH_DATA_AND_BINDING"; ++exact; }
        }
        else
        {
            // reclassified / rebuilt / ambiguous already counted; fold in binding
            // divergence for the report (mutation already counted above).
            if (any_bind_diff) { ++binding_diff; }
            if (mut) { status += "+MUTATION"; }
        }

        if (status != "MATCH_DATA_AND_BINDING")
        {
            diff_lines.push_back(llformat("%s pass=%u %s%s%s", status.c_str(), late->mPass,
                bind_why.c_str(),
                structural_mut ? (std::string(" mut:") + mut_why).c_str() : "",
                binding_drift ? (std::string(" drift:") + drift_why).c_str() : ""));
            LL_INFOS("CloneFidelity") << "instance-src=" << shortId(early.mWearerId)
                << " kind=rigged status=" << status << " pass=" << late->mPass
                << " early_di=" << early.mAddress << " late_di=" << late->mAddress
                << " late_membership=true material=" << late->mMaterialKind
                << " material_id=" << late->mMaterialId
                << " ghost_vs_stock=" << (any_bind_diff ? bind_why : std::string("match"))
                << " struct_mut=" << (structural_mut ? mut_why : std::string("none"))
                << " binding_drift=" << (binding_drift ? drift_why : std::string("none"))
                << LL_ENDL;
        }
    }

    // SOURCE_ONLY_DROPPED: live records never claimed by a retained early record.
    // BUT suppress a live record whose VB+range matches a RETAINED early -- it is
    // a deliberately deduped twin (e.g. a PASS_GLTF_GLOW_RIGGED draw sharing a base
    // batch's VB/range) that the collector's pass-blind dedup already recorded via
    // its DROP_DEDUP report. Without this suppression the SAME twin is counted
    // TWICE (DROP_DEDUP + SOURCE_ONLY_DROPPED); this removes only the duplicate.
    // NOTE: it does NOT let emissive/glow content reach a clean PASS -- DROP_DEDUP
    // still (correctly) keeps total_diffs >= 1, because dropping a distinct glow
    // pass is a GENUINE fidelity loss: the clone's glow sweep draws only glow-pass
    // batches (llactormover.cpp ~3819), so a base+glow avatar that retains base and
    // drops the glow pass renders without its emissive glow. That FAIL is a TRUE
    // positive -- fixed by glow-completion in the collector (retain semantically-
    // distinct passes), not by the audit.
    for (const LiveRecord& rec : live_records)
    {
        if (rec.mMatched) { continue; }

        bool dedup_twin = false;
        for (const GhostAuditBatch* r : retained_early)
        {
            if (r->mWearerId == rec.mValues.mWearerId
                && r->mVertexBuffer == rec.mValues.mVertexBuffer
                && r->mStart == rec.mValues.mStart && r->mEnd == rec.mValues.mEnd
                && r->mOffset == rec.mValues.mOffset)
            {
                dedup_twin = true;
                break;
            }
        }
        if (dedup_twin) { continue; }   // deduped duplicate geometry, not omitted

        ++dropped;
        diff_lines.push_back(llformat("SOURCE_ONLY_DROPPED pass=%u", rec.mValues.mPass));
        LL_INFOS("CloneFidelity") << "instance-src=" << shortId(rec.mValues.mWearerId)
            << " kind=rigged status=SOURCE_ONLY_DROPPED pass=" << rec.mValues.mPass
            << " late_di=" << rec.mValues.mAddress
            << " material=" << rec.mValues.mMaterialKind << LL_ENDL;
    }

    // ---- static faces (COLOUR is the only thing the static overlay resolves) --
    S32 static_exact = 0, static_diff = 0, static_stale = 0, static_dropped = 0, static_unverifiable = 0;
    for (const GhostAuditStaticFace& early : mEarlyStatics)
    {
        auto it = live_static_by_address.find(early.mFaceAddress);
        size_t live_idx = (size_t)-1;
        if (it != live_static_by_address.end())
        {
            for (size_t idx : it->second)
            {
                if (!live_statics[idx].mMatched) { live_idx = idx; live_statics[idx].mMatched = true; break; }
            }
        }
        if (live_idx == (size_t)-1)
        {
            ++static_stale;
            diff_lines.push_back(llformat("STATIC CLONE_ONLY_STALE obj=%s", shortId(early.mObjectId).c_str()));
            LL_INFOS("CloneFidelity") << "kind=static status=CLONE_ONLY_STALE"
                << " face=" << early.mFaceAddress << " obj=" << early.mObjectId << LL_ENDL;
            continue;
        }
        const GhostAuditStaticFace& live = live_statics[live_idx].mValues;

        if (!live.mSourceUnverifiable.empty())
        {
            ++static_unverifiable;
            diff_lines.push_back(llformat("STATIC UNVERIFIABLE obj=%s (%s)",
                shortId(live.mObjectId).c_str(), live.mSourceUnverifiable.c_str()));
            LL_INFOS("CloneFidelity") << "kind=static status=UNVERIFIABLE"
                << " obj=" << live.mObjectId << " reason=" << live.mSourceUnverifiable
                << " texindex=" << live.mTextureIndex << " has_drawinfo=" << live.mHasDrawInfo << LL_ENDL;
            continue;
        }

        std::string why;
        if (colorBindingDiffers(live.mGhost, live.mStock, why))
        {
            ++static_diff;
            diff_lines.push_back(llformat("STATIC obj=%s %s", shortId(live.mObjectId).c_str(), why.c_str()));
            LL_INFOS("CloneFidelity") << "kind=static status=MATCH_DATA_BINDING_DIFF"
                << " obj=" << live.mObjectId
                << " ghost_color=" << refString(live.mGhost.mColor)
                << " stock_color=" << refString(live.mStock.mColor)
                << " texindex=" << live.mTextureIndex
                << " has_drawinfo=" << live.mHasDrawInfo << LL_ENDL;
        }
        else
        {
            ++static_exact;
        }
    }
    // late static faces never claimed by an early snapshot
    for (const LiveStatic& ls : live_statics)
    {
        if (!ls.mMatched)
        {
            ++static_dropped;
            LL_INFOS("CloneFidelity") << "kind=static status=SOURCE_ONLY_DROPPED"
                << " face=" << ls.mValues.mFaceAddress << " obj=" << ls.mValues.mObjectId << LL_ENDL;
        }
    }

    // ---- per-instance metadata header --------------------------------------
    for (const InstanceMeta& m : mInstanceMeta)
    {
        LLVOAvatar* src = LLDirectorCast::instance().resolve(m.mSourceId);
        report(llformat("instance %s src=%s style=%d %s",
            shortId(m.mInstanceId).c_str(),
            (src ? src->getFullname().c_str() : shortId(m.mResolvedSourceId).c_str()),
            m.mStyle, m.mFrozen ? "FROZEN(materials still live)" : "LIVE"));
    }

    // ---- chat summary -------------------------------------------------------
    const S32 total_surfaces = (S32)(retained_early.size() + mEarlyStatics.size());
    const S32 total_diffs = binding_diff + mutated + dropped + stale + reclassified
        + rebuilt + ambiguous + dedup_conflicts
        + static_diff + static_stale + static_dropped + static_unverifiable;

    if (total_diffs == 0)
    {
        report(llformat("DATA/BINDINGS PASS: %d surfaces matched (rigged %d static %d). "
                        "This does not assert pixel-identical rendering.",
                        total_surfaces, (S32)retained_early.size(), (S32)mEarlyStatics.size()));
    }
    else
    {
        report(llformat("frame %u / rigged %d static %d / exact %d binding-diff %d "
                        "mutated %d reclass %d rebuilt %d ambig %d dropped %d stale %d dedup %d",
                        mCaptureFrame, (S32)retained_early.size(), (S32)mEarlyStatics.size(),
                        exact, binding_diff, mutated, reclassified, rebuilt, ambiguous,
                        dropped, stale, dedup_conflicts));
        report(llformat("static: exact %d diff %d stale %d dropped %d unverifiable %d",
                        static_exact, static_diff, static_stale, static_dropped, static_unverifiable));
        S32 shown = 0;
        for (const std::string& line : diff_lines)
        {
            if (shown++ >= 8) { break; }
            report("DIFF " + line);
        }
        report("Full per-surface detail: log category CloneFidelity. Data/binding diff or "
               "drop = fixable; equal data with different pixels = render approximation.");
    }
}
