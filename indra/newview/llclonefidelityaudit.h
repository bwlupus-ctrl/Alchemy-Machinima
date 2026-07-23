/**
 * @file llclonefidelityaudit.h
 * @brief [CloneFidelity] Read-only diagnostic: is a Ghost Studio clone's
 *        harvested + resolved data identical to the source avatar's, mesh to
 *        materials? Answers "fixable data/resolution divergence" vs "data
 *        identical, only the render differs" per attachment, for the whole
 *        avatar. Two-timepoint: snapshot at the early harvest (collectGhostBatches)
 *        + re-walk the source's live draw maps immediately before render_ui()
 *        (while LLSpatialGroup::sNoDelete is true) and compare. Never dereferences
 *        a potentially-stale pointer -- membership is an address lookup, and the
 *        late records PIN their LLDrawInfo. See doc/CLONE_FIDELITY_AUDIT_*.md.
 *
 * Design note (deviates from the recipe deliberately): the ghost/stock BINDINGS
 * are RESOLVED at capture time (early: on the live harvested di; late: on the
 * pinned live di) and stored as VALUES, so the compare step never touches a raw
 * material pointer. This is strictly safer than resolving a value-copied
 * material at compare time and needs no exhaustive raw-material snapshot.
 */
#ifndef LL_LLCLONEFIDELITYAUDIT_H
#define LL_LLCLONEFIDELITYAUDIT_H

#include "lluuid.h"
#include "stdtypes.h"

#include <string>
#include <vector>

class LLVOAvatar;
class LLViewerObject;
class LLFace;
class LLSpatialGroup;
class LLDrawInfo;
class LLViewerTexture;
template <class T> class LLPointer;

// A resolved texture reference: the UUID + pointer the render path would select,
// classified so a null-gap / inherited / default is never confused with a real
// UUID or with each other.
struct GhostAuditTextureRef
{
    enum class Kind : U8
    {
        REAL,           // an actual viewer texture
        WHITE,          // sWhiteImagep default
        FLAT_NORMAL,    // sFlatNormalImagep default
        NULL_GAP,       // indexed GLTF slot deliberately unsampled (min_alpha=-1)
        MEDIA_OVERRIDE, // di->mTexture scalar media override
        INHERITED,      // generic mTextureList null -> unit left unchanged
        NONE            // no binding resolved
    };
    LLUUID  mUUID;
    const void* mPtr = nullptr;   // logged/compared only, never dereferenced
    Kind    mKind = Kind::NONE;

    bool sameBinding(const GhostAuditTextureRef& o) const
    {
        // effective-binding equality: kind + uuid (ptr is diagnostic only)
        return mKind == o.mKind && mUUID == o.mUUID;
    }
};

// What a draw path would bind for ONE surface/slot (no GL, no bind()).
struct GhostAuditBinding
{
    GhostAuditTextureRef mColor;
    GhostAuditTextureRef mNormal;
    GhostAuditTextureRef mSpecularOrORM;
    GhostAuditTextureRef mEmissive;
    U64 mColorTransformHash = 0;    // KHR/TE UV transform of the colour map
    F32 mAlphaCutoff = 0.f;
    bool mDoubleSided = false;
    U8  mBump = 0;
};

// One slot of a batch (scalar = slot 0; indexed materials = 0..N-1), with the
// ghost's chosen binding and the stock render's chosen binding side by side.
struct GhostAuditSlot
{
    S32 mSlot = 0;
    bool mIsGap = false;            // indexed GLTF null gap
    bool mOverCap = false;          // slot index beyond the shader channel cap
    GhostAuditBinding mGhost;       // what the clone overlay resolves
    GhostAuditBinding mStock;       // what the source's real render resolves
};

// Value snapshot of one harvested rigged batch (early) or one live draw-map
// entry (late). No LLPointer, no raw material pointer -- resolved at capture.
struct GhostAuditBatch
{
    const void* mAddress = nullptr;         // LLDrawInfo* identity (compare only)
    const void* mVertexBuffer = nullptr;
    const void* mGroup = nullptr;

    LLUUID mWearerId;
    LLUUID mObjectRootId;                   // drawing avatar id (proxy for root)
    LLUUID mMaterialId;

    U32 mPass = 0;
    U16 mStart = 0;
    U16 mEnd = 0;
    U32 mCount = 0;
    U32 mOffset = 0;
    U32 mVertexTypeMask = 0;
    U64 mSkinHash = 0;
    bool mHasTextureIndex = false;

    U64 mTextureMatrixHash = 0;
    U64 mModelMatrixHash = 0;
    bool mHasTextureMatrix = false;
    bool mHasModelMatrix = false;

    U32 mShaderMask = 0;
    F32 mAlphaMaskCutoff = 0.f;
    U8  mDiffuseAlphaMode = 0;
    U8  mBump = 0;
    U8  mShiny = 0;
    bool mFullbright = false;
    bool mHasGlow = false;

    bool mRetainedByCollector = true;       // survived the VB+range dedup (early)
    LLUUID mMaterialClass;                  // unused; reserved
    std::string mMaterialKind;              // "scalar-gltf"/"indexed-gltf"/"legacy"/...

    std::vector<GhostAuditSlot> mSlots;     // per-slot ghost-vs-stock bindings
};

// Value snapshot of one harvested static (non-rigged) attachment face.
struct GhostAuditStaticFace
{
    const void* mFaceAddress = nullptr;
    LLUUID mObjectId;
    U32 mVertexTypeMask = 0;
    S32 mTEOffset = -1;
    S32 mTextureIndex = -1;
    bool mHasDrawInfo = false;
    U8  mAlphaKind = 0;
    F32 mCutoff = 0.f;
    bool mDoubleSided = false;
    U64 mRenderMatrixHash = 0;
    U64 mTextureMatrixHash = 0;
    GhostAuditBinding mGhost;   // what the static overlay resolves
    GhostAuditBinding mStock;   // what the source's real render resolves
    std::string mSourceUnverifiable;  // set when face->mDrawInfo absent at late
};

class LLCloneFidelityAudit
{
public:
    static LLCloneFidelityAudit& instance();

    // ---- /clonefidelity command surface ----
    // Resolve the command argument ("", "all", or an instance UUID) to a set of
    // eligible clone-instance ids. Returns false + empty when nothing matches.
    bool selectTargets(const std::string& argument, std::vector<LLUUID>& out) const;
    // Arm the audit for the NEXT complete render frame (does not walk geometry).
    bool arm(const std::vector<LLUUID>& instance_ids);
    bool isArmed() const;

    // ---- early capture (called from collectGhostBatches; no-op unless armed) ----
    void beginEarlyCapture(U32 frame);
    void captureEarlyRigged(LLVOAvatar* wearer, LLSpatialGroup* group, U32 pass,
                            const LLPointer<LLDrawInfo>& draw_info,
                            bool retained_by_collector);
    // (alpha_kind/cutoff/double_sided are the harvested GhostStaticFace's fields,
    // passed by value so this header needs no LLActorMover nested type.)
    void captureEarlyStatic(LLVOAvatar* wearer, LLViewerObject* object, LLFace* face,
                            U8 alpha_kind, F32 cutoff, bool double_sided);
    void endEarlyCapture();

    // ---- late audit (called immediately before render_ui, sNoDelete true) ----
    void runLateAuditIfPending();

    // chat feedback helper (shared by the command + the report)
    static void report(const std::string& message);

private:
    LLCloneFidelityAudit() = default;

    enum EState { STATE_IDLE, STATE_ARMED, STATE_CAPTURING, STATE_CAPTURED };

    void resetCaptureState();
    void doLateAudit();

    // per-clone-instance metadata for the report (instances sharing a source
    // are audited once but reported per instance with their own qualification).
    struct InstanceMeta
    {
        LLUUID mInstanceId;
        LLUUID mSourceId;           // raw (null = my avatar)
        LLUUID mResolvedSourceId;   // the real avatar id the harvest sees
        S32    mStyle = 0;
        bool   mFrozen = false;     // POSE_FROZEN (palettes/attach mats frozen; MATERIALS stay live)
    };

    EState mState = STATE_IDLE;
    U32 mArmedFrame = 0;         // frame the command armed on (capture next frame)
    U32 mCaptureFrame = 0;       // frame the early capture ran

    std::vector<LLUUID> mInstanceIds;       // clone instances to report under
    std::vector<InstanceMeta> mInstanceMeta;
    std::vector<LLUUID> mSourceIds;         // RESOLVED source avatar ids (deduped)

    std::vector<GhostAuditBatch> mEarlyBatches;
    std::vector<GhostAuditStaticFace> mEarlyStatics;
};

#endif // LL_LLCLONEFIDELITYAUDIT_H
