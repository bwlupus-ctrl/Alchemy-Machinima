/**
 * @file alghostmaterialresolver.h
 * @brief Source-resolved material snapshots for Ghost Studio clone backends.
 * @note Extracts the material phase of LLGhostAvatar's copy_prim_state() and
 *       supplies the GHOSTMATVERIFY acceptance seam.
 *
 * UNBUILT — pending Claude compile/link/in-world verification.
 */

#ifndef AL_ALGHOSTMATERIALRESOLVER_H
#define AL_ALGHOSTMATERIALRESOLVER_H

#include "llgltfmaterial.h"
#include "llpointer.h"
#include "llprimitive.h"
#include "lltextureentry.h"
#include "lluuid.h"
#include "llviewertexture.h"

#include <array>
#include <vector>

class LLVOAvatar;
class LLVOVolume;

struct ALGhostResolvedMaterialFace
{
    U8 mIndex = 0;
    LLTextureEntry mSemantic;
    LLUUID mRenderMaterialId;

    // Source-resolved legacy channels.
    LLPointer<LLViewerTexture> mDiffuse;
    LLPointer<LLViewerTexture> mNormal;
    LLPointer<LLViewerTexture> mSpecular;

    // Source-resolved PBR channels from the effective render material.
    LLPointer<LLViewerTexture> mBaseColor;
    LLPointer<LLViewerTexture> mPbrNormal;
    LLPointer<LLViewerTexture> mMetallicRoughness;
    LLPointer<LLViewerTexture> mEmissive;
    std::array<LLUUID, LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT>
        mPbrSemanticIds;

    // Kept separate so it can be applied after the base render-material block.
    LLPointer<LLGLTFMaterial> mOverride;
    bool mHasPbr = false;
    bool mHasBakedSemantic = false;
    bool mResolvedReady = true;
};

struct ALGhostResolvedMaterialObject
{
    LLUUID mSourceObjectId;
    U8 mFaceCount = 0;
    std::vector<ALGhostResolvedMaterialFace> mFaces;
    LLRenderMaterialParams mRenderMaterialParams;
    bool mHasRenderMaterialParams = false;
};

class ALGhostMaterialResolver
{
public:
    enum class VerifyStatus : U8
    {
        PASS,
        PENDING,
        FAIL
    };

    // Capture values and resolved texture objects before destination mutation.
    static ALGhostResolvedMaterialObject capture(const LLVOVolume* source);

    // Apply a fully resolved snapshot and register persistent bindings. A
    // pending face is rejected before destination mutation because this stage
    // has no source-aware re-promotion scheduler. Registration occurs before
    // setTE(), allowing LLViewerObject to make a private BoM render material
    // before any PBR channel is re-resolved.
    static bool apply(LLVOVolume* destination,
                      const ALGhostResolvedMaterialObject& snapshot);

    // Backend-neutral source preflight. Overlay continues to render source
    // draw artifacts by reference; this logs the same material domain that the
    // entity backend snapshots.
    static void auditSource(LLVOAvatar* source,
                            const char* backend,
                            bool exclude_temporary);

    // Acceptance-harness comparison. This is diagnostic and intentionally does
    // not replace face/pool/draw-enrollment verification.
    static VerifyStatus verifyBindings(const LLVOVolume* source,
                                       const LLVOVolume* clone);
};

#endif // AL_ALGHOSTMATERIALRESOLVER_H
